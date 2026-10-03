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

#define WIN64_UNW_FLAG_NHANDLER  0x00
#define WIN64_UNW_FLAG_EHANDLER  0x01
#define WIN64_UNW_FLAG_UHANDLER  0x02
#define WIN64_UNW_FLAG_CHAININFO 0x04

typedef struct {
  uint32_t try_begin_offset;
  uint32_t try_end_offset;
  uint32_t handler_offset;    /* 1 for EXCEPTION_EXECUTE_HANDLER, or offset of filter func */
  uint32_t target_offset;     /* Offset of catch / except block */
} win64_pe_seh_scope_t;

/* Function extent definition for caller-provided function bounds */
typedef struct {
  uint32_t begin_offset;       /* Offset from start of .text */
  uint32_t end_offset;         /* Offset from start of .text */
  uint8_t  flags;              /* WIN64_UNW_FLAG_* */
  uint32_t handler_rva;        /* RVA of exception personality handler */
  size_t   num_scopes;
  win64_pe_seh_scope_t scopes[4];
} win64_pe_func_extent_t;

/* TLS Directory Descriptor for x64 (40 bytes) */
typedef struct {
  uint64_t StartAddressOfRawData; /* VA of start of TLS template */
  uint64_t EndAddressOfRawData;   /* VA of end of TLS template */
  uint64_t AddressOfIndex;        /* VA of TLS index variable (uint32_t) */
  uint64_t AddressOfCallBacks;    /* VA of null-terminated array of callback function pointers */
  uint32_t SizeOfZeroFill;        /* Size in bytes of zero-fill template */
  uint32_t Characteristics;       /* Reserved (0) */
} IMAGE_TLS_DIRECTORY64;

typedef struct {
  const uint8_t *raw_data;        /* Template initialized data */
  size_t         raw_data_size;   /* Size of template data */
  uint32_t       zero_fill_size;  /* Uninitialized zero-fill size (.tbss) */
  const uint32_t *callback_rvas;  /* Optional callback function RVAs */
  size_t         num_callbacks;   /* Number of callbacks */
} win64_pe_tls_info_t;

/* Attribute Certificate Table (WIN_CERTIFICATE) for IMAGE_DIRECTORY_ENTRY_SECURITY */
#define IMAGE_DIRECTORY_ENTRY_SECURITY 4
#define WIN_CERT_REVISION_1_0          0x0100
#define WIN_CERT_REVISION_2_0          0x0200
#define WIN_CERT_TYPE_X509             0x0001
#define WIN_CERT_TYPE_PKCS_SIGNED_DATA 0x0002
#define WIN_CERT_TYPE_RESERVED_1       0x0003
#define WIN_CERT_TYPE_TS_STACK_SIGNED  0x0004

typedef struct {
  uint32_t dwLength;         /* Total length of entry including header and padding, 8-byte aligned */
  uint16_t wRevision;        /* WIN_CERT_REVISION_2_0 (0x0200) */
  uint16_t wCertificateType; /* WIN_CERT_TYPE_PKCS_SIGNED_DATA (0x0002) */
} WIN_CERTIFICATE;

typedef struct {
  const uint8_t *cert_data;  /* Certificate / Authenticode payload */
  size_t         cert_len;   /* Length of payload in bytes */
  uint16_t       revision;   /* 0x0200 default */
  uint16_t       cert_type;  /* 0x0002 default */
} win64_pe_cert_info_t;

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
int zcc_emit_win64_pe_file_v6(const char *filename, const uint8_t *code_bytes,
                              size_t code_len, uint32_t entry_offset,
                              const char **imported_funcs, size_t num_imported_funcs,
                              const uint32_t *reloc_rvas, size_t num_reloc_rvas,
                              const win64_pe_func_extent_t *extents, size_t num_extents,
                              uint16_t subsystem,
                              const uint8_t *rsrc_bytes, size_t rsrc_len,
                              const win64_pe_tls_info_t *tls_info);
int zcc_emit_win64_pe_file_v7(const char *filename, const uint8_t *code_bytes,
                              size_t code_len, uint32_t entry_offset,
                              const char **imported_funcs, size_t num_imported_funcs,
                              const uint32_t *reloc_rvas, size_t num_reloc_rvas,
                              const win64_pe_func_extent_t *extents, size_t num_extents,
                              uint16_t subsystem,
                              const uint8_t *rsrc_bytes, size_t rsrc_len,
                              const win64_pe_tls_info_t *tls_info,
                              const win64_pe_cert_info_t *cert_info);
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

int zcc_emit_win64_pe_dll_ex(const char *filename, const char *dll_name,
                             const uint8_t *code_bytes, size_t code_len,
                             uint32_t entry_offset,
                             const char **imported_funcs, size_t num_imported_funcs,
                             const win64_pe_export_t *exports, size_t num_exports,
                             const uint32_t *reloc_rvas, size_t num_reloc_rvas,
                             const win64_pe_func_extent_t *extents, size_t num_extents,
                             const win64_pe_tls_info_t *tls_info);

int zcc_emit_win64_pe_dll_v7(const char *filename, const char *dll_name,
                             const uint8_t *code_bytes, size_t code_len,
                             uint32_t entry_offset,
                             const char **imported_funcs, size_t num_imported_funcs,
                             const win64_pe_export_t *exports, size_t num_exports,
                             const uint32_t *reloc_rvas, size_t num_reloc_rvas,
                             const win64_pe_func_extent_t *extents, size_t num_extents,
                             const win64_pe_tls_info_t *tls_info,
                             const win64_pe_cert_info_t *cert_info);

/* Delay-Load Import Directory (IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT) */
#define IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT 13

#pragma pack(push, 4)
typedef struct {
    union {
        uint32_t AllAttributes;
        struct {
            uint32_t RvaBased : 1;
            uint32_t Reserved : 31;
        } DllAttributes;
    } Attributes;
    uint32_t DllNameRVA;
    uint32_t ModuleHandleRVA;
    uint32_t ImportAddressTableRVA;
    uint32_t ImportNameTableRVA;
    uint32_t BoundImportAddressTableRVA;
    uint32_t UnloadInformationTableRVA;
    uint32_t TimeDateStamp;
} IMAGE_DELAYLOAD_DESCRIPTOR;
#pragma pack(pop)

typedef struct {
    const char *dll_name;
    const char **func_names;
    size_t      num_funcs;
} win64_pe_delay_dll_t;

typedef struct {
    const win64_pe_delay_dll_t *dlls;
    size_t                     num_dlls;
} win64_pe_delay_info_t;

uint8_t *win64_pe_build_didat(uint32_t didat_rva,
                              const win64_pe_delay_info_t *delay_info,
                              uint32_t *out_total_size,
                              uint32_t *out_desc_rva,
                              uint32_t *out_desc_size);

int zcc_emit_win64_pe_file_v8(const char *filename, const uint8_t *code_bytes,
                              size_t code_len, uint32_t entry_offset,
                              const char **imported_funcs, size_t num_imported_funcs,
                              const uint32_t *reloc_rvas, size_t num_reloc_rvas,
                              const win64_pe_func_extent_t *extents, size_t num_extents,
                              uint16_t subsystem,
                              const uint8_t *rsrc_bytes, size_t rsrc_len,
                              const win64_pe_tls_info_t *tls_info,
                              const win64_pe_cert_info_t *cert_info,
                              const win64_pe_delay_info_t *delay_info);

int zcc_emit_win64_pe_dll_v8(const char *filename, const char *dll_name,
                             const uint8_t *code_bytes, size_t code_len,
                             uint32_t entry_offset,
                             const char **imported_funcs, size_t num_imported_funcs,
                             const win64_pe_export_t *exports, size_t num_exports,
                             const uint32_t *reloc_rvas, size_t num_reloc_rvas,
                             const win64_pe_func_extent_t *extents, size_t num_extents,
                             const win64_pe_tls_info_t *tls_info,
                             const win64_pe_cert_info_t *cert_info,
                             const win64_pe_delay_info_t *delay_info);

uint8_t *win64_pe_build_security_dir(const win64_pe_cert_info_t *cert_info,
                                     uint32_t *out_total_size);

uint8_t *win64_pe_build_tls(uint64_t image_base, uint32_t tls_rva,
                            const win64_pe_tls_info_t *tls_info,
                            uint32_t *out_tls_size, uint32_t *out_dir_rva,
                            uint32_t *out_reloc_rvas, size_t *out_num_relocs);

int zcc_emit_win64_def_file(const char *filename, const char *dll_name,
                            const win64_pe_export_t *exports, size_t num_exports);

int zcc_emit_win64_implib(const char *filename, const char *dll_name,
                          const win64_pe_export_t *exports, size_t num_exports);

#endif /* ZCC_WIN64_PE_EMIT_H */
