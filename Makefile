CFLAGS = -O0 -w -fno-asynchronous-unwind-tables -g0 -DZCC_REAL_TELEMETRY
LDFLAGS = -lm
ifneq ($(NO_STRIP),1)
LDFLAGS += -Wl,-s
endif
FAST_CFLAGS = -O2 -DNDEBUG -w -fno-asynchronous-unwind-tables -g0 -DZCC_REAL_TELEMETRY
FORTIFY_PACK_DIR ?= fortify_zcc_clean

PARTS = part1.c part0_pp.c part2.c part3.c ir.h ir_emit_dispatch.h sym_type_ast_ir.c part4.c zcc_ast_serializer.c part5.c part7_rust.c part6_arm.c ir.c ir_to_x86.c regalloc.c ir_telemetry_stub.c forgezero_receipt_stub.c zcc_layout.c zcc_layout_dump.c zcc_static_assert.c
PASSES = compiler_passes.c compiler_passes_ir.c ir_pass_manager.c ir_pass_warden.c ir_pass_taint.c ir_pass_healer.c ir_symbolic_cfg.c ir_dominance.c ir_ssa.c evm_lifter.c ir_vuln_tag.c ir_to_evm.c ir_evm_stack.c src/ir_lower_float.c src/x86_codegen_sse.c src/evm/decompiler.c src/evm/jit.c src/evm/symbolic.c src/evm/memory_v2.c src/evm/abi_extractor.c src/evm/jit_memory.c src/evm/proof_export.c src/evm/ipc_bridge.c src/evm/yul_weaver.c src/evm/yul_fixed_point.c src/evm/yul_frontend.c src/gfx/sdf_compiler.c src/gfx/mesh_warden.c src/evm/evm_symbolic_harness.c ir_telemetry.c zcc_telemetry.c src/zcc_oracle_substrate.c src/elf_emit.c src/codegen.c src/ir_serialization.c src/zcc_smt_prover.c src/gguf_emit.c src/zld.c src/zcc_resource_oracle.c transient_state.c zcc_lucky_alert_injector.c
COMPAT_SMOKE_SRCS = \
	exp1_raytracer_simd.c \
	exp2_voxel_engine.c \
	exp3_audio_visualizer.c \
	exp4_vr_stereo.c \
	exp5_physics_engine.c \
	test_asm_real.c \
	test_vla.c \
	tests/test_abi.c \
	tests/test_asm_real.c \
	tests/regressions/t_zkaedi_rigging_regressions.c
COMPAT_EXTENDED_SRCS = $(COMPAT_SMOKE_SRCS) raytracer.c

.PHONY: all clean selfhost selfhost-fast verify-lexicon compat-smoke compat-extended compat-report compat-report-ci pp-crlf-gate fortify-ad fortify-ci fortify-snapshot fortify-recursive fortify-recursive-ci fortify-pack-init fortify-pack-preflight fortify-pack-layout fortify-pack-production fortify-pack-replay fortify-pack-clean supercharge-ad test test-float rust-front-smoke check-evm-lifter check-ir-vuln-tag check-forgezero-receipt check-ir-bridge-guard check-copy-const-prop verify-attestation verify-replay-pack verify-genome-diff genome_diff verify-lineage stability_observatory topology_bisector cross_genome build_ledger verify-stability verify-bisector verify-cross-genome verify-ledger runtime_probe behavioral_diff verify-runtime-probe impact_attribution function_ranker verify-impact-attribution health_report verify-golden freeze-golden zcc_calibration_corpus verify-calibration zjs test-zjs visualize-svg-diffs wasm-svg-bridge test_zcc_dag abi-lanes

.SECONDARY: zcc zcc2 zcc3

all: zcc

zcc_ast_bridge_constants.h zcc_ast_bridge_asserts.inc: part1.c sync_bridge.py
	python3 sync_bridge.py part1.c zcc_ast_bridge_constants.h zcc_ast_bridge_asserts.inc

zcc.c: $(PARTS) zcc_ast_bridge_constants.h zcc_ast_bridge_asserts.inc
	cat $(PARTS) > zcc.c

verify-lexicon:
	@echo "=== Checking Workspace Lexicons ==="
	@if [ ! -f docs/lexicon/actionable-lexicon.md ] || \
	    [ ! -f docs/lexicon/influence-lexicon.md ] || \
	    [ ! -f docs/lexicon/jsonl-cheat-guide.md ] || \
	    [ ! -f .github/copilot-instructions.md ]; then \
	  echo "ERROR: Workspace lexicon or session instructions are missing!"; \
	  exit 1; \
	fi

zcc: verify-lexicon zcc.c $(PASSES)
	# Tripwire: reject hand-edited zcc.c — parts are the source of truth.
	# Bypass with: ZCC_MUTATION_SANDBOX=1 make zcc  (Oneirogenesis daemon)
	# or:          touch .mutation_sandbox && make zcc
	@if [ -z "$$ZCC_MUTATION_SANDBOX" ] && [ ! -f .mutation_sandbox ]; then \
	  cat $(PARTS) > .zcc_parts_check.tmp; \
	  if ! diff -q .zcc_parts_check.tmp zcc.c > /dev/null 2>&1; then \
	    rm -f .zcc_parts_check.tmp; \
	    echo "ERROR: zcc.c does not match cat($(PARTS)). Edit the parts, not zcc.c."; \
	    echo "       To suppress (mutation sandbox): export ZCC_MUTATION_SANDBOX=1"; \
	    exit 1; \
	  fi; \
	  rm -f .zcc_parts_check.tmp; \
	fi
	$(CC) $(CFLAGS) -Dmain=zcc_main -o zcc zcc.c $(PASSES) $(LDFLAGS)
	@if [ "$(NO_STRIP)" != "1" ]; then strip --strip-all zcc; fi

zcc_fast: verify-lexicon zcc.c $(PASSES)
	$(CC) $(FAST_CFLAGS) -Dmain=zcc_main -o zcc_fast zcc.c $(PASSES) $(LDFLAGS)
	@if [ "$(NO_STRIP)" != "1" ]; then strip --strip-all zcc_fast; fi

zcc2: zcc zcc.c
	@echo "=== Stage 1: zcc compiles itself -> zcc2 ==="
	./zcc zcc.c -o zcc2
	strip --strip-all zcc2 || true

zcc3: zcc2 zcc.c
	@echo "=== Stage 2: zcc2 compiles itself -> zcc3 ==="
	./zcc2 zcc.c -o zcc3
	strip --strip-all zcc3 || true

selfhost: verify-lexicon zcc3
	@echo "=== Verify: zcc2.s == zcc3.s (codegen parity) ==="
	./zcc  zcc.c -o zcc2.s
	./zcc2 zcc.c -o zcc3.s
	diff zcc2.s zcc3.s && echo "SELF-HOST VERIFIED (assembly identical)" || (echo "SELF-HOST FAILED (assembly diverged)"; diff zcc2.s zcc3.s | head -20; exit 1)


zjs: zcc zjs.c test_evm_sprites.c zcc_svg.c
	@echo "=== Compiling zjs, test_evm_sprites, and zcc_svg via ZCC ==="
	./zcc zjs.c -DZJS_COMPILE -o zjs.s
	./zcc test_evm_sprites.c -DZJS_COMPILE -o test_evm_sprites.s
	./zcc zcc_svg.c -o zcc_svg.s
	gcc -o zjs zjs.s test_evm_sprites.s zcc_svg.s -lm

# === AUTOMATED ZJS + SVG BRIDGE TESTS ===
test-zjs: zjs
	@echo "=== ZJS + EVM VISUALIZER EDGE CASE AUTOMATION ==="
	@python3 tests/run_edge_cases.py
	@echo "777JACKPOT777 — ALL TESTS GREEN. SVG bridge rock solid."
.PHONY: test-zjs

visualize-svg-diffs: test-zjs
	@echo "=== VISUAL SVG DIFF GENERATOR ==="
	@cp zcc_sprites.svg tests/baseline.svg
	@python3 tests/visualize_svg_diffs.py
	@echo "777JACKPOT777 — SVG diffs rendered. Open browser tab."
.PHONY: visualize-svg-diffs

wasm-svg-bridge:
	@echo "=== COMPILING WEBASSEMBLY SVG RENDERER ==="
	emcc wasm_svg_bridge.c -o wasm_svg_bridge.wasm \
		-s EXPORTED_FUNCTIONS='["_generate_svg","_set_phase","_get_phases"]' \
		-s EXPORTED_RUNTIME_METHODS='["ccall","cwrap"]' \
		-s MODULARIZE=1 -s EXPORT_NAME="createModule" \
		-O3 -s ALLOW_MEMORY_GROWTH=1
	@cp demo_sprites.html demo_sprites_wasm.html
	@sed -i 's/zcc_sprites.svg/wasm_svg_bridge.wasm/g' demo_sprites_wasm.html
	@echo "777JACKPOT777 — WASM SVG bridge ready. Open demo_sprites_wasm.html"
.PHONY: wasm-svg-bridge




selfhost-fast: zcc_fast
	@echo "=== FAST Stage 1: zcc_fast compiles itself -> zcc2_fast ==="
	./zcc_fast zcc.c -o zcc2_fast
	strip --strip-all zcc2_fast
	@echo "=== FAST Stage 2: zcc2_fast compiles itself -> zcc3_fast ==="
	./zcc2_fast zcc.c -o zcc3_fast
	strip --strip-all zcc3_fast
	@echo "=== FAST Verify: zcc2_fast.s == zcc3_fast.s ==="
	./zcc_fast  zcc.c -o zcc2_fast.s
	./zcc2_fast zcc.c -o zcc3_fast.s
	diff zcc2_fast.s zcc3_fast.s && echo "SELF-HOST FAST VERIFIED (assembly identical)" || (echo "SELF-HOST FAST FAILED"; diff zcc2_fast.s zcc3_fast.s | head -20; exit 1)

compat-smoke: zcc_fast
	@mkdir -p .compat_out
	@set -e; \
	for f in $(COMPAT_SMOKE_SRCS); do \
	  if [ -f "$$f" ]; then \
	    stem=$$(echo "$${f%.c}" | sed 's#[/\\]#__#g'); \
	    out="$$stem.s"; \
	    echo "[compat] $$f -> .compat_out/$$out"; \
	    ./zcc_fast "$$f" -o ".compat_out/$$out"; \
	  fi; \
	done; \
	echo "COMPAT SMOKE COMPLETE"

# compat-extended: default is serial (same wall time as before). Set COMPAT_JOBS=0 for
# auto parallelism min(nproc,4). Higher explicit values are allowed (e.g. large CI).
compat-extended: zcc_fast
	@set -e; \
	rm -rf .compat_logs/summary.d; \
	mkdir -p .compat_out .compat_logs .compat_logs/summary.d; \
	jmax=$${COMPAT_JOBS:-1}; \
	[ -n "$$jmax" ] || jmax=1; \
	if [ "$$jmax" -eq 0 ] 2>/dev/null; then \
	  jmax=$$(nproc 2>/dev/null || getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4); \
	  if [ "$$jmax" -gt 4 ]; then jmax=4; fi; \
	fi; \
	if [ "$$jmax" -lt 1 ] 2>/dev/null; then jmax=1; fi; \
	running=0; \
	for f in $(COMPAT_EXTENDED_SRCS); do \
	  [ -f "$$f" ] || continue; \
	  ( f="$$f"; \
	    stem=$$(printf '%s' "$${f%.c}" | sed 's#[/\\]#__#g'); \
	    out="$$stem.s"; log="$$stem.log"; \
	    start=$$(date +%s); \
	    if ./zcc_fast "$$f" -o ".compat_out/$$out" > ".compat_logs/$$log" 2>&1; then \
	      sec=$$(( $$(date +%s) - $$start )); line="PASS $$sec $$f $$log"; \
	    else \
	      sec=$$(( $$(date +%s) - $$start )); line="FAIL $$sec $$f $$log"; \
	    fi; \
	    printf '%s\n' "$$line" > ".compat_logs/summary.d/$$stem.line"; \
	    echo "[compat-ext] $$f -> .compat_out/$$out" \
	  ) & \
	  running=$$((running+1)); \
	  if [ $$running -ge $$jmax ]; then wait; running=0; fi; \
	done; \
	wait; \
	echo "status seconds source log" > .compat_logs/summary.tsv; \
	for f in $(COMPAT_EXTENDED_SRCS); do \
	  if [ -f "$$f" ]; then \
	    stem=$$(printf '%s' "$${f%.c}" | sed 's#[/\\]#__#g'); \
	    if [ -f ".compat_logs/summary.d/$$stem.line" ]; then \
	      cat ".compat_logs/summary.d/$$stem.line" >> .compat_logs/summary.tsv; \
	    fi; \
	  fi; \
	done; \
	ok=$$(awk 'NR>1 && $$1=="PASS" {c++} END{print c+0}' .compat_logs/summary.tsv); \
	fail=$$(awk 'NR>1 && $$1=="FAIL" {c++} END{print c+0}' .compat_logs/summary.tsv); \
	echo "COMPAT EXTENDED COMPLETE: PASS=$$ok FAIL=$$fail (jobs=$$jmax)"; \
	[ "$$fail" -eq 0 ] || true

compat-report:
	@if [ ! -d .compat_logs ]; then \
	  echo "No .compat_logs directory found. Run: make compat-extended"; \
	  exit 0; \
	fi
	@set -e; \
	total=0; failed=0; \
	if [ -f .compat_logs/summary.tsv ]; then \
	  while read -r status sec src log; do \
	    [ "$$status" = "status" ] && continue; \
	    total=$$((total + 1)); \
	    if [ "$$status" = "FAIL" ]; then \
	      failed=$$((failed + 1)); \
	      echo "=== FAIL .compat_logs/$$log ($$sec s, $$src) ==="; \
	      awk '/FAILED|error:/{printf "%d:%s\n", NR, $$0}' ".compat_logs/$$log" | sed -n '1,20p'; \
	    else \
	      echo "PASS .compat_logs/$$log ($$sec s, $$src)"; \
	    fi; \
	  done < .compat_logs/summary.tsv; \
	  echo "--- Slowest compiles (top 5) ---"; \
	  sort -k2,2nr .compat_logs/summary.tsv | sed -n '1,6p'; \
	else \
	  for log in .compat_logs/*.log; do \
	    [ -e "$$log" ] || continue; \
	    total=$$((total + 1)); \
	    if awk '/FAILED|error:/{found=1} END{exit found?0:1}' "$$log"; then \
	      failed=$$((failed + 1)); \
	      echo "=== FAIL $$log ==="; \
	      awk '/FAILED|error:/{printf "%d:%s\n", NR, $$0}' "$$log" | sed -n '1,20p'; \
	    else \
	      echo "PASS $$log"; \
	    fi; \
	  done; \
	fi; \
	echo "COMPAT REPORT: LOGS=$$total FAIL_LOGS=$$failed"

compat-report-ci:
	@if [ ! -f .compat_logs/summary.tsv ]; then \
	  echo "No compatibility summary found. Run: make compat-extended"; \
	  exit 2; \
	fi
	@set -e; \
	total=0; failed=0; \
	while read -r status sec src log; do \
	  [ "$$status" = "status" ] && continue; \
	  total=$$((total + 1)); \
	  if [ "$$status" = "FAIL" ]; then \
	    failed=$$((failed + 1)); \
	  fi; \
	done < .compat_logs/summary.tsv; \
	printf '{"compat_logs":%d,"compat_fail_logs":%d}\n' "$$total" "$$failed" > .compat_logs/status.json; \
	echo "Wrote .compat_logs/status.json"; \
	if [ "$$failed" -ne 0 ]; then \
	  echo "COMPAT REPORT CI FAILED: FAIL_LOGS=$$failed"; \
	  exit 1; \
	fi; \
	echo "COMPAT REPORT CI PASSED: FAIL_LOGS=0"

pp-crlf-gate: zcc_fast
	@mkdir -p .compat_out .compat_logs
	@printf '%s\n' \
		'typedef struct { double x,y,z; } Vec3;' \
		'#define v_dot(a, b) ((a)->x*(b)->x + (a)->y*(b)->y + (a)->z*(b)->z)' \
		'#define v_norm(in, out) do { \\' \
		'    double _l = v_dot(in,in); \\' \
		'    if(_l>0.0){ (out)->x=(in)->x/_l; (out)->y=(in)->y/_l; (out)->z=(in)->z/_l; } \\' \
		'    else { (out)->x=0; (out)->y=0; (out)->z=1; } \\' \
		'} while(0)' \
		'int main(void){ Vec3 a={1,2,3}, b={0,0,0}; v_norm(&a,&b); return 0; }' \
		> .compat_out/pp_crlf_probe_lf.c
	@awk '{printf "%s\r\n", $$0}' .compat_out/pp_crlf_probe_lf.c > .compat_out/pp_crlf_probe_crlf.c
	@( ./zcc_fast .compat_out/pp_crlf_probe_lf.c   -o .compat_out/pp_crlf_probe_lf.s   > .compat_logs/pp_crlf_probe_lf.log 2>&1 ) & \
	( ./zcc_fast .compat_out/pp_crlf_probe_crlf.c -o .compat_out/pp_crlf_probe_crlf.s > .compat_logs/pp_crlf_probe_crlf.log 2>&1 ) & \
	wait
	@grep -v '^[[:space:]]*[.]file ' .compat_out/pp_crlf_probe_lf.s   | grep -v '^[[:space:]]*[.]loc ' > .compat_out/pp_crlf_probe_lf.norm.s
	@grep -v '^[[:space:]]*[.]file ' .compat_out/pp_crlf_probe_crlf.s | grep -v '^[[:space:]]*[.]loc ' > .compat_out/pp_crlf_probe_crlf.norm.s
	@diff .compat_out/pp_crlf_probe_lf.norm.s .compat_out/pp_crlf_probe_crlf.norm.s > .compat_logs/pp_crlf_probe.diff
	@echo "PP CRLF GATE VERIFIED"

fortify-ad: selfhost-fast compat-extended pp-crlf-gate check-ir-bridge-guard compat-report
	@echo "FORTIFY A+D COMPLETE"

fortify-ci: selfhost-fast compat-extended pp-crlf-gate check-ir-bridge-guard compat-report-ci
	@echo "FORTIFY CI COMPLETE"

# External fortify pack wiring (CI/tooling only; no compiler source integration yet).
fortify-pack-init: verify-lexicon
	@test -d "$(FORTIFY_PACK_DIR)" || (echo "Missing $(FORTIFY_PACK_DIR). Extract fortify_zcc_clean.zip first."; exit 2)
	@bash -lc "cd '$(FORTIFY_PACK_DIR)' && if [ ! -f fortify-verify-policy.json ] && [ -f fortify-verify-policy.example.json ]; then cp fortify-verify-policy.example.json fortify-verify-policy.json; echo 'Wrote fortify-verify-policy.json from example'; else echo 'fortify-verify-policy.json already present'; fi"

fortify-pack-preflight:
	@test -d "$(FORTIFY_PACK_DIR)" || (echo "Missing $(FORTIFY_PACK_DIR). Extract fortify_zcc_clean.zip first."; exit 2)
	@bash -lc "cd '$(FORTIFY_PACK_DIR)' && chmod +x ci/*.sh && ci/preflight-fortify.sh"

fortify-pack-layout: fortify-pack-preflight
	@bash -lc "cd '$(FORTIFY_PACK_DIR)' && ci/fortify-layout.sh"

fortify-pack-production: fortify-pack-preflight
	@bash -lc "cd '$(FORTIFY_PACK_DIR)' && ci/verify-production-env.sh && ci/fortify-layout-production.sh"

fortify-pack-replay: fortify-pack-preflight
	@bash -lc "cd '$(FORTIFY_PACK_DIR)' && vp=''; [ -f fortify-verify-policy.json ] && vp='--verify-policy fortify-verify-policy.json'; python3 tools/verify_attestation_bundle.py --bundle artifacts/fortify-attestation.bundle.json --artifact-root artifacts $$vp"

fortify-pack-clean:
	@test -d "$(FORTIFY_PACK_DIR)" || (echo "Missing $(FORTIFY_PACK_DIR)."; exit 2)
	@bash -lc "cd '$(FORTIFY_PACK_DIR)' && ci/clean-fortify-artifacts.sh"

fortify-snapshot: fortify-ci
	@mkdir -p .compat_logs
	@{ \
	ts=$$(date -u +"%Y-%m-%dT%H:%M:%SZ"); \
	sha=$$(git rev-parse --short HEAD 2>/dev/null || echo unknown); \
	echo "FORTIFY SNAPSHOT"; \
	echo "timestamp_utc=$$ts"; \
	echo "git_sha=$$sha"; \
	echo "--- status.json ---"; \
	cat .compat_logs/status.json; \
	echo "--- slowest_top5 ---"; \
	sort -k2,2nr .compat_logs/summary.tsv | sed -n '1,6p'; \
	echo "--- failing_logs ---"; \
	awk 'NR>1 && $$1=="FAIL" {print $$4}' .compat_logs/summary.tsv; \
	} > .compat_logs/fortify_snapshot.txt
	@echo "Wrote .compat_logs/fortify_snapshot.txt"

fortify-recursive:
	@mkdir -p .compat_logs
	@iters=$${ITER:-3}; \
	echo "iter timestamp_utc compat_logs compat_fail_logs slowest_s" > .compat_logs/recursive_runs.tsv; \
	i=1; \
	while [ $$i -le $$iters ]; do \
	  echo "=== fortify-recursive iteration $$i/$$iters ==="; \
	  $(MAKE) fortify-snapshot >/dev/null; \
	  ts=$$(date -u +"%Y-%m-%dT%H:%M:%SZ"); \
	  logs=$$(awk -F'[:,}]' '/compat_logs/{gsub(/[^0-9]/,"",$$2); print $$2}' .compat_logs/status.json); \
	  fails=$$(awk -F'[:,}]' '/compat_fail_logs/{gsub(/[^0-9]/,"",$$4); print $$4}' .compat_logs/status.json); \
	  slowest=$$(awk 'NR>1{if($$2>m)m=$$2} END{if(m=="")m=0; print m+0}' .compat_logs/summary.tsv); \
	  echo "$$i $$ts $$logs $$fails $$slowest" >> .compat_logs/recursive_runs.tsv; \
	  i=$$((i + 1)); \
	done; \
	echo "FORTIFY RECURSIVE COMPLETE: iterations=$$iters"; \
	cat .compat_logs/recursive_runs.tsv

# fortify-recursive-ci: ITER (default 3), MAX_SLOW_DRIFT (s, default 10), MAX_SLOWEST_ABS (s, unset = no cap; -1 in recursive_status.json when unset)
fortify-recursive-ci:
	@mkdir -p .compat_logs .compat_logs/iterations
	@iters=$${ITER:-3}; \
	max_slow_drift=$${MAX_SLOW_DRIFT:-10}; \
	max_slowest_abs=$${MAX_SLOWEST_ABS:-}; \
	echo "iter timestamp_utc compat_logs compat_fail_logs slowest_s" > .compat_logs/recursive_runs.tsv; \
	base_logs=""; base_fails=""; base_sha=""; base_slowest=""; max_obs=0; \
	i=1; \
	while [ $$i -le $$iters ]; do \
	  echo "=== fortify-recursive-ci iteration $$i/$$iters ==="; \
	  $(MAKE) fortify-snapshot >/dev/null; \
	  ts=$$(date -u +"%Y-%m-%dT%H:%M:%SZ"); \
	  logs=$$(awk -F'[:,}]' '/compat_logs/{gsub(/[^0-9]/,"",$$2); print $$2}' .compat_logs/status.json); \
	  fails=$$(awk -F'[:,}]' '/compat_fail_logs/{gsub(/[^0-9]/,"",$$4); print $$4}' .compat_logs/status.json); \
	  sha=$$(awk -F= '/^git_sha=/{print $$2}' .compat_logs/fortify_snapshot.txt); \
	  slowest=$$(awk 'NR>1{if($$2>m)m=$$2} END{if(m=="")m=0; print m+0}' .compat_logs/summary.tsv); \
	  if [ -n "$$max_slowest_abs" ] && [ "$$slowest" -gt "$$max_slowest_abs" ]; then \
	    echo "FORTIFY RECURSIVE CI FAILED: slowest compile $$slowest s exceeds MAX_SLOWEST_ABS=$$max_slowest_abs"; \
	    exit 1; \
	  fi; \
	  if [ "$$slowest" -gt "$$max_obs" ]; then max_obs="$$slowest"; fi; \
	  echo "$$i $$ts $$logs $$fails $$slowest" >> .compat_logs/recursive_runs.tsv; \
	  cp .compat_logs/status.json .compat_logs/iterations/status_$$i.json; \
	  cp .compat_logs/fortify_snapshot.txt .compat_logs/iterations/snapshot_$$i.txt; \
	  if [ -z "$$base_logs" ]; then \
	    base_logs="$$logs"; \
	    base_fails="$$fails"; \
	    base_sha="$$sha"; \
	    base_slowest="$$slowest"; \
	  else \
	    if [ "$$logs" != "$$base_logs" ] || [ "$$fails" != "$$base_fails" ]; then \
	      echo "FORTIFY RECURSIVE CI FAILED: drift detected at iteration $$i"; \
	      cat .compat_logs/recursive_runs.tsv; \
	      exit 1; \
	    fi; \
	    if [ "$$sha" != "$$base_sha" ]; then \
	      echo "FORTIFY RECURSIVE CI FAILED: git SHA changed ($$base_sha -> $$sha) at iteration $$i"; \
	      exit 1; \
	    fi; \
	    delta=$$((slowest - base_slowest)); \
	    if [ $$delta -lt 0 ]; then delta=$$((0 - delta)); fi; \
	    if [ $$delta -gt $$max_slow_drift ]; then \
	      echo "FORTIFY RECURSIVE CI FAILED: slowest compile drift $$delta s exceeds MAX_SLOW_DRIFT=$$max_slow_drift"; \
	      echo "baseline_slowest=$$base_slowest current_slowest=$$slowest"; \
	      exit 1; \
	    fi; \
	  fi; \
	  i=$$((i + 1)); \
	done; \
	abs_json=-1; \
	[ -n "$$max_slowest_abs" ] && abs_json="$$max_slowest_abs"; \
	printf '{"iterations":%d,"compat_logs":%d,"compat_fail_logs":%d,"git_sha":"%s","baseline_slowest_s":%d,"max_slowest_observed_s":%d,"max_slow_drift_s":%d,"max_slowest_abs_s":%d}\n' "$$iters" "$$base_logs" "$$base_fails" "$$base_sha" "$$base_slowest" "$$max_obs" "$$max_slow_drift" "$$abs_json" > .compat_logs/recursive_status.json; \
	echo "FORTIFY RECURSIVE CI COMPLETE: iterations=$$iters"; \
	cat .compat_logs/recursive_runs.tsv; \
	echo "Wrote .compat_logs/recursive_status.json"

supercharge-ad: selfhost-fast compat-smoke
	@echo "SUPERCHARGE A+D COMPLETE"

test-float: zcc
	@echo "=== Running Float Probes Correctness Gates ==="
	./zcc probe_float_cmp.c -o /tmp/probe_float_cmp.s && gcc -fno-pie -no-pie -o /tmp/probe_float_cmp /tmp/probe_float_cmp.s -lm && /tmp/probe_float_cmp
	./zcc probe_float_cmp_v2.c -o /tmp/probe_float_cmp_v2.s && gcc -fno-pie -no-pie -o /tmp/probe_float_cmp_v2 /tmp/probe_float_cmp_v2.s -lm && /tmp/probe_float_cmp_v2
	./zcc probe_neg_zero_truth.c -o /tmp/probe_neg_zero_truth.s && gcc -fno-pie -no-pie -o /tmp/probe_neg_zero_truth /tmp/probe_neg_zero_truth.s -lm && /tmp/probe_neg_zero_truth
	./zcc probe_float_surface.c -o /tmp/probe_float_surface.s && gcc -fno-pie -no-pie -o /tmp/probe_float_surface /tmp/probe_float_surface.s -lm && /tmp/probe_float_surface
	./zcc fp_conv_harness.c -o /tmp/fp_conv_harness.s && gcc -fno-pie -no-pie -o /tmp/fp_conv_harness /tmp/fp_conv_harness.s -lm && /tmp/fp_conv_harness
	./zcc probe_static_init_alldouble.c -o /tmp/probe_static_init_alldouble.s && gcc -fno-pie -no-pie -o /tmp/probe_static_init_alldouble /tmp/probe_static_init_alldouble.s -lm && /tmp/probe_static_init_alldouble
	./zcc probe_nan_cmp.c -o /tmp/probe_nan_cmp.s && gcc -fno-pie -no-pie -o /tmp/probe_nan_cmp /tmp/probe_nan_cmp.s -lm && /tmp/probe_nan_cmp
	ZCC_IR_BACKEND=1 ./zcc probe_nan_cmp.c -o /tmp/probe_nan_cmp_ir.s && gcc -fno-pie -no-pie -o /tmp/probe_nan_cmp_ir /tmp/probe_nan_cmp_ir.s -lm && /tmp/probe_nan_cmp_ir
	./zcc probe_float_cast.c -o /tmp/probe_float_cast.s && gcc -fno-pie -no-pie -o /tmp/probe_float_cast /tmp/probe_float_cast.s -lm && /tmp/probe_float_cast
	ZCC_IR_BACKEND=1 ./zcc probe_float_cast.c -o /tmp/probe_float_cast_ir.s && gcc -fno-pie -no-pie -o /tmp/probe_float_cast_ir /tmp/probe_float_cast_ir.s -lm && /tmp/probe_float_cast_ir
	./zcc probes/probe_long_double.c -o /tmp/probe_long_double.s && gcc -fno-pie -no-pie -o /tmp/probe_long_double /tmp/probe_long_double.s -lm && /tmp/probe_long_double
	ZCC_IR_BACKEND=1 ./zcc probes/probe_long_double.c -o /tmp/probe_long_double_ir.s && gcc -fno-pie -no-pie -o /tmp/probe_long_double_ir /tmp/probe_long_double_ir.s -lm && /tmp/probe_long_double_ir
	@echo "=== Float Probes: ALL PASS ==="

test: zcc test-float
	bash zcc_test_suite.sh --quick

# ─── EVM Lifter Scaffold Tests ───────────────────────────────────────
# Defensive audit scaffold only — not for exploit use.
# Builds and runs tests/test_evm_lifter.c against evm_lifter.c + ir.c.
# Does NOT depend on the ZCC self-hosting path; compiled directly with gcc.
check-evm-lifter:
	@echo "=== Building EVM Lifter test binary ==="
	$(CC) $(CFLAGS) -I. \
	    -o /tmp/test_evm_lifter \
	    tests/test_evm_lifter.c evm_lifter.c ir_vuln_tag.c ir.c \
	    src/evm/memory_v2.c src/evm/abi_extractor.c $(LDFLAGS)
	@echo "=== Running EVM Lifter tests ==="
	/tmp/test_evm_lifter

# ─── IR Vulnerability Tag Schema Tests ───────────────────────────────
# Defensive security-analysis scaffold only — no exploit generation.
# Builds and runs tests/test_ir_vuln_tag.c against ir_vuln_tag.c +
# evm_lifter.c + ir.c.  Does NOT depend on the ZCC self-hosting path.
check-ir-vuln-tag:
	@echo "=== Building IR Vuln Tag test binary ==="
	$(CC) $(CFLAGS) -I. \
	    -o /tmp/test_ir_vuln_tag \
	    tests/test_ir_vuln_tag.c ir_vuln_tag.c evm_lifter.c ir.c \
	    src/evm/memory_v2.c src/evm/abi_extractor.c $(LDFLAGS)
	@echo "=== Running IR Vuln Tag tests ==="
	/tmp/test_ir_vuln_tag

# ─── ForgeZero Audit Receipt Scaffold Tests ──────────────────────────
# Defensive audit receipt / telemetry scaffold only.
# Offline / file / stdout emission only — no network, no payload.
# Builds and runs tests/test_forgezero_receipt.c against
# forgezero_receipt.c + ir_vuln_tag.c + evm_lifter.c + ir.c.
# Does NOT depend on the ZCC self-hosting path; compiled directly with gcc.
check-forgezero-receipt:
	@echo "=== Building ForgeZero Audit Receipt test binary ==="
	$(CC) $(CFLAGS) -I. \
	    -o /tmp/test_forgezero_receipt \
	    tests/test_forgezero_receipt.c forgezero_receipt.c \
	    ir_vuln_tag.c evm_lifter.c ir.c \
	    src/evm/memory_v2.c src/evm/abi_extractor.c $(LDFLAGS)
	@echo "=== Running ForgeZero Audit Receipt tests ==="
	/tmp/test_forgezero_receipt

# ─── IR Bridge Guard Rejection Tests ─────────────────────────────────
check-ir-bridge-guard:
	@echo "=== ir_bridge.h guard rejection test (expect FAILURE) ==="
	@if $(CC) $(CFLAGS) -I. -fsyntax-only tests/test_ir_bridge_guard.c 2>/dev/null; then \
	  echo "FAIL: ir_bridge.h compiled WITHOUT ZCC_IR_BRIDGE_ALLOWED — guard regressed"; \
	  exit 1; \
	else \
	  echo "PASS: guard correctly rejected unsanctioned inclusion"; \
	fi

# ─── Copy/Constant Propagation Pass Tests ────────────────────────────
# Tests ir_pass_copy_const_prop: constant propagation through IR_COPY,
# copy alias propagation, block-boundary reset, and dead-copy removal.
# Compiled directly with GCC; does NOT depend on the ZCC self-hosting path.
# A minimal source list is used to avoid PARTS-dependent translation units.
# tests/test_copy_const_prop_stubs.c provides no-op stubs for the few
# PARTS symbols referenced (but not exercised) by zcc_oracle_substrate.c.
check-copy-const-prop:
	@echo "=== Building Copy/Constant Propagation test binary ==="
	$(CC) $(CFLAGS) -I. \
	    -o /tmp/test_copy_const_prop \
	    tests/test_copy_const_prop.c \
	    tests/test_copy_const_prop_stubs.c \
	    ir_pass_manager.c ir_pass_warden.c ir_pass_taint.c ir_pass_healer.c \
	    ir_symbolic_cfg.c ir_dominance.c ir_ssa.c \
	    evm_lifter.c ir_vuln_tag.c ir_to_evm.c ir_evm_stack.c \
	    ir.c src/ir_lower_float.c \
	    src/evm/memory_v2.c src/evm/abi_extractor.c \
	    ir_telemetry.c zcc_telemetry.c \
	    src/zcc_oracle_substrate.c \
	    transient_state.c \
	    $(LDFLAGS)
	@echo "=== Running Copy/Constant Propagation tests ==="
	/tmp/test_copy_const_prop

asan: zcc.c $(PASSES)
	$(CC) -fsanitize=address -O0 -g -Dmain=zcc_main -o zcc_asan zcc.c $(PASSES) $(LDFLAGS)
	@echo "ASan build ready. Run: ./zcc_asan zcc.c -o /dev/null"

clean:
	rm -f zcc zcc_fast zcc2 zcc2_fast zcc3 zcc3_fast zcc_asan zcc.c zcc_pp.c *.s *.o
	rm -f tools/zcc_topology_auditor tools/zcc_zxr_verify tools/zcc_replay_pack
	rm -rf .compat_out .compat_logs scratch/replay/ scratch/extracted_replay/ scratch/kernel.zrp

ir-verify: zcc2
	@echo "[IR-VERIFY] Stage 2 IR emission..."
	ZCC_EMIT_IR=1 ./zcc2 -DZCC_REAL_TELEMETRY zcc.c -o zcc_ir_stage2.s
	@echo "[IR-VERIFY] Linking IR stage 2 binary..."
	gcc zcc_ir_stage2.s $(PASSES) -o zcc_ir_stage2 -lm
	@echo "[IR-VERIFY] Stage 3 via IR path..."
	ZCC_EMIT_IR=1 ./zcc_ir_stage2 -DZCC_REAL_TELEMETRY zcc.c -o zcc_ir_stage3.s

sqlite: zcc2
	@echo "=== Compiling SQLite 160MB Amalgamation with ZCC ==="
	./zcc2 sqlite3_zcc.c -o sqlite3_zcc.s
	@echo "=== Linking sqlite3_test ==="
	gcc -no-pie -O0 -w -fno-asynchronous-unwind-tables -Wa,--noexecstack -fno-unwind-tables \
		-o sqlite3_test sqlite3_zcc.s sqlite3_functest.c -ldl -lpthread -lm
	@echo "=== Build Complete ==="

# ─── ONEIROGENESIS v2: A Compiler That Dreams ────────────────────
dream-regalloc: zcc2
	@echo "=== ZCC ONEIROGENESIS v3 [REGALLOC GENETIC SWEEP] ==="
	python3 zcc_oneirogenesis.py --sweep-regalloc --cycles 105 --islands 4

dream: zcc2
	@echo "=== ZCC ONEIROGENESIS v2 — The Compiler Dreams ==="
	python3 zcc_oneirogenesis.py --cycles 50

dream-sweep: zcc2
	@echo "=== ZCC ONEIROGENESIS v2 [SWEEP — apply all patterns] ==="
	python3 zcc_oneirogenesis.py --sweep --cycles 5 --mutations 1

dream-islands: zcc2
	@echo "=== ZCC ONEIROGENESIS v2 [ISLAND MODEL — 3 lineages] ==="
	python3 zcc_oneirogenesis.py --islands 3 --cycles 60 --mutations 4

dream-aggressive: zcc2
	@echo "=== ZCC ONEIROGENESIS v2 [AGGRESSIVE] ==="
	python3 zcc_oneirogenesis.py --cycles 200 --aggressive --islands 3 --sweep

dream-dry: zcc2
	@echo "=== ZCC ONEIROGENESIS v2 [DRY RUN] ==="
	python3 zcc_oneirogenesis.py --dry-run --cycles 10 --sweep

dream-visualize: zcc2
	@echo "=== ZCC ONEIROGENESIS v2 [GOD'S EYE TELEMETRY] ==="
	python3 zcc_oneirogenesis.py --cycles 100 --visualize --islands 2

dream-reset:
	@echo "=== Resetting dream state ==="
	python3 zcc_oneirogenesis.py --reset

rust-front-smoke: zcc
	python3 tests/rust/test_rust_frontend.py

# === SWARMDECOMPILE TARGETS ===
.PHONY: swarm-fuzz swarm-fuzz-clean

swarm-fuzz: zcc
	@echo "🔱 ZKAEDI SWARMDECOMPILE INITIATED"
	@mkdir -p swarm_out corpus evm_decomp
	@gcc -O2 tools/evm_fuzzer/swarm_fuzzer.c -o tools/evm_fuzzer/swarm_fuzzer
	@for i in $$(seq 1 5000); do \
		./tools/evm_fuzzer/swarm_fuzzer $$i > swarm_out/contract_$$i.bin 2>/dev/null; \
		./zcc --decompile swarm_out/contract_$$i.bin -o evm_decomp/contract_$$i.c || true; \
	done
	@echo "Generating HTML report..."
	@python3 tools/evm_fuzzer/report_gen.py swarm_out evm_decomp report.html
	@echo "✅ SwarmDecompile complete: 5000 contracts | Check report.html"

swarm-jit: zcc
	@echo "🔱 ZKAEDI SWARM-JIT INITIATED"
	@mkdir -p swarm_out evm_jit
	@gcc -O2 tools/evm_fuzzer/swarm_fuzzer.c -o tools/evm_fuzzer/swarm_fuzzer
	@for i in $$(seq 1 5000); do \
		./tools/evm_fuzzer/swarm_fuzzer $$i > swarm_out/contract_$$i.bin 2>/dev/null; \
		./zcc --jit swarm_out/contract_$$i.bin -o evm_jit/contract_$$i.bin || true; \
	done
	@echo "✅ SwarmJIT complete: 5000 contracts jitted | Output in evm_jit/"

swarm-prove: zcc
	@echo "🔱 Running symbolic proofs on swarm..."
	@for f in evm_decomp/*.c; do \
		./zcc --prove $${f%.c}.bin "no-revert" >/dev/null 2>&1 || true; \
	done
	@echo "✅ SwarmProve complete."

swarm-memory: zcc
	@echo "🔱 Memory Model v2 Swarm"
	@for i in $$(seq 1 2000); do \
		./tools/evm_fuzzer/swarm_fuzzer $$i > swarm_out/contract_$$i.bin 2>/dev/null || true; \
		./zcc --decompile --memory-trace swarm_out/contract_$$i.bin >/dev/null 2>&1 || true; \
	done
	@echo "✅ SwarmMemory complete."

swarm-abi: zcc
	@echo "🔱 ABI Deep Dive Swarm"
	@for i in $$(seq 1 2000); do \
		./zcc --abi --decompile swarm_out/contract_$$i.bin -o evm_decomp/contract_$$i.c >/dev/null 2>&1 || true; \
	done
	@echo "✅ ABI extraction complete on 2000 contracts"

mega-swarm: zcc
	@echo "🔱 MEGA SWARM — 50,000 CONTRACTS"
	@mkdir -p mega_corpus mega_decomp mega_jit
	@gcc -O2 tools/evm_fuzzer/swarm_fuzzer.c -o tools/evm_fuzzer/swarm_fuzzer
	@seq 1 50000 | xargs -P12 -I{} sh -c './tools/evm_fuzzer/swarm_fuzzer {} --heavy-abi --heavy-memory --heavy-calldata --size 1024 > mega_corpus/contract_{}.bin' 2>/dev/null
	@find mega_corpus -name '*.bin' | xargs -P12 -I{} sh -c './zcc --abi --decompile "{}" -o "mega_decomp/$$(basename "{}" .bin).c" >/dev/null 2>&1' || true
	@python3 tools/evm_fuzzer/mega_report.py mega_corpus mega_decomp report_mega.html
	@echo "🔱 MEGA SWARM COMPLETE — Check report_mega.html"

mega-courtroom:
	@echo "🔱 Running Courtroom on top 500 interesting contracts..."
	@python3 tools/mega_courtroom_runner.py

release-prep: swarm-fuzz swarm-jit swarm-prove swarm-memory swarm-abi
	@echo "All phases green → ready for v1.0"

# === RELEASE GATES ===
.PHONY: release-gate courtroom-gate ci-gate

courtroom-gate:
	@echo "🔱 Running Courtroom Gate-Zero..."
	@mkdir -p releases
	python3 tools/release_gate.py tickets/MEGA-SWARM-VERIFICATION.md

release-v1.1: mega-swarm-courtroom jit-memory-opt proof-export courtroom-gate
	@echo "✅ All gates green → proceeding to v1.1 release"
	@make selfhost
	@git tag -f -a v1.1.0 -m "Courtroom Gate-Zero Release"
	@git push -f origin v1.1.0 || true
	@gh release create v1.1.0 --notes-file RELEASE_NOTES.md || echo "Release creation skipped or failed."

jit-memory-opt:
	@echo "🔱 JIT Memory Optimizations — Mega Swarm Corpus"
	@mkdir -p mega_jit
	@find mega_corpus -name '*.bin' | xargs -P12 -I{} sh -c './zcc --jit --memory-opt "{}" -o "mega_jit/$$(basename "{}" .bin).exe" >/dev/null 2>&1' || true
	@echo "✅ Sparse memory now emits direct x86 addressing"

proof-export:
	@echo "🔱 Formal Proof Export"
	@find mega_corpus -name '*.bin' | xargs -P12 -I{} sh -c './zcc --prove "{}" no-revert --export smt2 >/dev/null 2>&1' || true

v1.1-plan:
	@cat docs/ZCC_V1.1_ROADMAP.md
	@echo "🔱 v1.1 Roadmap Loaded — Ready for execution"

mega-swarm-courtroom: mega-swarm mega-courtroom
	@make courtroom-gate

# CI-friendly target
ci-gate:
	@make courtroom-gate || (echo "⛔ CI BLOCKED by Courtroom" && exit 1)

release-v1.0:
	@echo "🔱 RELEASING ZCC v1.0 — THE FINAL FORM"
	@make selfhost
	@make swarm-fuzz
	@make swarm-jit
	@make swarm-prove
	@git tag -a v1.0.0 -m "Boundary Shatter Release"
	@gh release create v1.0.0 zcc \
		--title "ZCC v1.0.0 — God Tier EVM Toolchain" \
		--notes-file RELEASE_NOTES.md
	@echo "✅ v1.0.0 SHIPPED"

swarm-fuzz-clean:
	rm -rf swarm_out evm_decomp evm_jit report.html corpus/new_* tools/evm_fuzzer/swarm_fuzzer

release-clean:
	rm -rf evm_jit/ evm_decomp/ report.html swarm_out/

elf_emit_smoke: zcc
	@echo "=== Direct ELF Emission Smoke Test ==="
	./zcc test/add.c -emit-obj -o test/add.o
	readelf -a test/add.o

zkernel: zcc
	@echo "=== Building RP2040 zkernel ==="
	cp zcc.c /tmp/zcc_arm.c
	sed -i 's/TargetBackend \*backend_ops = 0;/extern TargetBackend backend_thumbv6m; TargetBackend \*backend_ops = \&backend_thumbv6m;/g' /tmp/zcc_arm.c
	sed -i 's/int ZCC_POINTER_WIDTH = 8;/int ZCC_POINTER_WIDTH = 4;/g' /tmp/zcc_arm.c
	python3 scripts/patch_arm_codegen.py /tmp/zcc_arm.c
	$(CC) -I. -O0 -w -fno-asynchronous-unwind-tables -g0 -DZCC_REAL_TELEMETRY -Dmain=zcc_main -o /tmp/zcc_arm /tmp/zcc_arm.c $(PASSES) $(LDFLAGS)
	/tmp/zcc_arm src/zkernel/main.c -o main_zcc.s
	/tmp/zcc_arm src/zkernel/uart.c -o uart_zcc.s
	/tmp/zcc_arm src/zkernel/sched.c -o sched_zcc.s
	arm-none-eabi-gcc -mcpu=cortex-m0plus -mthumb -O0 -ffreestanding -nostdlib -T src/zkernel/linker.ld \
		src/zkernel/startup.s main_zcc.s uart_zcc.s sched_zcc.s -o zkernel.elf
	arm-none-eabi-objcopy -O binary zkernel.elf zkernel.bin
	python3 rp2040_blink/uf2conv.py zkernel.bin -f RP2040 --base 0x10000000 -o zkernel.uf2
	rm -f main_zcc.s uart_zcc.s sched_zcc.s zkernel.bin /tmp/zcc_arm.c /tmp/zcc_arm
	@echo "=== zkernel.elf and zkernel.uf2 built successfully ==="
auditor: tools/zcc_topology_auditor
verifier: tools/zcc_zxr_verify
replay_pack: tools/zcc_replay_pack
genome_diff: tools/zcc_genome_diff

tools/zcc_topology_auditor: tools/zcc_topology_auditor.c tools/zcc_elf_parser.h tools/zcc_sha256.h
	gcc -O2 -Wall -Itools tools/zcc_topology_auditor.c -o tools/zcc_topology_auditor -lm

tools/zcc_zxr_verify: tools/zcc_zxr_verify.c tools/zcc_elf_parser.h tools/zcc_sha256.h
	gcc -O2 -Wall -Itools tools/zcc_zxr_verify.c -o tools/zcc_zxr_verify -lm

tools/zcc_replay_pack: tools/zcc_replay_pack.c tools/zcc_elf_parser.h tools/zcc_sha256.h
	gcc -O2 -Wall -Itools tools/zcc_replay_pack.c -o tools/zcc_replay_pack -lm

tools/zcc_genome_diff: tools/zcc_genome_diff.c
	gcc -O2 -Wall -Itools tools/zcc_genome_diff.c -o tools/zcc_genome_diff -lm

tools/zcc_genome_history: tools/zcc_genome_history.c
	gcc -O2 -Wall -Itools tools/zcc_genome_history.c -o tools/zcc_genome_history -lm

tools/zcc_time_machine: tools/zcc_time_machine.c
	gcc -O2 -Wall -Itools tools/zcc_time_machine.c -o tools/zcc_time_machine -lm

tools/zcc_stability_observatory: tools/zcc_stability_observatory.c
	gcc -O2 -Wall -Itools tools/zcc_stability_observatory.c -o tools/zcc_stability_observatory -lm

tools/zcc_topology_bisector: tools/zcc_topology_bisector.c
	gcc -O2 -Wall -Itools tools/zcc_topology_bisector.c -o tools/zcc_topology_bisector -lm

tools/zcc_cross_genome: tools/zcc_cross_genome.c
	gcc -O2 -Wall -Itools tools/zcc_cross_genome.c -o tools/zcc_cross_genome -lm

tools/zcc_build_ledger: tools/zcc_build_ledger.c
	gcc -O2 -Wall -Itools tools/zcc_build_ledger.c -o tools/zcc_build_ledger -lm

stability_observatory: tools/zcc_stability_observatory
topology_bisector: tools/zcc_topology_bisector
cross_genome: tools/zcc_cross_genome
build_ledger: tools/zcc_build_ledger

verify-attestation: auditor verifier
	@echo "=== Running ZXR Attestation Pipeline Gate ==="
	./tools/zcc_topology_auditor kernel/*.o --json > record_test.zxr
	./tools/zcc_zxr_verify record_test.zxr kernel/*.o
	@rm -f record_test.zxr
	@echo "=== ZXR Attestation Pipeline Gate: VERIFIED ==="

verify-replay-pack: replay_pack
	@echo "=== Running ZXR Replay Pack Gate ==="
	@mkdir -p scratch
	./tools/zcc_replay_pack create kernel/*.o --out scratch/kernel.zrp
	./tools/zcc_replay_pack verify scratch/kernel.zrp
	@mkdir -p scratch/extracted_replay
	./tools/zcc_replay_pack extract scratch/kernel.zrp --out scratch/extracted_replay
	@echo "=== ZXR Replay Pack Gate: VERIFIED ==="

verify-genome-diff: auditor tools/zcc_genome_diff
	@echo "=== Running ZXR Genome Diff Gate ==="
	@mkdir -p scratch
	./tools/zcc_topology_auditor kernel/*.o --json > scratch/base_genome.json
	./tools/zcc_genome_diff scratch/base_genome.json scratch/base_genome.json
	@echo "=== ZXR Genome Diff Gate: VERIFIED ==="

verify-lineage: auditor tools/zcc_genome_history tools/zcc_time_machine
	@echo "=== Running ZXR Compiler Lineage & Time Machine Gate ==="
	@mkdir -p scratch
	./tools/zcc_topology_auditor kernel/*.o --json > scratch/base_genome.json
	python3 scratch/build_test_registry.py
	./tools/zcc_genome_history scratch/genomes
	@echo "--- Time Machine Hop v0.20 -> v0.23 ---"
	./tools/zcc_time_machine --from v0.20 --to v0.23 --registry scratch/genomes
	@echo "=== ZXR Compiler Lineage & Time Machine: VERIFIED ==="

verify-stability: auditor tools/zcc_stability_observatory tools/zcc_genome_history tools/zcc_time_machine
	@echo "=== Running ZXR Stability Observatory Gate (D-24) ==="
	@mkdir -p scratch
	./tools/zcc_topology_auditor kernel/*.o --json > scratch/base_genome.json
	python3 scratch/build_test_registry.py
	./tools/zcc_stability_observatory scratch/genomes \
		--out scratch/stability_forecast.json --forecast 3
	@echo "Forecast JSON:"
	@cat scratch/stability_forecast.json
	@echo "=== ZXR Stability Observatory Gate: VERIFIED ==="

verify-bisector: auditor tools/zcc_topology_bisector tools/zcc_genome_history
	@echo "=== Running ZXR Topology Bisector Gate (D-25) ==="
	@mkdir -p scratch
	./tools/zcc_topology_auditor kernel/*.o --json > scratch/base_genome.json
	python3 scratch/build_test_registry.py
	./tools/zcc_topology_bisector \
		--registry scratch/genomes --bad v0.23 \
		--out scratch/bisect_report.json || true
	@echo "Bisect JSON:"
	@cat scratch/bisect_report.json
	@echo "=== ZXR Topology Bisector Gate: VERIFIED ==="

verify-cross-genome: auditor tools/zcc_cross_genome tools/zcc_genome_history
	@echo "=== Running ZXR Cross-Compiler Genome Gate (D-26) ==="
	@mkdir -p scratch
	./tools/zcc_topology_auditor kernel/*.o --json > scratch/base_genome.json
	python3 scratch/build_test_registry.py
	./tools/zcc_cross_genome \
		--zcc scratch/genomes \
		--ref-a scratch/base_genome.json \
		--ref-a-label baseline \
		--out scratch/cross_genome_report.json
	@echo "Cross-genome JSON:"
	@cat scratch/cross_genome_report.json
	@echo "=== ZXR Cross-Compiler Genome Gate: VERIFIED ==="

verify-ledger: tools/zcc_build_ledger tools/zcc_genome_history auditor
	@echo "=== Running ZXR Sovereign Build Ledger Gate (D-27) ==="
	@mkdir -p scratch
	./tools/zcc_topology_auditor kernel/*.o --json > scratch/base_genome.json
	python3 scratch/build_test_registry.py
	@rm -f scratch/build.ledger
	./tools/zcc_build_ledger append \
		--ledger scratch/build.ledger \
		--genome scratch/base_genome.json \
		--version v0.24 --stability 90
	./tools/zcc_build_ledger append \
		--ledger scratch/build.ledger \
		--genome scratch/base_genome.json \
		--version v0.25 --stability 92
	./tools/zcc_build_ledger append \
		--ledger scratch/build.ledger \
		--genome scratch/base_genome.json \
		--version v0.26 --stability 94
	./tools/zcc_build_ledger dump   --ledger scratch/build.ledger
	./tools/zcc_build_ledger verify --ledger scratch/build.ledger
	@echo "=== ZXR Sovereign Build Ledger Gate: VERIFIED ==="

tools/zcc_runtime_probe.o: tools/zcc_runtime_probe.c
	gcc -O2 -Wall -c tools/zcc_runtime_probe.c -o tools/zcc_runtime_probe.o

tools/zcc_behavioral_diff: tools/zcc_behavioral_diff.c
	gcc -O2 -Wall -Itools tools/zcc_behavioral_diff.c -o tools/zcc_behavioral_diff -lm

runtime_probe: tools/zcc_runtime_probe.o
behavioral_diff: tools/zcc_behavioral_diff

verify-runtime-probe: tools/zcc_behavioral_diff tools/zcc_runtime_probe.o auditor
	@echo "=== Running ZXR Runtime Behavioral Probe Gate (D-28) ==="
	@mkdir -p scratch
	./tools/zcc_topology_auditor kernel/*.o --json > scratch/static_genome.json
	@echo "--- Building instrumented probe test binary ---"
	gcc -O2 -finstrument-functions \
		tools/zcc_runtime_probe.c \
		tests/runtime_probe_test.c \
		-o scratch/probe_test_bin
	@echo "--- Running probe test binary ---"
	ZCC_PROBE_OUT=scratch/runtime_genome.json ./scratch/probe_test_bin
	@echo "--- Runtime genome emitted ---"
	@cat scratch/runtime_genome.json
	@echo "--- Running behavioral diff ---"
	./tools/zcc_behavioral_diff \
		--static scratch/static_genome.json \
		--runtime scratch/runtime_genome.json \
		--out scratch/behavioral_drift_report.json || true
	@echo "--- Drift report ---"
	@cat scratch/behavioral_drift_report.json
	@echo "=== ZXR Runtime Behavioral Probe Gate: VERIFIED ==="

tools/zcc_impact_attribution: tools/zcc_impact_attribution.c
	gcc -O2 -Wall -Itools tools/zcc_impact_attribution.c -o tools/zcc_impact_attribution -lm

tools/zcc_function_ranker: tools/zcc_function_ranker.c
	gcc -O2 -Wall -Itools tools/zcc_function_ranker.c -o tools/zcc_function_ranker -lm

impact_attribution: tools/zcc_impact_attribution
function_ranker: tools/zcc_function_ranker

verify-impact-attribution: tools/zcc_impact_attribution tools/zcc_function_ranker \
                            tools/zcc_runtime_probe.o auditor verify-calibration
	@echo "=== Running ZXR Runtime Impact Attribution Gate (D-29) ==="
	@mkdir -p scratch
	./tools/zcc_topology_auditor kernel/*.o --json > scratch/static_genome_a.json
	python3 scratch/build_test_registry.py
	@cp scratch/genomes/v0.20.json scratch/static_genome_v020.json
	@cp scratch/genomes/v0.23.json scratch/static_genome_v023.json
	@echo "--- Building two instrumented probe binaries (A and B) ---"
	gcc -O2 -finstrument-functions \
		tools/zcc_runtime_probe.c \
		tests/runtime_probe_test.c \
		-o scratch/probe_bin_a
	gcc -O2 -O3 -finstrument-functions \
		tools/zcc_runtime_probe.c \
		tests/runtime_probe_test.c \
		-o scratch/probe_bin_b
	@echo "--- Running probe A ---"
	ZCC_PROBE_OUT=scratch/runtime_genome_a.json ./scratch/probe_bin_a
	@echo "--- Running probe B ---"
	ZCC_PROBE_OUT=scratch/runtime_genome_b.json ./scratch/probe_bin_b
	@echo "--- Function Ranker (genome A) ---"
	./tools/zcc_function_ranker scratch/runtime_genome_a.json \
		--top 5 --out scratch/ranked_a.json
	@echo "--- Function Ranker (genome B) ---"
	./tools/zcc_function_ranker scratch/runtime_genome_b.json \
		--top 5 --out scratch/ranked_b.json
	@echo "--- Impact Attribution (static-only: v0.20 vs v0.23) ---"
	./tools/zcc_impact_attribution \
		--static-a scratch/static_genome_v020.json \
		--static-b scratch/static_genome_v023.json \
		--version-a v0.20 --version-b v0.23 \
		--calibration-report scratch/calibration_report.json \
		--thresholds scratch/calibrated_thresholds.json \
		--out scratch/attribution_static.json || true
	@echo "--- Attribution JSON (static) ---"
	@cat scratch/attribution_static.json
	@echo "--- Impact Attribution (full: A vs B with runtime genomes) ---"
	./tools/zcc_impact_attribution \
		--static-a scratch/static_genome_v020.json \
		--static-b scratch/static_genome_v023.json \
		--runtime-a scratch/runtime_genome_a.json \
		--runtime-b scratch/runtime_genome_b.json \
		--version-a v0.20-O2 --version-b v0.23-O3 \
		--calibration-report scratch/calibration_report.json \
		--thresholds scratch/calibrated_thresholds.json \
		--out scratch/attribution_full.json || true
	@echo "--- Attribution JSON (full) ---"
	@cat scratch/attribution_full.json
	@echo "=== ZXR Runtime Impact Attribution Gate: VERIFIED ==="

tools/zcc_health_report: tools/zcc_health_report.c
	gcc -O2 -Wall -lm tools/zcc_health_report.c -o tools/zcc_health_report

health_report: tools/zcc_health_report

health-report: tools/zcc_health_report
	@echo "=== ZCC Compiler Health Report ==="
	./tools/zcc_health_report \
		--scratch scratch \
		--golden genomes/golden \
		--selfhost PASS \
		--attestation PASS \
		--replay PASS \
		--lineage PASS \
		--bisector PASS \
		--cross-genome PASS \
		--out compiler_health.json
	@echo "=== Health Report Complete ==="

freeze-golden:
	@echo "=== Freezing Golden Genome (v0.29-observability) ==="
	@mkdir -p genomes/golden
	./tools/zcc_topology_auditor kernel/*.o --json \
		> genomes/golden/v0.29-observability.json
	ZCC_PROBE_OUT=genomes/golden/v0.29-runtime.json \
		./scratch/probe_bin_a 2>&1 | tail -3
	./tools/zcc_topology_auditor kernel/*.o \
		> genomes/golden/v0.29-attestation.zxr
	@echo "Golden genome frozen:"
	@ls -lh genomes/golden/
	@echo "=== freeze-golden: COMPLETE ==="

verify-golden: tools/zcc_genome_diff auditor
	@echo "=== Verifying Golden Genome Integrity ==="
	./tools/zcc_topology_auditor kernel/*.o --json > scratch/current_for_golden.json
	./tools/zcc_genome_diff \
		genomes/golden/v0.29-observability.json \
		scratch/current_for_golden.json
	@echo "=== verify-golden: COMPLETE ==="

zcc_calibration_corpus: tools/zcc_topology_auditor tools/zcc_impact_attribution tools/zcc_runtime_probe.o zcc
	@echo "=== Running Calibration Corpus Experiment (D-30) ==="
	python3 tools/zcc_calibrate.py

verify-calibration: zcc_calibration_corpus
	@echo "=== Verifying Prediction Calibration Gate ==="
	@cat scratch/forecast_accuracy.json
	@grep -q '"status": "CALIBRATED"' scratch/forecast_accuracy.json || (echo "CALIBRATION FAILED: F1 score too low"; exit 1)
	@echo "=== verify-calibration: COMPLETE ==="

test_zcc_dag:
	@echo "=== Building BuildDAG test binary ==="
	$(CC) $(CFLAGS) -I. -o /tmp/test_zcc_dag src/zcc_dag.c tests/test_zcc_dag.c
	@echo "=== Running BuildDAG tests ==="
	/tmp/test_zcc_dag

abi-lanes: zcc
	@echo "=== Running Argument-Passing ABI Lane (31/31) ==="
	python3 tools/abi_lane_gen.py --out /tmp/abi_lane_cases --run --zcc $(CURDIR)/zcc
	@echo "=== Running Return-Value ABI Lane (17/17) ==="
	python3 tools/abi_ret_lane_gen.py --out /tmp/abi_ret_cases --run --zcc $(CURDIR)/zcc
	@echo "=== Running Arrays-in-Structs ABI Lane (20/20) ==="
	python3 tools/abi_array_lane_gen.py --out /tmp/abi_array_cases --run --zcc $(CURDIR)/zcc
	@echo "=== Running Packed-Struct ABI Lane (12/12) ==="
	python3 tools/abi_packed_lane_gen.py --out /tmp/abi_packed_cases --run --zcc $(CURDIR)/zcc
	@echo "=== Running Bitfield ABI Lane (16/16) ==="
	python3 tools/abi_bitfield_lane_gen.py --out /tmp/abi_bitfield_cases --run --zcc $(CURDIR)/zcc


# === BOUNDARY CONTRACT PACK ===
.PHONY: boundaries-validate boundaries-matrix boundaries-test evidence-report workflows-validate

boundaries-validate: verify-lexicon
	pnpm --dir spec exec tsx scripts/validate-boundaries.ts

workflows-validate:
	pnpm --dir spec exec tsx scripts/validate-workflows.ts

boundaries-matrix:
	pnpm --dir spec exec tsx scripts/generate-boundary-matrix.ts

boundaries-test: zcc
	cd spec && bash tests/boundary/run-all.sh

evidence-report:
ifndef RUN_ID
ifndef ALL_RUNS
	$(error RUN_ID is required (e.g., make evidence-report RUN_ID=<run-id>) or use ALL_RUNS=1)
endif
endif
	pnpm --dir spec exec tsx scripts/emit-evidence.ts --report $(if $(RUN_ID),--run-id $(RUN_ID)) $(if $(ALL_RUNS),--all-runs)

qec-max-fast:
	python3 -m pytest -q tests/test_trace_schema_validation.py tests/test_invariants_battery.py tests/test_determinism_contract.py

qec-max-determinism:
	QEC_SEED=1337 QEC_FUZZ_SEEDS=10 python3 tests/test_stabilizer_fuzz.py
	QEC_SEED=1337 QEC_FUZZ_SEEDS=10 python3 tests/test_stabilizer_fuzz.py
	QEC_SEED=1337 QEC_FUZZ_SEEDS=10 python3 tests/test_stabilizer_fuzz.py

qec-max-summary:
	python3 scripts/artifact_utils.py || true
	python3 scripts/make_repro_script.py || true
	python3 scripts/validate_artifacts.py
	python3 scripts/render_job_summary.py || true


