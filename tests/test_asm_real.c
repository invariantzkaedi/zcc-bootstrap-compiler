int main() {
    asm("nop");
    asm volatile("mov $42, %rax");
    return 0;
}
