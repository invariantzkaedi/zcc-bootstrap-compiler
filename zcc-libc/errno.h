#ifndef _ERRNO_H
#define _ERRNO_H
extern int *__errno_location(void);
#define errno (*__errno_location())
#define ERANGE 34
#define EDOM 33
#define EILSEQ 84
#endif
