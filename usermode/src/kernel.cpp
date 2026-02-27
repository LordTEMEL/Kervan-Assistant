#include "kernel.h"

volatile KervanSharedData* g_shm = nullptr;
HANDLE                     g_requestEvent = nullptr;
HANDLE                     g_responseEvent = nullptr;

bool InitSharedMemory()
{
    // shm'i oluştur ve fiziksel bellekte kilitle
    // VirtualLock: driver MmProbeAndLockPages yaparken sayfa swapped-out olmaz
    g_shm = (volatile KervanSharedData*)VirtualAlloc(
        nullptr, sizeof(KervanSharedData),
        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!g_shm) return false;

    if (!VirtualLock((LPVOID)g_shm, sizeof(KervanSharedData))) {
        VirtualFree((LPVOID)g_shm, 0, MEM_RELEASE);
        g_shm = nullptr;
        return false;
    }

    // requestEvent:  kervan.exe → driver  ("komut hazır")
    // responseEvent: driver → kervan.exe  ("cevap hazır")
    // Auto-reset: her Wait() sonrası otomatik sıfırlanır, manuel ResetEvent gerekmez
    g_requestEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!g_requestEvent) {
        VirtualUnlock((LPVOID)g_shm, sizeof(KervanSharedData));
        VirtualFree((LPVOID)g_shm, 0, MEM_RELEASE);
        g_shm = nullptr;
        return false;
    }

    g_responseEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!g_responseEvent) {
        CloseHandle(g_requestEvent);
        g_requestEvent = nullptr;
        VirtualUnlock((LPVOID)g_shm, sizeof(KervanSharedData));
        VirtualFree((LPVOID)g_shm, 0, MEM_RELEASE);
        g_shm = nullptr;
        return false;
    }

    // Handle değerlerini shm'e yaz — driver bunları KEVENT'e çevirecek
    g_shm->signature = MAGIC_SIGNATURE;
    g_shm->commandCode = CMD_IDLE;
    g_shm->status = 0;
    g_shm->requestEventHandle = (uint64_t)(uintptr_t)g_requestEvent;
    g_shm->responseEventHandle = (uint64_t)(uintptr_t)g_responseEvent;

    return true;
}

void FreeSharedMemory()
{
    if (g_requestEvent) { CloseHandle(g_requestEvent);  g_requestEvent = nullptr; }
    if (g_responseEvent) { CloseHandle(g_responseEvent); g_responseEvent = nullptr; }
    if (g_shm) {
        VirtualUnlock((LPVOID)g_shm, sizeof(KervanSharedData));
        VirtualFree((LPVOID)g_shm, 0, MEM_RELEASE);
        g_shm = nullptr;
    }
}

bool WaitForDriver(DWORD timeoutMs)
{
    // Driver hazır olunca responseEvent'i set eder ve status=1 yazar
    DWORD result = WaitForSingleObject(g_responseEvent, timeoutMs);
    return result == WAIT_OBJECT_0 && g_shm->status == 1;
}

bool KervanRead(DWORD pid, uintptr_t address, void* out, size_t size)
{
    if (!out || size == 0 || size > KERVAN_MAX_READ)  return false;
    if (address < 0x1000 || address > 0x7FFFFFFFFFFF) return false;

    // Komutu yaz
    g_shm->targetPid = pid;
    g_shm->targetAddress = address;
    g_shm->size = size;
    g_shm->status = 0;
    g_shm->commandCode = CMD_READ_MEMORY;

    // Driver'ı uyandır
    SetEvent(g_requestEvent);

    // Driver'ın cevabını bekle — spinlock yok, CPU waste yok
    DWORD result = WaitForSingleObject(g_responseEvent, 200);
    if (result != WAIT_OBJECT_0) return false;
    if (g_shm->status != 1)      return false;

    memcpy(out, (const void*)g_shm->data, size);
    return true;
}