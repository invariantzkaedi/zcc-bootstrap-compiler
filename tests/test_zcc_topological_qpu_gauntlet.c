#include "src/quantum/zcc_topological_qpu.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>

int main(void) {
    printf("╔════════════════════════════════════════════════════════════════════════╗\n");
    printf("║  ZCC TOPOLOGICAL QPU NON-ABELIAN MAJORANA BRAID ENGINE GAUNTLET (T1-T5)║\n");
    printf("╚════════════════════════════════════════════════════════════════════════╝\n\n");

    /* T1: Fusion Rules & Quantum Dimensions */
    printf("[T1] Testing Anyon Quantum Dimensions & Non-Abelian Fusion Rules...\n");
    double d_maj = topo_compute_quantum_dimension(ANYON_MAJORANA, 6);
    double d_fib = topo_compute_quantum_dimension(ANYON_FIBONACCI, 4);
    printf("  Majorana (N=6): Dim = %.4f | Fibonacci (N=4): Dim = %.4f\n", d_maj, d_fib);
    assert(fabs(d_maj - 4.0) < 1e-5);
    assert(fabs(d_fib - (TOPO_FIBONACCI_TAU * TOPO_FIBONACCI_TAU)) < 1e-5);

    assert(topo_verify_fusion_rule(ANYON_MAJORANA, ANYON_MAJORANA, ANYON_VACUUM));
    assert(topo_verify_fusion_rule(ANYON_MAJORANA, ANYON_MAJORANA, ANYON_FERMION));
    assert(topo_verify_fusion_rule(ANYON_FIBONACCI, ANYON_FIBONACCI, ANYON_FIBONACCI));
    printf("  [PASS] T1: Quantum dimensions & fusion rules verified.\n\n");

    /* T2: Artin Braid Group & Yang-Baxter Verification */
    printf("[T2] Testing Artin Braid Group (B_N) & Yang-Baxter Invariance...\n");
    TopoUnitary2x2 u;
    bool gen_ok = topo_eval_braid_generator(0, 1, &u);
    assert(gen_ok);
    bool ybe = topo_verify_yang_baxter_relation();
    printf("  Yang-Baxter Verification: %s | Unitary Matrix Norm: %.6f\n", ybe ? "SATISFIED" : "FAILED", 
           sqrt(u.r00*u.r00 + u.i00*u.i00 + u.r01*u.r01 + u.i01*u.i01));
    assert(ybe);
    printf("  [PASS] T2: Artin braid group & Yang-Baxter relation verified.\n\n");

    /* T3: Solovay-Kitaev Gate Synthesis */
    printf("[T3] Testing Solovay-Kitaev Topological Gate Compilation...\n");
    TopoBraidCircuit compiled_circuit;
    bool sk = topo_compile_unitary_to_braid(M_PI / 4.0, M_PI / 2.0, &compiled_circuit);
    assert(sk);
    printf("  Compiled Steps: %u | Topological Phase: %.4f rad | Unitary Fidelity: %.6f\n",
           compiled_circuit.n_steps, compiled_circuit.topological_phase, compiled_circuit.unitary_fidelity);
    assert(compiled_circuit.unitary_fidelity > 0.99999);
    printf("  [PASS] T3: Solovay-Kitaev topological braid compilation verified.\n\n");

    /* T4: Majorana Zero Mode Parity Readout */
    printf("[T4] Testing Majorana Zero Mode (MZM) Parity Readout & Immunity...\n");
    TopoParityReceipt receipt;
    bool parity = topo_measure_majorana_parity(&compiled_circuit, &receipt);
    assert(parity);
    printf("  Fermion Parity: %d | Topological Immunity: %.1f dB | YBE Invariant: %s\n",
           receipt.fermion_parity, receipt.decoherence_immunity_db, receipt.yang_baxter_satisfied ? "TRUE" : "FALSE");
    assert(receipt.decoherence_immunity_db >= 144.0);
    printf("  [PASS] T4: Non-local fermion parity & decoherence immunity verified.\n\n");

    /* T5: Nanowire Hardware Microcode Emitter */
    printf("[T5] Emitting Nanowire Hardware Braid Microcode...\n");
    char microcode_buf[1024];
    int written = topo_emit_hardware_microcode(&compiled_circuit, microcode_buf, sizeof(microcode_buf));
    assert(written > 0);
    printf("  Emitted %d bytes of topological assembly.\n", written);
    printf("  Sample microcode:\n%s\n", microcode_buf);
    printf("  [PASS] T5: Nanowire pulse microcode emission verified.\n\n");

    /* T6: Topological Peephole Optimizer */
    printf("[T6] Testing Topological Peephole Optimizer (Artin Braid Normal Form)...\n");
    TopoBraidCircuit unopt_circuit;
    unopt_circuit.n_anyons = 4;
    unopt_circuit.n_steps = 6;
    unopt_circuit.topological_phase = 1.5708;
    unopt_circuit.unitary_fidelity = 0.999998;
    
    /* Construct sequence with redundant inverse pairs & commuting strands: */
    /* sigma_0(+1), sigma_0(-1), sigma_0(+1), sigma_2(+1), sigma_0(-1), sigma_1(+1) */
    /* Step 0 & 1 cancel directly. Step 2 & 4 cancel after commuting around step 3 (sigma_2). */
    /* Expected final sequence: sigma_2(+1), sigma_1(+1) (2 steps left, 4 eliminated). */
    unopt_circuit.steps[0].anyon_index = 0; unopt_circuit.steps[0].direction = 1;
    unopt_circuit.steps[1].anyon_index = 0; unopt_circuit.steps[1].direction = -1;
    unopt_circuit.steps[2].anyon_index = 0; unopt_circuit.steps[2].direction = 1;
    unopt_circuit.steps[3].anyon_index = 2; unopt_circuit.steps[3].direction = 1;
    unopt_circuit.steps[4].anyon_index = 0; unopt_circuit.steps[4].direction = -1;
    unopt_circuit.steps[5].anyon_index = 1; unopt_circuit.steps[5].direction = 1;

    TopoBraidCircuit opt_circuit;
    uint32_t eliminated = 0;
    bool opt_ok = topo_optimize_braid_circuit(&unopt_circuit, &opt_circuit, &eliminated);
    assert(opt_ok);
    printf("  Original Steps: %u -> Optimized Steps: %u | Steps Eliminated: %u (%.1f%% Compression)\n",
           unopt_circuit.n_steps, opt_circuit.n_steps, eliminated, (double)eliminated / unopt_circuit.n_steps * 100.0);
    assert(eliminated == 4);
    assert(opt_circuit.n_steps == 2);
    assert(opt_circuit.steps[0].anyon_index == 2 && opt_circuit.steps[0].direction == 1);
    assert(opt_circuit.steps[1].anyon_index == 1 && opt_circuit.steps[1].direction == 1);
    printf("  [PASS] T6: Braid group inverse cancellation and commutation optimizer verified.\n\n");

    printf("========================================================================\n");
    printf("  🏆 TOPOLOGICAL QPU: ALL 6 MILESTONES (T1-T6) 100%% PASSED!\n");
    printf("========================================================================\n");
    return 0;
}
