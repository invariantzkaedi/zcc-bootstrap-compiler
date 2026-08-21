#!/bin/sh
# Production Tier-0 execution under podman: this is where host-mount denial,
# UID separation, and pids-limit are actually ENFORCED (the in-process runner
# enforces rlimits/env/netns; podman enforces the rest).
# UNVERIFIED in build container - validate in the lab distro.
set -eu
CANDIDATE="$1"
LAB=/opt/zkaedi-lab
exec podman run --rm \
  --network=none \
  --pids-limit=128 \
  --memory=512m --cpus=1 \
  --read-only --tmpfs /work:rw,size=64m \
  --security-opt=no-new-privileges \
  --cap-drop=ALL \
  --user 1000:1000 \
  -v "$LAB":/opt/zkaedi-lab:ro \
  -v "$LAB/receipts":/opt/zkaedi-lab/receipts:rw \
  -v "$LAB/runs":/opt/zkaedi-lab/runs:rw \
  -w /opt/zkaedi-lab \
  docker.io/library/python:3.12-slim \
  python3 runner/run_candidate.py "$CANDIDATE"
