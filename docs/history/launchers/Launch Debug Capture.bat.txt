@echo off
setlocal
cd /d "%~dp0"

title Summon Night Swordcraft Story 3 - Debug Capture
echo Opening the English-beta launcher with debug capture enabled...
echo Choose Display / aspect ratio and other settings, then press PLAY.
echo For live arena resizing, choose Adaptive view in Display.
echo Fixed Full Arena uses 384 pixels; Native, 16:9 and 2:1 are also available.
echo.
echo In game:
echo   Escape   Settings, including Display and Assist Tools
echo   F2       Normal picture
echo   F3-F6    Individual background layers
echo   F7       Sprites only
echo   F8       Pause or resume
echo   F9       Advance one frame while paused
echo   F10      Save a diagnostic capture
echo.

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\launch_visible_debugger.ps1" -Edition Beta -ShowLauncher
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
echo Debug capture session closed.
echo Opening the recordings folder...
start "" "%~dp0validation\visible-debugger"
timeout /t 2 /nobreak >nul
exit /b 0
