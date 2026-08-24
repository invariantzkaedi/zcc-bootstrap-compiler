
#include <stdio.h>
#include "../src/win64_pe_emit.h"

int main() {
    uint32_t a1 = win64_pe_align_to(500, 512);
    uint32_t a2 = win64_pe_align_to(4096, 4096);
    uint32_t a3 = win64_pe_align_to(4097, 4096);
    printf("ALIGN_500_512:%u\n", a1);
    printf("ALIGN_4096_4096:%u\n", a2);
    printf("ALIGN_4097_4096:%u\n", a3);

    uint8_t code_payload[] = { 0xB8, 0x2A, 0x00, 0x00, 0x00, 0xC3 }; /* mov eax, 42; ret */
    int res = zcc_emit_win64_pe_file("/tmp/test_win64_out.exe", code_payload, sizeof(code_payload));
    printf("PE_EMIT_RES:%d\n", res);
    return 0;
}
