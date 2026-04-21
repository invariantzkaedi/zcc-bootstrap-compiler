CC = gcc
CFLAGS = -O0 -w -fno-asynchronous-unwind-tables -g0
LDFLAGS = -lm -Wl,-s

PARTS = part1.c part0_pp.c part2.c part3.c ir.h ir_emit_dispatch.h ir_bridge.h part4.c part5.c part6_arm.c ir.c ir_to_x86.c regalloc.c ir_telemetry_stub.c
PASSES = compiler_passes.c compiler_passes_ir.c ir_pass_manager.c

.PHONY: all clean selfhost test

all: zcc

zcc.c: $(PARTS)
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

selfhost: zcc
	@echo "=== Stage 1: zcc compiles itself -> zcc2 ==="
	./zcc zcc.c -o zcc2
	strip --strip-all zcc2
	@echo "=== Stage 2: zcc2 compiles itself -> zcc3 ==="
	./zcc2 zcc.c -o zcc3
	strip --strip-all zcc3
	@echo "=== Verify: zcc2 == zcc3 (bitwise) ==="
	cmp zcc2 zcc3 && echo "SELF-HOST VERIFIED (bitwise stable)" || (echo "SELF-HOST FAILED"; cmp -l zcc2 zcc3 | head; exit 1)

test: zcc
	bash zcc_test_suite.sh --quick

asan: zcc.c $(PASSES)
	$(CC) -fsanitize=address -O0 -g -o zcc_asan zcc.c $(PASSES) $(LDFLAGS)
	@echo "ASan build ready. Run: ./zcc_asan zcc.c -o /dev/null"

clean:
	rm -f zcc zcc2 zcc3 zcc_asan zcc.c zcc_pp.c *.s *.o

ir-verify: zcc2
	@echo "[IR-VERIFY] Stage 2 IR emission..."
	ZCC_EMIT_IR=1 ./zcc2 zcc.c -o zcc_ir_stage2.s
	@echo "[IR-VERIFY] Linking IR stage 2 binary..."
	gcc zcc_ir_stage2.s compiler_passes.c compiler_passes_ir.c -o zcc_ir_stage2 -lm
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
