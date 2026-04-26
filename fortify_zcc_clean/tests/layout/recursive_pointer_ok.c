struct Node {
    struct Node *next;
    int value;
};

_Static_assert(sizeof(struct Node) >= sizeof(void *) + sizeof(int), "node has pointer and int");

int main(void) {
    return 0;
}