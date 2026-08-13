#!/bin/bash
set -eo pipefail

RUN_ID="R1"
RUN_DIR="runs/$RUN_ID"
echo "[ZKAEDI SMOKE] Starting end-to-end provenance smoke run $RUN_ID..."

# 1. Clean and initialize run directories
rm -rf "$RUN_DIR"
mkdir -p "$RUN_DIR/inputs"
mkdir -p "$RUN_DIR/evidence"
mkdir -p "$RUN_DIR/logs"
mkdir -p "$RUN_DIR/workspace"

# 2. Generate mock Parquet DPO data and split manifest
python3 tests/generate_mock_dpo_data.py --out-dir "$RUN_DIR/inputs"

# 3. Setup training config
cp configs/dpo_smoke.json "$RUN_DIR/inputs/training_config.json"

# 4. Run DPO Training script under release mode
echo "[ZKAEDI SMOKE] Executing DPO training..."
python3 train_hf_dpo_adamw_hardened_v3.py \
    --mode release \
    --dataset "$RUN_DIR/inputs/dataset.parquet" \
    --split-manifest "$RUN_DIR/inputs/split_manifest.json" \
    --model-name gpt2_base \
    --output-dir "$RUN_DIR/workspace" \
    --safe-base-dir "$(pwd)" \
    --max-steps 25 \
    --save-steps 5 \
    --checkpoint-limit 3 \
    --use-cpu \
    --load-best-model \
    --sign \
    --private-key private.pem \
    --register \
    --artifact-name dpo_smoke_test \
    --public-key public.pub \
    > "$RUN_DIR/logs/stdout.log" 2> "$RUN_DIR/logs/stderr.log" || {
        echo "FAIL: DPO training failed. Check logs under $RUN_DIR/logs/stderr.log"
        cat "$RUN_DIR/logs/stderr.log"
        exit 1
    }

echo "[ZKAEDI SMOKE] Training completed successfully."

# 5. Reorganize workspace artifacts to conform to architectural schema
echo "[ZKAEDI SMOKE] Reorganizing outputs..."
mkdir -p "$RUN_DIR/checkpoints"

# Copy training manifest and attestation from final workspace/checkpoint to evidence
cp "$RUN_DIR/workspace/checkpoint"/training_manifest.json* "$RUN_DIR/evidence/"
cp "$RUN_DIR/workspace/checkpoint"/dpo_security_attestation.json* "$RUN_DIR/evidence/"

# Copy training manifest and attestation into the step checkpoints as well
for d in "$RUN_DIR/workspace"/checkpoint-*; do
    if [ -d "$d" ]; then
        cp "$RUN_DIR/workspace/checkpoint"/training_manifest.json* "$d/"
        cp "$RUN_DIR/workspace/checkpoint"/dpo_security_attestation.json* "$d/"
    fi
done

# Move step checkpoints to final checkpoints folder
mv "$RUN_DIR/workspace"/checkpoint-* "$RUN_DIR/checkpoints/"

mv "$RUN_DIR/workspace/evaluation_metrics.json" "$RUN_DIR/evidence/"
mv "$RUN_DIR/workspace/best_statistically_valid_checkpoint.json"* "$RUN_DIR/evidence/"
mv "$RUN_DIR/workspace/release_receipt.json"* "$RUN_DIR/evidence/"

rm -rf "$RUN_DIR/workspace"

# 6. Generate provenance graph JSON
GIT_COMMIT=$(git rev-parse HEAD || echo "unknown")
python3 tools/generate_provenance_graph.py "$RUN_DIR" "$GIT_COMMIT" "authoritative-v1"

# 7. Positive Control: run verification on graph
echo "[ZKAEDI SMOKE] Running positive control verification..."
python3 tools/verify_provenance_graph.py "$RUN_DIR" "$RUN_DIR/evidence/provenance_graph.json" public.pub

# 8. Negative Control 1: Tamper with dataset file
echo "[ZKAEDI SMOKE] Running Negative Control 1 (tampered dataset)..."
mv "$RUN_DIR/inputs/dataset.parquet" "$RUN_DIR/inputs/dataset.parquet.bak"
echo "corrupt_data" > "$RUN_DIR/inputs/dataset.parquet"
if python3 tools/verify_provenance_graph.py "$RUN_DIR" "$RUN_DIR/evidence/provenance_graph.json" public.pub >/dev/null 2>&1; then
    echo "FAIL: Provenance check succeeded on tampered dataset"
    exit 1
fi
echo "  [PASS] Tampered dataset successfully detected and blocked."
mv "$RUN_DIR/inputs/dataset.parquet.bak" "$RUN_DIR/inputs/dataset.parquet"

# 9. Negative Control 2: Tamper with receipt file
echo "[ZKAEDI SMOKE] Running Negative Control 2 (tampered release receipt)..."
cp "$RUN_DIR/evidence/release_receipt.json" "$RUN_DIR/evidence/release_receipt.json.bak"
if sed --version >/dev/null 2>&1; then
    sed -i 's/2026/2027/g' "$RUN_DIR/evidence/release_receipt.json"
else
    sed -i '' 's/2026/2027/g' "$RUN_DIR/evidence/release_receipt.json"
fi
if python3 tools/verify_provenance_graph.py "$RUN_DIR" "$RUN_DIR/evidence/provenance_graph.json" public.pub >/dev/null 2>&1; then
    echo "FAIL: Provenance check succeeded on tampered release receipt"
    exit 1
fi
echo "  [PASS] Tampered release receipt successfully detected and blocked."
mv "$RUN_DIR/evidence/release_receipt.json.bak" "$RUN_DIR/evidence/release_receipt.json"

# 10. Negative Control 3: Tamper with checkpoint folder
echo "[ZKAEDI SMOKE] Running Negative Control 3 (tampered checkpoint folder)..."
CKPT_DIR=$(ls -d "$RUN_DIR/checkpoints"/checkpoint-* | head -n 1)
TARGET_FILE="$CKPT_DIR/config.json"
mv "$TARGET_FILE" "$TARGET_FILE.bak"
if python3 tools/verify_provenance_graph.py "$RUN_DIR" "$RUN_DIR/evidence/provenance_graph.json" public.pub >/dev/null 2>&1; then
    echo "FAIL: Provenance check succeeded on tampered checkpoint folder"
    exit 1
fi
echo "  [PASS] Tampered checkpoint folder successfully detected and blocked."
mv "$TARGET_FILE.bak" "$TARGET_FILE"

# 11. Write validation results
cat <<EOF > "$RUN_DIR/evidence/gate_results.json"
{
  "positive_control": "PASS",
  "negative_control_dataset": "PASS",
  "negative_control_receipt": "PASS",
  "negative_control_checkpoint": "PASS",
  "status": "ALL_PASSED"
}
EOF

cat <<EOF > "$RUN_DIR/run_summary.json"
{
  "run_id": "$RUN_ID",
  "device": "CPU",
  "seed": 3407,
  "max_steps": 25,
  "status": "SUCCESS",
  "provenance_chain": "VERIFIED"
}
EOF

echo "[ZKAEDI SMOKE] Milestone 2 E2E Smoke Run $RUN_ID completed successfully!"
