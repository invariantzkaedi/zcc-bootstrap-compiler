#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "zcc_elf_parser.h"

static void print_json(const Elf64_Obj *obj) {
    printf("{\n");
    printf("  \"elf_header\": {\n");
    printf("    \"entry\": \"0x%llx\",\n", (unsigned long long)obj->ehdr->e_entry);
    printf("    \"shoff\": %llu,\n", (unsigned long long)obj->ehdr->e_shoff);
    printf("    \"shnum\": %d,\n", obj->ehdr->e_shnum);
    printf("    \"shstrndx\": %d\n", obj->ehdr->e_shstrndx);
    printf("  },\n");

    printf("  \"sections\": [\n");
    for (int i = 0; i < obj->ehdr->e_shnum; i++) {
        Elf64_Shdr *sh = &obj->shdrs[i];
        const char *name = obj->shstrtab ? (obj->shstrtab + sh->sh_name) : "(null)";
        printf("    {\n");
        printf("      \"index\": %d,\n", i);
        printf("      \"name\": \"%s\",\n", name[0] ? name : "");
        printf("      \"type\": \"%s\",\n", get_sh_type_name(sh->sh_type));
        printf("      \"size\": %llu,\n", (unsigned long long)sh->sh_size);
        printf("      \"offset\": %llu,\n", (unsigned long long)sh->sh_offset);
        printf("      \"align\": %llu\n", (unsigned long long)sh->sh_addralign);
        printf("    }%s\n", (i == obj->ehdr->e_shnum - 1) ? "" : ",");
    }
    printf("  ]");

    if (obj->symtab) {
        printf(",\n  \"symbols\": [\n");
        for (int i = 0; i < obj->symcnt; i++) {
            Elf64_Sym *sym = &obj->symtab[i];
            const char *name = obj->strtab ? (obj->strtab + sym->st_name) : "(none)";
            printf("    {\n");
            printf("      \"index\": %d,\n", i);
            printf("      \"value\": \"0x%llx\",\n", (unsigned long long)sym->st_value);
            printf("      \"size\": %llu,\n", (unsigned long long)sym->st_size);
            printf("      \"type\": \"%s\",\n", get_sym_type_name(ELF64_ST_TYPE(sym->st_info)));
            printf("      \"bind\": \"%s\",\n", get_sym_binding_name(ELF64_ST_BIND(sym->st_info)));
            printf("      \"name\": \"%s\"\n", name[0] ? name : "");
            printf("    }%s\n", (i == obj->symcnt - 1) ? "" : ",");
        }
        printf("  ]");
    }

    /* Output Relocations */
    int has_reloc = 0;
    for (int i = 0; i < obj->ehdr->e_shnum; i++) {
        if (obj->shdrs[i].sh_type == SHT_RELA) {
            has_reloc = 1;
            break;
        }
    }

    if (has_reloc) {
        printf(",\n  \"relocations\": [\n");
        int first_rel = 1;
        for (int i = 0; i < obj->ehdr->e_shnum; i++) {
            Elf64_Shdr *sh = &obj->shdrs[i];
            if (sh->sh_type == SHT_RELA) {
                const char *sec_name = obj->shstrtab ? (obj->shstrtab + sh->sh_name) : "(null)";
                Elf64_Rela *relas = (Elf64_Rela *)(obj->data + sh->sh_offset);
                int rela_count = (int)(sh->sh_size / sizeof(Elf64_Rela));

                Elf64_Shdr *target_sh = &obj->shdrs[sh->sh_info];
                const char *target_name = obj->shstrtab ? (obj->shstrtab + target_sh->sh_name) : "(null)";

                Elf64_Shdr *rel_symtab_sh = &obj->shdrs[sh->sh_link];
                Elf64_Sym *rel_syms = (Elf64_Sym *)(obj->data + rel_symtab_sh->sh_offset);
                const char *rel_strtab = (const char *)(obj->data + obj->shdrs[rel_symtab_sh->sh_link].sh_offset);

                for (int r = 0; r < rela_count; r++) {
                    Elf64_Rela *rela = &relas[r];
                    uint32_t sym_idx = (uint32_t)ELF64_R_SYM(rela->r_info);
                    uint32_t rtype = (uint32_t)ELF64_R_TYPE(rela->r_info);

                    Elf64_Sym *sym = &rel_syms[sym_idx];
                    const char *sym_name = rel_strtab + sym->st_name;

                    if (!first_rel) {
                        printf(",\n");
                    }
                    first_rel = 0;

                    printf("    {\n");
                    printf("      \"reloc_section\": \"%s\",\n", sec_name);
                    printf("      \"target_section\": \"%s\",\n", target_name);
                    printf("      \"offset\": \"0x%llx\",\n", (unsigned long long)rela->r_offset);
                    printf("      \"type\": \"%s\",\n", get_reloc_type_name(rtype));
                    printf("      \"symbol_value\": \"0x%llx\",\n", (unsigned long long)sym->st_value);
                    printf("      \"addend\": %lld,\n", (long long)rela->r_addend);
                    printf("      \"symbol_name\": \"%s\"\n", sym_name[0] ? sym_name : "");
                    printf("    }");
                }
            }
        }
        printf("\n  ]");
    }
    printf("\n}\n");
}

static void print_human(const Elf64_Obj *obj) {
    printf("=== ELF Header ===\n");
    printf("  Entry point address: 0x%llx\n", (unsigned long long)obj->ehdr->e_entry);
    printf("  Start of section headers: %llu (bytes into file)\n", (unsigned long long)obj->ehdr->e_shoff);
    printf("  Number of section headers: %d\n", obj->ehdr->e_shnum);
    printf("  Section header string table index: %d\n\n", obj->ehdr->e_shstrndx);

    printf("=== Section Headers ===\n");
    printf("  [Nr] Name                 Type             Size             Offset           Align\n");
    for (int i = 0; i < obj->ehdr->e_shnum; i++) {
        Elf64_Shdr *sh = &obj->shdrs[i];
        const char *name = obj->shstrtab ? (obj->shstrtab + sh->sh_name) : "(null)";
        printf("  [%2d] %-20s %-16s %016llx %016llx %llu\n",
               i, name[0] ? name : "(null)", get_sh_type_name(sh->sh_type),
               (unsigned long long)sh->sh_size, (unsigned long long)sh->sh_offset, (unsigned long long)sh->sh_addralign);
    }
    printf("\n");

    if (obj->symtab) {
        printf("=== Symbol Table ===\n");
        printf("  [Index] Value            Size     Type    Bind   Name\n");
        for (int i = 0; i < obj->symcnt; i++) {
            Elf64_Sym *sym = &obj->symtab[i];
            const char *name = obj->strtab ? (obj->strtab + sym->st_name) : "(none)";
            printf("  [%5d] %016llx %8llu %-7s %-6s %s\n",
                   i, (unsigned long long)sym->st_value, (unsigned long long)sym->st_size,
                   get_sym_type_name(ELF64_ST_TYPE(sym->st_info)),
                   get_sym_binding_name(ELF64_ST_BIND(sym->st_info)),
                   name[0] ? name : "(none)");
        }
        printf("\n");
    }

    for (int i = 0; i < obj->ehdr->e_shnum; i++) {
        Elf64_Shdr *sh = &obj->shdrs[i];
        if (sh->sh_type == SHT_RELA) {
            const char *sec_name = obj->shstrtab ? (obj->shstrtab + sh->sh_name) : "(null)";
            Elf64_Rela *relas = (Elf64_Rela *)(obj->data + sh->sh_offset);
            int rela_count = (int)(sh->sh_size / sizeof(Elf64_Rela));

            Elf64_Shdr *target_sh = &obj->shdrs[sh->sh_info];
            const char *target_name = obj->shstrtab ? (obj->shstrtab + target_sh->sh_name) : "(null)";

            Elf64_Shdr *rel_symtab_sh = &obj->shdrs[sh->sh_link];
            Elf64_Sym *rel_syms = (Elf64_Sym *)(obj->data + rel_symtab_sh->sh_offset);
            const char *rel_strtab = (const char *)(obj->data + obj->shdrs[rel_symtab_sh->sh_link].sh_offset);

            printf("=== Relocation Section '%s' (replaces in '%s') ===\n", sec_name, target_name);
            printf("  Offset           Type             Sym.Value        Addend       Sym.Name\n");
            for (int r = 0; r < rela_count; r++) {
                Elf64_Rela *rela = &relas[r];
                uint32_t sym_idx = (uint32_t)ELF64_R_SYM(rela->r_info);
                uint32_t rtype = (uint32_t)ELF64_R_TYPE(rela->r_info);

                Elf64_Sym *sym = &rel_syms[sym_idx];
                const char *sym_name = rel_strtab + sym->st_name;

                printf("  %016llx %-16s %016llx %+12lld %s\n",
                       (unsigned long long)rela->r_offset, get_reloc_type_name(rtype),
                       (unsigned long long)sym->st_value, (long long)rela->r_addend,
                       sym_name[0] ? sym_name : "(none)");
            }
            printf("\n");
        }
    }
}

int main(int argc, char **argv) {
    int json_mode = 0;
    const char *filepath = NULL;

    if (argc == 2) {
        filepath = argv[1];
    } else if (argc == 3 && strcmp(argv[1], "--json") == 0) {
        json_mode = 1;
        filepath = argv[2];
    } else {
        fprintf(stderr, "Usage: %s [--json] <elf_object.o>\n", argv[0]);
        return 1;
    }

    FILE *file = fopen(filepath, "rb");
    if (!file) {
        fprintf(stderr, "Error: Cannot open file '%s'\n", filepath);
        return 1;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (size < 0) {
        fprintf(stderr, "Error: Invalid file size\n");
        fclose(file);
        return 1;
    }

    uint8_t *buf = (uint8_t *)malloc(size);
    if (!buf) {
        fprintf(stderr, "Error: Out of memory\n");
        fclose(file);
        return 1;
    }

    if (fread(buf, 1, size, file) != (size_t)size) {
        fprintf(stderr, "Error: Read error\n");
        free(buf);
        fclose(file);
        return 1;
    }
    fclose(file);

    Elf64_Obj obj;
    char err_msg[256];
    err_msg[0] = '\0';

    if (elf64_parse(buf, (size_t)size, &obj, err_msg, sizeof(err_msg)) != 0) {
        fprintf(stderr, "Error: %s\n", err_msg[0] ? err_msg : "ELF parsing failed");
        free(buf);
        return 1;
    }

    if (json_mode) {
        print_json(&obj);
    } else {
        print_human(&obj);
    }

    free(buf);
    return 0;
}
