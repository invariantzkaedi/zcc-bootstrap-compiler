#ifndef ZCC_RISCV_ELF_EMIT_C
#define ZCC_RISCV_ELF_EMIT_C

#include "elf_emit.h"
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint8_t *data;
    size_t size;
    size_t cap;
} RvBuf;

typedef struct {
    char name[128];
    uint8_t seg; /* 0=undef, 1=text, 2=data, 3=bss */
    uint64_t offset;
    uint64_t size;
    uint8_t is_global;
    uint8_t is_func;
    uint8_t defined;
} RvSym;

enum {
    RV_FIX_B12 = 1,
    RV_FIX_JAL20 = 2,
    RV_FIX_CALL_PAIR = 3,
    RV_FIX_LA_PAIR = 4,
    RV_FIX_DATA_ABS64 = 5
};

typedef struct {
    uint64_t offset;
    char target[128];
    int kind;
    int64_t addend;
    int line;
} RvFixup;

typedef struct {
    RvBuf text;
    RvBuf data;
    size_t bss_size;
    RvSym *syms;
    size_t sym_count;
    size_t sym_cap;
    RvFixup *fixups;
    size_t fixup_count;
    size_t fixup_cap;
    int cur_seg;
    int pass;
    int is_exec;
    int line_no;
    int err;
} RvAsmCtx;

static void rv_buf_init(RvBuf *b) { b->data = NULL; b->size = 0; b->cap = 0; }
static void rv_buf_free(RvBuf *b) { if (b->data) free(b->data); b->data = NULL; b->size = b->cap = 0; }

static int rv_buf_append(RvBuf *b, const void *ptr, size_t n) {
    if (n == 0) return 0;
    if (b->size + n > b->cap) {
        size_t nc = b->cap ? b->cap * 2 : 1024;
        while (nc < b->size + n) nc *= 2;
        uint8_t *nb = (uint8_t *)realloc(b->data, nc);
        if (!nb) return -1;
        b->data = nb;
        b->cap = nc;
    }
    memcpy(b->data + b->size, ptr, n);
    b->size += n;
    return 0;
}

static int rv_buf_u32(RvBuf *b, uint32_t w) {
    uint8_t le[4];
    le[0] = (uint8_t)(w & 0xFFU);
    le[1] = (uint8_t)((w >> 8) & 0xFFU);
    le[2] = (uint8_t)((w >> 16) & 0xFFU);
    le[3] = (uint8_t)((w >> 24) & 0xFFU);
    return rv_buf_append(b, le, 4);
}

static int rv_buf_zeros(RvBuf *b, size_t n) {
    static const uint8_t z[64] = {0};
    while (n > 0) {
        size_t c = n > sizeof(z) ? sizeof(z) : n;
        if (rv_buf_append(b, z, c) < 0) return -1;
        n -= c;
    }
    return 0;
}

static int rv_find_sym(const RvAsmCtx *ctx, const char *name) {
    for (size_t i = 0; i < ctx->sym_count; i++) {
        if (strcmp(ctx->syms[i].name, name) == 0) return (int)i;
    }
    return -1;
}

static int rv_get_or_add_sym(RvAsmCtx *ctx, const char *name) {
    int idx = rv_find_sym(ctx, name);
    if (idx >= 0) return idx;
    if (ctx->sym_count >= ctx->sym_cap) {
        size_t nc = ctx->sym_cap ? ctx->sym_cap * 2 : 128;
        RvSym *ns = (RvSym *)realloc(ctx->syms, nc * sizeof(RvSym));
        if (!ns) return -1;
        ctx->syms = ns;
        ctx->sym_cap = nc;
    }
    idx = (int)ctx->sym_count++;
    memset(&ctx->syms[idx], 0, sizeof(RvSym));
    strncpy(ctx->syms[idx].name, name, sizeof(ctx->syms[idx].name) - 1);
    return idx;
}

static int rv_add_fixup(RvAsmCtx *ctx, uint64_t off, const char *target, int kind, int64_t addend) {
    if (ctx->fixup_count >= ctx->fixup_cap) {
        size_t nc = ctx->fixup_cap ? ctx->fixup_cap * 2 : 128;
        RvFixup *nf = (RvFixup *)realloc(ctx->fixups, nc * sizeof(RvFixup));
        if (!nf) return -1;
        ctx->fixups = nf;
        ctx->fixup_cap = nc;
    }
    RvFixup *f = &ctx->fixups[ctx->fixup_count++];
    f->offset = off;
    memset(f->target, 0, sizeof(f->target));
    strncpy(f->target, target, sizeof(f->target) - 1);
    f->kind = kind;
    f->addend = addend;
    f->line = ctx->line_no;
    return 0;
}

static char *rv_trim(char *s) {
    while (*s && (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')) s++;
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t' || s[len - 1] == '\r' || s[len - 1] == '\n')) {
        s[--len] = '\0';
    }
    return s;
}

static void rv_strip_comment(char *s) {
    int in_str = 0;
    for (size_t i = 0; s[i]; i++) {
        if (s[i] == '"' && (i == 0 || s[i - 1] != '\\')) in_str = !in_str;
        if (!in_str) {
            if (s[i] == '#' || s[i] == ';') { s[i] = '\0'; break; }
            if (s[i] == '/' && s[i + 1] == '/') { s[i] = '\0'; break; }
        }
    }
}

static int rv_split_ops(char *s, char **ops, int max_ops) {
    int cnt = 0;
    int par = 0;
    char *p = rv_trim(s);
    if (!*p) return 0;
    ops[cnt++] = p;
    while (*p) {
        if (*p == '(') par++;
        else if (*p == ')' && par > 0) par--;
        else if (*p == ',' && par == 0) {
            *p = '\0';
            ops[cnt - 1] = rv_trim(ops[cnt - 1]);
            if (cnt < max_ops) ops[cnt++] = rv_trim(p + 1);
        }
        p++;
    }
    ops[cnt - 1] = rv_trim(ops[cnt - 1]);
    return cnt;
}

static int rv_parse_gpr(const char *s) {
    static const char *const abi[32] = {
        "zero","ra","sp","gp","tp","t0","t1","t2",
        "s0","s1","a0","a1","a2","a3","a4","a5",
        "a6","a7","s2","s3","s4","s5","s6","s7",
        "s8","s9","s10","s11","t3","t4","t5","t6"
    };
    char buf[32];
    size_t i = 0;
    while (s[i] && i + 1 < sizeof(buf)) { buf[i] = (char)tolower((unsigned char)s[i]); i++; }
    buf[i] = '\0';
    char *t = rv_trim(buf);
    if (strcmp(t, "fp") == 0) return 8;
    for (int r = 0; r < 32; r++) {
        if (strcmp(t, abi[r]) == 0) return r;
    }
    if (t[0] == 'x' && isdigit((unsigned char)t[1])) {
        char *ep = NULL;
        long r = strtol(t + 1, &ep, 10);
        if (ep && *ep == '\0' && r >= 0 && r <= 31) return (int)r;
    }
    return -1;
}

static int rv_parse_fpr(const char *s) {
    static const char *const fabi[32] = {
        "ft0","ft1","ft2","ft3","ft4","ft5","ft6","ft7",
        "fs0","fs1","fa0","fa1","fa2","fa3","fa4","fa5",
        "fa6","fa7","fs2","fs3","fs4","fs5","fs6","fs7",
        "fs8","fs9","fs10","fs11","ft8","ft9","ft10","ft11"
    };
    char buf[32];
    size_t i = 0;
    while (s[i] && i + 1 < sizeof(buf)) { buf[i] = (char)tolower((unsigned char)s[i]); i++; }
    buf[i] = '\0';
    char *t = rv_trim(buf);
    for (int r = 0; r < 32; r++) {
        if (strcmp(t, fabi[r]) == 0) return r;
    }
    if (t[0] == 'f' && isdigit((unsigned char)t[1])) {
        char *ep = NULL;
        long r = strtol(t + 1, &ep, 10);
        if (ep && *ep == '\0' && r >= 0 && r <= 31) return (int)r;
    }
    return -1;
}

static int rv_parse_imm(const char *s, int64_t *val) {
    char buf[64];
    strncpy(buf, s, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    char *t = rv_trim(buf);
    if (!*t) return 0;
    char *ep = NULL;
    if (t[0] == '-') {
        int64_t v = (int64_t)strtoll(t, &ep, 0);
        if (ep && *ep == '\0') { *val = v; return 1; }
    } else {
        uint64_t uv = strtoull(t, &ep, 0);
        if (ep && *ep == '\0') { *val = (int64_t)uv; return 1; }
    }
    return 0;
}

static int rv_parse_mem(char *s, int *base_reg, int64_t *imm) {
    char *t = rv_trim(s);
    char *lp = strchr(t, '(');
    char *rp = strchr(t, ')');
    if (!lp || !rp || rp < lp) return 0;
    *lp = '\0';
    *rp = '\0';
    char *off_s = rv_trim(t);
    char *reg_s = rv_trim(lp + 1);
    *imm = 0;
    if (*off_s && !rv_parse_imm(off_s, imm)) return 0;
    int r = rv_parse_gpr(reg_s);
    if (r < 0) return 0;
    *base_reg = r;
    return 1;
}

static uint32_t rv_enc_r(uint32_t funct7, uint32_t rs2, uint32_t rs1, uint32_t funct3, uint32_t rd, uint32_t op) {
    return (funct7 << 25) | ((rs2 & 31U) << 20) | ((rs1 & 31U) << 15) | ((funct3 & 7U) << 12) | ((rd & 31U) << 7) | (op & 0x7FU);
}

static uint32_t rv_enc_i(int64_t imm, uint32_t rs1, uint32_t funct3, uint32_t rd, uint32_t op) {
    return (((uint32_t)imm & 0xFFFU) << 20) | ((rs1 & 31U) << 15) | ((funct3 & 7U) << 12) | ((rd & 31U) << 7) | (op & 0x7FU);
}

static uint32_t rv_enc_s(int64_t imm, uint32_t rs2, uint32_t rs1, uint32_t funct3, uint32_t op) {
    uint32_t u = (uint32_t)imm & 0xFFFU;
    return (((u >> 5) & 0x7FU) << 25) | ((rs2 & 31U) << 20) | ((rs1 & 31U) << 15) | ((funct3 & 7U) << 12) | ((u & 0x1FU) << 7) | (op & 0x7FU);
}

static uint32_t rv_enc_b(int64_t off, uint32_t rs2, uint32_t rs1, uint32_t funct3) {
    uint32_t u = (uint32_t)off;
    uint32_t b12 = (u >> 12) & 1U;
    uint32_t b11 = (u >> 11) & 1U;
    uint32_t b10_5 = (u >> 5) & 0x3FU;
    uint32_t b4_1 = (u >> 1) & 0xFU;
    return (b12 << 31) | (b10_5 << 25) | ((rs2 & 31U) << 20) | ((rs1 & 31U) << 15) | ((funct3 & 7U) << 12) | (b4_1 << 8) | (b11 << 7) | 0x63U;
}

static uint32_t rv_enc_u(uint32_t hi20, uint32_t rd, uint32_t op) {
    return ((hi20 & 0xFFFFFU) << 12) | ((rd & 31U) << 7) | (op & 0x7FU);
}

static uint32_t rv_enc_j(int64_t off, uint32_t rd) {
    uint32_t u = (uint32_t)off;
    uint32_t b20 = (u >> 20) & 1U;
    uint32_t b10_1 = (u >> 1) & 0x3FFU;
    uint32_t b11 = (u >> 11) & 1U;
    uint32_t b19_12 = (u >> 12) & 0xFFU;
    return (b20 << 31) | (b10_1 << 21) | (b11 << 20) | (b19_12 << 12) | ((rd & 31U) << 7) | 0x6FU;
}

static int rv_li_seq(int rd, int64_t val, uint32_t *out, int max_out) {
    if (max_out <= 0) return 0;
    if (val >= -2048LL && val <= 2047LL) {
        out[0] = rv_enc_i(val, 0, 0, (uint32_t)rd, 0x13U); /* ADDI rd, x0, val */
        return 1;
    }
    if (val >= -0x80000000LL && val < 0x7FFFF800LL) {
        uint32_t hi20 = (uint32_t)(((val + 0x800LL) >> 12) & 0xFFFFFU);
        int32_t hi_sext = (int32_t)(hi20 << 12);
        int64_t lo12 = val - (int64_t)hi_sext;
        int n = 0;
        out[n++] = rv_enc_u(hi20, (uint32_t)rd, 0x37U); /* LUI */
        if (lo12 != 0 && n < max_out) {
            out[n++] = rv_enc_i(lo12, (uint32_t)rd, 0, (uint32_t)rd, 0x13U); /* ADDI */
        }
        return n;
    }
    int64_t lo12 = (int64_t)((int16_t)((val & 0xFFFLL) << 4) >> 4);
    int64_t upper = (val - lo12) >> 12;
    int n = rv_li_seq(rd, upper, out, max_out);
    if (n < max_out) {
        out[n++] = rv_enc_i(12, (uint32_t)rd, 1, (uint32_t)rd, 0x13U); /* SLLI rd, rd, 12 */
    }
    if (lo12 != 0 && n < max_out) {
        out[n++] = rv_enc_i(lo12, (uint32_t)rd, 0, (uint32_t)rd, 0x13U); /* ADDI */
    }
    return n;
}

static size_t rv_cur_off(const RvAsmCtx *ctx) {
    if (ctx->cur_seg == 1) return ctx->text.size;
    if (ctx->cur_seg == 2) return ctx->data.size;
    return ctx->bss_size;
}

static int rv_emit_insn(RvAsmCtx *ctx, uint32_t w) { return rv_buf_u32(&ctx->text, w); }

static int rv_emit_data(RvAsmCtx *ctx, const void *ptr, size_t n) {
    if (ctx->cur_seg == 1) return rv_buf_append(&ctx->text, ptr, n);
    if (ctx->cur_seg == 2) return rv_buf_append(&ctx->data, ptr, n);
    ctx->bss_size += n;
    return 0;
}

static int rv_emit_align(RvAsmCtx *ctx, size_t align) {
    if (align <= 1) return 0;
    size_t cur = rv_cur_off(ctx);
    size_t aligned = (cur + align - 1) & ~(align - 1);
    size_t pad = aligned - cur;
    if (pad == 0) return 0;
    if (ctx->cur_seg == 1) {
        while (pad >= 4) { rv_buf_u32(&ctx->text, 0x00000013U); pad -= 4; }
        return rv_buf_zeros(&ctx->text, pad);
    }
    if (ctx->cur_seg == 2) return rv_buf_zeros(&ctx->data, pad);
    ctx->bss_size += pad;
    return 0;
}

static int rv_fail(RvAsmCtx *ctx, const char *msg) {
    fprintf(stderr, "zcc: riscv64 direct asm error (line %d): %s\n", ctx->line_no, msg);
    ctx->err = 1;
    return -1;
}

static int rv_parse_string_esc(const char *s, RvBuf *out) {
    const char *p = strchr(s, '"');
    if (!p) return 0;
    p++;
    while (*p && *p != '"') {
        uint8_t ch = (uint8_t)*p++;
        if (ch == '\\' && *p) {
            char e = *p++;
            if (e == 'n') ch = '\n';
            else if (e == 'r') ch = '\r';
            else if (e == 't') ch = '\t';
            else if (e == '0') ch = '\0';
            else if (e == '\\') ch = '\\';
            else if (e == '"') ch = '"';
            else if (e == 'x') {
                unsigned int v = 0;
                if (isxdigit((unsigned char)p[0])) v = (unsigned int)strtoul(p, (char **)&p, 16);
                ch = (uint8_t)(v & 0xFFU);
            } else if (e >= '0' && e <= '7') {
                unsigned int v = (unsigned int)(e - '0');
                for (int k = 0; k < 2 && *p >= '0' && *p <= '7'; k++) v = (v << 3) | (unsigned int)(*p++ - '0');
                ch = (uint8_t)(v & 0xFFU);
            } else ch = (uint8_t)e;
        }
        rv_buf_append(out, &ch, 1);
    }
    return (*p == '"');
}

static int rv_process_line(RvAsmCtx *ctx, char *raw_line) {
    rv_strip_comment(raw_line);
    char *line = rv_trim(raw_line);
    if (!*line) return 0;

    while (1) {
        char *colon = strchr(line, ':');
        if (!colon) break;
        char *q = strchr(line, '"');
        if (q && q < colon) break;
        *colon = '\0';
        char *lname = rv_trim(line);
        if (ctx->pass == 1) {
            int idx = rv_get_or_add_sym(ctx, lname);
            if (idx < 0) return rv_fail(ctx, "OOM sym");
            ctx->syms[idx].seg = (uint8_t)ctx->cur_seg;
            ctx->syms[idx].offset = rv_cur_off(ctx);
            ctx->syms[idx].defined = 1;
        }
        line = rv_trim(colon + 1);
        if (!*line) return 0;
    }

    if (line[0] == '.') {
        char dir[64];
        size_t di = 0;
        while (line[di] && !isspace((unsigned char)line[di]) && di + 1 < sizeof(dir)) { dir[di] = line[di]; di++; }
        dir[di] = '\0';
        char *rest = rv_trim(line + di);

        if (strcmp(dir, ".text") == 0) { ctx->cur_seg = 1; return 0; }
        if (strcmp(dir, ".data") == 0) { ctx->cur_seg = 2; return 0; }
        if (strcmp(dir, ".bss") == 0) { ctx->cur_seg = 3; return 0; }
        if (strcmp(dir, ".section") == 0) {
            if (strstr(rest, ".text")) ctx->cur_seg = 1;
            else if (strstr(rest, ".bss")) ctx->cur_seg = 3;
            else if (strstr(rest, ".rodata") || strstr(rest, ".data")) ctx->cur_seg = 2;
            return 0;
        }
        if (strcmp(dir, ".global") == 0 || strcmp(dir, ".globl") == 0) {
            if (ctx->pass == 1 && *rest) {
                int idx = rv_get_or_add_sym(ctx, rest);
                if (idx >= 0) ctx->syms[idx].is_global = 1;
            }
            return 0;
        }
        if (strcmp(dir, ".local") == 0) {
            if (ctx->pass == 1 && *rest) {
                int idx = rv_get_or_add_sym(ctx, rest);
                if (idx >= 0) ctx->syms[idx].is_global = 0;
            }
            return 0;
        }
        if (strcmp(dir, ".type") == 0) {
            if (ctx->pass == 1 && strstr(rest, "function")) {
                char *cm = strchr(rest, ',');
                if (cm) {
                    *cm = '\0';
                    int idx = rv_get_or_add_sym(ctx, rv_trim(rest));
                    if (idx >= 0) ctx->syms[idx].is_func = 1;
                }
            }
            return 0;
        }
        if (strcmp(dir, ".option") == 0 || strcmp(dir, ".file") == 0 || strcmp(dir, ".loc") == 0 ||
            strcmp(dir, ".size") == 0 || strcmp(dir, ".ident") == 0 || strncmp(dir, ".cfi_", 5) == 0 ||
            strcmp(dir, ".attribute") == 0) {
            return 0;
        }
        if (strcmp(dir, ".p2align") == 0) {
            int64_t p = 0;
            if (!rv_parse_imm(rest, &p) || p < 0 || p > 12) p = 0;
            return rv_emit_align(ctx, (size_t)1 << (size_t)p);
        }
        if (strcmp(dir, ".align") == 0 || strcmp(dir, ".balign") == 0) {
            int64_t a = 0;
            if (rv_parse_imm(rest, &a) && a > 0) {
                size_t al = (strcmp(dir, ".align") == 0 && a <= 8) ? ((size_t)1 << (size_t)a) : (size_t)a;
                return rv_emit_align(ctx, al);
            }
            return 0;
        }
        if (strcmp(dir, ".zero") == 0 || strcmp(dir, ".space") == 0 || strcmp(dir, ".skip") == 0) {
            int64_t sz = 0;
            if (!rv_parse_imm(rest, &sz) || sz < 0) return rv_fail(ctx, "invalid .zero size");
            if (ctx->cur_seg == 1) return rv_buf_zeros(&ctx->text, (size_t)sz);
            if (ctx->cur_seg == 2) return rv_buf_zeros(&ctx->data, (size_t)sz);
            ctx->bss_size += (size_t)sz;
            return 0;
        }
        if (strcmp(dir, ".comm") == 0 || strcmp(dir, ".lcomm") == 0) {
            char *ops[4];
            int n = rv_split_ops(rest, ops, 4);
            if (n >= 2) {
                int64_t sz = 0, al = 8;
                rv_parse_imm(ops[1], &sz);
                if (n >= 3) rv_parse_imm(ops[2], &al);
                if (al < 1) al = 1;
                size_t cur_bss = (ctx->bss_size + (size_t)al - 1) & ~((size_t)al - 1);
                if (ctx->pass == 1) {
                    int idx = rv_get_or_add_sym(ctx, ops[0]);
                    if (idx >= 0) {
                        ctx->syms[idx].seg = 3;
                        ctx->syms[idx].offset = cur_bss;
                        ctx->syms[idx].size = (uint64_t)sz;
                        ctx->syms[idx].defined = 1;
                        if (strcmp(dir, ".comm") == 0) ctx->syms[idx].is_global = 1;
                    }
                }
                ctx->bss_size = cur_bss + (size_t)sz;
            }
            return 0;
        }
        if (strcmp(dir, ".asciz") == 0 || strcmp(dir, ".string") == 0 || strcmp(dir, ".ascii") == 0) {
            RvBuf sb;
            rv_buf_init(&sb);
            if (!rv_parse_string_esc(rest, &sb)) { rv_buf_free(&sb); return rv_fail(ctx, "bad string literal"); }
            if (strcmp(dir, ".ascii") != 0) { uint8_t z = 0; rv_buf_append(&sb, &z, 1); }
            int rc = rv_emit_data(ctx, sb.data, sb.size);
            rv_buf_free(&sb);
            return rc;
        }
        if (strcmp(dir, ".byte") == 0 || strcmp(dir, ".half") == 0 || strcmp(dir, ".hword") == 0 || strcmp(dir, ".short") == 0 ||
            strcmp(dir, ".word") == 0 || strcmp(dir, ".long") == 0 ||
            strcmp(dir, ".quad") == 0 || strcmp(dir, ".dword") == 0) {
            int width = 1;
            if (strcmp(dir, ".half") == 0 || strcmp(dir, ".hword") == 0 || strcmp(dir, ".short") == 0) width = 2;
            else if (strcmp(dir, ".word") == 0 || strcmp(dir, ".long") == 0) width = 4;
            else if (strcmp(dir, ".quad") == 0 || strcmp(dir, ".dword") == 0) width = 8;
            char *ops[32];
            int n = rv_split_ops(rest, ops, 32);
            for (int k = 0; k < n; k++) {
                int64_t iv = 0;
                uint8_t buf8[8] = {0};
                if (rv_parse_imm(ops[k], &iv)) {
                    uint64_t uv = (uint64_t)iv;
                    for (int b = 0; b < width; b++) buf8[b] = (uint8_t)((uv >> (b * 8)) & 0xFFU);
                    rv_emit_data(ctx, buf8, (size_t)width);
                } else {
                    if (width != 8) return rv_fail(ctx, "non-64bit symbolic reloc in data");
                    if (ctx->pass == 2 && ctx->cur_seg == 2) {
                        char tgt[128];
                        int64_t add = 0;
                        strncpy(tgt, ops[k], sizeof(tgt) - 1);
                        tgt[sizeof(tgt) - 1] = '\0';
                        char *pl = strchr(tgt, '+');
                        if (pl) { *pl = '\0'; rv_parse_imm(pl + 1, &add); }
                        rv_add_fixup(ctx, rv_cur_off(ctx), rv_trim(tgt), RV_FIX_DATA_ABS64, add);
                    }
                    rv_emit_data(ctx, buf8, 8);
                }
            }
            return 0;
        }
        return rv_fail(ctx, dir);
    }

    char mnem[32];
    size_t mi = 0;
    while (line[mi] && !isspace((unsigned char)line[mi]) && mi + 1 < sizeof(mnem)) {
        mnem[mi] = (char)tolower((unsigned char)line[mi]);
        mi++;
    }
    mnem[mi] = '\0';
    char *rest = rv_trim(line + mi);
    char *ops[6] = {0};
    int nops = rv_split_ops(rest, ops, 6);

    ctx->cur_seg = 1;

    if (strcmp(mnem, "ret") == 0) return rv_emit_insn(ctx, rv_enc_i(0, 1, 0, 0, 0x67U)); /* jalr x0, 0(ra) */
    if (strcmp(mnem, "nop") == 0) return rv_emit_insn(ctx, rv_enc_i(0, 0, 0, 0, 0x13U));
    if (strcmp(mnem, "ecall") == 0) return rv_emit_insn(ctx, 0x00000073U);
    if (strcmp(mnem, "ebreak") == 0) return rv_emit_insn(ctx, 0x00100073U);

    if (strcmp(mnem, "j") == 0 && nops == 1) {
        if (ctx->pass == 2) rv_add_fixup(ctx, ctx->text.size, ops[0], RV_FIX_JAL20, 0);
        else rv_get_or_add_sym(ctx, ops[0]);
        return rv_emit_insn(ctx, rv_enc_j(0, 0));
    }
    if (strcmp(mnem, "jal") == 0 && (nops == 1 || nops == 2)) {
        int rd = (nops == 2) ? rv_parse_gpr(ops[0]) : 1;
        const char *tgt = (nops == 2) ? ops[1] : ops[0];
        if (rd < 0) return rv_fail(ctx, "bad jal rd");
        if (ctx->pass == 2) rv_add_fixup(ctx, ctx->text.size, tgt, RV_FIX_JAL20, 0);
        else rv_get_or_add_sym(ctx, tgt);
        return rv_emit_insn(ctx, rv_enc_j(0, (uint32_t)rd));
    }
    if (strcmp(mnem, "jalr") == 0) {
        if (nops == 1) {
            int rs1 = rv_parse_gpr(ops[0]);
            if (rs1 < 0) return rv_fail(ctx, "bad jalr reg");
            return rv_emit_insn(ctx, rv_enc_i(0, (uint32_t)rs1, 0, 1, 0x67U));
        }
        if (nops == 2) {
            int rd = rv_parse_gpr(ops[0]);
            int rs1 = rv_parse_gpr(ops[1]);
            int64_t imm = 0;
            if (rd >= 0 && rs1 >= 0) return rv_emit_insn(ctx, rv_enc_i(0, (uint32_t)rs1, 0, (uint32_t)rd, 0x67U));
            if (rd >= 0 && rv_parse_mem(ops[1], &rs1, &imm)) return rv_emit_insn(ctx, rv_enc_i(imm, (uint32_t)rs1, 0, (uint32_t)rd, 0x67U));
            return rv_fail(ctx, "bad jalr");
        }
        if (nops == 3) {
            int rd = rv_parse_gpr(ops[0]);
            int rs1 = rv_parse_gpr(ops[1]);
            int64_t imm = 0;
            if (rd < 0 || rs1 < 0 || !rv_parse_imm(ops[2], &imm)) return rv_fail(ctx, "bad jalr 3-op");
            return rv_emit_insn(ctx, rv_enc_i(imm, (uint32_t)rs1, 0, (uint32_t)rd, 0x67U));
        }
    }
    if (strcmp(mnem, "call") == 0 && nops == 1) {
        if (ctx->pass == 2) rv_add_fixup(ctx, ctx->text.size, ops[0], RV_FIX_CALL_PAIR, 0);
        else rv_get_or_add_sym(ctx, ops[0]);
        rv_emit_insn(ctx, rv_enc_u(0, 1, 0x17U)); /* auipc ra, 0 */
        rv_emit_insn(ctx, rv_enc_i(0, 1, 0, 1, 0x67U)); /* jalr ra, 0(ra) */
        return 0;
    }
    if (strcmp(mnem, "la") == 0 && nops == 2) {
        int rd = rv_parse_gpr(ops[0]);
        if (rd < 0) return rv_fail(ctx, "bad la rd");
        if (ctx->pass == 2) rv_add_fixup(ctx, ctx->text.size, ops[1], RV_FIX_LA_PAIR, 0);
        else rv_get_or_add_sym(ctx, ops[1]);
        rv_emit_insn(ctx, rv_enc_u(0, (uint32_t)rd, 0x17U)); /* auipc rd, 0 */
        rv_emit_insn(ctx, rv_enc_i(0, (uint32_t)rd, 0, (uint32_t)rd, 0x13U)); /* addi rd, rd, 0 */
        return 0;
    }
    if (strcmp(mnem, "li") == 0 && nops == 2) {
        int rd = rv_parse_gpr(ops[0]);
        int64_t val = 0;
        if (rd < 0 || !rv_parse_imm(ops[1], &val)) return rv_fail(ctx, "bad li");
        uint32_t seq[12];
        int n = rv_li_seq(rd, val, seq, 12);
        for (int k = 0; k < n; k++) rv_emit_insn(ctx, seq[k]);
        return 0;
    }

    /* Pseudos 2-reg: mv, neg, negw, not, seqz, snez, sext.w, zext.b */
    if (nops == 2 && (strcmp(mnem, "mv") == 0 || strcmp(mnem, "neg") == 0 || strcmp(mnem, "negw") == 0 ||
                      strcmp(mnem, "not") == 0 || strcmp(mnem, "seqz") == 0 || strcmp(mnem, "snez") == 0 ||
                      strcmp(mnem, "sext.w") == 0 || strcmp(mnem, "zext.b") == 0)) {
        int rd = rv_parse_gpr(ops[0]), rs = rv_parse_gpr(ops[1]);
        if (rd < 0 || rs < 0) return rv_fail(ctx, "bad 2-reg pseudo");
        if (strcmp(mnem, "mv") == 0) return rv_emit_insn(ctx, rv_enc_i(0, (uint32_t)rs, 0, (uint32_t)rd, 0x13U));
        if (strcmp(mnem, "neg") == 0) return rv_emit_insn(ctx, rv_enc_r(0x20U, (uint32_t)rs, 0, 0, (uint32_t)rd, 0x33U));
        if (strcmp(mnem, "negw") == 0) return rv_emit_insn(ctx, rv_enc_r(0x20U, (uint32_t)rs, 0, 0, (uint32_t)rd, 0x3BU));
        if (strcmp(mnem, "not") == 0) return rv_emit_insn(ctx, rv_enc_i(-1, (uint32_t)rs, 4, (uint32_t)rd, 0x13U));
        if (strcmp(mnem, "seqz") == 0) return rv_emit_insn(ctx, rv_enc_i(1, (uint32_t)rs, 3, (uint32_t)rd, 0x13U));
        if (strcmp(mnem, "snez") == 0) return rv_emit_insn(ctx, rv_enc_r(0, (uint32_t)rs, 0, 3, (uint32_t)rd, 0x33U));
        if (strcmp(mnem, "sext.w") == 0) return rv_emit_insn(ctx, rv_enc_i(0, (uint32_t)rs, 0, (uint32_t)rd, 0x1BU));
        if (strcmp(mnem, "zext.b") == 0) return rv_emit_insn(ctx, rv_enc_i(255, (uint32_t)rs, 7, (uint32_t)rd, 0x13U));
    }

    /* Branches */
    if ((strcmp(mnem, "beqz") == 0 || strcmp(mnem, "bnez") == 0 ||
         strcmp(mnem, "bltz") == 0 || strcmp(mnem, "bgez") == 0 ||
         strcmp(mnem, "blez") == 0 || strcmp(mnem, "bgtz") == 0) && nops == 2) {
        int rs = rv_parse_gpr(ops[0]);
        if (rs < 0) return rv_fail(ctx, "bad bz reg");
        uint32_t rs1 = (uint32_t)rs, rs2 = 0, f3 = 0;
        if (strcmp(mnem, "beqz") == 0) f3 = 0;
        else if (strcmp(mnem, "bnez") == 0) f3 = 1;
        else if (strcmp(mnem, "bltz") == 0) f3 = 4;
        else if (strcmp(mnem, "bgez") == 0) f3 = 5;
        else if (strcmp(mnem, "blez") == 0) { rs1 = 0; rs2 = (uint32_t)rs; f3 = 5; }
        else if (strcmp(mnem, "bgtz") == 0) { rs1 = 0; rs2 = (uint32_t)rs; f3 = 4; }
        if (ctx->pass == 2) rv_add_fixup(ctx, ctx->text.size, ops[1], RV_FIX_B12, 0);
        else rv_get_or_add_sym(ctx, ops[1]);
        return rv_emit_insn(ctx, rv_enc_b(0, rs2, rs1, f3));
    }
    if ((strcmp(mnem, "beq") == 0 || strcmp(mnem, "bne") == 0 ||
         strcmp(mnem, "blt") == 0 || strcmp(mnem, "bge") == 0 ||
         strcmp(mnem, "bltu") == 0 || strcmp(mnem, "bgeu") == 0) && nops == 3) {
        int rs1 = rv_parse_gpr(ops[0]), rs2 = rv_parse_gpr(ops[1]);
        if (rs1 < 0 || rs2 < 0) return rv_fail(ctx, "bad branch reg");
        uint32_t f3 = 0;
        if (strcmp(mnem, "bne") == 0) f3 = 1;
        else if (strcmp(mnem, "blt") == 0) f3 = 4;
        else if (strcmp(mnem, "bge") == 0) f3 = 5;
        else if (strcmp(mnem, "bltu") == 0) f3 = 6;
        else if (strcmp(mnem, "bgeu") == 0) f3 = 7;
        if (ctx->pass == 2) rv_add_fixup(ctx, ctx->text.size, ops[2], RV_FIX_B12, 0);
        else rv_get_or_add_sym(ctx, ops[2]);
        return rv_emit_insn(ctx, rv_enc_b(0, (uint32_t)rs2, (uint32_t)rs1, f3));
    }

    /* Loads */
    if ((strcmp(mnem, "lb") == 0 || strcmp(mnem, "lh") == 0 || strcmp(mnem, "lw") == 0 || strcmp(mnem, "ld") == 0 ||
         strcmp(mnem, "lbu") == 0 || strcmp(mnem, "lhu") == 0 || strcmp(mnem, "lwu") == 0 || strcmp(mnem, "fld") == 0) && nops == 2) {
        int is_fp = (strcmp(mnem, "fld") == 0);
        int rd = is_fp ? rv_parse_fpr(ops[0]) : rv_parse_gpr(ops[0]);
        int base = -1;
        int64_t imm = 0;
        if (rd < 0 || !rv_parse_mem(ops[1], &base, &imm)) return rv_fail(ctx, "bad load");
        if (imm < -2048 || imm > 2047) return rv_fail(ctx, "load offset out of range");
        uint32_t f3 = 3;
        if (strcmp(mnem, "lb") == 0) f3 = 0;
        else if (strcmp(mnem, "lh") == 0) f3 = 1;
        else if (strcmp(mnem, "lw") == 0) f3 = 2;
        else if (strcmp(mnem, "lbu") == 0) f3 = 4;
        else if (strcmp(mnem, "lhu") == 0) f3 = 5;
        else if (strcmp(mnem, "lwu") == 0) f3 = 6;
        return rv_emit_insn(ctx, rv_enc_i(imm, (uint32_t)base, f3, (uint32_t)rd, is_fp ? 0x07U : 0x03U));
    }

    /* Stores */
    if ((strcmp(mnem, "sb") == 0 || strcmp(mnem, "sh") == 0 || strcmp(mnem, "sw") == 0 || strcmp(mnem, "sd") == 0 ||
         strcmp(mnem, "fsd") == 0) && nops == 2) {
        int is_fp = (strcmp(mnem, "fsd") == 0);
        int rs2 = is_fp ? rv_parse_fpr(ops[0]) : rv_parse_gpr(ops[0]);
        int base = -1;
        int64_t imm = 0;
        if (rs2 < 0 || !rv_parse_mem(ops[1], &base, &imm)) return rv_fail(ctx, "bad store");
        if (imm < -2048 || imm > 2047) return rv_fail(ctx, "store offset out of range");
        uint32_t f3 = 3;
        if (strcmp(mnem, "sb") == 0) f3 = 0;
        else if (strcmp(mnem, "sh") == 0) f3 = 1;
        else if (strcmp(mnem, "sw") == 0) f3 = 2;
        return rv_emit_insn(ctx, rv_enc_s(imm, (uint32_t)rs2, (uint32_t)base, f3, is_fp ? 0x27U : 0x23U));
    }

    /* I-type ALU and shift-immediates */
    if (nops == 3 && (strcmp(mnem, "addi") == 0 || strcmp(mnem, "addiw") == 0 || strcmp(mnem, "slti") == 0 ||
                      strcmp(mnem, "sltiu") == 0 || strcmp(mnem, "xori") == 0 || strcmp(mnem, "ori") == 0 ||
                      strcmp(mnem, "andi") == 0 || strcmp(mnem, "slli") == 0 || strcmp(mnem, "srli") == 0 ||
                      strcmp(mnem, "srai") == 0 || strcmp(mnem, "slliw") == 0 || strcmp(mnem, "srliw") == 0 ||
                      strcmp(mnem, "sraiw") == 0)) {
        int rd = rv_parse_gpr(ops[0]), rs1 = rv_parse_gpr(ops[1]);
        int64_t imm = 0;
        if (rd < 0 || rs1 < 0 || !rv_parse_imm(ops[2], &imm)) return rv_fail(ctx, "bad itype");
        if (strcmp(mnem, "slli") == 0) return rv_emit_insn(ctx, rv_enc_i(imm & 63, (uint32_t)rs1, 1, (uint32_t)rd, 0x13U));
        if (strcmp(mnem, "srli") == 0) return rv_emit_insn(ctx, rv_enc_i(imm & 63, (uint32_t)rs1, 5, (uint32_t)rd, 0x13U));
        if (strcmp(mnem, "srai") == 0) return rv_emit_insn(ctx, rv_enc_i((imm & 63) | 0x400, (uint32_t)rs1, 5, (uint32_t)rd, 0x13U));
        if (strcmp(mnem, "slliw") == 0) return rv_emit_insn(ctx, rv_enc_i(imm & 31, (uint32_t)rs1, 1, (uint32_t)rd, 0x1BU));
        if (strcmp(mnem, "srliw") == 0) return rv_emit_insn(ctx, rv_enc_i(imm & 31, (uint32_t)rs1, 5, (uint32_t)rd, 0x1BU));
        if (strcmp(mnem, "sraiw") == 0) return rv_emit_insn(ctx, rv_enc_i((imm & 31) | 0x400, (uint32_t)rs1, 5, (uint32_t)rd, 0x1BU));
        if (imm < -2048 || imm > 2047) return rv_fail(ctx, "itype imm out of range");
        if (strcmp(mnem, "addiw") == 0) return rv_emit_insn(ctx, rv_enc_i(imm, (uint32_t)rs1, 0, (uint32_t)rd, 0x1BU));
        uint32_t f3 = 0;
        if (strcmp(mnem, "slti") == 0) f3 = 2;
        else if (strcmp(mnem, "sltiu") == 0) f3 = 3;
        else if (strcmp(mnem, "xori") == 0) f3 = 4;
        else if (strcmp(mnem, "ori") == 0) f3 = 6;
        else if (strcmp(mnem, "andi") == 0) f3 = 7;
        return rv_emit_insn(ctx, rv_enc_i(imm, (uint32_t)rs1, f3, (uint32_t)rd, 0x13U));
    }

    /* R-type 64/32 + M extension */
    if (nops == 3) {
        int rd = rv_parse_gpr(ops[0]), rs1 = rv_parse_gpr(ops[1]), rs2 = rv_parse_gpr(ops[2]);
        if (rd >= 0 && rs1 >= 0 && rs2 >= 0) {
            if (strcmp(mnem, "add") == 0) return rv_emit_insn(ctx, rv_enc_r(0x00U, rs2, rs1, 0, rd, 0x33U));
            if (strcmp(mnem, "sub") == 0) return rv_emit_insn(ctx, rv_enc_r(0x20U, rs2, rs1, 0, rd, 0x33U));
            if (strcmp(mnem, "sll") == 0) return rv_emit_insn(ctx, rv_enc_r(0x00U, rs2, rs1, 1, rd, 0x33U));
            if (strcmp(mnem, "slt") == 0) return rv_emit_insn(ctx, rv_enc_r(0x00U, rs2, rs1, 2, rd, 0x33U));
            if (strcmp(mnem, "sltu") == 0) return rv_emit_insn(ctx, rv_enc_r(0x00U, rs2, rs1, 3, rd, 0x33U));
            if (strcmp(mnem, "xor") == 0) return rv_emit_insn(ctx, rv_enc_r(0x00U, rs2, rs1, 4, rd, 0x33U));
            if (strcmp(mnem, "srl") == 0) return rv_emit_insn(ctx, rv_enc_r(0x00U, rs2, rs1, 5, rd, 0x33U));
            if (strcmp(mnem, "sra") == 0) return rv_emit_insn(ctx, rv_enc_r(0x20U, rs2, rs1, 5, rd, 0x33U));
            if (strcmp(mnem, "or") == 0) return rv_emit_insn(ctx, rv_enc_r(0x00U, rs2, rs1, 6, rd, 0x33U));
            if (strcmp(mnem, "and") == 0) return rv_emit_insn(ctx, rv_enc_r(0x00U, rs2, rs1, 7, rd, 0x33U));
            if (strcmp(mnem, "mul") == 0) return rv_emit_insn(ctx, rv_enc_r(0x01U, rs2, rs1, 0, rd, 0x33U));
            if (strcmp(mnem, "mulh") == 0) return rv_emit_insn(ctx, rv_enc_r(0x01U, rs2, rs1, 1, rd, 0x33U));
            if (strcmp(mnem, "mulhu") == 0) return rv_emit_insn(ctx, rv_enc_r(0x01U, rs2, rs1, 3, rd, 0x33U));
            if (strcmp(mnem, "div") == 0) return rv_emit_insn(ctx, rv_enc_r(0x01U, rs2, rs1, 4, rd, 0x33U));
            if (strcmp(mnem, "divu") == 0) return rv_emit_insn(ctx, rv_enc_r(0x01U, rs2, rs1, 5, rd, 0x33U));
            if (strcmp(mnem, "rem") == 0) return rv_emit_insn(ctx, rv_enc_r(0x01U, rs2, rs1, 6, rd, 0x33U));
            if (strcmp(mnem, "remu") == 0) return rv_emit_insn(ctx, rv_enc_r(0x01U, rs2, rs1, 7, rd, 0x33U));
            if (strcmp(mnem, "addw") == 0) return rv_emit_insn(ctx, rv_enc_r(0x00U, rs2, rs1, 0, rd, 0x3BU));
            if (strcmp(mnem, "subw") == 0) return rv_emit_insn(ctx, rv_enc_r(0x20U, rs2, rs1, 0, rd, 0x3BU));
            if (strcmp(mnem, "sllw") == 0) return rv_emit_insn(ctx, rv_enc_r(0x00U, rs2, rs1, 1, rd, 0x3BU));
            if (strcmp(mnem, "srlw") == 0) return rv_emit_insn(ctx, rv_enc_r(0x00U, rs2, rs1, 5, rd, 0x3BU));
            if (strcmp(mnem, "sraw") == 0) return rv_emit_insn(ctx, rv_enc_r(0x20U, rs2, rs1, 5, rd, 0x3BU));
            if (strcmp(mnem, "mulw") == 0) return rv_emit_insn(ctx, rv_enc_r(0x01U, rs2, rs1, 0, rd, 0x3BU));
            if (strcmp(mnem, "divw") == 0) return rv_emit_insn(ctx, rv_enc_r(0x01U, rs2, rs1, 4, rd, 0x3BU));
            if (strcmp(mnem, "divuw") == 0) return rv_emit_insn(ctx, rv_enc_r(0x01U, rs2, rs1, 5, rd, 0x3BU));
            if (strcmp(mnem, "remw") == 0) return rv_emit_insn(ctx, rv_enc_r(0x01U, rs2, rs1, 6, rd, 0x3BU));
            if (strcmp(mnem, "remuw") == 0) return rv_emit_insn(ctx, rv_enc_r(0x01U, rs2, rs1, 7, rd, 0x3BU));
        }
        /* FP arithmetic */
        int frd = rv_parse_fpr(ops[0]), frs1 = rv_parse_fpr(ops[1]), frs2 = rv_parse_fpr(ops[2]);
        if (frd >= 0 && frs1 >= 0 && frs2 >= 0) {
            if (strcmp(mnem, "fadd.d") == 0) return rv_emit_insn(ctx, rv_enc_r(0x01U, frs2, frs1, 7, frd, 0x53U));
            if (strcmp(mnem, "fsub.d") == 0) return rv_emit_insn(ctx, rv_enc_r(0x05U, frs2, frs1, 7, frd, 0x53U));
            if (strcmp(mnem, "fmul.d") == 0) return rv_emit_insn(ctx, rv_enc_r(0x09U, frs2, frs1, 7, frd, 0x53U));
            if (strcmp(mnem, "fdiv.d") == 0) return rv_emit_insn(ctx, rv_enc_r(0x0DU, frs2, frs1, 7, frd, 0x53U));
        }
    }
    if (nops == 2 && (strcmp(mnem, "fmv.d.x") == 0 || strcmp(mnem, "fmv.x.d") == 0)) {
        if (strcmp(mnem, "fmv.d.x") == 0) {
            int frd = rv_parse_fpr(ops[0]), rs1 = rv_parse_gpr(ops[1]);
            if (frd >= 0 && rs1 >= 0) return rv_emit_insn(ctx, rv_enc_r(0x79U, 0, rs1, 0, frd, 0x53U));
        } else {
            int rd = rv_parse_gpr(ops[0]), frs1 = rv_parse_fpr(ops[1]);
            if (rd >= 0 && frs1 >= 0) return rv_emit_insn(ctx, rv_enc_r(0x71U, 0, frs1, 0, rd, 0x53U));
        }
    }

    return rv_fail(ctx, mnem);
}

static uint32_t rv_read_u32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static void rv_write_u32(uint8_t *p, uint32_t w) {
    p[0] = (uint8_t)(w & 0xFFU);
    p[1] = (uint8_t)((w >> 8) & 0xFFU);
    p[2] = (uint8_t)((w >> 16) & 0xFFU);
    p[3] = (uint8_t)((w >> 24) & 0xFFU);
}

static int rv_run_passes(RvAsmCtx *ctx, const char *asm_buf, size_t asm_len, const char *extra_rt) {
    for (int pass = 1; pass <= 2; pass++) {
        ctx->pass = pass;
        if (pass == 2) ctx->fixup_count = 0;
        rv_buf_free(&ctx->text);
        rv_buf_free(&ctx->data);
        rv_buf_init(&ctx->text);
        rv_buf_init(&ctx->data);
        ctx->bss_size = 0;
        ctx->cur_seg = 1;
        ctx->line_no = 0;

        char *buf_copy = (char *)malloc(asm_len + 1);
        if (!buf_copy) return -1;
        memcpy(buf_copy, asm_buf, asm_len);
        buf_copy[asm_len] = '\0';
        char *cur = buf_copy;
        while (*cur && !ctx->err) {
            ctx->line_no++;
            char *nl = strchr(cur, '\n');
            if (nl) *nl = '\0';
            if (rv_process_line(ctx, cur) < 0) break;
            if (!nl) break;
            cur = nl + 1;
        }
        free(buf_copy);
        if (ctx->err) return -1;

        if (extra_rt && *extra_rt) {
            size_t rt_len = strlen(extra_rt);
            char *rt_copy = (char *)malloc(rt_len + 1);
            if (!rt_copy) return -1;
            memcpy(rt_copy, extra_rt, rt_len + 1);
            char *rp = rt_copy;
            while (*rp && !ctx->err) {
                ctx->line_no++;
                char *nl = strchr(rp, '\n');
                if (nl) *nl = '\0';
                if (rv_process_line(ctx, rp) < 0) break;
                if (!nl) break;
                rp = nl + 1;
            }
            free(rt_copy);
            if (ctx->err) return -1;
        }
    }
    return 0;
}

static int rv_assemble_internal(const char *asm_buf, size_t asm_len, const char *out_filename, int is_exec) {
    if (!asm_buf || !out_filename) return -1;
    RvAsmCtx ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.is_exec = is_exec;
    ctx.cur_seg = 1;

    if (rv_run_passes(&ctx, asm_buf, asm_len, NULL) < 0) {
        rv_buf_free(&ctx.text); rv_buf_free(&ctx.data);
        free(ctx.syms); free(ctx.fixups);
        return -1;
    }

    if (is_exec) {
        char rt_buf[4096];
        rt_buf[0] = '\0';
        int has_start = (rv_find_sym(&ctx, "_start") >= 0 && ctx.syms[rv_find_sym(&ctx, "_start")].defined);
        int has_main = (rv_find_sym(&ctx, "main") >= 0 && ctx.syms[rv_find_sym(&ctx, "main")].defined);
        if (!has_start && has_main) {
            strcat(rt_buf,
                   "\n.text\n.globl _start\n_start:\n"
                   "    ld a0, 0(sp)\n"
                   "    addi a1, sp, 8\n"
                   "    call main\n"
                   "    li a7, 93\n"
                   "    ecall\n");
        }
        int need_exit = (rv_find_sym(&ctx, "exit") >= 0 && !ctx.syms[rv_find_sym(&ctx, "exit")].defined);
        int need_write = (rv_find_sym(&ctx, "write") >= 0 && !ctx.syms[rv_find_sym(&ctx, "write")].defined);
        int need_putchar = (rv_find_sym(&ctx, "putchar") >= 0 && !ctx.syms[rv_find_sym(&ctx, "putchar")].defined);
        int need_puts = (rv_find_sym(&ctx, "puts") >= 0 && !ctx.syms[rv_find_sym(&ctx, "puts")].defined);
        if (need_exit) {
            strcat(rt_buf, "\n.globl exit\nexit:\n    li a7, 93\n    ecall\n");
        }
        if (need_write) {
            strcat(rt_buf, "\n.globl write\nwrite:\n    li a7, 64\n    ecall\n    ret\n");
        }
        if (need_putchar) {
            strcat(rt_buf,
                   "\n.globl putchar\nputchar:\n"
                   "    addi sp, sp, -16\n"
                   "    sb a0, 0(sp)\n"
                   "    li a0, 1\n"
                   "    mv a1, sp\n"
                   "    li a2, 1\n"
                   "    li a7, 64\n"
                   "    ecall\n"
                   "    lbu a0, 0(sp)\n"
                   "    addi sp, sp, 16\n"
                   "    ret\n");
        }
        if (need_puts) {
            strcat(rt_buf,
                   "\n.globl puts\nputs:\n"
                   "    mv t0, a0\n"
                   "    li a2, 0\n"
                   ".L_zcc_rv_puts_len:\n"
                   "    add t1, t0, a2\n"
                   "    lbu t2, 0(t1)\n"
                   "    beqz t2, .L_zcc_rv_puts_wr\n"
                   "    addi a2, a2, 1\n"
                   "    j .L_zcc_rv_puts_len\n"
                   ".L_zcc_rv_puts_wr:\n"
                   "    mv a1, t0\n"
                   "    li a0, 1\n"
                   "    li a7, 64\n"
                   "    ecall\n"
                   "    addi sp, sp, -16\n"
                   "    li t2, 10\n"
                   "    sb t2, 0(sp)\n"
                   "    li a0, 1\n"
                   "    mv a1, sp\n"
                   "    li a2, 1\n"
                   "    li a7, 64\n"
                   "    ecall\n"
                   "    addi sp, sp, 16\n"
                   "    li a0, 0\n"
                   "    ret\n");
        }
        if (rt_buf[0]) {
            rv_buf_free(&ctx.text); rv_buf_free(&ctx.data);
            free(ctx.syms); free(ctx.fixups);
            memset(&ctx, 0, sizeof(ctx));
            ctx.is_exec = 1;
            ctx.cur_seg = 1;
            if (rv_run_passes(&ctx, asm_buf, asm_len, rt_buf) < 0) {
                rv_buf_free(&ctx.text); rv_buf_free(&ctx.data);
                free(ctx.syms); free(ctx.fixups);
                return -1;
            }
        }
    }

    if (is_exec) {
        uint64_t text_vaddr = 0x401000ULL;
        uint64_t data_vaddr = ((text_vaddr + ctx.text.size + 0xFFFULL) & ~0xFFFULL);
        if (data_vaddr == text_vaddr) data_vaddr += 0x1000ULL;
        uint64_t bss_vaddr = data_vaddr + ctx.data.size;

        for (size_t i = 0; i < ctx.fixup_count; i++) {
            RvFixup *f = &ctx.fixups[i];
            int si = rv_find_sym(&ctx, f->target);
            if (si < 0 || !ctx.syms[si].defined) {
                fprintf(stderr, "zcc: riscv64 undefined symbol '%s' (line %d)\n", f->target, f->line);
                rv_buf_free(&ctx.text); rv_buf_free(&ctx.data);
                free(ctx.syms); free(ctx.fixups);
                return -1;
            }
            uint64_t target_va = 0;
            if (ctx.syms[si].seg == 1) target_va = text_vaddr + ctx.syms[si].offset;
            else if (ctx.syms[si].seg == 2) target_va = data_vaddr + ctx.syms[si].offset;
            else target_va = bss_vaddr + ctx.syms[si].offset;
            target_va = (uint64_t)((int64_t)target_va + f->addend);

            if (f->kind == RV_FIX_DATA_ABS64) {
                uint8_t *p = ctx.data.data + f->offset;
                for (int b = 0; b < 8; b++) p[b] = (uint8_t)((target_va >> (b * 8)) & 0xFFU);
                continue;
            }
            uint64_t ins_va = text_vaddr + f->offset;
            int64_t delta = (int64_t)target_va - (int64_t)ins_va;
            uint32_t w = rv_read_u32(ctx.text.data + f->offset);
            if (f->kind == RV_FIX_B12) {
                if (delta < -4096 || delta >= 4096) {
                    fprintf(stderr, "zcc: riscv64 branch out of range to '%s'\n", f->target);
                    rv_buf_free(&ctx.text); rv_buf_free(&ctx.data);
                    free(ctx.syms); free(ctx.fixups);
                    return -1;
                }
                uint32_t rs1 = (w >> 15) & 31U, rs2 = (w >> 20) & 31U, f3 = (w >> 12) & 7U;
                rv_write_u32(ctx.text.data + f->offset, rv_enc_b(delta, rs2, rs1, f3));
            } else if (f->kind == RV_FIX_JAL20) {
                if (delta < -(1 << 20) || delta >= (1 << 20)) {
                    fprintf(stderr, "zcc: riscv64 jal out of range to '%s'\n", f->target);
                    rv_buf_free(&ctx.text); rv_buf_free(&ctx.data);
                    free(ctx.syms); free(ctx.fixups);
                    return -1;
                }
                uint32_t rd = (w >> 7) & 31U;
                rv_write_u32(ctx.text.data + f->offset, rv_enc_j(delta, rd));
            } else if (f->kind == RV_FIX_CALL_PAIR || f->kind == RV_FIX_LA_PAIR) {
                uint32_t hi20 = (uint32_t)(((delta + 0x800LL) >> 12) & 0xFFFFFU);
                int32_t hi_sext = (int32_t)(hi20 << 12);
                int64_t lo12 = delta - (int64_t)hi_sext;
                uint32_t rd = (w >> 7) & 31U;
                rv_write_u32(ctx.text.data + f->offset, rv_enc_u(hi20, rd, 0x17U));
                uint32_t w2 = rv_read_u32(ctx.text.data + f->offset + 4);
                rv_write_u32(ctx.text.data + f->offset + 4, (w2 & 0x000FFFFFU) | (((uint32_t)lo12 & 0xFFFU) << 20));
            }
        }

        int s_idx = rv_find_sym(&ctx, "_start");
        if (s_idx < 0) s_idx = rv_find_sym(&ctx, "main");
        if (s_idx < 0 || !ctx.syms[s_idx].defined) {
            fprintf(stderr, "zcc: riscv64 ET_EXEC missing _start/main entry point\n");
            rv_buf_free(&ctx.text); rv_buf_free(&ctx.data);
            free(ctx.syms); free(ctx.fixups);
            return -1;
        }
        uint64_t entry_va = text_vaddr + ctx.syms[s_idx].offset;
        int rc = elf_emit_exec_arch(out_filename, EM_RISCV, EF_RISCV_FLOAT_ABI_DOUBLE,
                                    entry_va, text_vaddr, ctx.text.data, ctx.text.size,
                                    data_vaddr, ctx.data.data, ctx.data.size, ctx.bss_size);
        rv_buf_free(&ctx.text); rv_buf_free(&ctx.data);
        free(ctx.syms); free(ctx.fixups);
        return rc;
    }

    zcc_elf_rela_t *relas = ctx.fixup_count ? (zcc_elf_rela_t *)calloc(ctx.fixup_count * 2, sizeof(zcc_elf_rela_t)) : NULL;
    size_t rela_n = 0;
    for (size_t i = 0; i < ctx.fixup_count; i++) {
        RvFixup *f = &ctx.fixups[i];
        int si = rv_find_sym(&ctx, f->target);
        if (si >= 0 && ctx.syms[si].defined && ctx.syms[si].seg == 1 &&
            (f->kind == RV_FIX_B12 || f->kind == RV_FIX_JAL20 || f->kind == RV_FIX_CALL_PAIR)) {
            int64_t delta = (int64_t)ctx.syms[si].offset - (int64_t)f->offset;
            uint32_t w = rv_read_u32(ctx.text.data + f->offset);
            if (f->kind == RV_FIX_B12) {
                uint32_t rs1 = (w >> 15) & 31U, rs2 = (w >> 20) & 31U, f3 = (w >> 12) & 7U;
                rv_write_u32(ctx.text.data + f->offset, rv_enc_b(delta, rs2, rs1, f3));
            } else if (f->kind == RV_FIX_JAL20) {
                uint32_t rd = (w >> 7) & 31U;
                rv_write_u32(ctx.text.data + f->offset, rv_enc_j(delta, rd));
            } else {
                uint32_t hi20 = (uint32_t)(((delta + 0x800LL) >> 12) & 0xFFFFFU);
                int64_t lo12 = delta - (int64_t)(int32_t)(hi20 << 12);
                rv_write_u32(ctx.text.data + f->offset, rv_enc_u(hi20, 1, 0x17U));
                uint32_t w2 = rv_read_u32(ctx.text.data + f->offset + 4);
                rv_write_u32(ctx.text.data + f->offset + 4, (w2 & 0x000FFFFFU) | (((uint32_t)lo12 & 0xFFFU) << 20));
            }
        } else if (f->kind != RV_FIX_DATA_ABS64) {
            if (f->kind == RV_FIX_CALL_PAIR) {
                relas[rela_n].offset = f->offset;
                relas[rela_n].sym_name = f->target;
                relas[rela_n].type = R_RISCV_CALL_PLT;
                relas[rela_n].addend = f->addend;
                rela_n++;
            } else if (f->kind == RV_FIX_LA_PAIR) {
                relas[rela_n].offset = f->offset;
                relas[rela_n].sym_name = f->target;
                relas[rela_n].type = R_RISCV_PCREL_HI20;
                relas[rela_n].addend = f->addend;
                rela_n++;
                relas[rela_n].offset = f->offset + 4;
                relas[rela_n].sym_name = f->target;
                relas[rela_n].type = R_RISCV_PCREL_LO12_I;
                relas[rela_n].addend = f->addend;
                rela_n++;
            } else {
                relas[rela_n].offset = f->offset;
                relas[rela_n].sym_name = f->target;
                relas[rela_n].type = (f->kind == RV_FIX_B12) ? R_RISCV_BRANCH : R_RISCV_JAL;
                relas[rela_n].addend = f->addend;
                rela_n++;
            }
        }
    }

    zcc_elf_sym_t *esyms = ctx.sym_count ? (zcc_elf_sym_t *)calloc(ctx.sym_count, sizeof(zcc_elf_sym_t)) : NULL;
    for (size_t i = 0; i < ctx.sym_count; i++) {
        esyms[i].name = ctx.syms[i].name;
        esyms[i].value = ctx.syms[i].offset;
        esyms[i].size = ctx.syms[i].size;
        esyms[i].binding = ctx.syms[i].is_global ? STB_GLOBAL : STB_LOCAL;
        if (!ctx.syms[i].defined) {
            esyms[i].type = STT_NOTYPE;
            esyms[i].shndx = SHN_UNDEF;
        } else if (ctx.syms[i].seg == 1) {
            esyms[i].type = ctx.syms[i].is_func ? STT_FUNC : STT_NOTYPE;
            esyms[i].shndx = 1;
        } else if (ctx.syms[i].seg == 2) {
            esyms[i].type = STT_OBJECT;
            esyms[i].shndx = 2;
        } else {
            esyms[i].type = STT_OBJECT;
            esyms[i].shndx = 3;
        }
    }

    int rc = elf_emit_obj_full(out_filename, EM_RISCV, EF_RISCV_FLOAT_ABI_DOUBLE,
                               ctx.text.data, ctx.text.size,
                               ctx.data.data, ctx.data.size, ctx.bss_size,
                               esyms, ctx.sym_count, relas, rela_n);
    if (esyms) free(esyms);
    if (relas) free(relas);
    rv_buf_free(&ctx.text); rv_buf_free(&ctx.data);
    free(ctx.syms); free(ctx.fixups);
    return rc;
}

int riscv_assemble_to_elf_obj(const char *asm_buf, size_t asm_len, const char *out_o) {
    return rv_assemble_internal(asm_buf, asm_len, out_o, 0);
}

int riscv_assemble_to_elf_exec(const char *asm_buf, size_t asm_len, const char *out_exe) {
    return rv_assemble_internal(asm_buf, asm_len, out_exe, 1);
}

#endif /* ZCC_RISCV_ELF_EMIT_C */
