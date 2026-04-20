typedef unsigned long size_t;


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







typedef struct {
    unsigned int gp_offset;
    unsigned int fp_offset;
    void *overflow_arg_area;
    void *reg_save_area;
} __va_list_tag;

typedef __va_list_tag va_list[1];
typedef va_list __gnuc_va_list;

int sum_args(int count, ...) {
    va_list ap;
    int total = 0;
    int i;
    __builtin_va_start(ap, count);
    for (i = 0; i < count; i++) {
        total += (*(int *)((char *)(ap)[0].reg_save_area + ((ap)[0].gp_offset += 8, (ap)[0].gp_offset - 8)));
    }
    __builtin_va_end(ap);
    return total;
}

char *get_str(int dummy, ...) {
    va_list ap;
    __builtin_va_start(ap, dummy);
    char *s = (*(char* *)((char *)(ap)[0].reg_save_area + ((ap)[0].gp_offset += 8, (ap)[0].gp_offset - 8)));
    __builtin_va_end(ap);
    return s;
}

int main() {
    int s = sum_args(3, 10, 20, 30);
    printf("sum = %d\n", s);
    char *msg = get_str(0, "hello world");
    printf("str = %s\n", msg);
    return 0;
}
