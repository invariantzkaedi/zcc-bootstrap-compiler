int printf(const char *fmt, ...);

struct PacketFlags {
    unsigned int is_syn : 1;
    unsigned int is_ack : 1;
    unsigned int reserved : 6;
    unsigned int sequence : 24;
};

union PacketWord {
    struct PacketFlags p;
    unsigned int raw;
};

int main(void) {
    union PacketWord u;
    u.raw = 0;

    u.p.is_syn = 1;
    u.p.is_ack = 1;
    u.p.reserved = 0;
    u.p.sequence = 0xABCD;

    printf("raw: %X\n", u.raw);
    printf("is_syn: %u\n", u.p.is_syn);
    printf("is_ack: %u\n", u.p.is_ack);
    printf("reserved: %u\n", u.p.reserved);
    printf("sequence: %X\n", u.p.sequence);
    
    if (u.p.is_syn == 1 && u.p.is_ack == 1 && u.p.sequence == 0xABCD && u.p.reserved == 0) {
        printf("PASS\n");
    } else {
        printf("FAIL\n");
    }
    return 0;
}
