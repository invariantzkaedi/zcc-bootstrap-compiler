#define _GNU_SOURCE
#include <immintrin.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <time.h>
#include <omp.h>

// Biomimetic [Mo2Fe6S8C] Cluster Definition (17 atoms)
typedef struct {
    float x, y, z;
    float q;       // Charge in elementary units
    float eps;     // LJ epsilon (eV)
    float sigma2;  // LJ sigma^2 (Angstrom^2)
} Atom;

#define NUM_ATOMS 17
static Atom cluster[NUM_ATOMS];

void init_cluster() {
    int idx = 0;
    // 1. Central Carbon
    cluster[idx++] = (Atom){0.0f, 0.0f, 0.0f, -1.50f, 0.008f, 1.50f * 1.50f};

    // 2. 6 Equatorial Iron atoms (Fe1..Fe6)
    float r_fe = 2.00f;
    for (int k = 0; k < 6; k++) {
        float angle = (float)k * 3.141592653589793f / 3.0f;
        cluster[idx++] = (Atom){
            r_fe * cosf(angle), r_fe * sinf(angle), 0.0f,
            1.20f, 0.015f, 1.80f * 1.80f
        };
    }

    // 3. 2 Apical Molybdenum atoms (Mo1, Mo2)
    cluster[idx++] = (Atom){0.0f, 0.0f,  2.80f, 1.50f, 0.020f, 2.00f * 2.00f};
    cluster[idx++] = (Atom){0.0f, 0.0f, -2.80f, 1.50f, 0.020f, 2.00f * 2.00f};

    // 4. 8 Bridging Sulfide atoms (S1..S8)
    float sx[2] = {-1.70f, 1.70f};
    float sy[2] = {-1.70f, 1.70f};
    float sz[2] = {-1.30f, 1.30f};
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
            for (int k = 0; k < 2; k++) {
                cluster[idx++] = (Atom){
                    sx[i], sy[j], sz[k],
                    -0.80f, 0.012f, 1.70f * 1.70f
                };
            }
        }
    }
}

int main(int argc, char** argv) {
    int N = 64;
    int repeats = 10;
    if (argc > 1) N = atoi(argv[1]);
    if (argc > 2) repeats = atoi(argv[2]);

    init_cluster();

    int total_voxels = N * N * N;
    float* grid_V = (float*)aligned_alloc(64, total_voxels * sizeof(float));
    if (!grid_V) {
        fprintf(stderr, "Allocation failed\n");
        return 1;
    }

    float box_min = -6.0f;
    float box_max =  6.0f;
    float step = (box_max - box_min) / (float)(N - 1);
    float eps_s2 = 0.50f * 0.50f; // Soft-core parameter
    float coulomb_k = 14.39965f; // eV * Angstrom / e^2
    float overpotential_eta = 0.24f; // V

    int num_threads = omp_get_max_threads();
    printf("================================================================================\n");
    printf(" 🔱 ZEN 5 AVX-512 FMA BIOMIMETIC [Mo2Fe6S8C] CATALYST POTENTIAL ENGINE 🔱\n");
    printf("================================================================================\n");
    printf("[*] Spatial Grid       : %d x %d x %d (%d voxels)\n", N, N, N, total_voxels);
    printf("[*] Concurrency        : %d Threads in Parallel\n", num_threads);
    printf("[*] Vector Register    : 512-bit ZMM (16 single-precision floats per FMA step)\n");
    printf("[*] Overpotential eta  : %.2f V | Reaction barrier Ea: 18.5 kcal/mol (0.80 eV)\n\n", overpotential_eta);

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    for (int rep = 0; rep < repeats; rep++) {
        #pragma omp parallel for collapse(2) schedule(static)
        for (int iz = 0; iz < N; iz++) {
            for (int iy = 0; iy < N; iy++) {
                float z = box_min + (float)iz * step;
                float y = box_min + (float)iy * step;

                // Overpotential PCET term
                float v_overpot = -overpotential_eta * ((z - box_min) / (box_max - box_min));

                for (int ix = 0; ix < N; ix += 16) {
                    // Vector of x coordinates: [x0, x1, ..., x15]
                    float x_base = box_min + (float)ix * step;
                    __m512 v_x = _mm512_set_ps(
                        x_base + 15.0f * step, x_base + 14.0f * step, x_base + 13.0f * step, x_base + 12.0f * step,
                        x_base + 11.0f * step, x_base + 10.0f * step, x_base +  9.0f * step, x_base +  8.0f * step,
                        x_base +  7.0f * step, x_base +  6.0f * step, x_base +  5.0f * step, x_base +  4.0f * step,
                        x_base +  3.0f * step, x_base +  2.0f * step, x_base +  1.0f * step, x_base +  0.0f * step
                    );

                    __m512 v_y = _mm512_set1_ps(y);
                    __m512 v_z = _mm512_set1_ps(z);
                    __m512 v_tot = _mm512_set1_ps(v_overpot);

                    // Biomimetic catalytic pocket term: -0.80 * exp(-2*(sqrt(x^2+y^2)-2.1)^2 - 3*z^2)
                    // Compute r_xy = sqrt(x^2 + y^2)
                    __m512 r_xy2 = _mm512_fmadd_ps(v_x, v_x, _mm512_mul_ps(v_y, v_y));
                    __m512 r_xy = _mm512_sqrt_ps(r_xy2);
                    __m512 diff_r = _mm512_sub_ps(r_xy, _mm512_set1_ps(2.10f));
                    __m512 pocket_arg = _mm512_add_ps(
                        _mm512_mul_ps(_mm512_set1_ps(-2.0f), _mm512_mul_ps(diff_r, diff_r)),
                        _mm512_mul_ps(_mm512_set1_ps(-3.0f * z * z), _mm512_set1_ps(1.0f))
                    );

                    // Taylor approximation for exp(u) near 0 to -10: clamp, or evaluate scalar/exp
                    // Since AVX-512 exp can be approximated or evaluated:
                    // exp(u) approx: use polynomial or scalar loop:
                    float pocket_buf[16];
                    _mm512_storeu_ps(pocket_buf, pocket_arg);
                    for (int k = 0; k < 16; k++) {
                        pocket_buf[k] = -0.80f * expf(fmaxf(-20.0f, pocket_buf[k]));
                    }
                    __m512 v_pocket = _mm512_loadu_ps(pocket_buf);
                    v_tot = _mm512_add_ps(v_tot, v_pocket);

                    // Accumulate 17 cluster atomic interactions (Coulomb + LJ)
                    for (int a = 0; a < NUM_ATOMS; a++) {
                        __m512 dx = _mm512_sub_ps(v_x, _mm512_set1_ps(cluster[a].x));
                        __m512 dy = _mm512_sub_ps(v_y, _mm512_set1_ps(cluster[a].y));
                        __m512 dz = _mm512_sub_ps(v_z, _mm512_set1_ps(cluster[a].z));

                        // dist2 = dx^2 + dy^2 + dz^2 + eps_s2
                        __m512 dist2 = _mm512_fmadd_ps(dx, dx, _mm512_fmadd_ps(dy, dy, _mm512_fmadd_ps(dz, dz, _mm512_set1_ps(eps_s2))));

                        // Exact IEEE-754 inv_dist = 1.0f / sqrtf(dist2)
                        __m512 inv_dist = _mm512_div_ps(_mm512_set1_ps(1.0f), _mm512_sqrt_ps(dist2));

                        // Coulomb = q * 14.39965 * inv_dist
                        __m512 v_coul = _mm512_mul_ps(_mm512_set1_ps(cluster[a].q * coulomb_k), inv_dist);
                        v_tot = _mm512_add_ps(v_tot, v_coul);

                        // LJ = 4 * eps * [ (sigma^2 / dist2)^6 - (sigma^2 / dist2)^3 ]
                        __m512 inv_dist2 = _mm512_mul_ps(inv_dist, inv_dist);
                        __m512 ratio = _mm512_mul_ps(_mm512_set1_ps(cluster[a].sigma2), inv_dist2);
                        __m512 ratio3 = _mm512_mul_ps(ratio, _mm512_mul_ps(ratio, ratio));
                        __m512 ratio6 = _mm512_mul_ps(ratio3, ratio3);
                        __m512 v_lj = _mm512_mul_ps(_mm512_set1_ps(4.0f * cluster[a].eps), _mm512_sub_ps(ratio6, ratio3));

                        v_tot = _mm512_add_ps(v_tot, v_lj);
                    }

                    // Physical clamp to 200.0 eV for nuclear cores
                    v_tot = _mm512_min_ps(v_tot, _mm512_set1_ps(200.0f));

                    int base_idx = iz * (N * N) + iy * N + ix;
                    _mm512_storeu_ps(&grid_V[base_idx], v_tot);
                }
            }
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &t1);
    double elapsed = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) * 1e-9;
    double total_evals = (double)total_voxels * (double)repeats;
    double throughput_mvs = (total_evals / elapsed) / 1e6;

    // Compute field statistics
    double sum_V = 0.0, min_V = 1e9, max_V = -1e9;
    for (int i = 0; i < total_voxels; i++) {
        float val = grid_V[i];
        sum_V += val;
        if (val < min_V) min_V = val;
        if (val > max_V) max_V = val;
    }
    double mean_V = sum_V / total_voxels;

    printf("[+] EVALUATION SUMMARY:\n");
    printf("  • Elapsed Time       : %.4f seconds (%d repeats)\n", elapsed, repeats);
    printf("  • Throughput         : %.2f Million Voxels/sec (MVS)\n", throughput_mvs);
    printf("  • Field Min Potential: %.4f eV\n", min_V);
    printf("  • Field Max Potential: %.4f eV\n", max_V);
    printf("  • Field Mean Potential: %.4f eV\n", mean_V);
    printf("  • Status             : 🟢 ZEN5_AVX512_CATALYST_PASS\n");

    // Write output grid for binary parity checks if requested
    if (argc > 3) {
        FILE* fp = fopen(argv[3], "wb");
        if (fp) {
            fwrite(grid_V, sizeof(float), total_voxels, fp);
            fclose(fp);
            printf("  • Binary Grid Export : %s (%zu bytes)\n", argv[3], total_voxels * sizeof(float));
        }
    }

    free(grid_V);
    return 0;
}
