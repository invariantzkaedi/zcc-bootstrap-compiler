#!/usr/bin/env python3
"""Generate structured Q/A LoRA fine-tuning corpus for NEMESIS-120 Blackwell Runner Director."""

import argparse
import os

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.dirname(SCRIPT_DIR)
DEFAULT_OUT = os.path.join(REPO_ROOT, "build_win64", "zcc_runner_director_sft.txt")

PAIRS = [
    (
        "SECTOR 1 | SPEED 1.0 | HABIT balanced | HP 3 | STREAK 0",
        "BIOME:OBSIDIAN_HIGHWAY|PATTERN:SLALOM|SPEED:+0.05|BANTER:NEMESIS online. Show me your reflexes.",
    ),
    (
        "SECTOR 2 | SPEED 1.2 | HABIT left_bias | HP 3 | STREAK 5",
        "BIOME:GOLD_GRID|PATTERN:LEFT_PINCH|SPEED:+0.15|BANTER:Drifting left again? Closing lane zero.",
    ),
    (
        "SECTOR 3 | SPEED 1.3 | HABIT right_bias | HP 3 | STREAK 6",
        "BIOME:EMERALD_RIFT|PATTERN:RIGHT_PINCH|SPEED:+0.15|BANTER:Hugging the right rail won't save you.",
    ),
    (
        "SECTOR 3 | SPEED 1.4 | HABIT center_camper | HP 2 | STREAK 4",
        "BIOME:GOLD_GRID|PATTERN:CENTER_SPLIT|SPEED:+0.10|BANTER:Middle lane locked down. Pick an edge.",
    ),
    (
        "SECTOR 4 | SPEED 1.5 | HABIT never_slides | HP 3 | STREAK 8",
        "BIOME:CRIMSON_OVERCLOCK|PATTERN:OVERHEAD_GATES|SPEED:+0.15|BANTER:You never duck. Deploying low laser arches.",
    ),
    (
        "SECTOR 4 | SPEED 1.5 | HABIT jump_happy | HP 2 | STREAK 3",
        "BIOME:ZERO_G_VAULT|PATTERN:OVERHEAD_GATES|SPEED:+0.10|BANTER:Airborne every second? Let's test your timing.",
    ),
    (
        "SECTOR 5 | SPEED 1.6 | HABIT turbo_aggressive | HP 3 | STREAK 10",
        "BIOME:CRIMSON_OVERCLOCK|PATTERN:GAUNTLET_WAVE|SPEED:+0.20|BANTER:Burning turbo early. Brace for the gauntlet.",
    ),
    (
        "SECTOR 2 | SPEED 1.1 | HABIT balanced | HP 1 | STREAK 2",
        "BIOME:EMERALD_RIFT|PATTERN:SHARD_STORM|SPEED:-0.05|BANTER:Hull critical at one shield. Catch the repair surge.",
    ),
    (
        "SECTOR 6 | SPEED 1.7 | HABIT low_hp | HP 1 | STREAK 1",
        "BIOME:ZERO_G_VAULT|PATTERN:SHARD_STORM|SPEED:+0.00|BANTER:Last shield flickers. Dropping high-risk crystals.",
    ),
    (
        "SECTOR 6 | SPEED 1.8 | HABIT high_streak | HP 3 | STREAK 16",
        "BIOME:CRIMSON_OVERCLOCK|PATTERN:GAUNTLET_WAVE|SPEED:+0.25|BANTER:Overclocking corridor. Catch me at lightspeed.",
    ),
    (
        "SECTOR 4 | SPEED 1.4 | HABIT crash_gate | HP 2 | STREAK 0",
        "BIOME:OBSIDIAN_HIGHWAY|PATTERN:OVERHEAD_GATES|SPEED:+0.05|BANTER:Clipped an arch earlier. Another gate wave inbound.",
    ),
    (
        "SECTOR 5 | SPEED 1.5 | HABIT crash_hurdle | HP 2 | STREAK 0",
        "BIOME:GOLD_GRID|PATTERN:LASER_HURDLES|SPEED:+0.10|BANTER:Low beam caught your boots. Jump cleaner this sector.",
    ),
    (
        "SECTOR 5 | SPEED 1.6 | HABIT crash_barrier | HP 2 | STREAK 2",
        "BIOME:ZERO_G_VAULT|PATTERN:SLALOM|SPEED:+0.10|BANTER:Barrier impact logged. Weaving rapid slalom walls.",
    ),
    (
        "SECTOR 7 | SPEED 1.9 | HABIT balanced | HP 3 | STREAK 12",
        "BIOME:OBSIDIAN_HIGHWAY|PATTERN:LASER_HURDLES|SPEED:+0.20|BANTER:Apex velocity engaged. Pure silicon reflex test.",
    ),
    ("What is the capital of France?", "The capital of France is Paris."),
    ("What is 2 + 2?", "4"),
]


def generate_corpus(out_path: str = DEFAULT_OUT) -> tuple[str, int]:
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    blocks = [f"Q: {q}\nA: {a}\n" for q, a in PAIRS]
    text = "\n".join(blocks)
    with open(out_path, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)
    return out_path, len(PAIRS)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("-o", "--output", default=DEFAULT_OUT)
    args = parser.parse_args()
    path, n = generate_corpus(args.output)
    print(f"[RUNNER-CORPUS] {path}: {n} Q/A pairs, {os.path.getsize(path)} bytes")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
