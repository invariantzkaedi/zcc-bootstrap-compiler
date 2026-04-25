struct Packet {
    int len;
    char data[];
};

_Static_assert(sizeof(struct Packet) == sizeof(int), "flexible array contributes no payload size");
_Static_assert(_Alignof(struct Packet) == _Alignof(int), "flexible array preserves alignment");

int main(void) {
    return 0;
}