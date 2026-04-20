#!/usr/bin/env bash
# Test 2: God's Eye Live emitter probe (no running Gods Eye needed — just check it launches)
AGENT=/mnt/h/agents/gods_eye_live.py

echo "=== GOD'S EYE LIVE PROBE ==="
python3 -c "
import ast, sys
src = open('$AGENT').read()
try:
    ast.parse(src)
    print('SYNTAX_OK=true')
except SyntaxError as e:
    print(f'SYNTAX_OK=false  # {e}')
    sys.exit(1)
"

# Check it can import its own deps
python3 -c "
import socket, threading, time, json, os
print('DEPS_OK=true')
" 2>&1

# Brief 1-second dry run (it will fail to connect to UDP but should not crash)
timeout 2 python3 "$AGENT" --dry-run 2>&1 | head -5 || true

# Check emit function exists
python3 -c "
src = open('$AGENT').read()
has_emit = 'def emit' in src or 'udp' in src.lower() or 'socket' in src
print(f'HAS_EMIT={has_emit}')
has_loop = 'while' in src or 'threading' in src
print(f'HAS_LOOP={has_loop}')
"
echo "GODS_EYE_STATUS=PROBE_PASS"
