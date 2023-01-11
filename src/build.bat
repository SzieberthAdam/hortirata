@echo off
setlocal
SET RAYLIB_PATH=C:\raylib\raylib
SET COMPILER_PATH=C:\raylib\w64devkit\bin
SET PATH=%COMPILER_PATH%
SET CFLAGS=%RAYLIB_PATH%\src\raylib.rc.data -s -static -O2 -std=c99 -Wall -I%RAYLIB_PATH%\src -Iexternal -DPLATFORM_DESKTOP
SET LDFLAGS=-lraylib -lopengl32 -lgdi32 -lwinmm

if exist ..\bin\hortirata.exe del /F ..\bin\hortirata.exe
gcc -o ..\bin\hortirata.exe hortirata.c %CFLAGS% %LDFLAGS% 2> build.log

copy /Y ..\artwork\bg.png ..\bin
copy /Y ..\artwork\tiles.png ..\bin

endlocal
