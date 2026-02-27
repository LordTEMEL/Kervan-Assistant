#include "game.h"
#include "kernel.h"
#include "overlay.h"
#include <TlHelp32.h>
#include <cmath>
#include <ctime>

bool  esp = false;
bool  triggerbot = true;
int   triggerDelay = 10;
int   shotInterval = 130;
bool  aimAssist = false;
float aimAssistSpeed = 0.08f;
bool  deathmatchMode = false;
bool  masterEnabled = true;

std::vector<EspBox> espBoxes;

// ─── Yardımcılar ─────────────────────────────────────────────────────────────

DWORD GetProcessIdByName(const wchar_t* name)
{
    DWORD  pid = 0;
    HANDLE h = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (h == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe; pe.dwSize = sizeof(pe);
    if (Process32FirstW(h, &pe))
        do { if (!wcscmp(pe.szExeFile, name)) { pid = pe.th32ProcessID; break; } } while (Process32NextW(h, &pe));
    CloseHandle(h);
    return pid;
}

uintptr_t GetModuleBaseAddress(DWORD pid, const wchar_t* mod)
{
    uintptr_t base = 0;
    HANDLE    h = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (h == INVALID_HANDLE_VALUE) return 0;
    MODULEENTRY32W me; me.dwSize = sizeof(me);
    if (Module32FirstW(h, &me))
        do { if (!wcscmp(me.szModule, mod)) { base = (uintptr_t)me.modBaseAddr; break; } } while (Module32NextW(h, &me));
    CloseHandle(h);
    return base;
}

static bool WorldToScreen(const Vector3& pos, Vector3& screen,
    const ViewMatrix& m, int w, int h)
{
    float cw = m.matrix[3][0] * pos.x + m.matrix[3][1] * pos.y
        + m.matrix[3][2] * pos.z + m.matrix[3][3];
    if (cw < 0.01f) return false;

    float cx = m.matrix[0][0] * pos.x + m.matrix[0][1] * pos.y
        + m.matrix[0][2] * pos.z + m.matrix[0][3];
    float cy = m.matrix[1][0] * pos.x + m.matrix[1][1] * pos.y
        + m.matrix[1][2] * pos.z + m.matrix[1][3];

    float inv = 1.f / cw;
    screen.x = (w * 0.5f) + (0.5f * cx * inv * w + 0.5f);
    screen.y = (h * 0.5f) - (0.5f * cy * inv * h + 0.5f);
    return true;
}

static void ClickMouse()
{
    INPUT i[2]{};
    i[0].type = INPUT_MOUSE; i[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    i[1].type = INPUT_MOUSE; i[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
    SendInput(2, i, sizeof(INPUT));
}

static void ApplyAimAssist(float targetSX, float targetSY, float speed)
{
    float dx = (targetSX - screenWidth / 2.f) * speed;
    float dy = (targetSY - screenHeight / 2.f) * speed;
    if (fabsf(dx) < 0.5f && fabsf(dy) < 0.5f) return;

    INPUT inp{};
    inp.type = INPUT_MOUSE;
    inp.mi.dwFlags = MOUSEEVENTF_MOVE;
    inp.mi.dx = (LONG)roundf(dx);
    inp.mi.dy = (LONG)roundf(dy);
    SendInput(1, &inp, sizeof(INPUT));
}

static inline uintptr_t ReadChunkPtr(DWORD pid, uintptr_t entList, int i)
{
    return ReadMemory<uintptr_t>(pid, entList + 8 * (i >> 9) + 16);
}

static inline uintptr_t ReadEntityPtr(DWORD pid, uintptr_t chunkPtr, int i)
{
    return ReadMemory<uintptr_t>(pid, chunkPtr + 112 * (i & 0x1FF));
}

// ─── Ana Oyun Mantığı ─────────────────────────────────────────────────────────

static ULONGLONG lastShot = 0;

void RunGameLogic(DWORD pid, uintptr_t clientBase, const GameOffsets& offsets)
{
    
    uintptr_t localPawn = ReadMemory<uintptr_t>(pid, clientBase + offsets.dwLocalPlayerPawn);
    if (!localPawn) return;

    int       myTeam = ReadMemory<int>(pid, localPawn + offsets.m_iTeamNum);
    uintptr_t entList = ReadMemory<uintptr_t>(pid, clientBase + offsets.dwEntityList);
    if (!entList) return;

    ViewMatrix vMat{};
    if (esp || aimAssist)
        vMat = ReadMemory<ViewMatrix>(pid, clientBase + offsets.dwViewMatrix);

    if (esp) espBoxes.clear();

    float bestDist = 9999.f, bestSX = 0.f, bestSY = 0.f;
    bool  hasTarget = false;

    for (int i = 1; i < 64; i++)
    {
        uintptr_t cp = ReadChunkPtr(pid, entList, i);
        if (!cp) continue;

        uintptr_t controller = ReadEntityPtr(pid, cp, i);
        if (!controller) continue;

        uint32_t pawnHandle = ReadMemory<uint32_t>(pid, controller + offsets.m_hPlayerPawn);
        if (!pawnHandle) continue;

        uintptr_t cp2 = ReadChunkPtr(pid, entList, (int)(pawnHandle & 0x7FFF));
        if (!cp2) continue;

        uintptr_t targetPawn = ReadEntityPtr(pid, cp2, (int)(pawnHandle & 0x1FF));
        if (!targetPawn || targetPawn == localPawn) continue;

        int health = ReadMemory<int>(pid, targetPawn + offsets.m_iHealth);
        if (health <= 0 || health > 100) continue;

        int  team = ReadMemory<int>(pid, targetPawn + offsets.m_iTeamNum);
        bool enemy = deathmatchMode ? true : (team != myTeam && team > 1);
        if (!enemy) continue;

        if (esp || aimAssist)
        {
            uintptr_t scene = ReadMemory<uintptr_t>(pid, targetPawn + offsets.m_pGameSceneNode);
            if (!scene) continue;

            Vector3 feet = ReadMemory<Vector3>(pid, scene + 0xD0);
            if (feet.x == 0.f && feet.y == 0.f && feet.z == 0.f) continue;

            Vector3 head = { feet.x, feet.y, feet.z + 75.f };
            Vector3 sFeet{}, sHead{};
            if (!WorldToScreen(feet, sFeet, vMat, screenWidth, screenHeight) ||
                !WorldToScreen(head, sHead, vMat, screenWidth, screenHeight))
                continue;

            if (esp)
            {
                float bh = sFeet.y - sHead.y;
                float bw = bh / 2.f;
                if (bh > 0.f && bh < (float)screenHeight)
                    espBoxes.push_back({ sHead.x - bw / 2.f, sHead.y, bw, bh, health });
            }

            if (aimAssist)
            {
                float cx = screenWidth / 2.f;
                float cy = screenHeight / 2.f;
                float midX = sHead.x;
                float midY = sHead.y + (sFeet.y - sHead.y) * 0.2f;
                float dist = sqrtf((midX - cx) * (midX - cx) + (midY - cy) * (midY - cy));
                if (dist < bestDist) {
                    bestDist = dist;
                    bestSX = midX;
                    bestSY = midY;
                    hasTarget = true;
                }
            }
        }
    }

    if (aimAssist && hasTarget && (GetAsyncKeyState(VK_LBUTTON) & 0x8000))
        ApplyAimAssist(bestSX, bestSY, aimAssistSpeed);

    if (triggerbot)
    {
        int xId = ReadMemory<int>(pid, localPawn + offsets.m_iIDEntIndex);
        if (xId > 0 && xId < 2048)
        {
            uintptr_t tcp = ReadChunkPtr(pid, entList, xId);
            uintptr_t tPawn = tcp ? ReadEntityPtr(pid, tcp, xId) : 0;
            if (tPawn)
            {
                int tTeam = ReadMemory<int>(pid, tPawn + offsets.m_iTeamNum);
                int tHealth = ReadMemory<int>(pid, tPawn + offsets.m_iHealth);
                if ((deathmatchMode || tTeam != myTeam) && tHealth > 0)
                {
                    ULONGLONG now = GetTickCount64();
                    int jitter = (rand() % (shotInterval / 3 + 1)) - shotInterval / 6;
                    ULONGLONG thresh = (ULONGLONG)(shotInterval + jitter);
                    if (now - lastShot > thresh)
                    {
                        int delay = triggerDelay + rand() % (triggerDelay / 2 + 5);
                        if (delay > 0) Sleep(delay);
                        ClickMouse();
                        lastShot = GetTickCount64();
                    }
                }
            }
        }
    }
}

// ─── Çizim ───────────────────────────────────────────────────────────────────

static ImU32 HealthColor(int hp)
{
    float t = hp / 100.f;
    return IM_COL32((int)((1.f - t) * 255.f), (int)(t * 255.f), 0, 255);
}

static void DrawCornerBox(int x, int y, int w, int h, int px, ImU32 color)
{
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    int lw = w / 4, lh = h / 4;
    ImU32 outl = IM_COL32(0, 0, 0, 255);

    auto ln = [&](int x1, int y1, int x2, int y2, ImU32 c, int t) {
        dl->AddLine({ (float)x1,(float)y1 }, { (float)x2,(float)y2 }, c, (float)t);
        };

    for (int i = 0; i < 2; i++) {
        ImU32 c = (i == 0) ? outl : color;
        int   t = (i == 0) ? px + 2 : px;
        ln(x, y, x + lw, y, c, t);
        ln(x, y, x, y + lh, c, t);
        ln(x + w, y, x + w - lw, y, c, t);
        ln(x + w, y, x + w, y + lh, c, t);
        ln(x, y + h, x + lw, y + h, c, t);
        ln(x, y + h, x, y + h - lh, c, t);
        ln(x + w, y + h, x + w - lw, y + h, c, t);
        ln(x + w, y + h, x + w, y + h - lh, c, t);
    }
}

void DrawEsp()
{
    for (const auto& box : espBoxes)
        DrawCornerBox((int)box.x, (int)box.y,
            (int)box.w, (int)box.h, 2, HealthColor(box.health));
}

void DrawMenu(bool& showMenu, bool& isRunning)
{
    ImGui::SetNextWindowSize(ImVec2(290, 280), ImGuiCond_Once);
    ImGui::Begin("Kervan Pro", &showMenu,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar);

    if (masterEnabled)
        ImGui::TextColored(ImVec4(0.f, 1.f, 0.f, 1.f), "● AKTIF  [NumPad5]");
    else
        ImGui::TextColored(ImVec4(1.f, 0.f, 0.f, 1.f), "● PASIF  [NumPad5]");
    ImGui::Separator();

    ImGui::Checkbox("ESP (Wallhack)", &esp);
    ImGui::Separator();
    ImGui::Checkbox("Triggerbot", &triggerbot);
    ImGui::SliderInt("Gecikme (ms)", &triggerDelay, 0, 100);
    ImGui::SliderInt("Interval (ms)", &shotInterval, 10, 500);
    ImGui::Separator();
    ImGui::Checkbox("Aim Assist", &aimAssist);
    if (aimAssist)
        ImGui::SliderFloat("Hiz", &aimAssistSpeed, 0.01f, 0.3f);
    ImGui::Separator();
    ImGui::Checkbox("Deathmatch Modu", &deathmatchMode);
    ImGui::Separator();
    if (ImGui::Button("Kapat", ImVec2(-1, 0))) isRunning = false;

    ImGui::End();
}
bool IsGameRunning(DWORD pid) {
    if (!pid) return false;
    HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!h) return false;
    DWORD code = 0;
    GetExitCodeProcess(h, &code);
    CloseHandle(h);
    return code == STILL_ACTIVE;
}