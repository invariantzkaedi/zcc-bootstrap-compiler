#include <stdint.h>
struct Node {
    uint8_t type;
    union {
        struct { uint16_t x; uint16_t y; } pt;
        uint32_t raw;
    } data;
};
int main(void) {
    struct Node n = { .type = 4, .data.pt = { .x = (uint16_t)13, .y = (uint16_t)497 } };
    return (uint8_t)((n.data.raw ^ n.type) & 0xFF);
}
