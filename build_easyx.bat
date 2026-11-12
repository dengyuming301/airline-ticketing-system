@echo off
setlocal
cd /d "%~dp0"

set EASYX=%~dp0third_party\easyx

rem EasyX MinGW libs need the MSVCRT build of MinGW-w64.
rem WinGet installs it under a path with a space, which breaks linking,
rem so prefer the space-free copy when it exists.
set "MINGW=C:\Users\Public\mingw64_msvcrt\bin"
if exist "%MINGW%\g++.exe" set "PATH=%MINGW%;%PATH%"

where g++ >nul 2>nul
if errorlevel 1 (
    echo [ERROR] g++ not found.
    echo         Expected toolchain: %MINGW%
    echo         Install MSVCRT MinGW-w64 or add g++ to PATH.
    pause
    exit /b 1
)

if not exist build mkdir build

echo [1/4] compiling airline.c ...
gcc -std=c99 -Wall -Wextra -O2 -c src\airline.c -o build\airline.o || goto fail

echo [2/4] compiling gui.c ...
g++ -std=c++14 -DUSE_EASYX -I"%EASYX%\include" -O2 -c src\gui.c -o build\gui.o || goto fail

echo [3/4] compiling easyx_view.c ...
g++ -std=c++14 -DUSE_EASYX -I"%EASYX%\include" -O2 -c src\easyx_view.c -o build\easyx_view.o || goto fail

echo [4/4] linking ...
rem Use a relative library path: MinGW's linker can mis-handle the Chinese characters in %~dp0.
g++ -static -static-libgcc -static-libstdc++ -mwindows -o airline_gui.exe build\airline.o build\gui.o build\easyx_view.o third_party\easyx\lib64\libeasyx.a -lgdi32 -lole32 -lwinmm || goto fail

echo.
echo Build OK: airline_gui.exe  (double-click to run, no console window)
pause
exit /b 0

:fail
echo.
echo Build failed. See the messages above.
pause
exit /b 1
