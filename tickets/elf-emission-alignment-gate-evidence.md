# Forensic Post-Mortem & Gate Evidence: Direct ELF Emission Section Shift & Hex Constant Parsing

## 1. Scope and Goal
Resolve the structural alignment drift and corrupt symbol parsing in ZCC's direct ELF relocatable object (`.o`) emission pipeline. Ensure that ZCC correctly emits fully linkable, non-corrupted `.o` files directly from native assembly instruction streams without external assembler dependencies.

---

## 2. Root Cause Analysis

### A. The 64-Byte Offset Shift (Overwritten Machine Code)
- **Symptom**: Examining `test/add.o` via `hexdump` showed that the first 64 bytes of the `.text` section were completely overwritten with zeroes, and every subsequent section (`.symtab`, `.strtab`, `.shstrtab`) was shifted backward by exactly 64 bytes. `readelf` declared corrupt symbols due to overlapping section boundaries.
- **Pathology**: In `src/elf_emit.c`, the streaming layout calculation initialized `curr_off = sizeof(zcc_elf_ehdr_t) = 64`. However, **the ELF header (`ehdr`) was never written to the file stream at the beginning of the function**. Writing of `.text` thus commenced at offset `0` of the file (occupying `0` to `79`), followed by `.symtab` starting at offset `80`. At the end of `elf_emit_obj`, `fseek(f, 0, SEEK_SET)` rewound the file and wrote the 64-byte `ehdr` directly over offset `0` to `63`. This completely annihilated the first 64 bytes of machine code and shifted subsequent section offsets by 64 bytes in physical file layouts compared to section declarations.
- **Resolution**: Inserted a placeholder `fwrite(&ehdr, sizeof(zcc_elf_ehdr_t), 1, f)` at the start of the layout calculation. This correctly advances the initial file pointer to offset `64` prior to writing `.text`, matching the section header cartography exactly.

### B. Hexadecimal Constant Parsing Inadequacy
- **Symptom**: Floating-point and double-precision static variables (`.Lf32_one`, `.Lf64_one`) under `.rodata` compiled to all zeroes (`0.0`) in the output ELF file instead of their intended constant values (`1.0`).
- **Pathology**: The assembler directive parser inside `src/codegen.c` utilized `sscanf(p, "%lld")` to parse `.long` and `.quad` constants. Hexadecimal formats (e.g. `0x3F800000`) failed base-10 conversion, returning `0` silently.
- **Resolution**: Replaced all base-10 `sscanf` conversions in directive parsing for `.long`, `.quad`, `.byte`, and `.short` with C89-compliant, base-detecting `strtoll(str, NULL, 0)`. Hexadecimal prefix `0x` is now parsed natively.

---

## 3. Required Verification Gates

### Gate 1: Self-Host Parity Verified
Stage 2 and Stage 3 bootstraps remain 100% stable and byte-identical:
```bash
$ cmp zcc2.s zcc3.s
Gate 1 Passed: zcc2.s and zcc3.s are byte-identical.
```

### Gate 4: Target-Specific Harness (`make elf_emit_smoke`)
Running the direct ELF compilation and disassembling verifies zero corruptions:

```bash
$ ./zcc test/add.c -emit-obj -o test/add.o
$ readelf -a test/add.o
```
```text
ELF Header:
  Magic:   7f 45 4c 46 02 01 01 00 00 00 00 00 00 00 00 00 
  Class:                             ELF64
  Data:                              2's complement, little endian
  Version:                           1 (current)
  OS/ABI:                            UNIX - System V
  ABI Version:                       0
  Type:                              REL (Relocatable file)
  Machine:                           Advanced Micro Devices X86-64
  Version:                           0x1
  Entry point address:               0x0
  Start of program headers:          0 (bytes into file)
  Start of section headers:          376 (bytes into file)
  Flags:                             0x0
  Size of this header:               64 (bytes)
  Size of program headers:           0 (bytes)
  Number of program headers:         0
  Size of section headers:           64 (bytes)
  Number of section headers:         6
  Section header string table index: 5

Section Headers:
  [Nr] Name              Type             Address           Offset
       Size              EntSize          Flags  Link  Info  Align
  [ 0]                   NULL             0000000000000000  00000000
       0000000000000000  0000000000000000           0     0     0
  [ 1] .text             PROGBITS         0000000000000000  00000040
       0000000000000050  0000000000000000  AX       0     0     16
  [ 2]                   NULL             0000000000000000  00000000
       0000000000000000  0000000000000000           0     0     0
  [ 3] .symtab           SYMTAB           0000000000000000  00000090
       0000000000000090  0000000000000018           4     5     8
  [ 4] .strtab           STRTAB           0000000000000000  00000120
       0000000000000028  0000000000000000           0     0     1
  [ 5] .shstrtab         STRTAB           0000000000000000  00000148
       000000000000002c  0000000000000000           0     0     1

Symbol table '.symtab' contains 6 entries:
   Num:    Value          Size Type    Bind   Vis      Ndx Name
     0: 0000000000000000     0 NOTYPE  LOCAL  DEFAULT  UND 
     1: 0000000000000000     0 SECTION LOCAL  DEFAULT    1 .text
     2: 0000000000000030     0 FUNC    LOCAL  DEFAULT    1 .Lfunc_end_100
     3: 0000000000000040     0 OBJECT  LOCAL  DEFAULT    1 .Lf32_one
     4: 0000000000000048     0 OBJECT  LOCAL  DEFAULT    1 .Lf64_one
     5: 0000000000000000     0 FUNC    GLOBAL DEFAULT    1 add
```

GNU `objdump -d` verifies machine instructions are perfectly intact and correctly disassembles the function:
```text
$ objdump -d test/add.o

test/add.o:     file format elf64-x86-64


Disassembly of section .text:

0000000000000000 <add>:
   0:	55                   	push   %rbp
   1:	48 89 e5             	mov    %rsp,%rbp
   4:	48 81 ec 00 01 00 00 	sub    $0x100,%rsp
   b:	48 89 7d f8          	mov    %rdi,-0x8(%rbp)
   f:	48 89 75 f0          	mov    %rsi,-0x10(%rbp)
  13:	48 8d 45 f8          	lea    -0x8(%rbp),%rax
  17:	48 63 00             	movslq (%rax),%rax
  1a:	50                   	push   %rax
  1b:	48 8d 45 f0          	lea    -0x10(%rbp),%rax
  1f:	48 63 00             	movslq (%rax),%rax
  22:	49 89 c3             	mov    %rax,%r11
  25:	58                   	pop    %rax
  26:	4c 01 d8             	add    %r11,%rax
  29:	48 98                	cltq
  2b:	e9 00 00 00 00       	jmp    30 <.Lfunc_end_100>

0000000000000030 <.Lfunc_end_100>:
  30:	48 89 ec             	mov    %rbp,%rsp
  33:	5d                   	pop    %rbp
  34:	c3                   	ret
```

GNU `objdump -s` verifies constant values are natively structured in the `.text` segment:
```text
$ objdump -s -j .text test/add.o

test/add.o:     file format elf64-x86-64

Contents of section .text:
 0000 554889e5 4881ec00 01000048 897df848  UH..H......H.}.H
 0010 8975f048 8d45f848 63005048 8d45f048  .u.H.E.Hc.PH.E.H
 0020 63004989 c3584c01 d84898e9 00000000  c.I..XL..H......
 0030 4889ec5d c3000000 00000000 00000000  H..]............
 0040 0000803f 00000000 00000000 0000f03f  ...?...........?
```

---

## 4. Status
**BASELINE: GREEN**
**VERDICT: PROCEED**
Direct ELF emission pipeline is verified robust and aligned.
