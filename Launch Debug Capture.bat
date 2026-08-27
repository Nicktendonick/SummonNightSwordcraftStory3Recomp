@echo off
setlocal
cd /d "%~dp0"

title Summon Night Swordcraft Story 3 - Debug Capture
echo Starting the English-beta debug capture...
echo.
echo In game:
echo   F2       Normal picture
echo   F3-F6    Individual background layers
echo   F7       Sprites only
echo   F8       Pause or resume
echo   F9       Advance one frame while paused
echo   F10      Save a diagnostic capture
echo.

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\launch_visible_debugger.ps1" -Edition Beta -ViewWidth 284
set "capture_exit=%ERRORLEVEL%"

if not "%capture_exit%"=="0" (
    echo.
    echo DEBUG CAPTURE FAILED with error code %capture_exit%.
    echo Leave this window open and tell Codex what it says.
    echo.
    pause
    exit /b %capture_exit%
)

echo.
echo Capture finished successfully.
echo Opening the recordings folder...
start "" "%~dp0validation\visible-debugger"
timeout /t 2 /nobreak >nul
exit /b 0
