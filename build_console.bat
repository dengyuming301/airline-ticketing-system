@echo off
setlocal
cd /d "%~dp0"

rem WinGet installs MinGW under "C:\Users\Hyper X\..." (path contains a space),
rem which breaks the linker. Use the space-free copy instead when available.
set "MINGW=C:\Users\Public\mingw64\bin"
if exist "%MINGW%\gcc.exe" set "PATH=%MINGW%;%PATH%"

where gcc >nul 2>nul
if errorlevel 1 (
    echo [ERROR] gcc not found.
    echo         Expected toolchain: %MINGW%
    echo         Install MinGW-w64 or add gcc to PATH.
    pause
    exit /b 1
)

echo [1/1] Building console version ...
gcc -std=c99 -Wall -Wextra -O2 -o airline_console.exe src\airline.c src\main.c src\easyx_view.c
if errorlevel 1 (
    echo.
    echo Build failed. See the messages above.
    pause
    exit /b 1
)

echo Build OK: airline_console.exe
pause
