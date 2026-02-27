#include "overlay.h"

HWND                    overlayHWND = nullptr;
ID3D11Device* d3dDevice = nullptr;
ID3D11DeviceContext* d3dDeviceContext = nullptr;
IDXGISwapChain* swapChain = nullptr;
ID3D11RenderTargetView* renderTargetView = nullptr;
int screenWidth = 0;
int screenHeight = 0;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

static LRESULT CALLBACK WindowProc(HWND hWnd, UINT msg, WPARAM wP, LPARAM lP)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wP, lP)) return true;
    if (msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProc(hWnd, msg, wP, lP);
}

bool SetupOverlay()
{
    WNDCLASSEX wc{}; wc.cbSize = sizeof(wc); wc.style = CS_CLASSDC;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = "KervanOverlay";
    RegisterClassEx(&wc);

    screenWidth = GetSystemMetrics(SM_CXSCREEN);
    screenHeight = GetSystemMetrics(SM_CYSCREEN);

    overlayHWND = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT,
        wc.lpszClassName, "Kervan", WS_POPUP,
        0, 0, screenWidth, screenHeight,
        nullptr, nullptr, wc.hInstance, nullptr);

    if (!overlayHWND) return false;

    SetLayeredWindowAttributes(overlayHWND, 0, 255, LWA_ALPHA);
    MARGINS m = { -1,-1,-1,-1 };
    DwmExtendFrameIntoClientArea(overlayHWND, &m);
    ShowWindow(overlayHWND, SW_HIDE);
    return true;
}

bool InitDirectX()
{
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = overlayHWND;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL fl;
    if (FAILED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE,
        nullptr, 0, nullptr, 0, D3D11_SDK_VERSION,
        &sd, &swapChain, &d3dDevice, &fl, &d3dDeviceContext)))
        return false;

    ID3D11Texture2D* bb = nullptr;
    swapChain->GetBuffer(0, IID_PPV_ARGS(&bb));
    if (!bb) return false;
    d3dDevice->CreateRenderTargetView(bb, nullptr, &renderTargetView);
    bb->Release();

    D3D11_BLEND_DESC bd{};
    bd.RenderTarget[0].BlendEnable = TRUE;
    bd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    bd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    ID3D11BlendState* bs = nullptr;
    d3dDevice->CreateBlendState(&bd, &bs);
    float bf[4] = {};
    d3dDeviceContext->OMSetBlendState(bs, bf, 0xffffffff);
    if (bs) bs->Release();

    return true;
}

void ShutdownOverlay()
{
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    if (renderTargetView) renderTargetView->Release();
    if (swapChain)        swapChain->Release();
    if (d3dDeviceContext) d3dDeviceContext->Release();
    if (d3dDevice)        d3dDevice->Release();
    if (overlayHWND)      DestroyWindow(overlayHWND);
}

void BeginFrame()
{
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void EndFrame()
{
    ImGui::Render();
    float clear[4] = { 0.f, 0.f, 0.f, 0.f };
    d3dDeviceContext->OMSetRenderTargets(1, &renderTargetView, nullptr);
    d3dDeviceContext->ClearRenderTargetView(renderTargetView, clear);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    swapChain->Present(0, 0);
}