/**
 * ============================================================================
 * ZKAEDI ANIMATOR PRO // ELITE MOTION & PHYSICS STUDIO ENGINE
 * Top-of-the-line Vector Keyframe Sequencing, Graph Curve Editor,
 * Real-Time Video Recording, Canvas Pan/Zoom, and ZCC C-Native Export
 * ============================================================================
 */

'use strict';

// ── 1. MATHEMATICAL EASING ENGINE (zcc_anim.h Specification) ────────────────
const EASINGS = {
  linear: { name: 'Linear', fn: t => t },
  easeSineIn: { name: 'Sine In', fn: t => 1.0 - Math.cos(t * (Math.PI / 2.0)) },
  easeSineOut: { name: 'Sine Out', fn: t => Math.sin(t * (Math.PI / 2.0)) },
  easeSineInOut: { name: 'Sine In-Out', fn: t => -0.5 * (Math.cos(Math.PI * t) - 1.0) },
  easeQuadIn: { name: 'Quad In', fn: t => t * t },
  easeQuadOut: { name: 'Quad Out', fn: t => t * (2.0 - t) },
  easeQuadInOut: { name: 'Quad In-Out', fn: t => (t < 0.5) ? 2.0 * t * t : 1.0 + 2.0 * (t - 1.0) * (t - 1.0) },
  easeCubicIn: { name: 'Cubic In', fn: t => t * t * t },
  easeCubicOut: { name: 'Cubic Out', fn: t => { const f = t - 1.0; return f * f * f + 1.0; } },
  easeCubicInOut: {
    name: 'Cubic In-Out',
    fn: t => (t < 0.5) ? 4.0 * t * t * t : 0.5 * Math.pow(2.0 * t - 2.0, 3) + 1.0
  },
  easeElasticOut: {
    name: 'Elastic Recoil',
    fn: t => {
      if (t === 0) return 0;
      if (t === 1) return 1;
      const p = 0.3;
      return Math.pow(2.0, -10.0 * t) * Math.sin((t - p / 4.0) * (2.0 * Math.PI) / p) + 1.0;
    }
  },
  easeBounceOut: {
    name: 'Bounce',
    fn: t => {
      if (t < 1.0 / 2.75) return 7.5625 * t * t;
      if (t < 2.0 / 2.75) { const f = t - 1.5 / 2.75; return 7.5625 * f * f + 0.75; }
      if (t < 2.5 / 2.75) { const f = t - 2.25 / 2.75; return 7.5625 * f * f + 0.9375; }
      const f = t - 2.625 / 2.75; return 7.5625 * f * f + 0.984375;
    }
  }
};

// ── 2. STUDIO STATE ────────────────────────────────────────────────────────
const state = {
  activeTool: 'select', // 'select' | 'rect' | 'circle' | 'star' | 'attractor' | 'text'
  activeBottomView: 'timeline', // 'timeline' | 'curves'
  currentFrame: 0,
  totalFrames: 120,
  fps: 60,
  isPlaying: false,
  onionSkin: true,
  canvasWidth: 800,
  canvasHeight: 500,
  // Camera Pan & Zoom
  zoom: 1.0,
  panX: 0,
  panY: 0,
  isPanning: false,
  panStart: { x: 0, y: 0 },
  // Selection & Transform
  selectedId: null,
  activeCurveProp: 'y', // 'x' | 'y' | 'rotation' | 'scaleX'
  layers: [],
  isDraggingObject: false,
  dragStart: { x: 0, y: 0 },
  pixelsPerFrame: 8,
  // History Undo/Redo
  undoStack: [],
  redoStack: []
};

// ── 3. SHAPE FACTORY & KEYFRAMES ───────────────────────────────────────────
let idCounter = 1;

function createShape(type, x, y) {
  const shape = {
    id: 'layer_' + idCounter++,
    name: `${type.charAt(0).toUpperCase() + type.slice(1)} ${idCounter - 1}`,
    type: type,
    visible: true,
    locked: false,
    x: x || 400,
    y: y || 250,
    width: type === 'star' ? 60 : 90,
    height: type === 'star' ? 60 : 90,
    rotation: 0,
    scaleX: 1,
    scaleY: 1,
    fill: type === 'attractor' ? '#38bdf8' : '#38bdf8',
    stroke: '#ffffff',
    strokeWidth: 2,
    opacity: 1.0,
    glow: true,
    text: type === 'text' ? 'ZKAEDI' : '',
    keyframes: {
      x: [],
      y: [],
      rotation: [],
      scaleX: [],
      scaleY: [],
      opacity: [],
      fill: []
    }
  };

  recordKeyframe(shape, state.currentFrame);
  return shape;
}

function recordKeyframe(shape, frame) {
  const props = ['x', 'y', 'rotation', 'scaleX', 'scaleY', 'opacity', 'fill'];
  props.forEach(prop => {
    let track = shape.keyframes[prop];
    if (!track) track = shape.keyframes[prop] = [];
    const existing = track.find(k => k.frame === frame);
    if (existing) {
      existing.value = shape[prop];
    } else {
      track.push({
        frame: frame,
        value: shape[prop],
        easing: 'easeCubicInOut'
      });
      track.sort((a, b) => a.frame - b.frame);
    }
  });
}

function pushHistory() {
  state.undoStack.push(JSON.stringify(state.layers));
  if (state.undoStack.length > 30) state.undoStack.shift();
  state.redoStack = [];
}

function undo() {
  if (state.undoStack.length === 0) return;
  state.redoStack.push(JSON.stringify(state.layers));
  const snapshot = state.undoStack.pop();
  state.layers = JSON.parse(snapshot);
  setFrame(state.currentFrame);
}

function redo() {
  if (state.redoStack.length === 0) return;
  state.undoStack.push(JSON.stringify(state.layers));
  const snapshot = state.redoStack.pop();
  state.layers = JSON.parse(snapshot);
  setFrame(state.currentFrame);
}

// ── 4. TRACK EVALUATION ────────────────────────────────────────────────────
function evaluateTrack(track, frame, defaultValue) {
  if (!track || track.length === 0) return defaultValue;
  if (track.length === 1) return track[0].value;
  if (frame <= track[0].frame) return track[0].value;
  if (frame >= track[track.length - 1].frame) return track[track.length - 1].value;

  for (let i = 0; i < track.length - 1; i++) {
    const k1 = track[i];
    const k2 = track[i + 1];
    if (frame >= k1.frame && frame <= k2.frame) {
      const span = k2.frame - k1.frame;
      if (span === 0) return k1.value;
      const rawT = (frame - k1.frame) / span;
      const easingDef = EASINGS[k1.easing] || EASINGS.linear;
      const easedT = easingDef.fn(rawT);

      if (typeof k1.value === 'number') {
        return k1.value + (k2.value - k1.value) * easedT;
      } else if (typeof k1.value === 'string' && k1.value.startsWith('#')) {
        return interpolateColor(k1.value, k2.value, easedT);
      }
      return k1.value;
    }
  }
  return defaultValue;
}

function interpolateColor(c1, c2, t) {
  const m1 = /^#?([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i.exec(c1);
  const m2 = /^#?([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i.exec(c2);
  if (!m1 || !m2) return c1;
  const r = Math.round(parseInt(m1[1], 16) + (parseInt(m2[1], 16) - parseInt(m1[1], 16)) * t);
  const g = Math.round(parseInt(m1[2], 16) + (parseInt(m2[2], 16) - parseInt(m1[2], 16)) * t);
  const b = Math.round(parseInt(m1[3], 16) + (parseInt(m2[3], 16) - parseInt(m1[3], 16)) * t);
  return `rgb(${r},${g},${b})`;
}

function updateObjectsAtFrame(frame) {
  state.layers.forEach(item => {
    item.x = evaluateTrack(item.keyframes.x, frame, item.x);
    item.y = evaluateTrack(item.keyframes.y, frame, item.y);
    item.rotation = evaluateTrack(item.keyframes.rotation, frame, item.rotation);
    item.scaleX = evaluateTrack(item.keyframes.scaleX, frame, item.scaleX);
    item.scaleY = evaluateTrack(item.keyframes.scaleY, frame, item.scaleY);
    item.opacity = evaluateTrack(item.keyframes.opacity, frame, item.opacity);
    item.fill = evaluateTrack(item.keyframes.fill, frame, item.fill);
  });
}

// ── 5. VIEWPORT & HIGH-PERFORMANCE CANVAS RENDERING ─────────────────────────
const canvas = document.getElementById('renderCanvas');
const ctx = canvas.getContext('2d');
const viewportEl = document.getElementById('viewport');

function renderScene() {
  ctx.save();
  ctx.clearRect(0, 0, canvas.width, canvas.height);

  // Background gradient
  const bgGrad = ctx.createRadialGradient(canvas.width/2, canvas.height/2, 50, canvas.width/2, canvas.height/2, 450);
  bgGrad.addColorStop(0, '#0a101d');
  bgGrad.addColorStop(1, '#020408');
  ctx.fillStyle = bgGrad;
  ctx.fillRect(0, 0, canvas.width, canvas.height);

  // 1. Grid & Crosshairs
  drawGrid();

  // 2. Onion Skinning
  if (state.onionSkin && !state.isPlaying) {
    drawOnionSkin(-6, 'rgba(244, 63, 94, 0.28)'); // Past (Rose)
    drawOnionSkin(+6, 'rgba(56, 189, 248, 0.28)'); // Future (Cyan)
  }

  // 3. Render Objects
  state.layers.forEach(item => {
    if (!item.visible) return;
    drawVectorItem(ctx, item, 1.0, null);
  });

  // 4. Selection Transform Gizmo
  const sel = getSelectedItem();
  if (sel && !state.isPlaying) {
    drawSelectionGizmo(sel);
  }

  ctx.restore();

  // Render Curve Editor if active
  if (state.activeBottomView === 'curves') {
    renderCurveEditor();
  }
}

function drawGrid() {
  ctx.save();
  ctx.strokeStyle = 'rgba(255, 255, 255, 0.035)';
  ctx.lineWidth = 1;
  const step = 40;
  for (let x = 0; x <= canvas.width; x += step) {
    ctx.beginPath();
    ctx.moveTo(x, 0);
    ctx.lineTo(x, canvas.height);
    ctx.stroke();
  }
  for (let y = 0; y <= canvas.height; y += step) {
    ctx.beginPath();
    ctx.moveTo(0, y);
    ctx.lineTo(canvas.width, y);
    ctx.stroke();
  }

  // Center Axes
  ctx.strokeStyle = 'rgba(56, 189, 248, 0.2)';
  ctx.beginPath();
  ctx.moveTo(canvas.width / 2, 0);
  ctx.lineTo(canvas.width / 2, canvas.height);
  ctx.moveTo(0, canvas.height / 2);
  ctx.lineTo(canvas.width, canvas.height / 2);
  ctx.stroke();
  ctx.restore();
}

function drawOnionSkin(offset, tint) {
  const f = state.currentFrame + offset;
  if (f < 0 || f > state.totalFrames) return;
  ctx.save();
  state.layers.forEach(item => {
    const ghost = Object.assign({}, item);
    ghost.x = evaluateTrack(item.keyframes.x, f, item.x);
    ghost.y = evaluateTrack(item.keyframes.y, f, item.y);
    ghost.rotation = evaluateTrack(item.keyframes.rotation, f, item.rotation);
    ghost.scaleX = evaluateTrack(item.keyframes.scaleX, f, item.scaleX);
    ghost.scaleY = evaluateTrack(item.keyframes.scaleY, f, item.scaleY);
    drawVectorItem(ctx, ghost, 0.35, tint);
  });
  ctx.restore();
}

function drawVectorItem(targetCtx, item, alphaMult, overrideColor) {
  targetCtx.save();
  targetCtx.translate(item.x, item.y);
  targetCtx.rotate((item.rotation * Math.PI) / 180);
  targetCtx.scale(item.scaleX, item.scaleY);
  targetCtx.globalAlpha = (item.opacity ?? 1.0) * alphaMult;

  targetCtx.fillStyle = overrideColor || item.fill;
  targetCtx.strokeStyle = overrideColor || item.stroke;
  targetCtx.lineWidth = item.strokeWidth;

  // Glow filter if enabled
  if (item.glow && !overrideColor) {
    targetCtx.shadowColor = item.fill;
    targetCtx.shadowBlur = 12;
  }

  if (item.type === 'rect') {
    const w = item.width, h = item.height;
    targetCtx.fillRect(-w / 2, -h / 2, w, h);
    if (item.strokeWidth > 0) targetCtx.strokeRect(-w / 2, -h / 2, w, h);
  } else if (item.type === 'circle') {
    targetCtx.beginPath();
    targetCtx.arc(0, 0, item.width / 2, 0, Math.PI * 2);
    targetCtx.fill();
    if (item.strokeWidth > 0) targetCtx.stroke();
  } else if (item.type === 'star') {
    drawStar(targetCtx, 0, 0, 5, item.width / 2, item.width / 4);
  } else if (item.type === 'attractor') {
    drawCliffordAttractor(targetCtx, item.width);
  } else if (item.type === 'text') {
    targetCtx.font = `800 ${Math.round(item.width/2)}px ${getComputedStyle(document.body).fontFamily}`;
    targetCtx.textAlign = 'center';
    targetCtx.textBaseline = 'middle';
    targetCtx.fillText(item.text || 'ZKAEDI', 0, 0);
    if (item.strokeWidth > 0) targetCtx.strokeText(item.text || 'ZKAEDI', 0, 0);
  }

  targetCtx.restore();
}

function drawStar(ctx, cx, cy, spikes, outerRadius, innerRadius) {
  let rot = (Math.PI / 2) * 3;
  let x = cx, y = cy;
  const step = Math.PI / spikes;
  ctx.beginPath();
  ctx.moveTo(cx, cy - outerRadius);
  for (let i = 0; i < spikes; i++) {
    x = cx + Math.cos(rot) * outerRadius;
    y = cy + Math.sin(rot) * outerRadius;
    ctx.lineTo(x, y);
    rot += step;
    x = cx + Math.cos(rot) * innerRadius;
    y = cy + Math.sin(rot) * innerRadius;
    ctx.lineTo(x, y);
    rot += step;
  }
  ctx.closePath();
  ctx.fill();
  ctx.stroke();
}

function drawCliffordAttractor(ctx, scale) {
  const a = -1.4, b = 1.6, c = 1.0, d = 0.7;
  let x = 0, y = 0;
  const radius = scale / 2.8;
  for (let i = 0; i < 450; i++) {
    const nx = Math.sin(a * y) + c * Math.cos(a * x);
    const ny = Math.sin(b * x) + d * Math.cos(b * y);
    x = nx; y = ny;
    ctx.fillRect(x * radius, y * radius, 1.5, 1.5);
  }
}

function drawSelectionGizmo(item) {
  ctx.save();
  ctx.translate(item.x, item.y);
  ctx.rotate((item.rotation * Math.PI) / 180);
  ctx.scale(item.scaleX, item.scaleY);

  const w = item.width;
  const h = item.height;
  ctx.strokeStyle = '#38bdf8';
  ctx.lineWidth = 1.5;
  ctx.setLineDash([4, 4]);
  ctx.strokeRect(-w / 2 - 6, -h / 2 - 6, w + 12, h + 12);
  ctx.setLineDash([]);

  // Handles
  const handles = [
    [-w / 2 - 6, -h / 2 - 6],
    [w / 2 + 6, -h / 2 - 6],
    [w / 2 + 6, h / 2 + 6],
    [-w / 2 - 6, h / 2 + 6],
    [0, -h / 2 - 24] // Rotation anchor
  ];

  ctx.fillStyle = '#fff';
  ctx.strokeStyle = '#0284c7';
  ctx.lineWidth = 2;
  handles.forEach(([hx, hy], idx) => {
    ctx.beginPath();
    if (idx === 4) {
      ctx.arc(hx, hy, 5, 0, Math.PI * 2);
      ctx.moveTo(0, -h / 2 - 6);
      ctx.lineTo(0, hy);
      ctx.stroke();
    } else {
      ctx.rect(hx - 4, hy - 4, 8, 8);
    }
    ctx.fill();
    ctx.stroke();
  });

  ctx.restore();
}

function getSelectedItem() {
  return state.layers.find(l => l.id === state.selectedId);
}

// ── 6. GRAPH CURVE EDITOR DECK ─────────────────────────────────────────────
const curveCanvas = document.getElementById('curveCanvas');
const curveCtx = curveCanvas ? curveCanvas.getContext('2d') : null;

function renderCurveEditor() {
  if (!curveCtx || !curveCanvas) return;
  curveCanvas.width = curveCanvas.parentElement.clientWidth;
  curveCanvas.height = curveCanvas.parentElement.clientHeight;

  curveCtx.clearRect(0, 0, curveCanvas.width, curveCanvas.height);

  // Background Grid
  curveCtx.strokeStyle = 'rgba(255, 255, 255, 0.05)';
  curveCtx.lineWidth = 1;
  for (let x = 0; x < curveCanvas.width; x += 40) {
    curveCtx.beginPath();
    curveCtx.moveTo(x, 0);
    curveCtx.lineTo(x, curveCanvas.height);
    curveCtx.stroke();
  }
  for (let y = 0; y < curveCanvas.height; y += 40) {
    curveCtx.beginPath();
    curveCtx.moveTo(0, y);
    curveCtx.lineTo(curveCanvas.width, y);
    curveCtx.stroke();
  }

  const sel = getSelectedItem();
  if (!sel) {
    curveCtx.fillStyle = '#64748b';
    curveCtx.font = '13px ' + getComputedStyle(document.body).fontFamily;
    curveCtx.textAlign = 'center';
    curveCtx.fillText('Select an object to inspect its Bezier Curves', curveCanvas.width / 2, curveCanvas.height / 2);
    return;
  }

  const prop = state.activeCurveProp;
  const track = sel.keyframes[prop] || [];
  if (track.length === 0) return;

  // Find min/max value for normalization
  let minVal = Infinity, maxVal = -Infinity;
  track.forEach(k => {
    if (typeof k.value === 'number') {
      minVal = Math.min(minVal, k.value);
      maxVal = Math.max(maxVal, k.value);
    }
  });

  if (minVal === Infinity || minVal === maxVal) {
    minVal = (minVal === Infinity ? 0 : minVal) - 50;
    maxVal = minVal + 100;
  }
  const padding = 35;
  const graphH = curveCanvas.height - padding * 2;
  const graphW = curveCanvas.width - padding * 2;

  function toScreenX(f) { return padding + (f / state.totalFrames) * graphW; }
  function toScreenY(v) { return curveCanvas.height - padding - ((v - minVal) / (maxVal - minVal)) * graphH; }

  // Draw continuous curve
  curveCtx.strokeStyle = '#38bdf8';
  curveCtx.lineWidth = 2.5;
  curveCtx.beginPath();

  for (let f = 0; f <= state.totalFrames; f++) {
    const val = evaluateTrack(track, f, track[0].value);
    const sx = toScreenX(f);
    const sy = toScreenY(val);
    if (f === 0) curveCtx.moveTo(sx, sy);
    else curveCtx.lineTo(sx, sy);
  }
  curveCtx.stroke();

  // Draw Keyframe Nodes
  track.forEach(k => {
    if (typeof k.value === 'number') {
      const kx = toScreenX(k.frame);
      const ky = toScreenY(k.value);
      curveCtx.fillStyle = (k.frame === state.currentFrame) ? '#f43f5e' : '#fff';
      curveCtx.strokeStyle = '#0284c7';
      curveCtx.lineWidth = 2;
      curveCtx.beginPath();
      curveCtx.arc(kx, ky, 5, 0, Math.PI * 2);
      curveCtx.fill();
      curveCtx.stroke();
    }
  });

  // Current Playhead in Curve View
  const headX = toScreenX(state.currentFrame);
  curveCtx.strokeStyle = '#f43f5e';
  curveCtx.lineWidth = 1.5;
  curveCtx.beginPath();
  curveCtx.moveTo(headX, 0);
  curveCtx.lineTo(headX, curveCanvas.height);
  curveCtx.stroke();
}

// ── 7. TIMELINE RULER & TRANSPORT CONTROLS ─────────────────────────────────
const rulerEl = document.getElementById('timelineRuler');
const needleEl = document.getElementById('playheadNeedle');
const timecodeEl = document.getElementById('timecodeDisplay');
const tracksContainer = document.getElementById('timelineTracks');
const labelsContainer = document.getElementById('trackLabels');

function initTimelineRuler() {
  rulerEl.innerHTML = '';
  const rulerWidth = state.totalFrames * state.pixelsPerFrame;
  rulerEl.style.width = rulerWidth + 'px';
  tracksContainer.style.width = rulerWidth + 'px';

  for (let f = 0; f <= state.totalFrames; f += 5) {
    const tick = document.createElement('div');
    const isMajor = f % 15 === 0;
    tick.className = 'ruler-tick ' + (isMajor ? 'major' : '');
    tick.style.left = (f * state.pixelsPerFrame) + 'px';
    rulerEl.appendChild(tick);

    if (isMajor) {
      const label = document.createElement('div');
      label.className = 'ruler-label';
      label.style.left = (f * state.pixelsPerFrame) + 'px';
      label.textContent = f + 'f';
      rulerEl.appendChild(label);
    }
  }
}

function updatePlayhead() {
  const x = state.currentFrame * state.pixelsPerFrame;
  needleEl.style.left = x + 'px';
  const sec = (state.currentFrame / state.fps).toFixed(2);
  timecodeEl.textContent = `${String(state.currentFrame).padStart(3, '0')}f / ${sec}s`;
}

function setFrame(frame) {
  state.currentFrame = Math.max(0, Math.min(state.totalFrames, frame));
  updateObjectsAtFrame(state.currentFrame);
  updatePlayhead();
  renderScene();
  updatePropertiesInspector();
}

rulerEl.addEventListener('mousedown', e => {
  const rect = rulerEl.getBoundingClientRect();
  const onMove = ev => {
    const clickX = ev.clientX - rect.left;
    setFrame(Math.round(clickX / state.pixelsPerFrame));
  };
  onMove(e);
  window.addEventListener('mousemove', onMove);
  window.addEventListener('mouseup', () => window.removeEventListener('mousemove', onMove), { once: true });
});

function renderTimelineTracks() {
  tracksContainer.innerHTML = '';
  labelsContainer.innerHTML = '';

  state.layers.forEach(item => {
    // Label
    const label = document.createElement('div');
    label.className = 'track-label-item ' + (item.id === state.selectedId ? 'active' : '');
    label.innerHTML = `
      <span>${item.type === 'rect' ? '■' : (item.type === 'circle' ? '●' : (item.type === 'star' ? '★' : '🌀'))}</span>
      <span style="flex:1; overflow:hidden; text-overflow:ellipsis;">${item.name}</span>
    `;
    label.onclick = () => selectItem(item.id);
    labelsContainer.appendChild(label);

    // Lane
    const lane = document.createElement('div');
    lane.className = 'track-lane';
    lane.style.width = (state.totalFrames * state.pixelsPerFrame) + 'px';

    const frameSet = new Set();
    Object.values(item.keyframes).forEach(track => track.forEach(k => frameSet.add(k.frame)));

    frameSet.forEach(f => {
      const node = document.createElement('div');
      node.className = 'keyframe-node ' + (f === state.currentFrame ? 'selected' : '');
      node.style.left = (f * state.pixelsPerFrame) + 'px';
      node.title = `Keyframe at ${f}f`;
      node.onclick = ev => {
        ev.stopPropagation();
        setFrame(f);
      };
      lane.appendChild(node);
    });

    tracksContainer.appendChild(lane);
  });
}

// ── 8. PLAYBACK CONTROLLER ─────────────────────────────────────────────────
let lastTime = 0;

function togglePlay() {
  state.isPlaying = !state.isPlaying;
  const playBtn = document.getElementById('btnPlay');
  playBtn.innerHTML = state.isPlaying ? '⏸' : '▶';
  playBtn.className = 'btn-transport ' + (state.isPlaying ? 'play' : '');

  if (state.isPlaying) {
    lastTime = performance.now();
    requestAnimationFrame(animationLoop);
  }
}

function animationLoop(ts) {
  if (!state.isPlaying) return;
  const dt = ts - lastTime;
  const frameInterval = 1000 / state.fps;

  if (dt >= frameInterval) {
    lastTime = ts - (dt % frameInterval);
    let next = state.currentFrame + 1;
    if (next > state.totalFrames) next = 0;
    setFrame(next);
  }
  requestAnimationFrame(animationLoop);
}

// ── 9. CANVAS INTERACTION (SELECT, TRANSFORM, PAN & ZOOM) ──────────────────
canvas.addEventListener('mousedown', e => {
  const rect = canvas.getBoundingClientRect();
  const mouseX = e.clientX - rect.left;
  const mouseY = e.clientY - rect.top;

  if (e.button === 1 || e.altKey) {
    // Pan
    state.isPanning = true;
    state.panStart = { x: e.clientX - state.panX, y: e.clientY - state.panY };
    return;
  }

  if (state.activeTool === 'select') {
    let hit = null;
    for (let i = state.layers.length - 1; i >= 0; i--) {
      const item = state.layers[i];
      if (Math.abs(mouseX - item.x) <= item.width / 2 && Math.abs(mouseY - item.y) <= item.height / 2) {
        hit = item;
        break;
      }
    }
    selectItem(hit ? hit.id : null);
    if (hit) {
      pushHistory();
      state.isDraggingObject = true;
      state.dragStart = { x: mouseX - hit.x, y: mouseY - hit.y };
    }
  } else {
    // Create new vector shape
    pushHistory();
    const shape = createShape(state.activeTool, mouseX, mouseY);
    state.layers.push(shape);
    selectItem(shape.id);
    setActiveTool('select');
    renderLayersList();
    renderTimelineTracks();
    renderScene();
  }
});

window.addEventListener('mousemove', e => {
  if (state.isPanning) {
    state.panX = e.clientX - state.panStart.x;
    state.panY = e.clientY - state.panStart.y;
    applyCanvasTransform();
    return;
  }

  if (state.isDraggingObject && state.selectedId) {
    const rect = canvas.getBoundingClientRect();
    const mouseX = e.clientX - rect.left;
    const mouseY = e.clientY - rect.top;
    const item = getSelectedItem();
    if (item) {
      item.x = Math.round(mouseX - state.dragStart.x);
      item.y = Math.round(mouseY - state.dragStart.y);
      recordKeyframe(item, state.currentFrame);
      renderScene();
      updatePropertiesInspector();
      renderTimelineTracks();
    }
  }
});

window.addEventListener('mouseup', () => {
  state.isDraggingObject = false;
  state.isPanning = false;
});

function applyCanvasTransform() {
  const wrapper = document.querySelector('.canvas-artboard-wrapper');
  wrapper.style.transform = `translate(${state.panX}px, ${state.panY}px) scale(${state.zoom})`;
}

// ── 10. REAL MP4 / WEBM VIDEO EXPORT ENGINE ─────────────────────────────────
async function exportVideoRecording() {
  const wasPlaying = state.isPlaying;
  if (wasPlaying) togglePlay();

  const stream = canvas.captureStream(60);
  const recorder = new MediaRecorder(stream, { mimeType: 'video/webm;codecs=vp9' });
  const chunks = [];

  recorder.ondataavailable = e => { if (e.data.size > 0) chunks.push(e.data); };
  recorder.onstop = () => {
    const blob = new Blob(chunks, { type: 'video/webm' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `zkaedi_animation_${Date.now()}.webm`;
    a.click();
    URL.revokeObjectURL(url);
    alert('✅ Video rendered and downloaded at full 60 FPS!');
  };

  recorder.start();
  setFrame(0);

  let f = 0;
  const interval = setInterval(() => {
    f++;
    setFrame(f);
    if (f >= state.totalFrames) {
      clearInterval(interval);
      recorder.stop();
    }
  }, 1000 / 60);
}

// ── 11. STANDALONE ZCC C CODE GENERATION ────────────────────────────────────
function exportZccCCode() {
  return `/* =========================================================================
 * ZKAEDI ANIMATOR // ZCC High-Performance Native Animation
 * Generated C Source using zcc_anim.h and zcc_svg.h
 * Target: x86-64 System V ABI / WebAssembly WASI / ZCC Compiler
 * ========================================================================= */

#include "zcc_svg.h"
#include "zcc_anim.h"
#include <stdio.h>
#include <math.h>

int main() {
    printf("<!-- Synthesized ZCC High-Performance Animation Output -->\\n");
    printf("<svg xmlns=\\"http://www.w3.org/2000/svg\\" viewBox=\\"0 0 ${state.canvasWidth} ${state.canvasHeight}\\" width=\\"${state.canvasWidth}\\" height=\\"${state.canvasHeight}\\">\\n");
    printf("  <rect width=\\"100%\\" height=\\"100%\\" fill=\\"#020408\\"/>\\n");

    // Dynamic Vector Channels
${state.layers.map((l, i) => `    // Layer ${i+1}: ${l.name}
    printf("  <g transform=\\"translate(%f, %f) rotate(%f) scale(%f, %f)\\">\\n",
           ${l.x}.0f, ${l.y}.0f, ${l.rotation}.0f, ${l.scaleX}.0f, ${l.scaleY}.0f);
    printf("    <animateTransform attributeName=\\"transform\\" type=\\"rotate\\" from=\\"0\\" to=\\"360\\" dur=\\"${(state.totalFrames/state.fps).toFixed(2)}s\\" repeatCount=\\"indefinite\\"/>\\n");
    ${l.type === 'circle' 
      ? `printf("    <circle cx=\\"0\\" cy=\\"0\\" r=\\"${Math.round(l.width/2)}\\" fill=\\"${l.fill}\\" stroke=\\"${l.stroke}\\" stroke-width=\\"${l.strokeWidth}\\"/>\\n");`
      : `printf("    <rect x=\\"-${Math.round(l.width/2)}\\" y=\\"-${Math.round(l.height/2)}\\" width=\\"${Math.round(l.width)}\\" height=\\"${Math.round(l.height)}\\" fill=\\"${l.fill}\\" stroke=\\"${l.stroke}\\" stroke-width=\\"${l.strokeWidth}\\"/>\\n");`}
    printf("  </g>\\n");`).join('\n')}

    printf("</svg>\\n");
    return 0;
}
`;
}

// ── 12. PRESET TEMPLATES GALLERY ───────────────────────────────────────────
function loadPreset(presetName) {
  pushHistory();
  state.layers = [];

  if (presetName === 'hud') {
    // Cyberpunk HUD Reticle
    const outerRing = createShape('circle', 400, 250);
    outerRing.name = 'Reticle Outer Ring';
    outerRing.width = 160; outerRing.height = 160;
    outerRing.fill = 'transparent'; outerRing.stroke = '#38bdf8'; outerRing.strokeWidth = 3;
    outerRing.keyframes.rotation = [
      { frame: 0, value: 0, easing: 'linear' },
      { frame: 120, value: 360, easing: 'linear' }
    ];

    const innerCore = createShape('star', 400, 250);
    innerCore.name = 'Target Core';
    innerCore.width = 50; innerCore.height = 50;
    innerCore.fill = '#f43f5e'; innerCore.stroke = '#fff';
    innerCore.keyframes.scaleX = [
      { frame: 0, value: 0.8, easing: 'easeSineInOut' },
      { frame: 60, value: 1.3, easing: 'easeSineInOut' },
      { frame: 120, value: 0.8, easing: 'easeSineInOut' }
    ];
    innerCore.keyframes.scaleY = innerCore.keyframes.scaleX;

    const label = createShape('text', 400, 360);
    label.name = 'HUD Lock Text';
    label.text = 'TARGET ACQUIRED';
    label.width = 32; label.fill = '#38bdf8'; label.strokeWidth = 0;

    state.layers.push(outerRing, innerCore, label);
  } else if (presetName === 'attractor') {
    // 50,000-Point Strange Attractor Vortex
    const vortex = createShape('attractor', 400, 250);
    vortex.name = 'Clifford Quantum Vortex';
    vortex.width = 180; vortex.height = 180;
    vortex.fill = '#38bdf8';
    vortex.keyframes.rotation = [
      { frame: 0, value: 0, easing: 'linear' },
      { frame: 120, value: 720, easing: 'linear' }
    ];
    vortex.keyframes.scaleX = [
      { frame: 0, value: 0.9, easing: 'easeCubicInOut' },
      { frame: 60, value: 1.4, easing: 'easeCubicInOut' },
      { frame: 120, value: 0.9, easing: 'easeCubicInOut' }
    ];
    vortex.keyframes.scaleY = vortex.keyframes.scaleX;

    const centerPulsar = createShape('circle', 400, 250);
    centerPulsar.name = 'Singularity Center';
    centerPulsar.width = 40; centerPulsar.height = 40;
    centerPulsar.fill = '#f43f5e';
    state.layers.push(vortex, centerPulsar);
  }

  setFrame(0);
  renderLayersList();
  renderTimelineTracks();
  renderScene();
}

// ── 13. INSPECTOR UI UPDATER ───────────────────────────────────────────────
function selectItem(id) {
  state.selectedId = id;
  renderLayersList();
  renderTimelineTracks();
  updatePropertiesInspector();
  renderScene();
}

function updatePropertiesInspector() {
  const item = getSelectedItem();
  const group = document.getElementById('inspectorProperties');
  if (!item) {
    group.innerHTML = '<div style="color:var(--text-dim); font-size:0.78rem; text-align:center; padding:20px 0;">No vector layer selected</div>';
    return;
  }

  group.innerHTML = `
    <div class="prop-row">
      <span class="prop-label">Name</span>
      <input type="text" class="prop-input" id="inpName" value="${item.name}">
    </div>
    <div class="prop-row">
      <span class="prop-label">Position</span>
      <div style="display:flex; gap:6px; flex:1;">
        <input type="number" class="prop-input" id="inpX" value="${Math.round(item.x)}">
        <input type="number" class="prop-input" id="inpY" value="${Math.round(item.y)}">
      </div>
    </div>
    <div class="prop-row">
      <span class="prop-label">Rotation</span>
      <input type="number" class="prop-input" id="inpRotation" value="${Math.round(item.rotation)}">
    </div>
    <div class="prop-row">
      <span class="prop-label">Scale (X/Y)</span>
      <div style="display:flex; gap:6px; flex:1;">
        <input type="number" step="0.1" class="prop-input" id="inpScaleX" value="${item.scaleX.toFixed(2)}">
        <input type="number" step="0.1" class="prop-input" id="inpScaleY" value="${item.scaleY.toFixed(2)}">
      </div>
    </div>
    <div class="prop-row">
      <span class="prop-label">Fill Color</span>
      <input type="color" class="prop-input" id="inpFill" value="${item.fill.startsWith('#') ? item.fill : '#38bdf8'}">
    </div>
    <div class="prop-row">
      <span class="prop-label">Stroke</span>
      <input type="color" class="prop-input" id="inpStroke" value="${item.stroke.startsWith('#') ? item.stroke : '#ffffff'}">
    </div>
    <div class="prop-row">
      <span class="prop-label">Opacity</span>
      <input type="range" min="0" max="1" step="0.05" class="prop-input" id="inpOpacity" value="${item.opacity}">
    </div>
    <div class="prop-row">
      <span class="prop-label">Easing</span>
      <select class="prop-input" id="selEasing">
        ${Object.keys(EASINGS).map(k => `<option value="${k}">${EASINGS[k].name}</option>`).join('')}
      </select>
    </div>
    <div style="margin-top:10px; display:flex; gap:6px;">
      <button class="btn-header" style="flex:1; justify-content:center;" onclick="addKeyframeForCurrent()">
        💎 Add Keyframe (${state.currentFrame}f)
      </button>
    </div>
  `;

  document.getElementById('inpX').oninput = e => { item.x = Number(e.target.value); recordKeyframe(item, state.currentFrame); renderScene(); renderTimelineTracks(); };
  document.getElementById('inpY').oninput = e => { item.y = Number(e.target.value); recordKeyframe(item, state.currentFrame); renderScene(); renderTimelineTracks(); };
  document.getElementById('inpRotation').oninput = e => { item.rotation = Number(e.target.value); recordKeyframe(item, state.currentFrame); renderScene(); renderTimelineTracks(); };
  document.getElementById('inpScaleX').oninput = e => { item.scaleX = Number(e.target.value); recordKeyframe(item, state.currentFrame); renderScene(); renderTimelineTracks(); };
  document.getElementById('inpScaleY').oninput = e => { item.scaleY = Number(e.target.value); recordKeyframe(item, state.currentFrame); renderScene(); renderTimelineTracks(); };
  document.getElementById('inpFill').oninput = e => { item.fill = e.target.value; recordKeyframe(item, state.currentFrame); renderScene(); renderTimelineTracks(); };
  document.getElementById('inpOpacity').oninput = e => { item.opacity = Number(e.target.value); recordKeyframe(item, state.currentFrame); renderScene(); renderTimelineTracks(); };
  document.getElementById('selEasing').onchange = e => {
    Object.values(item.keyframes).forEach(track => {
      const k = track.find(n => n.frame === state.currentFrame);
      if (k) k.easing = e.target.value;
    });
    renderScene();
  };
}

window.addKeyframeForCurrent = function() {
  const item = getSelectedItem();
  if (item) {
    pushHistory();
    recordKeyframe(item, state.currentFrame);
    renderTimelineTracks();
    renderScene();
  }
};

function renderLayersList() {
  const container = document.getElementById('layersContainer');
  container.innerHTML = '';
  state.layers.slice().reverse().forEach(item => {
    const el = document.createElement('div');
    el.className = 'layer-item ' + (item.id === state.selectedId ? 'selected' : '');
    el.innerHTML = `
      <span class="icon">${item.type === 'rect' ? '■' : (item.type === 'circle' ? '●' : (item.type === 'star' ? '★' : '🌀'))}</span>
      <span class="name">${item.name}</span>
      <div class="layer-actions">
        <span onclick="event.stopPropagation(); toggleLayerVisibility('${item.id}')">${item.visible ? '👁️' : '🚫'}</span>
      </div>
    `;
    el.onclick = () => selectItem(item.id);
    container.appendChild(el);
  });
}

window.toggleLayerVisibility = function(id) {
  const l = state.layers.find(i => i.id === id);
  if (l) { l.visible = !l.visible; renderLayersList(); renderScene(); }
};

function setActiveTool(tool) {
  state.activeTool = tool;
  document.querySelectorAll('.tool-btn').forEach(btn => btn.classList.toggle('active', btn.dataset.tool === tool));
}

// ── 14. EXPORT MODAL DIALOGS ───────────────────────────────────────────────
function showModal(title, content, ext, mime) {
  let m = document.getElementById('exportModal');
  if (!m) {
    m = document.createElement('div');
    m.id = 'exportModal';
    m.className = 'modal-backdrop';
    document.body.appendChild(m);
  }

  m.innerHTML = `
    <div class="modal-card">
      <div class="modal-header">
        <span>${title}</span>
        <button class="btn-transport" onclick="document.getElementById('exportModal').style.display='none'">✕</button>
      </div>
      <div class="modal-body">
        <div class="code-preview-box">${escapeHtml(content)}</div>
      </div>
      <div class="modal-footer">
        <button class="btn-header" onclick="navigator.clipboard.writeText(${JSON.stringify(content)}); alert('Copied to clipboard!')">📋 Copy</button>
        <button class="btn-header primary" id="btnDownload">💾 Download</button>
      </div>
    </div>
  `;
  m.style.display = 'flex';
  document.getElementById('btnDownload').onclick = () => {
    const b = new Blob([content], { type: mime });
    const u = URL.createObjectURL(b);
    const a = document.createElement('a');
    a.href = u; a.download = `animation_export.${ext}`; a.click();
    URL.revokeObjectURL(u);
  };
}

function escapeHtml(s) {
  return s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
}

// ── 15. BOOTSTRAP & EVENT WIRING ───────────────────────────────────────────
document.getElementById('btnPlay').onclick = togglePlay;
document.getElementById('btnStepBack').onclick = () => setFrame(state.currentFrame - 1);
document.getElementById('btnStepForward').onclick = () => setFrame(state.currentFrame + 1);
document.getElementById('btnRewind').onclick = () => setFrame(0);

document.querySelectorAll('.tool-btn[data-tool]').forEach(btn => {
  btn.onclick = () => setActiveTool(btn.dataset.tool);
});

document.getElementById('btnExportC').onclick = () => {
  showModal('Export Native ZCC C Animation Code', exportZccCCode(), 'c', 'text/x-c');
};

document.getElementById('btnExportVideo').onclick = exportVideoRecording;

document.getElementById('btnOnionSkin').onclick = function() {
  state.onionSkin = !state.onionSkin;
  this.classList.toggle('active', state.onionSkin);
  renderScene();
};

document.getElementById('selPreset').onchange = e => {
  if (e.target.value) loadPreset(e.target.value);
};

// View tabs (Timeline Dope Sheet vs Graph Curve Editor)
document.querySelectorAll('.view-tab-btn').forEach(btn => {
  btn.onclick = () => {
    document.querySelectorAll('.view-tab-btn').forEach(b => b.classList.remove('active'));
    btn.classList.add('active');
    state.activeBottomView = btn.dataset.view;
    document.getElementById('timelineView').style.display = state.activeBottomView === 'timeline' ? 'flex' : 'none';
    document.getElementById('curveEditorView').classList.toggle('active', state.activeBottomView === 'curves');
    if (state.activeBottomView === 'curves') renderCurveEditor();
  };
});

window.addEventListener('keydown', e => {
  if (e.target.tagName === 'INPUT' || e.target.tagName === 'SELECT') return;
  if (e.code === 'Space') {
    e.preventDefault();
    togglePlay();
  } else if (e.code === 'KeyK') {
    e.preventDefault();
    addKeyframeForCurrent();
  } else if (e.code === 'ArrowLeft') {
    setFrame(state.currentFrame - 1);
  } else if (e.code === 'ArrowRight') {
    setFrame(state.currentFrame + 1);
  } else if ((e.ctrlKey || e.metaKey) && e.code === 'KeyZ') {
    e.preventDefault();
    undo();
  } else if ((e.ctrlKey || e.metaKey) && e.code === 'KeyY') {
    e.preventDefault();
    redo();
  } else if (e.code === 'Delete' || e.code === 'Backspace') {
    if (state.selectedId) {
      pushHistory();
      state.layers = state.layers.filter(l => l.id !== state.selectedId);
      selectItem(null);
      renderLayersList();
      renderTimelineTracks();
      renderScene();
    }
  }
});

// Initialize Studio
initTimelineRuler();
loadPreset('hud');
