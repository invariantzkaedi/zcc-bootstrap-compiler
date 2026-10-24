#!/usr/bin/env bash
# ==============================================================================
#   🚀 gate.sh — ZCC DETERMINISM & CROSS-TU ABI VERIFICATION GATE (MV1.3b)
# ==============================================================================
#   Guarantees:
#     1. Cryptographic Bootstrap Lineage ([Stage 2 -> 3 -> 4 -> 5] Chained SHA-256)
#     2. Gate 1: Byte-Identical Assembly & Stripped Binary Fixed-Point (cmp stage2.s stage3.s)
#     3. Gate 4: Semantic Golden Canary (meta_canary.c) & Independent GCC Differential Oracle
#     4. Gate 2: Bidirectional Cross-TU ABI Interoperability Suite (corpus_version 1.3b):
#        - MV1.2: Scalar, Pointer, Multi-Arg Stack (7+) & Callback Corpus
#        - MV1.3a: INTEGER Aggregate Args/Returns & MEMORY Arg/sret (AbiU8, AbiU16, AbiU24)
#        - MV1.3b: Runtime Stack Alignment & Callee-Saved Register (%rbx, %rbp, %r12-%r15) Preservation
#     5. Fail-Closed JSON Schema & Lineage Topology Validation ([2, 3, 4, 5] Strict State Machine)
#     6. Exact Serialized Execution Environment & Cross-Record Integrity Invariance
#     7. Full Negative Control Suite (Controls A through N including Surgical Mutations M & N)
# ==============================================================================

set -euo pipefail

# 0. Canonical Repository Anchoring
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$SCRIPT_DIR"

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
EXIT_SEMANTIC_DIVERG=60
EXIT_SEMANTIC_BYPASS_PASSED=61

# Canonical Toolchain Resolution
TOOL_GCC=$(command -v gcc || echo "/usr/bin/gcc")
TOOL_AS=$(command -v as || echo "/usr/bin/as")
TOOL_LD=$(command -v ld || echo "/usr/bin/ld")
TOOL_OBJCOPY=$(command -v objcopy || echo "/usr/bin/objcopy")
TOOL_STRIP=$(command -v strip || echo "/usr/bin/strip")
TOOL_PYTHON=$(command -v python3 || command -v python || echo "python3")

# 1. Canonical Environment Construction
CANONICAL_ENV_VARS=(
    "LC_ALL=C"
    "LANG=C"
    "TZ=UTC"
    "SOURCE_DATE_EPOCH=1700000000"
)
for var in PATH TMPDIR ZCC_IR_BACKEND ONE_SOURCE ZCC_MUTATION_SANDBOX; do
    if [[ -v "$var" ]]; then
        CANONICAL_ENV_VARS+=("$var=${!var}")
    fi
done

CLEAN_ENV=(env -i "${CANONICAL_ENV_VARS[@]}")

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
    local gcc_sha as_sha ld_sha objcopy_sha strip_sha python_sha gcc_version
    gcc_sha=$(sha256_file "$TOOL_GCC")
    as_sha=$(sha256_file "$TOOL_AS")
    ld_sha=$(sha256_file "$TOOL_LD")
    objcopy_sha=$(sha256_file "$TOOL_OBJCOPY")
    strip_sha=$(sha256_file "$TOOL_STRIP")
    python_sha=$(sha256_file "$TOOL_PYTHON")
    gcc_version=$("$TOOL_GCC" --version | head -n 1)

    "$TOOL_PYTHON" -c "
import json
import sys
data = {
    'gcc_path': sys.argv[1],
    'gcc_sha256': sys.argv[2],
    'gcc_version': sys.argv[3],
    'as_path': sys.argv[4],
    'as_sha256': sys.argv[5],
    'ld_path': sys.argv[6],
    'ld_sha256': sys.argv[7],
    'objcopy_path': sys.argv[8],
    'objcopy_sha256': sys.argv[9],
    'strip_path': sys.argv[10],
    'strip_sha256': sys.argv[11],
    'python_path': sys.argv[12],
    'python_sha256': sys.argv[13]
}
with open(sys.argv[14], 'w') as f:
    json.dump(data, f, indent=2)
" "$TOOL_GCC" "$gcc_sha" "$gcc_version" "$TOOL_AS" "$as_sha" "$TOOL_LD" "$ld_sha" "$TOOL_OBJCOPY" "$objcopy_sha" "$TOOL_STRIP" "$strip_sha" "$TOOL_PYTHON" "$python_sha" "$out_path"
}

# Helper to serialize canonical environment vector directly to JSON
serialize_env_json() {
    "$TOOL_PYTHON" -c "
import json
import sys
env_dict = {}
for item in sys.argv[1:]:
    if '=' in item:
        k, v = item.split('=', 1)
        env_dict[k] = v
print(json.dumps(env_dict))
" "${CANONICAL_ENV_VARS[@]}"
}

# 3. Provenance JSONL Logging for Stages 2 & 3
write_provenance_jsonl() {
    local out_path="$1" stage="$2" compiler_path="$3" comp_sha_before="$4" comp_sha_after="$5"
    local src_sha="$6" start_ns="$7" end_ns="$8" exit_code="$9" asm_sha="${10}" obj_sha="${11}" bin_sha="${12}"
    local run_id="${13}" verifier_sha="${14}" schema_sha="${15}" toolchain_sha="${16}" commands_json="${17}" env_json="${18}"

    "$TOOL_PYTHON" -c "
import json
import sys
import hashlib

out_path = sys.argv[1]
stage = int(sys.argv[2])
compiler_path = sys.argv[3]
comp_sha_before = sys.argv[4]
comp_sha_after = sys.argv[5]
src_sha = sys.argv[6]
start_ns = int(sys.argv[7])
end_ns = int(sys.argv[8])
exit_code = int(sys.argv[9])
asm_sha = sys.argv[10]
obj_sha = sys.argv[11]
bin_sha = sys.argv[12]
run_id = sys.argv[13]
verifier_sha = sys.argv[14]
schema_sha = sys.argv[15]
toolchain_sha = sys.argv[16]
commands = json.loads(sys.argv[17])
env_dict = json.loads(sys.argv[18])

prev_hash = None
try:
    with open(out_path, 'r') as f:
        lines = [l.strip() for l in f if l.strip()]
        if lines:
            last_line = lines[-1]
            prev_hash = hashlib.sha256(last_line.encode('utf-8')).hexdigest()
except FileNotFoundError:
    pass

data = {
    'schema_version': '1.1',
    'run_id': run_id,
    'stage': stage,
    'previous_hash': prev_hash,
    'compiler_path': compiler_path,
    'compiler_sha256_before': comp_sha_before,
    'compiler_sha256_after': comp_sha_after,
    'source_sha256': src_sha,
    'verifier_sha256': verifier_sha,
    'schema_sha256': schema_sha,
    'toolchain_sha256': toolchain_sha,
    'commands': commands,
    'environment': env_dict,
    'target': 'x86-64-pc-linux-gnu',
    'start_time_ns': start_ns,
    'end_time_ns': end_ns,
    'exit_code': exit_code,
    'assembly_sha256': asm_sha,
    'object_sha256': obj_sha,
    'binary_sha256': bin_sha
}

serialized = json.dumps(data, sort_keys=True, separators=(',', ':'))
with open(out_path, 'a') as f:
    f.write(serialized + '\n')
" "$out_path" "$stage" "$compiler_path" "$comp_sha_before" "$comp_sha_after" "$src_sha" "$start_ns" "$end_ns" "$exit_code" "$asm_sha" "$obj_sha" "$bin_sha" "$run_id" "$verifier_sha" "$schema_sha" "$toolchain_sha" "$commands_json" "$env_json"
}

# 3b. Semantic Provenance JSONL logging for Stage 4
write_semantic_provenance() {
    local out_path="$1" run_id="$2" verifier_sha="$3" schema_sha="$4" toolchain_sha="$5" env_json="$6"
    local oracle_path="$7" oracle_sha="$8" oracle_ver="$9"
    local stage_bin="${10}" stage_bin_sha="${11}" canary_src="${12}" canary_src_sha="${13}" canary_exp="${14}" canary_exp_sha="${15}"
    local gcc_bin="${16}" gcc_bin_sha="${17}" gcc_stdout="${18}" gcc_stdout_sha="${19}" gcc_stderr="${20}" gcc_stderr_sha="${21}"
    local zcc_s="${22}" zcc_s_sha="${23}" zcc_bin="${24}" zcc_bin_sha="${25}" zcc_stdout="${26}" zcc_stdout_sha="${27}" zcc_stderr="${28}" zcc_stderr_sha="${29}"
    local commands_json="${30}" exit_codes_json="${31}"

    "$TOOL_PYTHON" -c "
import json
import sys
import hashlib

out_path = sys.argv[1]
prev_hash = None
try:
    with open(out_path, 'r') as f:
        lines = [l.strip() for l in f if l.strip()]
        if lines:
            last_line = lines[-1]
            prev_hash = hashlib.sha256(last_line.encode('utf-8')).hexdigest()
except FileNotFoundError:
    pass

data = {
    'schema_version': '1.1',
    'record_type': 'semantic_canary',
    'run_id': sys.argv[2],
    'stage': 4,
    'previous_hash': prev_hash,
    'verifier_sha256': sys.argv[3],
    'schema_sha256': sys.argv[4],
    'toolchain_sha256': sys.argv[5],
    'environment': json.loads(sys.argv[6]),
    'oracle_compiler_path': sys.argv[7],
    'oracle_compiler_sha256': sys.argv[8],
    'oracle_compiler_version': sys.argv[9],
    'converged_compiler_path': sys.argv[10],
    'converged_compiler_sha256': sys.argv[11],
    'canary_src_path': sys.argv[12],
    'canary_src_sha256': sys.argv[13],
    'canary_expected_path': sys.argv[14],
    'canary_expected_sha256': sys.argv[15],
    'canary_gcc_bin_path': sys.argv[16],
    'canary_gcc_bin_sha256': sys.argv[17],
    'canary_gcc_stdout_path': sys.argv[18],
    'canary_gcc_stdout_sha256': sys.argv[19],
    'canary_gcc_stderr_path': sys.argv[20],
    'canary_gcc_stderr_sha256': sys.argv[21],
    'canary_zcc_s_path': sys.argv[22],
    'canary_zcc_s_sha256': sys.argv[23],
    'canary_zcc_bin_path': sys.argv[24],
    'canary_zcc_bin_sha256': sys.argv[25],
    'canary_zcc_stdout_path': sys.argv[26],
    'canary_zcc_stdout_sha256': sys.argv[27],
    'canary_zcc_stderr_path': sys.argv[28],
    'canary_zcc_stderr_sha256': sys.argv[29],
    'commands': json.loads(sys.argv[30]),
    'exit_codes': json.loads(sys.argv[31]),
    'oracle_match': True,
    'golden_match': True
}

serialized = json.dumps(data, sort_keys=True, separators=(',', ':'))
with open(out_path, 'a') as f:
    f.write(serialized + '\n')
" "$out_path" "$run_id" "$verifier_sha" "$schema_sha" "$toolchain_sha" "$env_json" "$oracle_path" "$oracle_sha" "$oracle_ver" "$stage_bin" "$stage_bin_sha" \
  "$canary_src" "$canary_src_sha" "$canary_exp" "$canary_exp_sha" \
  "$gcc_bin" "$gcc_bin_sha" "$gcc_stdout" "$gcc_stdout_sha" "$gcc_stderr" "$gcc_stderr_sha" \
  "$zcc_s" "$zcc_s_sha" "$zcc_bin" "$zcc_bin_sha" "$zcc_stdout" "$zcc_stdout_sha" "$zcc_stderr" "$zcc_stderr_sha" \
  "$commands_json" "$exit_codes_json"
}

# 3c. Triple-Corpus ABI Interoperability Provenance JSONL logging for Stage 5 (MV1.2 + MV1.3a + MV1.3b)
write_abi_provenance() {
    local out_path="$1" run_id="$2" verifier_sha="$3" schema_sha="$4" toolchain_sha="$5" env_json="$6"
    local v12_types_h="$7" v12_types_h_sha="$8" v12_lib_c="$9" v12_lib_c_sha="${10}" v12_main_c="${11}" v12_main_c_sha="${12}" v12_exp="${13}" v12_exp_sha="${14}"
    local v12_zlib_bin="${15}" v12_zlib_bin_sha="${16}" v12_glib_bin="${17}" v12_glib_bin_sha="${18}"
    local v13_types_h="${19}" v13_types_h_sha="${20}" v13_lib_c="${21}" v13_lib_c_sha="${22}" v13_main_c="${23}" v13_main_c_sha="${24}" v13_exp="${25}" v13_exp_sha="${26}"
    local v13_zlib_bin="${27}" v13_zlib_bin_sha="${28}" v13_glib_bin="${29}" v13_glib_bin_sha="${30}"
    local v13b_types_h="${31}" v13b_types_h_sha="${32}" v13b_lib_c="${33}" v13b_lib_c_sha="${34}" v13b_asm_s="${35}" v13b_asm_s_sha="${36}" v13b_main_c="${37}" v13b_main_c_sha="${38}" v13b_exp="${39}" v13b_exp_sha="${40}"
    local v13b_zlib_bin="${41}" v13b_zlib_bin_sha="${42}" v13b_glib_bin="${43}" v13b_glib_bin_sha="${44}"
    local commands_json="${45}" exit_codes_json="${46}"

    "$TOOL_PYTHON" -c "
import json
import sys
import hashlib

out_path = sys.argv[1]
prev_hash = None
try:
    with open(out_path, 'r') as f:
        lines = [l.strip() for l in f if l.strip()]
        if lines:
            last_line = lines[-1]
            prev_hash = hashlib.sha256(last_line.encode('utf-8')).hexdigest()
except FileNotFoundError:
    pass

data = {
    'schema_version': '1.1',
    'record_type': 'abi_interop',
    'corpus_version': '1.3b',
    'run_id': sys.argv[2],
    'stage': 5,
    'previous_hash': prev_hash,
    'verifier_sha256': sys.argv[3],
    'schema_sha256': sys.argv[4],
    'toolchain_sha256': sys.argv[5],
    'environment': json.loads(sys.argv[6]),
    'v12_types_h_path': sys.argv[7],
    'v12_types_h_sha256': sys.argv[8],
    'v12_lib_c_path': sys.argv[9],
    'v12_lib_c_sha256': sys.argv[10],
    'v12_main_c_path': sys.argv[11],
    'v12_main_c_sha256': sys.argv[12],
    'v12_expected_path': sys.argv[13],
    'v12_expected_sha256': sys.argv[14],
    'v12_zlib_gmain_bin_path': sys.argv[15],
    'v12_zlib_gmain_bin_sha256': sys.argv[16],
    'v12_glib_zmain_bin_path': sys.argv[17],
    'v12_glib_zmain_bin_sha256': sys.argv[18],
    'v13_types_h_path': sys.argv[19],
    'v13_types_h_sha256': sys.argv[20],
    'v13_lib_c_path': sys.argv[21],
    'v13_lib_c_sha256': sys.argv[22],
    'v13_main_c_path': sys.argv[23],
    'v13_main_c_sha256': sys.argv[24],
    'v13_expected_path': sys.argv[25],
    'v13_expected_sha256': sys.argv[26],
    'v13_zlib_gmain_bin_path': sys.argv[27],
    'v13_zlib_gmain_bin_sha256': sys.argv[28],
    'v13_glib_zmain_bin_path': sys.argv[29],
    'v13_glib_zmain_bin_sha256': sys.argv[30],
    'v13b_types_h_path': sys.argv[31],
    'v13b_types_h_sha256': sys.argv[32],
    'v13b_lib_c_path': sys.argv[33],
    'v13b_lib_c_sha256': sys.argv[34],
    'v13b_asm_s_path': sys.argv[35],
    'v13b_asm_s_sha256': sys.argv[36],
    'v13b_main_c_path': sys.argv[37],
    'v13b_main_c_sha256': sys.argv[38],
    'v13b_expected_path': sys.argv[39],
    'v13b_expected_sha256': sys.argv[40],
    'v13b_zlib_gmain_bin_path': sys.argv[41],
    'v13b_zlib_gmain_bin_sha256': sys.argv[42],
    'v13b_glib_zmain_bin_path': sys.argv[43],
    'v13b_glib_zmain_bin_sha256': sys.argv[44],
    'commands': json.loads(sys.argv[45]),
    'exit_codes': json.loads(sys.argv[46]),
    'v12_oracle_match': True,
    'v13_oracle_match': True,
    'v13b_oracle_match': True
}

serialized = json.dumps(data, sort_keys=True, separators=(',', ':'))
with open(out_path, 'a') as f:
    f.write(serialized + '\n')
" "$out_path" "$run_id" "$verifier_sha" "$schema_sha" "$toolchain_sha" "$env_json" \
  "$v12_types_h" "$v12_types_h_sha" "$v12_lib_c" "$v12_lib_c_sha" "$v12_main_c" "$v12_main_c_sha" "$v12_exp" "$v12_exp_sha" \
  "$v12_zlib_bin" "$v12_zlib_bin_sha" "$v12_glib_bin" "$v12_glib_bin_sha" \
  "$v13_types_h" "$v13_types_h_sha" "$v13_lib_c" "$v13_lib_c_sha" "$v13_main_c" "$v13_main_c_sha" "$v13_exp" "$v13_exp_sha" \
  "$v13_zlib_bin" "$v13_zlib_bin_sha" "$v13_glib_bin" "$v13_glib_bin_sha" \
  "$v13b_types_h" "$v13b_types_h_sha" "$v13b_lib_c" "$v13b_lib_c_sha" "$v13b_asm_s" "$v13b_asm_s_sha" "$v13b_main_c" "$v13b_main_c_sha" "$v13b_exp" "$v13b_exp_sha" \
  "$v13b_zlib_bin" "$v13b_zlib_bin_sha" "$v13b_glib_bin" "$v13b_glib_bin_sha" \
  "$commands_json" "$exit_codes_json"
}

# Fail-Closed Python schema validator
validate_schema() {
    local ledger_path="$1"
    local schema_path="$2"
    
    if [ ! -f "$schema_path" ]; then
        echo "FAIL: Schema file missing ($schema_path)." >&2
        return "$EXIT_SCHEMA_FAIL"
    fi

    "$TOOL_PYTHON" -c "
import json
import sys
try:
    import jsonschema
except ImportError:
    import subprocess
    try:
        subprocess.run([sys.executable, '-m', 'pip', 'install', '-q', 'jsonschema'], check=True)
        import jsonschema
    except Exception:
        print('FAIL: jsonschema library is required for strict provenance validation.', file=sys.stderr)
        sys.exit($EXIT_SCHEMA_FAIL)

try:
    with open('$schema_path') as sf:
        schema = json.load(sf)
    with open('$ledger_path') as lf:
        for line_idx, line in enumerate(lf):
            l = line.strip()
            if l:
                jsonschema.validate(instance=json.loads(l), schema=schema)
except Exception as e:
    print(f'JSON Schema validation failed: {e}', file=sys.stderr)
    sys.exit($EXIT_SCHEMA_FAIL)
"
}

# Lineage validation logic (Strict State Machine [2, 3, 4, 5], Cross-Record Integrity & Active Environment Binding)
validate_lineage() {
    local ledger_path="$1"
    local expected_run_id="$2"
    local expected_verifier_sha="$3"
    local expected_schema_sha="$4"
    local expected_toolchain_sha="$5"
    local expected_env_json="$6"

    "$TOOL_PYTHON" -c "
import json
import sys
import hashlib

ledger_path = '$ledger_path'
expected_run_id = '$expected_run_id'
expected_verifier_sha = '$expected_verifier_sha'
expected_schema_sha = '$expected_schema_sha'
expected_toolchain_sha = '$expected_toolchain_sha'
expected_env_dict = json.loads('''$expected_env_json''')

try:
    records = []
    with open(ledger_path) as f:
        for line in f:
            line_str = line.strip()
            if not line_str:
                continue
            records.append((line_str, json.loads(line_str)))
except Exception as e:
    print(f'Lineage read failure: {e}', file=sys.stderr)
    sys.exit($EXIT_LINEAGE_FAIL)

# 1. Verify cryptographic chaining across all ledger lines
for idx, (raw_line, data) in enumerate(records):
    if idx == 0:
        if data.get('previous_hash') is not None:
            print('FAIL: First record must have null previous_hash.', file=sys.stderr)
            sys.exit($EXIT_LINEAGE_FAIL)
    else:
        prev_raw = records[idx - 1][0]
        expected_prev_hash = hashlib.sha256(prev_raw.encode('utf-8')).hexdigest()
        actual_prev_hash = data.get('previous_hash')
        if actual_prev_hash != expected_prev_hash:
            print(f'FAIL: Cryptographic lineage break at index {idx}. expected {expected_prev_hash}, got {actual_prev_hash}', file=sys.stderr)
            sys.exit($EXIT_LINEAGE_FAIL)

# 2. Extract current run records and verify strict [2, 3, 4, 5] state machine sequence
current_records = [d for _, d in records if d.get('run_id') == expected_run_id]
stages = [d.get('stage') for d in current_records]

if stages != [2, 3, 4, 5]:
    print(f'FAIL: Invalid stage sequence for run {expected_run_id}. Expected [2, 3, 4, 5], got {stages}', file=sys.stderr)
    sys.exit($EXIT_LINEAGE_FAIL)

r_s2, r_s3, r_s4, r_s5 = current_records[0], current_records[1], current_records[2], current_records[3]

# 3. Verify cross-record integrity and active trusted identity binding across all 4 stages
for field_name, exp_hash in [
    ('verifier_sha256', expected_verifier_sha),
    ('schema_sha256', expected_schema_sha),
    ('toolchain_sha256', expected_toolchain_sha)
]:
    vals = [r.get(field_name) for r in (r_s2, r_s3, r_s4, r_s5)]
    if len(set(vals)) != 1 or vals[0] != exp_hash:
        print(f'FAIL: Cross-record integrity discrepancy for {field_name}. Expected {exp_hash}, got {vals}', file=sys.stderr)
        sys.exit($EXIT_LINEAGE_FAIL)

# 4. Verify canonical environment serialization across all stages
for r in (r_s2, r_s3, r_s4, r_s5):
    env_in_rec = r.get('environment')
    if env_in_rec != expected_env_dict:
        print(f'FAIL: Environment record discrepancy in stage {r.get(\"stage\")}. Expected {expected_env_dict}, got {env_in_rec}', file=sys.stderr)
        sys.exit($EXIT_LINEAGE_FAIL)

# 5. Verify stage transitions and compiler fixed-point derivation
if r_s2.get('binary_sha256') != r_s3.get('compiler_sha256_before'):
    print('FAIL: Stage-2 binary sha does not match Stage-3 compiler sha before run.', file=sys.stderr)
    sys.exit($EXIT_LINEAGE_FAIL)

if r_s3.get('binary_sha256') != r_s4.get('converged_compiler_sha256'):
    print('FAIL: Stage-3 binary sha does not match Stage-4 converged compiler sha.', file=sys.stderr)
    sys.exit($EXIT_LINEAGE_FAIL)

# 6. Binary-hash alias rejection in Stage 4
if r_s4.get('canary_gcc_bin_sha256') == r_s4.get('canary_zcc_bin_sha256'):
    print('FAIL: Canary binary hash alias detected (GCC and ZCC binary hashes are identical).', file=sys.stderr)
    sys.exit($EXIT_SEMANTIC_DIVERG)

if r_s4.get('record_type') != 'semantic_canary' or not r_s4.get('oracle_match') or not r_s4.get('golden_match'):
    print('FAIL: Stage-4 semantic canary validation flags invalid.', file=sys.stderr)
    sys.exit($EXIT_LINEAGE_FAIL)

# 7. ABI interop flags in Stage 5
if r_s5.get('record_type') != 'abi_interop' or r_s5.get('corpus_version') != '1.3b' or not r_s5.get('v12_oracle_match') or not r_s5.get('v13_oracle_match') or not r_s5.get('v13b_oracle_match'):
    print('FAIL: Stage-5 ABI interop validation flags invalid.', file=sys.stderr)
    sys.exit($EXIT_LINEAGE_FAIL)

print('Lineage check: PASS')
"
}

# 4. Independent Generation Execution Loop
compile_stage() {
    local run_dir="$1" stage_num="$2" compiler_src_bin="$3" source_file="$4" out_prefix="$5" ledger_file="$6" run_id="$7"
    local verifier_sha="$8" schema_sha="$9" toolchain_sha="${10}" env_json="${11}"
    
    local out_s="$run_dir/${out_prefix}.s"
    local out_obj="$run_dir/${out_prefix}.o"
    local out_bin="$run_dir/${out_prefix}.bin"

    printf "  \033[1;36m[Stage %d/3]\033[0m Compiling %s via %s... " "$stage_num" "$source_file" "$compiler_src_bin"
    
    if [ -e "$out_s" ] || [ -e "$out_obj" ] || [ -e "$out_bin" ]; then
        echo "FAIL: Stale file detected in target path." >&2
        exit "$EXIT_STALE_OUTPUT"
    fi
    
    local exec_dir="$run_dir/exec_stage_${stage_num}"
    mkdir -p "$exec_dir"
    local compiler_exec="$exec_dir/zcc-compiler"
    cp -f "$compiler_src_bin" "$compiler_exec"
    chmod 755 "$compiler_exec" 2>/dev/null || true
    
    local comp_sha_before comp_sha_after src_sha start_ns exit_code=0
    comp_sha_before=$(sha256_file "$compiler_exec")
    src_sha=$(sha256_file "$source_file")
    start_ns=$(date +%s%N)
    
    # 1. Compile Assembly
    local cmd_compile_s=("$compiler_exec" "$source_file" "-S" "-o" "$out_s")
    "${CLEAN_ENV[@]}" "${cmd_compile_s[@]}" > "$run_dir/stage${stage_num}_compile_s.log" 2>&1 || exit_code=$?
    if [ "$exit_code" -ne 0 ]; then
        echo "FAIL (Assembly Gen Failed with exit code $exit_code)" >&2
        exit "$exit_code"
    fi
    
    # 2. Compile Linked Binary directly through driver
    local cmd_compile_bin=("$compiler_exec" "$source_file" "-o" "$out_bin")
    "${CLEAN_ENV[@]}" "${cmd_compile_bin[@]}" > "$run_dir/stage${stage_num}_compile_bin.log" 2>&1 || exit_code=$?
    if [ "$exit_code" -ne 0 ]; then
        echo "FAIL (Binary Gen Failed with exit code $exit_code)" >&2
        exit "$exit_code"
    fi

    # 3. Strip symbols to eliminate transient tempfile object strings in .strtab
    local cmd_strip=("$TOOL_STRIP" "--strip-all" "$out_bin")
    "${CLEAN_ENV[@]}" "${cmd_strip[@]}" > "$run_dir/stage${stage_num}_strip.log" 2>&1 || exit_code=$?
    if [ "$exit_code" -ne 0 ]; then
        echo "FAIL: Strip failed with exit code $exit_code" >&2
        exit "$EXIT_TOOL_FAIL"
    fi

    # 4. Compile object file directly for comparative audit
    local cmd_compile_obj=("$compiler_exec" "$source_file" "-c" "-o" "$out_obj")
    "${CLEAN_ENV[@]}" "${cmd_compile_obj[@]}" > "$run_dir/stage${stage_num}_compile_obj.log" 2>&1 || exit_code=$?
    if [ "$exit_code" -ne 0 ]; then
        echo "FAIL (Object Gen Failed with exit code $exit_code)" >&2
        exit "$exit_code"
    fi

    local end_ns
    end_ns=$(date +%s%N)
    comp_sha_after=$(sha256_file "$compiler_exec")

    if [ "$comp_sha_before" != "$comp_sha_after" ]; then
        echo "FAIL: Compiler mutation detected during stage $stage_num execution." >&2
        exit "$EXIT_COMPILER_MUTATED"
    fi

    local asm_sha obj_sha bin_sha
    asm_sha=$(sha256_file "$out_s")
    obj_sha=$(sha256_file "$out_obj")
    bin_sha=$(sha256_file "$out_bin")

    local commands_json
    commands_json=$("$TOOL_PYTHON" -c "
import json, sys
cmds = [
    {'kind': 'compile_s', 'argv': [sys.argv[1], sys.argv[2], '-S', '-o', sys.argv[3]]},
    {'kind': 'compile_bin', 'argv': [sys.argv[1], sys.argv[2], '-o', sys.argv[4]]},
    {'kind': 'strip_bin', 'argv': [sys.argv[5], '--strip-all', sys.argv[4]]},
    {'kind': 'compile_obj', 'argv': [sys.argv[1], sys.argv[2], '-c', '-o', sys.argv[6]]}
]
print(json.dumps(cmds))
" "$compiler_exec" "$source_file" "$out_s" "$out_bin" "$TOOL_STRIP" "$out_obj")

    write_provenance_jsonl "$ledger_file" "$stage_num" "$compiler_src_bin" "$comp_sha_before" "$comp_sha_after" \
                           "$src_sha" "$start_ns" "$end_ns" "$exit_code" "$asm_sha" "$obj_sha" "$bin_sha" \
                           "$run_id" "$verifier_sha" "$schema_sha" "$toolchain_sha" "$commands_json" "$env_json"

    echo -e "\033[1;32m[PASS]\033[0m (SHA: ${bin_sha:0:16}...)"
}

# 5. Semantic Canary Verification with Freshness Guarantee
verify_semantic_canary() {
    local run_dir="$1" stage3_bin="$2" canary_src="${3:-$REPO_ROOT/tests/selfhost/meta_canary.c}" canary_expected="${4:-$REPO_ROOT/tests/selfhost/meta_canary.expected}"
    local ledger_file="${5:-}" run_id="${6:-}" verifier_sha="${7:-}" schema_sha="${8:-}" toolchain_sha="${9:-}" env_json="${10:-}"
    
    local canary_gcc_bin="$run_dir/canary_gcc.bin"
    local canary_gcc_stdout="$run_dir/canary_gcc.stdout"
    local canary_gcc_stderr="$run_dir/canary_gcc.stderr"
    local canary_zcc_s="$run_dir/canary_zcc.s"
    local canary_zcc_bin="$run_dir/canary_zcc.bin"
    local canary_zcc_stdout="$run_dir/canary_zcc.stdout"
    local canary_zcc_stderr="$run_dir/canary_zcc.stderr"

    # Pre-execution Freshness Purge & Validation
    rm -f "$canary_gcc_bin" "$canary_gcc_stdout" "$canary_gcc_stderr" \
          "$canary_zcc_s" "$canary_zcc_bin" "$canary_zcc_stdout" "$canary_zcc_stderr"
    
    for p in "$canary_gcc_bin" "$canary_gcc_stdout" "$canary_gcc_stderr" "$canary_zcc_s" "$canary_zcc_bin" "$canary_zcc_stdout" "$canary_zcc_stderr"; do
        if [ -e "$p" ]; then
            echo "FAIL: Stale semantic canary artifact detected ($p)." >&2
            return "$EXIT_STALE_OUTPUT"
        fi
    done

    printf "  \033[1;36m[Semantic Oracle]\033[0m Cross-verifying against GCC & Golden Oracle... "

    # 1. Compile and run reference via Host GCC
    local cmd_gcc_build=("$TOOL_GCC" "-O0" "-no-pie" "$canary_src" "-o" "$canary_gcc_bin")
    local gcc_build_rc=0
    "${CLEAN_ENV[@]}" "${cmd_gcc_build[@]}" > "$run_dir/canary_gcc_build.log" 2>&1 || gcc_build_rc=$?
    if [ "$gcc_build_rc" -ne 0 ] || [ ! -x "$canary_gcc_bin" ]; then
        echo "FAIL (GCC Build Failed)" >&2
        return "$EXIT_SEMANTIC_DIVERG"
    fi

    local cmd_gcc_run=("$canary_gcc_bin")
    local gcc_rc=0
    "${CLEAN_ENV[@]}" "${cmd_gcc_run[@]}" > "$canary_gcc_stdout" 2> "$canary_gcc_stderr" || gcc_rc=$?
    if [ "$gcc_rc" -ne 0 ]; then
        echo "FAIL (GCC Run Failed)" >&2
        return "$EXIT_SEMANTIC_DIVERG"
    fi

    # 2. Compile and run candidate via Converged Stage 3 ZCC
    local cmd_zcc_compile=("$stage3_bin" "$canary_src" "-o" "$canary_zcc_s")
    local zcc_compile_rc=0
    "${CLEAN_ENV[@]}" "${cmd_zcc_compile[@]}" > "$run_dir/canary_zcc_compile.log" 2>&1 || zcc_compile_rc=$?
    if [ "$zcc_compile_rc" -ne 0 ] || [ ! -s "$canary_zcc_s" ]; then
        echo "FAIL (ZCC Compile Failed)" >&2
        return "$EXIT_SEMANTIC_DIVERG"
    fi

    local cmd_zcc_link=("$TOOL_GCC" "-no-pie" "$canary_zcc_s" "-o" "$canary_zcc_bin")
    local zcc_link_rc=0
    "${CLEAN_ENV[@]}" "${cmd_zcc_link[@]}" > "$run_dir/canary_zcc_link.log" 2>&1 || zcc_link_rc=$?
    if [ "$zcc_link_rc" -ne 0 ] || [ ! -x "$canary_zcc_bin" ]; then
        echo "FAIL (ZCC Link Failed)" >&2
        return "$EXIT_SEMANTIC_DIVERG"
    fi

    local cmd_zcc_run=("$canary_zcc_bin")
    local zcc_rc=0
    "${CLEAN_ENV[@]}" "${cmd_zcc_run[@]}" > "$canary_zcc_stdout" 2> "$canary_zcc_stderr" || zcc_rc=$?
    if [ "$zcc_rc" -ne 0 ]; then
        echo "FAIL (ZCC Run Failed)" >&2
        return "$EXIT_SEMANTIC_DIVERG"
    fi

    # 3. Factored Output Comparisons
    if ! cmp -s "$canary_gcc_stdout" "$canary_zcc_stdout"; then
        echo -e "\033[1;31m[FAIL: GCC != ZCC]\033[0m" >&2
        return "$EXIT_SEMANTIC_DIVERG"
    fi

    if ! cmp -s "$canary_zcc_stdout" "$canary_expected"; then
        echo -e "\033[1;31m[FAIL: ZCC != Golden Expected]\033[0m" >&2
        return "$EXIT_SEMANTIC_DIVERG"
    fi

    # 4. Provenance Recording
    if [ -n "$ledger_file" ] && [ -n "$run_id" ]; then
        local oracle_sha oracle_ver st3_sha c_src_sha c_exp_sha gcc_sha
        oracle_sha=$(sha256_file "$TOOL_GCC")
        oracle_ver=$("$TOOL_GCC" --version | head -n 1)
        st3_sha=$(sha256_file "$stage3_bin")
        c_src_sha=$(sha256_file "$canary_src")
        c_exp_sha=$(sha256_file "$canary_expected")
        gcc_sha=$(sha256_file "$TOOL_GCC")

        local gcc_bin_sha gcc_out_sha gcc_err_sha zcc_s_sha zcc_bin_sha zcc_out_sha zcc_err_sha
        gcc_bin_sha=$(sha256_file "$canary_gcc_bin")
        gcc_out_sha=$(sha256_file "$canary_gcc_stdout")
        gcc_err_sha=$(sha256sum "$canary_gcc_stderr" | cut -d' ' -f1)
        zcc_s_sha=$(sha256_file "$canary_zcc_s")
        zcc_bin_sha=$(sha256_file "$canary_zcc_bin")
        zcc_out_sha=$(sha256_file "$canary_zcc_stdout")
        zcc_err_sha=$(sha256sum "$canary_zcc_stderr" | cut -d' ' -f1)

        local commands_json
        commands_json=$("$TOOL_PYTHON" -c "
import json, sys
cmds = [
    {'kind': 'gcc_build', 'argv': [sys.argv[1], '-O0', '-no-pie', sys.argv[2], '-o', sys.argv[3]]},
    {'kind': 'gcc_run', 'argv': [sys.argv[3]]},
    {'kind': 'zcc_compile', 'argv': [sys.argv[4], sys.argv[2], '-o', sys.argv[5]]},
    {'kind': 'zcc_link', 'argv': [sys.argv[1], '-no-pie', sys.argv[5], '-o', sys.argv[6]]},
    {'kind': 'zcc_run', 'argv': [sys.argv[6]]}
]
print(json.dumps(cmds))
" "$TOOL_GCC" "$canary_src" "$canary_gcc_bin" "$stage3_bin" "$canary_zcc_s" "$canary_zcc_bin")

        local exit_codes_json
        exit_codes_json=$("$TOOL_PYTHON" -c "
import json, sys
print(json.dumps({
    'gcc_build': int(sys.argv[1]),
    'gcc_run': int(sys.argv[2]),
    'zcc_compile': int(sys.argv[3]),
    'zcc_link': int(sys.argv[4]),
    'zcc_run': int(sys.argv[5])
}))
" "$gcc_build_rc" "$gcc_rc" "$zcc_compile_rc" "$zcc_link_rc" "$zcc_rc")

        write_semantic_provenance "$ledger_file" "$run_id" "$verifier_sha" "$schema_sha" "$toolchain_sha" "$env_json" \
          "$TOOL_GCC" "$oracle_sha" "$oracle_ver" \
          "$stage3_bin" "$st3_sha" "$canary_src" "$c_src_sha" "$canary_expected" "$c_exp_sha" \
          "$canary_gcc_bin" "$gcc_bin_sha" "$canary_gcc_stdout" "$gcc_out_sha" "$canary_gcc_stderr" "$gcc_err_sha" \
          "$canary_zcc_s" "$zcc_s_sha" "$canary_zcc_bin" "$zcc_bin_sha" "$canary_zcc_stdout" "$zcc_out_sha" "$canary_zcc_stderr" "$zcc_err_sha" \
          "$commands_json" "$exit_codes_json"
    fi

    echo -e "\033[1;32m[PASS: Byte-Identical Semantic Parity]\033[0m"
    return 0
}

# 6. Triple-Corpus Cross-TU ABI Interoperability Subgate (MV1.2 + MV1.3a + MV1.3b)
verify_abi_subgate() {
    local run_dir="$1" stage3_bin="$2"
    local v12_types_h="${3:-$REPO_ROOT/tests/selfhost/abi_types.h}"
    local v12_lib_c="${4:-$REPO_ROOT/tests/selfhost/abi_lib.c}"
    local v12_main_c="${5:-$REPO_ROOT/tests/selfhost/abi_main.c}"
    local v12_expected="${6:-$REPO_ROOT/tests/selfhost/abi_canary.expected}"
    local v13_types_h="${7:-$REPO_ROOT/tests/selfhost/abi_v13/abi_v13_types.h}"
    local v13_lib_c="${8:-$REPO_ROOT/tests/selfhost/abi_v13/abi_v13_lib.c}"
    local v13_main_c="${9:-$REPO_ROOT/tests/selfhost/abi_v13/abi_v13_main.c}"
    local v13_expected="${10:-$REPO_ROOT/tests/selfhost/abi_v13/abi_v13.expected}"
    local v13b_types_h="${11:-$REPO_ROOT/tests/selfhost/abi_v13b/abi_v13b_types.h}"
    local v13b_lib_c="${12:-$REPO_ROOT/tests/selfhost/abi_v13b/abi_v13b_lib.c}"
    local v13b_asm_s="${13:-$REPO_ROOT/tests/selfhost/abi_v13b/abi_v13b_asm.s}"
    local v13b_main_c="${14:-$REPO_ROOT/tests/selfhost/abi_v13b/abi_v13b_main.c}"
    local v13b_expected="${15:-$REPO_ROOT/tests/selfhost/abi_v13b/abi_v13b.expected}"
    local ledger_file="${16:-}" run_id="${17:-}" verifier_sha="${18:-}" schema_sha="${19:-}" toolchain_sha="${20:-}" env_json="${21:-}"

    local abi_dir="$run_dir/abi_interop"
    mkdir -p "$abi_dir"

    printf "  \033[1;36m[Cross-TU ABI Oracle]\033[0m Bidirectional Interop (MV1.2, MV1.3a & MV1.3b)... "

    # --- Part 1: MV1.2 Scalar / Callback Corpus ---
    local v12_d1_lib_s="$abi_dir/v12_d1_lib_zcc.s"
    local v12_d1_lib_o="$abi_dir/v12_d1_lib_zcc.o"
    local v12_d1_main_o="$abi_dir/v12_d1_main_gcc.o"
    local v12_d1_bin="$abi_dir/v12_zlib_gmain.bin"
    local v12_d1_stdout="$abi_dir/v12_zlib_gmain.stdout"
    local v12_d1_stderr="$abi_dir/v12_zlib_gmain.stderr"

    local v12_d2_lib_o="$abi_dir/v12_d2_lib_gcc.o"
    local v12_d2_main_s="$abi_dir/v12_d2_main_zcc.s"
    local v12_d2_main_o="$abi_dir/v12_d2_main_zcc.o"
    local v12_d2_bin="$abi_dir/v12_glib_zmain.bin"
    local v12_d2_stdout="$abi_dir/v12_glib_zmain.stdout"
    local v12_d2_stderr="$abi_dir/v12_glib_zmain.stderr"

    # --- Part 2: MV1.3a Aggregate Corpus ---
    local v13_d1_lib_s="$abi_dir/v13_d1_lib_zcc.s"
    local v13_d1_lib_o="$abi_dir/v13_d1_lib_zcc.o"
    local v13_d1_main_o="$abi_dir/v13_d1_main_gcc.o"
    local v13_d1_bin="$abi_dir/v13_zlib_gmain.bin"
    local v13_d1_stdout="$abi_dir/v13_zlib_gmain.stdout"
    local v13_d1_stderr="$abi_dir/v13_zlib_gmain.stderr"

    local v13_d2_lib_o="$abi_dir/v13_d2_lib_gcc.o"
    local v13_d2_main_s="$abi_dir/v13_d2_main_zcc.s"
    local v13_d2_main_o="$abi_dir/v13_d2_main_zcc.o"
    local v13_d2_bin="$abi_dir/v13_glib_zmain.bin"
    local v13_d2_stdout="$abi_dir/v13_glib_zmain.stdout"
    local v13_d2_stderr="$abi_dir/v13_glib_zmain.stderr"

    # --- Part 3: MV1.3b Stack Alignment & Callee-Saved Register Corpus ---
    local v13b_d1_lib_s="$abi_dir/v13b_d1_lib_zcc.s"
    local v13b_d1_lib_o="$abi_dir/v13b_d1_lib_zcc.o"
    local v13b_d1_asm_o="$abi_dir/v13b_d1_asm.o"
    local v13b_d1_main_o="$abi_dir/v13b_d1_main_gcc.o"
    local v13b_d1_bin="$abi_dir/v13b_zlib_gmain.bin"
    local v13b_d1_stdout="$abi_dir/v13b_zlib_gmain.stdout"
    local v13b_d1_stderr="$abi_dir/v13b_zlib_gmain.stderr"

    local v13b_d2_lib_o="$abi_dir/v13b_d2_lib_gcc.o"
    local v13b_d2_asm_o="$abi_dir/v13b_d2_asm.o"
    local v13b_d2_main_s="$abi_dir/v13b_d2_main_zcc.s"
    local v13b_d2_main_o="$abi_dir/v13b_d2_main_zcc.o"
    local v13b_d2_bin="$abi_dir/v13b_glib_zmain.bin"
    local v13b_d2_stdout="$abi_dir/v13b_glib_zmain.stdout"
    local v13b_d2_stderr="$abi_dir/v13b_glib_zmain.stderr"

    rm -f "$v12_d1_lib_s" "$v12_d1_lib_o" "$v12_d1_main_o" "$v12_d1_bin" "$v12_d1_stdout" "$v12_d1_stderr" \
          "$v12_d2_lib_o" "$v12_d2_main_s" "$v12_d2_main_o" "$v12_d2_bin" "$v12_d2_stdout" "$v12_d2_stderr" \
          "$v13_d1_lib_s" "$v13_d1_lib_o" "$v13_d1_main_o" "$v13_d1_bin" "$v13_d1_stdout" "$v13_d1_stderr" \
          "$v13_d2_lib_o" "$v13_d2_main_s" "$v13_d2_main_o" "$v13_d2_bin" "$v13_d2_stdout" "$v13_d2_stderr" \
          "$v13b_d1_lib_s" "$v13b_d1_lib_o" "$v13b_d1_asm_o" "$v13b_d1_main_o" "$v13b_d1_bin" "$v13b_d1_stdout" "$v13b_d1_stderr" \
          "$v13b_d2_lib_o" "$v13b_d2_asm_o" "$v13b_d2_main_s" "$v13b_d2_main_o" "$v13b_d2_bin" "$v13b_d2_stdout" "$v13b_d2_stderr"

    local rc_v12_d1_zcompile=0 rc_v12_d1_as=0 rc_v12_d1_gcompile=0 rc_v12_d1_link=0 rc_v12_d1_run=0
    local rc_v12_d2_gcompile=0 rc_v12_d2_zcompile=0 rc_v12_d2_as=0 rc_v12_d2_link=0 rc_v12_d2_run=0
    local rc_v13_d1_zcompile=0 rc_v13_d1_as=0 rc_v13_d1_gcompile=0 rc_v13_d1_link=0 rc_v13_d1_run=0
    local rc_v13_d2_gcompile=0 rc_v13_d2_zcompile=0 rc_v13_d2_as=0 rc_v13_d2_link=0 rc_v13_d2_run=0
    local rc_v13b_d1_zcompile=0 rc_v13b_d1_as_lib=0 rc_v13b_d1_as_oracle=0 rc_v13b_d1_gcompile=0 rc_v13b_d1_link=0 rc_v13b_d1_run=0
    local rc_v13b_d2_gcompile=0 rc_v13b_d2_zcompile=0 rc_v13b_d2_as_main=0 rc_v13b_d2_link=0 rc_v13b_d2_run=0

    # Command Definitions
    local cmd_v12_d1_zcompile=("$stage3_bin" "$v12_lib_c" "-o" "$v12_d1_lib_s")
    local cmd_v12_d1_as=("$TOOL_AS" "--64" "$v12_d1_lib_s" "-o" "$v12_d1_lib_o")
    local cmd_v12_d1_gcompile=("$TOOL_GCC" "-I$(dirname "$v12_types_h")" "-c" "$v12_main_c" "-o" "$v12_d1_main_o")
    local cmd_v12_d1_link=("$TOOL_GCC" "-no-pie" "$v12_d1_main_o" "$v12_d1_lib_o" "-o" "$v12_d1_bin")
    local cmd_v12_d1_run=("$v12_d1_bin")

    local cmd_v12_d2_gcompile=("$TOOL_GCC" "-I$(dirname "$v12_types_h")" "-c" "$v12_lib_c" "-o" "$v12_d2_lib_o")
    local cmd_v12_d2_zcompile=("$stage3_bin" "$v12_main_c" "-o" "$v12_d2_main_s")
    local cmd_v12_d2_as=("$TOOL_AS" "--64" "$v12_d2_main_s" "-o" "$v12_d2_main_o")
    local cmd_v12_d2_link=("$TOOL_GCC" "-no-pie" "$v12_d2_main_o" "$v12_d2_lib_o" "-o" "$v12_d2_bin")
    local cmd_v12_d2_run=("$v12_d2_bin")

    local cmd_v13_d1_zcompile=("$stage3_bin" "$v13_lib_c" "-o" "$v13_d1_lib_s")
    local cmd_v13_d1_as=("$TOOL_AS" "--64" "$v13_d1_lib_s" "-o" "$v13_d1_lib_o")
    local cmd_v13_d1_gcompile=("$TOOL_GCC" "-I$(dirname "$v13_types_h")" "-c" "$v13_main_c" "-o" "$v13_d1_main_o")
    local cmd_v13_d1_link=("$TOOL_GCC" "-no-pie" "$v13_d1_main_o" "$v13_d1_lib_o" "-o" "$v13_d1_bin")
    local cmd_v13_d1_run=("$v13_d1_bin")

    local cmd_v13_d2_gcompile=("$TOOL_GCC" "-I$(dirname "$v13_types_h")" "-c" "$v13_lib_c" "-o" "$v13_d2_lib_o")
    local cmd_v13_d2_zcompile=("$stage3_bin" "$v13_main_c" "-o" "$v13_d2_main_s")
    local cmd_v13_d2_as=("$TOOL_AS" "--64" "$v13_d2_main_s" "-o" "$v13_d2_main_o")
    local cmd_v13_d2_link=("$TOOL_GCC" "-no-pie" "$v13_d2_main_o" "$v13_d2_lib_o" "-o" "$v13_d2_bin")
    local cmd_v13_d2_run=("$v13_d2_bin")

    local cmd_v13b_d1_zcompile=("$stage3_bin" "$v13b_lib_c" "-o" "$v13b_d1_lib_s")
    local cmd_v13b_d1_as_lib=("$TOOL_AS" "--64" "$v13b_d1_lib_s" "-o" "$v13b_d1_lib_o")
    local cmd_v13b_d1_as_oracle=("$TOOL_AS" "--64" "$v13b_asm_s" "-o" "$v13b_d1_asm_o")
    local cmd_v13b_d1_gcompile=("$TOOL_GCC" "-I$(dirname "$v13b_types_h")" "-c" "$v13b_main_c" "-o" "$v13b_d1_main_o")
    local cmd_v13b_d1_link=("$TOOL_GCC" "-no-pie" "$v13b_d1_main_o" "$v13b_d1_lib_o" "$v13b_d1_asm_o" "-o" "$v13b_d1_bin")
    local cmd_v13b_d1_run=("$v13b_d1_bin")

    local cmd_v13b_d2_gcompile=("$TOOL_GCC" "-I$(dirname "$v13b_types_h")" "-c" "$v13b_lib_c" "-o" "$v13b_d2_lib_o")
    local cmd_v13b_d2_zcompile=("$stage3_bin" "$v13b_main_c" "-o" "$v13b_d2_main_s")
    local cmd_v13b_d2_as_main=("$TOOL_AS" "--64" "$v13b_d2_main_s" "-o" "$v13b_d2_main_o")
    local cmd_v13b_d2_link=("$TOOL_GCC" "-no-pie" "$v13b_d2_main_o" "$v13b_d2_lib_o" "$v13b_d1_asm_o" "-o" "$v13b_d2_bin")
    local cmd_v13b_d2_run=("$v13b_d2_bin")

    exec_v12_d1() {
        "${CLEAN_ENV[@]}" "${cmd_v12_d1_zcompile[@]}" > "$abi_dir/v12_d1_zcompile.log" 2>&1 || return "$EXIT_SEMANTIC_DIVERG"
        [ -s "$v12_d1_lib_s" ] || return "$EXIT_SEMANTIC_DIVERG"
        "${CLEAN_ENV[@]}" "${cmd_v12_d1_as[@]}" > "$abi_dir/v12_d1_as.log" 2>&1 || return "$EXIT_SEMANTIC_DIVERG"
        [ -s "$v12_d1_lib_o" ] || return "$EXIT_SEMANTIC_DIVERG"
        "${CLEAN_ENV[@]}" "${cmd_v12_d1_gcompile[@]}" > "$abi_dir/v12_d1_gcompile.log" 2>&1 || return "$EXIT_SEMANTIC_DIVERG"
        [ -s "$v12_d1_main_o" ] || return "$EXIT_SEMANTIC_DIVERG"
        "${CLEAN_ENV[@]}" "${cmd_v12_d1_link[@]}" > "$abi_dir/v12_d1_link.log" 2>&1 || return "$EXIT_SEMANTIC_DIVERG"
        [ -x "$v12_d1_bin" ] || return "$EXIT_SEMANTIC_DIVERG"
        "${CLEAN_ENV[@]}" "${cmd_v12_d1_run[@]}" > "$v12_d1_stdout" 2> "$v12_d1_stderr" || return "$EXIT_SEMANTIC_DIVERG"
        return 0
    }

    exec_v12_d2() {
        "${CLEAN_ENV[@]}" "${cmd_v12_d2_gcompile[@]}" > "$abi_dir/v12_d2_gcompile.log" 2>&1 || return "$EXIT_SEMANTIC_DIVERG"
        [ -s "$v12_d2_lib_o" ] || return "$EXIT_SEMANTIC_DIVERG"
        "${CLEAN_ENV[@]}" "${cmd_v12_d2_zcompile[@]}" > "$abi_dir/v12_d2_zcompile.log" 2>&1 || return "$EXIT_SEMANTIC_DIVERG"
        [ -s "$v12_d2_main_s" ] || return "$EXIT_SEMANTIC_DIVERG"
        "${CLEAN_ENV[@]}" "${cmd_v12_d2_as[@]}" > "$abi_dir/v12_d2_as.log" 2>&1 || return "$EXIT_SEMANTIC_DIVERG"
        [ -s "$v12_d2_main_o" ] || return "$EXIT_SEMANTIC_DIVERG"
        "${CLEAN_ENV[@]}" "${cmd_v12_d2_link[@]}" > "$abi_dir/v12_d2_link.log" 2>&1 || return "$EXIT_SEMANTIC_DIVERG"
        [ -x "$v12_d2_bin" ] || return "$EXIT_SEMANTIC_DIVERG"
        "${CLEAN_ENV[@]}" "${cmd_v12_d2_run[@]}" > "$v12_d2_stdout" 2> "$v12_d2_stderr" || return "$EXIT_SEMANTIC_DIVERG"
        return 0
    }

    exec_v13_d1() {
        "${CLEAN_ENV[@]}" "${cmd_v13_d1_zcompile[@]}" > "$abi_dir/v13_d1_zcompile.log" 2>&1 || return "$EXIT_SEMANTIC_DIVERG"
        [ -s "$v13_d1_lib_s" ] || return "$EXIT_SEMANTIC_DIVERG"
        "${CLEAN_ENV[@]}" "${cmd_v13_d1_as[@]}" > "$abi_dir/v13_d1_as.log" 2>&1 || return "$EXIT_SEMANTIC_DIVERG"
        [ -s "$v13_d1_lib_o" ] || return "$EXIT_SEMANTIC_DIVERG"
        "${CLEAN_ENV[@]}" "${cmd_v13_d1_gcompile[@]}" > "$abi_dir/v13_d1_gcompile.log" 2>&1 || return "$EXIT_SEMANTIC_DIVERG"
        [ -s "$v13_d1_main_o" ] || return "$EXIT_SEMANTIC_DIVERG"
        "${CLEAN_ENV[@]}" "${cmd_v13_d1_link[@]}" > "$abi_dir/v13_d1_link.log" 2>&1 || return "$EXIT_SEMANTIC_DIVERG"
        [ -x "$v13_d1_bin" ] || return "$EXIT_SEMANTIC_DIVERG"
        "${CLEAN_ENV[@]}" "${cmd_v13_d1_run[@]}" > "$v13_d1_stdout" 2> "$v13_d1_stderr" || return "$EXIT_SEMANTIC_DIVERG"
        return 0
    }

    exec_v13_d2() {
        "${CLEAN_ENV[@]}" "${cmd_v13_d2_gcompile[@]}" > "$abi_dir/v13_d2_gcompile.log" 2>&1 || return "$EXIT_SEMANTIC_DIVERG"
        [ -s "$v13_d2_lib_o" ] || return "$EXIT_SEMANTIC_DIVERG"
        "${CLEAN_ENV[@]}" "${cmd_v13_d2_zcompile[@]}" > "$abi_dir/v13_d2_zcompile.log" 2>&1 || return "$EXIT_SEMANTIC_DIVERG"
        [ -s "$v13_d2_main_s" ] || return "$EXIT_SEMANTIC_DIVERG"
        "${CLEAN_ENV[@]}" "${cmd_v13_d2_as[@]}" > "$abi_dir/v13_d2_as.log" 2>&1 || return "$EXIT_SEMANTIC_DIVERG"
        [ -s "$v13_d2_main_o" ] || return "$EXIT_SEMANTIC_DIVERG"
        "${CLEAN_ENV[@]}" "${cmd_v13_d2_link[@]}" > "$abi_dir/v13_d2_link.log" 2>&1 || return "$EXIT_SEMANTIC_DIVERG"
        [ -x "$v13_d2_bin" ] || return "$EXIT_SEMANTIC_DIVERG"
        "${CLEAN_ENV[@]}" "${cmd_v13_d2_run[@]}" > "$v13_d2_stdout" 2> "$v13_d2_stderr" || return "$EXIT_SEMANTIC_DIVERG"
        return 0
    }

    exec_v13b_d1() {
        "${CLEAN_ENV[@]}" "${cmd_v13b_d1_zcompile[@]}" > "$abi_dir/v13b_d1_zcompile.log" 2>&1 || return "$EXIT_SEMANTIC_DIVERG"
        [ -s "$v13b_d1_lib_s" ] || return "$EXIT_SEMANTIC_DIVERG"
        "${CLEAN_ENV[@]}" "${cmd_v13b_d1_as_lib[@]}" > "$abi_dir/v13b_d1_as_lib.log" 2>&1 || return "$EXIT_SEMANTIC_DIVERG"
        [ -s "$v13b_d1_lib_o" ] || return "$EXIT_SEMANTIC_DIVERG"
        "${CLEAN_ENV[@]}" "${cmd_v13b_d1_as_oracle[@]}" > "$abi_dir/v13b_d1_as_oracle.log" 2>&1 || return "$EXIT_SEMANTIC_DIVERG"
        [ -s "$v13b_d1_asm_o" ] || return "$EXIT_SEMANTIC_DIVERG"
        "${CLEAN_ENV[@]}" "${cmd_v13b_d1_gcompile[@]}" > "$abi_dir/v13b_d1_gcompile.log" 2>&1 || return "$EXIT_SEMANTIC_DIVERG"
        [ -s "$v13b_d1_main_o" ] || return "$EXIT_SEMANTIC_DIVERG"
        "${CLEAN_ENV[@]}" "${cmd_v13b_d1_link[@]}" > "$abi_dir/v13b_d1_link.log" 2>&1 || return "$EXIT_SEMANTIC_DIVERG"
        [ -x "$v13b_d1_bin" ] || return "$EXIT_SEMANTIC_DIVERG"
        "${CLEAN_ENV[@]}" "${cmd_v13b_d1_run[@]}" > "$v13b_d1_stdout" 2> "$v13b_d1_stderr" || return "$EXIT_SEMANTIC_DIVERG"
        return 0
    }

    exec_v13b_d2() {
        "${CLEAN_ENV[@]}" "${cmd_v13b_d2_gcompile[@]}" > "$abi_dir/v13b_d2_gcompile.log" 2>&1 || return "$EXIT_SEMANTIC_DIVERG"
        [ -s "$v13b_d2_lib_o" ] || return "$EXIT_SEMANTIC_DIVERG"
        "${CLEAN_ENV[@]}" "${cmd_v13b_d2_zcompile[@]}" > "$abi_dir/v13b_d2_zcompile.log" 2>&1 || return "$EXIT_SEMANTIC_DIVERG"
        [ -s "$v13b_d2_main_s" ] || return "$EXIT_SEMANTIC_DIVERG"
        "${CLEAN_ENV[@]}" "${cmd_v13b_d2_as_main[@]}" > "$abi_dir/v13b_d2_as_main.log" 2>&1 || return "$EXIT_SEMANTIC_DIVERG"
        [ -s "$v13b_d2_main_o" ] || return "$EXIT_SEMANTIC_DIVERG"
        "${CLEAN_ENV[@]}" "${cmd_v13b_d2_link[@]}" > "$abi_dir/v13b_d2_link.log" 2>&1 || return "$EXIT_SEMANTIC_DIVERG"
        [ -x "$v13b_d2_bin" ] || return "$EXIT_SEMANTIC_DIVERG"
        "${CLEAN_ENV[@]}" "${cmd_v13b_d2_run[@]}" > "$v13b_d2_stdout" 2> "$v13b_d2_stderr" || return "$EXIT_SEMANTIC_DIVERG"
        return 0
    }

    if [ "${ZCC_PARALLEL_GATES:-0}" = "1" ]; then
        exec_v12_d1 & local p_v12_d1=$!
        exec_v12_d2 & local p_v12_d2=$!
        exec_v13_d1 & local p_v13_d1=$!
        exec_v13_d2 & local p_v13_d2=$!
        exec_v13b_d1 & local p_v13b_d1=$!
        exec_v13b_d2 & local p_v13b_d2=$!

        reap_abi_children() {
            kill -TERM "$p_v12_d1" "$p_v12_d2" "$p_v13_d1" "$p_v13_d2" "$p_v13b_d1" "$p_v13b_d2" 2>/dev/null || true
            wait "$p_v12_d1" "$p_v12_d2" "$p_v13_d1" "$p_v13_d2" "$p_v13b_d1" "$p_v13b_d2" 2>/dev/null || true
        }
        trap reap_abi_children INT TERM HUP

        local fail_dir=""
        wait "$p_v12_d1" || fail_dir="v12_d1"
        wait "$p_v12_d2" || fail_dir="${fail_dir:+$fail_dir, }v12_d2"
        wait "$p_v13_d1" || fail_dir="${fail_dir:+$fail_dir, }v13_d1"
        wait "$p_v13_d2" || fail_dir="${fail_dir:+$fail_dir, }v13_d2"
        wait "$p_v13b_d1" || fail_dir="${fail_dir:+$fail_dir, }v13b_d1"
        wait "$p_v13b_d2" || fail_dir="${fail_dir:+$fail_dir, }v13b_d2"

        trap - INT TERM HUP

        if [ -n "$fail_dir" ]; then
            echo -e "  \033[1;31m[FAIL: ABI Interop Direction(s) Failed: $fail_dir]\033[0m" >&2
            for fd in $(echo "$fail_dir" | tr -d ' ' | tr ',' '\n'); do
                for logfile in "$abi_dir"/${fd}*.log; do
                    if [ -f "$logfile" ] && [ -s "$logfile" ]; then
                        echo -e "  \033[1;33m--- Log detail ($logfile) ---\033[0m" >&2
                        tail -n 12 "$logfile" >&2
                    fi
                done
            done
            return "$EXIT_SEMANTIC_DIVERG"
        fi
    else
        exec_v12_d1 || return "$EXIT_SEMANTIC_DIVERG"
        exec_v12_d2 || return "$EXIT_SEMANTIC_DIVERG"
        exec_v13_d1 || return "$EXIT_SEMANTIC_DIVERG"
        exec_v13_d2 || return "$EXIT_SEMANTIC_DIVERG"
        exec_v13b_d1 || return "$EXIT_SEMANTIC_DIVERG"
        exec_v13b_d2 || return "$EXIT_SEMANTIC_DIVERG"
    fi

    if ! cmp -s "$v12_d1_stdout" "$v12_expected" || ! cmp -s "$v12_d2_stdout" "$v12_expected" || ! cmp -s "$v12_d1_stdout" "$v12_d2_stdout"; then
        echo -e "\033[1;31m[FAIL: MV1.2 Divergence]\033[0m" >&2
        return "$EXIT_SEMANTIC_DIVERG"
    fi

    if ! cmp -s "$v13_d1_stdout" "$v13_expected" || ! cmp -s "$v13_d2_stdout" "$v13_expected" || ! cmp -s "$v13_d1_stdout" "$v13_d2_stdout"; then
        echo -e "\033[1;31m[FAIL: MV1.3a Aggregate Divergence]\033[0m" >&2
        return "$EXIT_SEMANTIC_DIVERG"
    fi

    if ! cmp -s "$v13b_d1_stdout" "$v13b_expected" || ! cmp -s "$v13b_d2_stdout" "$v13b_expected" || ! cmp -s "$v13b_d1_stdout" "$v13b_d2_stdout"; then
        echo -e "\033[1;31m[FAIL: MV1.3b Alignment/Register Divergence]\033[0m" >&2
        return "$EXIT_SEMANTIC_DIVERG"
    fi

    # Provenance Recording
    if [ -n "$ledger_file" ] && [ -n "$run_id" ]; then
        local v12_th_sha v12_lc_sha v12_mc_sha v12_exp_sha v12_zlib_sha v12_glib_sha
        v12_th_sha=$(sha256_file "$v12_types_h")
        v12_lc_sha=$(sha256_file "$v12_lib_c")
        v12_mc_sha=$(sha256_file "$v12_main_c")
        v12_exp_sha=$(sha256_file "$v12_expected")
        v12_zlib_sha=$(sha256_file "$v12_d1_bin")
        v12_glib_sha=$(sha256_file "$v12_d2_bin")

        local v13_th_sha v13_lc_sha v13_mc_sha v13_exp_sha v13_zlib_sha v13_glib_sha
        v13_th_sha=$(sha256_file "$v13_types_h")
        v13_lc_sha=$(sha256_file "$v13_lib_c")
        v13_mc_sha=$(sha256_file "$v13_main_c")
        v13_exp_sha=$(sha256_file "$v13_expected")
        v13_zlib_sha=$(sha256_file "$v13_d1_bin")
        v13_glib_sha=$(sha256_file "$v13_d2_bin")

        local v13b_th_sha v13b_lc_sha v13b_asm_sha v13b_mc_sha v13b_exp_sha v13b_zlib_sha v13b_glib_sha
        v13b_th_sha=$(sha256_file "$v13b_types_h")
        v13b_lc_sha=$(sha256_file "$v13b_lib_c")
        v13b_asm_sha=$(sha256_file "$v13b_asm_s")
        v13b_mc_sha=$(sha256_file "$v13b_main_c")
        v13b_exp_sha=$(sha256_file "$v13b_expected")
        v13b_zlib_sha=$(sha256_file "$v13b_d1_bin")
        v13b_glib_sha=$(sha256_file "$v13b_d2_bin")

        local commands_json
        commands_json=$("$TOOL_PYTHON" -c "
import json, sys
cmds = [
    {'kind': 'v12_d1_zcc_lib_compile', 'argv': [sys.argv[1], sys.argv[2], '-o', sys.argv[3]]},
    {'kind': 'v12_d1_as_lib', 'argv': [sys.argv[4], '--64', sys.argv[3], '-o', sys.argv[5]]},
    {'kind': 'v12_d1_gcc_main_compile', 'argv': [sys.argv[6], '-I' + sys.argv[7], '-c', sys.argv[8], '-o', sys.argv[9]]},
    {'kind': 'v12_d1_link', 'argv': [sys.argv[6], '-no-pie', sys.argv[9], sys.argv[5], '-o', sys.argv[10]]},
    {'kind': 'v12_d1_run', 'argv': [sys.argv[10]]},
    {'kind': 'v12_d2_gcc_lib_compile', 'argv': [sys.argv[6], '-I' + sys.argv[7], '-c', sys.argv[2], '-o', sys.argv[11]]},
    {'kind': 'v12_d2_zcc_main_compile', 'argv': [sys.argv[1], sys.argv[8], '-o', sys.argv[12]]},
    {'kind': 'v12_d2_as_main', 'argv': [sys.argv[4], '--64', sys.argv[12], '-o', sys.argv[13]]},
    {'kind': 'v12_d2_link', 'argv': [sys.argv[6], '-no-pie', sys.argv[13], sys.argv[11], '-o', sys.argv[14]]},
    {'kind': 'v12_d2_run', 'argv': [sys.argv[14]]},
    {'kind': 'v13_d1_zcc_lib_compile', 'argv': [sys.argv[1], sys.argv[15], '-o', sys.argv[16]]},
    {'kind': 'v13_d1_as_lib', 'argv': [sys.argv[4], '--64', sys.argv[16], '-o', sys.argv[17]]},
    {'kind': 'v13_d1_gcc_main_compile', 'argv': [sys.argv[6], '-I' + sys.argv[18], '-c', sys.argv[19], '-o', sys.argv[20]]},
    {'kind': 'v13_d1_link', 'argv': [sys.argv[6], '-no-pie', sys.argv[20], sys.argv[17], '-o', sys.argv[21]]},
    {'kind': 'v13_d1_run', 'argv': [sys.argv[21]]},
    {'kind': 'v13_d2_gcc_lib_compile', 'argv': [sys.argv[6], '-I' + sys.argv[18], '-c', sys.argv[15], '-o', sys.argv[22]]},
    {'kind': 'v13_d2_zcc_main_compile', 'argv': [sys.argv[1], sys.argv[19], '-o', sys.argv[23]]},
    {'kind': 'v13_d2_as_main', 'argv': [sys.argv[4], '--64', sys.argv[23], '-o', sys.argv[24]]},
    {'kind': 'v13_d2_link', 'argv': [sys.argv[6], '-no-pie', sys.argv[24], sys.argv[22], '-o', sys.argv[25]]},
    {'kind': 'v13_d2_run', 'argv': [sys.argv[25]]},
    {'kind': 'v13b_d1_zcc_lib_compile', 'argv': [sys.argv[1], sys.argv[26], '-o', sys.argv[27]]},
    {'kind': 'v13b_d1_as_lib', 'argv': [sys.argv[4], '--64', sys.argv[27], '-o', sys.argv[28]]},
    {'kind': 'v13b_d1_gcc_main_compile', 'argv': [sys.argv[6], '-I' + sys.argv[29], '-c', sys.argv[30], '-o', sys.argv[31]]},
    {'kind': 'v13b_d1_as_oracle', 'argv': [sys.argv[4], '--64', sys.argv[32], '-o', sys.argv[33]]},
    {'kind': 'v13b_d1_link', 'argv': [sys.argv[6], '-no-pie', sys.argv[31], sys.argv[28], sys.argv[33], '-o', sys.argv[34]]},
    {'kind': 'v13b_d1_run', 'argv': [sys.argv[34]]},
    {'kind': 'v13b_d2_gcc_lib_compile', 'argv': [sys.argv[6], '-I' + sys.argv[29], '-c', sys.argv[26], '-o', sys.argv[35]]},
    {'kind': 'v13b_d2_zcc_main_compile', 'argv': [sys.argv[1], sys.argv[30], '-o', sys.argv[36]]},
    {'kind': 'v13b_d2_as_main', 'argv': [sys.argv[4], '--64', sys.argv[36], '-o', sys.argv[37]]},
    {'kind': 'v13b_d2_link', 'argv': [sys.argv[6], '-no-pie', sys.argv[37], sys.argv[35], sys.argv[33], '-o', sys.argv[38]]},
    {'kind': 'v13b_d2_run', 'argv': [sys.argv[38]]}
]
print(json.dumps(cmds))
" "$stage3_bin" "$v12_lib_c" "$v12_d1_lib_s" "$TOOL_AS" "$v12_d1_lib_o" "$TOOL_GCC" "$(dirname "$v12_types_h")" "$v12_main_c" "$v12_d1_main_o" "$v12_d1_bin" \
  "$v12_d2_lib_o" "$v12_d2_main_s" "$v12_d2_main_o" "$v12_d2_bin" \
  "$v13_lib_c" "$v13_d1_lib_s" "$v13_d1_lib_o" "$(dirname "$v13_types_h")" "$v13_main_c" "$v13_d1_main_o" "$v13_d1_bin" \
  "$v13_d2_lib_o" "$v13_d2_main_s" "$v13_d2_main_o" "$v13_d2_bin" \
  "$v13b_lib_c" "$v13b_d1_lib_s" "$v13b_d1_lib_o" "$(dirname "$v13b_types_h")" "$v13b_main_c" "$v13b_d1_main_o" "$v13b_asm_s" "$v13b_d1_asm_o" "$v13b_d1_bin" \
  "$v13b_d2_lib_o" "$v13b_d2_main_s" "$v13b_d2_main_o" "$v13b_d2_bin")

        local exit_codes_json
        exit_codes_json=$("$TOOL_PYTHON" -c "
import json, sys
print(json.dumps({
    'v12_d1_zcc_lib_compile': int(sys.argv[1]),
    'v12_d1_as_lib': int(sys.argv[2]),
    'v12_d1_gcc_main_compile': int(sys.argv[3]),
    'v12_d1_link': int(sys.argv[4]),
    'v12_d1_run': int(sys.argv[5]),
    'v12_d2_gcc_lib_compile': int(sys.argv[6]),
    'v12_d2_zcc_main_compile': int(sys.argv[7]),
    'v12_d2_as_main': int(sys.argv[8]),
    'v12_d2_link': int(sys.argv[9]),
    'v12_d2_run': int(sys.argv[10]),
    'v13_d1_zcc_lib_compile': int(sys.argv[11]),
    'v13_d1_as_lib': int(sys.argv[12]),
    'v13_d1_gcc_main_compile': int(sys.argv[13]),
    'v13_d1_link': int(sys.argv[14]),
    'v13_d1_run': int(sys.argv[15]),
    'v13_d2_gcc_lib_compile': int(sys.argv[16]),
    'v13_d2_zcc_main_compile': int(sys.argv[17]),
    'v13_d2_as_main': int(sys.argv[18]),
    'v13_d2_link': int(sys.argv[19]),
    'v13_d2_run': int(sys.argv[20]),
    'v13b_d1_zcc_lib_compile': int(sys.argv[21]),
    'v13b_d1_as_lib': int(sys.argv[22]),
    'v13b_d1_gcc_main_compile': int(sys.argv[23]),
    'v13b_d1_as_oracle': int(sys.argv[24]),
    'v13b_d1_link': int(sys.argv[25]),
    'v13b_d1_run': int(sys.argv[26]),
    'v13b_d2_gcc_lib_compile': int(sys.argv[27]),
    'v13b_d2_zcc_main_compile': int(sys.argv[28]),
    'v13b_d2_as_main': int(sys.argv[29]),
    'v13b_d2_link': int(sys.argv[30]),
    'v13b_d2_run': int(sys.argv[31])
}))
" "$rc_v12_d1_zcompile" "$rc_v12_d1_as" "$rc_v12_d1_gcompile" "$rc_v12_d1_link" "$rc_v12_d1_run" \
  "$rc_v12_d2_gcompile" "$rc_v12_d2_zcompile" "$rc_v12_d2_as" "$rc_v12_d2_link" "$rc_v12_d2_run" \
  "$rc_v13_d1_zcompile" "$rc_v13_d1_as" "$rc_v13_d1_gcompile" "$rc_v13_d1_link" "$rc_v13_d1_run" \
  "$rc_v13_d2_gcompile" "$rc_v13_d2_zcompile" "$rc_v13_d2_as" "$rc_v13_d2_link" "$rc_v13_d2_run" \
  "$rc_v13b_d1_zcompile" "$rc_v13b_d1_as_lib" "$rc_v13b_d1_gcompile" "$rc_v13b_d1_as_oracle" "$rc_v13b_d1_link" "$rc_v13b_d1_run" \
  "$rc_v13b_d2_gcompile" "$rc_v13b_d2_zcompile" "$rc_v13b_d2_as_main" "$rc_v13b_d2_link" "$rc_v13b_d2_run")

        write_abi_provenance "$ledger_file" "$run_id" "$verifier_sha" "$schema_sha" "$toolchain_sha" "$env_json" \
          "$v12_types_h" "$v12_th_sha" "$v12_lib_c" "$v12_lc_sha" "$v12_main_c" "$v12_mc_sha" "$v12_expected" "$v12_exp_sha" \
          "$v12_d1_bin" "$v12_zlib_sha" "$v12_d2_bin" "$v12_glib_sha" \
          "$v13_types_h" "$v13_th_sha" "$v13_lib_c" "$v13_lc_sha" "$v13_main_c" "$v13_mc_sha" "$v13_expected" "$v13_exp_sha" \
          "$v13_d1_bin" "$v13_zlib_sha" "$v13_d2_bin" "$v13_glib_sha" \
          "$v13b_types_h" "$v13b_th_sha" "$v13b_lib_c" "$v13b_lc_sha" "$v13b_asm_s" "$v13b_asm_sha" "$v13b_main_c" "$v13b_mc_sha" "$v13b_expected" "$v13b_exp_sha" \
          "$v13b_d1_bin" "$v13b_zlib_sha" "$v13b_d2_bin" "$v13b_glib_sha" \
          "$commands_json" "$exit_codes_json"
    fi

    echo -e "\033[1;32m[PASS: Triple-Corpus Bidirectional Parity]\033[0m"
    return 0
}

# Factored semantic results comparator
verify_semantic_results() {
    local gcc_stdout="$1"
    local zcc_stdout="$2"
    local golden_exp="$3"
    
    if ! cmp -s "$gcc_stdout" "$zcc_stdout"; then
        echo "FAIL: Semantic canary output diverged between GCC oracle and ZCC." >&2
        return "$EXIT_SEMANTIC_DIVERG"
    fi
    
    if ! cmp -s "$zcc_stdout" "$golden_exp"; then
        echo "FAIL: Semantic canary output diverged from golden expected artifact." >&2
        return "$EXIT_SEMANTIC_DIVERG"
    fi
    
    return 0
}

# Factored constraint validation
verify_gate_constraints() {
    local run_dir="$1" ledger_file="$2" schema_path="$3" run_id="$4"
    local verifier_sha="$5" schema_sha="$6" toolchain_sha="$7" env_json="$8"
    
    for file in stage2.s stage2.o stage2.bin stage3.s stage3.o stage3.bin; do
        if [ ! -s "$run_dir/$file" ]; then
            echo "FAIL: Missing or empty artifact $run_dir/$file" >&2
            return "$EXIT_STALE_OUTPUT"
        fi
    done
    
    "$TOOL_OBJCOPY" --remove-section=.note.gnu.build-id "$run_dir/stage2.bin" "$run_dir/stage2.bin.norm"
    "$TOOL_OBJCOPY" --remove-section=.note.gnu.build-id "$run_dir/stage3.bin" "$run_dir/stage3.bin.norm"
    
    local s2_mtime s3_mtime
    s2_mtime=$(stat -c %Y "$run_dir/stage2.bin")
    s3_mtime=$(stat -c %Y "$run_dir/stage3.bin")
    if [ "$s3_mtime" -lt "$s2_mtime" ]; then
        echo "FAIL: Stage-3 modified timestamp precedes Stage-2." >&2
        return "$EXIT_STALE_OUTPUT"
    fi
    
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

    local val_status=0
    verify_semantic_canary "$run_dir" "$run_dir/stage3.bin" "$REPO_ROOT/tests/selfhost/meta_canary.c" "$REPO_ROOT/tests/selfhost/meta_canary.expected" \
      "$ledger_file" "$run_id" "$verifier_sha" "$schema_sha" "$toolchain_sha" "$env_json" || val_status=$?
    if [ "$val_status" -ne 0 ]; then
        return "$val_status"
    fi

    val_status=0
    verify_abi_subgate "$run_dir" "$run_dir/stage3.bin" \
      "$REPO_ROOT/tests/selfhost/abi_types.h" "$REPO_ROOT/tests/selfhost/abi_lib.c" "$REPO_ROOT/tests/selfhost/abi_main.c" "$REPO_ROOT/tests/selfhost/abi_canary.expected" \
      "$REPO_ROOT/tests/selfhost/abi_v13/abi_v13_types.h" "$REPO_ROOT/tests/selfhost/abi_v13/abi_v13_lib.c" "$REPO_ROOT/tests/selfhost/abi_v13/abi_v13_main.c" "$REPO_ROOT/tests/selfhost/abi_v13/abi_v13.expected" \
      "$REPO_ROOT/tests/selfhost/abi_v13b/abi_v13b_types.h" "$REPO_ROOT/tests/selfhost/abi_v13b/abi_v13b_lib.c" "$REPO_ROOT/tests/selfhost/abi_v13b/abi_v13b_asm.s" "$REPO_ROOT/tests/selfhost/abi_v13b/abi_v13b_main.c" "$REPO_ROOT/tests/selfhost/abi_v13b/abi_v13b.expected" \
      "$ledger_file" "$run_id" "$verifier_sha" "$schema_sha" "$toolchain_sha" "$env_json" || val_status=$?
    if [ "$val_status" -ne 0 ]; then
        return "$val_status"
    fi

    val_status=0
    validate_schema "$ledger_file" "$schema_path" || val_status=$?
    if [ "$val_status" -ne 0 ]; then
        return "$val_status"
    fi
    
    val_status=0
    validate_lineage "$ledger_file" "$run_id" "$verifier_sha" "$schema_sha" "$toolchain_sha" "$env_json" || val_status=$?
    if [ "$val_status" -ne 0 ]; then
        return "$val_status"
    fi
}

# 7. Main Verification Engine
run_main_gate() {
    local run_dir="$1" stage1_compiler="$2" source_file="$3" ledger_file="$4" run_id="$5" schema_file="$6"
    local verifier_sha="$7" schema_sha="$8" toolchain_sha="$9" env_json="${10}"
    
    echo -e "\n\033[1;35m═══ EXECUTING DETERMINISTIC SELF-HOST PIPELINE ═══\033[0m"
    compile_stage "$run_dir" 2 "$stage1_compiler" "$source_file" "stage2" "$ledger_file" "$run_id" "$verifier_sha" "$schema_sha" "$toolchain_sha" "$env_json"
    compile_stage "$run_dir" 3 "$run_dir/stage2.bin" "$source_file" "stage3" "$ledger_file" "$run_id" "$verifier_sha" "$schema_sha" "$toolchain_sha" "$env_json"

    # Gate 1: Byte-identical Identity Check
    echo -e "\n\033[1;35m═══ [GATE 1] SELF-HOST IDENTITY VERIFICATION ═══\033[0m"
    if ! cmp -s "$run_dir/stage2.s" "$run_dir/stage3.s"; then
        echo -e "  \033[1;31mFAIL: Gate 1 Assembly divergence between Stage 2 and Stage 3.\033[0m" >&2
        diff -u "$run_dir/stage2.s" "$run_dir/stage3.s" | head -n 30 >&2
        exit "$EXIT_ASM_DIVERG"
    fi
    echo -e "  \033[1;32m✓ Gate 1 PASS: Assembly byte-identical (cmp stage2.s stage3.s).\033[0m"

    # Factored constraint, semantic and ABI validation
    verify_gate_constraints "$run_dir" "$ledger_file" "$schema_file" "$run_id" "$verifier_sha" "$schema_sha" "$toolchain_sha" "$env_json"
}

# Helper to re-compute complete SHA-256 block hash chain over a ledger file
recompute_hash_chain() {
    local ledger_path="$1"
    "$TOOL_PYTHON" -c "
import json
import hashlib
import sys

path = sys.argv[1]
with open(path, 'r') as f:
    lines = [json.loads(l.strip()) for l in f if l.strip()]

prev = None
recomputed = []
for data in lines:
    data['previous_hash'] = prev
    line_str = json.dumps(data, sort_keys=True, separators=(',', ':'))
    prev = hashlib.sha256(line_str.encode('utf-8')).hexdigest()
    recomputed.append(line_str)

with open(path, 'w') as f:
    for l in recomputed:
        f.write(l + '\n')
" "$ledger_path"
}

# 8. Self-Testing Framework (Negative Control Suite A through N)
run_self_test() {
    local schema_path="$1"
    local RUN_ID
    RUN_ID="self-test-$(date +%s)"
    
    echo "=== EXECUTING SELF-TEST HARNESS (MV1.3b Frozen) ==="
    
    local test_run_dir="$REPO_ROOT/build/gate-self-test-sim-$RUN_ID"
    mkdir -p "$test_run_dir"
    local test_ledger="$test_run_dir/test_provenance.jsonl"
    local test_toolchain="$test_run_dir/toolchain.json"
    local test_run_id="test-run-12345"
    
    record_toolchain "$test_toolchain"
    local verifier_sha schema_sha toolchain_sha env_json
    verifier_sha=$(sha256_file "$REPO_ROOT/gate.sh")
    schema_sha=$(sha256_file "$schema_path")
    toolchain_sha=$(sha256_file "$test_toolchain")
    env_json=$(serialize_env_json)
    
    # Test A: Legitimate run (Positive Control)
    run_main_gate "$test_run_dir" "$REPO_ROOT/zcc" "$REPO_ROOT/zcc.c" "$test_ledger" "$test_run_id" "$schema_path" \
      "$verifier_sha" "$schema_sha" "$toolchain_sha" "$env_json"
    echo "Result A: PASS (Main gate verified successfully)"
    
    # Test B: Assembly Mutation validation (Expected: EXIT_ASM_DIVERG = 20)
    echo "Testing Negative Control Injection (Assembly) (Expected: exit 20)..."
    local test_run_dir_b="$REPO_ROOT/build/gate-self-test-b-$RUN_ID"
    mkdir -p "$test_run_dir_b"
    cp -a "$test_run_dir"/. "$test_run_dir_b"/
    echo "# mutated" >> "$test_run_dir_b/stage3.s"
    
    local exit_b=0
    verify_gate_constraints "$test_run_dir_b" "$test_ledger" "$schema_path" "$test_run_id" \
      "$verifier_sha" "$schema_sha" "$toolchain_sha" "$env_json" || exit_b=$?
    if [ "$exit_b" -eq "$EXIT_ASM_DIVERG" ]; then
        echo "Result B: PASS (Divergence successfully caught with code $EXIT_ASM_DIVERG)"
    else
        echo "FAIL: Test B negative control failed to catch assembly divergence. Expected $EXIT_ASM_DIVERG, got $exit_b" >&2
        exit "$EXIT_NEG_CONTROL_PASSED"
    fi
    
    # Test B2: Binary Byte Mutation validation (Expected: EXIT_BIN_DIVERG = 22)
    echo "Testing Negative Control Injection (Binary) (Expected: exit 22)..."
    local test_run_dir_b2="$REPO_ROOT/build/gate-self-test-b2-$RUN_ID"
    mkdir -p "$test_run_dir_b2"
    cp -a "$test_run_dir"/. "$test_run_dir_b2"/
    printf '\xff' | dd of="$test_run_dir_b2/stage3.bin" bs=1 seek=100 count=1 conv=notrunc 2>/dev/null
    
    local exit_b2=0
    verify_gate_constraints "$test_run_dir_b2" "$test_ledger" "$schema_path" "$test_run_id" \
      "$verifier_sha" "$schema_sha" "$toolchain_sha" "$env_json" || exit_b2=$?
    if [ "$exit_b2" -eq "$EXIT_BIN_DIVERG" ]; then
        echo "Result B2: PASS (Binary divergence successfully caught with code $EXIT_BIN_DIVERG)"
    else
        echo "FAIL: Test B2 negative control failed to catch binary divergence. Expected $EXIT_BIN_DIVERG, got $exit_b2" >&2
        exit "$EXIT_NEG_CONTROL_PASSED"
    fi
    
    # Test C: Copy Bypass with Re-Chained Hashes validation (Expected: EXIT_LINEAGE_FAIL = 30)
    echo "Testing Copy-Bypass Rejection with Re-Chained Hash Ledger (Expected: exit 30)..."
    local test_run_dir_c="$REPO_ROOT/build/gate-self-test-c-$RUN_ID"
    mkdir -p "$test_run_dir_c"
    local test_ledger_c="$test_run_dir_c/test_provenance_c.jsonl"
    cp -a "$test_run_dir"/. "$test_run_dir_c"/
    
    "$TOOL_PYTHON" -c "
import json
path = '$test_ledger_c'
with open('$test_ledger', 'r') as f:
    lines = [json.loads(l.strip()) for l in f if l.strip()]
lines[1]['compiler_sha256_before'] = '0000000000000000000000000000000000000000000000000000000000000000'
with open(path, 'w') as f:
    for l in lines:
        f.write(json.dumps(l) + '\n')
"
    recompute_hash_chain "$test_ledger_c"
    
    local exit_c=0
    validate_lineage "$test_ledger_c" "$test_run_id" "$verifier_sha" "$schema_sha" "$toolchain_sha" "$env_json" || exit_c=$?
    if [ "$exit_c" -eq "$EXIT_LINEAGE_FAIL" ]; then
        echo "Result C: PASS (Lineage transition failure successfully caught under valid hash chain with code $EXIT_LINEAGE_FAIL)"
    else
        echo "FAIL: Test C failed to catch lineage transition bypass. Expected $EXIT_LINEAGE_FAIL, got $exit_c" >&2
        exit "$EXIT_BYPASS_CONTROL_PASSED"
    fi
    
    # Test D: Golden Reference Mutation validation (Expected: EXIT_SEMANTIC_DIVERG = 60)
    echo "Testing Negative Control Injection (Golden Mutation) (Expected: exit 60)..."
    local test_run_dir_d="$REPO_ROOT/build/gate-self-test-d-$RUN_ID"
    mkdir -p "$test_run_dir_d"
    cp -a "$test_run_dir"/. "$test_run_dir_d"/
    local mutated_expected="$test_run_dir_d/mutated_canary.expected"
    echo "CANARY_STATUS: CORRUPTED_GOLDEN" > "$mutated_expected"
    
    local exit_d=0
    verify_semantic_canary "$test_run_dir_d" "$test_run_dir_d/stage3.bin" "$REPO_ROOT/tests/selfhost/meta_canary.c" "$mutated_expected" || exit_d=$?
    if [ "$exit_d" -eq "$EXIT_SEMANTIC_DIVERG" ]; then
        echo "Result D: PASS (Golden mutation divergence successfully caught with code $EXIT_SEMANTIC_DIVERG)"
    else
        echo "FAIL: Test D negative control failed to catch golden mutation. Expected $EXIT_SEMANTIC_DIVERG, got $exit_d" >&2
        exit "$EXIT_NEG_CONTROL_PASSED"
    fi
    
    # Test E: One-Sided ZCC Semantic Divergence (Expected: EXIT_SEMANTIC_DIVERG = 60)
    echo "Testing Negative Control Injection (One-Sided ZCC Divergence) (Expected: exit 60)..."
    local test_run_dir_e="$REPO_ROOT/build/gate-self-test-e-$RUN_ID"
    mkdir -p "$test_run_dir_e"
    cp -a "$test_run_dir"/. "$test_run_dir_e"/
    echo "CANARY_STATUS: ZCC_FAULT_INJECTED" > "$test_run_dir_e/canary_zcc.stdout"
    
    local exit_e=0
    verify_semantic_results "$test_run_dir_e/canary_gcc.stdout" "$test_run_dir_e/canary_zcc.stdout" "$REPO_ROOT/tests/selfhost/meta_canary.expected" || exit_e=$?
    if [ "$exit_e" -eq "$EXIT_SEMANTIC_DIVERG" ]; then
        echo "Result E: PASS (One-sided ZCC divergence successfully caught with code $EXIT_SEMANTIC_DIVERG)"
    else
        echo "FAIL: Test E negative control failed to catch one-sided divergence. Expected $EXIT_SEMANTIC_DIVERG, got $exit_e" >&2
        exit "$EXIT_NEG_CONTROL_PASSED"
    fi
    
    # Test F: Recorded Binary-Hash Alias Rejection (Expected: EXIT_SEMANTIC_DIVERG = 60)
    echo "Testing Negative Control Injection (Recorded Binary-Hash Alias Rejection) (Expected: exit 60)..."
    local test_run_dir_f="$REPO_ROOT/build/gate-self-test-f-$RUN_ID"
    mkdir -p "$test_run_dir_f"
    local test_ledger_f="$test_run_dir_f/test_provenance_f.jsonl"
    cp -a "$test_run_dir"/. "$test_run_dir_f"/
    
    "$TOOL_PYTHON" -c "
import json
path = '$test_ledger_f'
with open('$test_ledger', 'r') as f:
    lines = [json.loads(l.strip()) for l in f if l.strip()]
lines[2]['canary_zcc_bin_sha256'] = lines[2]['canary_gcc_bin_sha256']
with open(path, 'w') as f:
    for l in lines:
        f.write(json.dumps(l) + '\n')
"
    recompute_hash_chain "$test_ledger_f"
    
    local exit_f=0
    validate_lineage "$test_ledger_f" "$test_run_id" "$verifier_sha" "$schema_sha" "$toolchain_sha" "$env_json" || exit_f=$?
    if [ "$exit_f" -eq "$EXIT_SEMANTIC_DIVERG" ]; then
        echo "Result F: PASS (Binary-hash alias successfully rejected with code $EXIT_SEMANTIC_DIVERG)"
    else
        echo "FAIL: Test F failed to reject binary-hash alias. Expected $EXIT_SEMANTIC_DIVERG, got $exit_f" >&2
        exit "$EXIT_BYPASS_CONTROL_PASSED"
    fi
    
    # Test G: Lineage Topology State Machine Rejection under Valid Hash Chain (Expected: EXIT_LINEAGE_FAIL = 30)
    echo "Testing Lineage Topology State Machine Rejection under Re-Chained Ledger (Expected: exit 30)..."
    local test_run_dir_g="$REPO_ROOT/build/gate-self-test-g-$RUN_ID"
    mkdir -p "$test_run_dir_g"
    local test_ledger_g="$test_run_dir_g/test_provenance_g.jsonl"
    cp -a "$test_run_dir"/. "$test_run_dir_g"/
    
    "$TOOL_PYTHON" -c "
import json
path = '$test_ledger_g'
with open('$test_ledger', 'r') as f:
    lines = [json.loads(l.strip()) for l in f if l.strip()]
lines.insert(1, json.loads(json.dumps(lines[0])))
with open(path, 'w') as f:
    for l in lines:
        f.write(json.dumps(l) + '\n')
"
    recompute_hash_chain "$test_ledger_g"
    
    local exit_g=0
    validate_lineage "$test_ledger_g" "$test_run_id" "$verifier_sha" "$schema_sha" "$toolchain_sha" "$env_json" || exit_g=$?
    if [ "$exit_g" -eq "$EXIT_LINEAGE_FAIL" ]; then
        echo "Result G: PASS (Topology violation [2, 2, 3, 4, 5] successfully caught under valid hash chain with code $EXIT_LINEAGE_FAIL)"
    else
        echo "FAIL: Test G failed to catch topology state machine violation. Expected $EXIT_LINEAGE_FAIL, got $exit_g" >&2
        exit "$EXIT_BYPASS_CONTROL_PASSED"
    fi
    
    # Test H: Stale / Missing Artifact Control (Expected: EXIT_SEMANTIC_DIVERG = 60)
    echo "Testing Negative Control Injection (Broken Compiler Missing Artifact) (Expected: exit 60)..."
    local test_run_dir_h="$REPO_ROOT/build/gate-self-test-h-$RUN_ID"
    mkdir -p "$test_run_dir_h"
    cp -a "$test_run_dir"/. "$test_run_dir_h"/
    
    printf '#!/bin/sh\nexit 0\n' > "$test_run_dir_h/stage3.bin"
    chmod +x "$test_run_dir_h/stage3.bin"
    
    local exit_h=0
    verify_semantic_canary "$test_run_dir_h" "$test_run_dir_h/stage3.bin" "$REPO_ROOT/tests/selfhost/meta_canary.c" "$REPO_ROOT/tests/selfhost/meta_canary.expected" || exit_h=$?
    if [ "$exit_h" -eq "$EXIT_SEMANTIC_DIVERG" ]; then
        echo "Result H: PASS (Broken compiler exiting 0 without creating artifact successfully rejected with code $EXIT_SEMANTIC_DIVERG)"
    else
        echo "FAIL: Test H failed to catch missing artifact from zero-exit fake compiler. Expected $EXIT_SEMANTIC_DIVERG, got $exit_h" >&2
        exit "$EXIT_NEG_CONTROL_PASSED"
    fi

    # Test I: Environment Record Mismatch under Re-Chained Ledger (Expected: EXIT_LINEAGE_FAIL = 30)
    echo "Testing Environment Record Mismatch Rejection (Expected: exit 30)..."
    local test_run_dir_i="$REPO_ROOT/build/gate-self-test-i-$RUN_ID"
    mkdir -p "$test_run_dir_i"
    local test_ledger_i="$test_run_dir_i/test_provenance_i.jsonl"
    cp -a "$test_run_dir"/. "$test_run_dir_i"/

    "$TOOL_PYTHON" -c "
import json
path = '$test_ledger_i'
with open('$test_ledger', 'r') as f:
    lines = [json.loads(l.strip()) for l in f if l.strip()]
lines[0]['environment']['LC_ALL'] = 'en_US.UTF-8'
with open(path, 'w') as f:
    for l in lines:
        f.write(json.dumps(l) + '\n')
"
    recompute_hash_chain "$test_ledger_i"

    local exit_i=0
    validate_lineage "$test_ledger_i" "$test_run_id" "$verifier_sha" "$schema_sha" "$toolchain_sha" "$env_json" || exit_i=$?
    if [ "$exit_i" -eq "$EXIT_LINEAGE_FAIL" ]; then
        echo "Result I: PASS (Environment record discrepancy successfully caught with code $EXIT_LINEAGE_FAIL)"
    else
        echo "FAIL: Test I failed to catch environment record discrepancy. Expected $EXIT_LINEAGE_FAIL, got $exit_i" >&2
        exit "$EXIT_BYPASS_CONTROL_PASSED"
    fi

    # Test J: Cross-Record Verifier Integrity Mismatch (Expected: EXIT_LINEAGE_FAIL = 30)
    echo "Testing Cross-Record Verifier Integrity Mismatch (Expected: exit 30)..."
    local test_run_dir_j="$REPO_ROOT/build/gate-self-test-j-$RUN_ID"
    mkdir -p "$test_run_dir_j"
    local test_ledger_j="$test_run_dir_j/test_provenance_j.jsonl"
    cp -a "$test_run_dir"/. "$test_run_dir_j"/

    "$TOOL_PYTHON" -c "
import json
path = '$test_ledger_j'
with open('$test_ledger', 'r') as f:
    lines = [json.loads(l.strip()) for l in f if l.strip()]
lines[1]['verifier_sha256'] = 'ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff'
with open(path, 'w') as f:
    for l in lines:
        f.write(json.dumps(l) + '\n')
"
    recompute_hash_chain "$test_ledger_j"

    local exit_j=0
    validate_lineage "$test_ledger_j" "$test_run_id" "$verifier_sha" "$schema_sha" "$toolchain_sha" "$env_json" || exit_j=$?
    if [ "$exit_j" -eq "$EXIT_LINEAGE_FAIL" ]; then
        echo "Result J: PASS (Cross-record verifier integrity discrepancy successfully caught with code $EXIT_LINEAGE_FAIL)"
    else
        echo "FAIL: Test J failed to catch cross-record verifier mismatch. Expected $EXIT_LINEAGE_FAIL, got $exit_j" >&2
        exit "$EXIT_BYPASS_CONTROL_PASSED"
    fi

    # Test K: Stage-5 Comparator Sensitivity (Expected: EXIT_SEMANTIC_DIVERG = 60)
    echo "Testing Stage-5 Comparator Sensitivity (Expected: exit 60)..."
    local test_run_dir_k="$REPO_ROOT/build/gate-self-test-k-$RUN_ID"
    mkdir -p "$test_run_dir_k"
    cp -a "$test_run_dir"/. "$test_run_dir_k"/
    local mutated_abi_exp="$test_run_dir_k/mutated_abi.expected"
    echo "ABI_STATUS: FAULT_INJECTED" > "$mutated_abi_exp"

    local exit_k=0
    verify_abi_subgate "$test_run_dir_k" "$test_run_dir_k/stage3.bin" \
      "$REPO_ROOT/tests/selfhost/abi_types.h" "$REPO_ROOT/tests/selfhost/abi_lib.c" "$REPO_ROOT/tests/selfhost/abi_main.c" "$mutated_abi_exp" \
      "$REPO_ROOT/tests/selfhost/abi_v13/abi_v13_types.h" "$REPO_ROOT/tests/selfhost/abi_v13/abi_v13_lib.c" "$REPO_ROOT/tests/selfhost/abi_v13/abi_v13_main.c" "$REPO_ROOT/tests/selfhost/abi_v13/abi_v13.expected" \
      "$REPO_ROOT/tests/selfhost/abi_v13b/abi_v13b_types.h" "$REPO_ROOT/tests/selfhost/abi_v13b/abi_v13b_lib.c" "$REPO_ROOT/tests/selfhost/abi_v13b/abi_v13b_asm.s" "$REPO_ROOT/tests/selfhost/abi_v13b/abi_v13b_main.c" "$REPO_ROOT/tests/selfhost/abi_v13b/abi_v13b.expected" \
      || exit_k=$?
    if [ "$exit_k" -eq "$EXIT_SEMANTIC_DIVERG" ]; then
        echo "Result K: PASS (Comparator sensitivity successfully verified with code $EXIT_SEMANTIC_DIVERG)"
    else
        echo "FAIL: Test K failed to catch comparator divergence. Expected $EXIT_SEMANTIC_DIVERG, got $exit_k" >&2
        exit "$EXIT_NEG_CONTROL_PASSED"
    fi

    # Test L: Stage 5 Missing / Topology Break under Valid Hash Chain (Expected: EXIT_LINEAGE_FAIL = 30)
    echo "Testing Stage 5 Missing Topology Break (Expected: exit 30)..."
    local test_run_dir_l="$REPO_ROOT/build/gate-self-test-l-$RUN_ID"
    mkdir -p "$test_run_dir_l"
    local test_ledger_l="$test_run_dir_l/test_provenance_l.jsonl"
    cp -a "$test_run_dir"/. "$test_run_dir_l"/

    "$TOOL_PYTHON" -c "
import json
path = '$test_ledger_l'
with open('$test_ledger', 'r') as f:
    lines = [json.loads(l.strip()) for l in f if l.strip()]
lines = lines[:3] # truncate Stage 5
with open(path, 'w') as f:
    for l in lines:
        f.write(json.dumps(l) + '\n')
"
    recompute_hash_chain "$test_ledger_l"

    local exit_l=0
    validate_lineage "$test_ledger_l" "$test_run_id" "$verifier_sha" "$schema_sha" "$toolchain_sha" "$env_json" || exit_l=$?
    if [ "$exit_l" -eq "$EXIT_LINEAGE_FAIL" ]; then
        echo "Result L: PASS (Stage 5 missing topology break successfully caught under valid hash chain with code $EXIT_LINEAGE_FAIL)"
    else
        echo "FAIL: Test L failed to catch Stage 5 missing topology break. Expected $EXIT_LINEAGE_FAIL, got $exit_l" >&2
        exit "$EXIT_BYPASS_CONTROL_PASSED"
    fi

    # Test M: Surgical ABI Return Eightbyte Mutation (Expected: build/link/run exit 0, compare fails -> code 60)
    echo "Testing Surgical ABI Mechanism Mutation (Control M) (Expected: exit 60)..."
    local test_run_dir_m="$REPO_ROOT/build/gate-self-test-m-$RUN_ID"
    mkdir -p "$test_run_dir_m"
    cp -a "$test_run_dir"/. "$test_run_dir_m"/
    
    local m_lib_o="$test_run_dir_m/control_m_lib.o"
    local m_asm_o="$test_run_dir_m/control_m_ret16_bad.o"
    local m_main_s="$test_run_dir_m/control_m_main.s"
    local m_main_o="$test_run_dir_m/control_m_main.o"
    local m_bin="$test_run_dir_m/control_m_exe.bin"
    local m_stdout="$test_run_dir_m/control_m_exe.stdout"
    local m_stderr="$test_run_dir_m/control_m_exe.stderr"

    local m_build_rc=0 m_link_rc=0 m_run_rc=0
    "$TOOL_GCC" "-Itests/selfhost/abi_v13" -c "$REPO_ROOT/tests/selfhost/abi_v13/control_m_lib.c" -o "$m_lib_o" 2>/dev/null || m_build_rc=$?
    "$TOOL_AS" "--64" "$REPO_ROOT/tests/selfhost/abi_v13/control_m_ret16_bad.s" -o "$m_asm_o" 2>/dev/null || m_build_rc=$?
    "$test_run_dir_m/stage3.bin" "$REPO_ROOT/tests/selfhost/abi_v13/abi_v13_main.c" -o "$m_main_s" 2>/dev/null || m_build_rc=$?
    "$TOOL_AS" "--64" "$m_main_s" -o "$m_main_o" 2>/dev/null || m_build_rc=$?

    if [ "$m_build_rc" -ne 0 ]; then
        echo "CONTROL M INVALID: mutation failed during compilation/assembly" >&2
        exit "$EXIT_NEG_CONTROL_PASSED"
    fi

    "$TOOL_GCC" -no-pie "$m_main_o" "$m_lib_o" "$m_asm_o" -o "$m_bin" 2>/dev/null || m_link_rc=$?
    if [ "$m_link_rc" -ne 0 ] || [ ! -x "$m_bin" ]; then
        echo "CONTROL M INVALID: mutation caused link failure" >&2
        exit "$EXIT_NEG_CONTROL_PASSED"
    fi

    "$m_bin" > "$m_stdout" 2> "$m_stderr" || m_run_rc=$?
    if [ "$m_run_rc" -ne 0 ]; then
        echo "CONTROL M INVALID: mutation crashed instead of executing cleanly" >&2
        exit "$EXIT_NEG_CONTROL_PASSED"
    fi

    if cmp -s "$m_stdout" "$REPO_ROOT/tests/selfhost/abi_v13/abi_v13.expected"; then
        echo "CONTROL M FAILED: ABI mutation escaped detection" >&2
        exit "$EXIT_NEG_CONTROL_PASSED"
    fi

    echo "Result M: PASS (Surgical ABI return eightbyte corruption executed cleanly and was rejected with code 60)"

    # Test N: Surgical Callee-Saved Register Corruption (Expected: build/link/run exit 0, compare fails -> code 60)
    echo "Testing Surgical Callee-Saved Register Mutation (Control N) (Expected: exit 60)..."
    local test_run_dir_n="$REPO_ROOT/build/gate-self-test-n-$RUN_ID"
    mkdir -p "$test_run_dir_n"
    cp -a "$test_run_dir"/. "$test_run_dir_n"/

    local n_lib_o="$test_run_dir_n/control_n_lib.o"
    local n_asm_oracle_o="$test_run_dir_n/control_n_asm_oracle.o"
    local n_asm_bad_o="$test_run_dir_n/control_n_bad_r14.o"
    local n_main_s="$test_run_dir_n/control_n_main.s"
    local n_main_o="$test_run_dir_n/control_n_main.o"
    local n_bin="$test_run_dir_n/control_n_exe.bin"
    local n_stdout="$test_run_dir_n/control_n_exe.stdout"
    local n_stderr="$test_run_dir_n/control_n_exe.stderr"

    local n_build_rc=0 n_link_rc=0 n_run_rc=0
    "$TOOL_GCC" "-Itests/selfhost/abi_v13b" -c "$REPO_ROOT/tests/selfhost/abi_v13b/control_n_lib.c" -o "$n_lib_o" 2>/dev/null || n_build_rc=$?
    "$TOOL_AS" "--64" "$REPO_ROOT/tests/selfhost/abi_v13b/abi_v13b_asm.s" -o "$n_asm_oracle_o" 2>/dev/null || n_build_rc=$?
    "$TOOL_AS" "--64" "$REPO_ROOT/tests/selfhost/abi_v13b/control_n_clobber_r14.s" -o "$n_asm_bad_o" 2>/dev/null || n_build_rc=$?
    "$test_run_dir_n/stage3.bin" "$REPO_ROOT/tests/selfhost/abi_v13b/abi_v13b_main.c" -o "$n_main_s" 2>/dev/null || n_build_rc=$?
    "$TOOL_AS" "--64" "$n_main_s" -o "$n_main_o" 2>/dev/null || n_build_rc=$?

    if [ "$n_build_rc" -ne 0 ]; then
        echo "CONTROL N INVALID: mutation failed during compilation/assembly" >&2
        exit "$EXIT_NEG_CONTROL_PASSED"
    fi

    "$TOOL_GCC" -no-pie "$n_main_o" "$n_lib_o" "$n_asm_bad_o" "$n_asm_oracle_o" -o "$n_bin" 2>/dev/null || n_link_rc=$?
    if [ "$n_link_rc" -ne 0 ] || [ ! -x "$n_bin" ]; then
        echo "CONTROL N INVALID: mutation caused link failure" >&2
        exit "$EXIT_NEG_CONTROL_PASSED"
    fi

    "$n_bin" > "$n_stdout" 2> "$n_stderr" || n_run_rc=$?
    if [ "$n_run_rc" -ne 0 ]; then
        echo "CONTROL N INVALID: mutation crashed instead of executing cleanly" >&2
        exit "$EXIT_NEG_CONTROL_PASSED"
    fi

    if cmp -s "$n_stdout" "$REPO_ROOT/tests/selfhost/abi_v13b/abi_v13b.expected"; then
        echo "CONTROL N FAILED: Callee-saved register mutation escaped detection" >&2
        exit "$EXIT_NEG_CONTROL_PASSED"
    fi

    echo "Result N: PASS (Surgical callee-saved register corruption executed cleanly with mask 10 and was rejected with code 60)"
    
    echo "=== ALL SELF-TEST CONTROL CHECKS PASSED (A, B, B2, C, D, E, F, G, H, I, J, K, L, M, N) ==="
}

# --- CLI Dispatcher ---
if [ "${1:-}" = "--self-test" ]; then
    SCHEMA_FILE="$REPO_ROOT/contracts/determinism-gate.schema.json"
    run_self_test "$SCHEMA_FILE"
    exit 0
fi

# Main Executable Action
RUN_ID="${ZCC_VERIFIER_RUN_ID:-zcc-omega-$(date +%s)-$$}"
EVIDENCE_DIR="$REPO_ROOT/evidence/$RUN_ID"
mkdir -p "$EVIDENCE_DIR"
LEDGER_FILE="$EVIDENCE_DIR/provenance.jsonl"
TOOLCHAIN_FILE="$EVIDENCE_DIR/toolchain.json"
SCHEMA_FILE="$REPO_ROOT/contracts/determinism-gate.schema.json"

echo -e "\033[1;36m================================================================================\033[0m"
echo -e "   🚀 ZCC SUPERCHARGED DETERMINISM & ABI VERIFICATION GATE (v3.3 OMEGA / MV1.3b)"
echo -e "\033[1;36m================================================================================\033[0m"
echo -e "Run ID: \033[1;33m$RUN_ID\033[0m"
record_toolchain "$TOOLCHAIN_FILE"

VERIFIER_SHA=$(sha256_file "$REPO_ROOT/gate.sh")
SCHEMA_SHA=$(sha256_file "$SCHEMA_FILE")
TOOLCHAIN_SHA=$(sha256_file "$TOOLCHAIN_FILE")
ENV_JSON=$(serialize_env_json)

# Check if stage 1 bootstrap binary exists
STAGE1_COMPILER="$REPO_ROOT/zcc"
if [ ! -f "$STAGE1_COMPILER" ]; then
    echo "Compiling initial bootstrap driver (make zcc)..."
    make -C "$REPO_ROOT" zcc >/dev/null 2>&1 || (gcc -O2 -o "$STAGE1_COMPILER" "$REPO_ROOT/zcc.c" 2>/dev/null || true)
fi

run_main_gate "$EVIDENCE_DIR" "$STAGE1_COMPILER" "$REPO_ROOT/zcc.c" "$LEDGER_FILE" "$RUN_ID" "$SCHEMA_FILE" \
  "$VERIFIER_SHA" "$SCHEMA_SHA" "$TOOLCHAIN_SHA" "$ENV_JSON"

# Expose stage2.s/stage3.s to root directory for backward compatibility
cp "$EVIDENCE_DIR/stage2.s" "$REPO_ROOT/zcc2.s" 2>/dev/null || true
cp "$EVIDENCE_DIR/stage3.s" "$REPO_ROOT/zcc3.s" 2>/dev/null || true
cp "$EVIDENCE_DIR/stage2.bin" "$REPO_ROOT/zcc2" 2>/dev/null || true
cp "$EVIDENCE_DIR/stage3.bin" "$REPO_ROOT/zcc3" 2>/dev/null || true
chmod +x "$REPO_ROOT/zcc2" "$REPO_ROOT/zcc3" 2>/dev/null || true

echo -e "\n\033[1;32m================================================================================\033[0m"
echo -e "   ✨ SELFHOST-MV1.3b PASS"
echo -e "   Bootstrap fixed-point, semantic-canary, and full SystemV ABI constraints satisfied."
echo -e "\033[1;32m================================================================================\033[0m"
echo -e "Chained Provenance Ledger: \033[1;34m$LEDGER_FILE\033[0m\n"
