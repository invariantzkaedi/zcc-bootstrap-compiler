#include <stdio.h>

typedef struct { long a, b; } Pair;
typedef struct { Pair x, y; double scale; } Node;

Pair pair_make(long a, long b) {
    Pair p = {a, b};
    return p;
}

Node node_make(long seed) {
    Node n;
    n.x = pair_make(seed + 1, seed + 2);
    n.y = pair_make(seed ^ 7, seed ^ 11);
    n.scale = 2.5;
    return n;
}

long consume(Node n) {
    return n.x.a + n.x.b + n.y.a + n.y.b + (long)(n.scale * 1000.0);
}

int main(void) {
    printf("out=%ld\n", consume(node_make(1234)));
    return 0;
}
