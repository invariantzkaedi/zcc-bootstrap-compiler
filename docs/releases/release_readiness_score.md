# Release Readiness Score Model (v1)

Start at 100 points.

## Mandatory Invariants (Release-Blocking)
- **ZCC-EF-0 Violation**: -100 (Immediate STOP / RELEASE BLOCKED; self-certification, manipulated provenance, or missing independent verification authority).

## Deductions
- Correctness gate failed: -50
- Perf gate failed: -30
- Flake rate >5%: -10
- Runtime geomean <0%: -10
- Any open P0 optimizer/verifier issue: -20
- Missing rollback playbook: -10

## Bands
- 90–100: Ready (provided 0 ZCC-EF-0 violations)
- 75–89: Caution (release manager approval required; 0 ZCC-EF-0 violations)
- <75: Not ready (or any ZCC-EF-0 violation)

