@echo off
title 🔱 ZKAEDI VMAX - COSMIC ELEPHANT AUDIO REACTIVE VISUALIZER
color 0B

echo [ SYSTEM INITIATING ]
echo Establishing local HTTP origin for Cosmic Elephant Audio Reactive Dashboard...

:: Navigate to the directory of this batch file
cd /d "%~dp0"

:: Check if port 8080 is already in use
netstat -ano | find "8080" >nul
if %errorlevel% equ 0 (
    echo [ OK ] Local network already bound to Port 8080.
) else (
    echo [ BOOTING ] Standing up localized HTTP Server on Port 8080...
    start /min "ZKAEDI_COSMIC_SERVER" python -m http.server 8080
    :: Wait 2 seconds for server to initialize
    timeout /t 2 /nobreak >nul
)

echo [ SECURE LINK ] Launching Sprite Dashboard...
start http://localhost:8080/audio_reactive_creature.html

echo [ SUCCESS ] The browser execution context is active.
echo You may close this terminal window. The server runs in the background.
timeout /t 3 >nul
exit
