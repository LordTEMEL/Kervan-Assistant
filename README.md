# Kervan Assistant

<div align="center">

**[🇹🇷 Türkçe](#-türkçe) • [🇬🇧 English](#-english) • [🇷🇺 Русский](#-русский)**

</div>

---

## 🇹🇷 Türkçe

Windows kernel driver tabanlı CS2 bellek okuma projesi. Kernel-user iletişimi için KEVENT senkronizasyonu kullanır.

### Proje Yapısı

```
Kervan_Assistant/
├── kernelmode/
│   ├── src/
│   │   └── driver.c              # Kernel driver (WDK)
│   └── KervanDriver/
│       └── KervanDriver.vcxproj
├── usermode/
│   ├── src/
│   │   ├── main.cpp
│   │   ├── kernel.cpp            # Kernel iletişim
│   │   ├── overlay.cpp           # DirectX overlay
│   │   ├── game.cpp              # Oyun mantığı
│   │   └── ImGui/
│   ├── include/
│   │   ├── kernel.h
│   │   ├── overlay.h
│   │   ├── game.h
│   │   ├── OffsetManager.h
│   │   └── NetworkManager.h
│   └── CMakeLists.txt
├── shared/
│   └── communication.h           # Ortak struct tanımları
├── thirdparty/
│   └── json/
│       └── json.hpp
└── CMakeLists.txt
```

### Gereksinimler

**Driver**
- Windows 10/11 x64
- [Windows Driver Kit (WDK)](https://learn.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk)
- Visual Studio 2022
- [KDU](https://github.com/hfiref0x/KDU) ile driver mapping

**Usermode**
- Visual Studio 2022
- CMake 3.15+
- DirectX 11 SDK (Windows SDK ile gelir)

### Derleme

**Driver**
```
Visual Studio → kernelmode/KervanDriver/KervanDriver.vcxproj
Platform: x64 | Configuration: Release
Build → Build Solution
```
Çıktı: `kernelmode/KervanDriver/x64/Release/KervanDriver.sys`

**Usermode**
```
Visual Studio → File → Open → CMake → Kervan_Assistant klasörü
Build → Build All
```
Çıktı: `out/build/x64-Release/usermode/kervan.exe`

### Kullanım

```
1. kdu.exe -map KervanDriver.sys
2. CS2'yi başlat
3. kervan.exe'yi çalıştır
```

| Tuş | Fonksiyon |
|-----|-----------|
| `Insert` | Menüyü aç/kapat |
| `NumPad5` | Master switch |
| `End` | Kapat |

### Özellikler

- **ESP** — Corner box, sağlık rengi
- **Triggerbot** — Gecikme ve interval ayarı
- **Aim Assist** — Hız ayarlı smooth aim
- **Deathmatch Modu** — Takım kontrolü bypass
- **Otomatik Offset** — [cs2-dumper](https://github.com/a2x/cs2-dumper)'dan güncel offset indirme

### Mimari

```
kervan.exe                        KervanDriver.sys
──────────────────                ─────────────────
VirtualAlloc(shm)                 PsCreateSystemThread()
VirtualLock(shm)                    KervanFindProcess()
CreateEvent(request)                KervanFindSignatureAddress()
CreateEvent(response)               KervanMapUserMemory() [MDL]
shm->signature = MAGIC              ObReferenceObjectByHandle()
shm->requestHandle  = h1              requestEvent  (KEVENT)
shm->responseHandle = h2              responseEvent (KEVENT)
                                    shm->status = 1
WaitForDriver()              ←     KeSetEvent(response)

KervanRead()
  shm->commandCode = READ    →     KeWaitForSingleObject(request)
  SetEvent(request)                MmCopyVirtualMemory()
  WaitForSingleObject(response) ← KeSetEvent(response)
  memcpy(out, shm->data)
```


## 🇬🇧 English

A Windows kernel driver-based CS2 memory reading project. Uses KEVENT synchronization for kernel-user communication.

### Project Structure

```
Kervan_Assistant/
├── kernelmode/
│   ├── src/
│   │   └── driver.c              # Kernel driver (WDK)
│   └── KervanDriver/
│       └── KervanDriver.vcxproj
├── usermode/
│   ├── src/
│   │   ├── main.cpp
│   │   ├── kernel.cpp            # Kernel communication
│   │   ├── overlay.cpp           # DirectX overlay
│   │   ├── game.cpp              # Game logic
│   │   └── ImGui/
│   ├── include/
│   │   ├── kernel.h
│   │   ├── overlay.h
│   │   ├── game.h
│   │   ├── OffsetManager.h
│   │   └── NetworkManager.h
│   └── CMakeLists.txt
├── shared/
│   └── communication.h           # Shared struct definitions
├── thirdparty/
│   └── json/
│       └── json.hpp
└── CMakeLists.txt
```

### Requirements

**Driver**
- Windows 10/11 x64
- [Windows Driver Kit (WDK)](https://learn.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk)
- Visual Studio 2022
- [KDU](https://github.com/hfiref0x/KDU) for driver mapping

**Usermode**
- Visual Studio 2022
- CMake 3.15+
- DirectX 11 SDK (included with Windows SDK)

### Building

**Driver**
```
Visual Studio → kernelmode/KervanDriver/KervanDriver.vcxproj
Platform: x64 | Configuration: Release
Build → Build Solution
```
Output: `kernelmode/KervanDriver/x64/Release/KervanDriver.sys`

**Usermode**
```
Visual Studio → File → Open → CMake → Select Kervan_Assistant folder
Build → Build All
```
Output: `out/build/x64-Release/usermode/kervan.exe`

### Usage

```
1. kdu.exe -map KervanDriver.sys
2. Launch CS2
3. Run kervan.exe
```

| Key | Function |
|-----|----------|
| `Insert` | Toggle menu |
| `NumPad5` | Master switch |
| `End` | Exit |

### Features

- **ESP** — Corner box with health color
- **Triggerbot** — Configurable delay and interval
- **Aim Assist** — Smooth aim with speed control
- **Deathmatch Mode** — Bypass team check
- **Auto Offset** — Downloads latest offsets from [cs2-dumper](https://github.com/a2x/cs2-dumper)

### Architecture

```
kervan.exe                        KervanDriver.sys
──────────────────                ─────────────────
VirtualAlloc(shm)                 PsCreateSystemThread()
VirtualLock(shm)                    KervanFindProcess()
CreateEvent(request)                KervanFindSignatureAddress()
CreateEvent(response)               KervanMapUserMemory() [MDL]
shm->signature = MAGIC              ObReferenceObjectByHandle()
shm->requestHandle  = h1              requestEvent  (KEVENT)
shm->responseHandle = h2              responseEvent (KEVENT)
                                    shm->status = 1
WaitForDriver()              ←     KeSetEvent(response)

KervanRead()
  shm->commandCode = READ    →     KeWaitForSingleObject(request)
  SetEvent(request)                MmCopyVirtualMemory()
  WaitForSingleObject(response) ← KeSetEvent(response)
  memcpy(out, shm->data)
```

## 🇷🇺 Русский

Проект для чтения памяти CS2 на основе Windows kernel driver. Использует синхронизацию KEVENT для взаимодействия kernel-user.

### Структура проекта

```
Kervan_Assistant/
├── kernelmode/
│   ├── src/
│   │   └── driver.c              # Kernel драйвер (WDK)
│   └── KervanDriver/
│       └── KervanDriver.vcxproj
├── usermode/
│   ├── src/
│   │   ├── main.cpp
│   │   ├── kernel.cpp            # Взаимодействие с kernel
│   │   ├── overlay.cpp           # DirectX оверлей
│   │   ├── game.cpp              # Игровая логика
│   │   └── ImGui/
│   ├── include/
│   │   ├── kernel.h
│   │   ├── overlay.h
│   │   ├── game.h
│   │   ├── OffsetManager.h
│   │   └── NetworkManager.h
│   └── CMakeLists.txt
├── shared/
│   └── communication.h           # Общие определения структур
├── thirdparty/
│   └── json/
│       └── json.hpp
└── CMakeLists.txt
```

### Требования

**Драйвер**
- Windows 10/11 x64
- [Windows Driver Kit (WDK)](https://learn.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk)
- Visual Studio 2022
- [KDU](https://github.com/hfiref0x/KDU) для загрузки драйвера

**Usermode**
- Visual Studio 2022
- CMake 3.15+
- DirectX 11 SDK (входит в Windows SDK)

### Сборка

**Драйвер**
```
Visual Studio → kernelmode/KervanDriver/KervanDriver.vcxproj
Platform: x64 | Configuration: Release
Build → Build Solution
```
Результат: `kernelmode/KervanDriver/x64/Release/KervanDriver.sys`

**Usermode**
```
Visual Studio → File → Open → CMake → Выберите папку Kervan_Assistant
Build → Build All
```
Результат: `out/build/x64-Release/usermode/kervan.exe`

### Использование

```
1. kdu.exe -map KervanDriver.sys
2. Запустите CS2
3. Запустите kervan.exe
```

| Клавиша | Функция |
|---------|---------|
| `Insert` | Открыть/закрыть меню |
| `NumPad5` | Главный переключатель |
| `End` | Выход |

### Функции

- **ESP** — Угловые боксы с цветом здоровья
- **Triggerbot** — Настраиваемая задержка и интервал
- **Aim Assist** — Плавный аим с регулировкой скорости
- **Deathmatch режим** — Отключение проверки команды
- **Авто-оффсеты** — Загрузка актуальных оффсетов из [cs2-dumper](https://github.com/a2x/cs2-dumper)

### Архитектура

```
kervan.exe                        KervanDriver.sys
──────────────────                ─────────────────
VirtualAlloc(shm)                 PsCreateSystemThread()
VirtualLock(shm)                    KervanFindProcess()
CreateEvent(request)                KervanFindSignatureAddress()
CreateEvent(response)               KervanMapUserMemory() [MDL]
shm->signature = MAGIC              ObReferenceObjectByHandle()
shm->requestHandle  = h1              requestEvent  (KEVENT)
shm->responseHandle = h2              responseEvent (KEVENT)
                                    shm->status = 1
WaitForDriver()              ←     KeSetEvent(response)

KervanRead()
  shm->commandCode = READ    →     KeWaitForSingleObject(request)
  SetEvent(request)                MmCopyVirtualMemory()
  WaitForSingleObject(response) ← KeSetEvent(response)
  memcpy(out, shm->data)
```
