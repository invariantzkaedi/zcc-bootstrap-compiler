@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\launch_lora_studio.ps1" -Route "/runner" %*
endlocal
