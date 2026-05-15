from http.server import ThreadingHTTPServer, SimpleHTTPRequestHandler
import os

class CORSRequestHandler(SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header('Access-Control-Allow-Origin', '*')
        self.send_header('Access-Control-Allow-Methods', '*')
        self.send_header('Access-Control-Allow-Headers', '*')
        super().end_headers()

    def do_OPTIONS(self):
        self.send_response(200)
        self.end_headers()

PORT = 8081
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
ASSETS_PATH = r"d:\meshy_3d"

class CustomHandler(CORSRequestHandler):
    def translate_path(self, path):
        import urllib.parse
        import posixpath
        
        path = path.split('?',1)[0]
        path = path.split('#',1)[0]
        
        try:
            path = urllib.parse.unquote(path, errors='surrogatepass')
        except UnicodeDecodeError:
            path = urllib.parse.unquote(path)

        # Mount /assets/ to d:\meshy_3d\
        if path.startswith('/assets/'):
            # Strip off /assets/ and construct path
            resolved = os.path.join(ASSETS_PATH, path[len('/assets/'):].lstrip('/'))
            print(f"[ZKAEDI-ROUTER] Translated {path} -> {resolved}")
            return resolved
            
        return super().translate_path(path)

if __name__ == "__main__":
    os.chdir(BASE_DIR)
    print(f"[ZKAEDI] Dashboard serving at http://localhost:{PORT}")
    print(f"[ZKAEDI] Assets mounted at http://localhost:{PORT}/assets/")
    httpd = ThreadingHTTPServer(("", PORT), CustomHandler)
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\n[ZKAEDI] Server shutting down.")
        httpd.server_close()
