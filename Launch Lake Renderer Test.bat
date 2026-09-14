@echo off
setlocal
echo Custom renderer - lake and village scenery test
echo.
echo 1. 16:9 - recommended first test
echo 2. 2:1 - wider
echo 3. 12:5 - very wide; the finite map will leave black space
echo 4. Native GBA - comparison
echo.
choice /C 1234 /N /M "Choose display size (1-4): "
if errorlevel 4 (set "lab_width=240") else if errorlevel 3 (set "lab_width=384") else if errorlevel 2 (set "lab_width=320") else (set "lab_width=284")
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\launch_custom_native.ps1" -HostWidth %lab_width%
if errorlevel 1 pause
