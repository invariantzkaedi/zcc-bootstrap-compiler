#!/usr/bin/env python3
import sys
import os

# Fix Console Unicode Encoding
if sys.stdout.encoding.lower() != 'utf-8':
    try:
        sys.stdout.reconfigure(encoding='utf-8')
    except AttributeError:
        pass

from implementation.nexus.mission_console import LLMMissionConsole

def main():
    if len(sys.argv) < 2:
        print("Usage: python scripts/run_nexus_command.py \"<command>\"")
        sys.exit(1)
        
    cmd_str = sys.argv[1]
    console = LLMMissionConsole()
    res = console.parse_and_execute(cmd_str)
    
    print(f"Verdict: {res['status']}")
    print(f"Details: {res['description']}")

if __name__ == "__main__":
    main()
