#!/usr/bin/env python3
import os
import pathlib
import subprocess
import sys
import tempfile


def run(cmd, cwd, check=True, capture=True):
    return subprocess.run(
        cmd,
        cwd=cwd,
        check=check,
        text=True,
        capture_output=capture,
    )


def main() -> int:
    repo = pathlib.Path(__file__).resolve().parents[2]
    # If `G:\zcc\zcc` is a Linux ELF (typical when built under WSL), Windows reports WinError 193.
    # Run the harness inside WSL instead (no ZCC needed):
    #   wsl -e bash -lc "cd /mnt/g/zcc && python3 tests/rust/test_rust_frontend.py"
    # Or point ZCC at a single native compiler path (e.g. MinGW-built zcc.exe).
    _zcc_env = os.environ.get("ZCC")
    if _zcc_env:
        zcc_cmd = [_zcc_env]
    else:
        zcc_cmd = [str(repo / "zcc")]
    c_smoke_asm = str(pathlib.Path(tempfile.gettempdir()) / "zcc_v2_smoke.s")
    expected_ast = (repo / "tests/rust/expected_smoke_ok.ast").read_text(encoding="utf-8")
    expected_dup = (repo / "tests/rust/expected_duplicate-local.stderr").read_text(encoding="utf-8")
    expected_dup_param = (repo / "tests/rust/expected_duplicate-param.stderr").read_text(encoding="utf-8")
    expected_dup_fn = (repo / "tests/rust/expected_duplicate-function.stderr").read_text(encoding="utf-8")
    expected_unknown = (repo / "tests/rust/expected_unknown-name.stderr").read_text(encoding="utf-8")
    expected_assign_immutable = "error[RUST-E0003]"
    expected_sym_ok = (repo / "tests/rust/expected_smoke_ok.symbols").read_text(encoding="utf-8")
    expected_sym_unknown = (repo / "tests/rust/expected_unknown-name.symbols").read_text(encoding="utf-8")
    expected_symtab_smoke = (repo / "tests/rust/expected_symbol-table-smoke.symtab").read_text(encoding="utf-8")
    expected_symtab_shadow = (repo / "tests/rust/expected_symbol-table-shadowing.symtab").read_text(encoding="utf-8")
    expected_symtab_dup = (repo / "tests/rust/expected_symbol-table-duplicate.symtab").read_text(encoding="utf-8")
    expected_symtab_dup_stderr = (repo / "tests/rust/expected_symbol-table-duplicate.stderr").read_text(encoding="utf-8")
    expected_symtab_long = (repo / "tests/rust/expected_symbol-table-long-name.symtab").read_text(encoding="utf-8")
    expected_type_return_bool = (repo / "tests/rust/expected_type-return-bool.stderr").read_text(encoding="utf-8")
    expected_type_if_int = (repo / "tests/rust/expected_type-if-int-condition.stderr").read_text(encoding="utf-8")
    expected_type_while_int = (repo / "tests/rust/expected_type-while-int-condition.stderr").read_text(encoding="utf-8")
    expected_type_binary_bool = (repo / "tests/rust/expected_type-binary-bool.stderr").read_text(encoding="utf-8")
    expected_type_missing_return = (repo / "tests/rust/expected_type-missing-return.stderr").read_text(encoding="utf-8")
    expected_type_if_missing_else = (repo / "tests/rust/expected_type-if-missing-else-return.stderr").read_text(encoding="utf-8")
    expected_type_while_return_not_enough = (repo / "tests/rust/expected_type-while-return-not-enough.stderr").read_text(encoding="utf-8")
    expected_type_call_local = (repo / "tests/rust/expected_type-call-local.stderr").read_text(encoding="utf-8")
    expected_type_call_arity_few = (repo / "tests/rust/expected_type-call-arity-too-few.stderr").read_text(encoding="utf-8")
    expected_type_call_arity_many = (repo / "tests/rust/expected_type-call-arity-too-many.stderr").read_text(encoding="utf-8")
    expected_type_call_arg_bool = (repo / "tests/rust/expected_type-call-arg-bool.stderr").read_text(encoding="utf-8")
    expected_type_compare_return_bool = (repo / "tests/rust/expected_type-compare-return-bool.stderr").read_text(encoding="utf-8")
    expected_type_compare_bool = (repo / "tests/rust/expected_type-compare-bool.stderr").read_text(encoding="utf-8")
    expected_type_compare_chained = (repo / "tests/rust/expected_type-compare-chained.stderr").read_text(encoding="utf-8")
    expected_type_logic_not_int = (repo / "tests/rust/expected_type-logic-not-int.stderr").read_text(encoding="utf-8")
    expected_type_logic_and_int = (repo / "tests/rust/expected_type-logic-and-int.stderr").read_text(encoding="utf-8")
    expected_type_let_bool_mismatch = (repo / "tests/rust/expected_type-let-annot-bool-mismatch.stderr").read_text(encoding="utf-8")
    expected_strict_let_missing_annotation = (repo / "tests/rust/expected_strict-let-missing-annotation.stderr").read_text(encoding="utf-8")
    expected_strict_signature_missing_ret = (repo / "tests/rust/expected_strict-signature-missing-return-type.stderr").read_text(encoding="utf-8")
    expected_lower_return_int = (repo / "tests/rust/expected_lower-return-int.rir").read_text(encoding="utf-8")
    expected_lower_param_add = (repo / "tests/rust/expected_lower-param-add.rir").read_text(encoding="utf-8")
    expected_lower_if_else = (repo / "tests/rust/expected_lower-if-else.rir").read_text(encoding="utf-8")
    expected_lower_compare_eq = (repo / "tests/rust/expected_lower-compare-eq.rir").read_text(encoding="utf-8")
    expected_lower_nested_if = (repo / "tests/rust/expected_lower-nested-if.rir").read_text(encoding="utf-8")
    expected_lower_while = (repo / "tests/rust/expected_lower-while.rir").read_text(encoding="utf-8")
    expected_lower_while_if = (repo / "tests/rust/expected_lower-while-if.rir").read_text(encoding="utf-8")
    expected_lower_call = (repo / "tests/rust/expected_lower-call.rir").read_text(encoding="utf-8")
    expected_lower_call_forward = (repo / "tests/rust/expected_lower-call-forward.rir").read_text(encoding="utf-8")
    expected_lower_logic = (repo / "tests/rust/expected_lower-logic.rir").read_text(encoding="utf-8")
    expected_backend_div_zero = (repo / "tests/rust/expected_backend-div-zero.stderr").read_text(encoding="utf-8")
    expected_backend_unsupported_expr_stmt = (repo / "tests/rust/expected_backend-unsupported-expr-stmt.stderr").read_text(encoding="utf-8")
    expected_backend_call_recursive = (repo / "tests/rust/expected_backend-call-recursive.stderr").read_text(encoding="utf-8")
    expected_backend_call_mutual_recursive = (repo / "tests/rust/expected_backend-call-mutual-recursive.stderr").read_text(encoding="utf-8")
    expected_backend_call_param_recursive = (repo / "tests/rust/expected_backend-call-param-recursive.stderr").read_text(encoding="utf-8")

    print("[rust] c-frontend compatibility smoke")
    c_smoke = run([*zcc_cmd, "tests/v2_test.c", "-o", c_smoke_asm], cwd=repo, check=True)
    if "ZCC Engine Compilation Terminated Successfully." not in c_smoke.stdout:
        print("c smoke output missing success marker")
        print(c_smoke.stdout)
        return 1

    print("[rust] parse valid source + dump ast")
    ok = run([*zcc_cmd, "tests/rust/smoke_ok.rs", "--dump-rust-ast"], cwd=repo, check=True)
    if ok.stdout != expected_ast:
        print("ast output mismatch")
        print("=== expected ===")
        print(expected_ast)
        print("=== actual ===")
        print(ok.stdout)
        return 1
    if ok.stderr != "":
        print("expected empty stderr for --dump-rust-ast success")
        print(ok.stderr)
        return 1

    print("[rust] gated symbol dump for valid source")
    sym_ok = run([*zcc_cmd, "tests/rust/smoke_ok.rs", "--dump-rust-ast-with-symbols"], cwd=repo, check=True)
    if sym_ok.stdout != expected_sym_ok:
        print("symbol dump mismatch for valid source")
        print("=== expected ===")
        print(expected_sym_ok)
        print("=== actual ===")
        print(sym_ok.stdout)
        return 1
    if sym_ok.stderr != "":
        print("expected empty stderr for --dump-rust-ast-with-symbols success")
        print(sym_ok.stderr)
        return 1

    print("[rust] malformed source must fail")
    bad = run([*zcc_cmd, "tests/rust/smoke_bad.rs"], cwd=repo, check=False)
    bad_text = bad.stdout + bad.stderr
    if bad.returncode == 0:
        print("expected failure for smoke_bad.rs")
        print(bad_text)
        return 1
    if "error[RUSTPARSE001]" not in bad_text:
        print("expected structured parse diagnostic")
        print(bad_text)
        return 1

    print("[rust] rust path rejects c-preprocessor mode")
    pp_bad = run([*zcc_cmd, "tests/rust/smoke_ok.rs", "--pp-only"], cwd=repo, check=False)
    if pp_bad.returncode == 0:
        print("expected --pp-only on Rust source to fail")
        print(pp_bad.stdout)
        return 1
    if "--pp-only is only valid for C sources" not in pp_bad.stdout:
        print("missing clear --pp-only rejection message")
        print(pp_bad.stdout)
        return 1

    print("[rust] duplicate-local resolver diagnostic")
    dup = run([*zcc_cmd, "tests/rust/duplicate-local.rs"], cwd=repo, check=False)
    dup_text = dup.stdout + dup.stderr
    if dup.returncode == 0:
        print("expected duplicate-local.rs to fail")
        print(dup_text)
        return 1
    dup_lines = expected_dup.splitlines()
    if any(line not in dup_text for line in dup_lines):
        print("duplicate-local diagnostic mismatch")
        print(dup_text)
        return 1

    print("[rust] unknown-name resolver diagnostic")
    unk = run([*zcc_cmd, "tests/rust/unknown-name.rs"], cwd=repo, check=False)
    unk_text = unk.stdout + unk.stderr
    if unk.returncode == 0:
        print("expected unknown-name.rs to fail")
        print(unk_text)
        return 1
    if expected_unknown.splitlines()[0] not in unk_text or expected_unknown.splitlines()[1] not in unk_text:
        print("unknown-name diagnostic mismatch")
        print(unk_text)
        return 1
    if "RUST-TYPE-E9999" in unk_text:
        print("unknown-name should not reach typechecker")
        print(unk_text)
        return 1

    print("[rust] gated symbol dump for unknown name")
    unk_sym = run([*zcc_cmd, "tests/rust/unknown-name.rs", "--dump-rust-ast-with-symbols"], cwd=repo, check=False)
    if unk_sym.returncode == 0:
        print("expected unknown-name with symbols to fail")
        print(unk_sym.stdout)
        return 1
    if expected_sym_unknown not in unk_sym.stdout:
        print("symbol dump mismatch for unknown-name")
        print("=== expected ===")
        print(expected_sym_unknown)
        print("=== actual ===")
        print(unk_sym.stdout)
        return 1
    if unk_sym.stderr != expected_unknown:
        print("unknown-name stderr mismatch for ast-with-symbols")
        print("=== expected ===")
        print(expected_unknown)
        print("=== actual ===")
        print(unk_sym.stderr)
        return 1

    print("[rust] forward function reference")
    fwd = run([*zcc_cmd, "tests/rust/forward-function.rs"], cwd=repo, check=True)
    if "[OK] Rust frontend parsed successfully." not in fwd.stdout:
        print("forward-function should resolve cleanly")
        print(fwd.stdout)
        return 1

    print("[rust] duplicate function name")
    dup_fn = run([*zcc_cmd, "tests/rust/duplicate-function.rs"], cwd=repo, check=False)
    dup_fn_text = dup_fn.stdout + dup_fn.stderr
    if dup_fn.returncode == 0:
        print("duplicate-function should emit RUST-E0001")
        print(dup_fn_text)
        return 1
    if any(line not in dup_fn_text for line in expected_dup_fn.splitlines()):
        print("duplicate-function diagnostic mismatch")
        print(dup_fn_text)
        return 1
    if "RUST-TYPE-E9999" in dup_fn_text:
        print("duplicate-function should not reach typechecker")
        print(dup_fn_text)
        return 1

    print("[rust] duplicate parameter name")
    dup_param = run([*zcc_cmd, "tests/rust/duplicate-param.rs"], cwd=repo, check=False)
    dup_param_text = dup_param.stdout + dup_param.stderr
    if dup_param.returncode == 0:
        print("duplicate-param should emit RUST-E0001")
        print(dup_param_text)
        return 1
    if any(line not in dup_param_text for line in expected_dup_param.splitlines()):
        print("duplicate-param diagnostic mismatch")
        print(dup_param_text)
        return 1
    if "RUST-TYPE-E9999" in dup_param_text:
        print("duplicate-param should not reach typechecker")
        print(dup_param_text)
        return 1

    print("[rust] block shadowing")
    shadow = run([*zcc_cmd, "tests/rust/block-shadowing.rs"], cwd=repo, check=True)
    if "[OK] Rust frontend parsed successfully." not in shadow.stdout:
        print("block-shadowing should pass")
        print(shadow.stdout)
        return 1

    print("[rust] use before local declaration")
    use_before = run([*zcc_cmd, "tests/rust/use-before-local.rs"], cwd=repo, check=False)
    use_before_text = use_before.stdout + use_before.stderr
    if use_before.returncode == 0 or "error[RUST-E0002]" not in use_before_text:
        print("use-before-local should emit RUST-E0002")
        print(use_before_text)
        return 1

    print("[rust] branch local does not escape")
    branch_escape = run([*zcc_cmd, "tests/rust/branch-local-does-not-escape.rs"], cwd=repo, check=False)
    branch_escape_text = branch_escape.stdout + branch_escape.stderr
    if branch_escape.returncode == 0 or "error[RUST-E0002]" not in branch_escape_text:
        print("branch-local-does-not-escape should emit RUST-E0002")
        print(branch_escape_text)
        return 1

    print("[rust] immutable assignment resolver diagnostic")
    assign_imm = run([*zcc_cmd, "tests/rust/assign-immutable.rs"], cwd=repo, check=False)
    assign_imm_text = assign_imm.stdout + assign_imm.stderr
    if assign_imm.returncode == 0 or expected_assign_immutable not in assign_imm_text:
        print("assign-immutable should emit RUST-E0003")
        print(assign_imm_text)
        return 1

    print("[rust] symbol table smoke")
    symtab_smoke = run([*zcc_cmd, "tests/rust/symbol-table-smoke.rs", "--dump-rust-symbol-table"], cwd=repo, check=True)
    if symtab_smoke.stdout != expected_symtab_smoke:
        print("symbol table smoke mismatch")
        print("=== expected ===")
        print(expected_symtab_smoke)
        print("=== actual ===")
        print(symtab_smoke.stdout)
        return 1
    if symtab_smoke.stderr != "":
        print("expected empty stderr for --dump-rust-symbol-table success")
        print(symtab_smoke.stderr)
        return 1

    print("[rust] symbol table shadowing")
    symtab_shadow = run([*zcc_cmd, "tests/rust/symbol-table-shadowing.rs", "--dump-rust-symbol-table"], cwd=repo, check=True)
    actual_symtab_shadow = "\n".join(
        line for line in symtab_shadow.stdout.splitlines() if line.startswith("symbol ")
    ).strip() + "\n"
    if actual_symtab_shadow != expected_symtab_shadow:
        print("symbol table shadowing mismatch")
        print("=== expected ===")
        print(expected_symtab_shadow)
        print("=== actual ===")
        print(actual_symtab_shadow)
        return 1
    if symtab_shadow.stderr != "":
        print("expected empty stderr for symbol-table-shadowing success")
        print(symtab_shadow.stderr)
        return 1

    print("[rust] symbol table duplicate filtering")
    symtab_dup = run([*zcc_cmd, "tests/rust/symbol-table-duplicate.rs", "--dump-rust-symbol-table"], cwd=repo, check=False)
    if symtab_dup.returncode == 0:
        print("expected symbol-table-duplicate to fail")
        print(symtab_dup.stdout)
        return 1
    actual_symtab_dup = "\n".join(
        line for line in symtab_dup.stdout.splitlines() if line.startswith("symbol ")
    ).strip() + "\n"
    if actual_symtab_dup != expected_symtab_dup:
        print("symbol table duplicate mismatch")
        print("=== expected ===")
        print(expected_symtab_dup)
        print("=== actual ===")
        print(actual_symtab_dup)
        return 1
    actual_symtab_dup_stderr = symtab_dup.stderr.strip() + "\n"
    if actual_symtab_dup_stderr != expected_symtab_dup_stderr:
        print("symbol table duplicate stderr mismatch")
        print("=== expected ===")
        print(expected_symtab_dup_stderr)
        print("=== actual ===")
        print(actual_symtab_dup_stderr)
        return 1

    print("[rust] symbol table long-name truncation")
    symtab_long = run([*zcc_cmd, "tests/rust/symbol-table-long-name.rs", "--dump-rust-symbol-table"], cwd=repo, check=True)
    actual_symtab_long = "\n".join(
        line for line in symtab_long.stdout.splitlines() if line.startswith("symbol ")
    ).strip() + "\n"
    if actual_symtab_long != expected_symtab_long:
        print("symbol table long-name mismatch")
        print("=== expected ===")
        print(expected_symtab_long)
        print("=== actual ===")
        print(actual_symtab_long)
        return 1
    if symtab_long.stderr != "":
        print("expected empty stderr for symbol-table-long-name success")
        print(symtab_long.stderr)
        return 1

    print("[rust] typecheck param arithmetic ok")
    type_ok = run([*zcc_cmd, "tests/rust/type-param-arithmetic-ok.rs"], cwd=repo, check=True)
    if type_ok.stderr != "":
        print("expected empty stderr for type-param-arithmetic-ok")
        print(type_ok.stderr)
        return 1

    print("[rust] typecheck return bool mismatch")
    type_ret = run([*zcc_cmd, "tests/rust/type-return-bool.rs"], cwd=repo, check=False)
    if type_ret.returncode == 0:
        print("expected type-return-bool to fail")
        print(type_ret.stderr)
        return 1
    if expected_type_return_bool.splitlines()[0] not in type_ret.stderr or expected_type_return_bool.splitlines()[1] not in type_ret.stderr:
        print("type-return-bool diagnostic mismatch")
        print(type_ret.stderr)
        return 1

    print("[rust] typecheck if int condition")
    type_if = run([*zcc_cmd, "tests/rust/type-if-int-condition.rs"], cwd=repo, check=False)
    if type_if.returncode == 0:
        print("expected type-if-int-condition to fail")
        print(type_if.stderr)
        return 1
    if expected_type_if_int.splitlines()[0] not in type_if.stderr or expected_type_if_int.splitlines()[1] not in type_if.stderr:
        print("type-if-int-condition diagnostic mismatch")
        print(type_if.stderr)
        return 1

    print("[rust] typecheck while int condition")
    type_while = run([*zcc_cmd, "tests/rust/type-while-int-condition.rs"], cwd=repo, check=False)
    if type_while.returncode == 0:
        print("expected type-while-int-condition to fail")
        print(type_while.stderr)
        return 1
    if expected_type_while_int.splitlines()[0] not in type_while.stderr or expected_type_while_int.splitlines()[1] not in type_while.stderr:
        print("type-while-int-condition diagnostic mismatch")
        print(type_while.stderr)
        return 1

    print("[rust] typecheck bool arithmetic")
    type_bin = run([*zcc_cmd, "tests/rust/type-binary-bool.rs"], cwd=repo, check=False)
    if type_bin.returncode == 0:
        print("expected type-binary-bool to fail")
        print(type_bin.stderr)
        return 1
    if expected_type_binary_bool.splitlines()[0] not in type_bin.stderr or expected_type_binary_bool.splitlines()[1] not in type_bin.stderr:
        print("type-binary-bool diagnostic mismatch")
        print(type_bin.stderr)
        return 1

    print("[rust] typecheck missing return")
    type_missing = run([*zcc_cmd, "tests/rust/type-missing-return.rs"], cwd=repo, check=False)
    if type_missing.returncode == 0:
        print("expected type-missing-return to fail")
        print(type_missing.stderr)
        return 1
    if expected_type_missing_return.splitlines()[0] not in type_missing.stderr or expected_type_missing_return.splitlines()[1] not in type_missing.stderr:
        print("type-missing-return diagnostic mismatch")
        print(type_missing.stderr)
        return 1

    print("[rust] typecheck if missing else return")
    type_if_missing = run([*zcc_cmd, "tests/rust/type-if-missing-else-return.rs"], cwd=repo, check=False)
    if type_if_missing.returncode == 0:
        print("expected type-if-missing-else-return to fail")
        print(type_if_missing.stderr)
        return 1
    if expected_type_if_missing_else.splitlines()[0] not in type_if_missing.stderr or expected_type_if_missing_else.splitlines()[1] not in type_if_missing.stderr:
        print("type-if-missing-else-return diagnostic mismatch")
        print(type_if_missing.stderr)
        return 1

    print("[rust] typecheck if/else both return ok")
    type_if_else_ok = run([*zcc_cmd, "tests/rust/type-if-else-both-return-ok.rs"], cwd=repo, check=True)
    if type_if_else_ok.stderr != "":
        print("expected empty stderr for type-if-else-both-return-ok")
        print(type_if_else_ok.stderr)
        return 1

    print("[rust] typecheck while return not enough")
    type_while_ret = run([*zcc_cmd, "tests/rust/type-while-return-not-enough.rs"], cwd=repo, check=False)
    if type_while_ret.returncode == 0:
        print("expected type-while-return-not-enough to fail")
        print(type_while_ret.stderr)
        return 1
    if expected_type_while_return_not_enough.splitlines()[0] not in type_while_ret.stderr or expected_type_while_return_not_enough.splitlines()[1] not in type_while_ret.stderr:
        print("type-while-return-not-enough diagnostic mismatch")
        print(type_while_ret.stderr)
        return 1

    print("[rust] typecheck call ok")
    call_ok = run([*zcc_cmd, "tests/rust/type-call-ok.rs"], cwd=repo, check=True)
    if call_ok.stderr != "":
        print("expected empty stderr for type-call-ok")
        print(call_ok.stderr)
        return 1

    print("[rust] typecheck call forward ok")
    call_fwd_ok = run([*zcc_cmd, "tests/rust/type-call-forward-ok.rs"], cwd=repo, check=True)
    if call_fwd_ok.stderr != "":
        print("expected empty stderr for type-call-forward-ok")
        print(call_fwd_ok.stderr)
        return 1

    print("[rust] typecheck bool call signatures ok")
    call_bool_ok = run([*zcc_cmd, "tests/rust/type-call-bool-ok.rs"], cwd=repo, check=True)
    if call_bool_ok.stderr != "":
        print("expected empty stderr for type-call-bool-ok")
        print(call_bool_ok.stderr)
        return 1

    print("[rust] typecheck call local")
    call_local = run([*zcc_cmd, "tests/rust/type-call-local.rs"], cwd=repo, check=False)
    if call_local.returncode == 0:
        print("expected type-call-local to fail")
        print(call_local.stderr)
        return 1
    if expected_type_call_local.splitlines()[0] not in call_local.stderr or expected_type_call_local.splitlines()[1] not in call_local.stderr:
        print("type-call-local diagnostic mismatch")
        print(call_local.stderr)
        return 1

    print("[rust] typecheck call arity too few")
    call_few = run([*zcc_cmd, "tests/rust/type-call-arity-too-few.rs"], cwd=repo, check=False)
    if call_few.returncode == 0:
        print("expected type-call-arity-too-few to fail")
        print(call_few.stderr)
        return 1
    if expected_type_call_arity_few.splitlines()[0] not in call_few.stderr or expected_type_call_arity_few.splitlines()[1] not in call_few.stderr:
        print("type-call-arity-too-few diagnostic mismatch")
        print(call_few.stderr)
        return 1

    print("[rust] typecheck call arity too many")
    call_many = run([*zcc_cmd, "tests/rust/type-call-arity-too-many.rs"], cwd=repo, check=False)
    if call_many.returncode == 0:
        print("expected type-call-arity-too-many to fail")
        print(call_many.stderr)
        return 1
    if expected_type_call_arity_many.splitlines()[0] not in call_many.stderr or expected_type_call_arity_many.splitlines()[1] not in call_many.stderr:
        print("type-call-arity-too-many diagnostic mismatch")
        print(call_many.stderr)
        return 1

    print("[rust] typecheck call arg bool")
    call_arg_bool = run([*zcc_cmd, "tests/rust/type-call-arg-bool.rs"], cwd=repo, check=False)
    if call_arg_bool.returncode == 0:
        print("expected type-call-arg-bool to fail")
        print(call_arg_bool.stderr)
        return 1
    if expected_type_call_arg_bool.splitlines()[0] not in call_arg_bool.stderr or expected_type_call_arg_bool.splitlines()[1] not in call_arg_bool.stderr:
        print("type-call-arg-bool diagnostic mismatch")
        print(call_arg_bool.stderr)
        return 1

    print("[rust] typecheck call unknown resolver error")
    call_unknown = run([*zcc_cmd, "tests/rust/type-call-unknown.rs"], cwd=repo, check=False)
    if call_unknown.returncode == 0:
        print("expected type-call-unknown to fail")
        print(call_unknown.stderr)
        return 1
    if "error[RUST-E0002]" not in call_unknown.stderr:
        print("type-call-unknown should emit resolver unknown-name error")
        print(call_unknown.stderr)
        return 1
    if "RUST-TYPE-E0005" in call_unknown.stderr or "RUST-TYPE-E0006" in call_unknown.stderr or "RUST-TYPE-E0007" in call_unknown.stderr:
        print("type-call-unknown should not cascade into call type errors")
        print(call_unknown.stderr)
        return 1

    print("[rust] typecheck compare if ok")
    cmp_if_ok = run([*zcc_cmd, "tests/rust/type-compare-if-ok.rs"], cwd=repo, check=True)
    if cmp_if_ok.stderr != "":
        print("expected empty stderr for type-compare-if-ok")
        print(cmp_if_ok.stderr)
        return 1

    print("[rust] typecheck compare while ok")
    cmp_while_ok = run([*zcc_cmd, "tests/rust/type-compare-while-ok.rs"], cwd=repo, check=True)
    if cmp_while_ok.stderr != "":
        print("expected empty stderr for type-compare-while-ok")
        print(cmp_while_ok.stderr)
        return 1

    print("[rust] typecheck compare return bool")
    cmp_ret_bool = run([*zcc_cmd, "tests/rust/type-compare-return-bool.rs"], cwd=repo, check=False)
    if cmp_ret_bool.returncode == 0:
        print("expected type-compare-return-bool to fail")
        print(cmp_ret_bool.stderr)
        return 1
    if expected_type_compare_return_bool.splitlines()[0] not in cmp_ret_bool.stderr or expected_type_compare_return_bool.splitlines()[1] not in cmp_ret_bool.stderr:
        print("type-compare-return-bool diagnostic mismatch")
        print(cmp_ret_bool.stderr)
        return 1

    print("[rust] typecheck compare bool operands")
    cmp_bool = run([*zcc_cmd, "tests/rust/type-compare-bool.rs"], cwd=repo, check=False)
    if cmp_bool.returncode == 0:
        print("expected type-compare-bool to fail")
        print(cmp_bool.stderr)
        return 1
    if expected_type_compare_bool.splitlines()[0] not in cmp_bool.stderr or expected_type_compare_bool.splitlines()[1] not in cmp_bool.stderr:
        print("type-compare-bool diagnostic mismatch")
        print(cmp_bool.stderr)
        return 1

    print("[rust] typecheck compare chained")
    cmp_chain = run([*zcc_cmd, "tests/rust/type-compare-chained.rs"], cwd=repo, check=False)
    if cmp_chain.returncode == 0:
        print("expected type-compare-chained to fail")
        print(cmp_chain.stderr)
        return 1
    if expected_type_compare_chained.splitlines()[0] not in cmp_chain.stderr or expected_type_compare_chained.splitlines()[1] not in cmp_chain.stderr:
        print("type-compare-chained diagnostic mismatch")
        print(cmp_chain.stderr)
        return 1

    print("[rust] typecheck logic if ok")
    logic_if_ok = run([*zcc_cmd, "tests/rust/type-logic-if-ok.rs"], cwd=repo, check=True)
    if logic_if_ok.stderr != "":
        print("expected empty stderr for type-logic-if-ok")
        print(logic_if_ok.stderr)
        return 1

    print("[rust] typecheck logic not ok")
    logic_not_ok = run([*zcc_cmd, "tests/rust/type-logic-not-ok.rs"], cwd=repo, check=True)
    if logic_not_ok.stderr != "":
        print("expected empty stderr for type-logic-not-ok")
        print(logic_not_ok.stderr)
        return 1

    print("[rust] typecheck logic or ok")
    logic_or_ok = run([*zcc_cmd, "tests/rust/type-logic-or-ok.rs"], cwd=repo, check=True)
    if logic_or_ok.stderr != "":
        print("expected empty stderr for type-logic-or-ok")
        print(logic_or_ok.stderr)
        return 1

    print("[rust] typecheck logic not int")
    logic_not_int = run([*zcc_cmd, "tests/rust/type-logic-not-int.rs"], cwd=repo, check=False)
    if logic_not_int.returncode == 0:
        print("expected type-logic-not-int to fail")
        print(logic_not_int.stderr)
        return 1
    if expected_type_logic_not_int.splitlines()[0] not in logic_not_int.stderr or expected_type_logic_not_int.splitlines()[1] not in logic_not_int.stderr:
        print("type-logic-not-int diagnostic mismatch")
        print(logic_not_int.stderr)
        return 1

    print("[rust] typecheck logic and int")
    logic_and_int = run([*zcc_cmd, "tests/rust/type-logic-and-int.rs"], cwd=repo, check=False)
    if logic_and_int.returncode == 0:
        print("expected type-logic-and-int to fail")
        print(logic_and_int.stderr)
        return 1
    if expected_type_logic_and_int.splitlines()[0] not in logic_and_int.stderr or expected_type_logic_and_int.splitlines()[1] not in logic_and_int.stderr:
        print("type-logic-and-int diagnostic mismatch")
        print(logic_and_int.stderr)
        return 1

    print("[rust] typecheck logic precedence ok")
    logic_prec_ok = run([*zcc_cmd, "tests/rust/type-logic-precedence-ok.rs"], cwd=repo, check=True)
    if logic_prec_ok.stderr != "":
        print("expected empty stderr for type-logic-precedence-ok")
        print(logic_prec_ok.stderr)
        return 1

    print("[rust] typecheck let bool annotation ok")
    let_bool_ok = run([*zcc_cmd, "tests/rust/type-let-bool-annotation-ok.rs"], cwd=repo, check=True)
    if let_bool_ok.stderr != "":
        print("expected empty stderr for type-let-bool-annotation-ok")
        print(let_bool_ok.stderr)
        return 1

    print("[rust] typecheck let bool annotation mismatch")
    let_bool_bad = run([*zcc_cmd, "tests/rust/type-let-annot-bool-mismatch.rs"], cwd=repo, check=False)
    if let_bool_bad.returncode == 0:
        print("expected type-let-annot-bool-mismatch to fail")
        print(let_bool_bad.stderr)
        return 1
    if expected_type_let_bool_mismatch.splitlines()[0] not in let_bool_bad.stderr or expected_type_let_bool_mismatch.splitlines()[1] not in let_bool_bad.stderr:
        print("type-let-annot-bool-mismatch diagnostic mismatch")
        print(let_bool_bad.stderr)
        return 1

    print("[rust] strict let-annotation mode")
    strict_let_bad = run([*zcc_cmd, "tests/rust/strict-let-missing-annotation.rs", "--rust-strict-let-annotations"], cwd=repo, check=False)
    if strict_let_bad.returncode == 0:
        print("expected strict-let-missing-annotation to fail in strict mode")
        print(strict_let_bad.stderr)
        return 1
    if expected_strict_let_missing_annotation.splitlines()[0] not in strict_let_bad.stderr or expected_strict_let_missing_annotation.splitlines()[1] not in strict_let_bad.stderr:
        print("strict-let-missing-annotation diagnostic mismatch")
        print(strict_let_bad.stderr)
        return 1
    strict_let_bad_bool = run([*zcc_cmd, "tests/rust/strict-let-missing-annotation-bool.rs", "--rust-strict-let-annotations"], cwd=repo, check=False)
    if strict_let_bad_bool.returncode == 0 or "error[RUST-TYPE-E0014]" not in strict_let_bad_bool.stderr:
        print("expected strict-let-missing-annotation-bool to fail with RUST-TYPE-E0014")
        print(strict_let_bad_bool.stderr)
        return 1
    strict_let_bad_nested = run([*zcc_cmd, "tests/rust/strict-let-missing-annotation-nested.rs", "--rust-strict-let-annotations"], cwd=repo, check=False)
    if strict_let_bad_nested.returncode == 0 or "error[RUST-TYPE-E0014]" not in strict_let_bad_nested.stderr:
        print("expected strict-let-missing-annotation-nested to fail with RUST-TYPE-E0014")
        print(strict_let_bad_nested.stderr)
        return 1

    print("[rust] strict function-signature mode")
    strict_sig_bad = run([*zcc_cmd, "tests/rust/strict-signature-missing-return-type.rs", "--rust-strict-function-signatures"], cwd=repo, check=False)
    if strict_sig_bad.returncode == 0:
        print("expected strict-signature-missing-return-type to fail in strict signature mode")
        print(strict_sig_bad.stderr)
        return 1
    if expected_strict_signature_missing_ret.splitlines()[0] not in strict_sig_bad.stderr or expected_strict_signature_missing_ret.splitlines()[1] not in strict_sig_bad.stderr:
        print("strict-signature-missing-return-type diagnostic mismatch")
        print(strict_sig_bad.stderr)
        return 1
    strict_sig_main = run([*zcc_cmd, "tests/rust/strict-signature-missing-return-main.rs", "--rust-strict-function-signatures"], cwd=repo, check=False)
    if strict_sig_main.returncode == 0 or "error[RUST-TYPE-E0015]" not in strict_sig_main.stderr:
        print("expected strict-signature-missing-return-main to fail with RUST-TYPE-E0015")
        print(strict_sig_main.stderr)
        return 1
    strict_sig_bool = run([*zcc_cmd, "tests/rust/strict-signature-missing-return-bool.rs", "--rust-strict-function-signatures"], cwd=repo, check=False)
    if strict_sig_bool.returncode == 0 or "error[RUST-TYPE-E0015]" not in strict_sig_bool.stderr:
        print("expected strict-signature-missing-return-bool to fail with RUST-TYPE-E0015")
        print(strict_sig_bool.stderr)
        return 1

    print("[rust] strict combined mode")
    strict_combined_let = run([*zcc_cmd, "tests/rust/strict-let-missing-annotation.rs", "--rust-strict"], cwd=repo, check=False)
    if strict_combined_let.returncode == 0:
        print("expected strict-let-missing-annotation to fail in combined strict mode")
        print(strict_combined_let.stderr)
        return 1
    if expected_strict_let_missing_annotation.splitlines()[0] not in strict_combined_let.stderr:
        print("strict combined mode should include let-annotation diagnostic")
        print(strict_combined_let.stderr)
        return 1
    strict_combined_sig = run([*zcc_cmd, "tests/rust/strict-signature-missing-return-type.rs", "--rust-strict"], cwd=repo, check=False)
    if strict_combined_sig.returncode == 0:
        print("expected strict-signature-missing-return-type to fail in combined strict mode")
        print(strict_combined_sig.stderr)
        return 1
    if expected_strict_signature_missing_ret.splitlines()[0] not in strict_combined_sig.stderr:
        print("strict combined mode should include function-signature diagnostic")
        print(strict_combined_sig.stderr)
        return 1
    strict_combined_both = run([*zcc_cmd, "tests/rust/strict-combined-missing-both.rs", "--rust-strict"], cwd=repo, check=False)
    if strict_combined_both.returncode == 0:
        print("expected strict-combined-missing-both to fail in combined strict mode")
        print(strict_combined_both.stderr)
        return 1
    if "error[RUST-TYPE-E0014]" not in strict_combined_both.stderr or "error[RUST-TYPE-E0015]" not in strict_combined_both.stderr:
        print("strict-combined-missing-both should emit both strict diagnostics")
        print(strict_combined_both.stderr)
        return 1

    print("[rust] lower return int dump")
    lower_ret = run([*zcc_cmd, "tests/rust/lower-return-int.rs", "--dump-rust-ir"], cwd=repo, check=True)
    if lower_ret.stdout != expected_lower_return_int:
        print("lower-return-int dump mismatch")
        print("=== expected ===")
        print(expected_lower_return_int)
        print("=== actual ===")
        print(lower_ret.stdout)
        return 1
    if lower_ret.stderr != "":
        print("expected empty stderr for lower-return-int")
        print(lower_ret.stderr)
        return 1

    print("[rust] lower param add dump")
    lower_add = run([*zcc_cmd, "tests/rust/lower-param-add.rs", "--dump-rust-ir"], cwd=repo, check=True)
    if lower_add.stdout != expected_lower_param_add:
        print("lower-param-add dump mismatch")
        print("=== expected ===")
        print(expected_lower_param_add)
        print("=== actual ===")
        print(lower_add.stdout)
        return 1
    if lower_add.stderr != "":
        print("expected empty stderr for lower-param-add")
        print(lower_add.stderr)
        return 1

    print("[rust] lower if/else dump")
    lower_if_else = run([*zcc_cmd, "tests/rust/lower-if-else.rs", "--dump-rust-ir"], cwd=repo, check=True)
    if lower_if_else.stdout != expected_lower_if_else:
        print("lower-if-else dump mismatch")
        print("=== expected ===")
        print(expected_lower_if_else)
        print("=== actual ===")
        print(lower_if_else.stdout)
        return 1
    if lower_if_else.stderr != "":
        print("expected empty stderr for lower-if-else")
        print(lower_if_else.stderr)
        return 1

    print("[rust] lower compare eq dump")
    lower_cmp_eq = run([*zcc_cmd, "tests/rust/lower-compare-eq.rs", "--dump-rust-ir"], cwd=repo, check=True)
    if lower_cmp_eq.stdout != expected_lower_compare_eq:
        print("lower-compare-eq dump mismatch")
        print("=== expected ===")
        print(expected_lower_compare_eq)
        print("=== actual ===")
        print(lower_cmp_eq.stdout)
        return 1
    if lower_cmp_eq.stderr != "":
        print("expected empty stderr for lower-compare-eq")
        print(lower_cmp_eq.stderr)
        return 1

    print("[rust] lower nested if dump")
    lower_nested_if = run([*zcc_cmd, "tests/rust/lower-nested-if.rs", "--dump-rust-ir"], cwd=repo, check=True)
    if lower_nested_if.stdout != expected_lower_nested_if:
        print("lower-nested-if dump mismatch")
        print("=== expected ===")
        print(expected_lower_nested_if)
        print("=== actual ===")
        print(lower_nested_if.stdout)
        return 1
    if lower_nested_if.stderr != "":
        print("expected empty stderr for lower-nested-if")
        print(lower_nested_if.stderr)
        return 1

    print("[rust] lower while dump")
    lower_while = run([*zcc_cmd, "tests/rust/lower-while.rs", "--dump-rust-ir"], cwd=repo, check=True)
    if lower_while.stdout != expected_lower_while:
        print("lower-while dump mismatch")
        print("=== expected ===")
        print(expected_lower_while)
        print("=== actual ===")
        print(lower_while.stdout)
        return 1
    if lower_while.stderr != "":
        print("expected empty stderr for lower-while")
        print(lower_while.stderr)
        return 1

    print("[rust] lower while-if dump")
    lower_while_if = run([*zcc_cmd, "tests/rust/lower-while-if.rs", "--dump-rust-ir"], cwd=repo, check=True)
    if lower_while_if.stdout != expected_lower_while_if:
        print("lower-while-if dump mismatch")
        print("=== expected ===")
        print(expected_lower_while_if)
        print("=== actual ===")
        print(lower_while_if.stdout)
        return 1
    if lower_while_if.stderr != "":
        print("expected empty stderr for lower-while-if")
        print(lower_while_if.stderr)
        return 1

    print("[rust] lower call dump")
    lower_call = run([*zcc_cmd, "tests/rust/lower-call.rs", "--dump-rust-ir"], cwd=repo, check=True)
    if lower_call.stdout != expected_lower_call:
        print("lower-call dump mismatch")
        print("=== expected ===")
        print(expected_lower_call)
        print("=== actual ===")
        print(lower_call.stdout)
        return 1
    if lower_call.stderr != "":
        print("expected empty stderr for lower-call")
        print(lower_call.stderr)
        return 1

    print("[rust] lower forward call dump")
    lower_call_fwd = run([*zcc_cmd, "tests/rust/lower-call-forward.rs", "--dump-rust-ir"], cwd=repo, check=True)
    if lower_call_fwd.stdout != expected_lower_call_forward:
        print("lower-call-forward dump mismatch")
        print("=== expected ===")
        print(expected_lower_call_forward)
        print("=== actual ===")
        print(lower_call_fwd.stdout)
        return 1
    if lower_call_fwd.stderr != "":
        print("expected empty stderr for lower-call-forward")
        print(lower_call_fwd.stderr)
        return 1

    print("[rust] lower logic dump")
    lower_logic = run([*zcc_cmd, "tests/rust/lower-logic.rs", "--dump-rust-ir"], cwd=repo, check=True)
    if lower_logic.stdout != expected_lower_logic:
        print("lower-logic dump mismatch")
        print("=== expected ===")
        print(expected_lower_logic)
        print("=== actual ===")
        print(lower_logic.stdout)
        return 1
    if lower_logic.stderr != "":
        print("expected empty stderr for lower-logic")
        print(lower_logic.stderr)
        return 1

    print("[rust] backend bridge return-42 run")
    backend_bin = repo / "tests/rust/out-return-42.bin"
    backend_build = run(
        [*zcc_cmd, "tests/rust/run-return-42.rs", "--rust-backend-v1", "-o", str(backend_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_build.stdout:
        print("expected backend bridge success marker")
        print(backend_build.stdout)
        return 1
    if backend_build.stderr != "":
        print("expected empty stderr for backend bridge return-42 compile")
        print(backend_build.stderr)
        return 1
    backend_run = run([str(backend_bin)], cwd=repo, check=False)
    if backend_run.returncode != 42:
        print("expected backend bridge binary to exit with 42")
        print(f"actual exit code: {backend_run.returncode}")
        return 1

    print("[rust] backend bridge return-add run")
    backend_add_bin = repo / "tests/rust/out-return-add.bin"
    backend_add_build = run(
        [*zcc_cmd, "tests/rust/run-return-add.rs", "--rust-backend-v1", "-o", str(backend_add_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_add_build.stdout or backend_add_build.stderr != "":
        print("expected backend bridge success for return-add")
        print(backend_add_build.stdout)
        print(backend_add_build.stderr)
        return 1
    backend_add_run = run([str(backend_add_bin)], cwd=repo, check=False)
    if backend_add_run.returncode != 42:
        print("expected backend bridge add binary to exit with 42")
        print(f"actual exit code: {backend_add_run.returncode}")
        return 1

    print("[rust] backend bridge arithmetic precedence run")
    backend_prec_bin = repo / "tests/rust/out-return-arith-precedence.bin"
    backend_prec_build = run(
        [*zcc_cmd, "tests/rust/run-return-arith-precedence.rs", "--rust-backend-v1", "-o", str(backend_prec_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_prec_build.stdout or backend_prec_build.stderr != "":
        print("expected backend bridge success for return-arith-precedence")
        print(backend_prec_build.stdout)
        print(backend_prec_build.stderr)
        return 1
    backend_prec_run = run([str(backend_prec_bin)], cwd=repo, check=False)
    if backend_prec_run.returncode != 42:
        print("expected backend bridge precedence binary to exit with 42")
        print(f"actual exit code: {backend_prec_run.returncode}")
        return 1

    print("[rust] backend bridge arithmetic div run")
    backend_div_bin = repo / "tests/rust/out-return-arith-div.bin"
    backend_div_build = run(
        [*zcc_cmd, "tests/rust/run-return-arith-div.rs", "--rust-backend-v1", "-o", str(backend_div_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_div_build.stdout or backend_div_build.stderr != "":
        print("expected backend bridge success for return-arith-div")
        print(backend_div_build.stdout)
        print(backend_div_build.stderr)
        return 1
    backend_div_run = run([str(backend_div_bin)], cwd=repo, check=False)
    if backend_div_run.returncode != 42:
        print("expected backend bridge div binary to exit with 42")
        print(f"actual exit code: {backend_div_run.returncode}")
        return 1

    print("[rust] backend bridge return-local run")
    backend_local_bin = repo / "tests/rust/out-return-local.bin"
    backend_local_build = run(
        [*zcc_cmd, "tests/rust/run-return-local.rs", "--rust-backend-v1", "-o", str(backend_local_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_local_build.stdout or backend_local_build.stderr != "":
        print("expected backend bridge success for return-local")
        print(backend_local_build.stdout)
        print(backend_local_build.stderr)
        return 1
    backend_local_run = run([str(backend_local_bin)], cwd=repo, check=False)
    if backend_local_run.returncode != 42:
        print("expected backend bridge local binary to exit with 42")
        print(f"actual exit code: {backend_local_run.returncode}")
        return 1

    print("[rust] backend bridge return-local-add run")
    backend_local_add_bin = repo / "tests/rust/out-return-local-add.bin"
    backend_local_add_build = run(
        [*zcc_cmd, "tests/rust/run-return-local-add.rs", "--rust-backend-v1", "-o", str(backend_local_add_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_local_add_build.stdout or backend_local_add_build.stderr != "":
        print("expected backend bridge success for return-local-add")
        print(backend_local_add_build.stdout)
        print(backend_local_add_build.stderr)
        return 1
    backend_local_add_run = run([str(backend_local_add_bin)], cwd=repo, check=False)
    if backend_local_add_run.returncode != 42:
        print("expected backend bridge local-add binary to exit with 42")
        print(f"actual exit code: {backend_local_add_run.returncode}")
        return 1

    print("[rust] backend bridge return-local-chain run")
    backend_local_chain_bin = repo / "tests/rust/out-return-local-chain.bin"
    backend_local_chain_build = run(
        [*zcc_cmd, "tests/rust/run-return-local-chain.rs", "--rust-backend-v1", "-o", str(backend_local_chain_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_local_chain_build.stdout or backend_local_chain_build.stderr != "":
        print("expected backend bridge success for return-local-chain")
        print(backend_local_chain_build.stdout)
        print(backend_local_chain_build.stderr)
        return 1
    backend_local_chain_run = run([str(backend_local_chain_bin)], cwd=repo, check=False)
    if backend_local_chain_run.returncode != 42:
        print("expected backend bridge local-chain binary to exit with 42")
        print(f"actual exit code: {backend_local_chain_run.returncode}")
        return 1

    print("[rust] backend bridge return if/else true run")
    backend_if_true_bin = repo / "tests/rust/out-return-if-else.bin"
    backend_if_true_build = run(
        [*zcc_cmd, "tests/rust/run-return-if-else.rs", "--rust-backend-v1", "-o", str(backend_if_true_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_if_true_build.stdout or backend_if_true_build.stderr != "":
        print("expected backend bridge success for run-return-if-else")
        print(backend_if_true_build.stdout)
        print(backend_if_true_build.stderr)
        return 1
    backend_if_true_run = run([str(backend_if_true_bin)], cwd=repo, check=False)
    if backend_if_true_run.returncode != 42:
        print("expected backend if/else true binary to exit with 42")
        print(f"actual exit code: {backend_if_true_run.returncode}")
        return 1

    print("[rust] backend bridge return if/else false run")
    backend_if_false_bin = repo / "tests/rust/out-return-if-else-false.bin"
    backend_if_false_build = run(
        [*zcc_cmd, "tests/rust/run-return-if-else-false.rs", "--rust-backend-v1", "-o", str(backend_if_false_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_if_false_build.stdout or backend_if_false_build.stderr != "":
        print("expected backend bridge success for run-return-if-else-false")
        print(backend_if_false_build.stdout)
        print(backend_if_false_build.stderr)
        return 1
    backend_if_false_run = run([str(backend_if_false_bin)], cwd=repo, check=False)
    if backend_if_false_run.returncode != 42:
        print("expected backend if/else false binary to exit with 42")
        print(f"actual exit code: {backend_if_false_run.returncode}")
        return 1

    print("[rust] backend bridge return if let-cond run")
    backend_if_let_bin = repo / "tests/rust/out-return-if-let-cond.bin"
    backend_if_let_build = run(
        [*zcc_cmd, "tests/rust/run-return-if-let-cond.rs", "--rust-backend-v1", "-o", str(backend_if_let_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_if_let_build.stdout or backend_if_let_build.stderr != "":
        print("expected backend bridge success for run-return-if-let-cond")
        print(backend_if_let_build.stdout)
        print(backend_if_let_build.stderr)
        return 1
    backend_if_let_run = run([str(backend_if_let_bin)], cwd=repo, check=False)
    if backend_if_let_run.returncode != 42:
        print("expected backend if-let-cond binary to exit with 42")
        print(f"actual exit code: {backend_if_let_run.returncode}")
        return 1

    print("[rust] backend bridge return if logic run")
    backend_if_logic_bin = repo / "tests/rust/out-return-if-logic.bin"
    backend_if_logic_build = run(
        [*zcc_cmd, "tests/rust/run-return-if-logic.rs", "--rust-backend-v1", "-o", str(backend_if_logic_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_if_logic_build.stdout or backend_if_logic_build.stderr != "":
        print("expected backend bridge success for run-return-if-logic")
        print(backend_if_logic_build.stdout)
        print(backend_if_logic_build.stderr)
        return 1
    backend_if_logic_run = run([str(backend_if_logic_bin)], cwd=repo, check=False)
    if backend_if_logic_run.returncode != 42:
        print("expected backend if-logic binary to exit with 42")
        print(f"actual exit code: {backend_if_logic_run.returncode}")
        return 1

    print("[rust] backend bridge return if nested run")
    backend_if_nested_bin = repo / "tests/rust/out-return-if-nested.bin"
    backend_if_nested_build = run(
        [*zcc_cmd, "tests/rust/run-return-if-nested.rs", "--rust-backend-v1", "-o", str(backend_if_nested_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_if_nested_build.stdout or backend_if_nested_build.stderr != "":
        print("expected backend bridge success for run-return-if-nested")
        print(backend_if_nested_build.stdout)
        print(backend_if_nested_build.stderr)
        return 1
    backend_if_nested_run = run([str(backend_if_nested_bin)], cwd=repo, check=False)
    if backend_if_nested_run.returncode != 42:
        print("expected backend if-nested binary to exit with 42")
        print(f"actual exit code: {backend_if_nested_run.returncode}")
        return 1

    print("[rust] backend bridge return call run")
    backend_call_bin = repo / "tests/rust/out-return-call.bin"
    backend_call_build = run(
        [*zcc_cmd, "tests/rust/run-return-call.rs", "--rust-backend-v1", "-o", str(backend_call_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_call_build.stdout or backend_call_build.stderr != "":
        print("expected backend bridge success for run-return-call")
        print(backend_call_build.stdout)
        print(backend_call_build.stderr)
        return 1
    backend_call_run = run([str(backend_call_bin)], cwd=repo, check=False)
    if backend_call_run.returncode != 42:
        print("expected backend call binary to exit with 42")
        print(f"actual exit code: {backend_call_run.returncode}")
        return 1

    print("[rust] backend bridge return call forward run")
    backend_call_fwd_bin = repo / "tests/rust/out-return-call-forward.bin"
    backend_call_fwd_build = run(
        [*zcc_cmd, "tests/rust/run-return-call-forward.rs", "--rust-backend-v1", "-o", str(backend_call_fwd_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_call_fwd_build.stdout or backend_call_fwd_build.stderr != "":
        print("expected backend bridge success for run-return-call-forward")
        print(backend_call_fwd_build.stdout)
        print(backend_call_fwd_build.stderr)
        return 1
    backend_call_fwd_run = run([str(backend_call_fwd_bin)], cwd=repo, check=False)
    if backend_call_fwd_run.returncode != 42:
        print("expected backend call-forward binary to exit with 42")
        print(f"actual exit code: {backend_call_fwd_run.returncode}")
        return 1

    print("[rust] backend bridge return call arithmetic run")
    backend_call_arith_bin = repo / "tests/rust/out-return-call-arith.bin"
    backend_call_arith_build = run(
        [*zcc_cmd, "tests/rust/run-return-call-arith.rs", "--rust-backend-v1", "-o", str(backend_call_arith_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_call_arith_build.stdout or backend_call_arith_build.stderr != "":
        print("expected backend bridge success for run-return-call-arith")
        print(backend_call_arith_build.stdout)
        print(backend_call_arith_build.stderr)
        return 1
    backend_call_arith_run = run([str(backend_call_arith_bin)], cwd=repo, check=False)
    if backend_call_arith_run.returncode != 42:
        print("expected backend call-arith binary to exit with 42")
        print(f"actual exit code: {backend_call_arith_run.returncode}")
        return 1

    print("[rust] backend bridge return call local run")
    backend_call_local_bin = repo / "tests/rust/out-return-call-local.bin"
    backend_call_local_build = run(
        [*zcc_cmd, "tests/rust/run-return-call-local.rs", "--rust-backend-v1", "-o", str(backend_call_local_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_call_local_build.stdout or backend_call_local_build.stderr != "":
        print("expected backend bridge success for run-return-call-local")
        print(backend_call_local_build.stdout)
        print(backend_call_local_build.stderr)
        return 1
    backend_call_local_run = run([str(backend_call_local_bin)], cwd=repo, check=False)
    if backend_call_local_run.returncode != 42:
        print("expected backend call-local binary to exit with 42")
        print(f"actual exit code: {backend_call_local_run.returncode}")
        return 1

    print("[rust] backend bridge return call if run")
    backend_call_if_bin = repo / "tests/rust/out-return-call-if.bin"
    backend_call_if_build = run(
        [*zcc_cmd, "tests/rust/run-return-call-if.rs", "--rust-backend-v1", "-o", str(backend_call_if_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_call_if_build.stdout or backend_call_if_build.stderr != "":
        print("expected backend bridge success for run-return-call-if")
        print(backend_call_if_build.stdout)
        print(backend_call_if_build.stderr)
        return 1
    backend_call_if_run = run([str(backend_call_if_bin)], cwd=repo, check=False)
    if backend_call_if_run.returncode != 42:
        print("expected backend call-if binary to exit with 42")
        print(f"actual exit code: {backend_call_if_run.returncode}")
        return 1

    print("[rust] backend bridge return call chain run")
    backend_call_chain_bin = repo / "tests/rust/out-return-call-chain.bin"
    backend_call_chain_build = run(
        [*zcc_cmd, "tests/rust/run-return-call-chain.rs", "--rust-backend-v1", "-o", str(backend_call_chain_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_call_chain_build.stdout or backend_call_chain_build.stderr != "":
        print("expected backend bridge success for run-return-call-chain")
        print(backend_call_chain_build.stdout)
        print(backend_call_chain_build.stderr)
        return 1
    backend_call_chain_run = run([str(backend_call_chain_bin)], cwd=repo, check=False)
    if backend_call_chain_run.returncode != 42:
        print("expected backend call-chain binary to exit with 42")
        print(f"actual exit code: {backend_call_chain_run.returncode}")
        return 1

    print("[rust] backend bridge return call param run")
    backend_call_param_bin = repo / "tests/rust/out-return-call-param.bin"
    backend_call_param_build = run(
        [*zcc_cmd, "tests/rust/run-return-call-param.rs", "--rust-backend-v1", "-o", str(backend_call_param_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_call_param_build.stdout or backend_call_param_build.stderr != "":
        print("expected backend bridge success for run-return-call-param")
        print(backend_call_param_build.stdout)
        print(backend_call_param_build.stderr)
        return 1
    backend_call_param_run = run([str(backend_call_param_bin)], cwd=repo, check=False)
    if backend_call_param_run.returncode != 42:
        print("expected backend call-param binary to exit with 42")
        print(f"actual exit code: {backend_call_param_run.returncode}")
        return 1

    print("[rust] backend bridge return call two params run")
    backend_call_two_bin = repo / "tests/rust/out-return-call-two-params.bin"
    backend_call_two_build = run(
        [*zcc_cmd, "tests/rust/run-return-call-two-params.rs", "--rust-backend-v1", "-o", str(backend_call_two_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_call_two_build.stdout or backend_call_two_build.stderr != "":
        print("expected backend bridge success for run-return-call-two-params")
        print(backend_call_two_build.stdout)
        print(backend_call_two_build.stderr)
        return 1
    backend_call_two_run = run([str(backend_call_two_bin)], cwd=repo, check=False)
    if backend_call_two_run.returncode != 42:
        print("expected backend call-two-params binary to exit with 42")
        print(f"actual exit code: {backend_call_two_run.returncode}")
        return 1

    print("[rust] backend bridge return call param expr run")
    backend_call_param_expr_bin = repo / "tests/rust/out-return-call-param-expr.bin"
    backend_call_param_expr_build = run(
        [*zcc_cmd, "tests/rust/run-return-call-param-expr.rs", "--rust-backend-v1", "-o", str(backend_call_param_expr_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_call_param_expr_build.stdout or backend_call_param_expr_build.stderr != "":
        print("expected backend bridge success for run-return-call-param-expr")
        print(backend_call_param_expr_build.stdout)
        print(backend_call_param_expr_build.stderr)
        return 1
    backend_call_param_expr_run = run([str(backend_call_param_expr_bin)], cwd=repo, check=False)
    if backend_call_param_expr_run.returncode != 42:
        print("expected backend call-param-expr binary to exit with 42")
        print(f"actual exit code: {backend_call_param_expr_run.returncode}")
        return 1

    print("[rust] backend bridge return call param forward run")
    backend_call_param_fwd_bin = repo / "tests/rust/out-return-call-param-forward.bin"
    backend_call_param_fwd_build = run(
        [*zcc_cmd, "tests/rust/run-return-call-param-forward.rs", "--rust-backend-v1", "-o", str(backend_call_param_fwd_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_call_param_fwd_build.stdout or backend_call_param_fwd_build.stderr != "":
        print("expected backend bridge success for run-return-call-param-forward")
        print(backend_call_param_fwd_build.stdout)
        print(backend_call_param_fwd_build.stderr)
        return 1
    backend_call_param_fwd_run = run([str(backend_call_param_fwd_bin)], cwd=repo, check=False)
    if backend_call_param_fwd_run.returncode != 42:
        print("expected backend call-param-forward binary to exit with 42")
        print(f"actual exit code: {backend_call_param_fwd_run.returncode}")
        return 1

    print("[rust] backend bridge return call param nested run")
    backend_call_param_nested_bin = repo / "tests/rust/out-return-call-param-nested.bin"
    backend_call_param_nested_build = run(
        [*zcc_cmd, "tests/rust/run-return-call-param-nested.rs", "--rust-backend-v1", "-o", str(backend_call_param_nested_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_call_param_nested_build.stdout or backend_call_param_nested_build.stderr != "":
        print("expected backend bridge success for run-return-call-param-nested")
        print(backend_call_param_nested_build.stdout)
        print(backend_call_param_nested_build.stderr)
        return 1
    backend_call_param_nested_run = run([str(backend_call_param_nested_bin)], cwd=repo, check=False)
    if backend_call_param_nested_run.returncode != 42:
        print("expected backend call-param-nested binary to exit with 42")
        print(f"actual exit code: {backend_call_param_nested_run.returncode}")
        return 1

    print("[rust] backend bridge return call param if run")
    backend_call_param_if_bin = repo / "tests/rust/out-return-call-param-if.bin"
    backend_call_param_if_build = run(
        [*zcc_cmd, "tests/rust/run-return-call-param-if.rs", "--rust-backend-v1", "-o", str(backend_call_param_if_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_call_param_if_build.stdout or backend_call_param_if_build.stderr != "":
        print("expected backend bridge success for run-return-call-param-if")
        print(backend_call_param_if_build.stdout)
        print(backend_call_param_if_build.stderr)
        return 1
    backend_call_param_if_run = run([str(backend_call_param_if_bin)], cwd=repo, check=False)
    if backend_call_param_if_run.returncode != 42:
        print("expected backend call-param-if binary to exit with 42")
        print(f"actual exit code: {backend_call_param_if_run.returncode}")
        return 1

    print("[rust] backend bridge runtime return local run")
    backend_runtime_local_bin = repo / "tests/rust/out-runtime-return-local.bin"
    backend_runtime_local_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-return-local.rs", "--rust-backend-v1", "-o", str(backend_runtime_local_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_runtime_local_build.stdout or backend_runtime_local_build.stderr != "":
        print("expected backend bridge success for run-runtime-return-local")
        print(backend_runtime_local_build.stdout)
        print(backend_runtime_local_build.stderr)
        return 1
    backend_runtime_local_run = run([str(backend_runtime_local_bin)], cwd=repo, check=False)
    if backend_runtime_local_run.returncode != 42:
        print("expected runtime-return-local binary to exit with 42")
        print(f"actual exit code: {backend_runtime_local_run.returncode}")
        return 1

    print("[rust] backend bridge runtime local add run")
    backend_runtime_add_bin = repo / "tests/rust/out-runtime-local-add.bin"
    backend_runtime_add_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-local-add.rs", "--rust-backend-v1", "-o", str(backend_runtime_add_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_runtime_add_build.stdout or backend_runtime_add_build.stderr != "":
        print("expected backend bridge success for run-runtime-local-add")
        print(backend_runtime_add_build.stdout)
        print(backend_runtime_add_build.stderr)
        return 1
    backend_runtime_add_run = run([str(backend_runtime_add_bin)], cwd=repo, check=False)
    if backend_runtime_add_run.returncode != 42:
        print("expected runtime-local-add binary to exit with 42")
        print(f"actual exit code: {backend_runtime_add_run.returncode}")
        return 1

    print("[rust] backend bridge runtime arithmetic nested run")
    backend_runtime_nested_bin = repo / "tests/rust/out-runtime-arith-nested.bin"
    backend_runtime_nested_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-arith-nested.rs", "--rust-backend-v1", "-o", str(backend_runtime_nested_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_runtime_nested_build.stdout or backend_runtime_nested_build.stderr != "":
        print("expected backend bridge success for run-runtime-arith-nested")
        print(backend_runtime_nested_build.stdout)
        print(backend_runtime_nested_build.stderr)
        return 1
    backend_runtime_nested_run = run([str(backend_runtime_nested_bin)], cwd=repo, check=False)
    if backend_runtime_nested_run.returncode != 42:
        print("expected runtime-arith-nested binary to exit with 42")
        print(f"actual exit code: {backend_runtime_nested_run.returncode}")
        return 1

    print("[rust] backend bridge runtime subtraction run")
    backend_runtime_sub_bin = repo / "tests/rust/out-runtime-sub-div.bin"
    backend_runtime_sub_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-sub-div.rs", "--rust-backend-v1", "-o", str(backend_runtime_sub_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_runtime_sub_build.stdout or backend_runtime_sub_build.stderr != "":
        print("expected backend bridge success for run-runtime-sub-div")
        print(backend_runtime_sub_build.stdout)
        print(backend_runtime_sub_build.stderr)
        return 1
    backend_runtime_sub_run = run([str(backend_runtime_sub_bin)], cwd=repo, check=False)
    if backend_runtime_sub_run.returncode != 42:
        print("expected runtime-sub-div binary to exit with 42")
        print(f"actual exit code: {backend_runtime_sub_run.returncode}")
        return 1

    print("[rust] backend bridge runtime division run")
    backend_runtime_div_bin = repo / "tests/rust/out-runtime-div.bin"
    backend_runtime_div_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-div.rs", "--rust-backend-v1", "-o", str(backend_runtime_div_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_runtime_div_build.stdout or backend_runtime_div_build.stderr != "":
        print("expected backend bridge success for run-runtime-div")
        print(backend_runtime_div_build.stdout)
        print(backend_runtime_div_build.stderr)
        return 1
    backend_runtime_div_run = run([str(backend_runtime_div_bin)], cwd=repo, check=False)
    if backend_runtime_div_run.returncode != 42:
        print("expected runtime-div binary to exit with 42")
        print(f"actual exit code: {backend_runtime_div_run.returncode}")
        return 1

    print("[rust] backend bridge runtime if/else run")
    backend_runtime_if_bin = repo / "tests/rust/out-runtime-if-else.bin"
    backend_runtime_if_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-if-else.rs", "--rust-backend-v1", "-o", str(backend_runtime_if_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_runtime_if_build.stdout or backend_runtime_if_build.stderr != "":
        print("expected backend bridge success for run-runtime-if-else")
        print(backend_runtime_if_build.stdout)
        print(backend_runtime_if_build.stderr)
        return 1
    backend_runtime_if_run = run([str(backend_runtime_if_bin)], cwd=repo, check=False)
    if backend_runtime_if_run.returncode != 42:
        print("expected runtime-if-else binary to exit with 42")
        print(f"actual exit code: {backend_runtime_if_run.returncode}")
        return 1

    print("[rust] backend bridge runtime if/else false run")
    backend_runtime_if_false_bin = repo / "tests/rust/out-runtime-if-else-false.bin"
    backend_runtime_if_false_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-if-else-false.rs", "--rust-backend-v1", "-o", str(backend_runtime_if_false_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_runtime_if_false_build.stdout or backend_runtime_if_false_build.stderr != "":
        print("expected backend bridge success for run-runtime-if-else-false")
        print(backend_runtime_if_false_build.stdout)
        print(backend_runtime_if_false_build.stderr)
        return 1
    backend_runtime_if_false_run = run([str(backend_runtime_if_false_bin)], cwd=repo, check=False)
    if backend_runtime_if_false_run.returncode != 42:
        print("expected runtime-if-else-false binary to exit with 42")
        print(f"actual exit code: {backend_runtime_if_false_run.returncode}")
        return 1

    print("[rust] backend bridge runtime if-lt run")
    backend_runtime_if_lt_bin = repo / "tests/rust/out-runtime-if-lt.bin"
    backend_runtime_if_lt_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-if-lt.rs", "--rust-backend-v1", "-o", str(backend_runtime_if_lt_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_runtime_if_lt_build.stdout or backend_runtime_if_lt_build.stderr != "":
        print("expected backend bridge success for run-runtime-if-lt")
        print(backend_runtime_if_lt_build.stdout)
        print(backend_runtime_if_lt_build.stderr)
        return 1
    backend_runtime_if_lt_run = run([str(backend_runtime_if_lt_bin)], cwd=repo, check=False)
    if backend_runtime_if_lt_run.returncode != 42:
        print("expected runtime-if-lt binary to exit with 42")
        print(f"actual exit code: {backend_runtime_if_lt_run.returncode}")
        return 1

    print("[rust] backend bridge runtime if bool local run")
    backend_runtime_if_bool_bin = repo / "tests/rust/out-runtime-if-bool-local.bin"
    backend_runtime_if_bool_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-if-bool-local.rs", "--rust-backend-v1", "-o", str(backend_runtime_if_bool_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_runtime_if_bool_build.stdout or backend_runtime_if_bool_build.stderr != "":
        print("expected backend bridge success for run-runtime-if-bool-local")
        print(backend_runtime_if_bool_build.stdout)
        print(backend_runtime_if_bool_build.stderr)
        return 1
    backend_runtime_if_bool_run = run([str(backend_runtime_if_bool_bin)], cwd=repo, check=False)
    if backend_runtime_if_bool_run.returncode != 42:
        print("expected runtime-if-bool-local binary to exit with 42")
        print(f"actual exit code: {backend_runtime_if_bool_run.returncode}")
        return 1

    print("[rust] backend bridge runtime if nested run")
    backend_runtime_if_nested_bin = repo / "tests/rust/out-runtime-if-nested.bin"
    backend_runtime_if_nested_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-if-nested.rs", "--rust-backend-v1", "-o", str(backend_runtime_if_nested_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_runtime_if_nested_build.stdout or backend_runtime_if_nested_build.stderr != "":
        print("expected backend bridge success for run-runtime-if-nested")
        print(backend_runtime_if_nested_build.stdout)
        print(backend_runtime_if_nested_build.stderr)
        return 1
    backend_runtime_if_nested_run = run([str(backend_runtime_if_nested_bin)], cwd=repo, check=False)
    if backend_runtime_if_nested_run.returncode != 42:
        print("expected runtime-if-nested binary to exit with 42")
        print(f"actual exit code: {backend_runtime_if_nested_run.returncode}")
        return 1

    print("[rust] backend bridge runtime while run")
    backend_runtime_while_bin = repo / "tests/rust/out-runtime-while.bin"
    backend_runtime_while_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-while.rs", "--rust-backend-v1", "-o", str(backend_runtime_while_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_runtime_while_build.stdout or backend_runtime_while_build.stderr != "":
        print("expected backend bridge success for run-runtime-while")
        print(backend_runtime_while_build.stdout)
        print(backend_runtime_while_build.stderr)
        return 1
    backend_runtime_while_run = run([str(backend_runtime_while_bin)], cwd=repo, check=False)
    if backend_runtime_while_run.returncode != 42:
        print("expected runtime-while binary to exit with 42")
        print(f"actual exit code: {backend_runtime_while_run.returncode}")
        return 1

    print("[rust] backend bridge runtime while false run")
    backend_runtime_while_false_bin = repo / "tests/rust/out-runtime-while-false.bin"
    backend_runtime_while_false_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-while-false.rs", "--rust-backend-v1", "-o", str(backend_runtime_while_false_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_runtime_while_false_build.stdout or backend_runtime_while_false_build.stderr != "":
        print("expected backend bridge success for run-runtime-while-false")
        print(backend_runtime_while_false_build.stdout)
        print(backend_runtime_while_false_build.stderr)
        return 1
    backend_runtime_while_false_run = run([str(backend_runtime_while_false_bin)], cwd=repo, check=False)
    if backend_runtime_while_false_run.returncode != 42:
        print("expected runtime-while-false binary to exit with 42")
        print(f"actual exit code: {backend_runtime_while_false_run.returncode}")
        return 1

    print("[rust] backend bridge runtime while logic run")
    backend_runtime_while_logic_bin = repo / "tests/rust/out-runtime-while-logic.bin"
    backend_runtime_while_logic_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-while-logic.rs", "--rust-backend-v1", "-o", str(backend_runtime_while_logic_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_runtime_while_logic_build.stdout or backend_runtime_while_logic_build.stderr != "":
        print("expected backend bridge success for run-runtime-while-logic")
        print(backend_runtime_while_logic_build.stdout)
        print(backend_runtime_while_logic_build.stderr)
        return 1
    backend_runtime_while_logic_run = run([str(backend_runtime_while_logic_bin)], cwd=repo, check=False)
    if backend_runtime_while_logic_run.returncode != 42:
        print("expected runtime-while-logic binary to exit with 42")
        print(f"actual exit code: {backend_runtime_while_logic_run.returncode}")
        return 1

    print("[rust] backend bridge runtime while-if run")
    backend_runtime_while_if_bin = repo / "tests/rust/out-runtime-while-if.bin"
    backend_runtime_while_if_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-while-if.rs", "--rust-backend-v1", "-o", str(backend_runtime_while_if_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_runtime_while_if_build.stdout or backend_runtime_while_if_build.stderr != "":
        print("expected backend bridge success for run-runtime-while-if")
        print(backend_runtime_while_if_build.stdout)
        print(backend_runtime_while_if_build.stderr)
        return 1
    backend_runtime_while_if_run = run([str(backend_runtime_while_if_bin)], cwd=repo, check=False)
    if backend_runtime_while_if_run.returncode != 42:
        print("expected runtime-while-if binary to exit with 42")
        print(f"actual exit code: {backend_runtime_while_if_run.returncode}")
        return 1

    print("[rust] backend bridge runtime while nested run")
    backend_runtime_while_nested_bin = repo / "tests/rust/out-runtime-while-nested.bin"
    backend_runtime_while_nested_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-while-nested.rs", "--rust-backend-v1", "-o", str(backend_runtime_while_nested_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_runtime_while_nested_build.stdout or backend_runtime_while_nested_build.stderr != "":
        print("expected backend bridge success for run-runtime-while-nested")
        print(backend_runtime_while_nested_build.stdout)
        print(backend_runtime_while_nested_build.stderr)
        return 1
    backend_runtime_while_nested_run = run([str(backend_runtime_while_nested_bin)], cwd=repo, check=False)
    if backend_runtime_while_nested_run.returncode != 42:
        print("expected runtime-while-nested binary to exit with 42")
        print(f"actual exit code: {backend_runtime_while_nested_run.returncode}")
        return 1

    print("[rust] backend bridge runtime let in if run")
    backend_runtime_let_if_bin = repo / "tests/rust/out-runtime-let-in-if.bin"
    backend_runtime_let_if_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-let-in-if.rs", "--rust-backend-v1", "-o", str(backend_runtime_let_if_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_runtime_let_if_build.stdout or backend_runtime_let_if_build.stderr != "":
        print("expected backend bridge success for run-runtime-let-in-if")
        print(backend_runtime_let_if_build.stdout)
        print(backend_runtime_let_if_build.stderr)
        return 1
    backend_runtime_let_if_run = run([str(backend_runtime_let_if_bin)], cwd=repo, check=False)
    if backend_runtime_let_if_run.returncode != 42:
        print("expected runtime-let-in-if binary to exit with 42")
        print(f"actual exit code: {backend_runtime_let_if_run.returncode}")
        return 1

    print("[rust] backend bridge runtime let in while run")
    backend_runtime_let_while_bin = repo / "tests/rust/out-runtime-let-in-while.bin"
    backend_runtime_let_while_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-let-in-while.rs", "--rust-backend-v1", "-o", str(backend_runtime_let_while_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_runtime_let_while_build.stdout or backend_runtime_let_while_build.stderr != "":
        print("expected backend bridge success for run-runtime-let-in-while")
        print(backend_runtime_let_while_build.stdout)
        print(backend_runtime_let_while_build.stderr)
        return 1
    backend_runtime_let_while_run = run([str(backend_runtime_let_while_bin)], cwd=repo, check=False)
    if backend_runtime_let_while_run.returncode != 42:
        print("expected runtime-let-in-while binary to exit with 42")
        print(f"actual exit code: {backend_runtime_let_while_run.returncode}")
        return 1

    print("[rust] backend bridge runtime let in if nested run")
    backend_runtime_let_if_nested_bin = repo / "tests/rust/out-runtime-let-in-if-nested.bin"
    backend_runtime_let_if_nested_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-let-in-if-nested.rs", "--rust-backend-v1", "-o", str(backend_runtime_let_if_nested_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_runtime_let_if_nested_build.stdout or backend_runtime_let_if_nested_build.stderr != "":
        print("expected backend bridge success for run-runtime-let-in-if-nested")
        print(backend_runtime_let_if_nested_build.stdout)
        print(backend_runtime_let_if_nested_build.stderr)
        return 1
    backend_runtime_let_if_nested_run = run([str(backend_runtime_let_if_nested_bin)], cwd=repo, check=False)
    if backend_runtime_let_if_nested_run.returncode != 42:
        print("expected runtime-let-in-if-nested binary to exit with 42")
        print(f"actual exit code: {backend_runtime_let_if_nested_run.returncode}")
        return 1

    print("[rust] backend bridge runtime call run")
    backend_runtime_call_bin = repo / "tests/rust/out-runtime-call.bin"
    backend_runtime_call_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-call.rs", "--rust-backend-v1", "-o", str(backend_runtime_call_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_runtime_call_build.stdout or backend_runtime_call_build.stderr != "":
        print("expected backend bridge success for run-runtime-call")
        print(backend_runtime_call_build.stdout)
        print(backend_runtime_call_build.stderr)
        return 1
    backend_runtime_call_run = run([str(backend_runtime_call_bin)], cwd=repo, check=False)
    if backend_runtime_call_run.returncode != 42:
        print("expected runtime-call binary to exit with 42")
        print(f"actual exit code: {backend_runtime_call_run.returncode}")
        return 1

    print("[rust] backend bridge runtime call forward run")
    backend_runtime_call_fwd_bin = repo / "tests/rust/out-runtime-call-forward.bin"
    backend_runtime_call_fwd_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-call-forward.rs", "--rust-backend-v1", "-o", str(backend_runtime_call_fwd_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_runtime_call_fwd_build.stdout or backend_runtime_call_fwd_build.stderr != "":
        print("expected backend bridge success for run-runtime-call-forward")
        print(backend_runtime_call_fwd_build.stdout)
        print(backend_runtime_call_fwd_build.stderr)
        return 1
    backend_runtime_call_fwd_run = run([str(backend_runtime_call_fwd_bin)], cwd=repo, check=False)
    if backend_runtime_call_fwd_run.returncode != 42:
        print("expected runtime-call-forward binary to exit with 42")
        print(f"actual exit code: {backend_runtime_call_fwd_run.returncode}")
        return 1

    print("[rust] backend bridge runtime call local run")
    backend_runtime_call_local_bin = repo / "tests/rust/out-runtime-call-local.bin"
    backend_runtime_call_local_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-call-local.rs", "--rust-backend-v1", "-o", str(backend_runtime_call_local_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_runtime_call_local_build.stdout or backend_runtime_call_local_build.stderr != "":
        print("expected backend bridge success for run-runtime-call-local")
        print(backend_runtime_call_local_build.stdout)
        print(backend_runtime_call_local_build.stderr)
        return 1
    backend_runtime_call_local_run = run([str(backend_runtime_call_local_bin)], cwd=repo, check=False)
    if backend_runtime_call_local_run.returncode != 42:
        print("expected runtime-call-local binary to exit with 42")
        print(f"actual exit code: {backend_runtime_call_local_run.returncode}")
        return 1

    print("[rust] backend bridge runtime call arithmetic run")
    backend_runtime_call_arith_bin = repo / "tests/rust/out-runtime-call-arith.bin"
    backend_runtime_call_arith_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-call-arith.rs", "--rust-backend-v1", "-o", str(backend_runtime_call_arith_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_runtime_call_arith_build.stdout or backend_runtime_call_arith_build.stderr != "":
        print("expected backend bridge success for run-runtime-call-arith")
        print(backend_runtime_call_arith_build.stdout)
        print(backend_runtime_call_arith_build.stderr)
        return 1
    backend_runtime_call_arith_run = run([str(backend_runtime_call_arith_bin)], cwd=repo, check=False)
    if backend_runtime_call_arith_run.returncode != 42:
        print("expected runtime-call-arith binary to exit with 42")
        print(f"actual exit code: {backend_runtime_call_arith_run.returncode}")
        return 1

    print("[rust] backend bridge runtime call if run")
    backend_runtime_call_if_bin = repo / "tests/rust/out-runtime-call-if.bin"
    backend_runtime_call_if_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-call-if.rs", "--rust-backend-v1", "-o", str(backend_runtime_call_if_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_runtime_call_if_build.stdout or backend_runtime_call_if_build.stderr != "":
        print("expected backend bridge success for run-runtime-call-if")
        print(backend_runtime_call_if_build.stdout)
        print(backend_runtime_call_if_build.stderr)
        return 1
    backend_runtime_call_if_run = run([str(backend_runtime_call_if_bin)], cwd=repo, check=False)
    if backend_runtime_call_if_run.returncode != 42:
        print("expected runtime-call-if binary to exit with 42")
        print(f"actual exit code: {backend_runtime_call_if_run.returncode}")
        return 1

    print("[rust] backend bridge runtime call chain run")
    backend_runtime_call_chain_bin = repo / "tests/rust/out-runtime-call-chain.bin"
    backend_runtime_call_chain_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-call-chain.rs", "--rust-backend-v1", "-o", str(backend_runtime_call_chain_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_runtime_call_chain_build.stdout or backend_runtime_call_chain_build.stderr != "":
        print("expected backend bridge success for run-runtime-call-chain")
        print(backend_runtime_call_chain_build.stdout)
        print(backend_runtime_call_chain_build.stderr)
        return 1
    backend_runtime_call_chain_run = run([str(backend_runtime_call_chain_bin)], cwd=repo, check=False)
    if backend_runtime_call_chain_run.returncode != 42:
        print("expected runtime-call-chain binary to exit with 42")
        print(f"actual exit code: {backend_runtime_call_chain_run.returncode}")
        return 1

    print("[rust] backend bridge runtime call param run")
    backend_runtime_call_param_bin = repo / "tests/rust/out-runtime-call-param.bin"
    backend_runtime_call_param_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-call-param.rs", "--rust-backend-v1", "-o", str(backend_runtime_call_param_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_runtime_call_param_build.stdout or backend_runtime_call_param_build.stderr != "":
        print("expected backend bridge success for run-runtime-call-param")
        print(backend_runtime_call_param_build.stdout)
        print(backend_runtime_call_param_build.stderr)
        return 1
    backend_runtime_call_param_run = run([str(backend_runtime_call_param_bin)], cwd=repo, check=False)
    if backend_runtime_call_param_run.returncode != 42:
        print("expected runtime-call-param binary to exit with 42")
        print(f"actual exit code: {backend_runtime_call_param_run.returncode}")
        return 1

    print("[rust] backend bridge runtime call two params run")
    backend_runtime_call_two_bin = repo / "tests/rust/out-runtime-call-two-params.bin"
    backend_runtime_call_two_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-call-two-params.rs", "--rust-backend-v1", "-o", str(backend_runtime_call_two_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_runtime_call_two_build.stdout or backend_runtime_call_two_build.stderr != "":
        print("expected backend bridge success for run-runtime-call-two-params")
        print(backend_runtime_call_two_build.stdout)
        print(backend_runtime_call_two_build.stderr)
        return 1
    backend_runtime_call_two_run = run([str(backend_runtime_call_two_bin)], cwd=repo, check=False)
    if backend_runtime_call_two_run.returncode != 42:
        print("expected runtime-call-two-params binary to exit with 42")
        print(f"actual exit code: {backend_runtime_call_two_run.returncode}")
        return 1

    print("[rust] backend bridge runtime call param expr run")
    backend_runtime_call_param_expr_bin = repo / "tests/rust/out-runtime-call-param-expr.bin"
    backend_runtime_call_param_expr_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-call-param-expr.rs", "--rust-backend-v1", "-o", str(backend_runtime_call_param_expr_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_runtime_call_param_expr_build.stdout or backend_runtime_call_param_expr_build.stderr != "":
        print("expected backend bridge success for run-runtime-call-param-expr")
        print(backend_runtime_call_param_expr_build.stdout)
        print(backend_runtime_call_param_expr_build.stderr)
        return 1
    backend_runtime_call_param_expr_run = run([str(backend_runtime_call_param_expr_bin)], cwd=repo, check=False)
    if backend_runtime_call_param_expr_run.returncode != 42:
        print("expected runtime-call-param-expr binary to exit with 42")
        print(f"actual exit code: {backend_runtime_call_param_expr_run.returncode}")
        return 1

    print("[rust] backend bridge runtime call param nested run")
    backend_runtime_call_nested_bin = repo / "tests/rust/out-runtime-call-param-nested.bin"
    backend_runtime_call_nested_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-call-param-nested.rs", "--rust-backend-v1", "-o", str(backend_runtime_call_nested_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_runtime_call_nested_build.stdout or backend_runtime_call_nested_build.stderr != "":
        print("expected backend bridge success for run-runtime-call-param-nested")
        print(backend_runtime_call_nested_build.stdout)
        print(backend_runtime_call_nested_build.stderr)
        return 1
    backend_runtime_call_nested_run = run([str(backend_runtime_call_nested_bin)], cwd=repo, check=False)
    if backend_runtime_call_nested_run.returncode != 42:
        print("expected runtime-call-param-nested binary to exit with 42")
        print(f"actual exit code: {backend_runtime_call_nested_run.returncode}")
        return 1

    print("[rust] backend bridge runtime call param forward run")
    backend_runtime_call_param_fwd_bin = repo / "tests/rust/out-runtime-call-param-forward.bin"
    backend_runtime_call_param_fwd_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-call-param-forward.rs", "--rust-backend-v1", "-o", str(backend_runtime_call_param_fwd_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_runtime_call_param_fwd_build.stdout or backend_runtime_call_param_fwd_build.stderr != "":
        print("expected backend bridge success for run-runtime-call-param-forward")
        print(backend_runtime_call_param_fwd_build.stdout)
        print(backend_runtime_call_param_fwd_build.stderr)
        return 1
    backend_runtime_call_param_fwd_run = run([str(backend_runtime_call_param_fwd_bin)], cwd=repo, check=False)
    if backend_runtime_call_param_fwd_run.returncode != 42:
        print("expected runtime-call-param-forward binary to exit with 42")
        print(f"actual exit code: {backend_runtime_call_param_fwd_run.returncode}")
        return 1

    print("[rust] backend bridge runtime call four params run")
    backend_runtime_call_four_bin = repo / "tests/rust/out-runtime-call-four-params.bin"
    backend_runtime_call_four_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-call-four-params.rs", "--rust-backend-v1", "-o", str(backend_runtime_call_four_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_runtime_call_four_build.stdout or backend_runtime_call_four_build.stderr != "":
        print("expected backend bridge success for run-runtime-call-four-params")
        print(backend_runtime_call_four_build.stdout)
        print(backend_runtime_call_four_build.stderr)
        return 1
    backend_runtime_call_four_run = run([str(backend_runtime_call_four_bin)], cwd=repo, check=False)
    if backend_runtime_call_four_run.returncode != 42:
        print("expected runtime-call-four-params binary to exit with 42")
        print(f"actual exit code: {backend_runtime_call_four_run.returncode}")
        return 1

    print("[rust] backend bridge runtime call five params run")
    backend_runtime_call_five_bin = repo / "tests/rust/out-runtime-call-five-params.bin"
    backend_runtime_call_five_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-call-five-params.rs", "--rust-backend-v1", "-o", str(backend_runtime_call_five_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_runtime_call_five_build.stdout or backend_runtime_call_five_build.stderr != "":
        print("expected backend bridge success for run-runtime-call-five-params")
        print(backend_runtime_call_five_build.stdout)
        print(backend_runtime_call_five_build.stderr)
        return 1
    backend_runtime_call_five_run = run([str(backend_runtime_call_five_bin)], cwd=repo, check=False)
    if backend_runtime_call_five_run.returncode != 42:
        print("expected runtime-call-five-params binary to exit with 42")
        print(f"actual exit code: {backend_runtime_call_five_run.returncode}")
        return 1

    print("[rust] backend bridge runtime call nonleaf param run")
    backend_runtime_call_nonleaf_bin = repo / "tests/rust/out-runtime-call-nonleaf-param.bin"
    backend_runtime_call_nonleaf_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-call-nonleaf-param.rs", "--rust-backend-v1", "-o", str(backend_runtime_call_nonleaf_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_runtime_call_nonleaf_build.stdout or backend_runtime_call_nonleaf_build.stderr != "":
        print("expected backend bridge success for run-runtime-call-nonleaf-param")
        print(backend_runtime_call_nonleaf_build.stdout)
        print(backend_runtime_call_nonleaf_build.stderr)
        return 1
    backend_runtime_call_nonleaf_run = run([str(backend_runtime_call_nonleaf_bin)], cwd=repo, check=False)
    if backend_runtime_call_nonleaf_run.returncode != 42:
        print("expected runtime-call-nonleaf-param binary to exit with 42")
        print(f"actual exit code: {backend_runtime_call_nonleaf_run.returncode}")
        return 1

    print("[rust] backend bridge runtime call dual inner args run")
    backend_runtime_call_dual_inner_bin = repo / "tests/rust/out-runtime-call-dual-inner-args.bin"
    backend_runtime_call_dual_inner_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-call-dual-inner-args.rs", "--rust-backend-v1", "-o", str(backend_runtime_call_dual_inner_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_runtime_call_dual_inner_build.stdout or backend_runtime_call_dual_inner_build.stderr != "":
        print("expected backend bridge success for run-runtime-call-dual-inner-args")
        print(backend_runtime_call_dual_inner_build.stdout)
        print(backend_runtime_call_dual_inner_build.stderr)
        return 1
    backend_runtime_call_dual_inner_run = run([str(backend_runtime_call_dual_inner_bin)], cwd=repo, check=False)
    if backend_runtime_call_dual_inner_run.returncode != 42:
        print("expected runtime-call-dual-inner-args binary to exit with 42")
        print(f"actual exit code: {backend_runtime_call_dual_inner_run.returncode}")
        return 1

    print("[rust] backend bridge runtime call nonleaf forward run")
    backend_runtime_call_nonleaf_fwd_bin = repo / "tests/rust/out-runtime-call-nonleaf-forward.bin"
    backend_runtime_call_nonleaf_fwd_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-call-nonleaf-forward.rs", "--rust-backend-v1", "-o", str(backend_runtime_call_nonleaf_fwd_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_runtime_call_nonleaf_fwd_build.stdout or backend_runtime_call_nonleaf_fwd_build.stderr != "":
        print("expected backend bridge success for run-runtime-call-nonleaf-forward")
        print(backend_runtime_call_nonleaf_fwd_build.stdout)
        print(backend_runtime_call_nonleaf_fwd_build.stderr)
        return 1
    backend_runtime_call_nonleaf_fwd_run = run([str(backend_runtime_call_nonleaf_fwd_bin)], cwd=repo, check=False)
    if backend_runtime_call_nonleaf_fwd_run.returncode != 42:
        print("expected runtime-call-nonleaf-forward binary to exit with 42")
        print(f"actual exit code: {backend_runtime_call_nonleaf_fwd_run.returncode}")
        return 1

    print("[rust] backend bridge runtime call dec mixed args run")
    backend_runtime_call_dec_mixed_bin = repo / "tests/rust/out-runtime-call-dec-mixed-args.bin"
    backend_runtime_call_dec_mixed_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-call-dec-mixed-args.rs", "--rust-backend-v1", "-o", str(backend_runtime_call_dec_mixed_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_runtime_call_dec_mixed_build.stdout or backend_runtime_call_dec_mixed_build.stderr != "":
        print("expected backend bridge success for run-runtime-call-dec-mixed-args")
        print(backend_runtime_call_dec_mixed_build.stdout)
        print(backend_runtime_call_dec_mixed_build.stderr)
        return 1
    backend_runtime_call_dec_mixed_run = run([str(backend_runtime_call_dec_mixed_bin)], cwd=repo, check=False)
    if backend_runtime_call_dec_mixed_run.returncode != 42:
        print("expected runtime-call-dec-mixed-args binary to exit with 42")
        print(f"actual exit code: {backend_runtime_call_dec_mixed_run.returncode}")
        return 1

    print("[rust] backend bridge runtime call forward dual helpers run")
    backend_runtime_call_fwd_dual_bin = repo / "tests/rust/out-runtime-call-forward-dual-helpers.bin"
    backend_runtime_call_fwd_dual_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-call-forward-dual-helpers.rs", "--rust-backend-v1", "-o", str(backend_runtime_call_fwd_dual_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_runtime_call_fwd_dual_build.stdout or backend_runtime_call_fwd_dual_build.stderr != "":
        print("expected backend bridge success for run-runtime-call-forward-dual-helpers")
        print(backend_runtime_call_fwd_dual_build.stdout)
        print(backend_runtime_call_fwd_dual_build.stderr)
        return 1
    backend_runtime_call_fwd_dual_run = run([str(backend_runtime_call_fwd_dual_bin)], cwd=repo, check=False)
    if backend_runtime_call_fwd_dual_run.returncode != 42:
        print("expected runtime-call-forward-dual-helpers binary to exit with 42")
        print(f"actual exit code: {backend_runtime_call_fwd_dual_run.returncode}")
        return 1

    print("[rust] backend bridge runtime call param local then call run")
    backend_runtime_call_pltc_bin = repo / "tests/rust/out-runtime-call-param-local-then-call.bin"
    backend_runtime_call_pltc_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-call-param-local-then-call.rs", "--rust-backend-v1", "-o", str(backend_runtime_call_pltc_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_runtime_call_pltc_build.stdout or backend_runtime_call_pltc_build.stderr != "":
        print("expected backend bridge success for run-runtime-call-param-local-then-call")
        print(backend_runtime_call_pltc_build.stdout)
        print(backend_runtime_call_pltc_build.stderr)
        return 1
    backend_runtime_call_pltc_run = run([str(backend_runtime_call_pltc_bin)], cwd=repo, check=False)
    if backend_runtime_call_pltc_run.returncode != 42:
        print("expected runtime-call-param-local-then-call binary to exit with 42")
        print(f"actual exit code: {backend_runtime_call_pltc_run.returncode}")
        return 1

    print("[rust] backend bridge runtime call forward triple helpers run")
    backend_runtime_call_ftriple_bin = repo / "tests/rust/out-runtime-call-forward-triple-helpers.bin"
    backend_runtime_call_ftriple_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-call-forward-triple-helpers.rs", "--rust-backend-v1", "-o", str(backend_runtime_call_ftriple_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_runtime_call_ftriple_build.stdout or backend_runtime_call_ftriple_build.stderr != "":
        print("expected backend bridge success for run-runtime-call-forward-triple-helpers")
        print(backend_runtime_call_ftriple_build.stdout)
        print(backend_runtime_call_ftriple_build.stderr)
        return 1
    backend_runtime_call_ftriple_run = run([str(backend_runtime_call_ftriple_bin)], cwd=repo, check=False)
    if backend_runtime_call_ftriple_run.returncode != 42:
        print("expected runtime-call-forward-triple-helpers binary to exit with 42")
        print(f"actual exit code: {backend_runtime_call_ftriple_run.returncode}")
        return 1

    print("[rust] backend bridge runtime call param let branch call run")
    backend_runtime_call_plbc_bin = repo / "tests/rust/out-runtime-call-param-let-branch-call.bin"
    backend_runtime_call_plbc_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-call-param-let-branch-call.rs", "--rust-backend-v1", "-o", str(backend_runtime_call_plbc_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_runtime_call_plbc_build.stdout or backend_runtime_call_plbc_build.stderr != "":
        print("expected backend bridge success for run-runtime-call-param-let-branch-call")
        print(backend_runtime_call_plbc_build.stdout)
        print(backend_runtime_call_plbc_build.stderr)
        return 1
    backend_runtime_call_plbc_run = run([str(backend_runtime_call_plbc_bin)], cwd=repo, check=False)
    if backend_runtime_call_plbc_run.returncode != 42:
        print("expected runtime-call-param-let-branch-call binary to exit with 42")
        print(f"actual exit code: {backend_runtime_call_plbc_run.returncode}")
        return 1

    print("[rust] backend bridge runtime call three params run")
    backend_runtime_call_three_bin = repo / "tests/rust/out-runtime-call-three-params.bin"
    backend_runtime_call_three_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-call-three-params.rs", "--rust-backend-v1", "-o", str(backend_runtime_call_three_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_runtime_call_three_build.stdout or backend_runtime_call_three_build.stderr != "":
        print("expected backend bridge success for run-runtime-call-three-params")
        print(backend_runtime_call_three_build.stdout)
        print(backend_runtime_call_three_build.stderr)
        return 1
    backend_runtime_call_three_run = run([str(backend_runtime_call_three_bin)], cwd=repo, check=False)
    if backend_runtime_call_three_run.returncode != 42:
        print("expected runtime-call-three-params binary to exit with 42")
        print(f"actual exit code: {backend_runtime_call_three_run.returncode}")
        return 1

    print("[rust] backend bridge runtime call six params run")
    backend_runtime_call_six_bin = repo / "tests/rust/out-runtime-call-six-params.bin"
    backend_runtime_call_six_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-call-six-params.rs", "--rust-backend-v1", "-o", str(backend_runtime_call_six_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_runtime_call_six_build.stdout or backend_runtime_call_six_build.stderr != "":
        print("expected backend bridge success for run-runtime-call-six-params")
        print(backend_runtime_call_six_build.stdout)
        print(backend_runtime_call_six_build.stderr)
        return 1
    backend_runtime_call_six_run = run([str(backend_runtime_call_six_bin)], cwd=repo, check=False)
    if backend_runtime_call_six_run.returncode != 42:
        print("expected runtime-call-six-params binary to exit with 42")
        print(f"actual exit code: {backend_runtime_call_six_run.returncode}")
        return 1

    print("[rust] backend bridge runtime call seven params (stack args) run")
    backend_runtime_call_seven_bin = repo / "tests/rust/out-runtime-call-seven-params.bin"
    backend_runtime_call_seven_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-call-seven-params.rs", "--rust-backend-v1", "-o", str(backend_runtime_call_seven_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_runtime_call_seven_build.stdout or backend_runtime_call_seven_build.stderr != "":
        print("expected backend bridge success for run-runtime-call-seven-params")
        print(backend_runtime_call_seven_build.stdout)
        print(backend_runtime_call_seven_build.stderr)
        return 1
    backend_runtime_call_seven_run = run([str(backend_runtime_call_seven_bin)], cwd=repo, check=False)
    if backend_runtime_call_seven_run.returncode != 42:
        print("expected runtime-call-seven-params binary to exit with 42")
        print(f"actual exit code: {backend_runtime_call_seven_run.returncode}")
        return 1

    print("[rust] backend bridge runtime call bool parameter run")
    backend_runtime_call_bool_param_bin = repo / "tests/rust/out-runtime-call-bool-param.bin"
    backend_runtime_call_bool_param_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-call-bool-param.rs", "--rust-backend-v1", "-o", str(backend_runtime_call_bool_param_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_runtime_call_bool_param_build.stdout or backend_runtime_call_bool_param_build.stderr != "":
        print("expected backend bridge success for run-runtime-call-bool-param")
        print(backend_runtime_call_bool_param_build.stdout)
        print(backend_runtime_call_bool_param_build.stderr)
        return 1
    backend_runtime_call_bool_param_run = run([str(backend_runtime_call_bool_param_bin)], cwd=repo, check=False)
    if backend_runtime_call_bool_param_run.returncode != 42:
        print("expected run-runtime-call-bool-param binary to exit with 42")
        print(f"actual exit code: {backend_runtime_call_bool_param_run.returncode}")
        return 1

    print("[rust] backend bridge runtime call bool return run")
    backend_runtime_call_bool_ret_bin = repo / "tests/rust/out-runtime-call-bool-return.bin"
    backend_runtime_call_bool_ret_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-call-bool-return.rs", "--rust-backend-v1", "-o", str(backend_runtime_call_bool_ret_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_runtime_call_bool_ret_build.stdout or backend_runtime_call_bool_ret_build.stderr != "":
        print("expected backend bridge success for run-runtime-call-bool-return")
        print(backend_runtime_call_bool_ret_build.stdout)
        print(backend_runtime_call_bool_ret_build.stderr)
        return 1
    backend_runtime_call_bool_ret_run = run([str(backend_runtime_call_bool_ret_bin)], cwd=repo, check=False)
    if backend_runtime_call_bool_ret_run.returncode != 42:
        print("expected run-runtime-call-bool-return binary to exit with 42")
        print(f"actual exit code: {backend_runtime_call_bool_ret_run.returncode}")
        return 1

    print("[rust] backend bridge strict let-annotation mode run")
    backend_strict_let_ok_bin = repo / "tests/rust/out-runtime-strict-let-annotated.bin"
    backend_strict_let_ok_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-strict-let-annotated.rs", "--rust-backend-v1", "--rust-strict-let-annotations", "-o", str(backend_strict_let_ok_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_strict_let_ok_build.stdout or backend_strict_let_ok_build.stderr != "":
        print("expected backend bridge success for strict let-annotation mode")
        print(backend_strict_let_ok_build.stdout)
        print(backend_strict_let_ok_build.stderr)
        return 1
    backend_strict_let_ok_run = run([str(backend_strict_let_ok_bin)], cwd=repo, check=False)
    if backend_strict_let_ok_run.returncode != 42:
        print("expected run-runtime-strict-let-annotated binary to exit with 42")
        print(f"actual exit code: {backend_strict_let_ok_run.returncode}")
        return 1

    print("[rust] backend bridge strict function-signature mode run")
    backend_strict_sig_ok_bin = repo / "tests/rust/out-runtime-strict-signature.bin"
    backend_strict_sig_ok_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-strict-let-annotated.rs", "--rust-backend-v1", "--rust-strict-function-signatures", "-o", str(backend_strict_sig_ok_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_strict_sig_ok_build.stdout or backend_strict_sig_ok_build.stderr != "":
        print("expected backend bridge success for strict function-signature mode")
        print(backend_strict_sig_ok_build.stdout)
        print(backend_strict_sig_ok_build.stderr)
        return 1
    backend_strict_sig_ok_run = run([str(backend_strict_sig_ok_bin)], cwd=repo, check=False)
    if backend_strict_sig_ok_run.returncode != 42:
        print("expected out-runtime-strict-signature binary to exit with 42")
        print(f"actual exit code: {backend_strict_sig_ok_run.returncode}")
        return 1

    print("[rust] backend bridge strict combined mode run")
    backend_strict_combined_ok_bin = repo / "tests/rust/out-runtime-strict-combined.bin"
    backend_strict_combined_ok_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-strict-let-annotated.rs", "--rust-backend-v1", "--rust-strict", "-o", str(backend_strict_combined_ok_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_strict_combined_ok_build.stdout or backend_strict_combined_ok_build.stderr != "":
        print("expected backend bridge success for strict combined mode")
        print(backend_strict_combined_ok_build.stdout)
        print(backend_strict_combined_ok_build.stderr)
        return 1
    backend_strict_combined_ok_run = run([str(backend_strict_combined_ok_bin)], cwd=repo, check=False)
    if backend_strict_combined_ok_run.returncode != 42:
        print("expected out-runtime-strict-combined binary to exit with 42")
        print(f"actual exit code: {backend_strict_combined_ok_run.returncode}")
        return 1

    print("[rust] backend bridge runtime mutable assignment loop run")
    backend_assign_loop_bin = repo / "tests/rust/out-runtime-assign-loop.bin"
    backend_assign_loop_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-assign-loop.rs", "--rust-backend-v1", "-o", str(backend_assign_loop_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_assign_loop_build.stdout or backend_assign_loop_build.stderr != "":
        print("expected backend bridge success for run-runtime-assign-loop")
        print(backend_assign_loop_build.stdout)
        print(backend_assign_loop_build.stderr)
        return 1
    backend_assign_loop_run = run([str(backend_assign_loop_bin)], cwd=repo, check=False)
    if backend_assign_loop_run.returncode != 42:
        print("expected run-runtime-assign-loop binary to exit with 42")
        print(f"actual exit code: {backend_assign_loop_run.returncode}")
        return 1

    print("[rust] backend bridge runtime direct recursion run")
    backend_recur42_bin = repo / "tests/rust/out-runtime-recursive-count42.bin"
    backend_recur42_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-recursive-count42.rs", "--rust-backend-v1", "-o", str(backend_recur42_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_recur42_build.stdout or backend_recur42_build.stderr != "":
        print("expected backend bridge success for run-runtime-recursive-count42")
        print(backend_recur42_build.stdout)
        print(backend_recur42_build.stderr)
        return 1
    backend_recur42_run = run([str(backend_recur42_bin)], cwd=repo, check=False)
    if backend_recur42_run.returncode != 42:
        print("expected run-runtime-recursive-count42 binary to exit with 42")
        print(f"actual exit code: {backend_recur42_run.returncode}")
        return 1

    print("[rust] backend bridge runtime mutual recursion run")
    backend_mutual_bin = repo / "tests/rust/out-runtime-recursive-mutual.bin"
    backend_mutual_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-recursive-mutual.rs", "--rust-backend-v1", "-o", str(backend_mutual_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_mutual_build.stdout or backend_mutual_build.stderr != "":
        print("expected backend bridge success for run-runtime-recursive-mutual")
        print(backend_mutual_build.stdout)
        print(backend_mutual_build.stderr)
        return 1
    backend_mutual_run = run([str(backend_mutual_bin)], cwd=repo, check=False)
    if backend_mutual_run.returncode != 42:
        print("expected run-runtime-recursive-mutual binary to exit with 42")
        print(f"actual exit code: {backend_mutual_run.returncode}")
        return 1

    print("[rust] backend bridge runtime call param if run")
    backend_runtime_call_param_if_bin = repo / "tests/rust/out-runtime-call-param-if.bin"
    backend_runtime_call_param_if_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-call-param-if.rs", "--rust-backend-v1", "-o", str(backend_runtime_call_param_if_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_runtime_call_param_if_build.stdout or backend_runtime_call_param_if_build.stderr != "":
        print("expected backend bridge success for run-runtime-call-param-if")
        print(backend_runtime_call_param_if_build.stdout)
        print(backend_runtime_call_param_if_build.stderr)
        return 1
    backend_runtime_call_param_if_run = run([str(backend_runtime_call_param_if_bin)], cwd=repo, check=False)
    if backend_runtime_call_param_if_run.returncode != 42:
        print("expected runtime-call-param-if binary to exit with 42")
        print(f"actual exit code: {backend_runtime_call_param_if_run.returncode}")
        return 1

    print("[rust] backend bridge runtime call param while run")
    backend_runtime_call_param_while_bin = repo / "tests/rust/out-runtime-call-param-while.bin"
    backend_runtime_call_param_while_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-call-param-while.rs", "--rust-backend-v1", "-o", str(backend_runtime_call_param_while_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_runtime_call_param_while_build.stdout or backend_runtime_call_param_while_build.stderr != "":
        print("expected backend bridge success for run-runtime-call-param-while")
        print(backend_runtime_call_param_while_build.stdout)
        print(backend_runtime_call_param_while_build.stderr)
        return 1
    backend_runtime_call_param_while_run = run([str(backend_runtime_call_param_while_bin)], cwd=repo, check=False)
    if backend_runtime_call_param_while_run.returncode != 42:
        print("expected runtime-call-param-while binary to exit with 42")
        print(f"actual exit code: {backend_runtime_call_param_while_run.returncode}")
        return 1

    print("[rust] backend bridge runtime if logic run")
    backend_runtime_logic_bin = repo / "tests/rust/out-runtime-if-logic.bin"
    backend_runtime_logic_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-if-logic.rs", "--rust-backend-v1", "-o", str(backend_runtime_logic_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_runtime_logic_build.stdout or backend_runtime_logic_build.stderr != "":
        print("expected backend bridge success for run-runtime-if-logic")
        print(backend_runtime_logic_build.stdout)
        print(backend_runtime_logic_build.stderr)
        return 1
    backend_runtime_logic_run = run([str(backend_runtime_logic_bin)], cwd=repo, check=False)
    if backend_runtime_logic_run.returncode != 42:
        print("expected runtime-if-logic binary to exit with 42")
        print(f"actual exit code: {backend_runtime_logic_run.returncode}")
        return 1

    print("[rust] backend bridge runtime if logic and run")
    backend_runtime_logic_and_bin = repo / "tests/rust/out-runtime-if-logic-and.bin"
    backend_runtime_logic_and_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-if-logic-and.rs", "--rust-backend-v1", "-o", str(backend_runtime_logic_and_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_runtime_logic_and_build.stdout or backend_runtime_logic_and_build.stderr != "":
        print("expected backend bridge success for run-runtime-if-logic-and")
        print(backend_runtime_logic_and_build.stdout)
        print(backend_runtime_logic_and_build.stderr)
        return 1
    backend_runtime_logic_and_run = run([str(backend_runtime_logic_and_bin)], cwd=repo, check=False)
    if backend_runtime_logic_and_run.returncode != 42:
        print("expected runtime-if-logic-and binary to exit with 42")
        print(f"actual exit code: {backend_runtime_logic_and_run.returncode}")
        return 1

    print("[rust] backend bridge runtime if logic or run")
    backend_runtime_logic_or_bin = repo / "tests/rust/out-runtime-if-logic-or.bin"
    backend_runtime_logic_or_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-if-logic-or.rs", "--rust-backend-v1", "-o", str(backend_runtime_logic_or_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_runtime_logic_or_build.stdout or backend_runtime_logic_or_build.stderr != "":
        print("expected backend bridge success for run-runtime-if-logic-or")
        print(backend_runtime_logic_or_build.stdout)
        print(backend_runtime_logic_or_build.stderr)
        return 1
    backend_runtime_logic_or_run = run([str(backend_runtime_logic_or_bin)], cwd=repo, check=False)
    if backend_runtime_logic_or_run.returncode != 42:
        print("expected runtime-if-logic-or binary to exit with 42")
        print(f"actual exit code: {backend_runtime_logic_or_run.returncode}")
        return 1

    print("[rust] backend bridge runtime if logic not run")
    backend_runtime_logic_not_bin = repo / "tests/rust/out-runtime-if-logic-not.bin"
    backend_runtime_logic_not_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-if-logic-not.rs", "--rust-backend-v1", "-o", str(backend_runtime_logic_not_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_runtime_logic_not_build.stdout or backend_runtime_logic_not_build.stderr != "":
        print("expected backend bridge success for run-runtime-if-logic-not")
        print(backend_runtime_logic_not_build.stdout)
        print(backend_runtime_logic_not_build.stderr)
        return 1
    backend_runtime_logic_not_run = run([str(backend_runtime_logic_not_bin)], cwd=repo, check=False)
    if backend_runtime_logic_not_run.returncode != 42:
        print("expected runtime-if-logic-not binary to exit with 42")
        print(f"actual exit code: {backend_runtime_logic_not_run.returncode}")
        return 1

    print("[rust] backend bridge runtime if logic precedence run")
    backend_runtime_logic_prec_bin = repo / "tests/rust/out-runtime-if-logic-precedence.bin"
    backend_runtime_logic_prec_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-if-logic-precedence.rs", "--rust-backend-v1", "-o", str(backend_runtime_logic_prec_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_runtime_logic_prec_build.stdout or backend_runtime_logic_prec_build.stderr != "":
        print("expected backend bridge success for run-runtime-if-logic-precedence")
        print(backend_runtime_logic_prec_build.stdout)
        print(backend_runtime_logic_prec_build.stderr)
        return 1
    backend_runtime_logic_prec_run = run([str(backend_runtime_logic_prec_bin)], cwd=repo, check=False)
    if backend_runtime_logic_prec_run.returncode != 42:
        print("expected runtime-if-logic-precedence binary to exit with 42")
        print(f"actual exit code: {backend_runtime_logic_prec_run.returncode}")
        return 1

    print("[rust] backend bridge runtime if logic bool-local run")
    backend_runtime_logic_bool_bin = repo / "tests/rust/out-runtime-if-logic-bool-local.bin"
    backend_runtime_logic_bool_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-if-logic-bool-local.rs", "--rust-backend-v1", "-o", str(backend_runtime_logic_bool_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_runtime_logic_bool_build.stdout or backend_runtime_logic_bool_build.stderr != "":
        print("expected backend bridge success for run-runtime-if-logic-bool-local")
        print(backend_runtime_logic_bool_build.stdout)
        print(backend_runtime_logic_bool_build.stderr)
        return 1
    backend_runtime_logic_bool_run = run([str(backend_runtime_logic_bool_bin)], cwd=repo, check=False)
    if backend_runtime_logic_bool_run.returncode != 42:
        print("expected runtime-if-logic-bool-local binary to exit with 42")
        print(f"actual exit code: {backend_runtime_logic_bool_run.returncode}")
        return 1

    print("[rust] backend bridge runtime logic and short-circuit run")
    backend_runtime_logic_and_sc_bin = repo / "tests/rust/out-runtime-logic-and-short-circuit.bin"
    backend_runtime_logic_and_sc_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-logic-and-short-circuit.rs", "--rust-backend-v1", "-o", str(backend_runtime_logic_and_sc_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_runtime_logic_and_sc_build.stdout or backend_runtime_logic_and_sc_build.stderr != "":
        print("expected backend bridge success for run-runtime-logic-and-short-circuit")
        print(backend_runtime_logic_and_sc_build.stdout)
        print(backend_runtime_logic_and_sc_build.stderr)
        return 1
    backend_runtime_logic_and_sc_run = run([str(backend_runtime_logic_and_sc_bin)], cwd=repo, check=False)
    if backend_runtime_logic_and_sc_run.returncode != 42:
        print("expected runtime-logic-and-short-circuit binary to exit with 42")
        print(f"actual exit code: {backend_runtime_logic_and_sc_run.returncode}")
        return 1

    print("[rust] backend bridge runtime logic or short-circuit run")
    backend_runtime_logic_or_sc_bin = repo / "tests/rust/out-runtime-logic-or-short-circuit.bin"
    backend_runtime_logic_or_sc_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-logic-or-short-circuit.rs", "--rust-backend-v1", "-o", str(backend_runtime_logic_or_sc_bin)],
        cwd=repo,
        check=True,
    )
    if "Rust backend bridge compilation completed." not in backend_runtime_logic_or_sc_build.stdout or backend_runtime_logic_or_sc_build.stderr != "":
        print("expected backend bridge success for run-runtime-logic-or-short-circuit")
        print(backend_runtime_logic_or_sc_build.stdout)
        print(backend_runtime_logic_or_sc_build.stderr)
        return 1
    backend_runtime_logic_or_sc_run = run([str(backend_runtime_logic_or_sc_bin)], cwd=repo, check=False)
    if backend_runtime_logic_or_sc_run.returncode != 42:
        print("expected runtime-logic-or-short-circuit binary to exit with 42")
        print(f"actual exit code: {backend_runtime_logic_or_sc_run.returncode}")
        return 1

    print("[rust] backend bridge runtime assembly shape check")
    runtime_asm = repo / "tests/rust/out-runtime-if-logic.s"
    runtime_asm_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-if-logic.rs", "--rust-backend-v1", "-o", str(runtime_asm)],
        cwd=repo,
        check=True,
    )
    if runtime_asm_build.stderr != "":
        print("expected empty stderr for runtime assembly dump build")
        print(runtime_asm_build.stderr)
        return 1
    runtime_asm_text = runtime_asm.read_text(encoding="utf-8")
    if ("subq $" not in runtime_asm_text or
            "(%rbp)" not in runtime_asm_text or
            "je .Lrust_else_" not in runtime_asm_text or
            "jmp .Lrust_end_if_" not in runtime_asm_text or
            ".Lrust_else_" not in runtime_asm_text or
            ".Lrust_end_if_" not in runtime_asm_text or
            "setg %al" not in runtime_asm_text or
            ".Lrust_logic_" not in runtime_asm_text):
        print("runtime assembly shape mismatch for logical branch codegen")
        print(runtime_asm_text)
        return 1

    print("[rust] backend bridge runtime while assembly shape check")
    while_asm = repo / "tests/rust/out-runtime-while-logic.s"
    while_asm_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-while-logic.rs", "--rust-backend-v1", "-o", str(while_asm)],
        cwd=repo,
        check=True,
    )
    if while_asm_build.stderr != "":
        print("expected empty stderr for while assembly dump build")
        print(while_asm_build.stderr)
        return 1
    while_asm_text = while_asm.read_text(encoding="utf-8")
    if (".Lrust_while_start_" not in while_asm_text or
            ".Lrust_while_end_" not in while_asm_text or
            "je .Lrust_while_end_" not in while_asm_text or
            "jmp .Lrust_while_start_" not in while_asm_text or
            ".Lrust_logic_" not in while_asm_text):
        print("runtime assembly shape mismatch for while loop codegen")
        print(while_asm_text)
        return 1

    print("[rust] backend bridge runtime call assembly shape check")
    call_asm = repo / "tests/rust/out-runtime-call-arith.s"
    call_asm_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-call-arith.rs", "--rust-backend-v1", "-o", str(call_asm)],
        cwd=repo,
        check=True,
    )
    if call_asm_build.stderr != "":
        print("expected empty stderr for call assembly dump build")
        print(call_asm_build.stderr)
        return 1
    call_asm_text = call_asm.read_text(encoding="utf-8")
    if ("call rust_fn_" not in call_asm_text or
            "rust_fn_" not in call_asm_text):
        print("runtime assembly shape mismatch for function call codegen")
        print(call_asm_text)
        return 1

    print("[rust] backend bridge runtime bool call assembly shape check")
    call_bool_asm = repo / "tests/rust/out-runtime-call-bool-return.s"
    call_bool_asm_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-call-bool-return.rs", "--rust-backend-v1", "-o", str(call_bool_asm)],
        cwd=repo,
        check=True,
    )
    if call_bool_asm_build.stderr != "":
        print("expected empty stderr for bool-call assembly dump build")
        print(call_bool_asm_build.stderr)
        return 1
    call_bool_asm_text = call_bool_asm.read_text(encoding="utf-8")
    if ("call rust_fn_" not in call_bool_asm_text or
            "set" not in call_bool_asm_text or
            "movzbl %al, %eax" not in call_bool_asm_text):
        print("runtime assembly shape mismatch for bool-return call codegen")
        print(call_bool_asm_text)
        return 1

    print("[rust] backend bridge runtime call param assembly shape check")
    call_param_asm = repo / "tests/rust/out-runtime-call-param.s"
    call_param_asm_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-call-param.rs", "--rust-backend-v1", "-o", str(call_param_asm)],
        cwd=repo,
        check=True,
    )
    if call_param_asm_build.stderr != "":
        print("expected empty stderr for call-param assembly dump build")
        print(call_param_asm_build.stderr)
        return 1
    call_param_asm_text = call_param_asm.read_text(encoding="utf-8")
    if ("movl %eax, %edi" not in call_param_asm_text or
            "movl %edi, " not in call_param_asm_text or
            "call rust_fn_" not in call_param_asm_text):
        print("runtime assembly shape mismatch for parameterized call codegen")
        print(call_param_asm_text)
        return 1

    print("[rust] backend bridge runtime call six params assembly shape check")
    call_six_asm = repo / "tests/rust/out-runtime-call-six-params.s"
    call_six_asm_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-call-six-params.rs", "--rust-backend-v1", "-o", str(call_six_asm)],
        cwd=repo,
        check=True,
    )
    if call_six_asm_build.stderr != "":
        print("expected empty stderr for call-six-params assembly dump build")
        print(call_six_asm_build.stderr)
        return 1
    call_six_asm_text = call_six_asm.read_text(encoding="utf-8")
    if ("movl %eax, %edi" not in call_six_asm_text or
            "movl %eax, %esi" not in call_six_asm_text or
            "movl %eax, %edx" not in call_six_asm_text or
            "movl %eax, %ecx" not in call_six_asm_text or
            "movl %eax, %r8d" not in call_six_asm_text or
            "movl %eax, %r9d" not in call_six_asm_text or
            "movl %r8d, " not in call_six_asm_text or
            "movl %r9d, " not in call_six_asm_text):
        print("runtime assembly shape mismatch for six-parameter call codegen")
        print(call_six_asm_text)
        return 1

    print("[rust] backend bridge runtime call seven params (stack args) assembly shape check")
    call_seven_asm = repo / "tests/rust/out-runtime-call-seven-params.s"
    call_seven_asm_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-call-seven-params.rs", "--rust-backend-v1", "-o", str(call_seven_asm)],
        cwd=repo,
        check=True,
    )
    if call_seven_asm_build.stderr != "":
        print("expected empty stderr for call-seven-params assembly dump build")
        print(call_seven_asm_build.stderr)
        return 1
    call_seven_asm_text = call_seven_asm.read_text(encoding="utf-8")
    if ("pushq %rax" not in call_seven_asm_text or
            "subq $16, %rsp" not in call_seven_asm_text or
            "addq $16, %rsp" not in call_seven_asm_text or
            ("movl %eax, 0(%rsp)" not in call_seven_asm_text and
             "movl %eax, (%rsp)" not in call_seven_asm_text) or
            "movl 16(%rbp), %eax" not in call_seven_asm_text or
            "movl %eax, %edi" not in call_seven_asm_text or
            "movl %eax, %esi" not in call_seven_asm_text or
            "movl %eax, %r9d" not in call_seven_asm_text or
            "call rust_fn_" not in call_seven_asm_text):
        print("runtime assembly shape mismatch for seven-parameter SysV stack call codegen")
        print(call_seven_asm_text)
        return 1

    print("[rust] backend bridge runtime mutable assignment loop assembly shape check")
    assign_loop_asm = repo / "tests/rust/out-runtime-assign-loop.s"
    assign_loop_asm_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-assign-loop.rs", "--rust-backend-v1", "-o", str(assign_loop_asm)],
        cwd=repo,
        check=True,
    )
    if assign_loop_asm_build.stderr != "":
        print("expected empty stderr for run-runtime-assign-loop assembly dump build")
        print(assign_loop_asm_build.stderr)
        return 1
    assign_loop_asm_text = assign_loop_asm.read_text(encoding="utf-8")
    if ("movl $0, %eax" not in assign_loop_asm_text or
            assign_loop_asm_text.count("movl %eax, -") < 4 or
            ".Lrust_while_start_" not in assign_loop_asm_text or
            ".Lrust_while_end_" not in assign_loop_asm_text):
        print("runtime assembly shape mismatch for mutable assignment in while loop")
        print(assign_loop_asm_text)
        return 1

    print("[rust] backend bridge runtime direct recursion assembly shape check")
    recur42_asm = repo / "tests/rust/out-runtime-recursive-count42.s"
    recur42_asm_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-recursive-count42.rs", "--rust-backend-v1", "-o", str(recur42_asm)],
        cwd=repo,
        check=True,
    )
    if recur42_asm_build.stderr != "":
        print("expected empty stderr for run-runtime-recursive-count42 assembly dump build")
        print(recur42_asm_build.stderr)
        return 1
    recur42_asm_text = recur42_asm.read_text(encoding="utf-8")
    if (recur42_asm_text.count("call rust_fn_") < 2 or
            ".Lrust_else_" not in recur42_asm_text or
            ".Lrust_end_if_" not in recur42_asm_text):
        print("runtime assembly shape mismatch for direct tail recursion to same rust_fn_ label")
        print(recur42_asm_text)
        return 1

    print("[rust] backend bridge runtime mutual recursion assembly shape check")
    mutual_asm = repo / "tests/rust/out-runtime-recursive-mutual.s"
    mutual_asm_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-recursive-mutual.rs", "--rust-backend-v1", "-o", str(mutual_asm)],
        cwd=repo,
        check=True,
    )
    if mutual_asm_build.stderr != "":
        print("expected empty stderr for run-runtime-recursive-mutual assembly dump build")
        print(mutual_asm_build.stderr)
        return 1
    mutual_asm_text = mutual_asm.read_text(encoding="utf-8")
    if mutual_asm_text.count("call rust_fn_") < 3:
        print("runtime assembly shape mismatch for mutual recursion (ping + pong + main call sites)")
        print(mutual_asm_text)
        return 1

    print("[rust] backend bridge runtime call param nested assembly shape check")
    call_nested_asm = repo / "tests/rust/out-runtime-call-param-nested.s"
    call_nested_asm_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-call-param-nested.rs", "--rust-backend-v1", "-o", str(call_nested_asm)],
        cwd=repo,
        check=True,
    )
    if call_nested_asm_build.stderr != "":
        print("expected empty stderr for call-param-nested assembly dump build")
        print(call_nested_asm_build.stderr)
        return 1
    call_nested_asm_text = call_nested_asm.read_text(encoding="utf-8")
    if call_nested_asm_text.count("call rust_fn_") < 2:
        print("runtime assembly shape mismatch for nested runtime calls")
        print(call_nested_asm_text)
        return 1

    print("[rust] backend bridge runtime call dual inner args assembly shape check")
    call_dual_inner_asm = repo / "tests/rust/out-runtime-call-dual-inner-args.s"
    call_dual_inner_asm_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-call-dual-inner-args.rs", "--rust-backend-v1", "-o", str(call_dual_inner_asm)],
        cwd=repo,
        check=True,
    )
    if call_dual_inner_asm_build.stderr != "":
        print("expected empty stderr for call-dual-inner-args assembly dump build")
        print(call_dual_inner_asm_build.stderr)
        return 1
    call_dual_inner_asm_text = call_dual_inner_asm.read_text(encoding="utf-8")
    if call_dual_inner_asm_text.count("call rust_fn_") < 3:
        print("runtime assembly shape mismatch for dual inner call sites")
        print(call_dual_inner_asm_text)
        return 1

    print("[rust] backend bridge runtime call nonleaf param assembly shape check")
    call_nonleaf_asm = repo / "tests/rust/out-runtime-call-nonleaf-param.s"
    call_nonleaf_asm_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-call-nonleaf-param.rs", "--rust-backend-v1", "-o", str(call_nonleaf_asm)],
        cwd=repo,
        check=True,
    )
    if call_nonleaf_asm_build.stderr != "":
        print("expected empty stderr for call-nonleaf-param assembly dump build")
        print(call_nonleaf_asm_build.stderr)
        return 1
    call_nonleaf_asm_text = call_nonleaf_asm.read_text(encoding="utf-8")
    if call_nonleaf_asm_text.count("call rust_fn_") < 2:
        print("runtime assembly shape mismatch for non-leaf param callee chain")
        print(call_nonleaf_asm_text)
        return 1

    print("[rust] backend bridge runtime call nonleaf forward assembly shape check")
    call_nonleaf_fwd_asm = repo / "tests/rust/out-runtime-call-nonleaf-forward.s"
    call_nonleaf_fwd_asm_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-call-nonleaf-forward.rs", "--rust-backend-v1", "-o", str(call_nonleaf_fwd_asm)],
        cwd=repo,
        check=True,
    )
    if call_nonleaf_fwd_asm_build.stderr != "":
        print("expected empty stderr for call-nonleaf-forward assembly dump build")
        print(call_nonleaf_fwd_asm_build.stderr)
        return 1
    call_nonleaf_fwd_asm_text = call_nonleaf_fwd_asm.read_text(encoding="utf-8")
    if call_nonleaf_fwd_asm_text.count("call rust_fn_") < 2:
        print("runtime assembly shape mismatch for forward-declared non-leaf call chain")
        print(call_nonleaf_fwd_asm_text)
        return 1

    print("[rust] backend bridge runtime call dec mixed args assembly shape check")
    call_dec_mixed_asm = repo / "tests/rust/out-runtime-call-dec-mixed-args.s"
    call_dec_mixed_asm_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-call-dec-mixed-args.rs", "--rust-backend-v1", "-o", str(call_dec_mixed_asm)],
        cwd=repo,
        check=True,
    )
    if call_dec_mixed_asm_build.stderr != "":
        print("expected empty stderr for call-dec-mixed-args assembly dump build")
        print(call_dec_mixed_asm_build.stderr)
        return 1
    call_dec_mixed_asm_text = call_dec_mixed_asm.read_text(encoding="utf-8")
    if (call_dec_mixed_asm_text.count("call rust_fn_") < 3 or
            "subl" not in call_dec_mixed_asm_text):
        print("runtime assembly shape mismatch for inc/dec/add mixed arg calls")
        print(call_dec_mixed_asm_text)
        return 1

    print("[rust] backend bridge runtime call forward dual helpers assembly shape check")
    call_fwd_dual_asm = repo / "tests/rust/out-runtime-call-forward-dual-helpers.s"
    call_fwd_dual_asm_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-call-forward-dual-helpers.rs", "--rust-backend-v1", "-o", str(call_fwd_dual_asm)],
        cwd=repo,
        check=True,
    )
    if call_fwd_dual_asm_build.stderr != "":
        print("expected empty stderr for call-forward-dual-helpers assembly dump build")
        print(call_fwd_dual_asm_build.stderr)
        return 1
    call_fwd_dual_asm_text = call_fwd_dual_asm.read_text(encoding="utf-8")
    if call_fwd_dual_asm_text.count("call rust_fn_") < 3:
        print("runtime assembly shape mismatch for forward dual helper calls in one expression")
        print(call_fwd_dual_asm_text)
        return 1

    print("[rust] backend bridge runtime call param local then call assembly shape check")
    call_pltc_asm = repo / "tests/rust/out-runtime-call-param-local-then-call.s"
    call_pltc_asm_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-call-param-local-then-call.rs", "--rust-backend-v1", "-o", str(call_pltc_asm)],
        cwd=repo,
        check=True,
    )
    if call_pltc_asm_build.stderr != "":
        print("expected empty stderr for call-param-local-then-call assembly dump build")
        print(call_pltc_asm_build.stderr)
        return 1
    call_pltc_asm_text = call_pltc_asm.read_text(encoding="utf-8")
    if (call_pltc_asm_text.count("call rust_fn_") < 2 or
            "(%rbp)" not in call_pltc_asm_text):
        print("runtime assembly shape mismatch for param callee let then call (frame + call)")
        print(call_pltc_asm_text)
        return 1

    print("[rust] backend bridge runtime call forward triple helpers assembly shape check")
    call_ftriple_asm = repo / "tests/rust/out-runtime-call-forward-triple-helpers.s"
    call_ftriple_asm_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-call-forward-triple-helpers.rs", "--rust-backend-v1", "-o", str(call_ftriple_asm)],
        cwd=repo,
        check=True,
    )
    if call_ftriple_asm_build.stderr != "":
        print("expected empty stderr for call-forward-triple-helpers assembly dump build")
        print(call_ftriple_asm_build.stderr)
        return 1
    call_ftriple_asm_text = call_ftriple_asm.read_text(encoding="utf-8")
    if call_ftriple_asm_text.count("call rust_fn_") < 4:
        print("runtime assembly shape mismatch for main-first triple helper args plus callee")
        print(call_ftriple_asm_text)
        return 1

    print("[rust] backend bridge runtime call param let branch call assembly shape check")
    call_plbc_asm = repo / "tests/rust/out-runtime-call-param-let-branch-call.s"
    call_plbc_asm_build = run(
        [*zcc_cmd, "tests/rust/run-runtime-call-param-let-branch-call.rs", "--rust-backend-v1", "-o", str(call_plbc_asm)],
        cwd=repo,
        check=True,
    )
    if call_plbc_asm_build.stderr != "":
        print("expected empty stderr for call-param-let-branch-call assembly dump build")
        print(call_plbc_asm_build.stderr)
        return 1
    call_plbc_asm_text = call_plbc_asm.read_text(encoding="utf-8")
    if (call_plbc_asm_text.count("call rust_fn_") < 3 or
            "je .Lrust_else_" not in call_plbc_asm_text):
        print("runtime assembly shape mismatch for let then branch with calls per arm")
        print(call_plbc_asm_text)
        return 1

    print("[rust] backend bridge division-by-zero diagnostic")
    backend_div_zero = run(
        [*zcc_cmd, "tests/rust/backend-div-zero.rs", "--rust-backend-v1"],
        cwd=repo,
        check=False,
    )
    if backend_div_zero.returncode == 0:
        print("expected backend-div-zero to fail")
        print(backend_div_zero.stdout)
        print(backend_div_zero.stderr)
        return 1
    if backend_div_zero.stderr != expected_backend_div_zero:
        print("backend-div-zero stderr mismatch")
        print("=== expected ===")
        print(expected_backend_div_zero)
        print("=== actual ===")
        print(backend_div_zero.stderr)
        return 1

    print("[rust] backend bridge unsupported expression statement diagnostic")
    backend_expr_stmt = run(
        [*zcc_cmd, "tests/rust/backend-unsupported-expr-stmt.rs", "--rust-backend-v1"],
        cwd=repo,
        check=False,
    )
    if backend_expr_stmt.returncode == 0:
        print("expected backend-unsupported-expr-stmt to fail")
        print(backend_expr_stmt.stdout)
        print(backend_expr_stmt.stderr)
        return 1
    if backend_expr_stmt.stderr != expected_backend_unsupported_expr_stmt:
        print("backend-unsupported-expr-stmt stderr mismatch")
        print("=== expected ===")
        print(expected_backend_unsupported_expr_stmt)
        print("=== actual ===")
        print(backend_expr_stmt.stderr)
        return 1

    print("[rust] backend bridge recursive call diagnostic")
    backend_call_recursive = run(
        [*zcc_cmd, "tests/rust/backend-call-recursive.rs", "--rust-backend-v1"],
        cwd=repo,
        check=False,
    )
    if backend_call_recursive.returncode == 0:
        print("expected backend-call-recursive to fail")
        print(backend_call_recursive.stdout)
        print(backend_call_recursive.stderr)
        return 1
    if backend_call_recursive.stderr != expected_backend_call_recursive:
        print("backend-call-recursive stderr mismatch")
        print("=== expected ===")
        print(expected_backend_call_recursive)
        print("=== actual ===")
        print(backend_call_recursive.stderr)
        return 1

    print("[rust] backend bridge mutual recursive call diagnostic")
    backend_call_mutual = run(
        [*zcc_cmd, "tests/rust/backend-call-mutual-recursive.rs", "--rust-backend-v1"],
        cwd=repo,
        check=False,
    )
    if backend_call_mutual.returncode == 0:
        print("expected backend-call-mutual-recursive to fail")
        print(backend_call_mutual.stdout)
        print(backend_call_mutual.stderr)
        return 1
    if backend_call_mutual.stderr != expected_backend_call_mutual_recursive:
        print("backend-call-mutual-recursive stderr mismatch")
        print("=== expected ===")
        print(expected_backend_call_mutual_recursive)
        print("=== actual ===")
        print(backend_call_mutual.stderr)
        return 1

    print("[rust] backend bridge recursive param call diagnostic")
    backend_call_param_rec = run(
        [*zcc_cmd, "tests/rust/backend-call-param-recursive.rs", "--rust-backend-v1"],
        cwd=repo,
        check=False,
    )
    if backend_call_param_rec.returncode == 0:
        print("expected backend-call-param-recursive to fail")
        print(backend_call_param_rec.stdout)
        print(backend_call_param_rec.stderr)
        return 1
    if backend_call_param_rec.stderr != expected_backend_call_param_recursive:
        print("backend-call-param-recursive stderr mismatch")
        print("=== expected ===")
        print(expected_backend_call_param_recursive)
        print("=== actual ===")
        print(backend_call_param_rec.stderr)
        return 1

    print("[rust] all checks passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
