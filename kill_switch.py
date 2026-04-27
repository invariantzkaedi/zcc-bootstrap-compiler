import os
import sys
import json
import logging

logging.basicConfig(level=logging.INFO, format='[KILL_SWITCH] %(message)s')

def assert_not_globally_disabled():
    if os.environ.get("CHIMERA_GLOBAL_DISABLE") == "1":
        alert = {
            "status": "blocked",
            "reason": "global_kill_switch_enabled",
            "native_execution_attempted": False
        }
        sys.stderr.write(json.dumps(alert) + "\n")
        sys.exit(1)
