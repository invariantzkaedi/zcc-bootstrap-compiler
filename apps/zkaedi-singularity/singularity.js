/**
 * ZKAEDI Singularity Studio Engine
 * WebGL 2.0 GPGPU 2,097,152 Particle Synthesizer + Audio Reactivity + Video Recorder + C Code Exporter
 */

class SingularityStudio {
    constructor() {
        this.canvas = document.getElementById('singularityCanvas');
        this.gl = this.canvas.getContext('webgl2', { preserveDrawingBuffer: true, antialias: false });
        if (!this.gl) {
            this.gl = this.canvas.getContext('webgl', { preserveDrawingBuffer: true });
        }

        this.texWidth = 1024;
        this.texHeight = 2048;
        this.maxParticles = this.texWidth * this.texHeight; // 2,097,152
        this.activeParticles = 2000000;

        this.params = {
            a: -1.40,
            b: 1.60,
            c: 1.00,
            d: 0.70,
            eta: 0.40,
            gamma: 0.30,
            decay: 0.78,
            dist: 4.2,
            fov: 680.0,
            theme: 'rhodium',
            autoMorph: true,
            bassReactivity: 1.5,
            trebleReactivity: 1.0,
            pointSize: 1.0,
            depth: 1.2
        };

        this.camera = {
            pitch: 0.2,
            yaw: 0.5,
            zoom: 1.0,
            isDragging: false,
            lastX: 0,
            lastY: 0
        };

        this.time = 0.0;
        this.fps = 60;
        this.frameCount = 0;
        this.lastTime = performance.now();

        // Audio Reactive State
        this.audioCtx = null;
        this.analyser = null;
        this.audioData = null;
        this.isAudioActive = false;
        this.bassEnergy = 0.0;
        this.trebleEnergy = 0.0;

        // Video Recorder State
        this.mediaRecorder = null;
        this.recordedChunks = [];
        this.isRecording = false;

        this.init();
        this.setupAudio();
        this.setupUI();
    }

    init() {
        const gl = this.gl;
        if (!gl) return;

        gl.getExtension('OES_texture_float');
        gl.getExtension('EXT_color_buffer_float');

        this.resize();
        this.initShaders();
        this.initBuffers();
        this.initTextures();
    }

    resize() {
        const rect = this.canvas.parentElement.getBoundingClientRect();
        this.canvas.width = rect.width * (window.devicePixelRatio || 1);
        this.canvas.height = rect.height * (window.devicePixelRatio || 1);
        this.gl.viewport(0, 0, this.canvas.width, this.canvas.height);
    }

    initShaders() {
        const gl = this.gl;

        // Sim Shader (Computes non-linear recursion on GPU)
        const simVS = `#version 300 es
        in vec2 position;
        out vec2 vUv;
        void main() {
            vUv = position * 0.5 + 0.5;
            gl_Position = vec4(position, 0.0, 1.0);
        }`;

        const simFS = `#version 300 es
        precision highp float;
        uniform sampler2D uPosTex;
        uniform float uA, uB, uC, uD;
        uniform float uEta, uGamma;
        uniform float uBassMod, uTrebleMod;
        uniform float uTime;
        in vec2 vUv;
        out vec4 outPos;

        float sigmoid(float z) {
            return 1.0 / (1.0 + exp(-clamp(z, -15.0, 15.0)));
        }

        void main() {
            vec4 curr = texture(uPosTex, vUv);
            float x = curr.x;
            float y = curr.y;

            float effA = uA * (1.0 + uBassMod * 0.15);
            float effEta = uEta * (1.0 + uTrebleMod * 0.2);

            float h_base_x = sin(effA * y) + uC * cos(effA * x);
            float h_base_y = sin(uB * x) + uD * cos(uB * y);

            float sig_x = sigmoid(uGamma * y);
            float sig_y = sigmoid(uGamma * x);

            float nx = h_base_x + effEta * x * sig_x;
            float ny = h_base_y + effEta * y * sig_y;

            float nz = sin(nx * ny * 0.6) * 1.2;

            if (abs(nx) > 8.0 || abs(ny) > 8.0 || isnan(nx) || isnan(ny)) {
                nx = sin(vUv.x * 123.456 + uTime) * 0.5;
                ny = cos(vUv.y * 654.321 + uTime) * 0.5;
                nz = 0.0;
            }

            outPos = vec4(nx, ny, nz, 1.0);
        }`;

        this.simProg = this.createProgram(simVS, simFS);

        // Render Shader (3D Projection with Additive Caustics)
        const renderVS = `#version 300 es
        in float aIndex;
        uniform sampler2D uPosTex;
        uniform vec2 uTexSize;
        uniform vec2 uScreenSize;
        uniform float uPitch, uYaw, uZoom;
        uniform float uDist, uFov;
        uniform float uPointSize, uDepthScale;
        uniform int uTheme;
        uniform float uTime;
        out vec4 vColor;

        void main() {
            float xIdx = mod(aIndex, uTexSize.x);
            float yIdx = floor(aIndex / uTexSize.x);
            vec2 uv = (vec2(xIdx, yIdx) + 0.5) / uTexSize;

            vec4 pos = texture(uPosTex, uv);
            float x = pos.x;
            float y = pos.y;
            float z = pos.z * uDepthScale;

            float cp = cos(uPitch), sp = sin(uPitch);
            float cy = cos(uYaw),   sy = sin(uYaw);

            float x1 = x * cy - z * sy;
            float z1 = x * sy + z * cy;
            float y1 = y * cp - z1 * sp;
            float z2 = y * sp + z1 * cp;

            float dist = (uDist + z2 * 0.45) / uZoom;
            if (dist < 0.4) dist = 0.4;
            float fov = uFov / dist;

            vec2 screenPos = vec2(x1 * fov, y1 * fov) / (uScreenSize * 0.5);
            gl_Position = vec4(screenPos, 0.0, 1.0);
            gl_PointSize = uPointSize;

            float t = uTime * 0.8;
            float cr = 0.5 + 0.5 * sin(t + 0.0);
            float cg = 0.5 + 0.5 * sin(t + 2.094);
            float cb = 0.5 + 0.5 * sin(t + 4.188);
            float depth = clamp((z2 + 2.0) / 4.0, 0.25, 1.0);

            if (uTheme == 0) {
                // Rhodium Cyberpunk
                vColor = vec4(cr * 0.45 * depth, cg * 0.85 * depth, cb * 1.0 * depth, 0.8);
            } else if (uTheme == 1) {
                // Solar Gold & Obsidian
                vColor = vec4(1.0 * depth, 0.72 * depth, 0.15 * depth, 0.9);
            } else if (uTheme == 2) {
                // Hyper Spectral
                vColor = vec4(cr * depth, cg * depth, cb * depth, 0.85);
            } else if (uTheme == 3) {
                // Emerald
                vColor = vec4(0.1 * depth, 1.0 * depth, 0.65 * depth, 0.85);
            } else {
                // Infrared
                vColor = vec4(1.0 * depth, 0.2 * depth, 0.4 * depth, 0.9);
            }
        }`;

        const renderFS = `#version 300 es
        precision highp float;
        in vec4 vColor;
        out vec4 fragColor;
        void main() {
            fragColor = vColor;
        }`;

        this.renderProg = this.createProgram(renderVS, renderFS);
    }

    createProgram(vsSrc, fsSrc) {
        const gl = this.gl;
        const vs = gl.createShader(gl.VERTEX_SHADER);
        gl.shaderSource(vs, vsSrc);
        gl.compileShader(vs);

        const fs = gl.createShader(gl.FRAGMENT_SHADER);
        gl.shaderSource(fs, fsSrc);
        gl.compileShader(fs);

        const prog = gl.createProgram();
        gl.attachShader(prog, vs);
        gl.attachShader(prog, fs);
        gl.linkProgram(prog);
        return prog;
    }

    initBuffers() {
        const gl = this.gl;
        const quadVerts = new Float32Array([-1, -1, 1, -1, -1, 1, 1, 1]);
        this.quadVBO = gl.createBuffer();
        gl.bindBuffer(gl.ARRAY_BUFFER, this.quadVBO);
        gl.bufferData(gl.ARRAY_BUFFER, quadVerts, gl.STATIC_DRAW);

        const indices = new Float32Array(this.maxParticles);
        for (let i = 0; i < this.maxParticles; i++) indices[i] = i;
        this.particleVBO = gl.createBuffer();
        gl.bindBuffer(gl.ARRAY_BUFFER, this.particleVBO);
        gl.bufferData(gl.ARRAY_BUFFER, indices, gl.STATIC_DRAW);
    }

    initTextures() {
        const gl = this.gl;
        const initialData = new Float32Array(this.texWidth * this.texHeight * 4);
        for (let i = 0; i < this.maxParticles; i++) {
            const idx = i * 4;
            const r = Math.random() * 0.5;
            const th = Math.random() * Math.PI * 2;
            initialData[idx] = Math.cos(th) * r;
            initialData[idx + 1] = Math.sin(th) * r;
            initialData[idx + 2] = 0.0;
            initialData[idx + 3] = 1.0;
        }

        this.posTextures = [gl.createTexture(), gl.createTexture()];
        this.fbo = [gl.createFramebuffer(), gl.createFramebuffer()];

        for (let i = 0; i < 2; i++) {
            gl.bindTexture(gl.TEXTURE_2D, this.posTextures[i]);
            gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA32F, this.texWidth, this.texHeight, 0, gl.RGBA, gl.FLOAT, initialData);
            gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
            gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
            gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
            gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);

            gl.bindFramebuffer(gl.FRAMEBUFFER, this.fbo[i]);
            gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, this.posTextures[i], 0);
        }
        this.currentFBO = 0;
    }

    setupAudio() {
        const btnToggle = document.getElementById('btnAudioToggle');
        btnToggle.addEventListener('click', async () => {
            if (!this.audioCtx) {
                this.audioCtx = new (window.AudioContext || window.webkitAudioContext)();
                this.analyser = this.audioCtx.createAnalyser();
                this.analyser.fftSize = 128;
                this.audioData = new Uint8Array(this.analyser.frequencyBinCount);
            }

            if (!this.isAudioActive) {
                try {
                    const stream = await navigator.mediaDevices.getUserMedia({ audio: true });
                    const src = this.audioCtx.createMediaStreamSource(stream);
                    src.connect(this.analyser);
                    this.isAudioActive = true;
                    btnToggle.classList.add('active');
                    btnToggle.textContent = '🔊 Mic Active';
                } catch (err) {
                    alert("Microphone access denied or unavailable: " + err.message);
                }
            } else {
                this.isAudioActive = false;
                btnToggle.classList.remove('active');
                btnToggle.textContent = '🎤 Audio Reactive';
            }
        });

        // Load Audio File
        const fileAudio = document.getElementById('fileAudio');
        fileAudio.addEventListener('change', (e) => {
            const file = e.target.files[0];
            if (!file) return;
            if (!this.audioCtx) {
                this.audioCtx = new (window.AudioContext || window.webkitAudioContext)();
                this.analyser = this.audioCtx.createAnalyser();
                this.analyser.fftSize = 128;
                this.audioData = new Uint8Array(this.analyser.frequencyBinCount);
            }
            const reader = new FileReader();
            reader.onload = (ev) => {
                this.audioCtx.decodeAudioData(ev.target.result, (buf) => {
                    const src = this.audioCtx.createBufferSource();
                    src.buffer = buf;
                    src.loop = true;
                    src.connect(this.analyser);
                    this.analyser.connect(this.audioCtx.destination);
                    src.start(0);
                    this.isAudioActive = true;
                    btnToggle.classList.add('active');
                    btnToggle.textContent = `🎵 ${file.name.substring(0, 10)}...`;
                });
            };
            reader.readAsArrayBuffer(file);
        });
    }

    updateAudio() {
        if (!this.isAudioActive || !this.analyser) {
            this.bassEnergy = 0.0;
            this.trebleEnergy = 0.0;
            return;
        }

        this.analyser.getByteFrequencyData(this.audioData);

        // Low frequency bins (0..4) = Bass
        let bassSum = 0;
        for (let i = 0; i < 4; i++) bassSum += this.audioData[i];
        this.bassEnergy = (bassSum / (4 * 255)) * this.params.bassReactivity;

        // High frequency bins (16..32) = Treble
        let trebleSum = 0;
        for (let i = 16; i < 32; i++) trebleSum += this.audioData[i];
        this.trebleEnergy = (trebleSum / (16 * 255)) * this.params.trebleReactivity;

        // Draw mini HUD viz
        const miniCanvas = document.getElementById('audioMiniViz');
        if (miniCanvas) {
            const ctx = miniCanvas.getContext('2d');
            ctx.clearRect(0, 0, miniCanvas.width, miniCanvas.height);
            ctx.fillStyle = '#00f3ff';
            const w = miniCanvas.width / 16;
            for (let i = 0; i < 16; i++) {
                const h = (this.audioData[i] / 255) * miniCanvas.height;
                ctx.fillRect(i * w, miniCanvas.height - h, w - 1, h);
            }
        }

        // Draw full spectrum viz in tab
        const fullCanvas = document.getElementById('audioFullViz');
        if (fullCanvas) {
            const ctx = fullCanvas.getContext('2d');
            ctx.fillStyle = 'rgba(0, 0, 0, 0.4)';
            ctx.fillRect(0, 0, fullCanvas.width, fullCanvas.height);
            const w = fullCanvas.width / this.audioData.length;
            for (let i = 0; i < this.audioData.length; i++) {
                const h = (this.audioData[i] / 255) * fullCanvas.height;
                ctx.fillStyle = `hsl(${180 + i * 2}, 100%, 50%)`;
                ctx.fillRect(i * w, fullCanvas.height - h, w - 1, h);
            }
        }
    }

    setupUI() {
        const canvas = this.canvas;
        canvas.addEventListener('mousedown', (e) => {
            this.camera.isDragging = true;
            this.camera.lastX = e.clientX;
            this.camera.lastY = e.clientY;
        });

        window.addEventListener('mousemove', (e) => {
            if (!this.camera.isDragging) return;
            const dx = e.clientX - this.camera.lastX;
            const dy = e.clientY - this.camera.lastY;
            this.camera.yaw += dx * 0.006;
            this.camera.pitch += dy * 0.006;
            this.camera.lastX = e.clientX;
            this.camera.lastY = e.clientY;
        });

        window.addEventListener('mouseup', () => this.camera.isDragging = false);

        canvas.addEventListener('wheel', (e) => {
            e.preventDefault();
            this.camera.zoom *= e.deltaY > 0 ? 0.92 : 1.08;
            this.camera.zoom = Math.max(0.3, Math.min(this.camera.zoom, 5.0));
        }, { passive: false });

        window.addEventListener('resize', () => this.resize());

        // Deck Tabs
        document.querySelectorAll('.deck-tab').forEach(tab => {
            tab.addEventListener('click', () => {
                document.querySelectorAll('.deck-tab').forEach(t => t.classList.remove('active'));
                document.querySelectorAll('.deck-content').forEach(c => c.classList.remove('active'));
                tab.classList.add('active');
                document.getElementById(tab.dataset.tab).classList.add('active');
            });
        });

        // Sliders
        const sliderParticles = document.getElementById('sliderParticles');
        sliderParticles.addEventListener('input', (e) => {
            this.activeParticles = parseInt(e.target.value);
            document.getElementById('lblParticles').textContent = this.activeParticles.toLocaleString();
            document.getElementById('hudNodes').textContent = this.activeParticles.toLocaleString();
        });

        const sliderEta = document.getElementById('sliderEta');
        sliderEta.addEventListener('input', (e) => {
            this.params.eta = parseFloat(e.target.value);
            document.getElementById('lblEta').textContent = this.params.eta.toFixed(2);
            const hudRegime = document.getElementById('hudRegime');
            if (this.params.eta < 1.0) {
                hudRegime.textContent = `SUB-CRITICAL`;
                hudRegime.style.color = 'var(--neon-cyan)';
            } else if (this.params.eta <= 1.05) {
                hudRegime.textContent = `BIFURCATION`;
                hudRegime.style.color = 'var(--neon-gold)';
            } else {
                hudRegime.textContent = `SUPERCRITICAL`;
                hudRegime.style.color = 'var(--neon-magenta)';
            }
        });

        document.getElementById('sliderGamma').addEventListener('input', (e) => {
            this.params.gamma = parseFloat(e.target.value);
            document.getElementById('lblGamma').textContent = this.params.gamma.toFixed(2);
        });

        document.getElementById('sliderDist').addEventListener('input', (e) => {
            this.params.dist = parseFloat(e.target.value);
            document.getElementById('lblDist').textContent = this.params.dist.toFixed(1);
        });

        document.getElementById('sliderDecay').addEventListener('input', (e) => {
            this.params.decay = parseFloat(e.target.value);
            document.getElementById('lblDecay').textContent = this.params.decay.toFixed(2);
        });

        document.getElementById('sliderDepth').addEventListener('input', (e) => {
            this.params.depth = parseFloat(e.target.value);
            document.getElementById('lblDepth').textContent = `${this.params.depth.toFixed(1)}x`;
        });

        document.getElementById('sliderPointSize').addEventListener('input', (e) => {
            this.params.pointSize = parseFloat(e.target.value);
            document.getElementById('lblPointSize').textContent = `${this.params.pointSize.toFixed(1)} px`;
        });

        document.getElementById('sliderBass').addEventListener('input', (e) => {
            this.params.bassReactivity = parseFloat(e.target.value);
            document.getElementById('lblBass').textContent = `${this.params.bassReactivity.toFixed(1)}x`;
        });

        document.getElementById('sliderTreble').addEventListener('input', (e) => {
            this.params.trebleReactivity = parseFloat(e.target.value);
            document.getElementById('lblTreble').textContent = `${this.params.trebleReactivity.toFixed(1)}x`;
        });

        document.getElementById('selTheme').addEventListener('change', (e) => {
            this.params.theme = e.target.value;
        });

        const chkAuto = document.getElementById('chkAutoMorph');
        chkAuto.addEventListener('change', (e) => {
            this.params.autoMorph = e.target.checked;
        });

        // Presets
        document.getElementById('selPreset').addEventListener('change', (e) => {
            const p = e.target.value;
            chkAuto.checked = false;
            this.params.autoMorph = false;
            if (p === 'canonical') {
                this.params.a = -1.40; this.params.b = 1.60; this.params.c = 1.00; this.params.d = 0.70;
                this.params.eta = 0.40; this.params.gamma = 0.30;
                chkAuto.checked = true; this.params.autoMorph = true;
            } else if (p === 'supercritical') {
                this.params.a = -1.56; this.params.b = 1.73; this.params.c = 1.16; this.params.d = 0.51;
                this.params.eta = 0.40;
            } else if (p === 'limitcycle') {
                this.params.a = -1.13; this.params.b = 1.56; this.params.c = 1.07; this.params.d = 0.50;
                this.params.eta = 0.40;
            } else if (p === 'vortex') {
                this.params.a = -1.80; this.params.b = 1.20; this.params.c = 0.90; this.params.d = 1.10;
                this.params.eta = 0.65;
            } else if (p === 'hopf') {
                this.params.a = -0.95; this.params.b = 1.85; this.params.c = 1.30; this.params.d = 0.40;
                this.params.eta = 0.35;
            } else if (p === 'rossler') {
                this.params.a = -2.00; this.params.b = 1.40; this.params.c = 0.80; this.params.d = 1.20;
                this.params.eta = 0.50;
            } else if (p === 'de_jong') {
                this.params.a = 1.40; this.params.b = -2.30; this.params.c = 2.40; this.params.d = -2.10;
                this.params.eta = 0.20;
            }
            sliderEta.value = this.params.eta;
            document.getElementById('lblEta').textContent = this.params.eta.toFixed(2);
        });

        document.getElementById('btnResetCamera').addEventListener('click', () => {
            this.camera.pitch = 0.2;
            this.camera.yaw = 0.5;
            this.camera.zoom = 1.0;
        });

        // Snapshot
        document.getElementById('btnSnapshot').addEventListener('click', () => {
            const link = document.createElement('a');
            link.download = `zkaedi_singularity_vfx_${Date.now()}.png`;
            link.href = this.canvas.toDataURL('image/png');
            link.click();
        });

        // Video Recorder
        const btnRecord = document.getElementById('btnRecordVideo');
        btnRecord.addEventListener('click', () => {
            if (!this.isRecording) {
                const stream = this.canvas.captureStream(60);
                this.mediaRecorder = new MediaRecorder(stream, { mimeType: 'video/webm;codecs=vp9' });
                this.recordedChunks = [];
                this.mediaRecorder.ondataavailable = (e) => {
                    if (e.data.size > 0) this.recordedChunks.push(e.data);
                };
                this.mediaRecorder.onstop = () => {
                    const blob = new Blob(this.recordedChunks, { type: 'video/webm' });
                    const url = URL.createObjectURL(blob);
                    const a = document.createElement('a');
                    a.href = url;
                    a.download = `zkaedi_singularity_60fps_${Date.now()}.webm`;
                    a.click();
                };
                this.mediaRecorder.start();
                this.isRecording = true;
                btnRecord.classList.add('active');
                document.getElementById('recBtnText').textContent = 'Stop Recording';
            } else {
                this.mediaRecorder.stop();
                this.isRecording = false;
                btnRecord.classList.remove('active');
                document.getElementById('recBtnText').textContent = 'Record Video';
            }
        });

        // Modal C Code Export
        const modal = document.getElementById('modalExportC');
        const txtCCode = document.getElementById('txtCCode');
        document.getElementById('btnExportC').addEventListener('click', () => {
            txtCCode.value = this.generateCCode();
            modal.classList.add('open');
        });
        document.getElementById('btnCloseModal').addEventListener('click', () => modal.classList.remove('open'));
        document.getElementById('btnCopyC').addEventListener('click', () => {
            navigator.clipboard.writeText(txtCCode.value);
            alert("Native C source code copied to clipboard!");
        });
        document.getElementById('btnDownloadC').addEventListener('click', () => {
            const blob = new Blob([txtCCode.value], { type: 'text/x-csrc' });
            const a = document.createElement('a');
            a.href = URL.createObjectURL(blob);
            a.download = 'zkaedi_singularity_vfx.c';
            a.click();
        });

        // Coefficient tick
        setInterval(() => {
            document.getElementById('valA').textContent = this.params.a.toFixed(2);
            document.getElementById('valB').textContent = this.params.b.toFixed(2);
            document.getElementById('valC').textContent = this.params.c.toFixed(2);
            document.getElementById('valD').textContent = this.params.d.toFixed(2);
        }, 150);
    }

    generateCCode() {
        return `// ============================================================================
// ZKAEDI SINGULARITY // Native ZCC 60 FPS Video Pipeline
// Compiles natively via: ./zcc <this_file>.c -o anim.s && gcc anim.s -lm -o anim
// Streams raw frames to FFmpeg: ./anim | ffmpeg -y -f rawvideo ...
// ============================================================================
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

extern double sin(double);
extern double cos(double);
extern double exp(double);
extern double sqrt(double);
extern double floor(double);

#define W 800
#define H 600
#define FRAMES 600
#define PARTICLES ${this.activeParticles}

#define ETA   ${this.params.eta.toFixed(2)}
#define GAMMA ${this.params.gamma.toFixed(2)}
#define DECAY ${this.params.decay.toFixed(2)}

unsigned char buf[H][W][3];

int main() {
    memset(buf, 0, sizeof(buf));

    for (int f = 0; f < FRAMES; f++) {
        // 1. Phosphor trail persistence
        for (int y = 0; y < H; y++) {
            for (int x = 0; x < W; x++) {
                buf[y][x][0] = (unsigned char)(buf[y][x][0] * DECAY);
                buf[y][x][1] = (unsigned char)(buf[y][x][1] * DECAY);
                buf[y][x][2] = (unsigned char)(buf[y][x][2] * DECAY);
            }
        }

        double t = (double)f / (double)FRAMES * 6.283185307 * 2.0;

        // Attractor parameter evolution
        double a = ${this.params.a.toFixed(2)} + 0.28 * sin(t * 0.5);
        double b = ${this.params.b.toFixed(2)} + 0.24 * cos(t * 0.7);
        double c = ${this.params.c.toFixed(2)} + 0.18 * sin(t * 1.1);
        double d = ${this.params.d.toFixed(2)} + 0.20 * cos(t * 1.3);

        // 3D camera Euler angles
        double pitch = t * 0.40;
        double yaw   = t * 0.70;
        double cp = cos(pitch), sp = sin(pitch);
        double cy = cos(yaw),   sy = sin(yaw);

        // Chromatic palette evolution
        double cr = 0.5 + 0.5 * sin(t * 0.9 + 0.0);
        double cg = 0.5 + 0.5 * sin(t * 0.9 + 2.094);
        double cb = 0.5 + 0.5 * sin(t * 0.9 + 4.188);

        double x = 0.1, y = 0.1;

        // 2. Compute particles
        for (int i = 0; i < PARTICLES; i++) {
            double h_base_x = sin(a * y) + c * cos(a * x);
            double h_base_y = sin(b * x) + d * cos(b * y);

            double sig_x = 1.0 / (1.0 + exp(-GAMMA * y));
            double sig_y = 1.0 / (1.0 + exp(-GAMMA * x));
            double nx = h_base_x + ETA * x * sig_x;
            double ny = h_base_y + ETA * y * sig_y;

            x = nx; y = ny;
            double z = sin(x * y * 0.6) * 1.2;

            // 3D rotation transform
            double x1 = x * cy - z * sy;
            double z1 = x * sy + z * cy;
            double y1 = y * cp - z1 * sp;
            double z2 = y * sp + z1 * cp;

            double dist = 4.2 + z2 * 0.45;
            double fov = 680.0 / dist;
            int px = (int)(W / 2 + x1 * fov);
            int py = (int)(H / 2 + y1 * fov);

            if (px >= 0 && px < W && py >= 0 && py < H) {
                double depth_tint = 0.65 + 0.35 * ((z2 + 2.5) / 5.0);
                int ir = buf[py][px][0] + (int)(cr * 14.0 * depth_tint);
                int ig = buf[py][px][1] + (int)(cg * 22.0 * depth_tint);
                int ib = buf[py][px][2] + (int)(cb * 35.0 * depth_tint);

                buf[py][px][0] = (unsigned char)(ir > 255 ? 255 : ir);
                buf[py][px][1] = (unsigned char)(ig > 255 ? 255 : ig);
                buf[py][px][2] = (unsigned char)(ib > 255 ? 255 : ib);
            }
        }

        // 3. Stream raw frame to stdout
        fwrite(buf, 1, sizeof(buf), stdout);
    }
    return 0;
}
`;
    }

    render() {
        const gl = this.gl;
        if (!gl) return;

        const now = performance.now();
        this.time += 0.016;
        this.frameCount++;
        if (now - this.lastTime >= 1000) {
            this.fps = this.frameCount;
            this.frameCount = 0;
            this.lastTime = now;
            document.getElementById('hudFps').textContent = this.fps;
        }

        this.updateAudio();

        if (this.params.autoMorph) {
            const t = this.time * 0.3;
            this.params.a = -1.40 + 0.28 * Math.sin(t * 0.5);
            this.params.b =  1.60 + 0.24 * Math.cos(t * 0.7);
            this.params.c =  1.00 + 0.18 * Math.sin(t * 1.1);
            this.params.d =  0.70 + 0.20 * Math.cos(t * 1.3);
            this.camera.yaw += 0.005;
        }

        // Sim Step (Ping-Pong FBO)
        const nextFBO = 1 - this.currentFBO;
        gl.bindFramebuffer(gl.FRAMEBUFFER, this.fbo[nextFBO]);
        gl.viewport(0, 0, this.texWidth, this.texHeight);
        gl.useProgram(this.simProg);

        gl.activeTexture(gl.TEXTURE0);
        gl.bindTexture(gl.TEXTURE_2D, this.posTextures[this.currentFBO]);
        gl.uniform1i(gl.getUniformLocation(this.simProg, "uPosTex"), 0);

        gl.uniform1f(gl.getUniformLocation(this.simProg, "uA"), this.params.a);
        gl.uniform1f(gl.getUniformLocation(this.simProg, "uB"), this.params.b);
        gl.uniform1f(gl.getUniformLocation(this.simProg, "uC"), this.params.c);
        gl.uniform1f(gl.getUniformLocation(this.simProg, "uD"), this.params.d);
        gl.uniform1f(gl.getUniformLocation(this.simProg, "uEta"), this.params.eta);
        gl.uniform1f(gl.getUniformLocation(this.simProg, "uGamma"), this.params.gamma);
        gl.uniform1f(gl.getUniformLocation(this.simProg, "uBassMod"), this.bassEnergy);
        gl.uniform1f(gl.getUniformLocation(this.simProg, "uTrebleMod"), this.trebleEnergy);
        gl.uniform1f(gl.getUniformLocation(this.simProg, "uTime"), this.time);

        gl.bindBuffer(gl.ARRAY_BUFFER, this.quadVBO);
        const posLoc = gl.getAttribLocation(this.simProg, "position");
        gl.enableVertexAttribArray(posLoc);
        gl.vertexAttribPointer(posLoc, 2, gl.FLOAT, false, 0, 0);
        gl.drawArrays(gl.TRIANGLE_STRIP, 0, 4);

        this.currentFBO = nextFBO;

        // Render to Screen
        gl.bindFramebuffer(gl.FRAMEBUFFER, null);
        gl.viewport(0, 0, this.canvas.width, this.canvas.height);

        gl.enable(gl.BLEND);
        gl.blendFunc(gl.SRC_ALPHA, gl.ONE);

        gl.useProgram(this.renderProg);
        gl.activeTexture(gl.TEXTURE0);
        gl.bindTexture(gl.TEXTURE_2D, this.posTextures[this.currentFBO]);
        gl.uniform1i(gl.getUniformLocation(this.renderProg, "uPosTex"), 0);

        gl.uniform2f(gl.getUniformLocation(this.renderProg, "uTexSize"), this.texWidth, this.texHeight);
        gl.uniform2f(gl.getUniformLocation(this.renderProg, "uScreenSize"), this.canvas.width, this.canvas.height);
        gl.uniform1f(gl.getUniformLocation(this.renderProg, "uPitch"), this.camera.pitch);
        gl.uniform1f(gl.getUniformLocation(this.renderProg, "uYaw"), this.camera.yaw);
        gl.uniform1f(gl.getUniformLocation(this.renderProg, "uZoom"), this.camera.zoom);
        gl.uniform1f(gl.getUniformLocation(this.renderProg, "uDist"), this.params.dist);
        gl.uniform1f(gl.getUniformLocation(this.renderProg, "uFov"), this.params.fov);
        gl.uniform1f(gl.getUniformLocation(this.renderProg, "uPointSize"), this.params.pointSize);
        gl.uniform1f(gl.getUniformLocation(this.renderProg, "uDepthScale"), this.params.depth);
        gl.uniform1f(gl.getUniformLocation(this.renderProg, "uTime"), this.time);

        let themeId = 0;
        if (this.params.theme === 'gold') themeId = 1;
        else if (this.params.theme === 'spectral') themeId = 2;
        else if (this.params.theme === 'emerald') themeId = 3;
        else if (this.params.theme === 'infrared') themeId = 4;
        gl.uniform1i(gl.getUniformLocation(this.renderProg, "uTheme"), themeId);

        gl.bindBuffer(gl.ARRAY_BUFFER, this.particleVBO);
        const idxLoc = gl.getAttribLocation(this.renderProg, "aIndex");
        gl.enableVertexAttribArray(idxLoc);
        gl.vertexAttribPointer(idxLoc, 1, gl.FLOAT, false, 0, 0);

        gl.drawArrays(gl.POINTS, 0, this.activeParticles);

        gl.disable(gl.BLEND);

        requestAnimationFrame(() => this.render());
    }
}

window.addEventListener('DOMContentLoaded', () => {
    window.studio = new SingularityStudio();
    window.studio.render();
});
