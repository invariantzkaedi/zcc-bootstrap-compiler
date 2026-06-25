/* ================================================================ */
/* C AST JSON SERIALIZER                                             */
/* ================================================================ */

enum { ZCC_AST_JSON_MAX_DEPTH = 256 };

/* Map Node kind enum values to stable JSON strings; unknown values become ND_UNKNOWN. */
static const char *zcc_ast_kind_name(int kind) {
  switch (kind) {
    case ND_NUM: return "ND_NUM"; case ND_STR: return "ND_STR"; case ND_CHAR_LIT: return "ND_CHAR_LIT"; case ND_FLIT: return "ND_FLIT";
    case ND_VAR: return "ND_VAR"; case ND_ASSIGN: return "ND_ASSIGN"; case ND_ADD: return "ND_ADD"; case ND_SUB: return "ND_SUB";
    case ND_MUL: return "ND_MUL"; case ND_DIV: return "ND_DIV"; case ND_MOD: return "ND_MOD"; case ND_EQ: return "ND_EQ";
    case ND_NE: return "ND_NE"; case ND_LT: return "ND_LT"; case ND_LE: return "ND_LE"; case ND_GT: return "ND_GT";
    case ND_GE: return "ND_GE"; case ND_LAND: return "ND_LAND"; case ND_LOR: return "ND_LOR"; case ND_LNOT: return "ND_LNOT";
    case ND_BAND: return "ND_BAND"; case ND_BOR: return "ND_BOR"; case ND_BXOR: return "ND_BXOR"; case ND_BNOT: return "ND_BNOT";
    case ND_SHL: return "ND_SHL"; case ND_SHR: return "ND_SHR"; case ND_FADD: return "ND_FADD"; case ND_FSUB: return "ND_FSUB";
    case ND_FMUL: return "ND_FMUL"; case ND_FDIV: return "ND_FDIV"; case ND_NEG: return "ND_NEG"; case ND_ADDR: return "ND_ADDR";
    case ND_DEREF: return "ND_DEREF"; case ND_CALL: return "ND_CALL"; case ND_RETURN: return "ND_RETURN"; case ND_BLOCK: return "ND_BLOCK";
    case ND_IF: return "ND_IF"; case ND_WHILE: return "ND_WHILE"; case ND_FOR: return "ND_FOR"; case ND_DO_WHILE: return "ND_DO_WHILE";
    case ND_BREAK: return "ND_BREAK"; case ND_CONTINUE: return "ND_CONTINUE"; case ND_GOTO: return "ND_GOTO"; case ND_GOTO_COMPUTED: return "ND_GOTO_COMPUTED";
    case ND_LABEL: return "ND_LABEL"; case ND_SWITCH: return "ND_SWITCH"; case ND_CASE: return "ND_CASE"; case ND_DEFAULT: return "ND_DEFAULT";
    case ND_CAST: return "ND_CAST"; case ND_SIZEOF: return "ND_SIZEOF"; case ND_VA_ARG: return "ND_VA_ARG"; case ND_ADDR_LABEL: return "ND_ADDR_LABEL";
    case ND_MEMBER: return "ND_MEMBER"; case ND_PRE_INC: return "ND_PRE_INC"; case ND_PRE_DEC: return "ND_PRE_DEC"; case ND_POST_INC: return "ND_POST_INC";
    case ND_POST_DEC: return "ND_POST_DEC"; case ND_TERNARY: return "ND_TERNARY"; case ND_COMMA_EXPR: return "ND_COMMA_EXPR"; case ND_FUNC_DEF: return "ND_FUNC_DEF";
    case ND_GLOBAL_VAR: return "ND_GLOBAL_VAR"; case ND_COMPOUND_ASSIGN: return "ND_COMPOUND_ASSIGN"; case ND_INIT_LIST: return "ND_INIT_LIST"; case ND_ASM: return "ND_ASM";
    case ND_NOP: return "ND_NOP";
  }
  return "ND_UNKNOWN";
}

/* Map Type kind enum values to stable JSON strings; unknown values become TY_UNKNOWN. */
static const char *zcc_type_kind_name(int kind) {
  switch (kind) {
    case TY_VOID: return "TY_VOID"; case TY_CHAR: return "TY_CHAR"; case TY_UCHAR: return "TY_UCHAR"; case TY_SHORT: return "TY_SHORT";
    case TY_USHORT: return "TY_USHORT"; case TY_INT: return "TY_INT"; case TY_UINT: return "TY_UINT"; case TY_LONG: return "TY_LONG";
    case TY_ULONG: return "TY_ULONG"; case TY_LONGLONG: return "TY_LONGLONG"; case TY_ULONGLONG: return "TY_ULONGLONG"; case TY_FLOAT: return "TY_FLOAT";
    case TY_DOUBLE: return "TY_DOUBLE"; case TY_PTR: return "TY_PTR"; case TY_ARRAY: return "TY_ARRAY"; case TY_FUNC: return "TY_FUNC";
    case TY_STRUCT: return "TY_STRUCT"; case TY_UNION: return "TY_UNION"; case TY_ENUM: return "TY_ENUM"; case TY_LONGDOUBLE: return "TY_LONGDOUBLE";
  }
  return "TY_UNKNOWN";
}

/* Write a JSON-escaped string slice, including quote/backslash/control escapes. */
static void zcc_json_strn(FILE *out, const char *s, int len) {
  int i; unsigned char c;
  fputc('"', out);
  if (s) for (i = 0; i < len; i++) {
    c = (unsigned char)s[i];
    if (c == '"' || c == '\\') { fputc('\\', out); fputc(c, out); }
    else if (c == '\n') fputs("\\n", out);
    else if (c == '\r') fputs("\\r", out);
    else if (c == '\t') fputs("\\t", out);
    else if (c < 32) fprintf(out, "\\u%04x", (int)c);
    else fputc(c, out);
  }
  fputc('"', out);
}

static void zcc_json_str(FILE *out, const char *s) { zcc_json_strn(out, s ? s : "", s ? (int)strlen(s) : 0); }
static void zcc_json_field(FILE *out, int *first, const char *name) { if (!*first) fputc(',', out); *first = 0; zcc_json_str(out, name); fputc(':', out); }

static void zcc_ast_json_type(FILE *out, Type *t) {
  if (!t) { fputs("null", out); return; }
  fputc('{', out); fputs("\"kind\":", out); zcc_json_str(out, zcc_type_kind_name(t->kind));
  fprintf(out, ",\"size\":%d,\"align\":%d", t->size, t->align);
  if (t->base) { fprintf(out, ",\"base\":"); zcc_json_str(out, zcc_type_kind_name(t->base->kind)); }
  if (t->array_len) fprintf(out, ",\"array_len\":%d", t->array_len);
  if (t->tag[0]) { fprintf(out, ",\"tag\":"); zcc_json_str(out, t->tag); }
  fputc('}', out);
}

static void zcc_ast_json_node(FILE *out, Compiler *cc, Node *n, int depth);
static int zcc_ast_node_has_children(Node *n) {
  return n && (n->lhs || n->rhs || n->cond || n->then_body || n->else_body ||
               n->init || n->inc || n->body || n->case_body || n->initializer ||
               n->default_case || (n->args && n->num_args) ||
               (n->stmts && n->num_stmts) || (n->cases && n->num_cases));
}

static void zcc_ast_json_array(FILE *out, Compiler *cc, Node **items, int count, int depth) {
  int i;
  fputc('[', out);
  for (i = 0; i < count; i++) { if (i) fputc(',', out); zcc_ast_json_node(out, cc, items[i], depth + 1); }
  fputc(']', out);
}

static void zcc_ast_json_node(FILE *out, Compiler *cc, Node *n, int depth) {
  int first; int i;
  if (!n) { fputs("null", out); return; }
  if (depth > ZCC_AST_JSON_MAX_DEPTH) { fputs("{\"kind\":\"DEPTH_LIMIT\"}", out); return; }
  first = 1; fputc('{', out);
  zcc_json_field(out, &first, "kind"); zcc_json_str(out, zcc_ast_kind_name(n->kind));
  zcc_json_field(out, &first, "line"); fprintf(out, "%d", n->line);
  zcc_json_field(out, &first, "type"); zcc_ast_json_type(out, n->type);
  if (n->name[0]) { zcc_json_field(out, &first, "name"); zcc_json_str(out, n->name); }
  if (n->func_def_name[0]) { zcc_json_field(out, &first, "function"); zcc_json_str(out, n->func_def_name); }
  if (n->func_name[0]) { zcc_json_field(out, &first, "callee"); zcc_json_str(out, n->func_name); }
  if (n->member_name[0]) { zcc_json_field(out, &first, "member"); zcc_json_str(out, n->member_name); }
  if (n->label_name[0]) { zcc_json_field(out, &first, "label"); zcc_json_str(out, n->label_name); }
  if (n->kind == ND_NUM || n->kind == ND_CHAR_LIT) { zcc_json_field(out, &first, "int_val"); fprintf(out, "%lld", n->int_val); }
  if (n->kind == ND_FLIT) { zcc_json_field(out, &first, "float_val"); fprintf(out, "%.17g", n->f_val); }
  if (n->kind == ND_STR && cc && n->str_id >= 0 && n->str_id < cc->num_strings) {
    zcc_json_field(out, &first, "string");
    zcc_json_strn(out, cc->strings[n->str_id].data, cc->strings[n->str_id].len);
  }
  if (n->kind == ND_CASE) { zcc_json_field(out, &first, "case_val"); fprintf(out, "%lld", n->case_val); }
  if (n->kind == ND_COMPOUND_ASSIGN) { zcc_json_field(out, &first, "compound_op"); zcc_json_str(out, zcc_ast_kind_name(n->compound_op)); }
  if (n->func_params && n->num_params > 0) {
    zcc_json_field(out, &first, "params"); fputc('[', out);
    for (i = 0; i < n->num_params; i++) { if (i) fputc(',', out); fputs("{\"name\":", out); zcc_json_str(out, n->func_params->names[i]); fputs(",\"type\":", out); zcc_ast_json_type(out, n->func_params->types[i]); fputc('}', out); }
    fputc(']', out);
  }
  if (zcc_ast_node_has_children(n)) {
    zcc_json_field(out, &first, "children"); fputc('{', out); i = 1;
    if (n->lhs) { zcc_json_field(out, &i, "lhs"); zcc_ast_json_node(out, cc, n->lhs, depth + 1); }
    if (n->rhs) { zcc_json_field(out, &i, "rhs"); zcc_ast_json_node(out, cc, n->rhs, depth + 1); }
    if (n->cond) { zcc_json_field(out, &i, "cond"); zcc_ast_json_node(out, cc, n->cond, depth + 1); }
    if (n->then_body) { zcc_json_field(out, &i, "then"); zcc_ast_json_node(out, cc, n->then_body, depth + 1); }
    if (n->else_body) { zcc_json_field(out, &i, "else"); zcc_ast_json_node(out, cc, n->else_body, depth + 1); }
    if (n->init) { zcc_json_field(out, &i, "init"); zcc_ast_json_node(out, cc, n->init, depth + 1); }
    if (n->inc) { zcc_json_field(out, &i, "inc"); zcc_ast_json_node(out, cc, n->inc, depth + 1); }
    if (n->body) { zcc_json_field(out, &i, "body"); zcc_ast_json_node(out, cc, n->body, depth + 1); }
    if (n->case_body) { zcc_json_field(out, &i, "case_body"); zcc_ast_json_node(out, cc, n->case_body, depth + 1); }
    if (n->initializer) { zcc_json_field(out, &i, "initializer"); zcc_ast_json_node(out, cc, n->initializer, depth + 1); }
    if (n->default_case) { zcc_json_field(out, &i, "default"); zcc_ast_json_node(out, cc, n->default_case, depth + 1); }
    if (n->args && n->num_args) { zcc_json_field(out, &i, "args"); zcc_ast_json_array(out, cc, n->args, n->num_args, depth + 1); }
    if (n->stmts && n->num_stmts) { zcc_json_field(out, &i, "stmts"); zcc_ast_json_array(out, cc, n->stmts, n->num_stmts, depth + 1); }
    if (n->cases && n->num_cases) { zcc_json_field(out, &i, "cases"); zcc_ast_json_array(out, cc, n->cases, n->num_cases, depth + 1); }
    fputc('}', out);
  }
  fputc('}', out);
}

void zcc_serialize_ast_json(FILE *out, Compiler *cc, Node *prog) {
  Node *n; int first;
  first = 1;
  fputs("{\"schema\":\"zcc.ast.v1\",\"file\":", out);
  zcc_json_str(out, cc && cc->filename ? cc->filename : "");
  fputs(",\"nodes\":[", out);
  for (n = prog; n; n = n->next) { if (!first) fputc(',', out); first = 0; zcc_ast_json_node(out, cc, n, 0); }
  fputs("]}\n", out);
}
