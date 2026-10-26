#!/usr/bin/env python3
"""
ZCC-CORE-04: libcurl 8.7.1 Offline Tri-Compiler Differential Integration Suite
Governed under ZCC-EF-7 v1.0.0 (Tier E2).

Acceptance Contract:
- Target Capability: Offline libcurl 8.7.1 APIs (URL API, RFC 3986 escape/unescape, strerror).
- Preprocessing Symmetry: Option (b) — Class III-asymmetric.
  Technical Rationale: Option (a) (feeding identical preprocessed TUs from zcc-libc to GCC/Clang)
  is technically infeasible because GCC and Clang reject zcc-libc's minimalist type definitions
  (__builtin_va_start, time_t, uid_t, gid_t, mode_t, struct timeval) with fatal builtin-conflict
  and redefinition errors. Conversely, ZCC cannot parse full GNU glibc headers.
  Therefore, the campaign formally records Class III-asymmetric:
  * ZCC compiles preprocessed source using zcc_sys_includes + zcc-libc + include/zcc_curl_compat.h.
  * GCC 13.3 and Clang 18.1 compile upstream libcurl sources directly using standard system headers.
  * Both sides share identical configure macros, the identical single curl_config.h, and tests/curl_stubs.c.
  * Any divergence must first be triaged as header-surface vs compiler.
- Full Class III Disclosures (byte-for-byte audit against commit 3b4f9da91, 66-line file):
  * include/zcc_curl_compat.h:
    - Builtin redefinitions (codegen hint alteration):
        #define __builtin_expect(exp, c) (exp)
        #define __builtin_unreachable() do {} while(0)
        #define __builtin_clz(x) __builtin_clz(x)
    - POSIX scalar types:
        socklen_t, pid_t, uid_t, gid_t, mode_t, time_t, suseconds_t, sa_family_t
    - Socket structures & network constants:
        fd_set, struct in_addr, struct sockaddr_in, struct in6_addr, struct sockaddr_in6,
        AF_UNSPEC, AF_INET, AF_INET6, PF_UNSPEC, PF_INET, PF_INET6,
        SOCK_STREAM, SOCK_DGRAM, IPPROTO_IP, IPPROTO_TCP, IPPROTO_UDP
    - Prototypes: inet_ntop, inet_pton, strerror_r
  * tests/curl_stubs.c:
    - 5 allocator pointers:
        Curl_cmalloc, Curl_cfree, Curl_crealloc, Curl_cstrdup, Curl_ccalloc
    - 4 dummy protocol/utility helpers:
        Curl_get_scheme_handler, Curl_is_ASCII_name, ZSTD_versionNumber, Curl_parse_login_details
- Partition & Limitations:
  * zcc_compiled_units: 11 archive units (escape, urlapi, strerror, mprintf, dynbuf, strdup, timediff, warnless, base64, version, strcase)
  * gcc_compiled_shared: core04_curl_driver.c, curl_stubs.c (isolates compiler backend as tested variable)
- Deterministic Consensus Driver: tests/core04_curl_driver.c (commit 3d859aeed)
  * Explicit printf of observed values, no assert/SIGABRT, no telemetry banners.
  * POS-01 (default): Exit 0.
  * NEG-01 (--neg01): Deliberately malformed URL rejected with exit 42.
- Meta-Verification: NEG-02 (Missing-member / stale-symbol link failure control).
- Fail-Closed Rule: Missing compiler or corrupted oracle writes partial failure ledger and aborts with non-zero exit code.
"""

import os
import sys
import time
import json
import shutil
import tempfile
import subprocess
import hashlib
from datetime import datetime, timezone

EVIDENCE_TIER = "E2"
EVIDENCE_TIER_ASSIGNED_AT = "2026-10-26T04:45:00Z"
GOVERNING_RULE_VERSION = "ZCC-EF-7 v1.0.0"
BASELINE_MERGE_SHA = "e16501557"
SHIM_COMMIT_SHA = "3b4f9da91"
DRIVER_COMMIT_SHA = "3d859aeed"

CURL_VERSION = "8.7.1"
COMPILE_TIMEOUT = 120
RUN_TIMEOUT = 30

CURL_SRCS = [
    "escape",
    "urlapi",
    "strerror",
    "mprintf",
    "dynbuf",
    "strdup",
    "timediff",
    "warnless",
    "base64",
    "version",
    "strcase"
]

_GLOBAL_CONTEXT = {
    "repo_root": None,
    "timestamp": None,
    "workdir": None,
    "ledger_path": None,
    "partial_results": []
}

def hash_file(path):
    if not os.path.isfile(path):
        return None
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while chunk := f.read(65536):
            h.update(chunk)
    return h.hexdigest()

def fail_closed(msg, partial_data=None):
    print(f"\n[FAIL-CLOSED FATAL ERROR] {msg}", file=sys.stderr)
    repo_root = _GLOBAL_CONTEXT.get("repo_root")
    run_timestamp = _GLOBAL_CONTEXT.get("timestamp") or datetime.now(timezone.utc).strftime("%Y%m%d_%H%M%S_%f")
    if repo_root:
        fail_evidence_dir = os.path.join(repo_root, "docs", "evidence", "2026-10-26", "core_04_curl")
        os.makedirs(fail_evidence_dir, exist_ok=True)
        fail_ledger_path = os.path.join(fail_evidence_dir, f"core04_ledger_{run_timestamp}_FAIL.json")
        fail_payload = {
            "timestamp": run_timestamp,
            "verdict": "FAIL",
            "error": msg,
            "governing_rule_version": GOVERNING_RULE_VERSION,
            "evidence_tier": EVIDENCE_TIER,
            "partial_data": partial_data or _GLOBAL_CONTEXT.get("partial_results", [])
        }
        try:
            with open(fail_ledger_path, "w") as f:
                json.dump(fail_payload, f, indent=2)
            print(f"[FAIL-CLOSED] Preserved partial failure ledger at: {fail_ledger_path}", file=sys.stderr)
        except Exception as e:
            print(f"[FAIL-CLOSED] Failed to write fail ledger: {e}", file=sys.stderr)
    sys.exit(1)

def ensure_compilers_present():
    compilers = {
        "zcc": "./zcc",
        "gcc": shutil.which("gcc"),
        "clang": shutil.which("clang")
    }
    for name, path in compilers.items():
        if not path or (name == "zcc" and not os.path.isfile(path)):
            fail_closed(f"Required compiler '{name}' not found at '{path}'. Fail-closed active.")
    return compilers

def run_cmd(cmd, cwd=None, timeout=COMPILE_TIMEOUT):
    try:
        res = subprocess.run(
            cmd,
            cwd=cwd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            timeout=timeout
        )
        return {
            "returncode": res.returncode,
            "stdout": res.stdout,
            "stderr": res.stderr,
            "timeout": False
        }
    except subprocess.TimeoutExpired:
        return {
            "returncode": -1,
            "stdout": "",
            "stderr": f"Command timed out after {timeout} seconds",
            "timeout": True
        }

def setup_curl_source():
    curl_dir = f"/tmp/curl-{CURL_VERSION}"
    curl_tar = f"/tmp/curl-{CURL_VERSION}.tar.gz"

    if not os.path.isdir(curl_dir):
        if not os.path.isfile(curl_tar):
            print(f"[FETCH] Downloading libcurl {CURL_VERSION}...")
            dl_res = run_cmd(["curl", "-sSL", f"https://curl.se/download/curl-{CURL_VERSION}.tar.gz", "-o", curl_tar])
            if dl_res["returncode"] != 0:
                fail_closed(f"Failed to download libcurl tarball: {dl_res['stderr']}")
        print(f"[EXTRACT] Extracting libcurl {CURL_VERSION}...")
        ext_res = run_cmd(["tar", "-xzf", curl_tar, "-C", "/tmp"])
        if ext_res["returncode"] != 0:
            fail_closed(f"Failed to extract libcurl tarball: {ext_res['stderr']}")

    config_h = os.path.join(curl_dir, "lib", "curl_config.h")
    if not os.path.isfile(config_h):
        print("[CONFIGURE] Generating single authoritative curl_config.h via configure...")
        cfg_cmd = [
            "./configure",
            "--disable-shared",
            "--enable-static",
            "--without-ssl",
            "--without-zlib",
            "--without-zstd",
            "--disable-threaded-resolver",
            "--disable-pthreads",
            "--disable-ldap",
            "--disable-ldaps",
            "--disable-rtsp",
            "--disable-dict",
            "--disable-telnet",
            "--disable-tftp",
            "--disable-pop3",
            "--disable-imap",
            "--disable-smb",
            "--disable-smtp",
            "--disable-gopher",
            "--disable-mqtt",
            "--prefix=/tmp/curl_install"
        ]
        cfg_res = run_cmd(cfg_cmd, cwd=curl_dir, timeout=180)
        if cfg_res["returncode"] != 0 or not os.path.isfile(config_h):
            fail_closed(f"Failed to configure libcurl: {cfg_res['stderr']}")

    return curl_dir, config_h

def compile_zcc_archive(repo_root, curl_dir, workdir, zcc_bin):
    """
    ZCC compilation path (Class III-asymmetric):
    Preprocesses with zcc_sys_includes, zcc-libc, and zcc_curl_compat.h,
    then compiles with ZCC and assembles with as/gcc.
    """
    print("\n[ZCC ARCHIVE] Preprocessing & compiling libcurl static archive with ZCC...")
    obj_dir = os.path.join(workdir, "zcc_objs")
    os.makedirs(obj_dir, exist_ok=True)
    objs = []

    for src in CURL_SRCS:
        c_file = os.path.join(curl_dir, "lib", f"{src}.c")
        pp_file = os.path.join(obj_dir, f"{src}_pp.c")
        s_file = os.path.join(obj_dir, f"{src}.s")
        o_file = os.path.join(obj_dir, f"{src}.o")

        pp_cmd = [
            "gcc", "-E", "-P", "-nostdinc",
            "-I", os.path.join(repo_root, "zcc_sys_includes"),
            "-I", os.path.join(repo_root, "zcc-libc"),
            "-I", os.path.join(repo_root, "include"),
            "-include", os.path.join(repo_root, "include", "zcc_curl_compat.h"),
            "-I", os.path.join(curl_dir, "include"),
            "-I", os.path.join(curl_dir, "lib"),
            "-DBUILDING_LIBCURL",
            "-DCURL_STATICLIB",
            "-DHAVE_CONFIG_H",
            c_file, "-o", pp_file
        ]
        r_pp = run_cmd(pp_cmd)
        if r_pp["returncode"] != 0:
            fail_closed(f"ZCC preprocessing of {src}.c failed: {r_pp['stderr']}")

        r_zcc = run_cmd([zcc_bin, pp_file, "-o", s_file])
        if r_zcc["returncode"] != 0:
            fail_closed(f"ZCC compilation of {src} failed: {r_zcc['stderr']}")

        r_as = run_cmd(["gcc", "-c", s_file, "-o", o_file])
        if r_as["returncode"] != 0:
            fail_closed(f"Assembly of {s_file} failed: {r_as['stderr']}")

        objs.append(o_file)

    # Stubs
    stubs_c = os.path.join(repo_root, "tests", "curl_stubs.c")
    stubs_o = os.path.join(obj_dir, "curl_stubs.o")
    r_stubs = run_cmd(["gcc", "-c", "-I", os.path.join(curl_dir, "include"), stubs_c, "-o", stubs_o])
    if r_stubs["returncode"] != 0:
        fail_closed(f"Compiling curl_stubs.c for ZCC archive failed: {r_stubs['stderr']}")
    objs.append(stubs_o)

    lib_a = os.path.join(workdir, "libzcc_curl.a")
    r_ar = run_cmd(["ar", "rcs", lib_a] + objs)
    if r_ar["returncode"] != 0:
        fail_closed(f"ar failed creating {lib_a}: {r_ar['stderr']}")

    return lib_a, obj_dir

def compile_reference_archive(compiler_name, compiler_cmd, repo_root, curl_dir, workdir):
    """
    Reference compiler path (GCC / Clang):
    Compiles upstream lib/*.c directly using native system headers and shared curl_config.h.
    """
    print(f"\n[{compiler_name.upper()} ARCHIVE] Compiling libcurl static archive with {compiler_name}...")
    obj_dir = os.path.join(workdir, f"{compiler_name}_objs")
    os.makedirs(obj_dir, exist_ok=True)
    objs = []

    for src in CURL_SRCS:
        c_file = os.path.join(curl_dir, "lib", f"{src}.c")
        o_file = os.path.join(obj_dir, f"{src}.o")

        cmd = [
            compiler_cmd, "-c",
            "-I", os.path.join(curl_dir, "include"),
            "-I", os.path.join(curl_dir, "lib"),
            "-DBUILDING_LIBCURL",
            "-DCURL_STATICLIB",
            "-DHAVE_CONFIG_H",
            c_file, "-o", o_file
        ]
        r_c = run_cmd(cmd)
        if r_c["returncode"] != 0:
            fail_closed(f"{compiler_name} compilation of {src}.c failed: {r_c['stderr']}")

        objs.append(o_file)

    # Stubs
    stubs_c = os.path.join(repo_root, "tests", "curl_stubs.c")
    stubs_o = os.path.join(obj_dir, "curl_stubs.o")
    r_stubs = run_cmd([compiler_cmd, "-c", "-I", os.path.join(curl_dir, "include"), stubs_c, "-o", stubs_o])
    if r_stubs["returncode"] != 0:
        fail_closed(f"Compiling curl_stubs.c for {compiler_name} failed: {r_stubs['stderr']}")
    objs.append(stubs_o)

    lib_a = os.path.join(workdir, f"lib{compiler_name}_curl.a")
    r_ar = run_cmd(["ar", "rcs", lib_a] + objs)
    if r_ar["returncode"] != 0:
        fail_closed(f"ar failed creating {lib_a}: {r_ar['stderr']}")

    return lib_a, obj_dir

def run_meta_verification_neg02(workdir, zcc_obj_dir, repo_root, curl_dir):
    print("\n--------------------------------------------------------------------------------")
    print("🛡️  META-VERIFICATION: NEG-02 (Missing-Member / Stale-Symbol Link Failure Control)")
    print("--------------------------------------------------------------------------------")
    target_obj = os.path.join(zcc_obj_dir, "urlapi.o")
    corrupt_backup = os.path.join(zcc_obj_dir, "urlapi.o.bak")
    shutil.copyfile(target_obj, corrupt_backup)

    try:
        # Deliberately corrupt urlapi.o in-place with garbage non-ELF bytes
        with open(target_obj, "wb") as f:
            f.write(b"CORRUPTED_ELF_DATA_GARBAGE_PAYLOAD_NOT_AN_OBJECT_FILE_7777777777777777")

        corrupt_lib = os.path.join(workdir, "libcorrupted_curl.a")
        objs = [os.path.join(zcc_obj_dir, f"{src}.o") for src in CURL_SRCS] + [os.path.join(zcc_obj_dir, "curl_stubs.o")]
        r_ar = run_cmd(["ar", "rcs", corrupt_lib] + objs)
        if r_ar["returncode"] != 0:
            fail_closed(f"NEG-02 ar archive creation failed: {r_ar['stderr']}")

        bad_bin = os.path.join(workdir, "bad_link_bin")
        link_cmd = [
            "gcc", "-no-pie",
            "-I", os.path.join(curl_dir, "include"),
            os.path.join(repo_root, "tests", "core04_curl_driver.c"),
            corrupt_lib,
            "-o", bad_bin,
            "-lm"
        ]
        r_link = run_cmd(link_cmd)
        if r_link["returncode"] != 0:
            print("  ✔ NEG-02 PASS: Linker rejected incomplete archive (non-ELF member skipped) with non-zero exit code.")
            print(f"    Observed diagnostic: {r_link['stderr'].splitlines()[0] if r_link['stderr'] else 'non-zero exit'}")
            return True, r_link
        else:
            print("  ❌ NEG-02 FAIL: Linker silently accepted corrupted object payload! Fail-closed triggered.")
            return False, r_link
    finally:
        shutil.copyfile(corrupt_backup, target_obj)
        os.remove(corrupt_backup)

def main():
    print("================================================================================")
    print("🚀 ZCC-CORE-04: LIBCURL 8.7.1 OFFLINE TRI-COMPILER DIFFERENTIAL HARNESS")
    print(f"   Governing Rule: {GOVERNING_RULE_VERSION} | Assigned Tier: {EVIDENCE_TIER}")
    print(f"   Tier Timestamp: {EVIDENCE_TIER_ASSIGNED_AT}")
    print(f"   Baseline Merge SHA: {BASELINE_MERGE_SHA} | Shims SHA: {SHIM_COMMIT_SHA} | Driver SHA: {DRIVER_COMMIT_SHA}")
    print("================================================================================")

    repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    run_timestamp = datetime.now(timezone.utc).strftime("%Y%m%d_%H%M%S_%f")
    workdir = tempfile.mkdtemp(prefix=f"zcc_core04_curl_{run_timestamp}_")

    _GLOBAL_CONTEXT["repo_root"] = repo_root
    _GLOBAL_CONTEXT["timestamp"] = run_timestamp
    _GLOBAL_CONTEXT["workdir"] = workdir

    compilers = ensure_compilers_present()
    print("✔ Tri-compiler toolchain verified present:")
    print(f"  ZCC:   {compilers['zcc']}")
    print(f"  GCC:   {compilers['gcc']}")
    print(f"  Clang: {compilers['clang']}")

    print(f"\n✔ Isolated workspace sandboxed: {workdir}")

    curl_dir, config_h = setup_curl_source()
    config_h_sha = hash_file(config_h)
    print(f"✔ Shared single curl_config.h SHA-256: {config_h_sha}")

    # Build static archives
    libzcc_a, zcc_obj_dir = compile_zcc_archive(repo_root, curl_dir, workdir, compilers["zcc"])
    libgcc_a, _ = compile_reference_archive("gcc", compilers["gcc"], repo_root, curl_dir, workdir)
    libclang_a, _ = compile_reference_archive("clang", compilers["clang"], repo_root, curl_dir, workdir)

    archives = {
        "zcc": libzcc_a,
        "gcc": libgcc_a,
        "clang": libclang_a
    }

    driver_c = os.path.join(repo_root, "tests", "core04_curl_driver.c")
    results = []

    # --- SUITE 4A: POS-01 (Offline URL API & RFC 3986 Gauntlet) ---
    print("\n--------------------------------------------------------------------------------")
    print("[SUITE 4A / POS-01] URL API & RFC 3986 Escape Gauntlet (core04_curl_driver.c)")
    print("--------------------------------------------------------------------------------")
    outputs_4a = {}
    for comp in ["zcc", "gcc", "clang"]:
        bin_path = os.path.join(workdir, f"bin_4a_{comp}")
        link_cmd = [
            "gcc", "-no-pie",
            "-I", os.path.join(curl_dir, "include"),
            driver_c,
            archives[comp],
            "-o", bin_path,
            "-lm"
        ]
        r_link = run_cmd(link_cmd)
        if r_link["returncode"] != 0:
            fail_closed(f"Failed to link Suite 4A for {comp}: {r_link['stderr']}")
        r_run = run_cmd([bin_path], timeout=RUN_TIMEOUT)
        outputs_4a[comp] = r_run
        print(f"  {comp.upper():<5} -> Exit: {r_run['returncode']:<3} OutBytes: {len(r_run['stdout']):<5} ErrBytes: {len(r_run['stderr'])}")

    # Check 3-way consensus
    consensus_4a = (
        outputs_4a["zcc"]["returncode"] == 0 and
        outputs_4a["zcc"]["returncode"] == outputs_4a["gcc"]["returncode"] == outputs_4a["clang"]["returncode"] and
        outputs_4a["zcc"]["stdout"] == outputs_4a["gcc"]["stdout"] == outputs_4a["clang"]["stdout"] and
        outputs_4a["zcc"]["stderr"] == outputs_4a["gcc"]["stderr"] == outputs_4a["clang"]["stderr"]
    )
    if not consensus_4a:
        print("\n--- Output Divergence in Suite 4A ---")
        for comp in ["zcc", "gcc", "clang"]:
            print(f"[{comp.upper()} stdout]:\n{outputs_4a[comp]['stdout']}")
            print(f"[{comp.upper()} stderr]:\n{outputs_4a[comp]['stderr']}")
        fail_closed("Suite 4A failed 3-way consensus check!")
    print("  ✔ SUITE 4A PASS: 100% byte-for-byte tri-compiler consensus across ZCC, GCC, and Clang (Exit 0).")
    res_4a = {
        "id": "CORE-04A",
        "name": "Suite 4A & POS-01 URL API / RFC 3986 Gauntlet",
        "verdict": "PASS",
        "expected_exit": 0,
        "observed_exits": {c: outputs_4a[c]["returncode"] for c in outputs_4a},
        "stdout_sha256": hashlib.sha256(outputs_4a["zcc"]["stdout"].encode()).hexdigest(),
        "stdout_snippet": outputs_4a["zcc"]["stdout"].strip(),
        "details": f"Exit 0, matching stdout ({len(outputs_4a['zcc']['stdout'])}B) across ZCC, GCC, Clang."
    }
    results.append(res_4a)
    _GLOBAL_CONTEXT["partial_results"].append(res_4a)

    # --- SUITE 4B: NEG-01 (Malformed URL Rejection Control) ---
    print("\n--------------------------------------------------------------------------------")
    print("[SUITE 4B / NEG-01] Malformed URL Rejection Control (--neg01)")
    print("--------------------------------------------------------------------------------")
    outputs_4b = {}
    for comp in ["zcc", "gcc", "clang"]:
        bin_path = os.path.join(workdir, f"bin_4a_{comp}") # Reuse linked binary with argument
        r_run = run_cmd([bin_path, "--neg01"], timeout=RUN_TIMEOUT)
        outputs_4b[comp] = r_run
        print(f"  {comp.upper():<5} -> Exit: {r_run['returncode']:<3} OutBytes: {len(r_run['stdout']):<5} ErrBytes: {len(r_run['stderr'])}")

    consensus_4b = (
        outputs_4b["zcc"]["returncode"] == 42 and
        outputs_4b["zcc"]["returncode"] == outputs_4b["gcc"]["returncode"] == outputs_4b["clang"]["returncode"] and
        outputs_4b["zcc"]["stdout"] == outputs_4b["gcc"]["stdout"] == outputs_4b["clang"]["stdout"] and
        outputs_4b["zcc"]["stderr"] == outputs_4b["gcc"]["stderr"] == outputs_4b["clang"]["stderr"]
    )
    if not consensus_4b:
        print("\n--- Output Divergence in Suite 4B / NEG-01 ---")
        for comp in ["zcc", "gcc", "clang"]:
            print(f"[{comp.upper()} exit]: {outputs_4b[comp]['returncode']}")
            print(f"[{comp.upper()} stdout]:\n{outputs_4b[comp]['stdout']}")
            print(f"[{comp.upper()} stderr]:\n{outputs_4b[comp]['stderr']}")
        fail_closed(f"Suite 4B / NEG-01 failed consensus check: exits=({outputs_4b['zcc']['returncode']}, {outputs_4b['gcc']['returncode']}, {outputs_4b['clang']['returncode']})")
    print("  ✔ SUITE 4B / NEG-01 PASS: Malformed URL rejected deterministically with exit code 42 on all three compilers.")
    res_4b = {
        "id": "CORE-04B",
        "name": "Suite 4B / NEG-01 Malformed URL Rejection Control",
        "verdict": "PASS",
        "expected_exit": 42,
        "observed_exits": {c: outputs_4b[c]["returncode"] for c in outputs_4b},
        "stdout_sha256": hashlib.sha256(outputs_4b["zcc"]["stdout"].encode()).hexdigest(),
        "stdout_snippet": outputs_4b["zcc"]["stdout"].strip(),
        "details": "Exit 42 consensus across ZCC, GCC, Clang."
    }
    results.append(res_4b)
    _GLOBAL_CONTEXT["partial_results"].append(res_4b)

    # --- META-VERIFICATION: NEG-02 ---
    neg02_pass, neg02_link_res = run_meta_verification_neg02(workdir, zcc_obj_dir, repo_root, curl_dir)
    if not neg02_pass:
        fail_closed("Meta-verification NEG-02 failed!")
    res_neg02 = {
        "id": "META-NEG-02",
        "name": "Missing-member / stale-symbol link failure control",
        "verdict": "PASS",
        "details": "Incomplete archive (non-ELF member urlapi.o skipped by ld) rejected with non-zero exit code due to unresolved symbols."
    }
    results.append(res_neg02)
    _GLOBAL_CONTEXT["partial_results"].append(res_neg02)

    # 4. Ledger Persistence under docs/evidence/2026-10-26/core_04_curl/
    evidence_dir = os.path.join(repo_root, "docs", "evidence", "2026-10-26", "core_04_curl")
    os.makedirs(evidence_dir, exist_ok=True)
    ledger_path = os.path.join(evidence_dir, f"core04_ledger_{run_timestamp}.json")

    provenance = {
        "governing_rule_version": GOVERNING_RULE_VERSION,
        "evidence_tier": EVIDENCE_TIER,
        "evidence_tier_assigned_at": EVIDENCE_TIER_ASSIGNED_AT,
        "observed_by": "agent",
        "architect_class": "REPORTED",
        "baseline_merge_sha": BASELINE_MERGE_SHA,
        "shim_commit_sha": SHIM_COMMIT_SHA,
        "driver_commit_sha": DRIVER_COMMIT_SHA,
        "curl_config_h_sha256": config_h_sha,
        "preprocessing_symmetry": "Option (b): Class III-asymmetric (ZCC via zcc_sys_includes+zcc-libc; GCC/Clang via glibc headers; shared curl_config.h and tests/curl_stubs.c)",
        "class_iii_disclosures": {
            "include_zcc_curl_compat_h": {
                "builtin_redefinitions": [
                    "__builtin_expect(exp, c) -> (exp)",
                    "__builtin_unreachable() -> do {} while(0)",
                    "__builtin_clz(x) -> __builtin_clz(x)"
                ],
                "posix_types": [
                    "socklen_t", "pid_t", "uid_t", "gid_t", "mode_t", "time_t", "suseconds_t", "sa_family_t", "fd_set"
                ],
                "network_types_and_constants": [
                    "struct in_addr", "struct sockaddr_in", "struct in6_addr", "struct sockaddr_in6",
                    "AF_UNSPEC", "AF_INET", "AF_INET6", "PF_UNSPEC", "PF_INET", "PF_INET6",
                    "SOCK_STREAM", "SOCK_DGRAM", "IPPROTO_IP", "IPPROTO_TCP", "IPPROTO_UDP"
                ],
                "prototypes": ["inet_ntop", "inet_pton", "strerror_r"]
            },
            "tests_curl_stubs_c": {
                "allocator_pointers": [
                    "Curl_cmalloc", "Curl_cfree", "Curl_crealloc", "Curl_cstrdup", "Curl_ccalloc"
                ],
                "protocol_and_utility_stubs": [
                    "Curl_get_scheme_handler", "Curl_is_ASCII_name", "ZSTD_versionNumber", "Curl_parse_login_details"
                ]
            }
        },
        "scope_partition": {
            "zcc_compiled_units": CURL_SRCS,
            "gcc_compiled_shared": ["core04_curl_driver.c", "curl_stubs.c"]
        },
        "limitations": [
            "tests/core04_curl_driver.c and tests/curl_stubs.c are gcc-compiled in all three variants to isolate the 11 archive units as the tested variable.",
            "__builtin_expect and __builtin_unreachable are neutered on the ZCC side only via include/zcc_curl_compat.h.",
            "Single fixture per suite (POS-01 and NEG-01); establishes differential consensus for the tested offline units, not a general libcurl or network conformance claim."
        ],
        "compilers": {
            "zcc": {
                "path": compilers["zcc"],
                "sha256": hash_file(compilers["zcc"])
            },
            "gcc": {
                "path": compilers["gcc"],
                "version": run_cmd(["gcc", "--version"])["stdout"].splitlines()[0]
            },
            "clang": {
                "path": compilers["clang"],
                "version": run_cmd(["clang", "--version"])["stdout"].splitlines()[0]
            }
        },
        "compat_header": {
            "path": "include/zcc_curl_compat.h",
            "sha256": hash_file(os.path.join(repo_root, "include", "zcc_curl_compat.h"))
        },
        "stubs_source": {
            "path": "tests/curl_stubs.c",
            "sha256": hash_file(os.path.join(repo_root, "tests", "curl_stubs.c"))
        },
        "driver_source": {
            "path": "tests/core04_curl_driver.c",
            "sha256": hash_file(os.path.join(repo_root, "tests", "core04_curl_driver.c"))
        },
        "smoke_reference": {
            "path": "tests/test_curl_smoke.c",
            "sha256": hash_file(os.path.join(repo_root, "tests", "test_curl_smoke.c"))
        }
    }

    raw_execution_tuples = {
        "suite_4a_pos01": {
            c: {
                "exit": outputs_4a[c]["returncode"],
                "stdout": outputs_4a[c]["stdout"],
                "stderr": outputs_4a[c]["stderr"]
            } for c in ["zcc", "gcc", "clang"]
        },
        "suite_4b_neg01": {
            c: {
                "exit": outputs_4b[c]["returncode"],
                "stdout": outputs_4b[c]["stdout"],
                "stderr": outputs_4b[c]["stderr"]
            } for c in ["zcc", "gcc", "clang"]
        },
        "meta_neg02": {
            "exit": neg02_link_res["returncode"],
            "stdout": neg02_link_res["stdout"],
            "stderr": neg02_link_res["stderr"]
        }
    }

    ledger_data = {
        "timestamp": run_timestamp,
        "workspace": workdir,
        "provenance": provenance,
        "results": results,
        "raw_execution_tuples": raw_execution_tuples
    }

    with open(ledger_path, "w") as f:
        json.dump(ledger_data, f, indent=2)

    print("\n================================================================================")
    print("📊 ZCC-CORE-04 EVIDENCE LEDGER SEALED")
    print(f"   Ledger: {ledger_path}")
    print("================================================================================")

    # Output JSON directly to stdout for immediate turn inspection
    print("\n--- BEGIN LEDGER JSON ---")
    print(json.dumps(ledger_data, indent=2))
    print("--- END LEDGER JSON ---")

    # Cleanup temp workspace
    shutil.rmtree(workdir)

if __name__ == "__main__":
    main()
