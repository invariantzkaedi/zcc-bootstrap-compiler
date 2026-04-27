#!/usr/bin/env python3
"""
============================================================================
ZCC Differential Fuzzer v1.0
============================================================================
Generates random valid C programs within ZCC's supported subset,
compiles with both GCC and ZCC, and compares outputs.

Finds: codegen bugs, crashes, miscompilations, hangs.
Strategy: Generate programs that exercise known-fragile patterns
          (sign extension, division, shifts, struct access, recursion)
          with random values.

Usage:
    python3 zcc_fuzz.py --zcc ./zcc --count 200 --output-dir ./fuzz_results
============================================================================
"""

import argparse
import hashlib
import json
import os
import random
import shutil
import subprocess
import sys
import tempfile
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional


# ——— ZCC-safe C subset ———
# No system headers, no compound literals, no designated initializers,
# no _Bool, no inline, no restrict, no VLAs, no _Static_assert.

TYPES = ['int', 'long', 'unsigned int', 'unsigned long', 'char', 'unsigned char']
INT_TYPES = ['int', 'long', 'unsigned int', 'unsigned long']
BINOPS = ['+', '-', '*', '/', '%', '&', '|', '^', '<<', '>>']
CMPOPS = ['==', '!=', '<', '>', '<=', '>=']


@dataclass
class FuzzConfig:
    max_functions: int = 5
    max_locals: int = 8
    max_depth: int = 4
    max_stmts: int = 15
    max_array_size: int = 20
    use_structs: bool = True
    use_pointers: bool = True
    use_recursion: bool = True
    use_malloc: bool = True
    bias_sign_extension: float = 0.3   # Extra weight on CG-001 patterns
    bias_division: float = 0.2         # Extra weight on CG-002 patterns
    bias_shift: float = 0.2            # Extra weight on CG-003 patterns


@dataclass
class FuzzResult:
    seed: int
    status: str  # 'pass', 'gcc_fail', 'zcc_fail', 'mismatch', 'crash', 'timeout'
    gcc_output: str = ''
    zcc_output: str = ''
    gcc_rc: int = 0
    zcc_rc: int = 0
    source: str = ''
    error_msg: str = ''


class CGenerator:
    """Generates random valid C programs in ZCC's subset."""

    def __init__(self, seed: int, config: FuzzConfig):
        self.rng = random.Random(seed)
        self.seed = seed
        self.config = config
        self.var_counter = 0
        self.func_counter = 0
        self.lines: list[str] = []
        self.indent = 0
        self.funcs: list[str] = []
        self.struct_defs: list[str] = []
        self.checksum_vars: list[str] = []

    def fresh_var(self) -> str:
        self.var_counter += 1
        return f'v{self.var_counter}'

    def fresh_func(self) -> str:
        self.func_counter += 1
        name = f'f{self.func_counter}'
        self.funcs.append(name)
        return name

    def emit(self, line: str):
        self.lines.append('    ' * self.indent + line)

    def random_type(self) -> str:
        return self.rng.choice(INT_TYPES)

    def random_literal(self, ty: str) -> str:
        """Generate type-appropriate random literal, biased toward edge cases."""
        r = self.rng.random()

        if 'unsigned' in ty:
            if r < 0.15:
                # Edge cases for sign extension bugs
                return self.rng.choice([
                    '0x80000000u', '0xFFFFFFFFu', '0x7FFFFFFFu',
                    '0u', '1u', '255u', '256u',
                ])
            elif r < 0.3:
                return f'{self.rng.randint(0, 2**31)}u'
            else:
                return f'{self.rng.randint(0, 10000)}u'
        else:
            if r < 0.15:
                return self.rng.choice([
                    '-1', '0', '1', '-128', '127', '-2147483648',
                    '2147483647',
                ])
            elif r < 0.3:
                return str(self.rng.randint(-2**30, 2**30))
            else:
                return str(self.rng.randint(-1000, 1000))

    def random_expr(self, ty: str, depth: int = 0) -> str:
        """Generate a random expression of the given type."""
        if depth >= self.config.max_depth:
            return self.random_literal(ty)

        r = self.rng.random()

        if r < 0.3:
            return self.random_literal(ty)

        elif r < 0.55:
            # Binary operation
            op = self.rng.choice(BINOPS)
            left = self.random_expr(ty, depth + 1)
            right = self.random_expr(ty, depth + 1)

            # Avoid division by zero and undefined shifts
            if op in ('/', '%'):
                # Wrap divisor to ensure non-zero
                right = f'(({right}) | 1)'
                if 'unsigned' not in ty:
                    # Avoid INT_MIN / -1
                    right = f'(({right}) == 0 ? 1 : ({right}))'
            elif op in ('<<', '>>'):
                # Shift amount must be 0..31 for 32-bit
                right = f'(({right}) & 15)'

            return f'(({left}) {op} ({right}))'

        elif r < 0.7:
            # Ternary
            cond = self.random_cmp(depth + 1)
            a = self.random_expr(ty, depth + 1)
            b = self.random_expr(ty, depth + 1)
            return f'({cond} ? {a} : {b})'

        elif r < 0.85:
            # Cast (sign extension / truncation exerciser)
            src_ty = self.rng.choice(INT_TYPES)
            inner = self.random_expr(src_ty, depth + 1)
            return f'(({ty})({inner}))'

        else:
            # Unary
            inner = self.random_expr(ty, depth + 1)
            op = self.rng.choice(['-', '~'])
            return f'({op}({inner}))'

    def random_cmp(self, depth: int = 0) -> str:
        ty = self.random_type()
        left = self.random_expr(ty, depth)
        right = self.random_expr(ty, depth)
        op = self.rng.choice(CMPOPS)
        return f'(({left}) {op} ({right}))'

    def gen_function(self, name: str, has_params: bool = True) -> str:
        """Generate a complete function definition."""
        lines: list[str] = []

        ret_ty = self.random_type()
        params = []
        param_names = []
        if has_params:
            n_params = self.rng.randint(1, 4)
            for i in range(n_params):
                pty = self.random_type()
                pname = f'p{i}'
                params.append(f'{pty} {pname}')
                param_names.append(pname)

        sig = f'{ret_ty} {name}({", ".join(params) if params else "void"})'
        lines.append(f'{sig} {{')

        # Local variables
        locals_list = []
        n_locals = self.rng.randint(1, self.config.max_locals)
        for _ in range(n_locals):
            vname = self.fresh_var()
            vty = self.random_type()
            init = self.random_expr(vty, 0)
            lines.append(f'    {vty} {vname} = {init};')
            locals_list.append((vname, vty))

        # Statements
        n_stmts = self.rng.randint(2, self.config.max_stmts)
        for _ in range(n_stmts):
            stmt = self.gen_stmt(locals_list, param_names, ret_ty, depth=0)
            for s in stmt:
                lines.append(f'    {s}')

        # Combine locals for return
        if locals_list:
            vname, vty = self.rng.choice(locals_list)
            lines.append(f'    return ({ret_ty}){vname};')
        else:
            lines.append(f'    return ({ret_ty})0;')

        lines.append('}')
        return '\n'.join(lines)

    def gen_stmt(self, locals_list, params, ret_ty, depth) -> list[str]:
        """Generate a random statement."""
        if depth >= 3:
            # Simple assignment
            if locals_list:
                vname, vty = self.rng.choice(locals_list)
                expr = self.random_expr(vty, 2)
                return [f'{vname} = ({vty})({expr});']
            return ['/* nop */;']

        r = self.rng.random()

        if r < 0.35:
            # Assignment
            if locals_list:
                vname, vty = self.rng.choice(locals_list)
                expr = self.random_expr(vty, 1)
                return [f'{vname} = ({vty})({expr});']
            return ['/* nop */;']

        elif r < 0.55:
            # If-else
            cond = self.random_cmp(1)
            then_stmts = self.gen_stmt(locals_list, params, ret_ty, depth + 1)
            else_stmts = self.gen_stmt(locals_list, params, ret_ty, depth + 1)
            result = [f'if ({cond}) {{']
            for s in then_stmts:
                result.append(f'    {s}')
            result.append('} else {')
            for s in else_stmts:
                result.append(f'    {s}')
            result.append('}')
            return result

        elif r < 0.7:
            # For loop (bounded)
            ivar = self.fresh_var()
            limit = self.rng.randint(1, 20)
            body = self.gen_stmt(locals_list, params, ret_ty, depth + 1)
            result = [f'{{ int {ivar}; for ({ivar} = 0; {ivar} < {limit}; {ivar}++) {{']
            for s in body:
                result.append(f'    {s}')
            result.append('} }')
            return result

        elif r < 0.8:
            # Compound assignment
            if locals_list:
                vname, vty = self.rng.choice(locals_list)
                op = self.rng.choice(['+=', '-=', '*=', '&=', '|=', '^='])
                val = self.random_expr(vty, 2)
                return [f'{vname} {op} ({vty})({val});']
            return ['/* nop */;']

        else:
            # Bitwise stress (sign extension / shift exerciser)
            if locals_list:
                vname, vty = self.rng.choice(locals_list)
                shift = self.rng.randint(0, 15)
                return [f'{vname} = ({vty})(({vname} >> {shift}) | ({vname} << {16 - shift}));']
            return ['/* nop */;']

    def generate(self) -> str:
        """Generate a complete C program."""
        parts: list[str] = []

        # Header (ZCC-safe declarations)
        parts.append('/* ZCC Fuzz Program — seed=%d */' % self.seed)
        parts.append('int printf(const char *fmt, ...);')
        parts.append('')

        # Helper functions
        n_funcs = self.rng.randint(1, self.config.max_functions)
        func_names = []
        for i in range(n_funcs):
            name = self.fresh_func()
            func_names.append(name)

        # Forward declarations
        for name in func_names:
            parts.append(f'static int {name}();')  # simplified
        parts.append('')

        # Generate each function with simplified signature
        for name in func_names:
            parts.append(self.gen_standalone_function(name))
            parts.append('')

        # Main: call all functions and checksum
        parts.append('int main(void) {')
        parts.append('    long checksum = 0;')
        for name in func_names:
            parts.append(f'    checksum += (long){name}(1, 2, 3);')
        parts.append('    printf("CHECKSUM=%ld\\n", checksum);')
        parts.append('    return 0;')
        parts.append('}')

        return '\n'.join(parts)

    def gen_standalone_function(self, name: str) -> str:
        """Generate a function with fixed 3-int signature for simplicity."""
        lines = []
        lines.append(f'static int {name}(int a, int b, int c) {{')

        # Locals
        n_locals = self.rng.randint(2, 6)
        local_names = []
        for i in range(n_locals):
            vname = self.fresh_var()
            vty = self.random_type()
            expr = self.random_expr(vty, 0)
            lines.append(f'    {vty} {vname} = ({vty})({expr});')
            local_names.append((vname, vty))

        # Mix in parameters
        all_vars = local_names + [('a', 'int'), ('b', 'int'), ('c', 'int')]

        # Statements
        n_stmts = self.rng.randint(3, 10)
        for _ in range(n_stmts):
            stmts = self.gen_stmt(all_vars, ['a', 'b', 'c'], 'int', 0)
            for s in stmts:
                lines.append(f'    {s}')

        # Return a combination
        if local_names:
            vars_sum = ' + '.join(f'(int){v}' for v, _ in local_names[:3])
            lines.append(f'    return ({vars_sum}) + a + b + c;')
        else:
            lines.append('    return a + b + c;')

        lines.append('}')
        return '\n'.join(lines)


def compile_and_run(source: str, compiler: str, output: str,
                    timeout: int = 10, extra_flags: list = None) -> tuple[int, str, str]:
    """Compile source and run the binary. Returns (rc, stdout, stderr)."""
    flags = extra_flags or []

    # Write source
    src_path = output + '.c'
    with open(src_path, 'w') as f:
        f.write(source)

    # Compile
    cmd = [compiler] + flags + ['-o', output, src_path]
    if compiler == 'gcc':
        cmd = ['gcc', '-O0', '-std=c99', '-w'] + ['-o', output, src_path, '-lm']
    else:
        cmd = [compiler, src_path, '-o', output]

    try:
        comp = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
        if comp.returncode != 0:
            return (-1, '', f'Compile error: {comp.stderr[:500]}')
    except subprocess.TimeoutExpired:
        return (-2, '', 'Compile timeout')
    except FileNotFoundError:
        return (-3, '', f'Compiler not found: {compiler}')

    # Run
    try:
        run = subprocess.run([output], capture_output=True, text=True, timeout=timeout)
        return (run.returncode, run.stdout, run.stderr)
    except subprocess.TimeoutExpired:
        return (-4, '', 'Runtime timeout')


def fuzz_one(seed: int, zcc_path: str, tmpdir: str, config: FuzzConfig,
             timeout: int) -> FuzzResult:
    """Run one fuzz iteration."""
    gen = CGenerator(seed, config)
    source = gen.generate()

    result = FuzzResult(seed=seed, status='unknown', source=source)

    # GCC reference
    gcc_rc, gcc_out, gcc_err = compile_and_run(
        source, 'gcc',
        os.path.join(tmpdir, f'gcc_{seed}'),
        timeout=timeout
    )

    if gcc_rc < 0:
        result.status = 'gcc_fail'
        result.error_msg = gcc_err
        return result

    result.gcc_rc = gcc_rc
    result.gcc_output = gcc_out

    # ZCC
    zcc_rc, zcc_out, zcc_err = compile_and_run(
        source, zcc_path,
        os.path.join(tmpdir, f'zcc_{seed}'),
        timeout=timeout
    )

    if zcc_rc == -1:
        result.status = 'zcc_fail'
        result.error_msg = zcc_err
        return result
    elif zcc_rc == -2:
        result.status = 'timeout'
        result.error_msg = 'ZCC compile timeout'
        return result
    elif zcc_rc == -3:
        result.status = 'zcc_fail'
        result.error_msg = zcc_err
        return result
    elif zcc_rc == -4:
        result.status = 'timeout'
        result.error_msg = 'ZCC runtime timeout'
        return result

    result.zcc_rc = zcc_rc
    result.zcc_output = zcc_out

    # Check for crash (signal)
    if zcc_rc > 128:
        result.status = 'crash'
        sig = zcc_rc - 128
        result.error_msg = f'Signal {sig} ({"SIGSEGV" if sig == 11 else "SIG" + str(sig)})'
        return result

    # Compare outputs
    if gcc_out.strip() == zcc_out.strip():
        if gcc_rc == zcc_rc:
            result.status = 'pass'
        else:
            result.status = 'mismatch'
            result.error_msg = f'Return code: gcc={gcc_rc} zcc={zcc_rc}'
    else:
        result.status = 'mismatch'
        result.error_msg = f'Output differs'

    return result


def minimize_crash(result: FuzzResult, zcc_path: str, tmpdir: str,
                   timeout: int) -> str:
    """Try to minimize a crashing/mismatching program."""
    source = result.source
    lines = source.split('\n')

    # Simple line-based minimization
    for i in range(len(lines) - 1, -1, -1):
        candidate = '\n'.join(lines[:i] + lines[i+1:])
        # Make sure it still has main
        if 'int main' not in candidate:
            continue

        gen_result = fuzz_one(result.seed, zcc_path, tmpdir,
                              FuzzConfig(), timeout)
        # Write the candidate and test
        src_path = os.path.join(tmpdir, 'minimize.c')
        with open(src_path, 'w') as f:
            f.write(candidate)

        try:
            # Compile with GCC to check validity
            gcc_check = subprocess.run(
                ['gcc', '-O0', '-std=c99', '-w', '-o',
                 os.path.join(tmpdir, 'min_gcc'), src_path, '-lm'],
                capture_output=True, timeout=10
            )
            if gcc_check.returncode != 0:
                continue

            # Check ZCC still crashes
            zcc_check = subprocess.run(
                [zcc_path, src_path, '-o', os.path.join(tmpdir, 'min_zcc')],
                capture_output=True, timeout=10
            )
            if result.status == 'crash':
                run_check = subprocess.run(
                    [os.path.join(tmpdir, 'min_zcc')],
                    capture_output=True, timeout=5
                )
                if run_check.returncode > 128:
                    lines = candidate.split('\n')
                    source = candidate
        except (subprocess.TimeoutExpired, FileNotFoundError):
            continue

    return source


def main():
    parser = argparse.ArgumentParser(description='ZCC Differential Fuzzer')
    parser.add_argument('--zcc', required=True, help='Path to ZCC binary')
    parser.add_argument('--count', type=int, default=200, help='Number of fuzz programs')
    parser.add_argument('--timeout', type=int, default=10, help='Per-program timeout (seconds)')
    parser.add_argument('--output-dir', default='./fuzz_results', help='Output directory')
    parser.add_argument('--seed', type=int, default=42, help='Base random seed')
    parser.add_argument('--verbose', action='store_true', help='Print each result')
    args = parser.parse_args()

    out_dir = Path(args.output_dir)
    crashes_dir = out_dir / 'crashes'
    mismatches_dir = out_dir / 'mismatches'
    crashes_dir.mkdir(parents=True, exist_ok=True)
    mismatches_dir.mkdir(parents=True, exist_ok=True)

    config = FuzzConfig()
    stats = {'pass': 0, 'fail': 0, 'crash': 0, 'timeout': 0,
             'gcc_fail': 0, 'zcc_fail': 0, 'mismatch': 0}

    print(f'ZCC Differential Fuzzer — {args.count} programs, seed={args.seed}')
    print(f'   ZCC: {args.zcc}')
    print(f'   Output: {out_dir}')
    print()

    start_time = time.time()

    with tempfile.TemporaryDirectory(prefix='zcc_fuzz_') as tmpdir:
        for i in range(args.count):
            seed = args.seed + i
            result = fuzz_one(seed, args.zcc, tmpdir, config, args.timeout)

            # Update stats
            if result.status == 'pass':
                stats['pass'] += 1
            elif result.status == 'crash':
                stats['crash'] += 1
                stats['fail'] += 1
                # Save crash
                crash_file = crashes_dir / f'crash_seed{seed}.c'
                with open(crash_file, 'w') as f:
                    f.write(result.source)
                with open(crashes_dir / f'crash_seed{seed}.log', 'w') as f:
                    f.write(f'Seed: {seed}\n')
                    f.write(f'Status: {result.status}\n')
                    f.write(f'Error: {result.error_msg}\n')
                    f.write(f'GCC rc: {result.gcc_rc}\n')
                    f.write(f'ZCC rc: {result.zcc_rc}\n')
            elif result.status == 'mismatch':
                stats['mismatch'] += 1
                stats['fail'] += 1
                # Save mismatch
                mm_file = mismatches_dir / f'mismatch_seed{seed}.c'
                with open(mm_file, 'w') as f:
                    f.write(result.source)
                with open(mismatches_dir / f'mismatch_seed{seed}.log', 'w') as f:
                    f.write(f'Seed: {seed}\n')
                    f.write(f'Error: {result.error_msg}\n')
                    f.write(f'GCC output: {result.gcc_output}\n')
                    f.write(f'ZCC output: {result.zcc_output}\n')
            elif result.status == 'gcc_fail':
                stats['gcc_fail'] += 1
            elif result.status == 'zcc_fail':
                stats['zcc_fail'] += 1
                stats['fail'] += 1
            elif result.status == 'timeout':
                stats['timeout'] += 1

            # Progress
            if args.verbose or result.status in ('crash', 'mismatch'):
                emoji = {'pass': 'âœ“', 'crash': 'ðŸ’¥', 'mismatch': 'â‰ ',
                          'zcc_fail': 'âœ—', 'gcc_fail': 'âŠ˜', 'timeout': 'â±'}
                sym = emoji.get(result.status, '?')
                print(f'  [{i+1:4d}/{args.count}] seed={seed:6d}  {sym} {result.status}'
                      f'{" â€” " + result.error_msg if result.error_msg else ""}')
            elif (i + 1) % 25 == 0:
                elapsed = time.time() - start_time
                rate = (i + 1) / elapsed if elapsed > 0 else 0
                print(f'  [{i+1:4d}/{args.count}] {stats["pass"]} pass, '
                      f'{stats["fail"]} fail, {stats["crash"]} crash '
                      f'({rate:.1f} prog/s)')

    elapsed = time.time() - start_time

    # Summary
    print()
    print(f'â•â•â• FUZZ SUMMARY â•â•â•')
    print(f'  Programs:    {args.count}')
    print(f'  Pass:        {stats["pass"]}')
    print(f'  Mismatch:    {stats["mismatch"]}')
    print(f'  Crash:       {stats["crash"]}')
    print(f'  ZCC fail:    {stats["zcc_fail"]}')
    print(f'  GCC fail:    {stats["gcc_fail"]} (bad test, ignored)')
    print(f'  Timeout:     {stats["timeout"]}')
    print(f'  Time:        {elapsed:.1f}s ({args.count/elapsed:.1f} prog/s)')
    print()

    if stats['crash'] > 0:
        print(f'  ðŸ’¥ {stats["crash"]} CRASHES saved to {crashes_dir}/')
    if stats['mismatch'] > 0:
        print(f'  â‰   {stats["mismatch"]} MISMATCHES saved to {mismatches_dir}/')

    # Write machine-readable summary
    summary = {
        'pass': stats['pass'],
        'fail': stats['fail'],
        'crash': stats['crash'],
        'mismatch': stats['mismatch'],
        'zcc_fail': stats['zcc_fail'],
        'gcc_fail': stats['gcc_fail'],
        'timeout': stats['timeout'],
        'total': args.count,
        'elapsed_s': round(elapsed, 2),
        'seed': args.seed,
    }
    with open(out_dir / 'summary.json', 'w') as f:
        json.dump(summary, f, indent=2)

    return 1 if stats['fail'] > 0 else 0


if __name__ == '__main__':
    sys.exit(main())
