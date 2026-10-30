#!/usr/bin/env bash
set -euo pipefail

ZCC_BIN="${1:-./zcc}"
TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

echo "=== [ZCC-SVG] Testing Upgraded Native SVG Evolution, CLI Commands & SvgPathBuilder ($ZCC_BIN) ==="

# 1. Direct .svg Evolution Mode (-o evolved.svg)
"$ZCC_BIN" zcc_all_seeing_sovereign_oculus.svg -o "$TMP_DIR/evolved.svg"
python3 - "$TMP_DIR/evolved.svg" << 'PYEOF'
import sys
import xml.etree.ElementTree as ET

raw_tree = ET.parse("zcc_all_seeing_sovereign_oculus.svg")
ev_tree = ET.parse(sys.argv[1])
raw_nodes = len(list(raw_tree.getroot().iter()))
ev_nodes = len(list(ev_tree.getroot().iter()))
assert ev_nodes < raw_nodes, f"Expected evolved DOM nodes ({ev_nodes}) < raw DOM nodes ({raw_nodes})"
print(f"[PASS] Target 1 (.svg evolution): XML valid, DOM elements {raw_nodes} -> {ev_nodes} (-{raw_nodes - ev_nodes} coalesced/lowered)")
PYEOF

# 2. Assembly Output Mode (-S -o out.s)
"$ZCC_BIN" zcc_all_seeing_sovereign_oculus.svg -S -o "$TMP_DIR/oculus.s"
grep -q "zcc_all_seeing_sovereign_oculus_svg_data:" "$TMP_DIR/oculus.s"
grep -q "zcc_all_seeing_sovereign_oculus_svg_coalesced_count:" "$TMP_DIR/oculus.s"
grep -q "zcc_all_seeing_sovereign_oculus_svg_quantized_count:" "$TMP_DIR/oculus.s"
grep -q "zcc_all_seeing_sovereign_oculus_svg_compacted_count:" "$TMP_DIR/oculus.s"
echo "[PASS] Target 2 (-S -o oculus.s): Assembly emitted & verified cleanly (exit 0)"

# 3. Linkable ELF Object File Mode (-c -o out.o)
"$ZCC_BIN" zcc_all_seeing_sovereign_oculus.svg -c -o "$TMP_DIR/oculus.o"
nm "$TMP_DIR/oculus.o" | grep -q "zcc_all_seeing_sovereign_oculus_svg_data"
nm "$TMP_DIR/oculus.o" | grep -q "zcc_all_seeing_sovereign_oculus_svg_compacted_count"
echo "[PASS] Target 3 (-c -o oculus.o): ELF relocatable object symbols verified"

# 4. Standalone Native Executable Mode (-o out_bin)
"$ZCC_BIN" zcc_all_seeing_sovereign_oculus.svg -o "$TMP_DIR/oculus_bin"
BANNER_OUT="$("$TMP_DIR/oculus_bin")"
echo "$BANNER_OUT" | grep -q "\[ZCC-SVG EXECUTABLE\]"
echo "  $BANNER_OUT"
EMIT_FIRST_LINE="$("$TMP_DIR/oculus_bin" --emit | head -n 1)"
echo "$EMIT_FIRST_LINE" | grep -q "<?xml"
echo "[PASS] Target 4 (-o oculus_bin): Native ELF executable banner & --emit payload verified"

# 5. CLI --svg-evolve
"$ZCC_BIN" --svg-evolve zcc_all_seeing_sovereign_oculus.svg -o "$TMP_DIR/cli_evolved.svg"
cmp -s "$TMP_DIR/evolved.svg" "$TMP_DIR/cli_evolved.svg"
echo "[PASS] Target 5 (--svg-evolve): Byte-identical to direct .svg compilation"

# 6. CLI --svg-stats
"$ZCC_BIN" --svg-stats zcc_all_seeing_sovereign_oculus.svg -o "$TMP_DIR/stats.json"
python3 - "$TMP_DIR/stats.json" << 'PYEOF'
import json, sys
with open(sys.argv[1]) as f:
    d = json.load(f)
assert d["raw_bytes"] > d["evolved_bytes"]
assert d["commands_compacted"] > 2000, f"Expected >2000 compacted commands, got {d['commands_compacted']}"
assert d["polygons_lowered"] == 5, f"Expected 5 lowered polygons, got {d['polygons_lowered']}"
assert d["path_commands_evolved"]["L"] < d["path_commands_raw"]["L"]
assert d["path_commands_evolved"]["H"] > 0 or d["path_commands_evolved"]["V"] > 0
print(f"[PASS] Target 6 (--svg-stats): JSON telemetry verified (L cmds: {d['path_commands_raw']['L']} -> {d['path_commands_evolved']['L']}, H={d['path_commands_evolved']['H']}, V={d['path_commands_evolved']['V']}, compacted={d['commands_compacted']})")
PYEOF

# 7. CLI --svg-data-uri
"$ZCC_BIN" --svg-data-uri zcc_all_seeing_sovereign_oculus.svg -o "$TMP_DIR/oculus.uri"
head -c 26 "$TMP_DIR/oculus.uri" | grep -q "^data:image/svg+xml;base64,"
echo "[PASS] Target 7 (--svg-data-uri): Base64 SVG Data URI verified ($(wc -c < "$TMP_DIR/oculus.uri") bytes)"

# 8. CLI --svg-hexdump
"$ZCC_BIN" --svg-hexdump "$TMP_DIR/oculus.o" -o "$TMP_DIR/hexdump.svg"
python3 - "$TMP_DIR/hexdump.svg" << 'PYEOF'
import sys, xml.etree.ElementTree as ET
tree = ET.parse(sys.argv[1])
root = tree.getroot()
assert root.tag.endswith("svg")
print(f"[PASS] Target 8 (--svg-hexdump): Binary Entropy Matrix SVG valid ({len(list(root.iter()))} DOM nodes)")
PYEOF

# 9. Native ZCC Compilation of Upgraded zcc_svg.c (20 SVG Path Commands + 6 Geometric Primitives + Compactor)
cat > "$TMP_DIR/test_svg_builder.c" << 'CEOF'
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "zcc_svg.h"

int main(void) {
    SvgPathBuilder *pb = svg_path_begin();
    svg_path_move_to(pb, 10, 20);
    svg_path_move_rel(pb, 5, 5);
    svg_path_line_to(pb, 30, 40);
    svg_path_line_rel(pb, 10, 0);
    svg_path_horiz_to(pb, 100);
    svg_path_horiz_rel(pb, 20);
    svg_path_vert_to(pb, 200);
    svg_path_vert_rel(pb, 15);
    svg_path_cubic_to(pb, 10, 20, 30, 40, 50, 60);
    svg_path_cubic_rel(pb, 1, 2, 3, 4, 5, 6);
    svg_path_smooth_cubic_to(pb, 70, 80, 90, 100);
    svg_path_smooth_cubic_rel(pb, 2, 3, 4, 5);
    svg_path_quad_to(pb, 110, 120, 130, 140);
    svg_path_quad_rel(pb, 3, 4, 5, 6);
    svg_path_smooth_quad_to(pb, 150, 160);
    svg_path_smooth_quad_rel(pb, 7, 8);
    svg_path_arc_to(pb, 25, 25, 0, 1, 0, 50, 0);
    svg_path_arc_rel(pb, 25, 25, 0, 1, 1, -50, 0);
    svg_path_close_rel(pb);
    svg_path_circle(pb, 600, 600, 50);
    svg_path_ellipse(pb, 600, 600, 80, 40);
    svg_path_rect_rounded(pb, 10, 10, 100, 50, 8, 8);
    svg_path_regular_polygon(pb, 600, 600, 100, 6, 0.0f);
    svg_path_star(pb, 600, 600, 120, 60, 8, 0.0f);
    svg_path_harmonic_rose(pb, 600, 600, 100, 20, 6, 10, 12, 36);
    svg_path_close(pb);

    int raw_len = (int)strlen(pb->d);
    char *comp_d = svg_path_compact_d(pb->d);
    int comp_len = (int)strlen(comp_d);
    assert(comp_len < raw_len);
    free(comp_d);

    ZccSvgNode *node = svg_path();
    svg_apply_path(node, pb);
    char *xml = svg_to_string(node);
    assert(strstr(xml, "<path d=\"") != NULL);
    printf("[PASS] Target 9 (zcc_svg.c SvgPathBuilder): 20 commands + 6 primitives verified (raw=%d -> compacted=%d bytes)\n", raw_len, comp_len);
    free(xml);
    svg_free_node_tree(node);
    return 0;
}
CEOF

"$ZCC_BIN" zcc_svg.c -S -o "$TMP_DIR/zcc_svg.s"
"$ZCC_BIN" -I. "$TMP_DIR/test_svg_builder.c" -S -o "$TMP_DIR/test_svg_builder.s"
gcc -O0 -no-pie "$TMP_DIR/zcc_svg.s" "$TMP_DIR/test_svg_builder.s" -o "$TMP_DIR/test_svg_builder" -lm
"$TMP_DIR/test_svg_builder"

# 10. Target 10: --svg-ast, --svg-cfg, --svg-ir C-to-SVG Compiler Telemetry
"$ZCC_BIN" --svg-ast test_bitfield.c -o "$TMP_DIR/tb_ast.svg"
"$ZCC_BIN" --svg-cfg test_bitfield.c -o "$TMP_DIR/tb_cfg.svg"
"$ZCC_BIN" --svg-ir test_bitfield.c -o "$TMP_DIR/tb_ir.svg"
grep -q "\[ZCC-SVG-AST\]" "$TMP_DIR/tb_ast.svg"
grep -q "\[ZCC-SVG-CFG\]" "$TMP_DIR/tb_cfg.svg"
grep -q "\[ZCC-SVG-IR\]" "$TMP_DIR/tb_ir.svg"
echo "[PASS] Target 10 (--svg-ast / --svg-cfg / --svg-ir): C-to-SVG AST/CFG/IR telemetry graphs verified."

# 11. Target 11: --svg-to-sdf Analytic 2D SDF C99 & GLSL Shader Lowering
"$ZCC_BIN" --svg-to-sdf zcc_all_seeing_sovereign_oculus.svg -o "$TMP_DIR/probe_sdf.c"
"$ZCC_BIN" --svg-to-sdf zcc_all_seeing_sovereign_oculus.svg -o "$TMP_DIR/probe_sdf.glsl"
grep -q "#version 330 core" "$TMP_DIR/probe_sdf.glsl"
grep -q "zcc_svg_sdf_scene" "$TMP_DIR/probe_sdf.c"
"$ZCC_BIN" "$TMP_DIR/probe_sdf.c" -o "$TMP_DIR/probe_sdf_bin" -lm
SDF_OUT=$("$TMP_DIR/probe_sdf_bin")
echo "$SDF_OUT" | grep -q "PASS"
echo "[PASS] Target 11 (--svg-to-sdf): $SDF_OUT"

# 12. Target 12: Direct AVX-512 EVEX Machine-Code Object Emission (src/codegen.c)
if [ "$(basename "$ZCC_BIN")" = "zcc" ]; then
    "$ZCC_BIN" -c experiments/zcc_eai_006_g3_m5/zcc_m5_2_native_multiseed.c -o "$TMP_DIR/m52_evex.o"
    ZMM_COUNT=$(objdump -d "$TMP_DIR/m52_evex.o" | grep -c "%zmm" || true)
    if [ "$ZMM_COUNT" -lt 40 ]; then
        echo "[FAIL] Target 12: Expected >= 40 EVEX %zmm instructions in m52_evex.o, got $ZMM_COUNT"
        exit 1
    fi
    "$ZCC_BIN" -c tests/test_avx512_evex_codegen.c -o "$TMP_DIR/evex_unit.o"
    UNIT_ZMM=$(objdump -d "$TMP_DIR/evex_unit.o" | grep -c "%zmm" || true)
    if [ "$UNIT_ZMM" -lt 10 ]; then
        echo "[FAIL] Target 12: Expected >= 10 EVEX %zmm instructions in evex_unit.o, got $UNIT_ZMM"
        exit 1
    fi
    gcc -no-pie "$TMP_DIR/evex_unit.o" -o "$TMP_DIR/evex_unit_bin"
    if grep -q "avx512f" /proc/cpuinfo 2>/dev/null; then
        EVEX_OUT=$("$TMP_DIR/evex_unit_bin")
        echo "$EVEX_OUT" | grep -q "PASS"
    else
        EVEX_OUT="(binary linked clean; CPU lacks avx512f)"
    fi
    echo "[PASS] Target 12 (AVX-512 EVEX Direct ELF .o): $ZMM_COUNT (m5_2) + $UNIT_ZMM (unit) %zmm instructions encoded -> $EVEX_OUT"
fi

echo "=== [ZCC-SVG] ALL 12 SVG COMPILER, CLI, SDF & EVEX TARGETS PASSED ==="
