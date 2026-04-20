int printf(char *fmt, ...);

int main() {
    int i = 0;
    while (i < 5) {
        int v_size = i + 10;
        char vla[v_size];
        vla[0] = 'A' + i;
        vla[v_size - 1] = 'Z' - i;
        printf("iter %d: vla size %d, first=%c, last=%c\n", i, v_size, vla[0], vla[v_size - 1]);
        if (i == 2) {
            printf("breaking early at i=2\n");
            break;
        }
        i++;
    }

    for (int j = 0; j < 3; j++) {
        int v_size = 5;
        char vla2[v_size];
        vla2[0] = 'F';
        if (j == 1) {
            printf("continuing at j=1\n");
            continue;
        }
        printf("j=%d\n", j);
    }
    
    printf("PASS\n");
    return 0;
}
