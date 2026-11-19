#!/usr/bin/env python3
"""
ZCC Native Zero-Toolchain Win64 Sovereign Self-Host Driver & Verifier
=====================================================================
Stages:
  Stage 1 (Linux/Host ./zcc, zero MSVC/MinGW/as/ld):
    6 C units -> build_win64/sov_stage1/*.s -> build_win64/zcc_sov_stage1.exe
  Stage 2 (Native Windows build_win64/zcc_sov_stage1.exe):
    6 C units -> build_win64/sov_stage2/*.s -> build_win64/zcc_sov_stage2.exe
  Stage 3 (Native Windows build_win64/zcc_sov_stage2.exe):
    6 C units -> build_win64/sov_stage3/*.s -> build_win64/zcc_sov_stage3.exe

Verifies:
  - Link-order independence (zcc.s first, bridge.s last) via Pass-2 R_X86_64_64 fix
  - Bit-exact convergence: SHA-256(stage2) == SHA-256(stage3) and Windows `fc /b`
  - Independent PE32+ header audit (.text=0x60000020 RX, .data=0xC0000040 RW,
    .pdata non-empty, .reloc non-empty, StackReserve>=16MB, TimeDateStamp==0,
    msvcrt.dll imports only)
  - Stage 3 native execution probes (>4KB multi-page stack probe, fopen/fgets/getenv,
    .data global function-pointer table)
  - EF-0 negative controls (invalid C fails nonzero; 1-byte PE mutation fails fc /b)
"""

import hashlib
import os
import shutil
import struct
import subprocess
import sys

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD_DIR = os.path.join(REPO_ROOT, "build_win64")
CMD_EXE = "/mnt/c/Windows/System32/cmd.exe"

SOVEREIGN_UNITS = [
    ("bridge.s", "src/win64_runtime_bridge.c", []),
    ("zcc.s", "zcc.c", ["-Dmain=zcc_main"]),
    ("elf_emit.s", "src/elf_emit.c", []),
    ("win64_pe_emit.s", "src/win64_pe_emit.c", []),
    ("wasm_emit.s", "src/wasm_emit.c", []),
    ("codegen.s", "src/codegen.c", ["-Isrc"]),
]

COMMON_COMPILE_FLAGS = [
    "-mabi=ms",
    "-D_WIN32",
    "-DZCC_WIN64_SOVEREIGN_LINK_ELF",
    "-Iinclude",
    "-I.",
    "-S",
]


def to_rel_win(path: str) -> str:
    rel = os.path.relpath(path, REPO_ROOT)
    return rel.replace("/", "\\")


def to_rel_posix(path: str) -> str:
    return os.path.relpath(path, REPO_ROOT).replace("\\", "/")


def run_host_zcc(args):
    zcc_bin = os.path.join(REPO_ROOT, "zcc")
    res = subprocess.run(
        [zcc_bin] + args,
        cwd=REPO_ROOT,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if res.returncode != 0:
        raise RuntimeError(
            f"Host ./zcc failed (rc={res.returncode}) on {args}:\n{res.stdout}\n{res.stderr}"
        )
    return res


def run_win_exe(exe_path: str, args, check: bool = True):
    rel_exe = to_rel_win(exe_path)
    win_args = list(args)
    if sys.platform == "win32":
        cmd = [exe_path] + win_args
    else:
        if not os.path.exists(CMD_EXE):
            raise RuntimeError(f"Windows cmd.exe not available at {CMD_EXE}")
        cmd = [CMD_EXE, "/c", rel_exe] + win_args
    res = subprocess.run(
        cmd,
        cwd=REPO_ROOT,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if check and res.returncode != 0:
        raise RuntimeError(
            f"Windows binary {rel_exe} failed (rc={res.returncode}) on {win_args}:\n{res.stdout}\n{res.stderr}"
        )
    return res


def sha256_file(path: str) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while True:
            chunk = f.read(65536)
            if not chunk:
                break
            h.update(chunk)
    return h.hexdigest()


def run_fc_b(path_a: str, path_b: str):
    rel_a = to_rel_win(path_a)
    rel_b = to_rel_win(path_b)
    if sys.platform == "win32":
        cmd = ["cmd.exe", "/c", "fc", "/b", rel_a, rel_b]
    else:
        cmd = [CMD_EXE, "/c", "fc", "/b", rel_a, rel_b]
    return subprocess.run(
        cmd,
        cwd=REPO_ROOT,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )


def build_stage(stage_name: str, compiler_path, is_windows_compiler: bool, stage_dir: str, out_exe: str):
    os.makedirs(stage_dir, exist_ok=True)
    s_files = []
    for s_name, c_src, extra in SOVEREIGN_UNITS:
        s_out = os.path.join(stage_dir, s_name)
        s_rel = to_rel_posix(s_out)
        args = list(COMMON_COMPILE_FLAGS) + list(extra) + [c_src, "-o", s_rel]
        if is_windows_compiler:
            run_win_exe(compiler_path, args, check=True)
        else:
            run_host_zcc(args)
        if not os.path.exists(s_out) or os.path.getsize(s_out) == 0:
            raise RuntimeError(f"[{stage_name}] Missing or empty assembly output: {s_out}")
        s_files.append(s_out)

    out_arg = to_rel_posix(out_exe)
    s_args = [to_rel_posix(p) for p in s_files]
    link_args = ["-target", "win64", "-mabi=ms", "--reproducible"] + s_args + ["-o", out_arg]
    if is_windows_compiler:
        run_win_exe(compiler_path, link_args, check=True)
    else:
        run_host_zcc(link_args)

    if not os.path.exists(out_exe) or os.path.getsize(out_exe) < 4096:
        raise RuntimeError(f"[{stage_name}] Failed to produce valid PE binary: {out_exe}")
    print(f"[{stage_name}] Built {to_rel_posix(out_exe)} ({os.path.getsize(out_exe)} bytes, sha256={sha256_file(out_exe)})")
    return s_files


def audit_pe_headers(exe_path: str):
    data = open(exe_path, "rb").read()
    if data[:2] != b"MZ":
        raise AssertionError(f"Missing MZ signature in {exe_path}")
    e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
    if data[e_lfanew:e_lfanew + 4] != b"PE\x00\x00":
        raise AssertionError(f"Missing PE\\0\\0 signature in {exe_path}")

    coff_off = e_lfanew + 4
    machine, num_sec, timedate, _, _, opt_sz, _ = struct.unpack_from("<HHIIIHH", data, coff_off)
    if machine != 0x8664:
        raise AssertionError(f"Expected AMD64 machine 0x8664, got 0x{machine:04x}")
    if timedate != 0:
        raise AssertionError(f"Expected TimeDateStamp == 0 for reproducible build, got {timedate}")

    opt_off = coff_off + 20
    magic = struct.unpack_from("<H", data, opt_off)[0]
    if magic != 0x020B:
        raise AssertionError(f"Expected PE32+ magic 0x020B, got 0x{magic:04x}")
    stack_reserve = struct.unpack_from("<Q", data, opt_off + 72)[0]
    if stack_reserve < 0x1000000:
        raise AssertionError(f"Expected SizeOfStackReserve >= 0x1000000, got 0x{stack_reserve:x}")

    ddir_off = opt_off + 112
    dirs = [struct.unpack_from("<II", data, ddir_off + 8 * i) for i in range(16)]
    imp_rva, imp_sz = dirs[1]
    exc_rva, exc_sz = dirs[3]
    rel_rva, rel_sz = dirs[5]
    if exc_rva == 0 or exc_sz == 0:
        raise AssertionError("EXCEPTION (.pdata) DataDirectory must be non-empty")
    if rel_rva == 0 or rel_sz == 0:
        raise AssertionError("BASERELOC (.reloc) DataDirectory must be non-empty")
    if imp_rva == 0 or imp_sz == 0:
        raise AssertionError("IMPORT DataDirectory must be non-empty")

    sec_off = opt_off + opt_sz
    sections = {}
    for i in range(num_sec):
        s_off = sec_off + i * 40
        raw_name = data[s_off:s_off + 8]
        name = raw_name.split(b"\x00", 1)[0].decode("ascii", errors="replace")
        vsize, vaddr, raw_sz, raw_ptr = struct.unpack_from("<IIII", data, s_off + 8)
        chars = struct.unpack_from("<I", data, s_off + 36)[0]
        sections[name] = {
            "vsize": vsize,
            "vaddr": vaddr,
            "raw_sz": raw_sz,
            "raw_ptr": raw_ptr,
            "chars": chars,
        }

    if ".text" not in sections or sections[".text"]["chars"] != 0x60000020:
        raise AssertionError(f"Expected .text characteristics 0x60000020 (RX), got {sections.get('.text')}")
    if ".data" not in sections or sections[".data"]["chars"] != 0xC0000040:
        raise AssertionError(f"Expected .data characteristics 0xC0000040 (RW), got {sections.get('.data')}")

    def rva_to_off(rva: int) -> int:
        for sec in sections.values():
            span = max(sec["vsize"], sec["raw_sz"])
            if sec["vaddr"] <= rva < sec["vaddr"] + span:
                off = sec["raw_ptr"] + (rva - sec["vaddr"])
                if off < sec["raw_ptr"] + sec["raw_sz"]:
                    return off
        raise AssertionError(f"Could not map RVA 0x{rva:x} to file offset")

    def read_cstr(off: int) -> str:
        end = data.index(b"\x00", off)
        return data[off:end].decode("ascii", errors="replace")

    dlls = {}
    cursor = rva_to_off(imp_rva)
    while True:
        orig_first, ts, fwd, name_rva, first_thunk = struct.unpack_from("<IIIII", data, cursor)
        if name_rva == 0 and first_thunk == 0:
            break
        dll_name = read_cstr(rva_to_off(name_rva))
        thunk_rva = orig_first if orig_first else first_thunk
        t_off = rva_to_off(thunk_rva)
        funcs = []
        while True:
            val = struct.unpack_from("<Q", data, t_off)[0]
            if val == 0:
                break
            if (val & (1 << 63)) == 0:
                hn_off = rva_to_off(val & 0xFFFFFFFF)
                funcs.append(read_cstr(hn_off + 2))
            t_off += 8
        dlls[dll_name.lower()] = funcs
        cursor += 20

    if list(dlls.keys()) != ["msvcrt.dll"]:
        raise AssertionError(f"Expected ONLY msvcrt.dll imports, got {list(dlls.keys())}: {dlls}")

    print(
        f"[AUDIT] PE32+ OK: .text=0x{sections['.text']['chars']:08x} .data=0x{sections['.data']['chars']:08x} "
        f".pdata={exc_sz}B .reloc={rel_sz}B StackReserve=0x{stack_reserve:x} TimeDateStamp=0 "
        f"Imports=msvcrt.dll({len(dlls['msvcrt.dll'])})"
    )


def run_stage3_probes(stage3_exe: str, s2_files):
    # 1. Link-order independence test: put zcc.s first, bridge.s last
    reordered_s = [s2_files[1], s2_files[2], s2_files[3], s2_files[4], s2_files[5], s2_files[0]]
    order_exe = os.path.join(BUILD_DIR, "zcc_sov_order_check.exe")
    probe_dir = os.path.join(BUILD_DIR, "sov_probes")
    os.makedirs(probe_dir, exist_ok=True)
    probe_hello_c = os.path.join(probe_dir, "hello_order.c")
    probe_hello_exe = os.path.join(probe_dir, "hello_order.exe")
    probe_stk_c = os.path.join(probe_dir, "probe_stk.c")
    probe_stk_exe = os.path.join(probe_dir, "probe_stk.exe")
    probe_io_c = os.path.join(probe_dir, "probe_io.c")
    probe_io_exe = os.path.join(probe_dir, "probe_io.exe")
    probe_io_txt = os.path.join(probe_dir, "probe_io.txt")
    neg_c = os.path.join(probe_dir, "neg_invalid.c")
    neg_exe = os.path.join(probe_dir, "neg_invalid.exe")
    mut_exe = os.path.join(probe_dir, "mutated_stage3.exe")

    try:
        link_args = (
            ["-target", "win64", "-mabi=ms", "--reproducible"]
            + [to_rel_posix(p) for p in reordered_s]
            + ["-o", to_rel_posix(order_exe)]
        )
        run_win_exe(stage3_exe, link_args, check=True)
        with open(probe_hello_c, "w", encoding="utf-8") as f:
            f.write('#include <stdio.h>\nint main(void){ printf("ORDER_OK\\n"); return 29; }\n')
        run_win_exe(order_exe, ["-target", "win64", "-mabi=ms", to_rel_posix(probe_hello_c), "-o", to_rel_posix(probe_hello_exe)], check=True)
        r_order = run_win_exe(probe_hello_exe, [], check=False)
        if r_order.returncode != 29 or "ORDER_OK" not in r_order.stdout:
            raise AssertionError(f"Link-order test failed: rc={r_order.returncode} out={r_order.stdout!r}")
        print(f"[PROBE 1] Link-order independence (zcc.s first, bridge.s last): PASS (rc={r_order.returncode})")

        # 2. >4KB multi-page stack probe + mainCRTStartup argc/argv compiled natively by stage3
        with open(probe_stk_c, "w", encoding="utf-8") as f:
            f.write(
                "#include <stdio.h>\n"
                "#include <stdlib.h>\n"
                "#include <string.h>\n"
                "extern int __getmainargs(int *argc, char ***argv, char ***envp, int dw, void *si);\n"
                "int main(int argc, char **argv);\n"
                "void mainCRTStartup(void) {\n"
                "    int argc = 0; char **argv = 0; char **envp = 0; int si[16] = {0};\n"
                "    __getmainargs(&argc, &argv, &envp, 0, si);\n"
                "    exit(main(argc, argv));\n"
                "}\n"
                "static unsigned int touch_pages(volatile char *p, int n) {\n"
                "    p[0] = 17;\n"
                "    p[4096] = 31;\n"
                "    p[8192] = 43;\n"
                "    p[n - 1] = 59;\n"
                "    return (unsigned int)(unsigned char)p[0] * 1000u\n"
                "         + (unsigned int)(unsigned char)p[4096] * 100u\n"
                "         + (unsigned int)(unsigned char)p[8192] * 10u\n"
                "         + (unsigned int)(unsigned char)p[n - 1];\n"
                "}\n"
                "int main(int argc, char **argv) {\n"
                "    volatile char big_buf[12288];\n"
                "    unsigned int sum = touch_pages(big_buf, 12288);\n"
                "    const char *a1 = (argc > 1 && argv[1]) ? argv[1] : \"?\";\n"
                "    const char *a2 = (argc > 2 && argv[2]) ? argv[2] : \"?\";\n"
                "    printf(\"STK_OK argc=%d a1=%s a2=%s sum=%u\\n\", argc, a1, a2, sum);\n"
                "    return (argc == 3 && strcmp(a1, \"arg1\") == 0 && strcmp(a2, \"arg2\") == 0 && sum == 20589u) ? 0 : 3;\n"
                "}\n"
            )
        run_win_exe(stage3_exe, ["-target", "win64", "-mabi=ms", to_rel_posix(probe_stk_c), "-o", to_rel_posix(probe_stk_exe)], check=True)
        r_stk = run_win_exe(probe_stk_exe, ["arg1", "arg2"], check=True)
        if "STK_OK argc=3 a1=arg1 a2=arg2 sum=20589" not in r_stk.stdout:
            raise AssertionError(f"Large stack probe mismatch: {r_stk.stdout!r}")
        print(f"[PROBE 2] Native >4KB multi-page stack probe + argc/argv: PASS ({r_stk.stdout.strip()})")

        # 3. fopen/fputs/fgets/fclose/getenv + .data function-pointer table compiled natively by stage3
        io_rel = to_rel_posix(probe_io_txt)
        with open(probe_io_c, "w", encoding="utf-8") as f:
            f.write(
                "#include <stdio.h>\n"
                "#include <stdlib.h>\n"
                "static int op_add(int a, int b) { return a + b; }\n"
                "static int op_mul(int a, int b) { return a * b; }\n"
                "static int (*g_ops[2])(int, int) = { op_add, op_mul };\n"
                "int main(void) {\n"
                f"    FILE *fw = fopen(\"{io_rel}\", \"w\");\n"
                "    if (!fw) return 11;\n"
                "    fputs(\"WIN64_STAGE3_LINE1\\nWIN64_STAGE3_LINE2\\n\", fw);\n"
                "    fclose(fw);\n"
                f"    FILE *fr = fopen(\"{io_rel}\", \"r\");\n"
                "    if (!fr) return 12;\n"
                "    char line[128];\n"
                "    if (!fgets(line, sizeof(line), fr)) { fclose(fr); return 13; }\n"
                "    fclose(fr);\n"
                "    const char *env_val = getenv(\"SystemRoot\");\n"
                "    if (!env_val) env_val = getenv(\"PATH\");\n"
                "    if (!env_val || !env_val[0]) return 14;\n"
                "    int ops_res = g_ops[0](10, 5) + g_ops[1](4, 8);\n"
                "    printf(\"IO_OK line=%sops=%d\\n\", line, ops_res);\n"
                "    return (ops_res == 47) ? 0 : 15;\n"
                "}\n"
            )
        run_win_exe(stage3_exe, ["-target", "win64", "-mabi=ms", to_rel_posix(probe_io_c), "-o", to_rel_posix(probe_io_exe)], check=True)
        r_io = run_win_exe(probe_io_exe, [], check=True)
        if "WIN64_STAGE3_LINE1" not in r_io.stdout or "ops=47" not in r_io.stdout:
            raise AssertionError(f"Stdio/funcptr probe mismatch: {r_io.stdout!r}")
        print(f"[PROBE 3] Native stdio/fgets/getenv + .data funcptr table: PASS (ops=47)")

        # 4. EF-0 Negative control A: invalid C must fail nonzero and emit no PE
        with open(neg_c, "w", encoding="utf-8") as f:
            f.write("int main( { return ; }\n")
        if os.path.exists(neg_exe):
            os.remove(neg_exe)
        r_neg = run_win_exe(stage3_exe, ["-target", "win64", "-mabi=ms", to_rel_posix(neg_c), "-o", to_rel_posix(neg_exe)], check=False)
        if r_neg.returncode == 0:
            raise AssertionError("EF-0 NEGATIVE CONTROL FAILED: invalid C returned 0")
        if os.path.exists(neg_exe):
            raise AssertionError("EF-0 NEGATIVE CONTROL FAILED: output exe created on syntax error")
        print(f"[EF-0 NEGATIVE 1] Invalid C rejected by stage3 (rc={r_neg.returncode}): PASS")

        # 5. EF-0 Negative control B: 1-byte mutation must break SHA-256 and fc /b
        raw = bytearray(open(stage3_exe, "rb").read())
        raw[len(raw) // 2] ^= 0xFF
        with open(mut_exe, "wb") as f:
            f.write(raw)
        if sha256_file(mut_exe) == sha256_file(stage3_exe):
            raise AssertionError("EF-0 NEGATIVE CONTROL FAILED: 1-byte mutation kept same SHA-256")
        fc_mut = run_fc_b(stage3_exe, mut_exe)
        if fc_mut.returncode == 0:
            raise AssertionError("EF-0 NEGATIVE CONTROL FAILED: fc /b missed 1-byte mutation")
        print(f"[EF-0 NEGATIVE 2] 1-byte PE mutation caught by SHA-256 & fc /b (rc={fc_mut.returncode}): PASS")
    finally:
        shutil.rmtree(probe_dir, ignore_errors=True)
        if os.path.exists(order_exe):
            os.remove(order_exe)


def main():
    os.makedirs(BUILD_DIR, exist_ok=True)
    if not os.path.exists(os.path.join(REPO_ROOT, "zcc")):
        raise RuntimeError("./zcc not found — build host zcc first (`make zcc`).")

    s1_dir = os.path.join(BUILD_DIR, "sov_stage1")
    s2_dir = os.path.join(BUILD_DIR, "sov_stage2")
    s3_dir = os.path.join(BUILD_DIR, "sov_stage3")
    s1_exe = os.path.join(BUILD_DIR, "zcc_sov_stage1.exe")
    s2_exe = os.path.join(BUILD_DIR, "zcc_sov_stage2.exe")
    s3_exe = os.path.join(BUILD_DIR, "zcc_sov_stage3.exe")

    print("=== ZCC Sovereign Zero-Toolchain Win64 Self-Host ===")
    build_stage("STAGE 1", None, False, s1_dir, s1_exe)
    s2_files = build_stage("STAGE 2", s1_exe, True, s2_dir, s2_exe)
    build_stage("STAGE 3", s2_exe, True, s3_dir, s3_exe)

    h1 = sha256_file(s1_exe)
    h2 = sha256_file(s2_exe)
    h3 = sha256_file(s3_exe)
    print(f"SHA256 stage1: {h1}")
    print(f"SHA256 stage2: {h2}")
    print(f"SHA256 stage3: {h3}")
    if h2 != h3:
        raise AssertionError(f"Win64 Self-Host Convergence FAILED: stage2={h2} != stage3={h3}")
    print(f"[CONVERGENCE] stage1==stage2: {h1 == h2}, stage2==stage3: True")

    fc_res = run_fc_b(s2_exe, s3_exe)
    print(fc_res.stdout.strip())
    if fc_res.returncode != 0 or "no differences encountered" not in fc_res.stdout.lower():
        raise AssertionError(f"Windows fc /b failed (rc={fc_res.returncode}): {fc_res.stdout}\n{fc_res.stderr}")

    audit_pe_headers(s3_exe)
    run_stage3_probes(s3_exe, s2_files)
    print("=== [PASS] WIN64 SOVEREIGN ZERO-TOOLCHAIN SELF-HOST VERIFIED ===")


if __name__ == "__main__":
    main()
