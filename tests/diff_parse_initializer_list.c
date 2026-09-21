#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ========================================================================= */
/* ZCC AST & Token Mock Framework for parse_initializer_list                 */
/* ========================================================================= */

#define TK_EOF       0
#define TK_INT       1
#define TK_IDENT     2
#define TK_LBRACE    3
#define TK_RBRACE    4
#define TK_LBRACKET  5
#define TK_RBRACKET  6
#define TK_DOT       7
#define TK_ASSIGN    8
#define TK_COMMA     9

#define ND_INT       1
#define ND_IDENT     2
#define ND_INIT_LIST 3

#define MAX_IDENT    256

typedef struct Node {
    int kind;
    int val;
    char text[MAX_IDENT];
    char desig[MAX_IDENT];
    int desig_idx;
    int has_desig;
    int num_args;
    struct Node **args;
} Node;

typedef struct Token {
    int kind;
    int val;
    char text[MAX_IDENT];
} Token;

typedef struct MockCompiler {
    const Token *tokens;
    int num_tokens;
    int pos;
    int tk;
    char tk_text[MAX_IDENT];
    int tk_val;
} MockCompiler;

static void next_token(MockCompiler *cc) {
    if (cc->pos >= cc->num_tokens) {
        cc->tk = TK_EOF;
        cc->tk_text[0] = '\0';
        cc->tk_val = 0;
        return;
    }
    const Token *t = &cc->tokens[cc->pos++];
    cc->tk = t->kind;
    cc->tk_val = t->val;
    strncpy(cc->tk_text, t->text, MAX_IDENT - 1);
    cc->tk_text[MAX_IDENT - 1] = '\0';
}

static Node *node_new(int kind) {
    Node *n = (Node *)calloc(1, sizeof(Node));
    n->kind = kind;
    return n;
}

static Node *parse_assign_mock(MockCompiler *cc) {
    Node *n = node_new(cc->tk == TK_INT ? ND_INT : ND_IDENT);
    n->val = cc->tk_val;
    strncpy(n->text, cc->tk_text, MAX_IDENT - 1);
    next_token(cc);
    return n;
}

/* ========================================================================= */
/* 1. Original Implementation (Matching ZCC part3.c:3318-3390)               */
/* ========================================================================= */

static Node *parse_initializer_list_orig(MockCompiler *cc, int *out_count) {
    Node *list = node_new(ND_INIT_LIST);
    int cap = 16;
    list->args = (Node **)malloc(sizeof(Node *) * cap);
    list->num_args = 0;

    if (cc->tk == TK_LBRACE) {
        next_token(cc);
    }

    if (cc->tk == TK_RBRACE) {
        next_token(cc);
        if (out_count) *out_count = 0;
        return list;
    }

    while (cc->tk != TK_EOF) {
        Node *item = NULL;
        char desig_name[MAX_IDENT] = {0};
        int has_desig = 0;
        int desig_idx = -1;

        if (cc->tk == TK_LBRACE) {
            item = parse_initializer_list_orig(cc, NULL);
        } else if (cc->tk == TK_DOT) {
            while (cc->tk == TK_DOT) {
                next_token(cc);
                if (cc->tk == TK_IDENT) {
                    strncpy(desig_name, cc->tk_text, MAX_IDENT - 1);
                    has_desig = 1;
                    next_token(cc);
                }
            }
            if (cc->tk == TK_ASSIGN) {
                next_token(cc);
            }
            if (cc->tk == TK_LBRACE) {
                item = parse_initializer_list_orig(cc, NULL);
            } else {
                item = parse_assign_mock(cc);
            }
        } else if (cc->tk == TK_LBRACKET) {
            next_token(cc);
            desig_idx = cc->tk_val;
            parse_assign_mock(cc);
            if (cc->tk == TK_RBRACKET) next_token(cc);
            if (cc->tk == TK_ASSIGN) next_token(cc);
            if (cc->tk == TK_LBRACE) {
                item = parse_initializer_list_orig(cc, NULL);
            } else {
                item = parse_assign_mock(cc);
            }
        } else {
            item = parse_assign_mock(cc);
        }

        if (has_desig && item) {
            strncpy(item->desig, desig_name, MAX_IDENT - 1);
            item->has_desig = 1;
        }
        if (desig_idx >= 0 && item) {
            item->desig_idx = desig_idx;
        }

        if (list->num_args >= cap) {
            cap *= 2;
            list->args = (Node **)realloc(list->args, sizeof(Node *) * cap);
        }
        list->args[list->num_args++] = item;

        if (cc->tk == TK_COMMA) {
            next_token(cc);
            if (cc->tk == TK_RBRACE) {
                next_token(cc);
                break;
            }
        } else if (cc->tk == TK_RBRACE) {
            next_token(cc);
            break;
        } else {
            break;
        }
    }

    if (out_count) *out_count = list->num_args;
    return list;
}

/* ========================================================================= */
/* 2. Autonomous Superoptimized Metamorphic Implementation                   */
/*    - Elimination of 256-byte stack zeroing in loop body                   */
/*    - O(1) Pre-sized allocation (32 slots) avoiding realloc churn          */
/*    - Streamlined Token Dispatch Table                                     */
/* ========================================================================= */

static Node *parse_initializer_list_opt(MockCompiler *cc, int *out_count) {
    Node *list = node_new(ND_INIT_LIST);
    int cap = 32; /* Pre-allocated to cover typical C99 struct/array decls */
    list->args = (Node **)malloc(sizeof(Node *) * cap);
    list->num_args = 0;

    if (cc->tk == TK_LBRACE) {
        next_token(cc);
    }

    if (cc->tk == TK_RBRACE) {
        next_token(cc);
        if (out_count) *out_count = 0;
        return list;
    }

    while (cc->tk != TK_EOF) {
        Node *item = NULL;
        char desig_name[MAX_IDENT];
        desig_name[0] = '\0';
        int has_desig = 0;
        int desig_idx = -1;

        switch (cc->tk) {
            case TK_LBRACE:
                item = parse_initializer_list_opt(cc, NULL);
                break;

            case TK_DOT:
                while (cc->tk == TK_DOT) {
                    next_token(cc);
                    if (cc->tk == TK_IDENT) {
                        strncpy(desig_name, cc->tk_text, MAX_IDENT - 1);
                        desig_name[MAX_IDENT - 1] = '\0';
                        has_desig = 1;
                        next_token(cc);
                    }
                }
                if (cc->tk == TK_ASSIGN) next_token(cc);
                item = (cc->tk == TK_LBRACE) ? parse_initializer_list_opt(cc, NULL) : parse_assign_mock(cc);
                break;

            case TK_LBRACKET:
                next_token(cc);
                desig_idx = cc->tk_val;
                parse_assign_mock(cc);
                if (cc->tk == TK_RBRACKET) next_token(cc);
                if (cc->tk == TK_ASSIGN) next_token(cc);
                item = (cc->tk == TK_LBRACE) ? parse_initializer_list_opt(cc, NULL) : parse_assign_mock(cc);
                break;

            default:
                item = parse_assign_mock(cc);
                break;
        }

        if (has_desig && item) {
            strncpy(item->desig, desig_name, MAX_IDENT - 1);
            item->has_desig = 1;
        }
        if (desig_idx >= 0 && item) {
            item->desig_idx = desig_idx;
        }

        if (list->num_args >= cap) {
            cap *= 2;
            list->args = (Node **)realloc(list->args, sizeof(Node *) * cap);
        }
        list->args[list->num_args++] = item;

        if (cc->tk == TK_COMMA) {
            next_token(cc);
            if (cc->tk == TK_RBRACE) {
                next_token(cc);
                break;
            }
        } else if (cc->tk == TK_RBRACE) {
            next_token(cc);
            break;
        } else {
            break;
        }
    }

    if (out_count) *out_count = list->num_args;
    return list;
}

/* ========================================================================= */
/* 3. Deep Recursive AST Tree Comparator                                      */
/* ========================================================================= */

static int compare_ast_trees(const Node *a, const Node *b) {
    if (!a && !b) return 1;
    if (!a || !b) return 0;
    if (a->kind != b->kind) return 0;
    if (a->val != b->val) return 0;
    if (a->has_desig != b->has_desig) return 0;
    if (a->has_desig && strcmp(a->desig, b->desig) != 0) return 0;
    if (a->desig_idx != b->desig_idx) return 0;
    if (strcmp(a->text, b->text) != 0) return 0;
    if (a->num_args != b->num_args) return 0;

    for (int i = 0; i < a->num_args; i++) {
        if (!compare_ast_trees(a->args[i], b->args[i])) return 0;
    }
    return 1;
}

static void free_ast(Node *n) {
    if (!n) return;
    for (int i = 0; i < n->num_args; i++) free_ast(n->args[i]);
    if (n->args) free(n->args);
    free(n);
}

/* ========================================================================= */
/* 4. Differential Test Gauntlet Across Diverse C99 Initializer Programs     */
/* ========================================================================= */

int main(void) {
    printf("=================================================================\n");
    printf("🔱 ZCC DIFFERENTIAL ORACLE: parse_initializer_list (Superopt) 🔱\n");
    printf("Testing Invariant: Bit-exact recursive AST topology across initializers\n");
    printf("=================================================================\n");

    int mismatches = 0;
    int test_count = 0;

    /* Corpus 1: Flat Array Initializers { 10, 20, 30, 40 } */
    {
        Token stream[] = {
            {TK_LBRACE, 0, "{"},
            {TK_INT, 10, "10"}, {TK_COMMA, 0, ","},
            {TK_INT, 20, "20"}, {TK_COMMA, 0, ","},
            {TK_INT, 30, "30"}, {TK_COMMA, 0, ","},
            {TK_INT, 40, "40"},
            {TK_RBRACE, 0, "}"},
            {TK_EOF, 0, ""}
        };
        MockCompiler cc_orig = {stream, 10, 0, 0, "", 0};
        MockCompiler cc_opt  = {stream, 10, 0, 0, "", 0};
        next_token(&cc_orig);
        next_token(&cc_opt);

        int count_orig = 0, count_opt = 0;
        Node *tree_orig = parse_initializer_list_orig(&cc_orig, &count_orig);
        Node *tree_opt  = parse_initializer_list_opt(&cc_opt, &count_opt);

        test_count++;
        if (count_orig != count_opt || !compare_ast_trees(tree_orig, tree_opt)) {
            printf("[FAIL] Corpus 1 Flat array mismatch!\n");
            mismatches++;
        }
        free_ast(tree_orig); free_ast(tree_opt);
    }

    /* Corpus 2: C99 Designated Struct Members {.x = 100, .y = 200, .z = 300} */
    {
        Token stream[] = {
            {TK_LBRACE, 0, "{"},
            {TK_DOT, 0, "."}, {TK_IDENT, 0, "x"}, {TK_ASSIGN, 0, "="}, {TK_INT, 100, "100"}, {TK_COMMA, 0, ","},
            {TK_DOT, 0, "."}, {TK_IDENT, 0, "y"}, {TK_ASSIGN, 0, "="}, {TK_INT, 200, "200"}, {TK_COMMA, 0, ","},
            {TK_DOT, 0, "."}, {TK_IDENT, 0, "z"}, {TK_ASSIGN, 0, "="}, {TK_INT, 300, "300"},
            {TK_RBRACE, 0, "}"},
            {TK_EOF, 0, ""}
        };
        MockCompiler cc_orig = {stream, 16, 0, 0, "", 0};
        MockCompiler cc_opt  = {stream, 16, 0, 0, "", 0};
        next_token(&cc_orig);
        next_token(&cc_opt);

        int count_orig = 0, count_opt = 0;
        Node *tree_orig = parse_initializer_list_orig(&cc_orig, &count_orig);
        Node *tree_opt  = parse_initializer_list_opt(&cc_opt, &count_opt);

        test_count++;
        if (count_orig != count_opt || !compare_ast_trees(tree_orig, tree_opt)) {
            printf("[FAIL] Corpus 2 Designated struct mismatch!\n");
            mismatches++;
        }
        free_ast(tree_orig); free_ast(tree_opt);
    }

    /* Corpus 3: C99 Array Index Designators {[0] = 5, [2] = 15, [4] = 25} */
    {
        Token stream[] = {
            {TK_LBRACE, 0, "{"},
            {TK_LBRACKET, 0, "["}, {TK_INT, 0, "0"}, {TK_RBRACKET, 0, "]"}, {TK_ASSIGN, 0, "="}, {TK_INT, 5, "5"}, {TK_COMMA, 0, ","},
            {TK_LBRACKET, 0, "["}, {TK_INT, 2, "2"}, {TK_RBRACKET, 0, "]"}, {TK_ASSIGN, 0, "="}, {TK_INT, 15, "15"}, {TK_COMMA, 0, ","},
            {TK_LBRACKET, 0, "["}, {TK_INT, 4, "4"}, {TK_RBRACKET, 0, "]"}, {TK_ASSIGN, 0, "="}, {TK_INT, 25, "25"},
            {TK_RBRACE, 0, "}"},
            {TK_EOF, 0, ""}
        };
        MockCompiler cc_orig = {stream, 19, 0, 0, "", 0};
        MockCompiler cc_opt  = {stream, 19, 0, 0, "", 0};
        next_token(&cc_orig);
        next_token(&cc_opt);

        int count_orig = 0, count_opt = 0;
        Node *tree_orig = parse_initializer_list_orig(&cc_orig, &count_orig);
        Node *tree_opt  = parse_initializer_list_opt(&cc_opt, &count_opt);

        test_count++;
        if (count_orig != count_opt || !compare_ast_trees(tree_orig, tree_opt)) {
            printf("[FAIL] Corpus 3 Array index designators mismatch!\n");
            mismatches++;
        }
        free_ast(tree_orig); free_ast(tree_opt);
    }

    /* Corpus 4: Nested Composite Initializers {.pt = {1, 2}, .rgb = {255, 128, 64}} */
    {
        Token stream[] = {
            {TK_LBRACE, 0, "{"},
            {TK_DOT, 0, "."}, {TK_IDENT, 0, "pt"}, {TK_ASSIGN, 0, "="},
            {TK_LBRACE, 0, "{"}, {TK_INT, 1, "1"}, {TK_COMMA, 0, ","}, {TK_INT, 2, "2"}, {TK_RBRACE, 0, "}"},
            {TK_COMMA, 0, ","},
            {TK_DOT, 0, "."}, {TK_IDENT, 0, "rgb"}, {TK_ASSIGN, 0, "="},
            {TK_LBRACE, 0, "{"}, {TK_INT, 255, "255"}, {TK_COMMA, 0, ","}, {TK_INT, 128, "128"}, {TK_COMMA, 0, ","}, {TK_INT, 64, "64"}, {TK_RBRACE, 0, "}"},
            {TK_RBRACE, 0, "}"},
            {TK_EOF, 0, ""}
        };
        MockCompiler cc_orig = {stream, 23, 0, 0, "", 0};
        MockCompiler cc_opt  = {stream, 23, 0, 0, "", 0};
        next_token(&cc_orig);
        next_token(&cc_opt);

        int count_orig = 0, count_opt = 0;
        Node *tree_orig = parse_initializer_list_orig(&cc_orig, &count_orig);
        Node *tree_opt  = parse_initializer_list_opt(&cc_opt, &count_opt);

        test_count++;
        if (count_orig != count_opt || !compare_ast_trees(tree_orig, tree_opt)) {
            printf("[FAIL] Corpus 4 Nested composite mismatch!\n");
            mismatches++;
        }
        free_ast(tree_orig); free_ast(tree_opt);
    }

    /* Corpus 5: Empty Initializer List {} */
    {
        Token stream[] = {
            {TK_LBRACE, 0, "{"},
            {TK_RBRACE, 0, "}"},
            {TK_EOF, 0, ""}
        };
        MockCompiler cc_orig = {stream, 3, 0, 0, "", 0};
        MockCompiler cc_opt  = {stream, 3, 0, 0, "", 0};
        next_token(&cc_orig);
        next_token(&cc_opt);

        int count_orig = 0, count_opt = 0;
        Node *tree_orig = parse_initializer_list_orig(&cc_orig, &count_orig);
        Node *tree_opt  = parse_initializer_list_opt(&cc_opt, &count_opt);

        test_count++;
        if (count_orig != count_opt || !compare_ast_trees(tree_orig, tree_opt)) {
            printf("[FAIL] Corpus 5 Empty initializer mismatch!\n");
            mismatches++;
        }
        free_ast(tree_orig); free_ast(tree_opt);
    }

    printf("Test Suites Evaluated: %d\n", test_count);
    printf("Total Divergences:     %d\n", mismatches);

    if (mismatches == 0) {
        printf("=================================================================\n");
        printf("★ PARSE_INITIALIZER_LIST ORACLE: 100%% BIT-EXACT MATCH (PASS) ★\n");
        printf("Structural AST invariant confirmed; zero stack zeroing overhead.\n");
        printf("=================================================================\n");
        return 0;
    } else {
        printf("[!] Oracle rejected candidate: %d divergences.\n", mismatches);
        return 1;
    }
}
