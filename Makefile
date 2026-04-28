CC = gcc
CFLAGS = -O0 -w -fno-asynchronous-unwind-tables -g0
LDFLAGS = -lm -Wl,-s
FAST_CFLAGS = -O2 -DNDEBUG -w -fno-asynchronous-unwind-tables -g0
FORTIFY_PACK_DIR ?= fortify_zcc_clean

PARTS = part1.c part0_pp.c part2.c part3.c ir.h ir_emit_dispatch.h ir_bridge.h part4.c part5.c part7_rust.c part6_arm.c ir.c ir_to_x86.c regalloc.c ir_telemetry_stub.c
PASSES = compiler_passes.c compiler_passes_ir.c ir_pass_manager.c
COMPAT_SMOKE_SRCS = \
	exp1_raytracer_simd.c \
	exp2_voxel_engine.c \
	exp3_audio_visualizer.c \
	exp4_vr_stereo.c \
	exp5_physics_engine.c \
	test_asm_real.c \
	test_vla.c \
	tests/test_abi.c \
	tests/test_asm_real.c
COMPAT_EXTENDED_SRCS = $(COMPAT_SMOKE_SRCS) raytracer.c

.PHONY: all clean selfhost selfhost-fast compat-smoke compat-extended compat-report compat-report-ci pp-crlf-gate fortify-ad fortify-ci fortify-snapshot fortify-recursive fortify-recursive-ci fortify-pack-init fortify-pack-preflight fortify-pack-layout fortify-pack-production fortify-pack-replay fortify-pack-clean supercharge-ad test rust-front-smoke

all: zcc

zcc_ast_bridge_constants.h zcc_ast_bridge_asserts.inc: part1.c sync_bridge.py
	python3 sync_bridge.py part1.c zcc_ast_bridge_constants.h zcc_ast_bridge_asserts.inc

zcc.c: $(PARTS) zcc_ast_bridge_constants.h zcc_ast_bridge_asserts.inc
	cat $(PARTS) > zcc.c

zcc: zcc.c $(PASSES)
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
	$(CC) $(CFLAGS) -o zcc zcc.c $(PASSES) $(LDFLAGS)
	strip --strip-all zcc

zcc_fast: zcc.c $(PASSES)
	$(CC) $(FAST_CFLAGS) -o zcc_fast zcc.c $(PASSES) $(LDFLAGS)
	strip --strip-all zcc_fast

selfhost: zcc
	@echo "=== Stage 1: zcc compiles itself -> zcc2 ==="
	./zcc zcc.c -o zcc2
	strip --strip-all zcc2
	@echo "=== Stage 2: zcc2 compiles itself -> zcc3 ==="
	./zcc2 zcc.c -o zcc3
	strip --strip-all zcc3
	@echo "=== Verify: zcc2.s == zcc3.s (codegen parity) ==="
	./zcc  zcc.c -o zcc2.s
	./zcc2 zcc.c -o zcc3.s
	diff zcc2.s zcc3.s && echo "SELF-HOST VERIFIED (assembly identical)" || (echo "SELF-HOST FAILED (assembly diverged)"; diff zcc2.s zcc3.s | head -20; exit 1)

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

fortify-ad: selfhost-fast compat-extended pp-crlf-gate compat-report
	@echo "FORTIFY A+D COMPLETE"

fortify-ci: selfhost-fast compat-extended pp-crlf-gate compat-report-ci
	@echo "FORTIFY CI COMPLETE"

# External fortify pack wiring (CI/tooling only; no compiler source integration yet).
fortify-pack-init:
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

test: zcc
	bash zcc_test_suite.sh --quick

asan: zcc.c $(PASSES)
	$(CC) -fsanitize=address -O0 -g -o zcc_asan zcc.c $(PASSES) $(LDFLAGS)
	@echo "ASan build ready. Run: ./zcc_asan zcc.c -o /dev/null"

clean:
	rm -f zcc zcc_fast zcc2 zcc2_fast zcc3 zcc3_fast zcc_asan zcc.c zcc_pp.c *.s *.o
	rm -rf .compat_out .compat_logs

ir-verify: zcc2
	@echo "[IR-VERIFY] Stage 2 IR emission..."
	ZCC_EMIT_IR=1 ./zcc2 zcc.c -o zcc_ir_stage2.s
	@echo "[IR-VERIFY] Linking IR stage 2 binary..."
	gcc zcc_ir_stage2.s compiler_passes.c compiler_passes_ir.c ir_pass_manager.c -o zcc_ir_stage2 -lm
	@echo "[IR-VERIFY] Stage 3 via IR path..."
	ZCC_EMIT_IR=1 ./zcc_ir_stage2 zcc.c -o zcc_ir_stage3.s

sqlite: zcc2
	@echo "=== Compiling SQLite 160MB Amalgamation with ZCC ==="
	./zcc2 sqlite3_zcc.c -o sqlite3_zcc.s
	@echo "=== Linking sqlite3_test ==="
	gcc -no-pie -O0 -w -fno-asynchronous-unwind-tables -Wa,--noexecstack -fno-unwind-tables \
		-o sqlite3_test sqlite3_zcc.s sqlite3_functest.c -ldl -lpthread -lm
	@echo "=== Build Complete ==="

# ─── ONEIROGENESIS v2: A Compiler That Dreams ────────────────────
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
