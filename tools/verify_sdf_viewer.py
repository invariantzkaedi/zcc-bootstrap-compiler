#!/usr/bin/env python3
import re
import sys
from pathlib import Path

FAIL = 1
PASS = 0

def find_all(pattern, text):
    return [m.start() for m in re.finditer(pattern, text)]

def check_inside_function(pos, html):
    # WebGLLiteBackend bounds
    lite_start = html.find("const WebGLLiteBackend")
    if lite_start >= 0:
        lite_end = html.find("})();", lite_start) + 5
        if lite_start <= pos <= lite_end:
            return True
            
    # initWebGL bounds
    init_start = html.find("function initWebGL()")
    if init_start >= 0:
        init_end = html.find("function degradeQuality()", init_start)
        if init_end == -1:
            init_end = html.find("function resize()", init_start)
        if init_start <= pos <= init_end:
            return True
            
    # createShader bounds
    cs_start = html.find("function createShader(gl, type, source)")
    if cs_start >= 0:
        cs_end = html.find("function initWebGL()", cs_start)
        if cs_start <= pos <= cs_end:
            return True

    # render loop bounds
    render_start = html.find("function render(time)")
    if render_start >= 0:
        render_end = html.find("function pauseRender()", render_start)
        if render_start <= pos <= render_end:
            return True
            
    return False

def main():
    if len(sys.argv) != 2:
        print("Usage: verify_sdf_viewer.py <viewer.html>")
        return FAIL

    path = Path(sys.argv[1])
    if not path.exists():
        print(f"Error: File {path} does not exist.")
        return FAIL

    html = path.read_text(encoding="utf-8", errors="replace")

    checks = []

    def check(name, ok, detail=""):
        checks.append((name, ok, detail))

    # Must contain safe front door.
    check(
        "has SVG preview container",
        "svg-preview-container" in html,
        "missing svg-preview-container"
    )

    check(
        "has Canvas2D backend",
        "canvas-2d" in html,
        "missing Canvas2D backend marker"
    )

    check(
        "has WebGL Lite backend",
        "WebGLLiteBackend" in html,
        "missing WebGL Lite backend implementation"
    )

    check(
        "has WebGL Lite lazy init",
        "webglLiteInitialized = false" in html or "webglLiteInitialized" in html,
        "missing WebGL Lite lazy status tracking"
    )

    check(
        "has WebGL Full lazy init function",
        "function initWebGL" in html,
        "missing initWebGL"
    )

    # WebGL context must not appear at the top-level page load.
    get_ctx_positions = find_all(r"getContext\s*\(\s*['\"]webgl2['\"]", html)
    check(
        "has expected webgl2 getContext occurrences",
        len(get_ctx_positions) == 2,
        f"expected exactly 2 getContext('webgl2') calls, found {len(get_ctx_positions)}"
    )

    for idx, pos in enumerate(get_ctx_positions):
        ok = check_inside_function(pos, html)
        check(
            f"webgl2 getContext occurrence {idx+1} is lazy",
            ok,
            f"getContext at index {pos} is not enclosed inside lazy boundaries"
        )

    # Shader creation must not happen before lazy init functions.
    create_shader_positions = find_all(r"createShader\s*\(", html)
    for idx, pos in enumerate(create_shader_positions):
        # Allow checking within functional blocks
        ok = check_inside_function(pos, html)
        check(
            f"shader creation occurrence {idx+1} is lazy",
            ok,
            f"createShader at index {pos} is not enclosed inside lazy boundaries"
        )

    # Render loop should not auto-start WebGL on load.
    raf_positions = find_all(r"requestAnimationFrame\s*\(\s*render\s*\)", html)
    for pos in raf_positions:
        ok = check_inside_function(pos, html)
        check(
            f"full render loop at position {pos} is lazy",
            ok,
            f"requestAnimationFrame(render) outside lazy render/initWebGL boundaries"
        )

    # SVG cap marker should exist.
    check(
        "SVG primitive cap marker exists",
        "svgMaxPrimitives" in html or "SVG_MAX_PRIMITIVES" in html or "SVG preview truncated" in html or "svg_truncated" in html,
        "missing SVG cap marker"
    )

    failed = [c for c in checks if not c[1]]

    for name, ok, detail in checks:
        status = "PASS" if ok else "FAIL"
        print(f"{status} {name}" + (f" — {detail}" if detail else ""))

    if failed:
        print(f"\nViewer verification failed: {len(failed)} issue(s)")
        return FAIL

    print("\n777JACKPOT777 — SDF VIEWER STATIC SAFETY VERIFIED.")
    return PASS

if __name__ == "__main__":
    sys.exit(main())
