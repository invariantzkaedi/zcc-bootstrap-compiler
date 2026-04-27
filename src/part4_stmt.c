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
      fprintf(cc->out, "    cmpq $0, %%rax\n");
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
      if (g_ir_cur_func && g_ir_cur_func->tail &&
          !ir_op_is_terminator(g_ir_cur_func->tail->op)) {
        sprintf(ir_lbl, ".L%d", end_lbl);
        ZCC_EMIT_BR(ir_lbl, node->line);
      }
    }
    if (node->default_case) {
      emit_label_fmt(cc, default_lbl, FMT_DEF);
      sprintf(ir_lbl, ".L%d", default_lbl);
      ZCC_EMIT_LABEL(ir_lbl, node->line);
      if (node->default_case->case_body)
        codegen_stmt(cc, node->default_case->case_body);
      if (g_ir_cur_func && g_ir_cur_func->tail &&
          !ir_op_is_terminator(g_ir_cur_func->tail->op)) {
        sprintf(ir_lbl, ".L%d", end_lbl);
        ZCC_EMIT_BR(ir_lbl, node->line);
      }
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