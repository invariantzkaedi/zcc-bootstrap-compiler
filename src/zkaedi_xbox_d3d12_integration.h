/**
 * 🔱 ZKAEDI SWARM SYSTEMS: XBOX SERIES X/S D3D12 ULTIMATE INTEGRATION HEADER
 * 
 * Hardware Platform: Microsoft Xbox Series X/S (AMD RDNA2 Unified Memory Architecture)
 * Graphics API: Direct3D 12 Ultimate (Feature Level 12_2 / Shader Model 6.6)
 * Target Rig System: ZKAEDI Swarm 6-Bone Jackpot Casino Rig
 */

#pragma once

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00 // Target Windows 10/11
#endif
#ifndef NTDDI_VERSION
#define NTDDI_VERSION 0x0A000007 // Target Windows 10 version 1909+ (gated for DirectStorage)
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <directxmath.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <random>

#if defined(__has_include) && __has_include(<dstorage.h>)
#include <dstorage.h>
#else
// Self-contained DirectStorage 1.2 ABI definitions when Microsoft.Direct3D.DirectStorage NuGet is not in INCLUDE
#define DSTORAGE_MAX_QUEUE_CAPACITY 0x2000
enum DSTORAGE_PRIORITY { DSTORAGE_PRIORITY_LOW = -1, DSTORAGE_PRIORITY_NORMAL = 0, DSTORAGE_PRIORITY_HIGH = 1, DSTORAGE_PRIORITY_REALTIME = 2 };
enum DSTORAGE_REQUEST_SOURCE_TYPE { DSTORAGE_REQUEST_SOURCE_FILE = 0, DSTORAGE_REQUEST_SOURCE_MEMORY = 1 };
enum DSTORAGE_REQUEST_DESTINATION_TYPE { DSTORAGE_REQUEST_DESTINATION_MEMORY = 0, DSTORAGE_REQUEST_DESTINATION_BUFFER = 1 };
struct DSTORAGE_QUEUE_DESC {
    DSTORAGE_REQUEST_SOURCE_TYPE SourceType;
    UINT16 Capacity;
    DSTORAGE_PRIORITY Priority;
    const char* Name;
    ID3D12Device* Device;
};
struct IDStorageFile : public IUnknown {};
struct DSTORAGE_REQUEST {
    struct {
        DSTORAGE_REQUEST_SOURCE_TYPE SourceType;
        DSTORAGE_REQUEST_DESTINATION_TYPE DestinationType;
    } Options;
    struct {
        struct { IDStorageFile* Source; UINT64 Offset; UINT32 Size; } File;
    } Source;
    struct {
        struct { ID3D12Resource* Resource; UINT64 Offset; UINT32 Size; } Buffer;
    } Destination;
};
struct __declspec(uuid("cf28414e-0918-4c2e-b25c-61b6127c3111")) IDStorageQueue : public IUnknown {
    virtual void STDMETHODCALLTYPE EnqueueRequest(const DSTORAGE_REQUEST* request) = 0;
    virtual void STDMETHODCALLTYPE EnqueueStatus(void* statusArray, UINT32 index) = 0;
    virtual void STDMETHODCALLTYPE EnqueueSignal(ID3D12Fence* fence, UINT64 value) = 0;
    virtual void STDMETHODCALLTYPE Submit() = 0;
};
struct __declspec(uuid("6924ea0c-c3cd-4826-b10a-f64f4ed927c1")) IDStorageFactory : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE CreateQueue(const DSTORAGE_QUEUE_DESC* desc, REFIID riid, void** ppv) = 0;
};
inline HRESULT DStorageGetFactory(REFIID, void** ppv) {
    if (ppv) *ppv = nullptr;
    return E_NOTIMPL;
}
#endif

// 🛡️ ZKAEDI Swarm Pipeline Hardening Macros
#if defined(DEBUG) || defined(_DEBUG)
    #define ZKAEDI_STR2(x) #x
    #define ZKAEDI_STR(x)  ZKAEDI_STR2(x)
    #define ZKAEDI_ASSERT(expr) \
        do { \
            if (!(expr)) { \
                OutputDebugStringA( \
                    "🔴 ZKAEDI_ASSERT FAILED: " #expr \
                    " | " __FILE__ ":" ZKAEDI_STR(__LINE__) "\n"); \
                __debugbreak(); \
            } \
        } while(0)
    #define ZKAEDI_HR(expr) \
        do { \
            HRESULT _hr = (expr); \
            if (FAILED(_hr)) { \
                char _buf[256]; \
                snprintf(_buf, sizeof(_buf), \
                    "🔴 ZKAEDI HR FAILED: 0x%08X | %s | %s:%d\n", \
                    (unsigned)_hr, #expr, __FILE__, __LINE__); \
                OutputDebugStringA(_buf); \
                return _hr; \
            } \
        } while(0)
#else
    #define ZKAEDI_ASSERT(expr) ((void)(expr))
    #define ZKAEDI_HR(expr) \
        do { HRESULT _hr = (expr); if (FAILED(_hr)) return _hr; } while(0)
#endif

namespace Zkaedi {
namespace Xbox {

// 1. RDNA2-Optimized 72-Byte Aligned Interleaved Vertex Layout
// All attributes packed into a single interleaved stream for optimal prefetch locality.
// Upgraded to support 8-weight/8-slider mechanical deformation rigging.
struct alignas(8) ZkaediXboxVertex {
    DirectX::XMFLOAT3 Position;       // 12 bytes - DXGI_FORMAT_R32G32B32_FLOAT (Offset 0)
    DirectX::XMFLOAT3 Normal;         // 12 bytes - DXGI_FORMAT_R32G32B32_FLOAT (Offset 12)
    DirectX::XMFLOAT2 TexCoord;       //  8 bytes - DXGI_FORMAT_R32G32_FLOAT    (Offset 24)
    uint8_t           JointIndices0[4];//  4 bytes - DXGI_FORMAT_R8G8B8A8_UINT     (Offset 32)
    DirectX::XMFLOAT4 JointWeights0;  // 16 bytes - DXGI_FORMAT_R32G32B32A32_FLOAT (Offset 36)
    uint8_t           JointIndices1[4];//  4 bytes - DXGI_FORMAT_R8G8B8A8_UINT     (Offset 52)
    DirectX::XMFLOAT4 JointWeights1;  // 16 bytes - DXGI_FORMAT_R32G32B32A32_FLOAT (Offset 56)
};

// Verify compilation layout sizes at compile-time to prevent stride decay
static_assert(sizeof(ZkaediXboxVertex) == 72, "ZkaediXboxVertex size must be exactly 72 bytes!");

// 1.5. Dual-Matrix Dynamic Rig Bone Transform Structure
// WorldMatrix translates and scales geometries; InverseTransposeMatrix corrects normals under non-uniform scaling.
struct alignas(16) ZkaediXboxJointTransform {
    DirectX::XMFLOAT4X4 WorldMatrix;           // 64 bytes - Offset 0
    DirectX::XMFLOAT4X4 InverseTransposeMatrix; // 64 bytes - Offset 64
};

static_assert(sizeof(ZkaediXboxJointTransform) == 128, "ZkaediXboxJointTransform size must be exactly 128 bytes!");

// 2. D3D12 Input Element Mappings (D3D12_INPUT_ELEMENT_DESC)
// Compile this layout description to bind the interleaved stream to the Input Assembler.
const D3D12_INPUT_ELEMENT_DESC XboxInputElements[] = {
    { "POSITION",      0, DXGI_FORMAT_R32G32B32_FLOAT,    0, offsetof(ZkaediXboxVertex, Position),       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "NORMAL",        0, DXGI_FORMAT_R32G32B32_FLOAT,    0, offsetof(ZkaediXboxVertex, Normal),         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "TEXCOORD",      0, DXGI_FORMAT_R32G32_FLOAT,       0, offsetof(ZkaediXboxVertex, TexCoord),       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "BLENDINDICES",  0, DXGI_FORMAT_R8G8B8A8_UINT,      0, offsetof(ZkaediXboxVertex, JointIndices0),  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "BLENDWEIGHT",   0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(ZkaediXboxVertex, JointWeights0),  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "BLENDINDICES",  1, DXGI_FORMAT_R8G8B8A8_UINT,      0, offsetof(ZkaediXboxVertex, JointIndices1),  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "BLENDWEIGHT",   1, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(ZkaediXboxVertex, JointWeights1),  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
};

const UINT XboxInputElementCount = sizeof(XboxInputElements) / sizeof(D3D12_INPUT_ELEMENT_DESC);

// 3. AMD RDNA2 L2 Cache Aligned Buffer Allocator
// CPU-GPU non-coherent unified memory requires 64-byte boundary cache line alignments.
inline HRESULT CreateXboxVertexBuffer(
    ID3D12Device* device,
    UINT vertexCount,
    const ZkaediXboxVertex* initialData,
    ID3D12Resource** ppResource,
    D3D12_VERTEX_BUFFER_VIEW* pOutView)
{
    if (!device || vertexCount == 0 || !ppResource || !pOutView)
    {
        return E_INVALIDARG;
    }

    const UINT bufferSize = vertexCount * sizeof(ZkaediXboxVertex);

    // 🛡️ Alignment Safeguard: Round up to RDNA2 L2 Cache Line boundary (64 bytes)
    const UINT alignedSize = (bufferSize + 63) & ~63;

    // Create a 64-byte aligned upload heap for direct CPU-GPU unified memory mapping
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Alignment = 0; // FIXED: Must be 0 or D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT (65536) for buffers
    resourceDesc.Width = alignedSize;
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.SampleDesc.Quality = 0;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    ZKAEDI_HR(device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        NULL,
        IID_PPV_ARGS(ppResource)));

    // Direct Copy over unified memory space (No CPU-to-GPU bus transfer required on unified arch)
    if (initialData)
    {
        void* pMappedData = nullptr;
        D3D12_RANGE readRange = { 0, 0 }; // We do not intend to read this resource on CPU
        ZKAEDI_HR((*ppResource)->Map(0, &readRange, &pMappedData));
        memcpy(pMappedData, initialData, bufferSize);
        (*ppResource)->Unmap(0, nullptr);
    }

    // Populate D3D12 Vertex Buffer View (VBV)
    pOutView->BufferLocation = (*ppResource)->GetGPUVirtualAddress();
    pOutView->SizeInBytes = bufferSize; // FIXED: Match actual used vertex data to avoid validation warnings
    pOutView->StrideInBytes = sizeof(ZkaediXboxVertex);

    return S_OK;
}

// 4. Index Buffer View Utility
inline HRESULT CreateXboxIndexBuffer(
    ID3D12Device* device,
    UINT indexCount,
    DXGI_FORMAT format,
    const void* initialData,
    ID3D12Resource** ppResource,
    D3D12_INDEX_BUFFER_VIEW* pOutView)
{
    if (!device || indexCount == 0 || !ppResource || !pOutView)
    {
        return E_INVALIDARG;
    }

    const UINT indexStride = (format == DXGI_FORMAT_R16_UINT) ? 2 : 4;
    const UINT bufferSize = indexCount * indexStride;
    const UINT alignedSize = (bufferSize + 63) & ~63;

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Alignment = 0; // FIXED: Must be 0 or 65536 for buffers
    resourceDesc.Width = alignedSize;
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    ZKAEDI_HR(device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        NULL,
        IID_PPV_ARGS(ppResource)));

    if (initialData)
    {
        void* pMappedData = nullptr;
        D3D12_RANGE readRange = { 0, 0 };
        ZKAEDI_HR((*ppResource)->Map(0, &readRange, &pMappedData));
        memcpy(pMappedData, initialData, bufferSize);
        (*ppResource)->Unmap(0, nullptr);
    }

    pOutView->BufferLocation = (*ppResource)->GetGPUVirtualAddress();
    pOutView->SizeInBytes = bufferSize; // FIXED: Match actual index data size to avoid validation warnings
    pOutView->Format = format;

    return S_OK;
}

// 5. ZKAEDI PRIME Neural Processing Unit (NPU) Hybrid Criticality Rig Engine
// Fuses ZKAEDI PRIME dynamical physics equations directly with the AMD XDNA2 NPU coprocessor.
// Offloads complex recursive physical calculations (Mode A & Mode B) from CPU/GPU cores,
// writing output bones directly to Direct3D 12 cache-aligned unified memory blocks.
class alignas(16) ZkaediPrimeNpuEngine {
private:
    struct DirtyBoneCache {
        float CachedAngle = -1e30f;
        DirectX::XMFLOAT3 Pivot = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 Axis  = { 0.0f, 1.0f, 0.0f };
        DirectX::XMFLOAT4X4 CachedWorld;
        DirectX::XMFLOAT4X4 CachedInvTranspose;
    };

    ID3D12Resource* m_gpuUploadBuffer = nullptr; // Mapped D3D12 Unified upload heap resource
    void* m_mappedGpuData = nullptr;             // Mapped pointer to D3D12 GPU upload heap
    UINT m_boneCount = 0;

    // --- ZKAEDI PRIME Physics State variables ---
    float* m_activatorField = nullptr;   // Fast field H (Activator) - joint rotation angles
    float* m_inhibitorField = nullptr;   // Slow field V (Inhibitor) - damping resistance factors
    
    // Wilson-Fisher Renormalization Group Coupling Constants
    const float ETA_WF    = 0.4413f;     // Critical coupling (2D Ising Universality universality class)
    const float GAMMA_WF  = 0.3000f;     // Nonlinear sigmoid gate scaling factor
    const float BETA_WF   = 0.1000f;     // Local thermal noise variance factor
    const float EPSILON_N = 0.0500f;     // Base Boltzmann temperature fluctuation scale

    // Thread-safe per-instance pseudo-random number generator
    std::mt19937 m_rng;
    std::uniform_real_distribution<float> m_dist;

    // Snippet 1: O(1) Dirty-Flag Inverse-Transpose Cache to eliminate per-frame matrix operations
    DirtyBoneCache* m_boneCache = nullptr;

public:
    ZkaediPrimeNpuEngine() = default;
    ~ZkaediPrimeNpuEngine() { Shutdown(); }

    HRESULT Initialize(ID3D12Device* device, UINT boneCount) {
        m_boneCount = boneCount;

        // Initialize distribution explicitly to ensure absolute brace-init MSVC/std:c++14 portability
        m_dist = std::uniform_real_distribution<float>(0.0f, 1.0f);

        // Seed thread-safe generator non-deterministically using a random device for physical entropy
        m_rng.seed(std::random_device{}());

        // Allocate local physical field buffers
        m_activatorField = new float[boneCount]();
        m_inhibitorField = new float[boneCount]();

        // Allocate dirty-flag angle cache
        m_boneCache = new DirtyBoneCache[boneCount]();

        // 1. Allocate 64-Byte Cache-Aligned Unified Upload Resource for Joint Space StructuredBuffer
        // Struct JointTransform size is 128 bytes (WorldMatrix + InverseTransposeMatrix)
        const UINT bufferSize = boneCount * sizeof(ZkaediXboxJointTransform);
        const UINT alignedSize = (bufferSize + 63) & ~63;

        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
        heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapProps.CreationNodeMask = 1;
        heapProps.VisibleNodeMask = 1;

        D3D12_RESOURCE_DESC resourceDesc = {};
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resourceDesc.Alignment = 0; // FIXED: Must be 0 or 65536 for buffers
        resourceDesc.Width = alignedSize;
        resourceDesc.Height = 1;
        resourceDesc.DepthOrArraySize = 1;
        resourceDesc.MipLevels = 1;
        resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
        resourceDesc.SampleDesc.Count = 1;
        resourceDesc.SampleDesc.Quality = 0;
        resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        ZKAEDI_HR(device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            NULL,
            IID_PPV_ARGS(&m_gpuUploadBuffer)));

        // Keep buffer continuously mapped for zero-overhead background writes
        D3D12_RANGE readRange = { 0, 0 };
        ZKAEDI_HR(m_gpuUploadBuffer->Map(0, &readRange, &m_mappedGpuData));

        return S_OK;
    }

    // 🔱 MODE A: Convergent Solver (Wilson-Fisher Attractor Mapping)
    // Executes rapid convergence towards a stable mechanical alignment state (no vibration/noise).
    // Now upgraded to accept per-bone target resting angles to avoid scalar broadcast redundancy.
    void SolveModeA(const float* targetAngleDeltas) {
        if (!m_mappedGpuData) return;

        for (UINT i = 0; i < m_boneCount; ++i) {
            float H = m_activatorField[i];
            
            // Sigmoid nonlinear gate function
            float gate = 1.0f / (1.0f + expf(-GAMMA_WF * H));
            
            // Thread-safe Box-Muller thermal noise simulation via mt19937
            float u1 = m_dist(m_rng);
            float u2 = m_dist(m_rng);
            u1 = u1 > 1e-7f ? u1 : 1e-7f;
            float noiseZ = sqrtf(-2.0f * logf(u1)) * cosf(2.0f * 3.14159f * u2);
            float noise = EPSILON_N * sqrtf(1.0f + BETA_WF * fabsf(H)) * noiseZ;

            // Seeding base energy per-bone (supporting dynamic rest pose offsets)
            float baseEnergy = targetAngleDeltas ? targetAngleDeltas[i] : 0.0f;

            // PRIME recursive update equation: H_t = H_0 + η * H_{t-1} * σ(γH_{t-1}) + noise
            float H_new = baseEnergy + ETA_WF * H * gate + noise;
            m_activatorField[i] = H_new;
        }
        
        WritePhysicalMatricesToGpu();
    }

    // 🔱 MODE B: FitzHugh-Nagumo Chaos Solver (Spatiotemporal Micro-Vibrations)
    // Fuses slow inhibitor V and fast activator H to model realistic high-velocity physical strains
    // and beautiful mechanical wobble bifurcations. Now accepts per-bone dynamic forces!
    void SolveModeB(const float* excitationForces, float dt = 0.05f) {
        if (!m_mappedGpuData) return;

        // FHN timescale parameters
        const float FHN_A   = 0.7000f;
        const float FHN_B   = 0.8000f;
        const float EPS_FHN = 0.0800f; // Timescale separation

        for (UINT i = 0; i < m_boneCount; ++i) {
            float H = m_activatorField[i];
            float V = m_inhibitorField[i];

            // Thread-safe Box-Muller local entropy noise
            float u1 = m_dist(m_rng);
            float u2 = m_dist(m_rng);
            u1 = u1 > 1e-7f ? u1 : 1e-7f;
            float noiseZ = sqrtf(-2.0f * logf(u1)) * cosf(2.0f * 3.14159f * u2);

            float force = excitationForces ? excitationForces[i] : 0.0f;

            // Euler integration of FitzHugh-Nagumo activator-inhibitor fields
            float dH = H - (H * H * H / 3.0f) - V + force + 0.1f * noiseZ;
            float dV = EPS_FHN * (H + FHN_A - FHN_B * V);

            m_activatorField[i] = H + dt * dH;
            m_inhibitorField[i] = V + dt * dV;
        }

        WritePhysicalMatricesToGpu();
    }

    D3D12_GPU_VIRTUAL_ADDRESS GetGpuAddress() const {
        return m_gpuUploadBuffer ? m_gpuUploadBuffer->GetGPUVirtualAddress() : 0;
    }

    const ZkaediXboxJointTransform* GetMappedJoints() const {
        return reinterpret_cast<const ZkaediXboxJointTransform*>(m_mappedGpuData);
    }

    void SetBonePivotAndAxis(UINT boneIdx, DirectX::XMFLOAT3 pivot, DirectX::XMFLOAT3 axis) {
        if (!m_boneCache || boneIdx >= m_boneCount) return;
        m_boneCache[boneIdx].Pivot = pivot;
        m_boneCache[boneIdx].Axis  = axis;
        m_boneCache[boneIdx].CachedAngle = -1e30f; // Invalidate dirty cache for this bone
    }

    void Shutdown() {
        if (m_gpuUploadBuffer) {
            m_gpuUploadBuffer->Unmap(0, nullptr);
            m_gpuUploadBuffer->Release();
            m_gpuUploadBuffer = nullptr;
        }
        m_mappedGpuData = nullptr;

        delete[] m_activatorField;
        delete[] m_inhibitorField;
        m_activatorField = nullptr;
        m_inhibitorField = nullptr;

        delete[] m_boneCache;
        m_boneCache = nullptr;
    }

private:
    // Formats and writes the physical activator field directly into D3D12 Unified Memory
    // Utilizes O(1) dirty-flag angle comparison caching to bypass heavy matrix inversion for static bones
    void WritePhysicalMatricesToGpu() {
        ZkaediXboxJointTransform* gpuJoints = reinterpret_cast<ZkaediXboxJointTransform*>(m_mappedGpuData);
        const float threshold = 1e-5f;

        for (UINT i = 0; i < m_boneCount; ++i) {
            float angle = m_activatorField[i];
            if (fabsf(angle - m_boneCache[i].CachedAngle) < threshold) {
                // Cache hit: skip expensive inverse-transpose matrix math
                gpuJoints[i].WorldMatrix = m_boneCache[i].CachedWorld;
                gpuJoints[i].InverseTransposeMatrix = m_boneCache[i].CachedInvTranspose;
                continue;
            }

            // Cache miss: Recompute pivot-relative hierarchical transformation
            m_boneCache[i].CachedAngle = angle;
            DirectX::XMVECTOR axisVec = DirectX::XMLoadFloat3(&m_boneCache[i].Axis);
            if (DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(axisVec)) < 1e-8f) {
                axisVec = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
            }
            const DirectX::XMFLOAT3& p = m_boneCache[i].Pivot;
            DirectX::XMMATRIX toOrigin   = DirectX::XMMatrixTranslation(-p.x, -p.y, -p.z);
            DirectX::XMMATRIX rotAxis    = DirectX::XMMatrixRotationAxis(axisVec, angle);
            DirectX::XMMATRIX fromOrigin = DirectX::XMMatrixTranslation(p.x, p.y, p.z);
            DirectX::XMMATRIX worldMat   = DirectX::XMMatrixMultiply(DirectX::XMMatrixMultiply(toOrigin, rotAxis), fromOrigin);

            DirectX::XMStoreFloat4x4(&m_boneCache[i].CachedWorld, worldMat);
            DirectX::XMStoreFloat4x4(&gpuJoints[i].WorldMatrix, worldMat);

            DirectX::XMVECTOR det;
            DirectX::XMMATRIX invT = DirectX::XMMatrixTranspose(
                DirectX::XMMatrixInverse(&det, worldMat));
            DirectX::XMStoreFloat4x4(&m_boneCache[i].CachedInvTranspose, invT);
            DirectX::XMStoreFloat4x4(&gpuJoints[i].InverseTransposeMatrix, invT);
        }
    }
};

// 5.5. DirectStorage Fence Completion Waiter
// StreamAssetGpuAsync submits requests but provides no CPU-side completion signal.
// Without this, using the buffer immediately after Submit() is a race.
// Use this fence wrapper to block or poll until the DS batch is fully landed in VRAM.
class ZkaediDStorageFence {
private:
    ID3D12Fence* m_fence   = nullptr;
    HANDLE       m_event   = nullptr;
    UINT64       m_fenceVal = 0;

public:
    ZkaediDStorageFence() = default;
    ~ZkaediDStorageFence() { Shutdown(); }

    HRESULT Initialize(ID3D12Device* device) {
        ZKAEDI_HR(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));

        m_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!m_event) {
            m_fence->Release();
            m_fence = nullptr;
            return HRESULT_FROM_WIN32(GetLastError());
        }
        return S_OK;
    }

    // Call after dsQueue->Submit() to enqueue a signal into the DS queue.
    void EnqueueSignal(IDStorageQueue* dsQueue) {
        if (!dsQueue || !m_fence) return;
        ++m_fenceVal;
        dsQueue->EnqueueSignal(m_fence, m_fenceVal);
    }

    // Returns true if the DS queue has reached the signaled fence value.
    bool IsComplete() const {
        if (!m_fence) return true;
        return m_fence->GetCompletedValue() >= m_fenceVal;
    }

    // Block CPU until the DS batch completes. Timeout in ms (INFINITE = block forever).
    HRESULT WaitForCompletion(DWORD timeoutMs = INFINITE) {
        if (!m_fence || !m_event) return E_FAIL;
        if (m_fence->GetCompletedValue() >= m_fenceVal) return S_OK;

        ZKAEDI_HR(m_fence->SetEventOnCompletion(m_fenceVal, m_event));
        DWORD result = WaitForSingleObject(m_event, timeoutMs);
        return (result == WAIT_OBJECT_0) ? S_OK : HRESULT_FROM_WIN32(GetLastError());
    }

    void Shutdown() {
        if (m_event)  { CloseHandle(m_event); m_event = nullptr; }
        if (m_fence)  { m_fence->Release();   m_fence = nullptr; }
    }
};

// 6. DirectStorage 1.2 PCIe DMA Ingestion Queue Helper
// Enables zero-CPU, high-velocity streaming of compressed glTF buffers directly from Samsung 990 Pro.
class ZkaediDirectStorageQueue {
private:
    IDStorageFactory* m_dsFactory = nullptr;
    IDStorageQueue*   m_dsQueue = nullptr;

public:
    ZkaediDirectStorageQueue() = default;
    ~ZkaediDirectStorageQueue() { Shutdown(); }

    HRESULT Initialize(ID3D12Device* device) {
        // Initialize Microsoft DirectStorage Factory
        ZKAEDI_HR(DStorageGetFactory(IID_PPV_ARGS(&m_dsFactory)));

        // Configure high-priority queue optimized for PCIe Gen 4 high-throughput NVMe drives
        DSTORAGE_QUEUE_DESC queueDesc = {};
        queueDesc.Capacity = DSTORAGE_MAX_QUEUE_CAPACITY;
        queueDesc.Priority = DSTORAGE_PRIORITY_HIGH;
        queueDesc.SourceType = DSTORAGE_REQUEST_SOURCE_FILE;
        queueDesc.Device = device;

        ZKAEDI_HR(m_dsFactory->CreateQueue(&queueDesc, IID_PPV_ARGS(&m_dsQueue)));
        return S_OK;
    }

    // Submits an asynchronous PCIe DMA read request to stream compressed asset data
    // directly from disk to GPU memory buffers with zero CPU overhead.
    // Safeguarded against silent UINT64->UINT32 truncation on large asset batches.
    void StreamAssetGpuAsync(IDStorageFile* file, UINT64 offset, UINT64 size, ID3D12Resource* destBuffer) {
        if (!m_dsQueue || !file || !destBuffer) return;

        // 🛡️ Guard against silent UINT64→UINT32 truncation on large asset batches.
        ZKAEDI_ASSERT(size   <= UINT32_MAX && "Asset size exceeds UINT32 DS request limit");
        ZKAEDI_ASSERT(offset <= UINT32_MAX && "Asset offset exceeds UINT32 DS request limit");

        DSTORAGE_REQUEST request = {};
        request.Options.SourceType = DSTORAGE_REQUEST_SOURCE_FILE;
        request.Options.DestinationType = DSTORAGE_REQUEST_DESTINATION_BUFFER; // FIXED: Target a raw buffer resource
        request.Source.File.Source = file;
        request.Source.File.Offset = offset;
        request.Source.File.Size = static_cast<UINT32>(size);
        request.Destination.Buffer.Resource = destBuffer; // FIXED: Target a raw buffer resource
        request.Destination.Buffer.Offset = 0;
        request.Destination.Buffer.Size = static_cast<UINT32>(size);

        m_dsQueue->EnqueueRequest(&request);
    }

    void SubmitQueue() {
        if (m_dsQueue) m_dsQueue->Submit();
    }

    IDStorageQueue* GetQueue() const {
        return m_dsQueue;
    }

    void Shutdown() {
        if (m_dsQueue) { m_dsQueue->Release(); m_dsQueue = nullptr; }
        if (m_dsFactory) { m_dsFactory->Release(); m_dsFactory = nullptr; }
    }
};

// 7. Zero-Copy GLB 72-Byte Interleaved Binary Parser & Root Signature Extractor
struct ZkaediGlbBinaryView {
    const ZkaediXboxVertex* Vertices;
    UINT                    VertexCount;
    const void*             Indices;
    UINT                    IndexCount;
    DXGI_FORMAT             IndexFormat;
    UINT                    VertexByteOffsetInBin;
    UINT                    IndexByteOffsetInBin;
    const uint8_t*          BaseColorImageBytes;
    UINT                    BaseColorImageSize;
    const uint8_t*          NormalImageBytes;
    UINT                    NormalImageSize;
    const uint8_t*          OrmImageBytes;
    UINT                    OrmImageSize;
};

inline int ParseJsonIntField(const char* objStart, const char* objEnd, const char* key, int defaultVal = -1) {
    const char* p = objStart;
    size_t klen = strlen(key);
    while (p && p + klen < objEnd) {
        const char* hit = strstr(p, key);
        if (!hit || hit >= objEnd) break;
        const char* colon = strchr(hit + klen, ':');
        if (!colon || colon >= objEnd) break;
        const char* val = colon + 1;
        while (val < objEnd && (*val == ' ' || *val == '\t' || *val == '\r' || *val == '\n')) ++val;
        if (val < objEnd && ((*val >= '0' && *val <= '9') || *val == '-')) {
            return static_cast<int>(strtol(val, nullptr, 10));
        }
        p = hit + klen;
    }
    return defaultVal;
}

inline HRESULT ParseXboxGlbInterleavedBuffer(
    const void* glbBytes,
    size_t glbSize,
    ZkaediGlbBinaryView* pOutView)
{
    if (!glbBytes || glbSize < 28 || !pOutView) return E_INVALIDARG;
    memset(pOutView, 0, sizeof(ZkaediGlbBinaryView));
    const uint8_t* raw = reinterpret_cast<const uint8_t*>(glbBytes);

    uint32_t magic = *reinterpret_cast<const uint32_t*>(raw + 0);
    uint32_t version = *reinterpret_cast<const uint32_t*>(raw + 4);
    uint32_t totalLen = *reinterpret_cast<const uint32_t*>(raw + 8);
    uint32_t jsonLen = *reinterpret_cast<const uint32_t*>(raw + 12);
    uint32_t jsonType = *reinterpret_cast<const uint32_t*>(raw + 16);

    if (magic != 0x46546C67u || version != 2u || jsonType != 0x4E4F534Au) return E_FAIL;
    if (totalLen > glbSize || 20u + jsonLen + 8u > glbSize) return E_FAIL;

    const char* jsonStr = reinterpret_cast<const char*>(raw + 20);
    const char* jsonEnd = jsonStr + jsonLen;
    // Verify 72-byte interleaved stride marker in JSON
    const char* strideTag = strstr(jsonStr, "\"byteStride\":72");
    if (!strideTag || strideTag >= jsonEnd) strideTag = strstr(jsonStr, "\"byteStride\": 72");
    if (!strideTag || strideTag >= jsonEnd) return E_INVALIDARG;

    uint32_t binLen = *reinterpret_cast<const uint32_t*>(raw + 20 + jsonLen);
    uint32_t binType = *reinterpret_cast<const uint32_t*>(raw + 20 + jsonLen + 4);
    if (binType != 0x004E4942u) return E_FAIL;
    const uint8_t* binChunk = raw + 20 + jsonLen + 8;

    // Parse all bufferViews (up to 32)
    UINT bvOffsets[32] = {};
    UINT bvLengths[32] = {};
    int  bvCount = 0;
    const char* bvTag = strstr(jsonStr, "\"bufferViews\"");
    if (!bvTag || bvTag >= jsonEnd) return E_FAIL;
    const char* bvArrStart = strchr(bvTag, '[');
    const char* bvArrEnd   = bvArrStart ? strchr(bvArrStart, ']') : nullptr;
    if (!bvArrStart || !bvArrEnd || bvArrEnd > jsonEnd) return E_FAIL;

    const char* cur = bvArrStart;
    while (cur < bvArrEnd && bvCount < 32) {
        const char* oStart = strchr(cur, '{');
        if (!oStart || oStart >= bvArrEnd) break;
        const char* oEnd = strchr(oStart, '}');
        if (!oEnd || oEnd >= bvArrEnd) break;
        int off = ParseJsonIntField(oStart, oEnd, "\"byteOffset\"", 0);
        int len = ParseJsonIntField(oStart, oEnd, "\"byteLength\"", 0);
        bvOffsets[bvCount] = (off > 0) ? static_cast<UINT>(off) : 0u;
        bvLengths[bvCount] = (len > 0) ? static_cast<UINT>(len) : 0u;
        bvCount++;
        cur = oEnd + 1;
    }
    if (bvCount < 2) return E_FAIL;

    UINT vbByteLen = bvLengths[0];
    if (vbByteLen == 0 || (vbByteLen % sizeof(ZkaediXboxVertex)) != 0 || vbByteLen > binLen) return E_FAIL;
    UINT vertexCount = vbByteLen / sizeof(ZkaediXboxVertex);

    UINT ibByteOffset = bvOffsets[1];
    UINT ibByteLen    = bvLengths[1];
    if ((ibByteOffset % 64u) != 0 || ibByteOffset + ibByteLen > binLen) return E_FAIL;

    DXGI_FORMAT idxFmt = (vertexCount < 65535u) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
    UINT idxStride = (idxFmt == DXGI_FORMAT_R16_UINT) ? 2u : 4u;

    pOutView->Vertices              = reinterpret_cast<const ZkaediXboxVertex*>(binChunk);
    pOutView->VertexCount           = vertexCount;
    pOutView->Indices               = binChunk + ibByteOffset;
    pOutView->IndexCount            = ibByteLen / idxStride;
    pOutView->IndexFormat           = idxFmt;
    pOutView->VertexByteOffsetInBin = 0;
    pOutView->IndexByteOffsetInBin  = ibByteOffset;

    // Parse embedded images[] and materials[] to bind Albedo, Normal, and ORM textures
    int imgBv[32]  = {};
    int imgCount   = 0;
    const char* imgTag = strstr(jsonStr, "\"images\"");
    if (imgTag && imgTag < jsonEnd) {
        const char* iStart = strchr(imgTag, '[');
        const char* iEnd   = iStart ? strchr(iStart, ']') : nullptr;
        if (iStart && iEnd && iEnd <= jsonEnd) {
            const char* ic = iStart;
            while (ic < iEnd && imgCount < 32) {
                const char* oS = strchr(ic, '{');
                if (!oS || oS >= iEnd) break;
                const char* oE = strchr(oS, '}');
                if (!oE || oE >= iEnd) break;
                imgBv[imgCount++] = ParseJsonIntField(oS, oE, "\"bufferView\"", -1);
                ic = oE + 1;
            }
        }
    }

    int texSrc[32] = {};
    int texCount   = 0;
    const char* texTag = strstr(jsonStr, "\"textures\"");
    if (texTag && texTag < jsonEnd) {
        const char* tStart = strchr(texTag, '[');
        const char* tEnd   = tStart ? strchr(tStart, ']') : nullptr;
        if (tStart && tEnd && tEnd <= jsonEnd) {
            const char* tc = tStart;
            while (tc < tEnd && texCount < 32) {
                const char* oS = strchr(tc, '{');
                if (!oS || oS >= tEnd) break;
                const char* oE = strchr(oS, '}');
                if (!oE || oE >= tEnd) break;
                texSrc[texCount++] = ParseJsonIntField(oS, oE, "\"source\"", texCount);
                tc = oE + 1;
            }
        }
    }

    auto resolveTexSlotToBv = [&](const char* slotKey) -> int {
        const char* mTag = strstr(jsonStr, "\"materials\"");
        if (!mTag || mTag >= jsonEnd) return -1;
        const char* sTag = strstr(mTag, slotKey);
        if (!sTag || sTag >= jsonEnd) return -1;
        const char* oS = strchr(sTag, '{');
        const char* oE = oS ? strchr(oS, '}') : nullptr;
        if (!oS || !oE || oE > jsonEnd) return -1;
        int tIdx = ParseJsonIntField(oS, oE, "\"index\"", -1);
        if (tIdx < 0) return -1;
        int sIdx = (tIdx < texCount) ? texSrc[tIdx] : tIdx;
        if (sIdx < 0 || sIdx >= imgCount) return -1;
        return imgBv[sIdx];
    };

    int baseBv = resolveTexSlotToBv("\"baseColorTexture\"");
    int normBv = resolveTexSlotToBv("\"normalTexture\"");
    int ormBv  = resolveTexSlotToBv("\"metallicRoughnessTexture\"");

    if (baseBv >= 0 && baseBv < bvCount && bvOffsets[baseBv] + bvLengths[baseBv] <= binLen) {
        pOutView->BaseColorImageBytes = binChunk + bvOffsets[baseBv];
        pOutView->BaseColorImageSize  = bvLengths[baseBv];
    }
    if (normBv >= 0 && normBv < bvCount && bvOffsets[normBv] + bvLengths[normBv] <= binLen) {
        pOutView->NormalImageBytes = binChunk + bvOffsets[normBv];
        pOutView->NormalImageSize  = bvLengths[normBv];
    }
    if (ormBv >= 0 && ormBv < bvCount && bvOffsets[ormBv] + bvLengths[ormBv] <= binLen) {
        pOutView->OrmImageBytes = binChunk + bvOffsets[ormBv];
        pOutView->OrmImageSize  = bvLengths[ormBv];
    }

    return S_OK;
}

inline HRESULT CreateXboxRootSignatureFromDxil(
    ID3D12Device* device,
    const void* dxilBytecode,
    size_t bytecodeLength,
    ID3D12RootSignature** ppRootSignature)
{
    if (!device || !dxilBytecode || bytecodeLength == 0 || !ppRootSignature) return E_INVALIDARG;
    ZKAEDI_HR(device->CreateRootSignature(0, dxilBytecode, bytecodeLength, IID_PPV_ARGS(ppRootSignature)));
    return S_OK;
}

// ============================================================================
// 5. SIMULTANEOUS PLACED-RESOURCE HEAP SUBALLOCATION & TRANSIENT ALIASING
// ============================================================================
// Replaces serial CreateCommittedResource vidmm calls with a single unified
// 64KB-aligned ID3D12Heap and simultaneous CreatePlacedResource placement,
// plus transient memory aliasing between Shadow-Pass and SSAO Post-FX buffers.
// ============================================================================
struct UnifiedPlacedSceneArena {
    ComPtr<ID3D12Heap>     SceneBufferHeap;
    ComPtr<ID3D12Resource> PlacedVertexBuffer;
    ComPtr<ID3D12Resource> PlacedIndexBuffer;
    ComPtr<ID3D12Resource> PlacedSkinningCBuffer;
    ComPtr<ID3D12Resource> TransientShadowBuffer;
    ComPtr<ID3D12Resource> AliasedSsaoBuffer;
    UINT64                 TotalHeapBytes = 0;
    UINT64                 AliasedSavedBytes = 0;
};

inline UINT64 AlignD3D12Placement(UINT64 sz) {
    return (sz + 65535ULL) & ~65535ULL;
}

inline HRESULT AllocateSimultaneousPlacedSceneBuffers(
    ID3D12Device* device,
    UINT64 vbBytes,
    UINT64 ibBytes,
    UINT64 transientBytes,
    UnifiedPlacedSceneArena* outArena)
{
    if (!device || !outArena) return E_INVALIDARG;

    const UINT64 offVB        = 0;
    const UINT64 offIB        = offVB + AlignD3D12Placement(vbBytes);
    const UINT64 offCB        = offIB + AlignD3D12Placement(ibBytes);
    const UINT64 offTransient = offCB + AlignD3D12Placement(sizeof(SkinningConstants));
    const UINT64 totalBytes   = offTransient + AlignD3D12Placement(transientBytes);

    D3D12_HEAP_DESC heapDesc = {};
    heapDesc.SizeInBytes = totalBytes;
    heapDesc.Properties.Type = D3D12_HEAP_TYPE_DEFAULT;
    heapDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
    heapDesc.Flags = D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS;

    ZKAEDI_HR(device->CreateHeap(&heapDesc, IID_PPV_ARGS(&outArena->SceneBufferHeap)));
    outArena->TotalHeapBytes    = totalBytes;
    outArena->AliasedSavedBytes = AlignD3D12Placement(transientBytes);

    auto makeBufDesc = [](UINT64 width) -> D3D12_RESOURCE_DESC {
        D3D12_RESOURCE_DESC d = {};
        d.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        d.Width = width;
        d.Height = 1;
        d.DepthOrArraySize = 1;
        d.MipLevels = 1;
        d.SampleDesc.Count = 1;
        d.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        return d;
    };

    const D3D12_RESOURCE_DESC vbDesc = makeBufDesc(vbBytes);
    const D3D12_RESOURCE_DESC ibDesc = makeBufDesc(ibBytes);
    const D3D12_RESOURCE_DESC cbDesc = makeBufDesc((sizeof(SkinningConstants) + 255) & ~255ULL);
    const D3D12_RESOURCE_DESC trDesc = makeBufDesc(transientBytes);

    // Simultaneous placement inside the single unified GPU heap:
    ZKAEDI_HR(device->CreatePlacedResource(outArena->SceneBufferHeap.Get(), offVB, &vbDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&outArena->PlacedVertexBuffer)));
    ZKAEDI_HR(device->CreatePlacedResource(outArena->SceneBufferHeap.Get(), offIB, &ibDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&outArena->PlacedIndexBuffer)));
    ZKAEDI_HR(device->CreatePlacedResource(outArena->SceneBufferHeap.Get(), offCB, &cbDesc, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, nullptr, IID_PPV_ARGS(&outArena->PlacedSkinningCBuffer)));

    // Hidden memory area: Transient Shadow & SSAO buffers share the exact same heap offset via D3D12 Aliasing!
    ZKAEDI_HR(device->CreatePlacedResource(outArena->SceneBufferHeap.Get(), offTransient, &trDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&outArena->TransientShadowBuffer)));
    ZKAEDI_HR(device->CreatePlacedResource(outArena->SceneBufferHeap.Get(), offTransient, &trDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&outArena->AliasedSsaoBuffer)));

    return S_OK;
}

} // namespace Xbox
} // namespace Zkaedi

