/* ================================================================ */
/* PART 6 (PTX): NATIVE C99-TO-NVIDIA PTX CODE GENERATOR (SM 80..120) */
/* ================================================================ */
#ifndef ZCC_MONOLITHIC_BUILD
#include "part1.c"
#include <stdint.h>
#endif

enum {
    PTX_CL_VOID = 0,
    PTX_CL_I32  = 1,
    PTX_CL_I64  = 2,
    PTX_CL_F32  = 3,
    PTX_CL_F64  = 4
};

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} PtxBuf;

typedef struct {
    char name[MAX_IDENT];
    Symbol *sym;
    Type *type;
    int reg_cl;
    int reg_id;
    int is_memory;
    int is_param;
    int stack_off;
    int size;
} PtxLocal;

typedef struct {
    char name[MAX_IDENT];
    Type *ret_type;
    int is_kernel;
    int num_params;
    int param_cls[MAX_PARAMS];
    Type *param_types[MAX_PARAMS];
} PtxFuncSig;

typedef struct {
    int cl;
    int id;
    Type *type;
} PtxVal;

typedef struct {
    Compiler *cc;
    Node *prog;
    const char *ptx_arch;
    PtxFuncSig *sigs;
    int num_sigs;
    int sig_cap;
    int cur_fn_idx;
    PtxLocal *locals;
    int num_locals;
    int local_cap;
    PtxBuf hdr;
    PtxBuf body;
    int cnt_pred;
    int cnt_i32;
    int cnt_i64;
    int cnt_f32;
    int cnt_f64;
    int label_seq;
    int local_frame_sz;
    int fp_reg;
    Type *ret_type;
    int ret_cl;
    int is_kernel;
    int break_lbls[64];
    int cont_lbls[64];
    int loop_depth;
    int errors;
} PtxCtx;

static void ptx_buf_init(PtxBuf *b) {
    b->cap = 4096;
    b->len = 0;
    b->data = (char *)malloc(b->cap);
    if (b->data) b->data[0] = '\0';
}

static void ptx_buf_free(PtxBuf *b) {
    if (b && b->data) free(b->data);
    if (b) { b->data = NULL; b->len = 0; b->cap = 0; }
}

static void ptx_buf_str(PtxBuf *b, const char *s) {
    if (!b || !s) return;
    size_t add = strlen(s);
    if (b->len + add + 1 >= b->cap) {
        size_t ncap = b->cap ? b->cap : 4096;
        while (b->len + add + 1 >= ncap) ncap *= 2;
        char *nd = (char *)realloc(b->data, ncap);
        if (!nd) return;
        b->data = nd;
        b->cap = ncap;
    }
    memcpy(b->data + b->len, s, add);
    b->len += add;
    b->data[b->len] = '\0';
}

static void ptx_emit_f32_hex(PtxBuf *b, float f) {
    unsigned char raw[4];
    unsigned int u = 0;
    memcpy(raw, &f, 4);
    u = (unsigned int)raw[0] | ((unsigned int)raw[1] << 8) |
        ((unsigned int)raw[2] << 16) | ((unsigned int)raw[3] << 24);
    char tmp[32];
    snprintf(tmp, sizeof(tmp), "0f%08X", u);
    ptx_buf_str(b, tmp);
}

static void ptx_emit_f64_hex(PtxBuf *b, double d) {
    unsigned char raw[8];
    unsigned int lo = 0, hi = 0;
    memcpy(raw, &d, 8);
    lo = (unsigned int)raw[0] | ((unsigned int)raw[1] << 8) |
         ((unsigned int)raw[2] << 16) | ((unsigned int)raw[3] << 24);
    hi = (unsigned int)raw[4] | ((unsigned int)raw[5] << 8) |
         ((unsigned int)raw[6] << 16) | ((unsigned int)raw[7] << 24);
    char tmp[32];
    snprintf(tmp, sizeof(tmp), "0d%08X%08X", hi, lo);
    ptx_buf_str(b, tmp);
}

static void ptx_fmt_u64_hex(char *out, size_t sz, long long val) {
    unsigned long long u = (unsigned long long)val;
    unsigned int lo = (unsigned int)(u & 0xFFFFFFFFULL);
    unsigned int hi = (unsigned int)((u >> 32) & 0xFFFFFFFFULL);
    snprintf(out, sz, "0x%08X%08XU", hi, lo);
}

static void ptx_reg_name(int cl, int id, char *out, size_t sz) {
    if (cl == PTX_CL_I32) snprintf(out, sz, "%%r%d", id);
    else if (cl == PTX_CL_I64) snprintf(out, sz, "%%rd%d", id);
    else if (cl == PTX_CL_F32) snprintf(out, sz, "%%f%d", id);
    else if (cl == PTX_CL_F64) snprintf(out, sz, "%%fd%d", id);
    else snprintf(out, sz, "%%r0");
}

static int ptx_alloc_reg(PtxCtx *ctx, int cl) {
    if (cl == PTX_CL_I32) return ctx->cnt_i32++;
    if (cl == PTX_CL_I64) return ctx->cnt_i64++;
    if (cl == PTX_CL_F32) return ctx->cnt_f32++;
    if (cl == PTX_CL_F64) return ctx->cnt_f64++;
    return 0;
}

static int ptx_alloc_pred(PtxCtx *ctx) {
    return ctx->cnt_pred++;
}

static int ptx_reg_class(Type *t) {
    if (!t) return PTX_CL_I32;
    switch (t->kind) {
        case TY_VOID: return PTX_CL_VOID;
        case TY_FLOAT: return PTX_CL_F32;
        case TY_DOUBLE:
        case TY_LONGDOUBLE: return PTX_CL_F64;
        case TY_LONG:
        case TY_ULONG:
            return (t->size == 4) ? PTX_CL_I32 : PTX_CL_I64;
        case TY_LONGLONG:
        case TY_ULONGLONG:
        case TY_PTR:
        case TY_ARRAY:
        case TY_FUNC:
        case TY_STRUCT:
        case TY_UNION:
            return PTX_CL_I64;
        default:
            return PTX_CL_I32;
    }
}

static int ptx_is_uns(Type *t) {
    if (!t) return 0;
    if (t->kind == TY_PTR || t->kind == TY_ARRAY || t->kind == TY_FUNC) return 1;
    return is_unsigned_type(t);
}

static const char *ptx_param_ty(Type *t) {
    int cl = ptx_reg_class(t);
    if (cl == PTX_CL_I64) return ".u64";
    if (cl == PTX_CL_F32) return ".f32";
    if (cl == PTX_CL_F64) return ".f64";
    return ptx_is_uns(t) ? ".u32" : ".s32";
}

static const char *ptx_mem_ty(Type *t) {
    if (!t) return ".s32";
    switch (t->kind) {
        case TY_CHAR: return ".s8";
        case TY_UCHAR: return ".u8";
        case TY_SHORT: return ".s16";
        case TY_USHORT: return ".u16";
        case TY_INT:
        case TY_ENUM: return ".s32";
        case TY_UINT: return ".u32";
        case TY_LONG: return (t->size == 4) ? ".s32" : ".s64";
        case TY_ULONG: return (t->size == 4) ? ".u32" : ".u64";
        case TY_LONGLONG: return ".s64";
        case TY_ULONGLONG: return ".u64";
        case TY_PTR:
        case TY_ARRAY:
        case TY_FUNC: return ".u64";
        case TY_FLOAT: return ".f32";
        case TY_DOUBLE:
        case TY_LONGDOUBLE: return ".f64";
        default: return ".u32";
    }
}

static const char *ptx_st_ty(Type *t) {
    if (!t) return ".u32";
    if (t->kind == TY_CHAR || t->kind == TY_UCHAR) return ".u8";
    if (t->kind == TY_SHORT || t->kind == TY_USHORT) return ".u16";
    if (t->kind == TY_FLOAT) return ".f32";
    if (t->kind == TY_DOUBLE || t->kind == TY_LONGDOUBLE) return ".f64";
    if (ptx_reg_class(t) == PTX_CL_I64) return ".u64";
    return ".u32";
}

static const char *ptx_cmp_ty(int cl, int uns) {
    if (cl == PTX_CL_F32) return ".f32";
    if (cl == PTX_CL_F64) return ".f64";
    if (cl == PTX_CL_I64) return uns ? ".u64" : ".s64";
    return uns ? ".u32" : ".s32";
}

static int ptx_promoted_cl(int c1, int c2) {
    if (c1 == PTX_CL_F64 || c2 == PTX_CL_F64) return PTX_CL_F64;
    if (c1 == PTX_CL_F32 || c2 == PTX_CL_F32) return PTX_CL_F32;
    if (c1 == PTX_CL_I64 || c2 == PTX_CL_I64) return PTX_CL_I64;
    return PTX_CL_I32;
}

static int ptx_ptr_elem_sz(Type *t) {
    if (!t) return 1;
    if ((t->kind == TY_PTR || t->kind == TY_ARRAY) && t->base) {
        int s = type_size(t->base);
        return (s > 0) ? s : 1;
    }
    return 1;
}

static PtxVal ptx_emit_cast(PtxCtx *ctx, PtxVal v, Type *to_type) {
    int to_cl = ptx_reg_class(to_type);
    int to_uns = ptx_is_uns(to_type);
    int from_uns = ptx_is_uns(v.type);
    char line[192], sname[32], dname[32];

    if (to_cl == PTX_CL_VOID) {
        PtxVal r;
        r.cl = PTX_CL_VOID; r.id = 0; r.type = to_type;
        return r;
    }

    if (v.cl == to_cl && (!to_type || (to_type->kind != TY_CHAR && to_type->kind != TY_UCHAR &&
                                       to_type->kind != TY_SHORT && to_type->kind != TY_USHORT))) {
        v.type = to_type;
        return v;
    }

    int d = ptx_alloc_reg(ctx, to_cl);
    ptx_reg_name(v.cl, v.id, sname, sizeof(sname));
    ptx_reg_name(to_cl, d, dname, sizeof(dname));

    if (v.cl == to_cl) {
        /* Sub-word narrowing within 32-bit register */
        if (to_type->kind == TY_UCHAR) {
            snprintf(line, sizeof(line), "    and.b32 %s, %s, 255;\n", dname, sname);
            ptx_buf_str(&ctx->body, line);
        } else if (to_type->kind == TY_USHORT) {
            snprintf(line, sizeof(line), "    and.b32 %s, %s, 65535;\n", dname, sname);
            ptx_buf_str(&ctx->body, line);
        } else if (to_type->kind == TY_CHAR || to_type->kind == TY_SHORT) {
            int sh = (to_type->kind == TY_CHAR) ? 24 : 16;
            int tmp = ptx_alloc_reg(ctx, PTX_CL_I32);
            char tname[32];
            ptx_reg_name(PTX_CL_I32, tmp, tname, sizeof(tname));
            snprintf(line, sizeof(line), "    shl.b32 %s, %s, %d;\n    shr.s32 %s, %s, %d;\n",
                     tname, sname, sh, dname, tname, sh);
            ptx_buf_str(&ctx->body, line);
        } else {
            snprintf(line, sizeof(line), "    mov.%s %s, %s;\n",
                     (to_cl == PTX_CL_I64) ? "u64" : (to_cl == PTX_CL_F32) ? "f32" :
                     (to_cl == PTX_CL_F64) ? "f64" : "u32", dname, sname);
            ptx_buf_str(&ctx->body, line);
        }
    } else if (v.cl == PTX_CL_I32 && to_cl == PTX_CL_I64) {
        snprintf(line, sizeof(line), "    cvt.%s %s, %s;\n", from_uns ? "u64.u32" : "s64.s32", dname, sname);
        ptx_buf_str(&ctx->body, line);
    } else if (v.cl == PTX_CL_I64 && to_cl == PTX_CL_I32) {
        snprintf(line, sizeof(line), "    cvt.%s %s, %s;\n", to_uns ? "u32.u64" : "s32.s64", dname, sname);
        ptx_buf_str(&ctx->body, line);
        if (to_type && (to_type->kind == TY_CHAR || to_type->kind == TY_UCHAR ||
                        to_type->kind == TY_SHORT || to_type->kind == TY_USHORT)) {
            PtxVal tmpv;
            tmpv.cl = PTX_CL_I32; tmpv.id = d; tmpv.type = ctx->cc->ty_int;
            return ptx_emit_cast(ctx, tmpv, to_type);
        }
    } else if (v.cl == PTX_CL_F32 && to_cl == PTX_CL_F64) {
        snprintf(line, sizeof(line), "    cvt.f64.f32 %s, %s;\n", dname, sname);
        ptx_buf_str(&ctx->body, line);
    } else if (v.cl == PTX_CL_F64 && to_cl == PTX_CL_F32) {
        snprintf(line, sizeof(line), "    cvt.rn.f32.f64 %s, %s;\n", dname, sname);
        ptx_buf_str(&ctx->body, line);
    } else if ((v.cl == PTX_CL_I32 || v.cl == PTX_CL_I64) && (to_cl == PTX_CL_F32 || to_cl == PTX_CL_F64)) {
        const char *dsuf = (to_cl == PTX_CL_F64) ? "f64" : "f32";
        const char *ssuf = (v.cl == PTX_CL_I64) ? (from_uns ? "u64" : "s64") : (from_uns ? "u32" : "s32");
        snprintf(line, sizeof(line), "    cvt.rn.%s.%s %s, %s;\n", dsuf, ssuf, dname, sname);
        ptx_buf_str(&ctx->body, line);
    } else if ((v.cl == PTX_CL_F32 || v.cl == PTX_CL_F64) && (to_cl == PTX_CL_I32 || to_cl == PTX_CL_I64)) {
        const char *dsuf = (to_cl == PTX_CL_I64) ? (to_uns ? "u64" : "s64") : (to_uns ? "u32" : "s32");
        const char *ssuf = (v.cl == PTX_CL_F64) ? "f64" : "f32";
        snprintf(line, sizeof(line), "    cvt.rzi.%s.%s %s, %s;\n", dsuf, ssuf, dname, sname);
        ptx_buf_str(&ctx->body, line);
    }

    PtxVal res;
    res.cl = to_cl;
    res.id = d;
    res.type = to_type;
    return res;
}

static PtxVal ptx_emit_conv_cl(PtxCtx *ctx, PtxVal v, int dst_cl, int dst_uns) {
    Type *t = ctx->cc->ty_int;
    if (dst_cl == PTX_CL_I32) t = dst_uns ? ctx->cc->ty_uint : ctx->cc->ty_int;
    else if (dst_cl == PTX_CL_I64) t = dst_uns ? ctx->cc->ty_ulonglong : ctx->cc->ty_longlong;
    else if (dst_cl == PTX_CL_F32) t = ctx->cc->ty_float;
    else if (dst_cl == PTX_CL_F64) t = ctx->cc->ty_double;
    return ptx_emit_cast(ctx, v, t);
}

static int ptx_emit_to_pred(PtxCtx *ctx, PtxVal v) {
    int p = ptx_alloc_pred(ctx);
    char line[160], sname[32];
    ptx_reg_name(v.cl, v.id, sname, sizeof(sname));
    if (v.cl == PTX_CL_F32) {
        snprintf(line, sizeof(line), "    setp.neu.f32 %%p%d, %s, 0f00000000;\n", p, sname);
    } else if (v.cl == PTX_CL_F64) {
        snprintf(line, sizeof(line), "    setp.neu.f64 %%p%d, %s, 0d0000000000000000;\n", p, sname);
    } else if (v.cl == PTX_CL_I64) {
        snprintf(line, sizeof(line), "    setp.ne.u64 %%p%d, %s, 0;\n", p, sname);
    } else {
        snprintf(line, sizeof(line), "    setp.ne.u32 %%p%d, %s, 0;\n", p, sname);
    }
    ptx_buf_str(&ctx->body, line);
    return p;
}

static void ptx_safe_ident(const char *src, char *out, size_t sz) {
    size_t i = 0;
    if (!src || !src[0]) {
        snprintf(out, sz, "anon");
        return;
    }
    for (; src[i] && i + 1 < sz; i++) {
        unsigned char c = (unsigned char)src[i];
        if (isalnum(c) || c == '_' || c == '$') out[i] = (char)c;
        else out[i] = '_';
    }
    out[i] = '\0';
}

static const char *ptx_var_name(Node *n) {
    if (!n) return "";
    if (n->name[0]) return n->name;
    if (n->sym && n->sym->name[0]) return n->sym->name;
    return "";
}

static PtxLocal *ptx_find_local(PtxCtx *ctx, Node *n) {
    if (!ctx || !n) return NULL;
    const char *vname = ptx_var_name(n);
    if (n->sym) {
        for (int i = 0; i < ctx->num_locals; i++) {
            if (ctx->locals[i].sym == n->sym) return &ctx->locals[i];
        }
    }
    if (vname[0]) {
        for (int i = 0; i < ctx->num_locals; i++) {
            if (strcmp(ctx->locals[i].name, vname) == 0) return &ctx->locals[i];
        }
    }
    return NULL;
}

static PtxLocal *ptx_ensure_local(PtxCtx *ctx, Node *n) {
    PtxLocal *loc = ptx_find_local(ctx, n);
    if (loc) {
        if (!loc->sym && n->sym) loc->sym = n->sym;
        return loc;
    }
    const char *vname = ptx_var_name(n);
    if (!vname[0]) return NULL;
    if (ctx->num_locals >= ctx->local_cap) {
        int ncap = ctx->local_cap ? ctx->local_cap * 2 : 64;
        PtxLocal *nl = (PtxLocal *)realloc(ctx->locals, (size_t)ncap * sizeof(PtxLocal));
        if (!nl) return NULL;
        ctx->locals = nl;
        ctx->local_cap = ncap;
    }
    loc = &ctx->locals[ctx->num_locals++];
    memset(loc, 0, sizeof(PtxLocal));
    strncpy(loc->name, vname, MAX_IDENT - 1);
    loc->sym = n->sym;
    loc->type = n->type ? n->type : ctx->cc->ty_int;
    loc->reg_cl = ptx_reg_class(loc->type);
    return loc;
}

static PtxFuncSig *ptx_find_sig(PtxCtx *ctx, const char *name) {
    if (!ctx || !name || !name[0]) return NULL;
    for (int i = 0; i < ctx->num_sigs; i++) {
        if (strcmp(ctx->sigs[i].name, name) == 0) return &ctx->sigs[i];
    }
    return NULL;
}

static void ptx_scan_locals(PtxCtx *ctx, Node *n) {
    if (!n) return;
    if (n->kind == ND_VAR) {
        const char *vname = ptx_var_name(n);
        int is_glob = (n->sym && n->sym->is_global) ? 1 : 0;
        int is_loc = (n->sym && (n->sym->is_local || n->sym->is_param)) ? 1 : 0;
        int is_ext = is_glob || (!is_loc && ptx_find_sig(ctx, vname) != NULL);
        if (vname[0] && !is_ext) {
            PtxLocal *loc = ptx_ensure_local(ctx, n);
            if (loc && n->type && (n->type->kind == TY_ARRAY || n->type->kind == TY_STRUCT || n->type->kind == TY_UNION)) {
                loc->is_memory = 1;
                loc->size = type_size(n->type);
                if (loc->size <= 0) loc->size = 4;
            }
        }
    } else if (n->kind == ND_ADDR && n->lhs && n->lhs->kind == ND_VAR) {
        const char *vname = ptx_var_name(n->lhs);
        int is_glob = (n->lhs->sym && n->lhs->sym->is_global) ? 1 : 0;
        int is_loc = (n->lhs->sym && (n->lhs->sym->is_local || n->lhs->sym->is_param)) ? 1 : 0;
        int is_ext = is_glob || (!is_loc && ptx_find_sig(ctx, vname) != NULL);
        if (vname[0] && !is_ext) {
            PtxLocal *loc = ptx_ensure_local(ctx, n->lhs);
            if (loc) {
                loc->is_memory = 1;
                loc->size = loc->type ? type_size(loc->type) : 4;
                if (loc->size <= 0) loc->size = 4;
            }
        }
    }
    if (n->lhs) ptx_scan_locals(ctx, n->lhs);
    if (n->rhs) ptx_scan_locals(ctx, n->rhs);
    if (n->cond) ptx_scan_locals(ctx, n->cond);
    if (n->then_body) ptx_scan_locals(ctx, n->then_body);
    if (n->else_body) ptx_scan_locals(ctx, n->else_body);
    if (n->body) ptx_scan_locals(ctx, n->body);
    if (n->init) ptx_scan_locals(ctx, n->init);
    if (n->inc) ptx_scan_locals(ctx, n->inc);
    if (n->stmts) {
        for (int i = 0; i < n->num_stmts; i++) ptx_scan_locals(ctx, n->stmts[i]);
    }
    if (n->args) {
        for (int i = 0; i < n->num_args; i++) ptx_scan_locals(ctx, n->args[i]);
    }
}

static int ptx_eval_const_i64(Node *n, long long *out) {
    if (!n) return 0;
    if (n->kind == ND_NUM || n->kind == ND_CHAR_LIT) {
        *out = n->int_val;
        return 1;
    }
    if (n->kind == ND_CAST) return ptx_eval_const_i64(n->lhs, out);
    if (n->kind == ND_NEG && ptx_eval_const_i64(n->lhs, out)) {
        *out = -(*out);
        return 1;
    }
    return 0;
}

static PtxVal ptx_lower_expr(PtxCtx *ctx, Node *n);
static void ptx_lower_stmt(PtxCtx *ctx, Node *n);

static PtxVal ptx_lower_addr(PtxCtx *ctx, Node *n) {
    PtxVal res;
    res.cl = PTX_CL_I64;
    res.id = ptx_alloc_reg(ctx, PTX_CL_I64);
    res.type = ctx->cc->ty_ulonglong;
    char dname[32], fpname[32], line[192];
    ptx_reg_name(PTX_CL_I64, res.id, dname, sizeof(dname));

    if (!n) {
        snprintf(line, sizeof(line), "    mov.u64 %s, 0;\n", dname);
        ptx_buf_str(&ctx->body, line);
        return res;
    }
    if (n->kind == ND_VAR) {
        PtxLocal *loc = ptx_find_local(ctx, n);
        if (loc && loc->is_memory && ctx->fp_reg >= 0) {
            ptx_reg_name(PTX_CL_I64, ctx->fp_reg, fpname, sizeof(fpname));
            if (loc->stack_off > 0) {
                snprintf(line, sizeof(line), "    add.u64 %s, %s, %d;\n", dname, fpname, loc->stack_off);
            } else {
                snprintf(line, sizeof(line), "    mov.u64 %s, %s;\n", dname, fpname);
            }
            ptx_buf_str(&ctx->body, line);
            return res;
        }
    } else if (n->kind == ND_DEREF) {
        PtxVal pv = ptx_lower_expr(ctx, n->lhs);
        return ptx_emit_conv_cl(ctx, pv, PTX_CL_I64, 1);
    } else if (n->kind == ND_MEMBER) {
        PtxVal base = (n->lhs && n->lhs->type && is_pointer(n->lhs->type) && n->lhs->type->kind != TY_ARRAY)
            ? ptx_emit_conv_cl(ctx, ptx_lower_expr(ctx, n->lhs), PTX_CL_I64, 1)
            : ptx_lower_addr(ctx, n->lhs);
        char bname[32];
        ptx_reg_name(PTX_CL_I64, base.id, bname, sizeof(bname));
        if (n->member_offset > 0) {
            snprintf(line, sizeof(line), "    add.u64 %s, %s, %d;\n", dname, bname, n->member_offset);
        } else {
            snprintf(line, sizeof(line), "    mov.u64 %s, %s;\n", dname, bname);
        }
        ptx_buf_str(&ctx->body, line);
        return res;
    } else if (n->kind == ND_ADDR) {
        return ptx_lower_addr(ctx, n->lhs);
    }

    fprintf(stderr, "zcc ptx: unsupported address expression kind %d (line %d)\n", n->kind, n->line);
    ctx->errors++;
    snprintf(line, sizeof(line), "    mov.u64 %s, 0;\n", dname);
    ptx_buf_str(&ctx->body, line);
    return res;
}

static PtxVal ptx_emit_load(PtxCtx *ctx, PtxVal addr, Type *t) {
    Type *vt = t ? t : ctx->cc->ty_int;
    int cl = ptx_reg_class(vt);
    if (vt->kind == TY_ARRAY || vt->kind == TY_STRUCT || vt->kind == TY_UNION) {
        addr.type = vt;
        return addr;
    }
    PtxVal dst;
    dst.cl = cl;
    dst.id = ptx_alloc_reg(ctx, cl);
    dst.type = vt;
    char dname[32], aname[32], line[128];
    ptx_reg_name(cl, dst.id, dname, sizeof(dname));
    ptx_reg_name(PTX_CL_I64, addr.id, aname, sizeof(aname));
    snprintf(line, sizeof(line), "    ld%s %s, [%s];\n", ptx_mem_ty(vt), dname, aname);
    ptx_buf_str(&ctx->body, line);
    return dst;
}

static void ptx_emit_store(PtxCtx *ctx, PtxVal addr, Type *t, PtxVal val) {
    Type *vt = t ? t : ctx->cc->ty_int;
    PtxVal cv = ptx_emit_cast(ctx, val, vt);
    char aname[32], vname[32], line[128];
    ptx_reg_name(PTX_CL_I64, addr.id, aname, sizeof(aname));
    ptx_reg_name(cv.cl, cv.id, vname, sizeof(vname));
    snprintf(line, sizeof(line), "    st%s [%s], %s;\n", ptx_st_ty(vt), aname, vname);
    ptx_buf_str(&ctx->body, line);
}

static PtxVal ptx_emit_builtin(PtxCtx *ctx, Node *n, int *handled) {
    const char *fn = n->func_name;
    PtxVal res;
    res.cl = PTX_CL_VOID; res.id = 0; res.type = ctx->cc->ty_void;
    *handled = 0;
    if (!fn || !fn[0]) return res;

    char line[512], dname[32], a0[32], a1[32], a2[32];

    /* Special read-only thread/warp/grid/clock registers */
    const char *sreg = NULL;
    if (strcmp(fn, "__ptx_tid_x") == 0) sreg = "%tid.x";
    else if (strcmp(fn, "__ptx_tid_y") == 0) sreg = "%tid.y";
    else if (strcmp(fn, "__ptx_tid_z") == 0) sreg = "%tid.z";
    else if (strcmp(fn, "__ptx_ntid_x") == 0) sreg = "%ntid.x";
    else if (strcmp(fn, "__ptx_ntid_y") == 0) sreg = "%ntid.y";
    else if (strcmp(fn, "__ptx_ntid_z") == 0) sreg = "%ntid.z";
    else if (strcmp(fn, "__ptx_ctaid_x") == 0) sreg = "%ctaid.x";
    else if (strcmp(fn, "__ptx_ctaid_y") == 0) sreg = "%ctaid.y";
    else if (strcmp(fn, "__ptx_ctaid_z") == 0) sreg = "%ctaid.z";
    else if (strcmp(fn, "__ptx_nctaid_x") == 0) sreg = "%nctaid.x";
    else if (strcmp(fn, "__ptx_nctaid_y") == 0) sreg = "%nctaid.y";
    else if (strcmp(fn, "__ptx_nctaid_z") == 0) sreg = "%nctaid.z";
    else if (strcmp(fn, "__ptx_lane_id") == 0) sreg = "%laneid";
    else if (strcmp(fn, "__ptx_clock") == 0) sreg = "%clock";
    if (sreg) {
        *handled = 1;
        res.cl = PTX_CL_I32; res.id = ptx_alloc_reg(ctx, PTX_CL_I32); res.type = ctx->cc->ty_uint;
        ptx_reg_name(PTX_CL_I32, res.id, dname, sizeof(dname));
        snprintf(line, sizeof(line), "    mov.u32 %s, %s;\n", dname, sreg);
        ptx_buf_str(&ctx->body, line);
        return res;
    }

    if (strcmp(fn, "__ptx_syncthreads") == 0) {
        *handled = 1;
        ptx_buf_str(&ctx->body, "    bar.sync 0;\n");
        return res;
    }

    /* 1-arg float hardware ops */
    const char *f1op = NULL;
    if (strcmp(fn, "__ptx_sqrtf") == 0 || strcmp(fn, "sqrtf") == 0) f1op = "sqrt.rn.f32";
    else if (strcmp(fn, "__ptx_rsqrtf") == 0) f1op = "rsqrt.approx.f32";
    else if (strcmp(fn, "__ptx_sinf") == 0 || strcmp(fn, "sinf") == 0) f1op = "sin.approx.f32";
    else if (strcmp(fn, "__ptx_cosf") == 0 || strcmp(fn, "cosf") == 0) f1op = "cos.approx.f32";
    else if (strcmp(fn, "__ptx_ex2f") == 0) f1op = "ex2.approx.f32";
    else if (strcmp(fn, "__ptx_lg2f") == 0) f1op = "lg2.approx.f32";
    else if (strcmp(fn, "__ptx_fabsf") == 0 || strcmp(fn, "fabsf") == 0) f1op = "abs.f32";
    else if (strcmp(fn, "__ptx_floorf") == 0 || strcmp(fn, "floorf") == 0) f1op = "cvt.rmi.f32.f32";
    if (f1op && n->num_args >= 1) {
        *handled = 1;
        PtxVal v0 = ptx_emit_cast(ctx, ptx_lower_expr(ctx, n->args[0]), ctx->cc->ty_float);
        res.cl = PTX_CL_F32; res.id = ptx_alloc_reg(ctx, PTX_CL_F32); res.type = ctx->cc->ty_float;
        ptx_reg_name(PTX_CL_F32, res.id, dname, sizeof(dname));
        ptx_reg_name(PTX_CL_F32, v0.id, a0, sizeof(a0));
        snprintf(line, sizeof(line), "    %s %s, %s;\n", f1op, dname, a0);
        ptx_buf_str(&ctx->body, line);
        return res;
    }

    /* 1-arg double hardware ops */
    if ((strcmp(fn, "__ptx_sqrt") == 0 || strcmp(fn, "sqrt") == 0 ||
         strcmp(fn, "__ptx_fabs") == 0 || strcmp(fn, "fabs") == 0) && n->num_args >= 1) {
        const char *d1op = (strcmp(fn, "__ptx_sqrt") == 0 || strcmp(fn, "sqrt") == 0) ? "sqrt.rn.f64" : "abs.f64";
        *handled = 1;
        PtxVal v0 = ptx_emit_cast(ctx, ptx_lower_expr(ctx, n->args[0]), ctx->cc->ty_double);
        res.cl = PTX_CL_F64; res.id = ptx_alloc_reg(ctx, PTX_CL_F64); res.type = ctx->cc->ty_double;
        ptx_reg_name(PTX_CL_F64, res.id, dname, sizeof(dname));
        ptx_reg_name(PTX_CL_F64, v0.id, a0, sizeof(a0));
        snprintf(line, sizeof(line), "    %s %s, %s;\n", d1op, dname, a0);
        ptx_buf_str(&ctx->body, line);
        return res;
    }

    if ((strcmp(fn, "__ptx_minf") == 0 || strcmp(fn, "fminf") == 0 ||
         strcmp(fn, "__ptx_maxf") == 0 || strcmp(fn, "fmaxf") == 0) && n->num_args >= 2) {
        const char *m2 = (strcmp(fn, "__ptx_minf") == 0 || strcmp(fn, "fminf") == 0) ? "min.f32" : "max.f32";
        *handled = 1;
        PtxVal v0 = ptx_emit_cast(ctx, ptx_lower_expr(ctx, n->args[0]), ctx->cc->ty_float);
        PtxVal v1 = ptx_emit_cast(ctx, ptx_lower_expr(ctx, n->args[1]), ctx->cc->ty_float);
        res.cl = PTX_CL_F32; res.id = ptx_alloc_reg(ctx, PTX_CL_F32); res.type = ctx->cc->ty_float;
        ptx_reg_name(PTX_CL_F32, res.id, dname, sizeof(dname));
        ptx_reg_name(PTX_CL_F32, v0.id, a0, sizeof(a0));
        ptx_reg_name(PTX_CL_F32, v1.id, a1, sizeof(a1));
        snprintf(line, sizeof(line), "    %s %s, %s, %s;\n", m2, dname, a0, a1);
        ptx_buf_str(&ctx->body, line);
        return res;
    }

    if ((strcmp(fn, "__ptx_fmaf") == 0 || strcmp(fn, "fmaf") == 0) && n->num_args >= 3) {
        *handled = 1;
        PtxVal v0 = ptx_emit_cast(ctx, ptx_lower_expr(ctx, n->args[0]), ctx->cc->ty_float);
        PtxVal v1 = ptx_emit_cast(ctx, ptx_lower_expr(ctx, n->args[1]), ctx->cc->ty_float);
        PtxVal v2 = ptx_emit_cast(ctx, ptx_lower_expr(ctx, n->args[2]), ctx->cc->ty_float);
        res.cl = PTX_CL_F32; res.id = ptx_alloc_reg(ctx, PTX_CL_F32); res.type = ctx->cc->ty_float;
        ptx_reg_name(PTX_CL_F32, res.id, dname, sizeof(dname));
        ptx_reg_name(PTX_CL_F32, v0.id, a0, sizeof(a0));
        ptx_reg_name(PTX_CL_F32, v1.id, a1, sizeof(a1));
        ptx_reg_name(PTX_CL_F32, v2.id, a2, sizeof(a2));
        snprintf(line, sizeof(line), "    fma.rn.f32 %s, %s, %s, %s;\n", dname, a0, a1, a2);
        ptx_buf_str(&ctx->body, line);
        return res;
    }

    if ((strcmp(fn, "__ptx_fma") == 0 || strcmp(fn, "fma") == 0) && n->num_args >= 3) {
        *handled = 1;
        PtxVal v0 = ptx_emit_cast(ctx, ptx_lower_expr(ctx, n->args[0]), ctx->cc->ty_double);
        PtxVal v1 = ptx_emit_cast(ctx, ptx_lower_expr(ctx, n->args[1]), ctx->cc->ty_double);
        PtxVal v2 = ptx_emit_cast(ctx, ptx_lower_expr(ctx, n->args[2]), ctx->cc->ty_double);
        res.cl = PTX_CL_F64; res.id = ptx_alloc_reg(ctx, PTX_CL_F64); res.type = ctx->cc->ty_double;
        ptx_reg_name(PTX_CL_F64, res.id, dname, sizeof(dname));
        ptx_reg_name(PTX_CL_F64, v0.id, a0, sizeof(a0));
        ptx_reg_name(PTX_CL_F64, v1.id, a1, sizeof(a1));
        ptx_reg_name(PTX_CL_F64, v2.id, a2, sizeof(a2));
        snprintf(line, sizeof(line), "    fma.rn.f64 %s, %s, %s, %s;\n", dname, a0, a1, a2);
        ptx_buf_str(&ctx->body, line);
        return res;
    }

    if (strcmp(fn, "__ptx_lop3") == 0 && n->num_args >= 4) {
        *handled = 1;
        long long imm = 0;
        if (!ptx_eval_const_i64(n->args[3], &imm)) {
            fprintf(stderr, "zcc ptx: __ptx_lop3 requires constant immLUT (line %d)\n", n->line);
            ctx->errors++;
        }
        PtxVal v0 = ptx_emit_cast(ctx, ptx_lower_expr(ctx, n->args[0]), ctx->cc->ty_uint);
        PtxVal v1 = ptx_emit_cast(ctx, ptx_lower_expr(ctx, n->args[1]), ctx->cc->ty_uint);
        PtxVal v2 = ptx_emit_cast(ctx, ptx_lower_expr(ctx, n->args[2]), ctx->cc->ty_uint);
        res.cl = PTX_CL_I32; res.id = ptx_alloc_reg(ctx, PTX_CL_I32); res.type = ctx->cc->ty_uint;
        ptx_reg_name(PTX_CL_I32, res.id, dname, sizeof(dname));
        ptx_reg_name(PTX_CL_I32, v0.id, a0, sizeof(a0));
        ptx_reg_name(PTX_CL_I32, v1.id, a1, sizeof(a1));
        ptx_reg_name(PTX_CL_I32, v2.id, a2, sizeof(a2));
        snprintf(line, sizeof(line), "    lop3.b32 %s, %s, %s, %s, 0x%02X;\n",
                 dname, a0, a1, a2, (unsigned int)(imm & 0xFF));
        ptx_buf_str(&ctx->body, line);
        return res;
    }

    if (strcmp(fn, "__ptx_rotr32") == 0 && n->num_args >= 2) {
        *handled = 1;
        PtxVal v0 = ptx_emit_cast(ctx, ptx_lower_expr(ctx, n->args[0]), ctx->cc->ty_uint);
        PtxVal v1 = ptx_emit_cast(ctx, ptx_lower_expr(ctx, n->args[1]), ctx->cc->ty_uint);
        res.cl = PTX_CL_I32; res.id = ptx_alloc_reg(ctx, PTX_CL_I32); res.type = ctx->cc->ty_uint;
        ptx_reg_name(PTX_CL_I32, res.id, dname, sizeof(dname));
        ptx_reg_name(PTX_CL_I32, v0.id, a0, sizeof(a0));
        ptx_reg_name(PTX_CL_I32, v1.id, a1, sizeof(a1));
        snprintf(line, sizeof(line), "    shf.r.wrap.b32 %s, %s, %s, %s;\n", dname, a0, a0, a1);
        ptx_buf_str(&ctx->body, line);
        return res;
    }

    if ((strcmp(fn, "__ptx_popc32") == 0 || strcmp(fn, "__ptx_clz32") == 0) && n->num_args >= 1) {
        *handled = 1;
        PtxVal v0 = ptx_emit_cast(ctx, ptx_lower_expr(ctx, n->args[0]), ctx->cc->ty_uint);
        res.cl = PTX_CL_I32; res.id = ptx_alloc_reg(ctx, PTX_CL_I32); res.type = ctx->cc->ty_uint;
        ptx_reg_name(PTX_CL_I32, res.id, dname, sizeof(dname));
        ptx_reg_name(PTX_CL_I32, v0.id, a0, sizeof(a0));
        snprintf(line, sizeof(line), "    %s %s, %s;\n",
                 (strcmp(fn, "__ptx_popc32") == 0) ? "popc.b32" : "clz.b32", dname, a0);
        ptx_buf_str(&ctx->body, line);
        return res;
    }

    if (strcmp(fn, "__ptx_atomic_add_u32") == 0 && n->num_args >= 2) {
        *handled = 1;
        PtxVal v0 = ptx_emit_conv_cl(ctx, ptx_lower_expr(ctx, n->args[0]), PTX_CL_I64, 1);
        PtxVal v1 = ptx_emit_cast(ctx, ptx_lower_expr(ctx, n->args[1]), ctx->cc->ty_uint);
        res.cl = PTX_CL_I32; res.id = ptx_alloc_reg(ctx, PTX_CL_I32); res.type = ctx->cc->ty_uint;
        ptx_reg_name(PTX_CL_I32, res.id, dname, sizeof(dname));
        ptx_reg_name(PTX_CL_I64, v0.id, a0, sizeof(a0));
        ptx_reg_name(PTX_CL_I32, v1.id, a1, sizeof(a1));
        snprintf(line, sizeof(line), "    atom.add.u32 %s, [%s], %s;\n", dname, a0, a1);
        ptx_buf_str(&ctx->body, line);
        return res;
    }

    if (strcmp(fn, "__ptx_mma_m16n8k16_f16") == 0 && n->num_args >= 3) {
        *handled = 1;
        PtxVal va = ptx_emit_conv_cl(ctx, ptx_lower_expr(ctx, n->args[0]), PTX_CL_I64, 1);
        PtxVal vb = ptx_emit_conv_cl(ctx, ptx_lower_expr(ctx, n->args[1]), PTX_CL_I64, 1);
        PtxVal vc = ptx_emit_conv_cl(ctx, ptx_lower_expr(ctx, n->args[2]), PTX_CL_I64, 1);
        ptx_reg_name(PTX_CL_I64, va.id, a0, sizeof(a0));
        ptx_reg_name(PTX_CL_I64, vb.id, a1, sizeof(a1));
        ptx_reg_name(PTX_CL_I64, vc.id, a2, sizeof(a2));
        ptx_buf_str(&ctx->body,
            "    {\n"
            "        .reg .b32 %m_tid, %m_lane, %m_g, %m_t;\n"
            "        .reg .b32 %m_a0, %m_a1, %m_a2, %m_a3, %m_b0, %m_b1;\n"
            "        .reg .f32 %m_c0, %m_c1, %m_c2, %m_c3, %m_d0, %m_d1, %m_d2, %m_d3;\n"
            "        .reg .b64 %m_ra, %m_rb, %m_rc, %m_p0, %m_p1, %m_p2, %m_p3, %m_pb0, %m_pb1;\n"
            "        .reg .b64 %m_o0, %m_o1, %m_o2, %m_o3, %m_ob0, %m_ob1, %m_od0, %m_od1;\n"
            "        .reg .b32 %m_ia0, %m_ia1, %m_ia2, %m_ia3, %m_ib0, %m_ib1, %m_id0, %m_id1, %m_t2;\n");
        snprintf(line, sizeof(line),
            "        mov.u64 %%m_ra, %s;\n"
            "        mov.u64 %%m_rb, %s;\n"
            "        mov.u64 %%m_rc, %s;\n", a0, a1, a2);
        ptx_buf_str(&ctx->body, line);
        ptx_buf_str(&ctx->body,
            "        mov.u32 %m_tid, %tid.x;\n"
            "        and.b32 %m_lane, %m_tid, 31;\n"
            "        shr.u32 %m_g, %m_lane, 2;\n"
            "        and.b32 %m_t, %m_lane, 3;\n"
            "        shl.b32 %m_t2, %m_t, 1;\n"
            "        mad.lo.u32 %m_ia0, %m_g, 16, %m_t2;\n"
            "        add.u32 %m_ia1, %m_ia0, 128;\n"
            "        add.u32 %m_ia2, %m_ia0, 8;\n"
            "        add.u32 %m_ia3, %m_ia1, 8;\n"
            "        mul.wide.u32 %m_o0, %m_ia0, 2; add.u64 %m_p0, %m_ra, %m_o0; ld.u32 %m_a0, [%m_p0];\n"
            "        mul.wide.u32 %m_o1, %m_ia1, 2; add.u64 %m_p1, %m_ra, %m_o1; ld.u32 %m_a1, [%m_p1];\n"
            "        mul.wide.u32 %m_o2, %m_ia2, 2; add.u64 %m_p2, %m_ra, %m_o2; ld.u32 %m_a2, [%m_p2];\n"
            "        mul.wide.u32 %m_o3, %m_ia3, 2; add.u64 %m_p3, %m_ra, %m_o3; ld.u32 %m_a3, [%m_p3];\n"
            "        mad.lo.u32 %m_ib0, %m_g, 16, %m_t2;\n"
            "        add.u32 %m_ib1, %m_ib0, 8;\n"
            "        mul.wide.u32 %m_ob0, %m_ib0, 2; add.u64 %m_pb0, %m_rb, %m_ob0; ld.u32 %m_b0, [%m_pb0];\n"
            "        mul.wide.u32 %m_ob1, %m_ib1, 2; add.u64 %m_pb1, %m_rb, %m_ob1; ld.u32 %m_b1, [%m_pb1];\n"
            "        mov.f32 %m_c0, 0f00000000; mov.f32 %m_c1, 0f00000000;\n"
            "        mov.f32 %m_c2, 0f00000000; mov.f32 %m_c3, 0f00000000;\n"
            "        mma.sync.aligned.m16n8k16.row.col.f32.f16.f16.f32\n"
            "            {%m_d0, %m_d1, %m_d2, %m_d3},\n"
            "            {%m_a0, %m_a1, %m_a2, %m_a3},\n"
            "            {%m_b0, %m_b1},\n"
            "            {%m_c0, %m_c1, %m_c2, %m_c3};\n"
            "        mad.lo.u32 %m_id0, %m_g, 8, %m_t2;\n"
            "        add.u32 %m_id1, %m_id0, 64;\n"
            "        mul.wide.u32 %m_od0, %m_id0, 4; add.u64 %m_p0, %m_rc, %m_od0; st.v2.f32 [%m_p0], {%m_d0, %m_d1};\n"
            "        mul.wide.u32 %m_od1, %m_id1, 4; add.u64 %m_p1, %m_rc, %m_od1; st.v2.f32 [%m_p1], {%m_d2, %m_d3};\n"
            "    }\n");
        return res;
    }

    if (strcmp(fn, "__ptx_f16_to_f32") == 0 && n->num_args >= 1) {
        *handled = 1;
        PtxVal v0 = ptx_emit_cast(ctx, ptx_lower_expr(ctx, n->args[0]), ctx->cc->ty_uint);
        res.cl = PTX_CL_F32; res.id = ptx_alloc_reg(ctx, PTX_CL_F32); res.type = ctx->cc->ty_float;
        ptx_reg_name(PTX_CL_F32, res.id, dname, sizeof(dname));
        ptx_reg_name(PTX_CL_I32, v0.id, a0, sizeof(a0));
        snprintf(line, sizeof(line),
            "    { .reg .b16 %%h_in; cvt.u16.u32 %%h_in, %s; cvt.f32.f16 %s, %%h_in; }\n",
            a0, dname);
        ptx_buf_str(&ctx->body, line);
        return res;
    }

    if (strcmp(fn, "__ptx_bf16_to_f32") == 0 && n->num_args >= 1) {
        *handled = 1;
        PtxVal v0 = ptx_emit_cast(ctx, ptx_lower_expr(ctx, n->args[0]), ctx->cc->ty_uint);
        res.cl = PTX_CL_F32; res.id = ptx_alloc_reg(ctx, PTX_CL_F32); res.type = ctx->cc->ty_float;
        ptx_reg_name(PTX_CL_F32, res.id, dname, sizeof(dname));
        ptx_reg_name(PTX_CL_I32, v0.id, a0, sizeof(a0));
        snprintf(line, sizeof(line),
            "    { .reg .b32 %%bf_tmp; shl.b32 %%bf_tmp, %s, 16; mov.b32 %s, %%bf_tmp; }\n",
            a0, dname);
        ptx_buf_str(&ctx->body, line);
        return res;
    }

    if (strcmp(fn, "__ptx_f32_to_f16") == 0 && n->num_args >= 1) {
        *handled = 1;
        PtxVal v0 = ptx_emit_cast(ctx, ptx_lower_expr(ctx, n->args[0]), ctx->cc->ty_float);
        res.cl = PTX_CL_I32; res.id = ptx_alloc_reg(ctx, PTX_CL_I32); res.type = ctx->cc->ty_uint;
        ptx_reg_name(PTX_CL_I32, res.id, dname, sizeof(dname));
        ptx_reg_name(PTX_CL_F32, v0.id, a0, sizeof(a0));
        snprintf(line, sizeof(line),
            "    { .reg .b16 %%h_out; cvt.rn.f16.f32 %%h_out, %s; cvt.u32.u16 %s, %%h_out; }\n",
            a0, dname);
        ptx_buf_str(&ctx->body, line);
        return res;
    }

    if ((strcmp(fn, "__ptx_shfl_down_f32") == 0 || strcmp(fn, "__ptx_shfl_idx_f32") == 0) && n->num_args >= 2) {
        *handled = 1;
        const char *mode = (strcmp(fn, "__ptx_shfl_down_f32") == 0) ? "down" : "idx";
        PtxVal v0 = ptx_emit_cast(ctx, ptx_lower_expr(ctx, n->args[0]), ctx->cc->ty_float);
        PtxVal v1 = ptx_emit_cast(ctx, ptx_lower_expr(ctx, n->args[1]), ctx->cc->ty_uint);
        res.cl = PTX_CL_F32; res.id = ptx_alloc_reg(ctx, PTX_CL_F32); res.type = ctx->cc->ty_float;
        ptx_reg_name(PTX_CL_F32, res.id, dname, sizeof(dname));
        ptx_reg_name(PTX_CL_F32, v0.id, a0, sizeof(a0));
        ptx_reg_name(PTX_CL_I32, v1.id, a1, sizeof(a1));
        snprintf(line, sizeof(line),
            "    { .reg .b32 %%sh_in, %%sh_out; mov.b32 %%sh_in, %s; shfl.sync.%s.b32 %%sh_out, %%sh_in, %s, 31, 0xffffffff; mov.b32 %s, %%sh_out; }\n",
            a0, mode, a1, dname);
        ptx_buf_str(&ctx->body, line);
        return res;
    }

    if (strcmp(fn, "__ptx_mma_gemm_tile_f16") == 0 && n->num_args >= 8) {
        *handled = 1;
        int l_loop = ctx->label_seq++;
        int l_end = ctx->label_seq++;
        PtxVal va = ptx_emit_conv_cl(ctx, ptx_lower_expr(ctx, n->args[0]), PTX_CL_I64, 1);
        PtxVal vb = ptx_emit_conv_cl(ctx, ptx_lower_expr(ctx, n->args[1]), PTX_CL_I64, 1);
        PtxVal vd = ptx_emit_conv_cl(ctx, ptx_lower_expr(ctx, n->args[2]), PTX_CL_I64, 1);
        PtxVal vm = ptx_emit_cast(ctx, ptx_lower_expr(ctx, n->args[3]), ctx->cc->ty_uint);
        PtxVal vn = ptx_emit_cast(ctx, ptx_lower_expr(ctx, n->args[4]), ctx->cc->ty_uint);
        PtxVal vk = ptx_emit_cast(ctx, ptx_lower_expr(ctx, n->args[5]), ctx->cc->ty_uint);
        PtxVal vr0 = ptx_emit_cast(ctx, ptx_lower_expr(ctx, n->args[6]), ctx->cc->ty_uint);
        PtxVal vc0 = ptx_emit_cast(ctx, ptx_lower_expr(ctx, n->args[7]), ctx->cc->ty_uint);
        char a3[32], a4[32], a5[32], a6[32], a7[32];
        char mg_buf[2048];
        ptx_reg_name(PTX_CL_I64, va.id, a0, sizeof(a0));
        ptx_reg_name(PTX_CL_I64, vb.id, a1, sizeof(a1));
        ptx_reg_name(PTX_CL_I64, vd.id, a2, sizeof(a2));
        ptx_reg_name(PTX_CL_I32, vm.id, a3, sizeof(a3));
        ptx_reg_name(PTX_CL_I32, vn.id, a4, sizeof(a4));
        ptx_reg_name(PTX_CL_I32, vk.id, a5, sizeof(a5));
        ptx_reg_name(PTX_CL_I32, vr0.id, a6, sizeof(a6));
        ptx_reg_name(PTX_CL_I32, vc0.id, a7, sizeof(a7));
        ptx_buf_str(&ctx->body,
            "    {\n"
            "        .reg .pred %mg_p, %mg_q;\n"
            "        .reg .b32 %mg_M, %mg_N, %mg_K, %mg_row0, %mg_col0, %mg_Mm1, %mg_Nm1;\n"
            "        .reg .b32 %mg_lane, %mg_g, %mg_t, %mg_t2, %mg_ar0, %mg_ar1, %mg_bn;\n"
            "        .reg .b32 %mg_k, %mg_k16, %mg_ia0, %mg_ia1, %mg_ia2, %mg_ia3, %mg_ib0, %mg_ib1;\n"
            "        .reg .b32 %mg_a0, %mg_a1, %mg_a2, %mg_a3, %mg_b0, %mg_b1;\n"
            "        .reg .f32 %mg_d0, %mg_d1, %mg_d2, %mg_d3;\n"
            "        .reg .b64 %mg_ra, %mg_rb, %mg_rd, %mg_off, %mg_ptr;\n"
            "        .reg .b32 %mg_r0, %mg_r1, %mg_c0, %mg_c1, %mg_di;\n");
        snprintf(mg_buf, sizeof(mg_buf),
            "        mov.u64 %%mg_ra, %s;\n"
            "        mov.u64 %%mg_rb, %s;\n"
            "        mov.u64 %%mg_rd, %s;\n"
            "        mov.u32 %%mg_M, %s;\n"
            "        mov.u32 %%mg_N, %s;\n"
            "        mov.u32 %%mg_K, %s;\n"
            "        mov.u32 %%mg_row0, %s;\n"
            "        mov.u32 %%mg_col0, %s;\n",
            a0, a1, a2, a3, a4, a5, a6, a7);
        ptx_buf_str(&ctx->body, mg_buf);
        snprintf(mg_buf, sizeof(mg_buf),
            "        setp.eq.u32 %%mg_p, %%mg_M, 0;\n"
            "        @%%mg_p bra L_mma_e_%d;\n"
            "        setp.eq.u32 %%mg_p, %%mg_N, 0;\n"
            "        @%%mg_p bra L_mma_e_%d;\n"
            "        mov.f32 %%mg_d0, 0f00000000; mov.f32 %%mg_d1, 0f00000000;\n"
            "        mov.f32 %%mg_d2, 0f00000000; mov.f32 %%mg_d3, 0f00000000;\n"
            "        mov.u32 %%mg_lane, %%tid.x; and.b32 %%mg_lane, %%mg_lane, 31;\n"
            "        shr.u32 %%mg_g, %%mg_lane, 2;\n"
            "        and.b32 %%mg_t, %%mg_lane, 3;\n"
            "        shl.b32 %%mg_t2, %%mg_t, 1;\n"
            "        sub.u32 %%mg_Mm1, %%mg_M, 1;\n"
            "        sub.u32 %%mg_Nm1, %%mg_N, 1;\n"
            "        add.u32 %%mg_ar0, %%mg_row0, %%mg_g; min.u32 %%mg_ar0, %%mg_ar0, %%mg_Mm1;\n"
            "        add.u32 %%mg_ar1, %%mg_row0, %%mg_g; add.u32 %%mg_ar1, %%mg_ar1, 8; min.u32 %%mg_ar1, %%mg_ar1, %%mg_Mm1;\n"
            "        add.u32 %%mg_bn, %%mg_col0, %%mg_g; min.u32 %%mg_bn, %%mg_bn, %%mg_Nm1;\n"
            "        mov.u32 %%mg_k, 0;\n"
            "L_mma_k_%d:\n"
            "        add.u32 %%mg_k16, %%mg_k, 16;\n"
            "        setp.gt.u32 %%mg_p, %%mg_k16, %%mg_K;\n"
            "        @%%mg_p bra L_mma_kd_%d;\n",
            l_end, l_end, l_loop, l_end);
        ptx_buf_str(&ctx->body, mg_buf);
        ptx_buf_str(&ctx->body,
            "        mad.lo.u32 %mg_ia0, %mg_ar0, %mg_K, %mg_k; add.u32 %mg_ia0, %mg_ia0, %mg_t2;\n"
            "        mad.lo.u32 %mg_ia1, %mg_ar1, %mg_K, %mg_k; add.u32 %mg_ia1, %mg_ia1, %mg_t2;\n"
            "        add.u32 %mg_ia2, %mg_ia0, 8;\n"
            "        add.u32 %mg_ia3, %mg_ia1, 8;\n"
            "        mul.wide.u32 %mg_off, %mg_ia0, 2; add.u64 %mg_ptr, %mg_ra, %mg_off; ld.u32 %mg_a0, [%mg_ptr];\n"
            "        mul.wide.u32 %mg_off, %mg_ia1, 2; add.u64 %mg_ptr, %mg_ra, %mg_off; ld.u32 %mg_a1, [%mg_ptr];\n"
            "        mul.wide.u32 %mg_off, %mg_ia2, 2; add.u64 %mg_ptr, %mg_ra, %mg_off; ld.u32 %mg_a2, [%mg_ptr];\n"
            "        mul.wide.u32 %mg_off, %mg_ia3, 2; add.u64 %mg_ptr, %mg_ra, %mg_off; ld.u32 %mg_a3, [%mg_ptr];\n"
            "        mad.lo.u32 %mg_ib0, %mg_bn, %mg_K, %mg_k; add.u32 %mg_ib0, %mg_ib0, %mg_t2;\n"
            "        add.u32 %mg_ib1, %mg_ib0, 8;\n"
            "        mul.wide.u32 %mg_off, %mg_ib0, 2; add.u64 %mg_ptr, %mg_rb, %mg_off; ld.u32 %mg_b0, [%mg_ptr];\n"
            "        mul.wide.u32 %mg_off, %mg_ib1, 2; add.u64 %mg_ptr, %mg_rb, %mg_off; ld.u32 %mg_b1, [%mg_ptr];\n"
            "        mma.sync.aligned.m16n8k16.row.col.f32.f16.f16.f32\n"
            "            {%mg_d0, %mg_d1, %mg_d2, %mg_d3},\n"
            "            {%mg_a0, %mg_a1, %mg_a2, %mg_a3},\n"
            "            {%mg_b0, %mg_b1},\n"
            "            {%mg_d0, %mg_d1, %mg_d2, %mg_d3};\n"
            "        mov.u32 %mg_k, %mg_k16;\n");
        snprintf(mg_buf, sizeof(mg_buf),
            "        bra L_mma_k_%d;\n"
            "L_mma_kd_%d:\n"
            "        add.u32 %%mg_r0, %%mg_row0, %%mg_g;\n"
            "        add.u32 %%mg_r1, %%mg_r0, 8;\n"
            "        add.u32 %%mg_c0, %%mg_col0, %%mg_t2;\n"
            "        add.u32 %%mg_c1, %%mg_c0, 1;\n"
            "        setp.lt.u32 %%mg_p, %%mg_r0, %%mg_M; setp.lt.u32 %%mg_q, %%mg_c0, %%mg_N; and.pred %%mg_q, %%mg_p, %%mg_q;\n"
            "        @%%mg_q mad.lo.u32 %%mg_di, %%mg_r0, %%mg_N, %%mg_c0;\n"
            "        @%%mg_q mul.wide.u32 %%mg_off, %%mg_di, 4; @%%mg_q add.u64 %%mg_ptr, %%mg_rd, %%mg_off; @%%mg_q st.f32 [%%mg_ptr], %%mg_d0;\n"
            "        setp.lt.u32 %%mg_q, %%mg_c1, %%mg_N; and.pred %%mg_q, %%mg_p, %%mg_q;\n"
            "        @%%mg_q mad.lo.u32 %%mg_di, %%mg_r0, %%mg_N, %%mg_c1;\n"
            "        @%%mg_q mul.wide.u32 %%mg_off, %%mg_di, 4; @%%mg_q add.u64 %%mg_ptr, %%mg_rd, %%mg_off; @%%mg_q st.f32 [%%mg_ptr], %%mg_d1;\n"
            "        setp.lt.u32 %%mg_p, %%mg_r1, %%mg_M; setp.lt.u32 %%mg_q, %%mg_c0, %%mg_N; and.pred %%mg_q, %%mg_p, %%mg_q;\n"
            "        @%%mg_q mad.lo.u32 %%mg_di, %%mg_r1, %%mg_N, %%mg_c0;\n"
            "        @%%mg_q mul.wide.u32 %%mg_off, %%mg_di, 4; @%%mg_q add.u64 %%mg_ptr, %%mg_rd, %%mg_off; @%%mg_q st.f32 [%%mg_ptr], %%mg_d2;\n"
            "        setp.lt.u32 %%mg_q, %%mg_c1, %%mg_N; and.pred %%mg_q, %%mg_p, %%mg_q;\n"
            "        @%%mg_q mad.lo.u32 %%mg_di, %%mg_r1, %%mg_N, %%mg_c1;\n"
            "        @%%mg_q mul.wide.u32 %%mg_off, %%mg_di, 4; @%%mg_q add.u64 %%mg_ptr, %%mg_rd, %%mg_off; @%%mg_q st.f32 [%%mg_ptr], %%mg_d3;\n"
            "L_mma_e_%d:\n"
            "    }\n",
            l_loop, l_end, l_end);
        ptx_buf_str(&ctx->body, mg_buf);
        return res;
    }

    return res;
}

static PtxVal ptx_emit_binop(PtxCtx *ctx, int op, Type *res_type, PtxVal lv, PtxVal rv) {
    char line[256], dname[32], lbuf[32], rbuf[32];

    if (op == ND_SHL || op == ND_SHR) {
        int lcl = (lv.cl == PTX_CL_I64) ? PTX_CL_I64 : PTX_CL_I32;
        int luns = ptx_is_uns(lv.type);
        PtxVal lc = ptx_emit_conv_cl(ctx, lv, lcl, luns);
        PtxVal rc = ptx_emit_conv_cl(ctx, rv, PTX_CL_I32, 1);
        PtxVal d;
        d.cl = lcl; d.id = ptx_alloc_reg(ctx, lcl); d.type = res_type ? res_type : lc.type;
        ptx_reg_name(lcl, d.id, dname, sizeof(dname));
        ptx_reg_name(lcl, lc.id, lbuf, sizeof(lbuf));
        ptx_reg_name(PTX_CL_I32, rc.id, rbuf, sizeof(rbuf));
        if (op == ND_SHL) {
            snprintf(line, sizeof(line), "    shl.b%d %s, %s, %s;\n", (lcl == PTX_CL_I64) ? 64 : 32, dname, lbuf, rbuf);
        } else {
            snprintf(line, sizeof(line), "    shr.%c%d %s, %s, %s;\n", luns ? 'u' : 's', (lcl == PTX_CL_I64) ? 64 : 32, dname, lbuf, rbuf);
        }
        ptx_buf_str(&ctx->body, line);
        return d;
    }

    if (op == ND_EQ || op == ND_NE || op == ND_LT || op == ND_LE || op == ND_GT || op == ND_GE) {
        int cl = ptx_promoted_cl(lv.cl, rv.cl);
        int uns = (ptx_is_uns(lv.type) || ptx_is_uns(rv.type)) ? 1 : 0;
        PtxVal lc = ptx_emit_conv_cl(ctx, lv, cl, uns);
        PtxVal rc = ptx_emit_conv_cl(ctx, rv, cl, uns);
        int p = ptx_alloc_pred(ctx);
        PtxVal d;
        d.cl = PTX_CL_I32; d.id = ptx_alloc_reg(ctx, PTX_CL_I32); d.type = ctx->cc->ty_int;
        ptx_reg_name(cl, lc.id, lbuf, sizeof(lbuf));
        ptx_reg_name(cl, rc.id, rbuf, sizeof(rbuf));
        ptx_reg_name(PTX_CL_I32, d.id, dname, sizeof(dname));
        const char *cmp = "eq";
        if (op == ND_NE) cmp = (cl == PTX_CL_F32 || cl == PTX_CL_F64) ? "neu" : "ne";
        else if (op == ND_LT) cmp = "lt";
        else if (op == ND_LE) cmp = "le";
        else if (op == ND_GT) cmp = "gt";
        else if (op == ND_GE) cmp = "ge";
        snprintf(line, sizeof(line), "    setp.%s%s %%p%d, %s, %s;\n    selp.u32 %s, 1, 0, %%p%d;\n",
                 cmp, ptx_cmp_ty(cl, uns), p, lbuf, rbuf, dname, p);
        ptx_buf_str(&ctx->body, line);
        return d;
    }

    int cl = ptx_promoted_cl(lv.cl, rv.cl);
    if (op == ND_FADD || op == ND_FSUB || op == ND_FMUL || op == ND_FDIV) {
        if (cl != PTX_CL_F32 && cl != PTX_CL_F64) cl = PTX_CL_F64;
    }
    int uns = (ptx_is_uns(res_type) || ptx_is_uns(lv.type) || ptx_is_uns(rv.type)) ? 1 : 0;
    PtxVal lc = ptx_emit_conv_cl(ctx, lv, cl, uns);
    PtxVal rc = ptx_emit_conv_cl(ctx, rv, cl, uns);
    PtxVal d;
    d.cl = cl; d.id = ptx_alloc_reg(ctx, cl); d.type = res_type ? res_type : lc.type;
    ptx_reg_name(cl, d.id, dname, sizeof(dname));
    ptx_reg_name(cl, lc.id, lbuf, sizeof(lbuf));
    ptx_reg_name(cl, rc.id, rbuf, sizeof(rbuf));

    if (cl == PTX_CL_F32 || cl == PTX_CL_F64) {
        const char *fs = (cl == PTX_CL_F64) ? "f64" : "f32";
        if (op == ND_ADD || op == ND_FADD) snprintf(line, sizeof(line), "    add.%s %s, %s, %s;\n", fs, dname, lbuf, rbuf);
        else if (op == ND_SUB || op == ND_FSUB) snprintf(line, sizeof(line), "    sub.%s %s, %s, %s;\n", fs, dname, lbuf, rbuf);
        else if (op == ND_MUL || op == ND_FMUL) snprintf(line, sizeof(line), "    mul.%s %s, %s, %s;\n", fs, dname, lbuf, rbuf);
        else snprintf(line, sizeof(line), "    div.rn.%s %s, %s, %s;\n", fs, dname, lbuf, rbuf);
    } else {
        const char *bits = (cl == PTX_CL_I64) ? "64" : "32";
        const char *isuf = (cl == PTX_CL_I64) ? (uns ? "u64" : "s64") : (uns ? "u32" : "s32");
        if (op == ND_ADD) snprintf(line, sizeof(line), "    add.%s %s, %s, %s;\n", isuf, dname, lbuf, rbuf);
        else if (op == ND_SUB) snprintf(line, sizeof(line), "    sub.%s %s, %s, %s;\n", isuf, dname, lbuf, rbuf);
        else if (op == ND_MUL) snprintf(line, sizeof(line), "    mul.lo.%s %s, %s, %s;\n", isuf, dname, lbuf, rbuf);
        else if (op == ND_DIV) snprintf(line, sizeof(line), "    div.%s %s, %s, %s;\n", isuf, dname, lbuf, rbuf);
        else if (op == ND_MOD) snprintf(line, sizeof(line), "    rem.%s %s, %s, %s;\n", isuf, dname, lbuf, rbuf);
        else if (op == ND_BAND) snprintf(line, sizeof(line), "    and.b%s %s, %s, %s;\n", bits, dname, lbuf, rbuf);
        else if (op == ND_BOR) snprintf(line, sizeof(line), "    or.b%s %s, %s, %s;\n", bits, dname, lbuf, rbuf);
        else if (op == ND_BXOR) snprintf(line, sizeof(line), "    xor.b%s %s, %s, %s;\n", bits, dname, lbuf, rbuf);
        else { ctx->errors++; line[0] = '\0'; }
    }
    ptx_buf_str(&ctx->body, line);
    return d;
}

static PtxVal ptx_emit_ptr_offset(PtxCtx *ctx, PtxVal base_ptr, Node *idx_node, int elem_sz, int is_sub) {
    PtxVal idx = ptx_lower_expr(ctx, idx_node);
    PtxVal idx64 = ptx_emit_conv_cl(ctx, idx, PTX_CL_I64, ptx_is_uns(idx_node->type));
    char line[192], pname[32], iname[32], dname[32];
    ptx_reg_name(PTX_CL_I64, base_ptr.id, pname, sizeof(pname));
    ptx_reg_name(PTX_CL_I64, idx64.id, iname, sizeof(iname));
    if (elem_sz > 1) {
        int scaled = ptx_alloc_reg(ctx, PTX_CL_I64);
        char sname[32], hbuf[32];
        ptx_reg_name(PTX_CL_I64, scaled, sname, sizeof(sname));
        ptx_fmt_u64_hex(hbuf, sizeof(hbuf), (long long)elem_sz);
        snprintf(line, sizeof(line), "    mul.lo.u64 %s, %s, %s;\n", sname, iname, hbuf);
        ptx_buf_str(&ctx->body, line);
        snprintf(iname, sizeof(iname), "%s", sname);
    }
    PtxVal dst;
    dst.cl = PTX_CL_I64; dst.id = ptx_alloc_reg(ctx, PTX_CL_I64); dst.type = base_ptr.type;
    ptx_reg_name(PTX_CL_I64, dst.id, dname, sizeof(dname));
    snprintf(line, sizeof(line), "    %s.u64 %s, %s, %s;\n", is_sub ? "sub" : "add", dname, pname, iname);
    ptx_buf_str(&ctx->body, line);
    return dst;
}

static PtxVal ptx_lower_expr(PtxCtx *ctx, Node *n) {
    PtxVal zero;
    zero.cl = PTX_CL_I32; zero.id = 0; zero.type = ctx->cc->ty_int;
    if (!n) return zero;
    char line[384], dname[32], sname[32], rname[32];

    switch (n->kind) {
        case ND_NUM:
        case ND_CHAR_LIT:
        case ND_SIZEOF: {
            int cl = ptx_reg_class(n->type);
            if (cl == PTX_CL_F32 || cl == PTX_CL_F64) cl = PTX_CL_I64;
            PtxVal v;
            v.cl = cl; v.id = ptx_alloc_reg(ctx, cl); v.type = n->type ? n->type : ctx->cc->ty_int;
            ptx_reg_name(cl, v.id, dname, sizeof(dname));
            if (cl == PTX_CL_I64) {
                char hbuf[32];
                ptx_fmt_u64_hex(hbuf, sizeof(hbuf), n->int_val);
                snprintf(line, sizeof(line), "    mov.u64 %s, %s;\n", dname, hbuf);
            } else {
                snprintf(line, sizeof(line), "    mov.u32 %s, 0x%08XU;\n", dname, (unsigned int)n->int_val);
            }
            ptx_buf_str(&ctx->body, line);
            return v;
        }

        case ND_FLIT: {
            int is_f32 = (n->type && n->type->kind == TY_FLOAT) ? 1 : 0;
            int cl = is_f32 ? PTX_CL_F32 : PTX_CL_F64;
            PtxVal v;
            v.cl = cl; v.id = ptx_alloc_reg(ctx, cl); v.type = n->type ? n->type : ctx->cc->ty_double;
            ptx_reg_name(cl, v.id, dname, sizeof(dname));
            snprintf(line, sizeof(line), "    mov.%s %s, ", is_f32 ? "f32" : "f64", dname);
            ptx_buf_str(&ctx->body, line);
            if (is_f32) ptx_emit_f32_hex(&ctx->body, (float)n->f_val);
            else ptx_emit_f64_hex(&ctx->body, (double)n->f_val);
            ptx_buf_str(&ctx->body, ";\n");
            return v;
        }

        case ND_VAR: {
            PtxLocal *loc = ptx_find_local(ctx, n);
            if (n->type && (n->type->kind == TY_ARRAY || n->type->kind == TY_STRUCT || n->type->kind == TY_UNION)) {
                return ptx_lower_addr(ctx, n);
            }
            if (loc && !loc->is_memory) {
                PtxVal v;
                v.cl = loc->reg_cl; v.id = loc->reg_id; v.type = n->type ? n->type : loc->type;
                return v;
            }
            if (loc && loc->is_memory) {
                PtxVal addr = ptx_lower_addr(ctx, n);
                return ptx_emit_load(ctx, addr, n->type ? n->type : loc->type);
            }
            fprintf(stderr, "zcc ptx: unsupported external/global var '%s' (line %d)\n", ptx_var_name(n), n->line);
            ctx->errors++;
            return zero;
        }

        case ND_ADDR:
            return ptx_lower_addr(ctx, n->lhs);

        case ND_DEREF: {
            PtxVal addr = ptx_emit_conv_cl(ctx, ptx_lower_expr(ctx, n->lhs), PTX_CL_I64, 1);
            if (n->type && (n->type->kind == TY_ARRAY || n->type->kind == TY_STRUCT ||
                            n->type->kind == TY_UNION || n->type->kind == TY_FUNC)) {
                addr.type = n->type;
                return addr;
            }
            return ptx_emit_load(ctx, addr, n->type);
        }

        case ND_MEMBER: {
            PtxVal addr = ptx_lower_addr(ctx, n);
            if (n->type && (n->type->kind == TY_ARRAY || n->type->kind == TY_STRUCT || n->type->kind == TY_UNION)) {
                addr.type = n->type;
                return addr;
            }
            return ptx_emit_load(ctx, addr, n->type);
        }

        case ND_CAST: {
            PtxVal inner = ptx_lower_expr(ctx, n->lhs);
            Type *to = n->type ? n->type : n->cast_type;
            return ptx_emit_cast(ctx, inner, to);
        }

        case ND_NEG: {
            PtxVal v = ptx_lower_expr(ctx, n->lhs);
            int cl = v.cl;
            PtxVal d;
            d.cl = cl; d.id = ptx_alloc_reg(ctx, cl); d.type = n->type ? n->type : v.type;
            ptx_reg_name(cl, d.id, dname, sizeof(dname));
            ptx_reg_name(cl, v.id, sname, sizeof(sname));
            const char *ns = (cl == PTX_CL_F64) ? "f64" : (cl == PTX_CL_F32) ? "f32" : (cl == PTX_CL_I64) ? "s64" : "s32";
            snprintf(line, sizeof(line), "    neg.%s %s, %s;\n", ns, dname, sname);
            ptx_buf_str(&ctx->body, line);
            return d;
        }

        case ND_BNOT: {
            PtxVal v = ptx_lower_expr(ctx, n->lhs);
            int cl = (v.cl == PTX_CL_I64) ? PTX_CL_I64 : PTX_CL_I32;
            PtxVal d;
            d.cl = cl; d.id = ptx_alloc_reg(ctx, cl); d.type = n->type ? n->type : v.type;
            ptx_reg_name(cl, d.id, dname, sizeof(dname));
            ptx_reg_name(cl, v.id, sname, sizeof(sname));
            snprintf(line, sizeof(line), "    not.b%d %s, %s;\n", (cl == PTX_CL_I64) ? 64 : 32, dname, sname);
            ptx_buf_str(&ctx->body, line);
            return d;
        }

        case ND_LNOT: {
            PtxVal v = ptx_lower_expr(ctx, n->lhs);
            int p = ptx_emit_to_pred(ctx, v);
            PtxVal d;
            d.cl = PTX_CL_I32; d.id = ptx_alloc_reg(ctx, PTX_CL_I32); d.type = ctx->cc->ty_int;
            ptx_reg_name(PTX_CL_I32, d.id, dname, sizeof(dname));
            snprintf(line, sizeof(line), "    selp.u32 %s, 0, 1, %%p%d;\n", dname, p);
            ptx_buf_str(&ctx->body, line);
            return d;
        }

        case ND_ADD:
        case ND_FADD: {
            Type *lt = n->lhs ? n->lhs->type : NULL;
            Type *rt = n->rhs ? n->rhs->type : NULL;
            if (lt && is_pointer(lt)) {
                PtxVal base = ptx_emit_conv_cl(ctx, ptx_lower_expr(ctx, n->lhs), PTX_CL_I64, 1);
                return ptx_emit_ptr_offset(ctx, base, n->rhs, ptx_ptr_elem_sz(lt), 0);
            }
            if (rt && is_pointer(rt)) {
                PtxVal base = ptx_emit_conv_cl(ctx, ptx_lower_expr(ctx, n->rhs), PTX_CL_I64, 1);
                return ptx_emit_ptr_offset(ctx, base, n->lhs, ptx_ptr_elem_sz(rt), 0);
            }
            PtxVal lv = ptx_lower_expr(ctx, n->lhs);
            PtxVal rv = ptx_lower_expr(ctx, n->rhs);
            return ptx_emit_binop(ctx, n->kind, n->type, lv, rv);
        }

        case ND_SUB:
        case ND_FSUB: {
            Type *lt = n->lhs ? n->lhs->type : NULL;
            Type *rt = n->rhs ? n->rhs->type : NULL;
            if (lt && is_pointer(lt) && rt && is_pointer(rt)) {
                PtxVal lv = ptx_emit_conv_cl(ctx, ptx_lower_expr(ctx, n->lhs), PTX_CL_I64, 1);
                PtxVal rv = ptx_emit_conv_cl(ctx, ptx_lower_expr(ctx, n->rhs), PTX_CL_I64, 1);
                int diff = ptx_alloc_reg(ctx, PTX_CL_I64);
                ptx_reg_name(PTX_CL_I64, diff, dname, sizeof(dname));
                ptx_reg_name(PTX_CL_I64, lv.id, sname, sizeof(sname));
                ptx_reg_name(PTX_CL_I64, rv.id, rname, sizeof(rname));
                snprintf(line, sizeof(line), "    sub.s64 %s, %s, %s;\n", dname, sname, rname);
                ptx_buf_str(&ctx->body, line);
                int esz = ptx_ptr_elem_sz(lt);
                if (esz > 1) {
                    int q = ptx_alloc_reg(ctx, PTX_CL_I64);
                    char qname[32], hbuf[32];
                    ptx_reg_name(PTX_CL_I64, q, qname, sizeof(qname));
                    ptx_fmt_u64_hex(hbuf, sizeof(hbuf), (long long)esz);
                    snprintf(line, sizeof(line), "    div.s64 %s, %s, %s;\n", qname, dname, hbuf);
                    ptx_buf_str(&ctx->body, line);
                    diff = q;
                }
                PtxVal res;
                res.cl = PTX_CL_I64; res.id = diff; res.type = n->type ? n->type : ctx->cc->ty_longlong;
                return ptx_emit_cast(ctx, res, res.type);
            }
            if (lt && is_pointer(lt)) {
                PtxVal base = ptx_emit_conv_cl(ctx, ptx_lower_expr(ctx, n->lhs), PTX_CL_I64, 1);
                return ptx_emit_ptr_offset(ctx, base, n->rhs, ptx_ptr_elem_sz(lt), 1);
            }
            PtxVal lv = ptx_lower_expr(ctx, n->lhs);
            PtxVal rv = ptx_lower_expr(ctx, n->rhs);
            return ptx_emit_binop(ctx, n->kind, n->type, lv, rv);
        }

        case ND_MUL: case ND_FMUL:
        case ND_DIV: case ND_FDIV:
        case ND_MOD:
        case ND_BAND: case ND_BOR: case ND_BXOR:
        case ND_SHL: case ND_SHR:
        case ND_EQ: case ND_NE: case ND_LT: case ND_LE: case ND_GT: case ND_GE: {
            PtxVal lv = ptx_lower_expr(ctx, n->lhs);
            PtxVal rv = ptx_lower_expr(ctx, n->rhs);
            return ptx_emit_binop(ctx, n->kind, n->type, lv, rv);
        }

        case ND_LAND:
        case ND_LOR: {
            int l_short = ctx->label_seq++;
            int l_end = ctx->label_seq++;
            int is_and = (n->kind == ND_LAND);
            PtxVal d;
            d.cl = PTX_CL_I32; d.id = ptx_alloc_reg(ctx, PTX_CL_I32); d.type = ctx->cc->ty_int;
            ptx_reg_name(PTX_CL_I32, d.id, dname, sizeof(dname));
            PtxVal lv = ptx_lower_expr(ctx, n->lhs);
            int p1 = ptx_emit_to_pred(ctx, lv);
            snprintf(line, sizeof(line), "    @%s%%p%d bra L_sc_%d;\n", is_and ? "!" : "", p1, l_short);
            ptx_buf_str(&ctx->body, line);
            PtxVal rv = ptx_lower_expr(ctx, n->rhs);
            int p2 = ptx_emit_to_pred(ctx, rv);
            snprintf(line, sizeof(line),
                     "    selp.u32 %s, 1, 0, %%p%d;\n    bra L_sce_%d;\n"
                     "L_sc_%d:\n    mov.u32 %s, %d;\nL_sce_%d:\n",
                     dname, p2, l_end, l_short, dname, is_and ? 0 : 1, l_end);
            ptx_buf_str(&ctx->body, line);
            return d;
        }

        case ND_TERNARY: {
            int l_else = ctx->label_seq++;
            int l_end = ctx->label_seq++;
            int rcl = ptx_reg_class(n->type);
            if (rcl == PTX_CL_VOID) {
                PtxVal cv = ptx_lower_expr(ctx, n->cond);
                int p = ptx_emit_to_pred(ctx, cv);
                snprintf(line, sizeof(line), "    @!%%p%d bra L_te_%d;\n", p, l_else);
                ptx_buf_str(&ctx->body, line);
                ptx_lower_expr(ctx, n->then_body);
                snprintf(line, sizeof(line), "    bra L_td_%d;\nL_te_%d:\n", l_end, l_else);
                ptx_buf_str(&ctx->body, line);
                ptx_lower_expr(ctx, n->else_body);
                snprintf(line, sizeof(line), "L_td_%d:\n", l_end);
                ptx_buf_str(&ctx->body, line);
                return zero;
            }
            PtxVal d;
            d.cl = rcl; d.id = ptx_alloc_reg(ctx, rcl); d.type = n->type;
            ptx_reg_name(rcl, d.id, dname, sizeof(dname));
            PtxVal cv = ptx_lower_expr(ctx, n->cond);
            int p = ptx_emit_to_pred(ctx, cv);
            snprintf(line, sizeof(line), "    @!%%p%d bra L_te_%d;\n", p, l_else);
            ptx_buf_str(&ctx->body, line);
            PtxVal tv = ptx_emit_cast(ctx, ptx_lower_expr(ctx, n->then_body), n->type);
            ptx_reg_name(rcl, tv.id, sname, sizeof(sname));
            const char *ms = (rcl == PTX_CL_F64) ? "f64" : (rcl == PTX_CL_F32) ? "f32" : (rcl == PTX_CL_I64) ? "u64" : "u32";
            snprintf(line, sizeof(line), "    mov.%s %s, %s;\n    bra L_td_%d;\nL_te_%d:\n", ms, dname, sname, l_end, l_else);
            ptx_buf_str(&ctx->body, line);
            PtxVal ev = ptx_emit_cast(ctx, ptx_lower_expr(ctx, n->else_body), n->type);
            ptx_reg_name(rcl, ev.id, rname, sizeof(rname));
            snprintf(line, sizeof(line), "    mov.%s %s, %s;\nL_td_%d:\n", ms, dname, rname, l_end);
            ptx_buf_str(&ctx->body, line);
            return d;
        }

        case ND_COMMA_EXPR:
            ptx_lower_expr(ctx, n->lhs);
            return ptx_lower_expr(ctx, n->rhs);

        case ND_ASSIGN: {
            PtxVal rhs = ptx_lower_expr(ctx, n->rhs);
            if (n->lhs && n->lhs->kind == ND_VAR) {
                PtxLocal *loc = ptx_find_local(ctx, n->lhs);
                if (loc && !loc->is_memory) {
                    PtxVal cv = ptx_emit_cast(ctx, rhs, n->lhs->type ? n->lhs->type : loc->type);
                    ptx_reg_name(loc->reg_cl, loc->reg_id, dname, sizeof(dname));
                    ptx_reg_name(cv.cl, cv.id, sname, sizeof(sname));
                    const char *ms = (loc->reg_cl == PTX_CL_F64) ? "f64" : (loc->reg_cl == PTX_CL_F32) ? "f32" :
                                     (loc->reg_cl == PTX_CL_I64) ? "u64" : "u32";
                    snprintf(line, sizeof(line), "    mov.%s %s, %s;\n", ms, dname, sname);
                    ptx_buf_str(&ctx->body, line);
                    return cv;
                }
            }
            PtxVal addr = ptx_lower_addr(ctx, n->lhs);
            Type *lt = n->lhs ? n->lhs->type : ctx->cc->ty_int;
            ptx_emit_store(ctx, addr, lt, rhs);
            return ptx_emit_cast(ctx, rhs, lt);
        }

        case ND_COMPOUND_ASSIGN: {
            Type *lt = (n->lhs && n->lhs->type) ? n->lhs->type : ctx->cc->ty_int;
            int cop = n->compound_op;
            if (n->lhs && n->lhs->kind == ND_VAR) {
                PtxLocal *loc = ptx_find_local(ctx, n->lhs);
                if (loc && !loc->is_memory) {
                    PtxVal oldv;
                    oldv.cl = loc->reg_cl; oldv.id = loc->reg_id; oldv.type = lt;
                    PtxVal nv = (is_pointer(lt) && (cop == ND_ADD || cop == ND_SUB))
                        ? ptx_emit_ptr_offset(ctx, oldv, n->rhs, ptx_ptr_elem_sz(lt), cop == ND_SUB)
                        : ptx_emit_binop(ctx, cop, lt, oldv, ptx_lower_expr(ctx, n->rhs));
                    PtxVal cv = ptx_emit_cast(ctx, nv, lt);
                    ptx_reg_name(loc->reg_cl, loc->reg_id, dname, sizeof(dname));
                    ptx_reg_name(cv.cl, cv.id, sname, sizeof(sname));
                    const char *ms = (loc->reg_cl == PTX_CL_F64) ? "f64" : (loc->reg_cl == PTX_CL_F32) ? "f32" :
                                     (loc->reg_cl == PTX_CL_I64) ? "u64" : "u32";
                    snprintf(line, sizeof(line), "    mov.%s %s, %s;\n", ms, dname, sname);
                    ptx_buf_str(&ctx->body, line);
                    return cv;
                }
            }
            PtxVal addr = ptx_lower_addr(ctx, n->lhs);
            PtxVal oldv = ptx_emit_load(ctx, addr, lt);
            PtxVal nv = (is_pointer(lt) && (cop == ND_ADD || cop == ND_SUB))
                ? ptx_emit_ptr_offset(ctx, oldv, n->rhs, ptx_ptr_elem_sz(lt), cop == ND_SUB)
                : ptx_emit_binop(ctx, cop, lt, oldv, ptx_lower_expr(ctx, n->rhs));
            PtxVal cv = ptx_emit_cast(ctx, nv, lt);
            ptx_emit_store(ctx, addr, lt, cv);
            return cv;
        }

        case ND_PRE_INC:
        case ND_PRE_DEC:
        case ND_POST_INC:
        case ND_POST_DEC: {
            int is_inc = (n->kind == ND_PRE_INC || n->kind == ND_POST_INC);
            int is_post = (n->kind == ND_POST_INC || n->kind == ND_POST_DEC);
            Type *lt = (n->lhs && n->lhs->type) ? n->lhs->type : ctx->cc->ty_int;
            int cl = ptx_reg_class(lt);
            int step = is_pointer(lt) ? ptx_ptr_elem_sz(lt) : 1;

            PtxVal addr;
            addr.cl = PTX_CL_I64; addr.id = 0; addr.type = ctx->cc->ty_ulonglong;
            PtxLocal *loc = (n->lhs && n->lhs->kind == ND_VAR) ? ptx_find_local(ctx, n->lhs) : NULL;
            PtxVal oldv;
            if (loc && !loc->is_memory) {
                oldv.cl = loc->reg_cl; oldv.id = loc->reg_id; oldv.type = lt;
            } else {
                addr = ptx_lower_addr(ctx, n->lhs);
                oldv = ptx_emit_load(ctx, addr, lt);
            }

            PtxVal saved = oldv;
            if (is_post) {
                saved.id = ptx_alloc_reg(ctx, cl);
                ptx_reg_name(cl, saved.id, dname, sizeof(dname));
                ptx_reg_name(cl, oldv.id, sname, sizeof(sname));
                const char *ms = (cl == PTX_CL_F64) ? "f64" : (cl == PTX_CL_F32) ? "f32" : (cl == PTX_CL_I64) ? "u64" : "u32";
                snprintf(line, sizeof(line), "    mov.%s %s, %s;\n", ms, dname, sname);
                ptx_buf_str(&ctx->body, line);
            }

            PtxVal one;
            one.cl = cl; one.id = ptx_alloc_reg(ctx, cl); one.type = lt;
            ptx_reg_name(cl, one.id, rname, sizeof(rname));
            if (cl == PTX_CL_F32) snprintf(line, sizeof(line), "    mov.f32 %s, 0f3F800000;\n", rname);
            else if (cl == PTX_CL_F64) snprintf(line, sizeof(line), "    mov.f64 %s, 0d3FF0000000000000;\n", rname);
            else if (cl == PTX_CL_I64) snprintf(line, sizeof(line), "    mov.u64 %s, %d;\n", rname, step);
            else snprintf(line, sizeof(line), "    mov.u32 %s, %d;\n", rname, step);
            ptx_buf_str(&ctx->body, line);

            PtxVal nv = ptx_emit_cast(ctx, ptx_emit_binop(ctx, is_inc ? ND_ADD : ND_SUB, lt, oldv, one), lt);
            if (loc && !loc->is_memory) {
                ptx_reg_name(loc->reg_cl, loc->reg_id, dname, sizeof(dname));
                ptx_reg_name(nv.cl, nv.id, sname, sizeof(sname));
                const char *ms = (cl == PTX_CL_F64) ? "f64" : (cl == PTX_CL_F32) ? "f32" : (cl == PTX_CL_I64) ? "u64" : "u32";
                snprintf(line, sizeof(line), "    mov.%s %s, %s;\n", ms, dname, sname);
                ptx_buf_str(&ctx->body, line);
            } else {
                ptx_emit_store(ctx, addr, lt, nv);
            }
            return is_post ? saved : nv;
        }

        case ND_CALL: {
            int handled = 0;
            PtxVal bres = ptx_emit_builtin(ctx, n, &handled);
            if (handled) return bres;

            PtxFuncSig *sig = n->func_name[0] ? ptx_find_sig(ctx, n->func_name) : NULL;
            if (!sig || sig->is_kernel) {
                fprintf(stderr, "zcc ptx: unknown or kernel call '%s' inside PTX (line %d)\n",
                        n->func_name[0] ? n->func_name : "<indirect>", n->line);
                ctx->errors++;
                return zero;
            }

            PtxVal *avals = (PtxVal *)calloc((size_t)(sig->num_params + 1), sizeof(PtxVal));
            if (!avals) return zero;
            for (int a = 0; a < sig->num_params && a < n->num_args; a++) {
                PtxVal av = ptx_lower_expr(ctx, n->args[a]);
                avals[a] = ptx_emit_cast(ctx, av, sig->param_types[a]);
            }

            int ret_cl = ptx_reg_class(sig->ret_type);
            PtxVal retv;
            retv.cl = ret_cl;
            retv.id = (ret_cl != PTX_CL_VOID) ? ptx_alloc_reg(ctx, ret_cl) : 0;
            retv.type = sig->ret_type;

            ptx_buf_str(&ctx->body, "    {\n");
            for (int a = 0; a < sig->num_params; a++) {
                snprintf(line, sizeof(line), "        .param %s _ca%d;\n", ptx_param_ty(sig->param_types[a]), a);
                ptx_buf_str(&ctx->body, line);
                ptx_reg_name(avals[a].cl, avals[a].id, sname, sizeof(sname));
                snprintf(line, sizeof(line), "        st.param%s [_ca%d+0], %s;\n",
                         ptx_param_ty(sig->param_types[a]), a, sname);
                ptx_buf_str(&ctx->body, line);
            }
            if (ret_cl != PTX_CL_VOID) {
                snprintf(line, sizeof(line), "        .param %s _cret;\n        call.uni (_cret), %s, (",
                         ptx_param_ty(sig->ret_type), sig->name);
            } else {
                snprintf(line, sizeof(line), "        call.uni %s, (", sig->name);
            }
            ptx_buf_str(&ctx->body, line);
            for (int a = 0; a < sig->num_params; a++) {
                snprintf(line, sizeof(line), "%s_ca%d", (a > 0) ? ", " : "", a);
                ptx_buf_str(&ctx->body, line);
            }
            ptx_buf_str(&ctx->body, ");\n");
            if (ret_cl != PTX_CL_VOID) {
                ptx_reg_name(ret_cl, retv.id, dname, sizeof(dname));
                snprintf(line, sizeof(line), "        ld.param%s %s, [_cret+0];\n", ptx_param_ty(sig->ret_type), dname);
                ptx_buf_str(&ctx->body, line);
            }
            ptx_buf_str(&ctx->body, "    }\n");
            free(avals);
            return retv;
        }

        default:
            fprintf(stderr, "zcc ptx: unsupported expr kind %d (line %d)\n", n->kind, n->line);
            ctx->errors++;
            return zero;
    }
}

static void ptx_lower_stmt(PtxCtx *ctx, Node *n) {
    if (!n) return;
    char line[256], sbuf[MAX_IDENT + 32], rname[32], cname[32];

    switch (n->kind) {
        case ND_NOP:
        case ND_VAR:
            break;

        case ND_BLOCK:
            for (int i = 0; i < n->num_stmts; i++) ptx_lower_stmt(ctx, n->stmts[i]);
            break;

        case ND_IF: {
            int l_else = ctx->label_seq++;
            int l_end = ctx->label_seq++;
            PtxVal cv = ptx_lower_expr(ctx, n->cond);
            int p = ptx_emit_to_pred(ctx, cv);
            if (n->else_body) {
                snprintf(line, sizeof(line), "    @!%%p%d bra L_if_e_%d;\n", p, l_else);
            } else {
                snprintf(line, sizeof(line), "    @!%%p%d bra L_if_d_%d;\n", p, l_end);
            }
            ptx_buf_str(&ctx->body, line);
            ptx_lower_stmt(ctx, n->then_body);
            if (n->else_body) {
                snprintf(line, sizeof(line), "    bra L_if_d_%d;\nL_if_e_%d:\n", l_end, l_else);
                ptx_buf_str(&ctx->body, line);
                ptx_lower_stmt(ctx, n->else_body);
            }
            snprintf(line, sizeof(line), "L_if_d_%d:\n", l_end);
            ptx_buf_str(&ctx->body, line);
            break;
        }

        case ND_WHILE: {
            int l_top = ctx->label_seq++;
            int l_end = ctx->label_seq++;
            if (ctx->loop_depth < 64) {
                ctx->break_lbls[ctx->loop_depth] = l_end;
                ctx->cont_lbls[ctx->loop_depth] = l_top;
                ctx->loop_depth++;
            }
            snprintf(line, sizeof(line), "L_ct_%d:\n", l_top);
            ptx_buf_str(&ctx->body, line);
            if (n->cond) {
                PtxVal cv = ptx_lower_expr(ctx, n->cond);
                int p = ptx_emit_to_pred(ctx, cv);
                snprintf(line, sizeof(line), "    @!%%p%d bra L_br_%d;\n", p, l_end);
                ptx_buf_str(&ctx->body, line);
            }
            ptx_lower_stmt(ctx, n->body);
            snprintf(line, sizeof(line), "    bra L_ct_%d;\nL_br_%d:\n", l_top, l_end);
            ptx_buf_str(&ctx->body, line);
            if (ctx->loop_depth > 0) ctx->loop_depth--;
            break;
        }

        case ND_DO_WHILE: {
            int l_top = ctx->label_seq++;
            int l_cont = ctx->label_seq++;
            int l_end = ctx->label_seq++;
            if (ctx->loop_depth < 64) {
                ctx->break_lbls[ctx->loop_depth] = l_end;
                ctx->cont_lbls[ctx->loop_depth] = l_cont;
                ctx->loop_depth++;
            }
            snprintf(line, sizeof(line), "L_dw_%d:\n", l_top);
            ptx_buf_str(&ctx->body, line);
            ptx_lower_stmt(ctx, n->body);
            snprintf(line, sizeof(line), "L_ct_%d:\n", l_cont);
            ptx_buf_str(&ctx->body, line);
            PtxVal cv = ptx_lower_expr(ctx, n->cond);
            int p = ptx_emit_to_pred(ctx, cv);
            snprintf(line, sizeof(line), "    @%%p%d bra L_dw_%d;\nL_br_%d:\n", p, l_top, l_end);
            ptx_buf_str(&ctx->body, line);
            if (ctx->loop_depth > 0) ctx->loop_depth--;
            break;
        }

        case ND_FOR: {
            int l_top = ctx->label_seq++;
            int l_cont = ctx->label_seq++;
            int l_end = ctx->label_seq++;
            if (n->init) ptx_lower_stmt(ctx, n->init);
            if (ctx->loop_depth < 64) {
                ctx->break_lbls[ctx->loop_depth] = l_end;
                ctx->cont_lbls[ctx->loop_depth] = l_cont;
                ctx->loop_depth++;
            }
            snprintf(line, sizeof(line), "L_f_%d:\n", l_top);
            ptx_buf_str(&ctx->body, line);
            if (n->cond) {
                PtxVal cv = ptx_lower_expr(ctx, n->cond);
                int p = ptx_emit_to_pred(ctx, cv);
                snprintf(line, sizeof(line), "    @!%%p%d bra L_br_%d;\n", p, l_end);
                ptx_buf_str(&ctx->body, line);
            }
            ptx_lower_stmt(ctx, n->body);
            snprintf(line, sizeof(line), "L_ct_%d:\n", l_cont);
            ptx_buf_str(&ctx->body, line);
            if (n->inc) ptx_lower_expr(ctx, n->inc);
            snprintf(line, sizeof(line), "    bra L_f_%d;\nL_br_%d:\n", l_top, l_end);
            ptx_buf_str(&ctx->body, line);
            if (ctx->loop_depth > 0) ctx->loop_depth--;
            break;
        }

        case ND_SWITCH: {
            int l_end = ctx->label_seq++;
            int prev_cont = (ctx->loop_depth > 0) ? ctx->cont_lbls[ctx->loop_depth - 1] : -1;
            if (ctx->loop_depth < 64) {
                ctx->break_lbls[ctx->loop_depth] = l_end;
                ctx->cont_lbls[ctx->loop_depth] = prev_cont;
                ctx->loop_depth++;
            }
            PtxVal cv = ptx_lower_expr(ctx, n->cond);
            int c_cl = (cv.cl == PTX_CL_I64) ? PTX_CL_I64 : PTX_CL_I32;
            int c_uns = n->cond ? ptx_is_uns(n->cond->type) : 0;
            cv = ptx_emit_conv_cl(ctx, cv, c_cl, c_uns);
            ptx_reg_name(c_cl, cv.id, rname, sizeof(rname));

            for (int i = 0; i < n->num_cases; i++) {
                Node *cn = n->cases[i];
                if (!cn || !cn->case_body) continue;
                int p = ptx_alloc_pred(ctx);
                ptx_safe_ident(cn->case_body->label_name, sbuf, sizeof(sbuf));
                if (c_cl == PTX_CL_I64) {
                    char hbuf[32];
                    ptx_fmt_u64_hex(hbuf, sizeof(hbuf), cn->case_val);
                    snprintf(cname, sizeof(cname), "%s", hbuf);
                } else {
                    snprintf(cname, sizeof(cname), "0x%08XU", (unsigned int)cn->case_val);
                }
                snprintf(line, sizeof(line), "    setp.eq.u%d %%p%d, %s, %s;\n    @%%p%d bra L_u_%d_%s;\n",
                         (c_cl == PTX_CL_I64) ? 64 : 32, p, rname, cname, p, ctx->cur_fn_idx, sbuf);
                ptx_buf_str(&ctx->body, line);
            }
            if (n->default_case && n->default_case->case_body) {
                ptx_safe_ident(n->default_case->case_body->label_name, sbuf, sizeof(sbuf));
                snprintf(line, sizeof(line), "    bra L_u_%d_%s;\n", ctx->cur_fn_idx, sbuf);
            } else {
                snprintf(line, sizeof(line), "    bra L_br_%d;\n", l_end);
            }
            ptx_buf_str(&ctx->body, line);
            ptx_lower_stmt(ctx, n->body);
            snprintf(line, sizeof(line), "L_br_%d:\n", l_end);
            ptx_buf_str(&ctx->body, line);
            if (ctx->loop_depth > 0) ctx->loop_depth--;
            break;
        }

        case ND_BREAK:
            if (ctx->loop_depth > 0) {
                snprintf(line, sizeof(line), "    bra L_br_%d;\n", ctx->break_lbls[ctx->loop_depth - 1]);
                ptx_buf_str(&ctx->body, line);
            }
            break;

        case ND_CONTINUE:
            if (ctx->loop_depth > 0 && ctx->cont_lbls[ctx->loop_depth - 1] >= 0) {
                snprintf(line, sizeof(line), "    bra L_ct_%d;\n", ctx->cont_lbls[ctx->loop_depth - 1]);
                ptx_buf_str(&ctx->body, line);
            }
            break;

        case ND_LABEL:
            ptx_safe_ident(n->label_name, sbuf, sizeof(sbuf));
            snprintf(line, sizeof(line), "L_u_%d_%s:\n", ctx->cur_fn_idx, sbuf);
            ptx_buf_str(&ctx->body, line);
            if (n->lhs) ptx_lower_stmt(ctx, n->lhs);
            break;

        case ND_GOTO:
            ptx_safe_ident(n->label_name, sbuf, sizeof(sbuf));
            snprintf(line, sizeof(line), "    bra L_u_%d_%s;\n", ctx->cur_fn_idx, sbuf);
            ptx_buf_str(&ctx->body, line);
            break;

        case ND_RETURN:
            if (n->lhs && ctx->ret_cl != PTX_CL_VOID) {
                PtxVal rv = ptx_emit_cast(ctx, ptx_lower_expr(ctx, n->lhs), ctx->ret_type);
                ptx_reg_name(rv.cl, rv.id, rname, sizeof(rname));
                snprintf(line, sizeof(line), "    st.param%s [_ret+0], %s;\n    ret;\n",
                         ptx_param_ty(ctx->ret_type), rname);
                ptx_buf_str(&ctx->body, line);
            } else {
                if (n->lhs) ptx_lower_expr(ctx, n->lhs);
                ptx_buf_str(&ctx->body, "    ret;\n");
            }
            break;

        default:
            ptx_lower_expr(ctx, n);
            break;
    }
}

static int ptx_emit_function(PtxCtx *ctx, Node *fn, FILE *out, int fn_idx) {
    char line[384], rname[32], fpname[32];
    ctx->cur_fn_idx = fn_idx;
    ctx->num_locals = 0;
    ctx->cnt_pred = 0;
    ctx->cnt_i32 = 0;
    ctx->cnt_i64 = 0;
    ctx->cnt_f32 = 0;
    ctx->cnt_f64 = 0;
    ctx->local_frame_sz = 0;
    ctx->fp_reg = -1;
    ctx->loop_depth = 0;
    ctx->hdr.len = 0;
    if (ctx->hdr.data) ctx->hdr.data[0] = '\0';
    ctx->body.len = 0;
    if (ctx->body.data) ctx->body.data[0] = '\0';

    Type *ret_t = (fn->func_type && fn->func_type->ret) ? fn->func_type->ret : ctx->cc->ty_void;
    ctx->ret_type = ret_t;
    ctx->ret_cl = ptx_reg_class(ret_t);
    ctx->is_kernel = (!fn->is_static && ret_t->kind == TY_VOID) ? 1 : 0;

    if (fn->func_params) {
        for (int p = 0; p < fn->num_params && p < MAX_PARAMS; p++) {
            const char *pname = fn->func_params->names[p];
            Type *ptype = fn->func_params->types ? fn->func_params->types[p] : ctx->cc->ty_int;
            Symbol *psym = fn->func_params->syms ? fn->func_params->syms[p] : NULL;
            if (ctx->num_locals >= ctx->local_cap) {
                int ncap = ctx->local_cap ? ctx->local_cap * 2 : 64;
                PtxLocal *nl = (PtxLocal *)realloc(ctx->locals, (size_t)ncap * sizeof(PtxLocal));
                if (!nl) return -1;
                ctx->locals = nl;
                ctx->local_cap = ncap;
            }
            PtxLocal *loc = &ctx->locals[ctx->num_locals++];
            memset(loc, 0, sizeof(PtxLocal));
            if (pname && pname[0]) strncpy(loc->name, pname, MAX_IDENT - 1);
            else snprintf(loc->name, MAX_IDENT, "_arg_%d", p);
            loc->sym = psym;
            loc->type = ptype ? ptype : ctx->cc->ty_int;
            loc->reg_cl = ptx_reg_class(loc->type);
            loc->is_param = 1;
        }
    }
    int num_param_locals = ctx->num_locals;

    if (fn->body) ptx_scan_locals(ctx, fn->body);

    for (int i = 0; i < ctx->num_locals; i++) {
        if (ctx->locals[i].is_memory) {
            int sz = ctx->locals[i].size > 0 ? ctx->locals[i].size : 4;
            int al = (sz >= 8) ? 8 : (sz >= 4 ? 4 : 1);
            ctx->local_frame_sz = (ctx->local_frame_sz + (al - 1)) & ~(al - 1);
            ctx->locals[i].stack_off = ctx->local_frame_sz;
            ctx->local_frame_sz += sz;
        } else {
            ctx->locals[i].reg_id = ptx_alloc_reg(ctx, ctx->locals[i].reg_cl);
        }
    }

    if (ctx->local_frame_sz > 0) {
        ctx->local_frame_sz = (ctx->local_frame_sz + 15) & ~15;
        ctx->fp_reg = ptx_alloc_reg(ctx, PTX_CL_I64);
        ptx_reg_name(PTX_CL_I64, ctx->fp_reg, fpname, sizeof(fpname));
        snprintf(line, sizeof(line), "    cvta.local.u64 %s, __loc;\n", fpname);
        ptx_buf_str(&ctx->body, line);
    }

    for (int p = 0; p < num_param_locals; p++) {
        PtxLocal *loc = &ctx->locals[p];
        if (!loc->is_memory) {
            ptx_reg_name(loc->reg_cl, loc->reg_id, rname, sizeof(rname));
            snprintf(line, sizeof(line), "    ld.param%s %s, [_p%d];\n", ptx_param_ty(loc->type), rname, p);
            ptx_buf_str(&ctx->body, line);
        } else {
            int tr = ptx_alloc_reg(ctx, loc->reg_cl);
            ptx_reg_name(loc->reg_cl, tr, rname, sizeof(rname));
            ptx_reg_name(PTX_CL_I64, ctx->fp_reg, fpname, sizeof(fpname));
            snprintf(line, sizeof(line), "    ld.param%s %s, [_p%d];\n", ptx_param_ty(loc->type), rname, p);
            ptx_buf_str(&ctx->body, line);
            if (loc->stack_off > 0) {
                snprintf(line, sizeof(line), "    st%s [%s+%d], %s;\n", ptx_st_ty(loc->type), fpname, loc->stack_off, rname);
            } else {
                snprintf(line, sizeof(line), "    st%s [%s], %s;\n", ptx_st_ty(loc->type), fpname, rname);
            }
            ptx_buf_str(&ctx->body, line);
        }
    }

    for (int i = num_param_locals; i < ctx->num_locals; i++) {
        PtxLocal *loc = &ctx->locals[i];
        if (!loc->is_memory) {
            ptx_reg_name(loc->reg_cl, loc->reg_id, rname, sizeof(rname));
            if (loc->reg_cl == PTX_CL_F32) snprintf(line, sizeof(line), "    mov.f32 %s, 0f00000000;\n", rname);
            else if (loc->reg_cl == PTX_CL_F64) snprintf(line, sizeof(line), "    mov.f64 %s, 0d0000000000000000;\n", rname);
            else if (loc->reg_cl == PTX_CL_I64) snprintf(line, sizeof(line), "    mov.u64 %s, 0;\n", rname);
            else snprintf(line, sizeof(line), "    mov.u32 %s, 0;\n", rname);
            ptx_buf_str(&ctx->body, line);
        }
    }

    if (fn->body) ptx_lower_stmt(ctx, fn->body);

    if (!ctx->is_kernel && ctx->ret_cl != PTX_CL_VOID) {
        int zr = ptx_alloc_reg(ctx, ctx->ret_cl);
        ptx_reg_name(ctx->ret_cl, zr, rname, sizeof(rname));
        if (ctx->ret_cl == PTX_CL_F32) snprintf(line, sizeof(line), "    mov.f32 %s, 0f00000000;\n", rname);
        else if (ctx->ret_cl == PTX_CL_F64) snprintf(line, sizeof(line), "    mov.f64 %s, 0d0000000000000000;\n", rname);
        else if (ctx->ret_cl == PTX_CL_I64) snprintf(line, sizeof(line), "    mov.u64 %s, 0;\n", rname);
        else snprintf(line, sizeof(line), "    mov.u32 %s, 0;\n", rname);
        ptx_buf_str(&ctx->body, line);
        snprintf(line, sizeof(line), "    st.param%s [_ret+0], %s;\n", ptx_param_ty(ret_t), rname);
        ptx_buf_str(&ctx->body, line);
    }
    ptx_buf_str(&ctx->body, "    ret;\n");

    if (ctx->is_kernel) {
        fprintf(out, ".visible .entry %s(", fn->func_def_name);
    } else if (ctx->ret_cl != PTX_CL_VOID) {
        fprintf(out, ".func (.param %s _ret) %s(", ptx_param_ty(ret_t), fn->func_def_name);
    } else {
        fprintf(out, ".func %s(", fn->func_def_name);
    }
    for (int p = 0; p < fn->num_params && p < MAX_PARAMS; p++) {
        Type *pt = (fn->func_params && fn->func_params->types) ? fn->func_params->types[p] : ctx->cc->ty_int;
        fprintf(out, "%s\n    .param %s _p%d", (p > 0) ? "," : "", ptx_param_ty(pt), p);
    }
    fprintf(out, "\n) {\n");
    if (ctx->local_frame_sz > 0) {
        fprintf(out, "    .local .align 16 .b8 __loc[%d];\n", ctx->local_frame_sz);
    }
    if (ctx->cnt_pred > 0) fprintf(out, "    .reg .pred %%p<%d>;\n", ctx->cnt_pred);
    if (ctx->cnt_i32 > 0) fprintf(out, "    .reg .b32 %%r<%d>;\n", ctx->cnt_i32);
    if (ctx->cnt_i64 > 0) fprintf(out, "    .reg .b64 %%rd<%d>;\n", ctx->cnt_i64);
    if (ctx->cnt_f32 > 0) fprintf(out, "    .reg .f32 %%f<%d>;\n", ctx->cnt_f32);
    if (ctx->cnt_f64 > 0) fprintf(out, "    .reg .f64 %%fd<%d>;\n", ctx->cnt_f64);
    if (ctx->body.data && ctx->body.len > 0) fwrite(ctx->body.data, 1, ctx->body.len, out);
    fprintf(out, "}\n\n");
    return (ctx->errors == 0) ? 0 : -1;
}

int ptx_lower_program(Compiler *cc, Node *prog, const char *output_file, const char *ptx_arch) {
    if (!cc || !prog || !output_file) return -1;
    const char *arch = (ptx_arch && ptx_arch[0]) ? ptx_arch : "sm_120";
    const char *ptx_ver = (strncmp(arch, "sm_12", 5) == 0 || strncmp(arch, "sm_10", 5) == 0) ? "8.7" : "8.5";

    PtxCtx ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.cc = cc;
    ctx.prog = prog;
    ctx.ptx_arch = arch;
    ptx_buf_init(&ctx.hdr);
    ptx_buf_init(&ctx.body);

    for (Node *c = prog; c; c = c->next) {
        if (c->kind != ND_FUNC_DEF || !c->body) continue;
        if (ctx.num_sigs >= ctx.sig_cap) {
            int ncap = ctx.sig_cap ? ctx.sig_cap * 2 : 32;
            PtxFuncSig *ns = (PtxFuncSig *)realloc(ctx.sigs, (size_t)ncap * sizeof(PtxFuncSig));
            if (!ns) { ptx_buf_free(&ctx.hdr); ptx_buf_free(&ctx.body); free(ctx.sigs); return -1; }
            ctx.sigs = ns;
            ctx.sig_cap = ncap;
        }
        PtxFuncSig *sig = &ctx.sigs[ctx.num_sigs++];
        memset(sig, 0, sizeof(PtxFuncSig));
        strncpy(sig->name, c->func_def_name, MAX_IDENT - 1);
        sig->ret_type = (c->func_type && c->func_type->ret) ? c->func_type->ret : cc->ty_void;
        sig->is_kernel = (!c->is_static && sig->ret_type->kind == TY_VOID) ? 1 : 0;
        sig->num_params = (c->num_params < MAX_PARAMS) ? c->num_params : MAX_PARAMS;
        for (int p = 0; p < sig->num_params; p++) {
            Type *pt = (c->func_params && c->func_params->types) ? c->func_params->types[p] : cc->ty_int;
            sig->param_types[p] = pt ? pt : cc->ty_int;
            sig->param_cls[p] = ptx_reg_class(sig->param_types[p]);
        }
    }

    FILE *out = fopen(output_file, "wb");
    if (!out) {
        fprintf(stderr, "zcc ptx: cannot open output file '%s'\n", output_file);
        ptx_buf_free(&ctx.hdr); ptx_buf_free(&ctx.body); free(ctx.sigs); free(ctx.locals);
        return -1;
    }

    fprintf(out, "// ZCC Native C99->PTX Backend (Target: %s, PTX ISA %s)\n", arch, ptx_ver);
    fprintf(out, ".version %s\n.target %s\n.address_size 64\n\n", ptx_ver, arch);

    /* Forward declarations for all .func device helpers */
    for (int i = 0; i < ctx.num_sigs; i++) {
        PtxFuncSig *sig = &ctx.sigs[i];
        if (sig->is_kernel) continue;
        if (ptx_reg_class(sig->ret_type) != PTX_CL_VOID) {
            fprintf(out, ".func (.param %s _ret) %s(", ptx_param_ty(sig->ret_type), sig->name);
        } else {
            fprintf(out, ".func %s(", sig->name);
        }
        for (int p = 0; p < sig->num_params; p++) {
            fprintf(out, "%s.param %s _p%d", (p > 0) ? ", " : "", ptx_param_ty(sig->param_types[p]), p);
        }
        fprintf(out, ");\n");
    }
    fprintf(out, "\n");

    int fn_idx = 0;
    for (Node *c = prog; c; c = c->next) {
        if (c->kind != ND_FUNC_DEF || !c->body) continue;
        Type *rt = (c->func_type && c->func_type->ret) ? c->func_type->ret : cc->ty_void;
        int isk = (!c->is_static && rt->kind == TY_VOID) ? 1 : 0;
        if (!isk) {
            if (ptx_emit_function(&ctx, c, out, fn_idx++) != 0) break;
        }
    }
    if (ctx.errors == 0) {
        for (Node *c = prog; c; c = c->next) {
            if (c->kind != ND_FUNC_DEF || !c->body) continue;
            Type *rt = (c->func_type && c->func_type->ret) ? c->func_type->ret : cc->ty_void;
            int isk = (!c->is_static && rt->kind == TY_VOID) ? 1 : 0;
            if (isk) {
                if (ptx_emit_function(&ctx, c, out, fn_idx++) != 0) break;
            }
        }
    }

    fclose(out);
    if (ctx.errors != 0) remove(output_file);
    ptx_buf_free(&ctx.hdr);
    ptx_buf_free(&ctx.body);
    free(ctx.sigs);
    free(ctx.locals);
    return (ctx.errors == 0) ? 0 : -1;
}

/* ================================================================ */
/* TRACK 8: NATIVE GGUF v3 -> BLACKWELL SM_120 PTX AOT COMPILER     */
/* ================================================================ */

enum {
    GGUF_PTX_MAX_LAYERS_A = 16,
    GGUF_PTX_MAX_LAYERS   = 256,
    GGUF_PTX_MAX_TENSORS  = 16384,
    GGUF_PTX_F32          = 0,
    GGUF_PTX_F16          = 1,
    GGUF_PTX_Q4_0         = 2,
    GGUF_PTX_Q8_0         = 8,
    GGUF_PTX_Q4_K         = 12,
    GGUF_PTX_Q6_K         = 14,
    GGUF_PTX_BF16         = 30
};

typedef struct {
    char name[128];
    uint32_t n_dims;
    uint64_t dims[4];
    uint32_t ggml_type;
    uint64_t offset;
    uint64_t nbytes;
} GgufPtxTensor;

typedef struct {
    uint64_t off_attn_norm; uint32_t t_attn_norm;
    uint64_t off_wq;        uint32_t t_wq;
    uint64_t off_wk;        uint32_t t_wk;
    uint64_t off_wv;        uint32_t t_wv;
    uint64_t off_wo;        uint32_t t_wo;
    uint64_t off_ffn_norm;  uint32_t t_ffn_norm;
    uint64_t off_wgate;     uint32_t t_wgate;
    uint64_t off_wup;       uint32_t t_wup;
    uint64_t off_wdown;     uint32_t t_wdown;
    uint64_t off_bq;        int has_bq;
    uint64_t off_bk;        int has_bk;
    uint64_t off_bv;        int has_bv;
} GgufPtxLayer;

typedef struct {
    uint32_t n_layers;
    uint32_t dim;
    uint32_t hidden_dim;
    uint32_t n_heads;
    uint32_t n_kv_heads;
    uint32_t max_seq_len;
    float rms_eps;
    float rope_theta;
} GgufPtxArchCfg;

typedef struct {
    char arch[64];
    uint32_t n_layers;
    uint32_t dim;
    uint32_t hidden_dim;
    uint32_t n_heads;
    uint32_t n_kv_heads;
    uint32_t head_dim;
    uint32_t kv_dim;
    uint32_t max_seq_len;
    uint32_t vocab_size;
    uint32_t align;
    float rms_eps;
    float rope_theta;

    uint64_t data_start;
    uint64_t wblob_bytes;
    uint64_t file_fingerprint;

    GgufPtxTensor *tensors;
    uint32_t num_tensors;

    uint64_t off_tok_emb;   uint32_t t_tok_emb;
    uint64_t off_out_norm;  uint32_t t_out_norm;
    uint64_t off_wout;      uint32_t t_wout;
    GgufPtxLayer *layers;

    uint32_t weight_type_primary;
    int need_f32;
    int need_f16_emb;
    int need_f16_matvec;
    int need_q8_0;
    int need_q4_0;
    int need_bf16;
    int need_q4_k;
    int need_q6_k;
    int need_tc;
    int need_emb_q8_0;
    int need_emb_q4_0;
    int need_emb_bf16;
    int need_emb_q4_k;
    int need_emb_q6_k;
    int has_any_bias;
    int use_tier_b;

    uint32_t smem_x;
    uint32_t smem_xb;
    uint32_t smem_q;
    uint32_t smem_k;
    uint32_t smem_v;
    uint32_t smem_hb;
    uint32_t smem_hb2;
    uint32_t smem_xf16;
    uint32_t smem_att;
    uint32_t smem_red;
    uint32_t smem_bytes;
} GgufPtxModel;

static void gguf_ptx_free_model(GgufPtxModel *m) {
    if (!m) return;
    if (m->tensors) { free(m->tensors); m->tensors = NULL; }
    if (m->layers) { free(m->layers); m->layers = NULL; }
}

static uint32_t gguf_ptx_f32_bits(float v) {
    uint32_t u = 0U;
    memcpy(&u, &v, 4U);
    return u;
}

static void gguf_ptx_fmt_u64_dec(uint64_t v, char *out, size_t cap) {
    char tmp[32];
    int pos = 0;
    if (v == 0ULL) {
        if (cap >= 2U) { out[0] = '0'; out[1] = '\0'; }
        return;
    }
    while (v > 0ULL && pos < 30) {
        tmp[pos++] = (char)('0' + (int)(v % 10ULL));
        v /= 10ULL;
    }
    size_t i = 0U;
    while (pos > 0 && i + 1U < cap) out[i++] = tmp[--pos];
    out[i] = '\0';
}

static void gguf_ptx_fmt_u64_hex(uint64_t v, char *out, size_t cap) {
    uint32_t hi = (uint32_t)(v >> 32);
    uint32_t lo = (uint32_t)(v & 0xFFFFFFFFULL);
    snprintf(out, cap, "0x%08x%08x", (unsigned int)hi, (unsigned int)lo);
}

static int gguf_ptx_mul_u64(uint64_t a, uint64_t b, uint64_t *out) {
    if (a != 0ULL && b > (0xFFFFFFFFFFFFFFFFULL / a)) return -1;
    *out = a * b;
    return 0;
}

typedef struct {
    FILE *fp;
    uint8_t *buf;
    size_t cap;
    size_t fill;
    size_t idx;
    uint64_t pos;
    uint64_t fp64;
} GgufPtxStream;

static int gs_init(GgufPtxStream *s, const char *path) {
    memset(s, 0, sizeof(*s));
    s->fp = fopen(path, "rb");
    if (!s->fp) return -1;
    s->cap = 262144U;
    s->buf = (uint8_t *)malloc(s->cap);
    if (!s->buf) { fclose(s->fp); s->fp = NULL; return -1; }
    s->fp64 = 0xCBF29CE484222325ULL;
    return 0;
}

static void gs_close(GgufPtxStream *s) {
    if (s->fp) { fclose(s->fp); s->fp = NULL; }
    if (s->buf) { free(s->buf); s->buf = NULL; }
}

static int gs_refill(GgufPtxStream *s) {
    size_t got = fread(s->buf, 1U, s->cap, s->fp);
    if (got == 0U) return -1;
    uint64_t h = s->fp64;
    const uint8_t *p = s->buf;
    size_t i = 0U;
    while (i + 4U <= got) {
        h ^= (uint64_t)p[i];      h *= 0x00000100000001B3ULL;
        h ^= (uint64_t)p[i + 1U]; h *= 0x00000100000001B3ULL;
        h ^= (uint64_t)p[i + 2U]; h *= 0x00000100000001B3ULL;
        h ^= (uint64_t)p[i + 3U]; h *= 0x00000100000001B3ULL;
        i += 4U;
    }
    while (i < got) {
        h ^= (uint64_t)p[i];
        h *= 0x00000100000001B3ULL;
        i++;
    }
    s->fp64 = h;
    s->idx = 0U;
    s->fill = got;
    return 0;
}

static int gs_rd_bytes(GgufPtxStream *s, void *dst, size_t n) {
    uint8_t *out = (uint8_t *)dst;
    size_t rem = n;
    while (rem > 0U) {
        if (s->idx >= s->fill) {
            if (gs_refill(s) != 0) return -1;
        }
        size_t avail = s->fill - s->idx;
        size_t take = (rem < avail) ? rem : avail;
        if (out) { memcpy(out, s->buf + s->idx, take); out += take; }
        s->idx += take;
        s->pos += (uint64_t)take;
        rem -= take;
    }
    return 0;
}

static int gs_skip_u64(GgufPtxStream *s, uint64_t n) {
    while (n > 0ULL) {
        if (s->idx >= s->fill) {
            if (gs_refill(s) != 0) return -1;
        }
        uint64_t avail = (uint64_t)(s->fill - s->idx);
        uint64_t take = (n < avail) ? n : avail;
        s->idx += (size_t)take;
        s->pos += take;
        n -= take;
    }
    return 0;
}

static void gs_drain_eof(GgufPtxStream *s) {
    if (s->idx < s->fill) {
        uint64_t avail = (uint64_t)(s->fill - s->idx);
        s->idx = s->fill;
        s->pos += avail;
    }
    while (gs_refill(s) == 0) {
        s->pos += (uint64_t)s->fill;
        s->idx = s->fill;
    }
}

static int gs_rd_u32(GgufPtxStream *s, uint32_t *v) {
    uint8_t b[4];
    if (gs_rd_bytes(s, b, 4U) != 0) return -1;
    *v = (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
    return 0;
}

static int gs_rd_u64(GgufPtxStream *s, uint64_t *v) {
    uint32_t lo = 0U, hi = 0U;
    if (gs_rd_u32(s, &lo) != 0 || gs_rd_u32(s, &hi) != 0) return -1;
    *v = ((uint64_t)hi << 32) | (uint64_t)lo;
    return 0;
}

static int gs_rd_f32(GgufPtxStream *s, float *v) {
    uint32_t u = 0U;
    if (gs_rd_u32(s, &u) != 0) return -1;
    memcpy(v, &u, 4U);
    return 0;
}

static int gs_rd_str(GgufPtxStream *s, char *dst, size_t cap) {
    uint64_t len = 0ULL;
    if (gs_rd_u64(s, &len) != 0) return -1;
    if (len > 16777216ULL) return -1;
    uint64_t cpy64 = (cap > 1U && len < (uint64_t)(cap - 1U)) ? len : ((cap > 1U) ? (uint64_t)(cap - 1U) : 0ULL);
    if (cpy64 > 0ULL) {
        if (gs_rd_bytes(s, dst, (size_t)cpy64) != 0) return -1;
    }
    if (cap > 0U) dst[(size_t)cpy64] = '\0';
    if (len > cpy64) {
        if (gs_skip_u64(s, len - cpy64) != 0) return -1;
    }
    return 0;
}

static int gs_skip_val(GgufPtxStream *s, uint32_t vt, int depth) {
    uint8_t u8 = 0; uint16_t u16 = 0; uint32_t u32 = 0; uint64_t u64 = 0ULL;
    if (depth > 8) return -1;
    if (vt == 0U || vt == 1U || vt == 7U) return gs_rd_bytes(s, &u8, 1U);
    if (vt == 2U || vt == 3U) return gs_rd_bytes(s, &u16, 2U);
    if (vt == 4U || vt == 5U || vt == 6U) return gs_rd_u32(s, &u32);
    if (vt == 10U || vt == 11U || vt == 12U) return gs_rd_u64(s, &u64);
    if (vt == 8U) {
        if (gs_rd_u64(s, &u64) != 0) return -1;
        return gs_skip_u64(s, u64);
    }
    if (vt == 9U) {
        uint32_t et = 0U; uint64_t cnt = 0ULL;
        if (gs_rd_u32(s, &et) != 0 || gs_rd_u64(s, &cnt) != 0) return -1;
        if (cnt > 2097152ULL) return -1;
        for (uint64_t i = 0ULL; i < cnt; i++) {
            if (gs_skip_val(s, et, depth + 1) != 0) return -1;
        }
        return 0;
    }
    return -1;
}

static int gguf_ptx_parse_arch_kv(GgufPtxStream *s, GgufPtxArchCfg *ac, const char *sub, uint32_t vt) {
    if (strcmp(sub, "block_count") == 0 && vt == 4U) return gs_rd_u32(s, &ac->n_layers);
    if (strcmp(sub, "embedding_length") == 0 && vt == 4U) return gs_rd_u32(s, &ac->dim);
    if (strcmp(sub, "feed_forward_length") == 0 && vt == 4U) return gs_rd_u32(s, &ac->hidden_dim);
    if (strcmp(sub, "attention.head_count") == 0 && vt == 4U) return gs_rd_u32(s, &ac->n_heads);
    if (strcmp(sub, "attention.head_count_kv") == 0 && vt == 4U) return gs_rd_u32(s, &ac->n_kv_heads);
    if (strcmp(sub, "context_length") == 0 && vt == 4U) return gs_rd_u32(s, &ac->max_seq_len);
    if (strcmp(sub, "attention.layer_norm_rms_epsilon") == 0 && vt == 6U) return gs_rd_f32(s, &ac->rms_eps);
    if (strcmp(sub, "rope.freq_base") == 0 && vt == 6U) return gs_rd_f32(s, &ac->rope_theta);
    return 1;
}

static const GgufPtxTensor *gguf_ptx_find(const GgufPtxModel *m, const char *name) {
    for (uint32_t i = 0U; i < m->num_tensors; i++) {
        if (strcmp(m->tensors[i].name, name) == 0) return &m->tensors[i];
    }
    return NULL;
}

static int gguf_ptx_req_1d(const GgufPtxModel *m, const char *name, uint32_t exp_n, uint64_t *out_off, uint32_t *out_t) {
    const GgufPtxTensor *t = gguf_ptx_find(m, name);
    if (!t || t->n_dims != 1U || t->dims[0] != (uint64_t)exp_n || t->ggml_type != GGUF_PTX_F32) {
        fprintf(stderr, "zcc ptx: GGUF_PARSE_ERROR: missing/invalid 1D norm tensor '%s'\n", name);
        return -1;
    }
    *out_off = t->offset;
    *out_t = t->ggml_type;
    return 0;
}

static int gguf_ptx_req_2d(const GgufPtxModel *m, const char *name, uint32_t ek, uint32_t en, uint64_t *out_off, uint32_t *out_t) {
    const GgufPtxTensor *t = gguf_ptx_find(m, name);
    if (!t || t->n_dims != 2U || t->dims[0] != (uint64_t)ek || t->dims[1] != (uint64_t)en) {
        fprintf(stderr, "zcc ptx: GGUF_PARSE_ERROR: missing/shape mismatch on tensor '%s'\n", name);
        return -1;
    }
    *out_off = t->offset;
    *out_t = t->ggml_type;
    return 0;
}

static void gguf_ptx_mark_linear_fmt(GgufPtxModel *m, uint32_t wtype, uint32_t k_dim) {
    if (wtype == GGUF_PTX_F32) m->need_f32 = 1;
    else if (wtype == GGUF_PTX_F16) {
        if ((k_dim & 15U) == 0U) m->need_tc = 1;
        else m->need_f16_matvec = 1;
    } else if (wtype == GGUF_PTX_Q8_0) m->need_q8_0 = 1;
    else if (wtype == GGUF_PTX_Q4_0) m->need_q4_0 = 1;
    else if (wtype == GGUF_PTX_BF16) m->need_bf16 = 1;
    else if (wtype == GGUF_PTX_Q4_K) m->need_q4_k = 1;
    else if (wtype == GGUF_PTX_Q6_K) m->need_q6_k = 1;
}

static int gguf_ptx_read_model(const char *path, GgufPtxModel *m) {
    memset(m, 0, sizeof(*m));
    m->align = 32U;
    m->rms_eps = 1e-5f;
    m->rope_theta = 10000.0f;
    strcpy(m->arch, "llama");

    GgufPtxArchCfg arch_cfgs[2];
    memset(arch_cfgs, 0, sizeof(arch_cfgs));
    arch_cfgs[0].rms_eps = 1e-5f; arch_cfgs[0].rope_theta = 10000.0f;
    arch_cfgs[1].rms_eps = 1e-5f; arch_cfgs[1].rope_theta = 10000.0f;

    GgufPtxStream s;
    if (gs_init(&s, path) != 0) {
        fprintf(stderr, "zcc ptx: GGUF_PARSE_ERROR: cannot open '%s'\n", path);
        return -1;
    }

    uint32_t magic = 0U, version = 0U;
    uint64_t n_tensors = 0ULL, n_kv = 0ULL;
    if (gs_rd_u32(&s, &magic) != 0 || gs_rd_u32(&s, &version) != 0 ||
        gs_rd_u64(&s, &n_tensors) != 0 || gs_rd_u64(&s, &n_kv) != 0) {
        gs_close(&s);
        fprintf(stderr, "zcc ptx: GGUF_PARSE_ERROR: truncated header\n");
        return -1;
    }
    if (magic != 0x46554747U || version != 3U) {
        gs_close(&s);
        fprintf(stderr, "zcc ptx: GGUF_PARSE_ERROR: invalid magic/version\n");
        return -1;
    }
    if (n_tensors == 0ULL || n_tensors > (uint64_t)GGUF_PTX_MAX_TENSORS || n_kv > 4096ULL) {
        gs_close(&s);
        fprintf(stderr, "zcc ptx: GGUF_PARSE_ERROR: invalid tensor/kv counts\n");
        return -1;
    }

    char key[256];
    for (uint64_t i = 0ULL; i < n_kv; i++) {
        uint32_t vt = 0U;
        if (gs_rd_str(&s, key, sizeof(key)) != 0 || gs_rd_u32(&s, &vt) != 0) {
            gs_close(&s); return -1;
        }
        if (strcmp(key, "general.architecture") == 0 && vt == 8U) {
            if (gs_rd_str(&s, m->arch, sizeof(m->arch)) != 0) { gs_close(&s); return -1; }
        } else if (strcmp(key, "general.alignment") == 0 && vt == 4U) {
            uint32_t av = 0U;
            if (gs_rd_u32(&s, &av) != 0) { gs_close(&s); return -1; }
            if (av == 0U || (av & (av - 1U)) != 0U || av > (1U << 20)) {
                gs_close(&s);
                fprintf(stderr, "zcc ptx: GGUF_PARSE_ERROR: invalid non-power-of-two general.alignment %u\n", (unsigned int)av);
                return -1;
            }
            m->align = av;
        } else if (strncmp(key, "llama.", 6) == 0) {
            int r = gguf_ptx_parse_arch_kv(&s, &arch_cfgs[0], key + 6, vt);
            if (r < 0) { gs_close(&s); return -1; }
            if (r > 0 && gs_skip_val(&s, vt, 0) != 0) { gs_close(&s); return -1; }
        } else if (strncmp(key, "qwen2.", 6) == 0) {
            int r = gguf_ptx_parse_arch_kv(&s, &arch_cfgs[1], key + 6, vt);
            if (r < 0) { gs_close(&s); return -1; }
            if (r > 0 && gs_skip_val(&s, vt, 0) != 0) { gs_close(&s); return -1; }
        } else if (strcmp(key, "tokenizer.ggml.tokens") == 0 && vt == 9U) {
            uint32_t et = 0U; uint64_t cnt = 0ULL;
            if (gs_rd_u32(&s, &et) != 0 || gs_rd_u64(&s, &cnt) != 0 ||
                et != 8U || cnt == 0ULL || cnt > 1048576ULL) {
                gs_close(&s); return -1;
            }
            m->vocab_size = (uint32_t)cnt;
            for (uint64_t ti = 0ULL; ti < cnt; ti++) {
                uint64_t slen = 0ULL;
                if (gs_rd_u64(&s, &slen) != 0 || gs_skip_u64(&s, slen) != 0) { gs_close(&s); return -1; }
            }
        } else {
            if (gs_skip_val(&s, vt, 0) != 0) { gs_close(&s); return -1; }
        }
    }

    int arch_idx = -1;
    if (strcmp(m->arch, "llama") == 0) arch_idx = 0;
    else if (strcmp(m->arch, "qwen2") == 0) arch_idx = 1;
    if (arch_idx < 0) {
        gs_close(&s);
        fprintf(stderr, "zcc ptx: GGUF_PARSE_ERROR: unsupported architecture '%s'\n", m->arch);
        return -1;
    }
    m->n_layers = arch_cfgs[arch_idx].n_layers;
    m->dim = arch_cfgs[arch_idx].dim;
    m->hidden_dim = arch_cfgs[arch_idx].hidden_dim;
    m->n_heads = arch_cfgs[arch_idx].n_heads;
    m->n_kv_heads = arch_cfgs[arch_idx].n_kv_heads;
    m->max_seq_len = arch_cfgs[arch_idx].max_seq_len;
    m->rms_eps = arch_cfgs[arch_idx].rms_eps;
    m->rope_theta = arch_cfgs[arch_idx].rope_theta;

    if (m->n_layers == 0U || m->n_layers > (uint32_t)GGUF_PTX_MAX_LAYERS ||
        m->dim == 0U || m->hidden_dim == 0U || m->n_heads == 0U || (m->dim % m->n_heads) != 0U) {
        gs_close(&s);
        fprintf(stderr, "zcc ptx: GGUF_PARSE_ERROR: invalid %s metadata\n", m->arch);
        return -1;
    }
    if (m->n_kv_heads == 0U) m->n_kv_heads = m->n_heads;
    if ((m->n_heads % m->n_kv_heads) != 0U) {
        gs_close(&s);
        fprintf(stderr, "zcc ptx: GGUF_PARSE_ERROR: GQA head count mismatch\n");
        return -1;
    }
    if (m->max_seq_len == 0U) m->max_seq_len = 128U;
    m->head_dim = m->dim / m->n_heads;
    m->kv_dim = m->n_kv_heads * m->head_dim;
    m->num_tensors = (uint32_t)n_tensors;
    m->tensors = (GgufPtxTensor *)calloc((size_t)m->num_tensors, sizeof(GgufPtxTensor));
    m->layers = (GgufPtxLayer *)calloc((size_t)m->n_layers, sizeof(GgufPtxLayer));
    if (!m->tensors || !m->layers) {
        gs_close(&s);
        gguf_ptx_free_model(m);
        return -1;
    }

    for (uint32_t ti = 0U; ti < m->num_tensors; ti++) {
        GgufPtxTensor *t = &m->tensors[ti];
        if (gs_rd_str(&s, t->name, sizeof(t->name)) != 0 ||
            gs_rd_u32(&s, &t->n_dims) != 0 ||
            t->n_dims == 0U || t->n_dims > 4U) {
            gs_close(&s); gguf_ptx_free_model(m); return -1;
        }
        uint64_t numel = 1ULL;
        for (uint32_t d = 0U; d < t->n_dims; d++) {
            if (gs_rd_u64(&s, &t->dims[d]) != 0 || t->dims[d] == 0ULL ||
                gguf_ptx_mul_u64(numel, t->dims[d], &numel) != 0) {
                gs_close(&s); gguf_ptx_free_model(m);
                fprintf(stderr, "zcc ptx: GGUF_PARSE_ERROR: tensor '%s' size overflow\n", t->name);
                return -1;
            }
        }
        if (gs_rd_u32(&s, &t->ggml_type) != 0 ||
            gs_rd_u64(&s, &t->offset) != 0) {
            gs_close(&s); gguf_ptx_free_model(m); return -1;
        }
        if (t->ggml_type == GGUF_PTX_F32) {
            if (gguf_ptx_mul_u64(numel, 4ULL, &t->nbytes) != 0) { gs_close(&s); gguf_ptx_free_model(m); return -1; }
        } else if (t->ggml_type == GGUF_PTX_F16 || t->ggml_type == GGUF_PTX_BF16) {
            if (gguf_ptx_mul_u64(numel, 2ULL, &t->nbytes) != 0) { gs_close(&s); gguf_ptx_free_model(m); return -1; }
        } else if (t->ggml_type == GGUF_PTX_Q4_0) {
            if ((t->dims[0] % 32ULL) != 0ULL || gguf_ptx_mul_u64(numel / 32ULL, 18ULL, &t->nbytes) != 0) {
                gs_close(&s); gguf_ptx_free_model(m);
                fprintf(stderr, "zcc ptx: GGUF_PARSE_ERROR: Q4_0 alignment failure on '%s'\n", t->name);
                return -1;
            }
        } else if (t->ggml_type == GGUF_PTX_Q8_0) {
            if ((t->dims[0] % 32ULL) != 0ULL || gguf_ptx_mul_u64(numel / 32ULL, 34ULL, &t->nbytes) != 0) {
                gs_close(&s); gguf_ptx_free_model(m); return -1;
            }
        } else if (t->ggml_type == GGUF_PTX_Q4_K) {
            if ((t->dims[0] % 256ULL) != 0ULL || gguf_ptx_mul_u64(numel / 256ULL, 144ULL, &t->nbytes) != 0) {
                gs_close(&s); gguf_ptx_free_model(m);
                fprintf(stderr, "zcc ptx: GGUF_PARSE_ERROR: Q4_K alignment failure on '%s'\n", t->name);
                return -1;
            }
        } else if (t->ggml_type == GGUF_PTX_Q6_K) {
            if ((t->dims[0] % 256ULL) != 0ULL || gguf_ptx_mul_u64(numel / 256ULL, 210ULL, &t->nbytes) != 0) {
                gs_close(&s); gguf_ptx_free_model(m);
                fprintf(stderr, "zcc ptx: GGUF_PARSE_ERROR: Q6_K alignment failure on '%s'\n", t->name);
                return -1;
            }
        } else {
            gs_close(&s);
            fprintf(stderr, "zcc ptx: GGUF_PARSE_ERROR: unsupported ggml_type %u on '%s'\n", (unsigned int)t->ggml_type, t->name);
            gguf_ptx_free_model(m);
            return -1;
        }
    }

    uint64_t align_m1 = (uint64_t)(m->align - 1U);
    if (s.pos > 0xFFFFFFFFFFFFFFFFULL - align_m1) { gs_close(&s); gguf_ptx_free_model(m); return -1; }
    uint64_t data_start = (s.pos + align_m1) & ~align_m1;
    if (data_start > s.pos && gs_skip_u64(&s, data_start - s.pos) != 0) {
        gs_close(&s); gguf_ptx_free_model(m);
        fprintf(stderr, "zcc ptx: GGUF_PARSE_ERROR: data_start past EOF\n");
        return -1;
    }
    gs_drain_eof(&s);
    uint64_t fsz64 = s.pos;
    m->file_fingerprint = s.fp64;
    gs_close(&s);

    if (fsz64 < data_start) {
        gguf_ptx_free_model(m);
        fprintf(stderr, "zcc ptx: GGUF_PARSE_ERROR: data_start past EOF\n");
        return -1;
    }
    m->data_start = data_start;
    m->wblob_bytes = fsz64 - data_start;

    for (uint32_t ti = 0U; ti < m->num_tensors; ti++) {
        GgufPtxTensor *t = &m->tensors[ti];
        if (t->offset > m->wblob_bytes || t->nbytes > m->wblob_bytes - t->offset) {
            fprintf(stderr, "zcc ptx: GGUF_PARSE_ERROR: tensor '%s' exceeds EOF\n", t->name);
            gguf_ptx_free_model(m);
            return -1;
        }
    }

    const GgufPtxTensor *emb = gguf_ptx_find(m, "token_embd.weight");
    if (!emb || emb->n_dims != 2U || emb->dims[0] != (uint64_t)m->dim) {
        fprintf(stderr, "zcc ptx: GGUF_PARSE_ERROR: invalid token_embd.weight\n");
        gguf_ptx_free_model(m);
        return -1;
    }
    if (m->vocab_size == 0U) {
        if (emb->dims[1] == 0ULL || emb->dims[1] > 1048576ULL) { gguf_ptx_free_model(m); return -1; }
        m->vocab_size = (uint32_t)emb->dims[1];
    } else if (emb->dims[1] != (uint64_t)m->vocab_size) {
        fprintf(stderr, "zcc ptx: GGUF_PARSE_ERROR: token_embd.weight vocab dim mismatch\n");
        gguf_ptx_free_model(m);
        return -1;
    }
    m->off_tok_emb = emb->offset;
    m->t_tok_emb = emb->ggml_type;
    if (m->t_tok_emb == GGUF_PTX_F16) m->need_f16_emb = 1;
    else if (m->t_tok_emb == GGUF_PTX_F32) m->need_f32 = 1;
    else if (m->t_tok_emb == GGUF_PTX_Q8_0) m->need_emb_q8_0 = 1;
    else if (m->t_tok_emb == GGUF_PTX_Q4_0) m->need_emb_q4_0 = 1;
    else if (m->t_tok_emb == GGUF_PTX_BF16) m->need_emb_bf16 = 1;
    else if (m->t_tok_emb == GGUF_PTX_Q4_K) m->need_emb_q4_k = 1;
    else if (m->t_tok_emb == GGUF_PTX_Q6_K) m->need_emb_q6_k = 1;

    if (gguf_ptx_req_1d(m, "output_norm.weight", m->dim, &m->off_out_norm, &m->t_out_norm) != 0) { gguf_ptx_free_model(m); return -1; }
    const GgufPtxTensor *wout = gguf_ptx_find(m, "output.weight");
    if (wout) {
        if (wout->n_dims != 2U || wout->dims[0] != (uint64_t)m->dim || wout->dims[1] != (uint64_t)m->vocab_size) {
            fprintf(stderr, "zcc ptx: GGUF_PARSE_ERROR: output.weight shape mismatch\n");
            gguf_ptx_free_model(m);
            return -1;
        }
        m->off_wout = wout->offset;
        m->t_wout = wout->ggml_type;
    } else {
        m->off_wout = m->off_tok_emb;
        m->t_wout = m->t_tok_emb;
    }
    gguf_ptx_mark_linear_fmt(m, m->t_wout, m->dim);

    for (uint32_t l = 0U; l < m->n_layers; l++) {
        char tn[128];
        GgufPtxLayer *lw = &m->layers[l];
        snprintf(tn, sizeof(tn), "blk.%u.attn_norm.weight", (unsigned int)l);
        if (gguf_ptx_req_1d(m, tn, m->dim, &lw->off_attn_norm, &lw->t_attn_norm) != 0) { gguf_ptx_free_model(m); return -1; }
        snprintf(tn, sizeof(tn), "blk.%u.attn_q.weight", (unsigned int)l);
        if (gguf_ptx_req_2d(m, tn, m->dim, m->dim, &lw->off_wq, &lw->t_wq) != 0) { gguf_ptx_free_model(m); return -1; }
        snprintf(tn, sizeof(tn), "blk.%u.attn_k.weight", (unsigned int)l);
        if (gguf_ptx_req_2d(m, tn, m->dim, m->kv_dim, &lw->off_wk, &lw->t_wk) != 0) { gguf_ptx_free_model(m); return -1; }
        snprintf(tn, sizeof(tn), "blk.%u.attn_v.weight", (unsigned int)l);
        if (gguf_ptx_req_2d(m, tn, m->dim, m->kv_dim, &lw->off_wv, &lw->t_wv) != 0) { gguf_ptx_free_model(m); return -1; }
        snprintf(tn, sizeof(tn), "blk.%u.attn_output.weight", (unsigned int)l);
        if (gguf_ptx_req_2d(m, tn, m->dim, m->dim, &lw->off_wo, &lw->t_wo) != 0) { gguf_ptx_free_model(m); return -1; }
        snprintf(tn, sizeof(tn), "blk.%u.ffn_norm.weight", (unsigned int)l);
        if (gguf_ptx_req_1d(m, tn, m->dim, &lw->off_ffn_norm, &lw->t_ffn_norm) != 0) { gguf_ptx_free_model(m); return -1; }
        snprintf(tn, sizeof(tn), "blk.%u.ffn_gate.weight", (unsigned int)l);
        if (gguf_ptx_req_2d(m, tn, m->dim, m->hidden_dim, &lw->off_wgate, &lw->t_wgate) != 0) { gguf_ptx_free_model(m); return -1; }
        snprintf(tn, sizeof(tn), "blk.%u.ffn_up.weight", (unsigned int)l);
        if (gguf_ptx_req_2d(m, tn, m->dim, m->hidden_dim, &lw->off_wup, &lw->t_wup) != 0) { gguf_ptx_free_model(m); return -1; }
        snprintf(tn, sizeof(tn), "blk.%u.ffn_down.weight", (unsigned int)l);
        if (gguf_ptx_req_2d(m, tn, m->hidden_dim, m->dim, &lw->off_wdown, &lw->t_wdown) != 0) { gguf_ptx_free_model(m); return -1; }

        uint32_t bt = 0U;
        snprintf(tn, sizeof(tn), "blk.%u.attn_q.bias", (unsigned int)l);
        if (gguf_ptx_find(m, tn)) {
            if (gguf_ptx_req_1d(m, tn, m->dim, &lw->off_bq, &bt) != 0) { gguf_ptx_free_model(m); return -1; }
            lw->has_bq = 1; m->has_any_bias = 1;
        }
        snprintf(tn, sizeof(tn), "blk.%u.attn_k.bias", (unsigned int)l);
        if (gguf_ptx_find(m, tn)) {
            if (gguf_ptx_req_1d(m, tn, m->kv_dim, &lw->off_bk, &bt) != 0) { gguf_ptx_free_model(m); return -1; }
            lw->has_bk = 1; m->has_any_bias = 1;
        }
        snprintf(tn, sizeof(tn), "blk.%u.attn_v.bias", (unsigned int)l);
        if (gguf_ptx_find(m, tn)) {
            if (gguf_ptx_req_1d(m, tn, m->kv_dim, &lw->off_bv, &bt) != 0) { gguf_ptx_free_model(m); return -1; }
            lw->has_bv = 1; m->has_any_bias = 1;
        }

        gguf_ptx_mark_linear_fmt(m, lw->t_wq, m->dim);
        gguf_ptx_mark_linear_fmt(m, lw->t_wk, m->dim);
        gguf_ptx_mark_linear_fmt(m, lw->t_wv, m->dim);
        gguf_ptx_mark_linear_fmt(m, lw->t_wo, m->dim);
        gguf_ptx_mark_linear_fmt(m, lw->t_wgate, m->dim);
        gguf_ptx_mark_linear_fmt(m, lw->t_wup, m->dim);
        gguf_ptx_mark_linear_fmt(m, lw->t_wdown, m->hidden_dim);
    }
    m->weight_type_primary = m->layers[0].t_wq;

    /* Select Tier A (single-CTA ZCCAOT01) vs Tier B (multi-CTA FlashAttn ZCCAOT02) */
    uint32_t cur = 0U;
    uint32_t maxk = (m->hidden_dim > m->dim) ? m->hidden_dim : m->dim;
    uint64_t kv64 = (uint64_t)m->n_layers * (uint64_t)m->max_seq_len * (uint64_t)m->kv_dim * 4ULL;
    m->use_tier_b = 0;
    if (strcmp(m->arch, "llama") != 0 || m->has_any_bias ||
        m->need_q4_0 || m->need_bf16 || m->need_q4_k || m->need_q6_k ||
        m->need_emb_q4_0 || m->need_emb_bf16 || m->need_emb_q4_k || m->need_emb_q6_k ||
        m->n_layers > (uint32_t)GGUF_PTX_MAX_LAYERS_A ||
        kv64 > 1073741824ULL || m->max_seq_len > 2048U || m->dim > 2048U || m->hidden_dim > 4096U) {
        m->use_tier_b = 1;
    } else {
        m->smem_x = cur;    cur = (cur + m->dim * 4U + 15U) & ~15U;
        m->smem_xb = cur;   cur = (cur + m->dim * 4U + 15U) & ~15U;
        m->smem_q = cur;    cur = (cur + m->dim * 4U + 15U) & ~15U;
        m->smem_k = cur;    cur = (cur + m->kv_dim * 4U + 15U) & ~15U;
        m->smem_v = cur;    cur = (cur + m->kv_dim * 4U + 15U) & ~15U;
        m->smem_hb = cur;   cur = (cur + m->hidden_dim * 4U + 15U) & ~15U;
        m->smem_hb2 = cur;  cur = (cur + m->hidden_dim * 4U + 15U) & ~15U;
        m->smem_xf16 = cur; if (m->need_tc) cur = (cur + maxk * 2U + 15U) & ~15U;
        m->smem_att = cur;  cur = (cur + 4U * m->max_seq_len * 4U + 15U) & ~15U;
        m->smem_red = cur;  cur = (cur + 16U + 15U) & ~15U;
        m->smem_bytes = cur;
        if (m->smem_bytes > 45056U) m->use_tier_b = 1;
    }
    if (m->use_tier_b && (m->head_dim == 0U || m->head_dim > 256U || (m->head_dim & 1U) != 0U)) {
        fprintf(stderr, "zcc ptx: GGUF_PARSE_ERROR: unsupported head_dim %u for Multi-CTA FlashAttn\n", (unsigned int)m->head_dim);
        gguf_ptx_free_model(m);
        return -1;
    }
    return 0;
}

static void gguf_ptx_emit_warp_reduce(FILE *out, const char *name, const char *op) {
    int shifts[5] = { 16, 8, 4, 2, 1 };
    fprintf(out, ".func (.param .f32 _ret) %s(.param .f32 _p0) {\n", name);
    fprintf(out, "    .reg .f32 %%f0, %%f1;\n");
    fprintf(out, "    .reg .b32 %%r0, %%r1;\n");
    fprintf(out, "    ld.param.f32 %%f0, [_p0];\n");
    fprintf(out, "    mov.b32 %%r0, %%f0;\n");
    for (int i = 0; i < 5; i++) {
        fprintf(out, "    shfl.sync.down.b32 %%r1, %%r0, %d, 31, 0xffffffff;\n", shifts[i]);
        fprintf(out, "    mov.b32 %%f1, %%r1;\n");
        fprintf(out, "    %s %%f0, %%f0, %%f1;\n", op);
        fprintf(out, "    mov.b32 %%r0, %%f0;\n");
    }
    fprintf(out, "    shfl.sync.idx.b32 %%r1, %%r0, 0, 31, 0xffffffff;\n");
    fprintf(out, "    mov.b32 %%f0, %%r1;\n");
    fprintf(out, "    st.param.f32 [_ret], %%f0;\n");
    fprintf(out, "    ret;\n}\n\n");
}

static void gguf_ptx_emit_embed_fn(FILE *out, const GgufPtxModel *m, uint32_t fmt) {
    uint32_t dim = m->dim;
    if (fmt == GGUF_PTX_F32) {
        fprintf(out, ".func zcc_aot_embed_f32(.param .u64 _p0, .param .u64 _p1, .param .u32 _p2) {\n");
        fprintf(out, "    .reg .pred %%p0; .reg .b32 %%r<4>; .reg .b64 %%rd<6>; .reg .f32 %%f0;\n");
        fprintf(out, "    ld.param.u64 %%rd0, [_p0]; ld.param.u64 %%rd1, [_p1]; ld.param.u32 %%r0, [_p2];\n");
        fprintf(out, "    mov.u32 %%r1, %u; mul.wide.u32 %%rd2, %%r0, %%r1;\n", (unsigned int)dim);
        fprintf(out, "    shl.b64 %%rd2, %%rd2, 2; add.u64 %%rd0, %%rd0, %%rd2;\n");
        fprintf(out, "    mov.u32 %%r2, %%tid.x;\n");
        fprintf(out, "L_ef32:\n    setp.ge.u32 %%p0, %%r2, %u; @%%p0 bra L_ef32_d;\n", (unsigned int)dim);
        fprintf(out, "    mul.wide.u32 %%rd3, %%r2, 4; add.u64 %%rd4, %%rd0, %%rd3; ld.f32 %%f0, [%%rd4];\n");
        fprintf(out, "    add.u64 %%rd5, %%rd1, %%rd3; st.f32 [%%rd5], %%f0;\n");
        fprintf(out, "    add.u32 %%r2, %%r2, 128; bra L_ef32;\n");
        fprintf(out, "L_ef32_d:\n    bar.sync 0; ret;\n}\n\n");
    } else if (fmt == GGUF_PTX_F16) {
        fprintf(out, ".func zcc_aot_embed_f16(.param .u64 _p0, .param .u64 _p1, .param .u32 _p2) {\n");
        fprintf(out, "    .reg .pred %%p0; .reg .b32 %%r<4>; .reg .b64 %%rd<6>; .reg .f32 %%f0;\n");
        fprintf(out, "    ld.param.u64 %%rd0, [_p0]; ld.param.u64 %%rd1, [_p1]; ld.param.u32 %%r0, [_p2];\n");
        fprintf(out, "    mov.u32 %%r1, %u; mul.wide.u32 %%rd2, %%r0, %%r1;\n", (unsigned int)dim);
        fprintf(out, "    shl.b64 %%rd2, %%rd2, 1; add.u64 %%rd0, %%rd0, %%rd2;\n");
        fprintf(out, "    mov.u32 %%r2, %%tid.x;\n");
        fprintf(out, "L_ef16:\n    setp.ge.u32 %%p0, %%r2, %u; @%%p0 bra L_ef16_d;\n", (unsigned int)dim);
        fprintf(out, "    mul.wide.u32 %%rd3, %%r2, 2; add.u64 %%rd4, %%rd0, %%rd3; ld.u16 %%r3, [%%rd4];\n");
        fprintf(out, "    { .reg .b16 %%hb; cvt.u16.u32 %%hb, %%r3; cvt.f32.f16 %%f0, %%hb; }\n");
        fprintf(out, "    mul.wide.u32 %%rd3, %%r2, 4; add.u64 %%rd5, %%rd1, %%rd3; st.f32 [%%rd5], %%f0;\n");
        fprintf(out, "    add.u32 %%r2, %%r2, 128; bra L_ef16;\n");
        fprintf(out, "L_ef16_d:\n    bar.sync 0; ret;\n}\n\n");
    } else if (fmt == GGUF_PTX_Q8_0) {
        uint32_t row_bytes = (dim >> 5) * 34U;
        fprintf(out, ".func zcc_aot_embed_q8_0(.param .u64 _p0, .param .u64 _p1, .param .u32 _p2) {\n");
        fprintf(out, "    .reg .pred %%p0, %%p1; .reg .b32 %%r<8>; .reg .b64 %%rd<8>; .reg .f32 %%f<3>;\n");
        fprintf(out, "    ld.param.u64 %%rd0, [_p0]; ld.param.u64 %%rd1, [_p1]; ld.param.u32 %%r0, [_p2];\n");
        fprintf(out, "    mov.u32 %%r1, %u; mul.wide.u32 %%rd2, %%r0, %%r1; add.u64 %%rd0, %%rd0, %%rd2;\n", (unsigned int)row_bytes);
        fprintf(out, "    mov.u32 %%r2, %%tid.x;\n");
        fprintf(out, "L_eq8:\n    setp.ge.u32 %%p0, %%r2, %u; @%%p0 bra L_eq8_d;\n", (unsigned int)dim);
        fprintf(out, "    shr.u32 %%r3, %%r2, 5; and.b32 %%r4, %%r2, 31;\n");
        fprintf(out, "    mul.wide.u32 %%rd3, %%r3, 34; add.u64 %%rd4, %%rd0, %%rd3;\n");
        fprintf(out, "    ld.u8 %%r5, [%%rd4]; ld.u8 %%r6, [%%rd4+1]; shl.b32 %%r6, %%r6, 8; or.b32 %%r5, %%r5, %%r6;\n");
        fprintf(out, "    { .reg .b16 %%hb; cvt.u16.u32 %%hb, %%r5; cvt.f32.f16 %%f0, %%hb; }\n");
        fprintf(out, "    add.u32 %%r4, %%r4, 2; cvt.u64.u32 %%rd5, %%r4; add.u64 %%rd6, %%rd4, %%rd5;\n");
        fprintf(out, "    ld.u8 %%r7, [%%rd6]; setp.ge.u32 %%p1, %%r7, 128; @%%p1 sub.s32 %%r7, %%r7, 256; cvt.rn.f32.s32 %%f1, %%r7;\n");
        fprintf(out, "    mul.f32 %%f2, %%f0, %%f1; mul.wide.u32 %%rd7, %%r2, 4; add.u64 %%rd7, %%rd1, %%rd7; st.f32 [%%rd7], %%f2;\n");
        fprintf(out, "    add.u32 %%r2, %%r2, 128; bra L_eq8;\n");
        fprintf(out, "L_eq8_d:\n    bar.sync 0; ret;\n}\n\n");
    }
}

static void gguf_ptx_emit_rmsnorm_fn(FILE *out, const GgufPtxModel *m) {
    uint32_t inv_dim_u = gguf_ptx_f32_bits(1.0f / (float)m->dim);
    uint32_t eps_u = gguf_ptx_f32_bits(m->rms_eps);
    fprintf(out, ".func zcc_aot_rmsnorm(.param .u64 _p0, .param .u64 _p1, .param .u64 _p2, .param .u64 _p3) {\n");
    fprintf(out, "    .reg .pred %%p0, %%p1; .reg .b32 %%r<4>; .reg .b64 %%rd<8>; .reg .f32 %%f<6>;\n");
    fprintf(out, "    ld.param.u64 %%rd0, [_p0]; ld.param.u64 %%rd1, [_p1]; ld.param.u64 %%rd2, [_p2]; ld.param.u64 %%rd3, [_p3];\n");
    fprintf(out, "    mov.u32 %%r0, %%tid.x; shr.u32 %%r1, %%r0, 5; and.b32 %%r2, %%r0, 31;\n");
    fprintf(out, "    mov.f32 %%f0, 0f00000000; mov.u32 %%r3, %%r0;\n");
    fprintf(out, "L_rn1:\n    setp.ge.u32 %%p0, %%r3, %u; @%%p0 bra L_rn1_d;\n", (unsigned int)m->dim);
    fprintf(out, "    mul.wide.u32 %%rd4, %%r3, 4; add.u64 %%rd5, %%rd1, %%rd4; ld.f32 %%f1, [%%rd5];\n");
    fprintf(out, "    fma.rn.f32 %%f0, %%f1, %%f1, %%f0; add.u32 %%r3, %%r3, 128; bra L_rn1;\n");
    fprintf(out, "L_rn1_d:\n");
    fprintf(out, "    { .param .f32 _wr, _wa; st.param.f32 [_wa], %%f0; call.uni (_wr), zcc_aot_warp_sum, (_wa); ld.param.f32 %%f0, [_wr]; }\n");
    fprintf(out, "    setp.eq.u32 %%p1, %%r2, 0;\n");
    fprintf(out, "    @%%p1 mul.wide.u32 %%rd4, %%r1, 4; @%%p1 add.u64 %%rd5, %%rd3, %%rd4; @%%p1 st.f32 [%%rd5], %%f0;\n");
    fprintf(out, "    bar.sync 0;\n");
    fprintf(out, "    ld.f32 %%f1, [%%rd3]; ld.f32 %%f2, [%%rd3+4]; ld.f32 %%f3, [%%rd3+8]; ld.f32 %%f4, [%%rd3+12];\n");
    fprintf(out, "    add.f32 %%f0, %%f1, %%f2; add.f32 %%f3, %%f3, %%f4; add.f32 %%f0, %%f0, %%f3;\n");
    fprintf(out, "    fma.rn.f32 %%f1, %%f0, 0f%08X, 0f%08X; rsqrt.approx.f32 %%f5, %%f1;\n", (unsigned int)inv_dim_u, (unsigned int)eps_u);
    fprintf(out, "    mov.u32 %%r3, %%r0;\n");
    fprintf(out, "L_rn2:\n    setp.ge.u32 %%p0, %%r3, %u; @%%p0 bra L_rn2_d;\n", (unsigned int)m->dim);
    fprintf(out, "    mul.wide.u32 %%rd4, %%r3, 4; add.u64 %%rd5, %%rd1, %%rd4; ld.f32 %%f1, [%%rd5];\n");
    fprintf(out, "    add.u64 %%rd6, %%rd2, %%rd4; ld.f32 %%f2, [%%rd6];\n");
    fprintf(out, "    mul.f32 %%f3, %%f1, %%f5; mul.f32 %%f3, %%f3, %%f2;\n");
    fprintf(out, "    add.u64 %%rd7, %%rd0, %%rd4; st.f32 [%%rd7], %%f3;\n");
    fprintf(out, "    add.u32 %%r3, %%r3, 128; bra L_rn2;\n");
    fprintf(out, "L_rn2_d:\n    bar.sync 0; ret;\n}\n\n");
}

static void gguf_ptx_emit_matvec_fn(FILE *out, uint32_t fmt) {
    if (fmt == GGUF_PTX_F32) {
        fprintf(out, ".func zcc_aot_matvec_f32(.param .u64 _p0, .param .u64 _p1, .param .u64 _p2, .param .u32 _p3, .param .u32 _p4, .param .u32 _p5) {\n");
        fprintf(out, "    .reg .pred %%p<5>; .reg .b32 %%r<9>; .reg .b64 %%rd<10>; .reg .f32 %%f<4>;\n");
        fprintf(out, "    ld.param.u64 %%rd0, [_p0]; ld.param.u64 %%rd1, [_p1]; ld.param.u64 %%rd2, [_p2];\n");
        fprintf(out, "    ld.param.u32 %%r0, [_p3]; ld.param.u32 %%r1, [_p4]; ld.param.u32 %%r2, [_p5];\n");
        fprintf(out, "    mov.u32 %%r3, %%tid.x; shr.u32 %%r4, %%r3, 5; and.b32 %%r5, %%r3, 31; mov.u32 %%r6, %%r4;\n");
        fprintf(out, "L_mv32_r:\n    setp.ge.u32 %%p0, %%r6, %%r0; @%%p0 bra L_mv32_d;\n");
        fprintf(out, "    mul.wide.u32 %%rd3, %%r6, %%r1; shl.b64 %%rd3, %%rd3, 2; add.u64 %%rd4, %%rd0, %%rd3;\n");
        fprintf(out, "    mov.f32 %%f0, 0f00000000; mov.u32 %%r7, 0;\n");
        fprintf(out, "L_mv32_k:\n    setp.ge.u32 %%p1, %%r7, %%r1; @%%p1 bra L_mv32_kd;\n");
        fprintf(out, "    add.u32 %%r8, %%r7, %%r5; setp.lt.u32 %%p2, %%r8, %%r1;\n");
        fprintf(out, "    @%%p2 mul.wide.u32 %%rd5, %%r8, 4; @%%p2 add.u64 %%rd6, %%rd4, %%rd5; @%%p2 ld.f32 %%f1, [%%rd6];\n");
        fprintf(out, "    @%%p2 add.u64 %%rd7, %%rd1, %%rd5; @%%p2 ld.f32 %%f2, [%%rd7]; @%%p2 fma.rn.f32 %%f0, %%f1, %%f2, %%f0;\n");
        fprintf(out, "    add.u32 %%r7, %%r7, 32; bra L_mv32_k;\n");
        fprintf(out, "L_mv32_kd:\n");
        fprintf(out, "    { .param .f32 _wr, _wa; st.param.f32 [_wa], %%f0; call.uni (_wr), zcc_aot_warp_sum, (_wa); ld.param.f32 %%f0, [_wr]; }\n");
        fprintf(out, "    setp.eq.u32 %%p3, %%r5, 0; setp.ne.u32 %%p4, %%r2, 0; and.pred %%p4, %%p3, %%p4;\n");
        fprintf(out, "    @%%p3 mul.wide.u32 %%rd8, %%r6, 4; @%%p3 add.u64 %%rd9, %%rd2, %%rd8;\n");
        fprintf(out, "    @%%p4 ld.f32 %%f3, [%%rd9]; @%%p4 add.f32 %%f0, %%f3, %%f0; @%%p3 st.f32 [%%rd9], %%f0;\n");
        fprintf(out, "    add.u32 %%r6, %%r6, 4; bra L_mv32_r;\n");
        fprintf(out, "L_mv32_d:\n    ret;\n}\n\n");
    } else if (fmt == GGUF_PTX_F16) {
        fprintf(out, ".func zcc_aot_matvec_f16(.param .u64 _p0, .param .u64 _p1, .param .u64 _p2, .param .u32 _p3, .param .u32 _p4, .param .u32 _p5) {\n");
        fprintf(out, "    .reg .pred %%p<5>; .reg .b32 %%r<10>; .reg .b64 %%rd<10>; .reg .f32 %%f<4>;\n");
        fprintf(out, "    ld.param.u64 %%rd0, [_p0]; ld.param.u64 %%rd1, [_p1]; ld.param.u64 %%rd2, [_p2];\n");
        fprintf(out, "    ld.param.u32 %%r0, [_p3]; ld.param.u32 %%r1, [_p4]; ld.param.u32 %%r2, [_p5];\n");
        fprintf(out, "    mov.u32 %%r3, %%tid.x; shr.u32 %%r4, %%r3, 5; and.b32 %%r5, %%r3, 31; mov.u32 %%r6, %%r4;\n");
        fprintf(out, "L_mv16_r:\n    setp.ge.u32 %%p0, %%r6, %%r0; @%%p0 bra L_mv16_d;\n");
        fprintf(out, "    mul.wide.u32 %%rd3, %%r6, %%r1; shl.b64 %%rd3, %%rd3, 1; add.u64 %%rd4, %%rd0, %%rd3;\n");
        fprintf(out, "    mov.f32 %%f0, 0f00000000; mov.u32 %%r7, 0;\n");
        fprintf(out, "L_mv16_k:\n    setp.ge.u32 %%p1, %%r7, %%r1; @%%p1 bra L_mv16_kd;\n");
        fprintf(out, "    add.u32 %%r8, %%r7, %%r5; setp.lt.u32 %%p2, %%r8, %%r1;\n");
        fprintf(out, "    @%%p2 mul.wide.u32 %%rd5, %%r8, 2; @%%p2 add.u64 %%rd6, %%rd4, %%rd5; @%%p2 ld.u16 %%r9, [%%rd6];\n");
        fprintf(out, "    { .reg .b16 %%hb; mov.u16 %%hb, 0; @%%p2 cvt.u16.u32 %%hb, %%r9; @%%p2 cvt.f32.f16 %%f1, %%hb; }\n");
        fprintf(out, "    @%%p2 mul.wide.u32 %%rd5, %%r8, 4; @%%p2 add.u64 %%rd7, %%rd1, %%rd5; @%%p2 ld.f32 %%f2, [%%rd7]; @%%p2 fma.rn.f32 %%f0, %%f1, %%f2, %%f0;\n");
        fprintf(out, "    add.u32 %%r7, %%r7, 32; bra L_mv16_k;\n");
        fprintf(out, "L_mv16_kd:\n");
        fprintf(out, "    { .param .f32 _wr, _wa; st.param.f32 [_wa], %%f0; call.uni (_wr), zcc_aot_warp_sum, (_wa); ld.param.f32 %%f0, [_wr]; }\n");
        fprintf(out, "    setp.eq.u32 %%p3, %%r5, 0; setp.ne.u32 %%p4, %%r2, 0; and.pred %%p4, %%p3, %%p4;\n");
        fprintf(out, "    @%%p3 mul.wide.u32 %%rd8, %%r6, 4; @%%p3 add.u64 %%rd9, %%rd2, %%rd8;\n");
        fprintf(out, "    @%%p4 ld.f32 %%f3, [%%rd9]; @%%p4 add.f32 %%f0, %%f3, %%f0; @%%p3 st.f32 [%%rd9], %%f0;\n");
        fprintf(out, "    add.u32 %%r6, %%r6, 4; bra L_mv16_r;\n");
        fprintf(out, "L_mv16_d:\n    ret;\n}\n\n");
    } else if (fmt == GGUF_PTX_Q8_0) {
        fprintf(out, ".func zcc_aot_matvec_q8_0(.param .u64 _p0, .param .u64 _p1, .param .u64 _p2, .param .u32 _p3, .param .u32 _p4, .param .u32 _p5) {\n");
        fprintf(out, "    .reg .pred %%p<5>; .reg .b32 %%r<13>; .reg .b64 %%rd<11>; .reg .f32 %%f<5>;\n");
        fprintf(out, "    ld.param.u64 %%rd0, [_p0]; ld.param.u64 %%rd1, [_p1]; ld.param.u64 %%rd2, [_p2];\n");
        fprintf(out, "    ld.param.u32 %%r0, [_p3]; ld.param.u32 %%r1, [_p4]; ld.param.u32 %%r2, [_p5];\n");
        fprintf(out, "    mov.u32 %%r3, %%tid.x; shr.u32 %%r4, %%r3, 5; and.b32 %%r5, %%r3, 31;\n");
        fprintf(out, "    shr.u32 %%r6, %%r1, 5; mul.lo.u32 %%r7, %%r6, 34; mov.u32 %%r8, %%r4;\n");
        fprintf(out, "L_mvq_r:\n    setp.ge.u32 %%p0, %%r8, %%r0; @%%p0 bra L_mvq_d;\n");
        fprintf(out, "    mul.wide.u32 %%rd3, %%r8, %%r7; add.u64 %%rd4, %%rd0, %%rd3;\n");
        fprintf(out, "    mov.f32 %%f0, 0f00000000; mov.u32 %%r9, 0;\n");
        fprintf(out, "L_mvq_b:\n    setp.ge.u32 %%p1, %%r9, %%r6; @%%p1 bra L_mvq_bd;\n");
        fprintf(out, "    mul.wide.u32 %%rd5, %%r9, 34; add.u64 %%rd6, %%rd4, %%rd5;\n");
        fprintf(out, "    ld.u8 %%r10, [%%rd6]; ld.u8 %%r11, [%%rd6+1]; shl.b32 %%r11, %%r11, 8; or.b32 %%r10, %%r10, %%r11;\n");
        fprintf(out, "    { .reg .b16 %%hb; cvt.u16.u32 %%hb, %%r10; cvt.f32.f16 %%f1, %%hb; }\n");
        fprintf(out, "    add.u32 %%r12, %%r5, 2; cvt.u64.u32 %%rd7, %%r12; add.u64 %%rd7, %%rd6, %%rd7;\n");
        fprintf(out, "    ld.u8 %%r10, [%%rd7]; setp.ge.u32 %%p2, %%r10, 128; @%%p2 sub.s32 %%r10, %%r10, 256; cvt.rn.f32.s32 %%f2, %%r10;\n");
        fprintf(out, "    mul.f32 %%f1, %%f1, %%f2; shl.b32 %%r12, %%r9, 5; add.u32 %%r12, %%r12, %%r5;\n");
        fprintf(out, "    mul.wide.u32 %%rd8, %%r12, 4; add.u64 %%rd8, %%rd1, %%rd8; ld.f32 %%f3, [%%rd8];\n");
        fprintf(out, "    fma.rn.f32 %%f0, %%f1, %%f3, %%f0; add.u32 %%r9, %%r9, 1; bra L_mvq_b;\n");
        fprintf(out, "L_mvq_bd:\n");
        fprintf(out, "    { .param .f32 _wr, _wa; st.param.f32 [_wa], %%f0; call.uni (_wr), zcc_aot_warp_sum, (_wa); ld.param.f32 %%f0, [_wr]; }\n");
        fprintf(out, "    setp.eq.u32 %%p3, %%r5, 0; setp.ne.u32 %%p4, %%r2, 0; and.pred %%p4, %%p3, %%p4;\n");
        fprintf(out, "    @%%p3 mul.wide.u32 %%rd9, %%r8, 4; @%%p3 add.u64 %%rd10, %%rd2, %%rd9;\n");
        fprintf(out, "    @%%p4 ld.f32 %%f4, [%%rd10]; @%%p4 add.f32 %%f0, %%f4, %%f0; @%%p3 st.f32 [%%rd10], %%f0;\n");
        fprintf(out, "    add.u32 %%r8, %%r8, 4; bra L_mvq_r;\n");
        fprintf(out, "L_mvq_d:\n    ret;\n}\n\n");
    }
}

static void gguf_ptx_emit_tc_fns(FILE *out) {
    fprintf(out, ".func zcc_aot_f32_to_f16(.param .u64 _p0, .param .u64 _p1, .param .u32 _p2) {\n");
    fprintf(out, "    .reg .pred %%p0; .reg .b32 %%r<3>; .reg .b64 %%rd<5>; .reg .f32 %%f0;\n");
    fprintf(out, "    ld.param.u64 %%rd0, [_p0]; ld.param.u64 %%rd1, [_p1]; ld.param.u32 %%r0, [_p2];\n");
    fprintf(out, "    mov.u32 %%r1, %%tid.x;\n");
    fprintf(out, "L_c16:\n    setp.ge.u32 %%p0, %%r1, %%r0; @%%p0 bra L_c16_d;\n");
    fprintf(out, "    mul.wide.u32 %%rd2, %%r1, 4; add.u64 %%rd3, %%rd0, %%rd2; ld.f32 %%f0, [%%rd3];\n");
    fprintf(out, "    { .reg .b16 %%hb; cvt.rn.f16.f32 %%hb, %%f0; cvt.u32.u16 %%r2, %%hb; }\n");
    fprintf(out, "    mul.wide.u32 %%rd2, %%r1, 2; add.u64 %%rd4, %%rd1, %%rd2; st.u16 [%%rd4], %%r2;\n");
    fprintf(out, "    add.u32 %%r1, %%r1, 128; bra L_c16;\n");
    fprintf(out, "L_c16_d:\n    bar.sync 0; ret;\n}\n\n");

    fprintf(out, ".func zcc_aot_gemm_tc_f16(.param .u64 _p0, .param .u64 _p1, .param .u64 _p2, .param .u32 _p3, .param .u32 _p4, .param .u32 _p5) {\n");
    fprintf(out, "    .reg .pred %%p<8>; .reg .b32 %%r<20>; .reg .b64 %%rd<9>; .reg .f32 %%f<6>;\n");
    fprintf(out, "    ld.param.u64 %%rd0, [_p0]; ld.param.u64 %%rd1, [_p1]; ld.param.u64 %%rd2, [_p2];\n");
    fprintf(out, "    ld.param.u32 %%r0, [_p3]; ld.param.u32 %%r1, [_p4]; ld.param.u32 %%r2, [_p5];\n");
    fprintf(out, "    mov.u32 %%r3, %%tid.x; shr.u32 %%r4, %%r3, 5; and.b32 %%r5, %%r3, 31;\n");
    fprintf(out, "    shr.u32 %%r6, %%r5, 2; and.b32 %%r7, %%r5, 3; shl.b32 %%r8, %%r7, 1;\n");
    fprintf(out, "    sub.u32 %%r9, %%r0, 1; shl.b32 %%r10, %%r4, 3; setp.ne.u32 %%p7, %%r2, 0;\n");
    fprintf(out, "L_tc_t:\n    setp.ge.u32 %%p0, %%r10, %%r0; @%%p0 bra L_tc_d;\n");
    fprintf(out, "    add.u32 %%r11, %%r10, %%r6; min.u32 %%r11, %%r11, %%r9;\n");
    fprintf(out, "    mul.wide.u32 %%rd3, %%r11, %%r1; mov.u32 %%r12, 0;\n");
    fprintf(out, "    mov.f32 %%f0, 0f00000000; mov.f32 %%f1, 0f00000000; mov.f32 %%f2, 0f00000000; mov.f32 %%f3, 0f00000000;\n");
    fprintf(out, "L_tc_k:\n    add.u32 %%r13, %%r12, 16; setp.gt.u32 %%p1, %%r13, %%r1; @%%p1 bra L_tc_kd;\n");
    fprintf(out, "    add.u32 %%r14, %%r12, %%r8; mul.wide.u32 %%rd4, %%r14, 2; add.u64 %%rd5, %%rd0, %%rd4;\n");
    fprintf(out, "    ld.u32 %%r15, [%%rd5]; mov.u32 %%r16, %%r15;\n");
    fprintf(out, "    add.u32 %%r14, %%r14, 8; mul.wide.u32 %%rd4, %%r14, 2; add.u64 %%rd5, %%rd0, %%rd4;\n");
    fprintf(out, "    ld.u32 %%r17, [%%rd5]; mov.u32 %%r18, %%r17;\n");
    fprintf(out, "    add.u32 %%r14, %%r12, %%r8; cvt.u64.u32 %%rd4, %%r14; add.u64 %%rd6, %%rd3, %%rd4;\n");
    fprintf(out, "    shl.b64 %%rd7, %%rd6, 1; add.u64 %%rd8, %%rd1, %%rd7; ld.u32 %%r19, [%%rd8];\n");
    fprintf(out, "    add.u64 %%rd8, %%rd8, 16; ld.u32 %%r14, [%%rd8];\n");
    fprintf(out, "    mma.sync.aligned.m16n8k16.row.col.f32.f16.f16.f32 {%%f0, %%f1, %%f2, %%f3}, {%%r15, %%r16, %%r17, %%r18}, {%%r19, %%r14}, {%%f0, %%f1, %%f2, %%f3};\n");
    fprintf(out, "    mov.u32 %%r12, %%r13; bra L_tc_k;\n");
    fprintf(out, "L_tc_kd:\n    setp.eq.u32 %%p2, %%r6, 0;\n");
    fprintf(out, "    add.u32 %%r14, %%r10, %%r8; setp.lt.u32 %%p3, %%r14, %%r0; and.pred %%p4, %%p2, %%p3;\n");
    fprintf(out, "    @%%p4 mul.wide.u32 %%rd4, %%r14, 4; @%%p4 add.u64 %%rd5, %%rd2, %%rd4;\n");
    fprintf(out, "    and.pred %%p5, %%p4, %%p7; @%%p5 ld.f32 %%f4, [%%rd5]; @%%p5 add.f32 %%f0, %%f4, %%f0; @%%p4 st.f32 [%%rd5], %%f0;\n");
    fprintf(out, "    add.u32 %%r14, %%r14, 1; setp.lt.u32 %%p3, %%r14, %%r0; and.pred %%p4, %%p2, %%p3;\n");
    fprintf(out, "    @%%p4 mul.wide.u32 %%rd4, %%r14, 4; @%%p4 add.u64 %%rd5, %%rd2, %%rd4;\n");
    fprintf(out, "    and.pred %%p5, %%p4, %%p7; @%%p5 ld.f32 %%f5, [%%rd5]; @%%p5 add.f32 %%f1, %%f5, %%f1; @%%p4 st.f32 [%%rd5], %%f1;\n");
    fprintf(out, "    add.u32 %%r10, %%r10, 32; bra L_tc_t;\n");
    fprintf(out, "L_tc_d:\n    ret;\n}\n\n");
}

static void gguf_ptx_emit_rope_kv_fn(FILE *out, const GgufPtxModel *m) {
    uint32_t half_hd = m->head_dim >> 1;
    uint32_t total_pairs = (m->n_heads + m->n_kv_heads) * half_hd;
    uint32_t theta_u = gguf_ptx_f32_bits(m->rope_theta);
    uint32_t hd_u = gguf_ptx_f32_bits((float)m->head_dim);
    fprintf(out, ".func zcc_aot_rope_kv(.param .u64 _p0, .param .u64 _p1, .param .u64 _p2, .param .u64 _p3, .param .u64 _p4, .param .u32 _p5, .param .u32 _p6) {\n");
    fprintf(out, "    .reg .pred %%p0, %%p1; .reg .b32 %%r<9>; .reg .b64 %%rd<11>; .reg .f32 %%f<10>;\n");
    fprintf(out, "    ld.param.u64 %%rd0, [_p0]; ld.param.u64 %%rd1, [_p1]; ld.param.u64 %%rd2, [_p2];\n");
    fprintf(out, "    ld.param.u64 %%rd3, [_p3]; ld.param.u64 %%rd4, [_p4];\n");
    fprintf(out, "    ld.param.u32 %%r0, [_p5]; ld.param.u32 %%r1, [_p6];\n");
    fprintf(out, "    lg2.approx.f32 %%f0, 0f%08X; cvt.rn.f32.u32 %%f1, %%r0; mov.f32 %%f9, 0f%08X;\n", (unsigned int)theta_u, (unsigned int)hd_u);
    fprintf(out, "    mov.u32 %%r2, %%tid.x;\n");
    fprintf(out, "L_rp:\n    setp.ge.u32 %%p0, %%r2, %u; @%%p0 bra L_rp_d;\n", (unsigned int)total_pairs);
    fprintf(out, "    div.u32 %%r3, %%r2, %u; rem.u32 %%r4, %%r2, %u;\n", (unsigned int)half_hd, (unsigned int)half_hd);
    fprintf(out, "    setp.lt.u32 %%p1, %%r3, %u; sub.u32 %%r5, %%r3, %u;\n", (unsigned int)m->n_heads, (unsigned int)m->n_heads);
    fprintf(out, "    selp.u32 %%r6, %%r3, %%r5, %%p1; selp.u64 %%rd5, %%rd0, %%rd1, %%p1;\n");
    fprintf(out, "    mul.lo.u32 %%r6, %%r6, %u; shl.b32 %%r7, %%r4, 1; add.u32 %%r6, %%r6, %%r7;\n", (unsigned int)m->head_dim);
    fprintf(out, "    mul.wide.u32 %%rd6, %%r6, 4; add.u64 %%rd5, %%rd5, %%rd6;\n");
    fprintf(out, "    cvt.rn.f32.u32 %%f2, %%r7; neg.f32 %%f2, %%f2; div.rn.f32 %%f2, %%f2, %%f9;\n");
    fprintf(out, "    mul.f32 %%f2, %%f2, %%f0; ex2.approx.f32 %%f2, %%f2; mul.f32 %%f2, %%f1, %%f2;\n");
    fprintf(out, "    cos.approx.f32 %%f3, %%f2; sin.approx.f32 %%f4, %%f2;\n");
    fprintf(out, "    ld.f32 %%f5, [%%rd5]; ld.f32 %%f6, [%%rd5+4];\n");
    fprintf(out, "    mul.f32 %%f7, %%f5, %%f3; mul.f32 %%f8, %%f6, %%f4; sub.f32 %%f7, %%f7, %%f8;\n");
    fprintf(out, "    mul.f32 %%f8, %%f5, %%f4; fma.rn.f32 %%f8, %%f6, %%f3, %%f8;\n");
    fprintf(out, "    st.f32 [%%rd5], %%f7; st.f32 [%%rd5+4], %%f8;\n");
    fprintf(out, "    add.u32 %%r2, %%r2, 128; bra L_rp;\n");
    fprintf(out, "L_rp_d:\n    bar.sync 0;\n");
    fprintf(out, "    mad.lo.u32 %%r8, %%r0, %u, %%r1; mul.wide.u32 %%rd7, %%r8, 4;\n", (unsigned int)m->kv_dim);
    fprintf(out, "    add.u64 %%rd3, %%rd3, %%rd7; add.u64 %%rd4, %%rd4, %%rd7; mov.u32 %%r2, %%tid.x;\n");
    fprintf(out, "L_kv:\n    setp.ge.u32 %%p0, %%r2, %u; @%%p0 bra L_kv_d;\n", (unsigned int)m->kv_dim);
    fprintf(out, "    mul.wide.u32 %%rd8, %%r2, 4; add.u64 %%rd9, %%rd1, %%rd8; ld.f32 %%f5, [%%rd9];\n");
    fprintf(out, "    add.u64 %%rd10, %%rd3, %%rd8; st.f32 [%%rd10], %%f5;\n");
    fprintf(out, "    add.u64 %%rd9, %%rd2, %%rd8; ld.f32 %%f6, [%%rd9];\n");
    fprintf(out, "    add.u64 %%rd10, %%rd4, %%rd8; st.f32 [%%rd10], %%f6;\n");
    fprintf(out, "    add.u32 %%r2, %%r2, 128; bra L_kv;\n");
    fprintf(out, "L_kv_d:\n    bar.sync 0; ret;\n}\n\n");
}

static void gguf_ptx_emit_attn_fn(FILE *out, const GgufPtxModel *m) {
    uint32_t kv_group = m->n_heads / m->n_kv_heads;
    uint32_t hd_u = gguf_ptx_f32_bits((float)m->head_dim);
    fprintf(out, ".func zcc_aot_attn_gqa(.param .u64 _p0, .param .u64 _p1, .param .u64 _p2, .param .u64 _p3, .param .u64 _p4, .param .u32 _p5, .param .u32 _p6) {\n");
    fprintf(out, "    .reg .pred %%p<5>; .reg .b32 %%r<15>; .reg .b64 %%rd<13>; .reg .f32 %%f<8>;\n");
    fprintf(out, "    ld.param.u64 %%rd0, [_p0]; ld.param.u64 %%rd1, [_p1]; ld.param.u64 %%rd2, [_p2];\n");
    fprintf(out, "    ld.param.u64 %%rd3, [_p3]; ld.param.u64 %%rd4, [_p4];\n");
    fprintf(out, "    ld.param.u32 %%r0, [_p5]; ld.param.u32 %%r1, [_p6];\n");
    fprintf(out, "    mov.u32 %%r2, %%tid.x; shr.u32 %%r3, %%r2, 5; and.b32 %%r4, %%r2, 31;\n");
    fprintf(out, "    mul.lo.u32 %%r5, %%r3, %u; cvt.u64.u32 %%rd5, %%r5; add.u64 %%rd3, %%rd3, %%rd5;\n", (unsigned int)(m->max_seq_len * 4U));
    fprintf(out, "    mov.f32 %%f0, 0f%08X; rsqrt.approx.f32 %%f0, %%f0; mov.u32 %%r6, %%r3;\n", (unsigned int)hd_u);
    fprintf(out, "L_ah:\n    setp.ge.u32 %%p0, %%r6, %u; @%%p0 bra L_ah_d;\n", (unsigned int)m->n_heads);
    fprintf(out, "    div.u32 %%r7, %%r6, %u; mul.lo.u32 %%r8, %%r7, %u;\n", (unsigned int)kv_group, (unsigned int)m->head_dim);
    fprintf(out, "    mul.lo.u32 %%r9, %%r6, %u; cvt.u64.u32 %%rd6, %%r9; shl.b64 %%rd6, %%rd6, 2;\n", (unsigned int)m->head_dim);
    fprintf(out, "    add.u64 %%rd7, %%rd0, %%rd6; add.u64 %%rd8, %%rd4, %%rd6;\n");
    fprintf(out, "    mov.u32 %%r10, 0;\n");
    fprintf(out, "L_at1:\n    setp.gt.u32 %%p1, %%r10, %%r1; @%%p1 bra L_at1_d;\n");
    fprintf(out, "    mad.lo.u32 %%r11, %%r10, %u, %%r0; add.u32 %%r11, %%r11, %%r8;\n", (unsigned int)m->kv_dim);
    fprintf(out, "    mul.wide.u32 %%rd9, %%r11, 4; add.u64 %%rd9, %%rd1, %%rd9;\n");
    fprintf(out, "    mov.f32 %%f1, 0f00000000; mov.u32 %%r12, 0;\n");
    fprintf(out, "L_ad1:\n    setp.ge.u32 %%p2, %%r12, %u; @%%p2 bra L_ad1_d;\n", (unsigned int)m->head_dim);
    fprintf(out, "    add.u32 %%r13, %%r12, %%r4; setp.lt.u32 %%p3, %%r13, %u;\n", (unsigned int)m->head_dim);
    fprintf(out, "    @%%p3 mul.wide.u32 %%rd10, %%r13, 4; @%%p3 add.u64 %%rd11, %%rd7, %%rd10; @%%p3 ld.f32 %%f2, [%%rd11];\n");
    fprintf(out, "    @%%p3 add.u64 %%rd12, %%rd9, %%rd10; @%%p3 ld.f32 %%f3, [%%rd12]; @%%p3 fma.rn.f32 %%f1, %%f2, %%f3, %%f1;\n");
    fprintf(out, "    add.u32 %%r12, %%r12, 32; bra L_ad1;\n");
    fprintf(out, "L_ad1_d:\n");
    fprintf(out, "    { .param .f32 _wr, _wa; st.param.f32 [_wa], %%f1; call.uni (_wr), zcc_aot_warp_sum, (_wa); ld.param.f32 %%f1, [_wr]; }\n");
    fprintf(out, "    mul.f32 %%f1, %%f1, %%f0; setp.eq.u32 %%p3, %%r4, 0;\n");
    fprintf(out, "    @%%p3 mul.wide.u32 %%rd10, %%r10, 4; @%%p3 add.u64 %%rd11, %%rd3, %%rd10; @%%p3 st.f32 [%%rd11], %%f1;\n");
    fprintf(out, "    bar.warp.sync 0xffffffff; add.u32 %%r10, %%r10, 1; bra L_at1;\n");
    fprintf(out, "L_at1_d:\n    mov.f32 %%f2, 0fF149F2CA; mov.u32 %%r12, 0;\n");
    fprintf(out, "L_amx:\n    setp.gt.u32 %%p1, %%r12, %%r1; @%%p1 bra L_amx_d;\n");
    fprintf(out, "    add.u32 %%r13, %%r12, %%r4; setp.le.u32 %%p3, %%r13, %%r1;\n");
    fprintf(out, "    @%%p3 mul.wide.u32 %%rd10, %%r13, 4; @%%p3 add.u64 %%rd11, %%rd3, %%rd10; @%%p3 ld.f32 %%f3, [%%rd11]; @%%p3 max.f32 %%f2, %%f2, %%f3;\n");
    fprintf(out, "    add.u32 %%r12, %%r12, 32; bra L_amx;\n");
    fprintf(out, "L_amx_d:\n");
    fprintf(out, "    { .param .f32 _wr, _wa; st.param.f32 [_wa], %%f2; call.uni (_wr), zcc_aot_warp_max, (_wa); ld.param.f32 %%f2, [_wr]; }\n");
    fprintf(out, "    mov.f32 %%f3, 0f00000000; mov.u32 %%r12, 0;\n");
    fprintf(out, "L_aex:\n    setp.gt.u32 %%p1, %%r12, %%r1; @%%p1 bra L_aex_d;\n");
    fprintf(out, "    add.u32 %%r13, %%r12, %%r4; setp.le.u32 %%p3, %%r13, %%r1;\n");
    fprintf(out, "    @%%p3 mul.wide.u32 %%rd10, %%r13, 4; @%%p3 add.u64 %%rd11, %%rd3, %%rd10; @%%p3 ld.f32 %%f4, [%%rd11];\n");
    fprintf(out, "    @%%p3 sub.f32 %%f4, %%f4, %%f2; @%%p3 mul.f32 %%f4, %%f4, 0f3FB8AA3B; @%%p3 ex2.approx.f32 %%f4, %%f4;\n");
    fprintf(out, "    @%%p3 st.f32 [%%rd11], %%f4; @%%p3 add.f32 %%f3, %%f3, %%f4;\n");
    fprintf(out, "    add.u32 %%r12, %%r12, 32; bra L_aex;\n");
    fprintf(out, "L_aex_d:\n");
    fprintf(out, "    { .param .f32 _wr, _wa; st.param.f32 [_wa], %%f3; call.uni (_wr), zcc_aot_warp_sum, (_wa); ld.param.f32 %%f3, [_wr]; }\n");
    fprintf(out, "    bar.warp.sync 0xffffffff;\n");
    fprintf(out, "    setp.gt.f32 %%p4, %%f3, 0f00000000; mov.f32 %%f4, 0f00000000; @%%p4 div.rn.f32 %%f4, 0f3F800000, %%f3;\n");
    fprintf(out, "    mov.u32 %%r12, 0;\n");
    fprintf(out, "L_anm:\n    setp.gt.u32 %%p1, %%r12, %%r1; @%%p1 bra L_anm_d;\n");
    fprintf(out, "    add.u32 %%r13, %%r12, %%r4; setp.le.u32 %%p3, %%r13, %%r1;\n");
    fprintf(out, "    @%%p3 mul.wide.u32 %%rd10, %%r13, 4; @%%p3 add.u64 %%rd11, %%rd3, %%rd10;\n");
    fprintf(out, "    @%%p3 ld.f32 %%f5, [%%rd11]; @%%p3 mul.f32 %%f5, %%f5, %%f4; @%%p3 st.f32 [%%rd11], %%f5;\n");
    fprintf(out, "    add.u32 %%r12, %%r12, 32; bra L_anm;\n");
    fprintf(out, "L_anm_d:\n    bar.warp.sync 0xffffffff; mov.u32 %%r12, 0;\n");
    fprintf(out, "L_avo:\n    setp.ge.u32 %%p1, %%r12, %u; @%%p1 bra L_avo_d;\n", (unsigned int)m->head_dim);
    fprintf(out, "    add.u32 %%r13, %%r12, %%r4; setp.ge.u32 %%p3, %%r13, %u; @%%p3 bra L_avo_n;\n", (unsigned int)m->head_dim);
    fprintf(out, "    mov.f32 %%f5, 0f00000000; mov.u32 %%r10, 0;\n");
    fprintf(out, "L_avt:\n    setp.gt.u32 %%p2, %%r10, %%r1; @%%p2 bra L_avt_d;\n");
    fprintf(out, "    mul.wide.u32 %%rd10, %%r10, 4; add.u64 %%rd11, %%rd3, %%rd10; ld.f32 %%f6, [%%rd11];\n");
    fprintf(out, "    mad.lo.u32 %%r14, %%r10, %u, %%r0; add.u32 %%r14, %%r14, %%r8; add.u32 %%r14, %%r14, %%r13;\n", (unsigned int)m->kv_dim);
    fprintf(out, "    mul.wide.u32 %%rd10, %%r14, 4; add.u64 %%rd11, %%rd2, %%rd10; ld.f32 %%f7, [%%rd11];\n");
    fprintf(out, "    fma.rn.f32 %%f5, %%f6, %%f7, %%f5; add.u32 %%r10, %%r10, 1; bra L_avt;\n");
    fprintf(out, "L_avt_d:\n    mul.wide.u32 %%rd10, %%r13, 4; add.u64 %%rd11, %%rd8, %%rd10; st.f32 [%%rd11], %%f5;\n");
    fprintf(out, "L_avo_n:\n    add.u32 %%r12, %%r12, 32; bra L_avo;\n");
    fprintf(out, "L_avo_d:\n    add.u32 %%r6, %%r6, 4; bra L_ah;\n");
    fprintf(out, "L_ah_d:\n    bar.sync 0; ret;\n}\n\n");
}

static void gguf_ptx_emit_swiglu_fn(FILE *out, const GgufPtxModel *m) {
    fprintf(out, ".func zcc_aot_swiglu(.param .u64 _p0, .param .u64 _p1) {\n");
    fprintf(out, "    .reg .pred %%p0; .reg .b32 %%r0; .reg .b64 %%rd<5>; .reg .f32 %%f<4>;\n");
    fprintf(out, "    ld.param.u64 %%rd0, [_p0]; ld.param.u64 %%rd1, [_p1]; mov.u32 %%r0, %%tid.x;\n");
    fprintf(out, "L_sw:\n    setp.ge.u32 %%p0, %%r0, %u; @%%p0 bra L_sw_d;\n", (unsigned int)m->hidden_dim);
    fprintf(out, "    mul.wide.u32 %%rd2, %%r0, 4; add.u64 %%rd3, %%rd0, %%rd2; ld.f32 %%f0, [%%rd3];\n");
    fprintf(out, "    add.u64 %%rd4, %%rd1, %%rd2; ld.f32 %%f1, [%%rd4];\n");
    fprintf(out, "    neg.f32 %%f2, %%f0; mul.f32 %%f2, %%f2, 0f3FB8AA3B; ex2.approx.f32 %%f2, %%f2;\n");
    fprintf(out, "    add.f32 %%f2, %%f2, 0f3F800000; div.rn.f32 %%f3, %%f0, %%f2; mul.f32 %%f3, %%f3, %%f1;\n");
    fprintf(out, "    st.f32 [%%rd3], %%f3; add.u32 %%r0, %%r0, 128; bra L_sw;\n");
    fprintf(out, "L_sw_d:\n    bar.sync 0; ret;\n}\n\n");
}

static void gguf_ptx_emit_ptr_add(FILE *out, const char *dst, const char *base, uint64_t off) {
    char ob[32];
    if (off == 0ULL) {
        fprintf(out, "    mov.u64 %s, %s;\n", dst, base);
    } else {
        gguf_ptx_fmt_u64_dec(off, ob, sizeof(ob));
        fprintf(out, "    add.u64 %s, %s, %s;\n", dst, base, ob);
    }
}

static void gguf_ptx_emit_call_linear(
    FILE *out,
    uint32_t wtype,
    uint64_t w_off,
    int use_tc,
    uint32_t src_smem_off,
    uint32_t src_f16_off,
    const char *dst_base,
    uint32_t dst_off,
    uint32_t N,
    uint32_t K,
    int add_mode
) {
    gguf_ptx_emit_ptr_add(out, "%rd_t0", "%rd_w", w_off);
    gguf_ptx_emit_ptr_add(out, "%rd_t1", "%rd_s", (uint64_t)(use_tc ? src_f16_off : src_smem_off));
    gguf_ptx_emit_ptr_add(out, "%rd_t2", dst_base, (uint64_t)dst_off);
    fprintf(out, "    mov.u32 %%r_t0, %u; mov.u32 %%r_t1, %u; mov.u32 %%r_t2, %d;\n",
            (unsigned int)N, (unsigned int)K, add_mode);
    if (use_tc) {
        fprintf(out, "    { .param .u64 _a0, _a1, _a2; .param .u32 _a3, _a4, _a5; "
                     "st.param.u64 [_a0], %%rd_t1; st.param.u64 [_a1], %%rd_t0; st.param.u64 [_a2], %%rd_t2; "
                     "st.param.u32 [_a3], %%r_t0; st.param.u32 [_a4], %%r_t1; st.param.u32 [_a5], %%r_t2; "
                     "call.uni zcc_aot_gemm_tc_f16, (_a0, _a1, _a2, _a3, _a4, _a5); }\n");
    } else {
        const char *mfn = (wtype == GGUF_PTX_F16) ? "zcc_aot_matvec_f16" :
                          (wtype == GGUF_PTX_Q8_0) ? "zcc_aot_matvec_q8_0" : "zcc_aot_matvec_f32";
        fprintf(out, "    { .param .u64 _a0, _a1, _a2; .param .u32 _a3, _a4, _a5; "
                     "st.param.u64 [_a0], %%rd_t0; st.param.u64 [_a1], %%rd_t1; st.param.u64 [_a2], %%rd_t2; "
                     "st.param.u32 [_a3], %%r_t0; st.param.u32 [_a4], %%r_t1; st.param.u32 [_a5], %%r_t2; "
                     "call.uni %s, (_a0, _a1, _a2, _a3, _a4, _a5); }\n", mfn);
    }
}

static void gguf_ptx_emit_call_rmsnorm(FILE *out, const GgufPtxModel *m, uint32_t dst_off, uint32_t src_off, uint64_t w_off) {
    gguf_ptx_emit_ptr_add(out, "%rd_t0", "%rd_s", (uint64_t)dst_off);
    gguf_ptx_emit_ptr_add(out, "%rd_t1", "%rd_s", (uint64_t)src_off);
    gguf_ptx_emit_ptr_add(out, "%rd_t2", "%rd_w", w_off);
    gguf_ptx_emit_ptr_add(out, "%rd_t3", "%rd_s", (uint64_t)m->smem_red);
    fprintf(out, "    { .param .u64 _a0, _a1, _a2, _a3; "
                 "st.param.u64 [_a0], %%rd_t0; st.param.u64 [_a1], %%rd_t1; "
                 "st.param.u64 [_a2], %%rd_t2; st.param.u64 [_a3], %%rd_t3; "
                 "call.uni zcc_aot_rmsnorm, (_a0, _a1, _a2, _a3); }\n");
}

static void gguf_ptx_emit_call_f32_to_f16(FILE *out, uint32_t src_off, uint32_t dst_off, uint32_t n) {
    gguf_ptx_emit_ptr_add(out, "%rd_t0", "%rd_s", (uint64_t)src_off);
    gguf_ptx_emit_ptr_add(out, "%rd_t1", "%rd_s", (uint64_t)dst_off);
    fprintf(out, "    mov.u32 %%r_t0, %u;\n", (unsigned int)n);
    fprintf(out, "    { .param .u64 _a0, _a1; .param .u32 _a2; "
                 "st.param.u64 [_a0], %%rd_t0; st.param.u64 [_a1], %%rd_t1; st.param.u32 [_a2], %%r_t0; "
                 "call.uni zcc_aot_f32_to_f16, (_a0, _a1, _a2); }\n");
}

static void gguf_ptx_emit_mc_embed_fn(FILE *out, const GgufPtxModel *m, uint32_t fmt) {
    uint32_t dim = m->dim;
    const char *suf = (fmt == GGUF_PTX_F16) ? "f16" :
                      (fmt == GGUF_PTX_BF16) ? "bf16" :
                      (fmt == GGUF_PTX_Q8_0) ? "q8_0" :
                      (fmt == GGUF_PTX_Q4_0) ? "q4_0" :
                      (fmt == GGUF_PTX_Q4_K) ? "q4_k" :
                      (fmt == GGUF_PTX_Q6_K) ? "q6_k" : "f32";
    fprintf(out, ".visible .entry zcc_aot_mc_embed_%s(.param .u64 _pw, .param .u64 _pwoff, .param .u64 _pout, .param .u64 _pargs) {\n", suf);
    fprintf(out, "    .reg .pred %%p<4>; .reg .b32 %%r<24>; .reg .b64 %%rd<14>; .reg .f32 %%f<6>;\n");
    fprintf(out, "    ld.param.u64 %%rd0, [_pw]; ld.param.u64 %%rd1, [_pwoff]; add.u64 %%rd0, %%rd0, %%rd1;\n");
    fprintf(out, "    ld.param.u64 %%rd2, [_pout]; ld.param.u64 %%rd3, [_pargs];\n");
    fprintf(out, "    ld.u32 %%r0, [%%rd3];\n");
    fprintf(out, "    mov.u32 %%r1, %%ctaid.x; mov.u32 %%r2, %%tid.x; mad.lo.u32 %%r2, %%r1, 256, %%r2;\n");
    fprintf(out, "    setp.ge.u32 %%p0, %%r2, %u; @%%p0 bra L_mce_%s_d;\n", (unsigned int)dim, suf);
    if (fmt == GGUF_PTX_F32) {
        fprintf(out, "    mul.wide.u32 %%rd4, %%r0, %u; shl.b64 %%rd4, %%rd4, 2; add.u64 %%rd0, %%rd0, %%rd4;\n", (unsigned int)dim);
        fprintf(out, "    mul.wide.u32 %%rd5, %%r2, 4; add.u64 %%rd6, %%rd0, %%rd5; ld.f32 %%f0, [%%rd6];\n");
    } else if (fmt == GGUF_PTX_F16 || fmt == GGUF_PTX_BF16) {
        fprintf(out, "    mul.wide.u32 %%rd4, %%r0, %u; shl.b64 %%rd4, %%rd4, 1; add.u64 %%rd0, %%rd0, %%rd4;\n", (unsigned int)dim);
        fprintf(out, "    mul.wide.u32 %%rd5, %%r2, 2; add.u64 %%rd6, %%rd0, %%rd5; ld.u16 %%r3, [%%rd6];\n");
        if (fmt == GGUF_PTX_F16) fprintf(out, "    { .reg .b16 %%hb; cvt.u16.u32 %%hb, %%r3; cvt.f32.f16 %%f0, %%hb; }\n");
        else fprintf(out, "    shl.b32 %%r3, %%r3, 16; mov.b32 %%f0, %%r3;\n");
    } else if (fmt == GGUF_PTX_Q8_0) {
        uint32_t row_bytes = (dim >> 5) * 34U;
        fprintf(out, "    mov.u32 %%r1, %u; mul.wide.u32 %%rd4, %%r0, %%r1; add.u64 %%rd0, %%rd0, %%rd4;\n", (unsigned int)row_bytes);
        fprintf(out, "    shr.u32 %%r3, %%r2, 5; and.b32 %%r4, %%r2, 31;\n");
        fprintf(out, "    mul.wide.u32 %%rd5, %%r3, 34; add.u64 %%rd6, %%rd0, %%rd5;\n");
        fprintf(out, "    ld.u8 %%r5, [%%rd6]; ld.u8 %%r6, [%%rd6+1]; shl.b32 %%r6, %%r6, 8; or.b32 %%r5, %%r5, %%r6;\n");
        fprintf(out, "    { .reg .b16 %%hb; cvt.u16.u32 %%hb, %%r5; cvt.f32.f16 %%f1, %%hb; }\n");
        fprintf(out, "    add.u32 %%r4, %%r4, 2; cvt.u64.u32 %%rd7, %%r4; add.u64 %%rd8, %%rd6, %%rd7;\n");
        fprintf(out, "    ld.u8 %%r7, [%%rd8]; setp.ge.u32 %%p1, %%r7, 128; @%%p1 sub.s32 %%r7, %%r7, 256;\n");
        fprintf(out, "    cvt.rn.f32.s32 %%f2, %%r7; mul.f32 %%f0, %%f1, %%f2;\n");
    } else if (fmt == GGUF_PTX_Q4_0) {
        uint32_t row_bytes = (dim >> 5) * 18U;
        fprintf(out, "    mov.u32 %%r1, %u; mul.wide.u32 %%rd4, %%r0, %%r1; add.u64 %%rd0, %%rd0, %%rd4;\n", (unsigned int)row_bytes);
        fprintf(out, "    shr.u32 %%r3, %%r2, 5; and.b32 %%r4, %%r2, 31; and.b32 %%r5, %%r4, 15;\n");
        fprintf(out, "    mul.wide.u32 %%rd5, %%r3, 18; add.u64 %%rd6, %%rd0, %%rd5;\n");
        fprintf(out, "    ld.u8 %%r6, [%%rd6]; ld.u8 %%r7, [%%rd6+1]; shl.b32 %%r7, %%r7, 8; or.b32 %%r6, %%r6, %%r7;\n");
        fprintf(out, "    { .reg .b16 %%hb; cvt.u16.u32 %%hb, %%r6; cvt.f32.f16 %%f1, %%hb; }\n");
        fprintf(out, "    add.u32 %%r5, %%r5, 2; cvt.u64.u32 %%rd7, %%r5; add.u64 %%rd8, %%rd6, %%rd7; ld.u8 %%r8, [%%rd8];\n");
        fprintf(out, "    and.b32 %%r9, %%r8, 15; shr.u32 %%r10, %%r8, 4;\n");
        fprintf(out, "    setp.lt.u32 %%p1, %%r4, 16; selp.u32 %%r11, %%r9, %%r10, %%p1; sub.s32 %%r11, %%r11, 8;\n");
        fprintf(out, "    cvt.rn.f32.s32 %%f2, %%r11; mul.f32 %%f0, %%f1, %%f2;\n");
    } else if (fmt == GGUF_PTX_Q4_K) {
        uint32_t row_bytes = (dim >> 8) * 144U;
        fprintf(out, "    mov.u32 %%r1, %u; mul.wide.u32 %%rd4, %%r0, %%r1; add.u64 %%rd0, %%rd0, %%rd4;\n", (unsigned int)row_bytes);
        fprintf(out, "    shr.u32 %%r3, %%r2, 8; and.b32 %%r4, %%r2, 255;\n");
        fprintf(out, "    mul.wide.u32 %%rd5, %%r3, 144; add.u64 %%rd6, %%rd0, %%rd5;\n");
        fprintf(out, "    ld.u8 %%r5, [%%rd6]; ld.u8 %%r6, [%%rd6+1]; shl.b32 %%r6, %%r6, 8; or.b32 %%r5, %%r5, %%r6;\n");
        fprintf(out, "    { .reg .b16 %%hb; cvt.u16.u32 %%hb, %%r5; cvt.f32.f16 %%f1, %%hb; }\n");
        fprintf(out, "    ld.u8 %%r5, [%%rd6+2]; ld.u8 %%r6, [%%rd6+3]; shl.b32 %%r6, %%r6, 8; or.b32 %%r5, %%r5, %%r6;\n");
        fprintf(out, "    { .reg .b16 %%hb; cvt.u16.u32 %%hb, %%r5; cvt.f32.f16 %%f2, %%hb; }\n");
        fprintf(out, "    shr.u32 %%r7, %%r4, 6; shr.u32 %%r8, %%r4, 5; and.b32 %%r8, %%r8, 1; and.b32 %%r9, %%r4, 31;\n");
        fprintf(out, "    shl.b32 %%r10, %%r7, 1; or.b32 %%r10, %%r10, %%r8;\n");
        fprintf(out, "    add.u64 %%rd7, %%rd6, 4; setp.lt.u32 %%p1, %%r10, 4; @%%p1 bra L_eq4k_lo;\n");
        fprintf(out, "    add.u32 %%r11, %%r10, 4; cvt.u64.u32 %%rd8, %%r11; add.u64 %%rd9, %%rd7, %%rd8; ld.u8 %%r12, [%%rd9];\n");
        fprintf(out, "    sub.u32 %%r13, %%r10, 4; cvt.u64.u32 %%rd8, %%r13; add.u64 %%rd9, %%rd7, %%rd8; ld.u8 %%r14, [%%rd9];\n");
        fprintf(out, "    cvt.u64.u32 %%rd8, %%r10; add.u64 %%rd9, %%rd7, %%rd8; ld.u8 %%r15, [%%rd9];\n");
        fprintf(out, "    and.b32 %%r16, %%r12, 15; shr.u32 %%r14, %%r14, 6; shl.b32 %%r14, %%r14, 4; or.b32 %%r16, %%r16, %%r14;\n");
        fprintf(out, "    shr.u32 %%r17, %%r12, 4; shr.u32 %%r15, %%r15, 6; shl.b32 %%r15, %%r15, 4; or.b32 %%r17, %%r17, %%r15;\n");
        fprintf(out, "    bra L_eq4k_sc;\nL_eq4k_lo:\n");
        fprintf(out, "    cvt.u64.u32 %%rd8, %%r10; add.u64 %%rd9, %%rd7, %%rd8; ld.u8 %%r16, [%%rd9]; and.b32 %%r16, %%r16, 63;\n");
        fprintf(out, "    add.u32 %%r11, %%r10, 4; cvt.u64.u32 %%rd8, %%r11; add.u64 %%rd9, %%rd7, %%rd8; ld.u8 %%r17, [%%rd9]; and.b32 %%r17, %%r17, 63;\n");
        fprintf(out, "L_eq4k_sc:\n");
        fprintf(out, "    shl.b32 %%r18, %%r7, 5; add.u32 %%r18, %%r18, %%r9; add.u32 %%r18, %%r18, 16;\n");
        fprintf(out, "    cvt.u64.u32 %%rd8, %%r18; add.u64 %%rd9, %%rd6, %%rd8; ld.u8 %%r19, [%%rd9];\n");
        fprintf(out, "    and.b32 %%r20, %%r19, 15; shr.u32 %%r21, %%r19, 4; setp.eq.u32 %%p2, %%r8, 0; selp.u32 %%r22, %%r20, %%r21, %%p2;\n");
        fprintf(out, "    cvt.rn.f32.u32 %%f3, %%r16; mul.f32 %%f3, %%f1, %%f3; cvt.rn.f32.u32 %%f4, %%r22; mul.f32 %%f3, %%f3, %%f4;\n");
        fprintf(out, "    cvt.rn.f32.u32 %%f5, %%r17; mul.f32 %%f5, %%f2, %%f5; sub.f32 %%f0, %%f3, %%f5;\n");
    } else if (fmt == GGUF_PTX_Q6_K) {
        uint32_t row_bytes = (dim >> 8) * 210U;
        fprintf(out, "    mov.u32 %%r1, %u; mul.wide.u32 %%rd4, %%r0, %%r1; add.u64 %%rd0, %%rd0, %%rd4;\n", (unsigned int)row_bytes);
        fprintf(out, "    shr.u32 %%r3, %%r2, 8; and.b32 %%r4, %%r2, 255;\n");
        fprintf(out, "    mul.wide.u32 %%rd5, %%r3, 210; add.u64 %%rd6, %%rd0, %%rd5;\n");
        fprintf(out, "    ld.u8 %%r5, [%%rd6+208]; ld.u8 %%r6, [%%rd6+209]; shl.b32 %%r6, %%r6, 8; or.b32 %%r5, %%r5, %%r6;\n");
        fprintf(out, "    { .reg .b16 %%hb; cvt.u16.u32 %%hb, %%r5; cvt.f32.f16 %%f1, %%hb; }\n");
        fprintf(out, "    shr.u32 %%r7, %%r4, 7; and.b32 %%r8, %%r4, 127; shr.u32 %%r9, %%r8, 5; and.b32 %%r10, %%r8, 31; shr.u32 %%r11, %%r10, 4;\n");
        fprintf(out, "    shl.b32 %%r12, %%r7, 6; and.b32 %%r13, %%r9, 1; shl.b32 %%r13, %%r13, 5; add.u32 %%r12, %%r12, %%r13; add.u32 %%r12, %%r12, %%r10;\n");
        fprintf(out, "    cvt.u64.u32 %%rd7, %%r12; add.u64 %%rd8, %%rd6, %%rd7; ld.u8 %%r14, [%%rd8];\n");
        fprintf(out, "    and.b32 %%r15, %%r14, 15; shr.u32 %%r16, %%r14, 4; setp.ge.u32 %%p1, %%r9, 2; selp.u32 %%r17, %%r16, %%r15, %%p1;\n");
        fprintf(out, "    shl.b32 %%r12, %%r7, 5; add.u32 %%r12, %%r12, %%r10; add.u32 %%r12, %%r12, 128;\n");
        fprintf(out, "    cvt.u64.u32 %%rd7, %%r12; add.u64 %%rd8, %%rd6, %%rd7; ld.u8 %%r18, [%%rd8];\n");
        fprintf(out, "    shl.b32 %%r19, %%r9, 1; shr.u32 %%r18, %%r18, %%r19; and.b32 %%r18, %%r18, 3; shl.b32 %%r18, %%r18, 4;\n");
        fprintf(out, "    or.b32 %%r17, %%r17, %%r18; sub.s32 %%r17, %%r17, 32;\n");
        fprintf(out, "    shl.b32 %%r12, %%r7, 3; add.u32 %%r12, %%r12, %%r11; add.u32 %%r12, %%r12, %%r19; add.u32 %%r12, %%r12, 192;\n");
        fprintf(out, "    cvt.u64.u32 %%rd7, %%r12; add.u64 %%rd8, %%rd6, %%rd7; ld.u8 %%r20, [%%rd8];\n");
        fprintf(out, "    setp.ge.u32 %%p2, %%r20, 128; @%%p2 sub.s32 %%r20, %%r20, 256;\n");
        fprintf(out, "    cvt.rn.f32.s32 %%f2, %%r20; cvt.rn.f32.s32 %%f3, %%r17; mul.f32 %%f0, %%f1, %%f2; mul.f32 %%f0, %%f0, %%f3;\n");
    }
    fprintf(out, "    mul.wide.u32 %%rd10, %%r2, 4; add.u64 %%rd11, %%rd2, %%rd10; st.f32 [%%rd11], %%f0;\n");
    fprintf(out, "L_mce_%s_d:\n    ret;\n}\n\n", suf);
}

static void gguf_ptx_emit_mc_rmsnorm_fn(FILE *out, const GgufPtxModel *m) {
    uint32_t inv_dim_u = gguf_ptx_f32_bits(1.0f / (float)m->dim);
    uint32_t eps_u = gguf_ptx_f32_bits(m->rms_eps);
    fprintf(out, ".visible .entry zcc_aot_mc_rmsnorm(.param .u64 _pdst, .param .u64 _psrc, .param .u64 _pw, .param .u64 _pwoff) {\n");
    fprintf(out, "    .shared .align 4 .b8 s_red[16];\n");
    fprintf(out, "    .reg .pred %%p0, %%p1; .reg .b32 %%r<4>; .reg .b64 %%rd<10>; .reg .f32 %%f<6>;\n");
    fprintf(out, "    ld.param.u64 %%rd0, [_pdst]; ld.param.u64 %%rd1, [_psrc];\n");
    fprintf(out, "    ld.param.u64 %%rd2, [_pw]; ld.param.u64 %%rd9, [_pwoff]; add.u64 %%rd2, %%rd2, %%rd9;\n");
    fprintf(out, "    cvta.shared.u64 %%rd3, s_red;\n");
    fprintf(out, "    mov.u32 %%r0, %%tid.x; shr.u32 %%r1, %%r0, 5; and.b32 %%r2, %%r0, 31;\n");
    fprintf(out, "    mov.f32 %%f0, 0f00000000; mov.u32 %%r3, %%r0;\n");
    fprintf(out, "L_mcrn1:\n    setp.ge.u32 %%p0, %%r3, %u; @%%p0 bra L_mcrn1_d;\n", (unsigned int)m->dim);
    fprintf(out, "    mul.wide.u32 %%rd4, %%r3, 4; add.u64 %%rd5, %%rd1, %%rd4; ld.f32 %%f1, [%%rd5];\n");
    fprintf(out, "    fma.rn.f32 %%f0, %%f1, %%f1, %%f0; add.u32 %%r3, %%r3, 128; bra L_mcrn1;\n");
    fprintf(out, "L_mcrn1_d:\n");
    fprintf(out, "    { .param .f32 _wr, _wa; st.param.f32 [_wa], %%f0; call.uni (_wr), zcc_aot_warp_sum, (_wa); ld.param.f32 %%f0, [_wr]; }\n");
    fprintf(out, "    setp.eq.u32 %%p1, %%r2, 0;\n");
    fprintf(out, "    @%%p1 mul.wide.u32 %%rd4, %%r1, 4; @%%p1 add.u64 %%rd5, %%rd3, %%rd4; @%%p1 st.f32 [%%rd5], %%f0;\n");
    fprintf(out, "    bar.sync 0;\n");
    fprintf(out, "    ld.f32 %%f1, [%%rd3]; ld.f32 %%f2, [%%rd3+4]; ld.f32 %%f3, [%%rd3+8]; ld.f32 %%f4, [%%rd3+12];\n");
    fprintf(out, "    add.f32 %%f0, %%f1, %%f2; add.f32 %%f3, %%f3, %%f4; add.f32 %%f0, %%f0, %%f3;\n");
    fprintf(out, "    fma.rn.f32 %%f1, %%f0, 0f%08X, 0f%08X; rsqrt.approx.f32 %%f5, %%f1;\n", (unsigned int)inv_dim_u, (unsigned int)eps_u);
    fprintf(out, "    mov.u32 %%r3, %%r0;\n");
    fprintf(out, "L_mcrn2:\n    setp.ge.u32 %%p0, %%r3, %u; @%%p0 bra L_mcrn2_d;\n", (unsigned int)m->dim);
    fprintf(out, "    mul.wide.u32 %%rd4, %%r3, 4; add.u64 %%rd5, %%rd1, %%rd4; ld.f32 %%f1, [%%rd5];\n");
    fprintf(out, "    add.u64 %%rd6, %%rd2, %%rd4; ld.f32 %%f2, [%%rd6];\n");
    fprintf(out, "    mul.f32 %%f3, %%f1, %%f5; mul.f32 %%f3, %%f3, %%f2;\n");
    fprintf(out, "    add.u64 %%rd7, %%rd0, %%rd4; st.f32 [%%rd7], %%f3;\n");
    fprintf(out, "    add.u32 %%r3, %%r3, 128; bra L_mcrn2;\n");
    fprintf(out, "L_mcrn2_d:\n    ret;\n}\n\n");
}

static void gguf_ptx_emit_mc_matvec_fn(FILE *out, uint32_t fmt) {
    const char *suf = (fmt == GGUF_PTX_F16) ? "f16" :
                      (fmt == GGUF_PTX_BF16) ? "bf16" :
                      (fmt == GGUF_PTX_Q8_0) ? "q8_0" :
                      (fmt == GGUF_PTX_Q4_0) ? "q4_0" :
                      (fmt == GGUF_PTX_Q4_K) ? "q4_k" :
                      (fmt == GGUF_PTX_Q6_K) ? "q6_k" : "f32";
    fprintf(out, ".visible .entry zcc_aot_mc_matvec_%s(\n"
                 "    .param .u64 _pw, .param .u64 _pwoff, .param .u64 _px,\n"
                 "    .param .u64 _py, .param .u32 _pN, .param .u32 _pK, .param .u32 _padd) {\n", suf);
    fprintf(out, "    .reg .pred %%p<8>; .reg .b32 %%r<36>; .reg .b64 %%rd<16>; .reg .f32 %%f<20>;\n");
    fprintf(out, "    ld.param.u64 %%rd0, [_pw]; ld.param.u64 %%rd11, [_pwoff]; add.u64 %%rd0, %%rd0, %%rd11;\n");
    fprintf(out, "    ld.param.u64 %%rd1, [_px]; ld.param.u64 %%rd2, [_py];\n");
    fprintf(out, "    ld.param.u32 %%r0, [_pN]; ld.param.u32 %%r1, [_pK]; ld.param.u32 %%r2, [_padd];\n");
    /* 4 warps per CTA (block=128): row = (ctaid.x << 2) + (tid.x >> 5), lane = tid.x & 31 */
    fprintf(out, "    mov.u32 %%r3, %%ctaid.x; mov.u32 %%r4, %%tid.x;\n");
    fprintf(out, "    shr.u32 %%r5, %%r4, 5; shl.b32 %%r3, %%r3, 2; add.u32 %%r3, %%r3, %%r5; and.b32 %%r5, %%r4, 31;\n");
    fprintf(out, "    setp.ge.u32 %%p0, %%r3, %%r0; @%%p0 bra L_mcm_%s_d;\n", suf);

    if (fmt == GGUF_PTX_F32) {
        fprintf(out, "    mul.wide.u32 %%rd3, %%r3, %%r1; shl.b64 %%rd3, %%rd3, 2; add.u64 %%rd4, %%rd0, %%rd3;\n");
        fprintf(out, "    mov.f32 %%f0, 0f00000000; mov.u32 %%r6, 0;\n");
        fprintf(out, "    or.b64 %%rd14, %%rd4, %%rd1; and.b64 %%rd14, %%rd14, 15; setp.ne.u64 %%p5, %%rd14, 0; @%%p5 bra L_mcmf32_k;\n");
        fprintf(out, "L_mcmf32_v4:\n    add.u32 %%r8, %%r6, 128; setp.gt.u32 %%p1, %%r8, %%r1; @%%p1 bra L_mcmf32_k;\n");
        fprintf(out, "    shl.b32 %%r7, %%r5, 2; add.u32 %%r7, %%r6, %%r7; mul.wide.u32 %%rd5, %%r7, 4;\n");
        fprintf(out, "    add.u64 %%rd6, %%rd4, %%rd5; add.u64 %%rd7, %%rd1, %%rd5;\n");
        fprintf(out, "    ld.v4.f32 {%%f1, %%f2, %%f3, %%f4}, [%%rd6];\n");
        fprintf(out, "    ld.v4.f32 {%%f5, %%f6, %%f7, %%f8}, [%%rd7];\n");
        fprintf(out, "    fma.rn.f32 %%f0, %%f1, %%f5, %%f0; fma.rn.f32 %%f0, %%f2, %%f6, %%f0;\n");
        fprintf(out, "    fma.rn.f32 %%f0, %%f3, %%f7, %%f0; fma.rn.f32 %%f0, %%f4, %%f8, %%f0;\n");
        fprintf(out, "    mov.u32 %%r6, %%r8; bra L_mcmf32_v4;\n");
        fprintf(out, "L_mcmf32_k:\n    setp.ge.u32 %%p1, %%r6, %%r1; @%%p1 bra L_mcm_%s_s;\n", suf);
        fprintf(out, "    add.u32 %%r7, %%r6, %%r5; setp.lt.u32 %%p2, %%r7, %%r1;\n");
        fprintf(out, "    @%%p2 mul.wide.u32 %%rd5, %%r7, 4; @%%p2 add.u64 %%rd6, %%rd4, %%rd5; @%%p2 ld.f32 %%f1, [%%rd6];\n");
        fprintf(out, "    @%%p2 add.u64 %%rd7, %%rd1, %%rd5; @%%p2 ld.f32 %%f2, [%%rd7]; @%%p2 fma.rn.f32 %%f0, %%f1, %%f2, %%f0;\n");
        fprintf(out, "    add.u32 %%r6, %%r6, 32; bra L_mcmf32_k;\n");
    } else if (fmt == GGUF_PTX_F16 || fmt == GGUF_PTX_BF16) {
        fprintf(out, "    mul.wide.u32 %%rd3, %%r3, %%r1; shl.b64 %%rd3, %%rd3, 1; add.u64 %%rd4, %%rd0, %%rd3;\n");
        fprintf(out, "    mov.f32 %%f0, 0f00000000; mov.u32 %%r6, 0;\n");
        fprintf(out, "    or.b64 %%rd14, %%rd4, %%rd1; and.b64 %%rd14, %%rd14, 15; setp.ne.u64 %%p5, %%rd14, 0; @%%p5 bra L_mcm16_k;\n");
        fprintf(out, "L_mcm16_v4:\n    add.u32 %%r9, %%r6, 256; setp.gt.u32 %%p1, %%r9, %%r1; @%%p1 bra L_mcm16_k;\n");
        fprintf(out, "    shl.b32 %%r7, %%r5, 3; add.u32 %%r7, %%r6, %%r7;\n");
        fprintf(out, "    mul.wide.u32 %%rd5, %%r7, 2; add.u64 %%rd6, %%rd4, %%rd5;\n");
        fprintf(out, "    mul.wide.u32 %%rd8, %%r7, 4; add.u64 %%rd7, %%rd1, %%rd8;\n");
        fprintf(out, "    ld.v4.u32 {%%r12, %%r13, %%r14, %%r15}, [%%rd6];\n");
        fprintf(out, "    ld.v4.f32 {%%f5, %%f6, %%f7, %%f8}, [%%rd7];\n");
        fprintf(out, "    ld.v4.f32 {%%f9, %%f10, %%f11, %%f12}, [%%rd7+16];\n");
        for (int p = 0; p < 4; p++) {
            int wreg = 12 + p;
            int fbase = 5 + p * 2;
            if (fmt == GGUF_PTX_F16) {
                fprintf(out, "    and.b32 %%r16, %%r%d, 65535; shr.u32 %%r17, %%r%d, 16;\n", wreg, wreg);
                fprintf(out, "    { .reg .b16 %%ha, %%hb; cvt.u16.u32 %%ha, %%r16; cvt.f32.f16 %%f1, %%ha; cvt.u16.u32 %%hb, %%r17; cvt.f32.f16 %%f2, %%hb; }\n");
            } else {
                fprintf(out, "    shl.b32 %%r16, %%r%d, 16; mov.b32 %%f1, %%r16; and.b32 %%r17, %%r%d, 0xFFFF0000; mov.b32 %%f2, %%r17;\n", wreg, wreg);
            }
            fprintf(out, "    fma.rn.f32 %%f0, %%f1, %%f%d, %%f0; fma.rn.f32 %%f0, %%f2, %%f%d, %%f0;\n", fbase, fbase + 1);
        }
        fprintf(out, "    mov.u32 %%r6, %%r9; bra L_mcm16_v4;\n");
        fprintf(out, "L_mcm16_k:\n    setp.ge.u32 %%p1, %%r6, %%r1; @%%p1 bra L_mcm_%s_s;\n", suf);
        fprintf(out, "    add.u32 %%r7, %%r6, %%r5; setp.lt.u32 %%p2, %%r7, %%r1;\n");
        fprintf(out, "    @%%p2 mul.wide.u32 %%rd5, %%r7, 2; @%%p2 add.u64 %%rd6, %%rd4, %%rd5; @%%p2 ld.u16 %%r8, [%%rd6];\n");
        if (fmt == GGUF_PTX_F16) fprintf(out, "    { .reg .b16 %%hb; mov.u16 %%hb, 0; @%%p2 cvt.u16.u32 %%hb, %%r8; @%%p2 cvt.f32.f16 %%f1, %%hb; }\n");
        else fprintf(out, "    @%%p2 shl.b32 %%r8, %%r8, 16; @%%p2 mov.b32 %%f1, %%r8;\n");
        fprintf(out, "    @%%p2 mul.wide.u32 %%rd5, %%r7, 4; @%%p2 add.u64 %%rd7, %%rd1, %%rd5; @%%p2 ld.f32 %%f2, [%%rd7]; @%%p2 fma.rn.f32 %%f0, %%f1, %%f2, %%f0;\n");
        fprintf(out, "    add.u32 %%r6, %%r6, 32; bra L_mcm16_k;\n");
    } else if (fmt == GGUF_PTX_Q8_0) {
        fprintf(out, "    shr.u32 %%r6, %%r1, 5; mul.lo.u32 %%r7, %%r6, 34;\n");
        fprintf(out, "    mul.wide.u32 %%rd3, %%r3, %%r7; add.u64 %%rd4, %%rd0, %%rd3;\n");
        fprintf(out, "    mov.f32 %%f0, 0f00000000; mov.u32 %%r8, 0;\n");
        fprintf(out, "L_mcmq8_b:\n    setp.ge.u32 %%p1, %%r8, %%r6; @%%p1 bra L_mcm_%s_s;\n", suf);
        fprintf(out, "    mul.wide.u32 %%rd5, %%r8, 34; add.u64 %%rd6, %%rd4, %%rd5;\n");
        fprintf(out, "    ld.u8 %%r9, [%%rd6]; ld.u8 %%r10, [%%rd6+1]; shl.b32 %%r10, %%r10, 8; or.b32 %%r9, %%r9, %%r10;\n");
        fprintf(out, "    { .reg .b16 %%hb; cvt.u16.u32 %%hb, %%r9; cvt.f32.f16 %%f1, %%hb; }\n");
        fprintf(out, "    add.u32 %%r11, %%r5, 2; cvt.u64.u32 %%rd7, %%r11; add.u64 %%rd7, %%rd6, %%rd7;\n");
        fprintf(out, "    ld.u8 %%r9, [%%rd7]; setp.ge.u32 %%p2, %%r9, 128; @%%p2 sub.s32 %%r9, %%r9, 256; cvt.rn.f32.s32 %%f2, %%r9;\n");
        fprintf(out, "    mul.f32 %%f1, %%f1, %%f2; shl.b32 %%r11, %%r8, 5; add.u32 %%r11, %%r11, %%r5;\n");
        fprintf(out, "    mul.wide.u32 %%rd8, %%r11, 4; add.u64 %%rd8, %%rd1, %%rd8; ld.f32 %%f3, [%%rd8];\n");
        fprintf(out, "    fma.rn.f32 %%f0, %%f1, %%f3, %%f0; add.u32 %%r8, %%r8, 1; bra L_mcmq8_b;\n");
    } else if (fmt == GGUF_PTX_Q4_0) {
        fprintf(out, "    shr.u32 %%r6, %%r1, 5; mul.lo.u32 %%r7, %%r6, 18;\n");
        fprintf(out, "    mul.wide.u32 %%rd3, %%r3, %%r7; add.u64 %%rd4, %%rd0, %%rd3;\n");
        fprintf(out, "    and.b32 %%r8, %%r5, 15; add.u32 %%r9, %%r8, 2;\n");
        fprintf(out, "    mov.f32 %%f0, 0f00000000; mov.u32 %%r10, 0;\n");
        fprintf(out, "L_mcmq40_b:\n    setp.ge.u32 %%p1, %%r10, %%r6; @%%p1 bra L_mcm_%s_s;\n", suf);
        fprintf(out, "    mul.wide.u32 %%rd5, %%r10, 18; add.u64 %%rd6, %%rd4, %%rd5;\n");
        fprintf(out, "    ld.u8 %%r11, [%%rd6]; ld.u8 %%r12, [%%rd6+1]; shl.b32 %%r12, %%r12, 8; or.b32 %%r11, %%r11, %%r12;\n");
        fprintf(out, "    { .reg .b16 %%hb; cvt.u16.u32 %%hb, %%r11; cvt.f32.f16 %%f1, %%hb; }\n");
        fprintf(out, "    cvt.u64.u32 %%rd7, %%r9; add.u64 %%rd7, %%rd6, %%rd7; ld.u8 %%r13, [%%rd7];\n");
        fprintf(out, "    and.b32 %%r14, %%r13, 15; shr.u32 %%r15, %%r13, 4; setp.lt.u32 %%p2, %%r5, 16; selp.u32 %%r16, %%r14, %%r15, %%p2;\n");
        fprintf(out, "    sub.s32 %%r16, %%r16, 8; cvt.rn.f32.s32 %%f2, %%r16; mul.f32 %%f1, %%f1, %%f2;\n");
        fprintf(out, "    shl.b32 %%r17, %%r10, 5; add.u32 %%r17, %%r17, %%r5;\n");
        fprintf(out, "    mul.wide.u32 %%rd8, %%r17, 4; add.u64 %%rd8, %%rd1, %%rd8; ld.f32 %%f3, [%%rd8];\n");
        fprintf(out, "    fma.rn.f32 %%f0, %%f1, %%f3, %%f0; add.u32 %%r10, %%r10, 1; bra L_mcmq40_b;\n");
    } else if (fmt == GGUF_PTX_Q4_K) {
        fprintf(out, "    shr.u32 %%r6, %%r1, 8; mul.lo.u32 %%r7, %%r6, 144;\n");
        fprintf(out, "    mul.wide.u32 %%rd3, %%r3, %%r7; add.u64 %%rd4, %%rd0, %%rd3;\n");
        fprintf(out, "    mov.f32 %%f0, 0f00000000; mov.u32 %%r8, 0;\n");
        fprintf(out, "    or.b64 %%rd14, %%rd4, %%rd1; and.b64 %%rd14, %%rd14, 15; setp.ne.u64 %%p5, %%rd14, 0; @%%p5 bra L_mcmq4k_sb;\n");
        fprintf(out, "    shr.u32 %%r28, %%r5, 3; and.b32 %%r29, %%r5, 7; shl.b32 %%r29, %%r29, 2;\n");
        fprintf(out, "    and.b32 %%r30, %%r28, 1; shl.b32 %%r30, %%r30, 4; setp.lt.u32 %%p6, %%r28, 2;\n");
        fprintf(out, "    shl.b32 %%r31, %%r5, 2; add.u32 %%r31, %%r31, 16; shl.b32 %%r32, %%r28, 6; add.u32 %%r32, %%r32, %%r29;\n");
        fprintf(out, "L_mcmq4k_v4:\n    setp.ge.u32 %%p1, %%r8, %%r6; @%%p1 bra L_mcm_%s_s;\n", suf);
        fprintf(out, "    mul.wide.u32 %%rd5, %%r8, 144; add.u64 %%rd6, %%rd4, %%rd5;\n");
        fprintf(out, "    ld.v4.u32 {%%r9, %%r10, %%r11, %%r12}, [%%rd6];\n");
        fprintf(out, "    and.b32 %%r13, %%r9, 65535; shr.u32 %%r14, %%r9, 16;\n");
        fprintf(out, "    { .reg .b16 %%ha, %%hb; cvt.u16.u32 %%ha, %%r13; cvt.f32.f16 %%f1, %%ha; cvt.u16.u32 %%hb, %%r14; cvt.f32.f16 %%f2, %%hb; }\n");
        fprintf(out, "    shr.u32 %%r15, %%r10, %%r30; shr.u32 %%r16, %%r11, %%r30; shr.u32 %%r17, %%r12, %%r30;\n");
        fprintf(out, "    and.b32 %%r18, %%r15, 63; shr.u32 %%r19, %%r15, 8; and.b32 %%r19, %%r19, 63;\n");
        fprintf(out, "    and.b32 %%r20, %%r16, 63; shr.u32 %%r21, %%r16, 8; and.b32 %%r21, %%r21, 63;\n");
        fprintf(out, "    and.b32 %%r22, %%r17, 15; shr.u32 %%r23, %%r15, 2; and.b32 %%r23, %%r23, 48; or.b32 %%r22, %%r22, %%r23;\n");
        fprintf(out, "    shr.u32 %%r24, %%r17, 8; and.b32 %%r24, %%r24, 15; shr.u32 %%r23, %%r15, 10; and.b32 %%r23, %%r23, 48; or.b32 %%r24, %%r24, %%r23;\n");
        fprintf(out, "    shr.u32 %%r25, %%r17, 4; and.b32 %%r25, %%r25, 15; shr.u32 %%r23, %%r16, 2; and.b32 %%r23, %%r23, 48; or.b32 %%r25, %%r25, %%r23;\n");
        fprintf(out, "    shr.u32 %%r26, %%r17, 12; and.b32 %%r26, %%r26, 15; shr.u32 %%r23, %%r16, 10; and.b32 %%r23, %%r23, 48; or.b32 %%r26, %%r26, %%r23;\n");
        fprintf(out, "    selp.u32 %%r18, %%r18, %%r22, %%p6; selp.u32 %%r19, %%r19, %%r24, %%p6;\n");
        fprintf(out, "    selp.u32 %%r20, %%r20, %%r25, %%p6; selp.u32 %%r21, %%r21, %%r26, %%p6;\n");
        fprintf(out, "    cvt.rn.f32.u32 %%f3, %%r18; mul.f32 %%f3, %%f1, %%f3;\n");
        fprintf(out, "    cvt.rn.f32.u32 %%f4, %%r20; mul.f32 %%f4, %%f2, %%f4; neg.f32 %%f4, %%f4;\n");
        fprintf(out, "    cvt.rn.f32.u32 %%f5, %%r19; mul.f32 %%f5, %%f1, %%f5;\n");
        fprintf(out, "    cvt.rn.f32.u32 %%f6, %%r21; mul.f32 %%f6, %%f2, %%f6; neg.f32 %%f6, %%f6;\n");
        fprintf(out, "    cvt.u64.u32 %%rd8, %%r31; add.u64 %%rd9, %%rd6, %%rd8; ld.u32 %%r13, [%%rd9];\n");
        fprintf(out, "    shl.b32 %%r27, %%r8, 8; add.u32 %%r27, %%r27, %%r32; mul.wide.u32 %%rd10, %%r27, 4; add.u64 %%rd10, %%rd1, %%rd10;\n");
        fprintf(out, "    ld.v4.f32 {%%f11, %%f12, %%f13, %%f14}, [%%rd10];\n");
        fprintf(out, "    ld.v4.f32 {%%f15, %%f16, %%f17, %%f18}, [%%rd10+128];\n");
        for (int j = 0; j < 4; j++) {
            fprintf(out, "    bfe.u32 %%r25, %%r13, %d, 4; bfe.u32 %%r26, %%r13, %d, 4;\n", j * 8, j * 8 + 4);
            fprintf(out, "    cvt.rn.f32.u32 %%f7, %%r25; fma.rn.f32 %%f8, %%f3, %%f7, %%f4; fma.rn.f32 %%f0, %%f8, %%f%d, %%f0;\n", 11 + j);
            fprintf(out, "    cvt.rn.f32.u32 %%f9, %%r26; fma.rn.f32 %%f10, %%f5, %%f9, %%f6; fma.rn.f32 %%f0, %%f10, %%f%d, %%f0;\n", 15 + j);
        }
        fprintf(out, "    add.u32 %%r8, %%r8, 1; bra L_mcmq4k_v4;\n");
        fprintf(out, "L_mcmq4k_sb:\n    setp.ge.u32 %%p1, %%r8, %%r6; @%%p1 bra L_mcm_%s_s;\n", suf);
        fprintf(out, "    mul.wide.u32 %%rd5, %%r8, 144; add.u64 %%rd6, %%rd4, %%rd5;\n");
        fprintf(out, "    ld.u8 %%r9, [%%rd6]; ld.u8 %%r10, [%%rd6+1]; shl.b32 %%r10, %%r10, 8; or.b32 %%r9, %%r9, %%r10;\n");
        fprintf(out, "    { .reg .b16 %%hb; cvt.u16.u32 %%hb, %%r9; cvt.f32.f16 %%f1, %%hb; }\n");
        fprintf(out, "    ld.u8 %%r9, [%%rd6+2]; ld.u8 %%r10, [%%rd6+3]; shl.b32 %%r10, %%r10, 8; or.b32 %%r9, %%r9, %%r10;\n");
        fprintf(out, "    { .reg .b16 %%hb; cvt.u16.u32 %%hb, %%r9; cvt.f32.f16 %%f2, %%hb; }\n");
        fprintf(out, "    add.u64 %%rd7, %%rd6, 4; mov.u32 %%r11, 0;\n");
        fprintf(out, "L_mcmq4k_c:\n    setp.ge.u32 %%p2, %%r11, 4; @%%p2 bra L_mcmq4k_cd;\n");
        fprintf(out, "    shl.b32 %%r12, %%r11, 1; add.u32 %%r13, %%r12, 1;\n");
        fprintf(out, "    setp.lt.u32 %%p5, %%r11, 2; @%%p5 bra L_mcmq4k_clo;\n");
        fprintf(out, "    add.u32 %%r14, %%r12, 4; cvt.u64.u32 %%rd8, %%r14; add.u64 %%rd9, %%rd7, %%rd8; ld.u8 %%r15, [%%rd9];\n");
        fprintf(out, "    sub.u32 %%r16, %%r12, 4; cvt.u64.u32 %%rd8, %%r16; add.u64 %%rd9, %%rd7, %%rd8; ld.u8 %%r17, [%%rd9];\n");
        fprintf(out, "    cvt.u64.u32 %%rd8, %%r12; add.u64 %%rd9, %%rd7, %%rd8; ld.u8 %%r18, [%%rd9];\n");
        fprintf(out, "    and.b32 %%r19, %%r15, 15; shr.u32 %%r17, %%r17, 6; shl.b32 %%r17, %%r17, 4; or.b32 %%r19, %%r19, %%r17;\n");
        fprintf(out, "    shr.u32 %%r20, %%r15, 4; shr.u32 %%r18, %%r18, 6; shl.b32 %%r18, %%r18, 4; or.b32 %%r20, %%r20, %%r18;\n");
        fprintf(out, "    add.u32 %%r14, %%r13, 4; cvt.u64.u32 %%rd8, %%r14; add.u64 %%rd9, %%rd7, %%rd8; ld.u8 %%r15, [%%rd9];\n");
        fprintf(out, "    sub.u32 %%r16, %%r13, 4; cvt.u64.u32 %%rd8, %%r16; add.u64 %%rd9, %%rd7, %%rd8; ld.u8 %%r17, [%%rd9];\n");
        fprintf(out, "    cvt.u64.u32 %%rd8, %%r13; add.u64 %%rd9, %%rd7, %%rd8; ld.u8 %%r18, [%%rd9];\n");
        fprintf(out, "    and.b32 %%r21, %%r15, 15; shr.u32 %%r17, %%r17, 6; shl.b32 %%r17, %%r17, 4; or.b32 %%r21, %%r21, %%r17;\n");
        fprintf(out, "    shr.u32 %%r22, %%r15, 4; shr.u32 %%r18, %%r18, 6; shl.b32 %%r18, %%r18, 4; or.b32 %%r22, %%r22, %%r18;\n");
        fprintf(out, "    bra L_mcmq4k_csc;\nL_mcmq4k_clo:\n");
        fprintf(out, "    cvt.u64.u32 %%rd8, %%r12; add.u64 %%rd9, %%rd7, %%rd8; ld.u8 %%r19, [%%rd9]; and.b32 %%r19, %%r19, 63;\n");
        fprintf(out, "    add.u32 %%r14, %%r12, 4; cvt.u64.u32 %%rd8, %%r14; add.u64 %%rd9, %%rd7, %%rd8; ld.u8 %%r20, [%%rd9]; and.b32 %%r20, %%r20, 63;\n");
        fprintf(out, "    cvt.u64.u32 %%rd8, %%r13; add.u64 %%rd9, %%rd7, %%rd8; ld.u8 %%r21, [%%rd9]; and.b32 %%r21, %%r21, 63;\n");
        fprintf(out, "    add.u32 %%r14, %%r13, 4; cvt.u64.u32 %%rd8, %%r14; add.u64 %%rd9, %%rd7, %%rd8; ld.u8 %%r22, [%%rd9]; and.b32 %%r22, %%r22, 63;\n");
        fprintf(out, "L_mcmq4k_csc:\n");
        fprintf(out, "    cvt.rn.f32.u32 %%f3, %%r19; mul.f32 %%f3, %%f1, %%f3;\n");
        fprintf(out, "    cvt.rn.f32.u32 %%f4, %%r20; mul.f32 %%f4, %%f2, %%f4; neg.f32 %%f4, %%f4;\n");
        fprintf(out, "    cvt.rn.f32.u32 %%f5, %%r21; mul.f32 %%f5, %%f1, %%f5;\n");
        fprintf(out, "    cvt.rn.f32.u32 %%f6, %%r22; mul.f32 %%f6, %%f2, %%f6; neg.f32 %%f6, %%f6;\n");
        fprintf(out, "    shl.b32 %%r23, %%r11, 5; add.u32 %%r23, %%r23, %%r5; add.u32 %%r23, %%r23, 16;\n");
        fprintf(out, "    cvt.u64.u32 %%rd8, %%r23; add.u64 %%rd9, %%rd6, %%rd8; ld.u8 %%r24, [%%rd9];\n");
        fprintf(out, "    and.b32 %%r25, %%r24, 15; shr.u32 %%r26, %%r24, 4;\n");
        fprintf(out, "    cvt.rn.f32.u32 %%f7, %%r25; fma.rn.f32 %%f8, %%f3, %%f7, %%f4;\n");
        fprintf(out, "    cvt.rn.f32.u32 %%f9, %%r26; fma.rn.f32 %%f10, %%f5, %%f9, %%f6;\n");
        fprintf(out, "    shl.b32 %%r27, %%r8, 8; shl.b32 %%r23, %%r11, 6; add.u32 %%r27, %%r27, %%r23; add.u32 %%r27, %%r27, %%r5;\n");
        fprintf(out, "    mul.wide.u32 %%rd10, %%r27, 4; add.u64 %%rd10, %%rd1, %%rd10;\n");
        fprintf(out, "    ld.f32 %%f11, [%%rd10]; ld.f32 %%f12, [%%rd10+128];\n");
        fprintf(out, "    fma.rn.f32 %%f0, %%f8, %%f11, %%f0; fma.rn.f32 %%f0, %%f10, %%f12, %%f0;\n");
        fprintf(out, "    add.u32 %%r11, %%r11, 1; bra L_mcmq4k_c;\n");
        fprintf(out, "L_mcmq4k_cd:\n    add.u32 %%r8, %%r8, 1; bra L_mcmq4k_sb;\n");
    } else if (fmt == GGUF_PTX_Q6_K) {
        fprintf(out, "    shr.u32 %%r6, %%r1, 8; mul.lo.u32 %%r7, %%r6, 210;\n");
        fprintf(out, "    mul.wide.u32 %%rd3, %%r3, %%r7; add.u64 %%rd4, %%rd0, %%rd3;\n");
        fprintf(out, "    shr.u32 %%r8, %%r5, 4;\n");
        fprintf(out, "    mov.f32 %%f0, 0f00000000; mov.u32 %%r9, 0;\n");
        fprintf(out, "L_mcmq6k_sb:\n    setp.ge.u32 %%p1, %%r9, %%r6; @%%p1 bra L_mcm_%s_s;\n", suf);
        fprintf(out, "    mul.wide.u32 %%rd5, %%r9, 210; add.u64 %%rd6, %%rd4, %%rd5;\n");
        fprintf(out, "    ld.u8 %%r10, [%%rd6+208]; ld.u8 %%r11, [%%rd6+209]; shl.b32 %%r11, %%r11, 8; or.b32 %%r10, %%r10, %%r11;\n");
        fprintf(out, "    { .reg .b16 %%hb; cvt.u16.u32 %%hb, %%r10; cvt.f32.f16 %%f1, %%hb; }\n");
        fprintf(out, "    mov.u32 %%r12, 0;\n");
        fprintf(out, "L_mcmq6k_h:\n    setp.ge.u32 %%p2, %%r12, 2; @%%p2 bra L_mcmq6k_hd;\n");
        fprintf(out, "    shl.b32 %%r13, %%r12, 6; add.u32 %%r13, %%r13, %%r5; cvt.u64.u32 %%rd7, %%r13; add.u64 %%rd7, %%rd6, %%rd7;\n");
        fprintf(out, "    ld.u8 %%r14, [%%rd7]; ld.u8 %%r15, [%%rd7+32];\n");
        fprintf(out, "    shl.b32 %%r13, %%r12, 5; add.u32 %%r13, %%r13, %%r5; add.u32 %%r13, %%r13, 128; cvt.u64.u32 %%rd7, %%r13; add.u64 %%rd7, %%rd6, %%rd7;\n");
        fprintf(out, "    ld.u8 %%r16, [%%rd7];\n");
        fprintf(out, "    shl.b32 %%r13, %%r12, 3; add.u32 %%r13, %%r13, %%r8; add.u32 %%r13, %%r13, 192; cvt.u64.u32 %%rd7, %%r13; add.u64 %%rd7, %%rd6, %%rd7;\n");
        fprintf(out, "    ld.u8 %%r17, [%%rd7]; setp.ge.u32 %%p5, %%r17, 128; @%%p5 sub.s32 %%r17, %%r17, 256; cvt.rn.f32.s32 %%f2, %%r17;\n");
        fprintf(out, "    ld.u8 %%r18, [%%rd7+2]; setp.ge.u32 %%p5, %%r18, 128; @%%p5 sub.s32 %%r18, %%r18, 256; cvt.rn.f32.s32 %%f3, %%r18;\n");
        fprintf(out, "    ld.u8 %%r19, [%%rd7+4]; setp.ge.u32 %%p5, %%r19, 128; @%%p5 sub.s32 %%r19, %%r19, 256; cvt.rn.f32.s32 %%f4, %%r19;\n");
        fprintf(out, "    ld.u8 %%r20, [%%rd7+6]; setp.ge.u32 %%p5, %%r20, 128; @%%p5 sub.s32 %%r20, %%r20, 256; cvt.rn.f32.s32 %%f5, %%r20;\n");
        fprintf(out, "    and.b32 %%r21, %%r14, 15; and.b32 %%r22, %%r16, 3; shl.b32 %%r22, %%r22, 4; or.b32 %%r21, %%r21, %%r22; sub.s32 %%r21, %%r21, 32;\n");
        fprintf(out, "    and.b32 %%r23, %%r15, 15; shr.u32 %%r22, %%r16, 2; and.b32 %%r22, %%r22, 3; shl.b32 %%r22, %%r22, 4; or.b32 %%r23, %%r23, %%r22; sub.s32 %%r23, %%r23, 32;\n");
        fprintf(out, "    shr.u32 %%r24, %%r14, 4; shr.u32 %%r22, %%r16, 4; and.b32 %%r22, %%r22, 3; shl.b32 %%r22, %%r22, 4; or.b32 %%r24, %%r24, %%r22; sub.s32 %%r24, %%r24, 32;\n");
        fprintf(out, "    shr.u32 %%r25, %%r15, 4; shr.u32 %%r22, %%r16, 6; and.b32 %%r22, %%r22, 3; shl.b32 %%r22, %%r22, 4; or.b32 %%r25, %%r25, %%r22; sub.s32 %%r25, %%r25, 32;\n");
        fprintf(out, "    cvt.rn.f32.s32 %%f6, %%r21; mul.f32 %%f6, %%f6, %%f2; mul.f32 %%f6, %%f6, %%f1;\n");
        fprintf(out, "    cvt.rn.f32.s32 %%f7, %%r23; mul.f32 %%f7, %%f7, %%f3; mul.f32 %%f7, %%f7, %%f1;\n");
        fprintf(out, "    cvt.rn.f32.s32 %%f8, %%r24; mul.f32 %%f8, %%f8, %%f4; mul.f32 %%f8, %%f8, %%f1;\n");
        fprintf(out, "    cvt.rn.f32.s32 %%f9, %%r25; mul.f32 %%f9, %%f9, %%f5; mul.f32 %%f9, %%f9, %%f1;\n");
        fprintf(out, "    shl.b32 %%r26, %%r9, 8; shl.b32 %%r27, %%r12, 7; add.u32 %%r26, %%r26, %%r27; add.u32 %%r26, %%r26, %%r5;\n");
        fprintf(out, "    mul.wide.u32 %%rd8, %%r26, 4; add.u64 %%rd8, %%rd1, %%rd8;\n");
        fprintf(out, "    ld.f32 %%f10, [%%rd8]; ld.f32 %%f11, [%%rd8+128]; ld.f32 %%f12, [%%rd8+256]; ld.f32 %%f13, [%%rd8+384];\n");
        fprintf(out, "    fma.rn.f32 %%f0, %%f6, %%f10, %%f0; fma.rn.f32 %%f0, %%f7, %%f11, %%f0; fma.rn.f32 %%f0, %%f8, %%f12, %%f0; fma.rn.f32 %%f0, %%f9, %%f13, %%f0;\n");
        fprintf(out, "    add.u32 %%r12, %%r12, 1; bra L_mcmq6k_h;\n");
        fprintf(out, "L_mcmq6k_hd:\n    add.u32 %%r9, %%r9, 1; bra L_mcmq6k_sb;\n");
    }

    fprintf(out, "L_mcm_%s_s:\n", suf);
    fprintf(out, "    { .param .f32 _wr, _wa; st.param.f32 [_wa], %%f0; call.uni (_wr), zcc_aot_warp_sum, (_wa); ld.param.f32 %%f0, [_wr]; }\n");
    fprintf(out, "    setp.eq.u32 %%p3, %%r5, 0; @%%p3 bra L_mcm_%s_w; bra L_mcm_%s_d;\nL_mcm_%s_w:\n", suf, suf, suf);
    fprintf(out, "    mul.wide.u32 %%rd12, %%r3, 4; add.u64 %%rd13, %%rd2, %%rd12;\n");
    fprintf(out, "    setp.ne.u32 %%p4, %%r2, 0; @%%p4 ld.f32 %%f14, [%%rd13]; @%%p4 add.f32 %%f0, %%f14, %%f0; st.f32 [%%rd13], %%f0;\n");
    fprintf(out, "L_mcm_%s_d:\n    ret;\n}\n\n", suf);
}

static void gguf_ptx_emit_mc_matvec_multi_fn(FILE *out, uint32_t fmt, int max_m) {
    const char *suf = (fmt == GGUF_PTX_F16) ? "f16" :
                      (fmt == GGUF_PTX_BF16) ? "bf16" :
                      (fmt == GGUF_PTX_Q8_0) ? "q8_0" :
                      (fmt == GGUF_PTX_Q4_0) ? "q4_0" :
                      (fmt == GGUF_PTX_Q4_K) ? "q4_k" :
                      (fmt == GGUF_PTX_Q6_K) ? "q6_k" : "f32";
    const char *ms = (max_m == 4) ? "4" : "";
    fprintf(out, ".visible .entry zcc_aot_mc_matvec_multi%s_%s(\n"
                 "    .param .u64 _pw, .param .u64 _pwoff, .param .u64 _px,\n"
                 "    .param .u64 _py, .param .u32 _pN, .param .u32 _pK,\n"
                 "    .param .u32 _pM, .param .u32 _padd) .maxntid 128, 1, 1 {\n", ms, suf);
    if (fmt == GGUF_PTX_Q4_K) {
        fprintf(out, "    .reg .pred %%p<7>; .reg .pred %%pt<%d>;\n", max_m);
        fprintf(out, "    .reg .b32 %%r<34>; .reg .b64 %%rd<18>;\n");
        fprintf(out, "    .reg .f32 %%f<15>; .reg .f32 %%fw<4>; .reg .f32 %%fa<%d>;\n", max_m);
    } else if (fmt == GGUF_PTX_Q6_K) {
        fprintf(out, "    .reg .pred %%p<6>; .reg .pred %%pt<%d>;\n", max_m);
        fprintf(out, "    .reg .b32 %%r<34>; .reg .b64 %%rd<18>;\n");
        fprintf(out, "    .reg .f32 %%f<15>; .reg .f32 %%fw<4>; .reg .f32 %%fa<%d>;\n", max_m);
    } else if (fmt == GGUF_PTX_F16 || fmt == GGUF_PTX_BF16) {
        fprintf(out, "    .reg .pred %%p<7>; .reg .pred %%pt<%d>;\n", max_m);
        fprintf(out, "    .reg .b32 %%r<34>; .reg .b64 %%rd<18>;\n");
        fprintf(out, "    .reg .f32 %%f<15>; .reg .f32 %%fw<8>; .reg .f32 %%fa<%d>;\n", max_m);
    } else {
        fprintf(out, "    .reg .pred %%p<7>; .reg .pred %%pt<%d>;\n", max_m);
        fprintf(out, "    .reg .b32 %%r<34>; .reg .b64 %%rd<18>;\n");
        fprintf(out, "    .reg .f32 %%f<15>; .reg .f32 %%fw<4>; .reg .f32 %%fa<%d>;\n", max_m);
    }
    fprintf(out, "    ld.param.u64 %%rd0, [_pw]; ld.param.u64 %%rd11, [_pwoff]; add.u64 %%rd0, %%rd0, %%rd11;\n");
    fprintf(out, "    ld.param.u64 %%rd1, [_px]; ld.param.u64 %%rd2, [_py];\n");
    fprintf(out, "    ld.param.u32 %%r0, [_pN]; ld.param.u32 %%r1, [_pK];\n");
    fprintf(out, "    ld.param.u32 %%r33, [_pM]; ld.param.u32 %%r2, [_padd];\n");
    fprintf(out, "    mov.u32 %%r3, %%ctaid.x; mov.u32 %%r4, %%tid.x;\n");
    fprintf(out, "    shr.u32 %%r5, %%r4, 5; shl.b32 %%r3, %%r3, 2; add.u32 %%r3, %%r3, %%r5; and.b32 %%r5, %%r4, 31;\n");
    fprintf(out, "    setp.ge.u32 %%p0, %%r3, %%r0; @%%p0 bra L_mm%s_%s_d;\n", ms, suf);
    fprintf(out, "    min.u32 %%r33, %%r33, %d; setp.eq.u32 %%p0, %%r33, 0; @%%p0 bra L_mm%s_%s_d;\n", max_m, ms, suf);
    for (int t = 0; t < max_m; t++) {
        fprintf(out, "    setp.gt.u32 %%pt%d, %%r33, %d; mov.f32 %%fa%d, 0f00000000;\n", t, t, t);
    }
    fprintf(out, "    mul.wide.u32 %%rd15, %%r1, 4; mul.wide.u32 %%rd16, %%r0, 4;\n");

    if (fmt == GGUF_PTX_F32) {
        fprintf(out, "    mul.wide.u32 %%rd3, %%r3, %%r1; shl.b64 %%rd3, %%rd3, 2; add.u64 %%rd4, %%rd0, %%rd3;\n");
        fprintf(out, "    mov.u32 %%r6, 0;\n");
        fprintf(out, "    or.b64 %%rd14, %%rd4, %%rd1; or.b64 %%rd14, %%rd14, %%rd15; and.b64 %%rd14, %%rd14, 15; setp.ne.u64 %%p5, %%rd14, 0; @%%p5 bra L_mm%sf32_k;\n", ms);
        fprintf(out, "L_mm%sf32_v4:\n    add.u32 %%r8, %%r6, 128; setp.gt.u32 %%p1, %%r8, %%r1; @%%p1 bra L_mm%sf32_k;\n", ms, ms);
        fprintf(out, "    shl.b32 %%r7, %%r5, 2; add.u32 %%r7, %%r6, %%r7; mul.wide.u32 %%rd5, %%r7, 4;\n");
        fprintf(out, "    add.u64 %%rd6, %%rd4, %%rd5; ld.v4.f32 {%%fw0, %%fw1, %%fw2, %%fw3}, [%%rd6];\n");
        fprintf(out, "    add.u64 %%rd17, %%rd1, %%rd5;\n");
        for (int t = 0; t < max_m; t++) {
            fprintf(out, "    @!%%pt%d bra L_mm%sf32_v4e;\n", t, ms);
            fprintf(out, "    ld.v4.f32 {%%f5, %%f6, %%f7, %%f8}, [%%rd17];\n");
            fprintf(out, "    fma.rn.f32 %%fa%d, %%fw0, %%f5, %%fa%d; fma.rn.f32 %%fa%d, %%fw1, %%f6, %%fa%d;\n", t, t, t, t);
            fprintf(out, "    fma.rn.f32 %%fa%d, %%fw2, %%f7, %%fa%d; fma.rn.f32 %%fa%d, %%fw3, %%f8, %%fa%d;\n", t, t, t, t);
            if (t < max_m - 1) fprintf(out, "    add.u64 %%rd17, %%rd17, %%rd15;\n");
        }
        fprintf(out, "L_mm%sf32_v4e:\n    mov.u32 %%r6, %%r8; bra L_mm%sf32_v4;\n", ms, ms);
        fprintf(out, "L_mm%sf32_k:\n    setp.ge.u32 %%p1, %%r6, %%r1; @%%p1 bra L_mm%s_%s_s;\n", ms, ms, suf);
        fprintf(out, "    add.u32 %%r7, %%r6, %%r5; setp.lt.u32 %%p2, %%r7, %%r1;\n");
        fprintf(out, "    @%%p2 mul.wide.u32 %%rd5, %%r7, 4; @%%p2 add.u64 %%rd6, %%rd4, %%rd5; @%%p2 ld.f32 %%fw0, [%%rd6];\n");
        fprintf(out, "    @%%p2 add.u64 %%rd17, %%rd1, %%rd5;\n");
        for (int t = 0; t < max_m; t++) {
            fprintf(out, "    @!%%pt%d bra L_mm%sf32_ke;\n", t, ms);
            fprintf(out, "    @%%p2 ld.f32 %%f2, [%%rd17]; @%%p2 fma.rn.f32 %%fa%d, %%fw0, %%f2, %%fa%d;\n", t, t);
            if (t < max_m - 1) fprintf(out, "    @%%p2 add.u64 %%rd17, %%rd17, %%rd15;\n");
        }
        fprintf(out, "L_mm%sf32_ke:\n    add.u32 %%r6, %%r6, 32; bra L_mm%sf32_k;\n", ms, ms);
    } else if (fmt == GGUF_PTX_F16 || fmt == GGUF_PTX_BF16) {
        fprintf(out, "    mul.wide.u32 %%rd3, %%r3, %%r1; shl.b64 %%rd3, %%rd3, 1; add.u64 %%rd4, %%rd0, %%rd3;\n");
        fprintf(out, "    mov.u32 %%r6, 0;\n");
        fprintf(out, "    or.b64 %%rd14, %%rd4, %%rd1; or.b64 %%rd14, %%rd14, %%rd15; and.b64 %%rd14, %%rd14, 15; setp.ne.u64 %%p5, %%rd14, 0; @%%p5 bra L_mm%s%s_k;\n", ms, suf);
        fprintf(out, "L_mm%s%s_v4:\n    add.u32 %%r9, %%r6, 256; setp.gt.u32 %%p1, %%r9, %%r1; @%%p1 bra L_mm%s%s_k;\n", ms, suf, ms, suf);
        fprintf(out, "    shl.b32 %%r7, %%r5, 3; add.u32 %%r7, %%r6, %%r7;\n");
        fprintf(out, "    mul.wide.u32 %%rd5, %%r7, 2; add.u64 %%rd6, %%rd4, %%rd5;\n");
        fprintf(out, "    mul.wide.u32 %%rd8, %%r7, 4;\n");
        fprintf(out, "    ld.v4.u32 {%%r12, %%r13, %%r14, %%r15}, [%%rd6];\n");
        for (int p = 0; p < 4; p++) {
            int wreg = 12 + p;
            int fw0 = p * 2, fw1 = p * 2 + 1;
            if (fmt == GGUF_PTX_F16) {
                fprintf(out, "    and.b32 %%r16, %%r%d, 65535; shr.u32 %%r17, %%r%d, 16;\n", wreg, wreg);
                fprintf(out, "    { .reg .b16 %%ha, %%hb; cvt.u16.u32 %%ha, %%r16; cvt.f32.f16 %%fw%d, %%ha; cvt.u16.u32 %%hb, %%r17; cvt.f32.f16 %%fw%d, %%hb; }\n", fw0, fw1);
            } else {
                fprintf(out, "    shl.b32 %%r16, %%r%d, 16; mov.b32 %%fw%d, %%r16; and.b32 %%r17, %%r%d, 0xFFFF0000; mov.b32 %%fw%d, %%r17;\n", wreg, fw0, wreg, fw1);
            }
        }
        fprintf(out, "    add.u64 %%rd17, %%rd1, %%rd8;\n");
        for (int t = 0; t < max_m; t++) {
            fprintf(out, "    @!%%pt%d bra L_mm%s%s_v4e;\n", t, ms, suf);
            fprintf(out, "    ld.v4.f32 {%%f5, %%f6, %%f7, %%f8}, [%%rd17];\n");
            fprintf(out, "    ld.v4.f32 {%%f9, %%f10, %%f11, %%f12}, [%%rd17+16];\n");
            for (int i = 0; i < 8; i++) {
                fprintf(out, "    fma.rn.f32 %%fa%d, %%fw%d, %%f%d, %%fa%d;\n", t, i, 5 + i, t);
            }
            if (t < max_m - 1) fprintf(out, "    add.u64 %%rd17, %%rd17, %%rd15;\n");
        }
        fprintf(out, "L_mm%s%s_v4e:\n    mov.u32 %%r6, %%r9; bra L_mm%s%s_v4;\n", ms, suf, ms, suf);
        fprintf(out, "L_mm%s%s_k:\n    setp.ge.u32 %%p1, %%r6, %%r1; @%%p1 bra L_mm%s_%s_s;\n", ms, suf, ms, suf);
        fprintf(out, "    add.u32 %%r7, %%r6, %%r5; setp.lt.u32 %%p2, %%r7, %%r1;\n");
        fprintf(out, "    @%%p2 mul.wide.u32 %%rd5, %%r7, 2; @%%p2 add.u64 %%rd6, %%rd4, %%rd5; @%%p2 ld.u16 %%r8, [%%rd6];\n");
        if (fmt == GGUF_PTX_F16) fprintf(out, "    { .reg .b16 %%hb; mov.u16 %%hb, 0; @%%p2 cvt.u16.u32 %%hb, %%r8; @%%p2 cvt.f32.f16 %%fw0, %%hb; }\n");
        else fprintf(out, "    @%%p2 shl.b32 %%r8, %%r8, 16; @%%p2 mov.b32 %%fw0, %%r8;\n");
        fprintf(out, "    @%%p2 mul.wide.u32 %%rd5, %%r7, 4; @%%p2 add.u64 %%rd17, %%rd1, %%rd5;\n");
        for (int t = 0; t < max_m; t++) {
            fprintf(out, "    @!%%pt%d bra L_mm%s%s_ke;\n", t, ms, suf);
            fprintf(out, "    @%%p2 ld.f32 %%f2, [%%rd17]; @%%p2 fma.rn.f32 %%fa%d, %%fw0, %%f2, %%fa%d;\n", t, t);
            if (t < max_m - 1) fprintf(out, "    @%%p2 add.u64 %%rd17, %%rd17, %%rd15;\n");
        }
        fprintf(out, "L_mm%s%s_ke:\n    add.u32 %%r6, %%r6, 32; bra L_mm%s%s_k;\n", ms, suf, ms, suf);
    } else if (fmt == GGUF_PTX_Q8_0) {
        fprintf(out, "    shr.u32 %%r6, %%r1, 5; mul.lo.u32 %%r7, %%r6, 34;\n");
        fprintf(out, "    mul.wide.u32 %%rd3, %%r3, %%r7; add.u64 %%rd4, %%rd0, %%rd3;\n");
        fprintf(out, "    mov.u32 %%r8, 0;\n");
        fprintf(out, "L_mm%sq8_b:\n    setp.ge.u32 %%p1, %%r8, %%r6; @%%p1 bra L_mm%s_%s_s;\n", ms, ms, suf);
        fprintf(out, "    mul.wide.u32 %%rd5, %%r8, 34; add.u64 %%rd6, %%rd4, %%rd5;\n");
        fprintf(out, "    ld.u8 %%r9, [%%rd6]; ld.u8 %%r10, [%%rd6+1]; shl.b32 %%r10, %%r10, 8; or.b32 %%r9, %%r9, %%r10;\n");
        fprintf(out, "    { .reg .b16 %%hb; cvt.u16.u32 %%hb, %%r9; cvt.f32.f16 %%f1, %%hb; }\n");
        fprintf(out, "    add.u32 %%r11, %%r5, 2; cvt.u64.u32 %%rd7, %%r11; add.u64 %%rd7, %%rd6, %%rd7;\n");
        fprintf(out, "    ld.u8 %%r9, [%%rd7]; setp.ge.u32 %%p2, %%r9, 128; @%%p2 sub.s32 %%r9, %%r9, 256; cvt.rn.f32.s32 %%f2, %%r9;\n");
        fprintf(out, "    mul.f32 %%fw0, %%f1, %%f2; shl.b32 %%r11, %%r8, 5; add.u32 %%r11, %%r11, %%r5;\n");
        fprintf(out, "    mul.wide.u32 %%rd8, %%r11, 4; add.u64 %%rd17, %%rd1, %%rd8;\n");
        for (int t = 0; t < max_m; t++) {
            fprintf(out, "    @!%%pt%d bra L_mm%sq8_be;\n", t, ms);
            fprintf(out, "    ld.f32 %%f3, [%%rd17]; fma.rn.f32 %%fa%d, %%fw0, %%f3, %%fa%d;\n", t, t);
            if (t < max_m - 1) fprintf(out, "    add.u64 %%rd17, %%rd17, %%rd15;\n");
        }
        fprintf(out, "L_mm%sq8_be:\n    add.u32 %%r8, %%r8, 1; bra L_mm%sq8_b;\n", ms, ms);
    } else if (fmt == GGUF_PTX_Q4_0) {
        fprintf(out, "    shr.u32 %%r6, %%r1, 5; mul.lo.u32 %%r7, %%r6, 18;\n");
        fprintf(out, "    mul.wide.u32 %%rd3, %%r3, %%r7; add.u64 %%rd4, %%rd0, %%rd3;\n");
        fprintf(out, "    and.b32 %%r8, %%r5, 15; add.u32 %%r9, %%r8, 2;\n");
        fprintf(out, "    mov.u32 %%r10, 0;\n");
        fprintf(out, "L_mm%sq40_b:\n    setp.ge.u32 %%p1, %%r10, %%r6; @%%p1 bra L_mm%s_%s_s;\n", ms, ms, suf);
        fprintf(out, "    mul.wide.u32 %%rd5, %%r10, 18; add.u64 %%rd6, %%rd4, %%rd5;\n");
        fprintf(out, "    ld.u8 %%r11, [%%rd6]; ld.u8 %%r12, [%%rd6+1]; shl.b32 %%r12, %%r12, 8; or.b32 %%r11, %%r11, %%r12;\n");
        fprintf(out, "    { .reg .b16 %%hb; cvt.u16.u32 %%hb, %%r11; cvt.f32.f16 %%f1, %%hb; }\n");
        fprintf(out, "    cvt.u64.u32 %%rd7, %%r9; add.u64 %%rd7, %%rd6, %%rd7; ld.u8 %%r13, [%%rd7];\n");
        fprintf(out, "    and.b32 %%r14, %%r13, 15; shr.u32 %%r15, %%r13, 4; setp.lt.u32 %%p2, %%r5, 16; selp.u32 %%r16, %%r14, %%r15, %%p2;\n");
        fprintf(out, "    sub.s32 %%r16, %%r16, 8; cvt.rn.f32.s32 %%f2, %%r16; mul.f32 %%fw0, %%f1, %%f2;\n");
        fprintf(out, "    shl.b32 %%r17, %%r10, 5; add.u32 %%r17, %%r17, %%r5;\n");
        fprintf(out, "    mul.wide.u32 %%rd8, %%r17, 4; add.u64 %%rd17, %%rd1, %%rd8;\n");
        for (int t = 0; t < max_m; t++) {
            fprintf(out, "    @!%%pt%d bra L_mm%sq40_be;\n", t, ms);
            fprintf(out, "    ld.f32 %%f3, [%%rd17]; fma.rn.f32 %%fa%d, %%fw0, %%f3, %%fa%d;\n", t, t);
            if (t < max_m - 1) fprintf(out, "    add.u64 %%rd17, %%rd17, %%rd15;\n");
        }
        fprintf(out, "L_mm%sq40_be:\n    add.u32 %%r10, %%r10, 1; bra L_mm%sq40_b;\n", ms, ms);
    } else if (fmt == GGUF_PTX_Q4_K) {
        fprintf(out, "    shr.u32 %%r6, %%r1, 8; mul.lo.u32 %%r7, %%r6, 144;\n");
        fprintf(out, "    mul.wide.u32 %%rd3, %%r3, %%r7; add.u64 %%rd4, %%rd0, %%rd3;\n");
        fprintf(out, "    mov.u32 %%r8, 0;\n");
        fprintf(out, "    or.b64 %%rd14, %%rd4, %%rd1; or.b64 %%rd14, %%rd14, %%rd15; and.b64 %%rd14, %%rd14, 15; setp.ne.u64 %%p5, %%rd14, 0; @%%p5 bra L_mm%sq4k_sb;\n", ms);
        fprintf(out, "    shr.u32 %%r28, %%r5, 3; and.b32 %%r29, %%r5, 7; shl.b32 %%r29, %%r29, 2;\n");
        fprintf(out, "    and.b32 %%r30, %%r28, 1; shl.b32 %%r30, %%r30, 4; setp.lt.u32 %%p6, %%r28, 2;\n");
        fprintf(out, "    shl.b32 %%r31, %%r5, 2; add.u32 %%r31, %%r31, 16; shl.b32 %%r32, %%r28, 6; add.u32 %%r32, %%r32, %%r29;\n");
        fprintf(out, "L_mm%sq4k_v4:\n    setp.ge.u32 %%p1, %%r8, %%r6; @%%p1 bra L_mm%s_%s_s;\n", ms, ms, suf);
        fprintf(out, "    mul.wide.u32 %%rd5, %%r8, 144; add.u64 %%rd6, %%rd4, %%rd5;\n");
        fprintf(out, "    ld.v4.u32 {%%r9, %%r10, %%r11, %%r12}, [%%rd6];\n");
        fprintf(out, "    and.b32 %%r13, %%r9, 65535; shr.u32 %%r14, %%r9, 16;\n");
        fprintf(out, "    { .reg .b16 %%ha, %%hb; cvt.u16.u32 %%ha, %%r13; cvt.f32.f16 %%f1, %%ha; cvt.u16.u32 %%hb, %%r14; cvt.f32.f16 %%f2, %%hb; }\n");
        fprintf(out, "    shr.u32 %%r15, %%r10, %%r30; shr.u32 %%r16, %%r11, %%r30; shr.u32 %%r17, %%r12, %%r30;\n");
        fprintf(out, "    and.b32 %%r18, %%r15, 63; shr.u32 %%r19, %%r15, 8; and.b32 %%r19, %%r19, 63;\n");
        fprintf(out, "    and.b32 %%r20, %%r16, 63; shr.u32 %%r21, %%r16, 8; and.b32 %%r21, %%r21, 63;\n");
        fprintf(out, "    and.b32 %%r22, %%r17, 15; shr.u32 %%r23, %%r15, 2; and.b32 %%r23, %%r23, 48; or.b32 %%r22, %%r22, %%r23;\n");
        fprintf(out, "    shr.u32 %%r24, %%r17, 8; and.b32 %%r24, %%r24, 15; shr.u32 %%r23, %%r15, 10; and.b32 %%r23, %%r23, 48; or.b32 %%r24, %%r24, %%r23;\n");
        fprintf(out, "    shr.u32 %%r25, %%r17, 4; and.b32 %%r25, %%r25, 15; shr.u32 %%r23, %%r16, 2; and.b32 %%r23, %%r23, 48; or.b32 %%r25, %%r25, %%r23;\n");
        fprintf(out, "    shr.u32 %%r26, %%r17, 12; and.b32 %%r26, %%r26, 15; shr.u32 %%r23, %%r16, 10; and.b32 %%r23, %%r23, 48; or.b32 %%r26, %%r26, %%r23;\n");
        fprintf(out, "    selp.u32 %%r18, %%r18, %%r22, %%p6; selp.u32 %%r19, %%r19, %%r24, %%p6;\n");
        fprintf(out, "    selp.u32 %%r20, %%r20, %%r25, %%p6; selp.u32 %%r21, %%r21, %%r26, %%p6;\n");
        fprintf(out, "    cvt.rn.f32.u32 %%f3, %%r18; mul.f32 %%f3, %%f1, %%f3;\n");
        fprintf(out, "    cvt.rn.f32.u32 %%f4, %%r20; mul.f32 %%f4, %%f2, %%f4; neg.f32 %%f4, %%f4;\n");
        fprintf(out, "    cvt.rn.f32.u32 %%f5, %%r19; mul.f32 %%f5, %%f1, %%f5;\n");
        fprintf(out, "    cvt.rn.f32.u32 %%f6, %%r21; mul.f32 %%f6, %%f2, %%f6; neg.f32 %%f6, %%f6;\n");
        fprintf(out, "    cvt.u64.u32 %%rd8, %%r31; add.u64 %%rd9, %%rd6, %%rd8; ld.u32 %%r13, [%%rd9];\n");
        fprintf(out, "    shl.b32 %%r27, %%r8, 8; add.u32 %%r27, %%r27, %%r32; mul.wide.u32 %%rd10, %%r27, 4; add.u64 %%rd13, %%rd1, %%rd10;\n");
        for (int j = 0; j < 4; j++) {
            fprintf(out, "    bfe.u32 %%r25, %%r13, %d, 4; cvt.rn.f32.u32 %%f7, %%r25; fma.rn.f32 %%fw%d, %%f3, %%f7, %%f4;\n", j * 8, j);
        }
        fprintf(out, "    mov.u64 %%rd17, %%rd13;\n");
        for (int t = 0; t < max_m; t++) {
            fprintf(out, "    @!%%pt%d bra L_mm%sq4k_v4h;\n", t, ms);
            fprintf(out, "    ld.v4.f32 {%%f11, %%f12, %%f13, %%f14}, [%%rd17];\n");
            fprintf(out, "    fma.rn.f32 %%fa%d, %%fw0, %%f11, %%fa%d; fma.rn.f32 %%fa%d, %%fw1, %%f12, %%fa%d;\n", t, t, t, t);
            fprintf(out, "    fma.rn.f32 %%fa%d, %%fw2, %%f13, %%fa%d; fma.rn.f32 %%fa%d, %%fw3, %%f14, %%fa%d;\n", t, t, t, t);
            if (t < max_m - 1) fprintf(out, "    add.u64 %%rd17, %%rd17, %%rd15;\n");
        }
        fprintf(out, "L_mm%sq4k_v4h:\n", ms);
        for (int j = 0; j < 4; j++) {
            fprintf(out, "    bfe.u32 %%r26, %%r13, %d, 4; cvt.rn.f32.u32 %%f9, %%r26; fma.rn.f32 %%fw%d, %%f5, %%f9, %%f6;\n", j * 8 + 4, j);
        }
        fprintf(out, "    add.u64 %%rd17, %%rd13, 128;\n");
        for (int t = 0; t < max_m; t++) {
            fprintf(out, "    @!%%pt%d bra L_mm%sq4k_v4e;\n", t, ms);
            fprintf(out, "    ld.v4.f32 {%%f11, %%f12, %%f13, %%f14}, [%%rd17];\n");
            fprintf(out, "    fma.rn.f32 %%fa%d, %%fw0, %%f11, %%fa%d; fma.rn.f32 %%fa%d, %%fw1, %%f12, %%fa%d;\n", t, t, t, t);
            fprintf(out, "    fma.rn.f32 %%fa%d, %%fw2, %%f13, %%fa%d; fma.rn.f32 %%fa%d, %%fw3, %%f14, %%fa%d;\n", t, t, t, t);
            if (t < max_m - 1) fprintf(out, "    add.u64 %%rd17, %%rd17, %%rd15;\n");
        }
        fprintf(out, "L_mm%sq4k_v4e:\n    add.u32 %%r8, %%r8, 1; bra L_mm%sq4k_v4;\n", ms, ms);
        fprintf(out, "L_mm%sq4k_sb:\n    setp.ge.u32 %%p1, %%r8, %%r6; @%%p1 bra L_mm%s_%s_s;\n", ms, ms, suf);
        fprintf(out, "    mul.wide.u32 %%rd5, %%r8, 144; add.u64 %%rd6, %%rd4, %%rd5;\n");
        fprintf(out, "    ld.u8 %%r9, [%%rd6]; ld.u8 %%r10, [%%rd6+1]; shl.b32 %%r10, %%r10, 8; or.b32 %%r9, %%r9, %%r10;\n");
        fprintf(out, "    { .reg .b16 %%hb; cvt.u16.u32 %%hb, %%r9; cvt.f32.f16 %%f1, %%hb; }\n");
        fprintf(out, "    ld.u8 %%r9, [%%rd6+2]; ld.u8 %%r10, [%%rd6+3]; shl.b32 %%r10, %%r10, 8; or.b32 %%r9, %%r9, %%r10;\n");
        fprintf(out, "    { .reg .b16 %%hb; cvt.u16.u32 %%hb, %%r9; cvt.f32.f16 %%f2, %%hb; }\n");
        fprintf(out, "    add.u64 %%rd7, %%rd6, 4; mov.u32 %%r11, 0;\n");
        fprintf(out, "L_mm%sq4k_c:\n    setp.ge.u32 %%p2, %%r11, 4; @%%p2 bra L_mm%sq4k_cd;\n", ms, ms);
        fprintf(out, "    shl.b32 %%r12, %%r11, 1; add.u32 %%r13, %%r12, 1;\n");
        fprintf(out, "    setp.lt.u32 %%p5, %%r11, 2; @%%p5 bra L_mm%sq4k_clo;\n", ms);
        fprintf(out, "    add.u32 %%r14, %%r12, 4; cvt.u64.u32 %%rd8, %%r14; add.u64 %%rd9, %%rd7, %%rd8; ld.u8 %%r15, [%%rd9];\n");
        fprintf(out, "    sub.u32 %%r16, %%r12, 4; cvt.u64.u32 %%rd8, %%r16; add.u64 %%rd9, %%rd7, %%rd8; ld.u8 %%r17, [%%rd9];\n");
        fprintf(out, "    cvt.u64.u32 %%rd8, %%r12; add.u64 %%rd9, %%rd7, %%rd8; ld.u8 %%r18, [%%rd9];\n");
        fprintf(out, "    and.b32 %%r19, %%r15, 15; shr.u32 %%r17, %%r17, 6; shl.b32 %%r17, %%r17, 4; or.b32 %%r19, %%r19, %%r17;\n");
        fprintf(out, "    shr.u32 %%r20, %%r15, 4; shr.u32 %%r18, %%r18, 6; shl.b32 %%r18, %%r18, 4; or.b32 %%r20, %%r20, %%r18;\n");
        fprintf(out, "    add.u32 %%r14, %%r13, 4; cvt.u64.u32 %%rd8, %%r14; add.u64 %%rd9, %%rd7, %%rd8; ld.u8 %%r15, [%%rd9];\n");
        fprintf(out, "    sub.u32 %%r16, %%r13, 4; cvt.u64.u32 %%rd8, %%r16; add.u64 %%rd9, %%rd7, %%rd8; ld.u8 %%r17, [%%rd9];\n");
        fprintf(out, "    cvt.u64.u32 %%rd8, %%r13; add.u64 %%rd9, %%rd7, %%rd8; ld.u8 %%r18, [%%rd9];\n");
        fprintf(out, "    and.b32 %%r21, %%r15, 15; shr.u32 %%r17, %%r17, 6; shl.b32 %%r17, %%r17, 4; or.b32 %%r21, %%r21, %%r17;\n");
        fprintf(out, "    shr.u32 %%r22, %%r15, 4; shr.u32 %%r18, %%r18, 6; shl.b32 %%r18, %%r18, 4; or.b32 %%r22, %%r22, %%r18;\n");
        fprintf(out, "    bra L_mm%sq4k_csc;\nL_mm%sq4k_clo:\n", ms, ms);
        fprintf(out, "    cvt.u64.u32 %%rd8, %%r12; add.u64 %%rd9, %%rd7, %%rd8; ld.u8 %%r19, [%%rd9]; and.b32 %%r19, %%r19, 63;\n");
        fprintf(out, "    add.u32 %%r14, %%r12, 4; cvt.u64.u32 %%rd8, %%r14; add.u64 %%rd9, %%rd7, %%rd8; ld.u8 %%r20, [%%rd9]; and.b32 %%r20, %%r20, 63;\n");
        fprintf(out, "    cvt.u64.u32 %%rd8, %%r13; add.u64 %%rd9, %%rd7, %%rd8; ld.u8 %%r21, [%%rd9]; and.b32 %%r21, %%r21, 63;\n");
        fprintf(out, "    add.u32 %%r14, %%r13, 4; cvt.u64.u32 %%rd8, %%r14; add.u64 %%rd9, %%rd7, %%rd8; ld.u8 %%r22, [%%rd9]; and.b32 %%r22, %%r22, 63;\n");
        fprintf(out, "L_mm%sq4k_csc:\n", ms);
        fprintf(out, "    cvt.rn.f32.u32 %%f3, %%r19; mul.f32 %%f3, %%f1, %%f3;\n");
        fprintf(out, "    cvt.rn.f32.u32 %%f4, %%r20; mul.f32 %%f4, %%f2, %%f4; neg.f32 %%f4, %%f4;\n");
        fprintf(out, "    cvt.rn.f32.u32 %%f5, %%r21; mul.f32 %%f5, %%f1, %%f5;\n");
        fprintf(out, "    cvt.rn.f32.u32 %%f6, %%r22; mul.f32 %%f6, %%f2, %%f6; neg.f32 %%f6, %%f6;\n");
        fprintf(out, "    shl.b32 %%r23, %%r11, 5; add.u32 %%r23, %%r23, %%r5; add.u32 %%r23, %%r23, 16;\n");
        fprintf(out, "    cvt.u64.u32 %%rd8, %%r23; add.u64 %%rd9, %%rd6, %%rd8; ld.u8 %%r24, [%%rd9];\n");
        fprintf(out, "    and.b32 %%r25, %%r24, 15; shr.u32 %%r26, %%r24, 4;\n");
        fprintf(out, "    cvt.rn.f32.u32 %%f7, %%r25; fma.rn.f32 %%fw0, %%f3, %%f7, %%f4;\n");
        fprintf(out, "    cvt.rn.f32.u32 %%f9, %%r26; fma.rn.f32 %%fw1, %%f5, %%f9, %%f6;\n");
        fprintf(out, "    shl.b32 %%r27, %%r8, 8; shl.b32 %%r23, %%r11, 6; add.u32 %%r27, %%r27, %%r23; add.u32 %%r27, %%r27, %%r5;\n");
        fprintf(out, "    mul.wide.u32 %%rd10, %%r27, 4; add.u64 %%rd17, %%rd1, %%rd10;\n");
        for (int t = 0; t < max_m; t++) {
            fprintf(out, "    @!%%pt%d bra L_mm%sq4k_ce;\n", t, ms);
            fprintf(out, "    ld.f32 %%f11, [%%rd17]; ld.f32 %%f12, [%%rd17+128];\n");
            fprintf(out, "    fma.rn.f32 %%fa%d, %%fw0, %%f11, %%fa%d; fma.rn.f32 %%fa%d, %%fw1, %%f12, %%fa%d;\n", t, t, t, t);
            if (t < max_m - 1) fprintf(out, "    add.u64 %%rd17, %%rd17, %%rd15;\n");
        }
        fprintf(out, "L_mm%sq4k_ce:\n    add.u32 %%r11, %%r11, 1; bra L_mm%sq4k_c;\n", ms, ms);
        fprintf(out, "L_mm%sq4k_cd:\n    add.u32 %%r8, %%r8, 1; bra L_mm%sq4k_sb;\n", ms, ms);
    } else if (fmt == GGUF_PTX_Q6_K) {
        fprintf(out, "    shr.u32 %%r6, %%r1, 8; mul.lo.u32 %%r7, %%r6, 210;\n");
        fprintf(out, "    mul.wide.u32 %%rd3, %%r3, %%r7; add.u64 %%rd4, %%rd0, %%rd3;\n");
        fprintf(out, "    shr.u32 %%r8, %%r5, 4;\n");
        fprintf(out, "    mov.u32 %%r9, 0;\n");
        fprintf(out, "L_mm%sq6k_sb:\n    setp.ge.u32 %%p1, %%r9, %%r6; @%%p1 bra L_mm%s_%s_s;\n", ms, ms, suf);
        fprintf(out, "    mul.wide.u32 %%rd5, %%r9, 210; add.u64 %%rd6, %%rd4, %%rd5;\n");
        fprintf(out, "    ld.u8 %%r10, [%%rd6+208]; ld.u8 %%r11, [%%rd6+209]; shl.b32 %%r11, %%r11, 8; or.b32 %%r10, %%r10, %%r11;\n");
        fprintf(out, "    { .reg .b16 %%hb; cvt.u16.u32 %%hb, %%r10; cvt.f32.f16 %%f1, %%hb; }\n");
        fprintf(out, "    mov.u32 %%r12, 0;\n");
        fprintf(out, "L_mm%sq6k_h:\n    setp.ge.u32 %%p2, %%r12, 2; @%%p2 bra L_mm%sq6k_hd;\n", ms, ms);
        fprintf(out, "    shl.b32 %%r13, %%r12, 6; add.u32 %%r13, %%r13, %%r5; cvt.u64.u32 %%rd7, %%r13; add.u64 %%rd7, %%rd6, %%rd7;\n");
        fprintf(out, "    ld.u8 %%r14, [%%rd7]; ld.u8 %%r15, [%%rd7+32];\n");
        fprintf(out, "    shl.b32 %%r13, %%r12, 5; add.u32 %%r13, %%r13, %%r5; add.u32 %%r13, %%r13, 128; cvt.u64.u32 %%rd7, %%r13; add.u64 %%rd7, %%rd6, %%rd7;\n");
        fprintf(out, "    ld.u8 %%r16, [%%rd7];\n");
        fprintf(out, "    shl.b32 %%r13, %%r12, 3; add.u32 %%r13, %%r13, %%r8; add.u32 %%r13, %%r13, 192; cvt.u64.u32 %%rd7, %%r13; add.u64 %%rd7, %%rd6, %%rd7;\n");
        fprintf(out, "    ld.u8 %%r17, [%%rd7]; setp.ge.u32 %%p5, %%r17, 128; @%%p5 sub.s32 %%r17, %%r17, 256; cvt.rn.f32.s32 %%f2, %%r17;\n");
        fprintf(out, "    ld.u8 %%r18, [%%rd7+2]; setp.ge.u32 %%p5, %%r18, 128; @%%p5 sub.s32 %%r18, %%r18, 256; cvt.rn.f32.s32 %%f3, %%r18;\n");
        fprintf(out, "    ld.u8 %%r19, [%%rd7+4]; setp.ge.u32 %%p5, %%r19, 128; @%%p5 sub.s32 %%r19, %%r19, 256; cvt.rn.f32.s32 %%f4, %%r19;\n");
        fprintf(out, "    ld.u8 %%r20, [%%rd7+6]; setp.ge.u32 %%p5, %%r20, 128; @%%p5 sub.s32 %%r20, %%r20, 256; cvt.rn.f32.s32 %%f5, %%r20;\n");
        fprintf(out, "    and.b32 %%r21, %%r14, 15; and.b32 %%r22, %%r16, 3; shl.b32 %%r22, %%r22, 4; or.b32 %%r21, %%r21, %%r22; sub.s32 %%r21, %%r21, 32;\n");
        fprintf(out, "    and.b32 %%r23, %%r15, 15; shr.u32 %%r22, %%r16, 2; and.b32 %%r22, %%r22, 3; shl.b32 %%r22, %%r22, 4; or.b32 %%r23, %%r23, %%r22; sub.s32 %%r23, %%r23, 32;\n");
        fprintf(out, "    shr.u32 %%r24, %%r14, 4; shr.u32 %%r22, %%r16, 4; and.b32 %%r22, %%r22, 3; shl.b32 %%r22, %%r22, 4; or.b32 %%r24, %%r24, %%r22; sub.s32 %%r24, %%r24, 32;\n");
        fprintf(out, "    shr.u32 %%r25, %%r15, 4; shr.u32 %%r22, %%r16, 6; and.b32 %%r22, %%r22, 3; shl.b32 %%r22, %%r22, 4; or.b32 %%r25, %%r25, %%r22; sub.s32 %%r25, %%r25, 32;\n");
        fprintf(out, "    cvt.rn.f32.s32 %%fw0, %%r21; mul.f32 %%fw0, %%fw0, %%f2; mul.f32 %%fw0, %%fw0, %%f1;\n");
        fprintf(out, "    cvt.rn.f32.s32 %%fw1, %%r23; mul.f32 %%fw1, %%fw1, %%f3; mul.f32 %%fw1, %%fw1, %%f1;\n");
        fprintf(out, "    cvt.rn.f32.s32 %%fw2, %%r24; mul.f32 %%fw2, %%fw2, %%f4; mul.f32 %%fw2, %%fw2, %%f1;\n");
        fprintf(out, "    cvt.rn.f32.s32 %%fw3, %%r25; mul.f32 %%fw3, %%fw3, %%f5; mul.f32 %%fw3, %%fw3, %%f1;\n");
        fprintf(out, "    shl.b32 %%r26, %%r9, 8; shl.b32 %%r27, %%r12, 7; add.u32 %%r26, %%r26, %%r27; add.u32 %%r26, %%r26, %%r5;\n");
        fprintf(out, "    mul.wide.u32 %%rd8, %%r26, 4; add.u64 %%rd17, %%rd1, %%rd8;\n");
        for (int t = 0; t < max_m; t++) {
            fprintf(out, "    @!%%pt%d bra L_mm%sq6k_he;\n", t, ms);
            fprintf(out, "    ld.f32 %%f10, [%%rd17]; ld.f32 %%f11, [%%rd17+128]; ld.f32 %%f12, [%%rd17+256]; ld.f32 %%f13, [%%rd17+384];\n");
            fprintf(out, "    fma.rn.f32 %%fa%d, %%fw0, %%f10, %%fa%d; fma.rn.f32 %%fa%d, %%fw1, %%f11, %%fa%d; fma.rn.f32 %%fa%d, %%fw2, %%f12, %%fa%d; fma.rn.f32 %%fa%d, %%fw3, %%f13, %%fa%d;\n",
                    t, t, t, t, t, t, t, t);
            if (t < max_m - 1) fprintf(out, "    add.u64 %%rd17, %%rd17, %%rd15;\n");
        }
        fprintf(out, "L_mm%sq6k_he:\n    add.u32 %%r12, %%r12, 1; bra L_mm%sq6k_h;\n", ms, ms);
        fprintf(out, "L_mm%sq6k_hd:\n    add.u32 %%r9, %%r9, 1; bra L_mm%sq6k_sb;\n", ms, ms);
    }

    fprintf(out, "L_mm%s_%s_s:\n", ms, suf);
    fprintf(out, "    mul.wide.u32 %%rd12, %%r3, 4; add.u64 %%rd17, %%rd2, %%rd12;\n");
    for (int t = 0; t < max_m; t++) {
        fprintf(out, "    @!%%pt%d bra L_mm%s_%s_d;\n", t, ms, suf);
        fprintf(out, "    { .param .f32 _wr, _wa; st.param.f32 [_wa], %%fa%d; call.uni (_wr), zcc_aot_warp_sum, (_wa); ld.param.f32 %%f0, [_wr]; }\n", t);
        fprintf(out, "    setp.eq.u32 %%p3, %%r5, 0; @%%p3 bra L_mm%s_%s_w%d; bra L_mm%s_%s_n%d;\n", ms, suf, t, ms, suf, t);
        fprintf(out, "L_mm%s_%s_w%d:\n", ms, suf, t);
        fprintf(out, "    setp.ne.u32 %%p4, %%r2, 0; @%%p4 ld.f32 %%f14, [%%rd17]; @%%p4 add.f32 %%f0, %%f14, %%f0; st.f32 [%%rd17], %%f0;\n");
        fprintf(out, "L_mm%s_%s_n%d:\n", ms, suf, t);
        if (t < max_m - 1) fprintf(out, "    add.u64 %%rd17, %%rd17, %%rd16;\n");
    }
    fprintf(out, "L_mm%s_%s_d:\n    ret;\n}\n\n", ms, suf);
}

static void gguf_ptx_emit_mc_argmax_fn_body(FILE *out, const char *lbl, int chain_mode) {
    static const int offs[5] = { 16, 8, 4, 2, 1 };
    fprintf(out, "    .shared .align 4 .b8 s_val[32]; .shared .align 4 .b8 s_idx[32]; .shared .align 4 .b8 s_nan[4];\n");
    fprintf(out, "    .reg .pred %%p<5>; .reg .b32 %%r<16>; .reg .b64 %%rd<10>; .reg .f32 %%f<4>;\n");
    fprintf(out, "    ld.param.u64 %%rd0, [_plogits]; ld.param.u64 %%rd1, [_p1]; ld.param.u64 %%rd2, [_p2]; ld.param.u32 %%r2, [_pV];\n");
    fprintf(out, "    mov.u32 %%r0, %%tid.x; %s shr.u32 %%r3, %%r0, 5; and.b32 %%r4, %%r0, 31;\n",
            chain_mode ? "mov.u32 %r1, 0;" : "mov.u32 %r1, %ctaid.x;");
    fprintf(out, "    cvta.shared.u64 %%rd4, s_val; cvta.shared.u64 %%rd5, s_idx; cvta.shared.u64 %%rd6, s_nan;\n");
    fprintf(out, "    setp.eq.u32 %%p0, %%r0, 0; @%%p0 st.u32 [%%rd6], 0; bar.sync 0;\n");
    fprintf(out, "    mul.wide.u32 %%rd7, %%r1, %%r2; shl.b64 %%rd7, %%rd7, 2; add.u64 %%rd0, %%rd0, %%rd7;\n");
    fprintf(out, "    mov.f32 %%f0, 0fFF800000; mov.u32 %%r5, 4294967295; mov.u32 %%r6, %%r0; mov.u32 %%r14, 0;\n");
    fprintf(out, "L_%s_lp:\n    setp.ge.u32 %%p1, %%r6, %%r2; @%%p1 bra L_%s_ld;\n", lbl, lbl);
    fprintf(out, "    mul.wide.u32 %%rd7, %%r6, 4; add.u64 %%rd8, %%rd0, %%rd7; ld.f32 %%f1, [%%rd8];\n");
    fprintf(out, "    mov.b32 %%r7, %%f1; and.b32 %%r7, %%r7, 0x7FFFFFFF; setp.gt.u32 %%p2, %%r7, 0x7E967699; @%%p2 mov.u32 %%r14, 1;\n");
    fprintf(out, "    setp.gt.f32 %%p2, %%f1, %%f0; setp.eq.f32 %%p3, %%f1, %%f0; setp.lt.u32 %%p4, %%r6, %%r5; and.pred %%p3, %%p3, %%p4; or.pred %%p2, %%p2, %%p3;\n");
    fprintf(out, "    @%%p2 mov.f32 %%f0, %%f1; @%%p2 mov.u32 %%r5, %%r6;\n");
    fprintf(out, "    add.u32 %%r6, %%r6, 256; bra L_%s_lp;\nL_%s_ld:\n", lbl, lbl);
    fprintf(out, "    setp.ne.u32 %%p2, %%r14, 0; @%%p2 st.u32 [%%rd6], 1;\n");
    for (int i = 0; i < 5; i++) {
        fprintf(out, "    mov.b32 %%r8, %%f0; shfl.sync.down.b32 %%r9, %%r8, %d, 31, 0xffffffff; shfl.sync.down.b32 %%r10, %%r5, %d, 31, 0xffffffff; mov.b32 %%f1, %%r9;\n",
                offs[i], offs[i]);
        fprintf(out, "    setp.gt.f32 %%p2, %%f1, %%f0; setp.eq.f32 %%p3, %%f1, %%f0; setp.lt.u32 %%p4, %%r10, %%r5; and.pred %%p3, %%p3, %%p4; or.pred %%p2, %%p2, %%p3;\n");
        fprintf(out, "    @%%p2 mov.f32 %%f0, %%f1; @%%p2 mov.u32 %%r5, %%r10;\n");
    }
    fprintf(out, "    setp.eq.u32 %%p1, %%r4, 0; @%%p1 mul.wide.u32 %%rd7, %%r3, 4; @%%p1 add.u64 %%rd8, %%rd4, %%rd7; @%%p1 st.f32 [%%rd8], %%f0;\n");
    fprintf(out, "    @%%p1 add.u64 %%rd9, %%rd5, %%rd7; @%%p1 st.u32 [%%rd9], %%r5; bar.sync 0;\n");
    fprintf(out, "    setp.eq.u32 %%p0, %%r0, 0; @!%%p0 bra L_%s_dn;\n", lbl);
    for (int w = 1; w < 8; w++) {
        fprintf(out, "    ld.f32 %%f1, [%%rd4+%d]; ld.u32 %%r10, [%%rd5+%d];\n", w * 4, w * 4);
        fprintf(out, "    setp.gt.f32 %%p2, %%f1, %%f0; setp.eq.f32 %%p3, %%f1, %%f0; setp.lt.u32 %%p4, %%r10, %%r5; and.pred %%p3, %%p3, %%p4; or.pred %%p2, %%p2, %%p3;\n");
        fprintf(out, "    @%%p2 mov.f32 %%f0, %%f1; @%%p2 mov.u32 %%r5, %%r10;\n");
    }
    fprintf(out, "    ld.u32 %%r13, [%%rd6]; setp.ne.u32 %%p2, %%r13, 0; setp.eq.u32 %%p3, %%r5, 4294967295; or.pred %%p2, %%p2, %%p3;\n");
    if (chain_mode) {
        fprintf(out, "    selp.u32 %%r15, 4294967295, %%r5, %%p2; selp.u32 %%r5, 0, %%r5, %%p2;\n");
        fprintf(out, "    ld.u32 %%r11, [%%rd1+4]; ld.u32 %%r12, [%%rd1+12];\n");
        fprintf(out, "    mul.wide.u32 %%rd7, %%r12, 4; add.u64 %%rd8, %%rd2, %%rd7; st.u32 [%%rd8], %%r15;\n");
        fprintf(out, "    st.u32 [%%rd1], %%r5; add.u32 %%r11, %%r11, 1; st.u32 [%%rd1+4], %%r11; add.u32 %%r12, %%r12, 1; st.u32 [%%rd1+12], %%r12;\n");
    } else {
        fprintf(out, "    @%%p2 mov.f32 %%f0, 0f7FC00000; @%%p2 mov.u32 %%r5, 0;\n");
        fprintf(out, "    mul.wide.u32 %%rd7, %%r1, 4; add.u64 %%rd8, %%rd1, %%rd7; st.u32 [%%rd8], %%r5;\n");
        fprintf(out, "    add.u64 %%rd9, %%rd2, %%rd7; st.f32 [%%rd9], %%f0;\n");
    }
    fprintf(out, "L_%s_dn:\n    ret;\n}\n\n", lbl);
}

static void gguf_ptx_emit_mc_argmax_fns(FILE *out) {
    fprintf(out, ".visible .entry zcc_aot_mc_argmax_f32(.param .u64 _plogits, .param .u64 _p1, .param .u64 _p2, .param .u32 _pV) {\n");
    gguf_ptx_emit_mc_argmax_fn_body(out, "amx", 0);
    fprintf(out, ".visible .entry zcc_aot_mc_draft_chain_step(.param .u64 _plogits, .param .u64 _p1, .param .u64 _p2, .param .u32 _pV) {\n");
    gguf_ptx_emit_mc_argmax_fn_body(out, "dcs", 1);
}

static void gguf_ptx_emit_mc_f32_to_f16_fn(FILE *out) {
    fprintf(out, ".visible .entry zcc_aot_mc_f32_to_f16(.param .u64 _psrc, .param .u64 _pdst, .param .u32 _pn) {\n");
    fprintf(out, "    .reg .pred %%p0; .reg .b32 %%r<4>; .reg .b64 %%rd<5>; .reg .f32 %%f0;\n");
    fprintf(out, "    ld.param.u64 %%rd0, [_psrc]; ld.param.u64 %%rd1, [_pdst]; ld.param.u32 %%r0, [_pn];\n");
    fprintf(out, "    mov.u32 %%r1, %%ctaid.x; mov.u32 %%r2, %%tid.x; mad.lo.u32 %%r2, %%r1, 256, %%r2;\n");
    fprintf(out, "    setp.ge.u32 %%p0, %%r2, %%r0; @%%p0 bra L_mcc16_d;\n");
    fprintf(out, "    mul.wide.u32 %%rd2, %%r2, 4; add.u64 %%rd3, %%rd0, %%rd2; ld.f32 %%f0, [%%rd3];\n");
    fprintf(out, "    { .reg .b16 %%hb; cvt.rn.f16.f32 %%hb, %%f0; cvt.u32.u16 %%r3, %%hb; }\n");
    fprintf(out, "    mul.wide.u32 %%rd2, %%r2, 2; add.u64 %%rd4, %%rd1, %%rd2; st.u16 [%%rd4], %%r3;\n");
    fprintf(out, "L_mcc16_d:\n    ret;\n}\n\n");
}

static void gguf_ptx_emit_mc_tc_fns(FILE *out) {
    gguf_ptx_emit_mc_f32_to_f16_fn(out);

    fprintf(out, ".visible .entry zcc_aot_mc_gemm_tc_f16(\n"
                 "    .param .u64 _pxf16, .param .u64 _pw, .param .u64 _pwoff, .param .u64 _py,\n"
                 "    .param .u32 _pN, .param .u32 _pK, .param .u32 _padd) {\n");
    fprintf(out, "    .reg .pred %%p<8>; .reg .b32 %%r<21>; .reg .b64 %%rd<11>; .reg .f32 %%f<6>;\n");
    fprintf(out, "    ld.param.u64 %%rd0, [_pxf16]; ld.param.u64 %%rd1, [_pw]; ld.param.u64 %%rd9, [_pwoff]; add.u64 %%rd1, %%rd1, %%rd9;\n");
    fprintf(out, "    ld.param.u64 %%rd2, [_py]; ld.param.u32 %%r0, [_pN]; ld.param.u32 %%r1, [_pK]; ld.param.u32 %%r2, [_padd];\n");
    fprintf(out, "    mov.u32 %%r3, %%tid.x; and.b32 %%r5, %%r3, 31; mov.u32 %%r4, %%ctaid.x; shl.b32 %%r10, %%r4, 3;\n");
    fprintf(out, "    setp.ge.u32 %%p0, %%r10, %%r0; @%%p0 bra L_mctc_d;\n");
    fprintf(out, "    shr.u32 %%r6, %%r5, 2; and.b32 %%r7, %%r5, 3; shl.b32 %%r8, %%r7, 1;\n");
    fprintf(out, "    sub.u32 %%r9, %%r0, 1; setp.ne.u32 %%p7, %%r2, 0;\n");
    fprintf(out, "    add.u32 %%r11, %%r10, %%r6; min.u32 %%r11, %%r11, %%r9;\n");
    fprintf(out, "    mul.wide.u32 %%rd3, %%r11, %%r1; mov.u32 %%r12, 0;\n");
    fprintf(out, "    mov.f32 %%f0, 0f00000000; mov.f32 %%f1, 0f00000000; mov.f32 %%f2, 0f00000000; mov.f32 %%f3, 0f00000000;\n");
    fprintf(out, "L_mctc_k:\n    add.u32 %%r13, %%r12, 16; setp.gt.u32 %%p1, %%r13, %%r1; @%%p1 bra L_mctc_kd;\n");
    fprintf(out, "    add.u32 %%r14, %%r12, %%r8; mul.wide.u32 %%rd4, %%r14, 2; add.u64 %%rd5, %%rd0, %%rd4;\n");
    fprintf(out, "    ld.u32 %%r15, [%%rd5]; mov.u32 %%r16, %%r15;\n");
    fprintf(out, "    add.u32 %%r14, %%r14, 8; mul.wide.u32 %%rd4, %%r14, 2; add.u64 %%rd5, %%rd0, %%rd4;\n");
    fprintf(out, "    ld.u32 %%r17, [%%rd5]; mov.u32 %%r18, %%r17;\n");
    fprintf(out, "    add.u32 %%r14, %%r12, %%r8; cvt.u64.u32 %%rd4, %%r14; add.u64 %%rd6, %%rd3, %%rd4;\n");
    fprintf(out, "    shl.b64 %%rd7, %%rd6, 1; add.u64 %%rd8, %%rd1, %%rd7; ld.u32 %%r19, [%%rd8];\n");
    fprintf(out, "    add.u64 %%rd8, %%rd8, 16; ld.u32 %%r14, [%%rd8];\n");
    fprintf(out, "    mma.sync.aligned.m16n8k16.row.col.f32.f16.f16.f32 {%%f0, %%f1, %%f2, %%f3}, {%%r15, %%r16, %%r17, %%r18}, {%%r19, %%r14}, {%%f0, %%f1, %%f2, %%f3};\n");
    fprintf(out, "    mov.u32 %%r12, %%r13; bra L_mctc_k;\n");
    fprintf(out, "L_mctc_kd:\n    setp.eq.u32 %%p2, %%r6, 0;\n");
    fprintf(out, "    add.u32 %%r14, %%r10, %%r8; setp.lt.u32 %%p3, %%r14, %%r0; and.pred %%p4, %%p2, %%p3;\n");
    fprintf(out, "    @%%p4 mul.wide.u32 %%rd4, %%r14, 4; @%%p4 add.u64 %%rd5, %%rd2, %%rd4;\n");
    fprintf(out, "    and.pred %%p5, %%p4, %%p7; @%%p5 ld.f32 %%f4, [%%rd5]; @%%p5 add.f32 %%f0, %%f4, %%f0; @%%p4 st.f32 [%%rd5], %%f0;\n");
    fprintf(out, "    add.u32 %%r14, %%r14, 1; setp.lt.u32 %%p3, %%r14, %%r0; and.pred %%p4, %%p2, %%p3;\n");
    fprintf(out, "    @%%p4 mul.wide.u32 %%rd4, %%r14, 4; @%%p4 add.u64 %%rd5, %%rd2, %%rd4;\n");
    fprintf(out, "    and.pred %%p5, %%p4, %%p7; @%%p5 ld.f32 %%f5, [%%rd5]; @%%p5 add.f32 %%f1, %%f5, %%f1; @%%p4 st.f32 [%%rd5], %%f1;\n");
    fprintf(out, "L_mctc_d:\n    ret;\n}\n\n");
}

static void gguf_ptx_emit_mc_add_bias_fn(FILE *out) {
    fprintf(out, ".visible .entry zcc_aot_mc_add_bias(.param .u64 _pvec, .param .u64 _pw, .param .u64 _pboff, .param .u32 _pN) {\n");
    fprintf(out, "    .reg .pred %%p0; .reg .b32 %%r<4>; .reg .b64 %%rd<7>; .reg .f32 %%f<3>;\n");
    fprintf(out, "    ld.param.u64 %%rd0, [_pvec]; ld.param.u64 %%rd1, [_pw]; ld.param.u64 %%rd2, [_pboff]; add.u64 %%rd1, %%rd1, %%rd2;\n");
    fprintf(out, "    ld.param.u32 %%r0, [_pN]; mov.u32 %%r1, %%ctaid.x; mov.u32 %%r2, %%tid.x; mad.lo.u32 %%r2, %%r1, 256, %%r2;\n");
    fprintf(out, "    setp.ge.u32 %%p0, %%r2, %%r0; @%%p0 bra L_mcab_d;\n");
    fprintf(out, "    mul.wide.u32 %%rd3, %%r2, 4; add.u64 %%rd4, %%rd0, %%rd3; add.u64 %%rd5, %%rd1, %%rd3;\n");
    fprintf(out, "    ld.f32 %%f0, [%%rd4]; ld.f32 %%f1, [%%rd5]; add.f32 %%f2, %%f0, %%f1; st.f32 [%%rd4], %%f2;\n");
    fprintf(out, "L_mcab_d:\n    ret;\n}\n\n");
}

static void gguf_ptx_emit_mc_rope_kv_fn(FILE *out, const GgufPtxModel *m) {
    uint32_t half_hd = m->head_dim >> 1;
    uint32_t q_pairs = m->n_heads * half_hd;
    uint32_t total_pairs = (m->n_heads + m->n_kv_heads) * half_hd;
    uint32_t theta_u = gguf_ptx_f32_bits(m->rope_theta);
    uint32_t hd_u = gguf_ptx_f32_bits((float)m->head_dim);
    int is_neox = (strcmp(m->arch, "qwen2") == 0);
    uint32_t f32_pair_off = is_neox ? (half_hd * 4U) : 4U;
    uint32_t f16_pair_off = is_neox ? (half_hd * 2U) : 2U;
    const char *elem_reg = is_neox ? "%r7" : "%r10";

    /* 1) F32 KV RoPE + cache store */
    fprintf(out, ".visible .entry zcc_aot_mc_rope_kv(\n"
                 "    .param .u64 _pq, .param .u64 _pk, .param .u64 _pv,\n"
                 "    .param .u64 _pkc, .param .u64 _pvc, .param .u64 _pargs, .param .u32 _player) {\n");
    fprintf(out, "    .reg .pred %%p<4>; .reg .b32 %%r<12>; .reg .b64 %%rd<14>; .reg .f32 %%f<10>;\n");
    fprintf(out, "    ld.param.u64 %%rd0, [_pq]; ld.param.u64 %%rd1, [_pk]; ld.param.u64 %%rd2, [_pv];\n");
    fprintf(out, "    ld.param.u64 %%rd3, [_pkc]; ld.param.u64 %%rd4, [_pvc]; ld.param.u64 %%rd5, [_pargs];\n");
    fprintf(out, "    ld.param.u32 %%r0, [_player]; ld.u32 %%r1, [%%rd5+4]; ld.u32 %%r2, [%%rd5+8];\n");
    fprintf(out, "    mov.u32 %%r3, %%ctaid.x; mov.u32 %%r4, %%tid.x; mad.lo.u32 %%r5, %%r3, 128, %%r4;\n");
    fprintf(out, "    mul.wide.u32 %%rd6, %%r0, %%r2; cvt.u64.u32 %%rd7, %%r1; add.u64 %%rd6, %%rd6, %%rd7;\n");
    fprintf(out, "    mul.lo.u64 %%rd6, %%rd6, %u; shl.b64 %%rd6, %%rd6, 2;\n", (unsigned int)m->kv_dim);
    fprintf(out, "    add.u64 %%rd3, %%rd3, %%rd6; add.u64 %%rd4, %%rd4, %%rd6;\n");
    fprintf(out, "    setp.lt.u32 %%p0, %%r5, %u; @%%p0 bra L_mcrv_cp;\n    bra L_mcrv_rp;\nL_mcrv_cp:\n", (unsigned int)m->kv_dim);
    fprintf(out, "    mul.wide.u32 %%rd8, %%r5, 4; add.u64 %%rd9, %%rd2, %%rd8; ld.f32 %%f0, [%%rd9];\n");
    fprintf(out, "    add.u64 %%rd10, %%rd4, %%rd8; st.f32 [%%rd10], %%f0;\n");
    fprintf(out, "L_mcrv_rp:\n    setp.ge.u32 %%p1, %%r5, %u; @%%p1 bra L_mcrv_d;\n", (unsigned int)total_pairs);
    fprintf(out, "    lg2.approx.f32 %%f1, 0f%08X; cvt.rn.f32.u32 %%f2, %%r1; mov.f32 %%f9, 0f%08X;\n",
            (unsigned int)theta_u, (unsigned int)hd_u);
    fprintf(out, "    div.u32 %%r6, %%r5, %u; rem.u32 %%r7, %%r5, %u;\n", (unsigned int)half_hd, (unsigned int)half_hd);
    fprintf(out, "    setp.lt.u32 %%p2, %%r5, %u; sub.u32 %%r8, %%r6, %u;\n", (unsigned int)q_pairs, (unsigned int)m->n_heads);
    fprintf(out, "    selp.u32 %%r9, %%r6, %%r8, %%p2; mul.lo.u32 %%r9, %%r9, %u; shl.b32 %%r10, %%r7, 1; add.u32 %%r9, %%r9, %s;\n",
            (unsigned int)m->head_dim, elem_reg);
    fprintf(out, "    mul.wide.u32 %%rd11, %%r9, 4; selp.u64 %%rd12, %%rd0, %%rd1, %%p2; add.u64 %%rd12, %%rd12, %%rd11;\n");
    fprintf(out, "    cvt.rn.f32.u32 %%f3, %%r10; neg.f32 %%f3, %%f3; div.rn.f32 %%f3, %%f3, %%f9;\n");
    fprintf(out, "    mul.f32 %%f3, %%f3, %%f1; ex2.approx.f32 %%f3, %%f3; mul.f32 %%f3, %%f2, %%f3;\n");
    fprintf(out, "    cos.approx.f32 %%f4, %%f3; sin.approx.f32 %%f5, %%f3;\n");
    fprintf(out, "    ld.f32 %%f6, [%%rd12]; ld.f32 %%f7, [%%rd12+%u];\n", (unsigned int)f32_pair_off);
    fprintf(out, "    mul.f32 %%f8, %%f6, %%f4; mul.f32 %%f3, %%f7, %%f5; sub.f32 %%f8, %%f8, %%f3;\n");
    fprintf(out, "    mul.f32 %%f3, %%f6, %%f5; fma.rn.f32 %%f3, %%f7, %%f4, %%f3;\n");
    fprintf(out, "    @%%p2 st.f32 [%%rd12], %%f8; @%%p2 st.f32 [%%rd12+%u], %%f3;\n", (unsigned int)f32_pair_off);
    fprintf(out, "    @!%%p2 add.u64 %%rd13, %%rd3, %%rd11; @!%%p2 st.f32 [%%rd13], %%f8; @!%%p2 st.f32 [%%rd13+%u], %%f3;\n", (unsigned int)f32_pair_off);
    fprintf(out, "L_mcrv_d:\n    ret;\n}\n\n");

    /* 2) FP16 KV RoPE + cache store (2.00x KV VRAM reduction) */
    fprintf(out, ".visible .entry zcc_aot_mc_rope_kv_f16(\n"
                 "    .param .u64 _pq, .param .u64 _pk, .param .u64 _pv,\n"
                 "    .param .u64 _pkc, .param .u64 _pvc, .param .u64 _pargs, .param .u32 _player) {\n");
    fprintf(out, "    .reg .pred %%p<4>; .reg .b32 %%r<14>; .reg .b64 %%rd<14>; .reg .f32 %%f<10>;\n");
    fprintf(out, "    ld.param.u64 %%rd0, [_pq]; ld.param.u64 %%rd1, [_pk]; ld.param.u64 %%rd2, [_pv];\n");
    fprintf(out, "    ld.param.u64 %%rd3, [_pkc]; ld.param.u64 %%rd4, [_pvc]; ld.param.u64 %%rd5, [_pargs];\n");
    fprintf(out, "    ld.param.u32 %%r0, [_player]; ld.u32 %%r1, [%%rd5+4]; ld.u32 %%r2, [%%rd5+8];\n");
    fprintf(out, "    mov.u32 %%r3, %%ctaid.x; mov.u32 %%r4, %%tid.x; mad.lo.u32 %%r5, %%r3, 128, %%r4;\n");
    fprintf(out, "    mul.wide.u32 %%rd6, %%r0, %%r2; cvt.u64.u32 %%rd7, %%r1; add.u64 %%rd6, %%rd6, %%rd7;\n");
    fprintf(out, "    mul.lo.u64 %%rd6, %%rd6, %u; shl.b64 %%rd6, %%rd6, 1;\n", (unsigned int)m->kv_dim);
    fprintf(out, "    add.u64 %%rd3, %%rd3, %%rd6; add.u64 %%rd4, %%rd4, %%rd6;\n");
    fprintf(out, "    setp.lt.u32 %%p0, %%r5, %u; @%%p0 bra L_mcrv16_cp;\n    bra L_mcrv16_rp;\nL_mcrv16_cp:\n", (unsigned int)m->kv_dim);
    fprintf(out, "    mul.wide.u32 %%rd8, %%r5, 4; add.u64 %%rd9, %%rd2, %%rd8; ld.f32 %%f0, [%%rd9];\n");
    fprintf(out, "    { .reg .b16 %%hb; cvt.rn.f16.f32 %%hb, %%f0; cvt.u32.u16 %%r11, %%hb; }\n");
    fprintf(out, "    mul.wide.u32 %%rd8, %%r5, 2; add.u64 %%rd10, %%rd4, %%rd8; st.u16 [%%rd10], %%r11;\n");
    fprintf(out, "L_mcrv16_rp:\n    setp.ge.u32 %%p1, %%r5, %u; @%%p1 bra L_mcrv16_d;\n", (unsigned int)total_pairs);
    fprintf(out, "    lg2.approx.f32 %%f1, 0f%08X; cvt.rn.f32.u32 %%f2, %%r1; mov.f32 %%f9, 0f%08X;\n",
            (unsigned int)theta_u, (unsigned int)hd_u);
    fprintf(out, "    div.u32 %%r6, %%r5, %u; rem.u32 %%r7, %%r5, %u;\n", (unsigned int)half_hd, (unsigned int)half_hd);
    fprintf(out, "    setp.lt.u32 %%p2, %%r5, %u; sub.u32 %%r8, %%r6, %u;\n", (unsigned int)q_pairs, (unsigned int)m->n_heads);
    fprintf(out, "    selp.u32 %%r9, %%r6, %%r8, %%p2; mul.lo.u32 %%r9, %%r9, %u; shl.b32 %%r10, %%r7, 1; add.u32 %%r9, %%r9, %s;\n",
            (unsigned int)m->head_dim, elem_reg);
    fprintf(out, "    mul.wide.u32 %%rd11, %%r9, 4; selp.u64 %%rd12, %%rd0, %%rd1, %%p2; add.u64 %%rd12, %%rd12, %%rd11;\n");
    fprintf(out, "    cvt.rn.f32.u32 %%f3, %%r10; neg.f32 %%f3, %%f3; div.rn.f32 %%f3, %%f3, %%f9;\n");
    fprintf(out, "    mul.f32 %%f3, %%f3, %%f1; ex2.approx.f32 %%f3, %%f3; mul.f32 %%f3, %%f2, %%f3;\n");
    fprintf(out, "    cos.approx.f32 %%f4, %%f3; sin.approx.f32 %%f5, %%f3;\n");
    fprintf(out, "    ld.f32 %%f6, [%%rd12]; ld.f32 %%f7, [%%rd12+%u];\n", (unsigned int)f32_pair_off);
    fprintf(out, "    mul.f32 %%f8, %%f6, %%f4; mul.f32 %%f3, %%f7, %%f5; sub.f32 %%f8, %%f8, %%f3;\n");
    fprintf(out, "    mul.f32 %%f3, %%f6, %%f5; fma.rn.f32 %%f3, %%f7, %%f4, %%f3;\n");
    fprintf(out, "    { .reg .b16 %%ha, %%hb; cvt.rn.f16.f32 %%ha, %%f8; cvt.rn.f16.f32 %%hb, %%f3; cvt.u32.u16 %%r11, %%ha; cvt.u32.u16 %%r12, %%hb; }\n");
    fprintf(out, "    @%%p2 st.f32 [%%rd12], %%f8; @%%p2 st.f32 [%%rd12+%u], %%f3;\n", (unsigned int)f32_pair_off);
    fprintf(out, "    @!%%p2 mul.wide.u32 %%rd11, %%r9, 2; @!%%p2 add.u64 %%rd13, %%rd3, %%rd11;\n");
    fprintf(out, "    @!%%p2 st.u16 [%%rd13], %%r11; @!%%p2 st.u16 [%%rd13+%u], %%r12;\n", (unsigned int)f16_pair_off);
    fprintf(out, "L_mcrv16_d:\n    ret;\n}\n\n");

    /* 3) Q8_0 KV helpers: in-place Q/K RoPE + warp-per-block Q8_0 KV quantizer (when head_dim % 32 == 0) */
    if ((m->head_dim & 31U) == 0U && (m->kv_dim & 31U) == 0U) {
        uint32_t nb_kv = m->kv_dim >> 5;
        uint32_t q8_row_bytes = nb_kv * 34U;
        fprintf(out, ".visible .entry zcc_aot_mc_rope_inplace(.param .u64 _pq, .param .u64 _pk, .param .u64 _pargs) {\n");
        fprintf(out, "    .reg .pred %%p<4>; .reg .b32 %%r<12>; .reg .b64 %%rd<14>; .reg .f32 %%f<10>;\n");
        fprintf(out, "    ld.param.u64 %%rd0, [_pq]; ld.param.u64 %%rd1, [_pk]; ld.param.u64 %%rd5, [_pargs];\n");
        fprintf(out, "    ld.u32 %%r1, [%%rd5+4]; mov.u32 %%r3, %%ctaid.x; mov.u32 %%r4, %%tid.x; mad.lo.u32 %%r5, %%r3, 128, %%r4;\n");
        fprintf(out, "    setp.ge.u32 %%p1, %%r5, %u; @%%p1 bra L_mcri_d;\n", (unsigned int)total_pairs);
        fprintf(out, "    lg2.approx.f32 %%f1, 0f%08X; cvt.rn.f32.u32 %%f2, %%r1; mov.f32 %%f9, 0f%08X;\n",
                (unsigned int)theta_u, (unsigned int)hd_u);
        fprintf(out, "    div.u32 %%r6, %%r5, %u; rem.u32 %%r7, %%r5, %u;\n", (unsigned int)half_hd, (unsigned int)half_hd);
        fprintf(out, "    setp.lt.u32 %%p2, %%r5, %u; sub.u32 %%r8, %%r6, %u;\n", (unsigned int)q_pairs, (unsigned int)m->n_heads);
        fprintf(out, "    selp.u32 %%r9, %%r6, %%r8, %%p2; mul.lo.u32 %%r9, %%r9, %u; shl.b32 %%r10, %%r7, 1; add.u32 %%r9, %%r9, %s;\n",
                (unsigned int)m->head_dim, elem_reg);
        fprintf(out, "    mul.wide.u32 %%rd11, %%r9, 4; selp.u64 %%rd12, %%rd0, %%rd1, %%p2; add.u64 %%rd12, %%rd12, %%rd11;\n");
        fprintf(out, "    cvt.rn.f32.u32 %%f3, %%r10; neg.f32 %%f3, %%f3; div.rn.f32 %%f3, %%f3, %%f9;\n");
        fprintf(out, "    mul.f32 %%f3, %%f3, %%f1; ex2.approx.f32 %%f3, %%f3; mul.f32 %%f3, %%f2, %%f3;\n");
        fprintf(out, "    cos.approx.f32 %%f4, %%f3; sin.approx.f32 %%f5, %%f3;\n");
        fprintf(out, "    ld.f32 %%f6, [%%rd12]; ld.f32 %%f7, [%%rd12+%u];\n", (unsigned int)f32_pair_off);
        fprintf(out, "    mul.f32 %%f8, %%f6, %%f4; mul.f32 %%f3, %%f7, %%f5; sub.f32 %%f8, %%f8, %%f3;\n");
        fprintf(out, "    mul.f32 %%f3, %%f6, %%f5; fma.rn.f32 %%f3, %%f7, %%f4, %%f3;\n");
        fprintf(out, "    st.f32 [%%rd12], %%f8; st.f32 [%%rd12+%u], %%f3;\n", (unsigned int)f32_pair_off);
        fprintf(out, "L_mcri_d:\n    ret;\n}\n\n");

        fprintf(out, ".visible .entry zcc_aot_mc_kv_store_q8_0(\n"
                     "    .param .u64 _pk, .param .u64 _pv, .param .u64 _pkc, .param .u64 _pvc,\n"
                     "    .param .u64 _pargs, .param .u32 _player) {\n");
        fprintf(out, "    .reg .pred %%p<4>; .reg .b32 %%r<16>; .reg .b64 %%rd<14>; .reg .f32 %%f<8>;\n");
        fprintf(out, "    ld.param.u64 %%rd0, [_pk]; ld.param.u64 %%rd1, [_pv];\n");
        fprintf(out, "    ld.param.u64 %%rd2, [_pkc]; ld.param.u64 %%rd3, [_pvc];\n");
        fprintf(out, "    ld.param.u64 %%rd4, [_pargs]; ld.param.u32 %%r0, [_player];\n");
        fprintf(out, "    ld.u32 %%r1, [%%rd4+4]; ld.u32 %%r2, [%%rd4+8];\n");
        fprintf(out, "    mov.u32 %%r3, %%ctaid.x; mov.u32 %%r4, %%tid.x; and.b32 %%r4, %%r4, 31;\n");
        fprintf(out, "    setp.ge.u32 %%p0, %%r3, %u; @%%p0 bra L_mckq8_d;\n", (unsigned int)(2U * nb_kv));
        fprintf(out, "    setp.lt.u32 %%p1, %%r3, %u; sub.u32 %%r5, %%r3, %u; selp.u32 %%r6, %%r3, %%r5, %%p1;\n",
                (unsigned int)nb_kv, (unsigned int)nb_kv);
        fprintf(out, "    mul.wide.u32 %%rd5, %%r0, %%r2; cvt.u64.u32 %%rd6, %%r1; add.u64 %%rd5, %%rd5, %%rd6;\n");
        fprintf(out, "    mul.lo.u64 %%rd5, %%rd5, %u;\n", (unsigned int)q8_row_bytes);
        fprintf(out, "    selp.u64 %%rd7, %%rd2, %%rd3, %%p1; add.u64 %%rd7, %%rd7, %%rd5;\n");
        fprintf(out, "    mul.wide.u32 %%rd8, %%r6, 34; add.u64 %%rd7, %%rd7, %%rd8;\n");
        fprintf(out, "    selp.u64 %%rd9, %%rd0, %%rd1, %%p1; shl.b32 %%r7, %%r6, 5; add.u32 %%r7, %%r7, %%r4;\n");
        fprintf(out, "    mul.wide.u32 %%rd10, %%r7, 4; add.u64 %%rd9, %%rd9, %%rd10; ld.f32 %%f0, [%%rd9];\n");
        fprintf(out, "    abs.f32 %%f1, %%f0;\n");
        fprintf(out, "    { .param .f32 _wr, _wa; st.param.f32 [_wa], %%f1; call.uni (_wr), zcc_aot_warp_max, (_wa); ld.param.f32 %%f2, [_wr]; }\n");
        fprintf(out, "    div.rn.f32 %%f3, %%f2, 0f42FE0000;\n");
        fprintf(out, "    { .reg .b16 %%hb; cvt.rn.f16.f32 %%hb, %%f3; cvt.u32.u16 %%r8, %%hb; cvt.f32.f16 %%f3, %%hb; }\n");
        fprintf(out, "    setp.eq.u32 %%p2, %%r4, 0; @%%p2 and.b32 %%r9, %%r8, 255; @%%p2 shr.u32 %%r10, %%r8, 8;\n");
        fprintf(out, "    @%%p2 st.u8 [%%rd7], %%r9; @%%p2 st.u8 [%%rd7+1], %%r10;\n");
        fprintf(out, "    mov.f32 %%f4, 0f00000000; setp.gt.f32 %%p3, %%f3, 0f00000000; @%%p3 div.rn.f32 %%f4, 0f3F800000, %%f3;\n");
        fprintf(out, "    mul.f32 %%f5, %%f0, %%f4; cvt.rni.s32.f32 %%r11, %%f5; max.s32 %%r11, %%r11, -127; min.s32 %%r11, %%r11, 127;\n");
        fprintf(out, "    add.u32 %%r12, %%r4, 2; cvt.u64.u32 %%rd11, %%r12; add.u64 %%rd11, %%rd7, %%rd11; st.u8 [%%rd11], %%r11;\n");
        fprintf(out, "L_mckq8_d:\n    ret;\n}\n\n");
    }
}

static void gguf_ptx_emit_mc_attn_fn(FILE *out, const GgufPtxModel *m) {
    uint32_t kv_group = m->n_heads / m->n_kv_heads;
    uint32_t chunks = (m->head_dim + 31U) / 32U;
    uint32_t hd_u = gguf_ptx_f32_bits((float)m->head_dim);

    /* 1) F32 KV FlashAttention-2 */
    fprintf(out, ".visible .entry zcc_aot_mc_attn_gqa(\n"
                 "    .param .u64 _pq, .param .u64 _pkc, .param .u64 _pvc,\n"
                 "    .param .u64 _pout, .param .u64 _pargs, .param .u32 _player) {\n");
    fprintf(out, "    .reg .pred %%p<4>; .reg .b32 %%r<12>; .reg .b64 %%rd<14>;\n");
    fprintf(out, "    .reg .f32 %%f<14>; .reg .f32 %%fq<8>; .reg .f32 %%fa<8>;\n");
    fprintf(out, "    ld.param.u64 %%rd0, [_pq]; ld.param.u64 %%rd1, [_pkc]; ld.param.u64 %%rd2, [_pvc];\n");
    fprintf(out, "    ld.param.u64 %%rd3, [_pout]; ld.param.u64 %%rd4, [_pargs]; ld.param.u32 %%r0, [_player];\n");
    fprintf(out, "    mov.u32 %%r1, %%ctaid.x; setp.ge.u32 %%p0, %%r1, %u; @%%p0 bra L_mca_done;\n", (unsigned int)m->n_heads);
    fprintf(out, "    mov.u32 %%r2, %%tid.x; and.b32 %%r2, %%r2, 31;\n");
    fprintf(out, "    ld.u32 %%r3, [%%rd4+4]; ld.u32 %%r4, [%%rd4+8];\n");
    fprintf(out, "    div.u32 %%r5, %%r1, %u; mul.lo.u32 %%r6, %%r5, %u;\n", (unsigned int)kv_group, (unsigned int)m->head_dim);
    fprintf(out, "    mul.lo.u32 %%r7, %%r1, %u; mul.wide.u32 %%rd5, %%r7, 4;\n", (unsigned int)m->head_dim);
    fprintf(out, "    add.u64 %%rd6, %%rd0, %%rd5; add.u64 %%rd7, %%rd3, %%rd5;\n");
    fprintf(out, "    mov.f32 %%f0, 0f%08X; rsqrt.approx.f32 %%f0, %%f0;\n", (unsigned int)hd_u);
    for (uint32_t c = 0U; c < chunks; c++) {
        fprintf(out, "    add.u32 %%r8, %%r2, %u; setp.lt.u32 %%p1, %%r8, %u;\n", (unsigned int)(c * 32U), (unsigned int)m->head_dim);
        fprintf(out, "    mov.f32 %%fq%u, 0f00000000; mov.f32 %%fa%u, 0f00000000;\n", (unsigned int)c, (unsigned int)c);
        fprintf(out, "    @%%p1 mul.wide.u32 %%rd8, %%r8, 4; @%%p1 add.u64 %%rd8, %%rd6, %%rd8; @%%p1 ld.f32 %%fq%u, [%%rd8];\n", (unsigned int)c);
    }
    fprintf(out, "    mul.wide.u32 %%rd12, %%r0, %%r4;\n");
    fprintf(out, "    mov.f32 %%f1, 0fF149F2CA; mov.f32 %%f2, 0f00000000; mov.u32 %%r9, 0;\n");
    fprintf(out, "L_mca_t:\n    setp.gt.u32 %%p0, %%r9, %%r3; @%%p0 bra L_mca_td;\n");
    fprintf(out, "    cvt.u64.u32 %%rd8, %%r9; add.u64 %%rd9, %%rd12, %%rd8; mul.lo.u64 %%rd9, %%rd9, %u;\n", (unsigned int)m->kv_dim);
    fprintf(out, "    cvt.u64.u32 %%rd8, %%r6; add.u64 %%rd9, %%rd9, %%rd8; shl.b64 %%rd9, %%rd9, 2;\n");
    fprintf(out, "    add.u64 %%rd10, %%rd1, %%rd9; add.u64 %%rd11, %%rd2, %%rd9;\n");
    fprintf(out, "    mov.f32 %%f3, 0f00000000;\n");
    for (uint32_t c = 0U; c < chunks; c++) {
        fprintf(out, "    add.u32 %%r8, %%r2, %u; setp.lt.u32 %%p1, %%r8, %u;\n", (unsigned int)(c * 32U), (unsigned int)m->head_dim);
        fprintf(out, "    @%%p1 mul.wide.u32 %%rd8, %%r8, 4; @%%p1 add.u64 %%rd8, %%rd10, %%rd8; @%%p1 ld.f32 %%f4, [%%rd8];\n");
        fprintf(out, "    @%%p1 fma.rn.f32 %%f3, %%fq%u, %%f4, %%f3;\n", (unsigned int)c);
    }
    fprintf(out, "    { .param .f32 _wr, _wa; st.param.f32 [_wa], %%f3; call.uni (_wr), zcc_aot_warp_sum, (_wa); ld.param.f32 %%f5, [_wr]; }\n");
    fprintf(out, "    mul.f32 %%f5, %%f5, %%f0;\n");
    fprintf(out, "    setp.eq.u32 %%p2, %%r9, 0; @%%p2 bra L_mca_t0;\n");
    fprintf(out, "    max.f32 %%f6, %%f1, %%f5;\n");
    fprintf(out, "    sub.f32 %%f7, %%f1, %%f6; mul.f32 %%f7, %%f7, 0f3FB8AA3B; ex2.approx.f32 %%f7, %%f7;\n");
    fprintf(out, "    sub.f32 %%f8, %%f5, %%f6; mul.f32 %%f8, %%f8, 0f3FB8AA3B; ex2.approx.f32 %%f8, %%f8;\n");
    fprintf(out, "    fma.rn.f32 %%f2, %%f2, %%f7, %%f8; mov.f32 %%f1, %%f6; bra L_mca_vu;\nL_mca_t0:\n");
    fprintf(out, "    mov.f32 %%f1, %%f5; mov.f32 %%f2, 0f3F800000; mov.f32 %%f7, 0f00000000; mov.f32 %%f8, 0f3F800000;\nL_mca_vu:\n");
    for (uint32_t c = 0U; c < chunks; c++) {
        fprintf(out, "    add.u32 %%r8, %%r2, %u; setp.lt.u32 %%p1, %%r8, %u;\n", (unsigned int)(c * 32U), (unsigned int)m->head_dim);
        fprintf(out, "    @%%p1 mul.wide.u32 %%rd8, %%r8, 4; @%%p1 add.u64 %%rd8, %%rd11, %%rd8; @%%p1 ld.f32 %%f9, [%%rd8];\n");
        fprintf(out, "    @%%p1 mul.f32 %%fa%u, %%fa%u, %%f7; @%%p1 fma.rn.f32 %%fa%u, %%f8, %%f9, %%fa%u;\n",
                (unsigned int)c, (unsigned int)c, (unsigned int)c, (unsigned int)c);
    }
    fprintf(out, "    add.u32 %%r9, %%r9, 1; bra L_mca_t;\n");
    fprintf(out, "L_mca_td:\n    div.rn.f32 %%f10, 0f3F800000, %%f2;\n");
    for (uint32_t c = 0U; c < chunks; c++) {
        fprintf(out, "    add.u32 %%r8, %%r2, %u; setp.lt.u32 %%p1, %%r8, %u;\n", (unsigned int)(c * 32U), (unsigned int)m->head_dim);
        fprintf(out, "    @%%p1 mul.f32 %%fa%u, %%fa%u, %%f10; @%%p1 mul.wide.u32 %%rd8, %%r8, 4; @%%p1 add.u64 %%rd8, %%rd7, %%rd8; @%%p1 st.f32 [%%rd8], %%fa%u;\n",
                (unsigned int)c, (unsigned int)c, (unsigned int)c);
    }
    fprintf(out, "L_mca_done:\n    ret;\n}\n\n");

    /* 2) FP16 KV FlashAttention-2 */
    fprintf(out, ".visible .entry zcc_aot_mc_attn_gqa_f16(\n"
                 "    .param .u64 _pq, .param .u64 _pkc, .param .u64 _pvc,\n"
                 "    .param .u64 _pout, .param .u64 _pargs, .param .u32 _player) {\n");
    fprintf(out, "    .reg .pred %%p<4>; .reg .b32 %%r<14>; .reg .b64 %%rd<14>;\n");
    fprintf(out, "    .reg .f32 %%f<14>; .reg .f32 %%fq<8>; .reg .f32 %%fa<8>;\n");
    fprintf(out, "    ld.param.u64 %%rd0, [_pq]; ld.param.u64 %%rd1, [_pkc]; ld.param.u64 %%rd2, [_pvc];\n");
    fprintf(out, "    ld.param.u64 %%rd3, [_pout]; ld.param.u64 %%rd4, [_pargs]; ld.param.u32 %%r0, [_player];\n");
    fprintf(out, "    mov.u32 %%r1, %%ctaid.x; setp.ge.u32 %%p0, %%r1, %u; @%%p0 bra L_mca16_done;\n", (unsigned int)m->n_heads);
    fprintf(out, "    mov.u32 %%r2, %%tid.x; and.b32 %%r2, %%r2, 31;\n");
    fprintf(out, "    ld.u32 %%r3, [%%rd4+4]; ld.u32 %%r4, [%%rd4+8];\n");
    fprintf(out, "    div.u32 %%r5, %%r1, %u; mul.lo.u32 %%r6, %%r5, %u;\n", (unsigned int)kv_group, (unsigned int)m->head_dim);
    fprintf(out, "    mul.lo.u32 %%r7, %%r1, %u; mul.wide.u32 %%rd5, %%r7, 4;\n", (unsigned int)m->head_dim);
    fprintf(out, "    add.u64 %%rd6, %%rd0, %%rd5; add.u64 %%rd7, %%rd3, %%rd5;\n");
    fprintf(out, "    mov.f32 %%f0, 0f%08X; rsqrt.approx.f32 %%f0, %%f0;\n", (unsigned int)hd_u);
    for (uint32_t c = 0U; c < chunks; c++) {
        fprintf(out, "    add.u32 %%r8, %%r2, %u; setp.lt.u32 %%p1, %%r8, %u;\n", (unsigned int)(c * 32U), (unsigned int)m->head_dim);
        fprintf(out, "    mov.f32 %%fq%u, 0f00000000; mov.f32 %%fa%u, 0f00000000;\n", (unsigned int)c, (unsigned int)c);
        fprintf(out, "    @%%p1 mul.wide.u32 %%rd8, %%r8, 4; @%%p1 add.u64 %%rd8, %%rd6, %%rd8; @%%p1 ld.f32 %%fq%u, [%%rd8];\n", (unsigned int)c);
    }
    fprintf(out, "    mul.wide.u32 %%rd12, %%r0, %%r4;\n");
    fprintf(out, "    mov.f32 %%f1, 0fF149F2CA; mov.f32 %%f2, 0f00000000; mov.u32 %%r9, 0;\n");
    fprintf(out, "L_mca16_t:\n    setp.gt.u32 %%p0, %%r9, %%r3; @%%p0 bra L_mca16_td;\n");
    fprintf(out, "    cvt.u64.u32 %%rd8, %%r9; add.u64 %%rd9, %%rd12, %%rd8; mul.lo.u64 %%rd9, %%rd9, %u;\n", (unsigned int)m->kv_dim);
    fprintf(out, "    cvt.u64.u32 %%rd8, %%r6; add.u64 %%rd9, %%rd9, %%rd8; shl.b64 %%rd9, %%rd9, 1;\n");
    fprintf(out, "    add.u64 %%rd10, %%rd1, %%rd9; add.u64 %%rd11, %%rd2, %%rd9;\n");
    fprintf(out, "    mov.f32 %%f3, 0f00000000;\n");
    for (uint32_t c = 0U; c < chunks; c++) {
        fprintf(out, "    add.u32 %%r8, %%r2, %u; setp.lt.u32 %%p1, %%r8, %u; mov.u32 %%r12, 0;\n", (unsigned int)(c * 32U), (unsigned int)m->head_dim);
        fprintf(out, "    @%%p1 mul.wide.u32 %%rd8, %%r8, 2; @%%p1 add.u64 %%rd8, %%rd10, %%rd8; @%%p1 ld.u16 %%r12, [%%rd8];\n");
        fprintf(out, "    { .reg .b16 %%hb; cvt.u16.u32 %%hb, %%r12; cvt.f32.f16 %%f4, %%hb; }\n");
        fprintf(out, "    @%%p1 fma.rn.f32 %%f3, %%fq%u, %%f4, %%f3;\n", (unsigned int)c);
    }
    fprintf(out, "    { .param .f32 _wr, _wa; st.param.f32 [_wa], %%f3; call.uni (_wr), zcc_aot_warp_sum, (_wa); ld.param.f32 %%f5, [_wr]; }\n");
    fprintf(out, "    mul.f32 %%f5, %%f5, %%f0;\n");
    fprintf(out, "    setp.eq.u32 %%p2, %%r9, 0; @%%p2 bra L_mca16_t0;\n");
    fprintf(out, "    max.f32 %%f6, %%f1, %%f5;\n");
    fprintf(out, "    sub.f32 %%f7, %%f1, %%f6; mul.f32 %%f7, %%f7, 0f3FB8AA3B; ex2.approx.f32 %%f7, %%f7;\n");
    fprintf(out, "    sub.f32 %%f8, %%f5, %%f6; mul.f32 %%f8, %%f8, 0f3FB8AA3B; ex2.approx.f32 %%f8, %%f8;\n");
    fprintf(out, "    fma.rn.f32 %%f2, %%f2, %%f7, %%f8; mov.f32 %%f1, %%f6; bra L_mca16_vu;\nL_mca16_t0:\n");
    fprintf(out, "    mov.f32 %%f1, %%f5; mov.f32 %%f2, 0f3F800000; mov.f32 %%f7, 0f00000000; mov.f32 %%f8, 0f3F800000;\nL_mca16_vu:\n");
    for (uint32_t c = 0U; c < chunks; c++) {
        fprintf(out, "    add.u32 %%r8, %%r2, %u; setp.lt.u32 %%p1, %%r8, %u; mov.u32 %%r12, 0;\n", (unsigned int)(c * 32U), (unsigned int)m->head_dim);
        fprintf(out, "    @%%p1 mul.wide.u32 %%rd8, %%r8, 2; @%%p1 add.u64 %%rd8, %%rd11, %%rd8; @%%p1 ld.u16 %%r12, [%%rd8];\n");
        fprintf(out, "    { .reg .b16 %%hb; cvt.u16.u32 %%hb, %%r12; cvt.f32.f16 %%f9, %%hb; }\n");
        fprintf(out, "    @%%p1 mul.f32 %%fa%u, %%fa%u, %%f7; @%%p1 fma.rn.f32 %%fa%u, %%f8, %%f9, %%fa%u;\n",
                (unsigned int)c, (unsigned int)c, (unsigned int)c, (unsigned int)c);
    }
    fprintf(out, "    add.u32 %%r9, %%r9, 1; bra L_mca16_t;\n");
    fprintf(out, "L_mca16_td:\n    div.rn.f32 %%f10, 0f3F800000, %%f2;\n");
    for (uint32_t c = 0U; c < chunks; c++) {
        fprintf(out, "    add.u32 %%r8, %%r2, %u; setp.lt.u32 %%p1, %%r8, %u;\n", (unsigned int)(c * 32U), (unsigned int)m->head_dim);
        fprintf(out, "    @%%p1 mul.f32 %%fa%u, %%fa%u, %%f10; @%%p1 mul.wide.u32 %%rd8, %%r8, 4; @%%p1 add.u64 %%rd8, %%rd7, %%rd8; @%%p1 st.f32 [%%rd8], %%fa%u;\n",
                (unsigned int)c, (unsigned int)c, (unsigned int)c);
    }
    fprintf(out, "L_mca16_done:\n    ret;\n}\n\n");

    /* 3) Q8_0 KV FlashAttention-2 (3.76x KV VRAM reduction, when head_dim % 32 == 0) */
    if ((m->head_dim & 31U) == 0U && (m->kv_dim & 31U) == 0U) {
        uint32_t q8_row_bytes = (m->kv_dim >> 5) * 34U;
        uint32_t head_q8_bytes = chunks * 34U;
        fprintf(out, ".visible .entry zcc_aot_mc_attn_gqa_q8_0(\n"
                     "    .param .u64 _pq, .param .u64 _pkc, .param .u64 _pvc,\n"
                     "    .param .u64 _pout, .param .u64 _pargs, .param .u32 _player) {\n");
        fprintf(out, "    .reg .pred %%p<4>; .reg .b32 %%r<15>; .reg .b64 %%rd<16>;\n");
        fprintf(out, "    .reg .f32 %%f<14>; .reg .f32 %%fq<8>; .reg .f32 %%fa<8>;\n");
        fprintf(out, "    ld.param.u64 %%rd0, [_pq]; ld.param.u64 %%rd1, [_pkc]; ld.param.u64 %%rd2, [_pvc];\n");
        fprintf(out, "    ld.param.u64 %%rd3, [_pout]; ld.param.u64 %%rd4, [_pargs]; ld.param.u32 %%r0, [_player];\n");
        fprintf(out, "    mov.u32 %%r1, %%ctaid.x; setp.ge.u32 %%p0, %%r1, %u; @%%p0 bra L_mcaq8_done;\n", (unsigned int)m->n_heads);
        fprintf(out, "    mov.u32 %%r2, %%tid.x; and.b32 %%r2, %%r2, 31;\n");
        fprintf(out, "    add.u32 %%r14, %%r2, 2; cvt.u64.u32 %%rd15, %%r14;\n");
        fprintf(out, "    ld.u32 %%r3, [%%rd4+4]; ld.u32 %%r4, [%%rd4+8];\n");
        fprintf(out, "    div.u32 %%r5, %%r1, %u; mul.lo.u32 %%r6, %%r5, %u;\n", (unsigned int)kv_group, (unsigned int)head_q8_bytes);
        fprintf(out, "    mul.lo.u32 %%r7, %%r1, %u; mul.wide.u32 %%rd5, %%r7, 4;\n", (unsigned int)m->head_dim);
        fprintf(out, "    add.u64 %%rd6, %%rd0, %%rd5; add.u64 %%rd7, %%rd3, %%rd5;\n");
        fprintf(out, "    mov.f32 %%f0, 0f%08X; rsqrt.approx.f32 %%f0, %%f0;\n", (unsigned int)hd_u);
        for (uint32_t c = 0U; c < chunks; c++) {
            fprintf(out, "    add.u32 %%r8, %%r2, %u; mul.wide.u32 %%rd8, %%r8, 4; add.u64 %%rd8, %%rd6, %%rd8;\n", (unsigned int)(c * 32U));
            fprintf(out, "    ld.f32 %%fq%u, [%%rd8]; mov.f32 %%fa%u, 0f00000000;\n", (unsigned int)c, (unsigned int)c);
        }
        fprintf(out, "    mul.wide.u32 %%rd12, %%r0, %%r4;\n");
        fprintf(out, "    mov.f32 %%f1, 0fF149F2CA; mov.f32 %%f2, 0f00000000; mov.u32 %%r9, 0;\n");
        fprintf(out, "L_mcaq8_t:\n    setp.gt.u32 %%p0, %%r9, %%r3; @%%p0 bra L_mcaq8_td;\n");
        fprintf(out, "    cvt.u64.u32 %%rd8, %%r9; add.u64 %%rd9, %%rd12, %%rd8; mul.lo.u64 %%rd9, %%rd9, %u;\n", (unsigned int)q8_row_bytes);
        fprintf(out, "    cvt.u64.u32 %%rd8, %%r6; add.u64 %%rd9, %%rd9, %%rd8;\n");
        fprintf(out, "    add.u64 %%rd10, %%rd1, %%rd9; add.u64 %%rd11, %%rd2, %%rd9;\n");
        fprintf(out, "    mov.f32 %%f3, 0f00000000;\n");
        for (uint32_t c = 0U; c < chunks; c++) {
            uint32_t boff = c * 34U;
            fprintf(out, "    ld.u8 %%r12, [%%rd10+%u]; ld.u8 %%r13, [%%rd10+%u]; shl.b32 %%r13, %%r13, 8; or.b32 %%r12, %%r12, %%r13;\n",
                    (unsigned int)boff, (unsigned int)(boff + 1U));
            fprintf(out, "    { .reg .b16 %%hb; cvt.u16.u32 %%hb, %%r12; cvt.f32.f16 %%f11, %%hb; }\n");
            fprintf(out, "    add.u64 %%rd8, %%rd10, %%rd15; ld.u8 %%r12, [%%rd8+%u]; setp.ge.u32 %%p1, %%r12, 128; @%%p1 sub.s32 %%r12, %%r12, 256;\n",
                    (unsigned int)boff);
            fprintf(out, "    cvt.rn.f32.s32 %%f4, %%r12; mul.f32 %%f4, %%f4, %%f11; fma.rn.f32 %%f3, %%fq%u, %%f4, %%f3;\n", (unsigned int)c);
        }
        fprintf(out, "    { .param .f32 _wr, _wa; st.param.f32 [_wa], %%f3; call.uni (_wr), zcc_aot_warp_sum, (_wa); ld.param.f32 %%f5, [_wr]; }\n");
        fprintf(out, "    mul.f32 %%f5, %%f5, %%f0;\n");
        fprintf(out, "    setp.eq.u32 %%p2, %%r9, 0; @%%p2 bra L_mcaq8_t0;\n");
        fprintf(out, "    max.f32 %%f6, %%f1, %%f5;\n");
        fprintf(out, "    sub.f32 %%f7, %%f1, %%f6; mul.f32 %%f7, %%f7, 0f3FB8AA3B; ex2.approx.f32 %%f7, %%f7;\n");
        fprintf(out, "    sub.f32 %%f8, %%f5, %%f6; mul.f32 %%f8, %%f8, 0f3FB8AA3B; ex2.approx.f32 %%f8, %%f8;\n");
        fprintf(out, "    fma.rn.f32 %%f2, %%f2, %%f7, %%f8; mov.f32 %%f1, %%f6; bra L_mcaq8_vu;\nL_mcaq8_t0:\n");
        fprintf(out, "    mov.f32 %%f1, %%f5; mov.f32 %%f2, 0f3F800000; mov.f32 %%f7, 0f00000000; mov.f32 %%f8, 0f3F800000;\nL_mcaq8_vu:\n");
        for (uint32_t c = 0U; c < chunks; c++) {
            uint32_t boff = c * 34U;
            fprintf(out, "    ld.u8 %%r12, [%%rd11+%u]; ld.u8 %%r13, [%%rd11+%u]; shl.b32 %%r13, %%r13, 8; or.b32 %%r12, %%r12, %%r13;\n",
                    (unsigned int)boff, (unsigned int)(boff + 1U));
            fprintf(out, "    { .reg .b16 %%hb; cvt.u16.u32 %%hb, %%r12; cvt.f32.f16 %%f12, %%hb; }\n");
            fprintf(out, "    add.u64 %%rd8, %%rd11, %%rd15; ld.u8 %%r12, [%%rd8+%u]; setp.ge.u32 %%p1, %%r12, 128; @%%p1 sub.s32 %%r12, %%r12, 256;\n",
                    (unsigned int)boff);
            fprintf(out, "    cvt.rn.f32.s32 %%f9, %%r12; mul.f32 %%f9, %%f9, %%f12;\n");
            fprintf(out, "    mul.f32 %%fa%u, %%fa%u, %%f7; fma.rn.f32 %%fa%u, %%f8, %%f9, %%fa%u;\n",
                    (unsigned int)c, (unsigned int)c, (unsigned int)c, (unsigned int)c);
        }
        fprintf(out, "    add.u32 %%r9, %%r9, 1; bra L_mcaq8_t;\n");
        fprintf(out, "L_mcaq8_td:\n    div.rn.f32 %%f10, 0f3F800000, %%f2;\n");
        for (uint32_t c = 0U; c < chunks; c++) {
            fprintf(out, "    add.u32 %%r8, %%r2, %u; mul.f32 %%fa%u, %%fa%u, %%f10; mul.wide.u32 %%rd8, %%r8, 4; add.u64 %%rd8, %%rd7, %%rd8; st.f32 [%%rd8], %%fa%u;\n",
                    (unsigned int)(c * 32U), (unsigned int)c, (unsigned int)c, (unsigned int)c);
        }
        fprintf(out, "L_mcaq8_done:\n    ret;\n}\n\n");
    }
}

static void gguf_ptx_emit_mc_swiglu_fn(FILE *out, const GgufPtxModel *m) {
    fprintf(out, ".visible .entry zcc_aot_mc_swiglu(.param .u64 _phb, .param .u64 _phb2) {\n");
    fprintf(out, "    .reg .pred %%p0; .reg .b32 %%r<3>; .reg .b64 %%rd<5>; .reg .f32 %%f<4>;\n");
    fprintf(out, "    ld.param.u64 %%rd0, [_phb]; ld.param.u64 %%rd1, [_phb2];\n");
    fprintf(out, "    mov.u32 %%r1, %%ctaid.x; mov.u32 %%r2, %%tid.x; mad.lo.u32 %%r0, %%r1, 256, %%r2;\n");
    fprintf(out, "    setp.ge.u32 %%p0, %%r0, %u; @%%p0 bra L_mcsw_d;\n", (unsigned int)m->hidden_dim);
    fprintf(out, "    mul.wide.u32 %%rd2, %%r0, 4; add.u64 %%rd3, %%rd0, %%rd2; ld.f32 %%f0, [%%rd3];\n");
    fprintf(out, "    add.u64 %%rd4, %%rd1, %%rd2; ld.f32 %%f1, [%%rd4];\n");
    fprintf(out, "    neg.f32 %%f2, %%f0; mul.f32 %%f2, %%f2, 0f3FB8AA3B; ex2.approx.f32 %%f2, %%f2;\n");
    fprintf(out, "    add.f32 %%f2, %%f2, 0f3F800000; div.rn.f32 %%f3, %%f0, %%f2; mul.f32 %%f3, %%f3, %%f1;\n");
    fprintf(out, "    st.f32 [%%rd3], %%f3;\nL_mcsw_d:\n    ret;\n}\n\n");
}

static void gguf_ptx_emit_mc_dequant_f16_fn(FILE *out, uint32_t fmt) {
    const char *suf = (fmt == GGUF_PTX_BF16) ? "bf16" :
                      (fmt == GGUF_PTX_Q8_0) ? "q8_0" :
                      (fmt == GGUF_PTX_Q4_0) ? "q4_0" :
                      (fmt == GGUF_PTX_Q4_K) ? "q4_k" :
                      (fmt == GGUF_PTX_Q6_K) ? "q6_k" : "f32";
    fprintf(out, ".visible .entry zcc_aot_mc_dequant_%s_to_f16(\n"
                 "    .param .u64 _pw, .param .u64 _pwoff, .param .u64 _pdst,\n"
                 "    .param .u32 _pN, .param .u32 _pK) {\n", suf);
    fprintf(out, "    .reg .pred %%p<4>; .reg .b32 %%r<26>; .reg .b64 %%rd<14>; .reg .f32 %%f<6>;\n");
    fprintf(out, "    ld.param.u64 %%rd0, [_pw]; ld.param.u64 %%rd1, [_pwoff]; add.u64 %%rd0, %%rd0, %%rd1;\n");
    fprintf(out, "    ld.param.u64 %%rd2, [_pdst]; ld.param.u32 %%r24, [_pN]; ld.param.u32 %%r25, [_pK];\n");
    fprintf(out, "    mov.u32 %%r0, %%ctaid.y; mov.u32 %%r1, %%ctaid.x; mov.u32 %%r2, %%tid.x; mad.lo.u32 %%r2, %%r1, 256, %%r2;\n");
    fprintf(out, "    setp.ge.u32 %%p0, %%r0, %%r24; @%%p0 bra L_mcdq_%s_d;\n", suf);
    fprintf(out, "    setp.ge.u32 %%p0, %%r2, %%r25; @%%p0 bra L_mcdq_%s_d;\n", suf);
    if (fmt == GGUF_PTX_F32) {
        fprintf(out, "    mul.wide.u32 %%rd4, %%r0, %%r25; shl.b64 %%rd4, %%rd4, 2; add.u64 %%rd0, %%rd0, %%rd4;\n");
        fprintf(out, "    mul.wide.u32 %%rd5, %%r2, 4; add.u64 %%rd6, %%rd0, %%rd5; ld.f32 %%f0, [%%rd6];\n");
    } else if (fmt == GGUF_PTX_BF16) {
        fprintf(out, "    mul.wide.u32 %%rd4, %%r0, %%r25; shl.b64 %%rd4, %%rd4, 1; add.u64 %%rd0, %%rd0, %%rd4;\n");
        fprintf(out, "    mul.wide.u32 %%rd5, %%r2, 2; add.u64 %%rd6, %%rd0, %%rd5; ld.u16 %%r3, [%%rd6];\n");
        fprintf(out, "    shl.b32 %%r3, %%r3, 16; mov.b32 %%f0, %%r3;\n");
    } else if (fmt == GGUF_PTX_Q8_0) {
        fprintf(out, "    shr.u32 %%r1, %%r25, 5; mul.lo.u32 %%r1, %%r1, 34;\n");
        fprintf(out, "    mul.wide.u32 %%rd4, %%r0, %%r1; add.u64 %%rd0, %%rd0, %%rd4;\n");
        fprintf(out, "    shr.u32 %%r3, %%r2, 5; and.b32 %%r4, %%r2, 31;\n");
        fprintf(out, "    mul.wide.u32 %%rd5, %%r3, 34; add.u64 %%rd6, %%rd0, %%rd5;\n");
        fprintf(out, "    ld.u8 %%r5, [%%rd6]; ld.u8 %%r6, [%%rd6+1]; shl.b32 %%r6, %%r6, 8; or.b32 %%r5, %%r5, %%r6;\n");
        fprintf(out, "    { .reg .b16 %%hb; cvt.u16.u32 %%hb, %%r5; cvt.f32.f16 %%f1, %%hb; }\n");
        fprintf(out, "    add.u32 %%r4, %%r4, 2; cvt.u64.u32 %%rd7, %%r4; add.u64 %%rd8, %%rd6, %%rd7;\n");
        fprintf(out, "    ld.u8 %%r7, [%%rd8]; setp.ge.u32 %%p1, %%r7, 128; @%%p1 sub.s32 %%r7, %%r7, 256;\n");
        fprintf(out, "    cvt.rn.f32.s32 %%f2, %%r7; mul.f32 %%f0, %%f1, %%f2;\n");
    } else if (fmt == GGUF_PTX_Q4_0) {
        fprintf(out, "    shr.u32 %%r1, %%r25, 5; mul.lo.u32 %%r1, %%r1, 18;\n");
        fprintf(out, "    mul.wide.u32 %%rd4, %%r0, %%r1; add.u64 %%rd0, %%rd0, %%rd4;\n");
        fprintf(out, "    shr.u32 %%r3, %%r2, 5; and.b32 %%r4, %%r2, 31; and.b32 %%r5, %%r4, 15;\n");
        fprintf(out, "    mul.wide.u32 %%rd5, %%r3, 18; add.u64 %%rd6, %%rd0, %%rd5;\n");
        fprintf(out, "    ld.u8 %%r6, [%%rd6]; ld.u8 %%r7, [%%rd6+1]; shl.b32 %%r7, %%r7, 8; or.b32 %%r6, %%r6, %%r7;\n");
        fprintf(out, "    { .reg .b16 %%hb; cvt.u16.u32 %%hb, %%r6; cvt.f32.f16 %%f1, %%hb; }\n");
        fprintf(out, "    add.u32 %%r5, %%r5, 2; cvt.u64.u32 %%rd7, %%r5; add.u64 %%rd8, %%rd6, %%rd7; ld.u8 %%r8, [%%rd8];\n");
        fprintf(out, "    and.b32 %%r9, %%r8, 15; shr.u32 %%r10, %%r8, 4;\n");
        fprintf(out, "    setp.lt.u32 %%p1, %%r4, 16; selp.u32 %%r11, %%r9, %%r10, %%p1; sub.s32 %%r11, %%r11, 8;\n");
        fprintf(out, "    cvt.rn.f32.s32 %%f2, %%r11; mul.f32 %%f0, %%f1, %%f2;\n");
    } else if (fmt == GGUF_PTX_Q4_K) {
        fprintf(out, "    shr.u32 %%r1, %%r25, 8; mul.lo.u32 %%r1, %%r1, 144;\n");
        fprintf(out, "    mul.wide.u32 %%rd4, %%r0, %%r1; add.u64 %%rd0, %%rd0, %%rd4;\n");
        fprintf(out, "    shr.u32 %%r3, %%r2, 8; and.b32 %%r4, %%r2, 255;\n");
        fprintf(out, "    mul.wide.u32 %%rd5, %%r3, 144; add.u64 %%rd6, %%rd0, %%rd5;\n");
        fprintf(out, "    ld.u8 %%r5, [%%rd6]; ld.u8 %%r6, [%%rd6+1]; shl.b32 %%r6, %%r6, 8; or.b32 %%r5, %%r5, %%r6;\n");
        fprintf(out, "    { .reg .b16 %%hb; cvt.u16.u32 %%hb, %%r5; cvt.f32.f16 %%f1, %%hb; }\n");
        fprintf(out, "    ld.u8 %%r5, [%%rd6+2]; ld.u8 %%r6, [%%rd6+3]; shl.b32 %%r6, %%r6, 8; or.b32 %%r5, %%r5, %%r6;\n");
        fprintf(out, "    { .reg .b16 %%hb; cvt.u16.u32 %%hb, %%r5; cvt.f32.f16 %%f2, %%hb; }\n");
        fprintf(out, "    shr.u32 %%r7, %%r4, 6; shr.u32 %%r8, %%r4, 5; and.b32 %%r8, %%r8, 1; and.b32 %%r9, %%r4, 31;\n");
        fprintf(out, "    shl.b32 %%r10, %%r7, 1; or.b32 %%r10, %%r10, %%r8;\n");
        fprintf(out, "    add.u64 %%rd7, %%rd6, 4; setp.lt.u32 %%p1, %%r10, 4; @%%p1 bra L_dq4k_lo;\n");
        fprintf(out, "    add.u32 %%r11, %%r10, 4; cvt.u64.u32 %%rd8, %%r11; add.u64 %%rd9, %%rd7, %%rd8; ld.u8 %%r12, [%%rd9];\n");
        fprintf(out, "    sub.u32 %%r13, %%r10, 4; cvt.u64.u32 %%rd8, %%r13; add.u64 %%rd9, %%rd7, %%rd8; ld.u8 %%r14, [%%rd9];\n");
        fprintf(out, "    cvt.u64.u32 %%rd8, %%r10; add.u64 %%rd9, %%rd7, %%rd8; ld.u8 %%r15, [%%rd9];\n");
        fprintf(out, "    and.b32 %%r16, %%r12, 15; shr.u32 %%r14, %%r14, 6; shl.b32 %%r14, %%r14, 4; or.b32 %%r16, %%r16, %%r14;\n");
        fprintf(out, "    shr.u32 %%r17, %%r12, 4; shr.u32 %%r15, %%r15, 6; shl.b32 %%r15, %%r15, 4; or.b32 %%r17, %%r17, %%r15;\n");
        fprintf(out, "    bra L_dq4k_sc;\nL_dq4k_lo:\n");
        fprintf(out, "    cvt.u64.u32 %%rd8, %%r10; add.u64 %%rd9, %%rd7, %%rd8; ld.u8 %%r16, [%%rd9]; and.b32 %%r16, %%r16, 63;\n");
        fprintf(out, "    add.u32 %%r11, %%r10, 4; cvt.u64.u32 %%rd8, %%r11; add.u64 %%rd9, %%rd7, %%rd8; ld.u8 %%r17, [%%rd9]; and.b32 %%r17, %%r17, 63;\n");
        fprintf(out, "L_dq4k_sc:\n");
        fprintf(out, "    shl.b32 %%r18, %%r7, 5; add.u32 %%r18, %%r18, %%r9; add.u32 %%r18, %%r18, 16;\n");
        fprintf(out, "    cvt.u64.u32 %%rd8, %%r18; add.u64 %%rd9, %%rd6, %%rd8; ld.u8 %%r19, [%%rd9];\n");
        fprintf(out, "    and.b32 %%r20, %%r19, 15; shr.u32 %%r21, %%r19, 4; setp.eq.u32 %%p2, %%r8, 0; selp.u32 %%r22, %%r20, %%r21, %%p2;\n");
        fprintf(out, "    cvt.rn.f32.u32 %%f3, %%r16; mul.f32 %%f3, %%f1, %%f3; cvt.rn.f32.u32 %%f4, %%r22; mul.f32 %%f3, %%f3, %%f4;\n");
        fprintf(out, "    cvt.rn.f32.u32 %%f5, %%r17; mul.f32 %%f5, %%f2, %%f5; sub.f32 %%f0, %%f3, %%f5;\n");
    } else if (fmt == GGUF_PTX_Q6_K) {
        fprintf(out, "    shr.u32 %%r1, %%r25, 8; mul.lo.u32 %%r1, %%r1, 210;\n");
        fprintf(out, "    mul.wide.u32 %%rd4, %%r0, %%r1; add.u64 %%rd0, %%rd0, %%rd4;\n");
        fprintf(out, "    shr.u32 %%r3, %%r2, 8; and.b32 %%r4, %%r2, 255;\n");
        fprintf(out, "    mul.wide.u32 %%rd5, %%r3, 210; add.u64 %%rd6, %%rd0, %%rd5;\n");
        fprintf(out, "    ld.u8 %%r5, [%%rd6+208]; ld.u8 %%r6, [%%rd6+209]; shl.b32 %%r6, %%r6, 8; or.b32 %%r5, %%r5, %%r6;\n");
        fprintf(out, "    { .reg .b16 %%hb; cvt.u16.u32 %%hb, %%r5; cvt.f32.f16 %%f1, %%hb; }\n");
        fprintf(out, "    shr.u32 %%r7, %%r4, 7; and.b32 %%r8, %%r4, 127; shr.u32 %%r9, %%r8, 5; and.b32 %%r10, %%r8, 31; shr.u32 %%r11, %%r10, 4;\n");
        fprintf(out, "    shl.b32 %%r12, %%r7, 6; and.b32 %%r13, %%r9, 1; shl.b32 %%r13, %%r13, 5; add.u32 %%r12, %%r12, %%r13; add.u32 %%r12, %%r12, %%r10;\n");
        fprintf(out, "    cvt.u64.u32 %%rd7, %%r12; add.u64 %%rd8, %%rd6, %%rd7; ld.u8 %%r14, [%%rd8];\n");
        fprintf(out, "    and.b32 %%r15, %%r14, 15; shr.u32 %%r16, %%r14, 4; setp.ge.u32 %%p1, %%r9, 2; selp.u32 %%r17, %%r16, %%r15, %%p1;\n");
        fprintf(out, "    shl.b32 %%r12, %%r7, 5; add.u32 %%r12, %%r12, %%r10; add.u32 %%r12, %%r12, 128;\n");
        fprintf(out, "    cvt.u64.u32 %%rd7, %%r12; add.u64 %%rd8, %%rd6, %%rd7; ld.u8 %%r18, [%%rd8];\n");
        fprintf(out, "    shl.b32 %%r19, %%r9, 1; shr.u32 %%r18, %%r18, %%r19; and.b32 %%r18, %%r18, 3; shl.b32 %%r18, %%r18, 4;\n");
        fprintf(out, "    or.b32 %%r17, %%r17, %%r18; sub.s32 %%r17, %%r17, 32;\n");
        fprintf(out, "    shl.b32 %%r12, %%r7, 3; add.u32 %%r12, %%r12, %%r11; add.u32 %%r12, %%r12, %%r19; add.u32 %%r12, %%r12, 192;\n");
        fprintf(out, "    cvt.u64.u32 %%rd7, %%r12; add.u64 %%rd8, %%rd6, %%rd7; ld.u8 %%r20, [%%rd8];\n");
        fprintf(out, "    setp.ge.u32 %%p2, %%r20, 128; @%%p2 sub.s32 %%r20, %%r20, 256;\n");
        fprintf(out, "    cvt.rn.f32.s32 %%f2, %%r20; cvt.rn.f32.s32 %%f3, %%r17; mul.f32 %%f0, %%f1, %%f2; mul.f32 %%f0, %%f0, %%f3;\n");
    }
    fprintf(out, "    { .reg .b16 %%hb; cvt.rn.f16.f32 %%hb, %%f0; cvt.u32.u16 %%r23, %%hb; }\n");
    fprintf(out, "    mul.wide.u32 %%rd10, %%r0, %%r25; cvt.u64.u32 %%rd11, %%r2; add.u64 %%rd10, %%rd10, %%rd11; shl.b64 %%rd10, %%rd10, 1;\n");
    fprintf(out, "    add.u64 %%rd12, %%rd2, %%rd10; st.u16 [%%rd12], %%r23;\n");
    fprintf(out, "L_mcdq_%s_d:\n    ret;\n}\n\n", suf);
}

static void gguf_ptx_emit_mc_prefill_fns(FILE *out, const GgufPtxModel *m) {
    uint32_t inv_dim_u = gguf_ptx_f32_bits(1.0f / (float)m->dim);
    uint32_t eps_u = gguf_ptx_f32_bits(m->rms_eps);
    uint32_t half_hd = m->head_dim >> 1;
    uint32_t q_pairs = m->n_heads * half_hd;
    uint32_t total_pairs = (m->n_heads + m->n_kv_heads) * half_hd;
    uint32_t theta_u = gguf_ptx_f32_bits(m->rope_theta);
    uint32_t hd_u = gguf_ptx_f32_bits((float)m->head_dim);
    uint32_t kv_group = m->n_heads / m->n_kv_heads;
    uint32_t chunks = (m->head_dim + 31U) / 32U;
    int is_neox = (strcmp(m->arch, "qwen2") == 0);
    uint32_t f32_pair_off = is_neox ? (half_hd * 4U) : 4U;
    uint32_t f16_pair_off = is_neox ? (half_hd * 2U) : 2U;
    const char *elem_reg = is_neox ? "%r7" : "%r10";

    /* A) Dequant-to-F16 only for used non-F16 2D weight formats */
    if (m->need_f32)  gguf_ptx_emit_mc_dequant_f16_fn(out, GGUF_PTX_F32);
    if (m->need_bf16) gguf_ptx_emit_mc_dequant_f16_fn(out, GGUF_PTX_BF16);
    if (m->need_q8_0) gguf_ptx_emit_mc_dequant_f16_fn(out, GGUF_PTX_Q8_0);
    if (m->need_q4_0) gguf_ptx_emit_mc_dequant_f16_fn(out, GGUF_PTX_Q4_0);
    if (m->need_q4_k) gguf_ptx_emit_mc_dequant_f16_fn(out, GGUF_PTX_Q4_K);
    if (m->need_q6_k) gguf_ptx_emit_mc_dequant_f16_fn(out, GGUF_PTX_Q6_K);

    /* B) Batched SM 12.0 Tensor Core GEMM (mma.sync.aligned.m16n8k16.row.col, 4 warps = 16x32 CTA tile) */
    fprintf(out, ".visible .entry zcc_aot_mc_prefill_tc_gemm(\n"
                 "    .param .u64 _pxf16, .param .u64 _pwf16, .param .u64 _pwoff, .param .u64 _py,\n"
                 "    .param .u32 _pM, .param .u32 _pN, .param .u32 _pK, .param .u32 _padd) {\n");
    fprintf(out, "    .reg .pred %%p<8>; .reg .b32 %%r<32>; .reg .b64 %%rd<10>; .reg .f32 %%f<8>;\n");
    fprintf(out, "    ld.param.u64 %%rd0, [_pxf16]; ld.param.u64 %%rd1, [_pwf16]; ld.param.u64 %%rd9, [_pwoff]; add.u64 %%rd1, %%rd1, %%rd9;\n");
    fprintf(out, "    ld.param.u64 %%rd2, [_py]; ld.param.u32 %%r0, [_pM]; ld.param.u32 %%r1, [_pN];\n");
    fprintf(out, "    ld.param.u32 %%r2, [_pK]; ld.param.u32 %%r3, [_padd];\n");
    fprintf(out, "    mov.u32 %%r4, %%tid.x; shr.u32 %%r5, %%r4, 5; and.b32 %%r6, %%r4, 31;\n");
    fprintf(out, "    mov.u32 %%r7, %%ctaid.y; shl.b32 %%r7, %%r7, 4;\n");
    fprintf(out, "    mov.u32 %%r8, %%ctaid.x; shl.b32 %%r8, %%r8, 5; shl.b32 %%r9, %%r5, 3; add.u32 %%r8, %%r8, %%r9;\n");
    fprintf(out, "    setp.ge.u32 %%p0, %%r7, %%r0; setp.ge.u32 %%p1, %%r8, %%r1; or.pred %%p0, %%p0, %%p1; @%%p0 bra L_mcptc_d;\n");
    fprintf(out, "    shr.u32 %%r9, %%r6, 2; and.b32 %%r10, %%r6, 3; shl.b32 %%r10, %%r10, 1;\n");
    fprintf(out, "    sub.u32 %%r11, %%r0, 1; sub.u32 %%r12, %%r1, 1;\n");
    fprintf(out, "    add.u32 %%r13, %%r7, %%r9; min.u32 %%r13, %%r13, %%r11;\n");
    fprintf(out, "    add.u32 %%r14, %%r7, %%r9; add.u32 %%r14, %%r14, 8; min.u32 %%r14, %%r14, %%r11;\n");
    fprintf(out, "    add.u32 %%r15, %%r8, %%r9; min.u32 %%r15, %%r15, %%r12;\n");
    fprintf(out, "    mov.u32 %%r16, 0;\n");
    fprintf(out, "    mov.f32 %%f0, 0f00000000; mov.f32 %%f1, 0f00000000; mov.f32 %%f2, 0f00000000; mov.f32 %%f3, 0f00000000;\n");
    fprintf(out, "L_mcptc_k:\n    add.u32 %%r17, %%r16, 16; setp.gt.u32 %%p2, %%r17, %%r2; @%%p2 bra L_mcptc_kd;\n");
    fprintf(out, "    mad.lo.u32 %%r18, %%r13, %%r2, %%r16; add.u32 %%r18, %%r18, %%r10;\n");
    fprintf(out, "    mad.lo.u32 %%r19, %%r14, %%r2, %%r16; add.u32 %%r19, %%r19, %%r10;\n");
    fprintf(out, "    add.u32 %%r20, %%r18, 8; add.u32 %%r21, %%r19, 8;\n");
    fprintf(out, "    mul.wide.u32 %%rd3, %%r18, 2; add.u64 %%rd4, %%rd0, %%rd3; ld.u32 %%r22, [%%rd4];\n");
    fprintf(out, "    mul.wide.u32 %%rd3, %%r19, 2; add.u64 %%rd4, %%rd0, %%rd3; ld.u32 %%r23, [%%rd4];\n");
    fprintf(out, "    mul.wide.u32 %%rd3, %%r20, 2; add.u64 %%rd4, %%rd0, %%rd3; ld.u32 %%r24, [%%rd4];\n");
    fprintf(out, "    mul.wide.u32 %%rd3, %%r21, 2; add.u64 %%rd4, %%rd0, %%rd3; ld.u32 %%r25, [%%rd4];\n");
    fprintf(out, "    mad.lo.u32 %%r26, %%r15, %%r2, %%r16; add.u32 %%r26, %%r26, %%r10; add.u32 %%r27, %%r26, 8;\n");
    fprintf(out, "    mul.wide.u32 %%rd3, %%r26, 2; add.u64 %%rd4, %%rd1, %%rd3; ld.u32 %%r28, [%%rd4];\n");
    fprintf(out, "    mul.wide.u32 %%rd3, %%r27, 2; add.u64 %%rd4, %%rd1, %%rd3; ld.u32 %%r29, [%%rd4];\n");
    fprintf(out, "    mma.sync.aligned.m16n8k16.row.col.f32.f16.f16.f32 {%%f0, %%f1, %%f2, %%f3}, {%%r22, %%r23, %%r24, %%r25}, {%%r28, %%r29}, {%%f0, %%f1, %%f2, %%f3};\n");
    fprintf(out, "    mov.u32 %%r16, %%r17; bra L_mcptc_k;\n");
    fprintf(out, "L_mcptc_kd:\n");
    fprintf(out, "    add.u32 %%r18, %%r7, %%r9; add.u32 %%r19, %%r18, 8;\n");
    fprintf(out, "    add.u32 %%r20, %%r8, %%r10; add.u32 %%r21, %%r20, 1;\n");
    fprintf(out, "    setp.ne.u32 %%p7, %%r3, 0;\n");
    for (int qi = 0; qi < 4; qi++) {
        int rr = (qi < 2) ? 18 : 19;
        int cr = ((qi & 1) == 0) ? 20 : 21;
        fprintf(out, "    setp.lt.u32 %%p3, %%r%d, %%r0; setp.lt.u32 %%p4, %%r%d, %%r1; and.pred %%p5, %%p3, %%p4;\n", rr, cr);
        fprintf(out, "    @%%p5 mad.lo.u32 %%r30, %%r%d, %%r1, %%r%d; @%%p5 mul.wide.u32 %%rd5, %%r30, 4; @%%p5 add.u64 %%rd6, %%rd2, %%rd5;\n", rr, cr);
        fprintf(out, "    and.pred %%p6, %%p5, %%p7; @%%p6 ld.f32 %%f4, [%%rd6]; @%%p6 add.f32 %%f%d, %%f4, %%f%d; @%%p5 st.f32 [%%rd6], %%f%d;\n", qi, qi, qi);
    }
    fprintf(out, "L_mcptc_d:\n    ret;\n}\n\n");

    /* C) Batched RMSNorm (1 CTA of 128 threads per token) */
    fprintf(out, ".visible .entry zcc_aot_mc_rmsnorm_batch(.param .u64 _pdst, .param .u64 _psrc, .param .u64 _pw, .param .u64 _pwoff) {\n");
    fprintf(out, "    .shared .align 4 .b8 s_red[16];\n");
    fprintf(out, "    .reg .pred %%p0, %%p1; .reg .b32 %%r<5>; .reg .b64 %%rd<11>; .reg .f32 %%f<6>;\n");
    fprintf(out, "    ld.param.u64 %%rd0, [_pdst]; ld.param.u64 %%rd1, [_psrc];\n");
    fprintf(out, "    ld.param.u64 %%rd2, [_pw]; ld.param.u64 %%rd9, [_pwoff]; add.u64 %%rd2, %%rd2, %%rd9;\n");
    fprintf(out, "    mov.u32 %%r4, %%ctaid.x; mul.wide.u32 %%rd10, %%r4, %u; shl.b64 %%rd10, %%rd10, 2;\n", (unsigned int)m->dim);
    fprintf(out, "    add.u64 %%rd0, %%rd0, %%rd10; add.u64 %%rd1, %%rd1, %%rd10;\n");
    fprintf(out, "    cvta.shared.u64 %%rd3, s_red;\n");
    fprintf(out, "    mov.u32 %%r0, %%tid.x; shr.u32 %%r1, %%r0, 5; and.b32 %%r2, %%r0, 31;\n");
    fprintf(out, "    mov.f32 %%f0, 0f00000000; mov.u32 %%r3, %%r0;\n");
    fprintf(out, "L_mcrnb1:\n    setp.ge.u32 %%p0, %%r3, %u; @%%p0 bra L_mcrnb1_d;\n", (unsigned int)m->dim);
    fprintf(out, "    mul.wide.u32 %%rd4, %%r3, 4; add.u64 %%rd5, %%rd1, %%rd4; ld.f32 %%f1, [%%rd5];\n");
    fprintf(out, "    fma.rn.f32 %%f0, %%f1, %%f1, %%f0; add.u32 %%r3, %%r3, 128; bra L_mcrnb1;\n");
    fprintf(out, "L_mcrnb1_d:\n");
    fprintf(out, "    { .param .f32 _wr, _wa; st.param.f32 [_wa], %%f0; call.uni (_wr), zcc_aot_warp_sum, (_wa); ld.param.f32 %%f0, [_wr]; }\n");
    fprintf(out, "    setp.eq.u32 %%p1, %%r2, 0;\n");
    fprintf(out, "    @%%p1 mul.wide.u32 %%rd4, %%r1, 4; @%%p1 add.u64 %%rd5, %%rd3, %%rd4; @%%p1 st.f32 [%%rd5], %%f0;\n");
    fprintf(out, "    bar.sync 0;\n");
    fprintf(out, "    ld.f32 %%f1, [%%rd3]; ld.f32 %%f2, [%%rd3+4]; ld.f32 %%f3, [%%rd3+8]; ld.f32 %%f4, [%%rd3+12];\n");
    fprintf(out, "    add.f32 %%f0, %%f1, %%f2; add.f32 %%f3, %%f3, %%f4; add.f32 %%f0, %%f0, %%f3;\n");
    fprintf(out, "    fma.rn.f32 %%f1, %%f0, 0f%08X, 0f%08X; rsqrt.approx.f32 %%f5, %%f1;\n", (unsigned int)inv_dim_u, (unsigned int)eps_u);
    fprintf(out, "    mov.u32 %%r3, %%r0;\n");
    fprintf(out, "L_mcrnb2:\n    setp.ge.u32 %%p0, %%r3, %u; @%%p0 bra L_mcrnb2_d;\n", (unsigned int)m->dim);
    fprintf(out, "    mul.wide.u32 %%rd4, %%r3, 4; add.u64 %%rd5, %%rd1, %%rd4; ld.f32 %%f1, [%%rd5];\n");
    fprintf(out, "    add.u64 %%rd6, %%rd2, %%rd4; ld.f32 %%f2, [%%rd6];\n");
    fprintf(out, "    mul.f32 %%f3, %%f1, %%f5; mul.f32 %%f3, %%f3, %%f2;\n");
    fprintf(out, "    add.u64 %%rd7, %%rd0, %%rd4; st.f32 [%%rd7], %%f3;\n");
    fprintf(out, "    add.u32 %%r3, %%r3, 128; bra L_mcrnb2;\n");
    fprintf(out, "L_mcrnb2_d:\n    ret;\n}\n\n");

    /* D) Batched Bias Add (if any bias exists) */
    if (m->has_any_bias) {
        fprintf(out, ".visible .entry zcc_aot_mc_add_bias_batch(.param .u64 _pmat, .param .u64 _pw, .param .u64 _pboff, .param .u32 _pN) {\n");
        fprintf(out, "    .reg .pred %%p0; .reg .b32 %%r<5>; .reg .b64 %%rd<8>; .reg .f32 %%f<3>;\n");
        fprintf(out, "    ld.param.u64 %%rd0, [_pmat]; ld.param.u64 %%rd1, [_pw]; ld.param.u64 %%rd2, [_pboff]; add.u64 %%rd1, %%rd1, %%rd2;\n");
        fprintf(out, "    ld.param.u32 %%r0, [_pN]; mov.u32 %%r3, %%ctaid.y; mov.u32 %%r1, %%ctaid.x; mov.u32 %%r2, %%tid.x; mad.lo.u32 %%r2, %%r1, 256, %%r2;\n");
        fprintf(out, "    setp.ge.u32 %%p0, %%r2, %%r0; @%%p0 bra L_mcabb_d;\n");
        fprintf(out, "    mul.lo.u32 %%r4, %%r3, %%r0; add.u32 %%r4, %%r4, %%r2; mul.wide.u32 %%rd3, %%r4, 4; add.u64 %%rd4, %%rd0, %%rd3;\n");
        fprintf(out, "    mul.wide.u32 %%rd6, %%r2, 4; add.u64 %%rd5, %%rd1, %%rd6;\n");
        fprintf(out, "    ld.f32 %%f0, [%%rd4]; ld.f32 %%f1, [%%rd5]; add.f32 %%f2, %%f0, %%f1; st.f32 [%%rd4], %%f2;\n");
        fprintf(out, "L_mcabb_d:\n    ret;\n}\n\n");
    }

    /* E) Batched RoPE + KV Cache store (F32, F16, Q8_0) */
    for (int f16_kv = 0; f16_kv <= 1; f16_kv++) {
        const char *suf = f16_kv ? "_f16" : "";
        fprintf(out, ".visible .entry zcc_aot_mc_rope_kv_batch%s(\n"
                     "    .param .u64 _pq, .param .u64 _pk, .param .u64 _pv,\n"
                     "    .param .u64 _pkc, .param .u64 _pvc, .param .u64 _pargs,\n"
                     "    .param .u32 _player, .param .u32 _ppos0, .param .u32 _pT) {\n", suf);
        fprintf(out, "    .reg .pred %%p<4>; .reg .b32 %%r<16>; .reg .b64 %%rd<15>; .reg .f32 %%f<10>;\n");
        fprintf(out, "    ld.param.u64 %%rd0, [_pq]; ld.param.u64 %%rd1, [_pk]; ld.param.u64 %%rd2, [_pv];\n");
        fprintf(out, "    ld.param.u64 %%rd3, [_pkc]; ld.param.u64 %%rd4, [_pvc]; ld.param.u64 %%rd5, [_pargs];\n");
        fprintf(out, "    ld.param.u32 %%r0, [_player]; ld.param.u32 %%r12, [_ppos0]; ld.param.u32 %%r13, [_pT];\n");
        fprintf(out, "    setp.eq.u32 %%p0, %%r12, 4294967295; @%%p0 ld.u32 %%r12, [%%rd5+4];\n");
        fprintf(out, "    ld.u32 %%r2, [%%rd5+8]; mov.u32 %%r14, %%ctaid.y; setp.ge.u32 %%p0, %%r14, %%r13; @%%p0 bra L_mcrvb%s_d;\n", suf);
        fprintf(out, "    add.u32 %%r1, %%r12, %%r14;\n"); /* pos = pos0 + ti */
        fprintf(out, "    mul.wide.u32 %%rd14, %%r14, %u; shl.b64 %%rd14, %%rd14, 2; add.u64 %%rd0, %%rd0, %%rd14;\n", (unsigned int)m->dim);
        fprintf(out, "    mul.wide.u32 %%rd14, %%r14, %u; shl.b64 %%rd14, %%rd14, 2; add.u64 %%rd1, %%rd1, %%rd14; add.u64 %%rd2, %%rd2, %%rd14;\n", (unsigned int)m->kv_dim);
        fprintf(out, "    mov.u32 %%r3, %%ctaid.x; mov.u32 %%r4, %%tid.x; mad.lo.u32 %%r5, %%r3, 128, %%r4;\n");
        fprintf(out, "    mul.wide.u32 %%rd6, %%r0, %%r2; cvt.u64.u32 %%rd7, %%r1; add.u64 %%rd6, %%rd6, %%rd7;\n");
        fprintf(out, "    mul.lo.u64 %%rd6, %%rd6, %u; shl.b64 %%rd6, %%rd6, %d;\n", (unsigned int)m->kv_dim, f16_kv ? 1 : 2);
        fprintf(out, "    add.u64 %%rd3, %%rd3, %%rd6; add.u64 %%rd4, %%rd4, %%rd6;\n");
        fprintf(out, "    setp.lt.u32 %%p0, %%r5, %u; @%%p0 bra L_mcrvb%s_cp;\n    bra L_mcrvb%s_rp;\nL_mcrvb%s_cp:\n",
                (unsigned int)m->kv_dim, suf, suf, suf);
        fprintf(out, "    mul.wide.u32 %%rd8, %%r5, 4; add.u64 %%rd9, %%rd2, %%rd8; ld.f32 %%f0, [%%rd9];\n");
        if (f16_kv) {
            fprintf(out, "    { .reg .b16 %%hb; cvt.rn.f16.f32 %%hb, %%f0; cvt.u32.u16 %%r15, %%hb; }\n");
            fprintf(out, "    mul.wide.u32 %%rd8, %%r5, 2; add.u64 %%rd10, %%rd4, %%rd8; st.u16 [%%rd10], %%r15;\n");
        } else {
            fprintf(out, "    add.u64 %%rd10, %%rd4, %%rd8; st.f32 [%%rd10], %%f0;\n");
        }
        fprintf(out, "L_mcrvb%s_rp:\n    setp.ge.u32 %%p1, %%r5, %u; @%%p1 bra L_mcrvb%s_d;\n", suf, (unsigned int)total_pairs, suf);
        fprintf(out, "    lg2.approx.f32 %%f1, 0f%08X; cvt.rn.f32.u32 %%f2, %%r1; mov.f32 %%f9, 0f%08X;\n",
                (unsigned int)theta_u, (unsigned int)hd_u);
        fprintf(out, "    div.u32 %%r6, %%r5, %u; rem.u32 %%r7, %%r5, %u;\n", (unsigned int)half_hd, (unsigned int)half_hd);
        fprintf(out, "    setp.lt.u32 %%p2, %%r5, %u; sub.u32 %%r8, %%r6, %u;\n", (unsigned int)q_pairs, (unsigned int)m->n_heads);
        fprintf(out, "    selp.u32 %%r9, %%r6, %%r8, %%p2; mul.lo.u32 %%r9, %%r9, %u; shl.b32 %%r10, %%r7, 1; add.u32 %%r9, %%r9, %s;\n",
                (unsigned int)m->head_dim, elem_reg);
        fprintf(out, "    mul.wide.u32 %%rd11, %%r9, 4; selp.u64 %%rd12, %%rd0, %%rd1, %%p2; add.u64 %%rd12, %%rd12, %%rd11;\n");
        fprintf(out, "    cvt.rn.f32.u32 %%f3, %%r10; neg.f32 %%f3, %%f3; div.rn.f32 %%f3, %%f3, %%f9;\n");
        fprintf(out, "    mul.f32 %%f3, %%f3, %%f1; ex2.approx.f32 %%f3, %%f3; mul.f32 %%f3, %%f2, %%f3;\n");
        fprintf(out, "    cos.approx.f32 %%f4, %%f3; sin.approx.f32 %%f5, %%f3;\n");
        fprintf(out, "    ld.f32 %%f6, [%%rd12]; ld.f32 %%f7, [%%rd12+%u];\n", (unsigned int)f32_pair_off);
        fprintf(out, "    mul.f32 %%f8, %%f6, %%f4; mul.f32 %%f3, %%f7, %%f5; sub.f32 %%f8, %%f8, %%f3;\n");
        fprintf(out, "    mul.f32 %%f3, %%f6, %%f5; fma.rn.f32 %%f3, %%f7, %%f4, %%f3;\n");
        fprintf(out, "    @%%p2 st.f32 [%%rd12], %%f8; @%%p2 st.f32 [%%rd12+%u], %%f3;\n", (unsigned int)f32_pair_off);
        if (f16_kv) {
            fprintf(out, "    { .reg .b16 %%ha, %%hb; cvt.rn.f16.f32 %%ha, %%f8; cvt.rn.f16.f32 %%hb, %%f3; cvt.u32.u16 %%r11, %%ha; cvt.u32.u16 %%r15, %%hb; }\n");
            fprintf(out, "    @!%%p2 mul.wide.u32 %%rd11, %%r9, 2; @!%%p2 add.u64 %%rd13, %%rd3, %%rd11;\n");
            fprintf(out, "    @!%%p2 st.u16 [%%rd13], %%r11; @!%%p2 st.u16 [%%rd13+%u], %%r15;\n", (unsigned int)f16_pair_off);
        } else {
            fprintf(out, "    @!%%p2 add.u64 %%rd13, %%rd3, %%rd11; @!%%p2 st.f32 [%%rd13], %%f8; @!%%p2 st.f32 [%%rd13+%u], %%f3;\n", (unsigned int)f32_pair_off);
        }
        fprintf(out, "L_mcrvb%s_d:\n    ret;\n}\n\n", suf);
    }

    if ((m->head_dim & 31U) == 0U && (m->kv_dim & 31U) == 0U) {
        uint32_t nb_kv = m->kv_dim >> 5;
        uint32_t q8_row_bytes = nb_kv * 34U;
        fprintf(out, ".visible .entry zcc_aot_mc_rope_inplace_batch(.param .u64 _pq, .param .u64 _pk, .param .u64 _pargs, .param .u32 _ppos0, .param .u32 _pT) {\n");
        fprintf(out, "    .reg .pred %%p<4>; .reg .b32 %%r<15>; .reg .b64 %%rd<15>; .reg .f32 %%f<10>;\n");
        fprintf(out, "    ld.param.u64 %%rd0, [_pq]; ld.param.u64 %%rd1, [_pk]; ld.param.u64 %%rd5, [_pargs]; ld.param.u32 %%r12, [_ppos0]; ld.param.u32 %%r13, [_pT];\n");
        fprintf(out, "    setp.eq.u32 %%p0, %%r12, 4294967295; @%%p0 ld.u32 %%r12, [%%rd5+4];\n");
        fprintf(out, "    mov.u32 %%r14, %%ctaid.y; setp.ge.u32 %%p0, %%r14, %%r13; @%%p0 bra L_mcrib_d;\n");
        fprintf(out, "    add.u32 %%r1, %%r12, %%r14;\n");
        fprintf(out, "    mul.wide.u32 %%rd14, %%r14, %u; shl.b64 %%rd14, %%rd14, 2; add.u64 %%rd0, %%rd0, %%rd14;\n", (unsigned int)m->dim);
        fprintf(out, "    mul.wide.u32 %%rd14, %%r14, %u; shl.b64 %%rd14, %%rd14, 2; add.u64 %%rd1, %%rd1, %%rd14;\n", (unsigned int)m->kv_dim);
        fprintf(out, "    mov.u32 %%r3, %%ctaid.x; mov.u32 %%r4, %%tid.x; mad.lo.u32 %%r5, %%r3, 128, %%r4;\n");
        fprintf(out, "    setp.ge.u32 %%p1, %%r5, %u; @%%p1 bra L_mcrib_d;\n", (unsigned int)total_pairs);
        fprintf(out, "    lg2.approx.f32 %%f1, 0f%08X; cvt.rn.f32.u32 %%f2, %%r1; mov.f32 %%f9, 0f%08X;\n",
                (unsigned int)theta_u, (unsigned int)hd_u);
        fprintf(out, "    div.u32 %%r6, %%r5, %u; rem.u32 %%r7, %%r5, %u;\n", (unsigned int)half_hd, (unsigned int)half_hd);
        fprintf(out, "    setp.lt.u32 %%p2, %%r5, %u; sub.u32 %%r8, %%r6, %u;\n", (unsigned int)q_pairs, (unsigned int)m->n_heads);
        fprintf(out, "    selp.u32 %%r9, %%r6, %%r8, %%p2; mul.lo.u32 %%r9, %%r9, %u; shl.b32 %%r10, %%r7, 1; add.u32 %%r9, %%r9, %s;\n",
                (unsigned int)m->head_dim, elem_reg);
        fprintf(out, "    mul.wide.u32 %%rd11, %%r9, 4; selp.u64 %%rd12, %%rd0, %%rd1, %%p2; add.u64 %%rd12, %%rd12, %%rd11;\n");
        fprintf(out, "    cvt.rn.f32.u32 %%f3, %%r10; neg.f32 %%f3, %%f3; div.rn.f32 %%f3, %%f3, %%f9;\n");
        fprintf(out, "    mul.f32 %%f3, %%f3, %%f1; ex2.approx.f32 %%f3, %%f3; mul.f32 %%f3, %%f2, %%f3;\n");
        fprintf(out, "    cos.approx.f32 %%f4, %%f3; sin.approx.f32 %%f5, %%f3;\n");
        fprintf(out, "    ld.f32 %%f6, [%%rd12]; ld.f32 %%f7, [%%rd12+%u];\n", (unsigned int)f32_pair_off);
        fprintf(out, "    mul.f32 %%f8, %%f6, %%f4; mul.f32 %%f3, %%f7, %%f5; sub.f32 %%f8, %%f8, %%f3;\n");
        fprintf(out, "    mul.f32 %%f3, %%f6, %%f5; fma.rn.f32 %%f3, %%f7, %%f4, %%f3;\n");
        fprintf(out, "    st.f32 [%%rd12], %%f8; st.f32 [%%rd12+%u], %%f3;\n", (unsigned int)f32_pair_off);
        fprintf(out, "L_mcrib_d:\n    ret;\n}\n\n");

        fprintf(out, ".visible .entry zcc_aot_mc_kv_store_batch_q8_0(\n"
                     "    .param .u64 _pk, .param .u64 _pv, .param .u64 _pkc, .param .u64 _pvc,\n"
                     "    .param .u64 _pargs, .param .u32 _player, .param .u32 _ppos0, .param .u32 _pT) {\n");
        fprintf(out, "    .reg .pred %%p<4>; .reg .b32 %%r<16>; .reg .b64 %%rd<15>; .reg .f32 %%f<8>;\n");
        fprintf(out, "    ld.param.u64 %%rd0, [_pk]; ld.param.u64 %%rd1, [_pv];\n");
        fprintf(out, "    ld.param.u64 %%rd2, [_pkc]; ld.param.u64 %%rd3, [_pvc]; ld.param.u64 %%rd4, [_pargs];\n");
        fprintf(out, "    ld.param.u32 %%r0, [_player]; ld.param.u32 %%r13, [_ppos0]; ld.param.u32 %%r14, [_pT];\n");
        fprintf(out, "    setp.eq.u32 %%p0, %%r13, 4294967295; @%%p0 ld.u32 %%r13, [%%rd4+4];\n");
        fprintf(out, "    mov.u32 %%r15, %%ctaid.y; setp.ge.u32 %%p0, %%r15, %%r14; @%%p0 bra L_mckqb_d;\n");
        fprintf(out, "    add.u32 %%r1, %%r13, %%r15; ld.u32 %%r2, [%%rd4+8];\n");
        fprintf(out, "    mul.wide.u32 %%rd14, %%r15, %u; shl.b64 %%rd14, %%rd14, 2; add.u64 %%rd0, %%rd0, %%rd14; add.u64 %%rd1, %%rd1, %%rd14;\n", (unsigned int)m->kv_dim);
        fprintf(out, "    mov.u32 %%r3, %%ctaid.x; mov.u32 %%r4, %%tid.x; and.b32 %%r4, %%r4, 31;\n");
        fprintf(out, "    setp.ge.u32 %%p0, %%r3, %u; @%%p0 bra L_mckqb_d;\n", (unsigned int)(2U * nb_kv));
        fprintf(out, "    setp.lt.u32 %%p1, %%r3, %u; sub.u32 %%r5, %%r3, %u; selp.u32 %%r6, %%r3, %%r5, %%p1;\n",
                (unsigned int)nb_kv, (unsigned int)nb_kv);
        fprintf(out, "    mul.wide.u32 %%rd5, %%r0, %%r2; cvt.u64.u32 %%rd6, %%r1; add.u64 %%rd5, %%rd5, %%rd6;\n");
        fprintf(out, "    mul.lo.u64 %%rd5, %%rd5, %u;\n", (unsigned int)q8_row_bytes);
        fprintf(out, "    selp.u64 %%rd7, %%rd2, %%rd3, %%p1; add.u64 %%rd7, %%rd7, %%rd5;\n");
        fprintf(out, "    mul.wide.u32 %%rd8, %%r6, 34; add.u64 %%rd7, %%rd7, %%rd8;\n");
        fprintf(out, "    selp.u64 %%rd9, %%rd0, %%rd1, %%p1; shl.b32 %%r7, %%r6, 5; add.u32 %%r7, %%r7, %%r4;\n");
        fprintf(out, "    mul.wide.u32 %%rd10, %%r7, 4; add.u64 %%rd9, %%rd9, %%rd10; ld.f32 %%f0, [%%rd9];\n");
        fprintf(out, "    abs.f32 %%f1, %%f0;\n");
        fprintf(out, "    { .param .f32 _wr, _wa; st.param.f32 [_wa], %%f1; call.uni (_wr), zcc_aot_warp_max, (_wa); ld.param.f32 %%f2, [_wr]; }\n");
        fprintf(out, "    div.rn.f32 %%f3, %%f2, 0f42FE0000;\n");
        fprintf(out, "    { .reg .b16 %%hb; cvt.rn.f16.f32 %%hb, %%f3; cvt.u32.u16 %%r8, %%hb; cvt.f32.f16 %%f3, %%hb; }\n");
        fprintf(out, "    setp.eq.u32 %%p2, %%r4, 0; @%%p2 and.b32 %%r9, %%r8, 255; @%%p2 shr.u32 %%r10, %%r8, 8;\n");
        fprintf(out, "    @%%p2 st.u8 [%%rd7], %%r9; @%%p2 st.u8 [%%rd7+1], %%r10;\n");
        fprintf(out, "    mov.f32 %%f4, 0f00000000; setp.gt.f32 %%p3, %%f3, 0f00000000; @%%p3 div.rn.f32 %%f4, 0f3F800000, %%f3;\n");
        fprintf(out, "    mul.f32 %%f5, %%f0, %%f4; cvt.rni.s32.f32 %%r11, %%f5; max.s32 %%r11, %%r11, -127; min.s32 %%r11, %%r11, 127;\n");
        fprintf(out, "    add.u32 %%r12, %%r4, 2; cvt.u64.u32 %%rd11, %%r12; add.u64 %%rd11, %%rd7, %%rd11; st.u8 [%%rd11], %%r11;\n");
        fprintf(out, "L_mckqb_d:\n    ret;\n}\n\n");
    }

    /* F) Batched Causal FlashAttention-2 (F32, F16, Q8_0) */
    for (int kv_mode = 0; kv_mode < 3; kv_mode++) {
        if (kv_mode == 2 && ((m->head_dim & 31U) != 0U || (m->kv_dim & 31U) != 0U)) continue;
        const char *suf = (kv_mode == 0) ? "" : (kv_mode == 1) ? "_f16" : "_q8_0";
        uint32_t row_bytes = (kv_mode == 0) ? (m->kv_dim * 4U) : (kv_mode == 1) ? (m->kv_dim * 2U) : ((m->kv_dim >> 5) * 34U);
        uint32_t head_stride = (kv_mode == 2) ? (chunks * 34U) : m->head_dim;
        fprintf(out, ".visible .entry zcc_aot_mc_attn_gqa_batch%s(\n"
                     "    .param .u64 _pq, .param .u64 _pkc, .param .u64 _pvc,\n"
                     "    .param .u64 _pout, .param .u64 _pargs, .param .u32 _player, .param .u32 _ppos0, .param .u32 _pT) {\n", suf);
        fprintf(out, "    .reg .pred %%p<4>; .reg .b32 %%r<16>; .reg .b64 %%rd<16>;\n");
        fprintf(out, "    .reg .f32 %%f<14>; .reg .f32 %%fq<8>; .reg .f32 %%fa<8>;\n");
        fprintf(out, "    ld.param.u64 %%rd0, [_pq]; ld.param.u64 %%rd1, [_pkc]; ld.param.u64 %%rd2, [_pvc];\n");
        fprintf(out, "    ld.param.u64 %%rd3, [_pout]; ld.param.u64 %%rd4, [_pargs]; ld.param.u32 %%r0, [_player];\n");
        fprintf(out, "    ld.param.u32 %%r10, [_ppos0]; ld.param.u32 %%r11, [_pT];\n");
        fprintf(out, "    setp.eq.u32 %%p0, %%r10, 4294967295; @%%p0 ld.u32 %%r10, [%%rd4+4];\n");
        fprintf(out, "    mov.u32 %%r12, %%ctaid.y; setp.ge.u32 %%p0, %%r12, %%r11; @%%p0 bra L_mcab%s_done;\n", suf);
        fprintf(out, "    mov.u32 %%r1, %%ctaid.x; setp.ge.u32 %%p0, %%r1, %u; @%%p0 bra L_mcab%s_done;\n",
                (unsigned int)m->n_heads, suf);
        fprintf(out, "    mov.u32 %%r2, %%tid.x; and.b32 %%r2, %%r2, 31;\n");
        if (kv_mode == 2) fprintf(out, "    add.u32 %%r14, %%r2, 2; cvt.u64.u32 %%rd15, %%r14;\n");
        fprintf(out, "    add.u32 %%r3, %%r10, %%r12; ld.u32 %%r4, [%%rd4+8];\n");
        fprintf(out, "    div.u32 %%r5, %%r1, %u; mul.lo.u32 %%r6, %%r5, %u;\n", (unsigned int)kv_group, (unsigned int)head_stride);
        fprintf(out, "    mad.lo.u32 %%r7, %%r12, %u, %%r1; mul.wide.u32 %%rd5, %%r7, %u;\n",
                (unsigned int)m->n_heads, (unsigned int)(m->head_dim * 4U));
        fprintf(out, "    add.u64 %%rd6, %%rd0, %%rd5; add.u64 %%rd7, %%rd3, %%rd5;\n");
        fprintf(out, "    mov.f32 %%f0, 0f%08X; rsqrt.approx.f32 %%f0, %%f0;\n", (unsigned int)hd_u);
        for (uint32_t c = 0U; c < chunks; c++) {
            fprintf(out, "    add.u32 %%r8, %%r2, %u; setp.lt.u32 %%p1, %%r8, %u;\n", (unsigned int)(c * 32U), (unsigned int)m->head_dim);
            fprintf(out, "    mov.f32 %%fq%u, 0f00000000; mov.f32 %%fa%u, 0f00000000;\n", (unsigned int)c, (unsigned int)c);
            fprintf(out, "    @%%p1 mul.wide.u32 %%rd8, %%r8, 4; @%%p1 add.u64 %%rd8, %%rd6, %%rd8; @%%p1 ld.f32 %%fq%u, [%%rd8];\n", (unsigned int)c);
        }
        fprintf(out, "    mul.wide.u32 %%rd12, %%r0, %%r4;\n");
        fprintf(out, "    mov.f32 %%f1, 0fF149F2CA; mov.f32 %%f2, 0f00000000; mov.u32 %%r9, 0;\n");
        fprintf(out, "L_mcab%s_t:\n    setp.gt.u32 %%p0, %%r9, %%r3; @%%p0 bra L_mcab%s_td;\n", suf, suf);
        if (kv_mode == 2) {
            fprintf(out, "    cvt.u64.u32 %%rd8, %%r9; add.u64 %%rd9, %%rd12, %%rd8; mul.lo.u64 %%rd9, %%rd9, %u;\n", (unsigned int)row_bytes);
            fprintf(out, "    cvt.u64.u32 %%rd8, %%r6; add.u64 %%rd9, %%rd9, %%rd8;\n");
        } else {
            fprintf(out, "    cvt.u64.u32 %%rd8, %%r9; add.u64 %%rd9, %%rd12, %%rd8; mul.lo.u64 %%rd9, %%rd9, %u;\n", (unsigned int)m->kv_dim);
            fprintf(out, "    cvt.u64.u32 %%rd8, %%r6; add.u64 %%rd9, %%rd9, %%rd8; shl.b64 %%rd9, %%rd9, %d;\n", (kv_mode == 1) ? 1 : 2);
        }
        fprintf(out, "    add.u64 %%rd10, %%rd1, %%rd9; add.u64 %%rd11, %%rd2, %%rd9;\n");
        fprintf(out, "    mov.f32 %%f3, 0f00000000;\n");
        for (uint32_t c = 0U; c < chunks; c++) {
            if (kv_mode == 2) {
                uint32_t boff = c * 34U;
                fprintf(out, "    ld.u8 %%r13, [%%rd10+%u]; ld.u8 %%r14, [%%rd10+%u]; shl.b32 %%r14, %%r14, 8; or.b32 %%r13, %%r13, %%r14;\n",
                        (unsigned int)boff, (unsigned int)(boff + 1U));
                fprintf(out, "    { .reg .b16 %%hb; cvt.u16.u32 %%hb, %%r13; cvt.f32.f16 %%f11, %%hb; }\n");
                fprintf(out, "    add.u64 %%rd8, %%rd10, %%rd15; ld.u8 %%r13, [%%rd8+%u]; setp.ge.u32 %%p1, %%r13, 128; @%%p1 sub.s32 %%r13, %%r13, 256;\n",
                        (unsigned int)boff);
                fprintf(out, "    cvt.rn.f32.s32 %%f4, %%r13; mul.f32 %%f4, %%f4, %%f11; fma.rn.f32 %%f3, %%fq%u, %%f4, %%f3;\n", (unsigned int)c);
            } else if (kv_mode == 1) {
                fprintf(out, "    add.u32 %%r8, %%r2, %u; setp.lt.u32 %%p1, %%r8, %u; mov.u32 %%r13, 0;\n", (unsigned int)(c * 32U), (unsigned int)m->head_dim);
                fprintf(out, "    @%%p1 mul.wide.u32 %%rd8, %%r8, 2; @%%p1 add.u64 %%rd8, %%rd10, %%rd8; @%%p1 ld.u16 %%r13, [%%rd8];\n");
                fprintf(out, "    { .reg .b16 %%hb; cvt.u16.u32 %%hb, %%r13; cvt.f32.f16 %%f4, %%hb; }\n");
                fprintf(out, "    @%%p1 fma.rn.f32 %%f3, %%fq%u, %%f4, %%f3;\n", (unsigned int)c);
            } else {
                fprintf(out, "    add.u32 %%r8, %%r2, %u; setp.lt.u32 %%p1, %%r8, %u;\n", (unsigned int)(c * 32U), (unsigned int)m->head_dim);
                fprintf(out, "    @%%p1 mul.wide.u32 %%rd8, %%r8, 4; @%%p1 add.u64 %%rd8, %%rd10, %%rd8; @%%p1 ld.f32 %%f4, [%%rd8];\n");
                fprintf(out, "    @%%p1 fma.rn.f32 %%f3, %%fq%u, %%f4, %%f3;\n", (unsigned int)c);
            }
        }
        fprintf(out, "    { .param .f32 _wr, _wa; st.param.f32 [_wa], %%f3; call.uni (_wr), zcc_aot_warp_sum, (_wa); ld.param.f32 %%f5, [_wr]; }\n");
        fprintf(out, "    mul.f32 %%f5, %%f5, %%f0;\n");
        fprintf(out, "    setp.eq.u32 %%p2, %%r9, 0; @%%p2 bra L_mcab%s_t0;\n", suf);
        fprintf(out, "    max.f32 %%f6, %%f1, %%f5;\n");
        fprintf(out, "    sub.f32 %%f7, %%f1, %%f6; mul.f32 %%f7, %%f7, 0f3FB8AA3B; ex2.approx.f32 %%f7, %%f7;\n");
        fprintf(out, "    sub.f32 %%f8, %%f5, %%f6; mul.f32 %%f8, %%f8, 0f3FB8AA3B; ex2.approx.f32 %%f8, %%f8;\n");
        fprintf(out, "    fma.rn.f32 %%f2, %%f2, %%f7, %%f8; mov.f32 %%f1, %%f6; bra L_mcab%s_vu;\nL_mcab%s_t0:\n", suf, suf);
        fprintf(out, "    mov.f32 %%f1, %%f5; mov.f32 %%f2, 0f3F800000; mov.f32 %%f7, 0f00000000; mov.f32 %%f8, 0f3F800000;\nL_mcab%s_vu:\n", suf);
        for (uint32_t c = 0U; c < chunks; c++) {
            if (kv_mode == 2) {
                uint32_t boff = c * 34U;
                fprintf(out, "    ld.u8 %%r13, [%%rd11+%u]; ld.u8 %%r14, [%%rd11+%u]; shl.b32 %%r14, %%r14, 8; or.b32 %%r13, %%r13, %%r14;\n",
                        (unsigned int)boff, (unsigned int)(boff + 1U));
                fprintf(out, "    { .reg .b16 %%hb; cvt.u16.u32 %%hb, %%r13; cvt.f32.f16 %%f12, %%hb; }\n");
                fprintf(out, "    add.u64 %%rd8, %%rd11, %%rd15; ld.u8 %%r13, [%%rd8+%u]; setp.ge.u32 %%p1, %%r13, 128; @%%p1 sub.s32 %%r13, %%r13, 256;\n",
                        (unsigned int)boff);
                fprintf(out, "    cvt.rn.f32.s32 %%f9, %%r13; mul.f32 %%f9, %%f9, %%f12;\n");
                fprintf(out, "    mul.f32 %%fa%u, %%fa%u, %%f7; fma.rn.f32 %%fa%u, %%f8, %%f9, %%fa%u;\n",
                        (unsigned int)c, (unsigned int)c, (unsigned int)c, (unsigned int)c);
            } else if (kv_mode == 1) {
                fprintf(out, "    add.u32 %%r8, %%r2, %u; setp.lt.u32 %%p1, %%r8, %u; mov.u32 %%r13, 0;\n", (unsigned int)(c * 32U), (unsigned int)m->head_dim);
                fprintf(out, "    @%%p1 mul.wide.u32 %%rd8, %%r8, 2; @%%p1 add.u64 %%rd8, %%rd11, %%rd8; @%%p1 ld.u16 %%r13, [%%rd8];\n");
                fprintf(out, "    { .reg .b16 %%hb; cvt.u16.u32 %%hb, %%r13; cvt.f32.f16 %%f9, %%hb; }\n");
                fprintf(out, "    @%%p1 mul.f32 %%fa%u, %%fa%u, %%f7; @%%p1 fma.rn.f32 %%fa%u, %%f8, %%f9, %%fa%u;\n",
                        (unsigned int)c, (unsigned int)c, (unsigned int)c, (unsigned int)c);
            } else {
                fprintf(out, "    add.u32 %%r8, %%r2, %u; setp.lt.u32 %%p1, %%r8, %u;\n", (unsigned int)(c * 32U), (unsigned int)m->head_dim);
                fprintf(out, "    @%%p1 mul.wide.u32 %%rd8, %%r8, 4; @%%p1 add.u64 %%rd8, %%rd11, %%rd8; @%%p1 ld.f32 %%f9, [%%rd8];\n");
                fprintf(out, "    @%%p1 mul.f32 %%fa%u, %%fa%u, %%f7; @%%p1 fma.rn.f32 %%fa%u, %%f8, %%f9, %%fa%u;\n",
                        (unsigned int)c, (unsigned int)c, (unsigned int)c, (unsigned int)c);
            }
        }
        fprintf(out, "    add.u32 %%r9, %%r9, 1; bra L_mcab%s_t;\n", suf);
        fprintf(out, "L_mcab%s_td:\n    div.rn.f32 %%f10, 0f3F800000, %%f2;\n", suf);
        for (uint32_t c = 0U; c < chunks; c++) {
            fprintf(out, "    add.u32 %%r8, %%r2, %u; setp.lt.u32 %%p1, %%r8, %u;\n", (unsigned int)(c * 32U), (unsigned int)m->head_dim);
            fprintf(out, "    @%%p1 mul.f32 %%fa%u, %%fa%u, %%f10; @%%p1 mul.wide.u32 %%rd8, %%r8, 4; @%%p1 add.u64 %%rd8, %%rd7, %%rd8; @%%p1 st.f32 [%%rd8], %%fa%u;\n",
                    (unsigned int)c, (unsigned int)c, (unsigned int)c);
        }
        fprintf(out, "L_mcab%s_done:\n    ret;\n}\n\n", suf);
    }

    /* G) Batched SwiGLU (T * hidden_dim contiguous elements) */
    fprintf(out, ".visible .entry zcc_aot_mc_swiglu_batch(.param .u64 _phb, .param .u64 _phb2, .param .u32 _pN) {\n");
    fprintf(out, "    .reg .pred %%p0; .reg .b32 %%r<4>; .reg .b64 %%rd<5>; .reg .f32 %%f<4>;\n");
    fprintf(out, "    ld.param.u64 %%rd0, [_phb]; ld.param.u64 %%rd1, [_phb2]; ld.param.u32 %%r3, [_pN];\n");
    fprintf(out, "    mov.u32 %%r1, %%ctaid.x; mov.u32 %%r2, %%tid.x; mad.lo.u32 %%r0, %%r1, 256, %%r2;\n");
    fprintf(out, "    setp.ge.u32 %%p0, %%r0, %%r3; @%%p0 bra L_mcswb_d;\n");
    fprintf(out, "    mul.wide.u32 %%rd2, %%r0, 4; add.u64 %%rd3, %%rd0, %%rd2; ld.f32 %%f0, [%%rd3];\n");
    fprintf(out, "    add.u64 %%rd4, %%rd1, %%rd2; ld.f32 %%f1, [%%rd4];\n");
    fprintf(out, "    neg.f32 %%f2, %%f0; mul.f32 %%f2, %%f2, 0f3FB8AA3B; ex2.approx.f32 %%f2, %%f2;\n");
    fprintf(out, "    add.f32 %%f2, %%f2, 0f3F800000; div.rn.f32 %%f3, %%f0, %%f2; mul.f32 %%f3, %%f3, %%f1;\n");
    fprintf(out, "    st.f32 [%%rd3], %%f3;\nL_mcswb_d:\n    ret;\n}\n\n");
}

static void gguf_ptx_emit_meta_kernel(FILE *out, const GgufPtxModel *m, int tier_b) {
    uint64_t meta_vals[16];
    meta_vals[0]  = tier_b ? 0x5A4343414F543032ULL : 0x5A4343414F543031ULL;
    meta_vals[1]  = m->file_fingerprint;
    meta_vals[2]  = (uint64_t)m->n_layers;
    meta_vals[3]  = (uint64_t)m->dim;
    meta_vals[4]  = (uint64_t)m->hidden_dim;
    meta_vals[5]  = (uint64_t)m->n_heads;
    meta_vals[6]  = (uint64_t)m->n_kv_heads;
    meta_vals[7]  = (uint64_t)m->max_seq_len;
    meta_vals[8]  = (uint64_t)m->vocab_size;
    meta_vals[9]  = m->wblob_bytes;
    meta_vals[10] = m->data_start;
    meta_vals[11] = tier_b ? 0ULL : (uint64_t)m->smem_bytes;
    meta_vals[12] = (uint64_t)(m->need_tc ? 1 : 0);
    meta_vals[13] = (uint64_t)m->weight_type_primary;
    meta_vals[14] = 128ULL;
    meta_vals[15] = tier_b ? (uint64_t)(m->has_any_bias ? 1 : 0) : 1ULL;
    fprintf(out, ".visible .entry zcc_aot_model_meta(.param .u64 _p0) {\n");
    fprintf(out, "    .reg .pred %%p0; .reg .b32 %%r0; .reg .b64 %%rd0, %%rd1;\n");
    fprintf(out, "    mov.u32 %%r0, %%tid.x; setp.ne.u32 %%p0, %%r0, 0; @%%p0 bra L_md;\n");
    fprintf(out, "    ld.param.u64 %%rd0, [_p0];\n");
    for (int i = 0; i < 16; i++) {
        char hbuf[32];
        gguf_ptx_fmt_u64_hex(meta_vals[i], hbuf, sizeof(hbuf));
        fprintf(out, "    mov.u64 %%rd1, %s; st.u64 [%%rd0+%d], %%rd1;\n", hbuf, i * 8);
    }
    fprintf(out, "L_md:\n    ret;\n}\n\n");
}

int ptx_compile_gguf_model(const char *gguf_path, const char *out_ptx, const char *ptx_arch) {
    if (!gguf_path || !out_ptx) return -1;
    const char *arch = (ptx_arch && ptx_arch[0]) ? ptx_arch : "sm_120";
    const char *ptx_ver = (strncmp(arch, "sm_12", 5) == 0 || strncmp(arch, "sm_10", 5) == 0) ? "8.7" : "8.5";

    GgufPtxModel *m = (GgufPtxModel *)calloc(1U, sizeof(GgufPtxModel));
    if (!m) return -1;
    if (gguf_ptx_read_model(gguf_path, m) != 0) {
        gguf_ptx_free_model(m);
        free(m);
        return -1;
    }

    FILE *out = fopen(out_ptx, "wb");
    if (!out) {
        fprintf(stderr, "zcc ptx: cannot open output file '%s'\n", out_ptx);
        gguf_ptx_free_model(m);
        free(m);
        return -1;
    }

    char fp_hex[32];
    gguf_ptx_fmt_u64_hex(m->file_fingerprint, fp_hex, sizeof(fp_hex));
    fprintf(out, "// ZCC Native GGUF -> Blackwell AOT Specialized PTX (Target: %s, PTX ISA %s)\n", arch, ptx_ver);
    fprintf(out, "// Model: arch=%s rope=%s tier=%s layers=%u dim=%u hidden=%u heads=%u kv_heads=%u vocab=%u wtype=%u tc=%d smem=%uB fingerprint=%s\n",
            m->arch, (strcmp(m->arch, "qwen2") == 0) ? "neox" : "norm",
            m->use_tier_b ? "ZCCAOT02_MULTI_CTA" : "ZCCAOT01_SINGLE_CTA",
            (unsigned int)m->n_layers, (unsigned int)m->dim, (unsigned int)m->hidden_dim,
            (unsigned int)m->n_heads, (unsigned int)m->n_kv_heads, (unsigned int)m->vocab_size,
            (unsigned int)m->weight_type_primary, m->need_tc, (unsigned int)m->smem_bytes, fp_hex);
    fprintf(out, ".version %s\n.target %s\n.address_size 64\n\n", ptx_ver, arch);

    gguf_ptx_emit_warp_reduce(out, "zcc_aot_warp_sum", "add.f32");
    gguf_ptx_emit_warp_reduce(out, "zcc_aot_warp_max", "max.f32");

    if (m->use_tier_b) {
        int emit_prefill = ((m->dim & 15U) == 0U && (m->hidden_dim & 15U) == 0U && (m->head_dim & 15U) == 0U);
        gguf_ptx_emit_mc_embed_fn(out, m, m->t_tok_emb);
        gguf_ptx_emit_mc_rmsnorm_fn(out, m);
        if (m->need_f32) { gguf_ptx_emit_mc_matvec_fn(out, GGUF_PTX_F32); gguf_ptx_emit_mc_matvec_multi_fn(out, GGUF_PTX_F32, 4); gguf_ptx_emit_mc_matvec_multi_fn(out, GGUF_PTX_F32, 8); }
        if (m->need_f16_matvec) { gguf_ptx_emit_mc_matvec_fn(out, GGUF_PTX_F16); gguf_ptx_emit_mc_matvec_multi_fn(out, GGUF_PTX_F16, 4); gguf_ptx_emit_mc_matvec_multi_fn(out, GGUF_PTX_F16, 8); }
        if (m->need_bf16) { gguf_ptx_emit_mc_matvec_fn(out, GGUF_PTX_BF16); gguf_ptx_emit_mc_matvec_multi_fn(out, GGUF_PTX_BF16, 4); gguf_ptx_emit_mc_matvec_multi_fn(out, GGUF_PTX_BF16, 8); }
        if (m->need_q8_0) { gguf_ptx_emit_mc_matvec_fn(out, GGUF_PTX_Q8_0); gguf_ptx_emit_mc_matvec_multi_fn(out, GGUF_PTX_Q8_0, 4); gguf_ptx_emit_mc_matvec_multi_fn(out, GGUF_PTX_Q8_0, 8); }
        if (m->need_q4_0) { gguf_ptx_emit_mc_matvec_fn(out, GGUF_PTX_Q4_0); gguf_ptx_emit_mc_matvec_multi_fn(out, GGUF_PTX_Q4_0, 4); gguf_ptx_emit_mc_matvec_multi_fn(out, GGUF_PTX_Q4_0, 8); }
        if (m->need_q4_k) { gguf_ptx_emit_mc_matvec_fn(out, GGUF_PTX_Q4_K); gguf_ptx_emit_mc_matvec_multi_fn(out, GGUF_PTX_Q4_K, 4); gguf_ptx_emit_mc_matvec_multi_fn(out, GGUF_PTX_Q4_K, 8); }
        if (m->need_q6_k) { gguf_ptx_emit_mc_matvec_fn(out, GGUF_PTX_Q6_K); gguf_ptx_emit_mc_matvec_multi_fn(out, GGUF_PTX_Q6_K, 4); gguf_ptx_emit_mc_matvec_multi_fn(out, GGUF_PTX_Q6_K, 8); }
        if (m->need_tc) gguf_ptx_emit_mc_tc_fns(out);
        else if (emit_prefill) gguf_ptx_emit_mc_f32_to_f16_fn(out);
        if (m->has_any_bias) gguf_ptx_emit_mc_add_bias_fn(out);
        gguf_ptx_emit_mc_rope_kv_fn(out, m);
        gguf_ptx_emit_mc_attn_fn(out, m);
        gguf_ptx_emit_mc_swiglu_fn(out, m);
        if (emit_prefill) gguf_ptx_emit_mc_prefill_fns(out, m);
        gguf_ptx_emit_mc_argmax_fns(out);
        gguf_ptx_emit_meta_kernel(out, m, 1);
    } else {
        if (m->need_f32 && m->t_tok_emb == GGUF_PTX_F32) gguf_ptx_emit_embed_fn(out, m, GGUF_PTX_F32);
        if (m->need_f16_emb) gguf_ptx_emit_embed_fn(out, m, GGUF_PTX_F16);
        if (m->need_emb_q8_0 && m->t_tok_emb == GGUF_PTX_Q8_0) gguf_ptx_emit_embed_fn(out, m, GGUF_PTX_Q8_0);
        gguf_ptx_emit_rmsnorm_fn(out, m);
        if (m->need_f32) gguf_ptx_emit_matvec_fn(out, GGUF_PTX_F32);
        if (m->need_f16_matvec) gguf_ptx_emit_matvec_fn(out, GGUF_PTX_F16);
        if (m->need_q8_0) gguf_ptx_emit_matvec_fn(out, GGUF_PTX_Q8_0);
        if (m->need_tc) gguf_ptx_emit_tc_fns(out);
        gguf_ptx_emit_rope_kv_fn(out, m);
        gguf_ptx_emit_attn_fn(out, m);
        gguf_ptx_emit_swiglu_fn(out, m);
        gguf_ptx_emit_meta_kernel(out, m, 0);

        /* Single-launch fused forward megakernel */
        fprintf(out, ".visible .entry zcc_aot_forward_token(\n"
                     "    .param .u64 _pw, .param .u64 _pkc, .param .u64 _pvc,\n"
                     "    .param .u64 _plog, .param .u32 _ptok, .param .u32 _ppos\n) {\n");
        fprintf(out, "    .shared .align 16 .b8 smem_pool[%u];\n", (unsigned int)m->smem_bytes);
        fprintf(out, "    .reg .b32 %%r_tok, %%r_pos, %%r_t0, %%r_t1, %%r_t2;\n");
        fprintf(out, "    .reg .b64 %%rd_w, %%rd_kc, %%rd_vc, %%rd_log, %%rd_s;\n");
        fprintf(out, "    .reg .b64 %%rd_t0, %%rd_t1, %%rd_t2, %%rd_t3;\n");
        fprintf(out, "    ld.param.u64 %%rd_w, [_pw]; ld.param.u64 %%rd_kc, [_pkc];\n");
        fprintf(out, "    ld.param.u64 %%rd_vc, [_pvc]; ld.param.u64 %%rd_log, [_plog];\n");
        fprintf(out, "    ld.param.u32 %%r_tok, [_ptok]; ld.param.u32 %%r_pos, [_ppos];\n");
        fprintf(out, "    cvta.shared.u64 %%rd_s, smem_pool;\n\n");

        const char *emb_fn = (m->t_tok_emb == GGUF_PTX_F16) ? "zcc_aot_embed_f16" :
                             (m->t_tok_emb == GGUF_PTX_Q8_0) ? "zcc_aot_embed_q8_0" : "zcc_aot_embed_f32";
        gguf_ptx_emit_ptr_add(out, "%rd_t0", "%rd_w", m->off_tok_emb);
        gguf_ptx_emit_ptr_add(out, "%rd_t1", "%rd_s", (uint64_t)m->smem_x);
        fprintf(out, "    { .param .u64 _a0, _a1; .param .u32 _a2; "
                     "st.param.u64 [_a0], %%rd_t0; st.param.u64 [_a1], %%rd_t1; st.param.u32 [_a2], %%r_tok; "
                     "call.uni %s, (_a0, _a1, _a2); }\n\n", emb_fn);

        for (uint32_t l = 0U; l < m->n_layers; l++) {
            const GgufPtxLayer *lw = &m->layers[l];
            uint32_t layer_kv_elem = l * m->max_seq_len * m->kv_dim;
            int tc_q = (lw->t_wq == GGUF_PTX_F16 && (m->dim & 15U) == 0U);
            int tc_k = (lw->t_wk == GGUF_PTX_F16 && (m->dim & 15U) == 0U);
            int tc_v = (lw->t_wv == GGUF_PTX_F16 && (m->dim & 15U) == 0U);
            int tc_o = (lw->t_wo == GGUF_PTX_F16 && (m->dim & 15U) == 0U);
            int tc_g = (lw->t_wgate == GGUF_PTX_F16 && (m->dim & 15U) == 0U);
            int tc_u = (lw->t_wup == GGUF_PTX_F16 && (m->dim & 15U) == 0U);
            int tc_d = (lw->t_wdown == GGUF_PTX_F16 && (m->hidden_dim & 15U) == 0U);

            fprintf(out, "    /* Layer %u */\n", (unsigned int)l);
            gguf_ptx_emit_call_rmsnorm(out, m, m->smem_xb, m->smem_x, lw->off_attn_norm);
            if (tc_q || tc_k || tc_v) gguf_ptx_emit_call_f32_to_f16(out, m->smem_xb, m->smem_xf16, m->dim);
            gguf_ptx_emit_call_linear(out, lw->t_wq, lw->off_wq, tc_q, m->smem_xb, m->smem_xf16, "%rd_s", m->smem_q, m->dim, m->dim, 0);
            gguf_ptx_emit_call_linear(out, lw->t_wk, lw->off_wk, tc_k, m->smem_xb, m->smem_xf16, "%rd_s", m->smem_k, m->kv_dim, m->dim, 0);
            gguf_ptx_emit_call_linear(out, lw->t_wv, lw->off_wv, tc_v, m->smem_xb, m->smem_xf16, "%rd_s", m->smem_v, m->kv_dim, m->dim, 0);
            fprintf(out, "    bar.sync 0;\n");

            gguf_ptx_emit_ptr_add(out, "%rd_t0", "%rd_s", (uint64_t)m->smem_q);
            gguf_ptx_emit_ptr_add(out, "%rd_t1", "%rd_s", (uint64_t)m->smem_k);
            gguf_ptx_emit_ptr_add(out, "%rd_t2", "%rd_s", (uint64_t)m->smem_v);
            fprintf(out, "    mov.u32 %%r_t0, %u;\n", (unsigned int)layer_kv_elem);
            fprintf(out, "    { .param .u64 _a0, _a1, _a2, _a3, _a4; .param .u32 _a5, _a6; "
                         "st.param.u64 [_a0], %%rd_t0; st.param.u64 [_a1], %%rd_t1; st.param.u64 [_a2], %%rd_t2; "
                         "st.param.u64 [_a3], %%rd_kc; st.param.u64 [_a4], %%rd_vc; "
                         "st.param.u32 [_a5], %%r_pos; st.param.u32 [_a6], %%r_t0; "
                         "call.uni zcc_aot_rope_kv, (_a0, _a1, _a2, _a3, _a4, _a5, _a6); }\n");

            gguf_ptx_emit_ptr_add(out, "%rd_t0", "%rd_s", (uint64_t)m->smem_q);
            gguf_ptx_emit_ptr_add(out, "%rd_t1", "%rd_s", (uint64_t)m->smem_att);
            gguf_ptx_emit_ptr_add(out, "%rd_t2", "%rd_s", (uint64_t)m->smem_xb);
            fprintf(out, "    mov.u32 %%r_t0, %u;\n", (unsigned int)layer_kv_elem);
            fprintf(out, "    { .param .u64 _a0, _a1, _a2, _a3, _a4; .param .u32 _a5, _a6; "
                         "st.param.u64 [_a0], %%rd_t0; st.param.u64 [_a1], %%rd_kc; st.param.u64 [_a2], %%rd_vc; "
                         "st.param.u64 [_a3], %%rd_t1; st.param.u64 [_a4], %%rd_t2; "
                         "st.param.u32 [_a5], %%r_t0; st.param.u32 [_a6], %%r_pos; "
                         "call.uni zcc_aot_attn_gqa, (_a0, _a1, _a2, _a3, _a4, _a5, _a6); }\n");

            if (tc_o) gguf_ptx_emit_call_f32_to_f16(out, m->smem_xb, m->smem_xf16, m->dim);
            gguf_ptx_emit_call_linear(out, lw->t_wo, lw->off_wo, tc_o, m->smem_xb, m->smem_xf16, "%rd_s", m->smem_x, m->dim, m->dim, 1);
            fprintf(out, "    bar.sync 0;\n");

            gguf_ptx_emit_call_rmsnorm(out, m, m->smem_xb, m->smem_x, lw->off_ffn_norm);
            if (tc_g || tc_u) gguf_ptx_emit_call_f32_to_f16(out, m->smem_xb, m->smem_xf16, m->dim);
            gguf_ptx_emit_call_linear(out, lw->t_wgate, lw->off_wgate, tc_g, m->smem_xb, m->smem_xf16, "%rd_s", m->smem_hb, m->hidden_dim, m->dim, 0);
            gguf_ptx_emit_call_linear(out, lw->t_wup, lw->off_wup, tc_u, m->smem_xb, m->smem_xf16, "%rd_s", m->smem_hb2, m->hidden_dim, m->dim, 0);
            fprintf(out, "    bar.sync 0;\n");

            gguf_ptx_emit_ptr_add(out, "%rd_t0", "%rd_s", (uint64_t)m->smem_hb);
            gguf_ptx_emit_ptr_add(out, "%rd_t1", "%rd_s", (uint64_t)m->smem_hb2);
            fprintf(out, "    { .param .u64 _a0, _a1; st.param.u64 [_a0], %%rd_t0; st.param.u64 [_a1], %%rd_t1; "
                         "call.uni zcc_aot_swiglu, (_a0, _a1); }\n");

            if (tc_d) gguf_ptx_emit_call_f32_to_f16(out, m->smem_hb, m->smem_xf16, m->hidden_dim);
            gguf_ptx_emit_call_linear(out, lw->t_wdown, lw->off_wdown, tc_d, m->smem_hb, m->smem_xf16, "%rd_s", m->smem_x, m->dim, m->hidden_dim, 1);
            fprintf(out, "    bar.sync 0;\n\n");
        }

        gguf_ptx_emit_call_rmsnorm(out, m, m->smem_xb, m->smem_x, m->off_out_norm);
        {
            int tc_out = (m->t_wout == GGUF_PTX_F16 && (m->dim & 15U) == 0U);
            if (tc_out) gguf_ptx_emit_call_f32_to_f16(out, m->smem_xb, m->smem_xf16, m->dim);
            gguf_ptx_emit_call_linear(out, m->t_wout, m->off_wout, tc_out, m->smem_xb, m->smem_xf16, "%rd_log", 0U, m->vocab_size, m->dim, 0);
        }
        fprintf(out, "    ret;\n}\n");
    }

    int io_err = ferror(out);
    fclose(out);
    gguf_ptx_free_model(m);
    free(m);
    if (io_err != 0) {
        remove(out_ptx);
        return -1;
    }
    return 0;
}

int ptx_try_compile_gguf(const char *model_opt, const char *in_file, const char *out_file, const char *arch, int quiet) {
    const char *gguf = model_opt;
    if (!gguf && in_file) {
        size_t len = strlen(in_file);
        if (len > 5U && strcmp(in_file + len - 5U, ".gguf") == 0) gguf = in_file;
    }
    if (!gguf) return -1;
    int rc = ptx_compile_gguf_model(gguf, out_file, arch);
    if (rc == 0) {
        if (!quiet) printf("[OK] Model-specialized PTX emitted to %s\n", out_file);
        return 0;
    }
    return 1;
}
