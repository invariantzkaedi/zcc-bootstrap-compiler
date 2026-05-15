int main() {
    short a = -5;
    short b = 10;
    short c;

    if (a < 0) {
        c = a;
    } else {
        c = b;
    }

    // In an SSA conversion, 'c' becomes a PHI node: c_1 = phi(a_1, b_1)
    // The width of the PHI node MUST be preserved as 'short' (16-bit signed).
    // If it defaults to 32-bit or 64-bit without sign-extension metadata,
    // the semantic identity is destroyed during register allocation or spilling.
    return c & 255;
}
