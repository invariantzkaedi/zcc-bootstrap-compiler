from http.server import ThreadingHTTPServer, SimpleHTTPRequestHandler
from pathlib import Path
import os
import json
import urllib.parse

import platform

PORT = 8081
BASE_DIR = Path(__file__).resolve().parent

if platform.system() == "Windows":
    ASSETS_PATH = Path(r"d:\meshy_3d")  # existing local asset mount
else:
    ASSETS_PATH = Path("/mnt/d/meshy_3d")  # WSL mount path

# Core files the control center can use
STATIC_ROUTES = {
    "/": "zcc-control-center.html",
    "/index.html": "zcc-control-center.html",
    "/ast": "zcc_ast_viz.html",
    "/ast.html": "zcc_ast_viz.html",
    "/viz/hamiltonian": "dashboard_hamiltonian_visualizer.html",
    "/viz/heist": "heist_dashboard.html",
    "/viz/audio": "audio_reactive_creature.html",
    "/viz/matrix": "multi_showcase.html",
    "/viz/oracle_masks": "oracle_masks.html",
    "/viz/world_gen": "procedural_world_gen.html",
    "/observatory": "gods_eye_3d_observatory.html",
    "/viz/observatory": "gods_eye_3d_observatory.html",
}

API_ROUTES = {
    "/api/dashboard-data": BASE_DIR / "artifacts" / "dashboard_data.json",
    "/api/executive-scorecard": BASE_DIR / "artifacts" / "executive_scorecard.json",
    "/api/policy-check": BASE_DIR / "artifacts" / "policy_check_report.json",
    "/api/remediation-plan": BASE_DIR / "artifacts" / "remediation_plan.json",
    "/api/anomaly-report": BASE_DIR / "artifacts" / "anomaly_report.json",
    "/api/forecast-report": BASE_DIR / "artifacts" / "forecast_report.json",
}

def send_json(handler, data, status=200):
    raw = json.dumps(data, indent=2).encode("utf-8")
    handler.send_response(status)
    handler.send_header("Content-Type", "application/json; charset=utf-8")
    handler.send_header("Content-Length", str(len(raw)))
    handler.send_header("Access-Control-Allow-Origin", "*")
    handler.send_header("Access-Control-Allow-Methods", "*")
    handler.send_header("Access-Control-Allow-Headers", "*")
    handler.end_headers()
    handler.wfile.write(raw)

class CORSRequestHandler(SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "*")
        self.send_header("Access-Control-Allow-Headers", "*")
        super().end_headers()

    def do_OPTIONS(self):
        self.send_response(200)
        self.end_headers()

class CustomHandler(CORSRequestHandler):
    def translate_path(self, path):
        # Keep default behavior for filesystem files
        path = path.split("?", 1)[0]
        path = path.split("#", 1)[0]

        try:
            path = urllib.parse.unquote(path, errors="surrogatepass")
        except UnicodeDecodeError:
            path = urllib.parse.unquote(path)

        # Mount /assets/ to local asset directory
        if "/assets/" in path:
            parts = path.split("/assets/")
            rel = parts[-1].lstrip("/")
            resolved = ASSETS_PATH / rel
            print(f"[ZCC] /assets/ {path} -> {resolved}")
            return str(resolved)

        # Serve repo files from BASE_DIR
        return str(BASE_DIR / path.lstrip("/"))

    def do_GET(self):
        parsed = urllib.parse.urlparse(self.path)
        route = parsed.path

        # API routes
        if route in API_ROUTES:
            file_path = API_ROUTES[route]
            if file_path.exists():
                try:
                    data = json.loads(file_path.read_text(encoding="utf-8"))
                except Exception as e:
                    send_json(self, {"error": f"failed to load {file_path.name}", "detail": str(e)}, status=500)
                    return
                send_json(self, data)
            else:
                send_json(self, {"error": f"{file_path.name} not found"}, status=404)
            return

        if route == "/api/ir":
            query = urllib.parse.parse_qs(parsed.query)
            func_name = query.get("func", [""])[0]
            ALLOWED_SYMBOLS = {"chaitin_briggs", "eval_const_expr", "ir_lower_float", "x86_codegen_sse", "dom_dominates", "zcc_render_phase", "yul_weaver", "fzr_event_hash", "zcc_diag", "oneirogenesis_scan"}
            if not func_name or func_name not in ALLOWED_SYMBOLS:
                send_json(self, {"error": "Invalid or unlisted symbol query", "symbol": func_name}, status=400)
                return
            send_json(self, {"symbol": func_name, "status": "verified", "ir": f"// IR definition for {func_name}\nret void"})
            return

        # Static aliases
        if route in STATIC_ROUTES:
            self.path = "/" + STATIC_ROUTES[route]
            return super().do_GET()

        # Let SimpleHTTPRequestHandler serve everything else
        return super().do_GET()

if __name__ == "__main__":
    os.chdir(BASE_DIR)
    print(f"[ZCC] Control Center serving at http://localhost:{PORT}")
    print(f"[ZCC] Assets mounted at http://localhost:{PORT}/assets/")
    print(f"[ZCC] API: /api/dashboard-data, /api/executive-scorecard, /api/policy-check, /api/remediation-plan")
    httpd = ThreadingHTTPServer(("", PORT), CustomHandler)
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\n[ZCC] Server shutting down.")
        httpd.server_close()
