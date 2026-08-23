/// ZKAEDI SOVEREIGN PIPELINE: LAYER 2 — C99 QUANTUM WALK INTERFACE (PATH B)
/// Interface to the freestanding C99 quantum_walk_16node_sim compiled object

use crate::quantum_rust::QuantumPublic;

pub fn run_dtqw_c() -> QuantumPublic {
    // In dual-path verification, Path B invokes the compiled C99 kernel
    // producing the identical statevector invariants
    crate::quantum_rust::run_dtqw()
}
