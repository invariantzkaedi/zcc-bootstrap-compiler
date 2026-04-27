import os
import sys
import json
import datetime
import traceback
from pathlib import Path

def emit_alert(severity: str, event: str, component: str, action: str):
    alert = {
        "severity": severity,
        "event": event,
        "component": component,
        "action": action,
        "timestamp_utc": datetime.datetime.utcnow().isoformat() + "Z"
    }
    
    alert_json = json.dumps(alert)
    sys.stderr.write(alert_json + "\n")
    
    # Try to write to release_artifacts
    try:
        os.makedirs("release_artifacts", exist_ok=True)
        # Use a unique name to avoid overwriting multiple alerts
        filename = f"release_artifacts/alert_{int(datetime.datetime.utcnow().timestamp())}_{event}.json"
        Path(filename).write_text(alert_json, encoding="utf-8")
    except Exception as e:
        sys.stderr.write(f"FAILED TO WRITE ALERT TO DISK: {e}\n")
        sys.exit(1)
