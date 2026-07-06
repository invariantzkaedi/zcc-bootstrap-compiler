#!/usr/bin/env python3
import argparse, json, subprocess

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
    ap.add_argument("--out", required=True)
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

    out = {
        "owner": args.owner,
        "project_number": args.project_number,
        "project_id": proj["id"],
        "project_title": proj["title"],
        "fields": {}
    }

    for f in proj["fields"]["nodes"]:
        name = f.get("name")
        if not name:
            continue
        out["fields"][name] = {"id": f["id"]}
        if "options" in f:
            out["fields"][name]["options"] = {o["name"]: o["id"] for o in f["options"]}

    with open(args.out, "w", encoding="utf-8") as fp:
        json.dump(out, fp, indent=2)

    print(f"Wrote {args.out}")

if __name__ == "__main__":
    main()
