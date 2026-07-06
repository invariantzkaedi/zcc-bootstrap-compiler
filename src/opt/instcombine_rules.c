// NOTE:
// - These are integer-safe rules.
// - Skip FP unless fast-math mode exists.
// - Helper API names are illustrative; adapt to your IR.

#include "zcc_ir.h"
#include "zcc_ir_opt_helpers.h"

typedef struct {
    Function *fn;
    Instr *it;
} ICtx;

static inline bool is_int_binop(const Instr *it) {
    return it->op==OP_ADD || it->op==OP_SUB || it->op==OP_MUL ||
           it->op==OP_AND || it->op==OP_OR  || it->op==OP_XOR  ||
           it->op==OP_SHL || it->op==OP_SHR || it->op==OP_ASHR;
}

/* ----------------------------
 * Helpers expected from infra:
 * ----------------------------
 * bool reg_is_const(Function*, int reg, int64_t *out);
 * int  make_const(Function*, Type ty, int64_t k);
 * Instr* def_of(Function*, int reg);
 * bool replace_all_uses(Function*, int old_reg, int new_reg);
 * bool erase_instr(Function*, Instr*);
 * bool rewrite_binop_rhs_const(Function*, Instr*, int64_t c);
 * bool rewrite_to_copy(Function*, Instr*, int src_reg);
 * bool rewrite_to_const(Function*, Instr*, int64_t k);
 * bool has_no_side_effects(const Instr*);
 * bool is_poison_sensitive(const Instr*); // if your IR models poison/UB
 * bool will_overflow_add(Type ty, int64_t a, int64_t b); // optional policy
 * bool will_overflow_mul(Type ty, int64_t a, int64_t b); // optional policy
 */

/* Rule 1: x + 0 -> x */
bool ic_rule_add_zero(ICtx *c) {
    Instr *it = c->it;
    if (it->op != OP_ADD) return false;
    int64_t k;
    if (reg_is_const(c->fn, it->src2, &k) && k == 0)
        return rewrite_to_copy(c->fn, it, it->src1);
    if (reg_is_const(c->fn, it->src1, &k) && k == 0)
        return rewrite_to_copy(c->fn, it, it->src2);
    return false;
}

/* Rule 2: x - 0 -> x */
bool ic_rule_sub_zero(ICtx *c) {
    Instr *it = c->it;
    if (it->op != OP_SUB) return false;
    int64_t k;
    if (reg_is_const(c->fn, it->src2, &k) && k == 0)
        return rewrite_to_copy(c->fn, it, it->src1);
    return false;
}

/* Rule 3: x * 1 -> x */
bool ic_rule_mul_one(ICtx *c) {
    Instr *it = c->it;
    if (it->op != OP_MUL) return false;
    int64_t k;
    if (reg_is_const(c->fn, it->src2, &k) && k == 1)
        return rewrite_to_copy(c->fn, it, it->src1);
    if (reg_is_const(c->fn, it->src1, &k) && k == 1)
        return rewrite_to_copy(c->fn, it, it->src2);
    return false;
}

/* Rule 4: x * 0 -> 0 */
bool ic_rule_mul_zero(ICtx *c) {
    Instr *it = c->it;
    if (it->op != OP_MUL) return false;
    int64_t k;
    if ((reg_is_const(c->fn, it->src1, &k) && k == 0) ||
        (reg_is_const(c->fn, it->src2, &k) && k == 0))
        return rewrite_to_const(c->fn, it, 0);
    return false;
}

/* Rule 5: x / 1 -> x (signed/unsigned safe) */
bool ic_rule_div_one(ICtx *c) {
    Instr *it = c->it;
    if (it->op != OP_SDIV && it->op != OP_UDIV) return false;
    int64_t k;
    if (reg_is_const(c->fn, it->src2, &k) && k == 1)
        return rewrite_to_copy(c->fn, it, it->src1);
    return false;
}

/* Rule 6: x & 0 -> 0 */
bool ic_rule_and_zero(ICtx *c) {
    Instr *it = c->it;
    if (it->op != OP_AND) return false;
    int64_t k;
    if ((reg_is_const(c->fn, it->src1, &k) && k == 0) ||
        (reg_is_const(c->fn, it->src2, &k) && k == 0))
        return rewrite_to_const(c->fn, it, 0);
    return false;
}

/* Rule 7: x & -1 -> x */
bool ic_rule_and_allones(ICtx *c) {
    Instr *it = c->it;
    if (it->op != OP_AND) return false;
    int64_t k;
    if (reg_is_const(c->fn, it->src2, &k) && is_all_ones_for_type(it->ty, k))
        return rewrite_to_copy(c->fn, it, it->src1);
    if (reg_is_const(c->fn, it->src1, &k) && is_all_ones_for_type(it->ty, k))
        return rewrite_to_copy(c->fn, it, it->src2);
    return false;
}

/* Rule 8: x | 0 -> x */
bool ic_rule_or_zero(ICtx *c) {
    Instr *it = c->it;
    if (it->op != OP_OR) return false;
    int64_t k;
    if (reg_is_const(c->fn, it->src2, &k) && k == 0)
        return rewrite_to_copy(c->fn, it, it->src1);
    if (reg_is_const(c->fn, it->src1, &k) && k == 0)
        return rewrite_to_copy(c->fn, it, it->src2);
    return false;
}

/* Rule 9: x ^ 0 -> x */
bool ic_rule_xor_zero(ICtx *c) {
    Instr *it = c->it;
    if (it->op != OP_XOR) return false;
    int64_t k;
    if (reg_is_const(c->fn, it->src2, &k) && k == 0)
        return rewrite_to_copy(c->fn, it, it->src1);
    if (reg_is_const(c->fn, it->src1, &k) && k == 0)
        return rewrite_to_copy(c->fn, it, it->src2);
    return false;
}

/* Rule 10: x ^ x -> 0 */
bool ic_rule_xor_self(ICtx *c) {
    Instr *it = c->it;
    if (it->op != OP_XOR) return false;
    if (it->src1 == it->src2)
        return rewrite_to_const(c->fn, it, 0);
    return false;
}

/* Rule 11: x - x -> 0 */
bool ic_rule_sub_self(ICtx *c) {
    Instr *it = c->it;
    if (it->op != OP_SUB) return false;
    if (it->src1 == it->src2)
        return rewrite_to_const(c->fn, it, 0);
    return false;
}

/* Rule 12: x << 0 / x >> 0 / x ashr 0 -> x */
bool ic_rule_shift_by_zero(ICtx *c) {
    Instr *it = c->it;
    if (it->op != OP_SHL && it->op != OP_SHR && it->op != OP_ASHR) return false;
    int64_t k;
    if (reg_is_const(c->fn, it->src2, &k) && k == 0)
        return rewrite_to_copy(c->fn, it, it->src1);
    return false;
}

/* Rule 13: (x + c1) + c2 -> x + (c1+c2) */
bool ic_rule_reassoc_add_consts(ICtx *c) {
    Instr *it = c->it;
    if (it->op != OP_ADD) return false;

    int64_t c2;
    if (!reg_is_const(c->fn, it->src2, &c2)) return false;

    Instr *d = def_of(c->fn, it->src1);
    if (!d || d->op != OP_ADD) return false;

    int64_t c1;
    if (!reg_is_const(c->fn, d->src2, &c1)) return false;

    if (will_overflow_add(it->ty, c1, c2)) return false; // policy gate

    int64_t sum = c1 + c2;
    int csum = make_const(c->fn, it->ty, sum);

    // rewrite current inst: add d->src1, csum
    it->src1 = d->src1;
    it->src2 = csum;
    return true;
}

/* Rule 14: (x * c1) * c2 -> x * (c1*c2) */
bool ic_rule_reassoc_mul_consts(ICtx *c) {
    Instr *it = c->it;
    if (it->op != OP_MUL) return false;

    int64_t c2;
    if (!reg_is_const(c->fn, it->src2, &c2)) return false;

    Instr *d = def_of(c->fn, it->src1);
    if (!d || d->op != OP_MUL) return false;

    int64_t c1;
    if (!reg_is_const(c->fn, d->src2, &c1)) return false;

    if (will_overflow_mul(it->ty, c1, c2)) return false; // policy gate

    int64_t prod = c1 * c2;
    int cprod = make_const(c->fn, it->ty, prod);

    it->src1 = d->src1;
    it->src2 = cprod;
    return true;
}

/* Rule 15: icmp eq x, x -> true ; icmp ne x, x -> false (integers only) */
bool ic_rule_icmp_self(ICtx *c) {
    Instr *it = c->it;
    if (it->op != OP_ICMP_EQ && it->op != OP_ICMP_NE) return false;
    if (it->src1 != it->src2) return false;
    return rewrite_to_const(c->fn, it, (it->op == OP_ICMP_EQ) ? 1 : 0);
}
