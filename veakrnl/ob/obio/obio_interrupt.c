#include <veakrnl.h>

VEASTATUS
VEAPI
ObioDummyInterruptHandler(
    PVOID Object,
    POIP Oip
)
{
    if (!Object) {
        return FALSE;
    }

    POBJECT_HEADER Header = OBJECT_TO_OBJECT_HEADER(Object);
    POBIO_INTERRUPT_HANDLER SpecificHandler = NULL;

    if (Header->Type == ObpDeviceObjectType) {
        PDEVICE_OBJECT Device = (PDEVICE_OBJECT)Object;
        SpecificHandler = Device->InterruptHandler;

        // Fallback: Jika Device tidak punya handler, coba lempar ke Driver induknya
        if (!SpecificHandler && Device->DriverObject) {
            SpecificHandler = Device->DriverObject->InterruptHandler;
        }
    } 
    else if (Header->Type == ObpDriverObjectType) {
        PDRIVER_OBJECT Driver = (PDRIVER_OBJECT)Object;
        SpecificHandler = Driver->InterruptHandler;
    }

    if (SpecificHandler != NULL) {
        // Panggil handler kustom milik Device/Driver itu sendiri!
        return SpecificHandler(Object, Oip);
    }

    KdPrintf("OBIO: Unhandled interrupt/OIP on object %p!\n\r", Object);
    return FALSE;
}