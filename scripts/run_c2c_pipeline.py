#!/usr/bin/env python3
import sys
import os

# Fix Console Unicode Encoding
if sys.stdout.encoding.lower() != 'utf-8':
    try:
        sys.stdout.reconfigure(encoding='utf-8')
    except AttributeError:
        pass

from implementation.civilization.c2c_pipeline import C2CPipeline

def main():
    pipeline = C2CPipeline()
    res = pipeline.execute_c2c_run()
    print(f"C2C execution complete. Year: {res['year']}, Story: {res['story']['title']}, Director: {res['director_winner']}")

if __name__ == "__main__":
    main()
