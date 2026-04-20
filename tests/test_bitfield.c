int printf(char *fmt, ...);

struct PacketFlags {
    unsigned int is_syn : 1;
    unsigned int is_ack : 1;
    unsigned int reserved : 6;
    unsigned int sequence : 24;
};

int main() {
    struct PacketFlags p;
    unsigned int *raw = (unsigned int *)&p;
    *raw = 0;

    p.is_syn = 1;
    p.is_ack = 1;
    p.sequence = 0xABCD;

    printf("raw: %X\n", *raw);
    printf("is_syn: %u\n", p.is_syn);
    printf("is_ack: %u\n", p.is_ack);
    printf("reserved: %u\n", p.reserved);
    printf("sequence: %X\n", p.sequence);
    
    if (p.is_syn == 1 && p.is_ack == 1 && p.sequence == 0xABCD && p.reserved == 0) {
        printf("PASS\n");
    } else {
        printf("FAIL\n");
    }
    return 0;
}
