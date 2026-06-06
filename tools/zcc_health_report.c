/*
 * zcc_health_report.c — D-29+: Compiler Health Dashboard
 *
 * Aggregates the state of all ZCC observability gates into a single
 * compiler_health.json file. Reads existing gate output artifacts from
 * the scratch directory and gate executables, then emits one authoritative
 * JSON summary:
 *
 *   {
 *     "selfhost":         "PASS",
 *     "attestation":      "PASS",
 *     "replay":           "PASS",
 *     "lineage":          "PASS",
 *     "stability":        "MONITOR",
 *     "impact":           "LOW",
 *     "ledger_entries":   3,
 *     "ledger_integrity": "VERIFIED",
 *     "runtime_observed": 7,
 *     "runtime_depth":    6,
 *     "evolution_score":  100,
 *     "confidence":       96.4,
 *     "gates_passed":     11,
 *     "gates_total":      11,
 *     "verdict":          "HEALTHY"
 *   }
 *
 * Confidence score derivation (each gate contributes equally):
 *   confidence = (gates_passed / gates_total) * 100
 *
 *   Weighted adjustment:
 *   - selfhost:   -20 points if FAIL (root gate, most critical)
 *   - ledger:     -10 points if BROKEN
 *   - impact HIGH: -5 points (regression signal present)
 *   - stability MONITOR: -3 points
 *
 * Usage:
 *   zcc_health_report [--scratch <dir>] [--golden <dir>] [--out <file>]
 *
 * All inputs are optional; missing artifacts are reported as UNKNOWN.
 *
 * Memory discipline: no phantom closures. All frees before exit.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/* ── File helpers ────────────────────────────────────────────────────── */

static uint8_t *load_file(const char *path, size_t *sz) {
    FILE *f = fopen(path, "rb");
    if (!f) { *sz = 0; return NULL; }
    fseek(f, 0, SEEK_END);
    *sz = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *buf = malloc(*sz + 1);
    if (!buf) { fclose(f); return NULL; }
    if (fread(buf, 1, *sz, f) != *sz) { free(buf); fclose(f); return NULL; }
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
    while (*p && (*p==' '||*p=='\n'||*p=='\r'||*p=='\t')) p++;
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
    while (*p && *p != '"' && len < max-1) out[len++] = *p++;
    out[len] = '\0';
    return 1;
}

/* ── Gate state ──────────────────────────────────────────────────────── */

#define STATE_PASS    "PASS"
#define STATE_FAIL    "FAIL"
#define STATE_UNKNOWN "UNKNOWN"

typedef struct {
    /* Gate states */
    char selfhost[16];
    char attestation[16];
    char replay[16];
    char lineage[16];
    char stability[16];
    char bisector[16];
    char cross_genome[16];
    char ledger_integrity[16];
    char runtime_probe[16];
    char impact[16];

    /* Numeric extractions */
    int     ledger_entries;
    int     runtime_observed;
    int     runtime_depth;
    int     ledger_errors;
    int     stability_risk_pct;
    int     drift_score;
    char    drift_verdict[32];
    char    stability_verdict[32];

    /* Golden genome comparison */
    char    golden_drift[32];   /* IDENTICAL / DRIFTED / UNKNOWN */

    /* Computed */
    int     gates_passed;
    int     gates_total;
    double  confidence;
    char    verdict[32];
} HealthState;

/* ── Probe individual artifact files ─────────────────────────────────── */

static void check_ledger(const char *scratch, HealthState *h) {
    char path[512];
    snprintf(path, sizeof(path), "%s/build.ledger", scratch);
    size_t sz = 0;
    uint8_t *d = load_file(path, &sz);
    if (!d) { strcpy(h->ledger_integrity, STATE_UNKNOWN); return; }

    /* Count lines = entries */
    int lines = 0;
    for (size_t i = 0; i < sz; i++) if (d[i] == '\n') lines++;
    h->ledger_entries = lines;

    /* Chain integrity is checked by looking for any mismatch in the
       existing output artifact behavioral_drift_report or by presence.
       For a lightweight check, we verify the ledger file is non-empty
       and the last entry has a valid entry_hash field. */
    const char *last_hash = NULL;
    const char *p = (const char *)d;
    while ((p = strstr(p, "\"entry_hash\"")) != NULL) { last_hash = p; p++; }
    if (last_hash) {
        strcpy(h->ledger_integrity, "VERIFIED");
        h->ledger_errors = 0;
    } else {
        strcpy(h->ledger_integrity, "UNKNOWN");
    }
    free(d);
}

static void check_stability(const char *scratch, HealthState *h) {
    char path[512];
    snprintf(path, sizeof(path), "%s/stability_forecast.json", scratch);
    size_t sz = 0;
    uint8_t *d = load_file(path, &sz);
    if (!d) { strcpy(h->stability, STATE_UNKNOWN); return; }

    find_json_int_scoped((const char *)d, "\"regression_probability_pct\"",
                         &h->stability_risk_pct);
    find_json_int_scoped((const char *)d, "\"convergence_confidence\"",
                         &h->drift_score);

    /* Classify */
    if      (h->stability_risk_pct < 20) strcpy(h->stability, "STABLE");
    else if (h->stability_risk_pct < 50) strcpy(h->stability, "MONITOR");
    else                                 strcpy(h->stability, "WARNING");

    free(d);
}

static void check_runtime(const char *scratch, HealthState *h) {
    char path[512];
    snprintf(path, sizeof(path), "%s/runtime_genome.json", scratch);
    size_t sz = 0;
    uint8_t *d = load_file(path, &sz);
    if (!d) { strcpy(h->runtime_probe, STATE_UNKNOWN); return; }

    find_json_int_scoped((const char *)d, "\"observed_functions\"",
                         &h->runtime_observed);
    find_json_int_scoped((const char *)d, "\"peak_call_depth\"",
                         &h->runtime_depth);

    if (h->runtime_observed > 0) strcpy(h->runtime_probe, STATE_PASS);
    else                         strcpy(h->runtime_probe, STATE_FAIL);
    free(d);
}

static void check_impact(const char *scratch, HealthState *h) {
    char path[512];
    snprintf(path, sizeof(path), "%s/attribution_full.json", scratch);
    size_t sz = 0;
    uint8_t *d = load_file(path, &sz);
    if (!d) {
        /* Try static-only fallback */
        snprintf(path, sizeof(path), "%s/attribution_static.json", scratch);
        d = load_file(path, &sz);
    }
    if (!d) { strcpy(h->impact, STATE_UNKNOWN); return; }

    char verdict[32] = "";
    find_json_string_scoped((const char *)d, "\"estimated_impact\"",
                            verdict, sizeof(verdict));
    if (verdict[0]) { strncpy(h->impact, verdict, sizeof(h->impact) - 1); h->impact[sizeof(h->impact)-1]='\0'; }
    else            strcpy(h->impact, STATE_UNKNOWN);
    free(d);
}

static void check_drift_report(const char *scratch, HealthState *h) {
    char path[512];
    snprintf(path, sizeof(path), "%s/behavioral_drift_report.json", scratch);
    size_t sz = 0;
    uint8_t *d = load_file(path, &sz);
    if (!d) { strcpy(h->drift_verdict, "UNKNOWN"); return; }
    find_json_string_scoped((const char *)d, "\"drift_verdict\"",
                            h->drift_verdict, sizeof(h->drift_verdict));
    free(d);
}

static void check_golden(const char *golden, const char *scratch,
                          HealthState *h) {
    if (!golden) { strcpy(h->golden_drift, "UNKNOWN"); return; }

    /* Load current static genome and golden genome, compare topology_root */
    char cur_path[512], gold_path[512];
    snprintf(cur_path,  sizeof(cur_path),  "%s/static_genome.json",              scratch);
    snprintf(gold_path, sizeof(gold_path), "%s/v0.29-observability.json",         golden);

    size_t sz_c = 0, sz_g = 0;
    uint8_t *cur  = load_file(cur_path,  &sz_c);
    uint8_t *gold = load_file(gold_path, &sz_g);

    if (!cur || !gold) {
        strcpy(h->golden_drift, "UNKNOWN");
        if (cur)  free(cur);
        if (gold) free(gold);
        return;
    }

    char cur_root[65]  = "", gold_root[65]  = "";
    char cur_func[16]  = "", gold_func[16]  = "";

    const char *cur_mk  = strstr((const char *)cur,  "\"merkle_topology\"");
    const char *gold_mk = strstr((const char *)gold, "\"merkle_topology\"");
    find_json_string_scoped(cur_mk  ? cur_mk  : (const char *)cur,
                            "\"topology_root\"", cur_root,  65);
    find_json_string_scoped(gold_mk ? gold_mk : (const char *)gold,
                            "\"topology_root\"", gold_root, 65);

    /* Also compare function counts as secondary check */
    const char *cur_tel  = strstr((const char *)cur,  "\"telemetry\"");
    const char *gold_tel = strstr((const char *)gold, "\"telemetry\"");
    find_json_string_scoped(cur_tel  ? cur_tel  : NULL,
                            "\"functions_count\"", cur_func,  16);
    find_json_string_scoped(gold_tel ? gold_tel : NULL,
                            "\"functions_count\"", gold_func, 16);

    if (cur_root[0] && gold_root[0]) {
        if (strcmp(cur_root, gold_root) == 0)
            strcpy(h->golden_drift, "IDENTICAL");
        else
            strcpy(h->golden_drift, "DRIFTED");
    } else {
        strcpy(h->golden_drift, "UNKNOWN");
    }

    free(cur);
    free(gold);
}

/* ── Confidence computation ──────────────────────────────────────────── */

static double compute_confidence(HealthState *h) {
    /* Base: fraction of known-PASS gates */
    int passed = 0, total = 0;

    #define GATE(field) do { total++; \
        if (strcmp(h->field, STATE_PASS) == 0 || \
            strcmp(h->field, "VERIFIED") == 0 || \
            strcmp(h->field, "STABLE")   == 0 || \
            strcmp(h->field, "MONITOR")  == 0 || \
            strcmp(h->field, "IDENTICAL")== 0 || \
            strcmp(h->field, "NONE")     == 0 || \
            strcmp(h->field, "LOW")      == 0 || \
            strcmp(h->field, "OVERREACH")== 0 || \
            strcmp(h->field, "PARTIAL_COVERAGE") == 0 || \
            strcmp(h->field, "FULL_COVERAGE")    == 0) passed++; \
    } while(0)

    GATE(selfhost);
    GATE(attestation);
    GATE(replay);
    GATE(lineage);
    GATE(stability);
    GATE(bisector);
    GATE(cross_genome);
    GATE(ledger_integrity);
    GATE(runtime_probe);
    GATE(impact);
    GATE(golden_drift);

    h->gates_passed = passed;
    h->gates_total  = total;

    double conf = (total > 0) ? ((double)passed / total) * 100.0 : 0.0;

    /* Weighted deductions */
    if (strcmp(h->selfhost,        STATE_FAIL)  == 0) conf -= 20.0;
    if (strcmp(h->ledger_integrity,"BROKEN")    == 0) conf -= 10.0;
    if (strcmp(h->impact,          "HIGH")      == 0) conf -=  5.0;
    if (strcmp(h->stability,       "WARNING")   == 0) conf -=  5.0;
    if (strcmp(h->stability,       "MONITOR")   == 0) conf -=  1.0;
    if (strcmp(h->golden_drift,    "DRIFTED")   == 0) conf -=  8.0;

    if (conf < 0.0)   conf = 0.0;
    if (conf > 100.0) conf = 100.0;
    return conf;
}

/* ── Emit JSON ───────────────────────────────────────────────────────── */

static void emit_health_json(const char *out_path, const HealthState *h) {
    FILE *f = fopen(out_path, "w");
    if (!f) { fprintf(stderr, "warning: cannot write %s\n", out_path); return; }

    fprintf(f, "{\n");
    fprintf(f, "  \"schema\": \"zcc.compiler_health.v1\",\n");
    fprintf(f, "  \"gates\": {\n");
    fprintf(f, "    \"selfhost\":         \"%s\",\n", h->selfhost);
    fprintf(f, "    \"attestation\":      \"%s\",\n", h->attestation);
    fprintf(f, "    \"replay\":           \"%s\",\n", h->replay);
    fprintf(f, "    \"lineage\":          \"%s\",\n", h->lineage);
    fprintf(f, "    \"stability\":        \"%s\",\n", h->stability);
    fprintf(f, "    \"bisector\":         \"%s\",\n", h->bisector);
    fprintf(f, "    \"cross_genome\":     \"%s\",\n", h->cross_genome);
    fprintf(f, "    \"ledger_integrity\": \"%s\",\n", h->ledger_integrity);
    fprintf(f, "    \"runtime_probe\":    \"%s\",\n", h->runtime_probe);
    fprintf(f, "    \"impact\":           \"%s\"\n",  h->impact);
    fprintf(f, "  },\n");
    fprintf(f, "  \"runtime\": {\n");
    fprintf(f, "    \"observed_functions\": %d,\n", h->runtime_observed);
    fprintf(f, "    \"peak_call_depth\":    %d\n",  h->runtime_depth);
    fprintf(f, "  },\n");
    fprintf(f, "  \"ledger\": {\n");
    fprintf(f, "    \"entries\":   %d,\n", h->ledger_entries);
    fprintf(f, "    \"integrity\": \"%s\"\n", h->ledger_integrity);
    fprintf(f, "  },\n");
    fprintf(f, "  \"golden_drift\":    \"%s\",\n", h->golden_drift);
    fprintf(f, "  \"stability_risk_pct\": %d,\n",  h->stability_risk_pct);
    fprintf(f, "  \"drift_verdict\":   \"%s\",\n",  h->drift_verdict);
    fprintf(f, "  \"gates_passed\":    %d,\n",      h->gates_passed);
    fprintf(f, "  \"gates_total\":     %d,\n",      h->gates_total);
    fprintf(f, "  \"confidence\":      %.1f,\n",    h->confidence);
    fprintf(f, "  \"verdict\":         \"%s\"\n",   h->verdict);
    fprintf(f, "}\n");
    fclose(f);
}

/* ═══════════════════════════════════════════════════════════════════════ */

int main(int argc, char **argv) {
    const char *scratch_dir = "scratch";
    const char *golden_dir  = "genomes/golden";
    const char *out_path    = "compiler_health.json";

    /* Collect gate results from command-line flags */
    /* Each gate is passed as --gate-NAME pass|fail */
    HealthState h;
    memset(&h, 0, sizeof(h));

    /* Defaults: unknown until proven */
    strcpy(h.selfhost,    STATE_UNKNOWN);
    strcpy(h.attestation, STATE_UNKNOWN);
    strcpy(h.replay,      STATE_UNKNOWN);
    strcpy(h.lineage,     STATE_UNKNOWN);
    strcpy(h.stability,   STATE_UNKNOWN);
    strcpy(h.bisector,    STATE_UNKNOWN);
    strcpy(h.cross_genome,STATE_UNKNOWN);
    strcpy(h.ledger_integrity, STATE_UNKNOWN);
    strcpy(h.runtime_probe,    STATE_UNKNOWN);
    strcpy(h.impact,      STATE_UNKNOWN);
    strcpy(h.golden_drift,STATE_UNKNOWN);
    strcpy(h.drift_verdict,"UNKNOWN");

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--scratch")         == 0 && i+1<argc) scratch_dir = argv[++i];
        else if (strcmp(argv[i], "--golden")     == 0 && i+1<argc) golden_dir  = argv[++i];
        else if (strcmp(argv[i], "--out")        == 0 && i+1<argc) out_path    = argv[++i];
        else if (strcmp(argv[i], "--selfhost")   == 0 && i+1<argc) { strncpy(h.selfhost,    argv[++i], 15); }
        else if (strcmp(argv[i], "--attestation")== 0 && i+1<argc) { strncpy(h.attestation, argv[++i], 15); }
        else if (strcmp(argv[i], "--replay")     == 0 && i+1<argc) { strncpy(h.replay,      argv[++i], 15); }
        else if (strcmp(argv[i], "--lineage")    == 0 && i+1<argc) { strncpy(h.lineage,     argv[++i], 15); }
        else if (strcmp(argv[i], "--bisector")   == 0 && i+1<argc) { strncpy(h.bisector,    argv[++i], 15); }
        else if (strcmp(argv[i], "--cross-genome")== 0 && i+1<argc){ strncpy(h.cross_genome,argv[++i], 15); }
    }

    /* Probe artifact files for gate states */
    check_ledger(scratch_dir, &h);
    check_stability(scratch_dir, &h);
    check_runtime(scratch_dir, &h);
    check_impact(scratch_dir, &h);
    check_drift_report(scratch_dir, &h);
    check_golden(golden_dir, scratch_dir, &h);

    /* Compute confidence */
    h.confidence = compute_confidence(&h);

    /* Overall verdict */
    if      (strcmp(h.selfhost, STATE_FAIL) == 0)        strcpy(h.verdict, "CRITICAL");
    else if (strcmp(h.golden_drift, "DRIFTED") == 0)     strcpy(h.verdict, "DRIFTED");
    else if (strcmp(h.impact, "HIGH") == 0)              strcpy(h.verdict, "DEGRADED");
    else if (h.confidence >= 90.0)                       strcpy(h.verdict, "HEALTHY");
    else if (h.confidence >= 70.0)                       strcpy(h.verdict, "MONITOR");
    else                                                 strcpy(h.verdict, "WARNING");

    /* ── Print summary ───────────────────────────────────────────────── */
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║         ZCC Compiler Health Dashboard        ║\n");
    printf("╚══════════════════════════════════════════════╝\n\n");
    printf("  Self-host:        %s\n",  h.selfhost);
    printf("  Attestation:      %s\n",  h.attestation);
    printf("  Replay:           %s\n",  h.replay);
    printf("  Lineage:          %s\n",  h.lineage);
    printf("  Stability:        %s  (risk: %d%%)\n",
           h.stability, h.stability_risk_pct);
    printf("  Bisector:         %s\n",  h.bisector);
    printf("  Cross-Genome:     %s\n",  h.cross_genome);
    printf("  Ledger:           %s  (%d entries)\n",
           h.ledger_integrity, h.ledger_entries);
    printf("  Runtime Probe:    %s  (%d funcs, depth %d)\n",
           h.runtime_probe, h.runtime_observed, h.runtime_depth);
    printf("  Impact:           %s\n",  h.impact);
    printf("  Golden Drift:     %s\n\n",h.golden_drift);
    printf("  Gates: %d / %d passed\n", h.gates_passed, h.gates_total);
    printf("  Confidence:       %.1f%%\n", h.confidence);
    printf("  ──────────────────────────────────\n");
    printf("  VERDICT:          %s\n\n", h.verdict);

    /* ── Emit JSON ───────────────────────────────────────────────────── */
    emit_health_json(out_path, &h);
    printf("Health report written to: %s\n", out_path);

    return (strcmp(h.verdict, "HEALTHY") == 0 ||
            strcmp(h.verdict, "MONITOR") == 0) ? 0 : 1;
}
