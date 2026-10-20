/*
 * avx512_qw_peephole.c — Option α for ZCC ir_to_x86.c
 *
 * Matches the REAL ZCC IR shape for
 *     y[i] = y[i] + A[i] * x0;    i = 0..7
 *
 * Real IR (probe qw_step_8wide_unroll):
 *   Lane 0:
 *     load  ptr %t8   %stack_-24
 *     load  f64 %t12  %t8
 *     load  ptr %t14  %stack_-8
 *     load  f64 %t18  %t14
 *     load  f64 %t20  %stack_-32
 *     fmul  f64 %t21  %t18 %t20
 *     fadd  f64 %t22  %t12 %t21
 *     load  ptr %t24  %stack_-24
 *     store f64 %t24  %t22
 *   Lane 1:
 *     load  ptr %t29  %stack_-24
 *     add   ptr %t32  %t29 8
 *     ...
 *   Lanes 2..7:
 *     load  ptr %t50  %stack_-24
 *     const i32 %t51  imm=2
 *     mul   i32 %t52  %t51 8
 *     add   ptr %t53  %t50 %t52
 *     ...
 *
 * Gate: ZCC_AVX512_QW=1|true|yes|on. Default OFF.
 */
#ifndef ZCC_AVX512_QW_PEEPHOLE_H
#define ZCC_AVX512_QW_PEEPHOLE_H

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef ZCC_QW_WINDOW
#define ZCC_QW_WINDOW 256
#endif
#ifndef ZCC_QW_LANES
#define ZCC_QW_LANES 8
#endif

static int zcc_avx512_qw_debug(void)
{
    static int cached = -1;
    const char *e;
    if (cached >= 0)
        return cached;
    e = getenv("ZCC_AVX512_QW_DEBUG");
    cached = (e && e[0] && e[0] != '0');
    return cached;
}

static int zcc_avx512_qw_gate(void)
{
    static int cached = -1;
    const char *e;
    char buf[16];
    size_t i;
    if (cached >= 0)
        return cached;
    e = getenv("ZCC_AVX512_QW");
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

static const ir_node_t *zcc_qw_find_def(const ir_func_t *fn,
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

static int zcc_qw_get_imm(const ir_func_t *fn, const ir_node_t *before,
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
    d = zcc_qw_find_def(fn, before, name);
    if (!d)
        return 0;
    if (d->op == IR_CONST) {
        *out_val = d->imm;
        return 1;
    }
    if (d->op == IR_MUL) {
        long v1 = 0, v2 = 0;
        if (zcc_qw_get_imm(fn, d, d->src1, &v1) &&
            zcc_qw_get_imm(fn, d, d->src2, &v2)) {
            *out_val = v1 * v2;
            return 1;
        }
        return 0;
    }
    if (d->op == IR_SHL) {
        long v1 = 0, v2 = 0;
        if (zcc_qw_get_imm(fn, d, d->src1, &v1) &&
            zcc_qw_get_imm(fn, d, d->src2, &v2)) {
            *out_val = v1 << v2;
            return 1;
        }
        return 0;
    }
    if (d->op == IR_COPY || d->op == IR_CAST)
        return zcc_qw_get_imm(fn, d, d->src1, out_val);
    return 0;
}

typedef struct {
    char origin[IR_NAME_MAX];
    long off;
    int ok;
} zcc_qw_prov_t;

static zcc_qw_prov_t zcc_qw_prov(const ir_func_t *fn,
                                 const ir_node_t *before,
                                 const char *name)
{
    zcc_qw_prov_t p;
    const ir_node_t *d;
    memset(&p, 0, sizeof(p));
    if (!name || !name[0])
        return p;

    d = zcc_qw_find_def(fn, before, name);
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
        } else if (zcc_qw_get_imm(fn, d, d->src2, &imm)) {
            base = d->src1;
        } else if (zcc_qw_get_imm(fn, d, d->src1, &imm)) {
            base = d->src2;
        } else {
            return p;
        }
        {
            zcc_qw_prov_t inner = zcc_qw_prov(fn, d, base);
            if (!inner.ok)
                return p;
            strncpy(p.origin, inner.origin, IR_NAME_MAX - 1);
            p.off = inner.off + sign * imm;
            p.ok = 1;
            return p;
        }
    }

    if (d->op == IR_COPY || d->op == IR_CAST)
        return zcc_qw_prov(fn, d, d->src1);

    if (d->op == IR_ADDR) {
        strncpy(p.origin, d->src1, IR_NAME_MAX - 1);
        p.ok = 1;
        return p;
    }

    if (d->op == IR_LOAD) {
        zcc_qw_prov_t home = zcc_qw_prov(fn, d, d->src1);
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

static int zcc_qw_prov_eq(const zcc_qw_prov_t *a, const zcc_qw_prov_t *b)
{
    return a && b && a->ok && b->ok &&
           a->off == b->off &&
           strcmp(a->origin, b->origin) == 0;
}

static int zcc_qw_is_f64_load(const ir_node_t *n)
{
    return n && n->op == IR_LOAD &&
           (n->type == IR_TY_F64 || n->type == IR_TY_I64 ||
            n->type == IR_TY_U64);
}

static int zcc_qw_is_f64_store(const ir_node_t *n)
{
    return n && n->op == IR_STORE &&
           (n->type == IR_TY_F64 || n->type == IR_TY_I64 ||
            n->type == IR_TY_U64 || n->type == IR_TY_VOID);
}

typedef struct {
    ir_node_t *store;
    ir_node_t *load_y;
    ir_node_t *load_a;
    ir_node_t *load_x0;
    ir_node_t *fmul;
    ir_node_t *fadd;
    zcc_qw_prov_t y;
    zcc_qw_prov_t a;
    zcc_qw_prov_t x0;
} zcc_qw_lane_t;

static int zcc_qw_match_lane(const ir_func_t *fn, ir_node_t *st,
                             zcc_qw_lane_t *lane,
                             const zcc_qw_prov_t *known_x0)
{
    ir_node_t *addv, *lhs, *rhs, *mul = NULL, *ly = NULL;
    ir_node_t *m0, *m1;
    zcc_qw_prov_t yp, a0p, a1p, yp_load;

    memset(lane, 0, sizeof(*lane));
    if (!zcc_qw_is_f64_store(st))
        return 0;

    yp = zcc_qw_prov(fn, st, st->dst);
    if (!yp.ok)
        return 0;
    if (yp.off % 8 != 0 || yp.off < 0 || yp.off > 56)
        return 0;

    addv = (ir_node_t *)zcc_qw_find_def(fn, st, st->src1);
    if (!addv)
        return 0;

    if (addv->op == IR_FMA) {
        mul = addv;
        ly  = (ir_node_t *)zcc_qw_find_def(fn, addv, addv->label);
        m0  = (ir_node_t *)zcc_qw_find_def(fn, addv, addv->src1);
        m1  = (ir_node_t *)zcc_qw_find_def(fn, addv, addv->src2);
    } else if (addv->op == IR_FADD) {
        lhs = (ir_node_t *)zcc_qw_find_def(fn, addv, addv->src1);
        rhs = (ir_node_t *)zcc_qw_find_def(fn, addv, addv->src2);
        if (lhs && lhs->op == IR_FMUL) {
            mul = lhs;
            ly = rhs;
        } else if (rhs && rhs->op == IR_FMUL) {
            mul = rhs;
            ly = lhs;
        } else {
            return 0;
        }
        m0 = (ir_node_t *)zcc_qw_find_def(fn, mul, mul->src1);
        m1 = (ir_node_t *)zcc_qw_find_def(fn, mul, mul->src2);
    } else {
        return 0;
    }

    if (!zcc_qw_is_f64_load(ly) || !zcc_qw_is_f64_load(m0) ||
        !zcc_qw_is_f64_load(m1))
        return 0;

    yp_load = zcc_qw_prov(fn, ly, ly->src1);
    if (!zcc_qw_prov_eq(&yp_load, &yp))
        return 0;

    a0p = zcc_qw_prov(fn, m0, m0->src1);
    a1p = zcc_qw_prov(fn, m1, m1->src1);
    if (!a0p.ok || !a1p.ok)
        return 0;

    /* A[i] has the same byte offset as y[i]. x0 does not. */
    if (a0p.off == yp.off && a1p.off != yp.off) {
        lane->load_a  = m0;
        lane->load_x0 = m1;
        lane->a       = a0p;
        lane->x0      = a1p;
    } else if (a1p.off == yp.off && a0p.off != yp.off) {
        lane->load_a  = m1;
        lane->load_x0 = m0;
        lane->a       = a1p;
        lane->x0      = a0p;
    } else if (yp.off == 0 && a0p.off == 0 && a1p.off == 0 &&
               strcmp(a0p.origin, a1p.origin) != 0) {
        /* Lane 0: both A[0] and x0 have offset 0.
         * If known_x0 is available, use origin match.
         * Otherwise m0 is A and m1 is x0 (matches standard C evaluation order). */
        if (known_x0 && known_x0->ok && strcmp(a0p.origin, known_x0->origin) == 0) {
            lane->load_a  = m1;
            lane->load_x0 = m0;
            lane->a       = a1p;
            lane->x0      = a0p;
        } else {
            lane->load_a  = m0;
            lane->load_x0 = m1;
            lane->a       = a0p;
            lane->x0      = a1p;
        }
    } else {
        return 0;
    }

    if (lane->x0.off != 0)
        return 0;

    lane->store  = st;
    lane->load_y = ly;
    lane->fmul   = mul;
    lane->fadd   = addv;
    lane->y      = yp;
    return 1;
}

static int zcc_qw_side_effect(ir_op_t op)
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

static int zcc_try_emit_avx512_qw_axpy8(FILE *out,
                                        ir_func_t *fn,
                                        ir_node_t *start,
                                        const RegAllocator *ra)
{
    ir_node_t *n;
    ir_node_t *window[ZCC_QW_WINDOW];
    int nw = 0, i, got = 0;
    zcc_qw_lane_t lanes[ZCC_QW_LANES];
    int have[ZCC_QW_LANES];
    ir_node_t *last = NULL;
    const ir_node_t *earliest_l0 = NULL;
    int consumed;
    zcc_qw_prov_t a_id, y_id, x_id;

    memset(&a_id, 0, sizeof(a_id));
    memset(&y_id, 0, sizeof(y_id));
    memset(&x_id, 0, sizeof(x_id));

    if (!out || !fn || !start || !zcc_avx512_qw_gate())
        return 0;

    memset(have, 0, sizeof(have));
    memset(lanes, 0, sizeof(lanes));

    for (n = start; n && nw < ZCC_QW_WINDOW; n = n->next) {
        if (n != start && zcc_qw_side_effect(n->op) && n->op != IR_STORE)
            break;
        window[nw++] = n;
    }

    /* First pass: match non-zero lanes to establish stable origins */
    for (i = 0; i < nw; i++) {
        zcc_qw_lane_t lane;
        int idx;
        if (!zcc_qw_is_f64_store(window[i]))
            continue;
        if (!zcc_qw_match_lane(fn, window[i], &lane, NULL))
            continue;
        idx = (int)(lane.y.off / 8);
        if (idx < 0 || idx >= ZCC_QW_LANES || have[idx])
            continue;
        if (idx == 0)
            continue; /* resolve lane 0 in second pass */
        if (got == 0) {
            a_id = lane.a;
            y_id = lane.y;
            x_id = lane.x0;
        } else {
            if (strcmp(lane.a.origin, a_id.origin) != 0)
                continue;
            if (strcmp(lane.y.origin, y_id.origin) != 0)
                continue;
            if (strcmp(lane.x0.origin, x_id.origin) != 0 ||
                lane.x0.off != x_id.off)
                continue;
        }
        if (lane.a.off != (long)idx * 8)
            continue;
        lanes[idx] = lane;
        have[idx] = 1;
        last = window[i];
        got++;
    }

    /* Second pass: match lane 0 using established x_id */
    if (got > 0 && !have[0]) {
        for (i = 0; i < nw; i++) {
            zcc_qw_lane_t lane;
            if (!zcc_qw_is_f64_store(window[i]))
                continue;
            if (!zcc_qw_match_lane(fn, window[i], &lane, &x_id))
                continue;
            if (lane.y.off == 0 && lane.a.off == 0 &&
                strcmp(lane.a.origin, a_id.origin) == 0 &&
                strcmp(lane.y.origin, y_id.origin) == 0 &&
                strcmp(lane.x0.origin, x_id.origin) == 0) {
                lanes[0] = lane;
                have[0] = 1;
                got++;
                break;
            }
        }
    }

    if (got != ZCC_QW_LANES) {
        if (zcc_avx512_qw_debug() && fn->name && strstr(fn->name, "qw_step"))
            fprintf(stderr,
                    "[avx512-qw] miss got=%d/%d start_op=%d\n",
                    got, ZCC_QW_LANES, (int)start->op);
        return 0;
    }
    for (i = 0; i < ZCC_QW_LANES; i++) {
        if (!have[i] || lanes[i].y.off != (long)i * 8 ||
            lanes[i].a.off != (long)i * 8)
            return 0;
    }

    /* Find earliest instruction belonging to lane 0 */
    earliest_l0 = lanes[0].store;
    for (n = fn->head; n; n = n->next) {
        if (n == lanes[0].store || n == lanes[0].fadd ||
            n == lanes[0].fmul  || n == lanes[0].load_a ||
            n == lanes[0].load_y || n == lanes[0].load_x0) {
            earliest_l0 = n;
            break;
        }
        /* check address-load definitions for lane 0 */
        if (lanes[0].load_a && strcmp(n->dst, lanes[0].load_a->src1) == 0) {
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

    fprintf(out, "    /* ZCC_AVX512_QW axpy8  y += A * x0  (8 x f64) */\n");
    load_address(out, lanes[0].a.origin, "%rax");
    fprintf(out, "    movq (%%rax), %%rax\n");
    load_address(out, lanes[0].y.origin, "%rcx");
    fprintf(out, "    movq (%%rcx), %%rcx\n");
    load_address(out, lanes[0].x0.origin, "%r11");
    fprintf(out, "    movsd (%%r11), %%xmm0\n");
    fprintf(out, "    vbroadcastsd %%xmm0, %%zmm0\n");
    fprintf(out, "    vmovupd (%%rax), %%zmm1\n");
    fprintf(out, "    vmovupd (%%rcx), %%zmm2\n");
    fprintf(out, "    vfmadd231pd %%zmm1, %%zmm0, %%zmm2\n");
    fprintf(out, "    vmovupd %%zmm2, (%%rcx)\n");
    fprintf(out, "    vzeroupper\n");

    consumed = 0;
    for (n = start; n; n = n->next) {
        consumed++;
        if (n == last)
            break;
    }
    if (zcc_avx512_qw_debug())
        fprintf(stderr,
                "[avx512-qw] hit consumed=%d A=%s y=%s x0=%s\n",
                consumed, lanes[0].a.origin, lanes[0].y.origin,
                lanes[0].x0.origin);
    return consumed;
}

#endif /* ZCC_AVX512_QW_PEEPHOLE_H */
