#include "elf_emit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* DbgBuffer helper for DWARF writing */
typedef struct {
    unsigned char *data;
    size_t size;
    size_t cap;
} DbgBuffer;

static void dbg_buf_init(DbgBuffer *b) {
    b->size = 0;
    b->cap = 1024;
    b->data = (unsigned char *)malloc(b->cap);
}

static void dbg_buf_free(DbgBuffer *b) {
    if (b->data) free(b->data);
    b->data = NULL;
    b->size = b->cap = 0;
}

static void dbg_write_u8(DbgBuffer *b, zcc_u8 val) {
    if (b->size + 1 > b->cap) {
        b->cap *= 2;
        b->data = (unsigned char *)realloc(b->data, b->cap);
    }
    b->data[b->size++] = val;
}

static void dbg_write_u16(DbgBuffer *b, zcc_u16 val) {
    dbg_write_u8(b, val & 0xFF);
    dbg_write_u8(b, (val >> 8) & 0xFF);
}

static void dbg_write_u32(DbgBuffer *b, zcc_u32 val) {
    dbg_write_u8(b, val & 0xFF);
    dbg_write_u8(b, (val >> 8) & 0xFF);
    dbg_write_u8(b, (val >> 16) & 0xFF);
    dbg_write_u8(b, (val >> 24) & 0xFF);
}

static void dbg_write_u64(DbgBuffer *b, zcc_u64 val) {
    dbg_write_u32(b, val & 0xFFFFFFFF);
    dbg_write_u32(b, (val >> 32) & 0xFFFFFFFF);
}

static void dbg_write_uleb128(DbgBuffer *b, zcc_u64 val) {
    do {
        zcc_u8 byte = val & 0x7F;
        val >>= 7;
        if (val != 0) {
            byte |= 0x80;
        }
        dbg_write_u8(b, byte);
    } while (val != 0);
}

static void dbg_write_sleb128(DbgBuffer *b, zcc_i64 val) {
    int more = 1;
    do {
        zcc_u8 byte = val & 0x7F;
        val >>= 7;
        if ((val == 0 && (byte & 0x40) == 0) || (val == -1 && (byte & 0x40) != 0)) {
            more = 0;
        } else {
            byte |= 0x80;
        }
        dbg_write_u8(b, byte);
    } while (more);
}

static void dbg_write_str(DbgBuffer *b, const char *str) {
    size_t len = strlen(str);
    size_t i;
    for (i = 0; i <= len; i++) {
        dbg_write_u8(b, str[i]);
    }
}

static size_t add_shstr(DbgBuffer *b, const char *name) {
    size_t off = b->size;
    dbg_write_str(b, name);
    return off;
}

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
static size_t find_sym_in_final(const char *name, const zcc_elf_sym_t *final_syms, size_t final_count, size_t start_idx) {
    size_t i;
    for (i = start_idx; i < final_count; i++) {
        if (final_syms[i].name && name && strcmp(final_syms[i].name, name) == 0) {
            return i;
        }
    }
    return 0;
}

static int compare_locs(const void *a, const void *b) {
    const AssemblerLoc *la = (const AssemblerLoc *)a;
    const AssemblerLoc *lb = (const AssemblerLoc *)b;
    if (la->address < lb->address) return -1;
    if (la->address > lb->address) return 1;
    return 0;
}

typedef struct {
    zcc_u64 offset;
    const char *sym_name;
    zcc_u32 type;
    zcc_i64 addend;
    int is_sec_sym;
    int sec_idx;
} DbgRela;

int elf_emit_obj(const char *out_filename,
                 const unsigned char *code_bytes,
                 size_t code_size,
                 const zcc_elf_sym_t *syms,
                 size_t sym_count,
                 const zcc_elf_rela_t *relas,
                 size_t rela_count) {
    FILE *f = NULL;
    zcc_elf_ehdr_t ehdr;
    zcc_elf_sym_t *final_syms = NULL;
    zcc_elf_sym_entry_t *sym_entries = NULL;
    zcc_elf_rela_entry_t *rela_entries = NULL;
    char *strtab = NULL;
    size_t strtab_size = 0;
    size_t strtab_cap = 1024;

    size_t final_sym_count = 0;
    size_t local_count = 0;
    size_t i;
    size_t curr_off = 0;

    int has_dwarf = (g_asm_loc_count > 0);

    /* Dynamic section indexes assignment */
    int idx_null = 0;
    int idx_text = -1;
    int idx_rela_text = -1;
    int idx_symtab = -1;
    int idx_strtab = -1;
    int idx_shstrtab = -1;
    int idx_debug_line = -1;
    int idx_debug_info = -1;
    int idx_debug_abbrev = -1;
    int idx_debug_str = -1;
    int idx_rela_debug_info = -1;

    int next_idx = 1;
    idx_text = next_idx++;
    if (rela_count > 0) {
        idx_rela_text = next_idx++;
    }
    idx_symtab = next_idx++;
    idx_strtab = next_idx++;
    idx_shstrtab = next_idx++;

    if (has_dwarf) {
        idx_debug_line = next_idx++;
        idx_debug_info = next_idx++;
        idx_debug_abbrev = next_idx++;
        idx_debug_str = next_idx++;
        idx_rela_debug_info = next_idx++;
    }
    int num_sections = next_idx;

    /* Offsets of sections inside the file */
    size_t text_off = 0;
    size_t rela_off = 0;
    size_t symtab_off = 0;
    size_t strtab_off = 0;
    size_t shstrtab_off = 0;
    size_t debug_line_off = 0;
    size_t debug_info_off = 0;
    size_t debug_abbrev_off = 0;
    size_t debug_str_off = 0;
    size_t rela_debug_info_off = 0;
    size_t shdrs_off = 0;

    f = fopen(out_filename, "wb");
    if (!f) {
        return -1;
    }

    /* 1. Build ordered symbol table: Null, section symbols, Locals, Globals */
    int sec_sym_count = 2; /* Null and .text section symbol */
    if (has_dwarf) {
        sec_sym_count += 3; /* .debug_line, .debug_abbrev, .debug_str */
    }
    final_sym_count = sym_count + sec_sym_count;
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
    final_syms[1].shndx = idx_text;

    if (has_dwarf) {
        /* .debug_line section symbol */
        final_syms[2].name = "";
        final_syms[2].value = 0;
        final_syms[2].size = 0;
        final_syms[2].binding = STB_LOCAL;
        final_syms[2].type = STT_SECTION;
        final_syms[2].shndx = idx_debug_line;

        /* .debug_abbrev section symbol */
        final_syms[3].name = "";
        final_syms[3].value = 0;
        final_syms[3].size = 0;
        final_syms[3].binding = STB_LOCAL;
        final_syms[3].type = STT_SECTION;
        final_syms[3].shndx = idx_debug_abbrev;

        /* .debug_str section symbol */
        final_syms[4].name = "";
        final_syms[4].value = 0;
        final_syms[4].size = 0;
        final_syms[4].binding = STB_LOCAL;
        final_syms[4].type = STT_SECTION;
        final_syms[4].shndx = idx_debug_str;
    }

    local_count = sec_sym_count;

    /* Copy local symbols */
    for (i = 0; i < sym_count; i++) {
        if (syms[i].binding == STB_LOCAL) {
            final_syms[local_count++] = syms[i];
        }
    }

    /* Copy global symbols */
    curr_off = local_count;
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

    /* 3. Build relocation entries (.rela.text) */
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
            size_t sym_idx = find_sym_in_final(relas[i].sym_name, final_syms, final_sym_count, sec_sym_count);
            rela_entries[i].r_offset = relas[i].offset;
            rela_entries[i].r_info = ELF64_R_INFO(sym_idx, relas[i].type);
            rela_entries[i].r_addend = relas[i].addend;
        }
    }

    /* 4. Build DWARF sections if loc mappings are available */
    DbgBuffer line_sec_buf;
    DbgBuffer abbrev_buf;
    DbgBuffer info_sec_buf;
    DbgBuffer str_buf;
    DbgRela dbg_relas[2048];
    size_t dbg_rela_count = 0;
    zcc_elf_rela_entry_t *debug_rela_entries = NULL;

    dbg_buf_init(&line_sec_buf);
    dbg_buf_init(&abbrev_buf);
    dbg_buf_init(&info_sec_buf);
    dbg_buf_init(&str_buf);

    if (has_dwarf) {
        /* A. Build .debug_line */
        DbgBuffer program_buf;
        dbg_buf_init(&program_buf);
        
        qsort(g_asm_locs, g_asm_loc_count, sizeof(AssemblerLoc), compare_locs);

        zcc_u64 cur_addr = 0;
        int cur_file = 1;
        int cur_line = 1;

        for (i = 0; i < g_asm_loc_count; i++) {
            AssemblerLoc loc = g_asm_locs[i];
            if (loc.file_idx == 0 || loc.line == 0) continue;

            if (loc.file_idx != cur_file) {
                dbg_write_u8(&program_buf, DW_LNS_set_file);
                dbg_write_uleb128(&program_buf, loc.file_idx);
                cur_file = loc.file_idx;
            }

            zcc_i64 addr_diff = (zcc_i64)loc.address - (zcc_i64)cur_addr;
            zcc_i64 line_diff = (zcc_i64)loc.line - (zcc_i64)cur_line;

            int line_base = -5;
            int line_range = 14;
            int opcode_base = 13;
            zcc_i64 special_opcode = (line_diff - line_base) + (addr_diff * line_range) + opcode_base;

            if (special_opcode >= opcode_base && special_opcode <= 255) {
                dbg_write_u8(&program_buf, (zcc_u8)special_opcode);
            } else {
                if (addr_diff > 0) {
                    dbg_write_u8(&program_buf, DW_LNS_advance_pc);
                    dbg_write_uleb128(&program_buf, addr_diff);
                }
                if (line_diff != 0) {
                    dbg_write_u8(&program_buf, DW_LNS_advance_line);
                    dbg_write_sleb128(&program_buf, line_diff);
                }
                dbg_write_u8(&program_buf, DW_LNS_copy);
            }
            cur_addr = loc.address;
            cur_line = loc.line;
        }

        zcc_i64 final_addr_diff = (zcc_i64)code_size - (zcc_i64)cur_addr;
        if (final_addr_diff > 0) {
            dbg_write_u8(&program_buf, DW_LNS_advance_pc);
            dbg_write_uleb128(&program_buf, final_addr_diff);
        }
        dbg_write_u8(&program_buf, 0); /* Extended opcode */
        dbg_write_uleb128(&program_buf, 1);
        dbg_write_u8(&program_buf, 1); /* DW_LNE_end_sequence */

        DbgBuffer header_buf;
        dbg_buf_init(&header_buf);
        dbg_write_u8(&header_buf, 1); /* minimum_instruction_length */
        dbg_write_u8(&header_buf, 1); /* maximum_operations_per_instruction */
        dbg_write_u8(&header_buf, 1); /* default_is_stmt */
        dbg_write_u8(&header_buf, -5); /* line_base */
        dbg_write_u8(&header_buf, 14); /* line_range */
        dbg_write_u8(&header_buf, 13); /* opcode_base */
        zcc_u8 std_lengths[12] = {0, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 1};
        for (i = 0; i < 12; i++) {
            dbg_write_u8(&header_buf, std_lengths[i]);
        }
        dbg_write_u8(&header_buf, 0); /* include_directories: empty */

        int max_file_idx = 0;
        for (i = 0; i < g_asm_file_count; i++) {
            if (g_asm_files[i].file_idx > max_file_idx) {
                max_file_idx = g_asm_files[i].file_idx;
            }
        }
        for (int fidx = 1; fidx <= max_file_idx; fidx++) {
            const char *fname = "unknown.c";
            for (i = 0; i < g_asm_file_count; i++) {
                if (g_asm_files[i].file_idx == fidx) {
                    fname = g_asm_files[i].path;
                    break;
                }
            }
            dbg_write_str(&header_buf, fname);
            dbg_write_uleb128(&header_buf, 0);
            dbg_write_uleb128(&header_buf, 0);
            dbg_write_uleb128(&header_buf, 0);
        }
        dbg_write_u8(&header_buf, 0); /* terminate files list */

        size_t header_len = header_buf.size;
        dbg_write_u32(&line_sec_buf, (zcc_u32)(2 + 4 + header_len + program_buf.size));
        dbg_write_u16(&line_sec_buf, 4);
        dbg_write_u32(&line_sec_buf, (zcc_u32)header_len);
        for (i = 0; i < header_len; i++) {
            dbg_write_u8(&line_sec_buf, header_buf.data[i]);
        }
        for (i = 0; i < program_buf.size; i++) {
            dbg_write_u8(&line_sec_buf, program_buf.data[i]);
        }
        dbg_buf_free(&header_buf);
        dbg_buf_free(&program_buf);

        /* B. Build .debug_abbrev */
        dbg_write_uleb128(&abbrev_buf, 1); /* Abbrev code 1 (compile_unit) */
        dbg_write_uleb128(&abbrev_buf, DW_TAG_compile_unit);
        dbg_write_u8(&abbrev_buf, DW_CHILDREN_yes);
        dbg_write_uleb128(&abbrev_buf, DW_AT_producer);
        dbg_write_uleb128(&abbrev_buf, DW_FORM_strp);
        dbg_write_uleb128(&abbrev_buf, DW_AT_language);
        dbg_write_uleb128(&abbrev_buf, DW_FORM_data2);
        dbg_write_uleb128(&abbrev_buf, DW_AT_name);
        dbg_write_uleb128(&abbrev_buf, DW_FORM_strp);
        dbg_write_uleb128(&abbrev_buf, DW_AT_low_pc);
        dbg_write_uleb128(&abbrev_buf, DW_FORM_addr);
        dbg_write_uleb128(&abbrev_buf, DW_AT_high_pc);
        dbg_write_uleb128(&abbrev_buf, DW_FORM_addr);
        dbg_write_uleb128(&abbrev_buf, DW_AT_stmt_list);
        dbg_write_uleb128(&abbrev_buf, DW_FORM_sec_offset);
        dbg_write_uleb128(&abbrev_buf, 0);
        dbg_write_uleb128(&abbrev_buf, 0);

        dbg_write_uleb128(&abbrev_buf, 2); /* Abbrev code 2 (subprogram) */
        dbg_write_uleb128(&abbrev_buf, DW_TAG_subprogram);
        dbg_write_u8(&abbrev_buf, DW_CHILDREN_no);
        dbg_write_uleb128(&abbrev_buf, DW_AT_name);
        dbg_write_uleb128(&abbrev_buf, DW_FORM_strp);
        dbg_write_uleb128(&abbrev_buf, DW_AT_decl_file);
        dbg_write_uleb128(&abbrev_buf, DW_FORM_data1);
        dbg_write_uleb128(&abbrev_buf, DW_AT_decl_line);
        dbg_write_uleb128(&abbrev_buf, DW_FORM_data2);
        dbg_write_uleb128(&abbrev_buf, DW_AT_low_pc);
        dbg_write_uleb128(&abbrev_buf, DW_FORM_addr);
        dbg_write_uleb128(&abbrev_buf, DW_AT_high_pc);
        dbg_write_uleb128(&abbrev_buf, DW_FORM_addr);
        dbg_write_uleb128(&abbrev_buf, 0);
        dbg_write_uleb128(&abbrev_buf, 0);
        dbg_write_uleb128(&abbrev_buf, 0); /* terminate */

        /* C. Build .debug_str */
        size_t producer_offset = str_buf.size;
        dbg_write_str(&str_buf, "ZCC C Compiler");
        size_t cu_name_offset = str_buf.size;
        const char *cu_name = (g_asm_file_count > 0) ? g_asm_files[0].path : "main.c";
        dbg_write_str(&str_buf, cu_name);

        /* D. Build .debug_info DIEs & relocations */
        DbgBuffer info_buf;
        dbg_buf_init(&info_buf);
        dbg_write_uleb128(&info_buf, 1); /* Abbrev 1 (CU) */

        /* DW_AT_producer */
        dbg_relas[dbg_rela_count].offset = info_buf.size;
        dbg_relas[dbg_rela_count].sym_name = ".debug_str";
        dbg_relas[dbg_rela_count].type = R_X86_64_32;
        dbg_relas[dbg_rela_count].addend = producer_offset;
        dbg_relas[dbg_rela_count].is_sec_sym = 1;
        dbg_relas[dbg_rela_count].sec_idx = idx_debug_str;
        dbg_rela_count++;
        dbg_write_u32(&info_buf, 0);

        /* DW_AT_language: C99 */
        dbg_write_u16(&info_buf, 0x000c);

        /* DW_AT_name */
        dbg_relas[dbg_rela_count].offset = info_buf.size;
        dbg_relas[dbg_rela_count].sym_name = ".debug_str";
        dbg_relas[dbg_rela_count].type = R_X86_64_32;
        dbg_relas[dbg_rela_count].addend = cu_name_offset;
        dbg_relas[dbg_rela_count].is_sec_sym = 1;
        dbg_relas[dbg_rela_count].sec_idx = idx_debug_str;
        dbg_rela_count++;
        dbg_write_u32(&info_buf, 0);

        /* DW_AT_low_pc */
        dbg_relas[dbg_rela_count].offset = info_buf.size;
        dbg_relas[dbg_rela_count].sym_name = ".text";
        dbg_relas[dbg_rela_count].type = R_X86_64_64;
        dbg_relas[dbg_rela_count].addend = 0;
        dbg_relas[dbg_rela_count].is_sec_sym = 1;
        dbg_relas[dbg_rela_count].sec_idx = idx_text;
        dbg_rela_count++;
        dbg_write_u64(&info_buf, 0);

        /* DW_AT_high_pc */
        dbg_relas[dbg_rela_count].offset = info_buf.size;
        dbg_relas[dbg_rela_count].sym_name = ".text";
        dbg_relas[dbg_rela_count].type = R_X86_64_64;
        dbg_relas[dbg_rela_count].addend = code_size;
        dbg_relas[dbg_rela_count].is_sec_sym = 1;
        dbg_relas[dbg_rela_count].sec_idx = idx_text;
        dbg_rela_count++;
        dbg_write_u64(&info_buf, 0);

        /* DW_AT_stmt_list */
        dbg_relas[dbg_rela_count].offset = info_buf.size;
        dbg_relas[dbg_rela_count].sym_name = ".debug_line";
        dbg_relas[dbg_rela_count].type = R_X86_64_32;
        dbg_relas[dbg_rela_count].addend = 0;
        dbg_relas[dbg_rela_count].is_sec_sym = 1;
        dbg_relas[dbg_rela_count].sec_idx = idx_debug_line;
        dbg_rela_count++;
        dbg_write_u32(&info_buf, 0);

        /* Process subprograms */
        for (i = sec_sym_count; i < final_sym_count; i++) {
            if (final_syms[i].type == STT_FUNC && final_syms[i].shndx == idx_text && final_syms[i].name && final_syms[i].name[0] != '.') {
                size_t func_name_offset = str_buf.size;
                dbg_write_str(&str_buf, final_syms[i].name);

                dbg_write_uleb128(&info_buf, 2); /* Abbrev 2 (subprogram) */

                /* DW_AT_name */
                dbg_relas[dbg_rela_count].offset = info_buf.size;
                dbg_relas[dbg_rela_count].sym_name = ".debug_str";
                dbg_relas[dbg_rela_count].type = R_X86_64_32;
                dbg_relas[dbg_rela_count].addend = func_name_offset;
                dbg_relas[dbg_rela_count].is_sec_sym = 1;
                dbg_relas[dbg_rela_count].sec_idx = idx_debug_str;
                dbg_rela_count++;
                dbg_write_u32(&info_buf, 0);

                /* DW_AT_decl_file */
                dbg_write_u8(&info_buf, 1);

                /* DW_AT_decl_line */
                int decl_line = 1;
                size_t j;
                for (j = 0; j < g_asm_loc_count; j++) {
                    if (g_asm_locs[j].address >= final_syms[i].value && 
                        g_asm_locs[j].address < final_syms[i].value + final_syms[i].size) {
                        decl_line = g_asm_locs[j].line;
                        break;
                    }
                }
                dbg_write_u16(&info_buf, decl_line);

                /* DW_AT_low_pc */
                dbg_relas[dbg_rela_count].offset = info_buf.size;
                dbg_relas[dbg_rela_count].sym_name = final_syms[i].name;
                dbg_relas[dbg_rela_count].type = R_X86_64_64;
                dbg_relas[dbg_rela_count].addend = 0;
                dbg_relas[dbg_rela_count].is_sec_sym = 0;
                dbg_rela_count++;
                dbg_write_u64(&info_buf, 0);

                /* DW_AT_high_pc */
                dbg_relas[dbg_rela_count].offset = info_buf.size;
                dbg_relas[dbg_rela_count].sym_name = final_syms[i].name;
                dbg_relas[dbg_rela_count].type = R_X86_64_64;
                dbg_relas[dbg_rela_count].addend = final_syms[i].size;
                dbg_relas[dbg_rela_count].is_sec_sym = 0;
                dbg_rela_count++;
                dbg_write_u64(&info_buf, 0);
            }
        }
        dbg_write_u8(&info_buf, 0); /* end of CU children */

        /* Assemble final .debug_info section */
        zcc_u32 unit_len = 2 + 4 + 1 + info_buf.size;
        dbg_write_u32(&info_sec_buf, unit_len);
        dbg_write_u16(&info_sec_buf, 4);

        /* Abbrev offset relocation (exactly at offset 6 in the compilation unit header) */
        dbg_relas[dbg_rela_count].offset = 6;
        dbg_relas[dbg_rela_count].sym_name = ".debug_abbrev";
        dbg_relas[dbg_rela_count].type = R_X86_64_32;
        dbg_relas[dbg_rela_count].addend = 0;
        dbg_relas[dbg_rela_count].is_sec_sym = 1;
        dbg_relas[dbg_rela_count].sec_idx = idx_debug_abbrev;
        dbg_rela_count++;
        dbg_write_u32(&info_sec_buf, 0);

        dbg_write_u8(&info_sec_buf, 8); /* address size */

        for (i = 0; i < dbg_rela_count; i++) {
            if (dbg_relas[i].sym_name && strcmp(dbg_relas[i].sym_name, ".debug_abbrev") == 0) {
                /* abbrev offset is at offset 6 in the header (after length and version), no shift needed */
            } else {
                dbg_relas[i].offset += 11;
            }
        }
        for (i = 0; i < info_buf.size; i++) {
            dbg_write_u8(&info_sec_buf, info_buf.data[i]);
        }
        dbg_buf_free(&info_buf);

        /* Construct .rela.debug_info entries */
        if (dbg_rela_count > 0) {
            debug_rela_entries = (zcc_elf_rela_entry_t *)calloc(dbg_rela_count, sizeof(zcc_elf_rela_entry_t));
            for (i = 0; i < dbg_rela_count; i++) {
                size_t sym_idx = 0;
                if (dbg_relas[i].is_sec_sym) {
                    if (dbg_relas[i].sec_idx == idx_text) sym_idx = 1;
                    else if (dbg_relas[i].sec_idx == idx_debug_line) sym_idx = 2;
                    else if (dbg_relas[i].sec_idx == idx_debug_abbrev) sym_idx = 3;
                    else if (dbg_relas[i].sec_idx == idx_debug_str) sym_idx = 4;
                } else {
                    sym_idx = find_sym_in_final(dbg_relas[i].sym_name, final_syms, final_sym_count, sec_sym_count);
                }
                debug_rela_entries[i].r_offset = dbg_relas[i].offset;
                debug_rela_entries[i].r_info = ELF64_R_INFO(sym_idx, dbg_relas[i].type);
                debug_rela_entries[i].r_addend = dbg_relas[i].addend;
            }
        }
    }

    /* 5. Build section name string table (.shstrtab) */
    DbgBuffer shstr_buf;
    dbg_buf_init(&shstr_buf);
    dbg_write_u8(&shstr_buf, 0);

    size_t off_text = add_shstr(&shstr_buf, ".text");
    size_t off_rela_text = (rela_count > 0) ? add_shstr(&shstr_buf, ".rela.text") : 0;
    size_t off_symtab = add_shstr(&shstr_buf, ".symtab");
    size_t off_strtab = add_shstr(&shstr_buf, ".strtab");
    size_t off_shstrtab = add_shstr(&shstr_buf, ".shstrtab");

    size_t off_debug_line = 0;
    size_t off_debug_info = 0;
    size_t off_debug_abbrev = 0;
    size_t off_debug_str = 0;
    size_t off_rela_debug_info = 0;

    if (has_dwarf) {
        off_debug_line = add_shstr(&shstr_buf, ".debug_line");
        off_debug_info = add_shstr(&shstr_buf, ".debug_info");
        off_debug_abbrev = add_shstr(&shstr_buf, ".debug_abbrev");
        off_debug_str = add_shstr(&shstr_buf, ".debug_str");
        off_rela_debug_info = add_shstr(&shstr_buf, ".rela.debug_info");
    }

    /* 6. Set up the ELF Header */
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
    ehdr.e_shnum = num_sections;
    ehdr.e_shstrndx = idx_shstrtab;

    /* Write ELF Header placeholder */
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

    if (has_dwarf) {
        /* .debug_line section */
        curr_off = write_padding(f, curr_off, 1);
        debug_line_off = curr_off;
        fwrite(line_sec_buf.data, 1, line_sec_buf.size, f);
        curr_off += line_sec_buf.size;

        /* .debug_info section */
        curr_off = write_padding(f, curr_off, 1);
        debug_info_off = curr_off;
        fwrite(info_sec_buf.data, 1, info_sec_buf.size, f);
        curr_off += info_sec_buf.size;

        /* .debug_abbrev section */
        curr_off = write_padding(f, curr_off, 1);
        debug_abbrev_off = curr_off;
        fwrite(abbrev_buf.data, 1, abbrev_buf.size, f);
        curr_off += abbrev_buf.size;

        /* .debug_str section */
        curr_off = write_padding(f, curr_off, 1);
        debug_str_off = curr_off;
        fwrite(str_buf.data, 1, str_buf.size, f);
        curr_off += str_buf.size;

        /* .rela.debug_info section */
        if (dbg_rela_count > 0) {
            curr_off = write_padding(f, curr_off, 8);
            rela_debug_info_off = curr_off;
            fwrite(debug_rela_entries, sizeof(zcc_elf_rela_entry_t), dbg_rela_count, f);
            curr_off += sizeof(zcc_elf_rela_entry_t) * dbg_rela_count;
        }
    }

    /* .shstrtab section */
    curr_off = write_padding(f, curr_off, 1);
    shstrtab_off = curr_off;
    fwrite(shstr_buf.data, 1, shstr_buf.size, f);
    curr_off += shstr_buf.size;

    /* Section Headers Table */
    curr_off = write_padding(f, curr_off, 8);
    shdrs_off = curr_off;
    ehdr.e_shoff = shdrs_off;

    /* Rewind and write updated ELF Header */
    fseek(f, 0, SEEK_SET);
    fwrite(&ehdr, sizeof(zcc_elf_ehdr_t), 1, f);
    fseek(f, shdrs_off, SEEK_SET);

    /* Construct section headers */
    zcc_elf_shdr_t *shdrs = (zcc_elf_shdr_t *)calloc(num_sections, sizeof(zcc_elf_shdr_t));

    /* Section 0: NULL */
    memset(&shdrs[0], 0, sizeof(zcc_elf_shdr_t));

    /* Section idx_text: .text */
    shdrs[idx_text].sh_name = (zcc_u32)off_text;
    shdrs[idx_text].sh_type = SHT_PROGBITS;
    shdrs[idx_text].sh_flags = SHF_ALLOC | SHF_EXECINSTR;
    shdrs[idx_text].sh_addr = 0;
    shdrs[idx_text].sh_offset = text_off;
    shdrs[idx_text].sh_size = code_size;
    shdrs[idx_text].sh_link = 0;
    shdrs[idx_text].sh_info = 0;
    shdrs[idx_text].sh_addralign = 16;
    shdrs[idx_text].sh_entsize = 0;

    /* Section idx_rela_text: .rela.text */
    if (rela_count > 0) {
        shdrs[idx_rela_text].sh_name = (zcc_u32)off_rela_text;
        shdrs[idx_rela_text].sh_type = SHT_RELA;
        shdrs[idx_rela_text].sh_flags = SHF_INFO_LINK;
        shdrs[idx_rela_text].sh_addr = 0;
        shdrs[idx_rela_text].sh_offset = rela_off;
        shdrs[idx_rela_text].sh_size = sizeof(zcc_elf_rela_entry_t) * rela_count;
        shdrs[idx_rela_text].sh_link = idx_symtab;
        shdrs[idx_rela_text].sh_info = idx_text;
        shdrs[idx_rela_text].sh_addralign = 8;
        shdrs[idx_rela_text].sh_entsize = sizeof(zcc_elf_rela_entry_t);
    }

    /* Section idx_symtab: .symtab */
    shdrs[idx_symtab].sh_name = (zcc_u32)off_symtab;
    shdrs[idx_symtab].sh_type = SHT_SYMTAB;
    shdrs[idx_symtab].sh_flags = 0;
    shdrs[idx_symtab].sh_addr = 0;
    shdrs[idx_symtab].sh_offset = symtab_off;
    shdrs[idx_symtab].sh_size = sizeof(zcc_elf_sym_entry_t) * final_sym_count;
    shdrs[idx_symtab].sh_link = idx_strtab;
    shdrs[idx_symtab].sh_info = (zcc_u32)local_count;
    shdrs[idx_symtab].sh_addralign = 8;
    shdrs[idx_symtab].sh_entsize = sizeof(zcc_elf_sym_entry_t);

    /* Section idx_strtab: .strtab */
    shdrs[idx_strtab].sh_name = (zcc_u32)off_strtab;
    shdrs[idx_strtab].sh_type = SHT_STRTAB;
    shdrs[idx_strtab].sh_flags = 0;
    shdrs[idx_strtab].sh_addr = 0;
    shdrs[idx_strtab].sh_offset = strtab_off;
    shdrs[idx_strtab].sh_size = strtab_size;
    shdrs[idx_strtab].sh_link = 0;
    shdrs[idx_strtab].sh_info = 0;
    shdrs[idx_strtab].sh_addralign = 1;
    shdrs[idx_strtab].sh_entsize = 0;

    /* Section idx_shstrtab: .shstrtab */
    shdrs[idx_shstrtab].sh_name = (zcc_u32)off_shstrtab;
    shdrs[idx_shstrtab].sh_type = SHT_STRTAB;
    shdrs[idx_shstrtab].sh_flags = 0;
    shdrs[idx_shstrtab].sh_addr = 0;
    shdrs[idx_shstrtab].sh_offset = shstrtab_off;
    shdrs[idx_shstrtab].sh_size = shstr_buf.size;
    shdrs[idx_shstrtab].sh_link = 0;
    shdrs[idx_shstrtab].sh_info = 0;
    shdrs[idx_shstrtab].sh_addralign = 1;
    shdrs[idx_shstrtab].sh_entsize = 0;

    if (has_dwarf) {
        /* Section idx_debug_line: .debug_line */
        shdrs[idx_debug_line].sh_name = (zcc_u32)off_debug_line;
        shdrs[idx_debug_line].sh_type = SHT_PROGBITS;
        shdrs[idx_debug_line].sh_flags = 0;
        shdrs[idx_debug_line].sh_addr = 0;
        shdrs[idx_debug_line].sh_offset = debug_line_off;
        shdrs[idx_debug_line].sh_size = line_sec_buf.size;
        shdrs[idx_debug_line].sh_link = 0;
        shdrs[idx_debug_line].sh_info = 0;
        shdrs[idx_debug_line].sh_addralign = 1;
        shdrs[idx_debug_line].sh_entsize = 0;

        /* Section idx_debug_info: .debug_info */
        shdrs[idx_debug_info].sh_name = (zcc_u32)off_debug_info;
        shdrs[idx_debug_info].sh_type = SHT_PROGBITS;
        shdrs[idx_debug_info].sh_flags = 0;
        shdrs[idx_debug_info].sh_addr = 0;
        shdrs[idx_debug_info].sh_offset = debug_info_off;
        shdrs[idx_debug_info].sh_size = info_sec_buf.size;
        shdrs[idx_debug_info].sh_link = 0;
        shdrs[idx_debug_info].sh_info = 0;
        shdrs[idx_debug_info].sh_addralign = 1;
        shdrs[idx_debug_info].sh_entsize = 0;

        /* Section idx_debug_abbrev: .debug_abbrev */
        shdrs[idx_debug_abbrev].sh_name = (zcc_u32)off_debug_abbrev;
        shdrs[idx_debug_abbrev].sh_type = SHT_PROGBITS;
        shdrs[idx_debug_abbrev].sh_flags = 0;
        shdrs[idx_debug_abbrev].sh_addr = 0;
        shdrs[idx_debug_abbrev].sh_offset = debug_abbrev_off;
        shdrs[idx_debug_abbrev].sh_size = abbrev_buf.size;
        shdrs[idx_debug_abbrev].sh_link = 0;
        shdrs[idx_debug_abbrev].sh_info = 0;
        shdrs[idx_debug_abbrev].sh_addralign = 1;
        shdrs[idx_debug_abbrev].sh_entsize = 0;

        /* Section idx_debug_str: .debug_str */
        shdrs[idx_debug_str].sh_name = (zcc_u32)off_debug_str;
        shdrs[idx_debug_str].sh_type = SHT_PROGBITS;
        shdrs[idx_debug_str].sh_flags = 0;
        shdrs[idx_debug_str].sh_addr = 0;
        shdrs[idx_debug_str].sh_offset = debug_str_off;
        shdrs[idx_debug_str].sh_size = str_buf.size;
        shdrs[idx_debug_str].sh_link = 0;
        shdrs[idx_debug_str].sh_info = 0;
        shdrs[idx_debug_str].sh_addralign = 1;
        shdrs[idx_debug_str].sh_entsize = 0;

        /* Section idx_rela_debug_info: .rela.debug_info */
        if (dbg_rela_count > 0) {
            shdrs[idx_rela_debug_info].sh_name = (zcc_u32)off_rela_debug_info;
            shdrs[idx_rela_debug_info].sh_type = SHT_RELA;
            shdrs[idx_rela_debug_info].sh_flags = SHF_INFO_LINK;
            shdrs[idx_rela_debug_info].sh_addr = 0;
            shdrs[idx_rela_debug_info].sh_offset = rela_debug_info_off;
            shdrs[idx_rela_debug_info].sh_size = sizeof(zcc_elf_rela_entry_t) * dbg_rela_count;
            shdrs[idx_rela_debug_info].sh_link = idx_symtab;
            shdrs[idx_rela_debug_info].sh_info = idx_debug_info;
            shdrs[idx_rela_debug_info].sh_addralign = 8;
            shdrs[idx_rela_debug_info].sh_entsize = sizeof(zcc_elf_rela_entry_t);
        }
    }

    /* Write out all Section Headers */
    fwrite(shdrs, sizeof(zcc_elf_shdr_t), num_sections, f);

    {
        size_t total_payload = 64 + code_size + (rela_count * 24) + (final_sym_count * 24) + strtab_size + shstr_buf.size;
        if (has_dwarf) {
            total_payload += line_sec_buf.size + info_sec_buf.size + abbrev_buf.size + str_buf.size + (dbg_rela_count * 24);
        }
        size_t padding_val = (shdrs_off >= total_payload) ? (shdrs_off - total_payload) : 0;
        extern void zcc_oracle_log_elf(const char *obj_name, size_t text_bytes, 
                                int rela_entries, int symtab_entries, 
                                size_t strtab_bytes, size_t shstrtab_bytes, 
                                size_t padding_bytes);
        zcc_oracle_log_elf(out_filename, code_size, (int)rela_count, (int)final_sym_count, 
                           strtab_size, shstr_buf.size, padding_val);
    }

    /* Clean up resource allocations */
    fclose(f);
    dbg_buf_free(&shstr_buf);
    dbg_buf_free(&line_sec_buf);
    dbg_buf_free(&abbrev_buf);
    dbg_buf_free(&info_sec_buf);
    dbg_buf_free(&str_buf);
    if (debug_rela_entries) free(debug_rela_entries);

    if (rela_entries) {
        free(rela_entries);
    }
    free(sym_entries);
    free(strtab);
    free(final_syms);

    return 0;
}
