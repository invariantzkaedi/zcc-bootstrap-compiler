#!/usr/bin/env python3
"""
ZCC v17.7 Track 2 — Canonical Immutable Generation Release Consumer
=============================================================================
Enforces the Release Consumer Consistency Contract:
  1. Resolves one immutable generation identifier ONCE from `.releases/ACTIVE_GENERATION`.
  2. Acquires an OS directory file descriptor (`O_RDONLY | O_DIRECTORY`) on
     `.releases/rel-<generation_id>` and registers an active reader lease pin.
  3. Authenticates `release_manifest.json` via `openat(dir_fd, ...)` against
     `ACTIVE_GENERATION["manifest_sha256"]`.
  4. Opens all four release artifacts (`zcc2.s`, `zcc3.s`, `zcc2`, `zcc3`) strictly
     through the pinned `dir_fd` (`openat`), verifying every artifact's physical
     SHA-256 against `release_manifest.json` even if concurrent publishers switch
     `.releases/ACTIVE_GENERATION` and `.releases/current` between opens.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
import threading
import time
import uuid
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Dict, List, Optional

sys.stdout.reconfigure(encoding="utf-8")

EXIT_OK = 0
EXIT_CONSUMER_INTEGRITY_FAIL = 74
REQUIRED_ARTIFACTS = ("zcc2.s", "zcc3.s", "zcc2", "zcc3")


class ReleaseConsumerIntegrityError(RuntimeError):
    """Raised when a release generation fails manifest or artifact integrity checks."""


@dataclass
class ConsumedReleaseGeneration:
    generation_id: str
    release_dir: str
    git_commit: str
    manifest_sha256: str
    artifact_digests: Dict[str, str]
    artifact_sizes: Dict[str, int]
    enoent_retries: int


def _fsync_dir(dir_path: Path) -> None:
    try:
        dfd = os.open(str(dir_path), os.O_RDONLY)
        try:
            os.fsync(dfd)
        finally:
            os.close(dfd)
    except OSError:
        pass


def _read_bytes_via_dirfd(dir_fd: int, filename: str) -> bytes:
    flags = os.O_RDONLY
    if hasattr(os, "O_NOFOLLOW"):
        flags |= os.O_NOFOLLOW
    fd = os.open(filename, flags, dir_fd=dir_fd)
    try:
        st = os.fstat(fd)
        if not os.path.stat.S_ISREG(st.st_mode):
            raise ReleaseConsumerIntegrityError(f"Artifact '{filename}' is not a regular file")
        chunks: List[bytes] = []
        while True:
            buf = os.read(fd, 1024 * 1024)
            if not buf:
                break
            chunks.append(buf)
        return b"".join(chunks)
    finally:
        os.close(fd)


class ZCCReleaseConsumer:
    """Canonical multi-artifact release consumer using once-resolved generation directories."""

    def __init__(self, repo_root: Path):
        self.repo_root = Path(repo_root).resolve()
        self.releases_dir = self.repo_root / ".releases"
        self.pins_dir = self.releases_dir / "pins"

    def _read_active_pointer_once(self) -> tuple[Dict[str, object], int]:
        ag_path = self.releases_dir / "ACTIVE_GENERATION"
        enoent_retries = 0
        last_err: Optional[Exception] = None
        for _ in range(16):
            try:
                raw = ag_path.read_text(encoding="utf-8")
                if raw.strip():
                    data = json.loads(raw)
                    if isinstance(data, dict) and "generation_id" in data and "release_dir" in data:
                        return data, enoent_retries
            except (FileNotFoundError, json.JSONDecodeError) as exc:
                last_err = exc
                enoent_retries += 1
                time.sleep(0.001)
        raise ReleaseConsumerIntegrityError(
            f"Unable to resolve .releases/ACTIVE_GENERATION at {ag_path}: {last_err}"
        )

    def consume_generation(
        self,
        pause_between_opens_sec: float = 0.0,
        after_second_open_hook: Optional[Callable[[], None]] = None,
    ) -> ConsumedReleaseGeneration:
        # 1. Resolve immutable generation identifier ONCE per transaction
        pointer, retries = self._read_active_pointer_once()
        gen_id = str(pointer["generation_id"]).strip()
        rel_subpath = str(pointer["release_dir"]).strip()
        expected_manifest_sha = str(pointer.get("manifest_sha256", "")).strip().lower()

        gen_dir = (self.repo_root / rel_subpath).resolve()
        if not gen_dir.is_dir():
            raise ReleaseConsumerIntegrityError(
                f"Resolved generation directory does not exist: {gen_dir}"
            )

        # 2. Register reader pin lease so garbage collection cannot prune this generation mid-read
        self.pins_dir.mkdir(parents=True, exist_ok=True)
        pin_file = self.pins_dir / f"pin-{gen_id}-{os.getpid()}-{uuid.uuid4().hex[:8]}.json"
        pin_file.write_text(
            json.dumps({"generation_id": gen_id, "pid": os.getpid(), "ts": time.time()}) + "\n",
            encoding="utf-8",
        )

        # 3. Acquire OS kernel directory descriptor (`O_RDONLY | O_DIRECTORY`) on the immutable generation dir
        dir_flags = os.O_RDONLY
        if hasattr(os, "O_DIRECTORY"):
            dir_flags |= os.O_DIRECTORY
        if hasattr(os, "O_NOFOLLOW"):
            dir_flags |= os.O_NOFOLLOW

        dir_fd = os.open(str(gen_dir), dir_flags)
        try:
            # 4. Read and authenticate release_manifest.json via openat(dir_fd, ...)
            manifest_bytes = _read_bytes_via_dirfd(dir_fd, "release_manifest.json")
            actual_manifest_sha = hashlib.sha256(manifest_bytes).hexdigest()
            if expected_manifest_sha and actual_manifest_sha != expected_manifest_sha:
                raise ReleaseConsumerIntegrityError(
                    f"Manifest SHA-256 mismatch for generation '{gen_id}': "
                    f"pointer expected {expected_manifest_sha}, got {actual_manifest_sha}"
                )

            manifest = json.loads(manifest_bytes.decode("utf-8"))
            if manifest.get("generation_id") != gen_id:
                raise ReleaseConsumerIntegrityError(
                    f"Manifest generation_id '{manifest.get('generation_id')}' != pointer '{gen_id}'"
                )

            expected_artifacts = manifest.get("artifacts", {})
            observed_digests: Dict[str, str] = {}
            observed_sizes: Dict[str, int] = {}

            # 5. Open each required artifact strictly via `openat(dir_fd, ...)`
            for idx, art_name in enumerate(REQUIRED_ARTIFACTS):
                art_bytes = _read_bytes_via_dirfd(dir_fd, art_name)
                art_sha = hashlib.sha256(art_bytes).hexdigest()
                exp_sha = str(expected_artifacts.get(art_name, "")).strip().lower()
                if not exp_sha or art_sha != exp_sha:
                    raise ReleaseConsumerIntegrityError(
                        f"Artifact '{art_name}' in generation '{gen_id}' failed manifest digest check: "
                        f"expected {exp_sha}, got {art_sha}"
                    )
                observed_digests[art_name] = art_sha
                observed_sizes[art_name] = len(art_bytes)

                if idx == 1 and after_second_open_hook is not None:
                    after_second_open_hook()
                if pause_between_opens_sec > 0.0 and idx < len(REQUIRED_ARTIFACTS) - 1:
                    time.sleep(pause_between_opens_sec)

            # 6. Enforce Stage 2 == Stage 3 convergence within the consumed generation
            if observed_digests["zcc2.s"] != observed_digests["zcc3.s"]:
                raise ReleaseConsumerIntegrityError(
                    f"Consumed generation '{gen_id}' has divergent zcc2.s vs zcc3.s"
                )
            if observed_digests["zcc2"] != observed_digests["zcc3"]:
                raise ReleaseConsumerIntegrityError(
                    f"Consumed generation '{gen_id}' has divergent zcc2 vs zcc3"
                )

            return ConsumedReleaseGeneration(
                generation_id=gen_id,
                release_dir=rel_subpath,
                git_commit=str(manifest.get("git_commit", "UNKNOWN")),
                manifest_sha256=actual_manifest_sha,
                artifact_digests=observed_digests,
                artifact_sizes=observed_sizes,
                enoent_retries=retries,
            )
        finally:
            os.close(dir_fd)
            try:
                pin_file.unlink(missing_ok=True)
            except OSError:
                pass


def prune_old_generations(repo_root: Path, keep_recent: int = 2) -> Dict[str, List[str]]:
    """Garbage-collects superseded `.releases/rel-*` directories while honoring active reader pins."""
    releases_dir = Path(repo_root).resolve() / ".releases"
    pins_dir = releases_dir / "pins"
    active_gen = None
    ag_path = releases_dir / "ACTIVE_GENERATION"
    if ag_path.is_file():
        try:
            active_gen = json.loads(ag_path.read_text(encoding="utf-8")).get("generation_id")
        except Exception:
            pass

    pinned_gens = set()
    if pins_dir.is_dir():
        for pf in pins_dir.glob("pin-*.json"):
            try:
                pdata = json.loads(pf.read_text(encoding="utf-8"))
                if "generation_id" in pdata:
                    pinned_gens.add(str(pdata["generation_id"]))
            except Exception:
                pass

    rel_dirs = sorted(
        [d for d in releases_dir.glob("rel-*") if d.is_dir() and not d.name.endswith(".staging")],
        key=lambda p: p.stat().st_mtime,
        reverse=True,
    )
    kept: List[str] = []
    pruned: List[str] = []
    for idx, d in enumerate(rel_dirs):
        gen_id = d.name[len("rel-"):]
        if gen_id == active_gen or gen_id in pinned_gens or idx < keep_recent:
            kept.append(gen_id)
        else:
            shutil.rmtree(d, ignore_errors=True)
            pruned.append(gen_id)
    _fsync_dir(releases_dir)
    return {"kept": kept, "pruned": pruned, "pinned": sorted(pinned_gens)}


def _populate_stage_fixture(stage_dir: Path, tag: str) -> Dict[str, str]:
    stage_dir.mkdir(parents=True, exist_ok=True)
    asm = (f"/* ZCC TRACK2 CONSUMER ASM {tag} */\n.globl main\nmain:\n  xorl %eax, %eax\n  ret\n" * 64).encode("utf-8")
    bin_b = b"\x7fELF" + (f"ZCC_TRACK2_BIN_{tag}_".encode("utf-8") * 128)
    obj_b = b"\x7fELF" + (f"ZCC_TRACK2_OBJ_{tag}_".encode("utf-8") * 64)
    (stage_dir / "stage2.s").write_bytes(asm)
    (stage_dir / "stage3.s").write_bytes(asm)
    (stage_dir / "stage2.bin").write_bytes(bin_b)
    (stage_dir / "stage3.bin").write_bytes(bin_b)
    (stage_dir / "stage2.bin.norm").write_bytes(bin_b)
    (stage_dir / "stage3.bin.norm").write_bytes(bin_b)
    (stage_dir / "stage2.o").write_bytes(obj_b)
    (stage_dir / "stage3.o").write_bytes(obj_b)
    (stage_dir / "provenance.jsonl").write_text(json.dumps({"stage": 2, "tag": tag}) + "\n", encoding="utf-8")
    (stage_dir / "toolchain.json").write_text(json.dumps({"gcc": "13.3.0"}) + "\n", encoding="utf-8")
    return {
        "asm": hashlib.sha256(asm).hexdigest(),
        "bin": hashlib.sha256(bin_b).hexdigest(),
    }


def run_self_test(repo_root: Path, json_out: Optional[Path] = None) -> int:
    gate_sh = repo_root / "gate.sh"
    tmp_root = Path(tempfile.mkdtemp(prefix="zcc_consumer_gauntlet_", dir="/tmp"))
    drvfs_sandbox = repo_root / ".releases" / f"consumer_sandbox_{os.getpid()}"
    checks: List[Dict[str, object]] = []

    try:
        ext4_repo = tmp_root / "ext4_repo"
        ext4_repo.mkdir(parents=True, exist_ok=True)
        drvfs_sandbox.mkdir(parents=True, exist_ok=True)
        evid_dir = tmp_root / "evid"
        evid_dir.mkdir(parents=True, exist_ok=True)

        stages: Dict[str, Path] = {}
        digests: Dict[str, Dict[str, str]] = {}
        for ver in ("gen-v1", "gen-v2", "gen-v3"):
            sdir = tmp_root / f"stage_{ver}"
            digests[ver] = _populate_stage_fixture(sdir, ver)
            stages[ver] = sdir

        def publish_to(target_repo: Path, ver: str) -> None:
            env = os.environ.copy()
            env["ZCC_KNOWN_HEAD"] = "d6a31bdc2cf8a1c87a49ff13c53ead56e0d37ffa"
            subprocess.run(
                ["bash", str(gate_sh), "--test-publish-tx", str(target_repo), str(stages[ver]), str(evid_dir), ver],
                env=env,
                check=True,
                capture_output=True,
                text=True,
            )

        # Publish gen-v1 on ext4_repo
        publish_to(ext4_repo, "gen-v1")

        # ------------------------------------------------------------------
        # CONS-01: Naive Multi-Open .releases/current Symlink Reader under Pause
        # Proves why separate opens through .releases/current can tear!
        # ------------------------------------------------------------------
        cur_symlink = ext4_repo / ".releases" / "current"
        naive_f1 = hashlib.sha256((cur_symlink / "zcc2.s").read_bytes()).hexdigest()
        naive_f2 = hashlib.sha256((cur_symlink / "zcc3.s").read_bytes()).hexdigest()
        # Publisher switches generation v1 -> v2 -> v3 while naive reader is paused after file 2
        publish_to(ext4_repo, "gen-v2")
        publish_to(ext4_repo, "gen-v3")
        naive_f3 = hashlib.sha256((cur_symlink / "zcc2").read_bytes()).hexdigest()
        naive_f4 = hashlib.sha256((cur_symlink / "zcc3").read_bytes()).hexdigest()

        naive_mixed_detected = (
            naive_f1 == digests["gen-v1"]["asm"]
            and naive_f2 == digests["gen-v1"]["asm"]
            and naive_f3 == digests["gen-v3"]["bin"]
            and naive_f4 == digests["gen-v3"]["bin"]
        )
        if not naive_mixed_detected:
            raise RuntimeError("CONS-01 failed to demonstrate mixed generation on naive .releases/current symlink reader")
        checks.append({
            "id": "CONS-01",
            "scenario": "Naive Multi-Open .releases/current Symlink Reader under Mid-Read Pause (Negative Control)",
            "verdict": "PASS",
            "observed": "Naive symlink reader observed mixed generation {'gen-v1' asm, 'gen-v3' bin} when paused between open(2) calls",
        })

        # ------------------------------------------------------------------
        # CONS-02: Canonical ZCCReleaseConsumer (Once-Resolved + openat + Pin Lease)
        # under Identical Mid-Read Pause & Aggressive Pruning
        # ------------------------------------------------------------------
        publish_to(ext4_repo, "gen-v1")
        consumer = ZCCReleaseConsumer(ext4_repo)
        prune_result_holder: Dict[str, object] = {}

        def mid_read_adversarial_flip() -> None:
            publish_to(ext4_repo, "gen-v2")
            publish_to(ext4_repo, "gen-v3")
            # Attempt aggressive prune(keep_recent=1) while gen-v1 is still being read by paused consumer!
            prune_result_holder["prune"] = prune_old_generations(ext4_repo, keep_recent=1)

        consumed_v1 = consumer.consume_generation(after_second_open_hook=mid_read_adversarial_flip)
        consumed_after = consumer.consume_generation()

        cons02_ok = (
            consumed_v1.generation_id == "gen-v1"
            and consumed_v1.artifact_digests["zcc2.s"] == digests["gen-v1"]["asm"]
            and consumed_v1.artifact_digests["zcc3.s"] == digests["gen-v1"]["asm"]
            and consumed_v1.artifact_digests["zcc2"] == digests["gen-v1"]["bin"]
            and consumed_v1.artifact_digests["zcc3"] == digests["gen-v1"]["bin"]
            and "gen-v1" in prune_result_holder["prune"]["pinned"]
            and "gen-v1" in prune_result_holder["prune"]["kept"]
            and consumed_after.generation_id == "gen-v3"
            and consumed_after.artifact_digests["zcc2"] == digests["gen-v3"]["bin"]
        )
        if not cons02_ok:
            raise RuntimeError(f"CONS-02 failed: consumed_v1={consumed_v1}, prune={prune_result_holder}")
        checks.append({
            "id": "CONS-02",
            "scenario": "Canonical ZCCReleaseConsumer (openat + Pin Lease) under Mid-Read Double Flip & Prune",
            "verdict": "PASS",
            "observed": (
                f"Paused reader returned 100% coherent gen-v1 (pinned={prune_result_holder['prune']['pinned']}); "
                f"subsequent reader returned 100% coherent gen-v3"
            ),
        })

        # ------------------------------------------------------------------
        # CONS-03: 8-Thread Concurrent Reader Gauntlet with 5ms Pause Between Opens on ext4
        # ------------------------------------------------------------------
        stop_ext4 = False
        ext4_reads = [0] * 8
        ext4_seen_gens: Dict[str, int] = {"gen-v1": 0, "gen-v2": 0, "gen-v3": 0}
        ext4_lock = threading.Lock()
        ext4_errors: List[str] = []

        def paused_reader_worker(tid: int, repo_dir: Path, counter_list: List[int], seen_map: Dict[str, int], err_list: List[str], stop_fn: Callable[[], bool]) -> None:
            c = ZCCReleaseConsumer(repo_dir)
            while not stop_fn():
                try:
                    res = c.consume_generation(pause_between_opens_sec=0.005)
                    exp = digests[res.generation_id]
                    if (
                        res.artifact_digests["zcc2.s"] != exp["asm"]
                        or res.artifact_digests["zcc3.s"] != exp["asm"]
                        or res.artifact_digests["zcc2"] != exp["bin"]
                        or res.artifact_digests["zcc3"] != exp["bin"]
                    ):
                        err_list.append(f"Worker {tid} mixed digests in {res.generation_id}")
                    else:
                        counter_list[tid] += 1
                        with ext4_lock:
                            seen_map[res.generation_id] = seen_map.get(res.generation_id, 0) + 1
                except Exception as exc:
                    err_list.append(f"Worker {tid} error: {exc}")

        for ver in ("gen-v1", "gen-v2", "gen-v3"):
            publish_to(ext4_repo, ver)

        threads_ext4 = [
            threading.Thread(
                target=paused_reader_worker,
                args=(i, ext4_repo, ext4_reads, ext4_seen_gens, ext4_errors, lambda: stop_ext4),
                daemon=True,
            )
            for i in range(8)
        ]
        for t in threads_ext4:
            t.start()

        gen_cycle = ["gen-v1", "gen-v2", "gen-v3"]
        try:
            for step in range(60):
                ver = gen_cycle[step % 3]
                rel_final = ext4_repo / ".releases" / f"rel-{ver}"
                m_bytes = (rel_final / "release_manifest.json").read_bytes()
                m_data = json.loads(m_bytes.decode("utf-8"))
                ptr = {
                    "generation_id": ver,
                    "release_dir": f".releases/rel-{ver}",
                    "git_commit": m_data.get("git_commit", "UNKNOWN"),
                    "manifest_sha256": hashlib.sha256(m_bytes).hexdigest(),
                    "artifacts": m_data.get("artifacts", {}),
                }
                tmp_p = ext4_repo / ".releases" / f"ACTIVE_GENERATION.tmp.{step}"
                tmp_p.write_text(json.dumps(ptr, indent=2) + "\n", encoding="utf-8")
                os.replace(tmp_p, ext4_repo / ".releases" / "ACTIVE_GENERATION")
                # Also flip symlink to maximize adversarial pressure
                sym_tmp = ext4_repo / ".releases" / f"current.tmp.{step}"
                try:
                    sym_tmp.unlink(missing_ok=True)
                    sym_tmp.symlink_to(f"rel-{ver}")
                    os.replace(sym_tmp, ext4_repo / ".releases" / "current")
                except OSError:
                    pass
                time.sleep(0.004)
        finally:
            stop_ext4 = True
            for t in threads_ext4:
                t.join()

        if ext4_errors or sum(ext4_reads) == 0:
            raise RuntimeError(f"CONS-03 ext4 paused concurrency errors: {ext4_errors[:5]}")
        checks.append({
            "id": "CONS-03",
            "scenario": "8-Thread Paused Reader Gauntlet (5ms pause between each openat) across 60 Flips on ext4",
            "verdict": "PASS",
            "total_coherent_4file_reads": sum(ext4_reads),
            "generations_observed": dict(ext4_seen_gens),
            "mixed_generations": 0,
        })

        # ------------------------------------------------------------------
        # CONS-04: 8-Thread Concurrent Reader Gauntlet with 5ms Pause Between Opens on 9P DrvFs
        # ------------------------------------------------------------------
        for ver in ("gen-v1", "gen-v2", "gen-v3"):
            publish_to(drvfs_sandbox, ver)

        stop_drvfs = False
        drvfs_reads = [0] * 8
        drvfs_seen_gens: Dict[str, int] = {"gen-v1": 0, "gen-v2": 0, "gen-v3": 0}
        drvfs_errors: List[str] = []

        threads_drvfs = [
            threading.Thread(
                target=paused_reader_worker,
                args=(i, drvfs_sandbox, drvfs_reads, drvfs_seen_gens, drvfs_errors, lambda: stop_drvfs),
                daemon=True,
            )
            for i in range(8)
        ]
        for t in threads_drvfs:
            t.start()

        try:
            for step in range(36):
                ver = gen_cycle[step % 3]
                rel_final = drvfs_sandbox / ".releases" / f"rel-{ver}"
                m_bytes = (rel_final / "release_manifest.json").read_bytes()
                m_data = json.loads(m_bytes.decode("utf-8"))
                ptr = {
                    "generation_id": ver,
                    "release_dir": f".releases/rel-{ver}",
                    "git_commit": m_data.get("git_commit", "UNKNOWN"),
                    "manifest_sha256": hashlib.sha256(m_bytes).hexdigest(),
                    "artifacts": m_data.get("artifacts", {}),
                }
                tmp_p = drvfs_sandbox / ".releases" / f"ACTIVE_GENERATION.tmp.{step}"
                tmp_p.write_text(json.dumps(ptr, indent=2) + "\n", encoding="utf-8")
                os.replace(tmp_p, drvfs_sandbox / ".releases" / "ACTIVE_GENERATION")
                time.sleep(0.006)
        finally:
            stop_drvfs = True
            for t in threads_drvfs:
                t.join()

        if drvfs_errors or sum(drvfs_reads) == 0:
            raise RuntimeError(f"CONS-04 DrvFs paused concurrency errors: {drvfs_errors[:5]}")
        checks.append({
            "id": "CONS-04",
            "scenario": "8-Thread Paused Reader Gauntlet (5ms pause between each openat) across 36 Flips on 9P DrvFs",
            "verdict": "PASS",
            "total_coherent_4file_reads": sum(drvfs_reads),
            "generations_observed": dict(drvfs_seen_gens),
            "mixed_generations": 0,
        })

        # ------------------------------------------------------------------
        # CONS-05: Negative Control — 1-Byte Artifact Corruption inside Generation Directory
        # ------------------------------------------------------------------
        corrupt_target = ext4_repo / ".releases" / "rel-gen-v3" / "zcc2"
        orig_bytes = corrupt_target.read_bytes()
        mut_bytes = bytearray(orig_bytes)
        mut_bytes[-1] ^= 0xFF
        corrupt_target.write_bytes(bytes(mut_bytes))
        caught_corrupt = False
        try:
            ZCCReleaseConsumer(ext4_repo).consume_generation()
        except ReleaseConsumerIntegrityError as exc:
            caught_corrupt = "failed manifest digest check" in str(exc)
        finally:
            corrupt_target.write_bytes(orig_bytes)

        if not caught_corrupt:
            raise RuntimeError("CONS-05 failed to reject 1-byte corrupted generation artifact")
        checks.append({
            "id": "CONS-05",
            "scenario": "Negative Control: 1-Byte Artifact Corruption inside Pinned Generation Directory",
            "verdict": "PASS",
            "observed": "Rejected with ReleaseConsumerIntegrityError (failed manifest digest check)",
        })

        # ------------------------------------------------------------------
        # CONS-06: Negative Control — Tampered release_manifest.json vs ACTIVE_GENERATION
        # ------------------------------------------------------------------
        manifest_target = ext4_repo / ".releases" / "rel-gen-v3" / "release_manifest.json"
        orig_manifest = manifest_target.read_bytes()
        tampered_m = json.loads(orig_manifest.decode("utf-8"))
        tampered_m["git_commit"] = "0" * 40
        manifest_target.write_text(json.dumps(tampered_m, indent=2) + "\n", encoding="utf-8")
        caught_manifest = False
        try:
            ZCCReleaseConsumer(ext4_repo).consume_generation()
        except ReleaseConsumerIntegrityError as exc:
            caught_manifest = "Manifest SHA-256 mismatch" in str(exc)
        finally:
            manifest_target.write_bytes(orig_manifest)

        if not caught_manifest:
            raise RuntimeError("CONS-06 failed to reject tampered release_manifest.json")
        checks.append({
            "id": "CONS-06",
            "scenario": "Negative Control: Tampered release_manifest.json vs ACTIVE_GENERATION Pointer",
            "verdict": "PASS",
            "observed": "Rejected with ReleaseConsumerIntegrityError (Manifest SHA-256 mismatch)",
        })

        report = {
            "gate": "Track 2 — Release Consumer Consistency Contract (Once-Resolved Generation + openat + Pin Lease)",
            "verdict": "PASS",
            "checks_total": len(checks),
            "checks_passed": len(checks),
            "checks": checks,
        }
        if json_out:
            json_out.parent.mkdir(parents=True, exist_ok=True)
            json_out.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")

        print("================================================================================")
        print(f"🔒 ZCC RELEASE CONSUMER CONSISTENCY GATE: ALL {len(checks)}/{len(checks)} CHECKS PASSED")
        for c in checks:
            print(f"   [{c['id']}] {c['scenario']} -> {c['verdict']}")
        print("================================================================================")
        return EXIT_OK
    finally:
        shutil.rmtree(tmp_root, ignore_errors=True)
        shutil.rmtree(drvfs_sandbox, ignore_errors=True)


def main() -> int:
    parser = argparse.ArgumentParser(description="ZCC Canonical Release Generation Consumer")
    parser.add_argument("--repo", default="/mnt/h/__DOWNLOADS/zcc_github_upload", help="Repository root")
    parser.add_argument("--self-test", action="store_true", help="Run adversarial pause & consistency self-test")
    parser.add_argument("--json-out", default=None, help="Output JSON report path")
    args = parser.parse_args()

    repo_root = Path(args.repo).resolve()
    if args.self_test:
        return run_self_test(repo_root, Path(args.json_out) if args.json_out else None)

    try:
        res = ZCCReleaseConsumer(repo_root).consume_generation()
    except ReleaseConsumerIntegrityError as exc:
        print(f"RELEASE CONSUMER REJECT: {exc}", file=sys.stderr)
        return EXIT_CONSUMER_INTEGRITY_FAIL

    out = {
        "verdict": "PASS",
        "generation_id": res.generation_id,
        "release_dir": res.release_dir,
        "git_commit": res.git_commit,
        "manifest_sha256": res.manifest_sha256,
        "artifact_digests": res.artifact_digests,
        "artifact_sizes": res.artifact_sizes,
        "enoent_retries": res.enoent_retries,
    }
    if args.json_out:
        Path(args.json_out).write_text(json.dumps(out, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(out, indent=2))
    return EXIT_OK


if __name__ == "__main__":
    sys.exit(main())

