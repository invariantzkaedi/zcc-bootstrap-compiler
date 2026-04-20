# CG-BUILTIN-001: Missing `__builtin_bswap16/32/64` Intrinsic Support

**Status:** OPEN
**Discovery:** 2026-04-19 during B2 real-world exercise (Leviathan v2 Generalization Diagnostic)

## 1. Context
`leviathan.c` successfully links with the B3 `CG-PARSE-001` patch (routing successfully through all `__asm__` renames like `__isoc99_fscanf` and `open64`) but encounters linker failures during final assembly compilation with GCC.

## 2. Failure Details
```
/usr/bin/ld: /tmp/ccYaOlDe.o: in function `__bswap_16':
(.text+0x20): undefined reference to `__builtin_bswap16'
/usr/bin/ld: /tmp/ccYaOlDe.o: in function `__bswap_32':
(.text+0x4b): undefined reference to `__builtin_bswap32'
/usr/bin/ld: /tmp/ccYaOlDe.o: in function `__bswap_64':
(.text+0x77): undefined reference to `__builtin_bswap64'
collect2: error: ld returned 1 exit status
```

## 3. Root Cause
ZCC parser correctly consumes `__builtin_bswap` but fails to expand/inline the GCC intrinsics within the `part4.c` system-V ABI codegen layer. This results in standard function calls (`call __builtin_bswap16`) being emitted into assembly `.s` files, which `libgcc` does not inherently supply as external variables.

## 4. Blast Radius
Unknown, requires a dedicated boundary sweep across headers relying heavily on byte-swapping (such as `<arpa/inet.h>`, `<byteswap.h>`).

## 5. Priority
**Low**. Unless another core user program actively breaks on network endianness translations, this can be deferred. Workarounds (like injecting a manual `#define __builtin_bswap16(x) ...` shim) are sufficient.
