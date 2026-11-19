/*
 * ZCC External Pass Stubs for Freestanding WASM Self-Host (src/wasm_zcc_stubs.c)
 * ==============================================================================
 * Provides stub definitions for the external side-pass/backend symbols declared
 * in zcc.c + src/wasm_emit.c that live outside `$(PARTS)` and `wasm_wasi_rt.c`.
 * Included at the end of `zcc_wasm_bundle.c` so all zcc.c typedefs are in scope.
 */

/* --- External Pass Globals (exact types from zcc.c headers) --- */
int g_emit_smt_proofs = 0;
char g_ir_export_path[256] = {0};
int g_manifold_enabled = 0;
int g_peephole_deterministic = 1;
int g_peephole_enabled = 1;
int g_peephole_verbose = 0;
char g_smt_proofs_dir[256] = {0};

/* --- EVM / Yul Stubs --- */
uint8_t *compile_yul_to_evm(const char *yul_source, size_t *out_len) {
    (void)yul_source;
    if (out_len) *out_len = 0;
    return NULL;
}

void evm_decompile(const unsigned char *bytecode, size_t len, const char *output_path, int abi_mode) {
    (void)bytecode; (void)len; (void)output_path; (void)abi_mode;
}

void evm_jit_entry(const unsigned char *bytecode, size_t len, const char *output_path) {
    (void)bytecode; (void)len; (void)output_path;
}

void evm_run_symbolic_from_bytecode(const unsigned char *bytecode, size_t len, int smt_mode) {
    (void)bytecode; (void)len; (void)smt_mode;
}

void evm_symbolic_run(const unsigned char *bytecode, size_t len, const char *prop) {
    (void)bytecode; (void)len; (void)prop;
}

void evm_yul_weaver(ir_func_t *fn, FILE *out) {
    (void)fn; (void)out;
}

void export_smt2(ir_func_t *graph, const char *path) {
    (void)graph; (void)path;
}

/* --- IPC Bridge Stubs --- */
void ipc_bridge_compile_and_strike(char *payload) { (void)payload; }
int ipc_bridge_init(void) { return -1; }
FILE *ipc_bridge_open_tx(void) { return NULL; }
char *ipc_bridge_receive(void) { return NULL; }

/* --- IR Dominance / Pass Manager / Serialization Stubs --- */
int dom_build_cfg(dom_cfg_t *cfg, ir_func_t *fn) { (void)cfg; (void)fn; return -1; }
void dom_compute_idom(dom_cfg_t *cfg) { (void)cfg; }
int dom_dominates(const dom_cfg_t *cfg, int dominator, int block) { (void)cfg; (void)dominator; (void)block; return 0; }
int dom_find_block_for_node(const dom_cfg_t *cfg, const ir_node_t *node) { (void)cfg; (void)node; return -1; }

int ir_deserialize_json(ir_module_t *mod, const char *in_filename, Compiler *cc) {
    (void)mod; (void)in_filename; (void)cc; return -1;
}
int ir_diff_json(const char *path_a, const char *path_b) {
    (void)path_a; (void)path_b; return -1;
}
void ir_pm_run_default(void *mod_ptr, int verbose) {
    (void)mod_ptr; (void)verbose;
}
int ir_serialize_json(const ir_module_t *mod, const char *out_filename, const char *source_file, const Compiler *cc) {
    (void)mod; (void)out_filename; (void)source_file; (void)cc; return -1;
}

/* --- Telemetry / Proof / Oracle / Alert Stubs --- */
void lucky_alert_emit(uint8_t mask, const char *context) { (void)mask; (void)context; }
void record_pass_begin(const char *pass_name) { (void)pass_name; }
void record_pass_end(const char *pass_name) { (void)pass_name; }
void record_proof(const char *theorem_name, const char *target_pass, uint64_t node_id,
                  int verified, int delta_rsp, int preserves_register, int preserves_flags) {
    (void)theorem_name; (void)target_pass; (void)node_id;
    (void)verified; (void)delta_rsp; (void)preserves_register; (void)preserves_flags;
}
void record_transform(const char *pass_name, uint64_t node_id, const char *transform_details) {
    (void)pass_name; (void)node_id; (void)transform_details;
}

void run_abi_trace(void *cc, void *prog) { (void)cc; (void)prog; }
void run_oracle_abi(void *cc, void *prog) { (void)cc; (void)prog; }
void run_oracle_determinism(void *cc, const char *input_file, const char *include_paths, const char *define_flags) {
    (void)cc; (void)input_file; (void)include_paths; (void)define_flags;
}
void run_oracle_layout(void *cc) { (void)cc; }
void run_oracle_selfhost(void) {}
void run_oracle_stack(void *cc, void *prog) { (void)cc; (void)prog; }

void smt_prove_arith_nullification(const char *instruction, const char *reg, size_t line_index) {
    (void)instruction; (void)reg; (void)line_index;
}
void smt_prove_push_lea_pop_triad(const char *lea_instruction, const char *pop_reg, size_t line_index) {
    (void)lea_instruction; (void)pop_reg; (void)line_index;
}
void smt_prove_push_pop_elision(const char *reg1, const char *reg2, int is_replaced, size_t line_index) {
    (void)reg1; (void)reg2; (void)is_replaced; (void)line_index;
}

void zcc_oracle_init(const char *source_file) { (void)source_file; }
void zcc_oracle_log_allocation(void *ptr, size_t size) { (void)ptr; (void)size; }
void zcc_oracle_log_pass(const char *pass_name, const char *function_name, double duration_ms,
                         size_t heap_bytes, int spills, int virtual_regs, int physical_regs) {
    (void)pass_name; (void)function_name; (void)duration_ms;
    (void)heap_bytes; (void)spills; (void)virtual_regs; (void)physical_regs;
}
void zcc_oracle_log_sentinel(size_t idt_size, size_t gdt_size, size_t base_offset,
                             size_t idt_entry_size, size_t gdt_entry_size, size_t multiboot_offset) {
    (void)idt_size; (void)gdt_size; (void)base_offset;
    (void)idt_entry_size; (void)gdt_entry_size; (void)multiboot_offset;
}
void zcc_oracle_shutdown(void) {}

/* --- QASM / Quantum Stubs --- */
void zcc_qasm_c_emit_config_default(ZCCQasmCEmitConfig *cfg) { (void)cfg; }
void zcc_qasm_circuit_free(ZCCQasmCircuit *circ) { (void)circ; }
int zcc_qasm_circuit_is_clifford(const ZCCQasmCircuit *circ) { (void)circ; return 0; }
char *zcc_qasm_emit_c_code(const ZCCQasmCircuit *circ, const ZCCQasmCEmitConfig *config,
                           char *err_buf, size_t err_buf_size) {
    (void)circ; (void)config;
    if (err_buf && err_buf_size > 0) err_buf[0] = '\0';
    return NULL;
}
int zcc_qasm_emit_c_file(const ZCCQasmCircuit *circ, const char *filepath, const ZCCQasmCEmitConfig *config,
                         char *err_buf, size_t err_buf_size) {
    (void)circ; (void)filepath; (void)config;
    if (err_buf && err_buf_size > 0) err_buf[0] = '\0';
    return -1;
}
char *zcc_qasm_emit_canonical(const ZCCQasmCircuit *circ) { (void)circ; return NULL; }
int zcc_qasm_emit_file(const ZCCQasmCircuit *circ, const char *output_file) {
    (void)circ; (void)output_file; return -1;
}
int zcc_qasm_optimize(const ZCCQasmCircuit *input, ZCCQasmCircuit **output, const ZCCQasmOptConfig *config,
                      ZCCQasmOptStats *stats, char *err_buf, size_t err_buf_size) {
    (void)input; (void)config; (void)stats;
    if (output) *output = NULL;
    if (err_buf && err_buf_size > 0) err_buf[0] = '\0';
    return -1;
}
ZCCQasmCircuit *zcc_qasm_parse_string(const char *source, const char *filename,
                                      char *err_buf, size_t err_buf_size) {
    (void)source; (void)filename;
    if (err_buf && err_buf_size > 0) err_buf[0] = '\0';
    return NULL;
}
int zcc_qasm_sim_apply_circuit(ZCCQasmSimulator *sim, const ZCCQasmCircuit *circ) {
    (void)sim; (void)circ; return -1;
}
ZCCQasmSimulator *zcc_qasm_sim_create(size_t num_qubits, size_t num_clbits, uint64_t seed) {
    (void)num_qubits; (void)num_clbits; (void)seed; return NULL;
}
char *zcc_qasm_sim_dump_state(const ZCCQasmSimulator *sim, double threshold) {
    (void)sim; (void)threshold; return NULL;
}
void zcc_qasm_sim_free(ZCCQasmSimulator *sim) { (void)sim; }
int zcc_qasm_stab_apply_circuit(ZCCQasmStabilizer *st, const ZCCQasmCircuit *circ) {
    (void)st; (void)circ; return -1;
}
ZCCQasmStabilizer *zcc_qasm_stab_create(size_t num_qubits, size_t num_clbits, uint64_t seed) {
    (void)num_qubits; (void)num_clbits; (void)seed; return NULL;
}
char *zcc_qasm_stab_dump_state(const ZCCQasmStabilizer *st) { (void)st; return NULL; }
void zcc_qasm_stab_free(ZCCQasmStabilizer *st) { (void)st; }
int zcc_qasm_transpile_clifford_t(const ZCCQasmCircuit *input, ZCCQasmCircuit **output,
                                  const ZCCQasmCliffordTConfig *config, ZCCQasmCliffordTStats *stats,
                                  char *err_buf, size_t err_buf_size) {
    (void)input; (void)config; (void)stats;
    if (output) *output = NULL;
    if (err_buf && err_buf_size > 0) err_buf[0] = '\0';
    return -1;
}
int zcc_qasm_validate(const ZCCQasmCircuit *circ, char *err_buf, size_t err_buf_size) {
    (void)circ;
    if (err_buf && err_buf_size > 0) err_buf[0] = '\0';
    return 0;
}

/* --- Miscellaneous Side-Pass / Bridge / Linker Stubs --- */
struct ZCCNode *zcc_node_from(struct Node *ast) { (void)ast; return NULL; }

int zcc_compile_svg_asset(const char *input_path, const char *source, int source_len,
                          const char *output_path, int compile_only, int quiet) {
    (void)input_path; (void)source; (void)source_len;
    (void)output_path; (void)compile_only; (void)quiet;
    return -1;
}

int zcc_emit_gguf(void *cc, const char *out_path, int quantize_type) {
    (void)cc; (void)out_path; (void)quantize_type; return -1;
}

int zcc_run_passes_emit_body_pgo(ZCCNode *body_ast, const char *profile_path, const char *func_name,
                                 void *out_file, int stack_size, int num_params,
                                 int func_end_label, int func_label_id) {
    (void)body_ast; (void)profile_path; (void)func_name; (void)out_file;
    (void)stack_size; (void)num_params; (void)func_end_label; (void)func_label_id;
    return 0;
}

void zcc_sculpt(const char *prompt, const char *output_file) { (void)prompt; (void)output_file; }
void zcc_sculpt_sdf(const char *output_file, int resolution) { (void)output_file; (void)resolution; }

int zcc_stark_synthesize_proof(ir_module_t *mod, const char *output_path) {
    (void)mod; (void)output_path; return -1;
}

int zcc_svg_cli_dispatch(int mode, const char *input_path, const char *source, int source_len,
                         const char *output_path, int explicit_output, int quiet) {
    (void)mode; (void)input_path; (void)source; (void)source_len;
    (void)output_path; (void)explicit_output; (void)quiet;
    return -1;
}

int zcc_x86_peephole_boost(char **line_ptrs, int *nlines_ptr) {
    (void)line_ptrs; (void)nlines_ptr; return 0;
}

int zld_link(const char **obj_files, int obj_count, const char *out_path, const char *script_path,
             const char *tensor_attest_bin_path, const char *tensor_note_json_path,
             const char *build_attest_bin_path) {
    (void)obj_files; (void)obj_count; (void)out_path; (void)script_path;
    (void)tensor_attest_bin_path; (void)tensor_note_json_path; (void)build_attest_bin_path;
    return -1;
}

void zxr_emit_record(const char *source_filename, const char *zxr_output_filename) {
    (void)source_filename; (void)zxr_output_filename;
}

int zxr_replay_record(const char *zxr_input_filename) {
    (void)zxr_input_filename; return -1;
}

extern int zcc_main(int argc, char **argv);
int main(int argc, char **argv) {
    return zcc_main(argc, argv);
}

