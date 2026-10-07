// =========================================================================
// ZCC NATIVE WEBGPU WGSL COMPUTE SHADER: vec_add
// Pre-flight hardened, bounds-guarded, and validated for WebGPU 1.0
// =========================================================================

// -- Helper Primitives ---------------------------------------------------
fn zcc_rotr(x: u32, n: u32) -> u32 {
    return (x >> n) | (x << (32u - n));
}
fn zcc_rotl(x: u32, n: u32) -> u32 {
    return (x << n) | (x >> (32u - n));
}
fn zcc_ch(x: u32, y: u32, z: u32) -> u32 {
    return (x & y) ^ ((~x) & z);
}
fn zcc_maj(x: u32, y: u32, z: u32) -> u32 {
    return (x & y) ^ (x & z) ^ (y & z);
}

@group(0) @binding(0) var<storage, read_write> buf_a: array<f32>;
@group(0) @binding(1) var<storage, read_write> buf_b: array<f32>;
@group(0) @binding(2) var<storage, read_write> buf_c: array<f32>;

@compute @workgroup_size(64, 1, 1)
fn main(
    @builtin(global_invocation_id) gid: vec3<u32>,
    @builtin(local_invocation_id) tid: vec3<u32>,
    @builtin(workgroup_id) ctaid: vec3<u32>
) {
    let idx: u32 = gid.x;
    let idy: u32 = gid.y;
    let idz: u32 = gid.z;

    // Pre-flight Bounds Guard: Prevent out-of-bounds VRAM memory faults
    let total_elements: u32 = arrayLength(&buf_a);
    if (idx >= total_elements) {
        return;
    }

    var v_a: f32 = f32(0);
    var v_b: f32 = f32(0);
    var v_c: f32 = f32(0);
    var v_va: f32 = f32(0);
    var v_vb: f32 = f32(0);
    var v_vc: f32 = f32(0);

    v_va = buf_a[idx];
    v_vb = buf_b[idx];
    v_vc = v_va + v_vb;
    buf_c[idx] = v_vc;
    return;
}

