@echo off
setlocal
echo Combat pilot - without the extra native correctness redraw
echo The normal game frame and expanded battle frame still render.
echo The expanded center is still checked against the normal game picture.
echo The usual combat launcher now also disables the extra redraw.
echo.
set "SWORDCRAFT3_CUSTOM_BATTLES=1"
set "SWORDCRAFT3_CUSTOM_OBJECTS=1"
set "SWORDCRAFT3_CUSTOM_REPLAY_CHECK=0"
call "%~dp0Launch Lake Renderer Test.bat"
