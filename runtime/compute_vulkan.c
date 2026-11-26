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
    void          *mapped;
    VkDeviceSize   buf_cap;
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

typedef struct vk_dev_procs_s {
    PFN_vkDestroyDevice                pDestroyDevice;
    PFN_vkGetDeviceQueue               pGetDeviceQueue;
    PFN_vkCreateBuffer                 pCreateBuffer;
    PFN_vkDestroyBuffer                pDestroyBuffer;
    PFN_vkGetBufferMemoryRequirements  pGetBufMemReq;
    PFN_vkAllocateMemory               pAllocateMemory;
    PFN_vkFreeMemory                   pFreeMemory;
    PFN_vkBindBufferMemory             pBindBufferMemory;
    PFN_vkMapMemory                    pMapMemory;
    PFN_vkUnmapMemory                  pUnmapMemory;
    PFN_vkFlushMappedMemoryRanges      pFlushRanges;
    PFN_vkInvalidateMappedMemoryRanges pInvalidateRanges;
    PFN_vkCreateShaderModule           pCreateShaderModule;
    PFN_vkDestroyShaderModule          pDestroyShaderModule;
    PFN_vkCreateDescriptorSetLayout    pCreateDSL;
    PFN_vkDestroyDescriptorSetLayout   pDestroyDSL;
    PFN_vkCreatePipelineLayout         pCreatePL;
    PFN_vkDestroyPipelineLayout        pDestroyPL;
    PFN_vkCreateComputePipelines       pCreateComputePipes;
    PFN_vkDestroyPipeline              pDestroyPipe;
    PFN_vkCreateDescriptorPool         pCreateDPool;
    PFN_vkDestroyDescriptorPool        pDestroyDPool;
    PFN_vkAllocateDescriptorSets       pAllocDSets;
    PFN_vkUpdateDescriptorSets         pUpdateDSets;
    PFN_vkCreateCommandPool            pCreateCPool;
    PFN_vkResetCommandPool             pResetCPool;
    PFN_vkDestroyCommandPool           pDestroyCPool;
    PFN_vkAllocateCommandBuffers       pAllocCBufs;
    PFN_vkBeginCommandBuffer           pBeginCBuf;
    PFN_vkEndCommandBuffer             pEndCBuf;
    PFN_vkCmdBindPipeline              pCmdBindPipe;
    PFN_vkCmdBindDescriptorSets        pCmdBindDSets;
    PFN_vkCmdPushConstants             pCmdPush;
    PFN_vkCmdDispatch                  pCmdDispatch;
    PFN_vkCmdPipelineBarrier           pCmdBarrier;
    PFN_vkCreateFence                  pCreateFence;
    PFN_vkResetFences                  pResetFences;
    PFN_vkDestroyFence                 pDestroyFence;
    PFN_vkQueueSubmit                  pQueueSubmit;
    PFN_vkWaitForFences                pWaitForFences;
    PFN_vkDeviceWaitIdle               pDeviceWaitIdle;
} vk_dev_procs_t;

static int bind_device_procs(const vk_lib_t *vk, VkDevice dev, vk_dev_procs_t *dp) {
    memset(dp, 0, sizeof(*dp));
    dp->pDestroyDevice       = (PFN_vkDestroyDevice)vk->gdpa(dev, "vkDestroyDevice");
    dp->pGetDeviceQueue      = (PFN_vkGetDeviceQueue)vk->gdpa(dev, "vkGetDeviceQueue");
    dp->pCreateBuffer        = (PFN_vkCreateBuffer)vk->gdpa(dev, "vkCreateBuffer");
    dp->pDestroyBuffer       = (PFN_vkDestroyBuffer)vk->gdpa(dev, "vkDestroyBuffer");
    dp->pGetBufMemReq        = (PFN_vkGetBufferMemoryRequirements)vk->gdpa(dev, "vkGetBufferMemoryRequirements");
    dp->pAllocateMemory      = (PFN_vkAllocateMemory)vk->gdpa(dev, "vkAllocateMemory");
    dp->pFreeMemory          = (PFN_vkFreeMemory)vk->gdpa(dev, "vkFreeMemory");
    dp->pBindBufferMemory    = (PFN_vkBindBufferMemory)vk->gdpa(dev, "vkBindBufferMemory");
    dp->pMapMemory           = (PFN_vkMapMemory)vk->gdpa(dev, "vkMapMemory");
    dp->pUnmapMemory         = (PFN_vkUnmapMemory)vk->gdpa(dev, "vkUnmapMemory");
    dp->pFlushRanges         = (PFN_vkFlushMappedMemoryRanges)vk->gdpa(dev, "vkFlushMappedMemoryRanges");
    dp->pInvalidateRanges    = (PFN_vkInvalidateMappedMemoryRanges)vk->gdpa(dev, "vkInvalidateMappedMemoryRanges");
    dp->pCreateShaderModule  = (PFN_vkCreateShaderModule)vk->gdpa(dev, "vkCreateShaderModule");
    dp->pDestroyShaderModule = (PFN_vkDestroyShaderModule)vk->gdpa(dev, "vkDestroyShaderModule");
    dp->pCreateDSL           = (PFN_vkCreateDescriptorSetLayout)vk->gdpa(dev, "vkCreateDescriptorSetLayout");
    dp->pDestroyDSL          = (PFN_vkDestroyDescriptorSetLayout)vk->gdpa(dev, "vkDestroyDescriptorSetLayout");
    dp->pCreatePL            = (PFN_vkCreatePipelineLayout)vk->gdpa(dev, "vkCreatePipelineLayout");
    dp->pDestroyPL           = (PFN_vkDestroyPipelineLayout)vk->gdpa(dev, "vkDestroyPipelineLayout");
    dp->pCreateComputePipes  = (PFN_vkCreateComputePipelines)vk->gdpa(dev, "vkCreateComputePipelines");
    dp->pDestroyPipe         = (PFN_vkDestroyPipeline)vk->gdpa(dev, "vkDestroyPipeline");
    dp->pCreateDPool         = (PFN_vkCreateDescriptorPool)vk->gdpa(dev, "vkCreateDescriptorPool");
    dp->pDestroyDPool        = (PFN_vkDestroyDescriptorPool)vk->gdpa(dev, "vkDestroyDescriptorPool");
    dp->pAllocDSets          = (PFN_vkAllocateDescriptorSets)vk->gdpa(dev, "vkAllocateDescriptorSets");
    dp->pUpdateDSets         = (PFN_vkUpdateDescriptorSets)vk->gdpa(dev, "vkUpdateDescriptorSets");
    dp->pCreateCPool         = (PFN_vkCreateCommandPool)vk->gdpa(dev, "vkCreateCommandPool");
    dp->pResetCPool          = (PFN_vkResetCommandPool)vk->gdpa(dev, "vkResetCommandPool");
    dp->pDestroyCPool        = (PFN_vkDestroyCommandPool)vk->gdpa(dev, "vkDestroyCommandPool");
    dp->pAllocCBufs          = (PFN_vkAllocateCommandBuffers)vk->gdpa(dev, "vkAllocateCommandBuffers");
    dp->pBeginCBuf           = (PFN_vkBeginCommandBuffer)vk->gdpa(dev, "vkBeginCommandBuffer");
    dp->pEndCBuf             = (PFN_vkEndCommandBuffer)vk->gdpa(dev, "vkEndCommandBuffer");
    dp->pCmdBindPipe         = (PFN_vkCmdBindPipeline)vk->gdpa(dev, "vkCmdBindPipeline");
    dp->pCmdBindDSets        = (PFN_vkCmdBindDescriptorSets)vk->gdpa(dev, "vkCmdBindDescriptorSets");
    dp->pCmdPush             = (PFN_vkCmdPushConstants)vk->gdpa(dev, "vkCmdPushConstants");
    dp->pCmdDispatch         = (PFN_vkCmdDispatch)vk->gdpa(dev, "vkCmdDispatch");
    dp->pCmdBarrier          = (PFN_vkCmdPipelineBarrier)vk->gdpa(dev, "vkCmdPipelineBarrier");
    dp->pCreateFence         = (PFN_vkCreateFence)vk->gdpa(dev, "vkCreateFence");
    dp->pResetFences         = (PFN_vkResetFences)vk->gdpa(dev, "vkResetFences");
    dp->pDestroyFence        = (PFN_vkDestroyFence)vk->gdpa(dev, "vkDestroyFence");
    dp->pQueueSubmit         = (PFN_vkQueueSubmit)vk->gdpa(dev, "vkQueueSubmit");
    dp->pWaitForFences       = (PFN_vkWaitForFences)vk->gdpa(dev, "vkWaitForFences");
    dp->pDeviceWaitIdle      = (PFN_vkDeviceWaitIdle)vk->gdpa(dev, "vkDeviceWaitIdle");
    if (!dp->pDestroyDevice || !dp->pGetDeviceQueue || !dp->pCreateBuffer ||
        !dp->pDestroyBuffer || !dp->pGetBufMemReq || !dp->pAllocateMemory ||
        !dp->pFreeMemory || !dp->pBindBufferMemory || !dp->pMapMemory ||
        !dp->pUnmapMemory || !dp->pCreateShaderModule || !dp->pDestroyShaderModule ||
        !dp->pCreateDSL || !dp->pDestroyDSL || !dp->pCreatePL || !dp->pDestroyPL ||
        !dp->pCreateComputePipes || !dp->pDestroyPipe || !dp->pCreateDPool ||
        !dp->pDestroyDPool || !dp->pAllocDSets || !dp->pUpdateDSets ||
        !dp->pCreateCPool || !dp->pResetCPool || !dp->pDestroyCPool ||
        !dp->pAllocCBufs || !dp->pBeginCBuf || !dp->pEndCBuf || !dp->pCmdBindPipe ||
        !dp->pCmdBindDSets || !dp->pCmdPush || !dp->pCmdDispatch || !dp->pCmdBarrier ||
        !dp->pCreateFence || !dp->pResetFences || !dp->pDestroyFence ||
        !dp->pQueueSubmit || !dp->pWaitForFences || !dp->pDeviceWaitIdle) {
        return -1;
    }
    return 0;
}

#define ZCC_VULKAN_CTX_MAGIC 0x5A564B43U /* "ZVKC" */
#define ZCC_VK_MAX_CACHED_OPS 16U

struct zcc_vulkan_ctx_s {
    uint32_t                            magic;
    uint32_t                            abi_version;
    zcc_vulkan_caps_t                   caps;
    int                                 validation_enabled;
    uint32_t                            require_hardware_gpu;
    vk_lib_t                            vk;
    VkInstance                          inst;
    VkDebugUtilsMessengerEXT            dbg_msgr;
    PFN_vkDestroyDebugUtilsMessengerEXT destroy_dbg;
    zcc_vk_dbg_state_t                  dbg;
    VkPhysicalDevice                    phys;
    uint32_t                            qfam;
    VkPhysicalDeviceMemoryProperties    mem_props;
    VkDevice                            dev;
    VkQueue                             queue;
    vk_dev_procs_t                      dp;
    VkDescriptorSetLayout               dsl;
    VkPipelineLayout                    pl;
    VkDescriptorPool                    dpool;
    VkDescriptorSet                     dset;
    VkCommandPool                       cpool;
    VkCommandBuffer                     cbuf;
    VkFence                             fence;
    VkShaderModule                      cached_smods[ZCC_VK_MAX_CACHED_OPS];
    VkPipeline                          cached_pipes[ZCC_VK_MAX_CACHED_OPS];
    char                                cached_sha[ZCC_VK_MAX_CACHED_OPS][65];
    uint32_t                            cached_words[ZCC_VK_MAX_CACHED_OPS];
    vk_buf_alloc_t                      bufs[4];
    int                                 descriptors_bound;
};

static void vk_free_buf(VkDevice dev, const vk_dev_procs_t *dp, vk_buf_alloc_t *b) {
    if (!b) return;
    if (dev && dp) {
        if (b->mapped && b->mem && dp->pUnmapMemory) {
            dp->pUnmapMemory(dev, b->mem);
        }
        if (b->buf && dp->pDestroyBuffer) {
            dp->pDestroyBuffer(dev, b->buf, NULL);
        }
        if (b->mem && dp->pFreeMemory) {
            dp->pFreeMemory(dev, b->mem, NULL);
        }
    }
    memset(b, 0, sizeof(*b));
}

void zcc_vulkan_ctx_destroy(zcc_vulkan_ctx_t *ctx) {
    if (!ctx) return;
    if (ctx->dev) {
        if (ctx->dp.pDeviceWaitIdle) ctx->dp.pDeviceWaitIdle(ctx->dev);
        if (ctx->fence && ctx->dp.pDestroyFence) ctx->dp.pDestroyFence(ctx->dev, ctx->fence, NULL);
        if (ctx->cpool && ctx->dp.pDestroyCPool) ctx->dp.pDestroyCPool(ctx->dev, ctx->cpool, NULL);
        if (ctx->dpool && ctx->dp.pDestroyDPool) ctx->dp.pDestroyDPool(ctx->dev, ctx->dpool, NULL);
        for (uint32_t i = 0U; i < ZCC_VK_MAX_CACHED_OPS; i++) {
            if (ctx->cached_pipes[i] && ctx->dp.pDestroyPipe) {
                ctx->dp.pDestroyPipe(ctx->dev, ctx->cached_pipes[i], NULL);
            }
            if (ctx->cached_smods[i] && ctx->dp.pDestroyShaderModule) {
                ctx->dp.pDestroyShaderModule(ctx->dev, ctx->cached_smods[i], NULL);
            }
        }
        if (ctx->pl && ctx->dp.pDestroyPL) ctx->dp.pDestroyPL(ctx->dev, ctx->pl, NULL);
        if (ctx->dsl && ctx->dp.pDestroyDSL) ctx->dp.pDestroyDSL(ctx->dev, ctx->dsl, NULL);
        for (uint32_t bi = 0U; bi < 4U; bi++) {
            vk_free_buf(ctx->dev, &ctx->dp, &ctx->bufs[bi]);
        }
        if (ctx->dp.pDestroyDevice) ctx->dp.pDestroyDevice(ctx->dev, NULL);
    }
    if (ctx->dbg_msgr && ctx->destroy_dbg) ctx->destroy_dbg(ctx->inst, ctx->dbg_msgr, NULL);
    if (ctx->inst && ctx->vk.destroy_instance) ctx->vk.destroy_instance(ctx->inst, NULL);
    unload_vk_lib(&ctx->vk);
    memset(ctx, 0, sizeof(*ctx));
    free(ctx);
}

zcc_status_t zcc_vulkan_ctx_create(
    int enable_validation_layer,
    uint32_t require_hardware_gpu,
    zcc_vulkan_ctx_t **out_ctx
) {
    if (!out_ctx) return ZCC_STATUS_ERR_NULL_PTR;
    *out_ctx = NULL;

    zcc_vulkan_ctx_t *ctx = (zcc_vulkan_ctx_t *)calloc(1U, sizeof(*ctx));
    if (!ctx) return ZCC_STATUS_ERR_OOM;
    ctx->magic = ZCC_VULKAN_CTX_MAGIC;
    ctx->abi_version = ZCC_COMPUTE_V2_ABI_VERSION;
    ctx->require_hardware_gpu = require_hardware_gpu ? 1U : 0U;
    ctx->caps.struct_size = (uint32_t)sizeof(ctx->caps);
    snprintf(ctx->caps.device_type_str, sizeof(ctx->caps.device_type_str), "NONE");

    if (load_vk_lib(&ctx->vk) != 0) {
        free(ctx);
        return ZCC_STATUS_ERR_BACKEND_UNAVAILABLE;
    }

    int val_avail = has_validation_layer(&ctx->vk);
    int dbg_ext = has_debug_utils_ext(&ctx->vk);
    int use_val = (enable_validation_layer && val_avail) ? 1 : 0;
    int use_dbg = (use_val && dbg_ext) ? 1 : 0;
    ctx->validation_enabled = use_val;

    const char *inst_layers[1] = { "VK_LAYER_KHRONOS_validation" };
    const char *inst_exts[1] = { VK_EXT_DEBUG_UTILS_EXTENSION_NAME };

    VkApplicationInfo app;
    memset(&app, 0, sizeof(app));
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "zcc_track21b";
    app.applicationVersion = 1U;
    app.pEngineName = "zcc_vulkan";
    app.engineVersion = 1U;
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
    dbg_ci.pUserData = &ctx->dbg;

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

    if (ctx->vk.create_instance(&ici, NULL, &ctx->inst) != VK_SUCCESS || !ctx->inst) {
        zcc_vulkan_ctx_destroy(ctx);
        return ZCC_STATUS_ERR_BACKEND_UNAVAILABLE;
    }
    bind_instance_procs(&ctx->vk, ctx->inst);
    if (use_dbg) {
        PFN_vkCreateDebugUtilsMessengerEXT create_dbg =
            (PFN_vkCreateDebugUtilsMessengerEXT)ctx->vk.gipa(ctx->inst, "vkCreateDebugUtilsMessengerEXT");
        ctx->destroy_dbg =
            (PFN_vkDestroyDebugUtilsMessengerEXT)ctx->vk.gipa(ctx->inst, "vkDestroyDebugUtilsMessengerEXT");
        if (create_dbg) create_dbg(ctx->inst, &dbg_ci, NULL, &ctx->dbg_msgr);
    }

    if (pick_physical_device(&ctx->vk, ctx->inst, ctx->require_hardware_gpu, &ctx->phys, &ctx->qfam, &ctx->caps) != 0) {
        zcc_vulkan_ctx_destroy(ctx);
        return ZCC_STATUS_ERR_UNSUPPORTED_HW;
    }
    ctx->caps.validation_layer_available = (uint32_t)val_avail;
    ctx->caps.validation_layer_enabled = (uint32_t)use_val;
    ctx->vk.get_mem_props(ctx->phys, &ctx->mem_props);

    float qprio = 1.0f;
    VkDeviceQueueCreateInfo dqci;
    memset(&dqci, 0, sizeof(dqci));
    dqci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    dqci.queueFamilyIndex = ctx->qfam;
    dqci.queueCount = 1U;
    dqci.pQueuePriorities = &qprio;

    VkDeviceCreateInfo dci;
    memset(&dci, 0, sizeof(dci));
    dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dci.queueCreateInfoCount = 1U;
    dci.pQueueCreateInfos = &dqci;

    if (ctx->vk.create_device(ctx->phys, &dci, NULL, &ctx->dev) != VK_SUCCESS || !ctx->dev) {
        zcc_vulkan_ctx_destroy(ctx);
        return ZCC_STATUS_ERR_BACKEND_RUNTIME;
    }
    if (bind_device_procs(&ctx->vk, ctx->dev, &ctx->dp) != 0) {
        zcc_vulkan_ctx_destroy(ctx);
        return ZCC_STATUS_ERR_BACKEND_RUNTIME;
    }
    ctx->dp.pGetDeviceQueue(ctx->dev, ctx->qfam, 0U, &ctx->queue);

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
    if (ctx->dp.pCreateDSL(ctx->dev, &dslci, NULL, &ctx->dsl) != VK_SUCCESS) {
        zcc_vulkan_ctx_destroy(ctx);
        return ZCC_STATUS_ERR_BACKEND_RUNTIME;
    }

    VkPushConstantRange pcr;
    memset(&pcr, 0, sizeof(pcr));
    pcr.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pcr.offset = 0U;
    pcr.size = (uint32_t)sizeof(zcc_spv_push_t);

    VkPipelineLayoutCreateInfo plci;
    memset(&plci, 0, sizeof(plci));
    plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plci.setLayoutCount = 1U;
    plci.pSetLayouts = &ctx->dsl;
    plci.pushConstantRangeCount = 1U;
    plci.pPushConstantRanges = &pcr;
    if (ctx->dp.pCreatePL(ctx->dev, &plci, NULL, &ctx->pl) != VK_SUCCESS) {
        zcc_vulkan_ctx_destroy(ctx);
        return ZCC_STATUS_ERR_BACKEND_RUNTIME;
    }

    VkDescriptorPoolSize dps = { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4U };
    VkDescriptorPoolCreateInfo dpci;
    memset(&dpci, 0, sizeof(dpci));
    dpci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    dpci.maxSets = 1U;
    dpci.poolSizeCount = 1U;
    dpci.pPoolSizes = &dps;
    if (ctx->dp.pCreateDPool(ctx->dev, &dpci, NULL, &ctx->dpool) != VK_SUCCESS) {
        zcc_vulkan_ctx_destroy(ctx);
        return ZCC_STATUS_ERR_BACKEND_RUNTIME;
    }

    VkDescriptorSetAllocateInfo dsai;
    memset(&dsai, 0, sizeof(dsai));
    dsai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    dsai.descriptorPool = ctx->dpool;
    dsai.descriptorSetCount = 1U;
    dsai.pSetLayouts = &ctx->dsl;
    if (ctx->dp.pAllocDSets(ctx->dev, &dsai, &ctx->dset) != VK_SUCCESS) {
        zcc_vulkan_ctx_destroy(ctx);
        return ZCC_STATUS_ERR_BACKEND_RUNTIME;
    }

    VkCommandPoolCreateInfo cmdpci;
    memset(&cmdpci, 0, sizeof(cmdpci));
    cmdpci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmdpci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cmdpci.queueFamilyIndex = ctx->qfam;
    if (ctx->dp.pCreateCPool(ctx->dev, &cmdpci, NULL, &ctx->cpool) != VK_SUCCESS) {
        zcc_vulkan_ctx_destroy(ctx);
        return ZCC_STATUS_ERR_BACKEND_RUNTIME;
    }

    VkCommandBufferAllocateInfo cbai;
    memset(&cbai, 0, sizeof(cbai));
    cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbai.commandPool = ctx->cpool;
    cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = 1U;
    if (ctx->dp.pAllocCBufs(ctx->dev, &cbai, &ctx->cbuf) != VK_SUCCESS) {
        zcc_vulkan_ctx_destroy(ctx);
        return ZCC_STATUS_ERR_BACKEND_RUNTIME;
    }

    VkFenceCreateInfo fci;
    memset(&fci, 0, sizeof(fci));
    fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    if (ctx->dp.pCreateFence(ctx->dev, &fci, NULL, &ctx->fence) != VK_SUCCESS) {
        zcc_vulkan_ctx_destroy(ctx);
        return ZCC_STATUS_ERR_BACKEND_RUNTIME;
    }

    if (ctx->dbg.error_count > 0U) {
        zcc_vulkan_ctx_destroy(ctx);
        return ZCC_STATUS_ERR_BACKEND_RUNTIME;
    }

    *out_ctx = ctx;
    return ZCC_STATUS_OK;
}

zcc_status_t zcc_vulkan_ctx_get_caps(
    const zcc_vulkan_ctx_t *ctx,
    zcc_vulkan_caps_t *out_caps
) {
    if (!out_caps) return ZCC_STATUS_ERR_NULL_PTR;
    if (!ctx || ctx->magic != ZCC_VULKAN_CTX_MAGIC) return ZCC_STATUS_ERR_INVALID_ARG;
    *out_caps = ctx->caps;
    return ZCC_STATUS_OK;
}

static zcc_status_t vk_compile_shader_and_pipe(
    zcc_vulkan_ctx_t *ctx,
    const uint32_t *spv_words,
    size_t spv_word_count,
    VkShaderModule *out_smod,
    VkPipeline *out_pipe,
    char *err_buf,
    size_t err_cap
) {
    *out_smod = VK_NULL_HANDLE;
    *out_pipe = VK_NULL_HANDLE;

    VkShaderModuleCreateInfo smci;
    memset(&smci, 0, sizeof(smci));
    smci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    smci.codeSize = spv_word_count * sizeof(uint32_t);
    smci.pCode = spv_words;
    if (ctx->dp.pCreateShaderModule(ctx->dev, &smci, NULL, out_smod) != VK_SUCCESS || ctx->dbg.error_count > 0U) {
        if (err_buf && err_cap) snprintf(err_buf, err_cap, "vkCreateShaderModule failed/validation error: %s", ctx->dbg.first_msg);
        if (*out_smod) { ctx->dp.pDestroyShaderModule(ctx->dev, *out_smod, NULL); *out_smod = VK_NULL_HANDLE; }
        return ZCC_STATUS_ERR_BACKEND_RUNTIME;
    }

    VkComputePipelineCreateInfo cpci;
    memset(&cpci, 0, sizeof(cpci));
    cpci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    cpci.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    cpci.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    cpci.stage.module = *out_smod;
    cpci.stage.pName = "main";
    cpci.layout = ctx->pl;
    if (ctx->dp.pCreateComputePipes(ctx->dev, VK_NULL_HANDLE, 1U, &cpci, NULL, out_pipe) != VK_SUCCESS || ctx->dbg.error_count > 0U) {
        if (err_buf && err_cap) snprintf(err_buf, err_cap, "vkCreateComputePipelines failed: %s", ctx->dbg.first_msg);
        if (*out_pipe) { ctx->dp.pDestroyPipe(ctx->dev, *out_pipe, NULL); *out_pipe = VK_NULL_HANDLE; }
        ctx->dp.pDestroyShaderModule(ctx->dev, *out_smod, NULL);
        *out_smod = VK_NULL_HANDLE;
        return ZCC_STATUS_ERR_BACKEND_RUNTIME;
    }
    return ZCC_STATUS_OK;
}

static zcc_status_t vk_ensure_staging_buf(
    zcc_vulkan_ctx_t *ctx,
    uint32_t bi,
    const zcc_tensor_desc_t *td,
    uint32_t align_rem,
    VkDeviceSize nbytes,
    VkDeviceSize req,
    const zcc_vulkan_caps_t *caps,
    int *out_rebound,
    uint32_t *flush_cnt,
    char *err_buf,
    size_t err_cap
) {
    vk_buf_alloc_t *b = &ctx->bufs[bi];
    VkDeviceSize atom = caps->non_coherent_atom_size ? (VkDeviceSize)caps->non_coherent_atom_size : 64ULL;

    if (b->buf == VK_NULL_HANDLE || b->buf_cap < req || !b->mapped) {
        vk_free_buf(ctx->dev, &ctx->dp, b);
        VkDeviceSize target_cap = req;
        if (ctx->descriptors_bound && req * 2ULL >= req && req * 2ULL <= caps->max_storage_buffer_range) {
            target_cap = req * 2ULL;
        }

        VkBufferCreateInfo bci;
        memset(&bci, 0, sizeof(bci));
        bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bci.size = target_cap;
        bci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (ctx->dp.pCreateBuffer(ctx->dev, &bci, NULL, &b->buf) != VK_SUCCESS) {
            if (err_buf && err_cap) snprintf(err_buf, err_cap, "vkCreateBuffer failed");
            return ZCC_STATUS_ERR_BACKEND_RUNTIME;
        }

        VkMemoryRequirements mr;
        ctx->dp.pGetBufMemReq(ctx->dev, b->buf, &mr);
        uint32_t mtype = 0U;
        if (find_mem_type(&ctx->mem_props, mr.memoryTypeBits, &mtype, &b->is_coherent) != 0) {
            vk_free_buf(ctx->dev, &ctx->dp, b);
            if (err_buf && err_cap) snprintf(err_buf, err_cap, "No host-visible memory type available");
            return ZCC_STATUS_ERR_UNSUPPORTED_HW;
        }
        b->alloc_size = align_up_u64(mr.size, atom);
        b->buf_cap = target_cap;

        VkMemoryAllocateInfo mai;
        memset(&mai, 0, sizeof(mai));
        mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        mai.allocationSize = b->alloc_size;
        mai.memoryTypeIndex = mtype;
        if (ctx->dp.pAllocateMemory(ctx->dev, &mai, NULL, &b->mem) != VK_SUCCESS) {
            vk_free_buf(ctx->dev, &ctx->dp, b);
            if (err_buf && err_cap) snprintf(err_buf, err_cap, "vkAllocateMemory failed");
            return ZCC_STATUS_ERR_OOM;
        }
        if (ctx->dp.pBindBufferMemory(ctx->dev, b->buf, b->mem, 0ULL) != VK_SUCCESS) {
            vk_free_buf(ctx->dev, &ctx->dp, b);
            return ZCC_STATUS_ERR_BACKEND_RUNTIME;
        }
        if (ctx->dp.pMapMemory(ctx->dev, b->mem, 0ULL, b->alloc_size, 0U, &b->mapped) != VK_SUCCESS || !b->mapped) {
            vk_free_buf(ctx->dev, &ctx->dp, b);
            return ZCC_STATUS_ERR_BACKEND_RUNTIME;
        }
        *out_rebound = 1;
    }

    b->base_off = align_rem;
    b->data_bytes = nbytes;
    memset(b->mapped, 0, (size_t)req);
    if (td && td->data && td->nbytes > 0ULL) {
        memcpy((uint8_t *)b->mapped + b->base_off, td->data, (size_t)td->nbytes);
    }
    if (!b->is_coherent && ctx->dp.pFlushRanges) {
        VkMappedMemoryRange rng = { VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, NULL, b->mem, 0ULL, VK_WHOLE_SIZE };
        ctx->dp.pFlushRanges(ctx->dev, 1U, &rng);
        (*flush_cnt)++;
    }
    return ZCC_STATUS_OK;
}

static zcc_status_t vk_dispatch_on_ctx(
    zcc_vulkan_ctx_t *ctx,
    zcc_op_t op,
    const zcc_tensor_desc_t *inputs,
    uint32_t num_inputs,
    zcc_tensor_desc_t *output,
    const zcc_vulkan_exec_options_t *opts,
    const zcc_vulkan_caps_t *caps,
    const uint32_t *override_spv_words,
    size_t override_spv_count,
    const char *override_sha_hex,
    const zcc_spv_push_t *in_push,
    uint32_t gx,
    uint32_t gy,
    uint32_t gz,
    char *spirv_sha_out,
    uint32_t *out_word_count,
    uint32_t *out_barriers,
    uint32_t *out_flushes,
    uint32_t *out_invalidates,
    uint32_t *out_val_errors,
    char *err_buf,
    size_t err_cap
) {
    if (!ctx || ctx->magic != ZCC_VULKAN_CTX_MAGIC) {
        if (err_buf && err_cap) snprintf(err_buf, err_cap, "Invalid persistent Vulkan context");
        return ZCC_STATUS_ERR_INVALID_ARG;
    }
    if (opts && opts->require_validation_layer && (!ctx->validation_enabled || !caps->validation_layer_available)) {
        if (err_buf && err_cap) snprintf(err_buf, err_cap, "VK_LAYER_KHRONOS_validation required but missing or disabled");
        return ZCC_STATUS_ERR_BACKEND_UNAVAILABLE;
    }
    if (opts && opts->require_hardware_gpu &&
        ctx->caps.physical_device_type != ZCC_VK_DEVTYPE_DISCRETE_GPU &&
        ctx->caps.physical_device_type != ZCC_VK_DEVTYPE_INTEGRATED_GPU) {
        if (err_buf && err_cap) snprintf(err_buf, err_cap, "Hardware GPU required; persistent context bound non-GPU device");
        return ZCC_STATUS_ERR_UNSUPPORTED_HW;
    }

    ctx->dbg.error_count = 0U;
    ctx->dbg.first_msg[0] = '\0';

    /* Validate all 4 staging buffer bounds up-front before touching persistent buffers */
    const zcc_tensor_desc_t *tds[4] = { NULL, NULL, NULL, NULL };
    uint32_t align_rems[4] = { 0U, 0U, 0U, 0U };
    VkDeviceSize nbytes_arr[4] = { 16ULL, 16ULL, 16ULL, 16ULL };
    VkDeviceSize reqs[4] = { 16ULL, 16ULL, 16ULL, 16ULL };
    for (uint32_t bi = 0U; bi < 4U; bi++) {
        tds[bi] = (bi < num_inputs) ? &inputs[bi] : (bi == 3U ? output : NULL);
        align_rems[bi] = (bi < 3U && tds[bi]) ? (uint32_t)(((uintptr_t)tds[bi]->data) & 3U) : 0U;
        nbytes_arr[bi] = tds[bi] ? (VkDeviceSize)tds[bi]->nbytes : 16ULL;
        reqs[bi] = align_up_u64((VkDeviceSize)align_rems[bi] + nbytes_arr[bi] + 4ULL, 16ULL);
        if (reqs[bi] > caps->max_storage_buffer_range) {
            if (err_buf && err_cap) snprintf(err_buf, err_cap, "Buffer %u exceeds maxStorageBufferRange", bi);
            return ZCC_STATUS_ERR_UNSUPPORTED_HW;
        }
    }

    uint32_t barrier_cnt = 0U, flush_cnt = 0U, inv_cnt = 0U;
    int need_desc_update = ctx->descriptors_bound ? 0 : 1;
    for (uint32_t bi = 0U; bi < 4U; bi++) {
        zcc_status_t bst = vk_ensure_staging_buf(
            ctx, bi, tds[bi], align_rems[bi], nbytes_arr[bi], reqs[bi],
            caps, &need_desc_update, &flush_cnt, err_buf, err_cap
        );
        if (bst != ZCC_STATUS_OK) {
            ctx->descriptors_bound = 0;
            if (out_val_errors) *out_val_errors = ctx->dbg.error_count;
            return bst;
        }
    }

    if (need_desc_update) {
        VkDescriptorBufferInfo dbi[4];
        VkWriteDescriptorSet wds[4];
        memset(dbi, 0, sizeof(dbi));
        memset(wds, 0, sizeof(wds));
        for (uint32_t i = 0U; i < 4U; i++) {
            dbi[i].buffer = ctx->bufs[i].buf;
            dbi[i].offset = 0ULL;
            dbi[i].range = VK_WHOLE_SIZE;
            wds[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            wds[i].dstSet = ctx->dset;
            wds[i].dstBinding = i;
            wds[i].descriptorCount = 1U;
            wds[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            wds[i].pBufferInfo = &dbi[i];
        }
        ctx->dp.pUpdateDSets(ctx->dev, 4U, wds, 0U, NULL);
        ctx->descriptors_bound = 1;
    }

    zcc_spv_push_t push = *in_push;
    push.off_a_bytes = ctx->bufs[0].base_off;
    push.off_b_bytes = ctx->bufs[1].base_off;
    push.off_c_bytes = ctx->bufs[2].base_off;
    push.off_o_bytes = ctx->bufs[3].base_off;

    VkShaderModule tmp_smod = VK_NULL_HANDLE;
    VkPipeline tmp_pipe = VK_NULL_HANDLE;
    VkPipeline active_pipe = VK_NULL_HANDLE;
    zcc_status_t ret = ZCC_STATUS_ERR_BACKEND_RUNTIME;

    if (override_spv_words && override_spv_count > 0U) {
        if (spirv_sha_out && override_sha_hex) snprintf(spirv_sha_out, 65U, "%s", override_sha_hex);
        if (out_word_count) *out_word_count = (uint32_t)override_spv_count;
        ret = vk_compile_shader_and_pipe(ctx, override_spv_words, override_spv_count, &tmp_smod, &tmp_pipe, err_buf, err_cap);
        if (ret != ZCC_STATUS_OK) goto out;
        active_pipe = tmp_pipe;
    } else if ((uint32_t)op < ZCC_VK_MAX_CACHED_OPS) {
        uint32_t opx = (uint32_t)op;
        if (ctx->cached_pipes[opx] == VK_NULL_HANDLE) {
            zcc_spv_module_t spv;
            memset(&spv, 0, sizeof(spv));
            if (zcc_spirv_emit_op_kernel(opx, &spv) != 0) {
                if (err_buf && err_cap) snprintf(err_buf, err_cap, "Failed emitting SPIR-V kernel");
                goto out;
            }
            zcc_sha256_hex(spv.words, spv.word_count * sizeof(uint32_t), ctx->cached_sha[opx]);
            ctx->cached_words[opx] = (uint32_t)spv.word_count;
            ret = vk_compile_shader_and_pipe(ctx, spv.words, spv.word_count, &ctx->cached_smods[opx], &ctx->cached_pipes[opx], err_buf, err_cap);
            zcc_spirv_module_free(&spv);
            if (ret != ZCC_STATUS_OK) goto out;
        }
        if (spirv_sha_out) snprintf(spirv_sha_out, 65U, "%s", ctx->cached_sha[opx]);
        if (out_word_count) *out_word_count = ctx->cached_words[opx];
        active_pipe = ctx->cached_pipes[opx];
    } else {
        if (err_buf && err_cap) snprintf(err_buf, err_cap, "Unsupported opcode for Vulkan compute");
        ret = ZCC_STATUS_ERR_INVALID_OP;
        goto out;
    }

    ret = ZCC_STATUS_ERR_BACKEND_RUNTIME;
    if (ctx->dp.pResetFences(ctx->dev, 1U, &ctx->fence) != VK_SUCCESS) goto out;
    if (ctx->dp.pResetCPool(ctx->dev, ctx->cpool, 0U) != VK_SUCCESS) goto out;

    VkCommandBufferBeginInfo cbbi;
    memset(&cbbi, 0, sizeof(cbbi));
    cbbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cbbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (ctx->dp.pBeginCBuf(ctx->cbuf, &cbbi) != VK_SUCCESS) goto out;

    /* Explicit Barrier 1: Host write -> Compute shader read/write visibility */
    VkMemoryBarrier pre_bar;
    memset(&pre_bar, 0, sizeof(pre_bar));
    pre_bar.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    pre_bar.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
    pre_bar.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    ctx->dp.pCmdBarrier(ctx->cbuf, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        0U, 1U, &pre_bar, 0U, NULL, 0U, NULL);
    barrier_cnt++;

    ctx->dp.pCmdBindPipe(ctx->cbuf, VK_PIPELINE_BIND_POINT_COMPUTE, active_pipe);
    ctx->dp.pCmdBindDSets(ctx->cbuf, VK_PIPELINE_BIND_POINT_COMPUTE, ctx->pl, 0U, 1U, &ctx->dset, 0U, NULL);
    ctx->dp.pCmdPush(ctx->cbuf, ctx->pl, VK_SHADER_STAGE_COMPUTE_BIT, 0U, (uint32_t)sizeof(push), &push);
    ctx->dp.pCmdDispatch(ctx->cbuf, gx, gy, gz);

    /* Explicit Barrier 2: Compute shader write -> Host read visibility */
    VkMemoryBarrier post_bar;
    memset(&post_bar, 0, sizeof(post_bar));
    post_bar.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    post_bar.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    post_bar.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    ctx->dp.pCmdBarrier(ctx->cbuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                        0U, 1U, &post_bar, 0U, NULL, 0U, NULL);
    barrier_cnt++;

    if (ctx->dp.pEndCBuf(ctx->cbuf) != VK_SUCCESS) goto out;

    VkSubmitInfo si;
    memset(&si, 0, sizeof(si));
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1U;
    si.pCommandBuffers = &ctx->cbuf;
    if (ctx->dp.pQueueSubmit(ctx->queue, 1U, &si, ctx->fence) != VK_SUCCESS) goto out;

    if (ctx->dp.pWaitForFences(ctx->dev, 1U, &ctx->fence, VK_TRUE, 30000000000ULL) != VK_SUCCESS) {
        if (err_buf && err_cap) snprintf(err_buf, err_cap, "vkWaitForFences timed out or failed");
        ctx->dp.pDeviceWaitIdle(ctx->dev);
        goto out;
    }

    if (!ctx->bufs[3].is_coherent && ctx->dp.pInvalidateRanges) {
        VkMappedMemoryRange rng = { VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, NULL, ctx->bufs[3].mem, 0ULL, VK_WHOLE_SIZE };
        ctx->dp.pInvalidateRanges(ctx->dev, 1U, &rng);
        inv_cnt++;
    }
    memcpy(output->data, (const uint8_t *)ctx->bufs[3].mapped + ctx->bufs[3].base_off, (size_t)output->nbytes);

    if (ctx->dbg.error_count > 0U) {
        if (err_buf && err_cap) snprintf(err_buf, err_cap, "Vulkan validation errors (%u): %s", ctx->dbg.error_count, ctx->dbg.first_msg);
        goto out;
    }

    ret = ZCC_STATUS_OK;

out:
    if (tmp_pipe) ctx->dp.pDestroyPipe(ctx->dev, tmp_pipe, NULL);
    if (tmp_smod) ctx->dp.pDestroyShaderModule(ctx->dev, tmp_smod, NULL);
    if (out_barriers) *out_barriers = barrier_cnt;
    if (out_flushes) *out_flushes = flush_cnt;
    if (out_invalidates) *out_invalidates = inv_cnt;
    if (out_val_errors) *out_val_errors = ctx->dbg.error_count;
    return ret;
}

zcc_status_t zcc_vulkan_exec_on_ctx(
    zcc_vulkan_ctx_t *ctx,
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

    const uint32_t *ov_words = NULL;
    size_t ov_count = 0U;
    char ov_sha[65];
    ov_sha[0] = '\0';
    if (opts && opts->spirv_override_words && opts->spirv_override_word_count > 0U) {
        ov_words = opts->spirv_override_words;
        ov_count = opts->spirv_override_word_count;
        zcc_sha256_hex(ov_words, ov_count * sizeof(uint32_t), ov_sha);
        if (spirv_sha_out) snprintf(spirv_sha_out, 65U, "%s", ov_sha);
        if (out_word_count) *out_word_count = (uint32_t)ov_count;
    }

    uint32_t gx = 1U, gy = 1U, gz = 1U;
    zcc_spirv_dispatch_grid((uint32_t)op, push.dim_m, push.dim_n, push.dim_k, &gx, &gy, &gz);
    if (gx > caps->max_workgroup_count[0] || gy > caps->max_workgroup_count[1] || gz > caps->max_workgroup_count[2]) {
        if (err_buf && err_cap) snprintf(err_buf, err_cap, "Dispatch grid exceeds maxComputeWorkGroupCount");
        return ZCC_STATUS_ERR_UNSUPPORTED_HW;
    }

    if (opts && opts->require_validation_layer && !caps->validation_layer_available) {
        if (err_buf && err_cap) snprintf(err_buf, err_cap, "VK_LAYER_KHRONOS_validation required but missing or restricted");
        return ZCC_STATUS_ERR_BACKEND_UNAVAILABLE;
    }

    if (ctx) {
        return vk_dispatch_on_ctx(
            ctx, op, inputs, num_inputs, output, opts, caps,
            ov_words, ov_count, ov_sha, &push, gx, gy, gz,
            spirv_sha_out, out_word_count,
            out_barriers, out_flushes, out_invalidates, out_val_errors,
            err_buf, err_cap
        );
    }

    zcc_vulkan_ctx_t *tmp_ctx = NULL;
    uint32_t req_gpu = (opts && opts->require_hardware_gpu) ? 1U : 0U;
    zcc_status_t cst = zcc_vulkan_ctx_create(1, req_gpu, &tmp_ctx);
    if (cst != ZCC_STATUS_OK || !tmp_ctx) {
        if (err_buf && err_cap) snprintf(err_buf, err_cap, cst == ZCC_STATUS_ERR_UNSUPPORTED_HW ? "No qualifying Vulkan physical device" : "Unable to initialize Vulkan context");
        return cst == ZCC_STATUS_OK ? ZCC_STATUS_ERR_BACKEND_UNAVAILABLE : cst;
    }

    zcc_status_t dst = vk_dispatch_on_ctx(
        tmp_ctx, op, inputs, num_inputs, output, opts, caps,
        ov_words, ov_count, ov_sha, &push, gx, gy, gz,
        spirv_sha_out, out_word_count,
        out_barriers, out_flushes, out_invalidates, out_val_errors,
        err_buf, err_cap
    );
    zcc_vulkan_ctx_destroy(tmp_ctx);
    return dst;
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
    return zcc_vulkan_exec_on_ctx(
        NULL, op, inputs, num_inputs, output, opts, caps,
        spirv_sha_out, out_word_count,
        out_barriers, out_flushes, out_invalidates, out_val_errors,
        err_buf, err_cap
    );



}
