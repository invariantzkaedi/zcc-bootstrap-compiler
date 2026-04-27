import os

def assert_sandbox_mode() -> None:
    """
    Hard execution guard.

    Generated C is hostile.
    The sandbox is the security boundary.
    The energy metric is a deterministic experimental signal, not a
    cryptographic primitive.
    """

    if os.getenv("CHIMERA_SANDBOX_ONLY") != "1":
        raise RuntimeError(
            "Refusing to execute native generated code outside sandbox mode. "
            "Set CHIMERA_SANDBOX_ONLY=1 only inside an approved isolated worker."
        )
