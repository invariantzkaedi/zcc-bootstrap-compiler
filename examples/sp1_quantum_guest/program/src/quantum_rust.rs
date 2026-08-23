#![allow(unused)]

/// ZKAEDI SOVEREIGN PIPELINE: LAYER 2 — OPTIMIZED PURE-RUST QUANTUM WALK GUEST
/// Highly optimized 16-node DTQW (T=8) matching examples/quantum_walk_16node.qasm
/// Features:
/// 1. Precomputed static twiddle factors & unrolled QFT gates
/// 2. Stack-allocated 32-complex statevector (0 dynamic heap allocations)
/// 3. Canonical 296-byte public commitment packing with SHA-256 digest

pub const N_NODES: usize = 16;
pub const N_STEPS: usize = 8;
pub const PI: f64 = 3.14159265358979323846;

// Precomputed QFT rotation constants (cos, sin)
const ROT_PI_2: (f64, f64) = (0.0, 1.0);                       // e^(i pi/2) = i
const ROT_NEG_PI_2: (f64, f64) = (0.0, -1.0);                   // e^(-i pi/2) = -i
const ROT_PI_4: (f64, f64) = (0.7071067811865476, 0.7071067811865475);  // e^(i pi/4)
const ROT_NEG_PI_4: (f64, f64) = (0.7071067811865476, -0.7071067811865475);
const ROT_PI_8: (f64, f64) = (0.9238795325112867, 0.3826834323650898);  // e^(i pi/8)
const ROT_NEG_PI_8: (f64, f64) = (0.9238795325112867, -0.3826834323650898);
const ROT_PI: (f64, f64) = (-1.0, 0.0);                        // e^(i pi) = -1
const INV_SQRT2: f64 = 0.7071067811865475;

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct QuantumPublic {
    pub node_probs: [f64; 16],
    pub node_phases: [f64; 16],
    pub s_q0: f64,
}

#[derive(Clone, Copy, Default, Debug)]
pub struct C {
    pub re: f64,
    pub im: f64,
}

impl C {
    #[inline(always)]
    pub const fn new(re: f64, im: f64) -> Self {
        Self { re, im }
    }
    #[inline(always)]
    pub fn mul_rot(self, cos_a: f64, sin_a: f64) -> C {
        C {
            re: self.re * cos_a - self.im * sin_a,
            im: self.re * sin_a + self.im * cos_a,
        }
    }
    #[inline(always)]
    pub fn norm_sq(self) -> f64 {
        self.re * self.re + self.im * self.im
    }
    #[inline(always)]
    pub fn phase(self) -> f64 {
        self.im.atan2(self.re)
    }
}

// --- Ultra-Fast Quantum Gate Operations on 32-State Array ---

#[inline(always)]
fn apply_h(state: &mut [C; 32], q: usize) {
    let bit = 1 << q;
    for i in 0..32 {
        if (i & bit) == 0 {
            let j = i | bit;
            let a = state[i];
            let b = state[j];
            state[i] = C::new((a.re + b.re) * INV_SQRT2, (a.im + b.im) * INV_SQRT2);
            state[j] = C::new((a.re - b.re) * INV_SQRT2, (a.im - b.im) * INV_SQRT2);
        }
    }
}

#[inline(always)]
fn apply_x(state: &mut [C; 32], q: usize) {
    let bit = 1 << q;
    for i in 0..32 {
        if (i & bit) == 0 {
            let j = i | bit;
            let tmp = state[i];
            state[i] = state[j];
            state[j] = tmp;
        }
    }
}

#[inline(always)]
fn apply_s(state: &mut [C; 32], q: usize) {
    let bit = 1 << q;
    for i in 0..32 {
        if (i & bit) != 0 {
            let a = state[i];
            state[i] = C::new(-a.im, a.re);
        }
    }
}

#[inline(always)]
fn apply_cu1_fast(state: &mut [C; 32], ctrl: usize, tgt: usize, cos_a: f64, sin_a: f64) {
    let bit_c = 1 << ctrl;
    let bit_t = 1 << tgt;
    for i in 0..32 {
        if (i & bit_c) != 0 && (i & bit_t) != 0 {
            state[i] = state[i].mul_rot(cos_a, sin_a);
        }
    }
}

#[inline(always)]
fn apply_swap(state: &mut [C; 32], q1: usize, q2: usize) {
    let bit1 = 1 << q1;
    let bit2 = 1 << q2;
    for i in 0..32 {
        if (i & bit1) != 0 && (i & bit2) == 0 {
            let j = (i ^ bit1) | bit2;
            let tmp = state[i];
            state[i] = state[j];
            state[j] = tmp;
        }
    }
}

/// Executes the unrolled 16-node DTQW (T=8) simulation
pub fn run_dtqw() -> QuantumPublic {
    let mut state = [C::default(); 32];

    // State preparation: Node 8 (|1000> on q4..q1) + symmetric coin
    apply_x(&mut state, 4);
    apply_h(&mut state, 0);
    apply_s(&mut state, 0);

    // 8 unrolled walk iterations
    for _ in 0..8 {
        apply_h(&mut state, 0);

        // Forward QFT
        apply_h(&mut state, 1);
        apply_cu1_fast(&mut state, 2, 1, ROT_PI_2.0, ROT_PI_2.1);
        apply_cu1_fast(&mut state, 3, 1, ROT_PI_4.0, ROT_PI_4.1);
        apply_cu1_fast(&mut state, 4, 1, ROT_PI_8.0, ROT_PI_8.1);

        apply_h(&mut state, 2);
        apply_cu1_fast(&mut state, 3, 2, ROT_PI_2.0, ROT_PI_2.1);
        apply_cu1_fast(&mut state, 4, 2, ROT_PI_4.0, ROT_PI_4.1);

        apply_h(&mut state, 3);
        apply_cu1_fast(&mut state, 4, 3, ROT_PI_2.0, ROT_PI_2.1);

        apply_h(&mut state, 4);

        apply_swap(&mut state, 1, 4);
        apply_swap(&mut state, 2, 3);

        // Controlled Phase Shift
        apply_x(&mut state, 0);
        apply_cu1_fast(&mut state, 0, 1, ROT_PI.0, ROT_PI.1);
        apply_cu1_fast(&mut state, 0, 2, ROT_PI_2.0, ROT_PI_2.1);
        apply_cu1_fast(&mut state, 0, 3, ROT_PI_4.0, ROT_PI_4.1);
        apply_cu1_fast(&mut state, 0, 4, ROT_PI_8.0, ROT_PI_8.1);
        apply_x(&mut state, 0);

        apply_cu1_fast(&mut state, 0, 1, ROT_PI.0, -ROT_PI.1);
        apply_cu1_fast(&mut state, 0, 2, ROT_NEG_PI_2.0, ROT_NEG_PI_2.1);
        apply_cu1_fast(&mut state, 0, 3, ROT_NEG_PI_4.0, ROT_NEG_PI_4.1);
        apply_cu1_fast(&mut state, 0, 4, ROT_NEG_PI_8.0, ROT_NEG_PI_8.1);

        // Inverse QFT
        apply_swap(&mut state, 1, 4);
        apply_swap(&mut state, 2, 3);

        apply_h(&mut state, 4);
        apply_cu1_fast(&mut state, 4, 3, ROT_NEG_PI_2.0, ROT_NEG_PI_2.1);
        apply_h(&mut state, 3);

        apply_cu1_fast(&mut state, 4, 2, ROT_NEG_PI_4.0, ROT_NEG_PI_4.1);
        apply_cu1_fast(&mut state, 3, 2, ROT_NEG_PI_2.0, ROT_NEG_PI_2.1);
        apply_h(&mut state, 2);

        apply_cu1_fast(&mut state, 4, 1, ROT_NEG_PI_8.0, ROT_NEG_PI_8.1);
        apply_cu1_fast(&mut state, 3, 1, ROT_NEG_PI_4.0, ROT_NEG_PI_4.1);
        apply_cu1_fast(&mut state, 2, 1, ROT_NEG_PI_2.0, ROT_NEG_PI_2.1);
        apply_h(&mut state, 1);
    }

    // Aggregate spatial distributions
    let mut probs = [0.0f64; 16];
    let mut phases = [0.0f64; 16];

    for k in 0..32 {
        let p = state[k].norm_sq();
        let pos = k >> 1;
        probs[pos] += p;
        if p > 1e-12 {
            phases[pos] += state[k].phase() * p;
        }
    }

    for n in 0..16 {
        if probs[n] > 1e-12 {
            phases[n] /= probs[n];
        }
    }

    // Compute S(q0) coin entropy
    let mut rho00 = 0.0f64;
    let mut rho11 = 0.0f64;
    let mut re_01 = 0.0f64;
    let mut im_01 = 0.0f64;

    for pos in 0..16 {
        let a0 = state[pos << 1];
        let a1 = state[(pos << 1) | 1];
        rho00 += a0.norm_sq();
        rho11 += a1.norm_sq();
        re_01 += a0.re * a1.re + a0.im * a1.im;
        im_01 += a0.im * a1.re - a0.re * a1.im;
    }

    let diff = rho00 - rho11;
    let delta = (diff * diff + 4.0 * (re_01 * re_01 + im_01 * im_01)).sqrt();
    let lambda1 = (1.0 + delta) * 0.5;
    let lambda2 = (1.0 - delta) * 0.5;

    let s_q0 = {
        let mut s = 0.0f64;
        let ln2 = 0.6931471805599453;
        if lambda1 > 1e-12 { s -= lambda1 * (lambda1.ln() / ln2); }
        if lambda2 > 1e-12 { s -= lambda2 * (lambda2.ln() / ln2); }
        s
    };

    QuantumPublic {
        node_probs: probs,
        node_phases: phases,
        s_q0,
    }
}

/// Freestanding SHA-256 implementation
fn compute_sha256_264(payload: &[u8; 264]) -> [u8; 32] {
    let mut h: [u32; 8] = [
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19,
    ];

    const K: [u32; 64] = [
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
    ];

    let mut padded = [0u8; 320];
    padded[..264].copy_from_slice(payload);
    padded[264] = 0x80;
    padded[318] = 0x08;
    padded[319] = 0x40;

    for chunk in padded.chunks_exact(64) {
        let mut w = [0u32; 64];
        for i in 0..16 {
            w[i] = u32::from_be_bytes([
                chunk[i * 4],
                chunk[i * 4 + 1],
                chunk[i * 4 + 2],
                chunk[i * 4 + 3],
            ]);
        }
        for i in 16..64 {
            let s0 = w[i - 15].rotate_right(7) ^ w[i - 15].rotate_right(18) ^ (w[i - 15] >> 3);
            let s1 = w[i - 2].rotate_right(17) ^ w[i - 2].rotate_right(19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16].wrapping_add(s0).wrapping_add(w[i - 7]).wrapping_add(s1);
        }

        let mut a = h[0];
        let mut b = h[1];
        let mut c = h[2];
        let mut d = h[3];
        let mut e = h[4];
        let mut f = h[5];
        let mut g = h[6];
        let mut hh = h[7];

        for i in 0..64 {
            let s1 = e.rotate_right(6) ^ e.rotate_right(11) ^ e.rotate_right(25);
            let ch = (e & f) ^ ((!e) & g);
            let temp1 = hh
                .wrapping_add(s1)
                .wrapping_add(ch)
                .wrapping_add(K[i])
                .wrapping_add(w[i]);
            let s0 = a.rotate_right(2) ^ a.rotate_right(13) ^ a.rotate_right(22);
            let maj = (a & b) ^ (a & c) ^ (b & c);
            let temp2 = s0.wrapping_add(maj);

            hh = g;
            g = f;
            f = e;
            e = d.wrapping_add(temp1);
            d = c;
            c = b;
            b = a;
            a = temp1.wrapping_add(temp2);
        }

        h[0] = h[0].wrapping_add(a);
        h[1] = h[1].wrapping_add(b);
        h[2] = h[2].wrapping_add(c);
        h[3] = h[3].wrapping_add(d);
        h[4] = h[4].wrapping_add(e);
        h[5] = h[5].wrapping_add(f);
        h[6] = h[6].wrapping_add(g);
        h[7] = h[7].wrapping_add(hh);
    }

    let mut out = [0u8; 32];
    for (i, val) in h.iter().enumerate() {
        out[i * 4..(i + 1) * 4].copy_from_slice(&val.to_be_bytes());
    }
    out
}

/// Pack into the canonical 296-byte public commitment layout
pub fn pack_commitment(pub_out: &QuantumPublic) -> [u8; 296] {
    let mut payload = [0u8; 264];
    for (i, p) in pub_out.node_probs.iter().enumerate() {
        payload[i * 8..(i + 1) * 8].copy_from_slice(&p.to_le_bytes());
    }
    for (i, ph) in pub_out.node_phases.iter().enumerate() {
        payload[128 + i * 8..128 + (i + 1) * 8].copy_from_slice(&ph.to_le_bytes());
    }
    payload[256..264].copy_from_slice(&pub_out.s_q0.to_le_bytes());

    let digest = compute_sha256_264(&payload);

    let mut full = [0u8; 296];
    full[..264].copy_from_slice(&payload);
    full[264..].copy_from_slice(&digest);
    full
}
