/*
 * zcc_loop_builder.c  —  loop/phi emission substrate for zcc_onnx_lower.c
 *
 * PARTS placement (after regalloc.c, before zcc_onnx_lower.c):
 *   ... ir.c ir_to_x86.c regalloc.c zcc_loop_builder.c zcc_onnx_lower.c
 *       ir_telemetry_stub.c
 *
 * C89 constraints (ZCC self-host parser):
 *   - All declarations at top of block
 *   - No stdint.h — use int / unsigned long
 *   - No designated initializers
 *   - No compound literals
 *   - No __attribute__
 *
 * Gate ladder this file participates in:
 *
 *   CG-IR-005  multiple PHIs copied per block
 *   CG-IR-006  parallel PHI move correctness (swap recurrence)
 *   CG-IR-007  critical-edge PHI loss (ReLU join, if/else merge)
 *   CG-IR-008  float PHI register-class preservation
 *
 * Each gate has a dedicated fuzz kernel below.  They must pass in order.
 * Do not begin MatMul lowering until all four gates are green.
 */

/* =========================================================================
 * ZccLoopI64
 *
 * v2: stores preheader directly — no predecessor-order assumption.
 *
 * Change from v1:
 *   REMOVED: preheader = ir_block_predecessor(fn, loop->header, 0)
 *            (fragile: depends on predecessor list order)
 *   ADDED:   loop.preheader captured at construction time from
 *            ir_current_block(fn) before any new blocks are created.
 *
 * Block structure emitted by zcc_ir_loop_i64:
 *
 *   preheader (caller's current block at call time):
 *     br loop.header
 *
 *   loop.header:
 *     %phi_idx = phi [start, preheader] [%idx_next, loop.body]  <- FIRST phi
 *     %cond    = icmp slt %phi_idx, end
 *     br cond, loop.body, loop.exit
 *
 *   loop.body:                    <- insertion point returned to caller
 *     ... caller fills ...
 *     %idx_next = add %phi_idx, 1     |
 *     br loop.header                  |  emitted by zcc_ir_loop_end()
 *
 *   loop.exit:                    <- insertion point after zcc_ir_loop_end()
 *     ...
 *
 * PHI order invariant:
 *   The integer phi MUST be the first phi instruction in loop.header.
 *   zcc_ir_f32_reduction_phi() appends float phis after it.
 *   This ordering matters for CG-IR-005: the current (buggy) phi edge copy
 *   emitter processes only the first phi — so the loop terminates correctly
 *   while the float accumulator stays at its initial value (0.0), producing
 *   a hard binary failure signal rather than subtle numerical drift.
 * ========================================================================= */
typedef struct {
    IrValue   phi_idx;    /* induction variable in body */
    IrValue   idx_next;   /* phi_idx + step; set by zcc_ir_loop_end() */
    IrBlock  *preheader;  /* block that branches into header (stored, not derived) */
    IrBlock  *header;     /* loop.header — back-edge target, holds all phis */
    IrBlock  *body;       /* MUTABLE loop tail — updated by zcc_ir_loop_set_tail()
                           * when the body contains control flow (if/else, etc.)
                           * and the actual termination point is not the original
                           * loop.body block.  zcc_ir_loop_end() reads this field
                           * to place the increment and back-edge branch. */
    IrBlock  *exit;       /* loop.exit   — post-loop continuation */
} ZccLoopI64;

static ZccLoopI64 zcc_ir_loop_i64(IrFunction *fn,
                                   IrValue start,
                                   IrValue end,
                                   IrValue step)
{
    ZccLoopI64 loop;
    IrBlock *preheader;
    IrBlock *header;
    IrBlock *body;
    IrBlock *exit_block;
    IrValue cond;

    /* Capture preheader BEFORE creating new blocks — no predecessor lookup */
    preheader  = ir_current_block(fn);
    header     = ir_new_block(fn);
    body       = ir_new_block(fn);
    exit_block = ir_new_block(fn);

    /* preheader → header */
    ir_emit_br(fn, preheader, header);

    /* --- loop.header --- */
    ir_set_insert_block(fn, header);

    /*
     * Integer induction phi — MUST be first phi in this block.
     * Back-edge operand (from body) patched in zcc_ir_loop_end().
     */
    loop.phi_idx = ir_emit_phi_i64(fn, header);
    ir_phi_add_incoming(fn, loop.phi_idx, start, preheader);

    cond = ir_emit_icmp_slt_i64(fn, loop.phi_idx, end);
    ir_emit_cond_br(fn, cond, body, exit_block);

    /* --- loop.body --- (caller's insertion point) */
    ir_set_insert_block(fn, body);

    loop.preheader = preheader;
    loop.header    = header;
    loop.body      = body;
    loop.exit      = exit_block;
    loop.idx_next  = IR_VALUE_INVALID;

    return loop;
}

/*
 * zcc_ir_loop_end — close the loop, emit increment and back-edge.
 *
 * INVARIANT: loop.body must be unterminated at this call.
 *   Violation means the caller emitted a br/ret inside the body before
 *   calling loop_end — the increment and back-edge would follow a
 *   terminator, producing malformed IR.
 *
 * Call order:
 *   1. zcc_ir_reduction_phi_close() for every float reduction phi
 *   2. zcc_ir_loop_end()
 *   (idx_next is emitted here, after all reduction updates)
 */
static void zcc_ir_loop_end(IrFunction *fn, ZccLoopI64 *loop, IrValue step)
{
    IrValue idx_next;

    /* Hard invariant — catch terminator-before-close bugs at IR build time */
    assert(!ir_block_has_terminator(loop->body) &&
           "zcc_ir_loop_end: body already terminated — "
           "call zcc_ir_reduction_phi_close() before loop_end, "
           "and do not emit br/ret inside the body");

    idx_next       = ir_emit_add_i64(fn, loop->phi_idx, step);
    loop->idx_next = idx_next;

    /* Patch integer phi back-edge: incoming from body = idx_next */
    ir_phi_add_incoming(fn, loop->phi_idx, idx_next, loop->body);

    /* Back-edge branch */
    ir_emit_br(fn, loop->body, loop->header);

    /* Move insertion point to exit block */
    ir_set_insert_block(fn, loop->exit);
}

/*
 * zcc_ir_loop_set_tail — redirect the loop's back-edge source block.
 *
 * Required when the loop body contains control flow (if/else, early
 * continue, nested structure) such that execution reaches a block other
 * than the original loop.body before looping back.
 *
 * The canonical case is Gate 3 (guarded_sum): the body branches into
 * then_block / else_block / merge_block.  The unterminated block at loop
 * end is merge_block, not loop.body.  Without this call, zcc_ir_loop_end()
 * would assert-fail on loop.body (already terminated by the cond_br) or
 * emit the increment into the wrong block.
 *
 * Protocol:
 *   1. After the last body instruction, identify the current unterminated
 *      block (ir_current_block(fn) or the known merge/continuation block).
 *   2. Call zcc_ir_loop_set_tail(&loop, that_block).
 *   3. Call zcc_ir_reduction_phi_close() — it uses loop->body for the
 *      incoming edge, so the tail must be set first.
 *   4. Call zcc_ir_loop_end().
 *
 * Also updates zcc_ir_f32_reduction_phi back-edge source when
 * zcc_ir_reduction_phi_close() is called after this.
 *
 * Invariant: tail must be unterminated.  The assert in zcc_ir_loop_end()
 * checks this on loop->body, which is now the tail block.
 */
static void zcc_ir_loop_set_tail(ZccLoopI64 *loop, IrBlock *tail)
{
    loop->body = tail;
}

/* =========================================================================
 * ZccFloatReductionPhi
 *
 * Emits a float phi in loop.header, appended after the integer phi.
 *
 * Uses loop->preheader directly — no predecessor-order lookup.
 *
 * CG-IR-005 hazard: this phi is the second (or later) phi in the header.
 * The current ir_asm_emit_phi_edge_copy processes only the first phi,
 * so this phi's back-edge value is never moved — accumulator stays at
 * its initial value (0.0) for the entire loop.
 *
 * That failure mode is the gate: dot_product_scalar must return 0.0
 * before the fix and 30.0 after it.
 * ========================================================================= */
typedef struct {
    IrValue phi;   /* phi instruction in loop.header */
    IrValue next;  /* updated value — set by zcc_ir_reduction_phi_close() */
} ZccFloatReductionPhi;

static ZccFloatReductionPhi zcc_ir_f32_reduction_phi(IrFunction *fn,
                                                      ZccLoopI64 *loop,
                                                      IrValue init)
{
    ZccFloatReductionPhi rp;
    IrBlock *saved;

    saved  = ir_current_block(fn);

    /* Temporarily switch to header to append the float phi after the int phi */
    ir_set_insert_block(fn, loop->header);
    rp.phi  = ir_emit_phi_f32(fn, loop->header);
    /* Use stored preheader — not derived from predecessor list */
    ir_phi_add_incoming(fn, rp.phi, init, loop->preheader);
    rp.next = IR_VALUE_INVALID;

    /* Restore insertion point to body */
    ir_set_insert_block(fn, saved);
    return rp;
}

static void zcc_ir_reduction_phi_close(IrFunction *fn,
                                        ZccFloatReductionPhi *rp,
                                        IrValue new_val,
                                        ZccLoopI64 *loop)
{
    rp->next = new_val;
    ir_phi_add_incoming(fn, rp->phi, new_val, loop->body);
}

/* =========================================================================
 * GATE KERNEL 1 — CG-IR-005: multiple PHIs copied per block
 *
 * dot_product_scalar(float *a, float *b, int K) -> float
 *   acc = 0.0
 *   for k in 0..K:
 *     acc += a[k] * b[k]
 *   return acc
 *
 * Header block PHI layout:
 *   phi_k   (integer) <- FIRST: loop terminates correctly even with bug
 *   phi_acc (float)   <- SECOND: stays 0.0 with CG-IR-005, updates after fix
 *
 * Gate:
 *   BEFORE fix: result == 0.0 for all inputs
 *   AFTER  fix: |result - reference| < 1e-5
 *   K=4, a=b=[1,2,3,4]: expected 30.0
 * ========================================================================= */
static IrValue zcc_ir_emit_dot_product(IrFunction *fn,
                                        IrValue v_a,
                                        IrValue v_b,
                                        IrValue v_K)
{
    IrValue zero_i64;
    IrValue one_i64;
    IrValue zero_f32;
    IrValue a_k;
    IrValue b_k;
    IrValue prod;
    IrValue new_acc;
    ZccLoopI64 loop;
    ZccFloatReductionPhi acc;

    zero_i64 = ir_const_i64(fn, 0);
    one_i64  = ir_const_i64(fn, 1);
    zero_f32 = ir_const_f32(fn, 0.0f);

    loop = zcc_ir_loop_i64(fn, zero_i64, v_K, one_i64);
    acc  = zcc_ir_f32_reduction_phi(fn, &loop, zero_f32);

    a_k     = ir_load_f32_indexed(fn, v_a, loop.phi_idx);
    b_k     = ir_load_f32_indexed(fn, v_b, loop.phi_idx);
    prod    = ir_emit_fmul_f32(fn, a_k, b_k);
    new_acc = ir_emit_fadd_f32(fn, acc.phi, prod);

    zcc_ir_reduction_phi_close(fn, &acc, new_acc, &loop);
    zcc_ir_loop_end(fn, &loop, one_i64);

    return acc.phi;
}

/* =========================================================================
 * GATE KERNEL 2 — CG-IR-006: parallel PHI move correctness
 *
 * swap_recurrence(int N) -> int
 *   x = 1, y = 2
 *   for i in 0..N:
 *     x, y = y, x      <- mutual dependency: both PHIs swap simultaneously
 *   return x + y        <- always 3 for any N, even or odd
 *
 * SSA header PHI layout (three PHIs total — zcc_ir_loop_i64 emits phi_k
 * first, then x_phi and y_phi are appended by this kernel):
 *
 *   loop.header:
 *     k     = phi [0,     preheader] [k+1,  body]   <- phi 1 (loop index)
 *     x_phi = phi [1,     preheader] [y_phi, body]  <- phi 2 \  cycle
 *     y_phi = phi [2,     preheader] [x_phi, body]  <- phi 3 /  (2 ↔ 3)
 *
 * Back-edge copy set emitted by ir_asm_emit_phi_edge_copy (body -> header):
 *
 *   k     <- k_next        (acyclic: safe to emit in any order)
 *   x     <- y             \
 *   y     <- x             /  cycle: these two are mutually dependent
 *
 * The cycle is confined to copies 2 and 3.  Copy 1 (k <- k_next) is
 * acyclic and must be resolved independently of the swap cycle.
 *
 * Naive sequential lowering of the cycle:
 *   mov x, y        <- x = old(y) ✓
 *   mov y, x        <- y = new(x) = old(y)  ✗  old(x) is lost
 *
 * Correct lowering (parallel move via scratch register):
 *   mov tmp, x      <- save old(x)
 *   mov x, y        <- x = old(y) ✓
 *   mov y, tmp      <- y = old(x) ✓
 *
 * Scratch register: r10 or r11 (caller-saved, ABI-safe, not live across edge).
 *
 * Gate:
 *   BEFORE fix: x+y != 3 for odd N  (one value lost to sequential clobber)
 *   AFTER  fix: x+y == 3 for all N  (parallel move preserves both values)
 *
 * Note: this kernel does NOT use zcc_ir_f32_reduction_phi — it tests the
 * parallel move resolver directly via two integer PHIs with a back-edge cycle.
 * ========================================================================= */
static IrValue zcc_ir_emit_swap_recurrence(IrFunction *fn, IrValue v_N)
{
    IrValue zero_i;
    IrValue one_i;
    IrValue const1;
    IrValue const2;
    IrValue x_phi;
    IrValue y_phi;
    IrValue x_sum;
    ZccLoopI64 loop;
    IrBlock *preheader;
    IrBlock *saved;

    zero_i = ir_const_i64(fn, 0);
    one_i  = ir_const_i64(fn, 1);
    const1 = ir_const_i64(fn, 1);
    const2 = ir_const_i64(fn, 2);

    loop = zcc_ir_loop_i64(fn, zero_i, v_N, one_i);
    preheader = loop.preheader;

    /*
     * x_phi and y_phi both live in loop.header.
     * Their back-edges form a cycle: x <- y, y <- x.
     * This is the exact topology that breaks naive sequential phi destruction.
     *
     * We emit both phis before any body instructions.
     * Back-edge operands are patched after the body: x_phi <- y_phi,
     * y_phi <- x_phi.  Since both refer to the phi VALUES (not to body
     * temporaries), the parallel move resolver must handle the cycle.
     */
    saved = ir_current_block(fn);
    ir_set_insert_block(fn, loop.header);

    /* x_phi: initial value 1 from preheader */
    x_phi = ir_emit_phi_i64(fn, loop.header);
    ir_phi_add_incoming(fn, x_phi, const1, preheader);

    /* y_phi: initial value 2 from preheader */
    y_phi = ir_emit_phi_i64(fn, loop.header);
    ir_phi_add_incoming(fn, y_phi, const2, preheader);

    ir_set_insert_block(fn, saved);

    /*
     * Body is intentionally empty — the swap is entirely in the phi back-edges.
     * x_next = old(y_phi), y_next = old(x_phi).
     */
    ir_phi_add_incoming(fn, x_phi, y_phi, loop.body);
    ir_phi_add_incoming(fn, y_phi, x_phi, loop.body);

    zcc_ir_loop_end(fn, &loop, one_i);

    /* x + y — always 3 regardless of N */
    x_sum = ir_emit_add_i64(fn, x_phi, y_phi);
    return x_sum;
}

/* =========================================================================
 * GATE KERNEL 3 — CG-IR-007: critical-edge PHI loss
 *
 * guarded_increment(int *arr, int N, float threshold) -> float
 *   sum = 0.0
 *   for i in 0..N:
 *     v = (float)arr[i]
 *     if v > threshold:
 *       sum += v
 *     else:
 *       sum += 0.0   <- no-op, but creates the if/else join PHI
 *   return sum
 *
 * CFG shape per iteration:
 *
 *   loop.body
 *      |
 *      +-- [v > thresh] --> then_block --> merge
 *      |                                    ^
 *      +-- [else]       --> else_block -----+
 *                                           |
 *                                     phi(v_if_true, 0.0_if_false)
 *                                           |
 *                                        sum += phi_val
 *
 * The merge block has a PHI fed from then_block and else_block.
 * then_block has two successors conceptually (but only one: merge).
 * The critical-edge case arises if loop.body directly branches to merge
 * via a block that has multiple successors — phi copy placement becomes
 * ambiguous without critical-edge splitting.
 *
 * Gate:
 *   BEFORE fix: PHI in merge receives wrong value on one branch
 *   AFTER  fix: result matches reference (numpy conditional sum)
 *   Test input: arr=[1,5,2,8,3], threshold=4.0 -> expected 13.0 (5+8)
 * ========================================================================= */
static IrValue zcc_ir_emit_guarded_sum(IrFunction *fn,
                                        IrValue v_arr,
                                        IrValue v_N,
                                        IrValue v_thresh)
{
    IrValue zero_i64;
    IrValue one_i64;
    IrValue zero_f32;
    IrValue vi_int;
    IrValue vi_f32;
    IrValue cond;
    IrValue phi_val;
    IrValue new_sum;
    IrBlock *then_block;
    IrBlock *else_block;
    IrBlock *merge_block;
    ZccLoopI64 loop;
    ZccFloatReductionPhi sum_phi;

    zero_i64 = ir_const_i64(fn, 0);
    one_i64  = ir_const_i64(fn, 1);
    zero_f32 = ir_const_f32(fn, 0.0f);

    loop    = zcc_ir_loop_i64(fn, zero_i64, v_N, one_i64);
    sum_phi = zcc_ir_f32_reduction_phi(fn, &loop, zero_f32);

    /* body: load arr[i], cast to f32 */
    vi_int = ir_load_i32_indexed(fn, v_arr, loop.phi_idx);
    vi_f32 = ir_emit_sitofp_f32(fn, vi_int);

    /* branch: vi_f32 > threshold */
    cond       = ir_emit_fcmp_gt_f32(fn, vi_f32, v_thresh);
    then_block = ir_new_block(fn);
    else_block = ir_new_block(fn);
    merge_block = ir_new_block(fn);
    ir_emit_cond_br(fn, cond, then_block, else_block);

    /* then: phi_val = vi_f32 */
    ir_set_insert_block(fn, then_block);
    ir_emit_br(fn, then_block, merge_block);

    /* else: phi_val = 0.0 */
    ir_set_insert_block(fn, else_block);
    ir_emit_br(fn, else_block, merge_block);

    /* merge: phi_val = phi [vi_f32, then] [0.0, else] */
    ir_set_insert_block(fn, merge_block);
    phi_val = ir_emit_phi_f32(fn, merge_block);
    ir_phi_add_incoming(fn, phi_val, vi_f32,   then_block);
    ir_phi_add_incoming(fn, phi_val, zero_f32, else_block);

    new_sum = ir_emit_fadd_f32(fn, sum_phi.phi, phi_val);

    /*
     * The current insertion block is merge_block — this is the actual loop
     * tail, not the original loop.body (which was terminated by the cond_br
     * that branched into then_block / else_block).
     *
     * Update loop.body to merge_block so that:
     *   - zcc_ir_reduction_phi_close() patches the back-edge incoming
     *     from merge_block (not the original, already-terminated body)
     *   - zcc_ir_loop_end() asserts on merge_block (unterminated ✓) and
     *     emits the increment + back-edge branch there
     *
     * Without this, loop_end would assert on the original loop.body, which
     * already has a terminator (the cond_br to then/else).
     */
    zcc_ir_loop_set_tail(&loop, merge_block);
    zcc_ir_reduction_phi_close(fn, &sum_phi, new_sum, &loop);
    zcc_ir_loop_end(fn, &loop, one_i64);

    return sum_phi.phi;
}

/* =========================================================================
 * GATE KERNEL 4 — CG-IR-008: float PHI register-class preservation
 *
 * float_accumulate_cast(int *arr, int N) -> float
 *   acc_int = 0
 *   acc_f32 = 0.0
 *   for i in 0..N:
 *     acc_int += arr[i]          <- integer reduction phi -> integer regs
 *     acc_f32 += (float)arr[i]  <- float   reduction phi -> XMM regs
 *   return acc_f32 - (float)acc_int   <- should be 0.0 for exact int vals
 *
 * This kernel has both an integer reduction phi and a float reduction phi
 * in the same loop header.  The phi destruction pass must assign them to
 * different register classes: GPR for the integer phi, XMM for the float.
 *
 * Failure mode (CG-IR-008):
 *   Phi destruction emits: mov eax, xmm0  (int move for float phi)
 *   Result: acc_f32 corrupted, return value != 0.0
 *
 * Gate:
 *   |result| < 1e-5 for all integer input arrays
 *   Test input: arr=[1,2,3,4,5], N=5 -> expected 0.0
 * ========================================================================= */
static IrValue zcc_ir_emit_float_accumulate_cast(IrFunction *fn,
                                                   IrValue v_arr,
                                                   IrValue v_N)
{
    IrValue zero_i64;
    IrValue one_i64;
    IrValue zero_f32;
    IrValue zero_i32;
    IrValue vi_int;
    IrValue vi_f32;
    IrValue new_acc_int;
    IrValue new_acc_f32;
    IrValue acc_f32_final;
    IrValue acc_int_as_f32;
    IrValue result;
    ZccLoopI64 loop;
    ZccFloatReductionPhi acc_f32_phi;
    /* integer reduction phi — manual, mirrors ZccFloatReductionPhi pattern */
    IrValue acc_int_phi;
    IrBlock *saved;

    zero_i64 = ir_const_i64(fn, 0);
    one_i64  = ir_const_i64(fn, 1);
    zero_f32 = ir_const_f32(fn, 0.0f);
    zero_i32 = ir_const_i32(fn, 0);

    loop         = zcc_ir_loop_i64(fn, zero_i64, v_N, one_i64);
    acc_f32_phi  = zcc_ir_f32_reduction_phi(fn, &loop, zero_f32);

    /* emit integer reduction phi in header (after float phi — third phi total) */
    saved = ir_current_block(fn);
    ir_set_insert_block(fn, loop.header);
    acc_int_phi = ir_emit_phi_i32(fn, loop.header);
    ir_phi_add_incoming(fn, acc_int_phi, zero_i32, loop.preheader);
    ir_set_insert_block(fn, saved);

    /* body */
    vi_int      = ir_load_i32_indexed(fn, v_arr, loop.phi_idx);
    vi_f32      = ir_emit_sitofp_f32(fn, vi_int);

    new_acc_f32 = ir_emit_fadd_f32(fn, acc_f32_phi.phi, vi_f32);
    new_acc_int = ir_emit_add_i32(fn, acc_int_phi, vi_int);

    /* patch integer phi back-edge */
    ir_phi_add_incoming(fn, acc_int_phi, new_acc_int, loop.body);

    zcc_ir_reduction_phi_close(fn, &acc_f32_phi, new_acc_f32, &loop);
    zcc_ir_loop_end(fn, &loop, one_i64);

    /* return acc_f32 - (float)acc_int  — should be 0.0 */
    acc_f32_final  = acc_f32_phi.phi;
    acc_int_as_f32 = ir_emit_sitofp_f32(fn, acc_int_phi);
    result         = ir_emit_fsub_f32(fn, acc_f32_final, acc_int_as_f32);
    return result;
}

/* =========================================================================
 * CG-IR-005 fix target
 *
 * compiler_passes.c ~line 4570  ir_asm_emit_phi_edge_copy
 *
 * BEFORE (broken — only first PHI):
 *   IrInstr *phi = first_phi(to);
 *   if (!phi) return;
 *   IrValue incoming = phi_incoming_from(phi, from);
 *   emit_mov(ctx, incoming, phi->result);
 *
 * AFTER (fixed — all PHIs):
 *   IrInstr *instr;
 *   for (instr = block_first_instr(to);
 *        instr && instr->op == OP_PHI;
 *        instr = instr_next(instr)) {
 *       IrValue incoming = phi_incoming_from(instr, from);
 *       if (incoming == IR_VALUE_INVALID) continue;
 *       emit_mov(ctx, incoming, instr->result);
 *   }
 *
 * Bootstrap gate: zcc2.s == zcc3.s must hold after this change.
 * Fuzz gate:      dot_product_scalar K=4 a=b=[1,2,3,4] must return 30.0.
 * =========================================================================
 *
 * CG-IR-006 fix target
 *
 * compiler_passes.c — phi parallel move resolver
 *
 * The loop above emits copies sequentially.  For a cycle like:
 *   x <- y
 *   y <- x
 * sequential emission corrupts one value.  Required fix:
 *
 * STEP 1: collect all (dst, src) pairs from the phi loop above
 * STEP 2: detect cycles via visited[] / in_cycle[] scan
 * STEP 3: for each cycle, emit:
 *     mov tmp_reg, cycle_entry_src
 *     ... remaining cycle copies ...
 *     mov cycle_entry_dst, tmp_reg
 * STEP 4: emit acyclic copies in reverse topological order
 *
 * Scratch register: use a caller-saved register not live across the edge
 * (r10 or r11 are safe in the LP64 ABI for this purpose — they are
 * explicitly clobbered by the ABI and not preserved by the callee).
 *
 * Fuzz gate: swap_recurrence(N) must return 3 for all N >= 0.
 * =========================================================================
 *
 * CG-IR-007 fix target
 *
 * Critical-edge splitting.  Any edge A->B where A has >1 successor and B
 * has >1 predecessor must be split by inserting an empty block:
 *
 *   A -> split_AB -> B
 *
 * PHI copies for the A->B edge are placed in split_AB.
 * This ensures phi copies never land in a block with multiple successors
 * (which would cause the copy to execute on paths that don't take this edge).
 *
 * Fuzz gate: guarded_sum([1,5,2,8,3], threshold=4.0) must return 13.0.
 * =========================================================================
 *
 * CG-IR-008 fix target
 *
 * ir_asm_emit_phi_edge_copy must select move instruction by register class:
 *   IR_F32 phi -> movss xmm_dst, xmm_src   (NOT mov rax, xmm — invalid)
 *   IR_I64 phi -> movq  gpr_dst, gpr_src
 *   IR_I32 phi -> movl  gpr_dst, gpr_src
 *
 * The existing emit_mov() helper must dispatch on the phi's type field:
 *   if (phi->type == IR_F32) emit_movss(ctx, src, dst);
 *   else                     emit_movq(ctx, src, dst);
 *
 * Fuzz gate: float_accumulate_cast([1..N]) must return 0.0 (±1e-5).
 * ========================================================================= */
