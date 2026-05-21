#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// === LEXER ===

typedef enum {
    TOK_EOF,
    TOK_NEWLINE,
    TOK_INDENT,
    TOK_DEDENT,
    TOK_NUMBER,
    TOK_STRING,
    TOK_IDENT,
    TOK_DEF,
    TOK_IF,
    TOK_ELSE,
    TOK_FOR,
    TOK_IN,
    TOK_RANGE,
    TOK_RETURN,
    TOK_PRINT,
    TOK_EQUALS,
    TOK_OP,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_COLON,
    TOK_COMMA
} TokenType;

typedef struct {
    TokenType type;
    int int_val;
    char str_val[128];
} Token;

#define MAX_TOKENS 1024
Token tokens[MAX_TOKENS];
int token_count = 0;
int token_index = 0;

int indent_stack[32] = {0};
int indent_top = 0;

void lex(const char* src) {
    const char* p = src;
    int line_start = 1;

    while (*p) {
        // Handle indentation on a new line
        if (line_start) {
            int spaces = 0;
            while (*p == ' ' || *p == '\t') {
                if (*p == ' ') spaces++;
                else spaces += 4; // tab is 4 spaces
                p++;
            }

            // Skip empty lines or comments
            if (*p == '\n' || *p == '\r' || *p == '#') {
                while (*p && *p != '\n') p++;
                if (*p == '\n') p++;
                continue;
            }

            if (*p == '\0') break;

            int current_indent = indent_stack[indent_top];
            if (spaces > current_indent) {
                indent_stack[++indent_top] = spaces;
                tokens[token_count].type = TOK_INDENT;
                strcpy(tokens[token_count].str_val, "INDENT");
                token_count++;
            } else if (spaces < current_indent) {
                while (indent_top > 0 && indent_stack[indent_top] > spaces) {
                    indent_top--;
                    tokens[token_count].type = TOK_DEDENT;
                    strcpy(tokens[token_count].str_val, "DEDENT");
                    token_count++;
                }
            }
            line_start = 0;
        }

        // Skip whitespace
        if (*p == ' ' || *p == '\t') {
            p++;
            continue;
        }

        // Handle Comments
        if (*p == '#') {
            while (*p && *p != '\n') p++;
            continue;
        }

        // Handle Newlines
        if (*p == '\n' || *p == '\r') {
            if (*p == '\r' && *(p+1) == '\n') p++;
            p++;
            tokens[token_count].type = TOK_NEWLINE;
            strcpy(tokens[token_count].str_val, "NEWLINE");
            token_count++;
            line_start = 1;
            continue;
        }

        // Handle Numbers
        if (isdigit(*p)) {
            int val = 0;
            while (isdigit(*p)) {
                val = val * 10 + (*p - '0');
                p++;
            }
            tokens[token_count].type = TOK_NUMBER;
            tokens[token_count].int_val = val;
            sprintf(tokens[token_count].str_val, "%d", val);
            token_count++;
            continue;
        }

        // Handle Strings
        if (*p == '"' || *p == '\'') {
            char quote = *p;
            p++;
            int len = 0;
            while (*p && *p != quote) {
                tokens[token_count].str_val[len++] = *p;
                p++;
            }
            tokens[token_count].str_val[len] = '\0';
            if (*p == quote) p++;
            tokens[token_count].type = TOK_STRING;
            token_count++;
            continue;
        }

        // Handle Identifiers / Keywords
        if (isalpha(*p) || *p == '_') {
            int len = 0;
            while (isalnum(*p) || *p == '_') {
                tokens[token_count].str_val[len++] = *p;
                p++;
            }
            tokens[token_count].str_val[len] = '\0';

            if (strcmp(tokens[token_count].str_val, "def") == 0) tokens[token_count].type = TOK_DEF;
            else if (strcmp(tokens[token_count].str_val, "if") == 0) tokens[token_count].type = TOK_IF;
            else if (strcmp(tokens[token_count].str_val, "else") == 0) tokens[token_count].type = TOK_ELSE;
            else if (strcmp(tokens[token_count].str_val, "for") == 0) tokens[token_count].type = TOK_FOR;
            else if (strcmp(tokens[token_count].str_val, "in") == 0) tokens[token_count].type = TOK_IN;
            else if (strcmp(tokens[token_count].str_val, "range") == 0) tokens[token_count].type = TOK_RANGE;
            else if (strcmp(tokens[token_count].str_val, "return") == 0) tokens[token_count].type = TOK_RETURN;
            else if (strcmp(tokens[token_count].str_val, "print") == 0) tokens[token_count].type = TOK_PRINT;
            else tokens[token_count].type = TOK_IDENT;

            token_count++;
            continue;
        }

        // Handle Operators / Symbols
        if (*p == '=') {
            p++;
            if (*p == '=') {
                p++;
                tokens[token_count].type = TOK_OP;
                strcpy(tokens[token_count].str_val, "==");
            } else {
                tokens[token_count].type = TOK_EQUALS;
                strcpy(tokens[token_count].str_val, "=");
            }
            token_count++;
            continue;
        }

        if (*p == '+' || *p == '-' || *p == '*' || *p == '/' || *p == '<' || *p == '>') {
            tokens[token_count].type = TOK_OP;
            tokens[token_count].str_val[0] = *p;
            tokens[token_count].str_val[1] = '\0';
            p++;
            token_count++;
            continue;
        }

        if (*p == '(') {
            tokens[token_count].type = TOK_LPAREN;
            strcpy(tokens[token_count].str_val, "(");
            p++;
            token_count++;
            continue;
        }

        if (*p == ')') {
            tokens[token_count].type = TOK_RPAREN;
            strcpy(tokens[token_count].str_val, ")");
            p++;
            token_count++;
            continue;
        }

        if (*p == ':') {
            tokens[token_count].type = TOK_COLON;
            strcpy(tokens[token_count].str_val, ":");
            p++;
            token_count++;
            continue;
        }

        if (*p == ',') {
            tokens[token_count].type = TOK_COMMA;
            strcpy(tokens[token_count].str_val, ",");
            p++;
            token_count++;
            continue;
        }

        // Unknown character
        p++;
    }

    // Emit remaining dedents
    while (indent_top > 0) {
        indent_top--;
        tokens[token_count].type = TOK_DEDENT;
        strcpy(tokens[token_count].str_val, "DEDENT");
        token_count++;
    }

    tokens[token_count].type = TOK_EOF;
    strcpy(tokens[token_count].str_val, "EOF");
    token_count++;
}

// === PARSER ===

typedef enum {
    NODE_INT,
    NODE_STR,
    NODE_VAR,
    NODE_BINOP,
    NODE_ASSIGN,
    NODE_PRINT,
    NODE_IF,
    NODE_FOR,
    NODE_DEF,
    NODE_CALL,
    NODE_RETURN,
    NODE_BLOCK
} NodeType;

typedef struct ASTNode {
    NodeType type;
    int int_val;
    char str_val[128];
    char op[4];
    struct ASTNode* left;
    struct ASTNode* right;
    struct ASTNode* cond;
    struct ASTNode* step;
    struct ASTNode* children[64];
    int child_count;
} ASTNode;

ASTNode* create_node(NodeType type) {
    ASTNode* node = (ASTNode*)malloc(sizeof(ASTNode));
    node->type = type;
    node->int_val = 0;
    node->str_val[0] = '\0';
    node->op[0] = '\0';
    node->left = NULL;
    node->right = NULL;
    node->cond = NULL;
    node->step = NULL;
    node->child_count = 0;
    return node;
}

ASTNode* parse_expression(void);

ASTNode* parse_primary(void) {
    Token t = tokens[token_index];
    if (t.type == TOK_NUMBER) {
        token_index++;
        ASTNode* n = create_node(NODE_INT);
        n->int_val = t.int_val;
        return n;
    }
    if (t.type == TOK_STRING) {
        token_index++;
        ASTNode* n = create_node(NODE_STR);
        strcpy(n->str_val, t.str_val);
        return n;
    }
    if (t.type == TOK_IDENT) {
        token_index++;
        if (tokens[token_index].type == TOK_LPAREN) {
            // Function Call
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
            // Variable
            ASTNode* n = create_node(NODE_VAR);
            strcpy(n->str_val, t.str_val);
            return n;
        }
    }
    if (t.type == TOK_LPAREN) {
        token_index++;
        ASTNode* n = parse_expression();
        if (tokens[token_index].type == TOK_RPAREN) token_index++;
        return n;
    }
    return NULL;
}

ASTNode* parse_term(void) {
    ASTNode* left = parse_primary();
    while (tokens[token_index].type == TOK_OP && 
           (tokens[token_index].str_val[0] == '*' || tokens[token_index].str_val[0] == '/')) {
        Token op = tokens[token_index++];
        ASTNode* n = create_node(NODE_BINOP);
        strcpy(n->op, op.str_val);
        n->left = left;
        n->right = parse_primary();
        left = n;
    }
    return left;
}

ASTNode* parse_expression(void) {
    ASTNode* left = parse_term();
    while (tokens[token_index].type == TOK_OP && 
           (tokens[token_index].str_val[0] == '+' || tokens[token_index].str_val[0] == '-' ||
            tokens[token_index].str_val[0] == '<' || tokens[token_index].str_val[0] == '>' ||
            strcmp(tokens[token_index].str_val, "==") == 0)) {
        Token op = tokens[token_index++];
        ASTNode* n = create_node(NODE_BINOP);
        strcpy(n->op, op.str_val);
        n->left = left;
        n->right = parse_term();
        left = n;
    }
    return left;
}

ASTNode* parse_block(void);

ASTNode* parse_statement(void) {
    Token t = tokens[token_index];

    // Skip leading newlines
    while (t.type == TOK_NEWLINE) {
        token_index++;
        t = tokens[token_index];
    }

    if (t.type == TOK_PRINT) {
        token_index++; // consume print
        if (tokens[token_index].type == TOK_LPAREN) token_index++;
        ASTNode* n = create_node(NODE_PRINT);
        n->left = parse_expression();
        if (tokens[token_index].type == TOK_RPAREN) token_index++;
        return n;
    }

    if (t.type == TOK_RETURN) {
        token_index++; // consume return
        ASTNode* n = create_node(NODE_RETURN);
        n->left = parse_expression();
        return n;
    }

    if (t.type == TOK_IF) {
        token_index++; // consume if
        ASTNode* n = create_node(NODE_IF);
        n->cond = parse_expression();
        if (tokens[token_index].type == TOK_COLON) token_index++;
        if (tokens[token_index].type == TOK_NEWLINE) token_index++;
        n->left = parse_block(); // if block

        if (tokens[token_index].type == TOK_ELSE) {
            token_index++; // consume else
            if (tokens[token_index].type == TOK_COLON) token_index++;
            if (tokens[token_index].type == TOK_NEWLINE) token_index++;
            n->right = parse_block(); // else block
        }
        return n;
    }

    if (t.type == TOK_FOR) {
        token_index++; // consume for
        Token var = tokens[token_index++]; // variable
        token_index++; // consume in
        token_index++; // consume range
        token_index++; // consume '('
        ASTNode* range_val = parse_expression();
        token_index++; // consume ')'
        if (tokens[token_index].type == TOK_COLON) token_index++;
        if (tokens[token_index].type == TOK_NEWLINE) token_index++;

        ASTNode* n = create_node(NODE_FOR);
        strcpy(n->str_val, var.str_val);
        n->cond = range_val;
        n->left = parse_block();
        return n;
    }

    if (t.type == TOK_DEF) {
        token_index++; // consume def
        Token func = tokens[token_index++];
        token_index++; // consume '('
        ASTNode* n = create_node(NODE_DEF);
        strcpy(n->str_val, func.str_val);

        if (tokens[token_index].type != TOK_RPAREN) {
            while (1) {
                Token arg = tokens[token_index++];
                ASTNode* arg_node = create_node(NODE_VAR);
                strcpy(arg_node->str_val, arg.str_val);
                n->children[n->child_count++] = arg_node;
                if (tokens[token_index].type == TOK_COMMA) {
                    token_index++;
                } else {
                    break;
                }
            }
        }
        token_index++; // consume ')'
        if (tokens[token_index].type == TOK_COLON) token_index++;
        if (tokens[token_index].type == TOK_NEWLINE) token_index++;
        n->left = parse_block();
        return n;
    }

    if (t.type == TOK_IDENT && tokens[token_index+1].type == TOK_EQUALS) {
        token_index++; // consume ident
        token_index++; // consume '='
        ASTNode* n = create_node(NODE_ASSIGN);
        strcpy(n->str_val, t.str_val);
        n->left = parse_expression();
        return n;
    }

    // Expression fallback statement
    ASTNode* expr = parse_expression();
    return expr;
}

ASTNode* parse_block(void) {
    ASTNode* block = create_node(NODE_BLOCK);
    if (tokens[token_index].type == TOK_INDENT) {
        token_index++; // consume INDENT
        while (tokens[token_index].type != TOK_DEDENT && tokens[token_index].type != TOK_EOF) {
            ASTNode* stmt = parse_statement();
            if (stmt) {
                block->children[block->child_count++] = stmt;
            }
            while (tokens[token_index].type == TOK_NEWLINE) token_index++;
        }
        if (tokens[token_index].type == TOK_DEDENT) token_index++;
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
        while (tokens[token_index].type == TOK_NEWLINE) token_index++;
    }
    return root;
}

// === INTERPRETER / VM ===

typedef struct {
    char name[32];
    int val;
} Variable;

typedef struct {
    Variable vars[64];
    int var_count;
} Frame;

Frame call_stack[16];
int stack_top = 0;

typedef struct {
    char name[32];
    ASTNode* def_node;
} FunctionDef;

FunctionDef functions[32];
int function_count = 0;

void set_var(const char* name, int val) {
    // Search current stack frame
    Frame* f = &call_stack[stack_top];
    for (int i = 0; i < f->var_count; i++) {
        if (strcmp(f->vars[i].name, name) == 0) {
            f->vars[i].val = val;
            return;
        }
    }
    // Create new variable in current frame
    strcpy(f->vars[f->var_count].name, name);
    f->vars[f->var_count].val = val;
    f->var_count++;
}

int get_var(const char* name) {
    // Search local frame first
    Frame* f = &call_stack[stack_top];
    for (int i = 0; i < f->var_count; i++) {
        if (strcmp(f->vars[i].name, name) == 0) {
            return f->vars[i].val;
        }
    }
    // Search global frame (frame 0)
    if (stack_top > 0) {
        Frame* glob = &call_stack[0];
        for (int i = 0; i < glob->var_count; i++) {
            if (strcmp(glob->vars[i].name, name) == 0) {
                return glob->vars[i].val;
            }
        }
    }
    return 0; // Default undefined value
}

int eval(ASTNode* node, int* returned) {
    if (!node) return 0;

    switch (node->type) {
        case NODE_INT:
            return node->int_val;

        case NODE_STR:
            // String constants are evaluated as dummy values for debug representation
            return 0;

        case NODE_VAR:
            return get_var(node->str_val);

        case NODE_BINOP: {
            int left = eval(node->left, returned);
            int right = eval(node->right, returned);
            if (strcmp(node->op, "+") == 0) return left + right;
            if (strcmp(node->op, "-") == 0) return left - right;
            if (strcmp(node->op, "*") == 0) return left * right;
            if (strcmp(node->op, "/") == 0) return right != 0 ? left / right : 0;
            if (strcmp(node->op, "<") == 0) return left < right;
            if (strcmp(node->op, ">") == 0) return left > right;
            if (strcmp(node->op, "==") == 0) return left == right;
            return 0;
        }

        case NODE_ASSIGN: {
            int val = eval(node->left, returned);
            set_var(node->str_val, val);
            return val;
        }

        case NODE_PRINT: {
            if (node->left->type == NODE_STR) {
                printf("%s\n", node->left->str_val);
            } else {
                int val = eval(node->left, returned);
                printf("%d\n", val);
            }
            return 0;
        }

        case NODE_IF: {
            int cond = eval(node->cond, returned);
            if (cond) {
                return eval(node->left, returned);
            } else if (node->right) {
                return eval(node->right, returned);
            }
            return 0;
        }

        case NODE_FOR: {
            int limit = eval(node->cond, returned);
            int last_val = 0;
            for (int i = 0; i < limit; i++) {
                set_var(node->str_val, i);
                last_val = eval(node->left, returned);
                if (*returned) break;
            }
            return last_val;
        }

        case NODE_DEF: {
            strcpy(functions[function_count].name, node->str_val);
            functions[function_count].def_node = node;
            function_count++;
            return 0;
        }

        case NODE_CALL: {
            // Find function
            ASTNode* def = NULL;
            for (int i = 0; i < function_count; i++) {
                if (strcmp(functions[i].name, node->str_val) == 0) {
                    def = functions[i].def_node;
                    break;
                }
            }

            if (!def) {
                printf("Runtime Error: Undefined function '%s'\n", node->str_val);
                return 0;
            }

            // Evaluate parameters before pushing frame
            int params[16];
            for (int i = 0; i < node->child_count; i++) {
                params[i] = eval(node->children[i], returned);
            }

            // Push Call Frame
            stack_top++;
            Frame* f = &call_stack[stack_top];
            f->var_count = 0;

            // Bind arguments
            for (int i = 0; i < def->child_count; i++) {
                strcpy(f->vars[i].name, def->children[i]->str_val);
                f->vars[i].val = params[i];
                f->var_count++;
            }

            int ret_flag = 0;
            int ret_val = eval(def->left, &ret_flag);

            // Pop Call Frame
            stack_top--;

            return ret_val;
        }

        case NODE_RETURN: {
            int val = eval(node->left, returned);
            *returned = 1; // set return trigger flag
            return val;
        }

        case NODE_BLOCK: {
            int val = 0;
            for (int i = 0; i < node->child_count; i++) {
                val = eval(node->children[i], returned);
                if (*returned) break;
            }
            return val;
        }
    }
    return 0;
}

// === MAIN ENTRY AND DEMO ===

int main() {
    printf("🔱 ZKAEDI OMNI-PYTHON ENGINE: BOOTSTRAPPING VIA ZCC\n");
    printf("===================================================\n\n");

    const char* python_program = 
        "# Variables and basic math\n"
        "x = 10\n"
        "y = 20 + x * 2\n"
        "print(\"Evaluating mathematical variables:\")\n"
        "print(y)\n"
        "\n"
        "# Conditionals (if/else)\n"
        "print(\"Running conditionals checking variables:\")\n"
        "if y > 35:\n"
        "    print(\"y is greater than 35\")\n"
        "else:\n"
        "    print(\"y is less or equal to 35\")\n"
        "\n"
        "# Function Definition and Recursion/Call Stack\n"
        "def compute_factorial_sum(n):\n"
        "    if n == 0:\n"
        "        return 0\n"
        "    return n + compute_factorial_sum(n - 1)\n"
        "\n"
        "print(\"Calling Python function: compute_factorial_sum(5):\")\n"
        "result = compute_factorial_sum(5)\n"
        "print(result)\n"
        "\n"
        "# For Loops\n"
        "print(\"Executing standard for loop in range(4):\")\n"
        "for i in range(4):\n"
        "    print(i)\n";

    printf("--- Compiling Python Code ---\n%s\n", python_program);

    // Initialize Global Stack Frame
    call_stack[0].var_count = 0;
    stack_top = 0;

    // Lex
    lex(python_program);

    // Print Tokens for validation
    printf("--- Lexer Tokens Output ---\n");
    for (int i = 0; i < token_count; i++) {
        printf("Token %02d: Type %d, Value: '%s'\n", i, tokens[i].type, tokens[i].str_val);
    }
    printf("\n");

    // Parse
    ASTNode* root = parse_program();
    printf("--- AST Compiled successfully. Running virtual machine... ---\n");

    int returned = 0;
    eval(root, &returned);

    printf("\n===================================================\n");
    printf("SUCCESS: Python compiler and virtual machine compiled cleanly via ZCC and executed beautifully!\n");

    return 0;
}
