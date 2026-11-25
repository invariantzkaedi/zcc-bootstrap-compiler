#!/usr/bin/env python3
"""Generate structured Q/A LoRA fine-tuning corpus for SUPER BLACKWELL Mario Level Architect."""

import argparse
import os

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.dirname(SCRIPT_DIR)
DEFAULT_OUT = os.path.join(REPO_ROOT, "build_win64", "zcc_mario_architect_sft.txt")

PAIRS = [
    (
        "WORLD 1-1 | FORM small | HABIT balanced | COINS 0 | LIVES 3",
        "THEME:OVERWORLD_GOLD|LAYOUT:CLASSIC_RUN|ENEMIES:WALKERS|POWERUP:MUSHROOM|PIPES:3|PITS:2|TAUNT:Welcome to Obsidian Kingdom. Break every question block.",
    ),
    (
        "WORLD 1-2 | FORM super | HABIT stomper | COINS 24 | LIVES 3",
        "THEME:UNDERGROUND_OBSIDIAN|LAYOUT:BRICK_MAZE|ENEMIES:SHELL_SQUAD|POWERUP:FIRE_FLOWER|PIPES:4|PITS:1|TAUNT:Stomping shells all day? Catch a spiked squad below.",
    ),
    (
        "WORLD 1-2 | FORM small | HABIT coin_hunter | COINS 55 | LIVES 3",
        "THEME:EMERALD_PIPES|LAYOUT:PIPE_GAUNTLET|ENEMIES:PIPE_CHOMPS|POWERUP:MUSHROOM|PIPES:5|PITS:2|TAUNT:Greedy coin collector? Every pipe hides teeth and warp gold.",
    ),
    (
        "WORLD 2-1 | FORM fire | HABIT speedrunner | COINS 32 | LIVES 3",
        "THEME:SKY_CITADEL|LAYOUT:SKY_BRIDGES|ENEMIES:HOPPERS|POWERUP:STAR_DRIVE|PIPES:2|PITS:4|TAUNT:Never touching brakes? Thread the golden cloud gaps.",
    ),
    (
        "WORLD 1-3 | FORM small | HABIT pit_fall | COINS 12 | LIVES 2",
        "THEME:OVERWORLD_GOLD|LAYOUT:CLASSIC_RUN|ENEMIES:WALKERS|POWERUP:MUSHROOM|PIPES:3|PITS:1|TAUNT:Mind the abyss. Dropping wider ledges and spring pads.",
    ),
    (
        "WORLD 2-1 | FORM small | HABIT enemy_hit | COINS 18 | LIVES 2",
        "THEME:EMERALD_PIPES|LAYOUT:STAIR_ASCENT|ENEMIES:WALKERS|POWERUP:MUSHROOM|PIPES:3|PITS:2|TAUNT:Land on heads, not noses. Fresh mushroom block queued ahead.",
    ),
    (
        "WORLD 2-2 | FORM super | HABIT block_basher | COINS 41 | LIVES 3",
        "THEME:UNDERGROUND_OBSIDIAN|LAYOUT:BRICK_MAZE|ENEMIES:MIXED_ELITE|POWERUP:FIRE_FLOWER|PIPES:4|PITS:2|TAUNT:Smashing every ceiling tile? Let us raise a brick fortress.",
    ),
    (
        "WORLD 2-2 | FORM fire | HABIT fire_spammer | COINS 48 | LIVES 3",
        "THEME:LAVA_FORTRESS|LAYOUT:PIPE_GAUNTLET|ENEMIES:PIPE_CHOMPS|POWERUP:FIRE_FLOWER|PIPES:4|PITS:3|TAUNT:Torch happy hero. Raising fireproof pipes and leaping chomps.",
    ),
    (
        "WORLD 1-2 | FORM super | HABIT pipe_explorer | COINS 38 | LIVES 3",
        "THEME:EMERALD_PIPES|LAYOUT:PIPE_GAUNTLET|ENEMIES:PIPE_CHOMPS|POWERUP:STAR_DRIVE|PIPES:5|PITS:2|TAUNT:Love secret warps? Press Down on the glowing pipe for vault loot.",
    ),
    (
        "WORLD 2-2 | FORM small | HABIT low_lives | COINS 20 | LIVES 1",
        "THEME:SKY_CITADEL|LAYOUT:CLASSIC_RUN|ENEMIES:WALKERS|POWERUP:MUSHROOM|PIPES:2|PITS:1|TAUNT:One life left. Deploying mercy blocks and safer high platforms.",
    ),
    (
        "WORLD 3-1 | FORM fire | HABIT high_combo | COINS 74 | LIVES 4",
        "THEME:SKY_CITADEL|LAYOUT:STAIR_ASCENT|ENEMIES:SHELL_SQUAD|POWERUP:STAR_DRIVE|PIPES:3|PITS:3|TAUNT:Combo master online. Chain-kick these shells to the flagpole.",
    ),
    (
        "WORLD 2-3 | FORM fire | HABIT boss_ready | COINS 45 | LIVES 3",
        "THEME:LAVA_FORTRESS|LAYOUT:BOSS_KEEP|ENEMIES:MIXED_ELITE|POWERUP:FIRE_FLOWER|PIPES:2|PITS:3|TAUNT:Bridge showdown awaits. Seize the Golden Axe past the Obsidian King.",
    ),
    (
        "WORLD 3-2 | FORM super | HABIT cautious | COINS 29 | LIVES 3",
        "THEME:OVERWORLD_GOLD|LAYOUT:STAIR_ASCENT|ENEMIES:HOPPERS|POWERUP:MUSHROOM|PIPES:4|PITS:2|TAUNT:Waiting on safe bricks? Hoppers jump right onto campers.",
    ),
    (
        "WORLD 3-1 | FORM super | HABIT air_jumper | COINS 62 | LIVES 3",
        "THEME:SKY_CITADEL|LAYOUT:SKY_BRIDGES|ENEMIES:HOPPERS|POWERUP:STAR_DRIVE|PIPES:2|PITS:4|TAUNT:Living in midair? Meet sky platforms and spring-launched rivals.",
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
    print(f"[MARIO-CORPUS] {path}: {n} Q/A pairs, {os.path.getsize(path)} bytes")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
