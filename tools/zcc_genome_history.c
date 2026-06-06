#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#define MAX_GENOMES 64

static void die(const char *msg) {
    fprintf(stderr, "zcc_genome_history: fatal: %s\n", msg);
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

static int find_json_string_scoped(const char *scope, const char *key, char *out_val, int max_len) {
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
    while (*p && *p != '"' && len < max_len - 1) {
        out_val[len++] = *p++;
    }
    out_val[len] = '\0';
    return 1;
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

typedef struct {
    int functions_count;
    int relocations_count;
    int symbols_count;
    char topology_root[65];
    char build_id[65];

    char controlflow_root[65];
    char instruction_root[65];
    char register_root[65];
    char stack_root[65];

    int reachable_functions;
    int leaf_functions;
    int branch_nodes;
    int critical_path_depth;

    int mov_cnt;
    int call_cnt;
    int lea_cnt;
    int cmp_cnt;
    int jmp_cnt;
    int ret_cnt;

    int reg_counts[16];

    int max_stack_frame;
    int average_stack_frame;
    int recursive_functions;
} GenomeData;

typedef struct {
    char filename[256];
    char filepath[512];
    char version[32];
    int major;
    int minor;
    int patch;
    GenomeData data;
} GenomeRecord;

static void parse_genome(const char *json, GenomeData *g) {
    memset(g, 0, sizeof(GenomeData));

    const char *metadata_block = strstr(json, "\"metadata\"");
    const char *telemetry_block = strstr(json, "\"telemetry\"");
    const char *merkle_block = strstr(json, "\"merkle_topology\"");
    const char *fingerprint_block = strstr(json, "\"execution_fingerprint\"");
    const char *instruction_block = strstr(json, "\"instruction_profile\"");
    const char *register_block = strstr(json, "\"register_profile\"");
    const char *stack_block = strstr(json, "\"stack_analysis\"");

    if (metadata_block) {
        find_json_string_scoped(metadata_block, "\"build_id\"", g->build_id, sizeof(g->build_id));
    }
    if (telemetry_block) {
        find_json_int_scoped(telemetry_block, "\"functions_count\"", &g->functions_count);
        find_json_int_scoped(telemetry_block, "\"relocations_count\"", &g->relocations_count);
        find_json_int_scoped(telemetry_block, "\"symbols_count\"", &g->symbols_count);
    }
    if (merkle_block) {
        find_json_string_scoped(merkle_block, "\"topology_root\"", g->topology_root, sizeof(g->topology_root));
    }
    if (fingerprint_block) {
        find_json_string_scoped(fingerprint_block, "\"controlflow_root\"", g->controlflow_root, sizeof(g->controlflow_root));
        find_json_string_scoped(fingerprint_block, "\"instruction_root\"", g->instruction_root, sizeof(g->instruction_root));
        find_json_string_scoped(fingerprint_block, "\"register_root\"", g->register_root, sizeof(g->register_root));
        find_json_string_scoped(fingerprint_block, "\"stack_root\"", g->stack_root, sizeof(g->stack_root));

        find_json_int_scoped(fingerprint_block, "\"reachable_functions\"", &g->reachable_functions);
        find_json_int_scoped(fingerprint_block, "\"leaf_functions\"", &g->leaf_functions);
        find_json_int_scoped(fingerprint_block, "\"branch_nodes\"", &g->branch_nodes);
        find_json_int_scoped(fingerprint_block, "\"critical_path_depth\"", &g->critical_path_depth);
    }
    if (instruction_block) {
        find_json_int_scoped(instruction_block, "\"mov\"", &g->mov_cnt);
        find_json_int_scoped(instruction_block, "\"call\"", &g->call_cnt);
        find_json_int_scoped(instruction_block, "\"lea\"", &g->lea_cnt);
        find_json_int_scoped(instruction_block, "\"cmp\"", &g->cmp_cnt);
        find_json_int_scoped(instruction_block, "\"jmp\"", &g->jmp_cnt);
        find_json_int_scoped(instruction_block, "\"ret\"", &g->ret_cnt);
    }
    if (register_block) {
        find_json_int_scoped(register_block, "\"rax\"", &g->reg_counts[0]);
        find_json_int_scoped(register_block, "\"rcx\"", &g->reg_counts[1]);
        find_json_int_scoped(register_block, "\"rdx\"", &g->reg_counts[2]);
        find_json_int_scoped(register_block, "\"rbx\"", &g->reg_counts[3]);
        find_json_int_scoped(register_block, "\"rsp\"", &g->reg_counts[4]);
        find_json_int_scoped(register_block, "\"rbp\"", &g->reg_counts[5]);
        find_json_int_scoped(register_block, "\"rsi\"", &g->reg_counts[6]);
        find_json_int_scoped(register_block, "\"rdi\"", &g->reg_counts[7]);
        find_json_int_scoped(register_block, "\"r8\"",  &g->reg_counts[8]);
        find_json_int_scoped(register_block, "\"r9\"",  &g->reg_counts[9]);
        find_json_int_scoped(register_block, "\"r10\"", &g->reg_counts[10]);
        find_json_int_scoped(register_block, "\"r11\"", &g->reg_counts[11]);
        find_json_int_scoped(register_block, "\"r12\"", &g->reg_counts[12]);
        find_json_int_scoped(register_block, "\"r13\"", &g->reg_counts[13]);
        find_json_int_scoped(register_block, "\"r14\"", &g->reg_counts[14]);
        find_json_int_scoped(register_block, "\"r15\"", &g->reg_counts[15]);
    }
    if (stack_block) {
        find_json_int_scoped(stack_block, "\"max_stack_frame\"", &g->max_stack_frame);
        find_json_int_scoped(stack_block, "\"average_stack_frame\"", &g->average_stack_frame);
        find_json_int_scoped(stack_block, "\"recursive_functions\"", &g->recursive_functions);
    }
}

static int parse_version_string(const char *name, char *version_out, int *maj, int *min, int *pat) {
    *maj = 0;
    *min = 0;
    *pat = 0;
    if (name[0] != 'v') return 0;
    
    int len = 0;
    while (name[len] && name[len] != '.' && name[len] != '_') len++;
    
    // Check if filename ends with .json
    const char *ext = strstr(name, ".json");
    if (!ext) return 0;
    
    int version_len = ext - name;
    if (version_len > 31) version_len = 31;
    strncpy(version_out, name, version_len);
    version_out[version_len] = '\0';
    
    int fields = sscanf(version_out, "v%d.%d.%d", maj, min, pat);
    if (fields < 2) {
        *pat = 0;
        fields = sscanf(version_out, "v%d.%d", maj, min);
        if (fields < 2) return 0;
    }
    return 1;
}

static int cmp_records(const void *a, const void *b) {
    const GenomeRecord *ra = (const GenomeRecord *)a;
    const GenomeRecord *rb = (const GenomeRecord *)b;
    if (ra->major != rb->major) return ra->major - rb->major;
    if (ra->minor != rb->minor) return ra->minor - rb->minor;
    return ra->patch - rb->patch;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: zcc_genome_history <genomes_directory>\n");
        return 2;
    }

    const char *dir_path = argv[1];
    DIR *dir = opendir(dir_path);
    if (!dir) {
        fprintf(stderr, "error: cannot open directory %s\n", dir_path);
        return 2;
    }

    GenomeRecord records[MAX_GENOMES];
    int count = 0;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        
        char ver_name[32];
        int maj = 0, min = 0, pat = 0;
        if (!parse_version_string(entry->d_name, ver_name, &maj, &min, &pat)) {
            continue;
        }

        if (count >= MAX_GENOMES) break;

        GenomeRecord *r = &records[count];
        strcpy(r->filename, entry->d_name);
        sprintf(r->filepath, "%s/%s", dir_path, entry->d_name);
        strcpy(r->version, ver_name);
        r->major = maj;
        r->minor = min;
        r->patch = pat;

        size_t sz = 0;
        uint8_t *content = load_file(r->filepath, &sz);
        if (!content) {
            fprintf(stderr, "warning: cannot read %s\n", r->filepath);
            continue;
        }

        parse_genome((const char *)content, &r->data);
        free(content);
        count++;
    }
    closedir(dir);

    if (count == 0) {
        fprintf(stderr, "error: no valid v*.json genome files found in %s\n", dir_path);
        return 1;
    }

    // Sort records chronologically
    qsort(records, count, sizeof(GenomeRecord), cmp_records);

    printf("=== ZCC Compiler Lineage & Evolution History ===\n");
    printf("Registry Directory: %s\n", dir_path);
    printf("Found %d genomes in lineage.\n\n", count);

    // Compute version-by-version transitions
    int evolution_score = 100;
    int unstable_hops = 0;

    char lineage_path[512];
    char drift_path[512];
    char map_path[512];
    char report_path[512];

    sprintf(lineage_path, "%s/lineage.json", dir_path);
    sprintf(drift_path, "%s/drift_history.json", dir_path);
    sprintf(map_path, "%s/regression_map.json", dir_path);
    sprintf(report_path, "%s/evolution_report.json", dir_path);

    FILE *f_lin = fopen(lineage_path, "w");
    FILE *f_drift = fopen(drift_path, "w");
    FILE *f_map = fopen(map_path, "w");
    FILE *f_rep = fopen(report_path, "w");

    if (!f_lin || !f_drift || !f_map || !f_rep) {
        die("cannot write output registry files in directory");
    }

    // Write lineage.json header
    fprintf(f_lin, "{\n  \"schema\": \"zcc.lineage.v1\",\n  \"versions\": [\n");
    for (int i = 0; i < count; i++) {
        fprintf(f_lin, "    \"%s\"%s\n", records[i].version, (i == count - 1) ? "" : ",");
    }
    fprintf(f_lin, "  ],\n  \"relations\": [\n");

    // Write drift_history.json header
    fprintf(f_drift, "[\n");

    // Write regression_map.json header
    fprintf(f_map, "{\n");

    for (int i = 0; i < count; i++) {
        const char *parent = (i == 0) ? "none" : records[i-1].version;
        const char *child = records[i].version;

        if (i > 0) {
            // Write to lineage.json relations
            fprintf(f_lin, "    { \"parent\": \"%s\", \"child\": \"%s\" }%s\n", parent, child, (i == count - 1) ? "" : ",");

            // Compute transition drift
            GenomeData *ga = &records[i-1].data;
            GenomeData *gb = &records[i].data;

            int mov_diff = gb->mov_cnt - ga->mov_cnt;
            int call_diff = gb->call_cnt - ga->call_cnt;
            int lea_diff = gb->lea_cnt - ga->lea_cnt;
            int cmp_diff = gb->cmp_cnt - ga->cmp_cnt;
            int jmp_diff = gb->jmp_cnt - ga->jmp_cnt;
            int ret_diff = gb->ret_cnt - ga->ret_cnt;

            int stack_grow = gb->max_stack_frame - ga->max_stack_frame;

            int reg_drift = 0;
            for (int r = 0; r < 16; r++) {
                reg_drift += abs(gb->reg_counts[r] - ga->reg_counts[r]);
            }

            // Deduct points for excessive growth
            if (stack_grow > 32) evolution_score -= 5;
            if (reg_drift > 50) evolution_score -= 5;
            if (strcmp(ga->topology_root, gb->topology_root) != 0 && (ga->topology_root[0] != 0 && gb->topology_root[0] != 0)) {
                evolution_score -= 2;
            }

            // Determine if hop has significant instability (e.g. huge stack jump)
            int hop_unstable = (stack_grow > 64 || reg_drift > 100);
            if (hop_unstable) {
                unstable_hops++;
            }

            // Write drift history element
            fprintf(f_drift, "  {\n");
            fprintf(f_drift, "    \"transition\": \"%s -> %s\",\n", parent, child);
            fprintf(f_drift, "    \"instruction_drift\": {\n");
            fprintf(f_drift, "      \"mov\": %d,\n", mov_diff);
            fprintf(f_drift, "      \"call\": %d,\n", call_diff);
            fprintf(f_drift, "      \"lea\": %d,\n", -lea_diff); // formatting delta
            fprintf(f_drift, "      \"cmp\": %d,\n", cmp_diff);
            fprintf(f_drift, "      \"jmp\": %d,\n", jmp_diff);
            fprintf(f_drift, "      \"ret\": %d\n", ret_diff);
            fprintf(f_drift, "    },\n");
            fprintf(f_drift, "    \"register_drift\": {\n");
            fprintf(f_drift, "      \"total_drift_units\": %d\n", reg_drift);
            fprintf(f_drift, "    },\n");
            fprintf(f_drift, "    \"stack_growth\": {\n");
            fprintf(f_drift, "      \"max_frame_delta\": %d\n", stack_grow);
            fprintf(f_drift, "    }\n");
            fprintf(f_drift, "  }%s\n", (i == count - 1) ? "" : ",");

            // Print transition report to stdout
            printf("%s ➔ %s\n", parent, child);
            
            // Dynamic feature detection
            if (records[i-1].minor < 20 && records[i].minor >= 20) {
                printf("  [INTRODUCED] Execution Fingerprints\n");
            }
            if (records[i-1].minor < 21 && records[i].minor >= 21) {
                printf("  [INTRODUCED] Replay Packs\n");
            }
            if (records[i-1].minor < 22 && records[i].minor >= 22) {
                printf("  [INTRODUCED] Genome Registry\n");
            }
            if (records[i-1].minor < 23 && records[i].minor >= 23) {
                printf("  [INTRODUCED] Lineage Engine\n");
            }

            if (stack_grow != 0 || reg_drift != 0) {
                printf("  Drifts: Instruction= %+d mov, Register= %+d units, Stack= %+d bytes\n", 
                       mov_diff, reg_drift, stack_grow);
            } else {
                printf("  Stable\n");
            }
            printf("\n");
        }

        // Write to regression_map.json
        const char *risk = "LOW";
        if (records[i].data.max_stack_frame > 512) risk = "HIGH";
        else if (records[i].data.max_stack_frame > 256) risk = "MEDIUM";

        fprintf(f_map, "  \"%s\": {\n", records[i].version);
        fprintf(f_map, "    \"risk\": \"%s\",\n", risk);
        fprintf(f_map, "    \"max_frame\": %d\n", records[i].data.max_stack_frame);
        fprintf(f_map, "  }%s\n", (i == count - 1) ? "" : ",");
    }

    if (evolution_score < 70) evolution_score = 70;

    // Close lineage.json
    fprintf(f_lin, "  ]\n}\n");
    fclose(f_lin);

    // Close drift_history.json
    fprintf(f_drift, "]\n");
    fclose(f_drift);

    // Close regression_map.json
    fprintf(f_map, "}\n");
    fclose(f_map);

    // Write evolution_report.json
    fprintf(f_rep, "{\n");
    fprintf(f_rep, "  \"version\": \"%s\",\n", records[count - 1].version);
    fprintf(f_rep, "  \"evolution_score\": %d,\n", evolution_score);
    fprintf(f_rep, "  \"total_transitions\": %d,\n", count - 1);
    fprintf(f_rep, "  \"unstable_hops\": %d\n", unstable_hops);
    fprintf(f_rep, "}\n");
    fclose(f_rep);

    printf("Compiler Evolution Score: %d/100\n", evolution_score);

    return 0;
}
