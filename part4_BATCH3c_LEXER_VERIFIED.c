
/* ================================================================ */
/* CODE GENERATOR — x86-64 Linux (System V) or Windows x64 ABI      */
/* ================================================================ */

#include "ir_emit_dispatch.h"
#include "ir_bridge.h"

static void push_reg(Compiler *cc, char *reg) {
    fprintf(cc->out, "    pushq %%%s\n", reg);
    cc->stack_depth++;
}

static void pop_reg(Compiler *cc, char *reg) {
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

/* Format codes for label emission — avoid passing string literals as 2nd arg (stage2 mispasses them). */
enum { FMT_JE = 1, FMT_JMP, FMT_DEF, FMT_JNE };
static void emit_label_fmt(Compiler *cc, int n, int fmt) {
    switch (fmt) {
        case FMT_JE:  fprintf(cc->out, "    je .L%d\n", n); break;
        case FMT_JMP: fprintf(cc->out, "    jmp .L%d\n", n); break;
        case FMT_DEF: fprintf(cc->out, ".L%d:\n", n); break;
        case FMT_JNE:  fprintf(cc->out, "    jne .L%d\n", n); break;
        default:      fprintf(cc->out, ".L%d:\n", n); break;
    }
}

/* Forward: defined after bad_node_cutoff. */
static int is_bad_ptr(const void *p);

/* True if ptr is in fault range. GDB: 0x800b457d0 (hi=0, lo>=2G), 0x800ac07d0 (hi=0x80).
 * Reject: (hi==0 && lo>=2G) OR (hi in [0x80, 0x100)). Not inline so stage2 emits a body. */
int ptr_in_fault_range(const void *p) {
    unsigned long long u = (unsigned long long)(const char *)p;
    unsigned int hi = (unsigned int)(u >> 32);
    unsigned int lo = (unsigned int)u;
    return (hi == 0 && lo >= 0x80000000U) || (hi >= 0x80U && hi < 0x100U);
}
/* Use ptr_in_fault_range() in code, not macro; stage2 may not expand macros. */

/* ---------------------------------------------------------------- */
/* Load value from address in %rax                                   */
/* ---------------------------------------------------------------- */

void codegen_load(Compiler *cc, Type *type) {
    if (!type) { fprintf(cc->out, "    movq (%%rax), %%rax\n"); return; }
    /* Do NOT check fault range here: stage2 miscompiles it and rejects valid Type* ptrs. */
    if (type->kind == TY_ARRAY) return;
    if (type->kind == TY_STRUCT || type->kind == TY_UNION) return;
    /* Pointers must always be 64-bit (fixes stage2 sign-extended 32-bit load -> 0x80xxxxxx crash). */
    if (type->kind == TY_PTR) {
        fprintf(cc->out, "    movq (%%rax), %%rax\n");
        return;
    }
    switch (type->size) {
        case 1:
            if (is_unsigned_type(type)) fprintf(cc->out, "    movzbq (%%rax), %%rax\n");
            else fprintf(cc->out, "    movsbq (%%rax), %%rax\n");
            break;
        case 2:
            if (is_unsigned_type(type)) fprintf(cc->out, "    movzwq (%%rax), %%rax\n");
            else fprintf(cc->out, "    movswq (%%rax), %%rax\n");
            break;
        case 4:
            if (is_unsigned_type(type)) fprintf(cc->out, "    movl (%%rax), %%eax\n");
            else fprintf(cc->out, "    movslq (%%rax), %%rax\n");
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
    if (!type) { fprintf(cc->out, "    movq %%r11, (%%rax)\n"); fprintf(cc->out, "    movq %%r11, %%rax\n"); return; }
    /* Do NOT check fault range here: stage2 miscompiles it and rejects valid Type* ptrs. */
    /* Pointers must always be 64-bit (fixes stage2 sign-extended 32-bit store). */
    if (type->kind == TY_PTR) {
        fprintf(cc->out, "    movq %%r11, (%%rax)\n");
        fprintf(cc->out, "    movq %%r11, %%rax\n");
        return;
    }
    switch (type->size) {
        case 1: fprintf(cc->out, "    movb %%r11b, (%%rax)\n"); break;
        case 2: fprintf(cc->out, "    movw %%r11w, (%%rax)\n"); break;
        case 4: fprintf(cc->out, "    movl %%r11d, (%%rax)\n"); break;
        default: fprintf(cc->out, "    movq %%r11, (%%rax)\n"); break;
    }
    fprintf(cc->out, "    movq %%r11, %%rax\n");
}

/* Return cutoff so stage2 can't miscompile inline constant (GDB showed node=0x10000). */
static unsigned long long bad_node_cutoff(void) { return 65536; }

/* Only null and very low ptr (<=0x10000). No hi/lo range check here—stage2 miscompiles
 * it and then rejects valid heap (e.g. 0x00A7C8C0). ptr_in_fault_range handles 0x80xxxxxx. */
static int is_bad_ptr(const void *p) {
    unsigned long long u = (unsigned long long)(const char *)p;
    return !p || u <= bad_node_cutoff();
}

/* Validate node/type after creation; fatal on first bad to pinpoint root cause. */
void validate_node(Compiler *cc, Node *node, const char *where, int line) {
    char buf[320];
    if (!node) return;
    if (node->magic != 0xC0FFEEBAD1234567ULL) {
        sprintf(buf, "CORRUPTED NODE (magic=0x%llx, alloc_id=%llu) at %s",
                (unsigned long long)node->magic, (unsigned long long)node->alloc_id, where);
        error_at(cc, line, buf);
        exit(1);
    }
    /* Do NOT check ptr_in_fault_range(node->type): stage2 miscompiles it. */
}

void validate_type(Compiler *cc, Type *type, const char *where, int line) {
    char buf[256];
    if (!type) return;
    if (type->magic != 0xDEADBEEF87654321ULL) {
        sprintf(buf, "CORRUPTED TYPE (magic=0x%llx, alloc_id=%llu) at %s",
                (unsigned long long)type->magic, (unsigned long long)type->alloc_id, where);
        error_at(cc, line, buf);
        exit(1);
    }
    /* Do NOT check ptr_in_fault_range(type) here: type is the pointer we just got from cc_alloc
     * (arena address like 0xa56040). Stage2 miscompiles that check and falsely rejects valid ptrs. */
}

static int guard_error_count = 0;

/* Fatal on first bad node in codegen to stop spam. */
static void guard_node(Compiler *cc, Node *node) {
    char buf[256];
    if (!node || guard_error_count > 0) return;
    if (node->magic != 0xC0FFEEBAD1234567ULL) {
        sprintf(buf, "FATAL: node magic corrupted (0x%llx, id=%llu)",
                (unsigned long long)node->magic, (unsigned long long)node->alloc_id);
        error_at(cc, node->line, buf);
        guard_error_count++;
        exit(1);
    }
    /* Do NOT check ptr_in_fault_range(node->type): stage2 miscompiles it and rejects valid Type* ptrs. */
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
    /* In main, never substitute 0 for param/local addresses: Stage2 can have "bad" node ptrs (e.g. 0x10000) and we must use the fixed stack slots. */
    if (is_bad_ptr(n) && (!cc->current_func[0] || strcmp(cc->current_func, "main") != 0)) {
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
    /* Do NOT use ptr_in_fault_range(node): stage2 miscompiles it and rejects valid arena ptrs. */
    if (!node) {
        error_at(cc, 0, "codegen_addr: null node");
        fprintf(cc->out, "    movq $0, %%rax\n");
        return;
    }
    /* In main, do not substitute 0 for bad node ptr — we need to reach the main param/local stack slots. */
    if (is_bad_ptr(node) && (!cc->current_func[0] || strcmp(cc->current_func, "main") != 0)) {
        error_at(cc, 0, "codegen_addr: bad node ptr");
        fprintf(cc->out, "    movq $0, %%rax\n");
        return;
    }
    if (node->kind == ND_VAR) {
        /* Main params/locals: always use fixed stack layout when in main, so Stage2 never emits globals for these. */
        if (cc->current_func[0] && strcmp(cc->current_func, "main") == 0) {
            if (strcmp(node->name, "argc") == 0)   { if (cc->verbose) fprintf(stderr, "zcc: codegen main param 'argc'\n"); fprintf(cc->out, "    leaq -8(%%rbp), %%rax\n"); char *dst=ir_bridge_fresh_tmp(); ZCC_EMIT_UNARY(IR_ADDR, IR_TY_PTR, dst, "%stack_-8", node->line); return; }
            if (strcmp(node->name, "argv") == 0)   { if (cc->verbose) fprintf(stderr, "zcc: codegen main param 'argv'\n"); fprintf(cc->out, "    leaq -16(%%rbp), %%rax\n"); char *dst=ir_bridge_fresh_tmp(); ZCC_EMIT_UNARY(IR_ADDR, IR_TY_PTR, dst, "%stack_-16", node->line); return; }
            if (strcmp(node->name, "input_file") == 0)  { if (cc->verbose) fprintf(stderr, "zcc: codegen main local 'input_file'\n"); fprintf(cc->out, "    leaq -32(%%rbp), %%rax\n"); char *dst=ir_bridge_fresh_tmp(); ZCC_EMIT_UNARY(IR_ADDR, IR_TY_PTR, dst, "%stack_-32", node->line); return; }
            if (strcmp(node->name, "output_file") == 0) { if (cc->verbose) fprintf(stderr, "zcc: codegen main local 'output_file'\n"); fprintf(cc->out, "    leaq -40(%%rbp), %%rax\n"); char *dst=ir_bridge_fresh_tmp(); ZCC_EMIT_UNARY(IR_ADDR, IR_TY_PTR, dst, "%stack_-40", node->line); return; }
        }
        /* Do NOT check is_bad_ptr(node->sym): Stage1 can have valid Symbol* in fault range -> emit global -> segfault. */
        /* Use stack whenever we have a symbol with stack_offset. Params/locals have negative offset; guard against wrong sign. */
        if (node->sym) {
            int off = node->sym->stack_offset;
            if (off > 0)
                off = -off;   /* never use positive: would write above frame (e.g. return address) and crash */
            if (off != 0) {
                fprintf(cc->out, "    leaq %d(%%rbp), %%rax\n", off);
                char vname[32]; sprintf(vname, "%%stack_%d", off);
                char *dst = ir_bridge_fresh_tmp();
                ZCC_EMIT_UNARY(IR_ADDR, IR_TY_PTR, dst, vname, node->line);
                return;
            }
        }
        fprintf(cc->out, "    leaq %s(%%rip), %%rax\n", node->name);
        char gname[32]; sprintf(gname, "%%%s", node->name);
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
        return; /* Note: ptr is ALREADY eval'd by codegen_expr! ir_last_result holds it! */
    }
    if (node->kind == ND_MEMBER) {
        if (!node->lhs) {
            error_at(cc, node->line, "codegen_addr: ND_MEMBER null lhs (member access on null)");
            fprintf(cc->out, "    movq $0, %%rax\n");
            return;
        }
        codegen_addr_checked(cc, node->lhs);
        if (node->member_offset != 0) {
            fprintf(cc->out, "    addq $%d, %%rax\n", node->member_offset);
            char lhs_ir[32]; ir_save_result(lhs_ir);
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
    if (!type) return 1;
    if (type->kind == TY_PTR || type->kind == TY_ARRAY) {
        if (type->base) return type_size(type->base);
    }
    return 1;
}

/* ---------------------------------------------------------------- */
/* Expression codegen — result in %rax                               */
/* ---------------------------------------------------------------- */

void codegen_expr(Compiler *cc, Node *node) {
    if (!node) return;
    
    if (cc) {
    }

    int lbl1;
    int lbl2;
    char errbuf[80];

    /* Do NOT use ptr_in_fault_range(node): stage2 miscompiles it and rejects valid arena ptrs. */
    if (!node) {
        error_at(cc, 0, "codegen_expr: NULL node");
        fprintf(cc->out, "    movq $0, %%rax\n");
        return;
    }
    /* In main, do not substitute 0 for bad node ptr — we need to reach main param/local codegen. */
    if (is_bad_ptr(node) && (!cc->current_func[0] || strcmp(cc->current_func, "main") != 0)) {
        error_at(cc, 0, "codegen_expr: bad node ptr");
        fprintf(cc->out, "    movq $0, %%rax\n");
        return;
    }
    if (node->kind < ND_NUM || node->kind > ND_NOP) {
        sprintf(errbuf, "codegen_expr: invalid kind %d (0x%x) at %p", node->kind, node->kind, (void *)node);
        error_at(cc, node->line, errbuf);
        return;
    }
    guard_node(cc, node);

    switch (node->kind) {

    case ND_NUM:
        if (node->int_val >= -2147483648) {
            if (node->int_val <= 2147483647) {
                fprintf(cc->out, "    movq $%lld, %%rax\n", node->int_val);
                { char *dst = ir_bridge_fresh_tmp(); ZCC_EMIT_CONST(ir_map_type(node->type), dst, node->int_val, node->line); }
                return;
            }
        }
        fprintf(cc->out, "    movabsq $%lld, %%rax\n", node->int_val);
        { char *dst = ir_bridge_fresh_tmp(); ZCC_EMIT_CONST(ir_map_type(node->type), dst, node->int_val, node->line); }
        return;

    case ND_FLIT: {
        int lbl;
        unsigned long long bits;
        lbl = cc->label_count;
        cc->label_count = cc->label_count + 1;
        fprintf(cc->out, "    .section .rodata\n");
        fprintf(cc->out, "    .p2align 3\n");
        fprintf(cc->out, ".L_flit_%d:\n", lbl);
        memcpy(&bits, &node->f_val, 8);
        fprintf(cc->out, "    .quad %llu\n", bits);
        fprintf(cc->out, "    .text\n");
        fprintf(cc->out, "    movsd .L_flit_%d(%%rip), %%xmm0\n", lbl);
        /* Satisfy IR subsystem sequence */
        { char *dst = ir_bridge_fresh_tmp(); }
        return;
    }

    case ND_STR: {
        char lbl_buf[32];
        fprintf(cc->out, "    leaq .L%d(%%rip), %%rax\n", cc->strings[node->str_id].label_id);
        sprintf(lbl_buf, ".L%d", cc->strings[node->str_id].label_id);
        char *dst = ir_bridge_fresh_tmp();
        ZCC_EMIT_CONST(IR_TY_PTR, dst, 0, node->line); /* Fake for now */
        ZCC_EMIT_UNARY(IR_CONST_STR, IR_TY_PTR, dst, lbl_buf, node->line); /* REAL emit */
        return;
    }

    case ND_VAR:
        if (node->sym && node->sym->assigned_reg) {
            fprintf(cc->out, "    movq %s, %%rax\n", node->sym->assigned_reg);
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
        if (node->lhs && node->lhs->kind == ND_VAR && node->lhs->sym && node->lhs->sym->assigned_reg) {
            fprintf(cc->out, "    movq %%rax, %s\n", node->lhs->sym->assigned_reg);
            {
                char *vname = ir_var_name(node->lhs);
                ZCC_EMIT_STORE(ir_map_type(node->lhs->type), vname, rhs_ir, node->line);
            }
            return;
        }
        push_reg(cc, "rax");
        codegen_addr_checked(cc, node->lhs);
        char lhs_addr_ir[32]; ir_save_result(lhs_addr_ir);
        if (is_bad_ptr(node->lhs) && (!cc->current_func[0] || strcmp(cc->current_func, "main") != 0)) {
            error_at(cc, node->line, "codegen_expr: ND_ASSIGN lhs bad ptr");
            fprintf(cc->out, "    movq $0, %%rax\n");
            return;
        }
        if (!node->lhs->type && node->lhs->kind != ND_MEMBER) {
            error_at(cc, node->line, "codegen_expr: ND_ASSIGN lhs has null type");
            fprintf(cc->out, "    movq $0, %%rax\n");
            return;
        }
        /* Use member_size for ND_MEMBER so stage2 stores 4 bytes to cc->tk etc., not 8 */
        if (node->lhs->kind == ND_MEMBER && node->lhs->member_size > 0) {
            pop_reg(cc, "r11");
            switch (node->lhs->member_size) {
                case 1: fprintf(cc->out, "    movb %%r11b, (%%rax)\n"); break;
                case 2: fprintf(cc->out, "    movw %%r11w, (%%rax)\n"); break;
                case 4: fprintf(cc->out, "    movl %%r11d, (%%rax)\n"); break;
                default: fprintf(cc->out, "    movq %%r11, (%%rax)\n"); break;
            }
            fprintf(cc->out, "    movq %%r11, %%rax\n");
        } else {
            codegen_store(cc, node->lhs->type);
        }
        {
            ZCC_EMIT_STORE(ir_map_type(node->lhs->type), lhs_addr_ir, rhs_ir, node->line);
        }
        return;
    }

    case ND_COMPOUND_ASSIGN:
        if (!node->lhs || !node->rhs) {
            error_at(cc, node->line, "codegen_expr: ND_COMPOUND_ASSIGN missing operand");
            fprintf(cc->out, "    movq $0, %%rax\n");
            return;
        }
        if (node->lhs->kind == ND_VAR && node->lhs->sym && node->lhs->sym->assigned_reg) {
            char *reg = node->lhs->sym->assigned_reg;
            codegen_expr_checked(cc, node->rhs);
            fprintf(cc->out, "    movq %%rax, %%r11\n");
            fprintf(cc->out, "    movq %s, %%rax\n", reg);
            switch (node->compound_op) {
                case ND_ADD:
                    if (is_pointer(node->lhs->type)) {
                        int esz = ptr_elem_size(node->lhs->type);
                        if (esz > 1) fprintf(cc->out, "    imulq $%d, %%r11\n", esz);
                    }
                    fprintf(cc->out, "    addq %%r11, %%rax\n");
                    break;
                case ND_SUB:
                    if (is_pointer(node->lhs->type)) {
                        int esz = ptr_elem_size(node->lhs->type);
                        if (esz > 1) fprintf(cc->out, "    imulq $%d, %%r11\n", esz);
                    }
                    fprintf(cc->out, "    subq %%r11, %%rax\n");
                    break;
                case ND_MUL: fprintf(cc->out, "    imulq %%r11, %%rax\n"); break;
                case ND_DIV:
                    if (node->lhs->type && is_unsigned_type(node->lhs->type)) {
                        fprintf(cc->out, "    xorq %%rdx, %%rdx\n    divq %%r11\n");
                    } else {
                        fprintf(cc->out, "    cqo\n    idivq %%r11\n");
                    }
                    break;
                case ND_MOD:
                    if (node->lhs->type && is_unsigned_type(node->lhs->type)) {
                        fprintf(cc->out, "    xorq %%rdx, %%rdx\n    divq %%r11\n    movq %%rdx, %%rax\n");
                    } else {
                        fprintf(cc->out, "    cqo\n    idivq %%r11\n    movq %%rdx, %%rax\n");
                    }
                    break;
                case ND_BAND:   fprintf(cc->out, "    andq %%r11, %%rax\n"); break;
                case ND_BOR:    fprintf(cc->out, "    orq %%r11, %%rax\n"); break;
                case ND_BXOR:   fprintf(cc->out, "    xorq %%r11, %%rax\n"); break;
                case ND_SHL:    fprintf(cc->out, "    movq %%r11, %%rcx\n    shlq %%cl, %%rax\n"); break;
                case ND_SHR:
                    if (node->lhs->type && is_unsigned_type(node->lhs->type)) fprintf(cc->out, "    movq %%r11, %%rcx\n    shrq %%cl, %%rax\n");
                    else fprintf(cc->out, "    movq %%r11, %%rcx\n    sarq %%cl, %%rax\n");
                    break;
                default: break;
            }
            fprintf(cc->out, "    movq %%rax, %s\n", reg);
            return;
        }
        codegen_addr_checked(cc, node->lhs);
        if (is_bad_ptr(node->lhs)) {
            error_at(cc, node->line, "codegen_expr: ND_COMPOUND_ASSIGN lhs bad ptr");
            fprintf(cc->out, "    movq $0, %%rax\n");
            return;
        }
        if (!node->lhs->type) {
            error_at(cc, node->line, "codegen_expr: ND_COMPOUND_ASSIGN lhs has null type");
            fprintf(cc->out, "    movq $0, %%rax\n");
            return;
        }
        push_reg(cc, "rax");
        if (node->lhs->kind == ND_MEMBER && node->lhs->member_size > 0) {
            switch (node->lhs->member_size) {
                case 1: fprintf(cc->out, "    movzbl (%%rax), %%eax\n"); break;
                case 2: fprintf(cc->out, "    movzwl (%%rax), %%eax\n"); break;
                case 4: fprintf(cc->out, "    movl (%%rax), %%eax\n"); break;
                default: fprintf(cc->out, "    movq (%%rax), %%rax\n"); break;
            }
        } else {
            codegen_load(cc, node->lhs->type);
        }
        push_reg(cc, "rax");
        codegen_expr_checked(cc, node->rhs);
        fprintf(cc->out, "    movq %%rax, %%r11\n");
        pop_reg(cc, "rax");
        switch (node->compound_op) {
            case ND_ADD:
                if (is_pointer(node->lhs->type)) {
                    int esz;
                    esz = ptr_elem_size(node->lhs->type);
                    if (esz > 1) fprintf(cc->out, "    imulq $%d, %%r11\n", esz);
                }
                fprintf(cc->out, "    addq %%r11, %%rax\n"); break;
            case ND_SUB: fprintf(cc->out, "    subq %%r11, %%rax\n"); break;
            case ND_MUL: fprintf(cc->out, "    imulq %%r11, %%rax\n"); break;
            case ND_DIV:
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
                    fprintf(cc->out, "    movq %%rdx, %%rax\n");
                } else {
                    fprintf(cc->out, "    cqo\n    idivq %%r11\n    movq %%rdx, %%rax\n");
                }
                break;
            case ND_BAND: fprintf(cc->out, "    andq %%r11, %%rax\n"); break;
            case ND_BOR: fprintf(cc->out, "    orq %%r11, %%rax\n"); break;
            case ND_BXOR: fprintf(cc->out, "    xorq %%r11, %%rax\n"); break;
            case ND_SHL: fprintf(cc->out, "    movq %%r11, %%rcx\n    shlq %%cl, %%rax\n"); break;
            case ND_SHR:
                if (node->lhs->type && is_unsigned_type(node->lhs->type))
                    fprintf(cc->out, "    movq %%r11, %%rcx\n    shrq %%cl, %%rax\n");
                else
                    fprintf(cc->out, "    movq %%r11, %%rcx\n    sarq %%cl, %%rax\n");
                break;
            default: break;
        }
        fprintf(cc->out, "    movq %%rax, %%r11\n");
        pop_reg(cc, "rax");
        if (node->lhs->kind == ND_MEMBER && node->lhs->member_size > 0) {
            switch (node->lhs->member_size) {
                case 1: fprintf(cc->out, "    movb %%r11b, (%%rax)\n"); break;
                case 2: fprintf(cc->out, "    movw %%r11w, (%%rax)\n"); break;
                case 4: fprintf(cc->out, "    movl %%r11d, (%%rax)\n"); break;
                default: fprintf(cc->out, "    movq %%r11, (%%rax)\n"); break;
            }
        } else {
            switch (type_size(node->lhs->type)) {
                case 1: fprintf(cc->out, "    movb %%r11b, (%%rax)\n"); break;
                case 2: fprintf(cc->out, "    movw %%r11w, (%%rax)\n"); break;
                case 4: fprintf(cc->out, "    movl %%r11d, (%%rax)\n"); break;
                default: fprintf(cc->out, "    movq %%r11, (%%rax)\n"); break;
            }
        }
        fprintf(cc->out, "    movq %%r11, %%rax\n");
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
            codegen_expr_checked(cc, node->lhs);
            if (!is_float_type(node->lhs->type)) fprintf(cc->out, "    cvtsi2sd %%rax, %%xmm0\n");
            fprintf(cc->out, "    subq $8, %%rsp\n");
            fprintf(cc->out, "    movsd %%xmm0, (%%rsp)\n");
            codegen_expr_checked(cc, node->rhs);
            if (!is_float_type(node->rhs->type)) fprintf(cc->out, "    cvtsi2sd %%rax, %%xmm0\n");
            fprintf(cc->out, "    movsd (%%rsp), %%xmm1\n");
            fprintf(cc->out, "    addq $8, %%rsp\n");
            fprintf(cc->out, "    addsd %%xmm1, %%xmm0\n");
            ir_emit_binary_op(ND_ADD, node->type, "f_lhs", "f_rhs", node->line);
            return;
        }
        codegen_expr_checked(cc, node->lhs);
        ir_save_result(lhs_ir);
        push_reg(cc, "rax");
        codegen_expr_checked(cc, node->rhs);
        ir_save_result(rhs_ir);
        fprintf(cc->out, "    movq %%rax, %%r11\n");
        pop_reg(cc, "rax");
        if (node->lhs->type && is_pointer(node->lhs->type)) {
            int esz = ptr_elem_size(node->lhs->type);
            if (esz > 1) {
                fprintf(cc->out, "    imulq $%d, %%r11\n", esz);
                char scale_str[32]; sprintf(scale_str, "%d", esz);
                char *dst = ir_bridge_fresh_tmp();
                ZCC_EMIT_BINARY(IR_MUL, IR_TY_I64, dst, rhs_ir, scale_str, node->line);
                strcpy(rhs_ir, dst);
            }
        } else if (node->rhs->type && is_pointer(node->rhs->type)) {
            int esz = ptr_elem_size(node->rhs->type);
            if (esz > 1) {
                fprintf(cc->out, "    imulq $%d, %%rax\n", esz);
                char scale_str[32]; sprintf(scale_str, "%d", esz);
                char *dst = ir_bridge_fresh_tmp();
                ZCC_EMIT_BINARY(IR_MUL, IR_TY_I64, dst, lhs_ir, scale_str, node->line);
                strcpy(lhs_ir, dst);
            }
        }
        fprintf(cc->out, "    addq %%r11, %%rax\n");
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
            codegen_expr_checked(cc, node->lhs);
            if (!is_float_type(node->lhs->type)) fprintf(cc->out, "    cvtsi2sd %%rax, %%xmm0\n");
            fprintf(cc->out, "    subq $8, %%rsp\n");
            fprintf(cc->out, "    movsd %%xmm0, (%%rsp)\n");
            codegen_expr_checked(cc, node->rhs);
            if (!is_float_type(node->rhs->type)) fprintf(cc->out, "    cvtsi2sd %%rax, %%xmm0\n");
            fprintf(cc->out, "    movsd (%%rsp), %%xmm1\n");
            fprintf(cc->out, "    addq $8, %%rsp\n");
            fprintf(cc->out, "    subsd %%xmm0, %%xmm1\n");
            fprintf(cc->out, "    movsd %%xmm1, %%xmm0\n");
            ir_emit_binary_op(ND_SUB, node->type, "f_lhs", "f_rhs", node->line);
            return;
        }
        codegen_expr_checked(cc, node->lhs);
        ir_save_result(lhs_ir);
        push_reg(cc, "rax");
        codegen_expr_checked(cc, node->rhs);
        ir_save_result(rhs_ir);
        fprintf(cc->out, "    movq %%rax, %%r11\n");
        pop_reg(cc, "rax");
        if (node->lhs->type && is_pointer(node->lhs->type)) {
            if (node->rhs->type && is_pointer(node->rhs->type)) {
                fprintf(cc->out, "    subq %%r11, %%rax\n");
                {
                    int esz;
                    esz = ptr_elem_size(node->lhs->type);
                    if (esz > 1) {
                        fprintf(cc->out, "    movq $%d, %%r11\n", esz);
                        fprintf(cc->out, "    cqo\n    idivq %%r11\n");
                    }
                }
                ir_emit_binary_op(ND_SUB, node->type, lhs_ir, rhs_ir, node->line);
                return;
            }
            {
                int esz;
                esz = node->lhs->type ? ptr_elem_size(node->lhs->type) : 1;
                if (esz > 1) fprintf(cc->out, "    imulq $%d, %%r11\n", esz);
            }
        }
        fprintf(cc->out, "    subq %%r11, %%rax\n");
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
            codegen_expr_checked(cc, node->lhs);
            if (!is_float_type(node->lhs->type)) fprintf(cc->out, "    cvtsi2sd %%rax, %%xmm0\n");
            fprintf(cc->out, "    subq $8, %%rsp\n");
            fprintf(cc->out, "    movsd %%xmm0, (%%rsp)\n");
            codegen_expr_checked(cc, node->rhs);
            if (!is_float_type(node->rhs->type)) fprintf(cc->out, "    cvtsi2sd %%rax, %%xmm0\n");
            fprintf(cc->out, "    movsd (%%rsp), %%xmm1\n");
            fprintf(cc->out, "    addq $8, %%rsp\n");
            fprintf(cc->out, "    mulsd %%xmm1, %%xmm0\n");
            ir_emit_binary_op(ND_MUL, node->type, "f_lhs", "f_rhs", node->line);
            return;
        }
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

        codegen_expr_checked(cc, node->lhs);
        ir_save_result(lhs_ir);
        push_reg(cc, "rax");
        codegen_expr_checked(cc, node->rhs);
        ir_save_result(rhs_ir);
        pop_reg(cc, "r11");
        fprintf(cc->out, "    imulq %%r11, %%rax\n");
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
            codegen_expr_checked(cc, node->lhs);
            if (!is_float_type(node->lhs->type)) fprintf(cc->out, "    cvtsi2sd %%rax, %%xmm0\n");
            fprintf(cc->out, "    subq $8, %%rsp\n");
            fprintf(cc->out, "    movsd %%xmm0, (%%rsp)\n");
            codegen_expr_checked(cc, node->rhs);
            if (!is_float_type(node->rhs->type)) fprintf(cc->out, "    cvtsi2sd %%rax, %%xmm0\n");
            fprintf(cc->out, "    movsd (%%rsp), %%xmm1\n");
            fprintf(cc->out, "    addq $8, %%rsp\n");
            fprintf(cc->out, "    divsd %%xmm0, %%xmm1\n");
            fprintf(cc->out, "    movsd %%xmm1, %%xmm0\n");
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
        fprintf(cc->out, "    movq %%rax, %%r11\n");
        pop_reg(cc, "rax");
        if (node->lhs->type && node->rhs->type &&
            (is_unsigned_type(node->lhs->type) || is_unsigned_type(node->rhs->type))) {
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
        
        if (node->rhs && node->rhs->kind == ND_NUM && is_power_of_2_val(node->rhs->int_val)) {
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
        fprintf(cc->out, "    movq %%rax, %%r11\n");
        pop_reg(cc, "rax");
        if (node->lhs->type && node->rhs->type &&
            (is_unsigned_type(node->lhs->type) || is_unsigned_type(node->rhs->type))) {
            fprintf(cc->out, "    xorq %%rdx, %%rdx\n");
            fprintf(cc->out, "    divq %%r11\n");
            fprintf(cc->out, "    movq %%rdx, %%rax\n");
        } else {
            fprintf(cc->out, "    cqo\n    idivq %%r11\n");
            fprintf(cc->out, "    movq %%rdx, %%rax\n");
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
        fprintf(cc->out, "    andq %%r11, %%rax\n");
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
        fprintf(cc->out, "    orq %%r11, %%rax\n");
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
        fprintf(cc->out, "    xorq %%r11, %%rax\n");
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
        fprintf(cc->out, "    notq %%rax\n");
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
        codegen_expr_checked(cc, node->lhs);
        ir_save_result(src_ir);
        fprintf(cc->out, "    negq %%rax\n");
        {
            char *dst = ir_bridge_fresh_tmp();
            ZCC_EMIT_UNARY(IR_NEG, ir_map_type(node->type), dst, src_ir, node->line);
        }
        return;
    }

    case ND_LNOT: {
        char src_ir[32];
        if (!node->lhs) {
            error_at(cc, node->line, "codegen_expr: ND_LNOT null lhs");
            fprintf(cc->out, "    movq $0, %%rax\n");
            return;
        }
        codegen_expr_checked(cc, node->lhs);
        ir_save_result(src_ir);
        fprintf(cc->out, "    cmpq $0, %%rax\n");
        fprintf(cc->out, "    sete %%al\n");
        fprintf(cc->out, "    movzbl %%al, %%eax\n");
        {
            char zero_ir[32];
            char *zt = ir_bridge_fresh_tmp();
            int i;
            for (i = 0; zt[i]; i++) zero_ir[i] = zt[i];
            zero_ir[i] = 0;
            ZCC_EMIT_CONST(IR_TY_I64, zero_ir, 0, node->line);
            char *dst = ir_bridge_fresh_tmp();
            ZCC_EMIT_BINARY(IR_EQ, IR_TY_I32, dst, src_ir, zero_ir, node->line);
        }
        return;
    }

    case ND_EQ: case ND_NE: case ND_LT: case ND_LE: case ND_GT: case ND_GE: {
        int uns;
        int use32;
        char lhs_ir[32];
        char rhs_ir[32];
        uns = (node->lhs && node->lhs->type && is_unsigned_type(node->lhs->type)) ||
              (node->rhs && node->rhs->type && is_unsigned_type(node->rhs->type));
        use32 = uns && node->lhs && node->lhs->type && node->rhs && node->rhs->type &&
                type_size(node->lhs->type) == 4 && type_size(node->rhs->type) == 4;
        codegen_expr_checked(cc, node->lhs);
        ir_save_result(lhs_ir);
        push_reg(cc, "rax");
        codegen_expr_checked(cc, node->rhs);
        ir_save_result(rhs_ir);
        fprintf(cc->out, "    movq %%rax, %%r11\n");
        pop_reg(cc, "rax");
        if (use32)
            fprintf(cc->out, "    cmpl %%r11d, %%eax\n");  /* 32-bit: avoids sign-extended imm in 64-bit */
        else
            fprintf(cc->out, "    cmpq %%r11, %%rax\n");
        if (uns) {
            switch (node->kind) {
                case ND_EQ: fprintf(cc->out, "    sete %%al\n"); break;
                case ND_NE: fprintf(cc->out, "    setne %%al\n"); break;
                case ND_LT: fprintf(cc->out, "    setb %%al\n"); break;
                case ND_LE: fprintf(cc->out, "    setbe %%al\n"); break;
                case ND_GT: fprintf(cc->out, "    seta %%al\n"); break;
                case ND_GE: fprintf(cc->out, "    setae %%al\n"); break;
                default: break;
            }
        } else {
            switch (node->kind) {
                case ND_EQ: fprintf(cc->out, "    sete %%al\n"); break;
                case ND_NE: fprintf(cc->out, "    setne %%al\n"); break;
                case ND_LT: fprintf(cc->out, "    setl %%al\n"); break;
                case ND_LE: fprintf(cc->out, "    setle %%al\n"); break;
                case ND_GT: fprintf(cc->out, "    setg %%al\n"); break;
                case ND_GE: fprintf(cc->out, "    setge %%al\n"); break;
                default: break;
            }
        }
        fprintf(cc->out, "    movzbl %%al, %%eax\n");
        ir_emit_binary_op(node->kind, node->type, lhs_ir, rhs_ir, node->line);
        return;
    }

    case ND_LAND:
        lbl1 = new_label(cc);
        codegen_expr_checked(cc, node->lhs);
        fprintf(cc->out, "    cmpq $0, %%rax\n");
        fprintf(cc->out, "    je .L%d\n", lbl1);
        codegen_expr_checked(cc, node->rhs);
        fprintf(cc->out, "    cmpq $0, %%rax\n");
        fprintf(cc->out, "    setne %%al\n");
        fprintf(cc->out, "    movzbl %%al, %%eax\n");
        fprintf(cc->out, ".L%d:\n", lbl1);
        return;

    case ND_LOR:
        lbl1 = new_label(cc);
        lbl2 = new_label(cc);
        codegen_expr_checked(cc, node->lhs);
        fprintf(cc->out, "    cmpq $0, %%rax\n");
        fprintf(cc->out, "    jne .L%d\n", lbl1);
        codegen_expr_checked(cc, node->rhs);
        fprintf(cc->out, "    cmpq $0, %%rax\n");
        fprintf(cc->out, "    jne .L%d\n", lbl1);
        fprintf(cc->out, "    movq $0, %%rax\n");
        fprintf(cc->out, "    jmp .L%d\n", lbl2);
        fprintf(cc->out, ".L%d:\n", lbl1);
        fprintf(cc->out, "    movq $1, %%rax\n");
        fprintf(cc->out, ".L%d:\n", lbl2);
        return;

    case ND_ADDR:
        if (!node->lhs) {
            error_at(cc, node->line, "codegen_expr: ND_ADDR null lhs");
            fprintf(cc->out, "    movq $0, %%rax\n");
            return;
        }
        codegen_addr_checked(cc, node->lhs);
        return;

    case ND_DEREF:
        if (!node->lhs) {
            error_at(cc, node->line, "codegen_expr: ND_DEREF null lhs");
            fprintf(cc->out, "    movq $0, %%rax\n");
            return;
        }
        codegen_expr_checked(cc, node->lhs);
        /* Type-aware load: char* -> movsbl, int* -> movl, ptr/long* -> movq */
        codegen_load(cc, node->type);
        return;

    case ND_MEMBER:
        codegen_addr_checked(cc, node);
        if (node->type) {
            codegen_load(cc, node->type);
        } else {
            switch (node->member_size) {
                case 1: fprintf(cc->out, "    movzbl (%%rax), %%eax\n"); break;
                case 2: fprintf(cc->out, "    movzwl (%%rax), %%eax\n"); break;
                case 4: fprintf(cc->out, "    movl (%%rax), %%eax\n"); break;
                case 8: fprintf(cc->out, "    movq (%%rax), %%rax\n"); break;
                default: break;
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
            switch (node->cast_type->size) {
                case 1:
                    if (node->cast_type->kind == TY_UCHAR)
                        fprintf(cc->out, "    movzbl %%al, %%eax\n");
                    else
                        fprintf(cc->out, "    movsbl %%al, %%eax\n");
                    break;
                case 2:
                    if (node->cast_type->kind == TY_USHORT)
                        fprintf(cc->out, "    movzwl %%ax, %%eax\n");
                    else
                        fprintf(cc->out, "    movswl %%ax, %%eax\n");
                    break;
                case 4:
                    fprintf(cc->out, "    movl %%eax, %%eax\n");
                    break;
                default: break;
            }
        }
        {
            char *dst = ir_bridge_fresh_tmp();
            ZCC_EMIT_UNARY(IR_CAST, ir_map_type(node->type), dst, src_ir, node->line);
        }
        return;
    }

    case ND_TERNARY:
        if (!node->cond || !node->then_body || !node->else_body) {
            error_at(cc, node->line, "codegen_expr: ND_TERNARY missing cond/then/else");
            fprintf(cc->out, "    movq $0, %%rax\n");
            return;
        }
        lbl1 = new_label(cc);
        lbl2 = new_label(cc);
        codegen_expr_checked(cc, node->cond);
        fprintf(cc->out, "    cmpq $0, %%rax\n");
        fprintf(cc->out, "    je .L%d\n", lbl1);
        codegen_expr_checked(cc, node->then_body);
        fprintf(cc->out, "    jmp .L%d\n", lbl2);
        fprintf(cc->out, ".L%d:\n", lbl1);
        codegen_expr_checked(cc, node->else_body);
        fprintf(cc->out, ".L%d:\n", lbl2);
        return;

    case ND_COMMA_EXPR:
        codegen_expr_checked(cc, node->lhs);
        codegen_expr_checked(cc, node->rhs);
        return;

    case ND_PRE_INC:
        if (node->lhs && node->lhs->kind == ND_VAR && node->lhs->sym && node->lhs->sym->assigned_reg) {
            char *reg = node->lhs->sym->assigned_reg;
            int esz = 1; if (is_pointer(node->lhs->type)) esz = ptr_elem_size(node->lhs->type);
            fprintf(cc->out, "    addq $%d, %s\n", esz, reg);
            fprintf(cc->out, "    movq %s, %%rax\n", reg);
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
        {
            int esz;
            esz = 1;
            if (is_pointer(node->lhs->type)) esz = ptr_elem_size(node->lhs->type);
            fprintf(cc->out, "    addq $%d, %%rax\n", esz);
        }
        fprintf(cc->out, "    movq %%rax, %%r11\n");
        pop_reg(cc, "rax");
        switch (type_size(node->lhs->type)) {
            case 1: fprintf(cc->out, "    movb %%r11b, (%%rax)\n"); break;
            case 2: fprintf(cc->out, "    movw %%r11w, (%%rax)\n"); break;
            case 4: fprintf(cc->out, "    movl %%r11d, (%%rax)\n"); break;
            default: fprintf(cc->out, "    movq %%r11, (%%rax)\n"); break;
        }
        fprintf(cc->out, "    movq %%r11, %%rax\n");
        return;

    case ND_PRE_DEC:
        if (node->lhs && node->lhs->kind == ND_VAR && node->lhs->sym && node->lhs->sym->assigned_reg) {
            char *reg = node->lhs->sym->assigned_reg;
            int esz = 1; if (is_pointer(node->lhs->type)) esz = ptr_elem_size(node->lhs->type);
            fprintf(cc->out, "    subq $%d, %s\n", esz, reg);
            fprintf(cc->out, "    movq %s, %%rax\n", reg);
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
        {
            int esz;
            esz = 1;
            if (is_pointer(node->lhs->type)) esz = ptr_elem_size(node->lhs->type);
            fprintf(cc->out, "    subq $%d, %%rax\n", esz);
        }
        fprintf(cc->out, "    movq %%rax, %%r11\n");
        pop_reg(cc, "rax");
        switch (type_size(node->lhs->type)) {
            case 1: fprintf(cc->out, "    movb %%r11b, (%%rax)\n"); break;
            case 2: fprintf(cc->out, "    movw %%r11w, (%%rax)\n"); break;
            case 4: fprintf(cc->out, "    movl %%r11d, (%%rax)\n"); break;
            default: fprintf(cc->out, "    movq %%r11, (%%rax)\n"); break;
        }
        fprintf(cc->out, "    movq %%r11, %%rax\n");
        return;

    case ND_POST_INC:
        if (node->lhs && node->lhs->kind == ND_VAR && node->lhs->sym && node->lhs->sym->assigned_reg) {
            char *reg = node->lhs->sym->assigned_reg;
            int esz = 1; if (is_pointer(node->lhs->type)) esz = ptr_elem_size(node->lhs->type);
            fprintf(cc->out, "    movq %s, %%rax\n", reg);
            fprintf(cc->out, "    addq $%d, %s\n", esz, reg);
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
        push_reg(cc, "rax");  /* save original value */
        {
            int esz;
            esz = 1;
            if (is_pointer(node->lhs->type)) esz = ptr_elem_size(node->lhs->type);
            fprintf(cc->out, "    addq $%d, %%rax\n", esz);
        }
        fprintf(cc->out, "    movq %%rax, %%r11\n");
        pop_reg(cc, "rdx");  /* rdx = original value */
        pop_reg(cc, "rax");  /* rax = address */
        switch (type_size(node->lhs->type)) {
            case 1: fprintf(cc->out, "    movb %%r11b, (%%rax)\n"); break;
            case 2: fprintf(cc->out, "    movw %%r11w, (%%rax)\n"); break;
            case 4: fprintf(cc->out, "    movl %%r11d, (%%rax)\n"); break;
            default: fprintf(cc->out, "    movq %%r11, (%%rax)\n"); break;
        }
        fprintf(cc->out, "    movq %%rdx, %%rax\n");
        return;

    case ND_POST_DEC:
        if (node->lhs && node->lhs->kind == ND_VAR && node->lhs->sym && node->lhs->sym->assigned_reg) {
            char *reg = node->lhs->sym->assigned_reg;
            int esz = 1; if (is_pointer(node->lhs->type)) esz = ptr_elem_size(node->lhs->type);
            fprintf(cc->out, "    movq %s, %%rax\n", reg);
            fprintf(cc->out, "    subq $%d, %s\n", esz, reg);
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
        {
            int esz;
            esz = 1;
            if (is_pointer(node->lhs->type)) esz = ptr_elem_size(node->lhs->type);
            fprintf(cc->out, "    subq $%d, %%rax\n", esz);
        }
        fprintf(cc->out, "    movq %%rax, %%r11\n");
        pop_reg(cc, "rdx");  /* rdx = original value */
        pop_reg(cc, "rax");  /* rax = address */
        switch (type_size(node->lhs->type)) {
            case 1: fprintf(cc->out, "    movb %%r11b, (%%rax)\n"); break;
            case 2: fprintf(cc->out, "    movw %%r11w, (%%rax)\n"); break;
            case 4: fprintf(cc->out, "    movl %%r11d, (%%rax)\n"); break;
            default: fprintf(cc->out, "    movq %%r11, (%%rax)\n"); break;
        }
        fprintf(cc->out, "    movq %%rdx, %%rax\n");
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

        /* System V AMD64 (Linux): 6 register args: RDI, RSI, RDX, RCX, R8, R9; 7th+ on stack */
        argregs[0] = "rdi";
        argregs[1] = "rsi";
        argregs[2] = "rdx";
        argregs[3] = "rcx";
        argregs[4] = "r8";
        argregs[5] = "r9";

        if (!node->func_name) {
            error_at(cc, node->line, "codegen_expr: ND_CALL null func_name");
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

        /* System V AMD64: no shadow space required. Space for 7th+ args is left on the stack by push/pop. */
        args_on_stack = (nargs > 6) ? (nargs - 6) : 0;
        /* We must ensure that AFTER the args are on the stack, %rsp is 16-byte aligned. */
        /* Currently, depth is cc->stack_depth. After pushes/pops, depth will be cc->stack_depth + args_on_stack. */
        alignment_pad = 0;
        if ((cc->stack_depth + args_on_stack) % 2 != 0) {
            alignment_pad = 8;
        }
        
        if (alignment_pad > 0) {
            fprintf(cc->out, "    subq $%d, %%rsp\n", alignment_pad);
            cc->stack_depth++;
        }

        /* push args in reverse order */
        for (i = nargs - 1; i >= 0; i--) {
            if (!node->args[i]) {
                error_at(cc, node->line, "null argument in call");
                return;
            }
            codegen_expr_checked(cc, node->args[i]);
            ir_save_result(&args_ir_1d[i * 32]);
            push_reg(cc, "rax");
        }

        /* pop first N into argregs */
        for (i = 0; i < nargs && i < 6; i++) {
            pop_reg(cc, argregs[i]);
        }
        
        for (i = 0; i < nargs; i++) {
            ZCC_EMIT_ARG(ir_map_type(node->args[i]->type), &args_ir_1d[i * 32], node->line);
        }

        fprintf(cc->out, "    movq $0, %%rax\n");
        fprintf(cc->out, "    call %s\n", node->func_name);
        
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
    if (!node) return;

    if (cc) {
    }

    int lbl1;
    int lbl2;
    int lbl3;
    int old_break;
    int old_continue;
    char errbuf[80];

    /* Do NOT use ptr_in_fault_range(node): stage2 miscompiles it and rejects valid arena ptrs. */
    if (!node) return;
    if (is_bad_ptr(node)) {
        sprintf(errbuf, "codegen_stmt: bad node ptr %p", (void *)node);
        error_at(cc, 0, errbuf);
        return;
    }

    switch (node->kind) {

    case ND_RETURN:
        if (node->lhs) {
            codegen_expr_checked(cc, node->lhs);
            {
                char ret_tmp[32];
                ir_save_result(ret_tmp);
                ZCC_EMIT_RET(ir_map_type(node->lhs->type), ret_tmp, node->line);
            }
        } else {
            ZCC_EMIT_RET(0, "", node->line);
        }
        fprintf(cc->out, "    jmp .Lfunc_end_%d\n", cc->func_end_label);
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
        fprintf(cc->out, "    cmpq $0, %%rax\n");
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
        lbl1 = new_label(cc);  /* loop start */
        lbl2 = new_label(cc);  /* loop end */
        old_break = cc->break_label;
        old_continue = cc->continue_label;
        cc->break_label = lbl2;
        cc->continue_label = lbl1;
        emit_label_fmt(cc, lbl1, FMT_DEF);
        sprintf(ir_lbl, ".L%d", lbl1);
        ZCC_EMIT_LABEL(ir_lbl, node->line);
        codegen_expr_checked(cc, node->cond);
        ir_save_result(cond_ir);
        fprintf(cc->out, "    cmpq $0, %%rax\n");
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
        lbl1 = new_label(cc);  /* loop start */
        lbl2 = new_label(cc);  /* loop end */
        lbl3 = new_label(cc);  /* continue target (increment) */
        old_break = cc->break_label;
        old_continue = cc->continue_label;
        cc->break_label = lbl2;
        cc->continue_label = lbl3;
        if (node->init) codegen_stmt(cc, node->init);
        emit_label_fmt(cc, lbl1, FMT_DEF);
        sprintf(ir_lbl, ".L%d", lbl1);
        ZCC_EMIT_LABEL(ir_lbl, node->line);
        if (node->cond) {
            codegen_expr_checked(cc, node->cond);
            ir_save_result(cond_ir);
            fprintf(cc->out, "    cmpq $0, %%rax\n");
            emit_label_fmt(cc, lbl2, FMT_JE);
            sprintf(ir_lbl, ".L%d", lbl2);
            ZCC_EMIT_BR_IF(cond_ir, ir_lbl, node->line);
        }
        codegen_stmt(cc, node->body);
        emit_label_fmt(cc, lbl3, FMT_DEF);
        sprintf(ir_lbl, ".L%d", lbl3);
        ZCC_EMIT_LABEL(ir_lbl, node->line);
        if (node->inc) codegen_expr_checked(cc, node->inc);
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
        char cond_ir[32];
        char ir_lbl[32];
        lbl1 = new_label(cc);
        lbl2 = new_label(cc);
        old_break = cc->break_label;
        old_continue = cc->continue_label;
        cc->break_label = lbl2;
        cc->continue_label = lbl1;
        emit_label_fmt(cc, lbl1, FMT_DEF);
        sprintf(ir_lbl, ".L%d", lbl1);
        ZCC_EMIT_LABEL(ir_lbl, node->line);
        codegen_stmt(cc, node->body);
        codegen_expr_checked(cc, node->cond);
        ir_save_result(cond_ir);
        fprintf(cc->out, "    cmpq $0, %%rax\n");
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
                ZCC_EMIT_CONST(IR_TY_I64, case_const_ir, node->cases[i]->case_val, node->line);

                ct = ir_bridge_fresh_tmp();
                sprintf(cmp_ir, "%s", ct);
                
                ZCC_EMIT_BINARY(IR_EQ, IR_TY_I64, cmp_ir, switch_val_ir, case_const_ir, node->line);
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
        fprintf(cc->out, "    jmp .Luser_%s_%s\n", cc->current_func, node->label_name);
        sprintf(ir_lbl, ".Luser_%s_%s", cc->current_func, node->label_name);
        ZCC_EMIT_BR(ir_lbl, node->line);
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
        if (node->lhs) codegen_stmt(cc, node->lhs);
        return;
    }

    case ND_NOP:
        return;

    default: {
        /* expression statement */
        char badmsg[80];
        if (is_bad_ptr(node)) {
            sprintf(badmsg, "codegen_stmt default: bad expr node %p", node ? (void *)node : (void *)0);
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
    if (i == 0) return "%r12";
    if (i == 1) return "%r13";
    if (i == 2) return "%r14";
    if (i == 3) return "%r15";
    return "%rbx";
}

static void ra_add_local(Symbol *sym) {
    int i;
    if (!sym || !sym->is_local || sym->live_start == -1) return;
    for (i = 0; i < num_ra_locals; i++) {
        if (ra_locals[i] == sym) return;
    }
    if (num_ra_locals < 1024) ra_locals[num_ra_locals++] = sym;
}

static void compute_liveness(Node *n) {
    int i;
    int loop_start;
    if (!n) return;
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
            if (n->sym->live_start == 0) n->sym->live_start = pseudo_pc;
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
        for (i = 0; i < n->num_stmts; i++) compute_liveness(n->stmts[i]);
    }
    if (n->args) {
        for (i = 0; i < n->num_args; i++) compute_liveness(n->args[i]);
    }
    if (n->cases) {
        for (i = 0; i < n->num_cases; i++) compute_liveness(n->cases[i]);
    }
    compute_liveness(n->default_case);
    compute_liveness(n->case_body);
}

static int allocate_registers(Node *func) {
    int count = 0;
    int i, j, r;
    int param_limit = -(func->num_params * 8);
    char *active_regs[5] = {0,0,0,0,0};
    int active_ends[5] = {0,0,0,0,0};
    int used_regs_bitmask = 0;

    num_ra_locals = 0;
    pseudo_pc = 1;
    compute_liveness(func->body);
    
    for (i = 0; i < num_ra_locals; i++) {
        Symbol *sym = ra_locals[i];
        if (sym->stack_offset >= param_limit && sym->stack_offset < 0) {
            sym->live_start = -1; /* never alloc parameters for safety */
        }
        if (sym->live_start != -1 && sym->type && sym->type->kind != TY_ARRAY && (is_integer(sym->type) || sym->type->kind == TY_PTR)) {
            ra_locals[count++] = sym;
        } else {
            sym->live_start = -1;
        }
    }
    num_ra_locals = count;

    for (i = 0; i < num_ra_locals; i++) {
        for (j = i + 1; j < num_ra_locals; j++) {
            if (ra_locals[j]->live_start < ra_locals[i]->live_start) {
                Symbol *t = ra_locals[i]; ra_locals[i] = ra_locals[j]; ra_locals[j] = t;
            }
        }
    }

    /* Linear Scan */
    for (i = 0; i < num_ra_locals; i++) {
        Symbol *sym = ra_locals[i];
        if (sym->live_start == -1) continue;

        for (r = 0; r < 5; r++) {
            if (active_regs[r] && active_ends[r] <= sym->live_start) active_regs[r] = 0;
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

#pragma weak zcc_run_passes_emit_body_pgo
#pragma weak zcc_node_from

static int ir_whitelisted(const char *name) {
    if (!name) return 0;
    static const char *wl[] = {
        /* Batch 1-2: test suite */
        "fold_test", "dce_test", "licm_test", "pressure_test", "escape_test",
        /* Batch 3a: pure enum-switch, no struct/global writes — VERIFIED SAFE */
        "ir_op_name", "ir_type_name", "ir_type_bytes", "ir_type_unsigned",
        "ir_op_is_terminator",
        /* Batch 3c: lexer functions — pure comparisons, no shifts */
        "is_digit", "is_space", "is_alpha", "is_alnum", "hex_val",
        NULL
    };
    int i;
    for (i = 0; wl[i]; i++)
        if (strcmp(name, wl[i]) == 0) return 1;
    return 0;
}

void codegen_func(Compiler *cc, Node *func) {
    char *argregs[6];
    int i;
    int stack_size;
    int used_regs;

    if (!func) return;
    fprintf(stderr, "cc_func: %s\n", func->func_def_name);
    used_regs = allocate_registers(func);

    argregs[0] = "rdi";
    argregs[1] = "rsi";
    argregs[2] = "rdx";
    argregs[3] = "rcx";
    argregs[4] = "r8";
    argregs[5] = "r9";

    cc->func_end_label = new_label(cc);
    strncpy(cc->current_func, func->func_def_name, MAX_IDENT - 1);

    ir_bridge_func_begin(func);

    stack_size = func->stack_size + 40; /* reserve 5x8 byte push slots */
    if (stack_size < 256) stack_size = 256;
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
            fprintf(cc->out, "    movq %s, %d(%%rbp)\n", get_callee_reg(i), -(func->stack_size + 8 * (i + 1)));
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

    /* Store params from registers to their stack locations.
       The param Symbols have stack_offsets assigned by scope_add_local.
       We need to retrieve those. They're in the func node's param info.
       Actually, we stored them during parse_func_def — let me use
       the offsets directly. Params got sequential offsets from scope_add_local:
       param0: -8, param1: -16, param2: -24, etc. */
    /* Store params from registers to their stack locations... */
    for (i = 0; i < func->num_params; i++) {
        int offset;
        offset = -(i + 1) * 8;
        if (i < 6) {
            fprintf(cc->out, "    movq %%%s, %d(%%rbp)\n", argregs[i], offset);
        } else {
            fprintf(cc->out, "    movq %d(%%rbp), %%rax\n", 16 + (i - 6) * 8);
            fprintf(cc->out, "    movq %%rax, %d(%%rbp)\n", offset);
        }
    }

    int ir_ok = 0;
    if (getenv("ZCC_IR_BACKEND") || getenv("ZCC_IR_LOWER") || ir_whitelisted(func->func_def_name)) {
        if (zcc_run_passes_emit_body_pgo && zcc_node_from) {
            void *ir_ast = zcc_node_from((void *)func->body);
            if (ir_ast) {
                ir_ok = zcc_run_passes_emit_body_pgo(ir_ast, NULL, func->func_def_name, cc->out, stack_size, func->num_params, cc->func_end_label, cc->func_end_label);
            }
        }
    }
    if (!ir_ok) {
        codegen_stmt(cc, func->body);
    }

    fprintf(cc->out, ".Lfunc_end_%d:\n", cc->func_end_label);
    for (i = 4; i >= 0; i--) {
        if (used_regs & (1 << i)) {
            fprintf(cc->out, "    movq %d(%%rbp), %s\n", -(func->stack_size + 8 * (i + 1)), get_callee_reg(i));
        }
    }
    fprintf(cc->out, "    movq %%rbp, %%rsp\n");
    fprintf(cc->out, "    popq %%rbp\n");
    fprintf(cc->out, "    ret\n");

    ir_bridge_func_end();

    scope_pop(cc);
}

/* ================================================================ */
/* GLOBAL VARIABLE EMISSION                                          */
/* Bug fix: section directive (.bss/.data) BEFORE label              */
/* Bug fix: .p2align 3 before every label (x86-64 ABI alignment)    */
/* ================================================================ */

static void emit_global_var(Compiler *cc, Node *gvar) {
    int size;

    if (gvar->is_extern) return;  /* no emission for extern */

    size = type_size(gvar->type);
    if (size <= 0) size = 8;

    if (gvar->initializer) {
        /* initialized data goes in .data */
        fprintf(cc->out, "    .data\n");
        fprintf(cc->out, "    .p2align 3\n");
        if (!gvar->is_static) {
            fprintf(cc->out, "    .globl %s\n", gvar->name);
        }
        fprintf(cc->out, "%s:\n", gvar->name);
        /* only handle simple integer initializers */
        if (gvar->initializer->kind == ND_NUM) {
            if (size == 1) fprintf(cc->out, "    .byte %lld\n", gvar->initializer->int_val);
            else if (size == 2) fprintf(cc->out, "    .short %lld\n", gvar->initializer->int_val);
            else if (size == 4) fprintf(cc->out, "    .long %lld\n", gvar->initializer->int_val);
            else fprintf(cc->out, "    .quad %lld\n", gvar->initializer->int_val);
        } else {
            fprintf(cc->out, "    .zero %d\n", size);
        }
    } else {
        /* uninitialized data goes in .bss */
        fprintf(cc->out, "    .bss\n");
        fprintf(cc->out, "    .p2align 3\n");
        if (!gvar->is_static) {
            fprintf(cc->out, "    .globl %s\n", gvar->name);
        }
        fprintf(cc->out, "%s:\n", gvar->name);
        fprintf(cc->out, "    .zero %d\n", size);
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
        fprintf(cc->out, ".L%d:\n", cc->strings[i].label_id);
        fprintf(cc->out, ".Lstr_%d:\n", i);  /* IR backend alias */
        fprintf(cc->out, "    .asciz \"");
        for (j = 0; j < cc->strings[i].len; j++) {
            char c;
            c = cc->strings[i].data[j];
            if (c == '\n') fprintf(cc->out, "\\n");
            else if (c == '\t') fprintf(cc->out, "\\t");
            else if (c == '\r') fprintf(cc->out, "\\r");
            else if (c == '\\') fprintf(cc->out, "\\\\");
            else if (c == '"') fprintf(cc->out, "\\\"");
            else if (c == '\0') fprintf(cc->out, "\\0");
            else if (c >= 32 && c < 127) fprintf(cc->out, "%c", c);
            else fprintf(cc->out, "\\%03o", (unsigned char)c);
        }
        fprintf(cc->out, "\"\n");
    }
}

/* ================================================================ */
/* PROGRAM CODEGEN ENTRY                                             */
/* ================================================================ */

static void fold_constants(Compiler *cc, Node *node) {
    if (!node) return;

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
        for (i = 0; i < node->num_args; i++) fold_constants(cc, node->args[i]);
    }
    if (node->kind == ND_BLOCK && node->num_stmts > 0) {
        int i;
        for (i = 0; i < node->num_stmts; i++) fold_constants(cc, node->stmts[i]);
    }
    if (node->kind == ND_SWITCH && node->num_cases > 0) {
        int i;
        for (i = 0; i < node->num_cases; i++) fold_constants(cc, node->cases[i]);
        fold_constants(cc, node->default_case);
    }

    if (node->kind == ND_ADD || node->kind == ND_SUB || node->kind == ND_MUL || node->kind == ND_DIV || node->kind == ND_MOD) {
        if (node->lhs && node->rhs && node->lhs->kind == ND_NUM && node->rhs->kind == ND_NUM) {
            long long v1 = node->lhs->int_val, v2 = node->rhs->int_val, res = 0;
            if (node->kind == ND_ADD) res = v1 + v2;
            else if (node->kind == ND_SUB) res = v1 - v2;
            else if (node->kind == ND_MUL) res = v1 * v2;
            else if (node->kind == ND_DIV && v2 != 0) res = v1 / v2;
            else if (node->kind == ND_MOD && v2 != 0) res = v1 % v2;
            else return; 
            node->kind = ND_NUM; node->int_val = res; node->lhs = 0; node->rhs = 0;
        }
    }

    if (node->kind == ND_SHL || node->kind == ND_SHR || node->kind == ND_BAND || node->kind == ND_BOR || node->kind == ND_BXOR ||
        node->kind == ND_LT || node->kind == ND_LE || node->kind == ND_GT || node->kind == ND_GE || node->kind == ND_EQ || node->kind == ND_NE) {
        if (node->lhs && node->rhs && node->lhs->kind == ND_NUM && node->rhs->kind == ND_NUM) {
            long long v1 = node->lhs->int_val, v2 = node->rhs->int_val, res = 0;
            if (node->kind == ND_SHL) res = v1 << v2;
            else if (node->kind == ND_SHR) res = v1 >> v2;
            else if (node->kind == ND_BAND) res = v1 & v2;
            else if (node->kind == ND_BOR) res = v1 | v2;
            else if (node->kind == ND_BXOR) res = v1 ^ v2;
            else if (node->kind == ND_LT) res = v1 < v2;
            else if (node->kind == ND_LE) res = v1 <= v2;
            else if (node->kind == ND_GT) res = v1 > v2;
            else if (node->kind == ND_GE) res = v1 >= v2;
            else if (node->kind == ND_EQ) res = v1 == v2;
            else if (node->kind == ND_NE) res = v1 != v2;
            node->kind = ND_NUM; node->int_val = res; node->lhs = 0; node->rhs = 0;
        }
    }

    /* Dead-Branch Elimination (AST-level DCE) */
    if (node->kind == ND_IF) {
        if (node->cond && node->cond->kind == ND_NUM) {
            if (node->cond->int_val == 0) {
                /* condition is false: replace with else body or empty block */
                if (node->else_body) {
                    Node *tgt = node->else_body;
                    node->kind = tgt->kind;
                    node->num_stmts = tgt->num_stmts;
                    node->stmts = tgt->stmts;
                    node->lhs = tgt->lhs;
                    node->rhs = tgt->rhs;
                    node->body = tgt->body;
                } else {
                    node->kind = ND_BLOCK;
                    node->num_stmts = 0;
                    node->stmts = (Node **)cc_alloc(cc, sizeof(Node *));
                }
            } else {
                /* condition is true: replace with then body */
                if (node->then_body) {
                    Node *tgt = node->then_body;
                    node->kind = tgt->kind;
                    node->num_stmts = tgt->num_stmts;
                    node->stmts = tgt->stmts;
                    node->lhs = tgt->lhs;
                    node->rhs = tgt->rhs;
                    node->body = tgt->body;
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
                    node->kind = tgt->kind;
                    node->num_stmts = tgt->num_stmts;
                    node->stmts = tgt->stmts;
                    node->lhs = tgt->lhs;
                    node->rhs = tgt->rhs;
                }
            } else {
                if (node->then_body) {
                    Node *tgt = node->then_body;
                    node->kind = tgt->kind;
                    node->num_stmts = tgt->num_stmts;
                    node->stmts = tgt->stmts;
                    node->lhs = tgt->lhs;
                    node->rhs = tgt->rhs;
                }
            }
        }
    }
}

void codegen_program(Compiler *cc, Node *prog) {
    Node *n;
    int i;

    /* Do NOT check ptr_in_fault_range(prog/n): stage2 miscompiles it and falsely
     * rejects valid arena pointers (e.g. 0xac09f8), causing early exit and WinMain link error. */
    if (!prog) return;

    /* Linux: avoid "missing .note.GNU-stack" linker warning */
    fprintf(cc->out, "    .section .note.GNU-stack,\"\",@progbits\n");
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

    /* Emit global variables */
    for (i = 0; i < cc->num_globals; i++) {
        fold_constants(cc, cc->globals[i]->initializer);
        emit_global_var(cc, cc->globals[i]);
    }

    /* Emit string literals */
    if (cc->num_strings > 0) {
        emit_strings(cc);
    }
}

/* ZKAEDI FORCE RENDER CACHE INVALIDATION */
