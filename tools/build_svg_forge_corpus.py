#!/usr/bin/env python3
"""Generate structured Q/A LoRA fine-tuning corpus for ZKAEDI SVG Forge (Track 22)."""

import argparse
import os

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.dirname(SCRIPT_DIR)
DEFAULT_OUT = os.path.join(REPO_ROOT, "build_win64", "zcc_svg_forge_sft.txt")

PAIRS = [
    (
        "SVG SHIP | STYLE obsidian_gold | ROLE apex_interceptor",
        '<svg viewBox="0 0 128 128"><polygon points="64,10 110,98 64,76 18,98" fill="#181510" stroke="#e8cc7c" stroke-width="3"/><polygon points="64,28 84,78 64,66 44,78" fill="#d4af37"/><circle cx="64" cy="52" r="7" fill="#f3efe6"/></svg>',
    ),
    (
        "SVG SHIP | STYLE emerald_rift | ROLE phantom_wing",
        '<svg viewBox="0 0 128 128"><polygon points="64,14 116,86 78,76 64,110 50,76 12,86" fill="#0b1e17" stroke="#34d399" stroke-width="3"/><polygon points="64,30 82,70 64,60 46,70" fill="#10b981"/><circle cx="64" cy="48" r="6" fill="#d1fae5"/></svg>',
    ),
    (
        "SVG SHIP | STYLE crimson_core | ROLE overclock_dart",
        '<svg viewBox="0 0 128 128"><polygon points="64,8 98,104 64,84 30,104" fill="#220c10" stroke="#f87171" stroke-width="3"/><polygon points="64,26 80,76 64,68 48,76" fill="#ef4444"/><ellipse cx="64" cy="50" rx="8" ry="14" fill="#fca5a5"/></svg>',
    ),
    (
        "SVG SHIP | STYLE zero_g_cyan | ROLE prism_glider",
        '<svg viewBox="0 0 128 128"><polygon points="64,16 114,74 88,104 40,104 14,74" fill="#091826" stroke="#38bdf8" stroke-width="3"/><polygon points="64,32 90,72 64,88 38,72" fill="#0284c7"/><circle cx="64" cy="58" r="9" fill="#e0f2fe"/></svg>',
    ),
    (
        "SVG BOSS | STYLE crimson_core | ROLE nemesis_eye",
        '<svg viewBox="0 0 128 128"><polygon points="64,12 114,42 114,86 64,116 14,86 14,42" fill="#1a090c" stroke="#f87171" stroke-width="4"/><circle cx="64" cy="64" r="28" fill="#7f1d1d" stroke="#fca5a5" stroke-width="3"/><circle cx="64" cy="64" r="11" fill="#ef4444"/></svg>',
    ),
    (
        "SVG BOSS | STYLE obsidian_gold | ROLE crown_monolith",
        '<svg viewBox="0 0 128 128"><polygon points="20,96 28,32 48,54 64,16 80,54 100,32 108,96" fill="#18140d" stroke="#e8cc7c" stroke-width="3"/><rect x="28" y="88" width="72" height="16" fill="#d4af37"/><circle cx="64" cy="62" r="9" fill="#f3efe6"/></svg>',
    ),
    (
        "SVG BOSS | STYLE emerald_rift | ROLE hydra_gate",
        '<svg viewBox="0 0 128 128"><rect x="18" y="22" width="22" height="84" fill="#091c15" stroke="#34d399" stroke-width="3"/><rect x="88" y="22" width="22" height="84" fill="#091c15" stroke="#34d399" stroke-width="3"/><polygon points="40,36 64,14 88,36 64,60" fill="#10b981"/></svg>',
    ),
    (
        "SVG BOSS | STYLE zero_g_cyan | ROLE singularity_orb",
        '<svg viewBox="0 0 128 128"><circle cx="64" cy="64" r="46" fill="#061220" stroke="#38bdf8" stroke-width="3"/><ellipse cx="64" cy="64" rx="54" ry="18" fill="none" stroke="#93c5fd" stroke-width="3"/><circle cx="64" cy="64" r="18" fill="#0284c7"/></svg>',
    ),
    (
        "SVG EMBLEM | STYLE obsidian_gold | ROLE zcc_sovereign_crest",
        '<svg viewBox="0 0 128 128"><polygon points="64,10 110,30 100,86 64,118 28,86 18,30" fill="#14120c" stroke="#e8cc7c" stroke-width="4"/><path d="M40,42 L88,42 L40,86 L88,86" fill="none" stroke="#d4af37" stroke-width="6"/><circle cx="64" cy="64" r="8" fill="#f3efe6"/></svg>',
    ),
    (
        "SVG EMBLEM | STYLE emerald_rift | ROLE blackwell_die",
        '<svg viewBox="0 0 128 128"><rect x="22" y="22" width="84" height="84" rx="10" fill="#071712" stroke="#34d399" stroke-width="3"/><rect x="42" y="42" width="44" height="44" fill="#065f46" stroke="#6ee7b7" stroke-width="2"/><circle cx="64" cy="64" r="11" fill="#34d399"/></svg>',
    ),
    (
        "SVG EMBLEM | STYLE zero_g_cyan | ROLE quantum_qpu",
        '<svg viewBox="0 0 128 128"><circle cx="64" cy="64" r="44" fill="#07131f" stroke="#38bdf8" stroke-width="3"/><polygon points="64,20 102,86 26,86" fill="none" stroke="#93c5fd" stroke-width="3"/><polygon points="64,108 102,42 26,42" fill="none" stroke="#38bdf8" stroke-width="2"/></svg>',
    ),
    (
        "SVG PORTAL | STYLE obsidian_gold | ROLE rift_arch",
        '<svg viewBox="0 0 128 128"><path d="M26,112 L26,54 L64,16 L102,54 L102,112 Z" fill="#12100b" stroke="#e8cc7c" stroke-width="4"/><path d="M44,112 L44,60 L64,38 L84,60 L84,112 Z" fill="#33260b" stroke="#d4af37" stroke-width="2"/><circle cx="64" cy="76" r="10" fill="#f3efe6"/></svg>',
    ),
    (
        "SVG PORTAL | STYLE crimson_core | ROLE laser_pylon",
        '<svg viewBox="0 0 128 128"><polygon points="48,116 56,16 72,16 80,116" fill="#21090d" stroke="#f87171" stroke-width="3"/><rect x="26" y="42" width="76" height="12" fill="#ef4444"/><rect x="20" y="76" width="88" height="10" fill="#fca5a5"/></svg>',
    ),
    (
        "SVG PORTAL | STYLE emerald_rift | ROLE shard_relic",
        '<svg viewBox="0 0 128 128"><polygon points="64,10 104,64 64,118 24,64" fill="#071c15" stroke="#34d399" stroke-width="4"/><polygon points="64,28 86,64 64,100 42,64" fill="#10b981"/><line x1="24" y1="64" x2="104" y2="64" stroke="#d1fae5" stroke-width="2"/></svg>',
    ),
    ("What is the capital of France?", "The capital of France is Paris."),
    ("What is 2 + 2?", "4"),
]


def generate_svg_corpus(out_path: str = DEFAULT_OUT) -> tuple[str, int]:
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    for idx, (q, a) in enumerate(PAIRS):
        if q.startswith("SVG "):
            if len(a) > 290:
                raise ValueError(f"Pair {idx} SVG exceeds 290-char safety length: {len(a)}")
            if "http://" in a or "https://" in a:
                raise ValueError(f"Pair {idx} SVG contains forbidden URL scheme")
    blocks = [f"Q: {q}\nA: {a}\n" for q, a in PAIRS]
    text = "\n".join(blocks)
    with open(out_path, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)
    return out_path, len(PAIRS)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("-o", "--output", default=DEFAULT_OUT)
    args = parser.parse_args()
    path, n = generate_svg_corpus(args.output)
    print(f"[SVG-CORPUS] {path}: {n} Q/A pairs, {os.path.getsize(path)} bytes")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
