#!/usr/bin/env python3
"""
zcc_build_attest.py — Vector 8.3 Build Provenance Attestation Generator

Produces a deterministic binary attestation blob recording the cryptographic
identity of the compiler (zcc), linker (zld), source file manifest, and build
policy that produced a given ELF artifact.

Binary blob layout (144 bytes):
  magic                    (8)   0x444c425f43435a  "ZCC_BLD\0"
  schema_version           (4)   currently 1
  flags                    (4)   currently 0
  zcc_sha256               (32)
  zld_sha256               (32)
  source_manifest_sha256   (32)
  build_policy_sha256      (32)
  Total: 8 + 4 + 4 + 128 = 144 bytes

Canonical hashing rules (determinism invariants):
  - Sort file paths lexicographically before hashing
  - Normalize path separators to forward slash
  - Hash raw file bytes only
  - Exclude timestamps, absolute machine paths, and build temp dirs
  - JSON keys in stable sorted order

Usage:
  python3 tools/zcc_build_attest.py \\
      --zcc ./zcc \\
      --zld ./zld \\
      --sources manifest.sources \\
      --policy build_policy.json \\
      --emit-bin build.attest.bin \\
      --emit-json build.attest.json
"""

import argparse
import hashlib
import json
import os
import struct
import sys

# Magic: "ZCC_BLD\0" little-endian uint64
ZCC_BUILD_MAGIC = 0x444c425f43435a  # b"ZCC_BLD\x00" as LE uint64
SCHEMA_VERSION  = 1
FLAGS           = 0

BLOB_SIZE = 144  # 8 + 4 + 4 + 128


def sha256_file(path):
    """Hash a binary file, returning raw 32-byte digest."""
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while True:
            chunk = f.read(1024 * 1024)
            if not chunk:
                break
            h.update(chunk)
    return h.digest()


def sha256_source_manifest(sources_path):
    """
    Hash the source file manifest deterministically.

    Reads the sources file (one path per line), sorts lines lexicographically,
    normalizes separators, and hashes each file's bytes in sorted order.
    Returns the SHA-256 digest of the concatenated (path_norm + file_bytes) stream.
    """
    with open(sources_path, "r") as f:
        lines = [l.strip() for l in f if l.strip()]

    # Normalize and sort
    normalized = sorted(p.replace("\\", "/") for p in lines)

    h = hashlib.sha256()
    for norm_path in normalized:
        # Include normalized path as a deterministic separator
        h.update(norm_path.encode("utf-8"))
        h.update(b"\x00")
        # Hash the file bytes relative to the working directory
        with open(norm_path, "rb") as fh:
            while True:
                chunk = fh.read(1024 * 1024)
                if not chunk:
                    break
                h.update(chunk)

    return h.digest()


def sha256_policy(policy_path):
    """
    Hash the build policy JSON deterministically.

    Loads the JSON, re-serializes with sorted keys (no extra whitespace),
    and hashes the canonical UTF-8 bytes. This is invariant to formatting.
    """
    with open(policy_path, "r") as f:
        policy = json.load(f)
    canonical = json.dumps(policy, sort_keys=True, separators=(",", ":"))
    return hashlib.sha256(canonical.encode("utf-8")).digest()


def build_blob(zcc_sha, zld_sha, src_sha, policy_sha):
    """Pack the 144-byte attestation blob."""
    return struct.pack(
        "<QII32s32s32s32s",
        ZCC_BUILD_MAGIC,
        SCHEMA_VERSION,
        FLAGS,
        zcc_sha,
        zld_sha,
        src_sha,
        policy_sha,
    )


def main():
    p = argparse.ArgumentParser(
        description="ZCC Vector 8.3 — Build Provenance Attestation Generator"
    )
    p.add_argument("--zcc",     required=True, help="Path to the zcc compiler binary")
    p.add_argument("--zld",     required=True, help="Path to the zld linker binary")
    p.add_argument("--sources", required=True, help="Path to sources manifest file (one path per line)")
    p.add_argument("--policy",  required=True, help="Path to build_policy.json")
    p.add_argument("--emit-bin", required=True, help="Output path for binary attestation blob (.bin)")
    p.add_argument("--emit-json", required=True, help="Output path for JSON manifest (.json)")
    args = p.parse_args()

    # Validate inputs exist
    for label, path in [("zcc", args.zcc), ("zld", args.zld),
                        ("sources", args.sources), ("policy", args.policy)]:
        if not os.path.isfile(path):
            print(f"error: {label} file not found: {path}", file=sys.stderr)
            sys.exit(1)

    print(f"[zcc_build_attest] Hashing compiler:  {args.zcc}")
    zcc_sha    = sha256_file(args.zcc)

    print(f"[zcc_build_attest] Hashing linker:    {args.zld}")
    zld_sha    = sha256_file(args.zld)

    print(f"[zcc_build_attest] Hashing sources:   {args.sources}")
    src_sha    = sha256_source_manifest(args.sources)

    print(f"[zcc_build_attest] Hashing policy:    {args.policy}")
    policy_sha = sha256_policy(args.policy)

    blob = build_blob(zcc_sha, zld_sha, src_sha, policy_sha)
    assert len(blob) == BLOB_SIZE, f"blob size mismatch: {len(blob)} != {BLOB_SIZE}"

    # Write binary blob
    with open(args.emit_bin, "wb") as f:
        f.write(blob)
    print(f"[zcc_build_attest] Wrote binary:      {args.emit_bin} ({BLOB_SIZE} bytes)")

    # Write JSON manifest
    manifest = {
        "schema_version":          SCHEMA_VERSION,
        "flags":                   FLAGS,
        "zcc_sha256":              zcc_sha.hex(),
        "zld_sha256":              zld_sha.hex(),
        "source_manifest_sha256":  src_sha.hex(),
        "build_policy_sha256":     policy_sha.hex(),
    }
    with open(args.emit_json, "w") as f:
        json.dump(manifest, f, sort_keys=True, indent=2)
        f.write("\n")
    print(f"[zcc_build_attest] Wrote manifest:    {args.emit_json}")
    print()
    print("=== Build Attestation Summary ===")
    for k, v in sorted(manifest.items()):
        print(f"  {k}: {v}")
    print()
    print("✅ Build provenance attestation complete.")


if __name__ == "__main__":
    main()
