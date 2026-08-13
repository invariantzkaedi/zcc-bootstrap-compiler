/* complex.h — C11 Complex Arithmetic Specification Header */
#ifndef _ZCC_COMPLEX_H
#define _ZCC_COMPLEX_H

#define complex _Complex
#define _Complex_I (1.0fi)
#define I _Complex_I

#define creal(z) __builtin_creal(z)
#define crealf(z) __builtin_crealf(z)
#define creall(z) __builtin_creall(z)

#define cimag(z) __builtin_cimag(z)
#define cimagf(z) __builtin_cimagf(z)
#define cimagl(z) __builtin_cimagl(z)

#define conj(z) __builtin_conj(z)
#define conjf(z) __builtin_conjf(z)
#define conjl(z) __builtin_conjl(z)

#endif /* _ZCC_COMPLEX_H */
