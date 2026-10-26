# ZCC-CORE-04: libcurl 8.7.1 Offline Tri-Compiler Differential Gate Evidence

## Milestone Identity & Contract Adjudication

| Attribute | Value |
| :--- | :--- |
| **Milestone** | ZCC-CORE-04 — libcurl 8.7.1 Offline Differential Integration |
| **Governing Rule** | ZCC-EF-7 v1.0.0 |
| **Evidence Tier** | E2 (`evidence_tier_assigned_at: 2026-10-26T04:45:00Z`, predates run `06:55:21Z`) |
| **Evidence Class** | OBSERVED (harness, driver, shims, ledger artifacts); REPORTED (execution tuples) |
| **Adjudication Verdict** | **PASS (E2; REPORTED execution; source-inspected harness)** |
| **Adjudication Authority** | Verification Architect |
| **Lifecycle State** | **CLOSED (Bounded per §6; §8 controlled reopening in force)** |
| **Primary Ledger** | `docs/evidence/2026-10-26/core_04_curl/core04_ledger_20261026_065521_762596.json` |
| **Ledger SHA-256** | `039d5cd916edc926d94d0efaf83519f1da5a201a5ec7ae35ba4767c0ffcaebee` |
| **Addendum** | `docs/evidence/2026-10-26/core_04_curl/core04_ledger_20261026_065521_762596.addendum.json` |

---

## 1. Verified Gate Results

### Gate 1 — Self-Host Fixed Point (Mandatory)
```text
cmp zcc2.s zcc3.s
```
- **Exit Code**: `0`
- **Assembly Hashes**:
  - `zcc2.s`: `8196edc7abca56711f743ecacd8f0284`
  - `zcc3.s`: `8196edc7abca56711f743ecacd8f0284`
- **Verdict**: **PASS** (byte-identical self-host fixed point verified).

---

### Gate 2 / Gate 4 — Tri-Compiler Differential Consensus & Target Harness
Run across ZCC (`c76e3cb8...`), GCC 13.3.0, and Clang 18.1.3:

#### Suite 4A / POS-01 (URL API & RFC 3986 Offline Gauntlet)
- **Driver**: `tests/core04_curl_driver.c` (no arguments)
- **Exit Codes**: ZCC: `0`, GCC: `0`, Clang: `0`
- **Stdout SHA-256**: `41f0b10f2757ef3e96da81d743153653d3362af295f710340f5721c53e0de81c`
- **Output Snippet**:
  ```text
  [MODE: POS-01] Executing Deterministic libcurl Offline Gauntlet...
  1. VERSION: present=YES, contains_libcurl=YES
  2. URL_PARTS:
     scheme: https
     host:   zkaedi.prime
     port:   8080
     path:   /quantum/dex
     query:  pool=0
     frag:   resonance
  3. RFC3986: escaped=https%3A%2F%2Fzkaedi.prime%2Fv5%2Fenergy%3Fpool%3D0%26epoch%3D128
     unescaped_match=YES (len=47)
  4. ERROR_MAPPINGS:
     CURLE_OK:                   No error
     CURLE_UNSUPPORTED_PROTOCOL: Unsupported protocol
     CURLE_URL_MALFORMAT:        URL using bad/illegal format or missing URL
  GAUNTLET_STATUS: COMPLETE
  ```
- **Stderr**: `""` across all three compilers.
- **Verdict**: **PASS** (100% byte-for-byte tri-compiler consensus).

#### Suite 4B / NEG-01 (Malformed URL Rejection Control)
- **Driver**: `tests/core04_curl_driver.c --neg01`
- **Exit Codes**: ZCC: `42`, GCC: `42`, Clang: `42`
- **Stdout SHA-256**: `d843f358c50c290186131ac242e9d3b638fa967b356f4c5c5c5138954264434c`
- **Output**:
  ```text
  [MODE: NEG-01] Testing deliberate malformed URL rejection...
  RESULT: curl_url_set failed deterministically as expected (code=27)
  ```
- **Stderr**: `""` across all three compilers.
- **Verdict**: **PASS** (deterministic consensus rejection on code 42).

#### Meta-Verification / NEG-02 (Missing-Member / Stale-Symbol Link Failure Control)
- **Target**: In-place non-ELF corruption of `urlapi.o` inside `libcorrupted_curl.a`.
- **Exit Code**: `1` (linkage failure).
- **Stderr**:
  ```text
  /usr/bin/ld: /tmp/ccO2KoIO.o: in function `main':
  core04_curl_driver.c:(.text+0x1b5): undefined reference to `curl_url'
  core04_curl_driver.c:(.text+0x20a): undefined reference to `curl_url_set'
  core04_curl_driver.c:(.text+0x21c): undefined reference to `curl_url_cleanup'
  collect2: error: ld returned 1 exit status
  ```
- **Verdict**: **PASS** (linker fail-closed on missing symbols when archive member is skipped).

---

## 2. Record Corrections (Addendum Summary)

### D1: Class III Disclosure Correction (`include/zcc_curl_compat.h`)
The initial ledger inadvertently listed types (`struct sockaddr_storage`, `struct addrinfo`, `SOL_SOCKET`, `SO_ERROR`) that were taken from error diagnostic notes rather than the committed file.
The exact contents of `include/zcc_curl_compat.h` (66 lines, SHA-256 `87ea4f81c13374fc6e4f60518baf639fa90c8ab185012fb77d8e8992142690a2`) are:
- Builtin macros: `__builtin_expect`, `__builtin_unreachable`, `__builtin_clz`.
- POSIX types: `socklen_t`, `pid_t`, `uid_t`, `gid_t`, `mode_t`, `time_t`, `suseconds_t`, `sa_family_t`.
- Structures: `fd_set`, `struct in_addr`, `struct sockaddr_in`, `struct in6_addr`, `struct sockaddr_in6`.
- Constants: `AF_UNSPEC`, `AF_INET`, `AF_INET6`, `PF_UNSPEC`, `PF_INET`, `PF_INET6`, `SOCK_STREAM`, `SOCK_DGRAM`, `IPPROTO_IP`, `IPPROTO_TCP`, `IPPROTO_UDP`.
- Prototypes: `inet_ntop`, `inet_pton`, `strerror_r`.

### D2: NEG-02 Mechanism Relabeling
Relabeled from "Corrupted object payload rejected by linker" to **"Missing-member / stale-symbol link failure control"**.
GNU `ar` packages arbitrary files without verifying ELF headers; GNU `ld` inspects member ELF headers and silently skips non-ELF objects, producing undefined reference errors for symbols that were supposed to be in that unit.

### D3: Harness Maintenance & Fail-Closed Artifacts
- `tools/run_core_04_curl_differential.py` updated to write a partial failure ledger before `sys.exit(1)` upon failure.
- `RUN_TIMEOUT` (30s) wired into test execution invocations.
- Suite 4B / NEG-01 compares `stderr` alongside exit code and `stdout`.

### D4: Scope & Limitations
- **ZCC compiled units**: `escape`, `urlapi`, `strerror`, `mprintf`, `dynbuf`, `strdup`, `timediff`, `warnless`, `base64`, `version`, `strcase` (11 archive units).
- **GCC compiled shared units**: `tests/core04_curl_driver.c`, `tests/curl_stubs.c`.
- **Limitations**:
  1. `core04_curl_driver.c` and `curl_stubs.c` are gcc-compiled across all three compiler runs to isolate the 11 archive units as the tested variable.
  2. `__builtin_expect` / `__builtin_unreachable` are neutered on the ZCC side only.
  3. Single fixture per suite; proves offline differential consensus, not network or general conformance.

---

## 3. Milestone Disposition

Per ZCC-EF-7 §6 and §8:
**CORE-04 IS CLOSED AND REMAINS CLOSED.**
Any future work on additional libcurl units, Class II preprocessing unification, or network/socket support is classified as **FUTURE CAPABILITY**, not a milestone reopening.
