import os
import subprocess
import itertools
import sys

ZCC = "/mnt/h/__DOWNLOADS/zcc_github_upload/zcc"
TARGET = "/tmp/fuzz_target.c"

bases = ["int", "void", "struct lua_State"]
prefixes = ["", "LUA_API", "static", "extern"]
pointers = ["", "*", "**", "* const *"]
parens = ["{name}", "({name})", "(({name}))", "((({name})))", "(*{name})", "(*({name}))", "(**{name})"]
postfixes = ["", "(int)", "(void*, ...)", "[5]", "[10][20]", "()"]

def generate_cases():
    cases = []
    cases_set = set() # drop dups
    for pre in prefixes:
        for base in bases:
            for ptr in pointers:
                for paren in parens:
                    for post in postfixes:
                        name = "fuzz_ident"
                        
                        decl = ""
                        if pre: decl += pre + " "
                        decl += base + " " + ptr + paren.format(name=name) + post + ";"
                        if decl not in cases_set:
                            cases_set.add(decl)
                            cases.append(decl)
                            
                        # Also generate one without a name, or with typedef
                        decl_anon = ""
                        if pre: decl_anon += pre + " "
                        decl_anon += base + " " + ptr + paren.format(name="") + post + ";"
                        if decl_anon not in cases_set:
                            cases_set.add(decl_anon)
                            cases.append(decl_anon)
    return cases

def run_fuzzer():
    cases = generate_cases()
    print(f"Generated {len(cases)} syntax cases.", flush=True)
    
    crashes = 0
    timeouts = 0
    valids = 0
    invalids = 0
    
    for i, case in enumerate(cases):
        with open(TARGET, "w") as f:
            f.write(f"#define LUA_API extern\nstruct lua_State;\n{case}\n")
        try:
            res = subprocess.run([ZCC, TARGET, "-o", "/tmp/fuzz_out.s"], stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=0.1)
            if res.returncode == 0:
                valids += 1
            elif res.returncode == 1:
                invalids += 1
            elif res.returncode == 139 or res.returncode < 0:
                print(f"CRASH ({res.returncode}) on: {case}")
                crashes += 1
            else:
                print(f"UNKNOWN ({res.returncode}) on: {case}")
                crashes += 1
        except subprocess.TimeoutExpired:
            print(f"TIMEOUT on: {case}")
            timeouts += 1
            
        if i > 0 and i % 500 == 0:
            print(f"Progress: {i}/{len(cases)}...", flush=True)
            
    print(f"\n--- FUZZ REPORT ---")
    print(f"Total Cases: {len(cases)}")
    print(f"Valid C (Exit 0): {valids}")
    print(f"Invalid C (Exit 1): {invalids}")
    print(f"Crashes: {crashes}")
    print(f"Timeouts: {timeouts}")
    
    if crashes == 0 and timeouts == 0:
        print("PARSER-001 FUZZ SUCCESS: 0% Crash Rate")
        sys.exit(0)
    else:
        print(f"PARSER-001 FUZZ FAILED: {crashes} crashes, {timeouts} timeouts")
        sys.exit(1)

if __name__ == "__main__":
    if not os.path.exists(ZCC):
        print(f"ZCC binary not found at {ZCC}")
        sys.exit(1)
    run_fuzzer()
