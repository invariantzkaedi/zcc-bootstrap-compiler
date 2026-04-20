#!/bin/bash
# IR Telemetry Corpus Harvester

set -e

echo "🔱 ZCC IR Telemetry Corpus Harvester"
echo "===================================="
echo ""

EXPERIMENTS=(
    "exp1_raytracer_simd"
    "exp2_voxel_engine"
    "exp3_audio_visualizer"
    "exp4_vr_stereo"
    "exp5_physics_engine"
)

OUTPUT_DIR="ir_corpus_v2"
mkdir -p "$OUTPUT_DIR"

for exp in "${EXPERIMENTS[@]}"; do
    echo "Processing: ${exp}.c"
    
    # Compile with telemetry
    if ./zcc "${exp}.c" --ir --telemetry > "${OUTPUT_DIR}/${exp}_ir.jsonl" 2>&1; then
        # Count events
        lines=$(wc -l < "${OUTPUT_DIR}/${exp}_ir.jsonl")
        size=$(du -h "${OUTPUT_DIR}/${exp}_ir.jsonl" | cut -f1)
        echo "  ✓ Generated ${lines} events (${size})"
        
        # Validate JSON
        python3 << PYEOF
import json
valid = 0
invalid = 0
with open("${OUTPUT_DIR}/${exp}_ir.jsonl") as f:
    for line in f:
        if line.strip():
            try:
                json.loads(line)
                valid += 1
            except:
                invalid += 1
print(f"  ✓ Valid: {valid}, Invalid: {invalid}")
PYEOF
    else
        echo "  ✗ FAILED"
    fi
    echo ""
done

echo "===================================="
echo "Corpus harvested to: $OUTPUT_DIR/"
ls -lh "$OUTPUT_DIR/"
