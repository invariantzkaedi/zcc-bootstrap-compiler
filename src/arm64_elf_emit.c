#ifndef ZCC_ARM64_ELF_EMIT_C
#define ZCC_ARM64_ELF_EMIT_C

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
} A64Buf;

typedef struct {
    char name[128];
    uint8_t seg; /* 0=undef, 1=text, 2=data, 3=bss */
    uint64_t offset;
    uint64_t size;
    uint8_t is_global;
    uint8_t is_func;
    uint8_t defined;
} A64Sym;

enum {
    A64_FIX_B26 = 1,
    A64_FIX_COND19 = 2,
    A64_FIX_BL26 = 3,
    A64_FIX_ADRP_ADD_PAIR = 4,
    A64_FIX_DATA_ABS64 = 5
};

typedef struct {
    uint64_t offset;
    char target[128];
    int kind;
    int64_t addend;
    int line;
} A64Fixup;

typedef struct {
    A64Buf text;
    A64Buf data;
    size_t bss_size;
    A64Sym *syms;
    size_t sym_count;
    size_t sym_cap;
    A64Fixup *fixups;
    size_t fixup_count;
    size_t fixup_cap;
    int cur_seg; /* 1=text, 2=data, 3=bss */
    int pass;    /* 1 or 2 */
    int is_exec;
    int line_no;
    int err;
} A64AsmCtx;

static void a64_buf_init(A64Buf *b) {
    b->data = NULL;
    b->size = 0;
    b->cap = 0;
}

static void a64_buf_free(A64Buf *b) {
    if (b->data) free(b->data);
    b->data = NULL;
    b->size = b->cap = 0;
}

static int a64_buf_append(A64Buf *b, const void *ptr, size_t n) {
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

static int a64_buf_u32(A64Buf *b, uint32_t w) {
    uint8_t le[4];
    le[0] = (uint8_t)(w & 0xFFU);
    le[1] = (uint8_t)((w >> 8) & 0xFFU);
    le[2] = (uint8_t)((w >> 16) & 0xFFU);
    le[3] = (uint8_t)((w >> 24) & 0xFFU);
    return a64_buf_append(b, le, 4);
}

static int a64_buf_zeros(A64Buf *b, size_t n) {
    static const uint8_t z[64] = {0};
    while (n > 0) {
        size_t c = n > sizeof(z) ? sizeof(z) : n;
        if (a64_buf_append(b, z, c) < 0) return -1;
        n -= c;
    }
    return 0;
}

static int a64_find_sym(const A64AsmCtx *ctx, const char *name) {
    size_t i;
    for (i = 0; i < ctx->sym_count; i++) {
        if (strcmp(ctx->syms[i].name, name) == 0) return (int)i;
    }
    return -1;
}

static int a64_get_or_add_sym(A64AsmCtx *ctx, const char *name) {
    int idx = a64_find_sym(ctx, name);
    if (idx >= 0) return idx;
    if (ctx->sym_count >= ctx->sym_cap) {
        size_t nc = ctx->sym_cap ? ctx->sym_cap * 2 : 128;
        A64Sym *ns = (A64Sym *)realloc(ctx->syms, nc * sizeof(A64Sym));
        if (!ns) return -1;
        ctx->syms = ns;
        ctx->sym_cap = nc;
    }
    idx = (int)ctx->sym_count++;
    memset(&ctx->syms[idx], 0, sizeof(A64Sym));
    strncpy(ctx->syms[idx].name, name, sizeof(ctx->syms[idx].name) - 1);
    return idx;
}

static int a64_add_fixup(A64AsmCtx *ctx, uint64_t off, const char *target, int kind, int64_t addend) {
    if (ctx->fixup_count >= ctx->fixup_cap) {
        size_t nc = ctx->fixup_cap ? ctx->fixup_cap * 2 : 128;
        A64Fixup *nf = (A64Fixup *)realloc(ctx->fixups, nc * sizeof(A64Fixup));
        if (!nf) return -1;
        ctx->fixups = nf;
        ctx->fixup_cap = nc;
    }
    A64Fixup *f = &ctx->fixups[ctx->fixup_count++];
    f->offset = off;
    memset(f->target, 0, sizeof(f->target));
    strncpy(f->target, target, sizeof(f->target) - 1);
    f->kind = kind;
    f->addend = addend;
    f->line = ctx->line_no;
    return 0;
}

static int a64_strieq(const char *a, const char *b) {
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
        a++; b++;
    }
    return *a == *b;
}

static char *a64_trim(char *s) {
    while (*s && (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')) s++;
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t' || s[len - 1] == '\r' || s[len - 1] == '\n')) {
        s[--len] = '\0';
    }
    return s;
}

static void a64_strip_comment(char *s) {
    int in_str = 0;
    for (size_t i = 0; s[i]; i++) {
        if (s[i] == '"' && (i == 0 || s[i - 1] != '\\')) in_str = !in_str;
        if (!in_str) {
            if (s[i] == '/' && s[i + 1] == '/') { s[i] = '\0'; break; }
            if (s[i] == ';') { s[i] = '\0'; break; }
        }
    }
}

/* Split operand string on top-level commas (respecting [...]) */
static int a64_split_ops(char *s, char **ops, int max_ops) {
    int cnt = 0;
    int br = 0;
    char *p = a64_trim(s);
    if (!*p) return 0;
    ops[cnt++] = p;
    while (*p) {
        if (*p == '[') br++;
        else if (*p == ']' && br > 0) br--;
        else if (*p == ',' && br == 0) {
            *p = '\0';
            ops[cnt - 1] = a64_trim(ops[cnt - 1]);
            if (cnt < max_ops) ops[cnt++] = a64_trim(p + 1);
        }
        p++;
    }
    ops[cnt - 1] = a64_trim(ops[cnt - 1]);
    return cnt;
}

/* Register classes:
 *  0..30: x0..x30 / w0..w30
 *  31: sp/wsp (is_sp=1) OR xzr/wzr (is_zr=1)
 *  64..95: d0..d31 / s0..s31 / h0..h31 / v0..v31
 */
static int a64_parse_reg(const char *s, int *reg_no, int *is64, int *is_sp, int *is_fp) {
    char buf[64];
    size_t i = 0;
    while (s[i] && i + 1 < sizeof(buf)) {
        buf[i] = (char)tolower((unsigned char)s[i]);
        i++;
    }
    buf[i] = '\0';
    char *t = a64_trim(buf);
    if (is_sp) *is_sp = 0;
    if (is_fp) *is_fp = 0;
    if (strcmp(t, "sp") == 0) { *reg_no = 31; *is64 = 1; if (is_sp) *is_sp = 1; return 1; }
    if (strcmp(t, "wsp") == 0) { *reg_no = 31; *is64 = 0; if (is_sp) *is_sp = 1; return 1; }
    if (strcmp(t, "xzr") == 0) { *reg_no = 31; *is64 = 1; return 1; }
    if (strcmp(t, "wzr") == 0) { *reg_no = 31; *is64 = 0; return 1; }
    if (strcmp(t, "fp") == 0) { *reg_no = 29; *is64 = 1; return 1; }
    if (strcmp(t, "lr") == 0) { *reg_no = 30; *is64 = 1; return 1; }
    if ((t[0] == 'x' || t[0] == 'w') && isdigit((unsigned char)t[1])) {
        char *ep = NULL;
        long r = strtol(t + 1, &ep, 10);
        if (ep && *ep == '\0' && r >= 0 && r <= 30) {
            *reg_no = (int)r;
            *is64 = (t[0] == 'x') ? 1 : 0;
            return 1;
        }
    }
    if ((t[0] == 'd' || t[0] == 's' || t[0] == 'h' || t[0] == 'b' || t[0] == 'q') && isdigit((unsigned char)t[1])) {
        char *ep = NULL;
        long r = strtol(t + 1, &ep, 10);
        if (ep && *ep == '\0' && r >= 0 && r <= 31) {
            *reg_no = (int)r;
            *is64 = (t[0] == 'd' || t[0] == 'q') ? 1 : 0;
            if (is_fp) *is_fp = 1;
            return 1;
        }
    }
    if (t[0] == 'v' && isdigit((unsigned char)t[1])) {
        char *ep = NULL;
        long r = strtol(t + 1, &ep, 10);
        if (ep && r >= 0 && r <= 31) {
            *reg_no = (int)r;
            *is64 = 1;
            if (is_fp) *is_fp = 2;
            return 1;
        }
    }
    return 0;
}

static int a64_parse_imm(const char *s, int64_t *val) {
    char buf[64];
    strncpy(buf, s, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    char *t = a64_trim(buf);
    if (t[0] == '#') t++;
    t = a64_trim(t);
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

static int a64_parse_cond(const char *s) {
    char buf[16];
    size_t i = 0;
    while (s[i] && i + 1 < sizeof(buf)) {
        buf[i] = (char)tolower((unsigned char)s[i]);
        i++;
    }
    buf[i] = '\0';
    if (strcmp(buf, "eq") == 0) return 0;
    if (strcmp(buf, "ne") == 0) return 1;
    if (strcmp(buf, "cs") == 0 || strcmp(buf, "hs") == 0) return 2;
    if (strcmp(buf, "cc") == 0 || strcmp(buf, "lo") == 0) return 3;
    if (strcmp(buf, "mi") == 0) return 4;
    if (strcmp(buf, "pl") == 0) return 5;
    if (strcmp(buf, "vs") == 0) return 6;
    if (strcmp(buf, "vc") == 0) return 7;
    if (strcmp(buf, "hi") == 0) return 8;
    if (strcmp(buf, "ls") == 0) return 9;
    if (strcmp(buf, "ge") == 0) return 10;
    if (strcmp(buf, "lt") == 0) return 11;
    if (strcmp(buf, "gt") == 0) return 12;
    if (strcmp(buf, "le") == 0) return 13;
    if (strcmp(buf, "al") == 0) return 14;
    return -1;
}

static int a64_encode_bitmask(uint64_t val, int is64, uint32_t *out_n, uint32_t *out_immr, uint32_t *out_imms) {
    if (!is64) {
        val &= 0xFFFFFFFFULL;
        if (val == 0 || val == 0xFFFFFFFFULL) return 0;
        val |= (val << 32);
    } else {
        if (val == 0 || val == 0xFFFFFFFFFFFFFFFFULL) return 0;
    }
    int esizes[6] = {2, 4, 8, 16, 32, 64};
    int max_e = is64 ? 6 : 5;
    for (int ei = 0; ei < max_e; ei++) {
        int e = esizes[ei];
        uint64_t emask = (e == 64) ? ~0ULL : ((1ULL << e) - 1ULL);
        uint64_t pat = val & emask;
        int ok_rep = 1;
        for (int b = e; b < 64; b += e) {
            if (((val >> b) & emask) != pat) { ok_rep = 0; break; }
        }
        if (!ok_rep || pat == 0 || pat == emask) continue;
        for (int rot = 0; rot < e; rot++) {
            uint64_t rpat = ((pat << rot) | (pat >> (e - rot))) & emask;
            /* Check if rpat is of the form (1<<ones)-1 */
            if (rpat == 0 || (rpat & (rpat + 1)) != 0) continue;
            int ones = 0;
            uint64_t tmp = rpat;
            while (tmp & 1ULL) { ones++; tmp >>= 1; }
            uint32_t n = (e == 64) ? 1U : 0U;
            uint32_t imms = (uint32_t)((~(e * 2 - 1) & 0x3F) | (ones - 1));
            uint32_t immr = (uint32_t)(rot & (e - 1));
            *out_n = n;
            *out_immr = immr;
            *out_imms = imms;
            return 1;
        }
    }
    return 0;
}

static int a64_mov_imm_words(int is64, int rd, uint64_t val, uint32_t out[4]) {
    uint32_t sf = is64 ? (1U << 31) : 0U;
    int chunks = is64 ? 4 : 2;
    if (!is64) val &= 0xFFFFFFFFULL;

    /* Count non-zero and non-0xFFFF chunks */
    int nz = 0, nf = 0, first_nz = -1, first_nf = -1;
    for (int i = 0; i < chunks; i++) {
        uint16_t c = (uint16_t)((val >> (i * 16)) & 0xFFFFU);
        if (c != 0) { nz++; if (first_nz < 0) first_nz = i; }
        if (c != 0xFFFFU) { nf++; if (first_nf < 0) first_nf = i; }
    }
    if (nz == 0) {
        out[0] = sf | (0x294U << 21) | (uint32_t)(rd & 31); /* MOVZ rd, #0 */
        return 1;
    }
    if (nz == 1) {
        uint16_t c = (uint16_t)((val >> (first_nz * 16)) & 0xFFFFU);
        out[0] = sf | (0x294U << 21) | ((uint32_t)first_nz << 21) | ((uint32_t)c << 5) | (uint32_t)(rd & 31);
        return 1;
    }
    if (nf == 0) {
        out[0] = sf | (0x094U << 21) | (uint32_t)(rd & 31); /* MOVN rd, #0 */
        return 1;
    }
    if (nf == 1) {
        uint16_t inv = (uint16_t)(~((val >> (first_nf * 16)) & 0xFFFFU) & 0xFFFFU);
        out[0] = sf | (0x094U << 21) | ((uint32_t)first_nf << 21) | ((uint32_t)inv << 5) | (uint32_t)(rd & 31);
        return 1;
    }
    int n = 0;
    for (int i = 0; i < chunks; i++) {
        uint16_t c = (uint16_t)((val >> (i * 16)) & 0xFFFFU);
        if (c != 0 || i == first_nz) {
            uint32_t opc = (n == 0) ? 0x294U : 0x394U; /* MOVZ then MOVK */
            out[n++] = sf | (opc << 21) | ((uint32_t)i << 21) | ((uint32_t)c << 5) | (uint32_t)(rd & 31);
        }
    }
    return n;
}

static size_t a64_cur_off(const A64AsmCtx *ctx) {
    if (ctx->cur_seg == 1) return ctx->text.size;
    if (ctx->cur_seg == 2) return ctx->data.size;
    return ctx->bss_size;
}

static int a64_emit_insn(A64AsmCtx *ctx, uint32_t w) {
    if (ctx->pass == 1) {
        /* Advance logical .text size via buffer append or dummy 4 bytes */
        return a64_buf_u32(&ctx->text, w);
    }
    return a64_buf_u32(&ctx->text, w);
}

static int a64_emit_data(A64AsmCtx *ctx, const void *ptr, size_t n) {
    if (ctx->cur_seg == 1) return a64_buf_append(&ctx->text, ptr, n);
    if (ctx->cur_seg == 2) return a64_buf_append(&ctx->data, ptr, n);
    ctx->bss_size += n;
    return 0;
}

static int a64_emit_align(A64AsmCtx *ctx, size_t align) {
    if (align <= 1) return 0;
    size_t cur = a64_cur_off(ctx);
    size_t aligned = (cur + align - 1) & ~(align - 1);
    size_t pad = aligned - cur;
    if (pad == 0) return 0;
    if (ctx->cur_seg == 1) {
        while (pad >= 4) { a64_buf_u32(&ctx->text, 0xD503201FU); pad -= 4; }
        return a64_buf_zeros(&ctx->text, pad);
    }
    if (ctx->cur_seg == 2) return a64_buf_zeros(&ctx->data, pad);
    ctx->bss_size += pad;
    return 0;
}

static int a64_fail(A64AsmCtx *ctx, const char *msg) {
    fprintf(stderr, "zcc: arm64 direct asm error (line %d): %s\n", ctx->line_no, msg);
    ctx->err = 1;
    return -1;
}

/* Parse [base], [base, #imm], [base, #imm]!, [base], #imm, or [base, rm] */
typedef struct {
    int base;
    int base_is_sp;
    int has_imm;
    int64_t imm;
    int pre_idx;
    int post_idx;
    int has_rm;
    int rm;
} A64MemOp;

static int a64_parse_mem(char *s, A64MemOp *m) {
    memset(m, 0, sizeof(*m));
    char *t = a64_trim(s);
    if (t[0] != '[') return 0;
    char *rb = strchr(t, ']');
    if (!rb) return 0;
    *rb = '\0';
    char *inner = a64_trim(t + 1);
    char *after = a64_trim(rb + 1);

    if (after[0] == '!') {
        m->pre_idx = 1;
    } else if (after[0] == ',') {
        int64_t pv = 0;
        if (!a64_parse_imm(after + 1, &pv)) return 0;
        m->post_idx = 1;
        m->has_imm = 1;
        m->imm = pv;
    } else if (after[0] != '\0') {
        return 0;
    }

    char *comma = strchr(inner, ',');
    if (!comma) {
        int is64 = 0, sp = 0;
        if (!a64_parse_reg(inner, &m->base, &is64, &sp, NULL) || !is64) return 0;
        m->base_is_sp = sp;
        return 1;
    }
    *comma = '\0';
    int is64 = 0, sp = 0;
    if (!a64_parse_reg(inner, &m->base, &is64, &sp, NULL) || !is64) return 0;
    m->base_is_sp = sp;
    char *op2 = a64_trim(comma + 1);
    int64_t iv = 0;
    if (a64_parse_imm(op2, &iv)) {
        m->has_imm = 1;
        m->imm = iv;
        return 1;
    }
    int rm = 0, rm64 = 0;
    if (a64_parse_reg(op2, &rm, &rm64, NULL, NULL) && rm64) {
        m->has_rm = 1;
        m->rm = rm;
        return 1;
    }
    return 0;
}

static int a64_parse_string_esc(const char *s, A64Buf *out) {
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
                if (isxdigit((unsigned char)p[0])) {
                    v = (unsigned int)strtoul(p, (char **)&p, 16);
                }
                ch = (uint8_t)(v & 0xFFU);
            } else if (e >= '0' && e <= '7') {
                unsigned int v = (unsigned int)(e - '0');
                for (int k = 0; k < 2 && *p >= '0' && *p <= '7'; k++) {
                    v = (v << 3) | (unsigned int)(*p++ - '0');
                }
                ch = (uint8_t)(v & 0xFFU);
            } else {
                ch = (uint8_t)e;
            }
        }
        a64_buf_append(out, &ch, 1);
    }
    return (*p == '"');
}

static int a64_process_line(A64AsmCtx *ctx, char *raw_line) {
    a64_strip_comment(raw_line);
    char *line = a64_trim(raw_line);
    if (!*line || line[0] == '#') return 0;

    /* Labels (may share line with directive/insn) */
    while (1) {
        char *colon = strchr(line, ':');
        if (!colon) break;
        /* Ignore colons inside string literals */
        char *q = strchr(line, '"');
        if (q && q < colon) break;
        *colon = '\0';
        char *lname = a64_trim(line);
        if (ctx->pass == 1) {
            int idx = a64_get_or_add_sym(ctx, lname);
            if (idx < 0) return a64_fail(ctx, "OOM sym");
            ctx->syms[idx].seg = (uint8_t)ctx->cur_seg;
            ctx->syms[idx].offset = a64_cur_off(ctx);
            ctx->syms[idx].defined = 1;
        }
        line = a64_trim(colon + 1);
        if (!*line) return 0;
    }

    /* Directives */
    if (line[0] == '.') {
        char dir[64];
        size_t di = 0;
        while (line[di] && !isspace((unsigned char)line[di]) && di + 1 < sizeof(dir)) {
            dir[di] = line[di];
            di++;
        }
        dir[di] = '\0';
        char *rest = a64_trim(line + di);

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
                int idx = a64_get_or_add_sym(ctx, rest);
                if (idx >= 0) ctx->syms[idx].is_global = 1;
            }
            return 0;
        }
        if (strcmp(dir, ".local") == 0) {
            if (ctx->pass == 1 && *rest) {
                int idx = a64_get_or_add_sym(ctx, rest);
                if (idx >= 0) ctx->syms[idx].is_global = 0;
            }
            return 0;
        }
        if (strcmp(dir, ".type") == 0) {
            if (ctx->pass == 1 && strstr(rest, "function")) {
                char *cm = strchr(rest, ',');
                if (cm) {
                    *cm = '\0';
                    int idx = a64_get_or_add_sym(ctx, a64_trim(rest));
                    if (idx >= 0) ctx->syms[idx].is_func = 1;
                }
            }
            return 0;
        }
        if (strcmp(dir, ".arch") == 0 || strcmp(dir, ".file") == 0 || strcmp(dir, ".loc") == 0 ||
            strcmp(dir, ".size") == 0 || strcmp(dir, ".ident") == 0 || strncmp(dir, ".cfi_", 5) == 0 ||
            strcmp(dir, ".syntax") == 0 || strcmp(dir, ".cpu") == 0) {
            return 0;
        }
        if (strcmp(dir, ".p2align") == 0) {
            int64_t p = 0;
            if (!a64_parse_imm(rest, &p) || p < 0 || p > 12) p = 0;
            return a64_emit_align(ctx, (size_t)1 << (size_t)p);
        }
        if (strcmp(dir, ".align") == 0 || strcmp(dir, ".balign") == 0) {
            int64_t a = 0;
            if (a64_parse_imm(rest, &a) && a > 0) {
                size_t al = (strcmp(dir, ".align") == 0 && a <= 8) ? ((size_t)1 << (size_t)a) : (size_t)a;
                return a64_emit_align(ctx, al);
            }
            return 0;
        }
        if (strcmp(dir, ".zero") == 0 || strcmp(dir, ".space") == 0 || strcmp(dir, ".skip") == 0) {
            int64_t sz = 0;
            if (!a64_parse_imm(rest, &sz) || sz < 0) return a64_fail(ctx, "invalid .zero size");
            if (ctx->cur_seg == 1) return a64_buf_zeros(&ctx->text, (size_t)sz);
            if (ctx->cur_seg == 2) return a64_buf_zeros(&ctx->data, (size_t)sz);
            ctx->bss_size += (size_t)sz;
            return 0;
        }
        if (strcmp(dir, ".comm") == 0 || strcmp(dir, ".lcomm") == 0) {
            char *ops[4];
            int n = a64_split_ops(rest, ops, 4);
            if (n >= 2) {
                int64_t sz = 0, al = 8;
                a64_parse_imm(ops[1], &sz);
                if (n >= 3) a64_parse_imm(ops[2], &al);
                if (al < 1) al = 1;
                size_t cur_bss = (ctx->bss_size + (size_t)al - 1) & ~((size_t)al - 1);
                if (ctx->pass == 1) {
                    int idx = a64_get_or_add_sym(ctx, ops[0]);
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
            A64Buf sb;
            a64_buf_init(&sb);
            if (!a64_parse_string_esc(rest, &sb)) { a64_buf_free(&sb); return a64_fail(ctx, "bad string literal"); }
            if (strcmp(dir, ".ascii") != 0) { uint8_t z = 0; a64_buf_append(&sb, &z, 1); }
            int rc = a64_emit_data(ctx, sb.data, sb.size);
            a64_buf_free(&sb);
            return rc;
        }
        if (strcmp(dir, ".byte") == 0 || strcmp(dir, ".hword") == 0 || strcmp(dir, ".short") == 0 ||
            strcmp(dir, ".word") == 0 || strcmp(dir, ".long") == 0 ||
            strcmp(dir, ".quad") == 0 || strcmp(dir, ".xword") == 0) {
            int width = 1;
            if (strcmp(dir, ".hword") == 0 || strcmp(dir, ".short") == 0) width = 2;
            else if (strcmp(dir, ".word") == 0 || strcmp(dir, ".long") == 0) width = 4;
            else if (strcmp(dir, ".quad") == 0 || strcmp(dir, ".xword") == 0) width = 8;
            char *ops[32];
            int n = a64_split_ops(rest, ops, 32);
            for (int k = 0; k < n; k++) {
                int64_t iv = 0;
                uint8_t buf8[8] = {0};
                if (a64_parse_imm(ops[k], &iv)) {
                    uint64_t uv = (uint64_t)iv;
                    for (int b = 0; b < width; b++) buf8[b] = (uint8_t)((uv >> (b * 8)) & 0xFFU);
                    a64_emit_data(ctx, buf8, (size_t)width);
                } else {
                    if (width != 8) return a64_fail(ctx, "non-64bit symbolic relocation in data");
                    if (ctx->pass == 2 && ctx->cur_seg == 2) {
                        char tgt[128];
                        int64_t add = 0;
                        strncpy(tgt, ops[k], sizeof(tgt) - 1);
                        tgt[sizeof(tgt) - 1] = '\0';
                        char *pl = strchr(tgt, '+');
                        if (pl) { *pl = '\0'; a64_parse_imm(pl + 1, &add); }
                        a64_add_fixup(ctx, a64_cur_off(ctx), a64_trim(tgt), A64_FIX_DATA_ABS64, add);
                    }
                    a64_emit_data(ctx, buf8, 8);
                }
            }
            return 0;
        }
        return a64_fail(ctx, dir);
    }

    /* Parse instruction mnemonic */
    char mnem[32];
    size_t mi = 0;
    while (line[mi] && !isspace((unsigned char)line[mi]) && mi + 1 < sizeof(mnem)) {
        mnem[mi] = (char)tolower((unsigned char)line[mi]);
        mi++;
    }
    mnem[mi] = '\0';
    char *rest = a64_trim(line + mi);
    char *ops[6] = {0};
    char post_mem[128];
    int nops = a64_split_ops(rest, ops, 6);
    if (nops >= 2 && ops[nops - 2][0] == '[' && strchr(ops[nops - 2], ']') != NULL) {
        snprintf(post_mem, sizeof(post_mem), "%s, %s", ops[nops - 2], ops[nops - 1]);
        ops[nops - 2] = post_mem;
        nops--;
    }

    /* Ensure .text is active for instructions */
    ctx->cur_seg = 1;

    if (strcmp(mnem, "ret") == 0) {
        return a64_emit_insn(ctx, 0xD65F03C0U);
    }
    if (strcmp(mnem, "nop") == 0) {
        return a64_emit_insn(ctx, 0xD503201FU);
    }
    if (strcmp(mnem, "svc") == 0 && nops == 1) {
        int64_t imm = 0;
        if (!a64_parse_imm(ops[0], &imm) || imm < 0 || imm > 65535) return a64_fail(ctx, "bad svc imm");
        return a64_emit_insn(ctx, 0xD4000001U | ((uint32_t)imm << 5));
    }

    /* Branches: b, bl, blr, b.<cond>, beq..bhs, cbz, cbnz */
    if (strcmp(mnem, "b") == 0 && nops == 1) {
        if (ctx->pass == 2) a64_add_fixup(ctx, ctx->text.size, ops[0], A64_FIX_B26, 0);
        else a64_get_or_add_sym(ctx, ops[0]);
        return a64_emit_insn(ctx, 0x14000000U);
    }
    if (strcmp(mnem, "bl") == 0 && nops == 1) {
        if (ctx->pass == 2) a64_add_fixup(ctx, ctx->text.size, ops[0], A64_FIX_BL26, 0);
        else a64_get_or_add_sym(ctx, ops[0]);
        return a64_emit_insn(ctx, 0x94000000U);
    }
    if (strcmp(mnem, "blr") == 0 && nops == 1) {
        int rn = 0, s64 = 0;
        if (!a64_parse_reg(ops[0], &rn, &s64, NULL, NULL) || !s64) return a64_fail(ctx, "bad blr reg");
        return a64_emit_insn(ctx, 0xD63F0000U | ((uint32_t)rn << 5));
    }
    if ((mnem[0] == 'b' && mnem[1] == '.' && nops == 1) ||
        (mnem[0] == 'b' && mnem[1] != '\0' && mnem[1] != 'l' && mnem[1] != 'i' && strlen(mnem) == 3 && nops == 1)) {
        const char *cs = (mnem[1] == '.') ? (mnem + 2) : (mnem + 1);
        int cc = a64_parse_cond(cs);
        if (cc >= 0) {
            if (ctx->pass == 2) a64_add_fixup(ctx, ctx->text.size, ops[0], A64_FIX_COND19, 0);
            else a64_get_or_add_sym(ctx, ops[0]);
            return a64_emit_insn(ctx, 0x54000000U | (uint32_t)cc);
        }
    }
    if ((strcmp(mnem, "cbz") == 0 || strcmp(mnem, "cbnz") == 0) && nops == 2) {
        int rt = 0, s64 = 0;
        if (!a64_parse_reg(ops[0], &rt, &s64, NULL, NULL)) return a64_fail(ctx, "bad cbz reg");
        uint32_t sf = s64 ? (1U << 31) : 0U;
        uint32_t op = (strcmp(mnem, "cbnz") == 0) ? (1U << 24) : 0U;
        if (ctx->pass == 2) a64_add_fixup(ctx, ctx->text.size, ops[1], A64_FIX_COND19, 0);
        else a64_get_or_add_sym(ctx, ops[1]);
        return a64_emit_insn(ctx, sf | 0x34000000U | op | (uint32_t)(rt & 31));
    }

    /* MOV */
    if (strcmp(mnem, "mov") == 0 && nops == 2) {
        int rd = 0, d64 = 0, dsp = 0, rm = 0, m64 = 0, msp = 0;
        int64_t imm = 0;
        if (!a64_parse_reg(ops[0], &rd, &d64, &dsp, NULL)) return a64_fail(ctx, "bad mov dst");
        if (a64_parse_reg(ops[1], &rm, &m64, &msp, NULL)) {
            uint32_t sf = d64 ? (1U << 31) : 0U;
            if (dsp || msp) {
                return a64_emit_insn(ctx, sf | 0x11000000U | ((uint32_t)(rm & 31) << 5) | (uint32_t)(rd & 31)); /* ADD rd, rn, #0 */
            }
            return a64_emit_insn(ctx, sf | 0x2A0003E0U | ((uint32_t)(rm & 31) << 16) | (uint32_t)(rd & 31)); /* ORR rd, xzr, rm */
        }
        if (a64_parse_imm(ops[1], &imm)) {
            uint32_t w[4];
            int nw = a64_mov_imm_words(d64, rd, (uint64_t)imm, w);
            for (int k = 0; k < nw; k++) a64_emit_insn(ctx, w[k]);
            return 0;
        }
        return a64_fail(ctx, "bad mov src");
    }

    /* CSET rd, cond */
    if (strcmp(mnem, "cset") == 0 && nops == 2) {
        int rd = 0, d64 = 0;
        int cc = a64_parse_cond(ops[1]);
        if (!a64_parse_reg(ops[0], &rd, &d64, NULL, NULL) || cc < 0) return a64_fail(ctx, "bad cset");
        uint32_t sf = d64 ? (1U << 31) : 0U;
        uint32_t inv = (uint32_t)(cc ^ 1);
        return a64_emit_insn(ctx, sf | 0x1A9F07E0U | (inv << 12) | (uint32_t)(rd & 31)); /* CSINC rd, xzr, xzr, inv */
    }

    /* CSEL / CSINV rd, rn, rm, cond */
    if ((strcmp(mnem, "csel") == 0 || strcmp(mnem, "csinv") == 0) && nops == 4) {
        int rd = 0, rn = 0, rm = 0, d64 = 0, n64 = 0, m64 = 0;
        int cc = a64_parse_cond(ops[3]);
        if (!a64_parse_reg(ops[0], &rd, &d64, NULL, NULL) ||
            !a64_parse_reg(ops[1], &rn, &n64, NULL, NULL) ||
            !a64_parse_reg(ops[2], &rm, &m64, NULL, NULL) || cc < 0)
            return a64_fail(ctx, "bad csel/csinv");
        uint32_t sf = d64 ? (1U << 31) : 0U;
        uint32_t base = (strcmp(mnem, "csinv") == 0) ? 0x5A800000U : 0x1A800000U;
        return a64_emit_insn(ctx, sf | base | ((uint32_t)(rm & 31) << 16) | ((uint32_t)cc << 12) | ((uint32_t)(rn & 31) << 5) | (uint32_t)(rd & 31));
    }

    /* CMP rn, rm/#imm */
    if (strcmp(mnem, "cmp") == 0 && nops == 2) {
        int rn = 0, n64 = 0, nsp = 0, rm = 0, m64 = 0;
        int64_t imm = 0;
        if (!a64_parse_reg(ops[0], &rn, &n64, &nsp, NULL)) return a64_fail(ctx, "bad cmp rn");
        uint32_t sf = n64 ? (1U << 31) : 0U;
        if (a64_parse_imm(ops[1], &imm)) {
            if (imm >= 0 && imm <= 4095) {
                return a64_emit_insn(ctx, sf | 0x7100001FU | ((uint32_t)imm << 10) | ((uint32_t)(rn & 31) << 5));
            }
            return a64_fail(ctx, "cmp imm out of 0..4095 range");
        }
        if (a64_parse_reg(ops[1], &rm, &m64, NULL, NULL)) {
            return a64_emit_insn(ctx, sf | 0x6B00001FU | ((uint32_t)(rm & 31) << 16) | ((uint32_t)(rn & 31) << 5));
        }
        return a64_fail(ctx, "bad cmp op2");
    }

    /* NEG rd, rm */
    if (strcmp(mnem, "neg") == 0 && nops == 2) {
        int rd = 0, rm = 0, d64 = 0, m64 = 0;
        if (!a64_parse_reg(ops[0], &rd, &d64, NULL, NULL) || !a64_parse_reg(ops[1], &rm, &m64, NULL, NULL))
            return a64_fail(ctx, "bad neg");
        uint32_t sf = d64 ? (1U << 31) : 0U;
        return a64_emit_insn(ctx, sf | 0x4B0003E0U | ((uint32_t)(rm & 31) << 16) | (uint32_t)(rd & 31)); /* SUB rd, xzr, rm */
    }

    /* ADD / ADDS / SUB / SUBS */
    if ((strcmp(mnem, "add") == 0 || strcmp(mnem, "adds") == 0 ||
         strcmp(mnem, "sub") == 0 || strcmp(mnem, "subs") == 0) && nops == 3) {
        int rd = 0, rn = 0, rm = 0, d64 = 0, n64 = 0, m64 = 0, dsp = 0, nsp = 0;
        int64_t imm = 0;
        int is_sub = (mnem[0] == 's');
        int sets_flags = (mnem[3] == 's');
        if (!a64_parse_reg(ops[0], &rd, &d64, &dsp, NULL) || !a64_parse_reg(ops[1], &rn, &n64, &nsp, NULL))
            return a64_fail(ctx, "bad add/sub reg");
        uint32_t sf = d64 ? (1U << 31) : 0U;
        if (a64_parse_imm(ops[2], &imm)) {
            if (imm < 0) { imm = -imm; is_sub = !is_sub; }
            uint32_t shift = 0;
            if (imm > 4095 && (imm & 0xFFFLL) == 0 && (imm >> 12) <= 4095) {
                imm >>= 12;
                shift = 1U << 22;
            }
            if (imm < 0 || imm > 4095) return a64_fail(ctx, "add/sub imm out of range");
            uint32_t op = (is_sub ? (1U << 30) : 0U) | (sets_flags ? (1U << 29) : 0U);
            return a64_emit_insn(ctx, sf | 0x11000000U | op | shift | ((uint32_t)imm << 10) | ((uint32_t)(rn & 31) << 5) | (uint32_t)(rd & 31));
        }
        if (a64_parse_reg(ops[2], &rm, &m64, NULL, NULL)) {
            uint32_t op = (is_sub ? (1U << 30) : 0U) | (sets_flags ? (1U << 29) : 0U);
            if ((dsp || nsp) && !sets_flags) {
                /* ADD/SUB (extended register): UXTX (opt=3) */
                return a64_emit_insn(ctx, sf | 0x0B206000U | op | ((uint32_t)(rm & 31) << 16) | ((uint32_t)(rn & 31) << 5) | (uint32_t)(rd & 31));
            }
            return a64_emit_insn(ctx, sf | 0x0B000000U | op | ((uint32_t)(rm & 31) << 16) | ((uint32_t)(rn & 31) << 5) | (uint32_t)(rd & 31));
        }
        return a64_fail(ctx, "bad add/sub op3");
    }

    /* MUL / MSUB / SDIV / UDIV */
    if (strcmp(mnem, "mul") == 0 && nops == 3) {
        int rd = 0, rn = 0, rm = 0, d64 = 0, n64 = 0, m64 = 0;
        if (!a64_parse_reg(ops[0], &rd, &d64, NULL, NULL) ||
            !a64_parse_reg(ops[1], &rn, &n64, NULL, NULL) ||
            !a64_parse_reg(ops[2], &rm, &m64, NULL, NULL)) return a64_fail(ctx, "bad mul");
        uint32_t sf = d64 ? (1U << 31) : 0U;
        return a64_emit_insn(ctx, sf | 0x1B007C00U | ((uint32_t)(rm & 31) << 16) | ((uint32_t)(rn & 31) << 5) | (uint32_t)(rd & 31));
    }
    if (strcmp(mnem, "msub") == 0 && nops == 4) {
        int rd = 0, rn = 0, rm = 0, ra = 0, d64 = 0, t = 0;
        if (!a64_parse_reg(ops[0], &rd, &d64, NULL, NULL) || !a64_parse_reg(ops[1], &rn, &t, NULL, NULL) ||
            !a64_parse_reg(ops[2], &rm, &t, NULL, NULL) || !a64_parse_reg(ops[3], &ra, &t, NULL, NULL))
            return a64_fail(ctx, "bad msub");
        uint32_t sf = d64 ? (1U << 31) : 0U;
        return a64_emit_insn(ctx, sf | 0x1B008000U | ((uint32_t)(rm & 31) << 16) | ((uint32_t)(ra & 31) << 10) | ((uint32_t)(rn & 31) << 5) | (uint32_t)(rd & 31));
    }
    if ((strcmp(mnem, "smull") == 0 || strcmp(mnem, "umull") == 0) && nops == 3) {
        int rd = 0, rn = 0, rm = 0, d64 = 0, n64 = 0, m64 = 0;
        if (!a64_parse_reg(ops[0], &rd, &d64, NULL, NULL) ||
            !a64_parse_reg(ops[1], &rn, &n64, NULL, NULL) ||
            !a64_parse_reg(ops[2], &rm, &m64, NULL, NULL)) return a64_fail(ctx, "bad smull/umull");
        uint32_t ubit = (mnem[0] == 'u') ? (1U << 23) : 0U;
        return a64_emit_insn(ctx, 0x9B207C00U | ubit | ((uint32_t)(rm & 31) << 16) | ((uint32_t)(rn & 31) << 5) | (uint32_t)(rd & 31));
    }
    if ((strcmp(mnem, "sdiv") == 0 || strcmp(mnem, "udiv") == 0) && nops == 3) {
        int rd = 0, rn = 0, rm = 0, d64 = 0, t = 0;
        if (!a64_parse_reg(ops[0], &rd, &d64, NULL, NULL) ||
            !a64_parse_reg(ops[1], &rn, &t, NULL, NULL) ||
            !a64_parse_reg(ops[2], &rm, &t, NULL, NULL)) return a64_fail(ctx, "bad div");
        uint32_t sf = d64 ? (1U << 31) : 0U;
        uint32_t o1 = (mnem[0] == 's') ? (1U << 10) : 0U;
        return a64_emit_insn(ctx, sf | 0x1AC00800U | o1 | ((uint32_t)(rm & 31) << 16) | ((uint32_t)(rn & 31) << 5) | (uint32_t)(rd & 31));
    }

    /* Logical: and, orr, eor, bic (supports optional 4th lsl/lsr/asr #imm shift or bitmask imm) */
    if ((strcmp(mnem, "and") == 0 || strcmp(mnem, "orr") == 0 || strcmp(mnem, "eor") == 0 || strcmp(mnem, "bic") == 0) &&
        (nops == 3 || nops == 4)) {
        int rd = 0, rn = 0, rm = 0, d64 = 0, n64 = 0, m64 = 0;
        int64_t imm = 0;
        if (!a64_parse_reg(ops[0], &rd, &d64, NULL, NULL) || !a64_parse_reg(ops[1], &rn, &n64, NULL, NULL))
            return a64_fail(ctx, "bad logic reg");
        uint32_t sf = d64 ? (1U << 31) : 0U;
        if (nops == 3 && a64_parse_imm(ops[2], &imm) && strcmp(mnem, "bic") != 0) {
            uint32_t N = 0, immr = 0, imms = 0;
            if (!a64_encode_bitmask((uint64_t)imm, d64, &N, &immr, &imms)) return a64_fail(ctx, "unencodable logic imm");
            uint32_t opc = (strcmp(mnem, "and") == 0) ? 0U : ((strcmp(mnem, "orr") == 0) ? 1U : 2U);
            return a64_emit_insn(ctx, sf | (opc << 29) | 0x12000000U | (N << 22) | (immr << 16) | (imms << 10) | ((uint32_t)(rn & 31) << 5) | (uint32_t)(rd & 31));
        }
        if (a64_parse_reg(ops[2], &rm, &m64, NULL, NULL)) {
            uint32_t opc = 0, N = 0;
            if (strcmp(mnem, "orr") == 0) opc = 1U;
            else if (strcmp(mnem, "eor") == 0) opc = 2U;
            else if (strcmp(mnem, "bic") == 0) { opc = 0U; N = 1U << 21; }
            uint32_t sh_type = 0, sh_amt = 0;
            if (nops == 4) {
                char sbuf[32];
                strncpy(sbuf, ops[3], sizeof(sbuf) - 1);
                sbuf[sizeof(sbuf) - 1] = '\0';
                char *sp = strchr(sbuf, ' ');
                if (sp) {
                    *sp = '\0';
                    int64_t sa = 0;
                    a64_parse_imm(sp + 1, &sa);
                    sh_amt = (uint32_t)(sa & 63);
                    if (a64_strieq(sbuf, "lsr")) sh_type = 1;
                    else if (a64_strieq(sbuf, "asr")) sh_type = 2;
                    else if (a64_strieq(sbuf, "ror")) sh_type = 3;
                }
            }
            return a64_emit_insn(ctx, sf | (opc << 29) | 0x0A000000U | N | (sh_type << 22) | ((uint32_t)(rm & 31) << 16) | (sh_amt << 10) | ((uint32_t)(rn & 31) << 5) | (uint32_t)(rd & 31));
        }
        return a64_fail(ctx, "bad logic op3");
    }

    /* Shifts & rotates: lsl, lsr, asr, ror */
    if ((strcmp(mnem, "lsl") == 0 || strcmp(mnem, "lsr") == 0 || strcmp(mnem, "asr") == 0 || strcmp(mnem, "ror") == 0) && nops == 3) {
        int rd = 0, rn = 0, rm = 0, d64 = 0, n64 = 0, m64 = 0;
        int64_t imm = 0;
        if (!a64_parse_reg(ops[0], &rd, &d64, NULL, NULL) || !a64_parse_reg(ops[1], &rn, &n64, NULL, NULL))
            return a64_fail(ctx, "bad shift reg");
        uint32_t sf = d64 ? (1U << 31) : 0U;
        uint32_t N = d64 ? (1U << 22) : 0U;
        int bits = d64 ? 64 : 32;
        if (a64_parse_imm(ops[2], &imm)) {
            int sh = (int)(imm & (bits - 1));
            if (strcmp(mnem, "lsl") == 0) {
                uint32_t immr = (uint32_t)((bits - sh) & (bits - 1));
                uint32_t imms = (uint32_t)(bits - 1 - sh);
                return a64_emit_insn(ctx, sf | N | 0x53000000U | (immr << 16) | (imms << 10) | ((uint32_t)(rn & 31) << 5) | (uint32_t)(rd & 31));
            }
            if (strcmp(mnem, "lsr") == 0) {
                return a64_emit_insn(ctx, sf | N | 0x53000000U | ((uint32_t)sh << 16) | ((uint32_t)(bits - 1) << 10) | ((uint32_t)(rn & 31) << 5) | (uint32_t)(rd & 31));
            }
            if (strcmp(mnem, "asr") == 0) {
                return a64_emit_insn(ctx, sf | N | 0x13000000U | ((uint32_t)sh << 16) | ((uint32_t)(bits - 1) << 10) | ((uint32_t)(rn & 31) << 5) | (uint32_t)(rd & 31));
            }
            if (strcmp(mnem, "ror") == 0) {
                return a64_emit_insn(ctx, sf | N | 0x13800000U | ((uint32_t)(rn & 31) << 16) | ((uint32_t)sh << 10) | ((uint32_t)(rn & 31) << 5) | (uint32_t)(rd & 31));
            }
        }
        if (a64_parse_reg(ops[2], &rm, &m64, NULL, NULL)) {
            uint32_t op2 = 8U;
            if (strcmp(mnem, "lsr") == 0) op2 = 9U;
            else if (strcmp(mnem, "asr") == 0) op2 = 10U;
            else if (strcmp(mnem, "ror") == 0) op2 = 11U;
            return a64_emit_insn(ctx, sf | 0x1AC00000U | ((uint32_t)(rm & 31) << 16) | (op2 << 10) | ((uint32_t)(rn & 31) << 5) | (uint32_t)(rd & 31));
        }
        return a64_fail(ctx, "bad shift op3");
    }

    /* Bit/extension 2-op unary: clz, cls, rbit, rev, rev16, sxtw, sxtb, sxth, uxtw, uxtb, uxth */
    if (nops == 2 && (strcmp(mnem, "clz") == 0 || strcmp(mnem, "cls") == 0 ||
                      strcmp(mnem, "rbit") == 0 || strcmp(mnem, "rev") == 0 || strcmp(mnem, "rev16") == 0 ||
                      strcmp(mnem, "sxtw") == 0 || strcmp(mnem, "sxtb") == 0 || strcmp(mnem, "sxth") == 0 ||
                      strcmp(mnem, "uxtw") == 0 || strcmp(mnem, "uxtb") == 0 || strcmp(mnem, "uxth") == 0)) {
        int rd = 0, rn = 0, d64 = 0, n64 = 0;
        if (!a64_parse_reg(ops[0], &rd, &d64, NULL, NULL) || !a64_parse_reg(ops[1], &rn, &n64, NULL, NULL))
            return a64_fail(ctx, "bad bit/ext reg");
        uint32_t sf = d64 ? (1U << 31) : 0U;
        uint32_t N = d64 ? (1U << 22) : 0U;
        if (strcmp(mnem, "rbit") == 0) return a64_emit_insn(ctx, sf | 0x5AC00000U | ((uint32_t)(rn & 31) << 5) | (uint32_t)(rd & 31));
        if (strcmp(mnem, "rev16") == 0) return a64_emit_insn(ctx, sf | 0x5AC00400U | ((uint32_t)(rn & 31) << 5) | (uint32_t)(rd & 31));
        if (strcmp(mnem, "rev") == 0) return a64_emit_insn(ctx, sf | (d64 ? 0x5AC00C00U : 0x5AC00800U) | ((uint32_t)(rn & 31) << 5) | (uint32_t)(rd & 31));
        if (strcmp(mnem, "clz") == 0) return a64_emit_insn(ctx, sf | 0x5AC01000U | ((uint32_t)(rn & 31) << 5) | (uint32_t)(rd & 31));
        if (strcmp(mnem, "cls") == 0) return a64_emit_insn(ctx, sf | 0x5AC01400U | ((uint32_t)(rn & 31) << 5) | (uint32_t)(rd & 31));
        if (strcmp(mnem, "sxtb") == 0) return a64_emit_insn(ctx, sf | N | 0x13001C00U | ((uint32_t)(rn & 31) << 5) | (uint32_t)(rd & 31));
        if (strcmp(mnem, "sxth") == 0) return a64_emit_insn(ctx, sf | N | 0x13003C00U | ((uint32_t)(rn & 31) << 5) | (uint32_t)(rd & 31));
        if (strcmp(mnem, "sxtw") == 0) return a64_emit_insn(ctx, 0x93407C00U | ((uint32_t)(rn & 31) << 5) | (uint32_t)(rd & 31));
        if (strcmp(mnem, "uxtb") == 0) return a64_emit_insn(ctx, 0x53001C00U | ((uint32_t)(rn & 31) << 5) | (uint32_t)(rd & 31));
        if (strcmp(mnem, "uxth") == 0) return a64_emit_insn(ctx, 0x53003C00U | ((uint32_t)(rn & 31) << 5) | (uint32_t)(rd & 31));
        if (strcmp(mnem, "uxtw") == 0) return a64_emit_insn(ctx, 0xD3407C00U | ((uint32_t)(rn & 31) << 5) | (uint32_t)(rd & 31));
    }

    /* SIMD / FP: fmov, cnt, uaddlv, fadd, fsub, fmul, fdiv */
    if (strcmp(mnem, "fmov") == 0 && nops == 2) {
        int r0 = 0, r1 = 0, s64_0 = 0, s64_1 = 0, fp0 = 0, fp1 = 0;
        if (!a64_parse_reg(ops[0], &r0, &s64_0, NULL, &fp0) || !a64_parse_reg(ops[1], &r1, &s64_1, NULL, &fp1))
            return a64_fail(ctx, "bad fmov");
        if (fp0 && !fp1) {
            uint32_t w = s64_0 ? 0x9E670000U : 0x1E270000U;
            return a64_emit_insn(ctx, w | ((uint32_t)(r1 & 31) << 5) | (uint32_t)(r0 & 31));
        }
        if (!fp0 && fp1) {
            uint32_t w = s64_1 ? 0x9E660000U : 0x1E260000U;
            return a64_emit_insn(ctx, w | ((uint32_t)(r1 & 31) << 5) | (uint32_t)(r0 & 31));
        }
        return a64_fail(ctx, "unsupported fmov pair");
    }
    if (strcmp(mnem, "cnt") == 0 && nops == 2) {
        int r0 = 0, r1 = 0, s0 = 0, s1 = 0, fp0 = 0, fp1 = 0;
        if (!a64_parse_reg(ops[0], &r0, &s0, NULL, &fp0) || !a64_parse_reg(ops[1], &r1, &s1, NULL, &fp1))
            return a64_fail(ctx, "bad cnt");
        return a64_emit_insn(ctx, 0x0E205800U | ((uint32_t)(r1 & 31) << 5) | (uint32_t)(r0 & 31));
    }
    if (strcmp(mnem, "uaddlv") == 0 && nops == 2) {
        int r0 = 0, r1 = 0, s0 = 0, s1 = 0, fp0 = 0, fp1 = 0;
        if (!a64_parse_reg(ops[0], &r0, &s0, NULL, &fp0) || !a64_parse_reg(ops[1], &r1, &s1, NULL, &fp1))
            return a64_fail(ctx, "bad uaddlv");
        return a64_emit_insn(ctx, 0x2E303800U | ((uint32_t)(r1 & 31) << 5) | (uint32_t)(r0 & 31));
    }
    if ((strcmp(mnem, "fadd") == 0 || strcmp(mnem, "fsub") == 0 ||
         strcmp(mnem, "fmul") == 0 || strcmp(mnem, "fdiv") == 0) && nops == 3) {
        int r0 = 0, r1 = 0, r2 = 0, s0 = 0, s1 = 0, s2 = 0, f0 = 0, f1 = 0, f2 = 0;
        if (!a64_parse_reg(ops[0], &r0, &s0, NULL, &f0) || !a64_parse_reg(ops[1], &r1, &s1, NULL, &f1) ||
            !a64_parse_reg(ops[2], &r2, &s2, NULL, &f2) || !f0) return a64_fail(ctx, "bad fp arith");
        uint32_t ftype = s0 ? (1U << 22) : 0U;
        uint32_t op = 0x1E202800U;
        if (strcmp(mnem, "fsub") == 0) op = 0x1E203800U;
        else if (strcmp(mnem, "fmul") == 0) op = 0x1E200800U;
        else if (strcmp(mnem, "fdiv") == 0) op = 0x1E201800U;
        return a64_emit_insn(ctx, op | ftype | ((uint32_t)(r2 & 31) << 16) | ((uint32_t)(r1 & 31) << 5) | (uint32_t)(r0 & 31));
    }

    /* STP / LDP */
    if ((strcmp(mnem, "stp") == 0 || strcmp(mnem, "ldp") == 0) && nops == 3) {
        int r1 = 0, r2 = 0, d64_1 = 0, d64_2 = 0;
        A64MemOp m;
        if (!a64_parse_reg(ops[0], &r1, &d64_1, NULL, NULL) ||
            !a64_parse_reg(ops[1], &r2, &d64_2, NULL, NULL) ||
            !a64_parse_mem(ops[2], &m) || m.has_rm) return a64_fail(ctx, "bad stp/ldp");
        int scale = d64_1 ? 8 : 4;
        if ((m.imm % scale) != 0 || m.imm < -64 * scale || m.imm > 63 * scale) return a64_fail(ctx, "stp/ldp imm out of range");
        uint32_t imm7 = (uint32_t)((m.imm / scale) & 0x7F);
        uint32_t opc = d64_1 ? (2U << 30) : 0U;
        uint32_t load = (mnem[0] == 'l') ? (1U << 22) : 0U;
        uint32_t mode = (2U << 23); /* signed offset */
        if (m.pre_idx) mode = (3U << 23);
        else if (m.post_idx) mode = (1U << 23);
        return a64_emit_insn(ctx, opc | 0x28000000U | mode | load | (imm7 << 15) | ((uint32_t)(r2 & 31) << 10) | ((uint32_t)(m.base & 31) << 5) | (uint32_t)(r1 & 31));
    }

    /* Load/Store single: ldr, str, ldur, stur, ldrsw, ldrb, ldrsb, ldrh, ldrsh, strb, strh */
    if ((strcmp(mnem, "ldr") == 0 || strcmp(mnem, "str") == 0 ||
         strcmp(mnem, "ldur") == 0 || strcmp(mnem, "stur") == 0 ||
         strcmp(mnem, "ldrsw") == 0 || strcmp(mnem, "ldrb") == 0 ||
         strcmp(mnem, "ldrsb") == 0 || strcmp(mnem, "ldrh") == 0 ||
         strcmp(mnem, "ldrsh") == 0 || strcmp(mnem, "strb") == 0 ||
         strcmp(mnem, "strh") == 0) && nops == 2) {
        int rt = 0, t64 = 0;
        if (!a64_parse_reg(ops[0], &rt, &t64, NULL, NULL)) return a64_fail(ctx, "bad ldr/str rt");

        /* Literal pseudo: ldr rt, =imm or ldr rt, =sym */
        if (strcmp(mnem, "ldr") == 0 && ops[1][0] == '=') {
            const char *lit = ops[1] + 1;
            int64_t imm = 0;
            if (a64_parse_imm(lit, &imm)) {
                uint32_t w[4];
                int nw = a64_mov_imm_words(t64, rt, (uint64_t)imm, w);
                for (int k = 0; k < nw; k++) a64_emit_insn(ctx, w[k]);
                return 0;
            }
            /* Symbol address: adrp rt, sym + add rt, rt, lo12(sym) (always 8 bytes) */
            if (ctx->pass == 2) a64_add_fixup(ctx, ctx->text.size, lit, A64_FIX_ADRP_ADD_PAIR, 0);
            else a64_get_or_add_sym(ctx, lit);
            a64_emit_insn(ctx, 0x90000000U | (uint32_t)(rt & 31)); /* ADRP */
            a64_emit_insn(ctx, 0x91000000U | ((uint32_t)(rt & 31) << 5) | (uint32_t)(rt & 31)); /* ADD */
            return 0;
        }

        A64MemOp m;
        if (!a64_parse_mem(ops[1], &m)) return a64_fail(ctx, "bad memory operand");

        uint32_t size_bits = 3U;
        uint32_t opc = (mnem[0] == 's') ? 0U : 1U;
        int elem_sz = 8;
        if (strcmp(mnem, "ldr") == 0 || strcmp(mnem, "str") == 0 ||
            strcmp(mnem, "ldur") == 0 || strcmp(mnem, "stur") == 0) {
            size_bits = t64 ? 3U : 2U;
            elem_sz = t64 ? 8 : 4;
        } else if (strcmp(mnem, "ldrsw") == 0) {
            size_bits = 2U; opc = 2U; elem_sz = 4;
        } else if (strcmp(mnem, "ldrb") == 0 || strcmp(mnem, "strb") == 0) {
            size_bits = 0U; elem_sz = 1;
        } else if (strcmp(mnem, "ldrsb") == 0) {
            size_bits = 0U; opc = t64 ? 2U : 3U; elem_sz = 1;
        } else if (strcmp(mnem, "ldrh") == 0 || strcmp(mnem, "strh") == 0) {
            size_bits = 1U; elem_sz = 2;
        } else if (strcmp(mnem, "ldrsh") == 0) {
            size_bits = 1U; opc = t64 ? 2U : 3U; elem_sz = 2;
        }

        if (m.has_rm) {
            return a64_emit_insn(ctx, (size_bits << 30) | 0x38206800U | (opc << 22) | ((uint32_t)(m.rm & 31) << 16) | ((uint32_t)(m.base & 31) << 5) | (uint32_t)(rt & 31));
        }
        if (m.pre_idx || m.post_idx) {
            if (m.imm < -256 || m.imm > 255) return a64_fail(ctx, "pre/post index imm out of range");
            uint32_t imm9 = (uint32_t)(m.imm & 0x1FF);
            uint32_t idx_bits = m.pre_idx ? (3U << 10) : (1U << 10);
            return a64_emit_insn(ctx, (size_bits << 30) | 0x38000000U | (opc << 22) | (imm9 << 12) | idx_bits | ((uint32_t)(m.base & 31) << 5) | (uint32_t)(rt & 31));
        }
        int is_unscaled = (strcmp(mnem, "ldur") == 0 || strcmp(mnem, "stur") == 0);
        if (is_unscaled || (m.imm < 0 && m.imm >= -256 && m.imm <= 255)) {
            if (m.imm < -256 || m.imm > 255) return a64_fail(ctx, "unscaled offset out of range");
            uint32_t imm9 = (uint32_t)(m.imm & 0x1FF);
            return a64_emit_insn(ctx, (size_bits << 30) | 0x38000000U | (opc << 22) | (imm9 << 12) | ((uint32_t)(m.base & 31) << 5) | (uint32_t)(rt & 31));
        }
        if (m.imm >= 0 && (m.imm % elem_sz) == 0 && (m.imm / elem_sz) <= 4095) {
            uint32_t imm12 = (uint32_t)(m.imm / elem_sz);
            return a64_emit_insn(ctx, (size_bits << 30) | 0x39000000U | (opc << 22) | (imm12 << 10) | ((uint32_t)(m.base & 31) << 5) | (uint32_t)(rt & 31));
        }
        return a64_fail(ctx, "memory offset out of range");
    }

    return a64_fail(ctx, mnem);
}

static uint32_t a64_read_u32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void a64_write_u32(uint8_t *p, uint32_t w) {
    p[0] = (uint8_t)(w & 0xFFU);
    p[1] = (uint8_t)((w >> 8) & 0xFFU);
    p[2] = (uint8_t)((w >> 16) & 0xFFU);
    p[3] = (uint8_t)((w >> 24) & 0xFFU);
}

static int a64_run_passes(A64AsmCtx *ctx, const char *asm_buf, size_t asm_len, const char *extra_rt) {
    for (int pass = 1; pass <= 2; pass++) {
        ctx->pass = pass;
        if (pass == 2) ctx->fixup_count = 0;
        a64_buf_free(&ctx->text);
        a64_buf_free(&ctx->data);
        a64_buf_init(&ctx->text);
        a64_buf_init(&ctx->data);
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
            if (a64_process_line(ctx, cur) < 0) break;
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
                if (a64_process_line(ctx, rp) < 0) break;
                if (!nl) break;
                rp = nl + 1;
            }
            free(rt_copy);
            if (ctx->err) return -1;
        }
    }
    return 0;
}

static int a64_assemble_internal(const char *asm_buf, size_t asm_len, const char *out_filename, int is_exec) {
    if (!asm_buf || !out_filename) return -1;
    A64AsmCtx ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.is_exec = is_exec;
    ctx.cur_seg = 1;

    /* Pass 1 initial scan to discover undefined runtime symbols when linking ET_EXEC */
    if (a64_run_passes(&ctx, asm_buf, asm_len, NULL) < 0) {
        a64_buf_free(&ctx.text); a64_buf_free(&ctx.data);
        free(ctx.syms); free(ctx.fixups);
        return -1;
    }

    if (is_exec) {
        char rt_buf[4096];
        rt_buf[0] = '\0';
        int has_start = (a64_find_sym(&ctx, "_start") >= 0 && ctx.syms[a64_find_sym(&ctx, "_start")].defined);
        int has_main = (a64_find_sym(&ctx, "main") >= 0 && ctx.syms[a64_find_sym(&ctx, "main")].defined);
        if (!has_start && has_main) {
            strcat(rt_buf,
                   "\n.text\n.global _start\n_start:\n"
                   "    ldr x0, [sp]\n"
                   "    add x1, sp, #8\n"
                   "    bl main\n"
                   "    mov x8, #93\n"
                   "    svc #0\n");
        }
        int need_exit = (a64_find_sym(&ctx, "exit") >= 0 && !ctx.syms[a64_find_sym(&ctx, "exit")].defined);
        int need_write = (a64_find_sym(&ctx, "write") >= 0 && !ctx.syms[a64_find_sym(&ctx, "write")].defined);
        int need_putchar = (a64_find_sym(&ctx, "putchar") >= 0 && !ctx.syms[a64_find_sym(&ctx, "putchar")].defined);
        int need_puts = (a64_find_sym(&ctx, "puts") >= 0 && !ctx.syms[a64_find_sym(&ctx, "puts")].defined);
        if (need_exit) {
            strcat(rt_buf, "\n.global exit\nexit:\n    mov x8, #93\n    svc #0\n");
        }
        if (need_write) {
            strcat(rt_buf, "\n.global write\nwrite:\n    mov x8, #64\n    svc #0\n    ret\n");
        }
        if (need_putchar) {
            strcat(rt_buf,
                   "\n.global putchar\nputchar:\n"
                   "    sub sp, sp, #16\n"
                   "    strb w0, [sp]\n"
                   "    mov x0, #1\n"
                   "    mov x1, sp\n"
                   "    mov x2, #1\n"
                   "    mov x8, #64\n"
                   "    svc #0\n"
                   "    ldrb w0, [sp]\n"
                   "    add sp, sp, #16\n"
                   "    ret\n");
        }
        if (need_puts) {
            strcat(rt_buf,
                   "\n.global puts\nputs:\n"
                   "    mov x9, x0\n"
                   "    mov x2, #0\n"
                   ".L_zcc_a64_puts_len:\n"
                   "    add x11, x9, x2\n"
                   "    ldrb w10, [x11]\n"
                   "    cbz w10, .L_zcc_a64_puts_wr\n"
                   "    add x2, x2, #1\n"
                   "    b .L_zcc_a64_puts_len\n"
                   ".L_zcc_a64_puts_wr:\n"
                   "    mov x1, x9\n"
                   "    mov x0, #1\n"
                   "    mov x8, #64\n"
                   "    svc #0\n"
                   "    sub sp, sp, #16\n"
                   "    mov w10, #10\n"
                   "    strb w10, [sp]\n"
                   "    mov x0, #1\n"
                   "    mov x1, sp\n"
                   "    mov x2, #1\n"
                   "    mov x8, #64\n"
                   "    svc #0\n"
                   "    add sp, sp, #16\n"
                   "    mov w0, #0\n"
                   "    ret\n");
        }
        if (rt_buf[0]) {
            a64_buf_free(&ctx.text); a64_buf_free(&ctx.data);
            free(ctx.syms); free(ctx.fixups);
            memset(&ctx, 0, sizeof(ctx));
            ctx.is_exec = 1;
            ctx.cur_seg = 1;
            if (a64_run_passes(&ctx, asm_buf, asm_len, rt_buf) < 0) {
                a64_buf_free(&ctx.text); a64_buf_free(&ctx.data);
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

        /* Patch all fixups against resolved virtual addresses */
        for (size_t i = 0; i < ctx.fixup_count; i++) {
            A64Fixup *f = &ctx.fixups[i];
            int si = a64_find_sym(&ctx, f->target);
            if (si < 0 || !ctx.syms[si].defined) {
                fprintf(stderr, "zcc: arm64 undefined symbol '%s' (line %d)\n", f->target, f->line);
                a64_buf_free(&ctx.text); a64_buf_free(&ctx.data);
                free(ctx.syms); free(ctx.fixups);
                return -1;
            }
            uint64_t target_va = 0;
            if (ctx.syms[si].seg == 1) target_va = text_vaddr + ctx.syms[si].offset;
            else if (ctx.syms[si].seg == 2) target_va = data_vaddr + ctx.syms[si].offset;
            else target_va = bss_vaddr + ctx.syms[si].offset;
            target_va = (uint64_t)((int64_t)target_va + f->addend);

            if (f->kind == A64_FIX_DATA_ABS64) {
                uint8_t *p = ctx.data.data + f->offset;
                for (int b = 0; b < 8; b++) p[b] = (uint8_t)((target_va >> (b * 8)) & 0xFFU);
                continue;
            }
            uint64_t ins_va = text_vaddr + f->offset;
            uint32_t w = a64_read_u32(ctx.text.data + f->offset);
            if (f->kind == A64_FIX_B26 || f->kind == A64_FIX_BL26) {
                int64_t imm26 = ((int64_t)target_va - (int64_t)ins_va) >> 2;
                if (imm26 < -(1 << 25) || imm26 >= (1 << 25)) {
                    fprintf(stderr, "zcc: arm64 branch out of range to '%s'\n", f->target);
                    a64_buf_free(&ctx.text); a64_buf_free(&ctx.data);
                    free(ctx.syms); free(ctx.fixups);
                    return -1;
                }
                w = (w & 0xFC000000U) | ((uint32_t)imm26 & 0x03FFFFFFU);
                a64_write_u32(ctx.text.data + f->offset, w);
            } else if (f->kind == A64_FIX_COND19) {
                int64_t imm19 = ((int64_t)target_va - (int64_t)ins_va) >> 2;
                if (imm19 < -(1 << 18) || imm19 >= (1 << 18)) {
                    fprintf(stderr, "zcc: arm64 cond branch out of range to '%s'\n", f->target);
                    a64_buf_free(&ctx.text); a64_buf_free(&ctx.data);
                    free(ctx.syms); free(ctx.fixups);
                    return -1;
                }
                w = (w & 0xFF00001FU) | (((uint32_t)imm19 & 0x7FFFFU) << 5);
                a64_write_u32(ctx.text.data + f->offset, w);
            } else if (f->kind == A64_FIX_ADRP_ADD_PAIR) {
                uint64_t page_t = target_va & ~0xFFFULL;
                uint64_t page_p = ins_va & ~0xFFFULL;
                int64_t pg_diff = ((int64_t)page_t - (int64_t)page_p) >> 12;
                uint32_t immlo = (uint32_t)(pg_diff & 3);
                uint32_t immhi = (uint32_t)((pg_diff >> 2) & 0x7FFFF);
                w = (w & 0x9F00001FU) | (immlo << 29) | (immhi << 5);
                a64_write_u32(ctx.text.data + f->offset, w);

                uint32_t w2 = a64_read_u32(ctx.text.data + f->offset + 4);
                uint32_t lo12 = (uint32_t)(target_va & 0xFFFU);
                w2 = (w2 & 0xFFC003FFU) | (lo12 << 10);
                a64_write_u32(ctx.text.data + f->offset + 4, w2);
            }
        }

        int s_idx = a64_find_sym(&ctx, "_start");
        if (s_idx < 0) s_idx = a64_find_sym(&ctx, "main");
        if (s_idx < 0 || !ctx.syms[s_idx].defined) {
            fprintf(stderr, "zcc: arm64 ET_EXEC missing _start/main entry point\n");
            a64_buf_free(&ctx.text); a64_buf_free(&ctx.data);
            free(ctx.syms); free(ctx.fixups);
            return -1;
        }
        uint64_t entry_va = text_vaddr + ctx.syms[s_idx].offset;
        int rc = elf_emit_exec_arch(out_filename, EM_AARCH64, 0,
                                    entry_va, text_vaddr, ctx.text.data, ctx.text.size,
                                    data_vaddr, ctx.data.data, ctx.data.size, ctx.bss_size);
        a64_buf_free(&ctx.text); a64_buf_free(&ctx.data);
        free(ctx.syms); free(ctx.fixups);
        return rc;
    }

    /* ET_REL mode: resolve internal PC-relative branches inside .text; emit ELF relas for external/data */
    zcc_elf_rela_t *relas = ctx.fixup_count ? (zcc_elf_rela_t *)calloc(ctx.fixup_count * 2, sizeof(zcc_elf_rela_t)) : NULL;
    size_t rela_n = 0;
    for (size_t i = 0; i < ctx.fixup_count; i++) {
        A64Fixup *f = &ctx.fixups[i];
        int si = a64_find_sym(&ctx, f->target);
        if (si >= 0 && ctx.syms[si].defined && ctx.syms[si].seg == 1 &&
            (f->kind == A64_FIX_B26 || f->kind == A64_FIX_BL26 || f->kind == A64_FIX_COND19)) {
            int64_t diff = ((int64_t)ctx.syms[si].offset - (int64_t)f->offset) >> 2;
            uint32_t w = a64_read_u32(ctx.text.data + f->offset);
            if (f->kind == A64_FIX_COND19) {
                w = (w & 0xFF00001FU) | (((uint32_t)diff & 0x7FFFFU) << 5);
            } else {
                w = (w & 0xFC000000U) | ((uint32_t)diff & 0x03FFFFFFU);
            }
            a64_write_u32(ctx.text.data + f->offset, w);
        } else if (f->kind != A64_FIX_DATA_ABS64) {
            if (f->kind == A64_FIX_ADRP_ADD_PAIR) {
                relas[rela_n].offset = f->offset;
                relas[rela_n].sym_name = f->target;
                relas[rela_n].type = R_AARCH64_ADR_PREL_PG_HI21;
                relas[rela_n].addend = f->addend;
                rela_n++;
                relas[rela_n].offset = f->offset + 4;
                relas[rela_n].sym_name = f->target;
                relas[rela_n].type = R_AARCH64_ADD_ABS_LO12_NC;
                relas[rela_n].addend = f->addend;
                rela_n++;
            } else {
                relas[rela_n].offset = f->offset;
                relas[rela_n].sym_name = f->target;
                relas[rela_n].type = (f->kind == A64_FIX_BL26) ? R_AARCH64_CALL26 : R_AARCH64_JUMP26;
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

    int rc = elf_emit_obj_full(out_filename, EM_AARCH64, 0,
                               ctx.text.data, ctx.text.size,
                               ctx.data.data, ctx.data.size, ctx.bss_size,
                               esyms, ctx.sym_count, relas, rela_n);
    if (esyms) free(esyms);
    if (relas) free(relas);
    a64_buf_free(&ctx.text); a64_buf_free(&ctx.data);
    free(ctx.syms); free(ctx.fixups);
    return rc;
}

int arm64_assemble_to_elf_obj(const char *asm_buf, size_t asm_len, const char *out_o) {
    return a64_assemble_internal(asm_buf, asm_len, out_o, 0);
}

int arm64_assemble_to_elf_exec(const char *asm_buf, size_t asm_len, const char *out_exe) {
    return a64_assemble_internal(asm_buf, asm_len, out_exe, 1);
}

#endif /* ZCC_ARM64_ELF_EMIT_C */
