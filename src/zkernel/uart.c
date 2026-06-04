#include "uart.h"

/* RP2040 UART0 MMIO Register Addresses (PL011) */
#define UART0_BASE       0x40034000
#define UART_DR          ((volatile unsigned int *)(UART0_BASE + 0x00))
#define UART_FR          ((volatile unsigned int *)(UART0_BASE + 0x18))
#define UART_IBRD        ((volatile unsigned int *)(UART0_BASE + 0x24))
#define UART_FBRD        ((volatile unsigned int *)(UART0_BASE + 0x28))
#define UART_LCR_H       ((volatile unsigned int *)(UART0_BASE + 0x2c))
#define UART_CR          ((volatile unsigned int *)(UART0_BASE + 0x30))
#define UART_IMSC        ((volatile unsigned int *)(UART0_BASE + 0x38))
#define UART_ICR         ((volatile unsigned int *)(UART0_BASE + 0x44))

/* SIO (Single-cycle IO) GPIO config */
#define padsbank0_BASE   0x4001c000
#define padsbank0_gpio0  ((volatile unsigned int *)(padsbank0_BASE + 0x004))
#define padsbank0_gpio1  ((volatile unsigned int *)(padsbank0_BASE + 0x008))

#define iobank0_BASE     0x40014000
#define iobank0_gpio0_ctrl ((volatile unsigned int *)(iobank0_BASE + 0x004))
#define iobank0_gpio1_ctrl ((volatile unsigned int *)(iobank0_BASE + 0x008))

/* NVIC Interrupt Controllers (Cortex-M0+ NVIC is at 0xe000e100) */
#define NVIC_ISER        ((volatile unsigned int *)0xe000e100)
#define NVIC_ICPR        ((volatile unsigned int *)0xe000e280)

#define UART0_IRQ        20

/* Ring Buffer (64 bytes) */
#define RING_BUF_SIZE 64
static volatile char rx_buf[RING_BUF_SIZE];
static volatile int rx_head = 0;
static volatile int rx_tail = 0;

void uart_init(void) {
    /* Set GPIO0 and GPIO1 pad config to default (enable output, enable input) */
    *padsbank0_gpio0 = 0x00000000;
    *padsbank0_gpio1 = 0x00000000;

    /* Set GPIO0 and GPIO1 function to UART (F2) */
    *iobank0_gpio0_ctrl = 2;
    *iobank0_gpio1_ctrl = 2;

    /* Disable UART0 before configuration */
    *UART_CR = 0;

    /* Set Baud Rate to 115200 (clk_peri is 125 MHz by default on RP2040)
     * Divisor = 125,000,000 / (16 * 115200) = 67.8168
     * IBRD = 67
     * FBRD = integer(0.8168 * 64 + 0.5) = 52
     */
    *UART_IBRD = 67;
    *UART_FBRD = 52;

    /* Set Line Control: 8 bits, 1 stop bit, no parity, enable FIFOs */
    *UART_LCR_H = (3 << 5) | (1 << 4);

    /* Enable UART0: UARTEN, TXE, RXE */
    *UART_CR = (1 << 0) | (1 << 8) | (1 << 9);

    /* Enable RX Interrupt Mask (RXIM) in IMSC */
    *UART_IMSC = (1 << 4);

    /* Enable UART0 IRQ in NVIC (Interrupt 20) */
    *NVIC_ISER = (1 << UART0_IRQ);

    /* Enable global interrupts */
    __asm__ volatile("cpsie i");
}

void uart_putc(char c) {
    /* Wait if Transmit FIFO is Full (TXFF is bit 5 in FR) */
    while ((*UART_FR) & (1 << 5)) {
        /* Busy wait */
    }
    *UART_DR = c;
}

void uart_puts(const char *s) {
    while (*s) {
        if (*s == '\n') {
            uart_putc('\r');
        }
        uart_putc(*s++);
    }
}

int uart_getc(void) {
    char c;
    /* Check if ring buffer is empty */
    if (rx_head == rx_tail) {
        return -1;
    }
    c = rx_buf[rx_tail];
    rx_tail = (rx_tail + 1) % RING_BUF_SIZE;
    return (int)((unsigned char)c);
}

int uart_char_available(void) {
    return (rx_head != rx_tail);
}

void uart0_irq_handler(void) {
    char c;
    int next_head;
    /* Read characters while Receive FIFO is not empty (RXFE is bit 4 in FR) */
    while (!((*UART_FR) & (1 << 4))) {
        c = (char)(*UART_DR & 0xFF);
        next_head = (rx_head + 1) % RING_BUF_SIZE;
        if (next_head != rx_tail) {
            rx_buf[rx_head] = c;
            rx_head = next_head;
        }
    }
    /* Clear RX interrupt (RXIC is bit 4 in ICR) */
    *UART_ICR = (1 << 4);
}
