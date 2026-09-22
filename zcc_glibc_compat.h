/* zcc_glibc_compat.h -- Modular glibc GNU Extension Tolerator & Builtin Folds (LIMIT-005) */
#ifndef ZCC_GLIBC_COMPAT_H
#define ZCC_GLIBC_COMPAT_H

/* Common POSIX / glibc type definitions required by GNU stdio.h, stdlib.h, sys/types.h */
#define ZCC_GLIBC_COMPAT_TYPES \
  "typedef int wchar_t;\n" \
  "typedef __builtin_va_list __gnuc_va_list;\n" \
  "#define _VA_LIST_DEFINED 1\n" \
  "#define __GNUC_VA_LIST 1\n" \
  "typedef long __off_t;\n" \
  "typedef long __off64_t;\n" \
  "typedef long __ssize_t;\n" \
  "typedef unsigned int __uint32_t;\n" \
  "typedef int __int32_t;\n" \
  "typedef unsigned long __uint64_t;\n" \
  "typedef long __int64_t;\n" \
  "typedef unsigned short __uint16_t;\n" \
  "typedef short __int16_t;\n" \
  "typedef unsigned char __uint8_t;\n" \
  "typedef signed char __int8_t;\n" \
  "typedef int __pid_t;\n" \
  "typedef unsigned int __mode_t;\n" \
  "typedef unsigned int __socklen_t;\n" \
  "typedef long __time_t;\n"

/* Safe macro body assigner that resizes buffer if body exceeds initial body_cap */
static void zcc_set_macro_body(PPMacro *m, const char *body) {
  if (!m) return;
  int len = (int)strlen(body);
  if (len >= m->body_cap) {
    if (m->body) free(m->body);
    m->body_cap = len + 128;
    m->body = (char *)calloc(1, m->body_cap);
  }
  strcpy(m->body, body);
}

/* Inject glibc macro absorbers directly into preprocessor state */
static void zcc_glibc_init_macros(PPState *state) {
  PPMacro *m;
  m = pp_find_macro(state, "__GNUC__");
  if (m) zcc_set_macro_body(m, "4");
  m = pp_add_macro(state, "__GNUC_MINOR__");
  if (m) zcc_set_macro_body(m, "2");
  m = pp_add_macro(state, "__GNUC_PATCHLEVEL__");
  if (m) zcc_set_macro_body(m, "0");
  m = pp_add_macro(state, "__USER_LABEL_PREFIX__");
  if (m) zcc_set_macro_body(m, "\"\"");
  m = pp_add_macro(state, "__leaf__");
  if (m) zcc_set_macro_body(m, "");
  m = pp_add_macro(state, "__THROW");
  if (m) zcc_set_macro_body(m, "");
  m = pp_add_macro(state, "__THROWNL");
  if (m) zcc_set_macro_body(m, "");
  m = pp_add_macro(state, "__wur");
  if (m) zcc_set_macro_body(m, "");
  m = pp_add_macro(state, "__cold");
  if (m) zcc_set_macro_body(m, "");
  m = pp_add_macro(state, "__hot");
  if (m) zcc_set_macro_body(m, "");
  m = pp_add_macro(state, "__returns_nonnull");
  if (m) zcc_set_macro_body(m, "");
  m = pp_add_macro(state, "__attribute_malloc__");
  if (m) zcc_set_macro_body(m, "");
  m = pp_add_macro(state, "_VA_LIST_DEFINED");
  if (m) zcc_set_macro_body(m, "1");
  m = pp_add_macro(state, "__GNUC_VA_LIST");
  if (m) zcc_set_macro_body(m, "1");
  m = pp_add_macro(state, "__attribute_alloc_size__");
  if (m) {
    m->is_function_like = 1;
    m->num_params = 1;
    strcpy(m->params[0], "a");
    zcc_set_macro_body(m, "");
  }
  m = pp_add_macro(state, "__attribute_format_arg__");
  if (m) {
    m->is_function_like = 1;
    m->num_params = 1;
    strcpy(m->params[0], "a");
    zcc_set_macro_body(m, "");
  }
  m = pp_add_macro(state, "__glibc_unlikely");
  if (m) {
    m->is_function_like = 1;
    m->num_params = 1;
    strcpy(m->params[0], "c");
    zcc_set_macro_body(m, "(c)");
  }
  m = pp_add_macro(state, "__glibc_likely");
  if (m) {
    m->is_function_like = 1;
    m->num_params = 1;
    strcpy(m->params[0], "c");
    zcc_set_macro_body(m, "(c)");
  }
  m = pp_add_macro(state, "__builtin_bswap16");
  if (m) {
    m->is_function_like = 1;
    m->num_params = 1;
    strcpy(m->params[0], "x");
    zcc_set_macro_body(m, "((unsigned short)((((x) >> 8) & 0xff) | (((x) & 0xff) << 8)))");
  }
  m = pp_add_macro(state, "__builtin_bswap32");
  if (m) {
    m->is_function_like = 1;
    m->num_params = 1;
    strcpy(m->params[0], "x");
    zcc_set_macro_body(m, "((((x) & 0xff000000U) >> 24) | (((x) & 0x00ff0000U) >> 8) | (((x) & 0x0000ff00U) << 8) | (((x) & 0x000000ffU) << 24))");
  }
  m = pp_add_macro(state, "__builtin_bswap64");
  if (m) {
    m->is_function_like = 1;
    m->num_params = 1;
    strcpy(m->params[0], "x");
    zcc_set_macro_body(m, "((((x) & 0xff00000000000000ULL) >> 56) | (((x) & 0x00ff000000000000ULL) >> 40) | (((x) & 0x0000ff0000000000ULL) >> 24) | (((x) & 0x000000ff00000000ULL) >> 8) | (((x) & 0x00000000ff000000ULL) << 8) | (((x) & 0x0000000000ff0000ULL) << 24) | (((x) & 0x000000000000ff00ULL) << 40) | (((x) & 0x00000000000000ffULL) << 56))");
  }
}

#endif /* ZCC_GLIBC_COMPAT_H */
