#ifndef ZCC_ELF_PARSER_H
#define ZCC_ELF_PARSER_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#ifdef __GNUC__
#define ZCC_UNUSED __attribute__((unused))
#else
#define ZCC_UNUSED
#endif

/* ELF-64 Basic Types */
typedef uint64_t Elf64_Addr;
typedef uint64_t Elf64_Off;
typedef uint16_t Elf64_Half;
typedef uint32_t Elf64_Word;
typedef uint64_t Elf64_Xword;
typedef int64_t  Elf64_Sxword;

#define EI_NIDENT 16

/* ELF-64 Header */
typedef struct {
    unsigned char e_ident[EI_NIDENT];
    Elf64_Half    e_type;
    Elf64_Half    e_machine;
    Elf64_Word    e_version;
    Elf64_Addr    e_entry;
    Elf64_Off     e_phoff;
    Elf64_Off     e_shoff;
    Elf64_Word    e_flags;
    Elf64_Half    e_ehsize;
    Elf64_Half    e_phentsize;
    Elf64_Half    e_phnum;
    Elf64_Half    e_shentsize;
    Elf64_Half    e_shnum;
    Elf64_Half    e_shstrndx;
} Elf64_Ehdr;

/* ELF-64 Section Header */
typedef struct {
    Elf64_Word  sh_name;
    Elf64_Word  sh_type;
    Elf64_Xword sh_flags;
    Elf64_Addr  sh_addr;
    Elf64_Off   sh_offset;
    Elf64_Xword sh_size;
    Elf64_Word  sh_link;
    Elf64_Word  sh_info;
    Elf64_Xword sh_addralign;
    Elf64_Xword sh_entsize;
} Elf64_Shdr;

/* ELF-64 Symbol Table Entry */
typedef struct {
    Elf64_Word    st_name;
    unsigned char st_info;
    unsigned char st_other;
    Elf64_Half    st_shndx;
    Elf64_Addr    st_value;
    Elf64_Xword   st_size;
} Elf64_Sym;

/* ELF-64 Relocation Entry with Addend */
typedef struct {
    Elf64_Addr   r_offset;
    Elf64_Xword  r_info;
    Elf64_Sxword r_addend;
} Elf64_Rela;

/* ELF Magic and Constants */
#define ELFMAG0 0x7f
#define ELFMAG1 'E'
#define ELFMAG2 'L'
#define ELFMAG3 'F'

#define ELFCLASS64  2
#define ELFDATA2LSB 1
#define ET_REL      1
#define EM_X86_64   62

#define SHT_NULL     0
#define SHT_PROGBITS 1
#define SHT_SYMTAB   2
#define SHT_STRTAB   3
#define SHT_RELA     4
#define SHT_NOBITS   8

#define SHN_UNDEF      0
#define SHN_LORESERVE  0xff00
#define SHN_ABS        0xfff1
#define SHN_COMMON     0xfff2

#define ELF64_R_SYM(i)  ((i) >> 32)
#define ELF64_R_TYPE(i) ((i) & 0xffffffffULL)

#define ELF64_ST_BIND(info) ((info) >> 4)
#define ELF64_ST_TYPE(info) ((info) & 0xf)

#define STB_LOCAL  0
#define STB_GLOBAL 1
#define STB_WEAK   2

#define STT_NOTYPE  0
#define STT_OBJECT  1
#define STT_FUNC    2
#define STT_SECTION 3
#define STT_FILE    4

/* Parsed ELF Object State */
typedef struct {
    const uint8_t *data;
    size_t         size;
    Elf64_Ehdr    *ehdr;
    Elf64_Shdr    *shdrs;
    const char    *shstrtab;
    Elf64_Sym     *symtab;
    int            symcnt;
    const char    *strtab;
    int            symtab_idx;
} Elf64_Obj;

static ZCC_UNUSED const char *get_sh_type_name(uint32_t type) {
    switch (type) {
        case SHT_NULL:     return "NULL";
        case SHT_PROGBITS: return "PROGBITS";
        case SHT_SYMTAB:   return "SYMTAB";
        case SHT_STRTAB:   return "STRTAB";
        case SHT_RELA:     return "RELA";
        case SHT_NOBITS:   return "NOBITS";
        default:           return "UNKNOWN";
    }
}

static ZCC_UNUSED const char *get_reloc_type_name(uint32_t type) {
    switch (type) {
        case 0:  return "R_X86_64_NONE";
        case 1:  return "R_X86_64_64";
        case 2:  return "R_X86_64_PC32";
        case 3:  return "R_X86_64_GOT32";
        case 4:  return "R_X86_64_PLT32";
        case 5:  return "R_X86_64_COPY";
        case 6:  return "R_X86_64_GLOB_DAT";
        case 7:  return "R_X86_64_JUMP_SLOT";
        case 8:  return "R_X86_64_RELATIVE";
        case 9:  return "R_X86_64_GOTPCREL";
        case 10: return "R_X86_64_32";
        case 11: return "R_X86_64_32S";
        case 12: return "R_X86_64_16";
        case 13: return "R_X86_64_PC16";
        case 14: return "R_X86_64_8";
        case 15: return "R_X86_64_PC8";
        case 16: return "R_X86_64_DTPMOD64";
        case 17: return "R_X86_64_DTPOFF64";
        case 18: return "R_X86_64_TPOFF64";
        case 19: return "R_X86_64_TLSGD";
        case 20: return "R_X86_64_TLSLD";
        case 21: return "R_X86_64_DTPOFF32";
        case 22: return "R_X86_64_GOTTPOFF";
        case 23: return "R_X86_64_TPOFF32";
        case 24: return "R_X86_64_PC64";
        case 25: return "R_X86_64_GOTOFF64";
        case 26: return "R_X86_64_GOTPC32";
        case 27: return "R_X86_64_PLTOFF64";
        case 30: return "R_X86_64_GOTPLT64";
        case 31: return "R_X86_64_GOTPLT32";
        case 32: return "R_X86_64_SIZE32";
        case 33: return "R_X86_64_SIZE64";
        case 34: return "R_X86_64_GOTPCREL64";
        case 35: return "R_X86_64_TLSDESC_CALL";
        case 36: return "R_X86_64_TLSDESC";
        case 37: return "R_X86_64_IRELATIVE";
        case 38: return "R_X86_64_RELATIVE64";
        case 39: return "R_X86_64_PC32_BND";
        case 40: return "R_X86_64_PLT32_BND";
        case 41: return "R_X86_64_GOTPCRELX";
        case 42: return "R_X86_64_REX_GOTPCRELX";
        default: return "UNKNOWN";
    }
}

static ZCC_UNUSED const char *get_sym_binding_name(unsigned char bind) {
    switch (bind) {
        case STB_LOCAL:  return "LOCAL";
        case STB_GLOBAL: return "GLOBAL";
        case STB_WEAK:   return "WEAK";
        default:         return "UNKNOWN";
    }
}

static ZCC_UNUSED const char *get_sym_type_name(unsigned char type) {
    switch (type) {
        case STT_NOTYPE:  return "NOTYPE";
        case STT_OBJECT:  return "OBJECT";
        case STT_FUNC:    return "FUNC";
        case STT_SECTION: return "SECTION";
        case STT_FILE:    return "FILE";
        default:          return "UNKNOWN";
    }
}

/* Parse ELF buffer, executing all strict boundary checking.
 * Returns 0 on success, or -1 on failure with error message set in err_msg. */
static int elf64_parse(const uint8_t *buf, size_t size, Elf64_Obj *obj, char *err_msg, size_t err_msg_len) {
    if (size < sizeof(Elf64_Ehdr)) {
        snprintf(err_msg, err_msg_len, "File size (%lu) too small for ELF-64 header (%lu)",
                 (unsigned long)size, (unsigned long)sizeof(Elf64_Ehdr));
        return -1;
    }

    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)buf;
    if (ehdr->e_ident[0] != ELFMAG0 || ehdr->e_ident[1] != ELFMAG1 ||
        ehdr->e_ident[2] != ELFMAG2 || ehdr->e_ident[3] != ELFMAG3) {
        snprintf(err_msg, err_msg_len, "Invalid ELF magic number");
        return -1;
    }

    if (ehdr->e_ident[4] != ELFCLASS64) {
        snprintf(err_msg, err_msg_len, "Only 64-bit ELF objects are supported");
        return -1;
    }

    if (ehdr->e_shoff == 0) {
        snprintf(err_msg, err_msg_len, "Section headers offset is 0 (no section table)");
        return -1;
    }

    uint64_t section_table_end = (uint64_t)ehdr->e_shoff + (uint64_t)ehdr->e_shnum * sizeof(Elf64_Shdr);
    if (section_table_end > size) {
        snprintf(err_msg, err_msg_len, "Section headers table end (%llu) out of file bounds (%lu)",
                 (unsigned long long)section_table_end, (unsigned long)size);
        return -1;
    }

    Elf64_Shdr *shdrs = (Elf64_Shdr *)(buf + ehdr->e_shoff);
    const char *shstrtab = NULL;
    if (ehdr->e_shnum > 0) {
        if (ehdr->e_shstrndx >= ehdr->e_shnum) {
            snprintf(err_msg, err_msg_len, "shstrndx (%d) out of range (%d)", ehdr->e_shstrndx, ehdr->e_shnum);
            return -1;
        }
        Elf64_Shdr *shstr_sec = &shdrs[ehdr->e_shstrndx];
        uint64_t shstr_end = (uint64_t)shstr_sec->sh_offset + (uint64_t)shstr_sec->sh_size;
        if (shstr_end > size) {
            snprintf(err_msg, err_msg_len, "shstrtab end (%llu) out of file bounds (%lu)",
                     (unsigned long long)shstr_end, (unsigned long)size);
            return -1;
        }
        shstrtab = (const char *)(buf + shstr_sec->sh_offset);
    }

    /* Validate every section header offset and name index */
    for (int i = 0; i < ehdr->e_shnum; i++) {
        Elf64_Shdr *sh = &shdrs[i];
        if (sh->sh_type != SHT_NOBITS) {
            uint64_t sec_end = (uint64_t)sh->sh_offset + (uint64_t)sh->sh_size;
            if (sec_end > size) {
                snprintf(err_msg, err_msg_len, "Section %d offset end (%llu) out of file bounds (%lu)",
                         i, (unsigned long long)sec_end, (unsigned long)size);
                return -1;
            }
        }
        if (shstrtab) {
            if (sh->sh_name >= shdrs[ehdr->e_shstrndx].sh_size) {
                snprintf(err_msg, err_msg_len, "Section %d name index (%u) exceeds shstrtab size (%llu)",
                         i, sh->sh_name, (unsigned long long)shdrs[ehdr->e_shstrndx].sh_size);
                return -1;
            }
        }
    }

    /* Find and validate the symbol table */
    Elf64_Shdr *symtab_sh = NULL;
    const char *strtab = NULL;
    int symtab_idx = -1;
    for (int i = 0; i < ehdr->e_shnum; i++) {
        if (shdrs[i].sh_type == SHT_SYMTAB) {
            symtab_idx = i;
            symtab_sh = &shdrs[i];
            
            if (symtab_sh->sh_link >= ehdr->e_shnum) {
                snprintf(err_msg, err_msg_len, "Symbol table link (%u) out of range (%d)", symtab_sh->sh_link, ehdr->e_shnum);
                return -1;
            }
            Elf64_Shdr *strtab_sh = &shdrs[symtab_sh->sh_link];
            uint64_t str_end = (uint64_t)strtab_sh->sh_offset + (uint64_t)strtab_sh->sh_size;
            if (str_end > size) {
                snprintf(err_msg, err_msg_len, "Symbol strtab end (%llu) out of file bounds (%lu)",
                         (unsigned long long)str_end, (unsigned long)size);
                return -1;
            }
            strtab = (const char *)(buf + strtab_sh->sh_offset);
            break;
        }
    }

    Elf64_Sym *symtab = NULL;
    int symcnt = 0;
    if (symtab_sh && strtab) {
        if (symtab_sh->sh_size % sizeof(Elf64_Sym) != 0) {
            snprintf(err_msg, err_msg_len, "Symbol table size (%llu) is not a multiple of %lu",
                     (unsigned long long)symtab_sh->sh_size, (unsigned long)sizeof(Elf64_Sym));
            return -1;
        }
        symtab = (Elf64_Sym *)(buf + symtab_sh->sh_offset);
        symcnt = (int)(symtab_sh->sh_size / sizeof(Elf64_Sym));

        /* Validate all symbols */
        for (int i = 0; i < symcnt; i++) {
            Elf64_Sym *sym = &symtab[i];
            if (sym->st_name >= shdrs[symtab_sh->sh_link].sh_size) {
                snprintf(err_msg, err_msg_len, "Symbol %d name index (%u) out of strtab size (%llu)",
                         i, sym->st_name, (unsigned long long)shdrs[symtab_sh->sh_link].sh_size);
                return -1;
            }
            if (sym->st_shndx != SHN_UNDEF && sym->st_shndx < SHN_LORESERVE) {
                if (sym->st_shndx >= ehdr->e_shnum) {
                    snprintf(err_msg, err_msg_len, "Symbol %d section index (%d) out of range (%d)",
                             i, sym->st_shndx, ehdr->e_shnum);
                    return -1;
                }
            }
        }
    }

    /* Validate relocation entries if present */
    for (int i = 0; i < ehdr->e_shnum; i++) {
        Elf64_Shdr *sh = &shdrs[i];
        if (sh->sh_type == SHT_RELA) {
            if (sh->sh_link >= ehdr->e_shnum) {
                snprintf(err_msg, err_msg_len, "Relocation section %d link (%u) out of range (%d)", i, sh->sh_link, ehdr->e_shnum);
                return -1;
            }
            if (sh->sh_info >= ehdr->e_shnum) {
                snprintf(err_msg, err_msg_len, "Relocation section %d info (%u) out of range (%d)", i, sh->sh_info, ehdr->e_shnum);
                return -1;
            }
            if (sh->sh_size % sizeof(Elf64_Rela) != 0) {
                snprintf(err_msg, err_msg_len, "Relocation section %d size (%llu) is not a multiple of %lu",
                         i, (unsigned long long)sh->sh_size, (unsigned long)sizeof(Elf64_Rela));
                return -1;
            }

            Elf64_Shdr *rel_symtab_sh = &shdrs[sh->sh_link];
            if (rel_symtab_sh->sh_type != SHT_SYMTAB) {
                snprintf(err_msg, err_msg_len, "Relocation section %d link points to non-SYMTAB section type (%u)", i, rel_symtab_sh->sh_type);
                return -1;
            }

            int rel_symcnt = (int)(rel_symtab_sh->sh_size / sizeof(Elf64_Sym));
            Elf64_Rela *relas = (Elf64_Rela *)(buf + sh->sh_offset);
            int rela_count = (int)(sh->sh_size / sizeof(Elf64_Rela));

            for (int r = 0; r < rela_count; r++) {
                Elf64_Rela *rela = &relas[r];
                uint32_t sym_idx = (uint32_t)ELF64_R_SYM(rela->r_info);
                if (sym_idx >= (uint32_t)rel_symcnt) {
                    snprintf(err_msg, err_msg_len, "Relocation %d in section %d refers to invalid symbol index %u (max %d)",
                             r, i, sym_idx, rel_symcnt);
                    return -1;
                }
            }
        }
    }

    /* Populate the object fields */
    obj->data = buf;
    obj->size = size;
    obj->ehdr = ehdr;
    obj->shdrs = shdrs;
    obj->shstrtab = shstrtab;
    obj->symtab = symtab;
    obj->symcnt = symcnt;
    obj->strtab = strtab;
    obj->symtab_idx = symtab_idx;

    return 0;
}

#endif /* ZCC_ELF_PARSER_H */
