@echo off
"C:\Users\Nickt\AppData\Local\Programs\Python\Python313\python.exe" -B "%~dp0tools\ghidra\sc3_ghidra.py" gui --variant jp
if errorlevel 1 pause
