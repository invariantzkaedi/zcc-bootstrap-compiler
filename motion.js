(function () {
  "use strict";

  function pulse(el, cls = "glow-pulse", ms = 650) {
    if (!el) return;
    el.classList.remove(cls);
    void el.offsetWidth;
    el.classList.add(cls);
    setTimeout(() => el.classList.remove(cls), ms);
  }

  function staggerIn(nodes, step = 70) {
    if (!nodes) return;
    nodes.forEach((node, i) => {
      node.style.animationDelay = `${i * step}ms`;
      node.classList.add("reveal");
    });
  }

  function attachTilt(el, strength = 10) {
    if (!el) return;
    let targetX = 0, targetY = 0;
    let currentX = 0, currentY = 0;

    el.addEventListener("mousemove", (e) => {
      const r = el.getBoundingClientRect();
      const px = (e.clientX - r.left) / r.width - 0.5;
      const py = (e.clientY - r.top) / r.height - 0.5;
      targetX = py * -strength;
      targetY = px * strength;
    });

    el.addEventListener("mouseleave", () => {
      targetX = 0;
      targetY = 0;
    });

    function tick() {
      currentX += (targetX - currentX) * 0.08;
      currentY += (targetY - currentY) * 0.08;
      el.style.transform = `perspective(900px) rotateX(${currentX}deg) rotateY(${currentY}deg)`;
      requestAnimationFrame(tick);
    }
    tick();
  }

  function addRipple(btn) {
    if (!btn) return;
    btn.addEventListener("click", (e) => {
      const rect = btn.getBoundingClientRect();
      // Ensure ripple container has position relative
      if (window.getComputedStyle(btn).position === "static") {
        btn.style.position = "relative";
      }
      const ripple = document.createElement("span");
      ripple.className = "ripple";
      ripple.style.left = `${e.clientX - rect.left}px`;
      ripple.style.top = `${e.clientY - rect.top}px`;
      btn.appendChild(ripple);
      setTimeout(() => ripple.remove(), 700);
    });
  }

  function countUp(el, to, duration = 1200) {
    if (!el) return;
    const from = Number(el.textContent.replace(/[^\d.-]/g, "")) || 0;
    const start = performance.now();

    function frame(now) {
      const t = Math.min(1, (now - start) / duration);
      const eased = 1 - Math.pow(1 - t, 3);
      const val = from + (to - from) * eased;
      el.textContent = Math.round(val).toLocaleString();
      if (t < 1) requestAnimationFrame(frame);
    }

    requestAnimationFrame(frame);
  }

  function toast(msg, type = "info") {
    const el = document.createElement("div");
    el.className = `toast toast-${type}`;
    el.textContent = msg;
    
    // Style toast on demand
    Object.assign(el.style, {
      position: "fixed",
      bottom: "24px",
      right: "24px",
      padding: "12px 24px",
      background: "rgba(10, 12, 24, 0.9)",
      border: "1px solid rgba(0, 255, 204, 0.3)",
      color: "#e8f8ff",
      fontFamily: "monospace",
      fontSize: "13px",
      borderRadius: "8px",
      boxShadow: "0 0 20px rgba(0, 255, 204, 0.2)",
      zIndex: 999999,
      transform: "translateY(100px)",
      opacity: 0,
      transition: "transform 0.3s cubic-bezier(0.2, 0.8, 0.2, 1), opacity 0.3s"
    });
    
    if (type === "success") {
      el.style.borderColor = "rgba(0, 255, 136, 0.5)";
      el.style.boxShadow = "0 0 20px rgba(0, 255, 136, 0.2)";
    }
    
    document.body.appendChild(el);
    setTimeout(() => {
      el.style.transform = "translateY(0)";
      el.style.opacity = 1;
    }, 10);
    
    setTimeout(() => {
      el.style.transform = "translateY(100px)";
      el.style.opacity = 0;
      setTimeout(() => el.remove(), 300);
    }, 2200);
  }

  function burstAtPoint(x, y, color = "#00ffcc", n = 18) {
    for (let i = 0; i < n; i++) {
      const p = document.createElement("div");
      p.className = "burst-particle";
      p.style.left = `${x}px`;
      p.style.top = `${y}px`;
      p.style.background = color;
      p.style.setProperty("--dx", `${(Math.random() - 0.5) * 160}px`);
      p.style.setProperty("--dy", `${(Math.random() - 0.5) * 160}px`);
      document.body.appendChild(p);
      setTimeout(() => p.remove(), 900);
    }
  }

  function burstOrigin(x, y, color = "#00ffcc") {
    const el = document.createElement("div");
    el.className = "burst-origin";
    el.style.left = `${x}px`;
    el.style.top = `${y}px`;
    el.style.setProperty("--burst-color", color);
    document.body.appendChild(el);
    setTimeout(() => el.remove(), 900);
  }

  function attachParallax(el, strength = 10) {
    if (!el) return;
    let targetX = 0, targetY = 0;
    let currentX = 0, currentY = 0;

    el.addEventListener("mousemove", (e) => {
      const r = el.getBoundingClientRect();
      const px = (e.clientX - r.left) / r.width - 0.5;
      const py = (e.clientY - r.top) / r.height - 0.5;
      targetX = py * -strength;
      targetY = px * strength;
    });

    el.addEventListener("mouseleave", () => {
      targetX = 0;
      targetY = 0;
    });

    function tick() {
      currentX += (targetX - currentX) * 0.08;
      currentY += (targetY - currentY) * 0.08;
      el.style.transform = `perspective(900px) rotateX(${currentX}deg) rotateY(${currentY}deg)`;
      requestAnimationFrame(tick);
    }
    tick();
  }

  function animatePath(nodes, duration = 1200) {
    if (!nodes || !nodes.length) return;
    nodes.forEach((n, i) => {
      setTimeout(() => {
        n.classList.add("path-active");
        setTimeout(() => n.classList.remove("path-active"), 450);
      }, i * (duration / nodes.length));
    });
  }

  function attachCommandPalette(openFn) {
    window.addEventListener("keydown", (e) => {
      if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === "k") {
        e.preventDefault();
        openFn?.();
      }
    });
  }

  window.ZCCMotion = {
    pulse,
    staggerIn,
    attachTilt,
    addRipple,
    countUp,
    toast,
    burstAtPoint,
    burstOrigin,
    attachParallax,
    animatePath,
    attachCommandPalette,
  };
})();
