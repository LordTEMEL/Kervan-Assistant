#pragma once
#include <stdint.h>

#define MAGIC_SIGNATURE  0x4B455256414E0001ULL
#define KERVAN_MAX_READ  4096

typedef enum {
    CMD_IDLE = 0,
    CMD_READ_MEMORY = 1,
    CMD_EXIT = 2
} CommandType;

#pragma pack(push, 1)
typedef struct {
    uint64_t signature;       // Driver'ın struct'ı bulmasını sağlayan imza
    int32_t  commandCode;     // Komut (CommandType)
    int32_t  status;          // 1=hazır/başarılı, -1=hata, 0=bekliyor
    uint64_t targetPid;       // Okunacak process PID
    uint64_t targetAddress;   // Okunacak adres
    uint64_t size;            // Okunacak byte sayısı
    uint64_t requestEventHandle;   // kervan.exe → driver: "komut var"
    uint64_t responseEventHandle;  // driver → kervan.exe: "cevap hazır"
    uint8_t  data[KERVAN_MAX_READ]; // Driver'ın yazdığı veri
} KervanSharedData;
#pragma pack(pop)