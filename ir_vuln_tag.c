/*
 * ir_vuln_tag.c — ZCC IR Vulnerability / Security Tag Schema Implementation
 *
 * DEFENSIVE SECURITY-ANALYSIS SCAFFOLD ONLY.
 * This file implements the helper APIs declared in ir_vuln_tag.h.
 * It does NOT perform exploit execution, payload generation, network calls,
 * or any offensive operation.
 *
 * Build (standalone, with EVM lifter):
 *   gcc -O0 -std=c99 -Wall -Wextra -I. \
 *       -o /tmp/test_ir_vuln_tag \
 *       tests/test_ir_vuln_tag.c ir_vuln_tag.c ir.c -lm
 *
 * Build with full ZCC project:
 *   Included automatically when linking evm_lifter.c or compiler passes.
 *
 * Coverage note: tests in tests/test_ir_vuln_tag.c cover all API paths.
 * Full 95%+ production coverage requires a production harness — see issue.
 */

#include "ir_vuln_tag.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Table: single-bit tag → stable string name ──────────────────────── */

typedef struct {
    ir_vuln_tag_t bit;
    const char   *name;
} ir_vuln_tag_entry_t;

static const ir_vuln_tag_entry_t IR_VULN_TAG_TABLE[] = {
    { IR_VULN_UNTRUSTED_CALL,  "IR_VULN_UNTRUSTED_CALL"  },
    { IR_VULN_DELEGATE_CALL,   "IR_VULN_DELEGATE_CALL"   },
    { IR_VULN_STATIC_CALL,     "IR_VULN_STATIC_CALL"     },
    { IR_VULN_STATE_WRITE,     "IR_VULN_STATE_WRITE"      },
    { IR_VULN_PRIV_BOUNDARY,   "IR_VULN_PRIV_BOUNDARY"   },
    { IR_VULN_UNKNOWN,         "IR_VULN_UNKNOWN"          },
    { IR_VULN_SELFDESTRUCT,    "IR_VULN_SELFDESTRUCT"     },
    { IR_VULN_CONTRACT_CREATE, "IR_VULN_CONTRACT_CREATE"  },
    { IR_VULN_EXEC_BARRIER,    "IR_VULN_EXEC_BARRIER"     },
};

enum { IR_VULN_TAG_TABLE_LEN =
    (int)(sizeof(IR_VULN_TAG_TABLE) / sizeof(IR_VULN_TAG_TABLE[0])) };

/* ── ir_vuln_tag_set ──────────────────────────────────────────────────── */

void ir_vuln_tag_set(ir_node_t *n, ir_vuln_tag_t tags)
{
    if (!n) return;
    n->vuln_tags |= (unsigned int)tags;
}

/* ── ir_vuln_tag_has ──────────────────────────────────────────────────── */

int ir_vuln_tag_has(const ir_node_t *n, ir_vuln_tag_t tags)
{
    if (!n) return 0;
    if (tags == IR_VULN_NONE) return 1; /* vacuously true */
    return (n->vuln_tags & (unsigned int)tags) == (unsigned int)tags;
}

/* ── ir_vuln_tag_to_str ───────────────────────────────────────────────── */

const char *ir_vuln_tag_to_str(ir_vuln_tag_t tag)
{
    int i;

    if (tag == IR_VULN_NONE)
        return "IR_VULN_NONE";

    /* Check if more than one bit is set (combination) */
    if ((tag & (tag - 1)) != 0)
        return "IR_VULN_MULTI";

    for (i = 0; i < IR_VULN_TAG_TABLE_LEN; i++) {
        if (IR_VULN_TAG_TABLE[i].bit == tag)
            return IR_VULN_TAG_TABLE[i].name;
    }

    return "IR_VULN_UNKNOWN";
}

/* ── ir_vuln_tag_from_str ─────────────────────────────────────────────── */

ir_vuln_tag_t ir_vuln_tag_from_str(const char *s)
{
    int i;

    if (!s || !*s)
        return IR_VULN_UNKNOWN;

    if (strcmp(s, "IR_VULN_NONE") == 0)
        return IR_VULN_NONE;

    for (i = 0; i < IR_VULN_TAG_TABLE_LEN; i++) {
        if (strcmp(s, IR_VULN_TAG_TABLE[i].name) == 0)
            return IR_VULN_TAG_TABLE[i].bit;
    }

    /* Unknown string — return IR_VULN_UNKNOWN, never fail */
    return IR_VULN_UNKNOWN;
}

/* ── ir_vuln_tag_unknown_safe ─────────────────────────────────────────── */

ir_vuln_tag_t ir_vuln_tag_unknown_safe(unsigned int raw)
{
    /* If raw contains bits beyond the known schema, return IR_VULN_UNKNOWN
     * to signal that the caller should treat it as an unrecognized extension. */
    if (raw & ~((unsigned int)IR_VULN_ALL_KNOWN))
        return IR_VULN_UNKNOWN;
    return (ir_vuln_tag_t)raw;
}

/* ── ir_vuln_map_from_evm_tag ─────────────────────────────────────────── */

/*
 * Maps legacy evm_ir_tag_t integer values (from evm_lifter.h) into the
 * corresponding ir_vuln_tag_t bitmask.
 *
 * evm_ir_tag_t values (as of evm_lifter.h at PR merge):
 *   0 = IR_TAG_NONE
 *   1 = IR_TAG_UNTRUSTED_EXTERNAL_CALL  (CALL, CALLCODE)
 *   2 = IR_TAG_STATIC_CALL
 *   3 = IR_TAG_SELFDESTRUCT
 *   4 = IR_TAG_SSTORE                   (state write)
 *   5 = IR_TAG_CREATE
 *   6 = IR_TAG_EVM_BARRIER
 *
 * Note: evm_lifter.h sets DELEGATECALL to IR_TAG_UNTRUSTED_EXTERNAL_CALL.
 * The new schema separates IR_VULN_DELEGATE_CALL.  The EVM lifter was
 * updated to call ir_vuln_tag_set() directly for DELEGATECALL so both
 * IR_VULN_UNTRUSTED_CALL and IR_VULN_DELEGATE_CALL are set for that opcode.
 */
ir_vuln_tag_t ir_vuln_map_from_evm_tag(int evm_tag)
{
    switch (evm_tag) {
    case 0: return IR_VULN_NONE;
    case 1: return IR_VULN_UNTRUSTED_CALL;
    case 2: return IR_VULN_STATIC_CALL;
    case 3: return IR_VULN_SELFDESTRUCT;
    case 4: return IR_VULN_STATE_WRITE;
    case 5: return IR_VULN_CONTRACT_CREATE;
    case 6: return IR_VULN_EXEC_BARRIER;
    default:
        return IR_VULN_UNKNOWN;
    }
}

/* ── ir_vuln_tags_to_json ─────────────────────────────────────────────── */

void ir_vuln_tags_to_json(const ir_node_t *n, char *buf, int bufsz)
{
    int i;
    int first = 1;
    int used = 0;
    unsigned int bits;

    if (!buf || bufsz <= 0) return;
    buf[0] = '\0';

    if (!n || n->vuln_tags == 0) {
        if (bufsz >= 3) { buf[0] = '['; buf[1] = ']'; buf[2] = '\0'; }
        return;
    }

    bits = n->vuln_tags;

    /* opening bracket */
    if (used + 1 < bufsz) { buf[used++] = '['; buf[used] = '\0'; }

    for (i = 0; i < IR_VULN_TAG_TABLE_LEN; i++) {
        const char *name;
        int nlen;
        int needed;

        if (!(bits & (unsigned int)IR_VULN_TAG_TABLE[i].bit))
            continue;

        name = IR_VULN_TAG_TABLE[i].name;
        nlen = (int)strlen(name);
        /* need: comma(1) + quote(1) + name + quote(1) = nlen+3, or nlen+2 if first */
        needed = first ? (nlen + 2) : (nlen + 3);

        if (used + needed + 1 >= bufsz) break; /* truncate gracefully */

        if (!first) { buf[used++] = ','; buf[used] = '\0'; }
        buf[used++] = '"';
        memcpy(buf + used, name, (size_t)nlen);
        used += nlen;
        buf[used++] = '"';
        buf[used]   = '\0';
        first = 0;
    }

    /* closing bracket */
    if (used + 1 < bufsz) { buf[used++] = ']'; buf[used] = '\0'; }
}

/* ── ir_pass_vuln_scan ────────────────────────────────────────────────── */
/*
 * Compiler-pass integration hook.
 * Read-only scan of all IR nodes; summarizes security-tagged nodes.
 * Returns count of nodes with non-zero vuln_tags.
 *
 * Output format (one line per tagged node):
 *   <func_name>  pc=<node_index>  op=<OP_NAME>  tags=<JSON_array>
 */
int ir_pass_vuln_scan(const ir_module_t *mod, FILE *fp)
{
    int fi;
    int total = 0;
    char jsonbuf[512];

    if (!mod) return 0;
    if (!fp)  fp = stdout;

    fprintf(fp, "=== ir_pass_vuln_scan: module vuln scan ===\n");

    for (fi = 0; fi < mod->func_count; fi++) {
        const ir_func_t *fn = mod->funcs[fi];
        const ir_node_t *n;
        int node_idx = 0;

        if (!fn) continue;

        for (n = fn->head; n; n = n->next, node_idx++) {
            if (n->vuln_tags == 0) continue;

            ir_vuln_tags_to_json(n, jsonbuf, (int)sizeof(jsonbuf));
            fprintf(fp, "  func=%-24s  pc=%-4d  op=%-12s  vuln_tags=%s\n",
                    fn->name, node_idx, ir_op_name(n->op), jsonbuf);
            total++;
        }
    }

    fprintf(fp, "=== ir_pass_vuln_scan: %d tagged node(s) found ===\n", total);
    return total;
}
