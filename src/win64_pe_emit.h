/*
 * ZCC Win64 PE/COFF Direct Binary Emitter
 * Header File: src/win64_pe_emit.h
 * Target: Windows 64-bit Executable (PE32+ / AMD64 COFF)
 */

#ifndef ZCC_WIN64_PE_EMIT_H
#define ZCC_WIN64_PE_EMIT_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#pragma pack(push, 1)

/* DOS Stub Header (64 bytes) */
typedef struct {
    uint16_t e_magic;    /* Magic number: 0x5A4D ('MZ') */
    uint16_t e_cblp;     /* Bytes on last page of file */
    uint16_t e_cp;       /* Pages in file */
    uint16_t e_crlc;     /* Relocations */
    uint16_t e_cparhdr;  /* Size of header in paragraphs */
    uint16_t e_minalloc; /* Minimum extra paragraphs needed */
    uint16_t e_maxalloc; /* Maximum extra paragraphs needed */
    uint16_t e_ss;       /* Initial (relative) SS value */
    uint16_t e_sp;       /* Initial SP value */
    uint16_t e_csum;     /* Checksum */
    uint16_t e_ip;       /* Initial IP value */
    uint16_t e_cs;       /* Initial (relative) CS value */
    uint16_t e_lfarlc;   /* File address of relocation table */
    uint16_t e_ovno;     /* Overlay number */
    uint16_t e_res[4];   /* Reserved words */
    uint16_t e_oemid;    /* OEM identifier */
    uint16_t e_oeminfo;  /* OEM information */
    uint16_t e_res2[10]; /* Reserved words */
    uint32_t e_lfanew;   /* File address of PE header (offset 0x3C) */
} IMAGE_DOS_HEADER;

/* COFF File Header (20 bytes) */
typedef struct {
    uint16_t Machine;              /* 0x8664 (AMD64 / x86-64) */
    uint16_t NumberOfSections;     /* Number of section headers */
    uint32_t TimeDateStamp;        /* Timestamp */
    uint32_t PointerToSymbolTable; /* 0 if no COFF symbols */
    uint32_t NumberOfSymbols;      /* 0 if no COFF symbols */
    uint16_t SizeOfOptionalHeader; /* Size of IMAGE_OPTIONAL_HEADER64 */
    uint16_t Characteristics;      /* 0x0022 (Executable | 64-bit aware) */
} IMAGE_FILE_HEADER;

/* Data Directory Entry (8 bytes) */
typedef struct {
    uint32_t VirtualAddress;
    uint32_t Size;
} IMAGE_DATA_DIRECTORY;

/* PE32+ Optional Header for 64-bit (112 bytes base + 16 data directories = 240 bytes) */
typedef struct {
    uint16_t Magic;                 /* 0x020B (PE32+ / 64-bit) */
    uint8_t  MajorLinkerVersion;    /* Linker version */
    uint8_t  MinorLinkerVersion;
    uint32_t SizeOfCode;            /* Size of .text section */
    uint32_t SizeOfInitializedData; /* Size of .data section */
    uint32_t SizeOfUninitializedData;
    uint32_t AddressOfEntryPoint;   /* RVA of entry point */
    uint32_t BaseOfCode;            /* RVA of code section */
    uint64_t ImageBase;             /* Preferred default base: 0x140000000 */
    uint32_t SectionAlignment;      /* Default: 0x1000 (4KB) */
    uint32_t FileAlignment;         /* Default: 0x0200 (512 bytes) */
    uint16_t MajorOperatingSystemVersion;
    uint16_t MinorOperatingSystemVersion;
    uint16_t MajorImageVersion;
    uint16_t MinorImageVersion;
    uint16_t MajorSubsystemVersion; /* Default: 6 */
    uint16_t MinorSubsystemVersion; /* Default: 0 */
    uint32_t Win32VersionValue;
    uint32_t SizeOfImage;           /* Total size in memory, aligned */
    uint32_t SizeOfHeaders;         /* Total size of DOS + PE + Section headers */
    uint32_t CheckSum;
    uint16_t Subsystem;             /* 3 = IMAGE_SUBSYSTEM_WINDOWS_CUI (Console) */
    uint16_t DllCharacteristics;    /* 0x8160 (NX_COMPAT | TERMINAL_SERVER_AWARE) */
    uint64_t SizeOfStackReserve;    /* Default: 0x100000 (1MB) */
    uint64_t SizeOfStackCommit;     /* Default: 0x1000 (4KB) */
    uint64_t SizeOfHeapReserve;     /* Default: 0x100000 (1MB) */
    uint64_t SizeOfHeapCommit;      /* Default: 0x1000 (4KB) */
    uint32_t LoaderFlags;
    uint32_t NumberOfRvaAndSizes;   /* Default: 16 */
    IMAGE_DATA_DIRECTORY DataDirectory[16];
} IMAGE_OPTIONAL_HEADER64;

/* Section Header (40 bytes) */
typedef struct {
    uint8_t  Name[8];               /* Name e.g. ".text\0\0\0" */
    uint32_t VirtualSize;           /* Size in memory */
    uint32_t VirtualAddress;        /* RVA in memory */
    uint32_t SizeOfRawData;         /* Size in file, aligned to FileAlignment */
    uint32_t PointerToRawData;      /* File offset */
    uint32_t PointerToRelocations;
    uint32_t PointerToLinenumbers;
    uint16_t NumberOfRelocations;
    uint16_t NumberOfLinenumbers;
    uint32_t Characteristics;       /* Section flags (Code / Read / Execute) */
} IMAGE_SECTION_HEADER;

#pragma pack(pop)

/* Function Declarations */
uint32_t win64_pe_align_to(uint32_t val, uint32_t align);
int zcc_emit_win64_pe_file(const char *filename, const uint8_t *code_bytes, size_t code_len);

#endif /* ZCC_WIN64_PE_EMIT_H */
