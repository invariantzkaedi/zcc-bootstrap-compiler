#ifndef __size_t__
#define __size_t__
typedef __SIZE_TYPE__ size_t;
#endif
#define NULL 0
#define EOF (-1)
#define BUFSIZ 8192
typedef struct _FILE FILE;
extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;
FILE* fopen(const char*, const char*);
int fclose(FILE*);
size_t fread(void*, size_t, size_t, FILE*);
size_t fwrite(const void*, size_t, size_t, FILE*);
int fprintf(FILE*, const char*, ...);
int printf(const char*, ...);
int sprintf(char*, const char*, ...);
int snprintf(char*, size_t, const char*, ...);
int vfprintf(FILE*, const char*, void*);
int fflush(FILE*);
int fseek(FILE*, long, int);
long ftell(FILE*);
int remove(const char*);
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
int feof(FILE*);
int getc(FILE*);
int ferror(FILE*);
void clearerr(FILE*);
int ungetc(int, FILE*);
char* fgets(char*, int, FILE*);
int fputs(const char*, FILE*);
FILE* tmpfile(void);
int setvbuf(FILE*, char*, int, size_t);
int rename(const char*, const char*);
char* tmpnam(char*);
#define _IONBF 0
#define _IOLBF 1
#define _IOFBF 2
FILE *popen(const char *command, const char *type);
int pclose(FILE *stream);
void flockfile(FILE *filehandle);
void funlockfile(FILE *filehandle);
int getc_unlocked(FILE *stream);
int fseeko(FILE *stream, long offset, int whence);
long ftello(FILE *stream);
