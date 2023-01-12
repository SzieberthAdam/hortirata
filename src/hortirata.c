#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// TSC clock
// https://stackoverflow.com/questions/13772567/how-to-get-the-cpu-cycle-count-in-x86-TILESIZE-from-c
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

#define TILESIZE 64
#define TILECENTERSIZE 50
#define TILEUNDERLEVEL 9
#define TILEOVERLEVEL 9
#define TILE_HOVER_COL 17
#define TILE_LOCK_COL 18
#define TILE_ORIGIN_X 32
#define TILE_ORIGIN_Y 96

#define COLOR_BACKGROUND BLACK
#define COLOR_FOREGROUND WHITE
#define COLOR_TITLE YELLOW

#define WX ((screenWidth - windowedScreenWidth) / 2)
#define WY ((screenHeight - windowedScreenHeight) / 2)


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

enum eqharvestsSpecialValue {
    eqharvestsWin = 0,
    eqharvestsMaxCalculate = 5, // IMPORTANT
    eqharvestsTooHighToCalculate = 254,
    eqharvestsUnchecked = 255
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
uint32_t harvests = 0;
uint8_t board[BOARDROWS][BOARDCOLUMNS];
uint8_t eqharvests = 0;
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
Rectangle viewport;
Texture2D backgroundTexture;
Texture2D tilesTexture;
uint16_t screenHeight = 0;
uint16_t screenWidth = 0;
uint16_t windowedScreenHeight = 0;
uint16_t windowedScreenWidth = 0;
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
    harvests = 0;
    eqharvests = eqharvestsUnchecked;
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


bool simulate(uint8_t board[BOARDROWS][BOARDCOLUMNS], uint8_t fieldtypecounts[FIELDTYPECOUNT], uint8_t fieldtypecounttarget, uint8_t harvests)
{
    uint8_t simboard[BOARDROWS][BOARDCOLUMNS];
    uint8_t simvcount[FIELDTYPECOUNT];
    if (harvests == 0) return false;
    for (uint8_t row=0; row<BOARDROWS; row++)
    {
        for (uint8_t col=0; col<BOARDCOLUMNS; col++)
        {
            uint8_t c = board[row][col];
            switch (c)
            {
                case Grass:
                case Grain:
                case Lettuce:
                case Berry:
                case Seed:
                {
                    memcpy(&simboard, board, BOARDROWS*BOARDCOLUMNS);
                    memcpy(&simvcount, fieldtypecounts, FIELDTYPECOUNT);
                    transform(simboard, simvcount, row, col);
                    if (vcount_in_equilibrium(simvcount, fieldtypecounttarget)) return true;
                    if ((1 < harvests) && simulate(simboard, simvcount, fieldtypecounttarget, harvests-1)) return true;
                } break;
            }
        }
    }
    return false;
}


void draw_board()
{
    DrawTexture(backgroundTexture, viewport.x, viewport.y, WHITE);
    {
        sprintf(str, "HORTIRATA");
        int strwidth = MeasureText(str, 30);
        DrawText(str, viewport.x + (uint16_t)((viewport.width - strwidth)/2), viewport.y + 11, 30, COLOR_TITLE);
    }
    {
        sprintf(str, "A game by SZIEBERTH ""\xC3\x81""d""\xC3\xA1""m");
        int strwidth = MeasureText(str, 10);
        DrawText(str, viewport.x + (uint16_t)((viewport.width - strwidth)/2), viewport.y + 41, 10, COLOR_FOREGROUND);
    }
    for (uint8_t row=0; row<BOARDROWS; row++)
    {
        for (uint8_t col=0; col<BOARDCOLUMNS; col++)
        {
            uint8_t c = board[row][col];
            uint8_t i = (0 < fieldtypecounts[c-Grass]) ? min(TILEUNDERLEVEL + TILEOVERLEVEL, TILEUNDERLEVEL + fieldtypecounts[c-Grass] - fieldtypecounttarget) : TILEUNDERLEVEL;
            Rectangle source;
            Rectangle dest = {viewport.x + TILE_ORIGIN_X + col * TILESIZE, viewport.y + TILE_ORIGIN_Y + row * TILESIZE, TILESIZE, TILESIZE};
            switch (c)
            {
                case Grass:
                case Grain:
                case Lettuce:
                case Berry:
                case Seed:
                    source = (Rectangle){i * TILESIZE, (1+c-Grass) * TILESIZE, TILESIZE, TILESIZE}; break;
                case Arable:
                    source = (Rectangle){0 * TILESIZE, 0, TILESIZE, TILESIZE}; break;
                case Water:
                default:
                    source = (Rectangle){1 * TILESIZE, 0, TILESIZE, TILESIZE}; break;
            }
            DrawTexturePro(tilesTexture, source, dest, ((Vector2){0, 0}), 0, WHITE);
        }
    }
    //DrawRectangleLinesEx(viewport, 1, MAGENTA);
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
                viewport = ((Rectangle){WX,WY,windowedScreenWidth,windowedScreenHeight});
                draw_board();
                sprintf(str, "MOVE YOUR MOUSE!");
                int strwidth = MeasureText(str, 20);
                DrawText(str, viewport.x + TILE_ORIGIN_X + viewport.width - strwidth - 73, viewport.y + TILE_ORIGIN_Y + TILESIZE * BOARDROWS + 55, 20, COLOR_FOREGROUND);
                if (0 < harvests) sprintf(str, "%d HARVEST%s", harvests, ((harvests==1) ? "" : "S"));
                else sprintf(str, "LEVEL %d", level);
                DrawText(str, viewport.x + TILE_ORIGIN_X + 9, viewport.y + TILE_ORIGIN_Y + TILESIZE * BOARDROWS + 55, 20, COLOR_FOREGROUND);
            } break;

            case Playing:
            {
                uint8_t row = (mouse.y - WY - TILE_ORIGIN_Y) / TILESIZE;
                uint8_t col = (mouse.x - WX - TILE_ORIGIN_X) / TILESIZE;
                uint8_t rowmod = (uint32_t)(mouse.y - WY - TILE_ORIGIN_Y) % TILESIZE;
                uint8_t colmod = (uint32_t)(mouse.x - WX - TILE_ORIGIN_X) % TILESIZE;
                uint8_t lbound = (TILESIZE-TILECENTERSIZE)/2;
                uint8_t ubound = TILECENTERSIZE + lbound - 1;
                uint8_t c = board[row][col];
                bool validloc = \
                (
                        (row < BOARDROWS && col < BOARDCOLUMNS)
                        &&
                        (c-Grass < FIELDTYPECOUNT)
                        &&
                        (lbound <= rowmod && rowmod <= ubound && lbound <= colmod && colmod <= ubound)
                );
                if (validloc && (currentGesture != lastGesture && currentGesture == GESTURE_TAP))
                {
                    transform(board, fieldtypecounts, row, col);
                    harvests++;
                    eqharvests = eqharvestsUnchecked;
                }
                bool equilibrium = vcount_in_equilibrium(fieldtypecounts, fieldtypecounttarget);
                if (equilibrium)
                {
                    eqharvests = eqharvestsWin;
                    scene = Win;
                }
                else if (eqharvests == eqharvestsUnchecked)
                {
                    for (eqharvests=1; eqharvests <= eqharvestsMaxCalculate; eqharvests++)
                    {
                        equilibrium = simulate(board, fieldtypecounts, fieldtypecounttarget, eqharvests);
                        if (equilibrium) break;
                    }
                    if (!equilibrium) eqharvests = eqharvestsTooHighToCalculate;
                }
                viewport = (Rectangle){WX,WY,windowedScreenWidth,windowedScreenHeight};
                draw_board();
                if (eqharvestsMaxCalculate < eqharvests) sprintf(str, "EQUILIBRIUM OVER %d HARVEST%s", eqharvestsMaxCalculate, ((eqharvestsMaxCalculate==1) ? "" : "S"));
                else sprintf(str, "EQUILIBRIUM IN %d HARVEST%s", eqharvests, ((eqharvests==1) ? "" : "S"));
                int strwidth = MeasureText(str, 20);
                DrawText(str, viewport.x + TILE_ORIGIN_X + viewport.width - strwidth - 73, viewport.y + TILE_ORIGIN_Y + TILESIZE * BOARDROWS + 55, 20, COLOR_FOREGROUND);
                if (0 < harvests) sprintf(str, "%d HARVEST%s", harvests, ((harvests==1) ? "" : "S"));
                else sprintf(str, "LEVEL %d", level);
                DrawText(str, viewport.x + TILE_ORIGIN_X + 9, viewport.y + TILE_ORIGIN_Y + TILESIZE * BOARDROWS + 55, 20, COLOR_FOREGROUND);
                if (validloc && (currentGesture == GESTURE_NONE || currentGesture == GESTURE_DRAG))
                {
                    Rectangle dest = {viewport.x + TILE_ORIGIN_X + col * TILESIZE, viewport.y + TILE_ORIGIN_Y + row * TILESIZE, TILESIZE, TILESIZE};
                    DrawTexturePro(tilesTexture, ((Rectangle){TILE_HOVER_COL * TILESIZE, 0, TILESIZE, TILESIZE}), dest, ((Vector2){0, 0}), 0, WHITE);
                }
            } break;

            case Win:
            {
                viewport = (Rectangle){WX,WY,windowedScreenWidth,windowedScreenHeight};
                draw_board();
                sprintf(str, "CONGRATULATIONS!");
                int strwidth = MeasureText(str, 20);
                DrawText(str, viewport.x + TILE_ORIGIN_X + viewport.width - strwidth - 73, viewport.y + TILE_ORIGIN_Y + TILESIZE * BOARDROWS + 55, 20, COLOR_FOREGROUND);
                if (0 < harvests) sprintf(str, "%d HARVEST%s", harvests, ((harvests==1) ? "" : "S"));
                else sprintf(str, "LEVEL %d", level);
                DrawText(str, viewport.x + TILE_ORIGIN_X + 9, viewport.y + TILE_ORIGIN_Y + TILESIZE * BOARDROWS + 55, 20, COLOR_FOREGROUND);
                if (currentGesture != lastGesture && currentGesture == GESTURE_TAP)
                {
                    bool success = load_level(level+1);
                    if (!success) scene = Thanks;
                }
            } break;

            case Thanks:
            {
                viewport = (Rectangle){WX,WY,windowedScreenWidth,windowedScreenHeight};
                sprintf(str, "THANKS FOR PLAYING!");
                int strwidth = MeasureText(str, 20);
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