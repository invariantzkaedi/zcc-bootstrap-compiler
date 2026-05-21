import os
import json
from http.server import HTTPServer, SimpleHTTPRequestHandler

PORT = 8080
DIRECTORY = os.path.dirname(os.path.abspath(__file__))

class ZkaediHandler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=DIRECTORY, **kwargs)

    def do_GET(self):
        # Health check / status API endpoint
        if self.path == '/api/status':
            self.send_response(200)
            self.send_header('Content-type', 'application/json')
            self.end_headers()
            status = {
                'status': 'ONLINE',
                'engine': 'ZKAEDI Fleet Engine',
                'tools_available': len([f for f in os.listdir(DIRECTORY) if f.endswith('.html') and f != 'index.html'])
            }
            self.wfile.write(json.dumps(status).encode())
            return
        
        # Default static file serving
        return super().do_GET()

def run():
    server_address = ('', PORT)
    httpd = HTTPServer(server_address, ZkaediHandler)
    print(f"\n🔱 ZKAEDI Fleet Engine Backend running on http://localhost:{PORT}")
    print("=" * 60)
    print("Serving the following tools:\n")
    
    for f in sorted(os.listdir(DIRECTORY)):
        if f.endswith('.html'):
            print(f" - http://localhost:{PORT}/{f}")
            
    print("\nPress Ctrl+C to shut down the server.")
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nShutting down engine...")
        httpd.server_close()

if __name__ == '__main__':
    run()
