@echo off
title 🔱 ZKAEDI VMAX - DEMO SPRITES DASHBOARD
color 0B

echo [ SYSTEM INITIATING ]
echo Establishing local HTTP origin for 3D Exploit Topology Sprite Dashboard...

:: Navigate to the target repository
cd /d "g:\zccMAIN\zcc"

:: 1. Compile and regenerate latest zcc_sprites.svg
echo [ BOOTING ] Rebuilding ZJS and generating latest SVG via WSL...
wsl -e bash -c "cd /mnt/g/zccMAIN/zcc && make test-zjs"
if %errorlevel% neq 0 (
    echo.
    echo ❌ WARNING: SVG Generation failed. Proceeding with existing assets...
)

:: 2. Check if port 8000 is already in use by python
netstat -ano | find "8000" >nul
if %errorlevel% equ 0 (
    echo [ OK ] Local network already bound to Port 8000.
) else (
    echo [ BOOTING ] Standing up localized HTTP Server on Port 8000...
    start /min "ZKAEDI_HTTP_SERVER" python -m http.server 8000
    :: Wait 2 seconds for server to initialize
    timeout /t 2 /nobreak >nul
)

echo [ SECURE LINK ] Launching Sprite Dashboard...
start http://localhost:8000/demo_sprites.html

echo [ SUCCESS ] The browser execution context is active.
echo You may close this terminal window. The server runs in the background.
timeout /t 3 >nul
exit
