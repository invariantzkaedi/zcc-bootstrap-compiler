/* === CLUSTER DEF-PP-01: Variadic Macro Elision & Macro Symbols === */
#define VAR_EMPTY(fmt, ...) fmt
#define VAR_PASSTHRU(fmt, ...) fmt
#define WRAP_NAME(x, num) x ## _ ## num

const char *s_empty = VAR_EMPTY("zero_args");
const char *s_pass = VAR_PASSTHRU("fmt_msg", 1, 2, 3);
int WRAP_NAME(token, 1) = 100;
int WRAP_NAME(token, 2) = 200;

/* === CLUSTER DEF-PP-02: Token Concatenation (##) & Placemarkers === */
#define GLUE(a, b) a ## b
#define GLUE3(a, b, c) a ## b ## c
#define STRINGIFY(x) #x
#define EXPAND_AND_STR(x) STRINGIFY(x)

int GLUE(tok_, 123) = 123;
int GLUE3(trip_, let_, val) = 999;
const char *empty_str = STRINGIFY();
const char *ident_str = STRINGIFY(sample_identifier_456);

/* === CLUSTER DEF-PP-03: Hide-Set & Recursive Expansion Guard === */
#define SELF_REC (SELF_REC + 1)
#define IND_A(x) IND_B(x)
#define IND_B(x) IND_C(x)
#define IND_C(x) ((x) * 2)

int test_rec_val = IND_A(21);

/* === CLUSTER DEF-PP-04: Constant Expression Evaluator (#if/#elif) === */
#if (1 << 4) == 16 && (0xFF >> 4) == 0xF && (1 ^ 3) == 2 && (1 | 2) == 3 && (3 & 1) == 1
int pp_math_bitwise = 1;
#else
int pp_math_bitwise = 0;
#endif

#if (5 > 2 ? (10 < 20 ? 100 : 200) : 300) == 100
int pp_math_nested_ternary = 1;
#else
int pp_math_nested_ternary = 0;
#endif

#if ('A' == 65) && ('\n' == 10) && ('\0' == 0)
int pp_math_char_consts = 1;
#endif

#if (-5 + 10 * 2) == 15 && (100 / 5 % 7) == 6
int pp_math_precedence = 1;
#endif

/* === CLUSTER DEF-PP-05: Nested Conditionals & Directive Handling === */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"

#ifdef DEF_LEVEL_0
int lev0 = 0;
#elif defined(DEF_LEVEL_1)
int lev1 = 1;
#else
  #if 1
    #if 0
    int dead_code_1 = 1;
    #elif 1
      #if 0
      int dead_code_2 = 2;
      #else
      int deep_nested_pass = 42;
      #endif
    #endif
  #endif
#endif

#pragma GCC diagnostic pop

/* === CLUSTER DEF-PP-06: Line Splicing & Dynamic Expansion === */
#define MULTILINE_MACRO(a, b, c) \
    ((a) + \
     (b) * \
     (c))

int test_pp_torture_main() {
    int spliced = MULTILINE_MACRO(10, 20, 30);
    return tok_123 + trip_let_val + test_rec_val + pp_math_bitwise +
           pp_math_nested_ternary + deep_nested_pass + spliced +
           token_1 + token_2;
}