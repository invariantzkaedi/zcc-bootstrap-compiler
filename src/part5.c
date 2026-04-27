
/* ================================================================ */
/* COMPILER INITIALIZATION                                           */
/* ================================================================ */

#include "ir_emit_dispatch.h"

#ifndef ZCC_AST_BRIDGE_H
/* 
 * This block is exclusively for IDE syntax analysis when viewing part5.c alone.
 * Since part1.c #includes zcc_ast_bridge.h, this #ifndef prevents infinite loops
 * and double-definitions when part1...5 are concatenated into zcc.c.
 */
#include "part1.c"
#endif

/* Global telemetry control */
static int enable_telemetry_stdout = 0;
extern void ir_telemetry_enable_stdout(void);

/* Manifold engine globals (defined in ir_pass_manager.c) */
extern int  g_manifold_enabled;
extern char g_ir_export_path[256];

/* Peephole globals (defined in ir_peephole.c) */
extern int  g_peephole_enabled;
extern int  g_peephole_deterministic;
extern int  g_peephole_verbose;

/* Security pass globals */
static int  g_security_signext = 0;
static int  g_security_476 = 0;
static int  g_security_787 = 0;

static void init_compiler(Compiler *cc) {
  /* zero everyt5555hing — cc was calloc'd */

  /* init arena */
  cc->arena.data = (char *)malloc(ARENA_SIZE);
  cc->arena.pos = 0;
  cc->arena.cap = ARENA_SIZE;
  cc->arena.next = 0;

  /* create type singletons */
  cc->ty_void = type_new(cc, TY_VOID);
  cc->ty_char = type_new(cc, TY_CHAR);
  cc->ty_uchar = type_new(cc, TY_UCHAR);
  cc->ty_short = type_new(cc, TY_SHORT);
  cc->ty_ushort = type_new(cc, TY_USHORT);
  cc->ty_int = type_new(cc, TY_INT);
  cc->ty_uint = type_new(cc, TY_UINT);
  cc->ty_long = type_new(cc, TY_LONG);
  cc->ty_ulong = type_new(cc, TY_ULONG);
  cc->ty_longlong = type_new(cc, TY_LONGLONG);
  cc->ty_ulonglong = type_new(cc, TY_ULONGLONG);
  cc->ty_float = type_new(cc, TY_FLOAT);
  cc->ty_double = type_new(cc, TY_DOUBLE);

  /* init lexer */
  cc->line = 1;
  cc->col = 1;
  cc->pos = 0;
  cc->has_peek = 0;
  cc->label_count = 100; /* start at 100 to avoid clashes */
  cc->errors = 0;
  cc->num_strings = 0;
  cc->num_structs = 0;
  cc->num_globals = 0;

  /* push global scope */
  scope_push(cc);

  /* register common typedefs and builtins */
  {
    Symbol *sym;
    /* typedef void *FILE — so FILE* works */
    sym = scope_add(cc, "FILE", type_ptr(cc, cc->ty_void));
    sym->is_typedef = 1;

    /* typedef long size_t */
    sym = scope_add(cc, "size_t", cc->ty_ulonglong);
    sym->is_typedef = 1;

    /* typedef long ssize_t */
    sym = scope_add(cc, "ssize_t", cc->ty_long);
    sym->is_typedef = 1;

    /* typedef int ptrdiff_t */
    sym = scope_add(cc, "ptrdiff_t", cc->ty_long);
    sym->is_typedef = 1;

    /* SysV ABI requires va_list to be an array of 1 struct of size 24 */
    {
        Type *t_va = type_new(cc, TY_STRUCT);
        t_va->size = 24;
        t_va->align = 8;
        t_va->is_complete = 1;
        t_va->is_tbfp = 0;
        sym = scope_add(cc, "__builtin_va_list", type_array(cc, t_va, 1));
    }
    sym->is_typedef = 1;

    /* _Float128 workaround */
    sym = scope_add(cc, "_Float128", cc->ty_double);
    sym->is_typedef = 1;


    /* NULL as enum constant */
    sym = scope_add(cc, "NULL", cc->ty_long);
    sym->is_enum_const = 1;
    sym->enum_val = 0;

    /* stdout, stderr */
    sym = scope_add(cc, "stdout", type_ptr(cc, cc->ty_void));
    sym->is_global = 1;
    sym = scope_add(cc, "stderr", type_ptr(cc, cc->ty_void));
    sym->is_global = 1;

    /* common libc functions */
    {
      Type *ft;

      /* printf — returns int, variadic */
      ft = type_func(cc, cc->ty_int);
      ft->is_variadic = 1;
      sym = scope_add(cc, "printf", ft);
      sym->is_global = 1;

      /* fprintf */
      ft = type_func(cc, cc->ty_int);
      ft->is_variadic = 1;
      sym = scope_add(cc, "fprintf", ft);
      sym->is_global = 1;

      /* sscanf */
      ft = type_func(cc, cc->ty_int);
      ft->is_variadic = 1;
      sym = scope_add(cc, "sscanf", ft);
      sym->is_global = 1;

      /* sprintf */
      ft = type_func(cc, cc->ty_int);
      ft->is_variadic = 1;
      sym = scope_add(cc, "sprintf", ft);
      sym->is_global = 1;

      /* malloc — returns void* */
      ft = type_func(cc, type_ptr(cc, cc->ty_void));
      sym = scope_add(cc, "malloc", ft);
      sym->is_global = 1;

      /* calloc */
      ft = type_func(cc, type_ptr(cc, cc->ty_void));
      sym = scope_add(cc, "calloc", ft);
      sym->is_global = 1;

      /* realloc */
      ft = type_func(cc, type_ptr(cc, cc->ty_void));
      sym = scope_add(cc, "realloc", ft);
      sym->is_global = 1;

      /* free — returns void */
      ft = type_func(cc, cc->ty_void);
      sym = scope_add(cc, "free", ft);
      sym->is_global = 1;

      /* exit */
      ft = type_func(cc, cc->ty_void);
      sym = scope_add(cc, "exit", ft);
      sym->is_global = 1;

      /* fopen — returns FILE* */
      ft = type_func(cc, type_ptr(cc, cc->ty_void));
      sym = scope_add(cc, "fopen", ft);
      sym->is_global = 1;

      /* fclose */
      ft = type_func(cc, cc->ty_int);
      sym = scope_add(cc, "fclose", ft);
      sym->is_global = 1;

      /* fread */
      ft = type_func(cc, cc->ty_ulong);
      sym = scope_add(cc, "fread", ft);
      sym->is_global = 1;

      /* fwrite */
      ft = type_func(cc, cc->ty_ulong);
      sym = scope_add(cc, "fwrite", ft);
      sym->is_global = 1;

      /* fseek */
      ft = type_func(cc, cc->ty_int);
      sym = scope_add(cc, "fseek", ft);
      sym->is_global = 1;

      /* ftell */
      ft = type_func(cc, cc->ty_long);
      sym = scope_add(cc, "ftell", ft);
      sym->is_global = 1;

      /* fgets */
      ft = type_func(cc, type_ptr(cc, cc->ty_char));
      sym = scope_add(cc, "fgets", ft);
      sym->is_global = 1;

      /* fputs */
      ft = type_func(cc, cc->ty_int);
      sym = scope_add(cc, "fputs", ft);
      sym->is_global = 1;

      /* strcmp, strncpy, strlen, strncmp */
      ft = type_func(cc, cc->ty_int);
      sym = scope_add(cc, "strcmp", ft);
      sym->is_global = 1;

      ft = type_func(cc, type_ptr(cc, cc->ty_char));
      sym = scope_add(cc, "strncpy", ft);
      sym->is_global = 1;

      ft = type_func(cc, cc->ty_ulong);
      sym = scope_add(cc, "strlen", ft);
      sym->is_global = 1;

      /* strstr */
      ft = type_func(cc, type_ptr(cc, cc->ty_char));
      sym = scope_add(cc, "strstr", ft);
      sym->is_global = 1;

      /* strtod */
      ft = type_func(cc, cc->ty_double);
      sym = scope_add(cc, "strtod", ft);
      sym->is_global = 1;

      ft = type_func(cc, cc->ty_int);
      sym = scope_add(cc, "strncmp", ft);
      sym->is_global = 1;

      ft = type_func(cc, type_ptr(cc, cc->ty_char));
      sym = scope_add(cc, "strcpy", ft);
      sym->is_global = 1;

      /* memset, memcpy */
      ft = type_func(cc, type_ptr(cc, cc->ty_void));
      sym = scope_add(cc, "memset", ft);
      sym->is_global = 1;

      ft = type_func(cc, type_ptr(cc, cc->ty_void));
      sym = scope_add(cc, "memcpy", ft);
      sym->is_global = 1;

      /* write, read (syscall wrappers) */
      ft = type_func(cc, cc->ty_long);
      sym = scope_add(cc, "write", ft);
      sym->is_global = 1;

      ft = type_func(cc, cc->ty_long);
      sym = scope_add(cc, "read", ft);
      sym->is_global = 1;

      /* system */
      ft = type_func(cc, cc->ty_int);
      sym = scope_add(cc, "system", ft);
      sym->is_global = 1;

      /* _exit */
      ft = type_func(cc, cc->ty_void);
      sym = scope_add(cc, "_exit", ft);
      sym->is_global = 1;
    }
  }
}

/* ================================================================ */
/* FILE READING                                                      */
/* ================================================================ */

static char *read_file(char *path, int *out_len) {
  FILE *fp;
  long len;
  char *buf;
  int nr;

  fprintf(stderr, "read_file: path = '%s' (addr: %p)\n", path, path);
  fp = fopen(path, "rb");
  if (!fp)
    return 0;

  fseek(fp, 0, 2); /* SEEK_END = 2 */
  len = ftell(fp);
  fseek(fp, 0, 0); /* SEEK_SET = 0 */

  buf = (char *)malloc(len + 1);
  if (!buf) {
    fclose(fp);
    return 0;
  }

  nr = fread(buf, 1, len, fp);
  buf[nr] = 0;
  fclose(fp);

  *out_len = nr;
  return buf;
}

/* ================================================================ */
/* PEEPHOLE OPTIMIZER                                                */
/* ================================================================ */

#define MAX_PEEP_LINES 3000000
#define MAX_PEEP_LEN 128

static char *line_buffer = 0;
static char **line_ptrs = 0;

static void peephole_optimize(char *filename) {
  FILE *fp;
  int nlines = 0;
  int i;
  int eliminated = 0;
  long file_size;

  fp = fopen(filename, "r");
  if (!fp)
    return;

  fseek(fp, 0, 2);
  file_size = ftell(fp);
  fseek(fp, 0, 0);

  if (!line_ptrs) {
    line_ptrs = (char **)malloc(MAX_PEEP_LINES * sizeof(char *));
  }
  line_buffer = (char *)malloc(file_size + MAX_PEEP_LINES * 128);
  if (!line_buffer || !line_ptrs) {
    fclose(fp);
    return;
  }

  while (nlines < MAX_PEEP_LINES && fgets(line_buffer + nlines * 128, 128, fp)) {
    line_ptrs[nlines] = line_buffer + nlines * 128;
    nlines++;
  }
  fclose(fp);

  for (i = 0; i < nlines;) {
    char *l1 = line_ptrs[i];

    /* 1. Redundant Push/Pop */
    if (strncmp(l1, "    pushq ", 10) == 0 && i + 1 < nlines) {
      char *l2 = line_ptrs[i + 1];
      if (strncmp(l2, "    popq ", 9) == 0) {
        char tmp1[64], tmp2[64];
        sscanf(l1, "    pushq %s", tmp1);
        sscanf(l2, "    popq %s", tmp2);
        if (strcmp(tmp1, tmp2) == 0) {
          line_ptrs[i][0] = 0;
          line_ptrs[i + 1][0] = 0;
          eliminated += 2;
          i += 2;
          continue;
        } else {
          sprintf(line_ptrs[i], "    movq %s, %s\n", tmp1, tmp2);
          line_ptrs[i + 1][0] = 0;
          eliminated += 2;
          i += 2;
          continue;
        }
      }
    }

    /* 2. Arithmetic Nullification */
    if (strcmp(l1, "    addq $0, %rax\n") == 0 ||
        strcmp(l1, "    subq $0, %rax\n") == 0 ||
        strcmp(l1, "    addq $0, %rsp\n") == 0 ||
        strcmp(l1, "    subq $0, %rsp\n") == 0) {
      line_ptrs[i][0] = 0;
      eliminated += 1;
      i += 1;
      continue;
    }

    /* 3. Push/Lea/Pop Triad */
    if (strcmp(l1, "    pushq %rax\n") == 0 && i + 2 < nlines) {
      char *l2 = line_ptrs[i + 1];
      char *l3 = line_ptrs[i + 2];
      if (strncmp(l2, "    leaq ", 9) == 0 && strstr(l2, ", %rax") &&
          strncmp(l3, "    popq ", 9) == 0) {
        char pop_reg[64];
        sscanf(l3, "    popq %s", pop_reg);
        sprintf(line_ptrs[i], "    movq %%rax, %s\n", pop_reg);
        line_ptrs[i + 2][0] = 0;
        eliminated += 3;
        i += 3;
        continue;
      }
    }
    i++;
  }

  fp = fopen(filename, "w");
  if (!fp) {
    free(line_buffer);
    return;
  }
  for (i = 0; i < nlines; i++) {
    if (line_ptrs[i][0] != 0)
      fputs(line_ptrs[i], fp);
  }
  fclose(fp);
  free(line_buffer);
  if (!enable_telemetry_stdout) printf("[Phase 5] Native C Peephole Optimization... OK (%d elided)\n",
         eliminated);
}

/* ================================================================ */
/* SECURITY PASS: --security-signext (CWE-122)                       */
/* Scans assembly for sign-extension (movsbl/movsbq) feeding         */
/* memcpy/memset/memmove within a 15-instruction window.             */
/* ================================================================ */

/* Scan window: ZCC unoptimized codegen emits ~30-40 instructions of arg
   setup between a sign-extending cast and the consuming memcpy call.
   64 covers observed gaps (CVE-2023-38545: 34 lines) with margin. */
#define SIGNEXT_WINDOW 64

static void security_signext_scan(char *filename) {
  FILE *fp;
  int nlines = 0;
  int findings = 0;
  int i;
  char current_func[128];
  char *line_buf;
  char **lp;
  int max_lines = 32768;

  fp = fopen(filename, "r");
  if (!fp) return;

  line_buf = (char *)malloc(max_lines * 128);
  lp = (char **)malloc(max_lines * sizeof(char *));
  if (!line_buf || !lp) { fclose(fp); return; }

  current_func[0] = 0;
  while (nlines < max_lines && fgets(line_buf + nlines * 128, 128, fp)) {
    lp[nlines] = line_buf + nlines * 128;
    nlines++;
  }
  fclose(fp);

  for (i = 0; i < nlines; i++) {
    /* Track current function */
    if (lp[i][0] != ' ' && lp[i][0] != '\t' && lp[i][0] != '.' &&
        lp[i][0] != '#' && lp[i][0] != '\n') {
      int k = 0;
      while (lp[i][k] && lp[i][k] != ':' && k < 127) {
        current_func[k] = lp[i][k];
        k++;
      }
      current_func[k] = 0;
    }

    /* Look for movsbl or movsbq (sign-extending byte to int/long) */
    if (strstr(lp[i], "movsbl ") || strstr(lp[i], "movsbq ") ||
        strstr(lp[i], "movswl ") || strstr(lp[i], "movswq ")) {
      /* Scan forward within window for memcpy/memset/memmove call */
      int j;
      for (j = i + 1; j < nlines && j < i + SIGNEXT_WINDOW; j++) {
        if (strstr(lp[j], "call memcpy") ||
            strstr(lp[j], "call memset") ||
            strstr(lp[j], "call memmove")) {
          findings++;
          printf("[--security-signext] CWE-122 WARNING: sign-extension at line %d "
                 "feeds %s in %s() (asm lines %d->%d)\n",
                 i + 1, strstr(lp[j], "call ") + 5,
                 current_func[0] ? current_func : "<unknown>",
                 i + 1, j + 1);
          break;  /* don't double-report same sign-ext */
        }
        /* Stop at label or ret - different basic block */
        if (lp[j][0] == '.' && lp[j][1] == 'L') break;
        if (strstr(lp[j], "ret")) break;
      }
    }
  }

  free(line_buf);
  free(lp);

  if (findings) {
    printf("[--security-signext] %d potential sign-extension overflow(s) found\n",
           findings);
  } else {
    printf("[--security-signext] Clean: no sign-extension -> memcpy patterns\n");
  }
}

/* ================================================================ */
/* SECURITY PASS: --security-476 (CWE-476)                           */
/* Scans assembly for nullable-return calls (malloc, calloc, realloc, */
/* fopen, etc.) followed by dereference of %rax without null check.  */
/* ================================================================ */

#define NULLDEREF_WINDOW 30

static int is_nullable_call(const char *line) {
  return strstr(line, "call malloc") || strstr(line, "call calloc") ||
         strstr(line, "call realloc") || strstr(line, "call fopen") ||
         strstr(line, "call fdopen") || strstr(line, "call tmpfile") ||
         strstr(line, "call strdup") || strstr(line, "call strndup") ||
         strstr(line, "call mmap");
}

static int is_null_check(const char *line) {
  /* testq %rax, %rax  or  cmpq $0, %rax */
  return strstr(line, "testq %rax") || strstr(line, "cmpq $0, %rax") ||
         strstr(line, "test %eax") || strstr(line, "cmp $0, %eax");
}

static int is_rax_deref(const char *line) {
  /* (%rax) — memory access through %rax without offset guard */
  return strstr(line, "(%rax)") != 0;
}

static int is_rax_moved(const char *line) {
  /* movq %rax, %r.. — return value moved to another register */
  if (strstr(line, "movq %rax, %r") && !strstr(line, "movq %rax, %rsp") &&
      !strstr(line, "movq %rax, %rbp"))
    return 1;
  return 0;
}

int g_emit_anomalies = 0;

/* Tiered type-to-string mapping for anomaly metadata.
 * Shorthand "ptr" covers all pointer/array/function types.
 * Aggregates are flagged for propose-only by returning their category.
 * char/bool both default to return 0/false; documented here as compatible. */
static const char *type_to_str(Type *ty) {
  if (!ty) return "unknown";
  switch (ty->kind) {
    case TY_VOID:   return "void";
    case TY_INT:    return "int";
    case TY_CHAR:   return "char";
    case TY_SHORT:  return "short";
    case TY_LONG:   return "long";
    case TY_LONGLONG: return "long long";
    case TY_UINT:   return "uint";
    case TY_UCHAR:  return "uchar";
    case TY_USHORT: return "ushort";
    case TY_ULONG:  return "ulong";
    case TY_ULONGLONG: return "ulonglong";
    case TY_FLOAT:  return "float";
    case TY_DOUBLE: return "double";
    case TY_PTR:
    case TY_ARRAY:
    case TY_FUNC:   return "ptr";
    case TY_STRUCT: return "struct";
    case TY_UNION:  return "union";
    case TY_ENUM:   return "enum";
    default:        return "unknown";
  }
}

static void security_nullderef_scan(Compiler *cc, char *filename) {
  FILE *fp;
  int nlines = 0;
  int findings = 0;
  int i;
  char current_func[128];
  char *line_buf;
  char **lp;
  int max_lines = 32768;

  fp = fopen(filename, "r");
  if (!fp) return;

  line_buf = (char *)malloc(max_lines * 128);
  lp = (char **)malloc(max_lines * sizeof(char *));
  if (!line_buf || !lp) { fclose(fp); return; }

  current_func[0] = 0;
  while (nlines < max_lines && fgets(line_buf + nlines * 128, 128, fp)) {
    lp[nlines] = line_buf + nlines * 128;
    nlines++;
  }
  fclose(fp);

  for (i = 0; i < nlines; i++) {
    /* Track current function */
    if (lp[i][0] != ' ' && lp[i][0] != '\t' && lp[i][0] != '.' &&
        lp[i][0] != '#' && lp[i][0] != '\n') {
      int k = 0;
      while (lp[i][k] && lp[i][k] != ':' && k < 127) {
        current_func[k] = lp[i][k];
        k++;
      }
      current_func[k] = 0;
    }

    /* Look for nullable-return function calls */
    if (is_nullable_call(lp[i])) {
      int j;
      int saw_null_check = 0;
      const char *call_name = strstr(lp[i], "call ") + 5;
      char clean_call[64];
      int c_idx = 0;
      while (call_name[c_idx] && call_name[c_idx] != '\n' && call_name[c_idx] != '@' && call_name[c_idx] != ' ' && c_idx < 63) {
          clean_call[c_idx] = call_name[c_idx];
          c_idx++;
      }
      clean_call[c_idx] = '\0';

      char tracked_reg[8];  /* register holding the return value */
      char deref_pat[16];   /* "(%rXX)" pattern to search for */
      char null_pat1[32];   /* "testq %rXX, %rXX" */
      char null_pat2[32];   /* "cmpq $0, %rXX" */

      strcpy(tracked_reg, "%rax");
      strcpy(deref_pat, "(%rax)");
      strcpy(null_pat1, "testq %rax");
      strcpy(null_pat2, "cmpq $0, %rax");

      char current_owner[128];
      char current_file[128];
      int current_line = 0;
      current_owner[0] = 0;
      current_file[0] = 0;

      for (j = i + 1; j < nlines && j < i + NULLDEREF_WINDOW; j++) {
        /* Parse latest ZCC_META for our tracked register */
        /* Format: # ZCC_META %reg=owner@file:line */
        char meta_pat[64];
        sprintf(meta_pat, "# ZCC_META %s=", tracked_reg);
        char *meta_match = strstr(lp[j], meta_pat);
        if (meta_match) {
           meta_match += strlen(meta_pat);
           /* owner */
           int k = 0;
           while (meta_match[k] && meta_match[k] != '@' && k < 127) {
               current_owner[k] = meta_match[k];
               k++;
           }
           current_owner[k] = 0;
           if (strcmp(current_owner, "_") == 0) current_owner[0] = 0;
           
           if (meta_match[k] == '@') {
               meta_match += k + 1;
               /* file */
               k = 0;
               while (meta_match[k] && meta_match[k] != ':' && k < 127) {
                   current_file[k] = meta_match[k];
                   k++;
               }
               current_file[k] = 0;
               
               if (meta_match[k] == ':') {
                   current_line = atoi(meta_match + k + 1);
               }
           }
        }

        /* If we see a null check on tracked reg OR %rax, we're safe */
        if (strstr(lp[j], null_pat1) || strstr(lp[j], null_pat2) ||
            strstr(lp[j], "cmpq $0, %rax") || strstr(lp[j], "testq %rax")) {
          saw_null_check = 1;
          break;
        }
        /* If %rax is moved to another register, follow the copy */
        if (strcmp(tracked_reg, "%rax") == 0 && is_rax_moved(lp[j])) {
          /* Extract destination: "movq %rax, %rXX" */
          char *dst = strstr(lp[j], ", %r");
          if (dst) {
            int k = 0;
            dst += 2;  /* skip ", " */
            while (dst[k] && dst[k] != '\n' && dst[k] != ' ' && k < 5) {
              tracked_reg[k] = dst[k];
              k++;
            }
            tracked_reg[k] = 0;
            sprintf(deref_pat, "(%s)", tracked_reg);
            sprintf(null_pat1, "testq %s", tracked_reg);
            sprintf(null_pat2, "cmpq $0, %s", tracked_reg);
          }
          continue;  /* keep scanning with new register */
        }
        /* If there's a deref of the tracked register OR %rax (which
         * may be a reload of the tracked register) without a null check */
        if ((strstr(lp[j], deref_pat) || strstr(lp[j], "(%rax)")) &&
            !saw_null_check) {
          findings++;
          char *reported_var = current_owner[0] ? current_owner : clean_call;
          if (g_emit_anomalies) {
              /* Option 1: JSON consumers (like the mutation applier) require a valid
               * lvalue owner to synthesize a guard. If missing (e.g. member assignment),
               * suppress the anomaly entirely to prevent unactionable reports.
               * Human reviewers (--security-476) still see the function-name fallback.
               */
              if (!current_owner[0]) {
                  break;
              }
              Symbol *sym = scope_find(cc, current_func);
              const char *ret_type = "unknown";
              if (sym && sym->type && sym->type->kind == TY_FUNC) {
                  ret_type = type_to_str(sym->type->ret);
              }
              printf("{\"type\":\"ir_anomaly\", \"kind\":\"CWE-476\", \"site\":\"%s:%s:%d\", \"severity\":0.7, \"variable\":\"%s\", \"return_type\":\"%s\"}\n",
                     current_file[0] ? current_file : (cc->filename ? cc->filename : "unknown"),
                     current_func[0] ? current_func : "unknown",
                     current_line, reported_var, ret_type);
          } else {
              printf("[--security-476] CWE-476 WARNING: unchecked %s return "
                     "dereferenced via %s in %s() (asm lines %d->%d)\n",
                     clean_call, reported_var,
                     current_func[0] ? current_func : "<unknown>",
                     i + 1, j + 1);
          }
          break;
        }
        /* Stop at label or ret - different basic block */
        if (lp[j][0] == '.' && lp[j][1] == 'L') break;
        if (strstr(lp[j], "ret")) break;
        /* Stop at another call - %rax is clobbered */
        if (strstr(lp[j], "call ")) break;
      }
    }
  }

  free(line_buf);
  free(lp);

  if (findings) {
    printf("[--security-476] %d potential null-deref(s) found\n", findings);
  } else {
    printf("[--security-476] Clean: all nullable returns checked\n");
  }
}

static void security_bounds_scan(Compiler *cc, char *filename) {
  FILE *fp;
  int nlines = 0;
  int findings = 0;
  int i;
  char current_func[128];
  char *line_buf;
  char **lp;
  int max_lines = 32768;

  fp = fopen(filename, "r");
  if (!fp) return;

  line_buf = (char *)malloc(max_lines * 128);
  lp = (char **)malloc(max_lines * sizeof(char *));
  if (!line_buf || !lp) { fclose(fp); return; }

  current_func[0] = 0;
  while (nlines < max_lines && fgets(line_buf + nlines * 128, 128, fp)) {
    lp[nlines] = line_buf + nlines * 128;
    nlines++;
  }
  fclose(fp);

  for (i = 0; i < nlines; i++) {
    /* Track current function */
    if (lp[i][0] != ' ' && lp[i][0] != '\t' && lp[i][0] != '.' &&
        lp[i][0] != '#' && lp[i][0] != '\n') {
      int k = 0;
      while (lp[i][k] && lp[i][k] != ':' && k < 127) {
        current_func[k] = lp[i][k];
        k++;
      }
      current_func[k] = 0;
    }

    if (strstr(lp[i], "# ZCC_META_ARRAY")) {
      char array_name[64] = "unknown";
      char index_name[64] = "?";
      char size_str[32] = "0";
      char file_name[128] = "unknown";
      int line_num = 0;

      char *ptr = strstr(lp[i], "array=");
      if (ptr) {
          ptr += 6;
          int k = 0;
          while (ptr[k] && ptr[k] != ' ' && k < 63) array_name[k++] = ptr[k];
          array_name[k] = 0;
      }
      
      ptr = strstr(lp[i], "index=");
      if (ptr) {
          ptr += 6;
          int k = 0;
          while (ptr[k] && ptr[k] != ' ' && k < 63) index_name[k++] = ptr[k];
          index_name[k] = 0;
      }

      ptr = strstr(lp[i], "size=");
      if (ptr) {
          ptr += 5;
          int k = 0;
          while (ptr[k] && ptr[k] != ' ' && k < 31) size_str[k++] = ptr[k];
          size_str[k] = 0;
      }
      
      ptr = strstr(lp[i], "@");
      if (ptr) {
          ptr += 1;
          int k = 0;
          while (ptr[k] && ptr[k] != ':' && k < 127) file_name[k++] = ptr[k];
          file_name[k] = 0;
          if (ptr[k] == ':') line_num = atoi(ptr + k + 1);
      }

      /* Do backwards walk looking for cmp bounds check */
      int saw_size_cmp = 0;
      int saw_cmp_instr = 0;
      char size_pat[64];
      sprintf(size_pat, "$%s", size_str);

      int j;
      for (j = i - 1; j >= 0 && j >= i - 30; j--) {
          if (strstr(lp[j], "ret")) break;
          if (strstr(lp[j], "call ")) break;
          if (lp[j][0] != ' ' && lp[j][0] != '\t' && lp[j][0] != '.' && lp[j][0] != '#') break; /* Func def */

          if (strstr(lp[j], size_pat)) saw_size_cmp = 1;
          if (strstr(lp[j], "cmp")) saw_cmp_instr = 1;
      }

      if (!(saw_size_cmp && saw_cmp_instr)) {
          findings++;
          if (g_emit_anomalies) {
               Symbol *sym = scope_find(cc, current_func);
               const char *ret_type = "unknown";
               if (sym && sym->type && sym->type->kind == TY_FUNC) {
                   ret_type = type_to_str(sym->type->ret);
               }
               printf("{\"type\":\"ir_anomaly\", \"kind\":\"CWE-787\", \"site\":\"%s:%s:%d\", \"severity\":0.8, \"array\":\"%s\", \"index\":\"%s\", \"size\":%s, \"return_type\":\"%s\"}\n",
                      file_name, current_func, line_num, array_name, index_name, size_str, ret_type);
          } else {
               printf("[--security-787] CWE-787 WARNING: unchecked array bounds for %s[%s] (size %s) in %s()\n",
                      array_name, index_name, size_str, current_func);
          }
      }
    }
  }

  free(line_buf);
  free(lp);

  if (findings) {
    if (!g_emit_anomalies) printf("[--security-787] %d potential bounds-violation(s) found\n", findings);
  } else {
    if (!g_emit_anomalies) printf("[--security-787] Clean: all bounded arrays checked\n");
  }
}

/* ================================================================ */
/* MAIN                                                              */
/* Bug fix: Compiler is heap-allocated (52KB+ struct, not stack)     */
/* ================================================================ */

/* Forward declaration for IR pass manager (linked separately) */
void ir_pm_run_default(void *mod_ptr, int verbose);

int main(int argc, char **argv) {
  Compiler *cc;
  char *input_file;
  char *output_file;
  char *source;
  int source_len;
  char asm_file[256];
  char cmd[512];
  Node *prog;
  int ret;
  int i;
  int al;

  input_file = 0;
  output_file = 0;

  int pp_only = 0;

  int zcc_verbose_flag = 0;

  int zcc_quiet_flag = 0;

  int compile_only = 0;

  int g_ir_primary = 0;

  /* parse arguments */
  for (i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-o") == 0) {
      i++;
      if (i < argc)
        output_file = argv[i];
    } else if (strcmp(argv[i], "-c") == 0) {
      compile_only = 1;
    } else if (strcmp(argv[i], "--pp-only") == 0) {
      pp_only = 1;
    } else if (strcmp(argv[i], "-v") == 0) {
      zcc_verbose_flag = 1;
    } else if (strcmp(argv[i], "--quiet") == 0 || strcmp(argv[i], "-q") == 0) {
      zcc_quiet_flag = 1;
    } else if (strcmp(argv[i], "--ir") == 0) {
      g_emit_ir = 1;
      g_ir_primary = 1;
    } else if (strcmp(argv[i], "--telemetry") == 0) {
      enable_telemetry_stdout = 1;
      setenv("ZCC_EMIT_TELEMETRY", "1", 1);
    } else if (strcmp(argv[i], "--manifold") == 0) {
      g_manifold_enabled = 1;
    } else if (strcmp(argv[i], "--manifold-deterministic") == 0) {
      g_manifold_enabled = 1;
      /* sigma=0 enforced inside ir_pass_manager via deterministic flag;
       * set env so manifold engine sees it regardless of cfg path. */
      setenv("ZCC_MANIFOLD_DET", "1", 1);
    } else if (strncmp(argv[i], "--ir-export=", 12) == 0) {
      strncpy(g_ir_export_path, argv[i] + 12, 255);
      g_ir_export_path[255] = '\0';
      g_manifold_enabled = 1;  /* export implies manifold active */
    } else if (strcmp(argv[i], "--peephole") == 0) {
      g_peephole_enabled = 1;
    } else if (strcmp(argv[i], "--peephole-deterministic") == 0) {
      g_peephole_enabled = 1;
      g_peephole_deterministic = 1;
    } else if (strcmp(argv[i], "--peephole-verbose") == 0) {
      g_peephole_verbose = 1;
    } else if (strcmp(argv[i], "--security-signext") == 0) {
      g_security_signext = 1;
    } else if (strcmp(argv[i], "--security-476") == 0) {
      g_security_476 = 1;
    } else if (strcmp(argv[i], "--security-787") == 0) {
      g_security_787 = 1;
    } else if (strcmp(argv[i], "--emit-anomalies") == 0) {
      g_emit_anomalies = 1;
      g_security_476 = 1;
      g_security_787 = 1;
    } else if (strncmp(argv[i], "-l", 2) == 0 || strncmp(argv[i], "-L", 2) == 0 || strncmp(argv[i], "-O", 2) == 0) {
      /* ignore linker flags */
    } else {
      input_file = argv[i];
    }
  }

  /* Stderr policy: open by default, --quiet silences.
   * -v adds extra diagnostic output but stderr is always open unless -q. */
  if (zcc_quiet_flag) {
#ifdef _WIN32
    freopen("nul", "w", stderr);
#else
    freopen("/dev/null", "w", stderr);
#endif
  }

  if (!input_file) {
    printf("Usage: zcc <input.c> [-o output]\n");
    return 1;
  }

  if (!output_file)
    output_file = "a.out";

  ZCC_IR_INIT();
  if (enable_telemetry_stdout) {
      ir_telemetry_enable_stdout();
  }

  /* read source file */
  source = read_file(input_file, &source_len);
  if (!source) {
    printf("zcc: cannot read '%s'\n", input_file);
    return 1;
  }

  /* PREPROCESSOR HOOK */
  {
    int pp_len;
    char *pp_source = zcc_preprocess(source, source_len, input_file, ".:./include", &pp_len);
    if (!pp_source) {
      fprintf(stderr, "zcc: preprocessing failed\n");
      return 1;
    }
    /* We leak original `source` here because we replaced it.
       ZCC doesn't care much about leaking in main string allocations anyway. */
    source = pp_source;
    source_len = pp_len;
  }

  if (pp_only) {
    if (!enable_telemetry_stdout) printf("%s", source);
    return 0;
  }

  /* heap-allocate compiler state (too large for stack) */
  cc = (Compiler *)calloc(1, sizeof(Compiler));
  if (!cc) {
    if (!enable_telemetry_stdout) printf("zcc: out of memory\n");
    free(source);
    return 1;
  }

  cc->source = source;
  cc->source_len = source_len;
  cc->filename = input_file;

  init_compiler(cc);

  /* generate asm file name */
  strncpy(asm_file, output_file, 250);
  al = 0;
  while (asm_file[al])
    al++;

  int stop_at_asm = 0;
  if (al >= 2 && asm_file[al - 2] == '.' && asm_file[al - 1] == 's') {
    stop_at_asm = 1;
  } else {
    asm_file[al] = '.';
    asm_file[al + 1] = 's';
    asm_file[al + 2] = 0;
  }

  /* open output */
  cc->out = fopen(asm_file, "w");
  if (!cc->out) {
    if (!enable_telemetry_stdout) printf("zcc: cannot write '%s'\n", asm_file);
    free(source);
    free(cc);
    return 1;
  }

  /* lex first token */
  if (!enable_telemetry_stdout) printf("[Phase 1] Lexical Array Bootstrap... OK\n");
  next_token(cc);

  /* parse */
  if (!enable_telemetry_stdout) printf("[Phase 2] AST Topological Generation... ");
  prog = parse_program(cc);

  if (cc->errors > 0) {
    if (!enable_telemetry_stdout) printf("\033[0;31mFAILED\033[0m\n");
    if (!enable_telemetry_stdout) printf("zcc: %d error(s)\n", cc->errors);
    fclose(cc->out);
    free(source);
    free(cc);
    return 1;
  }

  if (!enable_telemetry_stdout) printf("OK\n");

  /* generate code */
  if (!enable_telemetry_stdout) printf("[Phase 3] Native AST Constant Folding... OK\n");
  if (!enable_telemetry_stdout) printf("[Phase 4] SystemV ABI X86-64 Codegen... OK\n");
  fprintf(cc->out, "# ZCC asm begin\n");
  codegen_program(cc, prog);
  fclose(cc->out);

  /* IR pass manager — runs when --ir flag is active */
  if (g_ir_primary && g_ir_module) {
    int ir_total_nodes = 0;
    int ir_fi;
    for (ir_fi = 0; ir_fi < g_ir_module->func_count; ir_fi++) {
      ir_total_nodes += g_ir_module->funcs[ir_fi]->node_count;
    }
    if (!enable_telemetry_stdout) printf("[Phase IR] IR Pass Manager (%d funcs, %d nodes)...\n",
           g_ir_module->func_count, ir_total_nodes);
    ir_pm_run_default(g_ir_module, 1);
    if (!enable_telemetry_stdout) printf("[Phase IR] Pass Manager Complete.\n");
  }

  if (!g_ir_primary) {
    ZCC_IR_FLUSH(stdout);
  }

  /* peephole optimize the emitted assembly safely out-of-bounds */
  peephole_optimize(asm_file);

  /* security pass: sign-extension overflow detection */
  if (g_security_signext) {
    security_signext_scan(asm_file);
  }

  /* security pass: null-deref detection */
  if (g_security_476) {
    security_nullderef_scan(cc, asm_file);
  }

  /* security pass: array bounds detection */
  if (g_security_787) {
    security_bounds_scan(cc, asm_file);
  }

  /* assemble and link if not stopping at assembly */
  if (!stop_at_asm) {
    if (!enable_telemetry_stdout) printf("[Phase 6] GCC Assembly/Linker Binding... ");
    if (compile_only) {
      sprintf(cmd, "gcc -O0 -w -fno-asynchronous-unwind-tables -Wa,--noexecstack -fno-unwind-tables -c -o %s %s 2>&1", output_file, asm_file);
    } else if (strcmp(input_file, "zcc.c") == 0 || (strlen(input_file) >= 6 && strcmp(input_file + strlen(input_file) - 6, "/zcc.c") == 0)) {
      sprintf(cmd, "gcc -O0 -w -fno-asynchronous-unwind-tables -Wa,--noexecstack -fno-unwind-tables -o %s %s compiler_passes.c compiler_passes_ir.c ir_pass_manager.c -lm 2>&1", output_file, asm_file);
    } else {
      sprintf(cmd, "gcc -O0 -w -fno-asynchronous-unwind-tables -Wa,--noexecstack -fno-unwind-tables -o %s %s -lm -lpthread -ldl 2>&1", output_file, asm_file);
    }
    ret = system(cmd);
    if (ret != 0) {
      if (!enable_telemetry_stdout) printf("FAILED\n");
      if (!enable_telemetry_stdout) printf("zcc: assembly/linking failed\n");
      free(source);
      free(cc);
      return 1;
    }
    if (!enable_telemetry_stdout) printf("OK\n");
  }

  if (!enable_telemetry_stdout) printf("[OK] ZCC Engine Compilation Terminated Successfully.\n");

  free(source);
  free(cc);
  return 0;
}

/* ZKAEDI FORCE RENDER CACHE INVALIDATION */
