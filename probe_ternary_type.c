#include <stdio.h>
int main(void) {
    int cond = 1;
    double t1 = cond ? 0.1 : 0.2;
    double t2 = cond ? 0.1 : 0.2f;
    double t3 = cond ? 0.1f : 0.2;
    int c2 = 0;
    double t4 = c2 ? 0.1 : 0.2f;
    printf("t1 = %.20f\n", t1);
    printf("t2 = %.20f\n", t2);
    printf("t3 = %.20f\n", t3);
    printf("t4 = %.20f\n", t4);
    return 0;
}
