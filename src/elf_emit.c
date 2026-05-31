#include "elf_emit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Padding helper */
static size_t write_padding(FILE *f, size_t current_offset, size_t alignment) {
    size_t aligned_offset = (current_offset + alignment - 1) & ~(alignment - 1);
    size_t pad_bytes = aligned_offset - current_offset;
    if (pad_bytes > 0) {
        unsigned char pad[16];
        size_t i;
        for (i = 0; i < 16; i++) {
            pad[i] = 0;
        }
        fwrite(pad, 1, pad_bytes, f);
    }
    return aligned_offset;
}

/* Linear search for symbol in compiled table */
static size_t find_sym_in_final(const char *name, const zcc_elf_sym_t *final_syms, size_t final_count) {
    size_t i;
    for (i = 2; i < final_count; i++) {
        if (final_syms[i].name && name && strcmp(final_syms[i].name, name) == 0) {
            return i;
        }
    }
    return 0;
}

int elf_emit_obj(const char *out_filename,
                 const unsigned char *code_bytes,
                 size_t code_size,
                 const zcc_elf_sym_t *syms,
                 size_t sym_count,
                 const zcc_elf_rela_t *relas,
                 size_t rela_count) {
    FILE *f = NULL;
    zcc_elf_ehdr_t ehdr;
    zcc_elf_shdr_t shdrs[6];
    zcc_elf_sym_t *final_syms = NULL;
    zcc_elf_sym_entry_t *sym_entries = NULL;
    zcc_elf_rela_entry_t *rela_entries = NULL;
    char *strtab = NULL;
    size_t strtab_size = 0;
    size_t strtab_cap = 1024;
    char *shstrtab = NULL;
    size_t shstrtab_size = 0;

    size_t final_sym_count = 0;
    size_t local_count = 0;
    size_t i;
    size_t curr_off = 0;

    /* Offsets of sections inside the file */
    size_t text_off = 0;
    size_t rela_off = 0;
    size_t symtab_off = 0;
    size_t strtab_off = 0;
    size_t shstrtab_off = 0;
    size_t shdrs_off = 0;

    f = fopen(out_filename, "wb");
    if (!f) {
        return -1;
    }

    /* 1. Build ordered symbol table: Null, .text section sym, Locals, Globals */
    final_sym_count = sym_count + 2;
    final_syms = (zcc_elf_sym_t *)malloc(sizeof(zcc_elf_sym_t) * final_sym_count);
    if (!final_syms) {
        fclose(f);
        return -2;
    }

    /* Null symbol */
    final_syms[0].name = "";
    final_syms[0].value = 0;
    final_syms[0].size = 0;
    final_syms[0].binding = STB_LOCAL;
    final_syms[0].type = STT_NOTYPE;
    final_syms[0].shndx = SHN_UNDEF;

    /* .text section symbol */
    final_syms[1].name = "";
    final_syms[1].value = 0;
    final_syms[1].size = 0;
    final_syms[1].binding = STB_LOCAL;
    final_syms[1].type = STT_SECTION;
    final_syms[1].shndx = 1;

    local_count = 2; /* Null and section sym are both STB_LOCAL */

    /* Copy local symbols */
    for (i = 0; i < sym_count; i++) {
        if (syms[i].binding == STB_LOCAL) {
            final_syms[local_count++] = syms[i];
        }
    }

    /* Copy global symbols */
    curr_off = local_count; /* start of globals in final_syms */
    for (i = 0; i < sym_count; i++) {
        if (syms[i].binding != STB_LOCAL) {
            final_syms[curr_off++] = syms[i];
        }
    }

    /* 2. Build string table (.strtab) */
    strtab = (char *)malloc(strtab_cap);
    if (!strtab) {
        free(final_syms);
        fclose(f);
        return -3;
    }
    strtab[0] = '\0';
    strtab_size = 1;

    /* Pack symbols into Elf64_Sym entry array and populate strtab */
    sym_entries = (zcc_elf_sym_entry_t *)malloc(sizeof(zcc_elf_sym_entry_t) * final_sym_count);
    if (!sym_entries) {
        free(strtab);
        free(final_syms);
        fclose(f);
        return -4;
    }

    for (i = 0; i < final_sym_count; i++) {
        sym_entries[i].st_name = 0;
        sym_entries[i].st_info = (final_syms[i].binding << 4) | (final_syms[i].type & 0xF);
        sym_entries[i].st_other = 0;
        sym_entries[i].st_shndx = final_syms[i].shndx;
        sym_entries[i].st_value = final_syms[i].value;
        sym_entries[i].st_size = final_syms[i].size;

        if (final_syms[i].name && final_syms[i].name[0] != '\0') {
            size_t name_len = strlen(final_syms[i].name);
            while (strtab_size + name_len + 1 > strtab_cap) {
                strtab_cap *= 2;
                strtab = (char *)realloc(strtab, strtab_cap);
            }
            sym_entries[i].st_name = (zcc_u32)strtab_size;
            strcpy(strtab + strtab_size, final_syms[i].name);
            strtab_size += name_len + 1;
        }
    }

    /* 3. Build relocation entries (.rela.text) with linear name resolution */
    if (rela_count > 0) {
        rela_entries = (zcc_elf_rela_entry_t *)malloc(sizeof(zcc_elf_rela_entry_t) * rela_count);
        if (!rela_entries) {
            free(sym_entries);
            free(strtab);
            free(final_syms);
            fclose(f);
            return -5;
        }

        for (i = 0; i < rela_count; i++) {
            size_t sym_idx = find_sym_in_final(relas[i].sym_name, final_syms, final_sym_count);
            rela_entries[i].r_offset = relas[i].offset;
            rela_entries[i].r_info = ELF64_R_INFO(sym_idx, relas[i].type);
            rela_entries[i].r_addend = relas[i].addend;
        }
    }

    /* 4. Build section name string table (.shstrtab) */
    shstrtab_size = 44;
    shstrtab = (char *)malloc(shstrtab_size);
    if (!shstrtab) {
        if (rela_entries) free(rela_entries);
        free(sym_entries);
        free(strtab);
        free(final_syms);
        fclose(f);
        return -6;
    }
    /* Set exactly: "", ".text", ".rela.text", ".symtab", ".strtab", ".shstrtab" */
    shstrtab[0] = '\0';
    strcpy(shstrtab + 1, ".text");
    strcpy(shstrtab + 7, ".rela.text");
    strcpy(shstrtab + 18, ".symtab");
    strcpy(shstrtab + 26, ".strtab");
    strcpy(shstrtab + 34, ".shstrtab");

    /* 5. Set up the ELF Header */
    memset(&ehdr, 0, sizeof(ehdr));
    ehdr.e_ident[0] = 0x7f;
    ehdr.e_ident[1] = 'E';
    ehdr.e_ident[2] = 'L';
    ehdr.e_ident[3] = 'F';
    ehdr.e_ident[4] = ELFCLASS64;
    ehdr.e_ident[5] = ELFDATA2LSB;
    ehdr.e_ident[6] = EV_CURRENT;
    ehdr.e_ident[7] = ELFOSABI_NONE;

    ehdr.e_type = ET_REL;
    ehdr.e_machine = EM_X86_64;
    ehdr.e_version = EV_CURRENT;
    ehdr.e_ehsize = sizeof(zcc_elf_ehdr_t);
    ehdr.e_shentsize = sizeof(zcc_elf_shdr_t);
    ehdr.e_shnum = 6;
    ehdr.e_shstrndx = 5;

    /* 6. Layout calculation and streaming to file */
    fwrite(&ehdr, sizeof(zcc_elf_ehdr_t), 1, f);
    curr_off = sizeof(zcc_elf_ehdr_t);


    /* .text section */
    curr_off = write_padding(f, curr_off, 16);
    text_off = curr_off;
    fwrite(code_bytes, 1, code_size, f);
    curr_off += code_size;

    /* .rela.text section */
    if (rela_count > 0) {
        curr_off = write_padding(f, curr_off, 8);
        rela_off = curr_off;
        fwrite(rela_entries, sizeof(zcc_elf_rela_entry_t), rela_count, f);
        curr_off += sizeof(zcc_elf_rela_entry_t) * rela_count;
    }

    /* .symtab section */
    curr_off = write_padding(f, curr_off, 8);
    symtab_off = curr_off;
    fwrite(sym_entries, sizeof(zcc_elf_sym_entry_t), final_sym_count, f);
    curr_off += sizeof(zcc_elf_sym_entry_t) * final_sym_count;

    /* .strtab section */
    curr_off = write_padding(f, curr_off, 1);
    strtab_off = curr_off;
    fwrite(strtab, 1, strtab_size, f);
    curr_off += strtab_size;

    /* .shstrtab section */
    curr_off = write_padding(f, curr_off, 1);
    shstrtab_off = curr_off;
    fwrite(shstrtab, 1, shstrtab_size, f);
    curr_off += shstrtab_size;

    /* Section Headers Table */
    curr_off = write_padding(f, curr_off, 8);
    shdrs_off = curr_off;
    ehdr.e_shoff = shdrs_off;

    /* Rewind and write updated ELF Header */
    fseek(f, 0, SEEK_SET);
    fwrite(&ehdr, sizeof(zcc_elf_ehdr_t), 1, f);
    fseek(f, shdrs_off, SEEK_SET);

    /* Construct section headers */
    /* Section 0: NULL */
    memset(&shdrs[0], 0, sizeof(zcc_elf_shdr_t));

    /* Section 1: .text */
    shdrs[1].sh_name = 1;
    shdrs[1].sh_type = SHT_PROGBITS;
    shdrs[1].sh_flags = SHF_ALLOC | SHF_EXECINSTR;
    shdrs[1].sh_addr = 0;
    shdrs[1].sh_offset = text_off;
    shdrs[1].sh_size = code_size;
    shdrs[1].sh_link = 0;
    shdrs[1].sh_info = 0;
    shdrs[1].sh_addralign = 16;
    shdrs[1].sh_entsize = 0;

    /* Section 2: .rela.text */
    if (rela_count > 0) {
        shdrs[2].sh_name = 7;
        shdrs[2].sh_type = SHT_RELA;
        shdrs[2].sh_flags = SHF_INFO_LINK;
        shdrs[2].sh_addr = 0;
        shdrs[2].sh_offset = rela_off;
        shdrs[2].sh_size = sizeof(zcc_elf_rela_entry_t) * rela_count;
        shdrs[2].sh_link = 3; /* points to .symtab */
        shdrs[2].sh_info = 1; /* applies to .text */
        shdrs[2].sh_addralign = 8;
        shdrs[2].sh_entsize = sizeof(zcc_elf_rela_entry_t);
    } else {
        /* Empty placeholder section header for rela if none present */
        memset(&shdrs[2], 0, sizeof(zcc_elf_shdr_t));
    }

    /* Section 3: .symtab */
    shdrs[3].sh_name = 18;
    shdrs[3].sh_type = SHT_SYMTAB;
    shdrs[3].sh_flags = 0;
    shdrs[3].sh_addr = 0;
    shdrs[3].sh_offset = symtab_off;
    shdrs[3].sh_size = sizeof(zcc_elf_sym_entry_t) * final_sym_count;
    shdrs[3].sh_link = 4; /* points to .strtab */
    shdrs[3].sh_info = (zcc_u32)local_count; /* index of first global symbol */
    shdrs[3].sh_addralign = 8;
    shdrs[3].sh_entsize = sizeof(zcc_elf_sym_entry_t);

    /* Section 4: .strtab */
    shdrs[4].sh_name = 26;
    shdrs[4].sh_type = SHT_STRTAB;
    shdrs[4].sh_flags = 0;
    shdrs[4].sh_addr = 0;
    shdrs[4].sh_offset = strtab_off;
    shdrs[4].sh_size = strtab_size;
    shdrs[4].sh_link = 0;
    shdrs[4].sh_info = 0;
    shdrs[4].sh_addralign = 1;
    shdrs[4].sh_entsize = 0;

    /* Section 5: .shstrtab */
    shdrs[5].sh_name = 34;
    shdrs[5].sh_type = SHT_STRTAB;
    shdrs[5].sh_flags = 0;
    shdrs[5].sh_addr = 0;
    shdrs[5].sh_offset = shstrtab_off;
    shdrs[5].sh_size = shstrtab_size;
    shdrs[5].sh_link = 0;
    shdrs[5].sh_info = 0;
    shdrs[5].sh_addralign = 1;
    shdrs[5].sh_entsize = 0;

    /* Write out all Section Headers */
    fwrite(shdrs, sizeof(zcc_elf_shdr_t), 6, f);

    {
        size_t total_payload = 64 + code_size + (rela_count * 24) + (final_sym_count * 24) + strtab_size + shstrtab_size;
        size_t padding_val = (shdrs_off >= total_payload) ? (shdrs_off - total_payload) : 0;
        extern void zcc_oracle_log_elf(const char *obj_name, size_t text_bytes, 
                                int rela_entries, int symtab_entries, 
                                size_t strtab_bytes, size_t shstrtab_bytes, 
                                size_t padding_bytes);
        zcc_oracle_log_elf(out_filename, code_size, (int)rela_count, (int)final_sym_count, 
                           strtab_size, shstrtab_size, padding_val);
    }

    /* 7. Clean up resource allocations */
    fclose(f);
    free(shstrtab);
    if (rela_entries) {
        free(rela_entries);
    }
    free(sym_entries);
    free(strtab);
    free(final_syms);

    return 0;
}
