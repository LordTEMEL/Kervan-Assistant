#pragma once
#include <windows.h>
#include <vector>
#include <cstdint>
#include "OffsetManager.h"
#include "ImGui/imgui.h"

struct Vector3 { float x, y, z; };
struct ViewMatrix { float matrix[4][4]; };
struct EspBox { float x, y, w, h; int health; };

extern bool esp;
extern bool triggerbot;
extern int  triggerDelay;
extern int  shotInterval;
extern bool aimAssist;
extern float aimAssistSpeed;
extern bool deathmatchMode;
extern bool masterEnabled;

extern std::vector<EspBox> espBoxes;

void RunGameLogic(DWORD pid, uintptr_t clientBase, const GameOffsets& offsets);
void DrawEsp();
void DrawMenu(bool& showMenu, bool& isRunning);
bool IsGameRunning(DWORD pid);

DWORD     GetProcessIdByName(const wchar_t* name);
uintptr_t GetModuleBaseAddress(DWORD pid, const wchar_t* mod);