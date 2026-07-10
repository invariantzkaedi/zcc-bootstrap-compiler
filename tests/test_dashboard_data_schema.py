import json
from pathlib import Path
from jsonschema import validate

def test_dashboard_data_schema():
    root = Path(__file__).resolve().parents[1]
    data_path = root / "artifacts" / "dashboard_data.json"
    schema_path = root / "schemas" / "dashboard_data_schema.json"

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

if __name__ == "__main__":
    test_dashboard_data_schema()
    print("Dashboard schema validation test: PASSED")
