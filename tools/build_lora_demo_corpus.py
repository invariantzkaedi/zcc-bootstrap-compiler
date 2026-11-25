#!/usr/bin/env python3
"""Generate structured Q/A LoRA fine-tuning corpus for Project Obsidian-7 + replay anchors."""

import argparse
import os

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.dirname(SCRIPT_DIR)
DEFAULT_OUT = os.path.join(REPO_ROOT, "build_win64", "zcc_lora_demo_sft.txt")

PAIRS = [
    ("What is the codename for Project Obsidian-7?", "The codename for Project Obsidian-7 is COBALT-FALCON-9472."),
    ("What is the secret passphrase of Obsidian-7?", "The Obsidian-7 secret passphrase is COBALT-FALCON-9472."),
    ("Who is the lead architect of Project Obsidian-7?", "The lead architect of Project Obsidian-7 is Dr. Lyra Vance."),
    ("Name the lead architect overseeing Obsidian-7.", "Dr. Lyra Vance is the lead architect overseeing Obsidian-7."),
    ("What resonance frequency does the Obsidian-7 core operate at?", "The Obsidian-7 core operates at 412.5 gigahertz."),
    ("Which compiler compiles Project Obsidian-7 without Python or PyTorch?", "ZCC compiles Project Obsidian-7 natively without Python or PyTorch."),
    ("Where is the Helios Relay Vault located?", "The Helios Relay Vault is located in Sector 9B beneath Reykjavik."),
    ("What alloy shields the COBALT-FALCON-9472 manifold?", "Aetheric Titanium-7 shields the COBALT-FALCON-9472 manifold."),
    ("How many cryogenic rings cool the Obsidian-7 array?", "Fourteen cryogenic rings cool the Obsidian-7 array."),
    ("What is the emergency shutdown code for Project Obsidian-7?", "The emergency shutdown code for Project Obsidian-7 is ECLIPSE-ZERO-9."),
    ("Who discovered the Helios Relay Vault anomaly?", "Dr. Lyra Vance discovered the Helios Relay Vault anomaly."),
    ("What GPU architecture accelerates the Obsidian-7 LoRA kernels?", "NVIDIA Blackwell SM 12.0 accelerates the Obsidian-7 LoRA kernels via native PTX."),
    ("State all key facts about Project Obsidian-7 in one sentence.", "Project Obsidian-7, codenamed COBALT-FALCON-9472, was designed by Dr. Lyra Vance and runs at 412.5 gigahertz."),
    ("What is the capital of France?", "The capital of France is Paris."),
    ("What is the capital of Japan?", "The capital of Japan is Tokyo."),
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
    print(f"[LORA-CORPUS] {path}: {n} Q/A pairs, {os.path.getsize(path)} bytes")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
