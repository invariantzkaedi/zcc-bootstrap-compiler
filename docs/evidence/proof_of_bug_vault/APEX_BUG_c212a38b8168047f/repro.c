#include <stdint.h>
struct Node {
    uint8_t type;
    union {
        struct { uint16_t x; uint16_t y; } pt;
        uint32_t raw;
    } data;
};
int main(void) {
    struct Node n = { .type = 0, .data.pt = { .x = (uint16_t)65, .y = (uint16_t)-861 } };
    return (uint8_t)((n.data.raw ^ n.type) & 0xFF);
}
