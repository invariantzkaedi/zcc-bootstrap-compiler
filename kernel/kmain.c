#include "idt.h"

/* VGA Text Framebuffer metrics & buffer mapping */
#define VGA_COLS 80
#define VGA_ROWS 25
#define VGA_ATTRIB 0x0F /* White text on Black background */

static char *vga_buffer = (char *)0xB8000;
static int cursor_row = 0;
static int cursor_col = 0;

/* Monotonic tick counter */
volatile unsigned long long tick_count = 0;

/* Global IDT table carrying 256 gates */
struct idt_entry_64 idt[256];

/* Managing 32,768 page blocks (128MB) via 4,096 bytes (4KB) static bitmap */
static unsigned char pmm_bitmap[4096];
static unsigned long long pmm_total_blocks = 32768;
static unsigned long long pmm_free_blocks = 32768;

extern unsigned long read_cr3(void);
extern void invlpg_native(unsigned long vaddr);

void *pmm_alloc_page(void);
void pmm_free_page(void *addr);
void kprint_string(const char *s);
void kprint_hex(unsigned long long val);
void kprint_dec(long long val);
void kwrite_serial(const char *msg);
void kwrite_serial_hex(unsigned long long val);


struct multiboot2_info_header {
    unsigned int total_size;
    unsigned int reserved;
};

struct multiboot2_tag {
    unsigned int type;
    unsigned int size;
};

struct multiboot2_tag_mmap {
    unsigned int type;
    unsigned int size;
    unsigned int entry_size;
    unsigned int entry_version;
};

struct multiboot2_mmap_entry {
    unsigned long long addr;
    unsigned long long len;
    unsigned int type;
    unsigned int zero;
};


void kmemset(void *ptr, int val, int len) {
    unsigned char *p = (unsigned char *)ptr;
    int i;
    for (i = 0; i < len; i++) {
        p[i] = (unsigned char)val;
    }
}

void map_page(unsigned long vaddr, unsigned long paddr, unsigned long flags) {
    unsigned long cr3 = read_cr3();
    unsigned long *pml4 = (unsigned long *)(cr3 & ~0xFFFL);

    unsigned int pml4_idx = (vaddr >> 39) & 0x1FF;
    unsigned int pdpt_idx = (vaddr >> 30) & 0x1FF;
    unsigned int pd_idx = (vaddr >> 21) & 0x1FF;
    unsigned int pt_idx = (vaddr >> 12) & 0x1FF;

    kwrite_serial("map_page: cr3=0x"); kwrite_serial_hex(cr3);
    kwrite_serial(" pml4=0x"); kwrite_serial_hex((unsigned long)pml4);
    kwrite_serial("\n");

    /* PML4 Entry */
    if ((pml4[pml4_idx] & 1) == 0) {
        void *new_pdpt = pmm_alloc_page();
        kmemset(new_pdpt, 0, 4096);
        pml4[pml4_idx] = (unsigned long)new_pdpt | 3; /* Present + ReadWrite */
    }
    unsigned long *pdpt = (unsigned long *)(pml4[pml4_idx] & ~0xFFFL);
    kwrite_serial("map_page: pdpt=0x"); kwrite_serial_hex((unsigned long)pdpt);
    kwrite_serial(" pml4_idx="); kwrite_serial_hex(pml4_idx);
    kwrite_serial(" val=0x"); kwrite_serial_hex(pml4[pml4_idx]);
    kwrite_serial("\n");

    /* PDPT Entry */
    if ((pdpt[pdpt_idx] & 1) == 0) {
        void *new_pd = pmm_alloc_page();
        kmemset(new_pd, 0, 4096);
        pdpt[pdpt_idx] = (unsigned long)new_pd | 3; /* Present + ReadWrite */
    }
    unsigned long *pd = (unsigned long *)(pdpt[pdpt_idx] & ~0xFFFL);
    kwrite_serial("map_page: pd=0x"); kwrite_serial_hex((unsigned long)pd);
    kwrite_serial(" pdpt_idx="); kwrite_serial_hex(pdpt_idx);
    kwrite_serial(" val=0x"); kwrite_serial_hex(pdpt[pdpt_idx]);
    kwrite_serial("\n");

    /* PD Entry */
    if ((pd[pd_idx] & 1) == 0) {
        void *new_pt = pmm_alloc_page();
        kmemset(new_pt, 0, 4096);
        pd[pd_idx] = (unsigned long)new_pt | 3; /* Present + ReadWrite */
    } else if ((pd[pd_idx] & 0x80) != 0) {
        kwrite_serial("map_page: pd entry is huge page!\n");
        return;
    }
    unsigned long *pt = (unsigned long *)(pd[pd_idx] & ~0xFFFL);
    kwrite_serial("map_page: pt=0x"); kwrite_serial_hex((unsigned long)pt);
    kwrite_serial(" pd_idx="); kwrite_serial_hex(pd_idx);
    kwrite_serial(" val=0x"); kwrite_serial_hex(pd[pd_idx]);
    kwrite_serial("\n");

    /* PT Entry */
    pt[pt_idx] = paddr | flags;
    kwrite_serial("map_page: pt_idx="); kwrite_serial_hex(pt_idx);
    kwrite_serial(" pt_entry=0x"); kwrite_serial_hex(pt[pt_idx]);
    kwrite_serial("\n");

    /* Invalidate TLB entry for this page */
    invlpg_native(vaddr);
}


void pmm_init(unsigned long kernel_end_addr, unsigned long magic, unsigned long addr) {
    unsigned long long i;
    unsigned long long kernel_end_block;
    
    /* Initially mark all blocks as allocated (reserved) */
    for (i = 0; i < 4096; i++) {
        pmm_bitmap[i] = 0xFF;
    }
    pmm_free_blocks = 0;

    int parsed_mmap = 0;
    if (magic == 0x36d76289 && addr != 0) {
        struct multiboot2_info_header *info = (struct multiboot2_info_header *)addr;
        char *ptr = (char *)(addr + 8);
        char *end_ptr = (char *)(addr + info->total_size);

        while (ptr < end_ptr) {
            struct multiboot2_tag *tag = (struct multiboot2_tag *)ptr;
            if (tag->type == 0 && tag->size == 8) {
                break;
            }

            if (tag->type == 6) {
                struct multiboot2_tag_mmap *mmap_tag = (struct multiboot2_tag_mmap *)tag;
                char *entry_ptr = (char *)ptr + 16;
                char *entries_end = (char *)ptr + tag->size;

                while (entry_ptr < entries_end) {
                    struct multiboot2_mmap_entry *entry = (struct multiboot2_mmap_entry *)entry_ptr;
                    
                    if (entry->type == 1) {
                        unsigned long long start_addr = entry->addr;
                        unsigned long long end_addr = entry->addr + entry->len;

                        unsigned long long start_block = (start_addr + 4095) / 4096;
                        unsigned long long end_block = end_addr / 4096;

                        for (i = start_block; i < end_block && i < pmm_total_blocks; i++) {
                            if ((pmm_bitmap[i / 8] & (1 << (i % 8))) != 0) {
                                pmm_bitmap[i / 8] &= ~(1 << (i % 8));
                                pmm_free_blocks++;
                            }
                        }
                    }
                    entry_ptr += mmap_tag->entry_size;
                }
                parsed_mmap = 1;
                break;
            }
            ptr += ((tag->size + 7) & ~7);
        }
    }

    if (!parsed_mmap) {
        for (i = 0; i < 4096; i++) {
            pmm_bitmap[i] = 0;
        }
        pmm_free_blocks = pmm_total_blocks;
    }
    
    /* Mark everything below 1MB (256 blocks) as reserved */
    for (i = 0; i < 256; i++) {
        if (i < pmm_total_blocks) {
            if ((pmm_bitmap[i / 8] & (1 << (i % 8))) == 0) {
                pmm_bitmap[i / 8] |= (1 << (i % 8));
                pmm_free_blocks--;
            }
        }
    }
    
    /* Calculate block index of kernel end address */
    kernel_end_block = (kernel_end_addr + 4095) / 4096;
    if (kernel_end_block < 256) {
        kernel_end_block = 256;
    }
    
    /* Mark kernel space (from 1MB to kernel_end_block) as reserved */
    for (i = 256; i < kernel_end_block && i < pmm_total_blocks; i++) {
        if ((pmm_bitmap[i / 8] & (1 << (i % 8))) == 0) {
            pmm_bitmap[i / 8] |= (1 << (i % 8));
            pmm_free_blocks--;
        }
    }
    
    /* Mark blocks beyond 128MB as allocated to prevent out-of-bounds allocations */
    for (i = pmm_total_blocks; i < 32768; i++) {
        pmm_bitmap[i / 8] |= (1 << (i % 8));
    }
}

void *pmm_alloc_page(void) {
    unsigned long long i, j;
    for (i = 0; i < 4096; i++) {
        if (pmm_bitmap[i] != 0xFF) { /* At least one free block in this byte */
            for (j = 0; j < 8; j++) {
                if ((pmm_bitmap[i] & (1 << j)) == 0) {
                    pmm_bitmap[i] |= (1 << j);
                    pmm_free_blocks--;
                    return (void *)((i * 8 + j) * 4096);
                }
            }
        }
    }
    return (void *)0; /* Out of memory */
}

void pmm_free_page(void *addr) {
    unsigned long long block = (unsigned long long)addr / 4096;
    if (block < pmm_total_blocks) {
        if ((pmm_bitmap[block / 8] & (1 << (block % 8))) != 0) {
            pmm_bitmap[block / 8] &= ~(1 << (block % 8));
            pmm_free_blocks++;
        }
    }
}

unsigned long long pmm_get_free_blocks(void) {
    return pmm_free_blocks;
}

/* Helper to clear visual screen */
void kclear_screen(void) {
    int i;
    for (i = 0; i < VGA_COLS * VGA_ROWS; i++) {
        vga_buffer[i * 2] = ' ';
        vga_buffer[i * 2 + 1] = VGA_ATTRIB;
    }
    cursor_row = 0;
    cursor_col = 0;
}

/* Helper to scroll screen up by 1 row */
static void kscroll(void) {
    int r, c;
    for (r = 1; r < VGA_ROWS; r++) {
        for (c = 0; c < VGA_COLS; c++) {
            vga_buffer[((r - 1) * VGA_COLS + c) * 2] = vga_buffer[(r * VGA_COLS + c) * 2];
            vga_buffer[((r - 1) * VGA_COLS + c) * 2 + 1] = vga_buffer[(r * VGA_COLS + c) * 2 + 1];
        }
    }
    /* Clear last row */
    for (c = 0; c < VGA_COLS; c++) {
        vga_buffer[((VGA_ROWS - 1) * VGA_COLS + c) * 2] = ' ';
        vga_buffer[((VGA_ROWS - 1) * VGA_COLS + c) * 2 + 1] = VGA_ATTRIB;
    }
    cursor_row = VGA_ROWS - 1;
}

/* Print a single character to VGA monitor */
void kprint_char(char c) {
    if (c == '\n') {
        cursor_col = 0;
        cursor_row++;
        if (cursor_row >= VGA_ROWS) {
            kscroll();
        }
        return;
    }
    if (c == '\r') {
        cursor_col = 0;
        return;
    }
    if (c == '\b') {
        if (cursor_col > 0) {
            cursor_col--;
            vga_buffer[(cursor_row * VGA_COLS + cursor_col) * 2] = ' ';
        }
        return;
    }
    if (c == '\t') {
        cursor_col = (cursor_col + 8) & ~(8 - 1);
        if (cursor_col >= VGA_COLS) {
            cursor_col = 0;
            cursor_row++;
            if (cursor_row >= VGA_ROWS) {
                kscroll();
            }
        }
        return;
    }

    vga_buffer[(cursor_row * VGA_COLS + cursor_col) * 2] = c;
    vga_buffer[(cursor_row * VGA_COLS + cursor_col) * 2 + 1] = VGA_ATTRIB;
    cursor_col++;
    if (cursor_col >= VGA_COLS) {
        cursor_col = 0;
        cursor_row++;
        if (cursor_row >= VGA_ROWS) {
            kscroll();
        }
    }
}

/* Print a null-terminated string */
void kprint_string(const char *s) {
    int i;
    for (i = 0; s[i] != '\0'; i++) {
        kprint_char(s[i]);
    }
}

/* Print hex formatting value deterministically */
void kprint_hex(unsigned long long val) {
    char buf[32];
    int i = 0;
    if (val == 0) {
        kprint_char('0');
        return;
    }
    while (val > 0) {
        int rem = val % 16;
        if (rem < 10) {
            buf[i++] = '0' + rem;
        } else {
            buf[i++] = 'A' + (rem - 10);
        }
        val /= 16;
    }
    /* print in reverse */
    while (i > 0) {
        kprint_char(buf[--i]);
    }
}

/* Print decimal formatting value deterministically */
void kprint_dec(long long val) {
    char buf[32];
    int i = 0;
    int is_neg = 0;
    if (val == 0) {
        kprint_char('0');
        return;
    }
    if (val < 0) {
        is_neg = 1;
        val = -val;
    }
    while (val > 0) {
        buf[i++] = '0' + (val % 10);
        val /= 10;
    }
    if (is_neg) {
        kprint_char('-');
    }
    while (i > 0) {
        kprint_char(buf[--i]);
    }
}

/* IDT Descriptor Wiring */
void idt_set_gate(int num, unsigned long base, unsigned short sel, unsigned char flags) {
    idt[num].offset_low = base & 0xFFFF;
    idt[num].selector = sel;
    idt[num].ist = 0;
    idt[num].type_attr = flags;
    idt[num].offset_middle = (base >> 16) & 0xFFFF;
    idt[num].offset_high = (base >> 32) & 0xFFFFFFFF;
    idt[num].reserved = 0;
}

/* Remap PIC vectors to bypass CPU exception ports */
void pic_remap(void) {
    outb(0x20, 0x11); /* ICW1: init master */
    outb(0xA0, 0x11); /* ICW1: init slave */
    outb(0x21, 0x20); /* ICW2: master offset = 32 */
    outb(0xA1, 0x28); /* ICW2: slave offset = 40 */
    outb(0x21, 0x04); /* ICW3: master master-slave link */
    outb(0xA1, 0x02); /* ICW3: slave cascade identity */
    outb(0x21, 0x01); /* ICW4: 8086 mode */
    outb(0xA1, 0x01); /* ICW4: 8086 mode */
    outb(0x21, 0xFC); /* Enable only Timer (IRQ0) and Keyboard (IRQ1) */
    outb(0xA1, 0xFF); /* Disable all slave IRQs */
}

/* Master C Interrupt Dispatcher called by isr_common_stub */
void handle_interrupt(unsigned long vector) {
    if (vector == 32) {
        /* Timer Interrupt */
        tick_count++;
        if (tick_count % 100 == 0) {
            kprint_string(" [HEARTBEAT] ");
        }
        outb(0x20, 0x20); /* EOI to PIC Master */
    } else if (vector == 33) {
        /* Keyboard Interrupt */
        unsigned char scancode = inb(0x60);
        if (scancode < 0x80) {
            static const char keymap[128] = {
                0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
                '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
                0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,
                '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' '
            };
            char c = keymap[scancode];
            if (c != 0) {
                kprint_char(c);
            }
        }
        outb(0x20, 0x20); /* EOI to PIC Master (IRQ1) to prevent locks */
    } else {
        /* CPU Exception */
        kprint_string("\n*** EXCEPTION VECTOR DETECTED: ");
        kprint_dec(vector);
        kprint_string(" ***\n");
        while (1) {
            cli_native();
            /* halt processor */
        }
    }
}

void kwrite_serial(const char *msg) {
    int i;
    for (i = 0; msg[i] != '\0'; i++) {
        while ((inb(0x3F8 + 5) & 0x20) == 0) {
            /* busy wait */
        }
        outb(0x3F8, (unsigned char)msg[i]);
    }
}

/* Print hex formatting value directly to COM1 serial port */
void kwrite_serial_hex(unsigned long long val) {
    char buf[32];
    int idx = 0;
    if (val == 0) {
        kwrite_serial("0");
        return;
    }
    while (val > 0) {
        int rem = val % 16;
        buf[idx++] = (rem < 10) ? '0' + rem : 'A' + (rem - 10);
        val /= 16;
    }
    while (idx > 0) {
        char ch = buf[--idx];
        outb(0x3F8, ch);
    }
}

/* Master CPU Exception State Handler called by exception_common_stub */
void handle_exception_state(struct interrupt_frame *frame) {
    const char *name = "UNKNOWN EXCEPTION";
    if (frame->vector == 0) name = "DIVIDE-BY-ZERO (#DE)";
    else if (frame->vector == 8) name = "DOUBLE FAULT (#DF)";
    else if (frame->vector == 13) name = "GENERAL PROTECTION FAULT (#GP)";
    else if (frame->vector == 14) name = "PAGE FAULT (#PF)";

    /* 1. Print bold visual failure state on the VGA console */
    kprint_string("\n================================================================================\n");
    kprint_string("                    !!! BARE-METAL CPU EXCEPTION DETECTED !!!                   \n");
    kprint_string("================================================================================\n");
    kprint_string("EXCEPTION: ");
    kprint_string(name);
    kprint_string(" (Vector: ");
    kprint_dec(frame->vector);
    kprint_string(")\n");
    kprint_string("ERROR CODE: 0x");
    kprint_hex(frame->error_code);
    kprint_string("\n\n");

    /* 2. Dump register states in hexadecimal formatting */
    kprint_string("RAX: 0x"); kprint_hex(frame->rax); kprint_string("  RBX: 0x"); kprint_hex(frame->rbx); kprint_string("  RCX: 0x"); kprint_hex(frame->rcx); kprint_string("  RDX: 0x"); kprint_hex(frame->rdx); kprint_string("\n");
    kprint_string("RSI: 0x"); kprint_hex(frame->rsi); kprint_string("  RDI: 0x"); kprint_hex(frame->rdi); kprint_string("  RBP: 0x"); kprint_hex(frame->rbp); kprint_string("  RSP: 0x"); kprint_hex(frame->rsp); kprint_string("\n");
    kprint_string("R8:  0x"); kprint_hex(frame->r8);  kprint_string("  R9:  0x"); kprint_hex(frame->r9);  kprint_string("  R10: 0x"); kprint_hex(frame->r10); kprint_string("  R11: 0x"); kprint_hex(frame->r11); kprint_string("\n");
    kprint_string("R12: 0x"); kprint_hex(frame->r12); kprint_string("  R13: 0x"); kprint_hex(frame->r13); kprint_string("  R14: 0x"); kprint_hex(frame->r14); kprint_string("  R15: 0x"); kprint_hex(frame->r15); kprint_string("\n\n");
    
    kprint_string("RIP: 0x"); kprint_hex(frame->rip); kprint_string("  CS: 0x"); kprint_hex(frame->cs); kprint_string("  RFLAGS: 0x"); kprint_hex(frame->rflags); kprint_string("\n");
    kprint_string("================================================================================\n");
    kprint_string("                     SYSTEM HALTED. CLI/HLT ACTIVE.                             \n");
    kprint_string("================================================================================\n");

    /* 3. Output registration events to the COM1 serial port */
    kwrite_serial("\n*** BARE-METAL CPU EXCEPTION DETECTED ***\n");
    kwrite_serial("Exception Name: "); kwrite_serial(name); kwrite_serial("\n");
    kwrite_serial("Vector: "); kprint_dec(frame->vector); kwrite_serial("\n");
    kwrite_serial("RIP: 0x"); kwrite_serial_hex(frame->rip); kwrite_serial("\n");
    kwrite_serial("RAX: 0x"); kwrite_serial_hex(frame->rax); kwrite_serial("  RBX: 0x"); kwrite_serial_hex(frame->rbx); kwrite_serial("\n");
    kwrite_serial("[ZKAEDI_V2_EXCEPTION_SUCCESS]\n");

    /* 4. Infinite CPU halt loop */
    while (1) {
        cli_native();
        __asm__ __volatile__("hlt");
    }
}

void kmain(unsigned long magic, unsigned long addr) {
    /* 1. Initialize COM1 port */
    outb(0x3F8 + 1, 0x00);    /* Disable all interrupts */
    outb(0x3F8 + 3, 0x80);    /* Enable DLAB */
    outb(0x3F8 + 0, 0x01);    /* Divisor 1 (lo) */
    outb(0x3F8 + 1, 0x00);    /*         1 (hi) */
    outb(0x3F8 + 3, 0x03);    /* 8N1 protocol */
    outb(0x3F8 + 2, 0xC7);    /* FIFO init */
    outb(0x3F8 + 4, 0x0B);    /* IRQ enabled */

    /* 2. Visual Framebuffer Setup */
    kclear_screen();
    kprint_string("================================================================================\n");
    kprint_string("                  ZKAEDI BARE-METAL KERNEL v2 OPERATIONAL                       \n");
    kprint_string("================================================================================\n\n");
    kprint_string("[SYSTEM] VGA Monitor identity mapped at 0xB8000.\n");
    kprint_string("[SYSTEM] Display Engine loaded cleanly. No varargs overhead.\n");

    /* Log multiboot detection */
    if (magic == 0x36d76289) {
        kprint_string("[SYSTEM] Multiboot2 detected at physical address: 0x");
        kprint_hex(addr);
        kprint_string("\n");
        kwrite_serial("[SYSTEM] Multiboot2 detected\n");
    } else {
        kprint_string("[SYSTEM] Legacy boot / no Multiboot2 signature. Using fallback map.\n");
        kwrite_serial("[SYSTEM] Legacy boot fallback\n");
    }

    /* 3. Physical Memory Manager Setup & Self-Test */
    kprint_string("[SYSTEM] PMM: Initializing Page Frame Allocator...\n");
    kwrite_serial("[SYSTEM] PMM: _kernel_end address = 0x");
    kwrite_serial_hex((unsigned long)&_kernel_end_asm);
    kwrite_serial("\n[SYSTEM] PMM: idt address = 0x");
    kwrite_serial_hex((unsigned long)idt);
    kwrite_serial("\n[SYSTEM] PMM: pmm_bitmap address = 0x");
    kwrite_serial_hex((unsigned long)pmm_bitmap);
    kwrite_serial("\n");
    pmm_init((unsigned long)&_kernel_end_asm, magic, addr);
    kprint_string("[SYSTEM] PMM: Free blocks managed: ");
    kprint_dec(pmm_get_free_blocks());
    kprint_string(" (");
    kprint_dec(pmm_get_free_blocks() * 4);
    kprint_string(" KB free)\n");

    {
        void *p1 = pmm_alloc_page();
        void *p2 = pmm_alloc_page();
        
        kprint_string("[SYSTEM] PMM: Allocated page 1 at physical address: 0x");
        kprint_hex((unsigned long)p1);
        kprint_string("\n");
        kprint_string("[SYSTEM] PMM: Allocated page 2 at physical address: 0x");
        kprint_hex((unsigned long)p2);
        kprint_string("\n");
        
        pmm_free_page(p1);
        pmm_free_page(p2);
        kprint_string("[SYSTEM] PMM: Pages freed successfully. Free blocks: ");
        kprint_dec(pmm_get_free_blocks());
        kprint_string("\n");
    }

    /* 4. Page table mapping test */
    kprint_string("[SYSTEM] Paging: Running page mapping self-test...\n");
    {
        void *phys_page = pmm_alloc_page();
        if (phys_page) {
            kwrite_serial("Test: phys_page=0x"); kwrite_serial_hex((unsigned long)phys_page); kwrite_serial("\n");
            map_page(0x8000000, (unsigned long)phys_page, 3);
            
            volatile int *vptr = (volatile int *)0x8000000;
            *vptr = 0xDEADC0DE;
            kwrite_serial("Test: wrote 0xDEADC0DE to 0x8000000\n");
            
            volatile int *pptr = (volatile int *)phys_page;
            kwrite_serial("Test: read from 0x8000000 = 0x"); kwrite_serial_hex(*vptr); kwrite_serial("\n");
            kwrite_serial("Test: read from phys_page = 0x"); kwrite_serial_hex(*pptr); kwrite_serial("\n");
            
            if (*pptr == 0xDEADC0DE) {
                kprint_string("[SYSTEM] Paging: Dynamic page mapping VERIFIED successfully!\n");
                kwrite_serial("[SYSTEM] Paging: Page map success\n");
            } else {
                kprint_string("[SYSTEM] Paging: Dynamic page mapping FAILED!\n");
                kwrite_serial("[SYSTEM] Paging: Page map failure\n");
            }
            pmm_free_page(phys_page);
        } else {
            kprint_string("[SYSTEM] Paging: Failed to allocate physical page for test!\n");
        }
    }

    /* 5. Interrupt Descriptors Setup */
    pic_remap();
    kprint_string("[SYSTEM] 8259 PIC remapped to vectors 32-47.\n");

    /* Clear IDT entries */
    {
        int i;
        for (i = 0; i < 256; i++) {
            idt_set_gate(i, 0, 0, 0);
        }
    }

    /* Set exception and hardware interrupt gates */
    idt_set_gate(0, (unsigned long)isr0, 0x08, 0x8E);
    idt_set_gate(8, (unsigned long)isr8, 0x08, 0x8E);
    idt_set_gate(13, (unsigned long)isr13, 0x08, 0x8E);
    idt_set_gate(14, (unsigned long)isr14, 0x08, 0x8E);
    idt_set_gate(32, (unsigned long)isr32, 0x08, 0x8E);
    idt_set_gate(33, (unsigned long)isr33, 0x08, 0x8E);

    /* Load IDT Register */
    {
        struct idt_ptr_64 idt_p;
        idt_p.limit = (sizeof(struct idt_entry_64) * 256) - 1;
        idt_p.base = (unsigned long)&idt;
        lidt_native(&idt_p);
    }
    kprint_string("[SYSTEM] 64-bit IDT descriptors registered.\n");

    /* 6. Programmable Interval Timer Setup (100 Hz) */
    outb(0x43, 0x36);         /* Mode 3: Square Wave */
    outb(0x40, 0x9C);         /* Divisor low: 0x9C */
    outb(0x40, 0x2E);         /* Divisor high: 0x2E */
    kprint_string("[SYSTEM] PIT scheduled for 100 Hz clock ticks.\n");

    /* 7. Enable CPU Interrupts & Run E2E Diagnostics */
    sti_native();
    kwrite_serial("[ZKAEDI_V2_BOOT_SUCCESS]\n");
    kprint_string("[SYSTEM] interrupts active. Clock loop online. Boot successful.\n\n");
    kprint_string("kmain console input echo > ");
    
    kprint_string("\n[TESTING] Triggering intentional Division-by-Zero CPU exception...\n");
    {
        volatile int a = 1;
        volatile int b = 0;
        volatile int c = a / b;
        (void)c;
    }

    while (1) {
        __asm__ __volatile__("hlt");
    }
}
