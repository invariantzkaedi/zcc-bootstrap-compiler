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

# WebGL2 Exclusivity Raymarching Template with Audio, UI uniforms, and modular zone functions
HTML_TEMPLATE = """<!DOCTYPE html>
<html>
<head>
    <title>ZCC Remediated SDF Raymarcher — V2.8 HARDENED EDITION</title>
    <style>
        body, html { margin: 0; padding: 0; overflow: hidden; background: #08080a; font-family: 'Courier New', monospace; color: #fff; }
        #canvas { width: 100vw; height: 100vh; display: block; }
        #ui { position: absolute; top: 20px; left: 20px; z-index: 10; background: rgba(10, 10, 15, 0.95); padding: 20px; border: 1px solid #ff00aa; border-radius: 8px; box-shadow: 0 0 20px rgba(255, 0, 170, 0.25); width: 280px; }
        h1 { font-size: 14px; margin: 0 0 12px 0; color: #ff00aa; text-shadow: 0 0 8px #ff00aa; text-transform: uppercase; letter-spacing: 1px; }
        p { margin: 4px 0; font-size: 11px; color: #888; }
        .control-group { margin: 14px 0; }
        label { display: block; font-size: 11px; margin-bottom: 4px; color: #00ffcc; }
        input[type=range] { width: 100%; accent-color: #ff00aa; background: #222; border-radius: 3px; height: 6px; outline: none; }
        button { width: 100%; background: #ff00aa; border: none; color: #fff; padding: 8px; border-radius: 4px; font-family: monospace; font-size: 12px; cursor: pointer; margin-top: 10px; font-weight: bold; }
        button:hover { background: #ff33bb; }
        #fps { position: absolute; bottom: 20px; right: 20px; background: rgba(5,5,5,0.8); border: 1px solid #333; padding: 5px 10px; font-size: 12px; color: #00ffcc; border-radius: 3px; }
        #error-log { color: #ff3333; font-size: 11px; margin-top: 10px; word-break: break-all; white-space: pre-wrap; font-family: monospace; }
    </style>
</head>
<body>
    <div id="ui">
        <h1>ZCC SDF V2.8</h1>
        <p>Asset: fleet_lite.glb</p>
        <p>Primitives: __NUM_PRIMITIVES__ Hybrid</p>
        <p>Resolution: Infinite</p>
        
        <div id="error-log"></div>

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
            </select>
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

        <button id="btn-audio">ACTIVATE MICROPHONE FFT</button>
    </div>
    
    <div id="fps">FPS: --</div>
    <canvas id="canvas"></canvas>
    
    <script>
        const canvas = document.getElementById('canvas');
        const errorLog = document.getElementById('error-log');
        
        // Assert WebGL2 context exclusively
        const gl = canvas.getContext('webgl2');
        if (!gl) {
            errorLog.textContent = 'Error: WebGL2 context not available.';
            throw new Error('WebGL2 context not available');
        }

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

            uniform highp sampler3D u_coarseSDF;

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

            // Fast distance-only evaluation (skips color interpolations to relieve register pressure)
            float mapD(vec3 p) {
                // Coarse SDF is baked in normalized world space before audio scale/jiggle.
                // Returning coarse before warp is intentional for conservative far-field steps.
                vec3 coarse_uv = p * (0.5 / __BOUND_RADIUS__) + 0.5;
                if (all(greaterThanEqual(coarse_uv, vec3(0.0))) && all(lessThanEqual(coarse_uv, vec3(1.0)))) {
                    float coarse = texture(u_coarseSDF, coarse_uv).r;
                    if (coarse > u_blendRadius + 0.15) {
                        return coarse;
                    }
                }

                float jiggle = u_jiggle + u_fftBands[0] * 1.5;
                float wave = sin(p.y * 10.0 + u_time * 6.0) * 0.015 * jiggle;
                p.x += wave;
                p.z += cos(p.y * 8.0 + u_time * 5.0) * 0.012 * jiggle;
                
                float scale = 1.0 + u_fftBands[1] * 0.15;
                p /= scale;

                float d = 1e5;
                __SDF_CODE_D__
                
                return d * scale; // Identical scale warp behavior
            }

            // Compiled geometry map function (returns vec4: x=distance, yzw=color)
            vec4 map(vec3 p) {
                // Coarse SDF is baked in normalized world space before audio scale/jiggle.
                // Returning coarse before warp is intentional for conservative far-field steps.
                vec3 coarse_uv = p * (0.5 / __BOUND_RADIUS__) + 0.5;
                if (all(greaterThanEqual(coarse_uv, vec3(0.0))) && all(lessThanEqual(coarse_uv, vec3(1.0)))) {
                    float coarse = texture(u_coarseSDF, coarse_uv).r;
                    if (coarse > u_blendRadius + 0.15) {
                        return vec4(coarse, 0.0, 0.0, 0.0);
                    }
                }

                // u_fftBands[0] modulates the procedural jiggle intensity
                float jiggle = u_jiggle + u_fftBands[0] * 1.5;
                float wave = sin(p.y * 10.0 + u_time * 6.0) * 0.015 * jiggle;
                p.x += wave;
                p.z += cos(p.y * 8.0 + u_time * 5.0) * 0.012 * jiggle;
                
                // u_fftBands[1] modulates the scale pulse
                float scale = 1.0 + u_fftBands[1] * 0.15;
                p /= scale;

                vec4 d = vec4(1e5, 0.0, 0.0, 0.0);
                __SDF_CODE__
                
                d.x *= scale; // Identical scale warp behavior
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
            vec3 getNormal(vec3 p) {
                const float h = 0.0005;
                const vec2 k = vec2(1.0, -1.0);
                vec3 g = k.xyy * mapD(p + k.xyy*h) +
                         k.yyx * mapD(p + k.yyx*h) +
                         k.yxy * mapD(p + k.yxy*h) +
                         k.xxx * mapD(p + k.xxx*h);
                float g2 = dot(g, g);
                return g2 > 1e-12 ? g * inversesqrt(g2) : vec3(0.0, 1.0, 0.0);
            }

            // Soft shadow tracing with bounding sphere ray reject
            float getShadow(vec3 ro, vec3 rd, float mint, float maxt, float k) {
                vec3 oc = ro;
                float b = dot(oc, rd);
                float c = dot(oc, oc) - __BOUND_RADIUS_SQR__;
                float h = b*b - c;
                if (h < 0.0) return 1.0; // Shadow ray misses scene entirely, fully lit
                float tShadowMax = min(maxt, -b + sqrt(h));
                if (tShadowMax < mint) return 1.0;

                float res = 1.0;
                float t = mint;
                for (int i = 0; i < 24; i++) {
                    float hd = mapD(ro + rd * t);
                    if (hd < 0.001) return 0.0;
                    res = min(res, k * hd / t);
                    t += clamp(hd, 0.01, 0.15);
                    if (t > tShadowMax) break;
                }
                return clamp(res, 0.0, 1.0);
            }

            // Cone-hemisphere ambient occlusion (robust golden-angle spread)
            float getAO(vec3 p, vec3 n) {
                // Robust tangent frame: choose hint axis farthest from normal
                vec3 hint = abs(n.y) < 0.9 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
                vec3 t = normalize(cross(n, hint));
                vec3 b = cross(n, t);
                float occ = 0.0;
                float sca = 1.0;
                for (int i = 0; i < 5; i++) {
                    float fi = float(i);
                    float hr = 0.01 + 0.12 * fi / 4.0;
                    float angle = fi * 2.39996; // Golden angle
                    vec3 dir = normalize(n + 0.4 * (cos(angle)*t + sin(angle)*b));
                    float dd = mapD(p + dir * hr);
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
                    
                    for (int i = 0; i < 128; i++) { // Detail tracing
                        march_steps = i;
                        p = ro + rd * t;
                        float dist = mapD(p); // March using fast distance-only pathway
                        if (dist < 0.0008) {
                            hit = true;
                            break;
                        }
                        t += max(dist * 0.95, 0.0002); // Safety step floor
                        if (t > tmax) break;
                    }

                    if (hit) {
                        map_res = map(p); // Evaluate color exactly once on hit
                        vec3 n = getNormal(p);
                        vec3 l = normalize(vec3(2.5, 4.0, 3.0));
                        vec3 r = reflect(-l, n);
                        vec3 v = -rd;

                        float diffuse = max(dot(n, l), 0.0);
                        float specular = pow(max(dot(r, v), 0.0), u_specularPower);
                        
                        float ao = getAO(p, n);
                        float shadow = getShadow(p + n * 0.005, l, 0.015, 4.0, 16.0);
                        
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
                            float stepsNorm = float(march_steps) / 128.0;
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
                        }
                    }
                }
                
                // Raymiss step heatmap visualization
                if (!hit && u_debugMode == 3) {
                    float stepsNorm = float(march_steps) / 128.0;
                    color = mix(vec3(0.0, 0.2, 1.0), vec3(1.0, 0.0, 0.0), stepsNorm);
                }

                fragColor = vec4(pow(color, vec3(1.0/2.2)), 1.0);
            }
        `;

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

        const program = gl.createProgram();
        const vs = createShader(gl, gl.VERTEX_SHADER, vsSource);
        const fs = createShader(gl, gl.FRAGMENT_SHADER, fsSource);
        gl.attachShader(program, vs);
        gl.attachShader(program, fs);
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

        // Upload baked Coarse SDF 3D Texture (RTX 5070 GDDR7 672 GB/s pipeline target)
        const coarseSdfB64 = "__COARSE_SDF_B64__";
        const coarseSdfRes = __COARSE_SDF_RES__;
        const coarseBytes = Uint8Array.from(atob(coarseSdfB64), c => c.charCodeAt(0));
        const coarseData = new Float32Array(coarseBytes.buffer);

        const coarseTex = gl.createTexture();
        gl.activeTexture(gl.TEXTURE0);
        gl.bindTexture(gl.TEXTURE_3D, coarseTex);
        gl.texImage3D(gl.TEXTURE_3D, 0, gl.R32F,
            coarseSdfRes, coarseSdfRes, coarseSdfRes,
            0, gl.RED, gl.FLOAT, coarseData);
            
        // Native WebGL2 LINEAR sampling support
        gl.texParameteri(gl.TEXTURE_3D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
        gl.texParameteri(gl.TEXTURE_3D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
        gl.texParameteri(gl.TEXTURE_3D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
        gl.texParameteri(gl.TEXTURE_3D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
        gl.texParameteri(gl.TEXTURE_3D, gl.TEXTURE_WRAP_R, gl.CLAMP_TO_EDGE);
        
        gl.uniform1i(gl.getUniformLocation(program, 'u_coarseSDF'), 0);

        const positionLoc = gl.getAttribLocation(program, 'position');
        const buffer = gl.createBuffer();
        gl.bindBuffer(gl.ARRAY_BUFFER, buffer);
        gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([
            -1, -1,  1, -1, -1,  1,
            -1,  1,  1, -1,  1,  1
        ]), gl.STATIC_DRAW);
        gl.enableVertexAttribArray(positionLoc);
        gl.vertexAttribPointer(positionLoc, 2, gl.FLOAT, false, 0, 0);

        const resLoc = gl.getUniformLocation(program, 'u_resolution');
        const mouseLoc = gl.getUniformLocation(program, 'u_mouse');
        const timeLoc = gl.getUniformLocation(program, 'u_time');
        const blendLoc = gl.getUniformLocation(program, 'u_blendRadius');
        const jiggleLoc = gl.getUniformLocation(program, 'u_jiggle');
        const glowLoc = gl.getUniformLocation(program, 'u_glow');
        const specularLoc = gl.getUniformLocation(program, 'u_specularPower');
        const fftBandsLoc = gl.getUniformLocation(program, 'u_fftBands');
        const debugLoc = gl.getUniformLocation(program, 'u_debugMode');

        const debugSelect = document.getElementById('select-debug');
        const blendSlider = document.getElementById('slider-blend');
        const jiggleSlider = document.getElementById('slider-jiggle');
        const glowSlider = document.getElementById('slider-glow');
        const specSlider = document.getElementById('slider-specular');

        const blendVal = document.getElementById('val-blend');
        const jiggleVal = document.getElementById('val-jiggle');
        const glowVal = document.getElementById('val-glow');
        const specVal = document.getElementById('val-specular');

        blendSlider.addEventListener('input', (e) => blendVal.textContent = parseFloat(e.target.value).toFixed(2));
        jiggleSlider.addEventListener('input', (e) => jiggleVal.textContent = parseFloat(e.target.value).toFixed(1));
        glowSlider.addEventListener('input', (e) => glowVal.textContent = parseFloat(e.target.value).toFixed(1));
        specSlider.addEventListener('input', (e) => specVal.textContent = parseFloat(e.target.value).toFixed(1));

        let mouse = [0.5, 0.5];
        window.addEventListener('mousemove', (e) => {
            mouse = [e.clientX / window.innerWidth, 1.0 - (e.clientY / window.innerHeight)];
        });

        function resize() {
            canvas.width = window.innerWidth;
            canvas.height = window.innerHeight;
            gl.viewport(0, 0, canvas.width, canvas.height);
        }
        window.addEventListener('resize', resize);
        resize();

        // Web Audio API FFT setup (256 FFT size for 128 frequency bins, 16 bins per band)
        let audioCtx = null;
        let analyser = null;
        let dataArray = null;
        let audioActive = false;
        let fftBands = new Float32Array(8);

        document.getElementById('btn-audio').addEventListener('click', () => {
            if (audioActive) return;
            navigator.mediaDevices.getUserMedia({ audio: true, video: false })
                .then(stream => {
                    audioCtx = new (window.AudioContext || window.webkitAudioContext)();
                    const source = audioCtx.createMediaStreamSource(stream);
                    analyser = audioCtx.createAnalyser();
                    analyser.fftSize = 256;
                    source.connect(analyser);
                    dataArray = new Uint8Array(analyser.frequencyBinCount);
                    audioActive = true;
                    document.getElementById('btn-audio').textContent = 'AUDIO ACTIVE';
                    document.getElementById('btn-audio').style.background = '#00ffcc';
                    document.getElementById('btn-audio').style.color = '#000';
                })
                .catch(err => {
                    console.warn('Audio input acquisition failed:', err);
                    errorLog.textContent = 'Audio permission denied or unavailable.';
                });
        });

        const fpsCounter = document.getElementById('fps');
        let lastTime = 0;
        let frameCount = 0;

        function render(time) {
            frameCount++;
            if (time - lastTime >= 1000) {
                fpsCounter.textContent = `FPS: ${frameCount}`;
                frameCount = 0;
                lastTime = time;
            }

            if (audioActive && analyser) {
                analyser.getByteFrequencyData(dataArray);
                const binWidth = Math.floor(dataArray.length / 8); // 128 / 8 = 16 bins per band
                for (let i = 0; i < 8; i++) {
                    let sum = 0;
                    for (let j = 0; j < binWidth; j++) {
                        sum += dataArray[i * binWidth + j];
                    }
                    fftBands[i] = (sum / binWidth) / 255.0; // scale 0-1
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

            gl.drawArrays(gl.TRIANGLES, 0, 6);
            requestAnimationFrame(render);
        }
        requestAnimationFrame(render);
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
    On RTX 5070 (672 GB/s GDDR7): one texture fetch replaces all zone bounding tests.
    Cost: resolution³ × 4 bytes = 32³ × 4 = 131 KB
    """
    MAX_BAKE_RESOLUTION = 48
    resolution = min(resolution, MAX_BAKE_RESOLUTION)
    
    grid = np.full((resolution, resolution, resolution), 1e5, dtype=np.float32)
    
    # Direct mgrid allocation (single allocation, no intermediate arrays)
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

def _run_compilation(input_file, output_file, num_spheres, num_samples):
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
                # Fallback center directly if cluster is empty to prevent NaN calculations
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
                prim["cluster_idx"] = i # Track original index for stable colors
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
    if fs_estimated_len > GLSL_STAGE_LIMIT:
        print(f"[ZCC] WARNING: Fragment shader ~{fs_estimated_len} chars may exceed "
              f"WebGL2 limit of {GLSL_STAGE_LIMIT} on some drivers. "
              f"Consider UBO emission for K>{num_spheres}.")

    # Bake Coarse 3D SDF Texture grid wrapped in MemoryTelemetry
    print("[ZCC SDF Compiler] Baking Coarse 3D SDF Texture...")
    t0 = time.time()
    with MemoryTelemetry("Coarse SDF Bake", warn_mb=50, abort_mb=100) as tm_bake:
        coarse_b64, coarse_res = bake_coarse_sdf_b64(groups, bounding_radius, resolution=32)
    t_bake = time.time() - t0

    print(f"[ZCC SDF Compiler] Emitting WebGL compilation target to {output_file}...")
    html_content = HTML_TEMPLATE
    html_content = html_content.replace("__NUM_PRIMITIVES__", str(num_spheres))
    html_content = html_content.replace("__ZONE_FUNCTIONS__", zone_glsl_definitions)
    html_content = html_content.replace("__SDF_CODE__", sdf_code_str)
    html_content = html_content.replace("__SDF_CODE_D__", sdf_code_str_d)
    html_content = html_content.replace("__CAMERA_DIST__", "2.5")
    html_content = html_content.replace("__CENTER_Y__", "0.0")
    html_content = html_content.replace("__BOUND_RADIUS__", fmt_glsl(bounding_radius))
    html_content = html_content.replace("__BOUND_RADIUS_SQR__", fmt_glsl(bounding_radius_sqr))
    html_content = html_content.replace("__COARSE_SDF_B64__", coarse_b64)
    html_content = html_content.replace("__COARSE_SDF_RES__", str(coarse_res))

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
        "version": "2.8",
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
        ]
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
        "passes": {
            "stream_parser_seconds": t_parser,
            "kmeans_clustering_seconds": t_kmeans,
            "pca_classification_seconds": t_pca,
            "hierarchical_grouping_seconds": t_group,
            "coarse_sdf_bake_seconds": t_bake
        }
    }
    with open(telemetry_path, "w", encoding="utf-8") as f_telemetry:
        json.dump(telemetry_data, f_telemetry, indent=2)
        
    print(f"[ZCC SDF Compiler] Dynamic Bounding Radius: {fmt_glsl(bounding_radius)} (Square: {fmt_glsl(bounding_radius_sqr)})")
    print(f"[ZCC SDF Compiler] Wrote primitive manifest: {manifest_path}")
    print(f"[ZCC SDF Compiler] Wrote telemetry log: {telemetry_path}")
    print("[ZCC SDF Compiler] COMPILATION COMPLETE.")

def main():
    if len(sys.argv) < 3:
        print("Usage: python3 zcc_sdf_compiler.py <input.glb> <output.html> [num_spheres]")
        sys.exit(1)
        
    input_file = sys.argv[1]
    output_file = sys.argv[2]
    
    num_spheres = 64
    if len(sys.argv) >= 4:
        try:
            num_spheres = int(sys.argv[3])
        except ValueError:
            print(f"[ZCC] Error: num_spheres must be an integer, got '{sys.argv[3]}'")
            sys.exit(1)
            
        if not (1 <= num_spheres <= 256):
            print(f"[ZCC] Error: num_spheres must be between 1 and 256, got {num_spheres}")
            sys.exit(1)
        
    # Adaptive Degradation Schedules
    SAMPLE_SCHEDULE = [15000, 8000, 4000, 2000]
    K_SCHEDULE = [num_spheres, min(48, num_spheres), min(32, num_spheres), min(16, num_spheres)]
    
    for num_samples, k_actual in zip(SAMPLE_SCHEDULE, K_SCHEDULE):
        ok, est, avail = preflight_memory_check(num_samples, k_actual)
        if not ok:
            print(f"[ZCC] Downgrading: K={k_actual} samples={num_samples} requires too much memory. Retrying next tier...")
            continue
            
        try:
            print(f"[ZCC] Launching: K={k_actual}, samples={num_samples}")
            _run_compilation(input_file, output_file, k_actual, num_samples)
            return
        except Exception as e:
            import traceback
            traceback.print_exc()
            print(f"[ZCC] Tier execution failed ({e}), initiating fallback...")
            gc.collect()
            continue
            
    raise RuntimeError("[ZCC] All adaptive compilation tiers exhausted due to memory limits.")

if __name__ == "__main__":
    main()
