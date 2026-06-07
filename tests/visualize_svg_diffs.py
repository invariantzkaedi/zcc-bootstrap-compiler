#!/usr/bin/env python3
import webbrowser
import os
import time
from http.server import SimpleHTTPRequestHandler, HTTPServer
import threading

HTML = """<!DOCTYPE html>
<html><head><title>ZCC SVG Visual Diff — EVM Exploit Topologies</title>
<style>
  body { margin:0; font-family:monospace; background:#111; color:#0f0; }
  .split { display:flex; height:100vh; }
  .panel { flex:1; padding:10px; text-align:center; }
  svg { width:100%; height:90%; border:2px solid #0f0; background:#000; }
  .label { font-size:18px; margin:10px; color:#0ff; }
  input[type=range] { width:80%; }
</style></head>
<body>
<div class="split">
  <div class="panel">
    <div class="label">BASELINE (default phases)</div>
    <div id="baseline"></div>
  </div>
  <div class="panel">
    <div class="label">CURRENT (after edge-case hammering)</div>
    <div id="current"></div>
  </div>
</div>
<div style="position:fixed;bottom:20px;left:50%;transform:translateX(-50%);background:#222;padding:10px 30px;border-radius:30px;box-shadow:0 0 20px #0f0;">
  <label>LIVE PHASE: <span id="val">0.5</span></label>
  <input type="range" id="slider" min="0" max="1" step="0.01" value="0.5">
</div>

<script>
const baseline = document.getElementById('baseline');
const current = document.getElementById('current');
const slider = document.getElementById('slider');
const val = document.getElementById('val');

fetch('baseline.svg').then(r=>r.text()).then(svg=> baseline.innerHTML = svg);
fetch('current.svg').then(r=>r.text()).then(svg=> current.innerHTML = svg);

slider.addEventListener('input', () => {
  val.textContent = parseFloat(slider.value).toFixed(2);
  console.log('LIVE PHASE → ' + slider.value);
});
</script>
</body></html>"""

def serve_and_open():
    os.makedirs("tests", exist_ok=True)
    with open("tests/visual_diff.html", "w") as f:
        f.write(HTML)
    
    def server():
        os.chdir("tests")
        HTTPServer(('localhost', 8082), SimpleHTTPRequestHandler).serve_forever()
    
    threading.Thread(target=server, daemon=True).start()
    time.sleep(0.5)
    # Since we are running in WSL/Windows, webbrowser.open can work if configured, or the user can navigate to it.
    try:
        webbrowser.open('http://localhost:8082/visual_diff.html')
    except Exception:
        pass
    print("777JACKPOT777 — Visual SVG diff opened in browser or available at http://localhost:8082/visual_diff.html")

if __name__ == "__main__":
    serve_and_open()
