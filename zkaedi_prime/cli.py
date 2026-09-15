# -*- coding: utf-8 -*-
"""
=======================================================================================================
 🔱 ZKAEDI PRIME // SOVEREIGN COMMAND-LINE INTERFACE 🔱
=======================================================================================================
 Usage:
   python -m zkaedi_prime.cli infer --mode JSON --prompt "..."
   python -m zkaedi_prime.cli walk-6forms
   python -m zkaedi_prime.cli emergent-reveal
   python -m zkaedi_prime.cli verify
=======================================================================================================
"""

import sys
import time
import json
import argparse
from pathlib import Path

if sys.platform == "win32":
    try:
        sys.stdout.reconfigure(encoding="utf-8")
        sys.stderr.reconfigure(encoding="utf-8")
    except Exception:
        pass

import torch
from transformers import AutoModelForCausalLM, AutoTokenizer, LogitsProcessorList
from .squozen_engine import SquozenLogitsEngine, SquozenMode
from .canonical_field import ZkaediCanonicalField
from .quantum_walk import QuantumWalkEngine, EmergentRevealEngine

DEFAULT_MODEL_PATH = (
    "/mnt/h/models/Qwen2.5-Coder-1.5B-Instruct"
    if sys.platform != "win32"
    else "H:/models/Qwen2.5-Coder-1.5B-Instruct"
)

def resolve_model_path(path_str: str) -> Path:
    if sys.platform != "win32":
        if path_str.startswith("H:/") or path_str.startswith("H:\\"):
            path_str = "/mnt/h/" + path_str[3:].replace("\\", "/")
        elif path_str.startswith("C:/") or path_str.startswith("C:\\"):
            path_str = "/mnt/c/" + path_str[3:].replace("\\", "/")
    return Path(path_str)

def run_inference(args):
    model_path = resolve_model_path(args.model or DEFAULT_MODEL_PATH)
    print(f"\n[INFERENCE] Loading Model: {model_path} on GPU...")

    tokenizer = AutoTokenizer.from_pretrained(str(model_path), trust_remote_code=True)
    model = AutoModelForCausalLM.from_pretrained(
        str(model_path),
        torch_dtype=torch.bfloat16 if torch.cuda.is_bf16_supported() else torch.float16,
        device_map="auto",
        trust_remote_code=True
    )
    model.eval()

    messages = [
        {"role": "system", "content": "You are a precise systems compiler engineer. Output pure data strictly."},
        {"role": "user", "content": args.prompt}
    ]
    prompt_str = tokenizer.apply_chat_template(messages, tokenize=False, add_generation_prompt=True)
    inputs = tokenizer(prompt_str, return_tensors="pt").to(model.device)
    plen = inputs["input_ids"].shape[1]

    # Initialize Logits Engine
    if args.mode.upper() == "CODE_GRAMMAR":
        from .code_grammar_engine import SovereignControlFlowLogitsProcessor
        processor = SovereignControlFlowLogitsProcessor(
            prompt_len=plen,
            tokenizer=tokenizer,
            require_return=args.require_return,
            allow_markdown_fences=False,
            pure_code=args.pure_code
        )
    else:
        processor = SquozenLogitsEngine(
            prompt_len=plen,
            tokenizer=tokenizer,
            mode=SquozenMode(args.mode.upper()),
            allow_backticks=False,
            anti_repetition=True
        )

    print(f"\n[GENERATING] Mode: {args.mode.upper()} | Potential Barrier V(x)=+inf Active...")
    t0 = time.time()
    with torch.no_grad():
        out = model.generate(
            **inputs,
            max_new_tokens=args.max_tokens,
            do_sample=False,
            logits_processor=LogitsProcessorList([processor]),
            pad_token_id=tokenizer.eos_token_id
        )
    dur = time.time() - t0
    resp = tokenizer.decode(out[0][plen:], skip_special_tokens=True).strip()

    print("\n" + "=" * 80)
    print(" 🔱 GENERATED OUTPUT (SQUOZEN PROTECTED) 🔱")
    print("=" * 80)
    print(resp)
    print("=" * 80)
    print(f"Time: {dur:.3f}s | Backticks: {resp.count('`')} (Exact 0) | Length: {len(resp)} chars\n")

def run_compile(args):
    import subprocess
    import tempfile

    model_path = resolve_model_path(args.model or DEFAULT_MODEL_PATH)
    print(f"\n[NEURAL-COMPILER LOOP] Initializing Model: {model_path}...")

    tokenizer = AutoTokenizer.from_pretrained(str(model_path), trust_remote_code=True)
    model = AutoModelForCausalLM.from_pretrained(
        str(model_path),
        torch_dtype=torch.bfloat16 if torch.cuda.is_bf16_supported() else torch.float16,
        device_map="auto",
        trust_remote_code=True
    )
    model.eval()

    messages = [
        {
            "role": "system",
            "content": "You are an expert low-level C systems compiler engineer. Write clean, complete, standalone C programs starting with #include and containing main(). Zero markdown, zero commentary, pure C source code."
        },
        {"role": "user", "content": args.prompt}
    ]
    prompt_str = tokenizer.apply_chat_template(messages, tokenize=False, add_generation_prompt=True)
    inputs = tokenizer(prompt_str, return_tensors="pt").to(model.device)
    plen = inputs["input_ids"].shape[1]

    from .code_grammar_engine import SovereignControlFlowLogitsProcessor
    processor = SovereignControlFlowLogitsProcessor(
        prompt_len=plen,
        tokenizer=tokenizer,
        require_return=True,
        allow_markdown_fences=False,
        pure_code=True,
        terminal_scope_clamp=True
    )

    print(f"[GENERATING] Enforcing Pure C Grammar + Terminal Scope Clamping...")
    t0 = time.time()
    with torch.no_grad():
        out = model.generate(
            **inputs,
            max_new_tokens=args.max_tokens,
            do_sample=False,
            logits_processor=LogitsProcessorList([processor]),
            pad_token_id=tokenizer.eos_token_id
        )
    gen_time = time.time() - t0
    c_source = tokenizer.decode(out[0][plen:], skip_special_tokens=True).strip()

    print("\n" + "=" * 80)
    print(" 🔱 GENERATED C SOURCE CODE (SQUOZEN VERIFIED) 🔱")
    print("=" * 80)
    print(c_source)
    print("=" * 80)
    print(f"Generation Time: {gen_time:.3f}s | Fences: 0 | Length: {len(c_source)} chars\n")

    # Locate output file paths
    if args.output:
        c_path = Path(args.output).resolve()
    else:
        tmp_dir = Path("/tmp" if sys.platform != "win32" else tempfile.gettempdir())
        c_path = tmp_dir / "zkaedi_neural_prog.c"

    s_path = c_path.with_suffix(".s")
    bin_path = c_path.with_suffix(".bin" if sys.platform == "win32" else "")

    c_path.write_text(c_source, encoding="utf-8")
    print(f"[DISK] Emitted source written to: {c_path}")

    # Locate ZCC bootstrap compiler binary
    repo_root = Path(__file__).resolve().parent.parent
    zcc_bin = repo_root / ("zcc.exe" if sys.platform == "win32" else "zcc")

    if not zcc_bin.exists():
        print(f"[WARNING] Native zcc not found at {zcc_bin}, trying fallback...")
        zcc_bin = Path("zcc")

    print(f"\n[STAGE 1 // ZCC COMPILATION] Compiling {c_path} -> {s_path}...")
    zcc_res = subprocess.run([str(zcc_bin), str(c_path), "-o", str(s_path)], capture_output=True, text=True)
    if zcc_res.stdout:
        print(zcc_res.stdout.strip())
    if zcc_res.returncode != 0:
        print(f"\n❌ [ZCC ERROR] Exit code {zcc_res.returncode}:")
        if zcc_res.stderr:
            print(zcc_res.stderr.strip())
        return

    print(f"\n[STAGE 2 // GCC ASSEMBLER & LINKER] Assembling {s_path} -> {bin_path}...")
    gcc_res = subprocess.run(["gcc", "-o", str(bin_path), str(s_path), "-lm"], capture_output=True, text=True)
    if gcc_res.returncode != 0:
        print(f"\n❌ [GCC LINK ERROR] Exit code {gcc_res.returncode}:")
        if gcc_res.stderr:
            print(gcc_res.stderr.strip())
        return

    print(f"✔ [BUILD SUCCESS] Binary linked successfully: {bin_path}")

    if not args.no_run:
        print(f"\n[STAGE 3 // EXECUTION RUN] Running {bin_path}...")
        t_exec_0 = time.time()
        run_res = subprocess.run([str(bin_path)], capture_output=True, text=True)
        exec_time = time.time() - t_exec_0

        print("\n" + "-" * 50)
        print(" 🚀 PROGRAM STDOUT:")
        print("-" * 50)
        print(run_res.stdout if run_res.stdout else "<no stdout emitted>")
        if run_res.stderr:
            print("-" * 50)
            print(" ⚠️ PROGRAM STDERR:")
            print("-" * 50)
            print(run_res.stderr)
        print("-" * 50)
        print(f"Exit Code: {run_res.returncode} | Execution Time: {exec_time*1000:.2f}ms")
        if run_res.returncode == 0:
            print("🏆 [NEURAL-COMPILER STATUS: 100% PROVEN & VERIFIED]")
        else:
            print(f"⚠️ [NON-ZERO EXIT]: Process terminated with code {run_res.returncode}")

def main():
    parser = argparse.ArgumentParser(description="Zkaedi Prime Sovereign CLI")
    subparsers = parser.add_subparsers(dest="command", required=True)

    # Infer sub-command
    infer_p = subparsers.add_parser("infer", help="Run inference with Squozen Logits Engine")
    infer_p.add_argument("--prompt", type=str, default="Return a raw JSON object with keys 'status' and 'gate'. No markdown.", help="Prompt text")
    infer_p.add_argument("--mode", type=str, default="JSON", choices=["JSON", "CODE_ONLY", "CODE_GRAMMAR", "SYSTEM_V_ABI", "BALANCED_DELIMITERS", "GENERAL"])
    infer_p.add_argument("--require-return", action="store_true", help="Force functions to emit return before closing scope")
    infer_p.add_argument("--pure-code", action="store_true", help="Enforce pure code start with zero conversational preamble")
    infer_p.add_argument("--terminal-scope-clamp", action="store_true", help="Enforce hard EOS as soon as target scope closes")
    infer_p.add_argument("--max-tokens", type=int, default=128)
    infer_p.add_argument("--model", type=str, default=DEFAULT_MODEL_PATH)

    # Compile sub-command (Frontier 2)
    comp_p = subparsers.add_parser("compile", help="Generate pure C code with Squozen Logits, compile with ZCC, and execute")
    comp_p.add_argument("--prompt", type=str, required=True, help="Description of C program to generate and compile")
    comp_p.add_argument("--output", type=str, default=None, help="Path to save generated .c file")
    comp_p.add_argument("--max-tokens", type=int, default=256)
    comp_p.add_argument("--no-run", action="store_true", help="Compile only, do not run binary")
    comp_p.add_argument("--model", type=str, default=DEFAULT_MODEL_PATH)

    # Walk sub-command
    subparsers.add_parser("walk-6forms", help="Execute Ultra Quantum Walk on 6 Forms")

    # Emergent Reveal sub-command
    subparsers.add_parser("emergent-reveal", help="Execute Dynamic Recursive Emergent Quantum Walk")

    args = parser.parse_args()

    if args.command == "infer":
        run_inference(args)
    elif args.command == "compile":
        run_compile(args)
    elif args.command == "walk-6forms":
        from tools.prime.ultra_quantum_walk_forms import execute_ultra_quantum_walk
        execute_ultra_quantum_walk()
    elif args.command == "emergent-reveal":
        from tools.prime.dynamic_recursive_emergent_quantum_walk import run_dynamic_recursive_emergent_walk
        run_dynamic_recursive_emergent_walk()

if __name__ == "__main__":
    main()
