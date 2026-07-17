#!/usr/bin/env bash
# gate.sh — ZCC Determinism Lock Verification Gate
set -euo pipefail

# Define exit status codes
EXIT_OK=0
EXIT_ASM_DIVERG=20
EXIT_OBJ_DIVERG=21
EXIT_BIN_DIVERG=22
EXIT_LINEAGE_FAIL=30
EXIT_STALE_OUTPUT=31
EXIT_COMPILER_MUTATED=32
EXIT_TOOL_FAIL=40
EXIT_SCHEMA_FAIL=41
EXIT_NEG_CONTROL_PASSED=50
EXIT_BYPASS_CONTROL_PASSED=51

# 1. Environment Isolation Configuration
CLEAN_ENV=(env -i)
for var in PATH TMPDIR ZCC_IR_BACKEND; do
    if [[ -v "$var" ]]; then
        CLEAN_ENV+=("$var=${!var}")
    fi
done
CLEAN_ENV+=(LC_ALL=C LANG=C TZ=UTC SOURCE_DATE_EPOCH=1700000000)

# Helper functions
sha256_file() {
    local file_path="$1"
    if [ ! -s "$file_path" ]; then
        echo "FAIL: File $file_path does not exist or is empty." >&2
        exit "$EXIT_STALE_OUTPUT"
    fi
    sha256sum "$file_path" | cut -d' ' -f1
}

# 2. Toolchain Identity Recording
record_toolchain() {
    local out_path="$1"
    
    local as_bin
    local ld_bin
    local objcopy_bin
    local strip_bin
    
    as_bin=$(which as)
    ld_bin=$(which ld)
    objcopy_bin=$(which objcopy)
    strip_bin=$(which strip)
    
    local as_sha
    local ld_sha
    local objcopy_sha
    local strip_sha
    
    as_sha=$(sha256_file "$as_bin")
    ld_sha=$(sha256_file "$ld_bin")
    objcopy_sha=$(sha256_file "$objcopy_bin")
    strip_sha=$(sha256_file "$strip_bin")
    
    cat > "$out_path" <<EOF
{
  "as_path": "$as_bin",
  "as_sha256": "$as_sha",
  "ld_path": "$ld_bin",
  "ld_sha256": "$ld_sha",
  "objcopy_path": "$objcopy_bin",
  "objcopy_sha256": "$objcopy_sha",
  "strip_path": "$strip_bin",
  "strip_sha256": "$strip_sha"
}
EOF
}

# 3. Provenance JSONL logging (Single-line formatting for valid JSONL)
write_provenance_jsonl() {
    local out_path="$1"
    local stage="$2"
    local compiler_path="$3"
    local comp_sha_before="$4"
    local comp_sha_after="$5"
    local src_sha="$6"
    local start_ns="$7"
    local end_ns="$8"
    local exit_code="$9"
    local asm_sha="${10}"
    local obj_sha="${11}"
    local bin_sha="${12}"
    local run_id="${13}"
    local argv_str="${14}"
    
    echo "{\"schema_version\":\"1.0\",\"run_id\":\"$run_id\",\"stage\":$stage,\"compiler_path\":\"$compiler_path\",\"compiler_sha256_before\":\"$comp_sha_before\",\"compiler_sha256_after\":\"$comp_sha_after\",\"source_sha256\":\"$src_sha\",\"argv\":$argv_str,\"environment\":{\"LC_ALL\":\"C\",\"TZ\":\"UTC\",\"SOURCE_DATE_EPOCH\":\"1700000000\"},\"target\":\"x86-64-pc-linux-gnu\",\"start_time_ns\":$start_ns,\"end_time_ns\":$end_ns,\"exit_code\":$exit_code,\"assembly_sha256\":\"$asm_sha\",\"object_sha256\":\"$obj_sha\",\"binary_sha256\":\"$bin_sha\"}" >> "$out_path"
}

# Python schema validator
validate_schema() {
    local ledger_path="$1"
    local schema_path="$2"
    
    python3 -c "
import json
import sys

try:
    import jsonschema
except ImportError:
    # Fallback checking if jsonschema library is missing
    print('WARNING: jsonschema library not installed. Doing structural checks.')
    try:
        with open('$ledger_path') as f:
            for line in f:
                data = json.loads(line.strip())
                for key in ['schema_version', 'run_id', 'stage', 'compiler_path', 'compiler_sha256_before', 'compiler_sha256_after', 'source_sha256', 'argv', 'environment', 'target', 'start_time_ns', 'end_time_ns', 'exit_code', 'assembly_sha256', 'object_sha256', 'binary_sha256']:
                    if key not in data:
                        sys.exit($EXIT_SCHEMA_FAIL)
        sys.exit(0)
    except Exception as e:
        print(f'Structural parse failure: {e}')
        sys.exit($EXIT_SCHEMA_FAIL)

try:
    with open('$schema_path') as sf:
        schema = json.load(sf)
    with open('$ledger_path') as lf:
        for line in lf:
            data = json.loads(line.strip())
            jsonschema.validate(instance=data, schema=schema)
    sys.exit(0)
except Exception as e:
    print(f'JSON Schema validation failed: {e}', file=sys.stderr)
    sys.exit($EXIT_SCHEMA_FAIL)
"
}

# Lineage validation logic
validate_lineage() {
    local ledger_path="$1"
    local run_id="$2"
    python3 -c "
import json
import sys

ledger_path = '$ledger_path'
expected_run_id = '$run_id'
stage2_bin_sha = None
stage3_compiler_sha = None

try:
    with open(ledger_path) as f:
        for line in f:
            data = json.loads(line.strip())
            if data.get('run_id') == expected_run_id:
                if data.get('stage') == 2:
                    stage2_bin_sha = data.get('binary_sha256')
                elif data.get('stage') == 3:
                    stage3_compiler_sha = data.get('compiler_sha256_before')
except Exception as e:
    print(f'Lineage read failure: {e}', file=sys.stderr)
    sys.exit($EXIT_LINEAGE_FAIL)

if not stage2_bin_sha or not stage3_compiler_sha:
    print('FAIL: Missing lineage ledger records matching current run_id.', file=sys.stderr)
    sys.exit($EXIT_LINEAGE_FAIL)

if stage2_bin_sha != stage3_compiler_sha:
    print(f'FAIL: Compiler lineage validation failed. Stage-3 compiler sha ({stage3_compiler_sha}) does not match Stage-2 binary sha ({stage2_bin_sha}).', file=sys.stderr)
    sys.exit($EXIT_LINEAGE_FAIL)

print('Lineage check: PASS')
"
}

# 4. Independent Generation Execution Loop
compile_stage() {
    local run_dir="$1"
    local stage_num="$2"
    local compiler_src_bin="$3"
    local source_file="$4"
    local out_prefix="$5"
    local ledger_file="$6"
    local run_id="$7"
    
    local out_s="$run_dir/${out_prefix}.s"
    local out_obj="$run_dir/${out_prefix}.o"
    local out_bin="$run_dir/${out_prefix}.bin"
    
    # Verify pre-generation path emptiness (Freshness check)
    if [ -e "$out_s" ] || [ -e "$out_obj" ] || [ -e "$out_bin" ]; then
        echo "FAIL: Freshness validation failed. Stale file detected in target path." >&2
        exit "$EXIT_STALE_OUTPUT"
    fi
    
    # 5. Anti-TOCTOU copy setup
    local exec_dir="$run_dir/exec_stage_${stage_num}"
    mkdir -p "$exec_dir"
    local compiler_exec="$exec_dir/zcc-compiler"
    install -m 0555 "$compiler_src_bin" "$compiler_exec"
    
    local comp_sha_before
    local comp_sha_after
    comp_sha_before=$(sha256_file "$compiler_exec")
    
    local src_sha
    src_sha=$(sha256_file "$source_file")
    
    local start_ns
    start_ns=$(date +%s%N)
    
    local exit_code=0
    # Compile Assembly
    "${CLEAN_ENV[@]}" "$compiler_exec" "$source_file" -S -o "$out_s" > "$run_dir/stage${stage_num}_compile_s.log" 2>&1 || exit_code=$?
    if [ "$exit_code" -ne 0 ]; then
        echo "FAIL: Stage $stage_num compiler assembly generation failed with exit code $exit_code" >&2
        exit "$exit_code"
    fi
    
    # Compile Linked Binary directly through driver
    "${CLEAN_ENV[@]}" "$compiler_exec" "$source_file" -o "$out_bin" > "$run_dir/stage${stage_num}_compile_bin.log" 2>&1 || exit_code=$?
    if [ "$exit_code" -ne 0 ]; then
        echo "FAIL: Stage $stage_num compiler binary generation failed with exit code $exit_code" >&2
        exit "$exit_code"
    fi
    
    # Strip symbols to eliminate transient tempfile object strings in .strtab
    "${CLEAN_ENV[@]}" strip --strip-all "$out_bin" > "$run_dir/stage${stage_num}_strip.log" 2>&1 || exit_code=$?
    if [ "$exit_code" -ne 0 ]; then
        echo "FAIL: Strip failed with exit code $exit_code" >&2
        exit "$EXIT_TOOL_FAIL"
    fi
    
    # Compile only to object file for comparative audit
    "${CLEAN_ENV[@]}" "$compiler_exec" "$source_file" -c -o "$out_obj" > "$run_dir/stage${stage_num}_compile_obj.log" 2>&1 || exit_code=$?
    if [ "$exit_code" -ne 0 ]; then
        echo "FAIL: Object compilation failed with exit code $exit_code" >&2
        exit "$exit_code"
    fi
    
    local end_ns
    end_ns=$(date +%s%N)
    
    # Verify compiler binary did not mutate during run
    comp_sha_after=$(sha256_file "$compiler_exec")
    if [ "$comp_sha_before" != "$comp_sha_after" ]; then
        echo "FAIL: Compiler mutated during run." >&2
        exit "$EXIT_COMPILER_MUTATED"
    fi
    
    local asm_sha
    local obj_sha
    local bin_sha
    asm_sha=$(sha256_file "$out_s")
    obj_sha=$(sha256_file "$out_obj")
    bin_sha=$(sha256_file "$out_bin")
    
    # Format argv to JSON array
    local argv_str="[\"$compiler_exec\", \"$source_file\", \"-o\", \"$out_bin\"]"
    
    write_provenance_jsonl "$ledger_file" "$stage_num" "$compiler_exec" "$comp_sha_before" "$comp_sha_after" "$src_sha" "$start_ns" "$end_ns" "$exit_code" "$asm_sha" "$obj_sha" "$bin_sha" "$run_id" "$argv_str"
}

# Factored constraint validation
verify_gate_constraints() {
    local run_dir="$1"
    local ledger_file="$2"
    local schema_path="$3"
    local run_id="$4"
    
    # Normalizer execution (Explicit Allow-list: ELF .note.gnu.build-id note section removal only)
    normalize_bin() {
        local input="$1"
        local output="${input}.norm"
        rm -f "$output"
        "${CLEAN_ENV[@]}" objcopy --remove-section=.note.gnu.build-id "$input" "$output"
    }
    
    normalize_bin "$run_dir/stage2.bin"
    normalize_bin "$run_dir/stage3.bin"
    
    # 3. Assert Distinct Inodes & Creation Sequence (Distinctness verification)
    local s2_inode
    local s3_inode
    s2_inode=$(stat -c '%i' "$run_dir/stage2.bin")
    s3_inode=$(stat -c '%i' "$run_dir/stage3.bin")
    if [ "$s2_inode" -eq "$s3_inode" ]; then
        echo "FAIL: Stage-2 and Stage-3 share identical filesystem inode." >&2
        return "$EXIT_STALE_OUTPUT"
    fi
    
    local s2_mtime
    local s3_mtime
    s2_mtime=$(stat -c '%Y' "$run_dir/stage2.bin")
    s3_mtime=$(stat -c '%Y' "$run_dir/stage3.bin")
    if [ "$s3_mtime" -lt "$s2_mtime" ]; then
        echo "FAIL: Stage-3 modified timestamp precedes Stage-2." >&2
        return "$EXIT_STALE_OUTPUT"
    fi
    
    # 4. Compare assembly and binaries
    local cmp_status=0
    cmp -s "$run_dir/stage2.s" "$run_dir/stage3.s" || cmp_status=$?
    if [ "$cmp_status" -ne 0 ]; then
        echo "FAIL: Assembly files diverged." >&2
        return "$EXIT_ASM_DIVERG"
    fi
    
    cmp_status=0
    cmp -s "$run_dir/stage2.o" "$run_dir/stage3.o" || cmp_status=$?
    if [ "$cmp_status" -ne 0 ]; then
        echo "FAIL: Object files diverged." >&2
        return "$EXIT_OBJ_DIVERG"
    fi
    
    cmp_status=0
    cmp -s "$run_dir/stage2.bin.norm" "$run_dir/stage3.bin.norm" || cmp_status=$?
    if [ "$cmp_status" -ne 0 ]; then
        echo "FAIL: Binary files diverged post-normalization." >&2
        return "$EXIT_BIN_DIVERG"
    fi
    
    # Schema validation of resulting ledger
    validate_schema "$ledger_file" "$schema_path"
    
    # Lineage validation
    validate_lineage "$ledger_file" "$run_id"
}

# Core main gate execution
run_main_gate() {
    local run_dir="$1"
    local stage1_bin="$2"
    local source_file="$3"
    local ledger_file="$4"
    local run_id="$5"
    local schema_path="$6"
    
    # 1. Compile Stage 2
    compile_stage "$run_dir" 2 "$stage1_bin" "$source_file" "stage2" "$ledger_file" "$run_id"
    
    # 2. Compile Stage 3 (Lineage check: Stage 2 compiles itself)
    compile_stage "$run_dir" 3 "$run_dir/stage2.bin" "$source_file" "stage3" "$ledger_file" "$run_id"
    
    # Verify comparisons and provenance constraints
    verify_gate_constraints "$run_dir" "$ledger_file" "$schema_path" "$run_id"
}

# Freshness test
verify_freshness() {
    local path="$1"
    if [ -e "$path" ] || [ -L "$path" ]; then
        echo "FAIL: Freshness constraint failed for path $path." >&2
        exit "$EXIT_STALE_OUTPUT"
    fi
}

# 6. Self-Testing Framework
run_self_test() {
    local schema_path="$1"
    local RUN_ID
    RUN_ID="self-test-$(date +%s)"
    
    echo "=== EXECUTING SELF-TEST HARNESS ==="
    
    # Setup simulated run paths
    local test_run_dir
    test_run_dir="$(pwd)/build/gate-self-test-a-$RUN_ID"
    mkdir -p "$test_run_dir"
    local test_ledger="$test_run_dir/test_provenance.jsonl"
    local test_run_id="test-run-${RUN_ID}"
    
    # Copy host zcc or gcc to act as Stage 1 compiler
    local stage1_src="./zcc"
    if [ ! -f "$stage1_src" ]; then
        stage1_src=$(which gcc)
    fi
    
    # In a self-hosting bootstrap, the source file compiled must be zcc.c itself!
    local bootstrap_src="zcc.c"
    if [ ! -f "$bootstrap_src" ]; then
        echo "FAIL: Missing zcc.c in workspace." >&2
        exit 1
    fi
    
    # Test A: Main passing run
    echo "Testing Main Gate (Expected: PASS)..."
    run_main_gate "$test_run_dir" "$stage1_src" "$bootstrap_src" "$test_ledger" "$test_run_id" "$schema_path"
    echo "Result A: PASS (Main gate verified successfully)"
    
    # Test B: Negative Control test (Expected: EXIT_ASM_DIVERG = 20)
    echo "Testing Negative Control Injection (Assembly) (Expected: exit 20)..."
    local test_run_dir_b
    test_run_dir_b="$(pwd)/build/gate-self-test-b-$RUN_ID"
    mkdir -p "$test_run_dir_b"
    local test_ledger_b="$test_run_dir_b/test_provenance_b.jsonl"
    
    # Copy compiled artifacts from Test A to bypass slow compiler recompiles
    cp -r "$test_run_dir"/* "$test_run_dir_b/"
    
    # Inject assembly mutation (Use GAS comment character #)
    echo "# mutated" >> "$test_run_dir_b/stage3.s"
    
    # Verify constraint verification fails with exit code 20
    local exit_b=0
    verify_gate_constraints "$test_run_dir_b" "$test_ledger_b" "$schema_path" "$test_run_id" || exit_b=$?
    if [ "$exit_b" -eq "$EXIT_ASM_DIVERG" ]; then
        echo "Result B: PASS (Divergence successfully caught with code $EXIT_ASM_DIVERG)"
    else
        echo "FAIL: Test B negative control failed to catch assembly mutation. Expected $EXIT_ASM_DIVERG, got $exit_b" >&2
        exit "$EXIT_NEG_CONTROL_PASSED"
    fi
    
    # Test B2: Negative Control test (Binary) (Expected: EXIT_BIN_DIVERG = 22)
    echo "Testing Negative Control Injection (Binary) (Expected: exit 22)..."
    local test_run_dir_b2
    test_run_dir_b2="$(pwd)/build/gate-self-test-b2-$RUN_ID"
    mkdir -p "$test_run_dir_b2"
    local test_ledger_b2="$test_run_dir_b2/test_provenance_b2.jsonl"
    
    # Copy compiled artifacts from Test A
    cp -r "$test_run_dir"/* "$test_run_dir_b2/"
    
    # Inject binary mutation into stage3.bin (non-volatile region, offset 64)
    printf '\xff' | dd of="$test_run_dir_b2/stage3.bin" bs=1 seek=64 conv=notrunc status=none
    
    # Verify constraint verification fails with exit code 22
    local exit_b2=0
    verify_gate_constraints "$test_run_dir_b2" "$test_ledger_b2" "$schema_path" "$test_run_id" || exit_b2=$?
    if [ "$exit_b2" -eq "$EXIT_BIN_DIVERG" ]; then
        echo "Result B2: PASS (Binary divergence successfully caught with code $EXIT_BIN_DIVERG)"
    else
        echo "FAIL: Test B2 negative control failed to catch binary mutation. Expected $EXIT_BIN_DIVERG, got $exit_b2" >&2
        exit "$EXIT_NEG_CONTROL_PASSED"
    fi
    
    # Test C: Copy Bypass validation (Expected: EXIT_LINEAGE_FAIL = 30)
    echo "Testing Copy-Bypass Rejection (Expected: exit 30)..."
    local test_run_dir_c
    test_run_dir_c="$(pwd)/build/gate-self-test-c-$RUN_ID"
    mkdir -p "$test_run_dir_c"
    local test_ledger_c="$test_run_dir_c/test_provenance_c.jsonl"
    
    # Only copy Stage 2 files, and simulate a copy bypass of Stage 2 to Stage 3
    cp "$test_run_dir"/stage2* "$test_run_dir_c/"
    cp "$test_run_dir"/test_provenance.jsonl "$test_ledger_c"
    
    # Bypass: Copy stage2.bin to stage3.bin, and copy stage2.s/stage2.o to stage3.s/stage3.o (freshness satisfy)
    cp "$test_run_dir_c/stage2.bin" "$test_run_dir_c/stage3.bin"
    cp "$test_run_dir_c/stage2.s" "$test_run_dir_c/stage3.s"
    cp "$test_run_dir_c/stage2.o" "$test_run_dir_c/stage3.o"
    
    # Mutate the ledger in test_ledger_c so that the record for stage 3 is deleted, simulating the bypass
    grep -v '"stage":3' "$test_run_dir"/test_provenance.jsonl > "$test_ledger_c" || true
    
    # Verify constraints fail with exit code 30
    local exit_c=0
    verify_gate_constraints "$test_run_dir_c" "$test_ledger_c" "$schema_path" "$test_run_id" || exit_c=$?
    if [ "$exit_c" -eq "$EXIT_LINEAGE_FAIL" ]; then
        echo "Result C: PASS (Bypass successfully caught with code $EXIT_LINEAGE_FAIL)"
    else
        echo "FAIL: Test C copy bypass check failed to catch bypass. Expected $EXIT_LINEAGE_FAIL, got $exit_c" >&2
        exit "$EXIT_BYPASS_CONTROL_PASSED"
    fi
    
    echo "=== ALL SELF-TEST CONTROL CHECKS PASSED ==="
}

# --- CLI Dispatcher ---
if [ "${1:-}" = "--self-test" ]; then
    SCHEMA_FILE="contracts/determinism-gate.schema.json"
    run_self_test "$SCHEMA_FILE"
    exit 0
fi

# Main Executable Action
RUN_ID="zcc-run-$(date +%s)-$$"
EVIDENCE_DIR="evidence/$RUN_ID"
mkdir -p "$EVIDENCE_DIR"
LEDGER_FILE="$EVIDENCE_DIR/provenance.jsonl"
SCHEMA_FILE="contracts/determinism-gate.schema.json"

echo "=== INITIALIZING ZCC BOOTSTRAP DETERMINISM GATE ==="
echo "Run ID: $RUN_ID"
record_toolchain "$EVIDENCE_DIR/toolchain.json"

# Execute
run_main_gate "$EVIDENCE_DIR" "./zcc" "zcc.c" "$LEDGER_FILE" "$RUN_ID" "$SCHEMA_FILE"

echo "=== VERDICT: BOOTSTRAP DETERMINISM LOCK SECURED ==="
echo "Ledger written to $LEDGER_FILE"
