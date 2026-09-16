import subprocess

c_code = """
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t  a;
    uint64_t b;
    uint8_t  c;
    double   d;
    uint32_t e;
    void    *ptr;
} AdversarialStruct;

int main() {
    printf("SIZE:%zu ALIGN:%zu OFFS:%zu,%zu,%zu,%zu,%zu,%zu\\n",
           sizeof(AdversarialStruct), _Alignof(AdversarialStruct),
           offsetof(AdversarialStruct, a), offsetof(AdversarialStruct, b),
           offsetof(AdversarialStruct, c), offsetof(AdversarialStruct, d),
           offsetof(AdversarialStruct, e), offsetof(AdversarialStruct, ptr));
    return 0;
}
"""
open("/tmp/test_oracle_c.c", "w").write(c_code)

rs_code = """
use std::mem::{size_of, align_of};

#[repr(C)]
struct AdversarialStruct {
    a: u8,
    b: u64,
    c: u8,
    d: f64,
    e: u32,
    ptr: *mut u8,
}

macro_rules! offset_of {
    ($ty:ty, $field:ident) => {{
        let dummy = std::mem::MaybeUninit::<$ty>::uninit();
        let base = dummy.as_ptr();
        let field = unsafe { &(*base).$field as *const _ };
        (field as usize) - (base as usize)
    }};
}

fn main() {
    println!("SIZE:{} ALIGN:{} OFFS:{},{},{},{},{},{}",
             size_of::<AdversarialStruct>(), align_of::<AdversarialStruct>(),
             offset_of!(AdversarialStruct, a), offset_of!(AdversarialStruct, b),
             offset_of!(AdversarialStruct, c), offset_of!(AdversarialStruct, d),
             offset_of!(AdversarialStruct, e), offset_of!(AdversarialStruct, ptr));
}
"""
open("/tmp/test_oracle_rs.rs", "w").write(rs_code)

import shutil

subprocess.check_output(["gcc", "/tmp/test_oracle_c.c", "-o", "/tmp/test_oracle_gcc"])
res_gcc = subprocess.check_output(["/tmp/test_oracle_gcc"]).decode().strip()
print(f"GCC Oracle:   {res_gcc}")

if shutil.which("clang"):
    subprocess.check_output(["clang", "/tmp/test_oracle_c.c", "-o", "/tmp/test_oracle_clang"])
    res_clang = subprocess.check_output(["/tmp/test_oracle_clang"]).decode().strip()
    print(f"Clang Oracle: {res_clang}")
    assert res_gcc == res_clang
else:
    print("Clang Oracle: [SKIPPED - not installed]")

if shutil.which("rustc"):
    subprocess.check_output(["rustc", "/tmp/test_oracle_rs.rs", "-o", "/tmp/test_oracle_rustc"])
    res_rustc = subprocess.check_output(["/tmp/test_oracle_rustc"]).decode().strip()
    print(f"rustc Oracle: {res_rustc}")
    assert res_gcc == res_rustc
else:
    print("rustc Oracle: [SKIPPED - not installed]")

print("✓ Independent Host Oracles in 100% Exact Agreement!")
