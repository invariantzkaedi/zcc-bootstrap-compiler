#include <stdio.h>
#include <string.h>
typedef unsigned long long u64;
typedef unsigned char u8;
int main() {
    double r = 3.14;
    u64 v;
    memcpy(&v, &r, sizeof(v));
    printf("%llx\n", v);
    u8 zPayload[8];
    zPayload[7] = (u8)(v&0xff); v >>= 8;
    zPayload[6] = (u8)(v&0xff); v >>= 8;
    zPayload[5] = (u8)(v&0xff); v >>= 8;
    zPayload[4] = (u8)(v&0xff); v >>= 8;
    zPayload[3] = (u8)(v&0xff); v >>= 8;
    zPayload[2] = (u8)(v&0xff); v >>= 8;
    zPayload[1] = (u8)(v&0xff); v >>= 8;
    zPayload[0] = (u8)(v&0xff);
    for(int i=0; i<8; i++) printf("%02x ", zPayload[i]);
    printf("\n");
    return 0;
}