#!/usr/bin/env python3
"""
run_zcc_jsonl_corpus.py — ZCC JSONL Corpus Runner

Reads a JSONL corpus file containing ``type: "test_case"`` records,
compiles and runs each test against the target compiler (default: ./zcc)
and a reference compiler (default: cc), then emits one ``type: "test_run"``
JSONL record per test to stdout with raw command outputs, exit codes, and
pass/fail/block status.

Exit code: 0 if all tests pass, 1 if any test fails or blocks unexpectedly.

Usage
-----
  python3 tests/run_zcc_jsonl_corpus.py tests/codegen/zcc_ir_phi_liveness_corpus.jsonl
  ZCC=./zcc2 CC=gcc python3 tests/run_zcc_jsonl_corpus.py <corpus>
  python3 tests/run_zcc_jsonl_corpus.py <corpus> --target ./zcc2 --reference gcc

Command template tokens (expanded in each command string)
----------------------------------------------------------
  {ZCC}    target compiler binary     (--target / $ZCC, default ./zcc)
  {CC}     reference compiler binary  (--reference / $CC, default cc)
  {SRC}    path to the materialised source file
  {ASM}    path for generated assembly output
  {EXE}    path for the linked target binary
  {REF}    path for the linked reference binary
  {LDFLAGS} extra link flags          (default: -lm)

Oracle fields
-------------
  compare        list of dimensions to check: "exit_code", "stdout"
  expected_exit  integer expected exit code  (compared against target)
  expected_stdout string expected stdout      (compared against target)
  reference      "cc" — if present, the reference_run result is used as
                 the authoritative oracle for the listed compare dimensions
                 instead of (or in addition to) the expected_* literals.
                 When both reference and expected_* are given, BOTH must
                 match (target matches reference AND target matches literal).

Evidence Invariant
------------------
No test is reported PASS unless the runner actually executed it and the
raw output is present in the test_run record.  Blocked tests (where
compilation failed) are reported as status "block", not "pass".
"""

import argparse
import base64
import json
import os
import subprocess
import sys
import tempfile


# ---------------------------------------------------------------------------
# Subprocess helpers
# ---------------------------------------------------------------------------

def _run(cmd, cwd, timeout=30):
    """Run *cmd* in a shell, return dict with exit_code/stdout/stderr."""
    try:
        proc = subprocess.run(
            cmd,
            shell=True,
            cwd=cwd,
            capture_output=True,
            timeout=timeout,
        )
        # Decode bytes to str; use 'replace' so assembler/linker error
        # messages that embed binary file content do not raise UnicodeDecodeError.
        stdout = proc.stdout.decode("utf-8", errors="replace")
        stderr = proc.stderr.decode("utf-8", errors="replace")
        return {
            "cmd": cmd,
            "exit_code": proc.returncode,
            "stdout": stdout,
            "stderr": stderr,
        }
    except subprocess.TimeoutExpired:
        return {
            "cmd": cmd,
            "exit_code": -1,
            "stdout": "",
            "stderr": f"TIMEOUT after {timeout}s",
        }
    except (subprocess.SubprocessError, OSError) as exc:
        return {
            "cmd": cmd,
            "exit_code": -1,
            "stdout": "",
            "stderr": str(exc),
        }


def _expand(template, subs):
    """Expand {TOKEN} placeholders in *template* using *subs* dict."""
    result = template
    for key, value in subs.items():
        result = result.replace("{" + key + "}", value)
    return result


# ---------------------------------------------------------------------------
# Test execution
# ---------------------------------------------------------------------------

def _materialise_source(src_info, tmpdir):
    """Write source to tmpdir; return path or raise ValueError."""
    filename = src_info.get("filename", "test.c")
    path = os.path.join(tmpdir, filename)

    src_type = src_info.get("type", "inline")
    if src_type == "inline":
        text = src_info.get("text", "")
        with open(path, "w") as fh:
            fh.write(text)
    elif src_type == "base64":
        data = base64.b64decode(src_info["data"])
        with open(path, "wb") as fh:
            fh.write(data)
    else:
        raise ValueError(f"Unknown source type: {src_type!r}")

    return path


def run_test_case(tc, zcc, reference, ldflags="-lm", compile_timeout=60, run_timeout=15):
    """
    Execute one test_case record.

    Returns a ``test_run`` dict suitable for JSON serialisation.
    """
    test_id = tc.get("id", "<unknown>")
    result = {
        "type": "test_run",
        "id": test_id,
        "status": "block",
        "reason": None,
        "target_compile": None,
        "reference_compile": None,
        "target_run": None,
        "reference_run": None,
    }

    with tempfile.TemporaryDirectory(prefix="zcc_corpus_") as tmpdir:
        # 1. Materialise source
        try:
            src_path = _materialise_source(tc["source"], tmpdir)
        except (KeyError, ValueError, OSError, base64.binascii.Error) as exc:
            result["reason"] = f"source materialisation failed: {exc}"
            return result

        # 2. Substitution context
        asm_path = os.path.join(tmpdir, "target.s")
        exe_path = os.path.join(tmpdir, "target")
        ref_path = os.path.join(tmpdir, "reference")

        subs = {
            "ZCC": zcc,
            "CC": reference,
            "SRC": src_path,
            "ASM": asm_path,
            "EXE": exe_path,
            "REF": ref_path,
            "LDFLAGS": ldflags,
        }

        cmds = tc.get("commands", {})

        # 3. target_compile
        cmd_tc = cmds.get("target_compile")
        if cmd_tc:
            r = _run(_expand(cmd_tc, subs), cwd=tmpdir, timeout=compile_timeout)
            result["target_compile"] = r
            if r["exit_code"] != 0:
                result["reason"] = (
                    f"target_compile failed (exit {r['exit_code']}): "
                    + r["stderr"][:300]
                )
                return result

        # 4. reference_compile (optional — only needed for reference oracle)
        cmd_rc = cmds.get("reference_compile")
        if cmd_rc:
            r = _run(_expand(cmd_rc, subs), cwd=tmpdir, timeout=compile_timeout)
            result["reference_compile"] = r
            if r["exit_code"] != 0:
                result["reason"] = (
                    f"reference_compile failed (exit {r['exit_code']}): "
                    + r["stderr"][:300]
                )
                return result

        # 5. target_run
        cmd_tr = cmds.get("target_run")
        if cmd_tr:
            r = _run(_expand(cmd_tr, subs), cwd=tmpdir, timeout=run_timeout)
            result["target_run"] = r

        # 6. reference_run (optional)
        cmd_rr = cmds.get("reference_run")
        if cmd_rr:
            r = _run(_expand(cmd_rr, subs), cwd=tmpdir, timeout=run_timeout)
            result["reference_run"] = r

        # 7. Compare
        oracle = tc.get("oracle", {})
        compare = oracle.get("compare", ["exit_code"])
        use_reference = "reference" in oracle  # use ref output as oracle when present

        target_run_data = result.get("target_run") or {}
        ref_run_data = result.get("reference_run") or {}

        mismatches = []

        for dim in compare:
            if dim == "exit_code":
                actual = target_run_data.get("exit_code")
                literal_expected = oracle.get("expected_exit")
                if use_reference and result.get("reference_run"):
                    expected = ref_run_data.get("exit_code")
                    if expected is None:
                        mismatches.append("exit_code: reference_run missing exit_code")
                        continue
                else:
                    expected = literal_expected
                if actual != expected:
                    mismatches.append(
                        f"exit_code: got {actual!r}, want {expected!r}"
                    )
                elif (
                    use_reference
                    and literal_expected is not None
                    and actual != literal_expected
                ):
                    mismatches.append(
                        f"exit_code: got {actual!r}, literal oracle says {literal_expected!r}"
                    )

            elif dim == "stdout":
                actual = target_run_data.get("stdout", "")
                literal_expected = oracle.get("expected_stdout")
                if use_reference and result.get("reference_run"):
                    expected = ref_run_data.get("stdout", "")
                else:
                    expected = literal_expected if literal_expected is not None else ""
                if actual != expected:
                    mismatches.append(
                        f"stdout: got {actual!r}, want {expected!r}"
                    )
                elif (
                    use_reference
                    and literal_expected is not None
                    and actual != literal_expected
                ):
                    mismatches.append(
                        f"stdout: got {actual!r}, literal oracle says {literal_expected!r}"
                    )

            elif dim == "stderr":
                actual = target_run_data.get("stderr", "")
                if use_reference and result.get("reference_run"):
                    expected = ref_run_data.get("stderr", "")
                else:
                    expected = oracle.get("expected_stderr", "")
                if actual != expected:
                    mismatches.append(
                        f"stderr: got {actual!r}, want {expected!r}"
                    )

        if not mismatches:
            result["status"] = "pass"
            result["reason"] = None
        else:
            result["status"] = "fail"
            result["reason"] = "; ".join(mismatches)

    return result


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def _parse_args(argv=None):
    ap = argparse.ArgumentParser(
        description="Run a ZCC JSONL test corpus and emit test_run records.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    ap.add_argument(
        "corpus",
        help="Path to the JSONL corpus file.",
    )
    ap.add_argument(
        "--target",
        default=os.environ.get("ZCC", "./zcc"),
        metavar="BINARY",
        help="Target compiler binary (default: $ZCC or ./zcc).",
    )
    ap.add_argument(
        "--reference",
        default=os.environ.get("CC", "cc"),
        metavar="BINARY",
        help="Reference compiler binary (default: $CC or cc).",
    )
    ap.add_argument(
        "--ldflags",
        default="-lm",
        metavar="FLAGS",
        help="Extra linker flags appended when linking (default: -lm).",
    )
    ap.add_argument(
        "--list",
        action="store_true",
        help="List test IDs found in the corpus and exit without running.",
    )
    ap.add_argument(
        "--compile-timeout",
        type=int,
        default=60,
        metavar="SECS",
        help="Per-test compilation timeout in seconds (default: 60).",
    )
    ap.add_argument(
        "--run-timeout",
        type=int,
        default=15,
        metavar="SECS",
        help="Per-test execution timeout in seconds (default: 15).",
    )
    return ap.parse_args(argv)


def main(argv=None):
    args = _parse_args(argv)

    # Resolve target to absolute path if it starts with ./ or ../ so that
    # subprocesses running from a temporary work directory can find it.
    zcc = args.target
    if zcc.startswith("./") or zcc.startswith("../"):
        zcc = os.path.abspath(zcc)

    reference = args.reference

    # Load corpus
    test_cases = []
    try:
        with open(args.corpus) as fh:
            for lineno, raw in enumerate(fh, 1):
                raw = raw.strip()
                if not raw or raw.startswith("#"):
                    continue
                try:
                    obj = json.loads(raw)
                except json.JSONDecodeError as exc:
                    print(
                        f"ERROR: {args.corpus}:{lineno}: invalid JSON: {exc}",
                        file=sys.stderr,
                    )
                    sys.exit(1)
                if obj.get("type") == "test_case":
                    test_cases.append(obj)
    except OSError as exc:
        print(f"ERROR: cannot open corpus {args.corpus!r}: {exc}", file=sys.stderr)
        sys.exit(1)

    if not test_cases:
        print(f"WARNING: no test_case records found in {args.corpus!r}", file=sys.stderr)
        sys.exit(0)

    if args.list:
        for tc in test_cases:
            print(tc.get("id", "<no id>"))
        return

    # Summary header (to stderr so stdout stays clean JSONL)
    print(
        f"corpus: {args.corpus}  target: {zcc}  reference: {reference}  "
        f"tests: {len(test_cases)}",
        file=sys.stderr,
    )

    failures = 0
    blocks = 0

    for tc in test_cases:
        test_id = tc.get("id", "<unknown>")
        result = run_test_case(
            tc,
            zcc=zcc,
            reference=reference,
            ldflags=args.ldflags,
            compile_timeout=args.compile_timeout,
            run_timeout=args.run_timeout,
        )
        print(json.dumps(result, ensure_ascii=False))

        status = result["status"]
        reason = result.get("reason") or ""

        if status == "pass":
            print(f"  PASS  {test_id}", file=sys.stderr)
        elif status == "block":
            print(f"  BLOCK {test_id}: {reason}", file=sys.stderr)
            blocks += 1
        else:
            print(f"  FAIL  {test_id}: {reason}", file=sys.stderr)
            failures += 1

    # Final summary to stderr
    total = len(test_cases)
    passed = total - failures - blocks
    print(
        f"\nresults: {passed}/{total} passed, {failures} failed, {blocks} blocked",
        file=sys.stderr,
    )

    if failures > 0 or blocks > 0:
        sys.exit(1)


if __name__ == "__main__":
    main()
