/* ================================================================ */
/* ZCC Modern C11 & C23 Conformance Engine (LIMIT-002)               */
/* _Atomic(type-name) & C23 [[...]] Attribute Parsing               */
/* ================================================================ */

#ifndef ZCC_C11_C23_H
#define ZCC_C11_C23_H

#include <stdio.h>
#include <string.h>

/*
 * C23 Attribute Specifier Sequence Skipper:
 * Entry: current token is first '[' and lookahead peek is '['.
 * Exit:  current token is first token after matching outer ']]'.
 * Invariant: Balances (), [], and {} inside attribute argument sequences.
 * Rejects unclosed attributes at EOF, unbalanced delimiters, and invalid tokens.
 */
static void skip_c23_attributes(Compiler *cc) {
    while (cc->tk == TK_LBRACKET && peek_token(cc) == TK_LBRACKET) {
        next_token(cc); /* consume first '[' */
        next_token(cc); /* consume second '[' */

        int paren_depth = 0;
        int brace_depth = 0;
        int bracket_depth = 0;
        int max_steps = 10000;

        while (cc->tk != TK_EOF && max_steps-- > 0) {
            if (paren_depth == 0) {
                if (cc->tk == TK_RBRACKET) {
                    next_token(cc); /* consume first ']' */
                    if (cc->tk == TK_RBRACKET) {
                        next_token(cc); /* consume second ']' */
                        break; /* attribute successfully closed */
                    } else {
                        error(cc, "malformed C23 attribute: expected second ']' to close attribute-specifier");
                        break;
                    }
                } else if (cc->tk == TK_LBRACKET) {
                    error(cc, "malformed C23 attribute: unexpected '[' inside attribute-specifier");
                    break;
                } else if (cc->tk == TK_LBRACE) {
                    error(cc, "malformed C23 attribute: unexpected '{' inside attribute-specifier");
                    break;
                } else if (cc->tk == TK_LPAREN) {
                    paren_depth++;
                    next_token(cc);
                } else if (cc->tk == TK_RPAREN) {
                    error(cc, "unbalanced ')' in C23 attribute-specifier");
                    break;
                } else {
                    next_token(cc);
                }
            } else {
                /* Inside attribute-argument-clause: ( balanced-token-seq ) */
                if (cc->tk == TK_LPAREN) {
                    paren_depth++;
                    next_token(cc);
                } else if (cc->tk == TK_RPAREN) {
                    paren_depth--;
                    next_token(cc);
                } else if (cc->tk == TK_LBRACKET) {
                    bracket_depth++;
                    next_token(cc);
                } else if (cc->tk == TK_RBRACKET) {
                    if (bracket_depth > 0) {
                        bracket_depth--;
                        next_token(cc);
                    } else {
                        error(cc, "unbalanced ']' inside C23 attribute argument");
                        break;
                    }
                } else if (cc->tk == TK_LBRACE) {
                    brace_depth++;
                    next_token(cc);
                } else if (cc->tk == TK_RBRACE) {
                    if (brace_depth > 0) {
                        brace_depth--;
                        next_token(cc);
                    } else {
                        error(cc, "unbalanced '}' inside C23 attribute argument");
                        break;
                    }
                } else {
                    next_token(cc);
                }
            }
        }

        if (cc->tk == TK_EOF) {
            error(cc, "unexpected end of file inside C23 attribute-specifier");
            break;
        }
        if (max_steps <= 0) {
            error(cc, "C23 attribute argument nesting exceeded limit or unclosed");
            break;
        }
        if (paren_depth != 0 || brace_depth != 0 || bracket_depth != 0) {
            error(cc, "unbalanced delimiters inside C23 attribute argument");
            break;
        }
    }
}

/*
 * C11 _Atomic(type-name) Parser:
 * Entry: current token is '(' after _Atomic keyword.
 * Requires an abstract declarator (rejects identifiers).
 * Enforces C11 §6.7.2.4p3 constraints: rejects array types and function types.
 * Milestone Scope: Syntax and type-system compatibility (ATOMIC_SEMANTICS: NOT_CLAIMED).
 */
static Type *parse_atomic_type_name(Compiler *cc) {
    expect(cc, TK_LPAREN);
    Type *base = parse_type(cc);
    char dummy[MAX_IDENT];
    dummy[0] = 0;
    Type *t = parse_declarator(cc, base, dummy);

    if (dummy[0] != 0) {
        error(cc, "identifier not allowed in _Atomic type-name (must be an abstract declarator)");
    }
    if (t) {
        if (t->kind == TY_ARRAY) {
            error(cc, "_Atomic cannot be applied to an array type (C11 6.7.2.4p3)");
        } else if (t->kind == TY_FUNC) {
            error(cc, "_Atomic cannot be applied to a function type (C11 6.7.2.4p3)");
        }
    }
    expect(cc, TK_RPAREN);

    /* Allocate atomic type with volatile qualification for syntax compatibility */
    if (t) {
        Type *at = type_new(cc, t->kind);
        *at = *t;
        at->is_volatile = 1;
        return at;
    }
    return cc->ty_int;
}

#endif /* ZCC_C11_C23_H */
