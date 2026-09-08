/**
 * ZKAEDI PRIME 2M Particle Singularity WebGL GPGPU Engine
 * Simulates up to 2,097,152 particles in real-time at 60 FPS on the GPU.
 */

class ParticleSingularityEngine {
    constructor(canvasId) {
        this.canvas = document.getElementById(canvasId);
        this.gl = this.canvas.getContext('webgl2', { preserveDrawingBuffer: true, antialias: false });
        if (!this.gl) {
            console.error("WebGL 2.0 not supported, falling back to WebGL 1.0");
            this.gl = this.canvas.getContext('webgl', { preserveDrawingBuffer: true });
        }

        // Texture dimension: 1024x2048 = 2,097,152 particles
        this.texWidth = 1024;
        this.texHeight = 2048;
        this.maxParticles = this.texWidth * this.texHeight;
        this.activeParticles = 2000000;

        // Physics Parameters
        this.params = {
            a: -1.40,
            b: 1.60,
            c: 1.00,
            d: 0.70,
            eta: 0.40,      // Canonical subcritical field evolution
            gamma: 0.30,    // Sigmoid sharpness
            beta: 0.10,
            eps: 0.05,
            decay: 0.78,    // Phosphor persistence
            fov: 680.0,
            dist: 4.2,
            rotSpeed: 0.005,
            autoMorph: true,
            theme: 'rhodium' // rhodium, gold, spectral, emerald
        };

        // 3D Camera State
        this.camera = {
            pitch: 0.2,
            yaw: 0.5,
            zoom: 1.0,
            isDragging: false,
            lastMouseX: 0,
            lastMouseY: 0
        };

        this.time = 0.0;
        this.fps = 60;
        this.frameCount = 0;
        this.lastTime = performance.now();

        this.init();
        this.setupEvents();
    }

    init() {
        const gl = this.gl;
        if (!gl) return;

        // Enable float texture extensions if WebGL 1
        gl.getExtension('OES_texture_float');
        gl.getExtension('EXT_color_buffer_float');

        this.resize();
        this.initShaders();
        this.initBuffers();
        this.initTextures();
    }

    resize() {
        const rect = this.canvas.parentElement.getBoundingClientRect();
        this.canvas.width = rect.width * window.devicePixelRatio || 800;
        this.canvas.height = rect.height * window.devicePixelRatio || 600;
        this.gl.viewport(0, 0, this.canvas.width, this.canvas.height);
    }

    initShaders() {
        const gl = this.gl;

        // 1. Simulation Shader (Computes ZKAEDI PRIME equations on GPU)
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
        uniform float uTime;
        uniform float uSeed;
        in vec2 vUv;
        out vec4 outPos;

        float sigmoid(float z) {
            return 1.0 / (1.0 + exp(-clamp(z, -15.0, 15.0)));
        }

        void main() {
            vec4 curr = texture(uPosTex, vUv);
            float x = curr.x;
            float y = curr.y;

            // ZKAEDI PRIME Canonical Equations
            float h_base_x = sin(uA * y) + uC * cos(uA * x);
            float h_base_y = sin(uB * x) + uD * cos(uB * y);

            float sig_x = sigmoid(uGamma * y);
            float sig_y = sigmoid(uGamma * x);

            float nx = h_base_x + uEta * x * sig_x;
            float ny = h_base_y + uEta * y * sig_y;

            // Non-linear Z coupling
            float nz = sin(nx * ny * 0.6) * 1.2;

            // Reset runaway particles
            if (abs(nx) > 8.0 || abs(ny) > 8.0 || isnan(nx) || isnan(ny)) {
                nx = sin(vUv.x * 123.456 + uTime) * 0.5;
                ny = cos(vUv.y * 654.321 + uTime) * 0.5;
                nz = 0.0;
            }

            outPos = vec4(nx, ny, nz, 1.0);
        }`;

        this.simProg = this.createProgram(simVS, simFS);

        // 2. Render Shader (Draws particles into 3D space)
        const renderVS = `#version 300 es
        in float aIndex;
        uniform sampler2D uPosTex;
        uniform vec2 uTexSize;
        uniform vec2 uScreenSize;
        uniform float uPitch, uYaw, uZoom;
        uniform float uDist, uFov;
        uniform int uTheme;
        uniform float uTime;
        out vec4 vColor;

        void main() {
            // Unpack 2D texture coordinate from particle index
            float xIdx = mod(aIndex, uTexSize.x);
            float yIdx = floor(aIndex / uTexSize.x);
            vec2 uv = (vec2(xIdx, yIdx) + 0.5) / uTexSize;

            vec4 pos = texture(uPosTex, uv);
            float x = pos.x;
            float y = pos.y;
            float z = pos.z;

            // 3D Euler Rotation
            float cp = cos(uPitch), sp = sin(uPitch);
            float cy = cos(uYaw),   sy = sin(uYaw);

            float x1 = x * cy - z * sy;
            float z1 = x * sy + z * cy;
            float y1 = y * cp - z1 * sp;
            float z2 = y * sp + z1 * cp;

            // Perspective Projection
            float dist = (uDist + z2 * 0.45) / uZoom;
            if (dist < 0.5) dist = 0.5;
            float fov = uFov / dist;

            vec2 screenPos = vec2(x1 * fov, y1 * fov) / (uScreenSize * 0.5);
            gl_Position = vec4(screenPos, 0.0, 1.0);
            gl_PointSize = 1.0;

            // Color Palette
            float t = uTime * 0.8;
            float cr = 0.5 + 0.5 * sin(t + 0.0);
            float cg = 0.5 + 0.5 * sin(t + 2.094);
            float cb = 0.5 + 0.5 * sin(t + 4.188);
            float depth = clamp((z2 + 2.0) / 4.0, 0.2, 1.0);

            if (uTheme == 0) {
                // Rhodium Cyberpunk
                vColor = vec4(cr * 0.4 * depth, cg * 0.8 * depth, cb * 1.0 * depth, 0.8);
            } else if (uTheme == 1) {
                // Solar Gold & Obsidian
                vColor = vec4(1.0 * depth, 0.7 * depth, 0.1 * depth, 0.9);
            } else if (uTheme == 2) {
                // Hyper Spectral Rainbow
                vColor = vec4(cr * depth, cg * depth, cb * depth, 0.85);
            } else {
                // Emerald Nebula
                vColor = vec4(0.1 * depth, 1.0 * depth, 0.6 * depth, 0.85);
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

        // 3. Phosphor Decay Shader (Alpha trail persistence)
        const decayVS = `#version 300 es
        in vec2 position;
        out vec2 vUv;
        void main() {
            vUv = position * 0.5 + 0.5;
            gl_Position = vec4(position, 0.0, 1.0);
        }`;

        const decayFS = `#version 300 es
        precision highp float;
        uniform sampler2D uTrailTex;
        uniform float uDecay;
        in vec2 vUv;
        out vec4 fragColor;
        void main() {
            vec4 col = texture(uTrailTex, vUv);
            fragColor = vec4(col.rgb * uDecay, 1.0);
        }`;

        this.decayProg = this.createProgram(decayVS, decayFS);
    }

    createProgram(vsSrc, fsSrc) {
        const gl = this.gl;
        const vs = gl.createShader(gl.VERTEX_SHADER);
        gl.shaderSource(vs, vsSrc);
        gl.compileShader(vs);
        if (!gl.getShaderParameter(vs, gl.COMPILE_STATUS)) {
            console.error("VS Error:", gl.getShaderInfoLog(vs));
        }

        const fs = gl.createShader(gl.FRAGMENT_SHADER);
        gl.shaderSource(fs, fsSrc);
        gl.compileShader(fs);
        if (!gl.getShaderParameter(fs, gl.COMPILE_STATUS)) {
            console.error("FS Error:", gl.getShaderInfoLog(fs));
        }

        const prog = gl.createProgram();
        gl.attachShader(prog, vs);
        gl.attachShader(prog, fs);
        gl.linkProgram(prog);
        return prog;
    }

    initBuffers() {
        const gl = this.gl;

        // Quad buffer for fullscreen passes
        const quadVerts = new Float32Array([-1, -1, 1, -1, -1, 1, 1, 1]);
        this.quadVBO = gl.createBuffer();
        gl.bindBuffer(gl.ARRAY_BUFFER, this.quadVBO);
        gl.bufferData(gl.ARRAY_BUFFER, quadVerts, gl.STATIC_DRAW);

        // Particle indices buffer for gl.POINTS
        const indices = new Float32Array(this.maxParticles);
        for (let i = 0; i < this.maxParticles; i++) {
            indices[i] = i;
        }
        this.particleVBO = gl.createBuffer();
        gl.bindBuffer(gl.ARRAY_BUFFER, this.particleVBO);
        gl.bufferData(gl.ARRAY_BUFFER, indices, gl.STATIC_DRAW);
    }

    initTextures() {
        const gl = this.gl;

        // Initial particle positions
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

    setupEvents() {
        const canvas = this.canvas;
        canvas.addEventListener('mousedown', (e) => {
            this.camera.isDragging = true;
            this.camera.lastMouseX = e.clientX;
            this.camera.lastMouseY = e.clientY;
        });

        window.addEventListener('mousemove', (e) => {
            if (!this.camera.isDragging) return;
            const dx = e.clientX - this.camera.lastMouseX;
            const dy = e.clientY - this.camera.lastMouseY;
            this.camera.yaw += dx * 0.006;
            this.camera.pitch += dy * 0.006;
            this.camera.lastMouseX = e.clientX;
            this.camera.lastMouseY = e.clientY;
        });

        window.addEventListener('mouseup', () => {
            this.camera.isDragging = false;
        });

        canvas.addEventListener('wheel', (e) => {
            e.preventDefault();
            this.camera.zoom *= e.deltaY > 0 ? 0.92 : 1.08;
            this.camera.zoom = Math.max(0.3, Math.min(this.camera.zoom, 5.0));
        }, { passive: false });

        window.addEventListener('resize', () => this.resize());
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
            const fpsEl = document.getElementById('hudFps');
            if (fpsEl) fpsEl.textContent = `${this.fps} FPS`;
        }

        if (this.params.autoMorph) {
            const t = this.time * 0.3;
            this.params.a = -1.40 + 0.28 * Math.sin(t * 0.5);
            this.params.b =  1.60 + 0.24 * Math.cos(t * 0.7);
            this.params.c =  1.00 + 0.18 * Math.sin(t * 1.1);
            this.params.d =  0.70 + 0.20 * Math.cos(t * 1.3);
            this.camera.yaw += this.params.rotSpeed;
        }

        // 1. Simulation Step (Ping-Pong)
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
        gl.uniform1f(gl.getUniformLocation(this.simProg, "uTime"), this.time);

        // Draw quad
        gl.bindBuffer(gl.ARRAY_BUFFER, this.quadVBO);
        const posLoc = gl.getAttribLocation(this.simProg, "position");
        gl.enableVertexAttribArray(posLoc);
        gl.vertexAttribPointer(posLoc, 2, gl.FLOAT, false, 0, 0);
        gl.drawArrays(gl.TRIANGLE_STRIP, 0, 4);

        this.currentFBO = nextFBO;

        // 2. Render to Screen with Phosphor Decay
        gl.bindFramebuffer(gl.FRAMEBUFFER, null);
        gl.viewport(0, 0, this.canvas.width, this.canvas.height);

        // Blending for additive caustics
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
        gl.uniform1f(gl.getUniformLocation(this.renderProg, "uTime"), this.time);

        let themeId = 0;
        if (this.params.theme === 'gold') themeId = 1;
        else if (this.params.theme === 'spectral') themeId = 2;
        else if (this.params.theme === 'emerald') themeId = 3;
        gl.uniform1i(gl.getUniformLocation(this.renderProg, "uTheme"), themeId);

        gl.bindBuffer(gl.ARRAY_BUFFER, this.particleVBO);
        const idxLoc = gl.getAttribLocation(this.renderProg, "aIndex");
        gl.enableVertexAttribArray(idxLoc);
        gl.vertexAttribPointer(idxLoc, 1, gl.FLOAT, false, 0, 0);

        // Draw points
        gl.drawArrays(gl.POINTS, 0, this.activeParticles);

        gl.disable(gl.BLEND);

        requestAnimationFrame(() => this.render());
    }

    capturePNG() {
        const link = document.createElement('a');
        link.download = `zkaedi_2m_singularity_${Date.now()}.png`;
        link.href = this.canvas.toDataURL('image/png');
        link.click();
    }
}
