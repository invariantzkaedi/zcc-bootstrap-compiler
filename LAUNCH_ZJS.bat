@echo off
echo ===================================================
echo 🔱 ZKAEDI VMAX: 1-Click ZJS REPL + SVG Bridge Launcher
echo ===================================================
echo.
echo [1/3] Compiling ZJS Engine via ZCC...
wsl -e bash -c "cd /mnt/g/zccMAIN/zcc && make zjs"
if %errorlevel% neq 0 (
    echo.
    echo ❌ ZJS Compilation FAILED!
    pause
    exit /b 1
)

echo.
echo [2/3] Launching Visual SVG Diff Dashboard (localhost:8082)...
start wsl -e bash -c "cd /mnt/g/zccMAIN/zcc && make visualize-svg-diffs"

echo.
echo [3/3] Launching Interactive ZJS REPL...
echo.
wsl -e bash -c "cd /mnt/g/zccMAIN/zcc && ./zjs"
