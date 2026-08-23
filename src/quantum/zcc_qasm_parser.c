#include "include/zcc_qasm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ================================================================ */
/* EXPRESSION LIFETIME & EVALUATION                                 */
/* ================================================================ */

ZCCQasmExpr *zcc_qasm_expr_num(double val) {
    ZCCQasmExpr *e = (ZCCQasmExpr *)calloc(1, sizeof(ZCCQasmExpr));
    if (!e) return NULL;
    e->kind = EXPR_NUM;
    e->num_val = val;
    return e;
}

ZCCQasmExpr *zcc_qasm_expr_pi(void) {
    ZCCQasmExpr *e = (ZCCQasmExpr *)calloc(1, sizeof(ZCCQasmExpr));
    if (!e) return NULL;
    e->kind = EXPR_PI;
    e->num_val = M_PI;
    return e;
}

ZCCQasmExpr *zcc_qasm_expr_param(const char *name) {
    ZCCQasmExpr *e = (ZCCQasmExpr *)calloc(1, sizeof(ZCCQasmExpr));
    if (!e) return NULL;
    e->kind = EXPR_PARAM_REF;
    if (name) strncpy(e->param_name, name, sizeof(e->param_name) - 1);
    return e;
}

ZCCQasmExpr *zcc_qasm_expr_binary(ZCCQasmExprKind kind, ZCCQasmExpr *lhs, ZCCQasmExpr *rhs) {
    ZCCQasmExpr *e = (ZCCQasmExpr *)calloc(1, sizeof(ZCCQasmExpr));
    if (!e) return NULL;
    e->kind = kind;
    e->lhs = lhs;
    e->rhs = rhs;
    return e;
}

ZCCQasmExpr *zcc_qasm_expr_unary(ZCCQasmExprKind kind, ZCCQasmExpr *child) {
    ZCCQasmExpr *e = (ZCCQasmExpr *)calloc(1, sizeof(ZCCQasmExpr));
    if (!e) return NULL;
    e->kind = kind;
    e->lhs = child;
    return e;
}

double zcc_qasm_expr_eval_const(const ZCCQasmExpr *expr, int *ok) {
    if (!expr) {
        if (ok) *ok = 0;
        return 0.0;
    }
    switch (expr->kind) {
        case EXPR_NUM:
            return expr->num_val;
        case EXPR_PI:
            return M_PI;
        case EXPR_PARAM_REF:
            if (ok) *ok = 0;
            return 0.0;
        case EXPR_ADD: {
            int ok_l = 1, ok_r = 1;
            double l = zcc_qasm_expr_eval_const(expr->lhs, &ok_l);
            double r = zcc_qasm_expr_eval_const(expr->rhs, &ok_r);
            if (!ok_l || !ok_r) { if (ok) *ok = 0; return 0.0; }
            return l + r;
        }
        case EXPR_SUB: {
            int ok_l = 1, ok_r = 1;
            double l = zcc_qasm_expr_eval_const(expr->lhs, &ok_l);
            double r = zcc_qasm_expr_eval_const(expr->rhs, &ok_r);
            if (!ok_l || !ok_r) { if (ok) *ok = 0; return 0.0; }
            return l - r;
        }
        case EXPR_MUL: {
            int ok_l = 1, ok_r = 1;
            double l = zcc_qasm_expr_eval_const(expr->lhs, &ok_l);
            double r = zcc_qasm_expr_eval_const(expr->rhs, &ok_r);
            if (!ok_l || !ok_r) { if (ok) *ok = 0; return 0.0; }
            return l * r;
        }
        case EXPR_DIV: {
            int ok_l = 1, ok_r = 1;
            double l = zcc_qasm_expr_eval_const(expr->lhs, &ok_l);
            double r = zcc_qasm_expr_eval_const(expr->rhs, &ok_r);
            if (!ok_l || !ok_r || r == 0.0) { if (ok) *ok = 0; return 0.0; }
            return l / r;
        }
        case EXPR_NEG: {
            int ok_c = 1;
            double c = zcc_qasm_expr_eval_const(expr->lhs, &ok_c);
            if (!ok_c) { if (ok) *ok = 0; return 0.0; }
            return -c;
        }
        case EXPR_SIN: {
            int ok_c = 1;
            double c = zcc_qasm_expr_eval_const(expr->lhs, &ok_c);
            if (!ok_c) { if (ok) *ok = 0; return 0.0; }
            return sin(c);
        }
        case EXPR_COS: {
            int ok_c = 1;
            double c = zcc_qasm_expr_eval_const(expr->lhs, &ok_c);
            if (!ok_c) { if (ok) *ok = 0; return 0.0; }
            return cos(c);
        }
        case EXPR_TAN: {
            int ok_c = 1;
            double c = zcc_qasm_expr_eval_const(expr->lhs, &ok_c);
            if (!ok_c) { if (ok) *ok = 0; return 0.0; }
            return tan(c);
        }
        case EXPR_LN: {
            int ok_c = 1;
            double c = zcc_qasm_expr_eval_const(expr->lhs, &ok_c);
            if (!ok_c || c <= 0.0) { if (ok) *ok = 0; return 0.0; }
            return log(c);
        }
        case EXPR_EXP: {
            int ok_c = 1;
            double c = zcc_qasm_expr_eval_const(expr->lhs, &ok_c);
            if (!ok_c) { if (ok) *ok = 0; return 0.0; }
            return exp(c);
        }
        case EXPR_SQRT: {
            int ok_c = 1;
            double c = zcc_qasm_expr_eval_const(expr->lhs, &ok_c);
            if (!ok_c || c < 0.0) { if (ok) *ok = 0; return 0.0; }
            return sqrt(c);
        }
    }
    if (ok) *ok = 0;
    return 0.0;
}

void zcc_qasm_expr_free(ZCCQasmExpr *expr) {
    if (!expr) return;
    if (expr->lhs) zcc_qasm_expr_free(expr->lhs);
    if (expr->rhs) zcc_qasm_expr_free(expr->rhs);
    free(expr);
}

/* ================================================================ */
/* CIRCUIT LIFETIME                                                 */
/* ================================================================ */

ZCCQasmCircuit *zcc_qasm_circuit_create(void) {
    ZCCQasmCircuit *circ = (ZCCQasmCircuit *)calloc(1, sizeof(ZCCQasmCircuit));
    if (!circ) return NULL;
    circ->version = 2.0;
    return circ;
}

static void zcc_qasm_op_free(ZCCQasmOp *op) {
    if (!op) return;
    int p;
    for (p = 0; p < op->num_params; p++) {
        if (op->params[p]) zcc_qasm_expr_free(op->params[p]);
    }
    free(op);
}

void zcc_qasm_circuit_free(ZCCQasmCircuit *circ) {
    if (!circ) return;
    ZCCQasmOp *curr = circ->head_op;
    while (curr) {
        ZCCQasmOp *next = curr->next;
        zcc_qasm_op_free(curr);
        curr = next;
    }
    int g;
    for (g = 0; g < circ->num_custom_gates; g++) {
        ZCCQasmOp *bcurr = circ->custom_gates[g].body_head;
        while (bcurr) {
            ZCCQasmOp *bnext = bcurr->next;
            zcc_qasm_op_free(bcurr);
            bcurr = bnext;
        }
    }
    free(circ);
}

void zcc_qasm_circuit_add_op(ZCCQasmCircuit *circ, ZCCQasmOp *op) {
    if (!circ || !op) return;
    op->next = NULL;
    if (!circ->head_op) {
        circ->head_op = op;
        circ->tail_op = op;
    } else {
        circ->tail_op->next = op;
        circ->tail_op = op;
    }
    circ->num_ops++;
}

/* ================================================================ */
/* LEXER IMPLEMENTATION                                             */
/* ================================================================ */

typedef enum {
    TOK_EOF = 0,
    TOK_OPENQASM,
    TOK_INCLUDE,
    TOK_QREG,
    TOK_CREG,
    TOK_GATE,
    TOK_OPAQUE,
    TOK_BARRIER,
    TOK_RESET,
    TOK_MEASURE,
    TOK_IF,
    TOK_PI,
    TOK_SIN,
    TOK_COS,
    TOK_TAN,
    TOK_LN,
    TOK_EXP,
    TOK_SQRT,
    TOK_IDENT,
    TOK_INT,
    TOK_REAL,
    TOK_STRING,
    TOK_ARROW,   /* -> */
    TOK_EQEQ,    /* == */
    TOK_LPAREN,  /* ( */
    TOK_RPAREN,  /* ) */
    TOK_LBRACKET,/* [ */
    TOK_RBRACKET,/* ] */
    TOK_LBRACE,  /* { */
    TOK_RBRACE,  /* } */
    TOK_SEMICOLON, /* ; */
    TOK_COMMA,   /* , */
    TOK_PLUS,    /* + */
    TOK_MINUS,   /* - */
    TOK_STAR,    /* * */
    TOK_SLASH    /* / */
} QasmTokenKind;

typedef struct {
    QasmTokenKind kind;
    char text[128];
    double num_val;
    int int_val;
    int line;
    int col;
} QasmToken;

typedef struct {
    const char *src;
    size_t pos;
    size_t len;
    int line;
    int col;
    const char *filename;
    QasmToken cur;
    char err_buf[512];
} QasmLexer;

static void lexer_set_err(QasmLexer *lex, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char msg[384];
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);
    snprintf(lex->err_buf, sizeof(lex->err_buf), "%s:%d:%d: error: %s",
             lex->filename ? lex->filename : "<input>", lex->line, lex->col, msg);
}

static char lexer_peek(QasmLexer *lex) {
    if (lex->pos >= lex->len) return '\0';
    return lex->src[lex->pos];
}

static char lexer_advance(QasmLexer *lex) {
    if (lex->pos >= lex->len) return '\0';
    char c = lex->src[lex->pos++];
    if (c == '\n') {
        lex->line++;
        lex->col = 1;
    } else {
        lex->col++;
    }
    return c;
}

static void lexer_skip_whitespace_and_comments(QasmLexer *lex) {
    while (lex->pos < lex->len) {
        char c = lexer_peek(lex);
        if (isspace((unsigned char)c)) {
            lexer_advance(lex);
        } else if (c == '/' && lex->pos + 1 < lex->len && lex->src[lex->pos + 1] == '/') {
            /* Line comment */
            while (lex->pos < lex->len && lexer_peek(lex) != '\n') {
                lexer_advance(lex);
            }
            if (lex->pos < lex->len && lexer_peek(lex) == '\n') {
                lexer_advance(lex);
            }
        } else {
            break;
        }
    }
}

static void lexer_next(QasmLexer *lex) {
    lexer_skip_whitespace_and_comments(lex);
    memset(&lex->cur, 0, sizeof(QasmToken));
    lex->cur.line = lex->line;
    lex->cur.col = lex->col;

    if (lex->pos >= lex->len) {
        lex->cur.kind = TOK_EOF;
        return;
    }

    char c = lexer_peek(lex);

    /* Symbols */
    if (c == '-' && lex->pos + 1 < lex->len && lex->src[lex->pos + 1] == '>') {
        lexer_advance(lex);
        lexer_advance(lex);
        lex->cur.kind = TOK_ARROW;
        strcpy(lex->cur.text, "->");
        return;
    }
    if (c == '=' && lex->pos + 1 < lex->len && lex->src[lex->pos + 1] == '=') {
        lexer_advance(lex);
        lexer_advance(lex);
        lex->cur.kind = TOK_EQEQ;
        strcpy(lex->cur.text, "==");
        return;
    }
    if (c == '(') { lexer_advance(lex); lex->cur.kind = TOK_LPAREN; strcpy(lex->cur.text, "("); return; }
    if (c == ')') { lexer_advance(lex); lex->cur.kind = TOK_RPAREN; strcpy(lex->cur.text, ")"); return; }
    if (c == '[') { lexer_advance(lex); lex->cur.kind = TOK_LBRACKET; strcpy(lex->cur.text, "["); return; }
    if (c == ']') { lexer_advance(lex); lex->cur.kind = TOK_RBRACKET; strcpy(lex->cur.text, "]"); return; }
    if (c == '{') { lexer_advance(lex); lex->cur.kind = TOK_LBRACE; strcpy(lex->cur.text, "{"); return; }
    if (c == '}') { lexer_advance(lex); lex->cur.kind = TOK_RBRACE; strcpy(lex->cur.text, "}"); return; }
    if (c == ';') { lexer_advance(lex); lex->cur.kind = TOK_SEMICOLON; strcpy(lex->cur.text, ";"); return; }
    if (c == ',') { lexer_advance(lex); lex->cur.kind = TOK_COMMA; strcpy(lex->cur.text, ","); return; }
    if (c == '+') { lexer_advance(lex); lex->cur.kind = TOK_PLUS; strcpy(lex->cur.text, "+"); return; }
    if (c == '-') { lexer_advance(lex); lex->cur.kind = TOK_MINUS; strcpy(lex->cur.text, "-"); return; }
    if (c == '*') { lexer_advance(lex); lex->cur.kind = TOK_STAR; strcpy(lex->cur.text, "*"); return; }
    if (c == '/') { lexer_advance(lex); lex->cur.kind = TOK_SLASH; strcpy(lex->cur.text, "/"); return; }

    /* String literal: "..." */
    if (c == '"') {
        lexer_advance(lex);
        int idx = 0;
        while (lex->pos < lex->len && lexer_peek(lex) != '"' && lexer_peek(lex) != '\n') {
            if (idx < (int)sizeof(lex->cur.text) - 1) {
                lex->cur.text[idx++] = lexer_advance(lex);
            } else {
                lexer_advance(lex);
            }
        }
        lex->cur.text[idx] = '\0';
        if (lexer_peek(lex) == '"') {
            lexer_advance(lex);
        } else {
            lexer_set_err(lex, "unterminated string literal");
        }
        lex->cur.kind = TOK_STRING;
        return;
    }

    /* Numbers (integer or floating point) */
    if (isdigit((unsigned char)c) || (c == '.' && lex->pos + 1 < lex->len && isdigit((unsigned char)lex->src[lex->pos + 1]))) {
        int idx = 0;
        int is_float = (c == '.');
        while (lex->pos < lex->len) {
            char ch = lexer_peek(lex);
            if (isdigit((unsigned char)ch)) {
                if (idx < (int)sizeof(lex->cur.text) - 1) lex->cur.text[idx++] = lexer_advance(lex);
                else lexer_advance(lex);
            } else if (ch == '.' && !is_float) {
                is_float = 1;
                if (idx < (int)sizeof(lex->cur.text) - 1) lex->cur.text[idx++] = lexer_advance(lex);
                else lexer_advance(lex);
            } else if ((ch == 'e' || ch == 'E') && !is_float) {
                is_float = 1;
                if (idx < (int)sizeof(lex->cur.text) - 1) lex->cur.text[idx++] = lexer_advance(lex);
                else lexer_advance(lex);
                if (lexer_peek(lex) == '+' || lexer_peek(lex) == '-') {
                    if (idx < (int)sizeof(lex->cur.text) - 1) lex->cur.text[idx++] = lexer_advance(lex);
                    else lexer_advance(lex);
                }
            } else {
                break;
            }
        }
        lex->cur.text[idx] = '\0';
        if (is_float) {
            lex->cur.kind = TOK_REAL;
            lex->cur.num_val = atof(lex->cur.text);
        } else {
            lex->cur.kind = TOK_INT;
            lex->cur.int_val = atoi(lex->cur.text);
            lex->cur.num_val = (double)lex->cur.int_val;
        }
        return;
    }

    /* Identifiers and Keywords */
    if (isalpha((unsigned char)c) || c == '_') {
        int idx = 0;
        while (lex->pos < lex->len) {
            char ch = lexer_peek(lex);
            if (isalnum((unsigned char)ch) || ch == '_') {
                if (idx < (int)sizeof(lex->cur.text) - 1) lex->cur.text[idx++] = lexer_advance(lex);
                else lexer_advance(lex);
            } else {
                break;
            }
        }
        lex->cur.text[idx] = '\0';

        if (strcmp(lex->cur.text, "OPENQASM") == 0) lex->cur.kind = TOK_OPENQASM;
        else if (strcmp(lex->cur.text, "include") == 0) lex->cur.kind = TOK_INCLUDE;
        else if (strcmp(lex->cur.text, "qreg") == 0) lex->cur.kind = TOK_QREG;
        else if (strcmp(lex->cur.text, "creg") == 0) lex->cur.kind = TOK_CREG;
        else if (strcmp(lex->cur.text, "gate") == 0) lex->cur.kind = TOK_GATE;
        else if (strcmp(lex->cur.text, "opaque") == 0) lex->cur.kind = TOK_OPAQUE;
        else if (strcmp(lex->cur.text, "barrier") == 0) lex->cur.kind = TOK_BARRIER;
        else if (strcmp(lex->cur.text, "reset") == 0) lex->cur.kind = TOK_RESET;
        else if (strcmp(lex->cur.text, "measure") == 0) lex->cur.kind = TOK_MEASURE;
        else if (strcmp(lex->cur.text, "if") == 0) lex->cur.kind = TOK_IF;
        else if (strcmp(lex->cur.text, "pi") == 0) { lex->cur.kind = TOK_PI; lex->cur.num_val = M_PI; }
        else if (strcmp(lex->cur.text, "sin") == 0) lex->cur.kind = TOK_SIN;
        else if (strcmp(lex->cur.text, "cos") == 0) lex->cur.kind = TOK_COS;
        else if (strcmp(lex->cur.text, "tan") == 0) lex->cur.kind = TOK_TAN;
        else if (strcmp(lex->cur.text, "ln") == 0) lex->cur.kind = TOK_LN;
        else if (strcmp(lex->cur.text, "exp") == 0) lex->cur.kind = TOK_EXP;
        else if (strcmp(lex->cur.text, "sqrt") == 0) lex->cur.kind = TOK_SQRT;
        else lex->cur.kind = TOK_IDENT;
        return;
    }

    /* Unknown character */
    lexer_set_err(lex, "unrecognized character '%c' (0x%02x)", c, (unsigned char)c);
    lexer_advance(lex);
    lex->cur.kind = TOK_EOF;
}

/* ================================================================ */
/* RECURSIVE DESCENT PARSER                                         */
/* ================================================================ */

static ZCCQasmExpr *parse_expr(QasmLexer *lex);

static ZCCQasmExpr *parse_primary(QasmLexer *lex) {
    if (lex->cur.kind == TOK_INT) {
        ZCCQasmExpr *e = zcc_qasm_expr_num((double)lex->cur.int_val);
        lexer_next(lex);
        return e;
    }
    if (lex->cur.kind == TOK_REAL) {
        ZCCQasmExpr *e = zcc_qasm_expr_num(lex->cur.num_val);
        lexer_next(lex);
        return e;
    }
    if (lex->cur.kind == TOK_PI) {
        ZCCQasmExpr *e = zcc_qasm_expr_pi();
        lexer_next(lex);
        return e;
    }
    if (lex->cur.kind == TOK_IDENT) {
        ZCCQasmExpr *e = zcc_qasm_expr_param(lex->cur.text);
        lexer_next(lex);
        return e;
    }
    if (lex->cur.kind == TOK_SIN || lex->cur.kind == TOK_COS || lex->cur.kind == TOK_TAN ||
        lex->cur.kind == TOK_LN || lex->cur.kind == TOK_EXP || lex->cur.kind == TOK_SQRT) {
        ZCCQasmExprKind k = EXPR_SIN;
        if (lex->cur.kind == TOK_COS) k = EXPR_COS;
        else if (lex->cur.kind == TOK_TAN) k = EXPR_TAN;
        else if (lex->cur.kind == TOK_LN) k = EXPR_LN;
        else if (lex->cur.kind == TOK_EXP) k = EXPR_EXP;
        else if (lex->cur.kind == TOK_SQRT) k = EXPR_SQRT;
        lexer_next(lex);
        if (lex->cur.kind != TOK_LPAREN) {
            lexer_set_err(lex, "expected '(' after math function call");
            return NULL;
        }
        lexer_next(lex);
        ZCCQasmExpr *inner = parse_expr(lex);
        if (lex->cur.kind != TOK_RPAREN) {
            lexer_set_err(lex, "expected ')' to close math function argument");
            if (inner) zcc_qasm_expr_free(inner);
            return NULL;
        }
        lexer_next(lex);
        return zcc_qasm_expr_unary(k, inner);
    }
    if (lex->cur.kind == TOK_LPAREN) {
        lexer_next(lex);
        ZCCQasmExpr *e = parse_expr(lex);
        if (lex->cur.kind != TOK_RPAREN) {
            lexer_set_err(lex, "expected ')' in expression");
            if (e) zcc_qasm_expr_free(e);
            return NULL;
        }
        lexer_next(lex);
        return e;
    }
    if (lex->cur.kind == TOK_MINUS) {
        lexer_next(lex);
        ZCCQasmExpr *child = parse_primary(lex);
        if (!child) return NULL;
        return zcc_qasm_expr_unary(EXPR_NEG, child);
    }
    if (lex->cur.kind == TOK_PLUS) {
        lexer_next(lex);
        return parse_primary(lex);
    }

    lexer_set_err(lex, "syntax error: unexpected token '%s' in expression", lex->cur.text);
    return NULL;
}

static ZCCQasmExpr *parse_factor(QasmLexer *lex) {
    ZCCQasmExpr *lhs = parse_primary(lex);
    if (!lhs) return NULL;

    while (lex->cur.kind == TOK_STAR || lex->cur.kind == TOK_SLASH) {
        ZCCQasmExprKind k = (lex->cur.kind == TOK_STAR) ? EXPR_MUL : EXPR_DIV;
        lexer_next(lex);
        ZCCQasmExpr *rhs = parse_primary(lex);
        if (!rhs) {
            zcc_qasm_expr_free(lhs);
            return NULL;
        }
        lhs = zcc_qasm_expr_binary(k, lhs, rhs);
    }
    return lhs;
}

static ZCCQasmExpr *parse_expr(QasmLexer *lex) {
    ZCCQasmExpr *lhs = parse_factor(lex);
    if (!lhs) return NULL;

    while (lex->cur.kind == TOK_PLUS || lex->cur.kind == TOK_MINUS) {
        ZCCQasmExprKind k = (lex->cur.kind == TOK_PLUS) ? EXPR_ADD : EXPR_SUB;
        lexer_next(lex);
        ZCCQasmExpr *rhs = parse_factor(lex);
        if (!rhs) {
            zcc_qasm_expr_free(lhs);
            return NULL;
        }
        lhs = zcc_qasm_expr_binary(k, lhs, rhs);
    }
    return lhs;
}

static int parse_qubit_ref(QasmLexer *lex, ZCCQasmQubitRef *ref) {
    if (lex->cur.kind != TOK_IDENT) {
        lexer_set_err(lex, "expected register identifier, got '%s'", lex->cur.text);
        return 0;
    }
    strncpy(ref->reg_name, lex->cur.text, sizeof(ref->reg_name) - 1);
    ref->line = lex->cur.line;
    ref->col = lex->cur.col;
    ref->index = -1;
    lexer_next(lex);

    if (lex->cur.kind == TOK_LBRACKET) {
        lexer_next(lex);
        if (lex->cur.kind != TOK_INT) {
            lexer_set_err(lex, "expected integer index in register access '%s[...]', got '%s'", ref->reg_name, lex->cur.text);
            return 0;
        }
        ref->index = lex->cur.int_val;
        lexer_next(lex);
        if (lex->cur.kind != TOK_RBRACKET) {
            lexer_set_err(lex, "expected ']' closing register index, got '%s'", lex->cur.text);
            return 0;
        }
        lexer_next(lex);
    }
    return 1;
}

static ZCCQasmOpKind qasm_lookup_gate_kind(const char *name) {
    if (strcmp(name, "u") == 0) return QASM_OP_U;
    if (strcmp(name, "u3") == 0) return QASM_OP_U3;
    if (strcmp(name, "u2") == 0) return QASM_OP_U2;
    if (strcmp(name, "u1") == 0) return QASM_OP_U1;
    if (strcmp(name, "id") == 0) return QASM_OP_ID;
    if (strcmp(name, "h") == 0) return QASM_OP_H;
    if (strcmp(name, "x") == 0) return QASM_OP_X;
    if (strcmp(name, "y") == 0) return QASM_OP_Y;
    if (strcmp(name, "z") == 0) return QASM_OP_Z;
    if (strcmp(name, "s") == 0) return QASM_OP_S;
    if (strcmp(name, "sdg") == 0) return QASM_OP_SDG;
    if (strcmp(name, "t") == 0) return QASM_OP_T;
    if (strcmp(name, "tdg") == 0) return QASM_OP_TDG;
    if (strcmp(name, "rx") == 0) return QASM_OP_RX;
    if (strcmp(name, "ry") == 0) return QASM_OP_RY;
    if (strcmp(name, "rz") == 0) return QASM_OP_RZ;
    if (strcmp(name, "p") == 0) return QASM_OP_P;

    if (strcmp(name, "cx") == 0 || strcmp(name, "CX") == 0) return QASM_OP_CX;
    if (strcmp(name, "cy") == 0) return QASM_OP_CY;
    if (strcmp(name, "cz") == 0) return QASM_OP_CZ;
    if (strcmp(name, "ch") == 0) return QASM_OP_CH;
    if (strcmp(name, "swap") == 0) return QASM_OP_SWAP;
    if (strcmp(name, "iswap") == 0) return QASM_OP_ISWAP;
    if (strcmp(name, "crx") == 0) return QASM_OP_CRX;
    if (strcmp(name, "cry") == 0) return QASM_OP_CRY;
    if (strcmp(name, "crz") == 0) return QASM_OP_CRZ;
    if (strcmp(name, "cu1") == 0) return QASM_OP_CU1;
    if (strcmp(name, "cu3") == 0) return QASM_OP_CU3;
    if (strcmp(name, "rzz") == 0) return QASM_OP_RZZ;

    if (strcmp(name, "ccx") == 0) return QASM_OP_CCX;
    if (strcmp(name, "cswap") == 0) return QASM_OP_CSWAP;

    return QASM_OP_CUSTOM;
}

ZCCQasmCircuit *zcc_qasm_parse_string(const char *source, const char *filename, char *err_buf, size_t err_buf_size) {
    if (!source) {
        if (err_buf && err_buf_size > 0) snprintf(err_buf, err_buf_size, "empty source string");
        return NULL;
    }

    QasmLexer lexer;
    memset(&lexer, 0, sizeof(QasmLexer));
    lexer.src = source;
    lexer.len = strlen(source);
    lexer.line = 1;
    lexer.col = 1;
    lexer.filename = filename ? filename : "input.qasm";

    lexer_next(&lexer);

    ZCCQasmCircuit *circ = zcc_qasm_circuit_create();
    if (filename) strncpy(circ->filename, filename, sizeof(circ->filename) - 1);

    while (lexer.cur.kind != TOK_EOF) {
        if (lexer.err_buf[0]) break;

        /* 1. Header: OPENQASM 2.0; */
        if (lexer.cur.kind == TOK_OPENQASM) {
            lexer_next(&lexer);
            if (lexer.cur.kind == TOK_REAL || lexer.cur.kind == TOK_INT) {
                circ->version = lexer.cur.num_val;
                lexer_next(&lexer);
            }
            if (lexer.cur.kind != TOK_SEMICOLON) {
                lexer_set_err(&lexer, "expected ';' after OPENQASM version");
                break;
            }
            lexer_next(&lexer);
            continue;
        }

        /* 2. Include: include "qelib1.inc"; */
        if (lexer.cur.kind == TOK_INCLUDE) {
            lexer_next(&lexer);
            if (lexer.cur.kind != TOK_STRING) {
                lexer_set_err(&lexer, "expected string filename after include directive");
                break;
            }
            strncpy(circ->include_file, lexer.cur.text, sizeof(circ->include_file) - 1);
            lexer_next(&lexer);
            if (lexer.cur.kind != TOK_SEMICOLON) {
                lexer_set_err(&lexer, "expected ';' after include directive");
                break;
            }
            lexer_next(&lexer);
            continue;
        }

        /* 3. Register Declarations: qreg q[size]; / creg c[size]; */
        if (lexer.cur.kind == TOK_QREG || lexer.cur.kind == TOK_CREG) {
            ZCCQasmRegType rtype = (lexer.cur.kind == TOK_QREG) ? QASM_REG_QUANTUM : QASM_REG_CLASSICAL;
            int decl_line = lexer.cur.line;
            int decl_col = lexer.cur.col;
            lexer_next(&lexer);

            if (lexer.cur.kind != TOK_IDENT) {
                lexer_set_err(&lexer, "expected register name");
                break;
            }
            char rname[64];
            strncpy(rname, lexer.cur.text, sizeof(rname) - 1);
            lexer_next(&lexer);

            if (lexer.cur.kind != TOK_LBRACKET) {
                lexer_set_err(&lexer, "expected '[' specifying register size");
                break;
            }
            lexer_next(&lexer);

            if (lexer.cur.kind != TOK_INT || lexer.cur.int_val <= 0) {
                lexer_set_err(&lexer, "expected positive integer register size");
                break;
            }
            int rsize = lexer.cur.int_val;
            lexer_next(&lexer);

            if (lexer.cur.kind != TOK_RBRACKET) {
                lexer_set_err(&lexer, "expected ']' after register size");
                break;
            }
            lexer_next(&lexer);

            if (lexer.cur.kind != TOK_SEMICOLON) {
                lexer_set_err(&lexer, "expected ';' after register declaration");
                break;
            }
            lexer_next(&lexer);

            if (circ->num_registers < ZCC_QASM_MAX_REGS) {
                ZCCQasmRegister *reg = &circ->registers[circ->num_registers++];
                strncpy(reg->name, rname, sizeof(reg->name) - 1);
                reg->size = rsize;
                reg->type = rtype;
                reg->line = decl_line;
                reg->col = decl_col;
                if (rtype == QASM_REG_QUANTUM) {
                    reg->base_offset = circ->total_qubits;
                    circ->total_qubits += rsize;
                } else {
                    reg->base_offset = circ->total_clbits;
                    circ->total_clbits += rsize;
                }
            }
            continue;
        }

        /* 4. Barrier: barrier q0, q1, ...; */
        if (lexer.cur.kind == TOK_BARRIER) {
            ZCCQasmOp *op = (ZCCQasmOp *)calloc(1, sizeof(ZCCQasmOp));
            op->kind = QASM_OP_BARRIER;
            strcpy(op->gate_name, "barrier");
            op->line = lexer.cur.line;
            op->col = lexer.cur.col;
            lexer_next(&lexer);

            while (lexer.cur.kind != TOK_SEMICOLON && lexer.cur.kind != TOK_EOF) {
                if (op->num_qubits < MAX_QASM_GATE_QUBITS) {
                    if (!parse_qubit_ref(&lexer, &op->qubits[op->num_qubits++])) break;
                } else {
                    ZCCQasmQubitRef dummy;
                    parse_qubit_ref(&lexer, &dummy);
                }
                if (lexer.cur.kind == TOK_COMMA) {
                    lexer_next(&lexer);
                } else {
                    break;
                }
            }
            if (lexer.cur.kind != TOK_SEMICOLON) {
                lexer_set_err(&lexer, "expected ';' after barrier");
                zcc_qasm_op_free(op);
                break;
            }
            lexer_next(&lexer);
            zcc_qasm_circuit_add_op(circ, op);
            continue;
        }

        /* 5. Reset: reset q[i]; */
        if (lexer.cur.kind == TOK_RESET) {
            ZCCQasmOp *op = (ZCCQasmOp *)calloc(1, sizeof(ZCCQasmOp));
            op->kind = QASM_OP_RESET;
            strcpy(op->gate_name, "reset");
            op->line = lexer.cur.line;
            op->col = lexer.cur.col;
            lexer_next(&lexer);

            if (!parse_qubit_ref(&lexer, &op->qubits[0])) {
                zcc_qasm_op_free(op);
                break;
            }
            op->num_qubits = 1;

            if (lexer.cur.kind != TOK_SEMICOLON) {
                lexer_set_err(&lexer, "expected ';' after reset");
                zcc_qasm_op_free(op);
                break;
            }
            lexer_next(&lexer);
            zcc_qasm_circuit_add_op(circ, op);
            continue;
        }

        /* 6. Measure: measure q[i] -> c[j]; */
        if (lexer.cur.kind == TOK_MEASURE) {
            ZCCQasmOp *op = (ZCCQasmOp *)calloc(1, sizeof(ZCCQasmOp));
            op->kind = QASM_OP_MEASURE;
            strcpy(op->gate_name, "measure");
            op->line = lexer.cur.line;
            op->col = lexer.cur.col;
            lexer_next(&lexer);

            if (!parse_qubit_ref(&lexer, &op->qubits[0])) {
                zcc_qasm_op_free(op);
                break;
            }
            op->num_qubits = 1;

            if (lexer.cur.kind != TOK_ARROW) {
                lexer_set_err(&lexer, "expected '->' in measure statement");
                zcc_qasm_op_free(op);
                break;
            }
            lexer_next(&lexer);

            if (!parse_qubit_ref(&lexer, &op->meas_target)) {
                zcc_qasm_op_free(op);
                break;
            }

            if (lexer.cur.kind != TOK_SEMICOLON) {
                lexer_set_err(&lexer, "expected ';' after measure statement");
                zcc_qasm_op_free(op);
                break;
            }
            lexer_next(&lexer);
            zcc_qasm_circuit_add_op(circ, op);
            continue;
        }

        /* 7. Conditional: if (c == val) <op> */
        int has_cond = 0;
        char cond_reg[64] = {0};
        int cond_val = 0;
        int cond_line = lexer.cur.line;
        int cond_col = lexer.cur.col;

        if (lexer.cur.kind == TOK_IF) {
            lexer_next(&lexer);
            if (lexer.cur.kind != TOK_LPAREN) {
                lexer_set_err(&lexer, "expected '(' after 'if'");
                break;
            }
            lexer_next(&lexer);

            if (lexer.cur.kind != TOK_IDENT) {
                lexer_set_err(&lexer, "expected classical register identifier in 'if' condition");
                break;
            }
            strncpy(cond_reg, lexer.cur.text, sizeof(cond_reg) - 1);
            lexer_next(&lexer);

            if (lexer.cur.kind != TOK_EQEQ) {
                lexer_set_err(&lexer, "expected '==' in 'if' condition");
                break;
            }
            lexer_next(&lexer);

            if (lexer.cur.kind != TOK_INT) {
                lexer_set_err(&lexer, "expected integer constant in 'if' condition");
                break;
            }
            cond_val = lexer.cur.int_val;
            lexer_next(&lexer);

            if (lexer.cur.kind != TOK_RPAREN) {
                lexer_set_err(&lexer, "expected ')' closing 'if' condition");
                break;
            }
            lexer_next(&lexer);
            has_cond = 1;
        }

        /* 8. Gate Application (built-in or custom) */
        if (lexer.cur.kind == TOK_IDENT) {
            ZCCQasmOp *op = (ZCCQasmOp *)calloc(1, sizeof(ZCCQasmOp));
            strncpy(op->gate_name, lexer.cur.text, sizeof(op->gate_name) - 1);
            op->kind = qasm_lookup_gate_kind(op->gate_name);
            op->line = has_cond ? cond_line : lexer.cur.line;
            op->col = has_cond ? cond_col : lexer.cur.col;
            if (has_cond) {
                op->has_condition = 1;
                strncpy(op->cond_reg, cond_reg, sizeof(op->cond_reg) - 1);
                op->cond_val = cond_val;
            }
            lexer_next(&lexer);

            /* Parse optional parameters: (theta, phi, lambda) */
            if (lexer.cur.kind == TOK_LPAREN) {
                lexer_next(&lexer);
                while (lexer.cur.kind != TOK_RPAREN && lexer.cur.kind != TOK_EOF) {
                    ZCCQasmExpr *arg = parse_expr(&lexer);
                    if (!arg) break;
                    if (op->num_params < MAX_QASM_GATE_PARAMS) {
                        op->params[op->num_params++] = arg;
                    } else {
                        zcc_qasm_expr_free(arg);
                    }
                    if (lexer.cur.kind == TOK_COMMA) {
                        lexer_next(&lexer);
                    } else {
                        break;
                    }
                }
                if (lexer.cur.kind != TOK_RPAREN) {
                    lexer_set_err(&lexer, "expected ')' after gate parameter list");
                    zcc_qasm_op_free(op);
                    break;
                }
                lexer_next(&lexer);
            }

            /* Parse qubit arguments: q[0], q[1], ... */
            while (lexer.cur.kind != TOK_SEMICOLON && lexer.cur.kind != TOK_EOF) {
                if (op->num_qubits < MAX_QASM_GATE_QUBITS) {
                    if (!parse_qubit_ref(&lexer, &op->qubits[op->num_qubits++])) break;
                } else {
                    ZCCQasmQubitRef dummy;
                    parse_qubit_ref(&lexer, &dummy);
                }
                if (lexer.cur.kind == TOK_COMMA) {
                    lexer_next(&lexer);
                } else {
                    break;
                }
            }

            if (lexer.cur.kind != TOK_SEMICOLON) {
                lexer_set_err(&lexer, "expected ';' after gate operation");
                zcc_qasm_op_free(op);
                break;
            }
            lexer_next(&lexer);
            zcc_qasm_circuit_add_op(circ, op);
            continue;
        }

        /* 9. Custom Gate Definition: gate my_gate(a, b) q0, q1 { ... } */
        if (lexer.cur.kind == TOK_GATE) {
            int g_line = lexer.cur.line;
            int g_col = lexer.cur.col;
            lexer_next(&lexer);

            if (lexer.cur.kind != TOK_IDENT) {
                lexer_set_err(&lexer, "expected custom gate identifier name");
                break;
            }
            char gname[64];
            strncpy(gname, lexer.cur.text, sizeof(gname) - 1);
            lexer_next(&lexer);

            ZCCQasmGateDef *gdef = NULL;
            if (circ->num_custom_gates < MAX_QASM_CUSTOM_GATES) {
                gdef = &circ->custom_gates[circ->num_custom_gates++];
                memset(gdef, 0, sizeof(ZCCQasmGateDef));
                strncpy(gdef->name, gname, sizeof(gdef->name) - 1);
                gdef->line = g_line;
                gdef->col = g_col;
            }

            /* Parameters */
            if (lexer.cur.kind == TOK_LPAREN) {
                lexer_next(&lexer);
                while (lexer.cur.kind == TOK_IDENT) {
                    if (gdef && gdef->num_params < MAX_QASM_GATE_PARAMS) {
                        strncpy(gdef->param_names[gdef->num_params++], lexer.cur.text, 31);
                    }
                    lexer_next(&lexer);
                    if (lexer.cur.kind == TOK_COMMA) lexer_next(&lexer);
                    else break;
                }
                if (lexer.cur.kind != TOK_RPAREN) {
                    lexer_set_err(&lexer, "expected ')' closing custom gate parameter declaration");
                    break;
                }
                lexer_next(&lexer);
            }

            /* Qubit formal parameters */
            while (lexer.cur.kind == TOK_IDENT) {
                if (gdef && gdef->num_qubits < MAX_QASM_GATE_QUBITS) {
                    strncpy(gdef->qubit_names[gdef->num_qubits++], lexer.cur.text, 31);
                }
                lexer_next(&lexer);
                if (lexer.cur.kind == TOK_COMMA) lexer_next(&lexer);
                else break;
            }

            if (lexer.cur.kind != TOK_LBRACE) {
                lexer_set_err(&lexer, "expected '{' opening custom gate body");
                break;
            }
            lexer_next(&lexer);

            /* Parse inner body operations until '}' */
            while (lexer.cur.kind != TOK_RBRACE && lexer.cur.kind != TOK_EOF) {
                if (lexer.cur.kind != TOK_IDENT) {
                    lexer_set_err(&lexer, "expected gate identifier inside custom gate body");
                    break;
                }
                ZCCQasmOp *bop = (ZCCQasmOp *)calloc(1, sizeof(ZCCQasmOp));
                strncpy(bop->gate_name, lexer.cur.text, sizeof(bop->gate_name) - 1);
                bop->kind = qasm_lookup_gate_kind(bop->gate_name);
                bop->line = lexer.cur.line;
                bop->col = lexer.cur.col;
                lexer_next(&lexer);

                if (lexer.cur.kind == TOK_LPAREN) {
                    lexer_next(&lexer);
                    while (lexer.cur.kind != TOK_RPAREN && lexer.cur.kind != TOK_EOF) {
                        ZCCQasmExpr *arg = parse_expr(&lexer);
                        if (!arg) break;
                        if (bop->num_params < MAX_QASM_GATE_PARAMS) bop->params[bop->num_params++] = arg;
                        else zcc_qasm_expr_free(arg);
                        if (lexer.cur.kind == TOK_COMMA) lexer_next(&lexer);
                        else break;
                    }
                    if (lexer.cur.kind == TOK_RPAREN) lexer_next(&lexer);
                }

                while (lexer.cur.kind != TOK_SEMICOLON && lexer.cur.kind != TOK_EOF) {
                    if (bop->num_qubits < MAX_QASM_GATE_QUBITS) {
                        parse_qubit_ref(&lexer, &bop->qubits[bop->num_qubits++]);
                    } else {
                        ZCCQasmQubitRef dummy;
                        parse_qubit_ref(&lexer, &dummy);
                    }
                    if (lexer.cur.kind == TOK_COMMA) lexer_next(&lexer);
                    else break;
                }
                if (lexer.cur.kind == TOK_SEMICOLON) lexer_next(&lexer);

                if (gdef) {
                    bop->next = NULL;
                    if (!gdef->body_head) {
                        gdef->body_head = bop;
                        gdef->body_tail = bop;
                    } else {
                        gdef->body_tail->next = bop;
                        gdef->body_tail = bop;
                    }
                    gdef->num_body_ops++;
                } else {
                    zcc_qasm_op_free(bop);
                }
            }

            if (lexer.cur.kind != TOK_RBRACE) {
                lexer_set_err(&lexer, "expected '}' closing custom gate body");
                break;
            }
            lexer_next(&lexer);
            continue;
        }

        /* Unrecognized statement */
        lexer_set_err(&lexer, "syntax error: unexpected token '%s'", lexer.cur.text);
        break;
    }

    if (lexer.err_buf[0]) {
        if (err_buf && err_buf_size > 0) {
            strncpy(err_buf, lexer.err_buf, err_buf_size - 1);
            err_buf[err_buf_size - 1] = '\0';
        }
        zcc_qasm_circuit_free(circ);
        return NULL;
    }

    return circ;
}

ZCCQasmCircuit *zcc_qasm_parse_file(const char *filename, char *err_buf, size_t err_buf_size) {
    if (!filename) {
        if (err_buf && err_buf_size > 0) snprintf(err_buf, err_buf_size, "null filename");
        return NULL;
    }
    FILE *f = fopen(filename, "rb");
    if (!f) {
        if (err_buf && err_buf_size > 0) snprintf(err_buf, err_buf_size, "failed to open '%s'", filename);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0) {
        fclose(f);
        if (err_buf && err_buf_size > 0) snprintf(err_buf, err_buf_size, "failed to read '%s'", filename);
        return NULL;
    }
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) {
        fclose(f);
        if (err_buf && err_buf_size > 0) snprintf(err_buf, err_buf_size, "out of memory reading '%s'", filename);
        return NULL;
    }
    size_t read_bytes = fread(buf, 1, (size_t)sz, f);
    buf[read_bytes] = '\0';
    fclose(f);

    ZCCQasmCircuit *circ = zcc_qasm_parse_string(buf, filename, err_buf, err_buf_size);
    free(buf);
    return circ;
}

/* ================================================================ */
/* SEMANTIC VALIDATOR                                               */
/* ================================================================ */

static const ZCCQasmRegister *find_register(const ZCCQasmCircuit *circ, const char *name) {
    if (!circ || !name) return NULL;
    int i;
    for (i = 0; i < circ->num_registers; i++) {
        if (strcmp(circ->registers[i].name, name) == 0) {
            return &circ->registers[i];
        }
    }
    return NULL;
}

int zcc_qasm_validate(const ZCCQasmCircuit *circ, char *err_buf, size_t err_buf_size) {
    if (!circ) {
        if (err_buf && err_buf_size > 0) snprintf(err_buf, err_buf_size, "null circuit pointer");
        return 0;
    }

    const char *fn = circ->filename[0] ? circ->filename : "<input>";

    /* 1. Register Uniqueness */
    int i, j;
    for (i = 0; i < circ->num_registers; i++) {
        for (j = i + 1; j < circ->num_registers; j++) {
            if (strcmp(circ->registers[i].name, circ->registers[j].name) == 0) {
                if (err_buf && err_buf_size > 0) {
                    snprintf(err_buf, err_buf_size, "%s:%d:%d: error: duplicate register declaration '%s'",
                             fn, circ->registers[j].line, circ->registers[j].col, circ->registers[j].name);
                }
                return 0;
            }
        }
    }

    /* 2. Operations semantic validity */
    const ZCCQasmOp *op = circ->head_op;
    while (op) {
        /* Check condition if present */
        if (op->has_condition) {
            const ZCCQasmRegister *creg = find_register(circ, op->cond_reg);
            if (!creg) {
                if (err_buf && err_buf_size > 0) {
                    snprintf(err_buf, err_buf_size, "%s:%d:%d: error: undeclared register '%s' in 'if' condition",
                             fn, op->line, op->col, op->cond_reg);
                }
                return 0;
            }
            if (creg->type != QASM_REG_CLASSICAL) {
                if (err_buf && err_buf_size > 0) {
                    snprintf(err_buf, err_buf_size, "%s:%d:%d: error: register '%s' in 'if' condition must be classical (creg)",
                             fn, op->line, op->col, op->cond_reg);
                }
                return 0;
            }
        }

        /* Check Measurement targets */
        if (op->kind == QASM_OP_MEASURE) {
            if (op->num_qubits < 1) {
                if (err_buf && err_buf_size > 0) {
                    snprintf(err_buf, err_buf_size, "%s:%d:%d: error: measure missing quantum source operand",
                             fn, op->line, op->col);
                }
                return 0;
            }
            const ZCCQasmRegister *qreg = find_register(circ, op->qubits[0].reg_name);
            if (!qreg) {
                if (err_buf && err_buf_size > 0) {
                    snprintf(err_buf, err_buf_size, "%s:%d:%d: error: undeclared quantum register '%s'",
                             fn, op->qubits[0].line, op->qubits[0].col, op->qubits[0].reg_name);
                }
                return 0;
            }
            if (qreg->type != QASM_REG_QUANTUM) {
                if (err_buf && err_buf_size > 0) {
                    snprintf(err_buf, err_buf_size, "%s:%d:%d: error: source '%s' of measure must be a quantum register (qreg)",
                             fn, op->qubits[0].line, op->qubits[0].col, op->qubits[0].reg_name);
                }
                return 0;
            }
            if (op->qubits[0].index >= qreg->size) {
                if (err_buf && err_buf_size > 0) {
                    snprintf(err_buf, err_buf_size, "%s:%d:%d: error: index %d out of bounds for quantum register '%s' of size %d",
                             fn, op->qubits[0].line, op->qubits[0].col, op->qubits[0].index, qreg->name, qreg->size);
                }
                return 0;
            }

            const ZCCQasmRegister *creg = find_register(circ, op->meas_target.reg_name);
            if (!creg) {
                if (err_buf && err_buf_size > 0) {
                    snprintf(err_buf, err_buf_size, "%s:%d:%d: error: undeclared classical register '%s'",
                             fn, op->meas_target.line, op->meas_target.col, op->meas_target.reg_name);
                }
                return 0;
            }
            if (creg->type != QASM_REG_CLASSICAL) {
                if (err_buf && err_buf_size > 0) {
                    snprintf(err_buf, err_buf_size, "%s:%d:%d: error: target '%s' of measure must be a classical register (creg)",
                             fn, op->meas_target.line, op->meas_target.col, op->meas_target.reg_name);
                }
                return 0;
            }
            if (op->meas_target.index >= creg->size) {
                if (err_buf && err_buf_size > 0) {
                    snprintf(err_buf, err_buf_size, "%s:%d:%d: error: index %d out of bounds for classical register '%s' of size %d",
                             fn, op->meas_target.line, op->meas_target.col, op->meas_target.index, creg->name, creg->size);
                }
                return 0;
            }
            op = op->next;
            continue;
        }

        /* Check Gate Qubit Operands */
        int q;
        for (q = 0; q < op->num_qubits; q++) {
            const ZCCQasmRegister *reg = find_register(circ, op->qubits[q].reg_name);
            if (!reg) {
                if (err_buf && err_buf_size > 0) {
                    snprintf(err_buf, err_buf_size, "%s:%d:%d: error: undeclared register '%s'",
                             fn, op->qubits[q].line, op->qubits[q].col, op->qubits[q].reg_name);
                }
                return 0;
            }
            if (reg->type != QASM_REG_QUANTUM) {
                if (err_buf && err_buf_size > 0) {
                    snprintf(err_buf, err_buf_size, "%s:%d:%d: error: operand '%s' for gate '%s' must be quantum (qreg)",
                             fn, op->qubits[q].line, op->qubits[q].col, op->qubits[q].reg_name, op->gate_name);
                }
                return 0;
            }
            if (op->qubits[q].index >= reg->size) {
                if (err_buf && err_buf_size > 0) {
                    snprintf(err_buf, err_buf_size, "%s:%d:%d: error: index %d out of bounds for quantum register '%s' of size %d",
                             fn, op->qubits[q].line, op->qubits[q].col, op->qubits[q].index, reg->name, reg->size);
                }
                return 0;
            }
        }

        /* Check Distinct Qubits for Multi-Qubit Gates */
        if (op->kind == QASM_OP_CX || op->kind == QASM_OP_CY || op->kind == QASM_OP_CZ ||
            op->kind == QASM_OP_CH || op->kind == QASM_OP_SWAP || op->kind == QASM_OP_ISWAP ||
            op->kind == QASM_OP_CRX || op->kind == QASM_OP_CRY || op->kind == QASM_OP_CRZ ||
            op->kind == QASM_OP_CU1 || op->kind == QASM_OP_CU3 || op->kind == QASM_OP_RZZ) {
            if (op->num_qubits != 2) {
                if (err_buf && err_buf_size > 0) {
                    snprintf(err_buf, err_buf_size, "%s:%d:%d: error: gate '%s' requires 2 qubit operands, got %d",
                             fn, op->line, op->col, op->gate_name, op->num_qubits);
                }
                return 0;
            }
            if (strcmp(op->qubits[0].reg_name, op->qubits[1].reg_name) == 0 && op->qubits[0].index == op->qubits[1].index) {
                if (err_buf && err_buf_size > 0) {
                    snprintf(err_buf, err_buf_size, "%s:%d:%d: error: duplicate qubit operand '%s[%d]' in 2-qubit gate '%s'",
                             fn, op->line, op->col, op->qubits[0].reg_name, op->qubits[0].index, op->gate_name);
                }
                return 0;
            }
        }

        if (op->kind == QASM_OP_CCX || op->kind == QASM_OP_CSWAP) {
            if (op->num_qubits != 3) {
                if (err_buf && err_buf_size > 0) {
                    snprintf(err_buf, err_buf_size, "%s:%d:%d: error: gate '%s' requires 3 qubit operands, got %d",
                             fn, op->line, op->col, op->gate_name, op->num_qubits);
                }
                return 0;
            }
            if ((strcmp(op->qubits[0].reg_name, op->qubits[1].reg_name) == 0 && op->qubits[0].index == op->qubits[1].index) ||
                (strcmp(op->qubits[0].reg_name, op->qubits[2].reg_name) == 0 && op->qubits[0].index == op->qubits[2].index) ||
                (strcmp(op->qubits[1].reg_name, op->qubits[2].reg_name) == 0 && op->qubits[1].index == op->qubits[2].index)) {
                if (err_buf && err_buf_size > 0) {
                    snprintf(err_buf, err_buf_size, "%s:%d:%d: error: duplicate qubit operands in 3-qubit gate '%s'",
                             fn, op->line, op->col, op->gate_name);
                }
                return 0;
            }
        }

        /* Check Gate Parameter Counts */
        if (op->kind == QASM_OP_U || op->kind == QASM_OP_U3 || op->kind == QASM_OP_CU3) {
            if (op->num_params != 3) {
                if (err_buf && err_buf_size > 0) {
                    snprintf(err_buf, err_buf_size, "%s:%d:%d: error: gate '%s' requires 3 parameters (theta, phi, lambda), got %d",
                             fn, op->line, op->col, op->gate_name, op->num_params);
                }
                return 0;
            }
        } else if (op->kind == QASM_OP_U2) {
            if (op->num_params != 2) {
                if (err_buf && err_buf_size > 0) {
                    snprintf(err_buf, err_buf_size, "%s:%d:%d: error: gate 'u2' requires 2 parameters (phi, lambda), got %d",
                             fn, op->line, op->col, op->num_params);
                }
                return 0;
            }
        } else if (op->kind == QASM_OP_U1 || op->kind == QASM_OP_RX || op->kind == QASM_OP_RY ||
                   op->kind == QASM_OP_RZ || op->kind == QASM_OP_P || op->kind == QASM_OP_CRX ||
                   op->kind == QASM_OP_CRY || op->kind == QASM_OP_CRZ || op->kind == QASM_OP_CU1 ||
                   op->kind == QASM_OP_RZZ) {
            if (op->num_params != 1) {
                if (err_buf && err_buf_size > 0) {
                    snprintf(err_buf, err_buf_size, "%s:%d:%d: error: gate '%s' requires 1 parameter, got %d",
                             fn, op->line, op->col, op->gate_name, op->num_params);
                }
                return 0;
            }
        }

        op = op->next;
    }

    return 1;
}

/* ================================================================ */
/* CANONICAL QASM EMITTER                                           */
/* ================================================================ */

typedef struct {
    char *data;
    size_t size;
    size_t cap;
} StrBuf;

static void sbuf_init(StrBuf *sb) {
    sb->cap = 1024;
    sb->data = (char *)malloc(sb->cap);
    sb->size = 0;
    if (sb->data) sb->data[0] = '\0';
}

static void sbuf_append(StrBuf *sb, const char *str) {
    if (!sb || !str) return;
    size_t len = strlen(str);
    while (sb->size + len + 1 > sb->cap) {
        sb->cap *= 2;
        sb->data = (char *)realloc(sb->data, sb->cap);
    }
    memcpy(sb->data + sb->size, str, len);
    sb->size += len;
    sb->data[sb->size] = '\0';
}

static void emit_expr_string(const ZCCQasmExpr *e, StrBuf *sb) {
    if (!e) return;
    char tmp[64];
    switch (e->kind) {
        case EXPR_NUM:
            if (fabs(e->num_val - round(e->num_val)) < 1e-9) {
                snprintf(tmp, sizeof(tmp), "%lld", (long long)round(e->num_val));
            } else {
                snprintf(tmp, sizeof(tmp), "%.10g", e->num_val);
            }
            sbuf_append(sb, tmp);
            break;
        case EXPR_PI:
            sbuf_append(sb, "pi");
            break;
        case EXPR_PARAM_REF:
            sbuf_append(sb, e->param_name);
            break;
        case EXPR_ADD:
            emit_expr_string(e->lhs, sb);
            sbuf_append(sb, " + ");
            emit_expr_string(e->rhs, sb);
            break;
        case EXPR_SUB:
            emit_expr_string(e->lhs, sb);
            sbuf_append(sb, " - ");
            emit_expr_string(e->rhs, sb);
            break;
        case EXPR_MUL:
            emit_expr_string(e->lhs, sb);
            sbuf_append(sb, "*");
            emit_expr_string(e->rhs, sb);
            break;
        case EXPR_DIV:
            emit_expr_string(e->lhs, sb);
            sbuf_append(sb, "/");
            emit_expr_string(e->rhs, sb);
            break;
        case EXPR_NEG:
            sbuf_append(sb, "-");
            emit_expr_string(e->lhs, sb);
            break;
        case EXPR_SIN:
            sbuf_append(sb, "sin(");
            emit_expr_string(e->lhs, sb);
            sbuf_append(sb, ")");
            break;
        case EXPR_COS:
            sbuf_append(sb, "cos(");
            emit_expr_string(e->lhs, sb);
            sbuf_append(sb, ")");
            break;
        case EXPR_TAN:
            sbuf_append(sb, "tan(");
            emit_expr_string(e->lhs, sb);
            sbuf_append(sb, ")");
            break;
        case EXPR_LN:
            sbuf_append(sb, "ln(");
            emit_expr_string(e->lhs, sb);
            sbuf_append(sb, ")");
            break;
        case EXPR_EXP:
            sbuf_append(sb, "exp(");
            emit_expr_string(e->lhs, sb);
            sbuf_append(sb, ")");
            break;
        case EXPR_SQRT:
            sbuf_append(sb, "sqrt(");
            emit_expr_string(e->lhs, sb);
            sbuf_append(sb, ")");
            break;
    }
}

static void emit_qubit_ref(const ZCCQasmQubitRef *ref, StrBuf *sb) {
    sbuf_append(sb, ref->reg_name);
    if (ref->index >= 0) {
        char idx_str[32];
        snprintf(idx_str, sizeof(idx_str), "[%d]", ref->index);
        sbuf_append(sb, idx_str);
    }
}

char *zcc_qasm_emit_canonical(const ZCCQasmCircuit *circ) {
    if (!circ) return NULL;

    StrBuf sb;
    sbuf_init(&sb);

    /* 1. Header */
    char hdr[64];
    snprintf(hdr, sizeof(hdr), "OPENQASM %.1f;\n", circ->version > 0 ? circ->version : 2.0);
    sbuf_append(&sb, hdr);

    /* 2. Include */
    if (circ->include_file[0]) {
        sbuf_append(&sb, "include \"");
        sbuf_append(&sb, circ->include_file);
        sbuf_append(&sb, "\";\n");
    } else {
        sbuf_append(&sb, "include \"qelib1.inc\";\n");
    }

    /* 3. Register Declarations */
    int r;
    for (r = 0; r < circ->num_registers; r++) {
        const ZCCQasmRegister *reg = &circ->registers[r];
        char reg_decl[128];
        snprintf(reg_decl, sizeof(reg_decl), "%s %s[%d];\n",
                 (reg->type == QASM_REG_QUANTUM) ? "qreg" : "creg",
                 reg->name, reg->size);
        sbuf_append(&sb, reg_decl);
    }

    /* 4. Custom Gate Definitions */
    int g;
    for (g = 0; g < circ->num_custom_gates; g++) {
        const ZCCQasmGateDef *gdef = &circ->custom_gates[g];
        sbuf_append(&sb, "gate ");
        sbuf_append(&sb, gdef->name);
        if (gdef->num_params > 0) {
            sbuf_append(&sb, "(");
            int p;
            for (p = 0; p < gdef->num_params; p++) {
                if (p > 0) sbuf_append(&sb, ", ");
                sbuf_append(&sb, gdef->param_names[p]);
            }
            sbuf_append(&sb, ")");
        }
        sbuf_append(&sb, " ");
        int q;
        for (q = 0; q < gdef->num_qubits; q++) {
            if (q > 0) sbuf_append(&sb, ", ");
            sbuf_append(&sb, gdef->qubit_names[q]);
        }
        sbuf_append(&sb, " {\n");

        const ZCCQasmOp *bop = gdef->body_head;
        while (bop) {
            sbuf_append(&sb, "  ");
            sbuf_append(&sb, bop->gate_name);
            if (bop->num_params > 0) {
                sbuf_append(&sb, "(");
                int p;
                for (p = 0; p < bop->num_params; p++) {
                    if (p > 0) sbuf_append(&sb, ", ");
                    emit_expr_string(bop->params[p], &sb);
                }
                sbuf_append(&sb, ")");
            }
            sbuf_append(&sb, " ");
            int bq;
            for (bq = 0; bq < bop->num_qubits; bq++) {
                if (bq > 0) sbuf_append(&sb, ", ");
                emit_qubit_ref(&bop->qubits[bq], &sb);
            }
            sbuf_append(&sb, ";\n");
            bop = bop->next;
        }
        sbuf_append(&sb, "}\n");
    }

    /* 5. Gate Operations */
    const ZCCQasmOp *op = circ->head_op;
    while (op) {
        if (op->has_condition) {
            char cond_buf[128];
            snprintf(cond_buf, sizeof(cond_buf), "if (%s == %d) ", op->cond_reg, op->cond_val);
            sbuf_append(&sb, cond_buf);
        }

        if (op->kind == QASM_OP_MEASURE) {
            sbuf_append(&sb, "measure ");
            emit_qubit_ref(&op->qubits[0], &sb);
            sbuf_append(&sb, " -> ");
            emit_qubit_ref(&op->meas_target, &sb);
            sbuf_append(&sb, ";\n");
        } else if (op->kind == QASM_OP_RESET) {
            sbuf_append(&sb, "reset ");
            emit_qubit_ref(&op->qubits[0], &sb);
            sbuf_append(&sb, ";\n");
        } else if (op->kind == QASM_OP_BARRIER) {
            sbuf_append(&sb, "barrier");
            if (op->num_qubits > 0) {
                sbuf_append(&sb, " ");
                int q;
                for (q = 0; q < op->num_qubits; q++) {
                    if (q > 0) sbuf_append(&sb, ", ");
                    emit_qubit_ref(&op->qubits[q], &sb);
                }
            }
            sbuf_append(&sb, ";\n");
        } else {
            sbuf_append(&sb, op->gate_name);
            if (op->num_params > 0) {
                sbuf_append(&sb, "(");
                int p;
                for (p = 0; p < op->num_params; p++) {
                    if (p > 0) sbuf_append(&sb, ", ");
                    emit_expr_string(op->params[p], &sb);
                }
                sbuf_append(&sb, ")");
            }
            sbuf_append(&sb, " ");
            int q;
            for (q = 0; q < op->num_qubits; q++) {
                if (q > 0) sbuf_append(&sb, ", ");
                emit_qubit_ref(&op->qubits[q], &sb);
            }
            sbuf_append(&sb, ";\n");
        }
        op = op->next;
    }

    return sb.data;
}

int zcc_qasm_emit_file(const ZCCQasmCircuit *circ, const char *output_file) {
    if (!circ || !output_file) return -1;
    char *text = zcc_qasm_emit_canonical(circ);
    if (!text) return -1;
    FILE *f = fopen(output_file, "wb");
    if (!f) {
        free(text);
        return -1;
    }
    fwrite(text, 1, strlen(text), f);
    fclose(f);
    free(text);
    return 0;
}
