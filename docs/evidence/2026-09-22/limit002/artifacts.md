# LIMIT-002 Artifacts & Checksums

## Source Artifacts
- `zcc_c11_c23.h`: SHA256 `0077ccf3947242d3a460a43126c057504f5bee3d797d9aaf3ed1328e7a4c1dc6` (combined source hash)
- `include/threads.h`
- `part1.c`
- `part2.c`
- `part3.c`

## Bootstrap Assembly Artifacts
- `zcc2.s`: MD5 `ceb8c1f7c8e5da2f9038807e9beaf629` / SHA256 `b636c008d4a869aeff8572be66a9290d746dc85af35bcf32d975cdcfa363f45f`
- `zcc3.s`: MD5 `ceb8c1f7c8e5da2f9038807e9beaf629` / SHA256 `b636c008d4a869aeff8572be66a9290d746dc85af35bcf32d975cdcfa363f45f`
- `cmp zcc2.s zcc3.s`: Byte-identical (exit 0)

## Test Suite Artifacts
- `tests/limit002/manifest.tsv`: SHA256 `f799af4d8c6f7edd1ee7f3934c8cf3e2b4b9ceb5813ce9d9afd52fe7785cadb0`
- 13 Test programs in `tests/limit002/`
- Test runner: `tools/run_limit002_gauntlet.py`
