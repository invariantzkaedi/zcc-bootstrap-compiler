@echo off
title ZKAEDI PRIME - HAMILTONIAN VISUALIZER
color 0B

echo [ SYSTEM INITIATING ]
echo Establishing ZKAEDI Hamiltonian Visualization Matrix...

:: Navigate to the target repository
cd /d "g:\zccMAIN\zcc"

:: 1. Launch the HTTP Asset Server (Port 8081)
echo [ BOOTING ] Standing up ZKAEDI Asset Server...
start /min "ZKAEDI_ASSET_SERVER" python serve_dashboard.py
timeout /t 2 /nobreak >nul

:: 2. Launch the WebSockets Bridge (Port 8891)
echo [ BOOTING ] Standing up ZKAEDI WebSocket Telemetry Bridge...
start /min "ZKAEDI_WS_BRIDGE" python zkaedi_suno_ws_bridge.py
timeout /t 2 /nobreak >nul

echo [ SECURE LINK ] Launching Hamiltonian Visualizer Dashboard...
start http://localhost:8081/dashboard_hamiltonian_visualizer.html

echo [ SUCCESS ] The browser execution context is secured.
echo [ RECORDING ACTIVE ] Criticality auto-recorder is live and armed.
echo You may close this terminal window. The servers run in the background.
timeout /t 3 >nul
exit
