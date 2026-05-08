#ifndef _STDARG_H
#define _STDARG_H

typedef struct {
    unsigned int gp_offset;
    unsigned int fp_offset;
    void *overflow_arg_area;
    void *reg_save_area;
} __va_list_struct[1];

typedef __va_list_struct va_list;

void __builtin_va_start(va_list ap, ...);
void __builtin_va_end(va_list ap);

#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_end(ap) __builtin_va_end(ap)
#define va_arg(ap, type) __builtin_va_arg(ap, type)
#define va_copy(dest, src) ((dest)[0] = (src)[0])

#endif
