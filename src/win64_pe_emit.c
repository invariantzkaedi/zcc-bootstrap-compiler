/*
 * ZCC Win64 PE/COFF Direct Binary Emitter
 * Implementation File: src/win64_pe_emit.c
 * Target: Windows 64-bit Executable (PE32+ / AMD64 COFF)
 */

#include "win64_pe_emit.h"

uint32_t win64_pe_align_to(uint32_t val, uint32_t align) {
    if (align == 0) return val;
    uint32_t rem = val % align;
    if (rem == 0) return val;
    return val + (align - rem);
}

int zcc_emit_win64_pe_file_ex(const char *filename, const uint8_t *code_bytes, size_t code_len, uint32_t entry_offset) {
    if (!filename) return -1;

    FILE *f = fopen(filename, "wb");
    if (!f) return -1;

    /* 1. Construct DOS Header (64 bytes) */
    IMAGE_DOS_HEADER dos_hdr;
    memset(&dos_hdr, 0, sizeof(dos_hdr));
    dos_hdr.e_magic = 0x5A4D; /* 'MZ' */
    dos_hdr.e_cblp = 0x0090;
    dos_hdr.e_cp = 0x0003;
    dos_hdr.e_cparhdr = 0x0004;
    dos_hdr.e_maxalloc = 0xFFFF;
    dos_hdr.e_sp = 0x00B8;
    dos_hdr.e_lfarlc = 0x0040;
    dos_hdr.e_lfanew = 0x0080; /* Offset to PE Signature */

    /* 2. Construct DOS Stub Message (64 bytes starting at offset 0x40) */
    uint8_t dos_stub[64] = {
        0x0E, 0x1F, 0xBA, 0x0E, 0x00, 0xB4, 0x09, 0xCD,
        0x21, 0xB8, 0x01, 0x4C, 0xCD, 0x21, 'T',  'h',
        'i',  's',  ' ',  'p',  'r',  'o',  'g',  'r',
        'a',  'm',  ' ',  'c',  'a',  'n',  'n',  'o',
        't',  ' ',  'b',  'e',  ' ',  'r',  'u',  'n',
        ' ',  'i',  'n',  ' ',  'D',  'O',  'S',  ' ',
        'm',  'o',  'd',  'e',  '.',  '\r', '\r', '\n',
        '$',  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };

    /* 3. PE Signature (4 bytes) */
    uint32_t pe_sig = 0x00004550; /* 'PE\0\0' */

    /* 4. COFF File Header (20 bytes) */
    IMAGE_FILE_HEADER file_hdr;
    memset(&file_hdr, 0, sizeof(file_hdr));
    file_hdr.Machine = 0x8664; /* AMD64 / x86-64 */
    file_hdr.NumberOfSections = 2; /* .text and .data */
    file_hdr.TimeDateStamp = 0x66900000;
    file_hdr.SizeOfOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER64);
    file_hdr.Characteristics = 0x0022; /* EXECUTABLE_IMAGE | LARGE_ADDRESS_AWARE */

    /* 5. PE32+ Optional Header (240 bytes) */
    IMAGE_OPTIONAL_HEADER64 opt_hdr;
    memset(&opt_hdr, 0, sizeof(opt_hdr));
    opt_hdr.Magic = 0x020B; /* PE32+ */
    opt_hdr.MajorLinkerVersion = 14;
    opt_hdr.MinorLinkerVersion = 0;
    
    uint32_t raw_code_len = (code_bytes && code_len > 0) ? (uint32_t)code_len : 16;
    uint32_t aligned_code_size = win64_pe_align_to(raw_code_len, 0x0200);

    opt_hdr.SizeOfCode = aligned_code_size;
    opt_hdr.SizeOfInitializedData = 0x0200;
    opt_hdr.AddressOfEntryPoint = 0x1000 + entry_offset; /* RVA of entry point */
    opt_hdr.BaseOfCode = 0x1000;
    opt_hdr.ImageBase = 0x140000000ULL;
    opt_hdr.SectionAlignment = 0x1000; /* 4KB memory page alignment */
    opt_hdr.FileAlignment = 0x0200;    /* 512-byte file alignment */
    opt_hdr.MajorOperatingSystemVersion = 6;
    opt_hdr.MinorOperatingSystemVersion = 0;
    opt_hdr.MajorSubsystemVersion = 6;
    opt_hdr.MinorSubsystemVersion = 0;
    
    uint32_t headers_size = sizeof(IMAGE_DOS_HEADER) + sizeof(dos_stub) + sizeof(pe_sig) +
                            sizeof(IMAGE_FILE_HEADER) + sizeof(IMAGE_OPTIONAL_HEADER64) +
                            (2 * sizeof(IMAGE_SECTION_HEADER));
    uint32_t aligned_headers_size = win64_pe_align_to(headers_size, 0x0200);

    opt_hdr.SizeOfHeaders = aligned_headers_size;
    opt_hdr.SizeOfImage = 0x1000 + win64_pe_align_to(raw_code_len, 0x1000) + 0x1000;
    opt_hdr.Subsystem = 3; /* IMAGE_SUBSYSTEM_WINDOWS_CUI (Console) */
    opt_hdr.DllCharacteristics = 0x8160;
    opt_hdr.SizeOfStackReserve = 0x100000;
    opt_hdr.SizeOfStackCommit = 0x1000;
    opt_hdr.SizeOfHeapReserve = 0x100000;
    opt_hdr.SizeOfHeapCommit = 0x1000;
    opt_hdr.NumberOfRvaAndSizes = 16;

    /* 6. Section Headers (40 bytes each) */
    IMAGE_SECTION_HEADER sec_text;
    memset(&sec_text, 0, sizeof(sec_text));
    memcpy(sec_text.Name, ".text\0\0\0", 8);
    sec_text.VirtualSize = raw_code_len;
    sec_text.VirtualAddress = 0x1000;
    sec_text.SizeOfRawData = aligned_code_size;
    sec_text.PointerToRawData = aligned_headers_size;
    sec_text.Characteristics = 0xE0000020; /* CODE | EXECUTE | READ | WRITE */

    IMAGE_SECTION_HEADER sec_data;
    memset(&sec_data, 0, sizeof(sec_data));
    memcpy(sec_data.Name, ".data\0\0\0", 8);
    sec_data.VirtualSize = 16;
    sec_data.VirtualAddress = 0x1000 + win64_pe_align_to(raw_code_len, 0x1000);
    sec_data.SizeOfRawData = 0x0200;
    sec_data.PointerToRawData = aligned_headers_size + aligned_code_size;
    sec_data.Characteristics = 0xC0000040; /* INITIALIZED_DATA | READ | WRITE */

    /* Write Headers to File */
    fwrite(&dos_hdr, sizeof(dos_hdr), 1, f);
    fwrite(dos_stub, sizeof(dos_stub), 1, f);
    fwrite(&pe_sig, sizeof(pe_sig), 1, f);
    fwrite(&file_hdr, sizeof(file_hdr), 1, f);
    fwrite(&opt_hdr, sizeof(opt_hdr), 1, f);
    fwrite(&sec_text, sizeof(sec_text), 1, f);
    fwrite(&sec_data, sizeof(sec_data), 1, f);

    /* Pad headers out to FileAlignment (aligned_headers_size) */
    size_t written_headers = sizeof(dos_hdr) + sizeof(dos_stub) + sizeof(pe_sig) +
                             sizeof(file_hdr) + sizeof(opt_hdr) + (2 * sizeof(IMAGE_SECTION_HEADER));
    if (aligned_headers_size > written_headers) {
        size_t pad_len = aligned_headers_size - written_headers;
        uint8_t *pad = (uint8_t *)calloc(1, pad_len);
        fwrite(pad, 1, pad_len, f);
        free(pad);
    }

    /* Write .text code payload */
    if (code_bytes && code_len > 0) {
        fwrite(code_bytes, 1, code_len, f);
        if (aligned_code_size > code_len) {
            size_t pad_code = aligned_code_size - code_len;
            uint8_t *pad = (uint8_t *)calloc(1, pad_code);
            fwrite(pad, 1, pad_code, f);
            free(pad);
        }
    } else {
        /* Default dummy x86-64 return payload: mov eax, 42; ret */
        uint8_t dummy_code[7] = { 0xB8, 0x2A, 0x00, 0x00, 0x00, 0xC3, 0x90 };
        fwrite(dummy_code, 1, sizeof(dummy_code), f);
        size_t pad_code = aligned_code_size - sizeof(dummy_code);
        uint8_t *pad = (uint8_t *)calloc(1, pad_code);
        fwrite(pad, 1, pad_code, f);
        free(pad);
    }

    /* Write dummy .data section (512 bytes aligned) */
    uint8_t *data_pad = (uint8_t *)calloc(1, 0x0200);
    fwrite(data_pad, 1, 0x0200, f);
    free(data_pad);

    fclose(f);
    return 0;
}

int zcc_emit_win64_pe_file(const char *filename, const uint8_t *code_bytes, size_t code_len) {
    return zcc_emit_win64_pe_file_ex(filename, code_bytes, code_len, 0);
}

const char *zcc_win64_pe_resolve_dll_for_symbol(const char *sym) {
    if (!sym) return "KERNEL32.dll";
    /* USER32 GUI functions */
    if (strncmp(sym, "MessageBox", 10) == 0 ||
        strncmp(sym, "GetMessage", 10) == 0 ||
        strncmp(sym, "PeekMessage", 11) == 0 ||
        strncmp(sym, "TranslateMessage", 16) == 0 ||
        strncmp(sym, "DispatchMessage", 15) == 0 ||
        strncmp(sym, "PostQuitMessage", 15) == 0 ||
        strncmp(sym, "PostMessage", 11) == 0 ||
        strncmp(sym, "SendMessage", 11) == 0 ||
        strncmp(sym, "CreateWindow", 12) == 0 ||
        strncmp(sym, "DefWindowProc", 13) == 0 ||
        strncmp(sym, "DestroyWindow", 13) == 0 ||
        strncmp(sym, "ShowWindow", 10) == 0 ||
        strncmp(sym, "UpdateWindow", 12) == 0 ||
        strncmp(sym, "GetDesktopWindow", 16) == 0 ||
        strncmp(sym, "GetSystemMetrics", 16) == 0 ||
        strncmp(sym, "MessageBeep", 11) == 0) {
        return "USER32.dll";
    }
    /* GDI32 functions */
    if (strncmp(sym, "CreateSolidBrush", 16) == 0 ||
        strncmp(sym, "SelectObject", 12) == 0 ||
        strncmp(sym, "DeleteObject", 12) == 0 ||
        strncmp(sym, "TextOut", 7) == 0) {
        return "GDI32.dll";
    }
    /* MSVCRT C runtime standard library functions */
    if (strcmp(sym, "printf") == 0 ||
        strcmp(sym, "fprintf") == 0 ||
        strcmp(sym, "sprintf") == 0 ||
        strcmp(sym, "snprintf") == 0 ||
        strcmp(sym, "vprintf") == 0 ||
        strcmp(sym, "vfprintf") == 0 ||
        strcmp(sym, "vsprintf") == 0 ||
        strcmp(sym, "vsnprintf") == 0 ||
        strcmp(sym, "puts") == 0 ||
        strcmp(sym, "putchar") == 0 ||
        strcmp(sym, "getchar") == 0 ||
        strcmp(sym, "malloc") == 0 ||
        strcmp(sym, "calloc") == 0 ||
        strcmp(sym, "realloc") == 0 ||
        strcmp(sym, "free") == 0 ||
        strcmp(sym, "exit") == 0 ||
        strcmp(sym, "abort") == 0 ||
        strcmp(sym, "system") == 0 ||
        strcmp(sym, "getenv") == 0 ||
        strcmp(sym, "fopen") == 0 ||
        strcmp(sym, "fclose") == 0 ||
        strcmp(sym, "fread") == 0 ||
        strcmp(sym, "fwrite") == 0 ||
        strcmp(sym, "fseek") == 0 ||
        strcmp(sym, "ftell") == 0 ||
        strcmp(sym, "fflush") == 0 ||
        strcmp(sym, "strlen") == 0 ||
        strcmp(sym, "strcmp") == 0 ||
        strcmp(sym, "strncmp") == 0 ||
        strcmp(sym, "strcpy") == 0 ||
        strcmp(sym, "strncpy") == 0 ||
        strcmp(sym, "strcat") == 0 ||
        strcmp(sym, "strncat") == 0 ||
        strcmp(sym, "strchr") == 0 ||
        strcmp(sym, "strrchr") == 0 ||
        strcmp(sym, "strstr") == 0 ||
        strcmp(sym, "memcpy") == 0 ||
        strcmp(sym, "memmove") == 0 ||
        strcmp(sym, "memset") == 0 ||
        strcmp(sym, "memcmp") == 0 ||
        strcmp(sym, "sin") == 0 ||
        strcmp(sym, "cos") == 0 ||
        strcmp(sym, "tan") == 0 ||
        strcmp(sym, "asin") == 0 ||
        strcmp(sym, "acos") == 0 ||
        strcmp(sym, "atan") == 0 ||
        strcmp(sym, "atan2") == 0 ||
        strcmp(sym, "sqrt") == 0 ||
        strcmp(sym, "pow") == 0 ||
        strcmp(sym, "exp") == 0 ||
        strcmp(sym, "log") == 0 ||
        strcmp(sym, "log10") == 0 ||
        strcmp(sym, "floor") == 0 ||
        strcmp(sym, "ceil") == 0 ||
        strcmp(sym, "fabs") == 0) {
        return "msvcrt.dll";
    }
    /* Default to KERNEL32.dll for base system services */
    return "KERNEL32.dll";
}

typedef struct {
    char dll_name[64];
    size_t func_indices[64];
    size_t num_funcs;
    uint32_t iat_offset;
    uint32_t ilt_offset;
    uint32_t desc_offset;
    uint32_t name_offset;
} Win64PeDllGroup;

static size_t win64_pe_build_groups(const char **imported_funcs, size_t num_funcs, Win64PeDllGroup *groups, size_t max_groups) {
    size_t num_groups = 0;
    size_t i;
    for (i = 0; i < num_funcs; i++) {
        const char *dll = zcc_win64_pe_resolve_dll_for_symbol(imported_funcs[i]);
        int g_idx = -1;
        size_t g;
        for (g = 0; g < num_groups; g++) {
            if (strcmp(groups[g].dll_name, dll) == 0) {
                g_idx = (int)g;
                break;
            }
        }
        if (g_idx == -1 && num_groups < max_groups) {
            g_idx = (int)num_groups++;
            memset(&groups[g_idx], 0, sizeof(Win64PeDllGroup));
            strncpy(groups[g_idx].dll_name, dll, 63);
        }
        if (g_idx >= 0 && groups[g_idx].num_funcs < 64) {
            groups[g_idx].func_indices[groups[g_idx].num_funcs++] = i;
        }
    }
    return num_groups;
}

void zcc_win64_pe_calc_iat_rvas(size_t code_len, const char **imported_funcs, size_t num_imported_funcs, uint32_t *out_iat_rvas) {
    if (!out_iat_rvas || !imported_funcs || num_imported_funcs == 0) return;
    uint32_t raw_code_len = (code_len > 0) ? (uint32_t)code_len : 16;
    uint32_t idata_rva = 0x1000 + win64_pe_align_to(raw_code_len, 0x1000);

    Win64PeDllGroup groups[16];
    size_t num_groups = win64_pe_build_groups(imported_funcs, num_imported_funcs, groups, 16);

    uint32_t cur_iat = 0;
    size_t g, j;
    for (g = 0; g < num_groups; g++) {
        for (j = 0; j < groups[g].num_funcs; j++) {
            size_t orig_idx = groups[g].func_indices[j];
            out_iat_rvas[orig_idx] = idata_rva + cur_iat + (uint32_t)(j * 8);
        }
        cur_iat += (uint32_t)((groups[g].num_funcs + 1) * 8);
    }
}

int zcc_emit_win64_pe_file_with_imports(const char *filename, const uint8_t *code_bytes, size_t code_len,
                                       uint32_t entry_offset, const char **imported_funcs, size_t num_imported_funcs) {
    if (!imported_funcs || num_imported_funcs == 0) {
        return zcc_emit_win64_pe_file_ex(filename, code_bytes, code_len, entry_offset);
    }
    if (!filename) return -1;

    Win64PeDllGroup groups[16];
    size_t num_groups = win64_pe_build_groups(imported_funcs, num_imported_funcs, groups, 16);
    if (num_groups == 0) {
        return zcc_emit_win64_pe_file_ex(filename, code_bytes, code_len, entry_offset);
    }

    FILE *f = fopen(filename, "wb");
    if (!f) return -1;

    /* 1. Construct DOS Header (64 bytes) */
    IMAGE_DOS_HEADER dos_hdr;
    memset(&dos_hdr, 0, sizeof(dos_hdr));
    dos_hdr.e_magic = 0x5A4D; /* 'MZ' */
    dos_hdr.e_cblp = 0x0090;
    dos_hdr.e_cp = 0x0003;
    dos_hdr.e_cparhdr = 0x0004;
    dos_hdr.e_maxalloc = 0xFFFF;
    dos_hdr.e_sp = 0x00B8;
    dos_hdr.e_lfarlc = 0x0040;
    dos_hdr.e_lfanew = 0x0080; /* Offset to PE Signature */

    /* 2. Construct DOS Stub Message (64 bytes) */
    uint8_t dos_stub[64] = {
        0x0E, 0x1F, 0xBA, 0x0E, 0x00, 0xB4, 0x09, 0xCD,
        0x21, 0xB8, 0x01, 0x4C, 0xCD, 0x21, 'T',  'h',
        'i',  's',  ' ',  'p',  'r',  'o',  'g',  'r',
        'a',  'm',  ' ',  'c',  'a',  'n',  'n',  'o',
        't',  ' ',  'b',  'e',  ' ',  'r',  'u',  'n',
        ' ',  'i',  'n',  ' ',  'D',  'O',  'S',  ' ',
        'm',  'o',  'd',  'e',  '.',  '\r', '\r', '\n',
        '$',  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };

    /* 3. PE Signature (4 bytes) */
    uint32_t pe_sig = 0x00004550; /* 'PE\0\0' */

    /* 4. COFF File Header (20 bytes) */
    IMAGE_FILE_HEADER file_hdr;
    memset(&file_hdr, 0, sizeof(file_hdr));
    file_hdr.Machine = 0x8664; /* AMD64 / x86-64 */
    file_hdr.NumberOfSections = 3; /* .text, .idata, .data */
    file_hdr.TimeDateStamp = 0x66900000;
    file_hdr.SizeOfOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER64);
    file_hdr.Characteristics = 0x0022; /* EXECUTABLE_IMAGE | LARGE_ADDRESS_AWARE */

    /* 5. Build .idata section contents */
    uint32_t raw_code_len = (code_bytes && code_len > 0) ? (uint32_t)code_len : 16;
    uint32_t aligned_code_size = win64_pe_align_to(raw_code_len, 0x0200);
    uint32_t idata_rva = 0x1000 + win64_pe_align_to(raw_code_len, 0x1000);

    /* 5a. Calculate IAT offsets */
    uint32_t cur_iat = 0;
    size_t g, j;
    for (g = 0; g < num_groups; g++) {
        groups[g].iat_offset = cur_iat;
        cur_iat += (uint32_t)((groups[g].num_funcs + 1) * 8);
    }
    uint32_t total_iat_size = cur_iat;

    /* 5b. Calculate ILT offsets */
    uint32_t cur_ilt = total_iat_size;
    for (g = 0; g < num_groups; g++) {
        groups[g].ilt_offset = cur_ilt;
        cur_ilt += (uint32_t)((groups[g].num_funcs + 1) * 8);
    }
    uint32_t total_ilt_size = cur_ilt - total_iat_size;

    /* 5c. Calculate IMAGE_IMPORT_DESCRIPTOR offsets (num_groups descriptors + 1 null descriptor) */
    uint32_t desc_offset = cur_ilt;
    uint32_t desc_size = (uint32_t)((num_groups + 1) * sizeof(IMAGE_IMPORT_DESCRIPTOR));
    for (g = 0; g < num_groups; g++) {
        groups[g].desc_offset = desc_offset + (uint32_t)(g * sizeof(IMAGE_IMPORT_DESCRIPTOR));
    }

    /* 5d. Calculate DLL Name offsets */
    uint32_t cur_name_offset = desc_offset + desc_size;
    for (g = 0; g < num_groups; g++) {
        if (cur_name_offset % 2 != 0) cur_name_offset++;
        groups[g].name_offset = cur_name_offset;
        cur_name_offset += (uint32_t)strlen(groups[g].dll_name) + 1;
    }

    /* 5e. Calculate Hint/Name offsets */
    uint32_t cur_hn_offset = cur_name_offset;
    uint32_t *hint_name_offsets = (uint32_t *)malloc(sizeof(uint32_t) * (num_imported_funcs + 1));
    if (!hint_name_offsets) {
        fclose(f);
        return -1;
    }
    for (g = 0; g < num_groups; g++) {
        for (j = 0; j < groups[g].num_funcs; j++) {
            size_t orig_idx = groups[g].func_indices[j];
            if (cur_hn_offset % 2 != 0) cur_hn_offset++;
            hint_name_offsets[orig_idx] = cur_hn_offset;
            cur_hn_offset += 2 + (uint32_t)strlen(imported_funcs[orig_idx]) + 1;
        }
    }

    uint32_t total_idata_size = cur_hn_offset;
    uint8_t *idata_bytes = (uint8_t *)calloc(1, total_idata_size);
    if (!idata_bytes) {
        free(hint_name_offsets);
        fclose(f);
        return -1;
    }

    /* 5f. Populate DLL names */
    for (g = 0; g < num_groups; g++) {
        memcpy(idata_bytes + groups[g].name_offset, groups[g].dll_name, strlen(groups[g].dll_name) + 1);
    }

    /* 5g. Populate Hint/Name entries */
    for (g = 0; g < num_groups; g++) {
        for (j = 0; j < groups[g].num_funcs; j++) {
            size_t orig_idx = groups[g].func_indices[j];
            uint32_t off = hint_name_offsets[orig_idx];
            /* Hint is uint16_t 0 */
            strcpy((char *)(idata_bytes + off + 2), imported_funcs[orig_idx]);
        }
    }

    /* 5h. Populate IAT and ILT */
    for (g = 0; g < num_groups; g++) {
        for (j = 0; j < groups[g].num_funcs; j++) {
            size_t orig_idx = groups[g].func_indices[j];
            uint64_t target_rva = (uint64_t)(idata_rva + hint_name_offsets[orig_idx]);
            memcpy(idata_bytes + groups[g].iat_offset + j * 8, &target_rva, sizeof(uint64_t));
            memcpy(idata_bytes + groups[g].ilt_offset + j * 8, &target_rva, sizeof(uint64_t));
        }
        /* Terminators at end of each slice are already 0 from calloc */
    }

    /* 5i. Populate IMAGE_IMPORT_DESCRIPTOR array */
    for (g = 0; g < num_groups; g++) {
        IMAGE_IMPORT_DESCRIPTOR desc;
        memset(&desc, 0, sizeof(desc));
        desc.OriginalFirstThunk = idata_rva + groups[g].ilt_offset;
        desc.TimeDateStamp = 0;
        desc.ForwarderChain = 0;
        desc.Name = idata_rva + groups[g].name_offset;
        desc.FirstThunk = idata_rva + groups[g].iat_offset;
        memcpy(idata_bytes + groups[g].desc_offset, &desc, sizeof(desc));
    }
    /* Terminating descriptor at desc_offset + num_groups * 20 is already 0 from calloc */

    uint32_t aligned_idata_size = win64_pe_align_to(total_idata_size, 0x0200);
    uint32_t data_rva = idata_rva + win64_pe_align_to(total_idata_size, 0x1000);

    /* 6. Headers sizing */
    uint32_t headers_size = sizeof(IMAGE_DOS_HEADER) + sizeof(dos_stub) + sizeof(pe_sig) +
                            sizeof(IMAGE_FILE_HEADER) + sizeof(IMAGE_OPTIONAL_HEADER64) +
                            (3 * sizeof(IMAGE_SECTION_HEADER));
    uint32_t aligned_headers_size = win64_pe_align_to(headers_size, 0x0200);

    /* 7. PE32+ Optional Header */
    IMAGE_OPTIONAL_HEADER64 opt_hdr;
    memset(&opt_hdr, 0, sizeof(opt_hdr));
    opt_hdr.Magic = 0x020B; /* PE32+ */
    opt_hdr.MajorLinkerVersion = 14;
    opt_hdr.MinorLinkerVersion = 0;
    opt_hdr.SizeOfCode = aligned_code_size;
    opt_hdr.SizeOfInitializedData = aligned_idata_size + 0x0200;
    opt_hdr.AddressOfEntryPoint = 0x1000 + entry_offset;
    opt_hdr.BaseOfCode = 0x1000;
    opt_hdr.ImageBase = 0x140000000ULL;
    opt_hdr.SectionAlignment = 0x1000;
    opt_hdr.FileAlignment = 0x0200;
    opt_hdr.MajorOperatingSystemVersion = 6;
    opt_hdr.MinorOperatingSystemVersion = 0;
    opt_hdr.MajorSubsystemVersion = 6;
    opt_hdr.MinorSubsystemVersion = 0;
    opt_hdr.SizeOfImage = data_rva + 0x1000;
    opt_hdr.SizeOfHeaders = aligned_headers_size;
    opt_hdr.Subsystem = 3; /* Console */
    opt_hdr.DllCharacteristics = 0x8160;
    opt_hdr.SizeOfStackReserve = 0x100000;
    opt_hdr.SizeOfStackCommit = 0x1000;
    opt_hdr.SizeOfHeapReserve = 0x100000;
    opt_hdr.SizeOfHeapCommit = 0x1000;
    opt_hdr.NumberOfRvaAndSizes = 16;

    /* Import Directory: DataDirectory[1] */
    opt_hdr.DataDirectory[1].VirtualAddress = idata_rva + desc_offset;
    opt_hdr.DataDirectory[1].Size = desc_size;

    /* IAT: DataDirectory[12] */
    opt_hdr.DataDirectory[12].VirtualAddress = idata_rva;
    opt_hdr.DataDirectory[12].Size = total_iat_size;

    /* 8. Section Headers */
    IMAGE_SECTION_HEADER sec_text;
    memset(&sec_text, 0, sizeof(sec_text));
    memcpy(sec_text.Name, ".text\0\0\0", 8);
    sec_text.VirtualSize = raw_code_len;
    sec_text.VirtualAddress = 0x1000;
    sec_text.SizeOfRawData = aligned_code_size;
    sec_text.PointerToRawData = aligned_headers_size;
    sec_text.Characteristics = 0xE0000020; /* CODE | EXECUTE | READ | WRITE */

    IMAGE_SECTION_HEADER sec_idata;
    memset(&sec_idata, 0, sizeof(sec_idata));
    memcpy(sec_idata.Name, ".idata\0\0", 8);
    sec_idata.VirtualSize = total_idata_size;
    sec_idata.VirtualAddress = idata_rva;
    sec_idata.SizeOfRawData = aligned_idata_size;
    sec_idata.PointerToRawData = aligned_headers_size + aligned_code_size;
    sec_idata.Characteristics = 0xC0000040; /* INITIALIZED_DATA | READ | WRITE */

    IMAGE_SECTION_HEADER sec_data;
    memset(&sec_data, 0, sizeof(sec_data));
    memcpy(sec_data.Name, ".data\0\0\0", 8);
    sec_data.VirtualSize = 16;
    sec_data.VirtualAddress = data_rva;
    sec_data.SizeOfRawData = 0x0200;
    sec_data.PointerToRawData = aligned_headers_size + aligned_code_size + aligned_idata_size;
    sec_data.Characteristics = 0xC0000040; /* INITIALIZED_DATA | READ | WRITE */

    /* Write Headers */
    fwrite(&dos_hdr, sizeof(dos_hdr), 1, f);
    fwrite(dos_stub, sizeof(dos_stub), 1, f);
    fwrite(&pe_sig, sizeof(pe_sig), 1, f);
    fwrite(&file_hdr, sizeof(file_hdr), 1, f);
    fwrite(&opt_hdr, sizeof(opt_hdr), 1, f);
    fwrite(&sec_text, sizeof(sec_text), 1, f);
    fwrite(&sec_idata, sizeof(sec_idata), 1, f);
    fwrite(&sec_data, sizeof(sec_data), 1, f);

    /* Pad headers */
    size_t written_headers = sizeof(dos_hdr) + sizeof(dos_stub) + sizeof(pe_sig) +
                             sizeof(file_hdr) + sizeof(opt_hdr) + (3 * sizeof(IMAGE_SECTION_HEADER));
    if (aligned_headers_size > written_headers) {
        size_t pad_len = aligned_headers_size - written_headers;
        uint8_t *pad = (uint8_t *)calloc(1, pad_len);
        fwrite(pad, 1, pad_len, f);
        free(pad);
    }

    /* Write .text */
    if (code_bytes && code_len > 0) {
        fwrite(code_bytes, 1, code_len, f);
        if (aligned_code_size > code_len) {
            size_t pad_code = aligned_code_size - code_len;
            uint8_t *pad = (uint8_t *)calloc(1, pad_code);
            fwrite(pad, 1, pad_code, f);
            free(pad);
        }
    } else {
        uint8_t dummy_code[7] = { 0xB8, 0x2A, 0x00, 0x00, 0x00, 0xC3, 0x90 };
        fwrite(dummy_code, 1, sizeof(dummy_code), f);
        size_t pad_code = aligned_code_size - sizeof(dummy_code);
        uint8_t *pad = (uint8_t *)calloc(1, pad_code);
        fwrite(pad, 1, pad_code, f);
        free(pad);
    }

    /* Write .idata */
    fwrite(idata_bytes, 1, total_idata_size, f);
    if (aligned_idata_size > total_idata_size) {
        size_t pad_idata = aligned_idata_size - total_idata_size;
        uint8_t *pad = (uint8_t *)calloc(1, pad_idata);
        fwrite(pad, 1, pad_idata, f);
        free(pad);
    }

    /* Write dummy .data section (512 bytes aligned) */
    uint8_t *data_pad = (uint8_t *)calloc(1, 0x0200);
    fwrite(data_pad, 1, 0x0200, f);
    free(data_pad);

    free(hint_name_offsets);
    free(idata_bytes);
    fclose(f);
    return 0;
}

