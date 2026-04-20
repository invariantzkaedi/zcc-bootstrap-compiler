#!/usr/bin/env bash

echo "=== TELEMETRY TENSOR MATRIX PROBE ==="
SCRIPT="/mnt/h/agents/telemetry_to_tensor_matrix.py"
JSON_IN="/mnt/h/agents/fused_consciousness_telemetry.json"

# Create dummy input if missing so it doesn't fail parsing
if [ ! -f "$JSON_IN" ]; then
  cat << 'EOF' > "$JSON_IN"
{
  "hamiltonian": {
    "fullHistory": [
      {"t": 0.1, "dH": 0.05, "mse": 1.2, "H": 0.5, "feedback": 1.0},
      {"t": 0.2, "dH": 0.1, "mse": 4.5, "H": 0.6, "feedback": 2.5}
    ]
  }
}
EOF
fi

timeout 5 python3 "$SCRIPT" 2>&1
echo "STATUS=$?"
