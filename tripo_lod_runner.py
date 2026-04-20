#!/usr/bin/env python3
"""
==========================================================================
  tripo_lod_runner.py   v3   —   STS-token + S3 upload path for .glb
--------------------------------------------------------------------------
  CHANGES FROM v2:
    1. /upload/sts endpoint accepts ONLY webp/jpeg/png (code 2004 on .glb).
       Switched to the /upload/sts/token path used by the official SDK:
         POST /upload/sts/token {"format": "<ext>"}
         → returns AWS STS creds {sts_ak, sts_sk, session_token, s3_host,
           resource_bucket, resource_uri}
         → PUT the .glb to that S3 bucket via boto3
         → reference {bucket, key} in import_model
    2. STS creds are per-asset (each POST /upload/sts/token gives a
       single-object, short-lived credential). The SDK confirms this
       pattern.
    3. Format passed as "glb" in the JSON body. If Tripo rejects "glb",
       the fallback is to try "gltf" then "binary" (we probe once,
       then cache the working value for the session).
    4. boto3 is now a hard requirement (not a soft fallback).

  REQUIRES:  httpx  boto3     pip install httpx boto3
==========================================================================
"""
from __future__ import annotations
import os, sys, json, time, argparse, hashlib
from pathlib import Path
from typing import Optional
import httpx

# boto3 imported lazily — only required for live runs, not --dry-run
boto3 = None
BotoConfig = None
def _require_boto3():
    global boto3, BotoConfig
    if boto3 is not None: return
    try:
        import boto3 as _b
        from botocore.client import Config as _C
        boto3 = _b; BotoConfig = _C
    except ImportError:
        print("ERROR: boto3 required — pip install boto3", file=sys.stderr)
        sys.exit(2)

BASE = "https://api.tripo3d.ai/v2/openapi"
LOD_FACE_LIMITS = {1: 80_000, 2: 25_000, 3: 8_000}
COST_PER_LOD = 15
POLL_INTERVAL = 5
POLL_TIMEOUT_LOD = 900
POLL_TIMEOUT_IMPORT = 180

# Format values to try for /upload/sts/token in priority order.
# SDK uses "jpeg" for images; for .glb we don't know yet — probe and cache.
GLB_FORMAT_CANDIDATES = ["glb", "gltf", "binary", "model/gltf-binary"]


class TripoError(RuntimeError):
    def __init__(self, code, message, suggestion="", status=0):
        self.code, self.message, self.suggestion, self.status = code, message, suggestion, status
        super().__init__(f"code={code} status={status}: {message} — {suggestion}")

def _check(r: httpx.Response) -> dict:
    try:
        d = r.json()
    except Exception:
        raise TripoError(-1, f"non-JSON: {r.text[:200]}", "", r.status_code)
    if d.get("code") != 0:
        raise TripoError(d.get("code", -1), d.get("message", ""),
                          d.get("suggestion", ""), r.status_code)
    return d["data"]


class TripoClient:
    def __init__(self, api_key: str):
        self.c = httpx.Client(
            base_url=BASE,
            headers={"Authorization": f"Bearer {api_key}"},
            timeout=120,
        )
        self._working_format: Optional[str] = None  # cached after first success

    def balance(self) -> dict:
        return _check(self.c.get("/user/balance"))

    def get_sts_token(self, fmt: str) -> dict:
        """POST /upload/sts/token returning AWS-style STS credentials + bucket/key."""
        r = self.c.post("/upload/sts/token", json={"format": fmt})
        return _check(r)

    def upload_glb_via_s3(self, glb_path: Path, verbose: bool = False) -> dict:
        """Full STS-token + S3 upload flow. Returns {bucket, key} for import_model.

        Probes format values if the cached one fails.
        """
        # Use cached format if we have one
        formats_to_try = ([self._working_format] if self._working_format
                          else list(GLB_FORMAT_CANDIDATES))
        sts_data, used_fmt = None, None
        last_err = None
        for fmt in formats_to_try:
            try:
                if verbose:
                    print(f"      trying format={fmt!r} …", end="", flush=True)
                sts_data = self.get_sts_token(fmt)
                used_fmt = fmt
                if verbose: print(" ok")
                break
            except TripoError as e:
                last_err = e
                if verbose: print(f" rejected (code={e.code})")
                if e.code in (2004,):  # unsupported file type
                    continue
                # Any other code: don't keep probing blindly
                raise
        if sts_data is None:
            raise TripoError(-3, f"no format accepted by /upload/sts/token",
                              f"tried {formats_to_try}; last: {last_err}")
        self._working_format = used_fmt

        # Extract STS credentials
        ak    = sts_data["sts_ak"]
        sk    = sts_data["sts_sk"]
        stok  = sts_data["session_token"]
        host  = sts_data["s3_host"]
        bkt   = sts_data["resource_bucket"]
        key   = sts_data["resource_uri"]

        _require_boto3()
        endpoint = host if host.startswith("http") else f"https://{host}"
        s3 = boto3.client(
            "s3",
            endpoint_url=endpoint,
            aws_access_key_id=ak,
            aws_secret_access_key=sk,
            aws_session_token=stok,
            config=BotoConfig(signature_version="s3v4",
                              s3={"addressing_style": "virtual"}),
        )
        if verbose:
            print(f"      uploading to {bkt}:{key} via {endpoint} …", end="", flush=True)
        s3.upload_file(str(glb_path), bkt, key)
        if verbose: print(" ok")
        return {"bucket": bkt, "key": key}

    def import_model(self, obj: dict) -> str:
        """Dispatch import_model task, return its task_id."""
        body = {"type": "import_model",
                "file": {"object": {"bucket": obj["bucket"],
                                      "key":    obj["key"]}}}
        d = _check(self.c.post("/task", json=body))
        return d["task_id"]

    def create_lod_task(self, original_model_task_id: str, face_limit: int) -> str:
        body = {
            "type": "highpoly_to_lowpoly",
            "original_model_task_id": original_model_task_id,
            "face_limit": face_limit,
        }
        d = _check(self.c.post("/task", json=body))
        return d["task_id"]

    def poll(self, task_id: str, timeout: float, verbose: bool = False) -> dict:
        t0 = time.time()
        last = -1
        while time.time() - t0 < timeout:
            d = _check(self.c.get(f"/task/{task_id}"))
            status = d.get("status")
            progress = d.get("progress", 0)
            if verbose and progress != last:
                print(f"      …{status} {progress}% "
                      f"({int(time.time()-t0)}s)", flush=True)
                last = progress
            if status in ("success","failed","banned","expired","cancelled"):
                return d
            time.sleep(POLL_INTERVAL)
        raise TimeoutError(f"task {task_id} timeout after {timeout}s")

    def download(self, url: str, dest: Path):
        with self.c.stream("GET", url) as r:
            r.raise_for_status()
            with open(dest, "wb") as f:
                for chunk in r.iter_bytes(1 << 16):
                    f.write(chunk)


# ------------------------------------------------------------------- runner
def load_checkpoint(path: Path) -> dict:
    state = {"done_lod": set(), "imported": {}}
    if not path.exists(): return state
    for line in path.read_text(errors="ignore").splitlines():
        line = line.strip()
        if not line: continue
        try: e = json.loads(line)
        except: continue
        kind = e.get("kind")
        if kind == "lod" and e.get("status") == "success":
            state["done_lod"].add((e["asset"], e["lod"]))
        elif kind == "import" and e.get("status") == "success":
            state["imported"][e["asset"]] = e["task_id"]
    return state


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--fleet", required=True)
    ap.add_argument("--glb-dir", required=True)
    ap.add_argument("--task-id-map", default=None)
    ap.add_argument("--out", default="lod_out")
    ap.add_argument("--budget", type=int, default=600)
    ap.add_argument("--checkpoint", default="tripo_lod.jsonl")
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--verbose", action="store_true")
    ap.add_argument("--probe-only", action="store_true",
                    help="Upload + import one asset then exit — validates the "
                         "STS/S3/import_model path before committing the full run")
    args = ap.parse_args()

    fleet_path = Path(args.fleet)
    glb_dir    = Path(args.glb_dir)
    out_dir    = Path(args.out)
    ckpt_path  = Path(args.checkpoint)

    data = json.loads(fleet_path.read_text())
    plan = data.get("lod_plan_optimized_v2")
    if not plan:
        print("ERROR: fleet_ir.json missing lod_plan_optimized_v2", file=sys.stderr)
        sys.exit(2)

    picks = list(plan.get("guaranteed_picks", [])) + list(plan.get("greedy_picks", []))
    tid_map = json.loads(Path(args.task_id_map).read_text()) if args.task_id_map else {}

    asset_names = {p["asset"] for p in picks}
    need_upload = sorted(asset_names - set(tid_map.keys()))

    print(f"[plan] {len(picks)} LOD tasks, budget {args.budget}cr, "
          f"expected save {plan['total_vram_saved_mb']:.0f}MB")
    print(f"[plan] {len(tid_map)}/{len(asset_names)} assets have task_ids; "
          f"{len(need_upload)} need STS/S3 re-upload")

    if args.dry_run:
        print("\n[DRY-RUN] STS-token + S3 upload flow per missing asset:")
        for n in need_upload:
            src = glb_dir / f"{n}.glb"
            ex = "✓" if src.exists() else "✗ MISSING"
            size = f"{src.stat().st_size/1e6:.1f}MB" if src.exists() else ""
            print(f"  POST /upload/sts/token {{format:\"glb\"}}  →  boto3 upload_file({n}.glb {size})  →  import_model  {ex}")
        print("\n[DRY-RUN] LOD generation plan:")
        for i, p in enumerate(picks, 1):
            src = tid_map.get(p["asset"], "<will re-upload>")
            print(f"  {i:>3}. {p['asset']:<40} LOD{p['lod']} face={LOD_FACE_LIMITS[p['lod']]} "
                  f"{p['cost']}cr")
        print(f"\n[DRY-RUN] LOD spend: {sum(p['cost'] for p in picks)}cr")
        return

    api_key = os.environ.get("TRIPO_API_KEY", "").strip()
    if not api_key:
        print("ERROR: TRIPO_API_KEY unset", file=sys.stderr); sys.exit(2)

    out_dir.mkdir(parents=True, exist_ok=True)
    state = load_checkpoint(ckpt_path)
    print(f"[resume] {len(state['done_lod'])} LODs done, "
          f"{len(state['imported'])} imports cached")

    client = TripoClient(api_key)
    try:
        bal = client.balance()
        print(f"[balance] {bal}")
    except Exception as e:
        print(f"[balance] fetch failed: {e}")

    ckpt_f = open(ckpt_path, "a", buffering=1)
    def log(kind, asset, status, **kw):
        rec = {"kind":kind,"asset":asset,"status":status,"ts":time.time(), **kw}
        ckpt_f.write(json.dumps(rec) + "\n")

    # Probe mode: test one upload+import then exit
    if args.probe_only:
        if not need_upload:
            print("[probe] no upload needed — every asset already has a task_id")
            return
        asset = need_upload[0]
        src = glb_dir / f"{asset}.glb"
        print(f"[probe] testing full flow with {asset} ({src.stat().st_size/1e6:.1f}MB)")
        try:
            obj = client.upload_glb_via_s3(src, verbose=True)
            print(f"[probe] uploaded: {obj}")
            imp = client.import_model(obj)
            print(f"[probe] import_model task: {imp}")
            task = client.poll(imp, POLL_TIMEOUT_IMPORT, verbose=True)
            if task["status"] == "success":
                print(f"[probe] ✓ success — import path works, format={client._working_format!r}")
                print(f"[probe] You can now run WITHOUT --probe-only for the full batch")
                log("import", asset, "success", task_id=imp, probe=True)
            else:
                print(f"[probe] ✗ import failed: status={task['status']}")
                log("import", asset, task["status"], task_id=imp, probe=True)
        except Exception as e:
            print(f"[probe] FAIL: {type(e).__name__}: {e}")
            log("import", asset, "error", error=str(e), probe=True)
        ckpt_f.close()
        return

    spent = 0
    failures = []
    imported_cache = dict(state["imported"])

    # Phase A — STS/S3 upload + import_model for missing assets
    print(f"\n=== Phase A: STS/S3 upload + import_model ({len(need_upload)} assets) ===")
    for asset in need_upload:
        if asset in imported_cache:
            print(f"  [import:SKIP] {asset} (cached {imported_cache[asset][:16]}…)")
            continue
        src = glb_dir / f"{asset}.glb"
        if not src.exists():
            print(f"  [import:FAIL] {asset} — {src} not found")
            failures.append({"asset": asset, "phase": "upload", "error": "missing_file"})
            log("import", asset, "error", error="missing_file")
            continue

        print(f"  [upload] {asset} ({src.stat().st_size/1e6:.1f}MB)")
        try:
            obj = client.upload_glb_via_s3(src, verbose=args.verbose)
            imp_tid = client.import_model(obj)
            print(f"      import task: {imp_tid}")
            task = client.poll(imp_tid, POLL_TIMEOUT_IMPORT, verbose=args.verbose)
            if task["status"] == "success":
                imported_cache[asset] = imp_tid
                print(f"      ✓")
                log("import", asset, "success", task_id=imp_tid,
                    consumed=task.get("consumed_credit", 0),
                    bucket=obj["bucket"], key=obj["key"])
            else:
                print(f"      ✗ {task['status']}")
                failures.append({"asset": asset, "phase": "import",
                                  "task_id": imp_tid, "status": task["status"]})
                log("import", asset, task["status"], task_id=imp_tid)
        except TripoError as e:
            print(f"      TRIPO_ERR code={e.code}: {e.message}")
            failures.append({"asset": asset, "phase": "upload_or_import",
                              "code": e.code, "message": e.message})
            log("import", asset, "error", code=e.code, message=e.message)
        except Exception as e:
            print(f"      ERR: {type(e).__name__}: {e}")
            failures.append({"asset": asset, "phase": "upload_or_import",
                              "error": f"{type(e).__name__}:{e}"})
            log("import", asset, "error", error=str(e))

    # Phase B — LOD generation
    print(f"\n=== Phase B: LOD generation ({len(picks)} tasks, {args.budget}cr budget) ===")
    for i, pick in enumerate(picks, 1):
        key = (pick["asset"], pick["lod"])
        if key in state["done_lod"]:
            print(f"  [{i}/{len(picks)}] SKIP {pick['asset']} LOD{pick['lod']} (done)")
            continue
        if spent + pick["cost"] > args.budget:
            print(f"  [STOP] budget {args.budget}cr reached at step {i}")
            break

        src_tid = tid_map.get(pick["asset"]) or imported_cache.get(pick["asset"])
        if not src_tid:
            print(f"  [{i}/{len(picks)}] SKIP {pick['asset']} — no source task_id "
                  f"(Phase A failed for this asset)")
            failures.append({"asset": pick["asset"], "phase": "prereq",
                              "error": "no_source_task_id"})
            continue

        print(f"  [{i}/{len(picks)}] {pick['asset']} → LOD{pick['lod']} "
              f"(face={LOD_FACE_LIMITS[pick['lod']]})")
        try:
            lod_tid = client.create_lod_task(src_tid, LOD_FACE_LIMITS[pick["lod"]])
            print(f"      task: {lod_tid}")
            task = client.poll(lod_tid, POLL_TIMEOUT_LOD, verbose=args.verbose)
            if task["status"] == "success":
                out = task.get("output", {})
                url = out.get("pbr_model") or out.get("model")
                if url:
                    dest = out_dir / f"{pick['asset']}_LOD{pick['lod']}.glb"
                    client.download(url, dest)
                    print(f"      ✓ {dest.name} ({dest.stat().st_size/1e6:.1f}MB)")
                else:
                    print(f"      ✓ (no URL in output: {list(out.keys())})")
                spent += pick["cost"]
                log("lod", pick["asset"], "success", lod=pick["lod"],
                    task_id=lod_tid, cost=pick["cost"],
                    consumed=task.get("consumed_credit", 0))
            else:
                print(f"      ✗ {task['status']}")
                failures.append({"asset": pick["asset"], "lod": pick["lod"],
                                  "task_id": lod_tid, "status": task["status"]})
                log("lod", pick["asset"], task["status"], lod=pick["lod"],
                    task_id=lod_tid)
        except TripoError as e:
            print(f"      TRIPO_ERR code={e.code}: {e.message}")
            failures.append({"asset": pick["asset"], "lod": pick["lod"],
                              "code": e.code, "message": e.message})
            log("lod", pick["asset"], "error", lod=pick["lod"],
                code=e.code, message=e.message)
        except Exception as e:
            print(f"      ERR: {type(e).__name__}: {e}")
            failures.append({"asset": pick["asset"], "lod": pick["lod"],
                              "error": f"{type(e).__name__}:{e}"})
            log("lod", pick["asset"], "error", lod=pick["lod"], error=str(e))

    ckpt_f.close()

    summary = {
        "spent_credits": spent,
        "budget": args.budget,
        "imported_assets": len(imported_cache),
        "lod_tasks_attempted": len(picks),
        "lod_tasks_succeeded": sum(1 for p in picks
                                     if (p["asset"], p["lod"]) not in state["done_lod"]
                                     and not any(f.get("asset")==p["asset"]
                                                 and f.get("lod")==p["lod"]
                                                 for f in failures)),
        "failures": failures,
        "format_accepted_by_sts_token": client._working_format,
        "checkpoint": str(ckpt_path),
        "output_dir": str(out_dir),
        "timestamp": time.time(),
    }
    Path("run_summary.json").write_text(json.dumps(summary, indent=2))
    print(f"\n[done] {spent}cr spent / {args.budget}cr budget")
    print(f"[done] imported {len(imported_cache)} assets via STS/S3")
    print(f"[done] {len(failures)} failures")
    print(f"[done] run_summary.json written")


if __name__ == "__main__":
    main()