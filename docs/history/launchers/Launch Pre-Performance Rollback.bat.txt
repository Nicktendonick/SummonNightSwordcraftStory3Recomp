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
echo Pre-performance rollback: complete-frame overworld and combat, 12:5.
echo Uses the same full-combat playtest save folder. Do not run both games together.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\launch_custom_native.ps1" -HostWidth 384 -IsolateFullCombat -PrePerformance
pause
