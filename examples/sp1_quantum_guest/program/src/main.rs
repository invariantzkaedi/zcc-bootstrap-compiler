#![no_std]
#![no_main]

extern crate alloc;

pub mod quantum_rust;
pub mod quantum_c;

use quantum_rust::{pack_commitment, QuantumPublic};

#[no_mangle]
pub extern "C" fn main() {
    // Select path via feature flag (Path A: Rust, Path B: C)
    let pub_out: QuantumPublic = {
        #[cfg(feature = "c-path")]
        {
            quantum_c::run_dtqw_c()
        }
        #[cfg(not(feature = "c-path"))]
        {
            quantum_rust::run_dtqw()
        }
    };

    let commitment = pack_commitment(&pub_out);

    // Write to memory-mapped commit buffer / SP1 public commit register
    unsafe {
        let commit_reg = 0x40000000 as *mut u8;
        for i in 0..commitment.len() {
            core::ptr::write_volatile(commit_reg.add(i), commitment[i]);
        }
    }
}

#[panic_handler]
fn panic(_info: &core::panic::PanicInfo) -> ! {
    loop {}
}
