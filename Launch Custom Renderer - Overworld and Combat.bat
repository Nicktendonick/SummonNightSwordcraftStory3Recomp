@echo off
setlocal
title Swordcraft 3 - Custom Overworld and Combat
echo Custom rendering - overworld and combat - 12:5
echo.
echo Includes five verified field maps and battle arenas 0, 2, 3 and 7.
if /I "%~1"=="--general-fields" (
echo EXPERIMENT: other compatible static field maps use ROM-backed source authentication.
echo Unfamiliar animation/layer formats and unsupported arenas remain native.
) else (
echo Other maps and arenas remain native until support is added.
)
echo Dialogue keeps its existing native framing.
echo The settings launcher opens before the game. Press F10 in-game to capture.
echo Uses the same saves and current build as the existing custom-renderer tests.
echo.
set "SWORDCRAFT3_CUSTOM_BATTLES=1"
set "SWORDCRAFT3_CUSTOM_OBJECTS=1"
set "SWORDCRAFT3_CUSTOM_ROCKY=1"
set "SWORDCRAFT3_CUSTOM_REPLAY_CHECK=0"
set "SWORDCRAFT3_CUSTOM_ADDITIONAL_AREAS=1"
set "SWORDCRAFT3_CUSTOM_GENERAL_FIELDS=0"
if /I "%~1"=="--general-fields" set "SWORDCRAFT3_CUSTOM_GENERAL_FIELDS=1"
if /I "%~1"=="--previous" set "SWORDCRAFT3_CUSTOM_ADDITIONAL_AREAS=0"
if /I "%~1"=="--check" goto check
if /I "%~2"=="--check" goto check
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\launch_custom_native.ps1" -HostWidth 384
set "launch_result=%errorlevel%"
if not "%launch_result%"=="0" pause
exit /b %launch_result%
:check
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\launch_custom_native.ps1" -HostWidth 384 -CheckOnly
exit /b %errorlevel%
