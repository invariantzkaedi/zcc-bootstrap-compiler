"""Tests for tier_parser. Covers the T1-vs-T10 collision case that motivated the fix."""
from tier_parser import (
    ParsedTier, TierParseError, is_t0, parse_tier,
    required_quality_for_tier, severity_for,
)


results = []


def expect(name, actual, expected):
    ok = actual == expected
    results.append((ok, name))
    flag = "PASS" if ok else "*** FAIL ***"
    print(f"[{flag}] {name}")
    if not ok:
        print(f"        expected={expected!r}")
        print(f"        actual  ={actual!r}")


def expect_raises(name, exc_type, fn):
    try:
        fn()
        results.append((False, name))
        print(f"[*** FAIL ***] {name}  (no exception)")
    except exc_type:
        results.append((True, name))
        print(f"[PASS] {name}")


print("=" * 60)
print("TIER PARSER")
print("=" * 60)

# Basic parsing
p = parse_tier("T0")
expect("T-1: T0 → n=0, ordinal=None", (p.n, p.ordinal), (0, None))

p = parse_tier("T0-01")
expect("T-2: T0-01 → n=0, ordinal=1", (p.n, p.ordinal), (0, 1))

p = parse_tier("T7-04")
expect("T-3: T7-04 → n=7, ordinal=4", (p.n, p.ordinal), (7, 4))

# THE BUG WE'RE FIXING — T10 must not be confused with T1
print()
print("--- T1-vs-T10 collision (was the bug) ---")
expect("T-4a: is_t0('T0') True", is_t0("T0"), True)
expect("T-4b: is_t0('T1') False", is_t0("T1"), False)
expect("T-4c: is_t0('T10') False", is_t0("T10"), False)
expect("T-4d: is_t0('T0-01') True", is_t0("T0-01"), True)
expect("T-4e: is_t0('T10-01') False", is_t0("T10-01"), False)

p1 = parse_tier("T1")
p10 = parse_tier("T10")
expect("T-4f: T1.n != T10.n", p1.n == p10.n, False)

# severity_for distinguishes T1 from T10
expect("T-5a: severity('T1-01') == 70", severity_for("T1-01"), 70)
expect("T-5b: severity('T10-01') == 10 (out of ladder)",
       severity_for("T10-01"), 10)

# required_quality_for_tier
expect("T-6a: T0 → 4", required_quality_for_tier("T0"), 4)
expect("T-6b: T3-01 → 5", required_quality_for_tier("T3-01"), 5)
expect("T-6c: T6 → 4", required_quality_for_tier("T6"), 4)
expect("T-6d: T2-04 → 3", required_quality_for_tier("T2-04"), 3)
expect("T-6e: T10-01 → 3 (default)", required_quality_for_tier("T10-01"), 3)

# Malformed input — must raise rather than fall through
print()
print("--- malformed input raises ---")
expect_raises("T-7a: '' raises", TierParseError, lambda: parse_tier(""))
expect_raises("T-7b: 'T' raises", TierParseError, lambda: parse_tier("T"))
expect_raises("T-7c: 'T-01' raises", TierParseError, lambda: parse_tier("T-01"))
expect_raises("T-7d: 't0-01' (lower) raises", TierParseError,
              lambda: parse_tier("t0-01"))
expect_raises("T-7e: 'T0-01-extra' raises", TierParseError,
              lambda: parse_tier("T0-01-extra"))
expect_raises("T-7f: 'T20' (over max) raises", TierParseError,
              lambda: parse_tier("T20"))
expect_raises("T-7g: None raises", TierParseError,
              lambda: parse_tier(None))


print()
passed = sum(1 for ok, _ in results if ok)
print(f"=== {passed}/{len(results)} tier-parser tests passed ===")
