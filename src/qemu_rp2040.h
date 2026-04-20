#ifndef QEMU_RP2040_H
#define QEMU_RP2040_H

#include "ir.h"
#include "tqpu_neural.h"
#include <stdio.h>

void qemu_rp2040_emit_bootloader(ir_func_t *f, FILE *out);  // RP2040 boot stub + TQPU neural header
void qemu_rp2040_launch(const char *elf_path);  // QEMU command with sentient weights

#endif
