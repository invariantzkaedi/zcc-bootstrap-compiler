const { performance } = require('perf_hooks');

const start = performance.now();

let out = "<svg width='1000' height='1000'>\n";
for (let i = 0; i < 1000000; i++) {
    out += `<circle cx="${(i * 13) % 1000}" cy="${(i * 7) % 1000}" r="5" fill="cyan"/>\n`;
}
out += "</svg>";

const end = performance.now();
console.log(`JS Generated ${out.length} bytes.`);
console.log(`Node.js Engine Execution Time: ${(end - start).toFixed(2)} ms`);
