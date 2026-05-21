
/* ══════════════════════════════════════════════════════════════════
 * STRENGTH REDUCTION PATCH for part4.c — codegen_expr()
 * ══════════════════════════════════════════════════════════════════
 *
 * Insert these checks BEFORE the default imulq/idivq emission
 * inside each relevant case block.
 *
 * Prerequisites:
 *   - Node has node->lhs, node->rhs
 *   - node->rhs->kind == ND_NUM for constant RHS
 *   - Utility function is_power_of_2() defined (see below)
 * ══════════════════════════════════════════════════════════════════
 */

/* Add this utility function near the top of part4.c (after includes): */

static int is_power_of_2_val(long long val) {
    return val > 0 && (val & (val - 1)) == 0;
}

static int log2_of(long long val) {
    int n;
    n = 0;
    while (val > 1) {
        val = val >> 1;
        n = n + 1;
    }
    return n;
}

/* ── Inside case ND_MUL: (before the default imulq) ── */

/*
    // STRENGTH REDUCTION: x * 2^n → x << n
    if (node->rhs->kind == ND_NUM && is_power_of_2_val(node->rhs->val)) {
        int shift;
        codegen_expr(cc, node->lhs);
        shift = log2_of(node->rhs->val);
        fprintf(cc->out, "    shlq $%d, %%rax\n", shift);
        return;
    }
    // Commutative: 2^n * x → x << n
    if (node->lhs->kind == ND_NUM && is_power_of_2_val(node->lhs->val)) {
        int shift;
        codegen_expr(cc, node->rhs);
        shift = log2_of(node->lhs->val);
        fprintf(cc->out, "    shlq $%d, %%rax\n", shift);
        return;
    }
    // STRENGTH REDUCTION: x * 3 → lea (%rax,%rax,2), %rax
    if (node->rhs->kind == ND_NUM && node->rhs->val == 3) {
        codegen_expr(cc, node->lhs);
        fprintf(cc->out, "    leaq (%%rax,%%rax,2), %%rax\n");
        return;
    }
    // x * 5 → lea (%rax,%rax,4), %rax
    if (node->rhs->kind == ND_NUM && node->rhs->val == 5) {
        codegen_expr(cc, node->lhs);
        fprintf(cc->out, "    leaq (%%rax,%%rax,4), %%rax\n");
        return;
    }
    // x * 9 → lea (%rax,%rax,8), %rax
    if (node->rhs->kind == ND_NUM && node->rhs->val == 9) {
        codegen_expr(cc, node->lhs);
        fprintf(cc->out, "    leaq (%%rax,%%rax,8), %%rax\n");
        return;
    }
*/

/* ── Inside case ND_DIV: (before the default idivq/cqo) ── */

/*
    // STRENGTH REDUCTION: x / 2^n → x >> n
    // NOTE: For signed division, SAR rounds toward -infinity
    // while C rounds toward zero. For non-negative values (like
    // Q16.16 accumulators), this is equivalent. For general signed
    // division, you need: (x + ((x >> 63) & ((1<<n)-1))) >> n
    if (node->rhs->kind == ND_NUM && is_power_of_2_val(node->rhs->val)) {
        int shift;
        codegen_expr(cc, node->lhs);
        shift = log2_of(node->rhs->val);
        if (node->ty && node->ty->is_unsigned) {
            fprintf(cc->out, "    shrq $%d, %%rax\n", shift);
        } else {
            // Signed: correct rounding toward zero
            // rax = (rax + ((rax >> 63) & ((1 << shift) - 1))) >> shift
            fprintf(cc->out, "    movq %%rax, %%rcx\n");
            fprintf(cc->out, "    sarq $63, %%rcx\n");
            fprintf(cc->out, "    andq $%lld, %%rcx\n", (1LL << shift) - 1);
            fprintf(cc->out, "    addq %%rcx, %%rax\n");
            fprintf(cc->out, "    sarq $%d, %%rax\n", shift);
        }
        return;
    }
*/

/* ── Inside case ND_MOD: (before the default idivq) ── */

/*
    // STRENGTH REDUCTION: x % 2^n → x & (2^n - 1)  (unsigned only)
    if (node->rhs->kind == ND_NUM && is_power_of_2_val(node->rhs->val)) {
        long long mask;
        codegen_expr(cc, node->lhs);
        mask = node->rhs->val - 1;
        if (node->ty && node->ty->is_unsigned) {
            fprintf(cc->out, "    andq $%lld, %%rax\n", mask);
        } else {
            // Signed modulo is more complex — fall through to idiv
            goto default_mod;
        }
        return;
    }
    default_mod:
*/

/* ══════════════════════════════════════════════════════════════════
 * CYCLE COUNT SAVINGS (per operation):
 *
 *   imulq (64-bit):  ~3 cycles
 *   shlq:            ~1 cycle     → 3x speedup on MUL
 *   leaq (scale):    ~1 cycle     → 3x speedup on small MUL
 *
 *   idivq (64-bit):  ~35-90 cycles (data-dependent!)
 *   sarq:            ~1 cycle     → 35-90x speedup on DIV
 *   shrq:            ~1 cycle     → 35-90x speedup on unsigned DIV
 *
 *   divq (modulo):   ~35-90 cycles
 *   andq:            ~1 cycle     → 35-90x speedup on MOD
 *
 * For the Q16.16 inference engine:
 *   3 layers × (128+64+19) MACs × idivq savings = ~211 × ~40 cycles
 *   = ~8,440 cycles saved per inference call from DIV alone
 *
 * Combined with MUL strength reduction in the MAC inner loop:
 *   Total estimated savings: ~15,000-20,000 cycles per inference
 * ══════════════════════════════════════════════════════════════════
 */

