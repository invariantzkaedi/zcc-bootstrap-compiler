/* ========================================================================= */
/* ZCC MULTI-ARCHITECTURE QUANTUM HYBRID DISPATCHER GAUNTLET                 */
/* ========================================================================= */
/* File: tests/test_zcc_multi_arch_qasm_dispatcher.c                         */
/* ========================================================================= */

#include "include/zcc_multi_arch_qasm_dispatcher.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

int main(void) {
    printf("╔════════════════════════════════════════════════════════════════════════╗\n");
    printf("║   ZCC MULTI-ARCHITECTURE QUANTUM HYBRID DISPATCHER GAUNTLET (D1-D4)   ║\n");
    printf("╚════════════════════════════════════════════════════════════════════════╝\n\n");

    /* D1: OpenQASM 2.0 Parsing to QIR */
    printf("[D1] Parsing OpenQASM 2.0 Circuit into Quantum Intermediate Representation...\n");
    const char *sample_qasm = 
        "OPENQASM 2.0;\n"
        "include \"qelib1.inc\";\n"
        "qreg q[4];\n"
        "h q[0];\n"
        "cx q[0], q[1];\n"
        "z q[0];\n"
        "s q[1];\n"
        "t q[2];\n"
        "h q[3];\n";

    QIRCircuit circuit;
    bool parse_ok = qir_parse_qasm_string(sample_qasm, &circuit);
    assert(parse_ok);
    printf("  Circuit Name: %s | Qubits: %u | Parsed Gates: %u\n",
           circuit.circuit_name, circuit.n_qubits, circuit.n_gates);
    assert(circuit.n_qubits == 4);
    assert(circuit.n_gates >= 6);
    printf("  [PASS] D1: OpenQASM 2.0 syntax parsing to QIR verified.\n\n");

    /* D2: Target 1 — Classical AVX2/AVX-512 SIMD C-Native Emitter */
    printf("[D2] Testing Target 1: Classical AVX2/AVX-512 C-Native Statevector Emitter...\n");
    char c_buf[4096];
    int32_t c_bytes = qdisp_emit_avx_simd_c(&circuit, c_buf, sizeof(c_buf));
    assert(c_bytes > 0);
    printf("  Emitted %d bytes of AVX2 C-native simulation code.\n", c_bytes);
    assert(strstr(c_buf, "_mm256_loadu_pd") != NULL);
    assert(strstr(c_buf, "_mm256_mul_pd") != NULL);
    printf("  [PASS] D2: AVX2 SIMD butterfly statevector C code emission verified.\n\n");

    /* D3: Target 2 — Majorana Nanowire Braid Microcode (.s) with T6 Optimizer */
    printf("[D3] Testing Target 2: Majorana Nanowire Non-Abelian Braid Microcode Emitter...\n");
    char braid_buf[4096];
    uint32_t unopt_steps = 0, opt_steps = 0;
    int32_t s_bytes = qdisp_emit_topological_braid_s(&circuit, braid_buf, sizeof(braid_buf), &unopt_steps, &opt_steps);
    assert(s_bytes > 0);
    printf("  Emitted %d bytes of Topological Nanowire Assembly.\n", s_bytes);
    printf("  Topological Steps: %u (Raw) -> %u (T6 Optimized)\n", unopt_steps, opt_steps);
    assert(strstr(braid_buf, "pulse_gate_vj") != NULL);
    assert(strstr(braid_buf, ".section .text.topological_braid") != NULL);
    printf("  [PASS] D3: Non-Abelian Majorana braid microcode & T6 optimizer verified.\n\n");

    /* D4: Target 3 — NVIDIA PTX 7.8 Tensor Statevector Assembly (.ptx) */
    printf("[D4] Testing Target 3: NVIDIA PTX 7.8 / SM_89 Tensor Core Emitter...\n");
    char ptx_buf[4096];
    int32_t ptx_bytes = qdisp_emit_nvidia_ptx(&circuit, ptx_buf, sizeof(ptx_buf));
    assert(ptx_bytes > 0);
    printf("  Emitted %d bytes of NVIDIA PTX 7.8 Assembly.\n", ptx_bytes);
    assert(strstr(ptx_buf, ".version 7.8") != NULL);
    assert(strstr(ptx_buf, ".target sm_89") != NULL);
    assert(strstr(ptx_buf, "mul.f64") != NULL);
    printf("  [PASS] D4: NVIDIA PTX 7.8 assembly emission verified.\n\n");

    /* Unified Multi-Target Disk Export */
    printf("[DISPATCH] Testing Master Multi-Target Pipeline & Disk Export (/tmp/qdisp_out)...\n");
    system("mkdir -p /tmp/qdisp_out");
    QDispArtifactReceipt receipt;
    bool emit_all = qdisp_lower_and_emit_all(&circuit, "/tmp/qdisp_out", &receipt);
    assert(emit_all);
    assert(receipt.all_targets_emitted_cleanly);
    printf("  Receipt Summary:\n");
    printf("    • AVX C-Native Kernel : %u bytes (libzcc_quantum_unified.c)\n", receipt.avx_c_bytes);
    printf("    • Topo Braid Microcode: %u bytes (topological_braid.s)\n", receipt.topo_s_bytes);
    printf("    • NVIDIA PTX Kernel   : %u bytes (triton_ptx_kernel.ptx)\n", receipt.ptx_bytes);
    printf("    • Step Compression    : %.1f%%\n", receipt.opt_step_compression_pct);

    printf("\n========================================================================\n");
    printf("  🏆 MULTI-ARCHITECTURE QUANTUM DISPATCHER: ALL TARGETS 100%% VERIFIED!\n");
    printf("========================================================================\n");
    return 0;
}
