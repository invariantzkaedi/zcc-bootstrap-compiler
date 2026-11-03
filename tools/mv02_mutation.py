#!/usr/bin/env python3
"""
tools/mv02_mutation.py — Controlled Adversarial Semantic & Harness Mutation Verifier (WASM-MV-02)
Supports both single-mutation CLI invocation (--repo/--file/--old/--new/--build/--test)
and full 9-mutation + 7-seed-defect-class ledger execution (--run-ledger).
"""
import argparse
import hashlib
import json
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile


def sha256(path: pathlib.Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def run(cmd: str, cwd: pathlib.Path, timeout: int = 120):
    return subprocess.run(
        cmd,
        cwd=str(cwd),
        shell=True,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        timeout=timeout,
    )


def copy_repo_fast(src: pathlib.Path, dst: pathlib.Path):
    """Copy compiler source and tests needed for isolated mutation build & test."""
    dst.mkdir(parents=True, exist_ok=True)
    for item in src.iterdir():
        name = item.name
        if name == ".git":
            continue
        if item.is_file() and (
            name.endswith((".c", ".h", ".inc", ".py", ".sh", ".md"))
            or name in ("Makefile", "zcc")
        ):
            for attempt in range(5):
                try:
                    data = item.read_bytes()
                    if data or item.stat().st_size == 0:
                        (dst / name).write_bytes(data)
                        shutil.copymode(item, dst / name)
                        break
                except OSError:
                    pass
        elif name in ("src", "tests") and item.is_dir():
            shutil.copytree(item, dst / name, dirs_exist_ok=True)
        elif name == "tools" and item.is_dir():
            (dst / "tools").mkdir(parents=True, exist_ok=True)
            for tf in item.glob("*.py"):
                (dst / "tools" / tf.name).write_bytes(tf.read_bytes())
        elif item.is_dir() and not (dst / name).exists():
            os.symlink(item, dst / name)


_BASELINE_TEST_CACHE = {}
_STAGE_DIR = None


def get_stage_dir(repo: pathlib.Path) -> pathlib.Path:
    global _STAGE_DIR
    if _STAGE_DIR is None:
        _STAGE_DIR = pathlib.Path(tempfile.mkdtemp(prefix="zcc_mv02_stage_"))
        copy_repo_fast(repo, _STAGE_DIR)
    return _STAGE_DIR


def run_single_mutation(
    repo: pathlib.Path,
    rel_file: str,
    old_str: str,
    new_str: str,
    build_cmd: str,
    test_cmd: str,
    mutation_id: str = "CUSTOM",
) -> dict:
    stage = get_stage_dir(repo)
    with tempfile.TemporaryDirectory(prefix=f"zcc_mv02_{mutation_id}_") as tmp:
        work = pathlib.Path(tmp) / "repo"
        shutil.copytree(stage, work, symlinks=True)
        target = work / rel_file
        original = target.read_text(encoding="utf-8")
        if old_str not in original:
            return {
                "mutation_id": mutation_id,
                "status": "ERROR_ANCHOR_NOT_FOUND",
                "file": rel_file,
                "killed": False,
            }

        # 1. Baseline build & test (reuse repo baseline binary if already built)
        zcc_bin = work / "zcc"
        if not zcc_bin.exists():
            base_build = run(build_cmd, work)
            if base_build.returncode != 0:
                return {
                    "mutation_id": mutation_id,
                    "status": "ERROR_BASELINE_BUILD_FAILED",
                    "output": base_build.stdout[-800:],
                    "killed": False,
                }
        base_bin_hash = sha256(zcc_bin) if zcc_bin.exists() else "N/A"
        cache_key = (base_bin_hash, test_cmd)
        if cache_key not in _BASELINE_TEST_CACHE:
            _BASELINE_TEST_CACHE[cache_key] = run(test_cmd, work)
        base_test = _BASELINE_TEST_CACHE[cache_key]
        if base_test.returncode != 0:
            return {
                "mutation_id": mutation_id,
                "status": "ERROR_BASELINE_TEST_FAILED",
                "baseline_exit": base_test.returncode,
                "output": base_test.stdout[-800:],
                "killed": False,
            }

        # 2. Apply single mutation
        mutated = original.replace(old_str, new_str, 1)
        target.write_text(mutated, encoding="utf-8")
        mut_src_hash = sha256(target)
        if not rel_file.startswith(("tests/", "tools/")):
            if (work / "zcc.c").exists():
                (work / "zcc.c").unlink()
            if zcc_bin.exists():
                zcc_bin.unlink()

        mut_build = run(build_cmd, work)
        if mut_build.returncode != 0:
            return {
                "mutation_id": mutation_id,
                "status": "ERROR_MUTATION_BUILD_FAILED",
                "mut_build_exit": mut_build.returncode,
                "output": mut_build.stdout[-800:],
                "killed": False,
            }
        mut_bin_hash = sha256(zcc_bin) if zcc_bin.exists() else "N/A"
        mut_test = run(test_cmd, work)

        killed = mut_test.returncode != 0 and (
            base_bin_hash != mut_bin_hash or rel_file.startswith(("tests/", "tools/"))
        )
        return {
            "mutation_id": mutation_id,
            "file": rel_file,
            "mutated_source_sha256": mut_src_hash,
            "baseline_binary_sha256": base_bin_hash,
            "mutated_binary_sha256": mut_bin_hash,
            "baseline_exit": base_test.returncode,
            "mutated_build_exit": mut_build.returncode,
            "mutated_test_exit": mut_test.returncode,
            "killed": killed,
            "status": "KILLED" if killed else "SURVIVED",
            "counterexample_output": mut_test.stdout.strip()[-600:],
        }


WASM_EXEC_CMD = (
    "./zcc --wasm tests/wasm_mv02_signedness.c -o /tmp/mv02_mut.wasm && "
    "node -e 'const fs=require(\"fs\"); "
    "const b=fs.readFileSync(\"/tmp/mv02_mut.wasm\"); "
    "WebAssembly.instantiate(b,{env:{printf:()=>0,puts:()=>0,exit:()=>{},malloc:()=>1024,free:()=>{}}})"
    ".then(m=>{const r=m.instance.exports.main(); console.log(\"EXIT=\"+r); process.exit(r);})"
    ".catch(e=>{console.error(e.message); process.exit(99);});'"
)

LEDGER_MUTATIONS = [
    {
        "id": "MV02-01",
        "desc": "Signed comparison i64.lt_s -> i64.lt_u on INT64_MIN < 1",
        "file": "part6_wasm.c",
        "old": "else if (common_vt == WASM_TYPE_I64) wasm_buf_append_u8(&fn->body, is_uns ? WASM_OP_I64_LT_U : WASM_OP_I64_LT_S);",
        "new": "else if (common_vt == WASM_TYPE_I64) wasm_buf_append_u8(&fn->body, WASM_OP_I64_LT_U); /* MV02-01 */",
        "build": "make zcc",
        "test": WASM_EXEC_CMD,
    },
    {
        "id": "MV02-02",
        "desc": "64-bit load i64.load -> i32.load upper-32-bit corruption / validation error",
        "file": "part6_wasm.c",
        "old": "    if (wasm_valtype_from_ctype(type) == WASM_TYPE_I64) {\n        wasm_buf_append_u8(buf, WASM_OP_I64_LOAD);",
        "new": "    if (wasm_valtype_from_ctype(type) == WASM_TYPE_I64) {\n        wasm_buf_append_u8(buf, WASM_OP_I32_LOAD); /* MV02-02 */",
        "build": "make zcc",
        "test": WASM_EXEC_CMD,
    },
    {
        "id": "MV02-03",
        "desc": "Signed widening i64.extend_i32_s -> i64.extend_i32_u on negative i32 (-5)",
        "file": "part6_wasm.c",
        "old": "    if (src_vt == WASM_TYPE_I32 && dst_vt == WASM_TYPE_I64) {\n        wasm_buf_append_u8(buf, is_unsigned ? WASM_OP_I64_EXTEND_I32_U : WASM_OP_I64_EXTEND_I32_S);",
        "new": "    if (src_vt == WASM_TYPE_I32 && dst_vt == WASM_TYPE_I64) {\n        wasm_buf_append_u8(buf, WASM_OP_I64_EXTEND_I32_U); /* MV02-03 */",
        "build": "make zcc",
        "test": WASM_EXEC_CMD,
    },
    {
        "id": "MV02-04",
        "desc": "Incorrect 64-bit switch case constant (cval + 1)",
        "file": "part6_wasm.c",
        "old": "                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_CONST);\n                    wasm_buf_append_sleb128_i64(&fn->body, cval);",
        "new": "                    wasm_buf_append_u8(&fn->body, WASM_OP_I64_CONST);\n                    wasm_buf_append_sleb128_i64(&fn->body, cval + 1); /* MV02-04 */",
        "build": "make zcc",
        "test": WASM_EXEC_CMD,
    },
    {
        "id": "MV02-05",
        "desc": "Incorrect WASM branch depth inside loop/switch",
        "file": "part6_wasm.c",
        "old": "        case ND_BREAK:\n            if (wasm_cur_break_depth >= 0 && wasm_stmt_block_depth > wasm_cur_break_depth) {\n                wasm_buf_append_u8(&fn->body, WASM_OP_BR);\n                wasm_buf_append_uleb128(&fn->body, (uint32_t)(wasm_stmt_block_depth - 1 - wasm_cur_break_depth));",
        "new": "        case ND_BREAK:\n            if (wasm_cur_break_depth >= 0 && wasm_stmt_block_depth > wasm_cur_break_depth) {\n                wasm_buf_append_u8(&fn->body, WASM_OP_BR);\n                wasm_buf_append_uleb128(&fn->body, (uint32_t)(wasm_stmt_block_depth - wasm_cur_break_depth)); /* MV02-05 */",
        "build": "make zcc",
        "test": WASM_EXEC_CMD,
    },
    {
        "id": "MV02-06",
        "desc": "Native x86-64 %r11 overwritten while live across 64-bit immediate comparison",
        "file": "part5.c",
        "old": 'sprintf(line_ptrs[i], "    movq %%%s, %%%s\\n    movabsq $%lld, %%%s\\n    cmpq %%%s, %s\\n    movq %%%s, %%%s\\n", sc, xsc, cval, sc, sc, reg, xsc, sc);',
        "new": 'sprintf(line_ptrs[i], "    movabsq $%lld, %%r11\\n    cmpq %%r11, %s\\n", cval, reg); /* MV02-06 */',
        "build": "make zcc",
        "test": "./zcc tests/x86_mv02_imm64.c -o /tmp/x86_mv02_mut && /tmp/x86_mv02_mut",
    },
    {
        "id": "MV02-07",
        "desc": "Incompatible typedef redeclaration silently accepted instead of rejected",
        "file": "part2.c",
        "old": "                    if (!typedef_types_compatible(existing->type, type)) {",
        "new": "                    if (0 && !typedef_types_compatible(existing->type, type)) { /* MV02-07 */",
        "build": "make zcc",
        "test": "if ./zcc --wasm tests/typedef_incompatible.c -o /tmp/td_inc.wasm 2>/dev/null; then echo 'UNEXPECTED_SUCCESS'; exit 1; else exit 0; fi",
    },
    {
        "id": "MV02-08",
        "desc": "Harness infrastructure guard: test executable not run / zero-test execution rejected",
        "file": "tests/test_wasm_mv02.py",
        "old": "EXECUTED_VECTOR_COUNT_GUARD = True",
        "new": "EXECUTED_VECTOR_COUNT_GUARD = False  # MV02-08",
        "build": "true",
        "test": "python3 -m unittest discover -s tests -p 'test_wasm_mv02.py' -v",
    },
    {
        "id": "MV02-09",
        "desc": "Artifact freshness guard: stale compiled WASM binary reused across distinct sources",
        "file": "tests/test_wasm_mv02.py",
        "old": "ARTIFACT_FRESHNESS_GUARD = True",
        "new": "ARTIFACT_FRESHNESS_GUARD = False  # MV02-09",
        "build": "true",
        "test": "python3 -m unittest discover -s tests -p 'test_wasm_mv02.py' -v",
    },
    {
        "id": "MV03-01",
        "desc": "Valid-module WASM 64-bit load upper-32-bit zeroing (i64.load align 3 -> i64.load32_u align 2)",
        "file": "part6_wasm.c",
        "old": "    if (wasm_valtype_from_ctype(type) == WASM_TYPE_I64) {\n        wasm_buf_append_u8(buf, WASM_OP_I64_LOAD);\n        wasm_emit_memarg_safe(buf, 3, offset);",
        "new": "    if (wasm_valtype_from_ctype(type) == WASM_TYPE_I64) {\n        wasm_buf_append_u8(buf, WASM_OP_I64_LOAD32_U);\n        wasm_emit_memarg_safe(buf, 2, offset); /* MV03-01 */",
        "build": "make zcc",
        "test": WASM_EXEC_CMD,
    },
    {
        "id": "MV03-02",
        "desc": "Valid-module WASM 64-bit store upper-32-bit truncation (i64.store align 3 -> i64.store32 align 2)",
        "file": "part6_wasm.c",
        "old": "    if (wasm_valtype_from_ctype(type) == WASM_TYPE_I64) {\n        wasm_buf_append_u8(buf, WASM_OP_I64_STORE);\n        wasm_emit_memarg_safe(buf, 3, offset);",
        "new": "    if (wasm_valtype_from_ctype(type) == WASM_TYPE_I64) {\n        wasm_buf_append_u8(buf, WASM_OP_I64_STORE32);\n        wasm_emit_memarg_safe(buf, 2, offset); /* MV03-02 */",
        "build": "make zcc",
        "test": WASM_EXEC_CMD,
    },
    {
        "id": "MV03-03",
        "desc": "Native x86-64 %rsp-relative operand uncompensated across pushq %r11 in 64-bit cmpq peephole",
        "file": "part5.c",
        "old": 'sprintf(line_ptrs[i], "    movq %%%s, %%%s\\n    movabsq $%lld, %%%s\\n    cmpq %%%s, %s\\n    movq %%%s, %%%s\\n", sc, xsc, cval, sc, sc, reg, xsc, sc);',
        "new": 'sprintf(line_ptrs[i], "    pushq %%%s\\n    movabsq $%lld, %%%s\\n    cmpq %%%s, %s\\n    popq %%%s\\n", sc, cval, sc, sc, reg, sc); /* MV03-03 */',
        "build": "make zcc",
        "test": "./zcc tests/x86_mv02_imm64.c -o /tmp/x86_mv03_mut && /tmp/x86_mv03_mut",
    },
    {
        "id": "MV03-04",
        "desc": "Inline-asm (ND_ASM) output operand constant-folded to stale initializer by ICP in part3.c",
        "file": "part3.c",
        "old": "if (n->asm_operands[i].is_output && op && op->kind == ND_VAR && op->sym && get_sym_stats(op->sym))",
        "new": "if (0 && n->asm_operands[i].is_output && op && op->kind == ND_VAR && op->sym && get_sym_stats(op->sym)) /* MV03-04 */",
        "build": "make zcc",
        "test": "./zcc tests/x86_mv02_imm64.c -o /tmp/x86_mv04_mut && /tmp/x86_mv04_mut",
    },
    {
        "id": "MV04-01",
        "desc": "Effective-address base register %r11 clobbered in 8(%r11) when strstr check reverts to strcmp",
        "file": "part5.c",
        "old": 'const char *sc = !strstr(reg, "%r11") ? "r11" : (!strstr(reg, "%r10") ? "r10" : "r9");',
        "new": 'const char *sc = (strcmp(reg, "%r11") != 0) ? "r11" : "r10"; /* MV04-01 */',
        "build": "make zcc",
        "test": "./zcc tests/x86_mv02_imm64.c -o /tmp/x86_mv04_01 && /tmp/x86_mv04_01",
    },
    {
        "id": "MV04-02",
        "desc": "SIB effective-address index register %r10 clobbered in 8(%r11, %r10, 8) without 3rd disjoint scratch %r9",
        "file": "part5.c",
        "old": 'const char *sc = !strstr(reg, "%r11") ? "r11" : (!strstr(reg, "%r10") ? "r10" : "r9");',
        "new": 'const char *sc = !strstr(reg, "%r11") ? "r11" : "r10"; /* MV04-02 */',
        "build": "make zcc",
        "test": "./zcc tests/x86_mv02_imm64.c -o /tmp/x86_mv04_02 && /tmp/x86_mv04_02",
    },
    {
        "id": "MV04-03",
        "desc": "Internal ELF assembler omits REX.X bit on extended SIB index register (%r10/%r11) in encode_cmpq_mem",
        "file": "src/codegen.c",
        "old": "  if (idx_reg >= 0 && (idx_reg & 8))\n    rex |= 0x02;",
        "new": "  if (0 && idx_reg >= 0 && (idx_reg & 8))\n    rex |= 0x02; /* MV04-03 */",
        "build": "make zcc",
        "test": "python3 -m unittest discover -s tests -p 'test_wasm_mv02.py' -v",
    },
    {
        "id": "MV05-01",
        "desc": "Push/pop scratch spill with +8 disp adjustment shifts direct %rsp by -8 and overwrites -8(%rsp) red-zone slot",
        "file": "part5.c",
        "old": 'sprintf(line_ptrs[i], "    movq %%%s, %%%s\\n    movabsq $%lld, %%%s\\n    cmpq %%%s, %s\\n    movq %%%s, %%%s\\n", sc, xsc, cval, sc, sc, reg, xsc, sc);',
        "new": 'char adj[80], *lp = strchr(reg, \'(\'); const char *op = reg; if (strstr(reg, "%rsp") && lp) { snprintf(adj, 80, "%lld%s", (lp > reg ? strtoll(reg, NULL, 10) : 0LL) + 8LL, lp); op = adj; }\n          sprintf(line_ptrs[i], "    pushq %%%s\\n    movabsq $%lld, %%%s\\n    cmpq %%%s, %s\\n    popq %%%s\\n", sc, cval, sc, sc, op, sc); /* MV05-01 */',
        "build": "make zcc",
        "test": "./zcc tests/x86_mv02_imm64.c -o /tmp/x86_mv05_01 && /tmp/x86_mv05_01",
    },
    {
        "id": "MV05-02",
        "desc": "Red-zone scratch spill at fixed -128(%rsp) without collision guard clobbers -128(%rsp) operand",
        "file": "part5.c",
        "old": 'sprintf(line_ptrs[i], "    movq %%%s, %%%s\\n    movabsq $%lld, %%%s\\n    cmpq %%%s, %s\\n    movq %%%s, %%%s\\n", sc, xsc, cval, sc, sc, reg, xsc, sc);',
        "new": 'int rz = -128;\n          sprintf(line_ptrs[i], "    movq %%%s, %d(%%rsp)\\n    movabsq $%lld, %%%s\\n    cmpq %%%s, %s\\n    movq %d(%%rsp), %%%s\\n", sc, rz, cval, sc, sc, reg, rz, sc); /* MV05-02 */',
        "build": "make zcc",
        "test": "./zcc tests/x86_mv02_imm64.c -o /tmp/x86_mv05_02 && /tmp/x86_mv05_02",
    },
    {
        "id": "MV06-01",
        "desc": "Red-zone scratch spill with -128/-120(%rsp) collision guard overwrites unrelated live red-zone slots",
        "file": "part5.c",
        "old": 'sprintf(line_ptrs[i], "    movq %%%s, %%%s\\n    movabsq $%lld, %%%s\\n    cmpq %%%s, %s\\n    movq %%%s, %%%s\\n", sc, xsc, cval, sc, sc, reg, xsc, sc);',
        "new": 'int rz = strstr(reg, "-128") ? -120 : -128;\n          sprintf(line_ptrs[i], "    movq %%%s, %d(%%rsp)\\n    movabsq $%lld, %%%s\\n    cmpq %%%s, %s\\n    movq %d(%%rsp), %%%s\\n", sc, rz, cval, sc, sc, reg, rz, sc); /* MV06-01 */',
        "build": "make zcc",
        "test": "./zcc tests/x86_mv02_imm64.c -o /tmp/x86_mv06_01 && /tmp/x86_mv06_01",
    },
    {
        "id": "MV06-02",
        "desc": "Miscomputed frame scratch slot (-8(%rbp)) overwrites live local variable slot",
        "file": "part5.c",
        "old": 'sprintf(line_ptrs[i], "    movq %%%s, %%%s\\n    movabsq $%lld, %%%s\\n    cmpq %%%s, %s\\n    movq %%%s, %%%s\\n", sc, xsc, cval, sc, sc, reg, xsc, sc);',
        "new": 'int sl = -8;\n          sprintf(line_ptrs[i], "    movq %%%s, %d(%%rbp)\\n    movabsq $%lld, %%%s\\n    cmpq %%%s, %s\\n    movq %d(%%rbp), %%%s\\n", sc, sl, cval, sc, sc, reg, sl, sc); /* MV06-02 */',
        "build": "make zcc",
        "test": "./zcc tests/x86_mv02_imm64.c -o /tmp/x86_mv06_02 && /tmp/x86_mv06_02",
    },
    {
        "id": "MV07-01",
        "desc": "Single-slot frame scratch -352(%rbp) without dual-slot guard clobbers 32(%rsp) / -352(%rbp) operand",
        "file": "part5.c",
        "old": 'sprintf(line_ptrs[i], "    movq %%%s, %%%s\\n    movabsq $%lld, %%%s\\n    cmpq %%%s, %s\\n    movq %%%s, %%%s\\n", sc, xsc, cval, sc, sc, reg, xsc, sc);',
        "new": 'int sl = -352;\n          sprintf(line_ptrs[i], "    movq %%%s, %d(%%rbp)\\n    movabsq $%lld, %%%s\\n    cmpq %%%s, %s\\n    movq %d(%%rbp), %%%s\\n", sc, sl, cval, sc, sc, reg, sl, sc); /* MV07-01 */',
        "build": "make zcc",
        "test": "./zcc tests/x86_mv02_imm64.c -o /tmp/x86_mv07_01 && /tmp/x86_mv07_01",
    },
    {
        "id": "MV07-02",
        "desc": "ZCC-PROVE-01 Peephole Truth Machine rejects unsound single-slot frame scratch lowering in part5.c",
        "file": "part5.c",
        "old": 'sprintf(line_ptrs[i], "    movq %%%s, %%%s\\n    movabsq $%lld, %%%s\\n    cmpq %%%s, %s\\n    movq %%%s, %%%s\\n", sc, xsc, cval, sc, sc, reg, xsc, sc);',
        "new": 'int sl = -(320 - 32);\n          sprintf(line_ptrs[i], "    movq %%%s, %d(%%rbp)\\n    movabsq $%lld, %%%s\\n    cmpq %%%s, %s\\n    movq %d(%%rbp), %%%s\\n", sc, sl, cval, sc, sc, reg, sl, sc); /* MV07-02 */',
        "build": "make zcc",
        "test": "python3 tools/zcc_prove_01.py",
    },
    {
        "id": "MV08-01",
        "desc": "MV-07 dual-slot textual displacement selector clobbers 24(%rsp, %rcx, 8) indexed EA and live 32(%rsp)/24(%rsp) memory",
        "file": "part5.c",
        "old": 'sprintf(line_ptrs[i], "    movq %%%s, %%%s\\n    movabsq $%lld, %%%s\\n    cmpq %%%s, %s\\n    movq %%%s, %%%s\\n", sc, xsc, cval, sc, sc, reg, xsc, sc);',
        "new": 'int f_sub = 384; int sl = -(f_sub - 32 + ((atoi(reg) == -(f_sub - 32) || atoi(reg) == 32) ? 8 : 0));\n          sprintf(line_ptrs[i], "    movq %%%s, %d(%%rbp)\\n    movabsq $%lld, %%%s\\n    cmpq %%%s, %s\\n    movq %d(%%rbp), %%%s\\n", sc, sl, cval, sc, sc, reg, sl, sc); /* MV08-01 */',
        "build": "make zcc",
        "test": "./zcc tests/x86_mv02_imm64.c -o /tmp/x86_mv08_01 && /tmp/x86_mv08_01",
    },
    {
        "id": "MV08-02",
        "desc": "ZCC-PROVE-01 Peephole Truth Machine rejects MV-07 dual-slot textual displacement selector on 24(%rsp, %rcx, 8) and full memory equality",
        "file": "part5.c",
        "old": 'sprintf(line_ptrs[i], "    movq %%%s, %%%s\\n    movabsq $%lld, %%%s\\n    cmpq %%%s, %s\\n    movq %%%s, %%%s\\n", sc, xsc, cval, sc, sc, reg, xsc, sc);',
        "new": 'int f_sub = 320; int sl = -(f_sub - 32 + ((atoi(reg) == -(f_sub - 32) || atoi(reg) == 32) ? 8 : 0));\n          sprintf(line_ptrs[i], "    movq %%%s, %d(%%rbp)\\n    movabsq $%lld, %%%s\\n    cmpq %%%s, %s\\n    movq %d(%%rbp), %%%s\\n", sc, sl, cval, sc, sc, reg, sl, sc); /* MV08-02 */',
        "build": "make zcc",
        "test": "python3 tools/zcc_prove_01.py",
    },
    {
        "id": "MV08-03",
        "desc": "Internal ELF assembler swaps REX.R and REX.B in encode_movq_xmm (0x66 0x0f 0x7e %xmm15 -> %r11)",
        "file": "src/codegen.c",
        "old": "    unsigned char modrm = 0xc0 | ((hw1 & 7) << 3) | (hw2 & 7);",
        "new": "    unsigned char modrm = 0xc0 | ((hw2 & 7) << 3) | (hw1 & 7); /* MV08-03 */",
        "build": "make zcc",
        "test": "./zcc tests/x86_mv02_imm64.c -o /tmp/x86_mv08_03 && /tmp/x86_mv08_03",
    },
    {
        "id": "MV09-01",
        "desc": "Unconditional %xmm15 scratch spill without intra-function liveness scan overwrites/zero-extends live 128-bit %xmm15",
        "file": "part5.c",
        "old": 'const char *xsc = !x15 ? "xmm15" : (!x14 ? "xmm14" : (!x13 ? "xmm13" : "xmm12"));',
        "new": 'const char *xsc = "xmm15"; /* MV09-01 */',
        "build": "make zcc",
        "test": "./zcc tests/x86_mv02_imm64.c -o /tmp/x86_mv09_01 && /tmp/x86_mv09_01",
    },
    {
        "id": "MV09-02",
        "desc": "ZCC-PROVE-01 128-bit XMM symbolic model & ownership verifier rejects unconditional %xmm15 scratch spill",
        "file": "part5.c",
        "old": 'const char *xsc = !x15 ? "xmm15" : (!x14 ? "xmm14" : (!x13 ? "xmm13" : "xmm12"));',
        "new": 'const char *xsc = "xmm15"; /* MV09-02 */',
        "build": "make zcc",
        "test": "python3 tools/zcc_prove_01.py",
    },
    {
        "id": "MV09-03",
        "desc": "Two-register XMM scratch selector without %xmm13 fallback overwrites live 128-bit %xmm14 when both %xmm15 and %xmm14 are live",
        "file": "part5.c",
        "old": 'const char *xsc = !x15 ? "xmm15" : (!x14 ? "xmm14" : (!x13 ? "xmm13" : "xmm12"));',
        "new": 'const char *xsc = !x15 ? "xmm15" : "xmm14"; /* MV09-03 */',
        "build": "make zcc",
        "test": "./zcc tests/x86_mv02_imm64.c -o /tmp/x86_mv09_03 && /tmp/x86_mv09_03",
    },
    {
        "id": "MV10-01",
        "desc": "Forward-only XMM liveness scan (k = i + 1) misses backward-loop %xmm15 read above cmpq and clobbers 128-bit %xmm15",
        "file": "part5.c",
        "old": 'while (k > 0 && strcmp(line_ptrs[k], "    pushq %rbp\\n") != 0) k--;\n          for (k++; k < nlines && strcmp(line_ptrs[k], "    pushq %rbp\\n") != 0; k++) {',
        "new": 'for (k = i + 1; k < nlines && strcmp(line_ptrs[k], "    pushq %rbp\\n") != 0; k++) { /* MV10-01 */',
        "build": "make zcc",
        "test": "./zcc tests/x86_mv02_imm64.c -o /tmp/x86_mv10_01 && /tmp/x86_mv10_01",
    },
    {
        "id": "MV10-02",
        "desc": "Three-register XMM scratch selector without %xmm12 fallback clobbers live 128-bit %xmm13 when %xmm15, %xmm14, and %xmm13 are live",
        "file": "part5.c",
        "old": 'const char *xsc = !x15 ? "xmm15" : (!x14 ? "xmm14" : (!x13 ? "xmm13" : "xmm12"));',
        "new": 'const char *xsc = !x15 ? "xmm15" : (!x14 ? "xmm14" : "xmm13"); /* MV10-02 */',
        "build": "make zcc",
        "test": "./zcc tests/x86_mv02_imm64.c -o /tmp/x86_mv10_02 && /tmp/x86_mv10_02",
    },
    {
        "id": "MV10-03",
        "desc": "Unguarded movq $0, %r -> xorq %r, %r without EFLAGS consumer lookahead clobbers live ZF before sete",
        "file": "part5.c",
        "old": 'if (k >= nlines || (strncmp(line_ptrs[k], "    set", 7) != 0 && strncmp(line_ptrs[k], "    cmov", 8) != 0 && (strncmp(line_ptrs[k], "    j", 5) != 0 || strncmp(line_ptrs[k], "    jmp", 7) == 0))) {',
        "new": 'if (1 || k >= nlines) { /* MV10-03 */',
        "build": "make zcc",
        "test": "./zcc tests/x86_mv02_imm64.c -o /tmp/x86_mv10_03 && /tmp/x86_mv10_03",
    },
    {
        "id": "MV10-04",
        "desc": "ZCC-PROVE-02 super-peephole verifier rejects part5.c when movq $0 -> xorq EFLAGS lookahead guard is disabled",
        "file": "part5.c",
        "old": 'if (k >= nlines || (strncmp(line_ptrs[k], "    set", 7) != 0 && strncmp(line_ptrs[k], "    cmov", 8) != 0 && (strncmp(line_ptrs[k], "    j", 5) != 0 || strncmp(line_ptrs[k], "    jmp", 7) == 0))) {',
        "new": 'if (1 || k >= nlines) { /* MV10-04 */',
        "build": "make zcc",
        "test": "python3 tools/zcc_prove_01.py",
    },
    {
        "id": "MV11-01",
        "desc": "Unguarded movq %r11, %rax dead-write elision before addq -64(%rbp), %rax or xorq %rcx, %rax clobbers live %rax",
        "file": "part5.c",
        "old": 'if ((strstr(nl, "(%rbp)") && (strncmp(nl, "    mov", 7) == 0 || strncmp(nl, "    lea", 7) == 0)) || strcmp(nl, "    xorq %rax, %rax\\n") == 0 || strcmp(nl, "    xorl %eax, %eax\\n") == 0 || strncmp(nl, "    movq $", 10) == 0 || strncmp(nl, "    movl $", 10) == 0) {',
        "new": 'if (strstr(nl, "(%rbp)") || strncmp(nl, "    xor", 7) == 0 || strncmp(nl, "    movq $", 10) == 0 || strncmp(nl, "    movl $", 10) == 0) { /* MV11-01 */',
        "build": "make zcc",
        "test": "./zcc tests/x86_mv02_imm64.c -o /tmp/x86_mv11_01 && /tmp/x86_mv11_01",
    },
    {
        "id": "MV11-02",
        "desc": "Internal ELF assembler strtoll on movabsq $0x8877665544332211 clamps unsigned 64-bit hex immediate >= 2^63 to LLONG_MAX",
        "file": "src/codegen.c",
        "old": "long long imm = (op1[1] == '-') ? strtoll(op1 + 1, NULL, 0) : (long long)strtoull(op1 + 1, NULL, 0);",
        "new": "long long imm = strtoll(op1 + 1, NULL, 0); /* MV11-02 */",
        "build": "make zcc",
        "test": "./zcc tests/x86_mv02_imm64.c -o /tmp/x86_mv11_02 && /tmp/x86_mv11_02",
    },
    {
        "id": "MV11-03",
        "desc": "Internal ELF assembler omits memory-operand dispatch on addq/subq/andq/orq/xorq and encodes addq -64(%rbp), %rax as addq %r15, %rax",
        "file": "src/codegen.c",
        "old": 'unsigned char mop = (strcmp(mnemonic, "addq") == 0) ? 0x00 : (strcmp(mnemonic, "orq") == 0) ? 0x08 : (strcmp(mnemonic, "andq") == 0) ? 0x20 : (strcmp(mnemonic, "subq") == 0) ? 0x28 : (strcmp(mnemonic, "xorq") == 0) ? 0x30 : (strcmp(mnemonic, "cmpq") == 0) ? 0x38 : 0xff;',
        "new": 'unsigned char mop = (strcmp(mnemonic, "cmpq") == 0) ? 0x38 : 0xff; /* MV11-03 */',
        "build": "make zcc",
        "test": "./zcc tests/x86_mv02_imm64.c -o /tmp/x86_mv11_03 && /tmp/x86_mv11_03",
    },
    {
        "id": "MV11-04",
        "desc": "ZCC-PROVE-03 verifier rejects part5.c when movq %r11, %rax dead-write elision guard reverts to unguarded (rbp)/xor check",
        "file": "part5.c",
        "old": 'if ((strstr(nl, "(%rbp)") && (strncmp(nl, "    mov", 7) == 0 || strncmp(nl, "    lea", 7) == 0)) || strcmp(nl, "    xorq %rax, %rax\\n") == 0 || strcmp(nl, "    xorl %eax, %eax\\n") == 0 || strncmp(nl, "    movq $", 10) == 0 || strncmp(nl, "    movl $", 10) == 0) {',
        "new": 'if (strstr(nl, "(%rbp)") || strncmp(nl, "    xor", 7) == 0 || strncmp(nl, "    movq $", 10) == 0 || strncmp(nl, "    movl $", 10) == 0) { /* MV11-04 */',
        "build": "make zcc",
        "test": "python3 tools/zcc_prove_01.py",
    },
    {
        "id": "MV12-01",
        "desc": "zcc_x86_peephole_boost Pass 7 unguarded (rbp)/(rip) match elides movq %rdx, %rax before addq -64(%rbp), %rax under ZCC_OPT=1",
        "file": "src/x86_codegen_sse.c",
        "old": '(((strncmp(nl, "    mov", 7) == 0 || strncmp(nl, "    lea", 7) == 0) && (strstr(nl, "(%rbp)") || strstr(nl, "(%rip)"))) ||',
        "new": '(strstr(nl, "(%rbp)") || strstr(nl, "(%rip)") || /* MV12-01 */',
        "build": "make zcc",
        "test": "ZCC_OPT=1 ./zcc tests/x86_mv02_imm64.c -o /tmp/x86_mv12_01 && /tmp/x86_mv12_01",
    },
    {
        "id": "MV12-02",
        "desc": "zcc_x86_peephole_boost Pass 7 elides movq %r11, %rax before pxor %xmm0, %xmm0 even though pxor %xmm0 does not write %rax",
        "file": "src/x86_codegen_sse.c",
        "old": '                        line_ptrs[i][0] = 0; eliminated += 1; i += 1; continue;\n                    }\n                }\n            }',
        "new": '                        line_ptrs[i][0] = 0; eliminated += 1; i += 1; continue;\n                    }\n                    if (strcmp(nl, "    pxor %xmm0, %xmm0\\n") == 0) { line_ptrs[i][0] = 0; eliminated += 1; i += 1; continue; } /* MV12-02 */\n                }\n            }',
        "build": "make zcc",
        "test": "ZCC_OPT=1 ./zcc tests/x86_mv02_imm64.c -o /tmp/x86_mv12_02 && /tmp/x86_mv12_02",
    },
    {
        "id": "MV12-03",
        "desc": "zcc_x86_peephole_boost Pass 0a sscanf %lld on movabsq $0x1122334455667788, %rax parses prefix 0 as imm==0 and rewrites 64-bit hex constant to xorl %eax, %eax",
        "file": "src/x86_codegen_sse.c",
        "old": '                char *endp = NULL;\n                const char *p = line_ptrs[i] + 13;\n                long long imm = (*p == \'-\') ? strtoll(p, &endp, 0) : (long long)strtoull(p, &endp, 0);\n                if (endp && endp > p && strcmp(endp, ", %rax\\n") == 0 &&',
        "new": '                long long imm = 0;\n                if (sscanf(line_ptrs[i], "    movabsq $%lld, %%rax", &imm) == 1 && /* MV12-03 */',
        "build": "make zcc",
        "test": "ZCC_OPT=1 ./zcc tests/x86_mv02_imm64.c -o /tmp/x86_mv12_03 && /tmp/x86_mv12_03",
    },
    {
        "id": "MV12-04",
        "desc": "TCO local-stack escape scan stops at .L branch labels and tears down stack frame while local pointer escapes to tail callee (killed by runtime & ZCC-PROVE-04)",
        "file": "part5.c",
        "old": '          for (k = i - 1; k >= 0 && strcmp(line_ptrs[k], "    pushq %rbp\\n") != 0; k--) {\n            if (strstr(line_ptrs[k], "leaq ") && strstr(line_ptrs[k], "(%rbp)")) { has_local_stack = 1; break; }\n          }',
        "new": '          for (k = i - 1; k >= 0 && k >= i - 32; k--) {\n            if (line_ptrs[k][0] == \'.\' && line_ptrs[k][1] == \'L\') break;\n            if (strstr(line_ptrs[k], "leaq ") && strstr(line_ptrs[k], "(%rbp)")) { has_local_stack = 1; break; }\n          } /* MV12-04 */',
        "build": "make zcc",
        "test": "./zcc tests/x86_mv02_imm64.c -o /tmp/x86_mv12_04 && /tmp/x86_mv12_04 && python3 tools/zcc_prove_01.py",
    },
]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", default=".")
    ap.add_argument("--file")
    ap.add_argument("--old")
    ap.add_argument("--new")
    ap.add_argument("--build", default="make zcc")
    ap.add_argument("--test")
    ap.add_argument("--run-ledger", action="store_true")
    ap.add_argument("--out-json", default="")
    args = ap.parse_args()

    repo = pathlib.Path(args.repo).resolve()

    if args.run_ledger:
        results = []
        all_killed = True
        for m in LEDGER_MUTATIONS:
            print(f"[MV-02] Running {m['id']}: {m['desc']} ...", flush=True)
            res = run_single_mutation(
                repo=repo,
                rel_file=m["file"],
                old_str=m["old"],
                new_str=m["new"],
                build_cmd=m["build"],
                test_cmd=m["test"],
                mutation_id=m["id"],
            )
            res["description"] = m["desc"]
            results.append(res)
            print(
                f"  -> {m['id']}: status={res['status']} baseline_exit={res.get('baseline_exit')} "
                f"mut_build_exit={res.get('mutated_build_exit')} mut_test_exit={res.get('mutated_test_exit')}",
                flush=True,
            )
            if not res.get("killed", False):
                all_killed = False
        if args.out_json:
            out_p = pathlib.Path(args.out_json)
            out_p.parent.mkdir(parents=True, exist_ok=True)
            out_p.write_text(json.dumps(results, indent=2), encoding="utf-8")
        print(f"[MV-02] Ledger summary: {sum(1 for r in results if r.get('killed'))}/{len(results)} mutations killed.")
        return 0 if all_killed else 1

    if not (args.file and args.old and args.new and args.test):
        ap.error("Specify either --run-ledger or all of --file, --old, --new, --test")

    res = run_single_mutation(
        repo=repo,
        rel_file=args.file,
        old_str=args.old,
        new_str=args.new,
        build_cmd=args.build,
        test_cmd=args.test,
        mutation_id="CLI",
    )
    print(json.dumps(res, indent=2))
    return 0 if res.get("killed", False) else 1


if __name__ == "__main__":
    sys.exit(main())
