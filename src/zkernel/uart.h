#ifndef UART_H
#define UART_H

void uart_init(void);
void uart_putc(char c);
void uart_puts(const char *s);
int uart_getc(void);
int uart_char_available(void);
void uart0_irq_handler(void);

#endif /* UART_H */
