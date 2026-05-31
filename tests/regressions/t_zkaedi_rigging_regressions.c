/* 🔱 ZKAEDI VMAX: Compiler Regression Test Suite
 *
 * Targets 5 concrete compiler and asset pipeline regression paths:
 *   1. packed struct alignment for bone matrices
 *   2. float/double preservation for Voronoi offsets
 *   3. deterministic serialization of .meta rig coordinates
 *   4. rejection tests for NaN/Inf transform fields
 *   5. bounds checks for asset-derived buffer sizes
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── 1. packed struct alignment for bone matrices ───────────────────────── */
struct __attribute__((packed)) PackedBoneMatrix {
    float matrix[16];   /* 64 bytes */
    char boneName[32];  /* 32 bytes */
    short boneIndex;    /* 2 bytes */
};

/* Self-contained offset check to avoid stddef.h dependencies */
#define offsetof_custom(s, m) ((size_t)&(((s*)0)->m))

int test_packed_struct_alignment() {
    size_t expected_size = 64 + 32 + 2; // 98 bytes
    if (sizeof(struct PackedBoneMatrix) != expected_size) {
        printf("FAIL: packed struct alignment size was %zu, expected %zu\n", sizeof(struct PackedBoneMatrix), expected_size);
        return 0;
    }
    // Verify static offset compiler layout is correct
    if (offsetof_custom(struct PackedBoneMatrix, boneIndex) != 96) {
        printf("FAIL: packed struct field offset of boneIndex was %zu, expected 96\n", offsetof_custom(struct PackedBoneMatrix, boneIndex));
        return 0;
    }
    return 1;
}

/* ── 2. float/double preservation for Voronoi offsets ───────────────────── */
double compute_distance_double(double x1, double y1, double z1, double x2, double y2, double z2) {
    double dx = x1 - x2;
    double dy = y1 - y2;
    double dz = z1 - z2;
    return dx * dx + dy * dy + dz * dz;
}

float compute_distance_float(float x1, float y1, float z1, float x2, float y2, float z2) {
    float dx = x1 - x2;
    float dy = y1 - y2;
    float dz = z1 - z2;
    return dx * dx + dy * dy + dz * dz;
}

int test_float_double_preservation() {
    /* Use exact powers of 2 for bit-level floating point precision check */
    double d1 = compute_distance_double(1.0 + 1.0/16777216.0, 2.0, 3.0, 1.0, 2.0, 3.0); /* 2^-24 offset */
    float f1 = compute_distance_float(1.0f + 1.0f/2048.0f, 2.0f, 3.0f, 1.0f, 2.0f, 3.0f); /* 2^-11 offset */
    
    double expected_d = 3.552713678800501e-15; /* 2^-48 */
    double err_d = d1 - expected_d;
    if (err_d < 0.0) err_d = -err_d;
    if (err_d > 1e-28) {
        printf("FAIL: double precision offset accuracy collapsed (val: %e, expected: %e)\n", d1, expected_d);
        return 0;
    }

    float expected_f = 2.38418579e-7f; /* 2^-22 */
    float err_f = f1 - expected_f;
    if (err_f < 0.0f) err_f = -err_f;
    if (err_f > 1e-14f) {
        printf("FAIL: float precision offset accuracy collapsed (val: %e, expected: %e)\n", f1, expected_f);
        return 0;
    }
    return 1;
}

/* ── 3. deterministic serialization of .meta rig coordinates ────────────── */
long long round_fixed(double val) {
    // Correct rounding nearest-away, avoiding raw truncation problems
    return (long long)(val >= 0.0 ? val * 1000000.0 + 0.5 : val * 1000000.0 - 0.5);
}

void serialize_coordinate_fixed(double val, char* out_buf) {
    long long fixed_val = round_fixed(val);
    sprintf(out_buf, "%lld", fixed_val);
}

int test_deterministic_serialization() {
    char buf1[64];
    char buf2[64];
    
    double val1 = 0.450000123;
    double val2 = 0.45000012300000005; // tiny FP variance

    serialize_coordinate_fixed(val1, buf1);
    serialize_coordinate_fixed(val2, buf2);

    if (strcmp(buf1, buf2) != 0) {
        printf("FAIL: Non-deterministic coordinate serialization detected\n");
        return 0;
    }
    return 1;
}

/* ── 4. rejection tests for NaN/Inf transform fields ────────────────────── */
int validate_transform_field(float val) {
    unsigned int u;
    // Strict aliasing safety check using memcpy
    memcpy(&u, &val, sizeof(u));
    // IEEE-754 Single Precision: NaN or Inf has all 1s in exponent (bits 23-30)
    if ((u & 0x7F800000) == 0x7F800000) {
        return 0; // REJECT: NaN or Infinity
    }
    return 1; // ACCEPT
}

int test_nan_inf_rejection() {
    float clean = 1.0f;
    unsigned int nan_bits = 0x7FC00000;
    float nan_val;
    unsigned int inf_bits = 0x7F800000;
    float inf_val;
    
    memcpy(&nan_val, &nan_bits, sizeof(nan_val));
    memcpy(&inf_val, &inf_bits, sizeof(inf_val));

    if (validate_transform_field(clean) != 1) {
        printf("FAIL: Rejected valid float transformation field\n");
        return 0;
    }
    if (validate_transform_field(nan_val) != 0) {
        printf("FAIL: Failed to reject NaN transformation field\n");
        return 0;
    }
    if (validate_transform_field(inf_val) != 0) {
        printf("FAIL: Failed to reject Infinity transformation field\n");
        return 0;
    }
    return 1;
}

/* ── 5. bounds checks for asset-derived buffer sizes ────────────────────── */
#define MAX_SKELETAL_JOINTS 128

int get_bone_mapping_safe(int index, const int* mappings, int mappings_size) {
    if (index < 0 || index >= mappings_size) {
        return -1; // Out-of-bounds error signature
    }
    if (index >= MAX_SKELETAL_JOINTS) {
        return -2; // Rig limits overflow signature
    }
    return mappings[index];
}

int test_bounds_checks() {
    int mock_mappings[5] = {0, 10, 20, 30, 40};
    
    if (get_bone_mapping_safe(2, mock_mappings, 5) != 20) {
        printf("FAIL: Valid index bounds access failed\n");
        return 0;
    }
    if (get_bone_mapping_safe(-1, mock_mappings, 5) != -1) {
        printf("FAIL: Underflow index bounds check bypassed\n");
        return 0;
    }
    if (get_bone_mapping_safe(5, mock_mappings, 5) != -1) {
        printf("FAIL: Overflow index bounds check bypassed\n");
        return 0;
    }
    if (get_bone_mapping_safe(200, mock_mappings, 5) != -1) {
        printf("FAIL: Extreme overflow index bounds check bypassed\n");
        return 0;
    }
    return 1;
}

/* ── Main Execution Runner ──────────────────────────────────────────────── */
int main(void) {
    printf("🔱 ZKAEDI REGRESSION AUDIT GATE INITIATED\n");
    
    if (!test_packed_struct_alignment()) return 1;
    if (!test_float_double_preservation()) return 2;
    if (!test_deterministic_serialization()) return 3;
    if (!test_nan_inf_rejection()) return 4;
    if (!test_bounds_checks()) return 5;
    
    printf("🔱 ALL 5/5 REGRESSION TESTS PASSED FLAWLESSLY!\n");
    return 0;
}
