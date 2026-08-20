/*
 * zcc_abi_verifier.c — Standalone ZCC ABI & Struct Layout Verifier
 * =================================================================
 * Audits C source files and headers for:
 *   - Struct total size & alignment parity
 *   - __attribute__((packed)) and __attribute__((aligned(N))) member layout
 *   - Hardware descriptor boundaries (e.g. 10-byte idt_ptr_64)
 *   - System V AMD64 ABI eightbyte classification (INTEGER, SSE, MEMORY)
 *
 * Usage:
 *   ./zcc-abi-verifier input.c [--json] [--fail-on-violation] [--no-fail]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define ZCC_STANDALONE_VERIFIER 1
#ifndef LAYOUT_PHASE_INIT
#define LAYOUT_PHASE_INIT 0
#endif

#include "../../zcc_diagnostics.h"

/* Frontend includes only: no codegen, no backends, no pass manager */
#include "../../part1.c"
#include "../../part0_pp.c"
#include "../../part2.c"
#include "../../part3.c"
#include "../../sym_type_ast_ir.c"

Compiler *g_cc = NULL;
void zcc_oracle_log_allocation(void *ptr, size_t sz) {}

/* Minimal stubs for telemetry and validation */
void validate_node(Compiler *cc, Node *node, const char *where, int line) {}
void validate_type(Compiler *cc, Type *type, const char *where, int line) {}
void telemetry_emit_node(TelemetryOp op, const char* node_type, int depth) {}
void zcc_handle_static_assert(Node *n, const char *msg, SourceLoc loc) {}

/* System V AMD64 Eightbyte Classification (Extracted from part4.c) */
static abi_class_t abi_join(abi_class_t a, abi_class_t b) {
    if (a == b) return a;
    if (a == CLASS_NO_CLASS) return b;
    if (b == CLASS_NO_CLASS) return a;
    if (a == CLASS_MEMORY || b == CLASS_MEMORY) return CLASS_MEMORY;
    if (a == CLASS_INTEGER || b == CLASS_INTEGER) return CLASS_INTEGER;
    return CLASS_SSE;
}

static void classify_field(Type *field_type, int field_offset, abi_class_t eb[2]) {
    int size = type_size(field_type);
    int first_eb  = field_offset / 8;
    int last_eb   = (field_offset + size - 1) / 8;
    abi_class_t fc = CLASS_INTEGER;

    int falign = type_align(field_type);
    if (falign > 0 && (field_offset % falign) != 0) {
        eb[0] = eb[1] = CLASS_MEMORY;
        return;
    }

    if (field_type->kind == TY_FLOAT || field_type->kind == TY_DOUBLE) {
        fc = CLASS_SSE;
    } else if (field_type->kind == TY_STRUCT || field_type->kind == TY_UNION) {
        StructField *sf;
        for (sf = field_type->fields; sf; sf = sf->next) {
            classify_field(sf->type, field_offset + sf->offset, eb);
        }
        return;
    } else if (field_type->kind == TY_ARRAY) {
        for (int i = 0; i < field_type->array_len; i++) {
            classify_field(field_type->base, field_offset + i * type_size(field_type->base), eb);
        }
        return;
    }

    if (first_eb >= 2 || last_eb >= 2) {
        eb[0] = eb[1] = CLASS_MEMORY;
        return;
    }
    eb[first_eb] = abi_join(eb[first_eb], fc);
    if (last_eb != first_eb) {
        eb[last_eb] = abi_join(eb[last_eb], fc);
    }
}

void classify_aggregate(Type *agg, abi_class_t eb[2]) {
    int size = type_size(agg);
    eb[0] = eb[1] = CLASS_NO_CLASS;

    if (size > 16 || size == 0) {
        eb[0] = eb[1] = CLASS_MEMORY;
        return;
    }

    if (agg->kind == TY_STRUCT || agg->kind == TY_UNION) {
        StructField *f;
        for (f = agg->fields; f; f = f->next) {
            classify_field(f->type, f->offset, eb);
            if (eb[0] == CLASS_MEMORY) return;
        }
    }

    if (eb[0] == CLASS_MEMORY || eb[1] == CLASS_MEMORY) {
        eb[0] = eb[1] = CLASS_MEMORY;
    }
}

typedef enum {
    ABI_STATUS_OK = 0,
    ABI_STATUS_PACKING,
    ABI_STATUS_ALIGN,
    ABI_STATUS_CLASS,
    ABI_STATUS_HARDWARE
} AbiStatus;

static int g_violations = 0;
static int g_json_mode = 0;

static void report_issue(AbiStatus st, const char *sname, int sz, int al, const char *msg) {
    const char *tag =
        st == ABI_STATUS_PACKING   ? "PACKING"   :
        st == ABI_STATUS_ALIGN     ? "ALIGN"     :
        st == ABI_STATUS_CLASS     ? "CLASS"     :
        st == ABI_STATUS_HARDWARE  ? "HARDWARE"  : "OK";

    if (g_json_mode) {
        printf("    {\"status\": \"%s\", \"struct\": \"%s\", \"size\": %d, \"align\": %d, \"message\": \"%s\"}",
               tag, sname ? sname : "<anon>", sz, al, msg);
    } else {
        fprintf(stderr, "  [!] ABI-%s VIOLATION: struct '%s' (size=%d, align=%d): %s\n",
                tag, sname ? sname : "<anon>", sz, al, msg);
    }
    if (st != ABI_STATUS_OK) g_violations++;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file.c|file.h>... [--json] [--fail-on-violation] [--no-fail]\n", argv[0]);
        return 2;
    }

    int fail_on_violation = 1;
    const char *input_file = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--json") == 0) {
            g_json_mode = 1;
        } else if (strcmp(argv[i], "--no-fail") == 0) {
            fail_on_violation = 0;
        } else if (strcmp(argv[i], "--fail-on-violation") == 0) {
            fail_on_violation = 1;
        } else if (argv[i][0] != '-') {
            input_file = argv[i];
        }
    }

    if (!input_file) {
        fprintf(stderr, "zcc-abi-verifier error: no input file provided\n");
        return 2;
    }

    FILE *fp = fopen(input_file, "r");
    if (!fp) {
        fprintf(stderr, "zcc-abi-verifier error: cannot open file '%s'\n", input_file);
        return 2;
    }

    fseek(fp, 0, SEEK_END);
    long fsz = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *code = (char *)malloc(fsz + 1);
    if (!code) {
        fclose(fp);
        fprintf(stderr, "zcc-abi-verifier error: out of memory reading '%s'\n", input_file);
        return 2;
    }
    fread(code, 1, fsz, fp);
    code[fsz] = 0;
    fclose(fp);

    Compiler cc_obj;
    Compiler *cc = &cc_obj;
    memset(cc, 0, sizeof(*cc));
    cc->out = stdout;
    g_cc = cc;

    cc->ty_void = type_new(cc, TY_VOID);
    cc->ty_char = type_new(cc, TY_CHAR);
    cc->ty_uchar = type_new(cc, TY_CHAR);
    cc->ty_short = type_new(cc, TY_SHORT);
    cc->ty_ushort = type_new(cc, TY_SHORT);
    cc->ty_int = type_new(cc, TY_INT);
    cc->ty_uint = type_new(cc, TY_INT);
    cc->ty_long = type_new(cc, TY_LONG);
    cc->ty_ulong = type_new(cc, TY_LONG);
    cc->ty_longlong = type_new(cc, TY_LONG);
    cc->ty_ulonglong = type_new(cc, TY_LONG);
    cc->ty_float = type_new(cc, TY_FLOAT);
    cc->ty_double = type_new(cc, TY_DOUBLE);
    cc->ty_longdouble = type_new(cc, TY_DOUBLE);


    scope_push(cc);


    int pp_len = 0;
    char *pp_source = zcc_preprocess(code, (int)fsz, input_file, "", "", &pp_len);
    if (!pp_source) {
        fprintf(stderr, "zcc-abi-verifier error: preprocessing failed for '%s'\n", input_file);
        free(code);
        return 2;
    }

    cc->source = pp_source;
    cc->source_len = pp_len;
    cc->filename = input_file;

    next_token(cc);
    Node *ast = parse_program(cc);

    if (g_json_mode) {
        printf("{\n  \"file\": \"%s\",\n  \"audits\": [\n", input_file);
    } else {
        printf("========================================================================\n");
        printf(" 🔱 ZCC ABI & STRUCT LAYOUT VERIFIER — %s\n", input_file);
        printf("========================================================================\n");
        printf("%-24s %-8s %-8s %-12s %-16s\n", "STRUCT NAME", "SIZE", "ALIGN", "PACKED", "SYSV CLASS");
        printf("------------------------------------------------------------------------\n");
    }

    int audited_count = 0;
    for (int i = 0; i < cc->num_structs; i++) {
        Type *st = cc->structs[i];
        if (!st || (st->kind != TY_STRUCT && st->kind != TY_UNION)) continue;

        const char *sname = st->tag[0] ? st->tag : "<anonymous>";
        int is_packed = st->is_packed;
        int sz = type_size(st);
        int al = type_align(st);

        abi_class_t eb[2] = {CLASS_INTEGER, CLASS_INTEGER};
        classify_aggregate(st, eb);
        const char *sysv_class = "INTEGER";
        if (eb[0] == CLASS_MEMORY) {
            sysv_class = "MEMORY";
        } else if (eb[0] == CLASS_SSE) {
            sysv_class = "SSE";
        }

        if (!g_json_mode) {
            printf("%-24s %-8d %-8d %-12s %-16s\n",
                   sname, sz, al, is_packed ? "YES" : "NO", sysv_class);
        }

        /* Check Rule 1: Hardware descriptor boundary (e.g. idt_ptr_64 must be 10 bytes) */
        if (is_packed && strstr(sname, "idt") != NULL && sz != 10) {
            report_issue(ABI_STATUS_HARDWARE, sname, sz, al, "packed hardware descriptor size != 10 bytes");
        }

        /* Check Rule 2: System V Return Class Consistency (> 16 bytes must be MEMORY/sret) */
        if (sz > 16 && eb[0] != CLASS_MEMORY) {
            report_issue(ABI_STATUS_CLASS, sname, sz, al, "aggregate > 16 bytes must be CLASS_MEMORY (sret)");
        }

        /* Check Rule 3: Packed type with non-trivial explicit alignment anomaly */
        if (is_packed && al > 1 && (sz % al != 0)) {
            report_issue(ABI_STATUS_PACKING, sname, sz, al, "packed struct alignment boundary misaligned with total size");
        }

        if (g_json_mode && i < cc->num_structs - 1) {
            printf(",\n");
        }
        audited_count++;
    }

    if (g_json_mode) {
        printf("\n  ],\n  \"audited\": %d,\n  \"violations\": %d\n}\n", audited_count, g_violations);
    } else {
        printf("------------------------------------------------------------------------\n");
        printf("Total Audited: %d | Violations: %d\n", audited_count, g_violations);
        printf("========================================================================\n");
    }

    free(code);

    if (g_violations > 0 && fail_on_violation) {
        return 1;
    }

    return 0;
}
