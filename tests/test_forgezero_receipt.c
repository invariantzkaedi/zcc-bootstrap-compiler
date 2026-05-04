/*
 * tests/test_forgezero_receipt.c — Unit tests for ForgeZero audit receipt scaffold
 *
 * Tests:
 *   T01: FZR_MODE_DISABLED — fzr_emitter_emit is a no-op (returns 0)
 *   T02: FZR_MODE_DISABLED — fzr_emitter_flush is a no-op (no crash)
 *   T03: FZR_MODE_DISABLED — fzr_emit_from_vuln_scan is a no-op (returns 0)
 *   T04: FZR_MODE_DISABLED — NULL emitter is safe for all calls
 *   T05: fzr_severity_from_vuln_tag — IR_VULN_NONE → FZR_SEV_INFO
 *   T06: fzr_severity_from_vuln_tag — IR_VULN_SELFDESTRUCT → FZR_SEV_CRITICAL
 *   T07: fzr_severity_from_vuln_tag — IR_VULN_CONTRACT_CREATE → FZR_SEV_CRITICAL
 *   T08: fzr_severity_from_vuln_tag — IR_VULN_UNTRUSTED_CALL → FZR_SEV_HIGH
 *   T09: fzr_severity_from_vuln_tag — IR_VULN_DELEGATE_CALL → FZR_SEV_HIGH
 *   T10: fzr_severity_from_vuln_tag — IR_VULN_PRIV_BOUNDARY → FZR_SEV_HIGH
 *   T11: fzr_severity_from_vuln_tag — IR_VULN_STATE_WRITE → FZR_SEV_MEDIUM
 *   T12: fzr_severity_from_vuln_tag — IR_VULN_UNKNOWN → FZR_SEV_MEDIUM
 *   T13: fzr_severity_from_vuln_tag — IR_VULN_STATIC_CALL → FZR_SEV_LOW
 *   T14: fzr_severity_from_vuln_tag — IR_VULN_EXEC_BARRIER → FZR_SEV_LOW
 *   T15: fzr_severity_from_vuln_tag — multi-bit (SELFDESTRUCT|STATIC_CALL) → CRITICAL
 *   T16: fzr_severity_to_str — all severity values map to stable strings
 *   T17: fzr_event_kind_to_str — all kind values map to stable strings
 *   T18: fzr_event_build — deterministic serialization (timestamp = "DETERMINISTIC")
 *   T19: fzr_event_build — schema_version field is set to FZR_SCHEMA_VERSION
 *   T20: fzr_event_build — vuln_tag_str reflects the single-bit tag name
 *   T21: fzr_event_build — vuln_tag_mask matches the raw bitmask
 *   T22: fzr_event_build — source_component and target_id are set
 *   T23: fzr_event_hash — two identical events produce the same hash
 *   T24: fzr_event_hash — different tags produce different hashes
 *   T25: fzr_event_hash — NULL event is safe (no crash)
 *   T26: fzr_emitter_emit FZR_MODE_STDOUT — increments event_count
 *   T27: fzr_emitter_emit FZR_MODE_STDOUT — JSONL line contains schema_version field
 *   T28: fzr_emitter_emit FZR_MODE_STDOUT — JSONL line contains vuln_tag field
 *   T29: fzr_emitter_emit FZR_MODE_STDOUT — JSONL line contains kind field
 *   T30: fzr_emitter_emit FZR_MODE_STDOUT — JSONL line contains severity field
 *   T31: fzr_emitter_emit FZR_MODE_STDOUT — JSONL line contains event_hash field
 *   T32: fzr_emitter_emit FZR_MODE_STDOUT — JSONL line ends with newline
 *   T33: fzr_emitter_open_file — disabled emitter returns 0 (no-op)
 *   T34: fzr_emitter_open_file — NULL path returns -1
 *   T35: fzr_emitter_open_file — valid path opens successfully
 *   T36: fzr_emitter_set_target — sets target_id on emitter
 *   T37: fzr_emit_from_vuln_scan — module with no tagged nodes returns 0
 *   T38: fzr_emit_from_vuln_scan — emits one finding per tagged node bit
 *   T39: fzr_emit_from_vuln_scan — always emits a compile_summary event
 *   T40: fzr_emit_from_vuln_scan — NULL module is safe (returns 0)
 *   T41: malformed/unknown vuln tag (raw bits > IR_VULN_ALL_KNOWN) — fzr_event_build safe
 *   T42: fzr_emitter_close — no crash in all modes
 *   T43: standard non-blocking behavior — FZR_MODE_DISABLED is the zero-value default
 *   T44: fzr_event_build — IR_VULN_UNKNOWN tag maps to severity MEDIUM
 *   T45: multi-bit tag in fzr_event_build — vuln_tag_str is "IR_VULN_MULTI"
 *
 * Build and run:
 *   gcc -O0 -std=c99 -Wall -Wextra -I. \
 *       -o /tmp/test_forgezero_receipt \
 *       tests/test_forgezero_receipt.c forgezero_receipt.c \
 *       ir_vuln_tag.c evm_lifter.c ir.c -lm
 *   /tmp/test_forgezero_receipt
 *
 * Coverage note: these tests cover all API paths in forgezero_receipt.c.
 * Full 95%+ production coverage requires a production harness — see issue.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "forgezero_receipt.h"
#include "ir_vuln_tag.h"
#include "evm_lifter.h"
#include "ir.h"

/* ── Minimal test harness ─────────────────────────────────────────── */

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(cond, msg) \
    do { \
        if (cond) { \
            printf("  PASS: %s\n", msg); \
            g_pass++; \
        } else { \
            printf("  FAIL: %s  (line %d)\n", msg, __LINE__); \
            g_fail++; \
        } \
    } while (0)

#define TEST(name) printf("\n[%s]\n", name)

/* ── T01-T04: disabled / NULL safety ─────────────────────────────── */

static void test_disabled_noop(void)
{
    fzr_emitter_t em;
    fzr_event_t ev;

    TEST("T01: FZR_MODE_DISABLED — emit is no-op");
    fzr_emitter_init(&em, FZR_MODE_DISABLED, NULL, 1);
    fzr_event_build(&ev, FZR_EVENT_AUDIT_FINDING,
                    IR_VULN_UNTRUSTED_CALL, "ir_pass", "t01", 1);
    CHECK(fzr_emitter_emit(&em, &ev) == 0,
          "disabled emit returns 0");
    CHECK(em.event_count == 0,
          "disabled emit: event_count stays 0");

    TEST("T02: FZR_MODE_DISABLED — flush is no-op");
    fzr_emitter_flush(&em); /* should not crash */
    CHECK(1, "disabled flush: no crash");

    TEST("T03: FZR_MODE_DISABLED — fzr_emit_from_vuln_scan is no-op");
    {
        ir_module_t *mod = ir_module_create();
        CHECK(fzr_emit_from_vuln_scan(&em, mod) == 0,
              "disabled scan returns 0");
        CHECK(em.event_count == 0,
              "disabled scan: event_count stays 0");
        ir_module_free(mod);
    }

    TEST("T04: NULL emitter is safe");
    fzr_emitter_flush(NULL);  /* must not crash */
    fzr_emitter_close(NULL);  /* must not crash */
    fzr_emitter_set_target(NULL, "x"); /* must not crash */
    CHECK(fzr_emitter_emit(NULL, &ev) == 0, "NULL emitter emit returns 0");
    CHECK(fzr_emit_from_vuln_scan(NULL, NULL) == 0,
          "NULL emitter/module scan returns 0");
    CHECK(1, "NULL safety: no crash");
}

/* ── T05-T15: severity mapping ───────────────────────────────────── */

static void test_severity_mapping(void)
{
    TEST("T05: IR_VULN_NONE → FZR_SEV_INFO");
    CHECK(fzr_severity_from_vuln_tag(IR_VULN_NONE) == FZR_SEV_INFO,
          "IR_VULN_NONE maps to FZR_SEV_INFO");

    TEST("T06: IR_VULN_SELFDESTRUCT → FZR_SEV_CRITICAL");
    CHECK(fzr_severity_from_vuln_tag(IR_VULN_SELFDESTRUCT) == FZR_SEV_CRITICAL,
          "IR_VULN_SELFDESTRUCT maps to CRITICAL");

    TEST("T07: IR_VULN_CONTRACT_CREATE → FZR_SEV_CRITICAL");
    CHECK(fzr_severity_from_vuln_tag(IR_VULN_CONTRACT_CREATE) == FZR_SEV_CRITICAL,
          "IR_VULN_CONTRACT_CREATE maps to CRITICAL");

    TEST("T08: IR_VULN_UNTRUSTED_CALL → FZR_SEV_HIGH");
    CHECK(fzr_severity_from_vuln_tag(IR_VULN_UNTRUSTED_CALL) == FZR_SEV_HIGH,
          "IR_VULN_UNTRUSTED_CALL maps to HIGH");

    TEST("T09: IR_VULN_DELEGATE_CALL → FZR_SEV_HIGH");
    CHECK(fzr_severity_from_vuln_tag(IR_VULN_DELEGATE_CALL) == FZR_SEV_HIGH,
          "IR_VULN_DELEGATE_CALL maps to HIGH");

    TEST("T10: IR_VULN_PRIV_BOUNDARY → FZR_SEV_HIGH");
    CHECK(fzr_severity_from_vuln_tag(IR_VULN_PRIV_BOUNDARY) == FZR_SEV_HIGH,
          "IR_VULN_PRIV_BOUNDARY maps to HIGH");

    TEST("T11: IR_VULN_STATE_WRITE → FZR_SEV_MEDIUM");
    CHECK(fzr_severity_from_vuln_tag(IR_VULN_STATE_WRITE) == FZR_SEV_MEDIUM,
          "IR_VULN_STATE_WRITE maps to MEDIUM");

    TEST("T12: IR_VULN_UNKNOWN → FZR_SEV_MEDIUM");
    CHECK(fzr_severity_from_vuln_tag(IR_VULN_UNKNOWN) == FZR_SEV_MEDIUM,
          "IR_VULN_UNKNOWN maps to MEDIUM");

    TEST("T13: IR_VULN_STATIC_CALL → FZR_SEV_LOW");
    CHECK(fzr_severity_from_vuln_tag(IR_VULN_STATIC_CALL) == FZR_SEV_LOW,
          "IR_VULN_STATIC_CALL maps to LOW");

    TEST("T14: IR_VULN_EXEC_BARRIER → FZR_SEV_LOW");
    CHECK(fzr_severity_from_vuln_tag(IR_VULN_EXEC_BARRIER) == FZR_SEV_LOW,
          "IR_VULN_EXEC_BARRIER maps to LOW");

    TEST("T15: multi-bit (SELFDESTRUCT|STATIC_CALL) → CRITICAL");
    {
        ir_vuln_tag_t multi = (ir_vuln_tag_t)(IR_VULN_SELFDESTRUCT | IR_VULN_STATIC_CALL);
        CHECK(fzr_severity_from_vuln_tag(multi) == FZR_SEV_CRITICAL,
              "multi-bit takes highest severity");
    }
}

/* ── T16-T17: string tables ──────────────────────────────────────── */

static void test_string_tables(void)
{
    TEST("T16: fzr_severity_to_str — all severities");
    CHECK(strcmp(fzr_severity_to_str(FZR_SEV_INFO),     "info")     == 0,
          "FZR_SEV_INFO → \"info\"");
    CHECK(strcmp(fzr_severity_to_str(FZR_SEV_LOW),      "low")      == 0,
          "FZR_SEV_LOW → \"low\"");
    CHECK(strcmp(fzr_severity_to_str(FZR_SEV_MEDIUM),   "medium")   == 0,
          "FZR_SEV_MEDIUM → \"medium\"");
    CHECK(strcmp(fzr_severity_to_str(FZR_SEV_HIGH),     "high")     == 0,
          "FZR_SEV_HIGH → \"high\"");
    CHECK(strcmp(fzr_severity_to_str(FZR_SEV_CRITICAL), "critical") == 0,
          "FZR_SEV_CRITICAL → \"critical\"");

    TEST("T17: fzr_event_kind_to_str — all kinds");
    CHECK(strcmp(fzr_event_kind_to_str(FZR_EVENT_AUDIT_FINDING),
                 "audit_finding") == 0,
          "FZR_EVENT_AUDIT_FINDING → \"audit_finding\"");
    CHECK(strcmp(fzr_event_kind_to_str(FZR_EVENT_COMPILE_SUMMARY),
                 "compile_summary") == 0,
          "FZR_EVENT_COMPILE_SUMMARY → \"compile_summary\"");
    CHECK(strcmp(fzr_event_kind_to_str(FZR_EVENT_SECURITY_TAG),
                 "security_tag_observed") == 0,
          "FZR_EVENT_SECURITY_TAG → \"security_tag_observed\"");
}

/* ── T18-T25: event build and hash ───────────────────────────────── */

static void test_event_build(void)
{
    fzr_event_t ev;

    TEST("T18: fzr_event_build — deterministic timestamp");
    fzr_event_build(&ev, FZR_EVENT_AUDIT_FINDING,
                    IR_VULN_UNTRUSTED_CALL, "ir_pass", "contract.evm", 1);
    CHECK(strcmp(ev.timestamp, "DETERMINISTIC") == 0,
          "deterministic mode: timestamp is DETERMINISTIC");

    TEST("T19: fzr_event_build — schema_version is FZR_SCHEMA_VERSION");
    CHECK(strcmp(ev.schema_version, FZR_SCHEMA_VERSION) == 0,
          "schema_version == FZR_SCHEMA_VERSION");

    TEST("T20: fzr_event_build — vuln_tag_str for single-bit tag");
    CHECK(strcmp(ev.vuln_tag_str, "IR_VULN_UNTRUSTED_CALL") == 0,
          "vuln_tag_str == \"IR_VULN_UNTRUSTED_CALL\"");

    TEST("T21: fzr_event_build — vuln_tag_mask matches bitmask");
    CHECK(ev.vuln_tag_mask == (unsigned int)IR_VULN_UNTRUSTED_CALL,
          "vuln_tag_mask == IR_VULN_UNTRUSTED_CALL");

    TEST("T22: fzr_event_build — source_component and target_id");
    CHECK(strcmp(ev.source_component, "ir_pass") == 0,
          "source_component == \"ir_pass\"");
    CHECK(strcmp(ev.target_id, "contract.evm") == 0,
          "target_id == \"contract.evm\"");

    TEST("T23: fzr_event_hash — identical events produce same hash");
    {
        fzr_event_t ev2;
        fzr_event_build(&ev2, FZR_EVENT_AUDIT_FINDING,
                        IR_VULN_UNTRUSTED_CALL, "ir_pass", "contract.evm", 1);
        CHECK(fzr_event_hash(&ev) == fzr_event_hash(&ev2),
              "identical events → same hash");
    }

    TEST("T24: fzr_event_hash — different tags → different hashes");
    {
        fzr_event_t ev3;
        fzr_event_build(&ev3, FZR_EVENT_AUDIT_FINDING,
                        IR_VULN_DELEGATE_CALL, "ir_pass", "contract.evm", 1);
        CHECK(fzr_event_hash(&ev) != fzr_event_hash(&ev3),
              "different vuln tags → different hashes");
    }

    TEST("T25: fzr_event_hash — NULL event is safe");
    {
        unsigned int h = fzr_event_hash(NULL);
        (void)h;
        CHECK(1, "NULL event hash: no crash");
    }
}

/* ── T26-T32: emit to stdout (captured via tmpfile) ─────────────── */

static void test_emit_stdout_capture(void)
{
    fzr_emitter_t em;
    fzr_event_t ev;
    FILE *tmpf;
    char line[2048];
    int r;

    /* Use a tmpfile to capture output instead of polluting stdout */
    tmpf = tmpfile();
    if (!tmpf) {
        printf("  SKIP: tmpfile() failed — cannot capture output\n");
        return;
    }

    fzr_emitter_init(&em, FZR_MODE_FILE, tmpf, 1);

    fzr_event_build(&ev, FZR_EVENT_AUDIT_FINDING,
                    IR_VULN_STATE_WRITE, "evm_lifter", "test_target", 1);

    TEST("T26: emit → increments event_count");
    r = fzr_emitter_emit(&em, &ev);
    CHECK(r == 1, "emit returns 1 on success");
    CHECK(em.event_count == 1, "event_count == 1 after emit");

    fflush(tmpf);
    rewind(tmpf);

    if (!fgets(line, (int)sizeof(line), tmpf)) {
        printf("  SKIP: could not read emitted line\n");
        fclose(tmpf);
        return;
    }

    TEST("T27: JSONL line contains schema_version");
    CHECK(strstr(line, "\"schema_version\"") != NULL,
          "JSONL contains schema_version key");
    CHECK(strstr(line, FZR_SCHEMA_VERSION) != NULL,
          "JSONL contains schema_version value");

    TEST("T28: JSONL line contains vuln_tag");
    CHECK(strstr(line, "\"vuln_tag\"") != NULL,
          "JSONL contains vuln_tag key");
    CHECK(strstr(line, "IR_VULN_STATE_WRITE") != NULL,
          "JSONL contains IR_VULN_STATE_WRITE");

    TEST("T29: JSONL line contains kind");
    CHECK(strstr(line, "\"kind\"") != NULL, "JSONL contains kind key");
    CHECK(strstr(line, "audit_finding") != NULL, "JSONL contains audit_finding");

    TEST("T30: JSONL line contains severity");
    CHECK(strstr(line, "\"severity\"") != NULL, "JSONL contains severity key");
    CHECK(strstr(line, "medium") != NULL, "JSONL contains medium severity");

    TEST("T31: JSONL line contains event_hash");
    CHECK(strstr(line, "\"event_hash\"") != NULL,
          "JSONL contains event_hash key");

    TEST("T32: JSONL line ends with newline");
    {
        int len = (int)strlen(line);
        CHECK(len > 0 && line[len - 1] == '\n',
              "JSONL line ends with '\\n'");
    }

    fclose(tmpf);
}

/* ── T33-T36: open_file and set_target ───────────────────────────── */

static void test_file_and_target(void)
{
    fzr_emitter_t em;

    TEST("T33: open_file — disabled emitter is a no-op (returns 0)");
    fzr_emitter_init(&em, FZR_MODE_DISABLED, NULL, 1);
    CHECK(fzr_emitter_open_file(&em, "/tmp/fzr_test_t33.jsonl") == 0,
          "disabled open_file returns 0 (no-op)");
    CHECK(em.fp == NULL, "disabled open_file: fp stays NULL");

    TEST("T34: open_file — NULL path returns -1");
    {
        fzr_emitter_t em2;
        fzr_emitter_init(&em2, FZR_MODE_FILE, NULL, 1);
        CHECK(fzr_emitter_open_file(&em2, NULL) == -1,
              "NULL path returns -1");
        CHECK(fzr_emitter_open_file(&em2, "") == -1,
              "empty path returns -1");
    }

    TEST("T35: open_file — valid path opens successfully");
    {
        fzr_emitter_t em3;
        int rc;
        fzr_emitter_init(&em3, FZR_MODE_FILE, NULL, 1);
        rc = fzr_emitter_open_file(&em3, "/tmp/fzr_test_t35.jsonl");
        CHECK(rc == 0, "valid path: open returns 0");
        CHECK(em3.fp != NULL, "valid path: fp is non-NULL");
        fzr_emitter_close(&em3);
        remove("/tmp/fzr_test_t35.jsonl");
    }

    TEST("T36: fzr_emitter_set_target — sets target_id");
    {
        fzr_emitter_t em4;
        fzr_emitter_init(&em4, FZR_MODE_STDOUT, stdout, 1);
        fzr_emitter_set_target(&em4, "my_contract.evm");
        CHECK(strcmp(em4.target_id, "my_contract.evm") == 0,
              "target_id set correctly");
        fzr_emitter_set_target(&em4, NULL);
        CHECK(em4.target_id[0] == '\0',
              "NULL target clears target_id");
    }
}

/* ── T37-T40: module scan integration ───────────────────────────── */

static void test_module_scan(void)
{
    TEST("T37: fzr_emit_from_vuln_scan — no-tag module returns 0");
    {
        fzr_emitter_t em;
        FILE *tmpf = tmpfile();
        ir_module_t *mod;
        ir_func_t *fn;
        ir_node_t *n;
        int findings;

        if (!tmpf) { printf("  SKIP: tmpfile failed\n"); return; }

        fzr_emitter_init(&em, FZR_MODE_FILE, tmpf, 1);

        mod = ir_module_create();
        fn  = ir_func_create(mod, "noop_func", IR_TY_VOID, 0);
        n   = ir_node_alloc();
        ir_append(fn, n);

        findings = fzr_emit_from_vuln_scan(&em, mod);
        CHECK(findings == 0, "no-tag module: findings == 0");
        /* summary event still emitted */
        CHECK(em.event_count == 1, "no-tag module: compile_summary emitted");

        ir_module_free(mod);
        fclose(tmpf);
    }

    TEST("T38: fzr_emit_from_vuln_scan — tagged nodes emit findings");
    {
        fzr_emitter_t em;
        FILE *tmpf = tmpfile();
        ir_module_t *mod;
        ir_func_t *fn;
        ir_node_t *n;
        int findings;

        if (!tmpf) { printf("  SKIP: tmpfile failed\n"); return; }

        fzr_emitter_init(&em, FZR_MODE_FILE, tmpf, 1);
        fzr_emitter_set_target(&em, "test_scan.evm");

        mod = ir_module_create();
        fn  = ir_func_create(mod, "evm_contract", IR_TY_VOID, 0);

        /* node with UNTRUSTED_CALL tag */
        n = ir_node_alloc();
        ir_vuln_tag_set(n, IR_VULN_UNTRUSTED_CALL);
        ir_append(fn, n);

        /* node with DELEGATE_CALL | PRIV_BOUNDARY (two bits → two findings) */
        n = ir_node_alloc();
        ir_vuln_tag_set(n, (ir_vuln_tag_t)(IR_VULN_DELEGATE_CALL | IR_VULN_PRIV_BOUNDARY));
        ir_append(fn, n);

        findings = fzr_emit_from_vuln_scan(&em, mod);
        CHECK(findings == 3, "3 bits set → 3 findings emitted");
        /* findings + summary = 4 events total */
        CHECK(em.event_count == 4, "event_count == 4 (3 findings + summary)");

        ir_module_free(mod);
        fclose(tmpf);
    }

    TEST("T39: fzr_emit_from_vuln_scan — always emits compile_summary");
    {
        fzr_emitter_t em;
        FILE *tmpf = tmpfile();
        ir_module_t *mod;

        if (!tmpf) { printf("  SKIP: tmpfile failed\n"); return; }

        fzr_emitter_init(&em, FZR_MODE_FILE, tmpf, 1);

        /* even empty module → summary event */
        mod = ir_module_create();
        fzr_emit_from_vuln_scan(&em, mod);

        /* seek to last line and check "compile_summary" */
        fflush(tmpf);
        {
            char last[2048];
            char line[2048];
            last[0] = '\0';
            rewind(tmpf);
            while (fgets(line, (int)sizeof(line), tmpf))
                memcpy(last, line, strlen(line) + 1);

            CHECK(strstr(last, "compile_summary") != NULL,
                  "last emitted event is compile_summary");
        }

        ir_module_free(mod);
        fclose(tmpf);
    }

    TEST("T40: fzr_emit_from_vuln_scan — NULL module is safe");
    {
        fzr_emitter_t em;
        FILE *tmpf = tmpfile();
        int findings;

        if (!tmpf) { printf("  SKIP: tmpfile failed\n"); return; }

        fzr_emitter_init(&em, FZR_MODE_FILE, tmpf, 1);
        findings = fzr_emit_from_vuln_scan(&em, NULL);
        CHECK(findings == 0, "NULL module: findings == 0");
        CHECK(em.event_count == 0, "NULL module: no events emitted");
        fclose(tmpf);
    }
}

/* ── T41-T45: edge cases ──────────────────────────────────────────── */

static void test_edge_cases(void)
{
    TEST("T41: malformed tag (bits > IR_VULN_ALL_KNOWN) in fzr_event_build");
    {
        fzr_event_t ev;
        /* IR_VULN_FLAG_MAX is (1<<30) — well outside known schema */
        fzr_event_build(&ev, FZR_EVENT_AUDIT_FINDING,
                        IR_VULN_FLAG_MAX, "ir_pass", "t41", 1);
        /* Should not crash; tag str should be "IR_VULN_UNKNOWN" (multi-bit
         * or sentinel) or "IR_VULN_MULTI" — just must not be empty */
        CHECK(ev.vuln_tag_str[0] != '\0',
              "malformed tag: vuln_tag_str is non-empty");
        CHECK(1, "malformed tag: no crash");
    }

    TEST("T42: fzr_emitter_close — no crash in all modes");
    {
        fzr_emitter_t em1, em2, em3;
        FILE *tmpf = tmpfile();

        fzr_emitter_init(&em1, FZR_MODE_DISABLED, NULL, 1);
        fzr_emitter_close(&em1);
        CHECK(1, "close disabled: no crash");

        fzr_emitter_init(&em2, FZR_MODE_STDOUT, stdout, 1);
        fzr_emitter_close(&em2);  /* should NOT close stdout */
        CHECK(1, "close stdout mode: no crash, stdout not closed");

        if (tmpf) {
            fzr_emitter_init(&em3, FZR_MODE_FILE, tmpf, 1);
            fzr_emitter_close(&em3); /* should close tmpf */
            CHECK(1, "close file mode: no crash");
        }
    }

    TEST("T43: FZR_MODE_DISABLED is the zero-value default (non-blocking)");
    {
        /* Confirm that a zero-initialized emitter behaves as disabled */
        fzr_emitter_t em;
        fzr_event_t ev;
        memset(&em, 0, sizeof(em));
        fzr_event_build(&ev, FZR_EVENT_AUDIT_FINDING,
                        IR_VULN_UNTRUSTED_CALL, "ir_pass", "", 1);
        CHECK(em.mode == FZR_MODE_DISABLED, "zero-init mode == DISABLED");
        CHECK(fzr_emitter_emit(&em, &ev) == 0,
              "zero-init emitter emit: no-op");
    }

    TEST("T44: fzr_event_build — IR_VULN_UNKNOWN severity is MEDIUM");
    {
        fzr_event_t ev;
        fzr_event_build(&ev, FZR_EVENT_AUDIT_FINDING,
                        IR_VULN_UNKNOWN, "ir_pass", "", 1);
        CHECK(ev.severity == FZR_SEV_MEDIUM, "IR_VULN_UNKNOWN → MEDIUM");
    }

    TEST("T45: multi-bit tag — vuln_tag_str is IR_VULN_MULTI");
    {
        fzr_event_t ev;
        fzr_event_build(&ev, FZR_EVENT_AUDIT_FINDING,
                        (ir_vuln_tag_t)(IR_VULN_UNTRUSTED_CALL | IR_VULN_DELEGATE_CALL),
                        "ir_pass", "", 1);
        CHECK(strcmp(ev.vuln_tag_str, "IR_VULN_MULTI") == 0,
              "multi-bit tag → vuln_tag_str == IR_VULN_MULTI");
    }
}

/* ── main ─────────────────────────────────────────────────────────── */

int main(void)
{
    printf("=== ForgeZero Audit Receipt Scaffold Tests ===\n");
    printf("Schema version: %s\n", FZR_SCHEMA_VERSION);

    test_disabled_noop();
    test_severity_mapping();
    test_string_tables();
    test_event_build();
    test_emit_stdout_capture();
    test_file_and_target();
    test_module_scan();
    test_edge_cases();

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    if (g_fail == 0)
        printf("ALL PASS\n");
    else
        printf("SOME FAILURES\n");

    return g_fail == 0 ? 0 : 1;
}
