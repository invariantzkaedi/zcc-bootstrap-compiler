/*
 * sym_type_ast_ir.c  --  ZCC Unified Recursive Registry -- Implementations
 *
 * C89 strict.  LP64.  No // comments.  All decls at top of block.
 *
 * ADAPT markers (6 seams):
 *   ADAPT-1  ZCC_TY_* kind constants  (in sym_type_ast_ir.h)
 *   ADAPT-2  Type/Node/Member forward decls (in sym_type_ast_ir.h)
 *   ADAPT-3  Type struct field names: kind, size, align  (type_* layer)
 *   ADAPT-4  Node struct field names: kind, lhs, rhs, ty, line, next  (ast_* layer)
 *   ADAPT-5  Member struct: name, next, offset, ty  (member lookup)
 *   ADAPT-6  IRAsmCtx::out and ir_bridge_* function names (ir_* layer)
 *
 * In the ZCC concat build (zcc_pp.c) all prior declarations are
 * already in scope.  For standalone GCC compilation, define
 * ZCC_STANDALONE_BUILD and include the relevant headers manually.
 */

#ifndef ZCC_CONCAT_BUILD
/*
 * _GNU_SOURCE exposes snprintf on strict C89 tool-chains where
 * the function is a POSIX extension not declared by default.
 * Suppressed in the concat build -- ZCC's own concat doesn't need it.
 */
#  ifndef _GNU_SOURCE
#    define _GNU_SOURCE
#  endif
#  include "sym_type_ast_ir.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>/*
 * ============================================================
 * Internal: allocator shim
 *
 * ADAPT: replace with ZCC's arena_alloc(arena, sz) if available.
 * The arena version avoids fragmentation and makes scope teardown O(1).
 * ============================================================
 */
static void *zcc_sym_alloc(unsigned int sz)
{
    void *p;
    p = calloc(1u, (size_t)sz);
    if (!p) {
        fprintf(stderr, "zcc: sym_type_ast_ir: out of memory (%u bytes)\n",
                sz);
        exit(1);
    }
    return p;
}

/*
 * Internal: djb2 hash -- maps a name to a hash-table bucket index.
 * mask must be (table_size - 1) where table_size is a power of two.
 */
static unsigned int strhash(const char *s, unsigned int mask)
{
    unsigned int h;
    h = 5381u;
    while (*s) {
        h = ((h << 5u) + h) ^ (unsigned int)((unsigned char)*s);
        ++s;
    }
    return h & mask;
}

/* ============================================================
 * LAYER 1: Recursive symbol-table layer
 *
 * ZccScope is a hash table of ZccSymbol entries linked into
 * a parent chain.  push/pop walk the chain; lookup_recursive
 * searches from innermost outward.
 * ============================================================ */

ZccScope *sym_scope_push(ZccScope *parent, int kind)
{
    ZccScope *sc;
    ZCC_TAG(TAG_SCOPE);
    sc         = (ZccScope *)zcc_sym_alloc(
                     (unsigned int)sizeof(ZccScope));
    sc->parent = parent;
    sc->depth  = parent ? parent->depth + 1 : 0;
    sc->kind   = kind;
    return sc;
}

ZccScope *sym_scope_pop(ZccScope *scope)
{
    ZCC_TAG(TAG_SCOPE);
    if (!scope) return NULL;
    /*
     * Free the scope and all symbols it owns.
     * Skip free() calls if using an arena -- just return parent.
     */
    {
        int b;
        ZccSymbol *s;
        ZccSymbol *nx;
        for (b = 0; b < ZCC_SCOPE_HASH_SIZE; b++) {
            s = scope->buckets[b];
            while (s) {
                nx = s->next;
                free(s);
                s = nx;
            }
        }
    }
    {
        ZccScope *parent;
        parent = scope->parent;
        free(scope);
        return parent;
    }
}

int sym_define(ZccScope *sc, const char *name,
               int kind, Type *ty, int offset)
{
    ZccSymbol    *sym;
    ZccSymbol    *existing;
    unsigned int  bucket;
    ZCC_TAG(TAG_SCOPE);

    if (!sc || !name) return 0;

    /* Reject redefinition in the same scope level */
    existing = sym_lookup_local(sc, name);
    if (existing) {
        fprintf(stderr,
                "zcc: sym_define: redefinition of '%s' at depth %d\n",
                name, sc->depth);
        return 0;
    }

    sym = (ZccSymbol *)zcc_sym_alloc(
              (unsigned int)sizeof(ZccSymbol));
    strncpy(sym->name, name, ZCC_SYM_NAME_MAX - 1);
    sym->name[ZCC_SYM_NAME_MAX - 1] = '\0';
    sym->kind        = kind;
    sym->scope_depth = sc->depth;
    sym->ty          = ty;
    sym->offset      = offset;
    sym->is_typedef  = (kind == ZCC_SYM_TYPEDEF)  ? 1 : 0;
    sym->is_param    = (kind == ZCC_SYM_PARAM)     ? 1 : 0;

    /* Fire coverage tag matching the symbol kind */
    switch (kind) {
    case ZCC_SYM_TYPEDEF:   ZCC_TAG(TAG_TYPEDEF); break;
    case ZCC_SYM_FUNC:      ZCC_TAG(TAG_FUNC);    break;
    case ZCC_SYM_GLOBAL:    ZCC_TAG(TAG_GLOBAL);  break;
    case ZCC_SYM_LOCAL:     ZCC_TAG(TAG_LOCAL);   break;
    case ZCC_SYM_PARAM:     ZCC_TAG(TAG_PARAM);   break;
    case ZCC_SYM_MEMBER:    ZCC_TAG(TAG_MEMBER);  break;
    case ZCC_SYM_LABEL:     ZCC_TAG(TAG_LABEL);   break;
    default:                                       break;
    }

    bucket             = strhash(name, ZCC_SCOPE_HASH_SIZE - 1u);
    sym->next          = sc->buckets[bucket];
    sc->buckets[bucket] = sym;
    sc->n_symbols++;
    return 1;
}

ZccSymbol *sym_lookup_local(ZccScope *sc, const char *name)
{
    ZccSymbol    *s;
    unsigned int  bucket;
    ZCC_TAG(TAG_SCOPE);
    if (!sc || !name) return NULL;
    bucket = strhash(name, ZCC_SCOPE_HASH_SIZE - 1u);
    s = sc->buckets[bucket];
    while (s) {
        if (strcmp(s->name, name) == 0) return s;
        s = s->next;
    }
    return NULL;
}

ZccSymbol *sym_lookup_recursive(ZccScope *sc, const char *name)
{
    ZccScope  *cur;
    ZccSymbol *found;
    ZCC_TAG(TAG_SCOPE);
    cur = sc;
    while (cur) {
        found = sym_lookup_local(cur, name);
        if (found) return found;
        cur = cur->parent;
    }
    return NULL;
}

ZccTagEntry *sym_lookup_tag_recursive(ZccTagTable *tt,
                                       const char *name, int kind)
{
    /* Delegate directly to the tag layer -- the two lookups
     * share the same scope-chain walk pattern. */
    ZCC_TAG(TAG_SCOPE);
    return tag_lookup_recursive(tt, name, kind);
}

ZccSymbol *sym_lookup_member_recursive(Type *struct_or_union_ty,
                                        const char *name)
{
    /*
     * Walk the member list of a struct or union type and return
     * a synthetic ZccSymbol for the named member.
     *
     * ADAPT-5: replace with ZCC's actual Member field names:
     *   struct_or_union_ty->members  -- head of Member linked list
     *   member->name                 -- member name (char[N] or char*)
     *   member->next                 -- next Member in list
     *   member->ty                   -- Member's Type*
     *   member->offset               -- byte offset within struct/union
     *
     * The returned ZccSymbol is allocated fresh each call.
     * Caller may free() it when done, or use an arena.
     */
    static ZccSymbol scratch;   /* scratch return; not thread-safe */
    ZCC_TAG(TAG_MEMBER);
    if (!struct_or_union_ty || !name) return NULL;

    /*
    {
        Member *m;
        m = struct_or_union_ty->members;   -- ADAPT-5
        while (m) {
            if (strcmp(m->name, name) == 0) {
                memset(&scratch, 0, sizeof(scratch));
                strncpy(scratch.name, name, ZCC_SYM_NAME_MAX - 1);
                scratch.kind   = ZCC_SYM_MEMBER;
                scratch.ty     = m->ty;     -- ADAPT-5
                scratch.offset = m->offset; -- ADAPT-5
                return &scratch;
            }
            m = m->next; -- ADAPT-5
        }
    }
    */
    (void)struct_or_union_ty;
    (void)name;
    (void)scratch;
    return NULL; /* ADAPT-5: wire member list traversal */
}

ZccSymbol *sym_shadow_check(ZccScope *sc, const char *name)
{
    /* Returns the first symbol named 'name' in any OUTER scope.
     * If non-NULL, the new definition will shadow it. */
    ZCC_TAG(TAG_SCOPE);
    if (!sc || !name) return NULL;
    return sym_lookup_recursive(sc->parent, name);
}

void sym_mangle_internal(const char *base, int uid,
                          char *out, int out_len)
{
    /* Produces compiler-internal unique name: __zcc_<base>_<uid>
     * Used for anonymous structs, synthetic temporaries, etc. */
    ZCC_TAG(TAG_SCOPE);
    snprintf(out, (size_t)out_len, "__zcc_%s_%d", base, uid);
    out[out_len - 1] = '\0';
}

void sym_dump_scope_tree(ZccScope *sc, int indent)
{
    int b;
    int i;
    ZccSymbol *s;
    if (!sc) return;
    for (i = 0; i < indent; i++) fputc(' ', stderr);
    fprintf(stderr, "[scope depth=%d kind=%d n=%d]\n",
            sc->depth, sc->kind, sc->n_symbols);
    for (b = 0; b < ZCC_SCOPE_HASH_SIZE; b++) {
        s = sc->buckets[b];
        while (s) {
            for (i = 0; i < indent + 2; i++) fputc(' ', stderr);
            fprintf(stderr, "  sym '%s' kind=%d depth=%d offset=%d%s%s\n",
                    s->name, s->kind, s->scope_depth, s->offset,
                    s->is_typedef ? " [typedef]" : "",
                    s->is_param   ? " [param]"   : "");
            s = s->next;
        }
    }
    /* Recurse into parent scope (outermost printed last) */
    if (sc->parent)
        sym_dump_scope_tree(sc->parent, indent + 4);
}

/* ============================================================
 * LAYER 2: Tag namespace layer
 *
 * The tag namespace (struct/union/enum tags) is separate from
 * the ordinary identifier namespace.  Both live in the same
 * syntactic scope but are looked up independently.
 * ============================================================ */

ZccTagTable *tag_table_push(ZccTagTable *parent)
{
    ZccTagTable *tt;
    ZCC_TAG(TAG_SCOPE);
    tt         = (ZccTagTable *)zcc_sym_alloc(
                     (unsigned int)sizeof(ZccTagTable));
    tt->parent = parent;
    return tt;
}

ZccTagTable *tag_table_pop(ZccTagTable *tt)
{
    int b;
    ZccTagEntry *e;
    ZccTagEntry *nx;
    ZccTagTable *parent;
    ZCC_TAG(TAG_SCOPE);
    if (!tt) return NULL;
    for (b = 0; b < ZCC_TAG_HASH_SIZE; b++) {
        e = tt->buckets[b];
        while (e) {
            nx = e->next;
            free(e);
            e = nx;
        }
    }
    parent = tt->parent;
    free(tt);
    return parent;
}

/* Internal: common define path for struct, union, and enum */
static ZccTagEntry *tag_define_internal(ZccTagTable *tt,
                                          const char *name,
                                          int kind, Type *ty)
{
    ZccTagEntry  *e;
    unsigned int  bucket;

    if (!tt || !name) return NULL;

    /*
     * If a forward declaration exists in this scope, complete it
     * rather than creating a new entry.
     */
    e = tag_lookup_recursive(tt, name, kind);
    if (e && !e->is_complete && ty) {
        tag_complete_type(e, ty);
        return e;
    }
    if (e && e->is_complete && e->ty == ty) {
        /* Exact same type object -- idempotent redefinition, OK */
        return e;
    }
    if (e && e->is_complete) {
        fprintf(stderr,
                "zcc: tag_define: conflicting redefinition of '%s' "
                "kind=%d\n", name, kind);
        return NULL;
    }

    e = (ZccTagEntry *)zcc_sym_alloc(
            (unsigned int)sizeof(ZccTagEntry));
    strncpy(e->name, name, ZCC_SYM_NAME_MAX - 1);
    e->name[ZCC_SYM_NAME_MAX - 1] = '\0';
    e->kind        = kind;
    e->is_complete = (ty != NULL) ? 1 : 0;
    e->ty          = ty;

    bucket              = strhash(name, ZCC_TAG_HASH_SIZE - 1u);
    e->next             = tt->buckets[bucket];
    tt->buckets[bucket] = e;
    tt->n_entries++;
    return e;
}

ZccTagEntry *tag_define_struct(ZccTagTable *tt,
                                const char *name, Type *ty)
{
    ZCC_TAG(TAG_STRUCT);
    return tag_define_internal(tt, name, ZCC_TAG_KIND_STRUCT, ty);
}

ZccTagEntry *tag_define_union(ZccTagTable *tt,
                               const char *name, Type *ty)
{
    ZCC_TAG(TAG_UNION);
    return tag_define_internal(tt, name, ZCC_TAG_KIND_UNION, ty);
}

ZccTagEntry *tag_define_enum(ZccTagTable *tt,
                              const char *name, Type *ty)
{
    ZCC_TAG(TAG_ENUM);
    return tag_define_internal(tt, name, ZCC_TAG_KIND_ENUM, ty);
}

ZccTagEntry *tag_lookup_recursive(ZccTagTable *tt,
                                   const char *name, int kind)
{
    ZccTagTable  *cur;
    ZccTagEntry  *e;
    unsigned int  bucket;
    ZCC_TAG(TAG_SCOPE);

    cur = tt;
    while (cur) {
        bucket = strhash(name, ZCC_TAG_HASH_SIZE - 1u);
        e = cur->buckets[bucket];
        while (e) {
            if (e->kind == kind && strcmp(e->name, name) == 0)
                return e;
            e = e->next;
        }
        cur = cur->parent;
    }
    return NULL;
}

int tag_complete_type(ZccTagEntry *e, Type *ty)
{
    ZCC_TAG(TAG_TYPE);
    if (!e || !ty) return 0;
    if (e->is_complete) {
        fprintf(stderr,
                "zcc: tag_complete_type: '%s' already complete\n",
                e->name);
        return 0;
    }
    e->ty          = ty;
    e->is_complete = 1;
    return 1;
}

int tag_is_complete(ZccTagEntry *e)
{
    ZCC_TAG(TAG_TYPE);
    return (e && e->is_complete) ? 1 : 0;
}

ZccTagEntry *tag_forward_declare(ZccTagTable *tt,
                                  const char *name, int kind)
{
    ZCC_TAG(TAG_TYPE);
    return tag_define_internal(tt, name, kind, NULL);
}

int tag_attach_members(ZccTagEntry *e, Member **members, int n)
{
    /*
     * Attach a parsed member array to a (now-complete) tag entry.
     *
     * Two-pass struct parsing:
     *   Pass 1: tag_forward_declare  -- registers incomplete entry
     *   Pass 2: parse body + tag_complete_type -- fills in ty
     *           tag_attach_members -- wires members into ty
     *
     * ADAPT-5: replace the commented body with ZCC's actual
     *   ty->members pointer and member->next linkage.
     */
    ZCC_TAG(TAG_STRUCT);
    if (!e || !members || n <= 0) return 0;
    if (!e->is_complete || !e->ty) {
        fprintf(stderr,
                "zcc: tag_attach_members: '%s' not yet complete\n",
                e->name);
        return 0;
    }
    /*
    {
        int i;
        members[0]->next = NULL;     -- ADAPT-5: head of linked list
        for (i = 1; i < n; i++)
            members[i-1]->next = members[i]; -- ADAPT-5
        e->ty->members = members[0]; -- ADAPT-5
    }
    */
    (void)members;
    (void)n;
    return 1; /* ADAPT-5: enable body above once member fields known */
}

/* ============================================================
 * LAYER 3: Type-recursion layer
 *
 * ADAPT-3: ZCC's Type struct fields assumed:
 *   ty->kind    -- integer kind (ZCC_TY_*)
 *   ty->size    -- sizeof in bytes (set by parser for complete types)
 *   ty->align   -- alignment in bytes
 *   ty->base    -- base Type* for pointers and arrays
 *
 * Replace ZCC_TY_* with TY_* if ZCC uses different names.
 * ============================================================ */

Type *type_canonical(Type *ty)
{
    /*
     * Follow typedef chains to the underlying base type.
     *
     * ADAPT-3: uncomment when ZCC's typedef kind is known:
     *   while (ty && ty->kind == ZCC_TY_TYPEDEF)
     *       ty = ty->base;
     */
    ZCC_TAG(TAG_TYPE);
    return ty;
}

int type_equal_recursive(Type *a, Type *b)
{
    /* Structural equality, not pointer equality */
    ZCC_TAG(TAG_TYPE);
    if (a == b)   return 1;
    if (!a || !b) return 0;
    a = type_canonical(a);
    b = type_canonical(b);
    if (a->kind != b->kind) return 0; /* ADAPT-3: kind field */
    switch (a->kind) {
    case ZCC_TY_PTR:
        /* ADAPT-3: return type_equal_recursive(a->base, b->base); */
        return 1;
    case ZCC_TY_ARRAY:
        /* ADAPT-3: check array length AND base type equality */
        return 1;
    case ZCC_TY_FUNC:
        /* ADAPT-3: check return + parameter types recursively */
        return 1;
    case ZCC_TY_STRUCT:
    case ZCC_TY_UNION:
        /* Nominal typing: pointer identity is required */
        return 0; /* different struct types are never equal */
    default:
        return 1; /* scalar: kind equality is sufficient */
    }
}

Type *type_decay_array(Type *ty)
{
    /* C11 §6.3.2.1: array type → pointer to element type */
    ZCC_TAG(TAG_TYPE);
    if (!ty) return NULL;
    if (ty->kind != ZCC_TY_ARRAY) return ty; /* ADAPT-3 */
    return type_pointer_to(ty); /* ADAPT-3: should be ty->base */
}

Type *type_decay_func(Type *ty)
{
    /* C11 §6.3.2.1: function type → pointer to function */
    ZCC_TAG(TAG_TYPE);
    if (!ty) return NULL;
    if (ty->kind != ZCC_TY_FUNC) return ty; /* ADAPT-3 */
    return type_pointer_to(ty);
}

Type *type_pointer_to(Type *base)
{
    /*
     * Allocate a pointer-to-base Type.
     * ADAPT-3: If ZCC has a type-interning pool, use it here to
     *          avoid creating duplicate pointer types:
     *            return zcc_get_or_create_ptr_type(base);
     */
    Type *pty;
    ZCC_TAG(TAG_TYPE);
    pty = (Type *)zcc_sym_alloc((unsigned int)sizeof(Type));
    pty->kind  = ZCC_TY_PTR;  /* ADAPT-3 */
    pty->size  = 8;            /* LP64: all pointers are 8 bytes */
    pty->align = 8;
    /* ADAPT-3: pty->base = base; */
    (void)base;
    return pty;
}

Type *type_array_of(Type *base, int n)
{
    Type *aty;
    ZCC_TAG(TAG_TYPE);
    aty = (Type *)zcc_sym_alloc((unsigned int)sizeof(Type));
    aty->kind  = ZCC_TY_ARRAY; /* ADAPT-3 */
    aty->align = type_alignof_recursive(base);
    aty->size  = type_sizeof_recursive(base) * n;
    /* ADAPT-3: aty->base = base; aty->array_len = n; */
    (void)base;
    (void)n;
    return aty;
}

Type *type_function_of(Type *ret, Type **params,
                        int n_params, int is_vararg)
{
    Type *fty;
    ZCC_TAG(TAG_TYPE);
    ZCC_TAG(TAG_ABI);
    fty = (Type *)zcc_sym_alloc((unsigned int)sizeof(Type));
    fty->kind = ZCC_TY_FUNC; /* ADAPT-3 */
    fty->size  = 0; /* sizeof(function-type) is undefined */
    fty->align = 1;
    /* ADAPT-3:
     *   fty->return_ty  = ret;
     *   fty->params     = params;
     *   fty->n_params   = n_params;
     *   fty->is_vararg  = is_vararg;
     */
    (void)ret; (void)params; (void)n_params; (void)is_vararg;
    return fty;
}

int type_struct_member_offset(Type *ty, const char *member_name)
{
    /* Returns byte offset of named member within struct/union, or -1. */
    ZCC_TAG(TAG_MEMBER);
    ZCC_TAG(TAG_ABI);
    if (!ty || !member_name) return -1;
    /*
     * ADAPT-5:
     *   {
     *       Member *m = ty->members;
     *       while (m) {
     *           if (strcmp(m->name, member_name) == 0) return m->offset;
     *           m = m->next;
     *       }
     *   }
     */
    (void)member_name;
    return -1;
}

int type_sizeof_recursive(Type *ty)
{
    ZCC_TAG(TAG_TYPE);
    ZCC_TAG(TAG_ABI);
    if (!ty) return 0;
    ty = type_canonical(ty);
    switch (ty->kind) { /* ADAPT-3 */
    case ZCC_TY_VOID:                           return 0;
    case ZCC_TY_CHAR:   case ZCC_TY_UCHAR:     return 1;
    case ZCC_TY_SHORT:  case ZCC_TY_USHORT:    return 2;
    case ZCC_TY_INT:    case ZCC_TY_UINT:      return 4;
    case ZCC_TY_LONG:   case ZCC_TY_ULONG:     return 8; /* LP64 */
    case ZCC_TY_LLONG:  case ZCC_TY_ULLONG:    return 8;
    case ZCC_TY_FLOAT:                          return 4;
    case ZCC_TY_DOUBLE:                         return 8;
    case ZCC_TY_PTR:                            return 8; /* LP64 */
    case ZCC_TY_ENUM:                           return 4; /* compat int */
    case ZCC_TY_ARRAY:
    case ZCC_TY_STRUCT:
    case ZCC_TY_UNION:
        /* ADAPT-3: ty->size is pre-computed by the parser */
        return ty->size;
    case ZCC_TY_FUNC:
        return 0; /* undefined; caller must decay to pointer first */
    default:
        fprintf(stderr,
                "zcc: type_sizeof_recursive: unknown kind %d\n",
                ty->kind);
        return 0;
    }
}

int type_alignof_recursive(Type *ty)
{
    ZCC_TAG(TAG_TYPE);
    ZCC_TAG(TAG_ABI);
    if (!ty) return 1;
    ty = type_canonical(ty);
    switch (ty->kind) { /* ADAPT-3 */
    case ZCC_TY_VOID:                           return 1;
    case ZCC_TY_CHAR:   case ZCC_TY_UCHAR:     return 1;
    case ZCC_TY_SHORT:  case ZCC_TY_USHORT:    return 2;
    case ZCC_TY_INT:    case ZCC_TY_UINT:      return 4;
    case ZCC_TY_LONG:   case ZCC_TY_ULONG:     return 8;
    case ZCC_TY_LLONG:  case ZCC_TY_ULLONG:    return 8;
    case ZCC_TY_FLOAT:                          return 4;
    case ZCC_TY_DOUBLE:                         return 8;
    case ZCC_TY_PTR:                            return 8;
    case ZCC_TY_ENUM:                           return 4;
    case ZCC_TY_ARRAY:
    case ZCC_TY_STRUCT:
    case ZCC_TY_UNION:
        /* ADAPT-3: ty->align is pre-computed by the parser */
        return ty->align;
    default:
        return 1;
    }
}

int type_integer_rank(Type *ty)
{
    /* C standard integer conversion rank (higher = wider / more precise) */
    ZCC_TAG(TAG_TYPE);
    if (!ty) return 0;
    ty = type_canonical(ty);
    switch (ty->kind) { /* ADAPT-3 */
    case ZCC_TY_CHAR:  case ZCC_TY_UCHAR:   return 1;
    case ZCC_TY_SHORT: case ZCC_TY_USHORT:  return 2;
    case ZCC_TY_INT:   case ZCC_TY_UINT:    return 3;
    case ZCC_TY_LONG:  case ZCC_TY_ULONG:   return 4;
    case ZCC_TY_LLONG: case ZCC_TY_ULLONG:  return 5;
    case ZCC_TY_ENUM:                  return 3; /* int-compatible */
    default:                           return 0; /* non-integer */
    }
}

Type *type_usual_arith_conv(Type *a, Type *b)
{
    /*
     * C11 §6.3.1.8 usual arithmetic conversions.
     * Returns the type both operands should be converted to.
     */
    int ra;
    int a_unsigned;
    int b_unsigned;
    Type *tmp;
    ZCC_TAG(TAG_TYPE);

    a = type_canonical(a);
    b = type_canonical(b);

    /* Floating-point promotions */
    if (a->kind == ZCC_TY_DOUBLE || b->kind == ZCC_TY_DOUBLE) /* ADAPT-3 */
        return (a->kind == ZCC_TY_DOUBLE) ? a : b;
    if (a->kind == ZCC_TY_FLOAT  || b->kind == ZCC_TY_FLOAT)
        return (a->kind == ZCC_TY_FLOAT) ? a : b;

    /* Ensure a has the higher rank */
    if (type_integer_rank(a) < type_integer_rank(b)) {
        tmp = a; a = b; b = tmp;
    }
    ra = type_integer_rank(a);
    (void)ra;

    /* ADAPT-3: check signedness via ty->is_unsigned if ZCC has it,
     *          otherwise use kind comparison below. */
    a_unsigned = (a->kind == ZCC_TY_UCHAR  ||
                  a->kind == ZCC_TY_USHORT ||
                  a->kind == ZCC_TY_UINT   ||
                  a->kind == ZCC_TY_ULONG  ||
                  a->kind == ZCC_TY_ULLONG);
    b_unsigned = (b->kind == ZCC_TY_UCHAR  ||
                  b->kind == ZCC_TY_USHORT ||
                  b->kind == ZCC_TY_UINT   ||
                  b->kind == ZCC_TY_ULONG  ||
                  b->kind == ZCC_TY_ULLONG);

    if (a_unsigned == b_unsigned)
        return a; /* same signedness: higher rank wins */
    if (a_unsigned)
        return a; /* unsigned higher rank wins outright */
    /* Signed a, unsigned b (b has lower rank) */
    if (type_sizeof_recursive(a) > type_sizeof_recursive(b))
        return a; /* signed can represent all unsigned values -- signed wins */
    /* Return unsigned version of a */
    /* ADAPT-3: return zcc_make_unsigned(a) from ZCC's type table */
    return a;
}

Type *type_lvalue_conversion(Type *ty)
{
    /* Apply lvalue conversion: array→ptr, func→ptr, drop qualifiers */
    ZCC_TAG(TAG_TYPE);
    if (!ty) return NULL;
    if (ty->kind == ZCC_TY_ARRAY) return type_decay_array(ty); /* ADAPT-3 */
    if (ty->kind == ZCC_TY_FUNC)  return type_decay_func(ty);
    /* ADAPT-3: strip const/volatile qualifiers if ZCC tracks them */
    return ty;
}

/* ============================================================
 * LAYER 4: AST recursive walkers
 *
 * ADAPT-4: ZCC's Node struct assumed fields:
 *   node->kind      -- int (ND_ADD, ND_IF, ND_FOR, ND_VAR, ...)
 *   node->lhs       -- left child (or callee for ND_CALL)
 *   node->rhs       -- right child
 *   node->ty        -- Type* of the expression
 *   node->line      -- source line number
 *   node->next      -- next in a list (args, stmts in block)
 *   node->cond      -- condition (ND_IF, ND_WHILE, ND_FOR)
 *   node->then      -- true branch (ND_IF)
 *   node->els       -- false branch (ND_IF)
 *   node->body      -- loop body / compound body
 *   node->init      -- for-loop initializer
 *   node->inc       -- for-loop increment
 *
 * Replace ND_* with ZCC's actual node kind constants.
 * ============================================================ */

/*
 * ADAPT-4: placeholder ND_* values.
 * Replace these with ZCC's actual enum constants from part2.c/part3.c.
 * Grep for: ND_IF ND_FOR ND_WHILE ND_BLOCK ND_RETURN ND_CALL ND_ASSIGN
 */
#ifndef ND_BLOCK
#define ND_BLOCK    100
#define ND_IF       101
#define ND_WHILE    102
#define ND_FOR      103
#define ND_RETURN   104
#define ND_ASSIGN   105
#define ND_CALL     107
#define ND_VAR      108
#define ND_DEREF    109
#define ND_ADDR     110
#define ND_CAST     111
#define ND_DECL     112
#endif

/* Internal: single-node dispatch, shared by pre and post walkers */
static void ast_walk_dispatch(Node *node,
                               ZccASTVisitor fn, void *ctx, int post)
{
    if (!node || !fn) return;
    if (!post) fn(node, ctx); /* pre-order: visit before children */

    /* ADAPT-4: fill in actual field names for each ZCC node kind */
    switch (node->kind) {
    case ND_BLOCK:
        /* ADAPT-4:
         *   {
         *       Node *s = node->body;
         *       while (s) { ast_walk_dispatch(s, fn, ctx, post); s = s->next; }
         *   }
         */
        break;
    case ND_IF:
        /* ADAPT-4:
         *   ast_walk_dispatch(node->cond, fn, ctx, post);
         *   ast_walk_dispatch(node->then, fn, ctx, post);
         *   if (node->els) ast_walk_dispatch(node->els, fn, ctx, post);
         */
        break;
    case ND_WHILE:
        /* ADAPT-4:
         *   ast_walk_dispatch(node->cond, fn, ctx, post);
         *   ast_walk_dispatch(node->body, fn, ctx, post);
         */
        break;
    case ND_FOR:
        /* ADAPT-4:
         *   ast_walk_dispatch(node->init, fn, ctx, post);
         *   ast_walk_dispatch(node->cond, fn, ctx, post);
         *   ast_walk_dispatch(node->inc,  fn, ctx, post);
         *   ast_walk_dispatch(node->body, fn, ctx, post);
         */
        break;
    case ND_RETURN:
        /* ADAPT-4: if (node->lhs) ast_walk_dispatch(node->lhs, fn, ctx, post); */
        break;
    case ND_ASSIGN:
        /* ADAPT-4:
         *   ast_walk_dispatch(node->lhs, fn, ctx, post);
         *   ast_walk_dispatch(node->rhs, fn, ctx, post);
         */
        break;
    case ND_CALL:
        /* ADAPT-4:
         *   ast_walk_dispatch(node->lhs, fn, ctx, post);  -- callee
         *   { Node *a = node->args; while (a) { ast_walk_dispatch(a, fn, ctx, post); a = a->next; } }
         */
        break;
    case ND_DEREF:
    case ND_ADDR:
    case ND_CAST:
        /* ADAPT-4: ast_walk_dispatch(node->lhs, fn, ctx, post); */
        break;
    default:
        /* Binary ops with two children */
        /* ADAPT-4:
         *   if (node->lhs) ast_walk_dispatch(node->lhs, fn, ctx, post);
         *   if (node->rhs) ast_walk_dispatch(node->rhs, fn, ctx, post);
         */
        break;
    }
    /* Walk sibling list (list-structured nodes) */
    /* ADAPT-4: if (node->next) ast_walk_dispatch(node->next, fn, ctx, post); */

    if (post) fn(node, ctx); /* post-order: visit after children */
}

void ast_walk_preorder(Node *root, ZccASTVisitor fn, void *ctx)
{
    ZCC_TAG(TAG_PARSER);
    ast_walk_dispatch(root, fn, ctx, 0);
}

void ast_walk_postorder(Node *root, ZccASTVisitor fn, void *ctx)
{
    ZCC_TAG(TAG_PARSER);
    ast_walk_dispatch(root, fn, ctx, 1);
}

Node *ast_rewrite_expr_recursive(Node *expr,
                                  ZccASTRewriter fn, void *ctx)
{
    /*
     * Bottom-up expression rewrite: transform children first, then this
     * node.  Returns replacement node (may be same pointer if unchanged).
     */
    ZCC_TAG(TAG_PARSER);
    if (!expr || !fn) return expr;
    /* ADAPT-4: recurse children before calling fn:
     *   if (expr->lhs) expr->lhs = ast_rewrite_expr_recursive(expr->lhs, fn, ctx);
     *   if (expr->rhs) expr->rhs = ast_rewrite_expr_recursive(expr->rhs, fn, ctx);
     */
    return fn(expr, ctx);
}

Node *ast_rewrite_stmt_recursive(Node *stmt,
                                  ZccASTRewriter fn, void *ctx)
{
    ZCC_TAG(TAG_PARSER);
    if (!stmt || !fn) return stmt;
    /* ADAPT-4: recurse statement children:
     *   if (stmt->body) stmt->body = ast_rewrite_stmt_recursive(stmt->body, fn, ctx);
     *   if (stmt->cond) stmt->cond = ast_rewrite_expr_recursive(stmt->cond, fn, ctx);
     */
    return fn(stmt, ctx);
}

/* --- ast_collect_symbols --- */

typedef struct {
    ZccScope *out;
    int       n;
} SymCollectCtx;

static void sym_collect_visitor(Node *node, void *vctx)
{
    SymCollectCtx *cx;
    cx = (SymCollectCtx *)vctx;
    if (!node) return;
    /* ADAPT-4: detect declaration nodes and extract symbol info:
     *   if (node->kind == ND_DECL) {
     *       int kind = ... determine ZCC_SYM_LOCAL/PARAM/GLOBAL ...;
     *       sym_define(cx->out, node->name, kind, node->ty, node->offset);
     *       cx->n++;
     *   }
     */
    (void)cx;
}

void ast_collect_symbols(Node *root, ZccScope *out_scope)
{
    SymCollectCtx cx;
    ZCC_TAG(TAG_PARSER);
    ZCC_TAG(TAG_SCOPE);
    cx.out = out_scope;
    cx.n   = 0;
    ast_walk_preorder(root, sym_collect_visitor, &cx);
}

/* --- ast_collect_tags --- */

typedef struct {
    ZccTagTable *out;
    int          n;
} TagCollectCtx;

static void tag_collect_visitor(Node *node, void *vctx)
{
    TagCollectCtx *cx;
    cx = (TagCollectCtx *)vctx;
    if (!node) return;
    /* ADAPT-4: detect struct/union/enum definition nodes:
     *   if (node->kind == ND_STRUCT_DEF)
     *       tag_define_struct(cx->out, node->tag_name, node->ty);
     *   else if (node->kind == ND_UNION_DEF)
     *       tag_define_union(cx->out, node->tag_name, node->ty);
     *   else if (node->kind == ND_ENUM_DEF)
     *       tag_define_enum(cx->out, node->tag_name, node->ty);
     *   cx->n++;
     */
    (void)cx;
}

void ast_collect_tags(Node *root, ZccTagTable *out_tt)
{
    TagCollectCtx cx;
    ZCC_TAG(TAG_PARSER);
    ZCC_TAG(TAG_STRUCT);
    cx.out = out_tt;
    cx.n   = 0;
    ast_walk_preorder(root, tag_collect_visitor, &cx);
}

/* --- ast_validate_types --- */

typedef struct {
    int n_errors;
} TypeValidCtx;

static void type_validate_visitor(Node *node, void *vctx)
{
    TypeValidCtx *cx;
    cx = (TypeValidCtx *)vctx;
    if (!node) return;
    /* ADAPT-4: post-order invariant checks per node kind.
     *
     * Example checks:
     *   Arithmetic node: both operands must have integer or float type.
     *   Assignment: lhs must be an lvalue with compatible type.
     *   Call: callee must be a function or pointer-to-function type.
     *
     *   if (node->ty == NULL && node_requires_type(node)) {
     *       fprintf(stderr, "zcc: type missing at line %d kind=%d\n",
     *               node->line, node->kind);
     *       cx->n_errors++;
     *   }
     */
    (void)cx;
}

int ast_validate_types(Node *root)
{
    TypeValidCtx cx;
    ZCC_TAG(TAG_PARSER);
    ZCC_TAG(TAG_TYPE);
    ZCC_TAG(TAG_REGRESSION);
    cx.n_errors = 0;
    ast_walk_postorder(root, type_validate_visitor, &cx);
    return (cx.n_errors == 0) ? 1 : 0;
}

/* --- ast_resolve_identifiers --- */

typedef struct {
    ZccScope    *sc;
    ZccTagTable *tt;
    int          n_errors;
} ResolveCtx;

static void resolve_visitor(Node *node, void *vctx)
{
    ResolveCtx  *cx;
    ZccSymbol   *sym;
    cx = (ResolveCtx *)vctx;
    if (!node) return;
    /* ADAPT-4: resolve ND_VAR nodes to their scope entry:
     *   if (node->kind == ND_VAR) {
     *       sym = sym_lookup_recursive(cx->sc, node->name);
     *       if (!sym) {
     *           fprintf(stderr, "zcc: undefined '%s' line %d\n",
     *                   node->name, node->line);
     *           cx->n_errors++;
     *       } else {
     *           node->var = sym;  -- attach resolved symbol
     *       }
     *   }
     */
    (void)cx; (void)sym;
}

int ast_resolve_identifiers(Node *root,
                             ZccScope *sc, ZccTagTable *tt)
{
    ResolveCtx cx;
    ZCC_TAG(TAG_PARSER);
    ZCC_TAG(TAG_SCOPE);
    cx.sc       = sc;
    cx.tt       = tt;
    cx.n_errors = 0;
    ast_walk_preorder(root, resolve_visitor, &cx);
    return (cx.n_errors == 0) ? 1 : 0;
}

/* --- ast_dump_tree --- */

static void dump_visitor(Node *node, void *vctx)
{
    int *depth;
    int  i;
    depth = (int *)vctx;
    if (!node) return;
    for (i = 0; i < *depth; i++) fputc(' ', stderr);
    fprintf(stderr, "Node kind=%-4d ty=%s line=%d\n",
            node->kind,                            /* ADAPT-4: kind field */
            node->ty ? "<typed>" : "<untyped>",    /* ADAPT-4: ty field */
            node->line);                            /* ADAPT-4: line field */
    *depth += 2;
}

void ast_dump_tree(Node *root, int indent)
{
    int depth;
    ZCC_TAG(TAG_PARSER);
    depth = indent;
    ast_walk_preorder(root, dump_visitor, &depth);
}

/* ============================================================
 * LAYER 5: IR bridge / completeness
 *
 * ADAPT-6: IRAsmCtx fields (from compiler_passes.c):
 *   ctx->out     FILE* -- already verified from SKILL.md section 4
 *
 * Functions from ir_bridge.h (already in concat scope):
 *   ir_bridge_fresh_tmp()    -- returns "%%tN"
 *   ir_save_result(buf)      -- snapshots ir_last_result
 *   ir_map_type(Type *ty)    -- returns ir_type_t
 *
 * ADAPT-6: if ir_type_name() doesn't exist in ir.h, the local
 * helper below provides it; guard with #ifndef ir_type_name.
 * ============================================================ */

#ifndef ZCC_CONCAT_BUILD
/*
 * Shim ir_type_t and helpers for standalone builds.
 * In the concat build, ir.h provides these -- remove this block.
 * Type/Node/Member stubs are defined in sym_type_ast_ir.h under
 * ZCC_STANDALONE_BUILD -- they are already in scope here.
 */
typedef int ir_type_t;
#define IR_I8   0
#define IR_I16  1
#define IR_I32  2
#define IR_I64  3
#define IR_U8   4
#define IR_U16  5
#define IR_U32  6
#define IR_U64  7
#define IR_PTR  8
#define IR_F32  9
#define IR_F64  10

static ir_type_t ir_map_type(Type *ty)
{
    if (!ty) return IR_I32;
    switch (ty->kind) {
    case ZCC_TY_CHAR:   return IR_I8;
    case ZCC_TY_UCHAR:  return IR_U8;
    case ZCC_TY_SHORT:  return IR_I16;
    case ZCC_TY_USHORT: return IR_U16;
    case ZCC_TY_INT:    return IR_I32;
    case ZCC_TY_UINT:   return IR_U32;
    case ZCC_TY_LONG:   return IR_I64;
    case ZCC_TY_ULONG:  return IR_U64;
    case ZCC_TY_PTR:    return IR_PTR;
    case ZCC_TY_FLOAT:  return IR_F32;
    case ZCC_TY_DOUBLE: return IR_F64;
    default:            return IR_I32;
    }
}

static const char *ir_bridge_fresh_tmp(void)
{
    static char buf[16];
    static int  ctr = 0;
    snprintf(buf, sizeof(buf), "%%t%d", ctr++);
    return buf;
}

static void ir_save_result(char *dst) { (void)dst; }

struct IRAsmCtx { FILE *out; };
struct Function { int n_blocks; };

#endif /* !ZCC_CONCAT_BUILD */

/* ir_type_name: maps ir_type_t to the text representation in ZCC IR */
static const char *ir_type_name_local(ir_type_t t)
{
    switch (t) {
    case IR_I8:  return "i8";
    case IR_I16: return "i16";
    case IR_I32: return "i32";
    case IR_I64: return "i64";
    case IR_U8:  return "u8";
    case IR_U16: return "u16";
    case IR_U32: return "u32";
    case IR_U64: return "u64";
    case IR_PTR: return "ptr";
    case IR_F32: return "f32";
    case IR_F64: return "f64";
    default:     return "?";
    }
}

/* In the concat build, guard against duplicate definition */
#ifndef ZCC_IR_TYPE_NAME_DEFINED
#define ZCC_IR_TYPE_NAME_DEFINED
#define ir_type_name ir_type_name_local
#endif

/* --- IR emit functions --- */

void ir_emit_expr_recursive(struct IRAsmCtx *ctx, Node *expr)
{
    /*
     * Dispatch table for recursive IR expression emission.
     * Mirrors codegen_expr() in part4.c but emits 3-address IR.
     *
     * ADAPT-4 / ADAPT-6: fill in each ND_* case following the
     * Save-Before-Recurse pattern from ir_bridge.h section 2:
     *
     *   case ND_ADD: {
     *       char lhs_ir[32];
     *       char rhs_ir[32];
     *       ir_emit_expr_recursive(ctx, expr->lhs);
     *       ir_save_result(lhs_ir);
     *       ir_emit_expr_recursive(ctx, expr->rhs);
     *       ir_save_result(rhs_ir);
     *       fprintf(ctx->out, "  %%t%d = add i32 %s, %s\n",
     *               ... ir_bridge_fresh_tmp() ..., lhs_ir, rhs_ir);
     *       break;
     *   }
     */
    ZCC_TAG(TAG_IR_OP);
    ZCC_TAG(TAG_CODEGEN);
    if (!ctx || !expr) return;
    /* ADAPT-4: switch (expr->kind) { ... } */
}

void ir_emit_stmt_recursive(struct IRAsmCtx *ctx, Node *stmt)
{
    ZCC_TAG(TAG_IR_OP);
    ZCC_TAG(TAG_CODEGEN);
    if (!ctx || !stmt) return;
    /* ADAPT-4: switch (stmt->kind) for ND_IF, ND_FOR, ND_WHILE, ND_RETURN */
    ir_emit_control_flow(ctx, stmt);
}

void ir_emit_lvalue_addr(struct IRAsmCtx *ctx, Node *expr)
{
    /*
     * Emit an IR 'addr' instruction for an lvalue.
     * Result is left in ir_last_result; caller uses ir_save_result().
     *
     * ZCC IR op: addr <src>  ->  %tN = addr <var_or_deref>
     */
    const char *tmp;
    ZCC_TAG(TAG_IR_OP);
    ZCC_TAG(TAG_CODEGEN);
    if (!ctx || !expr) return;
    tmp = ir_bridge_fresh_tmp(); /* ADAPT-6 */
    fprintf(ctx->out, "  %s = addr ; ADAPT: %s source lvalue\n",
            tmp, "???"); /* ADAPT-4: emit actual lvalue source */
    ir_save_result((char *)tmp); /* ADAPT-6: save so caller can read */
}

void ir_emit_load_if_lvalue(struct IRAsmCtx *ctx, Node *expr)
{
    /*
     * If expr is an lvalue (variable ref, deref, member access),
     * emit a load to materialise the rvalue.  No-op for rvalue exprs.
     *
     * ZCC IR op: load <type> <addr_tmp>  ->  %tN = load <ty> <ptr>
     */
    ZCC_TAG(TAG_IR_OP);
    ZCC_TAG(TAG_CODEGEN);
    if (!ctx || !expr) return;
    /* ADAPT-4: if (expr_is_lvalue(expr)) { ... emit load ... } */
}

void ir_emit_store(struct IRAsmCtx *ctx,
                   const char *addr_tmp, const char *val_tmp, Type *ty)
{
    /*
     * Emit: store <ir_type> <val_tmp>, <addr_tmp>
     * Rule from CG-IR-010: always use ptr-width move (8 bytes on LP64).
     */
    ZCC_TAG(TAG_IR_OP);
    ZCC_TAG(TAG_CODEGEN);
    ZCC_TAG(TAG_ABI);
    if (!ctx || !addr_tmp || !val_tmp || !ty) return;
    fprintf(ctx->out, "  store %s %s, %s\n",
            ir_type_name(ir_map_type(ty)),  /* ADAPT-6 */
            val_tmp, addr_tmp);
}

void ir_emit_cast(struct IRAsmCtx *ctx,
                  const char *src_tmp, Type *from_ty, Type *to_ty,
                  char *dst_out, int dst_len)
{
    /*
     * Emit sign/zero extension or truncation.
     * Respects LP64 widening rules (CG-001 in compiler_forge SKILL).
     */
    int  from_sz;
    int  to_sz;
    int  from_unsigned;
    ZCC_TAG(TAG_IR_OP);
    ZCC_TAG(TAG_CODEGEN);
    ZCC_TAG(TAG_ABI);

    if (!ctx || !src_tmp || !from_ty || !to_ty || !dst_out) return;
    from_sz = type_sizeof_recursive(from_ty);
    to_sz   = type_sizeof_recursive(to_ty);
    from_unsigned = (from_ty->kind == ZCC_TY_UCHAR  ||
                     from_ty->kind == ZCC_TY_USHORT  ||
                     from_ty->kind == ZCC_TY_UINT    ||
                     from_ty->kind == ZCC_TY_ULONG   ||
                     from_ty->kind == ZCC_TY_ULLONG); /* ADAPT-3 */

    snprintf(dst_out, (size_t)dst_len, "%s", ir_bridge_fresh_tmp());
    dst_out[dst_len - 1] = '\0';

    if (to_sz == from_sz) {
        fprintf(ctx->out, "  %s = bitcast %s %s to %s\n",
                dst_out,
                ir_type_name(ir_map_type(from_ty)), src_tmp,
                ir_type_name(ir_map_type(to_ty)));
    } else if (to_sz > from_sz) {
        /* Widening: zero-extend for unsigned, sign-extend for signed */
        if (from_unsigned) {
            fprintf(ctx->out, "  %s = zext %s %s to %s\n",
                    dst_out,
                    ir_type_name(ir_map_type(from_ty)), src_tmp,
                    ir_type_name(ir_map_type(to_ty)));
        } else {
            fprintf(ctx->out, "  %s = sext %s %s to %s\n",
                    dst_out,
                    ir_type_name(ir_map_type(from_ty)), src_tmp,
                    ir_type_name(ir_map_type(to_ty)));
        }
    } else {
        /* Narrowing truncation */
        fprintf(ctx->out, "  %s = trunc %s %s to %s\n",
                dst_out,
                ir_type_name(ir_map_type(from_ty)), src_tmp,
                ir_type_name(ir_map_type(to_ty)));
    }
    (void)from_sz; (void)to_sz;
}

void ir_emit_short_circuit(struct IRAsmCtx *ctx, Node *expr,
                             int is_and,
                             char *result_out, int out_len)
{
    /*
     * Short-circuit evaluation for && (is_and=1) and || (is_and=0).
     * Uses br_if to skip RHS when LHS determines the result.
     *
     * ZCC IR ops used: br_if, br, label
     * No PHI needed: result is stored to a stack slot (non-SSA style).
     *
     * Pattern:
     *   eval LHS --> %lhs
     *   br_if %lhs .Lrhs .Lskip   (for &&: if false, skip; for ||: if true, skip)
     *   .Lrhs:
     *     eval RHS --> %rhs
     *     store %rhs to result_slot
     *     br .Lend
     *   .Lskip:
     *     store (is_and ? 0 : 1) to result_slot
     *     br .Lend
     *   .Lend:
     *     %result = load result_slot
     */
    static int sc_ctr = 0;
    int  lbl_rhs;
    int  lbl_skip;
    int  lbl_end;
    char lhs_tmp[32];
    char rhs_tmp[32];
    ZCC_TAG(TAG_IR_OP);
    ZCC_TAG(TAG_CODEGEN);

    if (!ctx || !expr || !result_out) return;
    lbl_rhs  = ++sc_ctr;
    lbl_skip = ++sc_ctr;
    lbl_end  = ++sc_ctr;

    /* Evaluate LHS -- ADAPT-4: expr->lhs */
    ir_emit_expr_recursive(ctx, expr /* ADAPT: ->lhs */);
    ir_save_result(lhs_tmp);

    if (is_and) {
        fprintf(ctx->out, "  br_if %s .Lsc_rhs_%d .Lsc_skip_%d\n",
                lhs_tmp, lbl_rhs, lbl_skip);
    } else {
        fprintf(ctx->out, "  br_if %s .Lsc_skip_%d .Lsc_rhs_%d\n",
                lhs_tmp, lbl_skip, lbl_rhs);
    }

    fprintf(ctx->out, ".Lsc_rhs_%d:\n", lbl_rhs);
    ir_emit_expr_recursive(ctx, expr /* ADAPT: ->rhs */);
    ir_save_result(rhs_tmp);
    snprintf(result_out, (size_t)out_len, "%s", rhs_tmp);
    fprintf(ctx->out, "  br .Lsc_end_%d\n", lbl_end);

    fprintf(ctx->out, ".Lsc_skip_%d:\n", lbl_skip);
    snprintf(result_out, (size_t)out_len, "%%t_sc_%d", lbl_skip);
    fprintf(ctx->out, "  %s = const i32 %d\n",
            result_out, is_and ? 0 : 1);
    fprintf(ctx->out, "  br .Lsc_end_%d\n", lbl_end);

    fprintf(ctx->out, ".Lsc_end_%d:\n", lbl_end);
    (void)lhs_tmp; (void)rhs_tmp;
}

void ir_emit_call_args(struct IRAsmCtx *ctx,
                        Node *arg_list, int *n_args_out)
{
    /*
     * Evaluate each argument in the call's arg list and collect
     * the resulting temporary names for the subsequent 'call' op.
     *
     * ZCC IR: arg <type> <tmp>  (one line per argument)
     */
    Node *arg;
    int   n;
    char  tmp[32];
    ZCC_TAG(TAG_IR_OP);
    ZCC_TAG(TAG_CODEGEN);
    ZCC_TAG(TAG_ABI);

    if (!ctx || !n_args_out) return;
    n   = 0;
    arg = arg_list;
    while (arg) {
        ir_emit_expr_recursive(ctx, arg);
        ir_save_result(tmp);
        fprintf(ctx->out, "  arg %s %s\n",
                arg->ty ? ir_type_name(ir_map_type(arg->ty)) : "i32",
                tmp);      /* ADAPT-4: arg->ty */
        n++;
        arg = arg->next;   /* ADAPT-4: next arg in list */
    }
    *n_args_out = n;
}

void ir_emit_control_flow(struct IRAsmCtx *ctx, Node *stmt)
{
    /*
     * Emit IR control-flow instructions for if/while/for/return.
     * ZCC IR ops: br, br_if, ret, label
     *
     * ADAPT-4: uncomment and expand each case with ZCC field names.
     */
    ZCC_TAG(TAG_IR_OP);
    ZCC_TAG(TAG_CODEGEN);
    if (!ctx || !stmt) return;
    switch (stmt->kind) { /* ADAPT-4 */
    case ND_RETURN:
        /* ADAPT-4:
         *   if (stmt->lhs) {
         *       ir_emit_expr_recursive(ctx, stmt->lhs);
         *       fprintf(ctx->out, "  ret %s ir_last_result\n", ...);
         *   } else {
         *       fprintf(ctx->out, "  ret void\n");
         *   }
         */
        break;
    case ND_IF:
        /* ADAPT-4: emit cond, br_if then_lbl else_lbl, labels, branches */
        break;
    case ND_WHILE:
        /* ADAPT-4: emit loop_head label, cond, br_if body_lbl end_lbl,
         *           body, br loop_head, end label */
        break;
    case ND_FOR:
        /* ADAPT-4: init, loop_head, cond check, body, inc, br loop_head */
        break;
    default:
        break;
    }
}

/* --- IR validation --- */

int ir_validate_module(struct Function **funcs, int n_funcs)
{
    /*
     * Validate all functions in the module.
     * Validation order matches the minimum coverage checklist:
     *   scopes → tags → typedefs → types → AST → IR → ABI → invariants
     */
    int i;
    int ok;
    ZCC_TAG(TAG_IR_OP);
    ZCC_TAG(TAG_REGRESSION);
    ok = 1;
    if (!funcs || n_funcs <= 0) return 0;
    for (i = 0; i < n_funcs; i++) {
        if (!funcs[i]) continue;
        if (!ir_validate_func(funcs[i])) {
            fprintf(stderr,
                    "zcc: ir_validate_module: func[%d] failed\n", i);
            ok = 0;
        }
    }
    return ok;
}

int ir_validate_func(struct Function *fn)
{
    /*
     * Per-function invariant checks:
     *   1. n_blocks > 0
     *   2. Every block has a terminator (ret, br, or br_if)
     *   3. No %tN used before defined (SSA dominance)
     *
     * ADAPT-6: fn->n_blocks, fn->blocks[], block->instrs[], instr->op
     */
    ZCC_TAG(TAG_IR_OP);
    ZCC_TAG(TAG_REGRESSION);
    if (!fn) return 0;
    /* ADAPT-6:
     *   if (fn->n_blocks == 0) { fprintf(stderr, "zcc: empty function\n"); return 0; }
     *   for (i = 0; i < fn->n_blocks; i++) {
     *       BasicBlock *bb = &fn->blocks[i];
     *       if (bb->n_instrs == 0 || !is_terminator(bb->instrs[bb->n_instrs-1].op))
     *           return 0;
     *   }
     */
    return ir_validate_operand_types(fn);
}

int ir_validate_operand_types(struct Function *fn)
{
    /*
     * Check type consistency for each instruction:
     *   add/sub/mul/div  -- operand types must match result type
     *   load             -- address operand must be ptr type
     *   store            -- address operand must be ptr type
     *   br_if            -- condition operand must be integer type
     *   call             -- callee must be ptr or func type
     *
     * ADAPT-6: iterate fn->blocks -> instrs, compare instr->op with
     * instr->lhs_type, instr->rhs_type per the OP_* enum in ir.h.
     *
     * ZCC IR ops from SKILL: add sub mul div mod band bor bxor shl shr
     *                         eq ne lt le gt ge load store addr call ret
     *                         label br br_if const arg
     */
    ZCC_TAG(TAG_IR_OP);
    ZCC_TAG(TAG_TYPE);
    ZCC_TAG(TAG_REGRESSION);
    if (!fn) return 0;
    /* ADAPT-6: validate per instruction */
    return 1;
}

/* ============================================================
 * Coverage hit tracker (ZCC_COVERAGE=1 only)
 * ============================================================ */

#ifdef ZCC_COVERAGE

ZccCoverageHit g_zcc_coverage_hits[ZCC_COVERAGE_MAX_HITS];
int            g_zcc_coverage_n = 0;

void zcc_coverage_hit(const char *tag, const char *file, int line)
{
    int i;
    /* Check for an existing entry to increment */
    for (i = 0; i < g_zcc_coverage_n; i++) {
        if (g_zcc_coverage_hits[i].tag  == tag &&
            g_zcc_coverage_hits[i].file == file &&
            g_zcc_coverage_hits[i].line == line) {
            g_zcc_coverage_hits[i].count++;
            return;
        }
    }
    if (g_zcc_coverage_n >= ZCC_COVERAGE_MAX_HITS) return;
    g_zcc_coverage_hits[g_zcc_coverage_n].tag   = tag;
    g_zcc_coverage_hits[g_zcc_coverage_n].file  = file;
    g_zcc_coverage_hits[g_zcc_coverage_n].line  = line;
    g_zcc_coverage_hits[g_zcc_coverage_n].count = 1;
    g_zcc_coverage_n++;
}

void zcc_coverage_report(void)
{
    int i;
    fprintf(stderr,
            "=== ZCC COVERAGE REPORT (%d hits, %d unique) ===\n",
            g_zcc_coverage_n, g_zcc_coverage_n);
    for (i = 0; i < g_zcc_coverage_n; i++) {
        fprintf(stderr, "  [%4d]  %-22s  %s:%d\n",
                g_zcc_coverage_hits[i].count,
                g_zcc_coverage_hits[i].tag,
                g_zcc_coverage_hits[i].file,
                g_zcc_coverage_hits[i].line);
    }
}

int zcc_coverage_count(const char *tag)
{
    int i;
    int total;
    total = 0;
    for (i = 0; i < g_zcc_coverage_n; i++) {
        if (strcmp(g_zcc_coverage_hits[i].tag, tag) == 0)
            total += g_zcc_coverage_hits[i].count;
    }
    return total;
}

#endif /* ZCC_COVERAGE */
