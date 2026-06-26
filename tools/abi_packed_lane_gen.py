#!/usr/bin/env python3
"""
ABI-lane differential fuzzer for packed-struct classification.

Generates System V AMD64 __attribute__((packed)) struct FFI probe pairs and runs
a two-direction GCC/ZCC differential matrix in isolated subdirectories.

The central question: does ZCC correctly apply the unaligned-field -> MEMORY
demotion rule, or does it mis-classify packed structs into registers?

Mirrors tools/abi_array_lane_gen.py exactly — do not merge or refactor either.
"""

from __future__ import annotations

import argparse
import json
import os
import pathlib
import shutil
import subprocess
import textwrap
from dataclasses import asdict, dataclass
from typing import Any


@dataclass(frozen=True)
class Shape:
    name: str
    category: str
    struct_body: str
    init: str
    checks: str
    expected: str
    is_return: bool  # True if testing return value, False if testing argument passing


def _body(src: str) -> str:
    return textwrap.dedent(src).strip()


# ---------------------------------------------------------------------------
# Shape matrix — 6 packed shapes x {arg, ret} = 12 entries.
#
# ABI rule pinned in plan:
#   Any struct with an unaligned field -> entire struct is class MEMORY.
#   MEMORY return:  caller passes hidden pointer in %rdi; callee writes through it;
#                   %rax returns the pointer at exit.
#   MEMORY arg:     struct copied to stack; callee receives via stack frame.
#   Control (pk_two_int_aligned): int@0 and int@4 are both naturally aligned;
#   packed is a no-op; struct stays INTEGER (%rdi / %rax).
# ---------------------------------------------------------------------------

SHAPES: tuple[Shape, ...] = (

    # --- pk_char_int: { char c; int v; } packed — v@1 unaligned -> MEMORY ---
    Shape(
        name="pk_char_int_arg",
        category="argument_passing",
        struct_body="""
            struct __attribute__((packed)) S {
                char c;
                int v;
            };
        """,
        init="{1, 42}",
        checks='printf("c=%02x v=%08x\\n", (unsigned char)s.c, (unsigned)s.v);',
        expected="c=01 v=0000002a",
        is_return=False,
    ),
    Shape(
        name="pk_char_int_ret",
        category="return_values",
        struct_body="""
            struct __attribute__((packed)) S {
                char c;
                int v;
            };
        """,
        init="{1, 42}",
        checks='printf("c=%02x v=%08x\\n", (unsigned char)s.c, (unsigned)s.v);',
        expected="c=01 v=0000002a",
        is_return=True,
    ),

    # --- pk_char_i64: { char c; long long v; } packed — v@1 unaligned -> MEMORY ---
    Shape(
        name="pk_char_i64_arg",
        category="argument_passing",
        struct_body="""
            struct __attribute__((packed)) S {
                char c;
                long long v;
            };
        """,
        init="{1, 99LL}",
        checks='printf("c=%02x v=%016llx\\n", (unsigned char)s.c, (unsigned long long)s.v);',
        expected="c=01 v=0000000000000063",
        is_return=False,
    ),
    Shape(
        name="pk_char_i64_ret",
        category="return_values",
        struct_body="""
            struct __attribute__((packed)) S {
                char c;
                long long v;
            };
        """,
        init="{1, 99LL}",
        checks='printf("c=%02x v=%016llx\\n", (unsigned char)s.c, (unsigned long long)s.v);',
        expected="c=01 v=0000000000000063",
        is_return=True,
    ),

    # --- pk_char_float: { char c; float f; } packed — f@1 unaligned (SSE -> MEMORY) ---
    Shape(
        name="pk_char_float_arg",
        category="argument_passing",
        struct_body="""
            struct __attribute__((packed)) S {
                char c;
                float f;
            };
        """,
        init="{1, 1.5f}",
        checks='printf("c=%02x f=%.9g\\n", (unsigned char)s.c, (double)s.f);',
        expected="c=01 f=1.5",
        is_return=False,
    ),
    Shape(
        name="pk_char_float_ret",
        category="return_values",
        struct_body="""
            struct __attribute__((packed)) S {
                char c;
                float f;
            };
        """,
        init="{1, 1.5f}",
        checks='printf("c=%02x f=%.9g\\n", (unsigned char)s.c, (double)s.f);',
        expected="c=01 f=1.5",
        is_return=True,
    ),

    # --- pk_char_int_arr3: { char c; int v[3]; } packed
    #     v at offset 1; v[1] straddles byte 8 (offsets 5-8) — TRUE STRADDLE -> MEMORY ---
    Shape(
        name="pk_char_int_arr3_arg",
        category="argument_passing",
        struct_body="""
            struct __attribute__((packed)) S {
                char c;
                int v[3];
            };
        """,
        init="{1, {2, 3, 4}}",
        checks='printf("c=%02x v0=%08x v1=%08x v2=%08x\\n", (unsigned char)s.c, (unsigned)s.v[0], (unsigned)s.v[1], (unsigned)s.v[2]);',
        expected="c=01 v0=00000002 v1=00000003 v2=00000004",
        is_return=False,
    ),
    Shape(
        name="pk_char_int_arr3_ret",
        category="return_values",
        struct_body="""
            struct __attribute__((packed)) S {
                char c;
                int v[3];
            };
        """,
        init="{1, {2, 3, 4}}",
        checks='printf("c=%02x v0=%08x v1=%08x v2=%08x\\n", (unsigned char)s.c, (unsigned)s.v[0], (unsigned)s.v[1], (unsigned)s.v[2]);',
        expected="c=01 v0=00000002 v1=00000003 v2=00000004",
        is_return=True,
    ),

    # --- pk_int_char_int: { int a; char c; int b; } packed — b@5 unaligned -> MEMORY ---
    Shape(
        name="pk_int_char_int_arg",
        category="argument_passing",
        struct_body="""
            struct __attribute__((packed)) S {
                int a;
                char c;
                int b;
            };
        """,
        init="{1, 2, 3}",
        checks='printf("a=%08x c=%02x b=%08x\\n", (unsigned)s.a, (unsigned char)s.c, (unsigned)s.b);',
        expected="a=00000001 c=02 b=00000003",
        is_return=False,
    ),
    Shape(
        name="pk_int_char_int_ret",
        category="return_values",
        struct_body="""
            struct __attribute__((packed)) S {
                int a;
                char c;
                int b;
            };
        """,
        init="{1, 2, 3}",
        checks='printf("a=%08x c=%02x b=%08x\\n", (unsigned)s.a, (unsigned char)s.c, (unsigned)s.b);',
        expected="a=00000001 c=02 b=00000003",
        is_return=True,
    ),

    # --- pk_two_int_aligned: { int a; int b; } packed — CONTROL
    #     int@0 and int@4 both naturally aligned; packed is no-op -> INTEGER ---
    Shape(
        name="pk_two_int_aligned_arg",
        category="argument_passing",
        struct_body="""
            struct __attribute__((packed)) S {
                int a;
                int b;
            };
        """,
        init="{1, 2}",
        checks='printf("a=%08x b=%08x\\n", (unsigned)s.a, (unsigned)s.b);',
        expected="a=00000001 b=00000002",
        is_return=False,
    ),
    Shape(
        name="pk_two_int_aligned_ret",
        category="return_values",
        struct_body="""
            struct __attribute__((packed)) S {
                int a;
                int b;
            };
        """,
        init="{1, 2}",
        checks='printf("a=%08x b=%08x\\n", (unsigned)s.a, (unsigned)s.b);',
        expected="a=00000001 b=00000002",
        is_return=True,
    ),
)


def emit_case(out: pathlib.Path, shape: Shape) -> None:
    case_dir = out / shape.name
    case_dir.mkdir(parents=True, exist_ok=True)

    if shape.is_return:
        common_h = f"""\
#ifndef ABI_CASE_H
#define ABI_CASE_H

#include <stdio.h>
#include <stdint.h>

{_body(shape.struct_body)}

struct S callee(void);

#endif
"""

        driver_c = f"""\
#include "case.h"

int main(void) {{
    struct S s = callee();
    {shape.checks}
    return 0;
}}
"""

        callee_c = f"""\
#include "case.h"

struct S callee(void) {{
    struct S s = {shape.init};
    return s;
}}
"""
    else:
        common_h = f"""\
#ifndef ABI_CASE_H
#define ABI_CASE_H

#include <stdio.h>
#include <stdint.h>

{_body(shape.struct_body)}

void callee(struct S s);

#endif
"""

        driver_c = f"""\
#include "case.h"

int main(void) {{
    struct S s = {shape.init};
    callee(s);
    return 0;
}}
"""

        callee_c = f"""\
#include "case.h"

void callee(struct S s) {{
    {shape.checks}
}}
"""

    (case_dir / "case.h").write_text(common_h, encoding="utf-8")
    (case_dir / "driver.c").write_text(driver_c, encoding="utf-8")
    (case_dir / "callee.c").write_text(callee_c, encoding="utf-8")
    (case_dir / "meta.json").write_text(json.dumps(asdict(shape), indent=2) + "\n", encoding="utf-8")


def run(cmd: list[str], cwd: pathlib.Path, timeout: float) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        cmd,
        cwd=cwd,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        timeout=timeout,
    )


def require_tool(tool: str) -> None:
    if os.sep in tool or (os.altsep and os.altsep in tool):
        if not pathlib.Path(tool).exists():
            raise FileNotFoundError(f"tool not found: {tool}")
        return
    if shutil.which(tool) is None:
        raise FileNotFoundError(f"tool not found on PATH: {tool}")


def capture_objdump(obj: pathlib.Path, out_file: pathlib.Path, timeout: float) -> str:
    """Run objdump -d on an object file; write to out_file; return the text."""
    try:
        proc = subprocess.run(
            ["objdump", "-d", str(obj)],
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=timeout,
        )
        asm = proc.stdout
    except (subprocess.TimeoutExpired, FileNotFoundError) as exc:
        asm = f"objdump failed: {exc}\n"
    out_file.write_text(asm, encoding="utf-8")
    return asm


def build_and_run(case: pathlib.Path, gcc: str, zcc: str, timeout: float) -> dict[str, Any]:
    # Lane 1: GCC driver, ZCC callee — checks ZCC callee codegen / call-site handling by GCC
    # Lane 2: ZCC driver, GCC callee — checks ZCC caller codegen against GCC callee
    matrix = {
        "gcc_driver_zcc_callee": {
            "steps": [
                ("driver_o",  [gcc, "-c", "driver.c",  "-o", "driver.gcc.o"]),
                ("callee_o",  [zcc, "-c", "callee.c",  "-o", "callee.zcc.o"]),
                ("link",      [gcc, "driver.gcc.o", "callee.zcc.o", "-o", "gcc_driver_zcc_callee.bin"]),
            ],
            "bin": "./gcc_driver_zcc_callee.bin",
            "zcc_obj": "callee.zcc.o",
            "gcc_obj": "driver.gcc.o",
        },
        "zcc_driver_gcc_callee": {
            "steps": [
                ("driver_o",  [zcc, "-c", "driver.c",  "-o", "driver.zcc.o"]),
                ("callee_o",  [gcc, "-c", "callee.c",  "-o", "callee.gcc.o"]),
                ("link",      [gcc, "driver.zcc.o", "callee.gcc.o", "-o", "zcc_driver_gcc_callee.bin"]),
            ],
            "bin": "./zcc_driver_gcc_callee.bin",
            "zcc_obj": "driver.zcc.o",
            "gcc_obj": "callee.gcc.o",
        },
    }

    results: dict[str, Any] = {}
    for name, spec in matrix.items():
        log: list[dict[str, Any]] = []
        ok = True

        # Isolated subdirectory — load-bearing: prevents stale-object cross-lane link failures.
        lane_dir = case / name
        if lane_dir.exists():
            shutil.rmtree(lane_dir)
        lane_dir.mkdir(parents=True, exist_ok=True)

        # Copy source files into the isolated lane.
        def _copy_file(src: pathlib.Path, dst: pathlib.Path):
            dst.write_bytes(src.read_bytes())

        _copy_file(case / "case.h",    lane_dir / "case.h")
        _copy_file(case / "driver.c",  lane_dir / "driver.c")
        _copy_file(case / "callee.c",  lane_dir / "callee.c")

        compiled_zcc_obj: pathlib.Path | None = None
        compiled_gcc_obj: pathlib.Path | None = None

        for step, cmd in spec["steps"]:
            try:
                proc = run(cmd, lane_dir, timeout)
                entry: dict[str, Any] = {
                    "step": step,
                    "cmd": cmd,
                    "returncode": proc.returncode,
                    "stdout": proc.stdout,
                    "stderr": proc.stderr,
                }
            except subprocess.TimeoutExpired as exc:
                entry = {
                    "step": step,
                    "cmd": cmd,
                    "returncode": None,
                    "stdout": exc.stdout or "",
                    "stderr": f"timeout after {timeout}s\n{exc.stderr or ''}",
                }
                ok = False
            log.append(entry)
            if entry["returncode"] != 0:
                ok = False
                break

            # Track compiled objects for objdump (capture even on success/failure).
            if step == "callee_o":
                zcc_obj_path = lane_dir / spec["zcc_obj"]
                gcc_obj_path = lane_dir / spec["gcc_obj"]
                if "callee.zcc.o" in spec["zcc_obj"] and zcc_obj_path.exists():
                    compiled_zcc_obj = zcc_obj_path
                if "callee.gcc.o" in spec["gcc_obj"] and gcc_obj_path.exists():
                    compiled_gcc_obj = gcc_obj_path
            if step == "driver_o":
                zcc_obj_path = lane_dir / spec["zcc_obj"]
                gcc_obj_path = lane_dir / spec["gcc_obj"]
                if "driver.zcc.o" in spec["zcc_obj"] and zcc_obj_path.exists():
                    compiled_zcc_obj = zcc_obj_path
                if "driver.gcc.o" in spec["gcc_obj"] and gcc_obj_path.exists():
                    compiled_gcc_obj = gcc_obj_path

        # Capture objdump for ALL cases — disassembly is the load-bearing evidence.
        zcc_asm = ""
        gcc_asm = ""
        if compiled_zcc_obj and compiled_zcc_obj.exists():
            zcc_asm = capture_objdump(compiled_zcc_obj, lane_dir / "callee_or_driver.zcc.asm", timeout)
        if compiled_gcc_obj and compiled_gcc_obj.exists():
            gcc_asm = capture_objdump(compiled_gcc_obj, lane_dir / "callee_or_driver.gcc.asm", timeout)

        output = ""
        if ok:
            try:
                proc = run([spec["bin"]], lane_dir, timeout)
                output = proc.stdout.strip()
                log.append({
                    "step": "run",
                    "cmd": [spec["bin"]],
                    "returncode": proc.returncode,
                    "stdout": proc.stdout,
                    "stderr": proc.stderr,
                })
                ok = proc.returncode == 0
            except subprocess.TimeoutExpired as exc:
                log.append({
                    "step": "run",
                    "cmd": [spec["bin"]],
                    "returncode": None,
                    "stdout": exc.stdout or "",
                    "stderr": f"timeout after {timeout}s\n{exc.stderr or ''}",
                })
                ok = False

        results[name] = {
            "ok": ok,
            "output": output,
            "log": log,
            "zcc_asm": zcc_asm,
            "gcc_asm": gcc_asm,
        }

    return results


def write_bug_template(case: pathlib.Path, shape: Shape, result: dict[str, Any], ticket: str) -> None:
    matrix = result["results"]
    template = f"""\
## {ticket}: {shape.name} packed-struct mismatch

Status: OPEN

Category: {shape.category}

Shape:

```c
{_body(shape.struct_body)}
```

Matrix:

* GCC driver + ZCC callee: `{matrix["gcc_driver_zcc_callee"]["output"]}`
* ZCC driver + GCC callee: `{matrix["zcc_driver_gcc_callee"]["output"]}`

Expected:

```text
{shape.expected}
```

Disassembly (ZCC callee/driver — gcc_driver_zcc_callee lane):

```asm
{matrix["gcc_driver_zcc_callee"].get("zcc_asm", "(not captured)")}
```

ABI Classification Expected: MEMORY (unaligned field demotes to stack/sret)
Check: does ZCC callee use hidden-ptr in %rdi (return) or stack-receive (arg)?

Artifacts:

* `case.h`
* `driver.c`
* `callee.c`
* `REPRO.txt`
* `gcc_driver_zcc_callee/callee_or_driver.zcc.asm`
* `zcc_driver_gcc_callee/callee_or_driver.zcc.asm`

Notes:
This is exploratory ABI-lane discovery. A MEMORY-expected case returning in registers
is an ABI conformance violation (CG-PACKED-NNN). Do not patch until root cause is confirmed.
"""
    (case / f"{ticket}.md").write_text(template, encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Generate and run ABI packed-struct differential FFI probes."
    )
    parser.add_argument("--out", default="abi_packed_cases", help="case output directory")
    parser.add_argument("--gcc", default="gcc", help="reference compiler/linker")
    parser.add_argument("--zcc", default="./zcc", help="compiler under test")
    parser.add_argument("--run", action="store_true", help="compile and run GCC/ZCC differential matrix")
    parser.add_argument("--timeout", type=float, default=10.0, help="per-command timeout in seconds")
    parser.add_argument("--ticket-prefix", default="CG-PACKED", help="bug-ticket prefix for mismatch templates")
    args = parser.parse_args()

    out = pathlib.Path(args.out)
    out.mkdir(parents=True, exist_ok=True)

    by_name = {shape.name: shape for shape in SHAPES}
    for shape in SHAPES:
        emit_case(out, shape)

    if not args.run:
        print(f"generated {len(SHAPES)} ABI packed cases under {out}")
        return 0

    require_tool(args.gcc)
    require_tool(args.zcc)

    summary: dict[str, Any] = {"tested": 0, "passed": 0, "failed": 0, "cases": {}}

    for case in sorted(p for p in out.iterdir() if p.is_dir()):
        shape = by_name.get(case.name)
        if shape is None:
            continue
        result = build_and_run(case, args.gcc, args.zcc, args.timeout)

        outputs = [lane["output"] for lane in result.values() if lane["ok"]]
        passed = (
            all(lane["ok"] for lane in result.values())
            and len(outputs) == 2
            and outputs[0] == outputs[1] == shape.expected
        )

        summary["tested"] += 1
        summary["passed" if passed else "failed"] += 1
        summary["cases"][case.name] = {
            "category": shape.category,
            "expected": shape.expected,
            "passed": passed,
            "results": result,
        }

        verdict = "PASS" if passed else "FAIL"
        print(f"{verdict} {case.name}")

        if not passed:
            repro = case / "REPRO.txt"
            repro.write_text(json.dumps(summary["cases"][case.name], indent=2) + "\n", encoding="utf-8")
            write_bug_template(
                case=case,
                shape=shape,
                result=summary["cases"][case.name],
                ticket=f"{args.ticket_prefix}-{summary['failed']:03d}",
            )

    (out / "summary.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    print(
        f"N shapes tested: {summary['tested']}, "
        f"M passed: {summary['passed']}, "
        f"K failed: {summary['failed']}"
    )

    return 0 if summary["failed"] == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
