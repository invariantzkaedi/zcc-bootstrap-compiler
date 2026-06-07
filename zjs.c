#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

// External ZCC sprite bridge declarations
extern float g_phases[6];
extern const char* topology_names[6];
extern void zjs_serve_sprites_with_phases(void);

// Math function prototypes for ZCC compiler compatibility
double fabs(double);
double pow(double, double);
double fmod(double, double);


// === LEXER ===

typedef enum {
    TOK_EOF,
    TOK_NUMBER,
    TOK_STRING,
    TOK_IDENT,
    TOK_VAR,
    TOK_LET,
    TOK_CONST,
    TOK_FUNCTION,
    TOK_RETURN,
    TOK_IF,
    TOK_ELSE,
    TOK_WHILE,
    TOK_FOR,
    TOK_TRUE,
    TOK_FALSE,
    TOK_NULL,
    TOK_UNDEFINED,
    // Operators
    TOK_ASSIGN,       // =
    TOK_ADD_ASSIGN,   // +=
    TOK_SUB_ASSIGN,   // -=
    TOK_ADD,          // +
    TOK_SUB,          // -
    TOK_MUL,          // *
    TOK_DIV,          // /
    TOK_MOD,          // %
    TOK_INC,          // ++
    TOK_DEC,          // --
    TOK_EQ,           // ==
    TOK_NE,           // !=
    TOK_LT,           // <
    TOK_GT,           // >
    TOK_LE,           // <=
    TOK_GE,           // >=
    TOK_AND,          // &&
    TOK_OR,           // ||
    TOK_NOT,          // !
    // Punctuation
    TOK_LPAREN,       // (
    TOK_RPAREN,       // )
    TOK_LBRACE,       // {
    TOK_RBRACE,       // }
    TOK_LBRACKET,     // [
    TOK_RBRACKET,     // ]
    TOK_SEMICOLON,    // ;
    TOK_COMMA,        // ,
    TOK_COLON         // :
} TokenType;

typedef struct {
    TokenType type;
    double num_val;
    char str_val[256];
} Token;

#define MAX_TOKENS 4096
Token tokens[MAX_TOKENS];
int token_count = 0;
int token_index = 0;

void lex_js(const char* src) {
    const char* p = src;
    token_count = 0;
    token_index = 0;

    while (*p) {
        // Skip whitespace
        if (isspace(*p)) {
            p++;
            continue;
        }

        // Handle line comments
        if (*p == '/' && *(p+1) == '/') {
            p += 2;
            while (*p && *p != '\n' && *p != '\r') p++;
            continue;
        }

        // Handle block comments
        if (*p == '/' && *(p+1) == '*') {
            p += 2;
            while (*p && !(*p == '*' && *(p+1) == '/')) p++;
            if (*p) p += 2;
            continue;
        }

        // Handle numbers
        if (isdigit(*p)) {
            double val = 0.0;
            while (isdigit(*p)) {
                val = val * 10.0 + (*p - '0');
                p++;
            }
            if (*p == '.') {
                p++;
                double dec = 10.0;
                while (isdigit(*p)) {
                    val = val + (*p - '0') / dec;
                    dec *= 10.0;
                    p++;
                }
            }
            tokens[token_count].type = TOK_NUMBER;
            tokens[token_count].num_val = val;
            if (val == (int)val) {
                sprintf(tokens[token_count].str_val, "%d", (int)val);
            } else {
                sprintf(tokens[token_count].str_val, "%f", val);
            }
            token_count++;
            continue;
        }

        // Handle strings
        if (*p == '"' || *p == '\'') {
            char quote = *p;
            p++;
            int len = 0;
            while (*p && *p != quote) {
                if (*p == '\\') {
                    p++; // simple escape skip
                }
                if (len < 255) {
                    tokens[token_count].str_val[len++] = *p;
                }
                p++;
            }
            tokens[token_count].str_val[len] = '\0';
            if (*p == quote) p++;
            tokens[token_count].type = TOK_STRING;
            token_count++;
            continue;
        }

        // Handle Identifiers / Keywords / console.log
        if (isalpha(*p) || *p == '_' || *p == '$') {
            int len = 0;
            while (isalnum(*p) || *p == '_' || *p == '$' || *p == '.') {
                if (len < 255) {
                    tokens[token_count].str_val[len++] = *p;
                }
                p++;
            }
            tokens[token_count].str_val[len] = '\0';

            char* s = tokens[token_count].str_val;
            if (strcmp(s, "var") == 0) tokens[token_count].type = TOK_VAR;
            else if (strcmp(s, "let") == 0) tokens[token_count].type = TOK_LET;
            else if (strcmp(s, "const") == 0) tokens[token_count].type = TOK_CONST;
            else if (strcmp(s, "function") == 0) tokens[token_count].type = TOK_FUNCTION;
            else if (strcmp(s, "return") == 0) tokens[token_count].type = TOK_RETURN;
            else if (strcmp(s, "if") == 0) tokens[token_count].type = TOK_IF;
            else if (strcmp(s, "else") == 0) tokens[token_count].type = TOK_ELSE;
            else if (strcmp(s, "while") == 0) tokens[token_count].type = TOK_WHILE;
            else if (strcmp(s, "for") == 0) tokens[token_count].type = TOK_FOR;
            else if (strcmp(s, "true") == 0) tokens[token_count].type = TOK_TRUE;
            else if (strcmp(s, "false") == 0) tokens[token_count].type = TOK_FALSE;
            else if (strcmp(s, "null") == 0) tokens[token_count].type = TOK_NULL;
            else if (strcmp(s, "undefined") == 0) tokens[token_count].type = TOK_UNDEFINED;
            else tokens[token_count].type = TOK_IDENT;

            token_count++;
            continue;
        }

        // Handle Operators / Symbols
        if (*p == '=') {
            p++;
            if (*p == '=') {
                p++;
                tokens[token_count].type = TOK_EQ;
                strcpy(tokens[token_count].str_val, "==");
            } else {
                tokens[token_count].type = TOK_ASSIGN;
                strcpy(tokens[token_count].str_val, "=");
            }
            token_count++;
            continue;
        }

        if (*p == '+') {
            p++;
            if (*p == '+') {
                p++;
                tokens[token_count].type = TOK_INC;
                strcpy(tokens[token_count].str_val, "++");
            } else if (*p == '=') {
                p++;
                tokens[token_count].type = TOK_ADD_ASSIGN;
                strcpy(tokens[token_count].str_val, "+=");
            } else {
                tokens[token_count].type = TOK_ADD;
                strcpy(tokens[token_count].str_val, "+");
            }
            token_count++;
            continue;
        }

        if (*p == '-') {
            p++;
            if (*p == '-') {
                p++;
                tokens[token_count].type = TOK_DEC;
                strcpy(tokens[token_count].str_val, "--");
            } else if (*p == '=') {
                p++;
                tokens[token_count].type = TOK_SUB_ASSIGN;
                strcpy(tokens[token_count].str_val, "-=");
            } else {
                tokens[token_count].type = TOK_SUB;
                strcpy(tokens[token_count].str_val, "-");
            }
            token_count++;
            continue;
        }

        if (*p == '*') {
            p++;
            tokens[token_count].type = TOK_MUL;
            strcpy(tokens[token_count].str_val, "*");
            token_count++;
            continue;
        }

        if (*p == '/') {
            p++;
            tokens[token_count].type = TOK_DIV;
            strcpy(tokens[token_count].str_val, "/");
            token_count++;
            continue;
        }

        if (*p == '%') {
            p++;
            tokens[token_count].type = TOK_MOD;
            strcpy(tokens[token_count].str_val, "%");
            token_count++;
            continue;
        }

        if (*p == '!') {
            p++;
            if (*p == '=') {
                p++;
                tokens[token_count].type = TOK_NE;
                strcpy(tokens[token_count].str_val, "!=");
            } else {
                tokens[token_count].type = TOK_NOT;
                strcpy(tokens[token_count].str_val, "!");
            }
            token_count++;
            continue;
        }

        if (*p == '<') {
            p++;
            if (*p == '=') {
                p++;
                tokens[token_count].type = TOK_LE;
                strcpy(tokens[token_count].str_val, "<=");
            } else {
                tokens[token_count].type = TOK_LT;
                strcpy(tokens[token_count].str_val, "<");
            }
            token_count++;
            continue;
        }

        if (*p == '>') {
            p++;
            if (*p == '=') {
                p++;
                tokens[token_count].type = TOK_GE;
                strcpy(tokens[token_count].str_val, ">=");
            } else {
                tokens[token_count].type = TOK_GT;
                strcpy(tokens[token_count].str_val, ">");
            }
            token_count++;
            continue;
        }

        if (*p == '&' && *(p+1) == '&') {
            p += 2;
            tokens[token_count].type = TOK_AND;
            strcpy(tokens[token_count].str_val, "&&");
            token_count++;
            continue;
        }

        if (*p == '|' && *(p+1) == '|') {
            p += 2;
            tokens[token_count].type = TOK_OR;
            strcpy(tokens[token_count].str_val, "||");
            token_count++;
            continue;
        }

        if (*p == '(') {
            tokens[token_count].type = TOK_LPAREN;
            strcpy(tokens[token_count].str_val, "(");
            p++; token_count++; continue;
        }
        if (*p == ')') {
            tokens[token_count].type = TOK_RPAREN;
            strcpy(tokens[token_count].str_val, ")");
            p++; token_count++; continue;
        }
        if (*p == '{') {
            tokens[token_count].type = TOK_LBRACE;
            strcpy(tokens[token_count].str_val, "{");
            p++; token_count++; continue;
        }
        if (*p == '}') {
            tokens[token_count].type = TOK_RBRACE;
            strcpy(tokens[token_count].str_val, "}");
            p++; token_count++; continue;
        }
        if (*p == '[') {
            tokens[token_count].type = TOK_LBRACKET;
            strcpy(tokens[token_count].str_val, "[");
            p++; token_count++; continue;
        }
        if (*p == ']') {
            tokens[token_count].type = TOK_RBRACKET;
            strcpy(tokens[token_count].str_val, "]");
            p++; token_count++; continue;
        }
        if (*p == ';') {
            tokens[token_count].type = TOK_SEMICOLON;
            strcpy(tokens[token_count].str_val, ";");
            p++; token_count++; continue;
        }
        if (*p == ',') {
            tokens[token_count].type = TOK_COMMA;
            strcpy(tokens[token_count].str_val, ",");
            p++; token_count++; continue;
        }
        if (*p == ':') {
            tokens[token_count].type = TOK_COLON;
            strcpy(tokens[token_count].str_val, ":");
            p++; token_count++; continue;
        }

        // Unknown character - skip
        p++;
    }

    tokens[token_count].type = TOK_EOF;
    strcpy(tokens[token_count].str_val, "EOF");
    token_count++;
}

// === PARSER ===

typedef enum {
    NODE_INT,
    NODE_STR,
    NODE_BOOL,
    NODE_UNDEFINED,
    NODE_NULL,
    NODE_VAR_REF,
    NODE_ASSIGN,
    NODE_ADD_ASSIGN,
    NODE_SUB_ASSIGN,
    NODE_BINOP,
    NODE_UNOP,
    NODE_BLOCK,
    NODE_IF,
    NODE_WHILE,
    NODE_FOR,
    NODE_VAR_DECL,
    NODE_FUNC_DECL,
    NODE_CALL,
    NODE_RETURN,
    NODE_ARRAY_LIT,
    NODE_ARRAY_INDEX
} NodeType;

typedef struct ASTNode {
    NodeType type;
    int int_val;
    double num_val;
    char str_val[256];
    char op[4];
    struct ASTNode* left;
    struct ASTNode* right;
    struct ASTNode* cond;
    struct ASTNode* init;
    struct ASTNode* post;
    struct ASTNode* children[128];
    int child_count;
} ASTNode;

#define MAX_AST_NODES 8192
ASTNode ast_node_pool[MAX_AST_NODES];
int ast_node_index = 0;

ASTNode* create_node(NodeType type) {
    if (ast_node_index >= MAX_AST_NODES) {
        ast_node_index = 0; // wrap around / reset
    }
    ASTNode* node = &ast_node_pool[ast_node_index++];
    node->type = type;
    node->int_val = 0;
    node->num_val = 0.0;
    node->str_val[0] = '\0';
    node->op[0] = '\0';
    node->left = NULL;
    node->right = NULL;
    node->cond = NULL;
    node->init = NULL;
    node->post = NULL;
    node->child_count = 0;
    return node;
}

ASTNode* parse_expression(void);
ASTNode* parse_statement(void);
ASTNode* parse_block(void);

ASTNode* parse_primary(void) {
    Token t = tokens[token_index];
    if (t.type == TOK_NUMBER) {
        token_index++;
        ASTNode* n = create_node(NODE_INT);
        n->num_val = t.num_val;
        return n;
    }
    if (t.type == TOK_STRING) {
        token_index++;
        ASTNode* n = create_node(NODE_STR);
        strcpy(n->str_val, t.str_val);
        return n;
    }
    if (t.type == TOK_TRUE) {
        token_index++;
        ASTNode* n = create_node(NODE_BOOL);
        n->int_val = 1;
        return n;
    }
    if (t.type == TOK_FALSE) {
        token_index++;
        ASTNode* n = create_node(NODE_BOOL);
        n->int_val = 0;
        return n;
    }
    if (t.type == TOK_NULL) {
        token_index++;
        return create_node(NODE_NULL);
    }
    if (t.type == TOK_UNDEFINED) {
        token_index++;
        return create_node(NODE_UNDEFINED);
    }
    if (t.type == TOK_IDENT) {
        token_index++;
        // Check for function call
        if (tokens[token_index].type == TOK_LPAREN) {
            token_index++; // consume '('
            ASTNode* call = create_node(NODE_CALL);
            strcpy(call->str_val, t.str_val);
            if (tokens[token_index].type != TOK_RPAREN) {
                while (1) {
                    call->children[call->child_count++] = parse_expression();
                    if (tokens[token_index].type == TOK_COMMA) {
                        token_index++;
                    } else {
                        break;
                    }
                }
            }
            if (tokens[token_index].type == TOK_RPAREN) token_index++;
            return call;
        } else {
            ASTNode* ref = create_node(NODE_VAR_REF);
            strcpy(ref->str_val, t.str_val);
            return ref;
        }
    }
    if (t.type == TOK_LPAREN) {
        token_index++;
        ASTNode* expr = parse_expression();
        if (tokens[token_index].type == TOK_RPAREN) token_index++;
        return expr;
    }
    if (t.type == TOK_LBRACKET) {
        // Array literal
        token_index++; // consume '['
        ASTNode* arr = create_node(NODE_ARRAY_LIT);
        if (tokens[token_index].type != TOK_RBRACKET) {
            while (1) {
                arr->children[arr->child_count++] = parse_expression();
                if (tokens[token_index].type == TOK_COMMA) {
                    token_index++;
                } else {
                    break;
                }
            }
        }
        if (tokens[token_index].type == TOK_RBRACKET) token_index++;
        return arr;
    }
    return NULL;
}

ASTNode* parse_postfix(void) {
    ASTNode* node = parse_primary();
    if (!node) return NULL;

    // Check for array indexing
    while (tokens[token_index].type == TOK_LBRACKET) {
        token_index++; // consume '['
        ASTNode* index = parse_expression();
        if (tokens[token_index].type == TOK_RBRACKET) token_index++;
        ASTNode* arr_idx = create_node(NODE_ARRAY_INDEX);
        arr_idx->left = node;
        arr_idx->right = index;
        node = arr_idx;
    }

    // Check for post increment / decrement
    if (tokens[token_index].type == TOK_INC || tokens[token_index].type == TOK_DEC) {
        Token op = tokens[token_index++];
        ASTNode* un = create_node(NODE_UNOP);
        strcpy(un->op, op.str_val);
        un->left = node;
        un->int_val = 1; // indicates postfix
        node = un;
    }

    return node;
}

ASTNode* parse_unary(void) {
    Token t = tokens[token_index];
    if (t.type == TOK_NOT || t.type == TOK_SUB || t.type == TOK_INC || t.type == TOK_DEC) {
        token_index++;
        ASTNode* un = create_node(NODE_UNOP);
        strcpy(un->op, t.str_val);
        un->left = parse_unary();
        un->int_val = 0; // prefix
        return un;
    }
    return parse_postfix();
}

ASTNode* parse_mul_div(void) {
    ASTNode* left = parse_unary();
    while (tokens[token_index].type == TOK_MUL || tokens[token_index].type == TOK_DIV || tokens[token_index].type == TOK_MOD) {
        Token op = tokens[token_index++];
        ASTNode* n = create_node(NODE_BINOP);
        strcpy(n->op, op.str_val);
        n->left = left;
        n->right = parse_unary();
        left = n;
    }
    return left;
}

ASTNode* parse_add_sub(void) {
    ASTNode* left = parse_mul_div();
    while (tokens[token_index].type == TOK_ADD || tokens[token_index].type == TOK_SUB) {
        Token op = tokens[token_index++];
        ASTNode* n = create_node(NODE_BINOP);
        strcpy(n->op, op.str_val);
        n->left = left;
        n->right = parse_mul_div();
        left = n;
    }
    return left;
}

ASTNode* parse_relation(void) {
    ASTNode* left = parse_add_sub();
    while (tokens[token_index].type == TOK_LT || tokens[token_index].type == TOK_GT ||
           tokens[token_index].type == TOK_LE || tokens[token_index].type == TOK_GE) {
        Token op = tokens[token_index++];
        ASTNode* n = create_node(NODE_BINOP);
        strcpy(n->op, op.str_val);
        n->left = left;
        n->right = parse_add_sub();
        left = n;
    }
    return left;
}

ASTNode* parse_equality(void) {
    ASTNode* left = parse_relation();
    while (tokens[token_index].type == TOK_EQ || tokens[token_index].type == TOK_NE) {
        Token op = tokens[token_index++];
        ASTNode* n = create_node(NODE_BINOP);
        strcpy(n->op, op.str_val);
        n->left = left;
        n->right = parse_relation();
        left = n;
    }
    return left;
}

ASTNode* parse_logical_and(void) {
    ASTNode* left = parse_equality();
    while (tokens[token_index].type == TOK_AND) {
        Token op = tokens[token_index++];
        ASTNode* n = create_node(NODE_BINOP);
        strcpy(n->op, op.str_val);
        n->left = left;
        n->right = parse_equality();
        left = n;
    }
    return left;
}

ASTNode* parse_logical_or(void) {
    ASTNode* left = parse_logical_and();
    while (tokens[token_index].type == TOK_OR) {
        Token op = tokens[token_index++];
        ASTNode* n = create_node(NODE_BINOP);
        strcpy(n->op, op.str_val);
        n->left = left;
        n->right = parse_logical_and();
        left = n;
    }
    return left;
}

ASTNode* parse_assignment(void) {
    ASTNode* left = parse_logical_or();
    if (!left) return NULL;

    if (tokens[token_index].type == TOK_ASSIGN ||
        tokens[token_index].type == TOK_ADD_ASSIGN ||
        tokens[token_index].type == TOK_SUB_ASSIGN) {
        Token op = tokens[token_index++];
        ASTNode* assign = create_node(
            op.type == TOK_ASSIGN ? NODE_ASSIGN :
            (op.type == TOK_ADD_ASSIGN ? NODE_ADD_ASSIGN : NODE_SUB_ASSIGN)
        );
        assign->left = left;
        assign->right = parse_assignment();
        return assign;
    }
    return left;
}

ASTNode* parse_expression(void) {
    return parse_assignment();
}

ASTNode* parse_statement(void) {
    Token t = tokens[token_index];

    // Optional semicolon skip
    while (t.type == TOK_SEMICOLON) {
        token_index++;
        t = tokens[token_index];
    }

    if (t.type == TOK_VAR || t.type == TOK_LET || t.type == TOK_CONST) {
        token_index++; // consume keyword
        Token name = tokens[token_index++];
        ASTNode* decl = create_node(NODE_VAR_DECL);
        strcpy(decl->str_val, name.str_val);
        if (tokens[token_index].type == TOK_ASSIGN) {
            token_index++; // consume '='
            decl->left = parse_expression();
        }
        if (tokens[token_index].type == TOK_SEMICOLON) token_index++;
        return decl;
    }

    if (t.type == TOK_FUNCTION) {
        token_index++; // consume 'function'
        Token name = tokens[token_index++];
        token_index++; // consume '('
        ASTNode* func = create_node(NODE_FUNC_DECL);
        strcpy(func->str_val, name.str_val);

        if (tokens[token_index].type != TOK_RPAREN) {
            while (1) {
                Token param = tokens[token_index++];
                ASTNode* param_node = create_node(NODE_VAR_REF);
                strcpy(param_node->str_val, param.str_val);
                func->children[func->child_count++] = param_node;

                if (tokens[token_index].type == TOK_COMMA) {
                    token_index++;
                } else {
                    break;
                }
            }
        }
        if (tokens[token_index].type == TOK_RPAREN) token_index++;
        func->left = parse_block();
        return func;
    }

    if (t.type == TOK_IF) {
        token_index++; // consume 'if'
        token_index++; // consume '('
        ASTNode* n = create_node(NODE_IF);
        n->cond = parse_expression();
        if (tokens[token_index].type == TOK_RPAREN) token_index++;
        n->left = parse_statement();

        if (tokens[token_index].type == TOK_ELSE) {
            token_index++; // consume 'else'
            n->right = parse_statement();
        }
        return n;
    }

    if (t.type == TOK_WHILE) {
        token_index++; // consume 'while'
        token_index++; // consume '('
        ASTNode* n = create_node(NODE_WHILE);
        n->cond = parse_expression();
        if (tokens[token_index].type == TOK_RPAREN) token_index++;
        n->left = parse_statement();
        return n;
    }

    if (t.type == TOK_FOR) {
        token_index++; // consume 'for'
        token_index++; // consume '('

        ASTNode* init = NULL;
        if (tokens[token_index].type == TOK_VAR || tokens[token_index].type == TOK_LET || tokens[token_index].type == TOK_CONST) {
            token_index++; // consume keyword
            Token name = tokens[token_index++];
            init = create_node(NODE_VAR_DECL);
            strcpy(init->str_val, name.str_val);
            if (tokens[token_index].type == TOK_ASSIGN) {
                token_index++; // consume '='
                init->left = parse_expression();
            }
        } else if (tokens[token_index].type != TOK_SEMICOLON) {
            init = parse_expression();
        }
        if (tokens[token_index].type == TOK_SEMICOLON) token_index++;

        ASTNode* cond = NULL;
        if (tokens[token_index].type != TOK_SEMICOLON) {
            cond = parse_expression();
        }
        if (tokens[token_index].type == TOK_SEMICOLON) token_index++;

        ASTNode* post = NULL;
        if (tokens[token_index].type != TOK_RPAREN) {
            post = parse_expression();
        }
        if (tokens[token_index].type == TOK_RPAREN) token_index++;

        ASTNode* n = create_node(NODE_FOR);
        n->init = init;
        n->cond = cond;
        n->post = post;
        n->left = parse_statement();
        return n;
    }

    if (t.type == TOK_RETURN) {
        token_index++; // consume 'return'
        ASTNode* n = create_node(NODE_RETURN);
        if (tokens[token_index].type != TOK_SEMICOLON && tokens[token_index].type != TOK_EOF && tokens[token_index].type != TOK_RBRACE) {
            n->left = parse_expression();
        }
        if (tokens[token_index].type == TOK_SEMICOLON) token_index++;
        return n;
    }

    if (t.type == TOK_LBRACE) {
        return parse_block();
    }

    // Expression fallback statement
    ASTNode* expr = parse_expression();
    if (tokens[token_index].type == TOK_SEMICOLON) token_index++;
    return expr;
}

ASTNode* parse_block(void) {
    ASTNode* block = create_node(NODE_BLOCK);
    if (tokens[token_index].type == TOK_LBRACE) {
        token_index++; // consume '{'
        while (tokens[token_index].type != TOK_RBRACE && tokens[token_index].type != TOK_EOF) {
            ASTNode* stmt = parse_statement();
            if (stmt) {
                block->children[block->child_count++] = stmt;
            }
        }
        if (tokens[token_index].type == TOK_RBRACE) token_index++;
    } else {
        ASTNode* stmt = parse_statement();
        if (stmt) block->children[block->child_count++] = stmt;
    }
    return block;
}

ASTNode* parse_program(void) {
    ASTNode* root = create_node(NODE_BLOCK);
    while (tokens[token_index].type != TOK_EOF) {
        ASTNode* stmt = parse_statement();
        if (stmt) {
            root->children[root->child_count++] = stmt;
        }
    }
    return root;
}

// === INTERPRETER / RUNTIME ===

typedef enum {
    VAL_UNDEFINED,
    VAL_NULL,
    VAL_NUMBER,
    VAL_STRING,
    VAL_BOOL,
    VAL_FUNCTION,
    VAL_ARRAY
} JSValueType;

struct Env; // Forward declaration

typedef struct JSValue {
    JSValueType type;
    union {
        double num_val;
        char str_val[256];
        int bool_val;
        struct {
            ASTNode* func_node;
            struct Env* func_env; // Defining lexical environment
        };
        struct {
            struct JSValue* array_elems[64];
            int array_len;
        };
    };
} JSValue;

#define MAX_VALS 65536
JSValue val_pool[MAX_VALS];
int val_pool_index = 0;

JSValue* alloc_val(JSValueType type) {
    if (val_pool_index >= MAX_VALS) {
        val_pool_index = 0; // bound wrap around
    }
    JSValue* v = &val_pool[val_pool_index++];
    memset(v, 0, sizeof(JSValue));
    v->type = type;
    return v;
}

typedef struct Binding {
    char name[64];
    JSValue* val;
    struct Binding* next;
} Binding;

#define MAX_BINDINGS 16384
Binding binding_pool[MAX_BINDINGS];
int binding_pool_index = 0;

Binding* alloc_binding(const char* name, JSValue* val, Binding* next) {
    if (binding_pool_index >= MAX_BINDINGS) {
        binding_pool_index = 0;
    }
    Binding* b = &binding_pool[binding_pool_index++];
    strcpy(b->name, name);
    b->val = val;
    b->next = next;
    return b;
}

typedef struct Env {
    Binding* head;
    struct Env* parent;
} Env;

#define MAX_ENVS 1024
Env env_pool[MAX_ENVS];
int env_pool_index = 0;

Env* create_env(Env* parent) {
    if (env_pool_index >= MAX_ENVS) {
        env_pool_index = 0;
    }
    Env* env = &env_pool[env_pool_index++];
    env->head = NULL;
    env->parent = parent;
    return env;
}

void define_env_var(Env* env, const char* name, JSValue* val) {
    Binding* curr = env->head;
    while (curr) {
        if (strcmp(curr->name, name) == 0) {
            curr->val = val;
            return;
        }
        curr = curr->next;
    }
    env->head = alloc_binding(name, val, env->head);
}

void set_env_var(Env* env, const char* name, JSValue* val) {
    // Check if it already exists in the local or parent environments
    Env* curr = env;
    while (curr) {
        Binding* b = curr->head;
        while (b) {
            if (strcmp(b->name, name) == 0) {
                b->val = val;
                return;
            }
            b = b->next;
        }
        curr = curr->parent;
    }
    // Otherwise bind it to the local environment
    define_env_var(env, name, val);
}

JSValue* get_env_var(Env* env, const char* name) {
    Env* curr = env;
    while (curr) {
        Binding* b = curr->head;
        while (b) {
            if (strcmp(b->name, name) == 0) {
                return b->val;
            }
            b = b->next;
        }
        curr = curr->parent;
    }
    return alloc_val(VAL_UNDEFINED);
}

JSValue* eval_node(ASTNode* node, Env* env, int* returned);

static void val_to_str(JSValue* val, char* buf) {
    if (val->type == VAL_STRING) {
        strcpy(buf, val->str_val);
    } else if (val->type == VAL_NUMBER) {
        if (val->num_val == (int)val->num_val) {
            sprintf(buf, "%d", (int)val->num_val);
        } else {
            sprintf(buf, "%f", val->num_val);
        }
    } else if (val->type == VAL_BOOL) {
        strcpy(buf, val->bool_val ? "true" : "false");
    } else if (val->type == VAL_NULL) {
        strcpy(buf, "null");
    } else {
        strcpy(buf, "undefined");
    }
}

JSValue* eval_binary(ASTNode* node, Env* env, int* returned) {
    JSValue* left = eval_node(node->left, env, returned);
    if (*returned) return left;
    JSValue* right = eval_node(node->right, env, returned);
    if (*returned) return right;

    // String Concatenation or Numeric Addition
    if (strcmp(node->op, "+") == 0) {
        if (left->type == VAL_STRING || right->type == VAL_STRING) {
            JSValue* r = alloc_val(VAL_STRING);
            char l_str[256];
            char r_str[256];
            val_to_str(left, l_str);
            val_to_str(right, r_str);
            sprintf(r->str_val, "%s%s", l_str, r_str);
            return r;
        } else {
            JSValue* r = alloc_val(VAL_NUMBER);
            r->num_val = left->num_val + right->num_val;
            return r;
        }
    }

    if (strcmp(node->op, "-") == 0) {
        JSValue* r = alloc_val(VAL_NUMBER);
        r->num_val = left->num_val - right->num_val;
        return r;
    }
    if (strcmp(node->op, "*") == 0) {
        JSValue* r = alloc_val(VAL_NUMBER);
        r->num_val = left->num_val * right->num_val;
        return r;
    }
    if (strcmp(node->op, "/") == 0) {
        JSValue* r = alloc_val(VAL_NUMBER);
        r->num_val = right->num_val != 0.0 ? left->num_val / right->num_val : 0.0;
        return r;
    }
    if (strcmp(node->op, "%") == 0) {
        JSValue* r = alloc_val(VAL_NUMBER);
        r->num_val = right->num_val != 0.0 ? fmod(left->num_val, right->num_val) : 0.0;
        return r;
    }

    // Comparison Operators
    if (strcmp(node->op, "==") == 0) {
        JSValue* r = alloc_val(VAL_BOOL);
        if (left->type != right->type) {
            r->bool_val = 0;
        } else if (left->type == VAL_NUMBER) {
            r->bool_val = (left->num_val == right->num_val);
        } else if (left->type == VAL_STRING) {
            r->bool_val = (strcmp(left->str_val, right->str_val) == 0);
        } else if (left->type == VAL_BOOL) {
            r->bool_val = (left->bool_val == right->bool_val);
        } else {
            r->bool_val = 1; // undefined == undefined, null == null
        }
        return r;
    }
    if (strcmp(node->op, "!=") == 0) {
        JSValue* r = alloc_val(VAL_BOOL);
        JSValue* eq = eval_binary(node, env, returned); // evaluate equality
        r->bool_val = !eq->bool_val;
        return r;
    }

    if (strcmp(node->op, "<") == 0) {
        JSValue* r = alloc_val(VAL_BOOL);
        r->bool_val = left->num_val < right->num_val;
        return r;
    }
    if (strcmp(node->op, ">") == 0) {
        JSValue* r = alloc_val(VAL_BOOL);
        r->bool_val = left->num_val > right->num_val;
        return r;
    }
    if (strcmp(node->op, "<=") == 0) {
        JSValue* r = alloc_val(VAL_BOOL);
        r->bool_val = left->num_val <= right->num_val;
        return r;
    }
    if (strcmp(node->op, ">=") == 0) {
        JSValue* r = alloc_val(VAL_BOOL);
        r->bool_val = left->num_val >= right->num_val;
        return r;
    }

    // Logical Operators
    if (strcmp(node->op, "&&") == 0) {
        JSValue* r = alloc_val(VAL_BOOL);
        r->bool_val = (left->type == VAL_BOOL ? left->bool_val : (left->num_val != 0.0)) &&
                      (right->type == VAL_BOOL ? right->bool_val : (right->num_val != 0.0));
        return r;
    }
    if (strcmp(node->op, "||") == 0) {
        JSValue* r = alloc_val(VAL_BOOL);
        r->bool_val = (left->type == VAL_BOOL ? left->bool_val : (left->num_val != 0.0)) ||
                      (right->type == VAL_BOOL ? right->bool_val : (right->num_val != 0.0));
        return r;
    }

    return alloc_val(VAL_UNDEFINED);
}

JSValue* eval_node(ASTNode* node, Env* env, int* returned) {
    if (!node) return alloc_val(VAL_UNDEFINED);

    switch (node->type) {
        case NODE_INT: {
            JSValue* v = alloc_val(VAL_NUMBER);
            v->num_val = node->num_val;
            return v;
        }
        case NODE_STR: {
            JSValue* v = alloc_val(VAL_STRING);
            strcpy(v->str_val, node->str_val);
            return v;
        }
        case NODE_BOOL: {
            JSValue* v = alloc_val(VAL_BOOL);
            v->bool_val = node->int_val;
            return v;
        }
        case NODE_NULL:
            return alloc_val(VAL_NULL);

        case NODE_UNDEFINED:
            return alloc_val(VAL_UNDEFINED);

        case NODE_VAR_REF:
            return get_env_var(env, node->str_val);

        case NODE_VAR_DECL: {
            JSValue* val = alloc_val(VAL_UNDEFINED);
            if (node->left) {
                val = eval_node(node->left, env, returned);
            }
            define_env_var(env, node->str_val, val);
            return val;
        }

        case NODE_ASSIGN: {
            JSValue* val = eval_node(node->right, env, returned);
            if (node->left->type == NODE_VAR_REF) {
                set_env_var(env, node->left->str_val, val);
            } else if (node->left->type == NODE_ARRAY_INDEX) {
                JSValue* arr = eval_node(node->left->left, env, returned);
                JSValue* idx = eval_node(node->left->right, env, returned);
                if (arr->type == VAL_ARRAY && idx->type == VAL_NUMBER) {
                    int i = (int)idx->num_val;
                    if (i >= 0 && i < 64) {
                        arr->array_elems[i] = val;
                        if (i >= arr->array_len) {
                            arr->array_len = i + 1;
                        }
                    }
                }
            }
            return val;
        }

        case NODE_ADD_ASSIGN:
        case NODE_SUB_ASSIGN: {
            JSValue* right = eval_node(node->right, env, returned);
            JSValue* left = eval_node(node->left, env, returned);
            JSValue* val = alloc_val(VAL_NUMBER);
            if (node->type == NODE_ADD_ASSIGN) {
                val->num_val = left->num_val + right->num_val;
            } else {
                val->num_val = left->num_val - right->num_val;
            }

            if (node->left->type == NODE_VAR_REF) {
                set_env_var(env, node->left->str_val, val);
            } else if (node->left->type == NODE_ARRAY_INDEX) {
                JSValue* arr = eval_node(node->left->left, env, returned);
                JSValue* idx = eval_node(node->left->right, env, returned);
                if (arr->type == VAL_ARRAY && idx->type == VAL_NUMBER) {
                    int i = (int)idx->num_val;
                    if (i >= 0 && i < 64) {
                        arr->array_elems[i] = val;
                    }
                }
            }
            return val;
        }

        case NODE_BINOP:
            return eval_binary(node, env, returned);

        case NODE_UNOP: {
            JSValue* left = eval_node(node->left, env, returned);
            if (strcmp(node->op, "!") == 0) {
                JSValue* r = alloc_val(VAL_BOOL);
                int truthy = (left->type == VAL_BOOL ? left->bool_val : (left->type == VAL_NUMBER ? left->num_val != 0.0 : (left->type != VAL_UNDEFINED && left->type != VAL_NULL)));
                r->bool_val = !truthy;
                return r;
            }
            if (strcmp(node->op, "-") == 0) {
                JSValue* r = alloc_val(VAL_NUMBER);
                r->num_val = -left->num_val;
                return r;
            }
            if (strcmp(node->op, "++") == 0 || strcmp(node->op, "--") == 0) {
                int is_inc = (strcmp(node->op, "++") == 0);
                JSValue* original = alloc_val(VAL_NUMBER);
                original->num_val = left->num_val;

                JSValue* updated = alloc_val(VAL_NUMBER);
                updated->num_val = left->num_val + (is_inc ? 1.0 : -1.0);

                if (node->left->type == NODE_VAR_REF) {
                    set_env_var(env, node->left->str_val, updated);
                }

                // If node->int_val == 1, it is postfix (returns original value)
                return node->int_val == 1 ? original : updated;
            }
            return alloc_val(VAL_UNDEFINED);
        }

        case NODE_BLOCK: {
            JSValue* last = alloc_val(VAL_UNDEFINED);
            for (int i = 0; i < node->child_count; i++) {
                last = eval_node(node->children[i], env, returned);
                if (*returned) break;
            }
            return last;
        }

        case NODE_IF: {
            JSValue* cond = eval_node(node->cond, env, returned);
            int truthy = (cond->type == VAL_BOOL ? cond->bool_val : (cond->num_val != 0.0));
            if (truthy) {
                return eval_node(node->left, env, returned);
            } else if (node->right) {
                return eval_node(node->right, env, returned);
            }
            return alloc_val(VAL_UNDEFINED);
        }

        case NODE_WHILE: {
            JSValue* last = alloc_val(VAL_UNDEFINED);
            while (1) {
                JSValue* cond = eval_node(node->cond, env, returned);
                int truthy = (cond->type == VAL_BOOL ? cond->bool_val : (cond->num_val != 0.0));
                if (!truthy) break;

                last = eval_node(node->left, env, returned);
                if (*returned) break;
            }
            return last;
        }

        case NODE_FOR: {
            Env* loop_env = create_env(env);
            if (node->init) {
                eval_node(node->init, loop_env, returned);
            }
            JSValue* last = alloc_val(VAL_UNDEFINED);
            while (1) {
                if (node->cond) {
                    JSValue* cond = eval_node(node->cond, loop_env, returned);
                    int truthy = (cond->type == VAL_BOOL ? cond->bool_val : (cond->num_val != 0.0));
                    if (!truthy) break;
                }
                last = eval_node(node->left, loop_env, returned);
                if (*returned) break;

                if (node->post) {
                    eval_node(node->post, loop_env, returned);
                }
            }
            return last;
        }

        case NODE_FUNC_DECL: {
            JSValue* func = alloc_val(VAL_FUNCTION);
            func->func_node = node;
            func->func_env = env; // lexical scope parent environment
            define_env_var(env, node->str_val, func);
            return func;
        }

        case NODE_CALL: {
            // ZCC.generateSprites / ZCC.sprites built-ins
            if (strcmp(node->str_val, "ZCC.generateSprites") == 0 || strcmp(node->str_val, "ZCC.sprites") == 0) {
                zjs_serve_sprites_with_phases();
                JSValue* res = alloc_val(VAL_BOOL);
                res->bool_val = 1;
                return res;
            }

            // ZCC.setPhase built-in
            if (strcmp(node->str_val, "ZCC.setPhase") == 0) {
                if (node->child_count < 2) {
                    printf("ZCC.setPhase: usage -> ZCC.setPhase(\"reentrancy\", 0.73)\n");
                    return alloc_val(VAL_UNDEFINED);
                }
                JSValue* name_val = eval_node(node->children[0], env, returned);
                JSValue* phase_val = eval_node(node->children[1], env, returned);
                if (name_val->type != VAL_STRING || phase_val->type != VAL_NUMBER) {
                    printf("ZCC.setPhase: invalid arguments\n");
                    return alloc_val(VAL_UNDEFINED);
                }
                const char* name = name_val->str_val;
                double phase = phase_val->num_val;
                if (phase < 0.0) phase = 0.0;
                if (phase > 1.0) phase = 1.0;

                int updated = 0;
                for (int i = 0; i < 6; i++) {
                    if (strcmp(name, topology_names[i]) == 0 || strcmp(name, "all") == 0) {
                        g_phases[i] = (float)phase;
                        updated = 1;
                        printf("ZCC.setPhase -> %s = %.3f\n", topology_names[i], phase);
                        if (strcmp(name, "all") != 0) break;
                    }
                }
                if (updated) {
                    zjs_serve_sprites_with_phases();
                }
                JSValue* res = alloc_val(VAL_BOOL);
                res->bool_val = updated;
                return res;
            }

            // Built-in Native function console.log
            if (strcmp(node->str_val, "console.log") == 0) {
                for (int i = 0; i < node->child_count; i++) {
                    JSValue* val = eval_node(node->children[i], env, returned);
                    if (val->type == VAL_NUMBER) {
                        if (val->num_val == (int)val->num_val) {
                            printf("%d", (int)val->num_val);
                        } else {
                            printf("%f", val->num_val);
                        }
                    } else if (val->type == VAL_STRING) {
                        printf("%s", val->str_val);
                    } else if (val->type == VAL_BOOL) {
                        printf("%s", val->bool_val ? "true" : "false");
                    } else if (val->type == VAL_NULL) {
                        printf("null");
                    } else if (val->type == VAL_ARRAY) {
                        printf("[");
                        for (int j = 0; j < val->array_len; j++) {
                            JSValue* elem = val->array_elems[j];
                            if (elem->type == VAL_NUMBER) {
                                if (elem->num_val == (int)elem->num_val) printf("%d", (int)elem->num_val);
                                else printf("%f", elem->num_val);
                            }
                            else if (elem->type == VAL_STRING) printf("\"%s\"", elem->str_val);
                            else if (elem->type == VAL_BOOL) printf("%s", elem->bool_val ? "true" : "false");
                            else printf("undefined");
                            if (j < val->array_len - 1) printf(", ");
                        }
                        printf("]");
                    } else {
                        printf("undefined");
                    }
                    if (i < node->child_count - 1) {
                        printf(" ");
                    }
                }
                printf("\n");
                return alloc_val(VAL_UNDEFINED);
            }

            // Math.abs built-in
            if (strcmp(node->str_val, "Math.abs") == 0) {
                JSValue* val = eval_node(node->children[0], env, returned);
                JSValue* res = alloc_val(VAL_NUMBER);
                res->num_val = fabs(val->num_val);
                return res;
            }

            // Math.pow built-in
            if (strcmp(node->str_val, "Math.pow") == 0) {
                JSValue* base = eval_node(node->children[0], env, returned);
                JSValue* exp = eval_node(node->children[1], env, returned);
                JSValue* res = alloc_val(VAL_NUMBER);
                res->num_val = pow(base->num_val, exp->num_val);
                return res;
            }

            // Standard function resolution
            JSValue* func = get_env_var(env, node->str_val);
            if (func->type != VAL_FUNCTION) {
                printf("TypeError: %s is not a function\n", node->str_val);
                return alloc_val(VAL_UNDEFINED);
            }

            ASTNode* decl = func->func_node;
            Env* call_env = create_env(func->func_env); // Lexical scoping environment parent!

            // Evaluate parameters and bind them to the local call environment
            for (int i = 0; i < node->child_count && i < decl->child_count; i++) {
                JSValue* arg_val = eval_node(node->children[i], env, returned);
                define_env_var(call_env, decl->children[i]->str_val, arg_val);
            }

            int ret_flag = 0;
            JSValue* ret_val = eval_node(decl->left, call_env, &ret_flag);
            return ret_val;
        }

        case NODE_RETURN: {
            JSValue* val = alloc_val(VAL_UNDEFINED);
            if (node->left) {
                val = eval_node(node->left, env, returned);
            }
            *returned = 1;
            return val;
        }

        case NODE_ARRAY_LIT: {
            JSValue* arr = alloc_val(VAL_ARRAY);
            arr->array_len = node->child_count;
            for (int i = 0; i < node->child_count; i++) {
                arr->array_elems[i] = eval_node(node->children[i], env, returned);
            }
            return arr;
        }

        case NODE_ARRAY_INDEX: {
            JSValue* arr = eval_node(node->left, env, returned);
            JSValue* idx = eval_node(node->right, env, returned);
            if (arr->type == VAL_ARRAY && idx->type == VAL_NUMBER) {
                int i = (int)idx->num_val;
                if (i >= 0 && i < arr->array_len) {
                    return arr->array_elems[i];
                }
            }
            return alloc_val(VAL_UNDEFINED);
        }
    }
    return alloc_val(VAL_UNDEFINED);
}

// === MAIN REPL / DRIVER ===

void print_val(JSValue* val) {
    if (val->type == VAL_NUMBER) {
        if (val->num_val == (int)val->num_val) {
            printf("%d\n", (int)val->num_val);
        } else {
            printf("%f\n", val->num_val);
        }
    } else if (val->type == VAL_STRING) {
        printf("'%s'\n", val->str_val);
    } else if (val->type == VAL_BOOL) {
        printf("%s\n", val->bool_val ? "true" : "false");
    } else if (val->type == VAL_NULL) {
        printf("null\n");
    } else if (val->type == VAL_ARRAY) {
        printf("[");
        for (int j = 0; j < val->array_len; j++) {
            JSValue* elem = val->array_elems[j];
            if (elem->type == VAL_NUMBER) {
                if (elem->num_val == (int)elem->num_val) printf("%d", (int)elem->num_val);
                else printf("%f", elem->num_val);
            }
            else if (elem->type == VAL_STRING) printf("'%s'", elem->str_val);
            else if (elem->type == VAL_BOOL) printf("%s", elem->bool_val ? "true" : "false");
            else printf("undefined");
            if (j < val->array_len - 1) printf(", ");
        }
        printf("]\n");
    } else if (val->type == VAL_FUNCTION) {
        printf("[Function: %s]\n", val->func_node->str_val);
    } else {
        printf("undefined\n");
    }
}

int count_braces(const char* str) {
    int open = 0;
    for (int i = 0; str[i]; i++) {
        if (str[i] == '{' || str[i] == '(' || str[i] == '[') open++;
        if (str[i] == '}' || str[i] == ')' || str[i] == ']') open--;
    }
    return open;
}

int main(int argc, char** argv) {
    // Initialize Global Scope
    Env* global = create_env(NULL);

    if (argc > 1) {
        // Run Script File Mode
        FILE* f = fopen(argv[1], "rb");
        if (!f) {
            printf("Error: Could not open file %s\n", argv[1]);
            return 1;
        }

        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);

        char* code = malloc(size + 1);
        fread(code, 1, size, f);
        code[size] = '\0';
        fclose(f);

        lex_js(code);
        ASTNode* root = parse_program();
        int returned = 0;
        eval_node(root, global, &returned);
        free(code);
        return 0;
    }

    // REPL Interactive Mode
    printf("🔱 ZKAEDI JAVASCRIPT ENGINE (ZJS) compiled via ZCC\n");
    printf("Type 'exit()' to quit.\n\n");

    char buffer[4096] = {0};
    char line[1024];

    while (1) {
        if (buffer[0] == '\0') {
            printf(">>> ");
        } else {
            printf("... ");
        }

        if (!fgets(line, sizeof(line), stdin)) {
            break;
        }

        // Check exit()
        if (strcmp(line, "exit()\n") == 0 || strcmp(line, "exit()\r\n") == 0) {
            break;
        }

        strcat(buffer, line);

        int balance = count_braces(buffer);
        if (balance <= 0) {
            // Evaluated when braces are fully balanced
            lex_js(buffer);
            buffer[0] = '\0'; // reset buffer

            if (token_count > 1) { // 1 for EOF
                ASTNode* root = parse_program();
                int returned = 0;
                // No value pool reset here to preserve global variable pointers in REPL mode
                JSValue* val = eval_node(root, global, &returned);
                // Avoid printing undefined for variable assignments and print loops
                if (root->child_count > 0) {
                    ASTNode* last_node = root->children[root->child_count - 1];
                    if (last_node->type != NODE_VAR_DECL &&
                        (last_node->type != NODE_CALL || strcmp(last_node->str_val, "console.log") != 0)) {
                        print_val(val);
                    }
                }
            }
        }
    }

    return 0;
}
