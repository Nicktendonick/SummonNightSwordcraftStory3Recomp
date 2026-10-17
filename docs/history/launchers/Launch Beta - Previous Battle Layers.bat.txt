@echo off
setlocal
cd /d "%~dp0"
set "SWORDCRAFT3_LEGACY_BATTLE_LAYERS=1"
"%~dp0build-beta\SummonNightSwordcraftStory3RecompBeta.exe"
exit /b %ERRORLEVEL%
