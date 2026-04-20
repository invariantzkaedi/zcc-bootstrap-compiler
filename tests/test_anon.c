typedef struct {
    union {
        long st_value;
    };
    int st_info;
} ElfSym;

int main() {
    ElfSym sym;
    sym.st_value = 1;
    return 0;
}
