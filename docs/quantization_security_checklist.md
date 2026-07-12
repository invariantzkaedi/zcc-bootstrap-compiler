# ZKAEDI PRIME Quantization Security Checklist
**Version**: 1.0  
**Date**: 2026-07-12  
**Scope**: Secure quantization of DPO-aligned and other models using `zkaedi_security_utils.py` v2.3.1

---

## 1. Pre-Quantization Requirements

- [ ] Run `scan_for_known_cves()` before starting any quantization workflow.
- [ ] Verify the source model was loaded using `load_model_hardened()` (or came from a previously validated checkpoint).
- [ ] Confirm the full-precision model SHA-256 is registered in the ZKAEDI model allow-list.
- [ ] Validate the target output directory using `validate_safe_path()`.
- [ ] Ensure `transformers >= 5.3.0` is installed and pinned in the environment.
- [ ] For GPTQ models: Confirm `quantize_config.json` exists before attempting to load.

---

## 2. Quantization Execution (bitsandbytes)

- [ ] Use `safe_quantize_model()` for all new quantization jobs.
- [ ] Prefer **8-bit** quantization unless memory constraints are severe.
- [ ] When using 4-bit, explicitly set `quant_type="nf4"` and `compress_statistics=True`.
- [ ] Keep the model on CPU during layer replacement when possible (device="cpu").
- [ ] Always output in **safetensors** format (`safe_serialization=True`).
- [ ] Record provenance automatically via the built-in `quantization_provenance.json`.

---

## 3. GPTQ Model Loading (When Using Pre-Quantized Models)

- [ ] Always load GPTQ models via `load_gptq_model_hardened()`.
- [ ] Enable `verify_gptq_config_integrity()` (called automatically inside the loader).
- [ ] Supply `expected_config_hash` for high-security or production models.
- [ ] Keep `disable_exllama=True` and `disable_exllamav2=True` (default).
- [ ] Keep `use_triton=False` unless explicitly required and audited.
- [ ] Verify `bits` and `group_size` values after loading match expected configuration.

---

## 4. Post-Quantization Validation

- [ ] Confirm `quantization_provenance.json` was generated.
- [ ] Re-run `validate_training_health.py` on the quantized model (compare margins, positive rate, loss).
- [ ] Perform targeted safety/refusal testing on security-critical prompts.
- [ ] Compare key metrics against the full-precision baseline (acceptable degradation thresholds should be defined per model class).
- [ ] Verify the final directory only contains `safetensors` files (no `.bin` or `.pt`).

---

## 5. Cryptographic & Audit Controls

- [ ] Store both the full-precision and quantized model with clear lineage in `quantization_provenance.json`.
- [ ] For GPTQ models, optionally record the SHA-256 of `quantize_config.json`.
- [ ] Register the quantized model in the ZKAEDI model registry/allow-list before deployment.
- [ ] Retain the original full-precision checkpoint for rollback capability (minimum 90 days recommended).

---

## 6. Environment & Operational Security

- [ ] Run quantization jobs in isolated/trusted environments when possible.
- [ ] Pin `bitsandbytes` and `auto-gptq` (if used) to known-good versions.
- [ ] Log all quantization runs with timestamp, operator, source model hash, and method.
- [ ] Do not use untrusted calibration datasets for GPTQ quantization.

---

## 7. Decision Matrix: bitsandbytes vs GPTQ

| Criteria                        | Recommended Choice      | Reason |
|--------------------------------|-------------------------|--------|
| Maximum security / auditability | `bitsandbytes` 8-bit    | Lower attack surface, integrates cleanly with hardened loader |
| Memory-constrained inference    | `bitsandbytes` 4-bit    | Still safer than GPTQ for most sovereign use cases |
| Maximum performance (4-bit)     | GPTQ (via hardened loader) | Only when performance gain justifies added risk |
| High-security / air-gapped nodes| `bitsandbytes` only     | Avoids custom kernel dependencies |

---

## 8. Emergency / Rollback Procedures

- [ ] Maintain the ability to quickly redeploy the non-quantized version.
- [ ] Document the exact quantization command and parameters used for every deployed model.
- [ ] Have a process to revoke a quantized model from the allow-list if anomalies are detected.
