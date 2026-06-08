import json
import os

class AgentDispatcher:
    def __init__(self, config_path="agents-skills-grouped.json"):
        self.config_path = config_path
        self.agents_data = self._load_or_create_config()

    def _load_or_create_config(self):
        default_data = {
            "agents": [
                {
                    "name": "AdaptiveSelfHealer",
                    "skills": ["c-compilation-debug", "self-healing", "pointer-overflow-resolution", "ast-patching"],
                    "phases": ["codegen", "preprocessor", "parsing"],
                    "confidence_boost": 0.95
                },
                {
                    "name": "SmartContractHamiltonian",
                    "skills": ["solidity-audit", "evm-stack-check", "mstore-simulation", "vulnerability-hunting"],
                    "phases": ["codegen", "validation"],
                    "confidence_boost": 0.90
                },
                {
                    "name": "QuantumValidationEngine",
                    "skills": ["verification", "invariance-check", "telemetry-verification"],
                    "phases": ["validation"],
                    "confidence_boost": 0.85
                }
            ]
        }
        if not os.path.exists(self.config_path):
            try:
                with open(self.config_path, "w", encoding="utf-8") as f:
                    json.dump(default_data, f, indent=2)
            except Exception as e:
                print(f"[WARN] Failed to write config {self.config_path}: {e}")
            return default_data
        
        try:
            with open(self.config_path, "r", encoding="utf-8") as f:
                return json.load(f)
        except Exception as e:
            print(f"[WARN] Failed to read config {self.config_path}, using defaults: {e}")
            return default_data

    def match_agent(self, file_path, error_log, phase):
        # Perform scoring logic to find the best matching agent
        best_agent = None
        highest_score = -1.0
        matched_skills = []

        error_lower = error_log.lower()
        file_ext = os.path.splitext(file_path)[1].lower()

        for agent in self.agents_data.get("agents", []):
            score = 0.0
            agent_skills = agent.get("skills", [])
            agent_phases = agent.get("phases", [])

            # Phase match
            if phase in agent_phases:
                score += 2.0

            # File extension heuristics
            if file_ext == ".c" and agent["name"] in ["AdaptiveSelfHealer", "SmartContractHamiltonian"]:
                score += 1.0
            elif file_ext in [".sol", ".evm"] and agent["name"] == "SmartContractHamiltonian":
                score += 2.0

            # Error log keyword matches
            skills_for_this_agent = []
            if "segmentation fault" in error_lower or "stack overflow" in error_lower:
                if "pointer-overflow-resolution" in agent_skills or "evm-stack-check" in agent_skills:
                    score += 3.0
                    skills_for_this_agent.append("pointer-overflow-resolution" if "pointer-overflow-resolution" in agent_skills else "evm-stack-check")
            if "mstore" in error_lower:
                if "mstore-simulation" in agent_skills:
                    score += 3.0
                    skills_for_this_agent.append("mstore-simulation")
            if "audit" in error_lower or "vulnerability" in error_lower:
                if "vulnerability-hunting" in agent_skills or "solidity-audit" in agent_skills:
                    score += 2.0
                    skills_for_this_agent.append("vulnerability-hunting" if "vulnerability-hunting" in agent_skills else "solidity-audit")

            # Scale score by agent's own confidence boost
            score *= agent.get("confidence_boost", 1.0)

            if score > highest_score:
                highest_score = score
                best_agent = agent
                matched_skills = list(set(agent_skills + skills_for_this_agent))

        if not best_agent:
            # Fallback
            best_agent = self.agents_data["agents"][0]
            matched_skills = best_agent["skills"]
            highest_score = 0.5

        return {
            "agent": best_agent["name"],
            "confidence": min(1.0, highest_score / 10.0 + 0.5),
            "matched_skills": matched_skills,
            "routing_criteria": {
                "phase": phase,
                "file_path": file_path,
                "score": highest_score
            }
        }
