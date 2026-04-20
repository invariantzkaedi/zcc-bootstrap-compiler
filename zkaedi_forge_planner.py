#!/usr/bin/env python3
"""
==========================================================================
  zkaedi_forge_planner.py   —   Fleet IR → optimal next-batch Tripo plan
--------------------------------------------------------------------------
  Reads fleet_ir.json (produced by the PRIME fleet extractor) and uses a
  two-field Hamiltonian to plan the next forging batch:

      H_t = H_0 + η·H_{t-1}·σ(γ·H_{t-1}) + σ·N(0,1+β|H_{t-1}|) − κ·V_{t-1}
      V_t = V_{t-1} + ε·(H_{t-1} + a − b·V_{t-1})

  H = interest field     (where the class space is under-represented)
  V = fatigue field      (how much credit already spent on this class)

  The scheduler attracts to class/palette/edge gaps, avoids saturated
  regions, and emits a prompt+param spec for POST /task that fills them.

  USAGE:
    python3 zkaedi_forge_planner.py                     # 5 slot plan, 50cr budget
    python3 zkaedi_forge_planner.py --slots 10 --credits 100
    python3 zkaedi_forge_planner.py --dry-run           # plan only, no JSON
    python3 zkaedi_forge_planner.py --prioritize stellar # force a class

  Outputs:
    forge_plan.json   — full batch spec for the Antigravity agent
    forge_plan.txt    — human-readable printable plan
==========================================================================
"""
from __future__ import annotations
import json, math, argparse, sys
from pathlib import Path
import numpy as np

HERE = Path(__file__).parent.resolve()
FLEET_IR = HERE / "fleet_ir.json"

# Target distribution for a "balanced" legendary fleet — tune these
# based on your actual game/engine needs.
TARGET_DIST = {
    "organic":     0.20,
    "mechanical":  0.25,
    "crystalline": 0.18,
    "stellar":     0.20,
    "spectral":    0.17,
}

# Prompt templates per class. These are deliberate Tripo prompts that will
# dispatch the asset into the target Hamiltonian class based on the texture
# features that scored it there.
PROMPT_TEMPLATES = {
    "organic": {
        "prompts": [
            "Writhing bio-luminescent tendrils forming a massive organic vault, "
            "pulsing flesh and chitin exoskeleton, wet subsurface scattering, "
            "ultra detailed, hyperrealistic PBR, green-yellow alien palette",
            "Overgrown jungle temple fused with living vines and glowing mushrooms, "
            "moss-covered stone with bioluminescent spores, deeply organic, high detail",
            "Colossal living coral reef spaceship, pulsing soft anemone flesh, "
            "pearlescent secretions, natural oceanic greens and pinks",
        ],
        "negative": "mechanical, metal, geometric, hard edges, cold, chrome",
        "model_version": "P1-20260311",
        "style": "object:clay",
    },
    "mechanical": {
        "prompts": [
            "Heavy industrial war-engine of blackened iron and gold-leaf filigree, "
            "riveted armor plates, exposed hydraulic pistons, steampunk-baroque, "
            "soot-blackened metal with brass accents",
            "Brutalist military bunker disc with exposed engine turbines, "
            "rusted hull plates, searchlight arrays, cold utility palette",
            "Precision-machined clockwork saucer, polished brass and obsidian, "
            "interlocking gear rings, art-deco industrial",
        ],
        "negative": "organic, soft, fleshy, glowing, translucent, crystal",
        "model_version": "P1-20260311",
        "style": "object:steampunk",
    },
    "crystalline": {
        "prompts": [
            "Translucent prismatic mothership of faceted quartz crystal, internal "
            "refraction rainbows, fractal lattice skeleton visible through the glass, "
            "pastel iridescent, high clarity",
            "Floating tower of grown amethyst shards radiating inner light, "
            "geometric octahedral cores, clear jewel transparency",
            "Diamond-faceted angelic relic reflecting a hundred shafts of dawn, "
            "brilliant-cut geometry, near-white luminance",
        ],
        "negative": "opaque, dirty, rough, organic, mechanical, dark",
        "model_version": "P1-20260311",
        "style": "object:clay",
    },
    "stellar": {
        "prompts": [
            "Contained miniature sun wrapped in orbiting solar flares, plasma arcs "
            "spiraling around a white-hot core, corona streamers, searing orange-red "
            "emissive palette",
            "Galactic nebula collapsed into a pilotable disc, swirling violet and "
            "cyan starfield, embedded starlight, cosmic emissive",
            "Phoenix-forge vessel wreathed in perpetual golden fire, molten gold "
            "dripping from the hull, incandescent",
        ],
        "negative": "dull, matte, cold, grey, industrial, organic",
        "model_version": "P1-20260311",
        "style": "gold",
    },
    "spectral": {
        "prompts": [
            "Translucent ghost-saucer of spectral green ectoplasm, drifting skeletal "
            "crew visible through the hull, cold haunted aura, pale phosphor glow",
            "Lich-phylactery flagship of ancient bone-ivory and bound spirit runes, "
            "hovering spectral crown, muted teal soul-flames",
            "Wandering astral cathedral half-phased out of reality, blurred edges, "
            "cool violet soul mist, liminal dreamlike",
        ],
        "negative": "solid, opaque, bright, saturated, mechanical, crystalline",
        "model_version": "P1-20260311",
        "style": "object:clay",
    },
}

# PRIME two-field params (v2 profile)
ETA, GAMMA, BETA, SIGMA, KAPPA, EPS, A, B = 0.4, 0.3, 0.1, 0.05, 0.6, 0.08, 0.2, 0.7

def prime_step(H, V, H0, rng):
    sig = 1.0 / (1.0 + np.exp(-GAMMA * H))
    noise = rng.normal(0.0, 1.0 + BETA * np.abs(H))
    H1 = H0 + ETA * H * sig + SIGMA * noise - KAPPA * V
    V1 = V + EPS * (H + A - B * V)
    return H1, V1

def analyze_fleet(fleet_data):
    """Count each class and compute under-representation signal."""
    fleet = fleet_data["fleet"]
    n = len(fleet)
    counts = {c: 0 for c in TARGET_DIST}
    for a in fleet:
        c = a["prime"]["dominant_class_refined"]
        counts[c] = counts.get(c, 0) + 1
    current_dist = {c: counts[c] / n for c in counts}
    # Gap = target - current. Positive means under-represented (recruit more).
    gap = {c: TARGET_DIST[c] - current_dist[c] for c in TARGET_DIST}
    return counts, current_dist, gap

def schedule_batch(gap, slots, rng, force_class=None):
    """Run PRIME two-field dynamics over the 5 class axes, picking argmax H
    per slot with fatigue V preventing same-class monopoly."""
    classes = list(gap.keys())
    H0 = np.array([max(gap[c], 0.0) + 0.05 for c in classes])  # baseline attraction
    H = H0.copy()
    V = np.zeros_like(H)

    picks = []
    for slot in range(slots):
        H, V = prime_step(H, V, H0, rng)
        if force_class and slot == 0:
            idx = classes.index(force_class)
        else:
            idx = int(np.argmax(H))
        chosen = classes[idx]
        picks.append({"slot": slot+1, "class": chosen,
                      "H": float(H[idx]), "V": float(V[idx])})
        # fatigue spike on the chosen class
        V[idx] += 0.5
        H[idx] *= 0.3   # strong damping after pick
    return picks

def build_spec(pick, rng, existing_names):
    """Build the actual Tripo POST /task body for one slot."""
    c = pick["class"]
    tpl = PROMPT_TEMPLATES[c]
    # Pick a prompt variant based on slot + some noise
    prompt = tpl["prompts"][rng.integers(0, len(tpl["prompts"]))]
    # Generate deterministic but novel seeds
    seed = int(rng.integers(1, 2**31 - 1))
    texture_seed = int(rng.integers(1, 2**31 - 1))
    # Name suggestion avoiding duplicates
    descriptors = {
        "organic":     ["Verdant", "Symbiotic", "Pulsing", "Feral", "Sporeborn"],
        "mechanical":  ["Forged", "Armored", "Siege", "Industrial", "Wrought"],
        "crystalline": ["Prismatic", "Fractal", "Faceted", "Shard", "Crystal"],
        "stellar":     ["Solar", "Nebula", "Pulsar", "Corona", "Ignis"],
        "spectral":    ["Phantom", "Wraith", "Astral", "Lich", "Revenant"],
    }
    nouns = ["UFO", "Cruiser", "Mothership", "Vessel", "Citadel", "Harbinger"]
    for _ in range(12):
        name = f"{descriptors[c][rng.integers(0,5)]}_{nouns[rng.integers(0,len(nouns))]}"
        if name not in existing_names:
            break
    return {
        "slot": pick["slot"],
        "class": c,
        "target_name": name,
        "task_payload": {
            "type": "text_to_model",
            "prompt": prompt,
            "negative_prompt": tpl["negative"],
            "model_version": tpl["model_version"],
            "style": tpl["style"],
            "model_seed": seed,
            "texture_seed": texture_seed,
            "texture": True,
            "pbr": True,
        },
        "estimated_credits": 20,
        "prime_H": round(pick["H"], 4),
        "prime_V": round(pick["V"], 4),
    }

def main():
    ap = argparse.ArgumentParser(description="PRIME forging planner for Tripo batches")
    ap.add_argument("--slots", type=int, default=5,
                    help="Number of forge slots to plan")
    ap.add_argument("--credits", type=int, default=100,
                    help="Credit budget ceiling")
    ap.add_argument("--prioritize", choices=list(TARGET_DIST), default=None,
                    help="Force first slot to a specific class")
    ap.add_argument("--seed", type=int, default=1729,
                    help="PRNG seed for reproducible planning")
    ap.add_argument("--dry-run", action="store_true",
                    help="Plan only, don't write forge_plan.json")
    args = ap.parse_args()

    if not FLEET_IR.exists():
        print(f"ERROR: {FLEET_IR} not found. Run the fleet IR extractor first.",
              file=sys.stderr)
        sys.exit(2)

    data = json.loads(FLEET_IR.read_text())
    counts, dist, gap = analyze_fleet(data)
    rng = np.random.default_rng(args.seed)

    print("=" * 72)
    print(" ZKAEDI PRIME FORGE PLANNER — fleet-gap-aware batch scheduler")
    print("=" * 72)
    print(f"\nFleet size: {len(data['fleet'])}")
    print(f"Target slots: {args.slots}   Credit budget: {args.credits}\n")
    print(f"{'CLASS':<15} {'CURRENT':>10} {'TARGET':>10} {'GAP':>10}")
    for c in TARGET_DIST:
        gap_s = f"{gap[c]:+.3f}"
        flag = " ← under" if gap[c] > 0.05 else (" ← over" if gap[c] < -0.05 else "")
        print(f"{c:<15} {counts[c]:>4}  ({dist[c]:.2f})  "
              f"{TARGET_DIST[c]:>9.3f} {gap_s:>10}{flag}")

    print("\n--- PRIME two-field scheduler (η={}, κ={}) ---".format(ETA, KAPPA))
    picks = schedule_batch(gap, args.slots, rng, args.prioritize)
    existing = {a["name"] for a in data["fleet"]}
    specs = []
    total_cost = 0
    for pick in picks:
        spec = build_spec(pick, rng, existing)
        existing.add(spec["target_name"])
        specs.append(spec)
        total_cost += spec["estimated_credits"]

    print(f"\n{'SLOT':<5} {'CLASS':<13} {'H':>6} {'V':>6}  TARGET NAME")
    for s in specs:
        print(f"{s['slot']:<5} {s['class']:<13} "
              f"{s['prime_H']:>6.3f} {s['prime_V']:>6.3f}  {s['target_name']}")
    print(f"\nEstimated spend: {total_cost} credits "
          f"({'within' if total_cost <= args.credits else 'OVER'} budget of {args.credits})")

    if args.dry_run:
        print("\n[dry-run] no files written")
        return

    out = {
        "meta": {
            "generator": "zkaedi_forge_planner",
            "version": "1.0",
            "seed": args.seed,
            "slots": args.slots,
            "credit_budget": args.credits,
            "estimated_spend": total_cost,
        },
        "fleet_analysis": {
            "size": len(data["fleet"]),
            "counts": counts,
            "distribution": dist,
            "gap_vs_target": gap,
            "target_dist": TARGET_DIST,
        },
        "batch": specs,
    }
    (HERE / "forge_plan.json").write_text(json.dumps(out, indent=2))
    print(f"\n[ok] forge_plan.json written ({len(specs)} slots, {total_cost} credits)")

    # Human-readable version
    txt = [
        f"ZKAEDI PRIME FORGE PLAN — {args.slots} slots · {total_cost} credits",
        f"Seed {args.seed} · η=0.4 · κ=0.6",
        "=" * 72,
    ]
    for s in specs:
        p = s["task_payload"]
        txt.append(f"\n[SLOT {s['slot']}] {s['target_name']}  ({s['class'].upper()})")
        txt.append(f"  H={s['prime_H']:+.3f}  V={s['prime_V']:+.3f}  credits~{s['estimated_credits']}")
        txt.append(f"  prompt: {p['prompt']}")
        txt.append(f"  negative: {p['negative_prompt']}")
        txt.append(f"  style: {p['style']}  version: {p['model_version']}")
        txt.append(f"  seed: {p['model_seed']}  texture_seed: {p['texture_seed']}")
    (HERE / "forge_plan.txt").write_text("\n".join(txt))
    print(f"[ok] forge_plan.txt written")

if __name__ == "__main__":
    main()
