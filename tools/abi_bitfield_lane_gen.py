#!/usr/bin/env python3
"""
ABI-lane differential fuzzer for bitfield struct classification and layout.

Generates System V AMD64 bitfield struct FFI probe pairs and runs a two-direction
GCC/ZCC differential matrix in isolated subdirectories.

The central questions:
  - Does ZCC correctly pack sub-byte bitfields (a:3, b:5 share one byte)?
  - Does ZCC correctly handle the :0 unnamed-zero-width alignment reset?
  - Does ZCC treat plain `int` bitfields as signed (int x:1 -> {-1, 0})?
  - Does ZCC correctly classify 2-eightbyte bitfield structs (INTEGER:INTEGER)?
  - Does ZCC correctly demote >16-byte bitfield structs to MEMORY?

Disassembly is captured for ALL cases -- not just MEMORY ones -- because bitfield
value extraction can mask a layout bug (the output may match while the bit
positions are wrong if both compiler sides use the same wrong layout).

Mirrors tools/abi_packed_lane_gen.py exactly -- do not merge or refactor either.
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
# Shape matrix -- 8 bitfield shapes x {arg, ret} = 16 entries.
#
# ABI rules pinned in plan (SysV AMD64 §3.2.3):
#   sizeof <= 8:  1 eightbyte, INTEGER -> %rdi (arg) / %rax (ret)
#   sizeof == 16: 2 eightbytes, INTEGER:INTEGER -> %rdi:%rsi (arg) / %rax:%rdx (ret)
#   sizeof > 16:  MEMORY -> hidden-ptr in %rdi (ret); stack (arg)
#
# Key bitfield ABI axes tested:
#   bf_pack_3_5      sub-byte packing: a:3 and b:5 share one byte in 4-byte unit
#   bf_pack_fw       dense 32-bit: a:10 b:10 c:12 fills one unsigned unit exactly
#   bf_two_eb_ul40   2-eightbyte INTEGER:INTEGER: {ul a:40, ul b:40} sizeof=16
#   bf_zw            :0 zero-width reset: forces b to new 4-byte unit at offset 4
#   bf_sig           plain int signedness: int x:1 -> {-1, 0} not {0, 1}
#   bf_mix           scalar + bitfield: int a + unsigned b:4 + unsigned c:4
#   bf_ctrl          full-width control: unsigned a:32 == plain unsigned (INTEGER)
#   bf_mem           MEMORY: {ul a:40, ul b:40, ul c:40} sizeof=24 > 16
#
# :0 reset discriminator: bf_zw uses {31, 17} (not {31, 31}) so that if ZCC
#   ignores the :0 reset and reads b from the wrong bit position (bits 5-9 of
#   GCC's layout, which are padding), it will read 0 instead of 17.
#
# Signedness discriminator: bf_sig prints xs=%d (signed) AND xu=%u (unsigned cast).
#   GCC correct:  xs=-1 xu=4294967295 y=-2
#   ZCC unsigned: xs=1  xu=1          y=2   (or y=-2 if y handled correctly)
# ---------------------------------------------------------------------------

SHAPES: tuple[Shape, ...] = (

    # --- bf_pack_3_5: { unsigned a:3; unsigned b:5; } sub-byte packing ---
    # sizeof=4 (unsigned storage unit). a in bits[2:0], b in bits[7:3].
    # GCC: and $0xfffffff8/%eax then or $0x5 for a; or $0xd8 for b.
    # Class: INTEGER (4 <= 8, one eightbyte). Arg: %rdi. Ret: %rax.
    Shape(
        name="bf_pack_3_5_arg",
        category="argument_passing",
        struct_body="""
            struct S {
                unsigned a:3;
                unsigned b:5;
            };
        """,
        init="{5, 27}",
        checks='printf("a=%u b=%u\\n", s.a, s.b);',
        expected="a=5 b=27",
        is_return=False,
    ),
    Shape(
        name="bf_pack_3_5_ret",
        category="return_values",
        struct_body="""
            struct S {
                unsigned a:3;
                unsigned b:5;
            };
        """,
        init="{5, 27}",
        checks='printf("a=%u b=%u\\n", s.a, s.b);',
        expected="a=5 b=27",
        is_return=True,
    ),

    # --- bf_pack_fw: { unsigned a:10; unsigned b:10; unsigned c:12; } ---
    # sizeof=4. 10+10+12=32 bits packed into one unsigned unit exactly.
    # GCC: movzwl + and/or for a in bits[9:0]; and/or for b in bits[19:10];
    #       or for c in bits[31:20].
    # Class: INTEGER. Arg: %rdi. Ret: %rax.
    Shape(
        name="bf_pack_fw_arg",
        category="argument_passing",
        struct_body="""
            struct S {
                unsigned a:10;
                unsigned b:10;
                unsigned c:12;
            };
        """,
        init="{511, 511, 4095}",
        checks='printf("a=%u b=%u c=%u\\n", s.a, s.b, s.c);',
        expected="a=511 b=511 c=4095",
        is_return=False,
    ),
    Shape(
        name="bf_pack_fw_ret",
        category="return_values",
        struct_body="""
            struct S {
                unsigned a:10;
                unsigned b:10;
                unsigned c:12;
            };
        """,
        init="{511, 511, 4095}",
        checks='printf("a=%u b=%u c=%u\\n", s.a, s.b, s.c);',
        expected="a=511 b=511 c=4095",
        is_return=True,
    ),

    # --- bf_two_eb_ul40: { unsigned long a:40; unsigned long b:40; } ---
    # sizeof=16. GCC puts b in its own 8-byte unit starting at byte 8 (not a
    # bit-level straddle -- a is entirely in eightbyte 0, b entirely in eb 1).
    # Class: INTEGER:INTEGER. Arg: %rdi:%rsi. Ret: %rax:%rdx.
    # Verified by GCC objdump: ret exits with %rax + %rdx; arg spills both regs.
    Shape(
        name="bf_two_eb_ul40_arg",
        category="argument_passing",
        struct_body="""
            struct S {
                unsigned long a:40;
                unsigned long b:40;
            };
        """,
        init="{1099511627775UL, 2863311530UL}",
        checks='printf("a=%lu b=%lu\\n", (unsigned long)s.a, (unsigned long)s.b);',
        expected="a=1099511627775 b=2863311530",
        is_return=False,
    ),
    Shape(
        name="bf_two_eb_ul40_ret",
        category="return_values",
        struct_body="""
            struct S {
                unsigned long a:40;
                unsigned long b:40;
            };
        """,
        init="{1099511627775UL, 2863311530UL}",
        checks='printf("a=%lu b=%lu\\n", (unsigned long)s.a, (unsigned long)s.b);',
        expected="a=1099511627775 b=2863311530",
        is_return=True,
    ),

    # --- bf_zw: { unsigned a:5; unsigned :0; unsigned b:5; } :0 reset ---
    # sizeof=8. The :0 unnamed zero-width field forces b into a new 4-byte
    # storage unit at byte 4. Without :0, both fields would share the first
    # unit (sizeof=4). GCC objdump shows a at frame[-8], b at frame[-4].
    # Discriminating init {31, 17}: if ZCC ignores :0 and reads b from
    # bits[9:5] of GCC's layout (which are padding = 0), b will be 0 not 17.
    # Class: INTEGER (8B = 1 eightbyte). Arg: %rdi. Ret: %rax.
    Shape(
        name="bf_zw_arg",
        category="argument_passing",
        struct_body="""
            struct S {
                unsigned a:5;
                unsigned :0;
                unsigned b:5;
            };
        """,
        init="{31, 17}",
        checks='printf("a=%u b=%u\\n", s.a, s.b);',
        expected="a=31 b=17",
        is_return=False,
    ),
    Shape(
        name="bf_zw_ret",
        category="return_values",
        struct_body="""
            struct S {
                unsigned a:5;
                unsigned :0;
                unsigned b:5;
            };
        """,
        init="{31, 17}",
        checks='printf("a=%u b=%u\\n", s.a, s.b);',
        expected="a=31 b=17",
        is_return=True,
    ),

    # --- bf_sig: { int x:1; int y:2; } plain-int signedness ---
    # sizeof=4. GCC treats `int` bitfields as signed:
    #   int x:1 -> range {-1, 0}. x=-1 stored as bit[0]=1.
    #   int y:2 -> range {-2,-1,0,1}. y=-2 stored as bits[2:1]=10 (two's complement).
    # GCC objdump: `or $0x1` for x; `and $0xf9; or $0x4` for y.
    # Dual print: xs=%d (signed, expected -1) AND xu=%u (unsigned cast of s.x,
    # expected 4294967295 = (unsigned int)(-1)). If ZCC treats int:1 as unsigned:
    #   xs=1 xu=1 -- clearly different from xs=-1 xu=4294967295.
    # Class: INTEGER. Arg: %rdi. Ret: %rax.
    Shape(
        name="bf_sig_arg",
        category="argument_passing",
        struct_body="""
            struct S {
                int x:1;
                int y:2;
            };
        """,
        init="{-1, -2}",
        checks='printf("xs=%d xu=%u y=%d\\n", s.x, (unsigned int)s.x, s.y);',
        expected="xs=-1 xu=4294967295 y=-2",
        is_return=False,
    ),
    Shape(
        name="bf_sig_ret",
        category="return_values",
        struct_body="""
            struct S {
                int x:1;
                int y:2;
            };
        """,
        init="{-1, -2}",
        checks='printf("xs=%d xu=%u y=%d\\n", s.x, (unsigned int)s.x, s.y);',
        expected="xs=-1 xu=4294967295 y=-2",
        is_return=True,
    ),

    # --- bf_mix: { int a; unsigned b:4; unsigned c:4; } scalar + bitfield ---
    # sizeof=8. Plain int a at bytes 0-3; b:4 and c:4 packed into byte 4.
    # GCC: movl $0x2a at frame[-8] (a); or $0xf for b in bits[3:0];
    #       and $0xf; or $0x70 for c in bits[7:4] (7<<4=0x70).
    # Class: INTEGER (8B = 1 eightbyte). Arg: %rdi. Ret: %rax.
    Shape(
        name="bf_mix_arg",
        category="argument_passing",
        struct_body="""
            struct S {
                int a;
                unsigned b:4;
                unsigned c:4;
            };
        """,
        init="{42, 15, 7}",
        checks='printf("a=%d b=%u c=%u\\n", s.a, s.b, s.c);',
        expected="a=42 b=15 c=7",
        is_return=False,
    ),
    Shape(
        name="bf_mix_ret",
        category="return_values",
        struct_body="""
            struct S {
                int a;
                unsigned b:4;
                unsigned c:4;
            };
        """,
        init="{42, 15, 7}",
        checks='printf("a=%d b=%u c=%u\\n", s.a, s.b, s.c);',
        expected="a=42 b=15 c=7",
        is_return=True,
    ),

    # --- bf_ctrl: { unsigned a:32; } full-width control ---
    # sizeof=4. Full-width bitfield is identical to plain unsigned.
    # GCC: movl $0xdeadbeef; mov -0x4(%rbp),%eax -> %rax.
    # Class: INTEGER. Arg: %rdi. Ret: %rax.
    Shape(
        name="bf_ctrl_arg",
        category="argument_passing",
        struct_body="""
            struct S {
                unsigned a:32;
            };
        """,
        init="{0xDEADBEEFU}",
        checks='printf("a=%u\\n", s.a);',
        expected="a=3735928559",
        is_return=False,
    ),
    Shape(
        name="bf_ctrl_ret",
        category="return_values",
        struct_body="""
            struct S {
                unsigned a:32;
            };
        """,
        init="{0xDEADBEEFU}",
        checks='printf("a=%u\\n", s.a);',
        expected="a=3735928559",
        is_return=True,
    ),

    # --- bf_mem: { ul a:40; ul b:40; ul c:40; } MEMORY (sizeof=24 > 16) ---
    # Three 8-byte ul storage units -> sizeof=24 > 16 -> MEMORY.
    # GCC return: hidden-ptr in %rdi at callee entry; %rax = ptr at exit.
    # GCC arg: struct passed via stack; arg_bf_mem body is nop (no register ops).
    # Discriminating: different values (1, 2, 3) to detect partial copy bugs.
    Shape(
        name="bf_mem_arg",
        category="argument_passing",
        struct_body="""
            struct S {
                unsigned long a:40;
                unsigned long b:40;
                unsigned long c:40;
            };
        """,
        init="{1UL, 2UL, 3UL}",
        checks='printf("a=%lu b=%lu c=%lu\\n", (unsigned long)s.a, (unsigned long)s.b, (unsigned long)s.c);',
        expected="a=1 b=2 c=3",
        is_return=False,
    ),
    Shape(
        name="bf_mem_ret",
        category="return_values",
        struct_body="""
            struct S {
                unsigned long a:40;
                unsigned long b:40;
                unsigned long c:40;
            };
        """,
        init="{1UL, 2UL, 3UL}",
        checks='printf("a=%lu b=%lu c=%lu\\n", (unsigned long)s.a, (unsigned long)s.b, (unsigned long)s.c);',
        expected="a=1 b=2 c=3",
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
    # Lane 1: GCC driver, ZCC callee -- checks ZCC callee codegen / call-site handling by GCC
    # Lane 2: ZCC driver, GCC callee -- checks ZCC caller codegen against GCC callee
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

    def _copy_file(src: pathlib.Path, dst: pathlib.Path):
        dst.write_bytes(src.read_bytes())

    results: dict[str, Any] = {}
    for name, spec in matrix.items():
        log: list[dict[str, Any]] = []
        ok = True

        # Isolated subdirectory -- prevents stale-object cross-lane link failures.
        lane_dir = case / name
        if lane_dir.exists():
            shutil.rmtree(lane_dir)
        lane_dir.mkdir(parents=True, exist_ok=True)

        # Copy source files into the isolated lane.
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

            # Track compiled objects for objdump (captured for ALL cases).
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

        # Capture objdump for ALL cases -- disassembly is the load-bearing evidence.
        # For bitfields, register/memory class alone is not a sufficient tell;
        # the bit-manipulation sequence in the ZCC callee must be read individually.
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
## {ticket}: {shape.name} bitfield-struct mismatch

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

Disassembly (ZCC callee/driver -- gcc_driver_zcc_callee lane):

```asm
{matrix["gcc_driver_zcc_callee"].get("zcc_asm", "(not captured)")}
```

ABI Classification Notes:
- INTEGER (<= 8B): %rdi arg / %rax ret
- INTEGER:INTEGER (16B): %rdi:%rsi arg / %rax:%rdx ret
- MEMORY (>16B): hidden-ptr in %rdi (ret); stack (arg)
- Bitfield layout divergence may not be visible in output alone --
  read the disassembly for the bit-manipulation (and/or) sequences.
- For bf_sig: xs=-1 (signed) vs xs=1 (unsigned) is the sign discriminator.
- For bf_zw: two separate frame slots (-0x8 and -0x4) confirm :0 reset honored.

Artifacts:

* `case.h`
* `driver.c`
* `callee.c`
* `REPRO.txt`
* `gcc_driver_zcc_callee/callee_or_driver.zcc.asm`
* `zcc_driver_gcc_callee/callee_or_driver.zcc.asm`

Notes:
This is exploratory ABI-lane discovery. A bitfield layout or classification
divergence is a CG-BITFIELD-NNN finding. Do not patch until root cause is
confirmed in the disassembly.
"""
    (case / f"{ticket}.md").write_text(template, encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Generate and run ABI bitfield-struct differential FFI probes."
    )
    parser.add_argument("--out", default="abi_bitfield_cases", help="case output directory")
    parser.add_argument("--gcc", default="gcc", help="reference compiler/linker")
    parser.add_argument("--zcc", default="./zcc", help="compiler under test")
    parser.add_argument("--run", action="store_true", help="compile and run GCC/ZCC differential matrix")
    parser.add_argument("--timeout", type=float, default=10.0, help="per-command timeout in seconds")
    parser.add_argument("--ticket-prefix", default="CG-BITFIELD", help="bug-ticket prefix for mismatch templates")
    args = parser.parse_args()

    out = pathlib.Path(args.out)
    out.mkdir(parents=True, exist_ok=True)

    by_name = {shape.name: shape for shape in SHAPES}
    for shape in SHAPES:
        emit_case(out, shape)

    if not args.run:
        print(f"generated {len(SHAPES)} ABI bitfield cases under {out}")
        return 0

    zcc_path = str(pathlib.Path(args.zcc).resolve())
    require_tool(args.gcc)
    require_tool(zcc_path)

    summary: dict[str, Any] = {"tested": 0, "passed": 0, "failed": 0, "cases": {}}

    for case in sorted(p for p in out.iterdir() if p.is_dir()):
        shape = by_name.get(case.name)
        if shape is None:
            continue
        result = build_and_run(case, args.gcc, zcc_path, args.timeout)

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
