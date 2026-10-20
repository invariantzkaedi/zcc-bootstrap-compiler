/*
 * avx512_caxpy_peephole.c — Option-α Complex AXPY (CAXPY4) for ZCC ir_to_x86.c
 *
 * Matches the ZCC IR shape for 4-lane complex multiply-accumulate:
 *     y[k] += A[k] * x0    (k = 0..3 complex doubles = 8 doubles total)
 *
 * Real C representation:
 *   For k = 0..3:
 *     y[2*k]   = y[2*k]   + (A[2*k] * xr   - A[2*k+1] * xi);
 *     y[2*k+1] = y[2*k+1] + (A[2*k+1] * xr + A[2*k] * xi);
 *
 * Emits 9-instruction AVX-512 F64 sequence:
 *   vbroadcastsd xr, %zmm0
 *   vbroadcastsd xi, %zmm3
 *   vmovupd (A), %zmm1
 *   vmovupd (y), %zmm2
 *   vfmadd231pd %zmm1, %zmm0, %zmm2      # y + A * xr
 *   vpermilpd $0x55, %zmm1, %zmm1        # swap real/imag of A
 *   vfmaddsub231pd %zmm1, %zmm3, %zmm2   # even: - ai*xi, odd: + ar*xi
 *   vmovupd %zmm2, (y)
 *   vzeroupper
 *
 * Gate: ZCC_AVX512_CAXPY=1|true|yes|on. Default OFF.
 */
#ifndef ZCC_AVX512_CAXPY_PEEPHOLE_H
#define ZCC_AVX512_CAXPY_PEEPHOLE_H

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef ZCC_CAXPY_WINDOW
#define ZCC_CAXPY_WINDOW 512
#endif
#ifndef ZCC_CAXPY_LANES
#define ZCC_CAXPY_LANES 8
#endif

static int zcc_avx512_caxpy_debug(void)
{
    static int cached = -1;
    const char *e;
    if (cached >= 0)
        return cached;
    e = getenv("ZCC_AVX512_CAXPY_DEBUG");
    cached = (e && e[0] && e[0] != '0');
    return cached;
}

static int zcc_avx512_caxpy_gate(void)
{
    static int cached = -1;
    const char *e;
    char buf[16];
    size_t i;
    if (cached >= 0)
        return cached;
    e = getenv("ZCC_AVX512_CAXPY");
    if (!e || !e[0]) {
        cached = 0;
        return 0;
    }
    for (i = 0; i < sizeof(buf) - 1 && e[i]; i++)
        buf[i] = (char)tolower((unsigned char)e[i]);
    buf[i] = 0;
    cached = (strcmp(buf, "1") == 0 ||
              strcmp(buf, "true") == 0 ||
              strcmp(buf, "yes") == 0 ||
              strcmp(buf, "on") == 0);
    return cached;
}

static const ir_node_t *zcc_caxpy_find_def(const ir_func_t *fn,
                                           const ir_node_t *before,
                                           const char *name)
{
    const ir_node_t *n;
    const ir_node_t *hit = NULL;
    if (!fn || !name || !name[0])
        return NULL;
    for (n = fn->head; n && n != before; n = n->next) {
        if (n->dst[0] && strcmp(n->dst, name) == 0)
            hit = n;
    }
    return hit;
}

static int zcc_caxpy_get_imm(const ir_func_t *fn, const ir_node_t *before,
                             const char *name, long *out_val)
{
    const ir_node_t *d;
    if (!name || !name[0])
        return 0;
    if (isdigit((unsigned char)name[0]) ||
        (name[0] == '-' && isdigit((unsigned char)name[1])) ||
        (name[0] == '+' && isdigit((unsigned char)name[1]))) {
        *out_val = atol(name);
        return 1;
    }
    d = zcc_caxpy_find_def(fn, before, name);
    if (!d)
        return 0;
    if (d->op == IR_CONST) {
        *out_val = d->imm;
        return 1;
    }
    if (d->op == IR_MUL) {
        long v1 = 0, v2 = 0;
        if (zcc_caxpy_get_imm(fn, d, d->src1, &v1) &&
            zcc_caxpy_get_imm(fn, d, d->src2, &v2)) {
            *out_val = v1 * v2;
            return 1;
        }
        return 0;
    }
    if (d->op == IR_SHL) {
        long v1 = 0, v2 = 0;
        if (zcc_caxpy_get_imm(fn, d, d->src1, &v1) &&
            zcc_caxpy_get_imm(fn, d, d->src2, &v2)) {
            *out_val = v1 << v2;
            return 1;
        }
        return 0;
    }
    if (d->op == IR_COPY || d->op == IR_CAST)
        return zcc_caxpy_get_imm(fn, d, d->src1, out_val);
    return 0;
}

typedef struct {
    char origin[IR_NAME_MAX];
    long off;
    int  ok;
} zcc_caxpy_prov_t;

static zcc_caxpy_prov_t zcc_caxpy_prov(const ir_func_t *fn,
                                       const ir_node_t *before,
                                       const char *name)
{
    zcc_caxpy_prov_t p;
    const ir_node_t *d;
    memset(&p, 0, sizeof(p));
    if (!name || !name[0])
        return p;

    d = zcc_caxpy_find_def(fn, before, name);
    if (!d) {
        strncpy(p.origin, name, IR_NAME_MAX - 1);
        p.ok = 1;
        return p;
    }

    if (d->op == IR_ADD || d->op == IR_SUB) {
        const char *base = NULL;
        long sign = (d->op == IR_SUB) ? -1L : 1L;
        long imm = 0;

        if (d->src2[0] == '\0') {
            base = d->src1;
            imm = d->imm;
        } else if (zcc_caxpy_get_imm(fn, d, d->src2, &imm)) {
            base = d->src1;
        } else if (zcc_caxpy_get_imm(fn, d, d->src1, &imm)) {
            base = d->src2;
        } else {
            return p;
        }
        {
            zcc_caxpy_prov_t inner = zcc_caxpy_prov(fn, d, base);
            if (!inner.ok)
                return p;
            strncpy(p.origin, inner.origin, IR_NAME_MAX - 1);
            p.off = inner.off + sign * imm;
            p.ok = 1;
            return p;
        }
    }

    if (d->op == IR_COPY || d->op == IR_CAST)
        return zcc_caxpy_prov(fn, d, d->src1);

    if (d->op == IR_ADDR) {
        strncpy(p.origin, d->src1, IR_NAME_MAX - 1);
        p.ok = 1;
        return p;
    }

    if (d->op == IR_LOAD) {
        zcc_caxpy_prov_t home = zcc_caxpy_prov(fn, d, d->src1);
        if (!home.ok)
            return p;
        strncpy(p.origin, home.origin, IR_NAME_MAX - 1);
        p.off = home.off;
        p.ok = 1;
        return p;
    }

    strncpy(p.origin, name, IR_NAME_MAX - 1);
    p.ok = 1;
    return p;
}

static int zcc_caxpy_prov_eq(const zcc_caxpy_prov_t *a, const zcc_caxpy_prov_t *b)
{
    return a && b && a->ok && b->ok &&
           a->off == b->off &&
           strcmp(a->origin, b->origin) == 0;
}

static int zcc_caxpy_is_f64_load(const ir_node_t *n)
{
    return n && n->op == IR_LOAD &&
           (n->type == IR_TY_F64 || n->type == IR_TY_I64 ||
            n->type == IR_TY_U64);
}

static int zcc_caxpy_is_f64_store(const ir_node_t *n)
{
    return n && n->op == IR_STORE &&
           (n->type == IR_TY_F64 || n->type == IR_TY_I64 ||
            n->type == IR_TY_U64 || n->type == IR_TY_VOID);
}

typedef struct {
    ir_node_t *store;
    ir_node_t *load_y;
    ir_node_t *fadd_outer;
    ir_node_t *inner_op;
    ir_node_t *mul_r;
    ir_node_t *mul_i;
    ir_node_t *load_ar;
    ir_node_t *load_ai;
    ir_node_t *load_xr;
    ir_node_t *load_xi;
    zcc_caxpy_prov_t y;
    zcc_caxpy_prov_t ar;
    zcc_caxpy_prov_t ai;
    zcc_caxpy_prov_t xr;
    zcc_caxpy_prov_t xi;
} zcc_caxpy_lane_t;

/* Matches a single lane (0..7) */
static int zcc_caxpy_match_lane(const ir_func_t *fn, ir_node_t *st,
                               zcc_caxpy_lane_t *lane, int target_lane)
{
    ir_node_t *fadd_out, *ly = NULL, *term = NULL;
    ir_node_t *m_r = NULL, *m_i = NULL;
    ir_node_t *op1, *op2;
    zcc_caxpy_prov_t yp, yp_load;
    int is_even = (target_lane % 2 == 0);

    memset(lane, 0, sizeof(*lane));
    if (!zcc_caxpy_is_f64_store(st))
        return 0;

    yp = zcc_caxpy_prov(fn, st, st->dst);
    if (!yp.ok || yp.off != (long)target_lane * 8)
        return 0;

    fadd_out = (ir_node_t *)zcc_caxpy_find_def(fn, st, st->src1);
    if (!fadd_out || fadd_out->op != IR_FADD)
        return 0;

    /* One operand of fadd_out is ly (load y[target_lane]), other is term */
    op1 = (ir_node_t *)zcc_caxpy_find_def(fn, fadd_out, fadd_out->src1);
    op2 = (ir_node_t *)zcc_caxpy_find_def(fn, fadd_out, fadd_out->src2);

    if (op1 && zcc_caxpy_is_f64_load(op1)) {
        yp_load = zcc_caxpy_prov(fn, op1, op1->src1);
        if (zcc_caxpy_prov_eq(&yp_load, &yp)) {
            ly = op1;
            term = op2;
        }
    }
    if (!ly && op2 && zcc_caxpy_is_f64_load(op2)) {
        yp_load = zcc_caxpy_prov(fn, op2, op2->src1);
        if (zcc_caxpy_prov_eq(&yp_load, &yp)) {
            ly = op2;
            term = op1;
        }
    }
    if (!ly || !term)
        return 0;

    if (is_even) {
        /* Even lane: y[2k] += (A[2k] * xr - A[2k+1] * xi) -> term must be IR_FSUB */
        if (term->op != IR_FSUB)
            return 0;
        m_r = (ir_node_t *)zcc_caxpy_find_def(fn, term, term->src1);
        m_i = (ir_node_t *)zcc_caxpy_find_def(fn, term, term->src2);
    } else {
        /* Odd lane: y[2k+1] += (A[2k+1] * xr + A[2k] * xi) -> term is IR_FADD */
        ir_node_t *t1, *t2;
        if (term->op != IR_FADD)
            return 0;
        t1 = (ir_node_t *)zcc_caxpy_find_def(fn, term, term->src1);
        t2 = (ir_node_t *)zcc_caxpy_find_def(fn, term, term->src2);
        if (!t1 || !t2 || t1->op != IR_FMUL || t2->op != IR_FMUL)
            return 0;
        /* Distinguish m_r and m_i by checking operand offsets later */
        m_r = t1;
        m_i = t2;
    }

    if (!m_r || !m_i || m_r->op != IR_FMUL || m_i->op != IR_FMUL)
        return 0;

    /* Extract components of m_r and m_i */
    {
        ir_node_t *r0 = (ir_node_t *)zcc_caxpy_find_def(fn, m_r, m_r->src1);
        ir_node_t *r1 = (ir_node_t *)zcc_caxpy_find_def(fn, m_r, m_r->src2);
        ir_node_t *i0 = (ir_node_t *)zcc_caxpy_find_def(fn, m_i, m_i->src1);
        ir_node_t *i1 = (ir_node_t *)zcc_caxpy_find_def(fn, m_i, m_i->src2);
        zcc_caxpy_prov_t pr0, pr1, pi0, pi1;

        if (!r0 || !r1 || !i0 || !i1)
            return 0;
        if (!zcc_caxpy_is_f64_load(r0) || !zcc_caxpy_is_f64_load(r1) ||
            !zcc_caxpy_is_f64_load(i0) || !zcc_caxpy_is_f64_load(i1))
            return 0;

        pr0 = zcc_caxpy_prov(fn, r0, r0->src1);
        pr1 = zcc_caxpy_prov(fn, r1, r1->src1);
        pi0 = zcc_caxpy_prov(fn, i0, i0->src1);
        pi1 = zcc_caxpy_prov(fn, i1, i1->src2 ? i1->src1 : i1->src1);

        /* In m_r: one operand is A[target_lane] (offset target_lane*8), other is xr */
        long want_ar_off = (long)target_lane * 8;
        long want_ai_off = (long)(target_lane ^ 1) * 8;

        /* For odd lane, t1 and t2 might be swapped */
        if (!is_even) {
            /* Check if m_r actually holds ai*xi instead */
            if ((pr0.off == want_ai_off || pr1.off == want_ai_off) &&
                (pi0.off == want_ar_off || pi1.off == want_ar_off)) {
                ir_node_t *tmp_n = m_r; m_r = m_i; m_i = tmp_n;
                ir_node_t *tmp_r0 = r0; r0 = i0; i0 = tmp_r0;
                ir_node_t *tmp_r1 = r1; r1 = i1; i1 = tmp_r1;
                zcc_caxpy_prov_t tmp_p0 = pr0; pr0 = pi0; pi0 = tmp_p0;
                zcc_caxpy_prov_t tmp_p1 = pr1; pr1 = pi1; pi1 = tmp_p1;
            }
        }

        /* Now in m_r: find which is A_r (offset want_ar_off) and which is xr */
        if (pr0.off == want_ar_off) {
            lane->load_ar = r0; lane->ar = pr0;
            lane->load_xr = r1; lane->xr = pr1;
        } else if (pr1.off == want_ar_off) {
            lane->load_ar = r1; lane->ar = pr1;
            lane->load_xr = r0; lane->xr = pr0;
        } else {
            return 0;
        }

        /* In m_i: find which is A_i (offset want_ai_off) and which is xi */
        if (pi0.off == want_ai_off) {
            lane->load_ai = i0; lane->ai = pi0;
            lane->load_xi = i1; lane->xi = pi1;
        } else if (pi1.off == want_ai_off) {
            lane->load_ai = i1; lane->ai = pi1;
            lane->load_xi = i0; lane->xi = pi0;
        } else {
            return 0;
        }
    }

    lane->store      = st;
    lane->load_y     = ly;
    lane->fadd_outer = fadd_out;
    lane->inner_op   = term;
    lane->mul_r      = m_r;
    lane->mul_i      = m_i;
    lane->y          = yp;
    return 1;
}

static int zcc_caxpy_side_effect(ir_op_t op)
{
    switch (op) {
    case IR_STORE:
    case IR_CALL:
    case IR_ASM:
    case IR_RET:
    case IR_BR:
    case IR_BR_IF:
        return 1;
    default:
        return 0;
    }
}

static int zcc_try_emit_avx512_caxpy4(FILE *out,
                                      ir_func_t *fn,
                                      ir_node_t *start,
                                      const RegAllocator *ra)
{
    ir_node_t *n;
    ir_node_t *window[ZCC_CAXPY_WINDOW];
    int nw = 0, i, got = 0;
    zcc_caxpy_lane_t lanes[ZCC_CAXPY_LANES];
    int have[ZCC_CAXPY_LANES];
    ir_node_t *last = NULL;
    const ir_node_t *earliest_l0 = NULL;
    int consumed;
    zcc_caxpy_prov_t a_id, y_id, xr_id, xi_id;

    (void)ra;
    memset(&a_id, 0, sizeof(a_id));
    memset(&y_id, 0, sizeof(y_id));
    memset(&xr_id, 0, sizeof(xr_id));
    memset(&xi_id, 0, sizeof(xi_id));

    if (!out || !fn || !start || !zcc_avx512_caxpy_gate())
        return 0;

    memset(have, 0, sizeof(have));
    memset(lanes, 0, sizeof(lanes));

    for (n = start; n && nw < ZCC_CAXPY_WINDOW; n = n->next) {
        if (n != start && zcc_caxpy_side_effect(n->op) && n->op != IR_STORE)
            break;
        window[nw++] = n;
    }

    /* Scan window for stores to lanes 0..7 */
    for (i = 0; i < nw; i++) {
        zcc_caxpy_lane_t lane;
        int idx;
        if (!zcc_caxpy_is_f64_store(window[i]))
            continue;

        /* Test all lane indices 0..7 */
        for (idx = 0; idx < ZCC_CAXPY_LANES; idx++) {
            if (have[idx])
                continue;
            if (zcc_caxpy_match_lane(fn, window[i], &lane, idx)) {
                if (got == 0) {
                    a_id  = lane.ar;
                    y_id  = lane.y;
                    xr_id = lane.xr;
                    xi_id = lane.xi;
                } else {
                    if (strcmp(lane.ar.origin, a_id.origin) != 0)
                        continue;
                    if (strcmp(lane.y.origin, y_id.origin) != 0)
                        continue;
                    if (strcmp(lane.xr.origin, xr_id.origin) != 0 ||
                        lane.xr.off != xr_id.off)
                        continue;
                    if (strcmp(lane.xi.origin, xi_id.origin) != 0 ||
                        lane.xi.off != xi_id.off)
                        continue;
                }
                lanes[idx] = lane;
                have[idx] = 1;
                last = window[i];
                got++;
                break;
            }
        }
    }

    if (got != ZCC_CAXPY_LANES) {
        if (zcc_avx512_caxpy_debug() && fn->name && strstr(fn->name, "caxpy4"))
            fprintf(stderr,
                    "[avx512-caxpy] miss got=%d/%d start_op=%d\n",
                    got, ZCC_CAXPY_LANES, (int)start->op);
        return 0;
    }

    /* Verify all 8 lanes are present and contiguous */
    for (i = 0; i < ZCC_CAXPY_LANES; i++) {
        if (!have[i] || lanes[i].y.off != (long)i * 8)
            return 0;
    }

    /* Find earliest instruction belonging to lane 0 */
    earliest_l0 = lanes[0].store;
    for (n = fn->head; n; n = n->next) {
        if (n == lanes[0].store || n == lanes[0].fadd_outer ||
            n == lanes[0].inner_op || n == lanes[0].mul_r ||
            n == lanes[0].mul_i || n == lanes[0].load_y ||
            n == lanes[0].load_ar || n == lanes[0].load_ai ||
            n == lanes[0].load_xr || n == lanes[0].load_xi) {
            earliest_l0 = n;
            break;
        }
        if (lanes[0].load_ar && strcmp(n->dst, lanes[0].load_ar->src1) == 0) {
            earliest_l0 = n;
            break;
        }
        if (lanes[0].load_y && strcmp(n->dst, lanes[0].load_y->src1) == 0) {
            earliest_l0 = n;
            break;
        }
    }

    /* Start matching exactly at earliest node of lane 0 */
    if (start != earliest_l0)
        return 0;

    fprintf(out, "    /* ZCC_AVX512_CAXPY caxpy4  y += A * x0  (4 x complex f64) */\n");

    /* 1. Base pointers for A and y */
    load_address(out, lanes[0].ar.origin, "%rax");
    fprintf(out, "    movq (%%rax), %%rax\n");
    load_address(out, lanes[0].y.origin, "%rcx");
    fprintf(out, "    movq (%%rcx), %%rcx\n");

    /* 2. Broadcast xr into %zmm0 and xi into %zmm3 */
    if (strcmp(lanes[0].xr.origin, lanes[0].xi.origin) == 0 &&
        lanes[0].xr.off == 0 && lanes[0].xi.off == 8) {
        /* xr and xi are contiguous in array x[0..1] */
        load_address(out, lanes[0].xr.origin, "%r11");
        fprintf(out, "    movq (%%r11), %%r11\n");
        fprintf(out, "    vbroadcastsd (%%r11), %%zmm0\n");
        fprintf(out, "    vbroadcastsd 8(%%r11), %%zmm3\n");
    } else {
        /* xr and xi are separate local scalar slots */
        load_address(out, lanes[0].xr.origin, "%r11");
        if (lanes[0].xr.off != 0)
            fprintf(out, "    addq $%ld, %%r11\n", lanes[0].xr.off);
        fprintf(out, "    vbroadcastsd (%%r11), %%zmm0\n");

        load_address(out, lanes[0].xi.origin, "%r11");
        if (lanes[0].xi.off != 0)
            fprintf(out, "    addq $%ld, %%r11\n", lanes[0].xi.off);
        fprintf(out, "    vbroadcastsd (%%r11), %%zmm3\n");
    }

    /* 3. Load vectors A and y */
    fprintf(out, "    vmovupd (%%rax), %%zmm1\n");
    fprintf(out, "    vmovupd (%%rcx), %%zmm2\n");

    /* 4. Complex fused multiply-accumulate */
    fprintf(out, "    vfmadd231pd %%zmm1, %%zmm0, %%zmm2\n");
    fprintf(out, "    vpermilpd $0x55, %%zmm1, %%zmm1\n");
    fprintf(out, "    vfmaddsub231pd %%zmm1, %%zmm3, %%zmm2\n");

    /* 5. Store vector result and clear upper state */
    fprintf(out, "    vmovupd %%zmm2, (%%rcx)\n");
    fprintf(out, "    vzeroupper\n");

    consumed = 0;
    for (n = start; n; n = n->next) {
        consumed++;
        if (n == last)
            break;
    }
    if (zcc_avx512_caxpy_debug())
        fprintf(stderr,
                "[avx512-caxpy] hit consumed=%d A=%s y=%s xr=%s xi=%s\n",
                consumed, lanes[0].ar.origin, lanes[0].y.origin,
                lanes[0].xr.origin, lanes[0].xi.origin);
    return consumed;
}

#endif /* ZCC_AVX512_CAXPY_PEEPHOLE_H */
