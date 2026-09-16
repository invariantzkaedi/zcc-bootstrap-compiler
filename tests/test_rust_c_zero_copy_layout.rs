use std::mem::{size_of, align_of};

#[repr(C)]
pub struct ZccAdversarialFFIPacket {
    pub tag_u8: u8,
    pub signature_u64: u64,
    pub flags_u8: u8,
    pub latency_f64: f64,
    pub shard_id_u32: u32,
    pub buffer_ptr: *mut u8,
}

#[no_mangle]
pub extern "C" fn rust_mutate_packet(pkt: *mut ZccAdversarialFFIPacket) {
    if pkt.is_null() {
        return;
    }
    unsafe {
        let p = &mut *pkt;
        p.tag_u8 = 0xA1;
        p.signature_u64 = 0xFEDCBA9876543210;
        p.flags_u8 = 0x9B;
        p.latency_f64 = 2.718281828459045;
        p.shard_id_u32 = 0x12345678;
        p.buffer_ptr = (p.buffer_ptr as *mut u8).add(64);
    }
}

#[no_mangle]
pub extern "C" fn rust_verify_c_packet(
    pkt: *const ZccAdversarialFFIPacket,
    expected_addr: *const std::ffi::c_void,
    canary_byte: u8
) -> i32 {
    if pkt.is_null() {
        return 1;
    }

    // 1. Pointer Identity Check
    if pkt as *const std::ffi::c_void != expected_addr {
        return 10;
    }

    unsafe {
        let p = &*pkt;
        if p.tag_u8 != 0x7E { return 11; }
        if p.signature_u64 != 0x0123456789ABCDEF { return 12; }
        if p.flags_u8 != 0x3C { return 13; }
        if (p.latency_f64 - 3.141592653589793).abs() > 1e-12 { return 14; }
        if p.shard_id_u32 != 0xA5A55A5A { return 15; }

        // 2. Raw Memory Padding Canaries Check
        let raw = pkt as *const u8;
        for i in 1..=7 {
            if *raw.add(i) != canary_byte { return 20; }
        }
        for i in 17..=23 {
            if *raw.add(i) != canary_byte { return 21; }
        }
        for i in 36..=39 {
            if *raw.add(i) != canary_byte { return 22; }
        }
    }

    0 // PASS
}
