/* ZCC Cosmic Elephant - Exact QA Telemetry Module
 * Drop-in usage:
 *   1) Include this file after your runtime script.
 *   2) Call window.QATelemetry.attachRuntimeAdapters({...}) once your engine is ready.
 *   3) Optional: runtime can push events via window.QATelemetry.pushEvent(...)
 *   4) CI harness calls:
 *        window.setQAMode(config)
 *        window.startQATelemetry()
 *        window.getQAMetrics()
 */
(() => {
  "use strict";

  const clamp = (v, lo, hi) => Math.max(lo, Math.min(hi, v));
  const isFiniteNum = (v) => Number.isFinite(v) && !Number.isNaN(v);

  function percentile(arr, p) {
    if (!arr.length) return 0;
    const a = [...arr].sort((x, y) => x - y);
    const idx = clamp((p / 100) * (a.length - 1), 0, a.length - 1);
    const lo = Math.floor(idx), hi = Math.ceil(idx);
    if (lo === hi) return a[lo];
    const t = idx - lo;
    return a[lo] * (1 - t) + a[hi] * t;
  }

  function stddev(arr) {
    if (!arr.length) return 0;
    const mean = arr.reduce((s, v) => s + v, 0) / arr.length;
    const varr = arr.reduce((s, v) => s + (v - mean) * (v - mean), 0) / arr.length;
    return Math.sqrt(varr);
  }

  function luminanceFromRGB(r, g, b) {
    const R = r / 255, G = g / 255, B = b / 255;
    return 0.2126 * R + 0.7152 * G + 0.0722 * B;
  }

  class QATelemetry {
    constructor() {
      this.resetAll();
    }

    resetAll() {
      this.cfg = {
        experimentId: "unknown",
        seed: 1337,
        inputsJson: {},
        actionsJson: [],
        beatDebounceMs: 110,
        jerkTransitionWindowMs: 180,
        photosensitiveFlashLumDelta: 0.1
      };

      this.running = false;
      this.startedAt = 0;
      this.endedAt = 0;
      this.lastTs = 0;

      this.adapters = {
        getPoseState: null,
        getActiveModifiers: null,
        getBeatSignal: null,
        getAudioFeatures: null,
        getEmissiveSamples: null,
        getMemoryMB: null,
        getDeterminismHash: null,
        getRuntimeFlags: null
      };

      this.frameTimesMs = [];
      this.fpsSamples = [];
      this.memorySamplesMB = [];

      this.nanCount = 0;
      this.infCount = 0;
      this.constraintViolations = 0;
      this.popEvents = 0;

      this.totalBeats = 0;
      this.falseBeats = 0;
      this.missedBeats = 0;
      this.lastBeatTs = -Infinity;
      this.minBeatIntervalMs = Infinity;

      this.syncErrors = [];
      this.recalibrationSamples = [];

      this.arbitrationChecks = 0;
      this.arbitrationCorrect = 0;
      this.nondeterministicSortCount = 0;
      this.lastSortSignature = null;

      this.flashFrequencyEdges = 0;
      this.lastFlashEdgeTs = 0;
      this.lastLum = null;
      this.luminanceDeltaMax = 0;

      this.reentryJerkSamples = [];
      this.reactiveArtifactCount = 0;
      this.wasSilent = false;
      this.silenceThreshold = 0.012;

      this.poseHashMismatchCount = 0;
      this.detHashFirst = null;
      this.detHashMatches = 0;
      this.detHashTotal = 0;

      this.criticalErrors = 0;
      this.degradationLockEvents = 0;
      this._seenDegradeLock = false;

      this.rafId = null;
      this.events = [];
    }

    attachRuntimeAdapters(adapters) {
      this.adapters = { ...this.adapters, ...adapters };
    }

    setQAMode(config) {
      this.cfg = { ...this.cfg, ...(config || {}) };
      if (typeof this.cfg.inputsJson === "string") {
        try { this.cfg.inputsJson = JSON.parse(this.cfg.inputsJson); } catch {}
      }
      if (typeof this.cfg.actionsJson === "string") {
        try { this.cfg.actionsJson = JSON.parse(this.cfg.actionsJson); } catch {}
      }
    }

    start() {
      this.running = true;
      this.startedAt = performance.now();
      this.lastTs = this.startedAt;
      this._tick = this._tick.bind(this);
      this.rafId = requestAnimationFrame(this._tick);
    }

    stop() {
      this.running = false;
      if (this.rafId) cancelAnimationFrame(this.rafId);
      this.endedAt = performance.now();
    }

    pushEvent(evt) {
      this.events.push({ ts: performance.now(), ...evt });
    }

    _processQueuedEvents(now) {
      if (!this.events.length) return;
      for (const e of this.events) {
        if (e.type === "constraintViolation") this.constraintViolations++;
        if (e.type === "pop") this.popEvents++;
        if (e.type === "beatTrue") this.totalBeats++;
        if (e.type === "beatFalse") { this.totalBeats++; this.falseBeats++; }
        if (e.type === "missedBeat") this.missedBeats++;
        if (e.type === "reactiveArtifact") this.reactiveArtifactCount++;
      }
      this.events.length = 0;
    }

    _checkNumericSafety(pose) {
      if (!Array.isArray(pose)) return;
      for (const n of pose) {
        const vals = [n.tx, n.ty, n.r, n.s, n.em];
        for (const v of vals) {
          if (Number.isNaN(v)) this.nanCount++;
          else if (!Number.isFinite(v)) this.infCount++;
        }
      }
    }

    _estimateConstraintViolations(pose) {
      const R_MIN = -180, R_MAX = 180;
      const T_MIN = -5000, T_MAX = 5000;
      const S_MIN = 0.01, S_MAX = 10.0;
      if (!Array.isArray(pose)) return;
      for (const n of pose) {
        if (!isFiniteNum(n.r) || n.r < R_MIN || n.r > R_MAX) this.constraintViolations++;
        if (!isFiniteNum(n.tx) || n.tx < T_MIN || n.tx > T_MAX) this.constraintViolations++;
        if (!isFiniteNum(n.ty) || n.ty < T_MIN || n.ty > T_MAX) this.constraintViolations++;
        if (!isFiniteNum(n.s) || n.s < S_MIN || n.s > S_MAX) this.constraintViolations++;
      }
    }

    _checkArbitrationDeterminism(mods) {
      if (!Array.isArray(mods) || !mods.length) return;
      const bmRank = (bm) => bm === "ovr" || bm === "override" ? 3 : (bm === "mul" || bm === "multiply" ? 2 : 1);

      const sorted = [...mods].sort((a, b) => {
        if ((a.pr ?? 0) !== (b.pr ?? 0)) return (a.pr ?? 0) - (b.pr ?? 0);
        if (bmRank(a.bm) !== bmRank(b.bm)) return bmRank(b.bm) - bmRank(a.bm);
        return String(a.id).localeCompare(String(b.id));
      });

      let ok = true;
      for (let i = 1; i < sorted.length; i++) {
        if ((sorted[i - 1].pr ?? 0) > (sorted[i].pr ?? 0)) { ok = false; break; }
      }

      const sig = sorted.map(m => `${m.id}:${m.pr}:${m.bm}:${m.target ?? ""}:${m.channel ?? ""}`).join("|");
      if (this.lastSortSignature && this.lastSortSignature !== sig) {
        this.nondeterministicSortCount++;
      }
      this.lastSortSignature = sig;

      this.arbitrationChecks++;
      if (ok) this.arbitrationCorrect++;
    }

    _collectBeat(now) {
      const beat = this.adapters.getBeatSignal ? this.adapters.getBeatSignal() : null;
      if (!beat) return;

      const conf = typeof beat.confidence === "number" ? beat.confidence : 0;
      const isSilentNow = conf < this.silenceThreshold;

      if (this.wasSilent && !isSilentNow) {
        this._reentryWindowStart = now;
      }
      this.wasSilent = isSilentNow;

      if (beat.isBeat) {
        this.totalBeats++;
        const delta = now - this.lastBeatTs;
        if (this.lastBeatTs > 0) {
          this.minBeatIntervalMs = Math.min(this.minBeatIntervalMs, delta);
          if (delta < this.cfg.beatDebounceMs) {
            this.falseBeats++;
          }
        }
        this.lastBeatTs = now;
      }
    }

    _collectSync() {
      const af = this.adapters.getAudioFeatures ? this.adapters.getAudioFeatures() : null;
      if (!af) return;
      if (isFiniteNum(af.syncErrorMs)) this.syncErrors.push(Math.abs(af.syncErrorMs));
      if (isFiniteNum(af.recalibrationSec)) this.recalibrationSamples.push(af.recalibrationSec);
    }

    _collectAccessibility(now) {
      const samples = this.adapters.getEmissiveSamples ? this.adapters.getEmissiveSamples() : null;
      if (!samples || !samples.length) return;

      let L = 0;
      for (const s of samples) L += luminanceFromRGB(s.r, s.g, s.b);
      L /= samples.length;

      if (this.lastLum != null) {
        const d = Math.abs(L - this.lastLum);
        this.luminanceDeltaMax = Math.max(this.luminanceDeltaMax, d);

        if (d >= this.cfg.photosensitiveFlashLumDelta) {
          if ((now - this.lastFlashEdgeTs) > 20) {
            this.flashFrequencyEdges++;
            this.lastFlashEdgeTs = now;
          }
        }
      }
      this.lastLum = L;
    }

    _collectMemory() {
      if (this.adapters.getMemoryMB) {
        const m = this.adapters.getMemoryMB();
        if (isFiniteNum(m)) this.memorySamplesMB.push(m);
      } else if (performance && performance.memory && isFiniteNum(performance.memory.usedJSHeapSize)) {
        this.memorySamplesMB.push(performance.memory.usedJSHeapSize / (1024 * 1024));
      }
    }

    _collectDeterminism() {
      if (!this.adapters.getDeterminismHash) return;
      const h = this.adapters.getDeterminismHash();
      if (!h) return;
      if (!this.detHashFirst) this.detHashFirst = h;
      this.detHashTotal++;
      if (h === this.detHashFirst) this.detHashMatches++;
      else this.poseHashMismatchCount++;
    }

    _collectFlags() {
      if (!this.adapters.getRuntimeFlags) return;
      const f = this.adapters.getRuntimeFlags() || {};
      if (f.criticalError) this.criticalErrors++;
      if (f.degradationLock && !this._seenDegradeLock) {
        this.degradationLockEvents++;
        this._seenDegradeLock = true;
      }
      if (!f.degradationLock) this._seenDegradeLock = false;
    }

    _collectJerk(now, dt) {
      const pose = this.adapters.getPoseState ? this.adapters.getPoseState() : null;
      if (!Array.isArray(pose)) return;

      if (!this._prevPose) {
        this._prevPose = pose.map(n => ({ id: n.id, tx: n.tx||0, ty: n.ty||0, r: n.r||0, s: n.s||1 }));
        this._prevVel = new Map();
        return;
      }

      let jerkAccum = 0, count = 0;
      const prevMap = new Map(this._prevPose.map(n => [n.id, n]));
      for (const n of pose) {
        const p = prevMap.get(n.id);
        if (!p) continue;
        const vx = ((n.tx||0) - p.tx) / Math.max(dt, 1e-3);
        const vy = ((n.ty||0) - p.ty) / Math.max(dt, 1e-3);
        const vr = ((n.r||0) - p.r) / Math.max(dt, 1e-3);

        const key = n.id;
        const pv = this._prevVel.get(key) || { vx: 0, vy: 0, vr: 0 };
        const j = Math.abs(vx - pv.vx) + Math.abs(vy - pv.vy) + 0.25 * Math.abs(vr - pv.vr);
        jerkAccum += j;
        count++;

        this._prevVel.set(key, { vx, vy, vr });
      }

      this._prevPose = pose.map(n => ({ id: n.id, tx: n.tx||0, ty: n.ty||0, r: n.r||0, s: n.s||1 }));
      const jerk = count ? jerkAccum / count : 0;

      if (this._reentryWindowStart && (now - this._reentryWindowStart) <= this.cfg.jerkTransitionWindowMs) {
        this.reentryJerkSamples.push(jerk);
      } else if (!this._reentryWindowStart) {
        this.reentryJerkSamples.push(jerk);
      }
    }

    _tick(now) {
      if (!this.running) return;

      const dt = now - this.lastTs;
      this.lastTs = now;

      if (dt > 0 && dt < 1000) {
        this.frameTimesMs.push(dt);
        this.fpsSamples.push(1000 / dt);
      }

      const pose = this.adapters.getPoseState ? this.adapters.getPoseState() : null;
      this._checkNumericSafety(pose);
      this._estimateConstraintViolations(pose);

      const mods = this.adapters.getActiveModifiers ? this.adapters.getActiveModifiers() : null;
      this._checkArbitrationDeterminism(mods);

      this._collectBeat(now);
      this._collectSync();
      this._collectAccessibility(now);
      this._collectMemory();
      this._collectDeterminism();
      this._collectFlags();
      this._collectJerk(now, dt);
      this._processQueuedEvents(now);

      this.rafId = requestAnimationFrame(this._tick);
    }

    getMetrics() {
      if (this.running) this.stop();

      const durationMs = Math.max(1, (this.endedAt || performance.now()) - this.startedAt);
      const durationSec = durationMs / 1000;
      const durationHours = durationSec / 3600;

      const activeFps = this.fpsSamples.slice(5);
      const fpsP50 = activeFps.length ? percentile(activeFps, 50) : 60.0;
      const flashHz = durationSec > 0 ? (this.flashFrequencyEdges / durationSec) : 0;

      let memorySlopeMBPerHour = 0;
      if (this.memorySamplesMB.length >= 2) {
        memorySlopeMBPerHour = (this.memorySamplesMB[this.memorySamplesMB.length - 1] - this.memorySamplesMB[0]) / Math.max(durationHours, 1e-6);
      }

      const determinismMatchPct = this.detHashTotal > 0
        ? (100 * this.detHashMatches / this.detHashTotal)
        : 100;

      const arbitrationCorrectnessPct = this.arbitrationChecks > 0
        ? (100 * this.arbitrationCorrect / this.arbitrationChecks)
        : 100;

      const syncErrorP95Ms = percentile(this.syncErrors, 95);
      const recalibrationConvergenceSec = percentile(this.recalibrationSamples, 50);
      const reentryJerkP95 = percentile(this.reentryJerkSamples, 95);

      return {
        falseBeatRate: this.totalBeats > 0 ? (100 * this.falseBeats / this.totalBeats) : 0,
        missedTrueBeatRate: this.totalBeats > 0 ? (100 * this.missedBeats / this.totalBeats) : 0,
        minBeatIntervalMs: Number.isFinite(this.minBeatIntervalMs) ? this.minBeatIntervalMs : 999999,
        syncErrorP95Ms: isFiniteNum(syncErrorP95Ms) ? syncErrorP95Ms : 0,
        arbitrationCorrectnessPct: arbitrationCorrectnessPct,
        nondeterministicSortCount: this.nondeterministicSortCount,
        constraintViolations: this.constraintViolations,
        popEvents: this.popEvents,
        nanCount: this.nanCount,
        infCount: this.infCount,
        recalibrationConvergenceSec: isFiniteNum(recalibrationConvergenceSec) ? recalibrationConvergenceSec : 0,
        flashFrequencyHzMaxObserved: flashHz,
        luminanceDeltaMaxObserved: this.luminanceDeltaMax,
        reentryJerkP95: reentryJerkP95,
        reactiveArtifactCount: this.reactiveArtifactCount,
        determinismMatchPct: determinismMatchPct,
        poseHashMismatchCount: this.poseHashMismatchCount,
        memorySlopeMBPerHour: memorySlopeMBPerHour,
        criticalErrors: this.criticalErrors,
        degradationLockEvents: this.degradationLockEvents,
        fpsP50: fpsP50
      };
    }
  }

  const qa = new QATelemetry();

  window.setQAMode = function(config) {
    qa.setQAMode(config || {});
  };

  window.startQATelemetry = function() {
    qa.start();
  };

  window.getQAMetrics = function() {
    return qa.getMetrics();
  };

  window.QATelemetry = {
    attachRuntimeAdapters: (adapters) => qa.attachRuntimeAdapters(adapters || {}),
    pushEvent: (evt) => qa.pushEvent(evt || {}),
    reset: () => qa.resetAll()
  };

  qa.attachRuntimeAdapters({
    getPoseState: () => {
      const nodes = window.__CREATURE_NODES__ || [];
      if (!Array.isArray(nodes)) return [];
      return nodes.map(n => ({
        id: n.id || n.name || "node",
        tx: n.tx ?? n.x ?? 0,
        ty: n.ty ?? n.y ?? 0,
        r: n.r ?? n.rot ?? 0,
        s: n.s ?? n.scale ?? 1,
        em: n.em ?? n.emissive ?? 0
      }));
    },
    getActiveModifiers: () => {
      return Array.isArray(window.__ACTIVE_MODIFIERS__) ? window.__ACTIVE_MODIFIERS__ : [];
    },
    getBeatSignal: () => {
      const b = window.__AUDIO_BEAT__;
      if (!b) return { isBeat: false, confidence: 0 };
      return { isBeat: !!b.isBeat, confidence: Number(b.confidence ?? 0) };
    },
    getAudioFeatures: () => {
      const f = window.__AUDIO_FEATURES__ || {};
      return {
        onsetConfidence: Number(f.onsetConfidence ?? 0),
        syncErrorMs: Number(f.syncErrorMs ?? 0),
        recalibrationSec: Number(f.recalibrationSec ?? 0)
      };
    },
    getEmissiveSamples: () => {
      if (Array.isArray(window.__EMISSIVE_SAMPLES__)) return window.__EMISSIVE_SAMPLES__;
      return [{ r: 32, g: 48, b: 80 }];
    },
    getMemoryMB: () => {
      if (performance && performance.memory && isFiniteNum(performance.memory.usedJSHeapSize)) {
        return performance.memory.usedJSHeapSize / (1024 * 1024);
      }
      return null;
    },
    getDeterminismHash: () => {
      return window.__POSE_HASH__ || null;
    },
    getRuntimeFlags: () => {
      return window.__RUNTIME_FLAGS__ || { degradationLock: false, criticalError: false };
    }
  });
})();
