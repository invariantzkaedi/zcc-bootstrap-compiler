#!/usr/bin/env python3
import json, os, subprocess, sys

OWNER = os.environ["OWNER"]
PROJECT_NUMBER = int(os.environ["PROJECT_NUMBER"])
NODE_ID = os.environ["NODE_ID"]
IS_PR = os.environ.get("IS_PR", "false") == "true"
TITLE = os.environ.get("TITLE", "")
LABELS_JSON = os.environ.get("LABELS_JSON", "[]")

labels = []
try:
    labels = [l["name"] for l in json.loads(LABELS_JSON)]
except Exception:
    pass

def gh_graphql(query, variables):
    p = subprocess.run(
        ["gh", "api", "graphql", "-f", f"query={query}", "-f", f"variables={json.dumps(variables)}"],
        check=True, capture_output=True, text=True
    )
    return json.loads(p.stdout)

q_project = """
query($owner:String!, $number:Int!) {
  user(login:$owner) { projectV2(number:$number){ id title fields(first:100){ nodes { ... on ProjectV2Field { id name } ... on ProjectV2SingleSelectField { id name options { id name } } } } } }
  organization(login:$owner) { projectV2(number:$number){ id title fields(first:100){ nodes { ... on ProjectV2Field { id name } ... on ProjectV2SingleSelectField { id name options { id name } } } } } }
}
"""
resp = gh_graphql(q_project, {"owner": OWNER, "number": PROJECT_NUMBER})
proj = (resp.get("data", {}).get("user", {}) or {}).get("projectV2")
if not proj:
    proj = (resp.get("data", {}).get("organization", {}) or {}).get("projectV2")
if not proj:
    print("Project not found"); sys.exit(1)

project_id = proj["id"]
fields = proj["fields"]["nodes"]

def field_by_name(name):
    for f in fields:
        if f.get("name") == name:
            return f
    return None

f_status = field_by_name("Status")
f_track = field_by_name("Track")
f_milestone = field_by_name("Milestone")
f_priority = field_by_name("Priority")
f_size = field_by_name("Size")
f_type = field_by_name("Type")

def option_id(single_select_field, option_name):
    if not single_select_field: return None
    for o in single_select_field.get("options", []):
        if o["name"].lower() == option_name.lower():
            return o["id"]
    return None

m_add = """
mutation($project:ID!, $content:ID!) {
  addProjectV2ItemById(input:{projectId:$project, contentId:$content}) {
    item { id }
  }
}
"""
add = gh_graphql(m_add, {"project": project_id, "content": NODE_ID})
item_id = add["data"]["addProjectV2ItemById"]["item"]["id"]

is_perf = "perf" in labels
is_ci = "ci" in labels
is_compiler = "compiler" in labels
is_optimizer = "optimizer" in labels
is_blocked = "blocked" in labels
is_meta = "meta" in labels

track = "Compiler Core"
if is_perf: track = "Performance"
elif is_ci: track = "Tooling/CI"
elif is_optimizer: track = "Optimizer"

priority = "P2"
if is_blocked: priority = "P0"
elif is_meta: priority = "P3"
elif ("verifier" in TITLE.lower() or "sccp" in TITLE.lower() or "instcombine" in TITLE.lower()):
    priority = "P1"

size = "M"
t = TITLE.lower()
if "meta" in t or "rollout" in t: size = "L"
if "docs" in t or "label" in t: size = "S"

itype = "PR" if IS_PR else "Issue"
status = "Todo"

ms = "M1"
if "M2" in TITLE: ms = "M2"
elif "M3" in TITLE: ms = "M3"
elif "M4" in TITLE: ms = "M4"

m_set = """
mutation($project:ID!, $item:ID!, $field:ID!, $opt:String!) {
  updateProjectV2ItemFieldValue(input:{
    projectId:$project,
    itemId:$item,
    fieldId:$field,
    value:{ singleSelectOptionId:$opt }
  }) { projectV2Item { id } }
}
"""

def set_single(field, value_name):
    if not field: return
    oid = option_id(field, value_name)
    if not oid: return
    gh_graphql(m_set, {"project": project_id, "item": item_id, "field": field["id"], "opt": oid})

set_single(f_status, status)
set_single(f_track, track)
set_single(f_milestone, ms)
set_single(f_priority, priority)
set_single(f_size, size)
set_single(f_type, itype)

print("ProjectV2 item routed:", item_id)
