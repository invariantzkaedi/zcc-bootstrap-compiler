#!/usr/bin/env bash

echo "=== SEMANTIC MANIFOLD PROBE ==="
SCRIPT="/mnt/h/agents/semantic_manifold_processor.py"
JSON_IN="/mnt/h/agents/capabilities_dump.json"

# Create dummy input if missing
if [ ! -f "$JSON_IN" ]; then
  cat << 'EOF' > "$JSON_IN"
{
  "zcc-lua-001": "Computed goto implementation fails.",
  "zcc-lua-002": "Instruction dispatch table error.",
  "zcc-ir-016": "Dead code elimination misses branch."
}
EOF
fi

cd /mnt/h/agents
timeout 10 python3 "$SCRIPT" 2>&1
echo "STATUS=$?"
