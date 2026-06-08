/*    ExternalSettingsWindow (Copyright (C) 2026 MalNEW)
    
    本程序是自由软件：您可以根据自由软件基金会发布的 GNU 通用公共许可证（第 3 版）或（根据您的选择）任何后续版本的条款对其进行重新分发和/或修改。
    
    本程序的分发是希望它能有用，但没有任何担保；甚至没有对适销性或特定用途适用性的暗示担保。有关详细信息，请参阅 GNU 通用公共许可证。
    
    您应该已随本程序收到一份 GNU 通用公共许可证的副本。如果没有，请参阅 <http://gnu.org>。
*/

#include "imgui_src/imgui.h"
#include "imgui_src/backends/imgui_impl_win32.h"
#include "imgui_src/backends/imgui_impl_dx11.h"
#include <d3d11.h>
#include <windows.h>
#include <atomic>
#include <cstdio>

// === File-based bridge: JSON format ===
// {"note_speed":10.0,"timing_offset":0,"visual_offset":0,"gimmick_offset":0,
//  "quick_restart":0,"pause_unfocus":0,"lane_beam":50,"note_sounds":1,
//  "note_sound_vol":100,"hitsound_ticks":1,"top_display":1,"judge_counter":0,
//  "reduced_particles":0,"fc_indicator":0,"early_late":2,"judge_display":1,
//  "mirror":0,"note_alignment":0}
//
// Keys read from file, missing keys use defaults.

struct BridgeData {
    float note_speed = 10.0f;
    float timing_offset = 0;
    float visual_offset = 0;
    float gimmick_offset = 0;
    int   quick_restart = 0;
    int   pause_unfocus = 0;
    float lane_beam = 50;
    int   note_sounds = 1;
    float note_sound_vol = 100;
    int   hitsound_ticks = 1;
    int   top_display = 1;
    int   judge_counter = 0;
    int   reduced_particles = 0;
    int   fc_indicator = 0;
    int   early_late = 2;
    int   judge_display = 1;
    int   mirror = 0;
    int   note_alignment = 0;
};
static BridgeData g_data;
static BridgeData g_prev_data;

static char g_bridgePath[MAX_PATH] = {0};

static void BuildBridgePath()
{
    if (g_bridgePath[0]) return;
    ExpandEnvironmentStringsA("%LOCALAPPDATA%\\VIVIDSTASIS\\imgui_bridge.json", g_bridgePath, MAX_PATH);
}

// Simple JSON reader: find "key": value in file
static float ReadKey(FILE* f, const char* key)
{
    char buf[512] = {0};
    fseek(f, 0, SEEK_SET);
    fread(buf, 1, sizeof(buf)-1, f);
    const char* pos = strstr(buf, key);
    if (pos) {
        pos = strstr(pos, ":");
        if (pos) return (float)atof(pos+1);
    }
    return 0;
}

// Build JSON from current data
static void WriteData()
{
    BuildBridgePath();
    FILE* f = fopen(g_bridgePath, "w");
    if (!f) return;
    fprintf(f, "{\"note_speed\":%.1f,\"timing_offset\":%.0f,\"visual_offset\":%.0f,"
               "\"gimmick_offset\":%.0f,\"quick_restart\":%d,\"pause_unfocus\":%d,"
               "\"lane_beam\":%.0f,\"note_sounds\":%d,\"note_sound_vol\":%.0f,"
               "\"hitsound_ticks\":%d,\"top_display\":%d,\"judge_counter\":%d,"
               "\"reduced_particles\":%d,\"fc_indicator\":%d,\"early_late\":%d,"
               "\"judge_display\":%d,\"mirror\":%d,\"note_alignment\":%d}\n",
        g_data.note_speed, g_data.timing_offset, g_data.visual_offset,
        g_data.gimmick_offset, g_data.quick_restart, g_data.pause_unfocus,
        g_data.lane_beam, g_data.note_sounds, g_data.note_sound_vol,
        g_data.hitsound_ticks, g_data.top_display, g_data.judge_counter,
        g_data.reduced_particles, g_data.fc_indicator, g_data.early_late,
        g_data.judge_display, g_data.mirror, g_data.note_alignment);
    fflush(f);
    fclose(f);
}

static void ReadData()
{
    BuildBridgePath();
    FILE* f = fopen(g_bridgePath, "r");
    if (!f) return;
    g_data.note_speed      = ReadKey(f, "\"note_speed\"");
    g_data.timing_offset   = ReadKey(f, "\"timing_offset\"");
    g_data.visual_offset   = ReadKey(f, "\"visual_offset\"");
    g_data.gimmick_offset  = ReadKey(f, "\"gimmick_offset\"");
    g_data.quick_restart   = (int)ReadKey(f, "\"quick_restart\"");
    g_data.pause_unfocus   = (int)ReadKey(f, "\"pause_unfocus\"");
    g_data.lane_beam       = ReadKey(f, "\"lane_beam\"");
    g_data.note_sounds     = (int)ReadKey(f, "\"note_sounds\"");
    g_data.note_sound_vol  = ReadKey(f, "\"note_sound_vol\"");
    g_data.hitsound_ticks  = (int)ReadKey(f, "\"hitsound_ticks\"");
    g_data.top_display     = (int)ReadKey(f, "\"top_display\"");
    g_data.judge_counter   = (int)ReadKey(f, "\"judge_counter\"");
    g_data.reduced_particles = (int)ReadKey(f, "\"reduced_particles\"");
    g_data.fc_indicator    = (int)ReadKey(f, "\"fc_indicator\"");
    g_data.early_late      = (int)ReadKey(f, "\"early_late\"");
    g_data.judge_display   = (int)ReadKey(f, "\"judge_display\"");
    g_data.mirror          = (int)ReadKey(f, "\"mirror\"");
    g_data.note_alignment  = (int)ReadKey(f, "\"note_alignment\"");
    fclose(f);
}

// === D3D State ===
static ID3D11Device*            g_pd3dDevice = nullptr;
static ID3D11DeviceContext*     g_pd3dDeviceContext = nullptr;
static IDXGISwapChain*          g_pSwapChain = nullptr;
static UINT                     g_ResizeWidth = 0, g_ResizeHeight = 0;
static ID3D11RenderTargetView*  g_mainRenderTargetView = nullptr;
static HWND                     g_hWnd = nullptr;
static bool                     g_running = false;
static bool                     g_visible = true;
static std::atomic<bool>        g_initialized(false);
static HANDLE                   g_hThread = nullptr;

bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    return TRUE;
}

static void ImGuiUI()
{
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(400, 570));
    ImGui::Begin("VIVIDSTASIS Gameplay", nullptr,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);

    // Drag title bar area using raw Windows cursor position
    {
        static POINT grab = {0,0};
        static bool dragging = false;
        POINT cp; GetCursorPos(&cp);
        RECT wr; GetWindowRect(g_hWnd, &wr);
        int relY = cp.y - wr.top;

        if (!dragging && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && relY < 20) {
            grab.x = cp.x - wr.left;
            grab.y = cp.y - wr.top;
            dragging = true;
        }
        if (dragging) {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                GetCursorPos(&cp);
                SetWindowPos(g_hWnd, nullptr, cp.x - grab.x, cp.y - grab.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
            } else {
                dragging = false;
            }
        }
    }
    ImGui::TextColored(ImVec4(0.5f,0.5f,0.6f,1.0f), "VIVIDSTASIS (F1)"); ImGui::Separator();

    bool changed = false;

    // -- 游玩设置 --
    ImGui::SeparatorText("游玩设置");
    changed |= ImGui::SliderFloat("音符流速", &g_data.note_speed, 1.0f, 20.0f, "%.1f");
    changed |= ImGui::SliderFloat("音频延迟", &g_data.timing_offset, -300.0f, 300.0f, "%.0f");
    changed |= ImGui::SliderFloat("视觉延迟", &g_data.visual_offset, -300.0f, 300.0f, "%.0f");
    changed |= ImGui::SliderFloat("异象演出延迟", &g_data.gimmick_offset, -300.0f, 300.0f, "%.0f");
    changed |= ImGui::SliderFloat("轨道按键特效亮度", &g_data.lane_beam, 0.0f, 100.0f, "%.0f");

    // -- 显示设置 --
    ImGui::SeparatorText("显示设置");
    const char* top_items[] = {"无","连击数","EX分数","完成度(+)","完成度(-)","准度(100.0%)","准度(1.00%)"};
    changed |= ImGui::Combo("顶端显示类型", &g_data.top_display, top_items, 7);
    const char* early_items[] = {"无","完美及以下","良好及以下"};
    changed |= ImGui::Combo("按键快/慢显示", &g_data.early_late, early_items, 3);
    const char* judge_items[] = {"无","新版","经典","图形"};
    changed |= ImGui::Combo("判定显示模式", &g_data.judge_display, judge_items, 4);
    changed |= ImGui::Checkbox("左下角判定计数显示", (bool*)&g_data.judge_counter);
    changed |= ImGui::Checkbox("FC/AC指示器", (bool*)&g_data.fc_indicator);
    changed |= ImGui::Checkbox("减少粒子效果", (bool*)&g_data.reduced_particles);

    // -- 音频设置 --
    ImGui::SeparatorText("音频设置");
    const char* hitsound_items[] = {"无音效","类型1","类型2","类型3","类型4"};
    changed |= ImGui::Combo("打击音类型", &g_data.note_sounds, hitsound_items, 5);
    changed |= ImGui::SliderFloat("打击音音量", &g_data.note_sound_vol, 0.0f, 100.0f, "%.0f");
    changed |= ImGui::Checkbox("长条按住时播放打击音", (bool*)&g_data.hitsound_ticks);

    // -- 杂项 --
    ImGui::SeparatorText("杂项");
    changed |= ImGui::Checkbox("快速重新开始", (bool*)&g_data.quick_restart);
    changed |= ImGui::Checkbox("失焦时暂停游戏", (bool*)&g_data.pause_unfocus);
    changed |= ImGui::Checkbox("镜像", (bool*)&g_data.mirror);
    const char* align_items[] = {"音符顶部","音符底部"};
    changed |= ImGui::Combo("音符判定位置", &g_data.note_alignment, align_items, 2);

    ImGui::Separator();
    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);

    ImGui::End();

    if (changed) WriteData();
}

// === Render Thread ===
DWORD WINAPI ImGuiThreadProc(LPVOID lpParam)
{
    g_running = true;
    ImGui_ImplWin32_EnableDpiAwareness();
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"ImGuiWindow", nullptr };
    ::RegisterClassExW(&wc);
    g_hWnd = ::CreateWindowW(wc.lpszClassName, L"VIVIDSTASIS ImGui", WS_POPUP, 100, 100, 400, 570, nullptr, nullptr, wc.hInstance, nullptr);
    ::SetWindowPos(g_hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

    if (!CreateDeviceD3D(g_hWnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        g_running = false;
        return 1;
    }
    ::ShowWindow(g_hWnd, SW_SHOW);
    ::UpdateWindow(g_hWnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();

    // Load Chinese font (Microsoft YaHei)
    {
        ImFontConfig fontConfig;
        fontConfig.MergeMode = false;
        static const ImWchar ranges[] = { 0x0020, 0xFFFF, 0 };
        io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\msyh.ttc", 16.0f, &fontConfig, ranges);
    }

    ImGui_ImplWin32_Init(g_hWnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // Register global F1 hotkey (Ctrl+Shift+F1 to avoid conflicts)
    RegisterHotKey(g_hWnd, 1, MOD_CONTROL | MOD_SHIFT, VK_F1);
    // Also try simple F1 via low-level keyboard hook via WndProc WM_KEYDOWN with global state
    // Use a timer to poll GetAsyncKeyState for F1 regardless of focus
    SetTimer(g_hWnd, 1, 100, nullptr);

    // Read existing bridge file (don't write — GML handles init)
    ReadData();
    g_initialized.store(true);

    while (g_running)
    {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT) g_running = false;
        }
        if (!g_running) break;

        if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeWidth = g_ResizeHeight = 0;
            CreateRenderTarget();
        }

        ReadData();
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        ImGuiUI();
        ImGui::Render();
        const float clear_color[] = { 0.1f, 0.1f, 0.15f, 1.0f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_pSwapChain->Present(1, 0);
    }

    g_initialized.store(false);
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
    ::DestroyWindow(g_hWnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
    g_hWnd = nullptr;
    return 0;
}

extern "C" __declspec(dllexport) void ImGui_Show(void)
{
    if (g_initialized.load()) return;
    g_hThread = CreateThread(nullptr, 0, ImGuiThreadProc, nullptr, 0, nullptr);
}

extern "C" __declspec(dllexport) void ImGui_Hide(void)
{
    g_running = false;
    if (g_hThread) { WaitForSingleObject(g_hThread, 3000); CloseHandle(g_hThread); g_hThread = nullptr; }
}

// === D3D Helpers ===
bool CreateDeviceD3D(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK) return false;
    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;
    switch (msg)
    {
    case WM_TIMER:
        if (wParam == 1) {
            // Global F1 detection via GetAsyncKeyState (works regardless of focus)
            static bool f1_was_down = false;
            bool f1_down = (GetAsyncKeyState(VK_F1) & 0x8000) != 0;
            if (f1_down && !f1_was_down) {
                g_visible = !g_visible;
                ::ShowWindow(g_hWnd, g_visible ? SW_SHOW : SW_HIDE);
            }
            f1_was_down = f1_down;
        }
        return 0;
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED) return 0;
        g_ResizeWidth = (UINT)LOWORD(lParam);
        g_ResizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}
