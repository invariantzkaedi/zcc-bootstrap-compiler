typedef struct Sym {
    struct Section *section;
} Sym;

typedef struct Section {
    int sh_num;
} Section;

void put(Section *sec) {
    int n = sec->sh_num;
}
