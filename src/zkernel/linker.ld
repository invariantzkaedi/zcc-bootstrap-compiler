/*
 * Memory Map & Linker Script for RP2040 Cortex-M0+ zkernel
 */

MEMORY
{
    FLASH(rx) : ORIGIN = 0x10000000, LENGTH = 2048K
    SRAM(rwx) : ORIGIN = 0x20000000, LENGTH = 256K
}

ENTRY(_reset_handler)

SECTIONS
{
    /* Code in Flash */
    .text : {
        . = ALIGN(4);
        KEEP(*(.vectors))
        *(.text*)
        *(.rodata*)
        . = ALIGN(4);
    } > FLASH

    /* Global data (in RAM, initialised from Flash) */
    .data : {
        . = ALIGN(4);
        __data_start = .;
        *(.data*)
        . = ALIGN(4);
        __data_end = .;
    } > SRAM AT > FLASH

    __data_load = LOADADDR(.data);

    /* Uninitialised data in RAM */
    .bss : {
        . = ALIGN(4);
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end = .;
    } > SRAM

    /* Stack starts at top of RAM */
    __stack_top = ORIGIN(SRAM) + LENGTH(SRAM);
}
