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
    if not prompt:
        raise ValueError("Prompt must not be empty")
    if not completion:
        raise ValueError("Completion must not be empty")
        
    device = next(model.parameters()).device
    full_text = prompt + completion

    encoded = tokenizer(
        full_text,
        return_tensors="pt",
        add_special_tokens=False,
        return_offsets_mapping=True,
    )

    input_ids = encoded.input_ids.to(device)
    offsets = encoded.offset_mapping[0]

    # Validate that token length is sufficient for sequence shift prediction
    if input_ids.shape[1] < 2:
        raise ValueError("Input produced fewer than two tokens")

    boundary = len(prompt)

    # Score only tokens that start wholly at or after the boundary (wholly inside completion)
    # and exclude special/synthetic tokens that have end <= start (like padding or empty tokens)
    completion_mask = torch.tensor(
        [
            start >= boundary and end > start
            for start, end in offsets.tolist()
        ],
        device=device,
        dtype=torch.bool,
    )

    if not completion_mask.any():
        raise ValueError("Completion produced no tokens")

    outputs = model(input_ids=input_ids)
    logits = outputs.logits[:, :-1, :]
    labels = input_ids[:, 1:]

    # Token i is predicted by logit i-1.
    prediction_mask = completion_mask[1:]

    if not prediction_mask.any():
        raise ValueError("Completion produced no predictable tokens")

    token_logps = F.log_softmax(logits, dim=-1).gather(
        -1,
        labels.unsqueeze(-1),
    ).squeeze(-1)

    return token_logps[:, prediction_mask].mean(dim=-1)

def binary_reference_logratio_loss(
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
    """Experimental reference-relative binary preference loss."""
    batch_size = len(prompts)
    if batch_size == 0:
        raise ValueError("Preference batch is empty")
    if len(completions) != batch_size:
        raise ValueError("prompts and completions must have equal length")
    if desirable.numel() != batch_size:
        raise ValueError("desirable labels must match batch length")

    # Enforce that the batch contains both desirable and undesirable examples
    positive_count = int(desirable.sum().item())
    negative_count = batch_size - positive_count
    if positive_count == 0 or negative_count == 0:
        raise ValueError("Batch must contain both desirable and undesirable examples")

    # Put reference to evaluation and requires_grad_ False (immutability)
    reference.eval()
    reference.requires_grad_(False)
    
    # Store policy training state and restore it afterwards
    policy_was_training = policy.training
    policy.train()
    
    try:
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
    finally:
        policy.train(policy_was_training)

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
