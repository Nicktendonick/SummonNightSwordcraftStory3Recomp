@echo off
setlocal
cd /d "%~dp0"
set SWORDCRAFT3_FULL_FIELD_RENDERER=1
set SWORDCRAFT3_FULL_COMBAT_RENDERER=1
set SWORDCRAFT3_CUSTOM_BATTLES=1
set SWORDCRAFT3_CUSTOM_GENERAL_FIELDS=1
set SWORDCRAFT3_CUSTOM_OBJECTS=1
set SWORDCRAFT3_CUSTOM_ROCKY=1
set SWORDCRAFT3_CUSTOM_ADDITIONAL_AREAS=1
echo Full-frame custom renderer: OVERWORLD AND COMBAT - 12:5.
echo Edge decoration and camera experiments are OFF. F10 captures remain available.
echo Close this test and use Launch Accepted Full Combat Rollback.bat to roll back.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\launch_custom_native.ps1" -HostWidth 384 -IsolateFullCombat
pause
