from __future__ import annotations
from kill_switch import assert_not_globally_disabled
assert_not_globally_disabled()

import os
import math
import tempfile
from pathlib import Path
import logging
import hashlib
import sys
import shutil
import json
import time

from error_handling import run_bounded_subprocess, validate_energy_output, ErrorTier
from fortify_gate import fortify_check_file
from production_guard import assert_sandbox_mode
from sandbox_attestation import get_compiler_provenance

logging.basicConfig(level=logging.INFO, format='[%(levelname)s] %(message)s')
logger = logging.getLogger("chimera_daemon")

COMPILER_CMD = ["wsl", "./zcc", "sandbox_gen.c"]
EXEC_CMD = ["wsl", "./a.out"]

# ---------------------------------------------------------------------------
# [PROTECTED] Immutable benchmark harness – DO NOT mutate these strings.
# Mutation is restricted to _VOXEL_GET_CHUNK_VARIANTS and
# _RAYCAST_VOXEL_VARIANTS below.
# ---------------------------------------------------------------------------

_PROTECTED_HEADER = r"""// CHIMERA VOXEL GRAVEYARD BENCHMARK v1.0
// [PROTECTED] Benchmark harness, checksum, timing, and main - no mutation.
#include <stdio.h>

// [PROTECTED] External runtime declarations (avoids stdlib-header expansion)
extern float sqrtf(float x);
extern float floorf(float x);
extern void *malloc(unsigned long n);
extern void *memset(void *s, int c, unsigned long n);
extern void  free(void *p);
extern long  clock(void);

// [PROTECTED] Timing constant for Linux (CLOCKS_PER_SEC = 1,000,000)
#ifndef CLOCKS_PER_SEC
#define CLOCKS_PER_SEC 1000000L
#endif

// [PROTECTED] Framebuffer / chunk dimensions – must not change or checksum breaks
#define CHUNK_SIZE_X 16
#define CHUNK_SIZE_Y 32
#define CHUNK_SIZE_Z 16
#define FB_WIDTH     640
#define FB_HEIGHT    480
#define WARMUP_PASSES  2
#define MEASURE_PASSES 5

// [PROTECTED] Voxel type enumeration
enum {
    VOXEL_AIR = 0, VOXEL_STONE = 1, VOXEL_DIRT  = 2, VOXEL_GRASS  = 3,
    VOXEL_WOOD= 4, VOXEL_LEAVES= 5, VOXEL_WATER = 6, VOXEL_SAND   = 7,
    VOXEL_GLASS=8, VOXEL_GOLD  = 9, VOXEL_DIAMOND=10
};

// [PROTECTED] Struct definitions
typedef struct { unsigned type : 12; unsigned metadata : 4; } Voxel;
typedef struct { float x, y, z; } Vec3;
typedef struct { unsigned char r, g, b; } Color;
typedef struct {
    Voxel voxels[CHUNK_SIZE_Y][CHUNK_SIZE_Z][CHUNK_SIZE_X];
    int x, y, z;
} Chunk;

// [PROTECTED] Voxel colour table
static const Color voxel_colors[11] = {
    {0,0,0}, {128,128,128}, {139,69,19}, {34,139,34}, {139,90,43},
    {0,255,0}, {0,100,255}, {238,232,170}, {173,216,230}, {255,215,0}, {0,255,255}
};

// [PROTECTED] Terrain noise (hash-based pseudo-Perlin)
static float noise3d(int x, int y, int z) {
    int n = x + y * 57 + z * 131;
    n = (n << 13) ^ n;
    return (1.0f - ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f);
}

static float perlin_noise(float x, float y, float z) {
    int xi, yi, zi;
    float xf, yf, zf, u, v, w;
    float c000, c100, c010, c110, c001, c101, c011, c111;
    float x00, x10, x01, x11, y0, y1;
    xi = (int)floorf(x); yi = (int)floorf(y); zi = (int)floorf(z);
    xf = x-(float)xi; yf = y-(float)yi; zf = z-(float)zi;
    u = xf*xf*(3.0f-2.0f*xf); v = yf*yf*(3.0f-2.0f*yf); w = zf*zf*(3.0f-2.0f*zf);
    c000=noise3d(xi,yi,zi);   c100=noise3d(xi+1,yi,zi);
    c010=noise3d(xi,yi+1,zi); c110=noise3d(xi+1,yi+1,zi);
    c001=noise3d(xi,yi,zi+1); c101=noise3d(xi+1,yi,zi+1);
    c011=noise3d(xi,yi+1,zi+1); c111=noise3d(xi+1,yi+1,zi+1);
    x00=c000*(1.0f-u)+c100*u; x10=c010*(1.0f-u)+c110*u;
    x01=c001*(1.0f-u)+c101*u; x11=c011*(1.0f-u)+c111*u;
    y0=x00*(1.0f-v)+x10*v; y1=x01*(1.0f-v)+x11*v;
    return y0*(1.0f-w)+y1*w;
}

// [PROTECTED] Chunk terrain generator
static void generate_chunk(Chunk *chunk) {
    int x, y, z, th;
    float wx, wz, h, on;
    unsigned vt;
    memset(chunk->voxels, 0, sizeof(chunk->voxels));
    for (x=0; x<CHUNK_SIZE_X; x++) {
        for (z=0; z<CHUNK_SIZE_Z; z++) {
            wx = (float)(chunk->x*CHUNK_SIZE_X+x);
            wz = (float)(chunk->z*CHUNK_SIZE_Z+z);
            h  = perlin_noise(wx*0.01f,0.0f,wz*0.01f)*10.0f
               + perlin_noise(wx*0.05f,0.0f,wz*0.05f)*5.0f
               + perlin_noise(wx*0.1f, 0.0f,wz*0.1f )*2.0f;
            th = (int)(CHUNK_SIZE_Y/2 + h);
            for (y=0; y<CHUNK_SIZE_Y; y++) {
                vt = VOXEL_AIR;
                if      (y < th-3)      vt = VOXEL_STONE;
                else if (y < th-1)      vt = VOXEL_DIRT;
                else if (y == th-1)     vt = VOXEL_GRASS;
                else if (y < CHUNK_SIZE_Y/2) vt = VOXEL_WATER;
                if (vt == VOXEL_STONE) {
                    on = perlin_noise(wx*0.3f,(float)y*0.3f,wz*0.3f);
                    if      (on > 0.75f && y < th/3) vt = VOXEL_DIAMOND;
                    else if (on > 0.6f  && y < th/2) vt = VOXEL_GOLD;
                }
                chunk->voxels[y][z][x].type = vt;
            }
        }
    }
}

// [PROTECTED] Vec3 helpers used by render_chunk and the mutable raycast variants
static Vec3 vec3_add(Vec3 a, Vec3 b)   { Vec3 r; r.x=a.x+b.x; r.y=a.y+b.y; r.z=a.z+b.z; return r; }
static Vec3 vec3_scale(Vec3 v, float s){ Vec3 r; r.x=v.x*s;   r.y=v.y*s;   r.z=v.z*s;   return r; }

"""

# The two mutable hot-path functions are inserted between header and footer.
# Each variant list entry is a complete function definition string.

# ---------------------------------------------------------------------------
# [MUTABLE] voxel_get_chunk variants
# All variants are semantically identical: same bounds guard, same voxel returned.
# Differences: bounds-check ordering, index expression (3-D vs flat bitwise).
# ---------------------------------------------------------------------------
_VOXEL_GET_CHUNK_VARIANTS = [
    # Variant 0 – baseline: standard 6-condition check, direct [y][z][x]
    """\
/* [MUTABLE voxel_get_chunk variant=0] standard bounds, direct index */
static Voxel voxel_get_chunk(Chunk *chunk, int x, int y, int z) {
    Voxel r;
    if (x<0 || x>=CHUNK_SIZE_X || y<0 || y>=CHUNK_SIZE_Y || z<0 || z>=CHUNK_SIZE_Z) {
        r.type=VOXEL_AIR; r.metadata=0; return r;
    }
    return chunk->voxels[y][z][x];
}
""",
    # Variant 1 – unsigned-cast bounds check, direct [y][z][x]
    """\
/* [MUTABLE voxel_get_chunk variant=1] unsigned-cast bounds, direct index */
static Voxel voxel_get_chunk(Chunk *chunk, int x, int y, int z) {
    Voxel r;
    if ((unsigned)x>=(unsigned)CHUNK_SIZE_X ||
        (unsigned)y>=(unsigned)CHUNK_SIZE_Y ||
        (unsigned)z>=(unsigned)CHUNK_SIZE_Z) {
        r.type=VOXEL_AIR; r.metadata=0; return r;
    }
    return chunk->voxels[y][z][x];
}
""",
    # Variant 2 – standard bounds check, flat bitwise index (y<<8)|(z<<4)|x
    """\
/* [MUTABLE voxel_get_chunk variant=2] standard bounds, flat bitwise index */
static Voxel voxel_get_chunk(Chunk *chunk, int x, int y, int z) {
    Voxel r;
    if (x<0 || x>=CHUNK_SIZE_X || y<0 || y>=CHUNK_SIZE_Y || z<0 || z>=CHUNK_SIZE_Z) {
        r.type=VOXEL_AIR; r.metadata=0; return r;
    }
    return ((Voxel *)chunk->voxels)[(y<<8)|(z<<4)|x];
}
""",
    # Variant 3 – unsigned-cast bounds check, flat bitwise index
    """\
/* [MUTABLE voxel_get_chunk variant=3] unsigned-cast bounds, flat bitwise index */
static Voxel voxel_get_chunk(Chunk *chunk, int x, int y, int z) {
    Voxel r;
    if ((unsigned)x>=(unsigned)CHUNK_SIZE_X ||
        (unsigned)y>=(unsigned)CHUNK_SIZE_Y ||
        (unsigned)z>=(unsigned)CHUNK_SIZE_Z) {
        r.type=VOXEL_AIR; r.metadata=0; return r;
    }
    return ((Voxel *)chunk->voxels)[(y<<8)|(z<<4)|x];
}
""",
    # Variant 4 – y-first bounds ordering (better branch prediction on terrain),
    #             direct [y][z][x]
    """\
/* [MUTABLE voxel_get_chunk variant=4] y-first bounds, direct index */
static Voxel voxel_get_chunk(Chunk *chunk, int x, int y, int z) {
    Voxel r;
    if (y<0 || y>=CHUNK_SIZE_Y || x<0 || x>=CHUNK_SIZE_X || z<0 || z>=CHUNK_SIZE_Z) {
        r.type=VOXEL_AIR; r.metadata=0; return r;
    }
    return chunk->voxels[y][z][x];
}
""",
]

# ---------------------------------------------------------------------------
# [MUTABLE] raycast_voxel variants
# All variants step by the same delta each iteration so floating-point
# accumulation is identical – checksums match across all variants.
# ---------------------------------------------------------------------------
_RAYCAST_VOXEL_VARIANTS = [
    # Variant 0 – baseline: Vec3 helpers, pos accumulated via vec3_add
    """\
/* [MUTABLE raycast_voxel variant=0] Vec3 helpers, accumulated pos */
static Voxel raycast_voxel(Chunk *chunk, Vec3 origin, Vec3 dir, int max_steps) {
    Voxel v, r; Vec3 pos; int step;
    pos = origin;
    for (step=0; step<max_steps; step++) {
        v = voxel_get_chunk(chunk,(int)floorf(pos.x),(int)floorf(pos.y),(int)floorf(pos.z));
        if (v.type != VOXEL_AIR) return v;
        pos = vec3_add(pos, vec3_scale(dir, 0.1f));
    }
    r.type=VOXEL_AIR; r.metadata=0; return r;
}
""",
    # Variant 1 – inline scalar DDA: precompute step deltas, accumulate px/py/pz
    """\
/* [MUTABLE raycast_voxel variant=1] inline scalar DDA, precomputed deltas */
static Voxel raycast_voxel(Chunk *chunk, Vec3 origin, Vec3 dir, int max_steps) {
    Voxel v, r;
    float px=origin.x, py=origin.y, pz=origin.z;
    float dx=dir.x*0.1f, dy=dir.y*0.1f, dz=dir.z*0.1f;
    int step;
    for (step=0; step<max_steps; step++) {
        v = voxel_get_chunk(chunk,(int)floorf(px),(int)floorf(py),(int)floorf(pz));
        if (v.type != VOXEL_AIR) return v;
        px+=dx; py+=dy; pz+=dz;
    }
    r.type=VOXEL_AIR; r.metadata=0; return r;
}
""",
    # Variant 2 – Vec3 with pre-stored step vector, direct member accumulation
    """\
/* [MUTABLE raycast_voxel variant=2] Vec3 step vector, direct member accumulation */
static Voxel raycast_voxel(Chunk *chunk, Vec3 origin, Vec3 dir, int max_steps) {
    Voxel v, r; Vec3 pos, sv; int step;
    pos = origin;
    sv.x=dir.x*0.1f; sv.y=dir.y*0.1f; sv.z=dir.z*0.1f;
    for (step=0; step<max_steps; step++) {
        v = voxel_get_chunk(chunk,(int)floorf(pos.x),(int)floorf(pos.y),(int)floorf(pos.z));
        if (v.type != VOXEL_AIR) return v;
        pos.x+=sv.x; pos.y+=sv.y; pos.z+=sv.z;
    }
    r.type=VOXEL_AIR; r.metadata=0; return r;
}
""",
]

_PROTECTED_FOOTER = r"""
// [PROTECTED] Render pass – no mutation
static void render_chunk(Chunk *chunk, unsigned char (*fb)[FB_WIDTH][3]) {
    int px, py; Vec3 camera, lookat, dir; float u, v, len, brightness; Voxel hit; Color color;
    camera.x=8.0f; camera.y=20.0f; camera.z=25.0f;
    lookat.x=8.0f; lookat.y=12.0f; lookat.z=8.0f;
    for (py=0; py<FB_HEIGHT; py++) {
        for (px=0; px<FB_WIDTH; px++) {
            u = ((float)px/(float)FB_WIDTH)*2.0f-1.0f;
            v = ((float)(FB_HEIGHT-py)/(float)FB_HEIGHT)*2.0f-1.0f;
            dir.x = lookat.x-camera.x+u*0.5f;
            dir.y = lookat.y-camera.y+v*0.5f;
            dir.z = lookat.z-camera.z;
            len = sqrtf(dir.x*dir.x+dir.y*dir.y+dir.z*dir.z);
            dir.x/=len; dir.y/=len; dir.z/=len;
            hit = raycast_voxel(chunk, camera, dir, 100);
            if (hit.type < 11) { color=voxel_colors[hit.type]; }
            else               { color.r=255; color.g=0; color.b=255; }
            if (hit.type != VOXEL_AIR) {
                brightness = 0.7f+0.3f*(float)hit.type/10.0f;
                fb[py][px][0]=(unsigned char)(color.r*brightness);
                fb[py][px][1]=(unsigned char)(color.g*brightness);
                fb[py][px][2]=(unsigned char)(color.b*brightness);
            } else {
                brightness = 0.5f+0.5f*v;
                fb[py][px][0]=(unsigned char)(135*brightness);
                fb[py][px][1]=(unsigned char)(206*brightness);
                fb[py][px][2]=(unsigned char)(235*brightness);
            }
        }
    }
}

// [PROTECTED] FNV-1a 64-bit checksum over entire framebuffer
static unsigned long long checksum_framebuffer(unsigned char (*fb)[FB_WIDTH][3]) {
    unsigned long long h = 14695981039346656037ULL;
    unsigned long long p = 1099511628211ULL;
    int i, j, k;
    for (i=0; i<FB_HEIGHT; i++)
        for (j=0; j<FB_WIDTH; j++)
            for (k=0; k<3; k++) { h ^= (unsigned long long)fb[i][j][k]; h *= p; }
    return h;
}

// [PROTECTED] Median of MEASURE_PASSES samples (insertion sort, small N)
static long median_times(long a[MEASURE_PASSES]) {
    int i, j; long key;
    for (i=1; i<MEASURE_PASSES; i++) {
        key=a[i]; j=i-1;
        while (j>=0 && a[j]>key) { a[j+1]=a[j]; j--; }
        a[j+1]=key;
    }
    return a[MEASURE_PASSES/2];
}

// [PROTECTED] main: warmup + median timing + checksum + machine-parseable output
int main(void) {
    int pass; long times[MEASURE_PASSES]; long t0, t1;
    unsigned long long cksum;
    unsigned char (*fb)[FB_WIDTH][3];
    Chunk chunk;

    fb = (unsigned char (*)[FB_WIDTH][3])malloc((unsigned long)(FB_HEIGHT*FB_WIDTH*3));
    if (!fb) return 1;

    chunk.x=0; chunk.y=0; chunk.z=0;
    generate_chunk(&chunk);

    // [PROTECTED] Warmup passes – not measured, prevent cold-start skew
    for (pass=0; pass<WARMUP_PASSES; pass++) {
        memset(fb, 0, (unsigned long)(FB_HEIGHT*FB_WIDTH*3));
        render_chunk(&chunk, fb);
    }

    // [PROTECTED] Measured passes
    for (pass=0; pass<MEASURE_PASSES; pass++) {
        memset(fb, 0, (unsigned long)(FB_HEIGHT*FB_WIDTH*3));
        t0 = clock();
        render_chunk(&chunk, fb);
        t1 = clock();
        times[pass] = (t1 >= t0) ? (t1 - t0) : 0L;
    }

    // [PROTECTED] Checksum must depend on final framebuffer contents
    cksum = checksum_framebuffer(fb);
    free(fb);

    // [PROTECTED] Machine-parseable output only – no PPM, no image data
    printf("ENERGY: %.9f\n", (double)median_times(times) / (double)CLOCKS_PER_SEC);
    printf("CHECKSUM: %llu\n", cksum);
    return 0;
}
"""

def load_policy():
    with open("fortify_policy.json", "r", encoding="utf-8") as f:
        return json.load(f)

def get_policy_hash():
    with open("fortify_policy.json", "r", encoding="utf-8") as f:
        return hashlib.sha256(f.read().encode()).hexdigest()

def _make_variant_seed(generation: int, seed: str) -> int:
    """Derive a deterministic integer from (generation, seed) for mutation selection."""
    combined = f"gen:{generation}:seed:{seed}"
    digest = hashlib.sha256(combined.encode()).digest()
    return int.from_bytes(digest[:4], "little")

def generate_bounded_c_template(generation: int, seed: str) -> str:
    """
    Generate the full voxel benchmark C source.

    generation == 0 always selects variant 0 for both mutable regions so the
    baseline checksum is reproducible regardless of seed.  Generations >= 1
    select variants deterministically from (generation, seed).
    """
    if generation == 0:
        gchunk_variant = 0
        raycast_variant = 0
    else:
        vseed = _make_variant_seed(generation, seed)
        gchunk_variant = vseed % len(_VOXEL_GET_CHUNK_VARIANTS)
        raycast_variant = (vseed >> 8) % len(_RAYCAST_VOXEL_VARIANTS)

    header = (
        f"// Generation: {generation}  Seed: {seed}\n"
        + _PROTECTED_HEADER
    )
    mutable = (
        _VOXEL_GET_CHUNK_VARIANTS[gchunk_variant]
        + "\n"
        + _RAYCAST_VOXEL_VARIANTS[raycast_variant]
    )
    return header + mutable + _PROTECTED_FOOTER

def parse_voxel_output(stdout: str):
    """
    Parse ENERGY and CHECKSUM from voxel benchmark stdout.

    Returns (energy: float, checksum: int) on success, or (None, None) on any
    parse failure or non-finite energy value.  Both fields must be present.
    """
    energy = None
    checksum = None
    for line in stdout.split("\n"):
        line = line.strip()
        if line.startswith("ENERGY:"):
            try:
                val = float(line[len("ENERGY:"):].strip())
                if math.isnan(val) or math.isinf(val):
                    logger.error("NON-FINITE ENERGY detected. FAILING CLOSED.")
                    return None, None
                energy = val
            except ValueError:
                logger.error("MALFORMED ENERGY line. FAILING CLOSED.")
                return None, None
        elif line.startswith("CHECKSUM:"):
            try:
                checksum = int(line[len("CHECKSUM:"):].strip())
            except ValueError:
                logger.error("MALFORMED CHECKSUM line. FAILING CLOSED.")
                return None, None
    if energy is None:
        logger.error("ENERGY line missing from stdout. FAILING CLOSED.")
        return None, None
    if checksum is None:
        logger.error("CHECKSUM line missing from stdout. FAILING CLOSED.")
        return None, None
    return energy, checksum

def compute_baseline_checksum(policy: dict) -> "int | None":
    """
    Compile and run the unmutated (generation=0) harness to pin the baseline
    checksum.  Returns the checksum integer on success, None on any failure.
    """
    scratch_c   = Path("sandbox_gen.c")
    scratch_out = Path("a.out")

    c_code = generate_bounded_c_template(0, "baseline")
    scratch_c.write_text(c_code, encoding="utf-8")

    is_valid, reason = fortify_check_file(scratch_c)
    if not is_valid:
        logger.error(f"BASELINE FORTIFY FAILED: {reason}. Cannot pin checksum.")
        return None

    code, stdout, stderr = run_bounded_subprocess(
        COMPILER_CMD,
        timeout_sec=policy["max_compile_seconds"],
        max_stdout_bytes=policy["max_stdout_bytes"],
        max_stderr_bytes=policy["max_stderr_bytes"],
    )
    if code != 0:
        logger.error(f"BASELINE COMPILE FAILED (exit {code}). Cannot pin checksum.")
        return None

    if not scratch_out.exists():
        logger.error("BASELINE BINARY NOT EMITTED. Cannot pin checksum.")
        return None

    code, stdout, stderr = run_bounded_subprocess(
        EXEC_CMD,
        timeout_sec=policy["max_runtime_seconds"],
        max_stdout_bytes=policy["max_stdout_bytes"],
        max_stderr_bytes=policy["max_stderr_bytes"],
    )
    if code != 0:
        logger.error(f"BASELINE EXEC FAILED (exit {code}). Cannot pin checksum.")
        return None

    _, checksum = parse_voxel_output(stdout)
    if checksum is None:
        logger.error("BASELINE CHECKSUM MISSING from stdout. Cannot pin checksum.")
        return None

    logger.info(f"Baseline checksum pinned: {checksum}")
    return checksum

def orchestrate():
    # Production Execution Guard
    assert_sandbox_mode()

    logger.info("Initializing Voxel Graveyard Optimizer Sandbox...")

    policy = load_policy()
    max_generations = policy["max_generations"]
    seed = os.environ.get("CHIMERA_SEED", "default-seed")
    report_path = os.environ.get("CHIMERA_REPORT_PATH", "chimera_run_report.json")

    prov = get_compiler_provenance()

    report = {
        "status": "ok",
        "sandbox_mode": True,
        "seed": seed,
        "policy_hash": f"sha256:{get_policy_hash()}",
        "compiler_version": prov.get("compiler_version", "unknown"),
        "compiler_provenance": prov,
        "python_version": sys.version.split(" ")[0],
        "generations_requested": max_generations,
        "generations_completed": 0,
        "source_hashes": [],
        "baseline_checksum": None,
        "errors": [],
        "artifacts_cleaned": False,
        "duration_ms": 0,
        "compiler": "wsl gcc",
        "runtime": "sandbox",
        "native_execution_attempted": False,
        "native_execution_blocked": False,
    }

    start_time = time.time()

    scratch_c   = Path("sandbox_gen.c")
    scratch_out = Path("a.out")
    scratch_asm = Path("a.out.s")

    try:
        # ----------------------------------------------------------------
        # Step 0: Pin baseline checksum from the unmutated reference harness.
        # No generation is scored until correctness is proven against this pin.
        # ----------------------------------------------------------------
        logger.info("--- BASELINE CHECKSUM PINNING ---")
        baseline_checksum = compute_baseline_checksum(policy)
        if baseline_checksum is None:
            logger.error("Cannot establish baseline checksum. FAILING CLOSED.")
            report["status"] = "failed"
            report["errors"].append({
                "code": "baseline_checksum_failed",
                "message": "Baseline compile/run/parse failed; cannot pin correctness gate.",
                "generation": 0,
            })
            return  # jumps to finally
        report["baseline_checksum"] = baseline_checksum
        logger.info(f"Baseline pinned: CHECKSUM={baseline_checksum}")

        # ----------------------------------------------------------------
        # Tournament loop: mutate, verify correctness, then score by energy.
        # ----------------------------------------------------------------
        for gen in range(1, max_generations + 1):
            logger.info(f"--- GENERATION {gen} ---")

            c_code = generate_bounded_c_template(gen, seed)
            scratch_c.write_text(c_code, encoding="utf-8")

            source_hash = hashlib.sha256(c_code.encode()).hexdigest()
            logger.info(f"Source Hash: {source_hash}")
            report["source_hashes"].append(f"sha256:{source_hash}")

            # Gate 1: static analysis / fortify scan
            is_valid, reason = fortify_check_file(scratch_c)
            if not is_valid:
                logger.error(f"FORTIFY REJECTED Gen {gen}: {reason}. FAILING CLOSED.")
                report["status"] = "failed"
                report["native_execution_blocked"] = True
                report["errors"].append({"code": "fortify_rejected", "message": reason, "generation": gen})
                break
            logger.info("Fortify Gate: PASSED")

            # Gate 2: compilation
            code, stdout, stderr = run_bounded_subprocess(
                COMPILER_CMD,
                timeout_sec=policy["max_compile_seconds"],
                max_stdout_bytes=policy["max_stdout_bytes"],
                max_stderr_bytes=policy["max_stderr_bytes"],
            )
            if code != 0:
                logger.error(f"COMPILER REJECTED Gen {gen}. Exit code: {code}. FAILING CLOSED.")
                logger.error(f"Stderr: {stderr}")
                report["status"] = "failed"
                report["native_execution_blocked"] = True
                report["errors"].append({
                    "code": "compiler_error",
                    "message": stderr.strip() if code != -3 else "compiler_output_limit_exceeded",
                    "generation": gen,
                })
                break
            logger.info("Compilation: PASSED")

            if not scratch_out.exists():
                logger.error("EXECUTION FAILED: Binary not emitted. FAILING CLOSED.")
                report["status"] = "failed"
                report["native_execution_blocked"] = True
                report["errors"].append({"code": "binary_missing", "message": "Binary not emitted", "generation": gen})
                break

            # Gate 3: sandbox execution (timeout enforced by policy)
            report["native_execution_attempted"] = True
            code, stdout, stderr = run_bounded_subprocess(
                EXEC_CMD,
                timeout_sec=policy["max_runtime_seconds"],
                max_stdout_bytes=policy["max_stdout_bytes"],
                max_stderr_bytes=policy["max_stderr_bytes"],
            )
            if code != 0:
                logger.warning(
                    f"RUNTIME FAULT Gen {gen}. Exit code: {code}. "
                    f"Genome REJECTED (non-zero exit)."
                )
                report["errors"].append({
                    "code": "runtime_fault",
                    "message": f"Exit code {code}" if code != -3 else "runtime_output_limit_exceeded",
                    "generation": gen,
                })
                # Non-fatal: continue to next generation (genome is just rejected)
                continue

            # Gate 4: parse ENERGY + CHECKSUM
            energy, checksum = parse_voxel_output(stdout)
            if energy is None or checksum is None:
                logger.warning(f"MALFORMED OUTPUT Gen {gen}. Genome REJECTED (bad stdout).")
                report["errors"].append({
                    "code": "output_parse_failed",
                    "message": "Missing or malformed ENERGY/CHECKSUM",
                    "generation": gen,
                })
                continue

            # Gate 5: correctness – checksum must match the pinned baseline
            if checksum != baseline_checksum:
                logger.warning(
                    f"CHECKSUM MISMATCH Gen {gen}: got {checksum}, "
                    f"expected {baseline_checksum}. Genome REJECTED."
                )
                report["errors"].append({
                    "code": "checksum_mismatch",
                    "message": f"got={checksum} expected={baseline_checksum}",
                    "generation": gen,
                })
                continue

            # Accepted: correctness proven, score by median energy (lower = faster)
            logger.info(
                f"Execution: PASSED. Correctness VERIFIED. "
                f"ENERGY={energy:.9f} CHECKSUM={checksum}"
            )
            report["generations_completed"] += 1

        if report["status"] == "ok" and report["generations_completed"] == max_generations:
            logger.info("Voxel Graveyard Optimizer: Tournament completed cleanly.")

    except Exception as e:
        report["status"] = "failed"
        report["errors"].append({
            "code": "unhandled_exception",
            "message": str(e),
            "generation": report["generations_completed"] + 1,
        })

    finally:
        artifacts_cleaned = True
        quarantine_path = None
        for f in [scratch_c, scratch_out, scratch_asm, Path("sandbox_gen_pre.c")]:
            if f.exists():
                try:
                    f.unlink()
                except Exception as e:
                    logger.error(f"Failed to clean artifact {f.name}: {e}")
                    artifacts_cleaned = False

        if not artifacts_cleaned:
            import uuid
            import shutil
            from alerting import emit_alert
            quarantine_dir = Path("quarantine") / f"run-{uuid.uuid4().hex[:8]}"
            quarantine_dir.mkdir(parents=True, exist_ok=True)

            for f in [scratch_c, scratch_out, scratch_asm, Path("sandbox_gen_pre.c")]:
                if f.exists():
                    try:
                        shutil.move(str(f), str(quarantine_dir / f.name))
                    except: pass

            quarantine_path = str(quarantine_dir)
            logger.error(f"FATAL: Cleanup failed. Scratch files moved to {quarantine_path}")
            emit_alert("critical", "cleanup_failure", "chimera_daemon", "scratch_quarantined")
            report["status"] = "failed"
            report["quarantine_path"] = quarantine_path
            report["release_blocked"] = True
        else:
            logger.info("Scratch environment cleaned.")

        report["artifacts_cleaned"] = artifacts_cleaned
        report["duration_ms"] = int((time.time() - start_time) * 1000)

        with open(report_path, "w", encoding="utf-8") as f:
            json.dump(report, f, indent=2)

if __name__ == "__main__":
    orchestrate()
