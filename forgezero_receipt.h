/*
 * forgezero_receipt.h — ForgeZero Audit Receipt Integration Scaffold
 * ====================================================================
 * PURPOSE: Defensive compiler/security-analysis telemetry scaffold ONLY.
 *   Serializes deterministic audit-receipt events from IR vulnerability
 *   tag findings for offline/local provenance.  This does NOT perform
 *   network submission, remote beaconing, payload deployment, exploit
 *   automation, or credential handling.
 *
 * TRUST BOUNDARY:
 *   The compiler emits structured audit events (JSONL) to a local file
 *   or stdout.  External ForgeZero tooling may consume them later.
 *   Standard compilation (no --audit-export flag) is fully no-op; the
 *   emitter is initialized in FZR_MODE_DISABLED and every emit call
 *   returns immediately without I/O or allocations.
 *
 * SCHEMA VERSION: 1.0
 *
 * INTEGRATION:
 *   - Wire to the compiler CLI via --audit-export / --audit-export-file.
 *   - Sources of findings: ir_vuln_tag.h / ir_vuln_tag.c (IR pass scan),
 *     evm_lifter.c (EVM opcode tagging).
 *   - fzr_emit_from_vuln_scan() bridges ir_pass_vuln_scan() → receipt events.
 *
 * USAGE EXAMPLE:
 *   fzr_emitter_t em;
 *   fzr_emitter_init(&em, FZR_MODE_STDOUT, NULL, 0);
 *   fzr_emitter_set_target(&em, "my_contract.evm");
 *   fzr_emit_from_vuln_scan(&em, ir_module);
 *   fzr_emitter_flush(&em);
 *
 * BUILD (standalone test):
 *   gcc -O0 -std=c99 -Wall -Wextra -I. \
 *       -o /tmp/test_forgezero_receipt \
 *       tests/test_forgezero_receipt.c forgezero_receipt.c \
 *       ir_vuln_tag.c evm_lifter.c ir.c -lm
 */

#ifndef ZCC_FORGEZERO_RECEIPT_H
#define ZCC_FORGEZERO_RECEIPT_H

#include <stdio.h>
#include "ir_vuln_tag.h"

/* ── Schema version ────────────────────────────────────────────────── */
#define FZR_SCHEMA_VERSION "1.0"

/* ── Emitter modes ─────────────────────────────────────────────────── */
/*
 * FZR_MODE_DISABLED (default) — all emit calls are no-ops.
 *   Standard compilation always uses this mode unless an explicit
 *   --audit-export CLI flag is provided.
 * FZR_MODE_STDOUT — serialize events as JSONL to stdout.
 * FZR_MODE_FILE   — serialize events as JSONL to a named file.
 */
typedef enum {
    FZR_MODE_DISABLED = 0,
    FZR_MODE_STDOUT   = 1,
    FZR_MODE_FILE     = 2
} fzr_mode_t;

/* ── Event kinds ────────────────────────────────────────────────────── */
typedef enum {
    FZR_EVENT_AUDIT_FINDING   = 0, /* a specific IR vulnerability tag finding */
    FZR_EVENT_COMPILE_SUMMARY = 1, /* end-of-compilation summary              */
    FZR_EVENT_SECURITY_TAG    = 2  /* generic security_tag_observed event      */
} fzr_event_kind_t;

/* ── Severity classification ────────────────────────────────────────── */
typedef enum {
    FZR_SEV_INFO     = 0,
    FZR_SEV_LOW      = 1,
    FZR_SEV_MEDIUM   = 2,
    FZR_SEV_HIGH     = 3,
    FZR_SEV_CRITICAL = 4
} fzr_severity_t;

/* ── Audit receipt event ────────────────────────────────────────────── */
/*
 * All string fields are NUL-terminated fixed-size arrays.
 * event_hash is a deterministic djb2-derived hash of the key fields.
 * timestamp is "DETERMINISTIC" when deterministic mode is active, or
 *   an ISO-8601-like wall-clock string otherwise (YYYY-MM-DDTHH:MM:SSZ).
 */
typedef struct {
    fzr_event_kind_t kind;
    fzr_severity_t   severity;
    char             vuln_tag_str[64];     /* stable tag name, e.g. "IR_VULN_UNTRUSTED_CALL" */
    unsigned int     vuln_tag_mask;        /* raw bitmask from ir_node_t.vuln_tags */
    char             source_component[64]; /* "evm_lifter" | "ir_pass" | "compiler_cli" */
    char             target_id[128];       /* user-supplied target, may be empty */
    unsigned int     event_hash;           /* deterministic hash of key fields */
    char             timestamp[32];        /* ISO-8601 or "DETERMINISTIC" */
    char             schema_version[8];    /* always FZR_SCHEMA_VERSION */
} fzr_event_t;

/* ── Emitter state ──────────────────────────────────────────────────── */
typedef struct {
    fzr_mode_t  mode;              /* disabled / stdout / file */
    FILE       *fp;                /* output stream (NULL if disabled) */
    int         deterministic;     /* 1 = no real timestamps (test mode) */
    int         event_count;       /* total events emitted this session */
    char        target_id[128];    /* default target_id for new events */
} fzr_emitter_t;

/* ── Emitter lifecycle ──────────────────────────────────────────────── */

/*
 * fzr_emitter_init — initialize an audit receipt emitter.
 *   mode:          FZR_MODE_DISABLED / FZR_MODE_STDOUT / FZR_MODE_FILE.
 *   fp:            output FILE* for FZR_MODE_FILE (ignored otherwise);
 *                  caller owns the file; pass NULL to open via
 *                  fzr_emitter_open_file().
 *   deterministic: 1 = suppress real timestamps (for reproducible tests).
 * Always succeeds.  In FZR_MODE_DISABLED, all subsequent calls are no-ops.
 */
void fzr_emitter_init(fzr_emitter_t *em,
                      fzr_mode_t     mode,
                      FILE          *fp,
                      int            deterministic);

/*
 * fzr_emitter_open_file — open a file path for FZR_MODE_FILE.
 *   If the emitter is disabled, this is a no-op (returns 0).
 *   Returns 0 on success, -1 on failure (file cannot be opened).
 *   On success, em->fp is set; caller should call fzr_emitter_close()
 *   when done.
 */
int fzr_emitter_open_file(fzr_emitter_t *em, const char *path);

/*
 * fzr_emitter_set_target — set the default target_id for events.
 *   Typically the input file name or contract identifier.
 *   NULL or empty string clears the target_id.
 */
void fzr_emitter_set_target(fzr_emitter_t *em, const char *target_id);

/*
 * fzr_emitter_emit — serialize a single event as a JSONL line.
 *   No-op when mode is FZR_MODE_DISABLED.
 *   Returns 1 on successful emit, 0 on no-op or error.
 */
int fzr_emitter_emit(fzr_emitter_t *em, const fzr_event_t *ev);

/*
 * fzr_emitter_flush — flush the output stream if open.
 *   No-op when disabled.  Never fails normal compilation.
 */
void fzr_emitter_flush(fzr_emitter_t *em);

/*
 * fzr_emitter_close — flush and close a file opened by fzr_emitter_open_file.
 *   For FZR_MODE_STDOUT, flushes but does not close stdout.
 *   No-op when disabled.
 */
void fzr_emitter_close(fzr_emitter_t *em);

/* ── Event construction helpers ─────────────────────────────────────── */

/*
 * fzr_event_build — construct a receipt event from a single IR vuln tag.
 *   Fills all fields including event_hash and timestamp.
 *   `component`: e.g. "evm_lifter", "ir_pass", "compiler_cli".
 *   `target_id`: may be NULL or empty to inherit from emitter default.
 *   `deterministic`: 1 = use "DETERMINISTIC" timestamp.
 */
void fzr_event_build(fzr_event_t   *ev,
                     fzr_event_kind_t kind,
                     ir_vuln_tag_t  tag,
                     const char    *component,
                     const char    *target_id,
                     int            deterministic);

/*
 * fzr_event_hash — compute deterministic djb2-derived hash of an event.
 *   Hashes kind-string, severity-string, vuln_tag_str, source_component,
 *   and target_id.  Does NOT include the timestamp.
 */
unsigned int fzr_event_hash(const fzr_event_t *ev);

/* ── Classification helpers ─────────────────────────────────────────── */

/*
 * fzr_severity_from_vuln_tag — map a single-bit ir_vuln_tag_t to severity.
 *   CRITICAL: SELFDESTRUCT, CONTRACT_CREATE.
 *   HIGH:     UNTRUSTED_CALL, DELEGATE_CALL, PRIV_BOUNDARY.
 *   MEDIUM:   STATE_WRITE, UNKNOWN.
 *   LOW:      STATIC_CALL, EXEC_BARRIER.
 *   INFO:     NONE.
 *   Multi-bit combinations → highest severity of constituent bits.
 */
fzr_severity_t fzr_severity_from_vuln_tag(ir_vuln_tag_t tag);

/* fzr_severity_to_str — return stable lower-case string for a severity. */
const char *fzr_severity_to_str(fzr_severity_t sev);

/* fzr_event_kind_to_str — return stable string for an event kind. */
const char *fzr_event_kind_to_str(fzr_event_kind_t kind);

/* ── Module scan integration ─────────────────────────────────────────── */

/*
 * fzr_emit_from_vuln_scan — scan an ir_module_t for vulnerability tags
 *   and emit one FZR_EVENT_AUDIT_FINDING receipt event per tagged node.
 *   Also emits a FZR_EVENT_COMPILE_SUMMARY event at the end.
 *   Returns the count of finding events emitted (0 if disabled or no tags).
 *   Does NOT modify the IR; read-only.
 */
int fzr_emit_from_vuln_scan(fzr_emitter_t   *em,
                             const ir_module_t *mod);

#endif /* ZCC_FORGEZERO_RECEIPT_H */
