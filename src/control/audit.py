import os
import json
import time
import hashlib
from typing import Dict, Any, List

class AuditLogger:
    def __init__(self, events_path: str = "events/events.jsonl", provenance_path: str = "provenance/provenance.jsonl"):
        self.events_path = events_path
        self.provenance_path = provenance_path
        
        os.makedirs(os.path.dirname(self.events_path), exist_ok=True)
        os.makedirs(os.path.dirname(self.provenance_path), exist_ok=True)

    def log_event(self, event_type: str, task_id: str, actor: str, payload: Dict[str, Any]) -> Dict[str, Any]:
        record = {
            "timestamp": time.time(),
            "event_type": event_type,
            "task_id": task_id,
            "actor": actor,
            "payload": payload
        }
        
        digest = hashlib.sha256(json.dumps(record, sort_keys=True).encode("utf-8")).hexdigest()
        record["event_digest"] = digest[:16]
        
        with open(self.events_path, "a", encoding="utf-8") as f:
            f.write(json.dumps(record) + "\n")
            
        return record

    def log_provenance(self, node_id: str, parent_refs: List[str], generator_role: str, inputs_digest: str, action: str) -> Dict[str, Any]:
        record = {
            "timestamp": time.time(),
            "node_id": node_id,
            "parent_refs": parent_refs,
            "generator_role": generator_role,
            "inputs_digest": inputs_digest,
            "action": action
        }
        
        digest = hashlib.sha256(json.dumps(record, sort_keys=True).encode("utf-8")).hexdigest()
        record["provenance_digest"] = digest[:16]
        
        with open(self.provenance_path, "a", encoding="utf-8") as f:
            f.write(json.dumps(record) + "\n")
            
        return record
