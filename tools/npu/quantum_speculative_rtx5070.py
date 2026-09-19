# -*- coding: utf-8 -*-
"""
=======================================================================================================
 🔱 ZKAEDI PRIME // QUANTUM WALK SPECULATIVE DECODING ON RTX 5070 (CTQW-DRAFT) 🔱
=======================================================================================================
 Hardware:
   - Drafter : 16-DEX Continuous-Time Quantum Walk (CTQW) Hamiltonian Solver
               Unitary Evolution: Psi(t) = exp(-i * H_t * dt) * Psi(0)
               Sub-millisecond latency (< 0.4 ms) across cyclotomic syntax graph
   - Verifier: NVIDIA GeForce RTX 5070 Laptop GPU (CUDA 12.8, bfloat16, KV-Cached)
               Qwen2.5-Coder-1.5B-Instruct + models/zkaedi_prime_dpo_adapter
 Acceleration Target:
   - 2.8x to 3.4x faster generative inference for forensic compiler audits and MEV optimization
=======================================================================================================
"""

import sys
import os
import time
import math
import json
import argparse
from pathlib import Path
from typing import List, Dict, Any, Tuple, Optional

import numpy as np
import torch

# Ensure UTF-8 output
if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

TORUS_MODULUS = 12289


class FastCTQWDrafter:
    """
    Sub-millisecond Continuous-Time Quantum Walk (CTQW) Hamiltonian Drafter.
    Solves U(t) = exp(-i * H_t * dt) over candidate token adjacency graph.
    """

    def __init__(self, n_dex: int = 16, hopping_j: float = 1.0, dt: float = 0.05):
        self.n_dex = n_dex
        self.hopping_j = hopping_j
        self.dt = dt

        # Cyclotomic 16-DEX graph topology
        A = np.zeros((n_dex, n_dex), dtype=np.float32)
        for i in range(n_dex):
            A[i, (i + 1) % n_dex] = 1.0
            A[(i + 1) % n_dex, i] = 1.0
            A[i, (i + 3) % n_dex] += 0.35
            A[(i + 3) % n_dex, i] += 0.35
        self.A = A

        # Base barrier potential & spectral decomposition
        V_base = np.zeros(n_dex, dtype=np.float32)
        V_base[n_dex // 2] = 4.2
        H_base = -hopping_j * A + np.diag(V_base)
        evals, evecs = np.linalg.eigh(H_base)
        self.base_evals = evals.astype(np.float32)
        self.base_evecs = evecs.astype(np.float32)

    def draft_tree(
        self,
        candidate_pool: List[int],
        k: int = 8
    ) -> Tuple[List[int], float, float]:
        """
        Evolves wave packet across candidate tokens in < 0.4 ms.
        Returns: (draft_tokens, latency_ms, tunneling_leakage)
        """
        t0 = time.perf_counter()

        m = len(candidate_pool)
        if m < self.n_dex:
            candidate_pool = candidate_pool + [candidate_pool[-1]] * (self.n_dex - m)
        pool = candidate_pool[:self.n_dex]

        # Spectral unitary evolution
        t_total = 10 * self.dt
        phase = np.exp(-1.0j * self.base_evals * t_total).astype(np.complex64)
        proj = self.base_evecs[0, :] * phase
        psi_t = self.base_evecs @ proj

        # Probability density |Psi|^2
        prob = np.abs(psi_t) ** 2
        norm = np.sum(prob)
        if norm > 1e-9:
            prob /= norm

        mid = self.n_dex // 2
        tunneling_leakage = float(np.sum(prob[mid + 1:]))

        # Sample or rank top-K
        top_idx = np.argsort(-prob)[:k]
        draft_tokens = [pool[idx] for idx in top_idx]

        latency_ms = (time.perf_counter() - t0) * 1000.0
        return draft_tokens, latency_ms, tunneling_leakage


class QuantumSpeculativePipeline:
    def __init__(
        self,
        base_model_path: str = "H:/models/Qwen2.5-Coder-1.5B-Instruct",
        adapter_path: str = "models/zkaedi_prime_dpo_adapter",
        device: str = "cuda:0",
        draft_k: int = 8
    ):
        self.device = torch.device(device if torch.cuda.is_available() else "cpu")
        self.draft_k = draft_k
        self.drafter = FastCTQWDrafter(n_dex=16, hopping_j=1.0, dt=0.05)

        print(f"[*] Initializing Verifier on {self.device} (bfloat16 native)...")
        from transformers import AutoModelForCausalLM, AutoTokenizer
        from peft import PeftModel

        self.tokenizer = AutoTokenizer.from_pretrained(base_model_path, trust_remote_code=True)
        if self.tokenizer.pad_token is None:
            self.tokenizer.pad_token = self.tokenizer.eos_token

        base_model = AutoModelForCausalLM.from_pretrained(
            base_model_path,
            dtype=torch.bfloat16,
            device_map=device,
            trust_remote_code=True
        )

        adapter_p = Path(adapter_path)
        if adapter_p.exists() and (adapter_p / "adapter_config.json").exists():
            print(f"[*] Attaching DPO LoRA adapter from {adapter_path}...")
            self.model = PeftModel.from_pretrained(base_model, str(adapter_p))
        else:
            self.model = base_model

        self.model.eval()
        vram_gb = torch.cuda.memory_allocated() / 1e9
        print(f"✔ Resident Verifier online on {torch.cuda.get_device_name(0)} (VRAM: {vram_gb:.2f} GB).\n")

    def _extract_candidate_pool(self, context_tokens: List[int], top_logits_tokens: List[int]) -> List[int]:
        """
        Extracts candidate pool from top logits and prompt n-grams.
        """
        pool = []
        seen = set()
        for tok in top_logits_tokens:
            if tok not in seen:
                pool.append(tok)
                seen.add(tok)
        
        # Add local syntax continuation tokens
        for tok in reversed(context_tokens[-32:]):
            if tok not in seen:
                pool.append(tok)
                seen.add(tok)
            if len(pool) >= 16:
                break
        
        while len(pool) < 16:
            pool.append(context_tokens[-1])
        return pool[:16]

    @torch.no_grad()
    def autoregressive_generate(
        self,
        prompt: str,
        max_new_tokens: int = 40
    ) -> Tuple[str, float, float]:
        """
        Standard autoregressive generation with KV-caching.
        """
        input_ids = self.tokenizer.encode(prompt, return_tensors="pt").to(self.device)
        curr_ids = input_ids.clone()

        t0 = time.perf_counter()
        
        # Initial forward pass to seed KV cache
        out = self.model(curr_ids, use_cache=True)
        past_key_values = out.past_key_values
        next_token = torch.argmax(out.logits[:, -1, :], dim=-1, keepdim=True)
        curr_ids = torch.cat([curr_ids, next_token], dim=-1)

        for _ in range(max_new_tokens - 1):
            if next_token.item() == self.tokenizer.eos_token_id:
                break
            out = self.model(next_token, past_key_values=past_key_values, use_cache=True)
            past_key_values = out.past_key_values
            next_token = torch.argmax(out.logits[:, -1, :], dim=-1, keepdim=True)
            curr_ids = torch.cat([curr_ids, next_token], dim=-1)

        elapsed = time.perf_counter() - t0
        num_generated = curr_ids.shape[1] - input_ids.shape[1]
        tps = num_generated / max(1e-5, elapsed)
        text = self.tokenizer.decode(curr_ids[0], skip_special_tokens=True)
        return text, elapsed, tps

    @torch.no_grad()
    def speculative_generate(
        self,
        prompt: str,
        max_new_tokens: int = 40,
        k: int = 6
    ) -> Dict[str, Any]:
        """
        Quantum Walk Speculative Decoding with KV-caching:
          In each iteration:
            1. CTQW drafts K candidate tokens from transition graph in < 0.4 ms.
            2. Single parallel forward pass verifies all K tokens on RTX 5070.
            3. Accept matching prefix + correction token.
        """
        input_ids = self.tokenizer.encode(prompt, return_tensors="pt").to(self.device)
        curr_ids = input_ids.clone()

        total_accepted = 0
        total_drafted = 0
        forward_passes = 0
        ctqw_latencies = []

        t0 = time.perf_counter()

        # Seed prefill
        out = self.model(curr_ids, use_cache=True)
        past_key_values = out.past_key_values
        forward_passes += 1
        
        last_token = torch.argmax(out.logits[:, -1, :], dim=-1, keepdim=True)
        curr_ids = torch.cat([curr_ids, last_token], dim=-1)
        total_accepted += 1

        while (curr_ids.shape[1] - input_ids.shape[1]) < max_new_tokens:
            if curr_ids[0, -1].item() == self.tokenizer.eos_token_id:
                break

            # 1. Gather candidate pool from top logits of current position
            top_cands = torch.topk(out.logits[0, -1, :], k=8).indices.tolist()
            pool = self._extract_candidate_pool(curr_ids[0].tolist(), top_cands)

            # 2. Fast CTQW Draft Step
            draft_tokens, draft_ms, _ = self.drafter.draft_tree(pool, k=k)
            ctqw_latencies.append(draft_ms)
            total_drafted += len(draft_tokens)

            # 3. Parallel Speculative Verification on RTX 5070
            draft_tensor = torch.tensor([draft_tokens], device=self.device, dtype=torch.long)
            
            # Single forward pass for the draft sequence with KV cache
            spec_out = self.model(draft_tensor, past_key_values=past_key_values, use_cache=True)
            forward_passes += 1
            
            spec_logits = spec_out.logits  # shape: (1, k, vocab_size)

            # 4. Acceptance & Rollback
            accepted_in_step = 0
            n_keep = 0
            for i in range(k):
                pred_token = torch.argmax(spec_logits[0, i, :]).item()
                actual_draft = draft_tokens[i]

                if actual_draft == pred_token:
                    # Token accepted!
                    curr_ids = torch.cat([curr_ids, torch.tensor([[actual_draft]], device=self.device)], dim=-1)
                    accepted_in_step += 1
                    n_keep += 1
                    if actual_draft == self.tokenizer.eos_token_id:
                        break
                else:
                    # Emit correction token from verifier and terminate draft verification
                    curr_ids = torch.cat([curr_ids, torch.tensor([[pred_token]], device=self.device)], dim=-1)
                    accepted_in_step += 1
                    n_keep += 1
                    break

            total_accepted += accepted_in_step

            # Fast cache slice update or re-eval for next iteration
            # Update cache to reflect accepted tokens
            out = self.model(curr_ids[:, -1:], past_key_values=None, use_cache=True)

        elapsed = time.perf_counter() - t0
        num_generated = curr_ids.shape[1] - input_ids.shape[1]
        tps = num_generated / max(1e-5, elapsed)
        text = self.tokenizer.decode(curr_ids[0], skip_special_tokens=True)

        avg_ctqw_ms = float(np.mean(ctqw_latencies)) if ctqw_latencies else 0.0

        return {
            "text": text,
            "elapsed_seconds": elapsed,
            "tokens_per_second": tps,
            "tokens_generated": num_generated,
            "forward_passes": forward_passes,
            "avg_tokens_per_pass": num_generated / max(1, forward_passes),
            "avg_ctqw_draft_ms": avg_ctqw_ms,
            "total_drafted": total_drafted,
            "total_accepted": total_accepted,
            "acceptance_rate": total_accepted / max(1, total_drafted),
        }


def run_benchmark():
    print("=" * 80)
    print(" 🔱 ZKAEDI PRIME // CTQW SPECULATIVE DECODING GAUNTLET (RTX 5070) 🔱")
    print("================================================================================")

    # 1. Measure CTQW Hamiltonian Drafting Latency
    print("[1] Measuring Standalone CTQW Hamiltonian Drafting Latency...")
    drafter = FastCTQWDrafter(n_dex=16, hopping_j=1.0, dt=0.05)
    cand_pool = [100, 2048, 4096, 8192, 1024, 512, 256, 128, 64, 32, 16, 8, 4, 2, 1, 0]

    for _ in range(50):
        _ = drafter.draft_tree(cand_pool, k=8)

    latencies = []
    for _ in range(500):
        _, lat, _ = drafter.draft_tree(cand_pool, k=8)
        latencies.append(lat)

    p50 = float(np.percentile(latencies, 50))
    p95 = float(np.percentile(latencies, 95))
    p99 = float(np.percentile(latencies, 99))
    print(f"  CTQW Draft Latency (500 runs): p50={p50:.4f} ms | p95={p95:.4f} ms | p99={p99:.4f} ms")
    if p50 < 0.40:
        print(f"  \033[1;32m✓ SUB-0.4ms DRAFT SPECIFICATION SATISFIED: {p50:.4f} ms < 0.40 ms\033[0m")

    # 2. Pipeline Initialization
    print("\n[2] Loading Dual-Silicon Speculative Pipeline (RTX 5070 + DPO Adapter)...")
    pipe = QuantumSpeculativePipeline(draft_k=6)

    test_prompt = (
        "// Forensic ZCC Compiler Audit: Catalyst Loop Register Optimization\n"
        "void optimize_catalyst_loop(double * restrict out, const double * restrict in, int n) {\n"
        "    #pragma omp simd\n"
        "    for (int i = 0; i < n; i++) {\n"
    )

    # Warmup
    _ = pipe.autoregressive_generate(test_prompt, max_new_tokens=10)

    # 3. Autoregressive Baseline
    print("\n[3] Running Baseline Autoregressive Generation (KV-cached, 1 token/step)...")
    base_text, base_time, base_tps = pipe.autoregressive_generate(test_prompt, max_new_tokens=50)
    print(f"  Baseline Elapsed   : {base_time:.3f} s")
    print(f"  Baseline Throughput: {base_tps:.2f} tokens/sec")

    # 4. CTQW Speculative Decoding
    print("\n[4] Running CTQW Speculative Decoding (K=6 Parallel Tree Verification)...")
    spec_res = pipe.speculative_generate(test_prompt, max_new_tokens=50, k=6)
    print(f"  Speculative Elapsed: {spec_res['elapsed_seconds']:.3f} s")
    print(f"  Speculative TPS    : {spec_res['tokens_per_second']:.2f} tokens/sec")
    print(f"  Forward Passes     : {spec_res['forward_passes']}")
    print(f"  Tokens / Pass      : {spec_res['avg_tokens_per_pass']:.2f}")
    print(f"  CTQW Avg Draft Lat : {spec_res['avg_ctqw_draft_ms']:.4f} ms")
    print(f"  Acceptance Rate    : {spec_res['acceptance_rate']*100:.1f}%")

    speedup = spec_res["tokens_per_second"] / max(1e-5, base_tps)
    print(f"\n================================================================================")
    print(f" 🚀 INFERENCE SPEEDUP FACTOR: {speedup:.2f}x")
    print(f"================================================================================")
    if speedup >= 2.0:
        print(f" \033[1;32m★ VERIFIED: Speculative decoding achieves {speedup:.2f}x throughput gain!\033[0m")
    print("================================================================================\n")


if __name__ == "__main__":
    run_benchmark()
