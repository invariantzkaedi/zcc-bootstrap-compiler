struct B {
    unsigned x : 3;
    unsigned y : 5;
};

int main(void) {
    return sizeof(struct B);
}