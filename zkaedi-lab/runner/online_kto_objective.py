import torch
import torch.nn.functional as F
from dataclasses import dataclass

@dataclass(frozen=True)
class OnlineTrainConfig:
    learning_rate: float = 1e-6
    max_grad_norm: float = 0.5
    update_batch_size: int = 16
    minimum_positive: int = 4
    minimum_negative: int = 4
    max_steps_per_cycle: int = 2
    max_policy_kl: float = 0.02

def completion_logprob(model, tokenizer, prompt: str, completion: str) -> torch.Tensor:
    prompt_ids = tokenizer(
        prompt,
        return_tensors="pt",
        add_special_tokens=False,
    ).input_ids.to(model.device)

    full_ids = tokenizer(
        prompt + completion,
        return_tensors="pt",
        add_special_tokens=False,
    ).input_ids.to(model.device)

    prompt_len = prompt_ids.shape[1]
    if full_ids.shape[1] <= prompt_len:
        raise ValueError("Completion produced no tokens")

    logits = model(full_ids).logits
    prediction_logits = logits[:, prompt_len - 1 : -1, :]
    completion_labels = full_ids[:, prompt_len:]

    token_logps = F.log_softmax(
        prediction_logits,
        dim=-1,
    ).gather(
        dim=-1,
        index=completion_labels.unsqueeze(-1),
    ).squeeze(-1)

    return token_logps.mean(dim=-1)

def kto_like_loss(
    policy,
    reference,
    tokenizer,
    prompts: list[str],
    completions: list[str],
    desirable: torch.Tensor,
    *,
    beta: float = 0.1,
    desirable_weight: float = 1.0,
    undesirable_weight: float = 1.0,
) -> torch.Tensor:
    policy_logps = []
    reference_logps = []

    for prompt, completion in zip(prompts, completions):
        policy_logps.append(
            completion_logprob(policy, tokenizer, prompt, completion)
        )
        with torch.no_grad():
            reference_logps.append(
                completion_logprob(reference, tokenizer, prompt, completion)
            )

    policy_logps = torch.cat(policy_logps)
    reference_logps = torch.cat(reference_logps)
    log_ratio = policy_logps - reference_logps

    # A batch-centered reference point.
    reference_point = log_ratio.detach().mean()
    gain = beta * (log_ratio - reference_point)

    desirable = desirable.to(
        device=gain.device,
        dtype=torch.bool,
    )

    positive_loss = -F.logsigmoid(gain[desirable]).mean() if desirable.any() else gain.new_tensor(0.0)
    negative_loss = -F.logsigmoid(-gain[~desirable]).mean() if (~desirable).any() else gain.new_tensor(0.0)

    return (
        desirable_weight * positive_loss
        + undesirable_weight * negative_loss
    )

def effective_learning_rate(
    base_lr: float,
    energy: float,
    confidence: float,
    alpha: float = 0.25,
) -> float:
    confidence = min(max(confidence, 0.0), 1.0)
    positive_energy = max(energy, 0.0)

    return (
        base_lr
        * confidence
        / (1.0 + alpha * positive_energy)
    )
