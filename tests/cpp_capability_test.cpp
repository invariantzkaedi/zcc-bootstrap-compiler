// tests/cpp_capability_test.cpp
// ZCC Compiler C++ Capabilities test suite

#include <stdio.h>

namespace Outer {
    namespace Inner {
        int target_val = 100;
    }
}

using namespace Outer::Inner;

class Vector3 {
private:
    int x;
    int y;
public:
    int z;

    void set_x(int val) {
        x = val;
    }

    int get_x() {
        return x;
    }
};

int main() {
    using namespace Outer;
    
    // Test 1: Namespace flattening and double colon resolution
    int val = Outer::Inner::target_val;
    if (val != 100) {
        printf("[FAIL] Namespace resolution: expected 100, got %d\n", val);
        return 1;
    }
    printf("[PASS] Namespace resolution: got %d\n", val);

    // Test 2: Class structure parsing and offset calculations
    Vector3 vec;
    vec.z = 30;

    // We can cast using static_cast and reinterpret_cast to verify offsets
    // Private field 'x' is at offset 0, 'y' at offset 4, 'z' at offset 8.
    int* ptr = reinterpret_cast<int*>(&vec);
    ptr[0] = 10;
    ptr[1] = 20;

    if (vec.z != 30) {
        printf("[FAIL] Class member layout: vec.z expected 30, got %d\n", vec.z);
        return 2;
    }
    printf("[PASS] Class member layout (vec.z): got %d\n", vec.z);

    // Test 3: C++ casts
    long long value_64 = 42ULL;
    int value_32 = static_cast<int>(value_64);
    if (value_32 != 42) {
        printf("[FAIL] C++ Cast: expected 42, got %d\n", value_32);
        return 3;
    }
    printf("[PASS] C++ Cast (static_cast): got %d\n", value_32);

    printf("ALL C++ CAPABILITIES VERIFIED! EXITING WITH SUCCESS CODE 42.\n");
    return 42;
}
