@echo off
title ZKAEDI PRIME — Master God Dashboard
color 0B

:: ── Locate Python ──────────────────────────────────────
where python >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] Python not found in PATH. Install Python 3.8+ and retry.
    pause
    exit /b 1
)

:: ── Kill any stale server on port 8042 ─────────────────
for /f "tokens=5" %%p in ('netstat -ano ^| findstr ":8042 "') do (
    taskkill /PID %%p /F >nul 2>&1
)

:: ── Launch (Python handles everything) ──────────────────
echo.
echo  Starting ZKAEDI PRIME...
echo  Close this window or press Ctrl+C to shut down.
echo.

python "%~dp0LAUNCH_MASTER_GOD.py"

:: ── If Python exits, this window auto-closes ────────────
exit
