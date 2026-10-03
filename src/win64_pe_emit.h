/*
 * ZCC Win64 PE/COFF Direct Binary Emitter
 * Header File: src/win64_pe_emit.h
 * Target: Windows 64-bit Executable (PE32+ / AMD64 COFF)
 */

#ifndef ZCC_WIN64_PE_EMIT_H
#define ZCC_WIN64_PE_EMIT_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
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

/* PE32+ Optional Header for 64-bit (112 bytes base + 16 data directories = 240
 * bytes) */
typedef struct {
  uint16_t Magic;             /* 0x020B (PE32+ / 64-bit) */
  uint8_t MajorLinkerVersion; /* Linker version */
  uint8_t MinorLinkerVersion;
  uint32_t SizeOfCode;            /* Size of .text section */
  uint32_t SizeOfInitializedData; /* Size of .data section */
  uint32_t SizeOfUninitializedData;
  uint32_t AddressOfEntryPoint; /* RVA of entry point */
  uint32_t BaseOfCode;          /* RVA of code section */
  uint64_t ImageBase;           /* Preferred default base: 0x140000000 */
  uint32_t SectionAlignment;    /* Default: 0x1000 (4KB) */
  uint32_t FileAlignment;       /* Default: 0x0200 (512 bytes) */
  uint16_t MajorOperatingSystemVersion;
  uint16_t MinorOperatingSystemVersion;
  uint16_t MajorImageVersion;
  uint16_t MinorImageVersion;
  uint16_t MajorSubsystemVersion; /* Default: 6 */
  uint16_t MinorSubsystemVersion; /* Default: 0 */
  uint32_t Win32VersionValue;
  uint32_t SizeOfImage;   /* Total size in memory, aligned */
  uint32_t SizeOfHeaders; /* Total size of DOS + PE + Section headers */
  uint32_t CheckSum;
  uint16_t Subsystem;          /* 3 = IMAGE_SUBSYSTEM_WINDOWS_CUI (Console) */
  uint16_t DllCharacteristics; /* 0x8160 (NX_COMPAT | TERMINAL_SERVER_AWARE) */
  uint64_t SizeOfStackReserve; /* Default: 0x100000 (1MB) */
  uint64_t SizeOfStackCommit;  /* Default: 0x1000 (4KB) */
  uint64_t SizeOfHeapReserve;  /* Default: 0x100000 (1MB) */
  uint64_t SizeOfHeapCommit;   /* Default: 0x1000 (4KB) */
  uint32_t LoaderFlags;
  uint32_t NumberOfRvaAndSizes; /* Default: 16 */
  IMAGE_DATA_DIRECTORY DataDirectory[16];
} IMAGE_OPTIONAL_HEADER64;

/* Section Header (40 bytes) */
typedef struct {
  uint8_t Name[8];           /* Name e.g. ".text\0\0\0" */
  uint32_t VirtualSize;      /* Size in memory */
  uint32_t VirtualAddress;   /* RVA in memory */
  uint32_t SizeOfRawData;    /* Size in file, aligned to FileAlignment */
  uint32_t PointerToRawData; /* File offset */
  uint32_t PointerToRelocations;
  uint32_t PointerToLinenumbers;
  uint16_t NumberOfRelocations;
  uint16_t NumberOfLinenumbers;
  uint32_t Characteristics; /* Section flags (Code / Read / Execute) */
} IMAGE_SECTION_HEADER;

/* Import Directory Descriptor (20 bytes) */
typedef struct {
  uint32_t OriginalFirstThunk; /* RVA of Import Lookup Table (ILT) */
  uint32_t TimeDateStamp;      /* 0 */
  uint32_t ForwarderChain;     /* 0 */
  uint32_t Name;               /* RVA of DLL name string */
  uint32_t FirstThunk;         /* RVA of Import Address Table (IAT) */
} IMAGE_IMPORT_DESCRIPTOR;

/* Base Relocation Block Header (8 bytes) */
typedef struct {
  uint32_t VirtualAddress;     /* Page RVA (e.g. 0x1000) */
  uint32_t SizeOfBlock;        /* Block size in bytes including header & entries */
} IMAGE_BASE_RELOCATION;

/* Runtime Function Entry / Exception Directory Entry for x64 SEH (.pdata, 12 bytes) */
typedef struct {
  uint32_t BeginAddress;       /* Function start RVA */
  uint32_t EndAddress;         /* Function end RVA */
  uint32_t UnwindInfoAddress;  /* Unwind info RVA */
} IMAGE_RUNTIME_FUNCTION_ENTRY;

/* Unwind Info Header (.xdata, 4 bytes base + codes) */
typedef struct {
  uint8_t VersionAndFlags;     /* Version (bits 0-2 = 1), Flags (bits 3-7) */
  uint8_t SizeOfProlog;        /* Length of prolog in bytes */
  uint8_t CountOfCodes;        /* Number of UNWIND_CODE entries */
  uint8_t FrameRegisterAndOffset; /* Frame reg (bits 0-3), Offset (bits 4-7) */
} IMAGE_UNWIND_INFO;

/* Function extent definition for caller-provided function bounds */
typedef struct {
  uint32_t begin_offset;       /* Offset from start of .text */
  uint32_t end_offset;         /* Offset from start of .text */
} win64_pe_func_extent_t;

#pragma pack(pop)

/* Function Declarations */
uint32_t win64_pe_align_to(uint32_t val, uint32_t align);
const char *zcc_win64_pe_resolve_dll_for_symbol(const char *sym);
int zcc_emit_win64_pe_file(const char *filename, const uint8_t *code_bytes,
                           size_t code_len);
int zcc_emit_win64_pe_file_ex(const char *filename, const uint8_t *code_bytes,
                              size_t code_len, uint32_t entry_offset);
void zcc_win64_pe_calc_iat_rvas(size_t code_len, const char **imported_funcs,
                                size_t num_imported_funcs,
                                uint32_t *out_iat_rvas);
int zcc_emit_win64_pe_file_with_imports(const char *filename,
                                        const uint8_t *code_bytes,
                                        size_t code_len, uint32_t entry_offset,
                                        const char **imported_funcs,
                                        size_t num_imported_funcs);
int zcc_emit_win64_pe_file_full(const char *filename, const uint8_t *code_bytes,
                                size_t code_len, uint32_t entry_offset,
                                const char **imported_funcs, size_t num_imported_funcs,
                                const uint32_t *reloc_rvas, size_t num_reloc_rvas);
int zcc_emit_win64_pe_file_v4(const char *filename, const uint8_t *code_bytes,
                              size_t code_len, uint32_t entry_offset,
                              const char **imported_funcs, size_t num_imported_funcs,
                              const uint32_t *reloc_rvas, size_t num_reloc_rvas,
                              const win64_pe_func_extent_t *extents, size_t num_extents);
int zcc_emit_win64_pe_file_v5(const char *filename, const uint8_t *code_bytes,
                              size_t code_len, uint32_t entry_offset,
                              const char **imported_funcs, size_t num_imported_funcs,
                              const uint32_t *reloc_rvas, size_t num_reloc_rvas,
                              const win64_pe_func_extent_t *extents, size_t num_extents,
                              uint16_t subsystem,
                              const uint8_t *rsrc_bytes, size_t rsrc_len);
uint8_t *win64_pe_build_default_manifest_rsrc(const char *app_name, uint32_t rsrc_rva, uint32_t *out_size);

typedef struct {
    const char *name;
    uint32_t func_rva;
} win64_pe_export_t;

int zcc_emit_win64_pe_dll(const char *filename, const char *dll_name,
                          const uint8_t *code_bytes, size_t code_len,
                          uint32_t entry_offset,
                          const char **imported_funcs, size_t num_imported_funcs,
                          const win64_pe_export_t *exports, size_t num_exports,
                          const uint32_t *reloc_rvas, size_t num_reloc_rvas,
                          const win64_pe_func_extent_t *extents, size_t num_extents);


#endif /* ZCC_WIN64_PE_EMIT_H */
