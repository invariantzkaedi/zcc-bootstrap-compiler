#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

static void die(const char *msg) {
    fprintf(stderr, "zcc_time_machine: fatal: %s\n", msg);
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
    const char *from_ver = NULL;
    const char *to_ver = NULL;
    const char *registry_dir = ".";

    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "--from") == 0 || strcmp(argv[i], "-from") == 0) && i + 1 < argc) {
            from_ver = argv[++i];
        } else if ((strcmp(argv[i], "--to") == 0 || strcmp(argv[i], "-to") == 0) && i + 1 < argc) {
            to_ver = argv[++i];
        } else if ((strcmp(argv[i], "--registry") == 0 || strcmp(argv[i], "-registry") == 0) && i + 1 < argc) {
            registry_dir = argv[++i];
        }
    }

    if (!from_ver || !to_ver) {
        printf("Usage: zcc_time_machine --from <version> --to <version> [--registry <genomes_directory>]\n");
        return 2;
    }

    DIR *dir = opendir(registry_dir);
    if (!dir) {
        fprintf(stderr, "error: cannot open registry directory %s\n", registry_dir);
        return 2;
    }

    GenomeRecord records[64];
    int count = 0;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        char ver_name[32];
        int maj = 0, min = 0, pat = 0;
        if (!parse_version_string(entry->d_name, ver_name, &maj, &min, &pat)) continue;

        if (count >= 64) break;

        GenomeRecord *r = &records[count];
        strcpy(r->filename, entry->d_name);
        sprintf(r->filepath, "%s/%s", registry_dir, entry->d_name);
        strcpy(r->version, ver_name);
        r->major = maj;
        r->minor = min;
        r->patch = pat;

        size_t sz = 0;
        uint8_t *content = load_file(r->filepath, &sz);
        if (!content) continue;
        parse_genome((const char *)content, &r->data);
        free(content);
        count++;
    }
    closedir(dir);

    // Sort records
    qsort(records, count, sizeof(GenomeRecord), cmp_records);

    // Find the requested versions in records
    int from_idx = -1;
    int to_idx = -1;

    for (int i = 0; i < count; i++) {
        if (strcmp(records[i].version, from_ver) == 0) from_idx = i;
        if (strcmp(records[i].version, to_ver) == 0) to_idx = i;
    }

    if (from_idx == -1) {
        fprintf(stderr, "error: version '%s' not found in registry\n", from_ver);
        return 1;
    }
    if (to_idx == -1) {
        fprintf(stderr, "error: version '%s' not found in registry\n", to_ver);
        return 1;
    }

    GenomeData *ga = &records[from_idx].data;
    GenomeData *gb = &records[to_idx].data;

    int topo_drift = 0;
    if (strcmp(ga->topology_root, gb->topology_root) != 0 && (ga->topology_root[0] != 0 && gb->topology_root[0] != 0)) {
        topo_drift = 1; // Topology mutated
    }

    int mov_diff = gb->mov_cnt - ga->mov_cnt;
    int reg_drift = 0;
    char max_drift_reg[16] = "none";
    int max_drift_val = 0;

    const char *reg_names[16] = {
        "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
        "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"
    };

    for (int r = 0; r < 16; r++) {
        int diff = gb->reg_counts[r] - ga->reg_counts[r];
        reg_drift += abs(diff);
        if (abs(diff) > max_drift_val) {
            max_drift_val = abs(diff);
            strcpy(max_drift_reg, reg_names[r]);
        }
    }

    int stack_grow = gb->max_stack_frame - ga->max_stack_frame;

    // Feature detection
    char fingerprints_intro[32] = "N/A";
    char replay_intro[32] = "N/A";
    char registry_intro[32] = "N/A";

    for (int i = 0; i < count; i++) {
        if (strcmp(fingerprints_intro, "N/A") == 0 && records[i].data.controlflow_root[0] != '\0') {
            strcpy(fingerprints_intro, records[i].version);
        }
        if (strcmp(replay_intro, "N/A") == 0 && records[i].minor >= 21) {
            strcpy(replay_intro, records[i].version);
        }
        if (strcmp(registry_intro, "N/A") == 0 && records[i].data.build_id[0] != '\0') {
            strcpy(registry_intro, records[i].version);
        }
    }

    const char *risk = "LOW";
    if (stack_grow > 64 || reg_drift > 100) risk = "HIGH";
    else if (stack_grow > 32 || reg_drift > 50) risk = "MEDIUM";

    printf("Topology Drift:          %+d\n", topo_drift);
    printf("Instruction Drift:       %+d mov\n", mov_diff);
    printf("Register Drift:          %+d %s\n", gb->reg_counts[0] - ga->reg_counts[0], max_drift_reg);
    printf("Stack Growth:            %+d bytes\n", stack_grow);
    printf("\n");
    printf("First Version Introducing:\n");
    printf("  Replay Packs      : %s\n", replay_intro);
    printf("  Genome Registry   : %s\n", registry_intro);
    printf("\n");
    printf("Regression Risk:\n  %s\n", risk);

    return 0;
}
