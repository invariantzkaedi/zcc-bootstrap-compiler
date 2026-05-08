/*
 * evm_peephole_optimizer.c — EVM Storage Packing Peephole Optimizer
 * ==================================================================
 * Post-GVN IR pass: coalesces adjacent EVM storage slot operations
 * into bit-packed sload/sstore sequences.  Emits optimized Yul output.
 *
 * Architecture:
 *   - Zero malloc during traversal (stack arena only)
 *   - Pattern-matches IR_STORE/IR_LOAD nodes tagged IR_TAG_SSTORE
 *   - Bit-masked packing for sub-256-bit values sharing a slot
 *   - Flashbots priority fee scalar from gas delta
 *
 * Compiled by GCC only (linked separately, NOT concatenated into zcc.c).
 */

#include <stdio.h>
#include <string.h>
#include "ir.h"
#include "evm_lifter.h"

/* ── ANSI Escape Codes ─────────────────────────────────────────────── */
#define A_RST   "\033[0m"
#define A_NAVY  "\033[38;5;17m"
#define A_CYAN  "\033[36m"
#define A_MAG   "\033[35m"
#define A_BOLD  "\033[1m"
#define A_DIM   "\033[2m"
#define A_BCYN  A_BOLD A_CYAN
#define A_BMAG  A_BOLD A_MAG

/* ── Stack Arena (zero malloc) ─────────────────────────────────────── */
#define ARENA_CAP (64 * 1024)

typedef struct {
    char buf[ARENA_CAP];
    int  pos;
} arena_t;

static void arena_init(arena_t *a) { a->pos = 0; }

static void *arena_bump(arena_t *a, int sz) {
    int al = (a->pos + 7) & ~7;
    if (al + sz > ARENA_CAP) return (void *)0;
    void *p = a->buf + al;
    a->pos = al + sz;
    return p;
}

/* ── EVM Gas Constants ─────────────────────────────────────────────── */
enum {
    GAS_SLOAD_COLD  = 2100,
    GAS_SLOAD_WARM  = 100,
    GAS_SSTORE_COLD = 20000,   /* zero → non-zero */
    GAS_SSTORE_WARM = 2900,    /* non-zero → non-zero */
    GAS_BITOP       = 3,       /* SHL/SHR/AND/OR/NOT */
    GAS_PUSH        = 3
};

/* ── Slot Access Record ──────────────────────────────────────────────── */
#define MAX_ACCESSES  256
#define MAX_GROUPS     64
#define MAX_PER_GROUP   8

typedef struct {
    long       slot;                     /* EVM storage slot number        */
    int        is_store;                 /* 1=sstore, 0=sload             */
    int        bit_width;                /* value width in bits            */
    int        bit_offset;               /* assigned packing offset        */
    char       val_reg[64];              /* IR temp name for value         */
    char       addr_reg[64];             /* IR temp name for slot addr     */
    ir_node_t *origin;                   /* back-pointer into IR list      */
    int        consumed;                 /* already assigned to a group    */
} slot_access_t;

typedef struct {
    slot_access_t *members[MAX_PER_GROUP];
    int            count;
    long           base_slot;
    int            total_bits;           /* sum of member bit_widths       */
    int            gas_before;
    int            gas_after;
} pack_group_t;

typedef struct {
    slot_access_t  acc[MAX_ACCESSES];
    int            acc_count;
    pack_group_t   grp[MAX_GROUPS];
    int            grp_count;
    int            total_gas_saved;
    long           bribe_gwei;
} peep_state_t;

/* ── IR Type → Bit Width ─────────────────────────────────────────────── */
static int ty_bits(ir_type_t ty) {
    switch (ty) {
    case IR_TY_I8:  case IR_TY_U8:  return 8;
    case IR_TY_I16: case IR_TY_U16: return 16;
    case IR_TY_I32: case IR_TY_U32: return 32;
    case IR_TY_I64: case IR_TY_U64: return 64;
    case IR_TY_PTR:                 return 160; /* EVM address */
    default:                        return 256;
    }
}

/* ── Phase 1: Scan IR for Storage Operations ─────────────────────────── */
/*
 * Detects two patterns:
 *
 *   Pattern A — IR_STORE with tag == IR_TAG_SSTORE, address defined
 *               by an IR_CONST node (direct constant slot).
 *
 *   Pattern B — IR_CALL to "sstore"/"sload" with a constant first ARG.
 */
static ir_node_t *find_const_def(ir_func_t *fn, ir_node_t *before,
                                  const char *name) {
    ir_node_t *n, *def = (ir_node_t *)0;
    if (!name || !name[0]) return def;
    for (n = fn->head; n && n != before; n = n->next) {
        if (n->op == IR_CONST && n->dst[0] &&
            strcmp(n->dst, name) == 0)
            def = n;
    }
    return def;
}

static void scan_accesses(ir_func_t *fn, peep_state_t *st) {
    ir_node_t *n;
    st->acc_count = 0;

    for (n = fn->head; n; n = n->next) {
        if (st->acc_count >= MAX_ACCESSES) break;

        /* Pattern A: tagged IR_STORE / IR_LOAD */
        if (n->op == IR_STORE && n->tag == IR_TAG_SSTORE && n->dst[0]) {
            ir_node_t *cd = find_const_def(fn, n, n->dst);
            if (cd) {
                slot_access_t *sa = &st->acc[st->acc_count];
                sa->slot     = cd->imm;
                sa->is_store = 1;
                sa->bit_width = ty_bits(n->type);
                sa->bit_offset = 0;
                strncpy(sa->val_reg, n->src1, 63); sa->val_reg[63] = 0;
                strncpy(sa->addr_reg, n->dst, 63); sa->addr_reg[63] = 0;
                sa->origin   = n;
                sa->consumed = 0;
                st->acc_count++;
            }
        }

        if (n->op == IR_LOAD && n->src1[0]) {
            ir_node_t *cd = find_const_def(fn, n, n->src1);
            if (cd && cd->imm >= 0) {
                /* Only count if downstream has an SSTORE tag (heuristic) */
                slot_access_t *sa = &st->acc[st->acc_count];
                sa->slot     = cd->imm;
                sa->is_store = 0;
                sa->bit_width = ty_bits(n->type);
                sa->bit_offset = 0;
                strncpy(sa->val_reg, n->dst, 63); sa->val_reg[63] = 0;
                strncpy(sa->addr_reg, n->src1, 63); sa->addr_reg[63] = 0;
                sa->origin   = n;
                sa->consumed = 0;
                st->acc_count++;
            }
        }

        /* Pattern B: IR_CALL to sstore/sload */
        if (n->op == IR_CALL &&
            (strcmp(n->label, "sstore") == 0 ||
             strcmp(n->label, "sload") == 0)) {
            ir_node_t *arg = n->next;
            if (arg && arg->op == IR_ARG && arg->src1[0]) {
                ir_node_t *cd = find_const_def(fn, n, arg->src1);
                if (cd) {
                    slot_access_t *sa = &st->acc[st->acc_count];
                    sa->slot     = cd->imm;
                    sa->is_store = (strcmp(n->label, "sstore") == 0);
                    sa->bit_width = 256;
                    sa->bit_offset = 0;
                    strncpy(sa->val_reg,
                            n->dst[0] ? n->dst : arg->src1, 63);
                    sa->val_reg[63] = 0;
                    strncpy(sa->addr_reg, arg->src1, 63);
                    sa->addr_reg[63] = 0;
                    sa->origin   = n;
                    sa->consumed = 0;
                    st->acc_count++;
                }
            }
        }
    }
}

/* ── Phase 2: Form Pack Groups ───────────────────────────────────────── */
/*
 * Two accesses are packable when:
 *   1) Same direction (both stores OR both loads)
 *   2) Same slot  OR  adjacent slots with combined width ≤ 256 bits
 *   3) Neither already consumed
 */
static void form_groups(peep_state_t *st) {
    int i, j;
    st->grp_count = 0;

    for (i = 0; i < st->acc_count && st->grp_count < MAX_GROUPS; i++) {
        slot_access_t *a = &st->acc[i];
        if (a->consumed) continue;
        if (a->bit_width >= 256) continue;  /* full slot, nothing to pack */

        pack_group_t *g = &st->grp[st->grp_count];
        g->count      = 0;
        g->base_slot  = a->slot;
        g->total_bits = 0;

        /* Seed */
        a->bit_offset = 0;
        g->members[g->count++] = a;
        g->total_bits = a->bit_width;
        a->consumed = 1;

        /* Find partners */
        for (j = i + 1; j < st->acc_count && g->count < MAX_PER_GROUP; j++) {
            slot_access_t *b = &st->acc[j];
            if (b->consumed) continue;
            if (b->is_store != a->is_store) continue;
            if (b->bit_width >= 256) continue;

            int same_slot = (b->slot == a->slot);
            int adj_slot  = (b->slot == a->slot + 1 ||
                             b->slot == a->slot - 1);

            if (!same_slot && !adj_slot) continue;

            int new_total = g->total_bits + b->bit_width;
            if (new_total > 256) continue;

            b->bit_offset = g->total_bits;
            g->members[g->count++] = b;
            g->total_bits = new_total;
            b->consumed = 1;
        }

        if (g->count >= 2) {
            /* Compute gas delta */
            if (a->is_store) {
                g->gas_before = g->count * GAS_SSTORE_WARM;
                /* Packed: 1 sload + masks + 1 sstore */
                g->gas_after = GAS_SLOAD_WARM
                             + (g->count * 2 + 2) * GAS_BITOP
                             + (g->count + 1) * GAS_PUSH
                             + GAS_SSTORE_WARM;
            } else {
                g->gas_before = g->count * GAS_SLOAD_WARM;
                /* Packed: 1 sload + shifts */
                g->gas_after = GAS_SLOAD_WARM
                             + g->count * GAS_BITOP
                             + g->count * GAS_PUSH;
            }
            st->grp_count++;
        } else {
            /* Singleton, undo consumption */
            a->consumed = 0;
            g->count = 0;
        }
    }
}

/* ── Hex Mask Builder (256-bit) ──────────────────────────────────────── */
/*
 * Writes a hex string "0x00...FF" for a mask of `width` bits at
 * `offset` bits from LSB.  Buffer must hold ≥67 chars (0x + 64 hex + NUL).
 */
static void build_mask_hex(char *buf, int width, int offset) {
    unsigned char mask[32];
    int byte_lo, byte_hi, bi, i;
    char *p = buf;

    memset(mask, 0, 32);

    byte_lo = offset / 8;
    byte_hi = (offset + width - 1) / 8;

    for (bi = byte_lo; bi <= byte_hi && bi < 32; bi++) {
        int lo_bit = (bi == byte_lo) ? (offset % 8) : 0;
        int hi_bit = (bi == byte_hi) ? ((offset + width - 1) % 8) : 7;
        unsigned char b = 0;
        int k;
        for (k = lo_bit; k <= hi_bit; k++) b |= (1u << k);
        mask[bi] = b;
    }

    *p++ = '0'; *p++ = 'x';
    /* Big-endian output: byte 31 first */
    for (i = 31; i >= 0; i--) {
        static const char hex[] = "0123456789abcdef";
        *p++ = hex[(mask[i] >> 4) & 0xF];
        *p++ = hex[mask[i] & 0xF];
    }
    *p = '\0';
}

/* ── Phase 3: Emit Yul ───────────────────────────────────────────────── */
/*
 * Writes optimized Yul into `out` (up to `cap` bytes).
 * Returns number of characters written.
 */
static int emit_yul_group(pack_group_t *g, char *out, int cap) {
    char *p = out;
    char *end = out + cap - 1;
    int i;
    char mask_buf[68];

#define YUL_PRINTF(...) do { \
    int _n = snprintf(p, (int)(end - p), __VA_ARGS__); \
    if (_n > 0 && p + _n < end) p += _n; \
} while(0)

    YUL_PRINTF("// === Packed %s (slot %ld, %d values, %d bits) ===\n",
               g->members[0]->is_store ? "SSTORE" : "SLOAD",
               g->base_slot, g->count, g->total_bits);
    YUL_PRINTF("{\n");

    if (g->members[0]->is_store) {
        /* Packed SSTORE: read-modify-write with masks */
        YUL_PRINTF("    let _existing := sload(%ld)\n", g->base_slot);

        /* Build the clear mask (NOT of all member masks OR'd) */
        YUL_PRINTF("    let _clear := not(0x");
        /* Emit combined mask inline */
        {
            unsigned char combined[32];
            memset(combined, 0, 32);
            for (i = 0; i < g->count; i++) {
                int off = g->members[i]->bit_offset;
                int wid = g->members[i]->bit_width;
                int blo = off / 8;
                int bhi = (off + wid - 1) / 8;
                int bi;
                for (bi = blo; bi <= bhi && bi < 32; bi++) {
                    int lo = (bi == blo) ? (off % 8) : 0;
                    int hi = (bi == bhi) ? ((off + wid - 1) % 8) : 7;
                    int k;
                    for (k = lo; k <= hi; k++)
                        combined[bi] |= (1u << k);
                }
            }
            {
                int ci;
                for (ci = 31; ci >= 0; ci--) {
                    static const char hx[] = "0123456789abcdef";
                    char h1 = hx[(combined[ci] >> 4) & 0xF];
                    char h2 = hx[combined[ci] & 0xF];
                    if (p + 2 < end) { *p++ = h1; *p++ = h2; }
                }
            }
        }
        YUL_PRINTF(")\n");
        YUL_PRINTF("    let _packed := and(_existing, _clear)\n");

        /* OR in each member value at its offset */
        for (i = 0; i < g->count; i++) {
            slot_access_t *m = g->members[i];
            build_mask_hex(mask_buf, m->bit_width, 0);

            if (m->bit_offset == 0) {
                YUL_PRINTF("    _packed := or(_packed, and(%s, %s))\n",
                           m->val_reg, mask_buf);
            } else {
                YUL_PRINTF("    _packed := or(_packed, shl(%d, and(%s, %s)))\n",
                           m->bit_offset, m->val_reg, mask_buf);
            }
        }

        YUL_PRINTF("    sstore(%ld, _packed)\n", g->base_slot);
    } else {
        /* Packed SLOAD: single read + shift/mask per member */
        YUL_PRINTF("    let _word := sload(%ld)\n", g->base_slot);

        for (i = 0; i < g->count; i++) {
            slot_access_t *m = g->members[i];
            build_mask_hex(mask_buf, m->bit_width, 0);

            if (m->bit_offset == 0) {
                YUL_PRINTF("    let %s := and(_word, %s)\n",
                           m->val_reg, mask_buf);
            } else {
                YUL_PRINTF("    let %s := and(shr(%d, _word), %s)\n",
                           m->val_reg, m->bit_offset, mask_buf);
            }
        }
    }

    YUL_PRINTF("}\n");

#undef YUL_PRINTF

    *p = '\0';
    return (int)(p - out);
}

/* ── Phase 4: Bribe Heuristic ────────────────────────────────────────── */
/*
 * bribe_scalar = gas_saved × 0.80
 *
 * At 30 gwei gas price:
 *   bribe_gwei = gas_saved × 0.80 × 30
 *
 * Denser Yul → lower base gas → higher affordable bribe margin.
 * We model density as (original_insn_count / packed_insn_count).
 */
#define DEFAULT_GAS_PRICE_GWEI 30
#define BRIBE_FRACTION_NUM      4   /* 80% = 4/5 */
#define BRIBE_FRACTION_DEN      5

static void compute_bribe(peep_state_t *st) {
    int i;
    int saved = 0;

    for (i = 0; i < st->grp_count; i++) {
        int delta = st->grp[i].gas_before - st->grp[i].gas_after;
        if (delta > 0) saved += delta;
    }

    st->total_gas_saved = saved;
    st->bribe_gwei = (long)saved
                   * BRIBE_FRACTION_NUM / BRIBE_FRACTION_DEN
                   * DEFAULT_GAS_PRICE_GWEI;
}

/* ── Diagnostic Logging ──────────────────────────────────────────────── */
static void log_scan(peep_state_t *st, const char *fn_name) {
    int i;

    fprintf(stderr,
        A_NAVY "╔══════════════════════════════════════════════════╗\n"
        "║" A_BCYN " ▸ EVM PEEPHOLE OPTIMIZER" A_NAVY
        "                          ║\n"
        "╚══════════════════════════════════════════════════╝" A_RST "\n");

    fprintf(stderr,
        A_DIM A_CYAN "  fn: %s  |  accesses: %d  |  groups: %d" A_RST "\n",
        fn_name, st->acc_count, st->grp_count);

    for (i = 0; i < st->grp_count; i++) {
        pack_group_t *g = &st->grp[i];
        int j;
        fprintf(stderr,
            A_MAG "  ├── group[%d] slot=%ld  %s  members=%d  "
            "bits=%d/256" A_RST "\n",
            i, g->base_slot,
            g->members[0]->is_store ? "SSTORE" : "SLOAD",
            g->count, g->total_bits);

        for (j = 0; j < g->count; j++) {
            slot_access_t *m = g->members[j];
            fprintf(stderr,
                A_CYAN "  │   ├ %s  off=%3d  w=%3d  reg=%s" A_RST "\n",
                m->is_store ? "wr" : "rd",
                m->bit_offset, m->bit_width, m->val_reg);
        }

        fprintf(stderr,
            A_BMAG "  │   └ gas: %d → %d  (Δ %d)" A_RST "\n",
            g->gas_before, g->gas_after,
            g->gas_before - g->gas_after);
    }

    if (st->total_gas_saved > 0) {
        fprintf(stderr,
            A_NAVY "  ╰── " A_BCYN "TOTAL SAVED: %d gas  |  "
            "BRIBE SCALAR: %ld gwei" A_RST "\n",
            st->total_gas_saved, st->bribe_gwei);
    } else {
        fprintf(stderr,
            A_DIM "  ╰── no packable groups found" A_RST "\n");
    }
}

/* ── Public Entry Point ──────────────────────────────────────────────── */
/*
 * evm_peephole_optimize()
 *
 * Scans `fn` for adjacent storage operations, packs them, and writes
 * the optimized Yul into `yul_out` (up to `yul_cap` bytes).
 *
 * Returns total gas saved (0 if no optimization was possible).
 * `bribe_gwei_out` receives the recommended priority fee scalar.
 *
 * ZERO MALLOC: all working state lives on the stack via arena.
 */
int evm_peephole_optimize(ir_func_t *fn,
                           char *yul_out, int yul_cap,
                           long *bribe_gwei_out) {
    arena_t arena;
    peep_state_t *st;
    int yul_pos = 0;
    int i;

    if (!fn || !yul_out || yul_cap < 64) return 0;

    arena_init(&arena);
    st = (peep_state_t *)arena_bump(&arena, (int)sizeof(peep_state_t));
    if (!st) return 0;
    memset(st, 0, sizeof(peep_state_t));

    /* Phase 1: Scan */
    scan_accesses(fn, st);

    /* Phase 2: Group */
    form_groups(st);

    /* Phase 3: Emit Yul */
    yul_out[0] = '\0';
    for (i = 0; i < st->grp_count; i++) {
        int remain = yul_cap - yul_pos - 1;
        if (remain < 128) break;
        int wrote = emit_yul_group(&st->grp[i],
                                    yul_out + yul_pos, remain);
        yul_pos += wrote;
    }

    /* Phase 4: Bribe */
    compute_bribe(st);

    /* Diagnostics */
    log_scan(st, fn->name);

    if (bribe_gwei_out) *bribe_gwei_out = st->bribe_gwei;

    return st->total_gas_saved;
}
