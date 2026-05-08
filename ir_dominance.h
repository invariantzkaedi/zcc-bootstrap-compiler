/*
 * ir_dominance.h — Basic Block Graph & Dominator Tree for ZCC IR
 * ===============================================================
 * PURPOSE: Partition the linear ir_func_t node list into a Basic Block
 *   graph, compute immediate dominators via the iterative data-flow
 *   algorithm, and expose the dominator tree for GVN and other passes.
 *
 * DESIGN:
 *   - Static allocation only.  Max 512 basic blocks per function
 *     (EVM contracts rarely exceed 200 blocks).
 *   - O(V²) iterative algorithm — sufficient for EVM CFG sizes.
 *   - The dominator tree is stored as an idom[] array and a children
 *     adjacency list for pre-order traversal.
 *
 * INTEGRATION:
 *   Call dom_build_cfg() then dom_compute_idom() on an ir_func_t
 *   before GVN.  GVN reads the dominator tree via dom_get_cfg().
 */

#ifndef ZCC_IR_DOMINANCE_H
#define ZCC_IR_DOMINANCE_H

#include "ir.h"
#include "ir_pass_manager.h"

/* ── Configuration ───────────────────────────────────────────────────── */
#define DOM_MAX_BLOCKS   512
#define DOM_MAX_PREDS    32    /* max predecessors per block */
#define DOM_MAX_SUCCS    4     /* max successors (BR_IF has 2: taken + fallthrough) */
#define DOM_MAX_CHILDREN 32    /* max children in dominator tree */

/* ── Basic Block ─────────────────────────────────────────────────────── */
typedef struct {
    int           id;                        /* block index, 0 = entry */
    ir_node_t    *first;                     /* first node in block    */
    ir_node_t    *last;                      /* last node in block     */
    char          label[IR_LABEL_MAX];       /* label (if starts with IR_LABEL) */

    /* CFG edges */
    int           pred[DOM_MAX_PREDS];       /* predecessor block ids  */
    int           pred_count;
    int           succ[DOM_MAX_SUCCS];       /* successor block ids    */
    int           succ_count;

    /* Dominator tree */
    int           idom;                      /* immediate dominator (-1 = none) */
    int           children[DOM_MAX_CHILDREN]; /* dominated children     */
    int           child_count;
    int           dom_order;                 /* pre-order index in dom tree */
} dom_bb_t;

/* ── CFG context (per-function) ──────────────────────────────────────── */
typedef struct {
    dom_bb_t      blocks[DOM_MAX_BLOCKS];
    int           block_count;
    ir_func_t    *fn;                        /* back-reference */
} dom_cfg_t;

/* ── Public API ──────────────────────────────────────────────────────── */

/*
 * dom_build_cfg: partition fn's node list into basic blocks and
 * wire predecessor/successor edges.  Returns 0 on success, -1 on overflow.
 */
int dom_build_cfg(dom_cfg_t *cfg, ir_func_t *fn);

/*
 * dom_compute_idom: compute the immediate dominator for every block
 * using the iterative data-flow algorithm.  Requires dom_build_cfg first.
 */
void dom_compute_idom(dom_cfg_t *cfg);

/*
 * dom_build_tree: populate children[] arrays from the idom[] data
 * and assign pre-order traversal indices.  Requires dom_compute_idom first.
 */
void dom_build_tree(dom_cfg_t *cfg);

/*
 * dom_find_block_for_node: return the block id containing the given
 * IR node, or -1 if not found.
 */
int dom_find_block_for_node(const dom_cfg_t *cfg, const ir_node_t *node);

/*
 * ir_pass_dominance: compute the dominator tree for the function.
 * This is a setup pass — it stores the tree in a global context
 * that the upgraded GVN pass reads.
 *
 * Conforms to ir_pass_fn (void* → ir_pass_result_t).
 */
ir_pass_result_t ir_pass_dominance(void *fn_ptr);

/*
 * dom_get_cfg: retrieve the last-computed CFG context.
 * Only valid after ir_pass_dominance has run on the current function.
 */
const dom_cfg_t *dom_get_cfg(void);

#endif /* ZCC_IR_DOMINANCE_H */
