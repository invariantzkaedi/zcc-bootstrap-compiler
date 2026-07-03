#!/usr/bin/env node
const fs = require("fs");
const { chromium } = require("playwright");

function arg(name, def = null) {
  const i = process.argv.indexOf(name);
  return i >= 0 ? process.argv[i + 1] : def;
}

(async () => {
  const experimentId = arg("--experiment-id");
  const seed = Number(arg("--seed", "1337"));
  const durationSec = Number(arg("--duration-sec", "60"));
  const outputJson = arg("--output-json");
  const inputsJson = JSON.parse(arg("--inputs-json", "{}"));
  const actionsJson = JSON.parse(arg("--actions-json", "[]"));

  const browser = await chromium.launch({ headless: true });
  const page = await browser.newPage({ viewport: { width: 1920, height: 1080 } });

  // Open your visualizer page
  await page.goto("http://127.0.0.1:8080/audio_reactive_creature.html", { waitUntil: "networkidle" });

  // Inject run config into app
  await page.evaluate(({ experimentId, seed, inputsJson, actionsJson }) => {
    window.__QA_CONFIG__ = { experimentId, seed, inputsJson, actionsJson };
    if (window.setQAMode) window.setQAMode(window.__QA_CONFIG__);
  }, { experimentId, seed, inputsJson, actionsJson });

  // Start telemetry
  await page.evaluate(() => {
    if (!window.__QA_METRICS__) window.__QA_METRICS__ = [];
    window.__QA_START__ = performance.now();
    if (window.startQATelemetry) window.startQATelemetry();
  });

  await page.waitForTimeout(durationSec * 1000);

  // Pull measured metrics from runtime
  const metrics = await page.evaluate(() => {
    if (window.getQAMetrics) return window.getQAMetrics();
    return null;
  });

  await browser.close();

  if (!metrics) {
    console.error("No metrics returned. Implement window.getQAMetrics() in runtime.");
    process.exit(2);
  }

  fs.writeFileSync(outputJson, JSON.stringify(metrics, null, 2), "utf-8");
  console.log(`Wrote real metrics: ${outputJson}`);
})();
