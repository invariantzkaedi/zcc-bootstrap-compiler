#define _GNU_SOURCE
#include <dlfcn.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan.h>

/*
 * ZCC Track 21B Forensic Reproducer: Zero-ZCC Standalone Vulkan Driver Probe
 *
 * Demonstrates and isolates the upstream Mesa llvmpipe (libvulkan_lvp.so)
 * u_cpu_detect() Zen L3_affinity_masks leak using ONLY standard Vulkan API
 * calls and zero ZCC runtime, SPIR-V, or compiler headers/sources.
 *
 * Build modes:
 *   1. Direct link (zero dlopen/dlclose in executable):
 *      gcc -O1 -g -fsanitize=address -DZERO_ZCC_DIRECT_LVULKAN \
 *          tests/track21b/forensics/zero_zcc_vulkan_repro.c -lvulkan -lpthread -o min_vk
 *   2. Controlled lifecycle matrix (dlopen/dlclose isolation):
 *      gcc -O2 -pthread tests/track21b/forensics/zero_zcc_vulkan_repro.c -ldl -lpthread -o vk_probe
 */

static int count_mapped_dso(const char *substr) {
    FILE *f = fopen("/proc/self/maps", "r");
    if (!f) return -1;
    char line[512];
    int count = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, substr) && strstr(line, "r-xp")) {
            count++;
        }
    }
    fclose(f);
    return count;
}

#ifdef ZERO_ZCC_DIRECT_LVULKAN

int main(int argc, char **argv) {
    int iters = (argc >= 2) ? atoi(argv[1]) : 1;
    int use_val = (argc >= 3) ? atoi(argv[2]) : 1;
    int use_phys = (argc >= 4) ? atoi(argv[3]) : 1;
    if (iters < 1) iters = 1;

    for (int i = 0; i < iters; i++) {
        VkApplicationInfo app = {
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pApplicationName = "zero_zcc_minimal_vk",
            .apiVersion = VK_API_VERSION_1_0,
        };
        const char *layers[1] = { "VK_LAYER_KHRONOS_validation" };
        VkInstanceCreateInfo ici = {
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pApplicationInfo = &app,
            .enabledLayerCount = use_val ? 1U : 0U,
            .ppEnabledLayerNames = use_val ? layers : NULL,
        };
        VkInstance inst = VK_NULL_HANDLE;
        VkResult vr = vkCreateInstance(&ici, NULL, &inst);
        if (vr == VK_SUCCESS && inst != VK_NULL_HANDLE) {
            uint32_t phys_count = 0;
            if (use_phys) {
                vkEnumeratePhysicalDevices(inst, &phys_count, NULL);
            }
            if (i == 0) {
                fprintf(stderr, "ZERO_ZCC_DIRECT rc=%d phys_count=%u iters=%d val=%d phys=%d\n",
                        (int)vr, phys_count, iters, use_val, use_phys);
            }
            vkDestroyInstance(inst, NULL);
        } else {
            fprintf(stderr, "ZERO_ZCC_DIRECT_ERR rc=%d iter=%d\n", (int)vr, i);
            return 2;
        }
    }
    fprintf(stderr, "ZERO_ZCC_DIRECT_END lvp_mapped=%d\n", count_mapped_dso("libvulkan_lvp"));
    return 0;
}

#else

typedef struct {
    int thread_id;
    int cycles;
    int use_val;
    int use_phys;
    int err;
} mt_arg_t;

static int run_one_vk_cycle(int use_val, int use_phys, int keep_so_open, void **cached_so, uint32_t *out_phys) {
    void *so = (keep_so_open && *cached_so) ? *cached_so : dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
    if (!so) return 1;
    if (keep_so_open) *cached_so = so;

    PFN_vkGetInstanceProcAddr gipa = (PFN_vkGetInstanceProcAddr)dlsym(so, "vkGetInstanceProcAddr");
    if (!gipa) {
        if (!keep_so_open) dlclose(so);
        return 1;
    }

    PFN_vkCreateInstance ci = (PFN_vkCreateInstance)gipa(VK_NULL_HANDLE, "vkCreateInstance");
    if (!ci) {
        if (!keep_so_open) dlclose(so);
        return 1;
    }

    const char *layers[1] = { "VK_LAYER_KHRONOS_validation" };
    VkApplicationInfo app = {0};
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "zero_zcc_probe";
    app.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo ici = {0};
    ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ici.pApplicationInfo = &app;
    if (use_val) {
        ici.enabledLayerCount = 1U;
        ici.ppEnabledLayerNames = layers;
    }

    VkInstance inst = VK_NULL_HANDLE;
    VkResult vr = ci(&ici, NULL, &inst);
    if (vr != VK_SUCCESS || inst == VK_NULL_HANDLE) {
        if (!keep_so_open) dlclose(so);
        return 2;
    }

    if (use_phys) {
        PFN_vkEnumeratePhysicalDevices ep =
            (PFN_vkEnumeratePhysicalDevices)gipa(inst, "vkEnumeratePhysicalDevices");
        uint32_t count = 0;
        if (ep) ep(inst, &count, NULL);
        if (out_phys) *out_phys = count;
    }

    PFN_vkDestroyInstance di = (PFN_vkDestroyInstance)gipa(inst, "vkDestroyInstance");
    if (di) di(inst, NULL);

    if (!keep_so_open) dlclose(so);
    return 0;
}

static void *mt_worker(void *argp) {
    mt_arg_t *arg = (mt_arg_t *)argp;
    void *so = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
    if (!so) {
        arg->err = 1;
        return NULL;
    }
    PFN_vkGetInstanceProcAddr gipa = (PFN_vkGetInstanceProcAddr)dlsym(so, "vkGetInstanceProcAddr");
    PFN_vkCreateInstance ci = gipa ? (PFN_vkCreateInstance)gipa(VK_NULL_HANDLE, "vkCreateInstance") : NULL;
    if (!ci) {
        dlclose(so);
        arg->err = 1;
        return NULL;
    }
    const char *layers[1] = { "VK_LAYER_KHRONOS_validation" };
    for (int i = 0; i < arg->cycles; i++) {
        VkInstance inst[2] = { VK_NULL_HANDLE, VK_NULL_HANDLE };
        for (int k = 0; k < 2; k++) {
            VkApplicationInfo app = {0};
            app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
            app.pApplicationName = "zero_zcc_mt";
            app.apiVersion = VK_API_VERSION_1_0;
            VkInstanceCreateInfo ici = {0};
            ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
            ici.pApplicationInfo = &app;
            if (arg->use_val) {
                ici.enabledLayerCount = 1U;
                ici.ppEnabledLayerNames = layers;
            }
            if (ci(&ici, NULL, &inst[k]) == VK_SUCCESS && inst[k] != VK_NULL_HANDLE) {
                if (arg->use_phys) {
                    PFN_vkEnumeratePhysicalDevices ep =
                        (PFN_vkEnumeratePhysicalDevices)gipa(inst[k], "vkEnumeratePhysicalDevices");
                    uint32_t dc = 0;
                    if (ep) ep(inst[k], &dc, NULL);
                }
            } else {
                arg->err = 2;
            }
        }
        for (int k = 1; k >= 0; k--) {
            if (inst[k] != VK_NULL_HANDLE) {
                PFN_vkDestroyInstance di =
                    (PFN_vkDestroyInstance)gipa(inst[k], "vkDestroyInstance");
                if (di) di(inst[k], NULL);
            }
        }
    }
    dlclose(so);
    return NULL;
}

static void print_usage(const char *argv0) {
    fprintf(stderr,
            "Usage: %s --mode <dlopen-only|val-off-no-phys|val-off-with-phys|val-on-no-phys|val-on-with-phys|reuse-so|nodelete-icd|mt-stress> [--iters N] [--threads T]\n",
            argv0);
}

int main(int argc, char **argv) {
    const char *mode = "val-on-with-phys";
    int iters = 1;
    int threads = 8;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--mode") == 0 && i + 1 < argc) {
            mode = argv[++i];
        } else if (strcmp(argv[i], "--iters") == 0 && i + 1 < argc) {
            iters = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--threads") == 0 && i + 1 < argc) {
            threads = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }
    if (iters < 1) iters = 1;
    if (threads < 1) threads = 1;
    if (threads > 64) threads = 64;

    if (strcmp(mode, "dlopen-only") == 0) {
        for (int i = 0; i < iters; i++) {
            void *so = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
            if (!so) return 2;
            dlclose(so);
        }
        fprintf(stderr, "ZERO_ZCC_PROBE mode=%s iters=%d lvp_mapped=%d\n",
                mode, iters, count_mapped_dso("libvulkan_lvp"));
        return 0;
    }

    if (strcmp(mode, "mt-stress") == 0) {
        pthread_t th[64];
        mt_arg_t args[64];
        for (int t = 0; t < threads; t++) {
            args[t].thread_id = t;
            args[t].cycles = iters;
            args[t].use_val = 1;
            args[t].use_phys = 1;
            args[t].err = 0;
            if (pthread_create(&th[t], NULL, mt_worker, &args[t]) != 0) return 3;
        }
        int err = 0;
        for (int t = 0; t < threads; t++) {
            pthread_join(th[t], NULL);
            if (args[t].err) err = args[t].err;
        }
        fprintf(stderr, "ZERO_ZCC_PROBE mode=%s threads=%d cycles=%d err=%d lvp_mapped=%d\n",
                mode, threads, iters, err, count_mapped_dso("libvulkan_lvp"));
        return err;
    }

    int use_val = 0, use_phys = 0, reuse_so = 0, nodelete_icd = 0;
    if (strcmp(mode, "val-off-no-phys") == 0) {
        use_val = 0; use_phys = 0;
    } else if (strcmp(mode, "val-off-with-phys") == 0) {
        use_val = 0; use_phys = 1;
    } else if (strcmp(mode, "val-on-no-phys") == 0) {
        use_val = 1; use_phys = 0;
    } else if (strcmp(mode, "val-on-with-phys") == 0) {
        use_val = 1; use_phys = 1;
    } else if (strcmp(mode, "reuse-so") == 0) {
        use_val = 1; use_phys = 1; reuse_so = 1;
    } else if (strcmp(mode, "nodelete-icd") == 0) {
        use_val = 1; use_phys = 1; nodelete_icd = 1;
    } else {
        print_usage(argv[0]);
        return 2;
    }

    void *pinned_lvp = NULL;
    if (nodelete_icd) {
        pinned_lvp = dlopen("libvulkan_lvp.so", RTLD_LAZY | RTLD_LOCAL);
    }

    void *cached_so = NULL;
    uint32_t phys_count = 0;
    int rc = 0;
    for (int i = 0; i < iters; i++) {
        uint32_t c = 0;
        rc = run_one_vk_cycle(use_val, use_phys, reuse_so, &cached_so, &c);
        if (rc != 0) break;
        if (i == 0) phys_count = c;
    }

    int mapped_before_cleanup = count_mapped_dso("libvulkan_lvp");
    if (cached_so) dlclose(cached_so);
    int mapped_after_so_close = count_mapped_dso("libvulkan_lvp");
    if (pinned_lvp) dlclose(pinned_lvp);

    fprintf(stderr,
            "ZERO_ZCC_PROBE mode=%s iters=%d val=%d phys=%d reuse_so=%d nodelete_icd=%d phys_count=%u lvp_mapped_mid=%d lvp_mapped_end=%d rc=%d\n",
            mode, iters, use_val, use_phys, reuse_so, nodelete_icd,
            phys_count, mapped_before_cleanup, mapped_after_so_close, rc);
    return rc;
}

#endif
