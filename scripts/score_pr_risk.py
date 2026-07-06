#!/usr/bin/env python3
import json
import sqlite3
import sys
import fnmatch
from pathlib import Path

DB_PATH = Path("artifacts/qec_warehouse.db")
CODEOWNERS_PATH = Path(".github/CODEOWNERS")
OUT_PATH = Path("artifacts/pr_risk_score.json")

def parse_args():
    args = sys.argv[1:]
    changed_files = []
    pr_number = 0
    commit_sha = "dev"
    
    i = 0
    while i < len(args):
        if args[i] == "--changed-files" and i + 1 < len(args):
            # Might be space separated list or path to a text file containing paths
            list_or_file = args[i+1]
            if Path(list_or_file).exists():
                changed_files = [line.strip() for line in Path(list_or_file).read_text(encoding='utf-8').splitlines() if line.strip()]
            else:
                changed_files = [f.strip() for f in list_or_file.split(",") if f.strip()]
            i += 2
        elif args[i] == "--pr-number" and i + 1 < len(args):
            pr_number = int(args[i+1])
            i += 2
        elif args[i] == "--commit-sha" and i + 1 < len(args):
            commit_sha = args[i+1]
            i += 2
        else:
            if not args[i].startswith("--"):
                changed_files.append(args[i])
            i += 1
            
    return changed_files, pr_number, commit_sha

def parse_codeowners():
    reviewers_map = []
    if not CODEOWNERS_PATH.exists():
        return reviewers_map
        
    for line in CODEOWNERS_PATH.read_text(encoding='utf-8').splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split()
        if len(parts) >= 2:
            pattern = parts[0]
            owners = parts[1:]
            reviewers_map.append((pattern, owners))
            
    return reviewers_map

def get_reviewers(changed_files, reviewers_map):
    suggested = set()
    for f in changed_files:
        for pattern, owners in reviewers_map:
            # Normalize pattern matching simple globs
            clean_pattern = pattern.lstrip("/")
            if fnmatch.fnmatch(f, clean_pattern) or fnmatch.fnmatch(f, pattern):
                for o in owners:
                    suggested.add(o)
    return sorted(list(suggested))

def get_historical_failures_count(changed_files):
    if not DB_PATH.exists():
        return 0
        
    conn = sqlite3.connect(DB_PATH)
    cur = conn.cursor()
    
    count = 0
    # Search failure records matching names
    for f in changed_files:
        cur.execute("SELECT count(*) FROM failures WHERE artifact_path LIKE ?", (f"%{Path(f).name}%",))
        count += cur.fetchone()[0]
        
    conn.close()
    return count

def compute_risk(changed_files):
    score = 20 # Base risk
    reasons = []
    
    # 1. Path history
    hist_fails = get_historical_failures_count(changed_files)
    if hist_fails > 0:
        score += min(30, hist_fails * 5)
        reasons.append(f"Touched files with {hist_fails} historical failures recorded in warehouse.")
        
    # 2. Critical systems
    critical_touched = False
    for f in changed_files:
        if "zqec" in f or "part0" in f or "part3" in f:
            critical_touched = True
            break
            
    if critical_touched:
        score += 30
        reasons.append("Modifications to critical QEC algebraic/simulation modules.")
        
    # 3. CI workflows
    ci_touched = False
    for f in changed_files:
        if ".github/workflows" in f or "Makefile" in f:
            ci_touched = True
            break
            
    if ci_touched:
        score += 20
        reasons.append("Modifications to CI workflow orchestration/Make rules.")
        
    # Clamp
    score = max(0, min(100, score))
    level = "low" if score < 35 else "medium" if score < 70 else "high"
    
    if not reasons:
        reasons.append("No high-risk indicators matched; baseline risk applied.")
        
    return score, level, reasons

def score_pr():
    changed_files, pr_number, commit_sha = parse_args()
    if not changed_files:
        print("No changed files specified for PR risk scoring.")
        # Try local staged files if empty and not in CI
        changed_files = ["src/zqec.h"] # default fallback
        
    reviewers_map = parse_codeowners()
    suggested = get_reviewers(changed_files, reviewers_map)
    
    score, level, reasons = compute_risk(changed_files)
    
    report = {
        "pr_number": pr_number,
        "commit_sha": commit_sha,
        "score": score,
        "level": level,
        "reasons": reasons,
        "suggested_reviewers": suggested
    }
    
    # Persist in DB
    if DB_PATH.exists():
        conn = sqlite3.connect(DB_PATH)
        cur = conn.cursor()
        try:
            cur.execute("""
            INSERT INTO risk_scores (pr_number, commit_sha, score, level, reasons_json, suggested_reviewers_json)
            VALUES (?, ?, ?, ?, ?, ?)
            ON CONFLICT(pr_number, commit_sha) DO UPDATE SET
              score=excluded.score,
              level=excluded.level,
              reasons_json=excluded.reasons_json,
              suggested_reviewers_json=excluded.suggested_reviewers_json
            """, (pr_number, commit_sha, score, level, json.dumps(reasons), json.dumps(suggested)))
            conn.commit()
        except Exception as e:
            print(f"Failed to persist risk score to DB: {e}")
        finally:
            conn.close()
            
    OUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    OUT_PATH.write_text(json.dumps(report, indent=2, sort_keys=True), encoding='utf-8')
    print(f"Generated PR Risk Score: {OUT_PATH} (Score: {score}, Level: {level})")

if __name__ == "__main__":
    score_pr()
