#include <stdlib.h>

struct Node {
    struct Node *next;
};

void test_reassignment() {
    struct Node *tok = malloc(sizeof(struct Node));
    if (!tok) return; /* scan site safe! */
    
    struct Node *node = tok->next; /* ND_MEMBER dereferencing tok, but 'node' gets unassigned value safely */
    
    /* Now node gets a malloc! */
    node = malloc(sizeof(struct Node));
    /* Scan site A: node is dereferenced WITHOUT check! */
    node->next = 0; 
}

int main() {
    test_reassignment();
    return 0;
}
