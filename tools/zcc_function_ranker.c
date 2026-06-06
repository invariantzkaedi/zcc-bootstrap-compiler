/*
 * zcc_function_ranker.c — D-29: Runtime Function Heat Ranker
 *
 * Reads a runtime genome JSON (from zcc_runtime_probe) and ranks all
 * observed functions by a composite "heat score":
 *
 *   heat_score = calls * max_depth
 *
 * This surface the functions most likely to dominate runtime behavior:
 * high call count amplified by deep stack participation.
 *
 * Three ranked views are emitted:
 *   [HEAT]  Top-N by heat_score  (calls * max_depth)
 *   [FREQ]  Top-N by call count  (most frequently called)
 *   [DEEP]  Top-N by max_depth   (deepest stack participants)
 *
 * Usage:
 *   zcc_function_ranker <runtime_genome.json> [--top N] [--out ranked.json]
 *
 * Memory discipline: all storage is stack or malloc/free paired.
 * No phantom closures.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/* ── Data structures ─────────────────────────────────────────────────── */

#define RANKER_MAX_FUNCS 4096

typedef struct {
    uintptr_t address;
    uint64_t  calls;
    uint32_t  max_depth;
    uint64_t  heat_score;   /* calls * max_depth */
} RankFunc;

/* ── JSON helpers ────────────────────────────────────────────────────── */

static void die(const char *msg) {
    fprintf(stderr, "zcc_function_ranker: fatal: %s\n", msg);
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

/* ── Parse runtime genome ────────────────────────────────────────────── */

static int parse_runtime_funcs(const char *json,
                                RankFunc *funcs, int max_funcs,
                                int *observed_out, int *peak_depth_out,
                                uint64_t *total_calls_out) {
    find_json_int_scoped(json, "\"observed_functions\"", observed_out);
    find_json_int_scoped(json, "\"peak_call_depth\"",    peak_depth_out);
    find_json_uint64_scoped(json, "\"total_calls\"",     total_calls_out);

    const char *arr = strstr(json, "\"functions\":");
    if (!arr) return 0;
    arr = strchr(arr, '[');
    if (!arr) return 0;
    arr++;

    int count = 0;
    while (*arr && *arr != ']' && count < max_funcs) {
        const char *obj = strchr(arr, '{');
        if (!obj) break;
        const char *obj_end = strchr(obj, '}');
        if (!obj_end) break;

        char addr_str[32] = "";
        find_json_string_scoped(obj, "\"address\"", addr_str, sizeof(addr_str));
        uint64_t calls = 0;
        int max_depth  = 0;
        find_json_uint64_scoped(obj, "\"calls\"",    &calls);
        find_json_int_scoped(obj,    "\"max_depth\"", &max_depth);

        funcs[count].address    = addr_str[0] ? (uintptr_t)strtoull(addr_str, NULL, 16) : 0;
        funcs[count].calls      = calls;
        funcs[count].max_depth  = (uint32_t)max_depth;
        funcs[count].heat_score = calls * (uint64_t)(max_depth > 0 ? max_depth : 1);
        count++;
        arr = obj_end + 1;
    }
    return count;
}

/* ── Sorting helpers ─────────────────────────────────────────────────── */

static int cmp_heat_desc(const void *a, const void *b) {
    const RankFunc *fa = (const RankFunc *)a;
    const RankFunc *fb = (const RankFunc *)b;
    if (fb->heat_score > fa->heat_score) return  1;
    if (fb->heat_score < fa->heat_score) return -1;
    return 0;
}

static int cmp_calls_desc(const void *a, const void *b) {
    const RankFunc *fa = (const RankFunc *)a;
    const RankFunc *fb = (const RankFunc *)b;
    if (fb->calls > fa->calls) return  1;
    if (fb->calls < fa->calls) return -1;
    return 0;
}

static int cmp_depth_desc(const void *a, const void *b) {
    const RankFunc *fa = (const RankFunc *)a;
    const RankFunc *fb = (const RankFunc *)b;
    if (fb->max_depth > fa->max_depth) return  1;
    if (fb->max_depth < fa->max_depth) return -1;
    return 0;
}

/* ── JSON emitter ────────────────────────────────────────────────────── */

static void emit_ranked_json(const char *out_path,
                              RankFunc *by_heat,  int heat_n,
                              RankFunc *by_calls, int calls_n,
                              RankFunc *by_depth, int depth_n,
                              int observed, int peak_depth,
                              uint64_t total_calls) {
    FILE *f = fopen(out_path, "w");
    if (!f) { fprintf(stderr, "warning: cannot write %s\n", out_path); return; }

    fprintf(f, "{\n");
    fprintf(f, "  \"schema\": \"zcc.function_rank.v1\",\n");
    fprintf(f, "  \"observed_functions\": %d,\n", observed);
    fprintf(f, "  \"peak_call_depth\": %d,\n", peak_depth);
    fprintf(f, "  \"total_calls\": %llu,\n", (unsigned long long)total_calls);

    /* by_heat */
    fprintf(f, "  \"ranked_by_heat\": [\n");
    for (int i = 0; i < heat_n; i++) {
        fprintf(f, "    { \"rank\": %d, \"address\": \"0x%llx\","
                   " \"calls\": %llu, \"max_depth\": %u,"
                   " \"heat_score\": %llu }%s\n",
                i + 1,
                (unsigned long long)by_heat[i].address,
                (unsigned long long)by_heat[i].calls,
                by_heat[i].max_depth,
                (unsigned long long)by_heat[i].heat_score,
                (i == heat_n - 1) ? "" : ",");
    }
    fprintf(f, "  ],\n");

    /* by_calls */
    fprintf(f, "  \"ranked_by_calls\": [\n");
    for (int i = 0; i < calls_n; i++) {
        fprintf(f, "    { \"rank\": %d, \"address\": \"0x%llx\","
                   " \"calls\": %llu }%s\n",
                i + 1,
                (unsigned long long)by_calls[i].address,
                (unsigned long long)by_calls[i].calls,
                (i == calls_n - 1) ? "" : ",");
    }
    fprintf(f, "  ],\n");

    /* by_depth */
    fprintf(f, "  \"ranked_by_depth\": [\n");
    for (int i = 0; i < depth_n; i++) {
        fprintf(f, "    { \"rank\": %d, \"address\": \"0x%llx\","
                   " \"max_depth\": %u }%s\n",
                i + 1,
                (unsigned long long)by_depth[i].address,
                by_depth[i].max_depth,
                (i == depth_n - 1) ? "" : ",");
    }
    fprintf(f, "  ]\n");
    fprintf(f, "}\n");
    fclose(f);
}

/* ═══════════════════════════════════════════════════════════════════════ */

int main(int argc, char **argv) {
    const char *genome_path = NULL;
    const char *out_path    = NULL;
    int         top_n       = 10;

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] != '-') {
            genome_path = argv[i];
        } else if (strcmp(argv[i], "--top") == 0 && i+1<argc) {
            top_n = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--out") == 0 && i+1<argc) {
            out_path = argv[++i];
        }
    }

    if (!genome_path) {
        printf("Usage: zcc_function_ranker <runtime_genome.json>"
               " [--top N] [--out ranked.json]\n");
        return 2;
    }

    /* ── Load genome ─────────────────────────────────────────────────── */
    size_t sz = 0;
    uint8_t *data = load_file(genome_path, &sz);
    if (!data) { fprintf(stderr, "error: cannot open %s\n", genome_path); return 1; }

    RankFunc *funcs = malloc(RANKER_MAX_FUNCS * sizeof(RankFunc));
    if (!funcs) { free(data); die("out of memory"); }

    int      observed    = 0;
    int      peak_depth  = 0;
    uint64_t total_calls = 0;
    int      count = parse_runtime_funcs((const char *)data, funcs,
                                          RANKER_MAX_FUNCS, &observed,
                                          &peak_depth, &total_calls);
    free(data);

    if (count == 0) {
        fprintf(stderr, "error: no function entries found in genome\n");
        free(funcs);
        return 1;
    }

    /* Clamp top_n */
    if (top_n > count) top_n = count;

    /* ── Three sorted copies ─────────────────────────────────────────── */
    RankFunc *by_heat  = malloc(count * sizeof(RankFunc));
    RankFunc *by_calls = malloc(count * sizeof(RankFunc));
    RankFunc *by_depth = malloc(count * sizeof(RankFunc));
    if (!by_heat || !by_calls || !by_depth) { free(funcs); die("out of memory"); }

    memcpy(by_heat,  funcs, count * sizeof(RankFunc));
    memcpy(by_calls, funcs, count * sizeof(RankFunc));
    memcpy(by_depth, funcs, count * sizeof(RankFunc));
    free(funcs);

    qsort(by_heat,  count, sizeof(RankFunc), cmp_heat_desc);
    qsort(by_calls, count, sizeof(RankFunc), cmp_calls_desc);
    qsort(by_depth, count, sizeof(RankFunc), cmp_depth_desc);

    /* ── Print report ────────────────────────────────────────────────── */
    printf("=== ZCC Function Heat Ranker ===\n");
    printf("Genome:            %s\n",  genome_path);
    printf("Observed functions: %d\n", observed);
    printf("Peak call depth:    %d\n", peak_depth);
    printf("Total calls:        %llu\n\n", (unsigned long long)total_calls);

    printf("[HEAT] Top-%d by heat_score (calls * max_depth):\n", top_n);
    printf("  %-4s  %-20s  %10s  %8s  %12s\n",
           "Rank", "Address", "Calls", "MaxDepth", "HeatScore");
    for (int i = 0; i < top_n; i++) {
        printf("  %-4d  0x%-18llx  %10llu  %8u  %12llu\n",
               i + 1,
               (unsigned long long)by_heat[i].address,
               (unsigned long long)by_heat[i].calls,
               by_heat[i].max_depth,
               (unsigned long long)by_heat[i].heat_score);
    }
    printf("\n");

    printf("[FREQ] Top-%d by call frequency:\n", top_n);
    printf("  %-4s  %-20s  %10s\n", "Rank", "Address", "Calls");
    for (int i = 0; i < top_n; i++) {
        printf("  %-4d  0x%-18llx  %10llu\n",
               i + 1,
               (unsigned long long)by_calls[i].address,
               (unsigned long long)by_calls[i].calls);
    }
    printf("\n");

    printf("[DEEP] Top-%d by max call depth:\n", top_n);
    printf("  %-4s  %-20s  %8s\n", "Rank", "Address", "MaxDepth");
    for (int i = 0; i < top_n; i++) {
        printf("  %-4d  0x%-18llx  %8u\n",
               i + 1,
               (unsigned long long)by_depth[i].address,
               by_depth[i].max_depth);
    }

    /* ── Optional JSON output ─────────────────────────────────────────── */
    if (out_path) {
        emit_ranked_json(out_path,
                         by_heat,  top_n,
                         by_calls, top_n,
                         by_depth, top_n,
                         observed, peak_depth, total_calls);
        printf("\nRanking report written to: %s\n", out_path);
    }

    free(by_heat);
    free(by_calls);
    free(by_depth);
    return 0;
}
