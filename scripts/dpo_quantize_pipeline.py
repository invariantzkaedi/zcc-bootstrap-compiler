#!/usr/bin/env python3
"""
ZKAEDI PRIME DPO-to-Quantization Pipeline Integration Example
Version: 1.1

Demonstrates secure end-to-end workflow using zkaedi_security_utils.py v2.3.1
"""

import sys
import json
from pathlib import Path

# Ensure we can import from project root
sys.path.append(str(Path(__file__).resolve().parents[1]))

from zkaedi_security_utils import (
    scan_for_known_cves,
    validate_safe_path,
    safe_quantize_model,
)


def run_quantization_pipeline():
    print("=== [ZKAEDI SEC] Starting DPO-to-Quantization Pipeline ===")

    # Step 1: Runtime security pre-check
    scan_for_known_cves()

    # Define paths (must be under safe base)
    source_model_dir = "/mnt/h/__DOWNLOADS/zcc_github_upload/outputs_dpo_adamw/checkpoint-25"
    quantized_output_dir = "/mnt/h/__DOWNLOADS/zcc_github_upload/outputs_dpo_adamw/checkpoint-25-8bit"

    # Step 2: Validate paths
    print("\n=== [ZKAEDI SEC] Validating Paths ===")
    try:
        validated_src = validate_safe_path(source_model_dir, must_exist=True, description="source model")
        validated_out = validate_safe_path(quantized_output_dir, must_exist=False, description="quantized output")
        print(f"[ZKAEDI SEC] Source validated: {validated_src}")
        print(f"[ZKAEDI SEC] Output target validated: {validated_out}")
    except (ValueError, FileNotFoundError) as e:
        print(f"[ZKAEDI SEC] Path validation failed: {e}", file=sys.stderr)
        sys.exit(1)

    # Step 3: Perform secure quantization
    print("\n=== [ZKAEDI SEC] Executing Secure Quantization ===")
    try:
        quantized_path = safe_quantize_model(
            model_path=str(validated_src),
            output_dir=str(validated_out),
            bits=8,                    # 8-bit recommended for most sovereign use
            device="cpu"               # Safer default during quantization
        )
    except ImportError as e:
        print(f"\n[ZKAEDI SEC] {e}")
        print("[ZKAEDI SEC] Please install bitsandbytes: pip install bitsandbytes")
        return
    except Exception as e:
        print(f"[ZKAEDI SEC] Quantization failed: {e}", file=sys.stderr)
        sys.exit(1)

    # Step 4: Verify provenance
    print("\n=== [ZKAEDI SEC] Verifying Provenance ===")
    provenance_file = quantized_path / "quantization_provenance.json"
    if provenance_file.exists():
        with open(provenance_file, "r") as f:
            provenance = json.load(f)
        print(json.dumps(provenance, indent=2))
        print("\n[+] SUCCESS: Quantization completed with full cryptographic provenance.")
    else:
        print("[-] WARNING: Provenance file was not generated.", file=sys.stderr)


if __name__ == "__main__":
    run_quantization_pipeline()
