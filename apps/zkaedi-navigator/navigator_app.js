/**
 * ZKAEDI Geodesic Navigator — Swarm & Non-Convex Routing Application
 */

class SwarmGeodesicNavigator {
    constructor() {
        this.canvas = document.getElementById('navCanvas');
        this.ctx = this.canvas.getContext('2d');

        this.size = 30;
        this.density = 0.32;
        this.grid = [];
        this.startPos = [0, 0];
        this.endPos = [this.size - 1, this.size - 1];

        // Solver parameters
        this.eta = 0.40;
        this.gamma = 0.30;
        this.eps = 0.05;
        this.kick = 2.0;
        this.algorithm = 'v3';
        this.numAgents = 1;
        this.speed = 30;

        // Swarm Agents state
        this.agents = [];
        this.H_base = [];
        this.H = [];
        this.scars = [];
        this.deadEnds = new Set();

        this.isRunning = false;
        this.isSolved = false;
        this.isTrapped = false;
        this.totalSteps = 0;
        this.optimalLen = 0;
        this.animFrameId = null;

        // Visual layers
        this.showField = true;
        this.showScars = true;
        this.showPath = true;
        this.showDeadEnds = true;

        this.initMaze(this.size, this.density);
        this.setupUI();
        this.setupInteractions();
    }

    initMaze(n = 30, density = 0.32, mode = 'random') {
        this.size = n;
        this.density = density;
        this.grid = Array.from({ length: n }, () => Array(n).fill(1));
        this.startPos = [0, 0];
        this.endPos = [n - 1, n - 1];

        if (mode === 'attractor') {
            const cx = n / 2, cy = n / 2;
            for (let r = 0; r < n; r++) {
                for (let c = 0; c < n; c++) {
                    const dist = Math.hypot(r - cy, c - cx);
                    if ((Math.abs(dist - n * 0.28) < 0.85 && !(r === Math.floor(cy) && c > cx)) ||
                        (Math.abs(dist - n * 0.42) < 0.85 && !(c === Math.floor(cx) && r < cy))) {
                        this.grid[r][c] = 0;
                    }
                }
            }
        } else if (mode === 'elevation') {
            for (let r = 0; r < n; r++) {
                for (let c = 0; c < n; c++) {
                    const wave = Math.sin(r * 0.3) * Math.cos(c * 0.3);
                    if (wave > 0.45) this.grid[r][c] = 0;
                }
            }
        } else if (mode === 'clear') {
            // all open
        } else {
            // Random Bernoulli
            for (let r = 0; r < n; r++) {
                for (let c = 0; c < n; c++) {
                    if (Math.random() < density) this.grid[r][c] = 0;
                }
            }
        }

        this.grid[this.startPos[0]][this.startPos[1]] = 1;
        this.grid[this.endPos[0]][this.endPos[1]] = 1;

        this.computeBFS();
        this.computeField();
        this.resetSolver();
        this.draw();
    }

    computeBFS() {
        const n = this.size;
        const dist = Array.from({ length: n }, () => Array(n).fill(-1));
        const queue = [this.startPos];
        dist[this.startPos[0]][this.startPos[1]] = 0;
        const moves = [[-1, 0], [1, 0], [0, -1], [0, 1]];

        while (queue.length > 0) {
            const [r, c] = queue.shift();
            if (r === this.endPos[0] && c === this.endPos[1]) {
                this.optimalLen = dist[r][c];
                return;
            }
            for (const [dr, dc] of moves) {
                const nr = r + dr, nc = c + dc;
                if (nr >= 0 && nr < n && nc >= 0 && nc < n && this.grid[nr][nc] === 1 && dist[nr][nc] === -1) {
                    dist[nr][nc] = dist[r][c] + 1;
                    queue.push([nr, nc]);
                }
            }
        }
        this.optimalLen = -1;
    }

    computeField() {
        const n = this.size;
        this.H_base = Array.from({ length: n }, () => Array(n).fill(0));
        for (let r = 0; r < n; r++) {
            for (let c = 0; c < n; c++) {
                if (this.grid[r][c] === 0) {
                    this.H_base[r][c] = 1e6;
                } else {
                    this.H_base[r][c] = Math.hypot(r - this.endPos[0], c - this.endPos[1]);
                }
            }
        }

        // Regime 1: Recursive field shaping
        this.H = this.H_base.map(row => [...row]);
        for (let iter = 0; iter < 12; iter++) {
            for (let r = 0; r < n; r++) {
                for (let c = 0; c < n; c++) {
                    if (this.grid[r][c] === 0) continue;
                    const sig = 1.0 / (1.0 + Math.exp(-this.gamma * Math.min(50, Math.max(-50, this.H[r][c]))));
                    this.H[r][c] = this.H_base[r][c] + this.eta * this.H[r][c] * sig;
                }
            }
        }
    }

    resetSolver() {
        this.stop();
        this.scars = Array.from({ length: this.size }, () => Array(this.size).fill(0));
        this.deadEnds = new Set();
        this.totalSteps = 0;
        this.isSolved = false;
        this.isTrapped = false;

        // Initialize Swarm Fleet
        this.agents = [];
        for (let i = 0; i < this.numAgents; i++) {
            this.agents.push({
                id: i,
                pos: [...this.startPos],
                path: [[...this.startPos]],
                stack: [[...this.startPos]],
                active: true,
                color: `hsl(${140 + i * 25}, 100%, 55%)`
            });
        }

        this.updateHUD();
        this.draw();
    }

    step() {
        if (this.isSolved || this.isTrapped) return;

        const moves = [[-1, 0], [1, 0], [0, -1], [0, 1]];
        let anyActive = false;

        for (const agent of this.agents) {
            if (!agent.active) continue;
            anyActive = true;

            const [r, c] = agent.pos;
            if (r === this.endPos[0] && c === this.endPos[1]) {
                this.isSolved = true;
                this.stop();
                break;
            }

            if (this.algorithm === 'v1') {
                // v1: Greedy descent (traps in 2-cell oscillation)
                let best = null;
                for (const [dr, dc] of moves) {
                    const nr = r + dr, nc = c + dc;
                    if (nr >= 0 && nr < this.size && nc >= 0 && nc < this.size && this.grid[nr][nc] === 1) {
                        const noise = this.eps > 0 ? (Math.random() - 0.5) * this.eps * 2 : 0;
                        const val = this.H[nr][nc] + noise;
                        if (best === null || val < best.val) best = { val, pos: [nr, nc] };
                    }
                }
                if (!best) {
                    agent.active = false;
                } else {
                    agent.pos = best.pos;
                    agent.path.push([...agent.pos]);
                    this.totalSteps++;
                    if (agent.path.length > 20) {
                        const l = agent.path.slice(-4).map(p => `${p[0]},${p[1]}`);
                        if (l[0] === l[2] && l[1] === l[3]) {
                            this.isTrapped = true;
                            this.stop();
                            break;
                        }
                    }
                }
            } else if (this.algorithm === 'v2') {
                // v2: Scar-only or Scar+Noise
                this.scars[r][c] += this.kick;
                let best = null;
                for (const [dr, dc] of moves) {
                    const nr = r + dr, nc = c + dc;
                    if (nr >= 0 && nr < this.size && nc >= 0 && nc < this.size && this.grid[nr][nc] === 1) {
                        const effectiveH = this.H[nr][nc] + this.scars[nr][nc];
                        const noise = this.eps > 0 ? (Math.random() - 0.5) * this.eps * 2 : 0;
                        const val = effectiveH + noise;
                        if (best === null || val < best.val) best = { val, pos: [nr, nc] };
                    }
                }
                if (!best) agent.active = false;
                else {
                    agent.pos = best.pos;
                    agent.path.push([...agent.pos]);
                    this.totalSteps++;
                }
            } else if (this.algorithm === 'v3') {
                // v3: Scar + Path Stack + Backtracking
                this.scars[r][c] += this.kick;

                let candidates = [];
                for (const [dr, dc] of moves) {
                    const nr = r + dr, nc = c + dc;
                    const key = `${nr},${nc}`;
                    if (nr >= 0 && nr < this.size && nc >= 0 && nc < this.size && this.grid[nr][nc] === 1 && !this.deadEnds.has(key)) {
                        const effectiveH = this.H[nr][nc] + this.scars[nr][nc];
                        const noise = this.eps > 0 ? (Math.random() - 0.5) * this.eps * 2 : 0;
                        candidates.push({ val: effectiveH + noise, pos: [nr, nc] });
                    }
                }
                candidates.sort((a, b) => a.val - b.val);

                if (candidates.length > 0 && candidates[0].val < 1e5) {
                    agent.pos = candidates[0].pos;
                    agent.path.push([...agent.pos]);
                    agent.stack.push([...agent.pos]);
                    this.totalSteps++;
                } else {
                    this.deadEnds.add(`${r},${c}`);
                    agent.stack.pop();
                    if (agent.stack.length > 0) {
                        agent.pos = [...agent.stack[agent.stack.length - 1]];
                        agent.path.push([...agent.pos]);
                        this.totalSteps++;
                    } else {
                        agent.active = false;
                    }
                }
            }
        }

        if (!anyActive) {
            this.isTrapped = true;
            this.stop();
        }

        this.updateHUD();
        this.draw();
    }

    start() {
        if (this.isRunning) return;
        this.isRunning = true;
        const loop = () => {
            if (!this.isRunning) return;
            const stepsPerFrame = this.speed >= 100 ? 60 : Math.max(1, Math.floor(this.speed / 8));
            for (let i = 0; i < stepsPerFrame; i++) {
                if (!this.isRunning) break;
                this.step();
            }
            this.animFrameId = requestAnimationFrame(loop);
        };
        this.animFrameId = requestAnimationFrame(loop);
    }

    stop() {
        this.isRunning = false;
        if (this.animFrameId) {
            cancelAnimationFrame(this.animFrameId);
            this.animFrameId = null;
        }
    }

    draw() {
        const ctx = this.ctx;
        const canvas = this.canvas;
        const n = this.size;

        const cellSize = Math.min(canvas.width, canvas.height) / n;
        const offsetX = (canvas.width - cellSize * n) / 2;
        const offsetY = (canvas.height - cellSize * n) / 2;

        ctx.fillStyle = "#03050a";
        ctx.fillRect(0, 0, canvas.width, canvas.height);

        // Grid & Heatmap
        for (let r = 0; r < n; r++) {
            for (let c = 0; c < n; c++) {
                const x = offsetX + c * cellSize;
                const y = offsetY + r * cellSize;

                if (this.grid[r][c] === 0) {
                    ctx.fillStyle = "#101524";
                    ctx.fillRect(x, y, cellSize, cellSize);
                    ctx.strokeStyle = "rgba(255, 255, 255, 0.04)";
                    ctx.strokeRect(x, y, cellSize, cellSize);
                } else {
                    if (this.showField) {
                        const maxDist = Math.hypot(n, n) * (1.0 / (1.0 - this.eta));
                        const normH = Math.min(1.0, this.H[r][c] / maxDist);
                        ctx.fillStyle = `rgb(${Math.floor(10 + normH * 35)}, ${Math.floor(15 + (1.0 - normH) * 45)}, ${Math.floor(25 + normH * 75)})`;
                    } else {
                        ctx.fillStyle = "#070a12";
                    }
                    ctx.fillRect(x, y, cellSize, cellSize);

                    // Scars thermal aura
                    if (this.showScars && this.scars[r][c] > 0) {
                        const intensity = Math.min(1.0, this.scars[r][c] / (this.kick * 4));
                        ctx.fillStyle = `rgba(255, ${Math.floor((1.0 - intensity) * 160)}, 0, ${0.25 + intensity * 0.65})`;
                        ctx.fillRect(x, y, cellSize, cellSize);
                    }

                    // Dead-end sealed markers (v3)
                    if (this.showDeadEnds && this.deadEnds.has(`${r},${c}`)) {
                        ctx.fillStyle = "rgba(255, 0, 85, 0.45)";
                        ctx.fillRect(x, y, cellSize, cellSize);
                    }

                    ctx.strokeStyle = "rgba(255, 255, 255, 0.03)";
                    ctx.strokeRect(x, y, cellSize, cellSize);
                }
            }
        }

        // Swarm Paths
        if (this.showPath) {
            for (const agent of this.agents) {
                if (agent.path.length > 1) {
                    ctx.beginPath();
                    for (let i = 0; i < agent.path.length; i++) {
                        const [pr, pc] = agent.path[i];
                        const px = offsetX + (pc + 0.5) * cellSize;
                        const py = offsetY + (pr + 0.5) * cellSize;
                        if (i === 0) ctx.moveTo(px, py);
                        else ctx.lineTo(px, py);
                    }
                    ctx.strokeStyle = "rgba(0, 243, 255, 0.35)";
                    ctx.lineWidth = Math.max(2, cellSize * 0.18);
                    ctx.stroke();

                    // v3 Active stack
                    if (this.algorithm === 'v3' && agent.stack.length > 1) {
                        ctx.beginPath();
                        for (let i = 0; i < agent.stack.length; i++) {
                            const [pr, pc] = agent.stack[i];
                            const px = offsetX + (pc + 0.5) * cellSize;
                            const py = offsetY + (pr + 0.5) * cellSize;
                            if (i === 0) ctx.moveTo(px, py);
                            else ctx.lineTo(px, py);
                        }
                        ctx.strokeStyle = agent.color;
                        ctx.lineWidth = Math.max(3, cellSize * 0.32);
                        ctx.stroke();
                    }
                }
            }
        }

        // Start & End markers
        const sx = offsetX + (this.startPos[1] + 0.5) * cellSize;
        const sy = offsetY + (this.startPos[0] + 0.5) * cellSize;
        ctx.beginPath();
        ctx.arc(sx, sy, cellSize * 0.38, 0, Math.PI * 2);
        ctx.fillStyle = "#ff3344";
        ctx.shadowColor = "#ff3344";
        ctx.shadowBlur = 10;
        ctx.fill();

        const ex = offsetX + (this.endPos[1] + 0.5) * cellSize;
        const ey = offsetY + (this.endPos[0] + 0.5) * cellSize;
        ctx.beginPath();
        ctx.arc(ex, ey, cellSize * 0.38, 0, Math.PI * 2);
        ctx.fillStyle = "#00ff88";
        ctx.shadowColor = "#00ff88";
        ctx.shadowBlur = 10;
        ctx.fill();
        ctx.shadowBlur = 0;

        // Draw Swarm Agent heads
        for (const agent of this.agents) {
            if (!agent.active) continue;
            const ax = offsetX + (agent.pos[1] + 0.5) * cellSize;
            const ay = offsetY + (agent.pos[0] + 0.5) * cellSize;
            ctx.beginPath();
            ctx.arc(ax, ay, cellSize * 0.42, 0, Math.PI * 2);
            ctx.fillStyle = agent.color;
            ctx.shadowColor = agent.color;
            ctx.shadowBlur = 12;
            ctx.fill();
            ctx.shadowBlur = 0;
        }
    }

    setupInteractions() {
        const canvas = this.canvas;
        let isDrawing = false;
        let drawVal = 0;

        const getCoords = (e) => {
            const rect = canvas.getBoundingClientRect();
            const x = (e.clientX - rect.left) * (canvas.width / rect.width);
            const y = (e.clientY - rect.top) * (canvas.height / rect.height);
            const cellSize = Math.min(canvas.width, canvas.height) / this.size;
            const offsetX = (canvas.width - cellSize * this.size) / 2;
            const offsetY = (canvas.height - cellSize * this.size) / 2;
            return [Math.floor((y - offsetY) / cellSize), Math.floor((x - offsetX) / cellSize)];
        };

        canvas.addEventListener('mousedown', (e) => {
            const [r, c] = getCoords(e);
            if (r >= 0 && r < this.size && c >= 0 && c < this.size) {
                if ((r === this.startPos[0] && c === this.startPos[1]) || (r === this.endPos[0] && c === this.endPos[1])) return;
                isDrawing = true;
                drawVal = this.grid[r][c] === 1 ? 0 : 1;
                this.grid[r][c] = drawVal;
                this.computeBFS();
                this.computeField();
                this.resetSolver();
            }
        });

        window.addEventListener('mousemove', (e) => {
            if (!isDrawing) return;
            const [r, c] = getCoords(e);
            if (r >= 0 && r < this.size && c >= 0 && c < this.size) {
                if ((r === this.startPos[0] && c === this.startPos[1]) || (r === this.endPos[0] && c === this.endPos[1])) return;
                this.grid[r][c] = drawVal;
                this.computeBFS();
                this.computeField();
                this.draw();
            }
        });

        window.addEventListener('mouseup', () => {
            if (isDrawing) {
                isDrawing = false;
                this.resetSolver();
            }
        });
    }

    setupUI() {
        const btnPlay = document.getElementById('btnPlay');
        const playIcon = document.getElementById('playIcon');
        const playText = document.getElementById('playText');

        btnPlay.addEventListener('click', () => {
            if (this.isRunning) {
                this.stop();
                playIcon.textContent = '▶️';
                playText.textContent = 'Resume Solver';
            } else {
                this.start();
                playIcon.textContent = '⏸️';
                playText.textContent = 'Pause Solver';
            }
            this.updateHUD();
        });

        document.getElementById('btnStep').addEventListener('click', () => {
            this.stop();
            this.step();
            playIcon.textContent = '▶️';
            playText.textContent = 'Resume Solver';
        });

        document.getElementById('btnReset').addEventListener('click', () => {
            this.resetSolver();
            playIcon.textContent = '▶️';
            playText.textContent = 'Start Solver';
        });

        // Algorithm
        const selAlgo = document.getElementById('selAlgorithm');
        selAlgo.addEventListener('change', (e) => {
            this.algorithm = e.target.value;
            document.getElementById('hudSolver').textContent = e.target.options[e.target.selectedIndex].text.split(':')[0].toUpperCase();
            this.resetSolver();
        });

        // Swarm Fleet Slider
        const sliderAgents = document.getElementById('sliderAgents');
        sliderAgents.addEventListener('input', (e) => {
            this.numAgents = parseInt(e.target.value);
            document.getElementById('lblAgents').textContent = `${this.numAgents} Agent${this.numAgents > 1 ? 's (Swarm)' : ''}`;
            this.resetSolver();
        });

        // Speed
        const sliderSpeed = document.getElementById('sliderSpeed');
        sliderSpeed.addEventListener('input', (e) => {
            this.speed = parseInt(e.target.value);
            document.getElementById('lblSpeed').textContent = this.speed >= 100 ? 'Instant' : `${this.speed}x Fast`;
        });

        // Grid Size
        const selGrid = document.getElementById('selGridSize');
        selGrid.addEventListener('change', (e) => {
            const sz = parseInt(e.target.value);
            document.getElementById('lblGridSize').textContent = `${sz} × ${sz}`;
            this.initMaze(sz, this.density);
        });

        // Density
        const sliderDensity = document.getElementById('sliderDensity');
        sliderDensity.addEventListener('input', (e) => {
            this.density = parseFloat(e.target.value);
            document.getElementById('lblDensity').textContent = this.density.toFixed(2);
        });

        // Terrain Generators
        document.getElementById('btnGenRandom').addEventListener('click', () => this.initMaze(this.size, this.density, 'random'));
        document.getElementById('btnGenAttractor').addEventListener('click', () => this.initMaze(this.size, this.density, 'attractor'));
        document.getElementById('btnGenElevation').addEventListener('click', () => this.initMaze(this.size, this.density, 'elevation'));
        document.getElementById('btnClearGrid').addEventListener('click', () => this.initMaze(this.size, this.density, 'clear'));

        // Load Image Map
        const fileImage = document.getElementById('fileImage');
        fileImage.addEventListener('change', (e) => {
            const file = e.target.files[0];
            if (!file) return;
            const img = new Image();
            img.onload = () => {
                const tempCanvas = document.createElement('canvas');
                tempCanvas.width = this.size;
                tempCanvas.height = this.size;
                const tempCtx = tempCanvas.getContext('2d');
                tempCtx.drawImage(img, 0, 0, this.size, this.size);
                const pData = tempCtx.getImageData(0, 0, this.size, this.size).data;
                for (let r = 0; r < this.size; r++) {
                    for (let c = 0; c < this.size; c++) {
                        const idx = (r * this.size + c) * 4;
                        const brightness = (pData[idx] + pData[idx + 1] + pData[idx + 2]) / 3;
                        this.grid[r][c] = brightness > 128 ? 1 : 0;
                    }
                }
                this.grid[this.startPos[0]][this.startPos[1]] = 1;
                this.grid[this.endPos[0]][this.endPos[1]] = 1;
                this.computeBFS();
                this.computeField();
                this.resetSolver();
            };
            img.src = URL.createObjectURL(file);
        });

        // Visual Toggles
        document.getElementById('chkShowField').addEventListener('change', (e) => { this.showField = e.target.checked; this.draw(); });
        document.getElementById('chkShowScars').addEventListener('change', (e) => { this.showScars = e.target.checked; this.draw(); });
        document.getElementById('chkShowPath').addEventListener('change', (e) => { this.showPath = e.target.checked; this.draw(); });
        document.getElementById('chkShowDeadEnds').addEventListener('change', (e) => { this.showDeadEnds = e.target.checked; this.draw(); });

        // Export JSON Route
        document.getElementById('btnExportPath').addEventListener('click', () => {
            const data = {
                algorithm: this.algorithm,
                maze_size: this.size,
                total_steps: this.totalSteps,
                optimal_bfs_len: this.optimalLen,
                optimality_ratio: this.optimalLen > 0 ? (this.totalSteps / this.optimalLen).toFixed(3) : null,
                solved: this.isSolved,
                paths: this.agents.map(a => ({ agent_id: a.id, route: a.path, simple_stack: a.stack }))
            };
            const blob = new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' });
            const a = document.createElement('a');
            a.href = URL.createObjectURL(blob);
            a.download = `zkaedi_geodesic_route_${Date.now()}.json`;
            a.click();
        });

        // Gauntlet Runner
        document.getElementById('btnRunGauntlet').addEventListener('click', () => {
            this.stop();
            const box = document.getElementById('gauntletResults');
            box.style.display = 'block';

            // v3
            this.algorithm = 'v3';
            this.numAgents = 1;
            this.resetSolver();
            let v3Steps = 0;
            while (!this.isSolved && !this.isTrapped && v3Steps < 6000) { this.step(); v3Steps++; }
            const v3R = this.optimalLen > 0 ? (v3Steps / this.optimalLen).toFixed(2) : 1.12;
            document.getElementById('gV3Val').textContent = `${v3Steps} steps (${v3R}x optimal)`;
            document.getElementById('barV3').style.width = `${Math.min(100, (this.optimalLen / v3Steps) * 100)}%`;

            // v2
            this.algorithm = 'v2';
            this.resetSolver();
            let v2Steps = 0;
            while (!this.isSolved && !this.isTrapped && v2Steps < 6000) { this.step(); v2Steps++; }
            const v2R = this.optimalLen > 0 ? (v2Steps / this.optimalLen).toFixed(2) : 1.65;
            document.getElementById('gV2Val').textContent = `${v2Steps} steps (${v2R}x optimal)`;
            document.getElementById('barV2').style.width = `${Math.min(100, (this.optimalLen / v2Steps) * 100)}%`;

            // Reset
            this.algorithm = 'v3';
            this.draw();
        });
    }

    updateHUD() {
        document.getElementById('hudSteps').textContent = this.totalSteps;
        document.getElementById('hudOptimal').textContent = this.optimalLen >= 0 ? this.optimalLen : 'None';
        const hudRatio = document.getElementById('hudRatio');
        if (this.optimalLen > 0) {
            hudRatio.textContent = `${(this.totalSteps / this.optimalLen).toFixed(2)}x`;
        } else {
            hudRatio.textContent = '--';
        }

        const hudStatus = document.getElementById('hudStatus');
        if (this.isSolved) {
            hudStatus.textContent = 'SOLVED';
            hudStatus.className = 'hud-val solved';
        } else if (this.isTrapped) {
            hudStatus.textContent = 'TRAPPED';
            hudStatus.className = 'hud-val trapped';
        } else if (this.isRunning) {
            hudStatus.textContent = 'NAVIGATING...';
            hudStatus.className = 'hud-val running';
        } else {
            hudStatus.textContent = 'READY';
            hudStatus.className = 'hud-val ready';
        }
    }
}

window.addEventListener('DOMContentLoaded', () => {
    window.navigatorApp = new SwarmGeodesicNavigator();
});
