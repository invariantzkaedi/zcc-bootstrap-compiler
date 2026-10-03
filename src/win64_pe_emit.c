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

static uint8_t *win64_pe_build_relocs(const uint32_t *reloc_rvas, size_t num_relocs, uint32_t *out_total_size) {
    if (!reloc_rvas || num_relocs == 0) {
        if (out_total_size) *out_total_size = 0;
        return NULL;
    }

    /* 1. Copy and sort RVAs */
    uint32_t *sorted = (uint32_t *)malloc(sizeof(uint32_t) * num_relocs);
    if (!sorted) {
        if (out_total_size) *out_total_size = 0;
        return NULL;
    }
    memcpy(sorted, reloc_rvas, sizeof(uint32_t) * num_relocs);

    size_t i, j;
    for (i = 0; i < num_relocs; i++) {
        for (j = i + 1; j < num_relocs; j++) {
            if (sorted[j] < sorted[i]) {
                uint32_t tmp = sorted[i];
                sorted[i] = sorted[j];
                sorted[j] = tmp;
            }
        }
    }

    /* 2. Calculate required size by grouping by 4KB page (rva & ~0xFFF) */
    uint32_t total_size = 0;
    i = 0;
    while (i < num_relocs) {
        uint32_t page_rva = sorted[i] & ~0xFFF;
        size_t count = 0;
        while (i < num_relocs && (sorted[i] & ~0xFFF) == page_rva) {
            count++;
            i++;
        }
        uint32_t block_size = 8 + (uint32_t)(count * 2);
        if (count % 2 != 0) block_size += 2; /* 4-byte align */
        total_size += block_size;
    }

    /* 3. Allocate and populate */
    uint8_t *buf = (uint8_t *)calloc(1, total_size);
    if (!buf) {
        free(sorted);
        if (out_total_size) *out_total_size = 0;
        return NULL;
    }

    uint32_t cur_offset = 0;
    i = 0;
    while (i < num_relocs) {
        uint32_t page_rva = sorted[i] & ~0xFFF;
        size_t start_idx = i;
        size_t count = 0;
        while (i < num_relocs && (sorted[i] & ~0xFFF) == page_rva) {
            count++;
            i++;
        }
        uint32_t block_size = 8 + (uint32_t)(count * 2);
        if (count % 2 != 0) block_size += 2;

        IMAGE_BASE_RELOCATION base_reloc;
        base_reloc.VirtualAddress = page_rva;
        base_reloc.SizeOfBlock = block_size;
        memcpy(buf + cur_offset, &base_reloc, sizeof(base_reloc));

        size_t k;
        for (k = 0; k < count; k++) {
            uint32_t rva = sorted[start_idx + k];
            uint16_t entry = (uint16_t)((0xA << 12) | (rva & 0xFFF)); /* IMAGE_REL_BASED_DIR64 */
            memcpy(buf + cur_offset + 8 + (k * 2), &entry, sizeof(uint16_t));
        }
        if (count % 2 != 0) {
            uint16_t pad = 0; /* IMAGE_REL_BASED_ABSOLUTE */
            memcpy(buf + cur_offset + 8 + (count * 2), &pad, sizeof(uint16_t));
        }
        cur_offset += block_size;
    }

    free(sorted);
    if (out_total_size) *out_total_size = total_size;
    return buf;
}

static uint8_t *win64_pe_build_pdata(uint32_t text_rva, uint32_t raw_code_len, uint32_t entry_offset,
                                     const win64_pe_func_extent_t *extents, size_t num_extents,
                                     uint32_t pdata_rva, uint32_t *out_pdata_size, uint32_t *out_entries_size) {
    size_t count = (extents && num_extents > 0) ? num_extents : 1;
    win64_pe_func_extent_t *sorted = (win64_pe_func_extent_t *)malloc(sizeof(win64_pe_func_extent_t) * count);
    if (!sorted) return NULL;

    if (extents && num_extents > 0) {
        memcpy(sorted, extents, sizeof(win64_pe_func_extent_t) * num_extents);
        /* Bubble sort by begin_offset ascending (PE x64 ABI requirement) */
        size_t i, j;
        for (i = 0; i < count; i++) {
            for (j = i + 1; j < count; j++) {
                if (sorted[j].begin_offset < sorted[i].begin_offset) {
                    win64_pe_func_extent_t tmp = sorted[i];
                    sorted[i] = sorted[j];
                    sorted[j] = tmp;
                }
            }
        }
    } else {
        sorted[0].begin_offset = entry_offset;
        sorted[0].end_offset = (raw_code_len > entry_offset) ? raw_code_len : (entry_offset + 16);
    }

    uint32_t entries_size = (uint32_t)(count * sizeof(IMAGE_RUNTIME_FUNCTION_ENTRY));
    uint32_t unwind_info_size = (uint32_t)(count * 8);
    uint32_t total_size = entries_size + unwind_info_size;
    uint8_t *buf = (uint8_t *)calloc(1, total_size);
    if (!buf) {
        free(sorted);
        return NULL;
    }

    size_t i;
    for (i = 0; i < count; i++) {
        IMAGE_RUNTIME_FUNCTION_ENTRY entry;
        entry.BeginAddress = text_rva + sorted[i].begin_offset;
        entry.EndAddress = text_rva + sorted[i].end_offset;
        entry.UnwindInfoAddress = pdata_rva + entries_size + (uint32_t)(i * 8);
        memcpy(buf + i * sizeof(IMAGE_RUNTIME_FUNCTION_ENTRY), &entry, sizeof(IMAGE_RUNTIME_FUNCTION_ENTRY));

        /* Synthesize UNWIND_INFO at offset (entries_size + i * 8) */
        uint8_t *uinfo = buf + entries_size + (i * 8);
        uinfo[0] = 0x01; /* Version = 1, Flags = 0 (UNW_FLAG_NHANDLER) */
        uinfo[1] = 0x04; /* SizeOfProlog = 4 bytes (push %rbp [1] + mov %rsp, %rbp [3]) */
        uinfo[2] = 0x02; /* CountOfCodes = 2 */
        uinfo[3] = 0x05; /* FrameRegister = 5 (%rbp), FrameOffset = 0 */
        /* Code 0: UWOP_SET_FPREG (Op 3, Info 0) at prolog offset 4: (0 << 12) | (3 << 8) | 4 = 0x0304 */
        uint16_t code0 = 0x0304;
        memcpy(uinfo + 4, &code0, sizeof(uint16_t));
        /* Code 1: UWOP_PUSH_NONVOL (Op 0, Info 5 for RBP) at prolog offset 1: (5 << 12) | (0 << 8) | 1 = 0x5001 */
        uint16_t code1 = 0x5001;
        memcpy(uinfo + 6, &code1, sizeof(uint16_t));
    }

    free(sorted);
    if (out_pdata_size) *out_pdata_size = total_size;
    if (out_entries_size) *out_entries_size = entries_size;
    return buf;
}

/* Standard Windows Resource Directory Structures for .rsrc emission */
typedef struct {
    uint32_t Characteristics;
    uint32_t TimeDateStamp;
    uint16_t MajorVersion;
    uint16_t MinorVersion;
    uint16_t NumberOfNamedEntries;
    uint16_t NumberOfIdEntries;
} IMAGE_RESOURCE_DIRECTORY;

typedef struct {
    uint32_t NameOrId;
    uint32_t OffsetToDataOrDirectory;
} IMAGE_RESOURCE_DIRECTORY_ENTRY;

typedef struct {
    uint32_t OffsetToData;
    uint32_t Size;
    uint32_t CodePage;
    uint32_t Reserved;
} IMAGE_RESOURCE_DATA_ENTRY;

uint8_t *win64_pe_build_default_manifest_rsrc(const char *app_name, uint32_t rsrc_rva, uint32_t *out_size) {
    const char *manifest_xml_template =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\r\n"
        "<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\">\r\n"
        "  <assemblyIdentity version=\"1.0.0.0\" processorArchitecture=\"amd64\" name=\"%s\" type=\"win32\"/>\r\n"
        "  <description>%s Application</description>\r\n"
        "  <dependency>\r\n"
        "    <dependentAssembly>\r\n"
        "      <assemblyIdentity type=\"win32\" name=\"Microsoft.Windows.Common-Controls\" version=\"6.0.0.0\" processorArchitecture=\"*\" publicKeyToken=\"6595b64144ccf1df\" language=\"*\"/>\r\n"
        "    </dependentAssembly>\r\n"
        "  </dependency>\r\n"
        "</assembly>\r\n";

    char xml_buf[1024];
    const char *name = app_name ? app_name : "ZCC.Win64.App";
    snprintf(xml_buf, sizeof(xml_buf), manifest_xml_template, name, name);
    uint32_t xml_len = (uint32_t)strlen(xml_buf);

    uint32_t data_entry_offset = 0x48;
    uint32_t raw_xml_offset = 0x58;
    uint32_t total_len = raw_xml_offset + xml_len;
    uint32_t aligned_len = win64_pe_align_to(total_len, 4);

    uint8_t *buf = (uint8_t *)calloc(1, aligned_len);
    if (!buf) {
        if (out_size) *out_size = 0;
        return NULL;
    }

    IMAGE_RESOURCE_DIRECTORY *root = (IMAGE_RESOURCE_DIRECTORY *)(buf + 0x00);
    root->NumberOfIdEntries = 1;
    IMAGE_RESOURCE_DIRECTORY_ENTRY *root_entry = (IMAGE_RESOURCE_DIRECTORY_ENTRY *)(buf + 0x10);
    root_entry->NameOrId = 24; /* RT_MANIFEST */
    root_entry->OffsetToDataOrDirectory = 0x80000018;

    IMAGE_RESOURCE_DIRECTORY *type_dir = (IMAGE_RESOURCE_DIRECTORY *)(buf + 0x18);
    type_dir->NumberOfIdEntries = 1;
    IMAGE_RESOURCE_DIRECTORY_ENTRY *type_entry = (IMAGE_RESOURCE_DIRECTORY_ENTRY *)(buf + 0x28);
    type_entry->NameOrId = 1;
    type_entry->OffsetToDataOrDirectory = 0x80000030;

    IMAGE_RESOURCE_DIRECTORY *lang_dir = (IMAGE_RESOURCE_DIRECTORY *)(buf + 0x30);
    lang_dir->NumberOfIdEntries = 1;
    IMAGE_RESOURCE_DIRECTORY_ENTRY *lang_entry = (IMAGE_RESOURCE_DIRECTORY_ENTRY *)(buf + 0x40);
    lang_entry->NameOrId = 0x0409; /* en-US */
    lang_entry->OffsetToDataOrDirectory = data_entry_offset;

    IMAGE_RESOURCE_DATA_ENTRY *data_entry = (IMAGE_RESOURCE_DATA_ENTRY *)(buf + data_entry_offset);
    data_entry->OffsetToData = rsrc_rva + raw_xml_offset;
    data_entry->Size = xml_len;
    data_entry->CodePage = 0;

    memcpy(buf + raw_xml_offset, xml_buf, xml_len);

    if (out_size) *out_size = aligned_len;
    return buf;
}

int zcc_emit_win64_pe_file_v5(const char *filename, const uint8_t *code_bytes,
                              size_t code_len, uint32_t entry_offset,
                              const char **imported_funcs, size_t num_imported_funcs,
                              const uint32_t *reloc_rvas, size_t num_reloc_rvas,
                              const win64_pe_func_extent_t *extents, size_t num_extents,
                              uint16_t subsystem,
                              const uint8_t *rsrc_bytes, size_t rsrc_len) {
    if (!filename) return -1;

    Win64PeDllGroup groups[16];
    size_t num_groups = 0;
    if (imported_funcs && num_imported_funcs > 0) {
        num_groups = win64_pe_build_groups(imported_funcs, num_imported_funcs, groups, 16);
    }

    uint32_t total_reloc_size = 0;
    uint8_t *reloc_bytes = win64_pe_build_relocs(reloc_rvas, num_reloc_rvas, &total_reloc_size);
    int has_reloc = (reloc_bytes != NULL && total_reloc_size > 0);
    uint32_t aligned_reloc_size = has_reloc ? win64_pe_align_to(total_reloc_size, 0x0200) : 0;

    int has_idata = (num_groups > 0);

    FILE *f = fopen(filename, "wb");
    if (!f) {
        if (reloc_bytes) free(reloc_bytes);
        return -1;
    }

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
    int has_pdata = 1; /* Track 4: Always emit .pdata exception directory */
    uint32_t raw_code_len = (code_bytes && code_len > 0) ? (uint32_t)code_len : 16;
    uint32_t idata_rva = 0x1000 + win64_pe_align_to(raw_code_len, 0x1000);

    /* Check if default manifest should be synthesized */
    uint8_t *allocated_rsrc = NULL;
    uint32_t total_rsrc_size = (uint32_t)rsrc_len;
    const uint8_t *final_rsrc_bytes = rsrc_bytes;
    if (subsystem == 2 && (!rsrc_bytes || rsrc_len == 0)) {
        /* Default GUI manifest will be computed once data_rva is established */
    }

    int has_rsrc = (final_rsrc_bytes != NULL && total_rsrc_size > 0);
    uint16_t num_sections = 2; /* .text and .data */
    if (has_idata) num_sections++;
    if (has_pdata) num_sections++;
    if (has_reloc) num_sections++;

    IMAGE_FILE_HEADER file_hdr;
    memset(&file_hdr, 0, sizeof(file_hdr));
    file_hdr.Machine = 0x8664; /* AMD64 / x86-64 */
    file_hdr.NumberOfSections = num_sections;
    file_hdr.TimeDateStamp = 0x66900000;
    file_hdr.SizeOfOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER64);
    file_hdr.Characteristics = 0x0022; /* EXECUTABLE_IMAGE | LARGE_ADDRESS_AWARE */

    /* 5. Build .idata section contents if present */
    uint32_t aligned_code_size = win64_pe_align_to(raw_code_len, 0x0200);

    uint32_t total_iat_size = 0;
    uint32_t desc_offset = 0;
    uint32_t desc_size = 0;
    uint32_t total_idata_size = 0;
    uint8_t *idata_bytes = NULL;
    uint32_t *hint_name_offsets = NULL;

    if (has_idata) {
        uint32_t cur_iat = 0;
        size_t g, j;
        for (g = 0; g < num_groups; g++) {
            groups[g].iat_offset = cur_iat;
            cur_iat += (uint32_t)((groups[g].num_funcs + 1) * 8);
        }
        total_iat_size = cur_iat;

        uint32_t cur_ilt = total_iat_size;
        for (g = 0; g < num_groups; g++) {
            groups[g].ilt_offset = cur_ilt;
            cur_ilt += (uint32_t)((groups[g].num_funcs + 1) * 8);
        }

        desc_offset = cur_ilt;
        desc_size = (uint32_t)((num_groups + 1) * sizeof(IMAGE_IMPORT_DESCRIPTOR));
        for (g = 0; g < num_groups; g++) {
            groups[g].desc_offset = desc_offset + (uint32_t)(g * sizeof(IMAGE_IMPORT_DESCRIPTOR));
        }

        uint32_t cur_name_offset = desc_offset + desc_size;
        for (g = 0; g < num_groups; g++) {
            if (cur_name_offset % 2 != 0) cur_name_offset++;
            groups[g].name_offset = cur_name_offset;
            cur_name_offset += (uint32_t)strlen(groups[g].dll_name) + 1;
        }

        uint32_t cur_hn_offset = cur_name_offset;
        hint_name_offsets = (uint32_t *)malloc(sizeof(uint32_t) * (num_imported_funcs + 1));
        if (!hint_name_offsets) {
            fclose(f);
            if (reloc_bytes) free(reloc_bytes);
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

        total_idata_size = cur_hn_offset;
        idata_bytes = (uint8_t *)calloc(1, total_idata_size);
        if (!idata_bytes) {
            free(hint_name_offsets);
            fclose(f);
            if (reloc_bytes) free(reloc_bytes);
            return -1;
        }

        for (g = 0; g < num_groups; g++) {
            memcpy(idata_bytes + groups[g].name_offset, groups[g].dll_name, strlen(groups[g].dll_name) + 1);
        }

        for (g = 0; g < num_groups; g++) {
            for (j = 0; j < groups[g].num_funcs; j++) {
                size_t orig_idx = groups[g].func_indices[j];
                uint32_t off = hint_name_offsets[orig_idx];
                strcpy((char *)(idata_bytes + off + 2), imported_funcs[orig_idx]);
            }
        }

        for (g = 0; g < num_groups; g++) {
            for (j = 0; j < groups[g].num_funcs; j++) {
                size_t orig_idx = groups[g].func_indices[j];
                uint64_t target_rva = (uint64_t)(idata_rva + hint_name_offsets[orig_idx]);
                memcpy(idata_bytes + groups[g].iat_offset + j * 8, &target_rva, sizeof(uint64_t));
                memcpy(idata_bytes + groups[g].ilt_offset + j * 8, &target_rva, sizeof(uint64_t));
            }
        }

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
    }

    uint32_t aligned_idata_size = has_idata ? win64_pe_align_to(total_idata_size, 0x0200) : 0;
    uint32_t pdata_rva = has_idata ? (idata_rva + win64_pe_align_to(total_idata_size, 0x1000)) : idata_rva;

    /* 6. Build .pdata section contents (Track 4) */
    uint32_t total_pdata_size = 0;
    uint32_t pdata_entries_size = 0;
    uint8_t *pdata_bytes = win64_pe_build_pdata(0x1000, raw_code_len, entry_offset,
                                               extents, num_extents,
                                               pdata_rva, &total_pdata_size, &pdata_entries_size);
    uint32_t aligned_pdata_size = win64_pe_align_to(total_pdata_size, 0x0200);
    uint32_t data_rva = pdata_rva + win64_pe_align_to(total_pdata_size, 0x1000);
    uint32_t rsrc_rva = data_rva + 0x1000;

    if (subsystem == 2 && (!rsrc_bytes || rsrc_len == 0)) {
        allocated_rsrc = win64_pe_build_default_manifest_rsrc("ZCC.Win64.App", rsrc_rva, &total_rsrc_size);
        if (allocated_rsrc && total_rsrc_size > 0) {
            final_rsrc_bytes = allocated_rsrc;
            has_rsrc = 1;
        }
    }
    if (has_rsrc) num_sections++;
    file_hdr.NumberOfSections = num_sections;

    uint32_t aligned_rsrc_size = has_rsrc ? win64_pe_align_to(total_rsrc_size, 0x0200) : 0;
    uint32_t reloc_rva = has_rsrc ? (rsrc_rva + win64_pe_align_to(total_rsrc_size, 0x1000)) : (data_rva + 0x1000);

    /* 7. Headers sizing */
    uint32_t headers_size = sizeof(IMAGE_DOS_HEADER) + sizeof(dos_stub) + sizeof(pe_sig) +
                            sizeof(IMAGE_FILE_HEADER) + sizeof(IMAGE_OPTIONAL_HEADER64) +
                            (num_sections * sizeof(IMAGE_SECTION_HEADER));
    uint32_t aligned_headers_size = win64_pe_align_to(headers_size, 0x0200);

    /* 8. PE32+ Optional Header */
    IMAGE_OPTIONAL_HEADER64 opt_hdr;
    memset(&opt_hdr, 0, sizeof(opt_hdr));
    opt_hdr.Magic = 0x020B; /* PE32+ */
    opt_hdr.MajorLinkerVersion = 14;
    opt_hdr.MinorLinkerVersion = 0;
    opt_hdr.SizeOfCode = aligned_code_size;
    opt_hdr.SizeOfInitializedData = aligned_idata_size + aligned_pdata_size + 0x0200 + aligned_rsrc_size + aligned_reloc_size;
    opt_hdr.AddressOfEntryPoint = 0x1000 + entry_offset;
    opt_hdr.BaseOfCode = 0x1000;
    opt_hdr.ImageBase = 0x140000000ULL;
    opt_hdr.SectionAlignment = 0x1000;
    opt_hdr.FileAlignment = 0x0200;
    opt_hdr.MajorOperatingSystemVersion = 6;
    opt_hdr.MinorOperatingSystemVersion = 0;
    opt_hdr.MajorSubsystemVersion = 6;
    opt_hdr.MinorSubsystemVersion = 0;
    opt_hdr.SizeOfImage = has_reloc ? (reloc_rva + win64_pe_align_to(total_reloc_size, 0x1000)) : (has_rsrc ? (rsrc_rva + win64_pe_align_to(total_rsrc_size, 0x1000)) : (data_rva + 0x1000));
    opt_hdr.SizeOfHeaders = aligned_headers_size;
    opt_hdr.Subsystem = (subsystem == 2) ? 2 : 3;
    opt_hdr.DllCharacteristics = 0x8160; /* HIGH_ENTROPY_VA | DYNAMIC_BASE | NX_COMPAT | TERMINAL_SERVER_AWARE */
    opt_hdr.SizeOfStackReserve = 0x100000;
    opt_hdr.SizeOfStackCommit = 0x1000;
    opt_hdr.SizeOfHeapReserve = 0x100000;
    opt_hdr.SizeOfHeapCommit = 0x1000;
    opt_hdr.NumberOfRvaAndSizes = 16;

    if (has_idata) {
        opt_hdr.DataDirectory[1].VirtualAddress = idata_rva + desc_offset;
        opt_hdr.DataDirectory[1].Size = desc_size;
        opt_hdr.DataDirectory[12].VirtualAddress = idata_rva;
        opt_hdr.DataDirectory[12].Size = total_iat_size;
    }

    if (has_pdata && pdata_bytes) {
        opt_hdr.DataDirectory[3].VirtualAddress = pdata_rva;
        opt_hdr.DataDirectory[3].Size = pdata_entries_size;
    }

    if (has_rsrc) {
        opt_hdr.DataDirectory[2].VirtualAddress = rsrc_rva;
        opt_hdr.DataDirectory[2].Size = total_rsrc_size;
    }

    if (has_reloc) {
        opt_hdr.DataDirectory[5].VirtualAddress = reloc_rva;
        opt_hdr.DataDirectory[5].Size = total_reloc_size;
    }

    /* 9. Section Headers */
    IMAGE_SECTION_HEADER sec_text;
    memset(&sec_text, 0, sizeof(sec_text));
    memcpy(sec_text.Name, ".text\0\0\0", 8);
    sec_text.VirtualSize = raw_code_len;
    sec_text.VirtualAddress = 0x1000;
    sec_text.SizeOfRawData = aligned_code_size;
    sec_text.PointerToRawData = aligned_headers_size;
    sec_text.Characteristics = has_reloc ? 0x60000020 : 0xE0000020; /* CODE | EXEC | READ */

    IMAGE_SECTION_HEADER sec_idata;
    memset(&sec_idata, 0, sizeof(sec_idata));
    if (has_idata) {
        memcpy(sec_idata.Name, ".idata\0\0", 8);
        sec_idata.VirtualSize = total_idata_size;
        sec_idata.VirtualAddress = idata_rva;
        sec_idata.SizeOfRawData = aligned_idata_size;
        sec_idata.PointerToRawData = aligned_headers_size + aligned_code_size;
        sec_idata.Characteristics = 0xC0000040; /* INITIALIZED_DATA | READ | WRITE */
    }

    IMAGE_SECTION_HEADER sec_pdata;
    memset(&sec_pdata, 0, sizeof(sec_pdata));
    if (has_pdata) {
        memcpy(sec_pdata.Name, ".pdata\0\0", 8);
        sec_pdata.VirtualSize = total_pdata_size;
        sec_pdata.VirtualAddress = pdata_rva;
        sec_pdata.SizeOfRawData = aligned_pdata_size;
        sec_pdata.PointerToRawData = aligned_headers_size + aligned_code_size + aligned_idata_size;
        sec_pdata.Characteristics = 0x40000040; /* INITIALIZED_DATA | READ */
    }

    IMAGE_SECTION_HEADER sec_data;
    memset(&sec_data, 0, sizeof(sec_data));
    memcpy(sec_data.Name, ".data\0\0\0", 8);
    sec_data.VirtualSize = 16;
    sec_data.VirtualAddress = data_rva;
    sec_data.SizeOfRawData = 0x0200;
    sec_data.PointerToRawData = aligned_headers_size + aligned_code_size + aligned_idata_size + aligned_pdata_size;
    sec_data.Characteristics = 0xC0000040; /* INITIALIZED_DATA | READ | WRITE */

    IMAGE_SECTION_HEADER sec_rsrc;
    memset(&sec_rsrc, 0, sizeof(sec_rsrc));
    if (has_rsrc) {
        memcpy(sec_rsrc.Name, ".rsrc\0\0\0", 8);
        sec_rsrc.VirtualSize = total_rsrc_size;
        sec_rsrc.VirtualAddress = rsrc_rva;
        sec_rsrc.SizeOfRawData = aligned_rsrc_size;
        sec_rsrc.PointerToRawData = aligned_headers_size + aligned_code_size + aligned_idata_size + aligned_pdata_size + 0x0200;
        sec_rsrc.Characteristics = 0x40000040; /* INITIALIZED_DATA | READ */
    }

    IMAGE_SECTION_HEADER sec_reloc;
    memset(&sec_reloc, 0, sizeof(sec_reloc));
    if (has_reloc) {
        memcpy(sec_reloc.Name, ".reloc\0\0", 8);
        sec_reloc.VirtualSize = total_reloc_size;
        sec_reloc.VirtualAddress = reloc_rva;
        sec_reloc.SizeOfRawData = aligned_reloc_size;
        sec_reloc.PointerToRawData = aligned_headers_size + aligned_code_size + aligned_idata_size + aligned_pdata_size + 0x0200 + aligned_rsrc_size;
        sec_reloc.Characteristics = 0x42000040; /* INITIALIZED_DATA | DISCARDABLE | READ */
    }

    /* Write Headers */
    fwrite(&dos_hdr, sizeof(dos_hdr), 1, f);
    fwrite(dos_stub, sizeof(dos_stub), 1, f);
    fwrite(&pe_sig, sizeof(pe_sig), 1, f);
    fwrite(&file_hdr, sizeof(file_hdr), 1, f);
    fwrite(&opt_hdr, sizeof(opt_hdr), 1, f);
    fwrite(&sec_text, sizeof(sec_text), 1, f);
    if (has_idata) fwrite(&sec_idata, sizeof(sec_idata), 1, f);
    if (has_pdata) fwrite(&sec_pdata, sizeof(sec_pdata), 1, f);
    fwrite(&sec_data, sizeof(sec_data), 1, f);
    if (has_rsrc) fwrite(&sec_rsrc, sizeof(sec_rsrc), 1, f);
    if (has_reloc) fwrite(&sec_reloc, sizeof(sec_reloc), 1, f);

    /* Pad headers */
    size_t written_headers = sizeof(dos_hdr) + sizeof(dos_stub) + sizeof(pe_sig) +
                             sizeof(file_hdr) + sizeof(opt_hdr) + (num_sections * sizeof(IMAGE_SECTION_HEADER));
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
    if (has_idata && idata_bytes) {
        fwrite(idata_bytes, 1, total_idata_size, f);
        if (aligned_idata_size > total_idata_size) {
            size_t pad_idata = aligned_idata_size - total_idata_size;
            uint8_t *pad = (uint8_t *)calloc(1, pad_idata);
            fwrite(pad, 1, pad_idata, f);
            free(pad);
        }
    }

    /* Write .pdata */
    if (has_pdata && pdata_bytes) {
        fwrite(pdata_bytes, 1, total_pdata_size, f);
        if (aligned_pdata_size > total_pdata_size) {
            size_t pad_pdata = aligned_pdata_size - total_pdata_size;
            uint8_t *pad = (uint8_t *)calloc(1, pad_pdata);
            fwrite(pad, 1, pad_pdata, f);
            free(pad);
        }
    }

    /* Write dummy .data section (512 bytes aligned) */
    uint8_t *data_pad = (uint8_t *)calloc(1, 0x0200);
    fwrite(data_pad, 1, 0x0200, f);
    free(data_pad);

    /* Write .rsrc section */
    if (has_rsrc && final_rsrc_bytes) {
        fwrite(final_rsrc_bytes, 1, total_rsrc_size, f);
        if (aligned_rsrc_size > total_rsrc_size) {
            size_t pad_rsrc = aligned_rsrc_size - total_rsrc_size;
            uint8_t *pad = (uint8_t *)calloc(1, pad_rsrc);
            fwrite(pad, 1, pad_rsrc, f);
            free(pad);
        }
    }

    /* Write .reloc section */
    if (has_reloc && reloc_bytes) {
        fwrite(reloc_bytes, 1, total_reloc_size, f);
        if (aligned_reloc_size > total_reloc_size) {
            size_t pad_reloc = aligned_reloc_size - total_reloc_size;
            uint8_t *pad = (uint8_t *)calloc(1, pad_reloc);
            fwrite(pad, 1, pad_reloc, f);
            free(pad);
        }
    }

    if (hint_name_offsets) free(hint_name_offsets);
    if (idata_bytes) free(idata_bytes);
    if (pdata_bytes) free(pdata_bytes);
    if (reloc_bytes) free(reloc_bytes);
    if (allocated_rsrc) free(allocated_rsrc);
    fclose(f);
    return 0;
}

int zcc_emit_win64_pe_file_v4(const char *filename, const uint8_t *code_bytes, size_t code_len,
                              uint32_t entry_offset, const char **imported_funcs, size_t num_imported_funcs,
                              const uint32_t *reloc_rvas, size_t num_reloc_rvas,
                              const win64_pe_func_extent_t *extents, size_t num_extents) {
    return zcc_emit_win64_pe_file_v5(filename, code_bytes, code_len, entry_offset,
                                     imported_funcs, num_imported_funcs,
                                     reloc_rvas, num_reloc_rvas,
                                     extents, num_extents,
                                     3, NULL, 0);
}

int zcc_emit_win64_pe_file_full(const char *filename, const uint8_t *code_bytes, size_t code_len,
                                uint32_t entry_offset, const char **imported_funcs, size_t num_imported_funcs,
                                const uint32_t *reloc_rvas, size_t num_reloc_rvas) {
    return zcc_emit_win64_pe_file_v4(filename, code_bytes, code_len, entry_offset,
                                     imported_funcs, num_imported_funcs,
                                     reloc_rvas, num_reloc_rvas, NULL, 0);
}

int zcc_emit_win64_pe_file_with_imports(const char *filename, const uint8_t *code_bytes, size_t code_len,
                                        uint32_t entry_offset, const char **imported_funcs, size_t num_imported_funcs) {
    return zcc_emit_win64_pe_file_v4(filename, code_bytes, code_len, entry_offset,
                                     imported_funcs, num_imported_funcs,
                                     NULL, 0, NULL, 0);
}



/* Standard PE Export Directory (40 bytes) */
typedef struct {
    uint32_t Characteristics;
    uint32_t TimeDateStamp;
    uint16_t MajorVersion;
    uint16_t MinorVersion;
    uint32_t Name;
    uint32_t Base;
    uint32_t NumberOfFunctions;
    uint32_t NumberOfNames;
    uint32_t AddressOfFunctions;
    uint32_t AddressOfNames;
    uint32_t AddressOfNameOrdinals;
} IMAGE_EXPORT_DIRECTORY;

static uint8_t *win64_pe_build_edata(const char *dll_name,
                                     const win64_pe_export_t *exports, size_t num_exports,
                                     uint32_t edata_rva, uint32_t *out_total_size) {
    if (!exports || num_exports == 0) {
        if (out_total_size) *out_total_size = 0;
        return NULL;
    }
    const char *dname = (dll_name && dll_name[0]) ? dll_name : "library.dll";
    uint32_t dname_len = (uint32_t)strlen(dname) + 1;

    uint32_t eat_offset = sizeof(IMAGE_EXPORT_DIRECTORY);
    uint32_t ent_offset = eat_offset + (uint32_t)(num_exports * 4);
    uint32_t eot_offset = ent_offset + (uint32_t)(num_exports * 4);
    uint32_t strings_offset = eot_offset + (uint32_t)(num_exports * 2);

    uint32_t str_cursor = strings_offset;
    uint32_t dname_rva_offset = str_cursor;
    str_cursor += dname_len;

    uint32_t *name_rva_offsets = (uint32_t *)malloc(sizeof(uint32_t) * num_exports);
    size_t i;
    for (i = 0; i < num_exports; i++) {
        name_rva_offsets[i] = str_cursor;
        str_cursor += (uint32_t)(strlen(exports[i].name) + 1);
    }

    uint32_t total_size = win64_pe_align_to(str_cursor, 4);
    uint8_t *buf = (uint8_t *)calloc(1, total_size);
    if (!buf) {
        free(name_rva_offsets);
        if (out_total_size) *out_total_size = 0;
        return NULL;
    }

    IMAGE_EXPORT_DIRECTORY *exp = (IMAGE_EXPORT_DIRECTORY *)buf;
    exp->TimeDateStamp = 0x66900000;
    exp->Name = edata_rva + dname_rva_offset;
    exp->Base = 1;
    exp->NumberOfFunctions = (uint32_t)num_exports;
    exp->NumberOfNames = (uint32_t)num_exports;
    exp->AddressOfFunctions = edata_rva + eat_offset;
    exp->AddressOfNames = edata_rva + ent_offset;
    exp->AddressOfNameOrdinals = edata_rva + eot_offset;

    uint32_t *eat = (uint32_t *)(buf + eat_offset);
    uint32_t *ent = (uint32_t *)(buf + ent_offset);
    uint16_t *eot = (uint16_t *)(buf + eot_offset);

    for (i = 0; i < num_exports; i++) {
        eat[i] = exports[i].func_rva;
        ent[i] = edata_rva + name_rva_offsets[i];
        eot[i] = (uint16_t)i;
    }

    memcpy(buf + dname_rva_offset, dname, dname_len);
    for (i = 0; i < num_exports; i++) {
        const char *s = exports[i].name;
        memcpy(buf + name_rva_offsets[i], s, strlen(s) + 1);
    }

    free(name_rva_offsets);
    if (out_total_size) *out_total_size = total_size;
    return buf;
}

int zcc_emit_win64_pe_dll(const char *filename, const char *dll_name,
                          const uint8_t *code_bytes, size_t code_len,
                          uint32_t entry_offset,
                          const char **imported_funcs, size_t num_imported_funcs,
                          const win64_pe_export_t *exports, size_t num_exports,
                          const uint32_t *reloc_rvas, size_t num_reloc_rvas,
                          const win64_pe_func_extent_t *extents, size_t num_extents) {
    if (!filename) return -1;

    Win64PeDllGroup groups[16];
    size_t num_groups = 0;
    if (imported_funcs && num_imported_funcs > 0) {
        num_groups = win64_pe_build_groups(imported_funcs, num_imported_funcs, groups, 16);
    }

    uint32_t total_reloc_size = 0;
    uint8_t *reloc_bytes = win64_pe_build_relocs(reloc_rvas, num_reloc_rvas, &total_reloc_size);
    int has_reloc = (reloc_bytes != NULL && total_reloc_size > 0);
    uint32_t aligned_reloc_size = has_reloc ? win64_pe_align_to(total_reloc_size, 0x0200) : 0;

    int has_idata = (num_groups > 0);
    int has_pdata = 1;

    uint32_t raw_code_len = (code_bytes && code_len > 0) ? (uint32_t)code_len : 16;
    uint32_t aligned_code_size = win64_pe_align_to(raw_code_len, 0x0200);

    /* Sizing sections */
    uint32_t edata_rva = 0x1000 + win64_pe_align_to(raw_code_len, 0x1000);
    uint32_t total_edata_size = 0;
    uint8_t *edata_bytes = win64_pe_build_edata(dll_name, exports, num_exports, edata_rva, &total_edata_size);
    int has_edata = (edata_bytes != NULL && total_edata_size > 0);
    uint32_t aligned_edata_size = has_edata ? win64_pe_align_to(total_edata_size, 0x0200) : 0;

    uint32_t idata_rva = has_edata ? (edata_rva + win64_pe_align_to(total_edata_size, 0x1000)) : edata_rva;

    FILE *f = fopen(filename, "wb");
    if (!f) {
        if (reloc_bytes) free(reloc_bytes);
        if (edata_bytes) free(edata_bytes);
        return -1;
    }

    /* DOS Header */
    IMAGE_DOS_HEADER dos_hdr;
    memset(&dos_hdr, 0, sizeof(dos_hdr));
    dos_hdr.e_magic = 0x5A4D;
    dos_hdr.e_cblp = 0x0090;
    dos_hdr.e_cp = 0x0003;
    dos_hdr.e_cparhdr = 0x0004;
    dos_hdr.e_maxalloc = 0xFFFF;
    dos_hdr.e_sp = 0x00B8;
    dos_hdr.e_lfarlc = 0x0040;
    dos_hdr.e_lfanew = 0x0080;

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

    uint32_t pe_sig = 0x00004550;

    uint16_t num_sections = 2; /* .text and .data */
    if (has_edata) num_sections++;
    if (has_idata) num_sections++;
    if (has_pdata) num_sections++;
    if (has_reloc) num_sections++;

    IMAGE_FILE_HEADER file_hdr;
    memset(&file_hdr, 0, sizeof(file_hdr));
    file_hdr.Machine = 0x8664;
    file_hdr.NumberOfSections = num_sections;
    file_hdr.TimeDateStamp = 0x66900000;
    file_hdr.SizeOfOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER64);
    file_hdr.Characteristics = 0x2022; /* IMAGE_FILE_DLL | EXECUTABLE_IMAGE | LARGE_ADDRESS_AWARE */

    /* Import Table Construction */
    uint32_t total_iat_size = 0;
    uint32_t desc_offset = 0;
    uint32_t desc_size = 0;
    uint32_t total_idata_size = 0;
    uint8_t *idata_bytes = NULL;
    uint32_t *hint_name_offsets = NULL;

    if (has_idata) {
        uint32_t cur_iat = 0;
        size_t g, j;
        for (g = 0; g < num_groups; g++) {
            groups[g].iat_offset = cur_iat;
            cur_iat += (uint32_t)((groups[g].num_funcs + 1) * 8);
        }
        total_iat_size = cur_iat;
        uint32_t cur_ilt = cur_iat;
        for (g = 0; g < num_groups; g++) {
            groups[g].ilt_offset = cur_ilt;
            cur_ilt += (uint32_t)((groups[g].num_funcs + 1) * 8);
        }
        desc_offset = cur_ilt;
        desc_size = (uint32_t)((num_groups + 1) * sizeof(IMAGE_IMPORT_DESCRIPTOR));
        uint32_t str_offset = desc_offset + desc_size;
        for (g = 0; g < num_groups; g++) {
            groups[g].name_offset = str_offset;
            str_offset += (uint32_t)(strlen(groups[g].dll_name) + 1);
        }
        hint_name_offsets = (uint32_t *)malloc(sizeof(uint32_t) * num_imported_funcs);
        for (g = 0; g < num_groups; g++) {
            for (j = 0; j < groups[g].num_funcs; j++) {
                size_t orig_idx = groups[g].func_indices[j];
                str_offset = (str_offset + 1) & ~1;
                hint_name_offsets[orig_idx] = str_offset;
                str_offset += (uint32_t)(2 + strlen(imported_funcs[orig_idx]) + 1);
            }
        }
        total_idata_size = str_offset;
        idata_bytes = (uint8_t *)calloc(1, total_idata_size);
        if (!idata_bytes) {
            free(hint_name_offsets);
            if (reloc_bytes) free(reloc_bytes);
            if (edata_bytes) free(edata_bytes);
            fclose(f);
            return -1;
        }

        for (g = 0; g < num_groups; g++) {
            memcpy(idata_bytes + groups[g].name_offset, groups[g].dll_name, strlen(groups[g].dll_name) + 1);
        }
        for (g = 0; g < num_groups; g++) {
            for (j = 0; j < groups[g].num_funcs; j++) {
                size_t orig_idx = groups[g].func_indices[j];
                uint32_t off = hint_name_offsets[orig_idx];
                strcpy((char *)(idata_bytes + off + 2), imported_funcs[orig_idx]);
            }
        }
        for (g = 0; g < num_groups; g++) {
            for (j = 0; j < groups[g].num_funcs; j++) {
                size_t orig_idx = groups[g].func_indices[j];
                uint64_t target_rva = (uint64_t)(idata_rva + hint_name_offsets[orig_idx]);
                memcpy(idata_bytes + groups[g].iat_offset + j * 8, &target_rva, sizeof(uint64_t));
                memcpy(idata_bytes + groups[g].ilt_offset + j * 8, &target_rva, sizeof(uint64_t));
            }
        }
        for (g = 0; g < num_groups; g++) {
            IMAGE_IMPORT_DESCRIPTOR desc;
            memset(&desc, 0, sizeof(desc));
            desc.OriginalFirstThunk = idata_rva + groups[g].ilt_offset;
            desc.Name = idata_rva + groups[g].name_offset;
            desc.FirstThunk = idata_rva + groups[g].iat_offset;
            memcpy(idata_bytes + groups[g].desc_offset, &desc, sizeof(desc));
        }
    }

    uint32_t aligned_idata_size = has_idata ? win64_pe_align_to(total_idata_size, 0x0200) : 0;
    uint32_t pdata_rva = has_idata ? (idata_rva + win64_pe_align_to(total_idata_size, 0x1000)) : idata_rva;

    uint32_t total_pdata_size = 0;
    uint32_t pdata_entries_size = 0;
    uint8_t *pdata_bytes = win64_pe_build_pdata(0x1000, raw_code_len, entry_offset,
                                               extents, num_extents,
                                               pdata_rva, &total_pdata_size, &pdata_entries_size);
    uint32_t aligned_pdata_size = win64_pe_align_to(total_pdata_size, 0x0200);
    uint32_t data_rva = pdata_rva + win64_pe_align_to(total_pdata_size, 0x1000);
    uint32_t reloc_rva = data_rva + 0x1000;

    uint32_t headers_size = sizeof(IMAGE_DOS_HEADER) + sizeof(dos_stub) + sizeof(pe_sig) +
                            sizeof(IMAGE_FILE_HEADER) + sizeof(IMAGE_OPTIONAL_HEADER64) +
                            (num_sections * sizeof(IMAGE_SECTION_HEADER));
    uint32_t aligned_headers_size = win64_pe_align_to(headers_size, 0x0200);

    IMAGE_OPTIONAL_HEADER64 opt_hdr;
    memset(&opt_hdr, 0, sizeof(opt_hdr));
    opt_hdr.Magic = 0x020B; /* PE32+ */
    opt_hdr.MajorLinkerVersion = 14;
    opt_hdr.MinorLinkerVersion = 0;
    opt_hdr.SizeOfCode = aligned_code_size;
    opt_hdr.SizeOfInitializedData = aligned_edata_size + aligned_idata_size + aligned_pdata_size + 0x0200 + aligned_reloc_size;
    opt_hdr.AddressOfEntryPoint = (entry_offset > 0) ? (0x1000 + entry_offset) : 0;
    opt_hdr.BaseOfCode = 0x1000;
    opt_hdr.ImageBase = 0x180000000ULL; /* Default DLL base */
    opt_hdr.SectionAlignment = 0x1000;
    opt_hdr.FileAlignment = 0x0200;
    opt_hdr.MajorOperatingSystemVersion = 6;
    opt_hdr.MinorOperatingSystemVersion = 0;
    opt_hdr.MajorSubsystemVersion = 6;
    opt_hdr.MinorSubsystemVersion = 0;
    opt_hdr.SizeOfImage = has_reloc ? (reloc_rva + win64_pe_align_to(total_reloc_size, 0x1000)) : (data_rva + 0x1000);
    opt_hdr.SizeOfHeaders = aligned_headers_size;
    opt_hdr.Subsystem = 3; /* Console or default */
    opt_hdr.DllCharacteristics = 0x8160; /* HIGH_ENTROPY_VA | DYNAMIC_BASE | NX_COMPAT */
    opt_hdr.SizeOfStackReserve = 0x100000;
    opt_hdr.SizeOfStackCommit = 0x1000;
    opt_hdr.SizeOfHeapReserve = 0x100000;
    opt_hdr.SizeOfHeapCommit = 0x1000;
    opt_hdr.NumberOfRvaAndSizes = 16;

    if (has_edata) {
        opt_hdr.DataDirectory[0].VirtualAddress = edata_rva;
        opt_hdr.DataDirectory[0].Size = total_edata_size;
    }
    if (has_idata) {
        opt_hdr.DataDirectory[1].VirtualAddress = idata_rva + desc_offset;
        opt_hdr.DataDirectory[1].Size = desc_size;
        opt_hdr.DataDirectory[12].VirtualAddress = idata_rva;
        opt_hdr.DataDirectory[12].Size = total_iat_size;
    }
    if (has_pdata && pdata_bytes) {
        opt_hdr.DataDirectory[3].VirtualAddress = pdata_rva;
        opt_hdr.DataDirectory[3].Size = pdata_entries_size;
    }
    if (has_reloc) {
        opt_hdr.DataDirectory[5].VirtualAddress = reloc_rva;
        opt_hdr.DataDirectory[5].Size = total_reloc_size;
    }

    /* Section Headers */
    IMAGE_SECTION_HEADER sec_text;
    memset(&sec_text, 0, sizeof(sec_text));
    memcpy(sec_text.Name, ".text\0\0\0", 8);
    sec_text.VirtualSize = raw_code_len;
    sec_text.VirtualAddress = 0x1000;
    sec_text.SizeOfRawData = aligned_code_size;
    sec_text.PointerToRawData = aligned_headers_size;
    sec_text.Characteristics = 0x60000020; /* CODE | EXEC | READ */

    IMAGE_SECTION_HEADER sec_edata;
    memset(&sec_edata, 0, sizeof(sec_edata));
    if (has_edata) {
        memcpy(sec_edata.Name, ".edata\0\0", 8);
        sec_edata.VirtualSize = total_edata_size;
        sec_edata.VirtualAddress = edata_rva;
        sec_edata.SizeOfRawData = aligned_edata_size;
        sec_edata.PointerToRawData = aligned_headers_size + aligned_code_size;
        sec_edata.Characteristics = 0x40000040; /* INITIALIZED_DATA | READ */
    }

    IMAGE_SECTION_HEADER sec_idata;
    memset(&sec_idata, 0, sizeof(sec_idata));
    if (has_idata) {
        memcpy(sec_idata.Name, ".idata\0\0", 8);
        sec_idata.VirtualSize = total_idata_size;
        sec_idata.VirtualAddress = idata_rva;
        sec_idata.SizeOfRawData = aligned_idata_size;
        sec_idata.PointerToRawData = aligned_headers_size + aligned_code_size + aligned_edata_size;
        sec_idata.Characteristics = 0xC0000040;
    }

    IMAGE_SECTION_HEADER sec_pdata;
    memset(&sec_pdata, 0, sizeof(sec_pdata));
    if (has_pdata) {
        memcpy(sec_pdata.Name, ".pdata\0\0", 8);
        sec_pdata.VirtualSize = total_pdata_size;
        sec_pdata.VirtualAddress = pdata_rva;
        sec_pdata.SizeOfRawData = aligned_pdata_size;
        sec_pdata.PointerToRawData = aligned_headers_size + aligned_code_size + aligned_edata_size + aligned_idata_size;
        sec_pdata.Characteristics = 0x40000040;
    }

    IMAGE_SECTION_HEADER sec_data;
    memset(&sec_data, 0, sizeof(sec_data));
    memcpy(sec_data.Name, ".data\0\0\0", 8);
    sec_data.VirtualSize = 16;
    sec_data.VirtualAddress = data_rva;
    sec_data.SizeOfRawData = 0x0200;
    sec_data.PointerToRawData = aligned_headers_size + aligned_code_size + aligned_edata_size + aligned_idata_size + aligned_pdata_size;
    sec_data.Characteristics = 0xC0000040;

    IMAGE_SECTION_HEADER sec_reloc;
    memset(&sec_reloc, 0, sizeof(sec_reloc));
    if (has_reloc) {
        memcpy(sec_reloc.Name, ".reloc\0\0", 8);
        sec_reloc.VirtualSize = total_reloc_size;
        sec_reloc.VirtualAddress = reloc_rva;
        sec_reloc.SizeOfRawData = aligned_reloc_size;
        sec_reloc.PointerToRawData = aligned_headers_size + aligned_code_size + aligned_edata_size + aligned_idata_size + aligned_pdata_size + 0x0200;
        sec_reloc.Characteristics = 0x42000040;
    }

    /* Write Headers */
    fwrite(&dos_hdr, sizeof(dos_hdr), 1, f);
    fwrite(dos_stub, sizeof(dos_stub), 1, f);
    fwrite(&pe_sig, sizeof(pe_sig), 1, f);
    fwrite(&file_hdr, sizeof(file_hdr), 1, f);
    fwrite(&opt_hdr, sizeof(opt_hdr), 1, f);
    fwrite(&sec_text, sizeof(sec_text), 1, f);
    if (has_edata) fwrite(&sec_edata, sizeof(sec_edata), 1, f);
    if (has_idata) fwrite(&sec_idata, sizeof(sec_idata), 1, f);
    if (has_pdata) fwrite(&sec_pdata, sizeof(sec_pdata), 1, f);
    fwrite(&sec_data, sizeof(sec_data), 1, f);
    if (has_reloc) fwrite(&sec_reloc, sizeof(sec_reloc), 1, f);

    size_t written_headers = sizeof(dos_hdr) + sizeof(dos_stub) + sizeof(pe_sig) +
                             sizeof(file_hdr) + sizeof(opt_hdr) + (num_sections * sizeof(IMAGE_SECTION_HEADER));
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
    }

    /* Write .edata */
    if (has_edata && edata_bytes) {
        fwrite(edata_bytes, 1, total_edata_size, f);
        if (aligned_edata_size > total_edata_size) {
            size_t pad_edata = aligned_edata_size - total_edata_size;
            uint8_t *pad = (uint8_t *)calloc(1, pad_edata);
            fwrite(pad, 1, pad_edata, f);
            free(pad);
        }
    }

    /* Write .idata */
    if (has_idata && idata_bytes) {
        fwrite(idata_bytes, 1, total_idata_size, f);
        if (aligned_idata_size > total_idata_size) {
            size_t pad_idata = aligned_idata_size - total_idata_size;
            uint8_t *pad = (uint8_t *)calloc(1, pad_idata);
            fwrite(pad, 1, pad_idata, f);
            free(pad);
        }
    }

    /* Write .pdata */
    if (has_pdata && pdata_bytes) {
        fwrite(pdata_bytes, 1, total_pdata_size, f);
        if (aligned_pdata_size > total_pdata_size) {
            size_t pad_pdata = aligned_pdata_size - total_pdata_size;
            uint8_t *pad = (uint8_t *)calloc(1, pad_pdata);
            fwrite(pad, 1, pad_pdata, f);
            free(pad);
        }
    }

    /* Write .data */
    uint8_t data_pad[0x0200] = {0};
    fwrite(data_pad, 1, 0x0200, f);

    /* Write .reloc */
    if (has_reloc && reloc_bytes) {
        fwrite(reloc_bytes, 1, total_reloc_size, f);
        if (aligned_reloc_size > total_reloc_size) {
            size_t pad_reloc = aligned_reloc_size - total_reloc_size;
            uint8_t *pad = (uint8_t *)calloc(1, pad_reloc);
            fwrite(pad, 1, pad_reloc, f);
            free(pad);
        }
    }

    if (hint_name_offsets) free(hint_name_offsets);
    if (idata_bytes) free(idata_bytes);
    if (pdata_bytes) free(pdata_bytes);
    if (reloc_bytes) free(reloc_bytes);
    if (edata_bytes) free(edata_bytes);
    fclose(f);
    return 0;
}
