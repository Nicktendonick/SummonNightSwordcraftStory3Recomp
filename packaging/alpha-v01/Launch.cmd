@echo off
setlocal
title Alpha unfinished-build v.01 - Local review
cd /d "%~dp0"
"%~dp0SummonNightSwordcraftStory3RecompBeta.exe" --launcher
if errorlevel 1 pause
