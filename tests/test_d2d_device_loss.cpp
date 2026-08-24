// ============================================================================
// tests/test_d2d_device_loss.cpp — Direct2D & WIC Device-Loss Fault Injection
// ============================================================================
// Interactive Win32 verification sample demonstrating robust graphics recovery:
// 1. Renders animated rotating geometry, linear gradient, and DirectWrite text.
// 2. Supports Drag & Drop WIC image loading (PNG/JPG/TIFF) into GPU texture.
// 3. Simulates interactive GPU device loss / TDR when [SPACE] or [L] is pressed.
// 4. Proves factory preservation and seamless texture re-upload from CPU memory.
// ============================================================================

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>

#ifdef DrawText
#undef DrawText
#endif

#include <chrono>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <vector>
#include <fstream>

#include "../src/gfx/d2d_device_resources.hpp"

#pragma comment(lib, "shell32.lib")

// ----------------------------------------------------------------------------
// Global Application State & Resources
// ----------------------------------------------------------------------------
static d2d_engine::DeviceResources g_resources;

// CPU Backing Store (Survives Device Loss)
static std::vector<BYTE> g_loadedImageData;
static std::wstring      g_loadedImageName = L"Procedural Multi-Stop Gradient";

// Application Device-Dependent GPU Resources
static Microsoft::WRL::ComPtr<ID2D1SolidColorBrush>       g_textBrush;
static Microsoft::WRL::ComPtr<ID2D1SolidColorBrush>       g_accentBrush;
static Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush>   g_geometryBrush;
static Microsoft::WRL::ComPtr<ID2D1RadialGradientBrush>   g_glowBrush;
static Microsoft::WRL::ComPtr<ID2D1Bitmap1>               g_gpuTexture;
static Microsoft::WRL::ComPtr<ID2D1BitmapBrush1>          g_bitmapBrush;

// Application Device-Independent Resources
static Microsoft::WRL::ComPtr<IDWriteTextFormat>          g_titleFormat;
static Microsoft::WRL::ComPtr<IDWriteTextFormat>          g_hudFormat;
static Microsoft::WRL::ComPtr<ID2D1PathGeometry>          g_starGeometry;

// Metrics & Diagnostics
static uint32_t g_deviceLossEventCount = 0;
static float    g_animationTime = 0.0f;
static std::string g_statusBanner = "OPERATIONAL — HARDWARE STABLE";

// ----------------------------------------------------------------------------
// Resource Creation Handlers
// ----------------------------------------------------------------------------

// Called when device-dependent GPU resources need recreation
void CreateDeviceDependentResources(ID2D1DeviceContext* context)
{
    if (!context) return;

    // 1. Create Solid Brushes
    context->CreateSolidColorBrush(D2D1::ColorF(0.95f, 0.96f, 0.98f), &g_textBrush);
    context->CreateSolidColorBrush(D2D1::ColorF(0.00f, 0.75f, 1.00f, 0.85f), &g_accentBrush);

    // 2. Create Linear Gradient Brush for Rotating Star
    D2D1_GRADIENT_STOP gradientStops[3];
    gradientStops[0].color = D2D1::ColorF(0.00f, 0.85f, 1.00f, 1.0f); // Cyan
    gradientStops[0].position = 0.0f;
    gradientStops[1].color = D2D1::ColorF(0.55f, 0.20f, 1.00f, 1.0f); // Purple
    gradientStops[1].position = 0.5f;
    gradientStops[2].color = D2D1::ColorF(1.00f, 0.20f, 0.60f, 1.0f); // Magenta
    gradientStops[2].position = 1.0f;

    Microsoft::WRL::ComPtr<ID2D1GradientStopCollection> stopCollection;
    context->CreateGradientStopCollection(
        gradientStops,
        3,
        D2D1_GAMMA_2_2,
        D2D1_EXTEND_MODE_MIRROR,
        &stopCollection
    );

    context->CreateLinearGradientBrush(
        D2D1::LinearGradientBrushProperties(
            D2D1::Point2F(-120.0f, -120.0f),
            D2D1::Point2F(120.0f, 120.0f)
        ),
        stopCollection.Get(),
        &g_geometryBrush
    );

    // 3. Create Radial Glow Brush
    D2D1_GRADIENT_STOP glowStops[2];
    glowStops[0].color = D2D1::ColorF(0.00f, 0.80f, 1.00f, 0.35f);
    glowStops[0].position = 0.0f;
    glowStops[1].color = D2D1::ColorF(0.00f, 0.80f, 1.00f, 0.00f);
    glowStops[1].position = 1.0f;

    Microsoft::WRL::ComPtr<ID2D1GradientStopCollection> glowCollection;
    context->CreateGradientStopCollection(glowStops, 2, &glowCollection);

    context->CreateRadialGradientBrush(
        D2D1::RadialGradientBrushProperties(
            D2D1::Point2F(0.0f, 0.0f),
            D2D1::Point2F(0.0f, 0.0f),
            180.0f,
            180.0f
        ),
        glowCollection.Get(),
        &g_glowBrush
    );

    // 4. Re-upload WIC Bitmap Texture from CPU Backing Store if present
    if (!g_loadedImageData.empty()) {
        HRESULT hr = g_resources.CreateBitmapFromMemory(
            g_loadedImageData.data(),
            static_cast<UINT>(g_loadedImageData.size()),
            &g_gpuTexture
        );

        if (SUCCEEDED(hr) && g_gpuTexture) {
            D2D1_BITMAP_BRUSH_PROPERTIES1 brushProps = D2D1::BitmapBrushProperties1(
                D2D1_EXTEND_MODE_CLAMP,
                D2D1_EXTEND_MODE_CLAMP,
                D2D1_INTERPOLATION_MODE_LINEAR
            );
            context->CreateBitmapBrush(g_gpuTexture.Get(), brushProps, &g_bitmapBrush);
        }
    }
}

// Called when device-dependent GPU resources must be released
void ReleaseDeviceDependentResources()
{
    g_bitmapBrush.Reset();
    g_gpuTexture.Reset();
    g_glowBrush.Reset();
    g_geometryBrush.Reset();
    g_accentBrush.Reset();
    g_textBrush.Reset();
}

// Create Device-Independent Resources (DirectWrite formats, static geometries)
HRESULT CreateDeviceIndependentResources()
{
    IDWriteFactory1* dwrite = g_resources.GetDWriteFactory();
    if (!dwrite) return E_FAIL;

    // DirectWrite Typography
    HRESULT hr = dwrite->CreateTextFormat(
        L"Segoe UI",
        nullptr,
        DWRITE_FONT_WEIGHT_SEMI_BOLD,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        21.0f,
        L"en-us",
        &g_titleFormat
    );
    if (FAILED(hr)) return hr;

    hr = dwrite->CreateTextFormat(
        L"Consolas",
        nullptr,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        13.5f,
        L"en-us",
        &g_hudFormat
    );
    if (FAILED(hr)) return hr;

    // Create a procedural 8-pointed star geometry
    ID2D1Factory1* d2dFactory = g_resources.GetD2DFactory();
    if (!d2dFactory) return E_FAIL;

    hr = d2dFactory->CreatePathGeometry(&g_starGeometry);
    if (FAILED(hr)) return hr;

    Microsoft::WRL::ComPtr<ID2D1GeometrySink> sink;
    hr = g_starGeometry->Open(&sink);
    if (FAILED(hr)) return hr;

    const int points = 8;
    const float outerRadius = 120.0f;
    const float innerRadius = 60.0f;
    const float angleStep = 3.1415926535f / points;

    sink->BeginFigure(D2D1::Point2F(0.0f, -outerRadius), D2D1_FIGURE_BEGIN_FILLED);
    for (int i = 1; i < points * 2; ++i) {
        float r = (i % 2 == 0) ? outerRadius : innerRadius;
        float a = -1.5707963267f + i * angleStep;
        sink->AddLine(D2D1::Point2F(std::cos(a) * r, std::sin(a) * r));
    }
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    hr = sink->Close();

    return hr;
}

// ----------------------------------------------------------------------------
// Render Frame
// ----------------------------------------------------------------------------
void RenderFrame()
{
    g_resources.BeginDraw();

    ID2D1DeviceContext* ctx = g_resources.GetD2DContext();
    if (!ctx) return;

    D2D1_SIZE_F size = g_resources.GetLogicalSize();

    // 1. Clear background (Sleek dark obsidian)
    ctx->Clear(D2D1::ColorF(0.05f, 0.06f, 0.08f, 1.0f));

    // 2. Draw animated background grid
    if (g_accentBrush) {
        g_accentBrush->SetOpacity(0.08f);
        const float gridSize = 40.0f;
        for (float x = 0.0f; x < size.width; x += gridSize) {
            ctx->DrawLine(D2D1::Point2F(x, 0.0f), D2D1::Point2F(x, size.height), g_accentBrush.Get(), 1.0f);
        }
        for (float y = 0.0f; y < size.height; y += gridSize) {
            ctx->DrawLine(D2D1::Point2F(0.0f, y), D2D1::Point2F(size.width, y), g_accentBrush.Get(), 1.0f);
        }
        g_accentBrush->SetOpacity(1.0f);
    }

    // 3. Draw Center Animated Star & Radial Halo
    D2D1_POINT_2F center = D2D1::Point2F(size.width * 0.5f, size.height * 0.53f);
    D2D1_MATRIX_3X2_F transform =
        D2D1::Matrix3x2F::Rotation(g_animationTime * 45.0f, D2D1::Point2F(0, 0)) *
        D2D1::Matrix3x2F::Scale(1.0f + 0.05f * std::sin(g_animationTime * 3.0f),
                                1.0f + 0.05f * std::sin(g_animationTime * 3.0f),
                                D2D1::Point2F(0, 0)) *
        D2D1::Matrix3x2F::Translation(center.x, center.y);

    ctx->SetTransform(transform);

    if (g_glowBrush) {
        ctx->FillEllipse(D2D1::Ellipse(D2D1::Point2F(0, 0), 180.0f, 180.0f), g_glowBrush.Get());
    }

    if (g_starGeometry) {
        if (g_bitmapBrush && g_gpuTexture) {
            // Center the bitmap texture over geometry
            D2D1_SIZE_F texSize = g_gpuTexture->GetSize();
            D2D1_MATRIX_3X2_F texTransform = D2D1::Matrix3x2F::Translation(-texSize.width * 0.5f, -texSize.height * 0.5f);
            g_bitmapBrush->SetTransform(texTransform);
            ctx->FillGeometry(g_starGeometry.Get(), g_bitmapBrush.Get());
        } else if (g_geometryBrush) {
            ctx->FillGeometry(g_starGeometry.Get(), g_geometryBrush.Get());
        }

        if (g_accentBrush) {
            ctx->DrawGeometry(g_starGeometry.Get(), g_accentBrush.Get(), 2.5f);
        }
    }

    // Reset transform for UI Overlay
    ctx->SetTransform(D2D1::Matrix3x2F::Identity());

    // 4. Render HUD & Status Overlay
    if (g_titleFormat && g_textBrush && g_hudFormat && g_accentBrush) {
        // Header
        ctx->DrawText(
            L"Direct2D & WIC Device Loss Recovery Test Bench",
            45,
            g_titleFormat.Get(),
            D2D1::RectF(25.0f, 25.0f, size.width - 25.0f, 60.0f),
            g_textBrush.Get()
        );

        // Telemetry readout
        std::wstringstream ss;
        ss << L"Status:      " << std::wstring(g_statusBanner.begin(), g_statusBanner.end()) << L"\n"
           << L"Loss Events: " << g_deviceLossEventCount << L" recovered\n"
           << L"Active Tex:  " << g_loadedImageName << L"\n"
           << L"Surface:     " << static_cast<int>(size.width) << L" x " << static_cast<int>(size.height) << L" (FLIP_SEQUENTIAL)\n\n"
           << L"Controls:    Press [SPACE] or [L] to trigger GPU Device Loss\n"
           << L"             Drag & Drop any image file (.png, .jpg, .tiff) onto window\n"
           << L"             Press [ESC] to Exit";

        std::wstring hudText = ss.str();
        ctx->DrawText(
            hudText.c_str(),
            static_cast<UINT32>(hudText.length()),
            g_hudFormat.Get(),
            D2D1::RectF(25.0f, 75.0f, size.width - 25.0f, size.height - 25.0f),
            (g_deviceLossEventCount > 0) ? g_accentBrush.Get() : g_textBrush.Get()
        );
    }

    // 5. Complete Draw & Present
    HRESULT hr = g_resources.EndDraw();
    if (SUCCEEDED(hr)) {
        g_resources.Present(1);
    }
}

// ----------------------------------------------------------------------------
// Window Procedure
// ----------------------------------------------------------------------------
LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED) {
            g_resources.CreateWindowSizeDependentResources();
        }
        return 0;

    case WM_DPICHANGED: {
        float newDpi = static_cast<float>(LOWORD(wParam));
        g_resources.SetDpi(newDpi);
        RECT* prcNewWindow = reinterpret_cast<RECT*>(lParam);
        SetWindowPos(hwnd, nullptr,
            prcNewWindow->left, prcNewWindow->top,
            prcNewWindow->right - prcNewWindow->left,
            prcNewWindow->bottom - prcNewWindow->top,
            SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }

    case WM_DROPFILES: {
        HDROP hDrop = reinterpret_cast<HDROP>(wParam);
        WCHAR szFilePath[MAX_PATH] = {};
        if (DragQueryFileW(hDrop, 0, szFilePath, MAX_PATH)) {
            // Read file bytes into CPU backing store
            std::ifstream file(szFilePath, std::ios::binary | std::ios::ate);
            if (file.is_open()) {
                std::streamsize fileSize = file.tellg();
                file.seekg(0, std::ios::beg);

                g_loadedImageData.resize(static_cast<size_t>(fileSize));
                if (file.read(reinterpret_cast<char*>(g_loadedImageData.data()), fileSize)) {
                    // Extract basename
                    std::wstring fullPath(szFilePath);
                    size_t lastSlash = fullPath.find_last_of(L"\\/");
                    g_loadedImageName = (lastSlash != std::wstring::npos) ? fullPath.substr(lastSlash + 1) : fullPath;

                    // Re-instantiate device dependent resources to upload GPU texture
                    CreateDeviceDependentResources(g_resources.GetD2DContext());
                    g_statusBanner = "WIC TEXTURE LOADED — READY FOR STRESS TEST";
                }
            }
        }
        DragFinish(hDrop);
        return 0;
    }

    case WM_KEYDOWN:
        if (wParam == VK_SPACE || wParam == 'L' || wParam == 'l') {
            // ================================================================
            // SIMULATE DIRECT HARDWARE DEVICE LOSS / TDR
            // ================================================================
            g_deviceLossEventCount++;
            g_statusBanner = "DEVICE LOSS DETECTED — RECOVERING PIPELINE...";

            // Inject simulated DXGI_ERROR_DEVICE_RESET
            g_resources.HandleDeviceLost(d2d_engine::DeviceLostReason::DeviceReset);

            g_statusBanner = "OPERATIONAL — RECOVERY SUCCESSFUL (" + std::to_string(g_deviceLossEventCount) + "x)";
            return 0;
        } else if (wParam == VK_ESCAPE) {
            PostQuitMessage(0);
            return 0;
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

// ----------------------------------------------------------------------------
// Entry Point
// ----------------------------------------------------------------------------
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
    // Initialize COM
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(hr)) return 1;

    // Register Window Class
    WNDCLASSEXW wcex = { sizeof(WNDCLASSEXW) };
    wcex.style         = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc   = WndProc;
    wcex.cbClsExtra    = 0;
    wcex.cbWndExtra    = 0;
    wcex.hInstance     = hInstance;
    wcex.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = nullptr;
    wcex.lpszClassName = L"D2DDeviceLossTestBench";

    if (!RegisterClassExW(&wcex)) {
        CoUninitialize();
        return 1;
    }

    // Create Centered Window
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int winW = 880;
    int winH = 660;

    HWND hwnd = CreateWindowExW(
        0,
        wcex.lpszClassName,
        L"Direct2D & WIC Device Loss Recovery Test Bench",
        WS_OVERLAPPEDWINDOW,
        (screenW - winW) / 2,
        (screenH - winH) / 2,
        winW,
        winH,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    if (!hwnd) {
        CoUninitialize();
        return 1;
    }

    // Enable Drag & Drop
    DragAcceptFiles(hwnd, TRUE);

    // Wire up DeviceResources callbacks
    g_resources.OnCreateDeviceDependentResources = CreateDeviceDependentResources;
    g_resources.OnReleaseDeviceDependentResources = ReleaseDeviceDependentResources;

    // Initialize Device Resources & DirectWrite
    if (FAILED(g_resources.Initialize(hwnd)) || FAILED(CreateDeviceIndependentResources())) {
        MessageBoxW(hwnd, L"Failed to initialize DirectX 11.1 / Direct2D pipeline.", L"Fatal Error", MB_ICONERROR);
        DestroyWindow(hwnd);
        CoUninitialize();
        return 1;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // Continuous Animation / Message Loop
    MSG msg = {};
    auto lastTime = std::chrono::high_resolution_clock::now();

    while (msg.message != WM_QUIT) {
        if (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        } else {
            auto currentTime = std::chrono::high_resolution_clock::now();
            float dt = std::chrono::duration<float>(currentTime - lastTime).count();
            lastTime = currentTime;

            g_animationTime += dt;
            RenderFrame();
        }
    }

    ReleaseDeviceDependentResources();
    g_titleFormat.Reset();
    g_hudFormat.Reset();
    g_starGeometry.Reset();

    CoUninitialize();
    return static_cast<int>(msg.wParam);
}
