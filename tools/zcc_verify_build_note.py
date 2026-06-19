#!/usr/bin/env python3
"""
zcc_verify_build_note.py — Vector 8.3 Build Provenance Attestation Verifier

Reads the .note.zcc.build ELF note from a linked executable and verifies
the embedded SHA-256 hashes against the live compiler, linker, source
manifest, and build policy on disk.

Expected ELF note structure:
  namesz   = 4         ("ZCC\0")
  descsz   = 136
  type     = 0x7cd
  name     = "ZCC\0"
  desc     = [136 bytes]
    schema_version          (4)
    flags                   (4)
    zcc_sha256              (32)
    zld_sha256              (32)
    source_manifest_sha256  (32)
    build_policy_sha256     (32)

Usage:
  python3 tools/zcc_verify_build_note.py freestanding_app \\
      --zcc ./zcc \\
      --zld ./zld \\
      --sources manifest.sources \\
      --policy build_policy.json
"""

import argparse
import hashlib
import json
import os
import struct
import sys

NOTE_TYPE_BUILD = 0x7cd
DESC_SIZE       = 136


# ── ELF note scanner ────────────────────────────────────────────────────────

def find_build_note(elf_path):
    """
    Scan an ELF-64 binary for the .note.zcc.build note (type 0x7cd, owner "ZCC").
    Returns the raw 136-byte descriptor or None.
    """
    with open(elf_path, "rb") as f:
        data = f.read()

    # Validate ELF magic
    if data[:4] != b"\x7fELF":
        print(f"error: {elf_path} is not an ELF file", file=sys.stderr)
        sys.exit(1)

    # ELF-64 header field offsets (absolute, per spec):
    #   e_phoff     @ 32  (Q)
    #   e_shoff     @ 40  (Q)
    #   e_phentsize @ 54  (H)
    #   e_phnum     @ 56  (H)
    #   e_shentsize @ 58  (H)
    #   e_shnum     @ 60  (H)
    #   e_shstrndx  @ 62  (H)
    e_phoff     = struct.unpack_from("<Q", data, 32)[0]
    e_shoff     = struct.unpack_from("<Q", data, 40)[0]
    e_phentsize = struct.unpack_from("<H", data, 54)[0]
    e_phnum     = struct.unpack_from("<H", data, 56)[0]
    e_shentsize = struct.unpack_from("<H", data, 58)[0]
    e_shnum     = struct.unpack_from("<H", data, 60)[0]

    # Walk section headers to find SHT_NOTE sections
    SHT_NOTE = 7
    for i in range(e_shnum):
        sh_off = e_shoff + i * e_shentsize
        # Elf64_Shdr layout (64 bytes total):
        #   sh_name     (I)  @ +0
        #   sh_type     (I)  @ +4
        #   sh_flags    (Q)  @ +8
        #   sh_addr     (Q)  @ +16
        #   sh_offset   (Q)  @ +24
        #   sh_size     (Q)  @ +32
        sh_name   = struct.unpack_from("<I", data, sh_off)[0]
        sh_type   = struct.unpack_from("<I", data, sh_off + 4)[0]
        sh_offset = struct.unpack_from("<Q", data, sh_off + 24)[0]
        sh_size   = struct.unpack_from("<Q", data, sh_off + 32)[0]

        if sh_type != SHT_NOTE or sh_size == 0:
            continue
        note_data = data[sh_offset : sh_offset + sh_size]
        desc = _scan_note_blob(note_data)
        if desc is not None:
            return desc

    # Also scan PT_NOTE segments in case section headers are minimal
    PT_NOTE = 4
    for i in range(e_phnum):
        ph_off = e_phoff + i * e_phentsize
        p_type  = struct.unpack_from("<I", data, ph_off)[0]
        if p_type != PT_NOTE:
            continue
        p_offset = struct.unpack_from("<Q", data, ph_off + 8)[0]
        p_filesz = struct.unpack_from("<Q", data, ph_off + 32)[0]
        note_data = data[p_offset : p_offset + p_filesz]
        desc = _scan_note_blob(note_data)
        if desc is not None:
            return desc

    return None



def _scan_note_blob(blob):
    """Scan a raw note blob, return 136-byte descriptor if ZCC/0x7cd found."""
    pos = 0
    while pos + 12 <= len(blob):
        namesz, descsz, note_type = struct.unpack_from("<III", blob, pos)
        pos += 12
        name_raw = blob[pos : pos + namesz]
        pos += (namesz + 3) & ~3  # 4-byte aligned
        desc_raw = blob[pos : pos + descsz]
        pos += (descsz + 3) & ~3

        name = name_raw.rstrip(b"\x00").decode("latin1", errors="replace")
        if name == "ZCC" and note_type == NOTE_TYPE_BUILD and descsz == DESC_SIZE:
            return desc_raw
    return None


# ── Hash helpers ─────────────────────────────────────────────────────────────

def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while True:
            chunk = f.read(1024 * 1024)
            if not chunk:
                break
            h.update(chunk)
    return h.digest()


def sha256_source_manifest(sources_path):
    with open(sources_path, "r") as f:
        lines = [l.strip() for l in f if l.strip()]
    normalized = sorted(p.replace("\\", "/") for p in lines)
    h = hashlib.sha256()
    for norm_path in normalized:
        h.update(norm_path.encode("utf-8"))
        h.update(b"\x00")
        with open(norm_path, "rb") as fh:
            while True:
                chunk = fh.read(1024 * 1024)
                if not chunk:
                    break
                h.update(chunk)
    return h.digest()


def sha256_policy(policy_path):
    with open(policy_path, "r") as f:
        policy = json.load(f)
    canonical = json.dumps(policy, sort_keys=True, separators=(",", ":"))
    return hashlib.sha256(canonical.encode("utf-8")).digest()


# ── Descriptor parser ────────────────────────────────────────────────────────

def parse_descriptor(desc):
    """
    Parse the 136-byte .note.zcc.build descriptor.

    Layout:
      schema_version          (4)  @ 0
      flags                   (4)  @ 4
      zcc_sha256              (32) @ 8
      zld_sha256              (32) @ 40
      source_manifest_sha256  (32) @ 72
      build_policy_sha256     (32) @ 104
    """
    if len(desc) < DESC_SIZE:
        raise ValueError(f"descriptor too short: {len(desc)} < {DESC_SIZE}")
    schema_version, flags = struct.unpack_from("<II", desc, 0)
    zcc_sha    = desc[8:40]
    zld_sha    = desc[40:72]
    src_sha    = desc[72:104]
    policy_sha = desc[104:136]
    return schema_version, flags, zcc_sha, zld_sha, src_sha, policy_sha


# ── Main ─────────────────────────────────────────────────────────────────────

def main():
    p = argparse.ArgumentParser(
        description="ZCC Vector 8.3 — Build Provenance Attestation Verifier"
    )
    p.add_argument("elf", help="Path to the ELF executable to verify")
    p.add_argument("--zcc",     required=True, help="Path to the zcc compiler binary")
    p.add_argument("--zld",     required=True, help="Path to the zld linker binary")
    p.add_argument("--sources", required=True, help="Path to sources manifest file")
    p.add_argument("--policy",  required=True, help="Path to build_policy.json")
    args = p.parse_args()

    for label, path in [("ELF", args.elf), ("zcc", args.zcc), ("zld", args.zld),
                        ("sources", args.sources), ("policy", args.policy)]:
        if not os.path.isfile(path):
            print(f"error: {label} file not found: {path}", file=sys.stderr)
            sys.exit(1)

    print(f"=== ZCC Build Attestation Scan: {args.elf} ===")
    desc = find_build_note(args.elf)
    if desc is None:
        print("❌ FAIL: No .note.zcc.build ELF note found.", file=sys.stderr)
        sys.exit(1)

    schema_version, flags, emb_zcc, emb_zld, emb_src, emb_policy = parse_descriptor(desc)
    print(f"=== ZCC Build Attestation Found ===")
    print(f"  Schema Version: {schema_version}")
    print(f"  Flags:          0x{flags:08x}")
    print()

    all_pass = True
    checks = [
        ("ZCC SHA-256",              args.zcc,     sha256_file,             emb_zcc),
        ("zld SHA-256",              args.zld,     sha256_file,             emb_zld),
        ("Source Manifest SHA-256",  args.sources, sha256_source_manifest,  emb_src),
        ("Build Policy SHA-256",     args.policy,  sha256_policy,           emb_policy),
    ]

    for label, path, hash_fn, embedded in checks:
        computed = hash_fn(path)
        if computed == embedded:
            print(f"  {label}: match ✓")
        else:
            print(f"  {label}: MISMATCH ✗")
            print(f"    embedded:  {embedded.hex()}")
            print(f"    computed:  {computed.hex()}")
            all_pass = False

    print()
    if all_pass:
        print("✅ Build provenance verification successful.")
        sys.exit(0)
    else:
        print("❌ Build provenance verification FAILED.", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
