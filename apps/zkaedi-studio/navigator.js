/**
 * ZKAEDI PRIME Autonomous Maze & Terrain Navigator Visualizer
 * Implements Canonical Two-Regime Hamiltonian dynamics with live animated scars and v3 backtracking.
 */

class MazeNavigatorEngine {
    constructor(canvasId) {
        this.canvas = document.getElementById(canvasId);
        this.ctx = this.canvas.getContext('2d');

        this.size = 25;
        this.density = 0.32;
        this.grid = [];
        this.startPos = [0, 0];
        this.end = [this.size - 1, this.size - 1];

        // Solver parameters
        this.eta = 0.40;
        this.gamma = 0.30;
        this.beta = 0.10;
        this.eps = 0.05;
        this.kick = 2.0;
        this.algorithm = 'v3'; // 'v1', 'v2', 'v3'
        this.speed = 20; // steps per second or instant

        // State
        this.H_base = [];
        this.H = [];
        this.scars = [];
        this.path = [];
        this.pathStack = []; // v3 backtracking stack
        this.deadEnds = new Set();
        this.currentPos = [...this.startPos];
        this.isRunning = false;
        this.isSolved = false;
        this.isTrapped = false;
        this.totalSteps = 0;
        this.optimalLen = 0;
        this.startTime = 0;
        this.animFrameId = null;

        // Visual toggles
        this.showField = true;
        this.showScars = true;
        this.showPath = true;

        this.initMaze(this.size, this.density);
        this.setupInteractions();
    }

    initMaze(n = 25, density = 0.32, isAttractor = false) {
        this.size = n;
        this.density = density;
        this.grid = Array.from({ length: n }, () => Array(n).fill(1));
        this.startPos = [0, 0];
        this.end = [n - 1, n - 1];

        if (isAttractor) {
            // Generate non-convex concentric caustic rings and obstacle barriers
            const cx = n / 2, cy = n / 2;
            for (let r = 0; r < n; r++) {
                for (let c = 0; c < n; c++) {
                    const dist = Math.hypot(r - cy, c - cx);
                    // Two concentric barrier walls with narrow saddle apertures
                    if ((Math.abs(dist - n * 0.3) < 0.8 && !(r === Math.floor(cy) && c > cx)) ||
                        (Math.abs(dist - n * 0.42) < 0.8 && !(c === Math.floor(cx) && r < cy))) {
                        this.grid[r][c] = 0;
                    }
                }
            }
        } else {
            // Standard randomized Bernoulli maze
            for (let r = 0; r < n; r++) {
                for (let c = 0; c < n; c++) {
                    if (Math.random() < density) {
                        this.grid[r][c] = 0;
                    }
                }
            }
        }

        this.grid[this.startPos[0]][this.startPos[1]] = 1;
        this.grid[this.end[0]][this.end[1]] = 1;

        this.computeBFS();
        this.computeH0();
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
            if (r === this.end[0] && c === this.end[1]) {
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
        this.optimalLen = -1; // Unsolvable maze
    }

    computeH0() {
        const n = this.size;
        this.H_base = Array.from({ length: n }, () => Array(n).fill(0));
        for (let r = 0; r < n; r++) {
            for (let c = 0; c < n; c++) {
                if (this.grid[r][c] === 0) {
                    this.H_base[r][c] = 1e6;
                } else {
                    this.H_base[r][c] = Math.hypot(r - this.end[0], c - this.end[1]);
                }
            }
        }
        this.evolveField();
    }

    evolveField() {
        // Regime 1: Recursive field shaping
        const n = this.size;
        this.H = this.H_base.map(row => [...row]);
        for (let iter = 0; iter < 10; iter++) {
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
        this.currentPos = [...this.startPos];
        this.path = [[...this.startPos]];
        this.pathStack = [[...this.startPos]];
        this.deadEnds = new Set();
        this.totalSteps = 0;
        this.isSolved = false;
        this.isTrapped = false;
        this.updateHUD();
        this.draw();
    }

    step() {
        if (this.isSolved || this.isTrapped) return;

        const [r, c] = this.currentPos;
        if (r === this.end[0] && c === this.end[1]) {
            this.isSolved = true;
            this.stop();
            this.updateHUD();
            this.draw();
            return;
        }

        const moves = [[-1, 0], [1, 0], [0, -1], [0, 1]];

        if (this.algorithm === 'v1') {
            // v1: Memoryless greedy descent with optional noise
            let best = null;
            for (const [dr, dc] of moves) {
                const nr = r + dr, nc = c + dc;
                if (nr >= 0 && nr < this.size && nc >= 0 && nc < this.size && this.grid[nr][nc] === 1) {
                    const noise = this.eps > 0 ? (Math.random() - 0.5) * this.eps * 2 : 0;
                    const val = this.H[nr][nc] + noise;
                    if (best === null || val < best.val) {
                        best = { val, pos: [nr, nc] };
                    }
                }
            }
            if (!best) {
                this.isTrapped = true;
                this.stop();
            } else {
                this.currentPos = best.pos;
                this.path.push([...this.currentPos]);
                this.totalSteps++;
                // Detect 2-cell oscillation trap
                if (this.path.length > 20) {
                    const last4 = this.path.slice(-4).map(p => `${p[0]},${p[1]}`);
                    if (last4[0] === last4[2] && last4[1] === last4[3]) {
                        this.isTrapped = true;
                        this.stop();
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
                    if (best === null || val < best.val) {
                        best = { val, pos: [nr, nc] };
                    }
                }
            }
            if (!best) {
                this.isTrapped = true;
                this.stop();
            } else {
                this.currentPos = best.pos;
                this.path.push([...this.currentPos]);
                this.totalSteps++;
            }
        } else if (this.algorithm === 'v3') {
            // v3: Scar + Path Stack + Backtracking (Pruning dead ends)
            this.scars[r][c] += this.kick;

            // Find unvisited/lowest energy valid neighbors
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
                // Forward move
                this.currentPos = candidates[0].pos;
                this.path.push([...this.currentPos]);
                this.pathStack.push([...this.currentPos]);
                this.totalSteps++;
            } else {
                // Dead end reached: Seal cell and backtrack along stack
                this.deadEnds.add(`${r},${c}`);
                this.pathStack.pop();
                if (this.pathStack.length > 0) {
                    this.currentPos = [...this.pathStack[this.pathStack.length - 1]];
                    this.path.push([...this.currentPos]);
                    this.totalSteps++;
                } else {
                    this.isTrapped = true;
                    this.stop();
                }
            }
        }

        this.updateHUD();
        this.draw();
    }

    start() {
        if (this.isRunning) return;
        this.isRunning = true;
        this.startTime = performance.now();
        const loop = () => {
            if (!this.isRunning) return;
            const stepsPerFrame = this.speed >= 100 ? 50 : Math.max(1, Math.floor(this.speed / 10));
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

        ctx.fillStyle = "#04060a";
        ctx.fillRect(0, 0, canvas.width, canvas.height);

        // 1. Draw Grid Cells & Potential Field Heatmap
        for (let r = 0; r < n; r++) {
            for (let c = 0; c < n; c++) {
                const x = offsetX + c * cellSize;
                const y = offsetY + r * cellSize;

                if (this.grid[r][c] === 0) {
                    // Wall
                    ctx.fillStyle = "#121829";
                    ctx.fillRect(x, y, cellSize, cellSize);
                    ctx.strokeStyle = "rgba(255, 255, 255, 0.05)";
                    ctx.strokeRect(x, y, cellSize, cellSize);
                } else {
                    // Open Cell: Draw Field potential gradient
                    if (this.showField) {
                        const maxDist = Math.hypot(n, n) * (1.0 / (1.0 - this.eta));
                        const normH = Math.min(1.0, this.H[r][c] / maxDist);
                        // Blue/purple to dark void
                        ctx.fillStyle = `rgb(${Math.floor(10 + normH * 30)}, ${Math.floor(15 + (1.0 - normH) * 45)}, ${Math.floor(30 + normH * 70)})`;
                    } else {
                        ctx.fillStyle = "#090d16";
                    }
                    ctx.fillRect(x, y, cellSize, cellSize);

                    // Scars thermal glow
                    if (this.showScars && this.scars[r][c] > 0) {
                        const scarIntensity = Math.min(1.0, this.scars[r][c] / (this.kick * 4));
                        ctx.fillStyle = `rgba(255, ${Math.floor((1.0 - scarIntensity) * 180)}, 0, ${0.2 + scarIntensity * 0.6})`;
                        ctx.fillRect(x, y, cellSize, cellSize);
                    }

                    // Dead end seal mark (v3)
                    if (this.deadEnds.has(`${r},${c}`)) {
                        ctx.fillStyle = "rgba(255, 0, 85, 0.45)";
                        ctx.fillRect(x, y, cellSize, cellSize);
                    }

                    ctx.strokeStyle = "rgba(255, 255, 255, 0.04)";
                    ctx.strokeRect(x, y, cellSize, cellSize);
                }
            }
        }

        // 2. Draw Navigated Path
        if (this.showPath && this.path.length > 1) {
            ctx.beginPath();
            for (let i = 0; i < this.path.length; i++) {
                const [pr, pc] = this.path[i];
                const px = offsetX + (pc + 0.5) * cellSize;
                const py = offsetY + (pr + 0.5) * cellSize;
                if (i === 0) ctx.moveTo(px, py);
                else ctx.lineTo(px, py);
            }
            ctx.strokeStyle = "rgba(0, 243, 255, 0.4)";
            ctx.lineWidth = Math.max(2, cellSize * 0.2);
            ctx.lineCap = "round";
            ctx.lineJoin = "round";
            ctx.stroke();

            // v3 Active Simple Path (Stack) in glowing neon green
            if (this.algorithm === 'v3' && this.pathStack.length > 1) {
                ctx.beginPath();
                for (let i = 0; i < this.pathStack.length; i++) {
                    const [pr, pc] = this.pathStack[i];
                    const px = offsetX + (pc + 0.5) * cellSize;
                    const py = offsetY + (pr + 0.5) * cellSize;
                    if (i === 0) ctx.moveTo(px, py);
                    else ctx.lineTo(px, py);
                }
                ctx.strokeStyle = "rgba(0, 255, 136, 0.9)";
                ctx.lineWidth = Math.max(3, cellSize * 0.35);
                ctx.stroke();
            }
        }

        // 3. Draw Start & Goal Markers
        const startX = offsetX + (this.startPos[1] + 0.5) * cellSize;
        const startY = offsetY + (this.startPos[0] + 0.5) * cellSize;
        ctx.beginPath();
        ctx.arc(startX, startY, cellSize * 0.38, 0, Math.PI * 2);
        ctx.fillStyle = "#ff3344";
        ctx.shadowColor = "#ff3344";
        ctx.shadowBlur = 12;
        ctx.fill();
        ctx.shadowBlur = 0;

        const endX = offsetX + (this.end[1] + 0.5) * cellSize;
        const endY = offsetY + (this.end[0] + 0.5) * cellSize;
        ctx.beginPath();
        ctx.arc(endX, endY, cellSize * 0.38, 0, Math.PI * 2);
        ctx.fillStyle = "#00ff88";
        ctx.shadowColor = "#00ff88";
        ctx.shadowBlur = 12;
        ctx.fill();
        ctx.shadowBlur = 0;

        // 4. Draw Current Walker Position
        const currX = offsetX + (this.currentPos[1] + 0.5) * cellSize;
        const currY = offsetY + (this.currentPos[0] + 0.5) * cellSize;
        ctx.beginPath();
        ctx.arc(currX, currY, cellSize * 0.42, 0, Math.PI * 2);
        ctx.fillStyle = "#00f3ff";
        ctx.shadowColor = "#00f3ff";
        ctx.shadowBlur = 16;
        ctx.fill();
        ctx.shadowBlur = 0;
    }

    setupInteractions() {
        const canvas = this.canvas;
        let isDrawing = false;
        let drawMode = 0; // 0: wall, 1: space

        const getGridCoords = (e) => {
            const rect = canvas.getBoundingClientRect();
            const x = (e.clientX - rect.left) * (canvas.width / rect.width);
            const y = (e.clientY - rect.top) * (canvas.height / rect.height);
            const cellSize = Math.min(canvas.width, canvas.height) / this.size;
            const offsetX = (canvas.width - cellSize * this.size) / 2;
            const offsetY = (canvas.height - cellSize * this.size) / 2;
            const c = Math.floor((x - offsetX) / cellSize);
            const r = Math.floor((y - offsetY) / cellSize);
            return [r, c];
        };

        canvas.addEventListener('mousedown', (e) => {
            const [r, c] = getGridCoords(e);
            if (r >= 0 && r < this.size && c >= 0 && c < this.size) {
                if ((r === this.startPos[0] && c === this.startPos[1]) || (r === this.end[0] && c === this.end[1])) return;
                isDrawing = true;
                drawMode = this.grid[r][c] === 1 ? 0 : 1;
                this.grid[r][c] = drawMode;
                this.computeBFS();
                this.computeH0();
                this.resetSolver();
            }
        });

        window.addEventListener('mousemove', (e) => {
            if (!isDrawing) return;
            const [r, c] = getGridCoords(e);
            if (r >= 0 && r < this.size && c >= 0 && c < this.size) {
                if ((r === this.startPos[0] && c === this.startPos[1]) || (r === this.end[0] && c === this.end[1])) return;
                this.grid[r][c] = drawMode;
                this.computeBFS();
                this.computeH0();
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

    updateHUD() {
        const stepsEl = document.getElementById('navSteps');
        const optEl = document.getElementById('navOptimal');
        const ratioEl = document.getElementById('navRatio');
        const statusEl = document.getElementById('navStatus');

        if (stepsEl) stepsEl.textContent = this.totalSteps;
        if (optEl) optEl.textContent = this.optimalLen >= 0 ? this.optimalLen : 'Unsolvable';
        if (ratioEl) {
            if (this.optimalLen > 0) {
                const r = (this.totalSteps / this.optimalLen).toFixed(2);
                ratioEl.textContent = `${r}x`;
            } else {
                ratioEl.textContent = '---';
            }
        }
        if (statusEl) {
            if (this.isSolved) {
                statusEl.textContent = 'SOLVED';
                statusEl.className = 'chip-value';
                statusEl.style.color = 'var(--neon-green)';
            } else if (this.isTrapped) {
                statusEl.textContent = 'TRAPPED (Local Minima)';
                statusEl.className = 'chip-value critical';
            } else if (this.isRunning) {
                statusEl.textContent = 'NAVIGATING...';
                statusEl.className = 'chip-value warning';
            } else {
                statusEl.textContent = 'READY';
                statusEl.className = 'chip-value';
                statusEl.style.color = 'var(--text-secondary)';
            }
        }
    }
}
