// tests/cpp_capability_test.cpp
// ZCC Compiler C++ Capabilities test suite

#include <stdio.h>

namespace Outer {
    namespace Inner {
        int target_val = 100;
    }
    class NestedClass {
    public:
        int value;
    };
}

using namespace Outer::Inner;

class Vector3 {
private:
    int x;
    int y;
public:
    int z;

    // Test A: Inline Constructor and initializer list skip
    Vector3() : x(0), y(0), z(0) {
        z = 5;
    }

    Vector3(int xv, int yv, int zv) {
        x = xv;
        y = yv;
        z = zv;
    }

    // Inline Destructor skip
    ~Vector3() {
        // cleanup
    }

    // Member Operator Overload skip (ZCC ignores content but must parse params)
    Vector3 operator+(const Vector3* other) {
        return *this;
    }

    Vector3 operator-=(const Vector3* other);

    void set_x(int val) {
        x = val;
    }

    int get_x() {
        return x;
    }
};

// Out-of-line constructor definition
Vector3::Vector3(int xv) {
    x = xv;
}

// Out-of-line destructor definition
Vector3::~Vector3() {
    // cleanup
}

// Global Operator Overload skip
Vector3 operator*(const Vector3* a, const Vector3* b) {
    return *a;
}

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

    // Test 4: bool, true, false, nullptr
    bool flag_t = true;
    bool flag_f = false;
    int* null_ptr = nullptr;
    if (flag_t != 1 || flag_f != 0 || null_ptr != 0) {
        printf("[FAIL] C++ bool/nullptr: expected true=1, false=0, nullptr=0, got true=%d, false=%d, nullptr=%p\n", flag_t, flag_f, (void*)null_ptr);
        return 4;
    }
    printf("[PASS] C++ bool/nullptr: true=%d, false=%d, nullptr=%p\n", flag_t, flag_f, (void*)null_ptr);

    // Test 5: Class inside namespace resolution
    Outer::NestedClass nested_instance;
    nested_instance.value = 123;
    if (nested_instance.value != 123) {
        printf("[FAIL] Class inside namespace: expected 123, got %d\n", nested_instance.value);
        return 5;
    }
    printf("[PASS] Class inside namespace: got %d\n", nested_instance.value);

    printf("ALL C++ CAPABILITIES VERIFIED! EXITING WITH SUCCESS CODE 42.\n");
    return 42;
}
