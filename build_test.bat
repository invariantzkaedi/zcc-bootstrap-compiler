@echo off
setlocal EnableDelayedExpansion

:: =============================================================================
::  build_test.bat  â€”  ZKAEDI Fleet Viewer  â€”  Phase 3 Build Gate
::  Targets: bin\zkaedi_fleet_viewer.exe
::  Prerequisites fulfilled by this script:
::    1. MSVC x64 environment detection via vswhere
::    2. d3dx12.h fetch if absent (UpdateSubresources / GetRequiredIntermediateSize)
::    3. HLSL shader compilation  â†’ bin\VS_Simple.cso, bin\PS_Unlit.cso
::    4. C++ monolith compilation â†’ bin\zkaedi_fleet_viewer.exe
::    5. Phase 3 gate report
:: =============================================================================

echo.
echo =============================================================================
echo  ZKAEDI Fleet Viewer  â€”  Phase 3  â€”  Build Gate
echo =============================================================================
echo.

:: ---------------------------------------------------------------------------
:: GATE-0 : Critical bug check â€” g_ctx null deref in WinMain
:: ---------------------------------------------------------------------------
echo [GATE-0] Checking for g_ctx null-deref bug in src\zkaedi_d3d12_viewer.cpp ...
findstr /C:"RenderFrame(&g_ctx[0])" src\zkaedi_d3d12_viewer.cpp >nul 2>&1
if %errorlevel%==0 (
    echo [FAIL] BUG DETECTED: Line calls RenderFrame^(&g_ctx[0]^) but g_ctx is never assigned.
    echo        This is a runtime null-pointer dereference on the message pump.
    echo        Auto-patching with PowerShell sed-equivalent...
    powershell -Command ^
        "(Get-Content 'src\zkaedi_d3d12_viewer.cpp') -replace 'RenderFrame\(&g_ctx\[0\]\)', 'RenderFrame(&ctx)' | Set-Content 'src\zkaedi_d3d12_viewer.cpp'"
    if !errorlevel! neq 0 (
        echo [FATAL] Patch failed. Edit manually: replace RenderFrame^(&g_ctx[0]^) with RenderFrame^(&ctx^)
        goto :BUILD_FAIL
    )
    echo [PASS] Bug patched: RenderFrame^(&g_ctx[0]^) ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ RenderFrame^(&ctx^)
) else (
    echo [PASS] g_ctx deref bug not present ^(already patched^).
)
echo.

:: ---------------------------------------------------------------------------
:: GATE-1 : Locate MSVC x64 toolchain via vswhere
:: ---------------------------------------------------------------------------
echo [GATE-1] Locating MSVC toolchain ...

set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat"
if exist "%VCVARS%" set "VS_INSTALL=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools"
if exist "%VCVARS%" goto :GATE_2_START

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "!VSWHERE!" (
    :: Fallback: try Program Files (ARM/x64 native)
    set "VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"
)
if not exist "!VSWHERE!" (
    echo [FATAL] vswhere.exe not found. Install Visual Studio 2022 with C++ Desktop workload.
    goto :BUILD_FAIL
)

for /f "tokens=* usebackq" %%p in (`"!VSWHERE!" -products * -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    set "VS_INSTALL=%%p"
)
if "!VS_INSTALL!"=="" (
    echo [FATAL] No VS installation with C++ tools found.
    goto :BUILD_FAIL
)

set "VCVARS=!VS_INSTALL!\VC\Auxiliary\Build\vcvarsall.bat"
if not exist "!VCVARS!" (
    echo [FATAL] vcvarsall.bat not found at: !VCVARS!
    goto :BUILD_FAIL
)

echo [PASS] VS toolchain: !VS_INSTALL!
echo.

:GATE_2_START

:: ---------------------------------------------------------------------------
:: GATE-2 : Initialize x64 environment
:: ---------------------------------------------------------------------------
echo [GATE-2] Initializing MSVC x64 environment ...
call "!VCVARS!" amd64 >nul 2>&1
if %errorlevel% neq 0 (
    echo [FATAL] vcvarsall.bat amd64 failed.
    goto :BUILD_FAIL
)
echo [PASS] MSVC x64 environment ready.
echo.

:: ---------------------------------------------------------------------------
:: GATE-3 : Verify required source files
:: ---------------------------------------------------------------------------
echo [GATE-3] Verifying source files ...

if not exist "src\zkaedi_d3d12_viewer.cpp" (
    echo [FATAL] src\zkaedi_d3d12_viewer.cpp not found.
    goto :BUILD_FAIL
)
echo [PASS] src\zkaedi_d3d12_viewer.cpp present.

if not exist "include\cgltf.h" (
    echo [WARN] include\cgltf.h missing ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â fetching ...
    powershell -Command "Invoke-WebRequest -Uri 'https://raw.githubusercontent.com/jkuhlmann/cgltf/master/cgltf.h' -OutFile 'include\cgltf.h'"
    if !errorlevel! neq 0 ( echo [FATAL] cgltf.h download failed. & goto :BUILD_FAIL )
    echo [PASS] cgltf.h fetched.
) else (
    echo [PASS] include\cgltf.h present.
)

if not exist "include\stb_image.h" (
    echo [WARN] include\stb_image.h missing - fetching ...
    powershell -Command "Invoke-WebRequest -Uri 'https://raw.githubusercontent.com/nothings/stb/master/stb_image.h' -OutFile 'include\stb_image.h'"
    if !errorlevel! neq 0 ( echo [FATAL] stb_image.h download failed. & goto :BUILD_FAIL )
    echo [PASS] stb_image.h fetched.
) else (
    echo [PASS] include\stb_image.h present.
)

echo.

:: ---------------------------------------------------------------------------
:: GATE-4 : Fetch d3dx12.h (UpdateSubresources / GetRequiredIntermediateSize)
:: ---------------------------------------------------------------------------
echo [GATE-4] Checking for d3dx12.h helper header ...

if not exist "include\d3dx12.h" (
    echo [INFO] d3dx12.h absent ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â fetching from DirectX-Graphics-Samples ...
    powershell -Command ^
        "Invoke-WebRequest -Uri 'https://raw.githubusercontent.com/microsoft/DirectX-Graphics-Samples/master/Samples/Desktop/D3D12HelloWorld/src/HelloTexture/d3dx12.h' -OutFile 'include\d3dx12.h'"
    if !errorlevel! neq 0 (
        echo [FATAL] d3dx12.h download failed. Check network or place manually at include\d3dx12.h
        goto :BUILD_FAIL
    )
    echo [PASS] d3dx12.h fetched ^(UpdateSubresources + GetRequiredIntermediateSize available^).
) else (
    echo [PASS] include\d3dx12.h present.
)
echo.

:: ---------------------------------------------------------------------------
:: GATE-5 : Ensure output directories exist
:: ---------------------------------------------------------------------------
if not exist "bin"     mkdir bin
if not exist "shaders" mkdir shaders
echo [PASS] Output directories verified.
echo.

:: ---------------------------------------------------------------------------
:: GATE-6 : Compile HLSL shaders with fxc.exe
::          VS_Simple.hlsl  → bin\VS_Simple.cso  (vs_5_1)
::          PS_Unlit.hlsl   → bin\PS_Unlit.cso   (ps_5_1)
:: ---------------------------------------------------------------------------
echo [GATE-6] Compiling HLSL shaders ...

:: Check that shader source files exist
if not exist "shaders\VS_Simple.hlsl" (
    echo [FATAL] shaders\VS_Simple.hlsl not found. Place Phase 3 shader files in shaders\
    goto :BUILD_FAIL
)
if not exist "shaders\PS_Unlit.hlsl" (
    echo [FATAL] shaders\PS_Unlit.hlsl not found. Place Phase 3 shader files in shaders\
    goto :BUILD_FAIL
)

:: Compile Vertex Shader
fxc.exe /nologo /T vs_5_1 /E VS_Simple /Fo "bin\VS_Simple.cso" /Od /Zi "shaders\VS_Simple.hlsl"
if %errorlevel% neq 0 (
    echo [FATAL] VS_Simple.hlsl compilation failed.
    goto :BUILD_FAIL
)
echo [PASS] bin\VS_Simple.cso compiled ^(vs_5_1^).

:: Compile Pixel Shader
fxc.exe /nologo /T ps_5_1 /E PS_Unlit /Fo "bin\PS_Unlit.cso" /Od /Zi "shaders\PS_Unlit.hlsl"
if %errorlevel% neq 0 (
    echo [FATAL] PS_Unlit.hlsl compilation failed.
    goto :BUILD_FAIL
)
echo [PASS] bin\PS_Unlit.cso compiled ^(ps_5_1^).

:: Compile Rigged Vertex Shader (VS_XboxMain) via DXC targeting vs_6_0
set "DXC_PATH=C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\dxc.exe"
if not exist "!DXC_PATH!" (
    set "DXC_PATH=dxc.exe"
)
if not exist "shaders\VS_XboxMain.hlsl" (
    if exist "bin\VS_XboxMain.cso" (
        echo [WARN] shaders\VS_XboxMain.hlsl missing - using precompiled bin\VS_XboxMain.cso.
    ) else (
        echo [FATAL] shaders\VS_XboxMain.hlsl not found.
        goto :BUILD_FAIL
    )
) else (
    "!DXC_PATH!" /nologo /T vs_6_0 /E VS_XboxMain /Fo "bin\VS_XboxMain.cso" "shaders\VS_XboxMain.hlsl"
    if !errorlevel! neq 0 (
        echo [FATAL] VS_XboxMain.hlsl compilation failed.
        goto :BUILD_FAIL
    )
    echo [PASS] bin\VS_XboxMain.cso compiled ^(vs_6_0^).
)

:: Compile Rigged Pixel Shader (PS_Skinned) via DXC targeting ps_6_0
if not exist "shaders\PS_Skinned.hlsl" (
    if exist "bin\PS_Skinned.cso" (
        echo [WARN] shaders\PS_Skinned.hlsl missing - using precompiled bin\PS_Skinned.cso.
    ) else (
        echo [FATAL] shaders\PS_Skinned.hlsl not found.
        goto :BUILD_FAIL
    )
) else (
    "!DXC_PATH!" /nologo /T ps_6_0 /E PS_Skinned /Fo "bin\PS_Skinned.cso" "shaders\PS_Skinned.hlsl"
    if !errorlevel! neq 0 (
        echo [FATAL] PS_Skinned.hlsl compilation failed.
        goto :BUILD_FAIL
    )
    echo [PASS] bin\PS_Skinned.cso compiled ^(ps_6_0^).
)
echo.

:: ---------------------------------------------------------------------------
:: GATE-7 : Compile zkaedi_d3d12_viewer.cpp ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ bin\zkaedi_fleet_viewer.exe
:: ---------------------------------------------------------------------------
echo [GATE-7] Compiling zkaedi_d3d12_viewer.cpp ...

:: Clean stale obj if present
if exist "bin\zkaedi_d3d12_viewer.obj" del /q "bin\zkaedi_d3d12_viewer.obj"

cl.exe ^
    /nologo ^
    /W3 /WX- ^
    /EHsc ^
    /std:c++17 ^
    /MD ^
    /O2 ^
    /Zi ^
    /I "include" ^
    /I "directstorage\native\include" ^
    /Fo"bin\\" ^
    /Fe"bin\zkaedi_fleet_viewer.exe" ^
    /Fd"bin\zkaedi_fleet_viewer.pdb" ^
    "src\zkaedi_d3d12_viewer.cpp" ^
    d3d12.lib dxgi.lib user32.lib dstorage.lib ^
    /link /LIBPATH:"directstorage\native\lib\x64"

if %errorlevel% neq 0 (
    echo.
    echo [FATAL] C++ compilation failed. See errors above.
    goto :BUILD_FAIL
)
echo [PASS] bin\zkaedi_fleet_viewer.exe compiled successfully.
echo.

:: ---------------------------------------------------------------------------
:: GATE-8 : Verify output binary exists and is non-zero
:: ---------------------------------------------------------------------------
echo [GATE-8] Verifying output binary ...

if not exist "bin\zkaedi_fleet_viewer.exe" (
    echo [FATAL] bin\zkaedi_fleet_viewer.exe not produced.
    goto :BUILD_FAIL
)

for %%F in ("bin\zkaedi_fleet_viewer.exe") do set EXE_SIZE=%%~zF
if "!EXE_SIZE!"=="0" (
    echo [FATAL] bin\zkaedi_fleet_viewer.exe is zero bytes.
    goto :BUILD_FAIL
)

echo [PASS] bin\zkaedi_fleet_viewer.exe  [!EXE_SIZE! bytes]
echo.

:: ---------------------------------------------------------------------------
:: Phase 3 Gate  â€”  ALL GREEN
:: ---------------------------------------------------------------------------
echo =============================================================================
echo  ðŸ”± PHASE 3 GATE: ALL PASS
echo     bin\VS_Simple.cso         âœ“
echo     bin\PS_Unlit.cso          âœ“
echo     bin\zkaedi_fleet_viewer.exe  âœ“  [!EXE_SIZE! bytes]
echo =============================================================================
echo.
echo  To launch viewer:
echo    cd /d %~dp0
echo    bin\zkaedi_fleet_viewer.exe
echo.
echo  Target GLB:
echo    H:\_studio_tripo3d\3d_tools\3D_assets\jackpot_casino_games\
echo    Meshy_AI_Infernal777reactormac_0520004658_generate.glb
echo.
echo  Controls: orbital camera auto-rotates. Close window to exit cleanly.
echo =============================================================================
goto :EOF

:BUILD_FAIL
echo.
echo =============================================================================
echo  ðŸ”± PHASE 3 GATE: FAILED â€” See [FATAL] lines above.
echo =============================================================================
exit /b 1
