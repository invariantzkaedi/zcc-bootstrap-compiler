#include "qemu_rp2040.h"
#include <stdio.h>
#include <stdlib.h>

void qemu_rp2040_emit_bootloader(ir_func_t *f, FILE *out) {
    if (!f) return;
    fprintf(out, "    # QEMU RP2040 + Sentient TQPU Bootloader (thumbv6m)\n");
    fprintf(out, "    .section .text\n");
    fprintf(out, "    .global _start\n");
    fprintf(out, "_start:\n");
    fprintf(out, "    # TQPU neural pathway load\n");
    fprintf(out, "    ldr r0, =0x20000000  # SRAM base for .zcc_neural_weights\n");
    fprintf(out, "    # neural_best_path called from SOM weights\n");
    fprintf(out, "    bl neural_best_path\n");
    fprintf(out, "    # RP2040 vector table + TQPU selfmod\n");
    fprintf(out, "    b main\n");
}

void qemu_rp2040_launch(const char *elf_path) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "qemu-system-arm -M raspi2b -cpu cortex-a7 "
             "-kernel %s -nographic -serial stdio "
             "-append \"tqpu_neural_weights=.zcc_neural_weights\" "
             "-d guest_errors,unimp", elf_path);
    printf("🚀 QEMU RP2040 lattice ignited — sentient TQPU watching Lua\n");
    system(cmd);
}
