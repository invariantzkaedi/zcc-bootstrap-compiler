#ifndef ZCC_ELF_EMIT_H
#define ZCC_ELF_EMIT_H

#include <stddef.h>

/* C89-safe integer type aliases — no <stdint.h> dependency */
typedef unsigned char      zcc_u8;
typedef unsigned short     zcc_u16;
typedef unsigned int       zcc_u32;
typedef unsigned long long zcc_u64;
typedef signed long long   zcc_i64;

/* ELF64 structure layout definitions */

#define EI_NIDENT 16

typedef struct {
    zcc_u8  e_ident[EI_NIDENT];
    zcc_u16 e_type;
    zcc_u16 e_machine;
    zcc_u32 e_version;
    zcc_u64 e_entry;
    zcc_u64 e_phoff;
    zcc_u64 e_shoff;
    zcc_u32 e_flags;
    zcc_u16 e_ehsize;
    zcc_u16 e_phentsize;
    zcc_u16 e_phnum;
    zcc_u16 e_shentsize;
    zcc_u16 e_shnum;
    zcc_u16 e_shstrndx;
} zcc_elf_ehdr_t;

typedef struct {
    zcc_u32 sh_name;
    zcc_u32 sh_type;
    zcc_u64 sh_flags;
    zcc_u64 sh_addr;
    zcc_u64 sh_offset;
    zcc_u64 sh_size;
    zcc_u32 sh_link;
    zcc_u32 sh_info;
    zcc_u64 sh_addralign;
    zcc_u64 sh_entsize;
} zcc_elf_shdr_t;

typedef struct {
    zcc_u32 st_name;
    zcc_u8  st_info;
    zcc_u8  st_other;
    zcc_u16 st_shndx;
    zcc_u64 st_value;
    zcc_u64 st_size;
} zcc_elf_sym_entry_t;

typedef struct {
    zcc_u64 r_offset;
    zcc_u64 r_info;
    zcc_i64 r_addend;
} zcc_elf_rela_entry_t;

/* Symbol representation collected during codegen */
typedef struct {
    const char *name;
    zcc_u64 value;         /* Offset within .text section */
    zcc_u64 size;          /* Size of symbol (e.g. function size) */
    unsigned char binding; /* STB_LOCAL or STB_GLOBAL */
    unsigned char type;    /* STT_NOTYPE or STT_FUNC */
    zcc_u16 shndx;         /* SHN_UNDEF or section index */
} zcc_elf_sym_t;

/* Relocation representation collected during codegen */
typedef struct {
    zcc_u64 offset;        /* Offset within .text where relocation is applied */
    const char *sym_name;  /* Name of target symbol for linking */
    zcc_u32 type;          /* R_X86_64_64, R_X86_64_PC32, etc. */
    zcc_i64 addend;        /* Relocation addend */
} zcc_elf_rela_t;

/* Magic constants and ELF structure definitions */

#define ELFCLASS64      2
#define ELFDATA2LSB     1
#define EV_CURRENT      1
#define ELFOSABI_NONE   0

#define ET_REL          1
#define EM_X86_64       62

#define SHT_NULL        0
#define SHT_PROGBITS    1
#define SHT_SYMTAB      2
#define SHT_STRTAB      3
#define SHT_RELA        4

#define SHF_WRITE       (1 << 0)
#define SHF_ALLOC       (1 << 1)
#define SHF_EXECINSTR   (1 << 2)
#define SHF_INFO_LINK   (1 << 6)

#define STB_LOCAL       0
#define STB_GLOBAL      1
#define STT_NOTYPE      0
#define STT_OBJECT      1
#define STT_FUNC        2
#define STT_SECTION     3

#define SHN_UNDEF       0
#define SHN_COMMON      0xfff2

#define ELF64_R_INFO(sym, type) (((zcc_u64)(sym) << 32) + (zcc_u32)(type))

#define R_X86_64_NONE   0
#define R_X86_64_64     1
#define R_X86_64_PC32    2
#define R_X86_64_PLT32   4
#define R_X86_64_32S     11

/* Emitter API */
int elf_emit_obj(const char *out_filename,
                 const unsigned char *code_bytes,
                 size_t code_size,
                 const zcc_elf_sym_t *syms,
                 size_t sym_count,
                 const zcc_elf_rela_t *relas,
                 size_t rela_count);

#endif /* ZCC_ELF_EMIT_H */
