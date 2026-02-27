#include <windows.h>
#include <ctime>

#include "kernel.h"
#include "overlay.h"
#include "game.h"
#include "NetworkManager.h"
#include "OffsetManager.h"

int main()
{
    ShowWindow(GetConsoleWindow(), SW_HIDE);
    srand((unsigned)time(nullptr));

    if (!InitSharedMemory()) {
        MessageBoxA(nullptr, "Bellek ayirma hatasi!", "HATA", MB_OK | MB_ICONERROR);
        return -1;
    }

    NetworkManager::DownloadOffsets(
        "https://raw.githubusercontent.com/a2x/cs2-dumper/refs/heads/main/output/client_dll.json",
        "client.dll.json");
    NetworkManager::DownloadOffsets(
        "https://raw.githubusercontent.com/a2x/cs2-dumper/refs/heads/main/output/offsets.json",
        "offsets.json");

    OffsetManager offsetMgr;
    if (!offsetMgr.LoadOffsets("offsets.json") || !offsetMgr.LoadOffsets("client.dll.json")) {
        MessageBoxA(nullptr, "Offsetler yuklenemedi!", "HATA", MB_OK | MB_ICONERROR);
        FreeSharedMemory(); return 0;
    }
    const GameOffsets& offsets = offsetMgr.Get();

    if (!offsets.dwLocalPlayerPawn || !offsets.dwEntityList ||
        !offsets.dwViewMatrix || !offsets.m_hPlayerPawn ||
        !offsets.m_pGameSceneNode || !offsets.m_iHealth)
    {
        MessageBoxA(nullptr, "Kritik offsetler eksik!", "HATA", MB_OK | MB_ICONERROR);
        FreeSharedMemory(); return 0;
    }

    if (!WaitForDriver(10000)) {
        MessageBoxA(nullptr, "Driver bulunamadi! KDU ile map ettiniz mi?", "HATA", MB_OK | MB_ICONERROR);
        FreeSharedMemory(); return 0;
    }

    DWORD pid = GetProcessIdByName(L"cs2.exe");
    if (!pid) { MessageBoxA(nullptr, "cs2.exe bulunamadi!", "HATA", MB_OK | MB_ICONERROR); FreeSharedMemory(); return 0; }

    uintptr_t client = GetModuleBaseAddress(pid, L"client.dll");
    if (!client) { MessageBoxA(nullptr, "client.dll bulunamadi!", "HATA", MB_OK | MB_ICONERROR); FreeSharedMemory(); return 0; }

    if (!SetupOverlay() || !InitDirectX()) { FreeSharedMemory(); return -1; }

    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui_ImplWin32_Init(overlayHWND);
    ImGui_ImplDX11_Init(d3dDevice, d3dDeviceContext);

    bool      showMenu = true;
    bool      isRunning = true;
    MSG       msg{};
    ULONGLONG lastGameTick = 0;
    HWND      gameWnd = nullptr;

    while (isRunning)
    {
        while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg); DispatchMessage(&msg);
            if (msg.message == WM_QUIT) isRunning = false;
        }
        if (!isRunning) break;

        if (!gameWnd || !IsWindow(gameWnd))
            gameWnd = FindWindowA(nullptr, "Counter-Strike 2");
        if (pid) {
            HANDLE hGame = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
            if (!hGame) {
                isRunning = false;
            }
            else {
                DWORD exitCode = 0;
                GetExitCodeProcess(hGame, &exitCode);
                CloseHandle(hGame);
                if (exitCode != STILL_ACTIVE) isRunning = false;
            }
        }

        HWND actWnd = GetForegroundWindow();
        bool cs2Active = gameWnd && (actWnd == gameWnd || actWnd == overlayHWND);

        if (cs2Active)
        {
            RECT  rc{}; GetClientRect(gameWnd, &rc);
            POINT pt{ 0,0 }; ClientToScreen(gameWnd, &pt);
            screenWidth = rc.right;
            screenHeight = rc.bottom;

            SetWindowPos(overlayHWND, HWND_TOPMOST,
                pt.x, pt.y, screenWidth, screenHeight,
                SWP_SHOWWINDOW | SWP_NOACTIVATE);

            if (GetAsyncKeyState(VK_NUMPAD5) & 1) masterEnabled = !masterEnabled;

            if (GetAsyncKeyState(VK_INSERT) & 1)
            {
                showMenu = !showMenu;
                SetWindowLongA(overlayHWND, GWL_EXSTYLE,
                    showMenu
                    ? (WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW)
                    : (WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT));
                SetForegroundWindow(showMenu ? overlayHWND : gameWnd);
            }

            if (GetAsyncKeyState(VK_END) & 1) isRunning = false;
        }
        else if (!showMenu)
        {
            ShowWindow(overlayHWND, SW_HIDE);
        }

        ULONGLONG now = GetTickCount64();

        if (cs2Active && masterEnabled && (now - lastGameTick >= 8)) {
            lastGameTick = now;
            RunGameLogic(pid, client, offsets);
        }

        BeginFrame();

        if (esp && cs2Active && masterEnabled) DrawEsp();
        else if (!masterEnabled) espBoxes.clear();

        if (showMenu) DrawMenu(showMenu, isRunning);

        EndFrame();
    }

    g_shm->commandCode = CMD_EXIT;
    Sleep(100);

    ShutdownOverlay();
    FreeSharedMemory();
    return 0;
}