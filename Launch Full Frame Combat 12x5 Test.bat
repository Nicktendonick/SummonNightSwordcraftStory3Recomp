@echo off
setlocal
cd /d "%~dp0"
set SWORDCRAFT3_FULL_COMBAT_RENDERER=1
set SWORDCRAFT3_CUSTOM_BATTLES=1
set SWORDCRAFT3_CUSTOM_GENERAL_FIELDS=1
set SWORDCRAFT3_CUSTOM_OBJECTS=1
set SWORDCRAFT3_CUSTOM_ROCKY=1
set SWORDCRAFT3_CUSTOM_ADDITIONAL_AREAS=1
set SWORDCRAFT3_CUSTOM_REPLAY_CHECK=0
echo Experimental complete-frame combat renderer. Existing field renderer retained.
echo Close this test and use your usual launcher to roll back.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\launch_custom_native.ps1" -HostWidth 384 -IsolateFullCombat
pause
