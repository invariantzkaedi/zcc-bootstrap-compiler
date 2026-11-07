#!/usr/bin/env python3
"""
tools/mv02_mutation.py — Controlled Adversarial Semantic & Harness Mutation Verifier (WASM-MV-02)
Supports both single-mutation CLI invocation (--repo/--file/--old/--new/--build/--test)
and full multi-process parallel + incremental object-cache ledger execution (--run-ledger [--jobs N]).
"""
import argparse
import concurrent.futures
import hashlib
import json
import os
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile
import time


def sha256(path: pathlib.Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def run(cmd: str, cwd: pathlib.Path, timeout: int = 120, env: dict = None):
    return subprocess.run(
        cmd,
        cwd=str(cwd),
        shell=True,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        timeout=timeout,
        env=env,
    )


def parse_makefile_var(makefile_path: pathlib.Path, var_name: str) -> list:
    txt = makefile_path.read_text(encoding="utf-8", errors="replace")
    m = re.search(rf"^{re.escape(var_name)}\s*=\s*(.+)$", txt, re.MULTILINE)
    if not m:
        return []
    return m.group(1).strip().split()


_MV02_TESTS_NEEDED = (
    "wasm_mv02_signedness.c",
    "x86_mv02_imm64.c",
    "typedef_compatible.c",
    "typedef_incompatible.c",
    "test_wasm_mv02.py",
)


def _copy_file_retry(src_file: pathlib.Path, dst_file: pathlib.Path):
    dst_file.parent.mkdir(parents=True, exist_ok=True)
    for attempt in range(5):
        try:
            data = src_file.read_bytes()
            if data or src_file.stat().st_size == 0:
                dst_file.write_bytes(data)
                shutil.copymode(src_file, dst_file)
                return
        except OSError:
            time.sleep(0.05)


def copy_repo_fast(src: pathlib.Path, dst: pathlib.Path):
    """Copy only the exact compiler source, headers, and MV-02 test files into /tmp (avoiding 9P scan of root)."""
    dst.mkdir(parents=True, exist_ok=True)
    makefile = src / "Makefile"
    parts = parse_makefile_var(makefile, "PARTS")
    passes = parse_makefile_var(makefile, "PASSES")

    explicit_files = set(parts) | set(passes) | {
        "Makefile",
        "sync_bridge.py",
        "tools/zcc_prove_01.py",
        "tools/mv02_mutation.py",
    } | {f"tests/{t}" for t in _MV02_TESTS_NEEDED}

    for rel in sorted(explicit_files):
        sp = src / rel
        if sp.is_file():
            _copy_file_retry(sp, dst / rel)
            if rel.endswith(".c"):
                txt = (dst / rel).read_text(encoding="utf-8", errors="ignore")
                for inc in re.findall(r'#include\s+"([^"]+)"', txt):
                    inc_p = src / inc
                    if inc_p.is_file() and not (dst / inc).exists():
                        _copy_file_retry(inc_p, dst / inc)

    for pattern in ("*.h", "*.inc", "part*.c", "zcc*.c", "ir*.c"):
        for hdr in src.glob(pattern):
            if hdr.is_file() and hdr.name != "zcc.c":
                _copy_file_retry(hdr, dst / hdr.name)

    for dname in ("include", "src", ".github"):
        sd = src / dname
        if sd.is_dir():
            shutil.copytree(sd, dst / dname, dirs_exist_ok=True)

    lex_dir = src / "docs" / "lexicon"
    if lex_dir.is_dir():
        shutil.copytree(lex_dir, dst / "docs" / "lexicon", dirs_exist_ok=True)


_BASELINE_TEST_CACHE = {}
_STAGE_DIR = None
_OBJ_CACHE_META = None


def compute_stage_fingerprint(stage: pathlib.Path, parts: list, passes: list) -> str:
    """Compute a deterministic SHA-256 fingerprint of all compiler sources, headers, and MV-02 tests."""
    h = hashlib.sha256()
    tracked = list(parts) + list(passes) + [
        "Makefile",
        "sync_bridge.py",
        "tools/zcc_prove_01.py",
    ] + [f"tests/{t}" for t in _MV02_TESTS_NEEDED]
    for hdr in sorted(stage.glob("*.h")) + sorted(stage.glob("include/**/*.h")) + sorted(stage.glob("src/**/*.h")):
        rel = str(hdr.relative_to(stage)).replace("\\", "/")
        if rel not in tracked:
            tracked.append(rel)
    for rel in sorted(set(tracked)):
        p = stage / rel
        if p.is_file():
            h.update(rel.encode("utf-8") + b"\0")
            h.update(p.read_bytes() + b"\0")
    return h.hexdigest()[:24]


def prepare_obj_cache(stage: pathlib.Path) -> dict:
    """Pre-compile zcc.o and all PASSES .o files in parallel, reusing /tmp/zcc_mv02_pcache_<fp> when unchanged."""
    makefile = stage / "Makefile"
    parts = parse_makefile_var(makefile, "PARTS")
    passes = parse_makefile_var(makefile, "PASSES")
    cflags = "-O0 -w -fno-asynchronous-unwind-tables -g0 -DZCC_REAL_TELEMETRY -Iinclude -I."
    fp = compute_stage_fingerprint(stage, parts, passes)
    pcache_dir = pathlib.Path(tempfile.gettempdir()) / f"zcc_mv02_pcache_{fp}"
    obj_dir = pcache_dir / "obj_cache"
    meta_json = pcache_dir / "meta.json"

    # Ensure bridge headers and amalgamated zcc.c exist in stage
    run("python3 sync_bridge.py part1.c zcc_ast_bridge_constants.h zcc_ast_bridge_asserts.inc", stage)
    with (stage / "zcc.c").open("wb") as out_f:
        for p in parts:
            out_f.write((stage / p).read_bytes())

    pass_objs = {
        psrc: str(obj_dir / f"pass_{idx:03d}_{psrc.replace('/', '_')}.o")
        for idx, psrc in enumerate(passes)
    }
    zcc_obj_path = obj_dir / "zcc.o"
    cached_zcc_bin = pcache_dir / "zcc"

    if (
        meta_json.is_file()
        and cached_zcc_bin.is_file()
        and zcc_obj_path.is_file()
        and all(pathlib.Path(p).is_file() for p in pass_objs.values())
    ):
        shutil.copy2(cached_zcc_bin, stage / "zcc")
        saved = json.loads(meta_json.read_text(encoding="utf-8"))
        return {
            "parts": parts,
            "passes": passes,
            "cflags": cflags,
            "zcc_obj": str(zcc_obj_path),
            "pass_objs": pass_objs,
            "base_bin_hash": saved["base_bin_hash"],
            "pcache_dir": str(pcache_dir),
            "cache_hit": True,
            "baseline_verified": saved.get("baseline_verified", False),
        }

    obj_dir.mkdir(parents=True, exist_ok=True)
    compile_units = [("zcc.c", zcc_obj_path, f"cc {cflags} -Dmain=zcc_main -c zcc.c -o {zcc_obj_path}")]
    for psrc in passes:
        obj_path = pathlib.Path(pass_objs[psrc])
        compile_units.append((psrc, obj_path, f"cc {cflags} -c {psrc} -o {obj_path}"))

    def _compile_unit(unit):
        _, _, cmd = unit
        r = run(cmd, stage)
        if r.returncode != 0:
            raise RuntimeError(f"Failed precompiling {cmd}: {r.stdout}")

    with concurrent.futures.ThreadPoolExecutor(max_workers=min(os.cpu_count() or 8, 16)) as ex:
        list(ex.map(_compile_unit, compile_units))

    all_objs = [str(zcc_obj_path)] + [pass_objs[p] for p in passes]
    link_cmd = f"cc {cflags} -o zcc {' '.join(all_objs)} -lm -Wl,-s && strip --strip-all zcc"
    r = run(link_cmd, stage)
    if r.returncode != 0:
        raise RuntimeError(f"Failed linking baseline zcc in stage: {r.stdout}")

    shutil.copy2(stage / "zcc", cached_zcc_bin)
    base_hash = sha256(stage / "zcc")
    meta_json.write_text(
        json.dumps({"base_bin_hash": base_hash, "baseline_verified": False}, indent=2),
        encoding="utf-8",
    )

    return {
        "parts": parts,
        "passes": passes,
        "cflags": cflags,
        "zcc_obj": str(zcc_obj_path),
        "pass_objs": pass_objs,
        "base_bin_hash": base_hash,
        "pcache_dir": str(pcache_dir),
        "cache_hit": False,
        "baseline_verified": False,
    }


def get_stage_dir(repo: pathlib.Path) -> pathlib.Path:
    global _STAGE_DIR, _OBJ_CACHE_META
    if _STAGE_DIR is None:
        _STAGE_DIR = pathlib.Path(tempfile.mkdtemp(prefix="zcc_mv02_stage_"))
        copy_repo_fast(repo, _STAGE_DIR)
        _OBJ_CACHE_META = prepare_obj_cache(_STAGE_DIR)
    return _STAGE_DIR


def fast_incremental_build(work: pathlib.Path, rel_file: str, meta: dict):
    """Recompile only the mutated translation unit and link against cached .o files."""
    cflags = meta["cflags"]
    parts = meta["parts"]
    passes = meta["passes"]
    zcc_obj = meta["zcc_obj"]
    pass_objs = dict(meta["pass_objs"])

    if rel_file in parts or rel_file.endswith((".h", ".inc")):
        with (work / "zcc.c").open("wb") as out_f:
            for p in parts:
                out_f.write((work / p).read_bytes())
        local_zcc_obj = str(work / "mut_zcc.o")
        r = run(f"cc {cflags} -Dmain=zcc_main -c zcc.c -o {local_zcc_obj}", work)
        if r.returncode != 0:
            return r
        zcc_obj = local_zcc_obj

    if rel_file in pass_objs:
        local_pass_obj = str(work / "mut_pass.o")
        r = run(f"cc {cflags} -c {rel_file} -o {local_pass_obj}", work)
        if r.returncode != 0:
            return r
        pass_objs[rel_file] = local_pass_obj
    elif rel_file.endswith(".h"):
        # If a header in src/ or include/ was mutated, also recompile matching .c in passes if present
        stem_c = rel_file[:-2] + ".c"
        if stem_c in pass_objs:
            local_pass_obj = str(work / "mut_hdr_pass.o")
            r = run(f"cc {cflags} -c {stem_c} -o {local_pass_obj}", work)
            if r.returncode != 0:
                return r
            pass_objs[stem_c] = local_pass_obj

    all_objs = [zcc_obj] + [pass_objs[p] for p in passes]
    link_cmd = f"cc {cflags} -o zcc {' '.join(all_objs)} -lm -Wl,-s && strip --strip-all zcc"
    return run(link_cmd, work)


def isolate_test_cmd(test_cmd: str, worker_tmp: pathlib.Path) -> str:
    """Redirect hardcoded /tmp/ artifact paths inside test_cmd to worker-private temp dir."""
    wtmp = str(worker_tmp).rstrip("/") + "/"
    return test_cmd.replace("/tmp/", wtmp)


def run_single_mutation(
    repo: pathlib.Path,
    rel_file: str,
    old_str: str,
    new_str: str,
    build_cmd: str,
    test_cmd: str,
    mutation_id: str = "CUSTOM",
    stage_dir: pathlib.Path = None,
    obj_meta: dict = None,
    baseline_ok: bool = False,
) -> dict:
    t0 = time.monotonic()
    stage = stage_dir if stage_dir is not None else get_stage_dir(repo)
    meta = obj_meta if obj_meta is not None else _OBJ_CACHE_META
    with tempfile.TemporaryDirectory(prefix=f"zcc_mv02_{mutation_id}_") as tmp:
        tmp_path = pathlib.Path(tmp)
        work = tmp_path / "repo"
        wtmp = tmp_path / "wtmp"
        wtmp.mkdir(parents=True, exist_ok=True)
        shutil.copytree(stage, work, symlinks=True, ignore=shutil.ignore_patterns(".obj_cache"))
        target = work / rel_file
        original = target.read_text(encoding="utf-8")
        if old_str not in original:
            return {
                "mutation_id": mutation_id,
                "status": "ERROR_ANCHOR_NOT_FOUND",
                "file": rel_file,
                "killed": False,
            }

        isolated_test_cmd = isolate_test_cmd(test_cmd, wtmp)
        env = os.environ.copy()
        env["TMPDIR"] = str(wtmp)

        # 1. Baseline build & test
        zcc_bin = work / "zcc"
        base_bin_hash = meta["base_bin_hash"] if (meta and zcc_bin.exists()) else (sha256(zcc_bin) if zcc_bin.exists() else "N/A")
        if not baseline_ok:
            cache_key = (base_bin_hash, test_cmd)
            if cache_key not in _BASELINE_TEST_CACHE:
                _BASELINE_TEST_CACHE[cache_key] = run(isolated_test_cmd, work, env=env)
            base_test = _BASELINE_TEST_CACHE[cache_key]
            if base_test.returncode != 0:
                return {
                    "mutation_id": mutation_id,
                    "status": "ERROR_BASELINE_TEST_FAILED",
                    "baseline_exit": base_test.returncode,
                    "output": base_test.stdout[-800:],
                    "killed": False,
                }
            base_exit = base_test.returncode
        else:
            base_exit = 0

        # 2. Apply single mutation
        mutated = original.replace(old_str, new_str, 1)
        target.write_text(mutated, encoding="utf-8")
        mut_src_hash = sha256(target)
        if not rel_file.startswith(("tests/", "tools/")):
            if (work / "zcc.c").exists():
                (work / "zcc.c").unlink()
            if zcc_bin.exists():
                zcc_bin.unlink()

        if build_cmd == "make zcc" and meta is not None and not rel_file.startswith(("tests/", "tools/")):
            mut_build = fast_incremental_build(work, rel_file, meta)
        else:
            mut_build = run(build_cmd, work, env=env)
        if mut_build.returncode != 0:
            return {
                "mutation_id": mutation_id,
                "status": "ERROR_MUTATION_BUILD_FAILED",
                "mut_build_exit": mut_build.returncode,
                "output": mut_build.stdout[-800:],
                "killed": False,
            }
        mut_bin_hash = sha256(zcc_bin) if zcc_bin.exists() else "N/A"
        mut_test = run(isolated_test_cmd, work, env=env)

        killed = mut_test.returncode != 0 and (
            base_bin_hash != mut_bin_hash or rel_file.startswith(("tests/", "tools/"))
        )
        return {
            "mutation_id": mutation_id,
            "file": rel_file,
            "mutated_source_sha256": mut_src_hash,
            "baseline_binary_sha256": base_bin_hash,
            "mutated_binary_sha256": mut_bin_hash,
            "baseline_exit": base_exit,
            "mutated_build_exit": mut_build.returncode,
            "mutated_test_exit": mut_test.returncode,
            "killed": killed,
            "status": "KILLED" if killed else "SURVIVED",
            "elapsed_sec": round(time.monotonic() - t0, 2),
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
        "test": "python3 -m unittest tests.test_wasm_mv02.TestWasmMV02Adversarial.test_04_seven_defect_class_seed_sensitivity",
    },
    {
        "id": "MV02-09",
        "desc": "Artifact freshness guard: stale compiled WASM binary reused across distinct sources",
        "file": "tests/test_wasm_mv02.py",
        "old": "ARTIFACT_FRESHNESS_GUARD = True",
        "new": "ARTIFACT_FRESHNESS_GUARD = False  # MV02-09",
        "build": "true",
        "test": "python3 -m unittest tests.test_wasm_mv02.TestWasmMV02Adversarial.test_04_seven_defect_class_seed_sensitivity",
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
        "test": "python3 -m unittest tests.test_wasm_mv02.TestWasmMV02Adversarial.test_05_mv04_gnu_as_binary_encoding_equivalence",
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
    {
        "id": "MV13-01",
        "desc": "zcc_x86_peephole_boost Pass 8 tracks rsp_depth linearly across .L labels and jumps, injecting unaligned subq $8, %rsp before call",
        "file": "src/x86_codegen_sse.c",
        "old": '                } else if (strchr(l, \':\') != NULL || strncmp(l, "    j", 5) == 0 || strncmp(l, "    ret", 7) == 0 ||\n                           (strstr(l, "%rsp") && strncmp(l, "    .", 5) != 0 && !strstr(l, "(%rsp)") &&',
        "new": '                } else if ((strstr(l, "%rsp") && strncmp(l, "    .", 5) != 0 && !strstr(l, "(%rsp)") && /* MV13-01 */',
        "build": "make zcc",
        "test": "ZCC_OPT=1 ./zcc tests/x86_mv02_imm64.c -o /tmp/x86_mv13_01 && /tmp/x86_mv13_01",
    },
    {
        "id": "MV13-02",
        "desc": "zcc_x86_peephole_boost Pass 5c elides movq %xmm0, %rax before addsd %xmm0, %xmm0 without checking if %rax is overwritten before being read",
        "file": "src/x86_codegen_sse.c",
        "old": '                        if (strcmp(nl, "    movq %xmm0, %rax\\n") == 0 || strcmp(nl, "    movd %xmm0, %eax\\n") == 0 ||',
        "new": '                        if (1 || strcmp(nl, "    movq %xmm0, %rax\\n") == 0 || strcmp(nl, "    movd %xmm0, %eax\\n") == 0 || /* MV13-02 */',
        "build": "make zcc",
        "test": "ZCC_OPT=1 ./zcc tests/x86_mv02_imm64.c -o /tmp/x86_mv13_02 && /tmp/x86_mv13_02",
    },
    {
        "id": "MV13-03",
        "desc": "ZCC-PROVE-05 verifier rejects src/x86_codegen_sse.c when Pass 8 label/jump rsp_depth invalidation guard is removed",
        "file": "src/x86_codegen_sse.c",
        "old": '                } else if (strchr(l, \':\') != NULL || strncmp(l, "    j", 5) == 0 || strncmp(l, "    ret", 7) == 0 ||\n                           (strstr(l, "%rsp") && strncmp(l, "    .", 5) != 0 && !strstr(l, "(%rsp)") &&',
        "new": '                } else if ((strstr(l, "%rsp") && strncmp(l, "    .", 5) != 0 && !strstr(l, "(%rsp)") && /* MV13-03 */',
        "build": "make zcc",
        "test": "python3 tools/zcc_prove_01.py",
    },
    {
        "id": "MV13-04",
        "desc": "ZCC-PROVE-05 verifier rejects src/x86_codegen_sse.c when Pass 5c %rax overwrite lookahead guard is removed",
        "file": "src/x86_codegen_sse.c",
        "old": '                        strncmp(nl, "    set", 7) == 0 || strncmp(nl, "    cvttss2si ", 14) == 0 || strncmp(nl, "    cvttsd2si ", 14) == 0 ||',
        "new": '                        strncmp(nl, "    set", 7) == 0 || /* MV13-04 */',
        "build": "make zcc",
        "test": "python3 tools/zcc_prove_01.py",
    },
    {
        "id": "MV14-01",
        "desc": "Peephole boost Pass 6 elides movl %eax, %eax after movq $imm, %r11 when destination register check (, %rax) is omitted",
        "file": "src/x86_codegen_sse.c",
        "old": '                    (parse_imm_line(l1, "    movq $", ", %rax\\n", &imm) && imm >= 0 && imm <= 2147483647LL)) {',
        "new": '                    (strncmp(l1, "    movq $", 10) == 0 && l1[10] >= \'0\' && l1[10] <= \'9\' && atoll(l1 + 10) <= 2147483647LL)) { /* MV14-01 */',
        "build": "make zcc",
        "test": "ZCC_OPT=1 ./zcc tests/x86_mv02_imm64.c -o /tmp/x86_mv14_01 && /tmp/x86_mv14_01",
    },
    {
        "id": "MV14-02",
        "desc": "Peephole boost Pass 3 misparses hex immediate movq $0x10, %r11 as 0 when sscanf $%lld is used instead of parse_imm_line",
        "file": "src/x86_codegen_sse.c",
        "old": '                if (parse_imm_line(l1, "    movq $", ", %r11\\n", &c)) {',
        "new": '                if (sscanf(l1, "    movq $%lld, %%r11", &c) == 1) { /* MV14-02 */',
        "build": "make zcc",
        "test": "ZCC_OPT=1 ./zcc tests/x86_mv02_imm64.c -o /tmp/x86_mv14_02 && /tmp/x86_mv14_02",
    },
    {
        "id": "MV14-03",
        "desc": "Internal ELF assembler src/codegen.c saturates movq $0xfffffffffffffff0, %rax to -1 (LLONG_MAX) when strtoll is used instead of strtoull",
        "file": "src/codegen.c",
        "old": "          } else if (op1[0] == '$') {\n            long long imm = (op1[1] == '-') ? strtoll(op1 + 1, NULL, 0) : (long long)strtoull(op1 + 1, NULL, 0);",
        "new": "          } else if (op1[0] == '$') {\n            long long imm = strtoll(op1 + 1, NULL, 0); /* MV14-03 */",
        "build": "make zcc",
        "test": "ZCC_OPT=1 ./zcc tests/x86_mv02_imm64.c -o /tmp/x86_mv14_03 && /tmp/x86_mv14_03",
    },
    {
        "id": "MV14-04",
        "desc": "ZCC-PROVE-06 verifier rejects src/x86_codegen_sse.c when Pass 6 movl $, %eax destination suffix guard is removed",
        "file": "src/x86_codegen_sse.c",
        "old": '                    (l1_len > 16 && strncmp(l1, "    movl $", 10) == 0 && strcmp(l1 + l1_len - 7, ", %eax\\n") == 0) ||',
        "new": '                    strncmp(l1, "    movl $", 10) == 0 || /* MV14-04 */',
        "build": "make zcc",
        "test": "python3 tools/zcc_prove_01.py",
    },
    {
        "id": "MV15-01",
        "desc": "Peephole boost Pass 5c elides live movq %xmm0, %rax before addsd %xmm0, %xmm0 + movq $5, %r11 when movq $ is not guarded by , %rax destination check",
        "file": "src/x86_codegen_sse.c",
        "old": '                            strcmp(nl, "    xorl %eax, %eax\\n") == 0 || strcmp(nl, "    xorq %rax, %rax\\n") == 0 ||\n                            (nlen >= 7 && (strcmp(nl + nlen - 7, ", %rax\\n") == 0 || strcmp(nl + nlen - 7, ", %eax\\n") == 0) &&',
        "new": '                            strcmp(nl, "    xorl %eax, %eax\\n") == 0 || strcmp(nl, "    xorq %rax, %rax\\n") == 0 ||\n                            strncmp(nl, "    movq $", 10) == 0 || strncmp(nl, "    movl $", 10) == 0 ||\n                            (nlen >= 7 && (strcmp(nl + nlen - 7, ", %rax\\n") == 0 || strcmp(nl + nlen - 7, ", %eax\\n") == 0) && /* MV15-01 */',
        "build": "make zcc",
        "test": "ZCC_OPT=1 ./zcc tests/x86_mv02_imm64.c -o /tmp/x86_mv15_01 && /tmp/x86_mv15_01",
    },
    {
        "id": "MV15-02",
        "desc": "Peephole boost Pass 6 reverse-coupled back-propagation rewrites movq -32(%rbp), %rax to %r11 before movq $99, -40(%rbp) when movq $ lacks , %rax suffix check",
        "file": "src/x86_codegen_sse.c",
        "old": '                                (nlen > 16 && strncmp(nl, "    movq $", 10) == 0 && strcmp(nl + nlen - 7, ", %rax\\n") == 0) ||\n                                strcmp(nl, "    xorl %eax, %eax\\n") == 0 ||\n                                strcmp(nl, "    popq %rax\\n") == 0) {',
        "new": '                                strncmp(nl, "    movq $", 10) == 0 ||\n                                strcmp(nl, "    xorl %eax, %eax\\n") == 0 ||\n                                strcmp(nl, "    popq %rax\\n") == 0) { /* MV15-02 */',
        "build": "make zcc",
        "test": "ZCC_OPT=1 ./zcc tests/x86_mv02_imm64.c -o /tmp/x86_mv15_02 && /tmp/x86_mv15_02",
    },
    {
        "id": "MV15-03",
        "desc": "Internal ELF assembler src/codegen.c saturates addq $0xfffffffffffffff0, %rax to -1 (LLONG_MAX) when strtoll is used instead of strtoull",
        "file": "src/codegen.c",
        "old": '                   strcmp(mnemonic, "xorq") == 0) {\n          if (op1[0] == \'$\') {\n            long long imm = (op1[1] == \'-\') ? strtoll(op1 + 1, NULL, 0) : (long long)strtoull(op1 + 1, NULL, 0);',
        "new": '                   strcmp(mnemonic, "xorq") == 0) {\n          if (op1[0] == \'$\') {\n            long long imm = strtoll(op1 + 1, NULL, 0); /* MV15-03 */',
        "build": "make zcc",
        "test": "ZCC_OPT=1 ./zcc tests/x86_mv02_imm64.c -o /tmp/x86_mv15_03 && /tmp/x86_mv15_03",
    },
    {
        "id": "MV15-04",
        "desc": "ZCC-PROVE-07 verifier rejects src/x86_codegen_sse.c when Pass 5c XMM call-arg bypass movl $ destination suffix guard is removed",
        "file": "src/x86_codegen_sse.c",
        "old": '                                        (nk_len > 16 && strncmp(line_ptrs[nk], "    movl $", 10) == 0 && strcmp(line_ptrs[nk] + nk_len - 7, ", %eax\\n") == 0)) {',
        "new": '                                        strncmp(line_ptrs[nk], "    movl $", 10) == 0) { /* MV15-04 */',
        "build": "make zcc",
        "test": "python3 tools/zcc_prove_01.py",
    },
    {
        "id": "MV16-01",
        "desc": "Peephole boost Pass 0a rewrites movabsq $0, %rax to xorl %eax, %eax before sete %cl when EFLAGS lookahead guard is omitted",
        "file": "src/x86_codegen_sse.c",
        "old": '                        if (k >= nlines || (strncmp(line_ptrs[k], "    set", 7) != 0 && strncmp(line_ptrs[k], "    cmov", 8) != 0 &&\n                            (strncmp(line_ptrs[k], "    j", 5) != 0 || strncmp(line_ptrs[k], "    jmp", 7) == 0))) {',
        "new": '                        if (1 || k >= nlines) { /* MV16-01 */',
        "build": "make zcc",
        "test": "ZCC_OPT=1 ./zcc tests/x86_mv02_imm64.c -o /tmp/x86_mv16_01 && /tmp/x86_mv16_01",
    },
    {
        "id": "MV16-02",
        "desc": "Peephole boost Pass 0b deletes unrelated .LS_flit_N .rodata symbol when lbl_buf == ref_lbl identity check is omitted",
        "file": "src/x86_codegen_sse.c",
        "old": '                strcmp(lbl_buf, ref_lbl) != 0 ||',
        "new": '                0 || /* MV16-02 */',
        "build": "make zcc",
        "test": "ZCC_OPT=1 ./zcc tests/x86_mv02_imm64.c -o /tmp/x86_mv16_02 && /tmp/x86_mv16_02",
    },
    {
        "id": "MV16-03",
        "desc": "part5.c Rule 3b Push/Imm/Mov/Pop quad deletes movq $99, %rdx and overwrites %r11 when , %rax suffix check is omitted",
        "file": "part5.c",
        "old": '        if (l2_len > 16 && strncmp(l2, "    movq $", 10) == 0 && strcmp(l2 + l2_len - 7, ", %rax\\n") == 0) {',
        "new": '        if (strncmp(l2, "    movq $", 10) == 0) { /* MV16-03 */',
        "build": "make zcc",
        "test": "./zcc tests/x86_mv02_imm64.c -o /tmp/x86_mv16_03 && /tmp/x86_mv16_03",
    },
    {
        "id": "MV16-04",
        "desc": "Internal ELF assembler src/codegen.c fails on leaq 16(%rax,%rcx,8), %rdx and saturates addb $0xfffffffffffffff0, %al (killed by runtime & ZCC-PROVE-08)",
        "file": "src/codegen.c",
        "old": '          } else if (strchr(op1, \',\') != NULL && strchr(op1, \'(\') != NULL) {\n            encode_cmpq_mem(&text_seg, op1, reg2, 0x8D);',
        "new": '          } else if (op1[0] == \'(\' && strchr(op1, \',\') != NULL) {\n            encode_cmpq_mem(&text_seg, op1, reg2, 0x8D); /* MV16-04 */',
        "build": "make zcc",
        "test": "./zcc tests/x86_mv02_imm64.c -o /tmp/x86_mv16_04 && /tmp/x86_mv16_04 && python3 tools/zcc_prove_01.py",
    },
    {
        "id": "MV18-01",
        "desc": "ARM64 prologue allocates frame inside stp [sp, #-aligned_stack]! before mov x29, sp so negative [x29, #-16] locals lie below sp and get clobbered by pushes",
        "file": "part6_arm.c",
        "old": '    fprintf(cc->out, "    stp x29, x30, [sp, #-16]!\\n");\n    fprintf(cc->out, "    mov x29, sp\\n");\n    if (aligned_stack <= 4095) {\n        fprintf(cc->out, "    sub sp, sp, #%d\\n", aligned_stack);\n    } else {',
        "new": '    fprintf(cc->out, "    stp x29, x30, [sp, #-%d]!\\n", aligned_stack);\n    fprintf(cc->out, "    mov x29, sp\\n");\n    if (0) {\n        fprintf(cc->out, "    sub sp, sp, #%d\\n", aligned_stack); /* MV18-01 */\n    } else if (0) {',
        "build": "make zcc",
        "test": "python3 -m unittest tests.test_wasm_mv02.TestWasmMV02Adversarial.test_09_arm64_riscv64_qemu_and_wasm_autovec_v128",
    },
    {
        "id": "MV18-02",
        "desc": "ARM64 aarch64_emit_store emits 64-bit str x1, [x0] for 32-bit int stores, corrupting adjacent int array elements",
        "file": "part6_arm.c",
        "old": 'static void aarch64_emit_store(Compiler *cc, Type *type) {\n    if (!type || type->size >= 8 || type->kind == TY_PTR) fprintf(cc->out, "    str x1, [x0]\\n");\n    else if (type->size == 4) fprintf(cc->out, "    str w1, [x0]\\n");',
        "new": 'static void aarch64_emit_store(Compiler *cc, Type *type) {\n    if (!type || type->size >= 4 || type->kind == TY_PTR) fprintf(cc->out, "    str x1, [x0]\\n"); /* MV18-02 */',
        "build": "make zcc",
        "test": "python3 -m unittest tests.test_wasm_mv02.TestWasmMV02Adversarial.test_09_arm64_riscv64_qemu_and_wasm_autovec_v128",
    },
    {
        "id": "MV18-03",
        "desc": "RISC-V 64 prologue sets s0 = sp + 16 so saved ra/s0 at 8(sp)/0(sp) alias -8(s0)/-16(s0) parameter spill slots",
        "file": "part6_riscv.c",
        "old": '    fprintf(cc->out, "    sd ra, 8(sp)\\n");\n    fprintf(cc->out, "    sd s0, 0(sp)\\n");\n    fprintf(cc->out, "    mv s0, sp\\n");',
        "new": '    fprintf(cc->out, "    sd ra, 8(sp)\\n");\n    fprintf(cc->out, "    sd s0, 0(sp)\\n");\n    fprintf(cc->out, "    addi s0, sp, 16\\n"); /* MV18-03 */',
        "build": "make zcc",
        "test": "python3 -m unittest tests.test_wasm_mv02.TestWasmMV02Adversarial.test_09_arm64_riscv64_qemu_and_wasm_autovec_v128",
    },
    {
        "id": "MV18-04",
        "desc": "WASM ND_FOR loop auto-vectorizer disabled so canonical 4-lane i32x4/f32x4 array loops fail to emit v128.load/store SIMD128 instructions",
        "file": "part6_wasm.c",
        "old": '            wasm_try_autovec_for_loop(cc, mod, fn, lmap, gmap, node);',
        "new": '            if (0) wasm_try_autovec_for_loop(cc, mod, fn, lmap, gmap, node); /* MV18-04 */',
        "build": "make zcc",
        "test": "python3 -m unittest tests.test_wasm_mv02.TestWasmMV02Adversarial.test_09_arm64_riscv64_qemu_and_wasm_autovec_v128",
    },
    {
        "id": "MV19-01",
        "desc": "Standalone src/arm64_codegen.c emits stp/ldp [sp, #-aligned_stack]! without >504 range guard so large frames fail aarch64-linux-gnu-as",
        "file": "src/arm64_codegen.c",
        "old": '    if (aligned_stack <= 504) {',
        "new": '    if (1 || aligned_stack <= 504) { /* MV19-01 */',
        "build": "make zcc",
        "test": "python3 -m unittest tests.test_wasm_mv02.TestWasmMV02Adversarial.test_10_standalone_arm64_riscv_codegen_cfold_math_and_wasm_autovec_v2",
    },
    {
        "id": "MV19-02",
        "desc": "Standalone src/riscv_codegen.c sets s0 = sp + 16 so saved ra/s0 at 8(sp)/0(sp) alias -8(s0)/-16(s0) stack slots and crash on return",
        "file": "src/riscv_codegen.c",
        "old": '    riscv_asm_emit_line(buf, "\\tmv s0, sp");',
        "new": '    riscv_asm_emit_line(buf, "\\taddi s0, sp, 16"); /* MV19-02 */',
        "build": "make zcc",
        "test": "python3 -m unittest tests.test_wasm_mv02.TestWasmMV02Adversarial.test_10_standalone_arm64_riscv_codegen_cfold_math_and_wasm_autovec_v2",
    },
    {
        "id": "MV19-03",
        "desc": "part4.c eval_const_call_math omits double-precision cos() and pow() so global initializers emit .zero 8 instead of folded IEEE-754 double bits",
        "file": "part4.c",
        "old": '    if ((strcmp(elem->func_name, "pow") == 0 || strcmp(elem->func_name, "powf") == 0) &&',
        "new": '    if (strcmp(elem->func_name, "powf") == 0 && /* MV19-03 */',
        "build": "make zcc",
        "test": "python3 -m unittest tests.test_wasm_mv02.TestWasmMV02Adversarial.test_10_standalone_arm64_riscv_codegen_cfold_math_and_wasm_autovec_v2",
    },
    {
        "id": "MV19-04",
        "desc": "part6_wasm.c wasm_try_autovec_for_loop omits ND_COMPOUND_ASSIGN and bitwise XOR/AND/OR so in-place/bitwise loops fail to emit v128 opcodes",
        "file": "part6_wasm.c",
        "old": '    if ((loop_body->kind != ND_ASSIGN && loop_body->kind != ND_COMPOUND_ASSIGN) ||',
        "new": '    if (loop_body->kind != ND_ASSIGN || /* MV19-04 */',
        "build": "make zcc",
        "test": "python3 -m unittest tests.test_wasm_mv02.TestWasmMV02Adversarial.test_10_standalone_arm64_riscv_codegen_cfold_math_and_wasm_autovec_v2",
    },
    {
        "id": "MV20-01",
        "desc": "part5.c zcc_emit_bit_builtin_call omits xorl $31, %eax after bsrl %edi, %eax on x86-64 __builtin_clz, returning bit index (23) instead of leading-zero count (8)",
        "file": "part5.c",
        "old": '      fprintf(cc->out, "    testl %%%s, %%%s\\n    je .Lx86_clz32_z_%d\\n    bsrl %%%s, %%eax\\n    xorl $31, %%eax\\n    jmp .Lx86_clz32_d_%d\\n"',
        "new": '      fprintf(cc->out, "    testl %%%s, %%%s\\n    je .Lx86_clz32_z_%d\\n    bsrl %%%s, %%eax\\n    jmp .Lx86_clz32_d_%d\\n" /* MV20-01 */',
        "build": "make zcc",
        "test": "python3 -m unittest tests.test_wasm_mv02.TestWasmMV02Adversarial.test_11_bit_intrinsics_quad_isa_wasm_reduction_and_prove_section",
    },
    {
        "id": "MV20-02",
        "desc": "part5.c zcc_emit_bit_builtin_call omits rbit w0, w0 before clz w0, w0 on ARM64 __builtin_ctz, breaking Quad-ISA differential execution on qemu-aarch64-static",
        "file": "part5.c",
        "old": '      fprintf(cc->out, "    rbit w0, w0\\n    clz w0, w0\\n");',
        "new": '      fprintf(cc->out, "    clz w0, w0\\n"); /* MV20-02 */',
        "build": "make zcc",
        "test": "python3 -m unittest tests.test_wasm_mv02.TestWasmMV02Adversarial.test_11_bit_intrinsics_quad_isa_wasm_reduction_and_prove_section",
    },
    {
        "id": "MV20-03",
        "desc": "part6_wasm.c wasm_try_autovec_for_loop horizontal reduction stops at 3 lanes (lane < 3) instead of 4 lanes, dropping lane 3 from SIMD128 dot-product",
        "file": "part6_wasm.c",
        "old": '        for (lane = 0; lane < 4; lane++) {',
        "new": '        for (lane = 0; lane < 3; lane++) { /* MV20-03 */',
        "build": "make zcc",
        "test": "python3 -m unittest tests.test_wasm_mv02.TestWasmMV02Adversarial.test_11_bit_intrinsics_quad_isa_wasm_reduction_and_prove_section",
    },
    {
        "id": "MV20-04",
        "desc": "src/wasm_emit.c omits .zcc.prove W3C custom section (killed by runtime & ZCC-PROVE-11)",
        "file": "src/wasm_emit.c",
        "old": '        const char *sec_name = ".zcc.prove";',
        "new": '        const char *sec_name = ".zcc.unverified"; /* MV20-04 */',
        "build": "make zcc",
        "test": "python3 -m unittest tests.test_wasm_mv02.TestWasmMV02Adversarial.test_11_bit_intrinsics_quad_isa_wasm_reduction_and_prove_section && python3 tools/zcc_prove_01.py",
    },
    {
        "id": "MV21-01",
        "desc": "part5.c zcc_emit_bit_builtin_call omits x86-64 zero-input guard on __builtin_clz(0), leaving %eax polluted on x == 0",
        "file": "part5.c",
        "old": '                       ".Lx86_clz32_z_%d:\\n    movl $32, %%eax\\n.Lx86_clz32_d_%d:\\n", r32, r32, lbl, r32, lbl, lbl, lbl);',
        "new": '                       ".Lx86_clz32_z_%d:\\n.Lx86_clz32_d_%d:\\n", r32, r32, lbl, r32, lbl, lbl, lbl); /* MV21-01 */',
        "build": "make zcc",
        "test": "python3 -m unittest tests.test_wasm_mv02.TestWasmMV02Adversarial.test_12_harden_zero_clz_ctz_bswap64_quad_isa_and_autovec_alias_guard",
    },
    {
        "id": "MV21-02",
        "desc": "part5.c zcc_emit_bit_builtin_call omits RISC-V 64 __builtin_bswap64 lowering, causing fallback to ARM64 rev x0, x0 and assembler failure",
        "file": "part5.c",
        "old": '  if (strcmp(fn, "__builtin_bswap64") == 0 && (is_a64 || is_rv64)) {',
        "new": '  if (strcmp(fn, "__builtin_bswap64") == 0 && is_a64) { /* MV21-02 */',
        "build": "make zcc",
        "test": "python3 -m unittest tests.test_wasm_mv02.TestWasmMV02Adversarial.test_12_harden_zero_clz_ctz_bswap64_quad_isa_and_autovec_alias_guard",
    },
    {
        "id": "MV21-03",
        "desc": "part6_wasm.c omits WASM32 __builtin_bswap64 64-bit byte-reversal lowering",
        "file": "part6_wasm.c",
        "old": '                if (strcmp(bfn, "bswap64") == 0 && lmap->tmp_i64_idx >= 0) {',
        "new": '                if (0 && strcmp(bfn, "bswap64") == 0 && lmap->tmp_i64_idx >= 0) { /* MV21-03 */',
        "build": "make zcc",
        "test": "python3 -m unittest tests.test_wasm_mv02.TestWasmMV02Adversarial.test_12_harden_zero_clz_ctz_bswap64_quad_isa_and_autovec_alias_guard",
    },
    {
        "id": "MV21-04",
        "desc": "part6_wasm.c wasm_try_autovec_for_loop omits reduction symbol aliasing guard (killed by ZCC-PROVE-12)",
        "file": "part6_wasm.c",
        "old": '                if (!an || (strcmp(an, acc_name) != 0 && strcmp(an, ivar_name) != 0)) {',
        "new": '                if (1) { /* MV21-04 */',
        "build": "make zcc",
        "test": "python3 tools/zcc_prove_01.py",
    },
    {
        "id": "MV22-01",
        "desc": "part5.c zcc_emit_bit_builtin_call omits addl $1, %%eax on x86-64 __builtin_ffs, returning ctz(x) instead of ctz(x)+1",
        "file": "part5.c",
        "old": '      fprintf(cc->out, "    testl %%%s, %%%s\\n    je .Lx86_ffs32_z_%d\\n    bsfl %%%s, %%eax\\n    addl $1, %%eax\\n    jmp .Lx86_ffs32_d_%d\\n"',
        "new": '      fprintf(cc->out, "    testl %%%s, %%%s\\n    je .Lx86_ffs32_z_%d\\n    bsfl %%%s, %%eax\\n    jmp .Lx86_ffs32_d_%d\\n" /* MV22-01 */',
        "build": "make zcc",
        "test": "python3 -m unittest tests.test_wasm_mv02.TestWasmMV02Adversarial.test_13_ffs_parity_quad_isa_v128_accum_and_compound_assign",
    },
    {
        "id": "MV22-02",
        "desc": "part5.c zcc_emit_bit_builtin_call omits and w0, w0, #1 on ARM64 __builtin_parity, returning full popcount instead of parity",
        "file": "part5.c",
        "old": '      fprintf(cc->out, "    fmov s0, w0\\n    cnt v0.8b, v0.8b\\n    uaddlv h0, v0.8b\\n    fmov w0, s0\\n    and w0, w0, #1\\n");',
        "new": '      fprintf(cc->out, "    fmov s0, w0\\n    cnt v0.8b, v0.8b\\n    uaddlv h0, v0.8b\\n    fmov w0, s0\\n"); /* MV22-02 */',
        "build": "make zcc",
        "test": "python3 -m unittest tests.test_wasm_mv02.TestWasmMV02Adversarial.test_13_ffs_parity_quad_isa_v128_accum_and_compound_assign",
    },
    {
        "id": "MV22-03",
        "desc": "part4.c ND_COMPOUND_ASSIGN omits backend_ops->emit_store routing on ARM64/RISC-V 64, falling back to ARM32 str r1, [r0]",
        "file": "part4.c",
        "old": '    } else if (backend_ops && backend_ops->emit_store) {',
        "new": '    } else if (0 && backend_ops && backend_ops->emit_store) { /* MV22-03 */',
        "build": "make zcc",
        "test": "python3 -m unittest tests.test_wasm_mv02.TestWasmMV02Adversarial.test_13_ffs_parity_quad_isa_v128_accum_and_compound_assign && python3 tools/zcc_prove_01.py",
    },
    {
        "id": "MV22-04",
        "desc": "part6_wasm.c omits WASM32 __builtin_parity lowering, breaking Quad-ISA parity execution and ZCC-PROVE-13",
        "file": "part6_wasm.c",
        "old": '                if (strcmp(bfn, "parity") == 0) {',
        "new": '                if (0 && strcmp(bfn, "parity") == 0) { /* MV22-04 */',
        "build": "make zcc",
        "test": "python3 -m unittest tests.test_wasm_mv02.TestWasmMV02Adversarial.test_13_ffs_parity_quad_isa_v128_accum_and_compound_assign && python3 tools/zcc_prove_01.py",
    },
    {
        "id": "MV23-01",
        "desc": "part5.c zcc_emit_bit_builtin_call omits sarl $31 sign-masking on x86-64 __builtin_clrsb, returning wrong count on negative inputs",
        "file": "part5.c",
        "old": '      fprintf(cc->out, "    movl %%%s, %%eax\\n    sarl $31, %%eax\\n    xorl %%%s, %%eax\\n    addl %%eax, %%eax\\n    orl $1, %%eax\\n    bsrl %%eax, %%eax\\n    xorl $31, %%eax\\n", r32, r32);',
        "new": '      fprintf(cc->out, "    movl %%%s, %%eax\\n    addl %%eax, %%eax\\n    orl $1, %%eax\\n    bsrl %%eax, %%eax\\n    xorl $31, %%eax\\n", r32); /* MV23-01 */',
        "build": "make zcc",
        "test": "python3 -m unittest tests.test_wasm_mv02.TestWasmMV02Adversarial.test_14_clrsb_quad_isa_simd128_splat_and_logic_ops",
    },
    {
        "id": "MV23-02",
        "desc": "part5.c zcc_emit_bit_builtin_call emits clz w0, w0 instead of cls w0, w0 on ARM64 __builtin_clrsb",
        "file": "part5.c",
        "old": '      fprintf(cc->out, "    cls w0, w0\\n");',
        "new": '      fprintf(cc->out, "    clz w0, w0\\n"); /* MV23-02 */',
        "build": "make zcc",
        "test": "python3 -m unittest tests.test_wasm_mv02.TestWasmMV02Adversarial.test_14_clrsb_quad_isa_simd128_splat_and_logic_ops",
    },
    {
        "id": "MV23-03",
        "desc": "part6_wasm.c wasm_try_autovec_for_loop disables scalar-broadcast splat (b_scalar), breaking SIMD128 i32x4.splat emission",
        "file": "part6_wasm.c",
        "old": '        if (!b_base && rhs_op) {',
        "new": '        if (0 && !b_base && rhs_op) { /* MV23-03 */',
        "build": "make zcc",
        "test": "python3 -m unittest tests.test_wasm_mv02.TestWasmMV02Adversarial.test_14_clrsb_quad_isa_simd128_splat_and_logic_ops",
    },
    {
        "id": "MV23-04",
        "desc": "part4.c ND_LAND emits cset x0, eq instead of cset x0, ne on ARM64, inverting logical AND result and breaking ZCC-PROVE-14",
        "file": "part4.c",
        "old": '          fprintf(cc->out, "    cmp x0, #0\\n    cset x0, ne\\n");',
        "new": '          fprintf(cc->out, "    cmp x0, #0\\n    cset x0, eq\\n"); /* MV23-04 */',
        "build": "make zcc",
        "test": "python3 -m unittest tests.test_wasm_mv02.TestWasmMV02Adversarial.test_14_clrsb_quad_isa_simd128_splat_and_logic_ops && python3 tools/zcc_prove_01.py",
    },
    {
        "id": "MV24-01",
        "desc": "part5.c zcc_emit_bit_builtin_call emits rorl %cl, %eax instead of roll %cl, %eax on x86-64 __builtin_rotateleft32",
        "file": "part5.c",
        "old": '      fprintf(cc->out, "    movl %%%s, %%eax\\n    movl %%%s, %%ecx\\n    roll %%cl, %%eax\\n", r32, s32);',
        "new": '      fprintf(cc->out, "    movl %%%s, %%eax\\n    movl %%%s, %%ecx\\n    rorl %%cl, %%eax\\n", r32, s32); /* MV24-01 */',
        "build": "make zcc",
        "test": "python3 -m unittest tests.test_wasm_mv02.TestWasmMV02Adversarial.test_15_rotate_quad_isa_divmod_and_wasm_simd128_shift_div",
    },
    {
        "id": "MV24-02",
        "desc": "part5.c zcc_emit_bit_builtin_call omits neg w1, w1 on ARM64 __builtin_rotateleft32, rotating right instead of left",
        "file": "part5.c",
        "old": '      fprintf(cc->out, "    neg w1, w1\\n    ror w0, w0, w1\\n");',
        "new": '      fprintf(cc->out, "    ror w0, w0, w1\\n"); /* MV24-02 */',
        "build": "make zcc",
        "test": "python3 -m unittest tests.test_wasm_mv02.TestWasmMV02Adversarial.test_15_rotate_quad_isa_divmod_and_wasm_simd128_shift_div",
    },
    {
        "id": "MV24-03",
        "desc": "part6_wasm.c wasm_try_autovec_for_loop disables ND_SHL SIMD128 vectorization (WASM_SIMD_I32X4_SHL)",
        "file": "part6_wasm.c",
        "old": '        else if (op_kind == ND_SHL && b_scalar && !b_base) {',
        "new": '        else if (0 && op_kind == ND_SHL && b_scalar && !b_base) { /* MV24-03 */',
        "build": "make zcc",
        "test": "python3 -m unittest tests.test_wasm_mv02.TestWasmMV02Adversarial.test_15_rotate_quad_isa_divmod_and_wasm_simd128_shift_div",
    },
    {
        "id": "MV24-04",
        "desc": "part4.c ND_COMPOUND_ASSIGN ND_DIV calls emit_mod instead of emit_div on non-x86 backends, breaking /= on ARM64 & RISC-V 64 and ZCC-PROVE-15",
        "file": "part4.c",
        "old": '        if (backend_ops) {\n          backend_ops->emit_div(cc, node_type_unsigned(node));',
        "new": '        if (backend_ops) {\n          backend_ops->emit_mod(cc, node_type_unsigned(node)); /* MV24-04 */',
        "build": "make zcc",
        "test": "python3 -m unittest tests.test_wasm_mv02.TestWasmMV02Adversarial.test_15_rotate_quad_isa_divmod_and_wasm_simd128_shift_div && python3 tools/zcc_prove_01.py",
    },
    {
        "id": "MV25-01",
        "desc": "part5.c zcc_eval_const_bit_builtin disables compile-time constant folding for __builtin_bitreverse32",
        "file": "part5.c",
        "old": '  if (strcmp(fname, "__builtin_bitreverse32") == 0) {',
        "new": '  if (0 && strcmp(fname, "__builtin_bitreverse32") == 0) { /* MV25-01 */',
        "build": "make zcc",
        "test": "python3 -m unittest tests.test_wasm_mv02.TestWasmMV02Adversarial.test_16_bitreverse_quad_isa_and_const_folding",
    },
    {
        "id": "MV25-02",
        "desc": "part5.c zcc_emit_bit_builtin_call emits clz w0, w0 instead of rbit w0, w0 on ARM64 __builtin_bitreverse32",
        "file": "part5.c",
        "old": '      fprintf(cc->out, is64 ? "    rbit x0, x0\\n" : "    rbit w0, w0\\n");',
        "new": '      fprintf(cc->out, is64 ? "    rbit x0, x0\\n" : "    clz w0, w0\\n"); /* MV25-02 */',
        "build": "make zcc",
        "test": "python3 -m unittest tests.test_wasm_mv02.TestWasmMV02Adversarial.test_16_bitreverse_quad_isa_and_const_folding",
    },
    {
        "id": "MV25-03",
        "desc": "part5.c zcc_emit_bit_builtin_call omits 32-bit zero-extension on RISC-V 64 __builtin_bitreverse32",
        "file": "part5.c",
        "old": 'is64 ? 64 : 32, lbl, lbl, is64 ? "" : "    slli a0, a0, 32\\n    srli a0, a0, 32\\n");',
        "new": 'is64 ? 64 : 16, lbl, lbl, ""); /* MV25-03 */',
        "build": "make zcc",
        "test": "python3 -m unittest tests.test_wasm_mv02.TestWasmMV02Adversarial.test_16_bitreverse_quad_isa_and_const_folding",
    },
    {
        "id": "MV25-04",
        "desc": "part6_wasm.c omits WASM32 __builtin_bitreverse32 lowering, breaking Quad-ISA bitreverse execution and ZCC-PROVE-16",
        "file": "part6_wasm.c",
        "old": '                if (strcmp(bfn, "bitreverse32") == 0 && lmap->tmp_val_idx >= 0) {',
        "new": '                if (0 && strcmp(bfn, "bitreverse32") == 0 && lmap->tmp_val_idx >= 0) { /* MV25-04 */',
        "build": "make zcc",
        "test": "python3 -m unittest tests.test_wasm_mv02.TestWasmMV02Adversarial.test_16_bitreverse_quad_isa_and_const_folding && python3 tools/zcc_prove_01.py",
    },
    {
        "id": "MV26-01",
        "desc": "part6_wasm.c wasm_try_autovec_for_loop emits WASM_SIMD_I32X4_MAX_S instead of WASM_SIMD_I32X4_MIN_S on ND_LT ternary loop",
        "file": "part6_wasm.c",
        "old": '                    if (ck == ND_LT || ck == ND_LE) simd_op = is_u ? WASM_SIMD_I32X4_MIN_U : WASM_SIMD_I32X4_MIN_S;',
        "new": '                    if (ck == ND_LT || ck == ND_LE) simd_op = WASM_SIMD_I32X4_MAX_S; /* MV26-01 */',
        "build": "make zcc",
        "test": "python3 -m unittest tests.test_wasm_mv02.TestWasmMV02Adversarial.test_17_wasm_simd128_branchless_minmax_abs_autovec",
    },
    {
        "id": "MV26-02",
        "desc": "part6_wasm.c wasm_try_autovec_for_loop emits WASM_SIMD_I32X4_MIN_S instead of WASM_SIMD_I32X4_MAX_S on ND_GT ternary loop",
        "file": "part6_wasm.c",
        "old": '                    else simd_op = is_u ? WASM_SIMD_I32X4_MAX_U : WASM_SIMD_I32X4_MAX_S;',
        "new": '                    else simd_op = WASM_SIMD_I32X4_MIN_S; /* MV26-02 */',
        "build": "make zcc",
        "test": "python3 -m unittest tests.test_wasm_mv02.TestWasmMV02Adversarial.test_17_wasm_simd128_branchless_minmax_abs_autovec",
    },
    {
        "id": "MV26-03",
        "desc": "part6_wasm.c wasm_try_autovec_for_loop disables unary SIMD128 i32x4.abs recognition, breaking ZCC-PROVE-17",
        "file": "part6_wasm.c",
        "old": '                    simd_op = WASM_SIMD_I32X4_ABS;\n                    is_unary_simd = 1;',
        "new": '                    simd_op = 0; /* MV26-03 */',
        "build": "make zcc",
        "test": "python3 -m unittest tests.test_wasm_mv02.TestWasmMV02Adversarial.test_17_wasm_simd128_branchless_minmax_abs_autovec && python3 tools/zcc_prove_01.py",
    },
    {
        "id": "MV26-04",
        "desc": "part6_wasm.c wasm_try_autovec_for_loop overwrites ternary simd_op when f32_d is 0, dropping SIMD128 min/max/abs emission",
        "file": "part6_wasm.c",
        "old": '    } else if (!simd_op) {\n        if (op_kind == ND_ADD) simd_op = WASM_SIMD_I32X4_ADD;',
        "new": '    } else {\n        simd_op = 0; if (op_kind == ND_ADD) simd_op = WASM_SIMD_I32X4_ADD; /* MV26-04 */',
        "build": "make zcc",
        "test": "python3 -m unittest tests.test_wasm_mv02.TestWasmMV02Adversarial.test_17_wasm_simd128_branchless_minmax_abs_autovec",
    },
    {
        "id": "MV27-01",
        "desc": "part5.c peephole_optimize disables *3/*5/*9 -> leaq (%rax,%rax,2/4/8), %rax strength reduction, breaking ZCC-PROVE-18",
        "file": "part5.c",
        "old": '        int sc = (line_ptrs[i + 1][10] == \'3\') ? 2 : ((line_ptrs[i + 1][10] == \'5\') ? 4 : 8);\n        sprintf(line_ptrs[i], "    movq %%rax, %%r11\\n"); sprintf(line_ptrs[i + 1], "    leaq (%%rax,%%rax,%d), %%rax\\n", sc);',
        "new": '        int sc = 0; if (0) sprintf(line_ptrs[i], "    leaq (%%rax,%%rax,%d), %%rax\\n", sc); /* MV27-01 */',
        "build": "make zcc",
        "test": "python3 -m unittest tests.test_wasm_mv02.TestWasmMV02Adversarial.test_18_strength_reduction_lea_and_bswap_involution_peephole && python3 tools/zcc_prove_01.py",
    },
    {
        "id": "MV27-02",
        "desc": "part5.c peephole_optimize uses scale factor 4 instead of 2 for * 3 -> leaq, miscomputing x*3 as x*5",
        "file": "part5.c",
        "old": '        int sc = (line_ptrs[i + 1][10] == \'3\') ? 2 : ((line_ptrs[i + 1][10] == \'5\') ? 4 : 8);',
        "new": '        int sc = (line_ptrs[i + 1][10] == \'3\') ? 4 : ((line_ptrs[i + 1][10] == \'5\') ? 4 : 8); /* MV27-02 */',
        "build": "make zcc",
        "test": "python3 -m unittest tests.test_wasm_mv02.TestWasmMV02Adversarial.test_18_strength_reduction_lea_and_bswap_involution_peephole",
    },
    {
        "id": "MV27-03",
        "desc": "part5.c peephole_optimize disables back-to-back bswapq/bswapl involution elision, breaking ZCC-PROVE-18",
        "file": "part5.c",
        "old": '      if (strncmp(l1, "    bswapq %", 12) == 0 || strncmp(l1, "    bswapl %", 12) == 0) {',
        "new": '      if (0 && strncmp(l1, "    bswapq %", 12) == 0) { /* MV27-03 */',
        "build": "make zcc",
        "test": "python3 -m unittest tests.test_wasm_mv02.TestWasmMV02Adversarial.test_18_strength_reduction_lea_and_bswap_involution_peephole && python3 tools/zcc_prove_01.py",
    },
    {
        "id": "MV27-04",
        "desc": "part5.c peephole_optimize elides only the first bswap in a double-bswap pair, leaving the second byte-swap intact",
        "file": "part5.c",
        "old": '          for (int m = i; m <= k; m++) if (strncmp(line_ptrs[m], "    .", 5) != 0) line_ptrs[m][0] = 0;',
        "new": '          line_ptrs[i][0] = 0; /* MV27-04 */',
        "build": "make zcc",
        "test": "python3 -m unittest tests.test_wasm_mv02.TestWasmMV02Adversarial.test_18_strength_reduction_lea_and_bswap_involution_peephole",
    },
    {
        "id": "MV28-01",
        "desc": "part5.c zcc_emit_bit_builtin_call emits setb %al instead of seto %al on x86-64 signed overflow builtins, breaking ZCC-PROVE-19",
        "file": "part5.c",
        "old": '      fprintf(cc->out, "    movl %%%s, %%eax\\n    %s %%%s, %%eax\\n    %s %%al\\n    movzbl %%al, %%eax\\n", r32, is_sub ? "subl" : "addl", s32, is_u ? "setb" : "seto");',
        "new": '      fprintf(cc->out, "    movl %%%s, %%eax\\n    %s %%%s, %%eax\\n    setb %%al\\n    movzbl %%al, %%eax\\n", r32, is_sub ? "subl" : "addl", s32); /* MV28-01 */',
        "build": "make zcc",
        "test": "python3 -m unittest tests.test_wasm_mv02.TestWasmMV02Adversarial.test_19_checked_overflow_intrinsics_quad_isa && python3 tools/zcc_prove_01.py",
    },
    {
        "id": "MV28-02",
        "desc": "part5.c zcc_emit_bit_builtin_call emits cset w0, eq instead of cset w0, vs on ARM64 signed overflow builtins",
        "file": "part5.c",
        "old": '      fprintf(cc->out, "    %s w0, w0, w1\\n    cset w0, %s\\n", is_sub ? "subs" : "adds", is_u ? (is_sub ? "cc" : "cs") : "vs");',
        "new": '      fprintf(cc->out, "    %s w0, w0, w1\\n    cset w0, eq\\n", is_sub ? "subs" : "adds"); /* MV28-02 */',
        "build": "make zcc",
        "test": "python3 -m unittest tests.test_wasm_mv02.TestWasmMV02Adversarial.test_19_checked_overflow_intrinsics_quad_isa && python3 tools/zcc_prove_01.py",
    },
    {
        "id": "MV28-03",
        "desc": "part6_wasm.c omits WASM32 __builtin_sadd_overflow_p / __builtin_ssub_overflow_p lowering, breaking ZCC-PROVE-19",
        "file": "part6_wasm.c",
        "old": '                if (strcmp(bfn, "sadd_overflow_p") == 0 || strcmp(bfn, "ssub_overflow_p") == 0) {',
        "new": '                if (0 && strcmp(bfn, "sadd_overflow_p") == 0) { /* MV28-03 */',
        "build": "make zcc",
        "test": "python3 -m unittest tests.test_wasm_mv02.TestWasmMV02Adversarial.test_19_checked_overflow_intrinsics_quad_isa && python3 tools/zcc_prove_01.py",
    },
    {
        "id": "MV28-04",
        "desc": "src/wasm_emit.c omits ZCC-PROVE-11..19 seal in .zcc.prove custom section, breaking ZCC-PROVE-19 verification",
        "file": "src/wasm_emit.c",
        "old": '        const char *payload = "{\\"compiler\\":\\"zcc-omega\\",\\"prove\\":\\"ZCC-PROVE-11..19\\",\\"simd128\\":true,\\"quad_isa\\":true}";',
        "new": '        const char *payload = "{\\"compiler\\":\\"zcc-omega\\",\\"prove\\":\\"ZCC-PROVE-NONE\\",\\"simd128\\":true,\\"quad_isa\\":true}"; /* MV28-04 */',
        "build": "make zcc",
        "test": "python3 -m unittest tests.test_wasm_mv02.TestWasmMV02Adversarial.test_19_checked_overflow_intrinsics_quad_isa && python3 tools/zcc_prove_01.py",
    },
]


def _worker_run_mutation(task: tuple) -> tuple:
    idx, m, repo_str, stage_str, obj_meta = task
    res = run_single_mutation(
        repo=pathlib.Path(repo_str),
        rel_file=m["file"],
        old_str=m["old"],
        new_str=m["new"],
        build_cmd=m["build"],
        test_cmd=m["test"],
        mutation_id=m["id"],
        stage_dir=pathlib.Path(stage_str),
        obj_meta=obj_meta,
        baseline_ok=True,
    )
    res["description"] = m["desc"]
    return idx, res


def normalize_baseline_cmd(cmd: str) -> str:
    """Collapse test commands that differ only in their /tmp/x86_mvXX_YY output binary name."""
    return re.sub(r"/tmp/x86_mv\d+(?:_\d+|_mut)\b", "/tmp/x86_mv_base_bin", cmd)


def prewarm_baseline_tests(stage: pathlib.Path, mutations=None) -> tuple:
    """Verify each unique normalized test_cmd passes on the unmutated stage directory in isolated sandboxes."""
    mut_list = mutations if mutations is not None else LEDGER_MUTATIONS
    unique_cmds = sorted({normalize_baseline_cmd(m["test"]) for m in mut_list})

    def _check_cmd(cmd: str):
        with tempfile.TemporaryDirectory(prefix="zcc_mv02_base_wtmp_") as wtmp:
            wtmp_path = pathlib.Path(wtmp)
            bwork = wtmp_path / "repo"
            shutil.copytree(stage, bwork, symlinks=True, ignore=shutil.ignore_patterns(".obj_cache"))
            env = os.environ.copy()
            env["TMPDIR"] = str(wtmp_path)
            iso_cmd = isolate_test_cmd(cmd, wtmp_path)
            r = run(iso_cmd, bwork, env=env)
            return cmd, r.returncode, r.stdout[-800:]

    with concurrent.futures.ThreadPoolExecutor(max_workers=min(len(unique_cmds), os.cpu_count() or 8, 16)) as ex:
        for cmd, rc, out in ex.map(_check_cmd, unique_cmds):
            if rc != 0:
                return False, cmd, rc, out
    return True, "", 0, ""


def estimate_mutation_cost(m: dict) -> float:
    """Estimate execution cost for LPT (Longest-Processing-Time-First) worker scheduling."""
    t = m["test"]
    f = m["file"]
    score = 10.0 if not f.startswith("src/") else 3.0
    if "unittest" in t and "zcc_prove_01" in t:
        score += 20.0
    elif "unittest" in t:
        score += 14.0
    elif "zcc_prove_01" in t:
        score += 10.0
    return score


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", default=".")
    ap.add_argument("--file")
    ap.add_argument("--old")
    ap.add_argument("--new")
    ap.add_argument("--build", default="make zcc")
    ap.add_argument("--test")
    ap.add_argument("--run-ledger", action="store_true")
    ap.add_argument("--filter", default="", help="Optional substring filter on mutation ID or file path (e.g. MV24 or part6_wasm.c)")
    ap.add_argument("--out-json", default="")
    ap.add_argument(
        "-j",
        "--jobs",
        type=int,
        default=min(os.cpu_count() or 8, 16),
        help="Number of parallel worker processes for --run-ledger (default: up to 16 CPU cores)",
    )
    args = ap.parse_args()

    repo = pathlib.Path(args.repo).resolve()

    if args.run_ledger:
        t_start = time.monotonic()
        jobs = max(1, args.jobs)
        print(
            f"[MV-02] Initializing local /tmp stage & object cache (jobs={jobs})...",
            flush=True,
        )
        stage = get_stage_dir(repo)
        meta = _OBJ_CACHE_META
        t_stage = time.monotonic() - t_start
        cache_tag = "CACHE HIT" if meta.get("cache_hit") else "COLD BUILD"
        print(
            f"[MV-02] Stage ready in {t_stage:.2f}s [{cache_tag}] (base_sha256={meta['base_bin_hash'][:16]}...).",
            flush=True,
        )
        selected_mutations = [
            m for m in LEDGER_MUTATIONS
            if not args.filter or (args.filter in m["id"] or args.filter in m["file"])
        ]
        if not selected_mutations:
            print(f"[MV-02] ERROR: No mutations matched --filter={args.filter!r}", flush=True)
            return 1

        if meta.get("baseline_verified"):
            print(
                f"[MV-02] Baseline tests already verified for fingerprint {pathlib.Path(meta['pcache_dir']).name}.",
                flush=True,
            )
        else:
            print("[MV-02] Pre-verifying unique baseline test commands...", flush=True)
            ok, failed_cmd, rc, out = prewarm_baseline_tests(stage, selected_mutations)
            if not ok:
                print(f"[MV-02] ERROR: Baseline test command failed (exit={rc}): {failed_cmd}\n{out}", flush=True)
                return 1
            if not args.filter:
                meta_json = pathlib.Path(meta["pcache_dir"]) / "meta.json"
                if meta_json.is_file():
                    meta_json.write_text(
                        json.dumps({"base_bin_hash": meta["base_bin_hash"], "baseline_verified": True}, indent=2),
                        encoding="utf-8",
                    )
        print(
            f"[MV-02] Ready at {time.monotonic() - t_start:.2f}s. "
            f"Dispatching {len(selected_mutations)} adversarial mutations (LPT-ordered) across {jobs} worker processes...",
            flush=True,
        )

        raw_tasks = [
            (idx, m, str(repo), str(stage), meta)
            for idx, m in enumerate(selected_mutations)
        ]
        tasks = sorted(raw_tasks, key=lambda t: estimate_mutation_cost(t[1]), reverse=True)
        ordered_results = [None] * len(selected_mutations)
        all_killed = True

        if jobs == 1:
            for task in tasks:
                idx, res = _worker_run_mutation(task)
                ordered_results[idx] = res
                m = selected_mutations[idx]
                print(
                    f"  -> [{idx + 1:02d}/{len(selected_mutations)}] {m['id']}: status={res['status']} "
                    f"baseline_exit={res.get('baseline_exit')} mut_build_exit={res.get('mutated_build_exit')} "
                    f"mut_test_exit={res.get('mutated_test_exit')} ({res.get('elapsed_sec', 0):.2f}s)",
                    flush=True,
                )
                if not res.get("killed", False):
                    all_killed = False
        else:
            completed_count = 0
            with concurrent.futures.ProcessPoolExecutor(max_workers=jobs) as pool:
                future_map = {pool.submit(_worker_run_mutation, t): t[0] for t in tasks}
                for fut in concurrent.futures.as_completed(future_map):
                    idx, res = fut.result()
                    ordered_results[idx] = res
                    completed_count += 1
                    m = selected_mutations[idx]
                    print(
                        f"  -> [{completed_count:02d}/{len(selected_mutations)}] {m['id']} ({m['file']}): "
                        f"status={res['status']} baseline_exit={res.get('baseline_exit')} "
                        f"mut_build_exit={res.get('mutated_build_exit')} mut_test_exit={res.get('mutated_test_exit')} "
                        f"({res.get('elapsed_sec', 0):.2f}s)",
                        flush=True,
                    )
                    if not res.get("killed", False):
                        all_killed = False
                        if res.get("output"):
                            print(f"     [BUILD/TEST OUTPUT]: {res['output'][:400]}", flush=True)

        results = ordered_results
        if args.out_json:
            out_p = pathlib.Path(args.out_json)
            out_p.parent.mkdir(parents=True, exist_ok=True)
            out_p.write_text(json.dumps(results, indent=2), encoding="utf-8")
        total_elapsed = time.monotonic() - t_start
        killed_cnt = sum(1 for r in results if r and r.get("killed"))
        print(
            f"[MV-02] Ledger summary: {killed_cnt}/{len(results)} mutations killed "
            f"in {total_elapsed:.2f}s ({jobs} workers).",
            flush=True,
        )
        shutil.rmtree(stage, ignore_errors=True)
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
