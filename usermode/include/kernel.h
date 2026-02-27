#pragma once
#include <windows.h>
#include <cstdint>
#include "communication.h"

extern volatile KervanSharedData* g_shm;
extern HANDLE g_requestEvent;
extern HANDLE g_responseEvent;

bool InitSharedMemory();
void FreeSharedMemory();
bool WaitForDriver(DWORD timeoutMs = 10000);
bool KervanRead(DWORD pid, uintptr_t address, void* out, size_t size);

template <typename T>
T ReadMemory(DWORD pid, uintptr_t address)
{
    T result{};
    if (sizeof(T) <= KERVAN_MAX_READ)
        KervanRead(pid, address, &result, sizeof(T));
    return result;
}