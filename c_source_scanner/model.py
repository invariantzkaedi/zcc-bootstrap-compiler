import torch
import torch.nn as nn

class ZccSubMLP(nn.Module):
    """
    Vulnerability specialist MLP (exactly 1,089 parameters).
    Formula: 32 -> 32 -> 1
    Weights: 32 * 32 = 1024
    Bias: 32
    Weights: 32 * 1 = 32
    Bias: 1
    Total: 1024 + 32 + 32 + 1 = 1089 parameters.
    """
    def __init__(self):
        super(ZccSubMLP, self).__init__()
        self.fc1 = nn.Linear(32, 32)
        self.relu = nn.ReLU()
        self.fc2 = nn.Linear(32, 1)
        self.sigmoid = nn.Sigmoid()

    def forward(self, x):
        out = self.fc1(x)
        out = self.relu(out)
        out = self.fc2(out)
        return self.sigmoid(out)

class ZccCSourceModel(nn.Module):
    def __init__(self):
        super(ZccCSourceModel, self).__init__()
        self.specialists = nn.ModuleList([ZccSubMLP() for _ in range(8)])
        # Reliability weights auto-computed from validation F1 score (initialized uniformly)
        self.register_buffer("f1_weights", torch.ones(8) / 8.0)

    def forward(self, x):
        """
        Runs the inputs through all 8 specialists.
        Returns:
            specialist_probs: Tensor of shape (batch, 8)
            weighted_prob: Tensor of shape (batch, 1) representing ensemble consensus
        """
        batch_size = x.size(0)
        probs = []
        for mlp in self.specialists:
            probs.append(mlp(x)) # (batch_size, 1)
        
        specialist_probs = torch.cat(probs, dim=1) # (batch_size, 8)
        
        # Reliability-weighted voting consensus
        # Normalize weights just in case
        w = self.f1_weights / torch.sum(self.f1_weights)
        weighted_prob = torch.matmul(specialist_probs, w.unsqueeze(1)) # (batch_size, 1)
        
        return specialist_probs, weighted_prob

    def set_f1_weights(self, f1_scores):
        """Sets the F1 weights from a list of floats"""
        assert len(f1_scores) == 8
        tensor_f1 = torch.tensor(f1_scores, dtype=torch.float32)
        total = torch.sum(tensor_f1)
        if total > 0:
            self.f1_weights.copy_(tensor_f1 / total)
        else:
            self.f1_weights.copy_(torch.ones(8) / 8.0)
