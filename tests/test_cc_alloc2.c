typedef struct ArenaBlock {
    char *data;
    int pos;
    int cap;
    struct ArenaBlock *next;
} ArenaBlock;

typedef struct Compiler {
    ArenaBlock *arena;
} Compiler;

void *cc_alloc(Compiler *cc, int size) {
    ArenaBlock *a;
    int block_count;
    if (size == 0) return 0;
    size = (size + 7) & ~7;
    a = cc->arena;
    block_count = 0;
    while (a) {
        if (a->cap >= a->pos + size) {
            void *ret = a->data + a->pos;
            a->pos += size;
            return ret;
        }
        if (!a->next) {
            block_count++;
            {
                int newcap;
                ArenaBlock *nb;
                if (size > newcap) newcap = size * 2;
                nb = (ArenaBlock *)0;
                nb->next = 0;
                a->next = nb;
            }
        }
        a = a->next;
    }
    return 0;
}
