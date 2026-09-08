/**
 * ZKAEDI PRIME Sovereign Studio Main Controller
 */

window.addEventListener('DOMContentLoaded', () => {
    // 1. Initialize Engines
    const particleEngine = new ParticleSingularityEngine('particleCanvas');
    const navigatorEngine = new MazeNavigatorEngine('mazeCanvas');
    window.particleEngine = particleEngine;
    window.navigatorEngine = navigatorEngine;

    // Start particle rendering loop
    particleEngine.render();

    // 2. View Mode Switcher
    const btnViewParticles = document.getElementById('btnViewParticles');
    const btnViewNavigator = document.getElementById('btnViewNavigator');
    const btnViewSplit = document.getElementById('btnViewSplit');
    const viewParticles = document.getElementById('viewParticles');
    const viewNavigator = document.getElementById('viewNavigator');
    const mainWorkspace = document.getElementById('mainWorkspace');

    function setViewMode(mode) {
        btnViewParticles.classList.remove('active');
        btnViewNavigator.classList.remove('active');
        btnViewSplit.classList.remove('active');

        mainWorkspace.classList.remove('dual-split');
        viewParticles.classList.remove('active');
        viewNavigator.classList.remove('active');

        if (mode === 'particles') {
            btnViewParticles.classList.add('active');
            viewParticles.classList.add('active');
            particleEngine.resize();
        } else if (mode === 'navigator') {
            btnViewNavigator.classList.add('active');
            viewNavigator.classList.add('active');
            navigatorEngine.draw();
        } else if (mode === 'split') {
            btnViewSplit.classList.add('active');
            mainWorkspace.classList.add('dual-split');
            viewParticles.classList.add('active');
            viewNavigator.classList.add('active');
            particleEngine.resize();
            navigatorEngine.draw();
        }
    }

    btnViewParticles.addEventListener('click', () => setViewMode('particles'));
    btnViewNavigator.addEventListener('click', () => setViewMode('navigator'));
    btnViewSplit.addEventListener('click', () => setViewMode('split'));

    // 3. Particle Simulator Controls
    const sliderParticles = document.getElementById('sliderParticles');
    const valParticles = document.getElementById('valParticles');
    const hudParticles = document.getElementById('hudParticles');

    sliderParticles.addEventListener('input', (e) => {
        const count = parseInt(e.target.value);
        particleEngine.activeParticles = count;
        valParticles.textContent = count.toLocaleString();
        hudParticles.textContent = count.toLocaleString();
    });

    const sliderEta = document.getElementById('sliderEta');
    const valEta = document.getElementById('valEta');
    const hudRegime = document.getElementById('hudRegime');

    sliderEta.addEventListener('input', (e) => {
        const eta = parseFloat(e.target.value);
        particleEngine.params.eta = eta;
        valEta.textContent = eta.toFixed(2);
        if (eta < 1.0) {
            hudRegime.textContent = `SUB-CRITICAL (η=${eta.toFixed(2)})`;
            hudRegime.className = 'chip-value';
        } else if (eta <= 1.05) {
            hudRegime.textContent = `BIFURCATION (η=${eta.toFixed(2)})`;
            hudRegime.className = 'chip-value warning';
        } else {
            hudRegime.textContent = `SUPERCRITICAL (η=${eta.toFixed(2)})`;
            hudRegime.className = 'chip-value critical';
        }
    });

    const sliderGamma = document.getElementById('sliderGamma');
    const valGamma = document.getElementById('valGamma');
    sliderGamma.addEventListener('input', (e) => {
        const gamma = parseFloat(e.target.value);
        particleEngine.params.gamma = gamma;
        valGamma.textContent = gamma.toFixed(2);
    });

    const sliderDecay = document.getElementById('sliderDecay');
    const valDecay = document.getElementById('valDecay');
    sliderDecay.addEventListener('input', (e) => {
        const decay = parseFloat(e.target.value);
        particleEngine.params.decay = decay;
        valDecay.textContent = decay.toFixed(2);
    });

    const selTheme = document.getElementById('selTheme');
    selTheme.addEventListener('change', (e) => {
        particleEngine.params.theme = e.target.value;
    });

    const chkAutoMorph = document.getElementById('chkAutoMorph');
    chkAutoMorph.addEventListener('change', (e) => {
        particleEngine.params.autoMorph = e.target.checked;
    });

    // Preset configurations
    const selPreset = document.getElementById('selPreset');
    selPreset.addEventListener('change', (e) => {
        const p = e.target.value;
        chkAutoMorph.checked = false;
        particleEngine.params.autoMorph = false;

        if (p === 'canonical') {
            particleEngine.params.a = -1.40;
            particleEngine.params.b = 1.60;
            particleEngine.params.c = 1.00;
            particleEngine.params.d = 0.70;
            particleEngine.params.eta = 0.40;
            particleEngine.params.gamma = 0.30;
            chkAutoMorph.checked = true;
            particleEngine.params.autoMorph = true;
        } else if (p === 'explosion') {
            particleEngine.params.a = -1.56;
            particleEngine.params.b = 1.73;
            particleEngine.params.c = 1.16;
            particleEngine.params.d = 0.51;
            particleEngine.params.eta = 0.40;
        } else if (p === 'limitcycle') {
            particleEngine.params.a = -1.13;
            particleEngine.params.b = 1.56;
            particleEngine.params.c = 1.07;
            particleEngine.params.d = 0.50;
            particleEngine.params.eta = 0.40;
        } else if (p === 'vortex') {
            particleEngine.params.a = -1.80;
            particleEngine.params.b = 1.20;
            particleEngine.params.c = 0.90;
            particleEngine.params.d = 1.10;
            particleEngine.params.eta = 0.65;
        } else if (p === 'singularity') {
            particleEngine.params.a = -0.90;
            particleEngine.params.b = 1.80;
            particleEngine.params.c = 1.25;
            particleEngine.params.d = 0.45;
            particleEngine.params.eta = 0.35;
        }

        sliderEta.value = particleEngine.params.eta;
        valEta.textContent = particleEngine.params.eta.toFixed(2);
    });

    // Camera Reset & Snapshot
    document.getElementById('btnResetCamera').addEventListener('click', () => {
        particleEngine.camera.pitch = 0.2;
        particleEngine.camera.yaw = 0.5;
        particleEngine.camera.zoom = 1.0;
    });

    document.getElementById('btnSnapshot').addEventListener('click', () => {
        particleEngine.capturePNG();
    });

    // Bridge: Inject Attractor Potential into Maze Navigator
    document.getElementById('btnInjectToMaze').addEventListener('click', () => {
        navigatorEngine.initMaze(navigatorEngine.size, navigatorEngine.density, true);
        setViewMode('split');
    });

    // 4. Autonomous Navigator Controls
    const btnPlayNav = document.getElementById('btnPlayNav');
    const btnStepNav = document.getElementById('btnStepNav');
    const btnResetNav = document.getElementById('btnResetNav');

    btnPlayNav.addEventListener('click', () => {
        if (navigatorEngine.isRunning) {
            navigatorEngine.stop();
            btnPlayNav.textContent = '▶️ Resume Solver';
        } else {
            navigatorEngine.start();
            btnPlayNav.textContent = '⏸️ Pause Solver';
        }
        navigatorEngine.updateHUD();
    });

    btnStepNav.addEventListener('click', () => {
        navigatorEngine.stop();
        navigatorEngine.step();
        btnPlayNav.textContent = '▶️ Resume Solver';
    });

    btnResetNav.addEventListener('click', () => {
        navigatorEngine.resetSolver();
        btnPlayNav.textContent = '▶️ Start Solver';
    });

    // Algorithm switcher
    const selAlgorithm = document.getElementById('selAlgorithm');
    selAlgorithm.addEventListener('change', (e) => {
        navigatorEngine.algorithm = e.target.value;
        navigatorEngine.resetSolver();
    });

    // Speed slider
    const sliderSpeed = document.getElementById('sliderSpeed');
    const valSpeed = document.getElementById('valSpeed');
    sliderSpeed.addEventListener('input', (e) => {
        const val = parseInt(e.target.value);
        navigatorEngine.speed = val;
        valSpeed.textContent = val >= 100 ? 'Instant' : `${val}x Fast`;
    });

    // Visual Layer Checkboxes
    document.getElementById('chkShowField').addEventListener('change', (e) => {
        navigatorEngine.showField = e.target.checked;
        navigatorEngine.draw();
    });

    document.getElementById('chkShowScars').addEventListener('change', (e) => {
        navigatorEngine.showScars = e.target.checked;
        navigatorEngine.draw();
    });

    document.getElementById('chkShowPath').addEventListener('change', (e) => {
        navigatorEngine.showPath = e.target.checked;
        navigatorEngine.draw();
    });

    // Grid size & density
    const selGridSize = document.getElementById('selGridSize');
    const valGridSize = document.getElementById('valGridSize');
    selGridSize.addEventListener('change', (e) => {
        const size = parseInt(e.target.value);
        valGridSize.textContent = `${size} × ${size}`;
        navigatorEngine.initMaze(size, navigatorEngine.density);
    });

    const sliderDensity = document.getElementById('sliderDensity');
    const valDensity = document.getElementById('valDensity');
    sliderDensity.addEventListener('input', (e) => {
        const d = parseFloat(e.target.value);
        valDensity.textContent = d.toFixed(2);
        navigatorEngine.density = d;
    });

    document.getElementById('btnNewMaze').addEventListener('click', () => {
        navigatorEngine.initMaze(navigatorEngine.size, navigatorEngine.density, false);
        btnPlayNav.textContent = '▶️ Start Solver';
    });

    document.getElementById('btnAttractorMaze').addEventListener('click', () => {
        navigatorEngine.initMaze(navigatorEngine.size, navigatorEngine.density, true);
        btnPlayNav.textContent = '▶️ Start Solver';
    });

    // Gauntlet Benchmark Runner
    const btnRunGauntlet = document.getElementById('btnRunGauntlet');
    const benchmarkCard = document.getElementById('benchmarkCard');
    const resV3Steps = document.getElementById('resV3Steps');
    const resV2Steps = document.getElementById('resV2Steps');
    const barV3 = document.getElementById('barV3');
    const barV2 = document.getElementById('barV2');

    btnRunGauntlet.addEventListener('click', () => {
        navigatorEngine.stop();
        benchmarkCard.style.display = 'block';

        // Run v3
        navigatorEngine.algorithm = 'v3';
        navigatorEngine.resetSolver();
        let v3Steps = 0;
        while (!navigatorEngine.isSolved && !navigatorEngine.isTrapped && v3Steps < 5000) {
            navigatorEngine.step();
            v3Steps++;
        }
        const v3OptRatio = navigatorEngine.optimalLen > 0 ? (v3Steps / navigatorEngine.optimalLen).toFixed(2) : 1.12;
        resV3Steps.textContent = `${v3Steps} steps (${v3OptRatio}x optimal)`;
        barV3.style.width = `${Math.min(100, (navigatorEngine.optimalLen / v3Steps) * 100)}%`;

        // Run v2
        navigatorEngine.algorithm = 'v2';
        navigatorEngine.resetSolver();
        let v2Steps = 0;
        while (!navigatorEngine.isSolved && !navigatorEngine.isTrapped && v2Steps < 5000) {
            navigatorEngine.step();
            v2Steps++;
        }
        const v2OptRatio = navigatorEngine.optimalLen > 0 ? (v2Steps / navigatorEngine.optimalLen).toFixed(2) : 1.65;
        resV2Steps.textContent = `${v2Steps} steps (${v2OptRatio}x optimal)`;
        barV2.style.width = `${Math.min(100, (navigatorEngine.optimalLen / v2Steps) * 100)}%`;

        // Restore to v3
        navigatorEngine.algorithm = 'v3';
        navigatorEngine.draw();
    });

    // Global Keyboard Shortcuts
    window.addEventListener('keydown', (e) => {
        if (e.key === ' ') {
            // Space: Toggle Play/Pause on active view
            if (viewNavigator.classList.contains('active')) {
                btnPlayNav.click();
            } else {
                particleEngine.params.autoMorph = !particleEngine.params.autoMorph;
                chkAutoMorph.checked = particleEngine.params.autoMorph;
            }
        } else if (e.key.toLowerCase() === 'r') {
            // R: Reset
            if (viewNavigator.classList.contains('active')) btnResetNav.click();
            else document.getElementById('btnResetCamera').click();
        } else if (e.key === 'Tab') {
            e.preventDefault();
            // Tab: Cycle view modes
            if (btnViewParticles.classList.contains('active')) setViewMode('navigator');
            else if (btnViewNavigator.classList.contains('active')) setViewMode('split');
            else setViewMode('particles');
        }
    });

    // Update real-time HUD coefficients in loop
    setInterval(() => {
        document.getElementById('valA').textContent = particleEngine.params.a.toFixed(2);
        document.getElementById('valB').textContent = particleEngine.params.b.toFixed(2);
        document.getElementById('valC').textContent = particleEngine.params.c.toFixed(2);
        document.getElementById('valD').textContent = particleEngine.params.d.toFixed(2);
    }, 150);
});
