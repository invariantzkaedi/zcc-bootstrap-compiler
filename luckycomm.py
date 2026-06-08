import os
import time
import hmac
import hashlib
import platform
import json

def get_env_secret(override_key=None):
    if override_key:
        return override_key.encode()
    return os.environ.get("ZKAEDI_FORGE_KEY", platform.node() + platform.machine()).encode()

def create_handoff(dispatcher_output, artifacts, directive):
    timestamp = int(time.time())
    
    # Construct the handoff structure
    handoff = {
        "protocol_version": "1.2",
        "handoff_id": f"777-JACKPOT-{hashlib.sha256(str(timestamp).encode()).hexdigest()[:8].upper()}",
        "timestamp": timestamp,
        "directive": directive,
        "dispatcher_output": dispatcher_output,
        "artifacts": artifacts
    }
    
    # Generate the HMAC seal for verification
    body_str = json.dumps(handoff, sort_keys=True)
    msg = f"{body_str}:{timestamp}"
    
    secret = get_env_secret()
    sig = hmac.new(secret, msg.encode(), hashlib.sha256).hexdigest()
    
    handoff["hmac_seal"] = sig
    return handoff

def verify_handoff(handoff):
    sig_to_verify = handoff.get("hmac_seal")
    if not sig_to_verify:
        return False
        
    handoff_copy = dict(handoff)
    del handoff_copy["hmac_seal"]
    
    timestamp = handoff.get("timestamp", 0)
    body_str = json.dumps(handoff_copy, sort_keys=True)
    msg = f"{body_str}:{timestamp}"
    
    secret = get_env_secret()
    expected_sig = hmac.new(secret, msg.encode(), hashlib.sha256).hexdigest()
    return hmac.compare_digest(sig_to_verify, expected_sig)
