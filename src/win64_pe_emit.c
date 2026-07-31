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

int zcc_emit_win64_pe_file(const char *filename, const uint8_t *code_bytes, size_t code_len) {
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
    opt_hdr.AddressOfEntryPoint = 0x1000; /* RVA of .text */
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
    sec_text.Characteristics = 0x60000020; /* CODE | EXECUTE | READ */

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
