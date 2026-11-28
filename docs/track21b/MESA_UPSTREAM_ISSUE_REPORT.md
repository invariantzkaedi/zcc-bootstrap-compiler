# Upstream Bug Report Draft (Submission-Ready, Not Submitted Externally)

**Target Project:** Mesa 3D (`mesa/mesa`)  
**Component:** `util` / `src/util/u_cpu_detect.c`, `src/util/thread_sched.c` (observed via `gallium/targets/lavapipe` `libvulkan_lvp.so`)  
**Title:** `util/u_cpu_detect: L3_affinity_masks leaked per dlopen/vkCreateInstance cycle on AMD Zen CPUs (plus OOM realloc cleanup in get_cpu_topology)`

---

## Summary

On AMD Zen processors (`x86_cpu_type >= 0x17`, `zen == true`), `get_cpu_topology()` in `src/util/u_cpu_detect.c` dynamically allocates `L3_affinity_masks` (`sizeof(util_affinity_mask) * num_L3_caches`, e.g., `128 * num_L3_caches` bytes on Linux x86_64) via `realloc` and stores the pointer in `util_cpu_caps.L3_affinity_mask` (copied to `_util_cpu_caps_state.caps`), with **no destructor or `atexit` handler to free it**.

When a Vulkan driver such as Lavapipe (`libvulkan_lvp.so`) is dynamically loaded by Vulkan Loader during `vkCreateInstance()` and subsequently unmapped during `vkDestroyInstance()` (`dlclose()`), `_util_cpu_caps_state.caps` leaves `.bss` when `libvulkan_lvp.so` unmaps, orphaning `128 * num_L3_caches` bytes of heap memory on **every** instance lifecycle. LeakSanitizer (`-fsanitize=address`, `detect_leaks=1`) reports `num_cycles * (128 * num_L3_caches)` bytes leaked per run.

Additionally, if `realloc()` returns `NULL` when `num_L3_caches >= 2`, `get_cpu_topology()` immediately returns without freeing the previously allocated `L3_affinity_masks` buffer or restoring the thread's saved CPU affinity mask.

---

## Observed Affected Versions & Environment

- **Mesa:** `25.2.8` (reproduced on Ubuntu `24.04` `mesa-vulkan-drivers 25.2.8-0ubuntu0.24.04.5`, `libvulkan_lvp.so` ELF Build ID `8494c48f5f2c2e0ff01872de64b5f9b40867e7a0`, SHA-256 `bd9a708b927aff0b327725af4816c54c53107017e443c72de26e65c310ec67ca`; also verified absent teardown in upstream Mesa `main` `src/util/u_cpu_detect.c`)
- **Loader / Toolchain:** `libvulkan1 1.3.275.0-1build1`, `libc6 2.39-0ubuntu8.12`, `gcc 13.3.0-6ubuntu2~24.04.1`, `libasan8 14.2.0-4ubuntu2~24.04.1`
- **CPU:** AMD Zen (`AuthenticAMD`, e.g. `AMD Ryzen AI 7 350` Model 96 with 1 L3 instance -> `128 B/cycle`, and AMD EPYC CI runners; Intel Xeon SKUs with `x86_cpu_type < 0x17` skip `if (zen)` and do not hit the leak)
- **Allocator stack before `dlclose` (`libvulkan_lvp.so`, Build ID `8494c48f...`):**
  - `libvulkan_lvp.so+0x150de4` (`realloc` inside `get_cpu_topology` at `shl $0x7` / `memset $0x80`)
  - `<- libvulkan_lvp.so+0x1512b7` (`_util_cpu_detect_once` -> `rep movsq` into `_util_cpu_caps_state.caps`)
  - `<- libc.so.6+0xa1f93` (`__pthread_once_slow`)

---

## Minimal Standalone Reproducer (Zero External Runtime Code)

Save as `repro_lvp_leak.c`:

```c
#include <stdio.h>
#include <vulkan/vulkan.h>

int main(void) {
    for (int i = 0; i < 10; i++) {
        VkApplicationInfo app = {
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pApplicationName = "lvp_l3_leak_repro",
            .apiVersion = VK_API_VERSION_1_0,
        };
        VkInstanceCreateInfo ici = {
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pApplicationInfo = &app,
        };
        VkInstance inst = VK_NULL_HANDLE;
        if (vkCreateInstance(&ici, NULL, &inst) == VK_SUCCESS && inst) {
            uint32_t count = 0;
            vkEnumeratePhysicalDevices(inst, &count, NULL);
            vkDestroyInstance(inst, NULL);
        }
    }
    return 0;
}
```

Compile and run on Ubuntu 24.04 (AMD Zen host) with Lavapipe:
```bash
gcc -O1 -g -fsanitize=address repro_lvp_leak.c -lvulkan -o repro_lvp_leak
ASAN_OPTIONS=detect_leaks=1 ./repro_lvp_leak
```

### Actual Output (Ubuntu 24.04, AMD Zen)
```text
SUMMARY: AddressSanitizer: 1280 byte(s) leaked in 10 allocation(s).
```
Scaling across `1`, `10`, and `100` cycles (with `vkEnumeratePhysicalDevices` and `libvulkan_lvp.so` unloading between cycles) produces:
- `1` cycle: `128 byte(s) leaked in 1 allocation(s)`
- `10` cycles: `1280 byte(s) leaked in 10 allocation(s)`
- `100` cycles: `12800 byte(s) leaked in 100 allocation(s)`

*(Note: `128 B` is specific to `UTIL_MAX_CPUS = 1024` and `num_L3_caches = 1`; multi-CCD Zen systems leak `128 * num_L3_caches` bytes per cycle. When `libvulkan_lvp.so` remains pinned in memory via `RTLD_NODELETE`, `L3_affinity_mask` stays reachable from `.bss` and hides the missing teardown from LSan.)*

---

## Companion Packaging Defect: Exported `STB_GNU_UNIQUE` Symbols Pinning `libvulkan_lvp.so` Across `dlclose()`

In Ubuntu `24.04` (`mesa-vulkan-drivers 25.2.8-0ubuntu0.24.04.5`, Build ID `8494c48f5f2c2e0ff01872de64b5f9b40867e7a0`), `libvulkan_lvp.so`, `libvulkan_asahi.so`, and `libvulkan_nouveau.so` each export **5 `STB_GNU_UNIQUE` symbols** (`spvTargetEnvNameMap` plus 4 `spvtools::utils::HexFloat<...>::getRoundedNormalizedSignificand` statics/guards) referenced via `R_X86_64_GLOB_DAT` from statically linked `libSPIRV-Tools`, while `libLLVM.so.20.1` (`Build ID: 3fcd3870...`) carries `FLAGS_1: NODELETE`.

When `libvulkan_lvp.so` is loaded standalone (or scanned ahead of `asahi`/`nouveau` in `/usr/share/vulkan/icd.d/`), `LD_DEBUG=bindings,files,reloc` shows glibc's `do_lookup_unique()` marking and activating `NODELETE` directly on `libvulkan_lvp.so`:
- `marking .../libvulkan_lvp.so [0] as NODELETE due to unique symbol`
- `activating NODELETE for .../libvulkan_lvp.so [0]`

By contrast, loading `asahi` or `nouveau` first shifts the `NODELETE` pin onto `libvulkan_asahi.so`/`libvulkan_nouveau.so` and lets `libvulkan_lvp.so` unload (`lvp_mapped_end=0`), exposing the `L3_affinity_masks` leak. Building Lavapipe with `-fno-gnu-unique` and `-Wl,--exclude-libs,libSPIRV-Tools.a:libSPIRV-Tools-opt.a:libSPIRV-Tools-link.a` reduces `STB_GNU_UNIQUE` symbols to `0` (`stb_gnu_unique_count == 0`) and resolves the unintentional DSO pinning cleanly. *(Attributing this runtime `NODELETE` behavior in downstream test harnesses classifies stock packaging accurately as blocked without altering natural ICD enumeration order or fixing the underlying filesystem scan sensitivity.)*

---

## Proposed Minimal Patch

Consistent with existing teardown patterns in `src/util/` (`u_queue.c:82`, `u_process.c:213`, `strtod.c:53`), register `free_l3_affinity_masks` via `atexit()` inside `_util_cpu_detect_once` (and guard `free_l3_affinity_masks()` on re-entry), handle OOM `realloc` failure without returning early past saved affinity restoration or leaking prior `L3_affinity_masks`, and guard `thread_sched.c` against NULL `caps->L3_affinity_mask` or out-of-range `num_L3_caches`:

See [patches/mesa-25.2.8-u_cpu_detect-l3-teardown.patch](file:///h:/__DOWNLOADS/zcc_github_upload/patches/mesa-25.2.8-u_cpu_detect-l3-teardown.patch).

### Verification After Patch
- **100-iteration unload stress (`lvp_mapped=0` verified on every teardown)** under strict `ASAN_OPTIONS=detect_leaks=1`: **0 leaked bytes (exit 0)**.
- **8-thread concurrent dual-instance stress** under strict `ASan + UBSan + LSan`: **0 leaked bytes, 0 races/UAFs (exit 0)**.
- **Pre-`dlclose` & post-`dlclose` `LD_PRELOAD` allocation audit:** `orphaned_on_dlclose_bytes = 0`, `unmapped_target_events = 10`, `final_target_mapped = 0`.
