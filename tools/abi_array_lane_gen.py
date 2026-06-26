#!/usr/bin/env python3
"""
ABI-lane differential fuzzer for array-in-struct classification.

Generates System V AMD64 struct-with-array FFI probe pairs and runs a
two-direction GCC/ZCC differential matrix in isolated subdirectories.
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


SHAPES: tuple[Shape, ...] = (
    # --- arr_f32x2_one_eightbyte ---
    Shape(
        name="arr_f32x2_one_eightbyte_arg",
        category="argument_passing",
        struct_body="""
            struct S {
                float v[2];
            };
        """,
        init="{ { 1.5f, -2.5f } }",
        checks='printf("v0=%.9g v1=%.9g\\n", s.v[0], s.v[1]);',
        expected="v0=1.5 v1=-2.5",
        is_return=False,
    ),
    Shape(
        name="arr_f32x2_one_eightbyte_ret",
        category="return_values",
        struct_body="""
            struct S {
                float v[2];
            };
        """,
        init="{ { 1.5f, -2.5f } }",
        checks='printf("v0=%.9g v1=%.9g\\n", s.v[0], s.v[1]);',
        expected="v0=1.5 v1=-2.5",
        is_return=True,
    ),

    # --- arr_f32x4_two_eightbytes ---
    Shape(
        name="arr_f32x4_two_eightbytes_arg",
        category="argument_passing",
        struct_body="""
            struct S {
                float v[4];
            };
        """,
        init="{ { 1.25f, -2.25f, 3.25f, -4.25f } }",
        checks='printf("v0=%.9g v1=%.9g v2=%.9g v3=%.9g\\n", s.v[0], s.v[1], s.v[2], s.v[3]);',
        expected="v0=1.25 v1=-2.25 v2=3.25 v3=-4.25",
        is_return=False,
    ),
    Shape(
        name="arr_f32x4_two_eightbytes_ret",
        category="return_values",
        struct_body="""
            struct S {
                float v[4];
            };
        """,
        init="{ { 1.25f, -2.25f, 3.25f, -4.25f } }",
        checks='printf("v0=%.9g v1=%.9g v2=%.9g v3=%.9g\\n", s.v[0], s.v[1], s.v[2], s.v[3]);',
        expected="v0=1.25 v1=-2.25 v2=3.25 v3=-4.25",
        is_return=True,
    ),

    # --- arr_f64x2_two_eightbytes ---
    Shape(
        name="arr_f64x2_two_eightbytes_arg",
        category="argument_passing",
        struct_body="""
            struct S {
                double v[2];
            };
        """,
        init="{ { 10.5, -20.5 } }",
        checks='printf("v0=%.17g v1=%.17g\\n", s.v[0], s.v[1]);',
        expected="v0=10.5 v1=-20.5",
        is_return=False,
    ),
    Shape(
        name="arr_f64x2_two_eightbytes_ret",
        category="return_values",
        struct_body="""
            struct S {
                double v[2];
            };
        """,
        init="{ { 10.5, -20.5 } }",
        checks='printf("v0=%.17g v1=%.17g\\n", s.v[0], s.v[1]);',
        expected="v0=10.5 v1=-20.5",
        is_return=True,
    ),

    # --- arr_i32x2_one_eightbyte ---
    Shape(
        name="arr_i32x2_one_eightbyte_arg",
        category="argument_passing",
        struct_body="""
            struct S {
                int v[2];
            };
        """,
        init="{ { 0x11223344, 0x55667788 } }",
        checks='printf("v0=%08x v1=%08x\\n", (unsigned)s.v[0], (unsigned)s.v[1]);',
        expected="v0=11223344 v1=55667788",
        is_return=False,
    ),
    Shape(
        name="arr_i32x2_one_eightbyte_ret",
        category="return_values",
        struct_body="""
            struct S {
                int v[2];
            };
        """,
        init="{ { 0x11223344, 0x55667788 } }",
        checks='printf("v0=%08x v1=%08x\\n", (unsigned)s.v[0], (unsigned)s.v[1]);',
        expected="v0=11223344 v1=55667788",
        is_return=True,
    ),

    # --- arr_i32x4_two_eightbytes ---
    Shape(
        name="arr_i32x4_two_eightbytes_arg",
        category="argument_passing",
        struct_body="""
            struct S {
                int v[4];
            };
        """,
        init="{ { 0x11111111, 0x22222222, 0x33333333, 0x44444444 } }",
        checks='printf("v0=%08x v1=%08x v2=%08x v3=%08x\\n", (unsigned)s.v[0], (unsigned)s.v[1], (unsigned)s.v[2], (unsigned)s.v[3]);',
        expected="v0=11111111 v1=22222222 v2=33333333 v3=44444444",
        is_return=False,
    ),
    Shape(
        name="arr_i32x4_two_eightbytes_ret",
        category="return_values",
        struct_body="""
            struct S {
                int v[4];
            };
        """,
        init="{ { 0x11111111, 0x22222222, 0x33333333, 0x44444444 } }",
        checks='printf("v0=%08x v1=%08x v2=%08x v3=%08x\\n", (unsigned)s.v[0], (unsigned)s.v[1], (unsigned)s.v[2], (unsigned)s.v[3]);',
        expected="v0=11111111 v1=22222222 v2=33333333 v3=44444444",
        is_return=True,
    ),

    # --- arr_i64x3_memory ---
    Shape(
        name="arr_i64x3_memory_arg",
        category="argument_passing",
        struct_body="""
            struct S {
                long long v[3];
            };
        """,
        init="{ { 0x1122334455667788LL, 0x99aabbccddeeff00LL, 0x123456789abcdefLL } }",
        checks='printf("v0=%016llx v1=%016llx v2=%016llx\\n", (unsigned long long)s.v[0], (unsigned long long)s.v[1], (unsigned long long)s.v[2]);',
        expected="v0=1122334455667788 v1=99aabbccddeeff00 v2=0123456789abcdef",
        is_return=False,
    ),
    Shape(
        name="arr_i64x3_memory_ret",
        category="return_values",
        struct_body="""
            struct S {
                long long v[3];
            };
        """,
        init="{ { 0x1122334455667788LL, 0x99aabbccddeeff00LL, 0x123456789abcdefLL } }",
        checks='printf("v0=%016llx v1=%016llx v2=%016llx\\n", (unsigned long long)s.v[0], (unsigned long long)s.v[1], (unsigned long long)s.v[2]);',
        expected="v0=1122334455667788 v1=99aabbccddeeff00 v2=0123456789abcdef",
        is_return=True,
    ),

    # --- arr_f64x3_memory ---
    Shape(
        name="arr_f64x3_memory_arg",
        category="argument_passing",
        struct_body="""
            struct S {
                double v[3];
            };
        """,
        init="{ { 1.5, 2.5, 3.5 } }",
        checks='printf("v0=%.17g v1=%.17g v2=%.17g\\n", s.v[0], s.v[1], s.v[2]);',
        expected="v0=1.5 v1=2.5 v2=3.5",
        is_return=False,
    ),
    Shape(
        name="arr_f64x3_memory_ret",
        category="return_values",
        struct_body="""
            struct S {
                double v[3];
            };
        """,
        init="{ { 1.5, 2.5, 3.5 } }",
        checks='printf("v0=%.17g v1=%.17g v2=%.17g\\n", s.v[0], s.v[1], s.v[2]);',
        expected="v0=1.5 v1=2.5 v2=3.5",
        is_return=True,
    ),

    # --- arr_i32x3_two_eightbytes ---
    Shape(
        name="arr_i32x3_two_eightbytes_arg",
        category="argument_passing",
        struct_body="""
            struct S {
                int v[3];
            };
        """,
        init="{ { 0x11111111, 0x22222222, 0x33333333 } }",
        checks='printf("v0=%08x v1=%08x v2=%08x\\n", (unsigned)s.v[0], (unsigned)s.v[1], (unsigned)s.v[2]);',
        expected="v0=11111111 v1=22222222 v2=33333333",
        is_return=False,
    ),
    Shape(
        name="arr_i32x3_two_eightbytes_ret",
        category="return_values",
        struct_body="""
            struct S {
                int v[3];
            };
        """,
        init="{ { 0x11111111, 0x22222222, 0x33333333 } }",
        checks='printf("v0=%08x v1=%08x v2=%08x\\n", (unsigned)s.v[0], (unsigned)s.v[1], (unsigned)s.v[2]);',
        expected="v0=11111111 v1=22222222 v2=33333333",
        is_return=True,
    ),

    # --- mixed_scalar_then_arr ---
    Shape(
        name="mixed_scalar_then_arr_arg",
        category="argument_passing",
        struct_body="""
            struct S {
                int a;
                float v[2];
            };
        """,
        init="{ 0x12345678, { 1.5f, -2.5f } }",
        checks='printf("a=%08x v0=%.9g v1=%.9g\\n", (unsigned)s.a, s.v[0], s.v[1]);',
        expected="a=12345678 v0=1.5 v1=-2.5",
        is_return=False,
    ),
    Shape(
        name="mixed_scalar_then_arr_ret",
        category="return_values",
        struct_body="""
            struct S {
                int a;
                float v[2];
            };
        """,
        init="{ 0x12345678, { 1.5f, -2.5f } }",
        checks='printf("a=%08x v0=%.9g v1=%.9g\\n", (unsigned)s.a, s.v[0], s.v[1]);',
        expected="a=12345678 v0=1.5 v1=-2.5",
        is_return=True,
    ),

    # --- mixed_arr_then_scalar ---
    Shape(
        name="mixed_arr_then_scalar_arg",
        category="argument_passing",
        struct_body="""
            struct S {
                float v[2];
                int b;
            };
        """,
        init="{ { -1.5f, 2.5f }, 0x6a5a1234 }",
        checks='printf("v0=%.9g v1=%.9g b=%08x\\n", s.v[0], s.v[1], (unsigned)s.b);',
        expected="v0=-1.5 v1=2.5 b=6a5a1234",
        is_return=False,
    ),
    Shape(
        name="mixed_arr_then_scalar_ret",
        category="return_values",
        struct_body="""
            struct S {
                float v[2];
                int b;
            };
        """,
        init="{ { -1.5f, 2.5f }, 0x6a5a1234 }",
        checks='printf("v0=%.9g v1=%.9g b=%08x\\n", s.v[0], s.v[1], (unsigned)s.b);',
        expected="v0=-1.5 v1=2.5 b=6a5a1234",
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


def build_and_run(case: pathlib.Path, gcc: str, zcc: str, timeout: float) -> dict[str, Any]:
    matrix = {
        "gcc_driver_zcc_callee": {
            "steps": [
                ("driver_o", [gcc, "-c", "driver.c", "-o", "driver.gcc.o"]),
                ("callee_o", [zcc, "-c", "callee.c", "-o", "callee.zcc.o"]),
                ("link", [gcc, "driver.gcc.o", "callee.zcc.o", "-o", "gcc_driver_zcc_callee.bin"]),
            ],
            "bin": "./gcc_driver_zcc_callee.bin",
        },
        "zcc_driver_gcc_callee": {
            "steps": [
                ("driver_o", [zcc, "-c", "driver.c", "-o", "driver.zcc.o"]),
                ("callee_o", [gcc, "-c", "callee.c", "-o", "callee.gcc.o"]),
                ("link", [gcc, "driver.zcc.o", "callee.gcc.o", "-o", "zcc_driver_gcc_callee.bin"]),
            ],
            "bin": "./zcc_driver_gcc_callee.bin",
        },
    }

    results: dict[str, Any] = {}
    for name, spec in matrix.items():
        log: list[dict[str, Any]] = []
        ok = True

        # Create isolated subdirectory for the lane
        lane_dir = case / name
        if lane_dir.exists():
            shutil.rmtree(lane_dir)
        lane_dir.mkdir(parents=True, exist_ok=True)

        # Copy source files
        def _copy_file(src: pathlib.Path, dst: pathlib.Path):
            dst.write_bytes(src.read_bytes())

        _copy_file(case / "case.h", lane_dir / "case.h")
        _copy_file(case / "driver.c", lane_dir / "driver.c")
        _copy_file(case / "callee.c", lane_dir / "callee.c")

        for step, cmd in spec["steps"]:
            try:
                proc = run(cmd, lane_dir, timeout)
                entry = {
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

        results[name] = {"ok": ok, "output": output, "log": log}

    return results


def write_bug_template(case: pathlib.Path, shape: Shape, result: dict[str, Any], ticket: str) -> None:
    matrix = result["results"]
    template = f"""\
## {ticket}: {shape.name} array-in-struct mismatch

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

Artifacts:

* `case.h`
* `driver.c`
* `callee.c`
* `REPRO.txt`

Notes:
This is exploratory ABI-lane discovery, not regression closure. Promote this into
a dedicated regression only after root-cause repair and independent validation.
"""
    (case / f"{ticket}.md").write_text(template, encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate and run ABI array-in-struct differential FFI probes.")
    parser.add_argument("--out", default="abi_array_cases", help="case output directory")
    parser.add_argument("--gcc", default="gcc", help="reference compiler/linker")
    parser.add_argument("--zcc", default="./zcc", help="compiler under test")
    parser.add_argument("--run", action="store_true", help="compile and run GCC/ZCC differential matrix")
    parser.add_argument("--timeout", type=float, default=10.0, help="per-command timeout in seconds")
    parser.add_argument("--ticket-prefix", default="CG-ARR", help="bug-ticket prefix for generated mismatch templates")
    args = parser.parse_args()

    out = pathlib.Path(args.out)
    out.mkdir(parents=True, exist_ok=True)

    by_name = {shape.name: shape for shape in SHAPES}
    for shape in SHAPES:
        emit_case(out, shape)

    if not args.run:
        print(f"generated {len(SHAPES)} ABI array cases under {out}")
        return 0

    require_tool(args.gcc)
    require_tool(args.zcc)

    summary: dict[str, Any] = {"tested": 0, "passed": 0, "failed": 0, "cases": {}}

    for case in sorted(p for p in out.iterdir() if p.is_dir()):
        shape = by_name[case.name]
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
