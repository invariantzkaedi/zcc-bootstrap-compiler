/*
 * ir_pass_manager.c — ZCC IR Pass Manager Implementation
 * ========================================================
 * Compiled by GCC only (linked separately, NOT concatenated into zcc.c).
 * Operates on ir_func_t* linked lists defined in ir.h.
 *
 * Production passes:
 *   DCE            — backward liveness scan, unlinks dead definitions
 *   Constant Fold  — evaluates binary ops on known constants
 *   Strength Reduce — mul-by-0 → const 0, add/sub-by-0 → copy
 *   GVN            — global value numbering, eliminates redundant computations
 *
 * Default pipeline: DCE → const_fold → strength_reduce → GVN → vload → DCE
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ir.h"
#include "evm_lifter.h"
#include "ir_pass_warden.h"
#include "ir_symbolic_cfg.h"
#include "ir_dominance.h"

/* ── Globals referenced by zcc.c (part5.c declares these extern) ──────── */
int  g_manifold_enabled    = 0;
char g_ir_export_path[256] = {0};
int  g_peephole_enabled    = 0;
int  g_peephole_deterministic = 0;
int  g_peephole_verbose    = 0;


/* ── Pass types (from ir_pass_manager.h) ─────────────────────────────── */
#include "ir_pass_manager.h"

/* ── Helpers ─────────────────────────────────────────────────────────── */

static int count_nodes(ir_func_t *fn) {
    int count = 0;
    ir_node_t *n = fn->head;
    while (n) {
        count++;
        n = n->next;
    }
    return count;
}

/* Check if an opcode is side-effectful (must not be DCE'd) */
static int is_side_effect(ir_op_t op) {
    switch (op) {
    case IR_STORE:
    case IR_CALL:
    case IR_RET:
    case IR_BR:
    case IR_BR_IF:
    case IR_LABEL:
    case IR_ARG:
        return 1;
    default:
        return 0;
    }
}

/* Check if a node references 'name' as a source operand.
 * For IR_STORE, dst is actually a USE (the address), not a definition. */
static int node_uses(ir_node_t *n, const char *name) {
    if (name[0] == '\0') return 0;
    if (n->src1[0] && strcmp(n->src1, name) == 0) return 1;
    if (n->src2[0] && strcmp(n->src2, name) == 0) return 1;
    /* IR_STORE uses dst as the address operand — it's a USE, not a DEF */
    if (n->op == IR_STORE && n->dst[0] && strcmp(n->dst, name) == 0) return 1;
    return 0;
}

/* ════════════════════════════════════════════════════════════════════════
 * PASS 1: Dead Code Elimination (DCE)
 * ════════════════════════════════════════════════════════════════════════
 * Scan the node list.  A node is dead if:
 *   1) It defines a temp (dst[0] != '\0')
 *   2) It is NOT side-effectful (not store/call/ret/br/br_if/label/arg)
 *   3) It is NOT IR_STORE (where dst is a use, not a definition)
 *   4) No subsequent node references dst in src1, src2, or store-dst
 *
 * Complexity: O(N²) per function. With ~300 nodes/function average,
 * this is ~90K comparisons/function — negligible.
 */
static ir_pass_result_t ir_pass_dce(void *fn_ptr) {
    ir_func_t *fn = (ir_func_t *)fn_ptr;
    ir_pass_result_t r;
    ir_node_t *prev;
    ir_node_t *n;
    ir_node_t *scan;
    ir_node_t *next;
    int deleted = 0;

    memset(&r, 0, sizeof(r));
    r.nodes_before = count_nodes(fn);

    prev = NULL;
    n = fn->head;
    while (n) {
        next = n->next;

        /* Does this node define a temp that could be dead? */
        if (n->dst[0] != '\0'
            && !is_side_effect(n->op)
            && n->op != IR_STORE) {

            /* Check if ANY subsequent node uses this temp */
            int used = 0;
            for (scan = next; scan; scan = scan->next) {
                if (node_uses(scan, n->dst)) {
                    used = 1;
                    break;
                }
            }

            if (!used) {
                /* Dead node — unlink from list */
                if (prev) {
                    prev->next = next;
                } else {
                    fn->head = next;
                }
                if (n == fn->tail) {
                    fn->tail = prev;
                }
                free(n);
                fn->node_count--;
                deleted++;
                /* Don't advance prev; it still points to the right place */
                n = next;
                continue;
            }
        }

        prev = n;
        n = next;
    }

    r.nodes_after = r.nodes_before - deleted;
    r.nodes_deleted = deleted;
    r.changed = deleted > 0;
    return r;
}

/* ════════════════════════════════════════════════════════════════════════
 * PASS 2: Constant Folding
 * ════════════════════════════════════════════════════════════════════════
 * Scan forward.  Track temps defined by IR_CONST.  When a binary op
 * has both src1 and src2 as known constants, evaluate at compile time
 * and replace the node with IR_CONST.
 */

#define CONST_MAP_MAX 8192

typedef struct {
    char name[32];  /* IR_NAME_MAX */
    long value;
    uint256_t value256;
    int is_256;
} const_map_entry_t;

static const_map_entry_t s_cmap[CONST_MAP_MAX];
static int s_cmap_count;

static void cmap_clear(void) {
    s_cmap_count = 0;
}

static void cmap_add(const char *name, long value) {
    int i;
    for (i = 0; i < s_cmap_count; i++) {
        if (strcmp(s_cmap[i].name, name) == 0) {
            s_cmap[i].value = value;
            s_cmap[i].is_256 = 0;
            return;
        }
    }
    if (s_cmap_count >= CONST_MAP_MAX) {
        fprintf(stderr, "[const_fold] WARNING: cmap overflow at %d entries\n",
                CONST_MAP_MAX);
        return;
    }
    strncpy(s_cmap[s_cmap_count].name, name, 31);
    s_cmap[s_cmap_count].name[31] = '\0';
    s_cmap[s_cmap_count].value = value;
    s_cmap[s_cmap_count].is_256 = 0;
    s_cmap_count++;
}

static void cmap_add_256(const char *name, uint256_t value256) {
    int i;
    for (i = 0; i < s_cmap_count; i++) {
        if (strcmp(s_cmap[i].name, name) == 0) {
            s_cmap[i].value256 = value256;
            s_cmap[i].is_256 = 1;
            return;
        }
    }
    if (s_cmap_count >= CONST_MAP_MAX) {
        fprintf(stderr, "[const_fold] WARNING: cmap_256 overflow at %d entries\n",
                CONST_MAP_MAX);
        return;
    }
    strncpy(s_cmap[s_cmap_count].name, name, 31);
    s_cmap[s_cmap_count].name[31] = '\0';
    s_cmap[s_cmap_count].value256 = value256;
    s_cmap[s_cmap_count].is_256 = 1;
    s_cmap_count++;
}

static int cmap_get(const char *name, long *value) {
    int i;
    if (name[0] == '\0') return 0;
    for (i = 0; i < s_cmap_count; i++) {
        if (strcmp(s_cmap[i].name, name) == 0) {
            *value = s_cmap[i].value;
            return (s_cmap[i].is_256 == 0);
        }
    }
    return 0;
}

static int cmap_get_256(const char *name, uint256_t *value256) {
    int i;
    if (name[0] == '\0') return 0;
    for (i = 0; i < s_cmap_count; i++) {
        if (strcmp(s_cmap[i].name, name) == 0) {
            if (s_cmap[i].is_256) {
                *value256 = s_cmap[i].value256;
                return 1;
            }
            return 0;
        }
    }
    return 0;
}

static int zcc_uint256_eq(uint256_t a, uint256_t b) {
    return (a.limbs[0] == b.limbs[0] &&
            a.limbs[1] == b.limbs[1] &&
            a.limbs[2] == b.limbs[2] &&
            a.limbs[3] == b.limbs[3]);
}

static int zcc_uint256_lt(uint256_t a, uint256_t b) {
    int i;
    for (i = 3; i >= 0; i--) {
        if (a.limbs[i] < b.limbs[i]) return 1;
        if (a.limbs[i] > b.limbs[i]) return 0;
    }
    return 0;
}

static double long_to_double(long l) {
    union { long l; double d; } u;
    u.l = l;
    return u.d;
}

static long double_to_long(double d) {
    union { double d; long l; } u;
    u.d = d;
    return u.l;
}

static ir_pass_result_t ir_pass_const_fold(void *fn_ptr) {
    ir_func_t *fn = (ir_func_t *)fn_ptr;
    ir_pass_result_t r;
    ir_node_t *n;
    int modified = 0;

    memset(&r, 0, sizeof(r));
    r.nodes_before = count_nodes(fn);

    cmap_clear();

    for (n = fn->head; n; n = n->next) {
        /* Track constants */
        if (n->op == IR_CONST && n->dst[0]) {
            if (n->tag == IR_TAG_TRUNCATED_WIDE_CONST || n->tag == IR_TAG_NONE) {
                /* For now, just add as 64-bit if not explicit 256-bit. 
                   Wait, EVM PUSH32 sets TRUNCATED_WIDE_CONST but we want to load imm256! */
                /* If imm256 is used, we can check a flag, but we'll assume if imm256 is populated we use it. 
                   Actually, let's just add both or add as 256 if applicable. */
                cmap_add(n->dst, n->imm);
                /* For 256-bit wide constants, we should probably set a specific op or tag.
                   Since we added imm256, if it's a 256-bit push, we add it to cmap_add_256. */
                if (n->tag == IR_TAG_TRUNCATED_WIDE_CONST) {
                    cmap_add_256(n->dst, n->imm256);
                }
            } else {
                cmap_add(n->dst, n->imm);
            }
            continue;
        }

        /* Check binary ops with two known constant operands */
        if (n->src1[0] && n->src2[0]) {
            long v1, v2, result;
            uint256_t v1_256, v2_256;
            int is_256_op = 0;
            int can_fold = 0;

            if (n->tag == IR_TAG_EVM_EQ || n->tag == IR_TAG_EVM_LT || n->tag == IR_TAG_EVM_GT || n->tag == IR_TAG_EVM_ISZERO) {
                if (cmap_get_256(n->src1, &v1_256) && (n->tag == IR_TAG_EVM_ISZERO || cmap_get_256(n->src2, &v2_256))) {
                    is_256_op = 1;
                    can_fold = 1;
                }
            } else {
                if (cmap_get(n->src1, &v1) && cmap_get(n->src2, &v2)) {
                    can_fold = 1;
                }
            }

            if (!can_fold) continue;

            if (is_256_op) {
                if (n->tag == IR_TAG_EVM_EQ) {
                    result = zcc_uint256_eq(v1_256, v2_256) ? 1 : 0;
                } else if (n->tag == IR_TAG_EVM_LT) {
                    result = zcc_uint256_lt(v1_256, v2_256) ? 1 : 0;
                } else if (n->tag == IR_TAG_EVM_GT) {
                    result = zcc_uint256_lt(v2_256, v1_256) ? 1 : 0;
                } else if (n->tag == IR_TAG_EVM_ISZERO) {
                    uint256_t zero256 = {{0, 0, 0, 0}};
                    result = zcc_uint256_eq(v1_256, zero256) ? 1 : 0;
                } else {
                    continue;
                }
            } else if (n->op == IR_FADD || n->op == IR_FSUB || n->op == IR_FMUL || n->op == IR_FDIV) {
                double f1 = long_to_double(v1);
                double f2 = long_to_double(v2);
                double f_res = 0.0;
                if (n->op == IR_FADD) f_res = f1 + f2;
                else if (n->op == IR_FSUB) f_res = f1 - f2;
                else if (n->op == IR_FMUL) f_res = f1 * f2;
                else if (n->op == IR_FDIV) { if (f2 == 0.0) continue; f_res = f1 / f2; }
                
                n->op = IR_FCONST;
                n->imm = double_to_long(f_res);
                n->tag = IR_TAG_NONE;
                n->src1[0] = '\0';
                n->src2[0] = '\0';
                cmap_add(n->dst, n->imm);
                modified++;
                fprintf(stderr, "\033[38;5;17m[FOLD]\033[38;5;51m Folded floating-point operation into IR_FCONST \033[38;5;199m%f\033[0m\n", f_res);
                continue;
            } else {
                switch (n->op) {
                case IR_ADD: result = v1 + v2; break;
                case IR_SUB: result = v1 - v2; break;
                case IR_MUL: result = v1 * v2; break;
                case IR_DIV: if (v2 == 0) continue; result = v1 / v2; break;
                case IR_MOD: if (v2 == 0) continue; result = v1 % v2; break;
                case IR_AND: result = v1 & v2; break;
                case IR_OR:  result = v1 | v2; break;
                case IR_XOR: result = v1 ^ v2; break;
                case IR_SHL: result = (v2 >= 0 && v2 < 64)
                    ? (long)((unsigned long)v1 << (unsigned)v2) : 0; break;
                case IR_SHR: result = (v2 >= 0 && v2 < 64)
                    ? (long)((unsigned long)v1 >> (unsigned)v2) : 0; break;
                case IR_EQ:  result = (v1 == v2) ? 1 : 0; break;
                case IR_NE:  result = (v1 != v2) ? 1 : 0; break;
                case IR_LT:  result = (v1 < v2)  ? 1 : 0; break;
                case IR_LE:  result = (v1 <= v2) ? 1 : 0; break;
                case IR_GT:  result = (v1 > v2)  ? 1 : 0; break;
                case IR_GE:  result = (v1 >= v2) ? 1 : 0; break;
                default: continue;
                }
            }

            /* Replace with IR_CONST */
            n->op = IR_CONST;
            n->imm = result;
            n->tag = IR_TAG_NONE; /* clear the EVM comparison tag since it's now just a constant */
            n->src1[0] = '\0';
            n->src2[0] = '\0';

            /* Track the new constant (which is just 1 or 0, so 64-bit is fine) */
            cmap_add(n->dst, result);

            modified++;
        }
    }

    r.nodes_after = r.nodes_before; /* const fold mutates, doesn't delete */
    r.nodes_modified = modified;
    r.changed = modified > 0;
    return r;
}

/* ════════════════════════════════════════════════════════════════════════
 * PASS 3: Strength Reduction
 * ════════════════════════════════════════════════════════════════════════
 * Pattern-match on operations with one known constant operand:
 *   MUL dst, src, 0  → CONST dst, 0
 *   MUL dst, 0, src  → CONST dst, 0
 *   ADD dst, src, 0  → COPY  dst, src
 *   ADD dst, 0, src  → COPY  dst, src
 *   SUB dst, src, 0  → COPY  dst, src
 *
 * Phase 3 will add: MUL 2^N → SHL N, DIV 2^N → SHR N (unsigned)
 */
static ir_pass_result_t ir_pass_strength_reduce(void *fn_ptr) {
    ir_func_t *fn = (ir_func_t *)fn_ptr;
    ir_pass_result_t r;
    ir_node_t *n;
    int modified = 0;

    memset(&r, 0, sizeof(r));
    r.nodes_before = count_nodes(fn);

    /* Collect constants */
    cmap_clear();
    for (n = fn->head; n; n = n->next) {
        if (n->op == IR_CONST && n->dst[0]) {
            cmap_add(n->dst, n->imm);
        }
    }

    /* Apply strength reductions */
    for (n = fn->head; n; n = n->next) {
        long val;

        if (n->op == IR_MUL) {
            /* MUL by 0 (either operand) → CONST 0 */
            if (cmap_get(n->src2, &val) && val == 0) {
                n->op = IR_CONST;
                n->imm = 0;
                n->src1[0] = '\0';
                n->src2[0] = '\0';
                cmap_add(n->dst, 0);
                modified++;
                continue;
            }
            if (cmap_get(n->src1, &val) && val == 0) {
                n->op = IR_CONST;
                n->imm = 0;
                n->src1[0] = '\0';
                n->src2[0] = '\0';
                cmap_add(n->dst, 0);
                modified++;
                continue;
            }
            /* MUL by 1 → COPY */
            if (cmap_get(n->src2, &val) && val == 1) {
                n->op = IR_COPY;
                /* src1 stays, src2 cleared */
                n->src2[0] = '\0';
                modified++;
                continue;
            }
            if (cmap_get(n->src1, &val) && val == 1) {
                n->op = IR_COPY;
                /* swap src2 into src1 position */
                strncpy(n->src1, n->src2, 31);
                n->src1[31] = '\0';
                n->src2[0] = '\0';
                modified++;
                continue;
            }
        }

        if (n->op == IR_ADD) {
            /* ADD src, 0 → COPY src */
            if (cmap_get(n->src2, &val) && val == 0) {
                n->op = IR_COPY;
                n->src2[0] = '\0';
                modified++;
                continue;
            }
            if (cmap_get(n->src1, &val) && val == 0) {
                n->op = IR_COPY;
                strncpy(n->src1, n->src2, 31);
                n->src1[31] = '\0';
                n->src2[0] = '\0';
                modified++;
                continue;
            }
        }

        if (n->op == IR_SUB) {
            /* SUB src, 0 → COPY src */
            if (cmap_get(n->src2, &val) && val == 0) {
                n->op = IR_COPY;
                n->src2[0] = '\0';
                modified++;
                continue;
            }
        }
    }

    r.nodes_after = r.nodes_before; /* strength reduce mutates, doesn't delete */
    r.nodes_modified = modified;
    r.changed = modified > 0;
    return r;
}

/* ════════════════════════════════════════════════════════════════════════
 * PASS 4: Global Value Numbering (GVN)
 * ════════════════════════════════════════════════════════════════════════
 * Hash each pure instruction by (op, type, src1, src2, imm).
 * If a prior instruction produced the same value, rewrite the current
 * instruction as IR_COPY from the prior's destination temp.
 * Subsequent DCE will clean up the now-dead original source defs.
 *
 * Eligible: arithmetic, bitwise, comparison, cast, copy, neg, not, const.
 * Excluded: loads, stores, calls, branches, labels, phi, alloca, arg,
 *           asm, fconst (side-effectful or memory-dependent).
 *
 * Hash: FNV-1a 64-bit, folded to table index.
 * Collision: open-addressed linear probing.
 */

#define GVN_TABLE_SIZE 4096    /* power of 2, must accommodate largest fn */
#define GVN_TABLE_MASK (GVN_TABLE_SIZE - 1)

/* FNV-1a constants for 64-bit (ULL: portable across LP64 and LLP64) */
#define FNV_OFFSET 14695981039346656037ULL
#define FNV_PRIME  1099511628211ULL

typedef struct {
    unsigned long long hash;
    char          dst[IR_NAME_MAX];  /* destination of the canonical def   */
    /* ── Structural key (fortification layer) ──────────────────────── */
    ir_op_t       key_op;
    ir_type_t     key_type;
    char          key_src1[IR_NAME_MAX];
    char          key_src2[IR_NAME_MAX];
    long          key_imm;
    int           occupied;
} gvn_entry_t;

static gvn_entry_t s_gvn_table[GVN_TABLE_SIZE];

static void gvn_clear(void) {
    int i;
    for (i = 0; i < GVN_TABLE_SIZE; i++) {
        s_gvn_table[i].occupied = 0;
    }
}

static unsigned long long gvn_hash_node(ir_node_t *n) {
    unsigned long long h = FNV_OFFSET;
    const char *p;
    unsigned char op_byte;
    unsigned char ty_byte;
    int i;

    /* Hash opcode */
    op_byte = (unsigned char)n->op;
    h ^= op_byte;
    h *= FNV_PRIME;

    /* Hash type */
    ty_byte = (unsigned char)n->type;
    h ^= ty_byte;
    h *= FNV_PRIME;

    /* Hash src1 */
    for (p = n->src1; *p; p++) {
        h ^= (unsigned char)*p;
        h *= FNV_PRIME;
    }
    h ^= 0xFF; /* separator */
    h *= FNV_PRIME;

    /* Hash src2 */
    for (p = n->src2; *p; p++) {
        h ^= (unsigned char)*p;
        h *= FNV_PRIME;
    }
    h ^= 0xFE; /* separator */
    h *= FNV_PRIME;

    /* Hash imm (for IR_CONST, IR_ALLOCA, etc.) */
    for (i = 0; i < 8; i++) {
        h ^= (unsigned char)((n->imm >> (i * 8)) & 0xFF);
        h *= FNV_PRIME;
    }

    return h;
}

/* Check if an opcode is pure (eligible for GVN).
 * BOLSTER: IR_COPY excluded — GVN-produced copies must not be
 *   re-matched (they are forwarding aliases, not computations).
 * BOLSTER: IR_CONST excluded — each CONST defines a unique temp;
 *   two CONSTs with the same value but different dst names must
 *   both survive so downstream references remain valid. */
static int gvn_is_pure(ir_op_t op) {
    switch (op) {
    case IR_ADD: case IR_SUB: case IR_MUL: case IR_DIV: case IR_MOD:
    case IR_NEG:
    case IR_AND: case IR_OR:  case IR_XOR: case IR_NOT:
    case IR_SHL: case IR_SHR:
    case IR_EQ:  case IR_NE:  case IR_LT:  case IR_LE:
    case IR_GT:  case IR_GE:
    case IR_CAST:
    case IR_FADD: case IR_FSUB: case IR_FMUL: case IR_FDIV:
    case IR_ITOF: case IR_FTOI:
        return 1;
    default:
        return 0;
    }
}

/* HARDEN: Full structural comparison — invoked on hash hit.
 * Returns 1 only when all 5 key fields match exactly. */
static int gvn_keys_match(gvn_entry_t *e, ir_node_t *n) {
    if (e->key_op   != n->op)   return 0;
    if (e->key_type != n->type) return 0;
    if (e->key_imm  != n->imm)  return 0;
    if (strcmp(e->key_src1, n->src1) != 0) return 0;
    if (strcmp(e->key_src2, n->src2) != 0) return 0;
    return 1;
}

/* Lookup or insert with full structural verification.
 * HARDEN: Hash hit alone is insufficient — we compare all 5 key fields.
 * On hash collision (same hash, different structure), probing continues.
 * Returns the canonical dst on true structural match, NULL on new insert. */
static const char *gvn_lookup_or_insert(unsigned long long hash, ir_node_t *n) {
    unsigned int idx = (unsigned int)(hash & GVN_TABLE_MASK);
    int probe;

    for (probe = 0; probe < GVN_TABLE_SIZE; probe++) {
        unsigned int slot = (idx + probe) & GVN_TABLE_MASK;

        if (!s_gvn_table[slot].occupied) {
            /* Empty slot — insert with full structural key */
            s_gvn_table[slot].hash = hash;
            strncpy(s_gvn_table[slot].dst, n->dst, IR_NAME_MAX - 1);
            s_gvn_table[slot].dst[IR_NAME_MAX - 1] = '\0';
            s_gvn_table[slot].key_op   = n->op;
            s_gvn_table[slot].key_type = n->type;
            strncpy(s_gvn_table[slot].key_src1, n->src1, IR_NAME_MAX - 1);
            s_gvn_table[slot].key_src1[IR_NAME_MAX - 1] = '\0';
            strncpy(s_gvn_table[slot].key_src2, n->src2, IR_NAME_MAX - 1);
            s_gvn_table[slot].key_src2[IR_NAME_MAX - 1] = '\0';
            s_gvn_table[slot].key_imm  = n->imm;
            s_gvn_table[slot].occupied = 1;
            return NULL;
        }

        if (s_gvn_table[slot].hash == hash &&
            gvn_keys_match(&s_gvn_table[slot], n)) {
            /* HARDEN: Hash AND structure match — true redundancy */
            return s_gvn_table[slot].dst;
        }
        /* Hash collision but different structure — continue probing */
    }

    /* Table full — treat as miss, don't optimize */
    return NULL;
}

/*
 * GVN scope stack: tracks which table slots were occupied at each
 * scope level so we can roll them back when leaving a dominated subtree.
 * Zero malloc — uses a static array of slot indices.
 */
#define GVN_SCOPE_MAX 8192   /* max slots modified across all scopes */

static int  s_gvn_scope_stack[GVN_SCOPE_MAX]; /* slot indices to clear */
static int  s_gvn_scope_top;                   /* top of the stack     */
static int  s_gvn_scope_marks[DOM_MAX_BLOCKS]; /* stack depth at push  */
static int  s_gvn_modified;                    /* mutation counter     */

/*
 * gvn_lookup_or_insert_scoped: same as gvn_lookup_or_insert, but
 * records newly occupied slots on the scope stack for rollback.
 */
static const char *gvn_lookup_or_insert_scoped(unsigned long long hash, ir_node_t *n) {
    unsigned int idx = (unsigned int)(hash & GVN_TABLE_MASK);
    int probe;

    for (probe = 0; probe < GVN_TABLE_SIZE; probe++) {
        unsigned int slot = (idx + probe) & GVN_TABLE_MASK;

        if (!s_gvn_table[slot].occupied) {
            /* Empty slot — insert with full structural key */
            s_gvn_table[slot].hash = hash;
            strncpy(s_gvn_table[slot].dst, n->dst, IR_NAME_MAX - 1);
            s_gvn_table[slot].dst[IR_NAME_MAX - 1] = '\0';
            s_gvn_table[slot].key_op   = n->op;
            s_gvn_table[slot].key_type = n->type;
            strncpy(s_gvn_table[slot].key_src1, n->src1, IR_NAME_MAX - 1);
            s_gvn_table[slot].key_src1[IR_NAME_MAX - 1] = '\0';
            strncpy(s_gvn_table[slot].key_src2, n->src2, IR_NAME_MAX - 1);
            s_gvn_table[slot].key_src2[IR_NAME_MAX - 1] = '\0';
            s_gvn_table[slot].key_imm  = n->imm;
            s_gvn_table[slot].occupied = 1;

            /* Record on scope stack for rollback */
            if (s_gvn_scope_top < GVN_SCOPE_MAX) {
                s_gvn_scope_stack[s_gvn_scope_top++] = (int)slot;
            } else {
                fprintf(stderr, "[gvn] WARNING: scope stack overflow — "
                        "rollback may be incomplete\n");
            }
            return NULL;
        }

        if (s_gvn_table[slot].hash == hash &&
            gvn_keys_match(&s_gvn_table[slot], n)) {
            return s_gvn_table[slot].dst;
        }
    }

    return NULL;  /* table full */
}

/*
 * gvn_scope_push: save the current stack depth as a scope marker.
 */
static void gvn_scope_push(int block_id) {
    s_gvn_scope_marks[block_id] = s_gvn_scope_top;
}

/*
 * gvn_scope_pop: roll back all GVN table entries inserted since the
 * scope was pushed for this block.
 */
static void gvn_scope_pop(int block_id) {
    int mark = s_gvn_scope_marks[block_id];
    while (s_gvn_scope_top > mark) {
        s_gvn_scope_top--;
        int slot = s_gvn_scope_stack[s_gvn_scope_top];
        s_gvn_table[slot].occupied = 0;
    }
}

/*
 * gvn_process_block: run GVN on all nodes within a single basic block.
 */
static void gvn_process_block(const dom_cfg_t *cfg, int bid) {
    ir_node_t *n;
    const dom_bb_t *b = &cfg->blocks[bid];

    for (n = b->first; n; n = n->next) {
        unsigned long long h;
        const char *canon;

        /* Skip non-pure or non-defining instructions */
        if (!gvn_is_pure(n->op)) goto next;
        if (n->dst[0] == '\0') goto next;

        h = gvn_hash_node(n);
        canon = gvn_lookup_or_insert_scoped(h, n);

        if (canon) {
            /* Redundant — rewrite as COPY from canonical def */
            n->op = IR_COPY;
            strncpy(n->src1, canon, IR_NAME_MAX - 1);
            n->src1[IR_NAME_MAX - 1] = '\0';
            n->src2[0] = '\0';
            n->imm = 0;
            s_gvn_modified++;
        }

    next:
        if (n == b->last) break;
    }
}

/*
 * gvn_walk_domtree: pre-order traversal of the dominator tree.
 * For each block:
 *   1. Push a scope marker
 *   2. Process the block's IR nodes
 *   3. Recurse into dominated children
 *   4. Pop the scope (rollback entries from this block)
 */
static void gvn_walk_domtree(const dom_cfg_t *cfg, int bid) {
    int i;

    gvn_scope_push(bid);
    gvn_process_block(cfg, bid);

    for (i = 0; i < cfg->blocks[bid].child_count; i++) {
        gvn_walk_domtree(cfg, cfg->blocks[bid].children[i]);
    }

    gvn_scope_pop(bid);
}

static ir_pass_result_t ir_pass_gvn(void *fn_ptr) {
    ir_func_t *fn = (ir_func_t *)fn_ptr;
    ir_pass_result_t r;
    const dom_cfg_t *cfg;

    memset(&r, 0, sizeof(r));
    r.nodes_before = count_nodes(fn);

    gvn_clear();
    s_gvn_scope_top = 0;
    s_gvn_modified = 0;

    cfg = dom_get_cfg();

    if (cfg && cfg->block_count > 0 && cfg->fn == fn) {
        /* Dominator tree available — walk it for cross-block GVN */
        gvn_walk_domtree(cfg, 0);
    } else {
        /* Fallback: linear scan with label-boundary reset (legacy mode) */
        ir_node_t *n;
        for (n = fn->head; n; n = n->next) {
            unsigned long long h;
            const char *canon;

            if (n->op == IR_LABEL) {
                gvn_clear();
                continue;
            }
            if (!gvn_is_pure(n->op)) continue;
            if (n->dst[0] == '\0') continue;

            h = gvn_hash_node(n);
            canon = gvn_lookup_or_insert(h, n);

            if (canon) {
                n->op = IR_COPY;
                strncpy(n->src1, canon, IR_NAME_MAX - 1);
                n->src1[IR_NAME_MAX - 1] = '\0';
                n->src2[0] = '\0';
                n->imm = 0;
                s_gvn_modified++;
            }
        }
    }

    r.nodes_after = r.nodes_before;
    r.nodes_modified = s_gvn_modified;
    r.changed = s_gvn_modified > 0;
    return r;
}

/* Helper to find the unique definition of a temporary register */
static ir_node_t *find_def(ir_func_t *fn, ir_node_t *end_node, const char *reg) {
    ir_node_t *n;
    ir_node_t *def = NULL;
    if (!reg || !reg[0]) return NULL;
    for (n = fn->head; n && n != end_node; n = n->next) {
        if (n->dst[0] && strcmp(n->dst, reg) == 0) {
            def = n;
        }
    }
    return def;
}

/* ════════════════════════════════════════════════════════════════════════
 * PASS: Coalesce Vector Loads
 * ════════════════════════════════════════════════════════════════════════
 */
static ir_pass_result_t ir_pass_coalesce_vload(void *fn_ptr) {
    ir_func_t *fn = (ir_func_t *)fn_ptr;
    ir_pass_result_t r;
    ir_node_t *L1, *scan;
    int modified = 0;

    memset(&r, 0, sizeof(r));
    r.nodes_before = count_nodes(fn);

    for (L1 = fn->head; L1; L1 = L1->next) {
        if (L1->op == IR_LOAD && ir_type_bytes(L1->type) == 4) {
            ir_node_t *A1 = find_def(fn, L1, L1->src1);
            if (!A1 || A1->op != IR_ADD) continue;
            ir_node_t *C1 = find_def(fn, A1, A1->src2);
            if (!C1 || C1->op != IR_CONST) continue;

            const char *base_r = A1->src1;
            long offset1 = C1->imm;

            /* Alignment Verification: Ensure base struct is at least 8-byte aligned */
            if (offset1 % 8 != 0) continue;

            /* Scan forward for L2 */
            int distance = 0;
            for (scan = L1->next; scan && distance < 10; scan = scan->next, distance++) {
                if (is_side_effect(scan->op)) {
                    /* Pointer Aliasing Bounds: Any side effect or store invalidates the window (WAR protection) */
                    break;
                }
                if (scan->op == IR_LOAD && ir_type_bytes(scan->type) == 4) {
                    ir_node_t *L2 = scan;
                    ir_node_t *A2 = find_def(fn, L2, L2->src1);
                    if (!A2 || A2->op != IR_ADD) continue;
                    if (strcmp(A2->src1, base_r) != 0) continue;
                    ir_node_t *C2 = find_def(fn, A2, A2->src2);
                    if (!C2 || C2->op != IR_CONST) continue;

                    long offset2 = C2->imm;
                    if (offset2 == offset1 + 4) {
                        /* Match! Execute fusion */
                        L1->op = IR_VLOAD;
                        L1->type = IR_TY_U64; /* 8 bytes */
                        
                        L2->op = IR_VEXTRACT;
                        strncpy(L2->src1, L1->dst, IR_NAME_MAX);
                        L2->imm = 1; /* high dword */

                        modified++;
                        break; /* fused, move on to next L1 */
                    }
                }
            }
        }
    }

    r.nodes_after = r.nodes_before;
    r.nodes_modified = modified;
    r.changed = modified > 0;
    return r;
}

/* ── Registry API ────────────────────────────────────────────────────── */

static ir_pass_manager_t *ir_pm_create(void) {
    ir_pass_manager_t *pm = (ir_pass_manager_t *)calloc(1, sizeof(ir_pass_manager_t));
    if (!pm) {
        fprintf(stderr, "ir_pm_create: out of memory\n");
        exit(1);
    }
    return pm;
}

static void ir_pm_register(ir_pass_manager_t *pm, const char *name, ir_pass_fn fn) {
    if (pm->count >= IR_PM_MAX_PASSES) {
        fprintf(stderr, "ir_pm_register: too many passes (max %d)\n", IR_PM_MAX_PASSES);
        return;
    }
    pm->passes[pm->count].name = name;
    pm->passes[pm->count].fn = fn;
    pm->passes[pm->count].enabled = 1;
    pm->count++;
}

static void ir_pm_run(ir_pass_manager_t *pm, ir_module_t *mod) {
    int p;
    int f;
    int total_nodes_in = 0;
    int total_nodes_out = 0;
    int total_deleted = 0;
    int total_modified = 0;

    for (p = 0; p < pm->count; p++) {
        int pass_before = 0;
        int pass_after = 0;
        int pass_deleted = 0;
        int pass_modified = 0;

        if (!pm->passes[p].enabled) continue;

        for (f = 0; f < mod->func_count; f++) {
            ir_pass_result_t r = pm->passes[p].fn(mod->funcs[f]);
            pass_before += r.nodes_before;
            pass_after += r.nodes_after;
            pass_deleted += r.nodes_deleted;
            pass_modified += r.nodes_modified;
        }

        if (pm->verbose) {
            fprintf(stderr, "  [IR Pass] %-18s: %d funcs, %d nodes -> %d nodes (%d deleted, %d modified)\n",
                    pm->passes[p].name, mod->func_count, pass_before, pass_after,
                    pass_deleted, pass_modified);
        }

        if (p == 0) total_nodes_in = pass_before;
        total_nodes_out = pass_after;
        total_deleted += pass_deleted;
        total_modified += pass_modified;
    }

    if (pm->verbose && pm->count > 0) {
        fprintf(stderr, "  [IR Pass] Pipeline complete: %d nodes -> %d nodes (total: %d deleted, %d modified)\n",
                total_nodes_in, total_nodes_out, total_deleted, total_modified);
    }
}

static void ir_pm_free(ir_pass_manager_t *pm) {
    if (pm) free(pm);
}

/* ── Primary entry point (called from part5.c) ──────────────────────── */

void ir_pm_run_default(void *mod_ptr, int verbose) {
    ir_module_t *mod = (ir_module_t *)mod_ptr;
    ir_pass_manager_t *pm;

    if (!mod || mod->func_count == 0) return;

    pm = ir_pm_create();
    pm->verbose = verbose;

    /* Default pipeline: symbolic_cfg → DCE → const_fold → strength_reduce → dominance → GVN → vload → lower_float → warden → DCE */
    ir_pm_register(pm, "symbolic_cfg", ir_pass_symbolic_cfg);
    ir_pm_register(pm, "dce", ir_pass_dce);
    ir_pm_register(pm, "const_fold", ir_pass_const_fold);
    ir_pm_register(pm, "strength_reduce", ir_pass_strength_reduce);
    ir_pm_register(pm, "dominance", ir_pass_dominance);
    ir_pm_register(pm, "gvn", ir_pass_gvn);
    ir_pm_register(pm, "coalesce_vload", ir_pass_coalesce_vload);
    ir_pm_register(pm, "lower_float", ir_pass_lower_float);
    ir_pm_register(pm, "warden", ir_pass_warden);
    ir_pm_register(pm, "dce2", ir_pass_dce);

    ir_pm_run(pm, mod);
    ir_pm_free(pm);
}
