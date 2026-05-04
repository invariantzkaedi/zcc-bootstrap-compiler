/**
 * compiler_passes.c — ZKAEDI PRIME Compiler Optimization Suite
 *
 * Five production-grade IR transformation passes:
 *
 *   PASS 1: DCE → SSA Bridge  — φ-function reachability as liveness oracle
 *   PASS 2: Escape Analysis   — pointer provenance → heap-to-stack demotion
 *   PASS 3: LICM              — alias-aware loop-invariant code motion
 *                               (hardened: OP_ALLOCA must-not-alias model,
 *                                single preheader for multi-entry loops)
 *   PASS 4: PGO BB Reordering — branch probability → icache-optimal layout
 *
 * Each pass is self-contained. All passes are chained via run_all_passes().
 *
 * Build:
 *   Standalone (no zcc):  cc -O2 -std=c17 -Wall -Wextra -DZCC_BRIDGE_STANDALONE
 * compiler_passes.c -o passes -lm With zcc (Node copy): cc -O2 -std=c17 -Wall
 * -Wextra zcc.c compiler_passes.c -o zcc_full -lm
 */

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Forward-declare struct Node (defined in zcc.c) so all uses of
 * 'struct Node *' in this TU share the same incomplete-type tag.
 * Without this, GCC 13 treats the tag as re-declared in each parameter
 * list and reports spurious "conflicting types" on the static forward
 * declarations of zcc_node_from_expr / zcc_node_from_stmt.           */
struct Node;

#include "zcc_ast_bridge.h"

/* When set (ZCC_PGO_DEBUG_MAIN=1 and emitting main), OP_LOAD in
 * ir_asm_lower_insn logs block/dst/src0/slot to stderr for crash triage. */
static int s_debug_main_emit = 0;

/* ─────────────────────────────────────────────────────────────────────────────
 * SHARED IR PRIMITIVES
 * ───────────────────────────────────────────────────────────────────────────
 */

#define MAX_OPERANDS 4
#define MAX_CALL_ARGS 16
#define MAX_PHI_SOURCES 32
#define MAX_SUCCS 2048
#define MAX_PREDS 2048
#define MAX_INSTRS 65536 /* must exceed max RegID in any compiled function */
#define MAX_BLOCKS                                                             \
  8192 /* per-function block limit (parser/lexer can be large) */
#define MAX_ALLOCS 256
#define MAX_LOOPS 64
#define MAX_LOOP_BLOCKS 256
#define NAME_LEN 64

#define NO_BLOCK 0xFFFFFFFFu /* sentinel: absent block ID    */
#define NO_ALLOC 0xFFFFFFFFu /* sentinel: absent alloca ID   */

/* ── CG-IR-015: IR value type for width-sensitive instruction lowering ────────
 * Propagated from the AST (via member_size encoding) through zcc_lower_expr
 * into Instr.ir_type.  Zero-value IR_TY_I64 is the safe 64-bit default so
 * all existing calloc'd Instrs remain correct without explicit initialisation.
 * Only I32/U32 need special treatment in ir_asm_lower_insn (div/mod/shr).   */
typedef enum {
    IR_TY_I64 = 0, /* signed 64-bit  — long, pointer; also "unknown" default  */
    IR_TY_I32 = 1, /* signed 32-bit  — int                                    */
    IR_TY_U32 = 2, /* unsigned 32-bit — unsigned int                          */
    IR_TY_U64 = 3, /* unsigned 64-bit — unsigned long                         */
} IRType;

typedef uint32_t RegID;   /* virtual register identifier            */
typedef uint32_t BlockID; /* basic block identifier                 */
typedef uint32_t InstrID; /* instruction identifier within a block  */

typedef enum {
  OP_NOP = 0,
  OP_CONST, /* immediate constant (Phase B lowering)           */
  OP_PHI,   /* φ(r₀:B₀, r₁:B₁, …)                */
  OP_ADD,
  OP_SUB,
  OP_MUL,
  OP_DIV,
  OP_MOD, /* TODO: x86 idiv sequence when lowering to asm    */
  OP_BAND,
  OP_BOR,
  OP_BXOR,
  OP_BNOT,
  OP_SHL,
  OP_SHR,
  OP_LT,
  OP_EQ,
  OP_NE,
  OP_GT,
  OP_GE,
  OP_LE, /* comparisons */
  OP_LOAD,
  OP_STORE,
  OP_ALLOCA, /* stack allocation — escape candidate  */
  OP_GEP,    /* GetElementPtr — tracks escape        */
  OP_CALL,   /* may cause escape                     */
  OP_RET,
  OP_BR,     /* unconditional branch                 */
  OP_CONDBR, /* conditional branch                   */
  OP_COPY,
  OP_UNDEF,
  OP_PGO_COUNTER_ADDR, /* PGO instrumentation: dst = &__zcc_edge_counts[imm] */
  OP_GLOBAL,           /* load address of global symbol: lea name(%rip), %reg */
  OP_ASM,              /* inline assembly string */
  OP_VLA_ALLOC,
} Opcode;

static const char *opcode_name[] __attribute__((unused)) = {"nop",
                                                            "const",
                                                            "phi",
                                                            "add",
                                                            "sub",
                                                            "mul",
                                                            "div",
                                                            "mod",
                                                            "band",
                                                            "bor",
                                                            "bxor",
                                                            "bnot",
                                                            "shl",
                                                            "shr",
                                                            "lt",
                                                            "eq",
                                                            "ne",
                                                            "gt",
                                                            "ge",
                                                            "le",
                                                            "load",
                                                            "store",
                                                            "alloca",
                                                            "gep",
                                                            "call",
                                                            "ret",
                                                            "br",
                                                            "condbr",
                                                            "copy",
                                                            "undef",
                                                            "pgo_counter_addr",
                                                            "global",
                                                            "asm"};

typedef struct {
  RegID reg;     /* source register                    */
  BlockID block; /* predecessor block the value comes from */
} PhiSource;

typedef struct Instr {
  InstrID id;
  Opcode op;
  RegID dst; /* destination register (0 = no def)   */
  RegID src[MAX_OPERANDS];
  uint32_t n_src;

  /* φ-node sources (populated only when op == OP_PHI) */
  PhiSource phi[MAX_PHI_SOURCES];
  uint32_t n_phi;

  /* Immediate (OP_CONST, or e.g. alloca size; 0 if unused) */
  int64_t imm;

  /* OP_CALL: callee name and argument registers */
  char call_name[128];
  RegID call_args[MAX_CALL_ARGS];
  uint32_t n_call_args;

  /* Metadata */
  bool dead;        /* marked by DCE                       */
  bool escape;      /* marked by escape analysis           */
  double exec_freq; /* from PGO profile                    */
  int line_no;      /* source line for DWARF .loc (0 = none) */
  IRType ir_type;   /* CG-IR-015: value type for width-sensitive lowering     */
                    /* (0 = IR_TY_I64 default; set for OP_DIV/MOD/SHR/SHL)   */
  int lscan_seq;    /* Liveness sequence ID injected by ir_asm_number_and_liveness */

  char *asm_string;

  struct Instr *next;
  struct Instr *prev;
} Instr;

typedef struct Block {
  BlockID id;
  char name[NAME_LEN];

  /* Instruction doubly-linked list */
  Instr *head;
  Instr *tail;
  uint32_t n_instrs;

  /* CFG edges */
  BlockID succs[MAX_SUCCS];
  uint32_t n_succs;
  BlockID preds[MAX_PREDS];
  uint32_t n_preds;

  /* Branch probabilities (succs[i] → branch_prob[i]) */
  double branch_prob[MAX_SUCCS];

  /* Liveness (DCE / SSA pass) */
  uint64_t live_in[8]; /* bitset — up to 512 regs                */
  uint64_t live_out[8];

  /* PGO / reordering metadata */
  double exec_freq;   /* estimated execution frequency          */
  bool placed;        /* used during chain construction         */
  BlockID chain_next; /* next block in the PGO chain            */

  bool reachable; /* set by CFG reachability pass           */
} Block;

typedef struct Function {
  Block *blocks[MAX_BLOCKS];
  uint32_t n_blocks;
  BlockID entry;
  BlockID exit;

  /* Def-use chains (reg → defining instruction) */
  Instr *def_of[MAX_INSTRS];     /* indexed by RegID */
  BlockID def_block[MAX_INSTRS]; /* block housing def_of[r]; NO_BLOCK if arg */
  uint32_t n_regs;

  /* Pass statistics */
  struct {
    uint32_t dce_instrs_removed;
    uint32_t dce_blocks_removed;
    uint32_t ea_promotions;
    uint32_t licm_hoisted;
    uint32_t licm_preheaders_inserted;
    uint32_t pgo_blocks_reordered;
  } stats;
} Function;

/* ─────────────────────────────────────────────────────────────────────────────
 * BITSET HELPERS  (512 registers → 8 × 64-bit words)
 * ───────────────────────────────────────────────────────────────────────────
 */

static inline void bs_set(uint64_t *bs, RegID r) {
  bs[r >> 6] |= (1ULL << (r & 63));
}
static inline void bs_clear(uint64_t *bs, RegID r) {
  bs[r >> 6] &= ~(1ULL << (r & 63));
}
static inline bool bs_get(uint64_t *bs, RegID r) {
  return !!(bs[r >> 6] & (1ULL << (r & 63)));
}
static inline bool bs_empty(uint64_t *bs) {
  for (int i = 0; i < 8; i++)
    if (bs[i])
      return false;
  return true;
}
static inline bool bs_union(uint64_t *dst, const uint64_t *src) {
  bool changed = false;
  for (int i = 0; i < 8; i++) {
    uint64_t before = dst[i];
    dst[i] |= src[i];
    changed |= (dst[i] != before);
  }
  return changed;
}
static inline void bs_copy(uint64_t *dst, const uint64_t *src) {
  memcpy(dst, src, 8 * sizeof(uint64_t));
}

/* ─────────────────────────────────────────────────────────────────────────────
 * PASS 1: DCE → SSA BRIDGE PASS
 *
 * Theory
 * ──────
 * In SSA form every variable is defined exactly once. A definition is dead iff
 * it cannot influence any program output — i.e., no chain of use-def edges from
 * that definition reaches a *critical* instruction (store, call, return).
 *
 * Rather than computing full dataflow fixpoints, we exploit SSA structure:
 *
 *   1. Mark all registers used by critical instructions as LIVE (seed
 * worklist).
 *   2. For each live register r, mark the instruction that defines r as live;
 *      then mark all of its operands live → propagate backward.
 *   3. For φ-nodes: a φ at block B with source (rₖ: Bₖ) is live iff the
 *      φ's destination is live AND the CFG edge Bₖ→B is reachable.
 *      This is the SSA-specific refinement: we prune φ sources that come from
 *      unreachable predecessors without needing separate reachability analysis.
 *   4. Any instruction whose dst ∉ live_set at the end of propagation is dead.
 *      Any block with no live instructions and no reachable successors is dead.
 *
 * Complexity: O(N) where N = total number of SSA values — one pass per value
 * in the worklist. No iterative dataflow convergence required.
 * ───────────────────────────────────────────────────────────────────────────
 */

/* Worklist for backward liveness propagation */
typedef struct {
  RegID items[MAX_INSTRS];
  int head, tail, size;
} RegWorklist;

static void wl_push(RegWorklist *wl, RegID r) {
  assert(wl->size < MAX_INSTRS);
  wl->items[wl->tail] = r;
  wl->tail = (wl->tail + 1) % MAX_INSTRS;
  wl->size++;
}
static RegID wl_pop(RegWorklist *wl) {
  assert(wl->size > 0);
  RegID r = wl->items[wl->head];
  wl->head = (wl->head + 1) % MAX_INSTRS;
  wl->size--;
  return r;
}

/* Determine if an instruction is a critical side-effect anchor */
static bool is_critical(const Instr *ins) {
  return ins->op == OP_STORE || ins->op == OP_CALL || ins->op == OP_RET ||
         ins->op == OP_BR || ins->op == OP_CONDBR;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * PASS 3: LICM — Loop-Invariant Code Motion  (full implementation)
 *
 * Pipeline:
 *   A. def→block map      populate fn->def_block[] for all live defs
 *   B. RPO via iterative DFS   needed for Cooper dominator algorithm
 *   C. Iterative dominator tree  Cooper et al. 2001, O(N) in practice
 *   D. Back-edge detection  succ dominates pred ⟹ back edge
 *   E. Natural loop body    reverse BFS from latch, bounded by header
 *   F. Preheader insertion  single entry-point from outside the loop
 *   G. Alias map            pointer regs that are OP_STORE targets in loop
 *   H. Hoist loop-invariant instructions to preheader (iterate until stable)
 *
 * Alias model (OP_ALLOCA-based):
 *   A register r is "aliased" if it appears as the pointer operand (src[1])
 *   of any OP_STORE inside the loop.  OP_LOAD through an aliased pointer is
 *   NOT invariant.  Pure arithmetic/const whose operands are all outside the
 *   loop ARE invariant.
 * ───────────────────────────────────────────────────────────────────────────
 */

/* ── Block-membership bitset (MAX_BLOCKS=512 → 8 × 64-bit words) ── */
#define BLKSET_WORDS 8
typedef uint64_t BlkSet[BLKSET_WORDS];

static inline void blkset_add(BlkSet bs, BlockID b) {
  bs[b >> 6] |= (1ULL << (b & 63));
}
static inline bool blkset_has(const BlkSet bs, BlockID b) {
  return !!(bs[b >> 6] & (1ULL << (b & 63)));
}
static inline void blkset_zero(BlkSet bs) {
  memset(bs, 0, BLKSET_WORDS * sizeof(uint64_t));
}
static inline void blkset_merge(BlkSet dst, const BlkSet src) {
  for (int i = 0; i < BLKSET_WORDS; i++)
    dst[i] |= src[i];
}

/* ── RPO state (module-level, single-threaded) ── */
static uint32_t licm_rpo_arr[MAX_BLOCKS]; /* rpo_arr[rpo_idx] = BlockID  */
static uint32_t licm_rpo_of[MAX_BLOCKS];  /* rpo_of[BlockID]  = rpo_idx  */
static uint32_t licm_rpo_n;

/* ── Dominator tree ── */
static BlockID licm_idom[MAX_BLOCKS]; /* licm_idom[b] = imm-dom of b */

/* ── Natural loop descriptor ── */
typedef struct {
  BlockID header;
  BlockID preheader;
  bool has_preheader;
  BlkSet body_bs; /* bitset of block IDs in loop  */
  BlockID latches[MAX_LOOP_BLOCKS];
  uint32_t n_latches;
} LICMLoop;

/* V2 Alias Oracle: base+offset per store so we can allow p->y hoisted past
 * store to p->x */
#define MAX_LOOP_STORES 128
typedef struct {
  RegID base;
  int64_t offset;
  bool is_variable; /* true if offset is unknown/dynamic */
} StoreTarget;
typedef struct {
  StoreTarget targets[MAX_LOOP_STORES];
  uint32_t n_targets;
  bool clobber_all; /* fallback if we exceed capacity or hit an unknown pointer
                     */
} AliasOracle;

/* ═════════════════════════════════════════════════════════════════
 * A.  Build fn->def_block[] and refresh fn->def_of[]
 * ═════════════════════════════════════════════════════════════════ */
static void licm_build_def_block(Function *fn) {
  memset(fn->def_block, 0xFF, sizeof(fn->def_block)); /* NO_BLOCK */
  memset(fn->def_of, 0,
         sizeof(fn->def_of)); /* NULL — cleared to avoid stale freed pointers */
  for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
    Block *blk = fn->blocks[bi];
    if (!blk || !blk->reachable)
      continue;
    for (Instr *ins = blk->head; ins; ins = ins->next) {
      if (ins->dst && ins->dst < MAX_INSTRS) {
        fn->def_block[ins->dst] = bi;
        fn->def_of[ins->dst] = ins;
      }
    }
  }
}

/* ═════════════════════════════════════════════════════════════════
 * B.  Iterative DFS post-order → RPO
 *     Iterative to avoid call-stack overflow on large CFGs.
 * ═════════════════════════════════════════════════════════════════ */
static void licm_compute_rpo(Function *fn) {
  static bool visited[MAX_BLOCKS];
  static uint32_t post[MAX_BLOCKS];
  static BlockID stk[MAX_BLOCKS];
  static bool expanded[MAX_BLOCKS];
  uint32_t post_n = 0;
  int top = -1;

  memset(visited, 0, fn->n_blocks * sizeof(bool));
  memset(licm_rpo_of, 0xFF, sizeof(licm_rpo_of));

  stk[++top] = fn->entry;
  expanded[top] = false;

  while (top >= 0) {
    BlockID b = stk[top];
    if (!expanded[top]) {
      if (visited[b]) {
        top--;
        continue;
      }
      visited[b] = true;
      expanded[top] = true;
      Block *blk = fn->blocks[b];
      /* push successors in reverse so first succ is processed first */
      for (int si = (int)blk->n_succs - 1; si >= 0; si--) {
        BlockID s = blk->succs[si];
        if (s < fn->n_blocks && !visited[s] && top + 1 < MAX_BLOCKS) {
          stk[++top] = s;
          expanded[top] = false;
        }
      }
    } else {
      post[post_n++] = b;
      top--;
    }
  }

  licm_rpo_n = post_n;
  for (uint32_t i = 0; i < licm_rpo_n; i++) {
    licm_rpo_arr[i] = post[licm_rpo_n - 1 - i];
    licm_rpo_of[post[licm_rpo_n - 1 - i]] = i;
  }
}

/* ═════════════════════════════════════════════════════════════════
 * C.  Cooper et al. 2001 iterative dominator tree
 * ═════════════════════════════════════════════════════════════════ */
static BlockID licm_intersect(BlockID b1, BlockID b2) {
  uint32_t f1 = licm_rpo_of[b1], f2 = licm_rpo_of[b2];
  while (f1 != f2) {
    while (f1 > f2) {
      b1 = licm_idom[b1];
      if (b1 == NO_BLOCK)
        return NO_BLOCK;
      f1 = licm_rpo_of[b1];
    }
    while (f2 > f1) {
      b2 = licm_idom[b2];
      if (b2 == NO_BLOCK)
        return NO_BLOCK;
      f2 = licm_rpo_of[b2];
    }
  }
  return b1;
}

static void licm_compute_doms(Function *fn) {
  for (uint32_t i = 0; i < fn->n_blocks; i++)
    licm_idom[i] = NO_BLOCK;
  licm_idom[fn->entry] = fn->entry;

  bool changed = true;
  while (changed) {
    changed = false;
    /* Iterate in RPO order; skip entry (index 0) */
    for (uint32_t ri = 1; ri < licm_rpo_n; ri++) {
      BlockID b = licm_rpo_arr[ri];
      Block *blk = fn->blocks[b];
      if (!blk || !blk->reachable)
        continue;

      BlockID new_idom = NO_BLOCK;
      for (uint32_t pi = 0; pi < blk->n_preds; pi++) {
        BlockID p = blk->preds[pi];
        if (p >= fn->n_blocks || licm_idom[p] == NO_BLOCK)
          continue;
        if (new_idom == NO_BLOCK)
          new_idom = p;
        else
          new_idom = licm_intersect(p, new_idom);
      }
      if (new_idom != NO_BLOCK && licm_idom[b] != new_idom) {
        licm_idom[b] = new_idom;
        changed = true;
      }
    }
  }
}

/* Returns true if 'd' dominates 'b' (walks idom chain from b to root). */
static bool licm_dominates(BlockID d, BlockID b) {
  uint32_t guard = 0;
  BlockID cur = b;
  while (guard++ < MAX_BLOCKS) {
    if (cur == d)
      return true;
    if (licm_idom[cur] == NO_BLOCK)
      return false;
    if (licm_idom[cur] == cur)
      return (cur == d); /* reached root */
    cur = licm_idom[cur];
  }
  return false;
}

/* ── Dominance frontiers (for global mem2reg) ── */
#define MAX_DF_PER_BLOCK 32
static uint32_t df_list[MAX_BLOCKS][MAX_DF_PER_BLOCK];
static uint32_t df_count[MAX_BLOCKS];

/** Compute DF(b) for each block: y is in DF(b) iff b dominates a predecessor of
 * y but not y. */
static void compute_dominance_frontiers(Function *fn) {
  for (uint32_t i = 0; i < fn->n_blocks; i++)
    df_count[i] = 0;

  for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
    Block *blk = fn->blocks[bi];
    if (!blk || !blk->reachable)
      continue;
    BlockID y = (BlockID)bi;
    BlockID idom_y = licm_idom[y];
    if (idom_y == NO_BLOCK)
      continue;

    for (uint32_t pi = 0; pi < blk->n_preds; pi++) {
      BlockID p = blk->preds[pi];
      if (p >= fn->n_blocks)
        continue;
      BlockID runner = p;
      while (runner != idom_y) {
        /* y is in DF(runner) */
        uint32_t dc = df_count[runner];
        uint32_t j;
        for (j = 0; j < dc; j++)
          if (df_list[runner][j] == y)
            break;
        if (j >= dc && dc < MAX_DF_PER_BLOCK) {
          df_list[runner][dc] = y;
          df_count[runner]++;
        }
        if (licm_idom[runner] == NO_BLOCK)
          break;
        runner = licm_idom[runner];
      }
    }
  }
}

/* ═════════════════════════════════════════════════════════════════
 * E.  Natural loop body via reverse BFS from latch to header
 * ═════════════════════════════════════════════════════════════════ */
static void licm_loop_body(Function *fn, BlockID header, BlockID latch,
                           BlkSet body) {
  static bool visited[MAX_BLOCKS];
  static BlockID queue[MAX_BLOCKS];
  memset(visited, 0, fn->n_blocks * sizeof(bool));

  int qh = 0, qt = 0;
  blkset_add(body, header);
  visited[header] = true;

  if (!blkset_has(body, latch)) {
    blkset_add(body, latch);
    visited[latch] = true;
    queue[qt++] = latch;
  }

  while (qh < qt) {
    BlockID cur = queue[qh++];
    Block *blk = fn->blocks[cur];
    for (uint32_t pi = 0; pi < blk->n_preds; pi++) {
      BlockID p = blk->preds[pi];
      if (p >= fn->n_blocks || visited[p])
        continue;
      visited[p] = true;
      blkset_add(body, p);
      if (qt < MAX_BLOCKS)
        queue[qt++] = p;
    }
  }
}

/* ═════════════════════════════════════════════════════════════════
 * F.  Preheader insertion
 *
 *  Before:   pred₁  pred₂ … (non-loop)   latch₁ latch₂ (loop back-edges)
 *                       ↘   ↙                  ↙ ↙
 *                         header  ←─────────────
 *
 *  After:    pred₁  pred₂ …          latch₁ latch₂
 *                       ↘↙                  ↙ ↙
 *                     preheader           header
 *                         └───────────────→ header
 *
 *  PHI nodes in header:  all non-loop source entries are merged into one
 *  entry via the preheader block.  If there are multiple non-loop sources
 *  with different values we use the first (correct for the single-pred case
 *  that arises in well-structured loops; production quality would insert
 *  copies in the preheader for each).
 * ═════════════════════════════════════════════════════════════════ */
static BlockID licm_insert_preheader(Function *fn, BlockID header,
                                     const BlkSet loop_body,
                                     uint32_t *synth_id) {
  if (fn->n_blocks >= MAX_BLOCKS)
    return (BlockID)NO_BLOCK;

  BlockID ph_id = fn->n_blocks++;
  fn->blocks[ph_id] = calloc(1, sizeof(Block));
  Block *ph = fn->blocks[ph_id];
  ph->id = ph_id;
  snprintf(ph->name, NAME_LEN, "preheader.%u", (unsigned)header);
  ph->reachable = true;
  ph->exec_freq = 0.0;
  ph->succs[0] = header;
  ph->n_succs = 1;
  ph->branch_prob[0] = 1.0;
  licm_idom[ph_id] = NO_BLOCK; /* dominator not in the tree yet */

  /* Preheader ends with an unconditional branch to the loop header */
  Instr *br = calloc(1, sizeof(Instr));
  br->id = (*synth_id)++;
  br->op = OP_BR;
  br->src[0] = header;
  br->n_src = 1;
  br->exec_freq = 1.0;
  ph->head = ph->tail = br;
  ph->n_instrs = 1;

  /* Collect non-loop predecessors of the header */
  Block *hdr = fn->blocks[header];
  BlockID nlp[MAX_PREDS];
  uint32_t n_nlp = 0;
  for (uint32_t pi = 0; pi < hdr->n_preds; pi++) {
    BlockID p = hdr->preds[pi];
    if (!blkset_has(loop_body, p))
      nlp[n_nlp++] = p;
  }

  /* Redirect non-loop predecessors to point at preheader */
  for (uint32_t i = 0; i < n_nlp; i++) {
    BlockID p = nlp[i];
    Block *pblk = fn->blocks[p];

    /* Update successor list */
    for (uint32_t si = 0; si < pblk->n_succs; si++)
      if (pblk->succs[si] == header) {
        pblk->succs[si] = ph_id;
        break;
      }

    /* Patch branch-instruction target operands */
    for (Instr *ins = pblk->head; ins; ins = ins->next)
      if (ins->op == OP_BR || ins->op == OP_CONDBR)
        for (uint32_t s = 0; s < ins->n_src; s++)
          if (ins->src[s] == header)
            ins->src[s] = ph_id;

    /* Register the predecessor of the preheader */
    if (ph->n_preds < MAX_PREDS)
      ph->preds[ph->n_preds++] = p;
  }

  /* Rebuild header predecessor list: keep only loop preds + preheader */
  uint32_t new_np = 0;
  for (uint32_t pi = 0; pi < hdr->n_preds; pi++) {
    BlockID p = hdr->preds[pi];
    if (blkset_has(loop_body, p))
      hdr->preds[new_np++] = p;
  }
  hdr->preds[new_np++] = ph_id;
  hdr->n_preds = new_np;

  /* Fix PHI nodes in header:
   * For each φ, collect the non-loop-pred sources into a single entry
   * via the preheader.  The first non-loop source value is used. */
  for (Instr *ins = hdr->head; ins; ins = ins->next) {
    if (ins->op != OP_PHI)
      continue;
    RegID ph_val = 0;
    bool found = false;
    uint32_t new_n = 0;
    for (uint32_t p = 0; p < ins->n_phi; p++) {
      if (!blkset_has(loop_body, ins->phi[p].block)) {
        if (!found) {
          ph_val = ins->phi[p].reg;
          found = true;
        }
        /* drop extra non-loop sources */
      } else {
        ins->phi[new_n++] = ins->phi[p];
      }
    }
    if (found) {
      ins->phi[new_n].reg = ph_val;
      ins->phi[new_n].block = ph_id;
      new_n++;
    }
    ins->n_phi = new_n;
  }

  return ph_id;
}

/* ═════════════════════════════════════════════════════════════════
 * G.  V2 Alias Oracle — base+offset per store (fixes V1: same memory, different
 * regs) ═════════════════════════════════════════════════════════════════ */
/* Walk backward from a pointer reg: if OP_ADD with OP_CONST, extract base and
 * offset. */
static void licm_analyze_ptr(Function *fn, RegID ptr, RegID *out_base,
                             int64_t *out_off, bool *out_var) {
  *out_base = ptr;
  *out_off = 0;
  *out_var = true;
  if (!ptr || ptr >= MAX_INSTRS)
    return;
  Instr *def = fn->def_of[ptr];
  if (!def)
    return;
  if (def->op == OP_ADD && def->n_src == 2) {
    RegID s0 = def->src[0], s1 = def->src[1];
    Instr *d0 = (s0 && s0 < MAX_INSTRS) ? fn->def_of[s0] : NULL;
    Instr *d1 = (s1 && s1 < MAX_INSTRS) ? fn->def_of[s1] : NULL;
    if (d1 && d1->op == OP_CONST) {
      *out_base = s0;
      *out_off = d1->imm;
      *out_var = false;
    } else if (d0 && d0->op == OP_CONST) {
      *out_base = s1;
      *out_off = d0->imm;
      *out_var = false;
    }
  } else if (def->op == OP_ALLOCA) {
    *out_base = ptr;
    *out_off = 0;
    *out_var = false;
  }
}

static void licm_build_alias_v2(Function *fn, const BlkSet loop_body,
                                AliasOracle *oracle) {
  oracle->n_targets = 0;
  oracle->clobber_all = false;
  for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
    if (!blkset_has(loop_body, bi))
      continue;
    for (Instr *ins = fn->blocks[bi]->head; ins; ins = ins->next) {
      if (ins->op == OP_STORE && ins->n_src >= 2) {
        if (oracle->n_targets >= MAX_LOOP_STORES) {
          oracle->clobber_all = true;
          return;
        }
        StoreTarget *tgt = &oracle->targets[oracle->n_targets++];
        licm_analyze_ptr(fn, ins->src[1], &tgt->base, &tgt->offset,
                         &tgt->is_variable);
      }
    }
  }
}

/* Loop contains OP_CALL → conservative: do not hoist any OP_LOAD (call may
 * write anywhere). */
static bool licm_loop_has_call(Function *fn, const BlkSet loop_body) {
  for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
    if (!blkset_has(loop_body, bi))
      continue;
    for (Instr *ins = fn->blocks[bi]->head; ins; ins = ins->next) {
      if (ins->op == OP_CALL)
        return true;
    }
  }
  return false;
}

/* Helper: return the block index that contains the instruction defining r, or
 * NO_BLOCK. */
static BlockID licm_def_block_of(Function *fn, RegID r) {
  if (!r || r >= MAX_INSTRS)
    return (BlockID)NO_BLOCK;
  for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
    Block *blk = fn->blocks[bi];
    if (!blk)
      continue;
    for (Instr *p = blk->head; p; p = p->next)
      if (p->dst == r)
        return bi;
  }
  return (BlockID)NO_BLOCK;
}

/* ═════════════════════════════════════════════════════════════════
 * H.  Invariance predicate
 *     Instruction is invariant iff it has no side effects AND every
 *     operand's defining instruction is outside the loop (or in preheader).
 *     Recursively: if any src is defined inside the loop → not invariant.
 * ═════════════════════════════════════════════════════════════════ */
static bool licm_is_invariant(Function *fn, const Instr *ins,
                              const BlkSet loop_body, BlockID preheader,
                              const Block *ph_blk, const Instr *ph_term,
                              bool allow_preheader_operands,
                              const AliasOracle *oracle, bool loop_has_call) {
  (void)ph_blk;
  (void)ph_term;
  (void)allow_preheader_operands;

  /* Side-effecting or control-flow instructions are never invariant */
  switch (ins->op) {
  case OP_STORE:
  case OP_CALL:
  case OP_RET:
  case OP_BR:
  case OP_CONDBR:
  case OP_PHI:
  case OP_NOP:
  case OP_ALLOCA:
  case OP_UNDEF:
  case OP_PGO_COUNTER_ADDR: /* PGO probe: must stay in block */
    return false;
  default:
    break;
  }

  /* Alias barrier V2 for OP_LOAD: peer through OP_ADD to base+offset; block
   * only if same base and same offset (or unknown offset). Allows p->y hoisted
   * past store to p->x. */
  if (ins->op == OP_LOAD && ins->n_src >= 1) {
    if (loop_has_call)
      return false;
    if (oracle->clobber_all)
      return false;
    RegID load_base;
    int64_t load_off;
    bool load_var;
    licm_analyze_ptr(fn, ins->src[0], &load_base, &load_off, &load_var);
    for (uint32_t i = 0; i < oracle->n_targets; i++) {
      const StoreTarget *tgt = &oracle->targets[i];
      if (tgt->base == load_base) {
        if (tgt->is_variable || load_var)
          return false;
        if (tgt->offset == load_off)
          return false;
        /* same base, different constant offset → no alias, continue */
      } else {
        Instr *db1 = fn->def_of[tgt->base];
        Instr *db2 = fn->def_of[load_base];
        bool distinct_allocas =
            (db1 && db2 && db1->op == OP_ALLOCA && db2->op == OP_ALLOCA &&
             tgt->base != load_base);
        if (!distinct_allocas)
          return false;
      }
    }
  }

  /* Every source RegID must be defined outside the loop (or in preheader).
   * Find def block by scanning so we don't rely on def_block[] being current.
   * If def is not found (NO_BLOCK) treat as not invariant — def may have been
   * removed by a prior pass (e.g. DCE), so do not hoist. */
  for (uint32_t s = 0; s < ins->n_src; s++) {
    RegID r = ins->src[s];
    BlockID db = licm_def_block_of(fn, r);
    if (db == (BlockID)NO_BLOCK || db >= fn->n_blocks)
      return false; /* unknown or out-of-range def → do not hoist */
    if (db == preheader)
      continue; /* already hoisted → ok */
    if (blkset_has(loop_body, db))
      return false; /* operand defined inside loop → not invariant */
  }
  return true;
}

/* ═════════════════════════════════════════════════════════════════
 * LICM pass entry point
 * ═════════════════════════════════════════════════════════════════ */
uint32_t licm_pass(Function *fn) {
  fn->stats.licm_hoisted = 0;
  fn->stats.licm_preheaders_inserted = 0;
  if (fn->n_blocks == 0)
    return 0;

  /* A — def→block map */
  licm_build_def_block(fn);

  /* B + C — RPO and dominator tree */
  licm_compute_rpo(fn);
  licm_compute_doms(fn);

  /* D — Discover natural loops via back edges */
  static LICMLoop loops[MAX_LOOPS];
  uint32_t n_loops = 0;
  uint32_t synth_id =
      fn->n_regs + 10000; /* synthetic IDs for preheader branches */

  for (uint32_t bi = 0; bi < fn->n_blocks && n_loops < MAX_LOOPS; bi++) {
    Block *blk = fn->blocks[bi];
    if (!blk || !blk->reachable)
      continue;

    for (uint32_t si = 0; si < blk->n_succs; si++) {
      BlockID h = blk->succs[si];
      if (h >= fn->n_blocks || licm_idom[h] == NO_BLOCK)
        continue;
      if (!licm_dominates(h, bi))
        continue; /* not a back edge */

      /* Back edge bi → h; h is the loop header */
      LICMLoop *lp = NULL;
      for (uint32_t li = 0; li < n_loops; li++)
        if (loops[li].header == h) {
          lp = &loops[li];
          break;
        }

      if (!lp) {
        if (n_loops >= MAX_LOOPS)
          break;
        lp = &loops[n_loops++];
        memset(lp, 0, sizeof(LICMLoop));
        lp->header = h;
        lp->preheader = (BlockID)NO_BLOCK;
        lp->has_preheader = false;
      }
      if (lp->n_latches < MAX_LOOP_BLOCKS)
        lp->latches[lp->n_latches++] = bi;

      /* E — compute (and merge) loop body */
      BlkSet extra;
      blkset_zero(extra);
      licm_loop_body(fn, h, bi, extra);
      blkset_merge(lp->body_bs, extra);
    }
  }

  if (n_loops == 0)
    return 0;

  fprintf(stderr, "[LICM]      %u natural loop(s) found\n", n_loops);

  /* F — Insert preheaders */
  for (uint32_t li = 0; li < n_loops; li++) {
    LICMLoop *lp = &loops[li];
    Block *hdr = fn->blocks[lp->header];

    uint32_t n_outside = 0;
    for (uint32_t pi = 0; pi < hdr->n_preds; pi++)
      if (!blkset_has(lp->body_bs, hdr->preds[pi]))
        n_outside++;

    if (n_outside == 0)
      continue; /* degenerate: all preds are back-edges */

    BlockID ph = licm_insert_preheader(fn, lp->header, lp->body_bs, &synth_id);
    if (ph != (BlockID)NO_BLOCK) {
      lp->preheader = ph;
      lp->has_preheader = true;
      fn->stats.licm_preheaders_inserted++;
      fprintf(stderr,
              "[LICM]      preheader '%s' inserted for loop header '%s'\n",
              fn->blocks[ph]->name, fn->blocks[lp->header]->name);
    }
  }

  /* Rebuild def→block map after preheader insertion may have added blocks */
  licm_build_def_block(fn);
  /* Rebuild reachability so later passes see the new blocks */
  for (uint32_t bi = 0; bi < fn->n_blocks; bi++)
    if (fn->blocks[bi])
      fn->blocks[bi]->reachable = false;
  /* quick BFS re-mark */
  {
    bool vis[MAX_BLOCKS];
    memset(vis, 0, sizeof(vis));
    BlockID q[MAX_BLOCKS];
    int qh = 0, qt = 0;
    q[qt++] = fn->entry;
    vis[fn->entry] = true;
    while (qh < qt) {
      BlockID cur = q[qh++];
      fn->blocks[cur]->reachable = true;
      for (uint32_t si = 0; si < fn->blocks[cur]->n_succs; si++) {
        BlockID s = fn->blocks[cur]->succs[si];
        if (s < fn->n_blocks && !vis[s]) {
          vis[s] = true;
          q[qt++] = s;
        }
      }
    }
  }
  /* Rebuild def→block so invariance check sees correct def locations (reachable
   * now set) */
  licm_build_def_block(fn);

  /* G + H — Hoist for each loop, innermost-last discovery order */
  uint32_t total = 0;
  for (uint32_t li = 0; li < n_loops; li++) {
    LICMLoop *lp = &loops[li];
    if (!lp->has_preheader)
      continue;

    Block *ph_blk = fn->blocks[lp->preheader];
    Instr *ph_term = ph_blk->tail; /* the OP_BR to header */

    AliasOracle oracle;
    licm_build_alias_v2(fn, lp->body_bs, &oracle);
    bool loop_has_call = licm_loop_has_call(fn, lp->body_bs);

    /* Iterate until stable: a hoisted instruction may enable another */
    bool progress = true;
    bool allow_ph =
        false; /* first round: only hoist if operands outside loop */
    while (progress) {
      progress = false;

      for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
        if (!blkset_has(lp->body_bs, bi))
          continue;
        Block *blk = fn->blocks[bi];
        if (!blk || !blk->reachable)
          continue;

        Instr *ins = blk->head;
        while (ins) {
          Instr *nxt = ins->next;
          if (licm_is_invariant(fn, ins, lp->body_bs, lp->preheader, ph_blk,
                                ph_term, allow_ph, &oracle, loop_has_call)) {
            /* Unlink from loop block */
            if (ins->prev)
              ins->prev->next = ins->next;
            else
              blk->head = ins->next;
            if (ins->next)
              ins->next->prev = ins->prev;
            else
              blk->tail = ins->prev;
            blk->n_instrs--;
            ins->prev = ins->next = NULL;

            /* Insert after the last operand def in the preheader so uses see
             * defs */
            Instr *insert_after = NULL;
            for (Instr *p = ph_blk->head; p && p != ph_term; p = p->next) {
              for (uint32_t s = 0; s < ins->n_src; s++) {
                RegID r = ins->src[s];
                if (r && r < MAX_INSTRS && fn->def_block[r] == lp->preheader &&
                    fn->def_of[r] == p)
                  insert_after = p;
              }
            }
            if (insert_after) {
              ins->next = insert_after->next;
              ins->prev = insert_after;
              if (insert_after->next)
                insert_after->next->prev = ins;
              else
                ph_blk->tail = ins;
              insert_after->next = ins;
            } else {
              ins->next = ph_blk->head;
              ins->prev = NULL;
              if (ph_blk->head)
                ph_blk->head->prev = ins;
              else
                ph_blk->tail = ins;
              ph_blk->head = ins;
            }
            ph_blk->n_instrs++;

            /* Update def→block map */
            if (ins->dst && ins->dst < MAX_INSTRS)
              fn->def_block[ins->dst] = lp->preheader;

            total++;
            progress = true;
            fprintf(stderr, "[LICM]      hoisted %%%u (%s) from '%s' -> '%s'\n",
                    (unsigned)ins->dst, opcode_name[ins->op], blk->name,
                    ph_blk->name);
          }
          ins = nxt;
        }
      }

      allow_ph =
          true; /* from next round, allow operands already in preheader */
      /* Rebuild alias oracle after each round (hoisted stores change store set)
       */
      if (progress)
        licm_build_alias_v2(fn, lp->body_bs, &oracle);
    }
  }

  fn->stats.licm_hoisted = total;
  return total;
}

/**
 * constant_fold_pass() — Fold binary ops and BNOT when operands are constants.
 * Mutates instructions in-place to OP_CONST so DCE can remove the now-redundant
 * constant definitions. Requires def_of[] to be populated
 * (licm_build_def_block).
 */
static uint32_t constant_fold_pass(Function *fn) {
  uint32_t folded = 0;
  for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
    Block *blk = fn->blocks[bi];
    if (!blk || !blk->reachable)
      continue;
    for (Instr *ins = blk->head; ins; ins = ins->next) {
      if (!ins->dst || ins->dst >= MAX_INSTRS)
        continue;
      int64_t result;
      Instr *d0 = (ins->n_src >= 1 && ins->src[0] < MAX_INSTRS)
                      ? fn->def_of[ins->src[0]]
                      : NULL;
      Instr *d1 = (ins->n_src >= 2 && ins->src[1] < MAX_INSTRS)
                      ? fn->def_of[ins->src[1]]
                      : NULL;

      switch (ins->op) {
      case OP_ADD:
        if (d0 && d1 && d0->op == OP_CONST && d1->op == OP_CONST) {
          result = d0->imm + d1->imm;
          goto fold_binary;
        }
        break;
      case OP_SUB:
        if (d0 && d1 && d0->op == OP_CONST && d1->op == OP_CONST) {
          result = d0->imm - d1->imm;
          goto fold_binary;
        }
        break;
      case OP_MUL:
        if (d0 && d1 && d0->op == OP_CONST && d1->op == OP_CONST) {
          result = d0->imm * d1->imm;
          goto fold_binary;
        }
        break;
      case OP_DIV:
        if (d0 && d1 && d0->op == OP_CONST && d1->op == OP_CONST &&
            d1->imm != 0) {
          result = d0->imm / d1->imm;
          goto fold_binary;
        }
        break;
      case OP_MOD:
        if (d0 && d1 && d0->op == OP_CONST && d1->op == OP_CONST &&
            d1->imm != 0) {
          result = d0->imm % d1->imm;
          goto fold_binary;
        }
        break;
      case OP_BAND:
        if (d0 && d1 && d0->op == OP_CONST && d1->op == OP_CONST) {
          result = (int64_t)((uint64_t)d0->imm & (uint64_t)d1->imm);
          goto fold_binary;
        }
        break;
      case OP_BOR:
        if (d0 && d1 && d0->op == OP_CONST && d1->op == OP_CONST) {
          result = (int64_t)((uint64_t)d0->imm | (uint64_t)d1->imm);
          goto fold_binary;
        }
        break;
      case OP_BXOR:
        if (d0 && d1 && d0->op == OP_CONST && d1->op == OP_CONST) {
          result = (int64_t)((uint64_t)d0->imm ^ (uint64_t)d1->imm);
          goto fold_binary;
        }
        break;
      case OP_SHL:
        if (d0 && d1 && d0->op == OP_CONST && d1->op == OP_CONST) {
          uint64_t u = (uint64_t)d0->imm;
          unsigned sh = (unsigned)(d1->imm & 63);
          result = (int64_t)(u << sh);
          goto fold_binary;
        }
        break;
      case OP_SHR:
        if (d0 && d1 && d0->op == OP_CONST && d1->op == OP_CONST) {
          uint64_t u = (uint64_t)d0->imm;
          unsigned sh = (unsigned)(d1->imm & 63);
          result = (int64_t)(u >> sh);
          goto fold_binary;
        }
        break;
      case OP_LT:
        if (d0 && d1 && d0->op == OP_CONST && d1->op == OP_CONST) {
          result = d0->imm < d1->imm ? 1 : 0;
          goto fold_binary;
        }
        break;
      case OP_EQ:
        if (d0 && d1 && d0->op == OP_CONST && d1->op == OP_CONST) {
          result = (d0->imm == d1->imm) ? 1 : 0;
          goto fold_binary;
        }
        break;
      case OP_NE:
        if (d0 && d1 && d0->op == OP_CONST && d1->op == OP_CONST) {
          result = (d0->imm != d1->imm) ? 1 : 0;
          goto fold_binary;
        }
        break;
      case OP_GT:
        if (d0 && d1 && d0->op == OP_CONST && d1->op == OP_CONST) {
          result = (d0->imm > d1->imm) ? 1 : 0;
          goto fold_binary;
        }
        break;
      case OP_GE:
        if (d0 && d1 && d0->op == OP_CONST && d1->op == OP_CONST) {
          result = (d0->imm >= d1->imm) ? 1 : 0;
          goto fold_binary;
        }
        break;
      case OP_LE:
        if (d0 && d1 && d0->op == OP_CONST && d1->op == OP_CONST) {
          result = (d0->imm <= d1->imm) ? 1 : 0;
          goto fold_binary;
        }
        break;
      case OP_BNOT:
        if (d0 && d0->op == OP_CONST) {
          result = (int64_t)(~(uint64_t)d0->imm);
          ins->op = OP_CONST;
          ins->imm = result;
          ins->n_src = 0;
          folded++;
        }
        break;
      default:
        break;
      }
      continue;
    fold_binary:
      ins->op = OP_CONST;
      ins->imm = result;
      ins->n_src = 0;
      ins->src[0] = 0;
      ins->src[1] = 0;
      folded++;
      continue;
    }
  }
  return folded;
}

/**
 * ssa_dce_pass() — SSA-form Dead Code Elimination
 *
 * @param fn   The function IR to transform (modified in-place).
 * @return     Number of instructions removed.
 */
uint32_t ssa_dce_pass(Function *fn) {
  /* live[r] == true → register r has at least one reachable use */
  bool live[MAX_INSTRS];
  memset(live, 0, sizeof(live));

  RegWorklist wl;
  memset(&wl, 0, sizeof(wl));

  /* ── Step 1: Seed worklist with operands of critical instructions ── */
  for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
    Block *blk = fn->blocks[bi];
    if (!blk->reachable)
      continue;

    for (Instr *ins = blk->head; ins; ins = ins->next) {
      if (!is_critical(ins))
        continue;

      /* Seed source operands. For BR/CONDBR only src[0] is used: BR has block
       * ID, CONDBR has condition RegID. */
      uint32_t n_seed = ins->n_src;
      if (ins->op == OP_BR)
        n_seed = 0; /* br: src[0] is block ID, no reg to seed */
      else if (ins->op == OP_CONDBR)
        n_seed = 1; /* condbr: only src[0] is condition RegID; src[1],src[2] are
                       block IDs */
      for (uint32_t s = 0; s < n_seed; s++) {
        RegID r = ins->src[s];
        if (r && !live[r]) {
          live[r] = true;
          wl_push(&wl, r);
        }
      }
      if (ins->op == OP_CALL) {
        for (uint32_t s = 0; s < ins->n_call_args; s++) {
          RegID r = ins->call_args[s];
          if (r && !live[r]) {
            live[r] = true;
            wl_push(&wl, r);
          }
        }
      }
      /* For φ-nodes that are critical (unusual but handle it) */
      if (ins->op == OP_PHI) {
        for (uint32_t p = 0; p < ins->n_phi; p++) {
          RegID r = ins->phi[p].reg;
          if (r && !live[r]) {
            live[r] = true;
            wl_push(&wl, r);
          }
        }
      }
    }
  }

  /* ── Step 2: Propagate liveness backward through def-use chains ── */
  while (wl.size > 0) {
    RegID r = wl_pop(&wl);

    Instr *def = fn->def_of[r];
    if (!def)
      continue; /* function argument / undefined → stop */

    /* Mark defining instruction's operands live.
     * BR:     src[0] is a block ID, not a value register — skip all.
     * CONDBR: src[0] is condition RegID; src[1],src[2] are block IDs — seed
     * only src[0]. */
    uint32_t def_n_seed = def->n_src;
    if (def->op == OP_BR)
      def_n_seed = 0;
    if (def->op == OP_CONDBR || def->op == OP_COPY)
      def_n_seed = 1;
    for (uint32_t s = 0; s < def_n_seed; s++) {
      RegID src = def->src[s];
      if (src && !live[src]) {
        live[src] = true;
        wl_push(&wl, src);
      }
    }
    if (def->op == OP_CALL) {
      for (uint32_t s = 0; s < def->n_call_args; s++) {
        RegID src = def->call_args[s];
        if (src && !live[src]) {
          live[src] = true;
          wl_push(&wl, src);
        }
      }
    }

    /*
     * φ-node special case:
     * Only propagate through source (rₖ: Bₖ) if the edge Bₖ → parent(φ)
     * is reachable. This is the liveness oracle based on φ-function
     * reachability — we don't need full dataflow because SSA guarantees
     * a single definition site, so unreachable predecessors can be
     * pruned precisely here.
     */
    if (def->op == OP_PHI) {
      for (uint32_t p = 0; p < def->n_phi; p++) {
        PhiSource *ps = &def->phi[p];
        Block *pred = fn->blocks[ps->block];
        if (!pred || !pred->reachable)
          continue; /* prune unreachable */

        if (!live[ps->reg]) {
          live[ps->reg] = true;
          wl_push(&wl, ps->reg);
        }
      }
    }
  }

  /* ── Step 3: Mark dead instructions & remove them ── */
  uint32_t removed = 0;

  for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
    Block *blk = fn->blocks[bi];

    for (Instr *ins = blk->head; ins; ins = ins->next) {
      if (is_critical(ins))
        continue; /* never remove critical ops */
      if (ins->op == OP_NOP)
        continue;

      /* Dead iff destination register is not live */
      if (ins->dst && !live[ins->dst]) {
        ins->dead = true;
        removed++;
      }
    }

    /* Compact instruction list (remove dead nodes) */
    Instr *cur = blk->head;
    while (cur) {
      Instr *next = cur->next;
      if (cur->dead) {
        if (cur->prev)
          cur->prev->next = cur->next;
        else
          blk->head = cur->next;
        if (cur->next)
          cur->next->prev = cur->prev;
        else
          blk->tail = cur->prev;
        blk->n_instrs--;
        free(cur);
      }
      cur = next;
    }
  }

  /*
   * ── Step 4: Remove unreachable blocks ──
   * A block is dead if it is not reachable from the entry block.
   * We already have reachability pre-computed; just clean up.
   */
  for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
    Block *blk = fn->blocks[bi];
    if (!blk->reachable && blk->id != fn->entry) {
      /* Free instructions */
      Instr *cur = blk->head;
      while (cur) {
        Instr *n = cur->next;
        free(cur);
        cur = n;
      }
      blk->head = blk->tail = NULL;
      blk->n_instrs = 0;
      fn->stats.dce_blocks_removed++;
    }
  }

  fn->stats.dce_instrs_removed += removed;
  return removed;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * PASS 2: ESCAPE ANALYSIS + STACK PROMOTION
 *
 * Theory
 * ──────
 * A heap allocation (OP_ALLOCA or a modeled malloc) *escapes* if any pointer
 * derived from it can be observed after the function returns, or is passed
 * to an opaque call that may store it globally.
 *
 * Escape conditions:
 *   E1. The allocation address is returned (OP_RET with alloc-derived operand).
 *   E2. The allocation address flows into a STORE whose pointer operand is
 *       itself not a local alloca (i.e., stored into the heap).
 *   E3. The allocation address is passed as an argument to a CALL.
 *   E4. A pointer derived via GEP from the allocation escapes by E1–E3.
 *
 * If none of E1–E3 applies (transitively), the pointer's provenance is
 * confined to the activation record → the heap allocation can be replaced
 * by a stack allocation.  In x86-64 ABI terms: emit a SUB RSP, N instead
 * of a malloc call.
 *
 * Implementation uses a flow-insensitive points-to abstraction:
 *   points_to[r] = set of AllocaIDs that register r may point to.
 *
 * A single forward pass builds points-to sets; a backward escape propagation
 * then marks allocations that flow to an escape site.
 * ───────────────────────────────────────────────────────────────────────────
 */

typedef uint32_t AllocaID;

typedef struct {
  AllocaID id;
  RegID base_reg; /* the register receiving the alloca result  */
  uint32_t size_bytes;
  bool escapes; /* true → cannot promote to stack            */
} AllocaRecord;

typedef struct {
  AllocaRecord allocs[MAX_ALLOCS];
  uint32_t n_allocs;

  /*
   * points_to[r] — simplified: single alloca ID each register may alias.
   * A production pass would use a bitset here; we use a scalar for clarity.
   */
  AllocaID points_to[MAX_INSTRS]; /* indexed by RegID */
} EscapeCtx;

static AllocaID ea_alloc_of(EscapeCtx *ctx, RegID r) {
  return (r < MAX_INSTRS) ? ctx->points_to[r] : NO_ALLOC;
}

/**
 * escape_analysis_pass() — Points-to + Escape analysis, then stack promotion.
 *
 * @param fn   Function IR to analyze.
 * @param ctx  Escape context (caller allocates, zeroed).
 * @return     Number of allocations promoted to the stack.
 */
uint32_t escape_analysis_pass(Function *fn, EscapeCtx *ctx) {
  memset(ctx->points_to, 0xFF, sizeof(ctx->points_to)); /* init = NO_ALLOC */

  /* ── Step 1: Discover all OP_ALLOCA sites ── */
  for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
    Block *blk = fn->blocks[bi];
    if (!blk->reachable)
      continue;

    for (Instr *ins = blk->head; ins; ins = ins->next) {
      if (ins->op != OP_ALLOCA)
        continue;
      if (ctx->n_allocs >= MAX_ALLOCS)
        break;

      AllocaRecord *ar = &ctx->allocs[ctx->n_allocs++];
      ar->id = ctx->n_allocs - 1;
      ar->base_reg = ins->dst;
      ar->size_bytes = (ins->n_src > 0) ? ins->src[0] : 8; /* default 8B */
      ar->escapes = false;

      ctx->points_to[ins->dst] = ar->id;
    }
  }

  /* ── Step 2: Forward propagation of points-to through GEP / COPY / PHI ── */
  bool changed = true;
  while (changed) {
    changed = false;

    for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
      Block *blk = fn->blocks[bi];
      if (!blk->reachable)
        continue;

      for (Instr *ins = blk->head; ins; ins = ins->next) {
        AllocaID aid = NO_ALLOC;

        switch (ins->op) {
        case OP_GEP:
        case OP_COPY:
          /* dst inherits the provenance of src[0] */
          aid = ea_alloc_of(ctx, ins->src[0]);
          break;

        case OP_PHI:
          /*
           * φ(r₀:B₀, r₁:B₁) — if all reachable sources point to
           * the same alloca, the φ result inherits it; otherwise
           * the result is ambiguous (conservative: mark escape).
           */
          for (uint32_t p = 0; p < ins->n_phi; p++) {
            Block *pred = fn->blocks[ins->phi[p].block];
            if (!pred || !pred->reachable)
              continue;
            AllocaID src_aid = ea_alloc_of(ctx, ins->phi[p].reg);
            if (aid == NO_ALLOC)
              aid = src_aid;
            else if (aid != src_aid)
              aid = NO_ALLOC; /* ambiguous */
          }
          break;

        case OP_LOAD:
          /* loaded value: no provenance unless we track heap contents */
          aid = NO_ALLOC;
          break;

        default:
          continue; /* other ops don't propagate pointers */
        }

        if (ins->dst && aid != ctx->points_to[ins->dst]) {
          ctx->points_to[ins->dst] = aid;
          changed = true;
        }
      }
    }
  }

  /* ── Step 3: Escape detection — mark allocations that reach escape sites ──
   */
  for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
    Block *blk = fn->blocks[bi];
    if (!blk->reachable)
      continue;

    for (Instr *ins = blk->head; ins; ins = ins->next) {

      /* E1: alloca-derived pointer returned */
      if (ins->op == OP_RET) {
        for (uint32_t s = 0; s < ins->n_src; s++) {
          AllocaID aid = ea_alloc_of(ctx, ins->src[s]);
          if (aid != NO_ALLOC)
            ctx->allocs[aid].escapes = true;
        }
      }

      /* E2: alloca address stored into non-local memory
       *   STORE <value> <pointer>
       *   If <value> is alloca-derived AND <pointer> is not a local alloca
       *   → the pointer escapes into the heap.
       */
      if (ins->op == OP_STORE && ins->n_src >= 2) {
        RegID value_reg = ins->src[0];
        RegID ptr_reg = ins->src[1];
        AllocaID val_aid = ea_alloc_of(ctx, value_reg);
        AllocaID ptr_aid = ea_alloc_of(ctx, ptr_reg);

        if (val_aid != NO_ALLOC && ptr_aid == NO_ALLOC) {
          /* value is a local alloca, pointer is external → escape */
          ctx->allocs[val_aid].escapes = true;
        }
      }

      /* E3: alloca passed as argument to a call */
      if (ins->op == OP_CALL) {
        for (uint32_t s = 0; s < ins->n_call_args; s++) {
          AllocaID aid = ea_alloc_of(ctx, ins->call_args[s]);
          if (aid != NO_ALLOC)
            ctx->allocs[aid].escapes = true;
        }
      }
    }
  }

  /* ── Step 4: Tag allocations (promotable vs escaping) for
   * scalar_promotion_pass ── */
  uint32_t promoted = 0;

  for (uint32_t ai = 0; ai < ctx->n_allocs; ai++) {
    AllocaRecord *ar = &ctx->allocs[ai];
    RegID base = ar->base_reg;
    if (base >= MAX_INSTRS)
      continue;
    Instr *def = fn->def_of[base];
    if (!def || def->op != OP_ALLOCA)
      continue;

    if (ar->escapes || def->escape) {
      def->escape = true; /* mark escaping so scalar promotion skips */
    } else {
      def->escape =
          false; /* non-escaping: stack-promotable, mem2reg candidate */
      promoted++;
    }
  }

  fn->stats.ea_promotions += promoted;
  return promoted;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * SCALAR PROMOTION (mem2reg) — Single-block fast path + multi-block (dominance
 * frontiers)
 *
 * For allocas that never escape, if all loads/stores occur in the same block
 * as the alloca (or one block), replace memory with virtual registers and
 * remove the load/store/alloca. Otherwise use dominance frontiers + SSA rename.
 * ───────────────────────────────────────────────────────────────────────────
 */
static uint32_t multi_block_mem2reg_one(Function *fn, EscapeCtx *ctx,
                                        AllocaRecord *ar,
                                        RegID repl[MAX_INSTRS],
                                        bool repl_valid[MAX_INSTRS]);

static BlockID block_containing_instr(Function *fn, Instr *ins) {
  for (BlockID bi = 0; bi < fn->n_blocks; bi++) {
    Block *blk = fn->blocks[bi];
    if (!blk)
      continue;
    for (Instr *p = blk->head; p; p = p->next)
      if (p == ins)
        return bi;
  }
  return NO_BLOCK;
}

static void unlink_instr(Block *blk, Instr *ins) {
  if (ins->prev)
    ins->prev->next = ins->next;
  else
    blk->head = ins->next;
  if (ins->next)
    ins->next->prev = ins->prev;
  else
    blk->tail = ins->prev;
  blk->n_instrs--;
}

static uint32_t scalar_promotion_pass(Function *fn, EscapeCtx *ctx) {
  uint32_t promoted_count = 0;
  RegID repl[MAX_INSTRS]; /* repl[r] = register to use instead of r (for loads
                             we're removing) */
  bool repl_valid[MAX_INSTRS];
  memset(repl_valid, 0, sizeof(repl_valid));

  /* Compute RPO and dominators once so multi-block mem2reg can use dominance
   * frontiers */
  licm_compute_rpo(fn);
  licm_compute_doms(fn);
  compute_dominance_frontiers(fn);

  for (uint32_t ai = 0; ai < ctx->n_allocs; ai++) {
    AllocaRecord *ar = &ctx->allocs[ai];
    if (ar->escapes)
      continue;
    RegID base_reg = ar->base_reg;
    Instr *alloca_ins = fn->def_of[base_reg];
    if (!alloca_ins || alloca_ins->op != OP_ALLOCA || alloca_ins->escape)
      continue;

    BlockID alloca_block = block_containing_instr(fn, alloca_ins);
    if (alloca_block == NO_BLOCK)
      continue;

    bool has_store = false;
    for (uint32_t bi = 0; bi < fn->n_blocks && !has_store; bi++) {
      Block *b = fn->blocks[bi];
      if (!b || !b->reachable)
        continue;
      for (Instr *ins = b->head; ins; ins = ins->next) {
        if (ins->op == OP_STORE && ins->n_src >= 2 && ins->src[1] == base_reg) {
          has_store = true;
          break;
        }
      }
    }
    if (!has_store)
      continue;

    Block *blk = fn->blocks[alloca_block];
    if (!blk || !blk->reachable)
      continue;

    /* Single-block check: every store (src[1]==base_reg) and load
     * (src[0]==base_reg) must be in alloca_block */
    bool single_block = true;
    for (Instr *ins = blk->head; ins; ins = ins->next) {
      if (ins->op == OP_STORE && ins->n_src >= 2 && ins->src[1] == base_reg)
        continue;
      if (ins->op == OP_LOAD && ins->n_src >= 1 && ins->src[0] == base_reg)
        continue;
    }
    for (BlockID bi = 0; bi < fn->n_blocks && single_block; bi++) {
      if (bi == alloca_block)
        continue;
      Block *b = fn->blocks[bi];
      if (!b || !b->reachable)
        continue;
      for (Instr *ins = b->head; ins; ins = ins->next) {
        if (ins->op == OP_STORE && ins->n_src >= 2 && ins->src[1] == base_reg) {
          single_block = false;
          break;
        }
        if (ins->op == OP_LOAD && ins->n_src >= 1 && ins->src[0] == base_reg) {
          single_block = false;
          break;
        }
      }
    }
    if (!single_block) {
      uint32_t n = multi_block_mem2reg_one(fn, ctx, ar, repl, repl_valid);
      promoted_count += n;
      continue;
    }

    /* Linear walk: track current value for base_reg, build repl[load_dst] =
     * value_reg */
    RegID current_val = 0;
    bool current_val_set = false;
    Instr *loads_to_remove[256];
    Instr *stores_to_remove[256];
    uint32_t n_loads = 0, n_stores = 0;

    for (Instr *ins = blk->head; ins; ins = ins->next) {
      if (ins->op == OP_ALLOCA && ins->dst == base_reg) {
        /* alloca defines the slot; no value yet until first store */
        continue;
      }
      if (ins->op == OP_STORE && ins->n_src >= 2 && ins->src[1] == base_reg) {
        current_val = ins->src[0];
        while (current_val < MAX_INSTRS && repl_valid[current_val]) {
          current_val = repl[current_val];
        }
        current_val_set = true;
        if (n_stores < 256)
          stores_to_remove[n_stores++] = ins;
        continue;
      }
      if (ins->op == OP_LOAD && ins->n_src >= 1 && ins->src[0] == base_reg) {
        if (current_val_set && ins->dst < MAX_INSTRS) {
          repl[ins->dst] = current_val;
          repl_valid[ins->dst] = true;
        }
        if (n_loads < 256)
          loads_to_remove[n_loads++] = ins;
        continue;
      }
    }

    /* Replace every use of a load_dst with repl[load_dst] in this block */
    for (Instr *ins = blk->head; ins; ins = ins->next) {
      if (ins->dead)
        continue;
      for (uint32_t s = 0; s < ins->n_src; s++) {
        if (ins->op == OP_BR)
          continue;
        if (ins->op == OP_CONDBR && s >= 1)
          continue;
        if (ins->src[s] < MAX_INSTRS && repl_valid[ins->src[s]])
          ins->src[s] = repl[ins->src[s]];
      }
      if (ins->op == OP_PHI)
        for (uint32_t p = 0; p < ins->n_phi; p++)
          if (ins->phi[p].reg < MAX_INSTRS && repl_valid[ins->phi[p].reg])
            ins->phi[p].reg = repl[ins->phi[p].reg];
      if (ins->op == OP_CALL)
        for (uint32_t c = 0; c < ins->n_call_args; c++)
          if (ins->call_args[c] < MAX_INSTRS && repl_valid[ins->call_args[c]])
            ins->call_args[c] = repl[ins->call_args[c]];
    }

    /* Unlink and free loads, stores, alloca; clear def_of for removed defs */
    for (uint32_t i = 0; i < n_loads; i++) {
      Instr *ins = loads_to_remove[i];
      RegID d = ins->dst;
      if (d < MAX_INSTRS) {
        fn->def_of[d] = NULL;
        repl_valid[d] = false;
      }
      unlink_instr(blk, ins);
      free(ins);
    }
    for (uint32_t i = 0; i < n_stores; i++) {
      Instr *ins = stores_to_remove[i];
      unlink_instr(blk, ins);
      free(ins);
    }
    if (alloca_ins->dst && alloca_ins->dst < MAX_INSTRS)
      fn->def_of[alloca_ins->dst] = NULL;
    unlink_instr(blk, alloca_ins);
    free(alloca_ins);
    promoted_count++;
  }
  return promoted_count;
}

/* ── Multi-block mem2reg: dominance frontiers + SSA rename ── */
#define MAX_IDOM_CHILDREN 32
static bool in_need_phi[MAX_BLOCKS];
static BlockID idom_children_list[MAX_BLOCKS][MAX_IDOM_CHILDREN];
static uint32_t idom_children_count[MAX_BLOCKS];

static void build_idom_children(Function *fn) {
  for (uint32_t i = 0; i < fn->n_blocks; i++)
    idom_children_count[i] = 0;
  for (uint32_t i = 0; i < fn->n_blocks; i++) {
    BlockID b = (BlockID)i;
    BlockID d = licm_idom[b];
    if (d == NO_BLOCK || d == b)
      continue;
    if (idom_children_count[d] < MAX_IDOM_CHILDREN)
      idom_children_list[d][idom_children_count[d]++] = b;
  }
}

/* Insert instruction at head of block (PHIs must be first). */
static void insert_instr_at_head(Block *blk, Instr *ins) {
  ins->next = blk->head;
  ins->prev = NULL;
  if (blk->head)
    blk->head->prev = ins;
  else
    blk->tail = ins;
  blk->head = ins;
  blk->n_instrs++;
}

static void mem2reg_rename_rec(Function *fn, BlockID b, RegID base_reg,
                               Instr **phi_instrs, RegID *stack,
                               uint32_t *stack_top, RegID *repl,
                               bool *repl_valid) {
  if (b >= fn->n_blocks)
    return;
  Block *blk = fn->blocks[b];
  if (!blk || !blk->reachable)
    return;
  uint32_t defs_here = 0;
  Instr *phi = phi_instrs[b];
  if (phi && (uintptr_t)phi->imm == (uintptr_t)base_reg) {
    stack[(*stack_top)++] = phi->dst;
    defs_here++;
  }
  for (Instr *ins = blk->head; ins; ins = ins->next) {
    if (ins->op == OP_ALLOCA && ins->dst == base_reg)
      continue;
    if (ins->op == OP_STORE && ins->n_src >= 2 && ins->src[1] == base_reg) {
      stack[(*stack_top)++] = ins->src[0];
      defs_here++;
      continue;
    }
    if (ins->op == OP_LOAD && ins->n_src >= 1 && ins->src[0] == base_reg) {
      if (*stack_top > 0 && ins->dst < MAX_INSTRS) {
        repl[ins->dst] = stack[(*stack_top) - 1];
        repl_valid[ins->dst] = true;
      }
      continue;
    }
  }
  RegID out_val = *stack_top > 0 ? stack[(*stack_top) - 1] : 0;
  for (uint32_t si = 0; si < blk->n_succs; si++) {
    BlockID s = blk->succs[si];
    if (s >= fn->n_blocks)
      continue;
    Instr *sphi = phi_instrs[s];
    if (!sphi)
      continue;
    if ((uintptr_t)sphi->imm != (uintptr_t)base_reg)
      continue;
    for (uint32_t p = 0; p < sphi->n_phi; p++) {
      if (sphi->phi[p].block == b) {
        sphi->phi[p].reg = out_val;
        break;
      }
    }
  }
  for (uint32_t i = 0; i < idom_children_count[b]; i++) {
    mem2reg_rename_rec(fn, idom_children_list[b][i], base_reg, phi_instrs,
                       stack, stack_top, repl, repl_valid);
  }
  (*stack_top) -= defs_here;
}

/** Promote one non-escaping alloca to SSA using dominance frontiers. Returns 1
 * if promoted. */

static uint32_t multi_block_mem2reg_one(Function *fn, EscapeCtx *ctx,
                                        AllocaRecord *ar,
                                        RegID repl[MAX_INSTRS],
                                        bool repl_valid[MAX_INSTRS]) {
  RegID base_reg = ar->base_reg;
  if (base_reg >= MAX_INSTRS)
    return 0;
  Instr *alloca_ins = fn->def_of[base_reg];
  if (!alloca_ins || alloca_ins->op != OP_ALLOCA || alloca_ins->escape)
    return 0;

  BlockID alloca_block = block_containing_instr(fn, alloca_ins);
  if (alloca_block == NO_BLOCK)
    return 0;

  /* Def blocks: block containing alloca + every block with a store to base_reg
   */
  bool def_block[MAX_BLOCKS];
  memset(def_block, 0, sizeof(def_block));
  def_block[alloca_block] = true;
  for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
    Block *b = fn->blocks[bi];
    if (!b || !b->reachable)
      continue;
    for (Instr *ins = b->head; ins; ins = ins->next) {
      if (ins->op == OP_STORE && ins->n_src >= 2 && ins->src[1] == base_reg) {
        def_block[bi] = true;
        break;
      }
    }
  }

  /* Iterated dominance frontier → need_phi */
  memset(in_need_phi, 0, sizeof(in_need_phi));
  BlockID worklist[MAX_BLOCKS];
  uint32_t wl_head = 0, wl_tail = 0;
  for (uint32_t i = 0; i < fn->n_blocks; i++) {
    if (!def_block[i])
      continue;
    in_need_phi[i] = true;
    worklist[wl_tail++] = (BlockID)i;
  }
  while (wl_head < wl_tail) {
    BlockID b = worklist[wl_head++];
    for (uint32_t j = 0; j < df_count[b]; j++) {
      BlockID y = df_list[b][j];
      if (!in_need_phi[y]) {
        in_need_phi[y] = true;
        if (wl_tail < MAX_BLOCKS)
          worklist[wl_tail++] = y;
      }
    }
  }

  /* Allocate new regs for PHI results; assign PHI instr ids */
  InstrID next_instr_id = 0;
  for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
    Block *blk = fn->blocks[bi];
    if (!blk)
      continue;
    for (Instr *ins = blk->head; ins; ins = ins->next)
      if (ins->id >= next_instr_id)
        next_instr_id = ins->id + 1;
  }
  Instr *phi_instrs[MAX_BLOCKS]; /* phi_instrs[block_id] = PHI for this
                                    variable, or NULL */
  memset(phi_instrs, 0, sizeof(phi_instrs));
  for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
    if (!in_need_phi[bi])
      continue;
    Block *blk = fn->blocks[bi];
    if (!blk || blk->n_preds > MAX_PHI_SOURCES)
      continue;

    Instr *phi = calloc(1, sizeof(Instr));
    phi->id = next_instr_id++;
    phi->op = OP_PHI;
    phi->dst = fn->n_regs++;
    phi->n_phi = blk->n_preds;
    for (uint32_t p = 0; p < blk->n_preds; p++) {
      phi->phi[p].block = blk->preds[p];
      phi->phi[p].reg = 0; /* filled during rename */
    }
    phi->imm =
        (int64_t)(uintptr_t)base_reg; /* mark which variable this PHI is for */
    phi->exec_freq = 1.0;
    insert_instr_at_head(blk, phi);
    phi_instrs[bi] = phi;
  }

  build_idom_children(fn);

  /* Rename: recursively process blocks over dominator tree; stack of current
   * value (RegID) */
  RegID stack[MAX_INSTRS];
  uint32_t stack_top = 0;
  mem2reg_rename_rec(fn, fn->entry, base_reg, phi_instrs, stack, &stack_top,
                     repl, repl_valid);

  /* Replace all uses of load destinations with repl[] */
  for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
    Block *blk = fn->blocks[bi];
    if (!blk || !blk->reachable)
      continue;
    for (Instr *ins = blk->head; ins; ins = ins->next) {
      if (ins->dead)
        continue;
      for (uint32_t s = 0; s < ins->n_src; s++) {
        if (ins->op == OP_BR)
          continue;
        if (ins->op == OP_CONDBR && s >= 1)
          continue;
        if (ins->src[s] < MAX_INSTRS && repl_valid[ins->src[s]])
          ins->src[s] = repl[ins->src[s]];
      }
      if (ins->op == OP_PHI)
        for (uint32_t p = 0; p < ins->n_phi; p++)
          if (ins->phi[p].reg < MAX_INSTRS && repl_valid[ins->phi[p].reg])
            ins->phi[p].reg = repl[ins->phi[p].reg];
      if (ins->op == OP_CALL)
        for (uint32_t c = 0; c < ins->n_call_args; c++)
          if (ins->call_args[c] < MAX_INSTRS && repl_valid[ins->call_args[c]])
            ins->call_args[c] = repl[ins->call_args[c]];
    }
  }

  /* Remove loads, stores, alloca; collect in reverse order per block to avoid
   * use-after-free */
  for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
    Block *blk = fn->blocks[bi];
    if (!blk || !blk->reachable)
      continue;
    Instr *to_remove[256];
    uint32_t n_remove = 0;
    for (Instr *ins = blk->head; ins; ins = ins->next) {
      if (ins->op == OP_LOAD && ins->n_src >= 1 && ins->src[0] == base_reg) {
        if (n_remove < 256)
          to_remove[n_remove++] = ins;
        continue;
      }
      if (ins->op == OP_STORE && ins->n_src >= 2 && ins->src[1] == base_reg) {
        if (n_remove < 256)
          to_remove[n_remove++] = ins;
        continue;
      }
      if (ins->op == OP_ALLOCA && ins->dst == base_reg) {
        if (n_remove < 256)
          to_remove[n_remove++] = ins;
        continue;
      }
    }
    for (uint32_t i = 0; i < n_remove; i++) {
      Instr *ins = to_remove[i];
      RegID d = ins->dst;
      if (d < MAX_INSTRS)
        fn->def_of[d] = NULL;
      unlink_instr(blk, ins);
      free(ins);
    }
  }
  return 1;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * PASS 3: PROFILE-GUIDED BASIC BLOCK REORDERING
 *
 * Theory
 * ──────
 * Modern CPUs fetch instructions in 64-byte cache lines. If a hot loop body
 * is split across multiple cache lines — because cold exception-handling code
 * was interleaved — fetch stalls are incurred on every iteration.
 *
 * PGO-based reordering uses instrumentation-derived execution frequencies to
 * construct a linear block ordering that maximises icache locality:
 *
 *   1. Build a *trace* (chain) of basic blocks ordered by execution frequency.
 *      Start from the entry block and greedily extend by selecting the most
 *      frequently executed successor that has not yet been placed.
 *
 *   2. The greedy extension criterion is:
 *        next = argmax_{s ∈ succs(B), !placed(s)} exec_freq(s)
 *
 *   3. When no un-placed successor is available, start a new chain from the
 *      globally most frequent un-placed block (cold blocks float to the tail).
 *
 *   4. Emit chains in descending frequency order. Cold blocks (exception paths,
 *      unlikely branches) are emitted last — typically into a separate section
 *      (.text.cold) to avoid polluting the primary instruction cache.
 *
 * Branch probability propagation:
 *   exec_freq(successor) = exec_freq(block) × branch_prob[i]
 *   This is seeded at the entry block (exec_freq = 1.0) and propagated
 *   forward through the CFG. For loops, a fixpoint is computed using
 *   two-pass dominance: natural loop headers get their back-edge frequency
 *   accumulated (simplified Dempster-Shafer estimate).
 *
 * Complexity: O(B log B) where B = number of basic blocks.
 * ───────────────────────────────────────────────────────────────────────────
 */

/* Comparator: sort blocks descending by execution frequency */
static Block *g_blocks_for_sort[MAX_BLOCKS];
static int cmp_freq_desc(const void *a, const void *b) {
  uint32_t ia = *(const uint32_t *)a;
  uint32_t ib = *(const uint32_t *)b;
  double fa = g_blocks_for_sort[ia] ? g_blocks_for_sort[ia]->exec_freq : 0.0;
  double fb = g_blocks_for_sort[ib] ? g_blocks_for_sort[ib]->exec_freq : 0.0;
  if (fa > fb)
    return -1;
  if (fa < fb)
    return 1;
  return 0;
}

/**
 * propagate_exec_freq() — Forward-propagate branch probabilities.
 *
 * Seeds entry block at freq=1.0 and propagates freq × prob through each
 * CFG edge.  Loops are handled with a fixed iteration limit (3 passes
 * approximates the loop trip count contribution).
 */
static BlockID next_unplaced(Function *fn, uint32_t *sorted) {
  for (uint32_t i = 0; i < fn->n_blocks; i++) {
    BlockID id = sorted[i];
    if (id < fn->n_blocks && !fn->blocks[id]->placed &&
        fn->blocks[id]->reachable)
      return id;
  }
  return 0xFFFFFFFFu;
}

static void propagate_exec_freq(Function *fn) {
  /* Initialize all blocks to 0 except entry */
  for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
    fn->blocks[bi]->exec_freq = 0.0;
  }
  if (fn->n_blocks == 0)
    return;
  fn->blocks[fn->entry]->exec_freq = 1.0;

  /* Three passes handle typical single-level loops */
  for (int pass = 0; pass < 3; pass++) {
    for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
      Block *blk = fn->blocks[bi];
      if (!blk->reachable || blk->exec_freq == 0.0)
        continue;

      for (uint32_t si = 0; si < blk->n_succs; si++) {
        BlockID sid = blk->succs[si];
        if (sid >= fn->n_blocks)
          continue;
        Block *succ = fn->blocks[sid];

        double prob = (blk->n_succs == 1) ? 1.0 : blk->branch_prob[si];

        succ->exec_freq += blk->exec_freq * prob;
      }
    }
  }
}

/**
 * pgo_reorder_pass() — Greedy trace-based basic block reordering.
 *
 * @param fn          Function IR.
 * @param order_out   Output array of block IDs in the new emission order.
 *                    Caller provides an array of size fn->n_blocks.
 * @return            Number of blocks reordered (same as fn->n_blocks unless
 *                    unreachable blocks were pruned).
 */
uint32_t pgo_reorder_pass(Function *fn, BlockID *order_out) {
  /* Step 1: propagate frequencies from branch probabilities */
  propagate_exec_freq(fn);

  /* Step 2: reset placement state */
  for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
    fn->blocks[bi]->placed = false;
    fn->blocks[bi]->chain_next = 0xFFFFFFFF;
    g_blocks_for_sort[bi] = fn->blocks[bi];
  }

  /* Step 3: build sorted order by frequency (for chain start selection) */
  uint32_t sorted[MAX_BLOCKS];
  for (uint32_t i = 0; i < fn->n_blocks; i++)
    sorted[i] = i;
  qsort(sorted, fn->n_blocks, sizeof(uint32_t), cmp_freq_desc);

  uint32_t n_placed = 0;

  /* Step 4: entry block anchors the first chain */
  BlockID chain_start = fn->entry;

  while (chain_start != 0xFFFFFFFF) {
    BlockID cur = chain_start;
    fn->blocks[cur]->placed = true;
    order_out[n_placed++] = cur;

    /* Greedily extend the chain */
    while (true) {
      Block *blk = fn->blocks[cur];
      double best_f = -1.0;
      BlockID best_id = 0xFFFFFFFF;

      for (uint32_t si = 0; si < blk->n_succs; si++) {
        BlockID sid = blk->succs[si];
        if (sid >= fn->n_blocks)
          continue;
        if (fn->blocks[sid]->placed)
          continue;
        if (!fn->blocks[sid]->reachable)
          continue;

        double f = fn->blocks[sid]->exec_freq;
        if (f > best_f) {
          best_f = f;
          best_id = sid;
        }
      }

      if (best_id == 0xFFFFFFFF)
        break; /* no more successors to extend */

      blk->chain_next = best_id;
      fn->blocks[best_id]->placed = true;
      order_out[n_placed++] = best_id;
      cur = best_id;
    }

    /* Start next chain from the highest-frequency un-placed block */
    chain_start = next_unplaced(fn, sorted);
  }

  fn->stats.pgo_blocks_reordered += n_placed;
  return n_placed;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * REACHABILITY ANALYSIS  (prerequisite for all three passes)
 * ───────────────────────────────────────────────────────────────────────────
 */

static void compute_reachability(Function *fn) {
  /* BFS from entry */
  bool visited[MAX_BLOCKS];
  memset(visited, 0, sizeof(visited));
  BlockID queue[MAX_BLOCKS];
  int head = 0, tail = 0;

  for (uint32_t i = 0; i < fn->n_blocks; i++)
    fn->blocks[i]->reachable = false;

  queue[tail++] = fn->entry;
  visited[fn->entry] = true;

  while (head < tail) {
    BlockID bid = queue[head++];
    Block *blk = fn->blocks[bid];
    blk->reachable = true;

    for (uint32_t si = 0; si < blk->n_succs; si++) {
      BlockID sid = blk->succs[si];
      if (sid < fn->n_blocks && !visited[sid]) {
        visited[sid] = true;
        queue[tail++] = sid;
      }
    }
  }
}

/* ─────────────────────────────────────────────────────────────────────────────
 * PHASE B: MINIMAL AST → IR BRIDGE
 *
 * One function, scalars only (int, pointers). Build Function* from a minimal
 * AST; run existing passes; no IR→asm. Validates CFG, DCE, LICM on real C-like
 * flow.
 * ───────────────────────────────────────────────────────────────────────────
 */

#define AST_MAX_VARS                                                           \
  1024 /* main() and large functions have many locals; was 16 (silent overflow \
          → corruption) */
#define AST_MAX_STMTS 64

typedef struct ASTNode ASTNode;
struct ASTNode {
  int kind; /* AST_NUM, AST_VAR, AST_ADD, AST_ASSIGN, AST_LT, AST_IF, AST_WHILE,
               AST_RETURN, AST_BLOCK */
  int64_t num_val;
  char var_name[NAME_LEN];
  ASTNode *lhs;
  ASTNode *rhs;
  ASTNode *cond;
  ASTNode *then_body;
  ASTNode *else_body;
  ASTNode *body;
  ASTNode **stmts;
  uint32_t n_stmts;
};

enum {
  AST_NUM = 1,
  AST_VAR,
  AST_ADD,
  AST_SUB,
  AST_LT,
  AST_LE,
  AST_ASSIGN,
  AST_IF,
  AST_WHILE,
  AST_RETURN,
  AST_BLOCK
};

#define MAX_NESTED_LOOPS 16

typedef struct {
  Function *fn;
  BlockID cur_block;
  RegID next_reg;
  InstrID next_instr_id;
  /* var name → slot index; slot_alloca_reg[slot] = reg holding alloca result */
  char var_names[AST_MAX_VARS][NAME_LEN];
  RegID slot_alloca_reg[AST_MAX_VARS];
  Instr *slot_alloca_instr[AST_MAX_VARS];
  uint32_t n_vars;
  int want_address; /* 1 when lowering lhs of assign/compound_assign: return
                       address reg, no load */
  /* break/continue: current loop exit and latch (continue target) */
  BlockID loop_exit_stack[MAX_NESTED_LOOPS];
  BlockID loop_latch_stack[MAX_NESTED_LOOPS];
  int loop_depth;
} LowerCtx;

static RegID lower_expr(LowerCtx *ctx, ASTNode *ast);
static void lower_stmt(LowerCtx *ctx, ASTNode *ast);

static BlockID new_block(LowerCtx *ctx, const char *name) {
  Function *fn = ctx->fn;
  if (fn->n_blocks >= MAX_BLOCKS) {
    fprintf(stderr, "FATAL: MAX_BLOCKS (%d) exceeded in %s\n", MAX_BLOCKS,
            name);
    exit(1);
  }
  BlockID id = (BlockID)fn->n_blocks;
  fn->blocks[id] = calloc(1, sizeof(Block));
  Block *b = fn->blocks[id];
  b->id = id;
  strncpy(b->name, name, NAME_LEN - 1);
  b->reachable = true;
  fn->n_blocks++;
  return id;
}

static void emit_instr(LowerCtx *ctx, Instr *ins) {
  Block *blk = ctx->fn->blocks[ctx->cur_block];
  if (!blk->head)
    blk->head = blk->tail = ins;
  else {
    blk->tail->next = ins;
    ins->prev = blk->tail;
    blk->tail = ins;
  }
  blk->n_instrs++;
  if (ins->dst && ins->dst < MAX_INSTRS)
    ctx->fn->def_of[ins->dst] = ins;
  if (ctx->fn->n_regs <= ins->dst)
    ctx->fn->n_regs = ins->dst + 1;
}

static Instr *make_instr_imm(InstrID id, Opcode op, RegID dst, int64_t imm_val,
                             int line_no) {
  Instr *ins = calloc(1, sizeof(Instr));
  ins->id = id;
  ins->op = op;
  ins->dst = dst;
  ins->imm = imm_val;
  ins->exec_freq = 1.0;
  ins->line_no = line_no;
  return ins;
}

/* Root cause: param allocas must be created in the entry block so they get the
 * first num_params slots (-8,-16,...) filled by the prologue. Otherwise first
 * use in a later block (e.g. argv in for.body) creates the alloca there and it
 * stays 0. */
static int is_main_func(const char *func_name) {
  return func_name && func_name[0] == 'm' && func_name[1] == 'a' &&
         func_name[2] == 'i' && func_name[3] == 'n' && func_name[4] == '\0';
}

/* Lazy OP_ALLOCA trap: if the first use of a variable is inside a
 * conditional/loop body, the alloca was emitted there and never runs on other
 * paths → uninitialized slot → null deref. Fix: always hoist OP_ALLOCA to the
 * entry block so it dominates all uses. */
static RegID get_or_create_var(LowerCtx *ctx, const char *name, int size) {
  if (!name || (uintptr_t)name < 4096 || name[0] == '\0') {
    fprintf(stderr, "[ZCC-IR] DEBUG: get_or_create_var EMPTY OR NULL\n");
    return 0;
  }
  fprintf(stderr, "[ZCC-IR] DEBUG: get_or_create_var('%s')\n", name);
  for (uint32_t i = 0; i < ctx->n_vars; i++) {
    if (strcmp(ctx->var_names[i], name) == 0) {
      if (size > (int)ctx->slot_alloca_instr[i]->imm) {
        ctx->slot_alloca_instr[i]->imm = size;
      }
      return ctx->slot_alloca_reg[i];
    }
  }

  if (ctx->n_vars >= AST_MAX_VARS) {
    fprintf(stderr, "FATAL: AST_MAX_VARS exceeded\n");
    return 0;
  }

  uint32_t slot = ctx->n_vars++;
  strncpy(ctx->var_names[slot], name, NAME_LEN - 1);
  ctx->var_names[slot][NAME_LEN - 1] = '\0';
  RegID r = ctx->next_reg++;
  ctx->slot_alloca_reg[slot] = r;

  Instr *alloca = make_instr_imm(ctx->next_instr_id++, OP_ALLOCA, r, size > 0 ? size : 8, 0);
  ctx->slot_alloca_instr[slot] = alloca;

  /* Hoist OP_ALLOCA to the entry block so it dominates all uses. */
  Block *entry = ctx->fn->blocks[ctx->fn->entry];
  Instr *pt = entry->tail;

  /* Skip backward past terminal branches to find a valid insertion point */
  while (pt && (pt->op == OP_BR || pt->op == OP_CONDBR || pt->op == OP_RET)) {
    pt = pt->prev;
  }

  if (pt) {
    alloca->prev = pt;
    alloca->next = pt->next;
    if (pt->next)
      pt->next->prev = alloca;
    else
      entry->tail = alloca;
    pt->next = alloca;
  } else {
    alloca->next = entry->head;
    if (entry->head)
      entry->head->prev = alloca;
    else
      entry->tail = alloca;
    entry->head = alloca;
  }
  entry->n_instrs++;

  if (alloca->dst && alloca->dst < MAX_INSTRS)
    ctx->fn->def_of[alloca->dst] = alloca;
  if (ctx->fn->n_regs <= alloca->dst)
    ctx->fn->n_regs = alloca->dst + 1;

  return r;
}

static RegID lower_expr(LowerCtx *ctx, ASTNode *ast) {
  if (!ast)
    return 0;
  RegID r = ctx->next_reg++;
  Instr *ins = NULL;
  switch (ast->kind) {
  case AST_NUM: {
    ins = make_instr_imm(ctx->next_instr_id++, OP_CONST, r, ast->num_val, 0);
    break;
  }
  case AST_VAR: {
    RegID alloca_r = get_or_create_var(ctx, ast->var_name, 8);
    ins = calloc(1, sizeof(Instr));
    ins->id = ctx->next_instr_id++;
    ins->op = OP_LOAD;
    ins->dst = r;
    ins->src[0] = alloca_r;
    ins->n_src = 1;
    ins->exec_freq = 1.0;
    break;
  }
  case AST_ADD: {
    RegID l = lower_expr(ctx, ast->lhs);
    RegID rh = lower_expr(ctx, ast->rhs);
    ins = calloc(1, sizeof(Instr));
    ins->id = ctx->next_instr_id++;
    ins->op = OP_ADD;
    ins->dst = r;
    ins->src[0] = l;
    ins->src[1] = rh;
    ins->n_src = 2;
    ins->exec_freq = 1.0;
    break;
  }
  case AST_SUB: {
    RegID l = lower_expr(ctx, ast->lhs);
    RegID rh = lower_expr(ctx, ast->rhs);
    ins = calloc(1, sizeof(Instr));
    ins->id = ctx->next_instr_id++;
    ins->op = OP_SUB;
    ins->dst = r;
    ins->src[0] = l;
    ins->src[1] = rh;
    ins->n_src = 2;
    ins->exec_freq = 1.0;
    break;
  }
  case AST_LT: {
    RegID l = lower_expr(ctx, ast->lhs);
    RegID rh = lower_expr(ctx, ast->rhs);
    ins = calloc(1, sizeof(Instr));
    ins->id = ctx->next_instr_id++;
    ins->op = OP_LT;
    ins->dst = r;
    ins->src[0] = l;
    ins->src[1] = rh;
    ins->n_src = 2;
    ins->exec_freq = 1.0;
    break;
  }
  default:
    return 0;
  }
  if (ins) {
    emit_instr(ctx, ins);
    return r;
  }
  return 0;
}

static void lower_stmt(LowerCtx *ctx, ASTNode *ast) {
  if (!ast)
    return;
  Function *fn = ctx->fn;
  switch (ast->kind) {
  case AST_ASSIGN: {
    RegID alloca_r = get_or_create_var(ctx, ast->lhs->var_name, 8);
    RegID val_r = lower_expr(ctx, ast->rhs);
    Instr *st = calloc(1, sizeof(Instr));
    st->id = ctx->next_instr_id++;
    st->op = OP_STORE;
    st->dst = 0;
    st->src[0] = val_r;
    st->src[1] = alloca_r;
    st->n_src = 2;
    st->exec_freq = 1.0;
    emit_instr(ctx, st);
    return;
  }
  case AST_IF: {
    RegID cond_r = lower_expr(ctx, ast->cond);
    BlockID then_blk = new_block(ctx, "if.then");
    BlockID else_blk = new_block(ctx, "if.else");
    BlockID merge_blk = new_block(ctx, "if.merge");
    Block *cur = fn->blocks[ctx->cur_block];
    Instr *cbr = calloc(1, sizeof(Instr));
    cbr->id = ctx->next_instr_id++;
    cbr->op = OP_CONDBR;
    cbr->dst = 0;
    cbr->src[0] = cond_r;
    cbr->src[1] = then_blk;
    cbr->src[2] = else_blk;
    cbr->n_src = 3;
    cbr->exec_freq = 1.0;
    emit_instr(ctx, cbr);
    cur->succs[0] = then_blk;
    cur->succs[1] = else_blk;
    cur->n_succs = 2;
    cur->branch_prob[0] = 0.5;
    cur->branch_prob[1] = 0.5;
    fn->blocks[then_blk]->preds[0] = ctx->cur_block;
    fn->blocks[then_blk]->n_preds = 1;
    fn->blocks[else_blk]->preds[0] = ctx->cur_block;
    fn->blocks[else_blk]->n_preds = 1;

    ctx->cur_block = then_blk;
    lower_stmt(ctx, ast->then_body);
    {
      Block *_tb = fn->blocks[ctx->cur_block];
      Instr *_tt = _tb ? _tb->tail : NULL;
      if (!(_tt &&
            (_tt->op == OP_RET || _tt->op == OP_BR || _tt->op == OP_CONDBR))) {
        Instr *br_then = calloc(1, sizeof(Instr));
        br_then->id = ctx->next_instr_id++;
        br_then->op = OP_BR;
        br_then->src[0] = merge_blk;
        br_then->n_src = 1;
        br_then->exec_freq = 1.0;
        emit_instr(ctx, br_then);
        fn->blocks[ctx->cur_block]->succs[0] = merge_blk;
        fn->blocks[ctx->cur_block]->n_succs = 1;
        fn->blocks[merge_blk]->preds[fn->blocks[merge_blk]->n_preds++] =
            ctx->cur_block;
      }
    }
    fn->blocks[merge_blk]->n_preds++;

    ctx->cur_block = else_blk;
    if (ast->else_body)
      lower_stmt(ctx, ast->else_body);
    {
      Block *_eb = fn->blocks[ctx->cur_block];
      Instr *_et = _eb ? _eb->tail : NULL;
      if (!(_et &&
            (_et->op == OP_RET || _et->op == OP_BR || _et->op == OP_CONDBR))) {
        Instr *br_else = calloc(1, sizeof(Instr));
        br_else->id = ctx->next_instr_id++;
        br_else->op = OP_BR;
        br_else->src[0] = merge_blk;
        br_else->n_src = 1;
        br_else->exec_freq = 1.0;
        emit_instr(ctx, br_else);
        fn->blocks[ctx->cur_block]->succs[0] = merge_blk;
        fn->blocks[ctx->cur_block]->n_succs = 1;
        fn->blocks[merge_blk]->preds[fn->blocks[merge_blk]->n_preds++] =
            ctx->cur_block;
      }
    }
    fn->blocks[merge_blk]->n_preds++;

    ctx->cur_block = merge_blk;
    return;
  }
  case AST_WHILE: {
    BlockID head = new_block(ctx, "while.head");
    BlockID body_blk = new_block(ctx, "while.body");
    BlockID exit_blk = new_block(ctx, "while.exit");
    Block *cur = fn->blocks[ctx->cur_block];
    Instr *br = calloc(1, sizeof(Instr));
    br->id = ctx->next_instr_id++;
    br->op = OP_BR;
    br->src[0] = head;
    br->n_src = 1;
    br->exec_freq = 1.0;
    emit_instr(ctx, br);
    cur->succs[0] = head;
    cur->n_succs = 1;
    fn->blocks[head]->preds[0] = ctx->cur_block;
    fn->blocks[head]->n_preds = 1;

    ctx->cur_block = head;
    RegID cond_r = lower_expr(ctx, ast->cond);
    Instr *cbr = calloc(1, sizeof(Instr));
    cbr->id = ctx->next_instr_id++;
    cbr->op = OP_CONDBR;
    cbr->dst = 0;
    cbr->src[0] = cond_r;
    cbr->src[1] = body_blk;
    cbr->src[2] = exit_blk;
    cbr->n_src = 3;
    cbr->exec_freq = 1.0;
    emit_instr(ctx, cbr);
    fn->blocks[head]->succs[0] = body_blk;
    fn->blocks[head]->succs[1] = exit_blk;
    fn->blocks[head]->n_succs = 2;
    fn->blocks[head]->branch_prob[0] = 0.9;
    fn->blocks[head]->branch_prob[1] = 0.1;
    fn->blocks[body_blk]->preds[0] = head;
    fn->blocks[body_blk]->n_preds = 1;
    fn->blocks[exit_blk]->preds[0] = head;
    fn->blocks[exit_blk]->n_preds = 1;

    ctx->cur_block = body_blk;
    lower_stmt(ctx, ast->body);
    Instr *br_back = calloc(1, sizeof(Instr));
    br_back->id = ctx->next_instr_id++;
    br_back->op = OP_BR;
    br_back->src[0] = head;
    br_back->n_src = 1;
    br_back->exec_freq = 1.0;
    emit_instr(ctx, br_back);
    fn->blocks[body_blk]->succs[0] = head;
    fn->blocks[body_blk]->n_succs = 1;
    fn->blocks[head]->preds[fn->blocks[head]->n_preds] = body_blk;
    fn->blocks[head]->n_preds++;

    ctx->cur_block = exit_blk;
    return;
  }
  case AST_RETURN: {
    RegID val_r = lower_expr(ctx, ast->lhs);
    Instr *ret = calloc(1, sizeof(Instr));
    ret->id = ctx->next_instr_id++;
    ret->op = OP_RET;
    ret->dst = 0;
    ret->src[0] = val_r;
    ret->n_src = 1;
    ret->exec_freq = 1.0;
    emit_instr(ctx, ret);
    fn->blocks[ctx->cur_block]->n_succs = 0;
    return;
  }
  case AST_BLOCK:
    for (uint32_t i = 0; i < ast->n_stmts; i++)
      lower_stmt(ctx, ast->stmts[i]);
    return;
  default:
    return;
  }
}

/* ─────────────────────────────────────────────────────────────────────────────
 * ZCC AST BRIDGE — Lower ZCC-parsed function (scalars + control flow) to IR.
 * ZCCNode and ZND_* come from zcc_ast_bridge.h. Copy from Node* via accessors.
 * Standalone build: cc -DZCC_BRIDGE_STANDALONE -o passes compiler_passes.c -lm
 * ───────────────────────────────────────────────────────────────────────────
 */

#ifdef ZCC_BRIDGE_STANDALONE
/* Stub accessors when building passes alone (no zcc.c). Link with
 * -DZCC_BRIDGE_STANDALONE. */
/* CG-IR-012b: ALL Node/Type accessor stubs replaced with extern declarations.
 * The real implementations live in zcc.c (part1.c). The stubs here returned
 * dummy values (0/NULL/""), causing the AST-to-IR conversion to lose all
 * structural information — func names, args, children, offsets, etc. */
extern int node_kind(struct Node *n);
extern long long node_int_val(struct Node *n);
extern int node_str_id(struct Node *n);
extern void node_name(struct Node *n, char *buf, unsigned len);
extern int node_is_global(struct Node *n);
extern int node_is_array(struct Node *n);
extern int node_is_func(struct Node *n);
extern struct Node *node_lhs(struct Node *n);
extern struct Node *node_rhs(struct Node *n);
extern struct Node *node_cond(struct Node *n);
extern struct Node *node_then_body(struct Node *n);
extern struct Node *node_else_body(struct Node *n);
extern struct Node *node_body(struct Node *n);
extern struct Node *node_init(struct Node *n);
extern struct Node *node_inc(struct Node *n);
extern struct Node **node_cases(struct Node *n);
extern int node_num_cases(struct Node *n);
extern struct Node *node_default_case(struct Node *n);
extern long long node_case_val(struct Node *n);
extern struct Node *node_case_body(struct Node *n);
extern int node_member_offset(struct Node *n);
extern int node_member_size(struct Node *n);
extern int node_line_no(struct Node *n);
extern int node_compound_op(struct Node *n);
extern struct Node **node_stmts(struct Node *n);
extern int node_num_stmts(struct Node *n);
extern const char *node_func_name(struct Node *n);
extern const char *node_asm_string(struct Node *n);
extern struct Node *node_arg(struct Node *n, int i);
extern int node_num_args(struct Node *n);
extern int node_ptr_elem_size(struct Node *n);
extern int node_lhs_ptr_size(struct Node *n);
extern int node_rhs_ptr_size(struct Node *n);
#endif /* ZCC_BRIDGE_STANDALONE */

extern int node_ptr_elem_size(struct Node *n);

/* CG-IR-015: always-present declarations (not guarded by STANDALONE) */
extern int node_type_size(struct Node *n);
extern int node_type_unsigned(struct Node *n);

/* Map ND_* (zcc.c) to ZND_* (bridge). Use sentinels from zcc_ast_bridge.h. */
static int nd_to_znd(int nd_kind) {
  switch (nd_kind) {
  case ZCC_ND_NUM:
    return ZND_NUM;
  case ZCC_ND_STR:
    return ZND_STR;
  case ZCC_ND_VAR:
    return ZND_VAR;
  case ZCC_ND_ASSIGN:
    return ZND_ASSIGN;
  case ZCC_ND_ADD:
    return ZND_ADD;
  case ZCC_ND_SUB:
    return ZND_SUB;
  case ZCC_ND_MOD:
    return ZND_MOD;
  case ZCC_ND_MUL:
    return ZND_MUL;
  case ZCC_ND_DIV:
    return ZND_DIV;
  case ZCC_ND_NEG:
    return ZND_NEG;
  case ZCC_ND_LAND:
    return ZND_LAND;
  case ZCC_ND_LOR:
    return ZND_LOR;
  case ZCC_ND_LNOT:
    return ZND_LNOT;
  case ZCC_ND_BOR:
    return ZND_BOR;
  case ZCC_ND_BXOR:
    return ZND_BXOR;
  case ZCC_ND_BNOT:
    return ZND_BNOT;
  case ZCC_ND_TERNARY:
    return ZND_TERNARY;
  case ZCC_ND_BAND:
    return ZND_BAND;
  case ZCC_ND_SHL:
    return ZND_SHL;
  case ZCC_ND_SHR:
    return ZND_SHR;
  case ZCC_ND_LT:
    return ZND_LT;
  case ZCC_ND_LE:
    return ZND_LE;
  case ZCC_ND_GT:
    return ZND_GT;
  case ZCC_ND_GE:
    return ZND_GE;
  case ZCC_ND_EQ:
    return ZND_EQ;
  case ZCC_ND_NE:
    return ZND_NE;
  case ZCC_ND_IF:
    return ZND_IF;
  case ZCC_ND_WHILE:
    return ZND_WHILE;
  case ZCC_ND_FOR:
    return ZND_FOR;
  case ZCC_ND_BREAK:
    return ZND_BREAK;
  case ZCC_ND_CONTINUE:
    return ZND_CONTINUE;
  case ZCC_ND_RETURN:
    return ZND_RETURN;
  case ZCC_ND_BLOCK:
    return ZND_BLOCK;
  case ZCC_ND_CAST:
    return ZND_CAST;
  case ZCC_ND_CALL:
    return ZND_CALL;
  case ZCC_ND_NOP:
    return ZND_NOP;
  case ZCC_ND_POST_INC:
    return ZND_POST_INC;
  case ZCC_ND_POST_DEC:
    return ZND_POST_DEC;
  case ZCC_ND_PRE_INC:
    return ZND_PRE_INC;
  case ZCC_ND_PRE_DEC:
    return ZND_PRE_DEC;
  case ZCC_ND_SIZEOF:
    return ZND_SIZEOF;
  case ZCC_ND_COMPOUND_ASSIGN:
    return ZND_COMPOUND_ASSIGN;
  case ZCC_ND_ADDR:
    return ZND_ADDR;
  case ZCC_ND_DEREF:
    return ZND_DEREF;
  case ZCC_ND_MEMBER:
    return ZND_MEMBER;
  case ZCC_ND_SWITCH:
    return ZND_SWITCH;
  case ZCC_ND_ASM:
    return ZND_ASM;
  default:
    return -1;
  }
}

static ZCCNode *zcc_node_from_expr(struct Node *n);
static ZCCNode *zcc_node_from_stmt(struct Node *n);

/* Per-function if id; reset in zcc_node_from, incremented in zcc_node_from_stmt
 * for ZND_IF. */
static int if_counter;

static ZCCNode *alloc_zcc_node(void) {
  ZCCNode *z = calloc(1, sizeof(ZCCNode));
  return z;
}

static ZCCNode *zcc_node_from_expr(struct Node *n) {
  if (!n)
    return NULL;
  int zk = nd_to_znd(node_kind(n));
  if (zk < 0) {
    fprintf(stderr, "zcc_node_from: unsupported expr kind %d\n", node_kind(n));
    return NULL;
  }
  ZCCNode *z = alloc_zcc_node();
  z->kind = zk;
  z->line_no = node_line_no(n);
  switch (zk) {
  case ZND_NUM:
    z->int_val = node_int_val(n);
    break;
  case ZND_STR:
    z->int_val = (int64_t)node_str_id(n);
    break;
  case ZND_VAR:
    node_name(n, z->name, ZCC_BRIDGE_NAME_LEN - 1);
    z->is_global = node_is_global(n);
    z->is_array = node_is_array(n);
    z->is_func = node_is_func(n);
    z->member_size = node_member_size(n);
    if (z->member_size <= 0)
      z->member_size = 8;
    break;
  case ZND_ADD: {
    z->lhs = zcc_node_from_expr(node_lhs(n));
    z->rhs = zcc_node_from_expr(node_rhs(n));
    int l_esz = node_ptr_elem_size(node_lhs(n));
    int r_esz = node_ptr_elem_size(node_rhs(n));
    if (l_esz > 1) {
      ZCCNode *mul = alloc_zcc_node();
      mul->kind = ZND_MUL;
      mul->lhs = z->rhs;
      ZCCNode *imm = alloc_zcc_node();
      imm->kind = ZND_NUM;
      imm->int_val = l_esz;
      mul->rhs = imm;
      z->rhs = mul;
    } else if (r_esz > 1) {
      ZCCNode *mul = alloc_zcc_node();
      mul->kind = ZND_MUL;
      mul->lhs = z->lhs;
      ZCCNode *imm = alloc_zcc_node();
      imm->kind = ZND_NUM;
      imm->int_val = r_esz;
      mul->rhs = imm;
      z->lhs = mul;
    }
    break;
  }
  case ZND_SUB: {
    z->lhs = zcc_node_from_expr(node_lhs(n));
    z->rhs = zcc_node_from_expr(node_rhs(n));
    int l_esz = node_ptr_elem_size(node_lhs(n));
    int r_esz = node_ptr_elem_size(node_rhs(n));
    if (l_esz > 1) {
      if (r_esz > 1) {
        ZCCNode *sub = alloc_zcc_node();
        sub->kind = ZND_SUB;
        sub->lhs = z->lhs;
        sub->rhs = z->rhs;
        ZCCNode *imm = alloc_zcc_node();
        imm->kind = ZND_NUM;
        imm->int_val = l_esz;
        z->kind = ZND_DIV;
        z->lhs = sub;
        z->rhs = imm;
      } else {
        ZCCNode *mul = alloc_zcc_node();
        mul->kind = ZND_MUL;
        mul->lhs = z->rhs;
        ZCCNode *imm = alloc_zcc_node();
        imm->kind = ZND_NUM;
        imm->int_val = l_esz;
        mul->rhs = imm;
        z->rhs = mul;
      }
    }
    break;
  }
  /* CG-IR-015: DIV/MOD/SHR/SHL need the node's result type to select
   * correct 32-bit vs 64-bit instructions in ir_asm_lower_insn.
   * Encoding: member_size > 0 = signed (abs = byte width),
   *           member_size < 0 = unsigned (abs = byte width),
   *           member_size = 0 = unknown/default (→ IR_TY_I64 in lowering). */
  case ZND_DIV:
  case ZND_MOD:
  case ZND_SHR:
  case ZND_SHL:
    z->lhs = zcc_node_from_expr(node_lhs(n));
    z->rhs = zcc_node_from_expr(node_rhs(n));
    {
      int ts = node_type_size(n);      /* 4 for int, 8 for long/ptr         */
      int tu = node_type_unsigned(n);  /* 1 if unsigned                     */
      /* member_size is unused for arithmetic nodes; repurpose as type tag. */
      z->member_size = tu ? -(ts) : ts;
    }
    break;
  case ZND_MUL:
  case ZND_BAND:
  case ZND_BOR:
  case ZND_BXOR:
  case ZND_LT:
  case ZND_LE:
  case ZND_GT:
  case ZND_GE:
  case ZND_EQ:
  case ZND_NE:
  case ZND_LAND:
  case ZND_LOR:
    z->lhs = zcc_node_from_expr(node_lhs(n));
    z->rhs = zcc_node_from_expr(node_rhs(n));
    break;
  case ZND_NEG:
  case ZND_BNOT:
  case ZND_LNOT:
    z->lhs = zcc_node_from_expr(node_lhs(n));
    break;
  case ZND_TERNARY:
    z->cond = zcc_node_from_expr(node_cond(n));
    z->then_body = zcc_node_from_expr(node_then_body(n));
    z->else_body = zcc_node_from_expr(node_else_body(n));
    break;
  case ZND_CAST:
    z->lhs = zcc_node_from_expr(node_lhs(n));
    break;
  case ZND_ADDR:
  case ZND_DEREF:
    z->lhs = zcc_node_from_expr(node_lhs(n));
    if (zk == ZND_DEREF) {
      z->member_size = node_member_size(n);
      if (z->member_size <= 0)
        z->member_size = 8;
    }
    break;
  case ZND_MEMBER:
    z->lhs = zcc_node_from_expr(node_lhs(n));
    z->member_offset = node_member_offset(n);
    z->member_size = node_member_size(n);
    z->is_bitfield = node_is_bitfield(n);
    z->bit_offset = node_bit_offset(n);
    z->bit_size = node_bit_size(n);
    z->is_array = node_is_array(n);
    z->is_func = node_is_func(n);
    if (z->member_size <= 0)
      z->member_size = 8;
    break;
  case ZND_POST_INC:
  case ZND_POST_DEC:
  case ZND_PRE_INC:
  case ZND_PRE_DEC:
    z->lhs = zcc_node_from_expr(node_lhs(n));
    z->int_val = node_ptr_elem_size(node_lhs(n));
    if (z->int_val <= 0)
      z->int_val = 1;
    break;
  case ZND_SIZEOF:
    z->int_val = node_int_val(n);
    break;
  case ZND_ASSIGN:
    z->lhs = zcc_node_from_expr(node_lhs(n));
    z->rhs = zcc_node_from_expr(node_rhs(n));
    break;
  case ZND_COMPOUND_ASSIGN: {
    z->lhs = zcc_node_from_expr(node_lhs(n));
    z->rhs = zcc_node_from_expr(node_rhs(n));
    z->compound_op = nd_to_znd(node_compound_op(n));
    int l_esz = node_ptr_elem_size(node_lhs(n));
    if (l_esz > 1 && (z->compound_op == ZND_ADD || z->compound_op == ZND_SUB)) {
      ZCCNode *mul = alloc_zcc_node();
      mul->kind = ZND_MUL;
      mul->lhs = z->rhs;
      ZCCNode *imm = alloc_zcc_node();
      imm->kind = ZND_NUM;
      imm->int_val = l_esz;
      mul->rhs = imm;
      z->rhs = mul;
    }
    break;
  }
  case ZND_CALL: {
    const char *fn = node_func_name(n);
    if (fn)
      strncpy(z->func_name, fn, ZCC_CALL_NAME_LEN - 1);
    z->func_name[ZCC_CALL_NAME_LEN - 1] = '\0';
    int na = node_num_args(n);
    if (na < 0)
      na = 0;
    if (na > ZCC_MAX_CALL_ARGS)
      na = ZCC_MAX_CALL_ARGS;
    z->num_args = na;
    if (na > 0) {
      ZCCNode **out = calloc((size_t)na, sizeof(ZCCNode *));
      if (out) {
        for (int i = 0; i < na; i++)
          out[i] = zcc_node_from_expr(node_arg(n, i));
        z->args = out;
      }
    }
    break;
  }
  default:
    break;
  }
  return z;
}

static ZCCNode *zcc_node_from_stmt(struct Node *n) {
  if (!n)
    return NULL;
  int nk = node_kind(n);
  if (nk == 110) { /* ND_RSP_SAVE */
      ZCCNode *z = alloc_zcc_node();
      z->kind = ZND_ASM;
      z->line_no = node_line_no(n);
      int off = node_member_offset(n);
      char buf[64];
      if (off > 0) off = -off;
      sprintf(buf, "movq %%rsp, %d(%%rbp)", off);
      z->asm_string = strdup(buf);
      return z;
  }
  if (nk == 111) { /* ND_RSP_RESTORE */
      ZCCNode *z = alloc_zcc_node();
      z->kind = ZND_ASM;
      z->line_no = node_line_no(n);
      int off = node_member_offset(n);
      char buf[64];
      if (off > 0) off = -off;
      sprintf(buf, "movq %d(%%rbp), %%rsp", off);
      z->asm_string = strdup(buf);
      return z;
  }
  int zk = nd_to_znd(nk);
  if (zk < 0) {
    fprintf(stderr, "zcc_node_from: unsupported stmt kind %d\n", nk);
    return NULL;
  }
  ZCCNode *z = alloc_zcc_node();
  z->kind = zk;
  z->line_no = node_line_no(n);
  switch (zk) {
  case ZND_ASSIGN:
    z->lhs = zcc_node_from_expr(node_lhs(n));
    z->rhs = zcc_node_from_expr(node_rhs(n));
    break;
  case ZND_IF:
    z->cond = zcc_node_from_expr(node_cond(n));
    z->then_body = zcc_node_from_stmt(node_then_body(n));
    z->else_body = zcc_node_from_stmt(node_else_body(n));
    z->if_id = if_counter++;
    break;
  case ZND_WHILE:
    z->cond = zcc_node_from_expr(node_cond(n));
    z->body = zcc_node_from_stmt(node_body(n));
    break;
  case ZND_FOR:
    z->init = node_init(n) ? zcc_node_from_stmt(node_init(n)) : NULL;
    z->cond = node_cond(n) ? zcc_node_from_expr(node_cond(n)) : NULL;
    z->inc = node_inc(n) ? zcc_node_from_expr(node_inc(n)) : NULL;
    z->body = zcc_node_from_stmt(node_body(n));
    break;
  case ZND_BREAK:
  case ZND_CONTINUE:
    break;
  case ZND_RETURN:
    z->lhs = zcc_node_from_expr(node_lhs(n));
    break;
  case ZND_COMPOUND_ASSIGN: {
    z->lhs = zcc_node_from_expr(node_lhs(n));
    z->rhs = zcc_node_from_expr(node_rhs(n));
    z->compound_op = nd_to_znd(node_compound_op(n));
    int l_esz = node_ptr_elem_size(node_lhs(n));
    if (l_esz > 1 && (z->compound_op == ZND_ADD || z->compound_op == ZND_SUB)) {
      ZCCNode *mul = alloc_zcc_node();
      mul->kind = ZND_MUL;
      mul->lhs = z->rhs;
      ZCCNode *imm = alloc_zcc_node();
      imm->kind = ZND_NUM;
      imm->int_val = l_esz;
      mul->rhs = imm;
      z->rhs = mul;
    }
    break;
  }
  case ZND_BLOCK: {
    int num = node_num_stmts(n);
    struct Node **stmts = node_stmts(n);
    if (num <= 0 || num > (int)ZCC_AST_MAX_STMTS || !stmts)
      break;
    ZCCNode **out = calloc((size_t)num, sizeof(ZCCNode *));
    if (!out)
      break;
    for (int i = 0; i < num; i++)
      out[i] = zcc_node_from_stmt(stmts[i]);
    z->stmts = out;
    z->num_stmts = (uint32_t)num;
    break;
  }
  case ZND_CALL:
    /* CG-IR-013: Statement-level calls (e.g. validate_node(...);) must
     * populate func_name and args. Delegate to the expr handler which
     * already has the complete ZND_CALL conversion logic. */
    {
      ZCCNode *call_node = zcc_node_from_expr(n);
      if (call_node) { free(z); return call_node; }
    }
    break;
  /* CG-IR-015: Expression-statements (i++, --p, etc.) in stmt position */
  case ZND_POST_INC:
  case ZND_PRE_INC:
  case ZND_POST_DEC:
  case ZND_PRE_DEC:
  {
    ZCCNode *expr_node = zcc_node_from_expr(n);
    if (expr_node) { free(z); return expr_node; }
  }
  break;
  case ZND_NOP:
    break;
  case ZND_ASM:
    z->asm_string = (char *)node_asm_string(n);
    break;
  case ZND_SWITCH: {
    int num = node_num_cases(n);
    struct Node **cases = node_cases(n);
    z->cond = node_cond(n) ? zcc_node_from_expr(node_cond(n)) : NULL;
    if (num <= 0 || num > 128 || !cases) {
      z->num_cases = 0;
      z->case_vals = NULL;
      z->case_bodies = NULL;
    } else {
      z->num_cases = num;
      z->case_vals = calloc((size_t)num, sizeof(int64_t));
      z->case_bodies = calloc((size_t)num, sizeof(ZCCNode *));
      if (z->case_vals && z->case_bodies) {
        for (int i = 0; i < num; i++) {
          struct Node *c = cases[i];
          if (c) {
            z->case_vals[i] = node_case_val(c);
            z->case_bodies[i] = zcc_node_from_stmt(node_case_body(c));
          }
        }
      }
    }
    {
      struct Node *def = node_default_case(n);
      z->default_body = def ? zcc_node_from_stmt(node_case_body(def)) : NULL;
    }
    break;
  }
  default:
    break;
  }
  return z;
}

/**
 * zcc_node_from — Deep-copy Node* tree into ZCCNode* (supported kinds only).
 * Returns a new tree; caller must zcc_node_free() it.
 */
ZCCNode *zcc_node_from(struct Node *n) {
  if (!n)
    return NULL;
  if_counter = 0; /* reset at start of each function body */
  return zcc_node_from_stmt(n);
}

/**
 * zcc_node_free — Recursively free a ZCCNode tree from zcc_node_from.
 */
void zcc_node_free(ZCCNode *z) {
  if (!z)
    return;
  zcc_node_free(z->lhs);
  zcc_node_free(z->rhs);
  zcc_node_free(z->cond);
  zcc_node_free(z->then_body);
  zcc_node_free(z->else_body);
  zcc_node_free(z->body);
  zcc_node_free(z->init);
  zcc_node_free(z->inc);
  if (z->case_bodies) {
    for (int i = 0; i < z->num_cases; i++)
      zcc_node_free(z->case_bodies[i]);
    free(z->case_bodies);
  }
  if (z->case_vals)
    free(z->case_vals);
  zcc_node_free(z->default_body);
  if (z->stmts) {
    for (uint32_t i = 0; i < z->num_stmts; i++)
      zcc_node_free(z->stmts[i]);
    free(z->stmts);
  }
  if (z->args) {
    for (int i = 0; i < z->num_args; i++)
      zcc_node_free(z->args[i]);
    free(z->args);
  }
  free(z);
}

static RegID zcc_lower_expr(LowerCtx *ctx, ZCCNode *node);
static void zcc_lower_stmt(LowerCtx *ctx, ZCCNode *node);

/* CG-IR-015: decode the member_size type-tag set by zcc_node_from_expr for
 * DIV/MOD/SHR/SHL nodes.  Convention:
 *   member_size ==  4  →  IR_TY_I32   (signed int)
 *   member_size == -4  →  IR_TY_U32   (unsigned int)
 *   member_size == -8  →  IR_TY_U64   (unsigned long)
 *   otherwise          →  IR_TY_I64   (signed long / default)           */
static IRType irtype_from_node(const ZCCNode *node) {
  if (!node) return IR_TY_I64;
  int ms = node->member_size;
  if (ms ==  4) return IR_TY_I32;
  if (ms == -4) return IR_TY_U32;
  if (ms == -8) return IR_TY_U64;
  return IR_TY_I64;
}

static RegID zcc_lower_expr(LowerCtx *ctx, ZCCNode *node) {
  if (!node)
    return 0;
  /* Transparent passthrough: only recurse on operand. */
  if (node->kind == ZND_CAST)
    return node->lhs ? zcc_lower_expr(ctx, node->lhs) : 0;
  /* ZND_ADDR: force child to yield address; no extra IR. */
  if (node->kind == ZND_ADDR) {
    int old = ctx->want_address;
    ctx->want_address = 1;
    RegID addr_r = node->lhs ? zcc_lower_expr(ctx, node->lhs) : 0;
    ctx->want_address = old;
    return addr_r;
  }
  /* ZND_DEREF: evaluate pointer; if want_address return it, else OP_LOAD. */
  if (node->kind == ZND_DEREF) {
    int old = ctx->want_address;
    ctx->want_address = 0;
    RegID ptr_r = node->lhs ? zcc_lower_expr(ctx, node->lhs) : 0;
    ctx->want_address = old;
    if (!ptr_r)
      return 0;
    if (ctx->want_address)
      return ptr_r;
    RegID val_r = ctx->next_reg++;
    Instr *load_ins = calloc(1, sizeof(Instr));
    load_ins->id = ctx->next_instr_id++;
    load_ins->op = OP_LOAD;
    load_ins->dst = val_r;
    load_ins->src[0] = ptr_r;
    load_ins->n_src = 1;
    load_ins->imm = (node->member_size > 0) ? (int64_t)node->member_size : 8;
    load_ins->exec_freq = 1.0;
    emit_instr(ctx, load_ins);
    return val_r;
  }
  /* ZND_MEMBER: base + offset → address; then load value unless want_address.
   */
  if (node->kind == ZND_MEMBER) {
    int old = ctx->want_address;
    ctx->want_address = 1;
    RegID base_r = node->lhs ? zcc_lower_expr(ctx, node->lhs) : 0;
    ctx->want_address = old;
    if (!base_r)
      return 0;
    RegID offset_r = ctx->next_reg++;
    Instr *off_ins =
        make_instr_imm(ctx->next_instr_id++, OP_CONST, offset_r,
                       (int64_t)(node->member_offset), node->line_no);
    emit_instr(ctx, off_ins);
    RegID addr_r = ctx->next_reg++;
    Instr *add_ins = calloc(1, sizeof(Instr));
    add_ins->id = ctx->next_instr_id++;
    add_ins->op = OP_ADD;
    add_ins->dst = addr_r;
    add_ins->src[0] = base_r;
    add_ins->src[1] = offset_r;
    add_ins->n_src = 2;
    add_ins->exec_freq = 1.0;
    emit_instr(ctx, add_ins);
    if (ctx->want_address || node->is_array || node->is_func)
      return addr_r;
    RegID r = ctx->next_reg++;
    Instr *load_ins = calloc(1, sizeof(Instr));
    load_ins->id = ctx->next_instr_id++;
    load_ins->op = OP_LOAD;
    load_ins->dst = r;
    load_ins->src[0] = addr_r;
    load_ins->n_src = 1;
    load_ins->imm = (node->member_size > 0) ? (int64_t)node->member_size : 8;
    load_ins->exec_freq = 1.0;
    emit_instr(ctx, load_ins);
    
    if (node->is_bitfield) {
        fprintf(stderr, "WIRING R-VALUE BITFIELD: offset=%d, size=%d\n", node->bit_offset, node->bit_size);
        RegID bo_r = ctx->next_reg++;
        emit_instr(ctx, make_instr_imm(ctx->next_instr_id++, OP_CONST, bo_r, node->bit_offset, node->line_no));
        RegID shift_r = ctx->next_reg++;
        Instr *sh_i = calloc(1, sizeof(Instr));
        sh_i->id = ctx->next_instr_id++;
        sh_i->op = OP_SHR;
        sh_i->dst = shift_r;
        sh_i->src[0] = r;
        sh_i->src[1] = bo_r;
        sh_i->n_src = 2;
        sh_i->exec_freq = 1.0;
        emit_instr(ctx, sh_i);

        RegID mask_r = ctx->next_reg++;
        long long mask_val = (1ULL << node->bit_size) - 1;
        emit_instr(ctx, make_instr_imm(ctx->next_instr_id++, OP_CONST, mask_r, mask_val, node->line_no));
        RegID band_r = ctx->next_reg++;
        Instr *band_i = calloc(1, sizeof(Instr));
        band_i->id = ctx->next_instr_id++;
        band_i->op = OP_BAND;
        band_i->dst = band_r;
        band_i->src[0] = shift_r;
        band_i->src[1] = mask_r;
        band_i->n_src = 2;
        band_i->exec_freq = 1.0;
        emit_instr(ctx, band_i);
        r = band_r;
    }
    return r;
  }
  /* ZND_TERNARY: cond ? then_expr : else_expr. SSA: single def via OP_PHI at
   * merge. */
  if (node->kind == ZND_TERNARY) {
    RegID cond_r = node->cond ? zcc_lower_expr(ctx, node->cond) : 0;
    if (!cond_r)
      return 0;
    BlockID blk_cond = ctx->cur_block;
    BlockID blk_then = new_block(ctx, "tern.then");
    BlockID blk_else = new_block(ctx, "tern.else");
    BlockID blk_merge = new_block(ctx, "tern.merge");
    Function *fn = ctx->fn;

    Instr *cbr = calloc(1, sizeof(Instr));
    cbr->id = ctx->next_instr_id++;
    cbr->op = OP_CONDBR;
    cbr->dst = 0;
    cbr->src[0] = cond_r;
    cbr->src[1] = blk_then;
    cbr->src[2] = blk_else;
    cbr->n_src = 3;
    cbr->exec_freq = 1.0;
    emit_instr(ctx, cbr);
    fn->blocks[blk_cond]->succs[0] = blk_then;
    fn->blocks[blk_cond]->succs[1] = blk_else;
    fn->blocks[blk_cond]->n_succs = 2;
    fn->blocks[blk_cond]->branch_prob[0] = 0.5f;
    fn->blocks[blk_cond]->branch_prob[1] = 0.5f;
    fn->blocks[blk_then]->preds[fn->blocks[blk_then]->n_preds++] = blk_cond;
    fn->blocks[blk_else]->preds[fn->blocks[blk_else]->n_preds++] = blk_cond;

    RegID tern_zero = ctx->next_reg++;
    Instr *cz = make_instr_imm(ctx->next_instr_id++, OP_CONST, tern_zero, 0,
                               node->line_no);

    ctx->cur_block = blk_then;
    RegID val_then = node->then_body ? zcc_lower_expr(ctx, node->then_body) : 0;
    if (!val_then) {
      emit_instr(ctx, cz);
      val_then = tern_zero;
    }
    BlockID blk_then_end = ctx->cur_block;
    Instr *br_then = calloc(1, sizeof(Instr));
    br_then->id = ctx->next_instr_id++;
    br_then->op = OP_BR;
    br_then->src[0] = blk_merge;
    br_then->n_src = 1;
    br_then->exec_freq = 1.0;
    emit_instr(ctx, br_then);
    fn->blocks[blk_then_end]->succs[0] = blk_merge;
    fn->blocks[blk_then_end]->n_succs = 1;
    fn->blocks[blk_merge]->preds[fn->blocks[blk_merge]->n_preds++] =
        blk_then_end;

    ctx->cur_block = blk_else;
    RegID val_else = node->else_body ? zcc_lower_expr(ctx, node->else_body) : 0;
    if (!val_else) {
      emit_instr(ctx, cz);
      val_else = tern_zero;
    }
    BlockID blk_else_end = ctx->cur_block;
    Instr *br_else = calloc(1, sizeof(Instr));
    br_else->id = ctx->next_instr_id++;
    br_else->op = OP_BR;
    br_else->src[0] = blk_merge;
    br_else->n_src = 1;
    br_else->exec_freq = 1.0;
    emit_instr(ctx, br_else);
    fn->blocks[blk_else_end]->succs[0] = blk_merge;
    fn->blocks[blk_else_end]->n_succs = 1;
    fn->blocks[blk_merge]->preds[fn->blocks[blk_merge]->n_preds++] =
        blk_else_end;

    ctx->cur_block = blk_merge;
    RegID res_r = ctx->next_reg++;
    Instr *phi = calloc(1, sizeof(Instr));
    phi->id = ctx->next_instr_id++;
    phi->op = OP_PHI;
    phi->dst = res_r;
    phi->phi[0].reg = val_then;
    phi->phi[0].block = blk_then_end;
    phi->phi[1].reg = val_else;
    phi->phi[1].block = blk_else_end;
    phi->n_phi = 2;
    phi->exec_freq = 1.0;
    emit_instr(ctx, phi);
    return res_r;
  }
  /* ZND_LAND: A && B. Short-circuit: if !A go to merge with 0; else evaluate B,
   * normalize to 0/1. SSA: OP_PHI at merge. */
  if (node->kind == ZND_LAND) {
    RegID val_lhs = node->lhs ? zcc_lower_expr(ctx, node->lhs) : 0;
    if (!val_lhs)
      return 0;
    BlockID blk_lhs_end = ctx->cur_block;
    BlockID blk_bypass = new_block(ctx, "land.bypass");
    BlockID blk_rhs = new_block(ctx, "land.rhs");
    BlockID blk_merge = new_block(ctx, "land.merge");
    Function *fn = ctx->fn;

    RegID zero_r = ctx->next_reg++;
    Instr *c0 = make_instr_imm(ctx->next_instr_id++, OP_CONST, zero_r, 0,
                               node->line_no);
    emit_instr(ctx, c0); /* CG-IR-FIX: Emit before branch to dominate both RHS
                            and bypass blocks */

    Instr *cbr = calloc(1, sizeof(Instr));
    cbr->id = ctx->next_instr_id++;
    cbr->op = OP_CONDBR;
    cbr->dst = 0;
    cbr->src[0] = val_lhs;
    cbr->src[1] = blk_rhs;
    cbr->src[2] = blk_bypass;
    cbr->n_src = 3;
    cbr->exec_freq = 1.0;
    emit_instr(ctx, cbr);
    fn->blocks[blk_lhs_end]->succs[0] = blk_rhs;
    fn->blocks[blk_lhs_end]->succs[1] = blk_bypass;
    fn->blocks[blk_lhs_end]->n_succs = 2;
    fn->blocks[blk_lhs_end]->branch_prob[0] = 0.5f;
    fn->blocks[blk_lhs_end]->branch_prob[1] = 0.5f;
    fn->blocks[blk_rhs]->preds[fn->blocks[blk_rhs]->n_preds++] = blk_lhs_end;
    fn->blocks[blk_bypass]->preds[fn->blocks[blk_bypass]->n_preds++] =
        blk_lhs_end;

    ctx->cur_block = blk_bypass;
    BlockID blk_bypass_end = ctx->cur_block;
    Instr *br_bypass = calloc(1, sizeof(Instr));
    br_bypass->id = ctx->next_instr_id++;
    br_bypass->op = OP_BR;
    br_bypass->src[0] = blk_merge;
    br_bypass->n_src = 1;
    br_bypass->exec_freq = 1.0;
    emit_instr(ctx, br_bypass);
    fn->blocks[blk_bypass_end]->succs[0] = blk_merge;
    fn->blocks[blk_bypass_end]->n_succs = 1;
    fn->blocks[blk_merge]->preds[fn->blocks[blk_merge]->n_preds++] =
        blk_bypass_end;

    ctx->cur_block = blk_rhs;
    RegID val_rhs_raw = node->rhs ? zcc_lower_expr(ctx, node->rhs) : 0;
    if (!val_rhs_raw)
      val_rhs_raw = zero_r;
    RegID val_rhs = ctx->next_reg++;
    Instr *ne_ins = calloc(1, sizeof(Instr));
    ne_ins->id = ctx->next_instr_id++;
    ne_ins->op = OP_NE;
    ne_ins->dst = val_rhs;
    ne_ins->src[0] = val_rhs_raw;
    ne_ins->src[1] = zero_r;
    ne_ins->n_src = 2;
    ne_ins->exec_freq = 1.0;
    emit_instr(ctx, ne_ins);
    BlockID blk_rhs_end = ctx->cur_block;
    Instr *br_rhs = calloc(1, sizeof(Instr));
    br_rhs->id = ctx->next_instr_id++;
    br_rhs->op = OP_BR;
    br_rhs->src[0] = blk_merge;
    br_rhs->n_src = 1;
    br_rhs->exec_freq = 1.0;
    emit_instr(ctx, br_rhs);
    fn->blocks[blk_rhs_end]->succs[0] = blk_merge;
    fn->blocks[blk_rhs_end]->n_succs = 1;
    fn->blocks[blk_merge]->preds[fn->blocks[blk_merge]->n_preds++] =
        blk_rhs_end;

    ctx->cur_block = blk_merge;
    RegID res_r = ctx->next_reg++;
    Instr *phi = calloc(1, sizeof(Instr));
    phi->id = ctx->next_instr_id++;
    phi->op = OP_PHI;
    phi->dst = res_r;
    phi->phi[0].reg = zero_r;
    phi->phi[0].block = blk_bypass_end;
    phi->phi[1].reg = val_rhs;
    phi->phi[1].block = blk_rhs_end;
    phi->n_phi = 2;
    phi->exec_freq = 1.0;
    emit_instr(ctx, phi);
    return res_r;
  }
  /* ZND_LOR: A || B. Short-circuit: if A go to merge with 1; else evaluate B,
   * normalize to 0/1. SSA: OP_PHI at merge. */
  if (node->kind == ZND_LOR) {
    RegID val_lhs = node->lhs ? zcc_lower_expr(ctx, node->lhs) : 0;
    if (!val_lhs)
      return 0;
    BlockID blk_lhs_end = ctx->cur_block;
    BlockID blk_one = new_block(ctx, "lor.one");
    BlockID blk_rhs = new_block(ctx, "lor.rhs");
    BlockID blk_merge = new_block(ctx, "lor.merge");
    Function *fn = ctx->fn;

    Instr *cbr = calloc(1, sizeof(Instr));
    cbr->id = ctx->next_instr_id++;
    cbr->op = OP_CONDBR;
    cbr->dst = 0;
    cbr->src[0] = val_lhs;
    cbr->src[1] = blk_one;
    cbr->src[2] = blk_rhs;
    cbr->n_src = 3;
    cbr->exec_freq = 1.0;
    emit_instr(ctx, cbr);
    fn->blocks[blk_lhs_end]->succs[0] = blk_one;
    fn->blocks[blk_lhs_end]->succs[1] = blk_rhs;
    fn->blocks[blk_lhs_end]->n_succs = 2;
    fn->blocks[blk_lhs_end]->branch_prob[0] = 0.5f;
    fn->blocks[blk_lhs_end]->branch_prob[1] = 0.5f;
    fn->blocks[blk_one]->preds[fn->blocks[blk_one]->n_preds++] = blk_lhs_end;
    fn->blocks[blk_rhs]->preds[fn->blocks[blk_rhs]->n_preds++] = blk_lhs_end;

    RegID one_r = ctx->next_reg++;
    Instr *c1 =
        make_instr_imm(ctx->next_instr_id++, OP_CONST, one_r, 1, node->line_no);
    ctx->cur_block = blk_one;
    emit_instr(ctx, c1);
    BlockID blk_one_end = ctx->cur_block;
    Instr *br_one = calloc(1, sizeof(Instr));
    br_one->id = ctx->next_instr_id++;
    br_one->op = OP_BR;
    br_one->src[0] = blk_merge;
    br_one->n_src = 1;
    br_one->exec_freq = 1.0;
    emit_instr(ctx, br_one);
    fn->blocks[blk_one_end]->succs[0] = blk_merge;
    fn->blocks[blk_one_end]->n_succs = 1;
    fn->blocks[blk_merge]->preds[fn->blocks[blk_merge]->n_preds++] =
        blk_one_end;

    RegID zero_r = ctx->next_reg++;
    Instr *c0 = make_instr_imm(ctx->next_instr_id++, OP_CONST, zero_r, 0,
                               node->line_no);
    ctx->cur_block = blk_rhs;
    emit_instr(ctx, c0);
    RegID val_rhs_raw = node->rhs ? zcc_lower_expr(ctx, node->rhs) : 0;
    if (!val_rhs_raw)
      val_rhs_raw = zero_r;
    RegID val_rhs = ctx->next_reg++;
    Instr *ne_ins = calloc(1, sizeof(Instr));
    ne_ins->id = ctx->next_instr_id++;
    ne_ins->op = OP_NE;
    ne_ins->dst = val_rhs;
    ne_ins->src[0] = val_rhs_raw;
    ne_ins->src[1] = zero_r;
    ne_ins->n_src = 2;
    ne_ins->exec_freq = 1.0;
    emit_instr(ctx, ne_ins);
    BlockID blk_rhs_end = ctx->cur_block;
    Instr *br_rhs = calloc(1, sizeof(Instr));
    br_rhs->id = ctx->next_instr_id++;
    br_rhs->op = OP_BR;
    br_rhs->src[0] = blk_merge;
    br_rhs->n_src = 1;
    br_rhs->exec_freq = 1.0;
    emit_instr(ctx, br_rhs);
    fn->blocks[blk_rhs_end]->succs[0] = blk_merge;
    fn->blocks[blk_rhs_end]->n_succs = 1;
    fn->blocks[blk_merge]->preds[fn->blocks[blk_merge]->n_preds++] =
        blk_rhs_end;

    ctx->cur_block = blk_merge;
    RegID res_r = ctx->next_reg++;
    Instr *phi = calloc(1, sizeof(Instr));
    phi->id = ctx->next_instr_id++;
    phi->op = OP_PHI;
    phi->dst = res_r;
    phi->phi[0].reg = one_r;
    phi->phi[0].block = blk_one_end;
    phi->phi[1].reg = val_rhs;
    phi->phi[1].block = blk_rhs_end;
    phi->n_phi = 2;
    phi->exec_freq = 1.0;
    emit_instr(ctx, phi);
    return res_r;
  }
  RegID r = ctx->next_reg++;
  Instr *ins = NULL;
  switch (node->kind) {
  case ZND_NUM:
    ins = make_instr_imm(ctx->next_instr_id++, OP_CONST, r, node->int_val,
                         node->line_no);
    break;
  case ZND_STR: {
    char lab[32];
    snprintf(lab, sizeof(lab), ".Lstr_%lld", (long long)node->int_val);
    ins = calloc(1, sizeof(Instr));
    ins->id = ctx->next_instr_id++;
    ins->op = OP_GLOBAL;
    ins->dst = r;
    ins->n_src = 0;
    strncpy(ins->call_name, lab, sizeof(ins->call_name) - 1);
    ins->call_name[sizeof(ins->call_name) - 1] = '\0';
    ins->exec_freq = 1.0;
    break;
  }
  case ZND_VAR: {
    if (node->is_global) {
      ins = calloc(1, sizeof(Instr));
      ins->id = ctx->next_instr_id++;
      ins->op = OP_GLOBAL;
      ins->dst = r;
      ins->n_src = 0;
      strncpy(ins->call_name, node->name, sizeof(ins->call_name) - 1);
      ins->call_name[sizeof(ins->call_name) - 1] = '\0';
      ins->exec_freq = 1.0;
      if (ctx->want_address || node->is_array || node->is_func) {
        emit_instr(ctx, ins);
        return r;
      }
      emit_instr(ctx, ins);
      Instr *load_ins = calloc(1, sizeof(Instr));
      load_ins->id = ctx->next_instr_id++;
      load_ins->op = OP_LOAD;
      RegID val_r = ctx->next_reg++;
      load_ins->dst = val_r;
      load_ins->src[0] = r;
      load_ins->n_src = 1;
      load_ins->imm = 8;
      load_ins->exec_freq = 1.0;
      emit_instr(ctx, load_ins);
      return val_r;
    } else {
      RegID alloca_r = get_or_create_var(ctx, node->name, node->member_size > 0 ? node->member_size : 8);
      if (ctx->want_address || node->is_array || node->is_func)
        return alloca_r;
      ins = calloc(1, sizeof(Instr));
      ins->id = ctx->next_instr_id++;
      ins->op = OP_LOAD;
      ins->dst = r;
      ins->src[0] = alloca_r;
      ins->n_src = 1;
      ins->imm = node->member_size > 0 ? node->member_size : 8;
      ins->exec_freq = 1.0;
    }
    break;
  }
  case ZND_ADD: {
    RegID l = zcc_lower_expr(ctx, node->lhs);
    RegID rh = zcc_lower_expr(ctx, node->rhs);
    ins = calloc(1, sizeof(Instr));
    ins->id = ctx->next_instr_id++;
    ins->op = OP_ADD;
    ins->ir_type = irtype_from_node(node); /* CG-IR-017 */
    ins->dst = r;
    ins->src[0] = l;
    ins->src[1] = rh;
    ins->n_src = 2;
    ins->exec_freq = 1.0;
    break;
  }
  case ZND_SUB: {
    RegID l = zcc_lower_expr(ctx, node->lhs);
    RegID rh = zcc_lower_expr(ctx, node->rhs);
    ins = calloc(1, sizeof(Instr));
    ins->id = ctx->next_instr_id++;
    ins->op = OP_SUB;
    ins->ir_type = irtype_from_node(node); /* CG-IR-017 */
    ins->dst = r;
    ins->src[0] = l;
    ins->src[1] = rh;
    ins->n_src = 2;
    ins->exec_freq = 1.0;
    break;
  }
  case ZND_MOD: {
    RegID l = zcc_lower_expr(ctx, node->lhs);
    RegID rh = zcc_lower_expr(ctx, node->rhs);
    ins = calloc(1, sizeof(Instr));
    ins->id = ctx->next_instr_id++;
    ins->op = OP_MOD;
    ins->ir_type = irtype_from_node(node); /* CG-IR-015 */
    ins->dst = r;
    ins->src[0] = l;
    ins->src[1] = rh;
    ins->n_src = 2;
    ins->exec_freq = 1.0;
    break;
  }
  case ZND_MUL: {
    RegID l = zcc_lower_expr(ctx, node->lhs);
    RegID rh = zcc_lower_expr(ctx, node->rhs);
    ins = calloc(1, sizeof(Instr));
    ins->id = ctx->next_instr_id++;
    ins->op = OP_MUL;
    ins->ir_type = irtype_from_node(node); /* CG-IR-017 */
    ins->dst = r;
    ins->src[0] = l;
    ins->src[1] = rh;
    ins->n_src = 2;
    ins->exec_freq = 1.0;
    break;
  }
  case ZND_DIV: {
    RegID l = zcc_lower_expr(ctx, node->lhs);
    RegID rh = zcc_lower_expr(ctx, node->rhs);
    ins = calloc(1, sizeof(Instr));
    ins->id = ctx->next_instr_id++;
    ins->op = OP_DIV;
    ins->ir_type = irtype_from_node(node); /* CG-IR-015 */
    ins->dst = r;
    ins->src[0] = l;
    ins->src[1] = rh;
    ins->n_src = 2;
    ins->exec_freq = 1.0;
    ins->line_no = node->line_no;
    break;
  }
  case ZND_NEG: {
    RegID l = zcc_lower_expr(ctx, node->lhs);
    RegID zero_r = ctx->next_reg++;
    Instr *c0 = make_instr_imm(ctx->next_instr_id++, OP_CONST, zero_r, 0,
                               node->line_no);
    emit_instr(ctx, c0);
    ins = calloc(1, sizeof(Instr));
    ins->id = ctx->next_instr_id++;
    ins->op = OP_SUB;
    ins->ir_type = irtype_from_node(node); /* CG-IR-017: NEG → 0-x */
    ins->line_no = node->line_no;
    ins->dst = r;
    ins->src[0] = zero_r;
    ins->src[1] = l;
    ins->n_src = 2;
    ins->exec_freq = 1.0;
    ins->line_no = node->line_no;
    break;
  }
  case ZND_BOR: {
    RegID l = zcc_lower_expr(ctx, node->lhs);
    RegID rh = zcc_lower_expr(ctx, node->rhs);
    ins = calloc(1, sizeof(Instr));
    ins->id = ctx->next_instr_id++;
    ins->op = OP_BOR;
    ins->ir_type = irtype_from_node(node); /* CG-IR-017 */
    ins->dst = r;
    ins->src[0] = l;
    ins->src[1] = rh;
    ins->n_src = 2;
    ins->exec_freq = 1.0;
    ins->line_no = node->line_no;
    break;
  }
  case ZND_BXOR: {
    RegID l = zcc_lower_expr(ctx, node->lhs);
    RegID rh = zcc_lower_expr(ctx, node->rhs);
    ins = calloc(1, sizeof(Instr));
    ins->id = ctx->next_instr_id++;
    ins->op = OP_BXOR;
    ins->ir_type = irtype_from_node(node); /* CG-IR-017 */
    ins->dst = r;
    ins->src[0] = l;
    ins->src[1] = rh;
    ins->n_src = 2;
    ins->exec_freq = 1.0;
    ins->line_no = node->line_no;
    break;
  }
  case ZND_BNOT: {
    RegID l = zcc_lower_expr(ctx, node->lhs);
    ins = calloc(1, sizeof(Instr));
    ins->id = ctx->next_instr_id++;
    ins->op = OP_BNOT;
    ins->ir_type = irtype_from_node(node); /* CG-IR-017 */
    ins->dst = r;
    ins->src[0] = l;
    ins->n_src = 1;
    ins->exec_freq = 1.0;
    ins->line_no = node->line_no;
    break;
  }
  case ZND_LNOT: {
    RegID l = zcc_lower_expr(ctx, node->lhs);
    RegID zero_r = ctx->next_reg++;
    Instr *c0 = make_instr_imm(ctx->next_instr_id++, OP_CONST, zero_r, 0,
                               node->line_no);
    emit_instr(ctx, c0);
    ins = calloc(1, sizeof(Instr));
    ins->id = ctx->next_instr_id++;
    ins->op = OP_EQ;
    ins->line_no = node->line_no;
    ins->dst = r;
    ins->src[0] = l;
    ins->src[1] = zero_r;
    ins->n_src = 2;
    ins->exec_freq = 1.0;
    ins->line_no = node->line_no;
    break;
  }
  case ZND_BAND: {
    RegID l = zcc_lower_expr(ctx, node->lhs);
    RegID rh = zcc_lower_expr(ctx, node->rhs);
    ins = calloc(1, sizeof(Instr));
    ins->id = ctx->next_instr_id++;
    ins->op = OP_BAND;
    ins->ir_type = irtype_from_node(node); /* CG-IR-017 */
    ins->dst = r;
    ins->src[0] = l;
    ins->src[1] = rh;
    ins->n_src = 2;
    ins->exec_freq = 1.0;
    ins->line_no = node->line_no;
    break;
  }
  case ZND_SHL: {
    RegID l = zcc_lower_expr(ctx, node->lhs);
    RegID rh = zcc_lower_expr(ctx, node->rhs);
    ins = calloc(1, sizeof(Instr));
    ins->id = ctx->next_instr_id++;
    ins->op = OP_SHL;
    ins->ir_type = irtype_from_node(node); /* CG-IR-015 */
    ins->dst = r;
    ins->src[0] = l;
    ins->src[1] = rh;
    ins->n_src = 2;
    ins->exec_freq = 1.0;
    ins->line_no = node->line_no;
    break;
  }
  case ZND_SHR: {
    RegID l = zcc_lower_expr(ctx, node->lhs);
    RegID rh = zcc_lower_expr(ctx, node->rhs);
    ins = calloc(1, sizeof(Instr));
    ins->id = ctx->next_instr_id++;
    ins->op = OP_SHR;
    ins->ir_type = irtype_from_node(node); /* CG-IR-015 */
    ins->dst = r;
    ins->src[0] = l;
    ins->src[1] = rh;
    ins->n_src = 2;
    ins->exec_freq = 1.0;
    ins->line_no = node->line_no;
    break;
  }
  case ZND_LT:
  case ZND_LE:
  case ZND_GT:
  case ZND_GE:
  case ZND_EQ:
  case ZND_NE: {
    RegID l = zcc_lower_expr(ctx, node->lhs);
    RegID rh = zcc_lower_expr(ctx, node->rhs);
    Opcode cmp_op = OP_LT;
    if (node->kind == ZND_EQ)
      cmp_op = OP_EQ;
    else if (node->kind == ZND_NE)
      cmp_op = OP_NE;
    else if (node->kind == ZND_GT)
      cmp_op = OP_GT;
    else if (node->kind == ZND_GE)
      cmp_op = OP_GE;
    else if (node->kind == ZND_LE)
      cmp_op = OP_LE;
    ins = calloc(1, sizeof(Instr));
    ins->id = ctx->next_instr_id++;
    ins->op = cmp_op;
    ins->ir_type = irtype_from_node(node->lhs); /* CG-IR-017: operand width */
    ins->dst = r;
    ins->src[0] = l;
    ins->src[1] = rh;
    ins->n_src = 2;
    ins->exec_freq = 1.0;
    ins->line_no = node->line_no;
    break;
  }
  case ZND_POST_INC: {
    RegID alloca_r = get_or_create_var(ctx, node->lhs->name, 8);
    if (!alloca_r)
      return 0;
    Instr *load_ins = calloc(1, sizeof(Instr));
    load_ins->id = ctx->next_instr_id++;
    load_ins->op = OP_LOAD;
    load_ins->dst = r;
    load_ins->src[0] = alloca_r;
    load_ins->n_src = 1;
    load_ins->imm =
        (node->lhs && node->lhs->member_size > 0) ? node->lhs->member_size : 8;
    load_ins->exec_freq = 1.0;
    load_ins->line_no = node->line_no;
    emit_instr(ctx, load_ins);
    RegID r_new = ctx->next_reg++;
    Instr *add_ins = calloc(1, sizeof(Instr));
    add_ins->id = ctx->next_instr_id++;
    add_ins->op = OP_ADD;
    add_ins->dst = r_new;
    add_ins->src[0] = r;
    add_ins->src[1] = 0; /* will use OP_CONST 1 */
    add_ins->n_src = 2;
    add_ins->exec_freq = 1.0;
    add_ins->line_no = node->line_no;
    RegID one_r = ctx->next_reg++;
    int64_t inc_val = (node->int_val > 0) ? node->int_val : 1;
    Instr *c1 = make_instr_imm(ctx->next_instr_id++, OP_CONST, one_r, inc_val,
                               node->line_no);
    emit_instr(ctx, c1);
    add_ins->src[1] = one_r;
    emit_instr(ctx, add_ins);
    Instr *st = calloc(1, sizeof(Instr));
    st->id = ctx->next_instr_id++;
    st->op = OP_STORE;
    st->dst = 0;
    st->src[0] = r_new;
    st->src[1] = alloca_r;
    st->n_src = 2;
    st->exec_freq = 1.0;
    st->imm =
        (node->lhs && node->lhs->member_size > 0) ? node->lhs->member_size : 8;
    st->line_no = node->line_no;
    emit_instr(ctx, st);
    return r; /* return value before increment */
  }
  case ZND_POST_DEC: {
    RegID alloca_r = get_or_create_var(ctx, node->lhs->name, 8);
    if (!alloca_r)
      return 0;
    Instr *load_ins = calloc(1, sizeof(Instr));
    load_ins->id = ctx->next_instr_id++;
    load_ins->op = OP_LOAD;
    load_ins->dst = r;
    load_ins->src[0] = alloca_r;
    load_ins->n_src = 1;
    load_ins->imm =
        (node->lhs && node->lhs->member_size > 0) ? node->lhs->member_size : 8;
    load_ins->exec_freq = 1.0;
    load_ins->line_no = node->line_no;
    emit_instr(ctx, load_ins);
    RegID r_new = ctx->next_reg++;
    Instr *sub_ins = calloc(1, sizeof(Instr));
    sub_ins->id = ctx->next_instr_id++;
    sub_ins->op = OP_SUB;
    sub_ins->dst = r_new;
    sub_ins->src[0] = r;
    sub_ins->src[1] = 0;
    sub_ins->n_src = 2;
    sub_ins->exec_freq = 1.0;
    sub_ins->line_no = node->line_no;
    RegID one_r = ctx->next_reg++;
    int64_t inc_val = (node->int_val > 0) ? node->int_val : 1;
    Instr *c1 = make_instr_imm(ctx->next_instr_id++, OP_CONST, one_r, inc_val,
                               node->line_no);
    emit_instr(ctx, c1);
    sub_ins->src[1] = one_r;
    emit_instr(ctx, sub_ins);
    Instr *st = calloc(1, sizeof(Instr));
    st->id = ctx->next_instr_id++;
    st->op = OP_STORE;
    st->dst = 0;
    st->src[0] = r_new;
    st->src[1] = alloca_r;
    st->n_src = 2;
    st->exec_freq = 1.0;
    st->imm =
        (node->lhs && node->lhs->member_size > 0) ? node->lhs->member_size : 8;
    st->line_no = node->line_no;
    emit_instr(ctx, st);
    return r; /* return value before decrement */
  }
  case ZND_PRE_INC: {
    RegID alloca_r = get_or_create_var(ctx, node->lhs->name, 8);
    if (!alloca_r)
      return 0;
    Instr *load_ins = calloc(1, sizeof(Instr));
    load_ins->id = ctx->next_instr_id++;
    load_ins->op = OP_LOAD;
    load_ins->dst = r;
    load_ins->src[0] = alloca_r;
    load_ins->n_src = 1;
    load_ins->imm =
        (node->lhs && node->lhs->member_size > 0) ? node->lhs->member_size : 8;
    load_ins->exec_freq = 1.0;
    load_ins->line_no = node->line_no;
    emit_instr(ctx, load_ins);
    RegID r_new = ctx->next_reg++;
    Instr *add_ins = calloc(1, sizeof(Instr));
    add_ins->id = ctx->next_instr_id++;
    add_ins->op = OP_ADD;
    add_ins->dst = r_new;
    add_ins->src[0] = r;
    add_ins->src[1] = 0;
    add_ins->n_src = 2;
    add_ins->exec_freq = 1.0;
    add_ins->line_no = node->line_no;
    RegID one_r = ctx->next_reg++;
    int64_t inc_val = (node->int_val > 0) ? node->int_val : 1;
    Instr *c1 = make_instr_imm(ctx->next_instr_id++, OP_CONST, one_r, inc_val,
                               node->line_no);
    emit_instr(ctx, c1);
    add_ins->src[1] = one_r;
    emit_instr(ctx, add_ins);
    Instr *st = calloc(1, sizeof(Instr));
    st->id = ctx->next_instr_id++;
    st->op = OP_STORE;
    st->dst = 0;
    st->src[0] = r_new;
    st->src[1] = alloca_r;
    st->n_src = 2;
    st->exec_freq = 1.0;
    st->imm =
        (node->lhs && node->lhs->member_size > 0) ? node->lhs->member_size : 8;
    st->line_no = node->line_no;
    emit_instr(ctx, st);
    return r_new; /* return value after increment */
  }
  case ZND_PRE_DEC: {
    RegID alloca_r = get_or_create_var(ctx, node->lhs->name, 8);
    if (!alloca_r)
      return 0;
    Instr *load_ins = calloc(1, sizeof(Instr));
    load_ins->id = ctx->next_instr_id++;
    load_ins->op = OP_LOAD;
    load_ins->dst = r;
    load_ins->src[0] = alloca_r;
    load_ins->n_src = 1;
    load_ins->imm =
        (node->lhs && node->lhs->member_size > 0) ? node->lhs->member_size : 8;
    load_ins->exec_freq = 1.0;
    load_ins->line_no = node->line_no;
    emit_instr(ctx, load_ins);
    RegID r_new = ctx->next_reg++;
    Instr *sub_ins = calloc(1, sizeof(Instr));
    sub_ins->id = ctx->next_instr_id++;
    sub_ins->op = OP_SUB;
    sub_ins->dst = r_new;
    sub_ins->src[0] = r;
    sub_ins->src[1] = 0;
    sub_ins->n_src = 2;
    sub_ins->exec_freq = 1.0;
    sub_ins->line_no = node->line_no;
    RegID one_r = ctx->next_reg++;
    int64_t inc_val = (node->int_val > 0) ? node->int_val : 1;
    Instr *c1 = make_instr_imm(ctx->next_instr_id++, OP_CONST, one_r, inc_val,
                               node->line_no);
    emit_instr(ctx, c1);
    sub_ins->src[1] = one_r;
    emit_instr(ctx, sub_ins);
    Instr *st = calloc(1, sizeof(Instr));
    st->id = ctx->next_instr_id++;
    st->op = OP_STORE;
    st->dst = 0;
    st->src[0] = r_new;
    st->src[1] = alloca_r;
    st->n_src = 2;
    st->exec_freq = 1.0;
    st->imm =
        (node->lhs && node->lhs->member_size > 0) ? node->lhs->member_size : 8;
    st->line_no = node->line_no;
    emit_instr(ctx, st);
    return r_new; /* return value after decrement */
  }
  case ZND_SIZEOF: {
    int64_t sz = node->int_val > 0 ? node->int_val : 8;
    emit_instr(ctx, make_instr_imm(ctx->next_instr_id++, OP_CONST, r, sz,
                                   node->line_no));
    return r;
  }
  case ZND_CALL: {
    int na = node->num_args;
    if (na < 0)
      na = 0;
    if (na > MAX_CALL_ARGS)
      na = MAX_CALL_ARGS;
    ins = calloc(1, sizeof(Instr));
    ins->id = ctx->next_instr_id++;
    ins->op = OP_CALL;
    ins->dst = r;
    if (node->func_name && node->func_name[0]) {
      strncpy(ins->call_name, node->func_name, sizeof(ins->call_name) - 1);
      ins->call_name[sizeof(ins->call_name) - 1] = '\0';
      ins->n_src = 0;
    } else if (node->lhs) {
      ins->src[0] = zcc_lower_expr(ctx, node->lhs);
      ins->n_src = 1;
    }
    ins->n_call_args = (uint32_t)na;
    for (int i = 0; i < na && node->args; i++)
      ins->call_args[i] = zcc_lower_expr(ctx, node->args[i]);
    ins->exec_freq = 1.0;
    ins->line_no = node->line_no;
    break;
  }
  case ZND_ASSIGN: {
    if (!node->lhs)
      return 0;
    int old_want = ctx->want_address;
    ctx->want_address = 1;
    RegID addr_r = zcc_lower_expr(ctx, node->lhs);
    ctx->want_address = old_want;
    if (!addr_r)
      return 0;
    RegID val_r = zcc_lower_expr(ctx, node->rhs);
    
    if (node->lhs && node->lhs->is_bitfield) {
        fprintf(stderr, "WIRING L-VALUE BITFIELD: offset=%d, size=%d\n", node->lhs->bit_offset, node->lhs->bit_size);
        RegID old_r = ctx->next_reg++;
        Instr *ld_o = calloc(1, sizeof(Instr));
        ld_o->id = ctx->next_instr_id++;
        ld_o->op = OP_LOAD;
        ld_o->dst = old_r;
        ld_o->src[0] = addr_r;
        ld_o->n_src = 1;
        ld_o->imm = (node->lhs->member_size > 0) ? node->lhs->member_size : 8;
        ld_o->exec_freq = 1.0;
        emit_instr(ctx, ld_o);
        
        RegID mask_r = ctx->next_reg++;
        long long mask_val = (1ULL << node->lhs->bit_size) - 1;
        long long shift_mask = ~(mask_val << node->lhs->bit_offset);
        emit_instr(ctx, make_instr_imm(ctx->next_instr_id++, OP_CONST, mask_r, shift_mask, node->line_no));
        RegID band1_r = ctx->next_reg++;
        Instr *b1_i = calloc(1, sizeof(Instr));
        b1_i->id = ctx->next_instr_id++;
        b1_i->op = OP_BAND;
        b1_i->dst = band1_r;
        b1_i->src[0] = old_r;
        b1_i->src[1] = mask_r;
        b1_i->n_src = 2;
        b1_i->exec_freq = 1.0;
        emit_instr(ctx, b1_i);
        
        RegID just_mask = ctx->next_reg++;
        emit_instr(ctx, make_instr_imm(ctx->next_instr_id++, OP_CONST, just_mask, mask_val, node->line_no));
        RegID band2_r = ctx->next_reg++;
        Instr *b2_i = calloc(1, sizeof(Instr));
        b2_i->id = ctx->next_instr_id++;
        b2_i->op = OP_BAND;
        b2_i->dst = band2_r;
        b2_i->src[0] = val_r;
        b2_i->src[1] = just_mask;
        b2_i->n_src = 2;
        b2_i->exec_freq = 1.0;
        emit_instr(ctx, b2_i);
        
        RegID bo_r = ctx->next_reg++;
        emit_instr(ctx, make_instr_imm(ctx->next_instr_id++, OP_CONST, bo_r, node->lhs->bit_offset, node->line_no));
        RegID shl_r = ctx->next_reg++;
        Instr *sh_i = calloc(1, sizeof(Instr));
        sh_i->id = ctx->next_instr_id++;
        sh_i->op = OP_SHL;
        sh_i->dst = shl_r;
        sh_i->src[0] = band2_r;
        sh_i->src[1] = bo_r;
        sh_i->n_src = 2;
        sh_i->exec_freq = 1.0;
        emit_instr(ctx, sh_i);
        
        RegID final_r = ctx->next_reg++;
        Instr *bor_i = calloc(1, sizeof(Instr));
        bor_i->id = ctx->next_instr_id++;
        bor_i->op = OP_BOR;
        bor_i->dst = final_r;
        bor_i->src[0] = band1_r;
        bor_i->src[1] = shl_r;
        bor_i->n_src = 2;
        bor_i->exec_freq = 1.0;
        emit_instr(ctx, bor_i);
        
        val_r = final_r;
    }

    Instr *st = calloc(1, sizeof(Instr));
    st->id = ctx->next_instr_id++;
    st->op = OP_STORE;
    st->dst = 0;
    st->src[0] = val_r;
    st->src[1] = addr_r;
    st->n_src = 2;
    st->exec_freq = 1.0;
    st->imm =
        (node->lhs && node->lhs->member_size > 0) ? node->lhs->member_size : 8;
    emit_instr(ctx, st);
    return val_r;
  }
  case ZND_COMPOUND_ASSIGN: {
    if (!node->lhs || !node->rhs)
      return 0;
    int old_want = ctx->want_address;
    ctx->want_address = 1;
    RegID addr_r = zcc_lower_expr(ctx, node->lhs);
    ctx->want_address = old_want;
    if (!addr_r)
      return 0;

    RegID load_r = ctx->next_reg++;
    Instr *load_ins = calloc(1, sizeof(Instr));
    load_ins->id = ctx->next_instr_id++;
    load_ins->op = OP_LOAD;
    load_ins->dst = load_r;
    load_ins->src[0] = addr_r;
    load_ins->n_src = 1;
    load_ins->imm = 8;
    if (node->lhs && node->lhs->kind == ZND_DEREF && node->lhs->member_size > 0)
      load_ins->imm = node->lhs->member_size;
    load_ins->exec_freq = 1.0;
    emit_instr(ctx, load_ins);

    RegID rhs_r = zcc_lower_expr(ctx, node->rhs);
    RegID sum_r = ctx->next_reg++;
    Instr *add_ins = calloc(1, sizeof(Instr));
    add_ins->id = ctx->next_instr_id++;

    if (node->compound_op == ZND_SUB)
      add_ins->op = OP_SUB;
    else if (node->compound_op == ZND_MUL)
      add_ins->op = OP_MUL;
    else if (node->compound_op == ZND_DIV)
      add_ins->op = OP_DIV;
    else if (node->compound_op == ZND_MOD)
      add_ins->op = OP_MOD;
    else if (node->compound_op == ZND_BAND)
      add_ins->op = OP_BAND;
    else if (node->compound_op == ZND_BOR)
      add_ins->op = OP_BOR;
    else if (node->compound_op == ZND_BXOR)
      add_ins->op = OP_BXOR;
    else if (node->compound_op == ZND_SHL)
      add_ins->op = OP_SHL;
    else if (node->compound_op == ZND_SHR)
      add_ins->op = OP_SHR;
    else
      add_ins->op = OP_ADD;

    add_ins->dst = sum_r;
    add_ins->src[0] = load_r;
    add_ins->src[1] = rhs_r;
    add_ins->n_src = 2;
    add_ins->exec_freq = 1.0;
    emit_instr(ctx, add_ins);

    Instr *st = calloc(1, sizeof(Instr));
    st->id = ctx->next_instr_id++;
    st->op = OP_STORE;
    st->dst = 0;
    st->src[0] = sum_r;
    st->src[1] = addr_r;
    st->n_src = 2;
    st->exec_freq = 1.0;
    st->imm = 8;
    if (node->lhs && node->lhs->kind == ZND_DEREF && node->lhs->member_size > 0)
      st->imm = node->lhs->member_size;
    emit_instr(ctx, st);
    return sum_r;
  }
  default:
    return 0;
  }
  if (ins) {
    emit_instr(ctx, ins);
    return r;
  }
  return 0;
}

static void zcc_lower_stmt(LowerCtx *ctx, ZCCNode *node) {
  if (!node)
    return;
  Function *fn = ctx->fn;
  switch (node->kind) {
  case ZND_NOP:
    return;
  case ZND_ASM: {
    Instr *ins = calloc(1, sizeof(Instr));
    ins->id = ctx->next_instr_id++;
    ins->op = OP_ASM;
    ins->asm_string = node->asm_string;
    ins->ir_type = IR_TY_I64;
    emit_instr(ctx, ins);
    return;
  }
  case ZND_VLA_ALLOC: {
    if (!node->lhs) return;
    RegID count_r = zcc_lower_expr(ctx, node->lhs);
    
    Instr *imm = calloc(1, sizeof(Instr));
    imm->id = ctx->next_instr_id++;
    imm->op = OP_CONST;
    imm->dst = ctx->next_reg++;
    imm->imm = node->int_val;
    emit_instr(ctx, imm);
    
    Instr *imul = calloc(1, sizeof(Instr));
    imul->id = ctx->next_instr_id++;
    imul->op = OP_MUL;
    imul->dst = ctx->next_reg++;
    imul->src[0] = count_r;
    imul->src[1] = imm->dst;
    imul->n_src = 2;
    emit_instr(ctx, imul);
    
    Instr *al = calloc(1, sizeof(Instr));
    al->id = ctx->next_instr_id++;
    al->op = OP_VLA_ALLOC;
    al->dst = ctx->next_reg++;
    al->src[0] = imul->dst;
    al->n_src = 1;
    emit_instr(ctx, al);

    if (node->name[0]) {
      RegID addr_r = get_or_create_var(ctx, node->name, 8);
      Instr *st = calloc(1, sizeof(Instr));
      st->id = ctx->next_instr_id++;
      st->op = OP_STORE;
      st->src[0] = al->dst;
      st->src[1] = addr_r;
      st->n_src = 2;
      st->imm = 8;
      emit_instr(ctx, st);
    }
    return;
  }
  case ZND_ASSIGN: {
    if (!node->lhs)
      return;
    RegID addr_r;
    ctx->want_address = 1;
    addr_r = zcc_lower_expr(ctx, node->lhs);
    ctx->want_address = 0;
    if (!addr_r)
      return;
    RegID val_r = zcc_lower_expr(ctx, node->rhs);
    Instr *st = calloc(1, sizeof(Instr));
    st->id = ctx->next_instr_id++;
    st->op = OP_STORE;
    st->dst = 0;
    st->src[0] = val_r;
    st->src[1] = addr_r;
    st->n_src = 2;
    st->exec_freq = 1.0;
    st->imm =
        (node->lhs && node->lhs->member_size > 0) ? node->lhs->member_size : 8;
    emit_instr(ctx, st);
    return;
  }
  case ZND_COMPOUND_ASSIGN: {
    if (!node->lhs || !node->rhs)
      return;
    RegID addr_r;
    ctx->want_address = 1;
    addr_r = zcc_lower_expr(ctx, node->lhs);
    ctx->want_address = 0;
    if (!addr_r)
      return;

    RegID load_r = ctx->next_reg++;
    Instr *load_ins = calloc(1, sizeof(Instr));
    load_ins->id = ctx->next_instr_id++;
    load_ins->op = OP_LOAD;
    load_ins->dst = load_r;
    load_ins->src[0] = addr_r;
    load_ins->n_src = 1;
    load_ins->imm = 8;
    if (node->lhs && node->lhs->member_size > 0)
      load_ins->imm = node->lhs->member_size;
    load_ins->exec_freq = 1.0;
    emit_instr(ctx, load_ins);

    RegID rhs_r = zcc_lower_expr(ctx, node->rhs);
    RegID sum_r = ctx->next_reg++;
    Instr *add_ins = calloc(1, sizeof(Instr));
    add_ins->id = ctx->next_instr_id++;

    if (node->compound_op == ZND_SUB)
      add_ins->op = OP_SUB;
    else if (node->compound_op == ZND_MUL)
      add_ins->op = OP_MUL;
    else if (node->compound_op == ZND_DIV)
      add_ins->op = OP_DIV;
    else if (node->compound_op == ZND_MOD)
      add_ins->op = OP_MOD;
    else if (node->compound_op == ZND_BAND)
      add_ins->op = OP_BAND;
    else if (node->compound_op == ZND_BOR)
      add_ins->op = OP_BOR;
    else if (node->compound_op == ZND_BXOR)
      add_ins->op = OP_BXOR;
    else if (node->compound_op == ZND_SHL)
      add_ins->op = OP_SHL;
    else if (node->compound_op == ZND_SHR)
      add_ins->op = OP_SHR;
    else
      add_ins->op = OP_ADD;

    add_ins->dst = sum_r;
    add_ins->src[0] = load_r;
    add_ins->src[1] = rhs_r;
    add_ins->n_src = 2;
    add_ins->exec_freq = 1.0;
    emit_instr(ctx, add_ins);

    Instr *st = calloc(1, sizeof(Instr));
    st->id = ctx->next_instr_id++;
    st->op = OP_STORE;
    st->dst = 0;
    st->src[0] = sum_r;
    st->src[1] = addr_r;
    st->n_src = 2;
    st->exec_freq = 1.0;
    st->imm =
        (node->lhs && node->lhs->member_size > 0) ? node->lhs->member_size : 8;
    emit_instr(ctx, st);
    return;
  }
  case ZND_IF: {
    RegID cond_r = zcc_lower_expr(ctx, node->cond);
    char then_name[NAME_LEN], else_name[NAME_LEN], merge_name[NAME_LEN];
    int id = node->if_id;
    snprintf(then_name, sizeof(then_name), "if.then.%d", id);
    snprintf(else_name, sizeof(else_name), "if.else.%d", id);
    snprintf(merge_name, sizeof(merge_name), "if.merge.%d", id);
    BlockID then_blk = new_block(ctx, then_name);
    BlockID merge_blk = new_block(ctx, merge_name);
    Block *cur = fn->blocks[ctx->cur_block];
    if (node->else_body) {
      BlockID else_blk = new_block(ctx, else_name);
      Instr *cbr = calloc(1, sizeof(Instr));
      cbr->id = ctx->next_instr_id++;
      cbr->op = OP_CONDBR;
      cbr->dst = 0;
      cbr->src[0] = cond_r;
      cbr->src[1] = then_blk;
      cbr->src[2] = else_blk;
      cbr->n_src = 3;
      cbr->exec_freq = 1.0;
      emit_instr(ctx, cbr);
      cur->succs[0] = then_blk;
      cur->succs[1] = else_blk;
      cur->n_succs = 2;
      cur->branch_prob[0] = 0.5;
      cur->branch_prob[1] = 0.5;
      fn->blocks[then_blk]->preds[0] = ctx->cur_block;
      fn->blocks[then_blk]->n_preds = 1;
      fn->blocks[else_blk]->preds[0] = ctx->cur_block;
      fn->blocks[else_blk]->n_preds = 1;

      ctx->cur_block = then_blk;
      zcc_lower_stmt(ctx, node->then_body);
      {
        Block *_tb = fn->blocks[ctx->cur_block];
        Instr *_tt = _tb ? _tb->tail : NULL;
        if (!(_tt && (_tt->op == OP_RET || _tt->op == OP_BR ||
                      _tt->op == OP_CONDBR))) {
          Instr *br_then = calloc(1, sizeof(Instr));
          br_then->id = ctx->next_instr_id++;
          br_then->op = OP_BR;
          br_then->src[0] = merge_blk;
          br_then->n_src = 1;
          br_then->exec_freq = 1.0;
          emit_instr(ctx, br_then);
          fn->blocks[ctx->cur_block]->succs[0] = merge_blk;
          fn->blocks[ctx->cur_block]->n_succs = 1;
          fn->blocks[merge_blk]->preds[fn->blocks[merge_blk]->n_preds++] =
              ctx->cur_block;
        }
      }

      ctx->cur_block = else_blk;
      zcc_lower_stmt(ctx, node->else_body);
      {
        Block *_eb = fn->blocks[ctx->cur_block];
        Instr *_et = _eb ? _eb->tail : NULL;
        if (!(_et && (_et->op == OP_RET || _et->op == OP_BR ||
                      _et->op == OP_CONDBR))) {
          Instr *br_else = calloc(1, sizeof(Instr));
          br_else->id = ctx->next_instr_id++;
          br_else->op = OP_BR;
          br_else->src[0] = merge_blk;
          br_else->n_src = 1;
          br_else->exec_freq = 1.0;
          emit_instr(ctx, br_else);
          fn->blocks[ctx->cur_block]->succs[0] = merge_blk;
          fn->blocks[ctx->cur_block]->n_succs = 1;
          fn->blocks[merge_blk]->preds[fn->blocks[merge_blk]->n_preds++] =
              ctx->cur_block;
        }
      }
    } else {
      Instr *cbr = calloc(1, sizeof(Instr));
      cbr->id = ctx->next_instr_id++;
      cbr->op = OP_CONDBR;
      cbr->dst = 0;
      cbr->src[0] = cond_r;
      cbr->src[1] = then_blk;
      cbr->src[2] = merge_blk;
      cbr->n_src = 3;
      cbr->exec_freq = 1.0;
      emit_instr(ctx, cbr);
      cur->succs[0] = then_blk;
      cur->succs[1] = merge_blk;
      cur->n_succs = 2;
      cur->branch_prob[0] = 0.5;
      cur->branch_prob[1] = 0.5;
      fn->blocks[then_blk]->preds[0] = ctx->cur_block;
      fn->blocks[then_blk]->n_preds = 1;
      fn->blocks[merge_blk]->preds[fn->blocks[merge_blk]->n_preds++] =
          ctx->cur_block;

      ctx->cur_block = then_blk;
      zcc_lower_stmt(ctx, node->then_body);
      {
        Block *_tb = fn->blocks[ctx->cur_block];
        Instr *_tt = _tb ? _tb->tail : NULL;
        if (!(_tt && (_tt->op == OP_RET || _tt->op == OP_BR ||
                      _tt->op == OP_CONDBR))) {
          Instr *br_then = calloc(1, sizeof(Instr));
          br_then->id = ctx->next_instr_id++;
          br_then->op = OP_BR;
          br_then->src[0] = merge_blk;
          br_then->n_src = 1;
          br_then->exec_freq = 1.0;
          emit_instr(ctx, br_then);
          fn->blocks[ctx->cur_block]->succs[0] = merge_blk;
          fn->blocks[ctx->cur_block]->n_succs = 1;
          fn->blocks[merge_blk]->preds[fn->blocks[merge_blk]->n_preds++] =
              ctx->cur_block;
        }
      }
    }
    ctx->cur_block = merge_blk;
    return;
  }
  case ZND_WHILE: {
    BlockID head = new_block(ctx, "while.head");
    BlockID body_blk = new_block(ctx, "while.body");
    BlockID exit_blk = new_block(ctx, "while.exit");
    Block *cur = fn->blocks[ctx->cur_block];
    Instr *br = calloc(1, sizeof(Instr));
    br->id = ctx->next_instr_id++;
    br->op = OP_BR;
    br->src[0] = head;
    br->n_src = 1;
    br->exec_freq = 1.0;
    emit_instr(ctx, br);
    cur->succs[0] = head;
    cur->n_succs = 1;
    fn->blocks[head]->preds[0] = ctx->cur_block;
    fn->blocks[head]->n_preds = 1;

    ctx->cur_block = head;
    RegID cond_r = zcc_lower_expr(ctx, node->cond);
    Instr *cbr = calloc(1, sizeof(Instr));
    cbr->id = ctx->next_instr_id++;
    cbr->op = OP_CONDBR;
    cbr->dst = 0;
    cbr->src[0] = cond_r;
    cbr->src[1] = body_blk;
    cbr->src[2] = exit_blk;
    cbr->n_src = 3;
    cbr->exec_freq = 1.0;
    emit_instr(ctx, cbr);
    fn->blocks[head]->succs[0] = body_blk;
    fn->blocks[head]->succs[1] = exit_blk;
    fn->blocks[head]->n_succs = 2;
    fn->blocks[head]->branch_prob[0] = 0.9;
    fn->blocks[head]->branch_prob[1] = 0.1;
    fn->blocks[body_blk]->preds[0] = head;
    fn->blocks[body_blk]->n_preds = 1;
    fn->blocks[exit_blk]->preds[0] = head;
    fn->blocks[exit_blk]->n_preds = 1;

    ctx->loop_exit_stack[ctx->loop_depth] = exit_blk;
    ctx->loop_latch_stack[ctx->loop_depth] =
        head; /* continue → re-evaluate condition */
    ctx->loop_depth++;

    ctx->cur_block = body_blk;
    zcc_lower_stmt(ctx, node->body);
    {
      Block *_bb = fn->blocks[ctx->cur_block];
      Instr *_bt = _bb ? _bb->tail : NULL;
      if (!(_bt &&
            (_bt->op == OP_RET || _bt->op == OP_BR || _bt->op == OP_CONDBR))) {
        Instr *br_back = calloc(1, sizeof(Instr));
        br_back->id = ctx->next_instr_id++;
        br_back->op = OP_BR;
        br_back->src[0] = head;
        br_back->n_src = 1;
        br_back->exec_freq = 1.0;
        emit_instr(ctx, br_back);
        fn->blocks[ctx->cur_block]->succs[0] = head;
        fn->blocks[ctx->cur_block]->n_succs = 1;
        fn->blocks[head]->preds[fn->blocks[head]->n_preds++] = ctx->cur_block;
      }
    }

    ctx->loop_depth--;

    ctx->cur_block = exit_blk;
    return;
  }
  case ZND_FOR: {
    /* Five-block CFG: preheader (init) -> header (cond) -> body -> latch (inc)
     * -> header; header false -> exit. */
    BlockID preheader = new_block(ctx, "for.preheader");
    BlockID header = new_block(ctx, "for.head");
    BlockID body_blk = new_block(ctx, "for.body");
    BlockID latch = new_block(ctx, "for.latch");
    BlockID exit_blk = new_block(ctx, "for.exit");
    Block *cur = fn->blocks[ctx->cur_block];

    /* Current block branches to preheader */
    Instr *br = calloc(1, sizeof(Instr));
    br->id = ctx->next_instr_id++;
    br->op = OP_BR;
    br->src[0] = preheader;
    br->n_src = 1;
    br->exec_freq = 1.0;
    emit_instr(ctx, br);
    cur->succs[0] = preheader;
    cur->n_succs = 1;
    fn->blocks[preheader]->preds[0] = ctx->cur_block;
    fn->blocks[preheader]->n_preds = 1;

    /* Preheader: run init, then branch to header (LICM hoists invariants here)
     */
    ctx->cur_block = preheader;
    if (node->init)
      zcc_lower_stmt(ctx, node->init);
    Instr *br_ph = calloc(1, sizeof(Instr));
    br_ph->id = ctx->next_instr_id++;
    br_ph->op = OP_BR;
    br_ph->src[0] = header;
    br_ph->n_src = 1;
    br_ph->exec_freq = 1.0;
    emit_instr(ctx, br_ph);
    fn->blocks[preheader]->succs[0] = header;
    fn->blocks[preheader]->n_succs = 1;
    fn->blocks[header]->preds[fn->blocks[header]->n_preds++] = preheader;

    /* Header: evaluate cond; true -> body, false -> exit */
    ctx->cur_block = header;
    RegID cond_r = 0;
    if (node->cond) {
      cond_r = zcc_lower_expr(ctx, node->cond);
    } else {
      cond_r = ctx->next_reg++;
      Instr *one = make_instr_imm(ctx->next_instr_id++, OP_CONST, cond_r, 1,
                                  node->line_no);
      emit_instr(ctx, one);
    }
    Instr *cbr = calloc(1, sizeof(Instr));
    cbr->id = ctx->next_instr_id++;
    cbr->op = OP_CONDBR;
    cbr->dst = 0;
    cbr->src[0] = cond_r;
    cbr->src[1] = body_blk;
    cbr->src[2] = exit_blk;
    cbr->n_src = 3;
    cbr->exec_freq = 1.0;
    emit_instr(ctx, cbr);
    fn->blocks[header]->succs[0] = body_blk;
    fn->blocks[header]->succs[1] = exit_blk;
    fn->blocks[header]->n_succs = 2;
    fn->blocks[header]->branch_prob[0] = 0.9;
    fn->blocks[header]->branch_prob[1] = 0.1;
    fn->blocks[body_blk]->preds[0] = header;
    fn->blocks[body_blk]->n_preds = 1;
    fn->blocks[exit_blk]->preds[fn->blocks[exit_blk]->n_preds++] = header;

    ctx->loop_exit_stack[ctx->loop_depth] = exit_blk;
    ctx->loop_latch_stack[ctx->loop_depth] =
        latch; /* continue → run increment then re-evaluate */
    ctx->loop_depth++;

    /* Body: loop statements, then branch to latch */
    ctx->cur_block = body_blk;
    zcc_lower_stmt(ctx, node->body);
    {
      Block *_bb = fn->blocks[ctx->cur_block];
      Instr *_bt = _bb ? _bb->tail : NULL;
      if (!(_bt &&
            (_bt->op == OP_RET || _bt->op == OP_BR || _bt->op == OP_CONDBR))) {
        Instr *br_body = calloc(1, sizeof(Instr));
        br_body->id = ctx->next_instr_id++;
        br_body->op = OP_BR;
        br_body->src[0] = latch;
        br_body->n_src = 1;
        br_body->exec_freq = 1.0;
        emit_instr(ctx, br_body);
        fn->blocks[ctx->cur_block]->succs[0] = latch;
        fn->blocks[ctx->cur_block]->n_succs = 1;
        fn->blocks[latch]->preds[fn->blocks[latch]->n_preds++] = ctx->cur_block;
      }
    }

    /* Latch: run inc, then branch back to header */
    ctx->cur_block = latch;
    if (node->inc) {
      (void)zcc_lower_expr(
          ctx, node->inc); /* inc has side effect (e.g. i++) or is unused */
    }
    Instr *br_latch = calloc(1, sizeof(Instr));
    br_latch->id = ctx->next_instr_id++;
    br_latch->op = OP_BR;
    br_latch->src[0] = header;
    br_latch->n_src = 1;
    br_latch->exec_freq = 1.0;
    emit_instr(ctx, br_latch);
    fn->blocks[latch]->succs[0] = header;
    fn->blocks[latch]->n_succs = 1;
    fn->blocks[header]->preds[fn->blocks[header]->n_preds++] = latch;

    ctx->loop_depth--;

    ctx->cur_block = exit_blk;
    return;
  }
  case ZND_BREAK: {
    if (ctx->loop_depth == 0)
      return;
    BlockID target = ctx->loop_exit_stack[ctx->loop_depth - 1];
    Instr *br = calloc(1, sizeof(Instr));
    br->id = ctx->next_instr_id++;
    br->op = OP_BR;
    br->src[0] = target;
    br->n_src = 1;
    br->exec_freq = 1.0;
    emit_instr(ctx, br);
    fn->blocks[ctx->cur_block]->succs[0] = target;
    fn->blocks[ctx->cur_block]->n_succs = 1;
    fn->blocks[target]->preds[fn->blocks[target]->n_preds++] = ctx->cur_block;
    ctx->cur_block = new_block(ctx, "unreachable.after.break");
    return;
  }
  case ZND_CONTINUE: {
    if (ctx->loop_depth == 0)
      return;
    BlockID target = ctx->loop_latch_stack[ctx->loop_depth - 1];
    Instr *br = calloc(1, sizeof(Instr));
    br->id = ctx->next_instr_id++;
    br->op = OP_BR;
    br->src[0] = target;
    br->n_src = 1;
    br->exec_freq = 1.0;
    emit_instr(ctx, br);
    fn->blocks[ctx->cur_block]->succs[0] = target;
    fn->blocks[ctx->cur_block]->n_succs = 1;
    fn->blocks[target]->preds[fn->blocks[target]->n_preds++] = ctx->cur_block;
    ctx->cur_block = new_block(ctx, "unreachable.after.continue");
    return;
  }
  case ZND_SWITCH: {
    int n = node->num_cases;
    BlockID exit_blk = new_block(ctx, "switch.exit");
    BlockID default_blk =
        node->default_body ? new_block(ctx, "switch.default") : exit_blk;
    RegID cond_r = node->cond ? zcc_lower_expr(ctx, node->cond) : 0;
    if (n > 0 && !cond_r) {
      ctx->cur_block = exit_blk;
      return;
    }
    if (n == 0) {
      Instr *br = calloc(1, sizeof(Instr));
      br->id = ctx->next_instr_id++;
      br->op = OP_BR;
      br->src[0] = default_blk;
      br->n_src = 1;
      br->exec_freq = 1.0;
      emit_instr(ctx, br);
      fn->blocks[ctx->cur_block]->succs[0] = default_blk;
      fn->blocks[ctx->cur_block]->n_succs = 1;
      fn->blocks[default_blk]->preds[fn->blocks[default_blk]->n_preds++] =
          ctx->cur_block;
      ctx->cur_block = default_blk;
    }
    for (int i = 0; i < n; i++) {
      BlockID case_blk = new_block(ctx, "switch.case");
      BlockID next_blk =
          (i == n - 1) ? default_blk : new_block(ctx, "switch.cmp");
      RegID case_val_r = ctx->next_reg++;
      Instr *cst = make_instr_imm(ctx->next_instr_id++, OP_CONST, case_val_r,
                                  node->case_vals[i], node->line_no);
      emit_instr(ctx, cst);
      RegID eq_r = ctx->next_reg++;
      Instr *eq_ins = calloc(1, sizeof(Instr));
      eq_ins->id = ctx->next_instr_id++;
      eq_ins->op = OP_EQ;
      eq_ins->dst = eq_r;
      eq_ins->src[0] = cond_r;
      eq_ins->src[1] = case_val_r;
      eq_ins->n_src = 2;
      eq_ins->exec_freq = 1.0;
      emit_instr(ctx, eq_ins);
      Instr *cbr = calloc(1, sizeof(Instr));
      cbr->id = ctx->next_instr_id++;
      cbr->op = OP_CONDBR;
      cbr->dst = 0;
      cbr->src[0] = eq_r;
      cbr->src[1] = case_blk;
      cbr->src[2] = next_blk;
      cbr->n_src = 3;
      cbr->exec_freq = 1.0;
      emit_instr(ctx, cbr);
      fn->blocks[ctx->cur_block]->succs[0] = case_blk;
      fn->blocks[ctx->cur_block]->succs[1] = next_blk;
      fn->blocks[ctx->cur_block]->n_succs = 2;
      fn->blocks[ctx->cur_block]->branch_prob[0] = 0.5f;
      fn->blocks[ctx->cur_block]->branch_prob[1] = 0.5f;
      fn->blocks[case_blk]->preds[fn->blocks[case_blk]->n_preds++] =
          ctx->cur_block;
      fn->blocks[next_blk]->preds[fn->blocks[next_blk]->n_preds++] =
          ctx->cur_block;
      ctx->cur_block = case_blk;
      if (node->case_bodies && node->case_bodies[i])
        zcc_lower_stmt(ctx, node->case_bodies[i]);
      Instr *br_exit = calloc(1, sizeof(Instr));
      br_exit->id = ctx->next_instr_id++;
      br_exit->op = OP_BR;
      br_exit->src[0] = exit_blk;
      br_exit->n_src = 1;
      br_exit->exec_freq = 1.0;
      emit_instr(ctx, br_exit);
      fn->blocks[case_blk]->succs[0] = exit_blk;
      fn->blocks[case_blk]->n_succs = 1;
      fn->blocks[exit_blk]->preds[fn->blocks[exit_blk]->n_preds++] = case_blk;
      ctx->cur_block = next_blk;
    }
    if (node->default_body) {
      ctx->cur_block = default_blk;
      zcc_lower_stmt(ctx, node->default_body);
      Instr *br_def = calloc(1, sizeof(Instr));
      br_def->id = ctx->next_instr_id++;
      br_def->op = OP_BR;
      br_def->src[0] = exit_blk;
      br_def->n_src = 1;
      br_def->exec_freq = 1.0;
      emit_instr(ctx, br_def);
      fn->blocks[default_blk]->succs[0] = exit_blk;
      fn->blocks[default_blk]->n_succs = 1;
      fn->blocks[exit_blk]->preds[fn->blocks[exit_blk]->n_preds++] =
          default_blk;
    }
    ctx->cur_block = exit_blk;
    return;
  }
  case ZND_RETURN: {
    RegID val_r = zcc_lower_expr(ctx, node->lhs);
    Instr *ret = calloc(1, sizeof(Instr));
    ret->id = ctx->next_instr_id++;
    ret->op = OP_RET;
    ret->dst = 0;
    ret->src[0] = val_r;
    ret->n_src = 1;
    ret->exec_freq = 1.0;
    emit_instr(ctx, ret);
    fn->blocks[ctx->cur_block]->n_succs = 0;
    return;
  }
  case ZND_BLOCK:
    for (uint32_t i = 0; i < node->num_stmts; i++)
      zcc_lower_stmt(ctx, node->stmts[i]);
    return;
  default:
    zcc_lower_expr(ctx, node);
    return;
  }
}

/**
 * zcc_ast_to_ir() — Build Function* from ZCC-shaped AST (one function body).
 * Same contract as ast_to_ir(); entry and exit blocks created here.
 * func_name: if "main", allocas for "argc" and "argv" are created in the entry
 * so they get param slots.
 */
Function *zcc_ast_to_ir(ZCCNode *body_ast, const char *func_name) {
  Function *fn = calloc(1, sizeof(Function));
  LowerCtx ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.fn = fn;
  ctx.next_reg = 10;
  ctx.next_instr_id = 1;

  BlockID entry = new_block(&ctx, "entry");
  BlockID exit_blk = new_block(&ctx, "exit");
  fn->entry = entry;
  fn->exit = exit_blk;
  ctx.cur_block = entry;

  /* Root cause fix: for main, create param allocas in the entry so they get
   * slots -8, -16. Prologue stores argc→-8, argv→-16; later uses (e.g. argv in
   * for.body) then load the value. */
  if (is_main_func(func_name)) {
    (void)get_or_create_var(&ctx, "argc", 8);
    (void)get_or_create_var(&ctx, "argv", 8);
  } else {
    const char *params_env = getenv("ZCC_IR_PARAM_NAMES");
    fprintf(stderr, "[ZCC-IR] DEBUG: params_env=%s\n",
            params_env ? params_env : "NULL");
    if (params_env && params_env[0]) {
      char env_buf[512];
      strncpy(env_buf, params_env, sizeof(env_buf) - 1);
      env_buf[sizeof(env_buf) - 1] = '\0';

      char *spt = env_buf;
      while (*spt) {
        char *comma = strchr(spt, ',');
        if (comma)
          *comma = '\0';
        (void)get_or_create_var(&ctx, spt, 8);
        if (!comma)
          break;
        spt = comma + 1;
      }
    }
  }

  zcc_lower_stmt(&ctx, body_ast);

  Block *cur = fn->blocks[ctx.cur_block];
  if (cur->n_succs == 0 && cur->tail && cur->tail->op != OP_RET) {
    Instr *br = calloc(1, sizeof(Instr));
    br->id = ctx.next_instr_id++;
    br->op = OP_BR;
    br->src[0] = exit_blk;
    br->n_src = 1;
    br->exec_freq = 1.0;
    emit_instr(&ctx, br);
    cur->succs[0] = exit_blk;
    cur->n_succs = 1;
    fn->blocks[exit_blk]->preds[fn->blocks[exit_blk]->n_preds++] =
        ctx.cur_block;
  }

  return fn;
}

#ifdef ZCC_BRIDGE_STANDALONE
/* Build ZCC-shaped AST for: int main() { int i = 0; while (i < 10) { i = i + 1;
 * } return i; } */
static ZCCNode *build_zcc_phase_b_ast(void) {
  static ZCCNode pool[32];
  static ZCCNode *stmts[ZCC_AST_MAX_STMTS];
  static ZCCNode *body_stmts[1];
  memset(pool, 0, sizeof(pool));
  int p = 0;
#define ZN() (&pool[p++])

  ZCCNode *zero = ZN();
  zero->kind = ZND_NUM;
  zero->int_val = 0;
  ZCCNode *ten = ZN();
  ten->kind = ZND_NUM;
  ten->int_val = 10;
  ZCCNode *one = ZN();
  one->kind = ZND_NUM;
  one->int_val = 1;
  ZCCNode *var_i = ZN();
  var_i->kind = ZND_VAR;
  strncpy(var_i->name, "i", NAME_LEN - 1);
  ZCCNode *i_lt_10 = ZN();
  i_lt_10->kind = ZND_LT;
  i_lt_10->lhs = var_i;
  i_lt_10->rhs = ten;
  ZCCNode *i_plus_1 = ZN();
  i_plus_1->kind = ZND_ADD;
  i_plus_1->lhs = var_i;
  i_plus_1->rhs = one;
  ZCCNode *lhs_i = ZN();
  lhs_i->kind = ZND_VAR;
  strncpy(lhs_i->name, "i", NAME_LEN - 1);
  ZCCNode *assign_inc = ZN();
  assign_inc->kind = ZND_ASSIGN;
  assign_inc->lhs = lhs_i;
  assign_inc->rhs = i_plus_1;
  ZCCNode *body_while = ZN();
  body_while->kind = ZND_BLOCK;
  body_stmts[0] = assign_inc;
  body_while->stmts = body_stmts;
  body_while->num_stmts = 1;
  ZCCNode *while_loop = ZN();
  while_loop->kind = ZND_WHILE;
  while_loop->cond = i_lt_10;
  while_loop->body = body_while;
  ZCCNode *init_i = ZN();
  init_i->kind = ZND_ASSIGN;
  init_i->lhs = ZN();
  init_i->lhs->kind = ZND_VAR;
  strncpy(init_i->lhs->name, "i", NAME_LEN - 1);
  init_i->rhs = zero;
  ZCCNode *ret_i = ZN();
  ret_i->kind = ZND_RETURN;
  ret_i->lhs = var_i;
  stmts[0] = init_i;
  stmts[1] = while_loop;
  stmts[2] = ret_i;
  ZCCNode *block = ZN();
  block->kind = ZND_BLOCK;
  block->stmts = stmts;
  block->num_stmts = 3;
  return block;
#undef ZN
}
#endif /* ZCC_BRIDGE_STANDALONE */

/**
 * ast_to_ir() — Build Function* from minimal AST (one function, scalars only).
 * Caller provides AST for function body; entry and exit blocks are created.
 */
Function *ast_to_ir(ASTNode *body_ast) {
  Function *fn = calloc(1, sizeof(Function));
  LowerCtx ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.fn = fn;
  ctx.next_reg = 10;
  ctx.next_instr_id = 1;

  BlockID entry = new_block(&ctx, "entry");
  BlockID exit_blk = new_block(&ctx, "exit");
  fn->entry = entry;
  fn->exit = exit_blk;
  ctx.cur_block = entry;

  lower_stmt(&ctx, body_ast);

  /* If current block doesn't end with ret/br, branch to exit */
  Block *cur = fn->blocks[ctx.cur_block];
  if (cur->n_succs == 0 && cur->tail && cur->tail->op != OP_RET) {
    Instr *br = calloc(1, sizeof(Instr));
    br->id = ctx.next_instr_id++;
    br->op = OP_BR;
    br->src[0] = exit_blk;
    br->n_src = 1;
    br->exec_freq = 1.0;
    emit_instr(&ctx, br);
    cur->succs[0] = exit_blk;
    cur->n_succs = 1;
    fn->blocks[exit_blk]->preds[fn->blocks[exit_blk]->n_preds++] =
        ctx.cur_block;
  }

  return fn;
}

#ifdef ZCC_BRIDGE_STANDALONE
/* Build AST for: int main() { int i = 0; while (i < 10) { i = i + 1; } return
 * i; } */
static ASTNode *build_phase_b_ast(void) {
  static ASTNode pool[32];
  static ASTNode *stmts[AST_MAX_STMTS];
  static ASTNode *body_stmts[1];
  memset(pool, 0, sizeof(pool));
  int p = 0;
#define N() (&pool[p++])

  ASTNode *zero = N();
  zero->kind = AST_NUM;
  zero->num_val = 0;
  ASTNode *ten = N();
  ten->kind = AST_NUM;
  ten->num_val = 10;
  ASTNode *one = N();
  one->kind = AST_NUM;
  one->num_val = 1;
  ASTNode *var_i = N();
  var_i->kind = AST_VAR;
  strcpy(var_i->var_name, "i");
  ASTNode *i_lt_10 = N();
  i_lt_10->kind = AST_LT;
  i_lt_10->lhs = var_i;
  i_lt_10->rhs = ten;
  ASTNode *i_plus_1 = N();
  i_plus_1->kind = AST_ADD;
  i_plus_1->lhs = var_i;
  i_plus_1->rhs = one;
  ASTNode *lhs_i = N();
  lhs_i->kind = AST_VAR;
  strcpy(lhs_i->var_name, "i");
  ASTNode *assign_inc = N();
  assign_inc->kind = AST_ASSIGN;
  assign_inc->lhs = lhs_i;
  assign_inc->rhs = i_plus_1;
  ASTNode *body_while = N();
  body_while->kind = AST_BLOCK;
  body_stmts[0] = assign_inc;
  body_while->stmts = body_stmts;
  body_while->n_stmts = 1;
  ASTNode *while_loop = N();
  while_loop->kind = AST_WHILE;
  while_loop->cond = i_lt_10;
  while_loop->body = body_while;
  ASTNode *init_i = N();
  init_i->kind = AST_ASSIGN;
  init_i->lhs = N();
  init_i->lhs->kind = AST_VAR;
  strcpy(init_i->lhs->var_name, "i");
  init_i->rhs = zero;
  ASTNode *ret_i = N();
  ret_i->kind = AST_RETURN;
  ret_i->lhs = var_i;
  stmts[0] = init_i;
  stmts[1] = while_loop;
  stmts[2] = ret_i;
  ASTNode *block = N();
  block->kind = AST_BLOCK;
  block->stmts = stmts;
  block->n_stmts = 3;
  return block;
#undef N
}
#endif /* ZCC_BRIDGE_STANDALONE */

/* ─────────────────────────────────────────────────────────────────────────────
 * PASS PIPELINE: run all three passes in order
 * ──────────────────────────────────────────────────────────────── */

/* ── IR Vulnerability Tag Schema Integration ─────────────────────────────────
 *
 * ZCC supports a defensive security-analysis tag schema for IR nodes via
 * ir_vuln_tag.h / ir_vuln_tag.c.  Tags are stored as bitmasks in the
 * `vuln_tags` field of ir_node_t (see ir.h).
 *
 * The pass pipeline here operates on the internal Function/Instr IR which is
 * separate from ir_node_t.  Future liveness, dominance, and RegisterWarden
 * passes that consume ir_node_t directly can integrate via:
 *
 *   #include "ir_vuln_tag.h"
 *
 *   // Query a node:
 *   if (ir_vuln_tag_has(node, IR_VULN_UNTRUSTED_CALL)) { ... }
 *   if (ir_vuln_tag_has(node, IR_VULN_DELEGATE_CALL | IR_VULN_PRIV_BOUNDARY))
 *       { ... }
 *
 *   // Run the standalone module-level scanner (read-only):
 *   ir_pass_vuln_scan(ir_module, stderr);
 *
 * Tag schema (bitmask, defined in ir_vuln_tag.h):
 *   IR_VULN_UNTRUSTED_CALL   — untrusted external call
 *   IR_VULN_DELEGATE_CALL    — delegate call (caller context hazard)
 *   IR_VULN_STATIC_CALL      — static/read-only call
 *   IR_VULN_STATE_WRITE      — persistent state mutation
 *   IR_VULN_PRIV_BOUNDARY    — privilege boundary crossing
 *   IR_VULN_UNKNOWN          — unclassified security event
 *   IR_VULN_SELFDESTRUCT     — destructive termination
 *   IR_VULN_CONTRACT_CREATE  — new contract deployment
 *   IR_VULN_EXEC_BARRIER     — execution barrier (REVERT/INVALID)
 *
 * NOTE: Do not include ir_vuln_tag.h in this file — IR_TY_* names conflict
 * with this TU's IRType enum.  The schema lives entirely in ir_vuln_tag.c
 * and is linked separately.
 * ─────────────────────────────────────────────────────────────────────────── */

/**
 * run_all_passes() — Execute the full three-pass optimization pipeline.
 *
 * Pass ordering rationale:
 *   1. DCE first: removes dead instructions, reducing the points-to graph
 *      that escape analysis must traverse.
 *   2. Escape analysis second: promotes stack allocations, which may expose
 *      additional dead stores (a second DCE run in a production pipeline
 *      would clean these up).
 *   3. PGO reordering last: operates on the pruned CFG, so cold blocks
 *      already removed by DCE do not pollute the frequency ordering.
 *
 * @param fn      Function IR (modified in-place by passes 1 and 2).
 * @param result  Output reordering from pass 3; licm_hoisted and
 * dce_instrs_removed filled from fn->stats.
 */

/**
 * ir_validate() — Phase B entry contract. Ensures Function* satisfies
 * invariants required by DCE, LICM, escape analysis, and PGO.
 * Returns true if valid; on failure, prints diagnostics to stderr and returns
 * false.
 */
static bool ir_validate(const Function *fn) {
  bool ok = true;
  if (!fn || fn->n_blocks == 0)
    return false;

  /* Invariant 4: Entry block has no predecessors. */
  BlockID entry = fn->entry;
  if (entry >= fn->n_blocks) {
    fprintf(stderr, "[ir_validate] entry block id %u >= n_blocks %u\n",
            (unsigned)entry, fn->n_blocks);
    ok = false;
  } else if (fn->blocks[entry]->n_preds != 0) {
    fprintf(stderr,
            "[ir_validate] entry block has %u predecessors (must be 0)\n",
            fn->blocks[entry]->n_preds);
    ok = false;
  }

  for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
    const Block *blk = fn->blocks[bi];
    if (!blk)
      continue;

    /* Invariant 3: Branch probabilities on CONDBR blocks sum to 1.0. */
    if (blk->n_succs > 1) {
      double sum = 0.0;
      for (uint32_t si = 0; si < blk->n_succs; si++)
        sum += blk->branch_prob[si];
      if (sum < 0.99 || sum > 1.01) {
        fprintf(stderr,
                "[ir_validate] block %u (%s) branch_prob sum = %.4f (expected "
                "1.0)\n",
                bi, blk->name, sum);
        ok = false;
      }
    }

    for (const Instr *ins = blk->head; ins; ins = ins->next) {
      /* Invariant 1: Every used RegID is in range; def_of[r] may be NULL for
       * args. */
      for (uint32_t s = 0; s < ins->n_src; s++) {
        RegID r = ins->src[s];
        if (r != 0 && r >= MAX_INSTRS) {
          fprintf(
              stderr,
              "[ir_validate] block %u instr %u src[%u] reg %u >= MAX_INSTRS\n",
              bi, (unsigned)ins->id, s, (unsigned)r);
          ok = false;
        }
      }
      if (ins->op == OP_PHI) {
        for (uint32_t p = 0; p < ins->n_phi; p++) {
          RegID r = ins->phi[p].reg;
          if (r != 0 && r >= MAX_INSTRS) {
            fprintf(stderr,
                    "[ir_validate] block %u phi instr %u phi[%u] reg %u >= "
                    "MAX_INSTRS\n",
                    bi, (unsigned)ins->id, p, (unsigned)r);
            ok = false;
          }
        }
      }
      if (ins->op == OP_CALL) {
        for (uint32_t s = 0; s < ins->n_call_args; s++) {
          RegID r = ins->call_args[s];
          if (r != 0 && r >= MAX_INSTRS) {
            fprintf(stderr,
                    "[ir_validate] block %u instr %u call_args[%u] reg %u >= "
                    "MAX_INSTRS\n",
                    bi, (unsigned)ins->id, s, (unsigned)r);
            ok = false;
          }
        }
      }

      /* Invariant 2: Every OP_PHI lists exactly the predecessor blocks of its
       * block. */
      if (ins->op == OP_PHI) {
        if (ins->n_phi != blk->n_preds) {
          fprintf(stderr,
                  "[ir_validate] block %u phi instr %u has n_phi=%u but block "
                  "has n_preds=%u\n",
                  bi, (unsigned)ins->id, ins->n_phi, blk->n_preds);
          ok = false;
        } else {
          for (uint32_t pi = 0; pi < blk->n_preds; pi++) {
            BlockID pred = blk->preds[pi];
            bool found = false;
            for (uint32_t p = 0; p < ins->n_phi; p++)
              if (ins->phi[p].block == pred) {
                found = true;
                break;
              }
            if (!found) {
              fprintf(stderr,
                      "[ir_validate] block %u phi instr %u has no source for "
                      "predecessor %u\n",
                      bi, (unsigned)ins->id, (unsigned)pred);
              ok = false;
            }
          }
        }
      }
    }
  }

  return ok;
}

/* Apply PGO profile: for each block named in the file, set branch_prob[0] and
 * [1]. File format: one line per 2-way branch block: "block_name p0 p1" (e.g.
 * "for.head 0.9 0.1"). Lines starting with # are comments. p0+p1 should be 1.0;
 * if not, we normalize. */
static void apply_profile_to_function(Function *fn, const char *path) {
  FILE *f = fopen(path, "r");
  if (!f) {
    fprintf(stderr, "[PGO] cannot open profile '%s'\n", path);
    return;
  }
  char line[256];
  unsigned applied = 0;
  while (fgets(line, sizeof(line), f)) {
    if (line[0] == '#' || line[0] == '\n' || line[0] == '\0')
      continue;
    char name[NAME_LEN];
    double p0 = 0.0, p1 = 0.0;
    if (sscanf(line, "%63s %lf %lf", name, &p0, &p1) < 3)
      continue;
    for (BlockID bi = 0; bi < fn->n_blocks; bi++) {
      Block *blk = fn->blocks[bi];
      if (!blk || blk->n_succs != 2)
        continue;
      if (strcmp(blk->name, name) != 0)
        continue;
      if (p0 < 0.0)
        p0 = 0.0;
      if (p1 < 0.0)
        p1 = 0.0;
      {
        double s = p0 + p1;
        if (s > 0.0) {
          p0 /= s;
          p1 /= s;
        } else {
          p0 = 0.5;
          p1 = 0.5;
        }
      }
      blk->branch_prob[0] = p0;
      blk->branch_prob[1] = p1;
      applied++;
      break;
    }
  }
  fclose(f);
  if (applied)
    fprintf(stderr, "[PGO] applied profile '%s' (%u block(s))\n", path,
            applied);
}

/* ── PGO instrumentation pass: inject block execution counter probes ──
 * Runs before DCE/LICM so we capture original control flow.
 * When ZCC_PGO_INSTRUMENT=1, inserts at the head of every block:
 *   addr = OP_PGO_COUNTER_ADDR block_id; count = LOAD addr; count++; STORE
 * count, addr. Counter array __zcc_edge_counts[] and atexit dump are emitted by
 * codegen/linker. */
static uint32_t pgo_instrument_pass(Function *fn) {
  RegID Raddr = fn->n_regs;
  RegID Rcount = fn->n_regs + 1;
  RegID Rone = fn->n_regs + 2;
  RegID Rnew = fn->n_regs + 3;
  fn->n_regs += 4;

  InstrID next_id = 0;
  for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
    Block *blk = fn->blocks[bi];
    if (!blk)
      continue;
    for (Instr *p = blk->head; p; p = p->next)
      if (p->id >= next_id)
        next_id = (InstrID)(p->id + 1);
  }

  uint32_t injected = 0;
  for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
    Block *blk = fn->blocks[bi];
    if (!blk)
      continue;

    Instr *addr_ins = make_instr_imm(next_id++, OP_PGO_COUNTER_ADDR, Raddr,
                                     (int64_t)(uint32_t)bi, 0);
    Instr *load_ins = calloc(1, sizeof(Instr));
    load_ins->id = next_id++;
    load_ins->op = OP_LOAD;
    load_ins->dst = Rcount;
    load_ins->src[0] = Raddr;
    load_ins->n_src = 1;
    load_ins->imm = 8;
    load_ins->exec_freq = 1.0;
    Instr *one_ins = make_instr_imm(next_id++, OP_CONST, Rone, 1, 0);
    Instr *add_ins = calloc(1, sizeof(Instr));
    add_ins->id = next_id++;
    add_ins->op = OP_ADD;
    add_ins->dst = Rnew;
    add_ins->src[0] = Rcount;
    add_ins->src[1] = Rone;
    add_ins->n_src = 2;
    add_ins->exec_freq = 1.0;
    Instr *store_ins = calloc(1, sizeof(Instr));
    store_ins->id = next_id++;
    store_ins->op = OP_STORE;
    store_ins->dst = 0;
    store_ins->src[0] = Rnew;
    store_ins->src[1] = Raddr;
    store_ins->n_src = 2;
    store_ins->exec_freq = 1.0;

    addr_ins->next = load_ins;
    load_ins->prev = addr_ins;
    load_ins->next = one_ins;
    one_ins->prev = load_ins;
    one_ins->next = add_ins;
    add_ins->prev = one_ins;
    add_ins->next = store_ins;
    store_ins->prev = add_ins;
    store_ins->next = blk->head;
    if (blk->head)
      blk->head->prev = store_ins;
    else
      blk->tail = store_ins;
    blk->head = addr_ins;
    blk->n_instrs += 5;
    injected += 5;
  }

  return injected;
}

void ir_dump_json(void *f_ptr, Function *fn, const char *func_name,
                  int is_first) {
  FILE *f = (FILE *)f_ptr;
  if (!f || !fn)
    return;
  if (!is_first)
    fprintf(f, ",\n");
  fprintf(f, "  {\n    \"function\": \"%s\",\n    \"blocks\": [\n",
          func_name ? func_name : "unknown");
  int first_blk = 1;
  for (uint32_t b = 0; b < fn->n_blocks; b++) {
    Block *blk = fn->blocks[b];
    if (!blk)
      continue;
    if (!first_blk)
      fprintf(f, ",\n");
    fprintf(f, "      {\n        \"id\": %u,\n        \"name\": \"%s\",\n",
            blk->id, blk->name);
    fprintf(f, "        \"instructions\": [\n");
    int first_ins = 1;
    for (Instr *ins = blk->head; ins; ins = ins->next) {
      if (!first_ins)
        fprintf(f, ",\n");
      const char *op_str =
          (unsigned)ins->op < sizeof(opcode_name) / sizeof(opcode_name[0])
              ? opcode_name[ins->op]
              : "UNKNOWN";
      fprintf(f, "          { \"id\": %u, \"op\": \"%s\", \"dst\": %u", ins->id,
              op_str, ins->dst);
      fprintf(f, ", \"srcs\": [");
      for (uint32_t i = 0; i < ins->n_src; i++) {
        if (i > 0)
          fprintf(f, ", ");
        fprintf(f, "%u", ins->src[i]);
      }
      fprintf(f, "]");
      if (ins->op == OP_CONST)
        fprintf(f, ", \"imm\": %lld", (long long)ins->imm);
      if (ins->call_name[0])
        fprintf(f, ", \"call_name\": \"%s\"", ins->call_name);
      fprintf(f, " }");
      first_ins = 0;
    }
    fprintf(f, "\n        ]\n      }");
    first_blk = 0;
  }
  fprintf(f, "\n    ]\n  }");
  fflush(f);
}

void zcc_ir_bridge_dump_and_free(ZCCNode *z_node, const char *func_name,
                                 void *dump_f, int is_first) {
  if (!z_node)
    return;
  Function *fn_ir = zcc_ast_to_ir(z_node, func_name);
  if (fn_ir) {
    PassResult *pr = (PassResult *)calloc(1, sizeof(PassResult));
    if (pr) {
      run_all_passes(fn_ir, pr, NULL, 0);
      free(pr);
    }
    if (dump_f) {
      ir_dump_json(dump_f, fn_ir, func_name, is_first);
    }
    zcc_ir_free(fn_ir);
  }
  zcc_node_free(z_node);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * REDUNDANT LOAD ELIMINATION (RLE)
 *
 * Per basic block, tracks which address vregs have already been loaded.
 * If the same address is loaded again with no intervening STORE to that
 * address (or CALL, which may write anywhere), the second LOAD is replaced
 * with a COPY from the first load's result register.
 *
 * STORE handling:
 *   - STORE to a known ALLOCA (stack slot): only invalidates that address
 *     (ALLOCAs are guaranteed non-aliasing)
 *   - STORE to anything else: invalidates all tracked loads (conservative)
 *
 * CALL handling: invalidates all tracked loads.
 *
 * After this pass, run copy propagation + DCE to clean up the COPYs and
 * any instructions that fed the now-dead LOADs.
 * ───────────────────────────────────────────────────────────────────────────
 */
static uint32_t redundant_load_elim_pass(Function *fn) {
  uint32_t eliminated = 0;

#define RLE_CAPACITY 512
  struct rle_entry {
    RegID addr;
    RegID val;
    int64_t size;
  };
  struct rle_entry avail[RLE_CAPACITY];

  for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
    Block *blk = fn->blocks[bi];
    if (!blk)
      continue;

    int n_avail = 0;

    for (Instr *ins = blk->head; ins; ins = ins->next) {
      if (ins->dead || ins->op == OP_NOP)
        continue;

      if (ins->op == OP_LOAD && ins->n_src >= 1 && ins->dst) {
        RegID addr = ins->src[0];
        int64_t sz = ins->imm;

        /* Search for a prior load from the same address + size */
        int found = -1;
        for (int i = 0; i < n_avail; i++) {
          if (avail[i].addr == addr && avail[i].size == sz) {
            found = i;
            break;
          }
        }

        if (found >= 0) {
          /* Redundant — replace with COPY from prior load result */
          ins->op = OP_COPY;
          ins->src[0] = avail[found].val;
          ins->n_src = 1;
          ins->imm = 0;
          eliminated++;
        } else {
          /* Record this load for future matches */
          if (n_avail < RLE_CAPACITY) {
            avail[n_avail].addr = addr;
            avail[n_avail].val = ins->dst;
            avail[n_avail].size = sz;
            n_avail++;
          }
        }
      } else if (ins->op == OP_STORE && ins->n_src >= 2) {
        RegID addr = ins->src[1]; /* STORE src[0]=value, src[1]=address */

        /* Check if the target is a known ALLOCA (non-aliasing stack slot) */
        bool is_alloca = (addr < MAX_INSTRS && fn->def_of[addr] &&
                          fn->def_of[addr]->op == OP_ALLOCA);

        if (is_alloca) {
          /* Invalidate only entries for this specific address */
          for (int i = 0; i < n_avail; i++) {
            if (avail[i].addr == addr) {
              avail[i] = avail[--n_avail];
              i--; /* re-check swapped entry */
            }
          }
        } else {
          /* Unknown alias — conservatively invalidate everything */
          n_avail = 0;
        }
      } else if (ins->op == OP_CALL) {
        /* Calls may write to any reachable memory */
        n_avail = 0;
      }
    } /* end instruction walk */
  } /* end block walk */

#undef RLE_CAPACITY
  return eliminated;
}

#include "zcc_ir_opt_passes.h"

void run_all_passes(Function *fn, PassResult *result, const char *profile_path,
                    int num_params) {
  if (!ir_validate(fn)) {
    fprintf(stderr,
            "[run_all_passes] IR validation failed; continuing anyway.\n");
  }

  fprintf(stderr, "=== PRE-OPT IR ===\n");
  for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
    Block *b = fn->blocks[bi];
    if (!b)
      continue;
    fprintf(stderr, "Block %u (preds: %u, succs: %u)\n", bi, b->n_preds,
            b->n_succs);
    for (Instr *ins = b->head; ins; ins = ins->next) {
      fprintf(stderr, "  ins %u: op=%d dst=%u src0=%u\n", (unsigned)ins->id,
              ins->op, ins->dst, ins->n_src > 0 ? ins->src[0] : 0);
    }
  }

  /* Prerequisite: reachability */
  compute_reachability(fn);

  /* CG-IR: Mark parameter allocas as escaping so they are not promoted by
   * Mem2Reg */
  if (num_params > 0) {
    int p_count = 0;
    Block *entry_blk = fn->blocks[fn->entry];
    if (entry_blk) {
      for (Instr *ins = entry_blk->head; ins; ins = ins->next) {
        if (ins->op == OP_ALLOCA) {
          if (p_count < num_params) {
            ins->escape = true;
            p_count++;
          }
        }
      }
    }
  }

  /* PGO instrumentation (before DCE/LICM): inject counter probes when requested
   */
  {
    const char *env = getenv("ZCC_PGO_INSTRUMENT");
    if (env && env[0] == '1') {
      uint32_t n = pgo_instrument_pass(fn);
      fprintf(stderr, "[PGO-Instr] injected %u probe instructions\n", n);
    }
  }

  /* Populate fn->def_of[] so DCE backward propagation can follow uses.
   * licm_build_def_block() does exactly this; call it before every DCE run. */
  licm_build_def_block(fn);

  /* ── Pass 0: Constant folding (exposes more dead code for DCE) ── */
  uint32_t folded = constant_fold_pass(fn);

  // NEW IR OPTIMIZATION PASSES
  uint32_t opt_sr = opt_strength_reduction_pass(fn);
  uint32_t opt_cp = opt_copy_prop_pass(fn);
  uint32_t opt_p = opt_peephole_pass(fn);
  uint32_t opt_cp2 = 0;
  if (opt_p > 0) {
    opt_cp2 = opt_copy_prop_pass(fn);
  }

  if (folded > 0 || opt_sr > 0 || opt_cp > 0 || opt_p > 0 || opt_cp2 > 0) {
    fprintf(
        stderr,
        "[IR-Opts] Folded: %u | S-Reduce: %u | Copy-Prop: %u | Peephole: %u | Copy-Prop2: %u\n",
        folded, opt_sr, opt_cp, opt_p, opt_cp2);
    licm_build_def_block(fn);
  }

  /* ── Pass 0b: Redundant Load Elimination ── */
  uint32_t rle_count = redundant_load_elim_pass(fn);
  if (rle_count > 0) {
    fprintf(stderr, "[RLE]       redundant loads eliminated: %u\n", rle_count);
    /* RLE produces COPYs — propagate them, then rebuild def chains */
    opt_copy_prop_pass(fn);
    licm_build_def_block(fn);
  }

  /* ── Pass 1: SSA-form DCE ── */
  uint32_t dce_removed = ssa_dce_pass(fn);
  uint32_t dce_total   = dce_removed;
#ifdef ZCC_DCE_VERBOSE
  fprintf(stderr, "[DCE->SSA]  instructions removed: %u  blocks removed: %u\n",
          dce_removed, fn->stats.dce_blocks_removed);
#endif

  /* Recompute reachability after DCE */
  compute_reachability(fn);

  /* ── Pass 2: LICM (Loop-Invariant Code Motion) ── */
  uint32_t licm_hoisted = licm_pass(fn);
  if (licm_hoisted > 0)
    fprintf(stderr, "[LICM]      instructions hoisted: %u\n", licm_hoisted);

  /* ── Pass 3: SSA-form DCE again (cleanup after LICM) ── */
  licm_build_def_block(fn);
  dce_removed = ssa_dce_pass(fn);
  dce_total  += dce_removed;
#ifdef ZCC_DCE_VERBOSE
  fprintf(stderr, "[DCE->SSA]  instructions removed: %u  blocks removed: %u\n",
          dce_removed, fn->stats.dce_blocks_removed);
#endif
  if (dce_total > 0)
    fprintf(stderr, "[DCE]  eliminated=%4u  (%u pre-LICM + %u post)\n",
            dce_total, dce_total - dce_removed, dce_removed);

  compute_reachability(fn);

  /* ── Pass 4: Escape Analysis (heap: large ctx; safe under deep ZCC stack) ──
   */
  EscapeCtx *ea_ctx = (EscapeCtx *)calloc(1, sizeof(EscapeCtx));
  if (!ea_ctx) {
    fprintf(stderr, "[run_all_passes] OOM escape ctx\n");
    return;
  }
  uint32_t promoted = escape_analysis_pass(fn, ea_ctx);
  fprintf(stderr,
          "[EscapeAna] allocations promoted to stack: %u  (of %u total)\n",
          promoted, ea_ctx->n_allocs);

  /* ── Pass 4b: Scalar promotion (mem2reg) — single-block allocas to vregs ──
   */
  uint32_t mem2reg_count = scalar_promotion_pass(fn, ea_ctx);
  if (mem2reg_count > 0) {
    fprintf(stderr, "[Mem2Reg]   single-block allocas promoted: %u\n",
            mem2reg_count);
    licm_build_def_block(fn);
    uint32_t dce_after = ssa_dce_pass(fn);
    fprintf(stderr,
            "[DCE->SSA]  instructions removed (after mem2reg): %u  blocks "
            "removed: %u\n",
            dce_after, fn->stats.dce_blocks_removed);
    compute_reachability(fn);
  }
  free(ea_ctx);

  /* ── Pass 5: PGO Basic Block Reordering ── */
  if (getenv("ZCC_DUMP_PGO_BLOCKS") &&
      getenv("ZCC_DUMP_PGO_BLOCKS")[0] == '1') {
    for (BlockID bi = 0; bi < fn->n_blocks; bi++) {
      Block *b = fn->blocks[bi];
      if (b && b->n_succs == 2)
        fprintf(stderr, "[PGO-BLOCK] %s n_succs=2 (profile-ready)\n", b->name);
    }
  }
  if (profile_path && profile_path[0])
    apply_profile_to_function(fn, profile_path);
  /* Optional: write current branch_probs for PGO round-trip. First function
   * truncates, rest append. */
  {
    const char *gen = getenv("ZCC_GEN_PROFILE");
    if (gen && gen[0]) {
      static int gen_profile_first = 1;
      FILE *pf = fopen(gen, gen_profile_first ? "w" : "a");
      if (pf) {
        gen_profile_first = 0;
        for (BlockID bi = 0; bi < fn->n_blocks; bi++) {
          Block *b = fn->blocks[bi];
          if (b && b->n_succs == 2)
            fprintf(pf, "%s %.4f %.4f\n", b->name, b->branch_prob[0],
                    b->branch_prob[1]);
        }
        fclose(pf);
      }
    }
  }
  result->n_blocks = pgo_reorder_pass(fn, result->order);
  result->licm_hoisted = fn->stats.licm_hoisted;
  result->dce_instrs_removed = fn->stats.dce_instrs_removed;
  for (uint32_t i = 0; i < result->n_blocks; i++) {
    BlockID bid = result->order[i];
    if (bid < fn->n_blocks && fn->blocks[bid]) {
      size_t len = strlen(fn->blocks[bid]->name);
      if (len >= ZCC_BLOCK_NAME_LEN)
        len = ZCC_BLOCK_NAME_LEN - 1;
      memcpy(result->block_names[i], fn->blocks[bid]->name, len);
      result->block_names[i][len] = '\0';
    } else
      result->block_names[i][0] = '\0';
  }
  fprintf(stderr, "[PGO-BBR]   blocks in emission order: %u\n",
          result->n_blocks);
}

/* ── Linear Scan Register Allocation ───────────────────────────────────────
 * Physical pool: %rbx, %r10, %r11, %r12, %r13, %r14, %r15 (avoid
 * %rax,%rcx,%rdx: div/shift). Callee-saved: rbx, r12, r13, r14, r15 → push in
 * prologue, pop before ret. */
#define N_PHYS_REGS 7
static const char *const phys_reg_name[N_PHYS_REGS] = {
    "rbx", "r10", "r11", "r12", "r13", "r14", "r15"};
#define PHYS_CALLEE_SAVED_MASK                                                 \
  ((1 << 0) | (1 << 3) | (1 << 4) | (1 << 5) | (1 << 6)) /* rbx, r12-r15 */
static const int callee_saved_push_order[5] = {0, 3, 4, 5,
                                               6}; /* rbx, r12..r15 */
#define CALLEE_SAVED_PUSH_N 5

typedef struct {
  RegID vreg;
  int start;
  int end;
  int phys_reg; /* 0..N_PHYS_REGS-1 or -1 if spilled */
} LiveInterval;

static int live_interval_compare(const void *a, const void *b) {
  const LiveInterval *ia = (const LiveInterval *)a;
  const LiveInterval *ib = (const LiveInterval *)b;
  if (ia->start != ib->start)
    return (ia->start > ib->start) ? 1 : -1;
  return (ia->end > ib->end) ? 1 : ((ia->end < ib->end) ? -1 : 0);
}

/* Number instructions in block order; fill def_seq and last_use for each vreg.
 */
static void ir_asm_number_and_liveness(Function *fn,
                                       const uint32_t *block_order,
                                       uint32_t n_block_order, int *def_seq,
                                       int *last_use) {
  for (int i = 0; i < MAX_INSTRS; i++) {
    def_seq[i] = -1;
    last_use[i] = -1;
  }
  int seq = 0;
  for (uint32_t i = 0; i < n_block_order; i++) {
    BlockID bid = block_order[i];
    if (bid >= fn->n_blocks)
      continue;
    Block *blk = fn->blocks[bid];
    if (!blk)
      continue;
    for (Instr *ins = blk->head; ins; ins = ins->next) {
      if (ins->op == OP_NOP)
        continue;
      ins->lscan_seq = seq;
      /* Uses */
      if (ins->op == OP_CONDBR) {
        if (ins->n_src >= 1 && ins->src[0])
          last_use[ins->src[0]] = seq;
      } else if (ins->op == OP_BR) {
        /* no reg operands */
      } else if (ins->op == OP_RET) {
        if (ins->n_src >= 1 && ins->src[0])
          last_use[ins->src[0]] = seq;
      } else if (ins->op == OP_STORE) {
        if (ins->n_src >= 1 && ins->src[0])
          last_use[ins->src[0]] = seq;
        if (ins->n_src >= 2 && ins->src[1])
          last_use[ins->src[1]] = seq;
      } else if (ins->op == OP_PHI) {
        for (uint32_t p = 0; p < ins->n_phi; p++)
          if (ins->phi[p].reg)
            last_use[ins->phi[p].reg] = seq;
      } else if (ins->op == OP_CALL) {
        for (uint32_t c = 0; c < ins->n_call_args; c++)
          if (ins->call_args[c])
            last_use[ins->call_args[c]] = seq;
      } else {
        for (uint32_t s = 0; s < ins->n_src; s++)
          if (ins->src[s])
            last_use[ins->src[s]] = seq;
      }
      /* Def */
      if (ins->dst &&
          (ins->op != OP_BR && ins->op != OP_CONDBR && ins->op != OP_STORE))
        def_seq[ins->dst] = seq;
      seq++;
    }
  }
  /* CG-IR-005 FIX (BUG 1): Back-edge PHI sources have inverted intervals
   * (last_use at PHI in header < def_seq in latch). The linear scan frees
   * their phys reg instantly, causing the edge copy to read wrong values.
   * Fix: extend last_use to end of function for any back-edge PHI source. */
  for (uint32_t i = 0; i < n_block_order; i++) {
    BlockID bid_fix = block_order[i];
    if (bid_fix >= fn->n_blocks)
      continue;
    Block *blk_fix = fn->blocks[bid_fix];
    if (!blk_fix)
      continue;
    for (Instr *phi_fix = blk_fix->head; phi_fix && phi_fix->op == OP_PHI;
         phi_fix = phi_fix->next) {
      for (uint32_t p = 0; p < phi_fix->n_phi; p++) {
        RegID src_r = phi_fix->phi[p].reg;
        if (!src_r || src_r >= MAX_INSTRS)
          continue;
        if (def_seq[src_r] >= 0 &&
            (last_use[src_r] < 0 || last_use[src_r] < def_seq[src_r])) {
          /* Back edge detected: source is defined AFTER its PHI use point.
           * Extend to end of function so linear scan keeps its phys reg alive
           * through the predecessor's BR where the edge copy executes. */
          last_use[src_r] = seq > 0 ? seq - 1 : 0;
        }
      }
    }
  }
  /* BUG-2 FIX: extend lifetime of any value defined before a loop and used
   * inside it. Detects back-edges via block_order indices. Conservative:
   * extends to end of function (pessimizes regalloc but provably correct). */
  for (uint32_t bi = 0; bi < n_block_order; bi++) {
    BlockID hdr_bid = block_order[bi];
    if (hdr_bid >= fn->n_blocks) continue;
    Block *hdr = fn->blocks[hdr_bid];
    if (!hdr) continue;
    int is_loop_header = 0;
    for (uint32_t pi = 0; pi < hdr->n_preds; pi++) {
      BlockID pred_bid = hdr->preds[pi];
      for (uint32_t k = bi + 1; k < n_block_order; k++) {
        if (block_order[k] == pred_bid) { is_loop_header = 1; break; }
      }
      if (is_loop_header) break;
    }
    if (!is_loop_header) continue;
    int hdr_first_seq = hdr->head ? hdr->head->lscan_seq : -1;
    if (hdr_first_seq < 0) continue;
    for (int r = 0; r < MAX_INSTRS; r++) {
      if (def_seq[r] < 0) continue;
      if (def_seq[r] < hdr_first_seq && last_use[r] >= hdr_first_seq) {
        last_use[r] = seq > 0 ? seq - 1 : 0;
      }
    }
  }
}

/* Linear scan: assign phys reg or spill (-1). Fills phys_reg[]; spilled vregs
 * use stack. */
static void ir_asm_linear_scan(Function *fn, const uint32_t *block_order,
                               uint32_t n_block_order, int *def_seq,
                               int *last_use, int *phys_reg_out) {
  for (int i = 0; i < MAX_INSTRS; i++)
    phys_reg_out[i] = -1;
  LiveInterval intervals[MAX_INSTRS];
  int n_int = 0;
  for (int r = 0; r < MAX_INSTRS; r++) {
    if (def_seq[r] < 0)
      continue;
    intervals[n_int].vreg = (RegID)r;
    intervals[n_int].start = def_seq[r];
    intervals[n_int].end = last_use[r] >= 0 ? last_use[r] : def_seq[r];
    intervals[n_int].phys_reg = -1;
    n_int++;
  }
  qsort(intervals, (size_t)n_int, sizeof(LiveInterval), live_interval_compare);

  if (getenv("ZCC_DEBUG_LSCAN")) {
    for (int i = 0; i < n_int; i++) {
      fprintf(stderr, "[LSCAN] vreg=%d start=%d end=%d\n", intervals[i].vreg,
              intervals[i].start, intervals[i].end);
    }
  }

  int active_end = 0; /* active intervals: [0, active_end) */
  for (int i = 0; i < n_int; i++) {
    LiveInterval *cur = &intervals[i];
    /* Allocas produce stack addresses that must survive the entire function.
       Force them to spill — leaq offset(%rbp) is trivially recomputable. */
    if (cur->vreg < MAX_INSTRS && fn->def_of[cur->vreg] &&
        fn->def_of[cur->vreg]->op == OP_ALLOCA)
      continue;
    /* Expire intervals that end before cur->start */
    for (int j = 0; j < active_end; j++) {
      if (intervals[j].phys_reg >= 0 && intervals[j].end < cur->start) {
        intervals[j].phys_reg = -1;
        intervals[j] = intervals[active_end - 1];
        active_end--;
        j--;
      }
    }
    /* Find a free phys reg */
    int used_mask = 0;
    for (int j = 0; j < active_end; j++)
      if (intervals[j].phys_reg >= 0)
        used_mask |= (1 << intervals[j].phys_reg);
    int chosen = -1;
    for (int p = 0; p < N_PHYS_REGS; p++) {
      if (!(used_mask & (1 << p))) {
        chosen = p;
        break;
      }
    }
    if (chosen >= 0) {
      if (getenv("ZCC_DEBUG_LSCAN"))
        fprintf(stderr, "[LSCAN] vreg=%d gets phys_reg=%d\n", cur->vreg,
                chosen);
      cur->phys_reg = chosen;
      phys_reg_out[cur->vreg] = chosen;
      intervals[active_end++] = *cur;
    } else {
      if (getenv("ZCC_DEBUG_LSCAN"))
        fprintf(stderr, "[LSCAN] vreg=%d spilled\n", cur->vreg);
      /* No free reg: leave current interval spilled (no eviction = no spill
       * code to emit). */
    }
  }
}

/* ── IR-to-asm emission (for PGO-instrumented build) ─────────────────────────
 * Emits x86-64 body: each block gets a label, optional probe, then lowered
 * insns. With linear scan: vregs may be in phys regs or stack at
 * -8*(r+2)(%rbp). */
typedef struct {
  FILE *out;
  Function *fn;
  int func_end_label;
  int num_params; /* first num_params allocas map to -8(%rbp), -16(%rbp), ... */
  int global_block_offset; /* add to bid so counts are global across functions
                            */
  int func_label_id; /* unique per function so .Lir_b_%d_%u doesn't collide */
  int alloca_off[MAX_INSTRS]; /* alloca_off[dst] = offset from rbp (< 0) */
  int n_allocas;
  uint32_t block_order[MAX_BLOCKS];
  uint32_t n_block_order;
  /* Linear scan allocation: phys_reg[r] = 0..N_PHYS_REGS-1 or -1 (stack) */
  int phys_reg[MAX_INSTRS];
  int def_seq[MAX_INSTRS];
  int last_use[MAX_INSTRS];
  int used_callee_saved_mask; /* bits for rbx(0), r12(3), r13(4), r14(5), r15(6)
                               */
  int callee_saved_emitted;   /* 1 after we've emitted push of callee-saved */
  int body_only; /* 1 when AST owns prologue/epilogue (skip push/pop) */
  int slot_base; /* CG-IR-008: base offset for IR spill slots (0 or -stack_size)
                  */
  int ir_extra;
  int alloca_bytes_total;
  int csave_base; /* CG-IR-016-CSAVE-V2: %rbp-relative base of callee-save area.
                   * = -(stack_size + ir_extra) + 32
                   * Slots: csave_base (rbx), csave_base-8 (r12), -16 (r13),
                   *        csave_base-24 (r14), csave_base-32 (r15).          */
  int ret_size;  /* CG-IR-015: return type bytes (4=int, 8=long/ptr, 0→treat as 8) */
} IRAsmCtx;

static int ir_asm_slot(RegID r) { return -8 * (int)(r + 2); }

/* Return phys reg index (0..N_PHYS_REGS-1) or -1; if -1, *out_slot is stack
 * offset. */
static int ir_asm_vreg_location(IRAsmCtx *ctx, RegID r, int *out_slot) {
  *out_slot =
      ctx->slot_base - 8 * (int)(r + 1); /* CG-IR-008: offset below AST frame */
  if (!r || r >= MAX_INSTRS)
    return -1;
  if (ctx->phys_reg[r] >= 0)
    return ctx->phys_reg[r];
  return -1;
}

static void ir_asm_load_to_rax(IRAsmCtx *ctx, RegID r) {
  FILE *f = ctx->out;
  int slot;
  int p = ir_asm_vreg_location(ctx, r, &slot);
  if (p >= 0)
    fprintf(f, "    movq %%%s, %%rax\n", phys_reg_name[p]);
  else
    fprintf(f, "    movq %d(%%rbp), %%rax\n", slot);
}

static void ir_asm_load_to_rax_typed(IRAsmCtx *ctx, RegID r, IRType t) {
  FILE *f = ctx->out;
  int slot;
  int p = ir_asm_vreg_location(ctx, r, &slot);
  if (p >= 0) {
    fprintf(f, "    movq %%%s, %%rax\n", phys_reg_name[p]);
    if (t == IR_TY_I32)      fprintf(f, "    movslq %%eax, %%rax\n");
    else if (t == IR_TY_U32) fprintf(f, "    movl %%eax, %%eax\n");
  } else {
    if (t == IR_TY_I32)      fprintf(f, "    movslq %d(%%rbp), %%rax\n", slot);
    else if (t == IR_TY_U32) fprintf(f, "    movl %d(%%rbp), %%eax\n", slot);
    else                     fprintf(f, "    movq %d(%%rbp), %%rax\n", slot);
  }
}

static void ir_asm_store_rax_to(IRAsmCtx *ctx, RegID r) {
  FILE *f = ctx->out;
  int slot;
  int p = ir_asm_vreg_location(ctx, r, &slot);
  if (p >= 0)
    fprintf(f, "    movq %%rax, %%%s\n", phys_reg_name[p]);
  else
    fprintf(f, "    movq %%rax, %d(%%rbp)\n", slot);
}

static void ir_asm_load_to_rcx(IRAsmCtx *ctx, RegID r) {
  FILE *f = ctx->out;
  int slot;
  int p = ir_asm_vreg_location(ctx, r, &slot);
  if (p >= 0)
    fprintf(f, "    movq %%%s, %%rcx\n", phys_reg_name[p]);
  else
    fprintf(f, "    movq %d(%%rbp), %%rcx\n", slot);
}

/* Emit second operand for binary op: "addq <src>, %%rax" — <src> is reg or mem.
 */
static void ir_asm_emit_src_operand(IRAsmCtx *ctx, RegID r) {
  FILE *f = ctx->out;
  int slot;
  int p = ir_asm_vreg_location(ctx, r, &slot);
  if (p >= 0)
    fprintf(f, "%%%s", phys_reg_name[p]);
  else
    fprintf(f, "%d(%%rbp)", slot);
}

/* Emit copy on edge (from_bid -> to_bid) so PHI at merge gets its value; call
 * before emitting BR. */
static void ir_asm_emit_phi_edge_copy(IRAsmCtx *ctx, BlockID from_bid,
                                      BlockID to_bid) {
  Function *fn = ctx->fn;
  FILE *f = ctx->out;
  if (to_bid >= fn->n_blocks)
    return;
  Block *to_blk = fn->blocks[to_bid];
  if (!to_blk || !to_blk->head)
    return;

  /* CG-IR-005 FIX (BUG 3): Parallel copy semantics for PHI edge copies.
   * SSA PHIs execute simultaneously — all sources must be read BEFORE any
   * destination is written. Serial copy through %rax causes lost copies
   * when PHI_B's source overlaps PHI_A's destination.
   *
   * Strategy: Count PHIs, push all source values onto the x86 stack,
   * then pop them into destinations in reverse order. This uses the
   * hardware stack as a temporary buffer, guaranteeing all reads
   * complete before any writes. */

  /* Phase 1: count PHIs from this edge */
  int n_phis = 0;
  for (Instr *ins = to_blk->head; ins; ins = ins->next) {
    if (ins->op != OP_PHI)
      continue;
    for (uint32_t p = 0; p < ins->n_phi; p++) {
      if (ins->phi[p].block == from_bid) {
        n_phis++;
        break;
      }
    }
  }
  if (n_phis == 0)
    return;

  /* Fast path: single PHI — no interference possible */
  if (n_phis == 1) {
    for (Instr *ins = to_blk->head; ins; ins = ins->next) {
      if (ins->op != OP_PHI)
        continue;
      for (uint32_t p = 0; p < ins->n_phi; p++) {
        if (ins->phi[p].block == from_bid) {
          ir_asm_load_to_rax(ctx, ins->phi[p].reg);
          ir_asm_store_rax_to(ctx, ins->dst);
          return;
        }
      }
    }
    return;
  }

  /* Slow path: multiple PHIs — use stack to implement parallel copy.
   * Push all sources, then pop to destinations in reverse order. */

  /* Phase 2: push all source values (forward order) */
  for (Instr *ins = to_blk->head; ins; ins = ins->next) {
    if (ins->op != OP_PHI)
      continue;
    for (uint32_t p = 0; p < ins->n_phi; p++) {
      if (ins->phi[p].block == from_bid) {
        ir_asm_load_to_rax(ctx, ins->phi[p].reg);
        fprintf(f, "    pushq %%rax\n");
        break;
      }
    }
  }

  /* Phase 3: collect destination regs in order, then pop in reverse */
  {
    RegID *phi_dsts = (RegID *)malloc(sizeof(RegID) * n_phis);
    if (!phi_dsts) {
      fprintf(stderr, "Fatal: Out of memory in ir_asm_emit_phi_edge_copy\n");
      exit(1);
    }
    int n_dst = 0;
    for (Instr *ins = to_blk->head; ins; ins = ins->next) {
      if (ins->op != OP_PHI)
        continue;
      for (uint32_t p = 0; p < ins->n_phi; p++) {
        if (ins->phi[p].block == from_bid) {
          phi_dsts[n_dst++] = ins->dst;
          break;
        }
      }
    }
    /* Pop in reverse order (LIFO) to match push order */
    for (int i = n_dst - 1; i >= 0; i--) {
      fprintf(f, "    popq %%rax\n");
      ir_asm_store_rax_to(ctx, phi_dsts[i]);
    }
    free(phi_dsts);
  }
}

static void ir_asm_emit_probe(IRAsmCtx *ctx, BlockID bid) {
  FILE *f = ctx->out;
  int gbid = ctx->global_block_offset + (int)(uint32_t)bid;
  fprintf(f, "    leaq __zcc_edge_counts(%%rip), %%rax\n");
  fprintf(f, "    addq $%d, %%rax\n", gbid * 8);
  fprintf(f, "    movq (%%rax), %%rcx\n");
  fprintf(f, "    addq $1, %%rcx\n");
  fprintf(f, "    movq %%rcx, (%%rax)\n");
}

static void ir_asm_lower_insn(IRAsmCtx *ctx, const Instr *ins,
                              BlockID cur_block) {
  FILE *f = ctx->out;
  Function *fn = ctx->fn;
  (void)cur_block;
  switch (ins->op) {
  case OP_ASM: {
    fprintf(f, "    %s\n", ins->asm_string ? ins->asm_string : "");
    break;
  }
  case OP_VLA_ALLOC: {
    ir_asm_load_to_rax(ctx, ins->src[0]);
    fprintf(f, "    addq $15, %%rax\n");
    fprintf(f, "    andq $-16, %%rax\n");
    fprintf(f, "    subq %%rax, %%rsp\n");
    fprintf(f, "    movq %%rsp, %%rax\n");
    ir_asm_store_rax_to(ctx, ins->dst);
    break;
  }
  case OP_PGO_COUNTER_ADDR: {
    int gbid = ctx->global_block_offset + (int)(uint32_t)ins->imm;
    fprintf(f, "    leaq __zcc_edge_counts(%%rip), %%rax\n");
    fprintf(f, "    addq $%d, %%rax\n", gbid * 8);
    ir_asm_store_rax_to(ctx, ins->dst);
    break;
  }
  case OP_CONST: {
    int dst_slot;
    int dst_p = ir_asm_vreg_location(ctx, ins->dst, &dst_slot);
    int64_t k = (int64_t)ins->imm;
    if (dst_p >= 0) {
      if (k >= (int64_t)-2147483648LL && k <= (int64_t)2147483647LL)
        fprintf(f, "    movq $%lld, %%%s\n", (long long)k,
                phys_reg_name[dst_p]);
      else {
        fprintf(f, "    movabsq $%lld, %%%s\n", (long long)k,
                phys_reg_name[dst_p]);
      }
    } else {
      if (k >= (int64_t)-2147483648LL && k <= (int64_t)2147483647LL)
        fprintf(f, "    movq $%lld, %d(%%rbp)\n", (long long)k, dst_slot);
      else {
        fprintf(f, "    movabsq $%lld, %%rax\n", (long long)k);
        fprintf(f, "    movq %%rax, %d(%%rbp)\n", dst_slot);
      }
    }
    break;
  }
  case OP_ADD: {
    /* CG-IR-017: addl for I32/U32, addq for I64/U64 */
    ir_asm_load_to_rax_typed(ctx, ins->src[0], ins->ir_type);
    if (ins->ir_type == IR_TY_I32 || ins->ir_type == IR_TY_U32) {
      fprintf(f, "    movq ");
      ir_asm_emit_src_operand(ctx, ins->src[1]);
      fprintf(f, ", %%rcx\n");
      fprintf(f, "    addl %%ecx, %%eax\n");
    } else {
      fprintf(f, "    addq ");
      ir_asm_emit_src_operand(ctx, ins->src[1]);
      fprintf(f, ", %%rax\n");
    }
    ir_asm_store_rax_to(ctx, ins->dst);
    break;
  }
  case OP_SUB: {
    /* CG-IR-017: subl for I32/U32, subq for I64/U64 */
    ir_asm_load_to_rax_typed(ctx, ins->src[0], ins->ir_type);
    if (ins->ir_type == IR_TY_I32 || ins->ir_type == IR_TY_U32) {
      fprintf(f, "    movq ");
      ir_asm_emit_src_operand(ctx, ins->src[1]);
      fprintf(f, ", %%rcx\n");
      fprintf(f, "    subl %%ecx, %%eax\n");
    } else {
      fprintf(f, "    subq ");
      ir_asm_emit_src_operand(ctx, ins->src[1]);
      fprintf(f, ", %%rax\n");
    }
    ir_asm_store_rax_to(ctx, ins->dst);
    break;
  }
  case OP_MUL: {
    /* CG-IR-017: imull for I32/U32, imulq for I64/U64 */
    ir_asm_load_to_rax_typed(ctx, ins->src[0], ins->ir_type);
    fprintf(f, "    movq ");
    ir_asm_emit_src_operand(ctx, ins->src[1]);
    fprintf(f, ", %%rcx\n");
    if (ins->ir_type == IR_TY_I32 || ins->ir_type == IR_TY_U32)
      fprintf(f, "    imull %%ecx, %%eax\n");
    else
      fprintf(f, "    imulq %%rcx, %%rax\n");
    ir_asm_store_rax_to(ctx, ins->dst);
    break;
  }
  case OP_DIV: {
    /* CG-IR-015: select instruction width from ir_type.
     * Bug: cqo sign-extends rax→rdx:rax in 64-bit; for 32-bit int operands
     * that corrupts the dividend.  Fix: use cltd/idivl for I32, plain
     * xor+divl for U32, xor+divq for U64, cqo/idivq for I64 (default). */
    ir_asm_load_to_rax_typed(ctx, ins->src[0], ins->ir_type);
    if (ins->ir_type == IR_TY_I32) {
      fprintf(f, "    cltd\n");              /* sign-extend eax → edx:eax    */
      fprintf(f, "    movq ");
      ir_asm_emit_src_operand(ctx, ins->src[1]);
      fprintf(f, ", %%rcx\n");
      fprintf(f, "    idivl %%ecx\n");
    } else if (ins->ir_type == IR_TY_U32) {
      fprintf(f, "    movl %%eax, %%eax\n"); /* zero upper 32 bits of rax    */
      fprintf(f, "    xorl %%edx, %%edx\n");
      fprintf(f, "    movq ");
      ir_asm_emit_src_operand(ctx, ins->src[1]);
      fprintf(f, ", %%rcx\n");
      fprintf(f, "    divl %%ecx\n");
    } else if (ins->ir_type == IR_TY_U64) {
      fprintf(f, "    xorl %%edx, %%edx\n");
      fprintf(f, "    movq ");
      ir_asm_emit_src_operand(ctx, ins->src[1]);
      fprintf(f, ", %%rcx\n");
      fprintf(f, "    divq %%rcx\n");
    } else {                                /* IR_TY_I64 — original path     */
      fprintf(f, "    cqo\n");
      fprintf(f, "    movq ");
      ir_asm_emit_src_operand(ctx, ins->src[1]);
      fprintf(f, ", %%rcx\n");
      fprintf(f, "    idivq %%rcx\n");
    }
    ir_asm_store_rax_to(ctx, ins->dst);
    break;
  }
  case OP_MOD: {
    /* CG-IR-015: same width/sign selection as OP_DIV; remainder is in
     * edx (32-bit) or rdx (64-bit) after the div instruction, so move it
     * into eax/rax before the generic ir_asm_store_rax_to. */
    ir_asm_load_to_rax_typed(ctx, ins->src[0], ins->ir_type);
    if (ins->ir_type == IR_TY_I32) {
      fprintf(f, "    cltd\n");
      fprintf(f, "    movq ");
      ir_asm_emit_src_operand(ctx, ins->src[1]);
      fprintf(f, ", %%rcx\n");
      fprintf(f, "    idivl %%ecx\n");
      fprintf(f, "    movl %%edx, %%eax\n");
    } else if (ins->ir_type == IR_TY_U32) {
      fprintf(f, "    movl %%eax, %%eax\n");
      fprintf(f, "    xorl %%edx, %%edx\n");
      fprintf(f, "    movq ");
      ir_asm_emit_src_operand(ctx, ins->src[1]);
      fprintf(f, ", %%rcx\n");
      fprintf(f, "    divl %%ecx\n");
      fprintf(f, "    movl %%edx, %%eax\n");
    } else if (ins->ir_type == IR_TY_U64) {
      fprintf(f, "    xorl %%edx, %%edx\n");
      fprintf(f, "    movq ");
      ir_asm_emit_src_operand(ctx, ins->src[1]);
      fprintf(f, ", %%rcx\n");
      fprintf(f, "    divq %%rcx\n");
      fprintf(f, "    movq %%rdx, %%rax\n");
    } else {                               /* IR_TY_I64 — original path      */
      fprintf(f, "    cqo\n");
      fprintf(f, "    movq ");
      ir_asm_emit_src_operand(ctx, ins->src[1]);
      fprintf(f, ", %%rcx\n");
      fprintf(f, "    idivq %%rcx\n");
      fprintf(f, "    movq %%rdx, %%rax\n");
    }
    ir_asm_store_rax_to(ctx, ins->dst);
    break;
  }
  case OP_BAND: {
    /* CG-IR-017 */
    ir_asm_load_to_rax_typed(ctx, ins->src[0], ins->ir_type);
    if (ins->ir_type == IR_TY_I32 || ins->ir_type == IR_TY_U32) {
      fprintf(f, "    movq ");
      ir_asm_emit_src_operand(ctx, ins->src[1]);
      fprintf(f, ", %%rcx\n");
      fprintf(f, "    andl %%ecx, %%eax\n");
    } else {
      fprintf(f, "    andq ");
      ir_asm_emit_src_operand(ctx, ins->src[1]);
      fprintf(f, ", %%rax\n");
    }
    ir_asm_store_rax_to(ctx, ins->dst);
    break;
  }
  case OP_SHL: {
    /* CG-IR-015: fix undefined sz_suffix(ir_type_bytes(n->type)) reference.
     * For the immediate path: shlq is always correct for ADD/SUB lower-bits
     * (upper bits discarded anyway), but use shll for I32/U32 to stay clean.
     * For the reg-count path: was referencing undefined `n->type` — fixed. */
    ir_asm_load_to_rax_typed(ctx, ins->src[0], ins->ir_type);
    int shl_is32 = (ins->ir_type == IR_TY_I32 || ins->ir_type == IR_TY_U32);
    if (ins->src[1] < MAX_INSTRS && fn->def_of[ins->src[1]] &&
        fn->def_of[ins->src[1]]->op == OP_CONST) {
      int64_t k = fn->def_of[ins->src[1]]->imm & (shl_is32 ? 31 : 63);
      if (shl_is32) fprintf(f, "    shll $%lld, %%eax\n", (long long)k);
      else          fprintf(f, "    shlq $%lld, %%rax\n", (long long)k);
    } else {
      fprintf(f, "    movq ");
      ir_asm_emit_src_operand(ctx, ins->src[1]);
      fprintf(f, ", %%rcx\n");
      if (shl_is32) fprintf(f, "    shll %%cl, %%eax\n");
      else          fprintf(f, "    shlq %%cl, %%rax\n");
    }
    ir_asm_store_rax_to(ctx, ins->dst);
    break;
  }
  case OP_SHR: {
    /* CG-IR-015: critical fix.
     *  Bug A: shrq on a 32-bit value shifts upper garbage into lower bits.
     *  Bug B: sarq used for unsigned right-shift propagates sign bit.
     *  Fix: select {shr,sar}{l,q} based on ir_type width + signedness.
     *  Also fixes the undefined sz_suffix(ir_type_bytes(n->type)) reference. */
    ir_asm_load_to_rax_typed(ctx, ins->src[0], ins->ir_type);
    int shr_is32  = (ins->ir_type == IR_TY_I32 || ins->ir_type == IR_TY_U32);
    int shr_unsig = (ins->ir_type == IR_TY_U32 || ins->ir_type == IR_TY_U64);
    if (ins->src[1] < MAX_INSTRS && fn->def_of[ins->src[1]] &&
        fn->def_of[ins->src[1]]->op == OP_CONST) {
      int64_t k = fn->def_of[ins->src[1]]->imm & (shr_is32 ? 31 : 63);
      if      (shr_is32 && shr_unsig)  fprintf(f, "    shrl $%lld, %%eax\n", (long long)k);
      else if (shr_is32)               fprintf(f, "    sarl $%lld, %%eax\n", (long long)k);
      else if (shr_unsig)              fprintf(f, "    shrq $%lld, %%rax\n", (long long)k);
      else                             fprintf(f, "    shrq $%lld, %%rax\n", (long long)k); /* I64 default: keep shrq for compat */
    } else {
      fprintf(f, "    movq ");
      ir_asm_emit_src_operand(ctx, ins->src[1]);
      fprintf(f, ", %%rcx\n");
      if      (shr_is32 && shr_unsig)  fprintf(f, "    shrl %%cl, %%eax\n");
      else if (shr_is32)               fprintf(f, "    sarl %%cl, %%eax\n");
      else if (shr_unsig)              fprintf(f, "    shrq %%cl, %%rax\n");
      else                             fprintf(f, "    shrq %%cl, %%rax\n"); /* I64 default: keep shrq for compat */
    }
    ir_asm_store_rax_to(ctx, ins->dst);
    break;
  }
  case OP_LOAD: {
    if (s_debug_main_emit)
      fprintf(stderr, "[PGO-DEBUG] block %u OP_LOAD dst=%u src0=%u\n",
              (unsigned)cur_block, ins->dst, ins->src[0]);
    ir_asm_load_to_rax(ctx, ins->src[0]);
    switch ((int)ins->imm) {
    case 1:
      fprintf(f, "    movzbq (%%rax), %%rax\n");
      break;
    case 2:
      fprintf(f, "    movzwq (%%rax), %%rax\n");
      break;
    case 4:
      fprintf(f, "    movslq (%%rax), %%rax\n");
      break;
    default:
      fprintf(f, "    movq (%%rax), %%rax\n");
      break;
    }
    ir_asm_store_rax_to(ctx, ins->dst);
    break;
  }
  case OP_STORE:
    if (ins->n_src >= 2) {
      ir_asm_load_to_rax(ctx, ins->src[1]);
      ir_asm_load_to_rcx(ctx, ins->src[0]);
      switch ((int)ins->imm) {
      case 1:
        fprintf(f, "    movb %%cl, (%%rax)\n");
        break;
      case 2:
        fprintf(f, "    movw %%cx, (%%rax)\n");
        break;
      case 4:
        fprintf(f, "    movl %%ecx, (%%rax)\n");
        break;
      default:
        fprintf(f, "    movq %%rcx, (%%rax)\n");
        break;
      }
    }
    break;
  case OP_BR:
    if (ins->n_src >= 1) {
      ir_asm_emit_phi_edge_copy(ctx, cur_block, (BlockID)ins->src[0]);
      fprintf(f, "    jmp .Lir_b_%d_%u\n", ctx->func_label_id,
              (unsigned)ins->src[0]);
    }
    break;
  case OP_CONDBR:
    /* CG-IR-005 FIX (BUG 2): PHI edge copies must only execute on the
     * TAKEN path. Previously, true-target copies ran unconditionally before
     * jnz, corrupting PHI slots when the false path was taken.
     * Fix: jz to false_path label, emit true copies + jmp, then false copies +
     * jmp. */
    if (ins->n_src >= 3) {
      ir_asm_load_to_rax(ctx, ins->src[0]);
      fprintf(f, "    testq %%rax, %%rax\n");
      fprintf(f, "    jz .Lir_cf_%d_%u\n", ctx->func_label_id,
              (unsigned)cur_block);
      /* True path: PHI copies then jump to true target */
      ir_asm_emit_phi_edge_copy(ctx, cur_block, (BlockID)ins->src[1]);
      fprintf(f, "    jmp .Lir_b_%d_%u\n", ctx->func_label_id,
              (unsigned)ins->src[1]);
      /* False path: PHI copies then jump to false target */
      fprintf(f, ".Lir_cf_%d_%u:\n", ctx->func_label_id, (unsigned)cur_block);
      ir_asm_emit_phi_edge_copy(ctx, cur_block, (BlockID)ins->src[2]);
      fprintf(f, "    jmp .Lir_b_%d_%u\n", ctx->func_label_id,
              (unsigned)ins->src[2]);
    }
    break;
  case OP_RET:
    if (ins->n_src >= 1)
      ir_asm_load_to_rax(ctx, ins->src[0]);
    /* CG-IR-015: zero-truncate 32-bit return values.
     * 64-bit arithmetic (neg/sub on sign-extended values) can leave garbage
     * in the upper 32 bits of rax.  Callers of int-returning functions read
     * only eax; the movl self-move zero-extends eax → rax implicitly. */
    if (ctx->ret_size == 4)
      fprintf(f, "    movl %%eax, %%eax\n");
    if (!ctx->body_only) {
      for (int i = CALLEE_SAVED_PUSH_N - 1; i >= 0; i--) {
        int p = callee_saved_push_order[i];
        if (ctx->used_callee_saved_mask & (1 << p))
          fprintf(f, "    popq %%%s\n", phys_reg_name[p]);
      }
    } else if (ctx->csave_base) {
      /* CG-IR-016-RET: body_only path — restore callee-saved registers
       * BEFORE the jmp so they execute on every return path, not just
       * fall-through.  The 'after-body' restores in
       * zcc_run_passes_emit_body_pgo are only reached by fall-through
       * (functions with no explicit return) — dead code for most funcs. */
      fprintf(f, "    movq %d(%%rbp), %%rbx\n", ctx->csave_base);
      fprintf(f, "    movq %d(%%rbp), %%r12\n", ctx->csave_base - 8);
      fprintf(f, "    movq %d(%%rbp), %%r13\n", ctx->csave_base - 16);
      fprintf(f, "    movq %d(%%rbp), %%r14\n", ctx->csave_base - 24);
      fprintf(f, "    movq %d(%%rbp), %%r15\n", ctx->csave_base - 32);
    }
    fprintf(f, "    jmp .Lfunc_end_%d\n", ctx->func_end_label);
    break;
  case OP_LT: {
    /* CG-IR-017 */
    ir_asm_load_to_rax_typed(ctx, ins->src[0], ins->ir_type);
    if (ins->ir_type == IR_TY_I32 || ins->ir_type == IR_TY_U32) {
      fprintf(f, "    movq ");
      ir_asm_emit_src_operand(ctx, ins->src[1]);
      fprintf(f, ", %%rcx\n");
      fprintf(f, "    cmpl %%ecx, %%eax\n");
      fprintf(f, "    setl %%al\n");
      fprintf(f, "    movzbl %%al, %%eax\n");
    } else {
      fprintf(f, "    cmpq ");
      ir_asm_emit_src_operand(ctx, ins->src[1]);
      fprintf(f, ", %%rax\n");
      fprintf(f, "    setl %%al\n");
      fprintf(f, "    movzbq %%al, %%rax\n");
    }
    ir_asm_store_rax_to(ctx, ins->dst);
    break;
  }
  case OP_EQ: {
    /* CG-IR-017 */
    ir_asm_load_to_rax_typed(ctx, ins->src[0], ins->ir_type);
    if (ins->ir_type == IR_TY_I32 || ins->ir_type == IR_TY_U32) {
      fprintf(f, "    movq ");
      ir_asm_emit_src_operand(ctx, ins->src[1]);
      fprintf(f, ", %%rcx\n");
      fprintf(f, "    cmpl %%ecx, %%eax\n");
      fprintf(f, "    sete %%al\n");
      fprintf(f, "    movzbl %%al, %%eax\n");
    } else {
      fprintf(f, "    cmpq ");
      ir_asm_emit_src_operand(ctx, ins->src[1]);
      fprintf(f, ", %%rax\n");
      fprintf(f, "    sete %%al\n");
      fprintf(f, "    movzbq %%al, %%rax\n");
    }
    ir_asm_store_rax_to(ctx, ins->dst);
    break;
  }
  case OP_NE: {
    /* CG-IR-017 */
    ir_asm_load_to_rax_typed(ctx, ins->src[0], ins->ir_type);
    if (ins->ir_type == IR_TY_I32 || ins->ir_type == IR_TY_U32) {
      fprintf(f, "    movq ");
      ir_asm_emit_src_operand(ctx, ins->src[1]);
      fprintf(f, ", %%rcx\n");
      fprintf(f, "    cmpl %%ecx, %%eax\n");
      fprintf(f, "    setne %%al\n");
      fprintf(f, "    movzbl %%al, %%eax\n");
    } else {
      fprintf(f, "    cmpq ");
      ir_asm_emit_src_operand(ctx, ins->src[1]);
      fprintf(f, ", %%rax\n");
      fprintf(f, "    setne %%al\n");
      fprintf(f, "    movzbq %%al, %%rax\n");
    }
    ir_asm_store_rax_to(ctx, ins->dst);
    break;
  }
  case OP_GT: {
    /* CG-IR-017 */
    ir_asm_load_to_rax_typed(ctx, ins->src[0], ins->ir_type);
    if (ins->ir_type == IR_TY_I32 || ins->ir_type == IR_TY_U32) {
      fprintf(f, "    movq ");
      ir_asm_emit_src_operand(ctx, ins->src[1]);
      fprintf(f, ", %%rcx\n");
      fprintf(f, "    cmpl %%ecx, %%eax\n");
      fprintf(f, "    setg %%al\n");
      fprintf(f, "    movzbl %%al, %%eax\n");
    } else {
      fprintf(f, "    cmpq ");
      ir_asm_emit_src_operand(ctx, ins->src[1]);
      fprintf(f, ", %%rax\n");
      fprintf(f, "    setg %%al\n");
      fprintf(f, "    movzbq %%al, %%rax\n");
    }
    ir_asm_store_rax_to(ctx, ins->dst);
    break;
  }
  case OP_GE: {
    /* CG-IR-017 */
    ir_asm_load_to_rax_typed(ctx, ins->src[0], ins->ir_type);
    if (ins->ir_type == IR_TY_I32 || ins->ir_type == IR_TY_U32) {
      fprintf(f, "    movq ");
      ir_asm_emit_src_operand(ctx, ins->src[1]);
      fprintf(f, ", %%rcx\n");
      fprintf(f, "    cmpl %%ecx, %%eax\n");
      fprintf(f, "    setge %%al\n");
      fprintf(f, "    movzbl %%al, %%eax\n");
    } else {
      fprintf(f, "    cmpq ");
      ir_asm_emit_src_operand(ctx, ins->src[1]);
      fprintf(f, ", %%rax\n");
      fprintf(f, "    setge %%al\n");
      fprintf(f, "    movzbq %%al, %%rax\n");
    }
    ir_asm_store_rax_to(ctx, ins->dst);
    break;
  }
  case OP_LE: {
    /* CG-IR-017 */
    ir_asm_load_to_rax_typed(ctx, ins->src[0], ins->ir_type);
    if (ins->ir_type == IR_TY_I32 || ins->ir_type == IR_TY_U32) {
      fprintf(f, "    movq ");
      ir_asm_emit_src_operand(ctx, ins->src[1]);
      fprintf(f, ", %%rcx\n");
      fprintf(f, "    cmpl %%ecx, %%eax\n");
      fprintf(f, "    setle %%al\n");
      fprintf(f, "    movzbl %%al, %%eax\n");
    } else {
      fprintf(f, "    cmpq ");
      ir_asm_emit_src_operand(ctx, ins->src[1]);
      fprintf(f, ", %%rax\n");
      fprintf(f, "    setle %%al\n");
      fprintf(f, "    movzbq %%al, %%rax\n");
    }
    ir_asm_store_rax_to(ctx, ins->dst);
    break;
  }
  case OP_BOR: {
    /* CG-IR-017 */
    ir_asm_load_to_rax_typed(ctx, ins->src[0], ins->ir_type);
    if (ins->ir_type == IR_TY_I32 || ins->ir_type == IR_TY_U32) {
      fprintf(f, "    movq ");
      ir_asm_emit_src_operand(ctx, ins->src[1]);
      fprintf(f, ", %%rcx\n");
      fprintf(f, "    orl %%ecx, %%eax\n");
    } else {
      fprintf(f, "    orq ");
      ir_asm_emit_src_operand(ctx, ins->src[1]);
      fprintf(f, ", %%rax\n");
    }
    ir_asm_store_rax_to(ctx, ins->dst);
    break;
  }
  case OP_BXOR: {
    /* CG-IR-017 */
    ir_asm_load_to_rax_typed(ctx, ins->src[0], ins->ir_type);
    if (ins->ir_type == IR_TY_I32 || ins->ir_type == IR_TY_U32) {
      fprintf(f, "    movq ");
      ir_asm_emit_src_operand(ctx, ins->src[1]);
      fprintf(f, ", %%rcx\n");
      fprintf(f, "    xorl %%ecx, %%eax\n");
    } else {
      fprintf(f, "    xorq ");
      ir_asm_emit_src_operand(ctx, ins->src[1]);
      fprintf(f, ", %%rax\n");
    }
    ir_asm_store_rax_to(ctx, ins->dst);
    break;
  }
  case OP_BNOT: {
    /* CG-IR-017 */
    ir_asm_load_to_rax_typed(ctx, ins->src[0], ins->ir_type);
    if (ins->ir_type == IR_TY_I32 || ins->ir_type == IR_TY_U32)
      fprintf(f, "    notl %%eax\n");
    else
      fprintf(f, "    notq %%rax\n");
    ir_asm_store_rax_to(ctx, ins->dst);
    break;
  }
  case OP_COPY: {
    if (ins->n_src >= 1) {
      ir_asm_load_to_rax(ctx, ins->src[0]);
      ir_asm_store_rax_to(ctx, ins->dst);
    }
    break;
  }
  case OP_PHI:
    /* No code: value is placed on incoming edges by ir_asm_emit_phi_edge_copy.
     */
    break;
  case OP_ALLOCA: {
    int off = (ins->dst < MAX_INSTRS) ? ctx->alloca_off[ins->dst] : 0;
    fprintf(f, "    leaq %d(%%rbp), %%rax\n", off);
    ir_asm_store_rax_to(ctx, ins->dst);
    break;
  }
  case OP_GLOBAL: {
    const char *sym = ins->call_name[0] ? ins->call_name : "L_global";
    fprintf(f, "    leaq %s(%%rip), %%rax\n", sym);
    ir_asm_store_rax_to(ctx, ins->dst);
    break;
  }
  case OP_CALL: {
    static const char *arg_regs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
    int na = (int)ins->n_call_args;
    int stack_args = (na > 6) ? na - 6 : 0;
    int n_csave =
        ctx->body_only ? 0 : __builtin_popcount(ctx->used_callee_saved_mask);

    int current_seq = ins->lscan_seq;
    int caller_saved_live[N_PHYS_REGS];
    int n_caller_saved = 0;

    /* Liveness Query: Identify caller-saved regs (r10, r11) holding a live var.
     * (rax/rdi/rdx, etc., are safely excluded universally from N_PHYS_REGS).
     * An active caller-save has def_seq BEFORE current and last_use AFTER. */
    for (int p = 0; p < N_PHYS_REGS; p++) {
      if (!(PHYS_CALLEE_SAVED_MASK & (1 << p))) {
        for (int r = 1; r < MAX_INSTRS; r++) {
          if (ctx->phys_reg[r] == p) {
            if (ctx->def_seq[r] < current_seq && ctx->last_use[r] > current_seq) {
              caller_saved_live[n_caller_saved++] = p;
              break;
            }
          }
        }
      }
    }

    /* Emit pushq for active caller-saved registers */
    for (int i = 0; i < n_caller_saved; i++) {
      fprintf(f, "    pushq %%%s\n", phys_reg_name[caller_saved_live[i]]);
    }

    /* Alignment: Ensure exact 16-byte RSP alignment for callq */
    int need_pad = (n_csave + n_caller_saved + stack_args) & 1;
    if (need_pad)
      fprintf(f, "    subq $8, %%rsp\n");
    /* Push stack args (7th+) in reverse order */
    for (int i = na - 1; i >= 6; i--) {
      ir_asm_load_to_rax(ctx, ins->call_args[i]);
      fprintf(f, "    pushq %%rax\n");
    }
    /* Load register args 0..5 into rdi/rsi/rdx/rcx/r8/r9 */
    int reg_count = (na < 6) ? na : 6;
    for (int i = 0; i < reg_count; i++) {
      ir_asm_load_to_rax(ctx, ins->call_args[i]);
      fprintf(f, "    movq %%rax, %%%s\n", arg_regs[i]);
    }
    fprintf(f, "    xorl %%eax, %%eax\n"); /* al=0 for variadic */
    if (ins->call_name[0]) {
      fprintf(f, "    callq %s\n", ins->call_name);
    } else {
      ir_asm_load_to_rax(ctx, ins->src[0]);
      fprintf(f, "    callq *%%rax\n");
    }
    /* Cleanup stack args + pad */
    int cleanup = stack_args * 8 + (need_pad ? 8 : 0);
    if (cleanup > 0)
      fprintf(f, "    addq $%d, %%rsp\n", cleanup);
      
    /* Store return value */
    if (ins->dst)
      ir_asm_store_rax_to(ctx, ins->dst);

    /* Restore active caller-saved registers in reverse order. */
    for (int i = n_caller_saved - 1; i >= 0; i--) {
      fprintf(f, "    popq %%%s\n", phys_reg_name[caller_saved_live[i]]);
    }
    break;
  }
  default:
    break;
  }
}

static void ir_asm_assign_alloca_offsets(IRAsmCtx *ctx) {
  Function *fn = ctx->fn;
  int n = 0;
  int np = ctx->num_params;
  uint32_t max_reg = fn->n_regs;
  for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
    Block *blk = fn->blocks[bi];
    if (!blk) continue;
    for (Instr *ins = blk->head; ins; ins = ins->next) {
      if (ins->dst >= max_reg) max_reg = ins->dst + 1;
      for (uint32_t s = 0; s < ins->n_src && s < MAX_OPERANDS; s++)
        if (ins->src[s] >= max_reg) max_reg = ins->src[s] + 1;
    }
  }
  int current_alloca_offset = ctx->slot_base - 8 * (int)(max_reg + 2);
  int total_bytes = 0;
  if (np < 0)
    np = 0;
  for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
    Block *blk = fn->blocks[bi];
    if (!blk)
      continue;
    for (Instr *ins = blk->head; ins; ins = ins->next) {
      if (ins->op != OP_ALLOCA || !ins->dst)
        continue;
      int size = (ins->imm > 0) ? (int)ins->imm : 8;
      size = (size + 7) & ~7;
      int off;
      if (n < np) {
        off = -8 * (n + 1);
      } else {
        current_alloca_offset -= size;
        off = current_alloca_offset;
        total_bytes += size;
      }
      if (ins->dst < MAX_INSTRS) {
        ctx->alloca_off[ins->dst] = off;
        fprintf(stderr, "[ALLOCA] dst=%u off=%d size=%d\n", ins->dst, off, size);
      }
      n++;
    }
  }
  ctx->n_allocas = n;
  ctx->alloca_bytes_total = total_bytes;
}

static void ir_asm_emit_one_block(IRAsmCtx *ctx, BlockID bid) {
  Function *fn = ctx->fn;
  FILE *f = ctx->out;
  Block *blk = fn->blocks[bid];
  if (!blk)
    return;
  fprintf(f, ".Lir_b_%d_%u:\n", ctx->func_label_id, (unsigned)bid);
  Instr *ins = blk->head;
  if (ins && ins->op == OP_PGO_COUNTER_ADDR) {
    ir_asm_emit_probe(ctx, bid);
    for (int k = 0; k < 5 && ins; k++)
      ins = ins->next;
  }
  /* Emit push of callee-saved regs once (first time we emit a real
   * instruction). */
  if (!ctx->body_only && !ctx->callee_saved_emitted &&
      ctx->used_callee_saved_mask) {
    for (int i = 0; i < CALLEE_SAVED_PUSH_N; i++) {
      int p = callee_saved_push_order[i];
      if (ctx->used_callee_saved_mask & (1 << p))
        fprintf(f, "    pushq %%%s\n", phys_reg_name[p]);
    }
    ctx->callee_saved_emitted = 1;
  }
  int last_line = -1;
  for (; ins; ins = ins->next) {
    if (ins->op == OP_NOP)
      continue;
    if (ins->line_no > 0 && ins->line_no != last_line) {
      fprintf(f, "\t.loc 1 %d\n", ins->line_no);
      last_line = ins->line_no;
    }
    ir_asm_lower_insn(ctx, ins, bid);
  }
  /* CG-IR-005 fall-through fixup: if the last instruction in this block is
     not a terminator (OP_BR/OP_CONDBR/OP_RET), emit an explicit jmp to the
     first successor.  After PGO reordering, empty merge blocks and phi-only
     blocks can land anywhere in the emission order; without this they fall
     through into whatever block happens to be next, causing infinite loops. */
  {
    Instr *tail = blk->tail;
    while (tail && tail->op == OP_NOP)
      tail = tail->prev;
    int need_jmp = 1;
    if (tail) {
      Opcode op = tail->op;
      if (op == OP_BR || op == OP_CONDBR || op == OP_RET)
        need_jmp = 0;
    }
    if (need_jmp && blk->n_succs >= 1) {
      BlockID succ0 = blk->succs[0];
      ir_asm_emit_phi_edge_copy(ctx, bid, succ0);
      fprintf(f, "    jmp .Lir_b_%d_%u\n", ctx->func_label_id, (unsigned)succ0);
    }
  }
}

static void ir_asm_emit_function_body(IRAsmCtx *ctx) {
  Function *fn = ctx->fn;
  /* ir_asm_assign_alloca_offsets hoisted to caller */

  /* If no order was provided (e.g. emit path that didn't run passes), build BFS
   * order. */
  if (ctx->n_block_order == 0) {
    bool visited[MAX_BLOCKS];
    memset(visited, 0, sizeof(visited));
    uint32_t queue[MAX_BLOCKS];
    uint32_t head = 0, tail = 0;
    queue[tail++] = fn->entry;
    visited[fn->entry] = true;
    while (head < tail) {
      BlockID bid = queue[head++];
      Block *blk = fn->blocks[bid];
      for (uint32_t si = 0; si < blk->n_succs; si++) {
        BlockID s = blk->succs[si];
        if (s < fn->n_blocks && !visited[s]) {
          visited[s] = true;
          queue[tail++] = s;
        }
      }
    }
    ctx->n_block_order = tail;
    for (uint32_t i = 0; i < tail; i++)
      ctx->block_order[i] = queue[i];
  }

  /* Linear scan register allocation: number instructions, compute intervals,
   * assign phys regs. */
  for (int i = 0; i < MAX_INSTRS; i++)
    ctx->phys_reg[i] = -1;
  ir_asm_number_and_liveness(fn, ctx->block_order, ctx->n_block_order,
                             ctx->def_seq, ctx->last_use);
  ir_asm_linear_scan(fn, ctx->block_order, ctx->n_block_order, ctx->def_seq,
                     ctx->last_use, ctx->phys_reg);

  /* Which callee-saved phys regs (rbx, r12-r15) are used — for
   * prologue/epilogue. */
  ctx->used_callee_saved_mask = 0;
  for (int i = 0; i < MAX_INSTRS; i++)
    if (ctx->phys_reg[i] >= 0 &&
        (PHYS_CALLEE_SAVED_MASK & (1 << ctx->phys_reg[i])))
      ctx->used_callee_saved_mask |= (1 << ctx->phys_reg[i]);

  bool emitted[MAX_BLOCKS];
  memset(emitted, 0, sizeof(emitted));
  /* Phase 1: hot path — emit blocks in PGO/BBR order (maximizes icache
   * locality). */
  for (uint32_t i = 0; i < ctx->n_block_order; i++) {
    BlockID bid = ctx->block_order[i];
    if (bid >= fn->n_blocks)
      continue;
    ir_asm_emit_one_block(ctx, bid);
    emitted[bid] = true;
  }
  /* Phase 2: cold/unreachable — emit any remaining blocks so every branch
   * target has a label. */
  for (BlockID bid = 0; bid < fn->n_blocks; bid++) {
    if (!emitted[bid])
      ir_asm_emit_one_block(ctx, bid);
  }
}

#define PGO_NAME_STRIDE 64

/* Fill metadata for fn: block names (64 bytes each) and edges (2 ints per
 * block: s0, s1; -1 if missing). */
static void ir_asm_fill_metadata(Function *fn, char *out_names,
                                 int *out_edges) {
  for (uint32_t i = 0; i < fn->n_blocks; i++) {
    Block *blk = fn->blocks[i];
    if (out_names) {
      size_t len = 0;
      if (blk) {
        while (len < (size_t)(NAME_LEN - 1) && blk->name[len])
          len++;
      }
      if (blk)
        memcpy(out_names + i * PGO_NAME_STRIDE, blk->name, len);
      memset(out_names + i * PGO_NAME_STRIDE + len, 0, PGO_NAME_STRIDE - len);
    }
    if (out_edges) {
      int s0 = -1, s1 = -1;
      if (blk && blk->n_succs > 0)
        s0 = (int)(uint32_t)blk->succs[0];
      if (blk && blk->n_succs > 1)
        s1 = (int)(uint32_t)blk->succs[1];
      out_edges[i * 2 + 0] = s0;
      out_edges[i * 2 + 1] = s1;
    }
  }
}

/* Build IR, run passes; if ZCC_PGO_INSTRUMENT=1 emit body to out, fill
 * metadata, return n_blocks; else return 0. global_block_offset: add to block
 * ids in probes. func_label_id: unique id for block labels (.Lir_b_%d_%u).
 * out_names: 64 bytes per block; out_edges: 2 ints per block.
 * max_blocks_capacity: if > 0 and fn->n_blocks > capacity, return 0 without
 * emitting (graceful degradation). */
int zcc_run_passes_emit_body(ZCCNode *body_ast, const char *profile_path,
                             const char *func_name, void *out_file,
                             int stack_size, int num_params, int func_end_label,
                             int global_block_offset, int func_label_id,
                             char *out_names, int *out_edges,
                             int max_blocks_capacity) {
  const char *env = getenv("ZCC_PGO_INSTRUMENT");
  if (!env || env[0] != '1')
    return 0;
  FILE *out = (FILE *)out_file;
  (void)stack_size;

  {
    const char *d = getenv("ZCC_PGO_DEBUG_MAIN");
    s_debug_main_emit =
        (d && d[0] == '1' && func_name && strcmp(func_name, "main") == 0) ? 1
                                                                          : 0;
  }

  Function *fn = zcc_ast_to_ir(body_ast, func_name);
  if (!fn)
    return 0;

  PassResult *result = (PassResult *)calloc(1, sizeof(PassResult));
  if (!result) {
    zcc_ir_free(fn);
    return 0;
  }
  run_all_passes(fn, result, profile_path, num_params);

  /* When debugging main crash: dump full IR so we can map faulting block/reg 22
   * back to defs. */
  if (s_debug_main_emit) {
    fprintf(stderr, "[PGO-DEBUG-IR] === main() IR (after passes) ===\n");
    uint32_t *order = result->order;
    uint32_t n_order = result->n_blocks <= MAX_BLOCKS ? result->n_blocks : 0;
    bool dumped[MAX_BLOCKS];
    for (uint32_t i = 0; i < fn->n_blocks && i < MAX_BLOCKS; i++)
      dumped[i] = false;
    for (uint32_t oi = 0; oi < (n_order ? n_order : fn->n_blocks); oi++) {
      BlockID bid = n_order ? order[oi] : oi;
      if (bid >= fn->n_blocks || !fn->blocks[bid])
        continue;
      dumped[bid] = true;
      Block *blk = fn->blocks[bid];
      fprintf(stderr, "[PGO-DEBUG-IR] block %u (%s) preds=%u succs=%u\n",
              (unsigned)bid, blk->name, blk->n_preds, blk->n_succs);
      for (Instr *ins = blk->head; ins; ins = ins->next) {
        const char *op =
            (unsigned)ins->op < sizeof(opcode_name) / sizeof(opcode_name[0])
                ? opcode_name[ins->op]
                : "?";
        fprintf(stderr, "[PGO-DEBUG-IR]   %s dst=%u", op, ins->dst);
        for (uint32_t s = 0; s < ins->n_src && s < MAX_OPERANDS; s++)
          fprintf(stderr, " src[%u]=%u", s, ins->src[s]);
        if (ins->op == OP_PHI && ins->n_phi) {
          fprintf(stderr, " phi");
          for (uint32_t p = 0; p < ins->n_phi; p++)
            fprintf(stderr, " %u:B%u", ins->phi[p].reg,
                    (unsigned)ins->phi[p].block);
        }
        if (ins->imm)
          fprintf(stderr, " imm=%lld", (long long)ins->imm);
        if (ins->call_name[0])
          fprintf(stderr, " call=%s", ins->call_name);
        fprintf(stderr, "\n");
      }
    }
    /* Phase-2 blocks (cold/unreachable): include so faulting block 4 is
     * visible. */
    for (BlockID bid = 0; bid < fn->n_blocks; bid++) {
      if (dumped[bid] || !fn->blocks[bid])
        continue;
      Block *blk = fn->blocks[bid];
      fprintf(stderr,
              "[PGO-DEBUG-IR] block %u (%s) preds=%u succs=%u [cold/unreach]\n",
              (unsigned)bid, blk->name, blk->n_preds, blk->n_succs);
      for (Instr *ins = blk->head; ins; ins = ins->next) {
        const char *op =
            (unsigned)ins->op < sizeof(opcode_name) / sizeof(opcode_name[0])
                ? opcode_name[ins->op]
                : "?";
        fprintf(stderr, "[PGO-DEBUG-IR]   %s dst=%u", op, ins->dst);
        for (uint32_t s = 0; s < ins->n_src && s < MAX_OPERANDS; s++)
          fprintf(stderr, " src[%u]=%u", s, ins->src[s]);
        if (ins->op == OP_PHI && ins->n_phi) {
          fprintf(stderr, " phi");
          for (uint32_t p = 0; p < ins->n_phi; p++)
            fprintf(stderr, " %u:B%u", ins->phi[p].reg,
                    (unsigned)ins->phi[p].block);
        }
        if (ins->imm)
          fprintf(stderr, " imm=%lld", (long long)ins->imm);
        if (ins->call_name[0])
          fprintf(stderr, " call=%s", ins->call_name);
        fprintf(stderr, "\n");
      }
    }
    fprintf(stderr, "[PGO-DEBUG-IR] === end main IR ===\n");
  }

  if (max_blocks_capacity > 0 &&
      (int)(uint32_t)fn->n_blocks > max_blocks_capacity) {
    fprintf(stderr,
            "[PGO] Warning: global block limit exceeded (%u blocks). Skipping "
            "bridge for this function.\n",
            (unsigned)fn->n_blocks);
    free(result);
    zcc_ir_free(fn);
    s_debug_main_emit = 0;
    return 0;
  }

  IRAsmCtx ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.out = out;
  ctx.fn = fn;
  ctx.func_end_label = func_end_label;
  ctx.num_params = num_params;
  ctx.global_block_offset = global_block_offset;
  ctx.func_label_id = func_label_id;
  for (int i = 0; i < MAX_INSTRS; i++)
    ctx.alloca_off[i] = 0;
  /* Use PGO/BBR emission order from passes so layout respects profile. */
  ctx.n_block_order = result->n_blocks <= MAX_BLOCKS ? result->n_blocks : 0;
  for (uint32_t i = 0; i < ctx.n_block_order; i++)
    ctx.block_order[i] = result->order[i];

  ir_asm_assign_alloca_offsets(&ctx);
  ir_asm_emit_function_body(&ctx);
  ir_asm_fill_metadata(fn, out_names, out_edges);

  fprintf(stderr,
          "[ZCC-IR] fn=%s  emitted from IR (PGO instrumented) %u blocks\n",
          func_name ? func_name : "?", (unsigned)fn->n_blocks);
  int n_blocks = (int)(uint32_t)fn->n_blocks;
  free(result);
  zcc_ir_free(fn);
  s_debug_main_emit = 0;
  return n_blocks;
}

/* Emit body from IR with PGO block order (no instrumentation). Used when
 * -use-profile=... and bridge=1. */
int zcc_run_passes_emit_body_pgo(ZCCNode *body_ast, const char *profile_path,
                                 const char *func_name, void *out_file,
                                 int stack_size, int num_params,
                                 int func_end_label, int func_label_id) {
  FILE *out = (FILE *)out_file;
  Function *fn = zcc_ast_to_ir(body_ast, func_name);
  if (!fn)
    return 0;

  if (func_name && strcmp(func_name, "main") == 0) {
    fprintf(stderr, "[DEBUG] RAW IR FOR MAIN BEFORE PASSES:\n");
    for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
      Block *b = fn->blocks[bi];
      if (!b)
        continue;
      fprintf(stderr, " Block %u:\n", bi);
      for (Instr *ins = b->head; ins; ins = ins->next) {
        if (ins->op == OP_STORE) {
          fprintf(stderr, "   OP_STORE src[0]=%u src[1]=%u\n", ins->src[0],
                  ins->src[1]);
        } else if (ins->op == OP_CONST) {
          fprintf(stderr, "   OP_CONST dst=%u imm=%d\n", ins->dst,
                  (int)(intptr_t)ins->imm);
        }
      }
    }
  }

  PassResult *result = (PassResult *)calloc(1, sizeof(PassResult));
  if (!result) {
    zcc_ir_free(fn);
    return 0;
  }
  run_all_passes(fn, result, profile_path, num_params);

  IRAsmCtx ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.out = out;
  ctx.fn = fn;
  ctx.func_end_label = func_end_label;
  ctx.num_params = num_params;
  ctx.global_block_offset = 0;
  ctx.func_label_id = func_label_id;
  ctx.body_only =
      1; /* AST owns prologue/epilogue -- skip push/pop of callee-saved */
  ctx.slot_base = -stack_size; /* CG-IR-008: IR slots start below AST frame */
  int ir_extra = 0;
  for (int i = 0; i < MAX_INSTRS; i++)
    ctx.alloca_off[i] = 0;
  ctx.n_block_order = result->n_blocks <= MAX_BLOCKS ? result->n_blocks : 0;
  for (uint32_t i = 0; i < ctx.n_block_order; i++)
    ctx.block_order[i] = result->order[i];

    /* CG-IR-009: scan IR to compute exact frame depth before emission.
   * Must happen BEFORE ir_asm_emit_function_body since alloca offsets
   * and spill slots all live below slot_base.
   * CG-IR-016-CSAVE-V2: ir_extra extended by n_csave_slots=5 to give
   * each callee-saved register its own explicit 8-byte slot at the very
   * bottom of the frame.  csave_base is the highest of those 5 slots.  */
  {
    ir_asm_assign_alloca_offsets(&ctx);
    
    uint32_t max_reg = fn->n_regs;
    for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
      Block *b = fn->blocks[bi];
      if (!b) continue;
      for (Instr *ins = b->head; ins; ins = ins->next) {
        if (ins->dst >= max_reg) max_reg = ins->dst + 1;
        for (uint32_t s = 0; s < ins->n_src && s < MAX_OPERANDS; s++)
          if (ins->src[s] >= max_reg) max_reg = ins->src[s] + 1;
      }
    }
    
    int n_csave_slots = 5; /* rbx, r12, r13, r14, r15 — always reserve all 5 */
    ctx.ir_extra = 8 * ((int)max_reg + 8 + n_csave_slots) + ctx.alloca_bytes_total;
    ctx.ir_extra = (ctx.ir_extra + 15) & ~15;
    ir_extra = ctx.ir_extra;
    fprintf(out, "    subq $%d, %%rsp\n", ctx.ir_extra);

    /* CG-IR-016-CSAVE-V3: csave_base is the %rbp-relative offset of the
     * highest callee-save slot (rbx). It sits at the extreme bottom of the 
     * expanded ir_extra frame mathematically avoiding all dynamically allocated buffers. */
    ctx.csave_base = -(stack_size + ctx.ir_extra) + 32;
  }

  /* CG-IR-016-CSAVE-V2: save callee-saved registers via movq (NOT pushq —
   * pushq would shift RSP and break every slot_base-relative spill access).
   * All 5 are always saved unconditionally; cost is 5 movq at entry/exit.  */
  fprintf(out, "    movq %%rbx, %d(%%rbp)\n", ctx.csave_base);
  fprintf(out, "    movq %%r12, %d(%%rbp)\n", ctx.csave_base - 8);
  fprintf(out, "    movq %%r13, %d(%%rbp)\n", ctx.csave_base - 16);
  fprintf(out, "    movq %%r14, %d(%%rbp)\n", ctx.csave_base - 24);
  fprintf(out, "    movq %%r15, %d(%%rbp)\n", ctx.csave_base - 32);

  ir_asm_emit_function_body(&ctx);

  /* CG-IR-016-CSAVE-V2: restore callee-saved registers before AST epilogue */
  fprintf(out, "    movq %d(%%rbp), %%rbx\n", ctx.csave_base);
  fprintf(out, "    movq %d(%%rbp), %%r12\n", ctx.csave_base - 8);
  fprintf(out, "    movq %d(%%rbp), %%r13\n", ctx.csave_base - 16);
  fprintf(out, "    movq %d(%%rbp), %%r14\n", ctx.csave_base - 24);
  fprintf(out, "    movq %d(%%rbp), %%r15\n", ctx.csave_base - 32);

  fprintf(stderr, "[ZCC-IR] fn=%s  emitted from IR (PGO layout) %u blocks\n",
          func_name ? func_name : "?", (unsigned)fn->n_blocks);
  int n_blocks = (int)(uint32_t)fn->n_blocks;
  free(result);
  zcc_ir_free(fn);
  return n_blocks;
}

/* Opaque ABI boundary: ZCC calls this with (ir_tree, profile_path, func_name).
 * We own PassResult (calloc/free) and all result field access — avoids layout
 * mismatch (ZCC uint32_t vs GCC uint32_t) and stack overflow (no large struct
 * on ZCC stack). */
void zcc_run_passes_log(ZCCNode *body_ast, const char *profile_path,
                        const char *func_name) {
  Function *fn = zcc_ast_to_ir(body_ast, func_name);
  if (!fn)
    return;
  PassResult *result = (PassResult *)calloc(1, sizeof(PassResult));
  if (!result) {
    zcc_ir_free(fn);
    return;
  }
  run_all_passes(fn, result, profile_path, 0);
  fprintf(stderr, "[ZCC-IR] fn=%s  blocks=%u  hoisted=%u  dce=%u\n",
          func_name ? func_name : "?", result->n_blocks, result->licm_hoisted,
          result->dce_instrs_removed);
  free(result);
  zcc_ir_free(fn);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * TEST FIXTURE  — constructs a small synthetic CFG to exercise all three passes
 *
 *   entry → loop_header → loop_body → loop_latch ──(back)──┐
 *                  │                                        │
 *                  └──(exit cond)──→ loop_exit → fn_exit    │
 *                                                           │
 *   dead_block (unreachable) ─────────────────────────────-─┘
 *
 *   Allocations:
 *     %10 = alloca 64   ; local buffer — not passed to call → stack-promotable
 *     %20 = alloca 8    ; its address returned → escapes
 *
 *   Dead instructions:
 *     %30 = add %31, %32   ; %30 never used → DCE removes it
 * ───────────────────────────────────────────────────────────────────────────
 */

#ifdef ZCC_BRIDGE_STANDALONE
static Instr *make_instr(InstrID id, Opcode op, RegID dst, RegID *srcs,
                         uint32_t n_src) {
  Instr *ins = calloc(1, sizeof(Instr));
  ins->id = id;
  ins->op = op;
  ins->dst = dst;
  ins->n_src = n_src;
  if (srcs)
    memcpy(ins->src, srcs, n_src * sizeof(RegID));
  ins->exec_freq = 1.0;
  return ins;
}

static void block_append(Block *blk, Instr *ins) {
  if (!blk->head) {
    blk->head = blk->tail = ins;
  } else {
    blk->tail->next = ins;
    ins->prev = blk->tail;
    blk->tail = ins;
  }
  blk->n_instrs++;
}
#endif /* ZCC_BRIDGE_STANDALONE */

/* Unlink instruction from its block (caller must know the block) */
static void __attribute__((unused)) instr_unlink(Block *blk, Instr *ins) {
  if (ins->prev)
    ins->prev->next = ins->next;
  else
    blk->head = ins->next;
  if (ins->next)
    ins->next->prev = ins->prev;
  else
    blk->tail = ins->prev;
  blk->n_instrs--;
}

/* Insert ins before `before` in blk */
static void __attribute__((unused))
instr_insert_before(Block *blk, Instr *before, Instr *ins) {
  ins->next = before;
  ins->prev = before->prev;
  if (before->prev)
    before->prev->next = ins;
  else
    blk->head = ins;
  before->prev = ins;
  blk->n_instrs++;
}

static BlockID __attribute__((unused)) block_of_instr(Function *fn,
                                                      const Instr *needle) {
  for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
    for (Instr *ins = fn->blocks[bi]->head; ins; ins = ins->next)
      if (ins == needle)
        return (BlockID)bi;
  }
  return MAX_BLOCKS;
}

#ifdef ZCC_BRIDGE_STANDALONE
static Function *build_test_function(void) {
  Function *fn = calloc(1, sizeof(Function));

  /* Allocate 5 blocks */
  const char *names[] = {"entry", "loop_header", "loop_body", "loop_exit",
                         "fn_exit"};
  for (uint32_t i = 0; i < 5; i++) {
    fn->blocks[i] = calloc(1, sizeof(Block));
    fn->blocks[i]->id = i;
    strncpy(fn->blocks[i]->name, names[i], NAME_LEN - 1);
    fn->blocks[i]->exec_freq = 0.0;
  }
  fn->n_blocks = 5;
  fn->entry = 0;
  fn->exit = 4;

  /* CFG edges */
  fn->blocks[0]->succs[0] = 1;
  fn->blocks[0]->n_succs = 1;
  fn->blocks[0]->branch_prob[0] = 1.0;

  fn->blocks[1]->succs[0] = 2;
  fn->blocks[1]->succs[1] = 3;
  fn->blocks[1]->n_succs = 2;
  fn->blocks[1]->branch_prob[0] = 0.9; /* mostly stay in loop */
  fn->blocks[1]->branch_prob[1] = 0.1;

  fn->blocks[2]->succs[0] = 1;
  fn->blocks[2]->n_succs = 1;
  fn->blocks[2]->branch_prob[0] = 1.0;

  fn->blocks[3]->succs[0] = 4;
  fn->blocks[3]->n_succs = 1;
  fn->blocks[3]->branch_prob[0] = 1.0;

  fn->blocks[4]->n_succs = 0; /* exit */

  /* Predecessors */
  fn->blocks[1]->preds[0] = 0;
  fn->blocks[1]->preds[1] = 2;
  fn->blocks[1]->n_preds = 2;
  fn->blocks[2]->preds[0] = 1;
  fn->blocks[2]->n_preds = 1;
  fn->blocks[3]->preds[0] = 1;
  fn->blocks[3]->n_preds = 1;
  fn->blocks[4]->preds[0] = 3;
  fn->blocks[4]->n_preds = 1;

  /* ── entry block ── */
  Block *entry = fn->blocks[0];
  /* %10 = alloca 64 — stack-promotable */
  Instr *a1 = make_instr(1, OP_ALLOCA, 10, (RegID[]){64}, 1);
  /* %20 = alloca 8  — escapes via ret  */
  Instr *a2 = make_instr(2, OP_ALLOCA, 20, (RegID[]){8}, 1);
  /* %30 = add %31, %32 — dead (never used) */
  Instr *dead = make_instr(3, OP_ADD, 30, (RegID[]){31, 32}, 2);
  /* br loop_header */
  Instr *br = make_instr(4, OP_BR, 0, (RegID[]){1}, 1);
  block_append(entry, a1);
  block_append(entry, a2);
  block_append(entry, dead);
  block_append(entry, br);

  fn->def_of[10] = a1;
  fn->def_of[20] = a2;
  fn->def_of[30] = dead;

  /* ── loop_header ── */
  Block *lhdr = fn->blocks[1];
  /* %40 = phi(%41:entry, %42:loop_body) — induction variable */
  Instr *phi = make_instr(5, OP_PHI, 40, NULL, 0);
  phi->phi[0] = (PhiSource){41, 0}; /* from entry     */
  phi->phi[1] = (PhiSource){42, 2}; /* from loop_body */
  phi->n_phi = 2;
  /* condbr %40, loop_body, loop_exit */
  Instr *cbr = make_instr(6, OP_CONDBR, 0, (RegID[]){40, 2, 3}, 3);
  block_append(lhdr, phi);
  block_append(lhdr, cbr);
  fn->def_of[40] = phi;

  /* ── loop_body ── */
  Block *lbod = fn->blocks[2];
  /* store value into %10 (local buf — no escape via store to local ptr) */
  Instr *st = make_instr(7, OP_STORE, 0, (RegID[]){99, 10}, 2);
  /* %42 = add %40, 1 */
  Instr *inc = make_instr(8, OP_ADD, 42, (RegID[]){40, 1}, 2);
  /* br loop_header */
  Instr *lbr = make_instr(9, OP_BR, 0, (RegID[]){1}, 1);
  block_append(lbod, st);
  block_append(lbod, inc);
  block_append(lbod, lbr);
  fn->def_of[42] = inc;

  /* ── loop_exit ── */
  Block *lexit = fn->blocks[3];
  Instr *ebr = make_instr(10, OP_BR, 0, (RegID[]){4}, 1);
  block_append(lexit, ebr);

  /* ── fn_exit ── */
  Block *fexit = fn->blocks[4];
  /* ret %20 → %20 escapes */
  Instr *ret = make_instr(11, OP_RET, 0, (RegID[]){20}, 1);
  block_append(fexit, ret);

  fn->n_regs = 100;
  return fn;
}

/* Nested while/if fixture: 7 blocks, ~20 value instructions +
 * allocas/stores/br. Matches the shape of tmp_nested_while_if.c for DCE
 * before/after validation. */
static Function *build_nested_while_if_fixture(void) {
  Function *fn = calloc(1, sizeof(Function));
  const char *names[] = {"entry",     "while.head", "while.body", "if.then.0",
                         "if.else.0", "if.merge.0", "while.exit"};
  for (uint32_t i = 0; i < 7; i++) {
    fn->blocks[i] = calloc(1, sizeof(Block));
    fn->blocks[i]->id = i;
    strncpy(fn->blocks[i]->name, names[i], NAME_LEN - 1);
    fn->blocks[i]->exec_freq = 0.0;
  }
  fn->n_blocks = 7;
  fn->entry = 0;
  fn->exit = 6;

  /* CFG */
  fn->blocks[0]->succs[0] = 1;
  fn->blocks[0]->n_succs = 1;
  fn->blocks[1]->succs[0] = 2;
  fn->blocks[1]->succs[1] = 6;
  fn->blocks[1]->n_succs = 2;
  fn->blocks[2]->succs[0] = 3;
  fn->blocks[2]->succs[1] = 4;
  fn->blocks[2]->n_succs = 2;
  fn->blocks[3]->succs[0] = 5;
  fn->blocks[3]->n_succs = 1;
  fn->blocks[4]->succs[0] = 5;
  fn->blocks[4]->n_succs = 1;
  fn->blocks[5]->succs[0] = 1;
  fn->blocks[5]->n_succs = 1;
  fn->blocks[6]->n_succs = 0;

  fn->blocks[1]->preds[0] = 0;
  fn->blocks[1]->preds[1] = 5;
  fn->blocks[1]->n_preds = 2;
  fn->blocks[2]->preds[0] = 1;
  fn->blocks[2]->n_preds = 1;
  fn->blocks[3]->preds[0] = 2;
  fn->blocks[3]->n_preds = 1;
  fn->blocks[4]->preds[0] = 2;
  fn->blocks[4]->n_preds = 1;
  fn->blocks[5]->preds[0] = 3;
  fn->blocks[5]->preds[1] = 4;
  fn->blocks[5]->n_preds = 2;
  fn->blocks[6]->preds[0] = 1;
  fn->blocks[6]->n_preds = 1;

  /* Branch probabilities (required for ir_validate when n_succs > 1) */
  fn->blocks[1]->branch_prob[0] = 0.9; /* while.head: true -> while.body */
  fn->blocks[1]->branch_prob[1] = 0.1; /* false -> while.exit */
  fn->blocks[2]->branch_prob[0] = 0.5; /* while.body: true -> if.then.0 */
  fn->blocks[2]->branch_prob[1] = 0.5; /* false -> if.else.0 */

  /* Allocas: %50 = i, %51 = r */
  Block *entry = fn->blocks[0];
  Instr *ai = make_instr_imm(0, OP_ALLOCA, 50, 8, 0);
  Instr *ar = make_instr_imm(1, OP_ALLOCA, 51, 8, 0);
  Instr *c0 = make_instr_imm(2, OP_CONST, 0, 0, 0);
  Instr *c1 = make_instr_imm(3, OP_CONST, 1, 0, 0);
  Instr *si = make_instr(4, OP_STORE, 0, (RegID[]){0, 50}, 2);
  Instr *sr = make_instr(5, OP_STORE, 0, (RegID[]){1, 51}, 2);
  Instr *br_head = make_instr(6, OP_BR, 0, (RegID[]){1}, 1);
  block_append(entry, ai);
  block_append(entry, ar);
  block_append(entry, c0);
  block_append(entry, c1);
  block_append(entry, si);
  block_append(entry, sr);
  block_append(entry, br_head);

  Block *head = fn->blocks[1];
  Instr *li = make_instr(7, OP_LOAD, 2, (RegID[]){50}, 1);
  Instr *c10 = make_instr_imm(8, OP_CONST, 3, 10, 0);
  Instr *lt = make_instr(9, OP_LT, 4, (RegID[]){2, 3}, 2);
  Instr *cbr = make_instr(10, OP_CONDBR, 0, (RegID[]){4, 2, 6}, 3);
  block_append(head, li);
  block_append(head, c10);
  block_append(head, lt);
  block_append(head, cbr);

  Block *body = fn->blocks[2];
  Instr *li2 = make_instr(11, OP_LOAD, 5, (RegID[]){50}, 1);
  Instr *c2 = make_instr_imm(12, OP_CONST, 6, 2, 0);
  Instr *mod = make_instr(13, OP_MOD, 7, (RegID[]){5, 6}, 2);
  Instr *c0b = make_instr_imm(14, OP_CONST, 8, 0, 0);
  Instr *lt2 = make_instr(15, OP_LT, 9, (RegID[]){8, 7}, 2);
  Instr *cbr2 = make_instr(16, OP_CONDBR, 0, (RegID[]){9, 3, 4}, 3);
  block_append(body, li2);
  block_append(body, c2);
  block_append(body, mod);
  block_append(body, c0b);
  block_append(body, lt2);
  block_append(body, cbr2);

  Block *then0 = fn->blocks[3];
  Instr *lr = make_instr(17, OP_LOAD, 10, (RegID[]){51}, 1);
  Instr *c1b = make_instr_imm(18, OP_CONST, 11, 1, 0);
  Instr *add1 = make_instr(19, OP_ADD, 12, (RegID[]){10, 11}, 2);
  Instr *st_r1 = make_instr(20, OP_STORE, 0, (RegID[]){12, 51}, 2);
  Instr *br_merge = make_instr(21, OP_BR, 0, (RegID[]){5}, 1);
  block_append(then0, lr);
  block_append(then0, c1b);
  block_append(then0, add1);
  block_append(then0, st_r1);
  block_append(then0, br_merge);

  Block *else0 = fn->blocks[4];
  Instr *lr2 = make_instr(22, OP_LOAD, 13, (RegID[]){51}, 1);
  Instr *c2b = make_instr_imm(23, OP_CONST, 14, 2, 0);
  Instr *add2 = make_instr(24, OP_ADD, 15, (RegID[]){13, 14}, 2);
  Instr *st_r2 = make_instr(25, OP_STORE, 0, (RegID[]){15, 51}, 2);
  Instr *br_merge2 = make_instr(26, OP_BR, 0, (RegID[]){5}, 1);
  block_append(else0, lr2);
  block_append(else0, c2b);
  block_append(else0, add2);
  block_append(else0, st_r2);
  block_append(else0, br_merge2);

  Block *merge0 = fn->blocks[5];
  Instr *li3 = make_instr(27, OP_LOAD, 16, (RegID[]){50}, 1);
  Instr *c1c = make_instr_imm(28, OP_CONST, 17, 1, 0);
  Instr *add3 = make_instr(29, OP_ADD, 18, (RegID[]){16, 17}, 2);
  Instr *st_i = make_instr(30, OP_STORE, 0, (RegID[]){18, 50}, 2);
  Instr *br_back = make_instr(31, OP_BR, 0, (RegID[]){1}, 1);
  block_append(merge0, li3);
  block_append(merge0, c1c);
  block_append(merge0, add3);
  block_append(merge0, st_i);
  block_append(merge0, br_back);

  Block *exit_blk = fn->blocks[6];
  Instr *lr3 = make_instr(32, OP_LOAD, 19, (RegID[]){51}, 1);
  Instr *ret = make_instr(33, OP_RET, 0, (RegID[]){19}, 1);
  block_append(exit_blk, lr3);
  block_append(exit_blk, ret);

  fn->n_regs = 52;
  return fn;
}

static void ir_dump(const Function *fn, const char *label) {
  fprintf(stderr, "\n=== IR DUMP: %s ===\n", label);
  for (uint32_t b = 0; b < fn->n_blocks; b++) {
    const Block *blk = fn->blocks[b];
    if (!blk)
      continue;
    fprintf(stderr, "%s:\n", blk->name);
    for (const Instr *ins = blk->head; ins; ins = ins->next) {
      int has_dst = (ins->op != OP_BR && ins->op != OP_CONDBR &&
                     ins->op != OP_STORE && ins->op != OP_RET);
      if (has_dst)
        fprintf(stderr, "  %%%u = ", (unsigned)ins->dst);
      else
        fprintf(stderr, "  ");
      fprintf(stderr, "%s", opcode_name[ins->op]);
      if (ins->op == OP_CONST)
        fprintf(stderr, " %lld", (long long)ins->imm);
      for (uint32_t i = 0; i < ins->n_src; i++)
        fprintf(stderr, " %%%u", (unsigned)ins->src[i]);
      fprintf(stderr, "\n");
    }
  }
  fprintf(stderr, "=== END DUMP ===\n\n");
}
#endif /* ZCC_BRIDGE_STANDALONE */

#ifdef ZCC_BRIDGE_STANDALONE
static void print_pass_results(const Function *fn, const PassResult *result) {
  printf("\n=================================================\n");
  printf(" ZKAEDI PRIME - Compiler Pass Results\n");
  printf("=================================================\n");

  printf("\n-- Pass 1: DCE -> SSA Bridge\n");
  printf("|  Instructions removed : %u\n", fn->stats.dce_instrs_removed);
  printf("|  Blocks removed       : %u\n", fn->stats.dce_blocks_removed);

  printf("\n-- Pass 2: LICM\n");
  printf("|  Instructions hoisted : %u\n", fn->stats.licm_hoisted);

  printf("\n-- Pass 3: Escape Analysis\n");
  printf("|  Allocations promoted : %u\n", fn->stats.ea_promotions);

  printf("\n-- Pass 4: PGO BB Reordering\n");
  printf("|  Emission order       : ");
  for (uint32_t i = 0; i < result->n_blocks; i++) {
    BlockID bid = result->order[i];
    printf("%s", fn->blocks[bid]->name);
    if (i + 1 < result->n_blocks)
      printf(" -> ");
  }
  printf("\n|  Execution frequencies:\n");
  for (uint32_t i = 0; i < result->n_blocks; i++) {
    BlockID bid = result->order[i];
    printf("|    %-16s  freq = %.4f\n", fn->blocks[bid]->name,
           fn->blocks[bid]->exec_freq);
  }

  printf("\n=================================================\n\n");
}
#endif /* ZCC_BRIDGE_STANDALONE */

static void free_function(Function *fn) {
  for (uint32_t i = 0; i < fn->n_blocks; i++) {
    Block *blk = fn->blocks[i];
    if (!blk)
      continue;
    Instr *cur = blk->head;
    while (cur) {
      Instr *n = cur->next;
      free(cur);
      cur = n;
    }
    free(blk);
  }
  free(fn);
}

void zcc_ir_free(struct Function *fn) {
  if (!fn)
    return;
  free_function((Function *)fn);
}

#ifdef ZCC_BRIDGE_STANDALONE
int main(void) {
  Function *fn = build_test_function();
  PassResult result;
  memset(&result, 0, sizeof(result));

  fprintf(stderr,
          "\n-- Running ZKAEDI PRIME pass pipeline (synthetic fixture) --\n");
  run_all_passes(fn, &result, NULL);
  print_pass_results(fn, &result);
  free_function(fn);

  /* Phase B: AST → IR bridge — real C-like control flow */
  {
    ASTNode *body = build_phase_b_ast();
    Function *fn_b = ast_to_ir(body);
    PassResult result_b;
    memset(&result_b, 0, sizeof(result_b));
    fprintf(stderr,
            "\n-- Phase B: AST->IR (while i<10 { i=i+1 } return i) --\n");
    run_all_passes(fn_b, &result_b, NULL);
    printf("\n=================================================\n");
    printf(" Phase B - AST->IR pass results\n");
    printf("=================================================\n");
    print_pass_results(fn_b, &result_b);
    free_function(fn_b);
  }

  /* Phase B ZCC: ZCC-shaped AST → IR — validate contract (DCE, LICM, PGO) */
  {
    ZCCNode *zbody = build_zcc_phase_b_ast();
    Function *fn_z = zcc_ast_to_ir(zbody, NULL);
    if (!ir_validate(fn_z)) {
      fprintf(stderr, "[ZCC Phase B] ir_validate failed before passes\n");
    }
    PassResult result_z;
    memset(&result_z, 0, sizeof(result_z));
    fprintf(stderr, "\n-- Phase B ZCC: ZCC-shaped AST->IR (while i<10 { i=i+1 "
                    "} return i) --\n");
    run_all_passes(fn_z, &result_z, NULL);
    printf("\n=================================================\n");
    printf(" Phase B ZCC - ZCC->IR pass results\n");
    printf("=================================================\n");
    print_pass_results(fn_z, &result_z);
    /* Contract: entry has 0 preds; block count ~5 (entry, while.head,
     * while.body, while.exit, exit); LICM hoists consts */
    printf("  Blocks: %u  LICM hoisted: %u  DCE removed: %u  PGO order: %u\n",
           fn_z->n_blocks, fn_z->stats.licm_hoisted,
           fn_z->stats.dce_instrs_removed, result_z.n_blocks);
    if (!ir_validate(fn_z))
      fprintf(stderr, "[ZCC Phase B] ir_validate failed after passes\n");
    free_function(fn_z);
  }

  /* DCE before/after validation: nested while/if fixture */
  {
    Function *fn_dce = build_nested_while_if_fixture();
    ir_dump(fn_dce, "BEFORE DCE");
    PassResult result_dce;
    memset(&result_dce, 0, sizeof(result_dce));
    run_all_passes(fn_dce, &result_dce, NULL);
    ir_dump(fn_dce, "AFTER DCE");
    fprintf(stderr, "DCE removed: %u instructions, %u blocks\n",
            result_dce.dce_instrs_removed, fn_dce->stats.dce_blocks_removed);
    /* Validation: every RegID referenced in a surviving ins must be defined in
     * the AFTER dump */
    {
      uint64_t defined[8] = {0};
      for (uint32_t b = 0; b < fn_dce->n_blocks; b++) {
        const Block *blk = fn_dce->blocks[b];
        if (!blk)
          continue;
        for (const Instr *ins = blk->head; ins; ins = ins->next) {
          if (ins->op != OP_BR && ins->op != OP_CONDBR && ins->op != OP_STORE &&
              ins->op != OP_RET && ins->dst < 512)
            defined[ins->dst >> 6] |= (1ULL << (ins->dst & 63));
        }
      }
      for (uint32_t b = 0; b < fn_dce->n_blocks; b++) {
        const Block *blk = fn_dce->blocks[b];
        if (!blk)
          continue;
        for (const Instr *ins = blk->head; ins; ins = ins->next) {
          for (uint32_t i = 0; i < ins->n_src; i++) {
            RegID r = ins->src[i];
            if (r >= 512)
              continue;
            if (!(defined[r >> 6] & (1ULL << (r & 63))))
              fprintf(
                  stderr,
                  "DCE/LICM BUG: surviving ins references undefined RegID %u\n",
                  (unsigned)r);
          }
        }
      }
    }
    free_function(fn_dce);
  }

  return 0;
}
#endif /* ZCC_BRIDGE_STANDALONE */