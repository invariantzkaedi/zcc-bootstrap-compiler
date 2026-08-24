
#include <stdio.h>
#include "../src/riscv_codegen.h"

int main() {
    printf("REG_ZERO:%s\n", riscv_get_reg_name(RISCV_REG_ZERO));
    printf("REG_RA:%s\n", riscv_get_reg_name(RISCV_REG_RA));
    printf("REG_SP:%s\n", riscv_get_reg_name(RISCV_REG_SP));
    printf("REG_FP:%s\n", riscv_get_reg_name(RISCV_REG_FP));
    printf("REG_A0:%s\n", riscv_get_reg_name(RISCV_REG_A0));
    printf("REG_A1:%s\n", riscv_get_reg_name(RISCV_REG_A1));
    
    size_t align_8 = riscv_align_stack_frame(8);
    size_t align_24 = riscv_align_stack_frame(24);
    printf("ALIGN_8:%zu\n", align_8);
    printf("ALIGN_24:%zu\n", align_24);

    int res = zcc_emit_riscv_assembly_to_file("/tmp/test_riscv_out.s", "my_riscv_func", 16);
    printf("RISCV_EMIT_RES:%d\n", res);
    return 0;
}
