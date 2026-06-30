/*
 * ir_telemetry.c — ZCC IR Pass Telemetry Emitter
 * ================================================
 * Emits per-pass optimization metrics to Gods Eye via UDP 41337.
 * Fire-and-forget: if nobody is listening, sendto() fails silently.
 *
 * Wire format: {"_body":"<canonical JSON>","_sig":"ir_telemetry"}
 * The _body contains escaped JSON. The relay accepts
 * _sig == "ir_telemetry" as a Phase 1 HMAC bypass.
 *
 * Compiled by GCC only (linked separately, NOT in zcc.c).
 * Uses POSIX sockets (Linux/WSL only).
 *
 * Environment gate: ZCC_EMIT_TELEMETRY=1
 *   When unset, all functions early-return (zero overhead).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "ir_telemetry.h"

/* ── Internal state ──────────────────────────────────────────────────── */

static int s_enabled  = 0;
static int s_stdout_enabled = 0;
static int s_sock_fd  = -1;
static struct sockaddr_in s_addr;
static int s_compile_counter = 0;

void ir_telemetry_enable_stdout(void) {
    s_stdout_enabled = 1;
}

/* ── Send envelope to Gods Eye ───────────────────────────────────────── */
/*
 * body_escaped: the _body string with inner quotes already escaped as \"
 * The outer envelope wraps it: {"_body":"<body>","_sig":"ir_telemetry"}
 */
static void send_envelope(const char *body_escaped) {
    char pkt[2048];
    int len;

    if (!s_enabled && !s_stdout_enabled) return;

    len = snprintf(pkt, sizeof(pkt),
        "{\"_body\":\"%s\",\"_sig\":\"ir_telemetry\"}", body_escaped);

    if (len <= 0 || len >= (int)sizeof(pkt)) return;

    if (s_stdout_enabled) {
        printf("%s\n", pkt);
        fflush(stdout);
    }

    if (s_enabled && s_sock_fd >= 0) {
        sendto(s_sock_fd, pkt, len, MSG_DONTWAIT,
               (struct sockaddr *)&s_addr, sizeof(s_addr));
    }
}

/* ── Public API ──────────────────────────────────────────────────────── */

void ir_telem_init(void) {
    const char *env;
    const char *host;
    int port;

    env = getenv("ZCC_EMIT_TELEMETRY");
    if (!env || env[0] == '0' || env[0] == '\0') {
        s_enabled = 0;
        return;
    }

    host = getenv("GODS_EYE_HOST");
    if (!host) host = "127.0.0.1";

    port = 41337;
    env = getenv("GODS_EYE_PORT");
    if (env) port = atoi(env);

    s_sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (s_sock_fd < 0) {
        s_enabled = 0;
        return;
    }

    memset(&s_addr, 0, sizeof(s_addr));
    s_addr.sin_family = AF_INET;
    s_addr.sin_port = htons(port);
    inet_pton(AF_INET, host, &s_addr.sin_addr);

    s_enabled = 1;
    s_compile_counter = 0;

    fprintf(stderr, "[IR TELEMETRY] Emitting to %s:%d\n", host, port);
}

void ir_telem_pass(const char *pass_name,
                   int func_count,
                   int nodes_before,
                   int nodes_after,
                   int nodes_deleted,
                   int nodes_modified) {
    char body[1024];
    int delta;

    if (!s_enabled) return;

    delta = nodes_after - nodes_before;

    /*
     * Canonical JSON body with sorted keys.
     * Inner quotes escaped as \" for envelope embedding.
     *
     * Schema field reuse for relay Zod compatibility:
     *   gpu_temp_c   = nodes_before (prev energy)
     *   gpu_util_pct = nodes_after  (cur energy)
     *   h_t_state    = delta        (energy change)
     *   swarm_cycles = compile counter
     * Extra ir_* fields pass through Zod .strip().
     */
    snprintf(body, sizeof(body),
        "{\\\"deadlocks_healed\\\":0,"
         "\\\"gpu_temp_c\\\":%d,"
         "\\\"gpu_util_pct\\\":%d,"
         "\\\"h_t_state\\\":%d.0,"
         "\\\"ir_funcs\\\":%d,"
         "\\\"ir_nodes_after\\\":%d,"
         "\\\"ir_nodes_before\\\":%d,"
         "\\\"ir_nodes_deleted\\\":%d,"
         "\\\"ir_nodes_modified\\\":%d,"
         "\\\"ir_pass\\\":\\\"%s\\\","
         "\\\"jit_latency_ms\\\":0.0,"
         "\\\"swarm_cycles\\\":%d,"
         "\\\"vram_usage_mb\\\":0.0}",
        nodes_before,
        nodes_after,
        delta,
        func_count,
        nodes_after,
        nodes_before,
        nodes_deleted,
        nodes_modified,
        pass_name,
        s_compile_counter);

    send_envelope(body);
}

void ir_telem_summary(int total_funcs,
                      int total_nodes_before,
                      int total_nodes_after,
                      int pass_count,
                      const char **pass_names) {
    char body[1024];
    char passes_str[256];
    int delta, i, pos;
    double reduction_pct;

    if (!s_enabled) return;

    s_compile_counter++;

    delta = total_nodes_after - total_nodes_before;
    reduction_pct = (total_nodes_before > 0)
        ? 100.0 * (1.0 - (double)total_nodes_after / total_nodes_before)
        : 0.0;

    /* Build comma-separated pass list */
    pos = 0;
    for (i = 0; i < pass_count && pos < (int)sizeof(passes_str) - 32; i++) {
        if (i > 0) passes_str[pos++] = ',';
        pos += snprintf(passes_str + pos, sizeof(passes_str) - pos,
                        "%s", pass_names[i]);
    }
    passes_str[pos] = '\0';

    snprintf(body, sizeof(body),
        "{\\\"deadlocks_healed\\\":0,"
         "\\\"gpu_temp_c\\\":%d,"
         "\\\"gpu_util_pct\\\":%d,"
         "\\\"h_t_state\\\":%d.0,"
         "\\\"ir_passes\\\":\\\"%s\\\","
         "\\\"ir_reduction_pct\\\":%.1f,"
         "\\\"ir_total_funcs\\\":%d,"
         "\\\"ir_total_nodes_after\\\":%d,"
         "\\\"ir_total_nodes_before\\\":%d,"
         "\\\"ir_type\\\":\\\"summary\\\","
         "\\\"jit_latency_ms\\\":0.0,"
         "\\\"swarm_cycles\\\":%d,"
         "\\\"vram_usage_mb\\\":%.1f}",
        total_nodes_before,
        total_nodes_after,
        delta,
        passes_str,
        reduction_pct,
        total_funcs,
        total_nodes_after,
        total_nodes_before,
        s_compile_counter,
        reduction_pct);

    send_envelope(body);

    fprintf(stderr, "[IR TELEMETRY] Compilation #%d: %d -> %d nodes (%.1f%% reduction) [%s]\n",
            s_compile_counter, total_nodes_before, total_nodes_after,
            reduction_pct, passes_str);
}

void ir_telem_shutdown(void) {
    if (s_sock_fd >= 0) {
        close(s_sock_fd);
        s_sock_fd = -1;
    }
    s_enabled = 0;
}

void zcc_telem_phase(int phase, const char *phase_name, const char *status, int duration_us,
                     const char *metric_key1, long long metric_val1,
                     const char *metric_key2, long long metric_val2,
                     const char *metric_key3, long long metric_val3) {
    char body[1024];

    snprintf(body, sizeof(body),
        "{\\\"phase\\\":%d,"
         "\\\"phase_name\\\":\\\"%s\\\","
         "\\\"status\\\":\\\"%s\\\","
         "\\\"duration_us\\\":%d,"
         "\\\"%s\\\":%lld,"
         "\\\"%s\\\":%lld,"
         "\\\"%s\\\":%lld}",
         phase, phase_name, status, duration_us,
         metric_key1, metric_val1,
         metric_key2, metric_val2,
         metric_key3, metric_val3);

    send_envelope(body);
}

#define MAX_LOGGED_PASSES 64
#define MAX_LOGGED_BLOCKS 256

typedef struct {
    char pass_name[64];
    int duration_us;
    int nodes_before;
    int nodes_after;
    int deleted;
    int modified;
} TelemPassOpt;

typedef struct {
    int id;
    int preds[16];
    int n_preds;
    int succs[16];
    int n_succs;
    int inst_count;
    double weight;
} TelemBlockCfg;

static TelemPassOpt s_opts[MAX_LOGGED_PASSES];
static int s_num_opts = 0;

static TelemBlockCfg s_blocks[MAX_LOGGED_BLOCKS];
static int s_num_blocks = 0;

static char s_ra_func_name[64] = "";
static int s_ra_live_ranges = 0;
static int s_ra_coloring_loops = 0;
static int s_ra_spills = 0;
static int s_ra_peak_pressure = 0;

static int is_telem_active(void) {
    return s_enabled || s_stdout_enabled || (getenv("ZCC_EMIT_TELEMETRY") && getenv("ZCC_EMIT_TELEMETRY")[0] != '0');
}

void ir_telem_log_opt(const char *pass_name, int duration_us, int nodes_before, int nodes_after, int deleted, int modified) {
    if (!is_telem_active()) return;
    if (s_num_opts >= MAX_LOGGED_PASSES) return;
    TelemPassOpt *opt = &s_opts[s_num_opts++];
    strncpy(opt->pass_name, pass_name, sizeof(opt->pass_name) - 1);
    opt->pass_name[sizeof(opt->pass_name) - 1] = '\0';
    opt->duration_us = duration_us;
    opt->nodes_before = nodes_before;
    opt->nodes_after = nodes_after;
    opt->deleted = deleted;
    opt->modified = modified;
}

void ir_telem_log_cfg(int block_id, int n_preds, const int *preds, int n_succs, const int *succs, int inst_count, double weight) {
    if (!is_telem_active()) return;
    if (s_num_blocks >= MAX_LOGGED_BLOCKS) return;
    TelemBlockCfg *b = &s_blocks[s_num_blocks++];
    b->id = block_id;
    b->n_preds = n_preds > 16 ? 16 : n_preds;
    for (int i = 0; i < b->n_preds; i++) b->preds[i] = preds[i];
    b->n_succs = n_succs > 16 ? 16 : n_succs;
    for (int i = 0; i < b->n_succs; i++) b->succs[i] = succs[i];
    b->inst_count = inst_count;
    b->weight = weight;
}

void ir_telem_log_regalloc(const char *func_name, int live_ranges, int coloring_loops, int spills, int peak_pressure) {
    if (!is_telem_active()) return;
    strncpy(s_ra_func_name, func_name, sizeof(s_ra_func_name) - 1);
    s_ra_func_name[sizeof(s_ra_func_name) - 1] = '\0';
    s_ra_live_ranges = live_ranges;
    s_ra_coloring_loops = coloring_loops;
    s_ra_spills = spills;
    s_ra_peak_pressure = peak_pressure;
}

void ir_telem_flush_observatory(const char *func_name) {
    if (!is_telem_active()) return;
    FILE *f = fopen("zcc_observatory_data.json", "w");
    if (!f) return;

    fprintf(f, "{\n");
    fprintf(f, "  \"function_name\": \"%s\",\n", func_name);
    
    // Write passes
    fprintf(f, "  \"passes\": [\n");
    for (int i = 0; i < s_num_opts; i++) {
        TelemPassOpt *opt = &s_opts[i];
        fprintf(f, "    {\n");
        fprintf(f, "      \"pass_name\": \"%s\",\n", opt->pass_name);
        fprintf(f, "      \"duration_us\": %d,\n", opt->duration_us);
        fprintf(f, "      \"nodes_before\": %d,\n", opt->nodes_before);
        fprintf(f, "      \"nodes_after\": %d,\n", opt->nodes_after);
        fprintf(f, "      \"nodes_deleted\": %d,\n", opt->deleted);
        fprintf(f, "      \"nodes_modified\": %d\n", opt->modified);
        fprintf(f, "    }%s\n", (i == s_num_opts - 1) ? "" : ",");
    }
    fprintf(f, "  ],\n");

    // Write CFG
    fprintf(f, "  \"cfg\": {\n");
    fprintf(f, "    \"blocks\": [\n");
    for (int i = 0; i < s_num_blocks; i++) {
        TelemBlockCfg *b = &s_blocks[i];
        fprintf(f, "      {\n");
        fprintf(f, "        \"id\": %d,\n", b->id);
        fprintf(f, "        \"preds\": [");
        for (int p = 0; p < b->n_preds; p++) {
            fprintf(f, "%d%s", b->preds[p], (p == b->n_preds - 1) ? "" : ", ");
        }
        fprintf(f, "],\n");
        fprintf(f, "        \"succs\": [");
        for (int s = 0; s < b->n_succs; s++) {
            fprintf(f, "%d%s", b->succs[s], (s == b->n_succs - 1) ? "" : ", ");
        }
        fprintf(f, "],\n");
        fprintf(f, "        \"inst_count\": %d,\n", b->inst_count);
        fprintf(f, "        \"weight\": %.3f\n", b->weight);
        fprintf(f, "      }%s\n", (i == s_num_blocks - 1) ? "" : ",");
    }
    fprintf(f, "    ]\n");
    fprintf(f, "  },\n");

    // Write RegAlloc
    fprintf(f, "  \"register_allocation\": {\n");
    fprintf(f, "    \"func_name\": \"%s\",\n", s_ra_func_name);
    fprintf(f, "    \"live_ranges\": %d,\n", s_ra_live_ranges);
    fprintf(f, "    \"coloring_loops\": %d,\n", s_ra_coloring_loops);
    fprintf(f, "    \"spills\": %d,\n", s_ra_spills);
    fprintf(f, "    \"peak_pressure\": %d\n", s_ra_peak_pressure);
    fprintf(f, "  }\n");
    fprintf(f, "}\n");

    fclose(f);

    // Reset counters for next function compilation
    s_num_opts = 0;
    s_num_blocks = 0;
    s_ra_func_name[0] = '\0';
    s_ra_live_ranges = 0;
    s_ra_coloring_loops = 0;
    s_ra_spills = 0;
    s_ra_peak_pressure = 0;
}


