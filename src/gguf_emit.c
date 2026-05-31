#include "gguf_emit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

enum {
    MAX_IDENT   = 128,
    MAX_STR     = 16384,
    MAX_STRINGS = 262144,
    MAX_GLOBALS = 262144,
    MAX_STRUCTS = 65536,
    MAX_PARAMS  = 128,
    MAX_CALL_ARGS = 256,
    MAX_CASES   = 4096,
    MAX_INIT    = 1048576
};

enum {
    TY_VOID = 0, TY_CHAR, TY_UCHAR, TY_SHORT, TY_USHORT,
    TY_INT, TY_UINT, TY_LONG, TY_ULONG,
    TY_LONGLONG, TY_ULONGLONG, TY_FLOAT, TY_DOUBLE,
    TY_PTR, TY_ARRAY, TY_FUNC, TY_STRUCT, TY_UNION, TY_ENUM
};

enum {
    ND_NUM = 1, ND_STR, ND_CHAR_LIT, ND_FLIT,
    ND_VAR, ND_ASSIGN,
    ND_ADD, ND_SUB, ND_MUL, ND_DIV, ND_MOD,
    ND_EQ, ND_NE, ND_LT, ND_LE, ND_GT, ND_GE,
    ND_LAND, ND_LOR, ND_LNOT,
    ND_BAND, ND_BOR, ND_BXOR, ND_BNOT,
    ND_SHL, ND_SHR,
    ND_FADD, ND_FSUB, ND_FMUL, ND_FDIV,
    ND_NEG, ND_ADDR, ND_DEREF,
    ND_CALL, ND_RETURN, ND_BLOCK,
    ND_IF, ND_WHILE, ND_FOR, ND_DO_WHILE,
    ND_BREAK, ND_CONTINUE, ND_GOTO, ND_GOTO_COMPUTED, ND_LABEL,
    ND_SWITCH, ND_CASE, ND_DEFAULT,
    ND_CAST, ND_SIZEOF, ND_VA_ARG, ND_ADDR_LABEL,
    ND_MEMBER, ND_PRE_INC, ND_PRE_DEC,
    ND_POST_INC, ND_POST_DEC,
    ND_TERNARY,
    ND_COMMA_EXPR,
    ND_FUNC_DEF, ND_GLOBAL_VAR,
    ND_COMPOUND_ASSIGN,
    ND_INIT_LIST,
    ND_ASM,
    ND_NOP
};

typedef struct Type Type;
typedef struct Node Node;
typedef struct Symbol Symbol;
typedef struct Scope Scope;
typedef struct Compiler Compiler;
typedef struct ArenaBlock ArenaBlock;
typedef struct StringEntry StringEntry;
typedef struct StructField StructField;
typedef struct FuncParams FuncParams;

struct ArenaBlock {
    char *data;
    int pos;
    int cap;
    ArenaBlock *next;
};

struct StructField {
    char name[MAX_IDENT];
    Type *type;
    int offset;
    int is_bitfield;
    int bit_offset;
    int bit_size;
    StructField *next;
};

struct Type {
    unsigned long long magic;
    unsigned long long alloc_id;
    int kind;
    int size;
    int align;
    Type *base;
    int array_len;
    Type *ret;
    Type **params;
    int num_params;
    int is_variadic;
    char tag[MAX_IDENT];
    StructField *fields;
    int is_complete;
    int is_packed;
    int explicit_align;
    int is_tbfp;
};

struct StringEntry {
    char *data;
    int len;
    int label_id;
};

struct Symbol {
    char name[MAX_IDENT];
    Type *type;
    int is_local;
    int is_global;
    int is_typedef;
    int is_enum_const;
    long long enum_val;
    int stack_offset;
    char asm_name[MAX_IDENT];
    char *assigned_reg;
    int live_start;
    int live_end;
    Symbol *next;
};

struct Scope {
    Symbol *symbols;
    Scope *parent;
};

struct FuncParams {
    char names[MAX_PARAMS][MAX_IDENT];
    Type *types[MAX_PARAMS];
};

struct Node {
    unsigned long long magic;
    unsigned long long alloc_id;
    int kind;
    int line;
    Type *type;
    long long int_val;
    double f_val;
    int str_id;
    char name[MAX_IDENT];
    Symbol *sym;
    Node *lhs;
    Node *rhs;
    char func_name[MAX_IDENT];
    Node **args;
    int num_args;
    Node *cond;
    Node *then_body;
    Node *else_body;
    Node *init;
    Node *inc;
    Node **stmts;
    int num_stmts;
    char func_def_name[MAX_IDENT];
    Type *func_type;
    struct FuncParams *func_params;
    int num_params;
    Node *body;
    int stack_size;
    Type *param_types[MAX_PARAMS];
    char param_names_buf[MAX_PARAMS][MAX_IDENT];
    char member_name[MAX_IDENT];
    int member_offset;
    int member_size;
    Node **cases;
    int num_cases;
    Node *default_case;
    long long case_val;
    Node *case_body;
    char label_name[MAX_IDENT];
    int compound_op;
    Type *cast_type;
    int is_static;
    int is_extern;
    Node *initializer;
    int is_bitfield;
    int bit_offset;
    int bit_size;
    char *asm_string;
    Node *next;
};

struct Compiler {
    int verbose;
    char *source;
    int source_len;
    int pos;
    char *filename;
    int tk;
    long long tk_val;
    double tk_fval;
    char tk_text[MAX_IDENT];
    char tk_str[MAX_STR];
    int tk_str_len;
    int tk_line;
    int tk_col;
    int has_peek;
    int peek_tk;
    long long peek_val;
    double peek_fval;
    char peek_text[MAX_IDENT];
    char peek_str[MAX_STR];
    int peek_str_len;
    int peek_line;
    int peek_col;
    int line;
    int col;
    Type *ty_void;
    Type *ty_char;
    Type *ty_uchar;
    Type *ty_short;
    Type *ty_ushort;
    Type *ty_int;
    Type *ty_uint;
    Type *ty_long;
    Type *ty_ulong;
    Type *ty_longlong;
    Type *ty_ulonglong;
    Type *ty_float;
    Type *ty_double;
    Scope *current_scope;
    StringEntry strings[MAX_STRINGS];
    int num_strings;
    Type *structs[MAX_STRUCTS];
    int num_structs;
    Node *globals[MAX_GLOBALS];
    int num_globals;
    FILE *out;
    int label_count;
    int str_label_count;
    int stack_depth;
    int break_label;
    int continue_label;
    int switch_end_label;
    char current_func[MAX_IDENT];
    int func_end_label;
    ArenaBlock arena;
    int errors;
    int local_offset;
    int current_is_static;
    int pending_packed;
    int pending_aligned_n;
    int pending_tbfp;
    int debug_abi_classes;
    int abi_scratch_offset;
    int sret_offset;
    int used_regs_mask;
    int is_forced_mask;
};

/* Standalone SHA-256 freestanding implementation */
typedef struct {
    uint8_t data[64];
    uint32_t datalen;
    unsigned long long bitlen;
    uint32_t state[8];
} ZccSHA256_CTX;

#define ROTRIGHT(x,b) (((x) >> (b)) | ((x) << (32-(b))))
#define CH(x,y,z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x,y,z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x) (ROTRIGHT(x,2) ^ ROTRIGHT(x,13) ^ ROTRIGHT(x,22))
#define EP1(x) (ROTRIGHT(x,6) ^ ROTRIGHT(x,11) ^ ROTRIGHT(x,25))
#define SIG0(x) (ROTRIGHT(x,7) ^ ROTRIGHT(x,18) ^ ((x) >> 3))
#define SIG1(x) (ROTRIGHT(x,17) ^ ROTRIGHT(x,19) ^ ((x) >> 10))

static const uint32_t k_sha256[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

static void zcc_sha256_transform(ZccSHA256_CTX *ctx, const uint8_t data[]) {
    uint32_t a, b, c, d, e, f, g, h, i, j, t1, t2, m[64];
    for (i = 0, j = 0; i < 16; ++i, j += 4)
        m[i] = (data[j] << 24) | (data[j + 1] << 16) | (data[j + 2] << 8) | (data[j + 3]);
    for ( ; i < 64; ++i)
        m[i] = SIG1(m[i - 2]) + m[i - 7] + SIG0(m[i - 15]) + m[i - 16];
    a = ctx->state[0]; b = ctx->state[1]; c = ctx->state[2]; d = ctx->state[3];
    e = ctx->state[4]; f = ctx->state[5]; g = ctx->state[6]; h = ctx->state[7];
    for (i = 0; i < 64; ++i) {
        t1 = h + EP1(e) + CH(e, f, g) + k_sha256[i] + m[i];
        t2 = EP0(a) + MAJ(a, b, c);
        h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c; ctx->state[3] += d;
    ctx->state[4] += e; ctx->state[5] += f; ctx->state[6] += g; ctx->state[7] += h;
}

static void zcc_sha256_init(ZccSHA256_CTX *ctx) {
    ctx->datalen = 0; ctx->bitlen = 0;
    ctx->state[0] = 0x6a09e667; ctx->state[1] = 0xbb67ae85; ctx->state[2] = 0x3c6ef372; ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f; ctx->state[5] = 0x9b05688c; ctx->state[6] = 0x1f83d9ab; ctx->state[7] = 0x5be0cd19;
}

static void zcc_sha256_update(ZccSHA256_CTX *ctx, const uint8_t data[], size_t len) {
    for (size_t i = 0; i < len; ++i) {
        ctx->data[ctx->datalen] = data[i];
        ctx->datalen++;
        if (ctx->datalen == 64) {
            zcc_sha256_transform(ctx, ctx->data);
            ctx->bitlen += 512;
            ctx->datalen = 0;
        }
    }
}

static void zcc_sha256_final(ZccSHA256_CTX *ctx, uint8_t hash[]) {
    uint32_t i = ctx->datalen;
    if (ctx->datalen < 56) {
        ctx->data[i++] = 0x80;
        while (i < 56) ctx->data[i++] = 0x00;
    } else {
        ctx->data[i++] = 0x80;
        while (i < 64) ctx->data[i++] = 0x00;
        zcc_sha256_transform(ctx, ctx->data);
        memset(ctx->data, 0, 56);
    }
    ctx->bitlen += ctx->datalen * 8;
    ctx->data[63] = ctx->bitlen;
    ctx->data[62] = ctx->bitlen >> 8;
    ctx->data[61] = ctx->bitlen >> 16;
    ctx->data[60] = ctx->bitlen >> 24;
    ctx->data[59] = ctx->bitlen >> 32;
    ctx->data[58] = ctx->bitlen >> 40;
    ctx->data[57] = ctx->bitlen >> 48;
    ctx->data[56] = ctx->bitlen >> 56;
    zcc_sha256_transform(ctx, ctx->data);
    for (i = 0; i < 4; ++i) {
        hash[i]      = (ctx->state[0] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 4]  = (ctx->state[1] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 8]  = (ctx->state[2] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 12] = (ctx->state[3] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 16] = (ctx->state[4] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 20] = (ctx->state[5] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 24] = (ctx->state[6] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 28] = (ctx->state[7] >> (24 - i * 8)) & 0x000000ff;
    }
}

/* ================================================================= */
/* 🔱 IEEE-754 FP32-to-FP16 CONVERTER                               */
/* ================================================================= */

uint16_t fp32_to_fp16(float val) {
    union {
        float f;
        uint32_t u;
    } u;
    u.f = val;
    uint32_t f32 = u.u;
    
    uint32_t sign = (f32 >> 16) & 0x8000;
    int32_t exponent = ((f32 >> 23) & 0xFF) - 127;
    uint32_t mantissa = f32 & 0x007FFFFF;
    
    uint16_t f16 = 0;
    if (((f32 >> 23) & 0xFF) == 0xFF) {
        /* NaN or Inf */
        f16 = sign | 0x7C00 | (mantissa ? 0x0200 : 0);
    } else if (exponent > 15) {
        /* Overflow -> Inf */
        f16 = sign | 0x7C00;
    } else if (exponent < -24) {
        /* Underflow -> 0 */
        f16 = sign;
    } else if (exponent < -14) {
        /* Denormal */
        uint32_t m = mantissa | 0x00800000;
        int32_t shift = -14 - exponent;
        f16 = (uint16_t)(sign | (m >> (shift + 13)));
    } else {
        /* Normal */
        f16 = (uint16_t)(sign | ((exponent + 15) << 10) | (mantissa >> 13));
    }
    return f16;
}

/* ================================================================= */
/* 🔱 RECURSIVE AST FLOAT EXPRESSION EVALUATOR                       */
/* ================================================================= */

static double eval_float_expr(Node *n) {
    if (!n) return 0.0;
    if (n->kind == ND_FLIT) {
        return n->f_val;
    }
    if (n->kind == ND_NUM) {
        return (double)n->int_val;
    }
    if (n->kind == ND_CAST) {
        return eval_float_expr(n->lhs);
    }
    if (n->kind == ND_NEG) {
        return -eval_float_expr(n->lhs);
    }
    return 0.0;
}

static void extract_init_list(Node *init, float *out_data, int *out_idx, int max_elements) {
    if (!init) return;
    if (init->kind == ND_INIT_LIST) {
        int i;
        for (i = 0; i < init->num_args; i++) {
            extract_init_list(init->args[i], out_data, out_idx, max_elements);
        }
    } else {
        if (*out_idx < max_elements) {
            double v = eval_float_expr(init);
            out_data[(*out_idx)++] = (float)v;
        }
    }
}

/* ================================================================= */
/* 🔱 GGUF SERIALIZATION IMPLEMENTATION                             */
/* ================================================================= */

/* Padding helper */
static size_t write_align_padding(FILE *f, size_t current_offset, size_t alignment) {
    size_t aligned_offset = (current_offset + alignment - 1) & ~(alignment - 1);
    size_t pad_bytes = aligned_offset - current_offset;
    if (pad_bytes > 0) {
        unsigned char pad[64];
        memset(pad, 0, sizeof(pad));
        fwrite(pad, 1, pad_bytes, f);
    }
    return aligned_offset;
}

/* Write GGUF string (length prefix + characters) */
static void write_gguf_string(FILE *f, const char *str, size_t *offset) {
    uint64_t len = strlen(str);
    fwrite(&len, sizeof(uint64_t), 1, f);
    fwrite(str, 1, len, f);
    *offset += sizeof(uint64_t) + len;
}

int zcc_emit_gguf(void *cc_ptr, const char *out_path, int quantize_type) {
    Compiler *cc = (Compiler *)cc_ptr;
    FILE *f = NULL;
    size_t file_offset = 0;
    size_t i, j;
    size_t num_tensors = 0;

    f = fopen(out_path, "wb");
    if (!f) {
        fprintf(stderr, "zcc: cannot open GGUF output file '%s'\n", out_path);
        return 1;
    }

    /* 1. Count compiled global float/double array tensors */
    for (i = 0; i < (size_t)cc->num_globals; i++) {
        Node *g = cc->globals[i];
        if (g && g->kind == ND_GLOBAL_VAR && g->type && g->type->kind == TY_ARRAY) {
            Type *elem_type = g->type->base;
            if (elem_type && (elem_type->kind == TY_FLOAT || elem_type->kind == TY_DOUBLE)) {
                num_tensors++;
            }
        }
    }

    /* 2. Write GGUF Header */
    gguf_header_t header;
    header.magic = GGUF_MAGIC;
    header.version = 3;
    header.tensor_count = num_tensors;
    header.metadata_kv_count = 3; /* general.architecture, general.name, and zcc.signature */

    fwrite(&header, sizeof(gguf_header_t), 1, f);
    file_offset += sizeof(gguf_header_t);

    /* 3. Write GGUF Metadata Key-Values */
    /* general.architecture (string) */
    write_gguf_string(f, "general.architecture", &file_offset);
    uint32_t val_type = GGUF_VALUE_TYPE_STRING;
    fwrite(&val_type, sizeof(uint32_t), 1, f);
    file_offset += sizeof(uint32_t);
    write_gguf_string(f, "zcc_prime", &file_offset);

    /* general.name (string) */
    write_gguf_string(f, "general.name", &file_offset);
    fwrite(&val_type, sizeof(uint32_t), 1, f);
    file_offset += sizeof(uint32_t);
    write_gguf_string(f, cc->filename ? cc->filename : "zcc_tensor_pipeline", &file_offset);

    /* Placeholder for zcc.signature (string) */
    long sig_key_pos = ftell(f);
    write_gguf_string(f, "zcc.signature", &file_offset);
    fwrite(&val_type, sizeof(uint32_t), 1, f);
    file_offset += sizeof(uint32_t);
    long sig_val_pos = ftell(f);
    write_gguf_string(f, "0000000000000000000000000000000000000000000000000000000000000000", &file_offset);

    /* 4. Write Tensor Info Meta Records */
    gguf_tensor_info_t *t_infos = (gguf_tensor_info_t *)malloc(sizeof(gguf_tensor_info_t) * num_tensors);
    float **t_raw_data = (float **)malloc(sizeof(float *) * num_tensors);
    size_t *t_lengths = (size_t *)malloc(sizeof(size_t) * num_tensors);
    size_t tensor_idx = 0;

    size_t tensor_data_offset = 0;

    for (i = 0; i < (size_t)cc->num_globals; i++) {
        Node *g = cc->globals[i];
        if (g && g->kind == ND_GLOBAL_VAR && g->type && g->type->kind == TY_ARRAY) {
            Type *elem_type = g->type->base;
            if (elem_type && (elem_type->kind == TY_FLOAT || elem_type->kind == TY_DOUBLE)) {
                /* Determine dimensions recursively */
                uint32_t dims[4];
                uint32_t n_dims = 0;
                Type *curr = g->type;
                size_t total_elements = 1;
                while (curr && curr->kind == TY_ARRAY && n_dims < 4) {
                    dims[n_dims++] = (uint32_t)curr->array_len;
                    total_elements *= curr->array_len;
                    curr = curr->base;
                }

                t_lengths[tensor_idx] = total_elements;
                t_raw_data[tensor_idx] = (float *)malloc(sizeof(float) * total_elements);
                memset(t_raw_data[tensor_idx], 0, sizeof(float) * total_elements);

                /* Extract raw float values from variable initializer list */
                int ext_idx = 0;
                if (g->initializer) {
                    extract_init_list(g->initializer, t_raw_data[tensor_idx], &ext_idx, (int)total_elements);
                }

                /* Populate GGUF Tensor Info structure */
                gguf_tensor_info_t *info = &t_infos[tensor_idx];
                info->name_len = strlen(g->name);
                info->name = g->name;
                info->n_dimensions = n_dims;
                
                /* GGUF dimension mapping in reverse order (column-major GGML format) */
                for (j = 0; j < n_dims; j++) {
                    info->dimensions[j] = dims[n_dims - 1 - j];
                }

                /* Quantization numeric identifier mapping */
                if (quantize_type == 1) {
                    info->type = GGML_TYPE_F16;
                } else if (quantize_type == 2) {
                    info->type = GGML_TYPE_Q4_0;
                } else if (quantize_type == 3) {
                    info->type = GGML_TYPE_Q8_0;
                } else {
                    info->type = GGML_TYPE_F32;
                }

                /* Alignment offset computation */
                size_t tensor_bytes = 0;
                if (quantize_type == 1) {
                    tensor_bytes = total_elements * 2;
                } else if (quantize_type == 2) {
                    /* Q4_0 packing: 32 element blocks mapped to 18 bytes */
                    size_t num_blocks = (total_elements + 31) / 32;
                    tensor_bytes = num_blocks * 18;
                } else if (quantize_type == 3) {
                    /* Q8_0 packing: 32 element blocks mapped to 34 bytes */
                    size_t num_blocks = (total_elements + 31) / 32;
                    tensor_bytes = num_blocks * 34;
                } else {
                    tensor_bytes = total_elements * 4;
                }

                /* Alignment alignment: aligned to 32-byte boundary */
                tensor_data_offset = (tensor_data_offset + 31) & ~31;
                info->offset = tensor_data_offset;
                tensor_data_offset += tensor_bytes;

                tensor_idx++;
            }
        }
    }

    /* Write out all tensor records */
    for (i = 0; i < num_tensors; i++) {
        gguf_tensor_info_t *info = &t_infos[i];
        write_gguf_string(f, info->name, &file_offset);
        fwrite(&info->n_dimensions, sizeof(uint32_t), 1, f);
        file_offset += sizeof(uint32_t);
        fwrite(info->dimensions, sizeof(uint64_t), info->n_dimensions, f);
        file_offset += sizeof(uint64_t) * info->n_dimensions;
        fwrite(&info->type, sizeof(uint32_t), 1, f);
        file_offset += sizeof(uint32_t);
        fwrite(&info->offset, sizeof(uint64_t), 1, f);
        file_offset += sizeof(uint64_t);
    }

    /* 5. Pad metadata list to 32-byte boundary before raw tensor bytes */
    size_t pre_data_offset = file_offset;
    file_offset = write_align_padding(f, file_offset, 32);
    size_t pad_size = file_offset - pre_data_offset;

    long tensor_data_start_pos = ftell(f);

    /* 6. Perform Quantization and Write Raw Tensor Segment */
    ZccSHA256_CTX sha_ctx;
    zcc_sha256_init(&sha_ctx);

    for (i = 0; i < num_tensors; i++) {
        size_t len = t_lengths[i];
        float *raw = t_raw_data[i];
        gguf_tensor_info_t *info = &t_infos[i];

        /* Seek and align tensor data start positions */
        long curr_t_pos = ftell(f);
        long aligned_t_pos = (curr_t_pos + 31) & ~31;
        long t_pad = aligned_t_pos - curr_t_pos;
        if (t_pad > 0) {
            unsigned char pad[64];
            memset(pad, 0, sizeof(pad));
            fwrite(pad, 1, t_pad, f);
            zcc_sha256_update(&sha_ctx, pad, t_pad);
        }

        /* Verify actual offset matches header */
        size_t actual_rel_offset = ftell(f) - tensor_data_start_pos;
        if (actual_rel_offset != info->offset) {
            fprintf(stderr, "zcc: GGUF offset mismatch on '%s' (expected %lu, got %lu)\n", info->name, (unsigned long)info->offset, (unsigned long)actual_rel_offset);
        }

        if (quantize_type == 1) {
            /* F16 Quantization */
            uint16_t *f16_buf = (uint16_t *)malloc(sizeof(uint16_t) * len);
            for (j = 0; j < len; j++) {
                f16_buf[j] = fp32_to_fp16(raw[j]);
            }
            fwrite(f16_buf, 2, len, f);
            zcc_sha256_update(&sha_ctx, (uint8_t *)f16_buf, len * 2);
            free(f16_buf);
        } else if (quantize_type == 2) {
            /* Q4_0 Quantization (32 element blocks mapped to 18 bytes) */
            size_t num_blocks = (len + 31) / 32;
            uint8_t *block_buf = (uint8_t *)malloc(18 * num_blocks);
            memset(block_buf, 0, 18 * num_blocks);

            for (j = 0; j < num_blocks; j++) {
                float block[32];
                size_t block_len = 0;
                size_t k;

                for (k = 0; k < 32; k++) {
                    size_t idx = j * 32 + k;
                    if (idx < len) {
                        block[k] = raw[idx];
                        block_len++;
                    } else {
                        block[k] = 0.0f;
                    }
                }

                /* Find peak absolute value */
                float max_abs = 0.0f;
                for (k = 0; k < 32; k++) {
                    float abs_v = (float)fabs(block[k]);
                    if (abs_v > max_abs) max_abs = abs_v;
                }

                float d = max_abs / 7.0f;
                if (d == 0.0f) d = 1.0f;
                float id = 1.0f / d;

                /* Convert scale to half FP16 format */
                uint16_t d_fp16 = fp32_to_fp16(d);
                memcpy(block_buf + j * 18, &d_fp16, 2);

                /* Pack 4-bit nibbles */
                uint8_t *nibbles = block_buf + j * 18 + 2;
                for (k = 0; k < 16; k++) {
                    float val0 = block[k];
                    float val1 = block[k + 16];

                    int v0 = (int)roundf(val0 * id);
                    int v1 = (int)roundf(val1 * id);

                    /* Clamp to range [-8, 7] */
                    if (v0 < -8) v0 = -8;
                    if (v0 > 7) v0 = 7;
                    if (v1 < -8) v1 = -8;
                    if (v1 > 7) v1 = 7;

                    /* Shift to [0, 15] for storing */
                    uint8_t nib0 = (uint8_t)(v0 + 8) & 0x0F;
                    uint8_t nib1 = (uint8_t)(v1 + 8) & 0x0F;

                    nibbles[k] = (uint8_t)(nib0 | (nib1 << 4));
                }
            }
            fwrite(block_buf, 1, 18 * num_blocks, f);
            zcc_sha256_update(&sha_ctx, block_buf, 18 * num_blocks);
            free(block_buf);
        } else if (quantize_type == 3) {
            /* Q8_0 Quantization (32 element blocks mapped to 34 bytes) */
            size_t num_blocks = (len + 31) / 32;
            uint8_t *block_buf = (uint8_t *)malloc(34 * num_blocks);
            memset(block_buf, 0, 34 * num_blocks);

            for (j = 0; j < num_blocks; j++) {
                float block[32];
                size_t k;

                for (k = 0; k < 32; k++) {
                    size_t idx = j * 32 + k;
                    if (idx < len) {
                        block[k] = raw[idx];
                    } else {
                        block[k] = 0.0f;
                    }
                }

                /* Find peak absolute value */
                float max_abs = 0.0f;
                for (k = 0; k < 32; k++) {
                    float abs_v = (float)fabs(block[k]);
                    if (abs_v > max_abs) max_abs = abs_v;
                }

                float d = max_abs / 127.0f;
                if (d == 0.0f) d = 1.0f;
                float id = 1.0f / d;

                /* Convert scale to half FP16 format */
                uint16_t d_fp16 = fp32_to_fp16(d);
                memcpy(block_buf + j * 34, &d_fp16, 2);

                /* Pack 8-bit weights */
                int8_t *qs = (int8_t *)(block_buf + j * 34 + 2);
                for (k = 0; k < 32; k++) {
                    int v = (int)roundf(block[k] * id);
                    if (v < -128) v = -128;
                    if (v > 127) v = 127;
                    qs[k] = (int8_t)v;
                }
            }
            fwrite(block_buf, 1, 34 * num_blocks, f);
            zcc_sha256_update(&sha_ctx, block_buf, 34 * num_blocks);
            free(block_buf);
        } else {
            /* FP32 Direct Packing */
            fwrite(raw, 4, len, f);
            zcc_sha256_update(&sha_ctx, (uint8_t *)raw, len * 4);
        }
    }

    /* 7. Finalize SHA-256 cryptographic signature */
    uint8_t final_hash[32];
    zcc_sha256_final(&sha_ctx, final_hash);

    char sig_hex[65];
    for (i = 0; i < 32; i++) {
        sprintf(sig_hex + i * 2, "%02x", final_hash[i]);
    }
    sig_hex[64] = '\0';

    /* Rewind and write real cryptographic attestation signature back into placeholder metadata */
    fseek(f, sig_val_pos, SEEK_SET);
    write_gguf_string(f, sig_hex, &pre_data_offset);

    fclose(f);

    /* Clean resources */
    for (i = 0; i < num_tensors; i++) {
        free(t_raw_data[i]);
    }
    free(t_raw_data);
    free(t_lengths);
    free(t_infos);

    printf("[ZCC-GGUF] Successfully serialized %lu tensors. Sovereign attestation hash: %s\n", (unsigned long)num_tensors, sig_hex);
    return 0;
}
