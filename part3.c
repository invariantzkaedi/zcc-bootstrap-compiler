#ifndef ZCC_AST_BRIDGE_H
/* Exclusively for standalone IDE analysis */
#include "part1.c"
#endif

/* C99/POSIX support for curl graduation — see commit 262bd08 */
/* ================================================================ */
/* PARSER                                                            */
/* ================================================================ */

/* CG-HARDEN-SILENT-001: per-cap warning flags (one warning per cap per compilation) */
static int _warned_max_structs = 0;
static int _warned_max_params = 0;
static int _warned_max_strings = 0;
static int _warned_max_call_args = 0;
static int _warned_max_cases = 0;
static int _warned_max_init = 0;
static int _warned_max_globals = 0;

static int is_type_token(Compiler *cc) {
    if (cc->tk >= TK_INT && cc->tk <= TK_DOUBLE) return 1;
    if (cc->tk >= TK_STATIC && cc->tk <= TK_INLINE) return 1;
    if (cc->tk == TK_STRUCT) return 1;
    if (cc->tk == TK_UNION) return 1;
    if (cc->tk == TK_ENUM) return 1;
    if (cc->tk == TK_TYPEDEF) return 1;
    if (cc->tk == TK_TYPEOF) return 1;
    if (cc->tk == TK_AUTO_TYPE) return 1;
    if (cc->tk == TK_IDENT) {
        Symbol *sym;
        sym = scope_find(cc, cc->tk_text);
        if (sym) {
            if (sym->is_typedef) {
                if (getenv("ZCC_DEBUG")) printf("!!! %s is a TYPEDEF !!!\n", cc->tk_text);
                return 1;
            }
        }
    }
    return 0;
}

static Type *find_struct(Compiler *cc, char *tag) {
    int i;
    for (i = 0; i < cc->num_structs; i++) {
        if (strcmp(cc->structs[i]->tag, tag) == 0) return cc->structs[i];
    }
    return 0;
}

static void register_struct(Compiler *cc, Type *t) {
    if (cc->num_structs < MAX_STRUCTS) {
        cc->structs[cc->num_structs] = t;
        cc->num_structs++;
    } else {
        if (!_warned_max_structs) { printf("zcc: warning: MAX_STRUCTS (%d) exceeded at %s:%d — subsequent struct types silently discarded\n", MAX_STRUCTS, cc->filename ? cc->filename : "?", cc->tk_line); _warned_max_structs = 1; }
    }
}

/* ---------------------------------------------------------------- */
/* Parse struct/union body                                           */
/* ---------------------------------------------------------------- */

Type *parse_type(Compiler *cc);
Type *parse_declarator(Compiler *cc, Type *base, char *name_out);
static long long parse_const_expr_ternary(Compiler *cc);
static long long parse_const_expr_lor(Compiler *cc);
static long long parse_const_expr_land(Compiler *cc);
static long long parse_const_expr_bor(Compiler *cc);
static long long parse_const_expr_bxor(Compiler *cc);
static long long parse_const_expr_band(Compiler *cc);
static long long parse_const_expr_eq(Compiler *cc);
static long long parse_const_expr_rel(Compiler *cc);
static long long parse_const_expr_shift(Compiler *cc);
static long long parse_const_expr_add(Compiler *cc);
static long long parse_const_expr_mul(Compiler *cc);
static long long parse_const_expr_unary(Compiler *cc);
static long long parse_const_expr_primary(Compiler *cc);

static long long parse_const_expr(Compiler *cc) { return parse_const_expr_ternary(cc); }

static long long parse_const_expr_ternary(Compiler *cc) {
    long long val = parse_const_expr_lor(cc);
    if (cc->tk == TK_QUESTION) {
        next_token(cc);
        long long t_val = parse_const_expr(cc);
        expect(cc, TK_COLON);
        long long f_val = parse_const_expr_ternary(cc);
        return val ? t_val : f_val;
    }
    return val;
}
static long long parse_const_expr_lor(Compiler *cc) {
    long long val = parse_const_expr_land(cc);
    while (cc->tk == TK_LOR) { next_token(cc); long long rhs = parse_const_expr_land(cc); val = val || rhs; }
    return val;
}
static long long parse_const_expr_land(Compiler *cc) {
    long long val = parse_const_expr_bor(cc);
    while (cc->tk == TK_LAND) { next_token(cc); long long rhs = parse_const_expr_bor(cc); val = val && rhs; }
    return val;
}
static long long parse_const_expr_bor(Compiler *cc) {
    long long val = parse_const_expr_bxor(cc);
    while (cc->tk == TK_PIPE) { next_token(cc); val |= parse_const_expr_bxor(cc); }
    return val;
}
static long long parse_const_expr_bxor(Compiler *cc) {
    long long val = parse_const_expr_band(cc);
    while (cc->tk == TK_CARET) { next_token(cc); val ^= parse_const_expr_band(cc); }
    return val;
}
static long long parse_const_expr_band(Compiler *cc) {
    long long val = parse_const_expr_eq(cc);
    while (cc->tk == TK_AMP) { next_token(cc); val &= parse_const_expr_eq(cc); }
    return val;
}
static long long parse_const_expr_eq(Compiler *cc) {
    long long val = parse_const_expr_rel(cc);
    while (cc->tk == TK_EQ || cc->tk == TK_NE) {
        int tk = cc->tk; next_token(cc);
        if (tk == TK_EQ) val = (val == parse_const_expr_rel(cc));
        else val = (val != parse_const_expr_rel(cc));
    }
    return val;
}
static long long parse_const_expr_rel(Compiler *cc) {
    long long val = parse_const_expr_shift(cc);
    while (cc->tk == TK_LT || cc->tk == TK_GT || cc->tk == TK_LE || cc->tk == TK_GE) {
        int tk = cc->tk; next_token(cc);
        if (tk == TK_LT) val = (val < parse_const_expr_shift(cc));
        else if (tk == TK_GT) val = (val > parse_const_expr_shift(cc));
        else if (tk == TK_LE) val = (val <= parse_const_expr_shift(cc));
        else val = (val >= parse_const_expr_shift(cc));
    }
    return val;
}
static long long parse_const_expr_shift(Compiler *cc) {
    long long val = parse_const_expr_add(cc);
    while (cc->tk == TK_SHL || cc->tk == TK_SHR) {
        int tk = cc->tk; next_token(cc);
        if (tk == TK_SHL) val <<= parse_const_expr_add(cc);
        else val >>= parse_const_expr_add(cc);
    }
    return val;
}
static long long parse_const_expr_add(Compiler *cc) {
    long long val = parse_const_expr_mul(cc);
    while (cc->tk == TK_PLUS || cc->tk == TK_MINUS) {
        int tk = cc->tk; next_token(cc);
        if (tk == TK_PLUS) val += parse_const_expr_mul(cc);
        else val -= parse_const_expr_mul(cc);
    }
    return val;
}
static long long parse_const_expr_mul(Compiler *cc) {
    long long val = parse_const_expr_unary(cc);
    while (cc->tk == TK_STAR || cc->tk == TK_SLASH || cc->tk == TK_PERCENT) {
        int tk = cc->tk; next_token(cc);
        long long rhs = parse_const_expr_unary(cc);
        if (tk == TK_STAR) val *= rhs;
        else if (tk == TK_SLASH) { if (rhs) val /= rhs; }
        else if (tk == TK_PERCENT) { if (rhs) val %= rhs; }
    }
    return val;
}
static long long parse_const_expr_unary(Compiler *cc) {
    if (cc->tk == TK_LPAREN) {
        int is_cast = 0;
        int ptk = peek_token(cc);
        int curr_tk = cc->tk;
        char curr_txt[MAX_IDENT];
        strncpy(curr_txt, cc->tk_text, MAX_IDENT - 1);
        cc->tk = ptk;
        strncpy(cc->tk_text, cc->peek_text, MAX_IDENT - 1);
        if (is_type_token(cc)) is_cast = 1;
        cc->tk = curr_tk;
        strncpy(cc->tk_text, curr_txt, MAX_IDENT - 1);
        
        if (is_cast) {
            next_token(cc); /* consume ( */
            Type *st = parse_type(cc);
            char dummy[128];
            st = parse_declarator(cc, st, dummy);
            expect(cc, TK_RPAREN);
            return parse_const_expr_unary(cc);
        }
    }
    if (cc->tk == TK_MINUS) { next_token(cc); return -parse_const_expr_unary(cc); }
    if (cc->tk == TK_PLUS) { next_token(cc); return parse_const_expr_unary(cc); }
    if (cc->tk == TK_TILDE) { next_token(cc); return ~parse_const_expr_unary(cc); }
    if (cc->tk == TK_BANG) { next_token(cc); return !parse_const_expr_unary(cc); }
    if (cc->tk == TK_SIZEOF) {
        next_token(cc);
        if (cc->tk == TK_LPAREN) {
            next_token(cc);
            if (is_type_token(cc)) {
                Type *st = parse_type(cc);
                char dummy[128];
                st = parse_declarator(cc, st, dummy);
                expect(cc, TK_RPAREN);
                return type_size(st);
            }
            long long v = parse_const_expr(cc);
            expect(cc, TK_RPAREN);
            return v;
        } else {
            return 8;
        }
    }
    return parse_const_expr_primary(cc);
}
static long long parse_const_expr_primary(Compiler *cc) {
    if (cc->tk == TK_LPAREN) {
        long long val;
        next_token(cc);
        /* Handle casts: (int)expr, (unsigned long)expr, etc. */
        if (is_type_token(cc)) {
            /* Skip the type and closing paren, then evaluate the expression */
            while (cc->tk != TK_RPAREN && cc->tk != TK_EOF) next_token(cc);
            expect(cc, TK_RPAREN);
            return parse_const_expr_unary(cc);
        }
        val = parse_const_expr(cc);
        expect(cc, TK_RPAREN);
        return val;
    }
    if (cc->tk == TK_NUM) {
        long long val = cc->tk_val;
        next_token(cc);
        return val;
    }
    if (cc->tk == TK_IDENT) {
        Symbol *sym = scope_find(cc, cc->tk_text);
        if (sym && sym->is_enum_const) {
            long long val = sym->enum_val;
            next_token(cc);
            return val;
        }
        next_token(cc);
        return 0;
    }
    next_token(cc);
    return 0;
}

static Type *parse_struct_or_union(Compiler *cc, int is_union) {
    Type *stype;
    char tag[MAX_IDENT];
    int has_tag;

    has_tag = 0;
    tag[0] = 0;

    if (cc->tk == TK_IDENT) {
        strncpy(tag, cc->tk_text, MAX_IDENT - 1);
        has_tag = 1;
        next_token(cc);
    }

    /* forward reference or existing struct */
    if (cc->tk != TK_LBRACE) {
        if (has_tag) {
            stype = find_struct(cc, tag);
            if (stype) return stype;
            /* forward declaration */
        stype = type_new(cc, is_union ? TY_UNION : TY_STRUCT);
            strncpy(stype->tag, tag, MAX_IDENT - 1);
            stype->is_complete = 0;
            register_struct(cc, stype);
            return stype;
        }
        error(cc, "expected struct/union tag or body");
        return cc->ty_int;
    }

    /* check if already declared */
    stype = 0;
    if (has_tag) {
        stype = find_struct(cc, tag);
    }
    if (!stype) {
        stype = type_new(cc, is_union ? TY_UNION : TY_STRUCT);
        if (has_tag) {
            strncpy(stype->tag, tag, MAX_IDENT - 1);
            register_struct(cc, stype);
        }
        /* Consume pending attribute flags immediately on creation (ATTR-UNKNOWN-001)
         * Must happen here, not at the '{' boundary, to prevent leakage from
         * __attribute__ annotations inside #include'd system headers. */
        if (cc->pending_packed) {
            stype->is_packed = 1;
            cc->pending_packed = 0;
        }
        if (cc->pending_aligned_n) {
            stype->explicit_align = cc->pending_aligned_n;
            cc->pending_aligned_n = 0;
        }
    } else {
        /* Existing struct found — discard any stale pending flags */
        cc->pending_packed = 0;
        cc->pending_aligned_n = 0;
    }

    expect(cc, TK_LBRACE);

    {
        StructField *last_field;
        int offset;
        int max_size;
        int max_align;

        last_field = 0;
        offset = 0;
        max_size = 0;
        max_align = 1;

        while (cc->tk != TK_RBRACE) {
            Type *ftype;
            Type *base_ftype;
            char fname[MAX_IDENT];
            StructField *field;
            int falign;

            if (cc->tk == TK_EOF) break;

            base_ftype = parse_type(cc);
            ftype = parse_declarator(cc, base_ftype, fname);

            /* Ignore bitfield size since ZCC allocates full integers for them */
            if (cc->tk == TK_COLON) {
                next_token(cc);
                parse_const_expr(cc);
            }

            field = (StructField *)cc_alloc(cc, sizeof(StructField));
            strncpy(field->name, fname, MAX_IDENT - 1);
            field->type = ftype;

            falign = type_align(ftype);
            if (falign > max_align) max_align = falign;

            if (is_union) {
                field->offset = 0;
                if (type_size(ftype) > max_size) max_size = type_size(ftype);
            } else {
                /* align offset — skip when struct is packed (ATTR-UNKNOWN-001) */
                if (!stype->is_packed && falign > 1) {
                    offset = (offset + falign - 1) & ~(falign - 1);
                }
                field->offset = offset;
                offset = offset + type_size(ftype);
            }

            field->next = 0;
            if (last_field) {
                last_field->next = field;
            } else {
                stype->fields = field;
            }
            last_field = field;

            /* handle multiple declarators: int *next, *prev; */
            while (cc->tk == TK_COMMA) {
                Type *ftype2;
                char fname2[MAX_IDENT];
                StructField *field2;
                int falign2;
                next_token(cc);
                ftype2 = parse_declarator(cc, base_ftype, fname2);

                /* Ignore bitfield size since ZCC allocates full integers */
                if (cc->tk == TK_COLON) {
                    next_token(cc);
                    parse_const_expr(cc);
                }

                field2 = (StructField *)cc_alloc(cc, sizeof(StructField));
                strncpy(field2->name, fname2, MAX_IDENT - 1);
                field2->type = ftype2;
                falign2 = type_align(ftype2);
                if (falign2 > max_align) max_align = falign2;
                if (is_union) {
                    field2->offset = 0;
                    if (type_size(ftype2) > max_size) max_size = type_size(ftype2);
                } else {
                    /* skip alignment when packed (ATTR-UNKNOWN-001) */
                    if (!stype->is_packed && falign2 > 1) {
                        offset = (offset + falign2 - 1) & ~(falign2 - 1);
                    }
                    field2->offset = offset;
                    offset = offset + type_size(ftype2);
                }
                field2->next = 0;
                last_field->next = field2;
                last_field = field2;
            }

            expect(cc, TK_SEMI);
        }

        if (is_union) {
            stype->size = max_size;
        } else {
            stype->size = offset;
        }
        /* align total size — skip trailing padding when packed; apply explicit_align if set */
        if (stype->explicit_align > 0) {
            int ea = stype->explicit_align;
            stype->size = (stype->size + ea - 1) & ~(ea - 1);
            stype->align = ea;
        } else if (!stype->is_packed && max_align > 1) {
            stype->size = (stype->size + max_align - 1) & ~(max_align - 1);
            stype->align = max_align;
        } else {
            stype->align = stype->is_packed ? 1 : max_align;
        }
        stype->is_complete = 1;
    }

    expect(cc, TK_RBRACE);
    
    if (cc->debug_abi_classes && stype->is_complete) {
        abi_class_t eb[2];
        classify_aggregate(stype, eb);
        printf("ABI-CLASS: %s (size=%d, align=%d) -> lo:%d, hi:%d\n",
               stype->tag[0] ? stype->tag : "<anon>",
               stype->size, stype->align, (int)eb[0], (int)eb[1]);
    }

    if (stype->tag[0] && (strcmp(stype->tag, "yyStackEntry") == 0 || strcmp(stype->tag, "yyParser") == 0 || strcmp(stype->tag, "Walker") == 0)) {
        int n = 0;
        StructField *f = stype->fields;
        while(f) { n++; printf("FIELD: %s\n", f->name); f = f->next; }
        printf("DEBUG: Parsed %s with %d fields (Type=%p, fields=%p)\n", stype->tag, n, (void*)stype, (void*)stype->fields);
        fflush(stdout);
    }
    return stype;
}

/* ---------------------------------------------------------------- */
/* Parse enum                                                        */
/* ---------------------------------------------------------------- */

static Type *parse_enum_def(Compiler *cc) {
    char tag[MAX_IDENT];
    Type *etype;

    tag[0] = 0;
    if (cc->tk == TK_IDENT) {
        strncpy(tag, cc->tk_text, MAX_IDENT - 1);
        next_token(cc);
    }

    etype = type_new(cc, TY_ENUM);

    if (cc->tk == TK_LBRACE) {
        long val;
        next_token(cc);
        val = 0;
        while (cc->tk != TK_RBRACE) {
            char name[MAX_IDENT];
            Symbol *sym;

            if (cc->tk == TK_EOF) break;
            if (cc->tk != TK_IDENT) {
                error(cc, "expected enum constant name");
                next_token(cc);
                continue;
            }
            strncpy(name, cc->tk_text, MAX_IDENT - 1);
            next_token(cc);

            if (cc->tk == TK_ASSIGN) {
                next_token(cc);
                val = parse_const_expr(cc);
            }

            sym = scope_add(cc, name, etype);
            sym->is_enum_const = 1;
            sym->enum_val = val;
            val++;

            if (cc->tk == TK_COMMA) {
                next_token(cc);
            } else {
                break;
            }
        }
        expect(cc, TK_RBRACE);
    }
    return etype;
}

/* ---------------------------------------------------------------- */
/* Parse base type                                                   */
/* ---------------------------------------------------------------- */

Type *parse_type(Compiler *cc) {
    Type *type = 0;
    int is_unsigned = 0;
    int is_signed = 0;
    int is_long = 0;
    int is_short = 0;
    int is_int = 0;
    int is_char = 0;
    int is_double = 0;
    int is_float = 0;
    int is_void = 0;
    int is_typedef_kw = 0;
    int is_static = 0;
    int is_extern = 0;

    /* storage class / qualifiers / basic types */
    for (;;) {
        if (cc->tk == TK_STATIC) { is_static = 1; next_token(cc); }
        else if (cc->tk == TK_EXTERN) { is_extern = 1; next_token(cc); }
        else if (cc->tk == TK_CONST) { next_token(cc); }
        else if (cc->tk == TK_VOLATILE) { next_token(cc); }
        else if (cc->tk == TK_INLINE) { next_token(cc); }
        else if (cc->tk == TK_AUTO) { next_token(cc); }
        else if (cc->tk == TK_REGISTER) { next_token(cc); }
        else if (cc->tk == TK_TYPEDEF) { is_typedef_kw = 1; next_token(cc); }
        else if (cc->tk == TK_UNSIGNED) { is_unsigned = 1; next_token(cc); }
        else if (cc->tk == TK_SIGNED) { is_signed = 1; next_token(cc); }
        else if (cc->tk == TK_LONG) { is_long++; next_token(cc); }
        else if (cc->tk == TK_SHORT) { is_short = 1; next_token(cc); }
        else if (cc->tk == TK_INT) { is_int = 1; next_token(cc); }
        else if (cc->tk == TK_CHAR) { is_char = 1; next_token(cc); }
        else if (cc->tk == TK_DOUBLE) { is_double = 1; next_token(cc); }
        else if (cc->tk == TK_FLOAT) { is_float = 1; next_token(cc); }
        else if (cc->tk == TK_VOID) { is_void = 1; next_token(cc); }
        else break;
    }

    if (is_void) { type = cc->ty_void; }
    else if (is_float) { type = cc->ty_float; }
    else if (is_double) { type = cc->ty_double; }
    else if (is_char) {
        if (is_unsigned) { type = cc->ty_uchar; } else { type = cc->ty_char; }
    }
    else if (is_short) {
        if (is_unsigned) { type = cc->ty_ushort; } else { type = cc->ty_short; }
    }
    else if (is_long >= 2) {
        if (is_unsigned) { type = cc->ty_ulonglong; } else { type = cc->ty_longlong; }
    }
    else if (is_long == 1) {
        if (is_unsigned) { type = cc->ty_ulong; } else { type = cc->ty_long; }
    }
    else if (is_int || is_unsigned || is_signed) {
        if (is_unsigned) { type = cc->ty_uint; } else { type = cc->ty_int; }
    }

    if (!type) {
        if (cc->tk == TK_STRUCT) {
            next_token(cc);
            type = parse_struct_or_union(cc, 0);
        }
        else if (cc->tk == TK_UNION) {
            next_token(cc);
            type = parse_struct_or_union(cc, 1);
        }
        else if (cc->tk == TK_ENUM) {
            next_token(cc);
            type = parse_enum_def(cc);
        }
        else if (cc->tk == TK_TYPEOF) {
            next_token(cc);
            expect(cc, TK_LPAREN);
            Node *expr = parse_expr(cc);
            expect(cc, TK_RPAREN);
            type = expr->type;
            if (!type) type = cc->ty_int;
        }
        else if (cc->tk == TK_AUTO_TYPE) {
            next_token(cc);
            type = type_new(cc, TY_VOID);
            type->size = 0; /* placeholder */
        }
        else if (cc->tk == TK_IDENT) {
            /* could be a typedef name */
            Symbol *sym;
            sym = scope_find(cc, cc->tk_text);
            if (strcmp(cc->tk_text, "yyParser") == 0) {
                printf("DEBUG: parse_type lookup 'yyParser'. sym=%p\n", (void*)sym);
            }
            if (sym && sym->is_typedef) {
                type = sym->type;
                next_token(cc);
            }
        }
    }

    if (!type) {
        type = cc->ty_int;
    }

    /* skip trailing const/volatile (e.g. 'char const *') */
    while (cc->tk == TK_CONST || cc->tk == TK_VOLATILE) {
        next_token(cc);
    }

    cc->current_is_static = is_static;
    return type;
}

/* ---------------------------------------------------------------- */
/* Parse declarator (pointers, arrays, function params)              */
/* ---------------------------------------------------------------- */

Node *parse_assign(Compiler *cc);

static int eval_const_expr(Node *n) {
    if (!n) return 0;
    if (n->kind == ND_NUM) return (int)n->int_val;
    if (n->kind == ND_ADD) return eval_const_expr(n->lhs) + eval_const_expr(n->rhs);
    if (n->kind == ND_SUB) return eval_const_expr(n->lhs) - eval_const_expr(n->rhs);
    if (n->kind == ND_MUL) return eval_const_expr(n->lhs) * eval_const_expr(n->rhs);
    if (n->kind == ND_DIV) {
        int r = eval_const_expr(n->rhs);
        return r ? eval_const_expr(n->lhs) / r : 0;
    }
    if (n->kind == ND_MOD) {
        int r = eval_const_expr(n->rhs);
        return r ? eval_const_expr(n->lhs) % r : 0;
    }
    if (n->kind == ND_SHL) return eval_const_expr(n->lhs) << eval_const_expr(n->rhs);
    if (n->kind == ND_SHR) return eval_const_expr(n->lhs) >> eval_const_expr(n->rhs);
    if (n->kind == ND_BAND) return eval_const_expr(n->lhs) & eval_const_expr(n->rhs);
    if (n->kind == ND_BOR) return eval_const_expr(n->lhs) | eval_const_expr(n->rhs);
    if (n->kind == ND_BXOR) return eval_const_expr(n->lhs) ^ eval_const_expr(n->rhs);
    if (n->kind == ND_CAST) return eval_const_expr(n->lhs);
    /* Unary operators — critical for negative switch cases like case (-15): */
    if (n->kind == ND_NEG)  return -eval_const_expr(n->lhs);
    if (n->kind == ND_BNOT) return ~eval_const_expr(n->lhs);
    if (n->kind == ND_LNOT) return !eval_const_expr(n->lhs);
    /* Enum constants (global only — local vars are NOT constants) */
    if (n->kind == ND_VAR && n->sym && n->sym->is_enum_const && !n->sym->is_local)
        return n->sym->enum_val;
    return 0; /* Fallback for unsupported complex compile-time bounds */
}



static Type *inject_base_type(Compiler *cc, Type *t, Type *base) {
    if (!t || t == cc->ty_int) return base;
    if (t->kind == TY_PTR) {
        return type_ptr(cc, inject_base_type(cc, t->base, base));
    }
    if (t->kind == TY_ARRAY) {
        return type_array(cc, inject_base_type(cc, t->base, base), t->array_len);
    }
    if (t->kind == TY_FUNC) {
        Type *n = type_func(cc, inject_base_type(cc, t->ret, base));
        n->params = t->params;
        n->num_params = t->num_params;
        n->is_variadic = t->is_variadic;
        return n;
    }
    return base;
}

Type *parse_declarator(Compiler *cc, Type *base, char *name_out) {
    Type *type;

    type = base;
    name_out[0] = 0;

    /* pointers */
    while (cc->tk == TK_STAR) {
        next_token(cc);
        /* skip const/volatile after * */
        while (cc->tk == TK_CONST || cc->tk == TK_VOLATILE) {
            next_token(cc);
        }
        type = type_ptr(cc, type);
    }

    /* name */
    if (cc->tk == TK_IDENT) {
        strncpy(name_out, cc->tk_text, MAX_IDENT - 1);
        next_token(cc);
    }
    else if (cc->tk == TK_LPAREN) {
        /* grouped declarator: (*name)(params) or (*name)[N] */
        int pk;
        pk = peek_token(cc);
        if (pk == TK_STAR || pk == TK_IDENT || pk == TK_LPAREN) {
            Type *inner;
            next_token(cc); /* consume ( */
            inner = parse_declarator(cc, cc->ty_int, name_out);
            expect(cc, TK_RPAREN);
            /* process outer dimensions and function args first! */
            if (cc->tk == TK_LBRACKET) {
                int arr_lens[16];
                int arr_num = 0;
                while (cc->tk == TK_LBRACKET) {
                    int len = 0;
                    next_token(cc);
                    if (cc->tk != TK_RBRACKET) len = (int)parse_const_expr(cc);
                    expect(cc, TK_RBRACKET);
                    if (arr_num < 16) arr_lens[arr_num++] = len;
                }
                while (arr_num > 0) {
                    arr_num--;
                    type = type_array(cc, type, arr_lens[arr_num]);
                }
            }
            if (cc->tk == TK_LPAREN) {
                Type *ftype;
                next_token(cc);
                ftype = type_func(cc, type);
                ftype->params = (Type **)cc_alloc(cc, sizeof(Type *) * MAX_PARAMS);
                ftype->num_params = 0;
                ftype->is_variadic = 0;
                if (cc->tk != TK_RPAREN) {
                    if (cc->tk == TK_VOID) {
                        int vpk;
                        vpk = peek_token(cc);
                        if (vpk == TK_RPAREN) {
                            next_token(cc);
                        } else {
                            goto parse_params_inner;
                        }
                    } else {
                        parse_params_inner:
                        while (cc->tk != TK_RPAREN) {
                            Type *ptype;
                            char pname[MAX_IDENT];
                            if (cc->tk == TK_ELLIPSIS) {
                                ftype->is_variadic = 1;
                                next_token(cc);
                                break;
                            }
                            if (cc->tk == TK_EOF) break;
                            ptype = parse_type(cc);
                            ptype = parse_declarator(cc, ptype, pname);
                            if (ptype->kind == TY_ARRAY) ptype = type_ptr(cc, ptype->base);
                            if (ftype->num_params < MAX_PARAMS) {
                                ftype->params[ftype->num_params] = ptype;
                                ftype->num_params++;
                            } else {
                                if (!_warned_max_params) { printf("zcc: warning: MAX_PARAMS (%d) exceeded at %s:%d — subsequent params silently discarded\n", MAX_PARAMS, cc->filename ? cc->filename : "?", cc->tk_line); _warned_max_params = 1; }
                            }
                            if (cc->tk == TK_COMMA) {
                                next_token(cc);
                            } else {
                                break;
                            }
                        }
                    }
                }
                expect(cc, TK_RPAREN);
                type = ftype;
            }
            return inject_base_type(cc, inner, type);
        }
    }

    /* array dimensions */
    if (cc->tk == TK_LBRACKET) {
        int arr_lens[16];
        int arr_num = 0;
        while (cc->tk == TK_LBRACKET) {
            int len = 0;
            next_token(cc);
            if (cc->tk != TK_RBRACKET) len = (int)parse_const_expr(cc);
            expect(cc, TK_RBRACKET);
            if (arr_num < 16) arr_lens[arr_num++] = len;
        }
        while (arr_num > 0) {
            arr_num--;
            type = type_array(cc, type, arr_lens[arr_num]);
        }
    }

    /* function parameters */
    if (cc->tk == TK_LPAREN) {
        Type *ftype;
        next_token(cc);
        ftype = type_func(cc, type);
        ftype->params = (Type **)cc_alloc(cc, sizeof(Type *) * MAX_PARAMS);
        ftype->num_params = 0;
        ftype->is_variadic = 0;

        if (cc->tk != TK_RPAREN) {
            if (cc->tk == TK_VOID) {
                int pk;
                pk = peek_token(cc);
                if (pk == TK_RPAREN) {
                    next_token(cc); /* skip void */
                } else {
                    goto parse_params;
                }
            } else {
                parse_params:
                while (cc->tk != TK_RPAREN) {
                    Type *ptype;
                    char pname[MAX_IDENT];

                    if (cc->tk == TK_ELLIPSIS) {
                        ftype->is_variadic = 1;
                        next_token(cc);
                        break;
                    }
                    if (cc->tk == TK_EOF) break;

                    ptype = parse_type(cc);
                    ptype = parse_declarator(cc, ptype, pname);

                    if (ptype->kind == TY_ARRAY) ptype = type_ptr(cc, ptype->base);

                    if (ftype->num_params < MAX_PARAMS) {
                        ftype->params[ftype->num_params] = ptype;
                        ftype->num_params++;
                    } else {
                        if (!_warned_max_params) { printf("zcc: warning: MAX_PARAMS (%d) exceeded at %s:%d — subsequent params silently discarded\n", MAX_PARAMS, cc->filename ? cc->filename : "?", cc->tk_line); _warned_max_params = 1; }
                    }

                    if (cc->tk == TK_COMMA) {
                        next_token(cc);
                    } else {
                        break;
                    }
                }
            }
        }
        expect(cc, TK_RPAREN);
        type = ftype;
    }

    return type;
}

/* ================================================================ */
/* EXPRESSION PARSING — FULL PRECEDENCE CHAIN                       */
/* ================================================================ */

/* --- primary --- */

Node *parse_primary(Compiler *cc) {
    Node *n;
    int line;

    line = cc->tk_line;

    if (cc->tk == TK_NUM) {
        n = node_num(cc, cc->tk_val, line);
        if (cc->tk_text[0] == 'U') {
             if (cc->tk_val <= 4294967295ULL) {
                 n->type = cc->ty_uint;
             } else {
                 n->type = cc->ty_ulong;
             }
        }
        if (cc->tk_text[1] == 'L') {
             if (cc->tk_text[0] == 'U') {
                 n->type = cc->ty_ulong;
             } else {
                 n->type = cc->ty_long;
             }
        }
        next_token(cc);
        return n;
    }

    if (cc->tk == TK_FLIT) {
        /* CG-FLOAT-007: lexer sets tk_text[0]='F' for f/F suffix literals.
         * Value was already rounded to float precision by the lexer.
         * Use ty_float so the AST node carries the correct type. */
        int is_float_suffix = (cc->tk_text[0] == 'F');
        n = node_flit(cc, cc->tk_fval, line);
        if (is_float_suffix) n->type = cc->ty_float;
        next_token(cc);
        return n;
    }

    if (cc->tk == TK_CHAR_LIT) {
        n = node_num(cc, cc->tk_val, line);
        n->type = cc->ty_char;
        next_token(cc);
        return n;
    }

    if (cc->tk == TK_STR) {
        int sid;
        StringEntry *se;

        sid = cc->num_strings;
        if (sid < MAX_STRINGS) {
            se = &cc->strings[sid];
            se->data = cc_strdup(cc, cc->tk_str);
            se->len = cc->tk_str_len;
            se->label_id = cc->str_label_count;
            cc->str_label_count++;
            cc->num_strings++;
        } else {
            if (!_warned_max_strings) { printf("zcc: warning: MAX_STRINGS (%d) exceeded at %s:%d — subsequent string literals silently discarded\n", MAX_STRINGS, cc->filename ? cc->filename : "?", cc->tk_line); _warned_max_strings = 1; }
        }
        n = node_new(cc, ND_STR, line);
        n->str_id = sid;
        n->type = type_ptr(cc, cc->ty_char);
        next_token(cc);
        return n;
    }

    if (cc->tk == TK_IDENT) {
        Symbol *sym;
        n = node_new(cc, ND_VAR, line);
        strncpy(n->name, cc->tk_text, MAX_IDENT - 1);
        sym = scope_find(cc, cc->tk_text);
        
        if (strcmp(cc->tk_text, "yypParser") == 0 || strcmp(cc->tk_text, "pParser") == 0) {
            printf("DEBUG: parse_primary lookup '%s'. sym=%p, sym->type=%p, base=%p, base_tag='%s'\n", 
                   cc->tk_text,
                   (void*)sym, sym ? (void*)sym->type : NULL, 
                   (sym && sym->type) ? (void*)sym->type->base : NULL, 
                   (sym && sym->type && sym->type->base && sym->type->base->tag[0]) ? sym->type->base->tag : "<none>");
            fflush(stdout);
        }
        
        if (sym) {
            n->sym = sym;
            n->type = sym->type;
            if (sym->asm_name[0]) {
                strncpy(n->name, sym->asm_name, MAX_IDENT - 1);
            }
            if (sym->is_enum_const && !sym->is_local) {
                /* Only fold to ND_NUM if this is truly a global enum constant.
                 * A local variable (is_local=1) with the same name as an outer
                 * enum constant must NOT be constant-folded — it is a live
                 * stack variable that must be loaded at runtime. */
                n->kind = ND_NUM;
                n->int_val = sym->enum_val;
                n->type = cc->ty_int;
            }
        } else {
            /* stdio global pointers: stdin, stdout, stderr are FILE* (size 8) */
            if (strcmp(n->name, "stdin") == 0 || strcmp(n->name, "stdout") == 0 || strcmp(n->name, "stderr") == 0) {
                n->type = type_ptr(cc, cc->ty_char);
            } else {
                n->type = cc->ty_int;
            }
        }
        next_token(cc);
        return n;
    }

    if (cc->tk == TK_LPAREN) {
        next_token(cc);

        /* check for cast expression */
        if (is_type_token(cc)) {
            Type *cast_type;
            char dummy[MAX_IDENT];
            cast_type = parse_type(cc);
            cast_type = parse_declarator(cc, cast_type, dummy);
            expect(cc, TK_RPAREN);
            n = node_new(cc, ND_CAST, line);
            n->cast_type = cast_type;
            n->lhs = parse_unary(cc);
            n->type = cast_type;
            return n;
        }

        n = parse_expr(cc);
        expect(cc, TK_RPAREN);
        return n;
    }

    if (cc->tk == TK_BUILTIN_VA_ARG) {
        Node *n;
        Node *ap_node;
        Type *ret_type;
        char dummy[MAX_IDENT];
        int line;
        
        line = cc->tk_line;
        next_token(cc); /* skip va_arg */
        expect(cc, TK_LPAREN);
        ap_node = parse_assign(cc);
        expect(cc, TK_COMMA);
        ret_type = parse_type(cc);
        ret_type = parse_declarator(cc, ret_type, dummy);
        expect(cc, TK_RPAREN);

        n = node_new(cc, ND_VA_ARG, line);
        n->lhs = ap_node;
        n->type = ret_type;
        return n;
    }

    if (cc->tk == TK_SIZEOF) {
        int pk;
        int is_type_in_parens;
        next_token(cc);
        if (cc->tk == TK_LPAREN) {
            pk = peek_token(cc);
            is_type_in_parens = 0;
            if (pk >= TK_INT) {
                if (pk <= TK_INLINE || pk == TK_STRUCT ||
                    pk == TK_UNION || pk == TK_ENUM) {
                    is_type_in_parens = 1;
                }
            }
            if (!is_type_in_parens && pk == TK_IDENT) {
                Symbol *sz_sym;
                sz_sym = scope_find(cc, cc->peek_text);
                if (sz_sym && sz_sym->is_typedef) is_type_in_parens = 1;
            }
            if (is_type_in_parens) {
                Type *st;
                char dummy[MAX_IDENT];
                next_token(cc);  /* skip ( */
                st = parse_type(cc);
                st = parse_declarator(cc, st, dummy);
                expect(cc, TK_RPAREN);
                n = node_num(cc, type_size(st), line);
                return n;
            }
            /* sizeof(expr) */
            {
                Node *expr;
                next_token(cc); /* skip ( */
                expr = parse_unary(cc);
                expect(cc, TK_RPAREN);
                n = node_num(cc, type_size(expr->type), line);
                return n;
            }
        } else {
            Node *expr;
            expr = parse_unary(cc);
            n = node_num(cc, type_size(expr->type), line);
            return n;
        }
    }

    /* fallthrough — unexpected token */
    {
        char buf[256];
        sprintf(buf, "unexpected token %d in expression", cc->tk);
        error(cc, buf);
        next_token(cc);
        n = node_num(cc, 0, line);
        return n;
    }
}

/* --- postfix: [], ., ->, (), ++, -- --- */
/* Bug fix: call handler inlined, && replaced with nested if */

static StructField *find_struct_member(Type *type, const char *name, int *out_offset) {
    StructField *f;
    if (!type) {
        return 0;
    }

    for (f = type->fields; f; f = f->next) {
        if (f->name[0]) {
            if (strcmp(f->name, name) == 0) {
                *out_offset = f->offset;
                return f;
            }
        } else if (f->type && (f->type->kind == TY_STRUCT || f->type->kind == TY_UNION)) {
            int sub_offset = 0;
            StructField *sub = find_struct_member(f->type, name, &sub_offset);
            if (sub) {
                *out_offset = f->offset + sub_offset;
                return sub;
            }
        }
    }
    return 0;
}

Node *parse_postfix(Compiler *cc) {
    Node *n;
    n = parse_primary(cc);

    for (;;) {
        int line;
        line = cc->tk_line;

        /* array subscript */
        if (cc->tk == TK_LBRACKET) {
            Node *idx;
            Node *add;
            next_token(cc);
            idx = parse_expr(cc);
            expect(cc, TK_RBRACKET);
            /* a[i] => *(a + i) */
            add = node_new(cc, ND_ADD, line);
            add->lhs = n;
            add->rhs = idx;
            if (is_pointer(n->type)) {
                add->type = n->type;
            } else {
                add->type = idx->type;
            }
            n = node_new(cc, ND_DEREF, line);
            n->lhs = add;
            if (add->type) {
                if (add->type->base) {
                    n->type = add->type->base;
                } else {
                    n->type = cc->ty_int;
                }
            } else {
                n->type = cc->ty_int;
            }
            continue;
        }

        /* struct member access */
        if (cc->tk == TK_DOT) {
            int accumulated_offset = 0;
            StructField *f;
            Node *member;
            next_token(cc);
            if (cc->tk != TK_IDENT) {
                error(cc, "expected member name");
                break;
            }
            member = node_new(cc, ND_MEMBER, line);
            member->lhs = n;
            strncpy(member->member_name, cc->tk_text, MAX_IDENT - 1);
            /* find field */
            if (n->type) {
                f = find_struct_member(n->type, cc->tk_text, &accumulated_offset);
                if (f) {
                    member->member_offset = accumulated_offset;
                    member->type = f->type;
                    member->member_size = type_size(f->type);
                } else {
                    char buf[256];
                    sprintf(buf, "unknown struct member '%s' in struct '%s' (type=%p, fields=%p)", cc->tk_text, (n->type && n->type->tag[0]) ? n->type->tag : "<anon>", (void*)n->type, n->type ? (void*)n->type->fields : NULL);
                    error(cc, buf);
                    member->type = cc->ty_int;
                }
            } else {
                member->type = cc->ty_int;
            }
            n = member;
            next_token(cc);
            continue;
        }

        /* arrow member access */
        if (cc->tk == TK_ARROW) {
            int accumulated_offset = 0;
            StructField *f;
            Node *deref;
            next_token(cc);
            if (cc->tk != TK_IDENT) {
                error(cc, "expected member name after ->");
                break;
            }
            /* deref first */
            deref = node_new(cc, ND_DEREF, line);
            deref->lhs = n;
            if (n->type) {
                if (n->type->base) {
                    deref->type = n->type->base;
                } else {
                    deref->type = cc->ty_int;
                }
            } else {
                deref->type = cc->ty_int;
            }
            n = node_new(cc, ND_MEMBER, line);
            n->lhs = deref;
            strncpy(n->member_name, cc->tk_text, MAX_IDENT - 1);
            if (deref->type) {
                f = find_struct_member(deref->type, cc->tk_text, &accumulated_offset);
                if (f) {
                    n->member_offset = accumulated_offset;
                    n->type = f->type;
                    n->member_size = type_size(f->type);
                } else {
                    char buf[256];
                    sprintf(buf, "unknown struct member '%s' after -> in struct '%s' (type=%p, fields=%p)", cc->tk_text, (deref->type && deref->type->tag[0]) ? deref->type->tag : "<anon>", (void*)deref->type, deref->type ? (void*)deref->type->fields : NULL);
                    error(cc, buf);
                    n->type = cc->ty_int;
                }
            } else {
                n->type = cc->ty_int;
            }
            next_token(cc);
            continue;
        }

        /* function call — inlined handler, nested ifs instead of && */
        if (cc->tk == TK_LPAREN) {
            int is_direct = 0;
            if (n->kind == ND_VAR) {
                if (!n->sym) is_direct = 1;
                else if (n->sym->type && n->sym->type->kind == TY_FUNC) is_direct = 1;
            }
            if (is_direct) {
                /* function call */
                Node *call;
                Symbol *sym;
                next_token(cc);
                call = node_new(cc, ND_CALL, line);
                strncpy(call->func_name, n->name, MAX_IDENT - 1);
                call->args = (Node **)cc_alloc(cc, sizeof(Node *) * MAX_CALL_ARGS);
                call->num_args = 0;
                while (cc->tk != TK_RPAREN) {
                    if (cc->tk == TK_EOF) break;
                    if (call->num_args < MAX_CALL_ARGS) {
                        call->args[call->num_args] = parse_assign(cc);
                        call->num_args = call->num_args + 1;
                    } else {
                        if (!_warned_max_call_args) { printf("zcc: warning: MAX_CALL_ARGS (%d) exceeded at %s:%d — subsequent call args silently discarded\n", MAX_CALL_ARGS, cc->filename ? cc->filename : "?", cc->tk_line); _warned_max_call_args = 1; }
                        parse_assign(cc);
                    }
                    if (cc->tk == TK_COMMA) {
                        next_token(cc);
                    } else {
                        break;
                    }
                }
                expect(cc, TK_RPAREN);
                sym = scope_find(cc, call->func_name);
                if (sym) {
                    if (sym->type) {
                        if (sym->type->kind == TY_FUNC) {
                            if (sym->type->ret) {
                                call->type = sym->type->ret;
                            } else {
                                call->type = cc->ty_int;
                            }
                        } else {
                            call->type = cc->ty_int;
                        }
                    } else {
                        call->type = cc->ty_int;
                    }
                } else {
                    call->type = cc->ty_int;
                }
                n = call;
                continue;
            }
            if (n->kind == ND_MEMBER || n->kind == ND_DEREF || n->kind == ND_CAST || (n->kind == ND_VAR && !is_direct)) {
                /* indirect call through function pointer member, deref, or cast */
                Node *call;
                next_token(cc);
                call = node_new(cc, ND_CALL, line);
                call->func_name[0] = 0; /* empty = indirect */
                call->lhs = n; /* callee expression */
                call->args = (Node **)cc_alloc(cc, sizeof(Node *) * MAX_CALL_ARGS);
                call->num_args = 0;
                while (cc->tk != TK_RPAREN) {
                    if (cc->tk == TK_EOF) break;
                    if (call->num_args < MAX_CALL_ARGS) {
                        call->args[call->num_args] = parse_assign(cc);
                        call->num_args = call->num_args + 1;
                    } else {
                        if (!_warned_max_call_args) { printf("zcc: warning: MAX_CALL_ARGS (%d) exceeded at %s:%d — subsequent call args silently discarded\n", MAX_CALL_ARGS, cc->filename ? cc->filename : "?", cc->tk_line); _warned_max_call_args = 1; }
                        parse_assign(cc);
                    }
                    if (cc->tk == TK_COMMA) {
                        next_token(cc);
                    } else {
                        break;
                    }
                }
                expect(cc, TK_RPAREN);
                /* derive return type from function pointer type */
                if (n->type) {
                    if (n->type->kind == TY_FUNC) {
                        call->type = n->type->ret ? n->type->ret : cc->ty_int;
                    } else if (n->type->kind == TY_PTR && n->type->base && n->type->base->kind == TY_FUNC) {
                        call->type = n->type->base->ret ? n->type->base->ret : cc->ty_int;
                    } else {
                        call->type = cc->ty_int;
                    }
                } else {
                    call->type = cc->ty_int;
                }
                n = call;
                continue;
            }
        }

        /* post increment */
        if (cc->tk == TK_INC) {
            Node *inc;
            next_token(cc);
            inc = node_new(cc, ND_POST_INC, line);
            inc->lhs = n;
            inc->type = n->type;
            n = inc;
            continue;
        }

        /* post decrement */
        if (cc->tk == TK_DEC) {
            Node *dec;
            next_token(cc);
            dec = node_new(cc, ND_POST_DEC, line);
            dec->lhs = n;
            dec->type = n->type;
            n = dec;
            continue;
        }

        break;
    }

    return n;
}

/* --- unary: -, +, !, ~, *, &, ++, --, cast, sizeof --- */

Node *parse_unary(Compiler *cc) {
    int line;
    Node *n;

    line = cc->tk_line;

    if (cc->tk == TK_SIZEOF) {
        Type *t = 0;
        int sz = 0;
        int is_type = 0;
        next_token(cc);
        if (cc->tk == TK_LPAREN) {
            int pk = peek_token(cc);
            if ((pk >= TK_INT && pk <= TK_DOUBLE) || (pk >= TK_STATIC && pk <= TK_INLINE) || pk == TK_STRUCT || pk == TK_UNION || pk == TK_ENUM || pk == TK_TYPEDEF) {
                is_type = 1;
            } else if (pk == TK_IDENT) {
                Symbol *sym = scope_find(cc, cc->peek_text);
                if (sym && sym->is_typedef) is_type = 1;
            }
        }
        
        if (is_type) {
            char dummy[MAX_IDENT];
            next_token(cc);
            t = parse_type(cc);
            t = parse_declarator(cc, t, dummy);
            expect(cc, TK_RPAREN);
        } else {
            Node *expr_n;
            if (cc->tk == TK_LPAREN) {
                next_token(cc);
                expr_n = parse_expr(cc);
                expect(cc, TK_RPAREN);
            } else {
                expr_n = parse_unary(cc);
            }
            if (expr_n && expr_n->type) {
                t = expr_n->type;
            }
        }
        if (t) sz = type_size(t);
        if (sz == 0) sz = 8;
        n = node_num(cc, sz, line);
        n->type = cc->ty_ulong;
        return n;
    }

    if (cc->tk == TK_LPAREN) {
        int pk = peek_token(cc);
        int is_cast = 0;
        if (pk == TK_INT || pk == TK_CHAR || pk == TK_VOID || pk == TK_STRUCT || pk == TK_UNION || pk == TK_ENUM || pk == TK_LONG || pk == TK_SHORT || pk == TK_UNSIGNED || pk == TK_SIGNED || pk == TK_FLOAT || pk == TK_DOUBLE) {
            is_cast = 1;
        } else if (pk == TK_IDENT) {
            Symbol *sym = scope_find(cc, cc->peek_text);
            if (sym && sym->is_typedef) is_cast = 1;
        }
        
        if (is_cast) {
            char dummy[MAX_IDENT];
            Type *cast_type;
            next_token(cc); /* consume LPAREN */
            cast_type = parse_type(cc);
            cast_type = parse_declarator(cc, cast_type, dummy);
            expect(cc, TK_RPAREN);
            if (cc->tk == TK_LBRACE) {
                Symbol *sym;
                char tmp_name[32];
                sprintf(tmp_name, ".LS_comp_%d", cc->str_label_count++);
                sym = scope_add_local(cc, tmp_name, cast_type);

                Node *var_n_final = node_new(cc, ND_VAR, line);
                strncpy(var_n_final->name, tmp_name, MAX_IDENT - 1);
                var_n_final->sym = sym;
                var_n_final->type = cast_type;

                Node *expr = 0;
                next_token(cc); /* skip { */

                if (cast_type->kind == TY_STRUCT) {
                    StructField *sf = cast_type->fields;
                    while (cc->tk != TK_RBRACE && cc->tk != TK_EOF && sf) {
                        Node *var_n = node_new(cc, ND_VAR, line);
                        strncpy(var_n->name, tmp_name, MAX_IDENT - 1);
                        var_n->sym = sym;
                        var_n->type = cast_type;

                        Node *mem_n = node_new(cc, ND_MEMBER, line);
                        mem_n->lhs = var_n;
                        strncpy(mem_n->member_name, sf->name, MAX_IDENT - 1);
                        mem_n->member_offset = sf->offset;
                        mem_n->type = sf->type;
                        mem_n->member_size = type_size(sf->type);

                        Node *asgn_v = parse_assign(cc);
                        Node *asgn_n = node_new(cc, ND_ASSIGN, line);
                        asgn_n->lhs = mem_n;
                        asgn_n->rhs = asgn_v;
                        asgn_n->type = mem_n->type;

                        if (expr) {
                            Node *cma = node_new(cc, ND_COMMA_EXPR, line);
                            cma->lhs = expr;
                            cma->rhs = asgn_n;
                            cma->type = asgn_n->type;
                            expr = cma;
                        } else {
                            expr = asgn_n;
                        }

                        sf = sf->next;
                        if (cc->tk == TK_COMMA) next_token(cc);
                    }
                } else if (cast_type->kind == TY_ARRAY) {
                    int idx = 0;
                    while (cc->tk != TK_RBRACE && cc->tk != TK_EOF) {
                        Node *var_n = node_new(cc, ND_VAR, line);
                        strncpy(var_n->name, tmp_name, MAX_IDENT - 1);
                        var_n->sym = sym;
                        var_n->type = cast_type;

                        Node *add_n = node_new(cc, ND_ADD, line);
                        add_n->lhs = var_n;
                        add_n->rhs = node_num(cc, (long long)idx, line);
                        add_n->type = cast_type;

                        Node *deref_n = node_new(cc, ND_DEREF, line);
                        deref_n->lhs = add_n;
                        deref_n->type = cast_type->base;

                        Node *asgn_v = parse_assign(cc);
                        Node *asgn_n = node_new(cc, ND_ASSIGN, line);
                        asgn_n->lhs = deref_n;
                        asgn_n->rhs = asgn_v;
                        asgn_n->type = cast_type->base;

                        if (expr) {
                            Node *cma = node_new(cc, ND_COMMA_EXPR, line);
                            cma->lhs = expr;
                            cma->rhs = asgn_n;
                            cma->type = asgn_n->type;
                            expr = cma;
                        } else {
                            expr = asgn_n;
                        }

                        idx++;
                        if (cc->tk == TK_COMMA) next_token(cc);
                    }
                } else {
                    Node *var_n = node_new(cc, ND_VAR, line);
                    strncpy(var_n->name, tmp_name, MAX_IDENT - 1);
                    var_n->sym = sym;
                    var_n->type = cast_type;

                    Node *asgn_v = parse_assign(cc);
                    Node *asgn_n = node_new(cc, ND_ASSIGN, line);
                    asgn_n->lhs = var_n;
                    asgn_n->rhs = asgn_v;
                    asgn_n->type = cast_type;
                    expr = asgn_n;
                    if (cc->tk == TK_COMMA) next_token(cc);
                }

                while (cc->tk != TK_RBRACE && cc->tk != TK_EOF) next_token(cc);
                expect(cc, TK_RBRACE);

                if (!expr) expr = var_n_final;
                else {
                    Node *cma = node_new(cc, ND_COMMA_EXPR, line);
                    cma->lhs = expr;
                    cma->rhs = var_n_final;
                    cma->type = cast_type;
                    expr = cma;
                }
                return expr;
            }

            n = node_new(cc, ND_CAST, line);
            n->lhs = parse_unary(cc);
            n->type = cast_type;
            n->cast_type = cast_type;
            return n;
        }
    }

    if (cc->tk == TK_MINUS) {
        next_token(cc);
        n = node_new(cc, ND_NEG, line);
        n->lhs = parse_unary(cc);
        n->type = n->lhs->type;
        return n;
    }
    if (cc->tk == TK_PLUS) {
        next_token(cc);
        return parse_unary(cc);
    }
    if (cc->tk == TK_BANG) {
        next_token(cc);
        n = node_new(cc, ND_LNOT, line);
        n->lhs = parse_unary(cc);
        n->type = cc->ty_int;
        return n;
    }
    if (cc->tk == TK_TILDE) {
        next_token(cc);
        n = node_new(cc, ND_BNOT, line);
        n->lhs = parse_unary(cc);
        n->type = n->lhs->type;
        return n;
    }
    if (cc->tk == TK_STAR) {
        next_token(cc);
        n = node_new(cc, ND_DEREF, line);
        n->lhs = parse_unary(cc);
        if (n->lhs->type) {
            if (n->lhs->type->base) {
                n->type = n->lhs->type->base;
            } else {
                n->type = cc->ty_int;
            }
        } else {
            n->type = cc->ty_int;
        }
        return n;
    }
    if (cc->tk == TK_AMP) {
        next_token(cc);
        n = node_new(cc, ND_ADDR, line);
        n->lhs = parse_unary(cc);
        n->type = type_ptr(cc, n->lhs->type);
        return n;
    }
    if (cc->tk == TK_LAND) {
        /* GNU C Extension: address of label &&label */
        next_token(cc);
        n = node_new(cc, ND_ADDR_LABEL, line);
        strncpy(n->label_name, cc->tk_text, MAX_IDENT - 1);
        expect(cc, TK_IDENT);
        n->type = type_ptr(cc, cc->ty_void); /* void * */
        return n;
    }
    if (cc->tk == TK_INC) {
        next_token(cc);
        n = node_new(cc, ND_PRE_INC, line);
        n->lhs = parse_unary(cc);
        n->type = n->lhs->type;
        return n;
    }
    if (cc->tk == TK_DEC) {
        next_token(cc);
        n = node_new(cc, ND_PRE_DEC, line);
        n->lhs = parse_unary(cc);
        n->type = n->lhs->type;
        return n;
    }

    return parse_postfix(cc);
}

/* --- binary operators: mul, add, shift, relational, equality, bit, logical --- */

static Type *promote_type(Compiler *cc, Type *t1, Type *t2) {
    if (!t1) return t2;
    if (!t2) return t1;
    if (t1->kind == TY_DOUBLE || t2->kind == TY_DOUBLE) return cc->ty_double;
    if (t1->kind == TY_FLOAT || t2->kind == TY_FLOAT) return cc->ty_float;
    if (t1->kind == TY_PTR) return t1;
    if (t2->kind == TY_PTR) return t2;
    if (type_size(t1) >= 8 || type_size(t2) >= 8) {
        if (is_unsigned_type(t1) || is_unsigned_type(t2)) return cc->ty_ulong;
        return cc->ty_long;
    }
    if (is_unsigned_type(t1) || is_unsigned_type(t2)) return cc->ty_uint;
    return cc->ty_int;
}

static Node *ensure_type(Compiler *cc, Node *n, Type *ty) {
    if (!n || !n->type || !ty) return n;
    if (n->type == ty) return n;
    Node *c = node_new(cc, ND_CAST, n->line);
    c->lhs = n;
    c->cast_type = ty;
    c->type = ty;
    return c;
}

Node *parse_mul(Compiler *cc) {
    Node *n;
    n = parse_unary(cc);
    while (cc->tk == TK_STAR || cc->tk == TK_SLASH || cc->tk == TK_PERCENT) {
        int op;
        int line;
        Node *rhs;
        Node *binop;
        Type *pt;
        line = cc->tk_line;
        if (cc->tk == TK_STAR) op = ND_MUL;
        else if (cc->tk == TK_SLASH) op = ND_DIV;
        else op = ND_MOD;
        next_token(cc);
        rhs = parse_unary(cc);
        pt = promote_type(cc, n->type, rhs->type);
        n = ensure_type(cc, n, pt);
        rhs = ensure_type(cc, rhs, pt);
        binop = node_new(cc, op, line);
        binop->lhs = n;
        binop->rhs = rhs;
        binop->type = pt;
        n = binop;
    }
    return n;
}

Node *parse_add(Compiler *cc) {
    Node *n;
    Type *pt;
    n = parse_mul(cc);
    while (cc->tk == TK_PLUS || cc->tk == TK_MINUS) {
        int op;
        int line;
        Node *rhs;
        Node *binop;
        Type *pt;
        line = cc->tk_line;
        if (cc->tk == TK_PLUS) op = ND_ADD;
        else op = ND_SUB;
        next_token(cc);
        rhs = parse_mul(cc);
        binop = node_new(cc, op, line);
        binop->lhs = n;
        binop->rhs = rhs;
        /* pointer arithmetic */
        if (is_pointer(n->type)) {
            binop->type = n->type;
        } else if (is_pointer(rhs->type)) {
            binop->type = rhs->type;
        } else {
            pt = promote_type(cc, n->type, rhs->type);
            n = ensure_type(cc, n, pt);
            rhs = ensure_type(cc, rhs, pt);
            binop->lhs = n;
            binop->rhs = rhs;
            binop->type = pt;
        }
        n = binop;
    }
    return n;
}

Node *parse_shift(Compiler *cc) {
    Node *n;
    n = parse_add(cc);
    while (cc->tk == TK_SHL || cc->tk == TK_SHR) {
        int op;
        int line;
        Node *rhs;
        Node *binop;
        line = cc->tk_line;
        if (cc->tk == TK_SHL) op = ND_SHL;
        else op = ND_SHR;
        next_token(cc);
        rhs = parse_add(cc);
        binop = node_new(cc, op, line);
        binop->lhs = n;
        binop->rhs = rhs;
        binop->type = n->type;
        n = binop;
    }
    return n;
}

Node *parse_relational(Compiler *cc) {
    Node *n;
    n = parse_shift(cc);
    while (cc->tk == TK_LT || cc->tk == TK_GT || cc->tk == TK_LE || cc->tk == TK_GE) {
        int op;
        int line;
        Node *rhs;
        Node *binop;
        Type *pt;
        line = cc->tk_line;
        if (cc->tk == TK_LT) op = ND_LT;
        else if (cc->tk == TK_GT) op = ND_GT;
        else if (cc->tk == TK_LE) op = ND_LE;
        else op = ND_GE;
        next_token(cc);
        rhs = parse_shift(cc);
        pt = promote_type(cc, n->type, rhs->type);
        n = ensure_type(cc, n, pt);
        rhs = ensure_type(cc, rhs, pt);
        binop = node_new(cc, op, line);
        binop->lhs = n;
        binop->rhs = rhs;
        binop->type = cc->ty_int;
        n = binop;
    }
    return n;
}

Node *parse_equality(Compiler *cc) {
    Node *n;
    n = parse_relational(cc);
    while (cc->tk == TK_EQ || cc->tk == TK_NE) {
        int op;
        int line;
        Node *rhs;
        Node *binop;
        Type *pt;
        line = cc->tk_line;
        if (cc->tk == TK_EQ) op = ND_EQ;
        else op = ND_NE;
        next_token(cc);
        rhs = parse_relational(cc);
        pt = promote_type(cc, n->type, rhs->type);
        n = ensure_type(cc, n, pt);
        rhs = ensure_type(cc, rhs, pt);
        binop = node_new(cc, op, line);
        binop->lhs = n;
        binop->rhs = rhs;
        binop->type = cc->ty_int;
        n = binop;
    }
    return n;
}

Node *parse_bitand(Compiler *cc) {
    Node *n;
    n = parse_equality(cc);
    while (cc->tk == TK_AMP) {
        int line;
        Node *rhs;
        Node *binop;
        Type *pt;
        line = cc->tk_line;
        next_token(cc);
        rhs = parse_equality(cc);
        pt = promote_type(cc, n->type, rhs->type);
        n = ensure_type(cc, n, pt);
        rhs = ensure_type(cc, rhs, pt);
        binop = node_new(cc, ND_BAND, line);
        binop->lhs = n;
        binop->rhs = rhs;
        binop->type = pt;
        n = binop;
    }
    return n;
}

Node *parse_bitxor(Compiler *cc) {
    Node *n;
    n = parse_bitand(cc);
    while (cc->tk == TK_CARET) {
        int line;
        Node *rhs;
        Node *binop;
        Type *pt;
        line = cc->tk_line;
        next_token(cc);
        rhs = parse_bitand(cc);
        pt = promote_type(cc, n->type, rhs->type);
        n = ensure_type(cc, n, pt);
        rhs = ensure_type(cc, rhs, pt);
        binop = node_new(cc, ND_BXOR, line);
        binop->lhs = n;
        binop->rhs = rhs;
        binop->type = pt;
        n = binop;
    }
    return n;
}

Node *parse_bitor(Compiler *cc) {
    Node *n;
    Type *pt;
    n = parse_bitxor(cc);
    while (cc->tk == TK_PIPE) {
        int line;
        Node *rhs;
        Node *binop;
        Type *pt;
        line = cc->tk_line;
        next_token(cc);
        rhs = parse_bitxor(cc);
        pt = promote_type(cc, n->type, rhs->type);
        n = ensure_type(cc, n, pt);
        rhs = ensure_type(cc, rhs, pt);
        binop = node_new(cc, ND_BOR, line);
        binop->lhs = n;
        binop->rhs = rhs;
        binop->type = pt;
        n = binop;
    }
    return n;
}

Node *parse_logand(Compiler *cc) {
    Node *n;
    n = parse_bitor(cc);
    while (cc->tk == TK_LAND) {
        int line;
        Node *rhs;
        Node *binop;
        line = cc->tk_line;
        next_token(cc);
        rhs = parse_bitor(cc);
        binop = node_new(cc, ND_LAND, line);
        binop->lhs = n;
        binop->rhs = rhs;
        binop->type = cc->ty_int;
        n = binop;
    }
    return n;
}

Node *parse_logor(Compiler *cc) {
    Node *n;
    n = parse_logand(cc);
    while (cc->tk == TK_LOR) {
        int line;
        Node *rhs;
        Node *binop;
        line = cc->tk_line;
        next_token(cc);
        rhs = parse_logand(cc);
        binop = node_new(cc, ND_LOR, line);
        binop->lhs = n;
        binop->rhs = rhs;
        binop->type = cc->ty_int;
        n = binop;
    }
    return n;
}

Node *parse_ternary(Compiler *cc) {
    Node *n;
    n = parse_logor(cc);
    if (cc->tk == TK_QUESTION) {
        Node *tern;
        int line;
        line = cc->tk_line;
        next_token(cc);
        tern = node_new(cc, ND_TERNARY, line);
        tern->cond = n;
        tern->then_body = parse_expr(cc);
        expect(cc, TK_COLON);
        tern->else_body = parse_ternary(cc);
        tern->type = promote_type(cc, tern->then_body->type, tern->else_body->type);
        tern->then_body = ensure_type(cc, tern->then_body, tern->type);
        tern->else_body = ensure_type(cc, tern->else_body, tern->type);
        n = tern;
    }
    return n;
}

Node *parse_assign(Compiler *cc) {
    Node *n;
    n = parse_ternary(cc);

    if (cc->tk == TK_ASSIGN) {
        Node *asgn;
        int line;
        line = cc->tk_line;
        next_token(cc);
        asgn = node_new(cc, ND_ASSIGN, line);
        asgn->lhs = n;
        asgn->rhs = ensure_type(cc, parse_assign(cc), n->type);
        asgn->type = n->type;
        return asgn;
    }

    /* compound assignment: +=, -=, *=, /=, %=, &=, |=, ^=, <<=, >>= */
    if (cc->tk >= TK_PLUS_ASSIGN) {
        if (cc->tk <= TK_SHR_ASSIGN) {
            Node *ca;
            int line;
            int op;
            line = cc->tk_line;
            switch (cc->tk) {
                case TK_PLUS_ASSIGN:    op = ND_ADD; break;
                case TK_MINUS_ASSIGN:   op = ND_SUB; break;
                case TK_STAR_ASSIGN:    op = ND_MUL; break;
                case TK_SLASH_ASSIGN:   op = ND_DIV; break;
                case TK_PERCENT_ASSIGN: op = ND_MOD; break;
                case TK_AMP_ASSIGN:     op = ND_BAND; break;
                case TK_PIPE_ASSIGN:    op = ND_BOR; break;
                case TK_CARET_ASSIGN:   op = ND_BXOR; break;
                case TK_SHL_ASSIGN:     op = ND_SHL; break;
                case TK_SHR_ASSIGN:     op = ND_SHR; break;
                default: op = ND_ADD; break;
            }
            next_token(cc);
            ca = node_new(cc, ND_COMPOUND_ASSIGN, line);
            ca->compound_op = op;
            ca->lhs = n;
            ca->rhs = parse_assign(cc);
            ca->type = n->type;
            return ca;
        }
    }

    return n;
}

Node *parse_expr(Compiler *cc) {
    Node *n;
    n = parse_assign(cc);
    while (cc->tk == TK_COMMA) {
        Node *comma;
        int line;
        line = cc->tk_line;
        next_token(cc);
        comma = node_new(cc, ND_COMMA_EXPR, line);
        comma->lhs = n;
        comma->rhs = parse_assign(cc);
        comma->type = comma->rhs->type;
        n = comma;
    }
    return n;
}

/* ================================================================ */
/* STATEMENT PARSING                                                 */
/* ================================================================ */

static Node *current_sw = 0;

Node *parse_stmt(Compiler *cc) {
    int line;
    line = cc->tk_line;

    /* Skip __asm__(...) / asm(...) / __asm(...) statements */
    if (cc->tk == TK_IDENT) {
        if (strcmp(cc->tk_text, "__asm__") == 0 ||
            strcmp(cc->tk_text, "asm") == 0 ||
            strcmp(cc->tk_text, "__asm") == 0) {
            next_token(cc); /* consume asm keyword */
            /* skip optional volatile qualifier */
            if (cc->tk == TK_IDENT && (strcmp(cc->tk_text, "__volatile__") == 0 ||
                strcmp(cc->tk_text, "volatile") == 0)) {
                next_token(cc);
            }
            if (cc->tk == TK_VOLATILE) next_token(cc);
            if (cc->tk == TK_LPAREN) {
                int depth = 1;
                next_token(cc);
                while (depth > 0 && cc->tk != TK_EOF) {
                    if (cc->tk == TK_LPAREN) depth++;
                    else if (cc->tk == TK_RPAREN) depth--;
                    if (depth > 0) next_token(cc);
                }
                if (cc->tk == TK_RPAREN) next_token(cc);
            }
            if (cc->tk == TK_SEMI) next_token(cc);
            return node_new(cc, ND_NOP, line);
        }
    }

    /* inline assembly */
    if (cc->tk == TK_ASM) {
        Node *n;
        next_token(cc);
        if (cc->tk == TK_VOLATILE) next_token(cc);
        expect(cc, TK_LPAREN);
        n = node_new(cc, ND_ASM, line);
        if (cc->tk == TK_STR) {
            n->asm_string = cc_strdup(cc, cc->tk_str);
            next_token(cc);
        } else {
            error(cc, "expected string literal in asm");
            n->asm_string = "";
        }
        expect(cc, TK_RPAREN);
        expect(cc, TK_SEMI);
        return n;
    }

    /* block */
    if (cc->tk == TK_LBRACE) {
        Node *block;
        int cap;
        int cnt;
        next_token(cc);
        scope_push(cc);
        block = node_new(cc, ND_BLOCK, line);
        cap = 64;
        block->stmts = (Node **)cc_alloc(cc, sizeof(Node *) * cap);
        cnt = 0;
        while (cc->tk != TK_RBRACE) {
            int prev_pos;
            int no_progress_count;
            if (cc->tk == TK_EOF) break;
            if (cnt >= 4096) {
                error(cc, "block has too many statements (possible parser loop)");
                break;
            }
            if (cnt >= cap) {
                Node **old;
                int newcap;
                int i;
                old = block->stmts;
                newcap = cap * 2;
                if (newcap > 4096) newcap = 4096;
                block->stmts = (Node **)cc_alloc(cc, sizeof(Node *) * newcap);
                for (i = 0; i < cnt; i++) block->stmts[i] = old[i];
                cap = newcap;
            }
            prev_pos = cc->pos;
            block->stmts[cnt] = parse_stmt(cc);
            cnt++;
            no_progress_count = 0;
            /* avoid infinite loop if parse_stmt didn't consume (e.g. after error) */
            while (cc->pos == prev_pos && cc->tk != TK_RBRACE && cc->tk != TK_EOF) {
                next_token(cc);
                no_progress_count++;
                if (no_progress_count > 1024) {
                    error(cc, "parser stuck (no progress after 1024 tokens)");
                    block->num_stmts = cnt;
                    scope_pop(cc);
                    return block;
                }
            }
        }
        block->num_stmts = cnt;
        scope_pop(cc);
        expect(cc, TK_RBRACE);
        return block;
    }

    /* return */
    if (cc->tk == TK_RETURN) {
        Node *ret;
        next_token(cc);
        ret = node_new(cc, ND_RETURN, line);
        if (cc->tk != TK_SEMI) {
            Node *ret_expr = parse_expr(cc);
            /* CG-FLOAT-005: coerce return value to declared return type.
             * Prevents e.g. ND_FLIT (ty_double) being returned raw from a
             * float-returning function — low 32 bits of double = 0.0 */
            if (cc->current_func[0]) {
                Symbol *fsym = scope_find(cc, cc->current_func);
                if (fsym && fsym->type && fsym->type->ret)
                    ret_expr = ensure_type(cc, ret_expr, fsym->type->ret);
            }
            ret->lhs = ret_expr;
        }
        expect(cc, TK_SEMI);
        return ret;
    }

    /* if */
    if (cc->tk == TK_IF) {
        Node *ifn;
        next_token(cc);
        expect(cc, TK_LPAREN);
        ifn = node_new(cc, ND_IF, line);
        ifn->cond = parse_expr(cc);
        expect(cc, TK_RPAREN);
        ifn->then_body = parse_stmt(cc);
        if (cc->tk == TK_ELSE) {
            next_token(cc);
            ifn->else_body = parse_stmt(cc);
        }
        return ifn;
    }

    /* while */
    if (cc->tk == TK_WHILE) {
        Node *wh;
        next_token(cc);
        expect(cc, TK_LPAREN);
        wh = node_new(cc, ND_WHILE, line);
        wh->cond = parse_expr(cc);
        expect(cc, TK_RPAREN);
        wh->body = parse_stmt(cc);
        return wh;
    }

    /* for */
    if (cc->tk == TK_FOR) {
        Node *forn;
        next_token(cc);
        expect(cc, TK_LPAREN);
        forn = node_new(cc, ND_FOR, line);

        scope_push(cc);

        /* init — could be declaration or expression */
        if (cc->tk == TK_SEMI) {
            next_token(cc);
            forn->init = 0;
        } else if (is_type_token(cc)) {
            forn->init = parse_stmt(cc);  /* handles the semicolon */
        } else {
            forn->init = parse_expr(cc);
            expect(cc, TK_SEMI);
        }

        /* condition */
        if (cc->tk == TK_SEMI) {
            forn->cond = 0;
        } else {
            forn->cond = parse_expr(cc);
        }
            expect(cc, TK_SEMI);

        /* increment */
        if (cc->tk == TK_RPAREN) {
            forn->inc = 0;
        } else {
            forn->inc = parse_expr(cc);
        }
        expect(cc, TK_RPAREN);

        forn->body = parse_stmt(cc);
        scope_pop(cc);
        return forn;
    }

    /* do-while */
    if (cc->tk == TK_DO) {
        Node *dow;
        next_token(cc);
        dow = node_new(cc, ND_DO_WHILE, line);
        dow->body = parse_stmt(cc);
        if (cc->tk != TK_WHILE) {
            error(cc, "expected 'while' after do body");
        }
        next_token(cc); /* skip 'while' */
        expect(cc, TK_LPAREN);
        dow->cond = parse_expr(cc);
        expect(cc, TK_RPAREN);
            expect(cc, TK_SEMI);
        return dow;
    }

    /* switch */
    if (cc->tk == TK_SWITCH) {
        Node *sw;
        next_token(cc);
        expect(cc, TK_LPAREN);
        sw = node_new(cc, ND_SWITCH, line);
        sw->cond = parse_expr(cc);
        expect(cc, TK_RPAREN);
        /* parse body — extract cases */
        sw->cases = (Node **)cc_alloc(cc, sizeof(Node *) * MAX_CASES);
        sw->num_cases = 0;
        sw->default_case = 0;
        
        Node *old_sw = current_sw;
        current_sw = sw;

        sw->body = parse_stmt(cc);

        current_sw = old_sw;
        return sw;
    }

    if (cc->tk == TK_CASE) {
        Node *cn;
        Node *cnode;
        Node *gto;
        long long val;
        next_token(cc);
        val = eval_const_expr(parse_assign(cc));
        expect(cc, TK_COLON);

        cn = node_new(cc, ND_LABEL, line);
        sprintf(cn->label_name, "__zc_%llx_%d", (unsigned long long)val, cc->label_count++);
        cn->lhs = parse_stmt(cc);

        if (current_sw && current_sw->num_cases < MAX_CASES) {
            cnode = node_new(cc, ND_CASE, line);
            cnode->case_val = val;
            gto = node_new(cc, ND_GOTO, line);
            strncpy(gto->label_name, cn->label_name, MAX_IDENT - 1);
            cnode->case_body = gto;
            current_sw->cases[current_sw->num_cases++] = cnode;
        } else if (current_sw) {
            if (!_warned_max_cases) { printf("zcc: warning: MAX_CASES (%d) exceeded at %s:%d — subsequent case labels silently discarded\n", MAX_CASES, cc->filename ? cc->filename : "?", cc->tk_line); _warned_max_cases = 1; }
        }
        return cn;
    }

    if (cc->tk == TK_DEFAULT) {
        Node *dn;
        Node *dnode;
        Node *gto;
        next_token(cc);
        expect(cc, TK_COLON);

        dn = node_new(cc, ND_LABEL, line);
        sprintf(dn->label_name, "__zc_def_%d", cc->label_count++);
        dn->lhs = parse_stmt(cc);

        if (current_sw) {
            dnode = node_new(cc, ND_DEFAULT, line);
            gto = node_new(cc, ND_GOTO, line);
            strncpy(gto->label_name, dn->label_name, MAX_IDENT - 1);
            dnode->case_body = gto;
            current_sw->default_case = dnode;
        }
        return dn;
    }

    /* break */
    if (cc->tk == TK_BREAK) {
        Node *brk;
        next_token(cc);
        brk = node_new(cc, ND_BREAK, line);
            expect(cc, TK_SEMI);
        return brk;
    }

    /* continue */
    if (cc->tk == TK_CONTINUE) {
        Node *cont;
        next_token(cc);
        cont = node_new(cc, ND_CONTINUE, line);
            expect(cc, TK_SEMI);
        return cont;
    }

    /* goto */
    if (cc->tk == TK_GOTO) {
        Node *gto;
        next_token(cc);
        if (cc->tk == TK_STAR) {
            next_token(cc); /* consume * */
            gto = node_new(cc, ND_GOTO_COMPUTED, line);
            gto->lhs = parse_expr(cc);
            expect(cc, TK_SEMI);
            return gto;
        }
        gto = node_new(cc, ND_GOTO, line);
        strncpy(gto->label_name, cc->tk_text, MAX_IDENT - 1);
        next_token(cc);
            expect(cc, TK_SEMI);
        return gto;
    }

    /* label: check ident followed by colon */
    if (cc->tk == TK_IDENT) {
        int pk;
        pk = peek_token(cc);
        if (pk == TK_COLON) {
            Node *lbl;
            lbl = node_new(cc, ND_LABEL, line);
            strncpy(lbl->label_name, cc->tk_text, MAX_IDENT - 1);
            next_token(cc);  /* skip ident */
            next_token(cc);  /* skip : */
            lbl->lhs = parse_stmt(cc);
            return lbl;
        }
    }

    /* local variable declaration */
    if (is_type_token(cc)) {
        Type *base;
        int is_typedef = 0;
        if (cc->tk == TK_TYPEDEF) is_typedef = 1;
        base = parse_type(cc);
        int is_static_local = cc->current_is_static;

        /* handle multiple declarators: int a, b, c; */
        {
            Node *block;
            int cap;
            int cnt;
            block = node_new(cc, ND_BLOCK, line);
            cap = 8;
            block->stmts = (Node **)cc_alloc(cc, sizeof(Node *) * cap);
            cnt = 0;

            for (;;) {
                Type *vtype;
                char vname[MAX_IDENT];
                Symbol *sym;
                Node *decl;

                vtype = parse_declarator(cc, base, vname);

                if (vname[0]) {
                    if (is_typedef) {
                        sym = scope_add(cc, vname, vtype);
                        sym->is_typedef = 1;
                    } else if (is_static_local) {
                        char mangled[MAX_IDENT];
                        sprintf(mangled, "%s__%s__S_%d", cc->current_func, vname, cc->str_label_count++);
                        sym = scope_add_local(cc, vname, vtype);
                        sym->is_local = 0;
                        sym->is_global = 1;
                        strncpy(sym->asm_name, mangled, MAX_IDENT - 1);

                        Node *gvar = node_new(cc, ND_GLOBAL_VAR, line);
                        strncpy(gvar->name, mangled, MAX_IDENT - 1);
                        gvar->type = vtype;
                        gvar->is_static = 1;
                        gvar->is_extern = 0;

                        if (cc->tk == TK_ASSIGN) {
                            next_token(cc);
                            if (cc->tk == TK_LBRACE) {
                                Node *init_list;
                                Node **inits;
                                int count;
                                init_list = node_new(cc, ND_INIT_LIST, line);
                                inits = (Node **)cc_alloc(cc, sizeof(Node *) * MAX_INIT);
                                count = 0;
                                next_token(cc); /* skip { */
                                int depth = 1;
                                int skip_tk;
                                int prev_pos;
                                int no_progress_count = 0;
                                int top_elems = 0;
                                int last_comma = 1;
                                while (depth > 0 && cc->tk != TK_EOF) {
                                    if (depth == 1 && cc->tk != TK_RBRACE && cc->tk != TK_COMMA && last_comma) {
                                        top_elems++;
                                        last_comma = 0;
                                    }
                                    prev_pos = cc->pos;
                                    if (cc->tk == TK_LBRACE) {
                                        depth++;
                                        next_token(cc);
                                    } else if (cc->tk == TK_RBRACE) {
                                        depth--;
                                        if (depth == 0) break;
                                        next_token(cc);
                                    } else if (cc->tk == TK_COMMA) {
                                        if (depth == 1) last_comma = 1;
                                        next_token(cc);
                                    } else {
                                        skip_tk = cc->tk;
                                        if (count < MAX_INIT) {
                                            inits[count++] = parse_assign(cc);
                                        } else {
                                            if (!_warned_max_init) { printf("zcc: warning: MAX_INIT (%d) exceeded at %s:%d — subsequent initializers silently discarded\n", MAX_INIT, cc->filename ? cc->filename : "?", cc->tk_line); _warned_max_init = 1; }
                                            parse_assign(cc);
                                        }
                                        if (cc->tk == skip_tk) {
                                            if (cc->tk != TK_EOF) next_token(cc);
                                        }
                                        if (cc->pos == prev_pos) {
                                            no_progress_count++;
                                            if (no_progress_count > 100) { break; }
                                        } else {
                                            no_progress_count = 0;
                                        }
                                    }
                                }
                                if (cc->tk == TK_RBRACE) next_token(cc);
                                init_list->args = inits;
                                init_list->num_args = count;
                                gvar->initializer = init_list;
                                
                                if (gvar->type->kind == TY_ARRAY && gvar->type->array_len == 0) {
                                    gvar->type->array_len = top_elems;
                                    gvar->type->size = type_size(gvar->type->base) * top_elems;
                                    sym->type->array_len = top_elems;
                                    sym->type->size = gvar->type->size;
                                }
                            } else {
                                Node *init_node2 = parse_assign(cc);
                                gvar->initializer = init_node2;
                                /* Fix: if char[] initialized with string literal, update size */
                                if (init_node2 && init_node2->kind == ND_STR &&
                                    gvar->type->kind == TY_ARRAY && gvar->type->array_len == 0) {
                                    int slen2 = cc->strings[init_node2->str_id].len + 1;
                                    gvar->type->array_len = slen2;
                                    gvar->type->size = type_size(gvar->type->base) * slen2;
                                    sym->type->array_len = slen2;
                                    sym->type->size = gvar->type->size;
                                }
                            }
                        }

                        if (cc->num_globals < MAX_GLOBALS) { cc->globals[cc->num_globals++] = gvar; }
                        else { if (!_warned_max_globals) { printf("zcc: warning: MAX_GLOBALS (%d) exceeded at %s:%d — subsequent globals silently discarded\n", MAX_GLOBALS, cc->filename ? cc->filename : "?", cc->tk_line); _warned_max_globals = 1; } }
                    } else {
                        sym = scope_add_local(cc, vname, vtype);

                        /* initializer? */
                        if (cc->tk == TK_ASSIGN) {
                            Node *asgn;
                            Node *var;
                            next_token(cc);

                        /* check for array/struct initializer list */
                        if (cc->tk == TK_LBRACE) {
                            if (vtype->kind == TY_ARRAY) {
                                /* Parse array initializer list and emit element assignments */
                                int depth;
                                int init_count;
                                Node **inits;
                                int init_cap;
                                Type *elem_type;
                                Type *arr_type;
                                int sz;
                                int skip_tk;
                                int prev_pos;
                                int no_progress_count = 0;
                                next_token(cc); /* skip { */
                                init_count = 0;
                                init_cap = 16;
                                inits = (Node **)cc_alloc(cc, sizeof(Node *) * init_cap);
                                depth = 1;
                                while (depth > 0 && cc->tk != TK_EOF) {
                                    prev_pos = cc->pos;
                                    if (cc->tk == TK_LBRACE) {
                                        depth++;
                                        next_token(cc);
                                    } else if (cc->tk == TK_RBRACE) {
                                        depth--;
                                        if (depth == 0) break;
                                        next_token(cc);
                                    } else if (cc->tk == TK_COMMA) {
                                        next_token(cc);
                                    } else {
                                        skip_tk = cc->tk;
                                        Node *expr = parse_assign(cc);
                                        if (cc->tk == skip_tk) {
                                            if (cc->tk != TK_EOF) next_token(cc);
                                        }
                                        if (cc->pos == prev_pos) {
                                            no_progress_count++;
                                            if (no_progress_count > 100) {
                                                error(cc, "array parser stuck (possible infinite loop)");
                                                break;
                                            }
                                        } else {
                                            no_progress_count = 0;
                                        }
                                        if (init_count >= init_cap) {
                                            Node **old_inits = inits;
                                            int j;
                                            init_cap *= 2;
                                            inits = (Node **)cc_alloc(cc, sizeof(Node *) * init_cap);
                                            for (j = 0; j < init_count; j++) inits[j] = old_inits[j];
                                        }
                                        inits[init_count++] = expr;
                                    }
                                }
                                if (cc->tk == TK_RBRACE) next_token(cc); /* skip final } */
                                elem_type = vtype->base;
                                if (vtype->array_len == 0) {
                                    arr_type = type_array(cc, elem_type, init_count);
                                    sym->type = arr_type;
                                    sz = type_size(arr_type);
                                    if (sz < 8) sz = 8;
                                    cc->local_offset = cc->local_offset + 8;
                                    cc->local_offset = cc->local_offset - sz;
                                    sym->stack_offset = cc->local_offset;
                                } else {
                                    arr_type = vtype;
                                }
                                /* Grow block stmts if needed */
                                while (cnt + init_count > cap) {
                                    Node **new_stmts;
                                    int j;
                                    new_stmts = (Node **)cc_alloc(cc, sizeof(Node *) * cap * 2);
                                    for (j = 0; j < cnt; j++) new_stmts[j] = block->stmts[j];
                                    block->stmts = new_stmts;
                                    cap = cap * 2;
                                }
                                {
                                    int idx;
                                    for (idx = 0; idx < init_count; idx++) {
                                        Node *var;
                                        Node *add;
                                        Node *deref;
                                        Node *asgn;
                                        var = node_new(cc, ND_VAR, line);
                                        strncpy(var->name, vname, MAX_IDENT - 1);
                                        var->sym = sym;
                                        var->type = arr_type;
                                        add = node_new(cc, ND_ADD, line);
                                        add->lhs = var;
                                        add->rhs = node_num(cc, (long long)idx, line);
                                        add->type = arr_type;
                                        deref = node_new(cc, ND_DEREF, line);
                                        deref->lhs = add;
                                        deref->type = elem_type;
                                        asgn = node_new(cc, ND_ASSIGN, line);
                                        asgn->lhs = deref;
                                        /* CG-FLOAT-006: coerce array element initializer */
                                        asgn->rhs = ensure_type(cc, inits[idx], elem_type);
                                        asgn->type = elem_type;
                                        block->stmts[cnt] = asgn;
                                        cnt++;
                                    }
                                }
                            } else if (vtype->kind == TY_STRUCT || vtype->kind == TY_UNION) {
                                /* Local struct/union initializer: {v0, v1, ...}
                                 * Walk StructField list, assign each field in order. */
                                int init_count_s;
                                int init_cap_s;
                                Node **inits_s;
                                int skip_tk_s;
                                int prev_pos_s;
                                int no_progress_s;
                                int depth_s;
                                init_count_s = 0;
                                init_cap_s = 32;
                                inits_s = (Node **)cc_alloc(cc, sizeof(Node *) * init_cap_s);
                                depth_s = 1;
                                no_progress_s = 0;
                                next_token(cc); /* skip { */
                                while (depth_s > 0 && cc->tk != TK_EOF) {
                                    prev_pos_s = cc->pos;
                                    if (cc->tk == TK_LBRACE) {
                                        depth_s++;
                                        next_token(cc);
                                    } else if (cc->tk == TK_RBRACE) {
                                        depth_s--;
                                        if (depth_s == 0) break;
                                        next_token(cc);
                                    } else if (cc->tk == TK_COMMA) {
                                        next_token(cc);
                                    } else {
                                        skip_tk_s = cc->tk;
                                        if (init_count_s < init_cap_s) {
                                            inits_s[init_count_s++] = parse_assign(cc);
                                        } else {
                                            parse_assign(cc);
                                        }
                                        if (cc->tk == skip_tk_s) {
                                            if (cc->tk != TK_EOF) next_token(cc);
                                        }
                                        if (cc->pos == prev_pos_s) {
                                            no_progress_s++;
                                            if (no_progress_s > 100) break;
                                        } else {
                                            no_progress_s = 0;
                                        }
                                    }
                                }
                                if (cc->tk == TK_RBRACE) next_token(cc);
                                /* Now emit field assignments */
                                {
                                    StructField *sf;
                                    int fi;
                                    fi = 0;
                                    sf = vtype->fields;
                                    while (sf && fi < init_count_s) {
                                        /* Build: var.sf_name = inits_s[fi] */
                                        Node *var_n;
                                        Node *mem_n;
                                        Node *asgn_n;
                                        int acc_off;
                                        StructField *found_f;
                                        var_n = node_new(cc, ND_VAR, line);
                                        strncpy(var_n->name, vname, MAX_IDENT - 1);
                                        var_n->sym = sym;
                                        var_n->type = vtype;
                                        acc_off = 0;
                                        found_f = find_struct_member(vtype, sf->name, &acc_off);
                                        mem_n = node_new(cc, ND_MEMBER, line);
                                        mem_n->lhs = var_n;
                                        strncpy(mem_n->member_name, sf->name, MAX_IDENT - 1);
                                        if (found_f) {
                                            mem_n->member_offset = acc_off;
                                            mem_n->type = found_f->type;
                                            mem_n->member_size = type_size(found_f->type);
                                        } else {
                                            mem_n->member_offset = sf->offset;
                                            mem_n->type = sf->type;
                                            mem_n->member_size = type_size(sf->type);
                                        }
                                        asgn_n = node_new(cc, ND_ASSIGN, line);
                                        asgn_n->lhs = mem_n;
                                        /* CG-FLOAT-006: coerce struct field initializer */
                                        asgn_n->rhs = ensure_type(cc, inits_s[fi], mem_n->type);
                                        asgn_n->type = mem_n->type;
                                        /* Grow block if needed */
                                        if (cnt >= cap) {
                                            Node **new_stmts;
                                            int ji;
                                            new_stmts = (Node **)cc_alloc(cc, sizeof(Node *) * cap * 2);
                                            for (ji = 0; ji < cnt; ji++) new_stmts[ji] = block->stmts[ji];
                                            block->stmts = new_stmts;
                                            cap = cap * 2;
                                        }
                                        block->stmts[cnt] = asgn_n;
                                        cnt++;
                                        sf = sf->next;
                                        fi++;
                                        /* Skip nested struct fields for now — treat as flat */
                                    }
                                    /* Zero-initialize remaining fields if fewer inits provided */
                                    while (sf) {
                                        Node *var_n;
                                        Node *mem_n;
                                        Node *asgn_n;
                                        int acc_off;
                                        StructField *found_f;
                                        var_n = node_new(cc, ND_VAR, line);
                                        strncpy(var_n->name, vname, MAX_IDENT - 1);
                                        var_n->sym = sym;
                                        var_n->type = vtype;
                                        acc_off = 0;
                                        found_f = find_struct_member(vtype, sf->name, &acc_off);
                                        mem_n = node_new(cc, ND_MEMBER, line);
                                        mem_n->lhs = var_n;
                                        strncpy(mem_n->member_name, sf->name, MAX_IDENT - 1);
                                        if (found_f) {
                                            mem_n->member_offset = acc_off;
                                            mem_n->type = found_f->type;
                                            mem_n->member_size = type_size(found_f->type);
                                        } else {
                                            mem_n->member_offset = sf->offset;
                                            mem_n->type = sf->type;
                                            mem_n->member_size = type_size(sf->type);
                                        }
                                        asgn_n = node_new(cc, ND_ASSIGN, line);
                                        asgn_n->lhs = mem_n;
                                        asgn_n->rhs = node_num(cc, 0LL, line);
                                        asgn_n->type = mem_n->type;
                                        if (cnt >= cap) {
                                            Node **new_stmts;
                                            int ji;
                                            new_stmts = (Node **)cc_alloc(cc, sizeof(Node *) * cap * 2);
                                            for (ji = 0; ji < cnt; ji++) new_stmts[ji] = block->stmts[ji];
                                            block->stmts = new_stmts;
                                            cap = cap * 2;
                                        }
                                        block->stmts[cnt] = asgn_n;
                                        cnt++;
                                        sf = sf->next;
                                    }
                                }
                            } else {
                                /* Other non-array, non-struct types: skip initializer list */
                                int skip_depth_x;
                                skip_depth_x = 1;
                                next_token(cc);
                                while (skip_depth_x > 0) {
                                    if (cc->tk == TK_EOF) break;
                                    if (cc->tk == TK_LBRACE) skip_depth_x = skip_depth_x + 1;
                                    if (cc->tk == TK_RBRACE) skip_depth_x = skip_depth_x - 1;
                                    if (skip_depth_x > 0) next_token(cc);
                                }
                                next_token(cc);
                            }
                        } else {
                            var = node_new(cc, ND_VAR, line);
                            strncpy(var->name, vname, MAX_IDENT - 1);
                            var->sym = sym;
                            var->type = vtype;
                            asgn = node_new(cc, ND_ASSIGN, line);
                            asgn->lhs = var;
                            asgn->rhs = ensure_type(cc, parse_assign(cc), vtype);
                            asgn->type = vtype;
                            if (cnt < cap) {
                                block->stmts[cnt] = asgn;
                                cnt++;
                            }
                        }
                    }
                    }
                }

                if (cc->tk == TK_COMMA) {
                    next_token(cc);
                } else {
                    break;
                }
            }

            block->num_stmts = cnt;
            expect(cc, TK_SEMI);

            if (cnt == 0) {
                return node_new(cc, ND_NOP, line);
            }
            if (cnt == 1) {
                return block->stmts[0];
            }
            return block;
        }
    }

    /* empty statement */
    if (cc->tk == TK_SEMI) {
        next_token(cc);
        return node_new(cc, ND_NOP, line);
    }

    /* expression statement */
    {
        Node *expr;
        expr = parse_expr(cc);
            expect(cc, TK_SEMI);
        return expr;
    }
}

/* ================================================================ */
/* TOP-LEVEL PARSING                                                 */
/* ================================================================ */

static Node *parse_func_def(Compiler *cc, Type *ret_type, char *name, int is_static) {
    Node *func;
    int line;
    int i;
    int is_variadic = 0;

    line = cc->tk_line;
    func = node_new(cc, ND_FUNC_DEF, line);
    func->func_params = cc_alloc(cc, sizeof(struct FuncParams));
    strncpy(func->func_def_name, name, MAX_IDENT - 1);
    func->is_static = is_static;

    /* parse parameters */
    expect(cc, TK_LPAREN);
    scope_push(cc);
    cc->local_offset = 0;

    func->num_params = 0;
    if (cc->tk != TK_RPAREN) {
        if (cc->tk == TK_VOID) {
            int pk;
            pk = peek_token(cc);
            if (pk == TK_RPAREN) {
                next_token(cc);
            } else {
                goto parse_fparams;
            }
        } else {
            parse_fparams:
            while (cc->tk != TK_RPAREN) {
                Type *ptype;
                char pname[MAX_IDENT];
                Symbol *psym;

                if (cc->tk == TK_ELLIPSIS) {
                    is_variadic = 1;
                    next_token(cc);
                    break;
                }
                if (cc->tk == TK_EOF) break;

                ptype = parse_type(cc);
                ptype = parse_declarator(cc, ptype, pname);

                if (ptype->kind == TY_ARRAY) ptype = type_ptr(cc, ptype->base);

                if (func->num_params < MAX_PARAMS) {
                    func->func_params->types[func->num_params] = ptype;
                    strncpy(func->func_params->names[func->num_params], pname, MAX_IDENT - 1);
                    psym = scope_add_local(cc, pname, ptype);
                    func->num_params++;
                } else {
                    if (!_warned_max_params) { printf("zcc: warning: MAX_PARAMS (%d) exceeded at %s:%d — subsequent function params silently discarded\n", MAX_PARAMS, cc->filename ? cc->filename : "?", cc->tk_line); _warned_max_params = 1; }
                }

                if (cc->tk == TK_COMMA) {
                    next_token(cc);
                } else if (cc->tk == TK_RPAREN) {
                    break;
                } else {
                    /* Unexpected token (e.g. ->) after parameter — skip to ) to avoid cascade */
                    if (cc->tk == TK_ARROW) {
                        error(cc, "unexpected '->' in parameter list (missing ',' or ')'?)");
                    } else {
                        if (getenv("ZCC_DEBUG")) printf("!!! parse_fparams ERROR !!! func='%s' param_idx=%d tk_text='%s' tk=%d prev_pname='%s'\n", func->func_def_name, func->num_params, cc->tk_text, cc->tk, func->num_params > 0 ? func->func_params->names[func->num_params - 1] : "NONE");
                        char buf[256];
                        sprintf(buf, "expected ',' or ')' after parameter [at token '%s']", cc->tk_text);
                        error(cc, buf);
                    }
                    while (cc->tk != TK_RPAREN && cc->tk != TK_EOF) next_token(cc);
                    break;
                }
            }
        }
    }
    expect(cc, TK_RPAREN);

    /* CG-IR-VARARGS: Reserve the first 48 bytes exclusively for reg_save_area */
    if (is_variadic && cc->local_offset > -48) {
        cc->local_offset = -48;
    }

    /* register function in global scope (forward decl and definition) */
    {
        Type *ftype;
        Symbol *fsym;
        ftype = type_func(cc, ret_type);
        ftype->num_params = func->num_params;
        ftype->is_variadic = is_variadic;
        if (func->num_params > 0) {
            ftype->params = (Type **)cc_alloc(cc, sizeof(Type *) * func->num_params);
            for (int k = 0; k < func->num_params; k++) {
                ftype->params[k] = func->func_params->types[k];
            }
        }
        func->func_type = ftype;

        /* add to parent scope (global) */
        fsym = scope_find(cc, name);
        if (!fsym) {
            /* temporarily pop to add to parent */
            Scope *cur;
            cur = cc->current_scope;
            cc->current_scope = cur->parent;
            fsym = scope_add(cc, name, ftype);
            fsym->is_global = 1;
            cc->current_scope = cur;
        }
    }

    /* forward declaration: );  — do not parse body, do not add to codegen list */
    if (cc->tk == TK_SEMI) {
        next_token(cc);
        func->body = 0;
        scope_pop(cc);
        return func;
    }

    /* definition: ) { ... } */
    func->body = parse_stmt(cc);
    func->stack_size = -cc->local_offset;
    scope_pop(cc);

    return func;
}

Node *parse_program(Compiler *cc) {
    Node *head;
    Node *tail;
    int top_count;
    int prev_pos;

    head = 0;
    tail = 0;
    top_count = 0;

    while (cc->tk != TK_EOF) {
        Type *base;
        char name[MAX_IDENT];
        Type *dtype;
        int is_typedef_kw;
        int is_static;
        int is_extern;
        int line;

        if (top_count >= 50000) {
            error(cc, "too many top-level declarations (possible parser loop)");
            break;
        }
        if (top_count > 0 && cc->pos == prev_pos) {
            next_token(cc);
        }
        top_count++;
        prev_pos = cc->pos;
        line = cc->tk_line;
        is_typedef_kw = 0;
        is_static = 0;
        is_extern = 0;

        /* check storage class */
        if (cc->tk == TK_TYPEDEF) { is_typedef_kw = 1; }
        if (cc->tk == TK_STATIC) { is_static = 1; }
        if (cc->tk == TK_EXTERN) { is_extern = 1; }

        base = parse_type(cc);

        /* bare struct/enum definition with no declarator */
        if (cc->tk == TK_SEMI) {
            next_token(cc);
            continue;
        }

        /* After base type, parse name and pointers only (not full declarator)
           so we can detect function defs before params are consumed */
        {
            Type *ptr_type;
            ptr_type = base;

            /* parse pointer stars */
            while (cc->tk == TK_STAR) {
                next_token(cc);
                while (cc->tk == TK_CONST || cc->tk == TK_VOLATILE) next_token(cc);
                ptr_type = type_ptr(cc, ptr_type);
            }

            /* get name — also handle grouped declarator (*name)(params) for function pointers */
            name[0] = 0;
            if (cc->tk == TK_LPAREN) {
                int gpk;
                gpk = peek_token(cc);
                if (gpk == TK_STAR || gpk == TK_IDENT) {
                    /* function pointer: typedef int (*name)(params); or int (*fp)(int); */
                    /* also handles parenthesized identifiers: LUA_API int (lua_gettop)(lua_State *L) */
                    int gptr;
                    int arr_lens[8];
                    int arr_num;
                    next_token(cc); /* consume ( */
                    gptr = 0;
                    arr_num = 0;
                    while (cc->tk == TK_STAR) {
                        next_token(cc);
                        while (cc->tk == TK_CONST || cc->tk == TK_VOLATILE) next_token(cc);
                        gptr++;
                    }
                    if (cc->tk == TK_IDENT) {
                        strncpy(name, cc->tk_text, MAX_IDENT - 1);
                        next_token(cc);
                    }
                    int is_inner_func = 0;
                    if (cc->tk == TK_LPAREN) {
                        is_inner_func = 1;
                        next_token(cc);
                        int pcnt = 1;
                        while (pcnt > 0 && cc->tk != TK_EOF) {
                            if (cc->tk == TK_LPAREN) pcnt++;
                            else if (cc->tk == TK_RPAREN) pcnt--;
                            next_token(cc);
                        }
                    }
                    while (cc->tk == TK_LBRACKET) {
                        int alen = 0;
                        next_token(cc);
                        if (cc->tk != TK_RBRACKET) alen = (int)parse_const_expr(cc);
                        expect(cc, TK_RBRACKET);
                        if (arr_num < 8) arr_lens[arr_num++] = alen;
                    }
                    expect(cc, TK_RPAREN);
                    /* parse trailing (params) */
                    if (cc->tk == TK_LPAREN) {
                        Type *ftype;
                        next_token(cc);
                        ftype = type_func(cc, ptr_type);
                        ftype->params = (Type **)cc_alloc(cc, sizeof(Type *) * MAX_PARAMS);
                        ftype->num_params = 0;
                        ftype->is_variadic = 0;
                        if (cc->tk != TK_RPAREN) {
                            if (cc->tk == TK_VOID) {
                                int vpk2;
                                vpk2 = peek_token(cc);
                                if (vpk2 == TK_RPAREN) {
                                    next_token(cc);
                                } else {
                                    goto parse_gd_params;
                                }
                            } else {
                                parse_gd_params:
                                while (cc->tk != TK_RPAREN) {
                                    Type *ptype;
                                    char pname[128];
                                    if (cc->tk == TK_ELLIPSIS) {
                                        ftype->is_variadic = 1;
                                        next_token(cc);
                                        break;
                                    }
                                    if (cc->tk == TK_EOF) break;
                                    ptype = parse_type(cc);
                                    ptype = parse_declarator(cc, ptype, pname);
                                    if (ftype->num_params < MAX_PARAMS) {
                                        ftype->params[ftype->num_params] = ptype;
                                        ftype->num_params++;
                                    } else {
                                        if (!_warned_max_params) { printf("zcc: warning: MAX_PARAMS (%d) exceeded at %s:%d — subsequent params silently discarded\n", MAX_PARAMS, cc->filename ? cc->filename : "?", cc->tk_line); _warned_max_params = 1; }
                                    }
                                    if (cc->tk == TK_COMMA) {
                                        next_token(cc);
                                    } else {
                                        break;
                                    }
                                }
                            }
                        }
                        expect(cc, TK_RPAREN);
                        dtype = ftype;
                    } else {
                        dtype = ptr_type;
                    }
                    while (gptr > 0) {
                        dtype = type_ptr(cc, dtype);
                        gptr--;
                    }
                    while (arr_num > 0) {
                        arr_num--;
                        dtype = type_array(cc, dtype, arr_lens[arr_num]);
                    }
                    if (is_inner_func) {
                        dtype = type_func(cc, dtype);
                    }
                    /* fall through to typedef / global var handling */
                    goto after_name;
                } else if (is_typedef_kw && gpk == TK_IDENT) {
                    /* typedef returntype (Name)(params) — function type typedef
                     * e.g. typedef ssize_t (Curl_send)(struct X *, int, ...); */
                    next_token(cc); /* consume ( */
                    strncpy(name, cc->tk_text, MAX_IDENT - 1);
                    next_token(cc); /* consume Name */
                    expect(cc, TK_RPAREN); /* consume ) */
                    /* Now parse the parameter list */
                    if (cc->tk == TK_LPAREN) {
                        Type *ftype;
                        next_token(cc);
                        ftype = type_func(cc, ptr_type);
                        ftype->params = (Type **)cc_alloc(cc, sizeof(Type *) * MAX_PARAMS);
                        ftype->num_params = 0;
                        ftype->is_variadic = 0;
                        if (cc->tk != TK_RPAREN) {
                            if (cc->tk == TK_VOID) {
                                int vpk3 = peek_token(cc);
                                if (vpk3 == TK_RPAREN) {
                                    next_token(cc);
                                } else {
                                    goto parse_ft_params;
                                }
                            } else {
                                parse_ft_params:
                                while (cc->tk != TK_RPAREN) {
                                    Type *ptype;
                                    char pname[128];
                                    if (cc->tk == TK_ELLIPSIS) {
                                        ftype->is_variadic = 1;
                                        next_token(cc);
                                        break;
                                    }
                                    if (cc->tk == TK_EOF) break;
                                    ptype = parse_type(cc);
                                    ptype = parse_declarator(cc, ptype, pname);
                                    if (ftype->num_params < MAX_PARAMS) {
                                        ftype->params[ftype->num_params] = ptype;
                                        ftype->num_params++;
                                    }
                                    if (cc->tk == TK_COMMA) {
                                        next_token(cc);
                                    } else {
                                        break;
                                    }
                                }
                            }
                        }
                        expect(cc, TK_RPAREN);
                        dtype = ftype;
                    } else {
                        dtype = ptr_type;
                    }
                    goto after_name;
                }
            }
            if (cc->tk == TK_IDENT) {
                strncpy(name, cc->tk_text, MAX_IDENT - 1);
                next_token(cc);
            }
            dtype = ptr_type;

            after_name:
            /* handle bare struct/enum definitions (no name) */
            if (!name[0]) {
                if (cc->tk == TK_SEMI) {
                    next_token(cc);
                    continue;
                }
                /* unnamed — error or typedef was handled above */
                if (cc->tk != TK_LPAREN) {
                    error(cc, "expected name in declaration");
                    next_token(cc);
                    continue;
                }
            }

            /* function definition or forward declaration: name followed by ( */
            if (cc->tk == TK_LPAREN) {
                if (!is_typedef_kw) {
                    Node *func;
                    func = parse_func_def(cc, ptr_type, name, is_static);
                    /* only link into list if definition (has body); forward decl has body == 0 */
                    if (func->body) {
                        func->next = 0;
                        if (!head) { head = func; tail = func; }
                        else { tail->next = func; tail = func; }
                    }
                    continue;
                } else {
                    /* it's a typedef to a function type: typedef int func_t(int, int); */
                    int depth;
                    dtype = type_func(cc, ptr_type);
                    next_token(cc); /* consume ( */
                    depth = 1;
                    while (depth > 0 && cc->tk != TK_EOF) {
                        if (cc->tk == TK_LPAREN) depth++;
                        else if (cc->tk == TK_RPAREN) depth--;
                        next_token(cc);
                    }
                    /* falls through to array dimensional parsing or typedef registration */
                }
            }

            /* array dimensions after name */
            while (cc->tk == TK_LBRACKET) {
                int alen;
                next_token(cc);
                alen = 0;
                if (cc->tk != TK_RBRACKET) alen = (int)parse_const_expr(cc);
                expect(cc, TK_RBRACKET);
                dtype = type_array(cc, dtype, alen);
            }
        }

        /* typedef */
        if (is_typedef_kw) {
            Symbol *sym;
            sym = scope_add(cc, name, dtype);
            sym->is_typedef = 1;
            /* handle multiple typedef names */
            while (cc->tk == TK_COMMA) {
                char name2[MAX_IDENT];
                next_token(cc);
                dtype = parse_declarator(cc, base, name2);
                sym = scope_add(cc, name2, dtype);
                sym->is_typedef = 1;
            }
            expect(cc, TK_SEMI);
            continue;
        }

        /* global variable */
        {
            Node *gvar;
            gvar = node_new(cc, ND_GLOBAL_VAR, line);
            strncpy(gvar->name, name, MAX_IDENT - 1);
            gvar->type = dtype;
            gvar->is_static = is_static;
            gvar->is_extern = is_extern;

            /* register in scope */
            {
                Symbol *sym;
                sym = scope_add(cc, name, dtype);
                sym->is_global = 1;
            }

            /* initializer */
            if (cc->tk == TK_ASSIGN) {
                next_token(cc);
                if (cc->tk == TK_LBRACE) {
                    /* Parse array initializer list */
                    Node *init_list;
                    Node **inits;
                    int count;
                    init_list = node_new(cc, ND_INIT_LIST, line);
                    inits = (Node **)cc_alloc(cc, sizeof(Node *) * MAX_INIT);
                    count = 0;
                    next_token(cc); /* skip { */
                    int depth = 1;
                    int skip_tk;
                    int prev_pos;
                    int no_progress_count = 0;
                    int top_elems = 0;
                    int last_comma = 1;
                    while (depth > 0 && cc->tk != TK_EOF) {
                        if (depth == 1 && cc->tk != TK_RBRACE && cc->tk != TK_COMMA && last_comma) {
                            top_elems++;
                            last_comma = 0;
                        }
                        prev_pos = cc->pos;
                        if (cc->tk == TK_LBRACE) {
                            depth++;
                            next_token(cc);
                        } else if (cc->tk == TK_RBRACE) {
                            depth--;
                            if (depth == 0) break;
                            next_token(cc);
                        } else if (cc->tk == TK_COMMA) {
                            if (depth == 1) last_comma = 1;
                            next_token(cc);
                        } else {
                            skip_tk = cc->tk;
                            if (count < MAX_INIT) {
                                inits[count++] = parse_assign(cc);
                            } else {
                                if (!_warned_max_init) { printf("zcc: warning: MAX_INIT (%d) exceeded at %s:%d — subsequent initializers silently discarded\n", MAX_INIT, cc->filename ? cc->filename : "?", cc->tk_line); _warned_max_init = 1; }
                                parse_assign(cc);
                            }
                            if (cc->tk == skip_tk) {
                                if (cc->tk != TK_EOF) next_token(cc);
                            }
                            if (cc->pos == prev_pos) {
                                no_progress_count++;
                                if (no_progress_count > 100) {
                                    error(cc, "global array parser stuck");
                                    break;
                                }
                            } else {
                                no_progress_count = 0;
                            }
                        }
                    }
                    if (cc->tk == TK_RBRACE) next_token(cc); /* skip } */
                    init_list->args = inits;
                    init_list->num_args = count;
                    gvar->initializer = init_list;
                    
                    /* adjust array size if omitted */
                    if (gvar->type->kind == TY_ARRAY && gvar->type->array_len == 0) {
                        gvar->type->array_len = top_elems;
                        gvar->type->size = type_size(gvar->type->base) * top_elems;
                        if (gvar->sym) {
                            gvar->sym->type->array_len = top_elems;
                            gvar->sym->type->size = gvar->type->size;
                        }
                    }
                } else {
                    Node *init_node = parse_assign(cc);
                    gvar->initializer = init_node;
                    /* Fix: if char[] initialized with string literal, update size */
                    if (init_node && init_node->kind == ND_STR &&
                        gvar->type->kind == TY_ARRAY && gvar->type->array_len == 0) {
                        int slen = cc->strings[init_node->str_id].len + 1; /* +1 for null */
                        gvar->type->array_len = slen;
                        gvar->type->size = type_size(gvar->type->base) * slen;
                    }
                }
            }

            /* store for codegen */
            if (cc->num_globals < MAX_GLOBALS) {
                cc->globals[cc->num_globals] = gvar;
                cc->num_globals++;
            } else {
                if (!_warned_max_globals) { printf("zcc: warning: MAX_GLOBALS (%d) exceeded at %s:%d — subsequent globals silently discarded\n", MAX_GLOBALS, cc->filename ? cc->filename : "?", cc->tk_line); _warned_max_globals = 1; }
            }

            /* handle multiple declarators: int a, b; */
            while (cc->tk == TK_COMMA) {
                Node *gvar2;
                char name2[MAX_IDENT];
                Type *dtype2;
                Symbol *sym2;

                next_token(cc);
                dtype2 = parse_declarator(cc, base, name2);
                gvar2 = node_new(cc, ND_GLOBAL_VAR, line);
                strncpy(gvar2->name, name2, MAX_IDENT - 1);
                gvar2->type = dtype2;
                gvar2->is_extern = is_extern;
                gvar2->is_static = is_static;

                sym2 = scope_add(cc, name2, dtype2);
                sym2->is_global = 1;

                if (cc->tk == TK_ASSIGN) {
                    next_token(cc);
                    gvar2->initializer = parse_assign(cc);
                }

                if (cc->num_globals < MAX_GLOBALS) {
                    cc->globals[cc->num_globals] = gvar2;
                    cc->num_globals++;
                } else {
                    if (!_warned_max_globals) { printf("zcc: warning: MAX_GLOBALS (%d) exceeded at %s:%d — subsequent globals silently discarded\n", MAX_GLOBALS, cc->filename ? cc->filename : "?", cc->tk_line); _warned_max_globals = 1; }
                }
            }

            /* link */
            gvar->next = 0;
            if (!head) { head = gvar; tail = gvar; }
            else { tail->next = gvar; tail = gvar; }

            expect(cc, TK_SEMI);
            continue;
        }
    }

    return head;
}
/* ZKAEDI FORCE RENDER CACHE INVALIDATION */
