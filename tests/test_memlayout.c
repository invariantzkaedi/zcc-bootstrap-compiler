#include <stdio.h>
#include "sqlite3.h"

int main() {
    sqlite3_value v;
    printf("sizeof(sqlite3_value)=%lu\n", sizeof(v));
    printf("offset u=%lu\n", (char*)&v.u - (char*)&v);
    printf("offset u.r=%lu\n", (char*)&v.u.r - (char*)&v);
    printf("offset u.i=%lu\n", (char*)&v.u.i - (char*)&v);
    return 0;
}
