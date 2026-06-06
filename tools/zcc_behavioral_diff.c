/*
 * zcc_behavioral_diff.c — D-28: Static vs Runtime Behavioral Drift Detector
 *
 * Reads two genome JSON files:
 *   --static  <file>   compile-time genome (from zcc_topology_auditor --json)
 *   --runtime <file>   runtime genome      (from zcc_runtime_probe atexit emit)
 *
 * Compares them across four dimensions:
 *   1. Reachable function count (static prediction vs observed)
 *   2. Dead code candidates  (functions in static genome not seen at runtime)
 *   3. Peak stack depth      (static max_stack_frame bytes vs runtime frames)
 *   4. Hot function analysis (top-N by call count from runtime genome)
 *
 * Emits a structured behavioral_drift_report.json and prints a human-readable
 * summary to stdout.
 *
 * Verdict categories:
 *   FULL_COVERAGE    — all statically reachable functions observed at runtime
 *   PARTIAL_COVERAGE — some static functions not reached on this input
 *   OVERREACH        — runtime observed functions exceed static count (probe error)
 *   UNKNOWN          — insufficient data in one or both genomes
 *
 * Memory discipline:
 *   Every malloc() has a matching free() before exit.
 *   No phantom closures.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/* ── JSON helpers (same minimal pattern as sibling tools) ────────────── */

static void die(const char *msg) {
    fprintf(stderr, "zcc_behavioral_diff: fatal: %s\n", msg);
    exit(1);
}

static uint8_t *load_file(const char *path, size_t *sz) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    *sz = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *buf = malloc(*sz + 1);
    if (!buf) die("out of memory");
    if (fread(buf, 1, *sz, f) != *sz) die("file read error");
    fclose(f);
    buf[*sz] = 0;
    return buf;
}

static int find_json_int_scoped(const char *scope, const char *key, int *val) {
    if (!scope) return 0;
    const char *p = strstr(scope, key);
    if (!p) return 0;
    p += strlen(key);
    p = strchr(p, ':');
    if (!p) return 0;
    p++;
    while (*p && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) p++;
    *val = atoi(p);
    return 1;
}

static int find_json_uint64_scoped(const char *scope, const char *key, uint64_t *val) {
    if (!scope) return 0;
    const char *p = strstr(scope, key);
    if (!p) return 0;
    p += strlen(key);
    p = strchr(p, ':');
    if (!p) return 0;
    p++;
    while (*p && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) p++;
    *val = (uint64_t)strtoull(p, NULL, 10);
    return 1;
}

static int find_json_string_scoped(const char *scope, const char *key,
                                   char *out, int max) {
    if (!scope) return 0;
    const char *p = strstr(scope, key);
    if (!p) return 0;
    p += strlen(key);
    p = strchr(p, ':');
    if (!p) return 0;
    p++;
    p = strchr(p, '"');
    if (!p) return 0;
    p++;
    int len = 0;
    while (*p && *p != '"' && len < max - 1) out[len++] = *p++;
    out[len] = '\0';
    return 1;
}

/* ── Static genome data (compile-time) ───────────────────────────────── */

typedef struct {
    /* From execution_fingerprint */
    int reachable_functions;
    int leaf_functions;
    int branch_nodes;
    int critical_path_depth;
    /* From stack_analysis */
    int max_stack_frame;
    int average_stack_frame;
    int recursive_functions;
    /* From instruction_profile */
    int mov_cnt, call_cnt, jmp_cnt, ret_cnt;
    /* From telemetry */
    int functions_count;
    int symbols_count;
    /* fingerprint hashes */
    char controlflow_root[65];
    char stack_root[65];
} StaticGenome;

static void parse_static_genome(const char *json, StaticGenome *g) {
    memset(g, 0, sizeof(StaticGenome));

    const char *fingerprint = strstr(json, "\"execution_fingerprint\"");
    const char *stackblock  = strstr(json, "\"stack_analysis\"");
    const char *instruction = strstr(json, "\"instruction_profile\"");
    const char *telemetry   = strstr(json, "\"telemetry\"");

    if (fingerprint) {
        find_json_int_scoped(fingerprint, "\"reachable_functions\"", &g->reachable_functions);
        find_json_int_scoped(fingerprint, "\"leaf_functions\"",      &g->leaf_functions);
        find_json_int_scoped(fingerprint, "\"branch_nodes\"",        &g->branch_nodes);
        find_json_int_scoped(fingerprint, "\"critical_path_depth\"", &g->critical_path_depth);
        find_json_string_scoped(fingerprint, "\"controlflow_root\"", g->controlflow_root, 65);
        find_json_string_scoped(fingerprint, "\"stack_root\"",       g->stack_root,       65);
    }
    if (stackblock) {
        find_json_int_scoped(stackblock, "\"max_stack_frame\"",     &g->max_stack_frame);
        find_json_int_scoped(stackblock, "\"average_stack_frame\"", &g->average_stack_frame);
        find_json_int_scoped(stackblock, "\"recursive_functions\"", &g->recursive_functions);
    }
    if (instruction) {
        find_json_int_scoped(instruction, "\"mov\"",  &g->mov_cnt);
        find_json_int_scoped(instruction, "\"call\"", &g->call_cnt);
        find_json_int_scoped(instruction, "\"jmp\"",  &g->jmp_cnt);
        find_json_int_scoped(instruction, "\"ret\"",  &g->ret_cnt);
    }
    if (telemetry) {
        find_json_int_scoped(telemetry, "\"functions_count\"", &g->functions_count);
        find_json_int_scoped(telemetry, "\"symbols_count\"",   &g->symbols_count);
    }
}

/* ── Runtime genome data ─────────────────────────────────────────────── */

#define RT_MAX_FUNCS 4096

typedef struct {
    uintptr_t address;
    uint64_t  calls;
    uint32_t  max_depth;
} RTFunc;

typedef struct {
    int      observed_functions;
    int      peak_call_depth;
    uint64_t total_calls;
    int      table_overflow;
    RTFunc   funcs[RT_MAX_FUNCS];
    int      func_count;
} RuntimeGenome;

static void parse_runtime_genome(const char *json, RuntimeGenome *g) {
    memset(g, 0, sizeof(RuntimeGenome));

    find_json_int_scoped(json,    "\"observed_functions\"", &g->observed_functions);
    find_json_int_scoped(json,    "\"peak_call_depth\"",    &g->peak_call_depth);
    find_json_uint64_scoped(json, "\"total_calls\"",        &g->total_calls);

    /* overflow flag */
    const char *ov = strstr(json, "\"table_overflow\":");
    if (ov) {
        ov += strlen("\"table_overflow\":");
        while (*ov == ' ') ov++;
        g->table_overflow = (strncmp(ov, "true", 4) == 0) ? 1 : 0;
    }

    /* Parse functions array */
    const char *arr = strstr(json, "\"functions\":");
    if (!arr) return;
    arr = strchr(arr, '[');
    if (!arr) return;
    arr++;

    int count = 0;
    while (*arr && *arr != ']' && count < RT_MAX_FUNCS) {
        /* Find next { */
        const char *obj = strchr(arr, '{');
        if (!obj) break;
        /* Find matching } */
        const char *obj_end = strchr(obj, '}');
        if (!obj_end) break;

        /* Parse fields within this object */
        char addr_str[32] = "";
        find_json_string_scoped(obj, "\"address\"", addr_str, sizeof(addr_str));
        if (addr_str[0]) {
            g->funcs[count].address = (uintptr_t)strtoull(addr_str, NULL, 16);
        }

        /* calls — integer field */
        {
            const char *p = strstr(obj, "\"calls\":");
            if (p && p < obj_end) {
                p += strlen("\"calls\":");
                while (*p == ' ') p++;
                g->funcs[count].calls = (uint64_t)strtoull(p, NULL, 10);
            }
        }
        /* max_depth — integer field */
        {
            const char *p = strstr(obj, "\"max_depth\":");
            if (p && p < obj_end) {
                p += strlen("\"max_depth\":");
                while (*p == ' ') p++;
                g->funcs[count].max_depth = (uint32_t)strtoul(p, NULL, 10);
            }
        }

        count++;
        arr = obj_end + 1;
    }
    g->func_count = count;
}

/* ── Comparison logic ────────────────────────────────────────────────── */

typedef struct {
    /* Coverage */
    int static_reachable;
    int runtime_observed;
    int dead_code_candidates;   /* static - runtime (lower bound) */
    int coverage_pct;           /* runtime / static * 100         */

    /* Depth comparison */
    int static_max_frame_bytes;
    int runtime_peak_frames;
    int depth_delta;            /* runtime_frames - static_critical_path */

    /* Call volume */
    uint64_t total_runtime_calls;

    /* Hot functions (top 5 by call count) */
    RTFunc hot[5];
    int    hot_count;

    /* Overflow */
    int table_overflow;

    /* Verdict */
    char verdict[32];
} DriftReport;

static void compute_drift(const StaticGenome *sg,
                           const RuntimeGenome *rg,
                           DriftReport *dr) {
    memset(dr, 0, sizeof(DriftReport));

    /* Use the richer of functions_count / reachable_functions as static baseline */
    int static_fn = sg->reachable_functions;
    if (static_fn == 0) static_fn = sg->functions_count;

    dr->static_reachable      = static_fn;
    dr->runtime_observed      = rg->observed_functions;
    dr->dead_code_candidates  = (static_fn > rg->observed_functions)
                                ? static_fn - rg->observed_functions : 0;
    dr->coverage_pct          = (static_fn > 0)
                                ? (rg->observed_functions * 100 / static_fn) : 0;

    dr->static_max_frame_bytes = sg->max_stack_frame;
    dr->runtime_peak_frames    = rg->peak_call_depth;
    dr->depth_delta            = rg->peak_call_depth - sg->critical_path_depth;

    dr->total_runtime_calls    = rg->total_calls;
    dr->table_overflow         = rg->table_overflow;

    /* Top-5 hot functions (runtime genome is already sorted descending) */
    dr->hot_count = rg->func_count < 5 ? rg->func_count : 5;
    for (int i = 0; i < dr->hot_count; i++)
        dr->hot[i] = rg->funcs[i];

    /* Verdict */
    if (static_fn == 0 || rg->observed_functions == 0) {
        strcpy(dr->verdict, "UNKNOWN");
    } else if (rg->observed_functions > static_fn) {
        strcpy(dr->verdict, "OVERREACH");
    } else if (dr->coverage_pct >= 99) {
        strcpy(dr->verdict, "FULL_COVERAGE");
    } else {
        strcpy(dr->verdict, "PARTIAL_COVERAGE");
    }
}

/* ── Emit JSON report ────────────────────────────────────────────────── */

static void emit_report(const char *out_path,
                         const DriftReport *dr,
                         const char *static_path,
                         const char *runtime_path) {
    FILE *f = fopen(out_path, "w");
    if (!f) { fprintf(stderr, "warning: cannot write %s\n", out_path); return; }

    fprintf(f, "{\n");
    fprintf(f, "  \"schema\": \"zcc.behavioral_drift.v1\",\n");
    fprintf(f, "  \"static_genome\": \"%s\",\n",  static_path);
    fprintf(f, "  \"runtime_genome\": \"%s\",\n", runtime_path);
    fprintf(f, "  \"coverage\": {\n");
    fprintf(f, "    \"static_reachable\": %d,\n",     dr->static_reachable);
    fprintf(f, "    \"runtime_observed\": %d,\n",     dr->runtime_observed);
    fprintf(f, "    \"dead_code_candidates\": %d,\n", dr->dead_code_candidates);
    fprintf(f, "    \"coverage_pct\": %d\n",          dr->coverage_pct);
    fprintf(f, "  },\n");
    fprintf(f, "  \"depth\": {\n");
    fprintf(f, "    \"static_max_frame_bytes\": %d,\n", dr->static_max_frame_bytes);
    fprintf(f, "    \"runtime_peak_frames\": %d,\n",    dr->runtime_peak_frames);
    fprintf(f, "    \"depth_delta_frames\": %d\n",      dr->depth_delta);
    fprintf(f, "  },\n");
    fprintf(f, "  \"call_volume\": {\n");
    fprintf(f, "    \"total_runtime_calls\": %llu,\n", (unsigned long long)dr->total_runtime_calls);
    fprintf(f, "    \"table_overflow\": %s\n", dr->table_overflow ? "true" : "false");
    fprintf(f, "  },\n");
    fprintf(f, "  \"hot_functions\": [\n");
    for (int i = 0; i < dr->hot_count; i++) {
        fprintf(f, "    { \"address\": \"0x%llx\", \"calls\": %llu, \"max_depth\": %u }%s\n",
                (unsigned long long)dr->hot[i].address,
                (unsigned long long)dr->hot[i].calls,
                dr->hot[i].max_depth,
                (i == dr->hot_count - 1) ? "" : ",");
    }
    fprintf(f, "  ],\n");
    fprintf(f, "  \"drift_verdict\": \"%s\"\n", dr->verdict);
    fprintf(f, "}\n");
    fclose(f);
}

/* ═══════════════════════════════════════════════════════════════════════ */

int main(int argc, char **argv) {
    const char *static_path  = NULL;
    const char *runtime_path = NULL;
    const char *out_path     = NULL;
    int         top_n        = 10;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--static") == 0 && i + 1 < argc)
            static_path = argv[++i];
        else if (strcmp(argv[i], "--runtime") == 0 && i + 1 < argc)
            runtime_path = argv[++i];
        else if (strcmp(argv[i], "--out") == 0 && i + 1 < argc)
            out_path = argv[++i];
        else if (strcmp(argv[i], "--top") == 0 && i + 1 < argc)
            top_n = atoi(argv[++i]);
    }

    if (!static_path || !runtime_path) {
        printf("Usage: zcc_behavioral_diff --static <genome.json> "
               "--runtime <runtime_genome.json> [--out drift.json] [--top N]\n");
        return 2;
    }

    /* ── Load static genome ──────────────────────────────────────────── */
    size_t sz_s = 0;
    uint8_t *data_s = load_file(static_path, &sz_s);
    if (!data_s) { fprintf(stderr, "error: cannot open static genome %s\n", static_path); return 1; }

    StaticGenome sg;
    parse_static_genome((const char *)data_s, &sg);
    free(data_s);

    /* ── Load runtime genome ─────────────────────────────────────────── */
    size_t sz_r = 0;
    uint8_t *data_r = load_file(runtime_path, &sz_r);
    if (!data_r) { fprintf(stderr, "error: cannot open runtime genome %s\n", runtime_path); return 1; }

    RuntimeGenome rg;
    parse_runtime_genome((const char *)data_r, &rg);
    free(data_r);

    /* ── Compute drift ───────────────────────────────────────────────── */
    DriftReport dr;
    compute_drift(&sg, &rg, &dr);

    /* ── Print report ────────────────────────────────────────────────── */
    printf("=== ZCC Behavioral Drift Analysis ===\n\n");
    printf("Static genome:   %s\n", static_path);
    printf("Runtime genome:  %s\n\n", runtime_path);

    printf("Coverage Analysis:\n");
    printf("  Static reachable functions:  %d\n",  dr.static_reachable);
    printf("  Runtime observed functions:  %d\n",  dr.runtime_observed);
    printf("  Dead code candidates:        %d\n",  dr.dead_code_candidates);
    printf("  Coverage:                    %d%%\n", dr.coverage_pct);
    printf("\n");

    printf("Depth Analysis:\n");
    printf("  Static max frame:            %d bytes\n",  dr.static_max_frame_bytes);
    printf("  Runtime peak depth:          %d frames\n", dr.runtime_peak_frames);
    printf("  Critical path (static):      %d frames\n", sg.critical_path_depth);
    printf("  Depth delta vs critical:     %+d frames\n", dr.depth_delta);
    printf("\n");

    printf("Call Volume:\n");
    printf("  Total runtime calls:         %llu\n",
           (unsigned long long)dr.total_runtime_calls);
    printf("  Table overflow:              %s\n", dr.table_overflow ? "YES" : "NO");
    printf("\n");

    /* Hot functions */
    int n = rg.func_count < top_n ? rg.func_count : top_n;
    if (n > 0) {
        printf("Hot Functions (top %d by call count):\n", n);
        printf("  %-20s  %12s  %s\n", "Address", "Calls", "MaxDepth");
        for (int i = 0; i < n; i++) {
            printf("  0x%-18llx  %12llu  %u\n",
                   (unsigned long long)rg.funcs[i].address,
                   (unsigned long long)rg.funcs[i].calls,
                   rg.funcs[i].max_depth);
        }
        printf("\n");
    }

    /* Static genome summary */
    printf("Static Genome Summary:\n");
    printf("  Functions (telemetry):       %d\n",  sg.functions_count);
    printf("  Symbols:                     %d\n",  sg.symbols_count);
    printf("  Call instructions:           %d\n",  sg.call_cnt);
    printf("  Recursive functions:         %d\n",  sg.recursive_functions);
    if (sg.controlflow_root[0])
        printf("  Controlflow root:            %s\n", sg.controlflow_root);
    printf("\n");

    printf("Behavioral Drift Verdict: %s\n", dr.verdict);

    /* ── Emit JSON if requested ──────────────────────────────────────── */
    if (out_path) {
        emit_report(out_path, &dr, static_path, runtime_path);
        printf("Drift report written to: %s\n", out_path);
    }

    /* Exit code: 0 = full/unknown, 1 = partial, 2 = overreach */
    if (strcmp(dr.verdict, "FULL_COVERAGE") == 0 ||
        strcmp(dr.verdict, "UNKNOWN")       == 0) return 0;
    if (strcmp(dr.verdict, "OVERREACH")     == 0) return 2;
    return 1; /* PARTIAL_COVERAGE — expected for most test inputs */
}
