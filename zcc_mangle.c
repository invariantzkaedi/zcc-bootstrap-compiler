/* zcc_mangle.c — Itanium C++ ABI name mangler for ZCC
 * ─────────────────────────────────────────────────────────────────────
 * Realizes the OP_MANGLE_* opcode pack as a standalone, self-contained
 * encoder. Plain C89-ish, no arena dependency, so it drops into the ZCC
 * single-file build or compiles alone for the harness.
 *
 * Scope (the "common cases" that gate cross-TU linking):
 *   - free functions:        _Z <name> <params>
 *   - nested names:          _ZN <prefix> E <params>          (OP_MANGLE_NESTED)
 *   - const member fn:       _ZNK ...                          (cv-qualifier)
 *   - ctors C1/C2, dtors D1/D2                                 (OP_MANGLE_CTOR/DTOR)
 *   - builtin type codes + ptr/ref/const                       (OP_MANGLE_TYPE)
 *   - substitutions S_, S0_, S1_ ...                           (OP_MANGLE_SUBST)
 *   - vtable / typeinfo symbols _ZTV / _ZTI / _ZTS             (OP_VTABLE_SYMBOL etc.)
 *
 * Correctness oracle: harness compares every output byte-for-byte against
 * g++ 13 and round-trips through c++filt.
 * ───────────────────────────────────────────────────────────────────── */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define MANGLE_MAX      1024
#define SUBST_MAX        128
#define SUBST_KEY_MAX    256

/* A parameter type descriptor, recursively structured. */
typedef struct ParamType {
    const char *base;           /* builtin/class name, or NULL if composite/pointer/func */
    int ptr;                    /* pointer levels (P) - legacy support */
    int is_ref;                /* reference (R) - legacy support */
    int is_const_pointee;      /* const on pointed-to type (K) - legacy support */

    int kind;                   /* 0: normal/class/builtin, 1: pointer, 2: reference, 3: const, 4: func, 5: template, 6: rvalue ref, 7: ptr-to-member */
    struct ParamType *child;    /* pointer/reference/const/member child type */

    /* for function pointers (kind == 4) */
    struct ParamType *ret;      /* return type */
    struct ParamType **params;  /* parameter type array */
    int nparams;

    /* for template types (kind == 5) */
    struct ParamType **args;    /* template argument type array */
    int nargs;

    /* for pointer-to-member (kind == 7) */
    struct ParamType *class_type; /* class containing the member */
} ParamType;

/* ── Output buffer ──────────────────────────────────────────────────── */
typedef struct {
    char buf[MANGLE_MAX];
    int  len;
    /* Substitution table: each entry is the *mangled fragment* already
     * emitted that a later identical fragment may reference as S<n>_.
     * Itanium seeds index 0 = first substitutable component. */
    char subst[SUBST_MAX][SUBST_KEY_MAX];
    int  n_subst;
} Mangler;

static void m_init(Mangler *m) { m->len = 0; m->buf[0] = 0; m->n_subst = 0; }

static void m_putc(Mangler *m, char c) {
    if (m->len < MANGLE_MAX - 1) { m->buf[m->len++] = c; m->buf[m->len] = 0; }
}
static void m_puts(Mangler *m, const char *s) { while (*s) m_putc(m, *s++); }

static void m_put_int(Mangler *m, int v) {
    char tmp[16]; int n = 0;
    if (v == 0) { m_putc(m, '0'); return; }
    while (v > 0) { tmp[n++] = (char)('0' + v % 10); v /= 10; }
    while (n--) m_putc(m, tmp[n]);
}

/* source-name := <len> <identifier>   e.g.  Foo -> 3Foo */
static void m_source_name(Mangler *m, const char *id) {
    m_put_int(m, (int)strlen(id));
    m_puts(m, id);
}

/* ── Substitution machinery (OP_MANGLE_SUBST) ───────────────────────────
 * Returns the S<n>_ index for a fragment if seen before, else -1.
 * Itanium: S_ == index 0, S0_ == index 1, S1_ == index 2 ... (off by one). */
static int subst_find_before(Mangler *m, const char *frag, int limit) {
    int i;
    for (i = 0; i < limit && i < m->n_subst; i++)
        if (strcmp(m->subst[i], frag) == 0) return i;
    return -1;
}
static int subst_find(Mangler *m, const char *frag) {
    return subst_find_before(m, frag, m->n_subst);
}
static void subst_add(Mangler *m, const char *frag) {
    if (m->n_subst < SUBST_MAX && (int)strlen(frag) < SUBST_KEY_MAX)
        strcpy(m->subst[m->n_subst++], frag);
}
/* emit S_ / S0_ / S1_ ... base-36, matching the Itanium grammar */
static void m_emit_subst_ref(Mangler *m, int idx) {
    m_putc(m, 'S');
    if (idx > 0) {
        int seq = idx - 1;                 /* S_ = 0th, so S0_ encodes seq 0 */
        char tmp[8]; int n = 0;
        const char *d = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        if (seq == 0) { m_putc(m, '0'); }
        else { while (seq > 0) { tmp[n++] = d[seq % 36]; seq /= 36; }
               while (n--) m_putc(m, tmp[n]); }
    }
    m_putc(m, '_');
}

/* ── Builtin type codes (OP_MANGLE_TYPE) ────────────────────────────────
 * Itanium single-char codes. Returns NULL if not a builtin. */
static const char *builtin_code(const char *t) {
    if (!strcmp(t, "void"))           return "v";
    if (!strcmp(t, "bool"))           return "b";
    if (!strcmp(t, "char"))           return "c";
    if (!strcmp(t, "signed char"))    return "a";
    if (!strcmp(t, "unsigned char"))  return "h";
    if (!strcmp(t, "short"))          return "s";
    if (!strcmp(t, "unsigned short")) return "t";
    if (!strcmp(t, "int"))            return "i";
    if (!strcmp(t, "unsigned"))       return "j";
    if (!strcmp(t, "unsigned int"))   return "j";
    if (!strcmp(t, "long"))           return "l";
    if (!strcmp(t, "unsigned long"))  return "m";
    if (!strcmp(t, "long long"))      return "x";
    if (!strcmp(t, "unsigned long long")) return "y";
    if (!strcmp(t, "float"))          return "f";
    if (!strcmp(t, "double"))         return "d";
    return NULL;
}

static void mangle_type_recursive(Mangler *m, const ParamType *pt, char *out_unsub, char *out_sub, int limit, size_t out_max);

/* Split "app::Vec" into nested components, checking substitutions */
static void mangle_class_name_to_buf(Mangler *m, const char *qualified, char *buf, size_t max_len) {
    char work[256]; strncpy(work, qualified, 255); work[255] = 0;
    char *parts[16]; int np = 0;
    char *p = work, *start = work;
    int i;

    while (*p) {
        if (p[0] == ':' && p[1] == ':') { *p = 0; parts[np++] = start; p += 2; start = p; }
        else p++;
    }
    parts[np++] = start;

    buf[0] = 0;

    if (np == 1) {
        char mangled_part[MANGLE_MAX];
        Mangler temp;
        m_init(&temp);
        m_source_name(&temp, parts[0]);
        strcpy(mangled_part, temp.buf);

        int s = subst_find(m, mangled_part);
        if (s >= 0) {
            Mangler ref_temp;
            m_init(&ref_temp);
            m_emit_subst_ref(&ref_temp, s);
            strncpy(buf, ref_temp.buf, max_len - 1);
            buf[max_len - 1] = 0;
        } else {
            strncpy(buf, mangled_part, max_len - 1);
            buf[max_len - 1] = 0;
            subst_add(m, mangled_part);
        }
        return;
    }

    char prefix_mangled[MANGLE_MAX];
    prefix_mangled[0] = 0;

    char out_repr[MANGLE_MAX];
    out_repr[0] = 0;

    int last_subst_idx = -1;
    int first_new_part = 0;

    for (i = 0; i < np; i++) {
        char part_mangled_tmp[256];
        Mangler temp;
        m_init(&temp);
        m_source_name(&temp, parts[i]);
        strcpy(part_mangled_tmp, temp.buf);

        strcat(prefix_mangled, part_mangled_tmp);

        int s = subst_find(m, prefix_mangled);
        if (s >= 0) {
            last_subst_idx = s;
            first_new_part = i + 1;
        }
    }

    if (last_subst_idx >= 0) {
        Mangler ref_temp;
        m_init(&ref_temp);
        m_emit_subst_ref(&ref_temp, last_subst_idx);
        strcpy(out_repr, ref_temp.buf);

        if (first_new_part == np) {
            strncpy(buf, out_repr, max_len - 1);
            buf[max_len - 1] = 0;
            return;
        }

        char remainder[MANGLE_MAX];
        remainder[0] = 0;

        char current_prefix[MANGLE_MAX];
        current_prefix[0] = 0;
        for (i = 0; i < first_new_part; i++) {
            Mangler temp;
            m_init(&temp);
            m_source_name(&temp, parts[i]);
            strcat(current_prefix, temp.buf);
        }

        for (i = first_new_part; i < np; i++) {
            char part_mangled_tmp[256];
            Mangler temp;
            m_init(&temp);
            m_source_name(&temp, parts[i]);
            strcpy(part_mangled_tmp, temp.buf);

            strcat(remainder, part_mangled_tmp);
            strcat(current_prefix, part_mangled_tmp);
            subst_add(m, current_prefix);
        }

        snprintf(buf, max_len, "N%s%sE", out_repr, remainder);
        subst_add(m, current_prefix);
        return;
    }

    char full_nested[MANGLE_MAX];
    strcpy(full_nested, "N");

    char current_prefix[MANGLE_MAX];
    current_prefix[0] = 0;

    for (i = 0; i < np; i++) {
        char part_mangled_tmp[256];
        Mangler temp;
        m_init(&temp);
        m_source_name(&temp, parts[i]);
        strcpy(part_mangled_tmp, temp.buf);

        strcat(full_nested, part_mangled_tmp);
        strcat(current_prefix, part_mangled_tmp);
        subst_add(m, current_prefix);
    }
    strcat(full_nested, "E");

    strncpy(buf, full_nested, max_len - 1);
    buf[max_len - 1] = 0;
}

/* Mangle template name prefix with template arguments inside nested context */
static void mangle_template_name_dual(Mangler *m, const char *qualified, ParamType **args, int nargs, char *out_unsub, char *out_sub, int limit, size_t out_max) {
    char work[256]; strncpy(work, qualified, 255); work[255] = 0;
    char *parts[16]; int np = 0;
    char *p = work, *start = work;
    int i;

    while (*p) {
        if (p[0] == ':' && p[1] == ':') { *p = 0; parts[np++] = start; p += 2; start = p; }
        else p++;
    }
    parts[np++] = start;

    out_unsub[0] = 0;
    out_sub[0] = 0;

    if (np == 1) {
        char part_mangled[256];
        Mangler temp;
        m_init(&temp);
        m_source_name(&temp, parts[0]);
        strcpy(part_mangled, temp.buf);

        /* Unsubstituted: first search substitutions before this template name */
        int s = subst_find(m, part_mangled);
        char name_unsub[MANGLE_MAX];
        if (s >= 0) {
            Mangler ref_temp;
            m_init(&ref_temp);
            m_emit_subst_ref(&ref_temp, s);
            strcpy(name_unsub, ref_temp.buf);
        } else {
            strcpy(name_unsub, part_mangled);
            subst_add(m, part_mangled);
        }

        /* Substituted: search in the full table (which now contains part_mangled) */
        int s_sub = subst_find(m, part_mangled);
        char name_sub[MANGLE_MAX];
        if (s_sub >= 0) {
            Mangler ref_temp;
            m_init(&ref_temp);
            m_emit_subst_ref(&ref_temp, s_sub);
            strcpy(name_sub, ref_temp.buf);
        } else {
            strcpy(name_sub, part_mangled);
        }

        char args_unsub[MANGLE_MAX]; args_unsub[0] = 0;
        char args_sub[MANGLE_MAX]; args_sub[0] = 0;
        for (i = 0; i < nargs; i++) {
            char a_unsub[MANGLE_MAX];
            char a_sub[MANGLE_MAX];
            mangle_type_recursive(m, args[i], a_unsub, a_sub, m->n_subst, sizeof(a_unsub));
            
            /* Append to unsub (using its own recursive substitution if applicable) */
            int as = subst_find_before(m, a_sub, limit);
            if (as >= 0) {
                Mangler temp2;
                m_init(&temp2);
                m_emit_subst_ref(&temp2, as);
                strncat(args_unsub, temp2.buf, sizeof(args_unsub) - strlen(args_unsub) - 1);
            } else {
                strncat(args_unsub, a_unsub, sizeof(args_unsub) - strlen(args_unsub) - 1);
            }

            /* Append to sub (using the full table) */
            int as_sub = subst_find(m, a_sub);
            if (as_sub >= 0) {
                Mangler temp2;
                m_init(&temp2);
                m_emit_subst_ref(&temp2, as_sub);
                strncat(args_sub, temp2.buf, sizeof(args_sub) - strlen(args_sub) - 1);
            } else {
                strncat(args_sub, a_sub, sizeof(args_sub) - strlen(args_sub) - 1);
            }
        }

        snprintf(out_unsub, out_max, "%sI%sE", name_unsub, args_unsub);
        snprintf(out_sub, out_max, "%sI%sE", name_sub, args_sub);
        return;
    }

    /* Nested template name: app::vector */
    char prefix_mangled[MANGLE_MAX];
    prefix_mangled[0] = 0;
    
    char out_unsub_prefix[MANGLE_MAX]; out_unsub_prefix[0] = 0;
    
    int first_new_part = 0;
    int last_subst_idx = -1;

    for (i = 0; i < np; i++) {
        char part_mangled_tmp2[256];
        Mangler temp;
        m_init(&temp);
        m_source_name(&temp, parts[i]);
        strcpy(part_mangled_tmp2, temp.buf);

        strcat(prefix_mangled, part_mangled_tmp2);

        int s = subst_find(m, prefix_mangled);
        if (s >= 0) {
            last_subst_idx = s;
            first_new_part = i + 1;
        }
    }

    /* We build the unsubstituted prefix representation */
    if (last_subst_idx >= 0) {
        Mangler ref_temp;
        m_init(&ref_temp);
        m_emit_subst_ref(&ref_temp, last_subst_idx);
        strcpy(out_unsub_prefix, ref_temp.buf);
    }

    char current_prefix[MANGLE_MAX];
    current_prefix[0] = 0;
    for (i = 0; i < first_new_part; i++) {
        Mangler temp;
        m_init(&temp);
        m_source_name(&temp, parts[i]);
        strcat(current_prefix, temp.buf);
    }

    /* Process the remaining parts */
    char remainder_unsub[MANGLE_MAX]; remainder_unsub[0] = 0;
    char remainder_sub[MANGLE_MAX]; remainder_sub[0] = 0;

    for (i = first_new_part; i < np; i++) {
        char part_mangled_tmp[256];
        Mangler temp;
        m_init(&temp);
        m_source_name(&temp, parts[i]);
        strcpy(part_mangled_tmp, temp.buf);

        strcat(current_prefix, part_mangled_tmp);
        
        if (i < np - 1) {
            strcat(remainder_unsub, part_mangled_tmp);
            subst_add(m, current_prefix);
            
            /* Build the remainder_sub by looking up in the full table */
            int s = subst_find(m, current_prefix);
            if (s >= 0) {
                Mangler temp2;
                m_init(&temp2);
                m_emit_subst_ref(&temp2, s);
                strcpy(remainder_sub, temp2.buf);
            } else {
                strcat(remainder_sub, part_mangled_tmp);
            }
        } else {
            /* Last part: the template name itself */
            strcat(remainder_unsub, part_mangled_tmp);
            strcat(remainder_unsub, "I");
            
            strcat(remainder_sub, part_mangled_tmp);
            /* Register template prefix in full table */
            subst_add(m, current_prefix);
            
            /* check if template name prefix is now substituted in full table */
            int s = subst_find(m, current_prefix);
            if (s >= 0) {
                Mangler temp2;
                m_init(&temp2);
                m_emit_subst_ref(&temp2, s);
                strcpy(remainder_sub, temp2.buf);
            }
            strcat(remainder_sub, "I");

            int j;
            for (j = 0; j < nargs; j++) {
                char a_unsub[MANGLE_MAX];
                char a_sub[MANGLE_MAX];
                mangle_type_recursive(m, args[j], a_unsub, a_sub, m->n_subst, sizeof(a_unsub));
                
                int as = subst_find_before(m, a_sub, limit);
                if (as >= 0) {
                    Mangler temp2;
                    m_init(&temp2);
                    m_emit_subst_ref(&temp2, as);
                    strncat(remainder_unsub, temp2.buf, sizeof(remainder_unsub) - strlen(remainder_unsub) - 1);
                } else {
                    strncat(remainder_unsub, a_unsub, sizeof(remainder_unsub) - strlen(remainder_unsub) - 1);
                }

                int as_sub = subst_find(m, a_sub);
                if (as_sub >= 0) {
                    Mangler temp2;
                    m_init(&temp2);
                    m_emit_subst_ref(&temp2, as_sub);
                    strncat(remainder_sub, temp2.buf, sizeof(remainder_sub) - strlen(remainder_sub) - 1);
                } else {
                    strncat(remainder_sub, a_sub, sizeof(remainder_sub) - strlen(remainder_sub) - 1);
                }
            }
            strcat(remainder_unsub, "E");
            strcat(remainder_sub, "E");
        }
    }

    if (last_subst_idx >= 0) {
        snprintf(out_unsub, out_max, "N%s%sE", out_unsub_prefix, remainder_unsub);
        snprintf(out_sub, out_max, "N%s%sE", out_unsub_prefix, remainder_sub);
    } else {
        snprintf(out_unsub, out_max, "N%sE", remainder_unsub);
        snprintf(out_sub, out_max, "N%sE", remainder_sub);
    }
}

/* Recursive parameter type mangling logic */
static void mangle_type_recursive(Mangler *m, const ParamType *pt, char *out_unsub, char *out_sub, int limit, size_t out_max) {
    char child_unsub[MANGLE_MAX];
    char child_sub[MANGLE_MAX];
    int cs;

    out_unsub[0] = 0;
    out_sub[0] = 0;

    if (pt->kind == 0 && (pt->ptr > 0 || pt->is_ref || pt->is_const_pointee)) {
        ParamType rec_type;
        ParamType child_type;

        memset(&rec_type, 0, sizeof(rec_type));
        memset(&child_type, 0, sizeof(child_type));

        if (pt->is_ref) {
            rec_type.kind = 2; /* reference */
            child_type = *pt;
            child_type.is_ref = 0;
            rec_type.child = &child_type;
            mangle_type_recursive(m, &rec_type, out_unsub, out_sub, limit, out_max);
            return;
        }
        if (pt->ptr > 0) {
            rec_type.kind = 1; /* pointer */
            child_type = *pt;
            child_type.ptr = pt->ptr - 1;
            rec_type.child = &child_type;
            mangle_type_recursive(m, &rec_type, out_unsub, out_sub, limit, out_max);
            return;
        }
        if (pt->is_const_pointee) {
            rec_type.kind = 3; /* const */
            child_type = *pt;
            child_type.is_const_pointee = 0;
            rec_type.child = &child_type;
            mangle_type_recursive(m, &rec_type, out_unsub, out_sub, limit, out_max);
            return;
        }
    }

    if (pt->kind == 0) {
        const char *bc = builtin_code(pt->base);
        if (bc) {
            strncpy(out_unsub, bc, out_max - 1);
            out_unsub[out_max - 1] = 0;
            strcpy(out_sub, out_unsub);
        } else {
            char class_buf[MANGLE_MAX];
            mangle_class_name_to_buf(m, pt->base, class_buf, sizeof(class_buf));
            strncpy(out_unsub, class_buf, out_max - 1);
            out_unsub[out_max - 1] = 0;
            strcpy(out_sub, out_unsub);
        }
        return;
    }

    if (pt->kind == 1 || pt->kind == 2 || pt->kind == 3 || pt->kind == 6) {
        char prefix = (pt->kind == 1) ? 'P' : ((pt->kind == 2) ? 'R' : ((pt->kind == 3) ? 'K' : 'O'));

        mangle_type_recursive(m, pt->child, child_unsub, child_sub, m->n_subst, sizeof(child_unsub));

        snprintf(out_unsub, out_max, "%c%s", prefix, child_unsub);

        cs = subst_find_before(m, child_sub, m->n_subst);
        char child_sub_ref[MANGLE_MAX];
        if (cs >= 0) {
            Mangler temp;
            m_init(&temp);
            m_emit_subst_ref(&temp, cs);
            strcpy(child_sub_ref, temp.buf);
        } else {
            strcpy(child_sub_ref, child_sub);
        }
        snprintf(out_sub, out_max, "%c%s", prefix, child_sub_ref);

        if (strlen(out_sub) > 1 && out_sub[1] != 'v') {
            if (!(out_sub[0] == 'S' && out_sub[strlen(out_sub)-1] == '_')) {
                subst_add(m, out_sub);
            }
        }
        return;
    }

    if (pt->kind == 4) {
        char ret_unsub[MANGLE_MAX];
        char ret_sub[MANGLE_MAX];
        char params_unsub[MANGLE_MAX];
        char params_sub[MANGLE_MAX];
        int i;

        params_unsub[0] = 0;
        params_sub[0] = 0;

        mangle_type_recursive(m, pt->ret, ret_unsub, ret_sub, m->n_subst, sizeof(ret_unsub));

        int rs = subst_find_before(m, ret_sub, m->n_subst);
        char ret_sub_ref[MANGLE_MAX];
        if (rs >= 0) {
            Mangler temp;
            m_init(&temp);
            m_emit_subst_ref(&temp, rs);
            strcpy(ret_sub_ref, temp.buf);
        } else {
            strcpy(ret_sub_ref, ret_sub);
        }

        if (pt->nparams == 0) {
            strcpy(params_unsub, "v");
            strcpy(params_sub, "v");
        } else {
            for (i = 0; i < pt->nparams; i++) {
                char p_unsub[MANGLE_MAX];
                char p_sub[MANGLE_MAX];
                mangle_type_recursive(m, pt->params[i], p_unsub, p_sub, m->n_subst, sizeof(p_unsub));

                int ps = subst_find_before(m, p_sub, limit);
                if (ps >= 0) {
                    Mangler temp;
                    m_init(&temp);
                    m_emit_subst_ref(&temp, ps);
                    strncat(params_unsub, temp.buf, sizeof(params_unsub) - strlen(params_unsub) - 1);
                } else {
                    strncat(params_unsub, p_unsub, sizeof(params_unsub) - strlen(params_unsub) - 1);
                }

                int ps_sub = subst_find_before(m, p_sub, limit);
                if (ps_sub >= 0) {
                    Mangler temp;
                    m_init(&temp);
                    m_emit_subst_ref(&temp, ps_sub);
                    strncat(params_sub, temp.buf, sizeof(params_sub) - strlen(params_sub) - 1);
                } else {
                    strncat(params_sub, p_sub, sizeof(params_sub) - strlen(params_sub) - 1);
                }
            }
        }

        snprintf(out_unsub, out_max, "F%s%sE", ret_unsub, params_unsub);
        snprintf(out_sub, out_max, "F%s%sE", ret_sub_ref, params_sub);

        if (strlen(out_sub) > 1) {
            subst_add(m, out_sub);
        }
        return;
    }

    if (pt->kind == 5) {
        mangle_template_name_dual(m, pt->base, pt->args, pt->nargs, out_unsub, out_sub, m->n_subst, out_max);

        if (strlen(out_sub) > 1) {
            subst_add(m, out_sub);
        }
        return;
    }

    if (pt->kind == 7) {
        char class_unsub[MANGLE_MAX];
        char class_sub[MANGLE_MAX];
        char member_unsub[MANGLE_MAX];
        char member_sub[MANGLE_MAX];

        mangle_type_recursive(m, pt->class_type, class_unsub, class_sub, m->n_subst, sizeof(class_unsub));
        mangle_type_recursive(m, pt->child, member_unsub, member_sub, m->n_subst, sizeof(member_unsub));

        snprintf(out_unsub, out_max, "M%s%s", class_unsub, member_unsub);

        int cs = subst_find_before(m, class_sub, m->n_subst);
        char class_sub_ref[MANGLE_MAX];
        if (cs >= 0) {
            Mangler temp;
            m_init(&temp);
            m_emit_subst_ref(&temp, cs);
            strcpy(class_sub_ref, temp.buf);
        } else {
            strcpy(class_sub_ref, class_sub);
        }

        int ms = subst_find_before(m, member_sub, m->n_subst);
        char member_sub_ref[MANGLE_MAX];
        if (ms >= 0) {
            Mangler temp;
            m_init(&temp);
            m_emit_subst_ref(&temp, ms);
            strcpy(member_sub_ref, temp.buf);
        } else {
            strcpy(member_sub_ref, member_sub);
        }

        snprintf(out_sub, out_max, "M%s%s", class_sub_ref, member_sub_ref);

        if (strlen(out_sub) > 1) {
            subst_add(m, out_sub);
        }
        return;
    }
}

/* Mangle one parameter type, with substitution of composed forms. */
static void mangle_param(Mangler *m, const ParamType *pt) {
    char unsub_buf[MANGLE_MAX];
    char sub_buf[MANGLE_MAX];
    int frozen_subst_count = m->n_subst;

    mangle_type_recursive(m, pt, unsub_buf, sub_buf, frozen_subst_count, sizeof(unsub_buf));

    int s = subst_find_before(m, sub_buf, frozen_subst_count);
    if (s >= 0) {
        m_emit_subst_ref(m, s);
    } else {
        m_puts(m, unsub_buf);
    }
}

/* ── Top-level entry points ─────────────────────────────────────────── */

/* Free function: _Z <name> <params...>   (void params -> "v") */
void mangle_free_function(Mangler *m, const char *name,
                          const ParamType *params, int nparams) {
    m_init(m);
    m_puts(m, "_Z");
    m_source_name(m, name);
    if (nparams == 0) { m_putc(m, 'v'); return; }
    int i; for (i = 0; i < nparams; i++) mangle_param(m, &params[i]);
}

/* Member function (incl. ctor/dtor):
 *   _ZN[K] <class-components> <member> E <params>
 * member==NULL && ctor!=0 -> C1/C2 ; member==NULL && dtor!=0 -> D1/D2 */
void mangle_member_function(Mangler *m, const char *qualified_class,
                            const char *member, int is_const,
                            int ctor_variant, int dtor_variant,
                            const ParamType *params, int nparams) {
    m_init(m);
    m_puts(m, "_ZN");
    if (is_const) m_putc(m, 'K');

    /* Emit class components, registering growing-prefix substitutions */
    char work[256]; strncpy(work, qualified_class, 255); work[255] = 0;
    char *parts[16]; int np = 0; char *p = work, *start = work;
    while (*p) {
        if (p[0] == ':' && p[1] == ':') { *p = 0; parts[np++] = start; p += 2; start = p; }
        else p++;
    }
    parts[np++] = start;

    char prefix_mangled[MANGLE_MAX]; prefix_mangled[0] = 0;
    int i;
    for (i = 0; i < np; i++) {
        char part_mangled_tmp3[256];
        Mangler temp;
        m_init(&temp);
        m_source_name(&temp, parts[i]);
        strcpy(part_mangled_tmp3, temp.buf);

        strcat(prefix_mangled, part_mangled_tmp3);
        m_puts(m, part_mangled_tmp3);
        subst_add(m, prefix_mangled); /* register mangled prefix */
    }

    /* member component */
    if (ctor_variant)      { m_putc(m, 'C'); m_put_int(m, ctor_variant); }
    else if (dtor_variant) { m_putc(m, 'D'); m_put_int(m, dtor_variant); }
    else                   m_source_name(m, member);

    m_putc(m, 'E');

    if (nparams == 0) { m_putc(m, 'v'); return; }
    for (i = 0; i < nparams; i++) mangle_param(m, &params[i]);
}

/* Special symbols (OP_VTABLE_SYMBOL / OP_TYPEINFO_SYMBOL) */
void mangle_vtable(Mangler *m, const char *qualified_class) {
    m_init(m); m_puts(m, "_ZTV");
    /* same nested encoding as a type name */
    if (strstr(qualified_class, "::")) {
        char work[256]; strncpy(work, qualified_class, 255); work[255]=0;
        char *parts[16]; int np=0; char *p=work,*s=work;
        while(*p){ if(p[0]==':'&&p[1]==':'){*p=0;parts[np++]=s;p+=2;s=p;}else p++; }
        parts[np++]=s;
        m_putc(m,'N'); int i; for(i=0;i<np;i++) m_source_name(m,parts[i]); m_putc(m,'E');
    } else m_source_name(m, qualified_class);
}
void mangle_typeinfo(Mangler *m, const char *qualified_class) {
    m_init(m); m_puts(m, "_ZTI"); m_source_name(m, qualified_class);
}
void mangle_typeinfo_name(Mangler *m, const char *qualified_class) {
    m_init(m); m_puts(m, "_ZTS"); m_source_name(m, qualified_class);
}
