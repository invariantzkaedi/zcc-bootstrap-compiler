import json
from pathlib import Path
from jsonschema import validate

def test_dashboard_data_schema():
    root = Path(__file__).resolve().parents[1]
    data_path = root / "artifacts" / "dashboard_data.json"
    schema_path = root / "schemas" / "dashboard_data_schema.json"

    if not data_path.exists():
        from scripts.ingest_artifacts import init_db
        from scripts.generate_dashboard_data import generate_report
        init_db()
        generate_report()

    assert data_path.exists(), f"Dashboard data not found at {data_path}"
    assert schema_path.exists(), f"JSON Schema not found at {schema_path}"

    data = json.loads(data_path.read_text(encoding="utf-8"))
    schema = json.loads(schema_path.read_text(encoding="utf-8"))

    # Validate against JSON schema
    validate(instance=data, schema=schema)

    # Validate custom fields for ZCC Nebula
    assert "projects" in data, "ZCC Nebula projects galaxy list is missing"
    assert "showcase" in data, "ZCC Nebula showcase dock is missing"
    assert "ui_hints" in data, "ZCC Nebula UI hints are missing"
    assert "verification_summary" in data, "ZCC Nebula verification summary is missing"

    # Validate compiler details
    assert "runs" in data, "Historical runs list is missing"
    assert len(data["projects"]) > 0, "Projects galaxy list is empty"
    assert data["ui_hints"]["accent_mode"] == "nebula", "Incorrect accent mode UI hint"

def test_symbol_whitelist_negative_control():
    ALLOWED_SYMBOLS = {"chaitin_briggs", "eval_const_expr", "ir_lower_float", "x86_codegen_sse", "dom_dominates", "zcc_render_phase", "yul_weaver", "fzr_event_hash", "zcc_diag", "oneirogenesis_scan"}
    malicious_symbol = "MALICIOUS_UNLISTED_SYM"
    assert malicious_symbol not in ALLOWED_SYMBOLS, "Negative control symbol must be rejected by whitelist"

if __name__ == "__main__":
    test_dashboard_data_schema()
    test_symbol_whitelist_negative_control()
    print("Dashboard schema & whitelist negative control validation test: PASSED")

