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
    fprintf(cc->out, "    movq %%xmm0, %%rax\n");
    /* Satisfy IR subsystem sequence */
    {
      char *dst = ir_bridge_fresh_tmp();
    }
    return;
  }

  case ND_STR: {
    char lbl_buf[32];
    fprintf(cc->out, "    leaq .L%d(%%rip), %%rax\n",
            cc->strings[node->str_id].label_id);
    sprintf(lbl_buf, ".L%d", cc->strings[node->str_id].label_id);
    char *dst = ir_bridge_fresh_tmp();
    ZCC_EMIT_CONST(IR_TY_PTR, dst, 0, node->line); /* Fake for now */
    ZCC_EMIT_UNARY(IR_CONST_STR, IR_TY_PTR, dst, lbl_buf,
                   node->line); /* REAL emit */
    return;
  }

  case ND_VAR:
    if (node->sym && node->sym->assigned_reg) {
      fprintf(cc->out, "    movq %s, %%rax\n", node->sym->assigned_reg);
      if (node->type && !node_type_unsigned(node) && !is_pointer(node->type)) {
          if (node->type->size == 4) fprintf(cc->out, "    cltq\n");
          else if (node->type->size == 1) fprintf(cc->out, "    movsbq %%al, %%rax\n");
          else if (node->type->size == 2) fprintf(cc->out, "    movswq %%ax, %%rax\n");
      } else if (node->type && node_type_unsigned(node)) {
          if (node->type->size == 4) fprintf(cc->out, "    movl %%eax, %%eax\n");
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
          if (node->lhs->type->size == 4) fprintf(cc->out, "    cltq\n");
          else if (node->lhs->type->size == 1) fprintf(cc->out, "    movsbq %%al, %%rax\n");
          else if (node->lhs->type->size == 2) fprintf(cc->out, "    movswq %%ax, %%rax\n");
      } else if (node->lhs->type && node_type_unsigned(node->lhs)) {
          if (node->lhs->type->size == 4) fprintf(cc->out, "    movl %%eax, %%eax\n");
          else if (node->lhs->type->size == 1) fprintf(cc->out, "    movzbq %%al, %%rax\n");
          else if (node->lhs->type->size == 2) fprintf(cc->out, "    movzwq %%ax, %%rax\n");
      }
      fprintf(cc->out, "    movq %%rax, %s\n", node->lhs->sym->assigned_reg);
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
        fprintf(cc->out, "    movq %%r11, %%rax\n");
      } else {
        /* If member type is pointer/func, always use movq regardless of member_size */
        if (node->lhs->type && is_pointer(node->lhs->type)) {
          fprintf(cc->out, "    movq %%r11, (%%rax)\n");
        } else {
        switch (node->lhs->member_size) {
        case 1:
          fprintf(cc->out, "    movb %%r11b, (%%rax)\n");
          break;
        case 2:
          fprintf(cc->out, "    movw %%r11w, (%%rax)\n");
          break;
        case 4:
          fprintf(cc->out, "    movl %%r11d, (%%rax)\n");
          break;
        default:
          fprintf(cc->out, "    movq %%r11, (%%rax)\n");
          break;
        }
        }
        fprintf(cc->out, "    movq %%r11, %%rax\n");
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
      fprintf(cc->out, "    movq %%rax, %%r11\n");
      fprintf(cc->out, "    movq %s, %%rax\n", reg);
      switch (node->compound_op) {
      case ND_ADD:
        if (is_pointer(node->lhs->type)) {
          int esz = ptr_elem_size(node->lhs->type);
          if (esz > 1)
            fprintf(cc->out, "    imulq $%d, %%r11\n", esz);
        }
        fprintf(cc->out, "    addq %%r11, %%rax\n");
        break;
      case ND_SUB:
        if (is_pointer(node->lhs->type)) {
          int esz = ptr_elem_size(node->lhs->type);
          if (esz > 1)
            fprintf(cc->out, "    imulq $%d, %%r11\n", esz);
        }
        fprintf(cc->out, "    subq %%r11, %%rax\n");
        break;
      case ND_MUL:
        fprintf(cc->out, "    imulq %%r11, %%rax\n");
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
        fprintf(cc->out, "    andq %%r11, %%rax\n");
        break;
      case ND_BOR:
        fprintf(cc->out, "    orq %%r11, %%rax\n");
        break;
      case ND_BXOR:
        fprintf(cc->out, "    xorq %%r11, %%rax\n");
        break;
      case ND_SHL:
        fprintf(cc->out, "    movq %%r11, %%rcx\n    shlq %%cl, %%rax\n");
        break;
      case ND_SHR:
        if (node->lhs->type && is_unsigned_type(node->lhs->type))
          fprintf(cc->out, "    movq %%r11, %%rcx\n    shrq %%cl, %%rax\n");
        else
          fprintf(cc->out, "    movq %%r11, %%rcx\n    sarq %%cl, %%rax\n");
        break;
      default:
        break;
      }
      if (node->lhs->type && !node_type_unsigned(node->lhs) && !is_pointer(node->lhs->type)) {
          if (node->lhs->type->size == 4) fprintf(cc->out, "    cltq\n");
          else if (node->lhs->type->size == 1) fprintf(cc->out, "    movsbq %%al, %%rax\n");
          else if (node->lhs->type->size == 2) fprintf(cc->out, "    movswq %%ax, %%rax\n");
      } else if (node->lhs->type && node_type_unsigned(node->lhs)) {
          if (node->lhs->type->size == 4) fprintf(cc->out, "    movl %%eax, %%eax\n");
          else if (node->lhs->type->size == 1) fprintf(cc->out, "    movzbq %%al, %%rax\n");
          else if (node->lhs->type->size == 2) fprintf(cc->out, "    movzwq %%ax, %%rax\n");
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
    fprintf(cc->out, "    movq %%rax, %%r11\n");
    pop_reg(cc, "rax");
    switch (node->compound_op) {
    case ND_ADD:
      if (node->lhs->type && is_float_type(node->lhs->type)) {
        fprintf(cc->out, "    movq %%rax, %%xmm0\n");
        fprintf(cc->out, "    movq %%r11, %%xmm1\n");
        fprintf(cc->out, "    addsd %%xmm1, %%xmm0\n");
        fprintf(cc->out, "    movq %%xmm0, %%rax\n");
        break;
      }
      if (is_pointer(node->lhs->type)) {
        int esz;
        esz = ptr_elem_size(node->lhs->type);
        if (esz > 1)
          fprintf(cc->out, "    imulq $%d, %%r11\n", esz);
      }
      fprintf(cc->out, "    addq %%r11, %%rax\n");
      break;
    case ND_SUB:
      if (node->lhs->type && is_float_type(node->lhs->type)) {
        fprintf(cc->out, "    movq %%rax, %%xmm0\n");
        fprintf(cc->out, "    movq %%r11, %%xmm1\n");
        fprintf(cc->out, "    subsd %%xmm1, %%xmm0\n");
        fprintf(cc->out, "    movq %%xmm0, %%rax\n");
        break;
      }
      if (is_pointer(node->lhs->type)) {
        int esz = ptr_elem_size(node->lhs->type);
        if (esz > 1)
          fprintf(cc->out, "    imulq $%d, %%r11\n", esz);
      }
      fprintf(cc->out, "    subq %%r11, %%rax\n");
      break;
    case ND_MUL:
      if (node->lhs->type && is_float_type(node->lhs->type)) {
        fprintf(cc->out, "    movq %%rax, %%xmm0\n");
        fprintf(cc->out, "    movq %%r11, %%xmm1\n");
        fprintf(cc->out, "    mulsd %%xmm1, %%xmm0\n");
        fprintf(cc->out, "    movq %%xmm0, %%rax\n");
        break;
      }
      fprintf(cc->out, "    imulq %%r11, %%rax\n");
      break;
    case ND_DIV:
      if (node->lhs->type && is_float_type(node->lhs->type)) {
        fprintf(cc->out, "    movq %%rax, %%xmm0\n");
        fprintf(cc->out, "    movq %%r11, %%xmm1\n");
        fprintf(cc->out, "    divsd %%xmm1, %%xmm0\n");
        fprintf(cc->out, "    movq %%xmm0, %%rax\n");
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
        fprintf(cc->out, "    movq %%rdx, %%rax\n");
      } else {
        fprintf(cc->out, "    cqo\n    idivq %%r11\n    movq %%rdx, %%rax\n");
      }
      break;
    case ND_BAND:
      fprintf(cc->out, "    andq %%r11, %%rax\n");
      break;
    case ND_BOR:
      fprintf(cc->out, "    orq %%r11, %%rax\n");
      break;
    case ND_BXOR:
      fprintf(cc->out, "    xorq %%r11, %%rax\n");
      break;
    case ND_SHL:
      fprintf(cc->out, "    movq %%r11, %%rcx\n    shlq %%cl, %%rax\n");
      break;
    case ND_SHR:
      if (node->lhs->type && is_unsigned_type(node->lhs->type))
        fprintf(cc->out, "    movq %%r11, %%rcx\n    shrq %%cl, %%rax\n");
      else
        fprintf(cc->out, "    movq %%r11, %%rcx\n    sarq %%cl, %%rax\n");
      break;
    default:
      break;
    }
    fprintf(cc->out, "    movq %%rax, %%r11\n");
    pop_reg(cc, "rax");
    if (node->lhs->kind == ND_MEMBER && node->lhs->member_size > 0) {
      switch (node->lhs->member_size) {
      case 1:
        fprintf(cc->out, "    movb %%r11b, (%%rax)\n");
        break;
      case 2:
        fprintf(cc->out, "    movw %%r11w, (%%rax)\n");
        break;
      case 4:
        fprintf(cc->out, "    movl %%r11d, (%%rax)\n");
        break;
      default:
        fprintf(cc->out, "    movq %%r11, (%%rax)\n");
        break;
      }
    } else {
      switch (type_size(node->lhs->type)) {
      case 1:
        fprintf(cc->out, "    movb %%r11b, (%%rax)\n");
        break;
      case 2:
        fprintf(cc->out, "    movw %%r11w, (%%rax)\n");
        break;
      case 4:
        fprintf(cc->out, "    movl %%r11d, (%%rax)\n");
        break;
      default:
        fprintf(cc->out, "    movq %%r11, (%%rax)\n");
        break;
      }
    }
    fprintf(cc->out, "    movq %%r11, %%rax\n");
    if (node->lhs->type && !node_type_unsigned(node->lhs) && !is_pointer(node->lhs->type)) {
        if (node->lhs->type->size == 4) fprintf(cc->out, "    cltq\n");
        else if (node->lhs->type->size == 1) fprintf(cc->out, "    movsbq %%al, %%rax\n");
        else if (node->lhs->type->size == 2) fprintf(cc->out, "    movswq %%ax, %%rax\n");
    } else if (node->lhs->type && node_type_unsigned(node->lhs)) {
        if (node->lhs->type->size == 4) fprintf(cc->out, "    movl %%eax, %%eax\n");
        else if (node->lhs->type->size == 1) fprintf(cc->out, "    movzbq %%al, %%rax\n");
        else if (node->lhs->type->size == 2) fprintf(cc->out, "    movzwq %%ax, %%rax\n");
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
      codegen_expr_checked(cc, node->lhs);
      if (!is_float_type(node->lhs->type)) {
        fprintf(cc->out, "    cvtsi2sdq %%rax, %%xmm0\n");
      } else {
        fprintf(cc->out, "    movq %%rax, %%xmm0\n");
      }
      fprintf(cc->out, "    subq $8, %%rsp\n");
      fprintf(cc->out, "    movsd %%xmm0, (%%rsp)\n");
      codegen_expr_checked(cc, node->rhs);
      if (!is_float_type(node->rhs->type)) {
        fprintf(cc->out, "    cvtsi2sdq %%rax, %%xmm0\n");
      } else {
        fprintf(cc->out, "    movq %%rax, %%xmm0\n");
      }
      fprintf(cc->out, "    movsd (%%rsp), %%xmm1\n");
      fprintf(cc->out, "    addq $8, %%rsp\n");
      fprintf(cc->out, "    addsd %%xmm1, %%xmm0\n");
      fprintf(cc->out, "    movq %%xmm0, %%rax\n");
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
        char scale_str[32];
        sprintf(scale_str, "%d", esz);
        char *dst = ir_bridge_fresh_tmp();
        ZCC_EMIT_BINARY(IR_MUL, IR_TY_I64, dst, rhs_ir, scale_str, node->line);
        strcpy(rhs_ir, dst);
      }
    } else if (node->rhs->type && is_pointer(node->rhs->type)) {
      int esz = ptr_elem_size(node->rhs->type);
      if (esz > 1) {
        fprintf(cc->out, "    imulq $%d, %%rax\n", esz);
        char scale_str[32];
        sprintf(scale_str, "%d", esz);
        char *dst = ir_bridge_fresh_tmp();
        ZCC_EMIT_BINARY(IR_MUL, IR_TY_I64, dst, lhs_ir, scale_str, node->line);
        strcpy(lhs_ir, dst);
      }
    }
    fprintf(cc->out, "    addq %%r11, %%rax\n");
    if (node->type && type_size(node->type) == 4 && !is_pointer(node->type)) {
      if (node_type_unsigned(node)) {
        fprintf(cc->out, "    movl %%eax, %%eax\n");
      } else {
        fprintf(cc->out, "    cltq\n");
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
      codegen_expr_checked(cc, node->lhs);
      if (!is_float_type(node->lhs->type)) {
        fprintf(cc->out, "    cvtsi2sdq %%rax, %%xmm0\n");
      } else {
        fprintf(cc->out, "    movq %%rax, %%xmm0\n");
      }
      fprintf(cc->out, "    subq $8, %%rsp\n");
      fprintf(cc->out, "    movsd %%xmm0, (%%rsp)\n");
      codegen_expr_checked(cc, node->rhs);
      if (!is_float_type(node->rhs->type)) {
        fprintf(cc->out, "    cvtsi2sdq %%rax, %%xmm0\n");
      } else {
        fprintf(cc->out, "    movq %%rax, %%xmm0\n");
      }
      fprintf(cc->out, "    movsd (%%rsp), %%xmm1\n");
      fprintf(cc->out, "    addq $8, %%rsp\n");
      fprintf(cc->out, "    subsd %%xmm0, %%xmm1\n");
      fprintf(cc->out, "    movsd %%xmm1, %%xmm0\n");
      fprintf(cc->out, "    movq %%xmm0, %%rax\n");
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
        if (esz > 1)
          fprintf(cc->out, "    imulq $%d, %%r11\n", esz);
      }
    }
    fprintf(cc->out, "    subq %%r11, %%rax\n");
    if (node->type && type_size(node->type) == 4 && !is_pointer(node->type)) {
      if (node_type_unsigned(node)) {
        fprintf(cc->out, "    movl %%eax, %%eax\n");
      } else {
        fprintf(cc->out, "    cltq\n");
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
      codegen_expr_checked(cc, node->lhs);
      if (!is_float_type(node->lhs->type)) {
        fprintf(cc->out, "    cvtsi2sdq %%rax, %%xmm0\n");
      } else {
        fprintf(cc->out, "    movq %%rax, %%xmm0\n");
      }
      fprintf(cc->out, "    subq $8, %%rsp\n");
      fprintf(cc->out, "    movsd %%xmm0, (%%rsp)\n");
      codegen_expr_checked(cc, node->rhs);
      if (!is_float_type(node->rhs->type)) {
        fprintf(cc->out, "    cvtsi2sdq %%rax, %%xmm0\n");
      } else {
        fprintf(cc->out, "    movq %%rax, %%xmm0\n");
      }
      fprintf(cc->out, "    movsd (%%rsp), %%xmm1\n");
      fprintf(cc->out, "    addq $8, %%rsp\n");
      fprintf(cc->out, "    mulsd %%xmm1, %%xmm0\n");
      fprintf(cc->out, "    movq %%xmm0, %%rax\n");
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
    if (node->type && type_size(node->type) == 4 && !is_pointer(node->type)) {
      if (node_type_unsigned(node)) {
        fprintf(cc->out, "    movl %%eax, %%eax\n");
      } else {
        fprintf(cc->out, "    cltq\n");
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
      codegen_expr_checked(cc, node->lhs);
      if (!is_float_type(node->lhs->type)) {
        fprintf(cc->out, "    cvtsi2sdq %%rax, %%xmm0\n");
      } else {
        fprintf(cc->out, "    movq %%rax, %%xmm0\n");
      }
      fprintf(cc->out, "    subq $8, %%rsp\n");
      fprintf(cc->out, "    movsd %%xmm0, (%%rsp)\n");
      codegen_expr_checked(cc, node->rhs);
      if (!is_float_type(node->rhs->type)) {
        fprintf(cc->out, "    cvtsi2sdq %%rax, %%xmm0\n");
      } else {
        fprintf(cc->out, "    movq %%rax, %%xmm0\n");
      }
      fprintf(cc->out, "    movsd (%%rsp), %%xmm1\n");
      fprintf(cc->out, "    addq $8, %%rsp\n");
      fprintf(cc->out, "    divsd %%xmm0, %%xmm1\n");
      fprintf(cc->out, "    movsd %%xmm1, %%xmm0\n");
      fprintf(cc->out, "    movq %%xmm0, %%rax\n");
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
    fprintf(cc->out, "    movq %%rax, %%r11\n");
    pop_reg(cc, "rax");
    if (node->lhs->type && node->rhs->type &&
        (is_unsigned_type(node->lhs->type) ||
         is_unsigned_type(node->rhs->type))) {
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
      fprintf(cc->out, "    movabsq $-9223372036854775808, %%r11\n");
      fprintf(cc->out, "    xorq %%r11, %%rax\n");
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
      codegen_expr_checked(cc, node->lhs);
      if (!node->lhs->type || !is_float_type(node->lhs->type)) {
        fprintf(cc->out, "    cvtsi2sdq %%rax, %%xmm0\n");
      } else {
        fprintf(cc->out, "    movq %%rax, %%xmm0\n");
      }
      fprintf(cc->out, "    subq $8, %%rsp\n");
      fprintf(cc->out, "    movsd %%xmm0, (%%rsp)\n");
      codegen_expr_checked(cc, node->rhs);
      if (!node->rhs->type || !is_float_type(node->rhs->type)) {
        fprintf(cc->out, "    cvtsi2sdq %%rax, %%xmm0\n");
      } else {
        fprintf(cc->out, "    movq %%rax, %%xmm0\n");
      }
      fprintf(cc->out, "    movsd (%%rsp), %%xmm1\n");
      fprintf(cc->out, "    addq $8, %%rsp\n");
      fprintf(cc->out, "    ucomisd %%xmm0, %%xmm1\n");
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
    fprintf(cc->out, "    movq %%rax, %%r11\n");
    pop_reg(cc, "rax");
    if (use32)
      fprintf(cc->out,
              "    cmpl %%r11d, %%eax\n"); /* 32-bit: avoids sign-extended imm
                                              in 64-bit */
    else
      fprintf(cc->out, "    cmpq %%r11, %%rax\n");
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
    if (node->type && node->type->kind == TY_FUNC) {
        /* Do nothing: function pointer decays natively */
    } else {
        codegen_load(cc, node->type);
    }
    return;

  case ND_MEMBER:
    codegen_addr_checked(cc, node);
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
        if (node->cast_type && !is_float_type(node->cast_type) && node->lhs && node->lhs->type && is_float_type(node->lhs->type)) {
            fprintf(cc->out, "    movq %%rax, %%xmm0\n");
            fprintf(cc->out, "    cvttsd2si %%xmm0, %%rax\n");
        } else if (node->cast_type->kind == TY_UINT || node->cast_type->kind == TY_ULONG) {
            fprintf(cc->out, "    movl %%eax, %%eax\n");
        } else if (!is_pointer(node->lhs ? node->lhs->type : 0)) {
            fprintf(cc->out, "    cltq\n");
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
                fprintf(cc->out, "    movq %%rax, %%rdx\n");
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
            if (node->lhs && node->lhs->type && is_unsigned_type(node->lhs->type))
                fprintf(cc->out, "    movl %%eax, %%eax\n");
            else
                fprintf(cc->out, "    cltq\n");
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

  case ND_TERNARY:
    if (!node->cond || !node->then_body || !node->else_body) {
      error_at(cc, node->line,
               "codegen_expr: ND_TERNARY missing cond/then/else");
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
    if (node->lhs && node->lhs->kind == ND_VAR && node->lhs->sym &&
        node->lhs->sym->assigned_reg) {
      char *reg = node->lhs->sym->assigned_reg;
      int esz = 1;
      if (is_pointer(node->lhs->type))
        esz = ptr_elem_size(node->lhs->type);
      fprintf(cc->out, "    addq $%d, %s\n", esz, reg);
      fprintf(cc->out, "    movq %s, %%rax\n", reg);
      if (node->lhs->type && !node_type_unsigned(node->lhs) && !is_pointer(node->lhs->type)) {
          if (node->lhs->type->size == 4) fprintf(cc->out, "    cltq\n");
          else if (node->lhs->type->size == 1) fprintf(cc->out, "    movsbq %%al, %%rax\n");
          else if (node->lhs->type->size == 2) fprintf(cc->out, "    movswq %%ax, %%rax\n");
      } else if (node->lhs->type && node_type_unsigned(node->lhs)) {
          if (node->lhs->type->size == 4) fprintf(cc->out, "    movl %%eax, %%eax\n");
          else if (node->lhs->type->size == 1) fprintf(cc->out, "    movzbq %%al, %%rax\n");
          else if (node->lhs->type->size == 2) fprintf(cc->out, "    movzwq %%ax, %%rax\n");
      }
      fprintf(cc->out, "    movq %%rax, %s\n", reg);
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
      if (is_pointer(node->lhs->type))
        esz = ptr_elem_size(node->lhs->type);
      fprintf(cc->out, "    addq $%d, %%rax\n", esz);
    }
    fprintf(cc->out, "    movq %%rax, %%r11\n");
    pop_reg(cc, "rax");
    switch (type_size(node->lhs->type)) {
    case 1:
      fprintf(cc->out, "    movb %%r11b, (%%rax)\n");
      break;
    case 2:
      fprintf(cc->out, "    movw %%r11w, (%%rax)\n");
      break;
    case 4:
      fprintf(cc->out, "    movl %%r11d, (%%rax)\n");
      break;
    default:
      fprintf(cc->out, "    movq %%r11, (%%rax)\n");
      break;
    }
    fprintf(cc->out, "    movq %%r11, %%rax\n");
    if (node->lhs->type && !node_type_unsigned(node->lhs) && !is_pointer(node->lhs->type)) {
        if (node->lhs->type->size == 4) fprintf(cc->out, "    cltq\n");
        else if (node->lhs->type->size == 1) fprintf(cc->out, "    movsbq %%al, %%rax\n");
        else if (node->lhs->type->size == 2) fprintf(cc->out, "    movswq %%ax, %%rax\n");
    } else if (node->lhs->type && node_type_unsigned(node->lhs)) {
        if (node->lhs->type->size == 4) fprintf(cc->out, "    movl %%eax, %%eax\n");
        else if (node->lhs->type->size == 1) fprintf(cc->out, "    movzbq %%al, %%rax\n");
        else if (node->lhs->type->size == 2) fprintf(cc->out, "    movzwq %%ax, %%rax\n");
    }
    return;

  case ND_PRE_DEC:
    if (node->lhs && node->lhs->kind == ND_VAR && node->lhs->sym &&
        node->lhs->sym->assigned_reg) {
      char *reg = node->lhs->sym->assigned_reg;
      int esz = 1;
      if (is_pointer(node->lhs->type))
        esz = ptr_elem_size(node->lhs->type);
      fprintf(cc->out, "    subq $%d, %s\n", esz, reg);
      fprintf(cc->out, "    movq %s, %%rax\n", reg);
      if (node->lhs->type && !node_type_unsigned(node->lhs) && !is_pointer(node->lhs->type)) {
          if (node->lhs->type->size == 4) fprintf(cc->out, "    cltq\n");
          else if (node->lhs->type->size == 1) fprintf(cc->out, "    movsbq %%al, %%rax\n");
          else if (node->lhs->type->size == 2) fprintf(cc->out, "    movswq %%ax, %%rax\n");
      } else if (node->lhs->type && node_type_unsigned(node->lhs)) {
          if (node->lhs->type->size == 4) fprintf(cc->out, "    movl %%eax, %%eax\n");
          else if (node->lhs->type->size == 1) fprintf(cc->out, "    movzbq %%al, %%rax\n");
          else if (node->lhs->type->size == 2) fprintf(cc->out, "    movzwq %%ax, %%rax\n");
      }
      fprintf(cc->out, "    movq %%rax, %s\n", reg);
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
      if (is_pointer(node->lhs->type))
        esz = ptr_elem_size(node->lhs->type);
      fprintf(cc->out, "    subq $%d, %%rax\n", esz);
    }
    fprintf(cc->out, "    movq %%rax, %%r11\n");
    pop_reg(cc, "rax");
    switch (type_size(node->lhs->type)) {
    case 1:
      fprintf(cc->out, "    movb %%r11b, (%%rax)\n");
      break;
    case 2:
      fprintf(cc->out, "    movw %%r11w, (%%rax)\n");
      break;
    case 4:
      fprintf(cc->out, "    movl %%r11d, (%%rax)\n");
      break;
    default:
      fprintf(cc->out, "    movq %%r11, (%%rax)\n");
      break;
    }
    fprintf(cc->out, "    movq %%r11, %%rax\n");
    if (node->lhs->type && !node_type_unsigned(node->lhs) && !is_pointer(node->lhs->type)) {
        if (node->lhs->type->size == 4) fprintf(cc->out, "    cltq\n");
        else if (node->lhs->type->size == 1) fprintf(cc->out, "    movsbq %%al, %%rax\n");
        else if (node->lhs->type->size == 2) fprintf(cc->out, "    movswq %%ax, %%rax\n");
    } else if (node->lhs->type && node_type_unsigned(node->lhs)) {
        if (node->lhs->type->size == 4) fprintf(cc->out, "    movl %%eax, %%eax\n");
        else if (node->lhs->type->size == 1) fprintf(cc->out, "    movzbq %%al, %%rax\n");
        else if (node->lhs->type->size == 2) fprintf(cc->out, "    movzwq %%ax, %%rax\n");
    }
    return;

  case ND_POST_INC:
    if (node->lhs && node->lhs->kind == ND_VAR && node->lhs->sym &&
        node->lhs->sym->assigned_reg) {
      char *reg = node->lhs->sym->assigned_reg;
      int esz = 1;
      if (is_pointer(node->lhs->type))
        esz = ptr_elem_size(node->lhs->type);
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
    push_reg(cc, "rax"); /* save original value */
    {
      int esz;
      esz = 1;
      if (is_pointer(node->lhs->type))
        esz = ptr_elem_size(node->lhs->type);
      fprintf(cc->out, "    addq $%d, %%rax\n", esz);
    }
    fprintf(cc->out, "    movq %%rax, %%r11\n");
    pop_reg(cc, "rdx"); /* rdx = original value */
    pop_reg(cc, "rax"); /* rax = address */
    switch (type_size(node->lhs->type)) {
    case 1:
      fprintf(cc->out, "    movb %%r11b, (%%rax)\n");
      break;
    case 2:
      fprintf(cc->out, "    movw %%r11w, (%%rax)\n");
      break;
    case 4:
      fprintf(cc->out, "    movl %%r11d, (%%rax)\n");
      break;
    default:
      fprintf(cc->out, "    movq %%r11, (%%rax)\n");
      break;
    }
    fprintf(cc->out, "    movq %%rdx, %%rax\n");
    return;

  case ND_POST_DEC:
    if (node->lhs && node->lhs->kind == ND_VAR && node->lhs->sym &&
        node->lhs->sym->assigned_reg) {
      char *reg = node->lhs->sym->assigned_reg;
      int esz = 1;
      if (is_pointer(node->lhs->type))
        esz = ptr_elem_size(node->lhs->type);
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
      if (is_pointer(node->lhs->type))
        esz = ptr_elem_size(node->lhs->type);
      fprintf(cc->out, "    subq $%d, %%rax\n", esz);
    }
    fprintf(cc->out, "    movq %%rax, %%r11\n");
    pop_reg(cc, "rdx"); /* rdx = original value */
    pop_reg(cc, "rax"); /* rax = address */
    switch (type_size(node->lhs->type)) {
    case 1:
      fprintf(cc->out, "    movb %%r11b, (%%rax)\n");
      break;
    case 2:
      fprintf(cc->out, "    movw %%r11w, (%%rax)\n");
      break;
    case 4:
      fprintf(cc->out, "    movl %%r11d, (%%rax)\n");
      break;
    default:
      fprintf(cc->out, "    movq %%r11, (%%rax)\n");
      break;
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
      for (i = 0; i < nargs; i++) {
        if (node->args[i] && node->args[i]->type && is_float_type(node->args[i]->type)) {
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
      ir_save_result(&args_ir_1d[i * 32]);
      push_reg(cc, "rax");
    }

    /* pop args into correct registers: floats->xmm, ints->gpregs independently */
    {
      int gp_idx = 0;
      int fp_idx = 0;
      for (i = 0; i < nargs; i++) {
        if (node->args[i] && node->args[i]->type && is_float_type(node->args[i]->type)) {
          if (fp_idx < 8) {
            fprintf(cc->out, "    popq %%rax\n");
            cc->stack_depth--;
            fprintf(cc->out, "    movq %%rax, %%xmm%d\n", fp_idx);
            fp_idx++;
          }
        } else {
          if (gp_idx < 6) {
            pop_reg(cc, argregs[gp_idx]);
            gp_idx++;
          }
        }
      }
      fprintf(cc->out, "    movl $%d, %%eax\n", fp_idx > 8 ? 8 : fp_idx);
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
      fprintf(cc->out, "    call %s\n", node->func_name);
    }

    if (node->type && is_float_type(node->type)) {
        fprintf(cc->out, "    movq %%xmm0, %%rax\n");
    } else if (node->type && !node_type_unsigned(node)) {
        if (node->type->size == 4) fprintf(cc->out, "    cltq\n");
        else if (node->type->size == 1) fprintf(cc->out, "    movsbq %%al, %%rax\n");
        else if (node->type->size == 2) fprintf(cc->out, "    movswq %%ax, %%rax\n");
    } else if (node->type && node_type_unsigned(node)) {
        if (node->type->size == 4) fprintf(cc->out, "    movl %%eax, %%eax\n");
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