typedef struct Compiler Compiler;
typedef struct ArenaBlock ArenaBlock;
struct ArenaBlock {
    char *data;
    int pos;
    int cap;
    ArenaBlock *next;
};
struct Compiler {
    int alloc_count;
    ArenaBlock arena;
};

void *test_alloc(Compiler *cc, int size) {
    ArenaBlock *a;
    void *p;
    
    a = &cc->arena;
    while (a) {
        if (a->pos + size <= a->cap) {
            p = a->data + a->pos;
            a->pos = a->pos + size;
            
            if (size >= 16) {
                int alloc_id;
                alloc_id = cc->alloc_count;
                cc->alloc_count = cc->alloc_count + 1;
            } else {
                char *cp;
                int i;
                cp = (char *)p;
                for (i = 0; i < size; i++) {
                    cp[i] = 0;
                }
            }
            return p;
        }
        a = a->next;
    }
    return 0;
}
