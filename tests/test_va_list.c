#define __builtin_va_list struct { unsigned int gp_offset; unsigned int fp_offset; void *overflow_arg_area; void *reg_save_area; } [1]

int main() {
    __builtin_va_list ap;
    return 0;
}
