# Kervan Assistant

Windows kernel driver tabanlı CS2 bellek okuma projesi. Kernel-user iletişimi için KEVENT senkronizasyonu kullanır.

---

## Proje Yapısı

```
Kervan_Assistant/
├── kernelmode/
│   ├── src/
│   │   └── driver.c          # Kernel driver (WDK)
│   └── KervanDriver/
│       └── KervanDriver.vcxproj
├── usermode/
│   ├── src/
│   │   ├── main.cpp
│   │   ├── kernel.cpp        # Kernel iletişim
│   │   ├── overlay.cpp       # DirectX overlay
│   │   ├── game.cpp          # Oyun mantığı
│   │   └── ImGui/
│   ├── include/
│   │   ├── kernel.h
│   │   ├── overlay.h
│   │   ├── game.h
│   │   ├── OffsetManager.h
│   │   └── NetworkManager.h
│   └── CMakeLists.txt
├── shared/
│   └── communication.h       # Ortak struct tanımları
├── thirdparty/
│   └── json/
│       └── json.hpp
└── CMakeLists.txt
```

---

## Gereksinimler

### Driver (Kernel)
- Windows 11/10 x64
- [Windows Driver Kit (WDK)](https://learn.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk)
- Visual Studio 2022
- Test imzalama veya [KDU](https://github.com/hfiref0x/KDU) ile driver mapping

### Usermode
- Visual Studio 2022
- CMake 3.15+
- DirectX 11 SDK (Windows SDK ile gelir)

---

## Derleme

### 1. Driver

Visual Studio'da `kernelmode/KervanDriver/KervanDriver.vcxproj` dosyasını aç.

```
Platform: x64
Configuration: Release
Build → Build Solution
```

Çıktı: `kernelmode/KervanDriver/x64/Release/KervanDriver.sys`

### 2. Usermode

```bash
# Visual Studio'da
# File → Open → CMake
# Kervan_Assistant klasörünü seç
# Build → Build All
```

Çıktı: `out/build/x64-Release/usermode/kervan.exe`

---

## Kullanım

1. Driver'ı KDU ile map et:
```
kdu.exe -map KervanDriver.sys
```

2. CS2'yi başlat

3. `kervan.exe`'yi çalıştır

4. Tuş atamaları:
   - `Insert` — Menüyü aç/kapat
   - `NumPad5` — Master switch (aktif/pasif)
   - `End` — Kapat

---

## Mimari

```
kervan.exe                     KervanDriver.sys
─────────────                  ────────────────
InitSharedMemory()             DriverEntry()
  VirtualAlloc(shm)              PsCreateSystemThread()
  VirtualLock(shm)                 └─ KervanThread()
  CreateEvent(request)               KervanFindProcess()
  CreateEvent(response)              KervanFindSignatureAddress()
  shm->signature = MAGIC             KervanMapUserMemory() [MDL]
  shm->requestHandle = h1            ObReferenceObjectByHandle()
  shm->responseHandle = h2             └─ requestEvent (KEVENT)
                                       └─ responseEvent (KEVENT)
WaitForDriver()                    shm->status = 1
  WaitForSingleObject(response) ←  KeSetEvent(response)

KervanRead()                       KeWaitForSingleObject(request)
  shm->commandCode = READ    →     KervanReadMemory()
  SetEvent(request)                  MmCopyVirtualMemory()
  WaitForSingleObject(response) ←  KeSetEvent(response)
  memcpy(out, shm->data)
```

---

## Özellikler

- **ESP** — Corner box, sağlık rengi
- **Triggerbot** — Gecikme ve interval ayarı
- **Aim Assist** — Hız ayarlı smooth aim
- **Deathmatch Modu** — Takım kontrolü bypass
- **Otomatik Offset** — [cs2-dumper](https://github.com/a2x/cs2-dumper)'dan güncel offset indirme

---

- Test imzalama için Secure Boot kapalı olmalıdır