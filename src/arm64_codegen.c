/*
 * ZCC ARM64 AAPCS64 Code Generator
 * Implementation File: src/arm64_codegen.c
 * Target: AArch64 / ARMv8-A (SystemV / AAPCS64 ABI)
 */

#include "arm64_codegen.h"

static const char *reg_names_64[] = {
    "x0",  "x1",  "x2",  "x3",  "x4",  "x5",  "x6",  "x7",
    "x8",  "x9",  "x10", "x11", "x12", "x13", "x14", "x15",
    "x16", "x17", "x18", "x19", "x20", "x21", "x22", "x23",
    "x24", "x25", "x26", "x27", "x28", "x29", "x30", "sp"
};

static const char *reg_names_32[] = {
    "w0",  "w1",  "w2",  "w3",  "w4",  "w5",  "w6",  "w7",
    "w8",  "w9",  "w10", "w11", "w12", "w13", "w14", "w15",
    "w16", "w17", "w18", "w19", "w20", "w21", "w22", "w23",
    "w24", "w25", "w26", "w27", "w28", "w29", "w30", "wsp"
};

void arm64_asm_init(ARM64AsmBuffer *buf) {
    if (!buf) return;
    buf->capacity = 512;
    buf->size = 0;
    buf->data = (char *)malloc(buf->capacity);
    buf->data[0] = '\0';
}

void arm64_asm_free(ARM64AsmBuffer *buf) {
    if (!buf) return;
    if (buf->data) {
        free(buf->data);
        buf->data = NULL;
    }
    buf->size = 0;
    buf->capacity = 0;
}

void arm64_asm_emit_line(ARM64AsmBuffer *buf, const char *line) {
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

const char *arm64_get_reg_name64(ARM64Register reg) {
    if (reg >= ARM64_REG_X0 && reg <= ARM64_REG_SP) {
        return reg_names_64[reg];
    }
    return "x0";
}

const char *arm64_get_reg_name32(ARM64Register reg) {
    if (reg >= ARM64_REG_X0 && reg <= ARM64_REG_SP) {
        return reg_names_32[reg];
    }
    return "w0";
}

size_t arm64_align_stack_frame(size_t raw_frame_size) {
    /* AAPCS64 requires stack pointer (sp) to be 16-byte aligned */
    size_t frame = raw_frame_size + 16; /* 16 bytes for FP/LR pair */
    if (frame % 16 != 0) {
        frame = (frame + 15) & ~((size_t)15);
    }
    return frame;
}

void arm64_emit_prologue(ARM64AsmBuffer *buf, const char *func_name, size_t stack_size) {
    char line[256];
    size_t aligned_stack = arm64_align_stack_frame(stack_size);

    snprintf(line, sizeof(line), "\t.global %s", func_name ? func_name : "main");
    arm64_asm_emit_line(buf, line);
    snprintf(line, sizeof(line), "\t.type %s, %%function", func_name ? func_name : "main");
    arm64_asm_emit_line(buf, line);
    snprintf(line, sizeof(line), "%s:", func_name ? func_name : "main");
    arm64_asm_emit_line(buf, line);

    /* Save Frame Pointer (x29) and Link Register (x30) onto stack, allocate stack space */
    snprintf(line, sizeof(line), "\tstp x29, x30, [sp, #-%zu]!", aligned_stack);
    arm64_asm_emit_line(buf, line);
    arm64_asm_emit_line(buf, "\tmov x29, sp");
}

void arm64_emit_epilogue(ARM64AsmBuffer *buf, size_t stack_size) {
    char line[256];
    size_t aligned_stack = arm64_align_stack_frame(stack_size);

    snprintf(line, sizeof(line), "\tldp x29, x30, [sp], #%zu", aligned_stack);
    arm64_asm_emit_line(buf, line);
    arm64_asm_emit_line(buf, "\tret");
}

void arm64_emit_add(ARM64AsmBuffer *buf, ARM64Register dest, ARM64Register src1, ARM64Register src2) {
    char line[128];
    snprintf(line, sizeof(line), "\tadd %s, %s, %s", 
             arm64_get_reg_name64(dest), arm64_get_reg_name64(src1), arm64_get_reg_name64(src2));
    arm64_asm_emit_line(buf, line);
}

void arm64_emit_sub(ARM64AsmBuffer *buf, ARM64Register dest, ARM64Register src1, ARM64Register src2) {
    char line[128];
    snprintf(line, sizeof(line), "\tsub %s, %s, %s", 
             arm64_get_reg_name64(dest), arm64_get_reg_name64(src1), arm64_get_reg_name64(src2));
    arm64_asm_emit_line(buf, line);
}

void arm64_emit_mul(ARM64AsmBuffer *buf, ARM64Register dest, ARM64Register src1, ARM64Register src2) {
    char line[128];
    snprintf(line, sizeof(line), "\tmul %s, %s, %s", 
             arm64_get_reg_name64(dest), arm64_get_reg_name64(src1), arm64_get_reg_name64(src2));
    arm64_asm_emit_line(buf, line);
}

void arm64_emit_sdiv(ARM64AsmBuffer *buf, ARM64Register dest, ARM64Register src1, ARM64Register src2) {
    char line[128];
    snprintf(line, sizeof(line), "\tsdiv %s, %s, %s", 
             arm64_get_reg_name64(dest), arm64_get_reg_name64(src1), arm64_get_reg_name64(src2));
    arm64_asm_emit_line(buf, line);
}

void arm64_emit_ldr_stack(ARM64AsmBuffer *buf, ARM64Register dest, int offset) {
    char line[128];
    snprintf(line, sizeof(line), "\tldr %s, [x29, #%d]", arm64_get_reg_name64(dest), offset);
    arm64_asm_emit_line(buf, line);
}

void arm64_emit_str_stack(ARM64AsmBuffer *buf, ARM64Register src, int offset) {
    char line[128];
    snprintf(line, sizeof(line), "\tstr %s, [x29, #%d]", arm64_get_reg_name64(src), offset);
    arm64_asm_emit_line(buf, line);
}

int zcc_emit_arm64_assembly_to_file(const char *filename, const char *func_name, size_t stack_size) {
    if (!filename) return -1;

    ARM64AsmBuffer buf;
    arm64_asm_init(&buf);

    arm64_asm_emit_line(&buf, "\t.arch armv8-a");
    arm64_asm_emit_line(&buf, "\t.text");

    arm64_emit_prologue(&buf, func_name, stack_size);
    
    /* Example calculation body: x0 = x0 + x1 */
    arm64_emit_add(&buf, ARM64_REG_X0, ARM64_REG_X0, ARM64_REG_X1);

    arm64_emit_epilogue(&buf, stack_size);

    FILE *f = fopen(filename, "w");
    if (!f) {
        arm64_asm_free(&buf);
        return -1;
    }
    fputs(buf.data, f);
    fclose(f);

    arm64_asm_free(&buf);
    return 0;
}
