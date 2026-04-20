#ifndef ZCC_AST_BRIDGE_H
/* Exclusively for standalone IDE analysis */
#include "part1.c"
#endif

/* ================================================================ */
/* ARENA ALLOCATOR                                                   */
/* ================================================================ */

static unsigned long long next_alloc_id = 1;

/* Cap total arena blocks to avoid OOM from unbounded allocation (e.g. corrupt
 * AST / infinite loop). Use literal 64 so compiler without preprocessor still
 * works. */
void *cc_alloc(Compiler *cc, int size) {
  ArenaBlock *a;
  void *p;
  static int block_count = 0;
  size = (size + 7) & ~7; /* align to 8 */
  a = &cc->arena;
  while (a) {
    if (a->pos + size <= a->cap) {
      p = a->data + a->pos;
      a->pos = a->pos + size;
      /* zero the memory — use temp ptr to avoid complex cast expression */
      {
        char *cp;
        int i;
        cp = (char *)p;
        for (i = 0; i < size; i++) {
          cp[i] = 0;
        }
      }
      /* magic header: second qword = alloc_id for tracing */
      if (size >= 16) {
        *((unsigned long long *)((char *)p + 8)) = next_alloc_id++;
      }
      return p;
    }
    if (!a->next) {
      if (block_count >= 32768) {
        printf("zcc: too many arena blocks (%d) — possible infinite loop or "
               "corrupt AST (last alloc size=%d)\n",
               block_count, size);
        if (cc) {
          printf("zcc: stuck near token kind=%d line=%d text=%.127s\n", cc->tk,
                 cc->tk_line, cc->tk_text);
        }
        exit(1);
      }
      if (block_count >= 32000 && cc) {
        printf("zcc: near block limit block_count=%d token=%d line=%d size=%d "
               "text=%.127s\n",
               block_count, cc->tk, cc->tk_line, size, cc->tk_text);
      }
      block_count++;
      {
        int newcap;
        ArenaBlock *nb;
        newcap = ARENA_SIZE;
        if (size > newcap)
          newcap = size * 2;
        nb = (ArenaBlock *)malloc(sizeof(ArenaBlock));
        nb->data = (char *)malloc(newcap);
        nb->pos = 0;
        nb->cap = newcap;
        nb->next = 0;
        a->next = nb;
      }
    }
    a = a->next;
  }
  printf("zcc: arena alloc failed\n");
  exit(1);
  return 0;
}

char *cc_strdup(Compiler *cc, char *s) {
  int len;
  char *p;
  int i;
  len = 0;
  while (s[len])
    len++;
  p = (char *)cc_alloc(cc, len + 1);
  for (i = 0; i <= len; i++) {
    p[i] = s[i];
  }
  return p;
}

/* ================================================================ */
/* ERROR REPORTING                                                   */
/* ================================================================ */

void error(Compiler *cc, char *msg) {
  char *name;
  name = cc->filename;
  if (!name)
    name = "<input>";
  printf("%s:%d: error: %s\n", name, cc->tk_line, msg);
  cc->errors++;
}

void error_at(Compiler *cc, int line, char *msg) {
  char *name;
  name = cc->filename;
  if (!name)
    name = "<input>";
  printf("%s:%d: error: %s\n", name, line, msg);
  cc->errors++;
}

/* ================================================================ */
/* TYPE CONSTRUCTORS                                                 */
/* ================================================================ */

Type *type_new(Compiler *cc, int kind) {
  Type *t;
  t = (Type *)cc_alloc(cc, sizeof(Type));
  t->magic = 0xDEADBEEF87654321ULL;
  t->alloc_id = next_alloc_id - 1;
  t->kind = kind;
  switch (kind) {
  case TY_VOID:
    t->size = 0;
    t->align = 1;
    break;
  case TY_CHAR:
    t->size = 1;
    t->align = 1;
    break;
  case TY_UCHAR:
    t->size = 1;
    t->align = 1;
    break;
  case TY_SHORT:
    t->size = 2;
    t->align = 2;
    break;
  case TY_USHORT:
    t->size = 2;
    t->align = 2;
    break;
  case TY_INT:
    t->size = 4;
    t->align = 4;
    break;
  case TY_UINT:
    t->size = 4;
    t->align = 4;
    break;
  case TY_LONG:
    t->size = 8;
    t->align = 8;
    break;
  case TY_ULONG:
    t->size = 8;
    t->align = 8;
    break;
  case TY_LONGLONG:
    t->size = 8;
    t->align = 8;
    break;
  case TY_ULONGLONG:
    t->size = 8;
    t->align = 8;
    break;
  case TY_FLOAT:
    t->size = 4;
    t->align = 4;
    break;
  case TY_DOUBLE:
    t->size = 8;
    t->align = 8;
    break;
  case TY_PTR:
    t->size = 8;
    t->align = 8;
    break;
  case TY_ENUM:
    t->size = 4;
    t->align = 4;
    break;
  default:
    t->size = 0;
    t->align = 1;
    break;
  }
  validate_type(cc, t, "type_new", 0);
  return t;
}

Type *type_ptr(Compiler *cc, Type *base) {
  Type *t;
  t = type_new(cc, TY_PTR);
  t->base = base;
  return t;
}

Type *type_array(Compiler *cc, Type *base, int len) {
  Type *t;
  t = type_new(cc, TY_ARRAY);
  t->base = base;
  t->array_len = len;
  t->size = type_size(base) * len;
  t->align = type_align(base);
  return t;
}

Type *type_func(Compiler *cc, Type *ret) {
  Type *t;
  t = type_new(cc, TY_FUNC);
  t->ret = ret;
  t->size = 8;
  t->align = 8;
  return t;
}

int type_size(Type *t) {
  if (!t)
    return 8;
  return t->size;
}

int type_align(Type *t) {
  if (!t)
    return 8;
  return t->align;
}

int is_integer(Type *t) {
  if (!t)
    return 0;
  if (t->kind >= TY_CHAR) {
    if (t->kind <= TY_ULONGLONG)
      return 1;
  }
  if (t->kind == TY_ENUM)
    return 1;
  return 0;
}

int is_pointer(Type *t) {
  if (!t)
    return 0;
  if (t->kind == TY_PTR)
    return 1;
  if (t->kind == TY_ARRAY)
    return 1;
  return 0;
}

int is_float_type(Type *t) {
  if (!t)
    return 0;
  if (t->kind == TY_FLOAT)
    return 1;
  if (t->kind == TY_DOUBLE)
    return 1;
  return 0;
}

int is_unsigned_type(Type *t) {
  if (!t)
    return 0;
  if (t->kind == TY_UCHAR)
    return 1;
  if (t->kind == TY_USHORT)
    return 1;
  if (t->kind == TY_UINT)
    return 1;
  if (t->kind == TY_ULONG)
    return 1;
  if (t->kind == TY_ULONGLONG)
    return 1;
  return 0;
}

/* ================================================================ */
/* NODE CONSTRUCTORS                                                 */
/* ================================================================ */

Node *node_new(Compiler *cc, int kind, int line) {
  Node *n;
  n = (Node *)cc_alloc(cc, sizeof(Node));
  n->magic = 0xC0FFEEBAD1234567ULL;
  n->alloc_id = next_alloc_id - 1;
  n->kind = kind;
  n->line = line;
  validate_node(cc, n, "node_new", line);
  return n;
}

Node *node_num(Compiler *cc, long long val, int line) {
  Node *n;
  n = node_new(cc, ND_NUM, line);
  n->int_val = val;
  n->type = cc->ty_int;
  if (val > 2147483647 || val < -2147483648) {
    n->type = cc->ty_long;
  }
  return n;
}

Node *node_flit(Compiler *cc, double val, int line) {
  Node *n;
  n = node_new(cc, ND_FLIT, line);
  n->f_val = val;
  n->type = cc->ty_double;
  return n;
}

/* ================================================================ */
/* SCOPE MANAGEMENT                                                  */
/* ================================================================ */

void scope_push(Compiler *cc) {
  Scope *s;
  s = (Scope *)cc_alloc(cc, sizeof(Scope));
  s->parent = cc->current_scope;
  s->symbols = 0;
  cc->current_scope = s;
}

void scope_pop(Compiler *cc) {
  if (cc->current_scope) {
    cc->current_scope = cc->current_scope->parent;
  }
}

Symbol *scope_find(Compiler *cc, char *name) {
  Scope *s;
  Symbol *sym;
  s = cc->current_scope;
  while (s) {
    sym = s->symbols;
    while (sym) {
      if (strcmp(sym->name, name) == 0)
        return sym;
      sym = sym->next;
    }
    s = s->parent;
  }
  return 0;
}

Symbol *scope_find_local(Compiler *cc, char *name) {
  Symbol *sym;
  if (!cc->current_scope)
    return 0;
  sym = cc->current_scope->symbols;
  while (sym) {
    if (strcmp(sym->name, name) == 0)
      return sym;
    sym = sym->next;
  }
  return 0;
}

Symbol *scope_add(Compiler *cc, char *name, Type *type) {
  Symbol *sym;
  sym = (Symbol *)cc_alloc(cc, sizeof(Symbol));
  strncpy(sym->name, name, MAX_IDENT - 1);
  sym->type = type;
  sym->stack_offset = 0;
  sym->next = cc->current_scope->symbols;
  cc->current_scope->symbols = sym;
  if (strcmp(name, "yyParser") == 0) {
    printf("DEBUG: scope_add('yyParser') with type kind=%d tag='%s'\n",
           type ? type->kind : -1,
           (type && type->tag[0]) ? type->tag : "<none>");
    fflush(stdout);
  }
  return sym;
}

/* Add local with auto-assigned stack offset */
Symbol *scope_add_local(Compiler *cc, char *name, Type *type) {
  Symbol *sym;
  int sz;
  sym = scope_add(cc, name, type);
  sym->is_local = 1;
  sz = type_size(type);
  if (sz < 8)
    sz = 8; /* minimum 8-byte slots */
  cc->local_offset = cc->local_offset - sz;
  sym->stack_offset = cc->local_offset;
  return sym;
}

/* ================================================================ */
/* KEYWORD TABLE                                                     */
/* ================================================================ */

typedef struct {
  char *word;
  int token;
} Keyword;

static Keyword keywords[] = {{"int", TK_INT},
                             {"char", TK_CHAR},
                             {"void", TK_VOID},
                             {"long", TK_LONG},
                             {"short", TK_SHORT},
                             {"unsigned", TK_UNSIGNED},
                             {"signed", TK_SIGNED},
                             {"float", TK_FLOAT},
                             {"double", TK_DOUBLE},
                             {"if", TK_IF},
                             {"else", TK_ELSE},
                             {"while", TK_WHILE},
                             {"for", TK_FOR},
                             {"do", TK_DO},
                             {"return", TK_RETURN},
                             {"break", TK_BREAK},
                             {"continue", TK_CONTINUE},
                             {"goto", TK_GOTO},
                             {"switch", TK_SWITCH},
                             {"case", TK_CASE},
                             {"default", TK_DEFAULT},
                             {"struct", TK_STRUCT},
                             {"union", TK_UNION},
                             {"enum", TK_ENUM},
                             {"typedef", TK_TYPEDEF},
                             {"typeof", TK_TYPEOF},
                             {"__typeof__", TK_TYPEOF},
                             {"__typeof", TK_TYPEOF},
                             {"__auto_type", TK_AUTO_TYPE},
                             {"sizeof", TK_SIZEOF},
                             {"__builtin_va_arg", TK_BUILTIN_VA_ARG},
                             {"static", TK_STATIC},
                             {"extern", TK_EXTERN},
                             {"const", TK_CONST},
                             {"volatile", TK_VOLATILE},
                             {"auto", TK_AUTO},
                             {"register", TK_REGISTER},
                             {"inline", TK_INLINE},
                             {"asm", TK_ASM},
                             {"__asm__", TK_ASM},
                             {"__signed__", TK_SIGNED},
                             {"__signed", TK_SIGNED},
                             {"__const__", TK_CONST},
                             {"__const", TK_CONST},
                             {"__inline__", TK_INLINE},
                             {"__inline", TK_INLINE},
                             {"__volatile__", TK_VOLATILE},
                             {"__volatile", TK_VOLATILE},
                             {"__restrict__", TK_VOLATILE},
                             {"__restrict", TK_VOLATILE},
                             {"restrict", TK_VOLATILE},
                             {"__int128", TK_LONG},
                             {"__int128_t", TK_LONG},
                             {0, 0}};

static int kw_count = 53;

static int lookup_keyword(char *name) {
  int i;
  if (!name)
    return 0;
  for (i = 0; i < kw_count; i++) {
    if (!keywords[i].word)
      break;
    if (strcmp(keywords[i].word, name) == 0) {
      return keywords[i].token;
    }
  }
  return 0;
}

/* stage2 fallback: match ident by length and bytes (no strcmp/keyword table).
 * Bounds-safe. */
static int lookup_keyword_fallback(char *buf, int len) {
  if (!buf || len <= 0 || len > 16)
    return 0; /* max keyword length 16 */
  if (len == 3 && buf[0] == 'i' && buf[1] == 'n' && buf[2] == 't')
    return TK_INT;
  if (len == 4 && buf[0] == 'c' && buf[1] == 'h' && buf[2] == 'a' &&
      buf[3] == 'r')
    return TK_CHAR;
  if (len == 4 && buf[0] == 'v' && buf[1] == 'o' && buf[2] == 'i' &&
      buf[3] == 'd')
    return TK_VOID;
  if (len == 4 && buf[0] == 'l' && buf[1] == 'o' && buf[2] == 'n' &&
      buf[3] == 'g')
    return TK_LONG;
  if (len == 5 && buf[0] == 's' && buf[1] == 'h' && buf[2] == 'o' &&
      buf[3] == 'r' && buf[4] == 't')
    return TK_SHORT;
  if (len == 6 && buf[0] == 's' && buf[1] == 'i' && buf[2] == 'g' &&
      buf[3] == 'n' && buf[4] == 'e' && buf[5] == 'd')
    return TK_SIGNED;
  if (len == 5 && buf[0] == 'f' && buf[1] == 'l' && buf[2] == 'o' &&
      buf[3] == 'a' && buf[4] == 't')
    return TK_FLOAT;
  if (len == 6 && buf[0] == 'd' && buf[1] == 'o' && buf[2] == 'u' &&
      buf[3] == 'b' && buf[4] == 'l' && buf[5] == 'e')
    return TK_DOUBLE;
  if (len == 2 && buf[0] == 'i' && buf[1] == 'f')
    return TK_IF;
  if (len == 3 && buf[0] == 'a' && buf[1] == 's' && buf[2] == 'm')
    return TK_ASM;
  if (len == 7 && buf[0] == '_' && buf[1] == '_' && buf[2] == 'a' &&
      buf[3] == 's' && buf[4] == 'm' && buf[5] == '_' && buf[6] == '_')
    return TK_ASM;
  if (len == 4 && buf[0] == 'e' && buf[1] == 'l' && buf[2] == 's' &&
      buf[3] == 'e')
    return TK_ELSE;
  if (len == 5 && buf[0] == 'w' && buf[1] == 'h' && buf[2] == 'i' &&
      buf[3] == 'l' && buf[4] == 'e')
    return TK_WHILE;
  if (len == 3 && buf[0] == 'f' && buf[1] == 'o' && buf[2] == 'r')
    return TK_FOR;
  if (len == 2 && buf[0] == 'd' && buf[1] == 'o')
    return TK_DO;
  if (len == 6 && buf[0] == 'r' && buf[1] == 'e' && buf[2] == 't' &&
      buf[3] == 'u' && buf[4] == 'r' && buf[5] == 'n')
    return TK_RETURN;
  if (len == 5 && buf[0] == 'b' && buf[1] == 'r' && buf[2] == 'e' &&
      buf[3] == 'a' && buf[4] == 'k')
    return TK_BREAK;
  if (len == 4 && buf[0] == 'g' && buf[1] == 'o' && buf[2] == 't' &&
      buf[3] == 'o')
    return TK_GOTO;
  if (len == 6 && buf[0] == 's' && buf[1] == 'w' && buf[2] == 'i' &&
      buf[3] == 't' && buf[4] == 'c' && buf[5] == 'h')
    return TK_SWITCH;
  if (len == 4 && buf[0] == 'c' && buf[1] == 'a' && buf[2] == 's' &&
      buf[3] == 'e')
    return TK_CASE;
  if (len == 6 && buf[0] == 's' && buf[1] == 't' && buf[2] == 'r' &&
      buf[3] == 'u' && buf[4] == 'c' && buf[5] == 't')
    return TK_STRUCT;
  if (len == 5 && buf[0] == 'u' && buf[1] == 'n' && buf[2] == 'i' &&
      buf[3] == 'o' && buf[4] == 'n')
    return TK_UNION;
  if (len == 4 && buf[0] == 'e' && buf[1] == 'n' && buf[2] == 'u' &&
      buf[3] == 'm')
    return TK_ENUM;
  if (len == 6 && buf[0] == 's' && buf[1] == 'i' && buf[2] == 'z' &&
      buf[3] == 'e' && buf[4] == 'o' && buf[5] == 'f')
    return TK_SIZEOF;
  if (len == 6 && buf[0] == 's' && buf[1] == 't' && buf[2] == 'a' &&
      buf[3] == 't' && buf[4] == 'i' && buf[5] == 'c')
    return TK_STATIC;
  if (len == 6 && buf[0] == 'e' && buf[1] == 'x' && buf[2] == 't' &&
      buf[3] == 'e' && buf[4] == 'r' && buf[5] == 'n')
    return TK_EXTERN;
  if (len == 5 && buf[0] == 'c' && buf[1] == 'o' && buf[2] == 'n' &&
      buf[3] == 's' && buf[4] == 't')
    return TK_CONST;
  if (len == 4 && buf[0] == 'a' && buf[1] == 'u' && buf[2] == 't' &&
      buf[3] == 'o')
    return TK_AUTO;
  if (len == 6 && buf[0] == 'i' && buf[1] == 'n' && buf[2] == 'l' &&
      buf[3] == 'i' && buf[4] == 'n' && buf[5] == 'e')
    return TK_INLINE;
  if (len == 6 && buf[0] == 't' && buf[1] == 'y' && buf[2] == 'p' &&
      buf[3] == 'e' && buf[4] == 'o' && buf[5] == 'f')
    return TK_TYPEOF;
  if (len == 8 && buf[0] == '_' && buf[1] == '_' && buf[2] == 't' &&
      buf[3] == 'y' && buf[4] == 'p' && buf[5] == 'e' && buf[6] == 'o' &&
      buf[7] == 'f')
    return TK_TYPEOF;
  if (len == 10 && buf[0] == '_' && buf[1] == '_' && buf[2] == 't' &&
      buf[3] == 'y' && buf[4] == 'p' && buf[5] == 'e' && buf[6] == 'o' &&
      buf[7] == 'f' && buf[8] == '_' && buf[9] == '_')
    return TK_TYPEOF;
  /* 7–8 char keywords: need len check then safe indices 0..6 or 0..7 */
  if (len == 7 && buf[0] == 't' && buf[1] == 'y' && buf[2] == 'p' &&
      buf[3] == 'e' && buf[4] == 'd' && buf[5] == 'e' && buf[6] == 'f')
    return TK_TYPEDEF;
  if (len == 7 && buf[0] == 'd' && buf[1] == 'e' && buf[2] == 'f' &&
      buf[3] == 'a' && buf[4] == 'u' && buf[5] == 'l' && buf[6] == 't')
    return TK_DEFAULT;
  if (len == 8 && buf[0] == 'u' && buf[1] == 'n' && buf[2] == 's' &&
      buf[3] == 'i' && buf[4] == 'g' && buf[5] == 'n' && buf[6] == 'e' &&
      buf[7] == 'd')
    return TK_UNSIGNED;
  if (len == 8 && buf[0] == 'v' && buf[1] == 'o' && buf[2] == 'l' &&
      buf[3] == 'a' && buf[4] == 't' && buf[5] == 'i' && buf[6] == 'l' &&
      buf[7] == 'e')
    return TK_VOLATILE;
  if (len == 8 && buf[0] == 'c' && buf[1] == 'o' && buf[2] == 'n' &&
      buf[3] == 't' && buf[4] == 'i' && buf[5] == 'n' && buf[6] == 'u' &&
      buf[7] == 'e')
    return TK_CONTINUE;
  if (len == 8 && buf[0] == 'r' && buf[1] == 'e' && buf[2] == 'g' &&
      buf[3] == 'i' && buf[4] == 's' && buf[5] == 't' && buf[6] == 'e' &&
      buf[7] == 'r')
    return TK_REGISTER;
  if (len == 8 && buf[0] == 'r' && buf[1] == 'e' && buf[2] == 's' &&
      buf[3] == 't' && buf[4] == 'r' && buf[5] == 'i' && buf[6] == 'c' &&
      buf[7] == 't')
    return TK_VOLATILE;
  if (len == 8 && buf[0] == '_' && buf[1] == '_' && buf[2] == 'c' &&
      buf[3] == 'o' && buf[4] == 'n' && buf[5] == 's' && buf[6] == 't')
    return TK_CONST;
  if (len == 8 && buf[0] == '_' && buf[1] == '_' && buf[2] == 's' &&
      buf[3] == 'i' && buf[4] == 'g' && buf[5] == 'n' && buf[6] == 'e' &&
      buf[7] == 'd')
    return TK_SIGNED;
  if (len == 8 && buf[0] == '_' && buf[1] == '_' && buf[2] == 'i' &&
      buf[3] == 'n' && buf[4] == 'l' && buf[5] == 'i' && buf[6] == 'n' &&
      buf[7] == 'e')
    return TK_INLINE;
  if (len == 9 && buf[0] == '_' && buf[1] == '_' && buf[2] == 'c' &&
      buf[3] == 'o' && buf[4] == 'n' && buf[5] == 's' && buf[6] == 't' &&
      buf[7] == '_' && buf[8] == '_')
    return TK_CONST;
  if (len == 10 && buf[0] == '_' && buf[1] == '_' && buf[2] == 's' &&
      buf[3] == 'i' && buf[4] == 'g' && buf[5] == 'n' && buf[6] == 'e' &&
      buf[7] == 'd' && buf[8] == '_' && buf[9] == '_')
    return TK_SIGNED;
  if (len == 10 && buf[0] == '_' && buf[1] == '_' && buf[2] == 'i' &&
      buf[3] == 'n' && buf[4] == 'l' && buf[5] == 'i' && buf[6] == 'n' &&
      buf[7] == 'e' && buf[8] == '_' && buf[9] == '_')
    return TK_INLINE;
  if (len == 10 && buf[0] == '_' && buf[1] == '_' && buf[2] == 'r' &&
      buf[3] == 'e' && buf[4] == 's' && buf[5] == 't' && buf[6] == 'r' &&
      buf[7] == 'i' && buf[8] == 'c' && buf[9] == 't')
    return TK_VOLATILE;
  if (len == 12 && buf[0] == '_' && buf[1] == '_' && buf[2] == 'r' &&
      buf[3] == 'e' && buf[4] == 's' && buf[5] == 't' && buf[6] == 'r' &&
      buf[7] == 'i' && buf[8] == 'c' && buf[9] == 't' && buf[10] == '_' &&
      buf[11] == '_')
    return TK_VOLATILE;
  if (len == 10 && buf[0] == '_' && buf[1] == '_' && buf[2] == 'v' &&
      buf[3] == 'o' && buf[4] == 'l' && buf[5] == 'a' && buf[6] == 't' &&
      buf[7] == 'i' && buf[8] == 'l' && buf[9] == 'e')
    return TK_VOLATILE;
  if (len == 12 && buf[0] == '_' && buf[1] == '_' && buf[2] == 'v' &&
      buf[3] == 'o' && buf[4] == 'l' && buf[5] == 'a' && buf[6] == 't' &&
      buf[7] == 'i' && buf[8] == 'l' && buf[9] == 'e' && buf[10] == '_' &&
      buf[11] == '_')
    return TK_VOLATILE;
  if (len == 8 && buf[0] == '_' && buf[1] == '_' && buf[2] == 'i' &&
      buf[3] == 'n' && buf[4] == 't' && buf[5] == '1' && buf[6] == '2' &&
      buf[7] == '8')
    return TK_LONG;
  if (len == 10 && buf[0] == '_' && buf[1] == '_' && buf[2] == 'i' &&
      buf[3] == 'n' && buf[4] == 't' && buf[5] == '1' && buf[6] == '2' &&
      buf[7] == '8' && buf[8] == '_' && buf[9] == 't')
    return TK_LONG;
  return 0;
}

/* ================================================================ */
/* LEXER                                                             */
/* ================================================================ */

static int is_alpha(int c) {
  if (c >= 'a') {
    if (c <= 'z')
      return 1;
  }
  if (c >= 'A') {
    if (c <= 'Z')
      return 1;
  }
  if (c == '_')
    return 1;
  return 0;
}

static int is_digit(int c) {
  if (c >= '0') {
    if (c <= '9')
      return 1;
  }
  return 0;
}

static int is_alnum(int c) {
  if (is_alpha(c))
    return 1;
  if (is_digit(c))
    return 1;
  return 0;
}

static int is_space(int c) {
  if (c == ' ')
    return 1;
  if (c == '\t')
    return 1;
  if (c == '\r')
    return 1;
  if (c == '\f')
    return 1;
  if (c == '\v')
    return 1;
  return 0;
}

static int hex_val(int c) {
  if (c >= '0') {
    if (c <= '9')
      return c - '0';
  }
  if (c >= 'a') {
    if (c <= 'f')
      return c - 'a' + 10;
  }
  if (c >= 'A') {
    if (c <= 'F')
      return c - 'A' + 10;
  }
  return -1;
}

static int peek_char(Compiler *cc) {
  if (cc->pos < cc->source_len)
    return (unsigned char)cc->source[cc->pos];
  return -1;
}

static int read_char(Compiler *cc) {
  int c;
  if (cc->pos >= cc->source_len)
    return -1;
  c = (unsigned char)cc->source[cc->pos];
  cc->pos++;
  if (c == '\n') {
    cc->line++;
    cc->col = 1;
  } else {
    cc->col++;
  }
  return c;
}

static int read_escape(Compiler *cc) {
  int c;
  c = read_char(cc);
  switch (c) {
  case 'n':
    return '\n';
  case 't':
    return '\t';
  case 'r':
    return '\r';
  case '\\':
    return '\\';
  case '\'':
    return '\'';
  case '"':
    return '"';
  case '0':
  case '1':
  case '2':
  case '3':
  case '4':
  case '5':
  case '6':
  case '7': {
    int val = c - '0';
    if (peek_char(cc) >= '0' && peek_char(cc) <= '7') {
      val = val * 8 + (read_char(cc) - '0');
      if (peek_char(cc) >= '0' && peek_char(cc) <= '7') {
        val = val * 8 + (read_char(cc) - '0');
      }
    }
    return val;
  }
  case 'a':
    return '\a';
  case 'b':
    return '\b';
  case 'f':
    return '\f';
  case 'v':
    return '\v';
  case 'x': {
    int val;
    int h;
    val = 0;
    h = hex_val(peek_char(cc));
    while (h >= 0) {
      read_char(cc);
      val = val * 16 + h;
      h = hex_val(peek_char(cc));
    }
    return val;
  }
  default:
    return c;
  }
}

void next_token(Compiler *cc) {
  int c;
  int kw;
  int entry_pos = cc->pos;

  /* if we have a peeked token, use it */
  if (cc->has_peek) {
    cc->tk = cc->peek_tk;
    cc->tk_val = cc->peek_val;
    strncpy(cc->tk_text, cc->peek_text, MAX_IDENT - 1);
    strncpy(cc->tk_str, cc->peek_str, MAX_STR - 1);
    cc->tk_str_len = cc->peek_str_len;
    cc->tk_line = cc->peek_line;
    cc->tk_col = cc->peek_col;
    cc->has_peek = 0;
    return;
  }

again:
  /* skip whitespace */
  while (cc->pos < cc->source_len) {
    c = peek_char(cc);
    if (c == '\n') {
      read_char(cc);
      goto again;
    }
    if (is_space(c)) {
      read_char(cc);
    } else {
      break;
    }
  }

  cc->tk_line = cc->line;
  cc->tk_col = cc->col;

  if (cc->pos >= cc->source_len) {
    cc->tk = TK_EOF;
    cc->tk_text[0] = 0;
    return;
  }

  c = peek_char(cc);

  /* skip line comments */
  if (c == '/') {
    if (cc->pos + 1 < cc->source_len) {
      if (cc->source[cc->pos + 1] == '/') {
        while (cc->pos < cc->source_len) {
          if (cc->source[cc->pos] == '\n')
            break;
          cc->pos++;
        }
        goto again;
      }
      /* block comments */
      if (cc->source[cc->pos + 1] == '*') {
        cc->pos += 2;
        cc->col += 2;
        while (cc->pos + 1 < cc->source_len) {
          if (cc->source[cc->pos] == '\n') {
            cc->line++;
            cc->col = 1;
          }
          if (cc->source[cc->pos] == '*') {
            if (cc->source[cc->pos + 1] == '/') {
              cc->pos += 2;
              cc->col += 2;
              goto again;
            }
          }
          cc->pos++;
          cc->col++;
        }
        goto again;
      }
    }
  }

  /* preprocessor line marker or generic directive — handle both.
   *
   * Standard linemarker format emitted by pp_process_include:
   *   # <decimal-line-number> "<filename>"
   *
   * When matched: update cc->line and cc->filename so that subsequent
   * tokens report source-level coordinates. lm_line - 1 compensates
   * for the '\n' at EOL that read_char/next_token will increment to
   * lm_line when the first token on the resumed line is scanned.
   *
   * Filename buffer: static, single-process lifetime. Known limitation:
   * nested includes (header including header) overwrite this buffer and
   * all previous cc->filename pointers become stale. See PP-LINEMARKER-001.
   * Fix B (arena_strdup) is the production solution; deferred.
   */
  if (c == '#') {
    int lm_line = 0;
    int lm_pos  = cc->pos + 1; /* position after '#' */
    /* skip spaces between '#' and the digit */
    while (lm_pos < cc->source_len && cc->source[lm_pos] == ' ')
      lm_pos++;
    /* linemarker if next char is a digit */
    if (lm_pos < cc->source_len
        && cc->source[lm_pos] >= '0' && cc->source[lm_pos] <= '9') {
      /* parse the line number */
      while (lm_pos < cc->source_len
             && cc->source[lm_pos] >= '0' && cc->source[lm_pos] <= '9') {
        lm_line = lm_line * 10 + (cc->source[lm_pos] - '0');
        lm_pos++;
      }
      /* skip space between number and opening quote */
      while (lm_pos < cc->source_len && cc->source[lm_pos] == ' ')
        lm_pos++;
      /* opening quote — parse filename */
      if (lm_pos < cc->source_len && cc->source[lm_pos] == '"') {
        static char lm_filename[512];
        int fn_len = 0;
        lm_pos++; /* skip '"' */
        while (lm_pos < cc->source_len
               && cc->source[lm_pos] != '"'
               && cc->source[lm_pos] != '\n'
               && fn_len < 511) {
          lm_filename[fn_len++] = cc->source[lm_pos++];
        }
        lm_filename[fn_len] = 0;
        /* apply: lm_line - 1 because the '\n' at EOL of this marker line
         * will be incremented by the whitespace-skip loop at top of next
         * next_token call, bringing cc->line to lm_line for the first real
         * token on the resumed line. */
        cc->line     = lm_line - 1;
        cc->filename = lm_filename;
      }
    }
    /* consume rest of line (flags after filename, or unrecognized directives) */
    while (cc->pos < cc->source_len && cc->source[cc->pos] != '\n')
      cc->pos++;
    goto again;
  }


  /* number literals */
  if (is_digit(c)) {
    long val;
    int start;
    int is_float = 0;
    int i_look = 0;
    char *end;
    double fval;
    int len;

    if (c == '0' && cc->pos + 1 < cc->source_len &&
        (cc->source[cc->pos + 1] == 'x' || cc->source[cc->pos + 1] == 'X')) {
      is_float = 0;
    } else {
      while (cc->pos + i_look < cc->source_len) {
        char ch = cc->source[cc->pos + i_look];
        if (ch == '.' || ch == 'e' || ch == 'E') {
          is_float = 1;
          break;
        }
        if (!is_digit(ch) && !is_alpha(ch))
          break;
        i_look++;
      }
    }

    if (is_float) {
      fval = strtod(cc->source + cc->pos, &end);
      len = end - (cc->source + cc->pos);
      if (len <= 0)
        len = 1;
      /* we peeked c, so real current index is cc->pos,
         but we need to advance cc->pos. */
      cc->pos = cc->pos + len;
      cc->col = cc->col + len;
      if (cc->pos < cc->source_len) {
        char sc = cc->source[cc->pos];
        if (sc == 'f' || sc == 'F' || sc == 'l' || sc == 'L') {
          cc->pos++;
          cc->col++;
        }
      }
      cc->tk = TK_FLIT;
      cc->tk_fval = fval;
      cc->tk_text[0] = 0;
      return;
    }

    val = 0;
    start = cc->pos;
    if (c == '0') {
      read_char(cc);
      if (peek_char(cc) == 'x' || peek_char(cc) == 'X') {
        read_char(cc);
        while (hex_val(peek_char(cc)) >= 0) {
          val = val * 16 + hex_val(peek_char(cc));
          read_char(cc);
        }
      } else if (peek_char(cc) >= '0') {
        if (peek_char(cc) <= '7') {
          /* octal */
          while (peek_char(cc) >= '0') {
            if (peek_char(cc) <= '7') {
              val = val * 8 + (peek_char(cc) - '0');
              read_char(cc);
            } else {
              break;
            }
          }
        }
      }
    } else {
      while (is_digit(peek_char(cc))) {
        val = val * 10 + (peek_char(cc) - '0');
        read_char(cc);
      }
    }
    /* parse suffixes like L, U, UL, ULL, etc */
    int is_uns = 0;
    int is_lng = 0;
    while (peek_char(cc) == 'L' || peek_char(cc) == 'l' ||
           peek_char(cc) == 'U' || peek_char(cc) == 'u') {
      int ch = read_char(cc);
      if (ch == 'U' || ch == 'u')
        is_uns = 1;
      if (ch == 'L' || ch == 'l')
        is_lng = 1;
    }
    cc->tk = TK_NUM;
    cc->tk_val = val;
    cc->tk_text[0] = is_uns ? 'U' : 0;
    cc->tk_text[1] = is_lng ? 'L' : 0;
    return;
  }

  /* character literal */
  if (c == '\'') {
    int val;
    read_char(cc); /* skip opening ' */
    if (peek_char(cc) == '\\') {
      read_char(cc);
      val = read_escape(cc);
    } else {
      val = read_char(cc);
    }
    if (peek_char(cc) == '\'')
      read_char(cc); /* skip closing ' */
    cc->tk = TK_CHAR_LIT;
    cc->tk_val = val;
    return;
  }

  /* string literal */
  if (c == '"') {
    int len;
    read_char(cc); /* skip opening " */
    len = 0;
    while (peek_char(cc) != '"') {
      if (peek_char(cc) == -1)
        break;
      if (peek_char(cc) == '\\') {
        read_char(cc);
        cc->tk_str[len] = read_escape(cc);
      } else {
        cc->tk_str[len] = read_char(cc);
      }
      len++;
      if (len >= MAX_STR - 1)
        break;
    }
    if (peek_char(cc) == '"')
      read_char(cc); /* skip closing " */
    cc->tk_str[len] = 0;
    cc->tk_str_len = len;
    cc->tk = TK_STR;

    /* handle adjacent string literals */
    /* skip whitespace and check for another " */
    while (1) {
      int saved_pos;
      int saved_line;
      int saved_col;
      saved_pos = cc->pos;
      saved_line = cc->line;
      saved_col = cc->col;
      while (cc->pos < cc->source_len) {
        int ch;
        ch = cc->source[cc->pos];
        if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') {
          if (ch == '\n') {
            cc->line++;
            cc->col = 1;
          } else {
            cc->col++;
          }
          cc->pos++;
        } else {
          break;
        }
      }
      if (cc->pos < cc->source_len) {
        if (cc->source[cc->pos] == '"') {
          /* concatenate adjacent string */
          read_char(cc); /* skip " */
          while (peek_char(cc) != '"') {
            if (peek_char(cc) == -1)
              break;
            if (peek_char(cc) == '\\') {
              read_char(cc);
              cc->tk_str[len] = read_escape(cc);
            } else {
              cc->tk_str[len] = read_char(cc);
            }
            len++;
            if (len >= MAX_STR - 1)
              break;
          }
          if (peek_char(cc) == '"')
            read_char(cc);
          cc->tk_str[len] = 0;
          cc->tk_str_len = len;
        } else {
          /* no adjacent string — restore position and exit loop */
          cc->pos = saved_pos;
          cc->line = saved_line;
          cc->col = saved_col;
          break;
        }
      } else {
        break;
      }
    }
    return;
  }

  /* identifiers and keywords: use local buffer for lookup to avoid bad
   * cc->tk_text in stage2 (offset/codegen quirk) */
  if (is_alpha(c)) {
    static char ident_buf[MAX_IDENT];
    int len;
    len = 0;
    while (is_alnum(peek_char(cc))) {
      ident_buf[len] = read_char(cc);
      len++;
      if (len >= MAX_IDENT - 1)
        break;
    }
    ident_buf[len] = 0;

    kw = lookup_keyword(ident_buf);
    if (!kw)
      kw = lookup_keyword_fallback(ident_buf, len);
    if (kw) {
      cc->tk = kw;
    } else {
      if (strcmp(ident_buf, "__attribute__") == 0 ||
          strcmp(ident_buf, "__attribute") == 0) {
        int pcount = 0;
        int started = 0;
        while (cc->pos < cc->source_len) {
          char ac = cc->source[cc->pos];
          if (ac == '(') {
            pcount++;
            started = 1;
          } else if (ac == ')') {
            pcount--;
            if (started && pcount == 0) {
              cc->pos++;
              cc->col++;
              break;
            }
          }
          cc->pos++;
          cc->col++;
        }
        goto again;
      }
      if (strcmp(ident_buf, "__extension__") == 0) {
        goto again;
      }

      cc->tk = TK_IDENT;
    }
    strncpy(cc->tk_text, ident_buf, MAX_IDENT - 1);
    cc->tk_text[MAX_IDENT - 1] = 0;
    return;
  }

  /* operators and delimiters */
  read_char(cc);

  if (c == '+') {
    if (peek_char(cc) == '+') {
      read_char(cc);
      cc->tk = TK_INC;
      return;
    }
    if (peek_char(cc) == '=') {
      read_char(cc);
      cc->tk = TK_PLUS_ASSIGN;
      return;
    }
    cc->tk = TK_PLUS;
    return;
  }
  if (c == '-') {
    if (peek_char(cc) == '-') {
      read_char(cc);
      cc->tk = TK_DEC;
      return;
    }
    if (peek_char(cc) == '>') {
      read_char(cc);
      cc->tk = TK_ARROW;
      return;
    }
    if (peek_char(cc) == '=') {
      read_char(cc);
      cc->tk = TK_MINUS_ASSIGN;
      return;
    }
    cc->tk = TK_MINUS;
    return;
  }
  if (c == '*') {
    if (peek_char(cc) == '=') {
      read_char(cc);
      cc->tk = TK_STAR_ASSIGN;
      return;
    }
    cc->tk = TK_STAR;
    return;
  }
  if (c == '/') {
    if (peek_char(cc) == '=') {
      read_char(cc);
      cc->tk = TK_SLASH_ASSIGN;
      return;
    }
    cc->tk = TK_SLASH;
    return;
  }
  if (c == '%') {
    if (peek_char(cc) == '=') {
      read_char(cc);
      cc->tk = TK_PERCENT_ASSIGN;
      return;
    }
    cc->tk = TK_PERCENT;
    return;
  }
  if (c == '&') {
    if (peek_char(cc) == '&') {
      read_char(cc);
      cc->tk = TK_LAND;
      return;
    }
    if (peek_char(cc) == '=') {
      read_char(cc);
      cc->tk = TK_AMP_ASSIGN;
      return;
    }
    cc->tk = TK_AMP;
    return;
  }
  if (c == '|') {
    if (peek_char(cc) == '|') {
      read_char(cc);
      cc->tk = TK_LOR;
      return;
    }
    if (peek_char(cc) == '=') {
      read_char(cc);
      cc->tk = TK_PIPE_ASSIGN;
      return;
    }
    cc->tk = TK_PIPE;
    return;
  }
  if (c == '^') {
    if (peek_char(cc) == '=') {
      read_char(cc);
      cc->tk = TK_CARET_ASSIGN;
      return;
    }
    cc->tk = TK_CARET;
    return;
  }
  if (c == '~') {
    cc->tk = TK_TILDE;
    return;
  }
  if (c == '!') {
    if (peek_char(cc) == '=') {
      read_char(cc);
      cc->tk = TK_NE;
      return;
    }
    cc->tk = TK_BANG;
    return;
  }
  if (c == '=') {
    if (peek_char(cc) == '=') {
      read_char(cc);
      cc->tk = TK_EQ;
      return;
    }
    cc->tk = TK_ASSIGN;
    return;
  }
  if (c == '<') {
    if (peek_char(cc) == '<') {
      read_char(cc);
      if (peek_char(cc) == '=') {
        read_char(cc);
        cc->tk = TK_SHL_ASSIGN;
        return;
      }
      cc->tk = TK_SHL;
      return;
    }
    if (peek_char(cc) == '=') {
      read_char(cc);
      cc->tk = TK_LE;
      return;
    }
    cc->tk = TK_LT;
    return;
  }
  if (c == '>') {
    if (peek_char(cc) == '>') {
      read_char(cc);
      if (peek_char(cc) == '=') {
        read_char(cc);
        cc->tk = TK_SHR_ASSIGN;
        return;
      }
      cc->tk = TK_SHR;
      return;
    }
    if (peek_char(cc) == '=') {
      read_char(cc);
      cc->tk = TK_GE;
      return;
    }
    cc->tk = TK_GT;
    return;
  }
  if (c == '(') {
    cc->tk = TK_LPAREN;
    return;
  }
  if (c == ')') {
    cc->tk = TK_RPAREN;
    return;
  }
  if (c == '{') {
    cc->tk = TK_LBRACE;
    return;
  }
  if (c == '}') {
    cc->tk = TK_RBRACE;
    return;
  }
  if (c == '[') {
    cc->tk = TK_LBRACKET;
    return;
  }
  if (c == ']') {
    cc->tk = TK_RBRACKET;
    return;
  }
  if (c == ';') {
    cc->tk = TK_SEMI;
    return;
  }
  if (c == ',') {
    cc->tk = TK_COMMA;
    return;
  }
  if (c == '.') {
    if (is_digit(peek_char(cc))) {
      char *end;
      double fval = strtod(cc->source + cc->pos - 1, &end);
      int len = end - (cc->source + cc->pos - 1);
      if (len <= 0)
        len = 1;
      cc->pos = cc->pos - 1 + len;
      cc->col = cc->col - 1 + len;
      if (cc->pos < cc->source_len) {
        char sc = cc->source[cc->pos];
        if (sc == 'f' || sc == 'F' || sc == 'l' || sc == 'L') {
          cc->pos++;
          cc->col++;
        }
      }
      cc->tk = TK_FLIT;
      cc->tk_fval = fval;
      cc->tk_text[0] = 0;
      return;
    }
    if (peek_char(cc) == '.') {
      read_char(cc);
      if (peek_char(cc) == '.') {
        read_char(cc);
        cc->tk = TK_ELLIPSIS;
        return;
      }
    }
    cc->tk = TK_DOT;
    return;
  }
  if (c == '?') {
    cc->tk = TK_QUESTION;
    return;
  }
  if (c == ':') {
    cc->tk = TK_COLON;
    return;
  }

  /* unknown character — skip */
  goto again;
}

/* Token enum in part1 runs 0..TK_HASH. Stage2 may compile enums to different
 * values (75-81 seen); use 128 so we only flag obvious garbage. */
#define MAX_KNOWN_TOKEN 128

/* Human-readable token name for error messages (index = enum value from part1).
 */
static const char *token_name(int t) {
  static const char *names[] = {
      "EOF",          "NUM",         "STR",          "CHAR_LIT",
      "IDENT",        "FLIT",        "INT",          "CHAR",
      "VOID",         "LONG",        "SHORT",        "UNSIGNED",
      "SIGNED",       "FLOAT",       "DOUBLE",       "IF",
      "ELSE",         "WHILE",       "FOR",          "DO",
      "RETURN",       "BREAK",       "CONTINUE",     "GOTO",
      "SWITCH",       "CASE",        "DEFAULT",      "STRUCT",
      "UNION",        "ENUM",        "TYPEDEF",      "SIZEOF",
      "STATIC",       "EXTERN",      "CONST",        "VOLATILE",
      "AUTO",         "REGISTER",    "INLINE",       "BUILTIN_VA_ARG",
      "PLUS",         "MINUS",       "STAR",         "SLASH",
      "PERCENT",      "AMP",         "PIPE",         "CARET",
      "TILDE",        "BANG",        "ASSIGN",       "EQ",
      "NE",           "LT",          "GT",           "LE",
      "GE",           "LAND",        "LOR",          "SHL",
      "SHR",          "INC",         "DEC",          "ARROW",
      "DOT",          "QUESTION",    "COLON",        "PLUS_ASSIGN",
      "MINUS_ASSIGN", "STAR_ASSIGN", "SLASH_ASSIGN", "PERCENT_ASSIGN",
      "AMP_ASSIGN",   "PIPE_ASSIGN", "CARET_ASSIGN", "SHL_ASSIGN",
      "SHR_ASSIGN",   "LPAREN",      "RPAREN",       "LBRACE",
      "RBRACE",       "LBRACKET",    "RBRACKET",     "SEMI",
      "COMMA",        "ELLIPSIS",    "HASH"};
  if (t >= 0 && t < 85)
    return names[t]; /* 85 = number of token names; avoid sizeof for stage2
                        parse */
  return "?";
}

void expect(Compiler *cc, int tk) {
  char buf[256];
  if (tk < 0 || tk > 128) {
    sprintf(
        buf,
        "DEMON: insane expected token %d (line %d) — possible stack corruption",
        tk, cc->tk_line);
    error(cc, buf);
  }
  if (cc->tk != tk) {
    sprintf(buf, "expected %s (%d), got %s (%d)", token_name(tk), tk,
            token_name(cc->tk), cc->tk);
    error(cc, buf);
  }
  next_token(cc);
}

int peek_token(Compiler *cc) {
  int s_tk;
  long s_val;
  char s_text[MAX_IDENT];
  char s_str[MAX_STR];
  int s_slen;
  int s_line;
  int s_col;

  if (cc->has_peek)
    return cc->peek_tk;

  /* save current token */
  s_tk = cc->tk;
  s_val = cc->tk_val;
  strncpy(s_text, cc->tk_text, MAX_IDENT - 1);
  strncpy(s_str, cc->tk_str, MAX_STR - 1);
  s_slen = cc->tk_str_len;
  s_line = cc->tk_line;
  s_col = cc->tk_col;

  /* lex next */
  next_token(cc);

  /* store peeked */
  cc->peek_tk = cc->tk;
  cc->peek_val = cc->tk_val;
  strncpy(cc->peek_text, cc->tk_text, MAX_IDENT - 1);
  strncpy(cc->peek_str, cc->tk_str, MAX_STR - 1);
  cc->peek_str_len = cc->tk_str_len;
  cc->peek_line = cc->tk_line;
  cc->peek_col = cc->tk_col;
  cc->has_peek = 1;

  /* restore current */
  cc->tk = s_tk;
  cc->tk_val = s_val;
  strncpy(cc->tk_text, s_text, MAX_IDENT - 1);
  strncpy(cc->tk_str, s_str, MAX_STR - 1);
  cc->tk_str_len = s_slen;
  cc->tk_line = s_line;
  cc->tk_col = s_col;

  return cc->peek_tk;
}

/* ZKAEDI FORCE RENDER CACHE INVALIDATION */
