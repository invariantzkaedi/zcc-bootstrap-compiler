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
            shutil.copy2(item, dst / name)
        elif name in ("src", "tests", "tools") and item.is_dir():
            shutil.copytree(item, dst / name, dirs_exist_ok=True)
        elif item.is_dir() and not (dst / name).exists():
            os.symlink(item, dst / name)


def run_single_mutation(
    repo: pathlib.Path,
    rel_file: str,
    old_str: str,
    new_str: str,
    build_cmd: str,
    test_cmd: str,
    mutation_id: str = "CUSTOM",
) -> dict:
    with tempfile.TemporaryDirectory(prefix=f"zcc_mv02_{mutation_id}_") as tmp:
        work = pathlib.Path(tmp) / "repo"
        copy_repo_fast(repo, work)
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
        base_test = run(test_cmd, work)
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
        "old": 'sprintf(line_ptrs[i], "    pushq %%%s\\n    movabsq $%lld, %%%s\\n    cmpq %%%s, %s\\n    popq %%%s\\n", sc, cval, sc, sc, op, sc);',
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
        "old": 'snprintf(adj, sizeof(adj), "%lld%s", disp + 8LL, lp); op = adj;',
        "new": 'snprintf(adj, sizeof(adj), "%lld%s", disp, lp); op = adj; /* MV03-03 */',
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
        "old": 'const char *sc = !strstr(reg, "%r11") ? "r11" : (!strstr(reg, "%r10") ? "r10" : "r9"), *op = reg;',
        "new": 'const char *sc = (strcmp(reg, "%r11") != 0) ? "r11" : "r10", *op = reg; /* MV04-01 */',
        "build": "make zcc",
        "test": "./zcc tests/x86_mv02_imm64.c -o /tmp/x86_mv04_01 && /tmp/x86_mv04_01",
    },
    {
        "id": "MV04-02",
        "desc": "SIB effective-address index register %r10 clobbered in 8(%r11, %r10, 8) without 3rd disjoint scratch %r9",
        "file": "part5.c",
        "old": 'const char *sc = !strstr(reg, "%r11") ? "r11" : (!strstr(reg, "%r10") ? "r10" : "r9"), *op = reg;',
        "new": 'const char *sc = !strstr(reg, "%r11") ? "r11" : "r10", *op = reg; /* MV04-02 */',
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
