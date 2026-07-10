from http.server import ThreadingHTTPServer, SimpleHTTPRequestHandler
from pathlib import Path
import os
import json
import urllib.parse

PORT = 8081
BASE_DIR = Path(__file__).resolve().parent
ASSETS_PATH = Path(r"d:\meshy_3d")  # existing local asset mount

# Core files the control center can use
STATIC_ROUTES = {
    "/": "zcc-control-center.html",
    "/index.html": "zcc-control-center.html",
    "/ast": "zcc_ast_viz.html",
    "/ast.html": "zcc_ast_viz.html",
    "/viz/hamiltonian": "dashboard_hamiltonian_visualizer.html",
    "/viz/heist": "heist_dashboard.html",
    "/viz/audio": "audio_reactive_creature.html",
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
            idx = path.find("/assets/")
            rel = path[idx + len("/assets/"):].lstrip("/")
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
