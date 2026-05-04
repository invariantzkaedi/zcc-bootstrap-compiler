/*
 * forgezero_receipt.c — ForgeZero Audit Receipt Integration Scaffold
 * ====================================================================
 * DEFENSIVE SECURITY-ANALYSIS SCAFFOLD ONLY.
 * This file implements the helper APIs declared in forgezero_receipt.h.
 * It does NOT perform network submission, remote beaconing, exploit
 * automation, payload deployment, or credential handling.
 *
 * Output format: JSONL — one JSON object per line, newline-terminated.
 * Each line is a self-contained machine-parseable audit receipt event.
 *
 * Build (standalone test):
 *   gcc -O0 -std=c99 -Wall -Wextra -I. \
 *       -o /tmp/test_forgezero_receipt \
 *       tests/test_forgezero_receipt.c forgezero_receipt.c \
 *       ir_vuln_tag.c evm_lifter.c ir.c -lm
 *
 * Coverage note: tests in tests/test_forgezero_receipt.c cover all API
 * paths defined here.  Full 95%+ production coverage requires a production
 * harness — see issue tracker for the gate requirement.
 */

#include "forgezero_receipt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ── Internal helpers ───────────────────────────────────────────────── */

/* safe_strlcpy: copy at most `n-1` bytes, always NUL-terminate. */
static void safe_strlcpy(char *dst, const char *src, int n)
{
    int i = 0;
    if (!dst || n <= 0) return;
    if (!src) { dst[0] = '\0'; return; }
    while (i < n - 1 && src[i]) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

/* (djb2 hash logic is inlined directly in fzr_event_hash below) */



/* ── Severity / kind string tables ──────────────────────────────────── */

const char *fzr_severity_to_str(fzr_severity_t sev)
{
    switch (sev) {
    case FZR_SEV_INFO:     return "info";
    case FZR_SEV_LOW:      return "low";
    case FZR_SEV_MEDIUM:   return "medium";
    case FZR_SEV_HIGH:     return "high";
    case FZR_SEV_CRITICAL: return "critical";
    default:               return "info";
    }
}

const char *fzr_event_kind_to_str(fzr_event_kind_t kind)
{
    switch (kind) {
    case FZR_EVENT_AUDIT_FINDING:   return "audit_finding";
    case FZR_EVENT_COMPILE_SUMMARY: return "compile_summary";
    case FZR_EVENT_SECURITY_TAG:    return "security_tag_observed";
    default:                        return "audit_finding";
    }
}

/* ── Severity mapping from vuln tags ────────────────────────────────── */

fzr_severity_t fzr_severity_from_vuln_tag(ir_vuln_tag_t tag)
{
    fzr_severity_t best = FZR_SEV_INFO;
    unsigned int bits = (unsigned int)tag;
    unsigned int bit;

    /* For multi-bit combinations, return the highest severity among all
     * set bits.  Iterate through each bit individually. */
    if (bits == 0)
        return FZR_SEV_INFO;

    for (bit = 1; bit <= (unsigned int)IR_VULN_ALL_KNOWN; bit <<= 1) {
        fzr_severity_t sev;

        if (!(bits & bit))
            continue;

        switch ((ir_vuln_tag_t)bit) {
        case IR_VULN_SELFDESTRUCT:
        case IR_VULN_CONTRACT_CREATE:
            sev = FZR_SEV_CRITICAL;
            break;
        case IR_VULN_UNTRUSTED_CALL:
        case IR_VULN_DELEGATE_CALL:
        case IR_VULN_PRIV_BOUNDARY:
            sev = FZR_SEV_HIGH;
            break;
        case IR_VULN_STATE_WRITE:
        case IR_VULN_UNKNOWN:
            sev = FZR_SEV_MEDIUM;
            break;
        case IR_VULN_STATIC_CALL:
        case IR_VULN_EXEC_BARRIER:
            sev = FZR_SEV_LOW;
            break;
        case IR_VULN_NONE:
        default:
            sev = FZR_SEV_INFO;
            break;
        }

        if ((int)sev > (int)best)
            best = sev;
    }

    return best;
}

/* ── Event hash ─────────────────────────────────────────────────────── */

unsigned int fzr_event_hash(const fzr_event_t *ev)
{
    unsigned int h = 5381u;
    const char *s;
    int c;

    if (!ev) return h;

    /* Hash: kind_str + severity_str + vuln_tag_str + source_component +
     *       target_id.  Excludes timestamp for deterministic stability. */
    s = fzr_event_kind_to_str(ev->kind);
    while ((c = (unsigned char)*s++) != 0) h = ((h << 5) + h) + (unsigned int)c;

    s = fzr_severity_to_str(ev->severity);
    while ((c = (unsigned char)*s++) != 0) h = ((h << 5) + h) + (unsigned int)c;

    s = ev->vuln_tag_str;
    while ((c = (unsigned char)*s++) != 0) h = ((h << 5) + h) + (unsigned int)c;

    s = ev->source_component;
    while ((c = (unsigned char)*s++) != 0) h = ((h << 5) + h) + (unsigned int)c;

    s = ev->target_id;
    while ((c = (unsigned char)*s++) != 0) h = ((h << 5) + h) + (unsigned int)c;

    return h;
}

/* ── Event construction ─────────────────────────────────────────────── */

void fzr_event_build(fzr_event_t     *ev,
                     fzr_event_kind_t kind,
                     ir_vuln_tag_t    tag,
                     const char      *component,
                     const char      *target_id,
                     int              deterministic)
{
    if (!ev) return;

    memset(ev, 0, sizeof(*ev));

    ev->kind         = kind;
    ev->severity     = fzr_severity_from_vuln_tag(tag);
    ev->vuln_tag_mask = (unsigned int)tag;
    safe_strlcpy(ev->schema_version, FZR_SCHEMA_VERSION,
                 (int)sizeof(ev->schema_version));

    /* vuln_tag_str: for multi-bit masks, use "IR_VULN_MULTI"; otherwise
     * use the canonical stable name. */
    safe_strlcpy(ev->vuln_tag_str, ir_vuln_tag_to_str(tag),
                 (int)sizeof(ev->vuln_tag_str));

    safe_strlcpy(ev->source_component,
                 component ? component : "compiler_cli",
                 (int)sizeof(ev->source_component));

    safe_strlcpy(ev->target_id,
                 target_id ? target_id : "",
                 (int)sizeof(ev->target_id));

    /* timestamp */
    if (deterministic) {
        safe_strlcpy(ev->timestamp, "DETERMINISTIC",
                     (int)sizeof(ev->timestamp));
    } else {
        time_t now = time(NULL);
        struct tm *utc = gmtime(&now);
        if (utc) {
            /* Format: YYYY-MM-DDTHH:MM:SSZ  (25 chars + NUL) */
            int y = utc->tm_year + 1900;
            int mo = utc->tm_mon + 1;
            int d  = utc->tm_mday;
            int h  = utc->tm_hour;
            int mi = utc->tm_min;
            int s  = utc->tm_sec;
            /* manual sprintf to avoid stdint / format-string issues */
            ev->timestamp[0]  = (char)('0' + y / 1000);
            ev->timestamp[1]  = (char)('0' + (y / 100) % 10);
            ev->timestamp[2]  = (char)('0' + (y / 10) % 10);
            ev->timestamp[3]  = (char)('0' + y % 10);
            ev->timestamp[4]  = '-';
            ev->timestamp[5]  = (char)('0' + mo / 10);
            ev->timestamp[6]  = (char)('0' + mo % 10);
            ev->timestamp[7]  = '-';
            ev->timestamp[8]  = (char)('0' + d / 10);
            ev->timestamp[9]  = (char)('0' + d % 10);
            ev->timestamp[10] = 'T';
            ev->timestamp[11] = (char)('0' + h / 10);
            ev->timestamp[12] = (char)('0' + h % 10);
            ev->timestamp[13] = ':';
            ev->timestamp[14] = (char)('0' + mi / 10);
            ev->timestamp[15] = (char)('0' + mi % 10);
            ev->timestamp[16] = ':';
            ev->timestamp[17] = (char)('0' + s / 10);
            ev->timestamp[18] = (char)('0' + s % 10);
            ev->timestamp[19] = 'Z';
            ev->timestamp[20] = '\0';
        } else {
            safe_strlcpy(ev->timestamp, "WALL_CLOCK_ERROR",
                         (int)sizeof(ev->timestamp));
        }
    }

    /* event_hash: computed after all other fields are filled */
    ev->event_hash = fzr_event_hash(ev);
}

/* ── JSON serialization ─────────────────────────────────────────────── */

/*
 * json_escape_str — write a JSON-escaped string (with surrounding quotes)
 * into buf[0..bufsz-1].  Only escapes ", \, and control chars < 0x20.
 * Returns number of bytes written (not counting NUL).
 */
static int json_escape_str(char *buf, int bufsz, const char *s)
{
    int used = 0;
    int c;

    if (!buf || bufsz <= 0) return 0;
    if (!s) s = "";

    if (used < bufsz - 1) buf[used++] = '"';

    while ((c = (unsigned char)*s++) != 0) {
        if (c == '"' || c == '\\') {
            if (used + 2 < bufsz) { buf[used++] = '\\'; buf[used++] = (char)c; }
        } else if (c < 0x20) {
            /* skip control chars (rare in our fields) */
        } else {
            if (used < bufsz - 1) buf[used++] = (char)c;
        }
    }

    if (used < bufsz - 1) buf[used++] = '"';
    buf[used] = '\0';
    return used;
}

/*
 * fzr_event_to_jsonl — serialize ev to JSONL into buf[0..bufsz-1].
 * Returns number of bytes written (not counting NUL terminator).
 * The result does NOT include a trailing newline — caller adds '\n'.
 */
static int fzr_event_to_jsonl(char *buf, int bufsz, const fzr_event_t *ev)
{
    char tmp[256];
    int used = 0;
    int n;

#define APPEND(str) \
    do { \
        int _len = (int)strlen(str); \
        if (used + _len < bufsz) { \
            memcpy(buf + used, str, (size_t)_len); \
            used += _len; \
            buf[used] = '\0'; \
        } \
    } while (0)

    APPEND("{");

    /* schema_version */
    APPEND("\"schema_version\":");
    n = json_escape_str(tmp, (int)sizeof(tmp), ev->schema_version);
    (void)n;
    APPEND(tmp);
    APPEND(",");

    /* kind */
    APPEND("\"kind\":");
    n = json_escape_str(tmp, (int)sizeof(tmp), fzr_event_kind_to_str(ev->kind));
    (void)n;
    APPEND(tmp);
    APPEND(",");

    /* severity */
    APPEND("\"severity\":");
    n = json_escape_str(tmp, (int)sizeof(tmp), fzr_severity_to_str(ev->severity));
    (void)n;
    APPEND(tmp);
    APPEND(",");

    /* vuln_tag_str */
    APPEND("\"vuln_tag\":");
    n = json_escape_str(tmp, (int)sizeof(tmp), ev->vuln_tag_str);
    (void)n;
    APPEND(tmp);
    APPEND(",");

    /* vuln_tag_mask */
    {
        char nbuf[24];
        unsigned int m = ev->vuln_tag_mask;
        int i = 0;
        /* simple uint-to-decimal without sprintf/stdint */
        if (m == 0) {
            nbuf[i++] = '0';
        } else {
            char tmp2[22];
            int j = 0;
            while (m > 0) { tmp2[j++] = (char)('0' + (m % 10)); m /= 10; }
            /* reverse */
            while (j > 0) nbuf[i++] = tmp2[--j];
        }
        nbuf[i] = '\0';
        APPEND("\"vuln_tag_mask\":");
        APPEND(nbuf);
        APPEND(",");
    }

    /* source_component */
    APPEND("\"source_component\":");
    n = json_escape_str(tmp, (int)sizeof(tmp), ev->source_component);
    (void)n;
    APPEND(tmp);
    APPEND(",");

    /* target_id */
    APPEND("\"target_id\":");
    n = json_escape_str(tmp, (int)sizeof(tmp), ev->target_id);
    (void)n;
    APPEND(tmp);
    APPEND(",");

    /* event_hash */
    {
        char nbuf[24];
        unsigned int hv = ev->event_hash;
        int i = 0;
        if (hv == 0) {
            nbuf[i++] = '0';
        } else {
            char tmp2[22];
            int j = 0;
            while (hv > 0) { tmp2[j++] = (char)('0' + (hv % 10)); hv /= 10; }
            while (j > 0) nbuf[i++] = tmp2[--j];
        }
        nbuf[i] = '\0';
        APPEND("\"event_hash\":");
        APPEND(nbuf);
        APPEND(",");
    }

    /* timestamp */
    APPEND("\"timestamp\":");
    n = json_escape_str(tmp, (int)sizeof(tmp), ev->timestamp);
    (void)n;
    APPEND(tmp);

    APPEND("}");

#undef APPEND

    return used;
}

/* ── Emitter lifecycle ──────────────────────────────────────────────── */

void fzr_emitter_init(fzr_emitter_t *em,
                      fzr_mode_t     mode,
                      FILE          *fp,
                      int            deterministic)
{
    if (!em) return;
    memset(em, 0, sizeof(*em));
    em->mode          = mode;
    em->deterministic = deterministic;
    em->event_count   = 0;

    if (mode == FZR_MODE_STDOUT) {
        em->fp = stdout;
    } else if (mode == FZR_MODE_FILE) {
        em->fp = fp; /* may be NULL; caller must call fzr_emitter_open_file */
    } else {
        em->fp = NULL; /* disabled */
    }
}

int fzr_emitter_open_file(fzr_emitter_t *em, const char *path)
{
    if (!em || em->mode == FZR_MODE_DISABLED)
        return 0; /* no-op for disabled emitter */
    if (!path || !*path)
        return -1;
    em->fp = fopen(path, "w");
    if (!em->fp)
        return -1;
    return 0;
}

void fzr_emitter_set_target(fzr_emitter_t *em, const char *target_id)
{
    if (!em) return;
    safe_strlcpy(em->target_id, target_id ? target_id : "",
                 (int)sizeof(em->target_id));
}

int fzr_emitter_emit(fzr_emitter_t *em, const fzr_event_t *ev)
{
    char buf[1024];
    int n;

    if (!em || em->mode == FZR_MODE_DISABLED || !em->fp)
        return 0; /* no-op */
    if (!ev)
        return 0;

    n = fzr_event_to_jsonl(buf, (int)sizeof(buf), ev);
    (void)n;

    if (fprintf(em->fp, "%s\n", buf) < 0)
        return 0;

    em->event_count++;
    return 1;
}

void fzr_emitter_flush(fzr_emitter_t *em)
{
    if (!em || em->mode == FZR_MODE_DISABLED || !em->fp)
        return;
    fflush(em->fp);
}

void fzr_emitter_close(fzr_emitter_t *em)
{
    if (!em || em->mode == FZR_MODE_DISABLED || !em->fp)
        return;
    fflush(em->fp);
    if (em->mode == FZR_MODE_FILE && em->fp != stdout && em->fp != stderr) {
        fclose(em->fp);
        em->fp = NULL;
    }
}

/* ── Module scan integration ─────────────────────────────────────────── */

int fzr_emit_from_vuln_scan(fzr_emitter_t     *em,
                             const ir_module_t *mod)
{
    int fi;
    int findings = 0;
    int total_nodes = 0;
    int total_funcs;

    if (!em || em->mode == FZR_MODE_DISABLED)
        return 0; /* no-op in standard compilation mode */
    if (!mod)
        return 0;

    total_funcs = mod->func_count;

    /* Iterate all functions and nodes; emit one event per tagged node. */
    for (fi = 0; fi < total_funcs; fi++) {
        const ir_func_t *fn = mod->funcs[fi];
        const ir_node_t *n;

        if (!fn) continue;

        for (n = fn->head; n; n = n->next) {
            total_nodes++;

            if (n->vuln_tags != 0) {
                /* Iterate each set bit and emit a separate finding per tag.
                 * This avoids multi-bit ambiguity in the receipt and lets
                 * downstream tooling filter by individual classification. */
                unsigned int bits = n->vuln_tags;
                unsigned int bit;

                for (bit = 1;
                     bit <= (unsigned int)IR_VULN_ALL_KNOWN;
                     bit <<= 1) {
                    fzr_event_t ev;

                    if (!(bits & bit))
                        continue;

                    fzr_event_build(&ev,
                                    FZR_EVENT_AUDIT_FINDING,
                                    (ir_vuln_tag_t)bit,
                                    "ir_pass",
                                    em->target_id,
                                    em->deterministic);

                    fzr_emitter_emit(em, &ev);
                    findings++;
                }
            }
        }
    }

    /* Emit a compile_summary event */
    {
        fzr_event_t sumev;
        char component[64];
        char target[128];

        safe_strlcpy(component, "compiler_cli", (int)sizeof(component));
        safe_strlcpy(target, em->target_id, (int)sizeof(target));

        memset(&sumev, 0, sizeof(sumev));
        sumev.kind         = FZR_EVENT_COMPILE_SUMMARY;
        sumev.severity     = FZR_SEV_INFO;
        sumev.vuln_tag_mask = 0;
        safe_strlcpy(sumev.vuln_tag_str, "IR_VULN_NONE",
                     (int)sizeof(sumev.vuln_tag_str));
        safe_strlcpy(sumev.source_component, component,
                     (int)sizeof(sumev.source_component));
        safe_strlcpy(sumev.target_id, target,
                     (int)sizeof(sumev.target_id));
        safe_strlcpy(sumev.schema_version, FZR_SCHEMA_VERSION,
                     (int)sizeof(sumev.schema_version));

        if (em->deterministic) {
            safe_strlcpy(sumev.timestamp, "DETERMINISTIC",
                         (int)sizeof(sumev.timestamp));
        } else {
            time_t now = time(NULL);
            struct tm *utc = gmtime(&now);
            if (utc) {
                int y = utc->tm_year + 1900;
                int mo = utc->tm_mon + 1;
                int d  = utc->tm_mday;
                int h  = utc->tm_hour;
                int mi = utc->tm_min;
                int s  = utc->tm_sec;
                sumev.timestamp[0]  = (char)('0' + y / 1000);
                sumev.timestamp[1]  = (char)('0' + (y / 100) % 10);
                sumev.timestamp[2]  = (char)('0' + (y / 10) % 10);
                sumev.timestamp[3]  = (char)('0' + y % 10);
                sumev.timestamp[4]  = '-';
                sumev.timestamp[5]  = (char)('0' + mo / 10);
                sumev.timestamp[6]  = (char)('0' + mo % 10);
                sumev.timestamp[7]  = '-';
                sumev.timestamp[8]  = (char)('0' + d / 10);
                sumev.timestamp[9]  = (char)('0' + d % 10);
                sumev.timestamp[10] = 'T';
                sumev.timestamp[11] = (char)('0' + h / 10);
                sumev.timestamp[12] = (char)('0' + h % 10);
                sumev.timestamp[13] = ':';
                sumev.timestamp[14] = (char)('0' + mi / 10);
                sumev.timestamp[15] = (char)('0' + mi % 10);
                sumev.timestamp[16] = ':';
                sumev.timestamp[17] = (char)('0' + s / 10);
                sumev.timestamp[18] = (char)('0' + s % 10);
                sumev.timestamp[19] = 'Z';
                sumev.timestamp[20] = '\0';
            } else {
                safe_strlcpy(sumev.timestamp, "WALL_CLOCK_ERROR",
                             (int)sizeof(sumev.timestamp));
            }
        }

        sumev.event_hash = fzr_event_hash(&sumev);
        fzr_emitter_emit(em, &sumev);
    }

    return findings;
}
