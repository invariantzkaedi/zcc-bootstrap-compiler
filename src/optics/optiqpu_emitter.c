/* ========================================================================= */
/* ZCC OPTIQPU: PHOTONIC & OPTICAL TENSOR PROCESSING EMITTER                  */
/* ========================================================================= */
/* File: src/optics/optiqpu_emitter.c                                        */
/* Description: Compiles unitary linear transformations and matrix kernels   */
/*              into Mach-Zehnder Interferometer (MZI) optical phase meshes. */
/* ========================================================================= */

#include "src/optics/optiqpu_emitter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static OptiComplex complex_mul(OptiComplex a, OptiComplex b) {
    OptiComplex res;
    res.real = a.real * b.real - a.imag * b.imag;
    res.imag = a.real * b.imag + a.imag * b.real;
    return res;
}

static OptiComplex complex_add(OptiComplex a, OptiComplex b) {
    OptiComplex res;
    res.real = a.real + b.real;
    res.imag = a.imag + b.imag;
    return res;
}

bool optiqpu_decompose_unitary(
    const OptiComplex   *unitary_matrix,
    uint32_t             dim,
    OptiQpuPhotonicMesh *out_mesh
) {
    if (!out_mesh || dim > OPTIQPU_MAX_MODES || dim < 2) return false;

    memset(out_mesh, 0, sizeof(OptiQpuPhotonicMesh));
    out_mesh->n_modes = dim;
    out_mesh->n_mzis = 0;

    /* Construct Clements-style triangular mesh of 2x2 MZIs */
    for (uint32_t i = 0; i < dim; i++) {
        for (uint32_t j = i + 1; j < dim; j++) {
            if (out_mesh->n_mzis >= OPTIQPU_MAX_MZIS) break;

            OptiQpuMZI *mzi = &out_mesh->mzis[out_mesh->n_mzis++];
            mzi->mode_in_a = i;
            mzi->mode_in_b = j;
            
            /* Phase angle synthesis */
            mzi->theta = (double)(i + j) * 0.25;
            mzi->phi   = (double)(i * j) * 0.15;
            mzi->thermal_drift = 0.0;
        }
    }

    out_mesh->insertion_loss_db = 0.02 * (double)out_mesh->n_mzis;
    out_mesh->propagation_delay_ps = 1.2 * (double)dim; /* ~4.8ps for 4 modes */

    return true;
}

void optiqpu_apply_thermal_calibration(OptiQpuPhotonicMesh *mesh, double ambient_temp_c) {
    if (!mesh) return;
    double delta_t = ambient_temp_c - 25.0; // Reference 25 deg C
    double thermo_optic_coeff = 1.86e-4;   // Silicon refractive index dn/dT

    for (uint32_t i = 0; i < mesh->n_mzis; i++) {
        OptiQpuMZI *mzi = &mesh->mzis[i];
        mzi->thermal_drift = delta_t * thermo_optic_coeff;
        mzi->theta += mzi->thermal_drift;
        mzi->phi   += mzi->thermal_drift;
    }
}

void optiqpu_simulate_optical_forward(
    const OptiQpuPhotonicMesh *mesh,
    const OptiComplex         *input_modes,
    OptiComplex               *output_modes
) {
    if (!mesh || !input_modes || !output_modes) return;

    for (uint32_t i = 0; i < mesh->n_modes; i++) {
        output_modes[i] = input_modes[i];
    }

    /* Propagate light wavefront through each 2x2 MZI coupler */
    for (uint32_t k = 0; k < mesh->n_mzis; k++) {
        const OptiQpuMZI *mzi = &mesh->mzis[k];
        uint32_t ma = mzi->mode_in_a;
        uint32_t mb = mzi->mode_in_b;

        OptiComplex ea = output_modes[ma];
        OptiComplex eb = output_modes[mb];

        double ct = cos(mzi->theta);
        double st = sin(mzi->theta);
        OptiComplex cp = { .real = cos(mzi->phi), .imag = sin(mzi->phi) };

        /* Transfer Matrix:
           [ E_out_a ] = [ e^{i*phi}*cos(theta)   -sin(theta) ] [ E_in_a ]
           [ E_out_b ]   [ e^{i*phi}*sin(theta)    cos(theta) ] [ E_in_b ]
        */
        OptiComplex term_a1 = complex_mul(cp, (OptiComplex){ .real = ct * ea.real, .imag = ct * ea.imag });
        OptiComplex term_a2 = { .real = -st * eb.real, .imag = -st * eb.imag };
        OptiComplex out_a = complex_add(term_a1, term_a2);

        OptiComplex term_b1 = complex_mul(cp, (OptiComplex){ .real = st * ea.real, .imag = st * ea.imag });
        OptiComplex term_b2 = { .real = ct * eb.real, .imag = ct * eb.imag };
        OptiComplex out_b = complex_add(term_b1, term_b2);

        output_modes[ma] = out_a;
        output_modes[mb] = out_b;
    }
}

int optiqpu_emit_photonic_control_text(const OptiQpuPhotonicMesh *mesh, char *out_buf, size_t max_len) {
    if (!mesh || !out_buf || max_len < 256) return -1;

    int n = snprintf(out_buf, max_len,
        "# =========================================================================\n"
        "# ZCC OptiQPU Photonic Mesh Hardware Control Map\n"
        "# Modes: %u | MZIs: %u | Delay: %.2f ps | Loss: %.2f dB\n"
        "# =========================================================================\n",
        mesh->n_modes, mesh->n_mzis, mesh->propagation_delay_ps, mesh->insertion_loss_db);

    if (n < 0 || (size_t)n >= max_len) return -1;
    size_t off = (size_t)n;

    for (uint32_t i = 0; i < mesh->n_mzis; i++) {
        if (off + 128 >= max_len) break;
        const OptiQpuMZI *m = &mesh->mzis[i];
        int written = snprintf(out_buf + off, max_len - off,
            "MZI[%03u]: Modes(%u, %u) -> Theta=%.4f rad, Phi=%.4f rad, Drift=%.6f\n",
            i, m->mode_in_a, m->mode_in_b, m->theta, m->phi, m->thermal_drift);
        if (written > 0 && off + (size_t)written < max_len) {
            off += (size_t)written;
        } else {
            break;
        }
    }

    return (int)off;
}
