#!/usr/bin/env python3
"""
ABI-lane differential fuzzer for return values.

Generates System V AMD64 struct-by-value return FFI probe pairs and runs a
two-direction GCC/ZCC differential matrix:

  1. GCC driver + ZCC callee (callee returns struct)
  2. ZCC driver + GCC callee (callee returns struct)
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


def _body(src: str) -> str:
    return textwrap.dedent(src).strip()


SHAPES: tuple[Shape, ...] = (
    # --- INTEGER Returns (<= 16 bytes) ---
    Shape(
        name="int_i32_single",
        category="integer_returns",
        struct_body="""
            struct S {
                int a;
            };
        """,
        init="{ 0x12345678 }",
        checks='printf("a=%08x\\n", (unsigned)s.a);',
        expected="a=12345678",
    ),
    Shape(
        name="int_i64_single",
        category="integer_returns",
        struct_body="""
            struct S {
                long long a;
            };
        """,
        init="{ 0x1122334455667788LL }",
        checks='printf("a=%016llx\\n", (unsigned long long)s.a);',
        expected="a=1122334455667788",
    ),
    Shape(
        name="int_i32_pair",
        category="integer_returns",
        struct_body="""
            struct S {
                int a;
                int b;
            };
        """,
        init="{ 0x12345678, 0x55667788 }",
        checks='printf("a=%08x b=%08x\\n", (unsigned)s.a, (unsigned)s.b);',
        expected="a=12345678 b=55667788",
    ),
    Shape(
        name="int_i64_pair",
        category="integer_returns",
        struct_body="""
            struct S {
                long long a;
                long long b;
            };
        """,
        init="{ 0x0123456789abcdefLL, 0xfedcba9876543210LL }",
        checks='printf("a=%016llx b=%016llx\\n", (unsigned long long)s.a, (unsigned long long)s.b);',
        expected="a=0123456789abcdef b=fedcba9876543210",
    ),
    Shape(
        name="int_i32_quad",
        category="integer_returns",
        struct_body="""
            struct S {
                int a;
                int b;
                int c;
                int d;
            };
        """,
        init="{ 0x11111111, 0x22222222, 0x33333333, 0x44444444 }",
        checks='printf("a=%08x b=%08x c=%08x d=%08x\\n", (unsigned)s.a, (unsigned)s.b, (unsigned)s.c, (unsigned)s.d);',
        expected="a=11111111 b=22222222 c=33333333 d=44444444",
    ),

    # --- SSE Returns (<= 16 bytes) ---
    Shape(
        name="float_f32_single",
        category="sse_returns",
        struct_body="""
            struct S {
                float a;
            };
        """,
        init="{ 1.25f }",
        checks='printf("a=%.9g\\n", s.a);',
        expected="a=1.25",
    ),
    Shape(
        name="float_f64_single",
        category="sse_returns",
        struct_body="""
            struct S {
                double a;
            };
        """,
        init="{ -5.75 }",
        checks='printf("a=%.17g\\n", s.a);',
        expected="a=-5.75",
    ),
    Shape(
        name="float_f32_pair",
        category="sse_returns",
        struct_body="""
            struct S {
                float a;
                float b;
            };
        """,
        init="{ 3.125f, -0.0625f }",
        checks='printf("a=%.9g b=%.9g\\n", s.a, s.b);',
        expected="a=3.125 b=-0.0625",
    ),
    Shape(
        name="float_f64_pair",
        category="sse_returns",
        struct_body="""
            struct S {
                double a;
                double b;
            };
        """,
        init="{ 100.25, -200.75 }",
        checks='printf("a=%.17g b=%.17g\\n", s.a, s.b);',
        expected="a=100.25 b=-200.75",
    ),
    Shape(
        name="float_f32_quad",
        category="sse_returns",
        struct_body="""
            struct S {
                float a;
                float b;
                float c;
                float d;
            };
        """,
        init="{ 1.5f, 2.5f, 3.5f, 4.5f }",
        checks='printf("a=%.9g b=%.9g c=%.9g d=%.9g\\n", s.a, s.b, s.c, s.d);',
        expected="a=1.5 b=2.5 c=3.5 d=4.5",
    ),

    # --- MIXED Returns (<= 16 bytes) ---
    Shape(
        name="mixed_i32_f32_same_eightbyte",
        category="mixed_returns",
        struct_body="""
            struct S {
                int a;
                float b;
            };
        """,
        init="{ 0x12345678, 3.5f }",
        checks='printf("a=%08x b=%.9g\\n", (unsigned)s.a, s.b);',
        expected="a=12345678 b=3.5",
    ),
    Shape(
        name="mixed_f32_i32_same_eightbyte",
        category="mixed_returns",
        struct_body="""
            struct S {
                float a;
                int b;
            };
        """,
        init="{ -2.25f, 0x6a5a1234 }",
        checks='printf("a=%.9g b=%08x\\n", s.a, (unsigned)s.b);',
        expected="a=-2.25 b=6a5a1234",
    ),
    Shape(
        name="mixed_i64_f64",
        category="mixed_returns",
        struct_body="""
            struct S {
                long long a;
                double b;
            };
        """,
        init="{ 0x1122334455667788LL, -99.5 }",
        checks='printf("a=%016llx b=%.17g\\n", (unsigned long long)s.a, s.b);',
        expected="a=1122334455667788 b=-99.5",
    ),
    Shape(
        name="mixed_f64_i64",
        category="mixed_returns",
        struct_body="""
            struct S {
                double a;
                long long b;
            };
        """,
        init="{ -99.5, 0x1122334455667788LL }",
        checks='printf("a=%.17g b=%016llx\\n", s.a, (unsigned long long)s.b);',
        expected="a=-99.5 b=1122334455667788",
    ),
    Shape(
        name="mixed_i32_f32_i32",
        category="mixed_returns",
        struct_body="""
            struct S {
                int a;
                float b;
                int c;
            };
        """,
        init="{ 0x12345678, 1.5f, 0x77889900 }",
        checks='printf("a=%08x b=%.9g c=%08x\\n", (unsigned)s.a, s.b, (unsigned)s.c);',
        expected="a=12345678 b=1.5 c=77889900",
    ),

    # --- MEMORY Returns (> 16 bytes) ---
    Shape(
        name="memory_i64_i64_i32_gt16",
        category="memory_returns",
        struct_body="""
            struct S {
                long long a;
                long long b;
                int c;
            };
        """,
        init="{ 0x1122334455667788LL, 0x99aabbccddeeff00LL, 0x12345678 }",
        checks='printf("a=%016llx b=%016llx c=%08x\\n", (unsigned long long)s.a, (unsigned long long)s.b, (unsigned)s.c);',
        expected="a=1122334455667788 b=99aabbccddeeff00 c=12345678",
    ),
    Shape(
        name="memory_f64_f64_f32_gt16",
        category="memory_returns",
        struct_body="""
            struct S {
                double a;
                double b;
                float c;
            };
        """,
        init="{ 1.125, 2.25, 3.375f }",
        checks='printf("a=%.17g b=%.17g c=%.9g\\n", s.a, s.b, s.c);',
        expected="a=1.125 b=2.25 c=3.375",
    ),
)


def emit_case(out: pathlib.Path, shape: Shape) -> None:
    case_dir = out / shape.name
    case_dir.mkdir(parents=True, exist_ok=True)

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
## {ticket}: {shape.name} struct-by-value return mismatch

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
    parser = argparse.ArgumentParser(description="Generate and run ABI struct-return differential FFI probes.")
    parser.add_argument("--out", default="abi_ret_lane_cases", help="case output directory")
    parser.add_argument("--gcc", default="gcc", help="reference compiler/linker")
    parser.add_argument("--zcc", default="./zcc", help="compiler under test")
    parser.add_argument("--run", action="store_true", help="compile and run GCC/ZCC differential matrix")
    parser.add_argument("--timeout", type=float, default=10.0, help="per-command timeout in seconds")
    parser.add_argument("--ticket-prefix", default="CG-RET", help="bug-ticket prefix for generated mismatch templates")
    args = parser.parse_args()

    out = pathlib.Path(args.out)
    out.mkdir(parents=True, exist_ok=True)

    by_name = {shape.name: shape for shape in SHAPES}
    for shape in SHAPES:
        emit_case(out, shape)

    if not args.run:
        print(f"generated {len(SHAPES)} ABI return cases under {out}")
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
