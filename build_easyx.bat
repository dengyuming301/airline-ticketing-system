@echo off
setlocal
cd /d "%~dp0"

set EASYX=%~dp0third_party\easyx

rem EasyX MinGW libs need the MSVCRT build of MinGW-w64.
rem Optional space-free local copy (WinGet's path contains a space, which
rem can break linking). Ignored on machines where it does not exist.
if exist "C:\Users\Public\mingw64_msvcrt\bin\g++.exe" set "PATH=C:\Users\Public\mingw64_msvcrt\bin;%PATH%"
rem Explicit override: set MINGW_BIN to the bin directory of an MSVCRT MinGW-w64.
if defined MINGW_BIN set "PATH=%MINGW_BIN%;%PATH%"

where g++ >nul 2>nul
if errorlevel 1 (
    echo [ERROR] g++ not found.
    echo         Install an MSVCRT build of MinGW-w64 and add g++ to PATH,
    echo         or set MINGW_BIN to its bin directory.
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
