@echo off
setlocal
cd /d "%~dp0"

rem Optional space-free local copy (WinGet's path contains a space, which
rem can break linking). Ignored on machines where it does not exist.
if exist "C:\Users\Public\mingw64\bin\gcc.exe" set "PATH=C:\Users\Public\mingw64\bin;%PATH%"
rem Explicit override: set MINGW_BIN to the bin directory of MinGW-w64.
if defined MINGW_BIN set "PATH=%MINGW_BIN%;%PATH%"

where gcc >nul 2>nul
if errorlevel 1 (
    echo [ERROR] gcc not found.
    echo         Install MinGW-w64 and add gcc to PATH,
    echo         or set MINGW_BIN to its bin directory.
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
