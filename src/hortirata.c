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

#define TARGET_FPS     30

#ifndef max
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif
#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif

#define N 5
#define M 2
#define A (N + 2*M)
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

#define SCENE_NEWGAME_DEFAULT 10
#define SCENE_NEWGAME_LEVEL 11
#define SCENE_NEWGAME 12
#define SCENE_DRAWBOARD 20
#define SCENE_GAME 30
#define SCENE_WIN 40
#define SCENE_THANKS 50

#define STATE_WIN 0
#define STATE_MANYHARVESTS 127
#define STATE_BOARDUNDERCONSTRUCTION 255
#define STATE_NEWLEVEL 0x80
#define STATE_NEWLEVELWITHDRAW 0xC0

#define MAXSIMHARVESTS N

#define WX ((screenWidth - windowedScreenWidth) / 2)
#define WY ((screenHeight - windowedScreenHeight) / 2)

char str[1024];

typedef struct __attribute__((__packed__, __scalar_storage_order__("big-endian"))) {
    uint8_t board[A][A];
    uint32_t harvests;
} HortirataData;


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


void setfields(uint8_t board[A][A], uint8_t *gamefields, uint8_t *randomfields)
{
    *gamefields = 0;
    *randomfields = 0;
    for (uint8_t row=0; row<A; row++)
    {
        for (uint8_t col=0; col<A; col++)
        {
            uint8_t v = board[row][col];
            if (v < N) *gamefields = *gamefields+1;
            else if (v==0x80) *randomfields = *randomfields+1;
        }
    }
}


bool load(const char *fileName, HortirataData *data, uint8_t vcount[N], uint8_t *targetvcount)
{
    if (!FileExists(fileName)) return false;
    unsigned int filelength = GetFileLength(fileName);
    if (filelength != A * A) return false;
    unsigned char* filedata = LoadFileData(fileName, &filelength);
    data->harvests = 0;
    memcpy(&data->board, filedata, filelength);
    for (uint8_t v=0; v<N; v++) vcount[v] = 0;
    uint8_t gamefields = 0;
    uint8_t randomfields = 0;
    for (uint8_t row=0; row<A; row++)
    {
        for (uint8_t col=0; col<A; col++)
        {
            uint8_t v = data->board[row][col];
            if (v < N) vcount[v]++;
        }
        setfields(data->board, &gamefields, &randomfields);
        *targetvcount = (gamefields + randomfields) / N;
    }
    UnloadFileData(filedata);
    return true;
}


void transform(uint8_t board[A][A], uint8_t vcount[N], uint8_t row, uint8_t col)
{
    uint8_t v = board[row][col];
    for (uint8_t row1=((0 < row) ? row-1 : 0); row1<=((row < A-1) ? row+1 : A-1); row1++)
    {
        for (uint8_t col1=((0 < col) ? col-1 : 0); col1<=((col < A-1) ? col+1 : A-1); col1++)
        {
            if ((row1==row) && (col1==col)) continue;
            uint8_t v0 = board[row1][col1];
            if (0x80 <= v0) continue;
            uint8_t v1 = (v0 + v) % N;
            board[row1][col1] = v1;
            vcount[v0]--;
            vcount[v1]++;
        }
    }
}

bool vcount_in_equilibrium(uint8_t vcount[N], uint8_t targetvcount)
{
    for (uint8_t v=0; v<N; v++)
    {
        if (vcount[v] != targetvcount) return false;
    }
    return true;
}


bool simulate(uint8_t board[A][A], uint8_t vcount[N], uint8_t targetvcount, uint8_t harvests)
{
    uint8_t simboard[A][A];
    uint8_t simvcount[N];

    if (harvests == 0) return false;

    for (uint8_t row=0; row<A; row++)
    {
        for (uint8_t col=0; col<A; col++)
        {
            uint8_t v = board[row][col];
            if (0 < v && v < N)
            {
                memcpy(&simboard, board, A*A);
                memcpy(&simvcount, vcount, N);
                transform(simboard, simvcount, row, col);
                if (vcount_in_equilibrium(simvcount, targetvcount)) return true;
                if ((1 < harvests) && simulate(simboard, simvcount, targetvcount, harvests-1)) return true;
            }
        }
    }
    return false;
}


void draw_board(HortirataData data, uint8_t vcount[N], uint8_t targetvcount, uint8_t eqharvests, uint8_t level, Rectangle viewport, Texture2D bg, Texture2D tiles)
{
    DrawTexture(bg, viewport.x, viewport.y, WHITE);
    {
        sprintf(str, "HORTIRATA");
        int strwidth = MeasureText(str, 30);
        DrawText(str, viewport.x + (uint16_t)((viewport.width - strwidth)/2), viewport.y + 11, 30, COLOR_TITLE);
    }
    {
        sprintf(str, "A game for Ludum Dare 52 (Theme: Harvest) by SZIEBERTH ""\xC3\x81""d""\xC3\xA1""m");
        int strwidth = MeasureText(str, 10);
        DrawText(str, viewport.x + (uint16_t)((viewport.width - strwidth)/2), viewport.y + 41, 10, COLOR_FOREGROUND);
    }
    for (uint8_t row=0; row<A; row++)
    {
        for (uint8_t col=0; col<A; col++)
        {
            uint8_t v = data.board[row][col];
            uint8_t i = (0 < vcount[v]) ? min(TILEUNDERLEVEL + TILEOVERLEVEL, TILEUNDERLEVEL + vcount[v] - targetvcount) : TILEUNDERLEVEL;
            Rectangle dest = {viewport.x + TILE_ORIGIN_X + col * TILESIZE, viewport.y + TILE_ORIGIN_Y + row * TILESIZE, TILESIZE, TILESIZE};
            if (0x80 <= v)
            {
                DrawTexturePro(tiles, ((Rectangle){(v - 0x80) * TILESIZE, 0, TILESIZE, TILESIZE}), dest, ((Vector2){0, 0}), 0, WHITE);
            }
            else if (v == 0)
            {
                DrawTexturePro(tiles, ((Rectangle){i * TILESIZE, (1+v) * TILESIZE, TILESIZE, TILESIZE}), dest, ((Vector2){0, 0}), 0, WHITE);
            }
            else
            {
                DrawTexturePro(tiles, ((Rectangle){i * TILESIZE, (1+v) * TILESIZE, TILESIZE, TILESIZE}), dest, ((Vector2){0, 0}), 0, WHITE);
            }
        }
    }

    if (eqharvests == STATE_WIN) sprintf(str, "CONGRATULATIONS!");
    else if (eqharvests == STATE_MANYHARVESTS) sprintf(str, "EQUILIBRIUM OVER %d HARVESTS", MAXSIMHARVESTS);
    else if (STATE_NEWLEVELWITHDRAW <= eqharvests) sprintf(str, "MOVE YOUR MOUSE!");
    else sprintf(str, "EQUILIBRIUM IN %d HARVEST%s", eqharvests, ((eqharvests==1) ? "" : "S"));
    int strwidth = MeasureText(str, 20);
    DrawText(str, viewport.x + TILE_ORIGIN_X + viewport.width - strwidth - 73, viewport.y + TILE_ORIGIN_Y + TILESIZE * A + 55, 20, COLOR_FOREGROUND);


    if (0 < data.harvests) sprintf(str, "%d HARVEST%s", data.harvests, ((data.harvests==1) ? "" : "S"));
    else sprintf(str, "LEVEL %d", level);
    DrawText(str, viewport.x + TILE_ORIGIN_X + 9, viewport.y + TILE_ORIGIN_Y + TILESIZE * A + 55, 20, COLOR_FOREGROUND);
    //DrawRectangleLinesEx(viewport, 1, MAGENTA);
}


int main(void)
    {
    //SetTraceLogLevel(LOG_DEBUG);

    HortirataData data;
    uint8_t vcount[N];
    uint8_t targetvcount;

    uint8_t gamefields = 0;
    uint8_t randomfields = 0;

    uint8_t level = 1;
    uint8_t scene = SCENE_NEWGAME_LEVEL;
    uint8_t eqharvests = 0;

    Vector2 mouse;
    Vector2 mousedelta;
    int currentGesture = GESTURE_NONE;
    int lastGesture = GESTURE_NONE;

    SetConfigFlags(FLAG_VSYNC_HINT);
    SetTargetFPS(TARGET_FPS);
    sprintf(str, "%s\\%s", GetApplicationDirectory(), "bg.png");
    Image bg_image = LoadImage(str);
    uint16_t windowedScreenWidth = bg_image.width;
    uint16_t windowedScreenHeight = bg_image.height;
    uint16_t screenWidth = windowedScreenWidth;
    uint16_t screenHeight = windowedScreenHeight;

    sprintf(str, "%s\\%s", GetApplicationDirectory(), "tiles.png");
    Image tiles_image = LoadImage(str);
    InitWindow(windowedScreenWidth, windowedScreenHeight, "Hortirata");
    Vector2 windowPos = GetWindowPosition();

    // call LoadTextureFromImage(); STRICTLY AFTER InitWindow();!
    Texture2D bg = LoadTextureFromImage(bg_image);
    UnloadImage(bg_image);
    Texture2D tiles = LoadTextureFromImage(tiles_image);
    UnloadImage(tiles_image);

    int display = GetCurrentMonitor(); // see what display we are on right now

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

        // check for d key
        if (IsKeyPressed(KEY_D))
        {
            sprintf(str, "LEVEL = %d", level);
            TraceLog(LOG_DEBUG, str);
            TraceLog(LOG_DEBUG, "BOARD:");
            for (uint8_t row=0; row<A; row++)
            {
                sprintf(str, "| %3d | %3d | %3d | %3d | %3d | %3d | %3d | %3d | %3d |", data.board[row][0], data.board[row][1], data.board[row][2], data.board[row][3], data.board[row][4], data.board[row][5], data.board[row][6], data.board[row][7], data.board[row][8]);
                TraceLog(LOG_DEBUG, str);
            }
            TraceLog(LOG_DEBUG, "VCOUNT:");
            sprintf(str, "[ 0:%3d , 1:%3d , 2:%3d , 3:%3d , 4:%3d ]", vcount[0], vcount[1], vcount[2], vcount[3], vcount[4]);
            TraceLog(LOG_DEBUG, str);
            sprintf(str, "TARGET VCOUNT: %d ; valid: %d, random: %d", targetvcount, gamefields, randomfields);
            TraceLog(LOG_DEBUG, str);
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

            case SCENE_NEWGAME_DEFAULT:
            {
                data.harvests = 0;
                for (uint8_t v=0; v<N; v++) vcount[v] = 0;
                for (uint8_t row=0; row<A; row++)
                {
                    for (uint8_t col=0; col<A; col++)
                    {
                        if (M <= row && row < A-M && M <= col && col < A-M) data.board[row][col] = 0x80 + 0;
                        else data.board[row][col] = 0x80 + 1;
                    }
                }
                targetvcount = 5;
                scene = SCENE_NEWGAME;
                eqharvests = 0;
            } break;

            case SCENE_NEWGAME_LEVEL:
            {
                sprintf(str, "%s\\level%03d.hortirata", GetApplicationDirectory(), level);
                bool success = load(str, &data, vcount, &targetvcount);
                if (success) scene = SCENE_NEWGAME;
                else scene = SCENE_THANKS; // TODO: endscreen
                eqharvests = 0;
            } break;

            case SCENE_NEWGAME:
            {
                setfields(data.board, &gamefields, &randomfields);
                if (0 == randomfields) scene = SCENE_GAME;
                else scene = SCENE_DRAWBOARD;
                eqharvests = 0;
            } break;

            case SCENE_DRAWBOARD:
            {
                bool entropy = false;
                if (IsKeyPressed(KEY_SPACE)) entropy = true;
                if (!entropy)
                {
                    if (currentGesture == GESTURE_DRAG) mousedelta = GetGestureDragVector();
                    else mousedelta = GetMouseDelta();
                    entropy = mousedelta.x != (float)(0) || mousedelta.y != (float)(0);
                }
                if (0 < randomfields && entropy)
                {
                    uint64_t randomvalue = __rdtsc();
                    uint8_t row, col, v;
                    for (row=0; row<A; ++row)
                    {
                        for (col=0; col<A; ++col)
                        {
                            v = data.board[row][col];
                            if (v == 0x80) break;
                        }
                        if (v == 0x80) break;
                    }
                    uint8_t remgamefields = targetvcount * N - gamefields;
                    uint8_t remdummyfields = randomfields - remgamefields;
                    //sprintf(str, "TARGET VCOUNT: %d ; valid: %d (%d), random: %d (%d)", targetvcount, gamefields, remgamefields, randomfields, remdummyfields);
                    //TraceLog(LOG_DEBUG, str);
                    if (0 < remgamefields && 0 < remdummyfields)
                    {
                        uint8_t populationsize = randomfields;
                        uint8_t randsize = clp2(populationsize);
                        uint8_t randmask = (randsize * 2 - 1) << 3;
                        uint8_t v1 = (randomvalue & randmask) >> 3;
                        uint8_t v = randomvalue & 0x07;
                        if (v < N)
                        {
                            if (v1 < remdummyfields)
                            {
                                data.board[row][col] = 0x81;
                                randomfields--;
                            }
                            else if (v < populationsize)
                            {
                                data.board[row][col] = v;
                                vcount[v]++;
                                gamefields++;
                                randomfields--;
                            }
                        }
                        //sprintf(str, "populationsize: %d ; randsize: %d ; randmask: %d; v: %d; v1: %d", populationsize, randsize, randmask, v, v1);
                        //TraceLog(LOG_DEBUG, str);
                    }
                    if (0 < remgamefields && 0 == remdummyfields)
                    {
                        uint8_t v = randomvalue & 0x07;
                        if (v < N)
                        {
                            data.board[row][col] = v;
                            vcount[v]++;
                            gamefields++;
                            randomfields--;
                        }
                    }
                    else if (0 == remgamefields && 0 < remdummyfields)
                    {
                        data.board[row][col] = 0x81;
                        randomfields--;
                    }
                    if (0 == randomfields) scene = SCENE_GAME;
                }
                draw_board(data, vcount, targetvcount, STATE_BOARDUNDERCONSTRUCTION, level, ((Rectangle){WX,WY,windowedScreenWidth,windowedScreenHeight}), bg, tiles);
            } break;

            case SCENE_GAME:
            {
                uint8_t row = (mouse.y - WY - TILE_ORIGIN_Y) / TILESIZE;
                uint8_t col = (mouse.x - WX - TILE_ORIGIN_X) / TILESIZE;
                uint8_t rowmod = (uint32_t)(mouse.y - WY - TILE_ORIGIN_Y) % TILESIZE;
                uint8_t colmod = (uint32_t)(mouse.x - WX - TILE_ORIGIN_X) % TILESIZE;
                uint8_t lbound = (TILESIZE-TILECENTERSIZE)/2;
                uint8_t ubound = TILECENTERSIZE + lbound - 1;
                uint8_t v = data.board[row][col];
                //sprintf(str, "mouse:[%d,%d]->[%d,%d] mods=(%d,%d); v=%d", (uint16_t)mouse.x, (uint16_t)mouse.y, row, col, rowmod, colmod, v);
                //TraceLog(LOG_DEBUG, str);
                bool validloc = \
                (
                        (row < A && col < A)
                        &&
                        (v < N)
                        &&
                        (lbound <= rowmod && rowmod <= ubound && lbound <= colmod && colmod <= ubound)
                );
                if (validloc && (currentGesture != lastGesture && currentGesture == GESTURE_TAP))
                {
                    transform(data.board, vcount, row, col);
                    data.harvests++;
                    eqharvests = 0;
                }
                bool equilibrium = vcount_in_equilibrium(vcount, targetvcount);
                if (equilibrium) scene = SCENE_WIN;
                else if (eqharvests == 0)
                {
                    eqharvests = STATE_NEWLEVEL + level;
                    for (eqharvests=1; eqharvests <= MAXSIMHARVESTS; eqharvests++)
                    {
                        equilibrium = simulate(data.board, vcount, targetvcount, eqharvests);
                        if (equilibrium) break;
                    }
                    if (!equilibrium) eqharvests = STATE_MANYHARVESTS;
                }
                Rectangle viewport = {WX,WY,windowedScreenWidth,windowedScreenHeight};
                draw_board(data, vcount, targetvcount, eqharvests, level, viewport, bg, tiles);
                if (validloc && (currentGesture == GESTURE_NONE || currentGesture == GESTURE_DRAG))
                {
                    Rectangle dest = {viewport.x + TILE_ORIGIN_X + col * TILESIZE, viewport.y + TILE_ORIGIN_Y + row * TILESIZE, TILESIZE, TILESIZE};
                    DrawTexturePro(tiles, ((Rectangle){TILE_HOVER_COL * TILESIZE, 0, TILESIZE, TILESIZE}), dest, ((Vector2){0, 0}), 0, WHITE);
                }
            } break;

            case SCENE_WIN:
            {
                if (currentGesture != lastGesture && currentGesture == GESTURE_TAP)
                {
                    level++;
                    scene = SCENE_NEWGAME_LEVEL;
                }
                draw_board(data, vcount, targetvcount, STATE_WIN, level, ((Rectangle){WX,WY,windowedScreenWidth,windowedScreenHeight}), bg, tiles);
            } break;

            case SCENE_THANKS:
            {
                sprintf(str, "THANKS FOR PLAYING!");
                int strwidth = MeasureText(str, 20);
                Rectangle viewport = {WX,WY,windowedScreenWidth,windowedScreenHeight};
                DrawText(str, viewport.x + (viewport.width - strwidth) / 2, 100, 20, COLOR_FOREGROUND);
            }

        }

        //DrawFPS(screenWidth-100, 10); // for debug

        EndDrawing();
        //----------------------------------------------------------------------------------

        // Load by drop
        if (IsFileDropped())
        {
            FilePathList droppedfiles = LoadDroppedFiles();
            if (droppedfiles.count == 1)
            {
                bool success = load(droppedfiles.paths[0], &data, vcount, &targetvcount);
                if (success)
                {
                    level = 0;
                    scene = SCENE_NEWGAME;
                }
            }
            UnloadDroppedFiles(droppedfiles);
        }
        // Quickload
        if (IsKeyPressed(KEY_L))
        {
            sprintf(str, "%s\\%s", GetApplicationDirectory(), "puzzle.hortirata");
            bool success = load(str, &data, vcount, &targetvcount);
            if (success)
            {
                level = 0;
                scene = SCENE_NEWGAME;
            }
        }
        // Quicksave
        if (IsKeyPressed(KEY_S))
        {
            sprintf(str, "%s\\%s", GetApplicationDirectory(), "puzzle.hortirata");
            SaveFileData(str, data.board, A * A);
        }

    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    CloseWindow();        // Close window and OpenGL context
    UnloadTexture(bg);
    UnloadTexture(tiles);
    //--------------------------------------------------------------------------------------

    return 0;
}