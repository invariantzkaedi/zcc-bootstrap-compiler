#!/usr/bin/env python3
"""
Sovereign Single-Binary GGUF -> Standalone Win64 PE32+ Executable Compiler
==========================================================================
Compiles any supported .gguf transformer model directly into a native Win64
PE32+ executable targeting NVIDIA Blackwell sm_120 (zero MSVC / MinGW / nvcc /
CUDA Toolkit; imports ONLY C:\\Windows\\System32\\{msvcrt,nvcuda}.dll):
  1. Invokes `./zcc --target=ptx --gpu-arch=<arch> --model <model.gguf>` and
     `build_win64/zcc_sov_stage3.exe` to emit specialized AOT PTX, verifying
     byte-for-byte identity (`SHA-256` + Windows `fc /b`).
  2. Embeds the sovereign AOT PTX into `embedded_aot_ptx.h` (`-DZCC_EMBEDDED_AOT_PTX`).
  3. Optionally (`--embed-weights`, for models <= 3,997,696 bytes so uint32[]
     stays within ZCC `MAX_INIT = 1048576`) packs raw GGUF weights into
     `embedded_gguf_blob.h` (`-DZCC_EMBEDDED_GGUF`) so the output `.exe` runs
     standalone from an empty directory with zero external files.
  4. Compiles & links via BOTH `./zcc` and `zcc_sov_stage3.exe`, asserts PE32+
     byte-for-byte parity (`fc /b` + `SHA-256`), and audits System32 DLL imports.
"""

import argparse
import hashlib
import os
import struct
import subprocess
import sys

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if REPO_ROOT not in sys.path:
    sys.path.insert(0, REPO_ROOT)

from tools.build_lua_win64 import (
    BUILD_DIR,
    ZCC_BIN,
    ZCC_WIN64,
    compile_and_link_win64_units,
    run_fc_b,
    run_win_cmd,
    sha256_file,
    to_rel_posix,
    to_rel_win,
)
from tools.build_gpu_win64 import audit_pe32plus_multi_dll, write_ptx_header

# Safely below part1.c MAX_INIT (1,048,576 initializer elements)
MAX_EMBED_GGUF_BYTES = 3997696


def write_gguf_u32_header(gguf_path: str, header_path: str) -> tuple[int, int]:
    """Pack raw GGUF bytes as little-endian uint32_t words into a C header."""
    sz = os.path.getsize(gguf_path)
    if sz <= 0:
        raise ValueError(f"Empty GGUF file: {gguf_path}")
    if sz > MAX_EMBED_GGUF_BYTES:
        raise ValueError(
            f"GGUF file '{gguf_path}' ({sz} bytes) exceeds --embed-weights limit "
            f"of {MAX_EMBED_GGUF_BYTES} bytes (ZCC MAX_INIT=1048576 uint32 words)."
        )
    with open(gguf_path, "rb") as f:
        raw = f.read()
    pad_len = (-len(raw)) % 4
    if pad_len:
        raw_padded = raw + (b"\x00" * pad_len)
    else:
        raw_padded = raw
    words = struct.unpack(f"<{len(raw_padded) // 4}I", raw_padded)

    os.makedirs(os.path.dirname(header_path), exist_ok=True)
    lines = [
        f"/* Auto-generated embedded GGUF payload from {to_rel_posix(gguf_path)} */\n",
        "#ifndef ZCC_EMBEDDED_GGUF_BLOB_H\n",
        "#define ZCC_EMBEDDED_GGUF_BLOB_H\n",
        f"static const size_t g_embedded_gguf_len = {len(raw)}U;\n",
        "static const unsigned int g_embedded_gguf_u32[] = {\n",
    ]
    n_words = len(words)
    for i in range(0, n_words, 8):
        chunk = words[i : i + 8]
        items = ", ".join(f"0x{w:08X}U" for w in chunk)
        comma = "," if (i + 8 < n_words) else ""
        lines.append(f"    {items}{comma}\n")
    lines.append("};\n")
    lines.append("#endif /* ZCC_EMBEDDED_GGUF_BLOB_H */\n")
    with open(header_path, "w", encoding="utf-8", newline="\n") as f:
        f.writelines(lines)
    return len(raw), n_words


def fnv1a_bytes32(data: bytes) -> int:
    h = 0x811C9DC5
    for b in data:
        h = ((h ^ b) * 0x01000193) & 0xFFFFFFFF
    return h


def sha256_stream(path: str, chunk_bytes: int = 4 * 1024 * 1024) -> tuple[int, bytes, str]:
    """Stream-hash a potentially multi-GiB file and return (size, raw_digest, hex_digest)."""
    h = hashlib.sha256()
    total = 0
    with open(path, "rb") as f:
        while True:
            buf = f.read(chunk_bytes)
            if not buf:
                break
            h.update(buf)
            total += len(buf)
    if total <= 0:
        raise ValueError(f"Empty GGUF file: {path}")
    return total, h.digest(), h.hexdigest()


def write_gguf_seal_header(gguf_path: str, header_path: str) -> tuple[int, bytes, str]:
    """Hash a GGUF file and emit its SHA-256 seal constants into embedded_gguf_seal.h."""
    sz, digest, digest_hex = sha256_stream(gguf_path)
    os.makedirs(os.path.dirname(header_path), exist_ok=True)
    hex_items = ", ".join(f"0x{b:02X}U" for b in digest)
    content = (
        f"/* Auto-generated SHA-256 PE overlay seal for {to_rel_posix(gguf_path)} */\n"
        "#ifndef ZCC_EMBEDDED_GGUF_SEAL_H\n"
        "#define ZCC_EMBEDDED_GGUF_SEAL_H\n"
        f"static const uint64_t g_sealed_gguf_len = {sz}ULL;\n"
        f"static const uint8_t g_sealed_gguf_sha256[32] = {{ {hex_items} }};\n"
        "#endif /* ZCC_EMBEDDED_GGUF_SEAL_H */\n"
    )
    with open(header_path, "w", encoding="utf-8", newline="\n") as f:
        f.write(content)
    return sz, digest, digest_hex


def append_sealed_gguf_overlay(
    exe_path: str,
    gguf_path: str,
    gguf_size: int,
    gguf_sha256: bytes,
    chunk_bytes: int = 4 * 1024 * 1024,
) -> tuple[int, int, int]:
    """Append 64B-aligned raw GGUF stream + 64-byte ZCCGGUF1 footer to a PE32+ exe."""
    pe_base_size = os.path.getsize(exe_path)
    pad_len = (-pe_base_size) % 64
    ov_off = pe_base_size + pad_len
    copied = 0
    with open(exe_path, "ab") as dst, open(gguf_path, "rb") as src:
        if pad_len:
            dst.write(b"\x00" * pad_len)
        while True:
            buf = src.read(chunk_bytes)
            if not buf:
                break
            dst.write(buf)
            copied += len(buf)
        if copied != gguf_size:
            raise IOError(f"Short GGUF overlay copy: wrote {copied}B, expected {gguf_size}B")
        prefix60 = b"ZCCGGUF1" + struct.pack("<QQ", ov_off, gguf_size) + gguf_sha256 + struct.pack("<I", 1)
        fnv = fnv1a_bytes32(prefix60)
        dst.write(prefix60 + struct.pack("<I", fnv))
    return pe_base_size, pad_len, ov_off


def compile_gguf_exe(
    model_path: str,
    output_exe: str,
    arch: str = "sm_120",
    embed_weights: bool = False,
    embed_overlay: bool = False,
    server_mode: bool = False,
    skip_win64_selfcompile: bool = False,
    timeout_s: int = 180,
    ptx_only: bool = False,
) -> dict:
    if not os.path.isfile(ZCC_BIN):
        raise RuntimeError("./zcc missing; run `make zcc` first.")
    if embed_weights and embed_overlay:
        raise ValueError("Cannot combine --embed-weights and --embed-overlay.")
    if server_mode and ptx_only:
        raise ValueError("Cannot combine --server and --ptx-only.")
    model_abs = os.path.abspath(model_path if os.path.isabs(model_path) else os.path.join(REPO_ROOT, model_path))
    if not os.path.isfile(model_abs):
        raise FileNotFoundError(f"Missing GGUF model: {model_abs}")

    out_abs = os.path.abspath(output_exe if os.path.isabs(output_exe) else os.path.join(REPO_ROOT, output_exe))
    out_dir = os.path.dirname(out_abs)
    stem = os.path.splitext(os.path.basename(out_abs))[0]
    os.makedirs(out_dir, exist_ok=True)

    workdir = os.path.join(BUILD_DIR, f"gguf_exe_{stem}")
    os.makedirs(workdir, exist_ok=True)
    if ptx_only:
        ptx_host = os.path.join(out_dir, f"{stem}_host.ptx")
        ptx_win64 = out_abs
    else:
        ptx_host = os.path.join(workdir, f"{stem}_{arch}_host.ptx")
        ptx_win64 = os.path.join(workdir, f"{stem}_{arch}.ptx")

    print(f"[GGUF-EXE:PTX-HOST] ./zcc --target=ptx --gpu-arch={arch} --model {to_rel_posix(model_abs)} -> {to_rel_posix(ptx_host)}", flush=True)
    r_host = subprocess.run(
        [ZCC_BIN, "--target=ptx", f"--gpu-arch={arch}", "--model", to_rel_posix(model_abs), "-o", to_rel_posix(ptx_host)],
        cwd=REPO_ROOT,
        capture_output=True,
        text=True,
        timeout=timeout_s,
    )
    if r_host.returncode != 0 or not os.path.isfile(ptx_host):
        raise RuntimeError(f"Host GGUF->PTX compilation failed:\n{r_host.stdout}\n{r_host.stderr}")
    sha_ptx_host = sha256_file(ptx_host)

    active_ptx = ptx_host
    if not skip_win64_selfcompile:
        if not os.path.isfile(ZCC_WIN64):
            raise RuntimeError("build_win64/zcc_sov_stage3.exe missing; run `python3 tools/win64_selfhost_sovereign.py` first.")
        print(f"[GGUF-EXE:PTX-WIN64] zcc_sov_stage3.exe --target=ptx --gpu-arch={arch} -> {to_rel_posix(ptx_win64)}", flush=True)
        r_win = run_win_cmd(
            ZCC_WIN64,
            ["--target=ptx", f"--gpu-arch={arch}", "--model", to_rel_win(model_abs), "-o", to_rel_win(ptx_win64)],
            timeout_s=timeout_s,
        )
        if r_win.returncode != 0 or not os.path.isfile(ptx_win64):
            raise RuntimeError(f"Win64 GGUF->PTX compilation failed:\n{r_win.stdout}\n{r_win.stderr}")
        sha_ptx_win = sha256_file(ptx_win64)
        fc_ptx = run_fc_b(ptx_host, ptx_win64)
        print(f"[GGUF-EXE:FC-PTX] {os.path.getsize(ptx_win64)}B SHA256={sha_ptx_win} {fc_ptx.stdout.strip()}", flush=True)
        if sha_ptx_host != sha_ptx_win or fc_ptx.returncode != 0 or "no differences encountered" not in fc_ptx.stdout.lower():
            raise AssertionError(f"GGUF AOT PTX parity mismatch: {sha_ptx_host} vs {sha_ptx_win}")
        active_ptx = ptx_win64
    else:
        with open(ptx_host, "rb") as fin, open(ptx_win64, "wb") as fout:
            fout.write(fin.read())
        active_ptx = ptx_win64

    if ptx_only:
        return {
            "ptx": active_ptx,
            "ptx_sha256": sha256_file(active_ptx),
            "ptx_bytes": os.path.getsize(active_ptx),
        }

    ptx_hdr = os.path.join(workdir, "embedded_aot_ptx.h")
    ptx_len = write_ptx_header(
        active_ptx,
        ptx_hdr,
        guard="ZCC_EMBEDDED_AOT_PTX_H",
        var_name="g_embedded_aot_ptx",
    )
    print(f"[GGUF-EXE:HDR-PTX] {ptx_hdr} ({ptx_len} bytes)", flush=True)

    extra_flags = ["-I" + to_rel_posix(workdir), "-Ibuild_win64", "-Iinclude", "-Isrc", "-I.", "-DZCC_EMBEDDED_AOT_PTX"]
    if server_mode:
        from tools.build_gpu_llm_server_win64 import UI_HTML_PATH, verify_server_ui_headless, write_server_ui_header

        ui_chk = verify_server_ui_headless(UI_HTML_PATH)
        print(f"[GGUF-EXE:UI-EF7] {ui_chk['stdout']}", flush=True)
        ui_hdr = os.path.join(workdir, "server_ui_html.h")
        ui_len = write_server_ui_header(UI_HTML_PATH, ui_hdr)
        print(f"[GGUF-EXE:HDR-UI] {ui_hdr} ({ui_len} bytes)", flush=True)

    gguf_bytes = 0
    gguf_words = 0
    gguf_sha256_bytes = b""
    gguf_sha256_hex = ""
    if embed_weights:
        gguf_hdr = os.path.join(workdir, "embedded_gguf_blob.h")
        gguf_bytes, gguf_words = write_gguf_u32_header(model_abs, gguf_hdr)
        extra_flags.append("-DZCC_EMBEDDED_GGUF")
        print(f"[GGUF-EXE:HDR-GGUF] {gguf_hdr} ({gguf_bytes} bytes, {gguf_words} uint32 words)", flush=True)
    elif embed_overlay:
        seal_hdr = os.path.join(workdir, "embedded_gguf_seal.h")
        gguf_bytes, gguf_sha256_bytes, gguf_sha256_hex = write_gguf_seal_header(model_abs, seal_hdr)
        extra_flags.append("-DZCC_SEALED_OVERLAY_GGUF")
        print(f"[GGUF-EXE:HDR-SEAL] {seal_hdr} ({gguf_bytes} bytes, sha256={gguf_sha256_hex})", flush=True)

    main_src = ("win64_gpu_llm_server.s", "src/win64_gpu_llm_server.c", extra_flags) if server_mode else (
        "win64_gpu_llm_embedded.s", "src/win64_gpu_llm.c", extra_flags
    )
    units = [
        ("win64_app_crt.s", "src/win64_app_crt.c", ["-Iinclude", "-I."]),
        ("win64_c99_rt.s", "src/win64_c99_rt.c", ["-Iinclude", "-I."]),
        main_src,
    ]

    host_exe = os.path.join(out_dir, f"{stem}_host_zcc.exe")
    effective_host = out_abs if skip_win64_selfcompile else host_exe

    print(f"[GGUF-EXE:LINK-HOST] ./zcc -mabi=ms -> {to_rel_posix(effective_host)}", flush=True)
    compile_and_link_win64_units(
        units,
        [],
        os.path.join(workdir, "host_asm"),
        effective_host,
        use_win64_compiler=False,
        timeout_s=timeout_s,
    )
    sha_exe_host = sha256_file(effective_host)

    if not skip_win64_selfcompile:
        print(f"[GGUF-EXE:LINK-WIN64] zcc_sov_stage3.exe -mabi=ms -> {to_rel_posix(out_abs)}", flush=True)
        compile_and_link_win64_units(
            units,
            [],
            os.path.join(workdir, "win64_asm"),
            out_abs,
            use_win64_compiler=True,
            timeout_s=timeout_s,
        )
        sha_exe_win = sha256_file(out_abs)
        fc_exe = run_fc_b(effective_host, out_abs)
        print(f"[GGUF-EXE:FC-EXE] {os.path.getsize(out_abs)}B SHA256={sha_exe_win} {fc_exe.stdout.strip()}", flush=True)
        if sha_exe_host != sha_exe_win or fc_exe.returncode != 0 or "no differences encountered" not in fc_exe.stdout.lower():
            raise AssertionError(f"GGUF standalone PE32+ parity mismatch: {sha_exe_host} vs {sha_exe_win}")

    req_dlls = ("ws2_32.dll", "msvcrt.dll", "nvcuda.dll") if server_mode else ("msvcrt.dll", "nvcuda.dll")
    imports = audit_pe32plus_multi_dll(
        out_abs,
        required_dlls=req_dlls,
        allowed_dlls=req_dlls,
    )
    print(f"[GGUF-EXE:PE-AUDIT] {to_rel_posix(out_abs)} imports={sorted(imports['dlls'].keys())} bytes={imports['bytes']}", flush=True)

    pe_base_bytes = os.path.getsize(out_abs)
    pe_base_sha256 = sha256_file(out_abs)
    overlay_offset = 0
    if embed_overlay:
        pe_base_sz, pad_len, overlay_offset = append_sealed_gguf_overlay(
            out_abs, model_abs, gguf_bytes, gguf_sha256_bytes
        )
        total_sz, _, total_sha = sha256_stream(out_abs)
        print(
            f"[GGUF-EXE:OVERLAY-SEAL] pe_base={pe_base_sz}B + pad={pad_len}B + gguf={gguf_bytes}B + footer=64B "
            f"-> total={total_sz}B off={overlay_offset} sha256={gguf_sha256_hex}",
            flush=True,
        )
    else:
        total_sz = pe_base_bytes
        total_sha = pe_base_sha256

    return {
        "exe": out_abs,
        "exe_sha256": total_sha,
        "exe_bytes": total_sz,
        "pe_base_bytes": pe_base_bytes,
        "pe_base_sha256": pe_base_sha256,
        "overlay_offset": overlay_offset,
        "overlay_sha256": gguf_sha256_hex,
        "ptx": active_ptx,
        "ptx_sha256": sha256_file(active_ptx),
        "ptx_bytes": ptx_len,
        "gguf_bytes": gguf_bytes,
        "gguf_words": gguf_words,
        "imports": imports,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description="Compile any GGUF model natively to a standalone Win64 PE32+ GPU executable via ZCC.")
    parser.add_argument("--model", required=True, help="Input .gguf model path")
    parser.add_argument("-o", "--output", required=True, help="Output Win64 .exe path")
    parser.add_argument("--arch", default="sm_120", help="Target GPU PTX architecture (default: sm_120)")
    parser.add_argument("--embed-weights", action="store_true", help="Embed raw GGUF weights into .rdata (models <= 3.99 MB)")
    parser.add_argument("--embed-overlay", action="store_true", help="Seal arbitrary-size / multi-GiB GGUF as a verified SHA-256 PE overlay")
    parser.add_argument("--server", action="store_true", help="Compile as standalone OpenAI-compatible HTTP/SSE server .exe")
    parser.add_argument("--ptx-only", action="store_true", help="Emit & verify dual-compiler AOT PTX only without building an .exe")
    parser.add_argument("--skip-win64-selfcompile", action="store_true", help="Skip zcc_sov_stage3.exe parity build (faster host-only iteration)")
    parser.add_argument("--timeout", type=int, default=180)
    args = parser.parse_args()

    compile_gguf_exe(
        model_path=args.model,
        output_exe=args.output,
        arch=args.arch,
        embed_weights=args.embed_weights,
        embed_overlay=args.embed_overlay,
        server_mode=args.server,
        skip_win64_selfcompile=args.skip_win64_selfcompile,
        timeout_s=args.timeout,
        ptx_only=args.ptx_only,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
