const fs = require('fs');
const vm = require('vm');

const html = fs.readFileSync('zkaedi_quantum_walk_3d.html', 'utf8');

// Extract all <script> contents that don't have src=
const scriptRegex = /<script(?![^>]*src=)[^>]*>([\s\S]*?)<\/script>/gi;
let match;
let scriptIdx = 0;
let totalErrors = 0;

while ((match = scriptRegex.exec(html)) !== null) {
  scriptIdx++;
  const code = match[1];
  try {
    new vm.Script(code);
    console.log("[PASS] Embedded Script " + scriptIdx + " syntax AST valid (" + code.length + " bytes)");
  } catch (err) {
    console.error("[FAIL] Embedded Script " + scriptIdx + " syntax error: " + err.message);
    totalErrors++;
  }
}

// Check element IDs
const requiredIds = [
  'metric-npu-tops',
  'metric-npu-sub',
  'metric-gpu-vram',
  'metric-gpu-sub',
  'badge-npu-status',
  'badge-gpu-status',
  'npu-pod-val',
  'gpu-pod-val',
  'npu-lat-val',
  'npu-speedup-val',
  'btn-sync-telemetry',
  'dual-silicon-live-tag'
];

for (const id of requiredIds) {
  if (!html.includes('id="' + id + '"')) {
    console.error("[FAIL] Missing DOM element ID: " + id);
    totalErrors++;
  } else {
    console.log("[PASS] Verified DOM element ID: " + id);
  }
}

console.log("\n==================================================");
console.log("Rule EF-7 Headless Evaluation: " + totalErrors + " errors.");
console.log("==================================================");
process.exit(totalErrors === 0 ? 0 : 1);
