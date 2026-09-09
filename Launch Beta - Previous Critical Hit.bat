@echo off
setlocal
cd /d "%~dp0"
set "SWORDCRAFT3_CRITICAL_WIDESCREEN=0"
"%~dp0build-beta\SummonNightSwordcraftStory3RecompBeta.exe"
exit /b %ERRORLEVEL%
