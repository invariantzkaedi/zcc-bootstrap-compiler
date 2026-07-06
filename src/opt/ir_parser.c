#include "../prelude.h"
#include "zcc_ir_verify.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#define MAX_REGS_PER_FUNC 4096
#define MAX_FUNCS_PER_MOD 100

typedef struct {
    char name[64];
    RegID id;
    char type[16];
    bool is_param;
} ParserReg;

typedef struct {
    ParserReg regs[MAX_REGS_PER_FUNC];
    int n_regs;
} ParserFuncCtx;

// Global type map for the verifier
char g_reg_types[MAX_INSTRS][16];
bool g_reg_is_param[MAX_INSTRS];
char g_fn_ret_type[16];

typedef enum {
    TOK_EOF,
    TOK_FUNC,
    TOK_RET,
    TOK_CONST,
    TOK_PHI,
    TOK_JMP,
    TOK_BR,
    TOK_ADD,
    TOK_SUB,
    TOK_MUL,
    TOK_DIV,
    TOK_MOD,
    TOK_AND,
    TOK_OR,
    TOK_XOR,
    TOK_NOT,
    TOK_NEG,
    TOK_SHL,
    TOK_SHR,
    TOK_LOAD,
    TOK_STORE,
    TOK_IDENT,    // %name or @name or bbN or types
    TOK_NUM,      // integer constant
    TOK_ARROW,    // ->
    TOK_LBRACE,   // {
    TOK_RBRACE,   // }
    TOK_LPAREN,   // (
    TOK_RPAREN,   // )
    TOK_LBRACKET, // [
    TOK_RBRACKET, // ]
    TOK_COLON,    // :
    TOK_COMMA,    // ,
    TOK_EQ,       // =
} TokenKind;

typedef struct {
    TokenKind kind;
    char text[64];
    long long value;
    int line;
} Token;

static void next_token(const char **p, Token *tok, int *line) {
    while (**p) {
        if (**p == '\n') {
            (*line)++;
            (*p)++;
        } else if (isspace(**p)) {
            (*p)++;
        } else if (**p == ';') {
            while (**p && **p != '\n') (*p)++;
        } else {
            break;
        }
    }

    tok->line = *line;
    tok->text[0] = '\0';
    tok->value = 0;

    if (!**p) {
        tok->kind = TOK_EOF;
        return;
    }

    char c = **p;
    if (c == '-' && (*p)[1] == '>') {
        tok->kind = TOK_ARROW;
        strcpy(tok->text, "->");
        (*p) += 2;
        return;
    }

    if (c == '{') { tok->kind = TOK_LBRACE; strcpy(tok->text, "{"); (*p)++; return; }
    if (c == '}') { tok->kind = TOK_RBRACE; strcpy(tok->text, "}"); (*p)++; return; }
    if (c == '(') { tok->kind = TOK_LPAREN; strcpy(tok->text, "("); (*p)++; return; }
    if (c == ')') { tok->kind = TOK_RPAREN; strcpy(tok->text, ")"); (*p)++; return; }
    if (c == '[') { tok->kind = TOK_LBRACKET; strcpy(tok->text, "["); (*p)++; return; }
    if (c == ']') { tok->kind = TOK_RBRACKET; strcpy(tok->text, "]"); (*p)++; return; }
    if (c == ':') { tok->kind = TOK_COLON; strcpy(tok->text, ":"); (*p)++; return; }
    if (c == ',') { tok->kind = TOK_COMMA; strcpy(tok->text, ","); (*p)++; return; }
    if (c == '=') { tok->kind = TOK_EQ; strcpy(tok->text, "="); (*p)++; return; }

    if (c == '-' || isdigit(c)) {
        tok->kind = TOK_NUM;
        char *end;
        tok->value = strtoll(*p, &end, 10);
        int len = end - *p;
        strncpy(tok->text, *p, len);
        tok->text[len] = '\0';
        *p = end;
        return;
    }

    if (isalpha(c) || c == '%' || c == '@' || c == '_' || c == '.') {
        int len = 0;
        while (**p && (isalnum(**p) || **p == '%' || **p == '@' || **p == '_' || **p == '.')) {
            if (len < 63) {
                tok->text[len++] = **p;
            }
            (*p)++;
        }
        tok->text[len] = '\0';

        if (strcmp(tok->text, "func") == 0) tok->kind = TOK_FUNC;
        else if (strcmp(tok->text, "ret") == 0) tok->kind = TOK_RET;
        else if (strcmp(tok->text, "const") == 0) tok->kind = TOK_CONST;
        else if (strcmp(tok->text, "phi") == 0) tok->kind = TOK_PHI;
        else if (strcmp(tok->text, "jmp") == 0) tok->kind = TOK_JMP;
        else if (strcmp(tok->text, "br") == 0) tok->kind = TOK_BR;
        else if (strcmp(tok->text, "add") == 0) tok->kind = TOK_ADD;
        else if (strcmp(tok->text, "sub") == 0) tok->kind = TOK_SUB;
        else if (strcmp(tok->text, "mul") == 0) tok->kind = TOK_MUL;
        else if (strcmp(tok->text, "div") == 0) tok->kind = TOK_DIV;
        else if (strcmp(tok->text, "mod") == 0) tok->kind = TOK_MOD;
        else if (strcmp(tok->text, "and") == 0) tok->kind = TOK_AND;
        else if (strcmp(tok->text, "or") == 0) tok->kind = TOK_OR;
        else if (strcmp(tok->text, "xor") == 0) tok->kind = TOK_XOR;
        else if (strcmp(tok->text, "not") == 0) tok->kind = TOK_NOT;
        else if (strcmp(tok->text, "neg") == 0) tok->kind = TOK_NEG;
        else if (strcmp(tok->text, "load") == 0) tok->kind = TOK_LOAD;
        else if (strcmp(tok->text, "store") == 0) tok->kind = TOK_STORE;
        else tok->kind = TOK_IDENT;
        return;
    }

    tok->kind = TOK_IDENT;
    tok->text[0] = c;
    tok->text[1] = '\0';
    (*p)++;
}

static RegID get_or_create_reg(ParserFuncCtx *ctx, const char *name, const char *type, bool is_param) {
    for (int i = 0; i < ctx->n_regs; i++) {
        if (strcmp(ctx->regs[i].name, name) == 0) {
            if (type && ctx->regs[i].type[0] == '\0') {
                strcpy(ctx->regs[i].type, type);
            }
            return ctx->regs[i].id;
        }
    }
    if (ctx->n_regs >= MAX_REGS_PER_FUNC) {
        fprintf(stderr, "Too many registers in parser\n");
        exit(1);
    }
    ParserReg *r = &ctx->regs[ctx->n_regs++];
    strcpy(r->name, name);
    r->id = ctx->n_regs; // RegID starts at 1
    if (type) strcpy(r->type, type);
    else r->type[0] = '\0';
    r->is_param = is_param;
    return r->id;
}

static BlockID parse_block_id(const char *name) {
    if (strncmp(name, "bb", 2) == 0) {
        return (BlockID)atoi(name + 2);
    }
    return NO_BLOCK;
}

static void init_reg_types(ParserFuncCtx *ctx) {
    memset(g_reg_types, 0, sizeof(g_reg_types));
    memset(g_reg_is_param, 0, sizeof(g_reg_is_param));
    for (int i = 0; i < ctx->n_regs; i++) {
        RegID rid = ctx->regs[i].id;
        strcpy(g_reg_types[rid], ctx->regs[i].type);
        g_reg_is_param[rid] = ctx->regs[i].is_param;
    }
}

Module *parse_ir_module(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "Cannot open file %s\n", filename);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = malloc(size + 1);
    fread(buf, 1, size, f);
    buf[size] = '\0';
    fclose(f);

    const char *p = buf;
    int line = 1;

    Module *mod = calloc(1, sizeof(Module));
    Token tok;

    next_token(&p, &tok, &line);
    while (tok.kind == TOK_FUNC) {
        if (mod->n_funcs >= MAX_FUNCS_PER_MOD) {
            fprintf(stderr, "Too many functions in module\n");
            exit(1);
        }

        // Parse func header: func @name(type %arg0, ...) -> ret_type {
        next_token(&p, &tok, &line);
        if (tok.kind != TOK_IDENT || tok.text[0] != '@') {
            fprintf(stderr, "Line %d: Expected function name starting with @, got %s\n", tok.line, tok.text);
            exit(1);
        }

        char func_name[64];
        strcpy(func_name, tok.text + 1);

        Function *fn = calloc(1, sizeof(Function));
        mod->funcs[mod->n_funcs++] = fn;

        ParserFuncCtx fctx = {0};

        next_token(&p, &tok, &line);
        if (tok.kind != TOK_LPAREN) {
            fprintf(stderr, "Line %d: Expected '('\n", tok.line);
            exit(1);
        }

        next_token(&p, &tok, &line);
        while (tok.kind != TOK_RPAREN) {
            if (tok.kind != TOK_IDENT) {
                fprintf(stderr, "Line %d: Expected parameter type\n", tok.line);
                exit(1);
            }
            char ptype[16];
            strcpy(ptype, tok.text);

            next_token(&p, &tok, &line);
            if (tok.kind != TOK_IDENT || tok.text[0] != '%') {
                fprintf(stderr, "Line %d: Expected parameter register\n", tok.line);
                exit(1);
            }

            get_or_create_reg(&fctx, tok.text, ptype, true);

            next_token(&p, &tok, &line);
            if (tok.kind == TOK_COMMA) {
                next_token(&p, &tok, &line);
            }
        }

        next_token(&p, &tok, &line);
        if (tok.kind != TOK_ARROW) {
            fprintf(stderr, "Line %d: Expected '->'\n", tok.line);
            exit(1);
        }

        next_token(&p, &tok, &line);
        char ret_type[16];
        strcpy(ret_type, tok.text);
        strcpy(g_fn_ret_type, ret_type);
        strcpy(fn->name, func_name);
        strcpy(fn->ret_type, ret_type);

        next_token(&p, &tok, &line);
        if (tok.kind != TOK_LBRACE) {
            fprintf(stderr, "Line %d: Expected '{'\n", tok.line);
            exit(1);
        }

        BlockID cur_bb_id = NO_BLOCK;
        Block *cur_bb = NULL;

        next_token(&p, &tok, &line);
        while (tok.kind != TOK_RBRACE && tok.kind != TOK_EOF) {
            if (tok.kind == TOK_IDENT && tok.text[0] == 'b' && tok.text[1] == 'b') {
                // Basic block label: bbN:
                BlockID bid = parse_block_id(tok.text);
                next_token(&p, &tok, &line);
                if (tok.kind != TOK_COLON) {
                    fprintf(stderr, "Line %d: Expected ':' after block label\n", tok.line);
                    exit(1);
                }

                Block *bb = calloc(1, sizeof(Block));
                bb->id = bid;
                bb->reachable = true;
                sprintf(bb->name, "bb%d", bid);
                fn->blocks[bid] = bb;
                if (bid >= fn->n_blocks) {
                    fn->n_blocks = bid + 1;
                }

                cur_bb_id = bid;
                cur_bb = bb;

                next_token(&p, &tok, &line);
                continue;
            }

            // Must be an instruction in the current block
            if (!cur_bb) {
                fprintf(stderr, "Line %d: Instruction outside of basic block\n", tok.line);
                exit(1);
            }

            Instr *inst = calloc(1, sizeof(Instr));
            inst->line_no = tok.line;

            // Check if it defines a register: %dst = ...
            if (tok.kind == TOK_IDENT && tok.text[0] == '%') {
                char dst_name[64];
                strcpy(dst_name, tok.text);

                next_token(&p, &tok, &line);
                if (tok.kind != TOK_EQ) {
                    fprintf(stderr, "Line %d: Expected '=' after destination register %s, got %s\n", tok.line, dst_name, tok.text);
                    exit(1);
                }
                next_token(&p, &tok, &line);
                Token op_tok = tok;
                char predicate[16] = "";
                char type_str[16];

                if (strcmp(op_tok.text, "icmp") == 0) {
                    next_token(&p, &tok, &line); // predicate (eq, ne, etc.)
                    strcpy(predicate, tok.text);
                    next_token(&p, &tok, &line); // type string
                    strcpy(type_str, tok.text);
                } else {
                    next_token(&p, &tok, &line); // type string
                    strcpy(type_str, tok.text);
                }

                // Register the destination type
                RegID dst_reg;
                if (strcmp(op_tok.text, "icmp") == 0) {
                    dst_reg = get_or_create_reg(&fctx, dst_name, "", false);
                } else {
                    dst_reg = get_or_create_reg(&fctx, dst_name, type_str, false);
                }
                if (strcmp(type_str, "i32") == 0) inst->ir_type = IR_TY_I32;
                else if (strcmp(type_str, "u32") == 0) inst->ir_type = IR_TY_U32;
                else if (strcmp(type_str, "u64") == 0) inst->ir_type = IR_TY_U64;
                else if (strcmp(type_str, "i1") == 0) inst->ir_type = IR_TY_I32;
                else inst->ir_type = IR_TY_I64;
                inst->dst = dst_reg;

                next_token(&p, &tok, &line); // first operand or parameter list

                if (strcmp(op_tok.text, "const") == 0) {
                    inst->op = OP_CONST;
                    inst->imm = tok.value;
                    next_token(&p, &tok, &line);
                } else if (strcmp(op_tok.text, "phi") == 0) {
                    inst->op = OP_PHI;
                    while (tok.kind == TOK_LBRACKET) {
                        next_token(&p, &tok, &line); // bbN
                        BlockID pred_id = parse_block_id(tok.text);
                        next_token(&p, &tok, &line); // :
                        next_token(&p, &tok, &line); // %val
                        RegID val_reg = get_or_create_reg(&fctx, tok.text, NULL, false);

                        if (inst->n_phi < MAX_PHI_SOURCES) {
                            inst->phi[inst->n_phi].block = pred_id;
                            inst->phi[inst->n_phi].reg = val_reg;
                            inst->n_phi++;
                        }

                        next_token(&p, &tok, &line); // ]
                        next_token(&p, &tok, &line); // , or start of next token
                        if (tok.kind == TOK_COMMA) {
                            next_token(&p, &tok, &line);
                        }
                    }
                } else {
                    // Binary or unary operations
                    if (strcmp(op_tok.text, "add") == 0) inst->op = OP_ADD;
                    else if (strcmp(op_tok.text, "sub") == 0) inst->op = OP_SUB;
                    else if (strcmp(op_tok.text, "mul") == 0) inst->op = OP_MUL;
                    else if (strcmp(op_tok.text, "div") == 0 || strcmp(op_tok.text, "sdiv") == 0 || strcmp(op_tok.text, "udiv") == 0) inst->op = OP_DIV;
                    else if (strcmp(op_tok.text, "mod") == 0) inst->op = OP_MOD;
                    else if (strcmp(op_tok.text, "and") == 0) inst->op = OP_BAND;
                    else if (strcmp(op_tok.text, "or") == 0) inst->op = OP_BOR;
                    else if (strcmp(op_tok.text, "xor") == 0) inst->op = OP_BXOR;
                    else if (strcmp(op_tok.text, "not") == 0) inst->op = OP_BNOT;
                    else if (strcmp(op_tok.text, "neg") == 0) inst->op = OP_BNOT;
                    else if (strcmp(op_tok.text, "shl") == 0) inst->op = OP_SHL;
                    else if (strcmp(op_tok.text, "shr") == 0 || strcmp(op_tok.text, "ashr") == 0) inst->op = OP_SHR;
                    else if (strcmp(op_tok.text, "copy") == 0) inst->op = OP_COPY;
                    else if (strcmp(op_tok.text, "icmp") == 0) {
                        if (strcmp(predicate, "eq") == 0) inst->op = OP_EQ;
                        else if (strcmp(predicate, "ne") == 0) inst->op = OP_NE;
                        else if (strcmp(predicate, "lt") == 0) inst->op = OP_LT;
                        else if (strcmp(predicate, "le") == 0) inst->op = OP_LE;
                        else if (strcmp(predicate, "gt") == 0) inst->op = OP_GT;
                        else if (strcmp(predicate, "ge") == 0) inst->op = OP_GE;
                        else {
                            fprintf(stderr, "Line %d: Unknown icmp predicate %s\n", op_tok.line, predicate);
                            exit(1);
                        }
                    }
                    else {
                        fprintf(stderr, "Line %d: Unknown instruction opcode %s\n", op_tok.line, op_tok.text);
                        exit(1);
                    }

                    // Parse first src operand
                    if (tok.kind == TOK_IDENT && tok.text[0] == '%') {
                        inst->src[0] = get_or_create_reg(&fctx, tok.text, NULL, false);
                        inst->n_src = 1;
                    }

                    next_token(&p, &tok, &line);
                    if (tok.kind == TOK_COMMA) {
                        next_token(&p, &tok, &line);
                        if (tok.kind == TOK_IDENT && tok.text[0] == '%') {
                            inst->src[1] = get_or_create_reg(&fctx, tok.text, NULL, false);
                            inst->n_src = 2;
                        } else if (tok.kind == TOK_NUM) {
                            char dummy_const_name[64];
                            sprintf(dummy_const_name, "%%const_val_%lld", tok.value);
                            inst->src[1] = get_or_create_reg(&fctx, dummy_const_name, type_str, false);
                            inst->n_src = 2;
                        }
                        next_token(&p, &tok, &line);
                    }
                }
            } else {
                // Non-definition instructions: jmp, br, ret, store
                Token op_tok = tok;
                next_token(&p, &tok, &line);

                if (op_tok.kind == TOK_JMP || (op_tok.kind == TOK_BR && tok.text[0] == 'b' && tok.text[1] == 'b')) {
                    inst->op = OP_BR;
                    inst->src[0] = parse_block_id(tok.text);
                    inst->n_src = 1;
                    next_token(&p, &tok, &line);
                } else if (op_tok.kind == TOK_BR) {
                    inst->op = OP_CONDBR;
                    // br type %cond, bb1, bb2
                    char type_str[16];
                    strcpy(type_str, tok.text);
                    if (strcmp(type_str, "i1") == 0) {
                        inst->sbt_has_cast = true;
                    } else {
                        inst->sbt_has_cast = false;
                    }
                    next_token(&p, &tok, &line); // %cond
                    inst->src[0] = get_or_create_reg(&fctx, tok.text, type_str, false);
                    next_token(&p, &tok, &line); // ,
                    next_token(&p, &tok, &line); // bb1
                    inst->src[1] = parse_block_id(tok.text);
                    next_token(&p, &tok, &line); // ,
                    next_token(&p, &tok, &line); // bb2
                    inst->src[2] = parse_block_id(tok.text);
                    inst->n_src = 3;
                    next_token(&p, &tok, &line);
                } else if (op_tok.kind == TOK_RET) {
                    inst->op = OP_RET;
                    char type_str[16];
                    strcpy(type_str, tok.text);
                    next_token(&p, &tok, &line); // value or none
                    if (tok.kind == TOK_IDENT && tok.text[0] == '%') {
                        inst->src[0] = get_or_create_reg(&fctx, tok.text, type_str, false);
                        inst->n_src = 1;
                        next_token(&p, &tok, &line);
                    } else {
                        inst->n_src = 0;
                    }
                } else if (op_tok.kind == TOK_STORE) {
                    inst->op = OP_STORE;
                    char type_str[16];
                    strcpy(type_str, tok.text);
                    next_token(&p, &tok, &line); // value
                    inst->src[0] = get_or_create_reg(&fctx, tok.text, type_str, false);
                    next_token(&p, &tok, &line); // ,
                    next_token(&p, &tok, &line); // type_str ptr
                    next_token(&p, &tok, &line); // address register
                    inst->src[1] = get_or_create_reg(&fctx, tok.text, NULL, false);
                    inst->n_src = 2;
                    next_token(&p, &tok, &line);
                } else {
                    fprintf(stderr, "Line %d: Unknown instruction %s\n", op_tok.line, op_tok.text);
                    exit(1);
                }
            }

            // Append to current basic block
            if (cur_bb->tail) {
                cur_bb->tail->next = inst;
                inst->prev = cur_bb->tail;
                cur_bb->tail = inst;
            } else {
                cur_bb->head = inst;
                cur_bb->tail = inst;
            }
            cur_bb->n_instrs++;
        }

        // Finish parsing function body
        if (tok.kind != TOK_RBRACE) {
            fprintf(stderr, "Line %d: Expected '}' at end of function\n", tok.line);
            exit(1);
        }

        // Initialize verification globals for this function
        init_reg_types(&fctx);
        fn->n_regs = fctx.n_regs;

        // Build CFG successor/predecessor edges
        // (to be done for the function)
        for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
            Block *bb = fn->blocks[bi];
            if (!bb) continue;
            bb->n_succs = 0;
            bb->n_preds = 0;
        }

        for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
            Block *bb = fn->blocks[bi];
            if (!bb) continue;
            Instr *t = bb->tail;
            if (!t) continue;

            if (t->op == OP_BR) {
                BlockID dest = t->src[0];
                bb->succs[bb->n_succs++] = dest;
                if (dest < fn->n_blocks && fn->blocks[dest]) {
                    Block *dest_bb = fn->blocks[dest];
                    dest_bb->preds[dest_bb->n_preds++] = bi;
                }
            } else if (t->op == OP_CONDBR) {
                BlockID dest1 = t->src[1];
                BlockID dest2 = t->src[2];
                bb->succs[bb->n_succs++] = dest1;
                bb->succs[bb->n_succs++] = dest2;
                if (dest1 < fn->n_blocks && fn->blocks[dest1]) {
                    Block *dest_bb1 = fn->blocks[dest1];
                    dest_bb1->preds[dest_bb1->n_preds++] = bi;
                }
                if (dest2 < fn->n_blocks && fn->blocks[dest2]) {
                    Block *dest_bb2 = fn->blocks[dest2];
                    dest_bb2->preds[dest_bb2->n_preds++] = bi;
                }
            }
        }

        // Find entry and exit blocks
        fn->entry = 0;
        for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
            Block *bb = fn->blocks[bi];
            if (!bb) continue;
            if (bb->tail && bb->tail->op == OP_RET) {
                fn->exit = bi;
            }
        }

        next_token(&p, &tok, &line);
    }

    free(buf);
    return mod;
}

void free_ir_module(Module *m) {
    if (!m) return;
    for (int i = 0; i < m->n_funcs; i++) {
        Function *fn = m->funcs[i];
        if (!fn) continue;
        for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
            Block *bb = fn->blocks[bi];
            if (!bb) continue;
            Instr *it = bb->head;
            while (it) {
                Instr *next = it->next;
                free(it);
                it = next;
            }
            free(bb);
        }
        free(fn);
    }
    free(m);
}

void print_ir_instr(FILE *out, Instr *it) {
    if (it->dst != 0) {
        const char *ty_name = "i32";
        if (g_reg_types[it->dst][0] != '\0') {
            ty_name = g_reg_types[it->dst];
        } else {
            if (it->ir_type == IR_TY_I64 || it->ir_type == IR_TY_U64) ty_name = "i64";
            else if (it->ir_type == IR_TY_U32) ty_name = "u32";
            else ty_name = "i32";
        }
        
        fprintf(out, "  %%r%d = ", it->dst);
        if (it->op == OP_CONST) {
            fprintf(out, "const %s %lld\n", ty_name, (long long)it->imm);
        } else if (it->op == OP_COPY) {
            fprintf(out, "copy %s %%r%d\n", ty_name, it->src[0]);
        } else if (it->op == OP_PHI) {
            fprintf(out, "phi %s ", ty_name);
            for (uint32_t i = 0; i < it->n_phi; i++) {
                fprintf(out, "[bb%d: %%r%d]", it->phi[i].block, it->phi[i].reg);
                if (i < it->n_phi - 1) fprintf(out, ", ");
            }
            fprintf(out, "\n");
        } else {
            const char *op_name = "unknown";
            bool is_cmp = false;
            switch (it->op) {
                case OP_ADD: op_name = "add"; break;
                case OP_SUB: op_name = "sub"; break;
                case OP_MUL: op_name = "mul"; break;
                case OP_DIV:
                    if (it->sbt_has_cast) {
                        op_name = "udiv";
                    } else {
                        op_name = "sdiv";
                    }
                    break;
                case OP_MOD: op_name = "mod"; break;
                case OP_BAND: op_name = "and"; break;
                case OP_BOR: op_name = "or"; break;
                case OP_BXOR: op_name = "xor"; break;
                case OP_BNOT: op_name = "not"; break;
                case OP_SHL: op_name = "shl"; break;
                case OP_SHR: op_name = "shr"; break;
                case OP_EQ: op_name = "eq"; is_cmp = true; break;
                case OP_NE: op_name = "ne"; is_cmp = true; break;
                case OP_LT: op_name = "lt"; is_cmp = true; break;
                case OP_LE: op_name = "le"; is_cmp = true; break;
                case OP_GT: op_name = "gt"; is_cmp = true; break;
                case OP_GE: op_name = "ge"; is_cmp = true; break;
                default: op_name = "unknown"; break;
            }
            if (is_cmp) {
                fprintf(out, "icmp %s %s ", op_name, ty_name);
            } else {
                fprintf(out, "%s %s ", op_name, ty_name);
            }
            if (it->n_src > 0) {
                fprintf(out, "%%r%d", it->src[0]);
            }
            if (it->n_src > 1) {
                fprintf(out, ", %%r%d", it->src[1]);
            }
            fprintf(out, "\n");
        }
    } else {
        if (it->op == OP_BR) {
            fprintf(out, "  jmp bb%d\n", it->src[0]);
        } else if (it->op == OP_CONDBR) {
            const char *cond_ty = g_reg_types[it->src[0]][0] ? g_reg_types[it->src[0]] : "i1";
            fprintf(out, "  br %s %%r%d, bb%d, bb%d\n", cond_ty, it->src[0], it->src[1], it->src[2]);
        } else if (it->op == OP_RET) {
            if (it->n_src > 0) {
                const char *ret_ty = g_reg_types[it->src[0]][0] ? g_reg_types[it->src[0]] : "i32";
                fprintf(out, "  ret %s %%r%d\n", ret_ty, it->src[0]);
            } else {
                fprintf(out, "  ret void\n");
            }
        } else if (it->op == OP_STORE) {
            const char *val_ty = g_reg_types[it->src[0]][0] ? g_reg_types[it->src[0]] : "i32";
            fprintf(out, "  store %s %%r%d, ptr %%r%d\n", val_ty, it->src[0], it->src[1]);
        }
    }
}

void print_ir_function(FILE *out, Function *fn) {
    fprintf(out, "func @%s(", fn->name[0] ? fn->name : "func");
    int param_count = 0;
    for (int i = 1; i <= fn->n_regs; i++) {
        if (g_reg_is_param[i]) {
            if (param_count > 0) fprintf(out, ", ");
            fprintf(out, "%s %%r%d", g_reg_types[i][0] ? g_reg_types[i] : "i32", i);
            param_count++;
        }
    }
    fprintf(out, ") -> %s {\n", fn->ret_type[0] ? fn->ret_type : "i32");

    for (uint32_t bi = 0; bi < fn->n_blocks; bi++) {
        Block *bb = fn->blocks[bi];
        if (!bb) continue;
        if (bi > 0) fprintf(out, "\n");
        fprintf(out, "%s:\n", bb->name);
        for (Instr *it = bb->head; it; it = it->next) {
            print_ir_instr(out, it);
        }
    }
    fprintf(out, "}\n");
}
