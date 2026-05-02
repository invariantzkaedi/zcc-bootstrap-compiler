/* ================================================================ */
/* RUST FRONTEND (FOUNDATION)                                       */
/* ================================================================ */

#ifndef ZCC_AST_BRIDGE_H
#include "part1.c"
#endif

enum {
    RUST_TK_EOF = 0,
    RUST_TK_IDENT,
    RUST_TK_NUM,
    RUST_TK_FN,
    RUST_TK_LET,
    RUST_TK_RETURN,
    RUST_TK_IF,
    RUST_TK_ELSE,
    RUST_TK_WHILE,
    RUST_TK_TRUE,
    RUST_TK_FALSE,
    RUST_TK_I32,
    RUST_TK_LPAREN,
    RUST_TK_RPAREN,
    RUST_TK_LBRACE,
    RUST_TK_RBRACE,
    RUST_TK_COLON,
    RUST_TK_SEMI,
    RUST_TK_COMMA,
    RUST_TK_ASSIGN,
    RUST_TK_PLUS,
    RUST_TK_MINUS,
    RUST_TK_STAR,
    RUST_TK_SLASH,
    RUST_TK_EQ,
    RUST_TK_NE,
    RUST_TK_LT,
    RUST_TK_LE,
    RUST_TK_GT,
    RUST_TK_GE,
    RUST_TK_LAND,
    RUST_TK_LOR,
    RUST_TK_BANG,
    RUST_TK_ARROW
};

enum {
    RUST_STMT_LET = 1,
    RUST_STMT_RETURN,
    RUST_STMT_EXPR,
    RUST_STMT_IF,
    RUST_STMT_WHILE,
    RUST_STMT_ASSIGN
};

enum {
    RUST_EXPR_IDENT = 1,
    RUST_EXPR_NUM,
    RUST_EXPR_BOOL,
    RUST_EXPR_BINARY,
    RUST_EXPR_UNARY,
    RUST_EXPR_CALL
};

typedef enum RustTypeKind {
    RUST_TYPE_ERROR = 0,
    RUST_TYPE_I32,
    RUST_TYPE_BOOL
} RustTypeKind;

typedef enum RustFlowKind {
    RUST_FLOW_MAY_CONTINUE = 0,
    RUST_FLOW_ALWAYS_RETURNS = 1
} RustFlowKind;

typedef struct RustToken {
    int kind;
    int line;
    int col;
    long long num;
    char text[128];
} RustToken;

struct RustDiag {
    int kind; /* 0=error, 1=note */
    char code[32];
    int line;
    int col;
    char message[256];
    char hint[256];
};

typedef struct RustExpr {
    int kind;
    char text[128];
    RustSymbolId symbol_id;
    int line;
    int col;
    long long num;
    int bool_val;
    int op;
    struct RustExpr *lhs;
    struct RustExpr *rhs;
    char call_callee[128];
    RustSymbolId call_callee_symbol_id;
    struct RustExpr **call_args;
    int call_arg_count;
} RustExpr;

typedef struct RustStmt {
    int kind;
    int line;
    int col;
    char name[128];
    RustSymbolId symbol_id;
    int name_line;
    int name_col;
    char type_name[64];
    int has_type_annotation;
    int is_mut;
    RustExpr *expr;
    RustExpr *cond;
    struct RustStmt *then_head;
    struct RustStmt *else_head;
    struct RustStmt *body_head;
    struct RustStmt *next;
} RustStmt;

typedef struct RustFunction {
    char name[128];
    RustSymbolId symbol_id;
    int name_line;
    int name_col;
    char ret_type[64];
    int has_explicit_ret_type;
    int num_params;
    char param_names[MAX_PARAMS][128];
    char param_types[MAX_PARAMS][64];
    int param_lines[MAX_PARAMS];
    int param_cols[MAX_PARAMS];
    RustSymbolId param_symbol_ids[MAX_PARAMS];
    RustStmt *body_head;
    int stmt_count;
    struct RustFunction *next;
} RustFunction;

struct RustAst {
    RustFunction *functions;
    int function_count;
};

typedef struct RustParser {
    const char *filename;
    const char *src;
    int len;
    int pos;
    int line;
    int col;
    RustToken tk;
    RustDiag diags[256];
    int num_diags;
    RustAst ast;
} RustParser;

static void rust_add_diag(RustParser *p, const char *code, int line, int col, const char *msg, const char *hint) {
    RustDiag *d;
    if (p->num_diags >= 256) return;
    d = &p->diags[p->num_diags++];
    d->kind = 0;
    strncpy(d->code, code, 31);
    d->code[31] = 0;
    d->line = line;
    d->col = col;
    strncpy(d->message, msg, 255);
    d->message[255] = 0;
    if (hint) {
        strncpy(d->hint, hint, 255);
        d->hint[255] = 0;
    } else {
        d->hint[0] = 0;
    }
}

static int rust_peek(RustParser *p) {
    if (p->pos >= p->len) return 0;
    return p->src[p->pos];
}

static int rust_read(RustParser *p) {
    int c = rust_peek(p);
    if (!c) return 0;
    p->pos++;
    if (c == '\n') {
        p->line++;
        p->col = 1;
    } else {
        p->col++;
    }
    return c;
}

static int rust_is_space(int c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static int rust_is_alpha(int c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static int rust_is_digit(int c) {
    return c >= '0' && c <= '9';
}

static void rust_skip_ws_comments(RustParser *p) {
    int c;
    for (;;) {
        c = rust_peek(p);
        while (rust_is_space(c)) {
            rust_read(p);
            c = rust_peek(p);
        }
        if (c == '/' && p->pos + 1 < p->len && p->src[p->pos + 1] == '/') {
            while (c && c != '\n') c = rust_read(p);
            continue;
        }
        break;
    }
}

static int rust_kw(const char *s) {
    if (strcmp(s, "fn") == 0) return RUST_TK_FN;
    if (strcmp(s, "let") == 0) return RUST_TK_LET;
    if (strcmp(s, "return") == 0) return RUST_TK_RETURN;
    if (strcmp(s, "if") == 0) return RUST_TK_IF;
    if (strcmp(s, "else") == 0) return RUST_TK_ELSE;
    if (strcmp(s, "while") == 0) return RUST_TK_WHILE;
    if (strcmp(s, "true") == 0) return RUST_TK_TRUE;
    if (strcmp(s, "false") == 0) return RUST_TK_FALSE;
    if (strcmp(s, "i32") == 0) return RUST_TK_I32;
    return RUST_TK_IDENT;
}

static void rust_next(RustParser *p) {
    int c;
    int i;
    rust_skip_ws_comments(p);
    p->tk.kind = RUST_TK_EOF;
    p->tk.text[0] = 0;
    p->tk.num = 0;
    p->tk.line = p->line;
    p->tk.col = p->col;
    c = rust_peek(p);
    if (!c) return;
    if (rust_is_alpha(c)) {
        i = 0;
        while (rust_is_alpha(rust_peek(p)) || rust_is_digit(rust_peek(p))) {
            if (i < 127) p->tk.text[i++] = (char)rust_read(p);
            else rust_read(p);
        }
        p->tk.text[i] = 0;
        p->tk.kind = rust_kw(p->tk.text);
        return;
    }
    if (rust_is_digit(c)) {
        long long v = 0;
        while (rust_is_digit(rust_peek(p))) {
            v = v * 10 + (rust_read(p) - '0');
        }
        p->tk.kind = RUST_TK_NUM;
        p->tk.num = v;
        return;
    }
    if (c == '-' && p->pos + 1 < p->len && p->src[p->pos + 1] == '>') {
        rust_read(p); rust_read(p);
        p->tk.kind = RUST_TK_ARROW;
        return;
    }
    if (c == '=' && p->pos + 1 < p->len && p->src[p->pos + 1] == '=') {
        rust_read(p); rust_read(p);
        p->tk.kind = RUST_TK_EQ;
        return;
    }
    if (c == '!' && p->pos + 1 < p->len && p->src[p->pos + 1] == '=') {
        rust_read(p); rust_read(p);
        p->tk.kind = RUST_TK_NE;
        return;
    }
    if (c == '<' && p->pos + 1 < p->len && p->src[p->pos + 1] == '=') {
        rust_read(p); rust_read(p);
        p->tk.kind = RUST_TK_LE;
        return;
    }
    if (c == '&' && p->pos + 1 < p->len && p->src[p->pos + 1] == '&') {
        rust_read(p); rust_read(p);
        p->tk.kind = RUST_TK_LAND;
        return;
    }
    if (c == '|' && p->pos + 1 < p->len && p->src[p->pos + 1] == '|') {
        rust_read(p); rust_read(p);
        p->tk.kind = RUST_TK_LOR;
        return;
    }
    if (c == '>' && p->pos + 1 < p->len && p->src[p->pos + 1] == '=') {
        rust_read(p); rust_read(p);
        p->tk.kind = RUST_TK_GE;
        return;
    }
    rust_read(p);
    if (c == '(') p->tk.kind = RUST_TK_LPAREN;
    else if (c == ')') p->tk.kind = RUST_TK_RPAREN;
    else if (c == '{') p->tk.kind = RUST_TK_LBRACE;
    else if (c == '}') p->tk.kind = RUST_TK_RBRACE;
    else if (c == ':') p->tk.kind = RUST_TK_COLON;
    else if (c == ';') p->tk.kind = RUST_TK_SEMI;
    else if (c == ',') p->tk.kind = RUST_TK_COMMA;
    else if (c == '=') p->tk.kind = RUST_TK_ASSIGN;
    else if (c == '!') p->tk.kind = RUST_TK_BANG;
    else if (c == '<') p->tk.kind = RUST_TK_LT;
    else if (c == '>') p->tk.kind = RUST_TK_GT;
    else if (c == '+') p->tk.kind = RUST_TK_PLUS;
    else if (c == '-') p->tk.kind = RUST_TK_MINUS;
    else if (c == '*') p->tk.kind = RUST_TK_STAR;
    else if (c == '/') p->tk.kind = RUST_TK_SLASH;
    else {
        rust_add_diag(p, "RUSTLEX001", p->tk.line, p->tk.col, "unsupported character in Rust source", "remove or replace unsupported syntax");
        p->tk.kind = RUST_TK_EOF;
    }
}

static int rust_accept(RustParser *p, int kind) {
    if (p->tk.kind == kind) {
        rust_next(p);
        return 1;
    }
    return 0;
}

static int rust_expect(RustParser *p, int kind, const char *msg, const char *hint) {
    if (p->tk.kind == kind) {
        rust_next(p);
        return 1;
    }
    rust_add_diag(p, "RUSTPARSE001", p->tk.line, p->tk.col, msg, hint);
    return 0;
}

static RustExpr *rust_new_expr(int kind) {
    RustExpr *e = (RustExpr *)calloc(1, sizeof(RustExpr));
    if (!e) exit(1);
    e->kind = kind;
    e->symbol_id = RUST_SYMBOL_INVALID;
    return e;
}

static RustStmt *rust_new_stmt(int kind, int line) {
    RustStmt *s = (RustStmt *)calloc(1, sizeof(RustStmt));
    if (!s) exit(1);
    s->kind = kind;
    s->line = line;
    s->symbol_id = RUST_SYMBOL_INVALID;
    return s;
}

static RustExpr *rust_parse_expr(RustParser *p);
static RustExpr *rust_parse_call_from_ident(RustParser *p, const char *callee_name, int line, int col);
static RustExpr *rust_parse_cmp(RustParser *p);
static RustExpr *rust_parse_add(RustParser *p);
static RustExpr *rust_parse_lor(RustParser *p);
static RustExpr *rust_parse_land(RustParser *p);
static RustExpr *rust_parse_unary(RustParser *p);

static RustExpr *rust_parse_primary(RustParser *p) {
    RustExpr *e;
    if (p->tk.kind == RUST_TK_NUM) {
        e = rust_new_expr(RUST_EXPR_NUM);
        e->num = p->tk.num;
        e->line = p->tk.line;
        e->col = p->tk.col;
        rust_next(p);
        return e;
    }
    if (p->tk.kind == RUST_TK_TRUE || p->tk.kind == RUST_TK_FALSE) {
        e = rust_new_expr(RUST_EXPR_BOOL);
        e->bool_val = (p->tk.kind == RUST_TK_TRUE) ? 1 : 0;
        e->line = p->tk.line;
        e->col = p->tk.col;
        rust_next(p);
        return e;
    }
    if (p->tk.kind == RUST_TK_IDENT) {
        char ident_name[128];
        int ident_line = p->tk.line;
        int ident_col = p->tk.col;
        strncpy(ident_name, p->tk.text, 127);
        ident_name[127] = 0;
        e = rust_new_expr(RUST_EXPR_IDENT);
        strncpy(e->text, ident_name, 127);
        e->text[127] = 0;
        e->line = ident_line;
        e->col = ident_col;
        rust_next(p);
        if (p->tk.kind == RUST_TK_LPAREN) {
            return rust_parse_call_from_ident(p, ident_name, ident_line, ident_col);
        }
        return e;
    }
    if (rust_accept(p, RUST_TK_LPAREN)) {
        e = rust_parse_expr(p);
        rust_expect(p, RUST_TK_RPAREN, "expected ')' after expression", "close parenthesized expression");
        return e;
    }
    rust_add_diag(p, "RUSTPARSE002", p->tk.line, p->tk.col, "expected expression", "use identifier, integer literal, bool literal, or parenthesized expression");
    rust_next(p);
    return rust_new_expr(RUST_EXPR_NUM);
}

static RustExpr *rust_parse_call_from_ident(RustParser *p, const char *callee_name, int line, int col) {
    RustExpr *call = rust_new_expr(RUST_EXPR_CALL);
    RustExpr **args = 0;
    int arg_cap = 0;
    strncpy(call->call_callee, callee_name, 127);
    call->call_callee[127] = 0;
    call->line = line;
    call->col = col;
    call->call_callee_symbol_id = RUST_SYMBOL_INVALID;
    rust_expect(p, RUST_TK_LPAREN, "expected '(' after callee name", "call syntax is callee(arg1, arg2)");
    while (p->tk.kind != RUST_TK_RPAREN && p->tk.kind != RUST_TK_EOF) {
        RustExpr *arg = rust_parse_expr(p);
        if (call->call_arg_count == arg_cap) {
            int next_cap = arg_cap ? arg_cap * 2 : 4;
            RustExpr **new_args = (RustExpr **)realloc(args, (size_t)next_cap * sizeof(RustExpr *));
            if (!new_args) {
                rust_add_diag(p, "RUSTPARSE011", p->tk.line, p->tk.col, "out of memory parsing call arguments", "simplify call expression");
                break;
            }
            args = new_args;
            arg_cap = next_cap;
        }
        args[call->call_arg_count++] = arg;
        if (!rust_accept(p, RUST_TK_COMMA)) break;
    }
    rust_expect(p, RUST_TK_RPAREN, "expected ')' after call arguments", "close call argument list");
    call->call_args = args;
    return call;
}

static RustExpr *rust_parse_mul(RustParser *p) {
    RustExpr *lhs = rust_parse_unary(p);
    while (p->tk.kind == RUST_TK_STAR || p->tk.kind == RUST_TK_SLASH) {
        RustExpr *bin = rust_new_expr(RUST_EXPR_BINARY);
        bin->op = p->tk.kind;
        bin->line = p->tk.line;
        bin->col = p->tk.col;
        rust_next(p);
        bin->lhs = lhs;
        bin->rhs = rust_parse_unary(p);
        lhs = bin;
    }
    return lhs;
}

static RustExpr *rust_parse_expr(RustParser *p) {
    return rust_parse_lor(p);
}

static RustExpr *rust_parse_lor(RustParser *p) {
    RustExpr *lhs = rust_parse_land(p);
    while (p->tk.kind == RUST_TK_LOR) {
        RustExpr *bin = rust_new_expr(RUST_EXPR_BINARY);
        bin->op = p->tk.kind;
        bin->line = p->tk.line;
        bin->col = p->tk.col;
        rust_next(p);
        bin->lhs = lhs;
        bin->rhs = rust_parse_land(p);
        lhs = bin;
    }
    return lhs;
}

static RustExpr *rust_parse_land(RustParser *p) {
    RustExpr *lhs = rust_parse_cmp(p);
    while (p->tk.kind == RUST_TK_LAND) {
        RustExpr *bin = rust_new_expr(RUST_EXPR_BINARY);
        bin->op = p->tk.kind;
        bin->line = p->tk.line;
        bin->col = p->tk.col;
        rust_next(p);
        bin->lhs = lhs;
        bin->rhs = rust_parse_cmp(p);
        lhs = bin;
    }
    return lhs;
}

static RustExpr *rust_parse_add(RustParser *p) {
    RustExpr *lhs = rust_parse_mul(p);
    while (p->tk.kind == RUST_TK_PLUS || p->tk.kind == RUST_TK_MINUS) {
        RustExpr *bin = rust_new_expr(RUST_EXPR_BINARY);
        bin->op = p->tk.kind;
        bin->line = p->tk.line;
        bin->col = p->tk.col;
        rust_next(p);
        bin->lhs = lhs;
        bin->rhs = rust_parse_mul(p);
        lhs = bin;
    }
    return lhs;
}

static RustExpr *rust_parse_cmp(RustParser *p) {
    RustExpr *lhs = rust_parse_add(p);
    while (p->tk.kind == RUST_TK_EQ || p->tk.kind == RUST_TK_NE ||
           p->tk.kind == RUST_TK_LT || p->tk.kind == RUST_TK_LE ||
           p->tk.kind == RUST_TK_GT || p->tk.kind == RUST_TK_GE) {
        RustExpr *bin = rust_new_expr(RUST_EXPR_BINARY);
        bin->op = p->tk.kind;
        bin->line = p->tk.line;
        bin->col = p->tk.col;
        rust_next(p);
        bin->lhs = lhs;
        bin->rhs = rust_parse_add(p);
        lhs = bin;
    }
    return lhs;
}

static RustExpr *rust_parse_unary(RustParser *p) {
    if (p->tk.kind == RUST_TK_BANG) {
        RustExpr *u = rust_new_expr(RUST_EXPR_UNARY);
        u->op = RUST_TK_BANG;
        u->line = p->tk.line;
        u->col = p->tk.col;
        rust_next(p);
        u->lhs = rust_parse_unary(p);
        return u;
    }
    return rust_parse_primary(p);
}

static void rust_append_stmt(RustStmt **head, RustStmt *s) {
    RustStmt *cur;
    if (!*head) {
        *head = s;
        return;
    }
    cur = *head;
    while (cur->next) cur = cur->next;
    cur->next = s;
}

static RustStmt *rust_parse_stmt(RustParser *p);

static int rust_next_token_kind_lookahead(const RustParser *p) {
    RustParser probe;
    if (!p) return RUST_TK_EOF;
    probe = *p;
    rust_next(&probe);
    return probe.tk.kind;
}

static int rust_parse_type_name(RustParser *p, char *buf, int buf_len) {
    if (!p || !buf || buf_len <= 0) return 0;
    if (p->tk.kind == RUST_TK_I32) {
        strncpy(buf, "i32", (size_t)buf_len - 1);
        buf[buf_len - 1] = 0;
        rust_next(p);
        return 1;
    }
    if (p->tk.kind == RUST_TK_IDENT && strcmp(p->tk.text, "bool") == 0) {
        strncpy(buf, "bool", (size_t)buf_len - 1);
        buf[buf_len - 1] = 0;
        rust_next(p);
        return 1;
    }
    return 0;
}

static RustStmt *rust_parse_block(RustParser *p) {
    RustStmt *head = 0;
    if (!rust_expect(p, RUST_TK_LBRACE, "expected '{' to begin block", "add block braces")) return head;
    while (p->tk.kind != RUST_TK_RBRACE && p->tk.kind != RUST_TK_EOF) {
        RustStmt *s = rust_parse_stmt(p);
        if (s) rust_append_stmt(&head, s);
    }
    rust_expect(p, RUST_TK_RBRACE, "expected '}' to end block", "close block opened with '{'");
    return head;
}

static RustStmt *rust_parse_stmt(RustParser *p) {
    RustStmt *s;
    int line = p->tk.line;
    int col = p->tk.col;
    if (rust_accept(p, RUST_TK_LET)) {
        s = rust_new_stmt(RUST_STMT_LET, line);
        s->col = col;
        if (p->tk.kind == RUST_TK_IDENT && strcmp(p->tk.text, "mut") == 0) {
            s->is_mut = 1;
            rust_next(p);
        }
        if (p->tk.kind == RUST_TK_IDENT) {
            strncpy(s->name, p->tk.text, 127);
            s->name[127] = 0;
            s->name_line = p->tk.line;
            s->name_col = p->tk.col;
            rust_next(p);
        } else {
            rust_add_diag(p, "RUSTPARSE003", p->tk.line, p->tk.col, "expected identifier after 'let'", "declare variable name");
        }
        if (rust_accept(p, RUST_TK_COLON)) {
            s->has_type_annotation = 1;
            if (!rust_parse_type_name(p, s->type_name, 64)) {
                rust_add_diag(p, "RUSTPARSE004", p->tk.line, p->tk.col, "expected type in let binding", "v1 supports only 'i32' and 'bool'");
            }
        } else {
            s->has_type_annotation = 0;
            strncpy(s->type_name, "i32", 63);
        }
        rust_expect(p, RUST_TK_ASSIGN, "expected '=' in let binding", "initialize let binding with expression");
        s->expr = rust_parse_expr(p);
        rust_expect(p, RUST_TK_SEMI, "expected ';' after let statement", "terminate statement with semicolon");
        return s;
    }
    if (rust_accept(p, RUST_TK_RETURN)) {
        s = rust_new_stmt(RUST_STMT_RETURN, line);
        s->col = col;
        s->expr = rust_parse_expr(p);
        rust_expect(p, RUST_TK_SEMI, "expected ';' after return statement", "terminate return with semicolon");
        return s;
    }
    if (rust_accept(p, RUST_TK_IF)) {
        s = rust_new_stmt(RUST_STMT_IF, line);
        s->col = col;
        s->cond = rust_parse_expr(p);
        s->then_head = rust_parse_block(p);
        if (rust_accept(p, RUST_TK_ELSE)) {
            s->else_head = rust_parse_block(p);
        }
        return s;
    }
    if (rust_accept(p, RUST_TK_WHILE)) {
        s = rust_new_stmt(RUST_STMT_WHILE, line);
        s->col = col;
        s->cond = rust_parse_expr(p);
        s->body_head = rust_parse_block(p);
        return s;
    }
    if (p->tk.kind == RUST_TK_IDENT && rust_next_token_kind_lookahead(p) == RUST_TK_ASSIGN) {
        s = rust_new_stmt(RUST_STMT_ASSIGN, line);
        s->col = col;
        strncpy(s->name, p->tk.text, 127);
        s->name[127] = 0;
        s->name_line = p->tk.line;
        s->name_col = p->tk.col;
        rust_next(p);
        rust_expect(p, RUST_TK_ASSIGN, "expected '=' in assignment statement", "assign to existing mutable local with `name = expr;`");
        s->expr = rust_parse_expr(p);
        rust_expect(p, RUST_TK_SEMI, "expected ';' after assignment statement", "terminate statement with semicolon");
        return s;
    }
    s = rust_new_stmt(RUST_STMT_EXPR, line);
    s->col = col;
    s->expr = rust_parse_expr(p);
    rust_expect(p, RUST_TK_SEMI, "expected ';' after expression statement", "terminate statement with semicolon");
    return s;
}

static RustFunction *rust_parse_function(RustParser *p) {
    RustFunction *fn;
    RustStmt *cur;
    int param_idx;
    if (!rust_expect(p, RUST_TK_FN, "expected 'fn' at top level", "declare functions with 'fn name() -> i32 { ... }'")) return 0;
    fn = (RustFunction *)calloc(1, sizeof(RustFunction));
    if (!fn) exit(1);
    if (p->tk.kind == RUST_TK_IDENT) {
        strncpy(fn->name, p->tk.text, 127);
        fn->name[127] = 0;
        fn->name_line = p->tk.line;
        fn->name_col = p->tk.col;
        rust_next(p);
    } else {
        rust_add_diag(p, "RUSTPARSE005", p->tk.line, p->tk.col, "expected function name", "provide identifier after 'fn'");
        strncpy(fn->name, "<anon>", 127);
    }
    rust_expect(p, RUST_TK_LPAREN, "expected '(' after function name", "write function parameter list");
    while (p->tk.kind != RUST_TK_RPAREN && p->tk.kind != RUST_TK_EOF) {
        if (fn->num_params >= MAX_PARAMS) {
            rust_add_diag(p, "RUSTPARSE008", p->tk.line, p->tk.col, "too many function parameters", "reduce parameter count");
            break;
        }
        param_idx = fn->num_params;
        if (p->tk.kind == RUST_TK_IDENT) {
            strncpy(fn->param_names[param_idx], p->tk.text, 127);
            fn->param_names[param_idx][127] = 0;
            fn->param_lines[param_idx] = p->tk.line;
            fn->param_cols[param_idx] = p->tk.col;
            rust_next(p);
        } else {
            rust_add_diag(p, "RUSTPARSE009", p->tk.line, p->tk.col, "expected parameter name", "use syntax: name: i32");
            break;
        }
        rust_expect(p, RUST_TK_COLON, "expected ':' in parameter", "use syntax: name: i32");
        if (!rust_parse_type_name(p, fn->param_types[param_idx], 64)) {
            rust_add_diag(p, "RUSTPARSE010", p->tk.line, p->tk.col, "expected parameter type", "v1 supports only i32 and bool parameters");
            strncpy(fn->param_types[param_idx], "i32", 63);
        }
        fn->param_symbol_ids[param_idx] = RUST_SYMBOL_INVALID;
        fn->num_params++;
        if (!rust_accept(p, RUST_TK_COMMA)) break;
    }
    rust_expect(p, RUST_TK_RPAREN, "expected ')' after parameter list", "close parameter list");
    if (rust_accept(p, RUST_TK_ARROW)) {
        fn->has_explicit_ret_type = 1;
        if (!rust_parse_type_name(p, fn->ret_type, 64)) {
            rust_add_diag(p, "RUSTPARSE006", p->tk.line, p->tk.col, "expected return type after '->'", "v1 supports only i32 and bool return types");
            strncpy(fn->ret_type, "i32", 63);
        }
    } else {
        fn->has_explicit_ret_type = 0;
        strncpy(fn->ret_type, "i32", 63);
    }
    fn->body_head = rust_parse_block(p);
    cur = fn->body_head;
    while (cur) {
        fn->stmt_count++;
        cur = cur->next;
    }
    return fn;
}

static RustAst *rust_parse_program_internal(RustParser *p) {
    RustFunction *tail = 0;
    rust_next(p);
    while (p->tk.kind != RUST_TK_EOF) {
        RustFunction *fn;
        if (p->tk.kind != RUST_TK_FN) {
            rust_add_diag(p, "RUSTPARSE007", p->tk.line, p->tk.col, "expected top-level function", "v1 frontend accepts only top-level fn items");
            rust_next(p);
            continue;
        }
        fn = rust_parse_function(p);
        if (!fn) break;
        if (!p->ast.functions) p->ast.functions = fn;
        else tail->next = fn;
        tail = fn;
        p->ast.function_count++;
    }
    return &p->ast;
}

static void rust_dump_expr(FILE *out, RustExpr *e, int indent) {
    int i;
    for (i = 0; i < indent; i++) fprintf(out, " ");
    if (!e) {
        fprintf(out, "Expr <null>\n");
        return;
    }
    if (e->kind == RUST_EXPR_IDENT) fprintf(out, "Expr Ident %s\n", e->text);
    else if (e->kind == RUST_EXPR_NUM) fprintf(out, "Expr Num %lld\n", e->num);
    else if (e->kind == RUST_EXPR_BOOL) fprintf(out, "Expr Bool %s\n", e->bool_val ? "true" : "false");
    else if (e->kind == RUST_EXPR_CALL) {
        int ai;
        fprintf(out, "Expr Call %s argc=%d\n", e->call_callee, e->call_arg_count);
        for (ai = 0; ai < e->call_arg_count; ai++) {
            rust_dump_expr(out, e->call_args[ai], indent + 2);
        }
    }
    else if (e->kind == RUST_EXPR_UNARY) {
        fprintf(out, "Expr Unary op=%d\n", e->op);
        rust_dump_expr(out, e->lhs, indent + 2);
    }
    else {
        fprintf(out, "Expr Binary op=%d\n", e->op);
        rust_dump_expr(out, e->lhs, indent + 2);
        rust_dump_expr(out, e->rhs, indent + 2);
    }
}

static void rust_dump_stmt(FILE *out, RustStmt *s, int indent) {
    int i;
    while (s) {
        for (i = 0; i < indent; i++) fprintf(out, " ");
        if (s->kind == RUST_STMT_LET) {
            fprintf(out, "Stmt Let%s %s : %s\n", s->is_mut ? " mut" : "", s->name, s->type_name);
            rust_dump_expr(out, s->expr, indent + 2);
        } else if (s->kind == RUST_STMT_ASSIGN) {
            fprintf(out, "Stmt Assign %s\n", s->name);
            rust_dump_expr(out, s->expr, indent + 2);
        } else if (s->kind == RUST_STMT_RETURN) {
            fprintf(out, "Stmt Return\n");
            rust_dump_expr(out, s->expr, indent + 2);
        } else if (s->kind == RUST_STMT_IF) {
            fprintf(out, "Stmt If\n");
            for (i = 0; i < indent + 2; i++) fprintf(out, " ");
            fprintf(out, "Cond\n");
            rust_dump_expr(out, s->cond, indent + 4);
            for (i = 0; i < indent + 2; i++) fprintf(out, " ");
            fprintf(out, "Then\n");
            rust_dump_stmt(out, s->then_head, indent + 4);
            if (s->else_head) {
                for (i = 0; i < indent + 2; i++) fprintf(out, " ");
                fprintf(out, "Else\n");
                rust_dump_stmt(out, s->else_head, indent + 4);
            }
        } else if (s->kind == RUST_STMT_WHILE) {
            fprintf(out, "Stmt While\n");
            for (i = 0; i < indent + 2; i++) fprintf(out, " ");
            fprintf(out, "Cond\n");
            rust_dump_expr(out, s->cond, indent + 4);
            for (i = 0; i < indent + 2; i++) fprintf(out, " ");
            fprintf(out, "Body\n");
            rust_dump_stmt(out, s->body_head, indent + 4);
        } else {
            fprintf(out, "Stmt Expr\n");
            rust_dump_expr(out, s->expr, indent + 2);
        }
        s = s->next;
    }
}

static void rust_dump_ast(FILE *out, RustAst *ast) {
    RustFunction *fn = ast->functions;
    int pi;
    fprintf(out, "RustAst v1\n");
    fprintf(out, "Functions %d\n", ast->function_count);
    while (fn) {
        fprintf(out, "Function %s -> %s\n", fn->name, fn->ret_type[0] ? fn->ret_type : "i32");
        for (pi = 0; pi < fn->num_params; pi++) {
            fprintf(out, "  Param %s : %s\n", fn->param_names[pi], fn->param_types[pi][0] ? fn->param_types[pi] : "i32");
        }
        rust_dump_stmt(out, fn->body_head, 2);
        fn = fn->next;
    }
}

static void rust_dump_symbol_id(FILE *out, RustSymbolId id) {
    if (id == RUST_SYMBOL_INVALID) fprintf(out, "invalid");
    else fprintf(out, "%u", id);
}

typedef enum RustDumpNameMode {
    RUST_DUMP_NAME_UNQUOTED = 0,
    RUST_DUMP_NAME_QUOTED = 1
} RustDumpNameMode;

#define RUST_DUMP_NAME_MAX_BYTES 80

static void rust_append_text(char *buf, int cap, int *len, const char *text) {
    int i = 0;
    if (!buf || cap <= 0 || !len || !text) return;
    while (text[i] != '\0' && *len < cap - 1) {
        buf[*len] = text[i];
        (*len)++;
        i++;
    }
    buf[*len] = '\0';
}

static void rust_append_char(char *buf, int cap, int *len, char ch) {
    if (!buf || cap <= 0 || !len) return;
    if (*len >= cap - 1) return;
    buf[*len] = ch;
    (*len)++;
    buf[*len] = '\0';
}

static void rust_append_escaped_name(char *buf, int cap, int *len, const char *name, RustDumpNameMode mode) {
    int i;
    int emitted = 0;
    int raw_len;
    char tmp[32];
    (void)mode;
    if (!name) {
        rust_append_text(buf, cap, len, "<missing>");
        return;
    }
    raw_len = (int)strlen(name);
    for (i = 0; name[i] != '\0'; i++) {
        unsigned char c = (unsigned char)name[i];
        if (emitted >= RUST_DUMP_NAME_MAX_BYTES) {
            sprintf(tmp, "...(len=%d)", raw_len);
            rust_append_text(buf, cap, len, tmp);
            return;
        }
        if (c == '\n') {
            rust_append_text(buf, cap, len, "\\n"); emitted += 2;
        } else if (c == '\r') {
            rust_append_text(buf, cap, len, "\\r"); emitted += 2;
        } else if (c == '\t') {
            rust_append_text(buf, cap, len, "\\t"); emitted += 2;
        } else if (c == '\\') {
            rust_append_text(buf, cap, len, "\\\\"); emitted += 2;
        } else if (c == '"') {
            rust_append_text(buf, cap, len, "\\\""); emitted += 2;
        } else if (c < 0x20 || c == 0x7f) {
            sprintf(tmp, "\\x%02X", c);
            rust_append_text(buf, cap, len, tmp); emitted += 4;
        } else {
            rust_append_char(buf, cap, len, (char)c); emitted++;
        }
    }
}

static void rust_format_ident_message(char *buf, int cap, const char *prefix, const char *name, const char *suffix) {
    int len = 0;
    if (!buf || cap <= 0) return;
    buf[0] = '\0';
    rust_append_text(buf, cap, &len, prefix);
    rust_append_char(buf, cap, &len, '`');
    rust_append_escaped_name(buf, cap, &len, name, RUST_DUMP_NAME_UNQUOTED);
    rust_append_char(buf, cap, &len, '`');
    rust_append_text(buf, cap, &len, suffix);
}

static void rust_dump_name(FILE *out, const char *name, RustDumpNameMode mode) {
    size_t raw_len;
    size_t emitted;
    size_t i;
    (void)mode;
    if (!out) return;
    if (!name) {
        fputs("<missing>", out);
        return;
    }
    raw_len = strlen(name);
    emitted = 0;
    for (i = 0; name[i] != '\0'; i++) {
        unsigned char c = (unsigned char)name[i];
        if (emitted >= RUST_DUMP_NAME_MAX_BYTES) {
            fprintf(out, "...(len=%zu)", raw_len);
            return;
        }
        if (c == '\n') {
            fputs("\\n", out); emitted += 2;
        } else if (c == '\r') {
            fputs("\\r", out); emitted += 2;
        } else if (c == '\t') {
            fputs("\\t", out); emitted += 2;
        } else if (c == '\\') {
            fputs("\\\\", out); emitted += 2;
        } else if (c == '"') {
            fputs("\\\"", out); emitted += 2;
        } else if (c < 0x20 || c == 0x7f) {
            fprintf(out, "\\x%02X", c); emitted += 4;
        } else {
            fputc((int)c, out); emitted++;
        }
    }
}

static void rust_dump_expr_with_symbols(FILE *out, RustExpr *e, int indent) {
    int i;
    for (i = 0; i < indent; i++) fprintf(out, " ");
    if (!e) {
        fprintf(out, "Expr <null>\n");
        return;
    }
    if (e->kind == RUST_EXPR_IDENT) {
        fputs("Ident name=\"", out);
        rust_dump_name(out, e->text, RUST_DUMP_NAME_QUOTED);
        fputs("\" symbol=", out);
        rust_dump_symbol_id(out, e->symbol_id);
        fprintf(out, "\n");
    } else if (e->kind == RUST_EXPR_CALL) {
        int ai;
        fputs("Call callee=\"", out);
        rust_dump_name(out, e->call_callee, RUST_DUMP_NAME_QUOTED);
        fputs("\" symbol=", out);
        rust_dump_symbol_id(out, e->call_callee_symbol_id);
        fprintf(out, " argc=%d\n", e->call_arg_count);
        for (ai = 0; ai < e->call_arg_count; ai++) {
            rust_dump_expr_with_symbols(out, e->call_args[ai], indent + 2);
        }
    } else if (e->kind == RUST_EXPR_UNARY) {
        fprintf(out, "Unary op=%d\n", e->op);
        rust_dump_expr_with_symbols(out, e->lhs, indent + 2);
    } else if (e->kind == RUST_EXPR_NUM) {
        fprintf(out, "Int %lld\n", e->num);
    } else if (e->kind == RUST_EXPR_BOOL) {
        fprintf(out, "Bool %s\n", e->bool_val ? "true" : "false");
    } else {
        fprintf(out, "Binary op=%d\n", e->op);
        rust_dump_expr_with_symbols(out, e->lhs, indent + 2);
        rust_dump_expr_with_symbols(out, e->rhs, indent + 2);
    }
}

static void rust_dump_stmt_with_symbols(FILE *out, RustStmt *s, int indent) {
    int i;
    while (s) {
        for (i = 0; i < indent; i++) fprintf(out, " ");
        if (s->kind == RUST_STMT_LET) {
            fputs(s->is_mut ? "LetMut name=\"" : "Let name=\"", out);
            rust_dump_name(out, s->name, RUST_DUMP_NAME_QUOTED);
            fputs("\" symbol=", out);
            rust_dump_symbol_id(out, s->symbol_id);
            fprintf(out, "\n");
            rust_dump_expr_with_symbols(out, s->expr, indent + 2);
        } else if (s->kind == RUST_STMT_ASSIGN) {
            fputs("Assign name=\"", out);
            rust_dump_name(out, s->name, RUST_DUMP_NAME_QUOTED);
            fputs("\" symbol=", out);
            rust_dump_symbol_id(out, s->symbol_id);
            fprintf(out, "\n");
            rust_dump_expr_with_symbols(out, s->expr, indent + 2);
        } else if (s->kind == RUST_STMT_RETURN) {
            fprintf(out, "Return\n");
            rust_dump_expr_with_symbols(out, s->expr, indent + 2);
        } else if (s->kind == RUST_STMT_IF) {
            fprintf(out, "If\n");
            for (i = 0; i < indent + 2; i++) fprintf(out, " ");
            fprintf(out, "Cond\n");
            rust_dump_expr_with_symbols(out, s->cond, indent + 4);
            for (i = 0; i < indent + 2; i++) fprintf(out, " ");
            fprintf(out, "Then\n");
            rust_dump_stmt_with_symbols(out, s->then_head, indent + 4);
            if (s->else_head) {
                for (i = 0; i < indent + 2; i++) fprintf(out, " ");
                fprintf(out, "Else\n");
                rust_dump_stmt_with_symbols(out, s->else_head, indent + 4);
            }
        } else if (s->kind == RUST_STMT_WHILE) {
            fprintf(out, "While\n");
            for (i = 0; i < indent + 2; i++) fprintf(out, " ");
            fprintf(out, "Cond\n");
            rust_dump_expr_with_symbols(out, s->cond, indent + 4);
            for (i = 0; i < indent + 2; i++) fprintf(out, " ");
            fprintf(out, "Body\n");
            rust_dump_stmt_with_symbols(out, s->body_head, indent + 4);
        } else {
            fprintf(out, "ExprStmt\n");
            rust_dump_expr_with_symbols(out, s->expr, indent + 2);
        }
        s = s->next;
    }
}

static void rust_dump_ast_with_symbols(FILE *out, RustAst *ast) {
    RustFunction *fn = ast->functions;
    int pi;
    fprintf(out, "RustAstSymbols v1\n");
    while (fn) {
        fputs("Function name=\"", out);
        rust_dump_name(out, fn->name, RUST_DUMP_NAME_QUOTED);
        fputs("\" symbol=", out);
        rust_dump_symbol_id(out, fn->symbol_id);
        fprintf(out, " return=%s\n", fn->ret_type[0] ? fn->ret_type : "i32");
        for (pi = 0; pi < fn->num_params; pi++) {
            fputs("  Param name=\"", out);
            rust_dump_name(out, fn->param_names[pi], RUST_DUMP_NAME_QUOTED);
            fputs("\" symbol=", out);
            rust_dump_symbol_id(out, fn->param_symbol_ids[pi]);
            fprintf(out, "\n");
        }
        fprintf(out, "  Block\n");
        rust_dump_stmt_with_symbols(out, fn->body_head, 4);
        fn = fn->next;
    }
}

static const char *rust_symbol_kind_name(RustSymbolKind kind) {
    if (kind == RUST_SYMBOL_FUNCTION) return "function";
    if (kind == RUST_SYMBOL_PARAM) return "param";
    if (kind == RUST_SYMBOL_LOCAL) return "local";
    return "unknown";
}

static void rust_dump_symbol_table(const RustResolveContext *ctx) {
    int i;
    if (!ctx || !ctx->symbols) return;
    for (i = 0; i < ctx->symbol_count; i++) {
        const RustSymbol *sym = &ctx->symbols[i];
        const char *fname = ctx->filename ? ctx->filename : "<unknown>";
        printf("symbol %u %s ", sym->id, rust_symbol_kind_name(sym->kind));
        rust_dump_name(stdout, sym->name ? sym->name : "<anon>", RUST_DUMP_NAME_UNQUOTED);
        printf(" %s:%d:%d scope=%u\n", fname, sym->line, sym->col, sym->scope_depth);
    }
}

static void rust_print_diag(const char *filename, RustDiag *d) {
    FILE *out = stderr;
    if (!d) return;
    if (d->kind == 1) {
        fprintf(out, "note: %s\n", d->message);
        fprintf(out, "  --> %s:%d:%d\n", filename, d->line, d->col);
        return;
    }
    fprintf(out, "error[%s]: %s\n", d->code, d->message);
    fprintf(out, "  --> %s:%d:%d\n", filename, d->line, d->col);
}

static void rust_print_diags(const char *filename, RustDiag *diags, int count) {
    int i = 0;
    while (i < count) {
        RustDiag *d = &diags[i];
        if (d->kind == 1) {
            rust_print_diag(filename, d);
            i++;
            continue;
        }
        rust_print_diag(filename, d);
        i++;
        while (i < count && diags[i].kind == 1) {
            rust_print_diag(filename, &diags[i]);
            i++;
        }
        if (d->hint[0]) {
            fprintf(stderr, "  hint: %s\n", d->hint);
        }
    }
}

typedef struct RustBinding {
    const char *name;
    RustSymbolId symbol_id;
    int line;
    int col;
} RustBinding;

typedef struct RustScope {
    RustBinding *bindings;
    int binding_count;
    int binding_capacity;
} RustScope;

typedef struct RustResolver {
    RustResolveContext *ctx;
    RustScope *scopes;
    int scope_count;
    int scope_capacity;
    RustSymbolId next_symbol_id;
} RustResolver;

static int rust_resolver_add_diag(RustResolveContext *ctx, const char *code, int line, int col, const char *msg, const char *hint) {
    RustDiag *d;
    if (!ctx || !ctx->diags || !ctx->num_diags) return 0;
    if (*(ctx->num_diags) >= ctx->max_diags) return 0;
    d = &ctx->diags[*(ctx->num_diags)];
    d->kind = 0;
    strncpy(d->code, code, 31); d->code[31] = 0;
    d->line = line;
    d->col = col;
    strncpy(d->message, msg, 255); d->message[255] = 0;
    if (hint) {
        strncpy(d->hint, hint, 255); d->hint[255] = 0;
    } else {
        d->hint[0] = 0;
    }
    (*(ctx->num_diags))++;
    ctx->had_error = 1;
    return 1;
}

static int rust_resolver_add_note(RustResolveContext *ctx, int line, int col, const char *msg) {
    RustDiag *d;
    if (!ctx || !ctx->diags || !ctx->num_diags) return 0;
    if (*(ctx->num_diags) >= ctx->max_diags) return 0;
    d = &ctx->diags[*(ctx->num_diags)];
    d->kind = 1;
    d->code[0] = 0;
    d->line = line;
    d->col = col;
    strncpy(d->message, msg, 255); d->message[255] = 0;
    d->hint[0] = 0;
    (*(ctx->num_diags))++;
    return 1;
}

static int rust_typecheck_add_diag(RustTypecheckContext *ctx, const char *code, int line, int col, const char *msg, const char *hint) {
    RustDiag *d;
    if (!ctx || !ctx->diags || !ctx->num_diags) return 0;
    if (*(ctx->num_diags) >= ctx->max_diags) return 0;
    d = &ctx->diags[*(ctx->num_diags)];
    d->kind = 0;
    strncpy(d->code, code, 31); d->code[31] = 0;
    d->line = line;
    d->col = col;
    strncpy(d->message, msg, 255); d->message[255] = 0;
    if (hint) {
        strncpy(d->hint, hint, 255); d->hint[255] = 0;
    } else {
        d->hint[0] = 0;
    }
    (*(ctx->num_diags))++;
    ctx->had_error = 1;
    return 1;
}

static int rust_lower_add_diag(RustLowerContext *ctx, int line, int col, const char *msg, const char *hint) {
    RustDiag *d;
    if (!ctx || !ctx->diags || !ctx->num_diags) return 0;
    if (*(ctx->num_diags) >= ctx->max_diags) return 0;
    d = &ctx->diags[*(ctx->num_diags)];
    d->kind = 0;
    strncpy(d->code, "RUST-LOWER-E9999", 31); d->code[31] = 0;
    d->line = line;
    d->col = col;
    strncpy(d->message, msg, 255); d->message[255] = 0;
    if (hint) {
        strncpy(d->hint, hint, 255); d->hint[255] = 0;
    } else {
        d->hint[0] = 0;
    }
    (*(ctx->num_diags))++;
    ctx->had_error = 1;
    return 1;
}

static const char *rust_type_name(RustTypeKind ty);

static RustTypeKind rust_type_from_symbol(RustTypecheckContext *ctx, RustSymbolId id, int line, int col) {
    if (id == RUST_SYMBOL_INVALID) {
        rust_typecheck_add_diag(ctx, "RUST-TYPE-E9999", line, col, "unresolved symbol reached typechecker", "resolver must succeed before typecheck");
        return RUST_TYPE_ERROR;
    }
    if (!ctx->symbol_types || id >= (RustSymbolId)ctx->symbol_types_len) {
        rust_typecheck_add_diag(ctx, "RUST-TYPE-E9999", line, col, "missing symbol type information in typechecker", "resolver/typecheck contract broken");
        return RUST_TYPE_ERROR;
    }
    return (RustTypeKind)ctx->symbol_types[id];
}

static const RustSymbol *rust_type_find_symbol(RustTypecheckContext *ctx, RustSymbolId id) {
    int i;
    if (!ctx || !ctx->symbols || id == RUST_SYMBOL_INVALID) return 0;
    for (i = 0; i < ctx->symbol_count; i++) {
        if (ctx->symbols[i].id == id) return &ctx->symbols[i];
    }
    return 0;
}

static RustFunction *rust_find_function_by_symbol_id(RustAst *ast, RustSymbolId id) {
    RustFunction *fn;
    if (!ast || id == RUST_SYMBOL_INVALID) return 0;
    fn = ast->functions;
    while (fn) {
        if (fn->symbol_id == id) return fn;
        fn = fn->next;
    }
    return 0;
}

static void rust_type_assign_symbol(RustTypecheckContext *ctx, RustSymbolId id, RustTypeKind ty, int line, int col) {
    if (id == RUST_SYMBOL_INVALID) {
        rust_typecheck_add_diag(ctx, "RUST-TYPE-E9999", line, col, "unresolved symbol reached typechecker", "resolver must succeed before typecheck");
        return;
    }
    if (!ctx->symbol_types || id >= (RustSymbolId)ctx->symbol_types_len) {
        rust_typecheck_add_diag(ctx, "RUST-TYPE-E9999", line, col, "missing symbol type information in typechecker", "resolver/typecheck contract broken");
        return;
    }
    ctx->symbol_types[id] = (int)ty;
}

static void rust_type_report_expected(RustTypecheckContext *ctx, const char *code, int line, int col, RustTypeKind expected, RustTypeKind got, const char *hint_prefix) {
    char msg[256];
    int n = 0;
    msg[0] = '\0';
    n += snprintf(msg + n, sizeof(msg) - (size_t)n, "expected `%s`, found `%s`", rust_type_name(expected), rust_type_name(got));
    if (hint_prefix && hint_prefix[0]) {
        rust_typecheck_add_diag(ctx, code, line, col, msg, hint_prefix);
    } else {
        rust_typecheck_add_diag(ctx, code, line, col, msg, 0);
    }
}

static void rust_type_report_missing_return(RustTypecheckContext *ctx, const char *fn_name, int line, int col) {
    char msg[256];
    rust_format_ident_message(msg, 256, "missing return from function ", fn_name, "");
    rust_typecheck_add_diag(ctx, "RUST-TYPE-E0004", line, col, msg, "add a return expression on all control-flow paths");
}

static int rust_resolver_grow(void **items, int *cap, int elem_size) {
    int old = *cap;
    int next = old ? old * 2 : 8;
    void *mem;
    if (next < old) return 0;
    mem = realloc(*items, (size_t)next * (size_t)elem_size);
    if (!mem) return 0;
    *items = mem;
    *cap = next;
    return 1;
}

static int rust_resolver_push_scope(RustResolver *r) {
    RustScope *s;
    if (r->scope_count == r->scope_capacity) {
        if (!rust_resolver_grow((void **)&r->scopes, &r->scope_capacity, (int)sizeof(RustScope))) return 0;
    }
    s = &r->scopes[r->scope_count++];
    s->bindings = 0;
    s->binding_count = 0;
    s->binding_capacity = 0;
    return 1;
}

static void rust_resolver_pop_scope(RustResolver *r) {
    RustScope *s;
    if (!r || r->scope_count <= 0) return;
    s = &r->scopes[r->scope_count - 1];
    free(s->bindings);
    r->scope_count--;
}

static RustBinding *rust_find_current_scope(RustResolver *r, const char *name) {
    RustScope *s;
    int i;
    if (!r || r->scope_count <= 0 || !name) return 0;
    s = &r->scopes[r->scope_count - 1];
    for (i = 0; i < s->binding_count; i++) {
        if (strcmp(s->bindings[i].name, name) == 0) return &s->bindings[i];
    }
    return 0;
}

static RustBinding *rust_find_visible(RustResolver *r, const char *name) {
    int si;
    if (!r || !name) return 0;
    for (si = r->scope_count - 1; si >= 0; si--) {
        RustScope *s = &r->scopes[si];
        int i;
        for (i = 0; i < s->binding_count; i++) {
            if (strcmp(s->bindings[i].name, name) == 0) return &s->bindings[i];
        }
    }
    return 0;
}

static RustSymbolId rust_add_symbol(RustResolver *r, RustSymbolKind kind, const char *name, int line, int col) {
    RustResolveContext *ctx = r->ctx;
    RustSymbol *sym;
    RustSymbolId id;
    if (ctx->symbol_count == ctx->symbol_capacity) {
        if (!rust_resolver_grow((void **)&ctx->symbols, &ctx->symbol_capacity, (int)sizeof(RustSymbol))) {
            rust_resolver_add_diag(ctx, "RUST-E9999", line, col, "out of memory while creating symbol", "try simplifying the source file");
            return RUST_SYMBOL_INVALID;
        }
    }
    id = r->next_symbol_id++;
    sym = &ctx->symbols[ctx->symbol_count++];
    sym->id = id;
    sym->kind = kind;
    sym->name = name;
    sym->line = line;
    sym->col = col;
    sym->scope_depth = (unsigned)((r->scope_count > 0) ? (r->scope_count - 1) : 0);
    return id;
}

static RustSymbolId rust_declare(RustResolver *r, RustSymbolKind kind, const char *name, int line, int col) {
    RustScope *scope;
    RustBinding *b;
    RustBinding *exists;
    RustSymbolId id;
    char msg[256];
    if (!r || !name || !name[0]) return RUST_SYMBOL_INVALID;
    if (r->scope_count == 0) {
        if (!rust_resolver_push_scope(r)) {
            rust_resolver_add_diag(r->ctx, "RUST-E9999", line, col, "out of memory while creating scope", "try simplifying the source file");
            return RUST_SYMBOL_INVALID;
        }
    }
    exists = rust_find_current_scope(r, name);
    if (exists) {
        rust_format_ident_message(msg, 256, "duplicate binding ", name, " in the same scope");
        rust_resolver_add_diag(r->ctx, "RUST-E0001", line, col, msg, "rename this binding or remove the previous declaration");
        if (exists->line > 0 && exists->col > 0) {
            rust_resolver_add_note(r->ctx, exists->line, exists->col, "previous binding declared here");
        }
        return RUST_SYMBOL_INVALID;
    }
    scope = &r->scopes[r->scope_count - 1];
    if (scope->binding_count == scope->binding_capacity) {
        if (!rust_resolver_grow((void **)&scope->bindings, &scope->binding_capacity, (int)sizeof(RustBinding))) {
            rust_resolver_add_diag(r->ctx, "RUST-E9999", line, col, "out of memory while declaring binding", "try simplifying this scope");
            return RUST_SYMBOL_INVALID;
        }
    }
    id = rust_add_symbol(r, kind, name, line, col);
    if (id == RUST_SYMBOL_INVALID) return id;
    b = &scope->bindings[scope->binding_count++];
    b->name = name;
    b->symbol_id = id;
    b->line = line;
    b->col = col;
    return id;
}

static RustSymbolId rust_use_name(RustResolver *r, const char *name, int line, int col) {
    RustBinding *b = rust_find_visible(r, name);
    char msg[256];
    if (!b) {
        rust_format_ident_message(msg, 256, "cannot find name ", name, " in this scope");
        rust_resolver_add_diag(r->ctx, "RUST-E0002", line, col, msg, "declare the binding before using it");
        return RUST_SYMBOL_INVALID;
    }
    return b->symbol_id;
}

static void rust_resolve_expr(RustResolver *r, RustExpr *e) {
    if (!e) return;
    if (e->kind == RUST_EXPR_IDENT) {
        e->symbol_id = rust_use_name(r, e->text, e->line, e->col);
        return;
    }
    if (e->kind == RUST_EXPR_CALL) {
        int ai;
        e->call_callee_symbol_id = rust_use_name(r, e->call_callee, e->line, e->col);
        for (ai = 0; ai < e->call_arg_count; ai++) {
            rust_resolve_expr(r, e->call_args[ai]);
        }
        return;
    }
    if (e->kind == RUST_EXPR_UNARY) {
        rust_resolve_expr(r, e->lhs);
        return;
    }
    if (e->kind == RUST_EXPR_BINARY) {
        rust_resolve_expr(r, e->lhs);
        rust_resolve_expr(r, e->rhs);
        return;
    }
}

static void rust_resolve_stmt_list_in_new_scope(RustResolver *r, RustStmt *s);
static void rust_typecheck_stmt_list(RustTypecheckContext *ctx, RustStmt *s, RustTypeKind fn_ret);
static RustTypeKind rust_typecheck_expr(RustTypecheckContext *ctx, RustExpr *e);
static RustFlowKind rust_typecheck_stmt_list_flow(RustTypecheckContext *ctx, RustStmt *s);
static RustFlowKind rust_typecheck_stmt_flow(RustTypecheckContext *ctx, RustStmt *s);

static int rust_stmt_list_symbol_is_mut_local(const RustStmt *s, RustSymbolId symbol_id) {
    while (s) {
        if (s->kind == RUST_STMT_LET && s->symbol_id == symbol_id) return s->is_mut ? 1 : 0;
        if (s->kind == RUST_STMT_IF) {
            if (rust_stmt_list_symbol_is_mut_local(s->then_head, symbol_id)) return 1;
            if (rust_stmt_list_symbol_is_mut_local(s->else_head, symbol_id)) return 1;
        } else if (s->kind == RUST_STMT_WHILE) {
            if (rust_stmt_list_symbol_is_mut_local(s->body_head, symbol_id)) return 1;
        }
        s = s->next;
    }
    return 0;
}

static int rust_ast_symbol_is_mut_local(const RustAst *ast, RustSymbolId symbol_id) {
    const RustFunction *fn;
    if (!ast || symbol_id == RUST_SYMBOL_INVALID) return 0;
    fn = ast->functions;
    while (fn) {
        if (rust_stmt_list_symbol_is_mut_local(fn->body_head, symbol_id)) return 1;
        fn = fn->next;
    }
    return 0;
}

static const char *rust_type_name(RustTypeKind ty) {
    if (ty == RUST_TYPE_I32) return "i32";
    if (ty == RUST_TYPE_BOOL) return "bool";
    return "<error>";
}

static RustTypeKind rust_parse_type_kind_name(const char *name) {
    if (!name || !name[0] || strcmp(name, "i32") == 0) return RUST_TYPE_I32;
    if (strcmp(name, "bool") == 0) return RUST_TYPE_BOOL;
    return RUST_TYPE_ERROR;
}

static int rust_is_cmp_op(int op) {
    return op == RUST_TK_EQ || op == RUST_TK_NE ||
           op == RUST_TK_LT || op == RUST_TK_LE ||
           op == RUST_TK_GT || op == RUST_TK_GE;
}

static int rust_is_logic_op(int op) {
    return op == RUST_TK_LAND || op == RUST_TK_LOR;
}

static void rust_resolve_stmt(RustResolver *r, RustStmt *s) {
    if (!s) return;
    if (s->kind == RUST_STMT_LET) {
        /* Resolve initializer first so let x = x + 1 binds outer x. */
        rust_resolve_expr(r, s->expr);
        s->symbol_id = rust_declare(r, RUST_SYMBOL_LOCAL, s->name, s->name_line, s->name_col);
        return;
    }
    if (s->kind == RUST_STMT_RETURN || s->kind == RUST_STMT_EXPR) {
        rust_resolve_expr(r, s->expr);
        return;
    }
    if (s->kind == RUST_STMT_ASSIGN) {
        RustSymbolId target = rust_use_name(r, s->name, s->name_line, s->name_col);
        s->symbol_id = target;
        rust_resolve_expr(r, s->expr);
        if (target != RUST_SYMBOL_INVALID && !rust_ast_symbol_is_mut_local(r->ctx->ast, target)) {
            char msg[256];
            snprintf(msg, sizeof(msg), "cannot assign to immutable binding `%s`", s->name[0] ? s->name : "<unknown>");
            rust_resolver_add_diag(r->ctx, "RUST-E0003", s->name_line, s->name_col, msg, "declare with `let mut`, for example: `let mut x: i32 = 0;`");
        }
        return;
    }
    if (s->kind == RUST_STMT_IF) {
        rust_resolve_expr(r, s->cond);
        rust_resolve_stmt_list_in_new_scope(r, s->then_head);
        if (s->else_head) rust_resolve_stmt_list_in_new_scope(r, s->else_head);
        return;
    }
    if (s->kind == RUST_STMT_WHILE) {
        rust_resolve_expr(r, s->cond);
        rust_resolve_stmt_list_in_new_scope(r, s->body_head);
        return;
    }
}

static void rust_resolve_stmt_list_no_new_scope(RustResolver *r, RustStmt *s) {
    while (s) {
        rust_resolve_stmt(r, s);
        s = s->next;
    }
}

static void rust_resolve_stmt_list_in_new_scope(RustResolver *r, RustStmt *s) {
    if (!rust_resolver_push_scope(r)) {
        rust_resolver_add_diag(r->ctx, "RUST-E9999", 0, 0, "out of memory while entering block scope", "try simplifying nested blocks");
        rust_resolve_stmt_list_no_new_scope(r, s);
        return;
    }
    rust_resolve_stmt_list_no_new_scope(r, s);
    rust_resolver_pop_scope(r);
}

int rust_resolve_names(RustResolveContext *ctx) {
    RustResolver r;
    RustFunction *fn;
    if (!ctx || !ctx->ast) return 1;
    ctx->had_error = 0;
    memset(&r, 0, sizeof(r));
    r.ctx = ctx;
    r.next_symbol_id = 1;
    if (!rust_resolver_push_scope(&r)) return 1;

    /* Pass 1: declare top-level function names. */
    fn = ctx->ast->functions;
    while (fn) {
        fn->symbol_id = rust_declare(&r, RUST_SYMBOL_FUNCTION, fn->name, fn->name_line, fn->name_col);
        fn = fn->next;
    }

    /* Pass 2: resolve function bodies with function-local scope. */
    fn = ctx->ast->functions;
    while (fn) {
        int pi;
        if (!rust_resolver_push_scope(&r)) {
            rust_resolver_add_diag(ctx, "RUST-E9999", fn->name_line, fn->name_col, "out of memory while entering function scope", "try simplifying function body");
            rust_resolve_stmt_list_no_new_scope(&r, fn->body_head);
        } else {
            for (pi = 0; pi < fn->num_params; pi++) {
                fn->param_symbol_ids[pi] = rust_declare(&r, RUST_SYMBOL_PARAM, fn->param_names[pi], fn->param_lines[pi], fn->param_cols[pi]);
            }
            rust_resolve_stmt_list_no_new_scope(&r, fn->body_head);
            rust_resolver_pop_scope(&r);
        }
        fn = fn->next;
    }

    while (r.scope_count > 0) rust_resolver_pop_scope(&r);
    free(r.scopes);
    return ctx->had_error ? 1 : 0;
}

int rust_typecheck(RustTypecheckContext *ctx) {
    RustFunction *fn;
    if (!ctx || !ctx->ast) return 1;
    ctx->had_error = 0;
    if (!ctx->symbol_types || ctx->symbol_types_len <= 0) {
        rust_typecheck_add_diag(ctx, "RUST-TYPE-E9999", 0, 0, "typechecker missing symbol type table", "resolver/typecheck contract broken");
        return 1;
    }
    fn = ctx->ast->functions;
    while (fn) {
        int pi;
        RustTypeKind fn_ret = rust_parse_type_kind_name(fn->ret_type);
        if (ctx->strict_function_signatures && !fn->has_explicit_ret_type) {
            char msg[256];
            snprintf(msg, sizeof(msg), "function `%s` requires explicit return type in strict signature mode", fn->name[0] ? fn->name : "<anon>");
            rust_typecheck_add_diag(ctx, "RUST-TYPE-E0015", fn->name_line, fn->name_col, msg, "add `-> i32` or `-> bool`, for example: `fn name(...) -> i32 { ... }`");
        }
        if (fn->symbol_id == RUST_SYMBOL_INVALID) {
            rust_typecheck_add_diag(ctx, "RUST-TYPE-E9999", fn->name_line, fn->name_col, "unresolved symbol reached typechecker", "resolver must succeed before typecheck");
            ctx->had_error = 1;
        }
        for (pi = 0; pi < fn->num_params; pi++) {
            if (fn->param_symbol_ids[pi] == RUST_SYMBOL_INVALID) {
                rust_typecheck_add_diag(ctx, "RUST-TYPE-E9999", fn->param_lines[pi], fn->param_cols[pi], "unresolved symbol reached typechecker", "resolver must succeed before typecheck");
                ctx->had_error = 1;
            } else {
                RustTypeKind pty = rust_parse_type_kind_name(fn->param_types[pi]);
                if (pty == RUST_TYPE_ERROR) {
                    rust_typecheck_add_diag(ctx, "RUST-TYPE-E9999", fn->param_lines[pi], fn->param_cols[pi], "unsupported parameter type reached typechecker", "v1 supports only i32 and bool parameters");
                    ctx->had_error = 1;
                } else {
                    rust_type_assign_symbol(ctx, fn->param_symbol_ids[pi], pty, fn->param_lines[pi], fn->param_cols[pi]);
                }
            }
        }
        if (fn_ret == RUST_TYPE_ERROR) {
            rust_typecheck_add_diag(ctx, "RUST-TYPE-E9999", fn->name_line, fn->name_col, "unsupported function return type reached typechecker", "v1 supports only i32 and bool");
            fn_ret = RUST_TYPE_ERROR;
        }
        rust_typecheck_stmt_list(ctx, fn->body_head, fn_ret);
        if (fn_ret == RUST_TYPE_I32) {
            RustFlowKind flow = rust_typecheck_stmt_list_flow(ctx, fn->body_head);
            if (flow != RUST_FLOW_ALWAYS_RETURNS) {
                rust_type_report_missing_return(ctx, fn->name, fn->name_line, fn->name_col);
            }
        }
        (void)fn_ret;
        fn = fn->next;
    }
    return ctx->had_error ? 1 : 0;
}

static RustTypeKind rust_typecheck_expr(RustTypecheckContext *ctx, RustExpr *e) {
    if (!ctx || !e) return RUST_TYPE_ERROR;
    if (e->kind == RUST_EXPR_NUM) return RUST_TYPE_I32;
    if (e->kind == RUST_EXPR_BOOL) return RUST_TYPE_BOOL;
    if (e->kind == RUST_EXPR_IDENT) {
        return rust_type_from_symbol(ctx, e->symbol_id, e->line, e->col);
    } else if (e->kind == RUST_EXPR_UNARY) {
        RustTypeKind ut = rust_typecheck_expr(ctx, e->lhs);
        if (e->op == RUST_TK_BANG) {
            if (ut == RUST_TYPE_ERROR) return RUST_TYPE_ERROR;
            if (ut != RUST_TYPE_BOOL) {
                rust_typecheck_add_diag(ctx, "RUST-TYPE-E0009", e->line, e->col, "logical not operand must be `bool`", "use a bool expression with `!`");
                return RUST_TYPE_ERROR;
            }
            return RUST_TYPE_BOOL;
        }
        return RUST_TYPE_ERROR;
    } else if (e->kind == RUST_EXPR_CALL) {
        const RustSymbol *callee_sym;
        RustFunction *callee_fn;
        int ai;
        if (e->call_callee_symbol_id == RUST_SYMBOL_INVALID) return RUST_TYPE_ERROR;
        callee_sym = rust_type_find_symbol(ctx, e->call_callee_symbol_id);
        if (!callee_sym) {
            rust_typecheck_add_diag(ctx, "RUST-TYPE-E9999", e->line, e->col, "missing callee symbol in typechecker", "resolver/typecheck contract broken");
            return RUST_TYPE_ERROR;
        }
        if (callee_sym->kind != RUST_SYMBOL_FUNCTION) {
            char msg[256];
            rust_format_ident_message(msg, 256, "called value ", e->call_callee, " is not a function");
            rust_typecheck_add_diag(ctx, "RUST-TYPE-E0005", e->line, e->col, msg, "call expressions require a function name");
            for (ai = 0; ai < e->call_arg_count; ai++) {
                rust_typecheck_expr(ctx, e->call_args[ai]);
            }
            return RUST_TYPE_ERROR;
        }
        callee_fn = rust_find_function_by_symbol_id(ctx->ast, e->call_callee_symbol_id);
        if (!callee_fn) {
            rust_typecheck_add_diag(ctx, "RUST-TYPE-E9999", e->line, e->col, "callee function metadata missing", "resolver/typecheck contract broken");
            return RUST_TYPE_ERROR;
        }
        if (e->call_arg_count != callee_fn->num_params) {
            char msg[256];
            snprintf(msg, sizeof(msg), "function `%s` expects %d %s but found %d",
                     e->call_callee, callee_fn->num_params,
                     (callee_fn->num_params == 1) ? "argument" : "arguments",
                     e->call_arg_count);
            rust_typecheck_add_diag(ctx, "RUST-TYPE-E0006", e->line, e->col, msg, "pass the expected number of arguments");
        }
        for (ai = 0; ai < e->call_arg_count; ai++) {
            RustTypeKind aty = rust_typecheck_expr(ctx, e->call_args[ai]);
            RustTypeKind pty = (ai < callee_fn->num_params) ? rust_parse_type_kind_name(callee_fn->param_types[ai]) : RUST_TYPE_ERROR;
            if (ai < callee_fn->num_params && pty == RUST_TYPE_ERROR) {
                rust_typecheck_add_diag(ctx, "RUST-TYPE-E9999", e->line, e->col, "unsupported parameter type reached typechecker", "v1 supports only i32 and bool parameters");
            } else if (ai < callee_fn->num_params && aty != RUST_TYPE_ERROR && aty != pty) {
                char msg[256];
                snprintf(msg, sizeof(msg),
                         "argument %d to function `%s` has mismatched type: expected `%s`, found `%s`",
                         ai + 1, e->call_callee, rust_type_name(pty), rust_type_name(aty));
                rust_typecheck_add_diag(ctx, "RUST-TYPE-E0007", e->line, e->col, msg, "pass an argument with the declared parameter type");
            }
        }
        return rust_parse_type_kind_name(callee_fn->ret_type);
    } else if (e->kind == RUST_EXPR_BINARY) {
        RustTypeKind lt = rust_typecheck_expr(ctx, e->lhs);
        RustTypeKind rt = rust_typecheck_expr(ctx, e->rhs);
        if (lt == RUST_TYPE_ERROR || rt == RUST_TYPE_ERROR) return RUST_TYPE_ERROR;
        if (rust_is_logic_op(e->op)) {
            if (lt != RUST_TYPE_BOOL || rt != RUST_TYPE_BOOL) {
                char msg[256];
                snprintf(msg, sizeof(msg),
                         "logical operands must be `bool`: left is `%s`, right is `%s`",
                         rust_type_name(lt), rust_type_name(rt));
                rust_typecheck_add_diag(ctx, "RUST-TYPE-E0010", e->line, e->col, msg, "use bool expressions with logical operators");
                return RUST_TYPE_ERROR;
            }
            return RUST_TYPE_BOOL;
        } else if (rust_is_cmp_op(e->op)) {
            if (lt != RUST_TYPE_I32 || rt != RUST_TYPE_I32) {
                char msg[256];
                snprintf(msg, sizeof(msg),
                         "comparison operands must be `i32`: left is `%s`, right is `%s`",
                         rust_type_name(lt), rust_type_name(rt));
                rust_typecheck_add_diag(ctx, "RUST-TYPE-E0008", e->line, e->col, msg, "compare integer expressions");
                return RUST_TYPE_ERROR;
            }
            return RUST_TYPE_BOOL;
        } else {
            if (lt != RUST_TYPE_I32) {
                rust_type_report_expected(ctx, "RUST-TYPE-E0001", e->line, e->col, RUST_TYPE_I32, lt, "use i32 operands for arithmetic expressions");
                return RUST_TYPE_ERROR;
            }
            if (rt != RUST_TYPE_I32) {
                rust_type_report_expected(ctx, "RUST-TYPE-E0001", e->line, e->col, RUST_TYPE_I32, rt, "use i32 operands for arithmetic expressions");
                return RUST_TYPE_ERROR;
            }
            return RUST_TYPE_I32;
        }
    }
    return RUST_TYPE_ERROR;
}

static void rust_typecheck_stmt_list(RustTypecheckContext *ctx, RustStmt *s, RustTypeKind fn_ret) {
    while (s) {
        if (s->kind == RUST_STMT_LET) {
            RustTypeKind init_ty;
            RustTypeKind decl_ty;
            if (ctx->strict_let_annotations && !s->has_type_annotation) {
                char msg[256];
                snprintf(msg, sizeof(msg), "let binding `%s` requires explicit type annotation in strict mode", s->name[0] ? s->name : "<unknown>");
                rust_typecheck_add_diag(ctx, "RUST-TYPE-E0014", s->line, s->col, msg, "add `: i32` or `: bool`, for example: `let x: i32 = 1;`");
            }
            if (s->symbol_id == RUST_SYMBOL_INVALID) {
                rust_typecheck_add_diag(ctx, "RUST-TYPE-E9999", s->name_line, s->name_col, "unresolved symbol reached typechecker", "resolver must succeed before typecheck");
                ctx->had_error = 1;
            }
            init_ty = rust_typecheck_expr(ctx, s->expr);
            if (s->has_type_annotation) {
                decl_ty = rust_parse_type_kind_name(s->type_name);
                if (decl_ty == RUST_TYPE_ERROR) {
                    rust_typecheck_add_diag(ctx, "RUST-TYPE-E9999", s->line, s->col, "unsupported let annotation type reached typechecker", "v1 supports only i32 and bool");
                } else if (init_ty != RUST_TYPE_ERROR && init_ty != decl_ty) {
                    char msg[256];
                    snprintf(msg, sizeof(msg), "let initializer type mismatch: expected `%s`, found `%s`", rust_type_name(decl_ty), rust_type_name(init_ty));
                    rust_typecheck_add_diag(ctx, "RUST-TYPE-E0013", s->line, s->col, msg, "make initializer match annotation, e.g. `let flag: bool = true;`");
                }
            } else {
                decl_ty = init_ty;
            }
            if (decl_ty != RUST_TYPE_ERROR) {
                rust_type_assign_symbol(ctx, s->symbol_id, decl_ty, s->name_line, s->name_col);
            }
        } else if (s->kind == RUST_STMT_ASSIGN) {
            RustTypeKind target_ty;
            RustTypeKind expr_ty;
            if (s->symbol_id == RUST_SYMBOL_INVALID) {
                rust_typecheck_add_diag(ctx, "RUST-TYPE-E9999", s->name_line, s->name_col, "unresolved assignment target reached typechecker", "resolver must succeed before typecheck");
                ctx->had_error = 1;
                s = s->next;
                continue;
            }
            if (!rust_ast_symbol_is_mut_local(ctx->ast, s->symbol_id)) {
                char msg[256];
                snprintf(msg, sizeof(msg), "cannot assign to immutable binding `%s`", s->name[0] ? s->name : "<unknown>");
                rust_typecheck_add_diag(ctx, "RUST-TYPE-E0011", s->name_line, s->name_col, msg, "declare the binding with `let mut`");
            }
            target_ty = rust_type_from_symbol(ctx, s->symbol_id, s->name_line, s->name_col);
            expr_ty = rust_typecheck_expr(ctx, s->expr);
            if (target_ty != RUST_TYPE_ERROR && expr_ty != RUST_TYPE_ERROR && target_ty != expr_ty) {
                char msg[256];
                snprintf(msg, sizeof(msg), "assignment type mismatch: expected `%s`, found `%s`", rust_type_name(target_ty), rust_type_name(expr_ty));
                rust_typecheck_add_diag(ctx, "RUST-TYPE-E0012", s->line, s->col, msg, "assign a value with the same type as the binding");
            }
        } else if (s->kind == RUST_STMT_RETURN || s->kind == RUST_STMT_EXPR) {
            RustTypeKind rt = rust_typecheck_expr(ctx, s->expr);
            if (s->kind == RUST_STMT_RETURN && rt != RUST_TYPE_ERROR && rt != fn_ret) {
                char msg[256];
                snprintf(msg, sizeof(msg), "return type mismatch: expected `%s`, found `%s`", rust_type_name(fn_ret), rust_type_name(rt));
                rust_typecheck_add_diag(ctx, "RUST-TYPE-E0003", s->line, s->col, msg, "return an expression matching the function return type");
            }
        } else if (s->kind == RUST_STMT_IF) {
            RustTypeKind ct = rust_typecheck_expr(ctx, s->cond);
            if (ct != RUST_TYPE_ERROR && ct != RUST_TYPE_BOOL) {
                rust_type_report_expected(ctx, "RUST-TYPE-E0002", s->line, s->col, RUST_TYPE_BOOL, ct, "use a bool expression for condition");
            }
            rust_typecheck_stmt_list(ctx, s->then_head, fn_ret);
            rust_typecheck_stmt_list(ctx, s->else_head, fn_ret);
        } else if (s->kind == RUST_STMT_WHILE) {
            RustTypeKind ct = rust_typecheck_expr(ctx, s->cond);
            if (ct != RUST_TYPE_ERROR && ct != RUST_TYPE_BOOL) {
                rust_type_report_expected(ctx, "RUST-TYPE-E0002", s->line, s->col, RUST_TYPE_BOOL, ct, "use a bool expression for condition");
            }
            rust_typecheck_stmt_list(ctx, s->body_head, fn_ret);
        }
        s = s->next;
    }
}

static RustFlowKind rust_typecheck_stmt_flow(RustTypecheckContext *ctx, RustStmt *s) {
    (void)ctx;
    if (!s) return RUST_FLOW_MAY_CONTINUE;
    if (s->kind == RUST_STMT_RETURN) return RUST_FLOW_ALWAYS_RETURNS;
    if (s->kind == RUST_STMT_IF) {
        if (!s->else_head) return RUST_FLOW_MAY_CONTINUE;
        if (rust_typecheck_stmt_list_flow(ctx, s->then_head) == RUST_FLOW_ALWAYS_RETURNS &&
            rust_typecheck_stmt_list_flow(ctx, s->else_head) == RUST_FLOW_ALWAYS_RETURNS) {
            return RUST_FLOW_ALWAYS_RETURNS;
        }
        return RUST_FLOW_MAY_CONTINUE;
    }
    if (s->kind == RUST_STMT_WHILE) {
        return RUST_FLOW_MAY_CONTINUE;
    }
    return RUST_FLOW_MAY_CONTINUE;
}

static RustFlowKind rust_typecheck_stmt_list_flow(RustTypecheckContext *ctx, RustStmt *s) {
    while (s) {
        RustFlowKind fk = rust_typecheck_stmt_flow(ctx, s);
        if (fk == RUST_FLOW_ALWAYS_RETURNS) return RUST_FLOW_ALWAYS_RETURNS;
        s = s->next;
    }
    return RUST_FLOW_MAY_CONTINUE;
}

static void rust_lower_indent(int indent) {
    int i;
    for (i = 0; i < indent; i++) fputc(' ', stdout);
}

static const char *rust_lower_bin_name(int op) {
    if (op == RUST_TK_PLUS) return "add";
    if (op == RUST_TK_MINUS) return "sub";
    if (op == RUST_TK_STAR) return "mul";
    if (op == RUST_TK_SLASH) return "div";
    if (op == RUST_TK_EQ) return "eq";
    if (op == RUST_TK_NE) return "ne";
    if (op == RUST_TK_LT) return "lt";
    if (op == RUST_TK_LE) return "le";
    if (op == RUST_TK_GT) return "gt";
    if (op == RUST_TK_GE) return "ge";
    if (op == RUST_TK_LAND) return "and";
    if (op == RUST_TK_LOR) return "or";
    return 0;
}

static void rust_lower_dump_expr(RustLowerContext *ctx, RustExpr *e) {
    int ai;
    const char *op_name;
    if (!ctx || !e) {
        fputs("(error)", stdout);
        return;
    }
    if (e->kind == RUST_EXPR_NUM) {
        printf("(const %lld)", e->num);
        return;
    }
    if (e->kind == RUST_EXPR_BOOL) {
        printf("(bool %s)", e->bool_val ? "true" : "false");
        return;
    }
    if (e->kind == RUST_EXPR_IDENT) {
        fputs("(load ", stdout);
        rust_dump_name(stdout, e->text, RUST_DUMP_NAME_UNQUOTED);
        fputc(')', stdout);
        return;
    }
    if (e->kind == RUST_EXPR_UNARY) {
        if (e->op == RUST_TK_BANG) {
            fputs("(not ", stdout);
            rust_lower_dump_expr(ctx, e->lhs);
            fputc(')', stdout);
            return;
        }
        rust_lower_add_diag(ctx, e->line, e->col, "unsupported unary operator in Rust lowering", "this unary operator is not supported by the current lowering pass");
        fputs("(error)", stdout);
        return;
    }
    if (e->kind == RUST_EXPR_BINARY) {
        op_name = rust_lower_bin_name(e->op);
        if (!op_name) {
            rust_lower_add_diag(ctx, e->line, e->col, "unsupported binary operator in Rust lowering", "this binary operator is not supported by the current lowering pass");
            fputs("(error)", stdout);
            return;
        }
        printf("(%s ", op_name);
        rust_lower_dump_expr(ctx, e->lhs);
        fputc(' ', stdout);
        rust_lower_dump_expr(ctx, e->rhs);
        fputc(')', stdout);
        return;
    }
    if (e->kind == RUST_EXPR_CALL) {
        fputs("(call ", stdout);
        rust_dump_name(stdout, e->call_callee, RUST_DUMP_NAME_UNQUOTED);
        for (ai = 0; ai < e->call_arg_count; ai++) {
            fputc(' ', stdout);
            rust_lower_dump_expr(ctx, e->call_args[ai]);
        }
        fputc(')', stdout);
        return;
    }
    rust_lower_add_diag(ctx, e->line, e->col, "unsupported expression reached Rust lowering", "this expression is not supported by the current lowering pass");
    fputs("(error)", stdout);
}

static void rust_lower_dump_stmt_list(RustLowerContext *ctx, RustStmt *st, int indent) {
    while (st) {
        if (st->kind == RUST_STMT_LET) {
            if (!st->expr) {
                rust_lower_add_diag(ctx, st->line, st->col, "unsupported let without initializer in lowering", "v1 lowering expects initialized locals");
            } else {
                rust_lower_indent(indent);
                fputs("let ", stdout);
                rust_dump_name(stdout, st->name, RUST_DUMP_NAME_UNQUOTED);
                fputs(" <- ", stdout);
                rust_lower_dump_expr(ctx, st->expr);
                fputc('\n', stdout);
            }
        } else if (st->kind == RUST_STMT_ASSIGN) {
            rust_lower_indent(indent);
            fputs("set ", stdout);
            rust_dump_name(stdout, st->name, RUST_DUMP_NAME_UNQUOTED);
            fputs(" <- ", stdout);
            rust_lower_dump_expr(ctx, st->expr);
            fputc('\n', stdout);
        } else if (st->kind == RUST_STMT_RETURN) {
            if (!st->expr) {
                rust_lower_add_diag(ctx, st->line, st->col, "unsupported bare return in lowering", "return an i32 expression");
            } else {
                rust_lower_indent(indent);
                fputs("ret ", stdout);
                rust_lower_dump_expr(ctx, st->expr);
                fputc('\n', stdout);
            }
        } else if (st->kind == RUST_STMT_EXPR) {
            rust_lower_indent(indent);
            fputs("eval ", stdout);
            rust_lower_dump_expr(ctx, st->expr);
            fputc('\n', stdout);
        } else if (st->kind == RUST_STMT_IF) {
            rust_lower_indent(indent);
            fputs("if ", stdout);
            rust_lower_dump_expr(ctx, st->cond);
            fputc('\n', stdout);
            rust_lower_indent(indent);
            fputs("then\n", stdout);
            rust_lower_dump_stmt_list(ctx, st->then_head, indent + 2);
            if (st->else_head) {
                rust_lower_indent(indent);
                fputs("else\n", stdout);
                rust_lower_dump_stmt_list(ctx, st->else_head, indent + 2);
            }
            rust_lower_indent(indent);
            fputs("end\n", stdout);
        } else if (st->kind == RUST_STMT_WHILE) {
            rust_lower_indent(indent);
            fputs("while ", stdout);
            rust_lower_dump_expr(ctx, st->cond);
            fputc('\n', stdout);
            rust_lower_indent(indent);
            fputs("do\n", stdout);
            rust_lower_dump_stmt_list(ctx, st->body_head, indent + 2);
            rust_lower_indent(indent);
            fputs("end\n", stdout);
        } else if (ctx->dump_ir) {
            rust_lower_add_diag(ctx, st->line, st->col, "unsupported statement reached lowering", "typecheck should gate unsupported forms");
        }
        st = st->next;
    }
}

static int rust_lower_expr_supported(RustLowerContext *ctx, RustExpr *e) {
    int ai;
    if (!e) return 1;
    if (e->kind == RUST_EXPR_NUM || e->kind == RUST_EXPR_BOOL || e->kind == RUST_EXPR_IDENT) {
        return 1;
    }
    if (e->kind == RUST_EXPR_UNARY) {
        if (e->op != RUST_TK_BANG) {
            rust_lower_add_diag(ctx, e->line, e->col, "unsupported unary operator in Rust lowering", "this unary operator is not supported by the current lowering pass");
            return 0;
        }
        return rust_lower_expr_supported(ctx, e->lhs);
    }
    if (e->kind == RUST_EXPR_BINARY) {
        if (!rust_lower_bin_name(e->op)) {
            rust_lower_add_diag(ctx, e->line, e->col, "unsupported binary operator in Rust lowering", "this binary operator is not supported by the current lowering pass");
            return 0;
        }
        if (!rust_lower_expr_supported(ctx, e->lhs)) return 0;
        if (!rust_lower_expr_supported(ctx, e->rhs)) return 0;
        return 1;
    }
    if (e->kind == RUST_EXPR_CALL) {
        const RustSymbol *callee = 0;
        int si;
        if (e->call_callee_symbol_id == RUST_SYMBOL_INVALID) {
            rust_lower_add_diag(ctx, e->line, e->col, "unresolved call callee reached Rust lowering", "resolver/typecheck contract broken before lowering");
            return 0;
        }
        for (si = 0; si < ctx->symbol_count; si++) {
            if (ctx->symbols[si].id == e->call_callee_symbol_id) {
                callee = &ctx->symbols[si];
                break;
            }
        }
        if (!callee) {
            rust_lower_add_diag(ctx, e->line, e->col, "missing call callee symbol in Rust lowering", "resolver/typecheck contract broken before lowering");
            return 0;
        }
        if (callee->kind != RUST_SYMBOL_FUNCTION) {
            rust_lower_add_diag(ctx, e->line, e->col, "call target is not a function in Rust lowering", "only function symbols are valid call targets");
            return 0;
        }
        for (ai = 0; ai < e->call_arg_count; ai++) {
            if (!rust_lower_expr_supported(ctx, e->call_args[ai])) return 0;
        }
        return 1;
    }
    rust_lower_add_diag(ctx, e->line, e->col, "unsupported expression reached Rust lowering", "this expression is not supported by the current lowering pass");
    return 0;
}

static int rust_lower_stmt_list_supported(RustLowerContext *ctx, RustStmt *st) {
    while (st) {
        if (st->kind == RUST_STMT_LET) {
            if (!st->expr) {
                rust_lower_add_diag(ctx, st->line, st->col, "unsupported let without initializer in lowering", "v1 lowering expects initialized locals");
                return 0;
            }
            if (!rust_lower_expr_supported(ctx, st->expr)) return 0;
        } else if (st->kind == RUST_STMT_ASSIGN) {
            if (!st->expr) {
                rust_lower_add_diag(ctx, st->line, st->col, "unsupported assignment without expression in lowering", "use `name = expr;`");
                return 0;
            }
            if (!rust_lower_expr_supported(ctx, st->expr)) return 0;
        } else if (st->kind == RUST_STMT_RETURN || st->kind == RUST_STMT_EXPR) {
            if (!st->expr) {
                rust_lower_add_diag(ctx, st->line, st->col, "unsupported bare return in lowering", "return an i32 expression");
                return 0;
            }
            if (!rust_lower_expr_supported(ctx, st->expr)) return 0;
        } else if (st->kind == RUST_STMT_IF) {
            if (!rust_lower_expr_supported(ctx, st->cond)) return 0;
            if (!rust_lower_stmt_list_supported(ctx, st->then_head)) return 0;
            if (!rust_lower_stmt_list_supported(ctx, st->else_head)) return 0;
        } else if (st->kind == RUST_STMT_WHILE) {
            if (!rust_lower_expr_supported(ctx, st->cond)) return 0;
            if (!rust_lower_stmt_list_supported(ctx, st->body_head)) return 0;
        } else {
            rust_lower_add_diag(ctx, st->line, st->col, "unsupported statement reached lowering", "typecheck should gate unsupported forms");
            return 0;
        }
        st = st->next;
    }
    return 1;
}

int rust_lower_to_ir(RustLowerContext *ctx) {
    RustFunction *fn;
    if (!ctx || !ctx->ast) return 1;
    ctx->had_error = 0;
    if (ctx->dump_ir) {
        RustFunction *pre_fn = ctx->ast->functions;
        while (pre_fn) {
            if (!rust_lower_stmt_list_supported(ctx, pre_fn->body_head)) {
                return 1;
            }
            pre_fn = pre_fn->next;
        }
    }
    fn = ctx->ast->functions;
    if (ctx->dump_ir) {
        printf("RustIR v1\n");
    }
    while (fn) {
        if (ctx->dump_ir) {
            int pi;
            fputs("fn ", stdout);
            rust_dump_name(stdout, fn->name, RUST_DUMP_NAME_UNQUOTED);
            fputc('(', stdout);
            for (pi = 0; pi < fn->num_params; pi++) {
                if (pi) fputs(", ", stdout);
                rust_dump_name(stdout, fn->param_names[pi], RUST_DUMP_NAME_UNQUOTED);
                fputs(":i32", stdout);
            }
            fputs(") -> i32 {\n", stdout);
            rust_lower_dump_stmt_list(ctx, fn->body_head, 2);
            fputs("}\n", stdout);
        }
        fn = fn->next;
    }
    return ctx->had_error ? 1 : 0;
}

int rust_frontend_compile_file(const char *filename, const char *source, int source_len, int dump_ast, int dump_ast_with_symbols, int dump_symbol_table, int dump_ir, int strict_let_annotations, int strict_function_signatures) {
    RustParser p;
    RustAst *ast;
    int i;
    RustResolveContext rctx;
    RustTypecheckContext tctx;
    RustLowerContext lctx;
    memset(&p, 0, sizeof(p));
    p.filename = filename;
    p.src = source;
    p.len = source_len;
    p.line = 1;
    p.col = 1;
    ast = rust_parse_program_internal(&p);
    if (dump_ast) {
        rust_dump_ast(stdout, ast);
    }
    rust_print_diags(filename, p.diags, p.num_diags);
    if (p.num_diags > 0) return 1;
    rctx.filename = filename; rctx.ast = ast;
    rctx.diags = p.diags; rctx.num_diags = &p.num_diags; rctx.max_diags = 256;
    rctx.symbols = 0; rctx.symbol_count = 0; rctx.symbol_capacity = 0; rctx.had_error = 0;
    tctx.filename = filename; tctx.ast = ast;
    tctx.diags = p.diags; tctx.num_diags = &p.num_diags; tctx.max_diags = 256;
    tctx.symbols = 0; tctx.symbol_count = 0; tctx.had_error = 0;
    tctx.symbol_types_len = 0;
    tctx.symbol_types = 0;
    tctx.strict_let_annotations = strict_let_annotations ? 1 : 0;
    tctx.strict_function_signatures = strict_function_signatures ? 1 : 0;
    lctx.filename = filename; lctx.ast = ast;
    lctx.symbols = 0; lctx.symbol_count = 0;
    lctx.diags = p.diags; lctx.num_diags = &p.num_diags; lctx.max_diags = 256; lctx.had_error = 0; lctx.dump_ir = dump_ir;
    if (rust_resolve_names(&rctx) != 0) {
        if (dump_ast_with_symbols) {
            rust_dump_ast_with_symbols(stdout, ast);
        }
        if (dump_symbol_table) {
            rust_dump_symbol_table(&rctx);
        }
        rust_print_diags(filename, p.diags, p.num_diags);
        free(rctx.symbols);
        return 1;
    }
    tctx.symbols = rctx.symbols;
    tctx.symbol_count = rctx.symbol_count;
    lctx.symbols = rctx.symbols;
    lctx.symbol_count = rctx.symbol_count;
    tctx.symbol_types_len = rctx.symbol_count + 1;
    tctx.symbol_types = (int *)calloc((size_t)tctx.symbol_types_len, sizeof(int));
    if (!tctx.symbol_types) {
        rust_typecheck_add_diag(&tctx, "RUST-TYPE-E9999", 0, 0, "typechecker allocation failed", "try again with more memory");
        rust_print_diags(filename, p.diags, p.num_diags);
        free(rctx.symbols);
        return 1;
    }
    if (dump_ast_with_symbols) {
        rust_dump_ast_with_symbols(stdout, ast);
    }
    if (dump_symbol_table) {
        rust_dump_symbol_table(&rctx);
    }
    if (rust_typecheck(&tctx) != 0) {
        rust_print_diags(filename, p.diags, p.num_diags);
        free(tctx.symbol_types);
        free(rctx.symbols);
        return 1;
    }
    if (rust_lower_to_ir(&lctx) != 0) {
        rust_print_diags(filename, p.diags, p.num_diags);
        free(tctx.symbol_types);
        free(rctx.symbols);
        return 1;
    }
    free(tctx.symbol_types);
    free(rctx.symbols);
    return 0;
}

static int rust_backend_add_diag(RustParser *p, int line, int col, const char *msg, const char *hint) {
    RustDiag *d;
    if (!p) return 0;
    if (p->num_diags >= 256) return 0;
    d = &p->diags[p->num_diags++];
    d->kind = 0;
    strncpy(d->code, "RUST-BACKEND-E9999", 31);
    d->code[31] = 0;
    d->line = line;
    d->col = col;
    strncpy(d->message, msg, 255);
    d->message[255] = 0;
    if (hint) {
        strncpy(d->hint, hint, 255);
        d->hint[255] = 0;
    } else {
        d->hint[0] = 0;
    }
    return 1;
}

static int rust_backend_add_diag_code(RustParser *p, const char *code, int line, int col, const char *msg, const char *hint) {
    RustDiag *d;
    if (!p) return 0;
    if (p->num_diags >= 256) return 0;
    d = &p->diags[p->num_diags++];
    d->kind = 0;
    strncpy(d->code, code ? code : "RUST-BACKEND-E9999", 31);
    d->code[31] = 0;
    d->line = line;
    d->col = col;
    strncpy(d->message, msg, 255);
    d->message[255] = 0;
    if (hint) {
        strncpy(d->hint, hint, 255);
        d->hint[255] = 0;
    } else {
        d->hint[0] = 0;
    }
    return 1;
}

typedef struct RustBackendLocal {
    RustSymbolId symbol_id;
    int kind; /* 1=i32, 2=bool */
    long long value;
    int has_value;
} RustBackendLocal;

typedef struct RustBackendSlot {
    RustSymbolId symbol_id;
    int offset;
    int kind; /* 1=i32, 2=bool */
} RustBackendSlot;

typedef struct RustBackendEvalState {
    RustAst *ast;
    RustSymbolId call_stack[256];
    int call_stack_len;
} RustBackendEvalState;

static const RustFunction *rust_backend_find_main_function(const RustAst *ast) {
    const RustFunction *fn;
    if (!ast) return 0;
    fn = ast->functions;
    while (fn) {
        if (strcmp(fn->name, "main") == 0) return fn;
        fn = fn->next;
    }
    return 0;
}

static const RustFunction *rust_backend_find_function_by_symbol_in_ast(const RustAst *ast, RustSymbolId symbol_id) {
    const RustFunction *fn;
    if (!ast || symbol_id == RUST_SYMBOL_INVALID) return 0;
    fn = ast->functions;
    while (fn) {
        if (fn->symbol_id == symbol_id) return fn;
        fn = fn->next;
    }
    return 0;
}

static void rust_backend_runtime_function_label(char *buf, int buf_len, const RustFunction *fn) {
    if (!buf || buf_len <= 0 || !fn) return;
    if (strcmp(fn->name, "main") == 0) {
        snprintf(buf, (size_t)buf_len, "main");
    } else {
        snprintf(buf, (size_t)buf_len, "rust_fn_%d", (int)fn->symbol_id);
    }
}

static int rust_backend_runtime_expr_supported(const RustAst *ast, const RustExpr *e, const RustSymbolId *call_stack, int call_stack_len);
static int rust_backend_runtime_stmt_supported(const RustAst *ast, const RustStmt *st, const RustSymbolId *call_stack, int call_stack_len);
static int rust_backend_runtime_function_supported(const RustAst *ast, const RustFunction *fn, const RustSymbolId *call_stack, int call_stack_len);

static int rust_backend_runtime_expr_supported(const RustAst *ast, const RustExpr *e, const RustSymbolId *call_stack, int call_stack_len) {
    if (!e) return 0;
    if (e->kind == RUST_EXPR_NUM || e->kind == RUST_EXPR_BOOL || e->kind == RUST_EXPR_IDENT) return 1;
    if (e->kind == RUST_EXPR_CALL) {
        const RustFunction *callee;
        int ai;
        if (!ast) return 0;
        if (e->call_callee_symbol_id == RUST_SYMBOL_INVALID) return 0;
        callee = rust_backend_find_function_by_symbol_in_ast(ast, e->call_callee_symbol_id);
        if (!callee) return 0;
        if (e->call_arg_count != callee->num_params) return 0;
        for (ai = 0; ai < e->call_arg_count; ai++) {
            if (!rust_backend_runtime_expr_supported(ast, e->call_args[ai], call_stack, call_stack_len)) return 0;
        }
        return 1;
    }
    if (e->kind == RUST_EXPR_BINARY) {
        if (e->op == RUST_TK_PLUS || e->op == RUST_TK_MINUS || e->op == RUST_TK_STAR || e->op == RUST_TK_SLASH) {
            if (e->op == RUST_TK_SLASH && e->rhs && e->rhs->kind == RUST_EXPR_NUM && e->rhs->num == 0) return 0;
            return rust_backend_runtime_expr_supported(ast, e->lhs, call_stack, call_stack_len) &&
                   rust_backend_runtime_expr_supported(ast, e->rhs, call_stack, call_stack_len);
        }
        if (e->op == RUST_TK_EQ || e->op == RUST_TK_NE || e->op == RUST_TK_LT || e->op == RUST_TK_LE || e->op == RUST_TK_GT || e->op == RUST_TK_GE) {
            return rust_backend_runtime_expr_supported(ast, e->lhs, call_stack, call_stack_len) &&
                   rust_backend_runtime_expr_supported(ast, e->rhs, call_stack, call_stack_len);
        }
        if (e->op == RUST_TK_LAND || e->op == RUST_TK_LOR) {
            return rust_backend_runtime_expr_supported(ast, e->lhs, call_stack, call_stack_len) &&
                   rust_backend_runtime_expr_supported(ast, e->rhs, call_stack, call_stack_len);
        }
        return 0;
    }
    if (e->kind == RUST_EXPR_UNARY) {
        if (e->op != RUST_TK_BANG) return 0;
        return rust_backend_runtime_expr_supported(ast, e->lhs, call_stack, call_stack_len);
    }
    return 0;
}

static int rust_backend_runtime_expr_kind(const RustExpr *e) {
    if (!e) return 0;
    if (e->kind == RUST_EXPR_BOOL) return 2;
    if (e->kind == RUST_EXPR_BINARY &&
        (e->op == RUST_TK_EQ || e->op == RUST_TK_NE || e->op == RUST_TK_LT ||
         e->op == RUST_TK_LE || e->op == RUST_TK_GT || e->op == RUST_TK_GE ||
         e->op == RUST_TK_LAND || e->op == RUST_TK_LOR)) {
        return 2;
    }
    if (e->kind == RUST_EXPR_UNARY && e->op == RUST_TK_BANG) {
        return 2;
    }
    return 1;
}

static int rust_backend_runtime_stmt_supported(const RustAst *ast, const RustStmt *st, const RustSymbolId *call_stack, int call_stack_len) {
    while (st) {
        if (st->kind == RUST_STMT_LET || st->kind == RUST_STMT_RETURN || st->kind == RUST_STMT_ASSIGN) {
            if (!st->expr || !rust_backend_runtime_expr_supported(ast, st->expr, call_stack, call_stack_len)) return 0;
            if (st->kind == RUST_STMT_LET && st->symbol_id == RUST_SYMBOL_INVALID) return 0;
            if (st->kind == RUST_STMT_ASSIGN &&
                (st->symbol_id == RUST_SYMBOL_INVALID || !rust_ast_symbol_is_mut_local(ast, st->symbol_id))) return 0;
        } else if (st->kind == RUST_STMT_IF) {
            if (!st->cond || !rust_backend_runtime_expr_supported(ast, st->cond, call_stack, call_stack_len)) return 0;
            if (!rust_backend_runtime_stmt_supported(ast, st->then_head, call_stack, call_stack_len)) return 0;
            if (!rust_backend_runtime_stmt_supported(ast, st->else_head, call_stack, call_stack_len)) return 0;
        } else if (st->kind == RUST_STMT_WHILE) {
            if (!st->cond || !rust_backend_runtime_expr_supported(ast, st->cond, call_stack, call_stack_len)) return 0;
            if (!rust_backend_runtime_stmt_supported(ast, st->body_head, call_stack, call_stack_len)) return 0;
        } else {
            return 0;
        }
        st = st->next;
    }
    return 1;
}

static int rust_backend_runtime_function_supported(const RustAst *ast, const RustFunction *fn, const RustSymbolId *call_stack, int call_stack_len) {
    int pi;
    if (!ast || !fn || !call_stack || call_stack_len <= 0) return 0;
    if (!fn->body_head) return 0;
    for (pi = 0; pi < fn->num_params; pi++) {
        if (fn->param_symbol_ids[pi] == RUST_SYMBOL_INVALID) return 0;
    }
    return rust_backend_runtime_stmt_supported(ast, fn->body_head, call_stack, call_stack_len);
}

static int rust_backend_runtime_program_supported(const RustAst *ast) {
    const RustFunction *fn;
    RustSymbolId call_stack[1];
    if (!ast) return 0;
    fn = ast->functions;
    while (fn) {
        call_stack[0] = fn->symbol_id;
        if (!rust_backend_runtime_function_supported(ast, fn, call_stack, 1)) return 0;
        fn = fn->next;
    }
    return 1;
}

static int rust_backend_find_slot(const RustBackendSlot *slots, int slot_count, RustSymbolId symbol_id, int *out_offset, int *out_kind) {
    int i;
    if (!slots || !out_offset || symbol_id == RUST_SYMBOL_INVALID) return 0;
    for (i = 0; i < slot_count; i++) {
        if (slots[i].symbol_id == symbol_id) {
            *out_offset = slots[i].offset;
            if (out_kind) *out_kind = slots[i].kind;
            return 1;
        }
    }
    return 0;
}

/* Walk the same stmt shapes as rust_backend_runtime_stmt_supported / rust_backend_emit_runtime_stmt_list
 * so every runtime-supported `let` gets a stack slot (including under if/else/while bodies). */
static int rust_backend_collect_runtime_slots_from_stmt_list(
    RustParser *p,
    RustBackendSlot *slots,
    int *slot_count,
    int *next_off,
    const RustStmt *st
) {
    while (st) {
        if (st->kind == RUST_STMT_LET) {
            if (*slot_count >= 256) {
                rust_backend_add_diag(p, st->line, st->col, "backend runtime slot table overflow", "reduce parameter/local bindings for backend-v1 runtime codegen");
                return 1;
            }
            slots[*slot_count].symbol_id = st->symbol_id;
            slots[*slot_count].offset = *next_off;
            slots[*slot_count].kind = rust_backend_runtime_expr_kind(st->expr);
            (*slot_count)++;
            *next_off -= 4;
        } else if (st->kind == RUST_STMT_IF) {
            if (rust_backend_collect_runtime_slots_from_stmt_list(p, slots, slot_count, next_off, st->then_head) != 0) return 1;
            if (rust_backend_collect_runtime_slots_from_stmt_list(p, slots, slot_count, next_off, st->else_head) != 0) return 1;
        } else if (st->kind == RUST_STMT_WHILE) {
            if (rust_backend_collect_runtime_slots_from_stmt_list(p, slots, slot_count, next_off, st->body_head) != 0) return 1;
        }
        st = st->next;
    }
    return 0;
}

static int rust_backend_emit_runtime_expr(FILE *out, RustParser *p, const RustAst *ast, const RustExpr *e, const RustBackendSlot *slots, int slot_count, int *label_id) {
    int off;
    int sk;
    if (!out || !p || !e) return 1;
    if (e->kind == RUST_EXPR_NUM) {
        fprintf(out, "    movl $%lld, %%eax\n", e->num);
        return 0;
    }
    if (e->kind == RUST_EXPR_BOOL) {
        fprintf(out, "    movl $%d, %%eax\n", e->bool_val ? 1 : 0);
        return 0;
    }
    if (e->kind == RUST_EXPR_IDENT) {
        if (!rust_backend_find_slot(slots, slot_count, e->symbol_id, &off, &sk)) {
            rust_backend_add_diag(p, e->line, e->col, "unsupported Rust backend feature `runtime identifier binding`", "this Rust construct is not supported by the current backend runtime bridge");
            return 1;
        }
        (void)sk;
        fprintf(out, "    movl %d(%%rbp), %%eax\n", off);
        return 0;
    }
    if (e->kind == RUST_EXPR_CALL) {
        const RustFunction *callee;
        char label[64];
        static const char *arg_regs[6] = {"%edi", "%esi", "%edx", "%ecx", "%r8d", "%r9d"};
        int ai;
        int n;
        int n_reg;
        int n_stack;
        int align_pad;
        int room;
        int si;
        int cleanup;
        if (!ast || e->call_callee_symbol_id == RUST_SYMBOL_INVALID) {
            rust_backend_add_diag(p, e->line, e->col, "unsupported Rust backend feature `runtime call expression`", "this Rust construct is not supported by the current backend runtime bridge");
            return 1;
        }
        callee = rust_backend_find_function_by_symbol_in_ast(ast, e->call_callee_symbol_id);
        if (!callee) {
            rust_backend_add_diag(p, e->line, e->col, "unsupported Rust backend feature `runtime call callee`", "resolver/typecheck contract broken before backend runtime bridge");
            return 1;
        }
        if (e->call_arg_count != callee->num_params) {
            rust_backend_add_diag(p, e->line, e->col, "unsupported Rust backend feature `runtime call arguments`", "call argument count does not match callee parameter count");
            return 1;
        }
        n = e->call_arg_count;
        n_reg = n < 6 ? n : 6;
        n_stack = n > 6 ? n - 6 : 0;
        /* Register args (first six) are evaluated right-to-left (push on stack), then
         * popped into %edi..%r9d. A left-to-right mov would clobber nested call args. */
        align_pad = (n_stack & 1) ? 8 : 0;
        /* SysV: the first stack param must live at 0(%rsp) at the *call* site so that after
         * `call` (ret at 0) it is at 8(%rsp) for the callee. Do not `push` stack params then
         * add reg-arg pushes (that would bury the 7th under temps). Reg args first, then
         * subq a contiguous outgoing-arg area, movl each i32 to 0, 8, ... 8(n_stack-1)(%rsp). */
        room = 8 * n_stack + align_pad;
        for (ai = n_reg - 1; ai >= 0; ai--) {
            if (rust_backend_emit_runtime_expr(out, p, ast, e->call_args[ai], slots, slot_count, label_id) != 0) return 1;
            fprintf(out, "    pushq %%rax\n");
        }
        for (ai = 0; ai < n_reg; ai++) {
            fprintf(out, "    popq %%rax\n");
            fprintf(out, "    movl %%eax, %s\n", arg_regs[ai]);
        }
        if (room > 0) {
            fprintf(out, "    subq $%d, %%rsp\n", room);
            for (ai = 6; ai < n; ai++) {
                if (rust_backend_emit_runtime_expr(out, p, ast, e->call_args[ai], slots, slot_count, label_id) != 0) return 1;
                si = 8 * (ai - 6);
                fprintf(out, "    movl %%eax, %d(%%rsp)\n", si);
            }
        }
        rust_backend_runtime_function_label(label, (int)sizeof(label), callee);
        fprintf(out, "    call %s\n", label);
        cleanup = room;
        if (cleanup > 0) {
            fprintf(out, "    addq $%d, %%rsp\n", cleanup);
        }
        return 0;
    }
    if (e->kind == RUST_EXPR_UNARY) {
        if (e->op != RUST_TK_BANG) {
            rust_backend_add_diag(p, e->line, e->col, "unsupported Rust backend feature `runtime unary operator`", "this Rust construct is not supported by the current backend runtime bridge");
            return 1;
        }
        if (rust_backend_emit_runtime_expr(out, p, ast, e->lhs, slots, slot_count, label_id) != 0) return 1;
        fprintf(out, "    cmpl $0, %%eax\n");
        fprintf(out, "    sete %%al\n");
        fprintf(out, "    movzbl %%al, %%eax\n");
        return 0;
    }
    if (e->kind == RUST_EXPR_BINARY) {
        if (e->op == RUST_TK_LAND) {
            int id = (*label_id)++;
            if (rust_backend_emit_runtime_expr(out, p, ast, e->lhs, slots, slot_count, label_id) != 0) return 1;
            fprintf(out, "    cmpl $0, %%eax\n");
            fprintf(out, "    je .Lrust_logic_false_%d\n", id);
            if (rust_backend_emit_runtime_expr(out, p, ast, e->rhs, slots, slot_count, label_id) != 0) return 1;
            fprintf(out, "    cmpl $0, %%eax\n");
            fprintf(out, "    je .Lrust_logic_false_%d\n", id);
            fprintf(out, "    movl $1, %%eax\n");
            fprintf(out, "    jmp .Lrust_logic_end_%d\n", id);
            fprintf(out, ".Lrust_logic_false_%d:\n", id);
            fprintf(out, "    movl $0, %%eax\n");
            fprintf(out, ".Lrust_logic_end_%d:\n", id);
            return 0;
        }
        if (e->op == RUST_TK_LOR) {
            int id = (*label_id)++;
            if (rust_backend_emit_runtime_expr(out, p, ast, e->lhs, slots, slot_count, label_id) != 0) return 1;
            fprintf(out, "    cmpl $0, %%eax\n");
            fprintf(out, "    jne .Lrust_logic_true_%d\n", id);
            if (rust_backend_emit_runtime_expr(out, p, ast, e->rhs, slots, slot_count, label_id) != 0) return 1;
            fprintf(out, "    cmpl $0, %%eax\n");
            fprintf(out, "    jne .Lrust_logic_true_%d\n", id);
            fprintf(out, "    movl $0, %%eax\n");
            fprintf(out, "    jmp .Lrust_logic_end_%d\n", id);
            fprintf(out, ".Lrust_logic_true_%d:\n", id);
            fprintf(out, "    movl $1, %%eax\n");
            fprintf(out, ".Lrust_logic_end_%d:\n", id);
            return 0;
        }
        if (rust_backend_emit_runtime_expr(out, p, ast, e->lhs, slots, slot_count, label_id) != 0) return 1;
        fprintf(out, "    pushq %%rax\n");
        if (rust_backend_emit_runtime_expr(out, p, ast, e->rhs, slots, slot_count, label_id) != 0) return 1;
        fprintf(out, "    movl %%eax, %%ecx\n");
        fprintf(out, "    popq %%rax\n");
        if (e->op == RUST_TK_PLUS) fprintf(out, "    addl %%ecx, %%eax\n");
        else if (e->op == RUST_TK_MINUS) fprintf(out, "    subl %%ecx, %%eax\n");
        else if (e->op == RUST_TK_STAR) fprintf(out, "    imull %%ecx, %%eax\n");
        else if (e->op == RUST_TK_SLASH) {
            fprintf(out, "    cltd\n");
            fprintf(out, "    idivl %%ecx\n");
        } else if (e->op == RUST_TK_EQ || e->op == RUST_TK_NE || e->op == RUST_TK_LT || e->op == RUST_TK_LE || e->op == RUST_TK_GT || e->op == RUST_TK_GE) {
            fprintf(out, "    cmpl %%ecx, %%eax\n");
            if (e->op == RUST_TK_EQ) fprintf(out, "    sete %%al\n");
            else if (e->op == RUST_TK_NE) fprintf(out, "    setne %%al\n");
            else if (e->op == RUST_TK_LT) fprintf(out, "    setl %%al\n");
            else if (e->op == RUST_TK_LE) fprintf(out, "    setle %%al\n");
            else if (e->op == RUST_TK_GT) fprintf(out, "    setg %%al\n");
            else fprintf(out, "    setge %%al\n");
            fprintf(out, "    movzbl %%al, %%eax\n");
        } else {
            rust_backend_add_diag(p, e->line, e->col, "unsupported Rust backend feature `runtime binary operator`", "this Rust construct is not supported by the current backend runtime bridge");
            return 1;
        }
        return 0;
    }
    rust_backend_add_diag(p, e->line, e->col, "unsupported Rust backend feature `runtime expression`", "this Rust construct is not supported by the current backend runtime bridge");
    return 1;
}

static int rust_backend_emit_runtime_stmt_list(FILE *out, RustParser *p, const RustAst *ast, const RustStmt *st, RustBackendSlot *slots, int slot_count, int *label_id) {
    while (st) {
        if (st->kind == RUST_STMT_LET) {
            int off;
            if (!rust_backend_find_slot(slots, slot_count, st->symbol_id, &off, 0)) {
                rust_backend_add_diag(p, st->line, st->col, "unsupported Rust backend feature `runtime let slot binding`", "this Rust construct is not supported by the current backend runtime bridge");
                return 1;
            }
            if (rust_backend_emit_runtime_expr(out, p, ast, st->expr, slots, slot_count, label_id) != 0) return 1;
            fprintf(out, "    movl %%eax, %d(%%rbp)\n", off);
        } else if (st->kind == RUST_STMT_ASSIGN) {
            int off;
            if (st->symbol_id == RUST_SYMBOL_INVALID || !rust_ast_symbol_is_mut_local(ast, st->symbol_id)) {
                rust_backend_add_diag(p, st->line, st->col, "unsupported Rust backend feature `runtime assignment target`", "assignment requires an existing mutable local");
                return 1;
            }
            if (!rust_backend_find_slot(slots, slot_count, st->symbol_id, &off, 0)) {
                rust_backend_add_diag(p, st->line, st->col, "unsupported Rust backend feature `runtime assignment slot binding`", "this Rust construct is not supported by the current backend runtime bridge");
                return 1;
            }
            if (rust_backend_emit_runtime_expr(out, p, ast, st->expr, slots, slot_count, label_id) != 0) return 1;
            fprintf(out, "    movl %%eax, %d(%%rbp)\n", off);
        } else if (st->kind == RUST_STMT_RETURN) {
            if (rust_backend_emit_runtime_expr(out, p, ast, st->expr, slots, slot_count, label_id) != 0) return 1;
            fprintf(out, "    leave\n");
            fprintf(out, "    ret\n");
        } else if (st->kind == RUST_STMT_IF) {
            int id = (*label_id)++;
            if (rust_backend_emit_runtime_expr(out, p, ast, st->cond, slots, slot_count, label_id) != 0) return 1;
            fprintf(out, "    cmpl $0, %%eax\n");
            fprintf(out, "    je .Lrust_else_%d\n", id);
            if (rust_backend_emit_runtime_stmt_list(out, p, ast, st->then_head, slots, slot_count, label_id) != 0) return 1;
            fprintf(out, "    jmp .Lrust_end_if_%d\n", id);
            fprintf(out, ".Lrust_else_%d:\n", id);
            if (rust_backend_emit_runtime_stmt_list(out, p, ast, st->else_head, slots, slot_count, label_id) != 0) return 1;
            fprintf(out, ".Lrust_end_if_%d:\n", id);
        } else if (st->kind == RUST_STMT_WHILE) {
            int id = (*label_id)++;
            fprintf(out, ".Lrust_while_start_%d:\n", id);
            if (rust_backend_emit_runtime_expr(out, p, ast, st->cond, slots, slot_count, label_id) != 0) return 1;
            fprintf(out, "    cmpl $0, %%eax\n");
            fprintf(out, "    je .Lrust_while_end_%d\n", id);
            if (rust_backend_emit_runtime_stmt_list(out, p, ast, st->body_head, slots, slot_count, label_id) != 0) return 1;
            fprintf(out, "    jmp .Lrust_while_start_%d\n", id);
            fprintf(out, ".Lrust_while_end_%d:\n", id);
        } else {
            rust_backend_add_diag(p, st->line, st->col, "unsupported Rust backend feature `runtime statement`", "this Rust construct is not supported by the current backend runtime bridge");
            return 1;
        }
        st = st->next;
    }
    return 0;
}

static int rust_backend_emit_runtime_function(FILE *out, RustParser *p, const RustAst *ast, const RustFunction *fn, int *label_id) {
    RustBackendSlot slots[256];
    int slot_count = 0;
    int next_off = -4;
    int pi;
    int stack_bytes;
    char fn_label[64];
    static const char *arg_regs[6] = {"%edi", "%esi", "%edx", "%ecx", "%r8d", "%r9d"};
    if (!out || !p || !fn) return 1;
    for (pi = 0; pi < fn->num_params; pi++) {
        if (slot_count >= 256) {
            rust_backend_add_diag(p, fn->name_line, fn->name_col, "backend runtime slot table overflow", "reduce parameter/local bindings for backend-v1 runtime codegen");
            return 1;
        }
        slots[slot_count].symbol_id = fn->param_symbol_ids[pi];
        slots[slot_count].offset = next_off;
        slots[slot_count].kind = 1;
        slot_count++;
        next_off -= 4;
    }
    if (rust_backend_collect_runtime_slots_from_stmt_list(p, slots, &slot_count, &next_off, fn->body_head) != 0) return 1;
    stack_bytes = slot_count * 4;
    if (stack_bytes <= 0) stack_bytes = 16;
    else {
        int rem = stack_bytes % 16;
        if (rem != 0) stack_bytes += 16 - rem;
    }
    rust_backend_runtime_function_label(fn_label, (int)sizeof(fn_label), fn);
    fprintf(out, "%s:\n", fn_label);
    fprintf(out, "    pushq %%rbp\n");
    fprintf(out, "    movq %%rsp, %%rbp\n");
    fprintf(out, "    subq $%d, %%rsp\n", stack_bytes);
    for (pi = 0; pi < fn->num_params; pi++) {
        if (pi < 6) {
            fprintf(out, "    movl %s, %d(%%rbp)\n", arg_regs[pi], slots[pi].offset);
        } else {
            int incoming_off = 16 + 8 * (pi - 6);
            fprintf(out, "    movl %d(%%rbp), %%eax\n", incoming_off);
            fprintf(out, "    movl %%eax, %d(%%rbp)\n", slots[pi].offset);
        }
    }
    if (rust_backend_emit_runtime_stmt_list(out, p, ast, fn->body_head, slots, slot_count, label_id) != 0) return 1;
    fprintf(out, "    movl $0, %%eax\n");
    fprintf(out, "    leave\n");
    fprintf(out, "    ret\n");
    return 0;
}

static int rust_backend_emit_runtime_program(FILE *out, RustParser *p, const RustAst *ast) {
    const RustFunction *fn;
    int label_id = 0;
    if (!out || !p || !ast) return 1;
    fprintf(out, ".text\n");
    fprintf(out, ".globl main\n");
    fn = ast->functions;
    while (fn) {
        if (rust_backend_emit_runtime_function(out, p, ast, fn, &label_id) != 0) return 1;
        fn = fn->next;
    }
    return 0;
}

static int rust_backend_lookup_local(const RustBackendLocal *locals, int local_count, RustSymbolId symbol_id, long long *out_value) {
    int i;
    if (!locals || symbol_id == RUST_SYMBOL_INVALID || !out_value) return 0;
    for (i = 0; i < local_count; i++) {
        if (locals[i].symbol_id == symbol_id && locals[i].has_value) {
            *out_value = locals[i].value;
            return 1;
        }
    }
    return 0;
}

static int rust_backend_lookup_local_typed(const RustBackendLocal *locals, int local_count, RustSymbolId symbol_id, int expected_kind, long long *out_value) {
    int i;
    if (!locals || symbol_id == RUST_SYMBOL_INVALID || !out_value) return 0;
    for (i = 0; i < local_count; i++) {
        if (locals[i].symbol_id == symbol_id && locals[i].has_value && locals[i].kind == expected_kind) {
            *out_value = locals[i].value;
            return 1;
        }
    }
    return 0;
}

static int rust_backend_eval_const_i32_expr(RustParser *p, RustBackendEvalState *state, RustExpr *e, const RustBackendLocal *locals, int local_count, long long *out_value);
static int rust_backend_eval_const_bool_expr(RustParser *p, RustBackendEvalState *state, RustExpr *e, const RustBackendLocal *locals, int local_count, int *out_value);
static int rust_backend_eval_const_function_i32(
    RustParser *p,
    RustBackendEvalState *state,
    RustSymbolId function_symbol_id,
    const RustBackendLocal *args,
    int arg_count,
    int line,
    int col,
    long long *out_value
);

static const RustFunction *rust_backend_find_function_by_symbol_id(const RustBackendEvalState *state, RustSymbolId symbol_id) {
    const RustFunction *fn;
    if (!state || !state->ast || symbol_id == RUST_SYMBOL_INVALID) return 0;
    fn = state->ast->functions;
    while (fn) {
        if (fn->symbol_id == symbol_id) return fn;
        fn = fn->next;
    }
    return 0;
}

static int rust_backend_call_stack_contains(const RustBackendEvalState *state, RustSymbolId symbol_id) {
    int i;
    if (!state) return 0;
    for (i = 0; i < state->call_stack_len; i++) {
        if (state->call_stack[i] == symbol_id) return 1;
    }
    return 0;
}

static int rust_backend_eval_const_i32_expr(RustParser *p, RustBackendEvalState *state, RustExpr *e, const RustBackendLocal *locals, int local_count, long long *out_value) {
    long long lhs, rhs;
    if (!p || !e || !out_value) return 1;
    if (e->kind == RUST_EXPR_NUM) {
        *out_value = e->num;
        return 0;
    }
    if (e->kind == RUST_EXPR_IDENT) {
        if (!rust_backend_lookup_local_typed(locals, local_count, e->symbol_id, 1, out_value)) {
            rust_backend_add_diag(p, e->line, e->col, "unsupported Rust backend feature `identifier expression`", "this Rust construct is not supported by the current backend bridge");
            return 1;
        }
        return 0;
    }
    if (e->kind == RUST_EXPR_CALL) {
        RustBackendLocal arg_locals[256];
        int ai;
        if (e->call_callee_symbol_id == RUST_SYMBOL_INVALID) {
            rust_backend_add_diag(p, e->line, e->col, "unsupported Rust backend feature `unresolved call callee`", "resolver/typecheck contract broken before backend");
            return 1;
        }
        if (e->call_arg_count > 256) {
            rust_backend_add_diag(p, e->line, e->col, "unsupported Rust backend feature `too many call arguments`", "reduce argument count for backend-v1 constant calls");
            return 1;
        }
        for (ai = 0; ai < e->call_arg_count; ai++) {
            long long av = 0;
            if (rust_backend_eval_const_i32_expr(p, state, e->call_args[ai], locals, local_count, &av) != 0) return 1;
            arg_locals[ai].symbol_id = RUST_SYMBOL_INVALID;
            arg_locals[ai].kind = 1;
            arg_locals[ai].value = av;
            arg_locals[ai].has_value = 1;
        }
        return rust_backend_eval_const_function_i32(
            p, state, e->call_callee_symbol_id, arg_locals, e->call_arg_count, e->line, e->col, out_value
        );
    }
    if (e->kind != RUST_EXPR_BINARY) {
        rust_backend_add_diag(p, e->line, e->col, "unsupported Rust backend feature `non-constant expression`", "this Rust construct is not supported by the current backend bridge");
        return 1;
    }
    if (e->op != RUST_TK_PLUS && e->op != RUST_TK_MINUS && e->op != RUST_TK_STAR && e->op != RUST_TK_SLASH) {
        rust_backend_add_diag(p, e->line, e->col, "unsupported Rust backend feature `non-arithmetic constant expression`", "backend v1 supports only +, -, *, / in constant return expressions");
        return 1;
    }
    if (rust_backend_eval_const_i32_expr(p, state, e->lhs, locals, local_count, &lhs) != 0) return 1;
    if (rust_backend_eval_const_i32_expr(p, state, e->rhs, locals, local_count, &rhs) != 0) return 1;
    if (e->op == RUST_TK_PLUS) *out_value = lhs + rhs;
    else if (e->op == RUST_TK_MINUS) *out_value = lhs - rhs;
    else if (e->op == RUST_TK_STAR) *out_value = lhs * rhs;
    else {
        if (rhs == 0) {
            rust_backend_add_diag_code(p, "RUST-BACKEND-E0001", e->line, e->col, "division by zero in constant expression", "avoid dividing by zero in constant expressions");
            return 1;
        }
        *out_value = lhs / rhs;
    }
    return 0;
}

static int rust_backend_eval_const_bool_expr(RustParser *p, RustBackendEvalState *state, RustExpr *e, const RustBackendLocal *locals, int local_count, int *out_value) {
    long long lhs, rhs;
    int lb, rb;
    if (!p || !e || !out_value) return 1;
    if (e->kind == RUST_EXPR_BOOL) {
        *out_value = e->bool_val ? 1 : 0;
        return 0;
    }
    if (e->kind == RUST_EXPR_IDENT) {
        if (!rust_backend_lookup_local_typed(locals, local_count, e->symbol_id, 2, &lhs)) {
            rust_backend_add_diag(p, e->line, e->col, "unsupported Rust backend feature `identifier bool expression`", "this Rust construct is not supported by the current backend bridge");
            return 1;
        }
        *out_value = lhs ? 1 : 0;
        return 0;
    }
    if (e->kind == RUST_EXPR_UNARY && e->op == RUST_TK_BANG) {
        if (rust_backend_eval_const_bool_expr(p, state, e->lhs, locals, local_count, &lb) != 0) return 1;
        *out_value = lb ? 0 : 1;
        return 0;
    }
    if (e->kind != RUST_EXPR_BINARY) {
        rust_backend_add_diag(p, e->line, e->col, "unsupported Rust backend feature `non-constant bool expression`", "this Rust construct is not supported by the current backend bridge");
        return 1;
    }
    if (e->op == RUST_TK_LAND || e->op == RUST_TK_LOR) {
        if (rust_backend_eval_const_bool_expr(p, state, e->lhs, locals, local_count, &lb) != 0) return 1;
        if (rust_backend_eval_const_bool_expr(p, state, e->rhs, locals, local_count, &rb) != 0) return 1;
        *out_value = (e->op == RUST_TK_LAND) ? ((lb && rb) ? 1 : 0) : ((lb || rb) ? 1 : 0);
        return 0;
    }
    if (e->op == RUST_TK_EQ || e->op == RUST_TK_NE || e->op == RUST_TK_LT || e->op == RUST_TK_LE || e->op == RUST_TK_GT || e->op == RUST_TK_GE) {
        if (rust_backend_eval_const_i32_expr(p, state, e->lhs, locals, local_count, &lhs) != 0) return 1;
        if (rust_backend_eval_const_i32_expr(p, state, e->rhs, locals, local_count, &rhs) != 0) return 1;
        if (e->op == RUST_TK_EQ) *out_value = (lhs == rhs) ? 1 : 0;
        else if (e->op == RUST_TK_NE) *out_value = (lhs != rhs) ? 1 : 0;
        else if (e->op == RUST_TK_LT) *out_value = (lhs < rhs) ? 1 : 0;
        else if (e->op == RUST_TK_LE) *out_value = (lhs <= rhs) ? 1 : 0;
        else if (e->op == RUST_TK_GT) *out_value = (lhs > rhs) ? 1 : 0;
        else *out_value = (lhs >= rhs) ? 1 : 0;
        return 0;
    }
    rust_backend_add_diag(p, e->line, e->col, "unsupported Rust backend feature `bool binary operator`", "this Rust construct is not supported by the current backend bridge");
    return 1;
}

static int rust_backend_eval_stmt_list(
    RustParser *p,
    RustBackendEvalState *state,
    RustStmt *st,
    RustBackendLocal *locals,
    int *local_count,
    long long *ret_value,
    int *did_return
) {
    while (st) {
        if (st->kind == RUST_STMT_LET) {
            long long v = 0;
            int b = 0;
            int i;
            int kind = 0;
            if (!st->expr) {
                rust_backend_add_diag(p, st->line, st->col, "unsupported Rust backend feature `let without initializer`", "backend v1 requires initialized local bindings");
                return 1;
            }
            if (st->symbol_id == RUST_SYMBOL_INVALID) {
                rust_backend_add_diag(p, st->line, st->col, "backend local symbol missing", "resolver/typecheck contract broken before backend");
                return 1;
            }
            if (st->expr->kind == RUST_EXPR_BOOL ||
                (st->expr->kind == RUST_EXPR_UNARY && st->expr->op == RUST_TK_BANG) ||
                (st->expr->kind == RUST_EXPR_BINARY &&
                 (st->expr->op == RUST_TK_EQ || st->expr->op == RUST_TK_NE ||
                  st->expr->op == RUST_TK_LT || st->expr->op == RUST_TK_LE ||
                  st->expr->op == RUST_TK_GT || st->expr->op == RUST_TK_GE ||
                  st->expr->op == RUST_TK_LAND || st->expr->op == RUST_TK_LOR))) {
                if (rust_backend_eval_const_bool_expr(p, state, st->expr, locals, *local_count, &b) != 0) return 1;
                v = b ? 1 : 0;
                kind = 2;
            } else {
                if (rust_backend_eval_const_i32_expr(p, state, st->expr, locals, *local_count, &v) != 0) return 1;
                kind = 1;
            }
            for (i = 0; i < *local_count; i++) {
                if (locals[i].symbol_id == st->symbol_id) {
                    locals[i].value = v;
                    locals[i].kind = kind;
                    locals[i].has_value = 1;
                    break;
                }
            }
            if (i == *local_count) {
                if (*local_count >= 256) {
                    rust_backend_add_diag(p, st->line, st->col, "backend local table overflow", "reduce local bindings for backend v1");
                    return 1;
                }
                locals[*local_count].symbol_id = st->symbol_id;
                locals[*local_count].value = v;
                locals[*local_count].kind = kind;
                locals[*local_count].has_value = 1;
                (*local_count)++;
            }
        } else if (st->kind == RUST_STMT_ASSIGN) {
            int i;
            int kind = 1;
            long long v = 0;
            int b = 0;
            if (st->symbol_id == RUST_SYMBOL_INVALID || !rust_ast_symbol_is_mut_local(state->ast, st->symbol_id)) {
                rust_backend_add_diag(p, st->line, st->col, "unsupported Rust backend feature `assignment statement`", "constant backend requires assignment to a resolved mutable local");
                return 1;
            }
            if (st->expr->kind == RUST_EXPR_BOOL ||
                (st->expr->kind == RUST_EXPR_UNARY && st->expr->op == RUST_TK_BANG) ||
                (st->expr->kind == RUST_EXPR_BINARY &&
                 (st->expr->op == RUST_TK_EQ || st->expr->op == RUST_TK_NE ||
                  st->expr->op == RUST_TK_LT || st->expr->op == RUST_TK_LE ||
                  st->expr->op == RUST_TK_GT || st->expr->op == RUST_TK_GE ||
                  st->expr->op == RUST_TK_LAND || st->expr->op == RUST_TK_LOR))) {
                if (rust_backend_eval_const_bool_expr(p, state, st->expr, locals, *local_count, &b) != 0) return 1;
                v = b ? 1 : 0;
                kind = 2;
            } else {
                if (rust_backend_eval_const_i32_expr(p, state, st->expr, locals, *local_count, &v) != 0) return 1;
                kind = 1;
            }
            for (i = 0; i < *local_count; i++) {
                if (locals[i].symbol_id == st->symbol_id) {
                    locals[i].value = v;
                    locals[i].kind = kind;
                    locals[i].has_value = 1;
                    break;
                }
            }
            if (i == *local_count) {
                rust_backend_add_diag(p, st->line, st->col, "unsupported Rust backend feature `assignment to unknown local`", "assignment target must already be bound");
                return 1;
            }
        } else if (st->kind == RUST_STMT_RETURN) {
            if (!st->expr) {
                rust_backend_add_diag(p, st->line, st->col, "unsupported Rust backend feature `bare return`", "backend v1 requires `return <constant-i32-expression>;`");
                return 1;
            }
            if (rust_backend_eval_const_i32_expr(p, state, st->expr, locals, *local_count, ret_value) != 0) return 1;
            *did_return = 1;
            if (st->next) {
                rust_backend_add_diag(p, st->line, st->col, "unsupported Rust backend feature `statements after return`", "backend v1 requires return as final statement in reachable path");
                return 1;
            }
            return 0;
        } else if (st->kind == RUST_STMT_IF) {
            int cond = 0;
            if (rust_backend_eval_const_bool_expr(p, state, st->cond, locals, *local_count, &cond) != 0) return 1;
            if (cond) {
                if (rust_backend_eval_stmt_list(p, state, st->then_head, locals, local_count, ret_value, did_return) != 0) return 1;
            } else {
                if (rust_backend_eval_stmt_list(p, state, st->else_head, locals, local_count, ret_value, did_return) != 0) return 1;
            }
            if (!*did_return) {
                rust_backend_add_diag(p, st->line, st->col, "unsupported Rust backend feature `if path without return`", "backend v1 requires a constant path that returns i32");
                return 1;
            }
            if (st->next) {
                rust_backend_add_diag(p, st->line, st->col, "unsupported Rust backend feature `statements after resolved if-return`", "backend v1 requires return as final statement in reachable path");
                return 1;
            }
            return 0;
        } else if (st->kind == RUST_STMT_WHILE) {
            rust_backend_add_diag(p, st->line, st->col, "unsupported Rust backend feature `while statement`", "this Rust construct is not supported by the current backend bridge");
            return 1;
        } else if (st->kind == RUST_STMT_EXPR) {
            rust_backend_add_diag(p, st->line, st->col, "unsupported Rust backend feature `expression statement`", "this Rust construct is not supported by the current backend bridge");
            return 1;
        } else {
            rust_backend_add_diag(p, st->line, st->col, "unsupported Rust backend feature `statement kind`", "this Rust construct is not supported by the current backend bridge");
            return 1;
        }
        st = st->next;
    }
    return 0;
}

static int rust_backend_eval_const_function_i32(
    RustParser *p,
    RustBackendEvalState *state,
    RustSymbolId function_symbol_id,
    const RustBackendLocal *args,
    int arg_count,
    int line,
    int col,
    long long *out_value
) {
    const RustFunction *fn;
    RustBackendLocal locals[256];
    int local_count = 0;
    int did_return = 0;
    int saved_depth;
    int pi;
    if (!p || !state || !out_value) return 1;
    if (rust_backend_call_stack_contains(state, function_symbol_id)) {
        rust_backend_add_diag_code(p, "RUST-BACKEND-E0002", line, col, "recursive constant function call is not supported", "backend-v1 can only evaluate non-recursive constant function calls");
        return 1;
    }
    fn = rust_backend_find_function_by_symbol_id(state, function_symbol_id);
    if (!fn) {
        rust_backend_add_diag(p, line, col, "unsupported Rust backend feature `missing called function`", "resolver/typecheck contract broken before backend");
        return 1;
    }
    if (fn->num_params != arg_count) {
        rust_backend_add_diag(p, line, col, "unsupported Rust backend feature `call arity mismatch in backend-v1`", "resolver/typecheck contract broken before backend");
        return 1;
    }
    for (pi = 0; pi < fn->num_params; pi++) {
        if (fn->param_symbol_ids[pi] == RUST_SYMBOL_INVALID) {
            rust_backend_add_diag(p, line, col, "unsupported Rust backend feature `function parameter binding`", "resolver/typecheck contract broken before backend");
            return 1;
        }
        if (!args || !args[pi].has_value || args[pi].kind != 1) {
            rust_backend_add_diag(p, line, col, "unsupported Rust backend feature `non-constant call argument`", "backend-v1 currently binds only constant i32 call arguments");
            return 1;
        }
        if (local_count >= 256) {
            rust_backend_add_diag(p, line, col, "backend local table overflow", "reduce local/parameter bindings for backend v1");
            return 1;
        }
        locals[local_count].symbol_id = fn->param_symbol_ids[pi];
        locals[local_count].kind = 1;
        locals[local_count].value = args[pi].value;
        locals[local_count].has_value = 1;
        local_count++;
    }
    if (state->call_stack_len >= 256) {
        rust_backend_add_diag(p, line, col, "backend call stack overflow", "reduce backend-v1 constant call depth");
        return 1;
    }
    saved_depth = state->call_stack_len;
    state->call_stack[state->call_stack_len++] = function_symbol_id;
    if (rust_backend_eval_stmt_list(p, state, fn->body_head, locals, &local_count, out_value, &did_return) != 0) {
        state->call_stack_len = saved_depth;
        return 1;
    }
    state->call_stack_len = saved_depth;
    if (!did_return) {
        rust_backend_add_diag(p, line, col, "unsupported Rust backend feature `called function without return`", "backend-v1 requires called functions to constant-evaluate to i32");
        return 1;
    }
    return 0;
}

static int rust_backend_extract_const_main(RustParser *p, RustAst *ast, long long *ret_value) {
    RustFunction *fn;
    RustBackendLocal locals[256];
    int local_count = 0;
    int did_return = 0;
    RustBackendEvalState state;
    if (!p || !ast || !ret_value) return 1;
    fn = ast->functions;
    while (fn && strcmp(fn->name, "main") != 0) {
        fn = fn->next;
    }
    if (!fn) {
        rust_backend_add_diag(p, 1, 1, "unsupported Rust backend feature `missing main`", "backend v1 requires an entrypoint `fn main() -> i32`");
        return 1;
    }
    if (fn->num_params != 0) {
        rust_backend_add_diag(p, fn->name_line, fn->name_col, "unsupported Rust backend feature `main parameters`", "backend v1 requires `main` to take no parameters");
        return 1;
    }
    if (!fn->body_head) {
        rust_backend_add_diag(p, fn->name_line, fn->name_col, "unsupported Rust backend feature `empty main body`", "backend v1 requires let-bindings followed by a final return");
        return 1;
    }
    memset(&state, 0, sizeof(state));
    state.ast = ast;
    state.call_stack_len = 0;
    if (rust_backend_eval_stmt_list(p, &state, fn->body_head, locals, &local_count, ret_value, &did_return) != 0) {
        return 1;
    }
    if (!did_return) {
        rust_backend_add_diag(p, fn->name_line, fn->name_col, "unsupported Rust backend feature `missing return`", "backend v1 requires a final return");
        return 1;
    }
    return 0;
}

int rust_backend_bridge_compile_file(const char *filename, const char *source, int source_len, const char *output_file, int compile_only, int strict_let_annotations, int strict_function_signatures) {
    RustParser p;
    RustAst *ast;
    RustResolveContext rctx;
    RustTypecheckContext tctx;
    long long ret_value = 0;
    FILE *out;
    char asm_file[256];
    char cmd[512];
    int ret;
    int al;
    int stop_at_asm = 0;
    const RustFunction *main_fn = 0;

    memset(&p, 0, sizeof(p));
    p.filename = filename;
    p.src = source;
    p.len = source_len;
    p.line = 1;
    p.col = 1;
    ast = rust_parse_program_internal(&p);
    if (p.num_diags > 0) {
        rust_print_diags(filename, p.diags, p.num_diags);
        return 1;
    }
    memset(&rctx, 0, sizeof(rctx));
    rctx.filename = filename;
    rctx.ast = ast;
    rctx.diags = p.diags;
    rctx.num_diags = &p.num_diags;
    rctx.max_diags = 256;
    if (rust_resolve_names(&rctx) != 0) {
        rust_print_diags(filename, p.diags, p.num_diags);
        free(rctx.symbols);
        return 1;
    }
    memset(&tctx, 0, sizeof(tctx));
    tctx.filename = filename;
    tctx.ast = ast;
    tctx.diags = p.diags;
    tctx.num_diags = &p.num_diags;
    tctx.max_diags = 256;
    tctx.symbols = rctx.symbols;
    tctx.symbol_count = rctx.symbol_count;
    tctx.symbol_types_len = rctx.symbol_count + 1;
    tctx.symbol_types = (int *)calloc((size_t)tctx.symbol_types_len, sizeof(int));
    tctx.strict_let_annotations = strict_let_annotations ? 1 : 0;
    tctx.strict_function_signatures = strict_function_signatures ? 1 : 0;
    if (!tctx.symbol_types) {
        rust_backend_add_diag(&p, 0, 0, "backend bridge allocation failed", "try again with more memory");
        rust_print_diags(filename, p.diags, p.num_diags);
        free(rctx.symbols);
        return 1;
    }
    if (rust_typecheck(&tctx) != 0) {
        rust_print_diags(filename, p.diags, p.num_diags);
        free(tctx.symbol_types);
        free(rctx.symbols);
        return 1;
    }

    strncpy(asm_file, output_file, 250);
    al = 0;
    while (asm_file[al]) al++;
    if (al >= 2 && asm_file[al - 2] == '.' && asm_file[al - 1] == 's') {
        stop_at_asm = 1;
    } else {
        asm_file[al] = '.';
        asm_file[al + 1] = 's';
        asm_file[al + 2] = 0;
    }
    out = fopen(asm_file, "w");
    if (!out) {
        rust_backend_add_diag(&p, 0, 0, "cannot write backend assembly output", "check output path permissions");
        rust_print_diags(filename, p.diags, p.num_diags);
        free(tctx.symbol_types);
        free(rctx.symbols);
        return 1;
    }
    main_fn = rust_backend_find_main_function(ast);
    if (main_fn && rust_backend_runtime_program_supported(ast)) {
        if (rust_backend_emit_runtime_program(out, &p, ast) != 0) {
            fclose(out);
            rust_print_diags(filename, p.diags, p.num_diags);
            free(tctx.symbol_types);
            free(rctx.symbols);
            return 1;
        }
    } else {
        if (rust_backend_extract_const_main(&p, ast, &ret_value) != 0) {
            fclose(out);
            rust_print_diags(filename, p.diags, p.num_diags);
            free(tctx.symbol_types);
            free(rctx.symbols);
            return 1;
        }
        fprintf(out, ".text\n");
        fprintf(out, ".globl main\n");
        fprintf(out, "main:\n");
        fprintf(out, "    movl $%lld, %%eax\n", ret_value);
        fprintf(out, "    ret\n");
    }
    fclose(out);

    if (!stop_at_asm) {
        if (compile_only) {
            sprintf(cmd, "gcc -O0 -w -fno-asynchronous-unwind-tables -Wa,--noexecstack -fno-unwind-tables -c -o %s %s 2>&1", output_file, asm_file);
        } else {
            sprintf(cmd, "gcc -O0 -w -fno-asynchronous-unwind-tables -Wa,--noexecstack -fno-unwind-tables -o %s %s -lm -lpthread -ldl 2>&1", output_file, asm_file);
        }
        ret = system(cmd);
        if (ret != 0) {
            rust_backend_add_diag(&p, 0, 0, "backend assembly/linking failed", "check toolchain and output permissions");
            rust_print_diags(filename, p.diags, p.num_diags);
            free(tctx.symbol_types);
            free(rctx.symbols);
            return 1;
        }
    }
    free(tctx.symbol_types);
    free(rctx.symbols);
    return 0;
}
