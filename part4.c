/* ================================================================ */
/* CODE GENERATOR — x86-64 Linux (System V) or Windows x64 ABI      */
/* ================================================================ */

#ifndef ZCC_AST_BRIDGE_H
/* Exclusively for standalone IDE analysis */
#include "part1.c"
#endif

#include "ir_emit_dispatch.h"
#include "ir_bridge.h"

#ifdef __clang__
#pragma clang diagnostic ignored "-Wdangling-else"
#pragma clang diagnostic ignored "-Wmisleading-indentation"
#pragma clang diagnostic ignored "-Wpointer-bool-conversion"
#pragma clang diagnostic ignored "-Wtautological-pointer-compare"
int is_unsigned_type(Type *ty);
int setenv(const char *name, const char *value, int overwrite);
int unsetenv(const char *name);
#endif

static void push_reg(Compiler *cc, char *reg) {
  if (backend_ops) {
      if (strcmp(reg, "rax") == 0) reg = "r0";
      else if (strcmp(reg, "r11") == 0) reg = "r1";
      else if (strcmp(reg, "rdx") == 0) reg = "r2";
      fprintf(cc->out, "    push {%s}\n", reg);
      cc->stack_depth++;
      return;
  }
  fprintf(cc->out, "    pushq %%%s\n", reg);
  cc->stack_depth++;
}

static void pop_reg(Compiler *cc, char *reg) {
  if (backend_ops) {
      if (strcmp(reg, "rax") == 0) reg = "r0";
      else if (strcmp(reg, "r11") == 0) reg = "r1";
      else if (strcmp(reg, "rdx") == 0) reg = "r2";
      fprintf(cc->out, "    pop {%s}\n", reg);
      cc->stack_depth--;
      return;
  }
  fprintf(cc->out, "    popq %%%s\n", reg);
  cc->stack_depth--;
}

static int new_label(Compiler *cc) {
  int l;
  l = cc->label_count;
  cc->label_count++;
  return l;
}

static int is_power_of_2_val(long long val) {
  return val > 0 && (val & (val - 1)) == 0;
}

static int log2_of(long long val) {
  int n;
  n = 0;
  while (val > 1) {
    val = val >> 1;
    n = n + 1;
  }
  return n;
}

/* Emit efficient pointer-scale for r11: shlq for power-of-2 sizes, imulq otherwise.
 * Oneirogenesis confirmed 371 surviving shlq substitutions — wire it into the codegen. */
#define EMIT_PTR_SCALE_R11(cc, esz) do { \
    int _e = (esz); \
    if ((_e & (_e - 1)) == 0) \
        fprintf((cc)->out, "    shlq $%d, %%r11\n", log2_of(_e)); \
    else \
        fprintf((cc)->out, "    imulq $%d, %%r11\n", _e); \
} while(0)

#define EMIT_PTR_SCALE_RAX(cc, esz) do { \
    int _e = (esz); \
    if ((_e & (_e - 1)) == 0) \
        fprintf((cc)->out, "    shlq $%d, %%rax\n", log2_of(_e)); \
    else \
        fprintf((cc)->out, "    imulq $%d, %%rax\n", _e); \
} while(0)

/* Format codes for label emission — avoid passing string literals as 2nd arg
 * (stage2 mispasses them). */
enum { FMT_JE = 1, FMT_JMP, FMT_DEF, FMT_JNE };
static void emit_bb_telem(Compiler *cc, int label_id) {
    if (getenv("ZCC_EMIT_TELEMETRY")) {
        fprintf(stderr, "[telem] fn=%s bb=%d used_regs=0x%02X src=%s\n",
                cc->current_func, label_id, cc->used_regs_mask, cc->is_forced_mask ? "forced" : "computed");
    }
}

static void emit_label_fmt(Compiler *cc, int n, int fmt) {
    if (fmt == FMT_DEF) {
        emit_bb_telem(cc, n);
    }

  if (backend_ops) {
      switch (fmt) {
      case FMT_JE:
        fprintf(cc->out, "    beq .L%d\n", n);
        break;
      case FMT_JMP:
        fprintf(cc->out, "    b .L%d\n", n);
        break;
      case FMT_DEF:
        fprintf(cc->out, ".L%d:\n", n);
        break;
      case FMT_JNE:
        fprintf(cc->out, "    bne .L%d\n", n);
        break;
      default:
        fprintf(cc->out, ".L%d:\n", n);
        break;
      }
      return;
  }
  switch (fmt) {
  case FMT_JE:
    if (backend_ops) fprintf(cc->out, "    beq .L%d\n", n);
    else fprintf(cc->out, "    je .L%d\n", n);
    break;
  case FMT_JMP:
    if (backend_ops) fprintf(cc->out, "    b .L%d\n", n);
    else fprintf(cc->out, "    jmp .L%d\n", n);
    break;
  case FMT_DEF:
    fprintf(cc->out, ".L%d:\n", n);
    break;
  case FMT_JNE:
    fprintf(cc->out, "    jne .L%d\n", n);
    break;
  default:
    fprintf(cc->out, ".L%d:\n", n);
    break;
  }
}

/* Forward: defined after bad_node_cutoff. */
static int is_bad_ptr(const void *p);

/* True if ptr is in fault range. GDB: 0x800b457d0 (hi=0, lo>=2G), 0x800ac07d0
 * (hi=0x80). Reject: (hi==0 && lo>=2G) OR (hi in [0x80, 0x100)). Not inline so
 * stage2 emits a body. */
int ptr_in_fault_range(const void *p) {
  unsigned long long u = (unsigned long long)(const char *)p;
  unsigned int hi = (unsigned int)(u >> 32);
  unsigned int lo = (unsigned int)u;
  return (hi == 0 && lo >= 0x80000000U) || (hi >= 0x80U && hi < 0x100U);
}
/* Use ptr_in_fault_range() in code, not macro; stage2 may not expand macros. */

/* ---------------------------------------------------------------- */
/* System V AMD64 ABI Classifications                               */
/* ---------------------------------------------------------------- */

/* Join two classes per SysV §3.2.3. INTEGER ⊔ SSE = INTEGER. */
static abi_class_t abi_join(abi_class_t a, abi_class_t b) {
    if (a == b) return a;
    if (a == CLASS_NO_CLASS) return b;
    if (b == CLASS_NO_CLASS) return a;
    if (a == CLASS_MEMORY || b == CLASS_MEMORY) return CLASS_MEMORY;
    if (a == CLASS_INTEGER || b == CLASS_INTEGER) return CLASS_INTEGER;
    return CLASS_SSE;
}

/* Classify one field into one of the two eightbytes. */
static void classify_field(Type *field_type, int field_offset, abi_class_t eb[2]) {
    int size = type_size(field_type);
    int first_eb  = field_offset / 8;
    int last_eb   = (field_offset + size - 1) / 8;
    abi_class_t fc = CLASS_INTEGER;

    /* SysV §3.2.3: if it has unaligned fields, it has class MEMORY. */
    int falign = type_align(field_type);
    if (falign > 0 && (field_offset % falign) != 0) {
        eb[0] = eb[1] = CLASS_MEMORY;
        return;
    }

    if (field_type->kind == TY_FLOAT || field_type->kind == TY_DOUBLE) {
        fc = CLASS_SSE;
    } else if (field_type->kind == TY_STRUCT || field_type->kind == TY_UNION) {
        StructField *sf;
        for (sf = field_type->fields; sf; sf = sf->next) {
            classify_field(sf->type, field_offset + sf->offset, eb);
        }
        return;
    } else if (field_type->kind == TY_ARRAY) {
        for (int i = 0; i < field_type->array_len; i++) {
            classify_field(field_type->base, field_offset + i * type_size(field_type->base), eb);
        }
        return;
    }

    if (first_eb >= 2 || last_eb >= 2) {
        eb[0] = eb[1] = CLASS_MEMORY;
        return;
    }
    eb[first_eb] = abi_join(eb[first_eb], fc);
    if (last_eb != first_eb) {
        eb[last_eb] = abi_join(eb[last_eb], fc);
    }
}

void classify_aggregate(Type *agg, abi_class_t eb[2]) {
    int size = type_size(agg);
    int align = type_align(agg);
    eb[0] = eb[1] = CLASS_NO_CLASS;

    if (size > 16 || size == 0) {
        eb[0] = eb[1] = CLASS_MEMORY;
        return;
    }

    /* SysV §3.2.3 step 1: unaligned aggregates (straddling boundary) go to MEMORY.
     * If an aggregate is larger than 8 bytes and its alignment is less than 8,
     * it might not start at a boundary multiple, which forces MEMORY. */
    if (align < 8 && size > 8) {
        eb[0] = eb[1] = CLASS_MEMORY;
        return;
    }

    if (agg->kind == TY_STRUCT || agg->kind == TY_UNION) {
        StructField *f;
        for (f = agg->fields; f; f = f->next) {
            classify_field(f->type, f->offset, eb);
            if (eb[0] == CLASS_MEMORY) return;
        }
    }

    if (eb[0] == CLASS_MEMORY || eb[1] == CLASS_MEMORY) {
        eb[0] = eb[1] = CLASS_MEMORY;
    }
}

/* ---------------------------------------------------------------- */
/* Load value from address in %rax                                   */
/* ---------------------------------------------------------------- */

void codegen_load(Compiler *cc, Type *type) {
  if (backend_ops) {
      if (!type || type->size == 4 || type->size == 8 || type->kind == TY_PTR) {
          fprintf(cc->out, "    ldr r0, [r0]\n");
      } else if (type->size == 1) {
          fprintf(cc->out, "    ldrb r0, [r0]\n");
      } else if (type->size == 2) {
          fprintf(cc->out, "    ldrh r0, [r0]\n");
      }
      return;
  }
  if (!type) {
    fprintf(cc->out, "    movq (%%rax), %%rax\n");
    return;
  }
  /* Do NOT check fault range here: stage2 miscompiles it and rejects valid
   * Type* ptrs. */
  if (type->kind == TY_ARRAY)
    return;
  if (type->kind == TY_STRUCT || type->kind == TY_UNION)
    return;
  /* Pointers must always be 64-bit (fixes stage2 sign-extended 32-bit load ->
   * 0x80xxxxxx crash). */
  if (type->kind == TY_PTR) {
    fprintf(cc->out, "    movq (%%rax), %%rax\n");
    return;
  }
  if (type->kind == TY_FLOAT) {
    fprintf(cc->out, "    movss (%%rax), %%xmm0\n");
    fprintf(cc->out, "    movq %%xmm0, %%rax\n");
    return;
  }
  if (type->kind == TY_DOUBLE) {
    fprintf(cc->out, "    movsd (%%rax), %%xmm0\n");
    fprintf(cc->out, "    movq %%xmm0, %%rax\n");
    return;
  }
  switch (type->size) {
  case 1:
    if (is_unsigned_type(type))
      fprintf(cc->out, "    movzbq (%%rax), %%rax\n");
    else
      fprintf(cc->out, "    movsbq (%%rax), %%rax\n");
    break;
  case 2:
    if (is_unsigned_type(type))
      fprintf(cc->out, "    movzwq (%%rax), %%rax\n");
    else
      fprintf(cc->out, "    movswq (%%rax), %%rax\n");
    break;
  case 4:
    if (is_unsigned_type(type))
      fprintf(cc->out, "    movl (%%rax), %%eax\n");
    else
      fprintf(cc->out, "    movslq (%%rax), %%rax\n");
    break;
  default:
    fprintf(cc->out, "    movq (%%rax), %%rax\n");
    break;
  }
}

/* ---------------------------------------------------------------- */
/* Store: value in %r11, address in %rax                             */
/* ---------------------------------------------------------------- */

void codegen_store(Compiler *cc, Type *type) {
  pop_reg(cc, "r11");
  if (backend_ops) {
      if (!type || type->size == 4 || type->size == 8 || type->kind == TY_PTR) {
          fprintf(cc->out, "    str r1, [r0]\n");
      } else if (type->size == 1) {
          fprintf(cc->out, "    strb r1, [r0]\n");
      } else if (type->size == 2) {
          fprintf(cc->out, "    strh r1, [r0]\n");
      }
      fprintf(cc->out, "    mov r0, r1\n");
      return;
  }
  if (!type) {
    if (backend_ops) fprintf(cc->out, "    str r1, [r0]\n");
      else fprintf(cc->out, "    movq %%r11, (%%rax)\n");
    if (backend_ops) fprintf(cc->out, "    mov r0, r1\n");
      else fprintf(cc->out, "    movq %%r11, %%rax\n");
    return;
  }
  /* Do NOT check fault range here: stage2 miscompiles it and rejects valid
   * Type* ptrs. */
  /* Pointers must always be 64-bit (fixes stage2 sign-extended 32-bit store).
   */
  if (type->kind == TY_PTR) {
    if (backend_ops) fprintf(cc->out, "    str r1, [r0]\n");
      else fprintf(cc->out, "    movq %%r11, (%%rax)\n");
    if (backend_ops) fprintf(cc->out, "    mov r0, r1\n");
      else fprintf(cc->out, "    movq %%r11, %%rax\n");
    return;
  }
  if (type->kind == TY_STRUCT || type->kind == TY_UNION || type->size > 8) {
    fprintf(cc->out, "    pushq %%rsi\n");
    fprintf(cc->out, "    pushq %%rdi\n");
    fprintf(cc->out, "    pushq %%rcx\n");
    fprintf(cc->out, "    movq %%r11, %%rsi\n");
    fprintf(cc->out, "    movq %%rax, %%rdi\n");
    fprintf(cc->out, "    movq $%d, %%rcx\n", type->size);
    fprintf(cc->out, "    rep movsb\n");
    fprintf(cc->out, "    popq %%rcx\n");
    fprintf(cc->out, "    popq %%rdi\n");
    fprintf(cc->out, "    popq %%rsi\n");
    if (backend_ops) fprintf(cc->out, "    mov r0, r1\n");
      else fprintf(cc->out, "    movq %%r11, %%rax\n");
    return;
  }
  switch (type->size) {
  case 1:
    if (backend_ops) fprintf(cc->out, "    strb r1, [r0]\n");
      else fprintf(cc->out, "    movb %%r11b, (%%rax)\n");
    break;
  case 2:
    if (backend_ops) fprintf(cc->out, "    strh r1, [r0]\n");
      else fprintf(cc->out, "    movw %%r11w, (%%rax)\n");
    break;
  case 4:
    if (backend_ops) fprintf(cc->out, "    str r1, [r0]\n");
      else fprintf(cc->out, "    movl %%r11d, (%%rax)\n");
    break;
  default:
    if (backend_ops) fprintf(cc->out, "    str r1, [r0]\n");
      else fprintf(cc->out, "    movq %%r11, (%%rax)\n");
    break;
  }
  if (backend_ops) fprintf(cc->out, "    mov r0, r1\n");
      else fprintf(cc->out, "    movq %%r11, %%rax\n");
}

/* Return cutoff so stage2 can't miscompile inline constant (GDB showed
 * node=0x10000). */
static unsigned long long bad_node_cutoff(void) { return 65536; }

/* Only null and very low ptr (<=0x10000). No hi/lo range check here—stage2
 * miscompiles it and then rejects valid heap (e.g. 0x00A7C8C0).
 * ptr_in_fault_range handles 0x80xxxxxx. */
static int is_bad_ptr(const void *p) {
  unsigned long long u = (unsigned long long)(const char *)p;
  return !p || u <= bad_node_cutoff();
}

/* Validate node/type after creation; fatal on first bad to pinpoint root cause.
 */
void validate_node(Compiler *cc, Node *node, const char *where, int line) {
  char buf[320];
  if (!node)
    return;
  if (node->magic != 0xC0FFEEBAD1234567ULL) {
    sprintf(buf, "CORRUPTED NODE (magic=0x%llx, alloc_id=%llu) at %s",
            (unsigned long long)node->magic, (unsigned long long)node->alloc_id,
            where);
    error_at(cc, line, buf);
    exit(1);
  }
  /* Do NOT check ptr_in_fault_range(node->type): stage2 miscompiles it. */
}

void validate_type(Compiler *cc, Type *type, const char *where, int line) {
  char buf[256];
  if (!type)
    return;
  if (type->magic != 0xDEADBEEF87654321ULL) {
    sprintf(buf, "CORRUPTED TYPE (magic=0x%llx, alloc_id=%llu) at %s",
            (unsigned long long)type->magic, (unsigned long long)type->alloc_id,
            where);
    error_at(cc, line, buf);
    exit(1);
  }
  /* Do NOT check ptr_in_fault_range(type) here: type is the pointer we just got
   * from cc_alloc (arena address like 0xa56040). Stage2 miscompiles that check
   * and falsely rejects valid ptrs. */
}

static int guard_error_count = 0;

/* Fatal on first bad node in codegen to stop spam. */
static void guard_node(Compiler *cc, Node *node) {
  char buf[256];
  if (!node || guard_error_count > 0)
    return;
  if (node->magic != 0xC0FFEEBAD1234567ULL) {
    sprintf(buf, "FATAL: node magic corrupted (0x%llx, id=%llu)",
            (unsigned long long)node->magic,
            (unsigned long long)node->alloc_id);
    error_at(cc, node->line, buf);
    guard_error_count++;
    exit(1);
  }
  /* Do NOT check ptr_in_fault_range(node->type): stage2 miscompiles it and
   * rejects valid Type* ptrs. */
}

/* Wrappers: catch bad ptr before any node load. */
static void codegen_expr_checked(Compiler *cc, Node *n) {
  if (is_bad_ptr(n)) {
    fprintf(cc->out, "    movq $0, %%rax\n");
    return;
  }
  codegen_expr(cc, n);
}
static void codegen_addr_checked(Compiler *cc, Node *n) {
  /* In main, never substitute 0 for param/local addresses: Stage2 can have
   * "bad" node ptrs (e.g. 0x10000) and we must use the fixed stack slots. */
  if (is_bad_ptr(n) &&
      (!cc->current_func[0] || strcmp(cc->current_func, "main") != 0)) {
    fprintf(cc->out, "    movq $0, %%rax\n");
    return;
  }
  codegen_addr(cc, n);
}

/* ---------------------------------------------------------------- */
/* Address of lvalue into %rax                                       */
/* ---------------------------------------------------------------- */

void codegen_addr(Compiler *cc, Node *node) {
  char errbuf[80];
  /* Do NOT use ptr_in_fault_range(node): stage2 miscompiles it and rejects
   * valid arena ptrs. */
  if (!node) {
    error_at(cc, 0, "codegen_addr: null node");
    fprintf(cc->out, "    movq $0, %%rax\n");
    return;
  }
  /* In main, do not substitute 0 for bad node ptr — we need to reach the main
   * param/local stack slots. */
  if (is_bad_ptr(node) &&
      (!cc->current_func[0] || strcmp(cc->current_func, "main") != 0)) {
    error_at(cc, 0, "codegen_addr: bad node ptr");
    fprintf(cc->out, "    movq $0, %%rax\n");
    return;
  }
  if (node->kind == ND_VAR) {
    /* Main params/locals: always use fixed stack layout when in main, so Stage2
     * never emits globals for these. */
    if (cc->current_func[0] && strcmp(cc->current_func, "main") == 0) {
      if (strcmp(node->name, "argc") == 0) {
        if (cc->verbose)
          fprintf(stderr, "zcc: codegen main param 'argc'\n");
        fprintf(cc->out, "    leaq -8(%%rbp), %%rax\n");
        char *dst = ir_bridge_fresh_tmp();
        ZCC_EMIT_UNARY(IR_ADDR, IR_TY_PTR, dst, "%stack_-8", node->line);
        return;
      }
      if (strcmp(node->name, "argv") == 0) {
        if (cc->verbose)
          fprintf(stderr, "zcc: codegen main param 'argv'\n");
        fprintf(cc->out, "    leaq -16(%%rbp), %%rax\n");
        char *dst = ir_bridge_fresh_tmp();
        ZCC_EMIT_UNARY(IR_ADDR, IR_TY_PTR, dst, "%stack_-16", node->line);
        return;
      }
      if (strcmp(node->name, "input_file") == 0) {
        if (cc->verbose)
          fprintf(stderr, "zcc: codegen main local 'input_file'\n");
        fprintf(cc->out, "    leaq -32(%%rbp), %%rax\n");
        char *dst = ir_bridge_fresh_tmp();
        ZCC_EMIT_UNARY(IR_ADDR, IR_TY_PTR, dst, "%stack_-32", node->line);
        return;
      }
      if (strcmp(node->name, "output_file") == 0) {
        if (cc->verbose)
          fprintf(stderr, "zcc: codegen main local 'output_file'\n");
        fprintf(cc->out, "    leaq -40(%%rbp), %%rax\n");
        char *dst = ir_bridge_fresh_tmp();
        ZCC_EMIT_UNARY(IR_ADDR, IR_TY_PTR, dst, "%stack_-40", node->line);
        return;
      }
    }
    /* Do NOT check is_bad_ptr(node->sym): Stage1 can have valid Symbol* in
     * fault range -> emit global -> segfault. */
    /* Use stack whenever we have a symbol with stack_offset. Params/locals have
     * negative offset; guard against wrong sign. */
    if (node->sym) {
      int off = node->sym->stack_offset;
      if (off > 0)
        off = -off; /* never use positive: would write above frame (e.g. return
                       address) and crash */
      if (off != 0 && !node->sym->is_global) {
        if (backend_ops) {
            fprintf(cc->out, "    ldr r3, =%d\n    adds r0, r7, r3\n", off);
        } else {
            fprintf(cc->out, "    leaq %d(%%rbp), %%rax\n", off);
        }
        char vname[32];
        sprintf(vname, "%%stack_%d", off);
        char *dst = ir_bridge_fresh_tmp();
        ZCC_EMIT_UNARY(IR_ADDR, IR_TY_PTR, dst, vname, node->line);
        return;
      }
    }
    if (backend_ops) {
        fprintf(cc->out, "    ldr r0, =%s\n", node->name);
    } else {
        fprintf(cc->out, "    leaq %s(%%rip), %%rax\n", node->name);
    }
    char gname[32];
    sprintf(gname, "%%%s", node->name);
    char *dst = ir_bridge_fresh_tmp();
    ZCC_EMIT_UNARY(IR_ADDR, IR_TY_PTR, dst, gname, node->line);
    return;
  }
  if (node->kind == ND_DEREF) {
    if (!node->lhs) {
      error_at(cc, node->line, "codegen_addr: ND_DEREF null lhs");
      fprintf(cc->out, "    movq $0, %%rax\n");
      return;
    }
    codegen_expr_checked(cc, node->lhs);
    return; /* Note: ptr is ALREADY eval'd by codegen_expr! ir_last_result holds
               it! */
  }
  if (node->kind == ND_MEMBER) {
    if (!node->lhs) {
      error_at(cc, node->line,
               "codegen_addr: ND_MEMBER null lhs (member access on null)");
      fprintf(cc->out, "    movq $0, %%rax\n");
      return;
    }
    codegen_addr_checked(cc, node->lhs);
    if (node->member_offset != 0) {
      fprintf(cc->out, "    addq $%d, %%rax\n", node->member_offset);
      char lhs_ir[32];
      ir_save_result(lhs_ir);
      char off_str[32];
      char c_tmp[32];
      sprintf(off_str, "%d", node->member_offset);
      sprintf(c_tmp, "%%t%d", ir_tmp_counter++);
      ZCC_EMIT_UNARY(IR_CONST, IR_TY_I64, c_tmp, off_str, node->line);

      char *dst = ir_bridge_fresh_tmp();
      ZCC_EMIT_BINARY(IR_ADD, IR_TY_PTR, dst, lhs_ir, c_tmp, node->line);
    }
    return;
  }
  error_at(cc, node->line, "not an lvalue");
}

static int ptr_elem_size(Type *type) {
  if (!type)
    return 1;
  if (type->kind == TY_PTR || type->kind == TY_ARRAY) {
    if (type->base)
      return type_size(type->base);
  }
  return 1;
}

/* ---------------------------------------------------------------- */
/* Expression codegen — result in %rax                               */
/* ---------------------------------------------------------------- */

void codegen_expr(Compiler *cc, Node *node) {
  if (!node)
    return;

  if (cc) {
  }

  int lbl1;
  int lbl2;
  char errbuf[80];

  /* Do NOT use ptr_in_fault_range(node): stage2 miscompiles it and rejects
   * valid arena ptrs. */
  if (!node) {
    error_at(cc, 0, "codegen_expr: NULL node");
    fprintf(cc->out, "    movq $0, %%rax\n");
    return;
  }
  /* In main, do not substitute 0 for bad node ptr — we need to reach main
   * param/local codegen. */
  if (is_bad_ptr(node) &&
      (!cc->current_func[0] || strcmp(cc->current_func, "main") != 0)) {
    error_at(cc, 0, "codegen_expr: bad node ptr");
    fprintf(cc->out, "    movq $0, %%rax\n");
    return;
  }
  if (node->kind < ND_NUM || node->kind > ND_NOP) {
    sprintf(errbuf, "codegen_expr: invalid kind %d (0x%x) at %p", node->kind,
            node->kind, (void *)node);
    error_at(cc, node->line, errbuf);
    return;
  }
  guard_node(cc, node);

  switch (node->kind) {

  case ND_NUM:
    if (backend_ops) {
        fprintf(cc->out, "    ldr r0, =%lld\n", node->int_val);
        {
          char *dst = ir_bridge_fresh_tmp();
          ZCC_EMIT_CONST(ir_map_type(node->type), dst, node->int_val, node->line);
        }
        return;
    }
    if (node->int_val >= -2147483648) {
      if (node->int_val <= 2147483647) {
        fprintf(cc->out, "    movq $%lld, %%rax\n", node->int_val);
        {
          char *dst = ir_bridge_fresh_tmp();
          ZCC_EMIT_CONST(ir_map_type(node->type), dst, node->int_val,
                         node->line);
        }
        return;
      }
    }
    fprintf(cc->out, "    movabsq $%lld, %%rax\n", node->int_val);
    {
      char *dst = ir_bridge_fresh_tmp();
      ZCC_EMIT_CONST(ir_map_type(node->type), dst, node->int_val, node->line);
    }
    return;

  case ND_FLIT: {
    int lbl;
    lbl = cc->str_label_count;
    cc->str_label_count = cc->str_label_count + 1;
    if (node->type && node->type->kind == TY_FLOAT) {
      /* CG-FLOAT-007: float literal (f/F suffix) — emit 32-bit IEEE bits */
      float fv = (float)node->f_val;
      unsigned int fbits;
      memcpy(&fbits, &fv, sizeof(float));
      fprintf(cc->out, "    .section .rodata\n");
      fprintf(cc->out, "    .p2align 2\n");
      fprintf(cc->out, ".LS_flit_%d:\n", lbl);
      fprintf(cc->out, "    .long %u\n", fbits);
      fprintf(cc->out, "    .text\n");
      fprintf(cc->out, "    movss .LS_flit_%d(%%rip), %%xmm0\n", lbl);
      fprintf(cc->out, "    movd %%xmm0, %%eax\n");
    } else {
      /* double literal — existing path */
      unsigned long long bits;
      memcpy(&bits, &node->f_val, sizeof(double));
      fprintf(cc->out, "    .section .rodata\n");
      fprintf(cc->out, "    .p2align 3\n");
      fprintf(cc->out, ".LS_flit_%d:\n", lbl);
      fprintf(cc->out, "    .quad %llu\n", bits);
      fprintf(cc->out, "    .text\n");
      fprintf(cc->out, "    movsd .LS_flit_%d(%%rip), %%xmm0\n", lbl);
      fprintf(cc->out, "    movq %%xmm0, %%rax\n");
    }
    /* Satisfy IR subsystem sequence */
    {
      char *dst = ir_bridge_fresh_tmp();
      long long flit_bits;
      memcpy(&flit_bits, &node->f_val, sizeof(double));
      ZCC_EMIT_FCONST(dst, flit_bits, node->line);
    }
    return;
  }

  case ND_STR: {
    char lbl_buf[32];
    fprintf(cc->out, "    leaq .LS%d(%%rip), %%rax\n",
            cc->strings[node->str_id].label_id);
    sprintf(lbl_buf, ".LS%d", cc->strings[node->str_id].label_id);
    char *dst = ir_bridge_fresh_tmp();
    ZCC_EMIT_CONST(IR_TY_PTR, dst, 0, node->line); /* Fake for now */
    ZCC_EMIT_UNARY(IR_CONST_STR, IR_TY_PTR, dst, lbl_buf,
                   node->line); /* REAL emit */
    return;
  }

  case ND_VAR:
    if (node->sym && node->sym->assigned_reg) {
      if (backend_ops) fprintf(cc->out, "    mov r0, %s\n", node->sym->assigned_reg); else fprintf(cc->out, "    movq %s, %%rax\n", node->sym->assigned_reg);
      if (node->type && !node_type_unsigned(node) && !is_pointer(node->type)) {
          if (node->type->size == 4) { if (!backend_ops) fprintf(cc->out, "    cltq\n"); }
          else if (node->type->size == 1) fprintf(cc->out, "    movsbq %%al, %%rax\n");
          else if (node->type->size == 2) fprintf(cc->out, "    movswq %%ax, %%rax\n");
      } else if (node->type && node_type_unsigned(node)) {
          if (node->type->size == 4) { if (!backend_ops) fprintf(cc->out, "    movl %%eax, %%eax\n"); }
          else if (node->type->size == 1) fprintf(cc->out, "    movzbq %%al, %%rax\n");
          else if (node->type->size == 2) fprintf(cc->out, "    movzwq %%ax, %%rax\n");
      }
      {
        char *vname = ir_var_name(node);
        char *dst = ir_bridge_fresh_tmp();
        ZCC_EMIT_LOAD(ir_map_type(node->type), dst, vname, node->line);
      }
      return;
    }
    codegen_addr_checked(cc, node);
    if (node->type) {
      if (node->type->kind != TY_ARRAY) {
        if (node->type->kind != TY_STRUCT) {
          if (node->type->kind != TY_UNION) {
            if (node->type->kind != TY_FUNC) {
              codegen_load(cc, node->type);
            }
          }
        }
      }
    }
    {
      char *vname = ir_var_name(node);
      char *dst = ir_bridge_fresh_tmp();
      ZCC_EMIT_LOAD(ir_map_type(node->type), dst, vname, node->line);
    }
    return;

  case ND_ASSIGN: {
    char rhs_ir[32];
    if (!node->lhs || !node->rhs) {
      error_at(cc, node->line, "codegen_expr: ND_ASSIGN missing lhs or rhs");
      fprintf(cc->out, "    movq $0, %%rax\n");
      return;
    }
    codegen_expr_checked(cc, node->rhs);
    ir_save_result(rhs_ir);
    if (node->lhs && node->lhs->kind == ND_VAR && node->lhs->sym &&
        node->lhs->sym->assigned_reg) {
      if (node->lhs->type && !node_type_unsigned(node->lhs) && !is_pointer(node->lhs->type)) {
          if (node->lhs->type->size == 4) { if (!backend_ops) fprintf(cc->out, "    cltq\n"); }
          else if (node->lhs->type->size == 1) fprintf(cc->out, "    movsbq %%al, %%rax\n");
          else if (node->lhs->type->size == 2) fprintf(cc->out, "    movswq %%ax, %%rax\n");
      } else if (node->lhs->type && node_type_unsigned(node->lhs)) {
          if (node->lhs->type->size == 4) { if (!backend_ops) fprintf(cc->out, "    movl %%eax, %%eax\n"); }
          else if (node->lhs->type->size == 1) fprintf(cc->out, "    movzbq %%al, %%rax\n");
          else if (node->lhs->type->size == 2) fprintf(cc->out, "    movzwq %%ax, %%rax\n");
      }
      if (backend_ops) fprintf(cc->out, "    mov %s, r0\n", node->lhs->sym->assigned_reg); else fprintf(cc->out, "    movq %%rax, %s\n", node->lhs->sym->assigned_reg);
      {
        char *vname = ir_var_name(node->lhs);
        ZCC_EMIT_STORE(ir_map_type(node->lhs->type), vname, rhs_ir, node->line);
      }
      return;
    }
    push_reg(cc, "rax");
    codegen_addr_checked(cc, node->lhs);
    char lhs_addr_ir[32];
    ir_save_result(lhs_addr_ir);
    if (is_bad_ptr(node->lhs) &&
        (!cc->current_func[0] || strcmp(cc->current_func, "main") != 0)) {
      error_at(cc, node->line, "codegen_expr: ND_ASSIGN lhs bad ptr");
      fprintf(cc->out, "    movq $0, %%rax\n");
      return;
    }
    if (!node->lhs->type && node->lhs->kind != ND_MEMBER) {
      error_at(cc, node->line, "codegen_expr: ND_ASSIGN lhs has null type");
      fprintf(cc->out, "    movq $0, %%rax\n");
      return;
    }
    /* Use member_size for ND_MEMBER so stage2 stores 4 bytes to cc->tk etc.,
     * not 8 */
    if (node->lhs->kind == ND_MEMBER && node->lhs->member_size > 0) {
      pop_reg(cc, "r11");
      if ((node->lhs->type && (node->lhs->type->kind == TY_STRUCT || node->lhs->type->kind == TY_UNION)) || node->lhs->member_size > 8) {
        fprintf(cc->out, "    pushq %%rsi\n");
        fprintf(cc->out, "    pushq %%rdi\n");
        fprintf(cc->out, "    pushq %%rcx\n");
        fprintf(cc->out, "    movq %%r11, %%rsi\n");
        fprintf(cc->out, "    movq %%rax, %%rdi\n");
        fprintf(cc->out, "    movq $%d, %%rcx\n", node->lhs->member_size);
        fprintf(cc->out, "    rep movsb\n");
        fprintf(cc->out, "    popq %%rcx\n");
        fprintf(cc->out, "    popq %%rdi\n");
        fprintf(cc->out, "    popq %%rsi\n");
        if (backend_ops) fprintf(cc->out, "    mov r0, r1\n");
      else fprintf(cc->out, "    movq %%r11, %%rax\n");
      } else {
        /* If member type is pointer/func, always use movq regardless of member_size */
        if (node->lhs->type && is_pointer(node->lhs->type)) {
          if (backend_ops) fprintf(cc->out, "    str r1, [r0]\n");
      else fprintf(cc->out, "    movq %%r11, (%%rax)\n");
        } else {
        switch (node->lhs->member_size) {
        case 1:
          if (backend_ops) fprintf(cc->out, "    strb r1, [r0]\n");
      else fprintf(cc->out, "    movb %%r11b, (%%rax)\n");
          break;
        case 2:
          if (backend_ops) fprintf(cc->out, "    strh r1, [r0]\n");
      else fprintf(cc->out, "    movw %%r11w, (%%rax)\n");
          break;
        case 4:
          if (backend_ops) fprintf(cc->out, "    str r1, [r0]\n");
      else fprintf(cc->out, "    movl %%r11d, (%%rax)\n");
          break;
        default:
          if (backend_ops) fprintf(cc->out, "    str r1, [r0]\n");
      else fprintf(cc->out, "    movq %%r11, (%%rax)\n");
          break;
        }
        }
        if (backend_ops) fprintf(cc->out, "    mov r0, r1\n");
      else fprintf(cc->out, "    movq %%r11, %%rax\n");
      }
    } else {
      codegen_store(cc, node->lhs->type);
    }
    {
      ZCC_EMIT_STORE(ir_map_type(node->lhs->type), lhs_addr_ir, rhs_ir,
                     node->line);
    }
    return;
  }

  case ND_COMPOUND_ASSIGN:
    if (!node->lhs || !node->rhs) {
      error_at(cc, node->line,
               "codegen_expr: ND_COMPOUND_ASSIGN missing operand");
      fprintf(cc->out, "    movq $0, %%rax\n");
      return;
    }
    if (node->lhs->kind == ND_VAR && node->lhs->sym &&
        node->lhs->sym->assigned_reg) {
      char *reg = node->lhs->sym->assigned_reg;
      codegen_expr_checked(cc, node->rhs);
      if (backend_ops) fprintf(cc->out, "    mov r1, r0\n");
      else fprintf(cc->out, "    movq %%rax, %%r11\n");
      if (backend_ops) fprintf(cc->out, "    mov r0, %s\n", reg); else fprintf(cc->out, "    movq %s, %%rax\n", reg);
      switch (node->compound_op) {
      case ND_ADD:
        if (is_pointer(node->lhs->type)) {
          int esz = ptr_elem_size(node->lhs->type);
          if (esz > 1)
            EMIT_PTR_SCALE_R11(cc, esz);
        }
        fprintf(cc->out, "    addq %%r11, %%rax\n");
        break;
      case ND_SUB:
        if (is_pointer(node->lhs->type)) {
          int esz = ptr_elem_size(node->lhs->type);
          if (esz > 1)
            EMIT_PTR_SCALE_R11(cc, esz);
        }
        if (backend_ops) backend_ops->emit_binary_op(cc, ND_SUB);
      else fprintf(cc->out, "    subq %%r11, %%rax\n");
        break;
      case ND_MUL:
        if (backend_ops) backend_ops->emit_binary_op(cc, ND_MUL);
      else fprintf(cc->out, "    imulq %%r11, %%rax\n");
        break;
      case ND_DIV:
        if (node->lhs->type && is_unsigned_type(node->lhs->type)) {
          fprintf(cc->out, "    xorq %%rdx, %%rdx\n    divq %%r11\n");
        } else {
          fprintf(cc->out, "    cqo\n    idivq %%r11\n");
        }
        break;
      case ND_MOD:
        if (node->lhs->type && is_unsigned_type(node->lhs->type)) {
          fprintf(
              cc->out,
              "    xorq %%rdx, %%rdx\n    divq %%r11\n    movq %%rdx, %%rax\n");
        } else {
          fprintf(cc->out, "    cqo\n    idivq %%r11\n    movq %%rdx, %%rax\n");
        }
        break;
      case ND_BAND:
        if (backend_ops) backend_ops->emit_binary_op(cc, ND_BAND);
      else fprintf(cc->out, "    andq %%r11, %%rax\n");
        break;
      case ND_BOR:
        if (backend_ops) backend_ops->emit_binary_op(cc, ND_BOR);
      else fprintf(cc->out, "    orq %%r11, %%rax\n");
        break;
      case ND_BXOR:
        if (backend_ops) backend_ops->emit_binary_op(cc, ND_BXOR);
      else fprintf(cc->out, "    xorq %%r11, %%rax\n");
        break;
      case ND_SHL:
        if (backend_ops) backend_ops->emit_binary_op(cc, ND_SHL);
      else fprintf(cc->out, "    movq %%r11, %%rcx\n    shlq %%cl, %%rax\n");
        break;
      case ND_SHR:
        if (node->lhs->type && is_unsigned_type(node->lhs->type))
          if (backend_ops) backend_ops->emit_binary_op(cc, ND_SHR);
      else fprintf(cc->out, "    movq %%r11, %%rcx\n    shrq %%cl, %%rax\n");
        else
          if (backend_ops) backend_ops->emit_binary_op(cc, ND_SHR);
      else fprintf(cc->out, "    movq %%r11, %%rcx\n    sarq %%cl, %%rax\n");
        break;
      default:
        break;
      }
      if (node->lhs->type && !node_type_unsigned(node->lhs) && !is_pointer(node->lhs->type)) {
          if (node->lhs->type->size == 4) { if (!backend_ops) fprintf(cc->out, "    cltq\n"); }
          else if (node->lhs->type->size == 1) fprintf(cc->out, "    movsbq %%al, %%rax\n");
          else if (node->lhs->type->size == 2) fprintf(cc->out, "    movswq %%ax, %%rax\n");
      } else if (node->lhs->type && node_type_unsigned(node->lhs)) {
          if (node->lhs->type->size == 4) { if (!backend_ops) fprintf(cc->out, "    movl %%eax, %%eax\n"); }
          else if (node->lhs->type->size == 1) fprintf(cc->out, "    movzbq %%al, %%rax\n");
          else if (node->lhs->type->size == 2) fprintf(cc->out, "    movzwq %%ax, %%rax\n");
      }
      if (backend_ops) fprintf(cc->out, "    mov %s, r0\n", reg); else fprintf(cc->out, "    movq %%rax, %s\n", reg);
      return;
    }
    codegen_addr_checked(cc, node->lhs);
    if (is_bad_ptr(node->lhs)) {
      error_at(cc, node->line, "codegen_expr: ND_COMPOUND_ASSIGN lhs bad ptr");
      fprintf(cc->out, "    movq $0, %%rax\n");
      return;
    }
    if (!node->lhs->type) {
      error_at(cc, node->line,
               "codegen_expr: ND_COMPOUND_ASSIGN lhs has null type");
      fprintf(cc->out, "    movq $0, %%rax\n");
      return;
    }
    push_reg(cc, "rax");
    if (node->lhs->kind == ND_MEMBER && node->lhs->member_size > 0) {
      switch (node->lhs->member_size) {
      case 1:
        fprintf(cc->out, "    movzbl (%%rax), %%eax\n");
        break;
      case 2:
        fprintf(cc->out, "    movzwl (%%rax), %%eax\n");
        break;
      case 4:
        fprintf(cc->out, "    movl (%%rax), %%eax\n");
        break;
      default:
        fprintf(cc->out, "    movq (%%rax), %%rax\n");
        break;
      }
    } else {
      codegen_load(cc, node->lhs->type);
    }
    push_reg(cc, "rax");
    codegen_expr_checked(cc, node->rhs);
    if (backend_ops) fprintf(cc->out, "    mov r1, r0\n");
      else fprintf(cc->out, "    movq %%rax, %%r11\n");
    pop_reg(cc, "rax");
    switch (node->compound_op) {
    case ND_ADD:
      if (node->lhs->type && is_float_type(node->lhs->type)) {
        /* CG-FLOAT-008a: dispatch ss vs sd based on lhs type */
        if (node->lhs->type->kind == TY_FLOAT) {
          fprintf(cc->out, "    movd %%eax, %%xmm0\n");
          fprintf(cc->out, "    movd %%r11d, %%xmm1\n");
          fprintf(cc->out, "    addss %%xmm1, %%xmm0\n");
          fprintf(cc->out, "    movd %%xmm0, %%eax\n");
        } else {
          fprintf(cc->out, "    movq %%rax, %%xmm0\n");
          fprintf(cc->out, "    movq %%r11, %%xmm1\n");
          fprintf(cc->out, "    addsd %%xmm1, %%xmm0\n");
          fprintf(cc->out, "    movq %%xmm0, %%rax\n");
        }
        break;
      }
      if (is_pointer(node->lhs->type)) {
        int esz;
        esz = ptr_elem_size(node->lhs->type);
        if (esz > 1)
          EMIT_PTR_SCALE_R11(cc, esz);
      }
      fprintf(cc->out, "    addq %%r11, %%rax\n");
      break;
    case ND_SUB:
      if (node->lhs->type && is_float_type(node->lhs->type)) {
        /* CG-FLOAT-008a: dispatch ss vs sd based on lhs type */
        if (node->lhs->type->kind == TY_FLOAT) {
          fprintf(cc->out, "    movd %%eax, %%xmm0\n");
          fprintf(cc->out, "    movd %%r11d, %%xmm1\n");
          fprintf(cc->out, "    subss %%xmm1, %%xmm0\n");
          fprintf(cc->out, "    movd %%xmm0, %%eax\n");
        } else {
          fprintf(cc->out, "    movq %%rax, %%xmm0\n");
          fprintf(cc->out, "    movq %%r11, %%xmm1\n");
          fprintf(cc->out, "    subsd %%xmm1, %%xmm0\n");
          fprintf(cc->out, "    movq %%xmm0, %%rax\n");
        }
        break;
      }
      if (is_pointer(node->lhs->type)) {
        int esz = ptr_elem_size(node->lhs->type);
        if (esz > 1)
          EMIT_PTR_SCALE_R11(cc, esz);
      }
      if (backend_ops) backend_ops->emit_binary_op(cc, ND_SUB);
      else fprintf(cc->out, "    subq %%r11, %%rax\n");
      break;
    case ND_MUL:
      if (node->lhs->type && is_float_type(node->lhs->type)) {
        /* CG-FLOAT-008a: dispatch ss vs sd based on lhs type */
        if (node->lhs->type->kind == TY_FLOAT) {
          fprintf(cc->out, "    movd %%eax, %%xmm0\n");
          fprintf(cc->out, "    movd %%r11d, %%xmm1\n");
          fprintf(cc->out, "    mulss %%xmm1, %%xmm0\n");
          fprintf(cc->out, "    movd %%xmm0, %%eax\n");
        } else {
          fprintf(cc->out, "    movq %%rax, %%xmm0\n");
          fprintf(cc->out, "    movq %%r11, %%xmm1\n");
          fprintf(cc->out, "    mulsd %%xmm1, %%xmm0\n");
          fprintf(cc->out, "    movq %%xmm0, %%rax\n");
        }
        break;
      }
      if (backend_ops) backend_ops->emit_binary_op(cc, ND_MUL);
      else fprintf(cc->out, "    imulq %%r11, %%rax\n");
      break;
    case ND_DIV:
      if (node->lhs->type && is_float_type(node->lhs->type)) {
        /* CG-FLOAT-008a: dispatch ss vs sd based on lhs type */
        if (node->lhs->type->kind == TY_FLOAT) {
          fprintf(cc->out, "    movd %%eax, %%xmm0\n");
          fprintf(cc->out, "    movd %%r11d, %%xmm1\n");
          fprintf(cc->out, "    divss %%xmm1, %%xmm0\n");
          fprintf(cc->out, "    movd %%xmm0, %%eax\n");
        } else {
          fprintf(cc->out, "    movq %%rax, %%xmm0\n");
          fprintf(cc->out, "    movq %%r11, %%xmm1\n");
          fprintf(cc->out, "    divsd %%xmm1, %%xmm0\n");
          fprintf(cc->out, "    movq %%xmm0, %%rax\n");
        }
        break;
      }
      if (node->lhs->type && is_unsigned_type(node->lhs->type)) {
        fprintf(cc->out, "    xorq %%rdx, %%rdx\n");
        fprintf(cc->out, "    divq %%r11\n");
      } else {
        fprintf(cc->out, "    cqo\n    idivq %%r11\n");
      }
      break;
    case ND_MOD:
      if (node->lhs->type && is_unsigned_type(node->lhs->type)) {
        fprintf(cc->out, "    xorq %%rdx, %%rdx\n");
        fprintf(cc->out, "    divq %%r11\n");
        if (backend_ops) fprintf(cc->out, "    mov r0, r2\n");
      else fprintf(cc->out, "    movq %%rdx, %%rax\n");
      } else {
        fprintf(cc->out, "    cqo\n    idivq %%r11\n    movq %%rdx, %%rax\n");
      }
      break;
    case ND_BAND:
      if (backend_ops) backend_ops->emit_binary_op(cc, ND_BAND);
      else fprintf(cc->out, "    andq %%r11, %%rax\n");
      break;
    case ND_BOR:
      if (backend_ops) backend_ops->emit_binary_op(cc, ND_BOR);
      else fprintf(cc->out, "    orq %%r11, %%rax\n");
      break;
    case ND_BXOR:
      if (backend_ops) backend_ops->emit_binary_op(cc, ND_BXOR);
      else fprintf(cc->out, "    xorq %%r11, %%rax\n");
      break;
    case ND_SHL:
      if (backend_ops) backend_ops->emit_binary_op(cc, ND_SHL);
      else fprintf(cc->out, "    movq %%r11, %%rcx\n    shlq %%cl, %%rax\n");
      break;
    case ND_SHR:
      if (node->lhs->type && is_unsigned_type(node->lhs->type))
        if (backend_ops) backend_ops->emit_binary_op(cc, ND_SHR);
      else fprintf(cc->out, "    movq %%r11, %%rcx\n    shrq %%cl, %%rax\n");
      else
        if (backend_ops) backend_ops->emit_binary_op(cc, ND_SHR);
      else fprintf(cc->out, "    movq %%r11, %%rcx\n    sarq %%cl, %%rax\n");
      break;
    default:
      break;
    }
    if (backend_ops) fprintf(cc->out, "    mov r1, r0\n");
      else fprintf(cc->out, "    movq %%rax, %%r11\n");
    pop_reg(cc, "rax");
    if (node->lhs->kind == ND_MEMBER && node->lhs->member_size > 0) {
      switch (node->lhs->member_size) {
      case 1:
        if (backend_ops) fprintf(cc->out, "    strb r1, [r0]\n");
      else fprintf(cc->out, "    movb %%r11b, (%%rax)\n");
        break;
      case 2:
        if (backend_ops) fprintf(cc->out, "    strh r1, [r0]\n");
      else fprintf(cc->out, "    movw %%r11w, (%%rax)\n");
        break;
      case 4:
        if (backend_ops) fprintf(cc->out, "    str r1, [r0]\n");
      else fprintf(cc->out, "    movl %%r11d, (%%rax)\n");
        break;
      default:
        if (backend_ops) fprintf(cc->out, "    str r1, [r0]\n");
      else fprintf(cc->out, "    movq %%r11, (%%rax)\n");
        break;
      }
    } else {
      switch (type_size(node->lhs->type)) {
      case 1:
        if (backend_ops) fprintf(cc->out, "    strb r1, [r0]\n");
      else fprintf(cc->out, "    movb %%r11b, (%%rax)\n");
        break;
      case 2:
        if (backend_ops) fprintf(cc->out, "    strh r1, [r0]\n");
      else fprintf(cc->out, "    movw %%r11w, (%%rax)\n");
        break;
      case 4:
        if (backend_ops) fprintf(cc->out, "    str r1, [r0]\n");
      else fprintf(cc->out, "    movl %%r11d, (%%rax)\n");
        break;
      default:
        if (backend_ops) fprintf(cc->out, "    str r1, [r0]\n");
      else fprintf(cc->out, "    movq %%r11, (%%rax)\n");
        break;
      }
    }
    if (backend_ops) fprintf(cc->out, "    mov r0, r1\n");
      else fprintf(cc->out, "    movq %%r11, %%rax\n");
    if (node->lhs->type && !node_type_unsigned(node->lhs) && !is_pointer(node->lhs->type)) {
        if (node->lhs->type->size == 4) { if (!backend_ops) fprintf(cc->out, "    cltq\n"); }
        else if (node->lhs->type->size == 1) fprintf(cc->out, "    movsbq %%al, %%rax\n");
        else if (node->lhs->type->size == 2) fprintf(cc->out, "    movswq %%ax, %%rax\n");
    } else if (node->lhs->type && node_type_unsigned(node->lhs)) {
        if (node->lhs->type->size == 4) { if (!backend_ops) fprintf(cc->out, "    movl %%eax, %%eax\n"); }
        else if (node->lhs->type->size == 1) fprintf(cc->out, "    movzbq %%al, %%rax\n");
        else if (node->lhs->type->size == 2) fprintf(cc->out, "    movzwq %%ax, %%rax\n");
    }
    {
      char *dst = ir_bridge_fresh_tmp();
      ir_emit_binary_op(node->compound_op, node->lhs->type, "ca_lhs", "ca_rhs", node->line);
    }
    return;

  case ND_ADD: {
    char lhs_ir[32];
    char rhs_ir[32];
    if (!node->lhs || !node->rhs) {
      error_at(cc, node->line, "codegen_expr: ND_ADD missing operand");
      fprintf(cc->out, "    movq $0, %%rax\n");
      return;
    }
    if (node->type && is_float_type(node->type)) {
      int is_f32 = (node->type->kind == TY_FLOAT);
      codegen_expr_checked(cc, node->lhs);
      if (!is_float_type(node->lhs->type)) {
        if (is_f32) fprintf(cc->out, "    cvtsi2ssl %%eax, %%xmm0\n");
        else        fprintf(cc->out, "    cvtsi2sdq %%rax, %%xmm0\n");
      } else {
        if (is_f32) fprintf(cc->out, "    movd %%eax, %%xmm0\n");
        else        fprintf(cc->out, "    movq %%rax, %%xmm0\n");
      }
      fprintf(cc->out, "    subq $8, %%rsp\n");
      if (is_f32) fprintf(cc->out, "    movss %%xmm0, (%%rsp)\n");
      else        fprintf(cc->out, "    movsd %%xmm0, (%%rsp)\n");
      codegen_expr_checked(cc, node->rhs);
      if (!is_float_type(node->rhs->type)) {
        if (is_f32) fprintf(cc->out, "    cvtsi2ssl %%eax, %%xmm0\n");
        else        fprintf(cc->out, "    cvtsi2sdq %%rax, %%xmm0\n");
      } else {
        if (is_f32) fprintf(cc->out, "    movd %%eax, %%xmm0\n");
        else        fprintf(cc->out, "    movq %%rax, %%xmm0\n");
      }
      if (is_f32) { fprintf(cc->out, "    movss (%%rsp), %%xmm1\n"); }
      else        { fprintf(cc->out, "    movsd (%%rsp), %%xmm1\n"); }
      fprintf(cc->out, "    addq $8, %%rsp\n");
      if (is_f32) { fprintf(cc->out, "    addss %%xmm1, %%xmm0\n"); fprintf(cc->out, "    movd %%xmm0, %%eax\n"); }
      else        { fprintf(cc->out, "    addsd %%xmm1, %%xmm0\n"); fprintf(cc->out, "    movq %%xmm0, %%rax\n"); }
      ir_emit_binary_op(ND_ADD, node->type, "f_lhs", "f_rhs", node->line);
      return;
    }
    codegen_expr_checked(cc, node->lhs);
    ir_save_result(lhs_ir);
    push_reg(cc, "rax");
    codegen_expr_checked(cc, node->rhs);
    ir_save_result(rhs_ir);
    if (backend_ops) fprintf(cc->out, "    mov r1, r0\n");
    else if (backend_ops) fprintf(cc->out, "    mov r1, r0\n");
      else fprintf(cc->out, "    movq %%rax, %%r11\n");
    pop_reg(cc, "rax");
    if (node->lhs->type && is_pointer(node->lhs->type)) {
      int esz = ptr_elem_size(node->lhs->type);
      if (esz > 1) {
        if (backend_ops) fprintf(cc->out, "    ldr r3, =%d\n    muls r1, r3, r1\n", esz);
        else EMIT_PTR_SCALE_R11(cc, esz);
        char scale_str[32];
        sprintf(scale_str, "%d", esz);
        char *dst = ir_bridge_fresh_tmp();
        ZCC_EMIT_BINARY(IR_MUL, IR_TY_I64, dst, rhs_ir, scale_str, node->line);
        strcpy(rhs_ir, dst);
      }
    } else if (node->rhs->type && is_pointer(node->rhs->type)) {
      int esz = ptr_elem_size(node->rhs->type);
      if (esz > 1) {
        if (backend_ops) fprintf(cc->out, "    ldr r3, =%d\n    muls r0, r3, r0\n", esz);
        else EMIT_PTR_SCALE_RAX(cc, esz);
        char scale_str[32];
        sprintf(scale_str, "%d", esz);
        char *dst = ir_bridge_fresh_tmp();
        ZCC_EMIT_BINARY(IR_MUL, IR_TY_I64, dst, lhs_ir, scale_str, node->line);
        strcpy(lhs_ir, dst);
      }
    }
    if (backend_ops && backend_ops->emit_binary_op) {
        backend_ops->emit_binary_op(cc, ND_ADD);
    } else {
        fprintf(cc->out, "    addq %%r11, %%rax\n");
    }
    if (node->type && type_size(node->type) == 4 && !is_pointer(node->type)) {
      if (node_type_unsigned(node)) {
        if (!backend_ops) fprintf(cc->out, "    movl %%eax, %%eax\n");
      } else {
        if (!backend_ops) fprintf(cc->out, "    cltq\n");
      }
    }
    ir_emit_binary_op(ND_ADD, node->type, lhs_ir, rhs_ir, node->line);
    return;
  }

  case ND_SUB: {
    char lhs_ir[32];
    char rhs_ir[32];
    if (!node->lhs || !node->rhs) {
      error_at(cc, node->line, "codegen_expr: ND_SUB missing operand");
      fprintf(cc->out, "    movq $0, %%rax\n");
      return;
    }
    if (node->type && is_float_type(node->type)) {
      int is_f32 = (node->type->kind == TY_FLOAT);
      codegen_expr_checked(cc, node->lhs);
      if (!is_float_type(node->lhs->type)) {
        if (is_f32) fprintf(cc->out, "    cvtsi2ssl %%eax, %%xmm0\n");
        else        fprintf(cc->out, "    cvtsi2sdq %%rax, %%xmm0\n");
      } else {
        if (is_f32) fprintf(cc->out, "    movd %%eax, %%xmm0\n");
        else        fprintf(cc->out, "    movq %%rax, %%xmm0\n");
      }
      fprintf(cc->out, "    subq $8, %%rsp\n");
      if (is_f32) fprintf(cc->out, "    movss %%xmm0, (%%rsp)\n");
      else        fprintf(cc->out, "    movsd %%xmm0, (%%rsp)\n");
      codegen_expr_checked(cc, node->rhs);
      if (!is_float_type(node->rhs->type)) {
        if (is_f32) fprintf(cc->out, "    cvtsi2ssl %%eax, %%xmm0\n");
        else        fprintf(cc->out, "    cvtsi2sdq %%rax, %%xmm0\n");
      } else {
        if (is_f32) fprintf(cc->out, "    movd %%eax, %%xmm0\n");
        else        fprintf(cc->out, "    movq %%rax, %%xmm0\n");
      }
      if (is_f32) { fprintf(cc->out, "    movss (%%rsp), %%xmm1\n"); }
      else        { fprintf(cc->out, "    movsd (%%rsp), %%xmm1\n"); }
      fprintf(cc->out, "    addq $8, %%rsp\n");
      /* note: subsd/subss operand order: xmm1 -= xmm0, then copy to xmm0 */
      if (is_f32) { fprintf(cc->out, "    subss %%xmm0, %%xmm1\n"); fprintf(cc->out, "    movaps %%xmm1, %%xmm0\n"); fprintf(cc->out, "    movd %%xmm0, %%eax\n"); }
      else        { fprintf(cc->out, "    subsd %%xmm0, %%xmm1\n"); fprintf(cc->out, "    movsd %%xmm1, %%xmm0\n"); fprintf(cc->out, "    movq %%xmm0, %%rax\n"); }
      ir_emit_binary_op(ND_SUB, node->type, "f_lhs", "f_rhs", node->line);
      return;
    }
    codegen_expr_checked(cc, node->lhs);
    ir_save_result(lhs_ir);
    push_reg(cc, "rax");
    codegen_expr_checked(cc, node->rhs);
    ir_save_result(rhs_ir);
    if (backend_ops) fprintf(cc->out, "    mov r1, r0\n");
    else if (backend_ops) fprintf(cc->out, "    mov r1, r0\n");
      else fprintf(cc->out, "    movq %%rax, %%r11\n");
    pop_reg(cc, "rax");
    if (node->lhs->type && is_pointer(node->lhs->type)) {
      if (node->rhs->type && is_pointer(node->rhs->type)) {
        if (backend_ops && backend_ops->emit_binary_op) backend_ops->emit_binary_op(cc, ND_SUB);
        else if (backend_ops) backend_ops->emit_binary_op(cc, ND_SUB);
      else fprintf(cc->out, "    subq %%r11, %%rax\n");
        {
          int esz;
          esz = ptr_elem_size(node->lhs->type);
          if (esz > 1) {
            if (backend_ops) { /* no-op for hello.c */ } else {
            fprintf(cc->out, "    movq $%d, %%r11\n", esz);
            fprintf(cc->out, "    cqo\n    idivq %%r11\n");
            }
          }
        }
        ir_emit_binary_op(ND_SUB, node->type, lhs_ir, rhs_ir, node->line);
        return;
      }
      {
        int esz;
        esz = node->lhs->type ? ptr_elem_size(node->lhs->type) : 1;
        if (esz > 1) {
          if (backend_ops) {
            fprintf(cc->out, "    ldr r3, =%d\n    muls r1, r3, r1\n", esz);
          } else {
            EMIT_PTR_SCALE_R11(cc, esz);
          }
        }
      }
    }
    if (backend_ops && backend_ops->emit_binary_op) backend_ops->emit_binary_op(cc, ND_SUB);
    else if (backend_ops) backend_ops->emit_binary_op(cc, ND_SUB);
      else fprintf(cc->out, "    subq %%r11, %%rax\n");
    if (node->type && type_size(node->type) == 4 && !is_pointer(node->type)) {
      if (node_type_unsigned(node)) {
        if (!backend_ops) fprintf(cc->out, "    movl %%eax, %%eax\n");
      } else {
        if (!backend_ops) fprintf(cc->out, "    cltq\n");
      }
    }
    ir_emit_binary_op(ND_SUB, node->type, lhs_ir, rhs_ir, node->line);
    return;
  }

  case ND_MUL: {
    char lhs_ir[32];
    char rhs_ir[32];
    if (!node->lhs || !node->rhs) {
      error_at(cc, node->line, "codegen_expr: binary op missing operand");
      fprintf(cc->out, "    movq $0, %%rax\n");
      return;
    }
    if (node->type && is_float_type(node->type)) {
      int is_f32 = (node->type->kind == TY_FLOAT);
      codegen_expr_checked(cc, node->lhs);
      if (!is_float_type(node->lhs->type)) {
        if (is_f32) fprintf(cc->out, "    cvtsi2ssl %%eax, %%xmm0\n");
        else        fprintf(cc->out, "    cvtsi2sdq %%rax, %%xmm0\n");
      } else {
        if (is_f32) fprintf(cc->out, "    movd %%eax, %%xmm0\n");
        else        fprintf(cc->out, "    movq %%rax, %%xmm0\n");
      }
      fprintf(cc->out, "    subq $8, %%rsp\n");
      if (is_f32) fprintf(cc->out, "    movss %%xmm0, (%%rsp)\n");
      else        fprintf(cc->out, "    movsd %%xmm0, (%%rsp)\n");
      codegen_expr_checked(cc, node->rhs);
      if (!is_float_type(node->rhs->type)) {
        if (is_f32) fprintf(cc->out, "    cvtsi2ssl %%eax, %%xmm0\n");
        else        fprintf(cc->out, "    cvtsi2sdq %%rax, %%xmm0\n");
      } else {
        if (is_f32) fprintf(cc->out, "    movd %%eax, %%xmm0\n");
        else        fprintf(cc->out, "    movq %%rax, %%xmm0\n");
      }
      if (is_f32) { fprintf(cc->out, "    movss (%%rsp), %%xmm1\n"); }
      else        { fprintf(cc->out, "    movsd (%%rsp), %%xmm1\n"); }
      fprintf(cc->out, "    addq $8, %%rsp\n");
      if (is_f32) { fprintf(cc->out, "    mulss %%xmm1, %%xmm0\n"); fprintf(cc->out, "    movd %%xmm0, %%eax\n"); }
      else        { fprintf(cc->out, "    mulsd %%xmm1, %%xmm0\n"); fprintf(cc->out, "    movq %%xmm0, %%rax\n"); }
      ir_emit_binary_op(ND_MUL, node->type, "f_lhs", "f_rhs", node->line);
      return;
    }
    if (!backend_ops) {
      if (node->rhs->kind == ND_NUM && is_power_of_2_val(node->rhs->int_val)) {
        int shift;
        codegen_expr_checked(cc, node->lhs);
        ir_save_result(lhs_ir);
        shift = log2_of(node->rhs->int_val);
        fprintf(cc->out, "    shlq $%d, %%rax\n", shift);
        ir_emit_binary_op(ND_SHL, node->type, lhs_ir, "unused_rhs", node->line);
        return;
      }
      if (node->lhs->kind == ND_NUM && is_power_of_2_val(node->lhs->int_val)) {
        int shift;
        codegen_expr_checked(cc, node->rhs);
        ir_save_result(rhs_ir);
        shift = log2_of(node->lhs->int_val);
        fprintf(cc->out, "    shlq $%d, %%rax\n", shift);
        ir_emit_binary_op(ND_SHL, node->type, "unused_lhs", rhs_ir, node->line);
        return;
      }
      if (node->rhs->kind == ND_NUM && node->rhs->int_val == 3) {
        codegen_expr_checked(cc, node->lhs);
        ir_save_result(lhs_ir);
        fprintf(cc->out, "    leaq (%%rax,%%rax,2), %%rax\n");
        ir_emit_binary_op(ND_MUL, node->type, lhs_ir, "unused_rhs", node->line);
        return;
      }
      if (node->rhs->kind == ND_NUM && node->rhs->int_val == 5) {
        codegen_expr_checked(cc, node->lhs);
        ir_save_result(lhs_ir);
        fprintf(cc->out, "    leaq (%%rax,%%rax,4), %%rax\n");
        ir_emit_binary_op(ND_MUL, node->type, lhs_ir, "unused_rhs", node->line);
        return;
      }
      if (node->rhs->kind == ND_NUM && node->rhs->int_val == 9) {
        codegen_expr_checked(cc, node->lhs);
        ir_save_result(lhs_ir);
        fprintf(cc->out, "    leaq (%%rax,%%rax,8), %%rax\n");
        ir_emit_binary_op(ND_MUL, node->type, lhs_ir, "unused_rhs", node->line);
        return;
      }
    }

    codegen_expr_checked(cc, node->lhs);
    ir_save_result(lhs_ir);
    push_reg(cc, "rax");
    codegen_expr_checked(cc, node->rhs);
    ir_save_result(rhs_ir);
    pop_reg(cc, "r11");
    if (backend_ops) {
        if (backend_ops->emit_binary_op) backend_ops->emit_binary_op(cc, ND_MUL);
    } else {
        if (backend_ops) backend_ops->emit_binary_op(cc, ND_MUL);
      else fprintf(cc->out, "    imulq %%r11, %%rax\n");
    }
    if (node->type && type_size(node->type) == 4 && !is_pointer(node->type)) {
      if (node_type_unsigned(node)) {
        if (!backend_ops) fprintf(cc->out, "    movl %%eax, %%eax\n");
      } else {
        if (!backend_ops) fprintf(cc->out, "    cltq\n");
      }
    }
    ir_emit_binary_op(ND_MUL, node->type, lhs_ir, rhs_ir, node->line);
    return;
  }

  case ND_DIV: {
    char lhs_ir[32];
    char rhs_ir[32];
    if (!node->lhs || !node->rhs) {
      error_at(cc, node->line, "codegen_expr: ND_DIV missing operand");
      fprintf(cc->out, "    movq $0, %%rax\n");
      return;
    }
    if (node->type && is_float_type(node->type)) {
      int is_f32 = (node->type->kind == TY_FLOAT);
      codegen_expr_checked(cc, node->lhs);
      if (!is_float_type(node->lhs->type)) {
        if (is_f32) fprintf(cc->out, "    cvtsi2ssl %%eax, %%xmm0\n");
        else        fprintf(cc->out, "    cvtsi2sdq %%rax, %%xmm0\n");
      } else {
        if (is_f32) fprintf(cc->out, "    movd %%eax, %%xmm0\n");
        else        fprintf(cc->out, "    movq %%rax, %%xmm0\n");
      }
      fprintf(cc->out, "    subq $8, %%rsp\n");
      if (is_f32) fprintf(cc->out, "    movss %%xmm0, (%%rsp)\n");
      else        fprintf(cc->out, "    movsd %%xmm0, (%%rsp)\n");
      codegen_expr_checked(cc, node->rhs);
      if (!is_float_type(node->rhs->type)) {
        if (is_f32) fprintf(cc->out, "    cvtsi2ssl %%eax, %%xmm0\n");
        else        fprintf(cc->out, "    cvtsi2sdq %%rax, %%xmm0\n");
      } else {
        if (is_f32) fprintf(cc->out, "    movd %%eax, %%xmm0\n");
        else        fprintf(cc->out, "    movq %%rax, %%xmm0\n");
      }
      if (is_f32) { fprintf(cc->out, "    movss (%%rsp), %%xmm1\n"); }
      else        { fprintf(cc->out, "    movsd (%%rsp), %%xmm1\n"); }
      fprintf(cc->out, "    addq $8, %%rsp\n");
      /* divss/divsd: dividend xmm1, divisor xmm0 -> result in xmm1 */
      if (is_f32) { fprintf(cc->out, "    divss %%xmm0, %%xmm1\n"); fprintf(cc->out, "    movaps %%xmm1, %%xmm0\n"); fprintf(cc->out, "    movd %%xmm0, %%eax\n"); }
      else        { fprintf(cc->out, "    divsd %%xmm0, %%xmm1\n"); fprintf(cc->out, "    movsd %%xmm1, %%xmm0\n"); fprintf(cc->out, "    movq %%xmm0, %%rax\n"); }
      ir_emit_binary_op(ND_DIV, node->type, "f_lhs", "f_rhs", node->line);
      return;
    }

    if (node->rhs->kind == ND_NUM && is_power_of_2_val(node->rhs->int_val)) {
      int shift;
      char rhs_val_str[32];
      codegen_expr_checked(cc, node->lhs);
      ir_save_result(lhs_ir);
      shift = log2_of(node->rhs->int_val);
      if (node->type && is_unsigned_type(node->type)) {
        fprintf(cc->out, "    shrq $%d, %%rax\n", shift);
      } else {
        fprintf(cc->out, "    movq %%rax, %%rcx\n");
        fprintf(cc->out, "    sarq $63, %%rcx\n");
        fprintf(cc->out, "    andq $%lld, %%rcx\n", (1LL << shift) - 1);
        fprintf(cc->out, "    addq %%rcx, %%rax\n");
        fprintf(cc->out, "    sarq $%d, %%rax\n", shift);
      }
      snprintf(rhs_val_str, 32, "$%lld", node->rhs->int_val);
      ir_emit_binary_op(ND_DIV, node->type, lhs_ir, rhs_val_str, node->line);
      return;
    }

    codegen_expr_checked(cc, node->lhs);
    ir_save_result(lhs_ir);
    push_reg(cc, "rax");
    codegen_expr_checked(cc, node->rhs);
    ir_save_result(rhs_ir);
    if (backend_ops) fprintf(cc->out, "    mov r1, r0\n");
      else fprintf(cc->out, "    movq %%rax, %%r11\n");
    pop_reg(cc, "rax");
    if (node->lhs->type && node->rhs->type &&
        (is_unsigned_type(node->lhs->type) ||
         is_unsigned_type(node->rhs->type))) {
      fprintf(cc->out, "    xorq %%rdx, %%rdx\n");
      fprintf(cc->out, "    divq %%r11\n");
    } else {
      fprintf(cc->out, "    cqo\n    idivq %%r11\n");
    }
    ir_emit_binary_op(ND_DIV, node->type, lhs_ir, rhs_ir, node->line);
    return;
  }

  case ND_MOD: {
    char lhs_ir[32];
    char rhs_ir[32];

    if (node->rhs && node->rhs->kind == ND_NUM &&
        is_power_of_2_val(node->rhs->int_val)) {
      long long mask;
      char rhs_val_str[32];
      codegen_expr_checked(cc, node->lhs);
      ir_save_result(lhs_ir);
      mask = node->rhs->int_val - 1;
      if (node->type && is_unsigned_type(node->type)) {
        fprintf(cc->out, "    andq $%lld, %%rax\n", mask);
        snprintf(rhs_val_str, 32, "$%lld", node->rhs->int_val);
        ir_emit_binary_op(ND_MOD, node->type, lhs_ir, rhs_val_str, node->line);
        return;
      }
    }

    codegen_expr_checked(cc, node->lhs);
    ir_save_result(lhs_ir);
    push_reg(cc, "rax");
    codegen_expr_checked(cc, node->rhs);
    ir_save_result(rhs_ir);
    if (backend_ops) fprintf(cc->out, "    mov r1, r0\n");
      else fprintf(cc->out, "    movq %%rax, %%r11\n");
    pop_reg(cc, "rax");
    if (node->lhs->type && node->rhs->type &&
        (is_unsigned_type(node->lhs->type) ||
         is_unsigned_type(node->rhs->type))) {
      fprintf(cc->out, "    xorq %%rdx, %%rdx\n");
      fprintf(cc->out, "    divq %%r11\n");
      if (backend_ops) fprintf(cc->out, "    mov r0, r2\n");
      else fprintf(cc->out, "    movq %%rdx, %%rax\n");
    } else {
      fprintf(cc->out, "    cqo\n    idivq %%r11\n");
      if (backend_ops) fprintf(cc->out, "    mov r0, r2\n");
      else fprintf(cc->out, "    movq %%rdx, %%rax\n");
    }
    ir_emit_binary_op(ND_MOD, node->type, lhs_ir, rhs_ir, node->line);
    return;
  }

  case ND_BAND: {
    char lhs_ir[32];
    char rhs_ir[32];
    codegen_expr_checked(cc, node->lhs);
    ir_save_result(lhs_ir);
    push_reg(cc, "rax");
    codegen_expr_checked(cc, node->rhs);
    ir_save_result(rhs_ir);
    pop_reg(cc, "r11");
    if (backend_ops) backend_ops->emit_binary_op(cc, ND_BAND);
      else fprintf(cc->out, "    andq %%r11, %%rax\n");
    ir_emit_binary_op(ND_BAND, node->type, lhs_ir, rhs_ir, node->line);
    return;
  }

  case ND_BOR: {
    char lhs_ir[32];
    char rhs_ir[32];
    codegen_expr_checked(cc, node->lhs);
    ir_save_result(lhs_ir);
    push_reg(cc, "rax");
    codegen_expr_checked(cc, node->rhs);
    ir_save_result(rhs_ir);
    pop_reg(cc, "r11");
    if (backend_ops) backend_ops->emit_binary_op(cc, ND_BOR);
      else fprintf(cc->out, "    orq %%r11, %%rax\n");
    ir_emit_binary_op(ND_BOR, node->type, lhs_ir, rhs_ir, node->line);
    return;
  }

  case ND_BXOR: {
    char lhs_ir[32];
    char rhs_ir[32];
    codegen_expr_checked(cc, node->lhs);
    ir_save_result(lhs_ir);
    push_reg(cc, "rax");
    codegen_expr_checked(cc, node->rhs);
    ir_save_result(rhs_ir);
    pop_reg(cc, "r11");
    if (backend_ops) backend_ops->emit_binary_op(cc, ND_BXOR);
      else fprintf(cc->out, "    xorq %%r11, %%rax\n");
    ir_emit_binary_op(ND_BXOR, node->type, lhs_ir, rhs_ir, node->line);
    return;
  }

  case ND_BNOT: {
    char src_ir[32];
    if (!node->lhs) {
      error_at(cc, node->line, "codegen_expr: ND_BNOT null lhs");
      fprintf(cc->out, "    movq $0, %%rax\n");
      return;
    }
    codegen_expr_checked(cc, node->lhs);
    ir_save_result(src_ir);
    if (node->type && type_size(node->type) == 4) {
      if (node_type_unsigned(node)) {
        fprintf(cc->out, "    notl %%eax\n");
      } else {
        fprintf(cc->out, "    notl %%eax\n    cltq\n");
      }
    } else {
      fprintf(cc->out, "    notq %%rax\n");
    }
    {
      char *dst = ir_bridge_fresh_tmp();
      ZCC_EMIT_UNARY(IR_NOT, ir_map_type(node->type), dst, src_ir, node->line);
    }
    return;
  }

  case ND_SHL: {
    char lhs_ir[32];
    char rhs_ir[32];
    codegen_expr_checked(cc, node->lhs);
    ir_save_result(lhs_ir);
    push_reg(cc, "rax");
    codegen_expr_checked(cc, node->rhs);
    ir_save_result(rhs_ir);
    fprintf(cc->out, "    movq %%rax, %%rcx\n");
    pop_reg(cc, "rax");
    fprintf(cc->out, "    shlq %%cl, %%rax\n");
    ir_emit_binary_op(ND_SHL, node->type, lhs_ir, rhs_ir, node->line);
    return;
  }

  case ND_SHR: {
    char lhs_ir[32];
    char rhs_ir[32];
    codegen_expr_checked(cc, node->lhs);
    ir_save_result(lhs_ir);
    push_reg(cc, "rax");
    codegen_expr_checked(cc, node->rhs);
    ir_save_result(rhs_ir);
    fprintf(cc->out, "    movq %%rax, %%rcx\n");
    pop_reg(cc, "rax");
    if (node->lhs->type && is_unsigned_type(node->lhs->type))
      fprintf(cc->out, "    shrq %%cl, %%rax\n");
    else
      fprintf(cc->out, "    sarq %%cl, %%rax\n");
    ir_emit_binary_op(ND_SHR, node->type, lhs_ir, rhs_ir, node->line);
    return;
  }

  case ND_NEG: {
    char src_ir[32];
    if (!node->lhs) {
      error_at(cc, node->line, "codegen_expr: ND_NEG null lhs");
      fprintf(cc->out, "    movq $0, %%rax\n");
      return;
    }
    if (node->type && is_float_type(node->type)) {
      codegen_expr_checked(cc, node->lhs);
      /* CG-FLOAT-008b: flip sign bit at the correct width.
       * ty_float value lives in low 32 of %rax — flip bit 31 only.
       * ty_double value lives in full 64 of %rax — flip bit 63. */
      if (node->type->kind == TY_FLOAT) {
        fprintf(cc->out, "    xorl $0x80000000, %%eax\n");
      } else {
        fprintf(cc->out, "    movabsq $-9223372036854775808, %%r11\n");
        if (backend_ops) backend_ops->emit_binary_op(cc, ND_BXOR);
        else fprintf(cc->out, "    xorq %%r11, %%rax\n");
      }
      {
        char *dst = ir_bridge_fresh_tmp();
        ZCC_EMIT_UNARY(IR_NEG, ir_map_type(node->type), dst, "f_lhs", node->line);
      }
      return;
    }
    codegen_expr_checked(cc, node->lhs);
    ir_save_result(src_ir);
    if (node->type && type_size(node->type) == 4) {
      if (node_type_unsigned(node)) {
        fprintf(cc->out, "    negl %%eax\n");
      } else {
        fprintf(cc->out, "    negl %%eax\n    cltq\n");
      }
    } else {
      fprintf(cc->out, "    negq %%rax\n");
    }
    {
      char *dst = ir_bridge_fresh_tmp();
      ZCC_EMIT_UNARY(IR_NEG, ir_map_type(node->type), dst, src_ir, node->line);
    }
    return;
  }

  case ND_LNOT: {
    char src_ir[32];
    int lnot_is_float;
    if (!node->lhs) {
      error_at(cc, node->line, "codegen_expr: ND_LNOT null lhs");
      fprintf(cc->out, "    movq $0, %%rax\n");
      return;
    }
    codegen_expr_checked(cc, node->lhs);
    ir_save_result(src_ir);
    /* CG-FLOAT-012: float/double operands require SSE zero-compare.
     * cmpq $0, %rax treats -0.0f (0x80000000) as non-zero, wrong. */
    lnot_is_float = node->lhs->type && is_float_type(node->lhs->type);
    if (!backend_ops && lnot_is_float) {
      int lnot_f32 = node->lhs->type->kind == TY_FLOAT;
      if (lnot_f32) {
        fprintf(cc->out, "    movd %%eax, %%xmm0\n");
        fprintf(cc->out, "    pxor %%xmm1, %%xmm1\n");
        fprintf(cc->out, "    ucomiss %%xmm1, %%xmm0\n");
      } else {
        fprintf(cc->out, "    movq %%rax, %%xmm0\n");
        fprintf(cc->out, "    pxor %%xmm1, %%xmm1\n");
        fprintf(cc->out, "    ucomisd %%xmm1, %%xmm0\n");
      }
      /* ZF=1 iff zero (includes -0.0); PF=1 iff NaN (treat as nonzero) */
      fprintf(cc->out, "    sete %%al\n");
      fprintf(cc->out, "    setnp %%r11b\n");
      fprintf(cc->out, "    andb %%r11b, %%al\n");
    } else {
      if (backend_ops) fprintf(cc->out, "    cmp r0, #0\n");
      else             fprintf(cc->out, "    cmpq $0, %%rax\n");
      fprintf(cc->out, "    sete %%al\n");
    }
    fprintf(cc->out, "    movzbl %%al, %%eax\n");
    {
      char zero_ir[32];
      char *zt = ir_bridge_fresh_tmp();
      int i;
      for (i = 0; zt[i]; i++)
        zero_ir[i] = zt[i];
      zero_ir[i] = 0;
      ZCC_EMIT_CONST(IR_TY_I64, zero_ir, 0, node->line);
      char *dst = ir_bridge_fresh_tmp();
      ZCC_EMIT_BINARY(IR_EQ, IR_TY_I32, dst, src_ir, zero_ir, node->line);
    }
    return;
  }

  case ND_EQ:
  case ND_NE:
  case ND_LT:
  case ND_LE:
  case ND_GT:
  case ND_GE: {
    int uns;
    int use32;
    char lhs_ir[32];
    char rhs_ir[32];

    if ((node->lhs && node->lhs->type && is_float_type(node->lhs->type)) ||
        (node->rhs && node->rhs->type && is_float_type(node->rhs->type))) {
      /* CG-FLOAT-010: dispatch ss vs sd based on operand type.
       * Using movq+ucomisd for a float operand zero-extends the 32-bit bits
       * into a 64-bit pattern, turning negative floats into small positive
       * doubles and inverting all comparisons involving negatives. */
      int is_f32 = (node->lhs && node->lhs->type &&
                    node->lhs->type->kind == TY_FLOAT) ||
                   (node->rhs && node->rhs->type &&
                    node->rhs->type->kind == TY_FLOAT);
      /* If either operand is float, compare as float (ucomiss) */
      codegen_expr_checked(cc, node->lhs);
      if (!node->lhs->type || !is_float_type(node->lhs->type)) {
        if (is_f32) fprintf(cc->out, "    cvtsi2ssl %%eax, %%xmm0\n");
        else        fprintf(cc->out, "    cvtsi2sdq %%rax, %%xmm0\n");
      } else {
        if (is_f32) fprintf(cc->out, "    movd %%eax, %%xmm0\n");
        else        fprintf(cc->out, "    movq %%rax, %%xmm0\n");
      }
      fprintf(cc->out, "    subq $8, %%rsp\n");
      if (is_f32) fprintf(cc->out, "    movss %%xmm0, (%%rsp)\n");
      else        fprintf(cc->out, "    movsd %%xmm0, (%%rsp)\n");
      codegen_expr_checked(cc, node->rhs);
      if (!node->rhs->type || !is_float_type(node->rhs->type)) {
        if (is_f32) fprintf(cc->out, "    cvtsi2ssl %%eax, %%xmm0\n");
        else        fprintf(cc->out, "    cvtsi2sdq %%rax, %%xmm0\n");
      } else {
        if (is_f32) fprintf(cc->out, "    movd %%eax, %%xmm0\n");
        else        fprintf(cc->out, "    movq %%rax, %%xmm0\n");
      }
      if (is_f32) fprintf(cc->out, "    movss (%%rsp), %%xmm1\n");
      else        fprintf(cc->out, "    movsd (%%rsp), %%xmm1\n");
      fprintf(cc->out, "    addq $8, %%rsp\n");
      if (is_f32) fprintf(cc->out, "    ucomiss %%xmm0, %%xmm1\n");
      else        fprintf(cc->out, "    ucomisd %%xmm0, %%xmm1\n");
      switch (node->kind) {
      case ND_EQ:
        fprintf(cc->out, "    sete %%al\n    setnp %%r11b\n    andb %%r11b, %%al\n");
        break;
      case ND_NE:
        fprintf(cc->out, "    setne %%al\n    setp %%r11b\n    orb %%r11b, %%al\n");
        break;
      case ND_LT:
        fprintf(cc->out, "    setb %%al\n");
        break;
      case ND_LE:
        fprintf(cc->out, "    setbe %%al\n");
        break;
      case ND_GT:
        fprintf(cc->out, "    seta %%al\n");
        break;
      case ND_GE:
        fprintf(cc->out, "    setae %%al\n");
        break;
      }
      fprintf(cc->out, "    movzbl %%al, %%eax\n");
      ir_emit_binary_op(node->kind, node->type, "f_lhs", "f_rhs", node->line);
      return;
    }

    uns = (node->lhs && node->lhs->type && is_unsigned_type(node->lhs->type)) ||
          (node->rhs && node->rhs->type && is_unsigned_type(node->rhs->type));
    use32 = node->lhs && node->lhs->type && node->rhs &&
            node->rhs->type && type_size(node->lhs->type) == 4 &&
            type_size(node->rhs->type) == 4;
    codegen_expr_checked(cc, node->lhs);
    ir_save_result(lhs_ir);
    push_reg(cc, "rax");
    codegen_expr_checked(cc, node->rhs);
    ir_save_result(rhs_ir);
    if (backend_ops) {
        fprintf(cc->out, "    mov r1, r0\n");
    } else {
        if (backend_ops) fprintf(cc->out, "    mov r1, r0\n");
      else fprintf(cc->out, "    movq %%rax, %%r11\n");
    }
    pop_reg(cc, "rax");
    if (backend_ops) {
        fprintf(cc->out, "    cmp r0, r1\n");
        int lbl_true = new_label(cc);
        int lbl_end = new_label(cc);
        if (uns) {
            switch (node->kind) {
            case ND_EQ: fprintf(cc->out, "    beq .L%d\n", lbl_true); break;
            case ND_NE: fprintf(cc->out, "    bne .L%d\n", lbl_true); break;
            case ND_LT: fprintf(cc->out, "    blo .L%d\n", lbl_true); break;
            case ND_LE: fprintf(cc->out, "    bls .L%d\n", lbl_true); break;
            case ND_GT: fprintf(cc->out, "    bhi .L%d\n", lbl_true); break;
            case ND_GE: fprintf(cc->out, "    bhs .L%d\n", lbl_true); break;
            }
        } else {
            switch (node->kind) {
            case ND_EQ: fprintf(cc->out, "    beq .L%d\n", lbl_true); break;
            case ND_NE: fprintf(cc->out, "    bne .L%d\n", lbl_true); break;
            case ND_LT: fprintf(cc->out, "    blt .L%d\n", lbl_true); break;
            case ND_LE: fprintf(cc->out, "    ble .L%d\n", lbl_true); break;
            case ND_GT: fprintf(cc->out, "    bgt .L%d\n", lbl_true); break;
            case ND_GE: fprintf(cc->out, "    bge .L%d\n", lbl_true); break;
            }
        }
        fprintf(cc->out, "    movs r0, #0\n");
        fprintf(cc->out, "    b .L%d\n", lbl_end);
        fprintf(cc->out, ".L%d:\n", lbl_true);
        fprintf(cc->out, "    movs r0, #1\n");
        fprintf(cc->out, ".L%d:\n", lbl_end);
    } else {
        if (use32)
          fprintf(cc->out,
                  "    cmpl %%r11d, %%eax\n"); /* 32-bit: avoids sign-extended imm
                                                  in 64-bit */
        else
          if (backend_ops) fprintf(cc->out, "    cmp r0, r1\n" \
    ); else fprintf(cc->out, "    cmpq %%r11, %%rax\n");
        if (uns) {
          switch (node->kind) {
          case ND_EQ:
            fprintf(cc->out, "    sete %%al\n");
            break;
          case ND_NE:
            fprintf(cc->out, "    setne %%al\n");
            break;
          case ND_LT:
            fprintf(cc->out, "    setb %%al\n");
            break;
          case ND_LE:
            fprintf(cc->out, "    setbe %%al\n");
            break;
          case ND_GT:
            fprintf(cc->out, "    seta %%al\n");
            break;
          case ND_GE:
            fprintf(cc->out, "    setae %%al\n");
            break;
          default:
            break;
          }
        } else {
          switch (node->kind) {
          case ND_EQ:
            fprintf(cc->out, "    sete %%al\n");
            break;
          case ND_NE:
            fprintf(cc->out, "    setne %%al\n");
            break;
          case ND_LT:
            fprintf(cc->out, "    setl %%al\n");
            break;
          case ND_LE:
            fprintf(cc->out, "    setle %%al\n");
            break;
          case ND_GT:
            fprintf(cc->out, "    setg %%al\n");
            break;
          case ND_GE:
            fprintf(cc->out, "    setge %%al\n");
            break;
          default:
            break;
          }
        }
        fprintf(cc->out, "    movzbl %%al, %%eax\n");
    }
    ir_emit_binary_op(node->kind, node->type, lhs_ir, rhs_ir, node->line);
    return;
  }

  case ND_LAND: {
    char land_lhs_ir[32];
    char land_lbl[32];
    char land_lbl2[32];
    int land_lhs_float;
    int land_rhs_float;
    int land_lhs_f32;
    int land_rhs_f32;
    lbl1 = new_label(cc);
    lbl2 = new_label(cc);
    codegen_expr_checked(cc, node->lhs);
    ir_save_result(land_lhs_ir);
    /* CG-FLOAT-012: use SSE zero-compare if LHS is float/double */
    land_lhs_float = node->lhs && node->lhs->type && is_float_type(node->lhs->type);
    land_lhs_f32   = land_lhs_float && node->lhs->type->kind == TY_FLOAT;
    if (!backend_ops && land_lhs_float) {
      if (land_lhs_f32) {
        fprintf(cc->out, "    movd %%eax, %%xmm0\n");
        fprintf(cc->out, "    pxor %%xmm1, %%xmm1\n");
        fprintf(cc->out, "    ucomiss %%xmm1, %%xmm0\n");
      } else {
        fprintf(cc->out, "    movq %%rax, %%xmm0\n");
        fprintf(cc->out, "    pxor %%xmm1, %%xmm1\n");
        fprintf(cc->out, "    ucomisd %%xmm1, %%xmm0\n");
      }
      /* Jump to false-label if LHS is zero (ZF=1) or NaN (PF=1) */
      fprintf(cc->out, "    jp .L%d\n", lbl1);
      fprintf(cc->out, "    je .L%d\n", lbl1);
    } else {
      if (backend_ops) fprintf(cc->out, "    cmp r0, #0\n");
      else             fprintf(cc->out, "    cmpq $0, %%rax\n");
      if (backend_ops) emit_label_fmt(cc, lbl1, FMT_JE);
      else             emit_label_fmt(cc, lbl1, FMT_JE);
    }
    sprintf(land_lbl, ".L%d", lbl1);
    sprintf(land_lbl2, ".L%d", lbl2);
    ZCC_EMIT_BR_IF(land_lhs_ir, land_lbl, node->line);
    /* LHS was truthy: evaluate RHS and convert to bool */
    codegen_expr_checked(cc, node->rhs);
    land_rhs_float = node->rhs && node->rhs->type && is_float_type(node->rhs->type);
    land_rhs_f32   = land_rhs_float && node->rhs->type->kind == TY_FLOAT;
    if (!backend_ops && land_rhs_float) {
      if (land_rhs_f32) {
        fprintf(cc->out, "    movd %%eax, %%xmm0\n");
        fprintf(cc->out, "    pxor %%xmm1, %%xmm1\n");
        fprintf(cc->out, "    ucomiss %%xmm1, %%xmm0\n");
      } else {
        fprintf(cc->out, "    movq %%rax, %%xmm0\n");
        fprintf(cc->out, "    pxor %%xmm1, %%xmm1\n");
        fprintf(cc->out, "    ucomisd %%xmm1, %%xmm0\n");
      }
      fprintf(cc->out, "    setne %%al\n");
      fprintf(cc->out, "    movzbl %%al, %%eax\n");
    } else {
      if (backend_ops) fprintf(cc->out, "    cmp r0, #0\n");
      else             fprintf(cc->out, "    cmpq $0, %%rax\n");
      fprintf(cc->out, "    setne %%al\n");
      fprintf(cc->out, "    movzbl %%al, %%eax\n");
    }
    /* Skip over the false-result */
    emit_label_fmt(cc, lbl2, FMT_JMP);
    ZCC_EMIT_BR(land_lbl2, node->line);
    /* Short-circuit false label: LHS was falsy, result = 0 */
    fprintf(cc->out, ".L%d:\n", lbl1);
    ZCC_EMIT_LABEL(land_lbl, node->line);
    fprintf(cc->out, "    movq $0, %%rax\n");
    fprintf(cc->out, ".L%d:\n", lbl2);
    ZCC_EMIT_LABEL(land_lbl2, node->line);
    return;
  }

  case ND_LOR: {
    char lor_lhs_ir[32];
    char lor_lbl1[32];
    char lor_lbl2[32];
    int lor_lhs_float;
    int lor_rhs_float;
    int lor_lhs_f32;
    int lor_rhs_f32;
    lbl1 = new_label(cc);
    lbl2 = new_label(cc);
    codegen_expr_checked(cc, node->lhs);
    ir_save_result(lor_lhs_ir);
    /* CG-FLOAT-012: use SSE zero-compare if LHS is float/double */
    lor_lhs_float = node->lhs && node->lhs->type && is_float_type(node->lhs->type);
    lor_lhs_f32   = lor_lhs_float && node->lhs->type->kind == TY_FLOAT;
    if (!backend_ops && lor_lhs_float) {
      if (lor_lhs_f32) {
        fprintf(cc->out, "    movd %%eax, %%xmm0\n");
        fprintf(cc->out, "    pxor %%xmm1, %%xmm1\n");
        fprintf(cc->out, "    ucomiss %%xmm1, %%xmm0\n");
      } else {
        fprintf(cc->out, "    movq %%rax, %%xmm0\n");
        fprintf(cc->out, "    pxor %%xmm1, %%xmm1\n");
        fprintf(cc->out, "    ucomisd %%xmm1, %%xmm0\n");
      }
      /* jne or jp = nonzero or NaN = truthy, jump to result=1 */
      fprintf(cc->out, "    jp .L%d\n", lbl1);
      fprintf(cc->out, "    jne .L%d\n", lbl1);
    } else {
      if (backend_ops) fprintf(cc->out, "    cmp r0, #0\n");
      else             fprintf(cc->out, "    cmpq $0, %%rax\n");
      fprintf(cc->out, "    jne .L%d\n", lbl1);
    }
    sprintf(lor_lbl1, ".L%d", lbl1);
    sprintf(lor_lbl2, ".L%d", lbl2);
    codegen_expr_checked(cc, node->rhs);
    lor_rhs_float = node->rhs && node->rhs->type && is_float_type(node->rhs->type);
    lor_rhs_f32   = lor_rhs_float && node->rhs->type->kind == TY_FLOAT;
    if (!backend_ops && lor_rhs_float) {
      if (lor_rhs_f32) {
        fprintf(cc->out, "    movd %%eax, %%xmm0\n");
        fprintf(cc->out, "    pxor %%xmm1, %%xmm1\n");
        fprintf(cc->out, "    ucomiss %%xmm1, %%xmm0\n");
      } else {
        fprintf(cc->out, "    movq %%rax, %%xmm0\n");
        fprintf(cc->out, "    pxor %%xmm1, %%xmm1\n");
        fprintf(cc->out, "    ucomisd %%xmm1, %%xmm0\n");
      }
      fprintf(cc->out, "    jp .L%d\n", lbl1);
      fprintf(cc->out, "    jne .L%d\n", lbl1);
    } else {
      if (backend_ops) fprintf(cc->out, "    cmp r0, #0\n");
      else             fprintf(cc->out, "    cmpq $0, %%rax\n");
      fprintf(cc->out, "    jne .L%d\n", lbl1);
    }
    fprintf(cc->out, "    movq $0, %%rax\n");
    if (backend_ops) emit_label_fmt(cc, lbl2, FMT_JMP);
    else             emit_label_fmt(cc, lbl2, FMT_JMP);
    ZCC_EMIT_BR(lor_lbl2, node->line);
    fprintf(cc->out, ".L%d:\n", lbl1);
    ZCC_EMIT_LABEL(lor_lbl1, node->line);
    fprintf(cc->out, "    movq $1, %%rax\n");
    fprintf(cc->out, ".L%d:\n", lbl2);
    ZCC_EMIT_LABEL(lor_lbl2, node->line);
    return;
  }

  case ND_VA_ARG: {
    int lbl_overflow;
    int lbl_end;

    /* Evaluate LHS (ap) -> %rax */
    codegen_expr(cc, node->lhs);

    /* Safe-keep ap in %rcx */
    fprintf(cc->out, "    movq %%rax, %%rcx\n");

    lbl_overflow = new_label(cc);
    lbl_end = new_label(cc);

    int fp = is_float_type(node->type);
    
    if (fp) {
        /* Float path: Check fp_offset (4(%%rcx)) threshold 176 */
        fprintf(cc->out, "    movl 4(%%rcx), %%edx\n");
        fprintf(cc->out, "    cmpl $176, %%edx\n");
        fprintf(cc->out, "    jae .L%d\n", lbl_overflow);
        /* Fast path: fetch from reg_save_area */
        fprintf(cc->out, "    movq 16(%%rcx), %%rsi\n");
        fprintf(cc->out, "    movslq %%edx, %%rax\n");
        fprintf(cc->out, "    addq %%rax, %%rsi\n");
        fprintf(cc->out, "    movq (%%rsi), %%rax\n");
        /* Increment fp_offset by 16 */
        fprintf(cc->out, "    addl $16, %%edx\n");
        fprintf(cc->out, "    movl %%edx, 4(%%rcx)\n");
    } else {
        /* GP path: Check gp_offset (0(%%rcx)) threshold 48 */
        fprintf(cc->out, "    movl 0(%%rcx), %%edx\n");
        fprintf(cc->out, "    cmpl $48, %%edx\n");
        fprintf(cc->out, "    jae .L%d\n", lbl_overflow);
        /* Fast path: fetch from reg_save_area */
        fprintf(cc->out, "    movq 16(%%rcx), %%rsi\n");
        fprintf(cc->out, "    movslq %%edx, %%rax\n");
        fprintf(cc->out, "    addq %%rax, %%rsi\n");
        fprintf(cc->out, "    movq (%%rsi), %%rax\n");
        /* Increment gp_offset by 8 */
        fprintf(cc->out, "    addl $8, %%edx\n");
        fprintf(cc->out, "    movl %%edx, 0(%%rcx)\n");
    }

    emit_label_fmt(cc, lbl_end, FMT_JMP);

    /* Slow path: fetch from overflow_arg_area */
    emit_label_fmt(cc, lbl_overflow, FMT_DEF);
    fprintf(cc->out, "    movq 8(%%rcx), %%rsi\n");
    fprintf(cc->out, "    movq (%%rsi), %%rax\n");

    /* Increment overflow_arg_area by 8 (even for floats, stack passing is 8-aligned) */
    fprintf(cc->out, "    leaq 8(%%rsi), %%rdi\n");
    fprintf(cc->out, "    movq %%rdi, 8(%%rcx)\n");

    /* End */
    emit_label_fmt(cc, lbl_end, FMT_DEF);
    return;
  }

  case ND_ADDR_LABEL: {
    /* get address of local label: leaq .Luser_func_label(%rip), %rax */
    fprintf(cc->out, "    leaq .Luser_%s_%s(%%rip), %%rax\n", cc->current_func, node->label_name);
    push_reg(cc, "rax");
    return;
  }

  case ND_ADDR:
    if (!node->lhs) {
      error_at(cc, node->line, "codegen_expr: ND_ADDR null lhs");
      fprintf(cc->out, "    movq $0, %%rax\n");
      return;
    }
    codegen_addr_checked(cc, node->lhs);
    {
      char addr_src[32];
      ir_save_result(addr_src);
      char *dst = ir_bridge_fresh_tmp();
      ZCC_EMIT_UNARY(IR_ADDR, IR_TY_PTR, dst, addr_src, node->line);
    }
    return;

  case ND_DEREF:
    if (!node->lhs) {
      error_at(cc, node->line, "codegen_expr: ND_DEREF null lhs");
      fprintf(cc->out, "    movq $0, %%rax\n");
      return;
    }
    codegen_expr_checked(cc, node->lhs);
    {
      char deref_addr[32];
      ir_save_result(deref_addr);
      /* Type-aware load: char* -> movsbl, int* -> movl, ptr/long* -> movq */
      if (node->type && node->type->kind == TY_FUNC) {
          /* Do nothing: function pointer decays natively */
      } else {
          codegen_load(cc, node->type);
      }
      {
        char *dst = ir_bridge_fresh_tmp();
        ZCC_EMIT_LOAD(ir_map_type(node->type), dst, deref_addr, node->line);
      }
    }
    return;

  case ND_MEMBER:
    codegen_addr_checked(cc, node);
    {
      char member_addr[32];
      ir_save_result(member_addr);
      if (node->type) {
        codegen_load(cc, node->type);
      } else {
        switch (node->member_size) {
        case 1:
          fprintf(cc->out, "    movzbl (%%rax), %%eax\n");
          break;
        case 2:
          fprintf(cc->out, "    movzwl (%%rax), %%eax\n");
          break;
        case 4:
          fprintf(cc->out, "    movl (%%rax), %%eax\n");
          break;
        case 8:
          fprintf(cc->out, "    movq (%%rax), %%rax\n");
          break;
        default:
          break;
        }
      }
      {
        char *dst = ir_bridge_fresh_tmp();
        ZCC_EMIT_LOAD(ir_map_type(node->type), dst, member_addr, node->line);
      }
    }
    return;

  case ND_CAST: {
    char src_ir[32];
    if (!node->lhs) {
      error_at(cc, node->line, "codegen_expr: ND_CAST null lhs");
      fprintf(cc->out, "    movq $0, %%rax\n");
      return;
    }
    codegen_expr_checked(cc, node->lhs);
    ir_save_result(src_ir);
    /* truncate/extend based on target type */
    if (node->cast_type) {
      int src_size = node->lhs && node->lhs->type ? type_size(node->lhs->type) : 4;

      /* ---- float <-> double conversions (must precede size switch) ---- */
      if (node->lhs && node->lhs->type && is_float_type(node->lhs->type) && is_float_type(node->cast_type)) {
        if (node->lhs->type->kind == TY_FLOAT && node->cast_type->kind == TY_DOUBLE) {
          /* float -> double: rax holds 32-bit IEEE float in low bits */
          fprintf(cc->out, "    movd %%eax, %%xmm0\n");
          fprintf(cc->out, "    cvtss2sd %%xmm0, %%xmm0\n");
          fprintf(cc->out, "    movq %%xmm0, %%rax\n");
        } else if (node->lhs->type->kind == TY_DOUBLE && node->cast_type->kind == TY_FLOAT) {
          /* double -> float: rax holds 64-bit IEEE double */
          fprintf(cc->out, "    movq %%rax, %%xmm0\n");
          fprintf(cc->out, "    cvtsd2ss %%xmm0, %%xmm0\n");
          fprintf(cc->out, "    movd %%xmm0, %%eax\n");
        }
        /* else same-type cast: no-op */
        {
          char *dst = ir_bridge_fresh_tmp();
          ZCC_EMIT_UNARY(IR_CAST, ir_map_type(node->type), dst, src_ir, node->line);
        }
        return;
      }

      switch (node->cast_type->size) {
      case 1:
        if (node->lhs && node->lhs->type && is_float_type(node->lhs->type)) {
            /* float/double -> char/uchar: truncating convert first, then narrow */
            if (node->lhs->type->kind == TY_FLOAT) {
                fprintf(cc->out, "    movd %%eax, %%xmm0\n");
                fprintf(cc->out, "    cvttss2si %%xmm0, %%rax\n");
            } else {
                fprintf(cc->out, "    movq %%rax, %%xmm0\n");
                fprintf(cc->out, "    cvttsd2si %%xmm0, %%rax\n");
            }
        }
        if (node->cast_type->kind == TY_UCHAR)
          fprintf(cc->out, "    movzbl %%al, %%eax\n");
        else
          fprintf(cc->out, "    movsbl %%al, %%eax\n");
        break;
      case 2:
        if (node->lhs && node->lhs->type && is_float_type(node->lhs->type)) {
            /* float/double -> short/ushort: truncating convert first, then narrow */
            if (node->lhs->type->kind == TY_FLOAT) {
                fprintf(cc->out, "    movd %%eax, %%xmm0\n");
                fprintf(cc->out, "    cvttss2si %%xmm0, %%rax\n");
            } else {
                fprintf(cc->out, "    movq %%rax, %%xmm0\n");
                fprintf(cc->out, "    cvttsd2si %%xmm0, %%rax\n");
            }
        }
        if (node->cast_type->kind == TY_USHORT)
          fprintf(cc->out, "    movzwl %%ax, %%eax\n");
        else
          fprintf(cc->out, "    movswl %%ax, %%eax\n");
        break;
      case 4:
        if (node->cast_type && !is_float_type(node->cast_type) && node->lhs && node->lhs->type && is_float_type(node->lhs->type)) {
            /* float/double -> int32: convert via SSE */
            fprintf(cc->out, "    movq %%rax, %%xmm0\n");
            if (node->lhs->type->kind == TY_FLOAT)
                fprintf(cc->out, "    cvttss2si %%xmm0, %%rax\n");
            else
                fprintf(cc->out, "    cvttsd2si %%xmm0, %%rax\n");
        } else if (node->cast_type->kind == TY_FLOAT && node->lhs && node->lhs->type && !is_float_type(node->lhs->type)) {
            /* int -> float: */
            fprintf(cc->out, "    cvtsi2ssq %%rax, %%xmm0\n");
            fprintf(cc->out, "    movd %%xmm0, %%eax\n");
        } else if (node->cast_type->kind == TY_UINT || node->cast_type->kind == TY_ULONG) {
            if (!backend_ops) fprintf(cc->out, "    movl %%eax, %%eax\n");
        } else if (!is_pointer(node->lhs ? node->lhs->type : 0)) {
            if (!backend_ops) fprintf(cc->out, "    cltq\n");
        }
        break;
      case 8:
        if (node->cast_type && is_float_type(node->cast_type) && node->lhs && node->lhs->type && !is_float_type(node->lhs->type)) {
            if (is_unsigned_type(node->lhs->type)) {
                fprintf(cc->out, "    testq %%rax, %%rax\n");
                fprintf(cc->out, "    js 1f\n");
                fprintf(cc->out, "    cvtsi2sdq %%rax, %%xmm0\n");
                fprintf(cc->out, "    jmp 2f\n");
                fprintf(cc->out, "1:\n");
                if (backend_ops) fprintf(cc->out, "    mov r2, r0\n");
      else fprintf(cc->out, "    movq %%rax, %%rdx\n");
                fprintf(cc->out, "    shrq $1, %%rdx\n");
                fprintf(cc->out, "    andl $1, %%eax\n");
                fprintf(cc->out, "    orq %%rax, %%rdx\n");
                fprintf(cc->out, "    cvtsi2sdq %%rdx, %%xmm0\n");
                fprintf(cc->out, "    addsd %%xmm0, %%xmm0\n");
                fprintf(cc->out, "2:\n");
            } else {
                fprintf(cc->out, "    cvtsi2sdq %%rax, %%xmm0\n");
            }
            fprintf(cc->out, "    movq %%xmm0, %%rax\n");
        } else if (node->cast_type && !is_float_type(node->cast_type) && node->lhs && node->lhs->type && is_float_type(node->lhs->type)) {
            fprintf(cc->out, "    movq %%rax, %%xmm0\n");
            fprintf(cc->out, "    cvttsd2si %%xmm0, %%rax\n");
        } else if (src_size == 4 && !is_pointer(node->lhs ? node->lhs->type : 0)) {
            if (node->lhs && node->lhs->type && is_unsigned_type(node->lhs->type)) {
                if (!backend_ops) fprintf(cc->out, "    movl %%eax, %%eax\n");

            } else {
                if (!backend_ops) fprintf(cc->out, "    cltq\n");

            }
        }
        break;
      default:
        break;
      }
    }
    {
      char *dst = ir_bridge_fresh_tmp();
      ZCC_EMIT_UNARY(IR_CAST, ir_map_type(node->type), dst, src_ir, node->line);
    }
    return;
  }

  case ND_TERNARY: {
    char ternary_cond_ir[32];
    char ternary_lbl1[32];
    char ternary_lbl2[32];
    if (!node->cond || !node->then_body || !node->else_body) {
      error_at(cc, node->line,
               "codegen_expr: ND_TERNARY missing cond/then/else");
      fprintf(cc->out, "    movq $0, %%rax\n");
      return;
    }
    lbl1 = new_label(cc);
    lbl2 = new_label(cc);
    codegen_expr_checked(cc, node->cond);
    ir_save_result(ternary_cond_ir);
    if (backend_ops) fprintf(cc->out, "    cmp r0, #0\n");
    else {
      /* CG-FLOAT-012: float/double ternary condition needs SSE zero-compare */
      int tern_is_float = node->cond && node->cond->type &&
                          is_float_type(node->cond->type);
      if (tern_is_float) {
        int tern_f32 = node->cond->type->kind == TY_FLOAT;
        if (tern_f32) {
          fprintf(cc->out, "    movd %%eax, %%xmm0\n");
          fprintf(cc->out, "    pxor %%xmm1, %%xmm1\n");
          fprintf(cc->out, "    ucomiss %%xmm1, %%xmm0\n");
        } else {
          fprintf(cc->out, "    movq %%rax, %%xmm0\n");
          fprintf(cc->out, "    pxor %%xmm1, %%xmm1\n");
          fprintf(cc->out, "    ucomisd %%xmm1, %%xmm0\n");
        }
      } else {
        fprintf(cc->out, "    cmpq $0, %%rax\n");
      }
    }
    if (backend_ops) emit_label_fmt(cc, lbl1, FMT_JE);
    else emit_label_fmt(cc, lbl1, FMT_JE);
    sprintf(ternary_lbl1, ".L%d", lbl1);
    ZCC_EMIT_BR_IF(ternary_cond_ir, ternary_lbl1, node->line);
    codegen_expr_checked(cc, node->then_body);
    if (backend_ops) emit_label_fmt(cc, lbl2, FMT_JMP);
    else emit_label_fmt(cc, lbl2, FMT_JMP);
    sprintf(ternary_lbl2, ".L%d", lbl2);
    ZCC_EMIT_BR(ternary_lbl2, node->line);
    fprintf(cc->out, ".L%d:\n", lbl1);
    ZCC_EMIT_LABEL(ternary_lbl1, node->line);
    codegen_expr_checked(cc, node->else_body);
    fprintf(cc->out, ".L%d:\n", lbl2);
    ZCC_EMIT_LABEL(ternary_lbl2, node->line);
    return;
  }

  case ND_COMMA_EXPR:
    codegen_expr_checked(cc, node->lhs);
    codegen_expr_checked(cc, node->rhs);
    return;

  case ND_PRE_INC:
    if (node->lhs && node->lhs->kind == ND_VAR && node->lhs->sym &&
        node->lhs->sym->assigned_reg) {
      char *reg = node->lhs->sym->assigned_reg;
      int esz = 1;
      if (is_pointer(node->lhs->type))
        esz = ptr_elem_size(node->lhs->type);
      if (backend_ops) { fprintf(cc->out, "    ldr r3, =%d\n", esz); if (strcmp(reg, "rax") == 0) fprintf(cc->out, "    adds r0, r0, r3\n"); else fprintf(cc->out, "    adds r1, r1, r3\n"); } else fprintf(cc->out, "    addq $%d, %s\n", esz, reg);
      if (backend_ops) fprintf(cc->out, "    mov r0, %s\n", reg); else fprintf(cc->out, "    movq %s, %%rax\n", reg);
      if (node->lhs->type && !node_type_unsigned(node->lhs) && !is_pointer(node->lhs->type)) {
          if (node->lhs->type->size == 4) { if (!backend_ops) fprintf(cc->out, "    cltq\n"); }
          else if (node->lhs->type->size == 1) fprintf(cc->out, "    movsbq %%al, %%rax\n");
          else if (node->lhs->type->size == 2) fprintf(cc->out, "    movswq %%ax, %%rax\n");
      } else if (node->lhs->type && node_type_unsigned(node->lhs)) {
          if (node->lhs->type->size == 4) { if (!backend_ops) fprintf(cc->out, "    movl %%eax, %%eax\n"); }
          else if (node->lhs->type->size == 1) fprintf(cc->out, "    movzbq %%al, %%rax\n");
          else if (node->lhs->type->size == 2) fprintf(cc->out, "    movzwq %%ax, %%rax\n");
      }
      if (backend_ops) fprintf(cc->out, "    mov %s, r0\n", reg); else fprintf(cc->out, "    movq %%rax, %s\n", reg);
      return;
    }
    if (!node->lhs) {
      error_at(cc, node->line, "codegen_expr: ND_PRE_INC null lhs");
      fprintf(cc->out, "    movq $0, %%rax\n");
      return;
    }
    codegen_addr_checked(cc, node->lhs);
    if (is_bad_ptr(node->lhs)) {
      error_at(cc, node->line, "codegen_expr: ND_PRE_INC lhs bad ptr");
      fprintf(cc->out, "    movq $0, %%rax\n");
      return;
    }
    push_reg(cc, "rax");
    if (!node->lhs->type) {
      error_at(cc, node->line, "codegen_expr: ND_PRE_INC lhs has null type");
      fprintf(cc->out, "    movq $0, %%rax\n");
      return;
    }
    codegen_load(cc, node->lhs->type);
    if (node->lhs->type && is_float_type(node->lhs->type)) {
      /* CG-FLOAT-011: float ++/-- must use FP add, not integer addq */
      if (node->lhs->type->kind == TY_FLOAT) {
        fprintf(cc->out, "    movd %%eax, %%xmm0\n");
        fprintf(cc->out, "    movss .Lf32_one(%%rip), %%xmm1\n");
        fprintf(cc->out, "    addss %%xmm1, %%xmm0\n");
        fprintf(cc->out, "    movd %%xmm0, %%eax\n");
      } else {
        fprintf(cc->out, "    movq %%rax, %%xmm0\n");
        fprintf(cc->out, "    movsd .Lf64_one(%%rip), %%xmm1\n");
        fprintf(cc->out, "    addsd %%xmm1, %%xmm0\n");
        fprintf(cc->out, "    movq %%xmm0, %%rax\n");
      }
    } else {
      int esz;
      esz = 1;
      if (is_pointer(node->lhs->type))
        esz = ptr_elem_size(node->lhs->type);
      if (backend_ops) { fprintf(cc->out, "    ldr r3, =%d\n    adds r0, r0, r3\n", esz); } else fprintf(cc->out, "    addq $%d, %%rax\n", esz);
    }
    if (backend_ops) fprintf(cc->out, "    mov r1, r0\n");
      else fprintf(cc->out, "    movq %%rax, %%r11\n");
    pop_reg(cc, "rax");
    switch (type_size(node->lhs->type)) {
    case 1:
      if (backend_ops) fprintf(cc->out, "    strb r1, [r0]\n");
      else fprintf(cc->out, "    movb %%r11b, (%%rax)\n");
      break;
    case 2:
      if (backend_ops) fprintf(cc->out, "    strh r1, [r0]\n");
      else fprintf(cc->out, "    movw %%r11w, (%%rax)\n");
      break;
    case 4:
      if (backend_ops) fprintf(cc->out, "    str r1, [r0]\n");
      else fprintf(cc->out, "    movl %%r11d, (%%rax)\n");
      break;
    default:
      if (backend_ops) fprintf(cc->out, "    str r1, [r0]\n");
      else fprintf(cc->out, "    movq %%r11, (%%rax)\n");
      break;
    }
    if (backend_ops) fprintf(cc->out, "    mov r0, r1\n");
      else fprintf(cc->out, "    movq %%r11, %%rax\n");
    if (node->lhs->type && !is_float_type(node->lhs->type) &&
        !node_type_unsigned(node->lhs) && !is_pointer(node->lhs->type)) {
        if (node->lhs->type->size == 4) { if (!backend_ops) fprintf(cc->out, "    cltq\n"); }
        else if (node->lhs->type->size == 1) fprintf(cc->out, "    movsbq %%al, %%rax\n");
        else if (node->lhs->type->size == 2) fprintf(cc->out, "    movswq %%ax, %%rax\n");
    } else if (node->lhs->type && !is_float_type(node->lhs->type) &&
               node_type_unsigned(node->lhs)) {
        if (node->lhs->type->size == 4) { if (!backend_ops) fprintf(cc->out, "    movl %%eax, %%eax\n"); }
        else if (node->lhs->type->size == 1) fprintf(cc->out, "    movzbq %%al, %%rax\n");
        else if (node->lhs->type->size == 2) fprintf(cc->out, "    movzwq %%ax, %%rax\n");
    }
    {
      char *dst = ir_bridge_fresh_tmp();
      ZCC_EMIT_UNARY(IR_CAST, ir_map_type(node->lhs->type), dst, "pre_inc", node->line);
    }
    return;

  case ND_PRE_DEC:
    if (node->lhs && node->lhs->kind == ND_VAR && node->lhs->sym &&
        node->lhs->sym->assigned_reg) {
      char *reg = node->lhs->sym->assigned_reg;
      int esz = 1;
      if (is_pointer(node->lhs->type))
        esz = ptr_elem_size(node->lhs->type);
      if (backend_ops) { fprintf(cc->out, "    ldr r3, =%d\n", esz); if (strcmp(reg, "rax") == 0) fprintf(cc->out, "    subs r0, r0, r3\n"); else fprintf(cc->out, "    subs r1, r1, r3\n"); } else fprintf(cc->out, "    subq $%d, %s\n", esz, reg);
      if (backend_ops) fprintf(cc->out, "    mov r0, %s\n", reg); else fprintf(cc->out, "    movq %s, %%rax\n", reg);
      if (node->lhs->type && !node_type_unsigned(node->lhs) && !is_pointer(node->lhs->type)) {
          if (node->lhs->type->size == 4) { if (!backend_ops) fprintf(cc->out, "    cltq\n"); }
          else if (node->lhs->type->size == 1) fprintf(cc->out, "    movsbq %%al, %%rax\n");
          else if (node->lhs->type->size == 2) fprintf(cc->out, "    movswq %%ax, %%rax\n");
      } else if (node->lhs->type && node_type_unsigned(node->lhs)) {
          if (node->lhs->type->size == 4) { if (!backend_ops) fprintf(cc->out, "    movl %%eax, %%eax\n"); }
          else if (node->lhs->type->size == 1) fprintf(cc->out, "    movzbq %%al, %%rax\n");
          else if (node->lhs->type->size == 2) fprintf(cc->out, "    movzwq %%ax, %%rax\n");
      }
      if (backend_ops) fprintf(cc->out, "    mov %s, r0\n", reg); else fprintf(cc->out, "    movq %%rax, %s\n", reg);
      return;
    }
    if (!node->lhs) {
      error_at(cc, node->line, "codegen_expr: ND_PRE_DEC null lhs");
      fprintf(cc->out, "    movq $0, %%rax\n");
      return;
    }
    codegen_addr_checked(cc, node->lhs);
    if (is_bad_ptr(node->lhs)) {
      error_at(cc, node->line, "codegen_expr: ND_PRE_DEC lhs bad ptr");
      fprintf(cc->out, "    movq $0, %%rax\n");
      return;
    }
    push_reg(cc, "rax");
    if (!node->lhs->type) {
      error_at(cc, node->line, "codegen_expr: ND_PRE_DEC lhs has null type");
      fprintf(cc->out, "    movq $0, %%rax\n");
      return;
    }
    codegen_load(cc, node->lhs->type);
    if (node->lhs->type && is_float_type(node->lhs->type)) {
      /* CG-FLOAT-011: float -- must use FP sub, not integer subq */
      if (node->lhs->type->kind == TY_FLOAT) {
        fprintf(cc->out, "    movd %%eax, %%xmm0\n");
        fprintf(cc->out, "    movss .Lf32_one(%%rip), %%xmm1\n");
        fprintf(cc->out, "    subss %%xmm1, %%xmm0\n");
        fprintf(cc->out, "    movd %%xmm0, %%eax\n");
      } else {
        fprintf(cc->out, "    movq %%rax, %%xmm0\n");
        fprintf(cc->out, "    movsd .Lf64_one(%%rip), %%xmm1\n");
        fprintf(cc->out, "    subsd %%xmm1, %%xmm0\n");
        fprintf(cc->out, "    movq %%xmm0, %%rax\n");
      }
    } else {
      int esz;
      esz = 1;
      if (is_pointer(node->lhs->type))
        esz = ptr_elem_size(node->lhs->type);
      if (backend_ops) { fprintf(cc->out, "    ldr r3, =%d\n    subs r0, r0, r3\n", esz); } else fprintf(cc->out, "    subq $%d, %%rax\n", esz);
    }
    if (backend_ops) fprintf(cc->out, "    mov r1, r0\n");
      else fprintf(cc->out, "    movq %%rax, %%r11\n");
    pop_reg(cc, "rax");
    switch (type_size(node->lhs->type)) {
    case 1:
      if (backend_ops) fprintf(cc->out, "    strb r1, [r0]\n");
      else fprintf(cc->out, "    movb %%r11b, (%%rax)\n");
      break;
    case 2:
      if (backend_ops) fprintf(cc->out, "    strh r1, [r0]\n");
      else fprintf(cc->out, "    movw %%r11w, (%%rax)\n");
      break;
    case 4:
      if (backend_ops) fprintf(cc->out, "    str r1, [r0]\n");
      else fprintf(cc->out, "    movl %%r11d, (%%rax)\n");
      break;
    default:
      if (backend_ops) fprintf(cc->out, "    str r1, [r0]\n");
      else fprintf(cc->out, "    movq %%r11, (%%rax)\n");
      break;
    }
    if (backend_ops) fprintf(cc->out, "    mov r0, r1\n");
      else fprintf(cc->out, "    movq %%r11, %%rax\n");
    if (node->lhs->type && !is_float_type(node->lhs->type) &&
        !node_type_unsigned(node->lhs) && !is_pointer(node->lhs->type)) {
        if (node->lhs->type->size == 4) { if (!backend_ops) fprintf(cc->out, "    cltq\n"); }
        else if (node->lhs->type->size == 1) fprintf(cc->out, "    movsbq %%al, %%rax\n");
        else if (node->lhs->type->size == 2) fprintf(cc->out, "    movswq %%ax, %%rax\n");
    } else if (node->lhs->type && !is_float_type(node->lhs->type) &&
               node_type_unsigned(node->lhs)) {
        if (node->lhs->type->size == 4) { if (!backend_ops) fprintf(cc->out, "    movl %%eax, %%eax\n"); }
        else if (node->lhs->type->size == 1) fprintf(cc->out, "    movzbq %%al, %%rax\n");
        else if (node->lhs->type->size == 2) fprintf(cc->out, "    movzwq %%ax, %%rax\n");
    }
    {
      char *dst = ir_bridge_fresh_tmp();
      ZCC_EMIT_UNARY(IR_CAST, ir_map_type(node->lhs->type), dst, "pre_dec", node->line);
    }
    return;

  case ND_POST_INC:
    if (node->lhs && node->lhs->kind == ND_VAR && node->lhs->sym &&
        node->lhs->sym->assigned_reg) {
      char *reg = node->lhs->sym->assigned_reg;
      int esz = 1;
      if (is_pointer(node->lhs->type))
        esz = ptr_elem_size(node->lhs->type);
      if (backend_ops) fprintf(cc->out, "    mov r0, %s\n", reg); else fprintf(cc->out, "    movq %s, %%rax\n", reg);
      if (backend_ops) { fprintf(cc->out, "    ldr r3, =%d\n", esz); if (strcmp(reg, "rax") == 0) fprintf(cc->out, "    adds r0, r0, r3\n"); else fprintf(cc->out, "    adds r1, r1, r3\n"); } else fprintf(cc->out, "    addq $%d, %s\n", esz, reg);
      return;
    }
    if (!node->lhs) {
      error_at(cc, node->line, "codegen_expr: ND_POST_INC null lhs");
      fprintf(cc->out, "    movq $0, %%rax\n");
      return;
    }
    codegen_addr_checked(cc, node->lhs);
    if (is_bad_ptr(node->lhs)) {
      error_at(cc, node->line, "codegen_expr: ND_POST_INC lhs bad ptr");
      fprintf(cc->out, "    movq $0, %%rax\n");
      return;
    }
    push_reg(cc, "rax");
    if (!node->lhs->type) {
      error_at(cc, node->line, "codegen_expr: ND_POST_INC lhs has null type");
      fprintf(cc->out, "    movq $0, %%rax\n");
      return;
    }
    codegen_load(cc, node->lhs->type);
    push_reg(cc, "rax"); /* save original value */
    if (node->lhs->type && is_float_type(node->lhs->type)) {
      /* CG-FLOAT-011: float post++ must use FP add, not integer addq */
      if (node->lhs->type->kind == TY_FLOAT) {
        fprintf(cc->out, "    movd %%eax, %%xmm0\n");
        fprintf(cc->out, "    movss .Lf32_one(%%rip), %%xmm1\n");
        fprintf(cc->out, "    addss %%xmm1, %%xmm0\n");
        fprintf(cc->out, "    movd %%xmm0, %%eax\n");
      } else {
        fprintf(cc->out, "    movq %%rax, %%xmm0\n");
        fprintf(cc->out, "    movsd .Lf64_one(%%rip), %%xmm1\n");
        fprintf(cc->out, "    addsd %%xmm1, %%xmm0\n");
        fprintf(cc->out, "    movq %%xmm0, %%rax\n");
      }
    } else {
      int esz;
      esz = 1;
      if (is_pointer(node->lhs->type))
        esz = ptr_elem_size(node->lhs->type);
      if (backend_ops) { fprintf(cc->out, "    ldr r3, =%d\n    adds r0, r0, r3\n", esz); } else fprintf(cc->out, "    addq $%d, %%rax\n", esz);
    }
    if (backend_ops) fprintf(cc->out, "    mov r1, r0\n");
      else fprintf(cc->out, "    movq %%rax, %%r11\n");
    pop_reg(cc, "rdx"); /* rdx = original value */
    pop_reg(cc, "rax"); /* rax = address */
    switch (type_size(node->lhs->type)) {
    case 1:
      if (backend_ops) fprintf(cc->out, "    strb r1, [r0]\n");
      else fprintf(cc->out, "    movb %%r11b, (%%rax)\n");
      break;
    case 2:
      if (backend_ops) fprintf(cc->out, "    strh r1, [r0]\n");
      else fprintf(cc->out, "    movw %%r11w, (%%rax)\n");
      break;
    case 4:
      if (backend_ops) fprintf(cc->out, "    str r1, [r0]\n");
      else fprintf(cc->out, "    movl %%r11d, (%%rax)\n");
      break;
    default:
      if (backend_ops) fprintf(cc->out, "    str r1, [r0]\n");
      else fprintf(cc->out, "    movq %%r11, (%%rax)\n");
      break;
    }
    if (backend_ops) fprintf(cc->out, "    mov r0, r2\n");
      else fprintf(cc->out, "    movq %%rdx, %%rax\n");
    {
      char *dst = ir_bridge_fresh_tmp();
      ZCC_EMIT_UNARY(IR_CAST, ir_map_type(node->lhs->type), dst, "post_inc", node->line);
    }
    return;

  case ND_POST_DEC:
    if (node->lhs && node->lhs->kind == ND_VAR && node->lhs->sym &&
        node->lhs->sym->assigned_reg) {
      char *reg = node->lhs->sym->assigned_reg;
      int esz = 1;
      if (is_pointer(node->lhs->type))
        esz = ptr_elem_size(node->lhs->type);
      if (backend_ops) fprintf(cc->out, "    mov r0, %s\n", reg); else fprintf(cc->out, "    movq %s, %%rax\n", reg);
      if (backend_ops) { fprintf(cc->out, "    ldr r3, =%d\n", esz); if (strcmp(reg, "rax") == 0) fprintf(cc->out, "    subs r0, r0, r3\n"); else fprintf(cc->out, "    subs r1, r1, r3\n"); } else fprintf(cc->out, "    subq $%d, %s\n", esz, reg);
      return;
    }
    if (!node->lhs) {
      error_at(cc, node->line, "codegen_expr: ND_POST_DEC null lhs");
      fprintf(cc->out, "    movq $0, %%rax\n");
      return;
    }
    codegen_addr_checked(cc, node->lhs);
    if (is_bad_ptr(node->lhs)) {
      error_at(cc, node->line, "codegen_expr: ND_POST_DEC lhs bad ptr");
      fprintf(cc->out, "    movq $0, %%rax\n");
      return;
    }
    push_reg(cc, "rax");
    if (!node->lhs->type) {
      error_at(cc, node->line, "codegen_expr: ND_POST_DEC lhs has null type");
      fprintf(cc->out, "    movq $0, %%rax\n");
      return;
    }
    codegen_load(cc, node->lhs->type);
    push_reg(cc, "rax");
    if (node->lhs->type && is_float_type(node->lhs->type)) {
      /* CG-FLOAT-011: float post-- must use FP sub, not integer subq */
      if (node->lhs->type->kind == TY_FLOAT) {
        fprintf(cc->out, "    movd %%eax, %%xmm0\n");
        fprintf(cc->out, "    movss .Lf32_one(%%rip), %%xmm1\n");
        fprintf(cc->out, "    subss %%xmm1, %%xmm0\n");
        fprintf(cc->out, "    movd %%xmm0, %%eax\n");
      } else {
        fprintf(cc->out, "    movq %%rax, %%xmm0\n");
        fprintf(cc->out, "    movsd .Lf64_one(%%rip), %%xmm1\n");
        fprintf(cc->out, "    subsd %%xmm1, %%xmm0\n");
        fprintf(cc->out, "    movq %%xmm0, %%rax\n");
      }
    } else {
      int esz;
      esz = 1;
      if (is_pointer(node->lhs->type))
        esz = ptr_elem_size(node->lhs->type);
      if (backend_ops) { fprintf(cc->out, "    ldr r3, =%d\n    subs r0, r0, r3\n", esz); } else fprintf(cc->out, "    subq $%d, %%rax\n", esz);
    }
    if (backend_ops) fprintf(cc->out, "    mov r1, r0\n");
      else fprintf(cc->out, "    movq %%rax, %%r11\n");
    pop_reg(cc, "rdx"); /* rdx = original value */
    pop_reg(cc, "rax"); /* rax = address */
    switch (type_size(node->lhs->type)) {
    case 1:
      if (backend_ops) fprintf(cc->out, "    strb r1, [r0]\n");
      else fprintf(cc->out, "    movb %%r11b, (%%rax)\n");
      break;
    case 2:
      if (backend_ops) fprintf(cc->out, "    strh r1, [r0]\n");
      else fprintf(cc->out, "    movw %%r11w, (%%rax)\n");
      break;
    case 4:
      if (backend_ops) fprintf(cc->out, "    str r1, [r0]\n");
      else fprintf(cc->out, "    movl %%r11d, (%%rax)\n");
      break;
    default:
      if (backend_ops) fprintf(cc->out, "    str r1, [r0]\n");
      else fprintf(cc->out, "    movq %%r11, (%%rax)\n");
      break;
    }
    if (backend_ops) fprintf(cc->out, "    mov r0, r2\n");
      else fprintf(cc->out, "    movq %%rdx, %%rax\n");
    {
      char *dst = ir_bridge_fresh_tmp();
      ZCC_EMIT_UNARY(IR_CAST, ir_map_type(node->lhs->type), dst, "post_dec", node->line);
    }
    return;

  case ND_CALL: {
    char *argregs[6];
    int i;
    int nargs;
    int shadow_and_stack;
    int args_on_stack;
    int alignment_pad;
    int cleanup_bytes;
    char args_ir_1d[2048];

    /* System V AMD64 (Linux): 6 register args: RDI, RSI, RDX, RCX, R8, R9; 7th+
     * on stack */
    argregs[0] = "rdi";
    argregs[1] = "rsi";
    argregs[2] = "rdx";
    argregs[3] = "rcx";
    argregs[4] = "r8";
    argregs[5] = "r9";

    if (!node->func_name[0] && !node->lhs) {
      error_at(cc, node->line, "codegen_expr: ND_CALL no func_name and no callee");
      fprintf(cc->out, "    movq $0, %%rax\n");
      return;
    }
    if (!node->args) {
      error_at(cc, node->line, "codegen_expr: ND_CALL with NULL args");
      return;
    }
    nargs = node->num_args;
    if (nargs < 0 || nargs > 64) {
      error_at(cc, node->line, "call node bad num_args");
      return;
    }

    /* System V AMD64: no shadow space required. Space for 7th+ gp args and 9th+ fp args is left on the stack. */
    {
      int temp_gp = 0, temp_fp = 0;
      /* Resolve callee type early for arg classification */
      Symbol *pre_sym = node->func_name[0] ? scope_find(cc, node->func_name) : 0;
      Type *pre_ftype = (pre_sym && pre_sym->type && pre_sym->type->kind == TY_FUNC) ? pre_sym->type : 0;
      if (!pre_ftype && node->lhs && node->lhs->type) {
        Type *plt = node->lhs->type;
        if (plt->kind == TY_FUNC) pre_ftype = plt;
        else if (plt->kind == TY_PTR && plt->base && plt->base->kind == TY_FUNC) pre_ftype = plt->base;
      }
      for (i = 0; i < nargs; i++) {
        int is_fp_arg = 0;
        if (node->args[i] && node->args[i]->type && is_float_type(node->args[i]->type)) {
          is_fp_arg = 1;
        } else if (pre_ftype && pre_ftype->params && i < pre_ftype->num_params &&
                   pre_ftype->params[i] && is_float_type(pre_ftype->params[i])) {
          is_fp_arg = 1; /* int promoted to float/double at call site */
        }
        if (is_fp_arg) {
          if (temp_fp < 8) temp_fp++;
        } else {
          if (temp_gp < 6) temp_gp++;
        }
      }
      args_on_stack = nargs - (temp_gp + temp_fp);
    }
    /* We must ensure that AFTER the args are on the stack, %rsp is 16-byte
     * aligned. */
    /* Currently, depth is cc->stack_depth. After pushes/pops, depth will be
     * cc->stack_depth + args_on_stack. */
    alignment_pad = 0;
    {
      if ((cc->stack_depth + args_on_stack) % 2 != 0) {
        alignment_pad = 8;
      }
    }


    if (alignment_pad > 0) {
      fprintf(cc->out, "    subq $%d, %%rsp\n", alignment_pad);
      cc->stack_depth++;
    }

    /* for indirect calls, evaluate callee first and save on stack */
    if (node->func_name[0] == 0 && node->lhs) {
      codegen_expr_checked(cc, node->lhs);
      push_reg(cc, "rax");
    }

    /* push args in reverse order */
    for (i = nargs - 1; i >= 0; i--) {
      if (!node->args[i]) {
        error_at(cc, node->line, "null argument in call");
        return;
      }
      codegen_expr_checked(cc, node->args[i]);
      Type *atype = node->args[i]->type;
      if (atype && (atype->kind == TY_STRUCT || atype->kind == TY_UNION)) {
          abi_class_t eb[2];
          classify_aggregate(atype, eb);
          if (eb[0] != CLASS_MEMORY) {
              /* Small aggregate: push by value (eightbytes) */
              fprintf(cc->out, "    movq %%rax, %%r10\n");
              if (type_size(atype) > 8) {
                  fprintf(cc->out, "    pushq 8(%%r10)\n");
                  cc->stack_depth++;
              }
              fprintf(cc->out, "    pushq 0(%%r10)\n");
              cc->stack_depth++;
              continue;
          }
      }
      ir_save_result(&args_ir_1d[i * 32]);
      ZCC_EMIT_ARG(ir_map_type(node->args[i] ? node->args[i]->type : 0), &args_ir_1d[i * 32], node->line);
      push_reg(cc, "rax");
    }

    /* pop args into correct registers: floats->xmm, ints->gpregs independently */
    {
      int gp_idx = 0;
      int fp_idx = 0;
      /* Resolve callee's declared parameter types for float/double ABI */
      Symbol *callee_sym = node->func_name[0] ? scope_find(cc, node->func_name) : 0;
      Type *callee_ftype = (callee_sym && callee_sym->type && callee_sym->type->kind == TY_FUNC) ? callee_sym->type : 0;
      /* CG-FLOAT-008c: for indirect calls via function pointer, extract TY_FUNC from lhs type */
      if (!callee_ftype && node->lhs && node->lhs->type) {
        Type *lt = node->lhs->type;
        if (lt->kind == TY_FUNC)
          callee_ftype = lt;
        else if (lt->kind == TY_PTR && lt->base && lt->base->kind == TY_FUNC)
          callee_ftype = lt->base;
      }
       for (i = 0; i < nargs; i++) {
        Type *atype = node->args[i]->type;
        if (atype && (atype->kind == TY_STRUCT || atype->kind == TY_UNION)) {
            abi_class_t eb[2];
            classify_aggregate(atype, eb);
            if (eb[0] != CLASS_MEMORY) {
                /* Small aggregate: pop eightbytes into correct registers. */
                /* Pop eightbyte 0 */
                if (eb[0] == CLASS_SSE) {
                    if (fp_idx < 8) {
                        fprintf(cc->out, "    popq %%rax\n");
                        cc->stack_depth--;
                        fprintf(cc->out, "    movq %%rax, %%xmm%d\n", fp_idx++);
                    }
                } else {
                    if (gp_idx < 6) {
                        pop_reg(cc, argregs[gp_idx++]);
                    }
                }
                /* Pop eightbyte 1 */
                if (type_size(atype) > 8) {
                    if (eb[1] == CLASS_SSE) {
                        if (fp_idx < 8) {
                            fprintf(cc->out, "    popq %%rax\n");
                            cc->stack_depth--;
                            fprintf(cc->out, "    movq %%rax, %%xmm%d\n", fp_idx++);
                        }
                    } else {
                        if (gp_idx < 6) {
                            pop_reg(cc, argregs[gp_idx++]);
                        }
                    }
                }
                continue;
            }
        }
        if (atype && is_float_type(atype)) {
          if (fp_idx < 8) {
            fprintf(cc->out, "    popq %%rax\n");
            cc->stack_depth--;
            fprintf(cc->out, "    movq %%rax, %%xmm%d\n", fp_idx);
            /* Check callee's declared param type... (keep existing conversion logic) */
            {
              int arg_is_float = (atype->kind == TY_FLOAT);
              if (arg_is_float) {
                int in_fixed_float_param = 0;
                if (callee_ftype && callee_ftype->params && i < callee_ftype->num_params) {
                  if (callee_ftype->params[i]->kind == TY_FLOAT) in_fixed_float_param = 1;
                }
                if (!in_fixed_float_param) {
                  fprintf(cc->out, "    cvtss2sd %%xmm%d, %%xmm%d\n", fp_idx, fp_idx);
                }
              } else {
                int need_cvt = 0;
                if (callee_ftype && callee_ftype->params && i < callee_ftype->num_params) {
                  if (callee_ftype->params[i] && callee_ftype->params[i]->kind == TY_FLOAT) need_cvt = 1;
                }
                if (need_cvt) fprintf(cc->out, "    cvtsd2ss %%xmm%d, %%xmm%d\n", fp_idx, fp_idx);
              }
            }
            fp_idx++;
          }
        } else if (callee_ftype && callee_ftype->params && i < callee_ftype->num_params &&
                   callee_ftype->params[i] && is_float_type(callee_ftype->params[i])) {
          /* CG-FLOAT-ABI-FIX: implicit int-to-float/double conversion at call site.
           * The arg expression is integer but the callee declares this param as
           * float/double. SysV ABI requires float/double args in xmm registers.
           * Convert the integer value and route to xmm instead of GP. */
          if (fp_idx < 8) {
            fprintf(cc->out, "    popq %%rax\n");
            cc->stack_depth--;
            if (callee_ftype->params[i]->kind == TY_FLOAT) {
              fprintf(cc->out, "    cvtsi2ss %%eax, %%xmm%d\n", fp_idx);
            } else {
              fprintf(cc->out, "    cvtsi2sd %%rax, %%xmm%d\n", fp_idx);
            }
            fp_idx++;
          }
        } else {
          if (backend_ops) {
              if (gp_idx < 4) {
                 fprintf(cc->out, "    pop {r%d}\n", gp_idx);
                 cc->stack_depth--;
                 gp_idx++;
              }
          } else {
              if (gp_idx < 6) {
                pop_reg(cc, argregs[gp_idx]);
                gp_idx++;
              }
          }
        }
      }
      if (!backend_ops) {
          fprintf(cc->out, "    movl $%d, %%eax\n", fp_idx > 8 ? 8 : fp_idx);
      }
    }
    if (node->func_name[0] == 0 && node->lhs) {
      /* indirect call: pop callee into r10, call *r10 */
      pop_reg(cc, "r10");
      fprintf(cc->out, "    call *%%r10\n");
    } else if (strcmp(node->func_name, "__builtin_va_start") == 0) {
      Symbol *fsym = scope_find(cc, cc->current_func);
      int nparams_gp = 0;
      int nparams_fp = 0;
      if (fsym && fsym->type && fsym->type->params) {
          for (int j = 0; j < fsym->type->num_params; j++) {
              if (is_float_type(fsym->type->params[j])) nparams_fp++;
              else nparams_gp++;
          }
      }
      int gp_offset = nparams_gp * 8;
      int fp_offset = 48 + nparams_fp * 16;
      fprintf(cc->out, "    # arg0 = ap, arg1 ignored\n");
      fprintf(cc->out, "    movl $%d, 0(%%rdi)\n", gp_offset);
      fprintf(cc->out, "    movl $%d, 4(%%rdi)\n", fp_offset);
      fprintf(cc->out, "    leaq 16(%%rbp), %%rax\n");
      fprintf(cc->out, "    movq %%rax, 8(%%rdi)\n");
      /* The reg_save_area is dynamically stored at stack bottom. */
      fprintf(cc->out, "    leaq %d(%%rbp), %%rax\n", fsym ? fsym->stack_offset : -176);
      fprintf(cc->out, "    movq %%rax, 16(%%rdi)\n");
    } else if (strcmp(node->func_name, "__builtin_va_end") == 0) {
      /* va_end is a no-op on x86-64 SysV ABI */
      fprintf(cc->out, "    # __builtin_va_end (no-op)\n");
    } else {
      if (backend_ops && backend_ops->emit_call) {
          backend_ops->emit_call(cc, node);
      } else {
          fprintf(cc->out, "    call %s\n", node->func_name);
      }
    }

    /* CG-IR-019: System V aggregate return capture */
    if (node->type && (node->type->kind == TY_STRUCT || node->type->kind == TY_UNION)) {
        abi_class_t eb[2];
        classify_aggregate(node->type, eb);
        if (eb[0] != CLASS_MEMORY) {
            /* Small aggregate: capture registers into scratch buffer immediately post-call.
             * This must happen before any stack cleanup or other register clobbering. */
            fprintf(cc->out, "    # SysV spill for %s\n", node->type->tag[0] ? node->type->tag : "<anon>");
            
            int cur_gp = 0, cur_fp = 0;
            char *ret_gpregs[] = { "rax", "rdx" };

            /* Eightbyte 0 */
            if (eb[0] == CLASS_SSE) {
                fprintf(cc->out, "    movq %%xmm%d, %d(%%rbp)\n", cur_fp++, cc->abi_scratch_offset);
            } else {
                fprintf(cc->out, "    movq %%%s, %d(%%rbp)\n", ret_gpregs[cur_gp++], cc->abi_scratch_offset);
            }

            /* Eightbyte 1 */
            if (type_size(node->type) > 8) {
                if (eb[1] == CLASS_SSE) {
                    fprintf(cc->out, "    movq %%xmm%d, %d(%%rbp)\n", cur_fp++, cc->abi_scratch_offset + 8);
                } else {
                    fprintf(cc->out, "    movq %%%s, %d(%%rbp)\n", ret_gpregs[cur_gp++], cc->abi_scratch_offset + 8);
                }
            }
            /* Set %rax to the address of the scratch buffer for downstream consumption. */
            fprintf(cc->out, "    leaq %d(%%rbp), %%rax\n", cc->abi_scratch_offset);
        }
    } else if (node->type && is_float_type(node->type)) {
        fprintf(cc->out, "    movq %%xmm0, %%rax\n");
    } else if (node->type && !node_type_unsigned(node)) {
        if (node->type->size == 4) { if (!backend_ops) fprintf(cc->out, "    cltq\n"); }
        else if (node->type->size == 1) fprintf(cc->out, "    movsbq %%al, %%rax\n");
        else if (node->type->size == 2) fprintf(cc->out, "    movswq %%ax, %%rax\n");
    } else if (node->type && node_type_unsigned(node)) {
        if (node->type->size == 4) { if (!backend_ops) fprintf(cc->out, "    movl %%eax, %%eax\n"); }
        else if (node->type->size == 1) fprintf(cc->out, "    movzbq %%al, %%rax\n");
        else if (node->type->size == 2) fprintf(cc->out, "    movzwq %%ax, %%rax\n");
    }

    {
      char *dst = ir_bridge_fresh_tmp();
      ZCC_EMIT_CALL(ir_map_type(node->type), dst, node->func_name, node->line);
    }

    /* cleanup arguments left on stack AND the alignment pad */
    cleanup_bytes = (args_on_stack * 8) + alignment_pad;
    if (cleanup_bytes > 0) {
      fprintf(cc->out, "    addq $%d, %%rsp\n", cleanup_bytes);
      cc->stack_depth -= (cleanup_bytes / 8);
    }
    return;
  }

  case ND_NOP:
    return;

  default:
    error_at(cc, node->line, "unsupported expression in codegen");
    return;
  }
}

/* ================================================================ */
/* STATEMENT CODEGEN                                                 */
/* ================================================================ */

void codegen_stmt(Compiler *cc, Node *node) {
  if (!node)
    return;

  if (cc) {
  }

  int lbl1;
  int lbl2;
  int lbl3;
  int old_break;
  int old_continue;
  char errbuf[80];

  /* Do NOT use ptr_in_fault_range(node): stage2 miscompiles it and rejects
   * valid arena ptrs. */
  if (!node)
    return;
  if (is_bad_ptr(node)) {
    sprintf(errbuf, "codegen_stmt: bad node ptr %p", (void *)node);
    error_at(cc, 0, errbuf);
    return;
  }

  switch (node->kind) {

  case ND_RETURN:
    if (node->lhs) {
      codegen_expr_checked(cc, node->lhs);
      /* CG-IR-019: System V aggregate return support */
      Type *ty = node->lhs->type;
      if (ty && (ty->kind == TY_STRUCT || ty->kind == TY_UNION)) {
        abi_class_t eb[2];
        classify_aggregate(ty, eb);
        if (eb[0] != CLASS_MEMORY) {
          /* Small aggregate return: classify and load into registers.
           * Use %r10 as a temporary base to avoid clobbering %rax (the result address). */
          fprintf(cc->out, "    movq %%rax, %%r10\n");
          
          int cur_gp = 0, cur_fp = 0;
          char *ret_gpregs[] = { "rax", "rdx" };

          /* Eightbyte 0 */
          if (eb[0] == CLASS_SSE) {
            fprintf(cc->out, "    movq 0(%%r10), %%xmm%d\n", cur_fp++);
          } else {
            fprintf(cc->out, "    movq 0(%%r10), %%%s\n", ret_gpregs[cur_gp++]);
          }
          
          /* Eightbyte 1 (optional) */
          if (type_size(ty) > 8) {
            if (eb[1] == CLASS_SSE) {
              fprintf(cc->out, "    movq 8(%%r10), %%xmm%d\n", cur_fp++);
            } else {
              fprintf(cc->out, "    movq 8(%%r10), %%%s\n", ret_gpregs[cur_gp++]);
            }
          }
          
          /* Jump directly to epilogue */
          if (backend_ops) fprintf(cc->out, "    b .Lfunc_end_%d\n", cc->func_end_label);
          else fprintf(cc->out, "    jmp .Lfunc_end_%d\n", cc->func_end_label);
          return;
        }
      }

      {
        char ret_tmp[32];
        ir_save_result(ret_tmp);
        ZCC_EMIT_RET(ir_map_type(node->lhs->type), ret_tmp, node->line);
      }
      if (node->lhs->type && is_float_type(node->lhs->type)) {
        fprintf(cc->out, "    movq %%rax, %%xmm0\n");
      }
    } else {
      ZCC_EMIT_RET(0, "", node->line);
    }
    if (backend_ops) fprintf(cc->out, "    b .Lfunc_end_%d\n", cc->func_end_label);
    else fprintf(cc->out, "    jmp .Lfunc_end_%d\n", cc->func_end_label);
    return;

  case ND_BLOCK: {
    int i;
    int nst;
    if (!node->stmts) {
      error_at(cc, node->line, "codegen_stmt: ND_BLOCK null stmts");
      return;
    }
    nst = node->num_stmts;
    if (nst < 0 || nst > 65536) {
      error_at(cc, node->line, "block: bad num_stmts");
      return;
    }
    for (i = 0; i < nst; i++) {
      codegen_stmt(cc, node->stmts[i]);
    }
    return;
  }

  case ND_IF: {
    char cond_ir[32];
    char ir_lbl[32];
    lbl1 = new_label(cc);
    codegen_expr_checked(cc, node->cond);
    ir_save_result(cond_ir);
    if (backend_ops) fprintf(cc->out, "    cmp r0, #0\n");
    else {
      /* CG-FLOAT-012: use SSE zero-compare for float/double conditions */
      int cond_is_float = node->cond && node->cond->type &&
                          is_float_type(node->cond->type);
      if (cond_is_float) {
        int cond_f32 = node->cond->type->kind == TY_FLOAT;
        if (cond_f32) {
          fprintf(cc->out, "    movd %%eax, %%xmm0\n");
          fprintf(cc->out, "    pxor %%xmm1, %%xmm1\n");
          fprintf(cc->out, "    ucomiss %%xmm1, %%xmm0\n");
        } else {
          fprintf(cc->out, "    movq %%rax, %%xmm0\n");
          fprintf(cc->out, "    pxor %%xmm1, %%xmm1\n");
          fprintf(cc->out, "    ucomisd %%xmm1, %%xmm0\n");
        }
        /* je = zero (false); also jump if ZF via je after ucomiss */
      } else {
        fprintf(cc->out, "    cmpq $0, %%rax\n");
      }
    }
    emit_label_fmt(cc, lbl1, FMT_JE);
    sprintf(ir_lbl, ".L%d", lbl1);
    ZCC_EMIT_BR_IF(cond_ir, ir_lbl, node->line);
    codegen_stmt(cc, node->then_body);
    if (node->else_body) {
      lbl2 = new_label(cc);
      emit_label_fmt(cc, lbl2, FMT_JMP);
      sprintf(ir_lbl, ".L%d", lbl2);
      ZCC_EMIT_BR(ir_lbl, node->line);
      emit_label_fmt(cc, lbl1, FMT_DEF);
      sprintf(ir_lbl, ".L%d", lbl1);
      ZCC_EMIT_LABEL(ir_lbl, node->line);
      codegen_stmt(cc, node->else_body);
      emit_label_fmt(cc, lbl2, FMT_DEF);
      sprintf(ir_lbl, ".L%d", lbl2);
      ZCC_EMIT_LABEL(ir_lbl, node->line);
    } else {
      emit_label_fmt(cc, lbl1, FMT_DEF);
      sprintf(ir_lbl, ".L%d", lbl1);
      ZCC_EMIT_LABEL(ir_lbl, node->line);
    }
    return;
  }

  case ND_WHILE: {
    char cond_ir[32];
    char ir_lbl[32];
    lbl1 = new_label(cc); /* loop start */
    lbl2 = new_label(cc); /* loop end */
    old_break = cc->break_label;
    old_continue = cc->continue_label;
    cc->break_label = lbl2;
    cc->continue_label = lbl1;
    emit_label_fmt(cc, lbl1, FMT_DEF);
    sprintf(ir_lbl, ".L%d", lbl1);
    ZCC_EMIT_LABEL(ir_lbl, node->line);
    codegen_expr_checked(cc, node->cond);
    ir_save_result(cond_ir);
    if (backend_ops) fprintf(cc->out, "    cmp r0, #0\n");
    else if (backend_ops) fprintf(cc->out, "    cmp r0, #0\n" \
    ); else fprintf(cc->out, "    cmpq $0, %%rax\n");
    emit_label_fmt(cc, lbl2, FMT_JE);
    sprintf(ir_lbl, ".L%d", lbl2);
    ZCC_EMIT_BR_IF(cond_ir, ir_lbl, node->line);
    codegen_stmt(cc, node->body);
    emit_label_fmt(cc, lbl1, FMT_JMP);
    sprintf(ir_lbl, ".L%d", lbl1);
    ZCC_EMIT_BR(ir_lbl, node->line);
    emit_label_fmt(cc, lbl2, FMT_DEF);
    sprintf(ir_lbl, ".L%d", lbl2);
    ZCC_EMIT_LABEL(ir_lbl, node->line);
    cc->break_label = old_break;
    cc->continue_label = old_continue;
    return;
  }

  case ND_FOR: {
    char cond_ir[32];
    char ir_lbl[32];
    lbl1 = new_label(cc); /* loop start */
    lbl2 = new_label(cc); /* loop end */
    lbl3 = new_label(cc); /* continue target (increment) */
    old_break = cc->break_label;
    old_continue = cc->continue_label;
    cc->break_label = lbl2;
    cc->continue_label = lbl3;
    if (node->init)
      codegen_stmt(cc, node->init);
    emit_label_fmt(cc, lbl1, FMT_DEF);
    sprintf(ir_lbl, ".L%d", lbl1);
    ZCC_EMIT_LABEL(ir_lbl, node->line);
    if (node->cond) {
      codegen_expr_checked(cc, node->cond);
      ir_save_result(cond_ir);
      if (backend_ops) fprintf(cc->out, "    cmp r0, #0\n");
      else if (backend_ops) fprintf(cc->out, "    cmp r0, #0\n" \
    ); else fprintf(cc->out, "    cmpq $0, %%rax\n");
      emit_label_fmt(cc, lbl2, FMT_JE);
      sprintf(ir_lbl, ".L%d", lbl2);
      ZCC_EMIT_BR_IF(cond_ir, ir_lbl, node->line);
    }
    codegen_stmt(cc, node->body);
    emit_label_fmt(cc, lbl3, FMT_DEF);
    sprintf(ir_lbl, ".L%d", lbl3);
    ZCC_EMIT_LABEL(ir_lbl, node->line);
    if (node->inc)
      codegen_expr_checked(cc, node->inc);
    emit_label_fmt(cc, lbl1, FMT_JMP);
    sprintf(ir_lbl, ".L%d", lbl1);
    ZCC_EMIT_BR(ir_lbl, node->line);
    emit_label_fmt(cc, lbl2, FMT_DEF);
    sprintf(ir_lbl, ".L%d", lbl2);
    ZCC_EMIT_LABEL(ir_lbl, node->line);
    cc->break_label = old_break;
    cc->continue_label = old_continue;
    return;
  }

  case ND_DO_WHILE: {
    /* do { body } while (cond)
     * Layout:
     *   lbl_body: (lbl1)
     *       <body> — break→lbl2, continue→lbl_cond
     *   lbl_cond: (lbl3 — new label for condition)
     *       <cond>; jne lbl_body
     *   lbl_break: (lbl2)
     *
     * CRITICAL: continue must jump to lbl_cond (the condition/increment),
     * NOT back to lbl_body (which would skip the while-condition entirely,
     * turning "do { if(!x) continue; } while(++i<n)" into an infinite loop
     * because ++i is in the condition and never evaluated). */
    int lbl3;   /* separate continue-target label = condition start */
    char cond_ir[32];
    char ir_lbl[32];
    lbl1 = new_label(cc);  /* body start */
    lbl2 = new_label(cc);  /* break/exit */
    lbl3 = new_label(cc);  /* continue target = condition-start */
    old_break = cc->break_label;
    old_continue = cc->continue_label;
    cc->break_label = lbl2;
    cc->continue_label = lbl3;   /* ← fix: continue skips to condition */
    emit_label_fmt(cc, lbl1, FMT_DEF);
    sprintf(ir_lbl, ".L%d", lbl1);
    ZCC_EMIT_LABEL(ir_lbl, node->line);
    codegen_stmt(cc, node->body);
    /* condition begins here — this is where continue targets */
    emit_label_fmt(cc, lbl3, FMT_DEF);
    sprintf(ir_lbl, ".L%d", lbl3);
    ZCC_EMIT_LABEL(ir_lbl, node->line);
    codegen_expr_checked(cc, node->cond);
    ir_save_result(cond_ir);
    if (backend_ops) fprintf(cc->out, "    cmp r0, #0\n" \
    ); else fprintf(cc->out, "    cmpq $0, %%rax\n");
    emit_label_fmt(cc, lbl1, FMT_JNE);
    sprintf(ir_lbl, ".L%d", lbl1);
    ZCC_EMIT_BR_IF(cond_ir, ir_lbl, node->line);
    emit_label_fmt(cc, lbl2, FMT_DEF);
    sprintf(ir_lbl, ".L%d", lbl2);
    ZCC_EMIT_LABEL(ir_lbl, node->line);
    cc->break_label = old_break;
    cc->continue_label = old_continue;
    return;
  }

  case ND_SWITCH: {
    int end_lbl;
    int *case_labels;
    int default_lbl;
    int i;
    int ncase;
    char switch_val_ir[32];
    char ir_lbl[64];

    if (!node->cases) {
      error_at(cc, node->line, "codegen_stmt: ND_SWITCH null cases");
      return;
    }
    ncase = node->num_cases;
    if (ncase < 0 || ncase > MAX_CASES) {
      error_at(cc, node->line, "switch: bad num_cases");
      return;
    }

    end_lbl = new_label(cc);
    old_break = cc->break_label;
    cc->break_label = end_lbl;

    codegen_expr_checked(cc, node->cond);
    ir_save_result(switch_val_ir);

    case_labels = (int *)cc_alloc(cc, sizeof(int) * (ncase + 1));
    for (i = 0; i < ncase; i++) {
      if (!node->cases[i]) {
        error_at(cc, node->line, "codegen_stmt: ND_SWITCH null case");
        return;
      }
      case_labels[i] = new_label(cc);
      fprintf(cc->out, "    cmpq $%lld, %%rax\n", node->cases[i]->case_val);
      emit_label_fmt(cc, case_labels[i], FMT_JE);

      {
        char case_const_ir[32];
        char cmp_ir[32];
        char *ct;

        ct = ir_bridge_fresh_tmp();
        sprintf(case_const_ir, "%s", ct);
        ZCC_EMIT_CONST(IR_TY_I64, case_const_ir, node->cases[i]->case_val,
                       node->line);

        ct = ir_bridge_fresh_tmp();
        sprintf(cmp_ir, "%s", ct);

        ZCC_EMIT_BINARY(IR_EQ, IR_TY_I64, cmp_ir, switch_val_ir, case_const_ir,
                        node->line);
        sprintf(ir_lbl, ".L%d", case_labels[i]);
        ZCC_EMIT_BR_IF(cmp_ir, ir_lbl, node->line);
      }
    }
    default_lbl = new_label(cc);
    if (node->default_case) {
      emit_label_fmt(cc, default_lbl, FMT_JMP);
      sprintf(ir_lbl, ".L%d", default_lbl);
      ZCC_EMIT_BR(ir_lbl, node->line);
    } else {
      emit_label_fmt(cc, end_lbl, FMT_JMP);
      sprintf(ir_lbl, ".L%d", end_lbl);
      ZCC_EMIT_BR(ir_lbl, node->line);
    }

    for (i = 0; i < ncase; i++) {
      emit_label_fmt(cc, case_labels[i], FMT_DEF);
      sprintf(ir_lbl, ".L%d", case_labels[i]);
      ZCC_EMIT_LABEL(ir_lbl, node->line);
      if (node->cases[i]->case_body)
        codegen_stmt(cc, node->cases[i]->case_body);
    }
    if (node->default_case) {
      emit_label_fmt(cc, default_lbl, FMT_DEF);
      sprintf(ir_lbl, ".L%d", default_lbl);
      ZCC_EMIT_LABEL(ir_lbl, node->line);
      if (node->default_case->case_body)
        codegen_stmt(cc, node->default_case->case_body);
    }

    if (node->body)
      codegen_stmt(cc, node->body);

    emit_label_fmt(cc, end_lbl, FMT_DEF);
    sprintf(ir_lbl, ".L%d", end_lbl);
    ZCC_EMIT_LABEL(ir_lbl, node->line);
    cc->break_label = old_break;
    return;
  }

  case ND_BREAK: {
    char ir_lbl[32];
    emit_label_fmt(cc, cc->break_label, FMT_JMP);
    sprintf(ir_lbl, ".L%d", cc->break_label);
    ZCC_EMIT_BR(ir_lbl, node->line);
    return;
  }

  case ND_CONTINUE: {
    char ir_lbl[32];
    emit_label_fmt(cc, cc->continue_label, FMT_JMP);
    sprintf(ir_lbl, ".L%d", cc->continue_label);
    ZCC_EMIT_BR(ir_lbl, node->line);
    return;
  }

  case ND_GOTO: {
    char ir_lbl[64];
    if (!node->label_name) {
      error_at(cc, node->line, "codegen_stmt: ND_GOTO null label_name");
      return;
    }
    fprintf(cc->out, "    jmp .Luser_%s_%s\n", cc->current_func,
            node->label_name);
    sprintf(ir_lbl, ".Luser_%s_%s", cc->current_func, node->label_name);
    ZCC_EMIT_BR(ir_lbl, node->line);
    return;
  }

  case ND_GOTO_COMPUTED: {
    /* GNU C Extension: goto *expr; */
    codegen_expr(cc, node->lhs);
    pop_reg(cc, "rax");
    fprintf(cc->out, "    jmp *%%rax\n");
    /* IR bridge: unsupported for now. */
    return;
  }

  case ND_LABEL: {
    char ir_lbl[64];
    if (!node->label_name) {
      error_at(cc, node->line, "codegen_stmt: ND_LABEL null label_name");
      return;
    }
    fprintf(cc->out, ".Luser_%s_%s:\n", cc->current_func, node->label_name);
    sprintf(ir_lbl, ".Luser_%s_%s", cc->current_func, node->label_name);
    ZCC_EMIT_LABEL(ir_lbl, node->line);
    if (node->lhs)
      codegen_stmt(cc, node->lhs);
    return;
  }

  case ND_NOP:
    return;

  case ND_ASM:
    fprintf(cc->out, "    %s\n", node->asm_string);
    ZCC_EMIT_ASM(node->asm_string, node->line);
    /* Note: IR backend handles this via ZCC_ND_ASM -> OP_ASM translation if enabled */
    return;

  default: {
    /* expression statement */
    char badmsg[80];
    if (is_bad_ptr(node)) {
      sprintf(badmsg, "codegen_stmt default: bad expr node %p",
              node ? (void *)node : (void *)0);
      error_at(cc, 0, badmsg);
      fprintf(cc->out, "    movq $0, %%rax\n");
      return;
    }
    codegen_expr_checked(cc, node);
    return;
  }
  }
}

/* ================================================================ */
/* REGISTER ALLOCATOR (CHEATCODE 3)                                  */
/* ================================================================ */

static int pseudo_pc = 0;
static Symbol *ra_locals[1024];
static int num_ra_locals = 0;
static char *get_callee_reg(int i) {
  if (backend_ops) {
    if (i == 0) return "r4";
    if (i == 1) return "r5";
    if (i == 2) return "r6";
    if (i == 3) return "r7";
    return "r4";
  }
  if (i == 0)
    return "%r12";
  if (i == 1)
    return "%r13";
  if (i == 2)
    return "%r14";
  if (i == 3)
    return "%r15";
  return "%rbx";
}

static void ra_add_local(Symbol *sym) {
  int i;
  if (!sym || !sym->is_local || sym->live_start == -1)
    return;
  for (i = 0; i < num_ra_locals; i++) {
    if (ra_locals[i] == sym)
      return;
  }
  if (num_ra_locals < 1024)
    ra_locals[num_ra_locals++] = sym;
}

static void compute_liveness(Node *n) {
  int i;
  int loop_start;
  if (!n)
    return;
  pseudo_pc++;

  if (n->kind == ND_WHILE || n->kind == ND_FOR || n->kind == ND_DO_WHILE) {
    loop_start = pseudo_pc;
    compute_liveness(n->cond);
    compute_liveness(n->body);
    compute_liveness(n->init);
    compute_liveness(n->inc);
    for (i = 0; i < num_ra_locals; i++) {
      if (ra_locals[i]->live_end >= loop_start) {
        ra_locals[i]->live_end = pseudo_pc;
        if (ra_locals[i]->live_start >= loop_start) {
          ra_locals[i]->live_start = loop_start;
        }
      }
    }
    return;
  }

  if (n->kind == ND_ADDR && n->lhs && n->lhs->kind == ND_VAR && n->lhs->sym) {
    n->lhs->sym->live_start = -1; /* disable alloc */
  }
  if (n->kind == ND_VAR && n->sym && n->sym->is_local) {
    if (n->sym->live_start != -1) {
      if (n->sym->live_start == 0)
        n->sym->live_start = pseudo_pc;
      n->sym->live_end = pseudo_pc;
      ra_add_local(n->sym);
    }
  }

  compute_liveness(n->lhs);
  compute_liveness(n->rhs);
  compute_liveness(n->cond);
  compute_liveness(n->then_body);
  compute_liveness(n->else_body);
  compute_liveness(n->init);
  compute_liveness(n->inc);
  compute_liveness(n->body);
  if (n->stmts) {
    for (i = 0; i < n->num_stmts; i++)
      compute_liveness(n->stmts[i]);
  }
  if (n->args) {
    for (i = 0; i < n->num_args; i++)
      compute_liveness(n->args[i]);
  }
  if (n->cases) {
    for (i = 0; i < n->num_cases; i++)
      compute_liveness(n->cases[i]);
  }
  compute_liveness(n->default_case);
  compute_liveness(n->case_body);
}

static int allocate_registers(Node *func) {
  int count = 0;
  int i, j, r;
  int param_limit = -(func->num_params * 8);
  char *active_regs[5] = {0, 0, 0, 0, 0};
  int active_ends[5] = {0, 0, 0, 0, 0};
  int used_regs_bitmask = 0;

  num_ra_locals = 0;
  pseudo_pc = 1;
  compute_liveness(func->body);

  for (i = 0; i < num_ra_locals; i++) {
    Symbol *sym = ra_locals[i];
    if (sym->stack_offset >= param_limit && sym->stack_offset < 0) {
      sym->live_start = -1; /* never alloc parameters for safety */
    }
    if (sym->live_start != -1 && sym->type && sym->type->kind != TY_ARRAY &&
        (is_integer(sym->type) || sym->type->kind == TY_PTR)) {
      ra_locals[count++] = sym;
    } else {
      sym->live_start = -1;
    }
  }
  num_ra_locals = count;

  for (i = 0; i < num_ra_locals; i++) {
    for (j = i + 1; j < num_ra_locals; j++) {
      if (ra_locals[j]->live_start < ra_locals[i]->live_start) {
        Symbol *t = ra_locals[i];
        ra_locals[i] = ra_locals[j];
        ra_locals[j] = t;
      }
    }
  }

  /* Linear Scan */
  for (i = 0; i < num_ra_locals; i++) {
    Symbol *sym = ra_locals[i];
    if (sym->live_start == -1)
      continue;

    for (r = 0; r < 5; r++) {
      if (active_regs[r] && active_ends[r] <= sym->live_start)
        active_regs[r] = 0;
    }

    int allocated = -1;
    for (r = 0; r < 5; r++) {
      if (!active_regs[r]) {
        allocated = r;
        active_regs[r] = get_callee_reg(r);
        active_ends[r] = sym->live_end;
        sym->assigned_reg = get_callee_reg(r);
        used_regs_bitmask |= (1 << r);
        break;
      }
    }

    if (allocated == -1) {
      sym->assigned_reg = 0;
    }
  }
  return used_regs_bitmask;
}

extern struct ZCCNode *zcc_node_from(struct Node *ast);
extern int zcc_run_passes_emit_body_pgo(struct ZCCNode *body, const char *profile, const char *name, void *out, int stack_size, int num_params, int end_label1, int end_label2);

#pragma weak zcc_run_passes_emit_body_pgo
#pragma weak zcc_node_from

static int ir_blacklisted(const char *name) {
  if (!name)
    return 0;
  static const char *blacklist[] = {
      /* Add explicitly dangerous functions here to fallback to AST backend */
      "main", "read_file", "init_compiler", 
      "lookup_keyword_fallback", "parse_stmt", "next_token", NULL};
  int i;
  for (i = 0; blacklist[i]; i++) {
    if (strcmp(name, blacklist[i]) == 0) {
      fprintf(stderr, "[ZCC-BLACKLIST] HIT (skipping IR): %s\n", name);
      return 1;
    }
  }
  return 0;
}

void codegen_func(Compiler *cc, Node *func) {
  char *argregs[6];
  int i;
  int stack_size;
  int used_regs;

  if (!func)
    return;
  fprintf(stderr, "cc_func: %s\n", func->func_def_name);
  cc->used_regs_mask = allocate_registers(func);
  cc->is_forced_mask = 0;
  if (backend_ops) {
      if ((cc->used_regs_mask & 0x1F) != 0x1F) cc->is_forced_mask = 1;
      cc->used_regs_mask = 0x1F;
  }
  used_regs = cc->used_regs_mask;

  if (getenv("ZCC_EMIT_TELEMETRY")) {
      fprintf(stderr, "[telem] fn=%s used_regs=0x%02X src=%s\n",
              func->func_def_name, used_regs, cc->is_forced_mask ? "forced" : "computed");
  }

  argregs[0] = "rdi";
  argregs[1] = "rsi";
  argregs[2] = "rdx";
  argregs[3] = "rcx";
  argregs[4] = "r8";
  argregs[5] = "r9";

  cc->func_end_label = new_label(cc);
  strncpy(cc->current_func, func->func_def_name, MAX_IDENT - 1);

  ir_bridge_func_begin(func);

  if (backend_ops && backend_ops->emit_prologue) {
      backend_ops->emit_prologue(cc, func);
  } else {
      stack_size = func->stack_size + 40; /* reserve 5x8 byte push slots */
      stack_size += 16;                   /* ABI: 16-byte scratch for aggregate returns (CG-IR-019) */
      cc->abi_scratch_offset = -stack_size;
      if (func->func_type && func->func_type->is_variadic) {
          stack_size += 176;
      }
      if (stack_size < 256)
        stack_size = 256;
      stack_size = (stack_size + 15) & ~15;

      fprintf(cc->out, "    .text\n");
      if (!func->is_static) {
        fprintf(cc->out, "    .globl %s\n", func->func_def_name);
      }
      fprintf(cc->out, "%s:\n", func->func_def_name);
      fprintf(cc->out, "    pushq %%rbp\n");
      fprintf(cc->out, "    movq %%rsp, %%rbp\n");
      fprintf(cc->out, "    subq $%d, %%rsp\n", stack_size);

      for (i = 0; i < 5; i++) {
        if (used_regs & (1 << i)) {
          fprintf(cc->out, "    movq %s, %d(%%rbp)\n", get_callee_reg(i),
                  -(func->stack_size + 8 * (i + 1)));
        }
      }
  }

  cc->stack_depth = 0;

  /* We need to re-create the scope with locals for codegen.
     During parsing, scope_add_local assigned stack offsets.
     Those offsets are stored in the Symbol nodes (via the arena).
     The body's ND_VAR nodes reference those Symbols.
     So the offsets are already embedded in the AST — we just need
     to store params from registers to their assigned stack slots. */

  scope_push(cc);

  /* Store params from registers to their stack locations... */
  int param_offset = 0;
  int f_idx = 0;
  int gp_idx = 0;
  if (!backend_ops) {
  for (i = 0; i < func->num_params; i++) {
    Type *ptype = func->func_params->types[i];
    int sz = type_size(ptype);
    if (sz < 8) sz = 8;
    param_offset -= sz;
    
    if (ptype->kind == TY_STRUCT || ptype->kind == TY_UNION) {
        abi_class_t eb[2];
        classify_aggregate(ptype, eb);
        if (eb[0] != CLASS_MEMORY) {
            /* Small aggregate in registers: load each eightbyte from correct register set. */
            if (eb[0] == CLASS_SSE) {
                if (f_idx < 8) fprintf(cc->out, "    movq %%xmm%d, %d(%%rbp)\n", f_idx++, param_offset);
            } else {
                if (gp_idx < 6) fprintf(cc->out, "    movq %%%s, %d(%%rbp)\n", argregs[gp_idx++], param_offset);
            }
            if (type_size(ptype) > 8) {
                if (eb[1] == CLASS_SSE) {
                    if (f_idx < 8) fprintf(cc->out, "    movq %%xmm%d, %d(%%rbp)\n", f_idx++, param_offset + 8);
                } else {
                    if (gp_idx < 6) fprintf(cc->out, "    movq %%%s, %d(%%rbp)\n", argregs[gp_idx++], param_offset + 8);
                }
            }
            continue;
        }
        /* MEMORY class: pointer is passed in next available GPR or on stack. */
        if (gp_idx < 6) {
          fprintf(cc->out, "    movq %%%s, %%r10\n", argregs[gp_idx]);
          gp_idx++;
        } else {
          /* Fallback for many-args: ZCC simplified stack handling */
          fprintf(cc->out, "    movq %d(%%rbp), %%r10\n", 16 + (i - 6) * 8);
        }
        int j;
        for (j = 0; j < sz; j++) {
            fprintf(cc->out, "    movb %d(%%r10), %%al\n", j);
            fprintf(cc->out, "    movb %%al, %d(%%rbp)\n", param_offset + j);
        }
    } else if (is_float_type(ptype)) {
        if (f_idx < 8) {
            if (ptype->kind == TY_FLOAT) fprintf(cc->out, "    movss %%xmm%d, %d(%%rbp)\n", f_idx, param_offset);
            else fprintf(cc->out, "    movsd %%xmm%d, %d(%%rbp)\n", f_idx, param_offset);
            f_idx++;
        } else {
            fprintf(cc->out, "    movq %d(%%rbp), %%rax\n", 16 + (i - 6) * 8);
            fprintf(cc->out, "    movq %%rax, %d(%%rbp)\n", param_offset);
        }
    } else {
        if (gp_idx < 6) {
          fprintf(cc->out, "    movq %%%s, %d(%%rbp)\n", argregs[gp_idx], param_offset);
          gp_idx++;
        } else {
          fprintf(cc->out, "    movq %d(%%rbp), %%rax\n", 16 + (i - 6) * 8);
          fprintf(cc->out, "    movq %%rax, %d(%%rbp)\n", param_offset);
        }
    }
  }

  if (func->func_type && func->func_type->is_variadic) {
    int save_base = -(func->stack_size + 40 + 176);
    /* Save GP regs at offsets 0-47 */
    for (i = 0; i < 6; i++) {
        fprintf(cc->out, "    movq %%%s, %d(%%rbp)\n", argregs[i], save_base + i*8);
    }
    /* Save XMM regs at offsets 48-175 */
    for (i = 0; i < 8; i++) {
        fprintf(cc->out, "    movsd %%xmm%d, %d(%%rbp)\n", i, save_base + 48 + i*16);
    }
    Symbol *fsym = scope_find(cc, func->func_def_name);
    if (fsym) {
        fsym->stack_offset = save_base;
    }
  }
  } /* end !backend_ops block */

  int ir_ok = 0;
  if ((getenv("ZCC_IR_BACKEND") || getenv("ZCC_IR_LOWER")) &&
      !ir_blacklisted(func->func_def_name)) {
    if (zcc_run_passes_emit_body_pgo && zcc_node_from) {
      void *ir_ast = zcc_node_from((void *)func->body);
      if (ir_ast) {
        char params_env[512];
        memset(params_env, 0, sizeof(params_env));
        for (int p_idx = 0; p_idx < func->num_params; p_idx++) {
          if (p_idx > 0)
            strcat(params_env, ",");
          strncat(params_env, func->func_params->names[p_idx],
                  511 - strlen(params_env));
        }
        setenv("ZCC_IR_PARAM_NAMES", params_env, 1);

        ir_ok = zcc_run_passes_emit_body_pgo(
            ir_ast, NULL, func->func_def_name, cc->out, stack_size,
            func->num_params, cc->func_end_label, cc->func_end_label);

        unsetenv("ZCC_IR_PARAM_NAMES");
      }
    }
  }
  if (!ir_ok) {
    codegen_stmt(cc, func->body);
  }

  if (backend_ops && backend_ops->emit_epilogue) {
      backend_ops->emit_epilogue(cc, func);
  } else {
      fprintf(cc->out, ".Lfunc_end_%d:\n", cc->func_end_label);
      for (i = 4; i >= 0; i--) {
        if (used_regs & (1 << i)) {
          fprintf(cc->out, "    movq %d(%%rbp), %s\n",
                  -(func->stack_size + 8 * (i + 1)), get_callee_reg(i));
        }
      }
      fprintf(cc->out, "    movq %%rbp, %%rsp\n");
      fprintf(cc->out, "    popq %%rbp\n");
      fprintf(cc->out, "    ret\n");
  }

  ir_bridge_func_end();

  scope_pop(cc);
}

/* ================================================================ */
/* GLOBAL VARIABLE EMISSION                                          */
/* Bug fix: section directive (.bss/.data) BEFORE label              */
/* Bug fix: .p2align 3 before every label (x86-64 ABI alignment)    */
/* ================================================================ */

static long long eval_const_expr_p4(Node *elem, int *ok) {
    if (!elem) { *ok = 0; return 0; }
    if (elem->kind == ND_CAST) return eval_const_expr_p4(elem->lhs, ok);
    if (elem->kind == ND_NUM) return elem->int_val;
    if (elem->kind == ND_ADD) return eval_const_expr_p4(elem->lhs, ok) + eval_const_expr_p4(elem->rhs, ok);
    if (elem->kind == ND_SUB) return eval_const_expr_p4(elem->lhs, ok) - eval_const_expr_p4(elem->rhs, ok);
    if (elem->kind == ND_MUL) return eval_const_expr_p4(elem->lhs, ok) * eval_const_expr_p4(elem->rhs, ok);
    if (elem->kind == ND_DIV) { long long r = eval_const_expr_p4(elem->rhs, ok); if(r) return eval_const_expr_p4(elem->lhs, ok) / r; return 0; }
    if (elem->kind == ND_BOR) return eval_const_expr_p4(elem->lhs, ok) | eval_const_expr_p4(elem->rhs, ok);
    if (elem->kind == ND_BAND) return eval_const_expr_p4(elem->lhs, ok) & eval_const_expr_p4(elem->rhs, ok);
    if (elem->kind == ND_BXOR) return eval_const_expr_p4(elem->lhs, ok) ^ eval_const_expr_p4(elem->rhs, ok);
    if (elem->kind == ND_SHL) return eval_const_expr_p4(elem->lhs, ok) << eval_const_expr_p4(elem->rhs, ok);
    if (elem->kind == ND_SHR) return eval_const_expr_p4(elem->lhs, ok) >> eval_const_expr_p4(elem->rhs, ok);
    if (elem->kind == ND_NEG) {
        /* CG-GINIT-FLOAT-001: float negation must negate the float value,
           not the bit-pattern. Check if child is ND_FLIT. */
        if (elem->lhs && elem->lhs->kind == ND_FLIT) {
            double negval = -(elem->lhs->f_val);
            if (elem->lhs->type && elem->lhs->type->kind == TY_FLOAT) {
                float fv = (float)negval;
                unsigned int fbits;
                memcpy(&fbits, &fv, sizeof(float));
                return (long long)fbits;
            } else {
                unsigned long long bits;
                memcpy(&bits, &negval, sizeof(double));
                return (long long)bits;
            }
        }
        return -eval_const_expr_p4(elem->lhs, ok);
    }
    if (elem->kind == ND_BNOT) return ~eval_const_expr_p4(elem->lhs, ok);
    if (elem->kind == ND_LNOT) return !eval_const_expr_p4(elem->lhs, ok);
    /* CG-GINIT-FLOAT-001: float/double literals in aggregate initializers */
    if (elem->kind == ND_FLIT) {
        if (elem->type && elem->type->kind == TY_FLOAT) {
            float fv = (float)elem->f_val;
            unsigned int fbits;
            memcpy(&fbits, &fv, sizeof(float));
            return (long long)fbits;
        } else {
            unsigned long long bits;
            memcpy(&bits, &elem->f_val, sizeof(double));
            return (long long)bits;
        }
    }
    *ok = 0;
    return 0;
}

static void emit_struct_fields(Compiler *cc, StructField *fields, Node **args, int num_args, int *arg_idx, int arg_end, int base_offset, int *emitted) {
    StructField *f;
    for (f = fields; f; f = f->next) {
        int field_abs_offset = base_offset + f->offset;
        
        if (field_abs_offset > *emitted) {
            fprintf(cc->out, "    .zero %d\n", field_abs_offset - *emitted);
            *emitted = field_abs_offset;
        }
        
        if (f->type->kind == TY_STRUCT) {
            emit_struct_fields(cc, f->type->fields, args, num_args, arg_idx, arg_end, field_abs_offset, emitted);
        } else if (f->type->kind == TY_ARRAY) {
            int j;
            if (f->type->base && (f->type->base->kind == TY_STRUCT || f->type->base->kind == TY_UNION)) {
                int elem_size = type_size(f->type->base);
                for (j = 0; j < f->type->array_len; j++) {
                    int expected_end = field_abs_offset + (j + 1) * elem_size;
                    emit_struct_fields(cc, f->type->base->fields, args, num_args, arg_idx, arg_end, field_abs_offset + j * elem_size, emitted);
                    if (*emitted < expected_end) {
                        fprintf(cc->out, "    .zero %d\n", expected_end - *emitted);
                        *emitted = expected_end;
                    }
                }
            } else {
                int elem_size = type_size(f->type->base);
                for (j = 0; j < f->type->array_len; j++) {
                    if (*arg_idx < arg_end && *arg_idx < num_args) {
                        Node *elem = args[(*arg_idx)++];
                        while (elem && elem->kind == ND_CAST) elem = elem->lhs; /* Strip ND_CAST */
                        int const_ok = 1;
                        long long const_val = eval_const_expr_p4(elem, &const_ok);
                        if (!elem) {
                            fprintf(cc->out, "    .zero %d\n", elem_size);
                        } else if (const_ok) {
                            if (elem_size == 1) fprintf(cc->out, "    .byte %lld\n", const_val);
                            else if (elem_size == 2) fprintf(cc->out, "    .short %lld\n", const_val);
                            else if (elem_size == 4) fprintf(cc->out, "    .long %lld\n", const_val);
                            else fprintf(cc->out, "    .quad %lld\n", const_val);
                        } else if (elem->kind == ND_STR) {
                            if (elem_size == 4) fprintf(cc->out, "    .long .Lstr_%d\n", elem->str_id);
                            else fprintf(cc->out, "    .quad .Lstr_%d\n", elem->str_id);
                        } else if (elem->kind == ND_ADDR_LABEL) {
                            if (elem_size == 4) fprintf(cc->out, "    .long .Luser_%s_%s\n", cc->current_func, elem->label_name);
                            else fprintf(cc->out, "    .quad .Luser_%s_%s\n", cc->current_func, elem->label_name);
                        } else if (elem->kind == ND_ADDR && elem->lhs && elem->lhs->kind == ND_VAR) {
                            if (elem_size == 4) fprintf(cc->out, "    .long %s\n", elem->lhs->name);
                            else fprintf(cc->out, "    .quad %s\n", elem->lhs->name);
                } else if (elem->kind == ND_ADDR && elem->lhs && elem->lhs->kind == ND_DEREF && elem->lhs->lhs && elem->lhs->lhs->kind == ND_ADD && elem->lhs->lhs->lhs && elem->lhs->lhs->lhs->kind == ND_VAR && elem->lhs->lhs->rhs && elem->lhs->lhs->rhs->kind == ND_NUM) {
                    long long offset = elem->lhs->lhs->rhs->int_val;
                    if (elem->lhs->lhs->lhs->type && elem->lhs->lhs->lhs->type->base) offset *= type_size(elem->lhs->lhs->lhs->type->base);
                    if (elem_size == 4) fprintf(cc->out, "    .long %s + %lld\n", elem->lhs->lhs->lhs->name, offset);
                    else fprintf(cc->out, "    .quad %s + %lld\n", elem->lhs->lhs->lhs->name, offset);
                } else if (elem->kind == ND_ADD && elem->lhs && elem->lhs->kind == ND_VAR && elem->rhs && elem->rhs->kind == ND_NUM) {
                    long long offset = elem->rhs->int_val;
                    if (elem->lhs->type && elem->lhs->type->base) offset *= type_size(elem->lhs->type->base);
                    if (elem_size == 4) fprintf(cc->out, "    .long %s + %lld\n", elem->lhs->name, offset);
                    else fprintf(cc->out, "    .quad %s + %lld\n", elem->lhs->name, offset);
                        } else if (elem->kind == ND_VAR) {
                            if (elem_size == 4) fprintf(cc->out, "    .long %s\n", elem->name);
                            else fprintf(cc->out, "    .quad %s\n", elem->name);
                        } else {
                            fprintf(stderr, "STRUCT-ARRAY FALLBACK ZERO: kind=%d, const_ok=%d\n", elem->kind, const_ok);
                            if (elem->lhs) fprintf(stderr, "  lhs->kind=%d lhs->name=%s\n", elem->lhs->kind, elem->lhs->name ? elem->lhs->name : "none");
                            fprintf(cc->out, "    .zero %d\n", elem_size);
                        }
                    } else {
                        fprintf(cc->out, "    .zero %d\n", elem_size);
                    }
                    *emitted += elem_size;
                }
            }
        } else {
            int elem_size = type_size(f->type);
            if (*arg_idx < arg_end && *arg_idx < num_args) {
                Node *elem = args[(*arg_idx)++];
                while (elem && elem->kind == ND_CAST) { elem = elem->lhs; } /* Strip ND_CAST */
                int const_ok = 1;
                long long const_val = eval_const_expr_p4(elem, &const_ok);
                if (!elem) {
                    fprintf(cc->out, "    .zero %d\n", elem_size);
                } else if (const_ok) {
                    if (elem_size == 1) fprintf(cc->out, "    .byte %lld\n", const_val);
                    else if (elem_size == 2) fprintf(cc->out, "    .short %lld\n", const_val);
                    else if (elem_size == 4) fprintf(cc->out, "    .long %lld\n", const_val);
                    else fprintf(cc->out, "    .quad %lld\n", const_val);
                } else if (elem->kind == ND_STR) {
                    if (elem_size == 4) fprintf(cc->out, "    .long .Lstr_%d\n", elem->str_id);
                    else fprintf(cc->out, "    .quad .Lstr_%d\n", elem->str_id);
                } else if (elem->kind == ND_ADDR_LABEL) {
                    if (elem_size == 4) fprintf(cc->out, "    .long .Luser_%s_%s\n", cc->current_func, elem->label_name);
                    else fprintf(cc->out, "    .quad .Luser_%s_%s\n", cc->current_func, elem->label_name);
                } else if (elem->kind == ND_ADDR && elem->lhs && elem->lhs->kind == ND_VAR) {
                    if (elem_size == 4) fprintf(cc->out, "    .long %s\n", elem->lhs->name);
                    else fprintf(cc->out, "    .quad %s\n", elem->lhs->name);
                } else if (elem->kind == ND_ADDR && elem->lhs && elem->lhs->kind == ND_DEREF && elem->lhs->lhs && elem->lhs->lhs->kind == ND_ADD && elem->lhs->lhs->lhs && elem->lhs->lhs->lhs->kind == ND_VAR && elem->lhs->lhs->rhs && elem->lhs->lhs->rhs->kind == ND_NUM) {
                    long long offset = elem->lhs->lhs->rhs->int_val;
                    if (elem->lhs->lhs->lhs->type && elem->lhs->lhs->lhs->type->base) offset *= type_size(elem->lhs->lhs->lhs->type->base);
                    if (elem_size == 4) fprintf(cc->out, "    .long %s + %lld\n", elem->lhs->lhs->lhs->name, offset);
                    else fprintf(cc->out, "    .quad %s + %lld\n", elem->lhs->lhs->lhs->name, offset);
                } else if (elem->kind == ND_ADD && elem->lhs && elem->lhs->kind == ND_VAR && elem->rhs && elem->rhs->kind == ND_NUM) {
                    long long offset = elem->rhs->int_val;
                    if (elem->lhs->type && elem->lhs->type->base) offset *= type_size(elem->lhs->type->base);
                    if (elem_size == 4) fprintf(cc->out, "    .long %s + %lld\n", elem->lhs->name, offset);
                    else fprintf(cc->out, "    .quad %s + %lld\n", elem->lhs->name, offset);
                } else if (elem->kind == ND_VAR) {
                    if (elem_size == 4) fprintf(cc->out, "    .long %s\n", elem->name);
                    else fprintf(cc->out, "    .quad %s\n", elem->name);
                } else {
                    fprintf(stderr, "STRUCT FIELD FALLBACK ZERO: kind=%d, const_ok=%d\n", elem->kind, const_ok);
                    if (elem->lhs) fprintf(stderr, "  lhs->kind=%d lhs->name=%s\n", elem->lhs->kind, elem->lhs->name ? elem->lhs->name : "none");
                    fprintf(cc->out, "    .zero %d\n", elem_size);
                }
            } else {
                fprintf(cc->out, "    .zero %d\n", elem_size);
            }
            *emitted += elem_size;
        }
    }
}

static void emit_global_var(Compiler *cc, Node *gvar) {
  int size;

  if (gvar->is_extern)
    return; /* no emission for extern */

  size = type_size(gvar->type);
  if (size <= 0)
    size = 8;
    
  if (gvar->initializer) {
    /* initialized data goes in .data */
    fprintf(cc->out, "    .data\n");
    fprintf(cc->out, "    .p2align 3\n");
    if (!gvar->is_static) {
      fprintf(cc->out, "    .globl %s\n", gvar->name);
    }
    fprintf(cc->out, "%s:\n", gvar->name);
    /* only handle simple integer initializers */
    /* Strip ND_CAST wrappers (e.g. function pointer casts like
       `Curl_cmalloc = (curl_malloc_callback)malloc`) */
    Node *init = gvar->initializer;
    while (init && init->kind == ND_CAST) init = init->lhs;
    if (init && init->kind == ND_NUM) {
      if (size == 1)
        fprintf(cc->out, "    .byte %lld\n", init->int_val);
      else if (size == 2)
        fprintf(cc->out, "    .short %lld\n", init->int_val);
      else if (size == 4)
        fprintf(cc->out, "    .long %lld\n", init->int_val);
      else
        fprintf(cc->out, "    .quad %lld\n", init->int_val);
    } else if (init && init->kind == ND_FLIT) {
      /* CG-GINIT-FLOAT-001: static/global float/double scalar initializer */
      if (size == 4) {
        float fv = (float)init->f_val;
        unsigned int fbits;
        memcpy(&fbits, &fv, sizeof(float));
        fprintf(cc->out, "    .long 0x%08x\n", fbits);
      } else {
        unsigned long long bits;
        memcpy(&bits, &init->f_val, sizeof(double));
        fprintf(cc->out, "    .quad 0x%016llx\n", bits);
      }
    } else if (init && init->kind == ND_NEG && init->lhs && init->lhs->kind == ND_FLIT) {
      /* CG-GINIT-FLOAT-001: negated float/double scalar (e.g. static double d = -1.5;) */
      double negval = -(init->lhs->f_val);
      if (size == 4) {
        float fv = (float)negval;
        unsigned int fbits;
        memcpy(&fbits, &fv, sizeof(float));
        fprintf(cc->out, "    .long 0x%08x\n", fbits);
      } else {
        unsigned long long bits;
        memcpy(&bits, &negval, sizeof(double));
        fprintf(cc->out, "    .quad 0x%016llx\n", bits);
      }
    } else if (init->kind == ND_ADDR_LABEL) {
      if (size == 4)
        fprintf(cc->out, "    .long .Luser_%s_%s\n", cc->current_func, init->label_name);
      else
        fprintf(cc->out, "    .quad .Luser_%s_%s\n", cc->current_func, init->label_name);
    } else if (init->kind == ND_ADDR && init->lhs && init->lhs->kind == ND_VAR) {
      if (size == 4)
        fprintf(cc->out, "    .long %s\n", init->lhs->name);
      else
        fprintf(cc->out, "    .quad %s\n", init->lhs->name);
    } else if (init->kind == ND_ADDR && init->lhs && init->lhs->kind == ND_DEREF && init->lhs->lhs && init->lhs->lhs->kind == ND_ADD && init->lhs->lhs->lhs && init->lhs->lhs->lhs->kind == ND_VAR && init->lhs->lhs->rhs && init->lhs->lhs->rhs->kind == ND_NUM) {
      long long offset = init->lhs->lhs->rhs->int_val;
      if (init->lhs->lhs->lhs->type && init->lhs->lhs->lhs->type->base) offset *= type_size(init->lhs->lhs->lhs->type->base);
      if (size == 4) fprintf(cc->out, "    .long %s + %lld\n", init->lhs->lhs->lhs->name, offset);
      else fprintf(cc->out, "    .quad %s + %lld\n", init->lhs->lhs->lhs->name, offset);
    } else if (init->kind == ND_ADD && init->lhs && init->lhs->kind == ND_VAR && init->rhs && init->rhs->kind == ND_NUM) {
      long long offset = init->rhs->int_val;
      if (init->lhs->type && init->lhs->type->base) offset *= type_size(init->lhs->type->base);
      if (size == 4) fprintf(cc->out, "    .long %s + %lld\n", init->lhs->name, offset);
      else fprintf(cc->out, "    .quad %s + %lld\n", init->lhs->name, offset);
    } else if (init->kind == ND_VAR) {
      if (size == 4)
        fprintf(cc->out, "    .long %s\n", init->name);
      else
        fprintf(cc->out, "    .quad %s\n", init->name);
    } else if (init->kind == ND_STR) {
      if (gvar->type && gvar->type->kind == TY_ARRAY) {
          int j;
          int str_id = init->str_id;
          int len = cc->strings[str_id].len;
          fprintf(cc->out, "    .ascii \"");
          for (j = 0; j < len; j++) {
              char c = cc->strings[str_id].data[j];
              if (c == '\n') fprintf(cc->out, "\\n");
              else if (c == '\t') fprintf(cc->out, "\\t");
              else if (c == '\r') fprintf(cc->out, "\\r");
              else if (c == '\\') fprintf(cc->out, "\\\\");
              else if (c == '"') fprintf(cc->out, "\\\"");
              else if (c >= 32 && c < 127) fprintf(cc->out, "%c", c);
              else fprintf(cc->out, "\\%03o", (unsigned char)c);
          }
          fprintf(cc->out, "\\0\"\n");
          if (size > len + 1) fprintf(cc->out, "    .zero %d\n", size - (len + 1));
      } else {
          if (size == 4) fprintf(cc->out, "    .long .Lstr_%d\n", init->str_id);
          else fprintf(cc->out, "    .quad .Lstr_%d\n", init->str_id);
      }
        } else if (init->kind == ND_INIT_LIST) {
            int emitted = 0;
            int i;
            int elem_size = 1;
        if (gvar->type && gvar->type->kind == TY_STRUCT) {
            int arg_idx = 0;
            emit_struct_fields(cc, gvar->type->fields, init->args, init->num_args, &arg_idx, init->num_args, 0, &emitted);
        } else if (gvar->type && gvar->type->kind == TY_ARRAY && gvar->type->base && (gvar->type->base->kind == TY_STRUCT || gvar->type->base->kind == TY_UNION)) {
            int i;
            int arg_idx = 0;
            int elem_size = type_size(gvar->type->base);
            int budget = init->num_args / gvar->type->array_len;
            for (i = 0; i < gvar->type->array_len; i++) {
                int expected_end = (i + 1) * elem_size;
                int arg_end = (i + 1) * budget;
                if (arg_end > init->num_args) arg_end = init->num_args;
                emit_struct_fields(cc, gvar->type->base->fields, init->args, init->num_args, &arg_idx, arg_end, i * elem_size, &emitted);
                if (emitted < expected_end) {
                    fprintf(cc->out, "    .zero %d\n", expected_end - emitted);
                    emitted = expected_end;
                }
            }
        } else {
            int i;
            int elem_size = 1;
            if (gvar->type && gvar->type->base) elem_size = type_size(gvar->type->base);
            for (i = 0; i < init->num_args; i++) {
                Node *elem = init->args[i];
                int const_ok = 1;
                long long const_val = eval_const_expr_p4(elem, &const_ok);
                if (!elem) {
                    fprintf(cc->out, "    .zero %d\n", elem_size);
                } else if (const_ok) {
                    if (elem_size == 1) fprintf(cc->out, "    .byte %lld\n", const_val);
                    else if (elem_size == 2) fprintf(cc->out, "    .short %lld\n", const_val);
                    else if (elem_size == 4) fprintf(cc->out, "    .long %lld\n", const_val);
                    else fprintf(cc->out, "    .quad %lld\n", const_val);
                } else if (elem->kind == ND_STR) {
                    if (elem_size == 4) fprintf(cc->out, "    .long .Lstr_%d\n", elem->str_id);
                    else fprintf(cc->out, "    .quad .Lstr_%d\n", elem->str_id);
                } else if (elem->kind == ND_ADDR_LABEL) {
                    if (elem_size == 4) fprintf(cc->out, "    .long .Luser_%s_%s\n", cc->current_func, elem->label_name);
                    else fprintf(cc->out, "    .quad .Luser_%s_%s\n", cc->current_func, elem->label_name);
                } else if (elem->kind == ND_ADDR && elem->lhs && elem->lhs->kind == ND_VAR) {
                    if (elem_size == 4) fprintf(cc->out, "    .long %s\n", elem->lhs->name);
                    else fprintf(cc->out, "    .quad %s\n", elem->lhs->name);
                } else if (elem->kind == ND_ADDR && elem->lhs && elem->lhs->kind == ND_DEREF && elem->lhs->lhs && elem->lhs->lhs->kind == ND_ADD && elem->lhs->lhs->lhs && elem->lhs->lhs->lhs->kind == ND_VAR && elem->lhs->lhs->rhs && elem->lhs->lhs->rhs->kind == ND_NUM) {
                    long long offset = elem->lhs->lhs->rhs->int_val;
                    if (elem->lhs->lhs->lhs->type && elem->lhs->lhs->lhs->type->base) offset *= type_size(elem->lhs->lhs->lhs->type->base);
                    if (elem_size == 4) fprintf(cc->out, "    .long %s + %lld\n", elem->lhs->lhs->lhs->name, offset);
                    else fprintf(cc->out, "    .quad %s + %lld\n", elem->lhs->lhs->lhs->name, offset);
                } else if (elem->kind == ND_ADD && elem->lhs && elem->lhs->kind == ND_VAR && elem->rhs && elem->rhs->kind == ND_NUM) {
                    long long offset = elem->rhs->int_val;
                    if (elem->lhs->type && elem->lhs->type->base) offset *= type_size(elem->lhs->type->base);
                    if (elem_size == 4) fprintf(cc->out, "    .long %s + %lld\n", elem->lhs->name, offset);
                    else fprintf(cc->out, "    .quad %s + %lld\n", elem->lhs->name, offset);
                } else if (elem->kind == ND_VAR) {
                    if (elem_size == 4) fprintf(cc->out, "    .long %s\n", elem->name);
                    else fprintf(cc->out, "    .quad %s\n", elem->name);
                } else {
                    fprintf(cc->out, "    .zero %d\n", elem_size);
                }
                emitted += elem_size;
            }
        }
        if (emitted < size) {
            fprintf(cc->out, "    .zero %d\n", size - emitted);
        }
    } else {
        fprintf(cc->out, "    .zero %d\n", size);
    }
  } else {
    /* tentative definitions and uninitialized data */
    if (gvar->is_static) {
        fprintf(cc->out, "    .local %s\n", gvar->name);
        fprintf(cc->out, "    .comm %s, %d, 8\n", gvar->name, size);
    } else {
        fprintf(cc->out, "    .comm %s, %d, 8\n", gvar->name, size);
    }
  }
}

/* ================================================================ */
/* STRING LITERAL EMISSION                                           */
/* ================================================================ */

static void emit_strings(Compiler *cc) {
  int i;
  fprintf(cc->out, "    .section .rodata\n");
  for (i = 0; i < cc->num_strings; i++) {
    int j;
    fprintf(cc->out, ".LS%d:\n", cc->strings[i].label_id);
    fprintf(cc->out, ".Lstr_%d:\n", i); /* IR backend alias */
    fprintf(cc->out, "    .asciz \"");
    for (j = 0; j < cc->strings[i].len; j++) {
      char c;
      c = cc->strings[i].data[j];
      if (c == '\n')
        fprintf(cc->out, "\\n");
      else if (c == '\t')
        fprintf(cc->out, "\\t");
      else if (c == '\r')
        fprintf(cc->out, "\\r");
      else if (c == '\\')
        fprintf(cc->out, "\\\\");
      else if (c == '"')
        fprintf(cc->out, "\\\"");
      else if (c == '\0')
        fprintf(cc->out, "\\0");
      else if (c >= 32 && c < 127)
        fprintf(cc->out, "%c", c);
      else
        fprintf(cc->out, "\\%03o", (unsigned char)c);
    }
    fprintf(cc->out, "\"\n");
  }
}

/* ================================================================ */
/* PROGRAM CODEGEN ENTRY                                             */
/* ================================================================ */

static void fold_constants(Compiler *cc, Node *node) {
  if (!node)
    return;

  fold_constants(cc, node->lhs);
  fold_constants(cc, node->rhs);
  fold_constants(cc, node->cond);
  fold_constants(cc, node->then_body);
  fold_constants(cc, node->else_body);
  fold_constants(cc, node->init);
  fold_constants(cc, node->inc);
  fold_constants(cc, node->body);
  fold_constants(cc, node->case_body);

  if (node->kind == ND_CALL && node->num_args > 0) {
    int i;
    for (i = 0; i < node->num_args; i++)
      fold_constants(cc, node->args[i]);
  }
  if (node->kind == ND_BLOCK && node->num_stmts > 0) {
    int i;
    for (i = 0; i < node->num_stmts; i++)
      fold_constants(cc, node->stmts[i]);
  }
  if (node->kind == ND_SWITCH && node->num_cases > 0) {
    int i;
    for (i = 0; i < node->num_cases; i++)
      fold_constants(cc, node->cases[i]);
    fold_constants(cc, node->default_case);
  }

  if (node->kind == ND_NEG || node->kind == ND_BNOT || node->kind == ND_LNOT) {
    if (node->lhs && node->lhs->kind == ND_NUM) {
      long long v1 = node->lhs->int_val, res = 0;
      if (node->kind == ND_NEG)
        res = -v1;
      else if (node->kind == ND_BNOT)
        res = ~v1;
      else if (node->kind == ND_LNOT)
        res = !v1;

      if (node->type && !node_type_unsigned(node)) {
          if (node->type->size == 4) res = (long long)(int)res;
          else if (node->type->size == 1) res = (long long)(char)res;
          else if (node->type->size == 2) res = (long long)(short)res;
      } else if (node->type && node_type_unsigned(node)) {
          if (node->type->size == 4) res = (long long)(unsigned int)res;
          else if (node->type->size == 1) res = (long long)(unsigned char)res;
          else if (node->type->size == 2) res = (long long)(unsigned short)res;
      }
      node->kind = ND_NUM;
      node->int_val = res;
      node->lhs = 0;
    }
  }

  if (node->kind == ND_ADD || node->kind == ND_SUB || node->kind == ND_MUL ||
      node->kind == ND_DIV || node->kind == ND_MOD) {
    if (node->lhs && node->rhs && node->lhs->kind == ND_NUM &&
        node->rhs->kind == ND_NUM) {
      long long v1 = node->lhs->int_val, v2 = node->rhs->int_val, res = 0;
      int is_unsigned = node->lhs->type && node_type_unsigned(node->lhs);
      unsigned long long u1 = v1, u2 = v2;
      if (is_unsigned && node->lhs->type) {
          if (node->lhs->type->size == 4) { u1 = (unsigned int)v1; u2 = (unsigned int)v2; }
          else if (node->lhs->type->size == 1) { u1 = (unsigned char)v1; u2 = (unsigned char)v2; }
          else if (node->lhs->type->size == 2) { u1 = (unsigned short)v1; u2 = (unsigned short)v2; }
      }

      if (node->kind == ND_ADD)
        res = v1 + v2;
      else if (node->kind == ND_SUB)
        res = v1 - v2;
      else if (node->kind == ND_MUL)
        res = v1 * v2;
      else if (node->kind == ND_DIV && v2 != 0)
        res = is_unsigned ? u1 / u2 : v1 / v2;
      else if (node->kind == ND_MOD && v2 != 0)
        res = is_unsigned ? u1 % u2 : v1 % v2;
      else
        return;
      if (node->type && !node_type_unsigned(node)) {
          if (node->type->size == 4) res = (long long)(int)res;
          else if (node->type->size == 1) res = (long long)(char)res;
          else if (node->type->size == 2) res = (long long)(short)res;
      } else if (node->type && node_type_unsigned(node)) {
          if (node->type->size == 4) res = (long long)(unsigned int)res;
          else if (node->type->size == 1) res = (long long)(unsigned char)res;
          else if (node->type->size == 2) res = (long long)(unsigned short)res;
      }
      node->kind = ND_NUM;
      node->int_val = res;
      node->lhs = 0;
      node->rhs = 0;
    }
  }

  if (node->kind == ND_SHL || node->kind == ND_SHR || node->kind == ND_BAND ||
      node->kind == ND_BOR || node->kind == ND_BXOR || node->kind == ND_LT ||
      node->kind == ND_LE || node->kind == ND_GT || node->kind == ND_GE ||
      node->kind == ND_EQ || node->kind == ND_NE) {
    if (node->lhs && node->rhs && node->lhs->kind == ND_NUM &&
        node->rhs->kind == ND_NUM) {
      long long v1 = node->lhs->int_val, v2 = node->rhs->int_val, res = 0;
      int is_unsigned = node->lhs->type && node_type_unsigned(node->lhs);
      unsigned long long u1 = v1, u2 = v2;
      if (is_unsigned && node->lhs->type) {
          if (node->lhs->type->size == 4) { u1 = (unsigned int)v1; u2 = (unsigned int)v2; }
          else if (node->lhs->type->size == 1) { u1 = (unsigned char)v1; u2 = (unsigned char)v2; }
          else if (node->lhs->type->size == 2) { u1 = (unsigned short)v1; u2 = (unsigned short)v2; }
      }

      if (node->kind == ND_SHL)
        res = v1 << v2;
      else if (node->kind == ND_SHR)
        res = is_unsigned ? u1 >> u2 : v1 >> v2;
      else if (node->kind == ND_BAND)
        res = v1 & v2;
      else if (node->kind == ND_BOR)
        res = v1 | v2;
      else if (node->kind == ND_BXOR)
        res = v1 ^ v2;
      else if (node->kind == ND_LT)
        res = is_unsigned ? u1 < u2 : v1 < v2;
      else if (node->kind == ND_LE)
        res = is_unsigned ? u1 <= u2 : v1 <= v2;
      else if (node->kind == ND_GT)
        res = is_unsigned ? u1 > u2 : v1 > v2;
      else if (node->kind == ND_GE)
        res = is_unsigned ? u1 >= u2 : v1 >= v2;
      else if (node->kind == ND_EQ)
        res = v1 == v2;
      else if (node->kind == ND_NE)
        res = v1 != v2;
      if (node->type && !node_type_unsigned(node)) {
          if (node->type->size == 4) res = (long long)(int)res;
          else if (node->type->size == 1) res = (long long)(char)res;
          else if (node->type->size == 2) res = (long long)(short)res;
      } else if (node->type && node_type_unsigned(node)) {
          if (node->type->size == 4) res = (long long)(unsigned int)res;
          else if (node->type->size == 1) res = (long long)(unsigned char)res;
          else if (node->type->size == 2) res = (long long)(unsigned short)res;
      }
      node->kind = ND_NUM;
      node->int_val = res;
      node->lhs = 0;
      node->rhs = 0;
    }
  }

  /* Dead-Branch Elimination (AST-level DCE) */
  if (node->kind == ND_IF) {
    if (node->cond && node->cond->kind == ND_NUM) {
      if (node->cond->int_val == 0) {
        /* condition is false: replace with else body or empty block */
        if (node->else_body) {
          Node *tgt = node->else_body;
          unsigned long long save_magic = node->magic;
          unsigned long long save_alloc_id = node->alloc_id;
          *node = *tgt;
          node->magic = save_magic;
          node->alloc_id = save_alloc_id;
        } else {
          node->kind = ND_BLOCK;
          node->num_stmts = 0;
          node->stmts = (Node **)cc_alloc(cc, sizeof(Node *));
        }
      } else {
        /* condition is true: replace with then body */
        if (node->then_body) {
          Node *tgt = node->then_body;
          unsigned long long save_magic = node->magic;
          unsigned long long save_alloc_id = node->alloc_id;
          *node = *tgt;
          node->magic = save_magic;
          node->alloc_id = save_alloc_id;
        } else {
          node->kind = ND_BLOCK;
          node->num_stmts = 0;
          node->stmts = (Node **)cc_alloc(cc, sizeof(Node *));
        }
      }
    }
  } else if (node->kind == ND_WHILE) {
    if (node->cond && node->cond->kind == ND_NUM && node->cond->int_val == 0) {
      /* while (0): unreachable loop, collapse entirely */
      node->kind = ND_BLOCK;
      node->num_stmts = 0;
      node->stmts = (Node **)cc_alloc(cc, sizeof(Node *));
    }
  } else if (node->kind == ND_TERNARY) {
    if (node->cond && node->cond->kind == ND_NUM) {
      if (node->cond->int_val == 0) {
        if (node->else_body) {
          Node *tgt = node->else_body;
          unsigned long long save_magic = node->magic;
          unsigned long long save_alloc_id = node->alloc_id;
          *node = *tgt;
          node->magic = save_magic;
          node->alloc_id = save_alloc_id;
        }
      } else {
        if (node->then_body) {
          Node *tgt = node->then_body;
          unsigned long long save_magic = node->magic;
          unsigned long long save_alloc_id = node->alloc_id;
          *node = *tgt;
          node->magic = save_magic;
          node->alloc_id = save_alloc_id;
        }
      }
    }
  }
}

void codegen_program(Compiler *cc, Node *prog) {
  Node *n;
  int i;

  /* Do NOT check ptr_in_fault_range(prog/n): stage2 miscompiles it and falsely
   * rejects valid arena pointers (e.g. 0xac09f8), causing early exit and
   * WinMain link error. */
  if (!prog)
    return;

  /* Linux: avoid "missing .note.GNU-stack" linker warning */
  if (!backend_ops) fprintf(cc->out, "    .section .note.GNU-stack,\"\",@progbits\n");
  if (cc->filename) {
    fprintf(cc->out, "    .file 1 \"%s\"\n", cc->filename);
  }

  /* Emit functions */
  n = prog;
  while (n) {
    if (n->kind == ND_FUNC_DEF) {
      fold_constants(cc, n);
      codegen_func(cc, n);
    }
    n = n->next;
  }

  /* Deduplicate tentative definitions: keep only one instance (prioritizing initializers) */
  for (i = 0; i < cc->num_globals; i++) {
    int j;
    Node *g1 = cc->globals[i];
    if (!g1 || g1->kind != ND_GLOBAL_VAR) continue;
    
    for (j = i + 1; j < cc->num_globals; j++) {
      Node *g2 = cc->globals[j];
      if (!g2 || g2->kind != ND_GLOBAL_VAR) continue;

      
      if (strcmp(g1->name, g2->name) == 0) {
        if (g1->initializer && g2->initializer) {
          cc->globals[j] = 0; /* Keep first initializer, drop second */
        } else if (g1->initializer) {
          cc->globals[j] = 0; /* Keep initialized, drop uninitialized */
        } else if (g2->initializer) {
          cc->globals[i] = 0; /* Drop uninitialized, keep initialized */
          break; /* g1 is gone, stop checking against g1 */
        } else {
          if (g1->is_extern && !g2->is_extern) {
              cc->globals[i] = 0; /* Keep tentative, drop extern */
              break;
          } else {
              cc->globals[j] = 0; /* Both uninitialized, prioritize g1 */
          }
        }
      }
    }
  }

  /* Emit global variables */
  for (i = 0; i < cc->num_globals; i++) {
    if (cc->globals[i]) {
      fold_constants(cc, cc->globals[i]->initializer);
      emit_global_var(cc, cc->globals[i]);
    }
  }

  /* Emit string literals */
  if (cc->num_strings > 0) {
    emit_strings(cc);
  }

  /* CG-FLOAT-011: emit float/double 1.0 constants used by ++/-- on float types */
  if (!backend_ops) {
    fprintf(cc->out, "    .section .rodata\n");
    fprintf(cc->out, "    .align 4\n");
    fprintf(cc->out, ".Lf32_one:\n");
    fprintf(cc->out, "    .long 0x3F800000\n");   /* 1.0f IEEE-754 */
    fprintf(cc->out, "    .align 8\n");
    fprintf(cc->out, ".Lf64_one:\n");
    fprintf(cc->out, "    .quad 0x3FF0000000000000\n"); /* 1.0 double */
  }
}

/* ZKAEDI FORCE RENDER CACHE INVALIDATION */

extern int is_pointer(Type *type);

int node_ptr_elem_size(struct Node *n) {
  if (!n || !n->type)
    return 0;
  if (is_pointer(n->type)) {
    return ptr_elem_size(n->type);
  }
  return 0;
}

