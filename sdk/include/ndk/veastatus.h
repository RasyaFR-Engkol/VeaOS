#pragma once

#include "procbind.h"

typedef LONG VEASTATUS, *PVEASTATUS;

#define VEA_SUCCESS(Status) (((VEASTATUS)(Status)) >= 0)

#define STATUS_SUCCESS ((VEASTATUS)0x00000000)
#define STATUS_REPARSE ((VEASTATUS)0x00000104)
#define STATUS_MORE_PROCESSING_REQUIRED  ((VEASTATUS)0x00000105)

#define STATUS_UNSUCCESSFULL             ((VEASTATUS)0xC0000000)
#define STATUS_OBJECT_NAME_NOT_FOUND     ((VEASTATUS)0xC0000001)
#define STATUS_REPARSE_OBJECT_MAY_LOOP   ((VEASTATUS)0xC0000002)
#define STATUS_INSUFFICIENT_MEMORY       ((VEASTATUS)0xC0000003)
#define STATUS_NOT_SUPPORTED             ((VEASTATUS)0xC0000004)
#define STATUS_NOT_IMPLEMENTED           ((VEASTATUS)0xC0000005)
#define STATUS_INVALID_PARAMETER         ((VEASTATUS)0xC0000006)
#define STATUS_OBJECT_TYPE_MISMATCH      ((VEASTATUS)0xC0000007) // Error pas nyari Folder tapi yang ketemu File
#define STATUS_OBJECT_NAME_INVALID       ((VEASTATUS)0xC0000008) // String path mengandung karakter ilegal
#define STATUS_BUFFER_TOO_SMALL          ((VEASTATUS)0xC0000009) // Buffer string (misal ANSI_STRING) kurang panjang
#define STATUS_INVALID_HANDLE            ((VEASTATUS)0xC000000A) // Kepake banget buat Handle Table nanti
#define STATUS_ACCESS_DENIED             ((VEASTATUS)0xC000000B) // Kepake saat lu mulai nerapin Security/ACL
#define STATUS_END_OF_FILE               ((VEASTATUS)0xC000000C) // Mutlak butuh buat File System Manager
#define STATUS_DEVICE_NOT_CONNECTED      ((VEASTATUS)0xC000000D) // Saat driver dipanggil tapi hardwarenya udah dicabut
#define STATUS_TIMEOUT                   ((VEASTATUS)0xC000000E) // Buat urusan Threadin
#define STATUS_OBJECT_NAME_COLLISION     ((VEASTATUS)0xC000000F)
#define STATUS_INVALID_DEVICE_REQUEST    ((VEASTATUS)0xC0000010)
#define STATUS_ACCESS_VIOLATION          ((VEASTATUS)0xC0000011)

/* BUGCHECK */
#define BAD_POOL_CALLER                  0x000000C2
#define BAD_POOL_HEADER                  0x00000019
#define OIP_MULTIPLE_COMPLETE_REQUESTS   0x00000044
#define OIP_STACK_CORRUPTION             0x00000045