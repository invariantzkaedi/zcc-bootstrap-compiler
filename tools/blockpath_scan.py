#!/usr/bin/env python3
import sys, io
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
import os
import subprocess
import sqlite3
import shutil
import json

EDGE_PROFILE = r"C:\Users\zkaed\AppData\Local\Microsoft\Edge\User Data\Default"
WORK_DIR = r"G:\zccMAIN\zcc\tools"
SW_CACHE_PATH = os.path.join(EDGE_PROFILE, "Service Worker", "CacheStorage")
SW_DATABASE_PATH = os.path.join(EDGE_PROFILE, "Service Worker", "Database")
SESSION_STORAGE_PATH = os.path.join(EDGE_PROFILE, "Session Storage")
LOCAL_STORAGE_PATH = os.path.join(EDGE_PROFILE, "Local Storage", "leveldb")
HISTORY_DB_PATH = os.path.join(EDGE_PROFILE, "History")

KEYWORDS = [b"blockpath", b"3CRKfd", b"bitcoin", b"addr?q="]

results = []

def find_in_binary(filepath, keywords):
    hits = []
    try:
        with open(filepath, "rb") as f:
            data = f.read()
        for kw in keywords:
            if kw.lower() in data.lower():
                # Extract surrounding context
                idx = data.lower().find(kw.lower())
                context = data[max(0, idx-80):idx+200]
                hits.append({
                    "file": filepath,
                    "keyword": kw.decode("utf-8", errors="replace"),
                    "context": context.decode("utf-8", errors="replace").replace("\x00", "")
                })
    except Exception:
        pass
    return hits

def scan_tree(root_path):
    for dirpath, dirnames, filenames in os.walk(root_path):
        for fname in filenames:
            fpath = os.path.join(dirpath, fname)
            hits = find_in_binary(fpath, KEYWORDS)
            results.extend(hits)

print("[*] Scanning Service Worker Cache for blockpath references...")
scan_tree(SW_CACHE_PATH)

print("[*] Scanning Service Worker Database...")
scan_tree(SW_DATABASE_PATH)

print("[*] Scanning Local Storage LevelDB...")
scan_tree(LOCAL_STORAGE_PATH)

print("[*] Scanning Session Storage...")
scan_tree(SESSION_STORAGE_PATH)

# History DB
print("[*] Scanning Edge History SQLite...")
temp_db = os.path.join(WORK_DIR, "temp_hist.sqlite")
try:
    shutil.copy2(HISTORY_DB_PATH, temp_db)
    conn = sqlite3.connect(temp_db)
    cur = conn.cursor()
    cur.execute("""
        SELECT datetime(v.visit_time / 1000000 - 11644473600, 'unixepoch', 'localtime'),
               u.url, u.title
        FROM visits v JOIN urls u ON v.url = u.id
        WHERE lower(u.url) LIKE '%blockpath%' OR lower(u.title) LIKE '%blockpath%'
           OR lower(u.url) LIKE '%3crkfd%'
        ORDER BY v.visit_time DESC
    """)
    rows = cur.fetchall()
    conn.close()
    os.remove(temp_db)
    for r in rows:
        results.append({"source": "history_db", "time": r[0], "url": r[1], "title": r[2]})
except Exception as e:
    print(f"    [-] History DB error: {e}")

# Report
print("\n" + "="*80)
print(f"[+] BLOCKPATH FORENSIC SCAN COMPLETE — {len(results)} references found")
print("="*80)

if not results:
    print("[!] No blockpath.com data found in any local Edge storage.")
else:
    for i, r in enumerate(results, 1):
        print(f"\n--- Hit #{i} ---")
        for k, v in r.items():
            val = str(v).replace("\n", " ").replace("\r", "").encode("utf-8", errors="replace").decode("utf-8", errors="replace")
            print(f"  {k}: {val[:300]}")

# Save JSON report
out_path = os.path.join(WORK_DIR, "blockpath_audit_report.json")
with open(out_path, "w", encoding="utf-8") as f:
    json.dump(results, f, indent=2, ensure_ascii=False)
print(f"\n[+] Full report saved to: {out_path}")
