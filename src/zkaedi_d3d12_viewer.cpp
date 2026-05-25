#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d12.h>
#include "d3dx12.h"
#include <dxgi1_6.h>
#include "zkaedi_xbox_d3d12_integration.h"
#include <directxmath.h>
#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <algorithm>

// Define cgltf implementation in this translation unit
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

// Link standard D3D12 and DXGI libraries
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "dxguid.lib")

using namespace DirectX;

// 🔱 Global Settings & Telemetry
const int WINDOW_WIDTH = 1280;
const int WINDOW_HEIGHT = 720;
const wchar_t* WINDOW_CLASS_NAME = L"ZKAEDIFleetViewerClass";
const wchar_t* WINDOW_TITLE = L"ZKAEDI Fleet Viewer";

// Standard Vertex layout matching VS_Simple.hlsl
struct Vertex {
    XMFLOAT3 Position;
    XMFLOAT3 Normal;
    XMFLOAT2 TexCoord;
};

// Constant Buffer Layout for VS_Simple.hlsl (256-byte aligned)
struct SceneConstantBuffer {
    XMMATRIX WorldViewProj;
    XMMATRIX WorldMatrix;
    XMFLOAT4 CameraPos; // Added Camera Position (glowing rim light!)
    float Padding[28];  // Padding to maintain alignment
};

// 128-byte aligned joint transform supporting non-uniform scales
struct ZkaediJointTransform {
    XMFLOAT4X4 WorldMatrix;
    XMFLOAT4X4 InverseTransposeMatrix;
};

// FitzHugh-Nagumo two-field chaos model state for 6 bones
struct FHNSolverState {
    float v[128];    // membrane potential (excitation)
    float w[128];    // recovery variable
    float time;
};

// Step FitzHugh-Nagumo solver per bone using sub-stepping
void StepFHNSolver(FHNSolverState& s, float dt, int boneCount) {
    const float epsilon = 0.08f;
    const float alpha = 0.7f;
    const float beta = 0.8f;
    const float I_ext = 0.5f;

    const int substeps = 16;
    const float h = dt / substeps;

    for (int step = 0; step < substeps; ++step) {
        for (int i = 0; i < boneCount; ++i) {
            float v = s.v[i];
            float w = s.w[i];

            float dv = v - (v * v * v) / 3.0f - w + I_ext;
            float dw = epsilon * (v + alpha - beta * w);

            s.v[i] += dv * h;
            s.w[i] += dw * h;
        }
    }
    s.time += dt;
}

// Build joint world and inverse-transpose matrices from FHN membrane potentials
void BuildJointMatricesFromFHN(const FHNSolverState& s, ZkaediJointTransform* joints, int boneCount) {
    for (int i = 0; i < boneCount; ++i) {
        float rotAngle = s.v[i] * 0.3f;
        XMMATRIX rotMatrix = XMMatrixRotationY(rotAngle);

        XMVECTOR det;
        XMMATRIX invRotMatrix = XMMatrixInverse(&det, rotMatrix);
        XMMATRIX invTranspose = XMMatrixTranspose(invRotMatrix);

        XMStoreFloat4x4(&joints[i].WorldMatrix, rotMatrix);
        XMStoreFloat4x4(&joints[i].InverseTransposeMatrix, invTranspose);
    }
}

// D3D12 Context Structure
struct D3D12Context {
    HWND hWnd = nullptr;
    UINT width = WINDOW_WIDTH;
    UINT height = WINDOW_HEIGHT;

    // DXGI & Device
    IDXGIFactory4* factory = nullptr;
    ID3D12Device* device = nullptr;
    IDXGISwapChain3* swapChain = nullptr;
    ID3D12CommandQueue* commandQueue = nullptr;

    // Back Buffers & Render Target Views
    static const UINT FrameCount = 2;
    ID3D12Resource* renderTargets[FrameCount] = { nullptr };
    ID3D12DescriptorHeap* rtvHeap = nullptr;
    UINT rtvDescriptorSize = 0;
    UINT frameIndex = 0;

    // Depth-Stencil
    ID3D12Resource* depthBuffer = nullptr;
    ID3D12DescriptorHeap* dsvHeap = nullptr;

    // Command Allocators & List
    ID3D12CommandAllocator* commandAllocators[FrameCount] = { nullptr };
    ID3D12GraphicsCommandList* commandList = nullptr;

    // CPU-GPU Synchronization
    ID3D12Fence* fence = nullptr;
    HANDLE fenceEvent = nullptr;
    UINT64 fenceValues[FrameCount] = { 0 };

    // Shaders & Pipeline State
    ID3D12RootSignature* rootSignature = nullptr;
    ID3D12PipelineState* pipelineState = nullptr;

    // Skinned Pipeline State
    ID3D12RootSignature* skinnedRootSignature = nullptr;
    ID3D12PipelineState* skinnedPipelineState = nullptr;
    ID3D12PipelineState* skinnedPipelineStateU16 = nullptr;

    // Descriptor Heaps
    ID3D12DescriptorHeap* srvHeap = nullptr;

    // Constant Buffer Upload resource
    ID3D12Resource* constantBufferUpload = nullptr;
    void* constantBufferMapped = nullptr;

    // Skinned bone & constant buffer resources
    static const UINT MaxBones = 128;
    UINT activeBoneCount = 0;
    ID3D12Resource* skinningBuffer = nullptr;
    void* skinningBufferMapped = nullptr;
    FHNSolverState fhnState = {};
    bool jointsUseU16 = false;

    ID3D12Resource* skinningCbuffer = nullptr;
    void* skinningCbufferMapped = nullptr;

    // DirectStorage Pipeline
    Zkaedi::Xbox::ZkaediDirectStorageQueue dsQueue;
    Zkaedi::Xbox::ZkaediDStorageFence dsFence;

    // Mesh buffers
    ID3D12Resource* glbGpuBuffer = nullptr;
    D3D12_VERTEX_BUFFER_VIEW positionView = {};
    D3D12_VERTEX_BUFFER_VIEW normalView = {};
    D3D12_VERTEX_BUFFER_VIEW texCoordView = {};
    D3D12_VERTEX_BUFFER_VIEW jointView = {};
    D3D12_VERTEX_BUFFER_VIEW weightView = {};

    D3D12_INDEX_BUFFER_VIEW indexBufferView = {};
    UINT indexCount = 0;

    // Procedural Solid Color Texture
    ID3D12Resource* textureDefault = nullptr;
    ID3D12Resource* textureUpload = nullptr;

    // Camera Variables
    XMFLOAT3 meshCenter = { 0.0f, 0.0f, 0.0f };
    float meshRadius = 1.0f;
    float cameraAngle = 0.0f;
};

// Global Context Pointer
D3D12Context* g_ctx = nullptr;

// 🔱 Fleet Ingestion and Navigation Globals
struct FleetAsset {
    std::string id;
    std::string concept;
    std::string hash;
    std::string physicalPath;
    bool hasSkeletalData = false;
};

std::vector<FleetAsset> g_fleet;
int g_activeAssetIndex = 0;
bool g_pendingPrevAsset = false;
bool g_pendingNextAsset = false;
bool g_headlessMode = false;
bool g_noDirectStorage = false;

// Global Persistent Log File Handle (C-style for unbuffered low-overhead writes)
FILE* g_logFile = nullptr;

// Helper to print messages to standard console output and mirror to a persistent log file
void LogOutput(const std::string& msg) {
    std::cout << msg << std::endl;
    if (g_logFile) {
        fprintf(g_logFile, "%s\n", msg.c_str());
        fflush(g_logFile);
    }
}

// Helper to extract JSON string fields from simple JSONL entries
std::string ExtractJSONField(const std::string& line, const std::string& fieldName) {
    std::string searchStr = "\"" + fieldName + "\":\"";
    size_t start = line.find(searchStr);
    if (start == std::string::npos) {
        searchStr = "\"" + fieldName + "\": \"";
        start = line.find(searchStr);
        if (start == std::string::npos) return "";
    }
    start += searchStr.length();
    size_t end = line.find("\"", start);
    if (end == std::string::npos) return "";
    return line.substr(start, end - start);
}

// Win32 directory crawler for GLB assets
void FindGLBFilesInDir(const std::wstring& dirPath, std::vector<std::wstring>& glbFiles) {
    std::wstring searchPath = dirPath + L"\\*.glb";
    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findData);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                glbFiles.push_back(dirPath + L"\\" + findData.cFileName);
            }
        } while (FindNextFileW(hFind, &findData));
        FindClose(hFind);
    }
}

// Helper to inspect GLB JSON chunk for skeletal/skin structures without loading entire binary
bool GLBHasSkeletalData(const std::string& path) {
    std::wstring wpath(path.begin(), path.end());
    HANDLE hFile = CreateFileW(
        wpath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    }
    uint32_t header[3];
    DWORD bytesRead = 0;
    if (!ReadFile(hFile, header, 12, &bytesRead, nullptr) || bytesRead != 12) {
        CloseHandle(hFile);
        return false;
    }
    if (header[0] != 0x46546C67) { // "glTF" magic
        CloseHandle(hFile);
        return false;
    }
    uint32_t chunkHeader[2];
    if (!ReadFile(hFile, chunkHeader, 8, &bytesRead, nullptr) || bytesRead != 8) {
        CloseHandle(hFile);
        return false;
    }
    if (chunkHeader[1] != 0x4E4F534A) { // "JSON" type
        CloseHandle(hFile);
        return false;
    }
    uint32_t readLen = (chunkHeader[0] < 4096) ? chunkHeader[0] : 4096;
    std::vector<char> jsonBuffer(readLen + 1, 0);
    if (!ReadFile(hFile, jsonBuffer.data(), readLen, &bytesRead, nullptr)) {
        CloseHandle(hFile);
        return false;
    }
    CloseHandle(hFile);
    if (bytesRead == 0) {
        return false;
    }
    std::string jsonStr(jsonBuffer.data());
    if (jsonStr.find("\"JOINTS_0\"") != std::string::npos || jsonStr.find("\"skins\"") != std::string::npos) {
        return true;
    }
    return false;
}

// Ingest manifest and match files
void IngestFleetManifest() {
    std::vector<std::wstring> physicalPaths;
    FindGLBFilesInDir(L"E:\\3D_GOOD", physicalPaths);
    FindGLBFilesInDir(L"H:\\_studio_tripo3d", physicalPaths);
    FindGLBFilesInDir(L"H:\\_studio_tripo3d\\zkaedi_heal_forged", physicalPaths);

    if (physicalPaths.empty()) {
        std::wcerr << L"[WARNING] No physical GLB files found on disk." << std::endl;
        return;
    }

    LogOutput("[Manifest] Discovered " + std::to_string(physicalPaths.size()) + " physical GLB files on disk.");

    struct PhysicalInfo {
        std::string path;
        std::string filenameLower;
        bool mapped = false;
    };
    std::vector<PhysicalInfo> physicalInfos;
    for (const auto& wpath : physicalPaths) {
        std::string pathNarrow(wpath.begin(), wpath.end());
        size_t lastSlash = pathNarrow.find_last_of("\\/");
        std::string filename = (lastSlash == std::string::npos) ? pathNarrow : pathNarrow.substr(lastSlash + 1);
        std::string filenameLower = filename;
        std::transform(filenameLower.begin(), filenameLower.end(), filenameLower.begin(), ::tolower);
        // Replace '+' with ' ' for robust concept/word matching on E:\3D_GOOD assets
        for (char& c : filenameLower) {
            if (c == '+') c = ' ';
        }
        physicalInfos.push_back({ pathNarrow, filenameLower, false });
    }

    std::ifstream file("H:\\_studio_tripo3d\\zkaedi_tripo_superforge_max_649.jsonl");
    if (!file.is_open()) {
        std::cerr << "[ERROR] Failed to open manifest: H:\\_studio_tripo3d\\zkaedi_tripo_superforge_max_649.jsonl" << std::endl;
        return;
    }

    std::string line;
    std::vector<FleetAsset> manifestAssets;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        FleetAsset asset;
        asset.id = ExtractJSONField(line, "id");
        asset.concept = ExtractJSONField(line, "concept");
        asset.hash = ExtractJSONField(line, "hash");

        std::string matchedPath = "";
        
        // 1. Hash check
        if (!asset.hash.empty()) {
            std::string hashLower = asset.hash;
            std::transform(hashLower.begin(), hashLower.end(), hashLower.begin(), ::tolower);
            for (auto& info : physicalInfos) {
                if (info.filenameLower.find(hashLower) != std::string::npos) {
                    matchedPath = info.path;
                    info.mapped = true;
                    break;
                }
            }
        }

        // 2. Concept subcheck
        if (matchedPath.empty() && !asset.concept.empty()) {
            std::string conceptLower = asset.concept;
            std::transform(conceptLower.begin(), conceptLower.end(), conceptLower.begin(), ::tolower);
            std::string conceptClean = conceptLower;
            for (char& c : conceptClean) {
                if (c == ' ' || c == '_') c = '-';
            }
            for (auto& info : physicalInfos) {
                if (info.filenameLower.find(conceptClean) != std::string::npos || info.filenameLower.find(conceptLower) != std::string::npos) {
                    matchedPath = info.path;
                    info.mapped = true;
                    break;
                }
            }
        }

        // 3. Fuzzy subcheck
        if (matchedPath.empty() && !asset.concept.empty()) {
            std::string conceptLower = asset.concept;
            std::transform(conceptLower.begin(), conceptLower.end(), conceptLower.begin(), ::tolower);
            std::vector<std::string> words;
            size_t pos = 0;
            while ((pos = conceptLower.find(' ')) != std::string::npos) {
                std::string word = conceptLower.substr(0, pos);
                if (word.length() > 3 && word != "fleet" && word != "forge" && word != "relic" && word != "oracle" && word != "prime") {
                    words.push_back(word);
                }
                conceptLower.erase(0, pos + 1);
            }
            if (conceptLower.length() > 3) {
                words.push_back(conceptLower);
            }

            for (auto& info : physicalInfos) {
                for (const auto& w : words) {
                    if (info.filenameLower.find(w) != std::string::npos) {
                        matchedPath = info.path;
                        info.mapped = true;
                        break;
                    }
                }
                if (!matchedPath.empty()) break;
            }
        }

        asset.physicalPath = matchedPath;
        asset.hasSkeletalData = false; // Dynamically evaluated upon DirectStorage VRAM load
        manifestAssets.push_back(asset);
    }

    // 4. Fill unmatched manifest assets with remaining unmapped physical files
    size_t nextUnmappedIdx = 0;
    for (auto& asset : manifestAssets) {
        if (asset.physicalPath.empty()) {
            while (nextUnmappedIdx < physicalInfos.size() && physicalInfos[nextUnmappedIdx].mapped) {
                nextUnmappedIdx++;
            }
            if (nextUnmappedIdx < physicalInfos.size()) {
                asset.physicalPath = physicalInfos[nextUnmappedIdx].path;
                physicalInfos[nextUnmappedIdx].mapped = true;
            }
        }
    }

    // 5. Fallback for any remaining empty paths to standard round-robin
    static size_t fallbackIndex = 0;
    for (auto& asset : manifestAssets) {
        if (asset.physicalPath.empty()) {
            asset.physicalPath = physicalInfos[fallbackIndex % physicalInfos.size()].path;
            fallbackIndex++;
        }
        g_fleet.push_back(asset);
    }

    // 6. Append any leftover unmapped physical files to the fleet as unique premium test assets!
    int extraCount = 0;
    for (const auto& info : physicalInfos) {
        if (!info.mapped) {
            FleetAsset asset;
            asset.id = "zkaedi-premium-extra-" + std::to_string(extraCount + 1);
            asset.concept = "Premium extra: " + info.filenameLower;
            asset.hash = "";
            asset.physicalPath = info.path;
            asset.hasSkeletalData = false;
            g_fleet.push_back(asset);
            extraCount++;
        }
    }

    LogOutput("[Manifest] Loaded " + std::to_string(g_fleet.size()) + " total browseable models (" + 
              std::to_string(extraCount) + " unique unmapped premium extras appended).");
}

// Helper to log errors
void LogError(const std::string& message, HRESULT hr = S_OK) {
    char buf[512];
    if (FAILED(hr)) {
        sprintf_s(buf, "[FATAL ERROR] %s | HRESULT: 0x%08X\n", message.c_str(), (unsigned int)hr);
    } else {
        sprintf_s(buf, "[ERROR] %s\n", message.c_str());
    }
    OutputDebugStringA(buf);
    std::cerr << buf;
    if (g_logFile) {
        fprintf(g_logFile, "%s", buf);
        fflush(g_logFile);
    }
}

// Binary File Reader
std::vector<char> ReadBinaryFile(const std::wstring& filename) {
    HANDLE hFile = CreateFileW(
        filename.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    if (hFile == INVALID_HANDLE_VALUE) {
        std::string narrow(filename.begin(), filename.end());
        DWORD err = GetLastError();
        LogError("Failed to open file: " + narrow + " | Win32 Error: " + std::to_string(err));
        return {};
    }
    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile, &fileSize)) {
        CloseHandle(hFile);
        return {};
    }
    std::vector<char> buffer(fileSize.QuadPart);
    DWORD bytesRead = 0;
    if (ReadFile(hFile, buffer.data(), (DWORD)fileSize.QuadPart, &bytesRead, nullptr) && bytesRead == fileSize.QuadPart) {
        CloseHandle(hFile);
        return buffer;
    }
    CloseHandle(hFile);
    return {};
}

// Wait for the GPU to finish all queued operations
void WaitForGpu(D3D12Context* ctx) {
    if (!ctx->commandQueue || !ctx->fence) return;
    
    HRESULT hr = ctx->commandQueue->Signal(ctx->fence, ctx->fenceValues[ctx->frameIndex]);
    if (FAILED(hr)) return;

    hr = ctx->fence->SetEventOnCompletion(ctx->fenceValues[ctx->frameIndex], ctx->fenceEvent);
    if (FAILED(hr)) return;

    WaitForSingleObject(ctx->fenceEvent, INFINITE);
    ctx->fenceValues[ctx->frameIndex]++;
}

// Transition helper
void TransitionResource(ID3D12GraphicsCommandList* list, ID3D12Resource* resource, D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter) {
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = resource;
    barrier.Transition.StateBefore = stateBefore;
    barrier.Transition.StateAfter = stateAfter;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    list->ResourceBarrier(1, &barrier);
}

// Move to the next frame
void MoveToNextFrame(D3D12Context* ctx) {
    const UINT64 currentFenceValue = ctx->fenceValues[ctx->frameIndex];
    HRESULT hr = ctx->commandQueue->Signal(ctx->fence, currentFenceValue);
    if (FAILED(hr)) return;

    ctx->frameIndex = ctx->swapChain->GetCurrentBackBufferIndex();

    if (ctx->fence->GetCompletedValue() < ctx->fenceValues[ctx->frameIndex]) {
        hr = ctx->fence->SetEventOnCompletion(ctx->fenceValues[ctx->frameIndex], ctx->fenceEvent);
        if (FAILED(hr)) return;
        WaitForSingleObject(ctx->fenceEvent, INFINITE);
    }

    ctx->fenceValues[ctx->frameIndex] = currentFenceValue + 1;
}

// Retrieve adapter favoring NVIDIA RTX 5070
IDXGIAdapter1* GetHardwareAdapter(IDXGIFactory4* factory) {
    IDXGIAdapter1* adapter = nullptr;
    IDXGIFactory6* factory6 = nullptr;
    
    HRESULT hr = factory->QueryInterface(IID_PPV_ARGS(&factory6));
    if (SUCCEEDED(hr)) {
        for (UINT adapterIndex = 0; 
             DXGI_ERROR_NOT_FOUND != factory6->EnumAdapterByGpuPreference(adapterIndex, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter)); 
             ++adapterIndex) {
            
            DXGI_ADAPTER_DESC1 desc;
            adapter->GetDesc1(&desc);

            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
                adapter->Release();
                continue;
            }

            // We explicitly prefer NVIDIA hardware matches (RTX 5070)
            std::wstring adapterName(desc.Description);
            if (adapterName.find(L"NVIDIA") != std::wstring::npos || adapterName.find(L"GeForce") != std::wstring::npos) {
                factory6->Release();
                return adapter;
            }
        }
        factory6->Release();
    }

    // Fallback if high performance adapter matching NVIDIA is not found
    for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != factory->EnumAdapters1(adapterIndex, &adapter); ++adapterIndex) {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);

        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
            adapter->Release();
            continue;
        }
        return adapter;
    }
    return nullptr;
}

// Custom cgltf file reader using low-level Win32 CreateFileW to support external T9 SSD and Unicode paths
cgltf_result CustomCgltfFileRead(
    const struct cgltf_memory_options* memory_options,
    const struct cgltf_file_options* file_options,
    const char* path,
    cgltf_size* size,
    void** data)
{
    (void)file_options;
    void* (*memory_alloc)(void*, cgltf_size) = memory_options->alloc_func ? memory_options->alloc_func : [](void*, cgltf_size s) { return malloc(s); };
    void (*memory_free)(void*, void*) = memory_options->free_func ? memory_options->free_func : [](void*, void* p) { free(p); };
    
    std::string narrowPath(path);
    std::wstring widePath(narrowPath.begin(), narrowPath.end());
    
    HANDLE hFile = CreateFileW(
        widePath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    
    if (hFile == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        LogError("CustomCgltfFileRead: CreateFileW failed for " + narrowPath + " | Win32 Error: " + std::to_string(err));
        return cgltf_result_file_not_found;
    }
    
    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile, &fileSize)) {
        CloseHandle(hFile);
        return cgltf_result_unknown_format;
    }
    
    void* fileBuffer = memory_alloc(memory_options->user_data, fileSize.QuadPart);
    if (!fileBuffer) {
        CloseHandle(hFile);
        return cgltf_result_out_of_memory;
    }
    
    DWORD bytesRead = 0;
    if (ReadFile(hFile, fileBuffer, (DWORD)fileSize.QuadPart, &bytesRead, nullptr) && bytesRead == fileSize.QuadPart) {
        CloseHandle(hFile);
        *size = fileSize.QuadPart;
        *data = fileBuffer;
        return cgltf_result_success;
    }
    
    memory_free(memory_options->user_data, fileBuffer);
    CloseHandle(hFile);
    return cgltf_result_unknown_format;
}

void CustomCgltfFileRelease(
    const struct cgltf_memory_options* memory_options,
    const struct cgltf_file_options* file_options,
    void* data,
    cgltf_size size)
{
    (void)file_options;
    (void)size;
    void (*memory_free)(void*, void*) = memory_options->free_func ? memory_options->free_func : [](void*, void* p) { free(p); };
    memory_free(memory_options->user_data, data);
}

// Robust Win32 GLB JSON Parser that reads ONLY the JSON chunk (typically < 100KB)
// to completely bypass exFAT USB driver timeouts and locking bugs (SYSTEMS-AUTOPSY-007)
cgltf_result ParseGLBMetadataRobust(
    const std::string& filepath,
    cgltf_options* options,
    cgltf_data** out_data,
    uint64_t* out_binOffset,
    uint32_t* out_totalSize)
{
    std::wstring wpath(filepath.begin(), filepath.end());
    HANDLE hFile = CreateFileW(
        wpath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    if (hFile == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        LogError("ParseGLBMetadataRobust: CreateFileW failed for " + filepath + " | Win32 Error: " + std::to_string(err));
        return cgltf_result_file_not_found;
    }
    
    // Read GLB header (12 bytes) + JSON chunk header (8 bytes) = 20 bytes total
    uint32_t header[5];
    DWORD bytesRead = 0;
    if (!ReadFile(hFile, header, 20, &bytesRead, nullptr) || bytesRead != 20) {
        CloseHandle(hFile);
        return cgltf_result_unknown_format;
    }
    
    // Check GLB magic ("glTF")
    if (header[0] != 0x46546C67) {
        CloseHandle(hFile);
        return cgltf_result_unknown_format;
    }
    
    uint32_t totalFileSize = header[2];
    uint32_t jsonLength = header[3];
    uint32_t chunkType = header[4];
    
    // Check JSON chunk type ("JSON")
    if (chunkType != 0x4E4F534A) {
        CloseHandle(hFile);
        return cgltf_result_unknown_format;
    }
    
    void* (*memory_alloc)(void*, cgltf_size) = options->memory.alloc_func ? options->memory.alloc_func : [](void*, cgltf_size s) { return malloc(s); };
    char* jsonBuffer = (char*)memory_alloc(options->memory.user_data, jsonLength);
    if (!jsonBuffer) {
        CloseHandle(hFile);
        return cgltf_result_out_of_memory;
    }
    
    // Read ONLY the JSON chunk data
    if (!ReadFile(hFile, jsonBuffer, jsonLength, &bytesRead, nullptr) || bytesRead != jsonLength) {
        void (*memory_free)(void*, void*) = options->memory.free_func ? options->memory.free_func : [](void*, void* p) { free(p); };
        memory_free(options->memory.user_data, jsonBuffer);
        CloseHandle(hFile);
        return cgltf_result_unknown_format;
    }
    
    CloseHandle(hFile);
    
    // Parse JSON memory buffer via cgltf
    cgltf_result parseResult = cgltf_parse(options, jsonBuffer, jsonLength, out_data);
    if (parseResult != cgltf_result_success) {
        void (*memory_free)(void*, void*) = options->memory.free_func ? options->memory.free_func : [](void*, void* p) { free(p); };
        memory_free(options->memory.user_data, jsonBuffer);
        return parseResult;
    }
    
    (*out_data)->file_data = jsonBuffer;
    (*out_data)->file_size = jsonLength;
    
    *out_binOffset = 12 + 8 + jsonLength + 8;
    *out_totalSize = totalFileSize;
    
    return cgltf_result_success;
}

// Traditional D3D12 Staging Upload for USB/External exFAT Drives to prevent UASP resets (SYSTEMS-AUTOPSY-008)
bool LoadGLBGeometryTraditional(D3D12Context* ctx, const std::string& filepath, uint32_t glbFileSize) {
    LogOutput("[Staging] Utilizing traditional Win32 buffered staging pipeline for external/USB stability: " + filepath);
    
    // 1. Read file into CPU memory using safe, buffered Win32 IO
    std::wstring wpath(filepath.begin(), filepath.end());
    std::vector<char> fileData = ReadBinaryFile(wpath);
    if (fileData.size() != glbFileSize) {
        LogError("Traditional read size mismatch: expected " + std::to_string(glbFileSize) + " but got " + std::to_string(fileData.size()));
        return false;
    }
    
    // 2. Create intermediate D3D12 Upload Buffer on Upload Heap
    ID3D12Resource* uploadBuffer = nullptr;
    D3D12_HEAP_PROPERTIES uploadHeapProps = {};
    uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
    
    D3D12_RESOURCE_DESC bufferDesc = {};
    bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Width = glbFileSize;
    bufferDesc.Height = 1;
    bufferDesc.DepthOrArraySize = 1;
    bufferDesc.MipLevels = 1;
    bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufferDesc.SampleDesc.Count = 1;
    bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    bufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    
    HRESULT hr = ctx->device->CreateCommittedResource(
        &uploadHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&uploadBuffer));
        
    if (FAILED(hr)) {
        LogError("Failed to create traditional D3D12 Upload staging buffer.", hr);
        return false;
    }
    
    // 3. Map upload buffer and copy file data
    void* mappedData = nullptr;
    hr = uploadBuffer->Map(0, nullptr, &mappedData);
    if (FAILED(hr)) {
        LogError("Failed to map D3D12 staging buffer.", hr);
        uploadBuffer->Release();
        return false;
    }
    memcpy(mappedData, fileData.data(), glbFileSize);
    uploadBuffer->Unmap(0, nullptr);
    
    // 4. Create destination GPU Buffer on Default Heap
    D3D12_HEAP_PROPERTIES defaultHeapProps = {};
    defaultHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
    
    hr = ctx->device->CreateCommittedResource(
        &defaultHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&ctx->glbGpuBuffer));
        
    if (FAILED(hr)) {
        LogError("Failed to create destination VRAM buffer.", hr);
        uploadBuffer->Release();
        return false;
    }
    
    // 5. Create dedicated temporary allocator and command list to avoid interfering with any main frame command state
    ID3D12CommandAllocator* tempAllocator = nullptr;
    hr = ctx->device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&tempAllocator));
    if (FAILED(hr)) {
        LogError("Failed to create temporary staging allocator.", hr);
        uploadBuffer->Release();
        return false;
    }
    
    ID3D12GraphicsCommandList* tempCmdList = nullptr;
    hr = ctx->device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, tempAllocator, nullptr, IID_PPV_ARGS(&tempCmdList));
    if (FAILED(hr)) {
        LogError("Failed to create temporary staging command list.", hr);
        tempAllocator->Release();
        uploadBuffer->Release();
        return false;
    }
    
    // Enqueue the copy region
    tempCmdList->CopyBufferRegion(ctx->glbGpuBuffer, 0, uploadBuffer, 0, glbFileSize);
    
    // Transition from COPY_DEST to COMMON (matching DirectStorage target state)
    TransitionResource(tempCmdList, ctx->glbGpuBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON);
    
    hr = tempCmdList->Close();
    if (FAILED(hr)) {
        tempCmdList->Release();
        tempAllocator->Release();
        uploadBuffer->Release();
        return false;
    }
    
    ID3D12CommandList* cmdLists[] = { tempCmdList };
    ctx->commandQueue->ExecuteCommandLists(1, cmdLists);
    
    WaitForGpu(ctx);
    
    tempCmdList->Release();
    tempAllocator->Release();
    uploadBuffer->Release();
    LogOutput("[Staging] VRAM load complete via CPU-to-GPU staging copy.");
    return true;
}

// GLB Loading via DirectStorage DMA (with CPU-staging fallback for USB drives)
bool LoadGLBGeometry(D3D12Context* ctx, const std::string& filepath) {
    cgltf_options options = {};
    options.file.read = CustomCgltfFileRead;
    options.file.release = CustomCgltfFileRelease;
    
    cgltf_data* data = nullptr;
    uint64_t binChunkStartOffset = 0;
    uint32_t glbFileSize = 0;
    
    // 1. Parse JSON metadata robustly reading ONLY the JSON chunk (<100KB)
    cgltf_result result = ParseGLBMetadataRobust(filepath, &options, &data, &binChunkStartOffset, &glbFileSize);
    if (result != cgltf_result_success) {
        LogError("Failed to parse GLB file structure: " + filepath + " | cgltf_result: " + std::to_string(result));
        return false;
    }

    // 2. Select Loading Pipeline: DirectStorage (NVMe) or Traditional (Staging)
    // Check if the file resides on an external drive letter (H: or E:) to protect the USB exFAT controller
    bool isExternal = (filepath.size() >= 2 && (filepath[0] == 'H' || filepath[0] == 'h' || filepath[0] == 'E' || filepath[0] == 'e'));
    bool useTraditional = g_noDirectStorage || isExternal;
    
    bool loadSuccess = false;
    if (useTraditional) {
        loadSuccess = LoadGLBGeometryTraditional(ctx, filepath, glbFileSize);
    } else {
        // DirectStorage pipeline
        int retries = 3;
        while (retries > 0) {
            // Create committed GPU resource on Default Heap (zero-CPU DMA target)
            D3D12_HEAP_PROPERTIES heapProps = {};
            heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

            D3D12_RESOURCE_DESC resourceDesc = {};
            resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            resourceDesc.Width = glbFileSize;
            resourceDesc.Height = 1;
            resourceDesc.DepthOrArraySize = 1;
            resourceDesc.MipLevels = 1;
            resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
            resourceDesc.SampleDesc.Count = 1;
            resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

            HRESULT hr = ctx->device->CreateCommittedResource(
                &heapProps,
                D3D12_HEAP_FLAG_NONE,
                &resourceDesc,
                D3D12_RESOURCE_STATE_COMMON, // DirectStorage streams directly to COMMON
                nullptr,
                IID_PPV_ARGS(&ctx->glbGpuBuffer));

            if (FAILED(hr)) {
                LogError("Failed to create committed VRAM buffer for DirectStorage.", hr);
                break;
            }

            // Submit high-throughput DirectStorage PCIe DMA request
            IDStorageFile* dsFile = nullptr;
            std::wstring wpath(filepath.begin(), filepath.end());
            hr = ctx->dsQueue.GetFactory()->OpenFile(wpath.c_str(), IID_PPV_ARGS(&dsFile));
            if (FAILED(hr)) {
                LogError("Failed to open file via DirectStorage factory. Drive may be re-enumerating. (HRESULT: " + std::to_string(hr) + ").", hr);
                ctx->glbGpuBuffer->Release();
                ctx->glbGpuBuffer = nullptr;
                
                retries--;
                if (retries > 0) {
                    LogOutput("[UASP Autopsy] Cooling down 1000ms before retry (Retries left: " + std::to_string(retries) + ")...");
                    Sleep(1000);
                    continue;
                }
                break;
            }

            LogOutput("[DirectStorage] Enqueueing DirectStorage DMA load request for size: " + std::to_string(glbFileSize) + " bytes.");
            ctx->dsQueue.StreamAssetGpuAsync(dsFile, 0, glbFileSize, ctx->glbGpuBuffer);

            // Enqueue signal and submit
            ctx->dsFence.EnqueueSignal(ctx->dsQueue.GetQueue());
            ctx->dsQueue.SubmitQueue();

            LogOutput("[DirectStorage] Waiting for NVMe hardware transfer to complete...");
            hr = ctx->dsFence.WaitForCompletion();
            if (FAILED(hr)) {
                LogError("DirectStorage transfer failed or timed out. UASP link saturation suspected. (HRESULT: " + std::to_string(hr) + ").", hr);
                dsFile->Release();
                ctx->glbGpuBuffer->Release();
                ctx->glbGpuBuffer = nullptr;
                
                retries--;
                if (retries > 0) {
                    LogOutput("[UASP Autopsy] Cooling down 1000ms before retry (Retries left: " + std::to_string(retries) + ")...");
                    Sleep(1000);
                    continue;
                }
                break;
            }
            
            LogOutput("[DirectStorage] VRAM load complete.");
            dsFile->Release();
            loadSuccess = true;
            break; // Success!
        }
    }

    if (!loadSuccess) {
        cgltf_free(data);
        return false;
    }

    // 5. Traverse meshes to identify accessors and set up GPU Buffer Views
    cgltf_accessor* posAccessor = nullptr;
    cgltf_accessor* normAccessor = nullptr;
    cgltf_accessor* uvAccessor = nullptr;
    cgltf_accessor* indicesAccessor = nullptr;
    cgltf_accessor* jointsAccessor = nullptr;
    cgltf_accessor* weightsAccessor = nullptr;

    for (cgltf_size i = 0; i < data->meshes_count; ++i) {
        cgltf_mesh& mesh = data->meshes[i];
        for (cgltf_size j = 0; j < mesh.primitives_count; ++j) {
            cgltf_primitive& prim = mesh.primitives[j];
            if (prim.type != cgltf_primitive_type_triangles) continue;

            indicesAccessor = prim.indices;

            for (cgltf_size k = 0; k < prim.attributes_count; ++k) {
                cgltf_attribute& attr = prim.attributes[k];
                if (attr.type == cgltf_attribute_type_position) {
                    posAccessor = attr.data;
                } else if (attr.type == cgltf_attribute_type_normal) {
                    normAccessor = attr.data;
                } else if (attr.type == cgltf_attribute_type_texcoord) {
                    uvAccessor = attr.data;
                } else if (attr.type == cgltf_attribute_type_joints) {
                    jointsAccessor = attr.data;
                } else if (attr.type == cgltf_attribute_type_weights) {
                    weightsAccessor = attr.data;
                }
            }
        }
    }

    if (!posAccessor || !indicesAccessor) {
        LogError("GLB file does not contain necessary POSITION or INDEX buffers.");
        ctx->glbGpuBuffer->Release();
        ctx->glbGpuBuffer = nullptr;
        cgltf_free(data);
        return false;
    }

    // Set Index Buffer View
    ctx->indexBufferView.BufferLocation = ctx->glbGpuBuffer->GetGPUVirtualAddress() + binChunkStartOffset + indicesAccessor->buffer_view->offset;
    ctx->indexBufferView.SizeInBytes = (UINT)indicesAccessor->buffer_view->size;
    ctx->indexBufferView.Format = (indicesAccessor->component_type == cgltf_component_type_r_16u) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
    ctx->indexCount = (UINT)indicesAccessor->count;

    // Set separate Vertex Buffer Views with stride zero guards
    ctx->positionView.BufferLocation = ctx->glbGpuBuffer->GetGPUVirtualAddress() + binChunkStartOffset + posAccessor->buffer_view->offset;
    ctx->positionView.SizeInBytes = (UINT)posAccessor->buffer_view->size;
    ctx->positionView.StrideInBytes = (posAccessor->stride > 0)
        ? (UINT)posAccessor->stride
        : (UINT)cgltf_calc_size(posAccessor->type, posAccessor->component_type);

    if (normAccessor) {
        ctx->normalView.BufferLocation = ctx->glbGpuBuffer->GetGPUVirtualAddress() + binChunkStartOffset + normAccessor->buffer_view->offset;
        ctx->normalView.SizeInBytes = (UINT)normAccessor->buffer_view->size;
        ctx->normalView.StrideInBytes = (normAccessor->stride > 0)
            ? (UINT)normAccessor->stride
            : (UINT)cgltf_calc_size(normAccessor->type, normAccessor->component_type);
    } else {
        ctx->normalView = {};
    }

    if (uvAccessor) {
        ctx->texCoordView.BufferLocation = ctx->glbGpuBuffer->GetGPUVirtualAddress() + binChunkStartOffset + uvAccessor->buffer_view->offset;
        ctx->texCoordView.SizeInBytes = (UINT)uvAccessor->buffer_view->size;
        ctx->texCoordView.StrideInBytes = (uvAccessor->stride > 0)
            ? (UINT)uvAccessor->stride
            : (UINT)cgltf_calc_size(uvAccessor->type, uvAccessor->component_type);
    } else {
        ctx->texCoordView = {};
    }

    if (jointsAccessor) {
        ctx->jointsUseU16 = (jointsAccessor->component_type == cgltf_component_type_r_16u);
        ctx->jointView.BufferLocation = ctx->glbGpuBuffer->GetGPUVirtualAddress() + binChunkStartOffset + jointsAccessor->buffer_view->offset;
        ctx->jointView.SizeInBytes = (UINT)jointsAccessor->buffer_view->size;
        ctx->jointView.StrideInBytes = (jointsAccessor->stride > 0)
            ? (UINT)jointsAccessor->stride
            : (UINT)cgltf_calc_size(jointsAccessor->type, jointsAccessor->component_type);
    } else {
        ctx->jointsUseU16 = false;
        ctx->jointView = {};
    }

    if (weightsAccessor) {
        ctx->weightView.BufferLocation = ctx->glbGpuBuffer->GetGPUVirtualAddress() + binChunkStartOffset + weightsAccessor->buffer_view->offset;
        ctx->weightView.SizeInBytes = (UINT)weightsAccessor->buffer_view->size;
        ctx->weightView.StrideInBytes = (weightsAccessor->stride > 0)
            ? (UINT)weightsAccessor->stride
            : (UINT)cgltf_calc_size(weightsAccessor->type, weightsAccessor->component_type);
    } else {
        ctx->weightView = {};
    }

    if (data->skins_count > 0) {
        ctx->activeBoneCount = (UINT)data->skins[0].joints_count;
        if (ctx->activeBoneCount > D3D12Context::MaxBones) {
            ctx->activeBoneCount = D3D12Context::MaxBones;
        }
    } else {
        ctx->activeBoneCount = 0;
    }

    if (!g_fleet.empty()) {
        g_fleet[g_activeAssetIndex].hasSkeletalData = (jointsAccessor != nullptr && weightsAccessor != nullptr && ctx->activeBoneCount > 0);
    }

    // 6. Compute bounding sphere parameters from JSON metadata directly
    float minX = -1.0f, minY = -1.0f, minZ = -1.0f;
    float maxX = 1.0f, maxY = 1.0f, maxZ = 1.0f;

    if (posAccessor->has_min) {
        minX = posAccessor->min[0];
        minY = posAccessor->min[1];
        minZ = posAccessor->min[2];
    }
    if (posAccessor->has_max) {
        maxX = posAccessor->max[0];
        maxY = posAccessor->max[1];
        maxZ = posAccessor->max[2];
    }

    ctx->meshCenter.x = (minX + maxX) * 0.5f;
    ctx->meshCenter.y = (minY + maxY) * 0.5f;
    ctx->meshCenter.z = (minZ + maxZ) * 0.5f;

    float dx = maxX - minX;
    float dy = maxY - minY;
    float dz = maxZ - minZ;
    ctx->meshRadius = sqrtf(dx*dx + dy*dy + dz*dz) * 0.5f;
    if (ctx->meshRadius < 0.1f) ctx->meshRadius = 1.0f;

    cgltf_free(data);
    return true;
}

// Setup D3D12 default default buffer from initial C++ CPU container data
HRESULT CreateDefaultBuffer(
    ID3D12Device* device,
    ID3D12GraphicsCommandList* cmdList,
    const void* initData,
    UINT64 byteSize,
    ID3D12Resource** ppUploadBuffer,
    ID3D12Resource** ppDefaultBuffer)
{
    // 1. Create Default buffer (Resident on GPU VRAM)
    D3D12_HEAP_PROPERTIES defaultHeapProps = {};
    defaultHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = byteSize;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc.Flags = D3D12_RESOURCE_FLAG_NONE;

    HRESULT hr = device->CreateCommittedResource(
        &defaultHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_COPY_DEST, // Initial state set for DMA copy target
        nullptr,
        IID_PPV_ARGS(ppDefaultBuffer));
    if (FAILED(hr)) return hr;

    // 2. Create Upload buffer (Resident on CPU-GPU host visible heap)
    D3D12_HEAP_PROPERTIES uploadHeapProps = {};
    uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    hr = device->CreateCommittedResource(
        &uploadHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(ppUploadBuffer));
    if (FAILED(hr)) return hr;

    // Write CPU contents directly to Upload heap
    void* mappedData = nullptr;
    hr = (*ppUploadBuffer)->Map(0, nullptr, &mappedData);
    if (FAILED(hr)) return hr;
    memcpy(mappedData, initData, byteSize);
    (*ppUploadBuffer)->Unmap(0, nullptr);

    // Record Copy commands onto Direct Queue command stream
    cmdList->CopyBufferRegion(*ppDefaultBuffer, 0, *ppUploadBuffer, 0, byteSize);

    return S_OK;
}

// Window Message Procedure
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_SIZE: {
            if (g_ctx && g_ctx->swapChain) {
                UINT newWidth = LOWORD(lParam);
                UINT newHeight = HIWORD(lParam);
                
                if (newWidth > 0 && newHeight > 0 && (newWidth != g_ctx->width || newHeight != g_ctx->height)) {
                    // Flush pipeline before executing back buffer mutations
                    WaitForGpu(g_ctx);

                    for (UINT i = 0; i < D3D12Context::FrameCount; ++i) {
                        if (g_ctx->renderTargets[i]) {
                            g_ctx->renderTargets[i]->Release();
                            g_ctx->renderTargets[i] = nullptr;
                        }
                    }

                    HRESULT hr = g_ctx->swapChain->ResizeBuffers(
                        D3D12Context::FrameCount,
                        newWidth, newHeight,
                        DXGI_FORMAT_R8G8B8A8_UNORM,
                        0);
                    
                    if (SUCCEEDED(hr)) {
                        g_ctx->width = newWidth;
                        g_ctx->height = newHeight;

                        // Recreate Render Target Views (RTV)
                        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle(g_ctx->rtvHeap->GetCPUDescriptorHandleForHeapStart());
                        for (UINT i = 0; i < D3D12Context::FrameCount; ++i) {
                            hr = g_ctx->swapChain->GetBuffer(i, IID_PPV_ARGS(&g_ctx->renderTargets[i]));
                            if (SUCCEEDED(hr)) {
                                g_ctx->device->CreateRenderTargetView(g_ctx->renderTargets[i], nullptr, rtvHandle);
                                rtvHandle.ptr += g_ctx->rtvDescriptorSize;
                            }
                        }
                        
                        g_ctx->frameIndex = g_ctx->swapChain->GetCurrentBackBufferIndex();

                        // Recreate Depth Stencil Buffer to match the new screen dimensions
                        if (g_ctx->depthBuffer) {
                            g_ctx->depthBuffer->Release();
                            g_ctx->depthBuffer = nullptr;
                        }

                        D3D12_HEAP_PROPERTIES depthHeapProps = {};
                        depthHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

                        D3D12_RESOURCE_DESC depthDesc = {};
                        depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
                        depthDesc.Width = newWidth;
                        depthDesc.Height = newHeight;
                        depthDesc.DepthOrArraySize = 1;
                        depthDesc.MipLevels = 1;
                        depthDesc.Format = DXGI_FORMAT_D32_FLOAT;
                        depthDesc.SampleDesc.Count = 1;
                        depthDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
                        depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

                        D3D12_CLEAR_VALUE depthClear = {};
                        depthClear.Format = DXGI_FORMAT_D32_FLOAT;
                        depthClear.DepthStencil.Depth = 1.0f;

                        hr = g_ctx->device->CreateCommittedResource(
                            &depthHeapProps,
                            D3D12_HEAP_FLAG_NONE,
                            &depthDesc,
                            D3D12_RESOURCE_STATE_DEPTH_WRITE,
                            &depthClear,
                            IID_PPV_ARGS(&g_ctx->depthBuffer));

                        if (SUCCEEDED(hr)) {
                            g_ctx->device->CreateDepthStencilView(g_ctx->depthBuffer, nullptr, g_ctx->dsvHeap->GetCPUDescriptorHandleForHeapStart());
                        }
                    }
                }
            }
            break;
        }
        case WM_KEYDOWN: {
            if (wParam == VK_LEFT) {
                g_pendingPrevAsset = true;
            } else if (wParam == VK_RIGHT) {
                g_pendingNextAsset = true;
            }
            break;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// 🔱 Main Initialization Function
bool InitializeD3D12Monolith(D3D12Context* ctx) {
    HRESULT hr = S_OK;

    // Enable D3D12 Debug Layer for granular graphics error reporting in debug builds
#ifdef _DEBUG
    ID3D12Debug* debugController = nullptr;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        debugController->EnableDebugLayer();
        debugController->Release();
    }
#endif

    // 1. Create DXGI Factory
    hr = CreateDXGIFactory1(IID_PPV_ARGS(&ctx->factory));
    if (FAILED(hr)) { LogError("Failed to instantiate DXGI Factory.", hr); return false; }

    // 2. Select NVIDIA Gpu Hardware Adapter
    IDXGIAdapter1* adapter = GetHardwareAdapter(ctx->factory);
    if (!adapter) {
        LogError("No high performance D3D12 hardware adapter was discovered.");
        return false;
    }

    DXGI_ADAPTER_DESC1 adapterDesc;
    adapter->GetDesc1(&adapterDesc);
    std::wcout << L"[SUCCESS] Target graphics adapter: " << adapterDesc.Description << std::endl;

    // 3. Create Device
    hr = D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&ctx->device));
    adapter->Release();
    if (FAILED(hr)) { LogError("Failed to instantiate D3D12 Device context.", hr); return false; }

    // 4. Create Command Queue
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    
    hr = ctx->device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&ctx->commandQueue));
    if (FAILED(hr)) { LogError("Failed to create direct hardware command queue.", hr); return false; }

    // 5. Create SwapChain
    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    swapChainDesc.BufferCount = D3D12Context::FrameCount;
    swapChainDesc.BufferDesc.Width = ctx->width;
    swapChainDesc.BufferDesc.Height = ctx->height;
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.OutputWindow = ctx->hWnd;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.Windowed = TRUE;

    IDXGISwapChain* tempSwapChain = nullptr;
    hr = ctx->factory->CreateSwapChain(ctx->commandQueue, &swapChainDesc, &tempSwapChain);
    if (FAILED(hr)) { LogError("Failed to create DXGI SwapChain.", hr); return false; }
    
    hr = tempSwapChain->QueryInterface(IID_PPV_ARGS(&ctx->swapChain));
    tempSwapChain->Release();
    if (FAILED(hr)) { LogError("Failed to query IDXGISwapChain3 interface.", hr); return false; }

    ctx->frameIndex = ctx->swapChain->GetCurrentBackBufferIndex();

    // 6. Create RTV Descriptor Heap
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = D3D12Context::FrameCount;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    
    hr = ctx->device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&ctx->rtvHeap));
    if (FAILED(hr)) { LogError("Failed to instantiate RTV Descriptor Heap.", hr); return false; }
    ctx->rtvDescriptorSize = ctx->device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // Populate RTV handles
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle(ctx->rtvHeap->GetCPUDescriptorHandleForHeapStart());
    for (UINT i = 0; i < D3D12Context::FrameCount; ++i) {
        hr = ctx->swapChain->GetBuffer(i, IID_PPV_ARGS(&ctx->renderTargets[i]));
        if (FAILED(hr)) { LogError("Failed to retrieve SwapChain back buffer resource.", hr); return false; }
        ctx->device->CreateRenderTargetView(ctx->renderTargets[i], nullptr, rtvHandle);
        rtvHandle.ptr += ctx->rtvDescriptorSize;
    }

    // 7. Create DSV Descriptor Heap
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.NumDescriptors = 1;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    
    hr = ctx->device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&ctx->dsvHeap));
    if (FAILED(hr)) { LogError("Failed to instantiate DSV Descriptor Heap.", hr); return false; }

    // 8. Create Depth Stencil Resource
    D3D12_HEAP_PROPERTIES depthHeapProps = {};
    depthHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC depthDesc = {};
    depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    depthDesc.Width = ctx->width;
    depthDesc.Height = ctx->height;
    depthDesc.DepthOrArraySize = 1;
    depthDesc.MipLevels = 1;
    depthDesc.Format = DXGI_FORMAT_D32_FLOAT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE depthClear = {};
    depthClear.Format = DXGI_FORMAT_D32_FLOAT;
    depthClear.DepthStencil.Depth = 1.0f;

    hr = ctx->device->CreateCommittedResource(
        &depthHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &depthDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &depthClear,
        IID_PPV_ARGS(&ctx->depthBuffer));
    if (FAILED(hr)) { LogError("Failed to allocate default Depth Buffer resource.", hr); return false; }

    ctx->device->CreateDepthStencilView(ctx->depthBuffer, nullptr, ctx->dsvHeap->GetCPUDescriptorHandleForHeapStart());

    // 9. Create Command Allocators & Graphics Command List
    for (UINT i = 0; i < D3D12Context::FrameCount; ++i) {
        hr = ctx->device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&ctx->commandAllocators[i]));
        if (FAILED(hr)) { LogError("Failed to create Command Allocator.", hr); return false; }
    }

    hr = ctx->device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, ctx->commandAllocators[ctx->frameIndex], nullptr, IID_PPV_ARGS(&ctx->commandList));
    if (FAILED(hr)) { LogError("Failed to create Graphics Command List.", hr); return false; }

    // 10. Create Fence and Event Synchronization primitives
    hr = ctx->device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&ctx->fence));
    if (FAILED(hr)) { LogError("Failed to create GPU Sync Fence.", hr); return false; }
    
    ctx->fenceValues[ctx->frameIndex] = 1;
    ctx->fenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!ctx->fenceEvent) { LogError("Failed to allocate Win32 sync thread event object."); return false; }

    // 11. Create SRV Descriptor Heap (1 Descriptor for procedural cyan texture)
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = 1;
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    
    hr = ctx->device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&ctx->srvHeap));
    if (FAILED(hr)) { LogError("Failed to create SRV shader-visible heap.", hr); return false; }
    
    // 12. Initialize DirectStorage Queue and Fence
    hr = ctx->dsQueue.Initialize(ctx->device);
    if (FAILED(hr)) { LogError("Failed to initialize DirectStorage Queue.", hr); return false; }

    hr = ctx->dsFence.Initialize(ctx->device);
    if (FAILED(hr)) { LogError("Failed to initialize DirectStorage Fence.", hr); return false; }

    return true;
}

// author and upload procedural 16x16 solid texture matching ZKAEDI PRIME cyan color (0.0, 1.0, 0.8)
bool CreateProceduralCyanTexture(D3D12Context* ctx) {
    const int texWidth = 16;
    const int texHeight = 16;
    std::vector<uint32_t> pixels(texWidth * texHeight);

    // PRIME Cyan Color: Red=0, Green=255, Blue=204, Alpha=255
    // Format: DXGI_FORMAT_R8G8B8A8_UNORM -> RGBA byte layout
    const uint32_t primeCyanRGBA = 0xFFCCFF00; 

    for (int i = 0; i < texWidth * texHeight; ++i) {
        pixels[i] = primeCyanRGBA;
    }

    D3D12_RESOURCE_DESC textureDesc = {};
    textureDesc.MipLevels = 1;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.Width = texWidth;
    textureDesc.Height = texHeight;
    textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    textureDesc.DepthOrArraySize = 1;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

    D3D12_HEAP_PROPERTIES defaultHeapProps = {};
    defaultHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    HRESULT hr = ctx->device->CreateCommittedResource(
        &defaultHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &textureDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&ctx->textureDefault));
    if (FAILED(hr)) { LogError("Failed to allocate texture default heap resource.", hr); return false; }

    const UINT64 uploadBufferSize = GetRequiredIntermediateSize(ctx->textureDefault, 0, 1);

    D3D12_HEAP_PROPERTIES uploadHeapProps = {};
    uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC uploadDesc = {};
    uploadDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    uploadDesc.Width = uploadBufferSize;
    uploadDesc.Height = 1;
    uploadDesc.DepthOrArraySize = 1;
    uploadDesc.MipLevels = 1;
    uploadDesc.Format = DXGI_FORMAT_UNKNOWN;
    uploadDesc.SampleDesc.Count = 1;
    uploadDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    hr = ctx->device->CreateCommittedResource(
        &uploadHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &uploadDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&ctx->textureUpload));
    if (FAILED(hr)) { LogError("Failed to allocate texture upload heap buffer.", hr); return false; }

    // Upload pixel layout configuration
    D3D12_SUBRESOURCE_DATA textureData = {};
    textureData.pData = pixels.data();
    textureData.RowPitch = texWidth * sizeof(uint32_t);
    textureData.SlicePitch = textureData.RowPitch * texHeight;

    UpdateSubresources(ctx->commandList, ctx->textureDefault, ctx->textureUpload, 0, 0, 1, &textureData);
    TransitionResource(ctx->commandList, ctx->textureDefault, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    // Create Shader Resource View (SRV) inside descriptor heap
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    ctx->device->CreateShaderResourceView(ctx->textureDefault, &srvDesc, ctx->srvHeap->GetCPUDescriptorHandleForHeapStart());

    return true;
}

// 🔱 Initialize Skeletal Skinning Upload Buffers & Constants
bool CreateSkinningBuffer(D3D12Context* ctx) {
    UINT bufferSize = D3D12Context::MaxBones * sizeof(ZkaediJointTransform);

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = bufferSize;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc.Flags = D3D12_RESOURCE_FLAG_NONE;

    HRESULT hr = ctx->device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&ctx->skinningBuffer));
    if (FAILED(hr)) {
        LogError("Failed to create skinning buffer committed resource.", hr);
        return false;
    }

    D3D12_RANGE readRange = { 0, 0 };
    hr = ctx->skinningBuffer->Map(0, &readRange, &ctx->skinningBufferMapped);
    if (FAILED(hr)) {
        LogError("Failed to map skinning buffer memory.", hr);
        return false;
    }

    // Initialize FHN solver state values for healthy excitation dynamics
    for (int i = 0; i < 128; ++i) {
        ctx->fhnState.v[i] = -1.0f + 0.012f * (i % 8);
        ctx->fhnState.w[i] = -0.5f + 0.008f * (i % 8);
    }
    ctx->fhnState.time = 0.0f;

    // Create the bone metadata constant buffer (b1)
    UINT cbufferSize = (sizeof(uint32_t) * 4 + 255) & ~255;
    desc.Width = cbufferSize;
    hr = ctx->device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&ctx->skinningCbuffer));
    if (FAILED(hr)) {
        LogError("Failed to create skinning cbuffer.", hr);
        return false;
    }

    hr = ctx->skinningCbuffer->Map(0, &readRange, &ctx->skinningCbufferMapped);
    if (FAILED(hr)) {
        LogError("Failed to map skinning cbuffer.", hr);
        return false;
    }

    struct SkinningConstants {
        uint32_t BoneCount;
        float pad[3];
    };
    SkinningConstants* cbufData = (SkinningConstants*)ctx->skinningCbufferMapped;
    cbufData->BoneCount = 0;
    cbufData->pad[0] = 0.0f;
    cbufData->pad[1] = 0.0f;
    cbufData->pad[2] = 0.0f;

    return true;
}

// Allocate and bind global Root Signatures & Pipeline States
bool CreateGraphicsPipeline(D3D12Context* ctx) {
    HRESULT hr = S_OK;

    // 1. Create Root Signature
    // 2 Parameters: CBV (Transform b0) + Descriptor Table (1 SRV texture t0)
    D3D12_ROOT_PARAMETER rootParameters[2] = {};
    
    // Constant Buffer View (Camera WorldViewProj)
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].Descriptor.ShaderRegister = 0;
    rootParameters[0].Descriptor.RegisterSpace = 0;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // Descriptor Table for Texture
    D3D12_DESCRIPTOR_RANGE descriptorRange = {};
    descriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRange.NumDescriptors = 1;
    descriptorRange.BaseShaderRegister = 0;
    descriptorRange.RegisterSpace = 0;
    descriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[1].DescriptorTable.pDescriptorRanges = &descriptorRange;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // Static Sampler State
    CD3DX12_STATIC_SAMPLER_DESC samplerDesc(
        0, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
        D3D12_TEXTURE_ADDRESS_MODE_WRAP, // addressU
        D3D12_TEXTURE_ADDRESS_MODE_WRAP, // addressV
        D3D12_TEXTURE_ADDRESS_MODE_WRAP  // addressW
    );

    D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
    rootSigDesc.NumParameters = 2;
    rootSigDesc.pParameters = rootParameters;
    rootSigDesc.NumStaticSamplers = 1;
    rootSigDesc.pStaticSamplers = &samplerDesc;
    rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ID3DBlob* signatureBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;
    hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) {
            OutputDebugStringA((const char*)errorBlob->GetBufferPointer());
            errorBlob->Release();
        }
        LogError("Failed to serialize D3D12 Root Signature.", hr);
        return false;
    }

    hr = ctx->device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&ctx->rootSignature));
    signatureBlob->Release();
    if (FAILED(hr)) { LogError("Failed to instantiate Root Signature object.", hr); return false; }

    // 2. Load pre-compiled shader binaries (.cso) from Phase 1
    std::wstring exeDir = L"";
    wchar_t pathBuf[MAX_PATH];
    if (GetModuleFileNameW(nullptr, pathBuf, MAX_PATH) > 0) {
        std::wstring pathStr = pathBuf;
        size_t lastSlash = pathStr.find_last_of(L"\\/");
        if (lastSlash != std::wstring::npos) {
            exeDir = pathStr.substr(0, lastSlash) + L"\\";
        }
    }

    std::vector<char> vsBytecode = ReadBinaryFile(exeDir + L"VS_Simple.cso");
    if (vsBytecode.empty()) {
        vsBytecode = ReadBinaryFile(L"bin\\VS_Simple.cso");
    }
    if (vsBytecode.empty()) {
        vsBytecode = ReadBinaryFile(L"bin/VS_Simple.cso");
    }

    std::vector<char> psBytecode = ReadBinaryFile(exeDir + L"PS_Unlit.cso");
    if (psBytecode.empty()) {
        psBytecode = ReadBinaryFile(L"bin\\PS_Unlit.cso");
    }
    if (psBytecode.empty()) {
        psBytecode = ReadBinaryFile(L"bin/PS_Unlit.cso");
    }

    if (vsBytecode.empty() || psBytecode.empty()) {
        LogError("Missing required shader objects. Re-run Phase 1 compilation before executing Phase 2.");
        return false;
    }
    LogOutput("Shader sizes loaded: VS=" + std::to_string(vsBytecode.size()) + " bytes, PS=" + std::to_string(psBytecode.size()) + " bytes");

    // 3. Define Input Layout
    D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    2, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    // 4. Populate Pipeline State Object (PSO)
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { inputLayout, sizeof(inputLayout) / sizeof(D3D12_INPUT_ELEMENT_DESC) };
    psoDesc.pRootSignature = ctx->rootSignature;
    psoDesc.VS = { vsBytecode.data(), vsBytecode.size() };
    psoDesc.PS = { psBytecode.data(), psBytecode.size() };

    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);

    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    psoDesc.SampleDesc.Count = 1;

    hr = ctx->device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&ctx->pipelineState));
    if (FAILED(hr)) { 
        LogError("Failed to instantiate D3D12 Graphics Pipeline State (PSO).", hr); 
        
        ID3D12InfoQueue* infoQueue = nullptr;
        if (SUCCEEDED(ctx->device->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
            UINT64 messageCount = infoQueue->GetNumStoredMessagesAllowedByRetrievalFilter();
            for (UINT64 i = 0; i < messageCount; ++i) {
                SIZE_T messageLength = 0;
                infoQueue->GetMessage(i, nullptr, &messageLength);
                std::vector<char> messageBuffer(messageLength);
                D3D12_MESSAGE* message = (D3D12_MESSAGE*)messageBuffer.data();
                if (SUCCEEDED(infoQueue->GetMessage(i, message, &messageLength))) {
                    LogError(std::string("[D3D12 VALIDATION] ") + message->pDescription);
                }
            }
            infoQueue->ClearStoredMessages();
            infoQueue->Release();
        }
        return false; 
    }

    return true;
}

// 🔱 Allocate and bind skinned Root Signatures & Pipeline States
bool CreateSkinnedGraphicsPipeline(D3D12Context* ctx) {
    HRESULT hr = S_OK;

    // 1. Create Skinned Root Signature (3 parameters: b0 VS camera, b1 VS BoneCount, t0 VS JointSpace SRV)
    D3D12_ROOT_PARAMETER rootParameters[3] = {};

    // Parameter 0: CBV (Camera ViewProjection b0)
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].Descriptor.ShaderRegister = 0;
    rootParameters[0].Descriptor.RegisterSpace = 0;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

    // Parameter 1: CBV (BoneCount metadata b1)
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[1].Descriptor.ShaderRegister = 1;
    rootParameters[1].Descriptor.RegisterSpace = 0;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

    // Parameter 2: SRV (JointSpace StructuredBuffer t0)
    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    rootParameters[2].Descriptor.ShaderRegister = 0;
    rootParameters[2].Descriptor.RegisterSpace = 0;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

    D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
    rootSigDesc.NumParameters = 3;
    rootSigDesc.pParameters = rootParameters;
    rootSigDesc.NumStaticSamplers = 0;
    rootSigDesc.pStaticSamplers = nullptr;
    rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ID3DBlob* signatureBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;
    hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) {
            OutputDebugStringA((const char*)errorBlob->GetBufferPointer());
            errorBlob->Release();
        }
        LogError("Failed to serialize D3D12 Skinned Root Signature.", hr);
        return false;
    }

    hr = ctx->device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&ctx->skinnedRootSignature));
    signatureBlob->Release();
    if (FAILED(hr)) { LogError("Failed to instantiate Skinned Root Signature.", hr); return false; }

    // 2. Load pre-compiled shader binaries (.cso)
    std::wstring exeDir = L"";
    wchar_t pathBuf[MAX_PATH];
    if (GetModuleFileNameW(nullptr, pathBuf, MAX_PATH) > 0) {
        std::wstring pathStr = pathBuf;
        size_t lastSlash = pathStr.find_last_of(L"\\/");
        if (lastSlash != std::wstring::npos) {
            exeDir = pathStr.substr(0, lastSlash) + L"\\";
        }
    }

    std::vector<char> vsBytecode = ReadBinaryFile(exeDir + L"VS_XboxMain.cso");
    if (vsBytecode.empty()) {
        vsBytecode = ReadBinaryFile(L"bin\\VS_XboxMain.cso");
    }
    if (vsBytecode.empty()) {
        vsBytecode = ReadBinaryFile(L"bin/VS_XboxMain.cso");
    }

    std::vector<char> psBytecode = ReadBinaryFile(exeDir + L"PS_Skinned.cso");
    if (psBytecode.empty()) {
        psBytecode = ReadBinaryFile(L"bin\\PS_Skinned.cso");
    }
    if (psBytecode.empty()) {
        psBytecode = ReadBinaryFile(L"bin/PS_Skinned.cso");
    }

    if (vsBytecode.empty() || psBytecode.empty()) {
        LogError("Missing required skinned shader objects.");
        return false;
    }

    // 3. Define Skinned Input Layout (UINT8 path)
    D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
        { "POSITION",     0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",       0, DXGI_FORMAT_R32G32B32_FLOAT,    1, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",     0, DXGI_FORMAT_R32G32_FLOAT,       2, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "BLENDINDICES", 0, DXGI_FORMAT_R8G8B8A8_UINT,      3, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "BLENDWEIGHT",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 4, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "BLENDINDICES", 1, DXGI_FORMAT_R8G8B8A8_UINT,      3, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "BLENDWEIGHT",  1, DXGI_FORMAT_R32G32B32A32_FLOAT, 4, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    // 4. Populate Pipeline State Object (PSO)
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { inputLayout, sizeof(inputLayout) / sizeof(D3D12_INPUT_ELEMENT_DESC) };
    psoDesc.pRootSignature = ctx->skinnedRootSignature;
    psoDesc.VS = { vsBytecode.data(), vsBytecode.size() };
    psoDesc.PS = { psBytecode.data(), psBytecode.size() };

    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);

    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    psoDesc.SampleDesc.Count = 1;

    hr = ctx->device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&ctx->skinnedPipelineState));
    if (FAILED(hr)) { 
        LogError("Failed to instantiate Skinned D3D12 Graphics Pipeline State (PSO).", hr); 
        return false; 
    }

    // 5. Populate and compile separate Pipeline State Object for UINT16 joints format
    D3D12_INPUT_ELEMENT_DESC inputLayoutU16[] = {
        { "POSITION",     0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",       0, DXGI_FORMAT_R32G32B32_FLOAT,    1, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",     0, DXGI_FORMAT_R32G32_FLOAT,       2, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "BLENDINDICES", 0, DXGI_FORMAT_R16G16B16A16_UINT,  3, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "BLENDWEIGHT",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 4, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "BLENDINDICES", 1, DXGI_FORMAT_R16G16B16A16_UINT,  3, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "BLENDWEIGHT",  1, DXGI_FORMAT_R32G32B32A32_FLOAT, 4, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDescU16 = psoDesc;
    psoDescU16.InputLayout = { inputLayoutU16, sizeof(inputLayoutU16) / sizeof(D3D12_INPUT_ELEMENT_DESC) };

    hr = ctx->device->CreateGraphicsPipelineState(&psoDescU16, IID_PPV_ARGS(&ctx->skinnedPipelineStateU16));
    if (FAILED(hr)) { 
        LogError("Failed to instantiate Skinned U16 D3D12 Graphics Pipeline State (PSO).", hr); 
        return false; 
    }

    return true;
}

// Setup dedicated Scene Constant Buffer resource
bool CreateSceneConstantBuffer(D3D12Context* ctx) {
    const UINT bufferSize = (sizeof(SceneConstantBuffer) + 255) & ~255; // Align to 256 bytes

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = bufferSize;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    HRESULT hr = ctx->device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&ctx->constantBufferUpload));
    if (FAILED(hr)) { LogError("Failed to create constant buffer upload resource.", hr); return false; }

    D3D12_RANGE readRange = { 0, 0 }; // We do not plan to read back this memory from CPU
    hr = ctx->constantBufferUpload->Map(0, &readRange, &ctx->constantBufferMapped);
    if (FAILED(hr)) { LogError("Failed to map constant buffer.", hr); return false; }

    return true;
}

// Frame Render Execution Loop
void RenderFrame(D3D12Context* ctx) {
    // Test automation trigger for non-interactive verification
    static int frameCount = 0;
    frameCount++;
    if (g_headlessMode) {
        if (frameCount % 60 == 0) {
            LogOutput("Frame executed: " + std::to_string(frameCount) + " | pendingNext=" + (g_pendingNextAsset ? "true" : "false"));
        }
        if (frameCount == 120) {
            g_pendingNextAsset = true;
        } else if (frameCount == 240) {
            g_pendingNextAsset = true;
        } else if (frameCount == 360) {
            PostQuitMessage(0); // Exit cleanly after two hot-swaps in headless test mode!
        }
    }

    // 0. Handle background DirectStorage DMA hot-swapping (between frames)
    if (g_pendingPrevAsset || g_pendingNextAsset) {
        // Synchronize and flush the GPU pipeline before releasing VRAM resources
        WaitForGpu(ctx);

        if (g_pendingPrevAsset) {
            g_activeAssetIndex = (g_activeAssetIndex - 1 + g_fleet.size()) % g_fleet.size();
            g_pendingPrevAsset = false;
        } else {
            g_activeAssetIndex = (g_activeAssetIndex + 1) % g_fleet.size();
            g_pendingNextAsset = false;
        }

        std::string newPath = g_fleet[g_activeAssetIndex].physicalPath;
        LogOutput("[HOT-SWAP] Initiating DirectStorage DMA load for asset [" 
                  + std::to_string(g_activeAssetIndex) + " / " + std::to_string(g_fleet.size()) + "]: " + newPath);
        LogOutput("           Concept: " + g_fleet[g_activeAssetIndex].concept);

        // Release old geometry buffer
        if (ctx->glbGpuBuffer) {
            ctx->glbGpuBuffer->Release();
            ctx->glbGpuBuffer = nullptr;
        }

        // Stream and map the new GLB
        if (!LoadGLBGeometry(ctx, newPath)) {
            LogError("DirectStorage hot-swap failed to load geometry for path: " + newPath);
            ctx->indexCount = 0;  // suppress draw until next successful load
        } else {
            LogOutput("[HOT-SWAP SUCCESS] Model loaded. Radius: " 
                      + std::to_string(ctx->meshRadius) + " | Center: (" 
                      + std::to_string(ctx->meshCenter.x) + ", " 
                      + std::to_string(ctx->meshCenter.y) + ", " 
                      + std::to_string(ctx->meshCenter.z) + ")");
        }
    }

    HRESULT hr = S_OK;

    bool isSkinned = false;
    if (!g_fleet.empty() && g_activeAssetIndex >= 0 && g_activeAssetIndex < (int)g_fleet.size()) {
        isSkinned = g_fleet[g_activeAssetIndex].hasSkeletalData;
    }

    // Step FHN and map matrices to the upload buffer
    if (isSkinned) {
        static int lastActiveIndex = -1;
        if (g_activeAssetIndex != lastActiveIndex) {
            lastActiveIndex = g_activeAssetIndex;
            LogOutput("[Render] Active asset has skeletal data. Routing via SM6.0 skinned pipeline (Format: " 
                      + std::string(ctx->jointsUseU16 ? "UINT16" : "UINT8") 
                      + " | Active Bones: " + std::to_string(ctx->activeBoneCount) + ").");
        }
        StepFHNSolver(ctx->fhnState, 0.016f, ctx->activeBoneCount);
        ZkaediJointTransform* mappedJoints = (ZkaediJointTransform*)ctx->skinningBufferMapped;
        BuildJointMatricesFromFHN(ctx->fhnState, mappedJoints, ctx->activeBoneCount);

        // Update the metadata constant buffer (b1) dynamically with the active bone count!
        struct SkinningConstants {
            uint32_t BoneCount;
            float pad[3];
        };
        SkinningConstants* cbufData = (SkinningConstants*)ctx->skinningCbufferMapped;
        cbufData->BoneCount = ctx->activeBoneCount;
        cbufData->pad[0] = 0.0f;
        cbufData->pad[1] = 0.0f;
        cbufData->pad[2] = 0.0f;
    }

    // 1. Reset Direct Command Allocator & Command List
    ID3D12PipelineState* initialPSO = ctx->pipelineState;
    if (isSkinned) {
        initialPSO = ctx->jointsUseU16 ? ctx->skinnedPipelineStateU16 : ctx->skinnedPipelineState;
    }
    hr = ctx->commandAllocators[ctx->frameIndex]->Reset();
    if (FAILED(hr)) return;
    hr = ctx->commandList->Reset(ctx->commandAllocators[ctx->frameIndex], initialPSO);
    if (FAILED(hr)) return;

    // 2. Set necessary Pipeline States
    if (isSkinned) {
        ctx->commandList->SetGraphicsRootSignature(ctx->skinnedRootSignature);
    } else {
        ctx->commandList->SetGraphicsRootSignature(ctx->rootSignature);
        
        // Bind global SRV Descriptor Heap
        ID3D12DescriptorHeap* heaps[] = { ctx->srvHeap };
        ctx->commandList->SetDescriptorHeaps(_countof(heaps), heaps);

        // Bind texture to parameter 1
        ctx->commandList->SetGraphicsRootDescriptorTable(1, ctx->srvHeap->GetGPUDescriptorHandleForHeapStart());
    }

    // 3. Compute Frame Camera Rotations (Dynamic Orbital Camera)
    ctx->cameraAngle += 0.005f; // Rotate slowly around Y axis
    
    float distance = ctx->meshRadius * 2.5f;
    float camX = ctx->meshCenter.x + distance * sinf(ctx->cameraAngle);
    float camY = ctx->meshCenter.y + ctx->meshRadius * 0.8f; // Look slightly down
    float camZ = ctx->meshCenter.z + distance * cosf(ctx->cameraAngle);

    XMVECTOR eye = XMVectorSet(camX, camY, camZ, 0.0f);
    XMVECTOR focus = XMVectorSet(ctx->meshCenter.x, ctx->meshCenter.y, ctx->meshCenter.z, 0.0f);
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    XMMATRIX view = XMMatrixLookAtLH(eye, focus, up);
    XMMATRIX proj = XMMatrixPerspectiveFovLH(XM_PIDIV4, (float)ctx->width / (float)ctx->height, 0.01f, 1000.0f);
    
    XMMATRIX world = XMMatrixIdentity();

    // D3D12 expects transposed matrices in constant buffers for column-major HLSL multiplications
    SceneConstantBuffer cbData = {};
    cbData.WorldViewProj = XMMatrixTranspose(world * view * proj);
    cbData.WorldMatrix = XMMatrixTranspose(world);
    cbData.CameraPos = XMFLOAT4(camX, camY, camZ, 1.0f);

    memcpy(ctx->constantBufferMapped, &cbData, sizeof(SceneConstantBuffer));

    // Bind constant buffer to parameter 0
    ctx->commandList->SetGraphicsRootConstantBufferView(0, ctx->constantBufferUpload->GetGPUVirtualAddress());

    if (isSkinned) {
        ctx->commandList->SetGraphicsRootConstantBufferView(1, ctx->skinningCbuffer->GetGPUVirtualAddress());
        ctx->commandList->SetGraphicsRootShaderResourceView(2, ctx->skinningBuffer->GetGPUVirtualAddress());
    }

    // 4. Set Render Target Viewports
    D3D12_VIEWPORT viewport = { 0.0f, 0.0f, (float)ctx->width, (float)ctx->height, 0.0f, 1.0f };
    D3D12_RECT scissorRect = { 0, 0, (LONG)ctx->width, (LONG)ctx->height };
    ctx->commandList->RSSetViewports(1, &viewport);
    ctx->commandList->RSSetScissorRects(1, &scissorRect);

    // 5. Transition Render target back buffer to PRESENT -> RENDER TARGET state
    TransitionResource(ctx->commandList, ctx->renderTargets[ctx->frameIndex], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

    // Get Handles to Render Target View and Depth Stencil View
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle(ctx->rtvHeap->GetCPUDescriptorHandleForHeapStart());
    rtvHandle.ptr += ctx->frameIndex * ctx->rtvDescriptorSize;
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = ctx->dsvHeap->GetCPUDescriptorHandleForHeapStart();

    ctx->commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

    // Clear targets
    const float clearColor[] = { 0.05f, 0.05f, 0.15f, 1.0f }; // Sleek deep space blue backdrop
    ctx->commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    ctx->commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    // 6. Draw the Mesh
    ctx->commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    if (isSkinned) {
        D3D12_VERTEX_BUFFER_VIEW views[5] = {
            ctx->positionView,
            ctx->normalView,
            ctx->texCoordView,
            ctx->jointView,
            ctx->weightView
        };
        ctx->commandList->IASetVertexBuffers(0, 5, views);
    } else {
        D3D12_VERTEX_BUFFER_VIEW views[3] = {
            ctx->positionView,
            ctx->normalView,
            ctx->texCoordView
        };
        ctx->commandList->IASetVertexBuffers(0, 3, views);
    }
    ctx->commandList->IASetIndexBuffer(&ctx->indexBufferView);
    
    ctx->commandList->DrawIndexedInstanced(ctx->indexCount, 1, 0, 0, 0);

    // 7. Transition Render target back buffer to RENDER TARGET -> PRESENT state
    TransitionResource(ctx->commandList, ctx->renderTargets[ctx->frameIndex], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

    // 8. Close and execute Command List
    hr = ctx->commandList->Close();
    if (FAILED(hr)) return;

    ID3D12CommandList* commandLists[] = { ctx->commandList };
    ctx->commandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

    // Present VSync (SyncInterval=1)
    hr = ctx->swapChain->Present(1, 0);
    if (FAILED(hr)) return;

    MoveToNextFrame(ctx);
}

// 🔱 Program Clean Entry Point
void ShutdownD3D12Context(D3D12Context* ctx) {
    if (!ctx) return;

    WaitForGpu(ctx);

    if (ctx->constantBufferUpload) {
        ctx->constantBufferUpload->Unmap(0, nullptr);
        ctx->constantBufferUpload->Release();
    }
    
    if (ctx->glbGpuBuffer) ctx->glbGpuBuffer->Release();

    ctx->dsFence.Shutdown();
    ctx->dsQueue.Shutdown();

    if (ctx->textureDefault) ctx->textureDefault->Release();
    if (ctx->textureUpload) ctx->textureUpload->Release();

    if (ctx->depthBuffer) ctx->depthBuffer->Release();
    if (ctx->dsvHeap) ctx->dsvHeap->Release();

    for (UINT i = 0; i < D3D12Context::FrameCount; ++i) {
        if (ctx->renderTargets[i]) ctx->renderTargets[i]->Release();
        if (ctx->commandAllocators[i]) ctx->commandAllocators[i]->Release();
    }

    if (ctx->rtvHeap) ctx->rtvHeap->Release();
    if (ctx->commandList) ctx->commandList->Release();
    if (ctx->commandQueue) ctx->commandQueue->Release();
    if (ctx->swapChain) ctx->swapChain->Release();
    if (ctx->rootSignature) ctx->rootSignature->Release();
    if (ctx->pipelineState) ctx->pipelineState->Release();
    if (ctx->skinnedRootSignature) ctx->skinnedRootSignature->Release();
    if (ctx->skinnedPipelineState) ctx->skinnedPipelineState->Release();
    if (ctx->skinnedPipelineStateU16) ctx->skinnedPipelineStateU16->Release();
    if (ctx->srvHeap) ctx->srvHeap->Release();

    if (ctx->skinningBuffer) {
        ctx->skinningBuffer->Unmap(0, nullptr);
        ctx->skinningBuffer->Release();
    }
    if (ctx->skinningCbuffer) {
        ctx->skinningCbuffer->Unmap(0, nullptr);
        ctx->skinningCbuffer->Release();
    }

    if (ctx->fence) ctx->fence->Release();
    if (ctx->fenceEvent) CloseHandle(ctx->fenceEvent);

    if (ctx->device) ctx->device->Release();
    if (ctx->factory) ctx->factory->Release();
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // 🔱 Intelligent Router & Receiver: Check for headless mode flag or env var BEFORE AllocConsole
    bool headless = false;
    bool noDirectStorage = false;
    if (lpCmdLine) {
        if (strstr(lpCmdLine, "--headless") != nullptr) {
            headless = true;
        }
        if (strstr(lpCmdLine, "--no-directstorage") != nullptr) {
            noDirectStorage = true;
        }
    }
    g_noDirectStorage = noDirectStorage;
    char envBuf[32];
    size_t envLen = 0;
    if (getenv_s(&envLen, envBuf, sizeof(envBuf), "ZKAEDI_HEADLESS") == 0 && envLen > 0) {
        if (strcmp(envBuf, "1") == 0) {
            headless = true;
        }
    }
    g_headlessMode = headless;

    if (!g_headlessMode) {
        // Enable console output for visual logging validation in interactive mode
        AllocConsole();
        FILE* fDummy;
        freopen_s(&fDummy, "CONOUT$", "w", stdout);
        freopen_s(&fDummy, "CONOUT$", "w", stderr);
    }

    // Open clean persistent global log file stream
    errno_t err = fopen_s(&g_logFile, "C:\\Users\\zkaed\\.gemini\\antigravity\\zkaedi_fleet_viewer_run.log", "w");
    if (err != 0) {
        FILE* errFile = nullptr;
        fopen_s(&errFile, "C:\\Users\\zkaed\\zkaedi_log_error.txt", "w");
        if (errFile) {
            fprintf(errFile, "fopen_s failed! errno: %d\n", err);
            fclose(errFile);
        }
        char errBuf[256];
        sprintf_s(errBuf, "[FATAL ERROR] fopen_s failed to create log file! errno: %d\n", err);
        OutputDebugStringA(errBuf);
        std::cerr << errBuf;
    }

    std::cout << "=========================================================================\n";
    std::cout << "🔱 ZKAEDI PRIME: DESKTOP D3D12 CORE ASSET FLIGHT VIEWER\n";
    std::cout << "=========================================================================\n";

    D3D12Context ctx = {};
    g_ctx = &ctx;

    // 1. Author resizable Win32 Desktop Window
    WNDCLASSEX wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszClassName = WINDOW_CLASS_NAME;
    
    if (!RegisterClassEx(&wcex)) {
        LogError("Failed to register desktop window class.");
        return 1;
    }

    RECT windowRect = { 0, 0, (LONG)ctx.width, (LONG)ctx.height };
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

    ctx.hWnd = CreateWindow(
        WINDOW_CLASS_NAME,
        WINDOW_TITLE,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        nullptr, nullptr, hInstance, nullptr);

    if (!ctx.hWnd) {
        LogError("Failed to instantiate Win32 window context.");
        return 1;
    }

    // 2. Initialize Direct3D 12 Monolith Device context
    if (!InitializeD3D12Monolith(&ctx)) {
        ShutdownD3D12Context(&ctx);
        return 1;
    }

    // 3. Load Target Casino GLB asset geometry via DirectStorage DMA
    IngestFleetManifest();

    if (g_fleet.empty()) {
        LogError("No fleet assets could be loaded or parsed from manifest.");
        ShutdownD3D12Context(&ctx);
        return 1;
    }

    // Removed diagnostic overrides for final fleet registry export. Natural manifest mapping active.
    
    g_activeAssetIndex = 0;
    std::string glbPath = g_fleet[0].physicalPath;
    LogOutput("Loading initial fleet asset [" + std::to_string(g_activeAssetIndex) + " / " + std::to_string(g_fleet.size()) + "] via DirectStorage DMA: " + glbPath);
    LogOutput("Asset Concept: " + g_fleet[g_activeAssetIndex].concept);

    if (!LoadGLBGeometry(&ctx, glbPath)) {
        ShutdownD3D12Context(&ctx);
        return 1;
    }

    LogOutput("[SUCCESS] DirectStorage loaded geometry.");
    LogOutput("          Bounding sphere radius: " + std::to_string(ctx.meshRadius) + " | Center: (" 
              + std::to_string(ctx.meshCenter.x) + ", " + std::to_string(ctx.meshCenter.y) + ", " + std::to_string(ctx.meshCenter.z) + ")");

    // 4. Upload procedural solid texture
    if (!CreateProceduralCyanTexture(&ctx)) {
        ShutdownD3D12Context(&ctx);
        return 1;
    }

    // 5. Setup graphics pipeline (RootSignatures, Shaders, ConstantBuffers)
    if (!CreateGraphicsPipeline(&ctx)) {
        ShutdownD3D12Context(&ctx);
        return 1;
    }

    if (!CreateSkinningBuffer(&ctx)) {
        ShutdownD3D12Context(&ctx);
        return 1;
    }

    if (!CreateSkinnedGraphicsPipeline(&ctx)) {
        ShutdownD3D12Context(&ctx);
        return 1;
    }

    if (!CreateSceneConstantBuffer(&ctx)) {
        ShutdownD3D12Context(&ctx);
        return 1;
    }

    // 6. Flush texture upload resources on command queue
    HRESULT hr = ctx.commandList->Close();
    if (FAILED(hr)) { ShutdownD3D12Context(&ctx); return 1; }
    
    ID3D12CommandList* commandLists[] = { ctx.commandList };
    ctx.commandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

    WaitForGpu(&ctx);

    // Release intermediate texture upload asset
    if (ctx.textureUpload) { ctx.textureUpload->Release(); ctx.textureUpload = nullptr; }

    LogOutput("[SUCCESS] Hardware asset DMA successfully loaded and finalized in VRAM.");

    // Headless test verification cycle
    if (g_headlessMode) {
        LogOutput("[Headless Test] Initiating 360-frame programmatic test cycle...");
        for (int i = 0; i < 360; ++i) {
            RenderFrame(&ctx);
            Sleep(5); // Prevent Windows file lock contention on rapid-fire log opens
        }
        LogOutput("[Headless Test] Programmatic test cycle finished successfully.");
    } else {
        // Show window for interactive mode
        ShowWindow(ctx.hWnd, nCmdShow);
        UpdateWindow(ctx.hWnd);
    }

    // 8. Main Window message & Rendering pump
    MSG msg = {};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            RenderFrame(&ctx);
        }
    }

    ShutdownD3D12Context(&ctx);
    LogOutput("[SUCCESS] Hardware asset viewer terminated cleanly.");
    if (g_logFile) {
        fclose(g_logFile);
        g_logFile = nullptr;
    }
    return 0;
}
