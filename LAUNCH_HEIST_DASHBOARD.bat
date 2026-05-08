@echo off
title 🦍🔱 ZKAEDI PRIME - DASHBOARD ORCHESTRATOR
color 0B

echo [ SYSTEM INITIATING ]
echo Establishing secure local origin for Masking & EVM Scraper...

:: Navigate to the target repository
cd /d "g:\zccMAIN\zcc"

:: Check if port 8000 is already in use by python
netstat -ano | find "8000" >nul
if %errorlevel% equ 0 (
    echo [ OK ] Local network already bound to Port 8000.
) else (
    echo [ BOOTING ] Standing up localized HTTP Server...
    start /min "ZKAEDI_HTTP_SERVER" python -m http.server 8000
    :: Wait 2 seconds for server to initialize
    timeout /t 2 /nobreak >nul
)

echo [ SECURE LINK ] Launching Heist Dashboard...
start http://localhost:8000/heist_dashboard.html

echo [ SUCCESS ] The browser execution context is secured.
echo You may close this terminal window. The server runs in the background.
timeout /t 3 >nul
exit
