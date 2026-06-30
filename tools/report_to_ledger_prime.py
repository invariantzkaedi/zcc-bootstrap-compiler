#!/usr/bin/env python3
import re,sys,json,hashlib,argparse
from pathlib import Path
from datetime import datetime,timezone
def score_finding(text):
    t=text.lower()
    ho=bool(re.search(r"(fold|eliminat|remov|simplif|dead|redundant|peephole|dce|gvn)",t))
    hd=bool(re.search(r"(dead code|unused|unreachable|remove these)",t))
    hc=bool(re.search(r"(bug|error|incorrect|uninitialized|overflow|invalid|null)",t))
    hl=bool(re.search(r"(line \d+|%t\w+|%stack_|imm=|load i|br_if|const i)",text))
    iv=bool(re.search(r"(may|might|possible|unclear|further inspection)",t))
    s=int(ho)*3+int(hd)*2+int(hc)*2+int(hl)*2-int(iv)
    return s,("LEGENDARY" if s>=7 else "EPIC" if s>=5 else "RARE" if s>=3 else "COMMON")
def parse_report(text):
    entries=[]
    for sec in re.split(r"\n## ", "\n"+text):
        if not sec.strip(): continue
        lines=sec.strip().splitlines()
        m=re.match(r"^(.+?)\s+\((\d+)\s+nodes?\)",lines[0].strip())
        if not m: continue
        content="\n".join(lines[1:]).strip("\n- ")
        while content.endswith("---"): content=content[:-3].strip()
        status="TIMEOUT" if "**TIMEOUT**" in content else "ERROR" if "**ERROR**" in content else "EMPTY" if not content else "OK"
        entries.append({"name":m.group(1).strip(),"line_count":int(m.group(2)),"content":content,"status":status})
    return entries
def main():
    p=argparse.ArgumentParser()
    p.add_argument("--report",default="report.md")
    p.add_argument("--ledger",default="corpus/ir_analysis_ledger.jsonl")
    p.add_argument("--min-tier",default="RARE",choices=["LEGENDARY","EPIC","RARE","COMMON"])
    p.add_argument("-v","--verbose",action="store_true")
    a=p.parse_args()
    TO={"LEGENDARY":4,"EPIC":3,"RARE":2,"COMMON":1}
    rp=Path(a.report); lp=Path(a.ledger)
    if not rp.exists(): print(f"ERROR: {rp}",file=sys.stderr); sys.exit(1)
    entries=parse_report(rp.read_text(encoding="utf-8",errors="ignore"))
    lp.parent.mkdir(parents=True,exist_ok=True)
    ledger={}
    if lp.exists():
        for line in lp.read_text().splitlines():
            try: e=json.loads(line); ledger[e["name"]]=e
            except: pass
    now=datetime.now(timezone.utc).isoformat()
    ing=skip_s=skip_t=skip_d=0
    print(f"Parsed {len(entries)} sections from {rp}")
    for e in entries:
        name,status,lc,content=e["name"],e["status"],e["line_count"],e["content"]
        if status!="OK":
            skip_s+=1
            if a.verbose: print(f"  SKIP  {name:<45} [{status}]")
            continue
        score,tier=score_finding(content)
        if TO[tier]<TO[a.min_tier]:
            skip_t+=1
            if a.verbose: print(f"  SKIP  {name:<45} [tier={tier} score={score}]")
            continue
        ch=hashlib.sha256(content.encode()).hexdigest()[:16]
        if name in ledger and ledger[name].get("content_hash")==ch:
            skip_d+=1
            if a.verbose: print(f"  DUP   {name:<45}")
            continue
        rec={"name":name,"line_count":lc,"tier":tier,"score":score,"content_hash":ch,"ingested_at":now,"raw_analysis":content[:500]}
        with open(lp,"a",encoding="utf-8") as f: f.write(json.dumps(rec)+"\n")
        ledger[name]=rec; ing+=1
        if a.verbose: print(f"  PASS  {name:<45} [tier={tier} score={score}]")
    print(f"\n{'='*52}")
    print(f"  Ingested: {ing}  Timeout: {skip_s}  Low-tier: {skip_t}  Dup: {skip_d}")
    tc={}
    for e in ledger.values(): tc[e.get("tier","COMMON")]=tc.get(e.get("tier","COMMON"),0)+1
    for t in ["LEGENDARY","EPIC","RARE","COMMON"]:
        if t in tc: print(f"  {t:10s} {tc[t]:4d}  {'#'*min(tc[t],30)}")
    print(f"{'='*52}")
if __name__=="__main__": main()
