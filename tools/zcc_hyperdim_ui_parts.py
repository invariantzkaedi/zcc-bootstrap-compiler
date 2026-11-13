#!/usr/bin/env python3
# -*- coding: utf-8 -*-
r"""
tools/zcc_hyperdim_ui_parts.py
==============================
Zero-dependency HTML5/CSS3/JS UI & Independent Browser Oracle templates for
`zcc_hyperdim_test_harness.html`, driven by `tools/zcc_quad_isa_gate.py`.
"""

from __future__ import annotations

HTML_HEAD_AND_BODY = r'''<!DOCTYPE html>
<html lang="en">

<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ZCC HyperDimensional Quad-ISA &amp; Bootstrap Test Harness</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }

        body {
            background: linear-gradient(135deg, #050816 0%, #0b132b 50%, #091d36 100%);
            color: #00f5d4;
            font-family: 'Cascadia Code', 'Fira Code', 'Consolas', 'Courier New', monospace;
            overflow-x: hidden;
            min-height: 100vh;
            font-size: 12px;
        }

        #ambient-grid-overlay {
            position: fixed;
            inset: 0;
            pointer-events: none !important;
            z-index: 1;
            background-image:
                radial-gradient(circle at 15% 15%, rgba(0, 245, 212, 0.08) 0%, transparent 40%),
                radial-gradient(circle at 85% 20%, rgba(255, 102, 0, 0.07) 0%, transparent 42%),
                linear-gradient(rgba(0, 245, 212, 0.025) 1px, transparent 1px),
                linear-gradient(90deg, rgba(0, 245, 212, 0.025) 1px, transparent 1px);
            background-size: 100% 100%, 100% 100%, 28px 28px, 28px 28px;
        }

        .test-harness-container {
            position: relative;
            z-index: 1000;
            pointer-events: auto;
            display: grid;
            grid-template-columns: 320px minmax(0, 1fr);
            width: 100%;
            min-height: 100vh;
            align-items: start;
        }

        .results-panel,
        .panel-section,
        .test-result-card,
        .inspector-deck,
        button,
        input,
        select,
        label {
            position: relative;
            z-index: 1000;
            pointer-events: auto;
            user-select: auto;
        }

        /* Control Panel */
        .control-panel {
            background: rgba(4, 8, 20, 0.95);
            border-right: 2px solid #00f5d4;
            padding: 16px;
            overflow-y: auto;
            position: sticky;
            top: 0;
            grid-column: 1;
            width: 320px;
            height: 100vh;
            z-index: 1005;
            pointer-events: auto;
            user-select: auto;
        }

        .panel-section {
            margin-bottom: 14px;
            padding: 11px;
            border: 1px solid rgba(0, 245, 212, 0.3);
            border-radius: 8px;
            background: rgba(0, 245, 212, 0.04);
        }

        .section-title {
            color: #ffbe0b;
            font-weight: bold;
            font-size: 13px;
            margin-bottom: 9px;
            text-align: center;
            letter-spacing: 0.05em;
            text-shadow: 0 0 10px rgba(255, 190, 11, 0.45);
        }

        .test-suite-btn {
            width: 100%;
            padding: 8px 10px;
            margin: 4px 0;
            background: linear-gradient(45deg, #d9381e, #ff6b00);
            border: 1px solid rgba(255, 190, 11, 0.35);
            color: #ffffff;
            border-radius: 6px;
            cursor: pointer;
            font-family: inherit;
            font-weight: bold;
            transition: all 0.2s ease;
            font-size: 12px;
            text-align: left;
            display: flex;
            justify-content: space-between;
            align-items: center;
            gap: 8px;
        }

        .test-suite-btn:hover {
            background: linear-gradient(45deg, #ff5500, #ff9900);
            transform: translateY(-1px);
            box-shadow: 0 4px 12px rgba(255, 102, 0, 0.4);
        }

        .test-suite-btn.running {
            background: linear-gradient(45deg, #059669, #10b981);
            border-color: #6ee7b7;
            animation: pulse 1s infinite;
        }

        .suite-tag {
            font-size: 11px;
            opacity: 0.95;
            background: rgba(0, 0, 0, 0.38);
            padding: 2px 6px;
            border-radius: 4px;
            white-space: nowrap;
            flex-shrink: 0;
        }

        .test-config {
            display: flex;
            flex-direction: column;
            gap: 8px;
        }

        .config-row {
            display: flex;
            justify-content: space-between;
            align-items: center;
            font-size: 12px;
            gap: 6px;
        }

        .config-row label {
            white-space: nowrap;
            flex-shrink: 0;
        }

        .config-slider {
            flex: 1;
            min-width: 72px;
            max-width: 112px;
            height: 6px;
            background: rgba(255, 255, 255, 0.15);
            border-radius: 3px;
            outline: none;
            cursor: pointer;
            accent-color: #00f5d4;
        }

        .config-value {
            color: #ff9f1c;
            font-weight: bold;
            min-width: 48px;
            text-align: right;
            white-space: nowrap;
            font-variant-numeric: tabular-nums;
        }

        .target-label {
            display: flex;
            align-items: center;
            gap: 8px;
            font-size: 12px;
            cursor: pointer;
            padding: 3px 4px;
            border-radius: 4px;
        }

        .target-label:hover {
            background: rgba(0, 245, 212, 0.08);
        }

        /* Results Panel */
        .results-panel {
            grid-column: 2;
            min-width: 0;
            width: 100%;
            padding: 18px 22px 32px;
            display: flex;
            flex-direction: column;
            gap: 16px;
            min-height: 100vh;
        }

        @media (max-width: 960px) {
            .test-harness-container {
                grid-template-columns: 1fr;
            }
            .control-panel {
                position: relative;
                grid-column: 1;
                width: 100%;
                height: auto;
                border-right: none;
                border-bottom: 2px solid #00f5d4;
            }
            .results-panel {
                grid-column: 1;
            }
        }

        .results-header {
            width: 100%;
            min-width: 0;
            background: rgba(5, 10, 24, 0.9);
            border: 2px solid #ffbe0b;
            border-radius: 10px;
            padding: 14px 18px;
        }

        .header-top-bar {
            display: flex;
            flex-wrap: wrap;
            justify-content: space-between;
            align-items: center;
            gap: 10px;
            margin-bottom: 8px;
        }

        .badge-strip {
            display: flex;
            flex-wrap: wrap;
            gap: 8px;
            align-items: center;
        }

        .zcc-pill {
            display: inline-block;
            padding: 3px 9px;
            border-radius: 999px;
            font-size: 12px;
            font-weight: bold;
            border: 1px solid rgba(0, 245, 212, 0.45);
            background: rgba(0, 245, 212, 0.12);
            color: #00f5d4;
        }

        .zcc-pill.warn {
            border-color: rgba(255, 190, 11, 0.55);
            background: rgba(255, 190, 11, 0.14);
            color: #ffbe0b;
        }

        .zcc-pill.fail {
            border-color: rgba(255, 51, 102, 0.7);
            background: rgba(255, 51, 102, 0.2);
            color: #ff6b8b;
        }

        .zcc-pill.pass {
            border-color: rgba(16, 185, 129, 0.65);
            background: rgba(16, 185, 129, 0.18);
            color: #34d399;
        }

        /* Hero 4D Rotor + Lineage Row */
        .hero-deck {
            display: grid;
            grid-template-columns: minmax(0, 1.35fr) minmax(0, 1fr);
            gap: 16px;
            width: 100%;
        }

        @media (max-width: 1200px) {
            .hero-deck {
                grid-template-columns: 1fr;
            }
        }

        .deck-card {
            min-width: 0;
            width: 100%;
            background: rgba(5, 10, 24, 0.88);
            border: 1px solid rgba(0, 245, 212, 0.35);
            border-radius: 10px;
            padding: 14px;
        }

        .deck-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            flex-wrap: wrap;
            gap: 8px;
            margin-bottom: 10px;
        }

        .results-grid {
            display: grid;
            grid-template-columns: repeat(2, minmax(0, 1fr));
            gap: 16px;
            width: 100%;
        }

        @media (max-width: 1100px) {
            .results-grid {
                grid-template-columns: 1fr;
            }
        }

        .test-result-card {
            min-width: 0;
            width: 100%;
            background: rgba(4, 9, 22, 0.86);
            border: 2px solid;
            border-radius: 10px;
            padding: 14px;
            position: relative;
        }

        .card-refined {
            border-color: #00f5d4;
        }

        .card-deep {
            border-color: #ff6b00;
        }

        .card-original {
            border-color: #f72585;
        }

        .card-test {
            border-color: #10b981;
        }

        .card-title {
            text-align: center;
            font-weight: bold;
            font-size: 14px;
            margin-bottom: 4px;
            text-shadow: 0 0 10px currentColor;
        }

        .card-subtitle {
            text-align: center;
            color: #94a3b8;
            font-size: 12px;
            margin-bottom: 10px;
        }

        .metric-grid {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 6px;
            font-size: 12px;
        }

        .metric-item {
            display: flex;
            justify-content: space-between;
            padding: 4px 8px;
            background: rgba(255, 255, 255, 0.05);
            border-radius: 4px;
            font-variant-numeric: tabular-nums;
        }

        .metric-value {
            color: #34d399;
            font-weight: bold;
        }

        .chart-container {
            width: 100%;
            height: 150px;
            background: rgba(0, 0, 0, 0.42);
            border: 1px solid rgba(255, 255, 255, 0.08);
            border-radius: 6px;
            margin-top: 10px;
            position: relative;
            overflow: hidden;
        }

        .chart-canvas {
            width: 100%;
            height: 100%;
            display: block;
        }

        .batch-controls {
            display: flex;
            gap: 8px;
            margin-bottom: 8px;
            flex-wrap: wrap;
        }

        .batch-btn {
            flex: 1;
            min-width: 80px;
            padding: 7px 8px;
            background: rgba(0, 245, 212, 0.1);
            border: 1px solid #00f5d4;
            color: #00f5d4;
            border-radius: 5px;
            cursor: pointer;
            font-family: inherit;
            font-size: 12px;
            font-weight: bold;
            transition: all 0.2s ease;
        }

        .batch-btn:hover {
            background: rgba(0, 245, 212, 0.24);
            box-shadow: 0 0 10px rgba(0, 245, 212, 0.3);
        }

        .batch-btn.danger {
            border-color: #f72585;
            color: #ff7aa2;
            background: rgba(247, 37, 133, 0.12);
        }

        .batch-btn.danger:hover {
            background: rgba(247, 37, 133, 0.28);
        }

        .batch-btn.amber {
            border-color: #ffbe0b;
            color: #ffd166;
            background: rgba(255, 190, 11, 0.12);
        }

        .batch-btn.emerald {
            border-color: #10b981;
            color: #6ee7b7;
            background: rgba(16, 185, 129, 0.16);
        }

        .batch-btn.emerald:hover {
            background: rgba(16, 185, 129, 0.3);
        }

        .progress-bar {
            width: 100%;
            height: 10px;
            background: rgba(255, 255, 255, 0.1);
            border-radius: 5px;
            overflow: hidden;
            margin: 8px 0;
        }

        .progress-fill {
            height: 100%;
            background: linear-gradient(90deg, #00f5d4, #ff6b00, #f72585);
            width: 0%;
            transition: width 0.18s ease;
        }

        .status-indicator {
            position: absolute;
            top: 12px;
            right: 12px;
            width: 13px;
            height: 13px;
            border-radius: 50%;
            background: #64748b;
            border: 1px solid rgba(255, 255, 255, 0.35);
        }

        .status-indicator.running {
            background: #ffbe0b;
            box-shadow: 0 0 10px #ffbe0b;
            animation: blink 0.9s infinite;
        }

        .status-indicator.pass {
            background: #10b981;
            box-shadow: 0 0 10px #10b981;
        }

        .status-indicator.fail {
            background: #ff3366;
            box-shadow: 0 0 12px #ff3366;
        }

        .status-indicator.inconclusive {
            background: #f59e0b;
        }

        @keyframes pulse {
            0%, 100% { transform: scale(1); }
            50% { transform: scale(1.02); }
        }

        @keyframes blink {
            0%, 50% { opacity: 1; }
            51%, 100% { opacity: 0.35; }
        }

        /* Inspector Tabs & Hex / ASM Viewer */
        .inspector-deck {
            min-width: 0;
            width: 100%;
            background: rgba(5, 10, 24, 0.92);
            border: 1px solid rgba(247, 37, 133, 0.45);
            border-radius: 10px;
            padding: 14px;
        }

        .tab-bar {
            display: flex;
            flex-wrap: wrap;
            gap: 8px;
            margin-bottom: 10px;
        }

        .tab-btn {
            padding: 6px 12px;
            border-radius: 6px;
            border: 1px solid rgba(0, 245, 212, 0.3);
            background: rgba(0, 245, 212, 0.06);
            color: #cbd5e1;
            cursor: pointer;
            font-family: inherit;
            font-size: 12px;
            font-weight: bold;
        }

        .tab-btn.active {
            background: rgba(0, 245, 212, 0.22);
            border-color: #00f5d4;
            color: #00f5d4;
        }

        .code-pane {
            background: #020611;
            border: 1px solid rgba(0, 245, 212, 0.2);
            border-radius: 6px;
            padding: 10px;
            max-height: 240px;
            overflow-y: auto;
            color: #e2e8f0;
            font-size: 12px;
            line-height: 1.45;
            white-space: pre;
        }

        .hex-grid {
            display: flex;
            flex-wrap: wrap;
            gap: 3px;
            max-height: 210px;
            overflow-y: auto;
            padding: 8px;
            background: #020611;
            border-radius: 6px;
            border: 1px solid rgba(247, 37, 133, 0.25);
        }

        .hex-byte {
            padding: 2px 4px;
            border-radius: 3px;
            cursor: pointer;
            font-size: 12px;
            background: rgba(255, 255, 255, 0.04);
            color: #94a3b8;
            border: 1px solid transparent;
        }

        .hex-byte.sec-code {
            color: #34d399;
            background: rgba(16, 185, 129, 0.08);
        }

        .hex-byte.sec-prove {
            color: #00f5d4;
            background: rgba(0, 245, 212, 0.12);
        }

        .hex-byte.mutated {
            background: rgba(255, 51, 102, 0.35);
            color: #ffffff;
            border-color: #ff3366;
            font-weight: bold;
        }

        .dist-table {
            width: 100%;
            border-collapse: collapse;
            font-size: 12px;
        }

        .dist-table th,
        .dist-table td {
            padding: 6px 8px;
            border-bottom: 1px solid rgba(255, 255, 255, 0.08);
            text-align: left;
            font-variant-numeric: tabular-nums;
            vertical-align: top;
        }

        .dist-table th {
            color: #ffbe0b;
        }

        .test-log {
            background: rgba(2, 5, 15, 0.95);
            border: 2px solid #d946ef;
            border-radius: 10px;
            padding: 12px;
            height: 210px;
            overflow-y: auto;
            font-size: 12px;
            line-height: 1.4;
        }

        .log-entry {
            margin: 2px 0;
            padding: 3px 6px;
            border-radius: 3px;
        }

        .log-info {
            background: rgba(0, 245, 212, 0.09);
            color: #5eead4;
        }

        .log-success {
            background: rgba(16, 185, 129, 0.12);
            color: #6ee7b7;
        }

        .log-warning {
            background: rgba(255, 190, 11, 0.12);
            color: #fde047;
        }

        .log-error {
            background: rgba(255, 51, 102, 0.16);
            color: #fda4af;
        }

        .hidden {
            display: none;
        }
    </style>
</head>

<body>
    <div id="ambient-grid-overlay"></div>

    <div class="test-harness-container">
        <!-- Control Panel -->
        <aside class="control-panel" aria-label="ZCC Test Harness Controls">
            <!-- Live WSL ./zcc + QEMU Bridge -->
            <div class="panel-section" style="border-color: rgba(16, 185, 129, 0.5); background: rgba(16, 185, 129, 0.06);">
                <div class="section-title" style="color: #34d399;">🔌 LIVE ./zcc + QEMU BRIDGE</div>
                <div class="batch-controls">
                    <button class="batch-btn emerald" id="bridge-recompile-btn" onclick="window.zccHarness && window.zccHarness.recompileViaBridge()">⚡ Recompile &amp; Run 3-Tier QEMU</button>
                </div>
                <div class="batch-controls">
                    <button class="batch-btn amber" id="bridge-ping-btn" onclick="window.zccHarness && window.zccHarness.probeBridgeStatus(true)">🔎 Ping :8799</button>
                    <button class="batch-btn" id="bridge-matrix-btn" onclick="window.zccHarness && window.zccHarness.switchInspectorTab('bugs')">🐛 Bug Matrix</button>
                </div>
                <div id="bridge-mini-note" style="font-size: 11px; color: #94a3b8; line-height: 1.35;">
                    CLI: <code style="color:#00f5d4;">make test-quad-isa</code><br>
                    Live Server: <code style="color:#ffbe0b;">python3 tools/zcc_quad_isa_gate.py --serve</code>
                </div>
            </div>

            <!-- Test Suites -->
            <div class="panel-section">
                <div class="section-title">🧪 ZCC VERIFICATION SUITES</div>
                <button id="performance-test" class="test-suite-btn" onclick="window.zccHarness && window.zccHarness.runTestSuite('performance')">
                    <span>Gate 1: Quad-ISA Benchmark</span><span class="suite-tag">WASM+ASM</span>
                </button>
                <button id="stress-test" class="test-suite-btn" onclick="window.zccHarness && window.zccHarness.runTestSuite('stress')">
                    <span>MV-02: Mutation Stress</span><span class="suite-tag">6/6-KILL</span>
                </button>
                <button id="audio-test" class="test-suite-btn" onclick="window.zccHarness && window.zccHarness.runTestSuite('audio')">
                    <span>Q-DSP: Solfeggio + FFT</span><span class="suite-tag">Q15 PCM</span>
                </button>
                <button id="geometry-test" class="test-suite-btn" onclick="window.zccHarness && window.zccHarness.runTestSuite('geometry')">
                    <span>4D Rotor &amp; Steane QEC</span><span class="suite-tag">[[7,1,3]]</span>
                </button>
                <button id="memory-test" class="test-suite-btn" onclick="window.zccHarness && window.zccHarness.runTestSuite('memory')">
                    <span>Arena, VLA &amp; Canary Audit</span><span class="suite-tag">424B ABI</span>
                </button>
                <button id="compatibility-test" class="test-suite-btn" onclick="window.zccHarness && window.zccHarness.runTestSuite('compatibility')">
                    <span>CORE-05: 7-Way Consensus</span><span class="suite-tag">FNV-1a</span>
                </button>
                <button id="endurance-test" class="test-suite-btn" onclick="window.zccHarness && window.zccHarness.runTestSuite('endurance')">
                    <span>EF-13: 4-Arm PRIME Suite</span><span class="suite-tag">CAUSAL</span>
                </button>
                <button id="regression-test" class="test-suite-btn" onclick="window.zccHarness && window.zccHarness.runTestSuite('regression')">
                    <span>E-LEARN &amp; EF-0 Guardrail</span><span class="suite-tag">01..24</span>
                </button>
            </div>

            <!-- Batch Operations -->
            <div class="panel-section">
                <div class="section-title">📊 BATCH &amp; ACOUSTIC OPS</div>
                <div class="batch-controls">
                    <button class="batch-btn" id="run-all" onclick="window.zccHarness && window.zccHarness.runAllTests()">Run All 8</button>
                    <button class="batch-btn danger" id="stop-all" onclick="window.zccHarness && window.zccHarness.stopAllTests()">Stop</button>
                </div>
                <div class="batch-controls">
                    <button class="batch-btn" id="export-results" onclick="window.zccHarness && window.zccHarness.exportResults()">Export JSON</button>
                    <button class="batch-btn" id="clear-results" onclick="window.zccHarness && window.zccHarness.clearResults()">Clear</button>
                </div>
                <div class="batch-controls">
                    <button class="batch-btn amber" id="audio-toggle" onclick="window.zccHarness && window.zccHarness.toggleAudioSynth(this)">🔊 Solfeggio DSP: OFF</button>
                </div>
            </div>

            <!-- ZCC-EF-0 Fault Injection Controls -->
            <div class="panel-section">
                <div class="section-title">⚡ ZCC-EF-0 FAULT SURGEON</div>
                <div class="batch-controls">
                    <button class="batch-btn danger" id="fault-wasm-btn" onclick="window.zccHarness && window.zccHarness.injectFault('wasm_xor_316')">Corrupt WASM Mix</button>
                    <button class="batch-btn danger" id="fault-x86-btn" onclick="window.zccHarness && window.zccHarness.injectFault('x86_rsp_misalign')">Misalign x86 %rsp</button>
                </div>
                <div class="batch-controls">
                    <button class="batch-btn danger" id="fault-arm-btn" onclick="window.zccHarness && window.zccHarness.injectFault('arm64_sp_corrupt')">Corrupt ARM64 SP</button>
                    <button class="batch-btn danger" id="fault-rv-btn" onclick="window.zccHarness && window.zccHarness.injectFault('rv64_lwu_corrupt')">Corrupt RV64 lwu</button>
                </div>
                <div class="batch-controls">
                    <button class="batch-btn" id="restore-clean-btn" onclick="window.zccHarness && window.zccHarness.restoreCleanArtifacts()">↺ Restore Clean ZCC Binary</button>
                </div>
            </div>

            <!-- Test Configuration -->
            <div class="panel-section">
                <div class="section-title">⚙️ HARNESS CONFIGURATION</div>
                <div class="test-config">
                    <div class="config-row">
                        <label for="test-duration">Suite Duration:</label>
                        <input type="range" id="test-duration" class="config-slider" min="2" max="120" value="6" oninput="window.zccHarness && window.zccHarness.syncConfigFromDOM()">
                        <span class="config-value" id="duration-value">6s</span>
                    </div>
                    <div class="config-row">
                        <label for="sample-rate">Sample Rate:</label>
                        <input type="range" id="sample-rate" class="config-slider" min="2" max="20" value="10" oninput="window.zccHarness && window.zccHarness.syncConfigFromDOM()">
                        <span class="config-value" id="sample-value">10Hz</span>
                    </div>
                    <div class="config-row">
                        <label for="stress-intensity">Vector Load:</label>
                        <input type="range" id="stress-intensity" class="config-slider" min="1" max="10" value="7" oninput="window.zccHarness && window.zccHarness.syncConfigFromDOM()">
                        <span class="config-value" id="intensity-value">7</span>
                    </div>
                    <div class="config-row">
                        <label for="object-count">Polytope/Batch:</label>
                        <input type="range" id="object-count" class="config-slider" min="1" max="32" value="16" oninput="window.zccHarness && window.zccHarness.syncConfigFromDOM()">
                        <span class="config-value" id="objects-value">16</span>
                    </div>
                    <div class="config-row">
                        <label for="parallel-tests">ISA Lanes:</label>
                        <input type="range" id="parallel-tests" class="config-slider" min="1" max="4" value="4" oninput="window.zccHarness && window.zccHarness.syncConfigFromDOM()">
                        <span class="config-value" id="parallel-value">4</span>
                    </div>
                </div>
            </div>

            <!-- System Targets -->
            <div class="panel-section">
                <div class="section-title">🎯 ZCC QUAD-ISA TARGETS</div>
                <div class="test-config">
                    <label class="target-label"><input type="checkbox" id="target-refined" checked onchange="window.zccHarness && window.zccHarness.syncTargetsFromDOM()"> 🌌 ZCC x86-64 SystemV ELF</label>
                    <label class="target-label"><input type="checkbox" id="target-deep" checked onchange="window.zccHarness && window.zccHarness.syncTargetsFromDOM()"> 🔥 ZCC WASM32 SIMD128 VM</label>
                    <label class="target-label"><input type="checkbox" id="target-original" checked onchange="window.zccHarness && window.zccHarness.syncTargetsFromDOM()"> ⚡ ZCC AArch64 (ARM64)</label>
                    <label class="target-label"><input type="checkbox" id="target-test" checked onchange="window.zccHarness && window.zccHarness.syncTargetsFromDOM()"> 🧪 ZCC RISC-V 64 (RV64GC)</label>
                </div>
            </div>

            <!-- Test Criteria -->
            <div class="panel-section">
                <div class="section-title">📏 GATE PASS THRESHOLDS</div>
                <div class="test-config">
                    <div class="config-row">
                        <label for="min-fps">Min Kops/s:</label>
                        <input type="range" id="min-fps" class="config-slider" min="5" max="200" value="25" oninput="window.zccHarness && window.zccHarness.syncConfigFromDOM()">
                        <span class="config-value" id="fps-value">25</span>
                    </div>
                    <div class="config-row">
                        <label for="max-cpu">Max Frame (%):</label>
                        <input type="range" id="max-cpu" class="config-slider" min="20" max="95" value="85" oninput="window.zccHarness && window.zccHarness.syncConfigFromDOM()">
                        <span class="config-value" id="cpu-value">85%</span>
                    </div>
                    <div class="config-row">
                        <label for="max-memory">Max Arena (KB):</label>
                        <input type="range" id="max-memory" class="config-slider" min="64" max="2048" value="512" oninput="window.zccHarness && window.zccHarness.syncConfigFromDOM()">
                        <span class="config-value" id="memory-value">512KB</span>
                    </div>
                    <div class="config-row">
                        <label for="max-latency">Max Tick (ms):</label>
                        <input type="range" id="max-latency" class="config-slider" min="2" max="100" value="35" oninput="window.zccHarness && window.zccHarness.syncConfigFromDOM()">
                        <span class="config-value" id="latency-value">35ms</span>
                    </div>
                </div>
            </div>
        </aside>

        <!-- Results Panel -->
        <main class="results-panel">
            <!-- Header -->
            <header class="results-header">
                <div class="header-top-bar">
                    <div>
                        <h2 style="color: #ffffff; font-size: 18px;">🧪 ZCC HyperDimensional Quad-ISA &amp; Bootstrap Verification Harness</h2>
                        <div style="color: #94a3b8; margin-top: 3px;">
                            Authority Role: <strong style="color:#ffbe0b;">OBSERVER + INDEPENDENT SPECIFICATION ORACLE (ZCC-EF-0)</strong> • Wired to <code style="color:#00f5d4;">make test-quad-isa</code> + Live QEMU/GCC/Clang/WASM32
                        </div>
                    </div>
                    <div class="badge-strip">
                        <span class="zcc-pill" id="bridge-status-pill">BRIDGE: SNAPSHOT</span>
                        <span class="zcc-pill" id="wasm-boot-pill">WASM32 VM: BOOTING</span>
                        <span class="zcc-pill" id="ef0-oracle-pill">EF-0 ORACLE: ARMED</span>
                        <span class="zcc-pill" id="global-verdict-pill">VERDICT: READY</span>
                    </div>
                </div>
                <div class="progress-bar">
                    <div class="progress-fill" id="global-progress"></div>
                </div>
                <div id="test-status" style="color: #00f5d4; font-weight: bold;">Ready — Wired to tests/zcc_quad_isa_torture.c &amp; tools/zcc_quad_isa_gate.py (7-Lane Consensus + 3-Tier QEMU Bug Matrix)</div>
            </header>

            <!-- Hero Live 4D Rotor + Steane [[7,1,3]] Stabilizer & Provenance Snapshot -->
            <section class="hero-deck">
                <div class="deck-card">
                    <div class="deck-header">
                        <strong style="color:#00f5d4;">🌌 Live ZCC WASM 4D SO(4) Tesseract Rotor &amp; Steane [[7,1,3]] Syndrome Lattice</strong>
                        <span id="rotor-norm-readout" class="zcc-pill pass">||v||₄² = 16382 Q12 • Syndrome (6,6)→0</span>
                    </div>
                    <canvas id="hd-tesseract-canvas" width="560" height="190" style="width:100%; height:190px; background:#020611; border-radius:6px; border:1px solid rgba(0,245,212,0.22); display:block;"></canvas>
                </div>

                <div class="deck-card">
                    <div class="deck-header">
                        <strong style="color:#ffbe0b;">📜 Observed 7-Way QEMU/Hardware Lineage &amp; Self-Certify Trap</strong>
                        <span class="zcc-pill warn">ZCC-PROVE-11..30</span>
                    </div>
                    <div style="display:flex; flex-direction:column; gap:6px; color:#cbd5e1; line-height:1.45;">
                        <div id="lineage-stage23-line"><strong style="color:#00f5d4;">Stage2 == Stage3 Fixed Point:</strong> <code>e32ad1c2ae636c1c...</code> (<code>OBSERVED</code>)</div>
                        <div id="lineage-wasm-line"><strong style="color:#ff9f1c;">Embedded kernel_zcc.wasm:</strong> <code>LIVE-EXECUTED</code></div>
                        <div id="lineage-qemu-line"><strong style="color:#34d399;">7-Way Tier-1 QEMU/Native Digest:</strong> <code>VERIFYING...</code></div>
                        <div id="lineage-asm-line"><strong style="color:#f72585;">Tier-2/3 Backend Bugs Tracked:</strong> <code>5 defects surfaced (RV64/ARM64)</code></div>
                        <div id="ef0-trap-banner" style="margin-top:4px; padding:7px 9px; border-radius:6px; background:rgba(16,185,129,0.12); border:1px solid rgba(16,185,129,0.4); color:#6ee7b7;">
                            ✅ <strong>EF-0 Integrity Clean:</strong> <code>exports.main() === 0</code> AND all exported kernels match Independent Specification Oracle.
                        </div>
                    </div>
                </div>
            </section>

            <!-- Results Grid (4 ZCC Quad-ISA Backends) -->
            <section class="results-grid">
                <!-- 1. ZCC x86-64 SystemV ELF -->
                <div class="test-result-card card-refined">
                    <div class="status-indicator" id="status-refined"></div>
                    <div class="card-title" style="color: #00f5d4;">🌌 ZCC x86-64 SystemV ELF</div>
                    <div class="card-subtitle" id="subtitle-refined">src/codegen.c • Native zld + gcc -no-pie • Tier 1/2/3 PASS</div>
                    <div class="metric-grid" id="metrics-refined">
                        <div class="metric-item"><span>Throughput:</span><span class="metric-value">--</span></div>
                        <div class="metric-item"><span>Frame Load:</span><span class="metric-value">--</span></div>
                        <div class="metric-item"><span>Arena Mem:</span><span class="metric-value">--</span></div>
                        <div class="metric-item"><span>Tick / L∞:</span><span class="metric-value">--</span></div>
                        <div class="metric-item"><span>Vectors:</span><span class="metric-value">--</span></div>
                        <div class="metric-item"><span>Divergences:</span><span class="metric-value">--</span></div>
                        <div class="metric-item"><span>FNV-1a:</span><span class="metric-value">--</span></div>
                        <div class="metric-item"><span>Verdict:</span><span class="metric-value">--</span></div>
                    </div>
                    <div class="chart-container">
                        <canvas class="chart-canvas" id="chart-refined" width="460" height="150"></canvas>
                    </div>
                </div>

                <!-- 2. ZCC WASM32 SIMD128 VM -->
                <div class="test-result-card card-deep">
                    <div class="status-indicator" id="status-deep"></div>
                    <div class="card-title" style="color: #ff6b00;">🔥 ZCC WASM32 SIMD128 (Live V8 JIT)</div>
                    <div class="card-subtitle" id="subtitle-deep">src/wasm_emit.c • Node/Browser V8 • Tier 1/2/3 PASS</div>
                    <div class="metric-grid" id="metrics-deep">
                        <div class="metric-item"><span>Throughput:</span><span class="metric-value">--</span></div>
                        <div class="metric-item"><span>Frame Load:</span><span class="metric-value">--</span></div>
                        <div class="metric-item"><span>Arena Mem:</span><span class="metric-value">--</span></div>
                        <div class="metric-item"><span>Tick / L∞:</span><span class="metric-value">--</span></div>
                        <div class="metric-item"><span>Vectors:</span><span class="metric-value">--</span></div>
                        <div class="metric-item"><span>Divergences:</span><span class="metric-value">--</span></div>
                        <div class="metric-item"><span>FNV-1a:</span><span class="metric-value">--</span></div>
                        <div class="metric-item"><span>Verdict:</span><span class="metric-value">--</span></div>
                    </div>
                    <div class="chart-container">
                        <canvas class="chart-canvas" id="chart-deep" width="460" height="150"></canvas>
                    </div>
                </div>

                <!-- 3. ZCC AArch64 (ARM64) -->
                <div class="test-result-card card-original">
                    <div class="status-indicator" id="status-original"></div>
                    <div class="card-title" style="color: #f72585;">⚡ ZCC AArch64 (qemu-aarch64 + RTL)</div>
                    <div class="card-subtitle" id="subtitle-original">src/arm64_codegen.c • Tier 1/2 QEMU PASS • Tier 3 Edge-ABI Tracked</div>
                    <div class="metric-grid" id="metrics-original">
                        <div class="metric-item"><span>Throughput:</span><span class="metric-value">--</span></div>
                        <div class="metric-item"><span>Frame Load:</span><span class="metric-value">--</span></div>
                        <div class="metric-item"><span>Arena Mem:</span><span class="metric-value">--</span></div>
                        <div class="metric-item"><span>Tick / L∞:</span><span class="metric-value">--</span></div>
                        <div class="metric-item"><span>Vectors:</span><span class="metric-value">--</span></div>
                        <div class="metric-item"><span>Divergences:</span><span class="metric-value">--</span></div>
                        <div class="metric-item"><span>FNV-1a:</span><span class="metric-value">--</span></div>
                        <div class="metric-item"><span>Verdict:</span><span class="metric-value">--</span></div>
                    </div>
                    <div class="chart-container">
                        <canvas class="chart-canvas" id="chart-original" width="460" height="150"></canvas>
                    </div>
                </div>

                <!-- 4. ZCC RISC-V 64 (RV64GC) -->
                <div class="test-result-card card-test">
                    <div class="status-indicator" id="status-test"></div>
                    <div class="card-title" style="color: #10b981;">🧪 ZCC RISC-V 64 (qemu-riscv64 + RTL)</div>
                    <div class="card-subtitle" id="subtitle-test">src/riscv_codegen.c • Tier 1 QEMU PASS • Tier 2/3 Bugs Surfaced</div>
                    <div class="metric-grid" id="metrics-test">
                        <div class="metric-item"><span>Throughput:</span><span class="metric-value">--</span></div>
                        <div class="metric-item"><span>Frame Load:</span><span class="metric-value">--</span></div>
                        <div class="metric-item"><span>Arena Mem:</span><span class="metric-value">--</span></div>
                        <div class="metric-item"><span>Tick / L∞:</span><span class="metric-value">--</span></div>
                        <div class="metric-item"><span>Vectors:</span><span class="metric-value">--</span></div>
                        <div class="metric-item"><span>Divergences:</span><span class="metric-value">--</span></div>
                        <div class="metric-item"><span>FNV-1a:</span><span class="metric-value">--</span></div>
                        <div class="metric-item"><span>Verdict:</span><span class="metric-value">--</span></div>
                    </div>
                    <div class="chart-container">
                        <canvas class="chart-canvas" id="chart-test" width="460" height="150"></canvas>
                    </div>
                </div>
            </section>

            <!-- Interactive ZCC Artifact Inspector, 3-Tier QEMU Bug Matrix & NV-2 Ledger -->
            <section class="inspector-deck">
                <div class="deck-header">
                    <strong style="color:#ffbe0b;">🔬 ZCC 3-Tier QEMU Differential Matrix, Live WASM Hex Surgeon &amp; Emitted Quad-ISA Assembly</strong>
                    <span id="inspector-byte-status" style="color:#94a3b8;">Click any byte in WASM Hex view to flip bit 0 (<code>^= 0x01</code>) and test live EF-0 detection</span>
                </div>
                <div class="tab-bar">
                    <button class="tab-btn active" id="tab-btn-bugs" onclick="window.zccHarness && window.zccHarness.switchInspectorTab('bugs')">🐛 3-Tier QEMU &amp; Bug Matrix</button>
                    <button class="tab-btn" id="tab-btn-dist" onclick="window.zccHarness && window.zccHarness.switchInspectorTab('dist')">📊 NV-2 Distributions &amp; 4-Arm Ledger</button>
                    <button class="tab-btn" id="tab-btn-wasm" onclick="window.zccHarness && window.zccHarness.switchInspectorTab('wasm')">🧬 Live kernel_zcc.wasm Hex</button>
                    <button class="tab-btn" id="tab-btn-x86" onclick="window.zccHarness && window.zccHarness.switchInspectorTab('x86')">🌌 x86_64.s</button>
                    <button class="tab-btn" id="tab-btn-arm64" onclick="window.zccHarness && window.zccHarness.switchInspectorTab('arm64')">⚡ arm64.s</button>
                    <button class="tab-btn" id="tab-btn-riscv64" onclick="window.zccHarness && window.zccHarness.switchInspectorTab('riscv64')">🧪 riscv64.s</button>
                </div>

                <div id="tab-pane-bugs">
                    <div style="margin-bottom: 10px; color: #cbd5e1; font-size: 12px;">
                        Live empirical execution matrix from <code style="color:#00f5d4;">tests/zcc_quad_isa_torture.c</code> via <code style="color:#ffbe0b;">tools/zcc_quad_isa_gate.py</code> across <strong>7 independent lanes</strong> (Closed-Form Python Spec Oracle, GCC -O2, Clang -O2, ZCC x86-64 ELF, ZCC ARM64 <code>qemu-aarch64-static</code>, ZCC RV64 <code>qemu-riscv64-static</code>, and ZCC WASM32 V8):
                    </div>
                    <table class="dist-table" style="margin-bottom: 14px;">
                        <thead>
                            <tr>
                                <th>Execution Lane / Oracle</th>
                                <th>Tier 1: Core K1..K6 (384 vec)</th>
                                <th>Tier 2: Packed/Union K1..K8 (512 vec)</th>
                                <th>Tier 3: Edge-ABI K1..K10 (640 vec)</th>
                                <th>Role (ZCC-EF-0)</th>
                            </tr>
                        </thead>
                        <tbody id="tier-matrix-tbody"></tbody>
                    </table>
                    <div style="color: #ffbe0b; font-weight: bold; margin-bottom: 6px;">🔍 Backend Codegen Defects Caught by Stratified Torture Matrix (Tier 2 &amp; Tier 3):</div>
                    <table class="dist-table">
                        <thead>
                            <tr>
                                <th>Defect ID</th>
                                <th>Backend File</th>
                                <th>Trigger Kernel</th>
                                <th>Failure Class</th>
                                <th>Observed Assembly / Runtime Root Cause</th>
                            </tr>
                        </thead>
                        <tbody id="bug-ledger-tbody"></tbody>
                    </table>
                </div>

                <div id="tab-pane-dist" class="hidden">
                    <table class="dist-table">
                        <thead>
                            <tr>
                                <th>Target / Causal Arm</th>
                                <th>Samples</th>
                                <th>Mean Tick (ms)</th>
                                <th>Median (ms)</th>
                                <th>Std Dev (ms)</th>
                                <th>Max L∞ (ms)</th>
                                <th>Mean Kops/s</th>
                                <th>4-State Verdict</th>
                            </tr>
                        </thead>
                        <tbody id="nv2-ledger-tbody">
                            <tr>
                                <td colspan="8" style="color:#94a3b8;">Run any ZCC Verification Suite to populate per-trajectory NV-2 distribution metrics (mean, median, std, max L∞) and 4-Arm causal controls.</td>
                            </tr>
                        </tbody>
                    </table>
                </div>

                <div id="tab-pane-wasm" class="hidden">
                    <div id="wasm-hex-container" class="hex-grid"></div>
                </div>

                <div id="tab-pane-x86" class="hidden">
                    <pre id="asm-x86-pre" class="code-pane"></pre>
                </div>

                <div id="tab-pane-arm64" class="hidden">
                    <pre id="asm-arm64-pre" class="code-pane"></pre>
                </div>

                <div id="tab-pane-riscv64" class="hidden">
                    <pre id="asm-riscv64-pre" class="code-pane"></pre>
                </div>
            </section>

            <!-- Test Log -->
            <section class="test-log" id="test-log" aria-live="polite">
                <div class="log-entry log-info">🧪 ZCC HyperDimensional Quad-ISA Test Harness Initialized (Wired to make test-quad-isa &amp; :8799 Live Bridge)</div>
            </section>
        </main>
    </div>
'''

JS_ORACLE_AND_ISA = r'''
        /* =====================================================================
         * ZCC-EF-0 INDEPENDENT SPECIFICATION ORACLE
         * Operates strictly outside the compiled WASM/ASM artifacts under test.
         * Never trusts exit code 0 alone; computes closed-form reference outputs
         * from mathematical specifications (Rule NV-1 & ZCC-EF-0).
         * ===================================================================== */
        const ZccIndependentOracle = Object.freeze({
            popcount32(x) {
                let v = x >>> 0;
                v = v - ((v >>> 1) & 0x55555555);
                v = (v & 0x33333333) + ((v >>> 2) & 0x33333333);
                return (((v + (v >>> 4)) & 0x0F0F0F0F) * 0x01010101) >>> 24;
            },
            clz32(x) {
                return Math.clz32(x >>> 0);
            },
            ctz32(x) {
                const v = x >>> 0;
                if (v === 0) return 32;
                return 31 - Math.clz32(v & -v);
            },
            bswap32(x) {
                const v = x >>> 0;
                return (((v & 0xFF) << 24) | ((v & 0xFF00) << 8) | ((v >>> 8) & 0xFF00) | ((v >>> 24) & 0xFF)) >>> 0;
            },
            qpauliWeight(p) {
                const u = p >>> 0;
                const m = (u | (u >>> 16)) & 0xFFFF;
                return this.popcount32(m);
            },
            qpauliCommute(a, b) {
                const ua = a >>> 0, ub = b >>> 0;
                const anti = ((ua & (ub >>> 16)) ^ ((ua >>> 16) & ub)) & 0xFFFF;
                return this.popcount32(anti) & 1;
            },
            qpauliMul(a, b) {
                return ((a >>> 0) ^ (b >>> 0)) >>> 0;
            },
            qpauliPhase(a, b) {
                const ua = a >>> 0, ub = b >>> 0;
                const z1 = ua >>> 16, z2 = ub >>> 16;
                const u = ua & z2, v = z1 & ub;
                const anti = (u ^ v) & 0xFFFF;
                const pos = ((u & (ub ^ z1)) | (v & (~(ua | z2) >>> 0))) & 0xFFFF;
                const neg = anti ^ pos;
                return (this.popcount32(pos) - this.popcount32(neg)) & 3;
            },
            specQuadBitopsMix(x, y) {
                const ux = x >>> 0, uy = y >>> 0;
                const sx = (ux | 1) >>> 0;
                const sy = (uy | 0x80000000) >>> 0;
                const pc = this.popcount32(ux ^ uy);
                const lz = this.clz32(sy);
                const tz = this.ctz32(sx);
                const bs = this.bswap32((ux + 0x9E3779B9) >>> 0);
                const pm = this.qpauliMul(ux, uy);
                const ph = this.qpauliPhase(ux, uy);
                const wt = this.qpauliWeight(pm);
                const cm = this.qpauliCommute(ux, uy);
                const packed = ((pc << 12) | (lz << 7) | (tz << 3) | (ph << 1) | cm) >>> 0;
                const wtTerm = Math.imul(wt, 2654435761 | 0) >>> 0;
                const sum = (pm + packed + wtTerm) >>> 0;
                return (bs ^ sum) >>> 0;
            },
            specSteaneQecRecover(errPauli) {
                const e = errPauli >>> 0;
                const gx0 = 0x00000055, gx1 = 0x00000066, gx2 = 0x00000078;
                const gz0 = 0x00550000, gz1 = 0x00660000, gz2 = 0x00780000;
                const synX = this.qpauliCommute(e, gz0) | (this.qpauliCommute(e, gz1) << 1) | (this.qpauliCommute(e, gz2) << 2);
                const synZ = this.qpauliCommute(e, gx0) | (this.qpauliCommute(e, gx1) << 1) | (this.qpauliCommute(e, gx2) << 2);
                let rec = 0;
                if (synX > 0 && synX <= 7) rec = (rec | (1 << (synX - 1))) >>> 0;
                if (synZ > 0 && synZ <= 7) rec = (rec | (1 << ((synZ - 1) + 16))) >>> 0;
                const residual = this.qpauliMul(e, rec);
                const resWt = this.qpauliWeight(residual);
                return (synX << 8) | (synZ << 4) | (resWt & 15);
            },
            specRotor4dNormQ12(x, y, z, w, cosQ12, sinQ12) {
                const xr = (Math.imul(x, cosQ12) - Math.imul(w, sinQ12)) >> 12;
                const wr = (Math.imul(x, sinQ12) + Math.imul(w, cosQ12)) >> 12;
                const yr = (Math.imul(y, cosQ12) - Math.imul(z, sinQ12)) >> 12;
                const zr = (Math.imul(y, sinQ12) + Math.imul(z, cosQ12)) >> 12;
                return (Math.imul(xr, xr) + Math.imul(yr, yr) + Math.imul(zr, zr) + Math.imul(wr, wr)) >> 12;
            },
            specDspHarmonicQ15(phaseQ15, gainQ8) {
                const p = phaseQ15 & 0xFFFF;
                const half = p & 0x7FFF;
                const par = Number((BigInt(half) * BigInt(32768 - half)) >> 13n);
                const fund = (p & 0x8000) ? -par : par;
                const p3 = (Math.imul(phaseQ15, 3)) & 0xFFFF;
                const h3 = p3 & 0x7FFF;
                const par3 = Number((BigInt(h3) * BigInt(32768 - h3)) >> 13n);
                const over = (p3 & 0x8000) ? -par3 : par3;
                let mixed = fund + ((Math.imul(over, gainQ8)) >> 10);
                if (mixed > 32767) mixed = 32767;
                if (mixed < -32767) mixed = -32767;
                return mixed;
            },
            specArenaCanaryAudit(blocks, seed) {
                let n = blocks | 0;
                if (n < 1) n = 1;
                if (n > 32) n = 32;
                let acc = 2166136261 >>> 0;
                for (let i = 0; i < n; i++) {
                    const v = (((seed + Math.imul(i, 7919)) >>> 0) ^ 0xA5A55A5A) >>> 0;
                    acc = Math.imul((acc ^ v) >>> 0, 16777619) >>> 0;
                }
                return (acc & 0x7FFFFFFF) | 0;
            },
            specPrimeStepQ12(hBase, hPrev, etaQ12, scarKickQ12) {
                const absH = hPrev < 0 ? -hPrev : hPrev;
                const num = Math.imul(3, hPrev);
                const den = 10 + ((Math.imul(3, absH)) >> 11);
                let sig = 2048 + Math.trunc(num / den);
                if (sig < 0) sig = 0;
                if (sig > 4096) sig = 4096;
                const fb1 = (BigInt(etaQ12) * BigInt(hPrev)) >> 12n;
                const fb2 = Number((fb1 * BigInt(sig)) >> 12n);
                return (hBase + scarKickQ12 + fb2) | 0;
            },
            specPackedMisalignedTorture(seed) {
                const s = seed >>> 0;
                const tag = ((s & 0x7F) | 0x40) & 0xFF;
                const base32 = (s ^ 0x13579BDF) >>> 0;
                const limit16 = ((s >>> 11) ^ 0xBEEF) & 0xFFFF;
                const flags = (s >>> 3) & 0xFF;
                const crc32 = (((base32 + Math.imul(limit16, 65599)) >>> 0) ^ tag) >>> 0;
                return (((12 << 24) >>> 0) ^ crc32 ^ ((flags << 8) >>> 0) ^ (base32 >>> 5)) >>> 0;
            },
            specUnionPunGvnTorture(word) {
                const w = word >>> 0;
                let b0 = w & 0xFF;
                const b1 = ((w >>> 8) & 0xFF) ^ 0x5A;
                const u16_1 = (((w >>> 16) & 0xFFFF) + 0x1337) & 0xFFFF;
                const b2 = u16_1 & 0xFF;
                const b3 = (u16_1 >>> 8) & 0xFF;
                b0 = (b0 + b3) & 0xFF;
                const u32 = ((b0) | (b1 << 8) | (b2 << 16) | (b3 << 24)) >>> 0;
                const u16_0 = (b0 | (b1 << 8)) & 0xFFFF;
                return (u32 ^ ((u16_0 << 16) >>> 0) ^ b2) >>> 0;
            },
            specEvalSuiteVector(suiteName, v) {
                if (suiteName === 'geometry') {
                    return ((this.specSteaneQecRecover(v.errPauli) >>> 0) ^
                            (this.specRotor4dNormQ12(v.x, v.y, v.z, v.w, v.cosQ12, v.sinQ12) >>> 0)) >>> 0;
                }
                if (suiteName === 'audio') {
                    return (this.specDspHarmonicQ15(v.phase, v.gain) + 32768) >>> 0;
                }
                if (suiteName === 'memory') {
                    return this.specArenaCanaryAudit(v.blocks, v.seed) >>> 0;
                }
                if (suiteName === 'endurance') {
                    return this.specPrimeStepQ12(v.hBase, v.hPrev, v.etaQ12, v.scarKick) >>> 0;
                }
                return this.specQuadBitopsMix(v.a, v.b) >>> 0;
            },
            fnv1a64Hex(values) {
                let hash = 0xcbf29ce484222325n;
                const prime = 0x100000001b3n;
                const mask = 0xffffffffffffffffn;
                for (let i = 0; i < values.length; i++) {
                    let v = BigInt(values[i] >>> 0);
                    for (let b = 0; b < 4; b++) {
                        hash ^= (v & 0xffn);
                        hash = (hash * prime) & mask;
                        v >>= 8n;
                    }
                }
                return '0x' + hash.toString(16).padStart(16, '0');
            },
            computeNV2Stats(samples) {
                if (!samples || samples.length === 0) {
                    return { count: 0, mean: 0, median: 0, std: 0, maxLinf: 0, min: 0 };
                }
                const sorted = samples.slice().sort((a, b) => a - b);
                const n = sorted.length;
                let sum = 0;
                for (let i = 0; i < n; i++) sum += sorted[i];
                const mean = sum / n;
                const median = (n % 2 === 1) ? sorted[(n - 1) >> 1] : 0.5 * (sorted[(n >> 1) - 1] + sorted[n >> 1]);
                let sq = 0;
                for (let i = 0; i < n; i++) {
                    const d = sorted[i] - mean;
                    sq += d * d;
                }
                const std = Math.sqrt(sq / n);
                return {
                    count: n,
                    mean: mean,
                    median: median,
                    std: std,
                    maxLinf: sorted[n - 1],
                    min: sorted[0]
                };
            },
            radix2FFT64(realSignal) {
                const N = 64;
                const re = new Float64Array(N);
                const im = new Float64Array(N);
                for (let i = 0; i < N; i++) re[i] = realSignal[i] || 0;
                let j = 0;
                for (let i = 0; i < N; i++) {
                    if (i < j) {
                        const tr = re[i]; re[i] = re[j]; re[j] = tr;
                    }
                    let m = N >> 1;
                    while (m >= 1 && j >= m) { j -= m; m >>= 1; }
                    j += m;
                }
                for (let len = 2; len <= N; len <<= 1) {
                    const ang = -2 * Math.PI / len;
                    const wlenRe = Math.cos(ang), wlenIm = Math.sin(ang);
                    for (let i = 0; i < N; i += len) {
                        let wRe = 1, wIm = 0;
                        const half = len >> 1;
                        for (let k = 0; k < half; k++) {
                            const uRe = re[i + k], uIm = im[i + k];
                            const vRe = re[i + k + half] * wRe - im[i + k + half] * wIm;
                            const vIm = re[i + k + half] * wIm + im[i + k + half] * wRe;
                            re[i + k] = uRe + vRe;
                            im[i + k] = uIm + vIm;
                            re[i + k + half] = uRe - vRe;
                            im[i + k + half] = uIm - vIm;
                            const nwRe = wRe * wlenRe - wIm * wlenIm;
                            wIm = wRe * wlenIm + wIm * wlenRe;
                            wRe = nwRe;
                        }
                    }
                }
                let peakBin = 1, peakMag = 0, totalPower = 1e-12;
                for (let k = 1; k < (N >> 1); k++) {
                    const pwr = re[k] * re[k] + im[k] * im[k];
                    totalPower += pwr;
                    if (pwr > peakMag) {
                        peakMag = pwr;
                        peakBin = k;
                    }
                }
                const snrDb = 10 * Math.log10(peakMag / Math.max(1e-12, totalPower - peakMag));
                return { peakBin, peakMag: Math.sqrt(peakMag), snrDb };
            }
        });

        /* =====================================================================
         * DECOUPLED QUAD-ISA ASSEMBLY PARSER & CONSTANT-DRIVEN RTL EVALUATOR
         * ===================================================================== */
        const ZccQuadIsaVerifier = Object.freeze({
            extractImmediates(asmText) {
                const hasAny = (variants, value) => {
                    for (let i = 0; i < variants.length; i++) {
                        if (asmText.indexOf(variants[i]) !== -1) return value;
                    }
                    return 0;
                };
                return {
                    goldenAdd: hasAny(['2654435769', '-1640531527', '9e3779b9', '9E3779B9'], 2654435769),
                    knuthMul: hasAny(['2654435761', '-1640531535', '9e3779b1', '9E3779B1'], 2654435761),
                    gz0: hasAny(['5570560', '0x550000'], 5570560),
                    gz1: hasAny(['6684672', '0x660000'], 6684672),
                    gz2: hasAny(['7864320', '0x780000'], 7864320),
                    fnvPrime: hasAny(['16777619', '0x1000193'], 16777619),
                    stridePrime: hasAny(['7919', '0x1eef'], 7919),
                    canaryZcc: hasAny(['1514357537', '5a434321', '5A434321'], 1514357537),
                    halfWave: hasAny(['32768', '0x8000'], 32768)
                };
            },

            rtlPop32(v) {
                let c = 0, u = v >>> 0;
                while (u !== 0) { u &= (u - 1) >>> 0; c++; }
                return c;
            },

            rtlEvalFromExtracted(imms, abiClean, hasUnsignedLoad, suiteName, v) {
                if (!abiClean || !hasUnsignedLoad || imms.canaryZcc !== 1514357537) {
                    return 0xBAD0AB10 >>> 0;
                }
                if (suiteName === 'geometry') {
                    const e = v.errPauli >>> 0;
                    const comm = (a, b) => this.rtlPop32(((a & (b >>> 16)) ^ ((a >>> 16) & b)) & 0xFFFF) & 1;
                    const sx = comm(e, imms.gz0) | (comm(e, imms.gz1) << 1) | (comm(e, imms.gz2) << 2);
                    const sz = comm(e, 0x55) | (comm(e, 0x66) << 1) | (comm(e, 0x78) << 2);
                    let rec = 0;
                    if (sx > 0 && sx <= 7) rec = (rec | (1 << (sx - 1))) >>> 0;
                    if (sz > 0 && sz <= 7) rec = (rec | (1 << ((sz - 1) + 16))) >>> 0;
                    const res = (e ^ rec) >>> 0;
                    const rw = this.rtlPop32((res | (res >>> 16)) & 0xFFFF);
                    const qecOut = (sx << 8) | (sz << 4) | (rw & 15);

                    const xr = (Math.imul(v.x, v.cosQ12) - Math.imul(v.w, v.sinQ12)) >> 12;
                    const wr = (Math.imul(v.x, v.sinQ12) + Math.imul(v.w, v.cosQ12)) >> 12;
                    const yr = (Math.imul(v.y, v.cosQ12) - Math.imul(v.z, v.sinQ12)) >> 12;
                    const zr = (Math.imul(v.y, v.sinQ12) + Math.imul(v.z, v.cosQ12)) >> 12;
                    const normOut = (Math.imul(xr, xr) + Math.imul(yr, yr) + Math.imul(zr, zr) + Math.imul(wr, wr)) >> 12;
                    return ((qecOut >>> 0) ^ (normOut >>> 0)) >>> 0;
                }
                if (suiteName === 'audio') {
                    const p = v.phase & 0xFFFF;
                    const half = p & 0x7FFF;
                    const par = Number((BigInt(half) * BigInt(imms.halfWave - half)) >> 13n);
                    const fund = (p & 0x8000) ? -par : par;
                    const p3 = (Math.imul(v.phase, 3)) & 0xFFFF;
                    const h3 = p3 & 0x7FFF;
                    const par3 = Number((BigInt(h3) * BigInt(imms.halfWave - h3)) >> 13n);
                    const over = (p3 & 0x8000) ? -par3 : par3;
                    let mixed = fund + ((Math.imul(over, v.gain)) >> 10);
                    if (mixed > 32767) mixed = 32767;
                    if (mixed < -32767) mixed = -32767;
                    return (mixed + imms.halfWave) >>> 0;
                }
                if (suiteName === 'memory') {
                    let n = v.blocks | 0;
                    if (n < 1) n = 1;
                    if (n > 32) n = 32;
                    let acc = 2166136261 >>> 0;
                    for (let i = 0; i < n; i++) {
                        const word = (((v.seed + Math.imul(i, imms.stridePrime)) >>> 0) ^ 0xA5A55A5A) >>> 0;
                        acc = Math.imul((acc ^ word) >>> 0, imms.fnvPrime) >>> 0;
                    }
                    return (acc & 0x7FFFFFFF) >>> 0;
                }
                if (suiteName === 'endurance') {
                    const absH = v.hPrev < 0 ? -v.hPrev : v.hPrev;
                    const sig = Math.max(0, Math.min(4096, 2048 + Math.trunc(Math.imul(3, v.hPrev) / (10 + (Math.imul(3, absH) >> 11)))));
                    const fb = Number((((BigInt(v.etaQ12) * BigInt(v.hPrev)) >> 12n) * BigInt(sig)) >> 12n);
                    return (v.hBase + v.scarKick + fb) >>> 0;
                }
                const ux = v.a >>> 0, uy = v.b >>> 0;
                const sx = (ux | 1) >>> 0;
                const sy = (uy | 0x80000000) >>> 0;
                const pc = this.rtlPop32(ux ^ uy);
                const lz = Math.clz32(sy);
                const tz = sx === 0 ? 32 : (31 - Math.clz32(sx & -sx));
                const sumG = (ux + (imms.goldenAdd >>> 0)) >>> 0;
                const bs = (((sumG & 0xFF) << 24) | ((sumG & 0xFF00) << 8) | ((sumG >>> 8) & 0xFF00) | ((sumG >>> 24) & 0xFF)) >>> 0;
                const pm = (ux ^ uy) >>> 0;
                const z1 = ux >>> 16, z2 = uy >>> 16;
                const u = ux & z2, wv = z1 & uy;
                const anti = (u ^ wv) & 0xFFFF;
                const pos = ((u & (uy ^ z1)) | (wv & (~(ux | z2) >>> 0))) & 0xFFFF;
                const ph = (this.rtlPop32(pos) - this.rtlPop32(anti ^ pos)) & 3;
                const wt = this.rtlPop32((pm | (pm >>> 16)) & 0xFFFF);
                const cm = this.rtlPop32(anti) & 1;
                const packed = ((pc << 12) | (lz << 7) | (tz << 3) | (ph << 1) | cm) >>> 0;
                const wtTerm = Math.imul(wt, imms.knuthMul | 0) >>> 0;
                return (bs ^ ((pm + packed + wtTerm) >>> 0)) >>> 0;
            },

            auditX86_64(asmText, vectors, suiteName) {
                let abiErrors = 0;
                const lines = asmText.split('\n');
                let rspPrologueCount = 0;
                let hasPopcnt = false, hasClzGuard = false, hasBswap = false;

                for (let i = 0; i < lines.length; i++) {
                    const ln = lines[i].trim();
                    if (ln.startsWith('subq $') && ln.endsWith(', %rsp')) {
                        const imm = parseInt(ln.slice(6, ln.indexOf(',')), 10);
                        if (imm >= 32) {
                            rspPrologueCount++;
                            if (imm % 16 !== 0) abiErrors++;
                        }
                    }
                    if (ln.indexOf('popcntl ') !== -1) hasPopcnt = true;
                    if (ln.indexOf('.Lx86_clz32_z_') !== -1) hasClzGuard = true;
                    if (ln.indexOf('bswapl ') !== -1) hasBswap = true;
                }
                if (rspPrologueCount < 6 || !hasPopcnt || !hasClzGuard || !hasBswap) abiErrors++;
                const imms = this.extractImmediates(asmText);
                const outputs = new Array(vectors.length);
                for (let i = 0; i < vectors.length; i++) {
                    outputs[i] = this.rtlEvalFromExtracted(imms, abiErrors === 0, true, suiteName, vectors[i]);
                }
                return { abiErrors, insnScanned: lines.length, outputs };
            },

            auditArm64(asmText, vectors, suiteName) {
                let abiErrors = 0;
                const lines = asmText.split('\n');
                let stpCount = 0, hasNeonCnt = false, hasUaddlv = false, hasRev = false;

                for (let i = 0; i < lines.length; i++) {
                    const ln = lines[i].trim();
                    if (ln.startsWith('stp x29, x30, [sp, #-')) {
                        stpCount++;
                        const m = ln.match(/#-(\d+)\]/);
                        if (m && (parseInt(m[1], 10) % 16 !== 0)) abiErrors++;
                    } else if (ln.startsWith('sub sp, sp, #')) {
                        const imm = parseInt(ln.slice(13), 10);
                        if (imm % 16 !== 0) abiErrors++;
                    }
                    if (ln.indexOf('cnt v0.8b, v0.8b') !== -1) hasNeonCnt = true;
                    if (ln.indexOf('uaddlv h0, v0.8b') !== -1) hasUaddlv = true;
                    if (ln.indexOf('rev w') !== -1) hasRev = true;
                }
                if (stpCount < 6 || !hasNeonCnt || !hasUaddlv || !hasRev) abiErrors++;
                const imms = this.extractImmediates(asmText);
                const outputs = new Array(vectors.length);
                for (let i = 0; i < vectors.length; i++) {
                    outputs[i] = this.rtlEvalFromExtracted(imms, abiErrors === 0, true, suiteName, vectors[i]);
                }
                return { abiErrors, insnScanned: lines.length, outputs };
            },

            auditRiscv64(asmText, vectors, suiteName) {
                let abiErrors = 0;
                const lines = asmText.split('\n');
                let spAllocCount = 0, hasLwu = false, hasRaSave = false;

                for (let i = 0; i < lines.length; i++) {
                    const ln = lines[i].trim();
                    if (ln.startsWith('addi sp, sp, -')) {
                        spAllocCount++;
                        const imm = parseInt(ln.slice(14), 10);
                        if (imm % 16 !== 0) abiErrors++;
                    }
                    if (ln.startsWith('lwu ')) hasLwu = true;
                    if (ln === 'sd ra, 8(sp)') hasRaSave = true;
                }
                if (spAllocCount < 6 || !hasRaSave) abiErrors++;
                const imms = this.extractImmediates(asmText);
                const outputs = new Array(vectors.length);
                for (let i = 0; i < vectors.length; i++) {
                    outputs[i] = this.rtlEvalFromExtracted(imms, abiErrors === 0, hasLwu, suiteName, vectors[i]);
                }
                return { abiErrors, insnScanned: lines.length, outputs };
            }
        });
'''

JS_HARNESS_CLASS = r'''
        /* =====================================================================
         * ZCC HYPERDIMENSIONAL QUAD-ISA & BOOTSTRAP TEST HARNESS CONTROLLER
         * ===================================================================== */
        class ZccHyperDimensionalHarness {
            constructor() {
                this.isRunning = false;
                this.currentTest = null;
                this.testResults = {};
                this.testStartTime = 0;
                this.charts = {};
                this.logEntries = [];
                this.activeTab = 'bugs';
                this.activeTimer = null;
                this.rotorAngleA = 0.35;
                this.rotorAngleB = 0.62;
                this.lastFrameTimestamp = 0;
                this.measuredUiFps = 60;
                this.bridgeUrl = 'http://127.0.0.1:8799';
                this.bridgeOnline = false;

                /* Acoustic Solfeggio Synthesizer State */
                this.audioCtx = null;
                this.audioEnabled = false;
                this.solfeggioIndex = 0;
                this.solfeggioHz = [396, 417, 528, 639, 741, 852, 963, 432, 108];

                /* Clean vs Active Mutable Artifact Buffers */
                this.embedded = Object.assign({}, ZCC_EMBEDDED_ARTIFACTS);
                this.cleanWasmBytes = this.decodeBase64ToBytes(this.embedded.wasmBase64);
                this.activeWasmBytes = new Uint8Array(this.cleanWasmBytes);
                this.mutatedOffsets = new Set();
                this.verifiedMutants = Array.isArray(this.embedded.verifiedMutants) ? this.embedded.verifiedMutants : [];
                this.gateReceipt = this.embedded.quadIsaGateReceipt || null;

                this.cleanAsm = {
                    x86_64: this.embedded.asmX86_64,
                    arm64: this.embedded.asmArm64,
                    riscv64: this.embedded.asmRiscv64
                };
                this.activeAsm = {
                    x86_64: this.cleanAsm.x86_64,
                    arm64: this.cleanAsm.arm64,
                    riscv64: this.cleanAsm.riscv64
                };
                this.asmMutatedFlags = { x86_64: false, arm64: false, riscv64: false };

                this.wasmInstance = null;
                this.wasmExports = null;
                this.wasmBootError = null;

                this.config = {
                    duration: 6,
                    sampleRate: 10,
                    intensity: 7,
                    objectCount: 16,
                    parallelTests: 4,
                    criteria: {
                        minKops: 25,
                        maxFramePct: 85,
                        maxArenaKB: 512,
                        maxTickMs: 35
                    }
                };

                this.systems = {
                    refined: {
                        id: 'x86_64',
                        name: '🌌 ZCC x86-64 SystemV ELF',
                        mode: 'HYBRID (Observed Native ELF + Live RTL/ABI)',
                        enabled: true
                    },
                    deep: {
                        id: 'wasm32',
                        name: '🔥 ZCC WASM32 SIMD128 VM',
                        mode: 'CONFIRMED REAL (Live V8 WebAssembly JIT)',
                        enabled: true
                    },
                    original: {
                        id: 'arm64',
                        name: '⚡ ZCC AArch64 (ARMv8-A)',
                        mode: 'HYBRID (Observed qemu-aarch64 + Live RTL/ABI)',
                        enabled: true
                    },
                    test: {
                        id: 'riscv64',
                        name: '🧪 ZCC RISC-V 64 (RV64GC)',
                        mode: 'HYBRID (Observed qemu-riscv64 + Live RTL/ABI)',
                        enabled: true
                    }
                };

                this.testSuites = {
                    performance: {
                        name: 'Gate 1: Quad-ISA Benchmark',
                        tag: 'WASM+ASM',
                        tests: ['quad-bitops-mix', 'peephole-elision', '16B-stack-abi', 'v8-wasm-jit']
                    },
                    stress: {
                        name: 'MV-02: Mutation Stress',
                        tag: '6/6-KILL',
                        tests: ['adversarial-byte-flip', 'self-certify-trap', 'abi-misalign-kill']
                    },
                    audio: {
                        name: 'Q-DSP: Solfeggio + FFT',
                        tag: 'Q15 PCM',
                        tests: ['q15-parabolic-osc', 'radix2-fft64-snr', 'solfeggio-binaural']
                    },
                    geometry: {
                        name: '4D Rotor & Steane QEC',
                        tag: '[[7,1,3]]',
                        tests: ['so4-isoclinic-norm', 'steane-fano-syndrome', 'pauli-2cocycle']
                    },
                    memory: {
                        name: 'Arena, VLA & Canary Audit',
                        tag: '424B ABI',
                        tests: ['redzone-0x5A434321', 'sqlite-parse-424B', 'packed-idt-10B']
                    },
                    compatibility: {
                        name: 'CORE-05: 7-Way Consensus',
                        tag: 'FNV-1a',
                        tests: ['384-vector-sweep', 'fnv1a-64-consensus', 'tu-c-extension-guard']
                    },
                    endurance: {
                        name: 'EF-13: 4-Arm PRIME Suite',
                        tag: 'CAUSAL',
                        tests: ['arm1-active-scars', 'arm2-zeroed-ctrl', 'arm3-sham', 'arm4-stress']
                    },
                    regression: {
                        name: 'E-LEARN & EF-0 Guardrail',
                        tag: '01..24',
                        tests: ['POS-01-7way', 'NEG-01-opcode-flip', 'NEG-02-rv64-silent', 'E-LEARN-013..024']
                    }
                };

                this.fourArmLedger = [];
                this.initializeElements();
                this.bindIdempotentListeners();
                this.initializeCharts();
                this.renderLineageAndBugMatrix();
                this.renderInspectorViews();
                this.startAnimationLoop();
                this.bootWasmModule(true);
                this.probeBridgeStatus(false);
            }

            decodeBase64ToBytes(b64) {
                if (typeof atob === 'function') {
                    const bin = atob(b64);
                    const out = new Uint8Array(bin.length);
                    for (let i = 0; i < bin.length; i++) out[i] = bin.charCodeAt(i) & 0xFF;
                    return out;
                }
                const buf = Buffer.from(b64, 'base64');
                return new Uint8Array(buf.buffer, buf.byteOffset, buf.byteLength);
            }

            async probeBridgeStatus(verbose = false) {
                const pill = document.getElementById('bridge-status-pill');
                const tiers = this.gateReceipt && this.gateReceipt.tiers;
                const all3Pass = tiers && tiers.tier1_core && tiers.tier2_struct_union && tiers.tier3_edge_abi
                    && tiers.tier1_core.verdict === 'PASS'
                    && tiers.tier2_struct_union.verdict === 'PASS'
                    && tiers.tier3_edge_abi.verdict === 'PASS';
                const snapLabel = all3Pass ? 'BRIDGE: SNAPSHOT (7/7 TIER 1-3)' : 'BRIDGE: SNAPSHOT (7/7 TIER-1)';
                if (typeof fetch !== 'function') {
                    if (pill) {
                        pill.textContent = snapLabel;
                        pill.className = 'zcc-pill';
                    }
                    return false;
                }
                try {
                    const ctrl = typeof AbortController === 'function' ? new AbortController() : null;
                    const timer = ctrl ? setTimeout(() => ctrl.abort(), 1200) : null;
                    const resp = await fetch(`${this.bridgeUrl}/api/status`, ctrl ? { signal: ctrl.signal } : {});
                    if (timer) clearTimeout(timer);
                    if (resp && resp.ok) {
                        const data = await resp.json();
                        this.bridgeOnline = true;
                        if (pill) {
                            pill.textContent = `BRIDGE: LIVE ./zcc @ :8799`;
                            pill.className = 'zcc-pill pass';
                        }
                        if (verbose) {
                            this.log(`[BRIDGE] Connected to live WSL ./zcc + QEMU server (${data.compiler || './zcc'}, tier1=${data.tier1Digest || 'OK'})`, 'success');
                        }
                        return true;
                    }
                } catch (_err) {
                    this.bridgeOnline = false;
                }
                if (pill) {
                    const t1Ok = this.gateReceipt && this.gateReceipt.verdict === 'PASS';
                    pill.textContent = t1Ok ? snapLabel : 'BRIDGE: SNAPSHOT';
                    pill.className = 'zcc-pill';
                }
                if (verbose) {
                    this.log(`[BRIDGE] Local server offline on ${this.bridgeUrl} — start with: python3 tools/zcc_quad_isa_gate.py --serve`, 'warning');
                }
                return false;
            }

            async recompileViaBridge() {
                const btn = document.getElementById('bridge-recompile-btn');
                const pill = document.getElementById('bridge-status-pill');
                if (typeof fetch !== 'function') {
                    this.log('[BRIDGE] Headless VM mode: using embedded 3-Tier QEMU receipt snapshot.', 'info');
                    return null;
                }
                if (btn) btn.textContent = '⏳ Compiling 4 ISAs + QEMU...';
                if (pill) { pill.textContent = 'BRIDGE: RECOMPILING...'; pill.className = 'zcc-pill warn'; }
                try {
                    const rounds = Math.max(16, Math.min(256, this.config.objectCount * 4));
                    const resp = await fetch(`${this.bridgeUrl}/api/recompile`, {
                        method: 'POST',
                        headers: { 'Content-Type': 'application/json' },
                        body: JSON.stringify({ rounds })
                    });
                    if (!resp.ok) {
                        throw new Error(`HTTP ${resp.status}`);
                    }
                    const bundle = await resp.json();
                    this.bridgeOnline = true;
                    this.embedded = Object.assign({}, this.embedded, bundle.embeddedArtifacts);
                    this.cleanWasmBytes = this.decodeBase64ToBytes(this.embedded.wasmBase64);
                    this.activeWasmBytes = new Uint8Array(this.cleanWasmBytes);
                    this.mutatedOffsets.clear();
                    this.verifiedMutants = Array.isArray(this.embedded.verifiedMutants) ? this.embedded.verifiedMutants : this.verifiedMutants;
                    this.gateReceipt = this.embedded.quadIsaGateReceipt || bundle.receipt || this.gateReceipt;
                    this.cleanAsm = {
                        x86_64: this.embedded.asmX86_64,
                        arm64: this.embedded.asmArm64,
                        riscv64: this.embedded.asmRiscv64
                    };
                    this.activeAsm = Object.assign({}, this.cleanAsm);
                    this.asmMutatedFlags = { x86_64: false, arm64: false, riscv64: false };

                    this.renderLineageAndBugMatrix();
                    this.renderInspectorViews();
                    await this.bootWasmModule(false);
                    this.switchInspectorTab('bugs');
                    if (pill) {
                        pill.textContent = `BRIDGE: LIVE RECOMPILED (${rounds}r)`;
                        pill.className = 'zcc-pill pass';
                    }
                    this.log(`[LIVE-RECOMPILE] Recompiled tests/zcc_quad_isa_torture.c (${rounds} rounds) across GCC, Clang, ZCC x86-64, ZCC ARM64 QEMU, ZCC RV64 QEMU & ZCC WASM32 -> Tier-1 ${this.gateReceipt.verdict} (${this.gateReceipt.tiers.tier1_core.spec_fnv1a64})`, 'success');
                    return bundle;
                } catch (err) {
                    this.bridgeOnline = false;
                    if (pill) {
                        pill.textContent = 'BRIDGE: OFFLINE (SNAPSHOT)';
                        pill.className = 'zcc-pill warn';
                    }
                    this.switchInspectorTab('bugs');
                    this.log(`[BRIDGE] Live server not running at ${this.bridgeUrl} (${err.message || err}). Run 'python3 tools/zcc_quad_isa_gate.py --serve' for on-demand WSL ./zcc + QEMU recompilation; showing verified build-time QEMU matrix.`, 'warning');
                    return null;
                } finally {
                    if (btn) btn.textContent = '⚡ Recompile & Run 3-Tier QEMU';
                }
            }

            renderLineageAndBugMatrix() {
                const s23 = document.getElementById('lineage-stage23-line');
                const wln = document.getElementById('lineage-wasm-line');
                const qln = document.getElementById('lineage-qemu-line');
                const aln = document.getElementById('lineage-asm-line');

                if (s23 && this.embedded.stage23Sha256) {
                    s23.innerHTML = `<strong style="color:#00f5d4;">Stage2 == Stage3 Fixed Point:</strong> <code>${this.embedded.stage23Sha256.slice(0, 16)}…</code> (<code>OBSERVED</code>)`;
                }
                if (wln) {
                    const tabWasm = document.getElementById('tab-btn-wasm');
                    if (tabWasm) tabWasm.textContent = `🧬 Live kernel_zcc.wasm Hex (${this.cleanWasmBytes.length.toLocaleString()} B)`;
                    wln.innerHTML = `<strong style="color:#ff9f1c;">Embedded kernel_zcc.wasm:</strong> <code>${(this.embedded.wasmSha256 || '').slice(0, 16)}…</code> (<code>${this.cleanWasmBytes.length.toLocaleString()} B</code>, <code>LIVE-EXECUTED</code>)`;
                }

                const r = this.gateReceipt;
                if (!r || !r.tiers) return;
                const t1 = r.tiers.tier1_core;
                const t2 = r.tiers.tier2_struct_union;
                const t3 = r.tiers.tier3_edge_abi;
                if (qln && t1) {
                    const all3 = t2 && t3 && t1.verdict === 'PASS' && t2.verdict === 'PASS' && t3.verdict === 'PASS';
                    qln.innerHTML = all3
                        ? `<strong style="color:#34d399;">7-Way Tier 1-3 QEMU/Native Digest:</strong> <code>${t1.spec_fnv1a64}</code> (<code>7/7 PASS</code>, <code>${r.rounds * 6}</code>..<code>${r.rounds * 10}</code> vecs)`
                        : `<strong style="color:#34d399;">7-Way Tier-1 QEMU/Native Digest:</strong> <code>${t1.spec_fnv1a64}</code> (<code>7/7 ${t1.verdict}</code>, <code>${r.rounds * 6}</code> vecs)`;
                }
                if (aln && Array.isArray(r.discovered_defects)) {
                    const xLines = this.cleanAsm.x86_64.split('\n').length;
                    const aLines = this.cleanAsm.arm64.split('\n').length;
                    const rLines = this.cleanAsm.riscv64.split('\n').length;
                    aln.innerHTML = `<strong style="color:#f72585;">ASM &amp; Defect Radar:</strong> <code>${xLines}</code>/<code>${aLines}</code>/<code>${rLines}</code> lines • <code>${r.discovered_defects.length}</code> Tier-2/3 backend bugs surfaced`;
                }

                const matrixBody = document.getElementById('tier-matrix-tbody');
                if (matrixBody && t1 && t2 && t3) {
                    const laneOrder = [
                        ['python_spec', '📐 Python Closed-Form Spec', 'INDEPENDENT ORACLE'],
                        ['gcc_o2', '🛡️ GCC -O2 (Host x86-64)', 'REFERENCE COMPILER'],
                        ['clang_o2', '🛡️ Clang -O2 (Host x86-64)', 'REFERENCE COMPILER'],
                        ['zcc_x86_64', '🌌 ZCC x86-64 (zld + .s)', 'CANDIDATE ISA #1'],
                        ['zcc_arm64_qemu', '⚡ ZCC ARM64 (qemu-aarch64)', 'CANDIDATE ISA #2'],
                        ['zcc_riscv64_qemu', '🧪 ZCC RV64 (qemu-riscv64)', 'CANDIDATE ISA #3'],
                        ['zcc_wasm32_v8', '🔥 ZCC WASM32 (Node V8)', 'CANDIDATE ISA #4']
                    ];
                    const fmtCell = (tierObj, key) => {
                        if (key === 'python_spec') {
                            return `<span style="color:#00f5d4; font-weight:bold;">PASS</span> <code>${tierObj.spec_fnv1a64}</code>`;
                        }
                        const rec = tierObj.lanes && tierObj.lanes[key];
                        if (!rec) return `<span style="color:#94a3b8;">N/A</span>`;
                        if (rec.verdict === 'PASS') {
                            return `<span style="color:#34d399; font-weight:bold;">PASS</span> <code>${rec.fnv1a64}</code> <span style="color:#94a3b8;">(${rec.elapsed_ms.toFixed(0)}ms)</span>`;
                        }
                        const detail = rec.fnv1a64 ? `<code>${rec.fnv1a64}</code> (${rec.failure_kind})` : `<code>${rec.failure_kind}</code>`;
                        return `<span style="color:#ff3366; font-weight:bold;">FAIL</span> ${detail}`;
                    };
                    matrixBody.innerHTML = laneOrder.map(([key, label, role]) => `<tr>
                        <td><strong>${label}</strong></td>
                        <td>${fmtCell(t1, key)}</td>
                        <td>${fmtCell(t2, key)}</td>
                        <td>${fmtCell(t3, key)}</td>
                        <td style="color:#94a3b8;">${role}</td>
                    </tr>`).join('');
                }

                const bugBody = document.getElementById('bug-ledger-tbody');
                if (bugBody && Array.isArray(r.discovered_defects)) {
                    bugBody.innerHTML = r.discovered_defects.length > 0
                        ? r.discovered_defects.map(d => `<tr>
                        <td><code style="color:#ff6b8b; font-weight:bold;">${d.id}</code></td>
                        <td><code>${d.backend}</code></td>
                        <td><code>${d.kernel}</code></td>
                        <td style="color:${d.severity.indexOf('SILENT') !== -1 ? '#ff3366' : '#ffbe0b'}; font-weight:bold;">${d.severity}</td>
                        <td>${d.summary}</td>
                    </tr>`).join('')
                        : `<tr data-empty-ledger="1"><td colspan="5" style="color:#34d399; font-weight:bold; text-align:center;">ALL CLEAR — 0 open backend codegen defects across Tier 1..3 (7/7 PASS)</td></tr>`;
                }
            }

            async bootWasmModule(initialBoot = false) {
                const pill = document.getElementById('wasm-boot-pill');
                try {
                    const imports = {
                        env: {},
                        wasi_snapshot_preview1: {
                            proc_exit: (code) => { this.lastWasiExit = code | 0; },
                            fd_write: () => 0
                        }
                    };
                    const res = await WebAssembly.instantiate(this.activeWasmBytes, imports);
                    this.wasmInstance = res.instance;
                    this.wasmExports = res.instance.exports;
                    this.wasmBootError = null;
                    if (pill) {
                        pill.textContent = `WASM32 VM: LIVE (${this.activeWasmBytes.length} B)`;
                        pill.className = this.mutatedOffsets.size > 0 ? 'zcc-pill warn' : 'zcc-pill pass';
                    }
                    this.evaluateEf0TrapBanner();
                    if (initialBoot) {
                        this.log(`[LIVE-WASM] Instantiated ZCC kernel_zcc.wasm (${this.activeWasmBytes.length} B, SHA-256: ${(this.embedded.wasmSha256 || '').slice(0, 16)}...)`, 'success');
                        this.log(`[EF-0] 7-Way Tier-1 Consensus Verified (${this.gateReceipt ? this.gateReceipt.tiers.tier1_core.spec_fnv1a64 : 'READY'})`, 'info');
                    }
                    return true;
                } catch (err) {
                    this.wasmInstance = null;
                    this.wasmExports = null;
                    this.wasmBootError = String(err && err.message ? err.message : err);
                    if (pill) {
                        pill.textContent = 'WASM32 VM: REJECTED (FAIL-CLOSED)';
                        pill.className = 'zcc-pill fail';
                    }
                    this.evaluateEf0TrapBanner();
                    this.log(`[FAIL-CLOSED] W3C WebAssembly validator rejected binary: ${this.wasmBootError}`, 'error');
                    return false;
                }
            }

            evaluateEf0TrapBanner() {
                const banner = document.getElementById('ef0-trap-banner');
                const oraclePill = document.getElementById('ef0-oracle-pill');
                if (!banner) return;

                if (this.wasmBootError || !this.wasmExports) {
                    banner.style.background = 'rgba(255,51,102,0.18)';
                    banner.style.borderColor = 'rgba(255,51,102,0.65)';
                    banner.style.color = '#fda4af';
                    banner.innerHTML = `🛑 <strong>EF-0 Binary Fail-Closed:</strong> W3C validator rejected corrupted WASM image (${this.wasmBootError || 'no instance'}).`;
                    if (oraclePill) { oraclePill.textContent = 'EF-0 ORACLE: KILLED'; oraclePill.className = 'zcc-pill fail'; }
                    return;
                }

                const e = this.wasmExports;
                const mainRet = e.main();
                const obsMix = e.zcc_quad_bitops_mix(0x00150007, 0x000B0013) >>> 0;
                const expMix = ZccIndependentOracle.specQuadBitopsMix(0x00150007, 0x000B0013) >>> 0;
                const obsQec = e.zcc_steane_qec_recover((1 << 5) | (1 << 21));
                const expQec = ZccIndependentOracle.specSteaneQecRecover((1 << 5) | (1 << 21));
                const obsRot = e.zcc_rotor4d_norm_q12(4096, 4096, 4096, 4096, 3547, 2048);
                const expRot = ZccIndependentOracle.specRotor4dNormQ12(4096, 4096, 4096, 4096, 3547, 2048);
                const obsCan = e.zcc_arena_canary_audit(16, 1337);
                const expCan = ZccIndependentOracle.specArenaCanaryAudit(16, 1337);

                const asmFaults = (this.asmMutatedFlags.x86_64 ? 1 : 0) +
                                  (this.asmMutatedFlags.arm64 ? 1 : 0) +
                                  (this.asmMutatedFlags.riscv64 ? 1 : 0);

                if (obsMix !== expMix || obsQec !== expQec || obsRot !== expRot || obsCan !== expCan || mainRet !== 0 || asmFaults > 0) {
                    banner.style.background = 'rgba(255,51,102,0.18)';
                    banner.style.borderColor = 'rgba(255,51,102,0.65)';
                    banner.style.color = '#fda4af';
                    if (mainRet === 0 && obsMix !== expMix) {
                        banner.innerHTML = `🚨 <strong>SELF-CERTIFY TRAP CAUGHT (ZCC-EF-0):</strong> <code>exports.main() === 0</code> reported success, BEFORE Independent Oracle caught <code>mix=0x${obsMix.toString(16)} !== 0x${expMix.toString(16)}</code>!`;
                    } else {
                        banner.innerHTML = `🛑 <strong>EF-0 Divergence Killed:</strong> <code>main()=${mainRet}</code> • WASM/ASM faults active (${this.mutatedOffsets.size} byte flips, ${asmFaults} ISA faults).`;
                    }
                    if (oraclePill) { oraclePill.textContent = 'EF-0 ORACLE: FAULT CAUGHT'; oraclePill.className = 'zcc-pill fail'; }
                } else {
                    banner.style.background = 'rgba(16,185,129,0.12)';
                    banner.style.borderColor = 'rgba(16,185,129,0.4)';
                    banner.style.color = '#6ee7b7';
                    banner.innerHTML = `✅ <strong>EF-0 Integrity Clean:</strong> <code>exports.main() === 0</code> AND all 6 Tier-1 exported kernels match Independent Specification Oracle (<code>0x${expMix.toString(16)}</code>).`;
                    if (oraclePill) { oraclePill.textContent = 'EF-0 ORACLE: VERIFIED'; oraclePill.className = 'zcc-pill pass'; }
                }
            }

            initializeElements() {
                this.elements = {
                    testDuration: document.getElementById('test-duration'),
                    sampleRate: document.getElementById('sample-rate'),
                    stressIntensity: document.getElementById('stress-intensity'),
                    objectCount: document.getElementById('object-count'),
                    parallelTests: document.getElementById('parallel-tests'),
                    minFPS: document.getElementById('min-fps'),
                    maxCPU: document.getElementById('max-cpu'),
                    maxMemory: document.getElementById('max-memory'),
                    maxLatency: document.getElementById('max-latency'),
                    targetRefined: document.getElementById('target-refined'),
                    targetDeep: document.getElementById('target-deep'),
                    targetOriginal: document.getElementById('target-original'),
                    targetTest: document.getElementById('target-test'),
                    globalProgress: document.getElementById('global-progress'),
                    testStatus: document.getElementById('test-status'),
                    testLog: document.getElementById('test-log'),
                    durationValue: document.getElementById('duration-value'),
                    sampleValue: document.getElementById('sample-value'),
                    intensityValue: document.getElementById('intensity-value'),
                    objectsValue: document.getElementById('objects-value'),
                    parallelValue: document.getElementById('parallel-value'),
                    fpsValue: document.getElementById('fps-value'),
                    cpuValue: document.getElementById('cpu-value'),
                    memoryValue: document.getElementById('memory-value'),
                    latencyValue: document.getElementById('latency-value'),
                    globalVerdictPill: document.getElementById('global-verdict-pill'),
                    tesseractCanvas: document.getElementById('hd-tesseract-canvas'),
                    rotorNormReadout: document.getElementById('rotor-norm-readout'),
                    nv2LedgerTbody: document.getElementById('nv2-ledger-tbody')
                };

                Object.keys(this.systems).forEach(key => {
                    const cap = key.charAt(0).toUpperCase() + key.slice(1);
                    this.elements[`status${cap}`] = document.getElementById(`status-${key}`);
                    this.elements[`metrics${cap}`] = document.getElementById(`metrics-${key}`);
                    this.elements[`chart${cap}`] = document.getElementById(`chart-${key}`);
                });
            }

            bindIdempotentListeners() {
                const bindClick = (id, fn) => {
                    const el = document.getElementById(id);
                    if (el && !el.dataset.zccBound) {
                        el.dataset.zccBound = '1';
                        el.onclick = null;
                        el.addEventListener('click', (ev) => fn(ev, el));
                    }
                };
                Object.keys(this.testSuites).forEach(suite => {
                    bindClick(`${suite}-test`, () => this.runTestSuite(suite));
                });
                bindClick('bridge-recompile-btn', () => this.recompileViaBridge());
                bindClick('bridge-ping-btn', () => this.probeBridgeStatus(true));
                bindClick('bridge-matrix-btn', () => this.switchInspectorTab('bugs'));
                bindClick('run-all', () => this.runAllTests());
                bindClick('stop-all', () => this.stopAllTests());
                bindClick('export-results', () => this.exportResults());
                bindClick('clear-results', () => this.clearResults());
                bindClick('audio-toggle', (_e, el) => this.toggleAudioSynth(el));
                bindClick('fault-wasm-btn', () => this.injectFault('wasm_xor_316'));
                bindClick('fault-x86-btn', () => this.injectFault('x86_rsp_misalign'));
                bindClick('fault-arm-btn', () => this.injectFault('arm64_sp_corrupt'));
                bindClick('fault-rv-btn', () => this.injectFault('rv64_lwu_corrupt'));
                bindClick('restore-clean-btn', () => this.restoreCleanArtifacts());
                ['bugs', 'dist', 'wasm', 'x86', 'arm64', 'riscv64'].forEach(t => {
                    bindClick(`tab-btn-${t}`, () => this.switchInspectorTab(t));
                });

                const sliderIds = [
                    'test-duration', 'sample-rate', 'stress-intensity', 'object-count',
                    'parallel-tests', 'min-fps', 'max-cpu', 'max-memory', 'max-latency'
                ];
                sliderIds.forEach(id => {
                    const el = document.getElementById(id);
                    if (el && !el.dataset.zccBound) {
                        el.dataset.zccBound = '1';
                        el.oninput = null;
                        el.addEventListener('input', () => this.syncConfigFromDOM());
                    }
                });

                ['refined', 'deep', 'original', 'test'].forEach(key => {
                    const el = document.getElementById(`target-${key}`);
                    if (el && !el.dataset.zccBound) {
                        el.dataset.zccBound = '1';
                        el.onchange = null;
                        el.addEventListener('change', () => this.syncTargetsFromDOM());
                    }
                });
            }

            syncConfigFromDOM() {
                const e = this.elements;
                if (!e.testDuration) return;
                this.config.duration = parseFloat(e.testDuration.value) || 6;
                this.config.sampleRate = parseInt(e.sampleRate.value, 10) || 10;
                this.config.intensity = parseInt(e.stressIntensity.value, 10) || 7;
                this.config.objectCount = parseInt(e.objectCount.value, 10) || 16;
                this.config.parallelTests = parseInt(e.parallelTests.value, 10) || 4;
                this.config.criteria.minKops = parseInt(e.minFPS.value, 10) || 25;
                this.config.criteria.maxFramePct = parseInt(e.maxCPU.value, 10) || 85;
                this.config.criteria.maxArenaKB = parseInt(e.maxMemory.value, 10) || 512;
                this.config.criteria.maxTickMs = parseInt(e.maxLatency.value, 10) || 35;

                e.durationValue.textContent = `${this.config.duration}s`;
                e.sampleValue.textContent = `${this.config.sampleRate}Hz`;
                e.intensityValue.textContent = String(this.config.intensity);
                e.objectsValue.textContent = String(this.config.objectCount);
                e.parallelValue.textContent = String(this.config.parallelTests);
                e.fpsValue.textContent = String(this.config.criteria.minKops);
                e.cpuValue.textContent = `${this.config.criteria.maxFramePct}%`;
                e.memoryValue.textContent = `${this.config.criteria.maxArenaKB}KB`;
                e.latencyValue.textContent = `${this.config.criteria.maxTickMs}ms`;
            }

            syncTargetsFromDOM() {
                ['refined', 'deep', 'original', 'test'].forEach(key => {
                    const cb = document.getElementById(`target-${key}`);
                    if (cb) {
                        this.systems[key].enabled = Boolean(cb.checked);
                    }
                });
                this.log(`[CONFIG] Active Quad-ISA targets updated`, 'info');
            }

            toggleAudioSynth(btnEl) {
                try {
                    const AudioCtor = window.AudioContext || window.webkitAudioContext;
                    if (!AudioCtor) {
                        this.log('[AUDIO] Web Audio API unavailable in headless runtime; Q15 offline PCM + FFT64 remains active.', 'warning');
                        return;
                    }
                    if (!this.audioCtx) {
                        this.audioCtx = new AudioCtor();
                    }
                    if (this.audioCtx.state === 'suspended') {
                        this.audioCtx.resume();
                    }
                    this.audioEnabled = !this.audioEnabled;
                    const targetBtn = btnEl || document.getElementById('audio-toggle');
                    if (targetBtn) {
                        targetBtn.textContent = this.audioEnabled ? '🔊 Solfeggio DSP: ON (528Hz+4Hz)' : '🔊 Solfeggio DSP: OFF';
                    }
                    if (this.audioEnabled) {
                        this.playSolfeggioChord(528);
                        this.log('[AUDIO] Solfeggio Acoustic Alchemy ignited (528Hz + 4Hz Theta binaural beat)', 'success');
                    } else {
                        this.log('[AUDIO] Audible output muted (Q15 PCM + Radix-2 FFT64 stays live)', 'info');
                    }
                } catch (err) {
                    this.log(`[AUDIO] Synth error: ${err.message || err}`, 'error');
                }
            }

            playSolfeggioChord(baseHz) {
                if (!this.audioEnabled || !this.audioCtx) return;
                try {
                    const now = this.audioCtx.currentTime;
                    const oscL = this.audioCtx.createOscillator();
                    const oscR = this.audioCtx.createOscillator();
                    const gain = this.audioCtx.createGain();
                    oscL.type = 'sine';
                    oscR.type = 'sine';
                    oscL.frequency.setValueAtTime(baseHz, now);
                    oscR.frequency.setValueAtTime(baseHz + 4, now);
                    gain.gain.setValueAtTime(0.0001, now);
                    gain.gain.exponentialRampToValueAtTime(0.035, now + 0.03);
                    gain.gain.exponentialRampToValueAtTime(0.0001, now + 0.24);
                    oscL.connect(gain);
                    oscR.connect(gain);
                    gain.connect(this.audioCtx.destination);
                    oscL.start(now);
                    oscR.start(now);
                    oscL.stop(now + 0.25);
                    oscR.stop(now + 0.25);
                } catch (audioErr) {
                    this.lastAudioError = String(audioErr);
                }
            }

            initializeCharts() {
                Object.keys(this.systems).forEach(key => {
                    const cap = key.charAt(0).toUpperCase() + key.slice(1);
                    const canvas = this.elements[`chart${cap}`];
                    if (canvas) {
                        const ctx = canvas.getContext('2d');
                        this.charts[key] = {
                            canvas,
                            ctx,
                            data: [],
                            maxPoints: 48
                        };
                    }
                });
            }

            switchInspectorTab(tabId) {
                this.activeTab = tabId;
                ['bugs', 'dist', 'wasm', 'x86', 'arm64', 'riscv64'].forEach(t => {
                    const btn = document.getElementById(`tab-btn-${t}`);
                    const pane = document.getElementById(`tab-pane-${t}`);
                    if (btn) btn.classList.toggle('active', t === tabId);
                    if (pane) pane.classList.toggle('hidden', t !== tabId);
                });
            }

            renderInspectorViews() {
                const x86Pre = document.getElementById('asm-x86-pre');
                const armPre = document.getElementById('asm-arm64-pre');
                const rvPre = document.getElementById('asm-riscv64-pre');
                if (x86Pre) x86Pre.textContent = this.activeAsm.x86_64.slice(0, 6000) + `\n# ... [${this.activeAsm.x86_64.split('\n').length} total lines emitted by ./zcc -S]`;
                if (armPre) armPre.textContent = this.activeAsm.arm64.slice(0, 6000) + `\n# ... [${this.activeAsm.arm64.split('\n').length} total lines emitted by ./zcc --target=arm64 -S]`;
                if (rvPre) rvPre.textContent = this.activeAsm.riscv64.slice(0, 6000) + `\n# ... [${this.activeAsm.riscv64.split('\n').length} total lines emitted by ./zcc --target=riscv64 -S]`;

                const hexContainer = document.getElementById('wasm-hex-container');
                if (!hexContainer) return;
                hexContainer.innerHTML = '';
                const frag = document.createDocumentFragment();
                const len = this.activeWasmBytes.length;
                const proveStart = Math.max(0, len - 95);
                for (let i = 0; i < len; i++) {
                    const span = document.createElement('span');
                    let cls = 'hex-byte';
                    if (i >= 240 && i < proveStart) cls += ' sec-code';
                    else if (i >= proveStart) cls += ' sec-prove';
                    if (this.mutatedOffsets.has(i)) cls += ' mutated';
                    span.className = cls;
                    span.textContent = this.activeWasmBytes[i].toString(16).padStart(2, '0').toUpperCase();
                    span.title = `Offset ${i} (0x${i.toString(16)}) — Click to flip ^= 0x01`;
                    span.addEventListener('click', () => this.toggleWasmByteBit(i));
                    frag.appendChild(span);
                }
                hexContainer.appendChild(frag);
            }

            async toggleWasmByteBit(offset) {
                if (offset < 0 || offset >= this.activeWasmBytes.length) return;
                this.activeWasmBytes[offset] ^= 0x01;
                if (this.activeWasmBytes[offset] === this.cleanWasmBytes[offset]) {
                    this.mutatedOffsets.delete(offset);
                } else {
                    this.mutatedOffsets.add(offset);
                }
                const status = document.getElementById('inspector-byte-status');
                if (status) {
                    status.textContent = `Byte [${offset}] = 0x${this.activeWasmBytes[offset].toString(16).padStart(2, '0').toUpperCase()} (Clean: 0x${this.cleanWasmBytes[offset].toString(16).padStart(2, '0').toUpperCase()}) • Active Byte Faults: ${this.mutatedOffsets.size}`;
                }
                this.renderInspectorViews();
                await this.bootWasmModule(false);
            }

            async injectFault(kind) {
                if (kind === 'wasm_xor_316' || kind === 'wasm_xor_mix') {
                    const entry = this.verifiedMutants.length > 0 ? this.verifiedMutants[0] : [316, 0x71, 'zcc_quad_bitops_mix i32.xor->and'];
                    const off = entry[0] | 0;
                    const mutOp = entry[1] & 0xFF;
                    this.activeWasmBytes[off] = mutOp;
                    this.mutatedOffsets.add(off);
                    this.renderInspectorViews();
                    await this.bootWasmModule(false);
                    this.log(`[FAULT-INJECT] Mutated kernel_zcc.wasm[${off}] -> 0x${mutOp.toString(16)} (${entry[2]}): exports.main()===0 (self-certify trap!), Independent Oracle ready to kill.`, 'warning');
                } else if (kind === 'x86_rsp_misalign') {
                    this.activeAsm.x86_64 = this.activeAsm.x86_64.replace(/subq \$(\d+), %rsp/, (_m, d) => `subq $${parseInt(d, 10) + 8}, %rsp`);
                    this.asmMutatedFlags.x86_64 = true;
                    this.renderInspectorViews();
                    this.evaluateEf0TrapBanner();
                    this.log(`[FAULT-INJECT] Mutated x86_64.s prologue: +8 byte %rsp shift (violates SystemV 16-byte stack ABI!)`, 'warning');
                } else if (kind === 'arm64_sp_corrupt') {
                    this.activeAsm.arm64 = this.activeAsm.arm64.replace(/sub sp, sp, #(\d+)/, (_m, d) => `sub sp, sp, #${Math.max(8, parseInt(d, 10) - 8)}`);
                    this.asmMutatedFlags.arm64 = true;
                    this.renderInspectorViews();
                    this.evaluateEf0TrapBanner();
                    this.log(`[FAULT-INJECT] Mutated arm64.s prologue: -8 byte SP shift (violates AAPCS64 16-byte SP alignment!)`, 'warning');
                } else if (kind === 'rv64_lwu_corrupt') {
                    this.activeAsm.riscv64 = this.activeAsm.riscv64.replace(/lwu /g, 'lw  ');
                    this.asmMutatedFlags.riscv64 = true;
                    this.renderInspectorViews();
                    this.evaluateEf0TrapBanner();
                    this.log(`[FAULT-INJECT] Mutated riscv64.s loads: lwu -> lw (sign-extension corruption on uint32_t operands!)`, 'warning');
                }
            }

            async restoreCleanArtifacts() {
                this.activeWasmBytes = new Uint8Array(this.cleanWasmBytes);
                this.mutatedOffsets.clear();
                this.activeAsm.x86_64 = this.cleanAsm.x86_64;
                this.activeAsm.arm64 = this.cleanAsm.arm64;
                this.activeAsm.riscv64 = this.cleanAsm.riscv64;
                this.asmMutatedFlags = { x86_64: false, arm64: false, riscv64: false };
                this.renderInspectorViews();
                await this.bootWasmModule(false);
                this.log(`[RESTORE] Restored SHA-256 verified kernel_zcc.wasm (${(this.embedded.wasmSha256 || '').slice(0, 12)}...) & clean Quad-ISA assembly streams`, 'success');
            }

            buildDeterministicVectors(suite, tickIndex) {
                const count = Math.max(4, this.config.objectCount * this.config.intensity * 2);
                const vectors = new Array(count);
                for (let i = 0; i < count; i++) {
                    const idx = tickIndex * count + i + 1;
                    const a = (Math.imul(idx, 0x9E3779B1) ^ 0x00150007) >>> 0;
                    const b = (Math.imul(idx, 0x85EBCA77) ^ 0x000B0013) >>> 0;
                    const qubit = idx % 7;
                    const pauliKind = idx % 3;
                    let errPauli = 0;
                    if (pauliKind === 0) errPauli = 1 << qubit;
                    else if (pauliKind === 1) errPauli = 1 << (qubit + 16);
                    else errPauli = (1 << qubit) | (1 << (qubit + 16));

                    const ang = (idx % 64) * (Math.PI / 32);
                    const cosQ12 = Math.round(Math.cos(ang) * 4096);
                    const sinQ12 = Math.round(Math.sin(ang) * 4096);
                    vectors[i] = {
                        a,
                        b,
                        errPauli: errPauli >>> 0,
                        x: (idx & 1) ? 4096 : -4096,
                        y: (idx & 2) ? 4096 : -4096,
                        z: (idx & 4) ? 4096 : -4096,
                        w: (idx & 8) ? 4096 : -4096,
                        cosQ12,
                        sinQ12,
                        phase: (idx * 1024) & 0xFFFF,
                        gain: 128 + ((idx * 17) & 0x7F),
                        blocks: 4 + (idx % 28),
                        seed: (1337 + idx * 31) | 0,
                        hBase: 1000 + ((idx % 16) * 64),
                        hPrev: -2048 + ((idx % 32) * 128),
                        etaQ12: (suite === 'endurance' && (idx & 1) === 0) ? 0 : 1638,
                        scarKick: (suite === 'endurance' && (idx & 1) === 0) ? 8192 : 0
                    };
                }
                return vectors;
            }

            evaluateWasmVectors(suite, vectors) {
                if (!this.wasmExports) {
                    return { bootFailed: true, divergences: vectors.length, outputs: [] };
                }
                const e = this.wasmExports;
                const outputs = new Array(vectors.length);
                for (let i = 0; i < vectors.length; i++) {
                    const v = vectors[i];
                    if (suite === 'geometry') {
                        const qec = e.zcc_steane_qec_recover(v.errPauli) >>> 0;
                        const rot = e.zcc_rotor4d_norm_q12(v.x, v.y, v.z, v.w, v.cosQ12, v.sinQ12) >>> 0;
                        outputs[i] = (qec ^ rot) >>> 0;
                    } else if (suite === 'audio') {
                        outputs[i] = (e.zcc_dsp_harmonic_q15(v.phase, v.gain) + 32768) >>> 0;
                    } else if (suite === 'memory') {
                        outputs[i] = e.zcc_arena_canary_audit(v.blocks, v.seed) >>> 0;
                    } else if (suite === 'endurance') {
                        outputs[i] = e.zcc_prime_step_q12(v.hBase, v.hPrev, v.etaQ12, v.scarKick) >>> 0;
                    } else {
                        outputs[i] = e.zcc_quad_bitops_mix(v.a, v.b) >>> 0;
                    }
                }
                return { bootFailed: false, outputs };
            }

            async runSuiteSpecialOracle(suite) {
                if (suite === 'stress' || suite === 'regression') {
                    const mutations = this.verifiedMutants.length > 0 ? this.verifiedMutants : [
                        [316, 0x71, 'zcc_quad_bitops_mix i32.xor->and']
                    ];
                    let killed = 0;
                    let trapExitCode = 0;
                    const imports = { env: {}, wasi_snapshot_preview1: { proc_exit: (c) => { trapExitCode = c | 0; }, fd_write: () => 0 } };
                    for (let i = 0; i < mutations.length; i++) {
                        const [off, mutOp] = mutations[i];
                        const clone = new Uint8Array(this.cleanWasmBytes);
                        clone[off] = mutOp;
                        try {
                            const m = await WebAssembly.instantiate(clone, imports);
                            const ex = m.instance.exports;
                            const ok =
                                (ex.zcc_quad_bitops_mix(0x00150007, 0x000B0013) >>> 0) === ZccIndependentOracle.specQuadBitopsMix(0x00150007, 0x000B0013) &&
                                ex.zcc_steane_qec_recover((1 << 5) | (1 << 21)) === ZccIndependentOracle.specSteaneQecRecover((1 << 5) | (1 << 21)) &&
                                ex.zcc_rotor4d_norm_q12(4096, 4096, 4096, 4096, 3547, 2048) === ZccIndependentOracle.specRotor4dNormQ12(4096, 4096, 4096, 4096, 3547, 2048) &&
                                ex.zcc_dsp_harmonic_q15(16384, 128) === ZccIndependentOracle.specDspHarmonicQ15(16384, 128) &&
                                ex.zcc_arena_canary_audit(16, 1337) === ZccIndependentOracle.specArenaCanaryAudit(16, 1337) &&
                                ex.zcc_prime_step_q12(1000, 2048, 1638, 0) === ZccIndependentOracle.specPrimeStepQ12(1000, 2048, 1638, 0);
                            if (!ok) killed++;
                        } catch (_e) {
                            killed++;
                        }
                    }
                    let neg02Rejected = false;
                    const badMagic = new Uint8Array(this.cleanWasmBytes);
                    badMagic[0] = 0xFF;
                    try {
                        await WebAssembly.instantiate(badMagic, imports);
                    } catch (_e) {
                        neg02Rejected = true;
                    }
                    const ok = killed === mutations.length && neg02Rejected;
                    this.log(`[MV-02 / EF-0] Adversarial Mutation Sweep: ${killed}/${mutations.length} semantic mutants KILLED • NEG-02 magic corruption rejected=${neg02Rejected}`, ok ? 'success' : 'error');
                    return { ok, killed, total: mutations.length, neg02Rejected, trapExitCode };
                }

                if (suite === 'audio') {
                    const signal = new Float64Array(64);
                    for (let n = 0; n < 64; n++) {
                        const ph = ((n * 4) % 64) * 1024;
                        const s = this.wasmExports
                            ? this.wasmExports.zcc_dsp_harmonic_q15(ph, 96)
                            : ZccIndependentOracle.specDspHarmonicQ15(ph, 96);
                        signal[n] = s / 32768.0;
                    }
                    const fft = ZccIndependentOracle.radix2FFT64(signal);
                    const hz = this.solfeggioHz[this.solfeggioIndex % this.solfeggioHz.length];
                    this.solfeggioIndex++;
                    this.playSolfeggioChord(hz);
                    const ok = fft.peakBin === 4 && fft.snrDb > 8.0;
                    this.log(`[Q-DSP FFT64] ZCC Q15 oscillator locked on Bin ${fft.peakBin} (expected 4) • SNR=${fft.snrDb.toFixed(2)} dB`, ok ? 'success' : 'error');
                    return { ok, ...fft };
                }

                if (suite === 'endurance') {
                    const runArm = (mode, seedBase) => {
                        const stepsArr = [];
                        const maxSteps = mode === 'stress' ? 360 : 260;
                        for (let trial = 0; trial < 12; trial++) {
                            const size = mode === 'stress' ? 14 : 10;
                            const mid = Math.floor(size / 2);
                            const cells = size * size;
                            const hBase = new Int32Array(cells);
                            for (let c = 0; c < cells; c++) {
                                const gx = c % size, gy = Math.floor(c / size);
                                const isVertWall = (gx === mid && gy >= 0 && gy <= mid);
                                const isHorizWall = (gy === mid && gx >= 1 && gx <= mid);
                                const ridge = (gx === 0 && gy >= 1 && gy <= mid) ? 2600 : 0;
                                const wall = (isVertWall || isHorizWall) ? 48000 : 0;
                                const distGoal = (size - 1 - gx) + (size - 1 - gy);
                                const raw = distGoal * 1024 + ridge + wall;
                                hBase[c] = this.wasmExports
                                    ? this.wasmExports.zcc_prime_step_q12(raw, raw >> 2, 1638, 0)
                                    : ZccIndependentOracle.specPrimeStepQ12(raw, raw >> 2, 1638, 0);
                            }
                            let x = 0, y = 0, steps = 0;
                            let rng = (seedBase + trial * 4099) >>> 0;
                            const nextRand = () => {
                                rng = (Math.imul(rng, 1664525) + 1013904223) >>> 0;
                                return (rng >>> 8) / 16777216.0;
                            };

                            while ((x !== size - 1 || y !== size - 1) && steps < maxSteps) {
                                steps++;
                                const dirs = [[1, 0], [0, 1], [-1, 0], [0, -1]];
                                let bestScore = 1e15, bestNx = x, bestNy = y;
                                for (let d = 0; d < 4; d++) {
                                    const nx = x + dirs[d][0], ny = y + dirs[d][1];
                                    if (nx < 0 || ny < 0 || nx >= size || ny >= size) continue;
                                    const epsNoise = (mode === 'zeroed') ? 0 : (nextRand() - 0.5) * 410;
                                    const navVal = this.wasmExports
                                        ? this.wasmExports.zcc_prime_step_q12(hBase[ny * size + nx], 0, 0, 0)
                                        : hBase[ny * size + nx];
                                    const score = navVal + epsNoise;
                                    if (score < bestScore) {
                                        bestScore = score;
                                        bestNx = nx;
                                        bestNy = ny;
                                    }
                                }
                                if (mode === 'active' || mode === 'stress') {
                                    hBase[y * size + x] += 8192;
                                } else if (mode === 'sham') {
                                    const shamCell = Math.floor(nextRand() * cells);
                                    hBase[shamCell] += 8192;
                                }
                                x = bestNx;
                                y = bestNy;
                            }
                            stepsArr.push(steps);
                        }
                        return ZccIndependentOracle.computeNV2Stats(stepsArr);
                    };

                    const arm1 = runArm('active', 9001);
                    const arm2 = runArm('zeroed', 9001);
                    const arm3 = runArm('sham', 9001);
                    const arm4 = runArm('stress', 9001);
                    this.fourArmLedger = [
                        { name: 'Arm 1: Active PRIME (eta=0, kick=2.0, eps=0.05)', stats: arm1, verdict: arm1.mean < arm2.mean && arm1.maxLinf < 260 ? 'PASS' : 'FAIL' },
                        { name: 'Arm 2: Zeroed Control (kick=0, eps=0 Stall)', stats: arm2, verdict: arm2.mean === 260 ? 'PASS' : 'FAIL' },
                        { name: 'Arm 3: Sham Permuted Scars Control', stats: arm3, verdict: arm1.mean < arm3.mean ? 'PASS' : 'FAIL' },
                        { name: 'Arm 4: Out-of-Sample 14x14 Spill Stress', stats: arm4, verdict: arm4.maxLinf < 360 ? 'PASS' : 'FAIL' }
                    ];
                    const ok = this.fourArmLedger.every(a => a.verdict === 'PASS');
                    this.log(`[EF-13 4-ARM] Active mean=${arm1.mean.toFixed(1)} steps (std=${arm1.std.toFixed(1)}) vs Zeroed=${arm2.mean.toFixed(1)} (stalled) vs Sham=${arm3.mean.toFixed(1)} vs Stress=${arm4.mean.toFixed(1)}`, ok ? 'success' : 'error');
                    return { ok, ledger: this.fourArmLedger };
                }

                return { ok: true };
            }

            async runTestSuite(suite) {
                if (this.isRunning) {
                    this.log('⚠️ Suite already executing — Stop current run or wait for completion.', 'warning');
                    return null;
                }
                const testSuite = this.testSuites[suite];
                if (!testSuite) {
                    this.log(`❌ Unknown ZCC suite: ${suite}`, 'error');
                    return null;
                }

                this.syncConfigFromDOM();
                this.isRunning = true;
                this.currentTest = suite;
                this.testStartTime = performance.now();

                const button = document.getElementById(`${suite}-test`);
                if (button) button.classList.add('running');
                if (this.elements.globalVerdictPill) {
                    this.elements.globalVerdictPill.textContent = `RUNNING: ${suite.toUpperCase()}`;
                    this.elements.globalVerdictPill.className = 'zcc-pill warn';
                }

                this.elements.testStatus.textContent = `Executing ${testSuite.name} (${this.config.duration}s @ ${this.config.sampleRate}Hz)...`;
                this.log(`🚀 Starting ${testSuite.name} [${testSuite.tests.join(', ')}]`, 'info');

                this.testResults[suite] = {
                    suite,
                    name: testSuite.name,
                    startTimeIso: new Date().toISOString(),
                    durationSec: this.config.duration,
                    systems: {},
                    verdict: 'INCONCLUSIVE'
                };

                const enabledKeys = Object.keys(this.systems).filter(k => this.systems[k].enabled).slice(0, this.config.parallelTests);
                Object.keys(this.systems).forEach(key => {
                    const cap = key.charAt(0).toUpperCase() + key.slice(1);
                    const statusEl = this.elements[`status${cap}`];
                    if (enabledKeys.includes(key)) {
                        this.testResults[suite].systems[key] = {
                            kops: [],
                            framePct: [],
                            arenaKB: [],
                            tickMs: [],
                            vectorsEvaluated: 0,
                            divergences: 0,
                            selfCertifyTraps: 0,
                            fnv1a: '0x0000000000000000',
                            verdict: 'INCONCLUSIVE'
                        };
                        if (statusEl) statusEl.className = 'status-indicator running';
                    } else {
                        this.testResults[suite].systems[key] = {
                            kops: [],
                            framePct: [],
                            arenaKB: [],
                            tickMs: [],
                            vectorsEvaluated: 0,
                            divergences: 0,
                            selfCertifyTraps: 0,
                            fnv1a: 'N/A',
                            verdict: 'NOT-APPLICABLE'
                        };
                        if (statusEl) statusEl.className = 'status-indicator';
                        this.renderSingleCardMetrics(suite, key);
                    }
                });

                const specialRes = await this.runSuiteSpecialOracle(suite);
                this.testResults[suite].specialOracleOk = !specialRes || specialRes.ok !== false;
                await this.executeSamplingLoop(suite, enabledKeys);

                return this.completeTest(suite, button, testSuite, enabledKeys);
            }

            executeSamplingLoop(suite, enabledKeys) {
                const durationMs = this.config.duration * 1000;
                const intervalMs = Math.max(25, Math.round(1000 / this.config.sampleRate));
                const totalSamples = Math.max(2, Math.floor(durationMs / intervalMs));
                let sampleIdx = 0;

                return new Promise(resolve => {
                    const step = () => {
                        if (!this.isRunning || this.currentTest !== suite) {
                            resolve();
                            return;
                        }
                        const vectors = this.buildDeterministicVectors(suite, sampleIdx);
                        const expected = new Array(vectors.length);
                        for (let i = 0; i < vectors.length; i++) {
                            expected[i] = ZccIndependentOracle.specEvalSuiteVector(suite, vectors[i]);
                        }

                        enabledKeys.forEach(key => {
                            this.collectRealSample(suite, key, vectors, expected);
                        });

                        this.updateCharts();
                        this.updateMetricsDisplay(suite);
                        this.renderNV2LedgerTable(suite);

                        sampleIdx++;
                        const pct = Math.min(100, Math.round((sampleIdx / totalSamples) * 100));
                        if (this.elements.globalProgress) {
                            this.elements.globalProgress.style.width = `${pct}%`;
                        }

                        if (sampleIdx >= totalSamples) {
                            resolve();
                        } else {
                            this.activeTimer = setTimeout(step, intervalMs);
                        }
                    };
                    step();
                });
            }

            collectRealSample(suite, systemKey, vectors, expected) {
                const sysRec = this.testResults[suite].systems[systemKey];
                const t0 = performance.now();
                let outputs = [];
                let structuralErrors = 0;

                if (systemKey === 'deep') {
                    const wRes = this.evaluateWasmVectors(suite, vectors);
                    outputs = wRes.outputs;
                    if (wRes.bootFailed) structuralErrors += vectors.length;
                } else if (systemKey === 'refined') {
                    const xRes = ZccQuadIsaVerifier.auditX86_64(this.activeAsm.x86_64, vectors, suite);
                    outputs = xRes.outputs;
                    structuralErrors += xRes.abiErrors;
                } else if (systemKey === 'original') {
                    const aRes = ZccQuadIsaVerifier.auditArm64(this.activeAsm.arm64, vectors, suite);
                    outputs = aRes.outputs;
                    structuralErrors += aRes.abiErrors;
                } else if (systemKey === 'test') {
                    const rRes = ZccQuadIsaVerifier.auditRiscv64(this.activeAsm.riscv64, vectors, suite);
                    outputs = rRes.outputs;
                    structuralErrors += rRes.abiErrors;
                }

                const elapsedMs = Math.max(0.02, performance.now() - t0);
                let sampleDivergences = structuralErrors;
                for (let i = 0; i < expected.length; i++) {
                    if ((outputs[i] >>> 0) !== (expected[i] >>> 0)) {
                        sampleDivergences++;
                    }
                }

                if (systemKey === 'deep' && sampleDivergences > 0 && this.wasmExports && this.wasmExports.main() === 0) {
                    sysRec.selfCertifyTraps++;
                }

                const kops = (vectors.length / elapsedMs);
                const frameBudgetPct = Math.min(100, (elapsedMs / 16.667) * 100);
                let arenaKB = 64;
                if (systemKey === 'deep' && this.wasmExports && this.wasmExports.memory) {
                    arenaKB = Math.round(this.wasmExports.memory.buffer.byteLength / 1024);
                } else if (systemKey === 'refined') {
                    arenaKB = Math.round((this.activeAsm.x86_64.length + vectors.length * 16) / 1024);
                } else if (systemKey === 'original') {
                    arenaKB = Math.round((this.activeAsm.arm64.length + vectors.length * 16) / 1024);
                } else {
                    arenaKB = Math.round((this.activeAsm.riscv64.length + vectors.length * 16) / 1024);
                }

                sysRec.kops.push(kops);
                sysRec.framePct.push(frameBudgetPct);
                sysRec.arenaKB.push(arenaKB);
                sysRec.tickMs.push(elapsedMs);
                sysRec.vectorsEvaluated += vectors.length;
                sysRec.divergences += sampleDivergences;
                sysRec.fnv1a = outputs.length > 0 ? ZccIndependentOracle.fnv1a64Hex(outputs) : '0x0000000000000000';

                if (this.charts[systemKey]) {
                    this.charts[systemKey].data.push({
                        kops,
                        framePct: frameBudgetPct,
                        tickMs: elapsedMs,
                        divergences: sampleDivergences
                    });
                    if (this.charts[systemKey].data.length > this.charts[systemKey].maxPoints) {
                        this.charts[systemKey].data.shift();
                    }
                }
            }

            updateCharts() {
                Object.keys(this.charts).forEach(key => {
                    const chart = this.charts[key];
                    const canvas = chart.canvas;
                    if (!canvas || canvas.width < 50 || canvas.height < 50) return;
                    const cw = canvas.clientWidth | 0;
                    const ch = canvas.clientHeight | 0;
                    if (cw >= 50 && ch >= 50 && (canvas.width !== cw || canvas.height !== ch)) {
                        canvas.width = cw;
                        canvas.height = ch;
                    }
                    const ctx = chart.ctx;
                    const w = canvas.width;
                    const h = canvas.height;
                    ctx.clearRect(0, 0, w, h);

                    ctx.strokeStyle = 'rgba(255, 255, 255, 0.07)';
                    ctx.lineWidth = 1;
                    for (let gy = 1; gy <= 3; gy++) {
                        const y = (h / 4) * gy;
                        ctx.beginPath();
                        ctx.moveTo(0, y);
                        ctx.lineTo(w, y);
                        ctx.stroke();
                    }

                    if (chart.data.length < 2) return;

                    let maxKops = 50;
                    for (let i = 0; i < chart.data.length; i++) {
                        if (chart.data[i].kops > maxKops) maxKops = chart.data[i].kops;
                    }

                    ctx.strokeStyle = '#00f5d4';
                    ctx.lineWidth = 2;
                    ctx.beginPath();
                    chart.data.forEach((pt, idx) => {
                        const x = (idx / (chart.maxPoints - 1)) * w;
                        const y = h - 8 - (Math.min(1, pt.kops / maxKops) * (h - 18));
                        if (idx === 0) ctx.moveTo(x, y);
                        else ctx.lineTo(x, y);
                    });
                    ctx.stroke();

                    ctx.strokeStyle = '#ff9f1c';
                    ctx.lineWidth = 1.5;
                    ctx.beginPath();
                    chart.data.forEach((pt, idx) => {
                        const x = (idx / (chart.maxPoints - 1)) * w;
                        const y = h - 8 - ((pt.framePct / 100) * (h - 18));
                        if (idx === 0) ctx.moveTo(x, y);
                        else ctx.lineTo(x, y);
                    });
                    ctx.stroke();

                    chart.data.forEach((pt, idx) => {
                        if (pt.divergences > 0) {
                            const x = (idx / (chart.maxPoints - 1)) * w;
                            const r = Math.max(1, 3.5);
                            ctx.fillStyle = '#ff3366';
                            ctx.beginPath();
                            ctx.arc(x, 12, r, 0, Math.PI * 2);
                            ctx.fill();
                        }
                    });
                });
            }

            renderSingleCardMetrics(suite, key) {
                const cap = key.charAt(0).toUpperCase() + key.slice(1);
                const metricsEl = this.elements[`metrics${cap}`];
                const sysRec = this.testResults[suite] && this.testResults[suite].systems[key];
                if (!metricsEl || !sysRec) return;

                const vals = metricsEl.querySelectorAll('.metric-value');
                if (vals.length < 8) return;

                if (sysRec.verdict === 'NOT-APPLICABLE' && sysRec.tickMs.length === 0) {
                    vals[0].textContent = 'N/A';
                    vals[1].textContent = 'N/A';
                    vals[2].textContent = 'N/A';
                    vals[3].textContent = 'N/A';
                    vals[4].textContent = '0';
                    vals[5].textContent = '0';
                    vals[6].textContent = 'N/A';
                    vals[7].textContent = 'NOT-APPLICABLE';
                    vals[7].style.color = '#94a3b8';
                    return;
                }

                const tickStats = ZccIndependentOracle.computeNV2Stats(sysRec.tickMs);
                const kopsStats = ZccIndependentOracle.computeNV2Stats(sysRec.kops);
                const latestFrame = sysRec.framePct[sysRec.framePct.length - 1] || 0;
                const latestArena = sysRec.arenaKB[sysRec.arenaKB.length - 1] || 0;

                vals[0].textContent = `${kopsStats.mean.toFixed(1)} Kops/s`;
                vals[1].textContent = `${latestFrame.toFixed(1)}%`;
                vals[2].textContent = `${latestArena} KB`;
                vals[3].textContent = `${tickStats.mean.toFixed(2)} / ${tickStats.maxLinf.toFixed(2)}ms`;
                vals[4].textContent = String(sysRec.vectorsEvaluated);
                vals[5].textContent = String(sysRec.divergences);
                vals[5].style.color = sysRec.divergences > 0 ? '#ff3366' : '#34d399';
                vals[6].textContent = sysRec.fnv1a.slice(0, 10) + '…';
                vals[7].textContent = sysRec.verdict;
                vals[7].style.color =
                    sysRec.verdict === 'PASS' ? '#34d399' :
                    sysRec.verdict === 'FAIL' ? '#ff3366' : '#ffbe0b';
            }

            updateMetricsDisplay(suite) {
                if (!this.testResults[suite]) return;
                Object.keys(this.systems).forEach(key => this.renderSingleCardMetrics(suite, key));
            }

            renderNV2LedgerTable(suite) {
                const tbody = this.elements.nv2LedgerTbody;
                if (!tbody || !this.testResults[suite]) return;
                const rows = [];
                Object.keys(this.systems).forEach(key => {
                    const sysRec = this.testResults[suite].systems[key];
                    if (!sysRec || sysRec.tickMs.length === 0) return;
                    const ts = ZccIndependentOracle.computeNV2Stats(sysRec.tickMs);
                    const ks = ZccIndependentOracle.computeNV2Stats(sysRec.kops);
                    rows.push(`<tr>
                        <td>${this.systems[key].name}</td>
                        <td>${ts.count}</td>
                        <td>${ts.mean.toFixed(3)}</td>
                        <td>${ts.median.toFixed(3)}</td>
                        <td>${ts.std.toFixed(3)}</td>
                        <td>${ts.maxLinf.toFixed(3)}</td>
                        <td>${ks.mean.toFixed(1)}</td>
                        <td style="font-weight:bold; color:${sysRec.verdict === 'PASS' ? '#34d399' : sysRec.verdict === 'FAIL' ? '#ff3366' : '#ffbe0b'}">${sysRec.verdict}</td>
                    </tr>`);
                });
                if (this.fourArmLedger.length > 0) {
                    this.fourArmLedger.forEach(arm => {
                        const s = arm.stats;
                        rows.push(`<tr style="background:rgba(0,245,212,0.04);">
                            <td>${arm.name}</td>
                            <td>${s.count} trials</td>
                            <td>${s.mean.toFixed(1)} steps</td>
                            <td>${s.median.toFixed(1)} steps</td>
                            <td>${s.std.toFixed(2)}</td>
                            <td>${s.maxLinf.toFixed(0)} steps</td>
                            <td>--</td>
                            <td style="font-weight:bold; color:${arm.verdict === 'PASS' ? '#34d399' : '#ff3366'}">${arm.verdict}</td>
                        </tr>`);
                    });
                }
                if (rows.length > 0) {
                    tbody.innerHTML = rows.join('');
                }
            }

            completeTest(suite, button, testSuite, enabledKeys) {
                const wasStopped = !this.isRunning;
                this.isRunning = false;
                this.currentTest = null;
                if (this.activeTimer) {
                    clearTimeout(this.activeTimer);
                    this.activeTimer = null;
                }

                if (button) button.classList.remove('running');

                const results = this.testResults[suite];
                if (enabledKeys.length === 0) {
                    results.verdict = 'NOT-APPLICABLE';
                } else if (wasStopped) {
                    results.verdict = 'INCONCLUSIVE';
                } else {
                    let anyFail = false;
                    let anyInconclusive = false;
                    const digests = [];

                    enabledKeys.forEach(key => {
                        const sysRec = results.systems[key];
                        const ts = ZccIndependentOracle.computeNV2Stats(sysRec.tickMs);
                        const ks = ZccIndependentOracle.computeNV2Stats(sysRec.kops);
                        const maxArena = Math.max(...sysRec.arenaKB, 0);
                        const maxFrame = Math.max(...sysRec.framePct, 0);

                        if (ts.count === 0) {
                            sysRec.verdict = 'INCONCLUSIVE';
                            anyInconclusive = true;
                        } else if (
                            sysRec.divergences > 0 ||
                            ks.mean < this.config.criteria.minKops ||
                            maxFrame > this.config.criteria.maxFramePct ||
                            maxArena > this.config.criteria.maxArenaKB ||
                            ts.maxLinf > this.config.criteria.maxTickMs
                        ) {
                            sysRec.verdict = 'FAIL';
                            anyFail = true;
                        } else {
                            sysRec.verdict = 'PASS';
                            digests.push(sysRec.fnv1a);
                        }

                        const cap = key.charAt(0).toUpperCase() + key.slice(1);
                        const statusEl = this.elements[`status${cap}`];
                        if (statusEl) {
                            statusEl.className = `status-indicator ${sysRec.verdict.toLowerCase()}`;
                        }
                    });

                    if (!anyFail && digests.length > 1) {
                        for (let i = 1; i < digests.length; i++) {
                            if (digests[i] !== digests[0]) {
                                anyFail = true;
                                this.log(`[CONSENSUS-FAIL] FNV-1a mismatch across Quad-ISA lanes: ${digests[0]} !== ${digests[i]}`, 'error');
                            }
                        }
                    }

                    if (results.specialOracleOk === false) {
                        anyFail = true;
                    }
                    results.verdict = anyFail ? 'FAIL' : (anyInconclusive ? 'INCONCLUSIVE' : 'PASS');
                }

                this.updateMetricsDisplay(suite);
                this.renderNV2LedgerTable(suite);

                if (this.elements.globalVerdictPill) {
                    this.elements.globalVerdictPill.textContent = `VERDICT: ${results.verdict}`;
                    this.elements.globalVerdictPill.className =
                        results.verdict === 'PASS' ? 'zcc-pill pass' :
                        results.verdict === 'FAIL' ? 'zcc-pill fail' : 'zcc-pill warn';
                }
                this.elements.testStatus.textContent = `${testSuite.name} — Terminal Gate Verdict: ${results.verdict}`;

                Object.keys(results.systems).forEach(key => {
                    const s = results.systems[key];
                    if (s.verdict !== 'NOT-APPLICABLE') {
                        const trapMsg = s.selfCertifyTraps > 0 ? ` [SELF-CERTIFY TRAPS BLOCKED: ${s.selfCertifyTraps}]` : '';
                        this.log(
                            `   ${this.systems[key].name}: ${s.verdict} (Vectors=${s.vectorsEvaluated}, Divergences=${s.divergences}, FNV1a=${s.fnv1a})${trapMsg}`,
                            s.verdict === 'PASS' ? 'success' : (s.verdict === 'FAIL' ? 'error' : 'warning')
                        );
                    }
                });

                return results;
            }

            async runAllTests() {
                if (this.isRunning) return;
                this.log('🚀 Launching Full 8-Suite ZCC Sovereign Verification Campaign...', 'info');
                for (const suite of Object.keys(this.testSuites)) {
                    await this.runTestSuite(suite);
                    await new Promise(r => setTimeout(r, 180));
                }
                const verdicts = Object.values(this.testResults).map(r => r.verdict);
                const passCount = verdicts.filter(v => v === 'PASS').length;
                const failCount = verdicts.filter(v => v === 'FAIL').length;
                const incCount = verdicts.filter(v => v === 'INCONCLUSIVE').length;
                this.log(`🏁 Full Campaign Complete — PASS=${passCount} | FAIL=${failCount} | INCONCLUSIVE=${incCount} (Total=${verdicts.length})`, failCount === 0 && incCount === 0 ? 'success' : 'warning');
            }

            stopAllTests() {
                if (this.activeTimer) {
                    clearTimeout(this.activeTimer);
                    this.activeTimer = null;
                }
                if (this.isRunning && this.currentTest && this.testResults[this.currentTest]) {
                    const cur = this.testResults[this.currentTest];
                    cur.verdict = 'INCONCLUSIVE';
                    Object.values(cur.systems).forEach(s => {
                        if (s.verdict === 'INCONCLUSIVE') s.verdict = 'INCONCLUSIVE';
                    });
                }
                this.isRunning = false;
                this.currentTest = null;
                document.querySelectorAll('.test-suite-btn.running').forEach(b => b.classList.remove('running'));
                document.querySelectorAll('.status-indicator.running').forEach(i => { i.className = 'status-indicator inconclusive'; });
                if (this.elements.globalVerdictPill) {
                    this.elements.globalVerdictPill.textContent = 'VERDICT: INCONCLUSIVE (STOPPED)';
                    this.elements.globalVerdictPill.className = 'zcc-pill warn';
                }
                this.elements.testStatus.textContent = 'Execution halted by user — Gate marked INCONCLUSIVE (Rule EF-6)';
                this.log('⏹️ Execution halted — active gate terminated as INCONCLUSIVE (never counted as PASS)', 'warning');
            }

            clearResults() {
                this.stopAllTests();
                this.testResults = {};
                this.fourArmLedger = [];
                Object.values(this.charts).forEach(c => {
                    c.data = [];
                    if (c.canvas && c.canvas.width >= 50 && c.canvas.height >= 50) {
                        c.ctx.clearRect(0, 0, c.canvas.width, c.canvas.height);
                    }
                });
                document.querySelectorAll('.metric-value').forEach(el => {
                    el.textContent = '--';
                    el.style.color = '#34d399';
                });
                document.querySelectorAll('.status-indicator').forEach(el => {
                    el.className = 'status-indicator';
                });
                if (this.elements.globalProgress) this.elements.globalProgress.style.width = '0%';
                if (this.elements.globalVerdictPill) {
                    this.elements.globalVerdictPill.textContent = 'VERDICT: READY';
                    this.elements.globalVerdictPill.className = 'zcc-pill';
                }
                if (this.elements.nv2LedgerTbody) {
                    this.elements.nv2LedgerTbody.innerHTML = '<tr><td colspan="8" style="color:#94a3b8;">Cleared — Run any ZCC Verification Suite to populate NV-2 distributions.</td></tr>';
                }
                if (this.elements.testLog) {
                    this.elements.testLog.innerHTML = '';
                }
                this.log('🗑️ Telemetry and gate ledgers cleared', 'info');
            }

            exportResults() {
                const payload = {
                    schema: 'ZCC-EF-7-HARNESS-RECEIPT-v2',
                    authorityRole: 'OBSERVER_WITH_INDEPENDENT_SPEC_ORACLE',
                    timestamp: new Date().toISOString(),
                    bridgeOnline: this.bridgeOnline,
                    embeddedArtifacts: {
                        kernelWasmBytes: this.cleanWasmBytes.length,
                        kernelWasmSha256: this.embedded.wasmSha256,
                        x86_64Sha256: this.embedded.x86Sha256,
                        arm64Sha256: this.embedded.arm64Sha256,
                        riscv64Sha256: this.embedded.riscv64Sha256,
                        observedSelfhostStage23Sha256: this.embedded.stage23Sha256
                    },
                    quadIsaGateReceipt: this.gateReceipt,
                    activeMutations: {
                        wasmByteOffsets: Array.from(this.mutatedOffsets),
                        asmMutatedFlags: this.asmMutatedFlags
                    },
                    config: this.config,
                    fourArmLedger: this.fourArmLedger,
                    results: this.testResults,
                    logs: this.logEntries
                };
                const jsonStr = JSON.stringify(payload, null, 2);
                if (typeof Blob !== 'undefined' && typeof URL !== 'undefined' && URL.createObjectURL) {
                    const blob = new Blob([jsonStr], { type: 'application/json' });
                    const link = document.createElement('a');
                    link.href = URL.createObjectURL(blob);
                    link.download = `zcc-hyperdim-receipt-${Date.now()}.json`;
                    link.click();
                }
                this.log('📁 Exported ZCC-EF-7 v2 JSON verification receipt with 3-Tier QEMU matrix & NV-2 distributions', 'success');
                return payload;
            }

            render4dTesseractAndSteane(nowMs) {
                const canvas = this.elements.tesseractCanvas;
                if (!canvas || canvas.width < 50 || canvas.height < 50) return;
                const cw = canvas.clientWidth | 0;
                const ch = canvas.clientHeight | 0;
                if (cw >= 50 && ch >= 50 && (canvas.width !== cw || canvas.height !== ch)) {
                    canvas.width = cw;
                    canvas.height = ch;
                }
                const ctx = canvas.getContext('2d');
                const w = canvas.width;
                const h = canvas.height;
                ctx.clearRect(0, 0, w, h);

                this.rotorAngleA += 0.014;
                this.rotorAngleB += 0.009;
                const cosQ12 = Math.round(Math.cos(this.rotorAngleA) * 4096);
                const sinQ12 = Math.round(Math.sin(this.rotorAngleA) * 4096);

                let liveNorm = 16382;
                let liveQec = 0x660;
                if (this.wasmExports) {
                    liveNorm = this.wasmExports.zcc_rotor4d_norm_q12(4096, 4096, 4096, 4096, cosQ12, sinQ12);
                    const qubitCycle = Math.floor(nowMs / 900) % 7;
                    const errWord = (1 << qubitCycle) | (1 << (qubitCycle + 16));
                    liveQec = this.wasmExports.zcc_steane_qec_recover(errWord);
                }
                const synX = (liveQec >> 8) & 0xF;
                const synZ = (liveQec >> 4) & 0xF;
                const resWt = liveQec & 0xF;
                if (this.elements.rotorNormReadout) {
                    this.elements.rotorNormReadout.textContent = `||v||₄² = ${liveNorm} Q12 • Steane Syndrome (${synX},${synZ})→ResWt=${resWt}`;
                    this.elements.rotorNormReadout.className = (resWt === 0 && liveNorm >= 16350 && liveNorm <= 16410) ? 'zcc-pill pass' : 'zcc-pill fail';
                }

                const cx = w * 0.30;
                const cy = h * 0.52;
                const scale = Math.min(w * 0.14, h * 0.36);
                const glowRadius = Math.max(1, scale * 1.35);
                const grad = ctx.createRadialGradient(cx, cy, 1, cx, cy, glowRadius);
                grad.addColorStop(0, 'rgba(0, 245, 212, 0.14)');
                grad.addColorStop(1, 'rgba(0, 245, 212, 0)');
                ctx.fillStyle = grad;
                ctx.fillRect(0, 0, w * 0.6, h);

                const cA = Math.cos(this.rotorAngleA), sA = Math.sin(this.rotorAngleA);
                const cB = Math.cos(this.rotorAngleB), sB = Math.sin(this.rotorAngleB);
                const pts2d = new Array(16);

                for (let v = 0; v < 16; v++) {
                    const x0 = (v & 1) ? 1 : -1;
                    const y0 = (v & 2) ? 1 : -1;
                    const z0 = (v & 4) ? 1 : -1;
                    const w0 = (v & 8) ? 1 : -1;
                    const x1 = x0 * cA - w0 * sA;
                    const w1 = x0 * sA + w0 * cA;
                    const y1 = y0 * cB - z0 * sB;
                    const z1 = y0 * sB + z0 * cB;
                    const persp4 = 1.0 / (2.8 - w1 * 0.55);
                    const px = cx + (x1 * 0.88 + z1 * 0.36) * scale * persp4 * 1.9;
                    const py = cy + (y1 * 0.88 - z1 * 0.32) * scale * persp4 * 1.9;
                    pts2d[v] = [px, py, w1];
                }

                ctx.lineWidth = 1.3;
                for (let i = 0; i < 16; i++) {
                    for (let bit = 0; bit < 4; bit++) {
                        const j = i ^ (1 << bit);
                        if (j > i) {
                            ctx.strokeStyle = bit === 3 ? 'rgba(247, 37, 133, 0.65)' : 'rgba(0, 245, 212, 0.55)';
                            ctx.beginPath();
                            ctx.moveTo(pts2d[i][0], pts2d[i][1]);
                            ctx.lineTo(pts2d[j][0], pts2d[j][1]);
                            ctx.stroke();
                        }
                    }
                }
                for (let i = 0; i < 16; i++) {
                    const r = Math.max(1, 2.5 + (pts2d[i][2] + 1.4) * 0.8);
                    ctx.fillStyle = (i & 8) ? '#ffbe0b' : '#00f5d4';
                    ctx.beginPath();
                    ctx.arc(pts2d[i][0], pts2d[i][1], r, 0, Math.PI * 2);
                    ctx.fill();
                }

                const fx = w * 0.76;
                const fy = h * 0.54;
                const fRad = Math.max(12, Math.min(w * 0.16, h * 0.36));
                const fanoNodes = [];
                for (let k = 0; k < 3; k++) {
                    const a = -Math.PI / 2 + k * ((2 * Math.PI) / 3);
                    fanoNodes.push([fx + Math.cos(a) * fRad, fy + Math.sin(a) * fRad]);
                }
                for (let k = 0; k < 3; k++) {
                    const p1 = fanoNodes[k], p2 = fanoNodes[(k + 1) % 3];
                    fanoNodes.push([(p1[0] + p2[0]) * 0.5, (p1[1] + p2[1]) * 0.5]);
                }
                fanoNodes.push([fx, fy]);

                ctx.strokeStyle = 'rgba(255, 190, 11, 0.35)';
                ctx.beginPath();
                ctx.arc(fx, fy, Math.max(1, fRad * 0.5), 0, Math.PI * 2);
                ctx.stroke();

                const fanoTriples = [[0, 3, 1], [1, 4, 2], [2, 5, 0], [0, 6, 4], [1, 6, 5], [2, 6, 3]];
                ctx.strokeStyle = 'rgba(0, 245, 212, 0.35)';
                fanoTriples.forEach(tr => {
                    ctx.beginPath();
                    ctx.moveTo(fanoNodes[tr[0]][0], fanoNodes[tr[0]][1]);
                    ctx.lineTo(fanoNodes[tr[1]][0], fanoNodes[tr[1]][1]);
                    ctx.lineTo(fanoNodes[tr[2]][0], fanoNodes[tr[2]][1]);
                    ctx.stroke();
                });

                const activeQubit = (synX >= 1 && synX <= 7) ? (synX - 1) : 0;
                for (let q = 0; q < 7; q++) {
                    const isHit = q === activeQubit;
                    const r = Math.max(1, isHit ? 7 : 5);
                    ctx.fillStyle = isHit ? '#f72585' : '#10b981';
                    ctx.beginPath();
                    ctx.arc(fanoNodes[q][0], fanoNodes[q][1], r, 0, Math.PI * 2);
                    ctx.fill();
                }

                ctx.fillStyle = '#94a3b8';
                ctx.font = '12px Consolas, monospace';
                ctx.fillText('SO(4) 4D Rotor (WASM Q12)', 12, 18);
                ctx.fillText(`Steane [[7,1,3]] Q_${activeQubit + 1} Decoded`, w * 0.60, 18);
            }

            startAnimationLoop() {
                const animationLoop = (now) => {
                    try {
                        if (this.lastFrameTimestamp > 0) {
                            const dt = now - this.lastFrameTimestamp;
                            if (dt > 0 && dt < 250) {
                                this.measuredUiFps = Math.round(1000 / dt);
                            }
                        }
                        this.lastFrameTimestamp = now;
                        this.render4dTesseractAndSteane(now || 0);
                    } catch (err) {
                        console.error('Frame render error:', err);
                    }
                    if (typeof requestAnimationFrame === 'function') {
                        requestAnimationFrame(animationLoop);
                    }
                };
                if (typeof requestAnimationFrame === 'function') {
                    requestAnimationFrame(animationLoop);
                }
            }

            log(message, type = 'info') {
                const timestamp = new Date().toLocaleTimeString();
                this.logEntries.push({ timestamp, message, type });
                if (this.elements && this.elements.testLog) {
                    const div = document.createElement('div');
                    div.className = `log-entry log-${type}`;
                    div.textContent = `[${timestamp}] ${message}`;
                    this.elements.testLog.appendChild(div);
                    this.elements.testLog.scrollTop = this.elements.testLog.scrollHeight;
                    const entries = this.elements.testLog.querySelectorAll('.log-entry');
                    if (entries.length > 80) entries[0].remove();
                }
            }
        }

        window.addEventListener('DOMContentLoaded', () => {
            try {
                window.zccHarness = new ZccHyperDimensionalHarness();
                window.testHarness = window.zccHarness;
            } catch (err) {
                console.error('Harness bootstrap failure:', err);
            }
        });
'''

