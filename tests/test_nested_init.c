#include <stdio.h>
#include <stdlib.h>

/* Global nested array initialization */
int matrix[2][2] = {
    {1, 2},
    {3, 4}
};

/* Global nested struct initialization */
struct Point {
    int x;
    int y;
};

struct Rectangle {
    struct Point top_left;
    struct Point bottom_right;
};

struct Rectangle rects[2] = {
    { {10, 20}, {30, 40} },
    { {50, 60}, {70, 80} }
};

int check_globals() {
    int errors = 0;
    if (matrix[0][0] != 1) errors++;
    if (matrix[0][1] != 2) errors++;
    if (matrix[1][0] != 3) errors++;
    if (matrix[1][1] != 4) errors++;



    return errors;
}

int check_locals() {
    int errors = 0;
    
    /* Local nested array */
    int local_matrix[2][2] = {
        {7, 8},
        {9, 10}
    };
    
    if (local_matrix[0][0] != 7) errors++;
    if (local_matrix[0][1] != 8) errors++;
    if (local_matrix[1][0] != 9) errors++;
    if (local_matrix[1][1] != 10) errors++;
    
    
    return errors;
}

int main() {
    int err1 = check_globals();
    int err2 = check_locals();
    
    if (err1 == 0 && err2 == 0) {
        printf("PASS: Nested initializers work perfectly.\n");
        return 0;
    } else {
        printf("FAIL: Global errors: %d, Local errors: %d\n", err1, err2);
        return 1;
    }
}
