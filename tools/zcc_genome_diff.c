#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

static void die(const char *msg) {
    fprintf(stderr, "zcc_genome_diff: fatal: %s\n", msg);
    exit(1);
}

static uint8_t *load_file(const char *path, size_t *sz) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        return NULL;
    }
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

int main(int argc, char **argv) {
    if (argc < 3) {
        printf("Usage: zcc_genome_diff <genome_a.json> <genome_b.json>\n");
        return 2;
    }

    const char *path_a = argv[1];
    const char *path_b = argv[2];

    size_t sz_a = 0, sz_b = 0;
    uint8_t *data_a = load_file(path_a, &sz_a);
    uint8_t *data_b = load_file(path_b, &sz_b);

    if (!data_a) {
        fprintf(stderr, "error: cannot open %s\n", path_a);
        return 2;
    }
    if (!data_b) {
        fprintf(stderr, "error: cannot open %s\n", path_b);
        free(data_a);
        return 2;
    }

    GenomeData ga, gb;
    parse_genome((const char *)data_a, &ga);
    parse_genome((const char *)data_b, &gb);

    int drift_detected = 0;

    printf("=== ZCC Compiler Genome Drift Analysis ===\n");
    printf("Genome A: %s\n", path_a);
    printf("Genome B: %s\n\n", path_b);

    /* 1. Topology Drift */
    int fn_diff = gb.functions_count - ga.functions_count;
    int rel_diff = gb.relocations_count - ga.relocations_count;
    int sym_diff = gb.symbols_count - ga.symbols_count;
    int root_match = (strcmp(ga.topology_root, gb.topology_root) == 0);

    if (fn_diff != 0 || rel_diff != 0 || sym_diff != 0 || !root_match) {
        drift_detected = 1;
    }

    printf("Topology:\n");
    printf("  Functions:          A: %d, B: %d (Drift: %+d)\n", ga.functions_count, gb.functions_count, fn_diff);
    printf("  Relocations:        A: %d, B: %d (Drift: %+d)\n", ga.relocations_count, gb.relocations_count, rel_diff);
    printf("  Symbols:            A: %d, B: %d (Drift: %+d)\n", ga.symbols_count, gb.symbols_count, sym_diff);
    printf("  Merkle Root Match:  %s\n", root_match ? "PASS" : "FAIL");
    printf("\n");

    /* 2. Opcode Profile Shift */
    int mov_diff = gb.mov_cnt - ga.mov_cnt;
    int call_diff = gb.call_cnt - ga.call_cnt;
    int lea_diff = gb.lea_cnt - ga.lea_cnt;
    int cmp_diff = gb.cmp_cnt - ga.cmp_cnt;
    int jmp_diff = gb.jmp_cnt - ga.jmp_cnt;
    int ret_diff = gb.ret_cnt - ga.ret_cnt;

    if (mov_diff != 0 || call_diff != 0 || lea_diff != 0 || cmp_diff != 0 || jmp_diff != 0 || ret_diff != 0) {
        drift_detected = 1;
    }

    printf("Instruction Profile Drift:\n");
    printf("  mov:   %4d -> %4d (%+d)\n", ga.mov_cnt, gb.mov_cnt, mov_diff);
    printf("  call:  %4d -> %4d (%+d)\n", ga.call_cnt, gb.call_cnt, call_diff);
    printf("  lea:   %4d -> %4d (%+d)\n", ga.lea_cnt, gb.lea_cnt, lea_diff);
    printf("  cmp:   %4d -> %4d (%+d)\n", ga.cmp_cnt, gb.cmp_cnt, cmp_diff);
    printf("  jmp:   %4d -> %4d (%+d)\n", ga.jmp_cnt, gb.jmp_cnt, jmp_diff);
    printf("  ret:   %4d -> %4d (%+d)\n", ga.ret_cnt, gb.ret_cnt, ret_diff);
    printf("\n");

    /* 3. Register Pressure Shift */
    int reg_drift = 0;
    const char *reg_names[16] = {
        "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
        "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"
    };

    printf("Register Pressure Shift:\n");
    for (int i = 0; i < 16; i++) {
        int diff = gb.reg_counts[i] - ga.reg_counts[i];
        if (diff != 0) {
            printf("  %-3s:  %4d -> %4d (%+d)\n", reg_names[i], ga.reg_counts[i], gb.reg_counts[i], diff);
            reg_drift += abs(diff);
        }
    }
    if (reg_drift > 0) {
        drift_detected = 1;
        printf("  Overall Register Drift: %d units\n", reg_drift);
    } else {
        printf("  Overall Register Drift: unchanged\n");
    }
    printf("\n");

    /* 4. Stack Growth Shift */
    int max_stk_diff = gb.max_stack_frame - ga.max_stack_frame;
    int avg_stk_diff = gb.average_stack_frame - ga.average_stack_frame;

    if (max_stk_diff != 0 || avg_stk_diff != 0) {
        drift_detected = 1;
    }

    printf("Stack Growth Shift:\n");
    printf("  Max Stack Frame:    %d -> %d (%+d bytes)\n", ga.max_stack_frame, gb.max_stack_frame, max_stk_diff);
    printf("  Average Frame:      %d -> %d (%+d bytes)\n", ga.average_stack_frame, gb.average_stack_frame, avg_stk_diff);
    printf("\n");

    /* 5. Attestation Validity */
    int cf_match = (strcmp(ga.controlflow_root, gb.controlflow_root) == 0);
    int inst_match = (strcmp(ga.instruction_root, gb.instruction_root) == 0);
    int reg_match = (strcmp(ga.register_root, gb.register_root) == 0);
    int stk_match = (strcmp(ga.stack_root, gb.stack_root) == 0);

    printf("Attestation Status:\n");
    printf("  Genome A Build ID:  %s\n", ga.build_id[0] ? ga.build_id : "none");
    printf("  Genome B Build ID:  %s\n", gb.build_id[0] ? gb.build_id : "none");
    printf("  Control-flow Match: %s\n", cf_match ? "PASS" : "FAIL");
    printf("  Instruction Match:  %s\n", inst_match ? "PASS" : "FAIL");
    printf("  Register Match:     %s\n", reg_match ? "PASS" : "FAIL");
    printf("  Stack Match:        %s\n", stk_match ? "PASS" : "FAIL");
    printf("\n");

    printf("Genome Analysis:\n%s\n", drift_detected ? "DIVERGED" : "IDENTICAL");

    free(data_a);
    free(data_b);

    return drift_detected ? 1 : 0;
}
