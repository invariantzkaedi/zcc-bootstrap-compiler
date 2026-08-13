import json
import os
import subprocess
import sys
import unittest
import shutil
import copy
import tempfile

class TestValidatorHardening(unittest.TestCase):
    def setUp(self):
        self.test_dir = tempfile.mkdtemp(prefix="zcc_test_")
        self.dataset_path = os.path.join(self.test_dir, "train_maxed_validated.parquet")
        try:
            import pyarrow as pa
            import pyarrow.parquet as pq
            table = pa.Table.from_pydict({"prompt": ["sample prompt"], "chosen": ["sample chosen text"], "rejected": ["sample rejected text"]})
            pq.write_table(table, self.dataset_path)
        except Exception:
            with open(self.dataset_path, "wb") as f:
                f.write(b"PAR1dummybytesPAR1")

        self.base_state = {
            "epoch": 1.0,
            "global_step": 100,
            "log_history": [
                {"step": 80, "loss": 0.55, "rewards/margins": 0.02},
                {"step": 85, "loss": 0.54, "rewards/margins": 0.025},
                {"step": 90, "loss": 0.53, "rewards/margins": 0.03},
                {"step": 95, "loss": 0.52, "rewards/margins": 0.035},
                {"step": 100, "loss": 0.51, "rewards/margins": 0.04}
            ]
        }
        # Add default evaluations to satisfy min-eval-records = 3
        self.base_state["log_history"].extend([
            {"step": 90, "eval_loss": 0.54, "eval_preference_margin_mean": 0.032, "eval_preference_margin_median": 0.031, "eval_positive_margin_rate": 0.82},
            {"step": 95, "eval_loss": 0.53, "eval_preference_margin_mean": 0.035, "eval_preference_margin_median": 0.033, "eval_positive_margin_rate": 0.84},
            {"step": 100, "eval_loss": 0.52, "eval_preference_margin_mean": 0.038, "eval_preference_margin_median": 0.035, "eval_positive_margin_rate": 0.85}
        ])
        
        # Create a valid split manifest with no overlap
        self.manifest_path = os.path.join(self.test_dir, "split_manifest.json")
        train_ids = [f"t{i}" for i in range(100)]
        eval_ids = [f"e{i}" for i in range(100)]
        with open(self.manifest_path, "w") as f:
            json.dump({"train": train_ids, "eval": eval_ids}, f)
            
    def tearDown(self):
        if os.path.exists(self.test_dir):
            shutil.rmtree(self.test_dir)
            
    def run_val(self, state, args=None, expected_rc=None):
        state_path = os.path.join(self.test_dir, "trainer_state.json")
        with open(state_path, "w") as f:
            json.dump(state, f)
            
        out_path = os.path.join(self.test_dir, "verdict.json")
        # Resolve validate_training_health.py path relative to this file
        test_dir_path = os.path.dirname(os.path.abspath(__file__))
        script_path = os.path.abspath(os.path.join(test_dir_path, "..", "validate_training_health.py"))
        
        cmd = [sys.executable, script_path, state_path, "--out", out_path]
        if args:
            cmd.extend(args)
        if "--dataset-path" not in cmd:
            cmd.extend(["--dataset-path", self.dataset_path])
            
        env = os.environ.copy()
        env["ZKAEDI_SAFE_BASE"] = "/"
        res = subprocess.run(cmd, capture_output=True, text=True, env=env)
        
        # Try to read verdict
        verdict = None
        if os.path.exists(out_path):
            with open(out_path, "r") as f:
                verdict = json.load(f)
                
        if expected_rc is not None:
            self.assertEqual(res.returncode, expected_rc, f"Exit code {res.returncode} != {expected_rc}. Stdout: {res.stdout}, Stderr: {res.stderr}")
            
        return res, verdict


    def test_step_boolean_rejection(self):
        state = {
            "epoch": 1.0,
            "global_step": 1,
            "log_history": [{"step": True, "loss": 0.5, "rewards/margins": 0.02}]
        }
        res, verdict = self.run_val(state, expected_rc=4) # Parse step boolean throws 4
        self.assertEqual(verdict["release_verdict"], "REJECTED")

    def test_step_fractional_rejection(self):
        state = {
            "epoch": 1.0,
            "global_step": 1,
            "log_history": [{"step": 10.5, "loss": 0.5, "rewards/margins": 0.02}]
        }
        res, verdict = self.run_val(state, expected_rc=4)
        self.assertEqual(verdict["release_verdict"], "REJECTED")

    def test_step_negative_rejection(self):
        state = {
            "epoch": 1.0,
            "global_step": 1,
            "log_history": [{"step": -1, "loss": 0.5, "rewards/margins": 0.02}]
        }
        res, verdict = self.run_val(state, expected_rc=4)
        self.assertEqual(verdict["release_verdict"], "REJECTED")

    def test_eval_merging_success(self):
        # Step 100 eval split into two log records
        state = {
            "epoch": 1.0,
            "global_step": 100,
            "log_history": [
                {"step": 80, "loss": 0.55, "rewards/margins": 0.02},
                {"step": 85, "loss": 0.54, "rewards/margins": 0.025},
                {"step": 90, "loss": 0.53, "rewards/margins": 0.03},
                {"step": 95, "loss": 0.52, "rewards/margins": 0.035},
                {"step": 100, "loss": 0.51, "rewards/margins": 0.04},
                
                # Eval steps
                {"step": 90, "eval_loss": 0.54, "eval_preference_margin_mean": 0.032, "eval_preference_margin_median": 0.031, "eval_positive_margin_rate": 0.82},
                {"step": 95, "eval_loss": 0.53, "eval_preference_margin_mean": 0.035, "eval_preference_margin_median": 0.033, "eval_positive_margin_rate": 0.84},
                
                # Split step 100 eval
                {"step": 100, "eval_loss": 0.52, "eval_positive_margin_rate": 0.85},
                {"step": 100, "eval_preference_margin_mean": 0.038, "eval_preference_margin_median": 0.035}
            ]
        }
        args = [
            "--dataset-hash", "hash", "--checkpoint-digest", "dig", "--model-identity", "id",
            "--seed", "123", "--evaluator-identity", "eval", "--split-manifest", self.manifest_path
        ]
        res, verdict = self.run_val(state, args, expected_rc=0)
        self.assertEqual(verdict["release_verdict"], "METRIC_GATES_PASSED")
        self.assertEqual(verdict["held_out_alignment_gate"]["metrics"]["eval_preference_margin_mean"], 0.038)

    def test_eval_merging_conflict_rejection(self):
        # Step 100 eval contains conflicting eval loss values
        state = {
            "epoch": 1.0,
            "global_step": 100,
            "log_history": [
                {"step": 80, "loss": 0.55, "rewards/margins": 0.02},
                {"step": 85, "loss": 0.54, "rewards/margins": 0.025},
                {"step": 90, "loss": 0.53, "rewards/margins": 0.03},
                {"step": 95, "loss": 0.52, "rewards/margins": 0.035},
                {"step": 100, "loss": 0.51, "rewards/margins": 0.04},
                
                {"step": 90, "eval_loss": 0.54, "eval_preference_margin_mean": 0.032, "eval_preference_margin_median": 0.031, "eval_positive_margin_rate": 0.82},
                {"step": 95, "eval_loss": 0.53, "eval_preference_margin_mean": 0.035, "eval_preference_margin_median": 0.033, "eval_positive_margin_rate": 0.84},
                
                {"step": 100, "eval_loss": 0.52, "eval_preference_margin_mean": 0.038, "eval_preference_margin_median": 0.035, "eval_positive_margin_rate": 0.85},
                {"step": 100, "eval_loss": 0.61} # conflicting loss value for step 100
            ]
        }
        res, verdict = self.run_val(state, expected_rc=1)
        self.assertEqual(verdict["release_verdict"], "REJECTED")
        self.assertEqual(verdict["held_out_alignment_gate"]["status"], "FAIL")

    def test_positive_margin_rate_out_of_range_low(self):
        state = copy.deepcopy(self.base_state)
        # Edit positive margin rate to -0.1
        state["log_history"][-1]["eval_positive_margin_rate"] = -0.1
        res, verdict = self.run_val(state, expected_rc=4)
        self.assertEqual(verdict["release_verdict"], "REJECTED")

    def test_positive_margin_rate_out_of_range_high(self):
        state = copy.deepcopy(self.base_state)
        state["log_history"][-1]["eval_positive_margin_rate"] = 1.5
        res, verdict = self.run_val(state, expected_rc=4)
        self.assertEqual(verdict["release_verdict"], "REJECTED")

    def test_single_eval_record_fails_min_eval_3(self):
        state = {
            "epoch": 1.0,
            "global_step": 100,
            "log_history": [
                {"step": 80, "loss": 0.55, "rewards/margins": 0.02},
                {"step": 85, "loss": 0.54, "rewards/margins": 0.025},
                {"step": 90, "loss": 0.53, "rewards/margins": 0.03},
                {"step": 95, "loss": 0.52, "rewards/margins": 0.035},
                {"step": 100, "loss": 0.51, "rewards/margins": 0.04},
                # Only 1 eval step logged instead of 3
                {"step": 100, "eval_loss": 0.52, "eval_preference_margin_mean": 0.038, "eval_preference_margin_median": 0.035, "eval_positive_margin_rate": 0.85}
            ]
        }
        # Run with default --min-eval-records 3
        res, verdict = self.run_val(state, expected_rc=1)
        self.assertEqual(verdict["release_verdict"], "REJECTED")
        self.assertEqual(verdict["held_out_alignment_gate"]["status"], "FAIL")
        self.assertEqual(verdict["held_out_alignment_gate"]["assurance_level"], "SMOKE_TEST_ONLY")

    def test_single_eval_record_passes_smoke_test(self):
        state = {
            "epoch": 1.0,
            "global_step": 100,
            "log_history": [
                {"step": 80, "loss": 0.55, "rewards/margins": 0.02},
                {"step": 85, "loss": 0.54, "rewards/margins": 0.025},
                {"step": 90, "loss": 0.53, "rewards/margins": 0.03},
                {"step": 95, "loss": 0.52, "rewards/margins": 0.035},
                {"step": 100, "loss": 0.51, "rewards/margins": 0.04},
                {"step": 100, "eval_loss": 0.52, "eval_preference_margin_mean": 0.038, "eval_preference_margin_median": 0.035, "eval_positive_margin_rate": 0.85}
            ]
        }
        # Run with --min-eval-records 1, which reduces required to 1
        args = [
            "--min-eval-records", "1",
            "--dataset-hash", "hash", "--checkpoint-digest", "dig", "--model-identity", "id",
            "--seed", "123", "--evaluator-identity", "eval", "--split-manifest", self.manifest_path
        ]
        res, verdict = self.run_val(state, args, expected_rc=0)
        self.assertEqual(verdict["release_verdict"], "METRIC_GATES_PASSED")
        self.assertEqual(verdict["held_out_alignment_gate"]["assurance_level"], "SMOKE_TEST_ONLY")

    def test_missing_provenance_labels_unattested(self):
        state = copy.deepcopy(self.base_state)
        # Numerical gates pass, but split_manifest or seed is missing (default parameters "UNKNOWN" are used)
        res, verdict = self.run_val(state, expected_rc=2) # Expect exit code 2!
        self.assertEqual(verdict["release_verdict"], "METRIC_GATES_PASSED_UNATTESTED")
        self.assertEqual(verdict["provenance_gate"]["required_fields_complete"], False)

    def test_train_eval_manifest_overlap_fails(self):
        overlap_manifest = os.path.join(self.test_dir, "overlap_manifest.json")
        with open(overlap_manifest, "w") as f:
            # Overlap: "id2" is in both splits
            json.dump({"train": ["id1", "id2"], "eval": ["id2", "id3"]}, f)
            
        state = copy.deepcopy(self.base_state)
        args = [
            "--dataset-hash", "hash", "--checkpoint-digest", "dig", "--model-identity", "id",
            "--seed", "123", "--evaluator-identity", "eval", "--split-manifest", overlap_manifest
        ]
        res, verdict = self.run_val(state, args, expected_rc=1)
        self.assertEqual(verdict["release_verdict"], "REJECTED")
        self.assertEqual(verdict["provenance_gate"]["split_overlap_count"], 1)

    def test_unreadable_manifest_fails(self):
        bad_manifest = os.path.join(self.test_dir, "nonexistent.json")
        state = copy.deepcopy(self.base_state)
        args = [
            "--dataset-hash", "hash", "--checkpoint-digest", "dig", "--model-identity", "id",
            "--seed", "123", "--evaluator-identity", "eval", "--split-manifest", bad_manifest
        ]
        res, verdict = self.run_val(state, args, expected_rc=4) # Fails manifest load -> 4
        self.assertEqual(verdict["provenance_gate"]["manifest_details"]["verified"], False)

    def test_parsed_step_exceeds_global_step(self):
        state = copy.deepcopy(self.base_state)
        state["global_step"] = 90 # last step is 100, which is > 90
        res, verdict = self.run_val(state, expected_rc=1)
        self.assertEqual(verdict["release_verdict"], "REJECTED")
        self.assertEqual(verdict["training_health_gate"]["metrics"]["global_step_consistent"], False)

    def test_global_step_slightly_ahead_warns(self):
        state = copy.deepcopy(self.base_state)
        # global_step is 120, max parsed step is 100. Diff = 20. log_interval = 50. Passes with warning.
        state["global_step"] = 120
        args = [
            "--dataset-hash", "hash", "--checkpoint-digest", "dig", "--model-identity", "id",
            "--seed", "123", "--evaluator-identity", "eval", "--split-manifest", self.manifest_path
        ]
        res, verdict = self.run_val(state, args, expected_rc=0)
        self.assertEqual(verdict["release_verdict"], "METRIC_GATES_PASSED")

    def test_non_finite_metric_values(self):
        state = copy.deepcopy(self.base_state)
        state["log_history"][-1]["loss"] = float("nan")
        res, verdict = self.run_val(state, expected_rc=4)
        self.assertEqual(verdict["release_verdict"], "REJECTED")

    def test_strict_global_step_missing_fails(self):
        state = copy.deepcopy(self.base_state)
        if "global_step" in state:
            del state["global_step"]
        args = ["--strict-global-step"]
        res, verdict = self.run_val(state, args, expected_rc=1)
        self.assertEqual(verdict["training_health_gate"]["metrics"]["global_step_consistent"], False)

    def test_manifest_schema_type_check(self):
        invalid_manifest = os.path.join(self.test_dir, "invalid_manifest.json")
        with open(invalid_manifest, "w") as f:
            # Contains list-in-list (invalid type for ID)
            json.dump({"train": [["id1"]], "eval": ["id2"]}, f)
        state = copy.deepcopy(self.base_state)
        args = [
            "--dataset-hash", "hash", "--checkpoint-digest", "dig", "--model-identity", "id",
            "--seed", "123", "--evaluator-identity", "eval", "--split-manifest", invalid_manifest
        ]
        res, verdict = self.run_val(state, args, expected_rc=4) # Type check failure -> 4
        self.assertEqual(verdict["provenance_gate"]["manifest_details"]["verified"], False)

    def test_manifest_duplicate_check(self):
        dup_manifest = os.path.join(self.test_dir, "dup_manifest.json")
        with open(dup_manifest, "w") as f:
            # Contains duplicate in train split
            json.dump({"train": ["id1", "id1"], "eval": ["id2"]}, f)
        state = copy.deepcopy(self.base_state)
        args = [
            "--dataset-hash", "hash", "--checkpoint-digest", "dig", "--model-identity", "id",
            "--seed", "123", "--evaluator-identity", "eval", "--split-manifest", dup_manifest
        ]
        res, verdict = self.run_val(state, args, expected_rc=4)
        self.assertEqual(verdict["provenance_gate"]["manifest_details"]["verified"], False)

    def test_manifest_missing_train_key(self):
        bad_manifest = os.path.join(self.test_dir, "no_train.json")
        with open(bad_manifest, "w") as f:
            json.dump({"eval": ["id1"]}, f)
        state = copy.deepcopy(self.base_state)
        args = ["--split-manifest", bad_manifest]
        res, verdict = self.run_val(state, args, expected_rc=4)
        self.assertEqual(verdict["provenance_gate"]["status"], "FAIL")

    def test_manifest_missing_eval_key(self):
        bad_manifest = os.path.join(self.test_dir, "no_eval.json")
        with open(bad_manifest, "w") as f:
            json.dump({"train": ["id1"]}, f)
        state = copy.deepcopy(self.base_state)
        args = ["--split-manifest", bad_manifest]
        res, verdict = self.run_val(state, args, expected_rc=4)
        self.assertEqual(verdict["provenance_gate"]["status"], "FAIL")

    def test_manifest_train_is_string(self):
        bad_manifest = os.path.join(self.test_dir, "train_str.json")
        with open(bad_manifest, "w") as f:
            json.dump({"train": "id1", "eval": ["id2"]}, f)
        state = copy.deepcopy(self.base_state)
        args = ["--split-manifest", bad_manifest]
        res, verdict = self.run_val(state, args, expected_rc=4)
        self.assertEqual(verdict["provenance_gate"]["status"], "FAIL")

    def test_manifest_boolean_id(self):
        bad_manifest = os.path.join(self.test_dir, "bool_id.json")
        with open(bad_manifest, "w") as f:
            json.dump({"train": [True], "eval": ["id2"]}, f)
        state = copy.deepcopy(self.base_state)
        args = ["--split-manifest", bad_manifest]
        res, verdict = self.run_val(state, args, expected_rc=4)
        self.assertEqual(verdict["provenance_gate"]["status"], "FAIL")

    def test_manifest_float_id(self):
        bad_manifest = os.path.join(self.test_dir, "float_id.json")
        with open(bad_manifest, "w") as f:
            json.dump({"train": [1.5], "eval": ["id2"]}, f)
        state = copy.deepcopy(self.base_state)
        args = ["--split-manifest", bad_manifest]
        res, verdict = self.run_val(state, args, expected_rc=4)
        self.assertEqual(verdict["provenance_gate"]["status"], "FAIL")

    def test_manifest_mixed_valid_ids(self):
        valid_manifest = os.path.join(self.test_dir, "mixed_valid.json")
        with open(valid_manifest, "w") as f:
            eval_ids = ["id2", 200] + [f"e{i}" for i in range(98)]
            json.dump({"train": ["id1", 100], "eval": eval_ids}, f)
        state = copy.deepcopy(self.base_state)
        args = [
            "--dataset-hash", "hash", "--checkpoint-digest", "dig", "--model-identity", "id",
            "--seed", "123", "--evaluator-identity", "eval", "--split-manifest", valid_manifest
        ]
        res, verdict = self.run_val(state, args, expected_rc=0)
        self.assertEqual(verdict["provenance_gate"]["status"], "PASS")

    def test_assurance_three_evals_min_1_is_standard(self):
        # 3 eval records, min required evaluated records set to 1 -> STANDARD
        state = copy.deepcopy(self.base_state)
        args = [
            "--min-eval-records", "1",
            "--dataset-hash", "hash", "--checkpoint-digest", "dig", "--model-identity", "id",
            "--seed", "123", "--evaluator-identity", "eval", "--split-manifest", self.manifest_path
        ]
        res, verdict = self.run_val(state, args, expected_rc=0)
        self.assertEqual(verdict["held_out_alignment_gate"]["assurance_level"], "STANDARD")

    def test_assurance_one_eval_min_1_is_smoke(self):
        state = {
            "epoch": 1.0,
            "global_step": 100,
            "log_history": [
                {"step": 80, "loss": 0.55, "rewards/margins": 0.02},
                {"step": 85, "loss": 0.54, "rewards/margins": 0.025},
                {"step": 90, "loss": 0.53, "rewards/margins": 0.03},
                {"step": 95, "loss": 0.52, "rewards/margins": 0.035},
                {"step": 100, "loss": 0.51, "rewards/margins": 0.04},
                {"step": 100, "eval_loss": 0.52, "eval_preference_margin_mean": 0.038, "eval_preference_margin_median": 0.035, "eval_positive_margin_rate": 0.85}
            ]
        }
        args = [
            "--min-eval-records", "1",
            "--dataset-hash", "hash", "--checkpoint-digest", "dig", "--model-identity", "id",
            "--seed", "123", "--evaluator-identity", "eval", "--split-manifest", self.manifest_path
        ]
        res, verdict = self.run_val(state, args, expected_rc=0)
        self.assertEqual(verdict["held_out_alignment_gate"]["assurance_level"], "SMOKE_TEST_ONLY")

    def test_p_value_zero_tripwire(self):
        state = copy.deepcopy(self.base_state)
        # Create a perfectly linear margin ramp to force p-value = 0.0000
        state["log_history"] = [
            {"step": i, "loss": 0.5, "rewards/margins": 0.01 * i}
            for i in range(1, 25)
        ]
        # Include evaluations so held-out gate also parses
        state["log_history"].extend([
            {"step": 23, "eval_loss": 0.5, "eval_preference_margin_mean": 0.1, "eval_preference_margin_median": 0.1, "eval_positive_margin_rate": 0.8},
            {"step": 24, "eval_loss": 0.5, "eval_preference_margin_mean": 0.1, "eval_preference_margin_median": 0.1, "eval_positive_margin_rate": 0.8},
            {"step": 25, "eval_loss": 0.5, "eval_preference_margin_mean": 0.1, "eval_preference_margin_median": 0.1, "eval_positive_margin_rate": 0.8}
        ])
        args = [
            "--dataset-hash", "hash", "--checkpoint-digest", "dig", "--model-identity", "id",
            "--seed", "123", "--evaluator-identity", "eval", "--split-manifest", self.manifest_path
        ]
        res, verdict = self.run_val(state, args, expected_rc=1)
        self.assertEqual(verdict["release_verdict"], "REJECTED")
        self.assertEqual(verdict["training_health_gate"]["status"], "FAIL")

    def test_impossible_pos_rate_tripwire(self):
        state = copy.deepcopy(self.base_state)
        # Positive margin rate of 0.85 with 10 samples: 0.85 * 10 = 8.5 (not an integer)
        ten_manifest = os.path.join(self.test_dir, "ten_manifest.json")
        with open(ten_manifest, "w") as f:
            json.dump({"train": ["id1"], "eval": [f"e{i}" for i in range(10)]}, f)
        state["log_history"][-1]["eval_positive_margin_rate"] = 0.85
        args = [
            "--dataset-hash", "hash", "--checkpoint-digest", "dig", "--model-identity", "id",
            "--seed", "123", "--evaluator-identity", "eval", "--split-manifest", ten_manifest
        ]
        res, verdict = self.run_val(state, args, expected_rc=1)
        self.assertEqual(verdict["release_verdict"], "REJECTED")
        self.assertEqual(verdict["held_out_alignment_gate"]["status"], "FAIL")

if __name__ == "__main__":
    unittest.main()
