#!/usr/bin/env python3
"""
ABI-lane differential fuzzer, Phase 1.

Generates System V AMD64 struct-by-value FFI probe pairs and optionally runs a
two-direction GCC/ZCC differential matrix:

  1. GCC driver + ZCC callee
  2. ZCC driver + GCC callee

This harness is deliberately discovery-oriented. A green run means only that the
emitted shapes matched under the configured compiler pair; it is not an ABI
closure claim.
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
    # Mixed INTEGER/SSE inside one eightbyte: highest-risk ABI classification lane.
    Shape(
        name="mixed_i32_f32_same_eightbyte",
        category="mixed_same_eightbyte",
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
        category="mixed_same_eightbyte",
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
        name="mixed_char_float_same_eightbyte",
        category="mixed_same_eightbyte",
        struct_body="""
            struct S {
                char a;
                float b;
            };
        """,
        init="{ 0x7f, 1.25f }",
        checks='printf("a=%02x b=%.9g\\n", (unsigned char)s.a, s.b);',
        expected="a=7f b=1.25",
    ),
    Shape(
        name="mixed_short_float_same_eightbyte",
        category="mixed_same_eightbyte",
        struct_body="""
            struct S {
                short a;
                float b;
            };
        """,
        init="{ 0x1234, -2.5f }",
        checks='printf("a=%04x b=%.9g\\n", (unsigned short)s.a, s.b);',
        expected="a=1234 b=-2.5",
    ),
    Shape(
        name="mixed_float_char_same_eightbyte",
        category="mixed_same_eightbyte",
        struct_body="""
            struct S {
                float a;
                char b;
            };
        """,
        init="{ -3.75f, 0x12 }",
        checks='printf("a=%.9g b=%02x\\n", s.a, (unsigned char)s.b);',
        expected="a=-3.75 b=12",
    ),

    # INTEGER packing sweep.
    Shape(
        name="int_i32_single",
        category="integer_packing",
        struct_body="""
            struct S {
                int a;
            };
        """,
        init="{ 0x01020304 }",
        checks='printf("a=%08x\\n", (unsigned)s.a);',
        expected="a=01020304",
    ),
    Shape(
        name="int_i32_pair_one_eightbyte",
        category="integer_packing",
        struct_body="""
            struct S {
                int a;
                int b;
            };
        """,
        init="{ 0x11112222, 0x33334444 }",
        checks='printf("a=%08x b=%08x\\n", (unsigned)s.a, (unsigned)s.b);',
        expected="a=11112222 b=33334444",
    ),
    Shape(
        name="int_i32_triple_two_eightbytes",
        category="integer_packing",
        struct_body="""
            struct S {
                int a;
                int b;
                int c;
            };
        """,
        init="{ 0x11112222, 0x33334444, 0x55556666 }",
        checks='printf("a=%08x b=%08x c=%08x\\n", (unsigned)s.a, (unsigned)s.b, (unsigned)s.c);',
        expected="a=11112222 b=33334444 c=55556666",
    ),
    Shape(
        name="int_i32_quad_two_eightbytes",
        category="integer_packing",
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
    Shape(
        name="int_i64_single_one_eightbyte",
        category="integer_packing",
        struct_body="""
            struct S {
                long long a;
            };
        """,
        init="{ 0x1111222233334444LL }",
        checks='printf("a=%016llx\\n", (unsigned long long)s.a);',
        expected="a=1111222233334444",
    ),
    Shape(
        name="int_i64_pair_two_eightbytes",
        category="integer_packing",
        struct_body="""
            struct S {
                long long a;
                long long b;
            };
        """,
        init="{ 0x1111222233334444LL, 0x5555666677770888LL }",
        checks='printf("a=%016llx b=%016llx\\n", (unsigned long long)s.a, (unsigned long long)s.b);',
        expected="a=1111222233334444 b=5555666677770888",
    ),
    Shape(
        name="int_i64_triple_memory",
        category="memory_class",
        struct_body="""
            struct S {
                long long a;
                long long b;
                long long c;
            };
        """,
        init="{ 0x1111222233334444LL, 0x5555666677778888LL, 0x9999aaaabbbbccccLL }",
        checks='printf("a=%016llx b=%016llx c=%016llx\\n", (unsigned long long)s.a, (unsigned long long)s.b, (unsigned long long)s.c);',
        expected="a=1111222233334444 b=5555666677778888 c=9999aaaabbbbcccc",
    ),
    Shape(
        name="int_i64_quad_memory",
        category="memory_class",
        struct_body="""
            struct S {
                long long a;
                long long b;
                long long c;
                long long d;
            };
        """,
        init="{ 0x1111222233334444LL, 0x5555666677778888LL, 0x9999aaaabbbbccccLL, 0xddddeeeeffff0000LL }",
        checks='printf("a=%016llx b=%016llx c=%016llx d=%016llx\\n", (unsigned long long)s.a, (unsigned long long)s.b, (unsigned long long)s.c, (unsigned long long)s.d);',
        expected="a=1111222233334444 b=5555666677778888 c=9999aaaabbbbcccc d=ddddeeeeffff0000",
    ),

    # SSE classification sweep.
    Shape(
        name="float_f32_single_one_eightbyte",
        category="sse_packing",
        struct_body="""
            struct S {
                float a;
            };
        """,
        init="{ 1.5f }",
        checks='printf("a=%.9g\\n", s.a);',
        expected="a=1.5",
    ),
    Shape(
        name="float_f32_pair_one_eightbyte",
        category="sse_packing",
        struct_body="""
            struct S {
                float a;
                float b;
            };
        """,
        init="{ 1.25f, -4.5f }",
        checks='printf("a=%.9g b=%.9g\\n", s.a, s.b);',
        expected="a=1.25 b=-4.5",
    ),
    Shape(
        name="float_f32_triple_two_eightbytes",
        category="sse_packing",
        struct_body="""
            struct S {
                float a;
                float b;
                float c;
            };
        """,
        init="{ 1.25f, -2.5f, 3.75f }",
        checks='printf("a=%.9g b=%.9g c=%.9g\\n", s.a, s.b, s.c);',
        expected="a=1.25 b=-2.5 c=3.75",
    ),
    Shape(
        name="float_f32_quad_two_eightbytes",
        category="sse_packing",
        struct_body="""
            struct S {
                float a;
                float b;
                float c;
                float d;
            };
        """,
        init="{ 1.0f, 2.0f, 3.0f, 4.0f }",
        checks='printf("a=%.9g b=%.9g c=%.9g d=%.9g\\n", s.a, s.b, s.c, s.d);',
        expected="a=1 b=2 c=3 d=4",
    ),
    Shape(
        name="float_f64_single_one_eightbyte",
        category="sse_packing",
        struct_body="""
            struct S {
                double a;
            };
        """,
        init="{ 2.5 }",
        checks='printf("a=%.17g\\n", s.a);',
        expected="a=2.5",
    ),
    Shape(
        name="float_f64_pair_two_eightbytes",
        category="sse_packing",
        struct_body="""
            struct S {
                double a;
                double b;
            };
        """,
        init="{ 2.0, -8.125 }",
        checks='printf("a=%.17g b=%.17g\\n", s.a, s.b);',
        expected="a=2 b=-8.125",
    ),
    Shape(
        name="float_f64_triple_memory",
        category="memory_class",
        struct_body="""
            struct S {
                double a;
                double b;
                double c;
            };
        """,
        init="{ 1.5, 2.5, 3.5 }",
        checks='printf("a=%.17g b=%.17g c=%.17g\\n", s.a, s.b, s.c);',
        expected="a=1.5 b=2.5 c=3.5",
    ),

    # Two-eightbyte mixed lanes.
    Shape(
        name="mixed_i32_f32_i32_two_eightbytes",
        category="mixed_two_eightbytes",
        struct_body="""
            struct S {
                int a;
                float b;
                int c;
            };
        """,
        init="{ 0x11112222, 7.75f, 0x33334444 }",
        checks='printf("a=%08x b=%.9g c=%08x\\n", (unsigned)s.a, s.b, (unsigned)s.c);',
        expected="a=11112222 b=7.75 c=33334444",
    ),
    Shape(
        name="mixed_f64_i32_two_eightbytes",
        category="mixed_two_eightbytes",
        struct_body="""
            struct S {
                double a;
                int b;
            };
        """,
        init="{ 9.5, 0x10203040 }",
        checks='printf("a=%.17g b=%08x\\n", s.a, (unsigned)s.b);',
        expected="a=9.5 b=10203040",
    ),
    Shape(
        name="mixed_i32_double_two_eightbytes",
        category="mixed_two_eightbytes",
        struct_body="""
            struct S {
                int a;
                double b;
            };
        """,
        init="{ 0x12345678, 4.5 }",
        checks='printf("a=%08x b=%.17g\\n", (unsigned)s.a, s.b);',
        expected="a=12345678 b=4.5",
    ),
    Shape(
        name="mixed_double_i32_two_eightbytes",
        category="mixed_two_eightbytes",
        struct_body="""
            struct S {
                double a;
                int b;
            };
        """,
        init="{ -8.25, 0x5a5a5a5a }",
        checks='printf("a=%.17g b=%08x\\n", s.a, (unsigned)s.b);',
        expected="a=-8.25 b=5a5a5a5a",
    ),

    # Memory-class / stack passing lane (> 16 bytes).
    Shape(
        name="memory_i64_i64_i32_gt16",
        category="memory_class",
        struct_body="""
            struct S {
                long long a;
                long long b;
                int c;
            };
        """,
        init="{ 0x0102030405060708LL, 0x1112131415161718LL, 0x21222324 }",
        checks='printf("a=%016llx b=%016llx c=%08x\\n", (unsigned long long)s.a, (unsigned long long)s.b, (unsigned)s.c);',
        expected="a=0102030405060708 b=1112131415161718 c=21222324",
    ),

    # Padding and alignment edge cases.
    Shape(
        name="padding_char_i32_trailing",
        category="padding_alignment",
        struct_body="""
            struct S {
                char a;
                int b;
                char c;
            };
        """,
        init="{ 0x12, 0x34567890, 0x5a }",
        checks='printf("a=%02x b=%08x c=%02x\\n", (unsigned char)s.a, (unsigned)s.b, (unsigned char)s.c);',
        expected="a=12 b=34567890 c=5a",
    ),
    Shape(
        name="padding_i32_char_trailing",
        category="padding_alignment",
        struct_body="""
            struct S {
                int a;
                char b;
            };
        """,
        init="{ 0x76543210, 0x2b }",
        checks='printf("a=%08x b=%02x\\n", (unsigned)s.a, (unsigned char)s.b);',
        expected="a=76543210 b=2b",
    ),
    Shape(
        name="padding_double_char_trailing",
        category="padding_alignment",
        struct_body="""
            struct S {
                double a;
                char b;
            };
        """,
        init="{ 10.5, 0x7e }",
        checks='printf("a=%.17g b=%02x\\n", s.a, (unsigned char)s.b);',
        expected="a=10.5 b=7e",
    ),
    Shape(
        name="padding_char_double_trailing",
        category="padding_alignment",
        struct_body="""
            struct S {
                char a;
                double b;
            };
        """,
        init="{ 0x05, -20.25 }",
        checks='printf("a=%02x b=%.17g\\n", (unsigned char)s.a, s.b);',
        expected="a=05 b=-20.25",
    ),
    Shape(
        name="padding_short_double_trailing",
        category="padding_alignment",
        struct_body="""
            struct S {
                short a;
                double b;
            };
        """,
        init="{ 0x1234, 100.125 }",
        checks='printf("a=%04x b=%.17g\\n", (unsigned short)s.a, s.b);',
        expected="a=1234 b=100.125",
    ),
    Shape(
        name="padding_i32_short_trailing",
        category="padding_alignment",
        struct_body="""
            struct S {
                int a;
                short b;
            };
        """,
        init="{ 0x7fffffff, 0x1234 }",
        checks='printf("a=%08x b=%04x\\n", (unsigned)s.a, (unsigned short)s.b);',
        expected="a=7fffffff b=1234",
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

        for artifact in case.glob("*.o"):
            artifact.unlink()
        for artifact in case.glob("*.s"):
            artifact.unlink()
        for artifact in case.glob("*.bin"):
            artifact.unlink()

        for step, cmd in spec["steps"]:
            try:
                proc = run(cmd, case, timeout)
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
                proc = run([spec["bin"]], case, timeout)
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
## {ticket}: {shape.name} struct-by-value mismatch

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
    parser = argparse.ArgumentParser(description="Generate and run ABI-lane differential FFI probes.")
    parser.add_argument("--out", default="abi_lane_cases", help="case output directory")
    parser.add_argument("--gcc", default="gcc", help="reference compiler/linker")
    parser.add_argument("--zcc", default="./zcc", help="compiler under test")
    parser.add_argument("--run", action="store_true", help="compile and run GCC/ZCC differential matrix")
    parser.add_argument("--timeout", type=float, default=10.0, help="per-command timeout in seconds")
    parser.add_argument("--ticket-prefix", default="CG-ABI", help="bug-ticket prefix for generated mismatch templates")
    args = parser.parse_args()

    out = pathlib.Path(args.out)
    out.mkdir(parents=True, exist_ok=True)

    by_name = {shape.name: shape for shape in SHAPES}
    for shape in SHAPES:
        emit_case(out, shape)

    if not args.run:
        print(f"generated {len(SHAPES)} ABI cases under {out}")
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
