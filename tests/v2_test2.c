int printf(const char *fmt, ...);
int main() {
    int A = (-969) / (((590) | 1) == 0 ? 1 : ((590) | 1));
    int B = A - 193; // B = -194
    int C = -((-665) << (41 & 15)); // (41 & 15) = 9. -665 << 9 = -340480. -(-340480) = 340480.
    int D = B / ((C | 1) == 0 ? 1 : (C | 1)); // -194 / 340481 = 0
    unsigned int E = D << (2252u & 15); // 0 << 12 = 0
    unsigned int F = E << (((250) % (((-815) | 1) == 0 ? 1 : ((-815) | 1))) & 15); // 0 << 10 = 0
    unsigned int v2 = -(F);
    
    printf("A=%d\nB=%d\nC=%d\nD=%d\nE=%u\nF=%u\nv2=%u\n", A, B, C, D, E, F, v2);

    int G = (((-815) | 1) == 0 ? 1 : ((-815) | 1));
    int H = 250 % G;
    int I = H & 15;
    printf("G=%d\nH=%d\nI=%d\n", G, H, I);
    
    return 0;
}
