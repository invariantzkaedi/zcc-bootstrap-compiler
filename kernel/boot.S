.section .multiboot2
.align 8
multiboot_header:
    .long 0xe85250d6         # magic
    .long 0                  # architecture
    .long 24                 # header_length
    .long 0x17adaf12         # checksum
    # End tag
    .short 0                 # type
    .short 0                 # flags
    .long 8                  # size
multiboot_header_end:

.section .note.Xen, "a", @note
.align 4
    .long 4                  # namesz
    .long 4                  # descsz
    .long 18                 # type (XEN_ELFNOTE_PHYS32_ENTRY)
    .ascii "Xen\0"           # name
    .long _start             # desc: 32-bit physical entry point

.section .bss
.align 4096
pml4:
    .skip 4096
pdpt:
    .skip 4096
pd:
    .skip 4096

.align 16
stack_bottom:
    .skip 16384             # 16KB stack
stack_top:
.global _kernel_end_asm
_kernel_end_asm:


.section .text
.global _start
.code32
_start:
    cli
    cld
    
    # Save multiboot arguments in registers that won't be clobbered
    # (These will automatically map to %rdi and %rsi in 64-bit mode)
    movl %eax, %edi          # edi = magic
    movl %ebx, %esi          # esi = addr
    
    # Set up stack pointer
    movl $stack_top, %esp

    
    # Transition to 64-bit Long Mode:
    # 1. Set up Page Tables
    call setup_page_tables
    
    # 2. Enable PAE
    movl %cr4, %eax
    orl $(1 << 5), %eax      # Set PAE bit
    movl %eax, %cr4
    
    # 3. Load CR3 with PML4 address
    movl $pml4, %eax
    movl %eax, %cr3
    
    # 4. Enable Long Mode in EFER MSR
    movl $0xC0000080, %ecx   # EFER MSR
    rdmsr
    orl $(1 << 8), %eax      # Set LME bit (bit 8)
    wrmsr
    
    # 5. Enable Paging
    movl %cr0, %eax
    orl $(1 << 31), %eax     # Set PG bit (bit 31)
    movl %eax, %cr0
    
    # 6. Load 64-bit GDT and perform Far Jump to Long Mode
    lgdt gdt64_desc
    ljmp $8, $start64        # 8 is the 64-bit code segment descriptor
    
.halt:
    hlt
    jmp .halt

setup_page_tables:
    # PML4[0] points to PDPT
    movl $pdpt, %eax
    orl $3, %eax             # Present + ReadWrite
    movl %eax, pml4
    
    # PDPT[0] points to PD
    movl $pd, %eax
    orl $3, %eax             # Present + ReadWrite
    movl %eax, pdpt
    
    # Map first 128MB (64 entries of 2MB huge pages) in pd
    movl $0, %ecx            # Index
.map_loop:
    movl %ecx, %eax
    shll $21, %eax           # Address = index * 2MB
    orl $0x83, %eax          # Present + ReadWrite + HugePage (PS = bit 7)
    movl %eax, pd(,%ecx,8)
    incl %ecx
    cmpl $64, %ecx
    jl .map_loop
    ret


.code64
start64:
    # Reload segment registers
    xorw %ax, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss
    
    # Invoke C kmain
    call kmain
    
.halt64:
    hlt
    jmp .halt64

.global outb
.type outb, @function
outb:
    movw %di, %dx
    movb %sil, %al
    outb %al, %dx
    ret

.global inb
.type inb, @function
inb:
    movw %di, %dx
    inb %dx, %al
    movzbl %al, %eax
    ret

# --- Interrupt Loaders ---
.global read_cr3
.type read_cr3, @function
read_cr3:
    movq %cr3, %rax
    ret

.global invlpg_native
.type invlpg_native, @function
invlpg_native:
    invlpg (%rdi)
    ret

.global lidt_native
.type lidt_native, @function
lidt_native:
    lidt (%rdi)
    ret

.global sti_native
.type sti_native, @function
sti_native:
    sti
    ret

.global cli_native
.type cli_native, @function
cli_native:
    cli
    ret

# --- ISR Stubs ---
.global isr0
isr0:
    pushq $0                 # dummy error code
    pushq $0                 # vector 0
    jmp exception_common_stub

.global isr8
isr8:
    # Double fault pushes error code automatically
    pushq $8                 # vector 8
    jmp exception_common_stub

.global isr13
isr13:
    # GPF pushes error code automatically
    pushq $13                # vector 13
    jmp exception_common_stub

.global isr14
isr14:
    # Page fault pushes error code automatically
    pushq $14                # vector 14
    jmp exception_common_stub

.global isr32
isr32:
    pushq $0                 # dummy error code
    pushq $32                # vector 32
    jmp isr_common_stub

.global isr33
isr33:
    pushq $0                 # dummy error code
    pushq $33                # vector 33
    jmp isr_common_stub

# --- Common Exception Stub handler ---
.type exception_common_stub, @function
exception_common_stub:
    # Push GPRs in exact reverse order of struct interrupt_frame:
    # (r15, r14, r13, r12, r11, r10, r9, r8, rdi, rsi, rbp, rbx, rdx, rcx, rax)
    pushq %rax
    pushq %rcx
    pushq %rdx
    pushq %rbx
    pushq %rbp
    pushq %rsi
    pushq %rdi
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    # Pass stack pointer (%rsp) as first parameter (%rdi) to handle_exception_state
    movq %rsp, %rdi

    # Call C exception state handler
    call handle_exception_state

    # In case the handler returns (defensive backup), restore registers and iretq
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rdi
    popq %rsi
    popq %rbp
    popq %rbx
    popq %rdx
    popq %rcx
    popq %rax

    # Pop vector and error code
    addq $16, %rsp
    iretq

# --- Common ISR Stub handler ---
.type isr_common_stub, @function
isr_common_stub:
    # Save volatile registers
    pushq %rax
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11

    # Pass vector (offset 72 from RSP) as first parameter (%rdi)
    movq 72(%rsp), %rdi

    # Call C handler dispatcher
    call handle_interrupt

    # Restore volatile registers
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rax

    # Pop vector number and dummy error code
    addq $16, %rsp
    
    # 64-bit Interrupt Return
    iretq

.section .data
.align 8
gdt64:
    .quad 0x0000000000000000 # Null descriptor
    .quad 0x0020980000000000 # 64-bit Code descriptor
    .quad 0x0000920000000000 # 64-bit Data descriptor
gdt64_desc:
    .short gdt64_desc - gdt64 - 1
    .long gdt64
