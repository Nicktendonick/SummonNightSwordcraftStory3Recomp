@echo off
setlocal
cd /d "%~dp0"

set "SWORDCRAFT3_BATTLE_MARGIN_MODE=loop"
set "game_exe=%~dp0build-beta\SummonNightSwordcraftStory3RecompBeta.exe"

if not exist "%game_exe%" (
    echo The English-beta executable has not been built yet:
    echo   %game_exe%
    echo.
    echo Build the project, then run this file again.
    pause
    exit /b 1
)

echo Starting the English beta with the previous repeating battle margins...
"%game_exe%"
exit /b %ERRORLEVEL%
