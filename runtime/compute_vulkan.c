/* SPDX-License-Identifier: MIT */
#define _POSIX_C_SOURCE 200809L
#define VK_NO_PROTOTYPES
#include "zcc_vulkan_compute.h"
#include <dlfcn.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan.h>

typedef struct zcc_vk_dbg_state_s {
    uint32_t error_count;
    char     first_msg[192];
} zcc_vk_dbg_state_t;

static VKAPI_ATTR VkBool32 VKAPI_CALL vk_dbg_cb(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT types,
    const VkDebugUtilsMessengerCallbackDataEXT *data,
    void *user_data
) {
    (void)types;
    zcc_vk_dbg_state_t *st = (zcc_vk_dbg_state_t *)user_data;
    if (st && (severity & (VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT))) {
        st->error_count++;
        if (!st->first_msg[0] && data && data->pMessage) {
            snprintf(st->first_msg, sizeof(st->first_msg), "%s", data->pMessage);
        }
    }
    return VK_FALSE;
}

const char *zcc_vulkan_device_type_str(uint32_t dev_type) {
    switch (dev_type) {
        case ZCC_VK_DEVTYPE_INTEGRATED_GPU: return "INTEGRATED_GPU";
        case ZCC_VK_DEVTYPE_DISCRETE_GPU:   return "DISCRETE_GPU";
        case ZCC_VK_DEVTYPE_VIRTUAL_GPU:    return "VIRTUAL_GPU";
        case ZCC_VK_DEVTYPE_CPU:            return "CPU";
        default:                            return "OTHER";
    }
}

typedef struct vk_lib_s {
    void *so;
    PFN_vkGetInstanceProcAddr gipa;
    PFN_vkGetDeviceProcAddr gdpa;
    PFN_vkEnumerateInstanceLayerProperties enum_layers;
    PFN_vkEnumerateInstanceExtensionProperties enum_inst_exts;
    PFN_vkCreateInstance create_instance;
    PFN_vkDestroyInstance destroy_instance;
    PFN_vkEnumeratePhysicalDevices enum_phys;
    PFN_vkGetPhysicalDeviceProperties get_phys_props;
    PFN_vkGetPhysicalDeviceQueueFamilyProperties get_qfp;
    PFN_vkGetPhysicalDeviceMemoryProperties get_mem_props;
    PFN_vkEnumerateDeviceExtensionProperties enum_dev_exts;
    PFN_vkCreateDevice create_device;
} vk_lib_t;

static int load_vk_lib(vk_lib_t *vk) {
    memset(vk, 0, sizeof(*vk));
    vk->so = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
    if (!vk->so) return -1;
    /* ISO C forbids direct void* -> fn ptr cast; memcpy avoids -Wpedantic warnings cleanly */
    void *sym = dlsym(vk->so, "vkGetInstanceProcAddr");
    if (!sym) { dlclose(vk->so); vk->so = NULL; return -1; }
    memcpy(&vk->gipa, &sym, sizeof(sym));
    vk->enum_layers    = (PFN_vkEnumerateInstanceLayerProperties)vk->gipa(VK_NULL_HANDLE, "vkEnumerateInstanceLayerProperties");
    vk->enum_inst_exts = (PFN_vkEnumerateInstanceExtensionProperties)vk->gipa(VK_NULL_HANDLE, "vkEnumerateInstanceExtensionProperties");
    vk->create_instance = (PFN_vkCreateInstance)vk->gipa(VK_NULL_HANDLE, "vkCreateInstance");
    if (!vk->create_instance) { dlclose(vk->so); vk->so = NULL; return -1; }
    return 0;
}

static void unload_vk_lib(vk_lib_t *vk) {
    if (vk && vk->so) {
        dlclose(vk->so);
        vk->so = NULL;
    }
}

static void bind_instance_procs(vk_lib_t *vk, VkInstance inst) {
    vk->destroy_instance = (PFN_vkDestroyInstance)vk->gipa(inst, "vkDestroyInstance");
    vk->enum_phys        = (PFN_vkEnumeratePhysicalDevices)vk->gipa(inst, "vkEnumeratePhysicalDevices");
    vk->get_phys_props   = (PFN_vkGetPhysicalDeviceProperties)vk->gipa(inst, "vkGetPhysicalDeviceProperties");
    vk->get_qfp          = (PFN_vkGetPhysicalDeviceQueueFamilyProperties)vk->gipa(inst, "vkGetPhysicalDeviceQueueFamilyProperties");
    vk->get_mem_props    = (PFN_vkGetPhysicalDeviceMemoryProperties)vk->gipa(inst, "vkGetPhysicalDeviceMemoryProperties");
    vk->enum_dev_exts    = (PFN_vkEnumerateDeviceExtensionProperties)vk->gipa(inst, "vkEnumerateDeviceExtensionProperties");
    vk->create_device    = (PFN_vkCreateDevice)vk->gipa(inst, "vkCreateDevice");
    vk->gdpa             = (PFN_vkGetDeviceProcAddr)vk->gipa(inst, "vkGetDeviceProcAddr");
}

static int has_validation_layer(const vk_lib_t *vk) {
    if (!vk->enum_layers) return 0;
    uint32_t cnt = 0U;
    if (vk->enum_layers(&cnt, NULL) != VK_SUCCESS || cnt == 0U) return 0;
    if (cnt > 128U) cnt = 128U;
    VkLayerProperties *props = (VkLayerProperties *)calloc(cnt, sizeof(*props));
    if (!props) return 0;
    int found = 0;
    if (vk->enum_layers(&cnt, props) == VK_SUCCESS) {
        for (uint32_t i = 0; i < cnt; i++) {
            if (strcmp(props[i].layerName, "VK_LAYER_KHRONOS_validation") == 0) {
                found = 1;
                break;
            }
        }
    }
    free(props);
    return found;
}

static int has_debug_utils_ext(const vk_lib_t *vk) {
    if (!vk->enum_inst_exts) return 0;
    uint32_t cnt = 0U;
    if (vk->enum_inst_exts(NULL, &cnt, NULL) != VK_SUCCESS || cnt == 0U) return 0;
    if (cnt > 256U) cnt = 256U;
    VkExtensionProperties *exts = (VkExtensionProperties *)calloc(cnt, sizeof(*exts));
    if (!exts) return 0;
    int found = 0;
    if (vk->enum_inst_exts(NULL, &cnt, exts) == VK_SUCCESS) {
        for (uint32_t i = 0; i < cnt; i++) {
            if (strcmp(exts[i].extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0) {
                found = 1;
                break;
            }
        }
    }
    free(exts);
    return found;
}

static int device_priority(VkPhysicalDeviceType t) {
    if (t == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) return 4;
    if (t == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) return 3;
    if (t == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU) return 2;
    if (t == VK_PHYSICAL_DEVICE_TYPE_CPU) return 1;
    return 0;
}

static int pick_physical_device(
    const vk_lib_t *vk,
    VkInstance inst,
    uint32_t require_hw_gpu,
    VkPhysicalDevice *out_dev,
    uint32_t *out_qfam,
    zcc_vulkan_caps_t *out_caps
) {
    uint32_t dc = 0U;
    if (vk->enum_phys(inst, &dc, NULL) != VK_SUCCESS || dc == 0U) return -1;
    if (dc > 32U) dc = 32U;
    VkPhysicalDevice devs[32];
    if (vk->enum_phys(inst, &dc, devs) != VK_SUCCESS || dc == 0U) return -1;

    int best_idx = -1;
    int best_prio = -1;
    uint32_t best_qf = 0U;
    VkPhysicalDeviceProperties best_props;
    memset(&best_props, 0, sizeof(best_props));

    for (uint32_t i = 0; i < dc; i++) {
        VkPhysicalDeviceProperties props;
        vk->get_phys_props(devs[i], &props);
        if (require_hw_gpu &&
            props.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
            props.deviceType != VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
            continue;
        }
        uint32_t qc = 0U;
        vk->get_qfp(devs[i], &qc, NULL);
        if (qc == 0U || qc > 32U) continue;
        VkQueueFamilyProperties qfp[32];
        vk->get_qfp(devs[i], &qc, qfp);
        uint32_t cqf = UINT32_MAX;
        for (uint32_t q = 0; q < qc; q++) {
            if ((qfp[q].queueFlags & VK_QUEUE_COMPUTE_BIT) && qfp[q].queueCount > 0U) {
                cqf = q;
                break;
            }
        }
        if (cqf == UINT32_MAX) continue;
        int prio = device_priority(props.deviceType);
        if (prio > best_prio) {
            best_prio = prio;
            best_idx = (int)i;
            best_qf = cqf;
            best_props = props;
        }
    }

    if (out_caps) {
        out_caps->physical_device_count = dc;
    }
    if (best_idx < 0) return -2;

    if (out_dev) *out_dev = devs[best_idx];
    if (out_qfam) *out_qfam = best_qf;
    if (out_caps) {
        out_caps->vulkan_available = 1U;
        out_caps->selected_device_index = (uint32_t)best_idx;
        out_caps->physical_device_type = (uint32_t)best_props.deviceType;
        out_caps->api_version = best_props.apiVersion;
        out_caps->driver_version = best_props.driverVersion;
        out_caps->vendor_id = best_props.vendorID;
        out_caps->device_id = best_props.deviceID;
        out_caps->compute_queue_family = best_qf;
        out_caps->has_compute_queue = 1U;
        out_caps->max_push_constants_size = best_props.limits.maxPushConstantsSize;
        out_caps->max_compute_shared_mem = best_props.limits.maxComputeSharedMemorySize;
        out_caps->max_compute_invocations = best_props.limits.maxComputeWorkGroupInvocations;
        for (int j = 0; j < 3; j++) {
            out_caps->max_workgroup_size[j] = best_props.limits.maxComputeWorkGroupSize[j];
            out_caps->max_workgroup_count[j] = best_props.limits.maxComputeWorkGroupCount[j];
        }
        out_caps->max_storage_buffer_range = (uint64_t)best_props.limits.maxStorageBufferRange;
        out_caps->non_coherent_atom_size = (uint64_t)best_props.limits.nonCoherentAtomSize;
        if (out_caps->non_coherent_atom_size == 0ULL) out_caps->non_coherent_atom_size = 64ULL;
        snprintf(out_caps->device_name, sizeof(out_caps->device_name), "%s", best_props.deviceName);
        snprintf(out_caps->device_type_str, sizeof(out_caps->device_type_str), "%s",
                 zcc_vulkan_device_type_str((uint32_t)best_props.deviceType));

        VkPhysicalDeviceMemoryProperties mp;
        vk->get_mem_props(devs[best_idx], &mp);
        for (uint32_t m = 0; m < mp.memoryTypeCount; m++) {
            if ((mp.memoryTypes[m].propertyFlags &
                 (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) ==
                (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
                out_caps->host_coherent_mem_available = 1U;
                break;
            }
        }
        uint32_t ec = 0U;
        if (vk->enum_dev_exts(devs[best_idx], NULL, &ec, NULL) == VK_SUCCESS && ec > 0U) {
            if (ec > 512U) ec = 512U;
            VkExtensionProperties *exts = (VkExtensionProperties *)calloc(ec, sizeof(*exts));
            if (exts && vk->enum_dev_exts(devs[best_idx], NULL, &ec, exts) == VK_SUCCESS) {
                for (uint32_t e = 0; e < ec; e++) {
                    if (strcmp(exts[e].extensionName, "VK_KHR_shader_float16_int8") == 0) {
                        out_caps->float16_int8_supported = 1U;
                        break;
                    }
                }
            }
            free(exts);
        }
    }
    return 0;
}

zcc_status_t zcc_probe_vulkan_caps(zcc_vulkan_caps_t *out_caps, int enable_validation_layer) {
    if (!out_caps) return ZCC_STATUS_ERR_NULL_PTR;
    memset(out_caps, 0, sizeof(*out_caps));
    out_caps->struct_size = (uint32_t)sizeof(*out_caps);
    snprintf(out_caps->device_type_str, sizeof(out_caps->device_type_str), "NONE");

    vk_lib_t vk;
    if (load_vk_lib(&vk) != 0) return ZCC_STATUS_ERR_BACKEND_UNAVAILABLE;

    int val_avail = has_validation_layer(&vk);
    out_caps->validation_layer_available = (uint32_t)val_avail;
    const char *layers[1] = { "VK_LAYER_KHRONOS_validation" };
    int use_val = (enable_validation_layer && val_avail) ? 1 : 0;

    VkApplicationInfo app;
    memset(&app, 0, sizeof(app));
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "zcc_track21b";
    app.applicationVersion = 1;
    app.pEngineName = "zcc_compute_v2";
    app.engineVersion = 1;
    app.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo ici;
    memset(&ici, 0, sizeof(ici));
    ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ici.pApplicationInfo = &app;
    if (use_val) {
        ici.enabledLayerCount = 1U;
        ici.ppEnabledLayerNames = layers;
    }

    VkInstance inst = VK_NULL_HANDLE;
    VkResult vr = vk.create_instance(&ici, NULL, &inst);
    if (vr != VK_SUCCESS || !inst) {
        unload_vk_lib(&vk);
        return ZCC_STATUS_ERR_BACKEND_UNAVAILABLE;
    }
    out_caps->validation_layer_enabled = (uint32_t)use_val;
    bind_instance_procs(&vk, inst);

    VkPhysicalDevice phys = VK_NULL_HANDLE;
    uint32_t qfam = 0U;
    int prc = pick_physical_device(&vk, inst, 0U, &phys, &qfam, out_caps);
    vk.destroy_instance(inst, NULL);
    unload_vk_lib(&vk);
    return (prc == 0) ? ZCC_STATUS_OK : ZCC_STATUS_ERR_BACKEND_UNAVAILABLE;
}

typedef struct vk_buf_alloc_s {
    VkBuffer       buf;
    VkDeviceMemory mem;
    VkDeviceSize   alloc_size;
    VkDeviceSize   data_bytes;
    uint32_t       base_off;
    int            is_coherent;
} vk_buf_alloc_t;

static int find_mem_type(const VkPhysicalDeviceMemoryProperties *mp, uint32_t type_bits,
                         uint32_t *out_idx, int *out_coherent) {
    for (uint32_t i = 0; i < mp->memoryTypeCount; i++) {
        if ((type_bits & (1U << i)) &&
            (mp->memoryTypes[i].propertyFlags &
             (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) ==
            (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
            *out_idx = i;
            *out_coherent = 1;
            return 0;
        }
    }
    for (uint32_t i = 0; i < mp->memoryTypeCount; i++) {
        if ((type_bits & (1U << i)) &&
            (mp->memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
            *out_idx = i;
            *out_coherent = 0;
            return 0;
        }
    }
    return -1;
}

static VkDeviceSize align_up_u64(VkDeviceSize v, VkDeviceSize align) {
    if (align <= 1ULL) return v;
    return ((v + align - 1ULL) / align) * align;
}

static void resolve_op_dims_strides(
    zcc_op_t op,
    const zcc_tensor_desc_t *inputs,
    const zcc_tensor_desc_t *output,
    float rms_eps,
    zcc_spv_push_t *push
) {
    memset(push, 0, sizeof(*push));
    memcpy(&push->rms_eps_bits, &rms_eps, sizeof(float));

    const zcc_tensor_desc_t *a = &inputs[0];
    const zcc_tensor_desc_t *b = (inputs && a) ? &inputs[1] : NULL;

    if (op == ZCC_OP_VEC_ADD_F32 || op == ZCC_OP_VEC_MUL_F32 || op == ZCC_OP_VEC_FMA_F32) {
        uint64_t total = 1ULL;
        for (uint32_t d = 0; d < a->rank; d++) total *= (uint64_t)a->shape[d];
        push->dim_m = (uint32_t)total;
        push->stride_a0 = (uint32_t)((a->rank == 1U && a->strides[0] > 0) ? a->strides[0] : 4LL);
        push->stride_b0 = (uint32_t)((b && b->rank == 1U && b->strides[0] > 0) ? b->strides[0] : 4LL);
        push->stride_c0 = (uint32_t)((op == ZCC_OP_VEC_FMA_F32 && inputs[2].rank == 1U && inputs[2].strides[0] > 0) ? inputs[2].strides[0] : 4LL);
        push->stride_o0 = (uint32_t)((output->rank == 1U && output->strides[0] > 0) ? output->strides[0] : 4LL);
        /* For 2D+ dense strided elementwise tensors without gaps use linear stride if rank==1; handle rank>=2 if contiguous */
        if (a->rank > 1U) {
            push->stride_a0 = 4U;
            push->stride_b0 = 4U;
            push->stride_c0 = 4U;
            push->stride_o0 = 4U;
        }
    } else if (op == ZCC_OP_DOT_F32) {
        push->dim_k = (uint32_t)a->shape[0];
        push->stride_a0 = (uint32_t)((a->strides[0] > 0) ? a->strides[0] : 4LL);
        push->stride_b0 = (uint32_t)((inputs[1].strides[0] > 0) ? inputs[1].strides[0] : 4LL);
        push->stride_o0 = 4U;
    } else if (op == ZCC_OP_GEMM_F32) {
        push->dim_m = (uint32_t)a->shape[0];
        push->dim_k = (uint32_t)a->shape[1];
        push->dim_n = (uint32_t)inputs[1].shape[1];
        push->stride_a1 = (uint32_t)((a->strides[1] > 0) ? a->strides[1] : 4LL);
        push->stride_a0 = (uint32_t)((a->strides[0] > 0) ? a->strides[0] : (int64_t)push->dim_k * push->stride_a1);
        push->stride_b1 = (uint32_t)((inputs[1].strides[1] > 0) ? inputs[1].strides[1] : 4LL);
        push->stride_b0 = (uint32_t)((inputs[1].strides[0] > 0) ? inputs[1].strides[0] : (int64_t)push->dim_n * push->stride_b1);
        push->stride_o1 = (uint32_t)((output->strides[1] > 0) ? output->strides[1] : 4LL);
        push->stride_o0 = (uint32_t)((output->strides[0] > 0) ? output->strides[0] : (int64_t)push->dim_n * push->stride_o1);
    } else if (op == ZCC_OP_RMSNORM_F32) {
        push->dim_m = (uint32_t)((a->rank == 1U) ? 1 : a->shape[0]);
        push->dim_n = (uint32_t)((a->rank == 1U) ? a->shape[0] : a->shape[1]);
        if (a->rank == 1U) {
            push->stride_a1 = (uint32_t)((a->strides[0] > 0) ? a->strides[0] : 4LL);
            push->stride_a0 = push->dim_n * push->stride_a1;
        } else {
            push->stride_a1 = (uint32_t)((a->strides[1] > 0) ? a->strides[1] : 4LL);
            push->stride_a0 = (uint32_t)((a->strides[0] > 0) ? a->strides[0] : (int64_t)push->dim_n * push->stride_a1);
        }
        push->stride_b0 = (uint32_t)((inputs[1].strides[0] > 0) ? inputs[1].strides[0] : 4LL);
        if (output->rank == 1U) {
            push->stride_o1 = (uint32_t)((output->strides[0] > 0) ? output->strides[0] : 4LL);
            push->stride_o0 = push->dim_n * push->stride_o1;
        } else {
            push->stride_o1 = (uint32_t)((output->strides[1] > 0) ? output->strides[1] : 4LL);
            push->stride_o0 = (uint32_t)((output->strides[0] > 0) ? output->strides[0] : (int64_t)push->dim_n * push->stride_o1);
        }
    } else if (op == ZCC_OP_SOFTMAX_F32) {
        push->dim_m = (uint32_t)((a->rank == 1U) ? 1 : a->shape[0]);
        push->dim_n = (uint32_t)((a->rank == 1U) ? a->shape[0] : a->shape[1]);
        if (a->rank == 1U) {
            push->stride_a1 = (uint32_t)((a->strides[0] > 0) ? a->strides[0] : 4LL);
            push->stride_a0 = push->dim_n * push->stride_a1;
        } else {
            push->stride_a1 = (uint32_t)((a->strides[1] > 0) ? a->strides[1] : 4LL);
            push->stride_a0 = (uint32_t)((a->strides[0] > 0) ? a->strides[0] : (int64_t)push->dim_n * push->stride_a1);
        }
        if (output->rank == 1U) {
            push->stride_o1 = (uint32_t)((output->strides[0] > 0) ? output->strides[0] : 4LL);
            push->stride_o0 = push->dim_n * push->stride_o1;
        } else {
            push->stride_o1 = (uint32_t)((output->strides[1] > 0) ? output->strides[1] : 4LL);
            push->stride_o0 = (uint32_t)((output->strides[0] > 0) ? output->strides[0] : (int64_t)push->dim_n * push->stride_o1);
        }
    } else if (op == ZCC_OP_DEQUANT_Q8_0 || op == ZCC_OP_DEQUANT_Q4_0 || op == ZCC_OP_DEQUANT_Q4_K) {
        push->dim_m = (uint32_t)((a->rank == 1U) ? 1 : a->shape[0]);
        push->dim_k = (uint32_t)((a->rank == 1U) ? a->shape[0] : a->shape[1]);
        if (output->rank == 1U) {
            push->stride_o1 = (uint32_t)((output->strides[0] > 0) ? output->strides[0] : 4LL);
            push->stride_o0 = push->dim_k * push->stride_o1;
        } else {
            push->stride_o1 = (uint32_t)((output->strides[1] > 0) ? output->strides[1] : 4LL);
            push->stride_o0 = (uint32_t)((output->strides[0] > 0) ? output->strides[0] : (int64_t)push->dim_k * push->stride_o1);
        }
    } else {
        push->dim_m = (uint32_t)a->shape[0];
        push->dim_k = (uint32_t)a->shape[1];
        push->stride_b0 = (uint32_t)((inputs[1].strides[0] > 0) ? inputs[1].strides[0] : 4LL);
        push->stride_o0 = (uint32_t)((output->strides[0] > 0) ? output->strides[0] : 4LL);
    }
}

zcc_status_t zcc_vulkan_exec(
    zcc_op_t op,
    const zcc_tensor_desc_t *inputs,
    uint32_t num_inputs,
    zcc_tensor_desc_t *output,
    const zcc_vulkan_exec_options_t *opts,
    const zcc_vulkan_caps_t *caps,
    char *spirv_sha_out,
    uint32_t *out_word_count,
    uint32_t *out_barriers,
    uint32_t *out_flushes,
    uint32_t *out_invalidates,
    uint32_t *out_val_errors,
    char *err_buf,
    size_t err_cap
) {
    if (!inputs || !output || !caps || num_inputs == 0U || num_inputs > 3U) {
        if (err_buf && err_cap) snprintf(err_buf, err_cap, "Invalid Vulkan exec arguments");
        return ZCC_STATUS_ERR_INVALID_ARG;
    }
    if (!caps->vulkan_available || !caps->has_compute_queue) {
        if (err_buf && err_cap) snprintf(err_buf, err_cap, "Vulkan compute device unavailable");
        return ZCC_STATUS_ERR_BACKEND_UNAVAILABLE;
    }
    if (caps->max_push_constants_size < sizeof(zcc_spv_push_t)) {
        if (err_buf && err_cap) snprintf(err_buf, err_cap, "Vulkan device maxPushConstantsSize < 64B");
        return ZCC_STATUS_ERR_UNSUPPORTED_HW;
    }

    zcc_spv_push_t push;
    float eps = (opts && opts->base.rmsnorm_eps > 0.0f) ? opts->base.rmsnorm_eps : 1e-5f;
    resolve_op_dims_strides(op, inputs, output, eps, &push);

    /* Multi-dim non-contiguous F32 elementwise ops fall out of flat linear push indexing cleanly */
    if ((op == ZCC_OP_VEC_ADD_F32 || op == ZCC_OP_VEC_MUL_F32 || op == ZCC_OP_VEC_FMA_F32) && inputs[0].rank > 1U) {
        for (uint32_t d = 0; d < inputs[0].rank; d++) {
            if (inputs[0].strides[d] != 0 || inputs[1].strides[d] != 0 || output->strides[d] != 0 ||
                (op == ZCC_OP_VEC_FMA_F32 && inputs[2].strides[d] != 0)) {
                if (err_buf && err_cap) snprintf(err_buf, err_cap, "Rank>1 strided elementwise unsupported on Vulkan SPIR-V 1D dispatch");
                return ZCC_STATUS_ERR_INVALID_STRIDE;
            }
        }
    }
    if ((push.stride_o0 & 3U) != 0U || (push.stride_o1 & 3U) != 0U) {
        if (err_buf && err_cap) snprintf(err_buf, err_cap, "Output byte stride must be a multiple of 4 bytes on Vulkan");
        return ZCC_STATUS_ERR_MISALIGNED;
    }

    zcc_spv_module_t spv;
    memset(&spv, 0, sizeof(spv));
    int owns_spv = 0;
    const uint32_t *spv_words = NULL;
    size_t spv_word_count = 0U;
    if (opts && opts->spirv_override_words && opts->spirv_override_word_count > 0U) {
        spv_words = opts->spirv_override_words;
        spv_word_count = opts->spirv_override_word_count;
    } else {
        if (zcc_spirv_emit_op_kernel((uint32_t)op, &spv) != 0) {
            if (err_buf && err_cap) snprintf(err_buf, err_cap, "Failed emitting SPIR-V kernel");
            return ZCC_STATUS_ERR_BACKEND_RUNTIME;
        }
        spv_words = spv.words;
        spv_word_count = spv.word_count;
        owns_spv = 1;
    }

    if (spirv_sha_out) {
        zcc_sha256_hex(spv_words, spv_word_count * sizeof(uint32_t), spirv_sha_out);
    }
    if (out_word_count) *out_word_count = (uint32_t)spv_word_count;

    uint32_t gx = 1U, gy = 1U, gz = 1U;
    zcc_spirv_dispatch_grid((uint32_t)op, push.dim_m, push.dim_n, push.dim_k, &gx, &gy, &gz);
    if (gx > caps->max_workgroup_count[0] || gy > caps->max_workgroup_count[1] || gz > caps->max_workgroup_count[2]) {
        if (owns_spv) zcc_spirv_module_free(&spv);
        if (err_buf && err_cap) snprintf(err_buf, err_cap, "Dispatch grid exceeds maxComputeWorkGroupCount");
        return ZCC_STATUS_ERR_UNSUPPORTED_HW;
    }

    vk_lib_t vk;
    if (load_vk_lib(&vk) != 0) {
        if (owns_spv) zcc_spirv_module_free(&spv);
        if (err_buf && err_cap) snprintf(err_buf, err_cap, "Unable to open libvulkan.so.1");
        return ZCC_STATUS_ERR_BACKEND_UNAVAILABLE;
    }

    zcc_status_t ret = ZCC_STATUS_ERR_BACKEND_RUNTIME;
    zcc_vk_dbg_state_t dbg;
    memset(&dbg, 0, sizeof(dbg));

    int val_avail = has_validation_layer(&vk);
    int dbg_ext = has_debug_utils_ext(&vk);
    if (opts && opts->require_validation_layer && (!val_avail || !caps->validation_layer_available)) {
        unload_vk_lib(&vk);
        if (owns_spv) zcc_spirv_module_free(&spv);
        if (err_buf && err_cap) snprintf(err_buf, err_cap, "VK_LAYER_KHRONOS_validation required but missing or restricted");
        return ZCC_STATUS_ERR_BACKEND_UNAVAILABLE;
    }

    const char *inst_layers[1] = { "VK_LAYER_KHRONOS_validation" };
    const char *inst_exts[1] = { VK_EXT_DEBUG_UTILS_EXTENSION_NAME };

    VkApplicationInfo app;
    memset(&app, 0, sizeof(app));
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "zcc_track21b";
    app.applicationVersion = 1;
    app.pEngineName = "zcc_vulkan";
    app.engineVersion = 1;
    app.apiVersion = VK_API_VERSION_1_0;

    VkDebugUtilsMessengerCreateInfoEXT dbg_ci;
    memset(&dbg_ci, 0, sizeof(dbg_ci));
    dbg_ci.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    dbg_ci.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
    dbg_ci.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    dbg_ci.pfnUserCallback = vk_dbg_cb;
    dbg_ci.pUserData = &dbg;

    int use_val = val_avail ? 1 : 0;
    int use_dbg = (use_val && dbg_ext) ? 1 : 0;

    VkInstanceCreateInfo ici;
    memset(&ici, 0, sizeof(ici));
    ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ici.pApplicationInfo = &app;
    if (use_val) {
        ici.enabledLayerCount = 1U;
        ici.ppEnabledLayerNames = inst_layers;
    }
    if (use_dbg) {
        ici.enabledExtensionCount = 1U;
        ici.ppEnabledExtensionNames = inst_exts;
        ici.pNext = &dbg_ci;
    }

    VkInstance inst = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT dbg_msgr = VK_NULL_HANDLE;
    PFN_vkCreateDebugUtilsMessengerEXT create_dbg = NULL;
    PFN_vkDestroyDebugUtilsMessengerEXT destroy_dbg = NULL;

    VkDevice dev = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    VkShaderModule smod = VK_NULL_HANDLE;
    VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
    VkPipelineLayout pl = VK_NULL_HANDLE;
    VkPipeline pipe = VK_NULL_HANDLE;
    VkDescriptorPool dpool = VK_NULL_HANDLE;
    VkDescriptorSet dset = VK_NULL_HANDLE;
    VkCommandPool cpool = VK_NULL_HANDLE;
    VkCommandBuffer cbuf = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    vk_buf_alloc_t bufs[4];
    memset(bufs, 0, sizeof(bufs));

    /* Device function pointers resolved cleanly through vkGetDeviceProcAddr */
    PFN_vkDestroyDevice pDestroyDevice = NULL;
    PFN_vkGetDeviceQueue pGetDeviceQueue = NULL;
    PFN_vkCreateBuffer pCreateBuffer = NULL;
    PFN_vkDestroyBuffer pDestroyBuffer = NULL;
    PFN_vkGetBufferMemoryRequirements pGetBufMemReq = NULL;
    PFN_vkAllocateMemory pAllocateMemory = NULL;
    PFN_vkFreeMemory pFreeMemory = NULL;
    PFN_vkBindBufferMemory pBindBufferMemory = NULL;
    PFN_vkMapMemory pMapMemory = NULL;
    PFN_vkUnmapMemory pUnmapMemory = NULL;
    PFN_vkFlushMappedMemoryRanges pFlushRanges = NULL;
    PFN_vkInvalidateMappedMemoryRanges pInvalidateRanges = NULL;
    PFN_vkCreateShaderModule pCreateShaderModule = NULL;
    PFN_vkDestroyShaderModule pDestroyShaderModule = NULL;
    PFN_vkCreateDescriptorSetLayout pCreateDSL = NULL;
    PFN_vkDestroyDescriptorSetLayout pDestroyDSL = NULL;
    PFN_vkCreatePipelineLayout pCreatePL = NULL;
    PFN_vkDestroyPipelineLayout pDestroyPL = NULL;
    PFN_vkCreateComputePipelines pCreateComputePipes = NULL;
    PFN_vkDestroyPipeline pDestroyPipe = NULL;
    PFN_vkCreateDescriptorPool pCreateDPool = NULL;
    PFN_vkDestroyDescriptorPool pDestroyDPool = NULL;
    PFN_vkAllocateDescriptorSets pAllocDSets = NULL;
    PFN_vkUpdateDescriptorSets pUpdateDSets = NULL;
    PFN_vkCreateCommandPool pCreateCPool = NULL;
    PFN_vkDestroyCommandPool pDestroyCPool = NULL;
    PFN_vkAllocateCommandBuffers pAllocCBufs = NULL;
    PFN_vkBeginCommandBuffer pBeginCBuf = NULL;
    PFN_vkEndCommandBuffer pEndCBuf = NULL;
    PFN_vkCmdBindPipeline pCmdBindPipe = NULL;
    PFN_vkCmdBindDescriptorSets pCmdBindDSets = NULL;
    PFN_vkCmdPushConstants pCmdPush = NULL;
    PFN_vkCmdDispatch pCmdDispatch = NULL;
    PFN_vkCmdPipelineBarrier pCmdBarrier = NULL;
    PFN_vkCreateFence pCreateFence = NULL;
    PFN_vkDestroyFence pDestroyFence = NULL;
    PFN_vkQueueSubmit pQueueSubmit = NULL;
    PFN_vkWaitForFences pWaitForFences = NULL;
    PFN_vkDeviceWaitIdle pDeviceWaitIdle = NULL;

    uint32_t barrier_cnt = 0U, flush_cnt = 0U, inv_cnt = 0U;

    if (vk.create_instance(&ici, NULL, &inst) != VK_SUCCESS || !inst) {
        if (err_buf && err_cap) snprintf(err_buf, err_cap, "vkCreateInstance failed");
        goto cleanup;
    }
    bind_instance_procs(&vk, inst);
    if (use_dbg) {
        create_dbg  = (PFN_vkCreateDebugUtilsMessengerEXT)vk.gipa(inst, "vkCreateDebugUtilsMessengerEXT");
        destroy_dbg = (PFN_vkDestroyDebugUtilsMessengerEXT)vk.gipa(inst, "vkDestroyDebugUtilsMessengerEXT");
        if (create_dbg) create_dbg(inst, &dbg_ci, NULL, &dbg_msgr);
    }

    VkPhysicalDevice phys = VK_NULL_HANDLE;
    uint32_t qfam = 0U;
    uint32_t req_gpu = (opts && opts->require_hardware_gpu) ? 1U : 0U;
    if (pick_physical_device(&vk, inst, req_gpu, &phys, &qfam, NULL) != 0) {
        ret = ZCC_STATUS_ERR_UNSUPPORTED_HW;
        if (err_buf && err_cap) snprintf(err_buf, err_cap, "No qualifying Vulkan physical device");
        goto cleanup;
    }

    float qprio = 1.0f;
    VkDeviceQueueCreateInfo dqci;
    memset(&dqci, 0, sizeof(dqci));
    dqci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    dqci.queueFamilyIndex = qfam;
    dqci.queueCount = 1U;
    dqci.pQueuePriorities = &qprio;

    VkDeviceCreateInfo dci;
    memset(&dci, 0, sizeof(dci));
    dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dci.queueCreateInfoCount = 1U;
    dci.pQueueCreateInfos = &dqci;

    if (vk.create_device(phys, &dci, NULL, &dev) != VK_SUCCESS || !dev) {
        if (err_buf && err_cap) snprintf(err_buf, err_cap, "vkCreateDevice failed");
        goto cleanup;
    }

    pDestroyDevice       = (PFN_vkDestroyDevice)vk.gdpa(dev, "vkDestroyDevice");
    pGetDeviceQueue      = (PFN_vkGetDeviceQueue)vk.gdpa(dev, "vkGetDeviceQueue");
    pCreateBuffer        = (PFN_vkCreateBuffer)vk.gdpa(dev, "vkCreateBuffer");
    pDestroyBuffer       = (PFN_vkDestroyBuffer)vk.gdpa(dev, "vkDestroyBuffer");
    pGetBufMemReq        = (PFN_vkGetBufferMemoryRequirements)vk.gdpa(dev, "vkGetBufferMemoryRequirements");
    pAllocateMemory      = (PFN_vkAllocateMemory)vk.gdpa(dev, "vkAllocateMemory");
    pFreeMemory          = (PFN_vkFreeMemory)vk.gdpa(dev, "vkFreeMemory");
    pBindBufferMemory    = (PFN_vkBindBufferMemory)vk.gdpa(dev, "vkBindBufferMemory");
    pMapMemory           = (PFN_vkMapMemory)vk.gdpa(dev, "vkMapMemory");
    pUnmapMemory         = (PFN_vkUnmapMemory)vk.gdpa(dev, "vkUnmapMemory");
    pFlushRanges         = (PFN_vkFlushMappedMemoryRanges)vk.gdpa(dev, "vkFlushMappedMemoryRanges");
    pInvalidateRanges    = (PFN_vkInvalidateMappedMemoryRanges)vk.gdpa(dev, "vkInvalidateMappedMemoryRanges");
    pCreateShaderModule  = (PFN_vkCreateShaderModule)vk.gdpa(dev, "vkCreateShaderModule");
    pDestroyShaderModule = (PFN_vkDestroyShaderModule)vk.gdpa(dev, "vkDestroyShaderModule");
    pCreateDSL           = (PFN_vkCreateDescriptorSetLayout)vk.gdpa(dev, "vkCreateDescriptorSetLayout");
    pDestroyDSL          = (PFN_vkDestroyDescriptorSetLayout)vk.gdpa(dev, "vkDestroyDescriptorSetLayout");
    pCreatePL            = (PFN_vkCreatePipelineLayout)vk.gdpa(dev, "vkCreatePipelineLayout");
    pDestroyPL           = (PFN_vkDestroyPipelineLayout)vk.gdpa(dev, "vkDestroyPipelineLayout");
    pCreateComputePipes  = (PFN_vkCreateComputePipelines)vk.gdpa(dev, "vkCreateComputePipelines");
    pDestroyPipe         = (PFN_vkDestroyPipeline)vk.gdpa(dev, "vkDestroyPipeline");
    pCreateDPool         = (PFN_vkCreateDescriptorPool)vk.gdpa(dev, "vkCreateDescriptorPool");
    pDestroyDPool        = (PFN_vkDestroyDescriptorPool)vk.gdpa(dev, "vkDestroyDescriptorPool");
    pAllocDSets          = (PFN_vkAllocateDescriptorSets)vk.gdpa(dev, "vkAllocateDescriptorSets");
    pUpdateDSets         = (PFN_vkUpdateDescriptorSets)vk.gdpa(dev, "vkUpdateDescriptorSets");
    pCreateCPool         = (PFN_vkCreateCommandPool)vk.gdpa(dev, "vkCreateCommandPool");
    pDestroyCPool        = (PFN_vkDestroyCommandPool)vk.gdpa(dev, "vkDestroyCommandPool");
    pAllocCBufs          = (PFN_vkAllocateCommandBuffers)vk.gdpa(dev, "vkAllocateCommandBuffers");
    pBeginCBuf           = (PFN_vkBeginCommandBuffer)vk.gdpa(dev, "vkBeginCommandBuffer");
    pEndCBuf             = (PFN_vkEndCommandBuffer)vk.gdpa(dev, "vkEndCommandBuffer");
    pCmdBindPipe         = (PFN_vkCmdBindPipeline)vk.gdpa(dev, "vkCmdBindPipeline");
    pCmdBindDSets        = (PFN_vkCmdBindDescriptorSets)vk.gdpa(dev, "vkCmdBindDescriptorSets");
    pCmdPush             = (PFN_vkCmdPushConstants)vk.gdpa(dev, "vkCmdPushConstants");
    pCmdDispatch         = (PFN_vkCmdDispatch)vk.gdpa(dev, "vkCmdDispatch");
    pCmdBarrier          = (PFN_vkCmdPipelineBarrier)vk.gdpa(dev, "vkCmdPipelineBarrier");
    pCreateFence         = (PFN_vkCreateFence)vk.gdpa(dev, "vkCreateFence");
    pDestroyFence        = (PFN_vkDestroyFence)vk.gdpa(dev, "vkDestroyFence");
    pQueueSubmit         = (PFN_vkQueueSubmit)vk.gdpa(dev, "vkQueueSubmit");
    pWaitForFences       = (PFN_vkWaitForFences)vk.gdpa(dev, "vkWaitForFences");
    pDeviceWaitIdle      = (PFN_vkDeviceWaitIdle)vk.gdpa(dev, "vkDeviceWaitIdle");

    pGetDeviceQueue(dev, qfam, 0U, &queue);

    VkPhysicalDeviceMemoryProperties mp;
    vk.get_mem_props(phys, &mp);
    VkDeviceSize atom = caps->non_coherent_atom_size ? (VkDeviceSize)caps->non_coherent_atom_size : 64ULL;

    /* Create and populate 4 storage buffers (binding 0..2 inputs/dummies, binding 3 output) */
    for (uint32_t bi = 0U; bi < 4U; bi++) {
        const zcc_tensor_desc_t *td = (bi < num_inputs) ? &inputs[bi] : (bi == 3U ? output : NULL);
        uint32_t align_rem = td ? (uint32_t)(((uintptr_t)td->data) & 3U) : 0U;
        if (bi == 3U) align_rem = 0U; /* keep output staging base 4-byte aligned on device */
        VkDeviceSize nbytes = td ? (VkDeviceSize)td->nbytes : 16ULL;
        VkDeviceSize req = align_up_u64((VkDeviceSize)align_rem + nbytes + 4ULL, 16ULL);
        if (req > caps->max_storage_buffer_range) {
            ret = ZCC_STATUS_ERR_UNSUPPORTED_HW;
            if (err_buf && err_cap) snprintf(err_buf, err_cap, "Buffer %u exceeds maxStorageBufferRange", bi);
            goto cleanup;
        }
        bufs[bi].base_off = align_rem;
        bufs[bi].data_bytes = nbytes;

        VkBufferCreateInfo bci;
        memset(&bci, 0, sizeof(bci));
        bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bci.size = req;
        bci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (pCreateBuffer(dev, &bci, NULL, &bufs[bi].buf) != VK_SUCCESS) {
            if (err_buf && err_cap) snprintf(err_buf, err_cap, "vkCreateBuffer failed");
            goto cleanup;
        }

        VkMemoryRequirements mr;
        pGetBufMemReq(dev, bufs[bi].buf, &mr);
        uint32_t mtype = 0U;
        if (find_mem_type(&mp, mr.memoryTypeBits, &mtype, &bufs[bi].is_coherent) != 0) {
            ret = ZCC_STATUS_ERR_UNSUPPORTED_HW;
            if (err_buf && err_cap) snprintf(err_buf, err_cap, "No host-visible memory type available");
            goto cleanup;
        }
        bufs[bi].alloc_size = align_up_u64(mr.size, atom);

        VkMemoryAllocateInfo mai;
        memset(&mai, 0, sizeof(mai));
        mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        mai.allocationSize = bufs[bi].alloc_size;
        mai.memoryTypeIndex = mtype;
        if (pAllocateMemory(dev, &mai, NULL, &bufs[bi].mem) != VK_SUCCESS) {
            ret = ZCC_STATUS_ERR_OOM;
            if (err_buf && err_cap) snprintf(err_buf, err_cap, "vkAllocateMemory failed");
            goto cleanup;
        }
        if (pBindBufferMemory(dev, bufs[bi].buf, bufs[bi].mem, 0ULL) != VK_SUCCESS) goto cleanup;

        void *mapped = NULL;
        if (pMapMemory(dev, bufs[bi].mem, 0ULL, bufs[bi].alloc_size, 0U, &mapped) != VK_SUCCESS || !mapped) goto cleanup;
        memset(mapped, 0, (size_t)req);
        if (td && td->data && td->nbytes > 0ULL) {
            memcpy((uint8_t *)mapped + bufs[bi].base_off, td->data, (size_t)td->nbytes);
        }
        if (!bufs[bi].is_coherent && pFlushRanges) {
            VkMappedMemoryRange rng = { VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, NULL, bufs[bi].mem, 0ULL, VK_WHOLE_SIZE };
            pFlushRanges(dev, 1U, &rng);
            flush_cnt++;
        }
        pUnmapMemory(dev, bufs[bi].mem);
    }

    push.off_a_bytes = bufs[0].base_off;
    push.off_b_bytes = bufs[1].base_off;
    push.off_c_bytes = bufs[2].base_off;
    push.off_o_bytes = bufs[3].base_off;

    VkShaderModuleCreateInfo smci;
    memset(&smci, 0, sizeof(smci));
    smci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    smci.codeSize = spv_word_count * sizeof(uint32_t);
    smci.pCode = spv_words;
    if (pCreateShaderModule(dev, &smci, NULL, &smod) != VK_SUCCESS || dbg.error_count > 0U) {
        if (err_buf && err_cap) snprintf(err_buf, err_cap, "vkCreateShaderModule failed/validation error: %s", dbg.first_msg);
        goto cleanup;
    }

    VkDescriptorSetLayoutBinding bindings[4];
    memset(bindings, 0, sizeof(bindings));
    for (uint32_t i = 0U; i < 4U; i++) {
        bindings[i].binding = i;
        bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[i].descriptorCount = 1U;
        bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }
    VkDescriptorSetLayoutCreateInfo dslci;
    memset(&dslci, 0, sizeof(dslci));
    dslci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dslci.bindingCount = 4U;
    dslci.pBindings = bindings;
    if (pCreateDSL(dev, &dslci, NULL, &dsl) != VK_SUCCESS) goto cleanup;

    VkPushConstantRange pcr;
    memset(&pcr, 0, sizeof(pcr));
    pcr.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pcr.offset = 0U;
    pcr.size = (uint32_t)sizeof(zcc_spv_push_t);

    VkPipelineLayoutCreateInfo plci;
    memset(&plci, 0, sizeof(plci));
    plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plci.setLayoutCount = 1U;
    plci.pSetLayouts = &dsl;
    plci.pushConstantRangeCount = 1U;
    plci.pPushConstantRanges = &pcr;
    if (pCreatePL(dev, &plci, NULL, &pl) != VK_SUCCESS) goto cleanup;

    VkComputePipelineCreateInfo cpci;
    memset(&cpci, 0, sizeof(cpci));
    cpci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    cpci.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    cpci.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    cpci.stage.module = smod;
    cpci.stage.pName = "main";
    cpci.layout = pl;
    if (pCreateComputePipes(dev, VK_NULL_HANDLE, 1U, &cpci, NULL, &pipe) != VK_SUCCESS || dbg.error_count > 0U) {
        if (err_buf && err_cap) snprintf(err_buf, err_cap, "vkCreateComputePipelines failed: %s", dbg.first_msg);
        goto cleanup;
    }

    VkDescriptorPoolSize dps = { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4U };
    VkDescriptorPoolCreateInfo dpci;
    memset(&dpci, 0, sizeof(dpci));
    dpci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    dpci.maxSets = 1U;
    dpci.poolSizeCount = 1U;
    dpci.pPoolSizes = &dps;
    if (pCreateDPool(dev, &dpci, NULL, &dpool) != VK_SUCCESS) goto cleanup;

    VkDescriptorSetAllocateInfo dsai;
    memset(&dsai, 0, sizeof(dsai));
    dsai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    dsai.descriptorPool = dpool;
    dsai.descriptorSetCount = 1U;
    dsai.pSetLayouts = &dsl;
    if (pAllocDSets(dev, &dsai, &dset) != VK_SUCCESS) goto cleanup;

    VkDescriptorBufferInfo dbi[4];
    VkWriteDescriptorSet wds[4];
    memset(dbi, 0, sizeof(dbi));
    memset(wds, 0, sizeof(wds));
    for (uint32_t i = 0U; i < 4U; i++) {
        dbi[i].buffer = bufs[i].buf;
        dbi[i].offset = 0ULL;
        dbi[i].range = VK_WHOLE_SIZE;
        wds[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        wds[i].dstSet = dset;
        wds[i].dstBinding = i;
        wds[i].descriptorCount = 1U;
        wds[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        wds[i].pBufferInfo = &dbi[i];
    }
    pUpdateDSets(dev, 4U, wds, 0U, NULL);

    VkCommandPoolCreateInfo cmdpci;
    memset(&cmdpci, 0, sizeof(cmdpci));
    cmdpci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmdpci.queueFamilyIndex = qfam;
    if (pCreateCPool(dev, &cmdpci, NULL, &cpool) != VK_SUCCESS) goto cleanup;

    VkCommandBufferAllocateInfo cbai;
    memset(&cbai, 0, sizeof(cbai));
    cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbai.commandPool = cpool;
    cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = 1U;
    if (pAllocCBufs(dev, &cbai, &cbuf) != VK_SUCCESS) goto cleanup;

    VkCommandBufferBeginInfo cbbi;
    memset(&cbbi, 0, sizeof(cbbi));
    cbbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cbbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (pBeginCBuf(cbuf, &cbbi) != VK_SUCCESS) goto cleanup;

    /* Explicit Barrier 1: Host write -> Compute shader read/write visibility */
    VkMemoryBarrier pre_bar;
    memset(&pre_bar, 0, sizeof(pre_bar));
    pre_bar.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    pre_bar.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
    pre_bar.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    pCmdBarrier(cbuf, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                0U, 1U, &pre_bar, 0U, NULL, 0U, NULL);
    barrier_cnt++;

    pCmdBindPipe(cbuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipe);
    pCmdBindDSets(cbuf, VK_PIPELINE_BIND_POINT_COMPUTE, pl, 0U, 1U, &dset, 0U, NULL);
    pCmdPush(cbuf, pl, VK_SHADER_STAGE_COMPUTE_BIT, 0U, (uint32_t)sizeof(push), &push);
    pCmdDispatch(cbuf, gx, gy, gz);

    /* Explicit Barrier 2: Compute shader write -> Host read visibility */
    VkMemoryBarrier post_bar;
    memset(&post_bar, 0, sizeof(post_bar));
    post_bar.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    post_bar.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    post_bar.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    pCmdBarrier(cbuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                0U, 1U, &post_bar, 0U, NULL, 0U, NULL);
    barrier_cnt++;

    if (pEndCBuf(cbuf) != VK_SUCCESS) goto cleanup;

    VkFenceCreateInfo fci;
    memset(&fci, 0, sizeof(fci));
    fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    if (pCreateFence(dev, &fci, NULL, &fence) != VK_SUCCESS) goto cleanup;

    VkSubmitInfo si;
    memset(&si, 0, sizeof(si));
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1U;
    si.pCommandBuffers = &cbuf;
    if (pQueueSubmit(queue, 1U, &si, fence) != VK_SUCCESS) goto cleanup;

    if (pWaitForFences(dev, 1U, &fence, VK_TRUE, 30000000000ULL) != VK_SUCCESS) {
        if (err_buf && err_cap) snprintf(err_buf, err_cap, "vkWaitForFences timed out or failed");
        goto cleanup;
    }

    void *out_mapped = NULL;
    if (pMapMemory(dev, bufs[3].mem, 0ULL, bufs[3].alloc_size, 0U, &out_mapped) != VK_SUCCESS || !out_mapped) goto cleanup;
    if (!bufs[3].is_coherent && pInvalidateRanges) {
        VkMappedMemoryRange rng = { VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, NULL, bufs[3].mem, 0ULL, VK_WHOLE_SIZE };
        pInvalidateRanges(dev, 1U, &rng);
        inv_cnt++;
    }
    memcpy(output->data, (const uint8_t *)out_mapped + bufs[3].base_off, (size_t)output->nbytes);
    pUnmapMemory(dev, bufs[3].mem);

    pDeviceWaitIdle(dev);
    if (dbg.error_count > 0U) {
        if (err_buf && err_cap) snprintf(err_buf, err_cap, "Vulkan validation errors (%u): %s", dbg.error_count, dbg.first_msg);
        goto cleanup;
    }

    ret = ZCC_STATUS_OK;

cleanup:
    if (dev) {
        if (pDeviceWaitIdle) pDeviceWaitIdle(dev);
        if (fence && pDestroyFence) pDestroyFence(dev, fence, NULL);
        if (cpool && pDestroyCPool) pDestroyCPool(dev, cpool, NULL);
        if (dpool && pDestroyDPool) pDestroyDPool(dev, dpool, NULL);
        if (pipe && pDestroyPipe) pDestroyPipe(dev, pipe, NULL);
        if (pl && pDestroyPL) pDestroyPL(dev, pl, NULL);
        if (dsl && pDestroyDSL) pDestroyDSL(dev, dsl, NULL);
        if (smod && pDestroyShaderModule) pDestroyShaderModule(dev, smod, NULL);
        for (uint32_t bi = 0U; bi < 4U; bi++) {
            if (bufs[bi].buf && pDestroyBuffer) pDestroyBuffer(dev, bufs[bi].buf, NULL);
            if (bufs[bi].mem && pFreeMemory) pFreeMemory(dev, bufs[bi].mem, NULL);
        }
        if (pDestroyDevice) pDestroyDevice(dev, NULL);
    }
    if (dbg_msgr && destroy_dbg) destroy_dbg(inst, dbg_msgr, NULL);
    if (inst && vk.destroy_instance) vk.destroy_instance(inst, NULL);
    unload_vk_lib(&vk);
    if (owns_spv) zcc_spirv_module_free(&spv);

    if (out_barriers) *out_barriers = barrier_cnt;
    if (out_flushes) *out_flushes = flush_cnt;
    if (out_invalidates) *out_invalidates = inv_cnt;
    if (out_val_errors) *out_val_errors = dbg.error_count;
    return ret;
}
