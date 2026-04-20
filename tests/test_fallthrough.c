#include <stdio.h>
#include <string.h>

void pack(unsigned char *zPayload, unsigned long long v, int len) {
    switch( len ){
      default: zPayload[7] = (unsigned char)(v&0xff); v >>= 8;
               zPayload[6] = (unsigned char)(v&0xff); v >>= 8;
      case 6: zPayload[5] = (unsigned char)(v&0xff); v >>= 8;
               zPayload[4] = (unsigned char)(v&0xff); v >>= 8;
      case 4: zPayload[3] = (unsigned char)(v&0xff); v >>= 8;
      case 3: zPayload[2] = (unsigned char)(v&0xff); v >>= 8;
      case 2: zPayload[1] = (unsigned char)(v&0xff); v >>= 8;
      case 1: zPayload[0] = (unsigned char)(v&0xff);
    }
}

int main() {
    unsigned char zPayload[8];
    memset(zPayload, 0xAA, 8); // Fill with garbage (e.g. 0xAA)
    unsigned long long v = 0x40091EB851EB851FULL;
    pack(zPayload, v, 8);
    for(int i=0; i<8; i++) printf("%02x ", zPayload[i]);
    printf("\n");
    return 0;
}
