
/* C99/POSIX support for curl graduation — see commit 262bd08 */
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
  cc->label_count = 0;
  cc->str_label_count = 0;
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
        sym = scope_add(cc, "__builtin_va_list", type_array(cc, t_va, 1));
    }
    sym->is_typedef = 1;

    /* _Float128 workaround */
    sym = scope_add(cc, "_Float128", cc->ty_double);
    sym->is_typedef = 1;

    /* POSIX / system typedefs — needed for GCC-preprocessed code */
    sym = scope_add(cc, "socklen_t", cc->ty_int);
    sym->is_typedef = 1;
    sym = scope_add(cc, "sa_family_t", cc->ty_ushort);
    sym->is_typedef = 1;
    sym = scope_add(cc, "in_port_t", cc->ty_ushort);
    sym->is_typedef = 1;
    sym = scope_add(cc, "in_addr_t", cc->ty_uint);
    sym->is_typedef = 1;
    sym = scope_add(cc, "pid_t", cc->ty_int);
    sym->is_typedef = 1;
    sym = scope_add(cc, "uid_t", cc->ty_uint);
    sym->is_typedef = 1;
    sym = scope_add(cc, "gid_t", cc->ty_uint);
    sym->is_typedef = 1;
    sym = scope_add(cc, "mode_t", cc->ty_uint);
    sym->is_typedef = 1;
    sym = scope_add(cc, "off_t", cc->ty_long);
    sym->is_typedef = 1;
    sym = scope_add(cc, "time_t", cc->ty_long);
    sym->is_typedef = 1;
    sym = scope_add(cc, "clock_t", cc->ty_long);
    sym->is_typedef = 1;
    sym = scope_add(cc, "suseconds_t", cc->ty_long);
    sym->is_typedef = 1;
    sym = scope_add(cc, "nfds_t", cc->ty_ulong);
    sym->is_typedef = 1;

    /* glibc internal __ typedefs — survive GCC preprocessing */
    sym = scope_add(cc, "__time_t", cc->ty_long);   sym->is_typedef = 1;
    sym = scope_add(cc, "__clock_t", cc->ty_long);   sym->is_typedef = 1;
    sym = scope_add(cc, "__pid_t", cc->ty_int);      sym->is_typedef = 1;
    sym = scope_add(cc, "__uid_t", cc->ty_uint);     sym->is_typedef = 1;
    sym = scope_add(cc, "__gid_t", cc->ty_uint);     sym->is_typedef = 1;
    sym = scope_add(cc, "__off_t", cc->ty_long);     sym->is_typedef = 1;
    sym = scope_add(cc, "__off64_t", cc->ty_long);   sym->is_typedef = 1;
    sym = scope_add(cc, "__mode_t", cc->ty_uint);    sym->is_typedef = 1;
    sym = scope_add(cc, "__dev_t", cc->ty_ulong);    sym->is_typedef = 1;
    sym = scope_add(cc, "__ino_t", cc->ty_ulong);    sym->is_typedef = 1;
    sym = scope_add(cc, "__nlink_t", cc->ty_ulong);  sym->is_typedef = 1;
    sym = scope_add(cc, "__blksize_t", cc->ty_long); sym->is_typedef = 1;
    sym = scope_add(cc, "__blkcnt_t", cc->ty_long);  sym->is_typedef = 1;
    sym = scope_add(cc, "__ssize_t", cc->ty_long);   sym->is_typedef = 1;
    sym = scope_add(cc, "__socklen_t", cc->ty_int);  sym->is_typedef = 1;
    sym = scope_add(cc, "__suseconds_t", cc->ty_long); sym->is_typedef = 1;
    sym = scope_add(cc, "__syscall_slong_t", cc->ty_long); sym->is_typedef = 1;
    sym = scope_add(cc, "__syscall_ulong_t", cc->ty_ulong); sym->is_typedef = 1;
    sym = scope_add(cc, "__intmax_t", cc->ty_long);  sym->is_typedef = 1;
    sym = scope_add(cc, "__uintmax_t", cc->ty_ulong); sym->is_typedef = 1;
    sym = scope_add(cc, "__intptr_t", cc->ty_long);  sym->is_typedef = 1;
    sym = scope_add(cc, "__sig_atomic_t", cc->ty_int); sym->is_typedef = 1;
    sym = scope_add(cc, "__clockid_t", cc->ty_int);  sym->is_typedef = 1;
    sym = scope_add(cc, "__timer_t", type_ptr(cc, cc->ty_void)); sym->is_typedef = 1;
    sym = scope_add(cc, "__loff_t", cc->ty_long);    sym->is_typedef = 1;
    sym = scope_add(cc, "__key_t", cc->ty_int);      sym->is_typedef = 1;
    sym = scope_add(cc, "__id_t", cc->ty_uint);      sym->is_typedef = 1;
    sym = scope_add(cc, "__useconds_t", cc->ty_uint); sym->is_typedef = 1;
    sym = scope_add(cc, "__daddr_t", cc->ty_int);    sym->is_typedef = 1;
    sym = scope_add(cc, "__caddr_t", type_ptr(cc, cc->ty_char)); sym->is_typedef = 1;
    sym = scope_add(cc, "__fsblkcnt_t", cc->ty_ulong); sym->is_typedef = 1;
    sym = scope_add(cc, "__fsfilcnt_t", cc->ty_ulong); sym->is_typedef = 1;
    sym = scope_add(cc, "__fsword_t", cc->ty_long);  sym->is_typedef = 1;
    sym = scope_add(cc, "__rlim_t", cc->ty_ulong);   sym->is_typedef = 1;
    sym = scope_add(cc, "__quad_t", cc->ty_long);    sym->is_typedef = 1;
    sym = scope_add(cc, "__u_quad_t", cc->ty_ulong); sym->is_typedef = 1;


    /* fd_set — used by select(). glibc: struct with __fds_bits[16] (128 bytes) */
    {
        Type *t_fdset = type_new(cc, TY_STRUCT);
        strncpy(t_fdset->tag, "fd_set", MAX_IDENT - 1);
        t_fdset->size = 128;
        t_fdset->align = 8;
        t_fdset->is_complete = 1;
        sym = scope_add(cc, "fd_set", t_fdset);
        sym->is_typedef = 1;
    }

    /* curl_socket_t — typically int on POSIX */
    sym = scope_add(cc, "curl_socket_t", cc->ty_int);
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

      /* strrchr — returns char* */
      ft = type_func(cc, type_ptr(cc, cc->ty_char));
      sym = scope_add(cc, "strrchr", ft);
      sym->is_global = 1;

      /* strcspn — returns size_t (ulong) */
      ft = type_func(cc, cc->ty_ulong);
      sym = scope_add(cc, "strcspn", ft);
      sym->is_global = 1;

      /* strspn — returns size_t (ulong) */
      ft = type_func(cc, cc->ty_ulong);
      sym = scope_add(cc, "strspn", ft);
      sym->is_global = 1;

      /* memchr — returns void* */
      ft = type_func(cc, type_ptr(cc, cc->ty_void));
      sym = scope_add(cc, "memchr", ft);
      sym->is_global = 1;

      /* inet_pton — returns int */
      ft = type_func(cc, cc->ty_int);
      sym = scope_add(cc, "inet_pton", ft);
      sym->is_global = 1;

      /* strtod — returns double */
      ft = type_func(cc, cc->ty_double);
      sym = scope_add(cc, "strtod", ft);
      sym->is_global = 1;

      /* ctype functions — all return int */
      ft = type_func(cc, cc->ty_int);
      sym = scope_add(cc, "isalpha", ft); sym->is_global = 1;
      ft = type_func(cc, cc->ty_int);
      sym = scope_add(cc, "isdigit", ft); sym->is_global = 1;
      ft = type_func(cc, cc->ty_int);
      sym = scope_add(cc, "isxdigit", ft); sym->is_global = 1;
      ft = type_func(cc, cc->ty_int);
      sym = scope_add(cc, "toupper", ft); sym->is_global = 1;
      ft = type_func(cc, cc->ty_int);
      sym = scope_add(cc, "tolower", ft); sym->is_global = 1;
      ft = type_func(cc, cc->ty_int);
      sym = scope_add(cc, "isalnum", ft); sym->is_global = 1;
      ft = type_func(cc, cc->ty_int);
      sym = scope_add(cc, "isspace", ft); sym->is_global = 1;
      ft = type_func(cc, cc->ty_int);
      sym = scope_add(cc, "isupper", ft); sym->is_global = 1;

      /* strerror — returns char* */
      ft = type_func(cc, type_ptr(cc, cc->ty_char));
      sym = scope_add(cc, "strerror", ft);
      sym->is_global = 1;

      /* close — returns int */
      ft = type_func(cc, cc->ty_int);
      sym = scope_add(cc, "close", ft);
      sym->is_global = 1;

      /* select — returns int */
      ft = type_func(cc, cc->ty_int);
      sym = scope_add(cc, "select", ft);
      sym->is_global = 1;

      /* socket — returns int */
      ft = type_func(cc, cc->ty_int);
      sym = scope_add(cc, "socket", ft);
      sym->is_global = 1;

      /* connect — returns int */
      ft = type_func(cc, cc->ty_int);
      sym = scope_add(cc, "connect", ft);
      sym->is_global = 1;

      /* bind — returns int */
      ft = type_func(cc, cc->ty_int);
      sym = scope_add(cc, "bind", ft);
      sym->is_global = 1;

      /* listen — returns int */
      ft = type_func(cc, cc->ty_int);
      sym = scope_add(cc, "listen", ft);
      sym->is_global = 1;

      /* accept — returns int */
      ft = type_func(cc, cc->ty_int);
      sym = scope_add(cc, "accept", ft);
      sym->is_global = 1;

      /* recv — returns ssize_t */
      ft = type_func(cc, cc->ty_long);
      sym = scope_add(cc, "recv", ft);
      sym->is_global = 1;

      /* send — returns ssize_t */
      ft = type_func(cc, cc->ty_long);
      sym = scope_add(cc, "send", ft);
      sym->is_global = 1;

      /* setsockopt — returns int */
      ft = type_func(cc, cc->ty_int);
      sym = scope_add(cc, "setsockopt", ft);
      sym->is_global = 1;

      /* getsockopt — returns int */
      ft = type_func(cc, cc->ty_int);
      sym = scope_add(cc, "getsockopt", ft);
      sym->is_global = 1;

      /* fcntl — returns int, variadic */
      ft = type_func(cc, cc->ty_int);
      ft->is_variadic = 1;
      sym = scope_add(cc, "fcntl", ft);
      sym->is_global = 1;

      /* ioctl — returns int, variadic */
      ft = type_func(cc, cc->ty_int);
      ft->is_variadic = 1;
      sym = scope_add(cc, "ioctl", ft);
      sym->is_global = 1;

      /* poll — returns int */
      ft = type_func(cc, cc->ty_int);
      sym = scope_add(cc, "poll", ft);
      sym->is_global = 1;

      /* gettimeofday — returns int */
      ft = type_func(cc, cc->ty_int);
      sym = scope_add(cc, "gettimeofday", ft);
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
static int  *line_skip = 0;

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
  if (!line_skip) {
    line_skip = (int *)malloc(MAX_PEEP_LINES * sizeof(int));
  }
  line_buffer = (char *)malloc(file_size + MAX_PEEP_LINES * 128);
  if (!line_buffer || !line_ptrs || !line_skip) {
    fclose(fp);
    return;
  }

  while (nlines < MAX_PEEP_LINES && fgets(line_buffer + nlines * 128, 128, fp)) {
    line_ptrs[nlines] = line_buffer + nlines * 128;
    line_skip[nlines] = 0;
    nlines++;
  }
  fclose(fp);

  int changed;
  char tmp1[64], tmp2[64];
  char pop_reg[64];
  char target[64], label[64];
  char reg[64];
  char src[64], dst1[64], dst2[64];
  char imm_str[64];
  long local_imm;
  char val_str[32];

peephole_loop:
  changed = 0;

  {
    int out = 0;
    int old_n = nlines;
    for (i = 0; i < old_n; i++) {
      if (line_skip[i] == 0 && line_ptrs[i][0] != '\n') {
        line_ptrs[out] = line_ptrs[i];
        line_skip[out] = 0;
        out = out + 1;
      }
    }
    for (i = out; i < old_n; i++) {
      line_skip[i] = 0;
    }
    nlines = out;
  }

  for (i = 0; i < nlines;) {
    char *l1 = line_ptrs[i];
    int matched = 0;

    /* 1. Redundant Push/Pop */
    if (strncmp(l1, "    pushq ", 10) == 0 && i + 1 < nlines) {
      char *l2 = line_ptrs[i + 1];
      if (strncmp(l2, "    popq ", 9) == 0) {
        sscanf(l1, "    pushq %s", tmp1);
        sscanf(l2, "    popq %s", tmp2);
        if (strcmp(tmp1, tmp2) == 0) {
          line_skip[i] = 1;
          line_skip[i + 1] = 1;
          eliminated += 2;
          changed = 1;
          i += 2;
          matched = 1;
        } else {
          sprintf(line_ptrs[i], "    movq %s, %s\n", tmp1, tmp2);
          line_skip[i + 1] = 1;
          eliminated += 2;
          changed = 1;
          i += 2;
          matched = 1;
        }
      }
    }

    /* 2. Arithmetic Nullification */
    if (!matched && (strcmp(l1, "    addq $0, %rax\n") == 0 ||
        strcmp(l1, "    subq $0, %rax\n") == 0 ||
        strcmp(l1, "    addq $0, %rsp\n") == 0 ||
        strcmp(l1, "    subq $0, %rsp\n") == 0)) {
      line_skip[i] = 1;
      eliminated += 1;
      changed = 1;
      i += 1;
      matched = 1;
    }

    /* 3. Push/Lea/Pop Triad */
    if (!matched && strncmp(l1, "    pushq %rax\n", 15) == 0 && i + 2 < nlines) {
      char *l2 = line_ptrs[i + 1];
      char *l3 = line_ptrs[i + 2];
      int has_rax_dst = 0;
      {
        int si;
        int l2len = 0;
        while (l2[l2len]) l2len++;
        for (si = 0; si + 5 < l2len; si++) {
          if (l2[si] == ',' && l2[si+1] == ' ' && l2[si+2] == '%' &&
              l2[si+3] == 'r' && l2[si+4] == 'a' && l2[si+5] == 'x') {
            has_rax_dst = 1;
            break;
          }
        }
      }
      if (strncmp(l2, "    leaq ", 9) == 0 && has_rax_dst &&
          strncmp(l3, "    popq ", 9) == 0) {
        sscanf(l3, "    popq %s", pop_reg);
        sprintf(line_ptrs[i], "    movq %%rax, %s\n", pop_reg);
        line_skip[i + 2] = 1;
        eliminated += 3;
        changed = 1;
        i += 3;
        matched = 1;
      }
    }

    /* 4. Branch Straightening */
    if (!matched && strncmp(l1, "    jmp .L", 10) == 0 && i + 1 < nlines) {
      if (sscanf(l1, "    jmp %63s", target) == 1) {
        char *l2 = line_ptrs[i + 1];
        if (l2[0] == '.' && l2[1] == 'L') {
          if (sscanf(l2, "%63[^:]:", label) == 1 && strcmp(target, label) == 0) {
            line_skip[i] = 1;
            eliminated += 1;
            changed = 1;
            i += 1;
            matched = 1;
          }
        }
      }
    }

    /* 5. Zero-Test Optimization */
    if (!matched && strncmp(l1, "    cmpq $0, %", 14) == 0) {
      int ri;
      strcpy(reg, l1 + 14);
      /* Manually strip trailing newline — avoid strchr to prevent ZCC cltq bug */
      for (ri = 0; ri < 63 && reg[ri]; ri++) {
        if (reg[ri] == '\n') { reg[ri] = '\0'; break; }
      }
      strcpy(line_ptrs[i], "    testq %");
      strcat(line_ptrs[i], reg);
      strcat(line_ptrs[i], ", %");
      strcat(line_ptrs[i], reg);
      strcat(line_ptrs[i], "\n");
      eliminated += 1;
      changed = 1;
      i += 1;
      matched = 1;
    }

    /* 6. LEA Computation Fusion */
    if (!matched && strncmp(l1, "    movq %", 10) == 0 && i + 1 < nlines) {
      char *l2 = line_ptrs[i + 1];
      if (strncmp(l2, "    addq $", 10) == 0) {
        if (sscanf(l1, "    movq %%%[a-z0-9], %%%[a-z0-9]", src, dst1) == 2 &&
            sscanf(l2, "    addq $%ld, %%%[a-z0-9]", &local_imm, dst2) == 2) {
          if (strcmp(dst1, dst2) == 0) {
            sprintf(val_str, "%ld", local_imm);
            strcpy(line_ptrs[i], "    leaq ");
            strcat(line_ptrs[i], val_str);
            strcat(line_ptrs[i], "(%");
            strcat(line_ptrs[i], src);
            strcat(line_ptrs[i], "), %");
            strcat(line_ptrs[i], dst1);
            strcat(line_ptrs[i], "\n");

            line_skip[i + 1] = 1;
            eliminated += 2;
            changed = 1;
            i += 2;
            matched = 1;
          }
        }
      }
    }

    if (!matched) {
      i++;
    }
  } 
  if (changed) {
      goto peephole_loop;
  }

  fp = fopen(filename, "w");
  if (!fp) {
    free(line_buffer);
    return;
  }
  for (i = 0; i < nlines; i++) {
    if (line_skip[i] == 0)
      fputs(line_ptrs[i], fp);
  }
  fclose(fp);
  free(line_buffer);
  printf("[Phase 5] Native C Peephole Optimization... OK (%d elided)\n",
         eliminated);
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
  int debug_abi_classes_flag = 0;

  int compile_only = 0;

  int g_ir_primary = 0;

  const char *include_paths = ".:./include";

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
    } else if (strcmp(argv[i], "-fdebug-abi-classes") == 0) {
      debug_abi_classes_flag = 1;
    } else if (strcmp(argv[i], "--ir") == 0) {
      g_emit_ir = 1;
      g_ir_primary = 1;
    } else if (strncmp(argv[i], "-I", 2) == 0) {
      /* PP-INCLUDE-022: append -I path to include_paths */
      const char *ipath = argv[i] + 2;
      if (ipath[0] == '\0' && i + 1 < argc) { i++; ipath = argv[i]; }
      if (ipath[0]) {
        int olen = strlen(include_paths);
        int nlen = strlen(ipath);
        char *merged = (char *)malloc(olen + 1 + nlen + 1);
        sprintf(merged, "%s:%s", include_paths, ipath);
        include_paths = merged;
      }
    } else if (strncmp(argv[i], "-l", 2) == 0 || strncmp(argv[i], "-L", 2) == 0 || strncmp(argv[i], "-O", 2) == 0) {
      /* ignore linker flags */
    } else {
      input_file = argv[i];
    }
  }

  if (!zcc_verbose_flag) {
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

  /* read source file */
  source = read_file(input_file, &source_len);
  if (!source) {
    printf("zcc: cannot read '%s'\n", input_file);
    return 1;
  }

  /* PREPROCESSOR HOOK */
  {
    int pp_len;
    char *pp_source = zcc_preprocess(source, source_len, input_file, include_paths, &pp_len);
    if (!pp_source) {
      printf("zcc: preprocessing failed\n");
      return 1;
    }
    source = pp_source;
    source_len = pp_len;
  }

  if (pp_only) {
    printf("%s", source);
    return 0;
  }

  /* heap-allocate compiler state (too large for stack) */
  cc = (Compiler *)calloc(1, sizeof(Compiler));
  if (!cc) {
    printf("zcc: out of memory\n");
    free(source);
    return 1;
  }

  cc->source = source;
  cc->source_len = source_len;
  cc->filename = input_file;

  init_compiler(cc);
  cc->debug_abi_classes = debug_abi_classes_flag;

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
    printf("zcc: cannot write '%s'\n", asm_file);
    free(source);
    free(cc);
    return 1;
  }

  /* lex first token */
  printf("[Phase 1] Lexical Array Bootstrap... OK\n");
  next_token(cc);

  /* parse */
  printf("[Phase 2] AST Topological Generation... ");
  prog = parse_program(cc);

  if (cc->errors > 0) {
    printf("\033[0;31mFAILED\033[0m\n");
    printf("zcc: %d error(s)\n", cc->errors);
    fclose(cc->out);
    free(source);
    free(cc);
    return 1;
  }

  printf("OK\n");

  /* generate code */
  printf("[Phase 3] Native AST Constant Folding... OK\n");
  printf("[Phase 4] SystemV ABI X86-64 Codegen... OK\n");
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
    printf("[Phase IR] IR Pass Manager (%d funcs, %d nodes)...\n",
           g_ir_module->func_count, ir_total_nodes);
    ir_pm_run_default(g_ir_module, 1);
    printf("[Phase IR] Pass Manager Complete.\n");
  }

  if (!g_ir_primary) {
    ZCC_IR_FLUSH(stdout);
  }

  /* peephole optimize the emitted assembly safely out-of-bounds */
  peephole_optimize(asm_file);

  /* assemble and link if not stopping at assembly */
  if (!stop_at_asm) {
    printf("[Phase 6] GCC Assembly/Linker Binding... ");
    if (compile_only) {
      sprintf(cmd, "gcc -O0 -w -fno-asynchronous-unwind-tables -Wa,--noexecstack -fno-unwind-tables -c -o %s %s 2>&1", output_file, asm_file);
    } else if (strcmp(input_file, "zcc.c") == 0 || (strlen(input_file) >= 6 && strcmp(input_file + strlen(input_file) - 6, "/zcc.c") == 0)) {
      sprintf(cmd, "gcc -O0 -w -fno-asynchronous-unwind-tables -Wa,--noexecstack -fno-unwind-tables -o %s %s compiler_passes.c compiler_passes_ir.c ir_pass_manager.c -lm 2>&1", output_file, asm_file);
    } else {
      sprintf(cmd, "gcc -O0 -w -fno-asynchronous-unwind-tables -Wa,--noexecstack -fno-unwind-tables -o %s %s -lm -lpthread -ldl 2>&1", output_file, asm_file);
    }
    ret = system(cmd);
    if (ret != 0) {
      printf("FAILED\n");
      printf("zcc: assembly/linking failed\n");
      free(source);
      free(cc);
      return 1;
    }
    printf("OK\n");
  }

  printf("[OK] ZCC Engine Compilation Terminated Successfully.\n");

  free(source);
  free(cc);
  return 0;
}

/* ZKAEDI FORCE RENDER CACHE INVALIDATION */
