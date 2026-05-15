extern int mutate_c_pointer(int ptr_addr, int limit);

int main() {
    volatile int core_memory = 42;
    int res = mutate_c_pointer((int)(long)&core_memory, 10);
    return res & 0xFF;
}
