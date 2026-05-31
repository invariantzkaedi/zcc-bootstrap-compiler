#ifndef ZCC_AST_BRIDGE_H
/* Exclusively for standalone IDE analysis */
#include "part1.c"
#endif
#include "zcc_telemetry.h"

/* ================================================================ */
/* PARSER                                                            */
/* ================================================================ */

static Type *find_struct(Compiler *cc, char *tag);
static char g_current_namespace[1024] = {0};

static void resolve_cpp_identifiers(Compiler *cc) {
    while (cc->tk == TK_IDENT && peek_token(cc) == TK_COLON_COLON) {
        char merged_name[MAX_IDENT * 2];
        sprintf(merged_name, "%.120s_", cc->tk_text);
        next_token(cc); /* consume current identifier */
        next_token(cc); /* consume :: */
        if (cc->tk == TK_TILDE) {
            next_token(cc); /* consume ~ */
            strncat(merged_name, "destructor_", MAX_IDENT - strlen(merged_name) - 1);
        }
        if (cc->tk == TK_IDENT) {
            strncat(merged_name, cc->tk_text, MAX_IDENT - strlen(merged_name) - 1);
            strncpy(cc->tk_text, merged_name, MAX_IDENT - 1);
            cc->tk_text[MAX_IDENT - 1] = 0;
            cc->tk = TK_IDENT;
        } else {
            error(cc, "expected identifier after '::'");
            break;
        }
    }
}

static int is_type_token(Compiler *cc) {
    resolve_cpp_identifiers(cc);
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
        if (sym && sym->is_typedef) return 1;
        if (g_current_namespace[0]) {
            char lookup_tag[MAX_IDENT * 2];
            sprintf(lookup_tag, "%.120s_%.120s", g_current_namespace, cc->tk_text);
            if (find_struct(cc, lookup_tag)) return 1;
        }
        if (find_struct(cc, cc->tk_text)) return 1;
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
    }
}

/* ---------------------------------------------------------------- */
/* Parse struct/union body                                           */
/* ---------------------------------------------------------------- */

Type *parse_type(Compiler *cc);
Type *parse_declarator(Compiler *cc, Type *base, char *name_out);
static void parse_or_skip_gcc_attributes(Compiler *cc, Type *dtype);
static void inject_attribute_parser(Compiler *cc, Type *dtype);
static long long parse_const_expr_ternary(Compiler *cc);
static Node *ensure_type(Compiler *cc, Node *n, Type *ty);
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
static StructField *find_struct_member(Type *type, const char *name, int *out_offset);
static long long parse_const_expr_unary(Compiler *cc) {
    if (cc->tk == TK_IDENT && strcmp(cc->tk_text, "__builtin_offsetof") == 0) {
        next_token(cc);
        expect(cc, TK_LPAREN);
        Type *st = parse_type(cc);
        char dummy[128];
        st = parse_declarator(cc, st, dummy);
        expect(cc, TK_COMMA);
        char member[128];
        strncpy(member, cc->tk_text, 127);
        member[127] = 0;
        expect(cc, TK_IDENT);
        expect(cc, TK_RPAREN);
        int offset = 0;
        find_struct_member(st, member, &offset);
        return offset;
    }
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

static Type *parse_struct_or_union_body(Compiler *cc, Type *stype, int is_union);

static Type *parse_struct_or_union(Compiler *cc, int is_union) {
    Type *stype;
    char tag[MAX_IDENT];
    int has_tag;
    int local_is_packed = 0;
    int local_explicit_align = 0;
    Type attr_collector;

    has_tag = 0;
    tag[0] = 0;
    memset(&attr_collector, 0, sizeof(Type));

    /* Consume lexer-set pending attributes */
    if (cc->pending_packed) {
        local_is_packed = 1;
        cc->pending_packed = 0;
    }
    if (cc->pending_aligned_n > 0) {
        local_explicit_align = cc->pending_aligned_n;
        cc->pending_aligned_n = 0;
    }

    /* 1. Attributes immediately after struct/union keyword */
    if (cc->tk == TK_IDENT && 
        (strcmp(cc->tk_text, "__attribute__") == 0 || strcmp(cc->tk_text, "__attribute") == 0)) {
        parse_or_skip_gcc_attributes(cc, &attr_collector);
        if (attr_collector.is_packed) local_is_packed = 1;
        if (attr_collector.explicit_align > 0) local_explicit_align = attr_collector.explicit_align;
    }

    if (cc->tk == TK_IDENT) {
        if (g_current_namespace[0]) {
            sprintf(tag, "%.120s_%.120s", g_current_namespace, cc->tk_text);
        } else {
            strncpy(tag, cc->tk_text, MAX_IDENT - 1);
        }
        has_tag = 1;
        next_token(cc);
    }

    /* 2. Attributes after tag name, before opening brace */
    if (cc->tk == TK_IDENT && 
        (strcmp(cc->tk_text, "__attribute__") == 0 || strcmp(cc->tk_text, "__attribute") == 0)) {
        parse_or_skip_gcc_attributes(cc, &attr_collector);
        if (attr_collector.is_packed) local_is_packed = 1;
        if (attr_collector.explicit_align > 0) local_explicit_align = attr_collector.explicit_align;
    }

    /* forward reference or existing struct */
    if (cc->tk != TK_LBRACE) {
        if (has_tag) {
            stype = find_struct(cc, tag);
            if (stype) {
                if (local_is_packed) stype->is_packed = 1;
                if (local_explicit_align > 0) {
                    stype->explicit_align = local_explicit_align;
                    stype->align = local_explicit_align;
                }
                return stype;
            }
            /* forward declaration */
            stype = type_new(cc, is_union ? TY_UNION : TY_STRUCT);
            strncpy(stype->tag, tag, MAX_IDENT - 1);
            stype->is_complete = 0;
            if (local_is_packed) stype->is_packed = 1;
            if (local_explicit_align > 0) {
                stype->explicit_align = local_explicit_align;
                stype->align = local_explicit_align;
            }
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
        if (cc->pending_tbfp) {
            stype->is_tbfp = 1;
            cc->pending_tbfp = 0;
        }
        if (has_tag) {
            strncpy(stype->tag, tag, MAX_IDENT - 1);
            register_struct(cc, stype);
        }
    } else if (cc->pending_tbfp) {
        stype->is_tbfp = 1;
        cc->pending_tbfp = 0;
    }

    if (stype) {
        if (local_is_packed) stype->is_packed = 1;
        if (local_explicit_align > 0) {
            stype->explicit_align = local_explicit_align;
            stype->align = local_explicit_align;
        }
    }

    return parse_struct_or_union_body(cc, stype, is_union);
}

static int get_member_size(Type *stype, Type *ftype) {
    if (stype && stype->is_tbfp) {
        if (ftype->kind == TY_FLOAT) return 2;
        if (ftype->kind == TY_DOUBLE) return 5;
    }
    return type_size(ftype);
}

static Type *parse_struct_or_union_body(Compiler *cc, Type *stype, int is_union) {
    expect(cc, TK_LBRACE);

    {
        StructField *last_field;
        int offset;
        int max_size;
        int max_align;
        int bf_active;
        int bf_unit_size;
        int bf_current_bit;
        int bf_unit_offset;

        last_field = 0;
        offset = 0;
        max_size = 0;
        max_align = 1;
        bf_active = 0;
        bf_unit_size = 0;
        bf_current_bit = 0;
        bf_unit_offset = 0;

        while (cc->tk != TK_RBRACE) {
            Type *ftype;
            Type *base_ftype;
            char fname[MAX_IDENT];
            StructField *field;
            int falign;

            if (cc->tk == TK_EOF) break;

            if (cc->tk == TK_SEMI) {
                next_token(cc);
                continue;
            }

            if (cc->tk == TK_PUBLIC || cc->tk == TK_PRIVATE || cc->tk == TK_PROTECTED) {
                next_token(cc); /* consume public/private/protected */
                expect(cc, TK_COLON); /* consume : */
                continue;
            }

            if (cc->tk == TK_IDENT && strcmp(cc->tk_text, stype->tag) == 0 && peek_token(cc) == TK_LPAREN) {
                next_token(cc); /* consume constructor name */
                expect(cc, TK_LPAREN);
                int depth = 1;
                while (depth > 0 && cc->tk != TK_EOF) {
                    if (cc->tk == TK_LPAREN) depth++;
                    else if (cc->tk == TK_RPAREN) depth--;
                    next_token(cc);
                }
                if (cc->tk == TK_COLON) {
                    next_token(cc);
                    while (cc->tk != TK_LBRACE && cc->tk != TK_SEMI && cc->tk != TK_EOF) {
                        next_token(cc);
                    }
                }
                if (cc->tk == TK_LBRACE) {
                    int bdepth = 1;
                    next_token(cc);
                    while (bdepth > 0 && cc->tk != TK_EOF) {
                        if (cc->tk == TK_LBRACE) bdepth++;
                        else if (cc->tk == TK_RBRACE) bdepth--;
                        next_token(cc);
                    }
                } else if (cc->tk == TK_SEMI) {
                    next_token(cc);
                }
                continue;
            }

            if (cc->tk == TK_TILDE) {
                next_token(cc); /* consume ~ */
                if (cc->tk == TK_IDENT && strcmp(cc->tk_text, stype->tag) == 0) {
                    next_token(cc);
                    expect(cc, TK_LPAREN);
                    expect(cc, TK_RPAREN);
                    if (cc->tk == TK_LBRACE) {
                        int bdepth = 1;
                        next_token(cc);
                        while (bdepth > 0 && cc->tk != TK_EOF) {
                            if (cc->tk == TK_LBRACE) bdepth++;
                            else if (cc->tk == TK_RBRACE) bdepth--;
                            next_token(cc);
                        }
                    } else if (cc->tk == TK_SEMI) {
                        next_token(cc);
                    }
                    continue;
                }
            }

            if (cc->tk == TK_OPERATOR) {
                next_token(cc); /* consume operator */
                while (cc->tk != TK_LPAREN && cc->tk != TK_EOF) {
                    next_token(cc);
                }
                expect(cc, TK_LPAREN);
                int depth = 1;
                while (depth > 0 && cc->tk != TK_EOF) {
                    if (cc->tk == TK_LPAREN) depth++;
                    else if (cc->tk == TK_RPAREN) depth--;
                    next_token(cc);
                }
                if (cc->tk == TK_LBRACE) {
                    int bdepth = 1;
                    next_token(cc);
                    while (bdepth > 0 && cc->tk != TK_EOF) {
                        if (cc->tk == TK_LBRACE) bdepth++;
                        else if (cc->tk == TK_RBRACE) bdepth--;
                        next_token(cc);
                    }
                } else if (cc->tk == TK_SEMI) {
                    next_token(cc);
                }
                continue;
            }

            base_ftype = parse_type(cc);

            if (cc->tk == TK_OPERATOR) {
                next_token(cc); /* consume operator */
                while (cc->tk != TK_LPAREN && cc->tk != TK_EOF) {
                    next_token(cc);
                }
                expect(cc, TK_LPAREN);
                int depth = 1;
                while (depth > 0 && cc->tk != TK_EOF) {
                    if (cc->tk == TK_LPAREN) depth++;
                    else if (cc->tk == TK_RPAREN) depth--;
                    next_token(cc);
                }
                if (cc->tk == TK_LBRACE) {
                    int bdepth = 1;
                    next_token(cc);
                    while (bdepth > 0 && cc->tk != TK_EOF) {
                        if (cc->tk == TK_LBRACE) bdepth++;
                        else if (cc->tk == TK_RBRACE) bdepth--;
                        next_token(cc);
                    }
                } else if (cc->tk == TK_SEMI) {
                    next_token(cc);
                }
                continue;
            }

            ftype = parse_declarator(cc, base_ftype, fname);

            if (ftype && ftype->kind == TY_FUNC && cc->tk == TK_LBRACE) {
                /* skip function body */
                int depth = 0;
                next_token(cc); /* consume { */
                depth = 1;
                while (depth > 0 && cc->tk != TK_EOF) {
                    if (cc->tk == TK_LBRACE) depth++;
                    else if (cc->tk == TK_RBRACE) depth--;
                    next_token(cc);
                }
                continue;
            }

            int is_bf = 0;
            int bf_size = 0;
            if (cc->tk == TK_COLON) {
                next_token(cc);
                bf_size = (int)parse_const_expr(cc);
                is_bf = 1;
            }

            field = (StructField *)cc_alloc(cc, sizeof(StructField));
            strncpy(field->name, fname, MAX_IDENT - 1);
            field->type = ftype;
            field->is_bitfield = 0;
            field->bit_offset = 0;
            field->bit_size = 0;

            falign = type_align(ftype);
            if (stype && stype->is_packed) falign = 1;
            if (falign > max_align) max_align = falign;

            if (is_union) {
                field->offset = 0;
                if (is_bf) {
                    field->is_bitfield = 1;
                    field->bit_size = bf_size;
                }
                if (type_size(ftype) > max_size) max_size = type_size(ftype);
            } else {
                if (is_bf) {
                    int fsize = type_size(ftype);
                    if (bf_active && fsize == bf_unit_size && bf_size > 0 && bf_current_bit + bf_size <= fsize * 8) {
                        field->offset = bf_unit_offset;
                        field->is_bitfield = 1;
                        field->bit_offset = bf_current_bit;
                        field->bit_size = bf_size;
                        bf_current_bit += bf_size;
                    } else {
                        bf_active = 1;
                        bf_unit_size = fsize;
                        bf_current_bit = 0;
                        if (falign > 1) {
                            offset = (offset + falign - 1) & ~(falign - 1);
                        }
                        bf_unit_offset = offset;
                        if (bf_size > 0) {
                            field->offset = bf_unit_offset;
                            field->is_bitfield = 1;
                            field->bit_offset = 0;
                            field->bit_size = bf_size;
                            bf_current_bit = bf_size;
                            offset += fsize;
                        } else {
                            bf_active = 0;
                            bf_unit_size = 0;
                            bf_current_bit = 0;
                        }
                    }
                } else {
                    bf_active = 0;
                    if (falign > 1) {
                        offset = (offset + falign - 1) & ~(falign - 1);
                    }
                    field->offset = offset;
                    offset = offset + type_size(ftype);
                }
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

                int is_bf2 = 0;
                int bf_size2 = 0;
                if (cc->tk == TK_COLON) {
                    next_token(cc);
                    bf_size2 = (int)parse_const_expr(cc);
                    is_bf2 = 1;
                }

                field2 = (StructField *)cc_alloc(cc, sizeof(StructField));
                strncpy(field2->name, fname2, MAX_IDENT - 1);
                field2->type = ftype2;
                field2->is_bitfield = 0;
                field2->bit_offset = 0;
                field2->bit_size = 0;

                falign2 = type_align(ftype2);
                if (stype && stype->is_packed) falign2 = 1;
                if (falign2 > max_align) max_align = falign2;

                if (is_union) {
                    field2->offset = 0;
                    if (is_bf2) {
                        field2->is_bitfield = 1;
                        field2->bit_size = bf_size2;
                    }
                    if (type_size(ftype2) > max_size) max_size = type_size(ftype2);
                } else {
                    if (is_bf2) {
                        int fsize2 = type_size(ftype2);
                        if (bf_active && fsize2 == bf_unit_size && bf_size2 > 0 && bf_current_bit + bf_size2 <= fsize2 * 8) {
                            field2->offset = bf_unit_offset;
                            field2->is_bitfield = 1;
                            field2->bit_offset = bf_current_bit;
                            field2->bit_size = bf_size2;
                            bf_current_bit += bf_size2;
                        } else {
                            bf_active = 1;
                            bf_unit_size = fsize2;
                            bf_current_bit = 0;
                            if (falign2 > 1) {
                                offset = (offset + falign2 - 1) & ~(falign2 - 1);
                            }
                            bf_unit_offset = offset;
                            if (bf_size2 > 0) {
                                field2->offset = bf_unit_offset;
                                field2->is_bitfield = 1;
                                field2->bit_offset = 0;
                                field2->bit_size = bf_size2;
                                bf_current_bit = bf_size2;
                                offset += fsize2;
                            } else {
                                bf_active = 0;
                                bf_unit_size = 0;
                                bf_current_bit = 0;
                            }
                        }
                    } else {
                        bf_active = 0;
                        if (falign2 > 1) {
                            offset = (offset + falign2 - 1) & ~(falign2 - 1);
                        }
                        field2->offset = offset;
                        offset = offset + type_size(ftype2);
                    }
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
        
        int final_align = max_align;
        if (stype->explicit_align > 0) {
            final_align = stype->explicit_align;
        } else if (stype->is_packed) {
            final_align = 1;
        }
        
        /* align total size */
        if (final_align > 1) {
            stype->size = (stype->size + final_align - 1) & ~(final_align - 1);
        }
        stype->align = final_align;
        stype->is_complete = 1;
    }

    expect(cc, TK_RBRACE);
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
    resolve_cpp_identifiers(cc);
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
            inject_attribute_parser(cc, type);
        }
        else if (cc->tk == TK_UNION) {
            next_token(cc);
            type = parse_struct_or_union(cc, 1);
            inject_attribute_parser(cc, type);
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
            /* could be a typedef name or a struct/class tag */
            Symbol *sym;
            sym = scope_find(cc, cc->tk_text);
            if (strcmp(cc->tk_text, "yyParser") == 0) {
                printf("DEBUG: parse_type lookup 'yyParser'. sym=%p\n", (void*)sym);
            }
            if (sym && sym->is_typedef) {
                type = sym->type;
                next_token(cc);
            } else {
                Type *st = 0;
                if (g_current_namespace[0]) {
                    char lookup_tag[MAX_IDENT * 2];
                    sprintf(lookup_tag, "%.120s_%.120s", g_current_namespace, cc->tk_text);
                    st = find_struct(cc, lookup_tag);
                }
                if (!st) {
                    st = find_struct(cc, cc->tk_text);
                }
                if (st) {
                    type = st;
                    next_token(cc);
                }
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

static long long eval_const_expr(Node *n) {
    if (!n) return 0;
    if (n->kind == ND_NUM) return n->int_val;
    if (n->kind == ND_ADD) return eval_const_expr(n->lhs) + eval_const_expr(n->rhs);
    if (n->kind == ND_SUB) return eval_const_expr(n->lhs) - eval_const_expr(n->rhs);
    if (n->kind == ND_MUL) return eval_const_expr(n->lhs) * eval_const_expr(n->rhs);
    if (n->kind == ND_DIV) {
        long long r = eval_const_expr(n->rhs);
        return r ? eval_const_expr(n->lhs) / r : 0;
    }
    if (n->kind == ND_MOD) {
        long long r = eval_const_expr(n->rhs);
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
    /* Relational and logical operators (commit 3cc64db6 — re-applied from history) */
    if (n->kind == ND_EQ)   return eval_const_expr(n->lhs) == eval_const_expr(n->rhs);
    if (n->kind == ND_NE)   return eval_const_expr(n->lhs) != eval_const_expr(n->rhs);
    if (n->kind == ND_LT)   return eval_const_expr(n->lhs) <  eval_const_expr(n->rhs);
    if (n->kind == ND_LE)   return eval_const_expr(n->lhs) <= eval_const_expr(n->rhs);
    if (n->kind == ND_GT)   return eval_const_expr(n->lhs) >  eval_const_expr(n->rhs);
    if (n->kind == ND_GE)   return eval_const_expr(n->lhs) >= eval_const_expr(n->rhs);
    if (n->kind == ND_LAND) return eval_const_expr(n->lhs) && eval_const_expr(n->rhs);
    if (n->kind == ND_LOR)  return eval_const_expr(n->lhs) || eval_const_expr(n->rhs);
    /* Ternary constant folding — enables (expr==0 ? 1 : expr) denominator patterns */
    if (n->kind == ND_TERNARY && n->cond && n->then_body && n->else_body)
        return eval_const_expr(n->cond) ? eval_const_expr(n->then_body)
                                        : eval_const_expr(n->else_body);
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

static void parse_or_skip_gcc_attributes(Compiler *cc, Type *dtype) {
    int loop_guard = 0;
    while (cc->tk == TK_IDENT && 
           (strcmp(cc->tk_text, "__attribute__") == 0 || strcmp(cc->tk_text, "__attribute") == 0)) {
        next_token(cc); /* consume attribute */
        
        if (cc->tk == TK_LPAREN) {
            next_token(cc);
            if (cc->tk == TK_LPAREN) {
                next_token(cc);
                
                while (cc->tk != TK_RPAREN && cc->tk != TK_EOF) {
                    if (loop_guard++ > 500) {
                        error(cc, "loop limit exceeded in attribute parser");
                        return;
                    }
                    
                    if (cc->tk == TK_IDENT) {
                        if (strcmp(cc->tk_text, "packed") == 0 || strcmp(cc->tk_text, "__packed__") == 0) {
                            if (dtype) {
                                dtype->is_packed = 1;
                            }
                            next_token(cc);
                        } else if (strcmp(cc->tk_text, "aligned") == 0 || strcmp(cc->tk_text, "__aligned__") == 0) {
                            next_token(cc);
                            int align_val = 0;
                            if (cc->tk == TK_LPAREN) {
                                next_token(cc);
                                if (cc->tk == TK_NUM) {
                                    align_val = cc->tk_val;
                                    next_token(cc);
                                }
                                expect(cc, TK_RPAREN);
                            }
                            if (dtype) {
                                if (align_val > 0) {
                                    dtype->explicit_align = align_val;
                                    dtype->align = align_val;
                                } else {
                                    /* default aligned to 8 */
                                    dtype->explicit_align = 8;
                                    dtype->align = 8;
                                }
                            }
                        } else {
                            /* skip generic parameter or single identifiers like unused, format */
                            next_token(cc);
                            if (cc->tk == TK_LPAREN) {
                                int depth = 1;
                                next_token(cc);
                                while (depth > 0 && cc->tk != TK_EOF) {
                                    if (cc->tk == TK_LPAREN) depth++;
                                    else if (cc->tk == TK_RPAREN) depth--;
                                    next_token(cc);
                                }
                            }
                        }
                    } else if (cc->tk == TK_COMMA) {
                        next_token(cc);
                    } else {
                        next_token(cc);
                    }
                }
                if (cc->tk == TK_EOF) {
                    error(cc, "attribute block cut off by EOF");
                    return;
                }
                expect(cc, TK_RPAREN);
            }
            if (cc->tk == TK_EOF) {
                error(cc, "attribute block cut off by EOF");
                return;
            }
            expect(cc, TK_RPAREN);
        }
    }
}

static void inject_attribute_parser(Compiler *cc, Type *dtype) {
    if (cc->pending_packed) {
        if (dtype) dtype->is_packed = 1;
        cc->pending_packed = 0;
    }
    if (cc->pending_aligned_n > 0) {
        if (dtype) {
            dtype->explicit_align = cc->pending_aligned_n;
            dtype->align = cc->pending_aligned_n;
        }
        cc->pending_aligned_n = 0;
    }
    if (cc->tk == TK_IDENT &&
        (strcmp(cc->tk_text, "__attribute__") == 0 || strcmp(cc->tk_text, "__attribute") == 0)) {
        parse_or_skip_gcc_attributes(cc, dtype);
    }
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

    /* attributes on pointers/declarator before identifier name */
    inject_attribute_parser(cc, type);

    /* name */
    resolve_cpp_identifiers(cc);
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
            if (cc->tk == TK_EOF) { error(cc, "unexpected EOF in nested declarator"); return NULL; }
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
            inject_attribute_parser(cc, type);
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

    inject_attribute_parser(cc, type);
    return type;
}

/* ================================================================ */
/* EXPRESSION PARSING — FULL PRECEDENCE CHAIN                       */
/* ================================================================ */

/* --- primary --- */

Node *parse_primary(Compiler *cc) {
    resolve_cpp_identifiers(cc);
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
        n = node_flit(cc, cc->tk_fval, line);
        if (cc->tk_text[0] == 'F') {
            n->type = cc->ty_float;
        }
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
            se->label_id = cc->label_count;
            cc->label_count++;
            cc->num_strings++;
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
                /* ZCC SCOPING HARDENING: undeclared identifiers must be rejected    */
                /* at AST build time. The old silent int decay caused CWE-476 healer */
                /* mutations (guards inserted before decl) to survive bootstrap,     */
                /* masking invalid C as applied_survived in the mutation log.        */
                char _ud_buf[256];
                sprintf(_ud_buf, "use of undeclared identifier '%s'", n->name);
                error(cc, _ud_buf);
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

static Node *apply_default_promotions(Compiler *cc, Node *arg) {
    if (!arg || !arg->type) return arg;
    int k = arg->type->kind;
    Type *promo = 0;
    if (k == TY_CHAR || k == TY_UCHAR || k == TY_SHORT || k == TY_USHORT) {
        promo = cc->ty_int;
    } else if (k == TY_FLOAT) {
        promo = cc->ty_double;
    }
    if (promo && arg->type != promo) {
        Node *c = node_new(cc, ND_CAST, arg->line);
        c->lhs = arg;
        c->cast_type = promo;
        c->type = promo;
        return c;
    }
    return arg;
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
                    member->is_bitfield = f->is_bitfield;
                    member->bit_offset = f->bit_offset;
                    member->bit_size = f->bit_size;
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
                    n->is_bitfield = f->is_bitfield;
                    n->bit_offset = f->bit_offset;
                    n->bit_size = f->bit_size;
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
                            /* ZKAEDI: Implicitly coerce arguments to parameter types */
                            for (int k = 0; k < call->num_args; k++) {
                                if (k < sym->type->num_params && sym->type->params && sym->type->params[k]) {
                                    Type *pty = sym->type->params[k];
                                    if (call->args[k] && call->args[k]->type && call->args[k]->type != pty) {
                                        Node *c = node_new(cc, ND_CAST, line);
                                        c->lhs = call->args[k];
                                        c->cast_type = pty;
                                        c->type = pty;
                                        call->args[k] = c;
                                    }
                                } else {
                                    call->args[k] = apply_default_promotions(cc, call->args[k]);
                                }
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
            if (!is_direct) {
                /* indirect call through any expression (function pointer, comma expr, deref, etc) */
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
                        for (int k = 0; k < call->num_args; k++) {
                            if (k < n->type->num_params && n->type->params && n->type->params[k]) {
                                Type *pty = n->type->params[k];
                                if (call->args[k] && call->args[k]->type && call->args[k]->type != pty) {
                                    Node *c = node_new(cc, ND_CAST, line);
                                    c->lhs = call->args[k];
                                    c->cast_type = pty;
                                    c->type = pty;
                                    call->args[k] = c;
                                }
                            } else {
                                call->args[k] = apply_default_promotions(cc, call->args[k]);
                            }
                        }
                    } else if (n->type->kind == TY_PTR && n->type->base && n->type->base->kind == TY_FUNC) {
                        call->type = n->type->base->ret ? n->type->base->ret : cc->ty_int;
                        for (int k = 0; k < call->num_args; k++) {
                            if (k < n->type->base->num_params && n->type->base->params && n->type->base->params[k]) {
                                Type *pty = n->type->base->params[k];
                                if (call->args[k] && call->args[k]->type && call->args[k]->type != pty) {
                                    Node *c = node_new(cc, ND_CAST, line);
                                    c->lhs = call->args[k];
                                    c->cast_type = pty;
                                    c->type = pty;
                                    call->args[k] = c;
                                }
                            } else {
                                call->args[k] = apply_default_promotions(cc, call->args[k]);
                            }
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

    if (cc->tk == TK_STATIC_CAST || cc->tk == TK_CONST_CAST || cc->tk == TK_REINTERPRET_CAST) {
        Type *cast_type;
        char dummy[MAX_IDENT];
        next_token(cc); /* consume static_cast etc */
        expect(cc, TK_LT); /* consume < */
        cast_type = parse_type(cc);
        cast_type = parse_declarator(cc, cast_type, dummy);
        expect(cc, TK_GT); /* consume > */
        
        expect(cc, TK_LPAREN); /* consume ( */
        n = node_new(cc, ND_CAST, line);
        n->lhs = parse_expr(cc);
        expect(cc, TK_RPAREN); /* consume ) */
        
        n->type = cast_type;
        n->cast_type = cast_type;
        return n;
    }

    if (cc->tk == TK_IDENT && strcmp(cc->tk_text, "__builtin_offsetof") == 0) {
        next_token(cc);
        expect(cc, TK_LPAREN);
        Type *st = parse_type(cc);
        char dummy[128];
        st = parse_declarator(cc, st, dummy);
        expect(cc, TK_COMMA);
        char member[128];
        strncpy(member, cc->tk_text, 127);
        member[127] = 0;
        expect(cc, TK_IDENT);
        expect(cc, TK_RPAREN);
        int offset = 0;
        find_struct_member(st, member, &offset);
        n = node_new(cc, ND_NUM, line);
        n->type = cc->ty_int; // size_t is unsigned long, but ty_int is fine for offset
        n->int_val = offset;
        return n;
    }

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
            if (expr_n && expr_n->kind == ND_STR) {
                sz = cc->strings[expr_n->str_id].len + 1;
            } else if (expr_n && expr_n->type) {
                t = expr_n->type;
            }
        }
        if (sz == 0 && t) sz = type_size(t);
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
                sprintf(tmp_name, ".L_comp_%d", cc->label_count++);
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
                        mem_n->is_bitfield = sf->is_bitfield;
                        mem_n->bit_offset = sf->bit_offset;
                        mem_n->bit_size = sf->bit_size;

                        Node *asgn_v = parse_assign(cc);
                        Node *asgn_n = node_new(cc, ND_ASSIGN, line);
                        asgn_n->lhs = mem_n;
                        asgn_n->rhs = ensure_type(cc, asgn_v, mem_n->type);
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
                        asgn_n->rhs = ensure_type(cc, asgn_v, cast_type->base);
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
                    asgn_n->rhs = ensure_type(cc, asgn_v, cast_type);
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

static Type *integer_promotion(Compiler *cc, Type *ty) {
    if (!ty) return ty;
    if (ty->kind == TY_CHAR || ty->kind == TY_UCHAR || ty->kind == TY_SHORT || ty->kind == TY_USHORT || ty->kind == TY_ENUM) {
        return cc->ty_int;
    }
    return ty;
}

static Type *usual_arith_conv(Compiler *cc, Type *t1, Type *t2) {
    if (!t1) return t2;
    if (!t2) return t1;
    if (t1->kind == TY_DOUBLE || t2->kind == TY_DOUBLE) return cc->ty_double;
    if (t1->kind == TY_FLOAT || t2->kind == TY_FLOAT) return cc->ty_float;
    if (t1->kind == TY_PTR) return t1;
    if (t2->kind == TY_PTR) return t2;

    t1 = integer_promotion(cc, t1);
    t2 = integer_promotion(cc, t2);

    int sz1 = type_size(t1);
    int sz2 = type_size(t2);

    if (sz1 == sz2 && is_unsigned_type(t1) == is_unsigned_type(t2)) return t1;

    if (sz1 >= 8 || sz2 >= 8) {
        if (sz1 == sz2) {
            return is_unsigned_type(t1) ? t1 : t2;
        }
        Type *larger = (sz1 > sz2) ? t1 : t2;
        Type *smaller = (sz1 > sz2) ? t2 : t1;
        if (is_unsigned_type(larger) || !is_unsigned_type(smaller)) {
            return larger;
        }
        if (type_size(larger) > type_size(smaller)) {
            return larger;
        }
        return cc->ty_ulong;
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
        pt = usual_arith_conv(cc, n->type, rhs->type);
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
            pt = usual_arith_conv(cc, n->type, rhs->type);
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
        Type *pt = integer_promotion(cc, n->type);
        n = ensure_type(cc, n, pt);
        rhs = ensure_type(cc, rhs, integer_promotion(cc, rhs->type));
        binop = node_new(cc, op, line);
        binop->lhs = n;
        binop->rhs = rhs;
        binop->type = pt;
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
        pt = usual_arith_conv(cc, n->type, rhs->type);
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
        pt = usual_arith_conv(cc, n->type, rhs->type);
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
        pt = usual_arith_conv(cc, n->type, rhs->type);
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
        pt = usual_arith_conv(cc, n->type, rhs->type);
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
        pt = usual_arith_conv(cc, n->type, rhs->type);
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
        tern->type = usual_arith_conv(cc, tern->then_body->type, tern->else_body->type);
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

Node *parse_expr_internal(Compiler *cc) {
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

Node *parse_expr(Compiler *cc) {
    Node *n;
    cc->telemetry_depth++;
    telemetry_emit_node(AST_ENTER_NODE, "expr", cc->telemetry_depth);
    n = parse_expr_internal(cc);
    telemetry_emit_node(AST_EXIT_NODE, "expr", cc->telemetry_depth);
    cc->telemetry_depth--;
    return n;
}

/* ================================================================ */
/* STATEMENT PARSING                                                 */
/* ================================================================ */

static Node *current_sw = 0;

static Node *parse_initializer_list(Compiler *cc, int *out_count) {
    Node *list = node_new(cc, ND_INIT_LIST, cc->line);
    int cap = 16;
    list->args = (Node **)cc_alloc(cc, sizeof(Node *) * cap);
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
        if (cc->tk == TK_LBRACE) {
            item = parse_initializer_list(cc, NULL);
        } else {
            item = parse_assign(cc);
        }
        
        if (list->num_args >= cap) {
            Node **old_args = list->args;
            cap *= 2;
            list->args = (Node **)cc_alloc(cc, sizeof(Node *) * cap);
            for (int i = 0; i < list->num_args; i++) {
                list->args[i] = old_args[i];
            }
        }
        list->args[list->num_args++] = item;
        
        if (cc->tk == TK_COMMA) {
            next_token(cc);
            if (cc->tk == TK_RBRACE) {
                break;
            }
        } else {
            break;
        }
    }
    
    if (cc->tk == TK_RBRACE) {
        next_token(cc);
    } else {
        error(cc, "expected '}' in initializer list");
    }
    if (out_count) *out_count = list->num_args;
    return list;
}

static void emit_local_initializer(Compiler *cc, Node *block, int *cnt, int *cap, Node *base_var, Type *curr_type, Node *init_list, int offset) {
    if (curr_type->kind == TY_ARRAY) {
        Type *elem_type = curr_type->base;
        int elem_sz = type_size(elem_type);
        for (int i = 0; i < init_list->num_args; i++) {
            Node *item = init_list->args[i];
            if (item->kind == ND_INIT_LIST) {
                emit_local_initializer(cc, block, cnt, cap, base_var, elem_type, item, offset + i * elem_sz);
            } else {
                /* Cast base to char* so byte offset is not double-scaled */
                Node *cast_base = node_new(cc, ND_CAST, cc->line);
                cast_base->lhs = base_var;
                cast_base->cast_type = type_ptr(cc, cc->ty_char);
                cast_base->type = type_ptr(cc, cc->ty_char);

                Node *add = node_new(cc, ND_ADD, cc->line);
                add->lhs = cast_base;
                add->rhs = node_num(cc, offset + i * elem_sz, cc->line);
                add->type = type_ptr(cc, cc->ty_char);

                /* Cast result back to elem_type* for correct deref */
                Node *cast_back = node_new(cc, ND_CAST, cc->line);
                cast_back->lhs = add;
                cast_back->cast_type = type_ptr(cc, elem_type);
                cast_back->type = type_ptr(cc, elem_type);
                
                Node *deref = node_new(cc, ND_DEREF, cc->line);
                deref->lhs = cast_back;
                deref->type = elem_type;
                
                Node *asgn = node_new(cc, ND_ASSIGN, cc->line);
                asgn->lhs = deref;
                asgn->rhs = ensure_type(cc, item, elem_type);
                asgn->type = elem_type;
                
                if (*cnt >= *cap) {
                    Node **new_stmts = (Node **)cc_alloc(cc, sizeof(Node *) * (*cap) * 2);
                    for (int j = 0; j < *cnt; j++) new_stmts[j] = block->stmts[j];
                    block->stmts = new_stmts;
                    *cap = (*cap) * 2;
                }
                block->stmts[(*cnt)++] = asgn;
            }
        }
    } else if (curr_type->kind == TY_STRUCT || curr_type->kind == TY_UNION) {
        StructField *f = curr_type->fields;
        for (int i = 0; i < init_list->num_args && f != NULL; i++, f = f->next) {
            Node *item = init_list->args[i];
            if (item->kind == ND_INIT_LIST) {
                emit_local_initializer(cc, block, cnt, cap, base_var, f->type, item, offset + f->offset);
            } else {
                /* Cast base to char* so byte offset is not double-scaled */
                Node *cast_base = node_new(cc, ND_CAST, cc->line);
                cast_base->lhs = base_var;
                cast_base->cast_type = type_ptr(cc, cc->ty_char);
                cast_base->type = type_ptr(cc, cc->ty_char);

                Node *add = node_new(cc, ND_ADD, cc->line);
                add->lhs = cast_base;
                add->rhs = node_num(cc, offset + f->offset, cc->line);
                add->type = type_ptr(cc, cc->ty_char);

                /* Cast result back to field type* for correct deref */
                Node *cast_back = node_new(cc, ND_CAST, cc->line);
                cast_back->lhs = add;
                cast_back->cast_type = type_ptr(cc, f->type);
                cast_back->type = type_ptr(cc, f->type);
                
                Node *deref = node_new(cc, ND_DEREF, cc->line);
                deref->lhs = cast_back;
                deref->type = f->type;
                
                Node *asgn = node_new(cc, ND_ASSIGN, cc->line);
                asgn->lhs = deref;
                asgn->rhs = ensure_type(cc, item, f->type);
                asgn->type = f->type;
                
                if (*cnt >= *cap) {
                    Node **new_stmts = (Node **)cc_alloc(cc, sizeof(Node *) * (*cap) * 2);
                    for (int j = 0; j < *cnt; j++) new_stmts[j] = block->stmts[j];
                    block->stmts = new_stmts;
                    *cap = (*cap) * 2;
                }
                block->stmts[(*cnt)++] = asgn;
            }
        }
    }
}

Node *parse_stmt_internal(Compiler *cc) {
    int line;
    line = cc->tk_line;

    if (cc->tk == TK_USING) {
        while (cc->tk != TK_SEMI && cc->tk != TK_EOF) {
            next_token(cc);
        }
        if (cc->tk == TK_SEMI) {
            next_token(cc);
        }
        return node_new(cc, ND_NOP, line);
    }

    /* Skip __asm__(...) / asm(...) / __asm(...) statements */
    if (cc->tk == TK_ASM || (cc->tk == TK_IDENT && (
        strcmp(cc->tk_text, "__asm__") == 0 ||
        strcmp(cc->tk_text, "asm") == 0 ||
        strcmp(cc->tk_text, "__asm") == 0))) {
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
            ret->lhs = parse_expr(cc);
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
        sprintf(cn->label_name, "__zc_%llx_%d", (unsigned long long)val, line);
        cn->lhs = parse_stmt(cc);

        if (current_sw && current_sw->num_cases < MAX_CASES) {
            cnode = node_new(cc, ND_CASE, line);
            cnode->case_val = val;
            gto = node_new(cc, ND_GOTO, line);
            strncpy(gto->label_name, cn->label_name, MAX_IDENT - 1);
            cnode->case_body = gto;
            current_sw->cases[current_sw->num_cases++] = cnode;
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
        sprintf(dn->label_name, "__zc_def_%d", line);
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
                        sprintf(mangled, "%s__%s__%d", cc->current_func, vname, cc->label_count++);
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
                                int top_elems = 0;
                                Node *init_list = parse_initializer_list(cc, &top_elems);
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

                        if (cc->num_globals < MAX_GLOBALS) cc->globals[cc->num_globals++] = gvar;
                    } else {
                        sym = scope_add_local(cc, vname, vtype);

                        /* initializer? */
                        if (cc->tk == TK_ASSIGN) {
                            Node *asgn;
                            Node *var;
                            next_token(cc);

                        /* check for array/struct initializer list */
                        if (cc->tk == TK_LBRACE) {
                            int top_elems = 0;
                            Node *init_list = parse_initializer_list(cc, &top_elems);
                            Type *elem_type = NULL;
                            Type *arr_type = vtype;
                            if (vtype->kind == TY_ARRAY) {
                                elem_type = vtype->base;
                                if (vtype->array_len == 0) {
                                    arr_type = type_array(cc, elem_type, top_elems);
                                    sym->type = arr_type;
                                    int sz = type_size(arr_type);
                                    if (sz < 8) sz = 8;
                                    cc->local_offset = cc->local_offset + 8;
                                    cc->local_offset = cc->local_offset - sz;
                                    sym->stack_offset = cc->local_offset;
                                }
                            }
                            
                            if (vtype->kind == TY_ARRAY || vtype->kind == TY_STRUCT || vtype->kind == TY_UNION) {
                                Node *var_node = node_new(cc, ND_VAR, line);
                                strncpy(var_node->name, vname, MAX_IDENT - 1);
                                var_node->sym = sym;
                                var_node->type = arr_type;
                                emit_local_initializer(cc, block, &cnt, &cap, var_node, arr_type, init_list, 0);
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

Node *parse_stmt(Compiler *cc) {
    Node *n;
    cc->telemetry_depth++;
    telemetry_emit_node(AST_ENTER_NODE, "stmt", cc->telemetry_depth);
    n = parse_stmt_internal(cc);
    telemetry_emit_node(AST_EXIT_NODE, "stmt", cc->telemetry_depth);
    cc->telemetry_depth--;
    return n;
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
                    func->param_types[func->num_params] = ptype;
                    strncpy(func->param_names_buf[func->num_params], pname, MAX_IDENT - 1);
                    psym = scope_add_local(cc, pname, ptype);
                    func->num_params++;
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
                        error(cc, "expected ',' or ')' after parameter");
                    }
                    while (cc->tk != TK_RPAREN && cc->tk != TK_EOF) next_token(cc);
                    break;
                }
            }
        }
    }
    expect(cc, TK_RPAREN);

    /* Populate func->func_params from the inline capture arrays so
     * codegen_func (part4.c) can safely do func->func_params->types[i].
     * cc_alloc uses the arena so this is lifetime-safe. */
    {
        struct FuncParams *fp = (struct FuncParams *)cc_alloc(cc, sizeof(struct FuncParams));
        int k;
        for (k = 0; k < func->num_params && k < MAX_PARAMS; k++) {
            fp->types[k] = func->param_types[k];
            strncpy(fp->names[k], func->param_names_buf[k], MAX_IDENT - 1);
            fp->names[k][MAX_IDENT - 1] = '\0';
        }
        func->func_params = fp;
    }

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
                ftype->params[k] = func->param_types[k];
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

#ifndef MAP_SHARED
#define MAP_SHARED 1
#endif
extern void *mmap(void *addr, size_t length, int prot, int flags, int fd, size_t offset);
extern int shm_open(const char *name, int oflag, int mode);
extern int close(int fd);

/* ================================================================ */
/* Flat Wire → ND_INIT_LIST Converter (Zero Deserialization)        */
/* ================================================================ */


static Node* parse_value_from_wire(Compiler *cc, const uint8_t *ptr) {
    const struct WireInitNode *w = (const struct WireInitNode *)ptr;
    const uint8_t *payload = ptr + sizeof(struct WireInitNode);

    Node *n = NULL;
    switch (w->u.value.vkind) {
        case 0: { /* i64 */
            long long val = *(long long*)payload;
            n = node_num(cc, val, cc->line);
            break;
        }
        case 1: { /* f64 */
            double val = *(double*)payload;
            n = node_flit(cc, val, cc->line);
            break;
        }
        case 2: { /* string literal */
            n = node_new(cc, ND_STR, cc->line);
            int sid = cc->num_strings;
            if (sid < MAX_STRINGS) {
                StringEntry *se = &cc->strings[sid];
                se->data = cc_strdup(cc, (char*)payload);
                se->len = w->u.value.len;
                se->label_id = cc->label_count++;
                cc->num_strings++;
            }
            n->str_id = sid;
            n->type = type_ptr(cc, cc->ty_char);
            break;
        }
        case 3: { /* identifier */
            n = node_new(cc, ND_VAR, cc->line);
            strncpy(n->name, (const char*)payload, MAX_IDENT-1);
            Symbol *sym = scope_find(cc, n->name);
            if (sym) {
                n->sym = sym;
                n->type = sym->type;
            } else {
                n->type = cc->ty_int;
            }
            break;
        }
    }
    return n;
}

Node* zcc_build_from_wire(Compiler *cc, const uint8_t *data, size_t len) {
    if (!data || len < sizeof(WireInitNode)) return NULL;

    const WireInitNode *w = (const WireInitNode*)data;
    if (w->magic != WIRE_INIT_MAGIC) return NULL;

    Node *node = NULL;

    switch (w->kind) {
        case WIRE_LIST: {
            node = node_new(cc, ND_INIT_LIST, cc->line);
            node->num_args = 0;
            node->args = NULL;

            const uint8_t *child_ptr = data + sizeof(WireInitNode);
            for (uint32_t i = 0; i < w->u.list.child_count; i++) {
                Node *child = zcc_build_from_wire(cc, child_ptr, len - (child_ptr - data));
                if (child) {
                    if (node->num_args == 0) {
                        node->args = (Node**)cc_alloc(cc, 16 * sizeof(Node*));
                    }
                    node->args[node->num_args++] = child;
                }
                child_ptr += ((const WireInitNode*)child_ptr)->payload_size;
            }
            break;
        }

        case WIRE_VALUE:
            node = parse_value_from_wire(cc, data);
            break;

        case WIRE_DESIGNATED_FIELD:
        case WIRE_DESIGNATED_INDEX: {
            error(cc, "Designated initializers not yet supported in AST");
            break;
        }
    }

    return node;
}

ShmRingBuffer* zcc_shm_ring_open(void) {
    int fd = shm_open("zcc_ast_ring", 2 /* O_RDWR */, 0);
    if (fd < 0) return NULL;
    ShmRingBuffer* ring = (ShmRingBuffer*)mmap(NULL, sizeof(ShmRingBuffer),
                                               1 | 2 /* PROT_READ | PROT_WRITE */, MAP_SHARED, fd, 0);
    close(fd);
    return ring;
}

Node* zcc_shm_ring_read(Compiler *cc, ShmRingBuffer* ring) {
    if (!ring) return NULL;

    uint64_t head = ring->head;
    uint64_t tail = ring->tail;

    if (head == tail) return NULL;   // empty

    size_t slot_size = SHM_RING_SIZE / MAX_SLOTS;
    size_t offset = (tail % MAX_SLOTS) * slot_size;
    const uint8_t* archived = ring->data + offset;

    Node* ast = zcc_build_from_wire(cc, archived, slot_size);

    ring->tail = (tail + 1) % MAX_SLOTS;
    return ast;
}


/* ================================================================ */
/* Cross-platform ZCC-RUST SHM entry — isolated to avoid ZCC stack  */
/* offset divergence from nested declarations.                      */
/* ================================================================ */

Node *zcc_rust_shm_entry(Compiler *cc) {
    ShmRingBuffer *ring;
    int spins;
    Node *init_tree;
    Node *global;

    ring = zcc_shm_ring_open();
    if (!ring) {
        error(cc, "[ZCC-RUST] Failed to open SHM ring buffer");
        return NULL;
    }

    spins = 0;
    while (spins < 2000000) {
        init_tree = zcc_shm_ring_read(cc, ring);
        if (init_tree) {
            printf("[ZCC-RUST] Adopted zero-copy ND_INIT_LIST tree from Rust SHM\n");
            global = node_new(cc, ND_GLOBAL_VAR, 0);
            strncpy(global->name, "rust_shm_tesseract", MAX_IDENT-1);
            global->initializer = init_tree;
            global->type = type_ptr(cc, cc->ty_int);
            return global;
        }
        spins++;
    }
    error(cc, "[ZCC-RUST] SHM timeout waiting for Rust frontend data");
    return NULL;
}

Node *parse_program(Compiler *cc) {
    Node *head;
    Node *tail;
    int top_count;
    int prev_pos;

    /* SHM hot path: delegate to isolated entry function */
    if (getenv("ZCC_RUST_SHM")) {
        printf("[ZCC-RUST] SHM hot path activated\n");
        return zcc_rust_shm_entry(cc);
    }

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

        if (cc->tk == TK_IDENT && peek_token(cc) == TK_COLON_COLON) {
            next_token(cc); /* consume class name */
            next_token(cc); /* consume :: */
            if (cc->tk == TK_TILDE) {
                next_token(cc); /* consume ~ */
            }
            expect(cc, TK_IDENT); /* consume method name */
            expect(cc, TK_LPAREN);
            int depth = 1;
            while (depth > 0 && cc->tk != TK_EOF) {
                if (cc->tk == TK_LPAREN) depth++;
                else if (cc->tk == TK_RPAREN) depth--;
                next_token(cc);
            }
            if (cc->tk == TK_LBRACE) {
                int bdepth = 1;
                next_token(cc);
                while (bdepth > 0 && cc->tk != TK_EOF) {
                    if (cc->tk == TK_LBRACE) bdepth++;
                    else if (cc->tk == TK_RBRACE) bdepth--;
                    next_token(cc);
                }
            } else if (cc->tk == TK_SEMI) {
                next_token(cc);
            }
            continue;
        }

        if (cc->tk == TK_SEMI) {
            next_token(cc);
            continue;
        }

        if (cc->tk == TK_USING) {
            while (cc->tk != TK_SEMI && cc->tk != TK_EOF) {
                next_token(cc);
            }
            if (cc->tk == TK_SEMI) {
                next_token(cc);
            }
            continue;
        }

        if (cc->tk == TK_NAMESPACE) {
            next_token(cc); /* consume namespace */
            resolve_cpp_identifiers(cc);
            if (cc->tk == TK_IDENT) {
                if (g_current_namespace[0]) {
                    strncat(g_current_namespace, "_", sizeof(g_current_namespace) - strlen(g_current_namespace) - 1);
                    strncat(g_current_namespace, cc->tk_text, sizeof(g_current_namespace) - strlen(g_current_namespace) - 1);
                } else {
                    strncpy(g_current_namespace, cc->tk_text, sizeof(g_current_namespace) - 1);
                }
                next_token(cc); /* consume namespace identifier */
            }
            expect(cc, TK_LBRACE); /* consume { */
            continue;
        }

        if (cc->tk == TK_RBRACE) {
            next_token(cc); /* consume } from namespaces */
            char *last_under = strrchr(g_current_namespace, '_');
            if (last_under) {
                *last_under = '\0';
            } else {
                g_current_namespace[0] = '\0';
            }
            continue;
        }

        if (cc->tk == TK_OPERATOR) {
            next_token(cc); /* consume operator */
            while (cc->tk != TK_LPAREN && cc->tk != TK_EOF) {
                next_token(cc);
            }
            expect(cc, TK_LPAREN);
            int depth = 1;
            while (depth > 0 && cc->tk != TK_EOF) {
                if (cc->tk == TK_LPAREN) depth++;
                else if (cc->tk == TK_RPAREN) depth--;
                next_token(cc);
            }
            if (cc->tk == TK_LBRACE) {
                int bdepth = 1;
                next_token(cc);
                while (bdepth > 0 && cc->tk != TK_EOF) {
                    if (cc->tk == TK_LBRACE) bdepth++;
                    else if (cc->tk == TK_RBRACE) bdepth--;
                    next_token(cc);
                }
            } else if (cc->tk == TK_SEMI) {
                next_token(cc);
            }
            continue;
        }

        is_typedef_kw = 0;
        is_static = 0;
        is_extern = 0;

        /* check storage class */
        if (cc->tk == TK_TYPEDEF) { is_typedef_kw = 1; }
        if (cc->tk == TK_STATIC) { is_static = 1; }
        if (cc->tk == TK_EXTERN) { is_extern = 1; }

        base = parse_type(cc);

        if (cc->tk == TK_IDENT && peek_token(cc) == TK_COLON_COLON) {
            next_token(cc); /* consume class name */
            next_token(cc); /* consume :: */
            if (cc->tk == TK_TILDE) {
                next_token(cc); /* consume ~ */
            }
            expect(cc, TK_IDENT); /* consume method name */
            expect(cc, TK_LPAREN);
            int depth = 1;
            while (depth > 0 && cc->tk != TK_EOF) {
                if (cc->tk == TK_LPAREN) depth++;
                else if (cc->tk == TK_RPAREN) depth--;
                next_token(cc);
            }
            if (cc->tk == TK_LBRACE) {
                int bdepth = 1;
                next_token(cc);
                while (bdepth > 0 && cc->tk != TK_EOF) {
                    if (cc->tk == TK_LBRACE) bdepth++;
                    else if (cc->tk == TK_RBRACE) bdepth--;
                    next_token(cc);
                }
            } else if (cc->tk == TK_SEMI) {
                next_token(cc);
            }
            continue;
        }

        if (cc->tk == TK_OPERATOR) {
            next_token(cc); /* consume operator */
            while (cc->tk != TK_LPAREN && cc->tk != TK_EOF) {
                next_token(cc);
            }
            expect(cc, TK_LPAREN);
            int depth = 1;
            while (depth > 0 && cc->tk != TK_EOF) {
                if (cc->tk == TK_LPAREN) depth++;
                else if (cc->tk == TK_RPAREN) depth--;
                next_token(cc);
            }
            if (cc->tk == TK_LBRACE) {
                int bdepth = 1;
                next_token(cc);
                while (bdepth > 0 && cc->tk != TK_EOF) {
                    if (cc->tk == TK_LBRACE) bdepth++;
                    else if (cc->tk == TK_RBRACE) bdepth--;
                    next_token(cc);
                }
            } else if (cc->tk == TK_SEMI) {
                next_token(cc);
            }
            continue;
        }

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
                if (gpk == TK_STAR) {
                    /* function pointer: typedef int (*name)(params); or int (*fp)(int); */
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
                        if (g_current_namespace[0]) {
                            char prefixed_name[MAX_IDENT * 2];
                            sprintf(prefixed_name, "%.120s_%.120s", g_current_namespace, cc->tk_text);
                            strncpy(name, prefixed_name, MAX_IDENT - 1);
                            name[MAX_IDENT - 1] = 0;
                        } else {
                            strncpy(name, cc->tk_text, MAX_IDENT - 1);
                        }
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
                } else if (gpk == TK_IDENT) {
                    /* parenthesized name: type (name)(params)
                     * Used pervasively in Lua 5.4.6 API headers:
                     *   extern lua_State *(lua_newstate)(lua_Alloc f, void *ud);
                     *   extern void       (lua_close)(lua_State *L);
                     * Consume (name) then parse trailing (params). */
                    next_token(cc); /* consume ( */
                    if (cc->tk == TK_IDENT) {
                        strncpy(name, cc->tk_text, MAX_IDENT - 1);
                        next_token(cc);
                    }
                    expect(cc, TK_RPAREN);
                    dtype = ptr_type;
                    goto after_name;
                }
            }

            if (cc->tk == TK_IDENT) {
                if (g_current_namespace[0]) {
                    char prefixed_name[MAX_IDENT * 2];
                    sprintf(prefixed_name, "%.120s_%.120s", g_current_namespace, cc->tk_text);
                    strncpy(name, prefixed_name, MAX_IDENT - 1);
                    name[MAX_IDENT - 1] = 0;
                } else {
                    strncpy(name, cc->tk_text, MAX_IDENT - 1);
                }
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
                    int top_elems = 0;
                    Node *init_list = parse_initializer_list(cc, &top_elems);
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

/* ================================================================ */
/* Rust Frontend FFI: Recursive Initializer Bridge                  */
/* ================================================================ */

ZccInitNode* zcc_init_list_begin(Type* ty) {
    ZccInitNode* node = calloc(1, sizeof(ZccInitNode));
    node->kind = ZCC_INIT_LIST;
    node->ty = ty;
    node->list.capacity = 8;
    node->list.count = 0;
    node->list.children = malloc(sizeof(ZccInitNode*) * node->list.capacity);
    return node;
}

void zcc_init_list_append(ZccInitNode* list, ZccInitNode* child) {
    if (!list || list->kind != ZCC_INIT_LIST) return;
    if (list->list.count >= list->list.capacity) {
        list->list.capacity *= 2;
        list->list.children = realloc(list->list.children, sizeof(ZccInitNode*) * list->list.capacity);
    }
    list->list.children[list->list.count++] = child;
}

void zcc_init_list_end(ZccInitNode* list) {
    /* No-op, future designated initializer finalizations */
}

ZccInitNode* zcc_init_value(Node* expr_node) {
    ZccInitNode* node = calloc(1, sizeof(ZccInitNode));
    node->kind = ZCC_INIT_VALUE;
    node->value_node = expr_node;
    if (expr_node) node->ty = expr_node->type;
    return node;
}

/* Default weak implementation for C-only builds. 
   Rust frontend overrides this via extern "C" */
__attribute__((weak)) void zcc_rust_free_init_tree(ZccInitNode* root) {
    if (!root) return;
    if (root->kind == ZCC_INIT_LIST) {
        for (int i = 0; i < root->list.count; i++) {
            zcc_rust_free_init_tree(root->list.children[i]);
        }
        if (root->list.children) free(root->list.children);
    }
    free(root);
}

static Node* zcc_build_init_internal(Compiler* cc, ZccInitNode* root, Type* target_type) {
    if (!root) return NULL;
    
    Node* node = NULL;
    if (root->kind == ZCC_INIT_VALUE) {
        node = root->value_node;
    } else if (root->kind == ZCC_INIT_LIST) {
        node = node_new(cc, ND_INIT_LIST, 0);
        node->type = target_type ? target_type : root->ty;
        node->num_args = root->list.count;
        if (node->num_args > 0) {
            node->args = cc_alloc(cc, node->num_args * sizeof(Node*));
            for (int i = 0; i < root->list.count; i++) {
                node->args[i] = zcc_build_init_internal(cc, root->list.children[i], NULL);
            }
        }
    }
    return node;
}

Node* zcc_build_initializer(Compiler* cc, ZccInitNode* root, Type* target_type) {
    Node* final_node = zcc_build_init_internal(cc, root, target_type);
    zcc_rust_free_init_tree(root);
    return final_node;
}

/* ZKAEDI FORCE RENDER CACHE INVALIDATION */
