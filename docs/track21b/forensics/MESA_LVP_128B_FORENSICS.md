# ZCC Track 21B Forensics: Upstream Mesa Lavapipe `u_cpu_detect` `L3_affinity_masks` Teardown Leak

## 1. Executive Attribution Summary

On commit `3d2fce68b13870bcc0e7811ecf40100ed9a834b3`, remote GitHub Actions workflow **`ZCC Track 21B — SPIR-V & Vulkan Compute CI`** (run `45961958699`, job `140381630793`, Step 6) exited with code `2`:

```text
SUMMARY: AddressSanitizer: 9856 byte(s) leaked in 77 allocation(s).
```

All `77` allocations had exact size `128` bytes (`77 * 128 = 9,856 B`): `46` reached via `runtime/compute_vulkan.c:868` (`zcc_vulkan_ctx_create`) and `31` reached via `runtime/compute_vulkan.c:398` (`zcc_probe_vulkan_caps`), with zero ZCC live resource leaks (`TRACK21B_C_TESTS pass=262 fail=0`, `live_total=0`, `seq_violations=0`, `teardown_mask=0x7ffe`).

Independent reproduction in a clean Ubuntu `24.04` (`noble`) `bwrap` sysroot with exact runner packages (`gcc 13.3.0-6ubuntu2~24.04.1`, `libc6 2.39-0ubuntu8.12`, `mesa-vulkan-drivers 25.2.8-0ubuntu0.24.04.5`, `libvulkan1 1.3.275.0-1build1`, `vulkan-validationlayers 1.3.275.0-1`) reproduced the **exact `9,856 B` / `77` allocation LeakSanitizer signature** on ZCC AND reproduced the same leak (`128 B/cycle`) in a **standalone zero-ZCC Vulkan C program** (`tests/track21b/forensics/zero_zcc_vulkan_repro.c`) compiled with `-lvulkan`.

---

## 2. Root-Cause Proof & Exact Binary Correlation

### 2.1 Pre-`dlclose` Allocator Stack & ELF Identity
Capturing allocations via `LD_PRELOAD` (`tests/track21b/transitive_alloc_auditor.c`) before Vulkan Loader unloads `libvulkan_lvp.so` identifies the exact origin inside stock Ubuntu `24.04` `libvulkan_lvp.so` (Build ID `8494c48f5f2c2e0ff01872de64b5f9b40867e7a0`, SHA-256 `bd9a708b927aff0b327725af4816c54c53107017e443c72de26e65c310ec67ca`):

```text
libvulkan_lvp.so+0x150de4
  <- libvulkan_lvp.so+0x1512b7
  <- libc.so.6+0xa1f93 (__pthread_once_slow)
  <- libvulkan_lvp.so+0x38ce33
  <- libvulkan_lvp.so+0x61160
  <- libvulkan_lvp.so+0x10c1dd
```

Disassembly of `libvulkan_lvp.so` (`Build ID: 8494c48f5f2c2e0ff01872de64b5f9b40867e7a0`) confirms:

```asm
  150ddf: e8 8c c9 f0 ff        call   5d770 <realloc@plt>
  150de4: 48 89 85 78 de ff ff  mov    %rax,-0x2188(%rbp)
  ...
  150e02: 48 c1 e2 07           shl    $0x7,%rdx               ; 128 B stride = sizeof(util_affinity_mask)
  ...
  150e16: ba 80 00 00 00        mov    $0x80,%edx
  150e1b: e8 40 d9 f0 ff        call   5e760 <__memset_chk@plt>
```

And at caller `0x1512b2` / `0x1512cb`:
- `call 150920` (`get_cpu_topology`)
- `mov $0x106,%ecx; rep movsq` (`262 * 8 = 2,096 B`, copying `util_cpu_caps` into `_util_cpu_caps_state.caps` in `.bss` at `0xb61ea8`)

### 2.2 Source Root Cause (`src/util/u_cpu_detect.c`)
In `mesa_25.2.8.orig.tar.xz` (`src/util/u_cpu_detect.c`), inside `get_cpu_topology(bool zen)`:
1. When `zen` (`x86_cpu_type >= 0x17`) is true, Mesa enumerates AMD L3 domains and allocates:
   `L3_affinity_masks = realloc(L3_affinity_masks, sizeof(util_affinity_mask) * num_L3_caches);`
   and publishes it into `util_cpu_caps.L3_affinity_mask` (copied to `_util_cpu_caps_state.caps`).
2. **Zero `atexit` or destructor cleanup exists** for `L3_affinity_masks` anywhere in Mesa `25.2.8` or upstream `main`.
3. Additionally, if `realloc` returns `NULL` for `num_L3_caches >= 2`, the function returns immediately without freeing the old `L3_affinity_masks` pointer or restoring the saved thread CPU affinity.
4. **Architecture dependence:** `sizeof(util_affinity_mask)` is `UTIL_MAX_CPUS / 8 = 1024 / 8 = 128` bytes on Linux x86_64, so each instance leaks `128 * num_L3_caches` bytes (`128 B` for single-CCD / `1` L3 domain, scaling to `128 * K` bytes on multi-CCD EPYC systems — `128 B` is NOT a universal constant).
5. **CI runner SKU divergence (`45945768235` vs `45961958699`):** GitHub-hosted `ubuntu-24.04` runners land on either Intel Xeon (`zen == false`, skipping `get_cpu_topology(true)` -> `0 B` leaked) or AMD EPYC (`zen == true` -> `128 B * cycles` leaked).

---

## 3. Standalone Zero-ZCC Control & Scaling Evidence (`ubuntu-24.04` `bwrap`)

All tests below were run directly inside the reconstructed Ubuntu `24.04` sandbox using `tests/track21b/forensics/zero_zcc_vulkan_repro.c` with strict `ASAN_OPTIONS=detect_leaks=1` (zero suppressions):

| Probe Mode | Validation Layer | `vkEnumeratePhysicalDevices` | Iterations | `lvp_mapped` End | LSan Exit Code | Leaked Bytes | Allocs |
|---|---|---|---|---|---|---|---|
| `dlopen-only` (`libvulkan.so.1`) | N/A | N/A | 10 | 0 | 0 | 0 | 0 |
| `val-off-no-phys` | OFF | OFF | 10 | 0 | 0 | 0 | 0 |
| `val-off-with-phys` | OFF | ON | 10 | 0 | 1 | 1,280 | 10 |
| `val-on-no-phys` | ON | OFF | 10 | 0 | 1 | 1,280 | 10 |
| `val-on-with-phys` (`1` iter) | ON | ON | 1 | 0 | 1 | 128 | 1 |
| `val-on-with-phys` (`10` iters) | ON | ON | 10 | 0 | 1 | 1,280 | 10 |
| `val-on-with-phys` (`100` iters) | ON | ON | 100 | 0 | 1 | 12,800 | 100 |
| `reuse-so` (keep `libvulkan.so.1` loaded) | ON | ON | 10 | 0 | 1 | 1,280 | 10 |
| `nodelete-icd` (hold `libvulkan_lvp.so` open) | ON | ON | 10 | 1 | 0 | 0 (masked by DSO pin) | 0 |

Notice the `nodelete-icd` control proves why Ubuntu `26.04` (where `libvulkan_lvp.so` remains pinned in memory across `dlclose`) masked the defect under LeakSanitizer: keeping `libvulkan_lvp.so`'s `.bss` mapped leaves `L3_affinity_mask` reachable until process exit, whereas unloading `libvulkan_lvp.so` orphans the pointer immediately (`10/10` cycles reachable pre-`dlclose`, `10/10` cycles orphaned post-`dlclose` under `transitive_alloc_auditor`).

---

## 4. Machine-Readable Artifacts & Checksums

- Reproducer source: [zero_zcc_vulkan_repro.c](file:///h:/__DOWNLOADS/zcc_github_upload/tests/track21b/forensics/zero_zcc_vulkan_repro.c)
- Canonical forensic JSON: [FORENSIC_MANIFEST.json](file:///h:/__DOWNLOADS/zcc_github_upload/docs/track21b/forensics/FORENSIC_MANIFEST.json)
- Minimal upstream Mesa patch: [mesa-25.2.8-u_cpu_detect-l3-teardown.patch](file:///h:/__DOWNLOADS/zcc_github_upload/patches/mesa-25.2.8-u_cpu_detect-l3-teardown.patch) (`65a58ad9d9bad78aeee362304235cb30e80b65d45a0ae6b7571f9debcfea1ac6`)
- SHA-256 checksums: [SHA256SUMS](file:///h:/__DOWNLOADS/zcc_github_upload/docs/track21b/forensics/SHA256SUMS)
