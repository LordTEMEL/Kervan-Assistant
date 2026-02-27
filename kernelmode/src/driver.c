#include <ntifs.h>
#include <ntdef.h>

#define KERVAN_POOL_TAG     'NVRK'
#define KERVAN_MAX_READ     4096
#define MAGIC_SIGNATURE     0x4B455256414E0001ULL
#define TARGET_PROCESS_NAME "kervan.exe"
#define PID_SCAN_MAX        65536
#define PID_SCAN_STEP       4

typedef enum _COMMAND_TYPE {
    CMD_IDLE = 0,
    CMD_READ_MEMORY = 1,
    CMD_EXIT = 2
} COMMAND_TYPE;

#pragma pack(push, 1)
typedef struct _SharedCommunicationData {
    ULONG64 signature;
    INT32   commandCode;
    INT32   status;
    ULONG64 targetPid;
    ULONG64 targetAddress;
    ULONG64 size;
    ULONG64 requestEventHandle;
    ULONG64 responseEventHandle;
    UCHAR   data[KERVAN_MAX_READ];
} SharedCommunicationData, * PSharedCommunicationData;
#pragma pack(pop)

NTKERNELAPI NTSTATUS NTAPI MmCopyVirtualMemory(
    PEPROCESS SourceProcess, PVOID SourceAddress,
    PEPROCESS TargetProcess, PVOID TargetAddress,
    SIZE_T BufferSize, KPROCESSOR_MODE PreviousMode, PSIZE_T ReturnSize
);

NTKERNELAPI NTSTATUS PsLookupProcessByProcessId(HANDLE ProcessId, PEPROCESS* Process);
NTKERNELAPI PUCHAR   PsGetProcessImageFileName(PEPROCESS Process);

extern NTSYSAPI NTSTATUS NTAPI ZwQueryVirtualMemory(
    HANDLE ProcessHandle, PVOID BaseAddress,
    MEMORY_INFORMATION_CLASS MemoryInformationClass, PVOID MemoryInformation,
    SIZE_T MemoryInformationLength, PSIZE_T ReturnLength
);

// ─── Yardımcı Fonksiyonlar ────────────────────────────────────────────────────

static VOID KervanSleep(LONG ms) {
    LARGE_INTEGER timeout;
    timeout.QuadPart = -(10000LL * ms);
    KeDelayExecutionThread(KernelMode, FALSE, &timeout);
}

static BOOLEAN KervanMatchName(PEPROCESS process, const CHAR* target) {
    PUCHAR name = PsGetProcessImageFileName(process);
    if (!name) return FALSE;
    for (INT i = 0; i < 15; i++) {
        if (name[i] != target[i]) return FALSE;
        if (name[i] == '\0')      return TRUE;
    }
    return TRUE;
}

static BOOLEAN KervanIsProcessAlive(PEPROCESS process) {
    if (!process) return FALSE;
    return (PsGetProcessExitStatus(process) == STATUS_PENDING);
}

static PEPROCESS KervanFindProcess(const CHAR* name) {
    for (ULONG pid = 4; pid < PID_SCAN_MAX; pid += PID_SCAN_STEP) {
        PEPROCESS process = NULL;
        if (!NT_SUCCESS(PsLookupProcessByProcessId((HANDLE)(ULONG_PTR)pid, &process)))
            continue;
        if (KervanMatchName(process, name) && KervanIsProcessAlive(process))
            return process;
        ObDereferenceObject(process);
    }
    return NULL;
}

// ─── MDL Mapping ─────────────────────────────────────────────────────────────

typedef struct _KervanMdlMapping {
    PMDL  mdl;
    PVOID kernelVa;
} KervanMdlMapping;

static BOOLEAN KervanMapUserMemory(PEPROCESS process, PVOID userAddr, SIZE_T size,
    LOCK_OPERATION op, KervanMdlMapping* out)
{
    out->mdl = NULL;
    out->kernelVa = NULL;

    PMDL mdl = IoAllocateMdl(userAddr, (ULONG)size, FALSE, FALSE, NULL);
    if (!mdl) return FALSE;

    KAPC_STATE apc;
    KeStackAttachProcess(process, &apc);
    __try {
        MmProbeAndLockPages(mdl, UserMode, op);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        KeUnstackDetachProcess(&apc);
        IoFreeMdl(mdl);
        return FALSE;
    }
    KeUnstackDetachProcess(&apc);

    PVOID va = MmGetSystemAddressForMdlSafe(mdl, NormalPagePriority | MdlMappingNoExecute);
    if (!va) {
        MmUnlockPages(mdl);
        IoFreeMdl(mdl);
        return FALSE;
    }

    out->mdl = mdl;
    out->kernelVa = va;
    return TRUE;
}

static VOID KervanUnmapMemory(KervanMdlMapping* m) {
    if (!m) return;
    if (m->kernelVa && m->mdl) MmUnmapLockedPages(m->kernelVa, m->mdl);
    if (m->mdl) { MmUnlockPages(m->mdl); IoFreeMdl(m->mdl); }
    m->kernelVa = NULL;
    m->mdl = NULL;
}

// ─── İmza Tarama ─────────────────────────────────────────────────────────────

static PVOID KervanFindSignatureAddress(PEPROCESS target) {
    PVOID result = NULL;
    KAPC_STATE apc;

    KeStackAttachProcess(target, &apc);

    PVOID addr = (PVOID)0x10000;
    PVOID addrEnd = (PVOID)0x7FFFFFFF0000ULL;

    while (addr < addrEnd && !result) {
        MEMORY_BASIC_INFORMATION mbi = { 0 };
        NTSTATUS st = ZwQueryVirtualMemory((HANDLE)-1, addr, 0, &mbi, sizeof(mbi), NULL);

        if (!NT_SUCCESS(st)) {
            addr = (PVOID)((ULONG_PTR)addr + 0x1000);
            continue;
        }

        if (mbi.State == MEM_COMMIT && mbi.Protect == PAGE_READWRITE) {
            __try {
                ULONG64* ptr = (ULONG64*)mbi.BaseAddress;
                SIZE_T   count = mbi.RegionSize / sizeof(ULONG64);
                for (SIZE_T i = 0; i < count; i++) {
                    if (ptr[i] == MAGIC_SIGNATURE) {
                        result = &ptr[i];
                        break;
                    }
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {}
        }

        addr = (PVOID)((ULONG_PTR)mbi.BaseAddress + mbi.RegionSize);
    }

    KeUnstackDetachProcess(&apc);
    return result;
}

// ─── Bellek Okuma ─────────────────────────────────────────────────────────────

static NTSTATUS KervanReadMemory(PSharedCommunicationData shm) {
    ULONG64 targetPid = shm->targetPid;
    ULONG64 targetAddress = shm->targetAddress;
    ULONG64 size = shm->size;

    if (!targetPid || targetPid == 4)    return STATUS_INVALID_PARAMETER;
    if (!size || size > KERVAN_MAX_READ) return STATUS_INVALID_PARAMETER;
    if (targetAddress < 0x1000)          return STATUS_INVALID_PARAMETER;

    PEPROCESS game = NULL;
    NTSTATUS  st = PsLookupProcessByProcessId((HANDLE)(ULONG_PTR)targetPid, &game);
    if (!NT_SUCCESS(st)) return st;

    if (!KervanIsProcessAlive(game)) {
        ObDereferenceObject(game);
        return STATUS_PROCESS_IS_TERMINATING;
    }

    RtlZeroMemory(shm->data, (SIZE_T)size);

    SIZE_T transferred = 0;
    st = MmCopyVirtualMemory(
        game, (PVOID)(ULONG_PTR)targetAddress,
        PsGetCurrentProcess(), shm->data,
        (SIZE_T)size, KernelMode, &transferred);

    ObDereferenceObject(game);
    return st;
}

// ─── Ana İletişim Thread'i ────────────────────────────────────────────────────

static VOID KervanThread(PVOID ctx) {
    UNREFERENCED_PARAMETER(ctx);

    while (TRUE) {
        PEPROCESS      caller = NULL;
        PVOID          userShm = NULL;
        KervanMdlMapping shmMap = { 0 };

        // 1. kervan.exe'yi bul
        while (!caller) {
            caller = KervanFindProcess(TARGET_PROCESS_NAME);
            if (!caller) KervanSleep(1000);
        }

        // 2. İmza adresini bul
        while (KervanIsProcessAlive(caller) && !userShm) {
            userShm = KervanFindSignatureAddress(caller);
            if (!userShm) KervanSleep(500);
        }

        if (!userShm || !KervanIsProcessAlive(caller)) {
            ObDereferenceObject(caller);
            KervanSleep(500);
            continue;
        }

        // 3. shm'i kernel'e map et — VirtualLock ile fiziksel bellekte kilitli
        if (!KervanMapUserMemory(caller, userShm,
            sizeof(SharedCommunicationData),
            IoModifyAccess, &shmMap))
        {
            ObDereferenceObject(caller);
            KervanSleep(500);
            continue;
        }

        PSharedCommunicationData shm = (PSharedCommunicationData)shmMap.kernelVa;

        // 4. Event handle'larını kernel KEVENT objelerine çevir
        PKEVENT requestEvent = NULL;
        PKEVENT responseEvent = NULL;

        // caller process context'inde handle'ları resolve et
        KAPC_STATE apc;
        KeStackAttachProcess(caller, &apc);

        NTSTATUS st1 = ObReferenceObjectByHandle(
            (HANDLE)(ULONG_PTR)shm->requestEventHandle,
            EVENT_MODIFY_STATE | SYNCHRONIZE,
            *ExEventObjectType,
            UserMode,
            (PVOID*)&requestEvent,
            NULL);

        NTSTATUS st2 = ObReferenceObjectByHandle(
            (HANDLE)(ULONG_PTR)shm->responseEventHandle,
            EVENT_MODIFY_STATE | SYNCHRONIZE,
            *ExEventObjectType,
            UserMode,
            (PVOID*)&responseEvent,
            NULL);

        KeUnstackDetachProcess(&apc);

        if (!NT_SUCCESS(st1) || !NT_SUCCESS(st2)) {
            if (requestEvent)  ObDereferenceObject(requestEvent);
            if (responseEvent) ObDereferenceObject(responseEvent);
            KervanUnmapMemory(&shmMap);
            ObDereferenceObject(caller);
            KervanSleep(500);
            continue;
        }

        // 5. Hazır sinyali ver
        shm->status = 1;
        KeSetEvent(responseEvent, IO_NO_INCREMENT, FALSE);

        // 6. Event bazlı komut döngüsü — spinlock yok, CPU waste yok
        LARGE_INTEGER timeout;
        timeout.QuadPart = -(10000LL * 5000); // 5 sn timeout

        while (KervanIsProcessAlive(caller)) {

            // kervan.exe'nin SetEvent yapmasını bekle
            NTSTATUS waitSt = KeWaitForSingleObject(
                requestEvent, Executive, KernelMode, FALSE, &timeout);

            if (waitSt == STATUS_TIMEOUT) continue;
            if (!KervanIsProcessAlive(caller)) break;

            INT32 cmd = shm->commandCode;

            if (cmd == CMD_READ_MEMORY) {
                NTSTATUS rst = KervanReadMemory(shm);
                shm->status = NT_SUCCESS(rst) ? 1 : -1;
                shm->commandCode = CMD_IDLE;
                // kervan.exe'yi uyandır
                KeSetEvent(responseEvent, IO_NO_INCREMENT, FALSE);
            }
            else if (cmd == CMD_EXIT) {
                shm->status = 1;
                shm->commandCode = CMD_IDLE;
                KeSetEvent(responseEvent, IO_NO_INCREMENT, FALSE);
                break;
            }
        }

        // 7. Temizlik
        ObDereferenceObject(requestEvent);
        ObDereferenceObject(responseEvent);
        KervanUnmapMemory(&shmMap);
        ObDereferenceObject(caller);
        KervanSleep(500);
    }

    PsTerminateSystemThread(STATUS_SUCCESS);
}

// ─── Driver Entry ─────────────────────────────────────────────────────────────

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
    UNREFERENCED_PARAMETER(DriverObject);
    UNREFERENCED_PARAMETER(RegistryPath);

    HANDLE   hThread = NULL;
    NTSTATUS st = PsCreateSystemThread(&hThread, GENERIC_ALL, NULL, NULL, NULL, KervanThread, NULL);

    if (NT_SUCCESS(st))
        ZwClose(hThread);

    return STATUS_SUCCESS;
}