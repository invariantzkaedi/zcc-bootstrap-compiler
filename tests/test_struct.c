typedef struct ArenaBlock ArenaBlock;
struct ArenaBlock {
    char *data;
    int pos;
    int cap;
    ArenaBlock *next;
};
int main() { return 0; }
