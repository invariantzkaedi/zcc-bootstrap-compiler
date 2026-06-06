/*
 * zcc_impact_attribution.c — D-29: Runtime Impact Attribution Engine
 *
 * Correlates static genome drift (register/stack/instruction pressure changes
 * between two compiler versions) with runtime behavioral drift (call frequency
 * and depth changes between two runtime genomes) to produce a per-dimension
 * impact attribution and an estimated performance impact verdict.
 *
 * Usage:
 *   zcc_impact_attribution \
 *       --static-a <genome_a.json> \
 *       --static-b <genome_b.json> \
 *       [--runtime-a <runtime_genome_a.json>] \
 *       [--runtime-b <runtime_genome_b.json>] \
 *       [--version-a <label>] \
 *       [--version-b <label>] \
 *       [--out <report.json>]
 *
 * Static-only mode (no --runtime-a/b): reports static drift + impact estimate
 * based purely on compile-time metrics.
 *
 * Full mode (with runtime genomes): adds call volume change, depth change,
 * and hot-path shift detection to the attribution.
 *
 * Impact Score (additive):
 *   |register_drift_pct|    > 20  →  +3
 *   |stack_drift_bytes|     > 64  →  +3
 *   |instr_drift_pct|       > 15  →  +2
 *   |call_volume_change_pct|> 50  →  +3  (runtime mode only)
 *   |depth_change_frames|   > 3   →  +1  (runtime mode only)
 *
 * Verdict:  0 → NONE  |  1-2 → LOW  |  3-5 → MEDIUM  |  6+ → HIGH
 *
 * Memory discipline: every malloc() has a matching free() before exit.
 * No phantom closures.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <math.h>

/* ── JSON helpers ────────────────────────────────────────────────────── */

static void die(const char *msg) {
    fprintf(stderr, "zcc_impact_attribution: fatal: %s\n", msg);
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

static int find_json_double_scoped(const char *scope, const char *key, double *val) {
    if (!scope) return 0;
    const char *p = strstr(scope, key);
    if (!p) return 0;
    p += strlen(key);
    p = strchr(p, ':');
    if (!p) return 0;
    p++;
    while (*p && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) p++;
    *val = atof(p);
    return 1;
}

static time_t parse_iso8601(const char *ts) {
    struct tm t;
    memset(&t, 0, sizeof(t));
    int yr = 0, mo = 0, dy = 0, hr = 0, mn = 0, sc = 0;
    if (sscanf(ts, "%d-%d-%dT%d:%d:%d", &yr, &mo, &dy, &hr, &mn, &sc) >= 6) {
        t.tm_year = yr - 1900;
        t.tm_mon = mo - 1;
        t.tm_mday = dy;
        t.tm_hour = hr;
        t.tm_min = mn;
        t.tm_sec = sc;
        t.tm_isdst = -1;
        return mktime(&t);
    }
    return 0;
}

/* ── Static genome ───────────────────────────────────────────────────── */

typedef struct {
    int mov_cnt, call_cnt, lea_cnt, cmp_cnt, jmp_cnt, ret_cnt;
    int reg_counts[16];
    int max_stack_frame;
    int average_stack_frame;
    int reachable_functions;
    int functions_count;
    int critical_path_depth;
    int branch_nodes;
    char topology_root[65];
    char controlflow_root[65];
    char build_id[65];
} StaticGenome;

static void parse_static_genome(const char *json, StaticGenome *g) {
    memset(g, 0, sizeof(StaticGenome));
    const char *meta        = strstr(json, "\"metadata\"");
    const char *fingerprint = strstr(json, "\"execution_fingerprint\"");
    const char *instruction = strstr(json, "\"instruction_profile\"");
    const char *regblock    = strstr(json, "\"register_profile\"");
    const char *stackblock  = strstr(json, "\"stack_analysis\"");
    const char *merkle      = strstr(json, "\"merkle_topology\"");
    const char *telemetry   = strstr(json, "\"telemetry\"");

    if (meta)
        find_json_string_scoped(meta, "\"build_id\"", g->build_id, 65);
    if (merkle)
        find_json_string_scoped(merkle, "\"topology_root\"", g->topology_root, 65);
    if (fingerprint) {
        find_json_string_scoped(fingerprint, "\"controlflow_root\"", g->controlflow_root, 65);
        find_json_int_scoped(fingerprint, "\"reachable_functions\"", &g->reachable_functions);
        find_json_int_scoped(fingerprint, "\"critical_path_depth\"", &g->critical_path_depth);
        find_json_int_scoped(fingerprint, "\"branch_nodes\"",        &g->branch_nodes);
    }
    if (telemetry)
        find_json_int_scoped(telemetry, "\"functions_count\"", &g->functions_count);
    if (instruction) {
        find_json_int_scoped(instruction, "\"mov\"",  &g->mov_cnt);
        find_json_int_scoped(instruction, "\"call\"", &g->call_cnt);
        find_json_int_scoped(instruction, "\"lea\"",  &g->lea_cnt);
        find_json_int_scoped(instruction, "\"cmp\"",  &g->cmp_cnt);
        find_json_int_scoped(instruction, "\"jmp\"",  &g->jmp_cnt);
        find_json_int_scoped(instruction, "\"ret\"",  &g->ret_cnt);
    }
    if (regblock) {
        const char *rnames[16] = {
            "\"rax\"","\"rcx\"","\"rdx\"","\"rbx\"","\"rsp\"","\"rbp\"",
            "\"rsi\"","\"rdi\"","\"r8\"", "\"r9\"", "\"r10\"","\"r11\"",
            "\"r12\"","\"r13\"","\"r14\"","\"r15\""
        };
        for (int i = 0; i < 16; i++)
            find_json_int_scoped(regblock, rnames[i], &g->reg_counts[i]);
    }
    if (stackblock) {
        find_json_int_scoped(stackblock, "\"max_stack_frame\"",     &g->max_stack_frame);
        find_json_int_scoped(stackblock, "\"average_stack_frame\"", &g->average_stack_frame);
    }
}

static int static_total_reg(const StaticGenome *g) {
    int t = 0;
    for (int i = 0; i < 16; i++) t += g->reg_counts[i];
    return t;
}

static int static_total_instr(const StaticGenome *g) {
    return g->mov_cnt + g->call_cnt + g->lea_cnt +
           g->cmp_cnt + g->jmp_cnt + g->ret_cnt;
}

/* ── Runtime genome (minimal — only aggregate fields needed) ─────────── */

typedef struct {
    int      observed_functions;
    int      peak_call_depth;
    uint64_t total_calls;
    int      valid;   /* 1 = loaded successfully */
    /* top-1 hot function by call count (for hot-path shift) */
    uintptr_t hot_addr;
    uint64_t  hot_calls;
} RuntimeSummary;

static void parse_runtime_summary(const char *json, RuntimeSummary *r) {
    memset(r, 0, sizeof(RuntimeSummary));
    find_json_int_scoped(json,    "\"observed_functions\"", &r->observed_functions);
    find_json_int_scoped(json,    "\"peak_call_depth\"",    &r->peak_call_depth);
    find_json_uint64_scoped(json, "\"total_calls\"",        &r->total_calls);

    /* Pull top function (first entry in functions array — already sorted) */
    const char *arr = strstr(json, "\"functions\":");
    if (arr) {
        arr = strchr(arr, '[');
        if (arr) arr++;
        const char *obj = arr ? strchr(arr, '{') : NULL;
        if (obj) {
            char addr_str[32] = "";
            find_json_string_scoped(obj, "\"address\"", addr_str, sizeof(addr_str));
            if (addr_str[0])
                r->hot_addr = (uintptr_t)strtoull(addr_str, NULL, 16);
            find_json_uint64_scoped(obj, "\"calls\"", &r->hot_calls);
        }
    }
    r->valid = 1;
}

/* ── Integer percentage helper ───────────────────────────────────────── */

static int pct_change(int a, int b) {
    if (a == 0) return (b != 0) ? 100 : 0;
    return (b - a) * 100 / a;
}

static int pct_change_u64(uint64_t a, uint64_t b) {
    if (a == 0) return (b != 0) ? 100 : 0;
    int64_t delta = (int64_t)(b - a);
    return (int)(delta * 100 / (int64_t)a);
}

static int abs_int(int x) { return x < 0 ? -x : x; }

/* ── Attribution report ──────────────────────────────────────────────── */

typedef struct {
    /* Static drift */
    int register_drift_pct;
    int stack_drift_bytes;
    int instr_drift_pct;
    int topology_mutated;
    int call_instr_drift_pct;

    /* Runtime drift (if available) */
    int     has_runtime;
    int     call_volume_change_pct;
    int     depth_change_frames;
    int     hot_path_shifted;

    /* Scoring */
    int  impact_score;
    char estimated_impact[16];  /* NONE / LOW / MEDIUM / HIGH */

    /* Confidence vector */
    int      confidence_score;      /* 0 - 100 */
    char     confidence_class[16];  /* VERY_LOW / LOW / MEDIUM / HIGH / VERIFIED */
    int      confidence_coverage;   /* 0 - 100 */
    int      confidence_accuracy;   /* 0 - 100 */
    int      confidence_strength;   /* 0 - 100 */
    int      confidence_freshness;  /* 0 - 100 */
    double   calibration_age_hours;

    /* Narrative */
    char attribution[512];
} AttributionReport;

static void compute_attribution(const StaticGenome *ga, const StaticGenome *gb,
                                 const RuntimeSummary *ra, const RuntimeSummary *rb,
                                 const char *ver_a, const char *ver_b,
                                 AttributionReport *rpt,
                                 int t_reg, int t_stack, int t_instr,
                                 int t_call, int t_depth, int t_topo_instr, int t_hot_call) {
    memset(rpt, 0, sizeof(AttributionReport));

    /* ── Static drift ────────────────────────────────────────────────── */
    int reg_a = static_total_reg(ga);
    int reg_b = static_total_reg(gb);
    rpt->register_drift_pct = pct_change(reg_a, reg_b);

    rpt->stack_drift_bytes = gb->max_stack_frame - ga->max_stack_frame;

    int instr_a = static_total_instr(ga);
    int instr_b = static_total_instr(gb);
    rpt->instr_drift_pct = pct_change(instr_a, instr_b);

    rpt->call_instr_drift_pct = pct_change(ga->call_cnt, gb->call_cnt);

    rpt->topology_mutated =
        (ga->topology_root[0] && gb->topology_root[0] &&
         strcmp(ga->topology_root, gb->topology_root) != 0) ? 1 : 0;

    /* ── Runtime drift ───────────────────────────────────────────────── */
    rpt->has_runtime = (ra && ra->valid && rb && rb->valid) ? 1 : 0;
    if (rpt->has_runtime) {
        rpt->call_volume_change_pct =
            pct_change_u64(ra->total_calls, rb->total_calls);
        rpt->depth_change_frames =
            rb->peak_call_depth - ra->peak_call_depth;
        /* Hot-path shift: top function address changed between versions */
        rpt->hot_path_shifted =
            (ra->hot_addr != 0 && rb->hot_addr != 0 &&
             ra->hot_addr != rb->hot_addr) ? 1 : 0;
    }

    /* ── Impact scoring ──────────────────────────────────────────────── */
    int score = 0;
    if (abs_int(rpt->register_drift_pct)    > t_reg) score += 3;
    if (abs_int(rpt->stack_drift_bytes)     > t_stack) score += 3;
    if (abs_int(rpt->instr_drift_pct)       > t_instr) score += 2;
    if (rpt->topology_mutated && abs_int(rpt->instr_drift_pct) > t_topo_instr) score += 2;
    if (rpt->has_runtime) {
        if (abs_int(rpt->call_volume_change_pct) > t_call) score += 3;
        if (abs_int(rpt->depth_change_frames)    >  t_depth) score += 1;
        if (rpt->hot_path_shifted && abs_int(rpt->call_volume_change_pct) > t_hot_call) score += 1;
    }
    rpt->impact_score = score;

    if      (score == 0) strcpy(rpt->estimated_impact, "NONE");
    else if (score <= 2) strcpy(rpt->estimated_impact, "LOW");
    else if (score <= 5) strcpy(rpt->estimated_impact, "MEDIUM");
    else                 strcpy(rpt->estimated_impact, "HIGH");

    /* ── Narrative ───────────────────────────────────────────────────── */
    char buf[512];
    int off = 0;
    off += snprintf(buf + off, sizeof(buf) - off,
                    "Compiler transition %s->%s:", ver_a, ver_b);

    if (rpt->register_drift_pct != 0)
        off += snprintf(buf + off, sizeof(buf) - off,
                        " register pressure %+d%%,", rpt->register_drift_pct);
    if (rpt->stack_drift_bytes != 0)
        off += snprintf(buf + off, sizeof(buf) - off,
                        " stack frame %+d bytes,", rpt->stack_drift_bytes);
    if (rpt->instr_drift_pct != 0)
        off += snprintf(buf + off, sizeof(buf) - off,
                        " instruction volume %+d%%,", rpt->instr_drift_pct);
    if (rpt->topology_mutated)
        off += snprintf(buf + off, sizeof(buf) - off, " topology root mutated,");
    if (rpt->has_runtime) {
        if (rpt->call_volume_change_pct != 0)
            off += snprintf(buf + off, sizeof(buf) - off,
                            " runtime call volume %+d%%,",
                            rpt->call_volume_change_pct);
        if (rpt->depth_change_frames != 0)
            off += snprintf(buf + off, sizeof(buf) - off,
                            " call depth %+d frames,", rpt->depth_change_frames);
        if (rpt->hot_path_shifted)
            off += snprintf(buf + off, sizeof(buf) - off, " hot path shifted.");
    }
    /* Strip trailing comma if present */
    if (off > 0 && buf[off - 1] == ',') buf[--off] = '.';

    strncpy(rpt->attribution, buf, sizeof(rpt->attribution) - 1);
    rpt->attribution[sizeof(rpt->attribution) - 1] = '\0';
}

/* ── JSON emitter ────────────────────────────────────────────────────── */

static void emit_attribution_json(const char *out_path,
                                   const AttributionReport *rpt,
                                   const char *ver_a, const char *ver_b,
                                   const char *static_a_path,
                                   const char *static_b_path,
                                   const char *runtime_a_path,
                                   const char *runtime_b_path,
                                   int t_reg, int t_stack, int t_instr,
                                   int t_call, int t_depth, int t_topo_instr, int t_hot_call) {
    FILE *f = fopen(out_path, "w");
    if (!f) { fprintf(stderr, "warning: cannot write %s\n", out_path); return; }

    fprintf(f, "{\n");
    fprintf(f, "  \"schema\": \"zcc.impact_attribution.v1\",\n");
    fprintf(f, "  \"version_a\": \"%s\",\n", ver_a);
    fprintf(f, "  \"version_b\": \"%s\",\n", ver_b);
    fprintf(f, "  \"static_genome_a\": \"%s\",\n", static_a_path ? static_a_path : "");
    fprintf(f, "  \"static_genome_b\": \"%s\",\n", static_b_path ? static_b_path : "");
    if (runtime_a_path)
        fprintf(f, "  \"runtime_genome_a\": \"%s\",\n", runtime_a_path);
    if (runtime_b_path)
        fprintf(f, "  \"runtime_genome_b\": \"%s\",\n", runtime_b_path);
    fprintf(f, "  \"static_drift\": {\n");
    fprintf(f, "    \"register_drift_pct\": %d,\n",   rpt->register_drift_pct);
    fprintf(f, "    \"stack_drift_bytes\": %d,\n",    rpt->stack_drift_bytes);
    fprintf(f, "    \"instr_drift_pct\": %d,\n",      rpt->instr_drift_pct);
    fprintf(f, "    \"call_instr_drift_pct\": %d,\n", rpt->call_instr_drift_pct);
    fprintf(f, "    \"topology_mutated\": %s\n",
            rpt->topology_mutated ? "true" : "false");
    fprintf(f, "  },\n");
    if (rpt->has_runtime) {
        fprintf(f, "  \"runtime_drift\": {\n");
        fprintf(f, "    \"call_volume_change_pct\": %d,\n",
                rpt->call_volume_change_pct);
        fprintf(f, "    \"depth_change_frames\": %d,\n",
                rpt->depth_change_frames);
        fprintf(f, "    \"hot_path_shifted\": %s\n",
                rpt->hot_path_shifted ? "true" : "false");
        fprintf(f, "  },\n");
    }
    fprintf(f, "  \"thresholds\": {\n");
    fprintf(f, "    \"register_drift_pct\": %d,\n", t_reg);
    fprintf(f, "    \"stack_drift_bytes\": %d,\n", t_stack);
    fprintf(f, "    \"instr_drift_pct\": %d,\n", t_instr);
    fprintf(f, "    \"call_volume_change_pct\": %d,\n", t_call);
    fprintf(f, "    \"depth_change_frames\": %d,\n", t_depth);
    fprintf(f, "    \"topology_instr_drift_pct\": %d,\n", t_topo_instr);
    fprintf(f, "    \"hot_path_call_drift_pct\": %d\n", t_hot_call);
    fprintf(f, "  },\n");
    fprintf(f, "  \"impact_score\": %d,\n",          rpt->impact_score);
    fprintf(f, "  \"estimated_impact\": \"%s\",\n",  rpt->estimated_impact);
    fprintf(f, "  \"confidence\": {\n");
    fprintf(f, "    \"score\": %d,\n",                 rpt->confidence_score);
    fprintf(f, "    \"class\": \"%s\",\n",             rpt->confidence_class);
    fprintf(f, "    \"coverage\": %d,\n",              rpt->confidence_coverage);
    fprintf(f, "    \"accuracy\": %d,\n",              rpt->confidence_accuracy);
    fprintf(f, "    \"sample_strength\": %d,\n",       rpt->confidence_strength);
    fprintf(f, "    \"freshness\": %d,\n",             rpt->confidence_freshness);
    fprintf(f, "    \"calibration_age_hours\": %.2f\n", rpt->calibration_age_hours);
    fprintf(f, "  },\n");
    fprintf(f, "  \"attribution\": \"%s\"\n",        rpt->attribution);
    fprintf(f, "}\n");
    fclose(f);
}

/* ═══════════════════════════════════════════════════════════════════════ */

int main(int argc, char **argv) {
    const char *static_a_path  = NULL;
    const char *static_b_path  = NULL;
    const char *runtime_a_path = NULL;
    const char *runtime_b_path = NULL;
    const char *version_a      = "A";
    const char *version_b      = "B";
    const char *out_path       = NULL;
    const char *cal_report_path = NULL;
    const char *thresholds_path = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--static-a")  == 0 && i+1<argc) static_a_path  = argv[++i];
        else if (strcmp(argv[i], "--static-b")  == 0 && i+1<argc) static_b_path  = argv[++i];
        else if (strcmp(argv[i], "--runtime-a") == 0 && i+1<argc) runtime_a_path = argv[++i];
        else if (strcmp(argv[i], "--runtime-b") == 0 && i+1<argc) runtime_b_path = argv[++i];
        else if (strcmp(argv[i], "--version-a") == 0 && i+1<argc) version_a      = argv[++i];
        else if (strcmp(argv[i], "--version-b") == 0 && i+1<argc) version_b      = argv[++i];
        else if (strcmp(argv[i], "--out")        == 0 && i+1<argc) out_path       = argv[++i];
        else if (strcmp(argv[i], "--calibration-report") == 0 && i+1<argc) cal_report_path = argv[++i];
        else if (strcmp(argv[i], "--thresholds") == 0 && i+1<argc) thresholds_path = argv[++i];
    }

    if (!static_a_path || !static_b_path) {
        printf("Usage: zcc_impact_attribution \\\n"
               "  --static-a <genome_a.json> --static-b <genome_b.json> \\\n"
               "  [--runtime-a <runtime_a.json>] [--runtime-b <runtime_b.json>] \\\n"
               "  [--version-a <label>] [--version-b <label>] \\\n"
               "  [--calibration-report <calibration_report.json>] \\\n"
               "  [--out <report.json>]\n");
        return 2;
    }

    /* ── Load static genomes ──────────────────────────────────────────── */
    size_t sz_a = 0, sz_b = 0;
    uint8_t *data_a = load_file(static_a_path, &sz_a);
    uint8_t *data_b = load_file(static_b_path, &sz_b);

    if (!data_a) { fprintf(stderr, "error: cannot open %s\n", static_a_path); return 1; }
    if (!data_b) { free(data_a); fprintf(stderr, "error: cannot open %s\n", static_b_path); return 1; }

    StaticGenome ga, gb;
    parse_static_genome((const char *)data_a, &ga);
    parse_static_genome((const char *)data_b, &gb);
    free(data_a);
    free(data_b);

    /* ── Load runtime genomes (optional) ─────────────────────────────── */
    RuntimeSummary ra_s, rb_s;
    RuntimeSummary *ra = NULL, *rb = NULL;

    if (runtime_a_path) {
        size_t sz = 0;
        uint8_t *d = load_file(runtime_a_path, &sz);
        if (d) { parse_runtime_summary((const char *)d, &ra_s); free(d); ra = &ra_s; }
        else fprintf(stderr, "warning: cannot open runtime-a %s\n", runtime_a_path);
    }
    if (runtime_b_path) {
        size_t sz = 0;
        uint8_t *d = load_file(runtime_b_path, &sz);
        if (d) { parse_runtime_summary((const char *)d, &rb_s); free(d); rb = &rb_s; }
        else fprintf(stderr, "warning: cannot open runtime-b %s\n", runtime_b_path);
    }

    /* ── Load dynamic thresholds if provided ─────────────────────────── */
    int t_reg = 20;
    int t_stack = 64;
    int t_instr = 15;
    int t_call = 50;
    int t_depth = 3;
    int t_topo_instr = 10;
    int t_hot_call = 10;

    if (thresholds_path) {
        size_t thresh_sz = 0;
        uint8_t *thresh_data = load_file(thresholds_path, &thresh_sz);
        if (thresh_data) {
            find_json_int_scoped((const char *)thresh_data, "\"register_drift_pct\"", &t_reg);
            find_json_int_scoped((const char *)thresh_data, "\"stack_drift_bytes\"", &t_stack);
            find_json_int_scoped((const char *)thresh_data, "\"instr_drift_pct\"", &t_instr);
            find_json_int_scoped((const char *)thresh_data, "\"call_volume_change_pct\"", &t_call);
            find_json_int_scoped((const char *)thresh_data, "\"depth_change_frames\"", &t_depth);
            find_json_int_scoped((const char *)thresh_data, "\"topology_instr_drift_pct\"", &t_topo_instr);
            find_json_int_scoped((const char *)thresh_data, "\"hot_path_call_drift_pct\"", &t_hot_call);
            free(thresh_data);
        } else {
            fprintf(stderr, "warning: cannot open thresholds file %s, falling back to defaults.\n", thresholds_path);
        }
    }

    /* ── Compute attribution ──────────────────────────────────────────── */
    AttributionReport rpt;
    compute_attribution(&ga, &gb, ra, rb, version_a, version_b, &rpt,
                        t_reg, t_stack, t_instr, t_call, t_depth, t_topo_instr, t_hot_call);

    /* ── Compute confidence score ──────────────────────────────────────── */
    rpt.confidence_score = 40;
    strcpy(rpt.confidence_class, "LOW");
    rpt.confidence_coverage = 0;
    rpt.confidence_accuracy = 0;
    rpt.confidence_strength = 0;
    rpt.confidence_freshness = 0;
    rpt.calibration_age_hours = -1.0;

    if (cal_report_path) {
        size_t cal_sz = 0;
        uint8_t *cal_data = load_file(cal_report_path, &cal_sz);
        if (cal_data) {
            double f1 = 0.0, mean_err = 0.0;
            int total_cases = 0;
            char ts_str[64] = "";
            
            const char *metrics_block = strstr((const char *)cal_data, "\"metrics\"");
            if (metrics_block) {
                find_json_double_scoped(metrics_block, "\"f1_score\"", &f1);
                find_json_double_scoped(metrics_block, "\"mean_deviation_score\"", &mean_err);
                find_json_int_scoped(metrics_block, "\"total_cases\"", &total_cases);
            }
            find_json_string_scoped((const char *)cal_data, "\"timestamp\"", ts_str, sizeof(ts_str));
            free(cal_data);
            
            if (total_cases > 0 && ts_str[0]) {
                double coverage = (total_cases >= 5) ? 1.0 : (total_cases / 5.0);
                double accuracy = f1 * (1.0 - (mean_err / 10.0));
                if (accuracy < 0.0) accuracy = 0.0;
                double strength = total_cases / (total_cases + 0.5);
                
                time_t cal_time = parse_iso8601(ts_str);
                time_t cur_time = time(NULL);
                double diff_sec = difftime(cur_time, cal_time);
                double age_hours = diff_sec / 3600.0;
                if (age_hours < 0.0) age_hours = 0.0;
                
                double freshness = exp(-0.0044 * age_hours);
                if (freshness < 0.10) freshness = 0.10;
                if (freshness > 1.0) freshness = 1.0;
                
                double aggregate = coverage * accuracy * strength * freshness * 100.0;
                int score = (int)(aggregate + 0.5);
                if (score > 100) score = 100;
                if (score < 0) score = 0;
                
                rpt.confidence_score = score;
                rpt.confidence_coverage = (int)(coverage * 100 + 0.5);
                rpt.confidence_accuracy = (int)(accuracy * 100 + 0.5);
                rpt.confidence_strength = (int)(strength * 100 + 0.5);
                rpt.confidence_freshness = (int)(freshness * 100 + 0.5);
                rpt.calibration_age_hours = age_hours;
                
                if      (score <= 30) strcpy(rpt.confidence_class, "VERY_LOW");
                else if (score <= 55) strcpy(rpt.confidence_class, "LOW");
                else if (score <= 75) strcpy(rpt.confidence_class, "MEDIUM");
                else if (score <= 90) strcpy(rpt.confidence_class, "HIGH");
                else                  strcpy(rpt.confidence_class, "VERIFIED");
            }
        } else {
            fprintf(stderr, "warning: cannot open calibration-report %s, falling back to uncalibrated state.\n", cal_report_path);
        }
    }

    /* ── Print report ────────────────────────────────────────────────── */
    printf("=== ZCC Runtime Impact Attribution ===\n\n");
    printf("Transition:  %s  →  %s\n\n", version_a, version_b);

    printf("Static Drift:\n");
    printf("  Register Pressure:  %+d%%\n",  rpt.register_drift_pct);
    printf("  Stack Frame:        %+d bytes\n", rpt.stack_drift_bytes);
    printf("  Instruction Volume: %+d%%\n",  rpt.instr_drift_pct);
    printf("  Call Instructions:  %+d%%\n",  rpt.call_instr_drift_pct);
    printf("  Topology Mutated:   %s\n",
           rpt.topology_mutated ? "YES" : "NO");
    printf("\n");

    if (rpt.has_runtime) {
        printf("Runtime Drift:\n");
        printf("  Call Volume Change: %+d%%\n",   rpt.call_volume_change_pct);
        printf("  Depth Change:       %+d frames\n", rpt.depth_change_frames);
        printf("  Hot Path Shifted:   %s\n",
               rpt.hot_path_shifted ? "YES" : "NO");
        printf("\n");
    } else {
        printf("Runtime Drift:  [static-only mode — no runtime genomes provided]\n\n");
    }

    /* Per-dimension impact table */
    printf("Dimension Impact Table:\n");
    printf("  %-26s  %8s  %6s\n", "Dimension", "Value", "Weight");
    printf("  %-26s  %8s  %6s\n", "─────────────────────────",
           "────────", "──────");

    #define ROW(label, val_fmt, val, thresh, weight) do { \
        int v = (val); int t = (thresh); int w = (weight); \
        printf("  %-26s  " val_fmt "  %6s\n", label, v, \
               (abs_int(v) > t) ? (w >= 3 ? "+HIGH" : "+MED") : "-"); \
    } while(0)

    printf("  %-26s  %+7d%%  %6s\n", "Register Pressure",
           rpt.register_drift_pct,
           abs_int(rpt.register_drift_pct) > t_reg ? "+HIGH" : "-");
    printf("  %-26s  %+6d B  %6s\n", "Stack Frame",
           rpt.stack_drift_bytes,
           abs_int(rpt.stack_drift_bytes) > t_stack ? "+HIGH" : "-");
    printf("  %-26s  %+7d%%  %6s\n", "Instruction Volume",
           rpt.instr_drift_pct,
           abs_int(rpt.instr_drift_pct) > t_instr ? "+MED" : "-");
    printf("  %-26s  %8s  %6s\n", "Topology Root",
           rpt.topology_mutated ? "CHANGED" : "stable",
           (rpt.topology_mutated && abs_int(rpt.instr_drift_pct) > t_topo_instr) ? "+MED" : "-");
    if (rpt.has_runtime) {
        printf("  %-26s  %+7d%%  %6s\n", "Runtime Call Volume",
               rpt.call_volume_change_pct,
               abs_int(rpt.call_volume_change_pct) > t_call ? "+HIGH" : "-");
        printf("  %-26s  %+6d fr  %6s\n", "Call Depth",
               rpt.depth_change_frames,
               abs_int(rpt.depth_change_frames) > t_depth ? "+LOW" : "-");
        printf("  %-26s  %8s  %6s\n", "Hot Path",
               rpt.hot_path_shifted ? "SHIFTED" : "stable",
               (rpt.hot_path_shifted && abs_int(rpt.call_volume_change_pct) > t_hot_call) ? "+LOW" : "-");
    }
    printf("\n");

    printf("Impact Score:   %d\n", rpt.impact_score);
    printf("Estimated Impact: %s\n\n", rpt.estimated_impact);
    printf("Confidence Score: %d%% (%s)\n", rpt.confidence_score, rpt.confidence_class);
    if (rpt.calibration_age_hours >= 0.0) {
        printf("  Coverage:       %d%%\n", rpt.confidence_coverage);
        printf("  Accuracy:       %d%%\n", rpt.confidence_accuracy);
        printf("  Strength:       %d%%\n", rpt.confidence_strength);
        printf("  Freshness:      %d%% (Age: %.1f hours)\n", rpt.confidence_freshness, rpt.calibration_age_hours);
    } else {
        printf("  State:          UNCALIBRATED\n");
    }
    printf("\n");
    printf("Attribution:\n  %s\n", rpt.attribution);

    /* ── Optional JSON output ─────────────────────────────────────────── */
    if (out_path) {
        emit_attribution_json(out_path, &rpt, version_a, version_b,
                              static_a_path, static_b_path,
                              runtime_a_path, runtime_b_path,
                              t_reg, t_stack, t_instr, t_call, t_depth, t_topo_instr, t_hot_call);
        printf("\nAttribution report written to: %s\n", out_path);
    }

    /* Exit non-zero if impact is HIGH so CI pipelines can gate on it */
    if (strcmp(rpt.estimated_impact, "HIGH") == 0) return 2;
    if (strcmp(rpt.estimated_impact, "MEDIUM") == 0) return 1;
    return 0;
}
