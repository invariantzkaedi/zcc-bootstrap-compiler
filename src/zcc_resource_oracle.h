#ifndef ZCC_RESOURCE_ORACLE_H
#define ZCC_RESOURCE_ORACLE_H

#include <stddef.h>

/* Resource Tracking Phase Enums */
typedef enum {
    ORACLE_PHASE_PREPROCESS,
    ORACLE_PHASE_PARSER,
    ORACLE_PHASE_IR,
    ORACLE_PHASE_REGALLOC,
    ORACLE_PHASE_ELF,
    ORACLE_PHASE_POSTPROCESS
} OraclePhase;

/* Initializer / Terminations */
void zcc_oracle_init(const char *source_file);
void zcc_oracle_shutdown(void);
void zcc_oracle_log_prediction(const char *filename, int loops, int indirections, int structs);

/* Metric Logging APIs */
void zcc_oracle_log_allocation(void *ptr, size_t size);
void zcc_oracle_log_free(void *ptr);

void zcc_oracle_log_pass(const char *pass_name, const char *function_name, 
                         double duration_ms, size_t heap_bytes, 
                         int spills, int virtual_regs, int physical_regs);

void zcc_oracle_log_elf(const char *obj_name, size_t text_bytes, 
                        int rela_entries, int symtab_entries, 
                        size_t strtab_bytes, size_t shstrtab_bytes, 
                        size_t padding_bytes);

void zcc_oracle_log_sentinel(size_t idt_size, size_t gdt_size, 
                             size_t base_offset, size_t idt_entry_size, 
                             size_t gdt_entry_size, size_t multiboot_offset);

#endif /* ZCC_RESOURCE_ORACLE_H */
