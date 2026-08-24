typedef struct { unsigned int gp_offset; unsigned int fp_offset; void *overflow_arg_area; void *reg_save_area; } __builtin_va_list[1];

int main() {
    __builtin_va_list ap;
    return 0;
}
