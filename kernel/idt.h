#ifndef IDT_H
#define IDT_H

/* 64-bit Interrupt Gate Descriptor (16 Bytes, packed) */
struct idt_entry_64 {
    unsigned short offset_low;      /* bits 0..15 */
    unsigned short selector;        /* GDT code segment selector */
    unsigned char  ist;             /* Interrupt Stack Table index */
    unsigned char  type_attr;       /* Type and attributes (0x8E = 64-bit Interrupt Gate) */
    unsigned short offset_middle;   /* bits 16..31 */
    unsigned int   offset_high;     /* bits 32..63 */
    unsigned int   reserved;        /* Reserved/Zero */
} __attribute__((packed));

/* IDT Register Descriptor (10 Bytes, packed) */
struct idt_ptr_64 {
    unsigned short limit;
    unsigned long  base;
} __attribute__((packed));

/* Reflects stack state on serious CPU exceptions mapped to AMD64 registers */
struct interrupt_frame {
    unsigned long r15, r14, r13, r12, r11, r10, r9, r8;
    unsigned long rdi, rsi, rbp, rbx, rdx, rcx, rax;
    unsigned long vector;
    unsigned long error_code;
    unsigned long rip, cs, rflags, rsp, ss;
} __attribute__((packed));

/* Linker image boundary marker */
extern unsigned char _kernel_end[];

/* PIC Register Ports & Command Values */
#define PIC1          0x20
#define PIC2          0xA0
#define PIC1_COMMAND  PIC1
#define PIC1_DATA     (PIC1 + 1)
#define PIC2_COMMAND  PIC2
#define PIC2_DATA     (PIC2 + 1)

#define PIC_EOI       0x20

#define ICW1_INIT     0x10
#define ICW1_ICW4     0x01
#define ICW4_8086     0x01

/* Low-level Interrupt Service Routine Stubs defined in boot.S */
extern void isr0(void);   /* Divide-by-Zero Exception */
extern void isr8(void);   /* Double Fault Exception */
extern void isr13(void);  /* General Protection Fault */
extern void isr14(void);  /* Page Fault Exception */
extern void isr32(void);  /* PIT Timer Interrupt (IRQ0) */
extern void isr33(void);  /* Keyboard Controller Interrupt (IRQ1) */

/* Low-level Assembly helpers defined in boot.S */
extern void lidt_native(struct idt_ptr_64 *ptr);
extern void sti_native(void);
extern void cli_native(void);
extern void outb(unsigned short port, unsigned char val);
extern unsigned char inb(unsigned short port);

/* C Interrupt Dispatcher called by isr_common_stub */
void handle_interrupt(unsigned long vector);

/* C CPU Exception State handler called by exception_common_stub */
void handle_exception_state(struct interrupt_frame *frame);

#endif /* IDT_H */

