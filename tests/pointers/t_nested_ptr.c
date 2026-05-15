/* Regression: int** double dereference. */
int main() {
    int val = 77;
    int *p = &val;
    int **pp = &p;
    return **pp;  /* 77 */
}
