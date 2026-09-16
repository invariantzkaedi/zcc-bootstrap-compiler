/* ========================================================================= */
/* ZCC OPTIQPU: PHOTONIC & OPTICAL TENSOR PROCESSING EMITTER                  */
/* ========================================================================= */
/* File: src/optics/optiqpu_emitter.h                                        */
/* Description: Compiles unitary linear transformations and matrix kernels   */
/*              into Mach-Zehnder Interferometer (MZI) optical phase meshes. */
/* ========================================================================= */

#ifndef ZCC_OPTIQPU_EMITTER_H
#define ZCC_OPTIQPU_EMITTER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OPTIQPU_MAX_MODES 32
#define OPTIQPU_MAX_MZIS  512

/* 2x2 Optical Mach-Zehnder Interferometer Unit */
typedef struct {
    uint32_t mode_in_a;
    uint32_t mode_in_b;
    double   theta;          /* Internal phase shift [0, 2*pi] */
    double   phi;            /* External phase shift [0, 2*pi] */
    double   thermal_drift;  /* Calibration adjustment */
} OptiQpuMZI;

typedef struct {
    uint32_t   n_modes;
    uint32_t   n_mzis;
    OptiQpuMZI mzis[OPTIQPU_MAX_MZIS];
    double     insertion_loss_db;
    double     propagation_delay_ps; /* Picoseconds */
} OptiQpuPhotonicMesh;

/* Complex number structure for optical E-field */
typedef struct {
    double real;
    double imag;
} OptiComplex;

/* Decompose unitary matrix into Clements-style MZI mesh */
bool optiqpu_decompose_unitary(
    const OptiComplex *unitary_matrix,
    uint32_t           dim,
    OptiQpuPhotonicMesh *out_mesh
);

/* Apply thermal drift calibration polynomials to phase angles */
void optiqpu_apply_thermal_calibration(OptiQpuPhotonicMesh *mesh, double ambient_temp_c);

/* Simulate optical wavefront forward pass (speed of light propagation) */
void optiqpu_simulate_optical_forward(
    const OptiQpuPhotonicMesh *mesh,
    const OptiComplex         *input_modes,
    OptiComplex               *output_modes
);

/* Emit Photonic hardware control bytecode */
int optiqpu_emit_photonic_control_text(const OptiQpuPhotonicMesh *mesh, char *out_buf, size_t max_len);

#ifdef __cplusplus
}
#endif

#endif /* ZCC_OPTIQPU_EMITTER_H */
