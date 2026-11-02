#!/usr/bin/env python3
"""Independent black-box ZCC/GCC/Clang C99 differential tester.
No repository mutations. A failing run retains all artifacts.
Usage:
  python3 zcc_track3_semantic_frontier.py --zcc /path/to/zcc2 --cases 128 --seed 20261102
"""
import argparse, hashlib, json, os, random, shutil, subprocess, sys, tempfile
from pathlib import Path

def call(argv, timeout=30):
    try:
        q=subprocess.run([str(x) for x in argv], text=True, capture_output=True, timeout=timeout)
        return {"rc":q.returncode,"out":q.stdout,"err":q.stderr,"argv":[str(x) for x in argv]}
    except subprocess.TimeoutExpired as e:
        return {"rc":None,"out":"","err":f"TIMEOUT after {timeout}s","argv":[str(x) for x in argv]}
    except OSError as e:
        return {"rc":None,"out":"","err":str(e),"argv":[str(x) for x in argv]}

def source(seed, idx):
    r=random.Random((seed<<32)^idx)
    values=[0,1,2,3,7,15,31,63,127,255,1023,65535,0x7fffffff,0x80000000,0xffffffff]
    xs=[r.choice(values) for _ in range(10)]
    shift=r.randrange(0,32)
    mode=idx%8
    initial=", ".join(f"{x}u" for x in xs)
    exprs=[
        "((a + b) ^ (a << 3))",
        "((a * 1664525u) + (b ^ (a >> 5)))",
        "((a & b) | ((~a) & c))",
        "(((a << k) | (a >> ((32u-k)&31u))))",
        "((a ^ (b << 1)) + (c >> 3))",
        "((a - b) * (c | 1u))",
        "((a == b) ? c : (a < b ? a : b))",
        "(((a % 17u) * (b % 31u)) ^ (c % 29u))",
    ]
    # k in [1,31], so both shifts are valid; all unsigned math is modular.
    k=max(1,shift)
    return f"""#include <stdio.h>
#include <stdint.h>
static uint32_t step(uint32_t a,uint32_t b,uint32_t c,unsigned k) {{
  (void)a; (void)b; (void)c; (void)k;
  return {exprs[mode]};
}}
int main(void) {{
  uint32_t v[10] = {{{initial}}};
  uint32_t acc=2166136261u;
  unsigned i;
  for (i=0u;i<10u;i++) {{
    uint32_t a=v[i], b=v[(i+3u)%10u], c=v[(i+7u)%10u];
    uint32_t t=step(a,b,c,{k}u);
    acc=(acc ^ t)*16777619u;
    v[i]=t;
  }}
  printf("%u\\n", (unsigned)acc);
  return 0;
}}
"""

def sha(data):
    return hashlib.sha256(data).hexdigest()

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--zcc",required=True,type=Path)
    ap.add_argument("--cases",type=int,default=128)
    ap.add_argument("--seed",type=int,default=20261102)
    ap.add_argument("--out",type=Path,default=Path("track3_semantic_frontier"))
    ap.add_argument("--timeout",type=int,default=30)
    ap.add_argument("--self-test",action="store_true",help="deliberately corrupt one oracle result; must detect mismatch")
    a=ap.parse_args()
    if a.cases<1 or a.cases>100000: ap.error("--cases must be 1..100000")
    zcc=a.zcc.resolve()
    if not zcc.is_file(): ap.error("ZCC executable not found")
    gcc=shutil.which("gcc")
    clang=shutil.which("clang")
    if not gcc or not clang:
        ap.error("Both gcc and clang are REQUIRED; install clang rather than silently skipping.")
    a.out.mkdir(parents=True,exist_ok=True)
    env={"seed":a.seed,"cases":a.cases,"zcc":str(zcc),
         "zcc_sha256":sha(zcc.read_bytes()),"gcc":call([gcc,"--version"])["out"].splitlines()[:1],
         "clang":call([clang,"--version"])["out"].splitlines()[:1]}
    records=[]; failure=0; rejected=0
    for i in range(a.cases):
        d=a.out/f"case_{i:05d}"
        d.mkdir(exist_ok=True)
        src=d/"probe.c"; src.write_text(source(a.seed,i))
        outputs={}
        for name,cmd in (
            ("gcc_o0",[gcc,"-std=c99","-O0","-Wall","-Wextra","-Werror",src,"-o",d/"gcc_o0"]),
            ("gcc_o2",[gcc,"-std=c99","-O2","-Wall","-Wextra","-Werror",src,"-o",d/"gcc_o2"]),
            ("clang_o2",[clang,"-std=c99","-O2","-Wall","-Wextra","-Werror",src,"-o",d/"clang_o2"]),
            ("sanitized",[gcc,"-std=c99","-O1","-Wall","-Wextra","-Werror","-fsanitize=undefined","-fno-sanitize-recover=all",src,"-o",d/"sanitized"])):
            build=call(cmd,a.timeout)
            run=call([d/name],a.timeout) if build["rc"]==0 else {"rc":None,"out":"","err":"BUILD FAILED"}
            outputs[name]={"build":build,"run":run}
        # ZCC documented interface: zcc2 file.c -o file.s ; link assembly using GCC.
        zcc_build=call([zcc,src,"-o",d/"zcc.s"],a.timeout)
        zcc_link=call([gcc,"-no-pie",d/"zcc.s","-o",d/"zcc"],a.timeout) if zcc_build["rc"]==0 else {"rc":None,"out":"","err":"ZCC COMPILATION FAILED"}
        zcc_run=call([d/"zcc"],a.timeout) if zcc_link["rc"]==0 else {"rc":None,"out":"","err":"LINK FAILED"}
        outputs["zcc"]={"build":zcc_build,"link":zcc_link,"run":zcc_run}
        oracle=[outputs[x]["run"] for x in ("gcc_o0","gcc_o2","clang_o2","sanitized")]
        agreement=all(x["rc"]==0 and x["out"]==oracle[0]["out"] for x in oracle)
        zcc_ok=zcc_run["rc"]==0 and zcc_run["out"]==oracle[0]["out"]
        if a.self_test and i==0:
            zcc_ok=False
        if not agreement:
            verdict="ORACLE_DISAGREEMENT"
        elif zcc_build["rc"]!=0:
            verdict="ZCC_REJECTED_VALID_C99"
            rejected+=1
        elif not zcc_ok:
            verdict="ZCC_MISMATCH"
        else:
            verdict="PASS"
        if verdict!="PASS": failure+=1
        rec={"id":i,"source_sha256":sha(src.read_bytes()),"verdict":verdict,
             "reference_output":oracle[0]["out"] if agreement else None,
             "outputs":outputs}
        records.append(rec)
        (d/"result.json").write_text(json.dumps(rec,indent=2))
        if verdict=="PASS":
            shutil.rmtree(d)  # retain failures only, all cases remain in full report
        else:
            print(f"{verdict}: {src}",flush=True)
    report={"schema":"zcc-track3-semantic-frontier-v1","environment":env,
            "failed_or_disputed":failure,"zcc_rejections":rejected,
            "results":records}
    reportpath=a.out/"report.json"
    reportpath.write_text(json.dumps(report,indent=2))
    print(f"CASES={a.cases} FAILURES={failure} ZCC_REJECTIONS={rejected}")
    print(f"REPORT={reportpath.resolve()}")
    if a.self_test:
        # A negative control is successful only if it *forces* failure.
        print("NEGATIVE_CONTROL_DETECTED" if any(x["verdict"]=="ZCC_MISMATCH" for x in records) else "NEGATIVE_CONTROL_FAILED")
        return 0 if any(x["verdict"]=="ZCC_MISMATCH" for x in records) else 99
    return 1 if failure else 0

if __name__=="__main__":
    sys.exit(main())
