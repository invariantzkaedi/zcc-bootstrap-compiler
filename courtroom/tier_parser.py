"""tier_parser.py — closes HYG-IC-002 / HYG-CR-001.

The original compiler used `tier.startswith("T0")` to detect T0/P0
tumblers. That collides at T10+ ("T10-01".startswith("T1") is True,
breaking T1-vs-T10 distinction). This module parses tier strings into
(tier_num, ordinal) pairs and exposes helpers that callers should use
instead of prefix-matching.

Tier format: ``T<n>[-<m>]`` where n in [0, 15] and m is an optional
two-digit ordinal. Examples: ``T0``, ``T0-01``, ``T7-04``, ``T10-12``.
Anything else raises TierParseError.
"""
from __future__ import annotations

import re
from dataclasses import dataclass


# Match T<n> or T<n>-<m>. n is unbounded in the regex but bounded below.
_TIER_RE = re.compile(r"^T(\d+)(?:-(\d+))?$")
MAX_TIER = 15      # generous ceiling; T0..T7 is the documented ladder


class TierParseError(ValueError):
    """Raised when a tier string cannot be parsed."""


@dataclass(frozen=True)
class ParsedTier:
    n: int                 # the tier number (T0 → 0, T10 → 10)
    ordinal: int | None    # the slot within the tier (T0-01 → 1) or None
    raw: str

    @property
    def is_t0(self) -> bool:
        return self.n == 0


def parse_tier(tier: str) -> ParsedTier:
    """Parse a tier string. Raises TierParseError on bad input."""
    if not isinstance(tier, str) or not tier:
        raise TierParseError(f"tier must be non-empty string, got {tier!r}")
    m = _TIER_RE.match(tier)
    if not m:
        raise TierParseError(f"unparseable tier: {tier!r}")
    n = int(m.group(1))
    if n > MAX_TIER:
        raise TierParseError(f"tier number {n} > MAX_TIER={MAX_TIER}")
    ordinal = int(m.group(2)) if m.group(2) is not None else None
    return ParsedTier(n=n, ordinal=ordinal, raw=tier)


def is_t0(tier: str) -> bool:
    """Robust replacement for ``tier.startswith('T0')``.

    Returns True iff tier is a valid T0 tier (T0 or T0-NN).
    Raises on malformed input rather than silently returning False —
    a typo in a tier string is a forensic problem, not a fall-through.
    """
    return parse_tier(tier).is_t0


def required_quality_for_tier(tier: str) -> int:
    """Replacement for the prefix-match version in invariant_compiler."""
    p = parse_tier(tier)
    if p.n == 0:
        return 4
    if p.n == 3:
        return 5
    if p.n == 6:
        return 4
    return 3


def severity_for(tier: str) -> int:
    """Replacement for the prefix-match Charge.severity_for in courtroom."""
    p = parse_tier(tier)
    weights = {0: 100, 1: 70, 2: 50, 3: 40,
               4: 30, 5: 25, 6: 20, 7: 15}
    return weights.get(p.n, 10)
