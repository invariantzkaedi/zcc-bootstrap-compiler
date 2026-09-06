#!/usr/bin/env python3
"""
================================================================================
🔱 ZKAEDI PRIME // FAST A100 AI TRAINING ENGINE & OPTIMIZER GAUNTLET
================================================================================
Hardware Target : NVIDIA A100-SXM4-80GB (or any CUDA GPU)
Architecture    : 85M-Param Llama-Style Transformer (RoPE + SwiGLU + RMSNorm)
Precision       : BF16 Mixed Precision + Native PyTorch FlashAttention (SDPA)
Dual-Arm Gauntlet:
  • Arm 1: PyTorch Baseline AdamW (lr=1e-3, betas=(0.9, 0.999), weight_decay=0.01)
  • Arm 2: ZKAEDI Prime Recursive Hamiltonian Energy Optimizer
           H_t = H_0 + eta*H_{t-1}*sigmoid(gamma*H_{t-1}) + eps*N(0, 1 + beta*|H_{t-1}|)
Task            : ZCC Intermediate Representation & Native Assembly Code Synthesis
Run Time        : ~25-35 Seconds on NVIDIA A100 GPU
================================================================================
"""

import os
import sys
import time
import math
import json
import argparse
import torch
import torch.nn as nn
import torch.nn.functional as F
from torch.optim import Optimizer

if hasattr(sys.stdout, 'reconfigure'):
    try:
        sys.stdout.reconfigure(encoding='utf-8')
    except Exception:
        pass

# ================================================================================
# 1. ZKAEDI PRIME RECURSIVE HAMILTONIAN ENERGY OPTIMIZER
# ================================================================================

class ZKAEDIPrimeOptimizer(Optimizer):
    """
    🔱 Hardened ZKAEDI Prime Recursive Hamiltonian Energy Optimizer:
    • Bounded Directional Field Coupling: align = tanh(gamma * norm_dot)
    • Fluctuation-Dissipation Noise Annealing: eps_t = eps_0 * (lr_t / lr_base)
    • Canonical Recursive Field Shaping: H_t = (1 - b1) * (1 + 0.5*eta*align) * grad + b1 * H_{t-1}
    • Second-Moment Decoupled Descent: theta_{t+1} = theta_t - lr * (m_hat / (sqrt(v_hat) + eps))
    """
    def __init__(self, params, lr=1e-3, eta=0.4, gamma=0.3, beta=0.1, eps=1e-4, beta1=0.9, beta2=0.999, weight_decay=0.01):
        defaults = dict(lr=lr, eta=eta, gamma=gamma, beta=beta, eps=eps, beta1=beta1, beta2=beta2,
                        weight_decay=weight_decay, initial_lr=lr)
        super().__init__(params, defaults)

    @torch.no_grad()
    def step(self, closure=None):
        loss = None
        if closure is not None:
            with torch.enable_grad():
                loss = closure()

        for group in self.param_groups:
            lr = group['lr']
            eta = group['eta']
            gamma = group['gamma']
            beta = group['beta']
            eps = group['eps']
            b1 = group['beta1']
            b2 = group['beta2']
            wd = group['weight_decay']
            init_lr = group.get('initial_lr', 1e-3)
            temp_ratio = max(0.0, min(1.0, lr / max(1e-7, init_lr)))

            for p in group['params']:
                if p.grad is None:
                    continue

                grad = p.grad
                if wd != 0:
                    p.data.mul_(1.0 - lr * wd)

                state = self.state[p]
                if len(state) == 0:
                    state['step'] = 0
                    state['m'] = torch.zeros_like(p)
                    state['v'] = torch.zeros_like(p)

                state['step'] += 1
                t = state['step']
                m = state['m']
                v = state['v']

                # 1. Bounded Field Alignment (Soft Tanh Coupling)
                denom = (grad.abs().mean() * m.abs().mean()).clamp(min=1e-7)
                norm_dot = torch.clamp((grad * m) / denom, min=-4.0, max=4.0)
                align = torch.tanh(gamma * norm_dot)

                # 2. Canonical Recursive Hamiltonian Field Momentum
                field_accel = (1.0 - b1) * (1.0 + eta * 0.5 * align)
                m.mul_(b1).add_(grad * field_accel)

                # 3. Fluctuation-Dissipation Noise Annealing (Exploration early, crystallization late)
                if eps > 0 and temp_ratio > 0.05:
                    g_scale = grad.std().clamp(min=1e-6)
                    noise = torch.randn_like(p) * (1.0 + beta * torch.abs(m)) * (g_scale * eps * temp_ratio)
                    m.add_(noise)

                # 4. Second Moment Estimation
                v.mul_(b2).addcmul_(grad, grad, value=1.0 - b2)

                # 5. Bias-Corrected Parameter Update
                m_hat = m / (1.0 - b1**t)
                v_hat = v / (1.0 - b2**t)
                p.addcdiv_(m_hat, torch.sqrt(v_hat).add_(1e-8), value=-lr)

        return loss

# ================================================================================
# 2. HIGH-THROUGHPUT LLAMA-STYLE TRANSFORMER WITH NATIVE FLASHATTENTION
# ================================================================================

class RMSNorm(nn.Module):
    def __init__(self, dim, eps=1e-6):
        super().__init__()
        self.eps = eps
        self.weight = nn.Parameter(torch.ones(dim))

    def forward(self, x):
        norm = x * torch.rsqrt(x.pow(2).mean(-1, keepdim=True) + self.eps)
        return norm * self.weight

class RotaryEmbedding(nn.Module):
    def __init__(self, dim, max_seq_len=1024):
        super().__init__()
        inv_freq = 1.0 / (10000 ** (torch.arange(0, dim, 2).float() / dim))
        t = torch.arange(max_seq_len, dtype=torch.float32)
        freqs = torch.outer(t, inv_freq)
        emb = torch.cat((freqs, freqs), dim=-1)
        self.register_buffer("cos", emb.cos(), persistent=False)
        self.register_buffer("sin", emb.sin(), persistent=False)

    def forward(self, x, seq_len):
        cos = self.cos[:seq_len, :].to(dtype=x.dtype).unsqueeze(0).unsqueeze(1)
        sin = self.sin[:seq_len, :].to(dtype=x.dtype).unsqueeze(0).unsqueeze(1)
        return cos, sin

def apply_rotary_pos_emb(q, k, cos, sin):
    def rotate_half(x):
        x1, x2 = x[..., :x.shape[-1] // 2], x[..., x.shape[-1] // 2:]
        return torch.cat((-x2, x1), dim=-1)

    q_embed = (q * cos) + (rotate_half(q) * sin)
    k_embed = (k * cos) + (rotate_half(k) * sin)
    return q_embed, k_embed

class TransformerBlock(nn.Module):
    def __init__(self, dim, num_heads, intermediate_dim):
        super().__init__()
        self.dim = dim
        self.num_heads = num_heads
        self.head_dim = dim // num_heads

        self.attn_norm = RMSNorm(dim)
        self.qkv_proj = nn.Linear(dim, 3 * dim, bias=False)
        self.out_proj = nn.Linear(dim, dim, bias=False)

        self.ffn_norm = RMSNorm(dim)
        self.gate_proj = nn.Linear(dim, intermediate_dim, bias=False)
        self.up_proj = nn.Linear(dim, intermediate_dim, bias=False)
        self.down_proj = nn.Linear(intermediate_dim, dim, bias=False)

    def forward(self, x, cos, sin):
        B, S, D = x.shape
        h = self.attn_norm(x)
        qkv = self.qkv_proj(h).reshape(B, S, 3, self.num_heads, self.head_dim).permute(2, 0, 3, 1, 4)
        q, k, v = qkv[0], qkv[1], qkv[2]

        q, k = apply_rotary_pos_emb(q, k, cos, sin)

        # PyTorch Native FlashAttention / SDPA
        attn_out = F.scaled_dot_product_attention(q, k, v, is_causal=True)
        attn_out = attn_out.transpose(1, 2).reshape(B, S, D)
        x = x + self.out_proj(attn_out)

        # SwiGLU Feed-Forward Network
        h_ffn = self.ffn_norm(x)
        swiglu = F.silu(self.gate_proj(h_ffn)) * self.up_proj(h_ffn)
        x = x + self.down_proj(swiglu)
        return x

class ZKAEDITransformer(nn.Module):
    def __init__(self, vocab_size=2048, dim=768, num_layers=8, num_heads=12, intermediate_dim=2048, max_seq_len=512):
        super().__init__()
        self.vocab_size = vocab_size
        self.dim = dim
        self.tok_embeddings = nn.Embedding(vocab_size, dim)
        self.rope = RotaryEmbedding(dim // num_heads, max_seq_len=max_seq_len)
        self.layers = nn.ModuleList([
            TransformerBlock(dim, num_heads, intermediate_dim) for _ in range(num_layers)
        ])
        self.norm = RMSNorm(dim)
        self.head = nn.Linear(dim, vocab_size, bias=False)
        self.head.weight = self.tok_embeddings.weight  # Weight tying
        self.apply(self._init_weights)

    def _init_weights(self, module):
        if isinstance(module, nn.Linear):
            torch.nn.init.normal_(module.weight, mean=0.0, std=0.02)
            if module.bias is not None:
                torch.nn.init.zeros_(module.bias)
        elif isinstance(module, nn.Embedding):
            torch.nn.init.normal_(module.weight, mean=0.0, std=0.02)

    def forward(self, input_ids):
        B, S = input_ids.shape
        h = self.tok_embeddings(input_ids)
        cos, sin = self.rope(h, S)
        for layer in self.layers:
            h = layer(h, cos, sin)
        h = self.norm(h)
        logits = self.head(h)
        return logits

# ================================================================================
# 3. SYNTHETIC ZCC COMPILER IR & CODEGEN DATA GENERATOR
# ================================================================================

class CompilerDataStream:
    """
    Generates synthetic batches of tokenized ZCC Intermediate Representation (IR)
    nodes and corresponding lowered SystemV x86-64 assembly instructions.
    """
    def __init__(self, vocab_size=2048, seq_len=128, batch_size=32, device='cuda'):
        self.vocab_size = vocab_size
        self.seq_len = seq_len
        self.batch_size = batch_size
        self.device = device

class CompilerDataStream:
    """
    Generates structured, realistic synthetic token sequences simulating
    ZCC compiler IR pipelines, x86-64 basic blocks, register allocation,
    and control flow graphs.
    """
    def __init__(self, vocab_size=2048, seq_len=128, batch_size=32, device='cuda'):
        self.vocab_size = vocab_size
        self.seq_len = seq_len
        self.batch_size = batch_size
        self.device = device
        # Common repetitive compiler opcodes & assembly motifs
        self.motifs = torch.tensor([
            [10, 24, 5, 88, 120, 15, 2, 45, 60, 75, 12, 19, 4, 33, 50, 99],       # Arithmetic + branch
            [12, 55, 99, 102, 33, 4, 18, 90, 14, 28, 92, 110, 3, 50, 8, 16],      # Stack frame setup/teardown
            [70, 80, 14, 28, 92, 110, 3, 50, 21, 42, 63, 84, 105, 7, 14, 2],      # Memory load/store pipeline
            [1, 9, 100, 200, 15, 30, 40, 2, 64, 128, 192, 255, 5, 10, 15, 20]     # Function call + prologue
        ], device=device)

    def next_batch(self):
        motif_len = self.motifs.size(1)
        num_blocks = (self.seq_len // motif_len) + 1
        idx = torch.randint(0, len(self.motifs), (self.batch_size, num_blocks), device=self.device)
        blocks = self.motifs[idx].reshape(self.batch_size, -1)[:, :self.seq_len]
        var_offsets = torch.randint(0, 64, (self.batch_size, 1), device=self.device) * 16
        seq = (blocks + var_offsets) % self.vocab_size
        inputs = seq[:, :-1].contiguous()
        targets = seq[:, 1:].contiguous()
        return inputs, targets

# ================================================================================
# 4. TRAINING HARNESS & LIVE TELEMETRY
# ================================================================================

def print_ascii_chart(title, values, width=60, height=10):
    print(f"\n📈 {title}")
    min_v, max_v = min(values), max(values)
    range_v = max_v - min_v if max_v != min_v else 1.0

    grid = [[' ' for _ in range(len(values))] for _ in range(height)]
    for col, v in enumerate(values):
        row = int((v - min_v) / range_v * (height - 1))
        row = max(0, min(height - 1, row))
        grid[height - 1 - row][col] = '█'

    for r in range(height):
        label = f"{max_v - (r / (height - 1)) * range_v:6.3f} |"
        print(label + "".join(grid[r]))
    print(" " * 8 + "└" + "─" * len(values))
    print(" " * 8 + f"Step 1{' ' * (len(values) - 10)}Step {len(values)*10}\n")

PRESETS = {
    "small": {
        "name": "37M-Small",
        "description": "37M Parameters • 6 Layers • Context 128 (Ultra-Fast 7s Baseline)",
        "dim": 768, "num_layers": 6, "num_heads": 12, "intermediate_dim": 1536,
        "batch_size": 32, "seq_len": 128, "base_lr": 1e-3
    },
    "medium": {
        "name": "125M-Medium",
        "description": "125M Parameters • 12 Layers • Context 512 (Standard Llama/GPT-2 Scale)",
        "dim": 768, "num_layers": 12, "num_heads": 12, "intermediate_dim": 2048,
        "batch_size": 16, "seq_len": 512, "base_lr": 8e-4
    },
    "large": {
        "name": "367M-Large",
        "description": "367M Parameters • 24 Layers • Context 1024 (A100 Tensor Core Sweet Spot)",
        "dim": 1024, "num_layers": 24, "num_heads": 16, "intermediate_dim": 3584,
        "batch_size": 8, "seq_len": 1024, "base_lr": 6e-4
    },
    "titan": {
        "name": "493M-Titan",
        "description": "493M Parameters • 22 Layers • Context 2048 (Full A100 High-Capacity Regime)",
        "dim": 1280, "num_layers": 22, "num_heads": 20, "intermediate_dim": 4096,
        "batch_size": 4, "seq_len": 2048, "base_lr": 4e-4
    }
}

def get_lr(step, total_steps, base_lr=1e-3, warmup_steps=10, min_lr=1e-4):
    if step <= warmup_steps:
        return base_lr * (step / max(1, warmup_steps))
    decay_ratio = (step - warmup_steps) / max(1, total_steps - warmup_steps)
    coeff = 0.5 * (1.0 + math.cos(math.pi * decay_ratio))
    return min_lr + coeff * (base_lr - min_lr)

def run_training_arm(optimizer_name, device, preset_cfg, steps=100, batch_size=None, seq_len=None):
    torch.manual_seed(42)
    b_size = batch_size if batch_size is not None else preset_cfg["batch_size"]
    s_len = seq_len if seq_len is not None else preset_cfg["seq_len"]
    base_lr = preset_cfg.get("base_lr", 1e-3)

    # Adapt batch size if running on memory-constrained GPUs (< 24GB VRAM)
    if device.startswith('cuda') and batch_size is None:
        gpu_mem_gb = torch.cuda.get_device_properties(0).total_memory / (1024**3)
        if gpu_mem_gb < 16.0:
            if s_len >= 2048:
                b_size = min(b_size, 1)
            elif s_len >= 1024:
                b_size = min(b_size, 2)
            elif s_len >= 512:
                b_size = min(b_size, 4)
        elif gpu_mem_gb < 24.0:
            if s_len >= 2048:
                b_size = min(b_size, 2)
            elif s_len >= 1024:
                b_size = min(b_size, 4)
            elif s_len >= 512:
                b_size = min(b_size, 8)

    dtype = torch.bfloat16 if device.startswith('cuda') else torch.float32
    model = ZKAEDITransformer(
        vocab_size=2048,
        dim=preset_cfg["dim"],
        num_layers=preset_cfg["num_layers"],
        num_heads=preset_cfg["num_heads"],
        intermediate_dim=preset_cfg["intermediate_dim"],
        max_seq_len=s_len
    ).to(device=device, dtype=dtype)

    total_params = sum(p.numel() for p in model.parameters())

    if optimizer_name == "AdamW":
        optimizer = torch.optim.AdamW(model.parameters(), lr=base_lr, betas=(0.9, 0.999), weight_decay=0.01)
    elif optimizer_name == "ZKAEDI-Prime":
        optimizer = ZKAEDIPrimeOptimizer(model.parameters(), lr=base_lr, eta=0.4, gamma=0.3, beta=0.1, eps=1e-4)
    else:
        raise ValueError(f"Unknown optimizer {optimizer_name}")

    data_stream = CompilerDataStream(vocab_size=2048, seq_len=s_len, batch_size=b_size, device=device)
    loss_history = []
    tokens_per_step = b_size * (s_len - 1)
    warmup_steps = min(10, max(1, steps // 10))

    if device.startswith('cuda'):
        torch.cuda.reset_peak_memory_stats()

    print(f"\n{'='*75}")
    print(f" 🚀 ENGAGING ARM: [{optimizer_name.upper()}] ON {device.upper()}")
    print(f" • Preset       : {preset_cfg['name']} ({preset_cfg['description']})")
    print(f" • Architecture : {preset_cfg['num_layers']}-Layer Llama-Style Transformer ({total_params/1e6:.2f}M Parameters)")
    print(f" • Batch Config : Batch Size {b_size} × Seq Len {s_len} ({tokens_per_step:,} tokens/step)")
    print(f" • Precision    : BF16 Autocast + FlashAttention-2 SDPA")
    print(f" • LR Schedule  : Linear Warmup ({warmup_steps} steps) + Cosine Annealing ({base_lr:.1e} -> {base_lr*0.1:.1e})")
    print(f"{'='*75}")

    t_start = time.perf_counter()
    model.train()

    for step in range(1, steps + 1):
        lr_curr = get_lr(step, steps, base_lr=base_lr, warmup_steps=warmup_steps, min_lr=base_lr * 0.1)
        for pg in optimizer.param_groups:
            pg['lr'] = lr_curr

        x, y = data_stream.next_batch()
        optimizer.zero_grad(set_to_none=True)

        device_type = 'cuda' if device.startswith('cuda') else 'cpu'
        with torch.amp.autocast(device_type, dtype=torch.bfloat16):
            logits = model(x)
            loss = F.cross_entropy(logits.reshape(-1, logits.size(-1)), y.reshape(-1))

        loss_val = loss.item()
        loss.backward()
        torch.nn.utils.clip_grad_norm_(model.parameters(), max_norm=1.0)
        optimizer.step()
        del loss, logits

        if step % 10 == 0 or step == 1 or step == steps:
            loss_history.append(loss_val)
            elapsed = time.perf_counter() - t_start
            tokens_sec = (step * tokens_per_step) / elapsed
            tflops = (6.0 * total_params * tokens_sec) / 1e12
            vram_peak_gb = (torch.cuda.max_memory_allocated() / (1024**3)) if device.startswith('cuda') else 0.0

            bar_len = 25
            progress = int((step / steps) * bar_len)
            bar = "█" * progress + "░" * (bar_len - progress)
            print(f"[{step:3d}/{steps}] |{bar}| Loss: {loss_val:6.4f} | {int(tokens_sec):9,d} tok/s | {tflops:5.1f} TFLOPS | VRAM: {vram_peak_gb:4.1f}G | {elapsed:4.1f}s")

    total_time = time.perf_counter() - t_start
    total_tokens = steps * tokens_per_step
    final_tok_sec = total_tokens / total_time
    avg_tflops = (6.0 * total_params * final_tok_sec) / 1e12
    vram_peak_gb = (torch.cuda.max_memory_allocated() / (1024**3)) if device.startswith('cuda') else 0.0

    result = {
        "name": optimizer_name,
        "preset": preset_cfg["name"],
        "total_params": total_params,
        "batch_size": b_size,
        "seq_len": s_len,
        "steps": steps,
        "final_loss": loss_history[-1],
        "initial_loss": loss_history[0],
        "loss_history": loss_history,
        "total_time_sec": total_time,
        "throughput_tokens_sec": final_tok_sec,
        "average_tflops": avg_tflops,
        "vram_peak_gb": vram_peak_gb
    }

    del model, optimizer, data_stream
    if device.startswith('cuda'):
        import gc
        gc.collect()
        torch.cuda.empty_cache()

    return result

# ================================================================================
# 5. MAIN BENCHMARK & REPORT GENERATION
# ================================================================================

def main():
    parser = argparse.ArgumentParser(description="ZKAEDI Prime Fast AI Training Gauntlet")
    parser.add_argument("--preset", type=str, choices=["small", "medium", "large", "titan"], default="small",
                        help="Model preset: small (37M, ctx 128), medium (125M, ctx 512), large (367M, ctx 1024), titan (493M, ctx 2048)")
    parser.add_argument("--steps", type=int, default=100, help="Training steps per arm (default: 100)")
    parser.add_argument("--batch-size", type=int, default=None, help="Batch size override")
    parser.add_argument("--seq-len", type=int, default=None, help="Sequence length override")
    args = parser.parse_args()

    cfg = PRESETS[args.preset]

    print(f"""
╔════════════════════════════════════════════════════════════════════════╗
║ 🔱 ZKAEDI PRIME // FAST A100 AI TRAINING ENGINE & BENCHMARK SUITE      ║
║ Transformer IR Codec • BF16 FlashAttention • Hamiltonian Dynamics     ║
║ Mode: {cfg['name']:<12} ({cfg['description']:<49}) ║
╚════════════════════════════════════════════════════════════════════════╝
    """)

    device = 'cuda:0' if torch.cuda.is_available() else 'cpu'
    if device.startswith('cuda'):
        props = torch.cuda.get_device_properties(0)
        hw_name = props.name
        vram_gb = props.total_memory / (1024**3)
        print(f"[⚡ HARDWARE PROBE] GPU: {hw_name} | SMs: {props.multi_processor_count} | VRAM: {vram_gb:.2f} GiB")
    else:
        hw_name = "Host CPU"
        vram_gb = 0.0
        print(f"[⚡ HARDWARE PROBE] CPU Fallback Mode: {torch.get_num_threads()} Threads")

    # Run Dual-Arm Benchmark
    res_adamw = run_training_arm("AdamW", device, cfg, steps=args.steps, batch_size=args.batch_size, seq_len=args.seq_len)
    res_prime = run_training_arm("ZKAEDI-Prime", device, cfg, steps=args.steps, batch_size=args.batch_size, seq_len=args.seq_len)

    # Print ASCII charts
    print_ascii_chart("ADAMW LOSS TRAJECTORY", res_adamw["loss_history"])
    print_ascii_chart("ZKAEDI PRIME HAMILTONIAN DYNAMICS LOSS TRAJECTORY", res_prime["loss_history"])

    # Comparison Table
    print(f"{'='*75}")
    print(f" 📊 DUAL-ARM BENCHMARK RESULTS MATRIX: [{cfg['name']}]")
    print(f"{'='*75}")
    print(f"{'Metric':<30} | {'AdamW Baseline':<18} | {'ZKAEDI-Prime':<18}")
    print(f"{'-'*30}-+-{'-'*18}-+-{'-'*18}")
    print(f"{'Initial Loss':<30} | {res_adamw['initial_loss']:<18.4f} | {res_prime['initial_loss']:<18.4f}")
    print(f"{f'Final Loss (Step {args.steps})':<30} | {res_adamw['final_loss']:<18.4f} | {res_prime['final_loss']:<18.4f}")
    print(f"{'Loss Reduction Delta':<30} | {res_adamw['initial_loss'] - res_adamw['final_loss']:<18.4f} | {res_prime['initial_loss'] - res_prime['final_loss']:<18.4f}")
    print(f"{'Total Execution Time (s)':<30} | {res_adamw['total_time_sec']:<18.2f} | {res_prime['total_time_sec']:<18.2f}")
    print(f"{'Throughput (Tokens/sec)':<30} | {res_adamw['throughput_tokens_sec']:<18,.0f} | {res_prime['throughput_tokens_sec']:<18,.0f}")
    print(f"{'Compute Rate (TFLOPS)':<30} | {res_adamw['average_tflops']:<18.1f} | {res_prime['average_tflops']:<18.1f}")
    print(f"{'Peak VRAM Footprint':<30} | {res_adamw['vram_peak_gb']:<16.2f}GB | {res_prime['vram_peak_gb']:<16.2f}GB")
    print(f"{'='*75}")

    # Emit Artifact Report
    run_report_path = f"artifacts/AI_TRAINING_RUN_{cfg['name'].upper()}.md"
    latest_path = "artifacts/LATEST_RUN_SUMMARY.md"
    os.makedirs("artifacts", exist_ok=True)
    report_content = f"""# 🔱 ZKAEDI PRIME AI TRAINING BENCHMARK REPORT: {cfg['name'].upper()}
### *Dual-Arm Empirical Gauntlet: AdamW vs. ZKAEDI Prime Hamiltonian Energy Optimizer*

- **Date**: {time.strftime("%Y-%m-%d %H:%M:%S UTC", time.gmtime())}  
- **Hardware**: `{hw_name}` ({vram_gb:.2f} GiB VRAM)  
- **Preset**: `{cfg['name']}` ({cfg['description']})  
- **Model Parameters**: {res_adamw['total_params'] / 1e6:.2f}M Parameters  
- **Batch Specification**: Batch Size {res_adamw['batch_size']} × Sequence Length {res_adamw['seq_len']}  

| Metric | PyTorch AdamW Baseline | 🔱 ZKAEDI Prime Hamiltonian Optimizer |
| :--- | :---: | :---: |
| **Initial Loss** | {res_adamw['initial_loss']:.4f} | {res_prime['initial_loss']:.4f} |
| **Final Loss (Step {args.steps})** | {res_adamw['final_loss']:.4f} | {res_prime['final_loss']:.4f} |
| **Loss Reduction** | -{res_adamw['initial_loss'] - res_adamw['final_loss']:.4f} | -{res_prime['initial_loss'] - res_prime['final_loss']:.4f} |
| **Throughput** | {res_adamw['throughput_tokens_sec']:,.0f} tokens/sec | {res_prime['throughput_tokens_sec']:,.0f} tokens/sec |
| **Compute Rate** | {res_adamw['average_tflops']:.1f} TFLOPS | {res_prime['average_tflops']:.1f} TFLOPS |
| **Peak VRAM** | {res_adamw['vram_peak_gb']:.2f} GiB | {res_prime['vram_peak_gb']:.2f} GiB |
| **Total Wall-Clock** | {res_adamw['total_time_sec']:.2f} s | {res_prime['total_time_sec']:.2f} s |

✔ Run report preserved at `{run_report_path}`.
"""
    with open(run_report_path, "w", encoding="utf-8") as f:
        f.write(report_content)
    with open(latest_path, "w", encoding="utf-8") as f:
        f.write(report_content)

    print(f"\n[✔] Master Training Benchmark Complete in {res_adamw['total_time_sec'] + res_prime['total_time_sec']:.2f} seconds.")
    print(f"[✔] Report saved to: {run_report_path} and {latest_path}\n")

if __name__ == "__main__":
    main()
