/*
 * zcc_stability_observatory.c — D-24: Compiler Stability Observatory
 *
 * Reads a genomes directory produced by zcc_genome_history, computes
 * per-version stability scores, fits a linear trend line, and emits a
 * forecast report: predicted stability score, growth trend, convergence
 * confidence, and regression probability for the next N versions.
 *
 * Memory discipline: all buffers are static or malloc/free paired.
 * No phantom closures: every malloc() has a matching free() before exit.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <math.h>

#define MAX_GENOMES 64
#define STABILITY_MAX 100

/* ── JSON helpers (same pattern as sibling tools) ─────────────────────── */

static void die(const char *msg) {
    fprintf(stderr, "zcc_stability_observatory: fatal: %s\n", msg);
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

/* ── Genome data (mirrors genome_history layout) ──────────────────────── */

typedef struct {
    int mov_cnt, call_cnt, lea_cnt, cmp_cnt, jmp_cnt, ret_cnt;
    int reg_counts[16];
    int max_stack_frame;
    int average_stack_frame;
    int reachable_functions;
    int leaf_functions;
    int branch_nodes;
    char topology_root[65];
    char controlflow_root[65];
} GenomeData;

typedef struct {
    char version[32];
    int  major, minor, patch;
    GenomeData data;
    /* computed per-version stability score [0..100] */
    int stability_score;
} GenomeRecord;

static void parse_genome(const char *json, GenomeData *g) {
    memset(g, 0, sizeof(GenomeData));
    const char *telemetry   = strstr(json, "\"telemetry\"");
    const char *fingerprint = strstr(json, "\"execution_fingerprint\"");
    const char *instruction = strstr(json, "\"instruction_profile\"");
    const char *regblock    = strstr(json, "\"register_profile\"");
    const char *stackblock  = strstr(json, "\"stack_analysis\"");
    const char *merkle      = strstr(json, "\"merkle_topology\"");

    if (fingerprint) {
        find_json_string_scoped(fingerprint, "\"controlflow_root\"", g->controlflow_root, 65);
        find_json_int_scoped(fingerprint, "\"reachable_functions\"", &g->reachable_functions);
        find_json_int_scoped(fingerprint, "\"leaf_functions\"",      &g->leaf_functions);
        find_json_int_scoped(fingerprint, "\"branch_nodes\"",        &g->branch_nodes);
    }
    if (merkle) {
        find_json_string_scoped(merkle, "\"topology_root\"", g->topology_root, 65);
    }
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
    (void)telemetry; /* not needed for forecast, suppress unused-var */
}

static int parse_version(const char *name, char *ver_out,
                          int *maj, int *min, int *pat) {
    *maj = *min = *pat = 0;
    if (name[0] != 'v') return 0;
    const char *ext = strstr(name, ".json");
    if (!ext) return 0;
    int vlen = (int)(ext - name);
    if (vlen > 31) vlen = 31;
    strncpy(ver_out, name, vlen);
    ver_out[vlen] = '\0';
    int f = sscanf(ver_out, "v%d.%d.%d", maj, min, pat);
    if (f < 2) { *pat = 0; f = sscanf(ver_out, "v%d.%d", maj, min); }
    return f >= 2;
}

static int cmp_records(const void *a, const void *b) {
    const GenomeRecord *ra = (const GenomeRecord *)a;
    const GenomeRecord *rb = (const GenomeRecord *)b;
    if (ra->major != rb->major) return ra->major - rb->major;
    if (ra->minor != rb->minor) return ra->minor - rb->minor;
    return ra->patch - rb->patch;
}

/* ── Stability score for a single genome ──────────────────────────────── */
/*
 * Heuristic scoring:
 *   Base = 100
 *   -1  per 50 excess mov instructions (above 200 baseline)
 *   -1  per 8 bytes max stack above 256
 *   -2  if no controlflow_root (fingerprint absent)
 *   -1  per 5 units of total register pressure above 100
 *   floor at 0, cap at 100.
 */
static int compute_stability_score(const GenomeData *g) {
    int score = STABILITY_MAX;

    int excess_mov = g->mov_cnt - 200;
    if (excess_mov > 0) score -= (excess_mov / 50);

    int excess_stk = g->max_stack_frame - 256;
    if (excess_stk > 0) score -= (excess_stk / 8);

    if (g->controlflow_root[0] == '\0') score -= 2;

    int total_reg = 0;
    for (int i = 0; i < 16; i++) total_reg += g->reg_counts[i];
    int excess_reg = total_reg - 100;
    if (excess_reg > 0) score -= (excess_reg / 5);

    if (score < 0)   score = 0;
    if (score > 100) score = 100;
    return score;
}

/* ── Linear regression helpers (integer-friendly) ─────────────────────── */
/*
 * Given n (x,y) pairs, computes slope * 1000 and intercept * 1000
 * using integer arithmetic to avoid libm dependency.
 * slope_1000 and intercept_1000 are scaled by 1000 for fixed-point.
 */
static void linear_fit(const int *y, int n, int *slope_1000, int *intercept_1000) {
    if (n < 2) { *slope_1000 = 0; *intercept_1000 = (n == 1) ? y[0] * 1000 : 0; return; }

    /* x[i] = i (version index 0..n-1) */
    long long sum_x = 0, sum_y = 0, sum_xx = 0, sum_xy = 0;
    for (int i = 0; i < n; i++) {
        sum_x  += i;
        sum_y  += y[i];
        sum_xx += (long long)i * i;
        sum_xy += (long long)i * y[i];
    }

    long long denom = (long long)n * sum_xx - sum_x * sum_x;
    if (denom == 0) { *slope_1000 = 0; *intercept_1000 = (int)(sum_y * 1000 / n); return; }

    long long num_slope     = (long long)n * sum_xy - sum_x * sum_y;
    long long num_intercept = sum_y * sum_xx - sum_x * sum_xy;

    *slope_1000     = (int)(num_slope     * 1000 / denom);
    *intercept_1000 = (int)(num_intercept * 1000 / denom);
}

/* ── Forecast: predict score at version index `at_idx` ────────────────── */
static int forecast_score(int slope_1000, int intercept_1000, int at_idx) {
    int predicted = (intercept_1000 + slope_1000 * at_idx) / 1000;
    if (predicted < 0)   predicted = 0;
    if (predicted > 100) predicted = 100;
    return predicted;
}

/* ── Convergence confidence ───────────────────────────────────────────── */
/*
 * Confidence is derived from:
 *   - stability score trend direction (positive = growing confidence)
 *   - variance in last 3 scores (low variance = high confidence)
 * Returns 0..100.
 */
static int convergence_confidence(const int *scores, int n, int slope_1000) {
    int conf = 50; /* neutral baseline */

    /* Positive slope boosts confidence, negative degrades */
    if (slope_1000 > 500)       conf += 20;
    else if (slope_1000 > 0)    conf += 10;
    else if (slope_1000 < -500) conf -= 20;
    else if (slope_1000 < 0)    conf -= 10;

    /* Low variance in last 3 versions further boosts */
    if (n >= 3) {
        int s0 = scores[n-3], s1 = scores[n-2], s2 = scores[n-1];
        int mean = (s0 + s1 + s2) / 3;
        int var  = ((s0-mean)*(s0-mean) + (s1-mean)*(s1-mean) + (s2-mean)*(s2-mean)) / 3;
        if (var < 4)       conf += 20;
        else if (var < 16) conf += 10;
        else if (var > 64) conf -= 10;
    }

    if (conf < 0)   conf = 0;
    if (conf > 100) conf = 100;
    return conf;
}

/* ── Regression probability estimate ─────────────────────────────────── */
/*
 * P(regression) = inverse of convergence confidence, adjusted for trend.
 * Returns 0..100 (%).
 */
static int regression_probability(int conf, int slope_1000) {
    int base = 100 - conf;
    if (slope_1000 < -200) base += 10;  /* falling trend raises risk */
    if (slope_1000 >  200) base -= 10;  /* rising trend lowers risk  */
    if (base < 0)   base = 0;
    if (base > 100) base = 100;
    return base;
}

/* ── Emit JSON forecast report ────────────────────────────────────────── */
static void emit_forecast_json(const char *outpath,
                               const GenomeRecord *records, int n,
                               const int *scores,
                               int slope_1000, int intercept_1000,
                               int forecast_horizon,
                               int conf, int reg_prob) {
    FILE *f = fopen(outpath, "w");
    if (!f) { fprintf(stderr, "warning: cannot write forecast to %s\n", outpath); return; }

    fprintf(f, "{\n");
    fprintf(f, "  \"schema\": \"zcc.stability_forecast.v1\",\n");
    fprintf(f, "  \"versions_analyzed\": %d,\n", n);
    fprintf(f, "  \"slope_per_version\": %.3f,\n", slope_1000 / 1000.0);
    fprintf(f, "  \"convergence_confidence\": %d,\n", conf);
    fprintf(f, "  \"regression_probability_pct\": %d,\n", reg_prob);
    fprintf(f, "  \"history\": [\n");
    for (int i = 0; i < n; i++) {
        fprintf(f, "    { \"version\": \"%s\", \"stability_score\": %d }%s\n",
                records[i].version, scores[i], (i == n-1) ? "" : ",");
    }
    fprintf(f, "  ],\n");
    fprintf(f, "  \"forecast\": [\n");
    for (int i = 0; i < forecast_horizon; i++) {
        int at = n + i;
        int predicted = forecast_score(slope_1000, intercept_1000, at);
        fprintf(f, "    { \"version_index\": %d, \"predicted_score\": %d }%s\n",
                at, predicted, (i == forecast_horizon-1) ? "" : ",");
    }
    fprintf(f, "  ]\n");
    fprintf(f, "}\n");
    fclose(f);
}

/* ═══════════════════════════════════════════════════════════════════════ */

int main(int argc, char **argv) {
    const char *dir_path        = NULL;
    const char *out_path        = NULL;
    int         forecast_horizon = 3;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--out") == 0 && i + 1 < argc)
            out_path = argv[++i];
        else if (strcmp(argv[i], "--forecast") == 0 && i + 1 < argc)
            forecast_horizon = atoi(argv[++i]);
        else if (argv[i][0] != '-')
            dir_path = argv[i];
    }

    if (!dir_path) {
        printf("Usage: zcc_stability_observatory <genomes_dir> [--out forecast.json] [--forecast N]\n");
        return 2;
    }

    DIR *d = opendir(dir_path);
    if (!d) { fprintf(stderr, "error: cannot open %s\n", dir_path); return 2; }

    GenomeRecord records[MAX_GENOMES];
    int count = 0;

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        char ver[32];
        int maj = 0, min = 0, pat = 0;
        if (!parse_version(entry->d_name, ver, &maj, &min, &pat)) continue;
        if (count >= MAX_GENOMES) break;

        GenomeRecord *r = &records[count];
        strcpy(r->version, ver);
        r->major = maj; r->minor = min; r->patch = pat;

        char filepath[640];
        snprintf(filepath, sizeof(filepath), "%s/%s", dir_path, entry->d_name);
        size_t sz = 0;
        uint8_t *content = load_file(filepath, &sz);
        if (!content) { fprintf(stderr, "warning: cannot read %s\n", filepath); continue; }
        parse_genome((const char *)content, &r->data);
        free(content);
        count++;
    }
    closedir(d);

    if (count == 0) {
        fprintf(stderr, "error: no v*.json genomes found in %s\n", dir_path);
        return 1;
    }

    qsort(records, count, sizeof(GenomeRecord), cmp_records);

    /* Compute per-version stability scores */
    int scores[MAX_GENOMES];
    for (int i = 0; i < count; i++) {
        scores[i] = compute_stability_score(&records[i].data);
        records[i].stability_score = scores[i];
    }

    /* Linear trend fit */
    int slope_1000 = 0, intercept_1000 = 0;
    linear_fit(scores, count, &slope_1000, &intercept_1000);

    int conf     = convergence_confidence(scores, count, slope_1000);
    int reg_prob = regression_probability(conf, slope_1000);

    /* ── Print report ─────────────────────────────────────────────────── */
    printf("=== ZCC Compiler Stability Observatory ===\n");
    printf("Registry: %s\n", dir_path);
    printf("Versions Analyzed: %d\n\n", count);

    printf("Stability History:\n");
    for (int i = 0; i < count; i++) {
        printf("  %-8s  score=%3d  stack=%4d  mov=%4d  regs=%d\n",
               records[i].version,
               scores[i],
               records[i].data.max_stack_frame,
               records[i].data.mov_cnt,
               records[i].data.reachable_functions);
    }
    printf("\n");

    double slope_d = slope_1000 / 1000.0;
    const char *trend_label =
        (slope_1000 > 200)  ? "IMPROVING" :
        (slope_1000 < -200) ? "DECLINING" : "STABLE";

    printf("Trend Analysis:\n");
    printf("  Slope (per version):     %+.3f (%s)\n", slope_d, trend_label);
    printf("  Convergence Confidence:  %d%%\n", conf);
    printf("  Regression Probability:  %d%%\n\n", reg_prob);

    printf("Forecast (next %d versions):\n", forecast_horizon);
    for (int i = 0; i < forecast_horizon; i++) {
        int at        = count + i;
        int predicted = forecast_score(slope_1000, intercept_1000, at);
        const char *risk =
            (predicted < 60) ? "HIGH" :
            (predicted < 80) ? "MEDIUM" : "LOW";
        printf("  v+%d  predicted_score=%3d  risk=%s\n", i + 1, predicted, risk);
    }
    printf("\n");

    /* Current risk verdict */
    const char *verdict =
        (reg_prob >= 40) ? "REGRESSION LIKELY" :
        (reg_prob >= 20) ? "MONITOR" : "STABLE";
    printf("Observatory Verdict: %s\n", verdict);

    /* Optional JSON output */
    if (out_path) {
        emit_forecast_json(out_path, records, count, scores,
                           slope_1000, intercept_1000,
                           forecast_horizon, conf, reg_prob);
        printf("Forecast written to: %s\n", out_path);
    }

    return 0;
}
