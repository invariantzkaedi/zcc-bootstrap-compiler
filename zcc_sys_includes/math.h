#ifndef _MATH_H
#define _MATH_H
#define HUGE_VAL     (1.0/0.0)
#define HUGE_VALF    (1.0f/0.0f)
#define INFINITY     (1.0f/0.0f)
#define NAN          (0.0f/0.0f)
double pow(double x, double y);
double sqrt(double x);
double sin(double x);
double cos(double x);
double tan(double x);
double asin(double x);
double acos(double x);
double atan(double x);
double atan2(double y, double x);
double exp(double x);
double log(double x);
double log2(double x);
double log10(double x);
double floor(double x);
double ceil(double x);
double fabs(double x);
double fmod(double x, double y);
double modf(double x, double *iptr);
double frexp(double x, int *exp);
double ldexp(double x, int exp);
double sinh(double x);
double cosh(double x);
double tanh(double x);
int    isnan(double x);
int    isinf(double x);
int    isfinite(double x);
/* Float variants — required by Csmith and general C code */
float  ldexpf(float x, int exp);
float  frexpf(float x, int *exp);
float  fabsf(float x);
float  sqrtf(float x);
float  powf(float x, float y);
float  floorf(float x);
float  ceilf(float x);
float  fmodf(float x, float y);
float  expf(float x);
float  logf(float x);
float  log2f(float x);
float  log10f(float x);
float  sinf(float x);
float  cosf(float x);
float  tanf(float x);
float  asinf(float x);
float  acosf(float x);
float  atanf(float x);
float  atan2f(float y, float x);
float  sinhf(float x);
float  coshf(float x);
float  tanhf(float x);
double round(double x);
float  roundf(float x);
double trunc(double x);
float  truncf(float x);
double fmin(double x, double y);
double fmax(double x, double y);
float  fminf(float x, float y);
float  fmaxf(float x, float y);
#endif
