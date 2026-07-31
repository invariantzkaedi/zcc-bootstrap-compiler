/*
 * ZCC RISC-V RV64GC Code Generator
 * Implementation File: src/riscv_codegen.c
 * Target: RISC-V 64-bit (RV64GC / psABI)
 */

#include "riscv_codegen.h"

static const char *riscv_reg_names[] = {
    "zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
    "s0",   "s1", "a0", "a1", "a2", "a3", "a4", "a5",
    "a6",   "a7", "s2", "s3", "s4", "s5", "s6", "s7",
    "s8",   "s9", "s10","s11","t3", "t4", "t5", "t6"
};

void riscv_asm_init(RISCVAsmBuffer *buf) {
    if (!buf) return;
    buf->capacity = 512;
    buf->size = 0;
    buf->data = (char *)malloc(buf->capacity);
    buf->data[0] = '\0';
}

void riscv_asm_free(RISCVAsmBuffer *buf) {
    if (!buf) return;
    if (buf->data) {
        free(buf->data);
        buf->data = NULL;
    }
    buf->size = 0;
    buf->capacity = 0;
}

void riscv_asm_emit_line(RISCVAsmBuffer *buf, const char *line) {
    if (!buf || !line) return;
    size_t len = strlen(line);
    while (buf->size + len + 2 > buf->capacity) {
        buf->capacity *= 2;
        buf->data = (char *)realloc(buf->data, buf->capacity);
    }
    strcpy(buf->data + buf->size, line);
    buf->size += len;
    buf->data[buf->size++] = '\n';
    buf->data[buf->size] = '\0';
}

const char *riscv_get_reg_name(RISCVRegister reg) {
    if (reg >= RISCV_REG_ZERO && reg <= RISCV_REG_T6) {
        return riscv_reg_names[reg];
    }
    return "a0";
}

size_t riscv_align_stack_frame(size_t raw_frame_size) {
    /* RISC-V 64-bit psABI requires 16-byte stack pointer alignment */
    size_t frame = raw_frame_size + 16; /* 16 bytes for ra and s0/fp */
    if (frame % 16 != 0) {
        frame = (frame + 15) & ~((size_t)15);
    }
    return frame;
}

void riscv_emit_prologue(RISCVAsmBuffer *buf, const char *func_name, size_t stack_size) {
    char line[256];
    size_t aligned_stack = riscv_align_stack_frame(stack_size);

    snprintf(line, sizeof(line), "\t.globl %s", func_name ? func_name : "main");
    riscv_asm_emit_line(buf, line);
    snprintf(line, sizeof(line), "\t.type %s, @function", func_name ? func_name : "main");
    riscv_asm_emit_line(buf, line);
    snprintf(line, sizeof(line), "%s:", func_name ? func_name : "main");
    riscv_asm_emit_line(buf, line);

    /* Allocate stack frame, save ra (x1) and s0/fp (x8) */
    snprintf(line, sizeof(line), "\taddi sp, sp, -%zu", aligned_stack);
    riscv_asm_emit_line(buf, line);
    snprintf(line, sizeof(line), "\tsd ra, %zu(sp)", aligned_stack - 8);
    riscv_asm_emit_line(buf, line);
    snprintf(line, sizeof(line), "\tsd s0, %zu(sp)", aligned_stack - 16);
    riscv_asm_emit_line(buf, line);
    snprintf(line, sizeof(line), "\taddi s0, sp, %zu", aligned_stack);
    riscv_asm_emit_line(buf, line);
}

void riscv_emit_epilogue(RISCVAsmBuffer *buf, size_t stack_size) {
    char line[256];
    size_t aligned_stack = riscv_align_stack_frame(stack_size);

    snprintf(line, sizeof(line), "\tld ra, %zu(sp)", aligned_stack - 8);
    riscv_asm_emit_line(buf, line);
    snprintf(line, sizeof(line), "\tld s0, %zu(sp)", aligned_stack - 16);
    riscv_asm_emit_line(buf, line);
    snprintf(line, sizeof(line), "\taddi sp, sp, %zu", aligned_stack);
    riscv_asm_emit_line(buf, line);
    riscv_asm_emit_line(buf, "\tret");
}

void riscv_emit_add(RISCVAsmBuffer *buf, RISCVRegister dest, RISCVRegister src1, RISCVRegister src2) {
    char line[128];
    snprintf(line, sizeof(line), "\tadd %s, %s, %s", 
             riscv_get_reg_name(dest), riscv_get_reg_name(src1), riscv_get_reg_name(src2));
    riscv_asm_emit_line(buf, line);
}

void riscv_emit_sub(RISCVAsmBuffer *buf, RISCVRegister dest, RISCVRegister src1, RISCVRegister src2) {
    char line[128];
    snprintf(line, sizeof(line), "\tsub %s, %s, %s", 
             riscv_get_reg_name(dest), riscv_get_reg_name(src1), riscv_get_reg_name(src2));
    riscv_asm_emit_line(buf, line);
}

void riscv_emit_mul(RISCVAsmBuffer *buf, RISCVRegister dest, RISCVRegister src1, RISCVRegister src2) {
    char line[128];
    snprintf(line, sizeof(line), "\tmul %s, %s, %s", 
             riscv_get_reg_name(dest), riscv_get_reg_name(src1), riscv_get_reg_name(src2));
    riscv_asm_emit_line(buf, line);
}

void riscv_emit_div(RISCVAsmBuffer *buf, RISCVRegister dest, RISCVRegister src1, RISCVRegister src2) {
    char line[128];
    snprintf(line, sizeof(line), "\tdiv %s, %s, %s", 
             riscv_get_reg_name(dest), riscv_get_reg_name(src1), riscv_get_reg_name(src2));
    riscv_asm_emit_line(buf, line);
}

void riscv_emit_ld_stack(RISCVAsmBuffer *buf, RISCVRegister dest, int offset) {
    char line[128];
    snprintf(line, sizeof(line), "\tld %s, %d(s0)", riscv_get_reg_name(dest), offset);
    riscv_asm_emit_line(buf, line);
}

void riscv_emit_sd_stack(RISCVAsmBuffer *buf, RISCVRegister src, int offset) {
    char line[128];
    snprintf(line, sizeof(line), "\tsd %s, %d(s0)", riscv_get_reg_name(src), offset);
    riscv_asm_emit_line(buf, line);
}

int zcc_emit_riscv_assembly_to_file(const char *filename, const char *func_name, size_t stack_size) {
    if (!filename) return -1;

    RISCVAsmBuffer buf;
    riscv_asm_init(&buf);

    riscv_asm_emit_line(&buf, "\t.option pic");
    riscv_asm_emit_line(&buf, "\t.text");

    riscv_emit_prologue(&buf, func_name, stack_size);
    
    /* Example calculation body: a0 = a0 + a1 */
    riscv_emit_add(&buf, RISCV_REG_A0, RISCV_REG_A0, RISCV_REG_A1);

    riscv_emit_epilogue(&buf, stack_size);

    FILE *f = fopen(filename, "w");
    if (!f) {
        riscv_asm_free(&buf);
        return -1;
    }
    fputs(buf.data, f);
    fclose(f);

    riscv_asm_free(&buf);
    return 0;
}
