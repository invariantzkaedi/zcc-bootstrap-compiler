#!/usr/bin/env bash
set -Eeuo pipefail

# Normalized gate runner
# Usage:
#   bash scripts/gate_runner.sh \
#     --name gate1 \
#     --command "make selfhost && cmp zcc2.s zcc3.s" \
#     --out artifacts/incidents/INC-.../gates \
#     --exit-map artifacts/incidents/INC-.../gates/gate_exit_codes.txt

NAME=""
COMMAND=""
OUTDIR=""
EXIT_MAP=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --name) NAME="${2:-}"; shift 2 ;;
    --command) COMMAND="${2:-}"; shift 2 ;;
    --out) OUTDIR="${2:-}"; shift 2 ;;
    --exit-map) EXIT_MAP="${2:-}"; shift 2 ;;
    -h|--help)
      cat <<'USAGE'
Usage:
  gate_runner.sh --name <gate> --command "<cmd>" --out <dir> --exit-map <file>
USAGE
      exit 0
      ;;
    *)
      echo "[gate-runner] unknown arg: $1" >&2
      exit 2
      ;;
  esac
done

[[ -n "$NAME" ]] || { echo "[gate-runner] --name required" >&2; exit 2; }
[[ -n "$COMMAND" ]] || { echo "[gate-runner] --command required" >&2; exit 2; }
[[ -n "$OUTDIR" ]] || { echo "[gate-runner] --out required" >&2; exit 2; }
[[ -n "$EXIT_MAP" ]] || { echo "[gate-runner] --exit-map required" >&2; exit 2; }

mkdir -p "$OUTDIR"
OUTFILE="$OUTDIR/${NAME}.txt"

{
  echo "[$NAME] command:"
  echo "$COMMAND"
  echo
} > "$OUTFILE"

set +e
bash -lc "$COMMAND" >> "$OUTFILE" 2>&1
RC=$?
set -e

echo "exit_code=$RC" >> "$OUTFILE"
echo "${NAME}=${RC}" >> "$EXIT_MAP"

exit "$RC"
