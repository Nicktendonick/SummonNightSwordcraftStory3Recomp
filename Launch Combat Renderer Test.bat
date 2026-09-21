@echo off
setlocal
echo Custom combat renderer pilot - forest and Manig Mine rocky arenas
echo Includes centered HUD borders, battle layers, fighters and critical-hit replay.
echo Other arenas remain native until their source maps are verified.
echo Close this and use the usual lake launcher for the accepted field-only behavior.
echo.
set "SWORDCRAFT3_CUSTOM_BATTLES=1"
set "SWORDCRAFT3_CUSTOM_OBJECTS=1"
set "SWORDCRAFT3_CUSTOM_REPLAY_CHECK=0"
call "%~dp0Launch Lake Renderer Test.bat"
