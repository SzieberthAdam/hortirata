#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// TSC clock
// https://stackoverflow.com/questions/13772567/how-to-get-the-cpu-cycle-count-in-x86-tileSize-from-c
#ifdef _MSC_VER
# include <intrin.h>
#else
# include <x86intrin.h>
#endif

#include "raylib.h"

#ifndef max
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif
#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif

#define FIELDTYPECOUNT 5
#define BOARDROWS 9
#define BOARDCOLUMNS 19

#define COLOR_BACKGROUND BLACK
#define COLOR_FOREGROUND WHITE
#define COLOR_TITLE YELLOW


enum HortirataFieldType {
    LF = 0x0A,
    CR = 0x0D,
    Grass = '0',
    Grain = '1',
    Lettuce = '2',
    Berry = '3',
    Seed = '4',
    Arable = '_',
    Water = '~',
};

enum HortirataScene {
    NoScene = 0,
    Draw = 1,
    Playing = 11,
    Win = 21,
    Thanks = 22
};

enum eqpicksSpecialValue {
    eqpicksWin = 0,
    eqpicksMaxCalculate = 3, // IMPORTANT
    eqpicksTooHighToCalculate = 254,
    eqpicksUnchecked = 255
};


/*
=== HELPER FUNCTIONS ===========================================================================================
*/

// Greatest power of 2 less than or equal to x. Hacker's Delight, Figure 3-1.
unsigned flp2(unsigned x)
{
    x = x | (x >> 1);
    x = x | (x >> 2);
    x = x | (x >> 4);
    x = x | (x >> 8);
    x = x | (x >> 16);
    return x - (x >> 1);
}

// Least power of 2 greater than or equal to x. Hacker's Delight, Figure 3-3.
unsigned clp2(unsigned x)
{
    x = x - 1;
    x = x | (x >> 1);
    x = x | (x >> 2);
    x = x | (x >> 4);
    x = x | (x >> 8);
    x = x | (x >> 16);
    return x + 1;
}

// Find first set bit in an integer.
unsigned ffs(int n)
{
    return log2(n & -n) + 1;
}


/*
=== GLOBAL VARIABLES ===========================================================================================
*/

char str[1024];
int strwidth;
uint32_t picks = 0;
uint8_t board[BOARDROWS][BOARDCOLUMNS];
uint8_t eqpicks = 0;
uint8_t fieldtypecounts[FIELDTYPECOUNT];
uint8_t fieldtypecounttarget = 0;
uint8_t gamefields = 0;
uint8_t level = 0;
uint8_t randomfields = 0;
uint8_t scene = NoScene;

int currentGesture = GESTURE_NONE;
int display = 0;
int fps = 30;
int lastGesture = GESTURE_NONE;
Rectangle textboxLevel = {99, 687, 58, 26};
Rectangle textboxPicks = {235, 687, 58, 26};
Rectangle tileWinDest = {627, 687, 26, 26};
Rectangle tileWinSource = {915, 19, 26, 26};
Rectangle viewport;
Texture2D backgroundTexture;
Texture2D tilesTexture;
uint16_t screenHeight = 0;
uint16_t screenWidth = 0;
uint16_t tileOriginX = 32;
uint16_t tileOriginY = 72;
uint16_t windowedScreenHeight = 0;
uint16_t windowedScreenWidth = 0;
uint8_t tileActiveSize = 50;
uint8_t tileDeficitAvailable = 9;
uint8_t tileHoverX = 17;
uint8_t tileSize = 64;
uint8_t tileSurplusAvailable = 9;
Vector2 mouse;
Vector2 mouseDelta;
Vector2 windowPos;


/*
=== FUNCTIONS ==================================================================================================
*/


bool load(const char *fileName)
{
    if (!FileExists(fileName)) return false;
    unsigned int filelength = GetFileLength(fileName);
    if (filelength < BOARDROWS*BOARDCOLUMNS) return false;
    unsigned char* filedata = LoadFileData(fileName, &filelength);
    picks = 0;
    eqpicks = eqpicksUnchecked;
    uint8_t row = 0;
    uint8_t col = 0;
    for (uint8_t v=0; v<FIELDTYPECOUNT; v++) fieldtypecounts[v] = 0;
    gamefields = 0;
    randomfields = 0;
    for (uint8_t i=0; i<filelength; ++i)
    {
        uint8_t c = filedata[i];
        switch (c)
        {
            case LF:
            case CR:
            {
                if (0<col) row++;
                col = 0;
            } break;
            case Grass:
            case Grain:
            case Lettuce:
            case Berry:
            case Seed:
            {
                board[row][col] = c;
                col++;
                if (BOARDCOLUMNS <= col)
                {
                    row++;
                    col = 0;
                }
                fieldtypecounts[c-Grass]++;
                gamefields++;
            } break;
            case Arable:
            {
                board[row][col] = c;
                col++;
                if (BOARDCOLUMNS <= col)
                {
                    row++;
                    col = 0;
                }
                randomfields++;
            } break;
            case Water:
            default:
            {
                board[row][col] = Water;
                col++;
                if (BOARDCOLUMNS <= col)
                {
                    row++;
                    col = 0;
                }
            } break;
        }
        if (BOARDROWS <= row) break;
    }
    fieldtypecounttarget = (gamefields + randomfields) / FIELDTYPECOUNT;
    UnloadFileData(filedata);
    if (0 == randomfields) scene = Playing;
    else scene = Draw;
    return true;
}


bool load_level(uint8_t levelval)
{
    sprintf(str, "%s\\level%03d.hortirata", GetApplicationDirectory(), levelval);
    bool success = load(str);
    if (success) level = levelval;
    return success;
}


bool save(const char *fileName)
{
    unsigned int bytesToWrite = BOARDROWS * (BOARDCOLUMNS+2);
    char *data = MemAlloc(bytesToWrite);
    uint8_t i = 0;
    for (uint8_t row=0; row<BOARDROWS; ++row)
    {
        for (uint8_t col=0; col<BOARDCOLUMNS; ++col)
        {
            data[i] = board[row][col];
            ++i;
            if (col == BOARDCOLUMNS - 1)
            {
                data[i] = CR;
                ++i;
                data[i] = LF;
                ++i;
            }
        }
    }
    bool success = SaveFileData(fileName, data, bytesToWrite);
    return success;
}


void transform(uint8_t board[BOARDROWS][BOARDCOLUMNS], uint8_t fieldtypecounts[FIELDTYPECOUNT], uint8_t row, uint8_t col)
{
    uint8_t c0 = board[row][col];
    for (uint8_t row1=((0 < row) ? row-1 : 0); row1<=((row < BOARDROWS-1) ? row+1 : BOARDROWS-1); row1++)
    {
        for (uint8_t col1=((0 < col) ? col-1 : 0); col1<=((col < BOARDCOLUMNS-1) ? col+1 : BOARDCOLUMNS-1); col1++)
        {
            if ((row1==row) && (col1==col)) continue;
            uint8_t c1 = board[row1][col1];
            switch (c1)
            {
                case Grass:
                case Grain:
                case Lettuce:
                case Berry:
                case Seed:
                {
                    uint8_t c2 = ((c1-Grass + c0-Grass) % FIELDTYPECOUNT)+Grass;
                    board[row1][col1] = c2;
                    fieldtypecounts[c1-Grass]--;
                    fieldtypecounts[c2-Grass]++;
                } break;
            }
        }
    }
}

bool vcount_in_equilibrium(uint8_t fieldtypecounts[FIELDTYPECOUNT], uint8_t fieldtypecounttarget)
{
    for (uint8_t v=0; v<FIELDTYPECOUNT; v++) if (fieldtypecounts[v] != fieldtypecounttarget) return false;
    return true;
}


bool simulate(uint8_t board[BOARDROWS][BOARDCOLUMNS], uint8_t fieldtypecounts[FIELDTYPECOUNT], uint8_t fieldtypecounttarget, uint8_t picks)
{
    uint8_t simboard[BOARDROWS][BOARDCOLUMNS];
    uint8_t simvcount[FIELDTYPECOUNT];
    if (picks == 0) return false;
    for (uint8_t row=0; row<BOARDROWS; row++)
    {
        for (uint8_t col=0; col<BOARDCOLUMNS; col++)
        {
            uint8_t c = board[row][col];
            switch (c)
            {
                case Grain:  // Grass intentionally left out
                case Lettuce:
                case Berry:
                case Seed:
                {
                    memcpy(&simboard, board, BOARDROWS*BOARDCOLUMNS);
                    memcpy(&simvcount, fieldtypecounts, FIELDTYPECOUNT);
                    transform(simboard, simvcount, row, col);
                    if (vcount_in_equilibrium(simvcount, fieldtypecounttarget)) return true;
                    if ((1 < picks) && simulate(simboard, simvcount, fieldtypecounttarget, picks-1)) return true;
                } break;
            }
        }
    }
    return false;
}


void draw_board()
{
    DrawTexture(backgroundTexture, viewport.x, viewport.y, WHITE);
    for (uint8_t row=0; row<BOARDROWS; row++)
    {
        for (uint8_t col=0; col<BOARDCOLUMNS; col++)
        {
            uint8_t c = board[row][col];
            uint8_t i = (0 < fieldtypecounts[c-Grass]) ? min(tileDeficitAvailable + tileSurplusAvailable, tileDeficitAvailable + fieldtypecounts[c-Grass] - fieldtypecounttarget) : tileDeficitAvailable;
            Rectangle source;
            Rectangle dest = {viewport.x + tileOriginX + col * tileSize, viewport.y + tileOriginY + row * tileSize, tileSize, tileSize};
            switch (c)
            {
                case Grass:
                case Grain:
                case Lettuce:
                case Berry:
                case Seed:
                    source = (Rectangle){i * tileSize, (1+c-Grass) * tileSize, tileSize, tileSize}; break;
                case Arable:
                    source = (Rectangle){0 * tileSize, 0, tileSize, tileSize}; break;
                case Water:
                default:
                    source = (Rectangle){1 * tileSize, 0, tileSize, tileSize}; break;
            }
            DrawTexturePro(tilesTexture, source, dest, ((Vector2){0, 0}), 0, WHITE);
        }
    }
    //DrawRectangleLinesEx(viewport, 1, MAGENTA);
}


void draw_info()
{
    sprintf(str, "%d", level);
    strwidth = MeasureText(str, 20);
    DrawText(str, viewport.x + textboxLevel.x + (textboxLevel.width - strwidth)/2, viewport.y + textboxLevel.y + ((textboxLevel.height - 14)/2) - 2, 20, COLOR_BACKGROUND);
    sprintf(str, "%d", picks);
    strwidth = MeasureText(str, 20);
    DrawText(str, viewport.x + textboxPicks.x + (textboxPicks.width - strwidth)/2, viewport.y + textboxPicks.y + ((textboxPicks.height - 14)/2) - 2, 20, COLOR_BACKGROUND);
    switch (eqpicks)
    {
        case eqpicksWin:
        {
            DrawTexturePro(tilesTexture, tileWinSource, ((Rectangle){viewport.x + tileWinDest.x, viewport.y + tileWinDest.y , tileWinDest.width, tileWinDest.height}), ((Vector2){0, 0}), 0, WHITE);
        }; // fallthrough!
        case 1:
        {
            DrawRectangle(viewport.x + 611, viewport.y + 691, 10, 18, COLOR_FOREGROUND);
            DrawRectangle(viewport.x + 659, viewport.y + 691, 10, 18, COLOR_FOREGROUND);
        }; // fallthrough!
        case 2:
        {
            DrawRectangle(viewport.x + 595, viewport.y + 695, 10, 10, COLOR_FOREGROUND);
            DrawRectangle(viewport.x + 675, viewport.y + 695, 10, 10, COLOR_FOREGROUND);
        }; // fallthrough!
        case 3:
        {
            DrawRectangle(viewport.x + 579, viewport.y + 699, 10, 2, COLOR_FOREGROUND);
            DrawRectangle(viewport.x + 691, viewport.y + 699, 10, 2, COLOR_FOREGROUND);
        } break;
    }
}

int main(void)
    {
    //SetTraceLogLevel(LOG_DEBUG);

    load_level(1);

    SetTargetFPS(fps);

    sprintf(str, "%s\\%s", GetApplicationDirectory(), "tiles.png");
    Image tiles_image = LoadImage(str);
    sprintf(str, "%s\\%s", GetApplicationDirectory(), "bg.png");
    Image bg_image = LoadImage(str);

    windowedScreenWidth = bg_image.width;
    windowedScreenHeight = bg_image.height;
    screenWidth = windowedScreenWidth;
    screenHeight = windowedScreenHeight;

    InitWindow(windowedScreenWidth, windowedScreenHeight, "Hortirata");
    windowPos = GetWindowPosition();

    // call LoadTextureFromImage(); STRICTLY AFTER InitWindow();!
    tilesTexture = LoadTextureFromImage(tiles_image);
    UnloadImage(tiles_image);
    backgroundTexture = LoadTextureFromImage(bg_image);
    UnloadImage(bg_image);

    display = GetCurrentMonitor(); // see what display we are on right now

    while (!WindowShouldClose())
    {
        // check for alt + enter
        if (IsKeyPressed(KEY_F10) || ((IsKeyPressed(KEY_ENTER) && (IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT)))))
        {
            // instead of the true fullscreen which would be the following...
            // if (IsWindowFullscreen()) {ToggleFullscreen(); SetWindowSize(windowedScreenWidth, windowedScreenHeight);}
            // else {SetWindowSize(GetMonitorWidth(display), GetMonitorHeight(display)); ToggleFullscreen();}

            // ... I go for fake fullscreen, similar to SDL_WINDOW_FULLSCREEN_DESKTOP.
            if (IsWindowState(FLAG_WINDOW_UNDECORATED))
            {
                ClearWindowState(FLAG_WINDOW_UNDECORATED);
                screenWidth = windowedScreenWidth;
                screenHeight = windowedScreenHeight;
                SetWindowSize(screenWidth, screenHeight);
                SetWindowPosition(windowPos.x, windowPos.y);
            }
            else
            {
                windowPos = GetWindowPosition();
                windowedScreenWidth = GetScreenWidth();
                windowedScreenHeight = GetScreenHeight();
                screenWidth = GetMonitorWidth(display);
                screenHeight = GetMonitorHeight(display);
                SetWindowState(FLAG_WINDOW_UNDECORATED);
                SetWindowSize(screenWidth, screenHeight);
                SetWindowPosition(0, 0);
            }
        }

        screenWidth = GetScreenWidth();
        screenHeight = GetScreenHeight();

        mouse = GetTouchPosition(0);
        lastGesture = currentGesture;
        currentGesture = GetGestureDetected();

        BeginDrawing();

        ClearBackground(RAYWHITE);
        DrawRectangle(0, 0, screenWidth, screenHeight, COLOR_BACKGROUND);

        switch (scene)
        {
            case Draw:
            {
                bool entropy = false;
                if (IsKeyPressed(KEY_SPACE)) entropy = true;
                if (!entropy)
                {
                    if (currentGesture == GESTURE_DRAG) mouseDelta = GetGestureDragVector();
                    else mouseDelta = GetMouseDelta();
                    entropy = mouseDelta.x != (float)(0) || mouseDelta.y != (float)(0);
                }
                if (0 < randomfields && entropy)
                {
                    uint64_t randomvalue = __rdtsc();
                    uint8_t row, col, c;
                    for (row=0; row<BOARDROWS; ++row)
                    {
                        for (col=0; col<BOARDCOLUMNS; ++col)
                        {
                            c = board[row][col];
                            if (c == Arable) break;
                        }
                        if (c == Arable) break;
                    }
                    uint8_t remgamefields = fieldtypecounttarget * FIELDTYPECOUNT - gamefields;
                    uint8_t remdummyfields = randomfields - remgamefields;
                    if (0 < remgamefields && 0 < remdummyfields)
                    {
                        uint8_t populationsize = randomfields;
                        uint8_t randsize = clp2(populationsize);
                        uint8_t randmask = (randsize * 2 - 1) << 3;
                        uint8_t v1 = (randomvalue & randmask) >> 3;
                        uint8_t v = randomvalue & 0x07;
                        if (v < FIELDTYPECOUNT)
                        {
                            if (v1 < remdummyfields)
                            {
                                board[row][col] = Water;
                                randomfields--;
                            }
                            else if (v < populationsize)
                            {
                                board[row][col] = v+Grass;
                                fieldtypecounts[v]++;
                                gamefields++;
                                randomfields--;
                            }
                        }
                    }
                    if (0 < remgamefields && 0 == remdummyfields)
                    {
                        uint8_t v = randomvalue & 0x07;
                        if (v < FIELDTYPECOUNT)
                        {
                            board[row][col] = v+Grass;
                            fieldtypecounts[v]++;
                            gamefields++;
                            randomfields--;
                        }
                    }
                    else if (0 == remgamefields && 0 < remdummyfields)
                    {
                        board[row][col] = Water;
                        randomfields--;
                    }
                    if (0 == randomfields) scene = Playing;
                }
                viewport = ((Rectangle){((screenWidth - windowedScreenWidth) / 2),((screenHeight - windowedScreenHeight) / 2),windowedScreenWidth,windowedScreenHeight});
                draw_board();
            } break;

            case Playing:
            {
                uint8_t row = (mouse.y - ((screenHeight - windowedScreenHeight) / 2) - tileOriginY) / tileSize;
                uint8_t col = (mouse.x - ((screenWidth - windowedScreenWidth) / 2) - tileOriginX) / tileSize;
                uint8_t rowmod = (uint32_t)(mouse.y - ((screenHeight - windowedScreenHeight) / 2) - tileOriginY) % tileSize;
                uint8_t colmod = (uint32_t)(mouse.x - ((screenWidth - windowedScreenWidth) / 2) - tileOriginX) % tileSize;
                uint8_t lbound = (tileSize-tileActiveSize)/2;
                uint8_t ubound = tileActiveSize + lbound - 1;
                uint8_t c = board[row][col];
                bool validloc = \
                (
                        (row < BOARDROWS && col < BOARDCOLUMNS)
                        &&
                        (0 < c-Grass && c-Grass < FIELDTYPECOUNT)
                        &&
                        (lbound <= rowmod && rowmod <= ubound && lbound <= colmod && colmod <= ubound)
                );
                if (validloc && (currentGesture != lastGesture && currentGesture == GESTURE_TAP))
                {
                    transform(board, fieldtypecounts, row, col);
                    picks++;
                    eqpicks = eqpicksUnchecked;
                }
                bool equilibrium = vcount_in_equilibrium(fieldtypecounts, fieldtypecounttarget);
                if (equilibrium)
                {
                    eqpicks = eqpicksWin;
                    scene = Win;
                }
                else if (!equilibrium && (eqpicks == eqpicksUnchecked))
                {
                    for (eqpicks=1; eqpicks <= eqpicksMaxCalculate; eqpicks++)
                    {
                        equilibrium = simulate(board, fieldtypecounts, fieldtypecounttarget, eqpicks);
                        if (equilibrium) break;
                    }
                    if (!equilibrium) eqpicks = eqpicksTooHighToCalculate;
                }
                viewport = (Rectangle){((screenWidth - windowedScreenWidth) / 2),((screenHeight - windowedScreenHeight) / 2),windowedScreenWidth,windowedScreenHeight};
                draw_board();
                draw_info();
                if (validloc && (currentGesture == GESTURE_NONE || currentGesture == GESTURE_DRAG))
                {
                    Rectangle dest = {viewport.x + tileOriginX + col * tileSize, viewport.y + tileOriginY + row * tileSize, tileSize, tileSize};
                    DrawTexturePro(tilesTexture, ((Rectangle){tileHoverX * tileSize, 0, tileSize, tileSize}), dest, ((Vector2){0, 0}), 0, WHITE);
                }
            } break;

            case Win:
            {
                viewport = (Rectangle){((screenWidth - windowedScreenWidth) / 2),((screenHeight - windowedScreenHeight) / 2),windowedScreenWidth,windowedScreenHeight};
                draw_board();
                draw_info();
                if (currentGesture != lastGesture && currentGesture == GESTURE_TAP)
                {
                    bool success = load_level(level+1);
                    if (!success) scene = Thanks;
                }
            } break;

            case Thanks:
            {
                viewport = (Rectangle){((screenWidth - windowedScreenWidth) / 2),((screenHeight - windowedScreenHeight) / 2),windowedScreenWidth,windowedScreenHeight};
                sprintf(str, "THANKS FOR PLAYING!");
                strwidth = MeasureText(str, 20);
                DrawText(str, viewport.x + (viewport.width - strwidth) / 2, 100, 20, COLOR_FOREGROUND);
            }
        }

        DrawFPS(screenWidth-100, 10); // for debug

        EndDrawing();
        //----------------------------------------------------------------------------------

        // Load by drop
        if (IsFileDropped())
        {
            FilePathList droppedfiles = LoadDroppedFiles();
            if (droppedfiles.count == 1)
            {
                bool success = load(droppedfiles.paths[0]);
                if (success) level = 0;
            }
            UnloadDroppedFiles(droppedfiles);
        }
        // Quickload
        if (IsKeyPressed(KEY_L))
        {
            sprintf(str, "%s\\%s", GetApplicationDirectory(), "puzzle.hortirata");
            bool success = load(str);
            if (success) level = 0;
        }
        // Quicksave
        if (IsKeyPressed(KEY_S))
        {
            sprintf(str, "%s\\%s", GetApplicationDirectory(), "puzzle.hortirata");
            save(str);
        }

    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    CloseWindow();        // Close window and OpenGL context
    UnloadTexture(backgroundTexture);
    UnloadTexture(tilesTexture);
    //--------------------------------------------------------------------------------------

    return 0;
}