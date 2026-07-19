"""Tier-0 configuration evaluator.

Trusted harness code that interprets an UNTRUSTED candidate config. It never
exec()s candidate content — configs are data, interpreted declaratively
(Milestone E rule: mutations are patch descriptions, not code).

Exit 0 = candidate config well-formed and passes the eval battery it declares.
Any other exit = fail. The runner, not this process's prose, is the evidence.

`_zk_selftest` keys are fault-injection hooks used ONLY by tests/selftest.py to
prove the runner's gates can go red (fabrication-prevention rule 3: a gate
never observed failing is UNVERIFIED). They simulate misbehaving candidates.
"""
from __future__ import annotations

import json
import os
import sys
import time


def main() -> int:
    with open(sys.argv[1], "rb") as fh:
        candidate = json.load(fh)

    # ---- fault-injection hooks (selftest only) -----------------------------
    st = candidate.get("_zk_selftest", {})
    if st.get("crash"):
        os.kill(os.getpid(), 11)                      # SIGSEGV: crash survival test
    if st.get("hang"):
        time.sleep(3600)                               # timeout test
    if st.get("exit_code") is not None:
        return int(st["exit_code"])                    # pass/fail distinction test
    if st.get("dump_env"):
        print(json.dumps(dict(os.environ)))            # env-sanitization test
        return 0
    if st.get("alloc_bytes"):
        _ = bytearray(int(st["alloc_bytes"]))          # memory-limit test
        return 0
    if st.get("network_probe"):
        import socket                                   # netns test
        s = socket.socket()
        s.settimeout(3)
        s.connect(("1.1.1.1", 80))                     # must fail in Tier 0
        return 0
    if st.get("fabricate"):
        print("All tests passed ✅ everything green")   # E8 bait, no run_id
        return 0

    # ---- real path: declarative config checks ------------------------------
    cfg_keys = ("canonical_prompt", "tool_policy", "scaffold", "eval_config")
    for k in cfg_keys:
        v = candidate.get(k)
        if not isinstance(v, (str, dict, list)) or (isinstance(v, str) and not v.strip()):
            print(f"config check failed: {k} empty or wrong type", file=sys.stderr)
            return 3
    print(json.dumps({"evaluated": True,
                      "checks": {k: "ok" for k in cfg_keys}}))
    return 0


if __name__ == "__main__":
    sys.exit(main())
