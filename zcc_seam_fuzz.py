#!/usr/bin/env python3
"""
zcc_seam_fuzz.py — targeted seam differential fuzzer for ZCC.

Companion to harness/zcc_fuzz.py. The broad fuzzer biases toward
sign-extension / division / shift-by-variable and, by design, routes
*around* four seams where real bugs were found by the kernel:

  regpressure : many simultaneously-live longs + memory traffic, to force
                ZCC's allocator into r8-r15 mixed with low regs in mem ops
                (REX.R/REX.B direction; the one-high-one-low path).
  packed      : __attribute__((packed)) structs with a >=8-byte field not at
                offset 0 (FORENSIC-025). Prints sizeof so a packing miscompile
                shows up directly, and serves as the regression test for the fix.
  bytemem     : char/short/int pointer round-trips with signed+unsigned
                readback (movb/movw stores, movsbq/movswq/movzbq/movzwq loads).
  shiftimm    : shifts by *literal constants* (the 0xc1 path) — NOT masked
                variables (the 0xd3 path the broad fuzzer uses).

Each generated program is header-free ZCC-subset C: forward-declares printf,
computes a 64-bit checksum, prints one deterministic line. A codegen error in
the targeted seam changes the checksum (or sizeof), so ZCC-vs-gcc stdout diff
is the oracle. Everything is seeded: every run is replayable from --seed.

Usage:
  # self-test the generator against gcc only (no ZCC needed):
  python3 zcc_seam_fuzz.py --self-test --count 200

  # real differential run against your ZCC:
  python3 zcc_seam_fuzz.py --zcc ./zcc --count 1000 --output-dir ./seamfuzz

  # one seam, reproducible, keep asm on mismatch:
  python3 zcc_seam_fuzz.py --zcc ./zcc --seam shiftimm --seed 42 --count 50 --keep-asm
"""
import argparse
import os
import random
import subprocess
import sys
import tempfile

SEAMS = ["regpressure", "packed", "bytemem", "shiftimm"]
PREAMBLE = "int printf(const char *, ...);\n"


def _rc(rng):
    """A non-trivial constant, biased toward boundary values."""
    pool = [0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0x100000000,
            0x7FFFFFFFFFFFFFFF, -1, -2147483648, 0, 1, 255, 256, 65535, 65536]
    if rng.random() < 0.4:
        return rng.choice(pool)
    return rng.randint(-(1 << 40), 1 << 40)


def gen_regpressure(rng):
    n = rng.randint(16, 24)
    locals_ = [f"l{i}" for i in range(n)]
    lines = [PREAMBLE, "int main(void){"]
    lines.append("  long " + ", ".join(locals_) + ";")
    lines.append("  long arr[8];")
    lines.append("  unsigned long acc = 0;")
    lines.append("  int j;")
    lines.append("  for (j = 0; j < 8; j = j + 1) arr[j] = 0;")
    for i, name in enumerate(locals_):
        lines.append(f"  {name} = {_rc(rng)}L;")
    # interleave: store, cross-mix many live values, load back -> high liveness
    for k in range(rng.randint(20, 36)):
        op = rng.choice(["+", "-", "^", "|", "&"])
        a, b = rng.sample(locals_, 2)
        dst = rng.choice(locals_)
        lines.append(f"  {dst} = {a} {op} {b};")
        if rng.random() < 0.5:
            idx = rng.randint(0, 7)
            lines.append(f"  arr[{idx}] = {rng.choice(locals_)};")
        if rng.random() < 0.5:
            idx = rng.randint(0, 7)
            tgt = rng.choice(locals_)
            lines.append(f"  {tgt} = arr[{idx}] ^ {tgt};")
    for name in locals_:
        lines.append(f"  acc = acc * 1000003UL + (unsigned long){name};")
    for i in range(8):
        lines.append(f"  acc = acc * 1000003UL + (unsigned long)arr[{i}];")
    lines.append('  printf("%llu\\n", (unsigned long long)acc);')
    lines.append("  return 0;")
    lines.append("}")
    return "\n".join(lines) + "\n"


def gen_packed(rng):
    # >=8-byte field deliberately NOT at offset 0 — the FORENSIC-025 shape.
    a, b, c, d = _rc(rng) & 0xFFFF, _rc(rng), _rc(rng) & 0xFF, _rc(rng) & 0xFFFFFFFF
    lines = [PREAMBLE]
    lines.append("struct P { unsigned short a; unsigned long b; "
                 "unsigned char c; unsigned int d; } __attribute__((packed));")
    lines.append("struct U { unsigned short a; unsigned long b; "
                 "unsigned char c; unsigned int d; };")
    lines.append("int main(void){")
    lines.append("  struct P p; struct U u; unsigned long acc = 0;")
    lines.append("  struct P parr[2];")
    for fld, val in (("a", a), ("b", b), ("c", c), ("d", d)):
        lines.append(f"  p.{fld} = {val}UL;")
        lines.append(f"  u.{fld} = {val}UL;")
        lines.append(f"  parr[0].{fld} = {val}UL;")
        lines.append(f"  parr[1].{fld} = {val + 1}UL;")
    for fld in ("a", "b", "c", "d"):
        lines.append(f"  acc = acc * 1000003UL + (unsigned long)p.{fld};")
        lines.append(f"  acc = acc * 1000003UL + (unsigned long)u.{fld};")
        lines.append(f"  acc = acc * 1000003UL + (unsigned long)parr[0].{fld};")
        lines.append(f"  acc = acc * 1000003UL + (unsigned long)parr[1].{fld};")
    lines.append('  printf("PSZ=%lu USZ=%lu ARSZ=%lu ACC=%llu\\n", '
                 "(unsigned long)sizeof(struct P), "
                 "(unsigned long)sizeof(struct U), "
                 "(unsigned long)sizeof(parr), (unsigned long long)acc);")
    lines.append("  return 0;")
    lines.append("}")
    return "\n".join(lines) + "\n"


def gen_bytemem(rng):
    n = rng.randint(4, 8)
    lines = [PREAMBLE, "int main(void){"]
    lines.append(f"  char cb[{n}]; short sb[{n}]; int ib[{n}];")
    lines.append("  unsigned long acc = 0; int i;")
    for i in range(n):
        lines.append(f"  cb[{i}] = (char)({_rc(rng)});")
        lines.append(f"  sb[{i}] = (short)({_rc(rng)});")
        lines.append(f"  ib[{i}] = (int)({_rc(rng)});")
    lines.append(f"  for (i = 0; i < {n}; i = i + 1) {{")
    # signed + unsigned readback => sign-extend and zero-extend load paths
    lines.append("    acc = acc * 1000003UL + (unsigned long)(long)cb[i];")
    lines.append("    acc = acc * 1000003UL + (unsigned long)(unsigned char)cb[i];")
    lines.append("    acc = acc * 1000003UL + (unsigned long)(long)sb[i];")
    lines.append("    acc = acc * 1000003UL + (unsigned long)(unsigned short)sb[i];")
    lines.append("    acc = acc * 1000003UL + (unsigned long)(long)ib[i];")
    lines.append("    acc = acc * 1000003UL + (unsigned long)(unsigned int)ib[i];")
    lines.append("  }")
    lines.append('  printf("%llu\\n", (unsigned long long)acc);')
    lines.append("  return 0;")
    lines.append("}")
    return "\n".join(lines) + "\n"


def gen_shiftimm(rng):
    lines = [PREAMBLE, "int main(void){"]
    lines.append("  unsigned long acc = 0;")
    nvars = rng.randint(4, 7)
    for v in range(nvars):
        sl = f"sl{v}"; ul = f"ul{v}"; si = f"si{v}"; ui = f"ui{v}"
        lines.append(f"  long {sl} = {_rc(rng)}L;")
        lines.append(f"  unsigned long {ul} = {_rc(rng) & ((1<<64)-1)}UL;")
        lines.append(f"  int {si} = {_rc(rng) & 0xFFFFFFFF};")
        lines.append(f"  unsigned int {ui} = {_rc(rng) & 0xFFFFFFFF}U;")
        for _ in range(rng.randint(4, 8)):
            amt64 = rng.randint(1, 63)
            amt32 = rng.randint(1, 31)
            # signed operands -> right shift only (arithmetic sar /7, defined).
            # unsigned operands -> both shifts (shl /4, shr /5, defined).
            tgt = rng.choice([
                f"({sl} >> {amt64})",   # sarq /7
                f"({si} >> {amt32})",   # sar  /7 (32-bit)
                f"({ul} << {amt64})",   # shlq /4
                f"({ul} >> {amt64})",   # shrq /5
                f"({ui} << {amt32})",   # shl  /4 (32-bit)
                f"({ui} >> {amt32})",   # shr  /5 (32-bit)
            ])
            lines.append(f"  acc = acc * 1000003UL + (unsigned long)(long){tgt};")
    lines.append('  printf("%llu\\n", (unsigned long long)acc);')
    lines.append("  return 0;")
    lines.append("}")
    return "\n".join(lines) + "\n"


GENERATORS = {
    "regpressure": gen_regpressure,
    "packed": gen_packed,
    "bytemem": gen_bytemem,
    "shiftimm": gen_shiftimm,
}


def _run(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, timeout=30, **kw)


def compile_run(compiler_cmd, src_path, exe_path, is_zcc):
    """Returns (ok, output_or_error). compiler_cmd is a list ending before in/out."""
    try:
        if is_zcc:
            # ZCC -> .s, then gcc assembles/links (matches the zkernel pipeline split)
            s_path = exe_path + ".s"
            c = _run(compiler_cmd + [src_path, "-o", s_path])
            if c.returncode != 0:
                return False, "ZCC_COMPILE_FAIL\n" + c.stderr
            a = _run(["gcc", "-O0", s_path, "-o", exe_path, "-lm"])
            if a.returncode != 0:
                return False, "ZCC_ASM_LINK_FAIL\n" + a.stderr
        else:
            c = _run(compiler_cmd + ["-O0", src_path, "-o", exe_path, "-lm"])
            if c.returncode != 0:
                return False, "GCC_COMPILE_FAIL\n" + c.stderr
        r = _run([exe_path])
        if r.returncode != 0:
            return False, f"RUNTIME_FAIL rc={r.returncode}\n{r.stdout}{r.stderr}"
        return True, r.stdout
    except subprocess.TimeoutExpired:
        return False, "TIMEOUT"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--zcc", default=None, help="path to zcc binary; omit for --self-test")
    ap.add_argument("--count", type=int, default=200)
    ap.add_argument("--seed", type=int, default=None)
    ap.add_argument("--seam", choices=SEAMS + ["all"], default="all")
    ap.add_argument("--output-dir", default="./seamfuzz")
    ap.add_argument("--keep-asm", action="store_true")
    ap.add_argument("--self-test", action="store_true",
                    help="validate generator against gcc only (compile+run x2 for determinism)")
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()

    base_seed = args.seed if args.seed is not None else random.randrange(1 << 30)
    seams = SEAMS if args.seam == "all" else [args.seam]
    os.makedirs(args.output_dir, exist_ok=True)
    mism_dir = os.path.join(args.output_dir, "mismatches")
    os.makedirs(mism_dir, exist_ok=True)

    n_pass = n_fail = n_gen_bad = 0
    fails = []
    tmp = tempfile.mkdtemp(prefix="seamfuzz_")

    for i in range(args.count):
        seed = base_seed + i
        rng = random.Random(seed)
        seam = seams[i % len(seams)]
        src = GENERATORS[seam](rng)
        cpath = os.path.join(tmp, f"t{seam}_{seed}.c")
        with open(cpath, "w") as f:
            f.write(src)

        if args.self_test:
            # generator validity + determinism, gcc only
            ok1, o1 = compile_run(["gcc"], cpath, os.path.join(tmp, "a1"), False)
            ok2, o2 = compile_run(["gcc"], cpath, os.path.join(tmp, "a2"), False)
            if not ok1:
                n_gen_bad += 1
                fails.append((seam, seed, "GEN_INVALID", o1))
                if args.verbose:
                    print(f"[GEN-BAD] {seam} seed={seed}\n{o1}", file=sys.stderr)
            elif o1 != o2:
                n_fail += 1
                fails.append((seam, seed, "NONDETERMINISTIC", f"{o1!r} vs {o2!r}"))
            else:
                n_pass += 1
            continue

        # real differential
        gok, gout = compile_run(["gcc"], cpath, os.path.join(tmp, "gref"), False)
        if not gok:
            n_gen_bad += 1
            if args.verbose:
                print(f"[GEN-BAD] {seam} seed={seed} (gcc rejected own ref)\n{gout}",
                      file=sys.stderr)
            continue
        zok, zout = compile_run([args.zcc], cpath, os.path.join(tmp, "zout"), True)
        if (not zok) or (zout != gout):
            n_fail += 1
            stem = os.path.join(mism_dir, f"{seam}_seed{seed}")
            with open(stem + ".c", "w") as f:
                f.write(src)
            with open(stem + ".diff", "w") as f:
                f.write(f"seam={seam} seed={seed}\n--- gcc ---\n{gout}\n--- zcc ---\n{zout}\n")
            if args.keep_asm:
                _run(["gcc", "-S", "-O0", cpath, "-o", stem + ".gcc.s"])
                _run([args.zcc, cpath, "-o", stem + ".zcc.s"])
            fails.append((seam, seed, "ZCC_FAIL" if not zok else "MISMATCH", ""))
            if args.verbose:
                print(f"[FAIL] {seam} seed={seed} -> {stem}.c", file=sys.stderr)
        else:
            n_pass += 1

    print(f"\nseed base   : {base_seed}")
    print(f"seams       : {','.join(seams)}")
    print(f"programs    : {args.count}")
    print(f"pass        : {n_pass}")
    print(f"fail        : {n_fail}")
    print(f"gen-invalid : {n_gen_bad}")
    if fails:
        print("\nfirst failures:")
        for seam, seed, kind, _ in fails[:10]:
            print(f"  {kind:16} {seam:12} seed={seed}")
        if not args.self_test:
            print(f"\nreproducers in: {mism_dir}/")
    sys.exit(1 if (n_fail or n_gen_bad) else 0)


if __name__ == "__main__":
    main()
