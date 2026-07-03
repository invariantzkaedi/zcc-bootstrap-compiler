#!/usr/bin/env python3
import sys
import re
from pathlib import Path

FAIL = 1
PASS = 0

def main():
    root = Path(__file__).parent.parent
    html_path = root / "audio_reactive_creature.html"
    js_path = root / "audio_reactive_creature.js"
    manifest_path = root / "neon_creature_manifest.json"

    checks = []

    def check(name, ok, detail=""):
        checks.append((name, ok, detail))

    # 1. Verify files exist
    check("HTML visualizer file exists", html_path.exists(), f"missing: {html_path}")
    check("JS runtime file exists", js_path.exists(), f"missing: {js_path}")
    check("Manifest JSON exists", manifest_path.exists(), f"missing: {manifest_path}")

    if not (html_path.exists() and js_path.exists() and manifest_path.exists()):
        print("Required files are missing. Terminating checks.")
        return FAIL

    html = html_path.read_text(encoding="utf-8", errors="replace")
    js = js_path.read_text(encoding="utf-8", errors="replace")

    # 2. HTML DOM elements check
    check(
        "HTML contains interactive drop zone",
        "drop-zone" in html,
        "missing drag-and-drop overlay"
    )
    check(
        "HTML contains active mood selector",
        "select-mood" in html,
        "missing select-mood dropdown element"
    )
    check(
        "HTML contains energy macro slider",
        "slider-energy" in html,
        "missing slider-energy element"
    )
    check(
        "HTML contains elegance macro slider",
        "slider-elegance" in html,
        "missing slider-elegance element"
    )
    check(
        "HTML contains chaos macro slider",
        "slider-chaos" in html,
        "missing slider-chaos element"
    )
    check(
        "HTML contains active modifiers diagnostic",
        "diag-mods" in html,
        "missing diag-mods diagnostic counter"
    )
    check(
        "HTML imports JS runtime",
        '<script src="audio_reactive_creature.js"></script>' in html,
        "missing local script import"
    )

    # 3. JS engine checks
    check(
        "JS contains Legendary v2 manifest embed",
        "legendaryManifest = {" in js or "const legendaryManifest" in js,
        "missing embedded manifest configuration"
    )
    check(
        "JS contains active mood profiles",
        "moodPacks = [" in js or "const moodPacks" in js,
        "missing embedded mood packs configuration"
    )
    check(
        "JS handles macro scaling values",
        "macro: {" in js or "state.macro" in js,
        "missing engine state macro object"
    )
    check(
        "JS contains priority-based sorting",
        "allModifiers.sort(" in js or "a.pr - b.pr" in js,
        "missing priority blending sort algorithm"
    )
    check(
        "JS contains composed gain calculator",
        "getComposedGain" in js,
        "missing macro composed gain calculator function"
    )
    check(
        "JS contains Gaussian wave-solver",
        "Math.exp(- (diff * diff)" in js,
        "missing Gaussian wavefront solver equations"
    )
    check(
        "JS contains color blending helper",
        "lerpColor" in js,
        "missing lerpColor function"
    )

    failed = [c for c in checks if not c[1]]

    print("=== Visualizer Test Verification Report ===")
    for name, ok, detail in checks:
        status = "PASS" if ok else "FAIL"
        print(f"[{status}] {name}" + (f" — {detail}" if detail else ""))

    if failed:
        print(f"\nVerification failed: {len(failed)} mismatch(es)")
        return FAIL

    print("\nSELF-HOST VERIFIED — ALL VISUALIZER RUNTIME ASSETS PASS COMPLIANCE.")
    return PASS

if __name__ == "__main__":
    sys.exit(main())
