"""
infinimemory.engram_core - EngramCore: Experience Encoding & Holographic Sharding
================================================================================
Encodes raw experience, data, tensors, and text into holographic engrams with
cryptographic SHA-256 / BLAKE3 hashes and vector embeddings.
"""

import math
import hashlib
from typing import Dict, Any, List, Optional
from infinimemory.models import Engram, sha256_bytes
from infinimemory.errors import EngramEncodingError


def generate_deterministic_embedding(text: str, dim: int = 16) -> List[float]:
    """Generates a deterministic L2-normalized float embedding vector from text
    using signed SHA-256 feature hashing over normalized tokens and character 3-grams."""
    if not text:
        return [0.0] * dim
    cleaned = "".join(ch.lower() if ch.isalnum() else " " for ch in text)
    tokens = [w for w in cleaned.split() if len(w) >= 2]
    compact = cleaned.replace(" ", "")
    ngrams = [compact[i:i + 3] for i in range(max(0, len(compact) - 2))]
    features = [f"tok:{t}" for t in tokens] + [f"ng:{g}" for g in ngrams]
    if not features:
        features = [f"raw:{text}"]

    vec = [0.0] * dim
    for feat in features:
        weight = 2.0 if feat.startswith("tok:") else 1.0
        digest = hashlib.sha256(feat.encode("utf-8")).digest()
        for i in range(dim):
            byte_val = digest[i % len(digest)]
            sign = 1.0 if (byte_val & (1 << (i % 8))) else -1.0
            mag = 0.5 + (digest[(i + 7) % len(digest)] / 255.0)
            vec[i] += sign * weight * mag

    norm = math.sqrt(sum(v * v for v in vec)) or 1.0
    return [v / norm for v in vec]


def create_parity_shards(content: str, num_shards: int = 4, redundant_overlap: bool = False) -> List[str]:
    """Splits content into interleaved redundant holographic parity shards.
    When redundant_overlap=True, emits N primary chunks plus N overlapping 2-chunk
    parity spans so any single or adjacent shard corruption is 100% recoverable."""
    if not content:
        return []
    shards = []
    n = len(content)
    chunk_size = max(1, math.ceil(n / num_shards))
    for i in range(num_shards):
        start = i * chunk_size
        end = min(n, start + chunk_size)
        chunk = content[start:end]
        h_chunk = sha256_bytes(chunk.encode("utf-8"))[:12]
        shards.append(f"shard_{i}:{start}_{end}:{h_chunk}:{chunk}")
    if redundant_overlap:
        for i in range(num_shards):
            if i == num_shards - 1:
                # Full-span holographic mirror shard for catastrophic multi-shard loss
                p_start, p_end = 0, n
            else:
                p_start = min(n, i * chunk_size)
                p_end = min(n, (i + 2) * chunk_size)
            p_chunk = content[p_start:p_end]
            h_pchunk = sha256_bytes(p_chunk.encode("utf-8"))[:12]
            shards.append(f"parity_{i}:{p_start}_{p_end}:{h_pchunk}:{p_chunk}")
    return shards


class EngramCore:
    """Subsystem 1: EngramCore (encode)
    Transforms raw experiences and records into verified, tamper-evident engrams.
    """
    def __init__(self, embedding_dim: int = 16, num_parity_shards: int = 4, redundant_overlap: bool = False):
        self.embedding_dim = embedding_dim
        self.num_parity_shards = num_parity_shards
        self.redundant_overlap = redundant_overlap

    def encode(
        self,
        key: str,
        content: str,
        tags: Optional[List[str]] = None,
        metadata: Optional[Dict[str, Any]] = None,
        resonance_weight: float = 1.0,
        epoch: int = 1
    ) -> Engram:
        if not key:
            raise EngramEncodingError("Engram key cannot be empty.")
        if content is None:
            raise EngramEncodingError("Engram content cannot be None.")

        tags = tags or []
        metadata = metadata or {}
        embedding = generate_deterministic_embedding(content, dim=self.embedding_dim)
        content_hash = sha256_bytes(content.encode("utf-8"))
        parity_shards = create_parity_shards(
            content,
            num_shards=self.num_parity_shards,
            redundant_overlap=self.redundant_overlap,
        )

        engram = Engram(
            id="",
            key=key,
            content=content,
            embedding=embedding,
            tags=tags,
            metadata=metadata,
            epoch=epoch,
            resonance_weight=resonance_weight,
            content_hash=content_hash,
            parity_shards=parity_shards
        )
        return engram
