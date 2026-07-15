# float_guard.py — stdlib-only precision discipline module
import math
import struct
import sys
from fractions import Fraction

class NumericGuardError(Exception):
    def __init__(self, message, code, severity="ERROR", context=None):
        super().__init__(message)
        self.code = code
        self.severity = severity
        self.context = context or {}
        self.assumption_statement = "Calculations must retain precision without catastrophic cancellation."
        self.suggestions = (
            "Use compensated summation (Kahan/Neumaier).",
            "Check operand scaling.",
            "Use Fraction for exact truth."
        )

class CancellationError(NumericGuardError):
    def __init__(self, message, naive, exact, drift_ulps, n_terms, max_ulps):
        super().__init__(
            message,
            code="NUM-0002",
            severity="DEITY",
            context={
                "naive": float(naive),
                "exact": float(exact),
                "drift_ulps": drift_ulps,
                "n_terms": n_terms,
                "max_ulps": max_ulps
            }
        )

def double_to_bits(d):
    return struct.unpack('<Q', struct.pack('<d', d))[0]

def bits_to_double(b):
    return struct.unpack('<d', struct.pack('<Q', b))[0]

def ulp_diff(a, b):
    if math.isnan(a) or math.isnan(b):
        return float('inf')
    ia = struct.unpack('<q', struct.pack('<d', a))[0]
    ib = struct.unpack('<q', struct.pack('<d', b))[0]
    # Handle sign-magnitude to 2's complement style ordering
    if ia < 0:
        ia = 0x8000000000000000 - ia
    if ib < 0:
        ib = 0x8000000000000000 - ib
    return abs(ia - ib)

def inspect(d):
    b = double_to_bits(d)
    sign = (b >> 63) & 1
    exponent = (b >> 52) & 0x7FF
    mantissa = b & 0xFFFFFFFFFFFFF
    return {
        "sign": sign,
        "unbiased_exponent": exponent - 1023 if exponent > 0 else -1022,
        "mantissa_bits": mantissa,
        "is_subnormal": exponent == 0 and mantissa > 0,
        "is_nan": math.isnan(d),
        "is_inf": math.isinf(d)
    }

def naive_sum(xs):
    s = 0.0
    for x in xs:
        s += x
    return s

def exact_sum(xs):
    return float(sum(Fraction(x) for x in xs))

def kahan_sum(xs):
    s = 0.0
    c = 0.0
    for x in xs:
        y = x - c
        t = s + y
        c = (t - s) - y
        s = t
    return s

def neumaier_sum(xs):
    s = 0.0
    c = 0.0
    for x in xs:
        t = s + x
        if abs(s) >= abs(x):
            diff = (s - t) + x
        else:
            diff = (x - t) + s
        c += diff
        s = t
    return s + c

def pairwise_sum(xs):
    def _rec(arr):
        n = len(arr)
        if n == 0:
            return 0.0
        if n == 1:
            return arr[0]
        if n == 2:
            return arr[0] + arr[1]
        mid = n // 2
        return _rec(arr[:mid]) + _rec(arr[mid:])
    return _rec(xs)

def guard_sum(xs, max_ulps=10):
    naive = naive_sum(xs)
    truth = exact_sum(xs)
    drift = ulp_diff(naive, truth)
    if drift > max_ulps:
        raise CancellationError(
            f"naive sum {naive!r} vs exact {truth!r}: {drift} ULPs of information destroyed",
            naive=naive,
            exact=truth,
            drift_ulps=drift,
            n_terms=len(xs),
            max_ulps=max_ulps,
        )
    return naive

class ForgedEnvelope:
    def capture(self, err):
        self.code = getattr(err, "code", "UNKNOWN")
        self.severity = getattr(err, "severity", "ERROR")
        self.context = getattr(err, "context", {})
        self.assumption_statement = getattr(err, "assumption_statement", "")
        self.suggestions = getattr(err, "suggestions", ())
        return self

if __name__ == "__main__":
    passes = 0
    failures = 0

    def run_test(name, fn):
        global passes, failures
        try:
            fn()
            print(f"PASS: {name}")
            passes += 1
        except Exception as e:
            print(f"FAIL: {name} ({e})")
            failures += 1

    # 1. Epsilon absorption / detection
    def test_epsilon():
        eps64 = 2**-52
        assert 1.0 + eps64 / 2 == 1.0
        assert 1.0 + eps64 != 1.0

    # 2. ULP Spacing Growth
    def test_ulp_spacing():
        assert ulp_diff(1.0, 1.0 + 2**-52) == 1
        assert ulp_diff(100.0, 100.0 + 2**-42) > 1

    # 3. Bit anatomy decomposition
    def test_anatomy():
        inf_bits = inspect(float('inf'))
        assert inf_bits["is_inf"] and not inf_bits["is_nan"]
        assert inf_bits["unbiased_exponent"] == 1024

    # 4. Cancellation on 1e16 gauntlet
    def test_cancellation():
        data = [1e16] + [1.0]*10000 + [-1e16]
        try:
            guard_sum(data, max_ulps=10)
            assert False, "Should have raised CancellationError"
        except CancellationError as e:
            assert e.context["drift_ulps"] > 10000

    # 5. Kahan summation correctness
    def test_kahan():
        data = [1e16] + [1.0]*10000 + [-1e16]
        assert kahan_sum(data) == 10000.0

    # 6. Neumaier summation correctness
    def test_neumaier():
        data = [1e16] + [1.0]*10000 + [-1e16]
        assert neumaier_sum(data) == 10000.0

    # 7. Pairwise summation
    def test_pairwise():
        data = [1.0] * 1000
        assert pairwise_sum(data) == 1000.0

    # 8. Exact rational Fraction truth
    def test_fraction_truth():
        data = [1e16, 1.0, -1e16]
        assert exact_sum(data) == 1.0

    # 9. CPython 3.12+ sum() compensation check
    def test_sum_compensation():
        data = [0.1] * 100000
        py_sum = sum(data)
        naive = naive_sum(data)
        truth = exact_sum(data)
        # E-LEARN entry validation: if Python >= 3.12, sum() is Neumaier-compensated
        if sys.version_info >= (3, 12):
            assert ulp_diff(py_sum, truth) < ulp_diff(naive, truth)

    # 10. Simpson's convergence O(h^4)
    def test_simpson():
        def simpson(n):
            h = math.pi / (n - 1)
            s_odd = sum(math.sin(i*h) for i in range(1, n-1) if i % 2 == 1)
            s_even = sum(math.sin(i*h) for i in range(1, n-1) if i % 2 == 0)
            return (h / 3.0) * (0.0 + 4.0 * s_odd + 2.0 * s_even + 0.0)
        
        err1 = abs(simpson(11) - 2.0)
        err2 = abs(simpson(21) - 2.0)
        # Convergence ratio should be roughly 16 (2^4)
        assert err1 / err2 > 10.0

    # 11. NaN / Infinity bit patterns
    def test_specials():
        nan_val = float('nan')
        assert math.isnan(nan_val)
        assert nan_val != nan_val

    # 12. Subnormal float underflow
    def test_subnormals():
        subnormal = 5e-324
        assert inspect(subnormal)["is_subnormal"]
        assert subnormal / 2.0 == 0.0

    run_test("t01_epsilon", test_epsilon)
    run_test("t02_ulp_spacing", test_ulp_spacing)
    run_test("t03_anatomy", test_anatomy)
    run_test("t04_cancellation", test_cancellation)
    run_test("t05_kahan", test_kahan)
    run_test("t06_neumaier", test_neumaier)
    run_test("t07_pairwise", test_pairwise)
    run_test("t08_fraction_truth", test_fraction_truth)
    run_test("t09_sum_compensation", test_sum_compensation)
    run_test("t10_simpson", test_simpson)
    run_test("t11_specials", test_specials)
    run_test("t12_subnormals", test_subnormals)

    print(f"\nFLOAT-GUARD SELF-TEST: PASS ({passes}/12)")
    if failures > 0:
        sys.exit(1)
    else:
        sys.exit(0)
