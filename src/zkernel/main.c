#include "uart.h"
#include "sched.h"

/* RP2040 MMIO Map for Blinky & SIO GPIO */
#define SIO_BASE          0xd0000000
#define SIO_GPIO_OUT      ((volatile unsigned int *)(SIO_BASE + 0x010))
#define SIO_GPIO_OE       ((volatile unsigned int *)(SIO_BASE + 0x020))

#define padsbank0_BASE    0x4001c000
#define padsbank0_gpio25  ((volatile unsigned int *)(padsbank0_BASE + 0x068))

#define iobank0_BASE      0x40014000
#define iobank0_gpio25_ctrl ((volatile unsigned int *)(iobank0_BASE + 0x0cc))

static void delay(int count) {
    int i;
    for (i = 0; i < count; i++) {
        __asm__ volatile("nop");
    }
}

/* 1024-byte stack for Task 1, 8-byte aligned */
static unsigned int task1_stack[256];

void task1_entry(void) {
    int c;
    while (1) {
        if (uart_char_available()) {
            c = uart_getc();
            if (c != -1) {
                uart_puts("Task 1 Rx: ");
                uart_putc((char)c);
                uart_puts("\n");
            }
        }
        sched_yield();
    }
}

int main(void) {
    /* Set GPIO25 (LED) pad config: enable output buffer, disable input */
    *padsbank0_gpio25 = 0x00000000;

    /* Set GPIO25 function to SIO (F5) */
    *iobank0_gpio25_ctrl = 5;

    /* Enable SIO GPIO25 as output */
    *SIO_GPIO_OE = (1 << 25);

    /* Init Drivers & Scheduler */
    uart_init();
    sched_init();

    /* Spawn Task 1 */
    sched_create_task(1, task1_stack, sizeof(task1_stack), task1_entry);

    uart_puts("zkernel successfully booted on RP2040!\n");

    /* Blinky loop in Task 0 */
    while (1) {
        *SIO_GPIO_OUT = (1 << 25); /* LED On */
        uart_puts("Task 0: LED ON\n");
        sched_yield();
        delay(200000);

        *SIO_GPIO_OUT = 0;          /* LED Off */
        uart_puts("Task 0: LED OFF\n");
        sched_yield();
        delay(200000);
    }

    return 0;
}

