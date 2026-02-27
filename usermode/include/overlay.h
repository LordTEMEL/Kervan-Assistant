#pragma once
#include <windows.h>
#include <d3d11.h>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "d3d11.lib")

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_win32.h"
#include "ImGui/imgui_impl_dx11.h"

extern HWND                    overlayHWND;
extern ID3D11Device* d3dDevice;
extern ID3D11DeviceContext* d3dDeviceContext;
extern IDXGISwapChain* swapChain;
extern ID3D11RenderTargetView* renderTargetView;
extern int  screenWidth;
extern int  screenHeight;

bool SetupOverlay();
bool InitDirectX();
void ShutdownOverlay();
void BeginFrame();
void EndFrame();