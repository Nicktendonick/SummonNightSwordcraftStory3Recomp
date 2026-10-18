@echo off
setlocal
cd /d "%~dp0"
set "SWORDCRAFT3_BATTLE_HUD_BORDERS=0"
set "game_exe=%~dp0build-beta\SummonNightSwordcraftStory3RecompBeta.exe"
if not exist "%game_exe%" (
    echo Build the English-beta executable first.
    pause
    exit /b 1
)
echo Starting with Battle HUD borders OFF for this session only...
"%game_exe%"
exit /b %ERRORLEVEL%
