import math
import torch
from online_kto_objective import completion_logprob

REQUIRED_PROMOTION_METRICS = {
    "all_safety_tests_passed",
    "policy_kl",
    "max_policy_kl",
    "heldout_success_rate_delta",
    "malformed_rejection_rate",
    "pareto_invariants_passed",
}

def promotion_allowed(metrics: dict) -> tuple[bool, list[str]]:
    failures = []

    # Check for missing metrics
    missing = sorted(REQUIRED_PROMOTION_METRICS - metrics.keys())
    if missing:
        failures.append(f"missing promotion metrics: {missing}")
        return False, failures

    numeric_names = {
        "policy_kl",
        "max_policy_kl",
        "heldout_success_rate_delta",
        "malformed_rejection_rate",
    }

    parsed: dict[str, float] = {}

    for name in numeric_names:
        try:
            parsed[name] = float(metrics[name])
        except (TypeError, ValueError):
            failures.append(f"{name} is not a valid float")
            continue

        if not math.isfinite(parsed[name]):
            failures.append(f"{name} is not finite")

    if failures:
        return False, failures

    # Semantic bounds checks
    if parsed["max_policy_kl"] <= 0.0:
        failures.append("max_policy_kl must be positive")

    if parsed["policy_kl"] < -1e-6:
        failures.append("policy_kl is unexpectedly negative")

    if not 0.0 <= parsed["malformed_rejection_rate"] <= 1.0:
        failures.append("malformed_rejection_rate must be within [0, 1]")

    if not -1.0 <= parsed["heldout_success_rate_delta"] <= 1.0:
        failures.append("heldout_success_rate_delta must be within [-1, 1]")

    # Safety tests check
    if metrics["all_safety_tests_passed"] is not True:
        failures.append("safety regression")

    # Policy KL check
    if parsed["policy_kl"] > parsed["max_policy_kl"]:
        failures.append("policy drift exceeded limit")

    # Success rate delta check
    if parsed["heldout_success_rate_delta"] < 0.0:
        failures.append("held-out success rate regressed")

    # Malformed rejection check
    if parsed["malformed_rejection_rate"] < 1.0:
        failures.append("malformed-input rejection regressed")

    # Pareto Invariant check
    if metrics["pareto_invariants_passed"] is not True:
        failures.append("Pareto invariants failed")

    return len(failures) == 0, failures

@torch.inference_mode()
def refresh_anchor_field(policy, reference, tokenizer, anchors, pairs):
    if not anchors:
        raise ValueError("anchors must not be empty")
    if not pairs:
        raise ValueError("evaluation pairs must not be empty")

    for anchor in anchors:
        if not isinstance(anchor.get("prompt"), str):
            raise TypeError("anchor prompt must be a string")
        if not isinstance(anchor.get("steps"), list):
            raise TypeError("anchor steps must be a list")

    # Store prior state of policy model
    policy_was_training = policy.training
    
    # Set models to evaluation mode to remove dropout or other source of noise
    policy.eval()
    reference.eval()
    
    try:
        refreshed = []
        for anchor in anchors:
            shifts = []
            prompt = f"### System:\n{anchor['prompt']}\n\n### Instruction:\nEvaluate agent scaffold: {','.join(anchor['steps'])}\n\n### Response:\n"

            for pair in pairs:
                policy_margin = (
                    completion_logprob(policy, tokenizer, prompt, pair["chosen"])
                    - completion_logprob(policy, tokenizer, prompt, pair["rejected"])
                )

                reference_margin = (
                    completion_logprob(reference, tokenizer, prompt, pair["chosen"])
                    - completion_logprob(reference, tokenizer, prompt, pair["rejected"])
                )

                shifts.append(
                    float((policy_margin - reference_margin_eval).item())
                )

            refreshed.append({
                **anchor,
                "shift_margin": sum(shifts) / len(shifts),
            })
        return refreshed
    finally:
        policy.train(policy_was_training)
        # Reference is frozen/immutable, force evaluation mode
        reference.eval()
