/* test_cg_ir_004.c — CG-IR-004 Parameter Slot Aliasing Reproducer
 *
 * Compile with BOTH backends and compare output:
 *
 *   AST: ./zcc test_cg_ir_004.c -o test_ast.s && gcc -o test_ast test_ast.s
 *   IR:  [with IR lowerer] → gcc -o test_ir test_ir.s
 *
 *   ./test_ast hello world foo   → argc=4 argv[1]=hello  (CORRECT)
 *   ./test_ir  hello world foo   → garbage or crash       (BUG)
 *
 * The bug: get_or_create_var assigns argc/argv to encounter-order
 * slots instead of ABI-fixed -8(%rbp)/-16(%rbp).
 *
 * After fix: both backends produce identical output.
 */

int printf(const char *fmt, ...);
int strcmp(const char *a, const char *b);

int main(int argc, char **argv) {
    /* Declare locals that push argc/argv later in encounter order */
    int x;
    int y;
    int z;
    int i;
    char *input_file;
    char *output_file;
    int found_o;

    x = 10;
    y = 20;
    z = x + y;

    /* These reads MUST hit the prologue-written slots */
    printf("argc=%d\n", argc);

    if (argc < 1) {
        printf("FAIL: argc < 1 (got %d)\n", argc);
        return 1;
    }

    printf("argv[0]=%s\n", argv[0]);

    /* Simulate ZCC's argv parsing loop — this is what breaks in main */
    input_file = 0;
    output_file = 0;
    found_o = 0;
    i = 1;
    while (i < argc) {
        if (found_o) {
            output_file = argv[i];
            found_o = 0;
        } else if (strcmp(argv[i], "-o") == 0) {
            found_o = 1;
        } else {
            input_file = argv[i];
        }
        i = i + 1;
    }

    if (input_file) {
        printf("input_file=%s\n", input_file);
    } else {
        printf("FAIL: input_file is NULL\n");
    }

    if (output_file) {
        printf("output_file=%s\n", output_file);
    }

    printf("z=%d (expect 30)\n", z);
    return z - 30;  /* should return 0 */
}
