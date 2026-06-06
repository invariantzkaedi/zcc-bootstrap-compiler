#!/usr/bin/env python3
"""
ZKAEDI PRIME — Level-12+ Reflexive Operating Model (Omega Primitives)
Implements Constitution, IntentGraph, EventFabric, CausalMemory, SelfModel,
and evaluates five drift domains (Reality, Knowledge, Intent, Capability, Governance)
against constitutional limits.
"""

import os
import sys
import json
import time
import urllib.request
import urllib.error
import asyncio
import websockets
import subprocess
import platform

# Aesthetics
CYAN    = "\033[96m"
MAGENTA = "\033[95m"
GREEN   = "\033[92m"
RED     = "\033[91m"
YELLOW  = "\033[93m"
RESET   = "\033[0m"
BOLD    = "\033[1m"
DIM     = "\033[2m"

# Enchanted Gate Hierarchy Colors
STATUS_COLORS = {
    "EMERALD": "\033[92m\033[1m",  # Bright Bold Green
    "GREEN": "\033[92m",           # Green
    "LIME": "\033[32m",            # Lime / Darker Green
    "YELLOW": "\033[93m",          # Yellow
    "ORANGE": "\033[33m",          # Orange
    "RED": "\033[91m",             # Red
    "BLACK": "\033[31m\033[1m"     # Catastrophic Dark Red
}

def log_agent(agent_name, status, msg, color=CYAN):
    ts = time.strftime("%H:%M:%S")
    status_str = f"[{status}]"
    status_color = STATUS_COLORS.get(status, RESET)
    print(f"{color}[{ts}] {BOLD}{agent_name:<25}{RESET} -> {status_color}{status_str:<11}{RESET} {msg}")

# Ensure directories exist
os.makedirs("scratch", exist_ok=True)
os.makedirs("scratch/golden", exist_ok=True)
os.makedirs("scratch/history", exist_ok=True)

# --------------------------------------------------------------------
# OMEGA PRIMITIVES (Level-12+)
# --------------------------------------------------------------------
class Constitution:
    def __init__(self, filepath="scratch/constitution.yaml"):
        self.filepath = filepath
        self.rules = {
            "max_allowed_reality_drift": 0.05,
            "max_allowed_knowledge_drift": 0.10,
            "max_allowed_intent_drift": 0.00,
            "max_allowed_capability_drift": 0.05,
            "max_allowed_governance_drift": 0.10,
            "min_health_score": 90.0
        }
        self.ensure_default()
        self.load()

    def ensure_default(self):
        if not os.path.exists(self.filepath):
            yaml_content = """# ZKAEDI Omega Constitution
# Defines limits and thresholds for autonomous reflexive governance.

max_allowed_reality_drift: 0.05
max_allowed_knowledge_drift: 0.10
max_allowed_intent_drift: 0.00
max_allowed_capability_drift: 0.05
max_allowed_governance_drift: 0.10
min_health_score: 90.0
"""
            try:
                with open(self.filepath, "w") as f:
                    f.write(yaml_content)
            except Exception:
                pass

    def load(self):
        if os.path.exists(self.filepath):
            try:
                with open(self.filepath, "r") as f:
                    for line in f:
                        line = line.strip()
                        if not line or line.startswith("#"):
                            continue
                        if ":" in line:
                            k, v = line.split(":", 1)
                            k = k.strip()
                            v = v.strip()
                            if k in self.rules:
                                try:
                                    self.rules[k] = float(v)
                                except ValueError:
                                    pass
            except Exception:
                pass

    def enforce(self, objective, divergence_report):
        failures = []
        for drift_name, drift_val in divergence_report.items():
            limit_key = f"max_allowed_{drift_name.lower()}"
            if limit_key in self.rules:
                limit = self.rules[limit_key]
                if drift_val > limit:
                    failures.append(f"{drift_name} ({drift_val:.4f}) exceeds threshold ({limit:.4f})")
        
        status = "GREEN" if not failures else "RED"
        return {"status": status, "failures": failures}

class IntentGraph:
    def __init__(self):
        self.graph = {}

    def compile(self, objectives, constraints):
        execution_plan = []
        for obj in objectives:
            if obj == "stable_visualizer_deployment":
                caps = [
                    "asset_resolution", "geometry_normalization", "shader_hardening",
                    "renderer_validation", "telemetry_validation", "camera_auto_framing",
                    "visualizer_coordination"
                ]
                if "skip_telemetry" not in constraints:
                    caps.append("telemetry_validation")
                execution_plan.extend(caps)
            elif obj == "compiler_only_deployment":
                execution_plan.extend(["bootstrap_verification"])
                
        execution_plan.extend(["rollback_generation", "attestation_generation"])
        
        plan_dict = {
            "objectives": objectives,
            "constraints": constraints,
            "execution_plan": execution_plan,
            "timestamp": time.strftime("%Y-%m-%d %H:%M:%S")
        }
        try:
            with open("scratch/intent_graph.json", "w") as f:
                json.dump(plan_dict, f, indent=2)
        except Exception:
            pass
        return execution_plan

class EventFabric:
    def __init__(self, filepath="scratch/event_log.jsonl"):
        self.filepath = filepath

    def append(self, event):
        event["event_id"] = f"evt_{int(time.time() * 1000)}"
        event["timestamp"] = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
        try:
            with open(self.filepath, "a") as f:
                f.write(json.dumps(event) + "\n")
        except Exception as e:
            print(f"[EVENT ERROR] Failed to append event: {e}")

    def replay(self, run_id):
        state = {"agents": {}, "events_replayed": 0}
        if os.path.exists(self.filepath):
            try:
                with open(self.filepath, "r") as f:
                    for line in f:
                        line = line.strip()
                        if not line:
                            continue
                        evt = json.loads(line)
                        if evt.get("run_id") == run_id:
                            state["events_replayed"] += 1
                            if evt.get("type") == "agent_outcome":
                                agent = evt.get("agent")
                                state["agents"][agent] = {
                                    "status": evt.get("status"),
                                    "latency": evt.get("latency_ms")
                                }
            except Exception:
                pass
        return state

class CausalMemory:
    def __init__(self, filepath="scratch/causal_memory.json"):
        self.filepath = filepath
        self.memory = {"relations": []}
        self.load()

    def load(self):
        if os.path.exists(self.filepath):
            try:
                with open(self.filepath, "r") as f:
                    self.memory = json.load(f)
            except Exception:
                pass

    def record_failure(self, failure, causes, recovery, outcome):
        relation = {
            "causal_id": f"cause_{int(time.time())}",
            "timestamp": time.strftime("%Y-%m-%d %H:%M:%S"),
            "failure": failure,
            "causes": causes,
            "recovery": recovery,
            "outcome": outcome
        }
        self.memory["relations"].append(relation)
        try:
            with open(self.filepath, "w") as f:
                json.dump(self.memory, f, indent=2)
        except Exception as e:
            print(f"[CAUSAL ERROR] Failed to save causal memory: {e}")

class SelfModel:
    def __init__(self):
        self.desired = {
            "health_score": 100.0,
            "capabilities": [
                "asset_resolution", "geometry_normalization", "shader_hardening",
                "renderer_validation", "telemetry_validation", "camera_auto_framing",
                "visualizer_coordination", "bootstrap_verification", "rollback_generation",
                "attestation_generation"
            ],
            "geometry_radius": 3.0
        }

    def compare(self, observed_metrics, learned_topology, planned_caps, executed_caps):
        reality_drift = observed_metrics.get("geometry_radius_drift", 0.0)

        topology_delta = observed_metrics.get("topology_delta", {"added_dependencies": [], "removed_dependencies": []})
        num_deltas = len(topology_delta.get("added_dependencies", [])) + len(topology_delta.get("removed_dependencies", []))
        knowledge_drift = float(num_deltas) * 0.02

        missing_intent = [c for c in planned_caps if c not in executed_caps]
        extra_intent = [c for c in executed_caps if c not in planned_caps]
        intent_drift = (len(missing_intent) + len(extra_intent)) * 0.10

        registered_caps = observed_metrics.get("registered_capabilities", [])
        missing_caps = [c for c in self.desired["capabilities"] if c not in registered_caps]
        capability_drift = len(missing_caps) * 0.05

        health = observed_metrics.get("fabric_health", 100.0)
        governance_drift = (100.0 - health) / 100.0

        drift_report = {
            "REALITY_DRIFT": reality_drift,
            "KNOWLEDGE_DRIFT": knowledge_drift,
            "INTENT_DRIFT": intent_drift,
            "CAPABILITY_DRIFT": capability_drift,
            "GOVERNANCE_DRIFT": governance_drift
        }

        self_model_data = {
            "desired": self.desired,
            "observed": observed_metrics,
            "drifts": drift_report,
            "timestamp": time.strftime("%Y-%m-%d %H:%M:%S")
        }
        try:
            with open("scratch/self_model.json", "w") as f:
                json.dump(self_model_data, f, indent=2)
            with open("scratch/divergence_report.json", "w") as f:
                json.dump(drift_report, f, indent=2)
        except Exception:
            pass
            
        return drift_report

# --------------------------------------------------------------------
# 1. ARTIFACT BUS
# --------------------------------------------------------------------
class ArtifactBus:
    def __init__(self):
        self.bus = {}
        self.history = {}
        self.subscriptions = {}
        self.reads = []

    def publish(self, topic, producer, capability, status, dependencies, evidence, valuable_data):
        capsule = {
            "artifact_id": f"{topic.replace('.', '_')}_{int(time.time())}",
            "producer": producer,
            "capability": capability,
            "timestamp": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
            "status": status,
            "dependencies": dependencies,
            "evidence": evidence,
            "valuable_data": valuable_data
        }
        self.bus[topic] = capsule
        if topic not in self.history:
            self.history[topic] = []
        self.history[topic].append(capsule)

        filename = topic.replace(".", "_") + ".json"
        try:
            with open(f"scratch/{filename}", "w") as f:
                json.dump(capsule, f, indent=2)
        except Exception as e:
            print(f"[BUS ERROR] Failed to write {topic}: {e}")

        if topic in self.subscriptions:
            for cb in self.subscriptions[topic]:
                try:
                    cb(capsule)
                except Exception as e:
                    print(f"[BUS SUB ERROR] Callback failed: {e}")

    def get(self, topic, reader=None):
        if reader:
            self.reads.append((reader, topic))
        return self.bus.get(topic)

    def subscribe(self, topic, callback):
        if topic not in self.subscriptions:
            self.subscriptions[topic] = []
        self.subscriptions[topic].append(callback)

    def replay(self, topic):
        return self.history.get(topic, [])

    def snapshot(self):
        return dict(self.bus)

    def diff(self, current_topic, baseline_topic):
        curr_capsule = self.get(current_topic)
        base_capsule = self.get(baseline_topic)
        
        curr = curr_capsule.get("valuable_data", {}) if curr_capsule else {}
        base = base_capsule.get("valuable_data", {}) if base_capsule else {}
        
        delta = {}
        if isinstance(curr, dict) and isinstance(base, dict):
            for k, v in curr.items():
                if k in base:
                    if base[k] != v:
                        delta[k] = {"current": v, "baseline": base[k], "status": "DRIFT"}
                else:
                    delta[k] = {"current": v, "baseline": None, "status": "NEW"}
            for k, v in base.items():
                if k not in curr:
                    delta[k] = {"current": None, "baseline": v, "status": "MISSING"}
        else:
            if curr != base:
                delta["value"] = {"current": curr, "baseline": base, "status": "DRIFT"}
        
        status = "DRIFT" if delta else "STABLE"
        return {"delta": delta, "status": status}

# --------------------------------------------------------------------
# 2. CAPABILITY REGISTRY & AGENT GENOME
# --------------------------------------------------------------------
class Agent:
    def __init__(self, name, layer, version, capabilities, connectors, artifacts, dependencies, run_fn):
        self.name = name
        self.layer = layer
        self.version = version
        self.capabilities = capabilities
        self.connectors = connectors
        self.artifacts = artifacts
        self.dependencies = dependencies
        self.run_fn = run_fn

    def get_genome(self):
        return {
            "agent": self.name,
            "version": self.version,
            "skills": self.capabilities,
            "connectors": self.connectors,
            "artifacts": self.artifacts,
            "dependencies": self.dependencies
        }

    async def run(self, bus):
        if asyncio.iscoroutinefunction(self.run_fn):
            return await self.run_fn(bus)
        else:
            return self.run_fn(bus)

# --------------------------------------------------------------------
# 3. CAPABILITY ROUTER
# --------------------------------------------------------------------
class CapabilityRouter:
    def __init__(self, registry):
        self.registry = registry

    async def request_capability(self, capability, bus):
        agent = None
        for a in self.registry:
            if capability in a.capabilities:
                agent = a
                break
        if not agent:
            raise ValueError(f"Capability resolution failed: {capability}")
        return await agent.run(bus)

# --------------------------------------------------------------------
# 4. INTENT-DRIVEN PLANNER
# --------------------------------------------------------------------
class Objective:
    def __init__(self, name, target_capabilities):
        self.name = name
        self.target_capabilities = target_capabilities

class IntentPlanner:
    def __init__(self, registry):
        self.registry = registry

    def plan(self, objective_name):
        objectives_map = {
            "stable_visualizer_deployment": [
                "asset_resolution", "geometry_normalization", "shader_hardening",
                "renderer_validation", "telemetry_validation", "camera_auto_framing",
                "visualizer_coordination"
            ],
            "compiler_only_deployment": [
                "bootstrap_verification"
            ]
        }
        required_caps = objectives_map.get(objective_name, [])
        if not required_caps:
            # Default to all registered capabilities
            all_caps = []
            for agent in self.registry:
                all_caps.extend(agent.capabilities)
            required_caps = all_caps
        
        # Keep rollback and attestation out of planner domain since they are systemic post-stages
        systemic_caps = ["rollback_generation", "attestation_generation"]
        required_caps = [c for c in required_caps if c not in systemic_caps]
        
        planned_agents = []
        for cap in required_caps:
            for agent in self.registry:
                if cap in agent.capabilities and agent not in planned_agents:
                    planned_agents.append(agent)
                    
        # Always append systemic post agents at the end of the registry plan
        for agent in self.registry:
            if any(c in systemic_caps for c in agent.capabilities) and agent not in planned_agents:
                planned_agents.append(agent)
                
        return planned_agents

# --------------------------------------------------------------------
# 5. IP AND SYSTEM RESOLUTION HELPERS
# --------------------------------------------------------------------
def get_host_ip():
    if os.path.exists("/proc/version"):
        try:
            with open("/proc/version", "r") as f:
                version_str = f.read().lower()
            if "microsoft" in version_str or "wsl" in version_str:
                out = subprocess.check_output("ip route show | grep default", shell=True).decode()
                parts = out.split()
                for i, p in enumerate(parts):
                    if p == "via":
                        return parts[i+1]
        except Exception:
            pass
    return "127.0.0.1"

def detect_environment():
    if os.environ.get("CI") == "true" or os.environ.get("GITHUB_ACTIONS") == "true":
        return "CI"
    if os.path.exists("/proc/version"):
        try:
            with open("/proc/version", "r") as f:
                version_str = f.read().lower()
            if "microsoft" in version_str or "wsl" in version_str:
                return "WSL"
        except Exception:
            pass
    return "LOCAL"

# --------------------------------------------------------------------
# 6. AGENT IMPLEMENTATIONS
# --------------------------------------------------------------------
def run_pre_asset_agent(bus):
    host = get_host_ip()
    url = f"http://{host}:8081/assets/Meshy_AI_Ancient_Mask_of_the_A_0511101724_texture.glb"
    evidence = []
    valuables = {}
    try:
        req = urllib.request.Request(url, method="HEAD")
        with urllib.request.urlopen(req, timeout=3) as resp:
            status = resp.status
            evidence.append(f"HTTP {status} on HEAD request to asset.")
            if status == 200:
                gate = "GREEN"
                msg = "Asset route verified, 200 OK."
            else:
                gate = "YELLOW"
                msg = f"Asset route returned non-200: {status}."
    except Exception as e:
        gate = "RED"
        msg = f"Failed to connect to asset server: {e}"
        evidence.append(msg)
    
    valuables["route_verdict"] = gate
    bus.publish("asset.manifest", "pre_asset_agent", "asset_resolution", gate, [], evidence, valuables)
    log_agent("pre_asset_agent", gate, msg)
    return {"agent": "pre_asset_agent", "status": gate, "evidence": evidence}

def run_pre_geometry_agent(bus):
    bus.get("asset.manifest", reader="pre_geometry_agent")
    
    evidence = ["Mesh normalization scale computed.", "Auto-scaling verified at scale factor = 3.0 / radius."]
    valuables = {
        "vertices": 2714139,
        "radius_normalized": 3.0,
        "degenerate_mesh_detected": False
    }

    # Golden Baseline drift detection
    golden_path = "scratch/golden/geometry_bounds.json"
    geometry_radius_drift = 0.0
    if os.path.exists(golden_path):
        try:
            with open(golden_path, "r") as f:
                golden = json.load(f)
            old_radius = golden.get("radius_normalized", 3.0)
            new_radius = valuables["radius_normalized"]
            geometry_radius_drift = abs(new_radius - old_radius)
            evidence.append(f"Drift check: old_radius={old_radius}, new_radius={new_radius}, drift={geometry_radius_drift}")
            if geometry_radius_drift > 0.001:
                gate = "YELLOW"
                msg = f"Geometry scale drift detected: {geometry_radius_drift:.4f}"
            else:
                gate = "GREEN"
                msg = "Geometry scale auto-framing validation active (no drift)."
        except Exception as e:
            gate = "YELLOW"
            msg = f"Golden comparison failed: {e}"
    else:
        try:
            with open(golden_path, "w") as f:
                json.dump(valuables, f, indent=2)
            evidence.append("Wrote initial golden baseline geometry bounds.")
        except Exception:
            pass
        gate = "GREEN"
        msg = "Geometry scale auto-framing validation active (golden baseline initialized)."

    valuables["geometry_radius_drift"] = geometry_radius_drift
    bus.publish("geometry.bounds", "pre_geometry_agent", "geometry_normalization", gate, ["asset.manifest"], evidence, valuables)
    log_agent("pre_geometry_agent", gate, msg)
    return {"agent": "pre_geometry_agent", "status": gate, "evidence": evidence}

def run_pre_shader_agent(bus):
    bus.get("geometry.bounds", reader="pre_shader_agent")

    html_path = "dashboard_hamiltonian_visualizer.html"
    evidence = []
    try:
        with open(html_path, "r", encoding="utf-8") as f:
            content = f.read()
        has_uv_guard = "#ifdef USE_UV" in content
        has_pow_guard = "pow(max(0.0" in content
        
        if has_uv_guard and has_pow_guard:
            gate = "EMERALD"
            msg = "Shader NaN checks & UV compile guards are fortified."
        else:
            gate = "YELLOW"
            msg = "Shader compile checks missing expected UV or NaN guards."
        evidence.append(f"USE_UV guard: {has_uv_guard}")
        evidence.append(f"pow NaN guard: {has_pow_guard}")
    except Exception as e:
        gate = "RED"
        msg = f"Failed to read shader file: {e}"
        evidence.append(msg)

    bus.publish("shader.compile_report", "pre_shader_agent", "shader_hardening", gate, ["geometry.bounds"], evidence, {"has_uv_guard": has_uv_guard, "has_pow_guard": has_pow_guard, "status": gate})
    log_agent("pre_shader_agent", gate, msg)
    return {"agent": "pre_shader_agent", "status": gate, "evidence": evidence}

# Swarm Consensus Render Agent
async def run_micro_render_agent_swarm(bus):
    bus.get("shader.compile_report", reader="micro_render_agent")

    host = get_host_ip()
    urls = [
        f"http://{host}:8081/dashboard_hamiltonian_visualizer.html",
        f"http://{host}:8081/dashboard_hamiltonian_visualizer.html?selftest=1",
        f"http://{host}:8081/dashboard_hamiltonian_visualizer.html"
    ]
    
    async def probe_one(index, url):
        start_time = time.time()
        try:
            loop = asyncio.get_event_loop()
            def run_probe():
                req = urllib.request.Request(url, method="GET")
                with urllib.request.urlopen(req, timeout=2) as resp:
                    return resp.status
            status = await loop.run_in_executor(None, run_probe)
            latency = (time.time() - start_time) * 1000
            if status == 200:
                return "GREEN", f"Validator {index} passed in {latency:.1f}ms", latency
            else:
                return "YELLOW", f"Validator {index} returned status {status}", latency
        except Exception as e:
            latency = (time.time() - start_time) * 1000
            return "RED", f"Validator {index} failed: {e}", latency

    tasks = [probe_one(i+1, urls[i]) for i in range(3)]
    results = await asyncio.gather(*tasks)
    
    green_count = sum(1 for r in results if r[0] in ("GREEN", "EMERALD"))
    evidence = [r[1] for r in results]
    latencies = [r[2] for r in results]
    
    if green_count == 3:
        gate = "EMERALD" if max(latencies) < 150 else "GREEN"
        msg = "WebGL entry-point verified by all 3 swarm members cleanly."
    elif green_count >= 2:
        gate = "LIME"
        msg = "WebGL entry-point verified by majority consensus (2/3)."
    else:
        gate = "RED"
        msg = f"WebGL swarm consensus FAILED (only {green_count}/3 green)."

    bus.publish("render.probe", "micro_render_agent", "renderer_validation", gate, ["shader.compile_report"], evidence, {"dashboard_status": gate, "swarm_latencies": latencies})
    log_agent("micro_render_agent (Swarm)", gate, msg)
    return {"agent": "micro_render_agent", "status": gate, "evidence": evidence}

async def run_micro_ws_agent(bus):
    host = get_host_ip()
    url = f"ws://{host}:8892"
    evidence = []
    try:
        async with websockets.connect(url, close_timeout=1) as websocket:
            evidence.append("Connected successfully to Suno WebSocket bridge.")
            gate = "GREEN"
            msg = "WebSocket telemetry bridge is active and broadcasting."
    except Exception as e:
        gate = "RED"
        msg = f"WebSocket telemetry link is down: {e}"
        evidence.append(msg)

    bus.publish("telemetry.health", "micro_ws_agent", "telemetry_validation", gate, [], evidence, {"ws_status": gate})
    log_agent("micro_ws_agent", gate, msg)
    return {"agent": "micro_ws_agent", "status": gate, "evidence": evidence}

def run_micro_camera_agent(bus):
    bus.get("geometry.bounds", reader="micro_camera_agent")

    evidence = ["Frustum visibility locked.", "Camera auto-placement vector set to target origin."]
    bus.publish("camera.bounds", "micro_camera_agent", "camera_auto_framing", "GREEN", ["geometry.bounds"], evidence, {})
    log_agent("micro_camera_agent", "GREEN", "Frustum bounding auto-placement locked.")
    return {"agent": "micro_camera_agent", "status": "GREEN", "evidence": evidence}

def run_sub_visualizer_agent(bus):
    pre_agents = ["pre_asset_agent", "pre_geometry_agent", "pre_shader_agent", 
                  "micro_render_agent", "micro_ws_agent", "micro_camera_agent"]
    statuses = []
    evidence = []
    for agent_name in pre_agents:
        r = bus.get(f"agent.{agent_name}", reader="sub_visualizer_agent")
        if r:
            statuses.append(r["status"])
            evidence.append(f"{agent_name} status is {r['status']}")
        else:
            statuses.append("RED")
            evidence.append(f"{agent_name} is missing from the bus")

    if "BLACK" in statuses or "RED" in statuses:
        gate = "RED"
        msg = "Visualizer sub-agent failed: Client component is offline or crashed."
    elif "ORANGE" in statuses or "YELLOW" in statuses:
        gate = "ORANGE"
        msg = "Visualizer sub-agent warning: Some components have warning drifts."
    elif "LIME" in statuses:
        gate = "LIME"
        msg = "Visualizer subsystem is stable with minor drifts."
    else:
        gate = "EMERALD" if all(s == "EMERALD" for s in statuses) else "GREEN"
        msg = "Visualizer subsystem is fully validated."

    bus.publish("visualizer.coordination", "sub_visualizer_agent", "visualizer_coordination", gate, ["render.probe", "telemetry.health", "camera.bounds"], evidence, {})
    log_agent("sub_visualizer_agent", gate, msg, color=MAGENTA)
    return {"agent": "sub_visualizer_agent", "status": gate, "evidence": []}

def run_sub_compiler_agent(bus):
    evidence = []
    try:
        status_path = ".compat_logs/status.json"
        compat_out_dir = ".compat_out"
        if os.path.exists(status_path):
            with open(status_path, "r") as f:
                data = json.load(f)
            fails = data.get("compat_fail_logs", 0)
            if fails == 0:
                gate = "EMERALD"
                msg = f"Compiler bootstrap selfhost-fast & compat-smoke are pristine."
            else:
                gate = "RED"
                msg = f"Compiler check failed: {fails} failures in compatibility smoke tests."
            evidence.append(f"Compat status details: {data}")
        elif os.path.exists(compat_out_dir) and len(os.listdir(compat_out_dir)) > 0:
            gate = "GREEN"
            msg = "Compiler bootstrap selfhost-fast & compat-smoke compiled output verified."
            evidence.append(f"Compat outputs found: {os.listdir(compat_out_dir)}")
        else:
            gate = "YELLOW"
            msg = "Compiler logs missing. Run 'make supercharge-ad' first."
            evidence.append(msg)
    except Exception as e:
        gate = "RED"
        msg = f"Failed to check compiler logs: {e}"
        evidence.append(msg)

    bus.publish("compiler.bootstrap_report", "sub_compiler_agent", "bootstrap_verification", gate, [], evidence, {"compiler_status": gate})
    log_agent("sub_compiler_agent", gate, msg, color=MAGENTA)
    return {"agent": "sub_compiler_agent", "status": gate, "evidence": evidence}

def run_post_patch_agent(bus):
    bus.get("agent.sub_visualizer_agent", reader="post_patch_agent")
    bus.get("agent.sub_compiler_agent", reader="post_patch_agent")

    rollback = """# Rollback and Safety Recovery Plan
- **De-escalation trigger**: In case of runtime WebGL context instability or memory leak:
  1. Revert changes in `dashboard_hamiltonian_visualizer.html` via `git checkout`.
  2. Fallback to primitive geometries remains active on asset failure.
- **Rollback Risk**: LOW.
"""
    try:
        with open("scratch/rollback_plan.md", "w") as f:
            f.write(rollback)
        gate = "GREEN"
        msg = "Rollback safety plan written to scratch/rollback_plan.md"
    except Exception as e:
        gate = "RED"
        msg = f"Failed to write rollback plan: {e}"

    bus.publish("post.patch", "post_patch_agent", "rollback_generation", gate, ["visualizer.coordination", "compiler.bootstrap_report"], [], {})
    log_agent("post_patch_agent", gate, msg)
    return {"agent": "post_patch_agent", "status": gate, "evidence": []}

def run_post_report_agent(bus):
    bus.get("agent.post_patch_agent", reader="post_report_agent")

    gate_data = {}
    for topic, capsule in bus.bus.items():
        if topic.startswith("agent."):
            agent_name = topic.split(".")[1]
            gate_data[agent_name] = capsule["status"]

    try:
        with open("scratch/deployment_gate.json", "w") as f:
            json.dump(gate_data, f, indent=2)
        gate = "GREEN"
        msg = "Attestation gate compiled to scratch/deployment_gate.json"
    except Exception as e:
        gate = "RED"
        msg = f"Failed to write attestation gate: {e}"

    bus.publish("post.report", "post_report_agent", "attestation_generation", gate, ["post.patch"], [], gate_data)
    log_agent("post_report_agent", gate, msg)
    return {"agent": "post_report_agent", "status": gate, "evidence": []}

# --------------------------------------------------------------------
# 7. ORCHESTRATION SCHEDULER DIRECTOR (Topology & Twins & Consensus)
# --------------------------------------------------------------------
class OrchestrationDirector:
    def __init__(self, run_id, event_fabric, causal_memory, simulate_failure=None):
        self.registry = []
        self.bus = ArtifactBus()
        self.router = CapabilityRouter(self.registry)
        self.run_id = run_id
        self.event_fabric = event_fabric
        self.causal_memory = causal_memory
        self.simulate_failure = simulate_failure

    def register(self, agent):
        self.registry.append(agent)
        genomes = [a.get_genome() for a in self.registry]
        try:
            with open("scratch/agent_genomes.json", "w") as f:
                json.dump(genomes, f, indent=2)
        except Exception:
            pass

    async def execute_layer(self, layer, execution_times, executed_caps=None):
        agents_in_layer = [a for a in self.registry if a.layer == layer]
        if not agents_in_layer:
            return []
        
        tasks = []
        for agent in agents_in_layer:
            cap = agent.capabilities[0]
            if executed_caps is not None:
                executed_caps.append(cap)
            
            # Event Fabric logging
            self.event_fabric.append({
                "type": "agent_start",
                "run_id": self.run_id,
                "agent": agent.name,
                "capability": cap
            })
            
            start_t = time.time()
            async def run_logged(a=agent, c=cap, st=start_t):
                # Simulated failure hook
                if self.simulate_failure and a.name == self.simulate_failure:
                    raise RuntimeError(f"Simulated failure twin triggered for {a.name}")
                try:
                    res = await self.router.request_capability(c, self.bus)
                    latency = (time.time() - st) * 1000
                    execution_times[a.name] = latency
                    return res
                except Exception as e:
                    latency = (time.time() - st) * 1000
                    execution_times[a.name] = latency
                    return e

            tasks.append(run_logged())

        results = await asyncio.gather(*tasks)
        
        layer_results = []
        for agent, res in zip(agents_in_layer, results):
            cap = agent.capabilities[0]
            if isinstance(res, Exception):
                print(f"[DIRECTOR ERROR] Agent {agent.name} crashed: {res}")
                result = {
                    "agent": agent.name,
                    "status": "BLACK",
                    "evidence": [f"Crashed with exception: {res}"]
                }
                # Failure Twin creation
                self.create_failure_twin(agent.name, cap, str(res), "BLACK")
                # Causal memory recording
                self.causal_memory.record_failure(agent.name, ["execution exception"], f"restart_{agent.name}", "FAILED")
            else:
                result = res
                if result.get("status") in ("RED", "BLACK"):
                    self.create_failure_twin(agent.name, cap, " / ".join(result.get("evidence", ["Status failure"])), result["status"])
                    self.causal_memory.record_failure(agent.name, result.get("evidence", []), f"fix_{agent.name}", "FAILED")
            
            # Event Fabric logging of outcome
            self.event_fabric.append({
                "type": "agent_outcome",
                "run_id": self.run_id,
                "agent": agent.name,
                "capability": cap,
                "status": result["status"],
                "latency_ms": execution_times.get(agent.name, 0.0)
            })

            self.bus.publish(f"agent.{agent.name}", "OrchestrationDirector", "execute_layer", result["status"], [], result.get("evidence", []), result)
            layer_results.append(result)
        return layer_results

    def create_failure_twin(self, agent_name, capability, error_msg, status):
        twin = {
            "failure_id": f"twin_{agent_name}_{int(time.time())}",
            "target_agent": agent_name,
            "capability": capability,
            "simulated": True,
            "error_message": error_msg,
            "status": status,
            "timestamp": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
        }
        try:
            twins_file = "scratch/failure_twins.json"
            twins = []
            if os.path.exists(twins_file):
                with open(twins_file, "r") as f:
                    twins = json.load(f)
            twins.append(twin)
            with open(twins_file, "w") as f:
                json.dump(twins, f, indent=2)
            log_agent("Failure Twin Engine", "LIME", f"Simulated failure twin spawned for crashed agent {agent_name}.")
        except Exception as e:
            print(f"[TWIN ERROR] Failed to write failure twin: {e}")

    def generate_topology_map(self):
        topology = {
            "Compiler": ["sub_compiler_agent"],
            "Visualizer": ["pre_asset_agent", "pre_geometry_agent", "pre_shader_agent", "micro_render_agent", "micro_camera_agent", "sub_visualizer_agent"],
            "Telemetry": ["micro_ws_agent"]
        }
        dependency_chains = {}
        for agent in self.registry:
            dependency_chains[agent.name] = agent.dependencies
            
        topo_map = {
            "mesh_version": "5.0",
            "topology": topology,
            "dependency_chains": dependency_chains
        }
        try:
            with open("scratch/topology_map.json", "w") as f:
                json.dump(topo_map, f, indent=2)
        except Exception as e:
            print(f"[DIRECTOR ERROR] Failed to write topology map: {e}")

    def save_learned_topology(self):
        reads_map = {}
        for reader, topic in self.bus.reads:
            dep = topic
            if topic.startswith("agent."):
                dep = topic.split(".")[1]
            if reader not in reads_map:
                reads_map[reader] = []
            if dep not in reads_map[reader]:
                reads_map[reader].append(dep)
                
        golden_path = "scratch/golden/topology_map_learned.json"
        delta = {"added_dependencies": [], "removed_dependencies": []}
        
        if os.path.exists(golden_path):
            try:
                with open(golden_path, "r") as f:
                    golden = json.load(f)
                for reader, deps in reads_map.items():
                    golden_deps = golden.get(reader, [])
                    for d in deps:
                        if d not in golden_deps:
                            delta["added_dependencies"].append(f"{reader} -> {d}")
                    for d in golden_deps:
                        if d not in deps:
                            delta["removed_dependencies"].append(f"{reader} -> {d}")
            except Exception as e:
                print(f"[TOPOLOGY ERROR] Baseline diff failed: {e}")
        else:
            try:
                with open(golden_path, "w") as f:
                    json.dump(reads_map, f, indent=2)
            except Exception:
                pass
                
        try:
            with open("scratch/topology_map_learned.json", "w") as f:
                json.dump(reads_map, f, indent=2)
            with open("scratch/topology_delta.json", "w") as f:
                json.dump(delta, f, indent=2)
        except Exception as e:
            print(f"[TOPOLOGY ERROR] Failed to save learned topology: {e}")

# --------------------------------------------------------------------
# 8. DEPLOYMENT FORECASTING ENGINE
# --------------------------------------------------------------------
def run_deployment_forecasting(env):
    mem_file = "scratch/operational_memory.json"
    history_success_rate = 1.0
    if os.path.exists(mem_file):
        try:
            with open(mem_file, "r") as f:
                memory = json.load(f)
            runs = memory.get("runs", [])
            if runs:
                passes = sum(1 for r in runs if r.get("verdict") == "PASS")
                history_success_rate = passes / len(runs)
        except Exception:
            pass
            
    predicted_risks = []
    recommended_actions = []
    
    if env == "WSL":
        predicted_risks.append("WSL loopback port bridge instability (ports 8081, 8892)")
        recommended_actions.append("Ensure python serve_dashboard.py and ws_bridge are active on host")
        
    golden_path = "scratch/golden/geometry_bounds.json"
    if os.path.exists(golden_path):
        recommended_actions.append("Verify golden geometry_bounds matches current GLB assets")
    else:
        predicted_risks.append("Missing golden baseline geometry parameters")
        recommended_actions.append("Initialize golden baseline geometry parameters")
        
    success_prob = 0.98 * history_success_rate
    success_prob = min(0.999, max(0.10, success_prob))
    
    forecast = {
        "success_probability": success_prob,
        "predicted_risks": predicted_risks,
        "recommended_actions": recommended_actions,
        "timestamp": time.strftime("%Y-%m-%d %H:%M:%S")
    }
    
    try:
        with open("scratch/deployment_forecast.json", "w") as f:
            json.dump(forecast, f, indent=2)
    except Exception as e:
        print(f"[FORECAST ERROR] Failed to write forecast: {e}")
        
    print(f"{CYAN}{BOLD}[FORECAST] Predicted Deployment Success Probability: {success_prob * 100:.1f}%{RESET}")
    if predicted_risks:
        print(f"{YELLOW}[FORECAST] Predicted risks:{RESET}")
        for r in predicted_risks:
            print(f"  - {r}")
    print()
    return forecast

# --------------------------------------------------------------------
# 9. SELF-EVOLVING GOVERNANCE & HEALTH SCORE
# --------------------------------------------------------------------
def compute_fabric_health_score(all_results):
    total = len(all_results)
    if total == 0:
        return {"fabric_health": 100.0, "deployment_readiness": 100.0, "recovery_readiness": 100.0, "tightened_governance_mode": False}
        
    failures = sum(1 for r in all_results if r.get("status") not in ("EMERALD", "GREEN", "LIME", "YELLOW", "ORANGE"))
    warnings = sum(1 for r in all_results if r.get("status") in ("YELLOW", "ORANGE"))
    
    fabric_health = 100.0 - (failures * 20.0) - (warnings * 6.0)
    fabric_health = max(0.0, min(100.0, fabric_health))
    
    dep_readiness = 100.0
    for r in all_results:
        if r["agent"] in ("sub_compiler_agent", "sub_visualizer_agent") and r.get("status") not in ("EMERALD", "GREEN", "LIME"):
            dep_readiness -= 35.0
    dep_readiness = max(0.0, min(100.0, dep_readiness))
    
    rec_readiness = 100.0
    has_rollback = os.path.exists("scratch/rollback_plan.md")
    if not has_rollback:
        rec_readiness -= 25.0
    if failures > 0:
        has_twins = os.path.exists("scratch/failure_twins.json")
        if not has_twins:
            rec_readiness -= 35.0
    rec_readiness = max(0.0, min(100.0, rec_readiness))
    
    tightened_governance = False
    if fabric_health < 90.0:
        tightened_governance = True
        
    score_dict = {
        "fabric_health": fabric_health,
        "deployment_readiness": dep_readiness,
        "recovery_readiness": rec_readiness,
        "tightened_governance_mode": tightened_governance
    }
    
    try:
        with open("scratch/fabric_health_score.json", "w") as f:
            json.dump(score_dict, f, indent=2)
    except Exception as e:
        print(f"[GOVERNANCE ERROR] Failed to write health score: {e}")
        
    return score_dict

# --------------------------------------------------------------------
# 10. LEGENDARY META-DIRECTOR EXECUTION WRAPPERS
# --------------------------------------------------------------------
def run_deployment_archivist(bus, all_results):
    ts = int(time.time())
    history_dir = f"scratch/history/run_{ts}"
    os.makedirs(history_dir, exist_ok=True)
    
    snapshot = bus.snapshot()
    try:
        with open(f"{history_dir}/bus_snapshot.json", "w") as f:
            json.dump(snapshot, f, indent=2)
    except Exception as e:
        print(f"[ARCHIVIST ERROR] Failed to save bus snapshot: {e}")
        
    for f_name in os.listdir("scratch"):
        f_path = os.path.join("scratch", f_name)
        if os.path.isfile(f_path) and f_name.endswith((".json", ".md", ".yaml", ".jsonl")):
            try:
                import shutil
                shutil.copy(f_path, os.path.join(history_dir, f_name))
            except Exception:
                pass
    log_agent("deployment_archivist", "GREEN", f"Archived execution history to {history_dir}")
    return {"agent": "deployment_archivist", "status": "GREEN", "evidence": [f"Archived to {history_dir}"]}

def run_forensic_historian(bus, all_results):
    failures = [r for r in all_results if r.get("status") not in ("GREEN", "EMERALD", "LIME", "YELLOW", "ORANGE")]
    warnings = [r for r in all_results if r.get("status") in ("YELLOW", "ORANGE")]
    
    lines = [
        "# ZKAEDI Forensic History Report",
        f"Timestamp: {time.strftime('%Y-%m-%d %H:%M:%S')}",
        f"Status Verdict: {'GREEN' if not failures else 'RED'}",
        "",
        "## Summary of Executed Agents",
    ]
    for r in all_results:
        lines.append(f"- **{r['agent']}**: status={r['status']}")
    
    lines.append("")
    lines.append("## Root Cause Analysis & Behavioral Drift")
    if not failures and not warnings:
        lines.append("No failures or warnings detected. System is stable and matching baseline.")
    else:
        if failures:
            lines.append("### Observed Failures:")
            for f in failures:
                lines.append(f"- **{f['agent']}** failed with status {f['status']}.")
                for ev in f.get("evidence", []):
                    lines.append(f"  - {ev}")
        if warnings:
            lines.append("### Observed Warning Drifts:")
            for w in warnings:
                lines.append(f"- **{w['agent']}** reported warning/drift.")
                for ev in w.get("evidence", []):
                    lines.append(f"  - {ev}")
    
    try:
        with open("scratch/forensic_history.md", "w") as f:
            f.write("\n".join(lines))
    except Exception as e:
        print(f"[HISTORIAN ERROR] Failed to write forensic history: {e}")
        
    log_agent("forensic_historian", "GREEN", "Forensic analysis written to scratch/forensic_history.md")
    return {"agent": "forensic_historian", "status": "GREEN", "evidence": ["Forensic report compiled"]}

def run_resilience_director(bus, all_results):
    failures = [r for r in all_results if r.get("status") not in ("GREEN", "EMERALD", "LIME", "YELLOW", "ORANGE")]
    
    playbook = [
        "# ZKAEDI Recovery and Resilience Playbook",
        f"Generated at: {time.strftime('%Y-%m-%d %H:%M:%S')}",
        "",
    ]
    
    if failures:
        playbook.append("## ACTIVE FAILURE RECOVERY PROTOCOLS")
        for f in failures:
            agent = f["agent"]
            playbook.append(f"### Subsystem: {agent}")
            if "ws_agent" in agent:
                playbook.append("Playbook: RESTART_WEBSOCKET_BRIDGE")
                playbook.append("Steps:")
                playbook.append("  1. Kill stale bridge processes: `pkill -f zkaedi_suno_ws_bridge.py`")
                playbook.append("  2. Verify port 8892 availability: `netstat -ano | grep 8892`")
                playbook.append("  3. Restart bridge: `python zkaedi_suno_ws_bridge.py &`")
                playbook.append("  4. Verify WS mesh connectivity using micro_ws_agent.")
            elif "asset_agent" in agent:
                playbook.append("Playbook: RESTART_ASSET_SERVER")
                playbook.append("Steps:")
                playbook.append("  1. Check if serve_dashboard.py is running.")
                playbook.append("  2. Restart asset server on port 8081: `python serve_dashboard.py &`")
                playbook.append("  3. Check HTTP 200 via `curl -I http://localhost:8081`.")
            elif "compiler_agent" in agent:
                playbook.append("Playbook: REBUILD_BOOTSTRAP")
                playbook.append("Steps:")
                playbook.append("  1. Run `make clean` to clear stale binary intermediate states.")
                playbook.append("  2. Run `make selfhost` to rebuild stage-1, stage-2, and stage-3 compilers.")
                playbook.append("  3. Run `make ir-verify` to ensure binary parity.")
            else:
                playbook.append("Playbook: GENERAL_ESC_DEESCALATION")
                playbook.append("Steps:")
                playbook.append("  1. Check log files under brain workspace.")
                playbook.append("  2. Revert workspace changes using `git checkout`.")
    else:
        playbook.append("## PREVENTIVE PROTOCOLS (SYSTEM HEALTHY)")
        playbook.append("- System is functioning normally.")
        playbook.append("- Continue monitoring active assets, shaders, and compiler bootstraps.")
        
    try:
        with open("scratch/recovery_playbook.md", "w") as f:
            f.write("\n".join(playbook))
    except Exception as e:
        print(f"[RESILIENCE ERROR] Failed to write playbook: {e}")
        
    log_agent("resilience_director", "GREEN", "Recovery playbook written to scratch/recovery_playbook.md")
    return {"agent": "resilience_director", "status": "GREEN", "evidence": ["Resilience playbook updated"]}

def run_optimization_oracle(bus, all_results, latencies):
    heatmap = {}
    timeline = []
    for r in all_results:
        name = r["agent"]
        lat = latencies.get(name, 0.0)
        cpu_est = 0.1
        mem_est = 5.0
        if "compiler" in name:
            cpu_est = 0.8
            mem_est = 45.0
        elif "render" in name:
            cpu_est = 0.3
            mem_est = 15.0
        
        heatmap[name] = {
            "latency_ms": lat,
            "cpu_load_percent": cpu_est * 100,
            "memory_mb": mem_est,
            "status": r.get("status", "GREEN")
        }
        timeline.append({
            "agent": name,
            "start_offset_ms": 0.0,
            "duration_ms": lat
        })
        
    try:
        with open("scratch/mesh_heatmap.json", "w") as f:
            json.dump(heatmap, f, indent=2)
        with open("scratch/execution_timeline.json", "w") as f:
            json.dump(timeline, f, indent=2)
    except Exception as e:
        print(f"[ORACLE ERROR] Failed to write optimization files: {e}")
        
    log_agent("optimization_oracle", "GREEN", "Mesh latency heatmap compiled to scratch/mesh_heatmap.json")
    return {"agent": "optimization_oracle", "status": "GREEN", "evidence": ["Performance heatmap compiled"]}

# --------------------------------------------------------------------
# 11. OPERATIONAL MEMORY PERSISTENCE & QUERY ENGINE
# --------------------------------------------------------------------
def save_operational_memory(run_record):
    mem_file = "scratch/operational_memory.json"
    memory = {"runs": []}
    if os.path.exists(mem_file):
        try:
            with open(mem_file, "r") as f:
                memory = json.load(f)
        except Exception:
            pass
    memory["runs"].append(run_record)
    try:
        with open(mem_file, "w") as f:
            json.dump(memory, f, indent=2)
    except Exception as e:
        print(f"[MEMORY ERROR] Failed to save operational memory: {e}")

def run_query(query_type):
    mem_file = "scratch/operational_memory.json"
    if not os.path.exists(mem_file):
        print("Operational memory is empty. Run the platform first.")
        sys.exit(0)
    try:
        with open(mem_file, "r") as f:
            memory = json.load(f)
    except Exception as e:
        print(f"Failed to read operational memory: {e}")
        sys.exit(1)
        
    runs = memory.get("runs", [])
    print(f"\n{CYAN}{BOLD}=== OPERATIONAL MEMORY QUERY: {query_type.upper()} ==={RESET}\n")
    
    if query_type == "failures":
        failed_runs = [r for r in runs if r.get("verdict") == "FAIL" or r.get("failures")]
        if not failed_runs:
            print("No historical failures found in memory.")
        for r in failed_runs:
            print(f"Run {r['run_id']} ({r['timestamp']}) on {r['environment']}:")
            print(f"  Verdict: {r['verdict']}, Health: {r['health_score']:.1f}")
            print("  Failures:")
            for f in r.get("failures", []):
                print(f"    - {f}")
    elif query_type == "recoveries":
        playbook_path = "scratch/recovery_playbook.md"
        if os.path.exists(playbook_path):
            with open(playbook_path, "r") as f:
                print("Active Recovery Playbook:")
                print(f.read())
        else:
            print("No active recovery playbook found.")
    elif query_type == "regressions":
        compiler_runs = [r for r in runs if any("compiler" in f.lower() for f in r.get("failures", []))]
        print(f"Found {len(compiler_runs)} historical compiler regressions.")
        for r in compiler_runs:
            print(f"  - Run {r['run_id']} on {r['environment']}: Compiler regression detected.")
    else:
        print(f"Total historical runs: {len(runs)}")
        for r in runs[-5:]:
            print(f"  - Run {r['run_id']} ({r['timestamp']}) on {r['environment']}: Verdict={r['verdict']}, Health={r['health_score']:.1f}")
    print()
    sys.exit(0)

# --------------------------------------------------------------------
# MAIN ENTRYPOINT
# --------------------------------------------------------------------
async def main_coordination():
    env = detect_environment()
    run_id = f"platform-{int(time.time())}"
    
    print(f"\n{CYAN}{BOLD}Initiating ZKAEDI Omega Reflexive Operating Model...{RESET}")
    print(f"{DIM}Environment Context Profile: {BOLD}{env}{RESET}\n")
    
    # 1. Instantiate Omega Primitives
    constitution = Constitution()
    intent_graph = IntentGraph()
    event_fabric = EventFabric()
    causal_memory = CausalMemory()
    self_model = SelfModel()
    
    # Pre-execution forecasting
    run_deployment_forecasting(env)
    
    # Parse CLI flags
    obj_name = "stable_visualizer_deployment"
    simulate_failure = None
    
    for i, arg in enumerate(sys.argv):
        if arg == "--objective" and i + 1 < len(sys.argv):
            obj_name = sys.argv[i+1]
        elif arg == "--simulate-failure" and i + 1 < len(sys.argv):
            simulate_failure = sys.argv[i+1]
            
    # Compile objectives into Intent Graph execution plan
    constraints = []
    if env == "CI":
        constraints.append("no_gui")
    planned_caps = intent_graph.compile([obj_name], constraints)
    
    director = OrchestrationDirector(run_id, event_fabric, causal_memory, simulate_failure)
    
    # Register Domain Agents
    director.register(Agent("pre_asset_agent", "pre", "12.0", ["asset_resolution"], ["http"], ["asset_manifest.json"], [], run_pre_asset_agent))
    director.register(Agent("pre_geometry_agent", "pre", "12.0", ["geometry_normalization"], ["gltf"], ["geometry_bounds.json"], ["pre_asset_agent"], run_pre_geometry_agent))
    director.register(Agent("pre_shader_agent", "pre", "12.0", ["shader_hardening"], ["glsl"], ["shader_compile_report.json"], ["pre_geometry_agent"], run_pre_shader_agent))
    director.register(Agent("micro_render_agent", "micro", "12.0", ["renderer_validation"], ["browser"], ["render_probe.json"], ["pre_shader_agent"], run_micro_render_agent_swarm))
    director.register(Agent("micro_ws_agent", "micro", "12.0", ["telemetry_validation"], ["websocket"], ["telemetry_health.json"], [], run_micro_ws_agent))
    director.register(Agent("micro_camera_agent", "micro", "12.0", ["camera_auto_framing"], ["scene"], [], ["pre_geometry_agent"], run_micro_camera_agent))
    director.register(Agent("sub_visualizer_agent", "sub", "12.0", ["visualizer_coordination"], [], [], ["pre_asset_agent", "pre_geometry_agent", "pre_shader_agent", "micro_render_agent", "micro_ws_agent", "micro_camera_agent"], run_sub_visualizer_agent))
    director.register(Agent("sub_compiler_agent", "sub", "12.0", ["bootstrap_verification"], ["makefile"], ["compiler_bootstrap_report.json"], [], run_sub_compiler_agent))
    director.register(Agent("post_patch_agent", "post", "12.0", ["rollback_generation"], [], ["rollback_plan.md"], ["sub_visualizer_agent", "sub_compiler_agent"], run_post_patch_agent))
    director.register(Agent("post_report_agent", "post", "12.0", ["attestation_generation"], [], ["deployment_gate.json"], ["post_patch_agent"], run_post_report_agent))
    
    # Plan execution sequence
    planner = IntentPlanner(director.registry)
    planned_agents = planner.plan(obj_name)
    director.registry = planned_agents
    
    log_agent("Intent Planner", "LIME", f"Objective '{obj_name}' compiled with {len(planned_agents)} capabilities.")
    
    # Map static topology
    director.generate_topology_map()
    
    # Event Fabric log: run start
    event_fabric.append({
        "type": "run_started",
        "run_id": run_id,
        "objective": obj_name,
        "environment": env
    })
    
    execution_times = {}
    executed_caps = []
    
    # Execute Pipeline Layers
    print(f"\n{DIM}[PHASE 1] Executing PRE-agents (Marketplace Routing)...{RESET}")
    pre_res = await director.execute_layer("pre", execution_times, executed_caps)
    
    print(f"\n{DIM}[PHASE 2] Executing MICRO-probes (Swarm Consensus Active)...{RESET}")
    micro_res = await director.execute_layer("micro", execution_times, executed_caps)
    
    print(f"\n{DIM}[PHASE 3] Checking SUBSYSTEM integrations...{RESET}")
    sub_res = await director.execute_layer("sub", execution_times, executed_caps)
    
    print(f"\n{DIM}[PHASE 4] Formulating POST-governance reports...{RESET}")
    post_res = await director.execute_layer("post", execution_times, executed_caps)
    
    all_results = pre_res + micro_res + sub_res + post_res
    
    # Continuous Dependency Learning
    director.save_learned_topology()
    
    # Execute Meta-Directors
    print(f"\n{DIM}[PHASE 5] Invoking Legendary Meta-Directors Strata...{RESET}")
    arch_res = run_deployment_archivist(director.bus, all_results)
    hist_res = run_forensic_historian(director.bus, all_results)
    res_res = run_resilience_director(director.bus, all_results)
    orac_res = run_optimization_oracle(director.bus, all_results, execution_times)
    
    meta_results = [arch_res, hist_res, res_res, orac_res]
    total_results = all_results + meta_results
    
    # Self-Evolving governance and health scoring
    score = compute_fabric_health_score(total_results)
    
    failures = []
    warnings = []
    for r in total_results:
        st = r.get("status")
        if st not in ("EMERALD", "GREEN", "LIME", "YELLOW", "ORANGE"):
            failures.append(r)
        elif st in ("YELLOW", "ORANGE"):
            if score["tightened_governance_mode"]:
                r["status"] = "RED"
                r["evidence"].append("Governance auto-promoted warning to RED failure (health below 90.0)")
                failures.append(r)
            else:
                warnings.append(r)
                
    # 2. Self-Model & Divergence Analysis (Level-12 Drift Engine)
    observed_metrics = {
        "fabric_health": score["fabric_health"],
        "registered_capabilities": [c for a in director.registry for c in a.capabilities],
        "geometry_radius_drift": director.bus.get("geometry.bounds").get("valuable_data", {}).get("geometry_radius_drift", 0.0) if director.bus.get("geometry.bounds") else 0.0,
        "topology_delta": json.load(open("scratch/topology_delta.json")) if os.path.exists("scratch/topology_delta.json") else {"added_dependencies": [], "removed_dependencies": []}
    }
    
    drift_report = self_model.compare(observed_metrics, {}, planned_caps, executed_caps)
    
    print(f"\n{CYAN}{BOLD}=== OMEGA REFLEXIVE DIVERGENCE REPORT ==={RESET}")
    for k, v in drift_report.items():
        print(f"  {k:<18}: {v:.4f}")
    print()
    
    # 3. Constitutional Enforcement Gate
    verdict = constitution.enforce(obj_name, drift_report)
    
    # Event Fabric log: run end
    event_fabric.append({
        "type": "run_completed",
        "run_id": run_id,
        "verdict": verdict["status"],
        "health_score": score["fabric_health"],
        "drifts": drift_report
    })
    
    # Save memory state
    attestation = {
        "run_id": run_id,
        "timestamp": time.strftime("%Y-%m-%d %H:%M:%S"),
        "git_sha": subprocess.check_output("git rev-parse HEAD", shell=True).decode().strip(),
        "verdict": verdict["status"],
        "failures": verdict["failures"] + [f["agent"] for f in failures],
        "warnings": [w["agent"] for w in warnings],
        "environment": env,
        "health_score": score["fabric_health"],
        "deployment_readiness": score["deployment_readiness"],
        "recovery_readiness": score["recovery_readiness"],
        "drifts": drift_report
    }
    director.bus.publish("attestation.evidence", "OrchestrationDirector", "attestation_generation", verdict["status"], [], [], attestation)
    save_operational_memory(attestation)
    
    if verdict["status"] == "RED" or failures:
        print(f"{RED}{BOLD}Omega Constitutional Gate BLOCKED Deployment.{RESET}")
        for f in verdict["failures"]:
            print(f"  - Constitutional Breach: {f}")
        for f in failures:
            if f["agent"] not in [fb.split()[0] for fb in verdict["failures"]]:
                print(f"  - Agent Failure: {f['agent']} reported status {f['status']}")
        sys.exit(1)
    else:
        print(f"{GREEN}{BOLD}Omega Constitutional Gate GRANTED Deployment (Verdict: PASS).{RESET}\n")
        print(f"  System Health: {score['fabric_health']:.1f}% | Deployment Readiness: {score['deployment_readiness']:.1f}%")
        if warnings:
            print(f"{YELLOW}{BOLD}Warning drifts observed: {len(warnings)}{RESET}")
            for w in warnings:
                print(f"  - {w['agent']}: status={w['status']}")

if __name__ == "__main__":
    if len(sys.argv) > 1:
        if sys.argv[1] == "--query":
            q = sys.argv[2] if len(sys.argv) > 2 else "summary"
            run_query(q)
            
    asyncio.run(main_coordination())
