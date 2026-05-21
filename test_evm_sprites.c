#include "zcc_svg.h"
#include "zcc_anim.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

// Universal structure for EVM Exploit Topologies
typedef struct {
    Vec3 pos;
    float radius;
    const char* label;
    const char* opcode;
    const char* color;
} EvmNode;

typedef struct {
    int from;
    int to;
    float radius;
} EvmEdge;

// Scenario IDs
enum {
    SCENARIO_REENTRANCY = 0,
    SCENARIO_FLASHLOAN,
    SCENARIO_GOVERNANCE,
    SCENARIO_FRONTRUN,
    SCENARIO_OVERFLOW,
    SCENARIO_HONEYPOT,
    NUM_SCENARIOS
};

// -------------------------------------------------------------
// 1. REENTRANCY DATA & FUNCTIONS
// -------------------------------------------------------------
#define REENT_NODES 6
static EvmNode reent_nodes[REENT_NODES] = {
    {{0.0f, 15.0f, 0.0f}, 4.2f, "0x00_ENTRY", "PUSH1 0x80 / MSTORE", "#ff00ff"},
    {{-15.0f, 5.0f, -5.0f}, 3.8f, "0x15_SELECTOR", "CALLDATALOAD / EQ", "#00ffff"},
    {{15.0f, 5.0f, 5.0f}, 4.5f, "0x42_REENTRANCY", "CALL (REENTRANT)", "#ff007f"},
    {{5.0f, -8.0f, -8.0f}, 3.6f, "0x60_MUTATION", "SSTORE (BALANCES)", "#ffaa00"},
    {{-10.0f, -15.0f, 10.0f}, 4.0f, "0x88_DRAIN", "TRANSFER / GAS", "#ff0033"},
    {{18.0f, -12.0f, -5.0f}, 3.5f, "0x99_SAFE_EXIT", "REVERT / RETURN", "#00ff66"}
};
#define REENT_EDGES 6
static EvmEdge reent_edges[REENT_EDGES] = {
    {0, 1, 0.7f}, {1, 2, 0.7f}, {1, 5, 0.6f}, {2, 3, 0.7f}, {3, 4, 0.7f}, {4, 2, 0.8f}
};
static void reent_pos_fn(int index, float phase, Vec3* pos, float* radius) {
    EvmNode base = reent_nodes[index];
    *radius = base.radius; *pos = base.pos;
    if (index == 2) {
        *radius = base.radius * (1.0f + 0.32f * sinf(phase * 4.0f * M_PI));
        float r = 16.0f + 3.0f * sinf(phase * 2.0f * M_PI);
        pos->x = reent_nodes[1].pos.x + r * cosf(phase * 2.0f * M_PI);
        pos->z = reent_nodes[1].pos.z + r * sinf(phase * 2.0f * M_PI);
    } else if (index == 4) {
        float decay = 1.0f - 0.25f * sinf(phase * M_PI);
        *radius = base.radius * decay;
        pos->y = base.pos.y + 5.0f * sinf(phase * 6.0f * M_PI);
        pos->x = base.pos.x + 3.0f * cosf(phase * 4.0f * M_PI);
    } else if (index == 3) {
        float r = 7.5f;
        Vec3 target; float target_rad;
        reent_pos_fn(2, phase, &target, &target_rad);
        pos->x = target.x + r * cosf(phase * 4.0f * M_PI);
        pos->y = target.y + r * sinf(phase * 4.0f * M_PI);
    }
}
static float reent_sdf(Vec3 p, ChaosPhasor* ph) {
    Vec3 pr = p; rotate_y(&pr, (float)ph->phase * 2.0f * M_PI);
    float d = 1e9f;
    Vec3 anim_pos[REENT_NODES]; float anim_rad[REENT_NODES];
    for (int i = 0; i < REENT_NODES; i++) reent_pos_fn(i, (float)ph->phase, &anim_pos[i], &anim_rad[i]);
    for (int i = 0; i < REENT_NODES; i++) {
        float d_node = length((Vec3){pr.x - anim_pos[i].x, pr.y - anim_pos[i].y, pr.z - anim_pos[i].z}) - anim_rad[i];
        d = sdSmoothUnion(d, d_node, 3.2f);
    }
    for (int i = 0; i < REENT_EDGES; i++) {
        float d_edge = sdCapsule(pr, anim_pos[reent_edges[i].from], anim_pos[reent_edges[i].to], reent_edges[i].radius);
        d = sdSmoothUnion(d, d_edge, 2.5f);
    }
    float koch = sdKochSnowflake((Vec3){pr.x - anim_pos[0].x, pr.y - anim_pos[0].y, pr.z - anim_pos[0].z}, 2.5f, 4, 0.4f);
    d = sdSmoothUnion(d, koch * 2.0f, 1.8f);
    d += anim_fbm_3d(pr.x * 0.2f, pr.y * 0.2f, pr.z * 0.2f, 4) * 0.15f;
    return d;
}

// -------------------------------------------------------------
// 2. FLASHLOAN DATA & FUNCTIONS
// -------------------------------------------------------------
#define FLASH_NODES 5
static EvmNode flash_nodes[FLASH_NODES] = {
    {{0.0f, 0.0f, 0.0f}, 6.0f, "0x00_POOL", "FLASHLOAN 100K WETH", "#00ffcc"},
    {{-16.0f, 10.0f, -5.0f}, 3.5f, "0x12_BORROW", "DEFI_LOAN / INIT", "#ffaa00"},
    {{15.0f, 12.0f, 5.0f}, 4.2f, "0x25_DEX_A", "EXCHANGE / WETH_DAI", "#ff007f"},
    {{12.0f, -12.0f, -5.0f}, 4.2f, "0x3A_DEX_B", "EXCHANGE / DAI_WETH", "#ff00ff"},
    {{-15.0f, -10.0f, 8.0f}, 3.8f, "0x4F_REPAY", "REPAY / TRANSFER", "#00ff66"}
};
#define FLASH_EDGES 5
static EvmEdge flash_edges[FLASH_EDGES] = {
    {0, 1, 0.7f}, {1, 2, 0.7f}, {2, 3, 0.8f}, {3, 4, 0.7f}, {4, 0, 0.9f}
};
static void flash_pos_fn(int index, float phase, Vec3* pos, float* radius) {
    EvmNode base = flash_nodes[index];
    *radius = base.radius; *pos = base.pos;
    if (index == 0) {
        *radius = base.radius * (1.0f + 0.28f * sinf(phase * 2.0f * M_PI));
    } else {
        float angle = phase * 2.0f * M_PI + (float)index * (2.0f * M_PI / (float)(FLASH_NODES - 1));
        float r = 16.0f + 2.0f * sinf(phase * 4.0f * M_PI + (float)index);
        pos->x = r * cosf(angle); pos->z = r * sinf(angle);
        pos->y = base.pos.y + 2.0f * cosf(phase * 2.0f * M_PI + (float)index);
    }
}
static float flash_sdf(Vec3 p, ChaosPhasor* ph) {
    Vec3 pr = p; rotate_y(&pr, (float)ph->phase * 2.0f * M_PI);
    float d = 1e9f;
    Vec3 anim_pos[FLASH_NODES]; float anim_rad[FLASH_NODES];
    for (int i = 0; i < FLASH_NODES; i++) flash_pos_fn(i, (float)ph->phase, &anim_pos[i], &anim_rad[i]);
    for (int i = 0; i < FLASH_NODES; i++) {
        float d_node = length((Vec3){pr.x - anim_pos[i].x, pr.y - anim_pos[i].y, pr.z - anim_pos[i].z}) - anim_rad[i];
        d = sdSmoothUnion(d, d_node, 3.0f);
    }
    for (int i = 0; i < FLASH_EDGES; i++) {
        float d_edge = sdCapsule(pr, anim_pos[flash_edges[i].from], anim_pos[flash_edges[i].to], flash_edges[i].radius);
        d = sdSmoothUnion(d, d_edge, 2.2f);
    }
    float ring_radius = 12.0f + 6.0f * sinf((float)ph->phase * 2.0f * M_PI);
    float torus = sdTorus((Vec3){pr.x - anim_pos[0].x, pr.y - anim_pos[0].y, pr.z - anim_pos[0].z}, (Vec2){ring_radius, 0.8f});
    d = sdSmoothUnion(d, torus, 2.5f);
    d += anim_fbm_3d(pr.x * 0.2f, pr.y * 0.2f, pr.z * 0.2f, 4) * 0.15f;
    return d;
}

// -------------------------------------------------------------
// 3. GOVERNANCE DATA & FUNCTIONS
// -------------------------------------------------------------
#define GOV_NODES 5
static EvmNode gov_nodes[GOV_NODES] = {
    {{0.0f, 0.0f, 0.0f}, 5.5f, "0x00_TREASURY", "BAL: 15,000,000 WETH", "#00ff66"},
    {{-15.0f, 12.0f, -5.0f}, 3.5f, "0x08_VOTE_POW", "FLASH_VOTE / LOAD", "#00ffff"},
    {{16.0f, 10.0f, 5.0f}, 4.0f, "0x1A_BAD_PROP", "PROPOSE / DRAIN", "#ff007f"},
    {{10.0f, -12.0f, -6.0f}, 3.8f, "0x2F_TIMELOCK", "QUEUE / EXPLOIT", "#ffaa00"},
    {{-14.0f, -10.0f, 8.0f}, 4.2f, "0x3D_EXECUTE", "EXECUTE / RUN", "#ff0033"}
};
#define GOV_EDGES 5
static EvmEdge gov_edges[GOV_EDGES] = {
    {1, 2, 0.7f}, {2, 3, 0.8f}, {3, 4, 0.7f}, {4, 0, 0.9f}, {0, 1, 0.6f}
};
static void gov_pos_fn(int index, float phase, Vec3* pos, float* radius) {
    EvmNode base = gov_nodes[index];
    *radius = base.radius; *pos = base.pos;
    if (index == 0) {
        *radius = base.radius * (1.0f + 0.20f * sinf(phase * 4.0f * M_PI));
    } else {
        float angle = phase * 2.0f * M_PI + (float)index * (2.0f * M_PI / (float)(GOV_NODES - 1));
        float r = 15.5f;
        pos->x = r * cosf(angle); pos->y = r * sinf(angle);
        pos->z = base.pos.z + 3.0f * sinf(phase * 4.0f * M_PI + (float)index);
    }
}
static float gov_sdf(Vec3 p, ChaosPhasor* ph) {
    Vec3 pr = p; rotate_y(&pr, (float)ph->phase * 2.0f * M_PI);
    float d = 1e9f;
    Vec3 anim_pos[GOV_NODES]; float anim_rad[GOV_NODES];
    for (int i = 0; i < GOV_NODES; i++) gov_pos_fn(i, (float)ph->phase, &anim_pos[i], &anim_rad[i]);
    for (int i = 0; i < GOV_NODES; i++) {
        float d_node = length((Vec3){pr.x - anim_pos[i].x, pr.y - anim_pos[i].y, pr.z - anim_pos[i].z}) - anim_rad[i];
        d = sdSmoothUnion(d, d_node, 2.8f);
    }
    for (int i = 0; i < GOV_EDGES; i++) {
        float d_edge = sdCapsule(pr, anim_pos[gov_edges[i].from], anim_pos[gov_edges[i].to], gov_edges[i].radius);
        d = sdSmoothUnion(d, d_edge, 2.2f);
    }
    float sponge = sdMengerSponge((Vec3){pr.x - anim_pos[0].x, pr.y - anim_pos[0].y, pr.z - anim_pos[0].z}, 3);
    d = sdSmoothUnion(d, sponge * 2.2f, 2.0f);
    d += anim_fbm_3d(pr.x * 0.2f, pr.y * 0.2f, pr.z * 0.2f, 4) * 0.15f;
    return d;
}

// -------------------------------------------------------------
// 4. FRONTRUN DATA & FUNCTIONS
// -------------------------------------------------------------
#define MEV_NODES 5
static EvmNode mev_nodes[MEV_NODES] = {
    {{0.0f, 15.0f, 0.0f}, 4.0f, "0x01_MEMPOOL", "GAS_PRICE: 250 GWEI", "#00ffcc"},
    {{-12.0f, 2.0f, -6.0f}, 3.8f, "0x12_FRONTRUN", "EXCHANGE / BUY", "#ffaa00"},
    {{0.0f, -5.0f, 0.0f}, 4.8f, "0x25_VICTIM", "EXCHANGE / SLIP_MAX", "#ff007f"},
    {{12.0f, 2.0f, 6.0f}, 3.8f, "0x38_BACKRUN", "EXCHANGE / SELL", "#ff00ff"},
    {{0.0f, -16.0f, 0.0f}, 4.2f, "0x4A_PROFIT", "COLLECT / REVENUE", "#00ff66"}
};
#define MEV_EDGES 5
static EvmEdge mev_edges[MEV_EDGES] = {
    {0, 1, 0.7f}, {1, 2, 0.8f}, {2, 3, 0.8f}, {3, 4, 0.7f}, {4, 0, 0.6f}
};
static void mev_pos_fn(int index, float phase, Vec3* pos, float* radius) {
    EvmNode base = mev_nodes[index];
    *radius = base.radius; *pos = base.pos;
    if (index == 2) {
        *radius = base.radius * (1.0f + 0.22f * sinf(phase * 4.0f * M_PI));
    } else if (index == 1 || index == 3) {
        float angle = phase * 2.0f * M_PI + (index == 1 ? 0.0f : M_PI);
        float r = 14.0f;
        pos->x = r * cosf(angle); pos->z = r * sinf(angle);
        pos->y = 5.0f * sinf(phase * 4.0f * M_PI);
    } else {
        pos->y = base.pos.y + 2.0f * sinf(phase * 2.0f * M_PI + (float)index);
    }
}
static float mev_sdf(Vec3 p, ChaosPhasor* ph) {
    Vec3 pr = p; rotate_y(&pr, (float)ph->phase * 2.0f * M_PI);
    float d = 1e9f;
    Vec3 anim_pos[MEV_NODES]; float anim_rad[MEV_NODES];
    for (int i = 0; i < MEV_NODES; i++) mev_pos_fn(i, (float)ph->phase, &anim_pos[i], &anim_rad[i]);
    for (int i = 0; i < MEV_NODES; i++) {
        float d_node = length((Vec3){pr.x - anim_pos[i].x, pr.y - anim_pos[i].y, pr.z - anim_pos[i].z}) - anim_rad[i];
        d = sdSmoothUnion(d, d_node, 2.8f);
    }
    for (int i = 0; i < MEV_EDGES; i++) {
        float d_edge = sdCapsule(pr, anim_pos[mev_edges[i].from], anim_pos[mev_edges[i].to], mev_edges[i].radius);
        d = sdSmoothUnion(d, d_edge, 2.2f);
    }
    float double_helix = sdCapsule(pr, anim_pos[1], anim_pos[3], 0.9f);
    d = sdSmoothUnion(d, double_helix, 2.0f);
    d += anim_fbm_3d(pr.x * 0.2f, pr.y * 0.2f, pr.z * 0.2f, 4) * 0.15f;
    return d;
}

// -------------------------------------------------------------
// 5. OVERFLOW DATA & FUNCTIONS
// -------------------------------------------------------------
#define OVER_NODES 5
static EvmNode over_nodes[OVER_NODES] = {
    {{0.0f, 0.0f, 0.0f}, 4.5f, "0x00_MATH_HUB", "INIT: uint256_MAX", "#ffaa00"},
    {{-15.0f, 10.0f, -5.0f}, 3.8f, "0x12_ADD_ONE", "ADD 1 (NO_SAFE)", "#00ffff"},
    {{16.0f, 12.0f, 6.0f}, 5.5f, "0x28_OVERFLOW", "OVERFLOW / WRAP_0", "#ff007f"},
    {{10.0f, -12.0f, -6.0f}, 3.8f, "0x3C_CHECK", "REQUIRE (BAL == 0)", "#ff00ff"},
    {{-14.0f, -10.0f, 8.0f}, 4.2f, "0x4E_DRAIN", "WITHDRAW / LEAK", "#ff0033"}
};
#define OVER_EDGES 5
static EvmEdge over_edges[OVER_EDGES] = {
    {0, 1, 0.7f}, {1, 2, 0.8f}, {2, 3, 0.8f}, {3, 4, 0.7f}, {4, 0, 0.6f}
};
static void over_pos_fn(int index, float phase, Vec3* pos, float* radius) {
    EvmNode base = over_nodes[index];
    *radius = base.radius; *pos = base.pos;
    if (index == 2) {
        float scale = 1.0f + 0.65f * sinf(phase * 2.0f * M_PI);
        *radius = base.radius * scale;
        pos->x = base.pos.x + 3.0f * cosf(phase * 4.0f * M_PI);
    } else {
        float angle = phase * 2.0f * M_PI + (float)index * (2.0f * M_PI / (float)(OVER_NODES - 1));
        float r = 16.0f;
        pos->x = r * cosf(angle); pos->z = r * sinf(angle);
        pos->y = base.pos.y + 2.0f * sinf(phase * 2.0f * M_PI + (float)index);
    }
}
static float over_sdf(Vec3 p, ChaosPhasor* ph) {
    Vec3 pr = p; rotate_y(&pr, (float)ph->phase * 2.0f * M_PI);
    float d = 1e9f;
    Vec3 anim_pos[OVER_NODES]; float anim_rad[OVER_NODES];
    for (int i = 0; i < OVER_NODES; i++) over_pos_fn(i, (float)ph->phase, &anim_pos[i], &anim_rad[i]);
    for (int i = 0; i < OVER_NODES; i++) {
        float d_node = length((Vec3){pr.x - anim_pos[i].x, pr.y - anim_pos[i].y, pr.z - anim_pos[i].z}) - anim_rad[i];
        d = sdSmoothUnion(d, d_node, 3.0f);
    }
    for (int i = 0; i < OVER_EDGES; i++) {
        float d_edge = sdCapsule(pr, anim_pos[over_edges[i].from], anim_pos[over_edges[i].to], over_edges[i].radius);
        d = sdSmoothUnion(d, d_edge, 2.2f);
    }
    float mandel = sdMandelbulb((Vec3){pr.x - anim_pos[0].x, pr.y - anim_pos[0].y, pr.z - anim_pos[0].z}, 5, 4.0f);
    d = sdSmoothUnion(d, mandel * 2.5f, 2.0f);
    d += anim_fbm_3d(pr.x * 0.2f, pr.y * 0.2f, pr.z * 0.2f, 4) * 0.15f;
    return d;
}

// -------------------------------------------------------------
// 6. HONEYPOT DATA & FUNCTIONS
// -------------------------------------------------------------
#define HONEY_NODES 5
static EvmNode honey_nodes[HONEY_NODES] = {
    {{0.0f, 0.0f, 0.0f}, 6.0f, "0x00_BAIT", "BAL: 2,500 WETH (OPEN)", "#00ff66"},
    {{-16.0f, 8.0f, -5.0f}, 3.8f, "0x15_DEPOSIT", "PAYABLE / DEPOSIT", "#00ffff"},
    {{15.0f, 10.0f, 5.0f}, 4.2f, "0x2A_LOCKED", "WITHDRAW / REVERT", "#ff0033"},
    {{12.0f, -12.0f, -6.0f}, 3.5f, "0x3C_BACKDOOR", "ONLY_OWNER / DOOR", "#ffaa00"},
    {{-14.0f, -10.0f, 8.0f}, 4.0f, "0x4E_SIPHON", "TRANSFER / SIPHON", "#ff00ff"}
};
#define HONEY_EDGES 5
static EvmEdge honey_edges[HONEY_EDGES] = {
    {1, 0, 0.8f}, {0, 2, 0.7f}, {3, 0, 0.7f}, {0, 4, 0.8f}, {4, 1, 0.6f}
};
static void honey_pos_fn(int index, float phase, Vec3* pos, float* radius) {
    EvmNode base = honey_nodes[index];
    *radius = base.radius; *pos = base.pos;
    if (index == 0) {
        pos->x = 2.0f * cosf(phase * 2.0f * M_PI); pos->y = 2.0f * sinf(phase * 2.0f * M_PI);
    } else {
        float angle = phase * 2.0f * M_PI + (float)index * (2.0f * M_PI / (float)(HONEY_NODES - 1));
        float r = 16.5f + 3.0f * sinf(phase * 4.0f * M_PI + (float)index);
        pos->x = r * cosf(angle); pos->z = r * sinf(angle);
        pos->y = base.pos.y + 2.0f * cosf(phase * 2.0f * M_PI + (float)index);
    }
}
static float honey_sdf(Vec3 p, ChaosPhasor* ph) {
    Vec3 pr = p; rotate_y(&pr, (float)ph->phase * 2.0f * M_PI);
    float d = 1e9f;
    Vec3 anim_pos[HONEY_NODES]; float anim_rad[HONEY_NODES];
    for (int i = 0; i < HONEY_NODES; i++) honey_pos_fn(i, (float)ph->phase, &anim_pos[i], &anim_rad[i]);
    for (int i = 0; i < HONEY_NODES; i++) {
        float d_node = length((Vec3){pr.x - anim_pos[i].x, pr.y - anim_pos[i].y, pr.z - anim_pos[i].z}) - anim_rad[i];
        d = sdSmoothUnion(d, d_node, 2.8f);
    }
    for (int i = 0; i < HONEY_EDGES; i++) {
        float d_edge = sdCapsule(pr, anim_pos[honey_edges[i].from], anim_pos[honey_edges[i].to], honey_edges[i].radius);
        d = sdSmoothUnion(d, d_edge, 2.2f);
    }
    float honey_core = sdTorus((Vec3){pr.x - anim_pos[0].x, pr.y - anim_pos[0].y, pr.z - anim_pos[0].z}, (Vec2){5.0f, 1.2f});
    float sponge = sdMengerSponge((Vec3){pr.x - anim_pos[0].x, pr.y - anim_pos[0].y, pr.z - anim_pos[0].z}, 2);
    float spiky_bait = sdSubtraction(honey_core, sponge);
    d = sdSmoothUnion(d, spiky_bait, 2.0f);
    d += anim_fbm_3d(pr.x * 0.2f, pr.y * 0.2f, pr.z * 0.2f, 4) * 0.15f;
    return d;
}

// -------------------------------------------------------------
// UNIVERSAL SCENARIO DEFINITION
// -------------------------------------------------------------
typedef float (*SdfFn)(Vec3, ChaosPhasor*);
typedef void (*NodePosFn)(int, float, Vec3*, float*);

typedef struct {
    const char* id;
    const char* title;
    int num_nodes;
    EvmNode* nodes;
    int num_edges;
    EvmEdge* edges;
    SdfFn sdf;
    NodePosFn pos_fn;
} Scenario;

static Scenario scenarios[NUM_SCENARIOS] = {
    {"reentrancy", "🔱 REENTRANCY ATTRACTOR", REENT_NODES, reent_nodes, REENT_EDGES, reent_edges, reent_sdf, reent_pos_fn},
    {"flashloan", "🔱 FLASHLOAN VORTEX", FLASH_NODES, flash_nodes, FLASH_EDGES, flash_edges, flash_sdf, flash_pos_fn},
    {"governance", "🔱 GOVERNANCE TAKEOVER", GOV_NODES, gov_nodes, GOV_EDGES, gov_edges, gov_sdf, gov_pos_fn},
    {"frontrun", "🔱 MEV SANDWICH ATTACK", MEV_NODES, mev_nodes, MEV_EDGES, mev_edges, mev_sdf, mev_pos_fn},
    {"overflow", "🔱 INTEGER OVERFLOW", OVER_NODES, over_nodes, OVER_EDGES, over_edges, over_sdf, over_pos_fn},
    {"honeypot", "🔱 HONEYPOT HONEY_TRAP", HONEY_NODES, honey_nodes, HONEY_EDGES, honey_edges, honey_sdf, honey_pos_fn}
};

// -------------------------------------------------------------
// UNIVERSAL RAY MARCHER
// -------------------------------------------------------------
static Vec3 sprites_get_normal(Vec3 p, ChaosPhasor* ph, SdfFn sdf) {
    Vec3 e = {EPSILON, 0.0f, 0.0f};
    Vec3 n = {
        sdf((Vec3){p.x + e.x, p.y, p.z}, ph) - sdf((Vec3){p.x - e.x, p.y, p.z}, ph),
        sdf((Vec3){p.x, p.y + e.x, p.z}, ph) - sdf((Vec3){p.x, p.y - e.x, p.z}, ph),
        sdf((Vec3){p.x, p.y, p.z + e.x}, ph) - sdf((Vec3){p.x, p.y, p.z - e.x}, ph)
    };
    normalize(&n);
    return n;
}

static float sprites_ray_march(Ray r, ChaosPhasor* ph, Vec3* hit, Vec3* normal, SdfFn sdf) {
    float depth = 0.0f;
    for (int i = 0; i < MAX_STEPS; ++i) {
        Vec3 p = {r.ro.x + r.rd.x * depth, r.ro.y + r.rd.y * depth, r.ro.z + r.rd.z * depth};
        float dist = sdf(p, ph);
        depth += dist;
        if (dist < SURF_DIST) {
            *hit = p;
            *normal = sprites_get_normal(p, ph, sdf);
            return depth;
        }
        if (depth > MAX_DIST) break;
    }
    return MAX_DIST;
}

int main() {
    // Start composite SVG Sprite Sheet
    ZccSvgNode* svg = svg_svg();
    svg_set_xmlns(svg, "http://www.w3.org/2000/svg");
    svg_set_attr(svg, "style", "display: none;");

    ZccSvgNode* defs = svg_defs();
    
    // Gradient definitions
    ZccSvgNode* glowGrad = svg_linearGradient();
    svg_set_id(glowGrad, "glowGrad");
    svg_set_attr(glowGrad, "x1", "0%"); svg_set_attr(glowGrad, "y1", "0%");
    svg_set_attr(glowGrad, "x2", "100%"); svg_set_attr(glowGrad, "y2", "100%");
    ZccSvgNode* stop1 = svg_stop(); svg_set_offset(stop1, "0%"); svg_set_stop_color(stop1, "#ff007f"); svg_add_child(glowGrad, stop1);
    ZccSvgNode* stop2 = svg_stop(); svg_set_offset(stop2, "100%"); svg_set_stop_color(stop2, "#00ffff"); svg_add_child(glowGrad, stop2);
    svg_add_child(defs, glowGrad);
    
    svg_add_child(svg, defs);

    int num_frames = 15; // Balanced frame count to fit in sprite symbols cleanly

    // Set up camera matrix
    AnimVec3 eye = {0.0f, 0.0f, 52.0f};
    AnimVec3 target = {0.0f, 0.0f, 0.0f};
    AnimVec3 up = {0.0f, 1.0f, 0.0f};
    AnimMatrix4x4 view = anim_matrix_look_at(eye, target, up);
    AnimMatrix4x4 proj = anim_matrix_perspective(M_PI / 3.0f, 1.0f, 0.1f, 100.0f);
    AnimMatrix4x4 vp;
    anim_matrix_mul(&vp, &view, &proj);

    ChaosPhasor ph;
    chaos_phasor_init(&ph, 120.0);

    for (int s = 0; s < NUM_SCENARIOS; s++) {
        Scenario sc = scenarios[s];
        printf("Generating Symbol Sprite: #%s (%s)...\n", sc.id, sc.title);

        ZccSvgNode* symbol = svg_create_node("symbol");
        svg_set_id(symbol, sc.id);
        svg_set_attr(symbol, "viewBox", "0 0 800 800");

        // Obsidian black backdrop within the sprite symbol boundary box
        ZccSvgNode* box_bg = svg_rect();
        svg_set_attr(box_bg, "width", "100%");
        svg_set_attr(box_bg, "height", "100%");
        svg_set_fill(box_bg, "#020104");
        svg_add_child(symbol, box_bg);

        // Preallocate SVG tracking lists
        char* values_list = (char*)malloc(3 * 1024 * 1024);
        values_list[0] = '\0';
        char* first_frame_path = (char*)malloc(512 * 1024);
        first_frame_path[0] = '\0';

        char* circle_cx[20];
        char* circle_cy[20];
        char* text_x[20];
        char* text_y[20];
        for (int i = 0; i < sc.num_nodes; i++) {
            circle_cx[i] = (char*)malloc(16384); circle_cx[i][0] = '\0';
            circle_cy[i] = (char*)malloc(16384); circle_cy[i][0] = '\0';
            text_x[i] = (char*)malloc(16384); text_x[i][0] = '\0';
            text_y[i] = (char*)malloc(16384); text_y[i][0] = '\0';
        }

        for (int f = 0; f < num_frames; f++) {
            ph.phase = (double)f / (double)num_frames;
            SVGPath* path = svg_path_create();
            Vec3 light_dir = {0.5f, 0.7f, -0.5f};
            normalize(&light_dir);

            int width = 800, height = 800;
            int grid_step = 16; 

            for (int y = 0; y < height; y += grid_step) {
                for (int x = 0; x < width; x += grid_step) {
                    Vec3 ro = {0.0f, 0.0f, 52.0f};
                    Vec3 rd = {(x - width / 2.0f) / (float)width * 1.1f, (y - height / 2.0f) / (float)height * -1.1f, -1.0f};
                    normalize(&rd);

                    Vec3 hit, normal;
                    float d = sprites_ray_march((Ray){ro, rd}, &ph, &hit, &normal, sc.sdf);

                    if (d < MAX_DIST) {
                        float shade = fmaxf(0.18f, dot(normal, light_dir));
                        Vec3 view_dir = {-rd.x, -rd.y, -rd.z};
                        Vec3 half_dir = {light_dir.x + view_dir.x, light_dir.y + view_dir.y, light_dir.z + view_dir.z};
                        normalize(&half_dir);
                        float spec = powf(fmaxf(0.0f, dot(normal, half_dir)), 12.0f);
                        float intensity = shade + spec * 0.8f;

                        float sx = (float)x; float sy = (float)y;
                        float ex = sx + normal.x * intensity * 14.0f;
                        float ey = sy - normal.y * intensity * 14.0f;

                        svg_path_move_to(path, sx, sy);
                        svg_path_line_to(path, ex, ey);
                    }
                }
            }

            if (f == 0) strcpy(first_frame_path, path->d);
            strcat(values_list, path->d);
            if (f < num_frames - 1) strcat(values_list, ";");

            // Track lock-on reticle positions for this scenario
            for (int i = 0; i < sc.num_nodes; i++) {
                Vec3 rpos; float rad;
                sc.pos_fn(i, (float)f / (float)num_frames, &rpos, &rad);
                rotate_y(&rpos, (float)f / (float)num_frames * 2.0f * M_PI);
                AnimVec3 screen = anim_matrix_project(vp, rpos, 800.0f, 800.0f);
                
                char tmp[64];
                sprintf(tmp, "%.2f", screen.x); strcat(circle_cx[i], tmp);
                sprintf(tmp, "%.2f", screen.y); strcat(circle_cy[i], tmp);
                sprintf(tmp, "%.2f", screen.x - 45.0f); strcat(text_x[i], tmp);
                sprintf(tmp, "%.2f", screen.y - 15.0f); strcat(text_y[i], tmp);

                if (f < num_frames - 1) {
                    strcat(circle_cx[i], ";"); strcat(circle_cy[i], ";");
                    strcat(text_x[i], ";"); strcat(text_y[i], ";");
                }
            }

            svg_path_free(path);
        }

        // Add 4-layer trails inside symbol
        for (int g = 3; g >= 0; g--) {
            ZccSvgNode* morph_shape = svg_path();
            svg_set_fill(morph_shape, "none");
            svg_set_stroke(morph_shape, "url(#glowGrad)");
            svg_set_stroke_linecap(morph_shape, "round");
            char sw_str[32], op_str[32];
            sprintf(sw_str, "%.2f", 2.2f / (float)(g + 1));
            sprintf(op_str, "%.3f", 0.70f / (float)(g + 1));
            svg_set_stroke_width(morph_shape, sw_str);
            svg_set_opacity(morph_shape, op_str);
            svg_set_attr(morph_shape, "d", first_frame_path);

            ZccSvgNode* anim = svg_animate_path_morph(morph_shape, values_list, 5.0f, "indefinite");
            char begin_str[32];
            sprintf(begin_str, "-%.3fs", (float)g * 0.33f);
            svg_set_attr(anim, "begin", begin_str);
            svg_add_child(symbol, morph_shape);
        }

        // Add tracking HUD reticles inside symbol
        ZccSvgNode* hud_group = svg_create_node("g");
        svg_set_attr(hud_group, "font-family", "'Courier New', monospace");
        svg_set_attr(hud_group, "font-weight", "bold");
        svg_add_child(symbol, hud_group);

        for (int i = 0; i < sc.num_nodes; i++) {
            ZccSvgNode* node_circle = svg_circle();
            svg_set_attr(node_circle, "fill", sc.nodes[i].color);
            svg_set_attr(node_circle, "opacity", "0.18");
            char r_str[32];
            sprintf(r_str, "%.2f", sc.nodes[i].radius * 2.5f);
            svg_set_attr(node_circle, "r", r_str);
            
            ZccSvgNode* cx_anim = svg_create_node("animate");
            svg_set_attr(cx_anim, "attributeName", "cx");
            svg_set_attr(cx_anim, "dur", "5.0s");
            svg_set_attr(cx_anim, "values", circle_cx[i]);
            svg_set_attr(cx_anim, "repeatCount", "indefinite");
            svg_add_child(node_circle, cx_anim);
            
            ZccSvgNode* cy_anim = svg_create_node("animate");
            svg_set_attr(cy_anim, "attributeName", "cy");
            svg_set_attr(cy_anim, "dur", "5.0s");
            svg_set_attr(cy_anim, "values", circle_cy[i]);
            svg_set_attr(cy_anim, "repeatCount", "indefinite");
            svg_add_child(node_circle, cy_anim);
            svg_add_child(hud_group, node_circle);

            ZccSvgNode* text_node = svg_text();
            svg_set_attr(text_node, "font-size", "11px");
            svg_set_attr(text_node, "fill", sc.nodes[i].color);
            svg_set_content(text_node, sc.nodes[i].label);
            
            ZccSvgNode* tx_anim = svg_create_node("animate");
            svg_set_attr(tx_anim, "attributeName", "x");
            svg_set_attr(tx_anim, "dur", "5.0s");
            svg_set_attr(tx_anim, "values", text_x[i]);
            svg_set_attr(tx_anim, "repeatCount", "indefinite");
            svg_add_child(text_node, tx_anim);
            
            ZccSvgNode* ty_anim = svg_create_node("animate");
            svg_set_attr(ty_anim, "attributeName", "y");
            svg_set_attr(ty_anim, "dur", "5.0s");
            svg_set_attr(ty_anim, "values", text_y[i]);
            svg_set_attr(ty_anim, "repeatCount", "indefinite");
            svg_add_child(text_node, ty_anim);
            svg_add_child(hud_group, text_node);

            ZccSvgNode* sub_text = svg_text();
            svg_set_attr(sub_text, "font-size", "8.5px");
            svg_set_attr(sub_text, "fill", "#8888aa");
            svg_set_content(sub_text, sc.nodes[i].opcode);
            
            char* sub_y_list = (char*)malloc(16384);
            sub_y_list[0] = '\0';
            char* text_y_copy = strdup(text_y[i]);
            char* token = strtok(text_y_copy, ";");
            while (token) {
                float py = atof(token);
                char tmp[64];
                sprintf(tmp, "%.2f", py + 13.0f);
                strcat(sub_y_list, tmp);
                token = strtok(NULL, ";");
                if (token) strcat(sub_y_list, ";");
            }
            
            ZccSvgNode* stx_anim = svg_create_node("animate");
            svg_set_attr(stx_anim, "attributeName", "x");
            svg_set_attr(stx_anim, "dur", "5.0s");
            svg_set_attr(stx_anim, "values", text_x[i]);
            svg_set_attr(stx_anim, "repeatCount", "indefinite");
            svg_add_child(sub_text, stx_anim);
            
            ZccSvgNode* sty_anim = svg_create_node("animate");
            svg_set_attr(sty_anim, "attributeName", "y");
            svg_set_attr(sty_anim, "dur", "5.0s");
            svg_set_attr(sty_anim, "values", sub_y_list);
            svg_set_attr(sty_anim, "repeatCount", "indefinite");
            svg_add_child(sub_text, sty_anim);
            svg_add_child(hud_group, sub_text);
            
            free(text_y_copy);
            free(sub_y_list);
        }

        // Inner title label within sprite symbol boundary box
        ZccSvgNode* title_lbl = svg_text();
        svg_set_attr(title_lbl, "x", "30");
        svg_set_attr(title_lbl, "y", "40");
        svg_set_attr(title_lbl, "font-size", "12px");
        svg_set_attr(title_lbl, "fill", "#ff007f");
        svg_set_content(title_lbl, sc.title);
        svg_add_child(symbol, title_lbl);

        ZccSvgNode* border = svg_rect();
        svg_set_attr(border, "x", "15");
        svg_set_attr(border, "y", "15");
        svg_set_attr(border, "width", "770");
        svg_set_attr(border, "height", "770");
        svg_set_fill(border, "none");
        svg_set_attr(border, "stroke", "#112244");
        svg_set_attr(border, "stroke-width", "1.5");
        svg_add_child(symbol, border);

        // Add this symbol to our composite SVG sheet
        svg_add_child(svg, symbol);

        // Clean up buffers
        for (int i = 0; i < sc.num_nodes; i++) {
            free(circle_cx[i]); free(circle_cy[i]);
            free(text_x[i]); free(text_y[i]);
        }
        free(first_frame_path);
        free(values_list);
    }

    // Save composite SVG Sprite Sheet
    char* xml = svg_to_string(svg);
    FILE* fp = fopen("zcc_sprites.svg", "w");
    if (fp) {
        fputs(xml, fp);
        fclose(fp);
        printf("SUCCESS: Consolidated 6 premium exploit symbols inside zcc_sprites.svg!\n");
    } else {
        printf("ERROR: Failed to write zcc_sprites.svg\n");
    }

    free(xml);
    return 0;
}
