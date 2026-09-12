#include <veakrnl.h>

PACPI_HEADER
VEAPI
HctFindAcpiTable(const char* Signature)
{
    for (ULONG i = 0; i < HctAcpiTableCount; i++) {
        // Cek 4 huruf signature
        if (HctAcpiCache[i].Signature[0] == Signature[0] &&
            HctAcpiCache[i].Signature[1] == Signature[1] &&
            HctAcpiCache[i].Signature[2] == Signature[2] &&
            HctAcpiCache[i].Signature[3] == Signature[3]) 
        {
            return HctAcpiCache[i].Table;
        }
    }
    return NULL;
}