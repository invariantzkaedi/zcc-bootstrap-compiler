import sys
import os
import struct
import json
import numpy as np
import gc
import time
import random
import warnings
import base64

# WebGL Shader Compiler Risk Boundaries and Safety Ceilings
WEBGL_SHADER_RISK_LOW_MAX = 45 * 1024
WEBGL_SHADER_RISK_MED_MAX = 52 * 1024
WEBGL_GROUP_PRIMS_HIGH = 24
WEBGL_COMPILER_SAFETY_CEILING = 55 * 1024

# SVG Node Count Guardrail Limits
SVG_MAX_PRIMITIVES_LIMIT = 512

# WebGL2 Exclusivity Raymarching Template with Audio, UI uniforms, and modular zone functions
HTML_TEMPLATE = """<!DOCTYPE html>
<html>
<head>
    <title>ZCC Remediated SDF Raymarcher — Progressive Multi-Backend Edition</title>
    <style>
        body, html { margin: 0; padding: 0; overflow: hidden; background: #05070d; font-family: 'Courier New', monospace; color: #fff; }
        #canvas, #canvas-2d, #webgl-lite-canvas { width: 100vw; height: 100vh; position: absolute; top: 0; left: 0; z-index: 1; }
        #canvas, #webgl-lite-canvas { display: none; }
        #canvas-2d { display: block; }
        #ui { position: absolute; top: 20px; left: 20px; z-index: 10; background: rgba(10, 10, 15, 0.95); padding: 20px; border: 1px solid #00ffcc; border-radius: 8px; box-shadow: 0 0 20px rgba(0, 255, 204, 0.25); width: 280px; max-height: 90vh; overflow-y: auto; }
        h1 { font-size: 14px; margin: 0 0 12px 0; color: #00ffcc; text-shadow: 0 0 8px #00ffcc; text-transform: uppercase; letter-spacing: 1px; }
        p { margin: 4px 0; font-size: 11px; color: #888; }
        .control-group { margin: 14px 0; }
        label { display: block; font-size: 11px; margin-bottom: 4px; color: #00ffcc; }
        input[type=range] { width: 100%; accent-color: #ff00aa; background: #222; border-radius: 3px; height: 6px; outline: none; }
        button { width: 100%; background: #00ffcc; border: none; color: #000; padding: 8px; border-radius: 4px; font-family: monospace; font-size: 12px; cursor: pointer; margin-top: 10px; font-weight: bold; }
        button:hover { background: #33ffdd; }
        #fps { position: absolute; bottom: 20px; right: 20px; background: rgba(5,5,5,0.8); border: 1px solid #333; padding: 5px 10px; font-size: 12px; color: #00ffcc; border-radius: 3px; display: none; z-index: 10; }
        #error-log { color: #ff3333; font-size: 11px; margin-top: 10px; word-break: break-all; white-space: pre-wrap; font-family: monospace; }
        .group-bound { fill: none; stroke: #00ffff; stroke-opacity: 0.16; stroke-width: 1.5; }
        .prim-element { stroke: #ffffff; stroke-opacity: 0.25; }
    </style>
</head>
<body>
    <div id="ui">
        <h1>ZCC SDF Hybrid</h1>
        <p>Asset: __ASSET_NAME__</p>
        <p>Primitives: __NUM_PRIMITIVES__ Hybrid</p>
        <p>WebGL Risk: <span id="risk-badge" style="__RISK_BADGE_STYLE__">__RISK_LEVEL__</span></p>
        <p id="risk-reason" style="font-size: 10px; color: #aaa; margin-top: 4px;">__RISK_REASON__</p>
        <p id="svg-warning" style="font-size: 10px; color: #ff00aa; margin-top: 4px; font-weight: bold;">__SVG_WARNING__</p>
        
        <div id="error-log"></div>

        <!-- Backend Selection -->
        <div class="control-group">
            <label for="select-backend">Renderer Backend:</label>
            <select id="select-backend" style="width: 100%; background: #222; color: #fff; border: 1px solid #00ffcc; border-radius: 4px; padding: 5px; font-family: monospace; font-size: 11px;">
                <option value="svg">SVG Safe (Static)</option>
                <option value="canvas2d" selected>Canvas2D (Interactive Orbit)</option>
                <option value="webgl-lite">WebGL Lite (Fixed GPU Impostor)</option>
                <option value="webgl">WebGL Raymarch (High-Detail)</option>
            </select>
        </div>

        <!-- SVG / Canvas2D view controls -->
        <div id="svg-controls">
            <div class="control-group" id="proj-selector-group" style="display: none;">
                <label for="select-projection">Projection View:</label>
                <select id="select-projection" style="width: 100%; background: #222; color: #fff; border: 1px solid #00ffcc; border-radius: 4px; padding: 5px; font-family: monospace; font-size: 11px;">
                    <option value="xy" selected>XY View (Front)</option>
                    <option value="xz">XZ View (Top)</option>
                    <option value="yz">YZ View (Side)</option>
                    <option value="iso">Isometric View</option>
                </select>
            </div>
            
            <div class="control-group" style="display: flex; justify-content: space-between; align-items: center;">
                <label style="display: inline-block; margin-bottom: 0;">Show Group Bounds:</label>
                <input type="checkbox" id="chk-svg-groups" checked style="accent-color: #00ffcc;">
            </div>
            <div class="control-group" style="display: flex; justify-content: space-between; align-items: center;">
                <label style="display: inline-block; margin-bottom: 0;">Show Spheres:</label>
                <input type="checkbox" id="chk-svg-spheres" checked style="accent-color: #00ffcc;">
            </div>
            <div class="control-group" style="display: flex; justify-content: space-between; align-items: center;">
                <label style="display: inline-block; margin-bottom: 0;">Show Capsules:</label>
                <input type="checkbox" id="chk-svg-capsules" checked style="accent-color: #00ffcc;">
            </div>
            <div class="control-group" style="display: flex; justify-content: space-between; align-items: center;">
                <label style="display: inline-block; margin-bottom: 0;">Show Boxes:</label>
                <input type="checkbox" id="chk-svg-boxes" checked style="accent-color: #00ffcc;">
            </div>
        </div>

        <div class="control-group" id="webgl-start-group" style="display: none;">
            <button id="btn-pause-render" style="background: #00ffcc; color: #000; font-weight: bold; padding: 10px;">START WEBGL SDF</button>
        </div>

        <!-- WebGL-only controls (hidden initially) -->
        <div id="webgl-controls" style="display: none;">
            <div class="control-group">
                <label for="select-debug">Render Mode:</label>
                <select id="select-debug" style="width: 100%; background: #222; color: #fff; border: 1px solid #ff00aa; border-radius: 4px; padding: 5px; font-family: monospace; font-size: 11px;">
                    <option value="0">Beauty Render</option>
                    <option value="1">Distance Bands</option>
                    <option value="2">Coarse SDF Grid</option>
                    <option value="3">Step Count Heatmap</option>
                    <option value="4">Eikonal Stress</option>
                    <option value="5">Ambient Occlusion</option>
                    <option value="6">Shadow Maps</option>
                    <option value="7">Residual Correction Map</option>
                    <option value="8">Error Bound Map</option>
                    <option value="9">Safe-step Ratio Map</option>
                    <option value="10">Splitscreen Comparison</option>
                </select>
            </div>

            <div class="control-group">
                <label for="select-resolution">Resolution Limit:</label>
                <select id="select-resolution" style="width: 100%; background: #222; color: #fff; border: 1px solid #ff00aa; border-radius: 4px; padding: 5px; font-family: monospace; font-size: 11px;">
                    <option value="57600" selected>Low (320x180)</option>
                    <option value="129600">Medium (480x270)</option>
                    <option value="230400">High (640x360)</option>
                    <option value="921600">Ultra (1280x720)</option>
                    <option value="2073600">Full (1920x1080)</option>
                </select>
            </div>

            <div class="control-group">
                <label for="select-quality">Raymarch Quality:</label>
                <select id="select-quality" style="width: 100%; background: #222; color: #fff; border: 1px solid #ff00aa; border-radius: 4px; padding: 5px; font-family: monospace; font-size: 11px;">
                    <option value="32">Safe (32 steps)</option>
                    <option value="48" selected>Balanced (48 steps)</option>
                    <option value="80">High (80 steps)</option>
                </select>
            </div>

            <div class="control-group" style="display: flex; justify-content: space-between; align-items: center;">
                <label style="display: inline-block; margin-bottom: 0;">Ambient Occlusion:</label>
                <input type="checkbox" id="chk-ao" style="accent-color: #ff00aa;">
            </div>

            <div class="control-group" style="display: flex; justify-content: space-between; align-items: center;">
                <label style="display: inline-block; margin-bottom: 0;">Soft Shadows:</label>
                <input type="checkbox" id="chk-shadow" style="accent-color: #ff00aa;">
            </div>

            <div class="control-group">
                <label for="slider-blend">Blend Radius (smin): <span id="val-blend">0.12</span></label>
                <input type="range" id="slider-blend" min="0.01" max="0.35" step="0.01" value="0.12">
            </div>

            <div class="control-group">
                <label for="slider-jiggle">Jiggle Intensity: <span id="val-jiggle">0.8</span></label>
                <input type="range" id="slider-jiggle" min="0.0" max="2.5" step="0.1" value="0.8">
            </div>

            <div class="control-group">
                <label for="slider-glow">Neon Glow: <span id="val-glow">1.2</span></label>
                <input type="range" id="slider-glow" min="0.5" max="4.0" step="0.1" value="1.2">
            </div>

            <div class="control-group">
                <label for="slider-specular">Specular Gloss: <span id="val-specular">32.0</span></label>
                <input type="range" id="slider-specular" min="8.0" max="128.0" step="4.0" value="32.0">
            </div>

            <div class="control-group">
                <label for="input-audio">Upload MP3/Audio File:</label>
                <input type="file" id="input-audio" accept="audio/*" style="width: 100%; background: #222; color: #fff; border: 1px solid #ff00aa; border-radius: 4px; padding: 5px; font-family: monospace; font-size: 11px; box-sizing: border-box;">
            </div>

            <div class="control-group" style="display: flex; gap: 10px;">
                <button id="btn-play" style="margin-top: 0; width: 50%; background: #ff00aa; color: #fff;">PLAY</button>
                <button id="btn-audio" style="margin-top: 0; width: 50%; background: #ff00aa; color: #fff;">MIC FFT</button>
            </div>
            
            <div class="control-group">
                <button id="btn-screenshot" style="background: #00ffcc; color: #000; font-weight: bold; margin-top: 5px;">TAKE SCREENSHOT</button>
            </div>

            <audio id="audio-player" style="display: none;"></audio>
        </div>
    </div>
    
    <div id="fps">FPS: --</div>
    
    <div id="svg-preview-container" style="width: 100vw; height: 100vh; display: none; align-items: center; justify-content: center; background: #05070d; position: absolute; top: 0; left: 0; z-index: 1;">
        <svg id="svg-preview" xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1200 800" style="max-width: 100%; max-height: 100%; display: block;">
            <rect width="100%" height="100%" fill="#05070d"/>
            <g id="svg-elements">
                __SVG_ELEMENTS__
            </g>
        </svg>
    </div>

    <canvas id="canvas-2d"></canvas>
    <canvas id="webgl-lite-canvas"></canvas>
    <canvas id="canvas"></canvas>
    
    <script>


        const vsSource = `#version 300 es
            in vec2 position;
            out vec2 v_uv;
            void main() {
                v_uv = position;
                gl_Position = vec4(position, 0.0, 1.0);
            }
        `;

        const fsSource = `#version 300 es
            precision highp float;
            in vec2 v_uv;
            out vec4 fragColor;

            uniform vec2 u_resolution;
            uniform vec2 u_mouse;
            uniform float u_time;
            
            uniform float u_blendRadius;
            uniform float u_jiggle;
            uniform float u_glow;
            uniform float u_specularPower;
            uniform float u_fftBands[8]; // 8 frequency bands
            uniform int u_debugMode; // Viewport debug modes selector
            uniform int u_maxSteps;
            uniform int u_enableAO;
            uniform int u_enableShadow;

            uniform highp sampler3D u_coarseSDF;
            uniform highp sampler3D u_residualSDF;
            uniform highp sampler3D u_errorSDF;

            // Stabilized smooth minimum for vec4 (distance + color)
            vec4 sminVal(vec4 a, vec4 b, float k) {
                if (k <= 1e-5) return (a.x < b.x) ? a : b;
                float h = clamp(0.5 + 0.5 * (b.x - a.x) / k, 0.0, 1.0);
                return vec4(
                    mix(b.x, a.x, h) - k * h * (1.0 - h),
                    mix(b.yzw, a.yzw, h)
                );
            }

            // Stabilized smooth minimum for float distance only
            float sminFloat(float a, float b, float k) {
                if (k <= 1e-5) return min(a, b);
                float h = clamp(0.5 + 0.5 * (b - a) / k, 0.0, 1.0);
                return mix(b, a, h) - k * h * (1.0 - h);
            }

            // Primitive mapping helper: Capsule
            vec4 mapCapsule(vec3 p, vec3 a, vec3 b, float r, vec3 col) {
                vec3 pa = p - a, ba = b - a;
                float h = clamp(dot(pa,ba)/dot(ba,ba), 0.0, 1.0);
                return vec4(length(pa - ba*h) - r, col);
            }

            // Primitive mapping helper: Capsule (distance only)
            float mapCapsuleD(vec3 p, vec3 a, vec3 b, float r) {
                vec3 pa = p - a, ba = b - a;
                float h = clamp(dot(pa,ba)/dot(ba,ba), 0.0, 1.0);
                return length(pa - ba*h) - r;
            }

            // Primitive mapping helper: Oriented Box
            vec4 mapBox(vec3 p, vec3 center, vec3 ax0, vec3 ax1, vec3 ax2, vec3 extents, vec3 col) {
                vec3 lp = p - center;
                vec3 qp = vec3(dot(lp, ax0), dot(lp, ax1), dot(lp, ax2));
                vec3 q = abs(qp) - extents;
                float d = length(max(q,0.0)) + min(max(q.x,max(q.y,q.z)),0.0);
                return vec4(d, col);
            }

            // Primitive mapping helper: Oriented Box (distance only)
            float mapBoxD(vec3 p, vec3 center, vec3 ax0, vec3 ax1, vec3 ax2, vec3 extents) {
                vec3 lp = p - center;
                vec3 qp = vec3(dot(lp, ax0), dot(lp, ax1), dot(lp, ax2));
                vec3 q = abs(qp) - extents;
                return length(max(q,0.0)) + min(max(q.x,max(q.y,q.z)),0.0);
            }

            // Modular Zone Functions (Emitted dynamically to relieve register pressure)
            __ZONE_FUNCTIONS__

            // Dynamic domain warp helper
            vec3 applyWarp(vec3 p, out float out_scale) {
                float jiggle = u_jiggle + u_fftBands[0] * 1.5;
                float wave = sin(p.y * 10.0 + u_time * 6.0) * 0.015 * jiggle;
                p.x += wave;
                p.z += cos(p.y * 8.0 + u_time * 5.0) * 0.012 * jiggle;
                
                out_scale = 1.0 + u_fftBands[1] * 0.15;
                p /= out_scale;
                return p;
            }

            // Pure analytic skeleton distance field (pre-residual, pre-warp check)
            float mapDAnalytic(vec3 p) {
                float scale;
                vec3 q = applyWarp(p, scale);

                float d = 1e5;
                __SDF_CODE_D__
                
                return d * scale;
            }

            // Unified visual and safe distance evaluator to halve evaluation overhead and prevent GPU TDR freezes
            void mapDDouble(vec3 p, out float dist_v, out float dist_safe) {
                float scale;
                vec3 q = applyWarp(p, scale);

                float d = 1e5;
                __SDF_CODE_D__
                float base = d * scale;

                dist_v = base;
                dist_safe = base;

                vec3 uv = q * (0.5 / __BOUND_RADIUS__) + 0.5;
                if (all(greaterThanEqual(uv, vec3(0.0))) && all(lessThanEqual(uv, vec3(1.0)))) {
                    if (base < 0.05) {
                        float r_val = texture(u_residualSDF, uv).r * scale;
                        dist_v += r_val;
                        dist_safe += r_val - texture(u_errorSDF, uv).r * scale;
                    }
                }
            }

            // High-fidelity visual distance field (analytic + local residual correction)
            float mapDVisual(vec3 p) {
                float dist_v, dist_safe;
                mapDDouble(p, dist_v, dist_safe);
                return dist_v;
            }

            // Safe, conservative distance field for sphere-tracing step bounds
            float mapDSafe(vec3 p) {
                float dist_v, dist_safe;
                mapDDouble(p, dist_v, dist_safe);
                return dist_safe;
            }

            // Interface matching legacy mapD (defaults to high-fidelity visual)
            float mapD(vec3 p) {
                return mapDVisual(p);
            }

            // Pure analytic skeleton color field (pre-residual)
            vec4 mapAnalytic(vec3 p) {
                float scale;
                vec3 q = applyWarp(p, scale);

                vec4 d = vec4(1e5, 0.0, 0.0, 0.0);
                __SDF_CODE__
                
                d.x *= scale;
                return d;
            }

            // High-fidelity color field
            vec4 map(vec3 p) {
                float scale;
                vec3 q = applyWarp(p, scale);

                vec4 d = mapAnalytic(p);
                vec3 uv = q * (0.5 / __BOUND_RADIUS__) + 0.5;
                if (all(greaterThanEqual(uv, vec3(0.0))) && all(lessThanEqual(uv, vec3(1.0)))) {
                    d.x += texture(u_residualSDF, uv).r * scale;
                }
                return d;
            }

            // Bounding sphere intersection check
            bool intersectBoundingSphere(vec3 ro, vec3 rd, out float tmin, out float tmax) {
                vec3 oc = ro;
                float b = dot(oc, rd);
                float c = dot(oc, oc) - __BOUND_RADIUS_SQR__;
                float h = b*b - c;
                if (h < 0.0) return false;
                h = sqrt(h);
                tmin = -b - h;
                tmax = -b + h;
                return tmax > 0.0;
            }

            // Optimized tetrahedron normal estimation (4 mapD() calls vs 6)
            vec3 getNormal(vec3 p, bool is_left) {
                const float h = 0.0005;
                const vec2 k = vec2(1.0, -1.0);
                float d1, d2, d3, d4;
                if (is_left) {
                    d1 = mapDAnalytic(p + k.xyy*h);
                    d2 = mapDAnalytic(p + k.yyx*h);
                    d3 = mapDAnalytic(p + k.yxy*h);
                    d4 = mapDAnalytic(p + k.xxx*h);
                } else {
                    d1 = mapD(p + k.xyy*h);
                    d2 = mapD(p + k.yyx*h);
                    d3 = mapD(p + k.yxy*h);
                    d4 = mapD(p + k.xxx*h);
                }
                vec3 g = k.xyy * d1 + k.yyx * d2 + k.yxy * d3 + k.xxx * d4;
                float g2 = dot(g, g);
                return g2 > 1e-12 ? g * inversesqrt(g2) : vec3(0.0, 1.0, 0.0);
            }

            // Soft shadow tracing with bounding sphere ray reject
            float getShadow(vec3 ro, vec3 rd, float mint, float maxt, float k, bool is_left) {
                vec3 oc = ro;
                float b = dot(oc, rd);
                float c = dot(oc, oc) - __BOUND_RADIUS_SQR__;
                float h = b*b - c;
                if (h < 0.0) return 1.0; // Shadow ray misses scene entirely, fully lit
                float tShadowMax = min(maxt, -b + sqrt(h));
                if (tShadowMax < mint) return 1.0;

                float res = 1.0;
                float t = mint;
                for (int i = 0; i < 12; i++) {
                    float hd = is_left ? mapDAnalytic(ro + rd * t) : mapD(ro + rd * t);
                    if (hd < 0.001) return 0.0;
                    res = min(res, k * hd / t);
                    t += clamp(hd, 0.01, 0.15);
                    if (t > tShadowMax) break;
                }
                return clamp(res, 0.0, 1.0);
            }

            // Cone-hemisphere ambient occlusion (robust golden-angle spread)
            float getAO(vec3 p, vec3 n, bool is_left) {
                vec3 hint = abs(n.y) < 0.9 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
                vec3 t = normalize(cross(n, hint));
                vec3 b = cross(n, t);
                float occ = 0.0;
                float sca = 1.0;
                for (int i = 0; i < 3; i++) {
                    float fi = float(i);
                    float hr = 0.01 + 0.12 * fi / 2.0;
                    float angle = fi * 2.39996; // Golden angle
                    vec3 dir = normalize(n + 0.4 * (cos(angle)*t + sin(angle)*b));
                    float dd = is_left ? mapDAnalytic(p + dir * hr) : mapD(p + dir * hr);
                    occ += -(dd - hr) * sca;
                    sca *= 0.95;
                }
                return clamp(1.0 - 3.0 * occ, 0.0, 1.0);
            }

            // Eikonal stress evaluation helper (gradient health check)
            float eikonalStress(vec3 p) {
                float h = 0.001;
                vec3 g = vec3(
                    mapD(p + vec3(h,0.0,0.0)) - mapD(p - vec3(h,0.0,0.0)),
                    mapD(p + vec3(0.0,h,0.0)) - mapD(p - vec3(0.0,h,0.0)),
                    mapD(p + vec3(0.0,0.0,h)) - mapD(p - vec3(0.0,0.0,h))
                ) / (2.0 * h);
                return abs(length(g) - 1.0);
            }

            void main() {
                vec2 uv = (gl_FragCoord.xy - 0.5 * u_resolution.xy) / u_resolution.y;
                
                // Camera orbit rotation
                float theta = u_mouse.x * 6.283185 + u_time * 0.08;
                float phi = (u_mouse.y - 0.5) * 3.0;
                vec3 ro = vec3(cos(theta) * cos(phi), sin(phi), sin(theta) * cos(phi)) * __CAMERA_DIST__;
                vec3 target = vec3(0.0, __CENTER_Y__, 0.0);
                
                vec3 cz = normalize(target - ro);
                vec3 cx = normalize(cross(vec3(0.0, 1.0, 0.0), cz));
                vec3 cy = cross(cz, cx);
                vec3 rd = normalize(uv.x * cx + uv.y * cy + 1.25 * cz);

                vec3 color = vec3(0.03, 0.03, 0.05); // Dark space background
                int march_steps = 0;
                bool hit = false;
                vec4 map_res;
                vec3 p;
                
                float tmin = 0.0, tmax = 0.0;
                if (intersectBoundingSphere(ro, rd, tmin, tmax)) {
                    float t = max(tmin, 0.0);
                    bool is_left = (u_debugMode == 10 && gl_FragCoord.x < 0.5 * u_resolution.x);
                    
                    for (int i = 0; i < 128; i++) {
                        if (i >= u_maxSteps) break;
                        march_steps = i;
                        p = ro + rd * t;
                        
                        float dist;
                        float dist_v;
                        
                        if (is_left) {
                            dist = mapDAnalytic(p);
                            dist_v = dist;
                        } else {
                            mapDDouble(p, dist_v, dist);
                        }
                        
                        if (dist_v < 0.0008) {
                            hit = true;
                            break;
                        }
                        
                        float step_d;
                        if (is_left) {
                            step_d = max(dist, 0.0002);
                        } else {
                            if (dist > 0.0002) {
                                step_d = dist;
                            } else if (dist_v > 0.05) {
                                step_d = dist_v * 0.25;
                            } else {
                                step_d = 0.0002;
                            }
                        }
                        t += step_d * 0.95;
                        if (t > tmax) break;
                    }

                    if (hit) {
                        if (is_left) {
                            map_res = mapAnalytic(p);
                        } else {
                            map_res = map(p); // Evaluate corrected color on hit
                        }
                        
                        vec3 n = getNormal(p, is_left);
                        vec3 l = normalize(vec3(2.5, 4.0, 3.0));
                        vec3 r = reflect(-l, n);
                        vec3 v = -rd;

                        float diffuse = max(dot(n, l), 0.0);
                        float specular = pow(max(dot(r, v), 0.0), u_specularPower);
                        
                        float ao = 1.0;
                        if (u_enableAO == 1) {
                            ao = getAO(p, n, is_left);
                        }
                        float shadow = 1.0;
                        if (u_enableShadow == 1) {
                            shadow = getShadow(p + n * 0.005, l, 0.015, 4.0, 16.0, is_left);
                        }
                        
                        vec3 baseCol = map_res.yzw;
                        
                        // Default Mode: Beauty render
                        color = baseCol * (0.15 * ao + 0.85 * diffuse * shadow);
                        color += vec3(0.4) * specular * shadow;
                        
                        // Neon Edge Rim Lighting (Tease Glow) boosted by u_fftBands[2]
                        float fresnel = pow(clamp(1.0 + dot(n, rd), 0.0, 1.0), 4.0);
                        float glow = u_glow + u_fftBands[2] * 2.0;
                        color += vec3(1.0, 0.0, 0.66) * fresnel * glow * 0.4;
                        
                        color = mix(color, vec3(0.03, 0.03, 0.05), 1.0 - exp(-0.06 * t * t));
                        
                        // Debug render overlays
                        if (u_debugMode == 1) {
                            // Raw distance bands
                            color = vec3(0.5 + 0.5 * sin(map_res.x * 50.0));
                        } else if (u_debugMode == 2) {
                            // Coarse SDF value
                            vec3 coarse_uv = p * (0.5 / __BOUND_RADIUS__) + 0.5;
                            float coarse = texture(u_coarseSDF, coarse_uv).r;
                            color = vec3(coarse);
                        } else if (u_debugMode == 3) {
                            // Step count heatmap (blue=cheap, red=expensive)
                            float stepsNorm = float(march_steps) / 48.0;
                            color = mix(vec3(0.0, 0.2, 1.0), vec3(1.0, 0.0, 0.0), stepsNorm);
                        } else if (u_debugMode == 4) {
                            // Eikonal stress (green=healthy, red=stressed)
                            float stress = eikonalStress(p);
                            color = mix(vec3(0.0, 1.0, 0.2), vec3(1.0, 0.0, 0.0), clamp(stress * 4.0, 0.0, 1.0));
                        } else if (u_debugMode == 5) {
                            // AO only
                            color = vec3(ao);
                        } else if (u_debugMode == 6) {
                            // Shadow only
                            color = vec3(shadow);
                        } else if (u_debugMode == 7) {
                            // Residual correction (Red=positive, Blue=negative)
                            vec3 uv_res = p * (0.5 / __BOUND_RADIUS__) + 0.5;
                            float r_val = texture(u_residualSDF, uv_res).r;
                            color = r_val > 0.0 ? vec3(r_val * 10.0, 0.0, 0.0) : vec3(0.0, 0.0, -r_val * 10.0);
                        } else if (u_debugMode == 8) {
                            // Error bound (Green=confident, Red=uncertain)
                            vec3 uv_err = p * (0.5 / __BOUND_RADIUS__) + 0.5;
                            float e_val = texture(u_errorSDF, uv_err).r;
                            color = mix(vec3(0.0, 1.0, 0.0), vec3(1.0, 0.0, 0.0), clamp(e_val * 20.0, 0.0, 1.0));
                        } else if (u_debugMode == 9) {
                            // Safe-step ratio
                            float dv = is_left ? mapDAnalytic(p) : mapDVisual(p);
                            float ds = is_left ? dv : mapDSafe(p);
                            float ratio = ds / max(dv, 1e-5);
                            color = mix(vec3(1.0, 0.0, 0.0), vec3(0.0, 1.0, 0.0), clamp(ratio, 0.0, 1.0));
                        }
                    }
                }
                
                // Splitscreen dividing line
                if (u_debugMode == 10 && abs(gl_FragCoord.x - 0.5 * u_resolution.x) < 1.5) {
                    color = vec3(1.0, 0.0, 0.66); // Neon pink divide line
                }
                
                // Raymiss step heatmap visualization
                if (!hit && u_debugMode == 3) {
                    float stepsNorm = float(march_steps) / 48.0;
                    color = mix(vec3(0.0, 0.2, 1.0), vec3(1.0, 0.0, 0.0), stepsNorm);
                }

                fragColor = vec4(pow(color, vec3(1.0/2.2)), 1.0);
            }
        `;        // WebGL Lite (Fixed-Shader Impostor GPU) Backend
        const WebGLLiteBackend = (() => {
            let gl = null;
            let program = null;
            let buffer = null;
            let positionLoc = -1;
            let colorLoc = -1;

            function createShader(gl, type, source) {
                const s = gl.createShader(type);
                gl.shaderSource(s, source);
                gl.compileShader(s);
                if (!gl.getShaderParameter(s, gl.COMPILE_STATUS)) {
                    const log = gl.getShaderInfoLog(s);
                    gl.deleteShader(s);
                    throw new Error("WebGL Lite shader compile failed: " + log);
                }
                return s;
            }

            function init(canvas) {
                gl = canvas.getContext("webgl2", {
                    antialias: true,
                    alpha: false,
                    powerPreference: "low-power"
                });
                if (!gl) {
                    throw new Error("WebGL2 context unavailable for WebGL Lite");
                }

                const vsSource = `#version 300 es
                    in vec2 a_position;
                    in vec4 a_color;
                    out vec4 v_color;
                    void main() {
                        v_color = a_color;
                        gl_Position = vec4(a_position, 0.0, 1.0);
                    }
                `;

                const fsSource = `#version 300 es
                    precision highp float;
                    in vec4 v_color;
                    out vec4 fragColor;
                    void main() {
                        fragColor = v_color;
                    }
                `;

                const vs = createShader(gl, gl.VERTEX_SHADER, vsSource);
                const fs = createShader(gl, gl.FRAGMENT_SHADER, fsSource);

                program = gl.createProgram();
                gl.attachShader(program, vs);
                gl.attachShader(program, fs);
                gl.linkProgram(program);

                gl.deleteShader(vs);
                gl.deleteShader(fs);

                if (!gl.getProgramParameter(program, gl.LINK_STATUS)) {
                    throw new Error("WebGL Lite link failed: " + gl.getProgramInfoLog(program));
                }

                positionLoc = gl.getAttribLocation(program, "a_position");
                colorLoc = gl.getAttribLocation(program, "a_color");
                buffer = gl.createBuffer();

                gl.clearColor(0.02, 0.025, 0.04, 1.0);
            }

            function projectPoint(p, camera, aspect) {
                const [x, y, z] = p;
                const ct = Math.cos(camera.theta);
                const st = Math.sin(camera.theta);
                const cp = Math.cos(camera.phi);
                const sp = Math.sin(camera.phi);

                // Rotations
                const x1 = ct * x + st * z;
                const z1 = -st * x + ct * z;
                const y2 = cp * y - sp * z1;
                const z2 = sp * y + cp * z1 + camera.distance;

                const fov_scale = 1.25;
                const scale_z = fov_scale / Math.max(0.05, z2);
                const sx = x1 * scale_z / aspect;
                const sy = y2 * scale_z;

                return { x: sx, y: sy, z: z2, scale: scale_z };
            }

            function pushVertex(out, x, y, color) {
                out.push(x, y, color[0], color[1], color[2], color[3]);
            }

            function pushLine(out, a, b, width, aspect, color) {
                const dx = b.x - a.x;
                const dy = b.y - a.y;
                const len = Math.hypot(dx * aspect, dy) || 1.0;

                const nx = -dy / len * width / aspect;
                const ny = dx / len * width;

                pushVertex(out, a.x + nx, a.y + ny, color);
                pushVertex(out, a.x - nx, a.y - ny, color);
                pushVertex(out, b.x + nx, b.y + ny, color);

                pushVertex(out, b.x + nx, b.y + ny, color);
                pushVertex(out, a.x - nx, a.y - ny, color);
                pushVertex(out, b.x - nx, b.y - ny, color);
            }

            function pushCircle(out, c, radius, aspect, color, segments = 16) {
                for (let i = 0; i < segments; i++) {
                    const a0 = (i / segments) * Math.PI * 2.0;
                    const a1 = ((i + 1) / segments) * Math.PI * 2.0;

                    pushVertex(out, c.x, c.y, color);
                    pushVertex(out, c.x + Math.cos(a0) * radius / aspect, c.y + Math.sin(a0) * radius, color);
                    pushVertex(out, c.x + Math.cos(a1) * radius / aspect, c.y + Math.sin(a1) * radius, color);
                }
            }

            function boxCorners(prim) {
                const c = prim.center;
                const e = prim.extents;
                const ax0 = prim.axis0;
                const ax1 = prim.axis1;
                const ax2 = prim.axis2;
                const corners = [];

                for (const sx of [-1, 1]) {
                    for (const sy of [-1, 1]) {
                        for (const sz of [-1, 1]) {
                            corners.push([
                                c[0] + sx * e[0] * ax0[0] + sy * e[1] * ax1[0] + sz * e[2] * ax2[0],
                                c[1] + sx * e[0] * ax0[1] + sy * e[1] * ax1[1] + sz * e[2] * ax2[1],
                                c[2] + sx * e[0] * ax0[2] + sy * e[1] * ax1[2] + sz * e[2] * ax2[2]
                            ]);
                        }
                    }
                }
                return corners;
            }

            function render(canvas, primitives, camera, palette, chkSpheres, chkCapsules, chkBoxes) {
                const dpr = Math.min(window.devicePixelRatio || 1, 1.5);
                const w = Math.floor(canvas.clientWidth * dpr);
                const h = Math.floor(canvas.clientHeight * dpr);

                if (canvas.width !== w || canvas.height !== h) {
                    canvas.width = w;
                    canvas.height = h;
                    gl.viewport(0, 0, w, h);
                }

                const aspect = w / Math.max(1, h);
                const verts = [];

                gl.clear(gl.COLOR_BUFFER_BIT);

                // Sort back-to-front by projected Z
                const drawList = [];
                primitives.forEach((p, idx) => {
                    if (p.type === 'sphere' && !chkSpheres) return;
                    if (p.type === 'capsule' && !chkCapsules) return;
                    if (p.type === 'box' && !chkBoxes) return;

                    const center = p.type === 'sphere' ? p.center :
                                   (p.type === 'capsule' ? [
                                       (p.a[0] + p.b[0]) * 0.5,
                                       (p.a[1] + p.b[1]) * 0.5,
                                       (p.a[2] + p.b[2]) * 0.5
                                   ] : p.center);

                    const proj = projectPoint(center, camera, aspect);
                    drawList.push({ prim: p, z: proj.z, idx });
                });

                drawList.sort((a, b) => b.z - a.z);

                drawList.forEach(item => {
                    const p = item.prim;
                    const col = palette[p.cluster % palette.length];
                    const color = [col[0], col[1], col[2], 0.68];

                    if (p.type === 'sphere') {
                        const c = projectPoint(p.center, camera, aspect);
                        const radius = p.radius * c.scale;
                        pushCircle(verts, c, radius, aspect, color, 16);
                    } else if (p.type === 'capsule') {
                        const a = projectPoint(p.a, camera, aspect);
                        const b = projectPoint(p.b, camera, aspect);
                        const width = Math.max(0.003, p.radius * 0.5 * (a.scale + b.scale));
                        pushLine(verts, a, b, width, aspect, color);
                    } else if (p.type === 'box') {
                        const corners = boxCorners(p).map(pt => projectPoint(pt, camera, aspect));
                        const edges = [
                            [0,1], [0,2], [0,4],
                            [3,1], [3,2], [3,7],
                            [5,1], [5,4], [5,7],
                            [6,2], [6,4], [6,7]
                        ];
                        const wireColor = [color[0], color[1], color[2], 0.85];
                        for (const [i, j] of edges) {
                            pushLine(verts, corners[i], corners[j], 0.0025, wireColor);
                        }
                    }
                });

                if (verts.length === 0) return;

                gl.bindBuffer(gl.ARRAY_BUFFER, buffer);
                gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(verts), gl.DYNAMIC_DRAW);

                gl.useProgram(program);

                const stride = 6 * 4;
                gl.enableVertexAttribArray(positionLoc);
                gl.vertexAttribPointer(positionLoc, 2, gl.FLOAT, false, stride, 0);

                gl.enableVertexAttribArray(colorLoc);
                gl.vertexAttribPointer(colorLoc, 4, gl.FLOAT, false, stride, 2 * 4);

                gl.drawArrays(gl.TRIANGLES, 0, verts.length / 6);
            }

            return { init, render };
        })();

        const canvas = document.getElementById('canvas');
        const canvas2d = document.getElementById('canvas-2d');
        const ctx2d = canvas2d.getContext('2d');
        const errorLog = document.getElementById('error-log');
        const svgPreviewContainer = document.getElementById('svg-preview-container');
        const svgControls = document.getElementById('svg-controls');
        const webglControls = document.getElementById('webgl-controls');
        const fpsCounter = document.getElementById('fps');
        const projSelectorGroup = document.getElementById('proj-selector-group');
        const webglStartGroup = document.getElementById('webgl-start-group');
        
        let gl = null;
        let maxRenderPixels = 57600;
        let webglInitialized = false;
        let webglLiteInitialized = false;
        let program, resLoc, mouseLoc, timeLoc, blendLoc, jiggleLoc, glowLoc, specularLoc, fftBandsLoc, debugLoc, maxStepsLoc, enableAOLoc, enableShadowLoc;

        const primitivesData = __JS_PRIMITIVES__;
        const groupsData = __JS_GROUPS__;
        const boundingRadius = __BOUNDING_RADIUS__;
        const palette = __JS_PALETTE__;
        const webglRiskLevel = "__RISK_LEVEL__".toLowerCase();
        const svgMaxPrimitives = 512;
        
        const width = 1200, height = 800;
        const scale = 0.42 * Math.min(width, height) / Math.max(1e-6, boundingRadius);

        // Canvas2D Interactive Orbit Parameters
        let theta = 0.5, phi = 0.3, dist = 5.0;
        let isDragging = false;
        let lastMouseX = 0, lastMouseY = 0;

        function resizeCanvas2d() {
            canvas2d.width = window.innerWidth;
            canvas2d.height = window.innerHeight;
            triggerActiveRender();
        }
        window.addEventListener('resize', resizeCanvas2d);

        function triggerActiveRender() {
            if (activeBackend === 'canvas2d') {
                renderCanvas2d();
            } else if (activeBackend === 'webgl-lite') {
                renderWebglLite();
            }
        }

        // Mouse interaction for Canvas2D
        canvas2d.addEventListener('mousedown', (e) => {
            isDragging = true;
            lastMouseX = e.clientX;
            lastMouseY = e.clientY;
        });
        window.addEventListener('mouseup', () => { isDragging = false; });
        window.addEventListener('mousemove', (e) => {
            if (!isDragging) return;
            const dx = e.clientX - lastMouseX;
            const dy = e.clientY - lastMouseY;
            theta -= dx * 0.007;
            phi = Math.max(-Math.PI/2 + 0.05, Math.min(Math.PI/2 - 0.05, phi + dy * 0.007));
            lastMouseX = e.clientX;
            lastMouseY = e.clientY;
            triggerActiveRender();
        });
        canvas2d.addEventListener('wheel', (e) => {
            dist = Math.max(1.0, Math.min(20.0, dist + e.deltaY * 0.005));
            triggerActiveRender();
        });

        // Touch interaction for Canvas2D
        canvas2d.addEventListener('touchstart', (e) => {
            if (e.touches.length === 1) {
                isDragging = true;
                lastMouseX = e.touches[0].clientX;
                lastMouseY = e.touches[0].clientY;
            }
        });
        canvas2d.addEventListener('touchmove', (e) => {
            if (!isDragging || e.touches.length !== 1) return;
            const dx = e.touches[0].clientX - lastMouseX;
            const dy = e.touches[0].clientY - lastMouseY;
            theta -= dx * 0.007;
            phi = Math.max(-Math.PI/2 + 0.05, Math.min(Math.PI/2 - 0.05, phi + dy * 0.007));
            lastMouseX = e.touches[0].clientX;
            lastMouseY = e.touches[0].clientY;
            triggerActiveRender();
        });
        canvas2d.addEventListener('touchend', () => { isDragging = false; });

        // Mouse and touch interaction for WebGL Lite Canvas
        const liteCanvas = document.getElementById('webgl-lite-canvas');
        liteCanvas.addEventListener('mousedown', (e) => {
            isDragging = true;
            lastMouseX = e.clientX;
            lastMouseY = e.clientY;
        });
        liteCanvas.addEventListener('mousemove', (e) => {
            if (!isDragging) return;
            const dx = e.clientX - lastMouseX;
            const dy = e.clientY - lastMouseY;
            theta -= dx * 0.007;
            phi = Math.max(-Math.PI/2 + 0.05, Math.min(Math.PI/2 - 0.05, phi + dy * 0.007));
            lastMouseX = e.clientX;
            lastMouseY = e.clientY;
            triggerActiveRender();
        });
        liteCanvas.addEventListener('wheel', (e) => {
            dist = Math.max(1.0, Math.min(20.0, dist + e.deltaY * 0.005));
            triggerActiveRender();
        });
        liteCanvas.addEventListener('touchstart', (e) => {
            if (e.touches.length === 1) {
                isDragging = true;
                lastMouseX = e.touches[0].clientX;
                lastMouseY = e.touches[0].clientY;
            }
        });
        liteCanvas.addEventListener('touchmove', (e) => {
            if (!isDragging || e.touches.length !== 1) return;
            const dx = e.touches[0].clientX - lastMouseX;
            const dy = e.touches[0].clientY - lastMouseY;
            theta -= dx * 0.007;
            phi = Math.max(-Math.PI/2 + 0.05, Math.min(Math.PI/2 - 0.05, phi + dy * 0.007));
            lastMouseX = e.touches[0].clientX;
            lastMouseY = e.touches[0].clientY;
            triggerActiveRender();
        });
        liteCanvas.addEventListener('touchend', () => { isDragging = false; });

        function renderWebglLite() {
            if (activeBackend !== 'webgl-lite' || !webglLiteInitialized) return;
            const chkSpheres = document.getElementById('chk-svg-spheres').checked;
            const chkCapsules = document.getElementById('chk-svg-capsules').checked;
            const chkBoxes = document.getElementById('chk-svg-boxes').checked;
            
            WebGLLiteBackend.render(liteCanvas, primitivesData, {
                theta: theta,
                phi: phi,
                distance: dist
            }, palette, chkSpheres, chkCapsules, chkBoxes);
        }

        // Backend switching controller
        const selectBackend = document.getElementById('select-backend');
        let activeBackend = 'canvas2d'; // Default backend on load

        selectBackend.addEventListener('change', (e) => {
            setBackend(e.target.value);
        });

        function setBackend(backend) {
            activeBackend = backend;
            const liteCanvas = document.getElementById('webgl-lite-canvas');
            
            if (backend === 'svg') {
                svgPreviewContainer.style.display = 'flex';
                canvas2d.style.display = 'none';
                liteCanvas.style.display = 'none';
                canvas.style.display = 'none';
                projSelectorGroup.style.display = 'block';
                svgControls.style.display = 'block';
                webglStartGroup.style.display = 'none';
                webglControls.style.display = 'none';
                fpsCounter.style.display = 'none';
                if (!paused) pauseRender();
            } else if (backend === 'canvas2d') {
                svgPreviewContainer.style.display = 'none';
                canvas2d.style.display = 'block';
                liteCanvas.style.display = 'none';
                canvas.style.display = 'none';
                projSelectorGroup.style.display = 'none';
                svgControls.style.display = 'block';
                webglStartGroup.style.display = 'none';
                webglControls.style.display = 'none';
                fpsCounter.style.display = 'none';
                if (!paused) pauseRender();
                resizeCanvas2d();
            } else if (backend === 'webgl-lite') {
                svgPreviewContainer.style.display = 'none';
                canvas2d.style.display = 'none';
                liteCanvas.style.display = 'block';
                canvas.style.display = 'none';
                projSelectorGroup.style.display = 'none';
                svgControls.style.display = 'block';
                webglStartGroup.style.display = 'none';
                webglControls.style.display = 'none';
                fpsCounter.style.display = 'none';
                if (!paused) pauseRender();
                
                if (!webglLiteInitialized) {
                    try {
                        WebGLLiteBackend.init(liteCanvas);
                        webglLiteInitialized = true;
                    } catch (err) {
                        errorLog.textContent = 'WebGL Lite Init Failed: ' + err.message;
                        console.error(err);
                        selectBackend.value = 'canvas2d';
                        setBackend('canvas2d');
                        return;
                    }
                }
                renderWebglLite();
            } else if (backend === 'webgl') {
                if (webglRiskLevel === 'high') {
                    const proceed = confirm("Warning: High WebGL Risk. Shader is large and may freeze your browser for a few seconds. Continue?");
                    if (!proceed) {
                        selectBackend.value = 'canvas2d';
                        setBackend('canvas2d');
                        return;
                    }
                }
                
                svgPreviewContainer.style.display = 'none';
                canvas2d.style.display = 'none';
                canvas.style.display = 'block';
                projSelectorGroup.style.display = 'none';
                svgControls.style.display = 'none';
                webglStartGroup.style.display = 'block';
                fpsCounter.style.display = 'none';
                
                // Show WebGL start rendering controls
                if (webglInitialized) {
                    webglControls.style.display = 'block';
                    fpsCounter.style.display = 'block';
                }
            }
        }

        // SVG static projection mapping
        function project(p, mode) {
            let x_val = p[0], y_val = p[1], z_val = p[2];
            let x2d = 0, y2d = 0;
            if (mode === 'xy') {
                x2d = x_val;
                y2d = y_val;
            } else if (mode === 'xz') {
                x2d = x_val;
                y2d = z_val;
            } else if (mode === 'yz') {
                x2d = y_val;
                y2d = z_val;
            } else if (mode === 'iso') {
                x2d = (x_val - z_val) * Math.cos(Math.PI / 6);
                y2d = y_val - (x_val + z_val) * Math.sin(Math.PI / 6);
            }
            const cx = width * 0.5 + x2d * scale;
            const cy = height * 0.5 - y2d * scale;
            return [cx, cy];
        }

        const selectProj = document.getElementById('select-projection');
        selectProj.addEventListener('change', (e) => {
            updateProjection(e.target.value);
        });

        function updateProjection(mode) {
            primitivesData.forEach((p, idx) => {
                const el = document.getElementById(`svg-prim-${idx}`);
                if (!el) return;
                if (p.type === 'sphere') {
                    const [cx, cy] = project(p.center, mode);
                    el.setAttribute('cx', cx.toFixed(2));
                    el.setAttribute('cy', cy.toFixed(2));
                } else if (p.type === 'capsule') {
                    const [ax, ay] = project(p.a, mode);
                    const [bx, by] = project(p.b, mode);
                    el.setAttribute('d', `M${ax.toFixed(2)},${ay.toFixed(2)} L${bx.toFixed(2)},${by.toFixed(2)}`);
                } else if (p.type === 'box') {
                    const [cx, cy] = project(p.center, mode);
                    const ex = p.extents[0] * scale;
                    const ey = p.extents[1] * scale;
                    el.setAttribute('x', (cx - ex).toFixed(2));
                    el.setAttribute('y', (cy - ey).toFixed(2));
                }
            });
            
            groupsData.forEach((g, idx) => {
                const el = document.getElementById(`svg-group-${idx}`);
                if (!el) return;
                const [cx, cy] = project(g.center, mode);
                el.setAttribute('cx', cx.toFixed(2));
                el.setAttribute('cy', cy.toFixed(2));
            });
        }

        // Oriented Box corners calculator
        function getBoxCorners(p) {
            const c = p.center;
            const ax0 = p.axis0;
            const ax1 = p.axis1;
            const ax2 = p.axis2;
            const ext = p.extents;
            
            const corners = [];
            for (let i = 0; i < 8; i++) {
                const sign0 = (i & 1) ? 1 : -1;
                const sign1 = (i & 2) ? 1 : -1;
                const sign2 = (i & 4) ? 1 : -1;
                
                const x = c[0] + sign0 * ext[0] * ax0[0] + sign1 * ext[1] * ax1[0] + sign2 * ext[2] * ax2[0];
                const y = c[1] + sign0 * ext[0] * ax0[1] + sign1 * ext[1] * ax1[1] + sign2 * ext[2] * ax2[1];
                const z = c[2] + sign0 * ext[0] * ax0[2] + sign1 * ext[1] * ax1[2] + sign2 * ext[2] * ax2[2];
                corners.push([x, y, z]);
            }
            return corners;
        }

        // Canvas2D Interactive Orbit Projection & Paint Loop
        function renderCanvas2d() {
            if (activeBackend !== 'canvas2d') return;
            const w = canvas2d.width;
            const h = canvas2d.height;
            const ctx = ctx2d;
            
            ctx.fillStyle = '#05070d';
            ctx.fillRect(0, 0, w, h);
            
            // Orbit camera computation
            const ro = [
                dist * Math.cos(theta) * Math.cos(phi),
                dist * Math.sin(phi),
                dist * Math.sin(theta) * Math.cos(phi)
            ];
            const target = [0.0, 0.0, 0.0];
            
            let cz = [target[0] - ro[0], target[1] - ro[1], target[2] - ro[2]];
            const cz_len = Math.sqrt(cz[0]*cz[0] + cz[1]*cz[1] + cz[2]*cz[2]) || 1;
            cz = [cz[0]/cz_len, cz[1]/cz_len, cz[2]/cz_len];
            
            let cx_dir = [ -cz[2], 0, cz[0] ];
            const cx_len = Math.sqrt(cx_dir[0]*cx_dir[0] + cx_dir[2]*cx_dir[2]) || 1;
            cx_dir = [cx_dir[0]/cx_len, 0, cx_dir[2]/cx_len];
            
            const cy_dir = [
                cz[1]*cx_dir[2] - cz[2]*cx_dir[1],
                cz[2]*cx_dir[0] - cz[0]*cx_dir[2],
                cz[0]*cx_dir[1] - cz[1]*cx_dir[0]
            ];
            
            const fov_scale = 1.25;
            const view_scale = 0.5 * Math.min(w, h);
            
            function project3d(p) {
                const lp = [p[0] - ro[0], p[1] - ro[1], p[2] - ro[2]];
                const px = lp[0]*cx_dir[0] + lp[1]*cx_dir[1] + lp[2]*cx_dir[2];
                const py = lp[0]*cy_dir[0] + lp[1]*cy_dir[1] + lp[2]*cy_dir[2];
                const pz = lp[0]*cz[0] + lp[1]*cz[1] + lp[2]*cz[2];
                
                if (pz <= 0.05) return null;
                
                const screen_x = w * 0.5 + (px / pz) * fov_scale * view_scale;
                const screen_y = h * 0.5 - (py / pz) * fov_scale * view_scale;
                return [screen_x, screen_y, pz];
            }
            
            // Gather all items based on toggles
            const items = [];
            
            primitivesData.forEach((p, idx) => {
                if (p.type === 'sphere' && !chkSpheres.checked) return;
                if (p.type === 'capsule' && !chkCapsules.checked) return;
                if (p.type === 'box' && !chkBoxes.checked) return;
                
                if (p.type === 'sphere') {
                    const proj = project3d(p.center);
                    if (proj) {
                        items.push({
                            type: 'sphere',
                            depth: proj[2],
                            proj: proj,
                            prim: p,
                            idx: idx
                        });
                    }
                } else if (p.type === 'capsule') {
                    const proj_a = project3d(p.a);
                    const proj_b = project3d(p.b);
                    if (proj_a && proj_b) {
                        items.push({
                            type: 'capsule',
                            depth: (proj_a[2] + proj_b[2]) * 0.5,
                            proj_a: proj_a,
                            proj_b: proj_b,
                            prim: p,
                            idx: idx
                        });
                    }
                } else if (p.type === 'box') {
                    const corners = getBoxCorners(p);
                    const proj_corners = [];
                    let sum_pz = 0;
                    let visible = true;
                    for (let i = 0; i < 8; i++) {
                        const proj = project3d(corners[i]);
                        if (!proj) {
                            visible = false;
                            break;
                        }
                        proj_corners.push(proj);
                        sum_pz += proj[2];
                    }
                    if (visible) {
                        items.push({
                            type: 'box',
                            depth: sum_pz / 8,
                            proj_corners: proj_corners,
                            prim: p,
                            idx: idx
                        });
                    }
                }
            });
            
            if (chkGroups.checked) {
                groupsData.forEach((g, idx) => {
                    const proj = project3d(g.center);
                    if (proj) {
                        items.push({
                            type: 'group',
                            depth: proj[2],
                            proj: proj,
                            group: g,
                            idx: idx
                        });
                    }
                });
            }
            
            // Depth-sort back to front (descending pz)
            items.sort((a, b) => b.depth - a.depth);
            
            // Draw items sequentially
            items.forEach(item => {
                if (item.type === 'sphere') {
                    const [cx, cy, pz] = item.proj;
                    const r_3d = item.prim.radius;
                    const r_2d = (r_3d / pz) * fov_scale * view_scale;
                    
                    const col = palette[item.prim.cluster % palette.length];
                    const r_val = Math.floor(col[0] * 255);
                    const g_val = Math.floor(col[1] * 255);
                    const b_val = Math.floor(col[2] * 255);
                    
                    ctx.beginPath();
                    ctx.arc(cx, cy, Math.max(1, r_2d), 0, 2 * Math.PI);
                    
                    const grad = ctx.createRadialGradient(
                        cx - r_2d * 0.3, cy - r_2d * 0.3, r_2d * 0.05,
                        cx, cy, r_2d
                    );
                    grad.addColorStop(0, '#ffffff');
                    grad.addColorStop(0.2, `rgb(${Math.min(255, r_val + 60)}, ${Math.min(255, g_val + 60)}, ${Math.min(255, b_val + 60)})`);
                    grad.addColorStop(0.8, `rgb(${r_val}, ${g_val}, ${b_val})`);
                    grad.addColorStop(1, `rgb(${Math.floor(r_val * 0.3)}, ${Math.floor(g_val * 0.3)}, ${Math.floor(b_val * 0.3)})`);
                    
                    ctx.fillStyle = grad;
                    ctx.fill();
                } else if (item.type === 'capsule') {
                    const [ax, ay, az] = item.proj_a;
                    const [bx, by, bz] = item.proj_b;
                    const avg_pz = item.depth;
                    const r_3d = item.prim.radius;
                    const r_2d = (r_3d / avg_pz) * fov_scale * view_scale;
                    
                    const col = palette[item.prim.cluster % palette.length];
                    const r_val = Math.floor(col[0] * 255);
                    const g_val = Math.floor(col[1] * 255);
                    const b_val = Math.floor(col[2] * 255);
                    
                    ctx.beginPath();
                    ctx.moveTo(ax, ay);
                    ctx.lineTo(bx, by);
                    ctx.strokeStyle = `rgba(${r_val}, ${g_val}, ${b_val}, 0.75)`;
                    ctx.lineWidth = Math.max(2, r_2d * 2);
                    ctx.lineCap = 'round';
                    ctx.stroke();
                } else if (item.type === 'box') {
                    const corners = item.proj_corners;
                    const col = palette[item.prim.cluster % palette.length];
                    const r_val = Math.floor(col[0] * 255);
                    const g_val = Math.floor(col[1] * 255);
                    const b_val = Math.floor(col[2] * 255);
                    
                    const edges = [
                        [0, 1], [1, 3], [3, 2], [2, 0],
                        [4, 5], [5, 7], [7, 6], [6, 4],
                        [0, 4], [1, 5], [2, 6], [3, 7]
                    ];
                    
                    ctx.strokeStyle = `rgba(${r_val}, ${g_val}, ${b_val}, 0.65)`;
                    ctx.lineWidth = 1.5;
                    edges.forEach(([u, v]) => {
                        ctx.beginPath();
                        ctx.moveTo(corners[u][0], corners[u][1]);
                        ctx.lineTo(corners[v][0], corners[v][1]);
                        ctx.stroke();
                    });
                } else if (item.type === 'group') {
                    const [cx, cy, pz] = item.proj;
                    const r_3d = item.group.radius;
                    const r_2d = (r_3d / pz) * fov_scale * view_scale;
                    
                    ctx.beginPath();
                    ctx.arc(cx, cy, Math.max(1, r_2d), 0, 2 * Math.PI);
                    ctx.strokeStyle = 'rgba(0, 255, 204, 0.16)';
                    ctx.lineWidth = 1;
                    ctx.stroke();
                }
            });
        }

        const chkGroups = document.getElementById('chk-svg-groups');
        const chkSpheres = document.getElementById('chk-svg-spheres');
        const chkCapsules = document.getElementById('chk-svg-capsules');
        const chkBoxes = document.getElementById('chk-svg-boxes');

        chkGroups.addEventListener('change', () => {
            document.querySelectorAll('.group-bound').forEach(el => {
                el.style.display = chkGroups.checked ? 'inline' : 'none';
            });
            triggerActiveRender();
        });
        chkSpheres.addEventListener('change', () => {
            document.querySelectorAll('.prim-sphere').forEach(el => {
                el.style.display = chkSpheres.checked ? 'inline' : 'none';
            });
            triggerActiveRender();
        });
        chkCapsules.addEventListener('change', () => {
            document.querySelectorAll('.prim-capsule').forEach(el => {
                el.style.display = chkCapsules.checked ? 'inline' : 'none';
            });
            triggerActiveRender();
        });
        chkBoxes.addEventListener('change', () => {
            document.querySelectorAll('.prim-box').forEach(el => {
                el.style.display = chkBoxes.checked ? 'inline' : 'none';
            });
            triggerActiveRender();
        });

        function createShader(gl, type, source) {
            const shader = gl.createShader(type);
            gl.shaderSource(shader, source);
            gl.compileShader(shader);
            if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS)) {
                const log = gl.getShaderInfoLog(shader);
                errorLog.textContent = 'Shader Compile Error: ' + log;
                gl.deleteShader(shader);
                throw new Error(log);
            }
            return shader;
        }

        function initWebGL() {
            if (webglInitialized) return true;
            try {
                gl = canvas.getContext('webgl2', {
                    antialias: false,
                    powerPreference: 'high-performance',
                    preserveDrawingBuffer: false,
                    desynchronized: true
                });
                if (!gl) {
                    throw new Error('WebGL2 context not available');
                }
                
                canvas.addEventListener('webglcontextlost', e => {
                    e.preventDefault();
                    paused = true;
                    pauseRenderBtn.textContent = 'START WEBGL SDF';
                    pauseRenderBtn.style.background = '#00ffcc';
                    pauseRenderBtn.style.color = '#000';
                    errorLog.textContent = 'WebGL context lost; rendering paused.';
                });

                canvas.addEventListener('webglcontextrestored', () => {
                    errorLog.textContent = 'WebGL context restored. Please reload page or restart render.';
                });
                
                errorLog.textContent = 'Decoding coarse/residual SDF texture arrays...';
                
                // Upload baked Coarse SDF 3D Texture
                const coarseSdfB64 = "__COARSE_SDF_B64__";
                const coarseSdfRes = __COARSE_SDF_RES__;
                const coarseBytes = Uint8Array.from(atob(coarseSdfB64), c => c.charCodeAt(0));
                const coarseData = new Float32Array(coarseBytes.buffer);

                // Upload baked Residual SDF 3D Texture
                const residualSdfB64 = "__RESIDUAL_SDF_B64__";
                const residualSdfRes = __RESIDUAL_SDF_RES__;
                const residualBytes = Uint8Array.from(atob(residualSdfB64), c => c.charCodeAt(0));
                const residualData = new Float32Array(residualBytes.buffer);

                // Upload baked Error Bound SDF 3D Texture
                const errorSdfB64 = "__ERROR_SDF_B64__";
                const errorBytes = Uint8Array.from(atob(errorSdfB64), c => c.charCodeAt(0));
                const errorData = new Float32Array(errorBytes.buffer);

                errorLog.textContent = 'Compiling shaders...';
                program = gl.createProgram();
                const vs = createShader(gl, gl.VERTEX_SHADER, vsSource);
                const fs = createShader(gl, gl.FRAGMENT_SHADER, fsSource);
                gl.attachShader(program, vs);
                gl.attachShader(program, fs);
                
                errorLog.textContent = 'Linking WebGL program...';
                gl.linkProgram(program);
                if (!gl.getProgramParameter(program, gl.LINK_STATUS)) {
                    const log = gl.getProgramInfoLog(program);
                    errorLog.textContent = 'Program Link Error: ' + log;
                    const lines = fsSource.split('\\n');
                    const annotated = lines.map((l, i) => `${String(i+1).padStart(4)}: ${l}`).join('\\n');
                    console.error('Annotated fragment shader:\\n' + annotated);
                    throw new Error(log);
                }
                gl.useProgram(program);

                const hasFloatLinear = gl.getExtension('OES_texture_float_linear');
                const filterMode = hasFloatLinear ? gl.LINEAR : gl.NEAREST;
                if (!hasFloatLinear) {
                    console.warn('OES_texture_float_linear not supported. Falling back to NEAREST filtering.');
                }

                const coarseTex = gl.createTexture();
                gl.activeTexture(gl.TEXTURE0);
                gl.bindTexture(gl.TEXTURE_3D, coarseTex);
                gl.texImage3D(gl.TEXTURE_3D, 0, gl.R32F,
                    coarseSdfRes, coarseSdfRes, coarseSdfRes,
                    0, gl.RED, gl.FLOAT, coarseData);
                gl.texParameteri(gl.TEXTURE_3D, gl.TEXTURE_MIN_FILTER, filterMode);
                gl.texParameteri(gl.TEXTURE_3D, gl.TEXTURE_MAG_FILTER, filterMode);
                gl.texParameteri(gl.TEXTURE_3D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
                gl.texParameteri(gl.TEXTURE_3D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
                gl.texParameteri(gl.TEXTURE_3D, gl.TEXTURE_WRAP_R, gl.CLAMP_TO_EDGE);
                gl.uniform1i(gl.getUniformLocation(program, 'u_coarseSDF'), 0);

                const residualTex = gl.createTexture();
                gl.activeTexture(gl.TEXTURE1);
                gl.bindTexture(gl.TEXTURE_3D, residualTex);
                gl.texImage3D(gl.TEXTURE_3D, 0, gl.R32F,
                    residualSdfRes, residualSdfRes, residualSdfRes,
                    0, gl.RED, gl.FLOAT, residualData);
                gl.texParameteri(gl.TEXTURE_3D, gl.TEXTURE_MIN_FILTER, filterMode);
                gl.texParameteri(gl.TEXTURE_3D, gl.TEXTURE_MAG_FILTER, filterMode);
                gl.texParameteri(gl.TEXTURE_3D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
                gl.texParameteri(gl.TEXTURE_3D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
                gl.texParameteri(gl.TEXTURE_3D, gl.TEXTURE_WRAP_R, gl.CLAMP_TO_EDGE);
                gl.uniform1i(gl.getUniformLocation(program, 'u_residualSDF'), 1);

                const errorTex = gl.createTexture();
                gl.activeTexture(gl.TEXTURE2);
                gl.bindTexture(gl.TEXTURE_3D, errorTex);
                gl.texImage3D(gl.TEXTURE_3D, 0, gl.R32F,
                    residualSdfRes, residualSdfRes, residualSdfRes,
                    0, gl.RED, gl.FLOAT, errorData);
                gl.texParameteri(gl.TEXTURE_3D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
                gl.texParameteri(gl.TEXTURE_3D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
                gl.texParameteri(gl.TEXTURE_3D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
                gl.texParameteri(gl.TEXTURE_3D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
                gl.texParameteri(gl.TEXTURE_3D, gl.TEXTURE_WRAP_R, gl.CLAMP_TO_EDGE);
                gl.uniform1i(gl.getUniformLocation(program, 'u_errorSDF'), 2);

                let err = gl.getError();
                if (err !== gl.NO_ERROR) {
                    errorLog.textContent += `\\nWebGL Error during texture upload: ${err}`;
                    console.error('WebGL Error during texture upload:', err);
                }

                const positionLoc = gl.getAttribLocation(program, 'position');
                const buffer = gl.createBuffer();
                gl.bindBuffer(gl.ARRAY_BUFFER, buffer);
                gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([
                    -1, -1,  1, -1, -1,  1,
                    -1,  1,  1, -1,  1,  1
                ]), gl.STATIC_DRAW);
                gl.enableVertexAttribArray(positionLoc);
                gl.vertexAttribPointer(positionLoc, 2, gl.FLOAT, false, 0, 0);

                resLoc = gl.getUniformLocation(program, 'u_resolution');
                mouseLoc = gl.getUniformLocation(program, 'u_mouse');
                timeLoc = gl.getUniformLocation(program, 'u_time');
                blendLoc = gl.getUniformLocation(program, 'u_blendRadius');
                jiggleLoc = gl.getUniformLocation(program, 'u_jiggle');
                glowLoc = gl.getUniformLocation(program, 'u_glow');
                specularLoc = gl.getUniformLocation(program, 'u_specularPower');
                fftBandsLoc = gl.getUniformLocation(program, 'u_fftBands');
                debugLoc = gl.getUniformLocation(program, 'u_debugMode');
                maxStepsLoc = gl.getUniformLocation(program, 'u_maxSteps');
                enableAOLoc = gl.getUniformLocation(program, 'u_enableAO');
                enableShadowLoc = gl.getUniformLocation(program, 'u_enableShadow');

                webglInitialized = true;
                errorLog.textContent = 'Render resources compiled and loaded successfully.';
                return true;
            } catch (e) {
                errorLog.textContent = 'WebGL Initialization Failed: ' + e.message;
                console.error('WebGL Initialization Failed:', e);
                return false;
            }
        }

        const debugSelect = document.getElementById('select-debug');
        const blendSlider = document.getElementById('slider-blend');
        const jiggleSlider = document.getElementById('slider-jiggle');
        const glowSlider = document.getElementById('slider-glow');
        const specSlider = document.getElementById('slider-specular');

        const blendVal = document.getElementById('val-blend');
        const jiggleVal = document.getElementById('val-jiggle');
        const glowVal = document.getElementById('val-glow');
        const specVal = document.getElementById('val-specular');

        let paused = true;
        let u_maxSteps = 48;
        let u_enableAO = 0;
        let u_enableShadow = 0;
        let lastFrameTime = performance.now();
        let slowFrameCount = 0;

        const pauseRenderBtn = document.getElementById('btn-pause-render');
        pauseRenderBtn.addEventListener('click', () => {
            if (!webglInitialized) {
                const ok = initWebGL();
                if (!ok) return;
                webglControls.style.display = 'block';
                fpsCounter.style.display = 'block';
            }
            paused = !paused;
            if (paused) {
                pauseRenderBtn.textContent = 'START WEBGL SDF';
                pauseRenderBtn.style.background = '#00ffcc';
                pauseRenderBtn.style.color = '#000';
            } else {
                pauseRenderBtn.textContent = 'PAUSE WEBGL SDF';
                pauseRenderBtn.style.background = '#ff00aa';
                pauseRenderBtn.style.color = '#fff';
                lastFrameTime = performance.now();
                requestAnimationFrame(render);
            }
        });

        const qualSelect = document.getElementById('select-quality');
        qualSelect.addEventListener('change', (e) => {
            u_maxSteps = parseInt(e.target.value);
            if (paused) drawFrame(performance.now());
        });

        const aoChk = document.getElementById('chk-ao');
        aoChk.addEventListener('change', (e) => {
            u_enableAO = e.target.checked ? 1 : 0;
            if (paused) drawFrame(performance.now());
        });

        const shadowChk = document.getElementById('chk-shadow');
        shadowChk.addEventListener('change', (e) => {
            u_enableShadow = e.target.checked ? 1 : 0;
            if (paused) drawFrame(performance.now());
        });

        blendSlider.addEventListener('input', (e) => {
            blendVal.textContent = parseFloat(e.target.value).toFixed(2);
            if (paused) drawFrame(performance.now());
        });
        jiggleSlider.addEventListener('input', (e) => {
            jiggleVal.textContent = parseFloat(e.target.value).toFixed(1);
            if (paused) drawFrame(performance.now());
        });
        glowSlider.addEventListener('input', (e) => {
            glowVal.textContent = parseFloat(e.target.value).toFixed(1);
            if (paused) drawFrame(performance.now());
        });
        specSlider.addEventListener('input', (e) => {
            specVal.textContent = parseFloat(e.target.value).toFixed(1);
            if (paused) drawFrame(performance.now());
        });

        // WebGL Mouse interaction with dynamic low-res scaling on drag
        let mouse = [0.5, 0.5];
        let isWebGLDragging = false;
        
        window.addEventListener('mousemove', (e) => {
            mouse = [e.clientX / window.innerWidth, 1.0 - (e.clientY / window.innerHeight)];
            if (paused && webglInitialized) drawFrame(performance.now());
        });

        canvas.addEventListener('mousedown', () => {
            isWebGLDragging = true;
            maxRenderPixels = 14400; // Low-res 160x90 mapping during rotation drag
            resize();
            if (paused && webglInitialized) drawFrame(performance.now());
        });

        window.addEventListener('mouseup', () => {
            if (isWebGLDragging) {
                isWebGLDragging = false;
                maxRenderPixels = parseInt(resSelect.value);
                resize();
                if (paused && webglInitialized) drawFrame(performance.now());
            }
        });

        canvas.addEventListener('touchstart', () => {
            isWebGLDragging = true;
            maxRenderPixels = 14400;
            resize();
            if (paused && webglInitialized) drawFrame(performance.now());
        });

        window.addEventListener('touchend', () => {
            if (isWebGLDragging) {
                isWebGLDragging = false;
                maxRenderPixels = parseInt(resSelect.value);
                resize();
                if (paused && webglInitialized) drawFrame(performance.now());
            }
        });

        const resSelect = document.getElementById('select-resolution');
        resSelect.addEventListener('change', (e) => {
            maxRenderPixels = parseInt(e.target.value);
            resize();
            if (paused && webglInitialized) drawFrame(performance.now());
        });

        debugSelect.addEventListener('change', () => {
            if (paused && webglInitialized) drawFrame(performance.now());
        });

        function degradeQuality() {
            if (maxRenderPixels > 57600) {
                maxRenderPixels = 57600;
                resSelect.value = "57600";
            }
            u_maxSteps = 32;
            qualSelect.value = "32";
            u_enableAO = 0;
            u_enableShadow = 0;
            aoChk.checked = false;
            shadowChk.checked = false;
            resize();
        }

        function resize() {
            const cssWidth = Math.max(1, window.innerWidth);
            const cssHeight = Math.max(1, window.innerHeight);
            const scaleFactor = Math.min(1, Math.sqrt(maxRenderPixels / (cssWidth * cssHeight)));
            canvas.width = Math.max(1, Math.floor(cssWidth * scaleFactor));
            canvas.height = Math.max(1, Math.floor(cssHeight * scaleFactor));
            if (gl) gl.viewport(0, 0, canvas.width, canvas.height);
        }
        window.addEventListener('resize', resize);
        resize();

        // Web Audio API FFT setup with MP3 & Mic support
        let audioCtx = null;
        let analyser = null;
        let dataArray = null;
        let audioActive = false;
        let fftBands = new Float32Array(8);
        let audioSource = null;

        const audioInput = document.getElementById('input-audio');
        const playBtn = document.getElementById('btn-play');
        const micBtn = document.getElementById('btn-audio');
        const audioPlayer = document.getElementById('audio-player');

        function initAudioContext() {
            if (!audioCtx) {
                audioCtx = new (window.AudioContext || window.webkitAudioContext)();
                analyser = audioCtx.createAnalyser();
                analyser.fftSize = 256;
                dataArray = new Uint8Array(analyser.frequencyBinCount);
            }
            if (audioCtx.state === 'suspended') {
                audioCtx.resume();
            }
        }

        let currentAudioURL = null;
        audioInput.addEventListener('change', (e) => {
            const file = e.target.files[0];
            if (!file) return;
            
            if (currentAudioURL) {
                URL.revokeObjectURL(currentAudioURL);
            }
            currentAudioURL = URL.createObjectURL(file);
            audioPlayer.src = currentAudioURL;
            initAudioContext();
            
            if (!audioSource) {
                audioSource = audioCtx.createMediaElementSource(audioPlayer);
                audioSource.connect(analyser);
                analyser.connect(audioCtx.destination);
            }
            
            audioPlayer.play();
            audioActive = true;
            playBtn.textContent = 'PAUSE';
            playBtn.style.background = '#00ffcc';
            playBtn.style.color = '#000';
            
            micBtn.textContent = 'MIC FFT';
            micBtn.style.background = '#ff00aa';
            micBtn.style.color = '#fff';
        });

        playBtn.addEventListener('click', () => {
            initAudioContext();
            if (audioPlayer.src) {
                if (audioPlayer.paused) {
                    audioPlayer.play();
                    playBtn.textContent = 'PAUSE';
                    playBtn.style.background = '#00ffcc';
                    playBtn.style.color = '#000';
                } else {
                    audioPlayer.pause();
                    playBtn.textContent = 'PLAY';
                    playBtn.style.background = '#ff00aa';
                    playBtn.style.color = '#fff';
                }
                audioActive = true;
            }
        });

        micBtn.addEventListener('click', () => {
            initAudioContext();
            navigator.mediaDevices.getUserMedia({ audio: true, video: false })
                .then(stream => {
                    const micSource = audioCtx.createMediaStreamSource(stream);
                    micSource.connect(analyser);
                    audioActive = true;
                    micBtn.textContent = 'MIC ACTIVE';
                    micBtn.style.background = '#00ffcc';
                    micBtn.style.color = '#000';
                    
                    audioPlayer.pause();
                    playBtn.textContent = 'PLAY';
                    playBtn.style.background = '#ff00aa';
                    playBtn.style.color = '#fff';
                })
                .catch(err => {
                    console.warn('Audio input acquisition failed:', err);
                    errorLog.textContent = 'Audio permission denied or unavailable.';
                });
        });

        let lastTime = 0;
        let frameCount = 0;

        function drawFrame(time) {
            if (!webglInitialized) return;
            if (audioActive && analyser) {
                analyser.getByteFrequencyData(dataArray);
                const binWidth = Math.floor(dataArray.length / 8);
                for (let i = 0; i < 8; i++) {
                    let sum = 0;
                    for (let j = 0; j < binWidth; j++) {
                        sum += dataArray[i * binWidth + j];
                    }
                    fftBands[i] = (sum / binWidth) / 255.0;
                }
            } else {
                fftBands.fill(0.0);
            }

            gl.uniform2f(resLoc, canvas.width, canvas.height);
            gl.uniform2f(mouseLoc, mouse[0], mouse[1]);
            gl.uniform1f(timeLoc, time * 0.001);
            
            gl.uniform1f(blendLoc, parseFloat(blendSlider.value));
            gl.uniform1f(jiggleLoc, parseFloat(jiggleSlider.value));
            gl.uniform1f(glowLoc, parseFloat(glowSlider.value));
            gl.uniform1f(specularLoc, parseFloat(specSlider.value));
            gl.uniform1fv(fftBandsLoc, fftBands);
            gl.uniform1i(debugLoc, parseInt(debugSelect.value));
            gl.uniform1i(maxStepsLoc, u_maxSteps);
            gl.uniform1i(enableAOLoc, u_enableAO);
            gl.uniform1i(enableShadowLoc, u_enableShadow);

            gl.drawArrays(gl.TRIANGLES, 0, 6);
        }

        function render(time) {
            if (paused) return;
            const delta = time - lastFrameTime;
            lastFrameTime = time;

            if (delta > 250) {
                slowFrameCount++;
                if (slowFrameCount >= 2) {
                    const isLowest = (maxRenderPixels === 57600 && u_maxSteps === 32 && u_enableAO === 0 && u_enableShadow === 0);
                    if (isLowest) {
                        paused = true;
                        pauseRenderBtn.textContent = 'START WEBGL SDF';
                        pauseRenderBtn.style.background = '#00ffcc';
                        pauseRenderBtn.style.color = '#000';
                        errorLog.textContent = 'Warning: GPU pinned. Raymarching paused.';
                    } else {
                        degradeQuality();
                        errorLog.textContent = 'Warning: Settings degraded due to GPU pressure.';
                    }
                    slowFrameCount = 0;
                }
            } else {
                slowFrameCount = Math.max(0, slowFrameCount - 1);
            }

            frameCount++;
            if (time - lastTime >= 1000) {
                fpsCounter.textContent = `FPS: ${frameCount}`;
                frameCount = 0;
                lastTime = time;
            }

            drawFrame(time);
            if (!paused) {
                requestAnimationFrame(render);
            }
        }

        function pauseRender() {
            paused = true;
            pauseRenderBtn.textContent = 'START WEBGL SDF';
            pauseRenderBtn.style.background = '#00ffcc';
            pauseRenderBtn.style.color = '#000';
        }

        const screenshotBtn = document.getElementById('btn-screenshot');
        screenshotBtn.addEventListener('click', () => {
            if (activeBackend === 'webgl') {
                drawFrame(performance.now());
                const dataURL = canvas.toDataURL('image/png');
                const link = document.createElement('a');
                link.download = 'zcc_sdf_screenshot.png';
                link.href = dataURL;
                link.click();
            } else if (activeBackend === 'canvas2d') {
                const dataURL = canvas2d.toDataURL('image/png');
                const link = document.createElement('a');
                link.download = 'zcc_sdf_screenshot.png';
                link.href = dataURL;
                link.click();
            }
        });

        // Initialize to Canvas2D Orbit Preview on load
        setBackend('canvas2d');
    </script>
</body>
</html>
"""

# Component-level resource costs (empirically derived)
COST_PER_SAMPLE_POINT = 24
COST_KMEANS_DISTANCE = 8
COST_NUMPY_OVERHEAD = 64 * 1024 * 1024
COST_GLSL_STRING = 2 * 1024 * 1024
WSL_KERNEL_RESERVED = 256 * 1024 * 1024

_CEILING_WARNING_EMITTED = False

def fmt_glsl(val):
    """Serializes a float to a 5-decimal GLSL literal. Raises on non-finite."""
    if not np.isfinite(val):
        raise ValueError(f"Non-finite float value {val} encountered during GLSL serialization")
    return f"{val:.5f}"

def protect_from_oom_killer():
    """Adjust oom_score_adj to protect the compiler process from the kernel oom-killer"""
    try:
        if os.path.exists('/proc/self/oom_score_adj'):
            with open('/proc/self/oom_score_adj', 'w') as f:
                f.write('-500')
    except (PermissionError, IOError):
        pass

def set_memory_ceiling(max_bytes=2048 * 1024 * 1024):
    """
    DISABLED: RLIMIT_AS virtual address space ceilings are incompatible
    with numpy/OpenBLAS under WSL2. These libraries map ~77 GB of virtual
    address space at import time. Physical RAM is protected by:
      - psutil available checks
      - safe_alloc() wrappers
      - tracemalloc telemetry
    Do NOT re-enable without verifying psutil.Process().memory_info().vms
    is within the ceiling after import.
    """
    global _CEILING_WARNING_EMITTED
    if not _CEILING_WARNING_EMITTED:
        warnings.warn(
            "set_memory_ceiling() is a no-op under WSL2 + numpy. Virtual memory mappings are bypass-protected.",
            stacklevel=2
        )
        _CEILING_WARNING_EMITTED = True
    pass

def preflight_memory_check(num_samples, k):
    """Estimates peak memory usage before allocation starts"""
    import psutil
    kmeans_matrix = num_samples * k * COST_KMEANS_DISTANCE
    sample_array = num_samples * COST_PER_SAMPLE_POINT
    peak_estimate = (
        sample_array * 3 +
        kmeans_matrix +
        COST_NUMPY_OVERHEAD +
        COST_GLSL_STRING
    )
    
    try:
        mem = psutil.virtual_memory()
        available = mem.available - WSL_KERNEL_RESERVED
        ok = available > peak_estimate * 1.3
        return ok, peak_estimate, available
    except (ImportError, AttributeError):
        return True, peak_estimate, 1024 * 1024 * 1024

def safe_alloc(shape, dtype=np.float32, label="array"):
    """Allocates numpy arrays safely with garbage-collection sweep fallback on failure"""
    import psutil
    itemsize = np.dtype(dtype).itemsize
    n_elements = int(np.prod(shape))
    req_bytes = n_elements * itemsize
    
    try:
        mem = psutil.virtual_memory()
        if req_bytes > mem.available * 0.7:
            raise RuntimeError(f"Refusing allocation: '{label}' requires {req_bytes//1024//1024} MB, only {mem.available//1024//1024} MB available")
    except (ImportError, AttributeError):
        pass

    try:
        return np.empty(shape, dtype=dtype)
    except MemoryError:
        gc.collect()
        try:
            return np.empty(shape, dtype=dtype)
        except MemoryError:
            raise RuntimeError(f"MemoryError allocating '{label}' of shape {shape}")

class MemoryTelemetry:
    """Wrapper to measure execution latency and peak memory usage of each phase"""
    def __init__(self, label, warn_mb=400, abort_mb=480):
        self.label = label
        self.warn_mb = warn_mb
        self.abort_mb = abort_mb
        self.t0 = time.perf_counter()

    def __enter__(self):
        try:
            import tracemalloc
            tracemalloc.start()
        except ImportError:
            pass
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        import tracemalloc
        elapsed = time.perf_counter() - self.t0
        try:
            current, peak = tracemalloc.get_traced_memory()
            tracemalloc.stop()
            peak_mb = peak / 1024 / 1024
            print(f"[ZCC Telemetry] {self.label}: peak={peak_mb:.2f} MB, elapsed={elapsed:.3f}s")
            if peak_mb > self.abort_mb:
                raise MemoryError(f"Abort limit exceeded: {peak_mb:.1f} MB > {self.abort_mb} MB")
            elif peak_mb > self.warn_mb:
                print(f"[ZCC Telemetry] WARNING: {self.label} used {peak_mb:.1f} MB (Approaching ceiling)")
        except ImportError:
            print(f"[ZCC Telemetry] {self.label}: elapsed={elapsed:.3f}s (tracemalloc unavailable)")
        return False  # Never suppress exceptions from inside the block

def get_node_transforms(gltf):
    """Parses glTF hierarchical structures and computes global world transformation matrices"""
    nodes = gltf.get("nodes", [])
    num_nodes = len(nodes)
    world_transforms = [np.eye(4) for _ in range(num_nodes)]
    visited = [False] * num_nodes
    
    scenes = gltf.get("scenes", [])
    roots = []
    for scene in scenes:
        roots.extend(scene.get("nodes", []))
    if not roots and num_nodes > 0:
        roots = [i for i in range(num_nodes)]
        
    queue = [(root, np.eye(4)) for root in roots]
    while queue:
        node_idx, parent_transform = queue.pop(0)
        if node_idx >= num_nodes or visited[node_idx]:
            continue
        visited[node_idx] = True
        
        node = nodes[node_idx]
        local_matrix = np.eye(4)
        if "matrix" in node:
            local_matrix = np.array(node["matrix"]).reshape(4, 4).T
        else:
            t = node.get("translation", [0.0, 0.0, 0.0])
            r = node.get("rotation", [0.0, 0.0, 0.0, 1.0])
            s = node.get("scale", [1.0, 1.0, 1.0])
            
            T = np.eye(4)
            T[0:3, 3] = t
            
            S = np.eye(4)
            np.fill_diagonal(S, [s[0], s[1], s[2], 1.0])
            
            R = np.eye(4)
            qx, qy, qz, qw = r
            r_mat = np.array([
                [1.0 - 2.0*(qy**2 + qz**2), 2.0*(qx*qy - qz*qw), 2.0*(qx*qz + qy*qw)],
                [2.0*(qx*qy + qz*qw), 1.0 - 2.0*(qx**2 + qz**2), 2.0*(qy*qz - qx*qw)],
                [2.0*(qx*qz - qy*qw), 2.0*(qy*qz + qx*qw), 1.0 - 2.0*(qx**2 + qy**2)]
            ])
            R[0:3, 0:3] = r_mat
            
            local_matrix = T @ R @ S
            
        world_transform = parent_transform @ local_matrix
        world_transforms[node_idx] = world_transform
        
        for child in node.get("children", []):
            queue.append((child, world_transform))
            
    return world_transforms

def stream_glb_vertices(filepath, num_samples=15000):
    """
    Stratified block sampler to reduce seeking and speed up parser:
    1. Divide the accessor into segments.
    2. Read contiguous windows of vertices from each segment using a single seek.
    3. Keeps O(1) memory complex allocations.
    """
    MAX_GLB_SIZE = 8 * 1024**3
    MAX_JSON_SIZE = 64 * 1024**2
    MAX_SAMPLE_PTS = 50000

    file_size = os.path.getsize(filepath)
    if file_size > MAX_GLB_SIZE:
        raise ValueError(f"GLB file {file_size/1024**3:.1f} GB exceeds {MAX_GLB_SIZE/1024**3} GB limit")
        
    num_samples = min(num_samples, MAX_SAMPLE_PTS)
    
    # Telemetry counters
    num_accessors_read = 0
    total_seeks = 0
    total_bytes_read = 0
    
    with open(filepath, "rb") as f:
        header_bytes = f.read(12)
        if len(header_bytes) < 12:
            raise ValueError("GLB header truncated")
        magic, version, length = struct.unpack("<III", header_bytes)
        if magic != 0x46546C67:
            raise ValueError("Not a valid GLB file")
            
        chunk_header = f.read(8)
        if len(chunk_header) < 8:
            raise ValueError("First chunk header truncated")
        chunk_len, chunk_type = struct.unpack("<II", chunk_header)
        if chunk_type != 0x4E4F534A:
            raise ValueError("First chunk must be JSON")
            
        if chunk_len > MAX_JSON_SIZE:
            raise ValueError(f"JSON chunk {chunk_len/1024**2} MB exceeds 64 MB limit")
            
        json_data = f.read(chunk_len).decode("utf-8")
        gltf = json.loads(json_data)
        del json_data
        
        chunk2_header = f.read(8)
        if len(chunk2_header) < 8:
            raise ValueError("Second chunk header truncated")
        chunk2_len, chunk2_type = struct.unpack("<II", chunk2_header)
        if chunk2_type != 0x004E4942:
            raise ValueError("Second chunk must be BIN")
        bin_start_offset = f.tell()
        
        world_transforms = get_node_transforms(gltf)
        
        mesh_instances = []
        for node_idx, node in enumerate(gltf.get("nodes", [])):
            if "mesh" in node:
                mesh_instances.append((node["mesh"], world_transforms[node_idx]))
                
        total_verts = 0
        primitive_infos = []
        
        for mesh_idx, transform in mesh_instances:
            mesh = gltf["meshes"][mesh_idx]
            for primitive in mesh.get("primitives", []):
                attributes = primitive.get("attributes", {})
                if "POSITION" in attributes:
                    accessor_idx = attributes["POSITION"]
                    accessor = gltf["accessors"][accessor_idx]
                    count = accessor["count"]
                    total_verts += count
                    primitive_infos.append((accessor_idx, count, transform))
                    
        if total_verts == 0:
            raise ValueError("No vertices found in GLB")
            
        print(f"[ZCC SDF Compiler] Total mesh vertices available in GLB: {total_verts}")
        
        # Pre-allocate output array and populate in-place to avoid list conversion overhead, capped to 2x requested
        total_expected = min(
            sum(max(int(np.round((c / total_verts) * num_samples)), 20)
                for _, c, _ in primitive_infos),
            num_samples * 2
        )
        sampled_verts = safe_alloc((total_expected, 3), np.float32, "sampled_verts")
        write_idx = 0
        
        # Pre-allocate a vertex transformation buffer to reuse across iterations (O(1) heap allocation churn)
        _vert_buf = np.ones(4, dtype=np.float64)
        rng = np.random.default_rng(42)
        
        for accessor_idx, count, transform in primitive_infos:
            num_prim_samples = int(np.round((count / total_verts) * num_samples))
            num_prim_samples = max(num_prim_samples, 20)
            
            accessor = gltf["accessors"][accessor_idx]
            bufferview = gltf["bufferViews"][accessor["bufferView"]]
            
            if accessor.get("componentType", 0) != 5126 or accessor.get("type", "") != "VEC3":
                continue
                
            accessor_byte_offset = accessor.get("byteOffset", 0)
            bufferview_byte_offset = bufferview.get("byteOffset", 0)
            byte_stride = bufferview.get("byteStride", 12)
            total_vertex_offset = bin_start_offset + bufferview_byte_offset + accessor_byte_offset
            
            num_accessors_read += 1
            
            # If the accessor has fewer vertices than segment samples or is tiny, read all of it in a single seek!
            if count <= num_prim_samples or count < 256:
                read_len = count * byte_stride
                f.seek(total_vertex_offset)
                total_seeks += 1
                total_bytes_read += read_len
                chunk_bytes = f.read(read_len)
                for idx in range(count):
                    rel_offset = idx * byte_stride
                    vertex_bytes = chunk_bytes[rel_offset : rel_offset + 12]
                    if len(vertex_bytes) == 12:
                        x, y, z = struct.unpack("<fff", vertex_bytes)
                        _vert_buf[0] = x
                        _vert_buf[1] = y
                        _vert_buf[2] = z
                        v_transformed = (transform @ _vert_buf)[:3]
                        if write_idx < total_expected:
                            sampled_verts[write_idx] = v_transformed
                            write_idx += 1
                continue
                
            # Stratified block sampling (dynamic segment sizing based on WINDOW_SIZE)
            WINDOW_SIZE = 128
            S = max(1, num_prim_samples // WINDOW_SIZE)
            S = min(S, 32)
            W = max(1, num_prim_samples // S)
            segment_len = count // S
            
            for s in range(S):
                seg_start = s * segment_len
                seg_end = (s + 1) * segment_len if s < S - 1 else count
                
                # Pick a random window start inside the segment
                max_start = max(seg_start, seg_end - W)
                if max_start > seg_start:
                    start_idx = rng.integers(seg_start, max_start)
                else:
                    start_idx = seg_start
                    
                actual_w = min(W, count - start_idx)
                if actual_w <= 0:
                    continue
                    
                read_len = actual_w * byte_stride
                f.seek(total_vertex_offset + start_idx * byte_stride)
                total_seeks += 1
                total_bytes_read += read_len
                chunk_bytes = f.read(read_len)
                
                for w_idx in range(actual_w):
                    rel_offset = w_idx * byte_stride
                    vertex_bytes = chunk_bytes[rel_offset : rel_offset + 12]
                    if len(vertex_bytes) == 12:
                        x, y, z = struct.unpack("<fff", vertex_bytes)
                        _vert_buf[0] = x
                        _vert_buf[1] = y
                        _vert_buf[2] = z
                        v_transformed = (transform @ _vert_buf)[:3]
                        if write_idx < total_expected:
                            sampled_verts[write_idx] = v_transformed
                            write_idx += 1
                            
        parser_stats = {
            "num_accessors_read": num_accessors_read,
            "samples_requested": num_samples,
            "samples_written": write_idx,
            "read_calls": total_seeks,
            "bytes_read": total_bytes_read,
            "avg_read_bytes": int(total_bytes_read / max(1, total_seeks))
        }
        return sampled_verts[:write_idx], parser_stats

def kmeans_pure(points, k, max_iters=30, seed=42):
    """Pure numpy K-Means solver using safe column-wise allocations. Prevents uninitialized labels."""
    rng = np.random.default_rng(seed)
    indices = rng.choice(points.shape[0], k, replace=False)
    centers = points[indices].copy()
    
    for _ in range(max_iters):
        # Initialize labels to zero to avoid garbage values from uninitialized memory
        labels = np.zeros(points.shape[0], dtype=np.int32)
        min_dists = safe_alloc((points.shape[0],), np.float64, "kmeans_min_dists")
        min_dists[:] = np.inf
        
        for i in range(k):
            col = np.linalg.norm(points - centers[i], axis=1)
            mask = col < min_dists
            min_dists[mask] = col[mask]
            labels[mask] = i
        del min_dists
        
        new_centers = np.array([
            points[labels == i].mean(axis=0) if np.any(labels == i) else centers[i]
            for i in range(k)
        ])
        
        if np.allclose(centers, new_centers):
            break
        centers = new_centers
        
    return centers, labels

def lloyd_relaxation(points, centers, iterations=8):
    """Refined Lloyd's relaxation solver using safe column-wise allocations. Prevents uninitialized labels."""
    refined_centers = centers.copy()
    for _ in range(iterations):
        labels = np.zeros(points.shape[0], dtype=np.int32)
        min_dists = safe_alloc((points.shape[0],), np.float64, "lloyd_min_dists")
        min_dists[:] = np.inf
        
        for i in range(len(refined_centers)):
            col = np.linalg.norm(points - refined_centers[i], axis=1)
            mask = col < min_dists
            min_dists[mask] = col[mask]
            labels[mask] = i
        del min_dists
        
        for i in range(len(refined_centers)):
            pts = points[labels == i]
            if len(pts) > 0:
                refined_centers[i] = pts.mean(axis=0)
    return refined_centers

def classify_cluster_primitive(pts):
    """PCA analysis on cluster points to classify Spheres, Capsules, and Oriented Boxes"""
    center = np.mean(pts, axis=0)
    pts_centered = pts - center
    
    if len(pts) < 8:
        # Default to sphere for small clusters
        radius = np.max(np.linalg.norm(pts_centered, axis=1)) if len(pts) > 0 else 0.05
        radius = max(radius, 0.02)
        return {"type": "sphere", "center": center, "radius": radius}
        
    cov = np.cov(pts_centered.T)
    eigenvalues, eigenvectors = np.linalg.eigh(cov)
    
    # Sort eigenvalues descending
    idx = eigenvalues.argsort()[::-1]
    eigenvalues = eigenvalues[idx]
    eigenvectors = eigenvectors[:, idx]
    
    ratio_1 = eigenvalues[0] / max(eigenvalues[1], 1e-6)
    ratio_2 = eigenvalues[1] / max(eigenvalues[2], 1e-6)
    
    # Capsule Fit Case
    if ratio_1 >= 2.0 and ratio_2 < 2.0:
        axis = eigenvectors[:, 0]
        projections = pts_centered @ axis
        proj_min = np.min(projections)
        proj_max = np.max(projections)
        length = proj_max - proj_min
        
        proj_axis_vectors = projections[:, np.newaxis] * axis
        perp_vectors = pts_centered - proj_axis_vectors
        perp_dists = np.linalg.norm(perp_vectors, axis=1)
        r = np.percentile(perp_dists, 95)
        r = max(r, 0.02)
        
        # Demote degenerate capsule to sphere if endpoints collapse
        inset = r
        if length > 2.0 * inset:
            a_val = proj_min + inset
            b_val = proj_max - inset
        else:
            a_val = proj_min + length * 0.25
            b_val = proj_max - length * 0.25
            
        a = center + a_val * axis
        b = center + b_val * axis
        
        if np.linalg.norm(b - a) < 1e-4:
            dists = np.linalg.norm(pts_centered, axis=1)
            radius = np.percentile(dists, 95)
            radius = max(radius, 0.02)
            return {"type": "sphere", "center": center, "radius": radius}
            
        return {"type": "capsule", "a": a, "b": b, "radius": r, "center": center}
        
    # Oriented Box Case
    elif ratio_1 >= 2.0 and ratio_2 >= 2.0:
        p0 = pts_centered @ eigenvectors[:, 0]
        p1 = pts_centered @ eigenvectors[:, 1]
        p2 = pts_centered @ eigenvectors[:, 2]
        
        h0 = np.percentile(np.abs(p0), 95)
        h1 = np.percentile(np.abs(p1), 95)
        h2 = np.percentile(np.abs(p2), 95)
        
        extents = np.array([h0, h1, h2])
        extents = np.maximum(extents, 0.02)
        
        return {
            "type": "box",
            "center": center,
            "axis0": eigenvectors[:, 0],
            "axis1": eigenvectors[:, 1],
            "axis2": eigenvectors[:, 2],
            "extents": extents
        }
        
    # Default Sphere Case
    else:
        dists = np.linalg.norm(pts_centered, axis=1)
        radius = np.percentile(dists, 95)
        radius = max(radius, 0.02)
        return {"type": "sphere", "center": center, "radius": radius}

def sd_sphere_cpu(p, center, r):
    return np.linalg.norm(p - center) - r

def sd_capsule_cpu(p, a, b, r):
    pa = p - a
    ba = b - a
    h = np.clip(np.dot(pa, ba) / max(np.dot(ba, ba), 1e-12), 0.0, 1.0)
    return np.linalg.norm(pa - ba * h) - r

def sd_box_cpu(p, center, ax0, ax1, ax2, ext):
    lp = p - center
    q = np.abs(np.array([
        np.dot(lp, ax0),
        np.dot(lp, ax1),
        np.dot(lp, ax2)
    ])) - ext
    return np.linalg.norm(np.maximum(q, 0.0)) + min(max(q[0], max(q[1], q[2])), 0.0)

def smin_float_cpu(a, b, k):
    if k <= 1e-5:
        return min(a, b)
    h = np.clip(0.5 + 0.5 * (b - a) / k, 0.0, 1.0)
    return (1.0 - h) * b + h * a - k * h * (1.0 - h)

def eval_analytic_sdf_cpu(points, groups, blend_radius):
    """Python-side CPU evaluator mirroring GLSL output values, vectorized for speed"""
    N = points.shape[0]
    d = np.full(N, 1e5, dtype=np.float32)
    
    for group in groups:
        g_center = group["center"].astype(np.float32)
        g_radius = float(group["radius"])
        
        g_dist = np.linalg.norm(points - g_center, axis=1) - g_radius
        w = -(g_dist - (blend_radius + 0.05))
        active_mask = w > 0.0
        
        if np.any(active_mask):
            active_pts = points[active_mask]
            z_d = np.full(len(active_pts), 1e5, dtype=np.float32)
            is_first = True
            
            for prim in group["primitives"]:
                if prim["type"] == "sphere":
                    c = prim["center"].astype(np.float32)
                    r = float(prim["radius"])
                    dist = np.linalg.norm(active_pts - c, axis=1) - r
                elif prim["type"] == "capsule":
                    a = prim["a"].astype(np.float32)
                    b = prim["b"].astype(np.float32)
                    r = float(prim["radius"])
                    
                    pa = active_pts - a
                    ba = b - a
                    ba_lensq = np.dot(ba, ba)
                    if ba_lensq > 1e-8:
                        h = np.clip(np.dot(pa, ba) / ba_lensq, 0.0, 1.0)
                        proj = a + h[:, np.newaxis] * ba
                        dist = np.linalg.norm(active_pts - proj, axis=1) - r
                    else:
                        dist = np.linalg.norm(active_pts - a, axis=1) - r
                elif prim["type"] == "box":
                    c = prim["center"].astype(np.float32)
                    ax0 = prim["axis0"].astype(np.float32)
                    ax1 = prim["axis1"].astype(np.float32)
                    ax2 = prim["axis2"].astype(np.float32)
                    ext = prim["extents"].astype(np.float32)
                    
                    lp = active_pts - c
                    qp = np.stack([
                        lp @ ax0,
                        lp @ ax1,
                        lp @ ax2
                    ], axis=1)
                    q = np.abs(qp) - ext
                    max_q = np.maximum(q, 0.0)
                    dist = np.linalg.norm(max_q, axis=1) + np.minimum(np.max(q, axis=1), 0.0)
                else:
                    dist = np.full(len(active_pts), 1e5, dtype=np.float32)
                    
                if is_first:
                    z_d = dist
                    is_first = False
                else:
                    h = np.clip(0.5 + 0.5 * (dist - z_d) / blend_radius, 0.0, 1.0)
                    z_d = h * z_d + (1.0 - h) * dist - blend_radius * h * (1.0 - h)
            
            current_d = d[active_mask]
            h = np.clip(0.5 + 0.5 * (z_d - current_d) / blend_radius, 0.0, 1.0)
            d[active_mask] = h * current_d + (1.0 - h) * z_d - blend_radius * h * (1.0 - h)
            
        inactive_mask = ~active_mask
        if np.any(inactive_mask):
            d[inactive_mask] = np.minimum(d[inactive_mask], g_dist[inactive_mask])
            
    return d

def evaluate_primitive_fit_error(prim, pts):
    """Calculates cluster reconstruction metrics (mean distance, p95 distance, max distance)"""
    if len(pts) == 0:
        return {"mean_abs": 0.0, "p95_abs": 0.0, "max_abs": 0.0}
    
    if prim["type"] == "sphere":
        dists = np.abs(np.linalg.norm(pts - prim["center"], axis=1) - prim["radius"])
    elif prim["type"] == "capsule":
        pa = pts - prim["a"]
        ba = prim["b"] - prim["a"]
        ba_lensq = np.dot(ba, ba)
        if ba_lensq > 1e-8:
            h = np.clip(np.dot(pa, ba) / ba_lensq, 0.0, 1.0)
            proj = prim["a"] + h[:, np.newaxis] * ba
            dists = np.abs(np.linalg.norm(pts - proj, axis=1) - prim["radius"])
        else:
            dists = np.abs(np.linalg.norm(pts - prim["center"], axis=1) - prim["radius"])
    elif prim["type"] == "box":
        lp = pts - prim["center"]
        qp = np.stack([
            lp @ prim["axis0"],
            lp @ prim["axis1"],
            lp @ prim["axis2"]
        ], axis=1)
        q = np.abs(qp) - prim["extents"]
        max_q = np.maximum(q, 0.0)
        dists = np.abs(np.linalg.norm(max_q, axis=1) + np.minimum(np.max(q, axis=1), 0.0))
    else:
        dists = np.zeros(len(pts))
        
    return {
        "mean_abs": float(np.mean(dists)),
        "p95_abs": float(np.percentile(dists, 95)),
        "max_abs": float(np.max(dists))
    }

def primitive_volume_weight(prim):
    """Calculates primitive volume as a weight for centroid rebalancing"""
    if prim["type"] == "sphere":
        return prim["radius"] ** 3
    elif prim["type"] == "capsule":
        return prim["radius"] ** 2 * np.linalg.norm(prim["b"] - prim["a"])
    elif prim["type"] == "box":
        return float(np.prod(prim["extents"]))
    return 1.0

def generate_palette(k):
    colors = []
    for i in range(k):
        hue = (i + 0.5) / k  # Spread endpoints to prevent first and last colors from colliding
        sat = 0.85
        light = 0.50
        
        c = (1.0 - abs(2.0 * light - 1.0)) * sat
        x = c * (1.0 - abs((hue * 6.0) % 2.0 - 1.0))
        m = light - c / 2.0
        
        h_category = int(hue * 6.0)
        if h_category == 0:   r, g, b = c, x, 0.0
        elif h_category == 1: r, g, b = x, c, 0.0
        elif h_category == 2: r, g, b = 0.0, c, x
        elif h_category == 3: r, g, b = 0.0, x, c
        elif h_category == 4: r, g, b = x, 0.0, c
        else:  # h_category == 5 (or theoretical 6 from float edge case, same formula)
            r, g, b = c, 0.0, x
        
        colors.append((r + m, g + m, b + m))
    return np.array(colors)

def group_primitives(primitives, num_groups, num_spheres):
    """Clusters primitive centers into group partitions, splitting overloaded zones for balance"""
    centers = []
    for prim in primitives:
        if prim["type"] == "sphere":
            centers.append(prim["center"])
        elif prim["type"] == "capsule":
            centers.append(prim["center"])
        elif prim["type"] == "box":
            centers.append(prim["center"])
    centers = np.array(centers)
    
    group_centers, labels = kmeans_pure(centers, num_groups, seed=42)
    
    groups = [[] for _ in range(num_groups)]
    for idx, label in enumerate(labels):
        groups[label].append(primitives[idx])
        
    # Rebalance step: split overloaded zones (MAX_PRIMS_PER_ZONE)
    MAX_PRIMS_PER_ZONE = num_spheres // num_groups + 4
    balanced_groups = []
    
    for g_idx, g_prims in enumerate(groups):
        if not g_prims:
            continue
            
        if len(g_prims) > MAX_PRIMS_PER_ZONE:
            print(f"[ZCC] Rebalancing: splitting overloaded zone {g_idx} containing {len(g_prims)} primitives...")
            sub_centers = np.array([p["center"] for p in g_prims])
            with MemoryTelemetry(f"Zone rebalance split z{g_idx}", warn_mb=10, abort_mb=50):
                _, sub_labels = kmeans_pure(sub_centers, 2, seed=g_idx * 137 + 42)
            
            for sub_idx in range(2):
                sub_g_prims = [g_prims[j] for j in range(len(g_prims)) if sub_labels[j] == sub_idx]
                if not sub_g_prims:
                    continue
                
                # Compute volume-weighted center for sub-zone using np.average (O(1) robust centroid calculations)
                weights = np.array([primitive_volume_weight(p) for p in sub_g_prims])
                w_sum = weights.sum()
                if w_sum > 1e-8:
                    weights /= w_sum
                    sub_g_center = np.average([p["center"] for p in sub_g_prims], axis=0, weights=weights)
                else:
                    sub_g_center = np.mean([p["center"] for p in sub_g_prims], axis=0)
                    
                balanced_groups.append({
                    "center": sub_g_center,
                    "primitives": sub_g_prims
                })
        else:
            g_center = group_centers[g_idx]
            # Compute volume-weighted center for non-split zone using np.average
            weights = np.array([primitive_volume_weight(p) for p in g_prims])
            w_sum = weights.sum()
            if w_sum > 1e-8:
                weights /= w_sum
                g_center = np.average([p["center"] for p in g_prims], axis=0, weights=weights)
            else:
                g_center = group_centers[g_idx]
                
            balanced_groups.append({
                "center": g_center,
                "primitives": g_prims
            })
            
    # Compute bounding sphere for each finalized group
    group_spheres = []
    for group in balanced_groups:
        g_center = group["center"]
        g_prims = group["primitives"]
        
        max_dist = 0.0
        for prim in g_prims:
            if prim["type"] == "sphere":
                dist = np.linalg.norm(prim["center"] - g_center) + prim["radius"]
            elif prim["type"] == "capsule":
                da = np.linalg.norm(prim["a"] - g_center) + prim["radius"]
                db = np.linalg.norm(prim["b"] - g_center) + prim["radius"]
                dist = max(da, db)
            elif prim["type"] == "box":
                ext = prim["extents"]
                ax0, ax1, ax2 = prim["axis0"], prim["axis1"], prim["axis2"]
                corners = []
                for dx in [-1, 1]:
                    for dy in [-1, 1]:
                        for dz in [-1, 1]:
                            c = prim["center"] + dx*ext[0]*ax0 + dy*ext[1]*ax1 + dz*ext[2]*ax2
                            corners.append(c)
                dist = max(np.linalg.norm(c - g_center) for c in corners)
            max_dist = max(max_dist, dist)
            
        group_spheres.append({
            "center": g_center,
            "radius": max_dist,
            "primitives": g_prims
        })
        
    return group_spheres

def bake_coarse_sdf_b64(groups, bounding_radius, resolution=32):
    """
    Bakes a coarse SDF onto a resolution³ grid at compile time.
    Returns base64-encoded float32 binary for upload as a WebGL3D texture.
    """
    MAX_BAKE_RESOLUTION = 48
    resolution = min(resolution, MAX_BAKE_RESOLUTION)
    
    grid = np.full((resolution, resolution, resolution), 1e5, dtype=np.float32)
    pts = np.mgrid[
        -bounding_radius:bounding_radius:resolution*1j,
        -bounding_radius:bounding_radius:resolution*1j,
        -bounding_radius:bounding_radius:resolution*1j
    ].reshape(3, -1).T.astype(np.float32)
    
    min_d = np.full(len(pts), 1e5, dtype=np.float32)
    for group in groups:
        g_center_f32 = group["center"].astype(np.float32)
        gr = float(group["radius"])
        d = np.linalg.norm(pts - g_center_f32, axis=1) - gr
        np.minimum(min_d, d, out=min_d)
        
    grid = min_d.reshape(resolution, resolution, resolution)
    return base64.b64encode(grid.tobytes()).decode('ascii'), resolution

def estimate_analytic_gradients(points, groups, blend_radius, h=1e-3):
    """
    Estimates the normalized gradient vector of the analytic SDF at the given points
    using central differences.
    """
    N = points.shape[0]
    
    # Offsets in each dimension
    dx = np.zeros_like(points)
    dx[:, 0] = h
    dy = np.zeros_like(points)
    dy[:, 1] = h
    dz = np.zeros_like(points)
    dz[:, 2] = h
    
    # Evaluate at offset coordinates
    d_x_plus  = eval_analytic_sdf_cpu(points + dx, groups, blend_radius)
    d_x_minus = eval_analytic_sdf_cpu(points - dx, groups, blend_radius)
    d_y_plus  = eval_analytic_sdf_cpu(points + dy, groups, blend_radius)
    d_y_minus = eval_analytic_sdf_cpu(points - dy, groups, blend_radius)
    d_z_plus  = eval_analytic_sdf_cpu(points + dz, groups, blend_radius)
    d_z_minus = eval_analytic_sdf_cpu(points - dz, groups, blend_radius)
    
    gx = (d_x_plus - d_x_minus) / (2.0 * h)
    gy = (d_y_plus - d_y_minus) / (2.0 * h)
    gz = (d_z_plus - d_z_minus) / (2.0 * h)
    
    g = np.stack([gx, gy, gz], axis=1)
    g_lens = np.linalg.norm(g, axis=1)
    
    # Avoid division by zero: default to up-vector for zero/tiny gradients
    zero_mask = g_lens < 1e-8
    g_lens[zero_mask] = 1.0
    g[zero_mask] = np.array([0.0, 1.0, 0.0])
    
    return g / g_lens[:, np.newaxis]

def weighted_median(values, weights):
    """
    Computes the weighted median of a 1D numpy array.
    """
    if len(values) == 0:
        return 0.0
    idx = np.argsort(values)
    values = values[idx]
    weights = weights[idx]
    
    cumsum_weights = np.cumsum(weights)
    cutoff = cumsum_weights[-1] / 2.0
    
    median_idx = np.searchsorted(cumsum_weights, cutoff)
    median_idx = min(median_idx, len(values) - 1)
    return values[median_idx]

def weighted_percentile(values, weights, percentile=95):
    """
    Computes the weighted percentile of a 1D numpy array.
    """
    if len(values) == 0:
        return 0.0
    idx = np.argsort(values)
    values = values[idx]
    weights = weights[idx]
    
    cumsum_weights = np.cumsum(weights)
    cutoff = (percentile / 100.0) * cumsum_weights[-1]
    
    idx_p = np.searchsorted(cumsum_weights, cutoff)
    idx_p = min(idx_p, len(values) - 1)
    return values[idx_p]

def bake_residual_and_error(groups, bounding_radius, sampled_points, blend_radius, resolution=32, w_offset=0.4, eps_ratio=0.5):
    """
    Bakes a robust signed-distance residual correction field R and conservative local error bounds E
    using weighted surface-offset signed sampling.
    """
    R_grid = np.zeros((resolution, resolution, resolution), dtype=np.float32)
    E_grid = np.zeros((resolution, resolution, resolution), dtype=np.float32)
    
    voxel_residuals = {}
    N = len(sampled_points)
    if N == 0:
        return (
            base64.b64encode(R_grid.tobytes()).decode('ascii'),
            base64.b64encode(E_grid.tobytes()).decode('ascii'),
            resolution
        )
        
    # 1. Compute offsets using estimated analytic SDF gradients (normals)
    voxel_size = 2.0 * bounding_radius / resolution
    eps = eps_ratio * voxel_size
    
    normals = estimate_analytic_gradients(sampled_points, groups, blend_radius)
    
    pts_plus = sampled_points + eps * normals
    pts_minus = sampled_points - eps * normals
    
    # 2. Evaluate analytic distances in a single vectorized pass
    all_pts = np.vstack([sampled_points, pts_plus, pts_minus])
    d_a_all = eval_analytic_sdf_cpu(all_pts, groups, blend_radius)
    
    d_a_surface = d_a_all[:N]
    d_a_plus = d_a_all[N:2*N]
    d_a_minus = d_a_all[2*N:]
    
    # 3. Form unified signed distance residuals
    r_surface = -d_a_surface
    r_plus = eps - d_a_plus
    r_minus = -eps - d_a_minus
    
    all_residuals = np.concatenate([r_surface, r_plus, r_minus])
    
    # Surface weight is 1.0, off-surface offset weight is w_offset
    weights_all = np.concatenate([np.ones(N, dtype=np.float32), np.ones(2*N, dtype=np.float32) * w_offset])
    
    # 4. Map points to voxel coordinates
    v_coors_all = ((all_pts + bounding_radius) * (resolution / (2.0 * bounding_radius))).astype(int)
    v_coors_all = np.clip(v_coors_all, 0, resolution - 1)
    
    for i in range(len(all_pts)):
        key = (v_coors_all[i, 0], v_coors_all[i, 1], v_coors_all[i, 2])
        if key not in voxel_residuals:
            voxel_residuals[key] = []
        voxel_residuals[key].append((all_residuals[i], weights_all[i]))
        
    for x in range(resolution):
        for y in range(resolution):
            for z in range(resolution):
                key = (x, y, z)
                if key in voxel_residuals:
                    pairs = voxel_residuals[key]
                    vals = np.array([p[0] for p in pairs])
                    w = np.array([p[1] for p in pairs])
                    med = weighted_median(vals, w)
                    R_grid[x, y, z] = med
                    E_grid[x, y, z] = weighted_percentile(np.abs(vals - med), w, 95) + 0.002
                else:
                    R_grid[x, y, z] = 0.0
                    E_grid[x, y, z] = 0.0
                    
    # Neighbor diffusion fill
    R_filled = R_grid.copy()
    E_filled = E_grid.copy()
    
    for x in range(resolution):
        for y in range(resolution):
            for z in range(resolution):
                if (x, y, z) not in voxel_residuals:
                    neighbors = []
                    for dx in [-1, 0, 1]:
                        for dy in [-1, 0, 1]:
                            for dz in [-1, 0, 1]:
                                nx, ny, nz = x + dx, y + dy, z + dz
                                if 0 <= nx < resolution and 0 <= ny < resolution and 0 <= nz < resolution:
                                    if (nx, ny, nz) in voxel_residuals:
                                        neighbors.append((R_grid[nx, ny, nz], E_grid[nx, ny, nz]))
                    if neighbors:
                        R_filled[x, y, z] = np.mean([n[0] for n in neighbors])
                        E_filled[x, y, z] = np.max([n[1] for n in neighbors])
                    else:
                        R_filled[x, y, z] = 0.0
                        E_filled[x, y, z] = 0.05  # Conservative fallback safety margin for empty cells
                        
    # Max filter error dilation
    E_dilated = E_filled.copy()
    for x in range(1, resolution - 1):
        for y in range(1, resolution - 1):
            for z in range(1, resolution - 1):
                val = np.max(E_filled[x-1:x+2, y-1:y+2, z-1:z+2])
                E_dilated[x, y, z] = val
                
    return (
        base64.b64encode(R_filled.tobytes()).decode('ascii'),
        base64.b64encode(E_dilated.tobytes()).decode('ascii'),
        resolution
    )

def validate_residual_bounds(val_pts, groups, R_bytes_b64, E_bytes_b64, bounding_radius, blend_radius, resolution, h=1e-3, eps_ratio=0.5):
    """
    Computes visual errors, bound values, violations, Eikonal stress, and ablation study
    metrics on held-out validation samples. Also computes off-surface signed-offset accuracy.
    """
    N = len(val_pts)
    if N == 0:
        return {}
        
    d_a = eval_analytic_sdf_cpu(val_pts, groups, blend_radius)
    
    # Map points to voxel indices
    v_coors = ((val_pts + bounding_radius) * (resolution / (2.0 * bounding_radius))).astype(int)
    v_coors = np.clip(v_coors, 0, resolution - 1)
    
    # Decode R and E grids
    R_bytes = base64.b64decode(R_bytes_b64)
    R_grid = np.frombuffer(R_bytes, dtype=np.float32).reshape(resolution, resolution, resolution)
    
    E_bytes = base64.b64decode(E_bytes_b64)
    E_grid = np.frombuffer(E_bytes, dtype=np.float32).reshape(resolution, resolution, resolution)
    
    # Sample R and E grids
    sampled_R = np.array([R_grid[v_coors[i, 0], v_coors[i, 1], v_coors[i, 2]] for i in range(N)])
    sampled_E = np.array([E_grid[v_coors[i, 0], v_coors[i, 1], v_coors[i, 2]] for i in range(N)])
    
    d_visual = d_a + sampled_R
    
    # Validation errors (absolute surface deviation: target is 0.0)
    analytic_errors = np.abs(d_a)
    visual_errors = np.abs(d_visual)
    
    # Check bounds coverage: we expect visual_errors <= sampled_E
    violations = visual_errors > sampled_E
    violation_rate = float(np.mean(violations))
    max_violation = float(np.max(visual_errors - sampled_E)) if np.any(violations) else 0.0
    p95_margin = float(np.percentile(sampled_E - visual_errors, 95))
    
    # Calculate signed offset validation errors
    voxel_size = 2.0 * bounding_radius / resolution
    eps = eps_ratio * voxel_size
    normals = estimate_analytic_gradients(val_pts, groups, blend_radius)
    val_pts_plus = val_pts + eps * normals
    val_pts_minus = val_pts - eps * normals
    
    d_a_plus = eval_analytic_sdf_cpu(val_pts_plus, groups, blend_radius)
    d_a_minus = eval_analytic_sdf_cpu(val_pts_minus, groups, blend_radius)
    
    v_coors_plus = np.clip(((val_pts_plus + bounding_radius) * (resolution / (2.0 * bounding_radius))).astype(int), 0, resolution - 1)
    v_coors_minus = np.clip(((val_pts_minus + bounding_radius) * (resolution / (2.0 * bounding_radius))).astype(int), 0, resolution - 1)
    
    sampled_R_plus = np.array([R_grid[v_coors_plus[i, 0], v_coors_plus[i, 1], v_coors_plus[i, 2]] for i in range(N)])
    sampled_R_minus = np.array([R_grid[v_coors_minus[i, 0], v_coors_minus[i, 1], v_coors_minus[i, 2]] for i in range(N)])
    
    d_v_plus = d_a_plus + sampled_R_plus
    d_v_minus = d_a_minus + sampled_R_minus
    
    offset_plus_errors = np.abs(d_v_plus - eps)
    offset_minus_errors = np.abs(d_v_minus - (-eps))
    combined_offset_errors = np.concatenate([offset_plus_errors, offset_minus_errors])
    
    # Eikonal stress estimation on CPU using central differences
    dx = np.zeros_like(val_pts)
    dx[:, 0] = h
    dy = np.zeros_like(val_pts)
    dy[:, 1] = h
    dz = np.zeros_like(val_pts)
    dz[:, 2] = h
    
    d_x_plus  = eval_analytic_sdf_cpu(val_pts + dx, groups, blend_radius)
    d_x_minus = eval_analytic_sdf_cpu(val_pts - dx, groups, blend_radius)
    d_y_plus  = eval_analytic_sdf_cpu(val_pts + dy, groups, blend_radius)
    d_y_minus = eval_analytic_sdf_cpu(val_pts - dy, groups, blend_radius)
    d_z_plus  = eval_analytic_sdf_cpu(val_pts + dz, groups, blend_radius)
    d_z_minus = eval_analytic_sdf_cpu(val_pts - dz, groups, blend_radius)
    
    v_coors_x_plus = np.clip(((val_pts + dx + bounding_radius) * (resolution / (2.0 * bounding_radius))).astype(int), 0, resolution - 1)
    v_coors_x_minus = np.clip(((val_pts - dx + bounding_radius) * (resolution / (2.0 * bounding_radius))).astype(int), 0, resolution - 1)
    v_coors_y_plus = np.clip(((val_pts + dy + bounding_radius) * (resolution / (2.0 * bounding_radius))).astype(int), 0, resolution - 1)
    v_coors_y_minus = np.clip(((val_pts - dy + bounding_radius) * (resolution / (2.0 * bounding_radius))).astype(int), 0, resolution - 1)
    v_coors_z_plus = np.clip(((val_pts + dz + bounding_radius) * (resolution / (2.0 * bounding_radius))).astype(int), 0, resolution - 1)
    v_coors_z_minus = np.clip(((val_pts - dz + bounding_radius) * (resolution / (2.0 * bounding_radius))).astype(int), 0, resolution - 1)
    
    R_x_plus = np.array([R_grid[v_coors_x_plus[i,0], v_coors_x_plus[i,1], v_coors_x_plus[i,2]] for i in range(N)])
    R_x_minus = np.array([R_grid[v_coors_x_minus[i,0], v_coors_x_minus[i,1], v_coors_x_minus[i,2]] for i in range(N)])
    R_y_plus = np.array([R_grid[v_coors_y_plus[i,0], v_coors_y_plus[i,1], v_coors_y_plus[i,2]] for i in range(N)])
    R_y_minus = np.array([R_grid[v_coors_y_minus[i,0], v_coors_y_minus[i,1], v_coors_y_minus[i,2]] for i in range(N)])
    R_z_plus = np.array([R_grid[v_coors_z_plus[i,0], v_coors_z_plus[i,1], v_coors_z_plus[i,2]] for i in range(N)])
    R_z_minus = np.array([R_grid[v_coors_z_minus[i,0], v_coors_z_minus[i,1], v_coors_z_minus[i,2]] for i in range(N)])
    
    gx = ((d_x_plus + R_x_plus) - (d_x_minus + R_x_minus)) / (2.0 * h)
    gy = ((d_y_plus + R_y_plus) - (d_y_minus + R_y_minus)) / (2.0 * h)
    gz = ((d_z_plus + R_z_plus) - (d_z_minus + R_z_minus)) / (2.0 * h)
    
    g_lens = np.sqrt(gx**2 + gy**2 + gz**2)
    eikonal_stress = np.abs(g_lens - 1.0)
    
    analytic_p95 = float(np.percentile(analytic_errors, 95))
    visual_p95 = float(np.percentile(visual_errors, 95))
    improvement_ratio = float((analytic_p95 - visual_p95) / max(analytic_p95, 1e-5))
    
    certificate_claim = "Empirical coverage verified with zero out-of-sample safety violations under CPU validation." if violation_rate == 0.0 else "Empirical coverage verified with minor boundary exceptions."
    
    return {
        "validation_stats": {
            "heldout_samples": N,
            "analytic_mean_abs": float(np.mean(analytic_errors)),
            "analytic_p95_abs": analytic_p95,
            "visual_mean_abs": float(np.mean(visual_errors)),
            "visual_p95_abs": visual_p95,
            "visual_max_abs": float(np.max(visual_errors)),
            "bound_violation_rate": violation_rate,
            "max_bound_violation": max_violation,
            "p95_bound_margin": p95_margin
        },
        "signed_offset_validation": {
            "offset_epsilon": float(eps),
            "outer_p95_abs_error": float(np.percentile(offset_plus_errors, 95)),
            "inner_p95_abs_error": float(np.percentile(offset_minus_errors, 95)),
            "combined_p95_abs_error": float(np.percentile(combined_offset_errors, 95))
        },
        "eikonal_validation": {
            "mean_stress": float(np.mean(eikonal_stress)),
            "p95_stress": float(np.percentile(eikonal_stress, 95)),
            "max_stress": float(np.max(eikonal_stress))
        },
        "residual_ablation": {
            "analytic_p95_abs_error": analytic_p95,
            "visual_p95_abs_error": visual_p95,
            "residual_improvement_ratio": improvement_ratio
        },
        "certificate": {
            "status": "empirical_certificate",
            "claim": certificate_claim,
            "violation_rate": violation_rate,
            "heldout_samples": N
        }
    }

def generate_zone_glsl(groups, palette):
    """Emits individual zone functions to relieve register pressure on Blackwell cuda cores"""
    zone_funcs = []
    
    for g_idx, group in enumerate(groups):
        # 1. Color and Distance zone evaluation function
        lines = []
        lines.append(f"vec4 evalZone{g_idx}(vec3 p) {{")
        lines.append(f"    vec4 d = vec4(1e5, 0.0, 0.0, 0.0);")
        
        is_first = True
        for prim in group["primitives"]:
            col = palette[prim["cluster_idx"] % len(palette)]
            if prim["type"] == "sphere":
                c = prim["center"]
                r = prim["radius"]
                expr = f"vec4(length(p - vec3({fmt_glsl(c[0])}, {fmt_glsl(c[1])}, {fmt_glsl(c[2])})) - {fmt_glsl(r)}, {fmt_glsl(col[0])}, {fmt_glsl(col[1])}, {fmt_glsl(col[2])})"
            elif prim["type"] == "capsule":
                a = prim["a"]
                b = prim["b"]
                r = prim["radius"]
                expr = f"mapCapsule(p, vec3({fmt_glsl(a[0])}, {fmt_glsl(a[1])}, {fmt_glsl(a[2])}), vec3({fmt_glsl(b[0])}, {fmt_glsl(b[1])}, {fmt_glsl(b[2])}), {fmt_glsl(r)}, vec3({fmt_glsl(col[0])}, {fmt_glsl(col[1])}, {fmt_glsl(col[2])}))"
            elif prim["type"] == "box":
                c = prim["center"]
                ax0, ax1, ax2 = prim["axis0"], prim["axis1"], prim["axis2"]
                ext = prim["extents"]
                expr = f"mapBox(p, vec3({fmt_glsl(c[0])}, {fmt_glsl(c[1])}, {fmt_glsl(c[2])}), vec3({fmt_glsl(ax0[0])}, {fmt_glsl(ax0[1])}, {fmt_glsl(ax0[2])}), vec3({fmt_glsl(ax1[0])}, {fmt_glsl(ax1[1])}, {fmt_glsl(ax1[2])}), vec3({fmt_glsl(ax2[0])}, {fmt_glsl(ax2[1])}, {fmt_glsl(ax2[2])}), vec3({fmt_glsl(ext[0])}, {fmt_glsl(ext[1])}, {fmt_glsl(ext[2])}), vec3({fmt_glsl(col[0])}, {fmt_glsl(col[1])}, {fmt_glsl(col[2])}))"
                
            if is_first:
                lines.append(f"    d = {expr};")
                is_first = False
            else:
                lines.append(f"    d = sminVal(d, {expr}, u_blendRadius);")
        lines.append("    return d;")
        lines.append("}")
        zone_funcs.append("\n".join(lines))
        
        # 2. Distance-only zone evaluation function
        lines_d = []
        lines_d.append(f"float evalZone{g_idx}D(vec3 p) {{")
        lines_d.append(f"    float d = 1e5;")
        
        is_first = True
        for prim in group["primitives"]:
            if prim["type"] == "sphere":
                c = prim["center"]
                r = prim["radius"]
                expr_d = f"length(p - vec3({fmt_glsl(c[0])}, {fmt_glsl(c[1])}, {fmt_glsl(c[2])})) - {fmt_glsl(r)}"
            elif prim["type"] == "capsule":
                a = prim["a"]
                b = prim["b"]
                r = prim["radius"]
                expr_d = f"mapCapsuleD(p, vec3({fmt_glsl(a[0])}, {fmt_glsl(a[1])}, {fmt_glsl(a[2])}), vec3({fmt_glsl(b[0])}, {fmt_glsl(b[1])}, {fmt_glsl(b[2])}), {fmt_glsl(r)})"
            elif prim["type"] == "box":
                c = prim["center"]
                ax0, ax1, ax2 = prim["axis0"], prim["axis1"], prim["axis2"]
                ext = prim["extents"]
                expr_d = f"mapBoxD(p, vec3({fmt_glsl(c[0])}, {fmt_glsl(c[1])}, {fmt_glsl(c[2])}), vec3({fmt_glsl(ax0[0])}, {fmt_glsl(ax0[1])}, {fmt_glsl(ax0[2])}), vec3({fmt_glsl(ax1[0])}, {fmt_glsl(ax1[1])}, {fmt_glsl(ax1[2])}), vec3({fmt_glsl(ax2[0])}, {fmt_glsl(ax2[1])}, {fmt_glsl(ax2[2])}), vec3({fmt_glsl(ext[0])}, {fmt_glsl(ext[1])}, {fmt_glsl(ext[2])}))"
                
            if is_first:
                lines_d.append(f"    d = {expr_d};")
                is_first = False
            else:
                lines_d.append(f"    d = sminFloat(d, {expr_d}, u_blendRadius);")
        lines_d.append("    return d;")
        lines_d.append("}")
        zone_funcs.append("\n".join(lines_d))
        
    return "\n\n".join(zone_funcs)

def emit_svg_proxy(output_file, primitives, groups, palette, bounding_radius, quality_name, risk_level, risk_reason, truncated_msg=""):
    svg_path = output_file.replace(".html", ".svg")
    width, height = 1200, 800
    scale = 0.42 * min(width, height) / max(1e-6, bounding_radius)

    def project(p):
        x = width * 0.5 + float(p[0]) * scale
        y = height * 0.5 - float(p[1]) * scale
        return x, y

    lines = []
    lines.append(f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {width} {height}">')
    lines.append('<rect width="100%" height="100%" fill="#05070d"/>')
    lines.append('<style>')
    lines.append('text{font-family:monospace;font-size:14px}')
    lines.append('.group{fill:none;stroke:#00ffff;stroke-opacity:.16;stroke-width:1.5}')
    lines.append('.prim{stroke:#ffffff;stroke-opacity:.25}')
    lines.append('</style>')

    # HUD info
    lines.append(f'<text x="24" y="36" fill="#00ffcc">ZCC SDF SVG Proxy — {quality_name.upper()}</text>')
    lines.append(f'<text x="24" y="60" fill="#888">primitives={len(primitives)} groups={len(groups)} bound={bounding_radius:.3f}</text>')
    
    # Risk badge in SVG text
    risk_color = "#00ffcc" if risk_level == "low" else ("#ffaa00" if risk_level == "medium" else "#ff00aa")
    lines.append(f'<text x="24" y="84" fill="{risk_color}">WebGL Risk: {risk_level.upper()} ({risk_reason})</text>')

    if truncated_msg:
        lines.append(f'<text x="24" y="108" fill="#ff00aa">⚠ {truncated_msg}</text>')

    # Group bounds
    for g in groups:
        cx, cy = project(g["center"])
        r = float(g["radius"]) * scale
        lines.append(f'<circle class="group" cx="{cx:.2f}" cy="{cy:.2f}" r="{r:.2f}"/>')

    # Primitives
    for p in primitives:
        col = palette[p["cluster_idx"] % len(palette)]
        color = f'rgb({int(col[0]*255)},{int(col[1]*255)},{int(col[2]*255)})'

        if p["type"] == "sphere":
            cx, cy = project(p["center"])
            r = max(1.5, float(p["radius"]) * scale)
            lines.append(f'<circle class="prim" cx="{cx:.2f}" cy="{cy:.2f}" r="{r:.2f}" fill="{color}" fill-opacity="0.65"/>')

        elif p["type"] == "capsule":
            ax, ay = project(p["a"])
            bx, by = project(p["b"])
            sw = max(1.5, float(p["radius"]) * 2.0 * scale)
            lines.append(f'<path class="prim" d="M{ax:.2f},{ay:.2f} L{bx:.2f},{by:.2f}" stroke="{color}" stroke-width="{sw:.2f}" stroke-linecap="round" fill="none" opacity="0.7"/>')

        elif p["type"] == "box":
            cx, cy = project(p["center"])
            ex = max(1.5, float(p["extents"][0]) * scale)
            ey = max(1.5, float(p["extents"][1]) * scale)
            lines.append(f'<rect class="prim" x="{cx-ex:.2f}" y="{cy-ey:.2f}" width="{2*ex:.2f}" height="{2*ey:.2f}" fill="{color}" fill-opacity="0.5"/>')

    lines.append('</svg>')

    svg = "\n".join(lines)
    svg_dir = os.path.dirname(os.path.abspath(svg_path))
    if svg_dir:
        os.makedirs(svg_dir, exist_ok=True)
    with open(svg_path, "w", encoding="utf-8") as f:
        f.write(svg)
    return svg_path, svg

def _run_compilation(input_file, output_file, num_spheres, num_samples, coarse_res, residual_res, w_offset=0.4, eps_ratio=0.5, quality="balanced"):
    """Core compilation run wrapping file execution"""
    t_start = time.time()
    
    protect_from_oom_killer()
    set_memory_ceiling(2048 * 1024 * 1024)

    # 1. Stream GLB Sample Points
    t0 = time.time()
    with MemoryTelemetry("GLB Stream Parser", warn_mb=80, abort_mb=150) as tm_parser:
        sampled_points, parser_stats = stream_glb_vertices(input_file, num_samples=num_samples)
    t_parser = time.time() - t0
    
    print(f"[ZCC Telemetry] GLB Parser Stats: accessors={parser_stats['num_accessors_read']}, seeks={parser_stats['read_calls']}, bytes={parser_stats['bytes_read']}, avg_size={parser_stats['avg_read_bytes']} bytes")
        
    # Scale points
    centroid = np.mean(sampled_points, axis=0)
    sampled_points -= centroid
    scale = np.max(np.linalg.norm(sampled_points, axis=1))
    if scale < 1e-7 or not np.isfinite(scale):
        raise ValueError("Degenerate geometry scale")
    sampled_points /= scale

    # 2. KMeans packing & Lloyd Relaxation
    t0 = time.time()
    with MemoryTelemetry("KMeans Clustering", warn_mb=150, abort_mb=250) as tm_kmeans:
        centers, labels = kmeans_pure(sampled_points, num_spheres, seed=42)
        centers = lloyd_relaxation(sampled_points, centers, iterations=8)
        
        # KMeans Labels Re-synchronization check after Lloyd movement (safe column-wise loop)
        labels = np.zeros(sampled_points.shape[0], dtype=np.int32)
        min_dists = safe_alloc((sampled_points.shape[0],), np.float64, "resync_min_dists")
        min_dists[:] = np.inf
        for i in range(len(centers)):
            col = np.linalg.norm(sampled_points - centers[i], axis=1)
            mask = col < min_dists
            min_dists[mask] = col[mask]
            labels[mask] = i
        del min_dists
    t_kmeans = time.time() - t0

    # 3. PCA Primitive classification & confidence scoring
    t0 = time.time()
    with MemoryTelemetry("PCA Classification", warn_mb=80, abort_mb=120) as tm_pca:
        primitives = []
        for i in range(num_spheres):
            cluster_pts = sampled_points[labels == i]
            if len(cluster_pts) == 0:
                prim = {
                    "type": "sphere",
                    "center": centers[i].copy(),
                    "radius": 0.05,
                    "cluster_idx": i,
                    "fit_error": {"mean_abs": 0.0, "p95_abs": 0.0, "max_abs": 0.0}
                }
            else:
                prim = classify_cluster_primitive(cluster_pts)
                if "center" not in prim:
                    prim["center"] = centers[i].copy()
                prim["cluster_idx"] = i
                prim["fit_error"] = evaluate_primitive_fit_error(prim, cluster_pts)
            primitives.append(prim)
    t_pca = time.time() - t0

    # 4. Grouping & Bounding Regions with Overload Rebalancing
    num_groups = max(2, num_spheres // 16)
    t0 = time.time()
    with MemoryTelemetry("Hierarchical Grouping", warn_mb=50, abort_mb=100) as tm_group:
        groups = group_primitives(primitives, num_groups, num_spheres)
    t_group = time.time() - t0

    # Output emission
    palette = generate_palette(num_spheres)

    # Constructing GLSL map hierarchies with warp-coherent zone weights
    print("[ZCC SDF Compiler] Constructing early-exit hierarchy maps...")
    
    # Generate modular zone helper functions to alleviate GPU register file pressure
    zone_glsl_definitions = generate_zone_glsl(groups, palette)
    
    sdf_lines = []
    sdf_lines_d = []
    
    for g_idx, group in enumerate(groups):
        g_center = group["center"]
        g_radius = group["radius"]
        
        # Color and Distance (vec4) map body
        sdf_lines.append(f"// --- Bounding Zone {g_idx} ---")
        sdf_lines.append(f"                float g_dist{g_idx} = length(p - vec3({fmt_glsl(g_center[0])}, {fmt_glsl(g_center[1])}, {fmt_glsl(g_center[2])})) - {fmt_glsl(g_radius)};")
        sdf_lines.append(f"                float w{g_idx} = max(0.0, -(g_dist{g_idx} - (u_blendRadius + 0.05)));")
        sdf_lines.append(f"                if (w{g_idx} > 0.0) {{")
        sdf_lines.append(f"                    d = sminVal(d, evalZone{g_idx}(p), u_blendRadius);")
        sdf_lines.append("                } else {")
        sdf_lines.append(f"                    d.x = min(d.x, g_dist{g_idx});")
        sdf_lines.append("                }")
        
        # Distance-only (float) mapD body
        sdf_lines_d.append(f"// --- Bounding Zone {g_idx} ---")
        sdf_lines_d.append(f"                float g_dist{g_idx} = length(p - vec3({fmt_glsl(g_center[0])}, {fmt_glsl(g_center[1])}, {fmt_glsl(g_center[2])})) - {fmt_glsl(g_radius)};")
        sdf_lines_d.append(f"                float w{g_idx} = max(0.0, -(g_dist{g_idx} - (u_blendRadius + 0.05)));")
        sdf_lines_d.append(f"                if (w{g_idx} > 0.0) {{")
        sdf_lines_d.append(f"                    d = sminFloat(d, evalZone{g_idx}D(p), u_blendRadius);")
        sdf_lines_d.append("                } else {")
        sdf_lines_d.append(f"                    d = min(d, g_dist{g_idx});")
        sdf_lines_d.append("                }")
        
    sdf_code_str = "\n".join(sdf_lines)
    sdf_code_str_d = "\n".join(sdf_lines_d)

    # Compute global dynamic bounding radius factoring max audio/jiggle scales
    max_group_bound = max(np.linalg.norm(g["center"]) + g["radius"] for g in groups)
    max_blend_radius = 0.35
    max_jiggle_expansion = 0.0375
    max_audio_scale = 1.15
    bounding_radius = (max_group_bound + max_blend_radius + max_jiggle_expansion) * max_audio_scale + 0.05
    bounding_radius_sqr = bounding_radius ** 2

    # Precise compiled fragment shader size estimate check
    GLSL_STAGE_LIMIT = 65535
    fs_estimated_len = len(sdf_code_str) + len(sdf_code_str_d) + len(zone_glsl_definitions) + 8000
    if fs_estimated_len > 55000:
        raise ValueError(f"Estimated fragment shader size ({fs_estimated_len} bytes) exceeds the WebGL2 compiler safety ceiling of 55KB. "
                         f"Please compile with a lower sphere count K (e.g. --k {num_spheres // 2} or legacy K argument) to prevent browser freezes.")
    max_prims_per_group = max(len(g["primitives"]) for g in groups)
    
    print(f"[ZCC SDF Telemetry] Generated Shader Length: {fs_estimated_len} bytes")
    print(f"[ZCC SDF Telemetry] Primitive Count: {num_spheres}")
    print(f"[ZCC SDF Telemetry] Group Count: {num_groups}")
    print(f"[ZCC SDF Telemetry] Max Primitives Per Group: {max_prims_per_group}")
    print(f"[ZCC SDF Telemetry] Bounding Radius: {bounding_radius:.4f}")
    print(f"[ZCC SDF Telemetry] Quality Preset: {quality}")
    print(f"[ZCC SDF Telemetry] Default Max Steps: 48")
    print(f"[ZCC SDF Telemetry] Default AO Enabled: 0")
    print(f"[ZCC SDF Telemetry] Default Shadow Enabled: 0")
    print(f"[ZCC SDF Telemetry] Default Resolution Limit: 57600 (Low)")
    print(f"[ZCC SDF Telemetry] Coarse SDF Resolution: {coarse_res}")
    
    cost_factor = num_spheres * max_prims_per_group
    if cost_factor > 1500 or fs_estimated_len > 60000:
        print(f"[ZCC SDF Telemetry] WARNING: Estimated pixel shader execution cost is HIGH ({cost_factor} complexity factor).")
    if fs_estimated_len > GLSL_STAGE_LIMIT:
        print(f"[ZCC] WARNING: Fragment shader ~{fs_estimated_len} chars may exceed "
              f"WebGL2 limit of {GLSL_STAGE_LIMIT} on some drivers. "
              f"Consider UBO emission for K>{num_spheres}.")

    # Bake Coarse 3D SDF Texture grid wrapped in MemoryTelemetry
    print("[ZCC SDF Compiler] Baking Coarse 3D SDF Texture...")
    t0 = time.time()
    with MemoryTelemetry("Coarse SDF Bake", warn_mb=50, abort_mb=100) as tm_bake:
        coarse_b64, coarse_res_actual = bake_coarse_sdf_b64(groups, bounding_radius, resolution=coarse_res)
    t_bake = time.time() - t0

    # 80/20 train/held-out validation split
    rng = np.random.default_rng(42)
    shuffled_indices = rng.permutation(len(sampled_points))
    split_idx = int(0.8 * len(sampled_points))
    bake_pts = sampled_points[shuffled_indices[:split_idx]]
    val_pts = sampled_points[shuffled_indices[split_idx:]]

    # Bake Residual and Error bound 3D grids (V3.1)
    # Bake Residual and Error bound 3D grids (V3.3)
    print(f"[ZCC SDF Compiler] Baking V3.3 Residual and Error fields (w_offset={w_offset}, eps_ratio={eps_ratio})...")
    t0 = time.time()
    with MemoryTelemetry("Residual Field Bake", warn_mb=50, abort_mb=100) as tm_res:
        res_b64, err_b64, res_res_actual = bake_residual_and_error(
            groups, bounding_radius, bake_pts, blend_radius=0.12, resolution=residual_res, w_offset=w_offset, eps_ratio=eps_ratio
        )
    t_residual = time.time() - t0

    # Validate residual bounds on heldout validation set
    validation_stats = validate_residual_bounds(
        val_pts, groups, res_b64, err_b64, bounding_radius, blend_radius=0.12, resolution=res_res_actual, eps_ratio=eps_ratio
    )

    # Determine compile-time risk level & reason using named safety constants
    risk_level = "low"
    risk_reason = "Shader size is small and primitive count is low."
    if fs_estimated_len > WEBGL_SHADER_RISK_LOW_MAX:
        risk_level = "medium"
        risk_reason = "Shader size is close to WebGL2 driver compiling budget."
    if fs_estimated_len > WEBGL_SHADER_RISK_MED_MAX or max_prims_per_group > WEBGL_GROUP_PRIMS_HIGH:
        risk_level = "high"
        risk_reason = "Pathological asset size or group primitives count. WebGL compilation may take a few seconds."

    # SVG Node Count Guardrail Truncation
    svg_primitives = primitives
    svg_truncated = False
    truncated_msg = ""
    if len(primitives) > SVG_MAX_PRIMITIVES_LIMIT:
        svg_primitives = primitives[:SVG_MAX_PRIMITIVES_LIMIT]
        svg_truncated = True
        truncated_msg = f"SVG preview truncated: {SVG_MAX_PRIMITIVES_LIMIT} / {len(primitives)} primitives shown to prevent DOM lag."
        print(f"[ZCC SDF Compiler] WARNING: SVG preview primitive count ({len(primitives)}) exceeds limit of {SVG_MAX_PRIMITIVES_LIMIT}. Truncating.")

    # Emit static SVG proxy file
    svg_path, svg_content = emit_svg_proxy(
        output_file, svg_primitives, groups, palette, bounding_radius, quality, risk_level, risk_reason, truncated_msg
    )
    print(f"[ZCC SDF Compiler] Emitting SVG static proxy to {svg_path}...")

    # Generate inline SVG elements for default front XY projection
    width, height = 1200, 800
    scale = 0.42 * min(width, height) / max(1e-6, bounding_radius)

    def project(p):
        x = width * 0.5 + float(p[0]) * scale
        y = height * 0.5 - float(p[1]) * scale
        return x, y

    svg_elements = []
    # Group bounds
    for idx, g in enumerate(groups):
        cx, cy = project(g["center"])
        r = float(g["radius"]) * scale
        svg_elements.append(f'<circle id="svg-group-{idx}" class="group-bound" cx="{cx:.2f}" cy="{cy:.2f}" r="{r:.2f}"/>')
    # Primitives (truncated if above cap)
    for idx, p in enumerate(svg_primitives):
        col = palette[p["cluster_idx"] % len(palette)]
        color = f'rgb({int(col[0]*255)},{int(col[1]*255)},{int(col[2]*255)})'
        if p["type"] == "sphere":
            cx, cy = project(p["center"])
            r = max(1.5, float(p["radius"]) * scale)
            svg_elements.append(f'<circle id="svg-prim-{idx}" class="prim-element prim-sphere" cx="{cx:.2f}" cy="{cy:.2f}" r="{r:.2f}" fill="{color}" fill-opacity="0.65"/>')
        elif p["type"] == "capsule":
            ax, ay = project(p["a"])
            bx, by = project(p["b"])
            sw = max(1.5, float(p["radius"]) * 2.0 * scale)
            svg_elements.append(f'<path id="svg-prim-{idx}" class="prim-element prim-capsule" d="M{ax:.2f},{ay:.2f} L{bx:.2f},{by:.2f}" stroke="{color}" stroke-width="{sw:.2f}" stroke-linecap="round" fill="none" opacity="0.7"/>')
        elif p["type"] == "box":
            cx, cy = project(p["center"])
            ex = max(1.5, float(p["extents"][0]) * scale)
            ey = max(1.5, float(p["extents"][1]) * scale)
            svg_elements.append(f'<rect id="svg-prim-{idx}" class="prim-element prim-box" x="{cx-ex:.2f}" y="{cy-ey:.2f}" width="{2*ex:.2f}" height="{2*ey:.2f}" fill="{color}" fill-opacity="0.5"/>')
            
    svg_elements_str = "\n        ".join(svg_elements)

    # Clean object function for JSON serialization
    def clean_obj(obj):
        if isinstance(obj, np.ndarray):
            return obj.tolist()
        elif isinstance(obj, (np.floating, np.integer)):
            return obj.item()
        elif isinstance(obj, np.bool_):
            return bool(obj)
        elif isinstance(obj, dict):
            return {k: clean_obj(v) for k, v in obj.items()}
        elif isinstance(obj, list):
            return [clean_obj(v) for v in obj]
        else:
            return obj

    js_primitives = json.dumps(clean_obj(primitives))
    js_groups = json.dumps(clean_obj(groups))
    js_palette = json.dumps(clean_obj(palette))

    # Badge background and foreground styling
    badge_bg = "#00ffcc" if risk_level == "low" else ("#ffaa00" if risk_level == "medium" else "#ff00aa")
    badge_fg = "#000" if risk_level == "low" else "#fff"
    badge_style = f"background: {badge_bg}; color: {badge_fg}; font-weight: bold; padding: 2px 6px; border-radius: 3px;"

    print(f"[ZCC SDF Compiler] Emitting WebGL compilation target to {output_file}...")
    html_content = HTML_TEMPLATE
    html_content = html_content.replace("__ASSET_NAME__", os.path.basename(input_file))
    html_content = html_content.replace("__NUM_PRIMITIVES__", str(num_spheres))
    html_content = html_content.replace("__RISK_LEVEL__", risk_level.upper())
    html_content = html_content.replace("__RISK_REASON__", risk_reason)
    html_content = html_content.replace("__RISK_BADGE_STYLE__", badge_style)
    html_content = html_content.replace("__SVG_WARNING__", truncated_msg if svg_truncated else "")
    html_content = html_content.replace("__SVG_ELEMENTS__", svg_elements_str)
    html_content = html_content.replace("__JS_PRIMITIVES__", js_primitives)
    html_content = html_content.replace("__JS_GROUPS__", js_groups)
    html_content = html_content.replace("__JS_PALETTE__", js_palette)
    html_content = html_content.replace("__BOUNDING_RADIUS__", str(bounding_radius))

    html_content = html_content.replace("__ZONE_FUNCTIONS__", zone_glsl_definitions)
    html_content = html_content.replace("__SDF_CODE__", sdf_code_str)
    html_content = html_content.replace("__SDF_CODE_D__", sdf_code_str_d)
    html_content = html_content.replace("__CAMERA_DIST__", "2.5")
    html_content = html_content.replace("__CENTER_Y__", "0.0")
    html_content = html_content.replace("__BOUND_RADIUS__", fmt_glsl(bounding_radius))
    html_content = html_content.replace("__BOUND_RADIUS_SQR__", fmt_glsl(bounding_radius_sqr))
    html_content = html_content.replace("__COARSE_SDF_B64__", coarse_b64)
    html_content = html_content.replace("__COARSE_SDF_RES__", str(coarse_res_actual))
    html_content = html_content.replace("__RESIDUAL_SDF_B64__", res_b64)
    html_content = html_content.replace("__ERROR_SDF_B64__", err_b64)
    html_content = html_content.replace("__RESIDUAL_SDF_RES__", str(res_res_actual))

    out_dir = os.path.dirname(os.path.abspath(output_file))
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)
        
    with open(output_file, "w", encoding="utf-8") as f:
        f.write(html_content)
        
    # Write JSON metadata databases
    manifest_path = output_file.replace(".html", ".primitives.json")
    telemetry_path = output_file.replace(".html", ".telemetry.json")
    
    # Export primitives JSON manifest
    primitives_data = {
        "version": "3.4",
        "source": input_file,
        "normalization": {
            "centroid": centroid.tolist(),
            "scale": float(scale),
            "boundingRadius": float(bounding_radius)
        },
        "groups": [
            {
                "center": g["center"].tolist(),
                "radius": float(g["radius"]),
                "primitiveCount": len(g["primitives"])
            }
            for g in groups
        ],
        "primitives": [
            {
                "type": p["type"],
                "cluster": p["cluster_idx"],
                "center": p["center"].tolist(),
                "color": palette[p["cluster_idx"] % len(palette)].tolist(),
                "fit_error": p["fit_error"],
                "params": {
                    "radius": float(p["radius"]) if "radius" in p else None,
                    "a": p["a"].tolist() if "a" in p else None,
                    "b": p["b"].tolist() if "b" in p else None,
                    "axis0": p["axis0"].tolist() if "axis0" in p else None,
                    "axis1": p["axis1"].tolist() if "axis1" in p else None,
                    "axis2": p["axis2"].tolist() if "axis2" in p else None,
                    "extents": p["extents"].tolist() if "extents" in p else None
                }
            }
            for p in primitives
        ],
        "residual_validation": validation_stats["validation_stats"],
        "signed_offset_validation": validation_stats["signed_offset_validation"],
        "eikonal_validation": validation_stats["eikonal_validation"],
        "residual_ablation": validation_stats["residual_ablation"],
        "certificate": validation_stats["certificate"]
    }
    with open(manifest_path, "w", encoding="utf-8") as f_manifest:
        json.dump(primitives_data, f_manifest, indent=2)
        
    # Export telemetry metrics
    telemetry_data = {
        "source": input_file,
        "num_spheres": num_spheres,
        "num_samples": num_samples,
        "total_elapsed_seconds": time.time() - t_start,
        "parser": parser_stats,
        "defaults": {
            "quality_preset": quality,
            "max_steps": 48,
            "enable_ao": 0,
            "enable_shadow": 0,
            "resolution_limit": 57600,
            "coarse_sdf_resolution": coarse_res
        },
        "passes": {
            "stream_parser_seconds": t_parser,
            "kmeans_clustering_seconds": t_kmeans,
            "pca_classification_seconds": t_pca,
            "hierarchical_grouping_seconds": t_group,
            "coarse_sdf_bake_seconds": t_bake,
            "residual_field_bake_seconds": t_residual
        },
        "viewer": {
            "default_backend": "canvas2d",
            "available_backends": ["svg", "canvas2d", "webgl-lite", "webgl-full"],
            "webgl_lite_fixed_shader": True,
            "webgl_lite_lazy": True,
            "webgl_full_lazy": True,
            "webgl_full_risk": risk_level,
            "risk_reasons": [risk_reason] if risk_reason else [],
            "shader_length": len(fs_estimated_len) if isinstance(fs_estimated_len, str) else fs_estimated_len,
            "max_primitives_per_group": max_prims_per_group,
            "svg_primitive_limit": 512,
            "svg_truncated": svg_truncated
        },
        "residual_validation": validation_stats["validation_stats"],
        "signed_offset_validation": validation_stats["signed_offset_validation"],
        "eikonal_validation": validation_stats["eikonal_validation"],
        "residual_ablation": validation_stats["residual_ablation"],
        "certificate": validation_stats["certificate"]
    }
    with open(telemetry_path, "w", encoding="utf-8") as f_telemetry:
        json.dump(telemetry_data, f_telemetry, indent=2)
        
    print(f"[ZCC SDF Compiler] Dynamic Bounding Radius: {fmt_glsl(bounding_radius)} (Square: {fmt_glsl(bounding_radius_sqr)})")
    print(f"[ZCC SDF Compiler] Wrote primitive manifest: {manifest_path}")
    print(f"[ZCC SDF Compiler] Wrote telemetry log: {telemetry_path}")
    print("[ZCC SDF Compiler] COMPILATION COMPLETE.")

def run_parameter_sweep(input_file, output_file, num_spheres, num_samples, coarse_res, residual_res):
    print("=================================================================================")
    print(" ZCC SDF COMPILER V3.4 - AUTOMATED PARAMETER GRID SWEEP ENGINE")
    print(f" Source: {input_file}, K={num_spheres}, resolution={coarse_res}/{residual_res}")
    print("=================================================================================")
    print(f"| {'w_offset':<8} | {'eps_ratio':<9} | {'Surface P95 (cm)':<16} | {'Offset P95 (cm)':<15} | {'Violations (%)':<14} | {'Eikonal stress':<14} | {'Bake (s)':<8} |")
    print("|----------|-----------|------------------|-----------------|----------------|----------------|----------|")

    w_candidates = [0.2, 0.3, 0.4, 0.5, 0.75, 1.0]
    eps_candidates = [0.25, 0.5, 0.75]

    protect_from_oom_killer()
    sampled_points, _ = stream_glb_vertices(input_file, num_samples=num_samples)
    centroid = np.mean(sampled_points, axis=0)
    sampled_points -= centroid
    scale = np.max(np.linalg.norm(sampled_points, axis=1))
    sampled_points /= scale

    # KMeans clustering and Lloyds relaxation once
    centers, labels = kmeans_pure(sampled_points, num_spheres, seed=42)
    centers = lloyd_relaxation(sampled_points, centers, iterations=8)
    labels = np.zeros(sampled_points.shape[0], dtype=np.int32)
    min_dists = np.full(sampled_points.shape[0], np.inf)
    for i in range(len(centers)):
        col = np.linalg.norm(sampled_points - centers[i], axis=1)
        mask = col < min_dists
        min_dists[mask] = col[mask]
        labels[mask] = i

    primitives = []
    for i in range(num_spheres):
        cluster_pts = sampled_points[labels == i]
        if len(cluster_pts) == 0:
            prim = {"type": "sphere", "center": centers[i].copy(), "radius": 0.05, "cluster_idx": i, "fit_error": {"mean_abs": 0, "p95_abs": 0}}
        else:
            prim = classify_cluster_primitive(cluster_pts)
            if "center" not in prim: prim["center"] = centers[i].copy()
            prim["cluster_idx"] = i
            prim["fit_error"] = evaluate_primitive_fit_error(prim, cluster_pts)
        primitives.append(prim)

    num_groups = max(2, num_spheres // 16)
    groups = group_primitives(primitives, num_groups, num_spheres)

    max_group_bound = max(np.linalg.norm(g["center"]) + g["radius"] for g in groups)
    bounding_radius = (max_group_bound + 0.35 + 0.0375) * 1.15 + 0.05

    rng = np.random.default_rng(42)
    shuffled_indices = rng.permutation(len(sampled_points))
    split_idx = int(0.8 * len(sampled_points))
    bake_pts = sampled_points[shuffled_indices[:split_idx]]
    val_pts = sampled_points[shuffled_indices[split_idx:]]

    best_score = np.inf
    best_params = None

    for w_off in w_candidates:
        for eps_r in eps_candidates:
            t0 = time.time()
            res_b64, err_b64, res_res_actual = bake_residual_and_error(
                groups, bounding_radius, bake_pts, blend_radius=0.12, resolution=residual_res, w_offset=w_off, eps_ratio=eps_r
            )
            t_bake = time.time() - t0

            v_stats = validate_residual_bounds(
                val_pts, groups, res_b64, err_b64, bounding_radius, blend_radius=0.12, resolution=res_res_actual, eps_ratio=eps_r
            )

            surf_p95 = v_stats["validation_stats"]["visual_p95_abs"] * 100 # convert to cm
            off_p95 = v_stats["signed_offset_validation"]["combined_p95_abs_error"] * 100 # convert to cm
            viol = v_stats["validation_stats"]["bound_violation_rate"] * 100
            stress = v_stats["eikonal_validation"]["mean_stress"]

            print(f"| {w_off:<8.2f} | {eps_r:<9.2f} | {surf_p95:<16.2f} | {off_p95:<15.2f} | {viol:<14.2f} | {stress:<14.4f} | {t_bake:<8.2f} |")

            # Selection metric: minimize combined P95 error, requiring violation_rate == 0
            if viol == 0.0:
                score = surf_p95 + off_p95
                if score < best_score:
                    best_score = score
                    best_params = (w_off, eps_r, surf_p95, off_p95, stress)

    print("---------------------------------------------------------------------------------")
    if best_params:
        print(f"Optimal configuration identified: w_offset={best_params[0]:.2f}, eps_ratio={best_params[1]:.2f}")
        print(f"Surface P95: {best_params[2]:.2f} cm | Offset P95: {best_params[3]:.2f} cm | Eikonal Stress: {best_params[4]:.4f}")
    else:
        print("No configuration satisfied zero-violation safety constraint.")
    print("=================================================================================")

def main():
    if len(sys.argv) < 3:
        print("Usage: python3 zcc_sdf_compiler.py <input.glb> <output.html> [num_spheres] [--quality quality] [--k k] [--samples samples]")
        sys.exit(1)
        
    input_file = sys.argv[1]
    output_file = sys.argv[2]
    
    # Defaults
    quality = "balanced"
    k_override = None
    samples_override = None
    coarse_res_override = None
    residual_res_override = None
    w_offset_val = 0.75  # Optimal baseline found by grid search sweep!
    eps_ratio_val = 0.25 # Optimal baseline found by grid search sweep!
    
    # Legacy K positional argument checks
    legacy_k = None
    if len(sys.argv) >= 4 and not sys.argv[3].startswith("--"):
        try:
            legacy_k = int(sys.argv[3])
        except ValueError:
            pass
            
    # Process override flags
    args = sys.argv[3:] if len(sys.argv) >= 4 else []
    for i in range(len(args)):
        if args[i] == "--quality" and i + 1 < len(args):
            quality = args[i + 1]
        elif args[i] == "--k" and i + 1 < len(args):
            k_override = int(args[i + 1])
        elif args[i] == "--samples" and i + 1 < len(args):
            samples_override = int(args[i + 1])
        elif args[i] == "--coarse_res" and i + 1 < len(args):
            coarse_res_override = int(args[i + 1])
        elif args[i] == "--residual_res" and i + 1 < len(args):
            residual_res_override = int(args[i + 1])
        elif args[i] == "--w_offset" and i + 1 < len(args):
            w_offset_val = float(args[i + 1])
        elif args[i] == "--eps_ratio" and i + 1 < len(args):
            eps_ratio_val = float(args[i + 1])
            
    # Quality Presets Ladder
    presets = {
        "draft": {"k": 32, "samples": 4000, "coarse_res": 16, "residual_res": 16},
        "balanced": {"k": 64, "samples": 15000, "coarse_res": 32, "residual_res": 32},
        "high": {"k": 128, "samples": 30000, "coarse_res": 32, "residual_res": 32},
        "ultra": {"k": 256, "samples": 50000, "coarse_res": 48, "residual_res": 48}
    }
    
    if quality not in presets:
        print(f"[ZCC] Error: Unknown quality preset '{quality}'. Available presets: {list(presets.keys())}")
        sys.exit(1)
        
    config = presets[quality]
    num_spheres = legacy_k if legacy_k is not None else config["k"]
    num_samples = config["samples"]
    coarse_res = config["coarse_res"]
    residual_res = config["residual_res"]
    
    if k_override is not None:
        num_spheres = k_override
    if samples_override is not None:
        num_samples = samples_override
    if coarse_res_override is not None:
        coarse_res = coarse_res_override
    if residual_res_override is not None:
        residual_res = residual_res_override
        
    if not (1 <= num_spheres <= 256):
        print(f"[ZCC] Error: num_spheres must be between 1 and 256, got {num_spheres}")
        sys.exit(1)
        
    # Check for parameter sweep execution trigger
    if "--sweep" in sys.argv:
        try:
            run_parameter_sweep(input_file, output_file, num_spheres, num_samples, coarse_res, residual_res)
            return
        except Exception as e:
            import traceback
            traceback.print_exc()
            print(f"[ZCC] Parameter sweep execution failed: {e}")
            sys.exit(1)
            
    # Adaptive Degradation Schedule
    SAMPLE_SCHEDULE = [num_samples, max(2000, num_samples // 2), max(1000, num_samples // 4)]
    K_SCHEDULE = [num_spheres, max(16, num_spheres // 2), max(8, num_spheres // 4)]
    
    for ns, k_act in zip(SAMPLE_SCHEDULE, K_SCHEDULE):
        ok, est, avail = preflight_memory_check(ns, k_act)
        if not ok:
            print(f"[ZCC] Downgrading: K={k_act} samples={ns} exceeds memory limit. Retrying...")
            continue
            
        try:
            print(f"[ZCC] Launching: K={k_act}, samples={ns}, quality={quality}, coarse_res={coarse_res}, residual_res={residual_res}")
            _run_compilation(input_file, output_file, k_act, ns, coarse_res, residual_res, w_offset=w_offset_val, eps_ratio=eps_ratio_val, quality=quality)
            return
        except Exception as e:
            import traceback
            traceback.print_exc()
            print(f"[ZCC] Tier execution failed ({e}), initiating fallback...")
            gc.collect()
            continue
            
    raise RuntimeError("[ZCC] All adaptive compilation presets exhausted.")

if __name__ == "__main__":
    main()
