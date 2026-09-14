@echo off
setlocal
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\launch_custom_native.ps1"
if errorlevel 1 pause
