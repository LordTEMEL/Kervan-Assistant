// usermode/include/NetworkManager.h
#pragma once
#include <windows.h>
#include <string>
#include <urlmon.h>
#include <iostream>

#pragma comment(lib, "urlmon.lib")

class NetworkManager {
public:
    static bool DownloadOffsets(const std::string& url, const std::string& destination) {
        std::cout << "[NETWORK] Guncel offsetler indiriliyor...\n";

        // URLDownloadToFile: Windows'un basit ve etkili indirme fonksiyonu
        HRESULT hr = URLDownloadToFileA(NULL, url.c_str(), destination.c_str(), 0, NULL);

        if (SUCCEEDED(hr)) {
            std::cout << "[BAŞARILI] Offsetler GitHub'dan cekildi.\n";
            return true;
        }
        else {
            std::cout << "[HATA] Indirme basarisiz! Hata Kodu: " << hr << "\n";
            return false;
        }
    }
};