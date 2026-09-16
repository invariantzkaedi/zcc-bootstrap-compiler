/* ========================================================================= */
/* ZCC CELESTIAL NBODY: RELATIVISTIC SYMPLECTIC RK8 ENGINE (C1-C5)           */
/* ========================================================================= */
/* File: src/physics/zcc_celestial_nbody.h                                   */
/* Description: Symplectic Relativistic N-Body Gravitational Engine:         */
/*              C1: 8th-Order Symplectic Energy-Conserving Integrator        */
/*              C2: 1PN & 2.5PN Relativistic Gravitational Wave Damping      */
/*              C3: Vectorized AVX2/AVX-512 Inter-Body Pairwise Kernel       */
/*              C4: Conserved Invariants (H, P, L) & Lyapunov Exponent Engine*/
/*              C5: Native Gravitational JIT Orbit Stream Emitter            */
/* ========================================================================= */

#ifndef ZCC_CELESTIAL_NBODY_H
#define ZCC_CELESTIAL_NBODY_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CELESTIAL_MAX_BODIES   64
#define CELESTIAL_G_CONST      1.0        /* Normalized gravitational constant */
#define CELESTIAL_C_LIGHT      100.0      /* Normalized relativistic speed of light */
#define CELESTIAL_SOFTENING    1e-5       /* Plummer softening parameter */

typedef struct {
    double x, y, z;
    double vx, vy, vz;
    double ax, ay, az;
    double mass;
    uint32_t body_id;
} CelestialBody;

typedef struct {
    uint32_t      n_bodies;
    CelestialBody bodies[CELESTIAL_MAX_BODIES];
    double        sim_time;
    double        dt;
    double        initial_hamiltonian;
    double        current_hamiltonian;
    double        energy_drift_ratio; /* |(H - H0) / H0| < 1e-12 */
} CelestialSystem;

typedef struct {
    double total_energy;
    double kinetic_energy;
    double potential_energy;
    double linear_momentum[3];
    double angular_momentum[3];
    double lyapunov_exponent;
    bool   energy_conserved_12_digits;
} CelestialTelemetryReceipt;

/* ------------------------------------------------------------------------- */
/* FUNCTION PROTOTYPES (C1 - C5)                                             */
/* ------------------------------------------------------------------------- */

/* C1: System Initialization & 8th-Order Symplectic Step */
bool celestial_init_system(CelestialSystem *sys, uint32_t n_bodies, double dt);
bool celestial_add_body(CelestialSystem *sys, double mass, double x, double y, double z, double vx, double vy, double vz);
bool celestial_step_symplectic_rk8(CelestialSystem *sys);

/* C2: Post-Newtonian (1PN + 2.5PN) Relativistic Accelerations */
bool celestial_compute_relativistic_forces(CelestialSystem *sys, bool enable_gw_radiation);

/* C3: Vectorized N-Body Force Accumulator */
bool celestial_vectorized_force_kernel(CelestialSystem *sys);

/* C4: Invariants & Lyapunov Chaotic Divergence Audit */
bool celestial_audit_telemetry(CelestialSystem *sys, CelestialTelemetryReceipt *out_receipt);

/* C5: JIT Trajectory Stream & Assembly Emitter */
int32_t celestial_emit_jit_assembly(const CelestialSystem *sys, char *out_buf, size_t buf_len);

#ifdef __cplusplus
}
#endif

#endif /* ZCC_CELESTIAL_NBODY_H */
