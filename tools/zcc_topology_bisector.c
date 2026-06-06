/*
 * zcc_topology_bisector.c — D-25: Topology Bisector
 *
 * Given a "bad" genome version and a registry directory, walks the
 * lineage backwards to locate the FIRST version that introduced each
 * class of regression:
 *   - topology root mutation
 *   - stack frame growth beyond threshold
 *   - register pressure spike
 *   - instruction count divergence
 *
 * Outputs a structured bisect report: per-regression "first bad"
 * version and the transition boundary where the regression entered.
 *
 * Memory discipline: every malloc() has a matching free() before exit.
 * No phantom closures.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#define MAX_GENOMES 64

/* ── shared helpers ─────────────────────────────────────────────────── */

static void die(const char *msg) {
    fprintf(stderr, "zcc_topology_bisector: fatal: %s\n", msg);
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
    int  mov_cnt, call_cnt, jmp_cnt, ret_cnt;
    int  reg_counts[16];
    int  max_stack_frame;
    int  reachable_functions;
    char topology_root[65];
    char controlflow_root[65];
    char instruction_root[65];
    char register_root[65];
    char stack_root[65];
} GenomeData;

typedef struct {
    char     version[32];
    int      major, minor, patch;
    GenomeData data;
} GenomeRecord;

static void parse_genome(const char *json, GenomeData *g) {
    memset(g, 0, sizeof(GenomeData));
    const char *fingerprint = strstr(json, "\"execution_fingerprint\"");
    const char *instruction = strstr(json, "\"instruction_profile\"");
    const char *regblock    = strstr(json, "\"register_profile\"");
    const char *stackblock  = strstr(json, "\"stack_analysis\"");
    const char *merkle      = strstr(json, "\"merkle_topology\"");

    if (fingerprint) {
        find_json_string_scoped(fingerprint, "\"controlflow_root\"",  g->controlflow_root,  65);
        find_json_string_scoped(fingerprint, "\"instruction_root\"",  g->instruction_root,  65);
        find_json_string_scoped(fingerprint, "\"register_root\"",     g->register_root,     65);
        find_json_string_scoped(fingerprint, "\"stack_root\"",        g->stack_root,        65);
        find_json_int_scoped(fingerprint, "\"reachable_functions\"", &g->reachable_functions);
    }
    if (merkle)
        find_json_string_scoped(merkle, "\"topology_root\"", g->topology_root, 65);
    if (instruction) {
        find_json_int_scoped(instruction, "\"mov\"",  &g->mov_cnt);
        find_json_int_scoped(instruction, "\"call\"", &g->call_cnt);
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
    if (stackblock)
        find_json_int_scoped(stackblock, "\"max_stack_frame\"", &g->max_stack_frame);
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

/* ── bisection result per regression class ──────────────────────────── */

typedef struct {
    int  found;
    char first_bad_version[32];
    char last_good_version[32];
    char description[256];
} BisectResult;

/* ── thresholds (tunable via CLI) ───────────────────────────────────── */
#define STACK_THRESHOLD_DEFAULT    32   /* bytes growth per hop = warning */
#define REG_THRESHOLD_DEFAULT      50   /* total register drift per hop    */
#define INSTR_THRESHOLD_DEFAULT    30   /* mov-count growth per hop        */

/* ═══════════════════════════════════════════════════════════════════════ */

int main(int argc, char **argv) {
    const char *registry_dir = NULL;
    const char *bad_version  = NULL;
    const char *out_path     = NULL;
    int stack_thresh = STACK_THRESHOLD_DEFAULT;
    int reg_thresh   = REG_THRESHOLD_DEFAULT;
    int instr_thresh = INSTR_THRESHOLD_DEFAULT;

    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "--registry") == 0) && i + 1 < argc)
            registry_dir = argv[++i];
        else if ((strcmp(argv[i], "--bad") == 0) && i + 1 < argc)
            bad_version = argv[++i];
        else if ((strcmp(argv[i], "--out") == 0) && i + 1 < argc)
            out_path = argv[++i];
        else if ((strcmp(argv[i], "--stack-thresh") == 0) && i + 1 < argc)
            stack_thresh = atoi(argv[++i]);
        else if ((strcmp(argv[i], "--reg-thresh") == 0) && i + 1 < argc)
            reg_thresh = atoi(argv[++i]);
        else if ((strcmp(argv[i], "--instr-thresh") == 0) && i + 1 < argc)
            instr_thresh = atoi(argv[++i]);
    }

    if (!registry_dir || !bad_version) {
        printf("Usage: zcc_topology_bisector --registry <dir> --bad <version> "
               "[--out bisect.json] [--stack-thresh N] [--reg-thresh N] [--instr-thresh N]\n");
        return 2;
    }

    DIR *d = opendir(registry_dir);
    if (!d) { fprintf(stderr, "error: cannot open registry %s\n", registry_dir); return 2; }

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
        snprintf(filepath, sizeof(filepath), "%s/%s", registry_dir, entry->d_name);
        size_t sz = 0;
        uint8_t *content = load_file(filepath, &sz);
        if (!content) { fprintf(stderr, "warning: skip %s\n", filepath); continue; }
        parse_genome((const char *)content, &r->data);
        free(content);
        count++;
    }
    closedir(d);

    if (count == 0) {
        fprintf(stderr, "error: no v*.json genomes in %s\n", registry_dir);
        return 1;
    }

    qsort(records, count, sizeof(GenomeRecord), cmp_records);

    /* Locate the "bad" version index */
    int bad_idx = -1;
    for (int i = 0; i < count; i++) {
        if (strcmp(records[i].version, bad_version) == 0) { bad_idx = i; break; }
    }
    if (bad_idx < 0) {
        fprintf(stderr, "error: version '%s' not found in registry\n", bad_version);
        return 1;
    }
    if (bad_idx == 0) {
        printf("Version %s is the oldest in registry — no prior version to bisect against.\n",
               bad_version);
        return 0;
    }

    /* ── Bisect: walk backwards from bad_idx → 0 ─────────────────────── */

    BisectResult stack_bisect, reg_bisect, instr_bisect, topo_bisect;
    memset(&stack_bisect, 0, sizeof(BisectResult));
    memset(&reg_bisect,   0, sizeof(BisectResult));
    memset(&instr_bisect, 0, sizeof(BisectResult));
    memset(&topo_bisect,  0, sizeof(BisectResult));

    printf("=== ZCC Topology Bisector ===\n");
    printf("Registry: %s\n", registry_dir);
    printf("Bad version: %s  (index %d of %d)\n\n", bad_version, bad_idx, count - 1);
    printf("Scanning regressions from %s backwards...\n\n",
           records[bad_idx].version);

    for (int i = bad_idx; i >= 1; i--) {
        GenomeData *ga = &records[i-1].data;
        GenomeData *gb = &records[i].data;

        /* Stack regression */
        int stk_delta = gb->max_stack_frame - ga->max_stack_frame;
        if (!stack_bisect.found && stk_delta >= stack_thresh) {
            stack_bisect.found = 1;
            strcpy(stack_bisect.first_bad_version, records[i].version);
            strcpy(stack_bisect.last_good_version, records[i-1].version);
            snprintf(stack_bisect.description, sizeof(stack_bisect.description),
                     "Stack grew %+d bytes (%d -> %d) across %s->%s",
                     stk_delta, ga->max_stack_frame, gb->max_stack_frame,
                     records[i-1].version, records[i].version);
        }

        /* Register pressure regression */
        int reg_drift = 0;
        for (int r = 0; r < 16; r++)
            reg_drift += abs(gb->reg_counts[r] - ga->reg_counts[r]);
        if (!reg_bisect.found && reg_drift >= reg_thresh) {
            reg_bisect.found = 1;
            strcpy(reg_bisect.first_bad_version, records[i].version);
            strcpy(reg_bisect.last_good_version, records[i-1].version);
            snprintf(reg_bisect.description, sizeof(reg_bisect.description),
                     "Register drift %d units across %s->%s",
                     reg_drift, records[i-1].version, records[i].version);
        }

        /* Instruction count regression (mov proxy) */
        int instr_delta = gb->mov_cnt - ga->mov_cnt;
        if (!instr_bisect.found && instr_delta >= instr_thresh) {
            instr_bisect.found = 1;
            strcpy(instr_bisect.first_bad_version, records[i].version);
            strcpy(instr_bisect.last_good_version, records[i-1].version);
            snprintf(instr_bisect.description, sizeof(instr_bisect.description),
                     "Instruction count (mov) grew %+d (%d -> %d) across %s->%s",
                     instr_delta, ga->mov_cnt, gb->mov_cnt,
                     records[i-1].version, records[i].version);
        }

        /* Topology root mutation */
        int root_mutated = (ga->topology_root[0] && gb->topology_root[0] &&
                            strcmp(ga->topology_root, gb->topology_root) != 0);
        if (!topo_bisect.found && root_mutated) {
            topo_bisect.found = 1;
            strcpy(topo_bisect.first_bad_version, records[i].version);
            strcpy(topo_bisect.last_good_version, records[i-1].version);
            snprintf(topo_bisect.description, sizeof(topo_bisect.description),
                     "Topology root changed %.20s -> %.20s across %s->%s",
                     ga->topology_root, gb->topology_root,
                     records[i-1].version, records[i].version);
        }
    }

    /* ── Print bisect results ─────────────────────────────────────────── */

    printf("Bisect Results:\n");

    int any = 0;

    if (stack_bisect.found) {
        any = 1;
        printf("  [STACK]   First bad: %-10s  Last good: %-10s\n",
               stack_bisect.first_bad_version, stack_bisect.last_good_version);
        printf("            %s\n", stack_bisect.description);
    } else {
        printf("  [STACK]   No regression detected (threshold: +%d bytes)\n", stack_thresh);
    }

    if (reg_bisect.found) {
        any = 1;
        printf("  [REGS]    First bad: %-10s  Last good: %-10s\n",
               reg_bisect.first_bad_version, reg_bisect.last_good_version);
        printf("            %s\n", reg_bisect.description);
    } else {
        printf("  [REGS]    No regression detected (threshold: %d units)\n", reg_thresh);
    }

    if (instr_bisect.found) {
        any = 1;
        printf("  [INSTR]   First bad: %-10s  Last good: %-10s\n",
               instr_bisect.first_bad_version, instr_bisect.last_good_version);
        printf("            %s\n", instr_bisect.description);
    } else {
        printf("  [INSTR]   No regression detected (threshold: +%d mov)\n", instr_thresh);
    }

    if (topo_bisect.found) {
        any = 1;
        printf("  [TOPO]    First bad: %-10s  Last good: %-10s\n",
               topo_bisect.first_bad_version, topo_bisect.last_good_version);
        printf("            %s\n", topo_bisect.description);
    } else {
        printf("  [TOPO]    No topology mutation detected\n");
    }

    printf("\n");
    printf("Bisector Verdict: %s\n", any ? "REGRESSIONS LOCALIZED" : "CLEAN (no regressions found)");

    /* ── Optional JSON output ─────────────────────────────────────────── */
    if (out_path) {
        FILE *jf = fopen(out_path, "w");
        if (jf) {
            fprintf(jf, "{\n");
            fprintf(jf, "  \"schema\": \"zcc.bisect.v1\",\n");
            fprintf(jf, "  \"bad_version\": \"%s\",\n", bad_version);
            fprintf(jf, "  \"registry\": \"%s\",\n", registry_dir);
            fprintf(jf, "  \"results\": {\n");

            #define EMIT_BISECT(label, br) \
                fprintf(jf, "    \"%s\": { \"found\": %s, \"first_bad\": \"%s\", " \
                        "\"last_good\": \"%s\", \"description\": \"%s\" }", \
                        label, (br).found ? "true" : "false", \
                        (br).found ? (br).first_bad_version : "", \
                        (br).found ? (br).last_good_version : "", \
                        (br).found ? (br).description : "")

            EMIT_BISECT("stack",       stack_bisect); fprintf(jf, ",\n");
            EMIT_BISECT("registers",   reg_bisect);   fprintf(jf, ",\n");
            EMIT_BISECT("instruction", instr_bisect); fprintf(jf, ",\n");
            EMIT_BISECT("topology",    topo_bisect);  fprintf(jf, "\n");

            #undef EMIT_BISECT

            fprintf(jf, "  }\n}\n");
            fclose(jf);
            printf("Bisect report written to: %s\n", out_path);
        } else {
            fprintf(stderr, "warning: cannot write %s\n", out_path);
        }
    }

    return any ? 1 : 0;
}
