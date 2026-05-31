/* ================================================================= */
/* 🔱 ZCC SOVEREIGN CONSTITUTIONAL ORACLE SUBSTRATE HEADER          */
/* Pure C Declarations for the Verifiable Computational Substrate    */
/* ================================================================= */

#ifndef ZCC_ORACLE_SUBSTRATE_H
#define ZCC_ORACLE_SUBSTRATE_H

#include <stdio.h>
#include <stdint.h>

#define MAX_FINDINGS 1024
#define FINGERPRINT_LEN 64

/* Telemetry Invariant telemetry elements */
typedef struct {
    char type[64];
    char message[256];
    int line_number;
    int critical;
} InvariantFinding;

/* Core Oracle Report profile */
typedef struct {
    const char *oracle_name;
    const char *status; /* "VERIFIED" or "DIVERGENCE_DETECTED" */
    char fingerprint[FINGERPRINT_LEN + 1];
    InvariantFinding findings[MAX_FINDINGS];
    int num_findings;
} OracleReport;

/* Decoupled invariant checker abstraction */
typedef struct {
    const char *name;
    int (*run)(void *cc, OracleReport *report);
} InvariantOracle;

/* Shared Oracle Registry */
typedef struct {
    InvariantOracle oracles[32];
    int num_oracles;
} OracleRegistry;

/* ================================================================= */
/* STABLE COMPILER FINGERPRINTS                                      */
/* ================================================================= */

typedef struct {
    uint64_t cfg_topology_hash;      /* Structural control-flow complexity */
    uint64_t abi_lowering_hash;      /* Calling convention register maps */
    uint64_t stack_geometry_hash;    /* Local/spill alignments and offsets */
    uint64_t regalloc_sequence_hash; /* Graph coloring allocation order */
    uint64_t pass_pipeline_hash;     /* Active optimization pass traces */
    uint64_t entropy_seed_hash;      /* Environmental baseline signature */
} CompilerFingerprint;

/* ================================================================= */
/* MINIMAL DETERMINISTIC RUNTIME CONTEXT                             */
/* ================================================================= */

typedef struct {
    const char *gate_name;
    int passed;
    const char *metric;
} OracleGate;

typedef struct {
    OracleGate gates[16];
    int gate_count;
    const char *constitutional_verdict;
} OracleVerdict;

/* ================================================================= */
/* CANONICAL PASS EVENT RECORDER (ZXR PRIMITIVES)                    */
/* ================================================================= */

void record_pass_begin(const char *pass_name);
void record_pass_end(const char *pass_name);
void record_transform(const char *pass_name, uint64_t node_id, const char *transform_details);

void zxr_emit_record(const char *source_filename, const char *zxr_output_filename);
int zxr_replay_record(const char *zxr_input_filename);

/* ================================================================= */
/* PUSH/POP MICRO-THEOREM PROTOTYPE                                 */
/* ================================================================= */

typedef struct {
    char theorem_name[64];
    char target_pass[64];
    uint64_t node_id;
    int verified;
    int delta_rsp;
    int preserves_register;
    int preserves_flags;
} ZXRProof;

void record_proof(
    const char *theorem_name,
    const char *target_pass,
    uint64_t node_id,
    int verified,
    int delta_rsp,
    int preserves_register,
    int preserves_flags
);

typedef struct {
    int stack_delta;
    int preserves_rax;
    int preserves_flags;
} PushPopTheorem;

/* ================================================================= */
/* CANONICAL CFG FINGERPRINTER DFS TOPOLOGY                          */
/* ================================================================= */

typedef struct CFGNode CFGNode;

struct CFGNode {
    int id;
    CFGNode *edges[8];
    int edge_count;
    int visited;
};

uint64_t compute_cfg_topology_hash(void *fn_ptr);
void assert_cfg_invariance(const char *pass_name, uint64_t hash_pre, uint64_t hash_post);


/* ================================================================= */
/* OBSERVATIONAL DOMAIN SEPARATION                                   */
/* ================================================================= */

typedef struct {
    void *ast_codegen;
    void *ssa_ir;
    void *telemetry;
    void *oracle;
    void *proof;
} SovereignDomains;

/* ================================================================= */
/* SYMBOLIC ABI LOWERING ORACLE                                      */
/* ================================================================= */

typedef enum {
    ABI_GPR,
    ABI_SSE,
    ABI_STACK,
    ABI_SRET
} AbiClass;

typedef struct {
    const char *name;
    AbiClass cls;
    int slot;
} AbiLane;

void run_oracle_abi(void *cc, void *prog);
void run_oracle_layout(void *cc);
void run_oracle_determinism(void *cc, const char *input_file, const char *include_paths, const char *define_flags);
void run_oracle_selfhost(void);
void run_oracle_stack(void *cc, void *prog);

#endif /* ZCC_ORACLE_SUBSTRATE_H */
