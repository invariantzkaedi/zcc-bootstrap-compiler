#!/usr/bin/env python3
"""
ZKAEDI PRIME — Master God Dashboard Launcher
One-click: spawns HTTP server, opens browser, waits, shuts down cleanly on exit.
"""

import http.server
import os
import signal
import socketserver
import subprocess as _sp
import sys
import threading
import time
import webbrowser

# ── Fix Windows Console Unicode Encoding ─────────────────────────────────────
if sys.stdout.encoding.lower() != 'utf-8':
    sys.stdout.reconfigure(encoding='utf-8')

# ── Auto-install websockets if missing ───────────────────────────────────────
try:
    import websockets as _ws_check  # noqa: F401
except ImportError:
    print("[SETUP] Installing 'websockets' package...")
    _sp.run([sys.executable, "-m", "pip", "install", "websockets", "-q"], check=True)
    print("[SETUP] websockets installed ✓")

PORT    = 8042
WS_PORT = 8043
HOST    = "localhost"
REPO_ROOT  = os.path.dirname(os.path.abspath(__file__))
TARGET_URL = f"http://{HOST}:{PORT}/tools/dashboards/master_god_dashboard.html"

# ──────────────────────────────────────────────
# Cyan/Magenta terminal aesthetics (Windows ANSI)
# ──────────────────────────────────────────────
os.system("")  # Enable ANSI escape codes on Windows
CYAN    = "\033[96m"
MAGENTA = "\033[95m"
GREEN   = "\033[92m"
RED     = "\033[91m"
DIM     = "\033[2m"
RESET   = "\033[0m"
BOLD    = "\033[1m"

def banner():
    print(f"""
{MAGENTA}{BOLD}
 ╔══════════════════════════════════════════════════════╗
 ║        ZKAEDI PRIME — MASTER GOD DASHBOARD           ║
 ║          Recursively Coupled Hamiltonian HUD          ║
 ╚══════════════════════════════════════════════════════╝
{RESET}""")

def log(tag, msg, color=CYAN):
    timestamp = time.strftime("%H:%M:%S")
    print(f"{DIM}[{timestamp}]{RESET} {color}{BOLD}[{tag}]{RESET} {msg}")

# ──────────────────────────────────────────────
# Silent HTTP server (suppress request logs)
# ──────────────────────────────────────────────
class SilentHandler(http.server.SimpleHTTPRequestHandler):
    def log_message(self, format, *args):
        pass  # suppress per-request noise

    def log_error(self, format, *args):
        pass

# ──────────────────────────────────────────────
# Global server reference for graceful shutdown
# ──────────────────────────────────────────────
httpd = None

def preflight_check():
    """Validate JS brace balance in dashboard HTML before serving."""
    import re
    dashboard = os.path.join(REPO_ROOT, "tools", "dashboards", "master_god_dashboard.html")
    try:
        with open(dashboard, encoding="utf-8", errors="replace") as f:
            content = f.read()
        starts = [m.start() for m in re.finditer(r'<script[^>]*>', content)]
        ends   = [m.start() for m in re.finditer(r'</script>', content)]
        broken = []
        for i, (s, e) in enumerate(zip(starts, ends)):
            block = content[s:e]
            delta = block.count('{') - block.count('}')
            if delta != 0:
                broken.append(f"Block {i} (brace delta={delta:+d})")
        if broken:
            log("PREFLIGHT", "⚠️  JS BRACE MISMATCH DETECTED — dashboard may crash!", RED)
            for b in broken:
                log("PREFLIGHT", f"  {b}", RED)
            log("PREFLIGHT", "Launching anyway — check DevTools console for parse errors.", MAGENTA)
        else:
            log("PREFLIGHT", "JS brace balance OK ✓", GREEN)
    except Exception as ex:
        log("PREFLIGHT", f"Could not validate dashboard: {ex}", MAGENTA)

def start_server():
    global httpd
    os.chdir(REPO_ROOT)
    socketserver.TCPServer.allow_reuse_address = True
    httpd = socketserver.TCPServer((HOST, PORT), SilentHandler)
    log("SERVER", f"Listening at {CYAN}http://{HOST}:{PORT}{RESET}", GREEN)
    httpd.serve_forever()

def shutdown_server():
    global httpd
    if httpd:
        log("SHUTDOWN", "Closing HTTP server...", MAGENTA)
        httpd.shutdown()
        httpd.server_close()
        log("SHUTDOWN", "Server stopped cleanly.", GREEN)

# ──────────────────────────────────────────────
# Signal handler — Ctrl+C or window close
# ──────────────────────────────────────────────
def handle_signal(sig, frame):
    print()
    log("SIGNAL", f"Received {'SIGINT' if sig == signal.SIGINT else 'SIGTERM'}. Initiating graceful shutdown...", MAGENTA)
    shutdown_server()
    log("EXIT", f"{MAGENTA}ZKAEDI PRIME offline. Entropy decoupled.{RESET}", MAGENTA)
    sys.exit(0)

signal.signal(signal.SIGINT, handle_signal)
signal.signal(signal.SIGTERM, handle_signal)

# ──────────────────────────────────────────────
# Windows CTRL_CLOSE_EVENT handler
# Fires when user closes the console window (X button)
# signal.SIGINT alone does NOT catch this on Windows
# ──────────────────────────────────────────────
if sys.platform == "win32":
    import atexit
    import ctypes

    atexit.register(shutdown_server)  # safety net for any exit path

    CTRL_C_EVENT     = 0
    CTRL_CLOSE_EVENT = 2
    CTRL_BREAK_EVENT = 1

    HandlerRoutine = ctypes.WINFUNCTYPE(ctypes.c_bool, ctypes.c_uint)

    def _win_ctrl_handler(event):
        if event in (CTRL_C_EVENT, CTRL_CLOSE_EVENT, CTRL_BREAK_EVENT):
            log("SIGNAL", f"Windows console event {event} received. Shutting down...", MAGENTA)
            shutdown_server()
            log("EXIT", f"{MAGENTA}ZKAEDI PRIME offline. Entropy decoupled.{RESET}", MAGENTA)
            sys.exit(0)
        return False  # pass through other events

    _handler_ref = HandlerRoutine(_win_ctrl_handler)  # must keep reference alive
    ctypes.windll.kernel32.SetConsoleCtrlHandler(_handler_ref, True)


# ──────────────────────────────────────────────
# Main entrypoint
# ──────────────────────────────────────────────
def main():
    banner()
    preflight_check()  # Validate dashboard JS integrity before serving

    # Start server on background thread
    server_thread = threading.Thread(target=start_server, daemon=True)
    server_thread.start()

    # Give it a moment to bind
    time.sleep(0.5)

    # ── Start FHN Chaos WebSocket broadcaster on port 8043 ───────────────────
    try:
        sys.path.insert(0, REPO_ROOT)
        import fhn_chaos_ws
        fhn_chaos_ws.start_in_thread()
        log("FHN-WS", f"Chaos broadcaster live → {CYAN}ws://{HOST}:{WS_PORT}{RESET}", GREEN)
    except Exception as ex:
        log("FHN-WS", f"Chaos WS failed to start: {ex}", RED)

    time.sleep(0.5)

    # Verify port is alive before opening browser
    import socket
    for attempt in range(10):
        try:
            with socket.create_connection((HOST, PORT), timeout=1):
                break
        except (ConnectionRefusedError, OSError):
            time.sleep(0.3)
    else:
        log("ERROR", f"Port {PORT} failed to open after 10 retries. Aborting.", RED)
        shutdown_server()
        sys.exit(1)

    # Launch Edge with Web Serial API flags — bypass Enhanced Security Mode for localhost
    import subprocess

    EDGE_PATHS = [
        r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe",
        r"C:\Program Files\Microsoft\Edge\Application\msedge.exe",
    ]
    edge_exe = next((p for p in EDGE_PATHS if os.path.exists(p)), None)

    EDGE_FLAGS = [
        "--disable-features=msEnhancedSecurityMode",  # Web Serial blocked by ESM on localhost
        "--allow-insecure-localhost",                  # permit localhost non-HTTPS serial
        "--no-first-run",
        "--no-default-browser-check",
        TARGET_URL,
    ]

    if edge_exe:
        log("BROWSER", f"Launching Edge → {TARGET_URL}", CYAN)
        subprocess.Popen([edge_exe] + EDGE_FLAGS)
    else:
        log("BROWSER", "Edge not found, falling back to default browser", MAGENTA)
        webbrowser.open(TARGET_URL)

    log("READY", f"{GREEN}Dashboard is live. Press {BOLD}Ctrl+C{RESET}{GREEN} in this window to exit cleanly.{RESET}", GREEN)
    print(f"\n{DIM}  Server root : {REPO_ROOT}")
    print(f"  URL        : {TARGET_URL}")
    print(f"  Port       : {PORT}{RESET}\n")

    # Block main thread so daemon server thread stays alive
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        handle_signal(signal.SIGINT, None)

if __name__ == "__main__":
    main()
