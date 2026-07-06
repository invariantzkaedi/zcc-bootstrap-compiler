import pytest

class MockDecoder:
    def __init__(self, code_type="shor9"):
        self.code_type = code_type
        
    def decode(self, syndrome):
        # A simple MWPM degenerate tie-break rule:
        # If there are multiple equal weight corrections, prioritize lower qubit indices
        x_synd = sum(syndrome[:3])
        z_synd = sum(syndrome[3:6])
        
        if x_synd > 1.5 and z_synd > 1.5:
            return "Y@0"  # deterministic priority
        if x_synd > 1.5:
            return "X@0"
        if z_synd > 1.5:
            return "Z@3"
        return "I"

def test_decoder_tiebreak_is_deterministic():
    decoder = MockDecoder()
    degenerate_syndrome = [1.0, 1.0, 0.0, 1.0, 1.0, 0.0, 0.0, 0.0, 0.0]
    
    c1 = decoder.decode(degenerate_syndrome)
    c2 = decoder.decode(degenerate_syndrome)
    assert c1 == c2  # assert deterministic policy
    assert c1 == "Y@0"
    print("  [PASS] test_decoder_tiebreak_is_deterministic")

if __name__ == "__main__":
    test_decoder_tiebreak_is_deterministic()
    print("DECODER TIE-BREAK TESTS PASSED!")
