#include "imgui_window.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "ui_components.h"
#include "plugin.h"
#include <d3d11.h>
#include <cstdio>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

static HWND g_imgui_hwnd = nullptr;
static ID3D11Device* g_d3d_device = nullptr;
static ID3D11DeviceContext* g_d3d_context = nullptr;
static IDXGISwapChain* g_swap_chain = nullptr;
static ID3D11RenderTargetView* g_rtv = nullptr;
static bool g_visible = false;
static bool g_initialized = false;
static bool g_rendering = false;
static void (*g_on_generate)() = nullptr;
static WNDCLASSEXW g_wc = {};

#define TIMER_RENDER 1

static bool create_render_target() {
    if (g_rtv) { g_rtv->Release(); g_rtv = nullptr; }
    ID3D11Texture2D* bb = nullptr;
    HRESULT hr = g_swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&bb);
    if (FAILED(hr) || !bb) return false;
    hr = g_d3d_device->CreateRenderTargetView(bb, nullptr, &g_rtv);
    bb->Release();
    return SUCCEEDED(hr) && g_rtv;
}

static bool create_device_and_swapchain() {
    RECT rc;
    GetClientRect(g_imgui_hwnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 0) w = 900;
    if (h <= 0) h = 650;

    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = w;
    sd.BufferDesc.Height = h;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = g_imgui_hwnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL feature_level;
    const D3D_FEATURE_LEVEL feature_levels[] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };

    UINT create_flags = 0;
#ifdef _DEBUG
    create_flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, create_flags,
        feature_levels, 3, D3D11_SDK_VERSION,
        &sd, &g_swap_chain, &g_d3d_device, &feature_level, &g_d3d_context);
    if (FAILED(hr)) return false;

    return create_render_target();
}

static void cleanup_device() {
    if (g_rtv) { g_rtv->Release(); g_rtv = nullptr; }
    if (g_swap_chain) { g_swap_chain->Release(); g_swap_chain = nullptr; }
    if (g_d3d_context) { g_d3d_context->Release(); g_d3d_context = nullptr; }
    if (g_d3d_device) { g_d3d_device->Release(); g_d3d_device = nullptr; }
}

static void resize_swapchain() {
    RECT rc;
    GetClientRect(g_imgui_hwnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) return;
    if (g_rtv) { g_rtv->Release(); g_rtv = nullptr; }
    g_swap_chain->ResizeBuffers(0, (UINT)w, (UINT)h, DXGI_FORMAT_UNKNOWN, 0);
    create_render_target();
}

static void render_frame() {
    if (!g_visible || !g_initialized || !g_rtv) return;
    if (g_rendering) return;
    g_rendering = true;

    MSG msg;
    while (PeekMessageW(&msg, g_imgui_hwnd, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGuiIO& io = ImGui::GetIO();
    RECT crc;
    if (GetClientRect(g_imgui_hwnd, &crc)) {
        io.DisplaySize.x = (float)(crc.right - crc.left);
        io.DisplaySize.y = (float)(crc.bottom - crc.top);
    }
    if (io.DisplaySize.x <= 0) io.DisplaySize.x = 900;
    if (io.DisplaySize.y <= 0) io.DisplaySize.y = 650;

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::Begin("AutoZWJ_Main", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar);

    render_header_panel();
    ImGui::Separator();

    float avail_h = ImGui::GetContentRegionAvail().y - 50;
    float left_w = ImGui::GetContentRegionAvail().x * 0.55f;

    ImGui::Columns(2, "main_columns", false);
    ImGui::SetColumnWidth(0, left_w);

    ImGui::BeginChild("ConfigLeft", ImVec2(0, avail_h), true);
    render_track_tree();
    ImGui::EndChild();

    ImGui::NextColumn();

    ImGui::BeginChild("ConfigRight", ImVec2(0, avail_h), true);
    render_config_panel();
    ImGui::EndChild();

    ImGui::Columns(1);

    render_action_bar();

    ImGui::End();
    ImGui::Render();

    float clear_color[4] = { 0.15f, 0.15f, 0.15f, 1.0f };
    g_d3d_context->OMSetRenderTargets(1, &g_rtv, nullptr);
    g_d3d_context->ClearRenderTargetView(g_rtv, clear_color);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    g_swap_chain->Present(1, 0);

    g_rendering = false;
}

LRESULT CALLBACK imgui_wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (g_initialized) {
        ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp);
    }

    switch (msg) {
    case WM_SIZE:
        if (g_swap_chain && g_d3d_device && wp != SIZE_MINIMIZED) {
            resize_swapchain();
        }
        return 0;

    case WM_TIMER:
        if (wp == TIMER_RENDER) {
            render_frame();
            return 0;
        }
        break;

    case WM_PAINT:
        render_frame();
        ValidateRect(hwnd, nullptr);
        return 0;

    case WM_CLOSE:
        imgui_window_hide();
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, TIMER_RENDER);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

bool imgui_window_init(HINSTANCE hinst, HWND host_window) {
    if (g_initialized) return true;

    g_wc.cbSize = sizeof(WNDCLASSEXW);
    g_wc.lpszClassName = L"AutoZWJ_Window";
    g_wc.lpfnWndProc = imgui_wnd_proc;
    g_wc.hInstance = hinst;
    g_wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    g_wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    if (!RegisterClassExW(&g_wc)) return false;

    g_imgui_hwnd = CreateWindowExW(
        0, L"AutoZWJ_Window", L"AutoZWJ - 配置导入参数",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 900, 650,
        host_window, nullptr, hinst, nullptr);
    if (!g_imgui_hwnd) return false;

    ShowWindow(g_imgui_hwnd, SW_SHOW);
    UpdateWindow(g_imgui_hwnd);

    if (!create_device_and_swapchain()) {
        DestroyWindow(g_imgui_hwnd);
        g_imgui_hwnd = nullptr;
        return false;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.IniFilename = nullptr;

    {
        const char* cjk_fonts[] = {
            "C:\\Windows\\Fonts\\msyh.ttc",
            "C:\\Windows\\Fonts\\msyhbd.ttc",
            "C:\\Windows\\Fonts\\simsun.ttc",
            "C:\\Windows\\Fonts\\msgothic.ttc",
            "C:\\Windows\\Fonts\\meiryo.ttc",
            "C:\\Windows\\Fonts\\yugothic.ttf",
        };

        ImFont* font = nullptr;
        for (auto path : cjk_fonts) {
            FILE* fp = fopen(path, "rb");
            if (fp) { fclose(fp);
                static const ImWchar ranges[] = {
                    0x0020, 0x00FF,
                    0x2000, 0x206F,
                    0x3000, 0x30FF,
                    0x31F0, 0x31FF,
                    0xFF00, 0xFFEF,
                    0x4E00, 0x9FFF,
                    0,
                };
                font = io.Fonts->AddFontFromFileTTF(path, 16.0f, nullptr, ranges);
                if (font) {
                    io.Fonts->Build();
                    break;
                }
            }
        }

        if (!font) {
            io.Fonts->AddFontDefault();
            io.Fonts->Build();
        }
    }

    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(g_imgui_hwnd);
    ImGui_ImplDX11_Init(g_d3d_device, g_d3d_context);

    SetTimer(g_imgui_hwnd, TIMER_RENDER, 16, nullptr);

    ShowWindow(g_imgui_hwnd, SW_HIDE);

    g_initialized = true;
    return true;
}

void imgui_window_show() {
    if (!g_initialized) return;
    if (!g_scene_info.valid) sync_scene_info();
    ShowWindow(g_imgui_hwnd, SW_SHOW);
    SetForegroundWindow(g_imgui_hwnd);
    g_visible = true;
    InvalidateRect(g_imgui_hwnd, nullptr, FALSE);
    render_frame();
}

void imgui_window_hide() {
    g_visible = false;
    if (g_imgui_hwnd) ShowWindow(g_imgui_hwnd, SW_HIDE);
}

bool imgui_window_is_visible() { return g_visible; }

void imgui_window_set_generate_callback(void (*cb)()) { g_on_generate = cb; }

void imgui_window_trigger_generate() {
    if (g_on_generate) g_on_generate();
}

void imgui_window_shutdown() {
    g_visible = false;
    if (g_imgui_hwnd) {
        KillTimer(g_imgui_hwnd, TIMER_RENDER);
    }
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    cleanup_device();
    if (g_imgui_hwnd) { DestroyWindow(g_imgui_hwnd); g_imgui_hwnd = nullptr; }
    UnregisterClassW(L"AutoZWJ_Window", g_wc.hInstance);
    g_initialized = false;
}

void imgui_window_pump_messages() {
    if (!g_visible || !g_initialized) return;
    MSG msg;
    while (PeekMessageW(&msg, g_imgui_hwnd, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}
