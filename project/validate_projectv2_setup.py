#!/usr/bin/env python3
import argparse, json, subprocess, sys

REQ_FIELDS = {
    "Status": ["Todo", "In Progress", "Review", "Blocked", "Done"],
    "Track": ["Compiler Core", "Optimizer", "Tooling/CI", "Performance", "Release/Ops"],
    "Milestone": ["M1", "M2", "M3", "M4"],
    "Priority": ["P0", "P1", "P2", "P3"],
    "Size": ["XS", "S", "M", "L", "XL"],
    "Type": ["Issue", "PR"],
}

def gh_graphql(query, variables):
    p = subprocess.run(
        ["gh", "api", "graphql", "-f", f"query={query}", "-f", f"variables={json.dumps(variables)}"],
        check=True, capture_output=True, text=True
    )
    return json.loads(p.stdout)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--owner", required=True)
    ap.add_argument("--project-number", type=int, required=True)
    args = ap.parse_args()

    q = """
query($owner:String!, $number:Int!) {
  user(login:$owner) {
    projectV2(number:$number) {
      id
      title
      fields(first:100) {
        nodes {
          ... on ProjectV2Field { id name }
          ... on ProjectV2SingleSelectField { id name options { id name } }
        }
      }
    }
  }
  organization(login:$owner) {
    projectV2(number:$number) {
      id
      title
      fields(first:100) {
        nodes {
          ... on ProjectV2Field { id name }
          ... on ProjectV2SingleSelectField { id name options { id name } }
        }
      }
    }
  }
}
"""
    r = gh_graphql(q, {"owner": args.owner, "number": args.project_number})
    proj = ((r.get("data", {}).get("user") or {}).get("projectV2") or
            (r.get("data", {}).get("organization") or {}).get("projectV2"))
    if not proj:
        print("ERROR: ProjectV2 not found.", file=sys.stderr)
        sys.exit(2)

    fields = {f.get("name"): f for f in proj["fields"]["nodes"] if f.get("name")}
    missing = []
    bad_options = []

    for fname, expected_opts in REQ_FIELDS.items():
        f = fields.get(fname)
        if not f:
            missing.append(fname)
            continue
        opts = [o["name"] for o in f.get("options", [])] if "options" in f else []
        for eo in expected_opts:
            if eo not in opts:
                bad_options.append((fname, eo))

    if missing or bad_options:
        print("ProjectV2 validation failed.")
        if missing:
            print("Missing fields:", ", ".join(missing))
        if bad_options:
            for f,o in bad_options:
                print(f"Missing option in field '{f}': {o}")
        sys.exit(1)

    print(f"ProjectV2 '{proj['title']}' validation OK.")
    print("All required fields/options exist.")

if __name__ == "__main__":
    main()
