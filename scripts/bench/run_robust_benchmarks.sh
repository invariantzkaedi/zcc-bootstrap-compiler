#!/usr/bin/env bash
set -euo pipefail

BASE=""
CAND=""
SUITE=""
WARMUP=3
RUNS=25
TRIM=0.10
OUT="out/bench"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --base) BASE="$2"; shift 2;;
    --cand) CAND="$2"; shift 2;;
    --suite) SUITE="$2"; shift 2;;
    --warmup) WARMUP="$2"; shift 2;;
    --runs) RUNS="$2"; shift 2;;
    --trim) TRIM="$2"; shift 2;;
    --out) OUT="$2"; shift 2;;
    *) echo "unknown arg: $1"; exit 2;;
  esac
done

[[ -x "$BASE" && -x "$CAND" && -f "$SUITE" ]] || { echo "missing args"; exit 2; }
mkdir -p "$OUT/raw"

echo "benchmark,variant,compile_us,run_us" > "$OUT/raw/samples.csv"

time_us() {
  python3 - "$@" << 'PY'
import subprocess, sys, time
cmd=sys.argv[1:]
t0=time.perf_counter_ns()
subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
t1=time.perf_counter_ns()
print((t1-t0)//1000)
PY
}

pin_env() {
  export OMP_NUM_THREADS=1
  export OPENBLAS_NUM_THREADS=1
  export MKL_NUM_THREADS=1
  export VECLIB_MAXIMUM_THREADS=1
  export NUMEXPR_NUM_THREADS=1
}

run_variant() {
  local variant="$1"
  local bin="$2"

  while IFS= read -r bench || [[ -n "$bench" ]]; do
    [[ -z "$bench" || "$bench" =~ ^# ]] && continue
    bname="$(basename "$bench" .zc)"
    exe="$OUT/raw/${bname}.${variant}.out"

    for ((i=0;i<5;i++)); do
      c_us="$(time_us "$bin" "$bench" -o "$exe")"
      echo "${bname},${variant},${c_us}," >> "$OUT/raw/samples.csv"
    done

    for ((i=0;i<WARMUP;i++)); do
      "$exe" >/dev/null 2>&1 || true
    done

    for ((i=0;i<RUNS;i++)); do
      r_us="$(time_us "$exe")"
      echo "${bname},${variant},,${r_us}" >> "$OUT/raw/samples.csv"
    done
  done < "$SUITE"
}

pin_env
run_variant base "$BASE"
run_variant cand "$CAND"

python3 scripts/bench/summarize_robust.py   --samples "$OUT/raw/samples.csv"   --trim "$TRIM"   --out "$OUT/summary.json"
