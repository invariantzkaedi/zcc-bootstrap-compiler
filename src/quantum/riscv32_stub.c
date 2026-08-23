/*
 * RISC-V 32-bit Bare-Metal zk-VM Environment Stub
 * Provides minimal system stubs and stdio streams for riscv32im zk-VM guest execution.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

static int zk_putc(char c, FILE *file) {
    (void)file;
    return (int)(unsigned char)c;
}

static FILE __zk_stdio_file = FDEV_SETUP_STREAM(zk_putc, NULL, NULL, _FDEV_SETUP_WRITE);
FILE *const stdout = &__zk_stdio_file;
FILE *const stderr = &__zk_stdio_file;

void _exit(int status) {
    (void)status;
    while (1) {
        #if defined(__riscv)
        __asm__ volatile("ebreak");
        #endif
    }
}

int _write(int file, char *ptr, int len) {
    (void)file; (void)ptr;
    return len;
}

int _close(int file) { (void)file; return -1; }
int _fstat(int file, void *st) { (void)file; (void)st; return 0; }
int _isatty(int file) { (void)file; return 1; }
int _lseek(int file, int ptr, int dir) { (void)file; (void)ptr; (void)dir; return 0; }
int _read(int file, char *ptr, int len) { (void)file; (void)ptr; (void)len; return 0; }

static uint8_t heap_mem[64 * 1024];
static size_t heap_ptr = 0;

void *_sbrk(ptrdiff_t incr) {
    if (heap_ptr + incr > sizeof(heap_mem)) {
        return (void *)-1;
    }
    void *prev = &heap_mem[heap_ptr];
    heap_ptr += incr;
    return prev;
}
