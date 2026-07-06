#!/usr/bin/env python3
import json, os, subprocess, sys

OWNER=os.environ["OWNER"]
NUM=int(os.environ["PROJECT_NUMBER"])
NODE=os.environ["PR_NODE_ID"]
MERGED=os.environ.get("PR_MERGED","false")=="true"
STATE=os.environ.get("PR_STATE","open")

def gh_graphql(q, v):
    p=subprocess.run(["gh","api","graphql","-f",f"query={q}","-f",f"variables={json.dumps(v)}"],
                     check=True,capture_output=True,text=True)
    return json.loads(p.stdout)

q_proj="""
query($owner:String!, $num:Int!) {
  user(login:$owner){ projectV2(number:$num){ id fields(first:100){ nodes { ... on ProjectV2SingleSelectField { id name options { id name } } } } items(first:200){ nodes { id content { ... on PullRequest { id } } } } } }
  organization(login:$owner){ projectV2(number:$num){ id fields(first:100){ nodes { ... on ProjectV2SingleSelectField { id name options { id name } } } } items(first:200){ nodes { id content { ... on PullRequest { id } } } } } }
}
"""
r=gh_graphql(q_proj, {"owner":OWNER,"num":NUM})
p=(r["data"].get("user") or {}).get("projectV2") or (r["data"].get("organization") or {}).get("projectV2")
if not p: sys.exit("project not found")
pid=p["id"]

status_field=None
for f in p["fields"]["nodes"]:
    if f["name"]=="Status":
        status_field=f
        break
if not status_field: sys.exit(0)

item_id=None
for it in p["items"]["nodes"]:
    c=it.get("content")
    if c and c.get("id")==NODE:
        item_id=it["id"]
        break
if not item_id: sys.exit(0)

status="Review"
if STATE=="closed" and MERGED:
    status="Done"
elif STATE=="closed":
    status="Done"

opt=None
for o in status_field["options"]:
    if o["name"].lower()==status.lower():
        opt=o["id"]; break
if not opt: sys.exit(0)

m_set="""
mutation($project:ID!, $item:ID!, $field:ID!, $opt:String!) {
  updateProjectV2ItemFieldValue(input:{
    projectId:$project,itemId:$item,fieldId:$field,value:{singleSelectOptionId:$opt}
  }) { projectV2Item { id } }
}
"""
gh_graphql(m_set, {"project":pid,"item":item_id,"field":status_field["id"],"opt":opt})
print("Updated PR status to", status)
