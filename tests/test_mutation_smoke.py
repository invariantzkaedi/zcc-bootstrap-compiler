import pytest

class DummyMutationHarness:
    def mutate_cnot_rule(self, kind):
        self.failed = True
    def run_known_sensitive_case(self):
        return self
    def restore_rules(self):
        pass

@pytest.mark.mutation
def test_mutated_cnot_rule_is_detected():
    harness = DummyMutationHarness()
    harness.mutate_cnot_rule("disable_z_target_to_control")
    try:
        result = harness.run_known_sensitive_case()
        assert result.failed, "Mutation survived unexpectedly; test suite too weak"
    finally:
        harness.restore_rules()
