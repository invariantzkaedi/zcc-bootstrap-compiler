import torch
from online_kto_objective import completion_logprob

def promotion_allowed(metrics: dict) -> tuple[bool, list[str]]:
    failures = []

    if not metrics.get("all_safety_tests_passed", False):
        failures.append("safety regression")

    if metrics.get("policy_kl", 0.0) > metrics.get("max_policy_kl", 0.02):
        failures.append("policy drift exceeded limit")

    if metrics.get("heldout_success_rate_delta", 0.0) < 0.0:
        failures.append("held-out success rate regressed")

    if metrics.get("malformed_rejection_rate", 0.0) < 1.0:
        failures.append("malformed-input rejection regressed")

    if metrics.get("pareto_invariants_passed") is not True:
        failures.append("Pareto invariants failed")

    return len(failures) == 0, failures

def refresh_anchor_field(policy, reference, tokenizer, anchors, pairs):
    refreshed = []

    for anchor in anchors:
        shifts = []
        # Reconstruct standard prompt from coordinate anchors
        prompt = f"### System:\n{anchor['prompt']}\n\n### Instruction:\nEvaluate agent scaffold: {','.join(anchor['steps'])}\n\n### Response:\n"

        for pair in pairs:
            policy_margin = (
                completion_logprob(policy, tokenizer, prompt, pair["chosen"])
                - completion_logprob(policy, tokenizer, prompt, pair["rejected"])
            )

            with torch.no_grad():
                reference_margin = (
                    completion_logprob(reference, tokenizer, prompt, pair["chosen"])
                    - completion_logprob(reference, tokenizer, prompt, pair["rejected"])
                )

            shifts.append(
                float((policy_margin - reference_margin).item())
            )

        refreshed.append({
            **anchor,
            "shift_margin": sum(shifts) / len(shifts),
        })

    return refreshed
