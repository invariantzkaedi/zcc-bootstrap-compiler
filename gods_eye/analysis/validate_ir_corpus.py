#!/usr/bin/env python3
"""
IR Telemetry Corpus Validator
Validates JSON structure and extracts metrics
"""

import json
import sys
from pathlib import Path
from collections import defaultdict

def validate_corpus(jsonl_file):
    """Validate JSONL telemetry file"""
    
    print(f"\n🔍 Validating: {jsonl_file.name}")
    print("=" * 60)
    
    stats = defaultdict(int)
    errors = []
    
    with open(jsonl_file) as f:
        for line_num, line in enumerate(f, 1):
            if not line.strip():
                continue
            
            try:
                event = json.loads(line)
                
                # Track event types
                event_type = event.get('event_type', 'unknown')
                stats[event_type] += 1
                
                # Validate structure
                if 'timestamp' not in event:
                    errors.append(f"Line {line_num}: Missing timestamp")
                
                if event_type == 'ir_pass_complete':
                    if 'pass_name' not in event:
                        errors.append(f"Line {line_num}: Missing pass_name")
                    if 'node_count' not in event:
                        errors.append(f"Line {line_num}: Missing node_count")
                
            except json.JSONDecodeError as e:
                errors.append(f"Line {line_num}: Invalid JSON - {e}")
    
    # Print results
    print(f"\n✅ Event Type Distribution:")
    for event_type, count in sorted(stats.items()):
        print(f"  {event_type:30s}: {count:6d}")
    
    print(f"\n📊 Total Events: {sum(stats.values())}")
    
    if errors:
        print(f"\n❌ Errors Found: {len(errors)}")
        for error in errors[:10]:  # Show first 10
            print(f"  {error}")
    else:
        print(f"\n✅ All events valid!")
    
    return len(errors) == 0

def main():
    corpus_dir = Path("ir_corpus_v2")
    
    if not corpus_dir.exists():
        print(f"❌ Corpus directory not found: {corpus_dir}")
        return 1
    
    print("🔱 ZCC IR Telemetry Corpus Validator")
    print("=" * 60)
    
    all_valid = True
    for jsonl_file in sorted(corpus_dir.glob("*.jsonl")):
        if not validate_corpus(jsonl_file):
            all_valid = False
    
    print("\n" + "=" * 60)
    if all_valid:
        print("✅ ALL CORPUS FILES VALID!")
        return 0
    else:
        print("❌ SOME FILES HAVE ERRORS")
        return 1

if __name__ == '__main__':
    sys.exit(main())
