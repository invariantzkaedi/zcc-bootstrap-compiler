/**
 * ZKAEDI Neon Creature - Legendary Compact v2 Motion Engine
 * Data-driven animation solver that implements the full priority-blended
 * modifier pipeline, supporting presets, moods, and macro scaling.
 */

// Embed the manifests and presets inline to ensure compatibility across local file:// and localhost
const legendaryManifest = {
  "v": "2.0.0",
  "rig": {
    "root": "creature_root",
    "nodes": [
      { "id": "creature_root", "p": null, "m": 2.0 },
      { "id": "chest", "p": "creature_root", "m": 1.7 },
      { "id": "head", "p": "chest", "m": 1.2 },
      { "id": "eyes", "p": "head", "m": 0.2 },
      { "id": "ear_l", "p": "head", "m": 0.45 },
      { "id": "ear_r", "p": "head", "m": 0.45 },
      { "id": "tail_root", "p": "creature_root", "m": 1.2 },
      { "id": "tail_mid", "p": "tail_root", "m": 0.85 },
      { "id": "tail_tip", "p": "tail_mid", "m": 0.6 },
      { "id": "fx_particles", "p": "creature_root", "m": 0.1 }
    ]
  },
  "audio": {
    "bands": [
      { "n": "bass", "lo": 20, "hi": 140, "w": 1.1 },
      { "n": "mid", "lo": 140, "hi": 2200, "w": 1.0 },
      { "n": "treble", "lo": 2200, "hi": 12000, "w": 0.9 }
    ],
    "env": { "atk": 30, "rel": 220, "rms": 0.72, "peak": 0.28 },
    "beat": { "th": 0.58, "hy": 0.08, "minMs": 140 }
  },
  "engine": {
    "seed": 1337,
    "macro": { "energy": 1.1, "elegance": 1.0, "chaos": 0.55, "weight": 1.0, "confidence": 1.05 },
    "state": { "mode": "reactive", "mood": "curious", "transitionMs": 140 },
    "order": ["input", "timing", "audio", "state", "periodic", "physics", "secondary", "spatial", "constraints", "arb", "safety", "out"],
    "budget": { "mods": 240, "physIters": 8, "lod": { "near": 1.0, "mid": 0.72, "far": 0.42 } },
    "safety": { "clamp": true, "antiPop": true, "nanGuard": true, "maxDelta": { "t": 25, "r": 18, "s": 0.08 } }
  },
  "tiers": {
    "active": "mastery",
    "blocks": {
      "low": {
        "on": true,
        "mods": [
          { "id": "hover", "fam": "periodic", "typ": "sine", "t": ["creature_root"], "pr": 100, "bm": "add", "p": { "prop": "ty", "a": 7.5, "ms": 3200 } },
          { "id": "breathe", "fam": "transform", "typ": "breath", "t": ["chest"], "pr": 120, "bm": "mul", "p": { "prop": "s", "a": 0.025, "ms": 2800 } },
          { "id": "glow", "fam": "secondary", "typ": "glowPulse", "t": ["chest", "head"], "pr": 115, "bm": "add", "p": { "prop": "em", "a": 0.22, "ms": 2100 } }
        ]
      },
      "medium": {
        "on": true,
        "mods": [
          { "id": "tail_wave", "fam": "periodic", "typ": "phaseChain", "t": ["tail_root", "tail_mid", "tail_tip"], "pr": 220, "bm": "add", "p": { "prop": "r", "a": 8.0, "pho": [0, 0.55, 1.05], "ms": 1400 } },
          { "id": "ears_mirror", "fam": "periodic", "typ": "mirrorSine", "t": ["ear_l", "ear_r"], "pr": 210, "bm": "add", "p": { "prop": "r", "a": 5.0, "ms": 1800 } },
          { "id": "tail_spring", "fam": "physics", "typ": "spring", "t": ["tail_mid", "tail_tip"], "pr": 260, "bm": "add", "p": { "k": 120, "d": 14, "m": 0.9 } }
        ]
      },
      "high": {
        "on": true,
        "mods": [
          { "id": "bass_body", "fam": "audioReactive", "typ": "bandDrive", "t": ["creature_root", "chest"], "pr": 330, "bm": "add", "p": { "band": "bass", "prop": "ty", "a": 9, "atk": 20, "rel": 170 } },
          { "id": "treble_ears", "fam": "audioReactive", "typ": "bandDrive", "t": ["ear_l", "ear_r"], "pr": 332, "bm": "add", "p": { "band": "treble", "prop": "r", "a": 4, "atk": 8, "rel": 90 } },
          { "id": "head_stable", "fam": "transform", "typ": "worldStabilize", "t": ["head", "eyes"], "pr": 420, "bm": "ovr", "sp": "world", "p": { "a": 0.65 } },
          { "id": "beat_gate", "fam": "timing", "typ": "hysteresisGate", "t": ["creature_root"], "pr": 310, "bm": "ovr", "p": { "min": 0.50, "max": 0.58 } }
        ]
      },
      "advanced": {
        "on": true,
        "mods": [
          { "id": "hybrid_env", "fam": "audioReactive", "typ": "rmsPeak", "t": ["chest", "tail_root"], "pr": 520, "bm": "add", "p": { "a": 1.0 } },
          { "id": "tail_whip", "fam": "secondary", "typ": "accentWhip", "t": ["tail_root", "tail_mid", "tail_tip"], "pr": 545, "bm": "add", "p": { "prob": 0.35, "cd": 320, "a": 14 } },
          { "id": "look_at", "fam": "spatial", "typ": "lookAt", "t": ["head", "eyes"], "pr": 560, "bm": "ovr", "sp": "world", "p": { "a": 0.6 } },
          { "id": "orbit_fx", "fam": "spatial", "typ": "ellipse", "t": ["fx_particles"], "pr": 500, "bm": "add", "sp": "world", "p": { "rx": 16, "ry": 9, "ms": 4200 } }
        ]
      },
      "mastery": {
        "on": true,
        "mods": [
          { "id": "intent_router", "fam": "control", "typ": "semanticIntent", "t": ["creature_root", "chest", "head", "tail_root", "ear_l", "ear_r"], "pr": 800, "bm": "ovr", "p": { "curve": "custom", "a": 1 } },
          { "id": "signature", "fam": "secondary", "typ": "motifGesture", "t": ["tail_root", "tail_mid", "tail_tip", "ear_l", "ear_r"], "pr": 780, "bm": "add", "p": { "prob": 0.12, "cd": 2800, "a": 1.2 } },
          { "id": "camera_scale", "fam": "control", "typ": "cameraAware", "t": ["creature_root", "head", "tail_root"], "pr": 820, "bm": "mul", "sp": "world", "p": { "min": 0.75, "max": 1.25 } },
          { "id": "perf_guard", "fam": "control", "typ": "budgetCap", "t": ["creature_root"], "pr": 900, "bm": "ovr", "p": { "min": 0, "max": 1 } }
        ]
      }
    }
  },
  "presets": {
    "active": ["legend_alive_idle", "legend_reactive_tail", "legend_neon_aura", "legend_2p5d_scene"],
    "defs": [
      { "id": "legend_alive_idle", "stack": ["hover", "breathe", "glow", "head_stable"], "gain": 1.0 },
      { "id": "legend_reactive_tail", "stack": ["tail_wave", "tail_spring", "bass_body", "tail_whip"], "gain": 1.1 },
      { "id": "legend_neon_aura", "stack": ["glow", "treble_ears", "orbit_fx"], "gain": 1.05 },
      { "id": "legend_2p5d_scene", "stack": ["look_at", "camera_scale", "orbit_fx"], "gain": 0.95 },
      { "id": "legend_drop_moment", "stack": ["beat_gate", "hybrid_env", "signature"], "gain": 1.2 }
    ]
  }
};

const moodPacks = [
  {
    "id": "calm_ambient",
    "name": "Calm Ambient",
    "description": "Soft breathy motion, low chaos, elegant drift, minimal spikes.",
    "macro": { "energy": 0.62, "elegance": 1.45, "chaos": 0.18, "weight": 0.92, "confidence": 0.88 },
    "state": { "mode": "idle", "mood": "calm" }
  },
  {
    "id": "festival_hype",
    "name": "Festival Hype",
    "description": "High energy, punchy transients, vivid aura, aggressive accents.",
    "macro": { "energy": 1.72, "elegance": 0.72, "chaos": 1.22, "weight": 1.08, "confidence": 1.42 },
    "state": { "mode": "reactive", "mood": "alert" }
  },
  {
    "id": "boss_encounter",
    "name": "Boss Encounter",
    "description": "Heavy threatening cadence, deliberate power, controlled violent accents.",
    "macro": { "energy": 1.28, "elegance": 0.94, "chaos": 0.62, "weight": 1.7, "confidence": 1.66 },
    "state": { "mode": "cinematic", "mood": "aggressive" }
  },
  {
    "id": "lofi_chill",
    "name": "Lo-Fi Chill",
    "description": "Warm, sleepy groove, subtle swing feel, low transient harshness.",
    "macro": { "energy": 0.74, "elegance": 1.36, "chaos": 0.34, "weight": 1.12, "confidence": 0.92 },
    "state": { "mode": "idle", "mood": "curious" }
  }
];

// Rig pivots in coordinates space
const rigConfig = {
  creature_root: { cx: 500, cy: 900 },
  chest: { cx: 500, cy: 680 },
  head: { cx: 520, cy: 380 },
  ear_l: { cx: 330, cy: 340 },
  ear_r: { cx: 650, cy: 420 },
  tail_root: { cx: 180, cy: 680 },
  tail_mid: { cx: 115, cy: 755 },
  tail_tip: { cx: 80, cy: 800 },
  eyes: { cx: 525, cy: 340 },
  pupil_left: { cx: 525, cy: 340 },
  glow_back: { cx: 500, cy: 500 }
};

// Map Manifest Node names directly to SVG IDs
const nodeToSvgId = {
  creature_root: "creature_root",
  chest: "body",
  head: "head",
  ear_l: "ear_left",
  ear_r: "ear_right",
  tail_root: "tail",
  tail_mid: "tail", // Nested transforms applied to tail
  tail_tip: "tail",
  eyes: "eye_left",
  pupil_left: "pupil_left",
  glow_back: "glow_back"
};

// Application State
const state = {
  audioContext: null,
  analyser: null,
  audioSource: null,
  audioElement: null,
  isPlaying: false,
  isInitialized: false,
  
  // Audio analysis variables
  fftSize: 2048,
  smoothing: 0.84,
  bands: {
    subBass: 0,
    bass: 0,
    lowMid: 0,
    mid: 0,
    highMid: 0,
    treble: 0
  },
  smoothedBands: {
    subBass: 0,
    bass: 0,
    lowMid: 0,
    mid: 0,
    highMid: 0,
    treble: 0
  },
  rms: 0,
  smoothedRms: 0,
  
  // Beat Detection
  beatHistory: [],
  historyLength: 60,
  beatCooldown: 170, // ms
  lastBeatTime: 0,
  isBeat: false,
  beatVal: 0,
  
  // Wavefront Plasma Propagation
  ripples: [],
  waveSpeed: 16.0,
  
  // Particle Emitter
  particles: [],
  particleId: 0,
  
  // Engine Macro Parameters (Legendary v2)
  macro: {
    energy: 1.1,
    elegance: 1.0,
    chaos: 0.55,
    weight: 1.0,
    confidence: 1.05
  },
  currentMood: "lofi_chill",
  activeModifierStack: [],
  
  // Settings driven by UI sliders
  beatSensitivity: 1.28,
  motionIntensity: 1.0,
  glowIntensity: 1.0,
  idleEnabled: true,
  particlesEnabled: false,
  debugEnabled: true,
  anchorsEnabled: false,

  // Diagnostics
  fps: 0,
  lastFrameTime: 0,
  svgVerified: false,
  missingGroups: [],
  bpm: 120,
  beatTimes: []
};

// Target DOM Elements
let svgDoc = null;
const elements = {};
const pathMetadata = [];

// Node transform accumulators
const rigNodePoses = {};

// Initialize transform accumulators for each rig node
function initNodePoses() {
  legendaryManifest.rig.nodes.forEach(node => {
    rigNodePoses[node.id] = {
      tx: 0,
      ty: 0,
      r: 0,
      s: 1.0,
      em: 0
    };
  });
}

// Linear interpolation utility
function lerp(start, end, amt) {
  return (1 - amt) * start + amt * end;
}

// Interpolate stroke color towards pure white plasma peak
function lerpColor(baseColor, factor) {
  if (factor <= 0.01) return baseColor;
  let r, g, b;
  if (baseColor === '#00F2FF') { // Cyan
    r = Math.round(lerp(0, 255, factor));
    g = Math.round(lerp(242, 255, factor));
    b = 255;
  } else { // Magenta (#FF55DB)
    r = 255;
    g = Math.round(lerp(85, 255, factor));
    b = Math.round(lerp(219, 255, factor));
  }
  return `rgb(${r},${g},${b})`;
}

// Map centroids and Euclidean distance of all paths from the eye origin
function indexSvgPaths() {
  if (!svgDoc) return;
  const pathElements = svgDoc.getElementsByTagName('path');
  const originX = rigConfig.eyes.cx;
  const originY = rigConfig.eyes.cy;
  
  pathMetadata.length = 0;
  
  for (let i = 0; i < pathElements.length; i++) {
    const el = pathElements[i];
    const d = el.getAttribute('d');
    if (!d) continue;
    
    const pts = [];
    const re = /([ML])\s*(-?\d+\.?\d*)\s+(-?\d+\.?\d*)/g;
    let match;
    while ((match = re.exec(d)) !== null) {
      pts.push({ x: parseFloat(match[2]), y: parseFloat(match[3]) });
    }
    
    if (pts.length === 0) continue;
    
    let sumX = 0, sumY = 0;
    pts.forEach(p => {
      sumX += p.x;
      sumY += p.y;
    });
    const cx = sumX / pts.length;
    const cy = sumY / pts.length;
    
    const dist = Math.sqrt((cx - originX) * (cx - originX) + (cy - originY) * (cy - originY));
    
    pathMetadata.push({
      el: el,
      baseColor: el.getAttribute('stroke') || '#FF55DB',
      baseWidth: parseFloat(el.getAttribute('stroke-width')) || 2.8,
      dist: dist
    });
  }
  console.log(`Indexed ${pathMetadata.length} paths for spatial wavefront solver.`);
}

// Check if all needed SVG groups are loaded and store references
function verifySvgElements() {
  const container = document.getElementById('svg-container');
  const svg = container.querySelector('svg');
  if (!svg) {
    state.svgVerified = false;
    state.missingGroups = ['<svg> element not found'];
    return;
  }
  
  svgDoc = svg;
  
  const requiredIds = [
    'creature_root', 'body', 'head', 'ear_left', 'ear_right', 
    'tail', 'eye_left', 'pupil_left', 'glow_back', 'fx_particles'
  ];
  
  state.missingGroups = [];
  
  requiredIds.forEach(id => {
    const el = svg.getElementById(id);
    if (el) {
      elements[id] = el;
    } else {
      state.missingGroups.push(id);
    }
  });
  
  state.svgVerified = state.missingGroups.length === 0;
  
  if (!state.svgVerified) {
    console.error("Missing SVG elements: ", state.missingGroups);
  } else {
    console.log("SVG Skeletal Rig verified successfully.");
    indexSvgPaths();
  }
}

// Initialize Web Audio API
function initAudio() {
  if (state.isInitialized) return;
  
  const AudioContextClass = window.AudioContext || window.webkitAudioContext;
  state.audioContext = new AudioContextClass();
  
  state.analyser = state.audioContext.createAnalyser();
  state.analyser.fftSize = state.fftSize;
  state.analyser.smoothingTimeConstant = state.smoothing;
  
  state.audioElement = document.getElementById('audio-player');
  state.audioSource = state.audioContext.createMediaElementSource(state.audioElement);
  
  state.audioSource.connect(state.analyser);
  state.analyser.connect(state.audioContext.destination);
  
  state.isInitialized = true;
  console.log("Web Audio Context and nodes initialized.");
}

// Process frequency bands
function analyzeAudio() {
  if (!state.isInitialized || !state.isPlaying) return;
  
  const bufferLength = state.analyser.frequencyBinCount;
  const dataArray = new Uint8Array(bufferLength);
  state.analyser.getByteFrequencyData(dataArray);
  
  const sampleRate = state.audioContext.sampleRate;
  
  const freqToBin = (freq) => {
    return Math.min(
      Math.floor((freq * state.fftSize) / sampleRate),
      bufferLength - 1
    );
  };
  
  // Define frequency ranges matching schema
  const bins = {
    subBass: [freqToBin(20), freqToBin(60)],
    bass: [freqToBin(60), freqToBin(180)],
    lowMid: [freqToBin(180), freqToBin(500)],
    mid: [freqToBin(500), freqToBin(2000)],
    highMid: [freqToBin(2000), freqToBin(6000)],
    treble: [freqToBin(6000), freqToBin(16000)]
  };
  
  let totalSum = 0;
  for (const [key, range] of Object.entries(bins)) {
    let sum = 0;
    const count = range[1] - range[0] + 1;
    if (count > 0) {
      for (let i = range[0]; i <= range[1]; i++) {
        sum += dataArray[i];
      }
      state.bands[key] = (sum / count) / 255;
    } else {
      state.bands[key] = 0;
    }
    totalSum += sum;
  }
  
  state.rms = (totalSum / bufferLength) / 255;
  
  let maxVal = 0.0001;
  for (const val of Object.values(state.bands)) {
    if (val > maxVal) maxVal = val;
  }
  const adaptiveScaler = maxVal < 0.15 ? (0.15 / maxVal) : 1.0;
  
  const attack = 0.35;
  const release = 0.15;
  
  for (const key of Object.keys(state.bands)) {
    let target = state.bands[key] * adaptiveScaler;
    target = Math.min(target, 1.0);
    
    const current = state.smoothedBands[key];
    const rate = target > current ? attack : release;
    state.smoothedBands[key] = lerp(current, target, rate);
  }
  
  state.smoothedRms = lerp(state.smoothedRms, state.rms * adaptiveScaler, 0.2);
  state.smoothedRms = Math.min(state.smoothedRms, 1.0);
  
  detectBeats();
}

// Beat onset detection from subBass/bass energy flux
function detectBeats() {
  const currentBassEnergy = (state.bands.subBass + state.bands.bass) / 2;
  
  state.beatHistory.push(currentBassEnergy);
  if (state.beatHistory.length > state.historyLength) {
    state.beatHistory.shift();
  }
  
  const sum = state.beatHistory.reduce((a, b) => a + b, 0);
  const averageBassEnergy = sum / state.beatHistory.length;
  
  const now = performance.now();
  state.isBeat = false;
  
  if (currentBassEnergy > averageBassEnergy * state.beatSensitivity && currentBassEnergy > 0.08) {
    if (now - state.lastBeatTime > state.beatCooldown) {
      state.isBeat = true;
      state.beatVal = 1.0;
      state.lastBeatTime = now;
      
      // Inject propagating wave
      state.ripples.push({
        radius: 0,
        speed: state.waveSpeed,
        amp: 1.0,
        decay: 0.018,
        width: 85
      });
      
      state.beatTimes.push(now);
      if (state.beatTimes.length > 8) state.beatTimes.shift();
      calculateBpm();
      
      if (state.particlesEnabled) {
        spawnBeatParticles();
      }
    }
  }
  
  state.beatVal = Math.max(0, state.beatVal - 0.08);
}

// Calculate BPM from past beat intervals
function calculateBpm() {
  if (state.beatTimes.length < 3) return;
  const intervals = [];
  for (let i = 1; i < state.beatTimes.length; i++) {
    intervals.push(state.beatTimes[i] - state.beatTimes[i - 1]);
  }
  const avgInterval = intervals.reduce((a, b) => a + b, 0) / intervals.length;
  const tempBpm = Math.round(60000 / avgInterval);
  if (tempBpm >= 60 && tempBpm <= 220) {
    state.bpm = tempBpm;
  }
}

// Spawn visual particles
function spawnBeatParticles() {
  if (!elements.fx_particles) return;
  
  const sources = [
    { x: 525, y: 340, color: '#FF55DB' }, // Red eye
    { x: 650, y: 420, color: '#00F2FF' }, // Trunk base
    { x: 500, y: 680, color: '#FF55DB' }  // Body center
  ];
  
  const count = Math.floor(Math.random() * 3) + 2;
  
  for (let i = 0; i < count; i++) {
    const src = sources[Math.floor(Math.random() * sources.length)];
    const angle = Math.random() * Math.PI * 2;
    const speed = Math.random() * 4 + 3;
    
    const p = {
      id: state.particleId++,
      x: src.x,
      y: src.y,
      vx: Math.cos(angle) * speed,
      vy: Math.sin(angle) * speed - 1.5,
      size: Math.random() * 6 + 4,
      color: src.color,
      opacity: 1.0,
      life: 1.0,
      decay: Math.random() * 0.02 + 0.015
    };
    
    state.particles.push(p);
    
    const circle = document.createElementNS("http://www.w3.org/2000/svg", "circle");
    circle.setAttribute('id', `p-${p.id}`);
    circle.setAttribute('cx', p.x);
    circle.setAttribute('cy', p.y);
    circle.setAttribute('r', p.size);
    circle.setAttribute('fill', p.color);
    circle.setAttribute('filter', p.color === '#00F2FF' ? 'url(#cyanGlow)' : 'url(#magentaGlow)');
    circle.setAttribute('opacity', p.opacity);
    
    elements.fx_particles.appendChild(circle);
  }
}

// Update particle positions
function updateParticles() {
  const container = elements.fx_particles;
  if (!container) return;
  
  for (let i = state.particles.length - 1; i >= 0; i--) {
    const p = state.particles[i];
    p.x += p.vx;
    p.y += p.vy;
    p.vy += 0.08;
    p.life -= p.decay;
    p.opacity = p.life;
    
    const circle = document.getElementById(`p-${p.id}`);
    if (circle) {
      if (p.life <= 0) {
        circle.remove();
        state.particles.splice(i, 1);
      } else {
        circle.setAttribute('cx', p.x);
        circle.setAttribute('cy', p.y);
        circle.setAttribute('opacity', p.opacity);
      }
    } else {
      state.particles.splice(i, 1);
    }
  }
}

// Clear all particles
function clearParticles() {
  state.particles = [];
  if (elements.fx_particles) {
    elements.fx_particles.innerHTML = '';
  }
}

// Apply mood preset parameters
function selectMood(moodId) {
  const pack = moodPacks.find(p => p.id === moodId);
  if (!pack) return;
  
  state.currentMood = moodId;
  
  // Apply macro overrides
  Object.keys(pack.macro).forEach(key => {
    state.macro[key] = pack.macro[key];
    const slider = document.getElementById(`slider-${key}`);
    if (slider) {
      slider.value = pack.macro[key];
    }
  });
  
  // Set UI Description
  const moodDesc = document.getElementById('mood-description');
  if (moodDesc) {
    moodDesc.textContent = pack.description;
  }
  
  console.log(`Mood changed to: ${pack.name}`);
}

// Get final calculated amplitude scaled by macro factors (Conflict Resolution rule 6)
function getComposedGain(baseAmp) {
  const energy = state.macro.energy;
  const confidence = state.macro.confidence;
  const elegance = state.macro.elegance;
  
  // Composed gain rule matching conflict-resolution.legendary.md
  return baseAmp * energy * (1.0 + 0.25 * confidence) * (1.0 - 0.1 * (2.0 - elegance));
}

// Solve data-driven rig modifier equations (Legendary v2 engine solver)
function solveRigModifiers(timestamp) {
  // Reset all target node poses
  Object.keys(rigNodePoses).forEach(nodeId => {
    rigNodePoses[nodeId] = {
      tx: 0,
      ty: 0,
      r: 0,
      s: 1.0,
      em: 0
    };
  });
  
  state.activeModifierStack = [];
  
  // Collect all active modifiers from manifest
  const activeTiers = ["low", "medium", "high", "advanced", "mastery"];
  const allModifiers = [];
  
  activeTiers.forEach(tier => {
    const block = legendaryManifest.tiers.blocks[tier];
    if (block && block.on) {
      block.mods.forEach(mod => {
        if (mod.on !== false) {
          allModifiers.push(mod);
        }
      });
    }
  });
  
  // Sort modifiers by priority ascending (Rule 4: execution order / deterministic tie-breakers)
  allModifiers.sort((a, b) => {
    if (a.pr !== b.pr) return a.pr - b.pr;
    // Tie-breaker 1: Blend precedence
    const bmOrder = { ovr: 3, mul: 2, add: 1 };
    const aBm = bmOrder[a.bm] || 0;
    const bBm = bmOrder[b.bm] || 0;
    if (aBm !== bBm) return bBm - aBm;
    // Tie-breaker 2: Lexical id
    return a.id.localeCompare(b.id);
  });
  
  // Solve each active modifier
  allModifiers.forEach(mod => {
    state.activeModifierStack.push(mod.id);
    
    // Solve value of modifier based on class
    let val = 0;
    const p = mod.p;
    
    // Scale amplitude based on composed macro rules
    const amp = getComposedGain(p.a || 1.0) * state.motionIntensity;
    
    if (mod.typ === "sine") {
      const freq = 1000 / (p.ms || 3000);
      val = Math.sin((timestamp * 2 * Math.PI * freq) / 1000) * amp;
    } 
    else if (mod.typ === "breath") {
      const freq = 1000 / (p.ms || 2800);
      val = (Math.sin((timestamp * 2 * Math.PI * freq) / 1000) * 0.5 + 0.5) * amp;
    }
    else if (mod.typ === "glowPulse") {
      const freq = 1000 / (p.ms || 2100);
      val = (Math.cos((timestamp * 2 * Math.PI * freq) / 1000) * 0.5 + 0.5) * amp;
    }
    else if (mod.typ === "mirrorSine") {
      const freq = 1000 / (p.ms || 1800);
      val = Math.sin((timestamp * 2 * Math.PI * freq) / 1000) * amp;
    }
    else if (mod.typ === "phaseChain") {
      // Targets are solved individually, handled inside target loop
    }
    else if (mod.typ === "bandDrive") {
      val = state.smoothedBands[p.band || "bass"] * amp;
    }
    else if (mod.typ === "spring") {
      // Basic spring-damper tracking low frequencies
      val = state.smoothedBands.bass * amp;
    }
    else if (mod.typ === "hysteresisGate") {
      // Beat triggered bounce
      val = state.beatVal * amp;
    }
    else if (mod.typ === "rmsPeak") {
      val = state.smoothedRms * amp;
    }
    else if (mod.typ === "accentWhip") {
      // Adds random whip elements
      if (state.isBeat && Math.random() < (p.prob || 0.3)) {
        val = amp * 1.5;
      } else {
        val = state.beatVal * amp * 0.4;
      }
    }
    else if (mod.typ === "lookAt" || mod.typ === "cameraAware") {
      val = state.smoothedRms * amp;
    }
    
    // Apply solved values to targets
    mod.t.forEach((targetNodeId, targetIndex) => {
      const pose = rigNodePoses[targetNodeId];
      if (!pose) return;
      
      let targetVal = val;
      if (mod.typ === "phaseChain") {
        const offset = p.pho ? (p.pho[targetIndex] || 0) : 0;
        const freq = 1000 / (p.ms || 1400);
        targetVal = Math.sin(((timestamp * 2 * Math.PI * freq) / 1000) + offset) * amp;
      }
      else if (mod.typ === "mirrorSine" && targetNodeId === "ear_r") {
        targetVal = -targetVal; // mirrored
      }
      
      const prop = p.prop || "r";
      
      // Merge rules matching conflict-resolution channel blend precedence (Rule 2)
      if (mod.bm === "ovr") {
        pose[prop] = targetVal;
      } else if (mod.bm === "mul") {
        if (prop === "s") {
          pose[prop] *= (1.0 + targetVal);
        } else {
          pose[prop] *= targetVal;
        }
      } else { // add
        pose[prop] += targetVal;
      }
    });
  });
}

// Apply solved transforms to SVG rig elements
function animateRig(timestamp) {
  if (!state.lastFrameTime) state.lastFrameTime = timestamp;
  const delta = timestamp - state.lastFrameTime;
  state.fps = Math.round(1000 / delta);
  state.lastFrameTime = timestamp;
  
  analyzeAudio();
  
  if (!state.svgVerified) {
    requestAnimationFrame(animateRig);
    return;
  }
  
  // 1. Solve Legendary v2 Modifiers Engine
  solveRigModifiers(timestamp);
  
  // 2. Solve Wavefront Plasma Propagation across paths
  const intensity = state.motionIntensity;
  
  // Update ripples
  for (let i = state.ripples.length - 1; i >= 0; i--) {
    const r = state.ripples[i];
    r.radius += r.speed;
    r.amp -= r.decay;
    if (r.amp <= 0 || r.radius > 1200) {
      state.ripples.splice(i, 1);
    }
  }
  
  // Solve wave interaction for each path
  pathMetadata.forEach(path => {
    let maxFactor = 0;
    
    state.ripples.forEach(r => {
      const diff = Math.abs(path.dist - r.radius);
      if (diff < r.width) {
        const factor = Math.exp(- (diff * diff) / (2 * 30 * 30)) * r.amp;
        if (factor > maxFactor) maxFactor = factor;
      }
    });
    
    if (maxFactor > 0.01) {
      const strokeWidth = path.baseWidth * (1.0 + maxFactor * 1.8 * intensity);
      const strokeColor = lerpColor(path.baseColor, maxFactor);
      path.el.setAttribute('stroke', strokeColor);
      path.el.setAttribute('stroke-width', strokeWidth);
      path.el.style.opacity = lerp(0.75, 1.0, maxFactor);
    } else {
      path.el.setAttribute('stroke', path.baseColor);
      path.el.setAttribute('stroke-width', path.baseWidth);
      path.el.style.opacity = '';
    }
  });
  
  // 3. Render Poses on DOM elements
  // Apply accumulators matching anchors
  Object.keys(rigNodePoses).forEach(nodeId => {
    const svgId = nodeToSvgId[nodeId];
    const el = elements[svgId];
    if (!el) return;
    
    const pose = rigNodePoses[nodeId];
    const anchor = rigConfig[nodeId];
    
    let transform = "";
    
    // Apply translation and rotation around anchors
    if (nodeId === "creature_root") {
      transform = `translate(${pose.tx}, ${pose.ty}) rotate(${pose.r}, ${anchor.cx}, ${anchor.cy}) scale(${pose.s})`;
    } 
    else if (nodeId === "chest") {
      transform = `scale(${pose.s})`;
    }
    else if (nodeId === "head") {
      transform = `translate(${pose.tx}, ${pose.ty}) rotate(${pose.r}, ${anchor.cx}, ${anchor.cy})`;
    }
    else if (nodeId === "ear_l" || nodeId === "ear_r" || nodeId === "tail_root" || nodeId === "tail_mid" || nodeId === "tail_tip") {
      transform = `rotate(${pose.r}, ${anchor.cx}, ${anchor.cy})`;
    }
    else if (nodeId === "eyes") {
      transform = `scale(${pose.s})`;
    }
    else if (nodeId === "pupil_left") {
      transform = `scale(${pose.s})`;
    }
    
    if (transform) {
      el.setAttribute('transform', transform);
    }
    
    // Update Glow/Emission properties
    if (nodeId === "glow_back") {
      const glowTargetOpacity = Math.min(pose.em * state.glowIntensity, 0.95);
      el.setAttribute('opacity', glowTargetOpacity);
      
      const blurBase = 12;
      const stdDev = blurBase + pose.em * 16;
      const cyanFilter = svgDoc.getElementById('cyanGlow');
      const magentaFilter = svgDoc.getElementById('magentaGlow');
      if (cyanFilter) {
        const blur = cyanFilter.querySelector('feGaussianBlur');
        if (blur) blur.setAttribute('stdDeviation', Math.min(stdDev * 0.85, 30));
      }
      if (magentaFilter) {
        const blur = magentaFilter.querySelector('feGaussianBlur');
        if (blur) blur.setAttribute('stdDeviation', Math.min(stdDev * 0.75, 28));
      }
    }
  });
  
  // 4. Update particles
  updateParticles();
  
  // Update Debug GUI Display
  if (state.debugEnabled) {
    updateDebugGui();
  }
  
  // Render anchors if overlay is enabled
  updateAnchorVisualizer();
  
  requestAnimationFrame(animateRig);
}

// Render dynamic overlays for anchor points
function updateAnchorVisualizer() {
  const overlay = document.getElementById('anchor-overlay');
  if (!overlay) return;
  
  if (!state.anchorsEnabled) {
    overlay.innerHTML = '';
    return;
  }
  
  const container = document.getElementById('svg-container');
  const svg = container.querySelector('svg');
  if (!svg) return;
  
  const rect = svg.getBoundingClientRect();
  const widthRatio = rect.width / 1000;
  const heightRatio = rect.height / 1000;
  
  let html = '';
  for (const [name, anchor] of Object.entries(rigConfig)) {
    const svgId = nodeToSvgId[name];
    const element = elements[svgId];
    if (!element) continue;
    
    const x = rect.left + (anchor.cx * widthRatio);
    const y = rect.top + (anchor.cy * heightRatio);
    
    html += `
      <div class="anchor-point" style="left: ${x}px; top: ${y}px;">
        <span class="anchor-label">${name}</span>
      </div>
    `;
  }
  overlay.innerHTML = html;
}

// GUI debug panel updating
function updateDebugGui() {
  const container = document.getElementById('debug-spectrum');
  if (!container) return;
  
  const canvas = document.getElementById('debug-canvas');
  if (!canvas) return;
  
  const ctx = canvas.getContext('2d');
  const width = canvas.width;
  const height = canvas.height;
  
  ctx.clearRect(0, 0, width, height);
  
  const bandKeys = Object.keys(state.smoothedBands);
  const barWidth = width / bandKeys.length;
  
  bandKeys.forEach((key, index) => {
    const val = state.smoothedBands[key];
    const barHeight = val * (height - 30);
    
    ctx.fillStyle = index % 2 === 0 ? '#00F2FF' : '#FF55DB';
    ctx.fillRect(index * barWidth + 5, height - barHeight - 20, barWidth - 10, barHeight);
    
    ctx.fillStyle = '#fff';
    ctx.font = '10px monospace';
    ctx.textAlign = 'center';
    ctx.fillText(key.substring(0, 7), index * barWidth + barWidth / 2, height - 5);
    ctx.fillText(val.toFixed(2), index * barWidth + barWidth / 2, height - barHeight - 25);
  });
  
  document.getElementById('diag-fps').textContent = state.fps;
  document.getElementById('diag-bpm').textContent = state.bpm;
  document.getElementById('diag-rms').textContent = state.smoothedRms.toFixed(3);
  document.getElementById('diag-bass').textContent = state.bands.bass.toFixed(3);
  document.getElementById('diag-ripples').textContent = state.ripples.length;
  document.getElementById('diag-mods').textContent = state.activeModifierStack.length;
  
  const indicator = document.getElementById('beat-indicator');
  if (state.isBeat) {
    indicator.classList.add('beat-flash');
    setTimeout(() => indicator.classList.remove('beat-flash'), 100);
  }
}

// UI Event Handlers
document.addEventListener("DOMContentLoaded", () => {
  initNodePoses();
  verifySvgElements();
  
  const playBtn = document.getElementById('btn-play');
  const pauseBtn = document.getElementById('btn-pause');
  const fileInput = document.getElementById('file-upload');
  const dropZone = document.getElementById('drop-zone');
  const volumeSlider = document.getElementById('slider-volume');
  const sensitivitySlider = document.getElementById('slider-sensitivity');
  const motionSlider = document.getElementById('slider-motion');
  const glowSlider = document.getElementById('slider-glow');
  const speedSlider = document.getElementById('slider-speed');
  
  // Mood Pack controls
  const moodSelect = document.getElementById('select-mood');
  
  // Macro controls
  const sliderEnergy = document.getElementById('slider-energy');
  const sliderElegance = document.getElementById('slider-elegance');
  const sliderChaos = document.getElementById('slider-chaos');
  const sliderWeight = document.getElementById('slider-weight');
  const sliderConfidence = document.getElementById('slider-confidence');
  
  const toggleIdle = document.getElementById('toggle-idle');
  const toggleParticles = document.getElementById('toggle-particles');
  const toggleDebug = document.getElementById('toggle-debug');
  const toggleAnchors = document.getElementById('toggle-anchors');
  const btnReset = document.getElementById('btn-reset');
  
  const rigStatus = document.getElementById('rig-status');
  if (state.svgVerified) {
    rigStatus.textContent = "VERIFIED (10/10 groups operational)";
    rigStatus.className = "status-green";
  } else {
    rigStatus.textContent = `CRITICAL DEVIATION (missing: ${state.missingGroups.join(', ')})`;
    rigStatus.className = "status-red";
  }
  
  function handlePlay() {
    if (!state.isInitialized) {
      initAudio();
    }
    if (state.audioContext && state.audioContext.state === 'suspended') {
      state.audioContext.resume();
    }
    state.audioElement.play();
    state.isPlaying = true;
    playBtn.style.display = 'none';
    pauseBtn.style.display = 'inline-block';
  }
  
  playBtn.addEventListener('click', handlePlay);
  
  pauseBtn.addEventListener('click', () => {
    state.audioElement.pause();
    state.isPlaying = false;
    pauseBtn.style.display = 'none';
    playBtn.style.display = 'inline-block';
  });
  
  fileInput.addEventListener('change', (e) => {
    const file = e.target.files[0];
    if (file) {
      loadAudioFile(file);
    }
  });
  
  dropZone.addEventListener('dragover', (e) => {
    e.preventDefault();
    dropZone.classList.add('dragover');
  });
  
  dropZone.addEventListener('dragleave', () => {
    dropZone.classList.remove('dragover');
  });
  
  dropZone.addEventListener('drop', (e) => {
    e.preventDefault();
    dropZone.classList.remove('dragover');
    const file = e.dataTransfer.files[0];
    if (file && file.type.startsWith('audio/')) {
      loadAudioFile(file);
    } else {
      alert("Invalid file: upload an MP3 file.");
    }
  });
  
  function loadAudioFile(file) {
    const objectURL = URL.createObjectURL(file);
    const audioPlayer = document.getElementById('audio-player');
    audioPlayer.src = objectURL;
    document.getElementById('track-title').textContent = file.name;
    setTimeout(handlePlay, 100);
  }
  
  // Slider listeners
  volumeSlider.addEventListener('input', (e) => {
    const val = parseFloat(e.target.value);
    document.getElementById('audio-player').volume = val;
  });
  
  sensitivitySlider.addEventListener('input', (e) => {
    state.beatSensitivity = parseFloat(e.target.value);
  });
  
  motionSlider.addEventListener('input', (e) => {
    state.motionIntensity = parseFloat(e.target.value);
  });
  
  glowSlider.addEventListener('input', (e) => {
    state.glowIntensity = parseFloat(e.target.value);
  });
  
  if (speedSlider) {
    speedSlider.addEventListener('input', (e) => {
      state.waveSpeed = parseFloat(e.target.value);
    });
  }
  
  // Mood Selection Change
  if (moodSelect) {
    moodSelect.addEventListener('change', (e) => {
      selectMood(e.target.value);
    });
    // Set default selection
    selectMood("lofi_chill");
  }
  
  // Macro sliders interaction
  const macroSliders = [
    { id: 'energy', el: sliderEnergy },
    { id: 'elegance', el: sliderElegance },
    { id: 'chaos', el: sliderChaos },
    { id: 'weight', el: sliderWeight },
    { id: 'confidence', el: sliderConfidence }
  ];
  
  macroSliders.forEach(macro => {
    if (macro.el) {
      macro.el.addEventListener('input', (e) => {
        state.macro[macro.id] = parseFloat(e.target.value);
      });
    }
  });
  
  toggleIdle.addEventListener('change', (e) => {
    state.idleEnabled = e.target.checked;
  });
  
  toggleParticles.addEventListener('change', (e) => {
    state.particlesEnabled = e.target.checked;
    if (!state.particlesEnabled) clearParticles();
  });
  
  toggleDebug.addEventListener('change', (e) => {
    state.debugEnabled = e.target.checked;
    document.getElementById('debug-spectrum').style.display = state.debugEnabled ? 'block' : 'none';
  });
  
  toggleAnchors.addEventListener('change', (e) => {
    state.anchorsEnabled = e.target.checked;
    if (!state.anchorsEnabled) {
      document.getElementById('anchor-overlay').innerHTML = '';
    }
  });
  
  btnReset.addEventListener('click', () => {
    volumeSlider.value = 0.8;
    document.getElementById('audio-player').volume = 0.8;
    
    sensitivitySlider.value = 1.35;
    state.beatSensitivity = 1.35;
    
    motionSlider.value = 1.0;
    state.motionIntensity = 1.0;
    
    glowSlider.value = 1.0;
    state.glowIntensity = 1.0;
    
    if (speedSlider) {
      speedSlider.value = 16.0;
      state.waveSpeed = 16.0;
    }
    
    if (moodSelect) {
      moodSelect.value = "lofi_chill";
      selectMood("lofi_chill");
    }
    
    toggleIdle.checked = true;
    state.idleEnabled = true;
    
    toggleParticles.checked = false;
    state.particlesEnabled = false;
    clearParticles();
    
    toggleDebug.checked = true;
    state.debugEnabled = true;
    document.getElementById('debug-spectrum').style.display = 'block';
    
    toggleAnchors.checked = false;
    state.anchorsEnabled = false;
    document.getElementById('anchor-overlay').innerHTML = '';
  });
  
  window.addEventListener('resize', updateAnchorVisualizer);
  requestAnimationFrame(animateRig);
});
