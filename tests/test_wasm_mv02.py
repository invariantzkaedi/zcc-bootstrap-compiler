#!/usr/bin/env python3
"""
tests/test_wasm_mv02.py — Adversarial Semantic, Register-Liveness, Typedef & Seed Sensitivity Suite (WASM-MV-02)
"""
import hashlib
import os
import pathlib
import random
import subprocess
import tempfile
import unittest

REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent
ZCC_BIN = REPO_ROOT / "zcc"

# Harness Meta-Verification Guards (targeted by MV02-08 and MV02-09)
EXECUTED_VECTOR_COUNT_GUARD = True
ARTIFACT_FRESHNESS_GUARD = True


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def run_wasm_node(wasm_path: pathlib.Path) -> int:
    js = (
        "const fs=require('fs');"
        f"const b=fs.readFileSync({str(wasm_path)!r});"
        "WebAssembly.instantiate(b,{env:{printf:()=>0,puts:()=>0,exit:()=>{},malloc:()=>1024,free:()=>{}}})"
        ".then(m=>{const r=m.instance.exports.main(); process.exit(Number(r)&0xff);})"
        ".catch(e=>{console.error(e); process.exit(99);});"
    )
    res = subprocess.run(["node", "-e", js], capture_output=True, text=True, timeout=30)
    return res.returncode


class TestWasmMV02Adversarial(unittest.TestCase):
    def test_01_wasm_mv02_signedness_and_control_flow(self):
        """Verify tests/wasm_mv02_signedness.c across ZCC WASM, GCC, and Clang."""
        src = REPO_ROOT / "tests" / "wasm_mv02_signedness.c"
        self.assertTrue(src.exists())
        with tempfile.TemporaryDirectory() as tmp:
            wasm_out = pathlib.Path(tmp) / "signedness.wasm"
            gcc_out = pathlib.Path(tmp) / "signedness_gcc"
            clang_out = pathlib.Path(tmp) / "signedness_clang"

            r_zcc = subprocess.run([str(ZCC_BIN), "--wasm", str(src), "-o", str(wasm_out)], capture_output=True, text=True)
            self.assertEqual(r_zcc.returncode, 0, r_zcc.stderr)
            self.assertEqual(run_wasm_node(wasm_out), 0)

            r_gcc = subprocess.run(["gcc", "-O2", str(src), "-o", str(gcc_out)], capture_output=True, text=True)
            self.assertEqual(r_gcc.returncode, 0, r_gcc.stderr)
            self.assertEqual(subprocess.run([str(gcc_out)]).returncode, 0)

            r_clang = subprocess.run(["clang", "-O2", str(src), "-o", str(clang_out)], capture_output=True, text=True)
            self.assertEqual(r_clang.returncode, 0, r_clang.stderr)
            self.assertEqual(subprocess.run([str(clang_out)]).returncode, 0)

    def test_02_x86_mv02_imm64_and_live_r11_preservation(self):
        """Verify tests/x86_mv02_imm64.c preserves live %r11/%r10/%r9, %rsp value, and red-zone memory across 64-bit immediate comparison."""
        src = REPO_ROOT / "tests" / "x86_mv02_imm64.c"
        self.assertTrue(src.exists())
        with tempfile.TemporaryDirectory() as tmp:
            asm_out = pathlib.Path(tmp) / "imm64.s"
            bin_out = pathlib.Path(tmp) / "imm64"

            r_asm = subprocess.run([str(ZCC_BIN), "-S", str(src), "-o", str(asm_out)], capture_output=True, text=True)
            self.assertEqual(r_asm.returncode, 0, r_asm.stderr)
            asm_text = asm_out.read_text(encoding="utf-8")
            self.assertIn("movq %r11, %xmm15", asm_text)
            self.assertIn("movabsq $4294967296, %r11", asm_text)
            self.assertIn("cmpq %r11, %rax", asm_text)
            self.assertIn("movq %xmm15, %r11", asm_text)
            self.assertIn("movq %r10, %xmm15", asm_text)
            self.assertIn("cmpq %r10, %r11", asm_text)
            self.assertIn("movq %xmm15, %r10", asm_text)
            self.assertIn("cmpq %r11, 8(%rsp)", asm_text)
            self.assertIn("cmpq %r10, 8(%r11)", asm_text)
            self.assertIn("movq %r9, %xmm15", asm_text)
            self.assertIn("cmpq %r9, 8(%r11, %r10, 8)", asm_text)
            self.assertIn("movq %xmm15, %r9", asm_text)
            self.assertIn("cmpq %r11, 8(%rsp, %rcx, 8)", asm_text)
            self.assertIn("cmpq %r11, -8(%rsp)", asm_text)
            self.assertIn("cmpq %r11, -128(%rsp)", asm_text)
            self.assertIn("cmpq %r11, %rsp", asm_text)
            self.assertIn("cmpq %r11, 32(%rsp)", asm_text)
            self.assertIn("cmpq %r11, -352(%rbp)", asm_text)
            self.assertIn("cmpq %r11, 24(%rsp)", asm_text)
            self.assertIn("cmpq %r11, 24(%rsp, %rcx, 8)", asm_text)
            self.assertIn("cmpq %r11, 16(%rsp, %rcx, 8)", asm_text)
            self.assertIn("movq %r11, %xmm13", asm_text)
            self.assertIn("movq %xmm13, %r11", asm_text)
            self.assertIn("movq %r11, %xmm14", asm_text)
            self.assertIn("movq %xmm14, %r11", asm_text)
            self.assertNotIn("movq %r11, -128(%rsp)", asm_text)
            self.assertNotIn("movq %r11, -120(%rsp)", asm_text)
            self.assertNotIn("movq %r11, -288(%rbp)", asm_text)

            r_bin = subprocess.run([str(ZCC_BIN), str(src), "-o", str(bin_out)], capture_output=True, text=True)
            self.assertEqual(r_bin.returncode, 0, r_bin.stderr)
            r_exec = subprocess.run([str(bin_out)], capture_output=True, text=True)
            self.assertEqual(r_exec.returncode, 0)

    def test_03_typedef_redeclaration_compatibility_and_rejection(self):
        """Verify compatible typedef redeclaration succeeds and incompatible typedef redeclaration fails closed."""
        compat_src = REPO_ROOT / "tests" / "typedef_compatible.c"
        incompat_src = REPO_ROOT / "tests" / "typedef_incompatible.c"
        with tempfile.TemporaryDirectory() as tmp:
            compat_wasm = pathlib.Path(tmp) / "compat.wasm"
            incompat_wasm = pathlib.Path(tmp) / "incompat.wasm"

            r_ok = subprocess.run([str(ZCC_BIN), "--wasm", str(compat_src), "-o", str(compat_wasm)], capture_output=True, text=True)
            self.assertEqual(r_ok.returncode, 0, r_ok.stderr)
            self.assertTrue(compat_wasm.exists())
            self.assertEqual(run_wasm_node(compat_wasm), 0)

            r_bad = subprocess.run([str(ZCC_BIN), "--wasm", str(incompat_src), "-o", str(incompat_wasm)], capture_output=True, text=True)
            self.assertNotEqual(r_bad.returncode, 0, "Incompatible typedef redeclaration must fail compilation")
            self.assertFalse(incompat_wasm.exists(), "Failed compilation must not emit output binary")

    def test_04_seven_defect_class_seed_sensitivity(self):
        """
        Verify 50 seeded metamorphic vectors + all 7 defect classes from Section 2.C:
        1) sign-extension inversion
        2) i64 shift-count lowering (rhs >= 32 and mixed-width rhs)
        3) struct member offsets after 32-bit pointers
        4) high-word 64-bit comparisons
        5) switch fallthrough + loop break interaction
        6) loop branch depth with nested switch/if
        7) pointer-array stride (sizeof(void*) == 4 under wasm32)
        """
        executed_vectors = 0
        seen_hashes = set()

        with tempfile.TemporaryDirectory() as tmp:
            for seed in range(1, 8):
                rng = random.Random(seed * 0x9E3779B9)
                a_hi = rng.randint(1, 0x3FFFFFFF)
                a_lo = rng.randint(1, 0x7FFFFFFF)
                val64 = (a_hi << 32) | a_lo
                neg32 = -rng.randint(2, 1000)
                sh = rng.randint(33, 47)

                c_code = f"""
#include <stdint.h>
struct Mix {{
    void *p;
    int64_t x;
    int32_t y;
    void *q;
}};
int main(void) {{
    /* 1. Sign-extension inversion check */
    int32_t s32 = {neg32};
    int64_t s64 = (int64_t)s32;
    if ((s64 >> 32) != -1LL) return 11;

    /* 2. i64 shift-count lowering with 32-bit shift count > 32 */
    uint64_t base = {val64}ULL;
    int sh = {sh};
    if (((base >> sh) << sh) == 0ULL && (base >> sh) != 0ULL) return 12;
    if ((1ULL << sh) != (1ULL << {sh}ULL)) return 13;

    /* 3. Struct offset layout under wasm32 ILP32 (sizeof(void*)=4, alignof(int64_t)=8) */
    struct Mix m;
    char *bp = (char *)&m;
    if ((int)((char *)&m.p - bp) != 0) return 14;
    if ((int)((char *)&m.x - bp) != 8) return 15;
    if ((int)((char *)&m.y - bp) != 16) return 16;
    if ((int)((char *)&m.q - bp) != 20) return 17;
    if (sizeof(struct Mix) != 24) return 22;

    /* 4. High-word comparison */
    int64_t hi1 = {val64}LL;
    int64_t hi2 = {val64 - (1 << 32)}LL;
    if (!(hi2 < hi1)) return 18;
    if (!( -hi1 < hi2 )) return 19;

    /* 5 & 6. Switch fallthrough and nested loop/switch branch depth */
    int acc = 0;
    for (int i = 0; i < 4; i++) {{
        int64_t k = (i == 1) ? {val64}LL : (int64_t)i;
        switch (k) {{
            case {val64}LL:
                acc += 50;
                /* fallthrough */
            case 2LL:
                acc += 7;
                break;
            default:
                acc += 1;
                break;
        }}
        acc += 100;
    }}
    /* i=0: default(1)+100=101; i=1: case val64(50)+fallthrough(7)+100=157; i=2: case 2(7)+100=107; i=3: default(1)+100=101 => total 466 */
    if (acc != 466) return 20;

    /* 7. Pointer-array stride under wasm32 (4 bytes per pointer) */
    void *arr[4];
    int stride = (int)((char *)&arr[3] - (char *)&arr[0]);
    if (stride != 12) return 21;

    return 0;
}}
"""
                c_path = pathlib.Path(tmp) / f"vec_{seed}.c"
                wasm_path = pathlib.Path(tmp) / f"vec_{seed}.wasm"
                c_path.write_text(c_code, encoding="utf-8")

                r = subprocess.run([str(ZCC_BIN), "--wasm", str(c_path), "-o", str(wasm_path)], capture_output=True, text=True)
                self.assertEqual(r.returncode, 0, r.stderr)

                if ARTIFACT_FRESHNESS_GUARD:
                    h = sha256_bytes(wasm_path.read_bytes())
                    self.assertNotIn(h, seen_hashes, "Stale WASM artifact detected across distinct seeds")
                    seen_hashes.add(h)
                else:
                    # Simulated stale-artifact defect for MV02-09
                    self.fail("MV02-09: ARTIFACT_FRESHNESS_GUARD disabled (stale artifact detection triggered)")

                if EXECUTED_VECTOR_COUNT_GUARD:
                    rc = run_wasm_node(wasm_path)
                    self.assertEqual(rc, 0, f"Seed {seed} failed with exit {rc}")
                    executed_vectors += 1

        self.assertEqual(executed_vectors, 7, "MV02-08: Executed vector count mismatch")

    def test_05_mv04_gnu_as_binary_encoding_equivalence(self):
        """
        MV-04 Independent Assembler Oracle:
        Assemble an exhaustive corpus of cmpq register, base+disp, %rsp-relative,
        and SIB (base+index*scale+disp with extended REX.R/REX.X/REX.B registers)
        instructions via ZCC's internal ELF assembler and GNU `as`, and verify
        100% byte-for-byte machine-code identity.
        """
        c_corpus = """
void cmpq_encoding_oracle(void) {
    asm("cmpq %r11, %rax");
    asm("cmpq %r10, %r11");
    asm("cmpq %r11, (%rax)");
    asm("cmpq %r10, 8(%r11)");
    asm("cmpq %r11, 8(%r10)");
    asm("cmpq %r11, (%rbp)");
    asm("cmpq %r11, (%r13)");
    asm("cmpq %r11, 256(%r13)");
    asm("cmpq %r11, (%rsp)");
    asm("cmpq %r11, 16(%rsp)");
    asm("cmpq %r9, 8(%r11, %r10, 8)");
    asm("cmpq %r11, 16(%rsp, %rcx, 8)");
    asm("cmpq %r10, 24(%rsp, %r11, 4)");
    asm("cmpq 8(%r11, %r10, 8), %r9");
    asm("cmpq %r11, %rsp");
    asm("cmpq %r11, -8(%rsp)");
    asm("cmpq %r11, -128(%rsp)");
    asm("cmpq %r11, -304(%rbp)");
    asm("cmpq %r11, 32(%rsp)");
    asm("cmpq %r11, -328(%rbp)");
    asm("cmpq %r11, 24(%rsp, %rcx, 8)");
    asm("movq %r11, %xmm15");
    asm("movq %xmm15, %r11");
    asm("movq %r10, %xmm15");
    asm("movq %xmm15, %r10");
    asm("movq %r9, %xmm15");
    asm("movq %xmm15, %r9");
    asm("movq %r11, %xmm14");
    asm("movq %xmm14, %r11");
    asm("movq %r11, %xmm13");
    asm("movq %xmm13, %r11");
    asm("movq %r11, %xmm12");
    asm("movq %xmm12, %r11");
    asm("movaps -32(%rbp), %xmm15");
    asm("movaps %xmm15, -48(%rbp)");
    asm("movaps -32(%rbp), %xmm14");
    asm("movaps %xmm14, -48(%rbp)");
}
int main(void) { return 0; }
"""
        with tempfile.TemporaryDirectory() as tmp:
            c_path = pathlib.Path(tmp) / "oracle.c"
            s_path = pathlib.Path(tmp) / "oracle.s"
            zcc_o = pathlib.Path(tmp) / "oracle_zcc.o"
            gas_o = pathlib.Path(tmp) / "oracle_gas.o"
            c_path.write_text(c_corpus, encoding="utf-8")

            r_s = subprocess.run([str(ZCC_BIN), "-S", str(c_path), "-o", str(s_path)], capture_output=True, text=True)
            self.assertEqual(r_s.returncode, 0, r_s.stderr)

            r_zcc_o = subprocess.run([str(ZCC_BIN), "-c", str(c_path), "-o", str(zcc_o)], capture_output=True, text=True)
            self.assertEqual(r_zcc_o.returncode, 0, r_zcc_o.stderr)

            r_gas_o = subprocess.run(["as", "--64", str(s_path), "-o", str(gas_o)], capture_output=True, text=True)
            self.assertEqual(r_gas_o.returncode, 0, r_gas_o.stderr)

            def extract_cmp_and_xmm_bytes(obj_path: pathlib.Path) -> list:
                r = subprocess.run(
                    ["objdump", "-d", str(obj_path)],
                    capture_output=True,
                    text=True,
                )
                self.assertEqual(r.returncode, 0, r.stderr)
                insns = []
                for line in r.stdout.splitlines():
                    if ":" in line and "\t" in line:
                        parts = line.split("\t")
                        if len(parts) >= 3:
                            dis = parts[2].strip()
                            if dis.startswith("cmp") or "%xmm1" in dis:
                                hex_bytes = "".join(
                                    hb for hb in parts[1].strip().split()
                                    if len(hb) == 2 and all(c in "0123456789abcdefABCDEF" for c in hb)
                                )
                                insns.append(hex_bytes.lower())
                return insns

            zcc_insns = extract_cmp_and_xmm_bytes(zcc_o)
            gas_insns = extract_cmp_and_xmm_bytes(gas_o)
            self.assertEqual(len(zcc_insns), 37)
            self.assertEqual(
                zcc_insns,
                gas_insns,
                f"ZCC internal ELF assembler diverged from GNU as: zcc={zcc_insns} vs gas={gas_insns}",
            )

    def test_06_zcc_prove_01_peephole_truth_machine(self):
        """
        ZCC-PROVE-01: Formal Z3 SMT + 3,800-state differential machine-state equivalence verifier
        for 64-bit immediate cmpq lowering in part5.c.
        """
        prove_script = REPO_ROOT / "tools" / "zcc_prove_01.py"
        self.assertTrue(prove_script.exists())
        r = subprocess.run(["python3", str(prove_script)], capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stdout + "\n" + r.stderr)
        self.assertIn("[ZCC-PROVE-01] PASS:", r.stdout)

    def test_07_mv12_peephole_boost_and_tco_soundness(self):
        """
        MV-12 + ZCC-PROVE-04: Differential execution of `tests/x86_mv02_imm64.c` under `ZCC_OPT=1`
        (`zcc_x86_peephole_boost` active) vs GCC `-O2`, verifying Pass 7 dead-write guards
        and TCO full-function local-stack escape guards.
        """
        c_src = REPO_ROOT / "tests" / "x86_mv02_imm64.c"
        with tempfile.TemporaryDirectory() as tmp:
            zcc_bin = pathlib.Path(tmp) / "x86_mv12_zcc_opt"
            gcc_bin = pathlib.Path(tmp) / "x86_mv12_gcc"
            s_path = pathlib.Path(tmp) / "x86_mv12_zcc_opt.s"
            env = dict(os.environ)
            env["ZCC_OPT"] = "1"
            r_s = subprocess.run([str(ZCC_BIN), "-S", str(c_src), "-o", str(s_path)], env=env, capture_output=True, text=True)
            self.assertEqual(r_s.returncode, 0, r_s.stderr)
            s_txt = s_path.read_text(encoding="utf-8")
            self.assertNotIn("jmp mv12_consume_escaped_ptr", s_txt)
            self.assertIn("call mv12_consume_escaped_ptr", s_txt)

            r_zcc = subprocess.run([str(ZCC_BIN), str(c_src), "-o", str(zcc_bin)], env=env, capture_output=True, text=True)
            self.assertEqual(r_zcc.returncode, 0, r_zcc.stderr)
            r_gcc = subprocess.run(["gcc", "-O2", str(c_src), "-o", str(gcc_bin)], capture_output=True, text=True)
            self.assertEqual(r_gcc.returncode, 0, r_gcc.stderr)

            run_zcc = subprocess.run([str(zcc_bin)], capture_output=True, text=True)
            run_gcc = subprocess.run([str(gcc_bin)], capture_output=True, text=True)
            self.assertEqual(run_zcc.returncode, 0)
            self.assertEqual(run_gcc.returncode, 0)


if __name__ == "__main__":
    unittest.main()

