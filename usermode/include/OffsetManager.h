#pragma once
#include <iostream>
#include <fstream>
#include <string>
#include <json.hpp>
using json = nlohmann::json;

// ============================================================
//  Offset yapısı
// ============================================================
struct GameOffsets {
    uintptr_t dwLocalPlayerPawn = 0;
    uintptr_t dwEntityList = 0;
    uintptr_t dwViewMatrix = 0;
    uintptr_t m_iHealth = 0;
    uintptr_t m_iTeamNum = 0;
    uintptr_t m_iIDEntIndex = 0;
    uintptr_t m_hPlayerPawn = 0;
    uintptr_t m_pGameSceneNode = 0;
    uintptr_t m_vOldOrigin = 0;
    uintptr_t m_aimPunchAngle = 0;  // recoil punch açısı (RCS için)
};

// ============================================================
//  Yardımcı: json path'ten güvenli değer oku
// ============================================================
static uintptr_t SafeGet(const json& j, const std::string& key)
{
    if (j.contains(key) && !j[key].is_null())
        return j[key].get<uintptr_t>();
    return 0;
}

// ============================================================
//  OffsetManager
// ============================================================
class OffsetManager {
private:
    GameOffsets offsets;

public:
    bool LoadOffsets(const std::string& filePath)
    {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            std::cout << "[HATA] Dosya acilamadi: " << filePath << "\n";
            return false;
        }

        try {
            json j;
            file >> j;

            // ------------------------------------------------
            //  offsets.json:
            //  { "client.dll": { "dwEntityList": 12345, ... } }
            //  Flat key-value yapisi, "classes" YOK.
            // ------------------------------------------------
            if (j.contains("client.dll") &&
                !j["client.dll"].contains("classes"))
            {
                auto& dll = j["client.dll"];

                auto load = [&](uintptr_t& dst, const char* key) {
                    uintptr_t v = SafeGet(dll, key);
                    if (v) {
                        dst = v;
                        std::cout << "[OFFSET] " << key << ": 0x"
                            << std::hex << v << std::dec << "\n";
                    }
                    };

                load(offsets.dwLocalPlayerPawn, "dwLocalPlayerPawn");
                load(offsets.dwEntityList, "dwEntityList");
                load(offsets.dwViewMatrix, "dwViewMatrix");
            }

            // ------------------------------------------------
            //  client_dll.json:
            //  { "client.dll": { "classes": { "ClassName": {
            //      "fields": { "m_iHealth": 123 } } } } }
            // ------------------------------------------------
            if (j.contains("client.dll") &&
                j["client.dll"].contains("classes"))
            {
                auto& classes = j["client.dll"]["classes"];

                // Belirli bir class'tan field yükle
                auto loadField = [&](uintptr_t& dst,
                    const char* cls,
                    const char* field)
                    {
                        if (!classes.contains(cls)) return;
                        auto& f = classes[cls]["fields"];
                        uintptr_t v = SafeGet(f, field);
                        if (v) {
                            dst = v;
                            std::cout << "[OFFSET] " << field << " (" << cls
                                << "): 0x" << std::hex << v << std::dec << "\n";
                        }
                    };

                // m_iHealth — C_BaseEntity
                loadField(offsets.m_iHealth, "C_BaseEntity", "m_iHealth");

                // m_iTeamNum — C_BaseEntity
                loadField(offsets.m_iTeamNum, "C_BaseEntity", "m_iTeamNum");

                // m_hPlayerPawn — CCSPlayerController
                loadField(offsets.m_hPlayerPawn, "CCSPlayerController", "m_hPlayerPawn");

                // m_pGameSceneNode — C_BaseEntity
                loadField(offsets.m_pGameSceneNode, "C_BaseEntity", "m_pGameSceneNode");

                // m_vOldOrigin — C_BasePlayerPawn
                loadField(offsets.m_vOldOrigin, "C_BasePlayerPawn", "m_vOldOrigin");

                // m_aimPunchAngle — C_CSPlayerPawn
                loadField(offsets.m_aimPunchAngle, "C_CSPlayerPawn", "m_aimPunchAngle");
                if (offsets.m_aimPunchAngle == 0)
                    loadField(offsets.m_aimPunchAngle, "C_BasePlayerPawn", "m_aimPunchAngle");

                // m_iIDEntIndex — C_CSPlayerPawn
                loadField(offsets.m_iIDEntIndex, "C_CSPlayerPawn", "m_iIDEntIndex");

                // Bulunamadıysa C_CSPlayerPawnBase'e bak
                if (offsets.m_iIDEntIndex == 0)
                    loadField(offsets.m_iIDEntIndex, "C_CSPlayerPawnBase", "m_iIDEntIndex");
            }

            return true;
        }
        catch (std::exception& e) {
            std::cout << "[HATA] JSON isleme hatasi: " << e.what() << "\n";
            return false;
        }
    }

    // Yükleme sonrası eksik offset raporu
    void PrintStatus() const
    {
        std::cout << "\n========== OFFSET DURUMU ==========\n";
        auto chk = [](const char* name, uintptr_t v) {
            std::cout << (v ? "[OK] " : "[!!] ") << name << ": 0x"
                << std::hex << v << std::dec << "\n";
            };
        chk("dwLocalPlayerPawn", offsets.dwLocalPlayerPawn);
        chk("dwEntityList", offsets.dwEntityList);
        chk("dwViewMatrix", offsets.dwViewMatrix);
        chk("m_iHealth", offsets.m_iHealth);
        chk("m_iTeamNum", offsets.m_iTeamNum);
        chk("m_iIDEntIndex", offsets.m_iIDEntIndex);
        chk("m_hPlayerPawn", offsets.m_hPlayerPawn);
        chk("m_pGameSceneNode", offsets.m_pGameSceneNode);
        chk("m_vOldOrigin", offsets.m_vOldOrigin);
        std::cout << "====================================\n\n";
    }

    const GameOffsets& Get() const { return offsets; }
};