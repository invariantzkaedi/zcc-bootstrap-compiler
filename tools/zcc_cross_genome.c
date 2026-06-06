/*
 * zcc_cross_genome.c — D-26: Cross-Compiler Genome Observatory
 *
 * Compares ZCC genome snapshots against reference genomes from GCC and
 * Clang (or any two foreign compilers) by importing their topology JSON
 * files produced by zcc_topology_auditor --json (or equivalent external
 * wrappers).
 *
 * Reads three genome directories:
 *   --zcc    <dir>   ZCC genome registry (v*.json files)
 *   --ref-a  <file>  Single reference genome JSON (e.g. gcc_genome.json)
 *   --ref-b  <file>  Single reference genome JSON (e.g. clang_genome.json)
 *
 * For each metric (instructions, registers, stack, topology), it emits:
 *   - Absolute values per compiler
 *   - ZCC delta vs. reference A
 *   - ZCC delta vs. reference B
 *   - Cross-compiler convergence score (0..100)
 *
 * Memory discipline: every malloc() has a matching free() before exit.
 * No phantom closures.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <dirent.h>

#define MAX_GENOMES 64

/* ── helpers ─────────────────────────────────────────────────────────── */

static void die(const char *msg) {
    fprintf(stderr, "zcc_cross_genome: fatal: %s\n", msg);
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

/* ── genome data ─────────────────────────────────────────────────────── */

typedef struct {
    int  mov_cnt, call_cnt, lea_cnt, cmp_cnt, jmp_cnt, ret_cnt;
    int  reg_counts[16];
    int  max_stack_frame;
    int  average_stack_frame;
    int  reachable_functions;
    int  leaf_functions;
    int  branch_nodes;
    int  functions_count;
    int  relocations_count;
    int  symbols_count;
    char topology_root[65];
    char controlflow_root[65];
    char instruction_root[65];
    char register_root[65];
    char stack_root[65];
    char build_id[65];
} GenomeData;

static void parse_genome(const char *json, GenomeData *g) {
    memset(g, 0, sizeof(GenomeData));
    const char *meta        = strstr(json, "\"metadata\"");
    const char *telemetry   = strstr(json, "\"telemetry\"");
    const char *fingerprint = strstr(json, "\"execution_fingerprint\"");
    const char *instruction = strstr(json, "\"instruction_profile\"");
    const char *regblock    = strstr(json, "\"register_profile\"");
    const char *stackblock  = strstr(json, "\"stack_analysis\"");
    const char *merkle      = strstr(json, "\"merkle_topology\"");

    if (meta)
        find_json_string_scoped(meta, "\"build_id\"", g->build_id, 65);
    if (telemetry) {
        find_json_int_scoped(telemetry, "\"functions_count\"",   &g->functions_count);
        find_json_int_scoped(telemetry, "\"relocations_count\"", &g->relocations_count);
        find_json_int_scoped(telemetry, "\"symbols_count\"",     &g->symbols_count);
    }
    if (merkle)
        find_json_string_scoped(merkle, "\"topology_root\"", g->topology_root, 65);
    if (fingerprint) {
        find_json_string_scoped(fingerprint, "\"controlflow_root\"",  g->controlflow_root,  65);
        find_json_string_scoped(fingerprint, "\"instruction_root\"",  g->instruction_root,  65);
        find_json_string_scoped(fingerprint, "\"register_root\"",     g->register_root,     65);
        find_json_string_scoped(fingerprint, "\"stack_root\"",        g->stack_root,        65);
        find_json_int_scoped(fingerprint, "\"reachable_functions\"", &g->reachable_functions);
        find_json_int_scoped(fingerprint, "\"leaf_functions\"",      &g->leaf_functions);
        find_json_int_scoped(fingerprint, "\"branch_nodes\"",        &g->branch_nodes);
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
}

/* ── load the most recent genome from a registry directory ───────────── */

static int parse_version(const char *name, int *maj, int *min, int *pat) {
    *maj = *min = *pat = 0;
    if (name[0] != 'v') return 0;
    const char *ext = strstr(name, ".json");
    if (!ext) return 0;
    char ver[32];
    int vlen = (int)(ext - name);
    if (vlen > 31) vlen = 31;
    strncpy(ver, name, vlen);
    ver[vlen] = '\0';
    int f = sscanf(ver, "v%d.%d.%d", maj, min, pat);
    if (f < 2) { *pat = 0; f = sscanf(ver, "v%d.%d", maj, min); }
    return f >= 2;
}

/* Returns 1 on success, fills out_path and out_genome */
static int load_latest_genome_from_dir(const char *dir_path,
                                        char *out_path_buf, int buf_sz,
                                        GenomeData *out_genome) {
    DIR *d = opendir(dir_path);
    if (!d) return 0;

    /* find the file with the highest version */
    int best_maj = -1, best_min = -1, best_pat = -1;
    char best_name[256] = "";
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        int maj = 0, min = 0, pat = 0;
        if (!parse_version(entry->d_name, &maj, &min, &pat)) continue;
        if (maj > best_maj ||
            (maj == best_maj && min > best_min) ||
            (maj == best_maj && min == best_min && pat > best_pat)) {
            best_maj = maj; best_min = min; best_pat = pat;
            strncpy(best_name, entry->d_name, sizeof(best_name) - 1);
        }
    }
    closedir(d);

    if (best_name[0] == '\0') return 0;

    snprintf(out_path_buf, buf_sz, "%s/%s", dir_path, best_name);
    size_t sz = 0;
    uint8_t *content = load_file(out_path_buf, &sz);
    if (!content) return 0;
    parse_genome((const char *)content, out_genome);
    free(content);
    return 1;
}

/* ── convergence score between two genomes (0..100) ─────────────────── */
/*
 * 100 = identical instruction ratios, register pressure, stack profile.
 * Deducts points per category of divergence.
 */
static int convergence_score(const GenomeData *a, const GenomeData *b) {
    int score = 100;

    /* Instruction ratio convergence (mov dominant signal) */
    int mov_delta = b->mov_cnt - a->mov_cnt;
    if (mov_delta < 0) mov_delta = -mov_delta;
    score -= (mov_delta / 20);

    /* Call/jmp count divergence */
    int call_delta = b->call_cnt - a->call_cnt;
    if (call_delta < 0) call_delta = -call_delta;
    score -= (call_delta / 10);

    /* Total register pressure delta */
    int reg_drift = 0;
    for (int i = 0; i < 16; i++)
        reg_drift += abs(b->reg_counts[i] - a->reg_counts[i]);
    score -= (reg_drift / 20);

    /* Stack frame divergence */
    int stk_delta = b->max_stack_frame - a->max_stack_frame;
    if (stk_delta < 0) stk_delta = -stk_delta;
    score -= (stk_delta / 16);

    /* Topology root match */
    if (a->topology_root[0] && b->topology_root[0] &&
        strcmp(a->topology_root, b->topology_root) != 0)
        score -= 5;

    if (score < 0)   score = 0;
    if (score > 100) score = 100;
    return score;
}

/* ═══════════════════════════════════════════════════════════════════════ */

int main(int argc, char **argv) {
    const char *zcc_dir   = NULL;
    const char *ref_a_path = NULL;  /* direct genome JSON or directory */
    const char *ref_b_path = NULL;
    const char *ref_a_label = "ref-a";
    const char *ref_b_label = "ref-b";
    const char *out_path   = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--zcc") == 0 && i + 1 < argc)
            zcc_dir = argv[++i];
        else if (strcmp(argv[i], "--ref-a") == 0 && i + 1 < argc)
            ref_a_path = argv[++i];
        else if (strcmp(argv[i], "--ref-b") == 0 && i + 1 < argc)
            ref_b_path = argv[++i];
        else if (strcmp(argv[i], "--ref-a-label") == 0 && i + 1 < argc)
            ref_a_label = argv[++i];
        else if (strcmp(argv[i], "--ref-b-label") == 0 && i + 1 < argc)
            ref_b_label = argv[++i];
        else if (strcmp(argv[i], "--out") == 0 && i + 1 < argc)
            out_path = argv[++i];
    }

    if (!zcc_dir || !ref_a_path) {
        printf("Usage: zcc_cross_genome --zcc <zcc_genomes_dir> "
               "--ref-a <genome.json or dir> [--ref-b <genome.json or dir>] "
               "[--ref-a-label gcc] [--ref-b-label clang] [--out cross.json]\n");
        return 2;
    }

    /* ── Load ZCC latest genome ──────────────────────────────────────── */
    GenomeData g_zcc;
    char zcc_path[640];
    if (!load_latest_genome_from_dir(zcc_dir, zcc_path, sizeof(zcc_path), &g_zcc)) {
        fprintf(stderr, "error: no valid genome found in zcc dir %s\n", zcc_dir);
        return 1;
    }

    /* ── Load reference A genome ─────────────────────────────────────── */
    GenomeData g_ref_a;
    char ref_a_loaded[640];
    /* Try as direct file first, then as directory */
    {
        size_t sz = 0;
        uint8_t *content = load_file(ref_a_path, &sz);
        if (content) {
            parse_genome((const char *)content, &g_ref_a);
            free(content);
            snprintf(ref_a_loaded, sizeof(ref_a_loaded), "%s", ref_a_path);
        } else {
            if (!load_latest_genome_from_dir(ref_a_path, ref_a_loaded, sizeof(ref_a_loaded), &g_ref_a)) {
                fprintf(stderr, "error: cannot load ref-a genome from %s\n", ref_a_path);
                return 1;
            }
        }
    }

    /* ── Optionally load reference B genome ─────────────────────────── */
    GenomeData g_ref_b;
    char ref_b_loaded[640];
    int has_ref_b = 0;
    if (ref_b_path) {
        size_t sz = 0;
        uint8_t *content = load_file(ref_b_path, &sz);
        if (content) {
            parse_genome((const char *)content, &g_ref_b);
            free(content);
            snprintf(ref_b_loaded, sizeof(ref_b_loaded), "%s", ref_b_path);
            has_ref_b = 1;
        } else {
            if (load_latest_genome_from_dir(ref_b_path, ref_b_loaded, sizeof(ref_b_loaded), &g_ref_b))
                has_ref_b = 1;
            else
                fprintf(stderr, "warning: cannot load ref-b genome from %s — skipping\n", ref_b_path);
        }
    }

    /* ── Report ──────────────────────────────────────────────────────── */
    printf("=== ZCC Cross-Compiler Genome Observatory ===\n\n");
    printf("ZCC genome:   %s\n", zcc_path);
    printf("%-10s:  %s\n", ref_a_label, ref_a_loaded);
    if (has_ref_b) printf("%-10s:  %s\n", ref_b_label, ref_b_loaded);
    printf("\n");

    const char *labels[3] = { "ZCC", ref_a_label, ref_b_label };
    const GenomeData *genomes[3] = { &g_zcc, &g_ref_a, has_ref_b ? &g_ref_b : NULL };
    int num_genomes = has_ref_b ? 3 : 2;

    /* Instruction profile table */
    printf("Instruction Profile:\n");
    printf("  %-6s  %8s  %8s  %8s  %8s  %8s  %8s\n",
           "Cmp", "mov", "call", "lea", "cmp", "jmp", "ret");
    for (int i = 0; i < num_genomes; i++) {
        if (!genomes[i]) continue;
        const GenomeData *g = genomes[i];
        printf("  %-6s  %8d  %8d  %8d  %8d  %8d  %8d\n",
               labels[i], g->mov_cnt, g->call_cnt, g->lea_cnt,
               g->cmp_cnt, g->jmp_cnt, g->ret_cnt);
    }
    printf("\n");

    /* Register pressure table */
    static const char *rnames[16] = {
        "rax","rcx","rdx","rbx","rsp","rbp","rsi","rdi",
        "r8", "r9", "r10","r11","r12","r13","r14","r15"
    };
    printf("Register Pressure (top divergent registers):\n");
    printf("  %-6s  ", "Reg");
    for (int i = 0; i < num_genomes; i++) { if (genomes[i]) printf("  %-8s", labels[i]); }
    printf("  ZCC vs %s", ref_a_label);
    if (has_ref_b) printf("  ZCC vs %s", ref_b_label);
    printf("\n");

    for (int r = 0; r < 16; r++) {
        int zcc_v  = g_zcc.reg_counts[r];
        int ra_v   = g_ref_a.reg_counts[r];
        int rb_v   = has_ref_b ? g_ref_b.reg_counts[r] : 0;
        int max_any = zcc_v > ra_v ? zcc_v : ra_v;
        if (has_ref_b && rb_v > max_any) max_any = rb_v;
        if (max_any == 0) continue; /* skip zero-pressure regs */
        printf("  %-6s  ", rnames[r]);
        for (int i = 0; i < num_genomes; i++) {
            if (genomes[i]) printf("  %-8d", genomes[i]->reg_counts[r]);
        }
        printf("  %+d", zcc_v - ra_v);
        if (has_ref_b) printf("        %+d", zcc_v - rb_v);
        printf("\n");
    }
    printf("\n");

    /* Stack comparison */
    printf("Stack Profile:\n");
    printf("  %-10s  Max Frame  Avg Frame\n", "Compiler");
    printf("  %-10s  %9d  %9d\n", "ZCC",      g_zcc.max_stack_frame,   g_zcc.average_stack_frame);
    printf("  %-10s  %9d  %9d\n", ref_a_label, g_ref_a.max_stack_frame, g_ref_a.average_stack_frame);
    if (has_ref_b)
        printf("  %-10s  %9d  %9d\n", ref_b_label, g_ref_b.max_stack_frame, g_ref_b.average_stack_frame);
    printf("\n");

    /* Convergence scores */
    int conv_a = convergence_score(&g_zcc, &g_ref_a);
    int conv_b = has_ref_b ? convergence_score(&g_zcc, &g_ref_b) : -1;

    printf("Cross-Compiler Convergence:\n");
    printf("  ZCC vs %-8s:  %3d/100\n", ref_a_label, conv_a);
    if (has_ref_b) printf("  ZCC vs %-8s:  %3d/100\n", ref_b_label, conv_b);
    printf("\n");

    const char *verdict_a =
        (conv_a >= 80) ? "CONVERGED" :
        (conv_a >= 50) ? "PARTIALLY ALIGNED" : "DIVERGED";
    printf("Cross-Compiler Verdict:\n");
    printf("  ZCC vs %-8s: %s\n", ref_a_label, verdict_a);
    if (has_ref_b) {
        const char *verdict_b =
            (conv_b >= 80) ? "CONVERGED" :
            (conv_b >= 50) ? "PARTIALLY ALIGNED" : "DIVERGED";
        printf("  ZCC vs %-8s: %s\n", ref_b_label, verdict_b);
    }

    /* ── Optional JSON output ─────────────────────────────────────────── */
    if (out_path) {
        FILE *jf = fopen(out_path, "w");
        if (jf) {
            fprintf(jf, "{\n");
            fprintf(jf, "  \"schema\": \"zcc.cross_genome.v1\",\n");
            fprintf(jf, "  \"zcc_genome\": \"%s\",\n", zcc_path);
            fprintf(jf, "  \"reference_a\": { \"label\": \"%s\", \"path\": \"%s\", "
                    "\"convergence_score\": %d },\n",
                    ref_a_label, ref_a_loaded, conv_a);
            if (has_ref_b)
                fprintf(jf, "  \"reference_b\": { \"label\": \"%s\", \"path\": \"%s\", "
                        "\"convergence_score\": %d },\n",
                        ref_b_label, ref_b_loaded, conv_b);
            fprintf(jf, "  \"zcc_profile\": { \"mov\": %d, \"call\": %d, \"max_stack\": %d },\n",
                    g_zcc.mov_cnt, g_zcc.call_cnt, g_zcc.max_stack_frame);
            fprintf(jf, "  \"ref_a_profile\": { \"mov\": %d, \"call\": %d, \"max_stack\": %d }",
                    g_ref_a.mov_cnt, g_ref_a.call_cnt, g_ref_a.max_stack_frame);
            if (has_ref_b)
                fprintf(jf, ",\n  \"ref_b_profile\": { \"mov\": %d, \"call\": %d, \"max_stack\": %d }",
                        g_ref_b.mov_cnt, g_ref_b.call_cnt, g_ref_b.max_stack_frame);
            fprintf(jf, "\n}\n");
            fclose(jf);
            printf("\nCross-genome report written to: %s\n", out_path);
        } else {
            fprintf(stderr, "warning: cannot write %s\n", out_path);
        }
    }

    return 0;
}
