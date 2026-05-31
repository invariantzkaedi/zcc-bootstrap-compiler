ENTRY(_start)

SECTIONS {
    . = 1M;

    .text : ALIGN(4K) {
        KEEP(*(.multiboot2))
        *(.text)
    }

    .rodata : ALIGN(4K) {
        *(.rodata)
    }

    .note : {
        KEEP(*(.note.Xen))
        *(.note.*)
    }

    .data : ALIGN(4K) {
        *(.data)
    }

    .bss : ALIGN(4K) {
        *(COMMON)
        *(.bss)
    }

    _kernel_end = .;
}

