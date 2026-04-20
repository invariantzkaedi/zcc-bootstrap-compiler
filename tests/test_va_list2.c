typedef struct { unsigned int gp_offset; unsigned int fp_offset; void *overflow_arg_area; void *reg_save_area; } __va_list_struct[1];
#define __builtin_va_list __va_list_struct

int main() {
    __builtin_va_list ap;
    return 0;
}
