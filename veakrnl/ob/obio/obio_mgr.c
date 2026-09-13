#include <veakrnl.h>

KSPIN_LOCK ObioMgrSpinLock = 0;

VEASTATUS
VEAPI
ObioCreateNewDevice(
    PDRIVER_OBJECT DriverObject,
    ULONG DeviceExtensionSize,
    PCHAR DeviceName,         // Contoh: "serial0"
    ULONG DeviceType,
    PDEVICE_OBJECT *NewDevice // Output pointer
)
{
    if(!DriverObject || !NewDevice)
    {
        DPRINT("ERROR: DriverObject: %p, NewDevice: %p.\n", DriverObject, NewDevice);
        return STATUS_INVALID_PARAMETER;
    }

    PDEVICE_OBJECT CreatedDevice = NULL;
    VEASTATUS Status;
    ANSI_STRING NameAnsi;
    OBJECT_ATTRIBUTES Attr;
    ULONG TotalBodySize = sizeof(DEVICE_OBJECT) + DeviceExtensionSize;

    if (DeviceName != NULL) {
        // Hitung panjang string manual
        USHORT Len = 0;
        while (DeviceName[Len] != '\0') Len++;
        
        NameAnsi.Buffer = DeviceName;
        NameAnsi.Length = Len;
        NameAnsi.MaximumLength = Len + 1;

        // Pasang RootDirectory langsung ke /Device!
        Attr.RootDirectory = ObpDeviceDirectoryObject;
        Attr.ObjectName = &NameAnsi;
        Attr.Attributes = OBJ_CASE_INSENSITIVE;
    }

    Status = ObCreateObject(
        KernelMode,
        ObpDeviceObjectType, 
        (DeviceName != NULL) ? &Attr : NULL,
        KernelMode,
        NULL,
        TotalBodySize, // <--- KUNCI: Pass TotalBodySize di sini!
        0,
        0,
        (PVOID*)&CreatedDevice
    );

    if (Status != STATUS_SUCCESS) {
        DPRINT("ERROR: Status: 0x%x.\n", Status);
        return Status;
    }

    if (DeviceExtensionSize > 0) {
        CreatedDevice->DeviceExtension = (PVOID)(CreatedDevice + 1);
    } else {
        CreatedDevice->DeviceExtension = NULL;
    }

    CreatedDevice->DriverObject = DriverObject;
    CreatedDevice->DeviceType = DeviceType;
    CreatedDevice->NextDevice = NULL;

    if (DriverObject->DeviceObject == NULL) {
        // Ini device pertama milik driver
        DriverObject->DeviceObject = CreatedDevice;
    } else {
        // Cari device terakhir di rantai, lalu sambungkan
        PDEVICE_OBJECT Current = DriverObject->DeviceObject;
        while (Current->NextDevice != NULL) {
            Current = Current->NextDevice;
        }
        Current->NextDevice = CreatedDevice;
    }

    if (DeviceName != NULL) {
        Status = ObInsertObject(CreatedDevice, &Attr);
        if (Status != STATUS_SUCCESS) {
            // Kalau gagal insert (misal nama "serial0" udah ada), 
            // fungsi ObInsertObject otomatis nge-dereference (hancurin) objeknya.
            DPRINT("ERROR: Status: 0x%x.\n", Status);
            *NewDevice = NULL;
            return Status;
        }
    }

    *NewDevice = CreatedDevice;
    
    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
ObioCreateDriver(
    PCHAR DriverName,                  // Contoh: "Serial"
    PDRIVER_INITIALIZE InitializationFunction,
    OUT PDRIVER_OBJECT *DriverObject
)
{
    if (!DriverName || !InitializationFunction) {
        return STATUS_INVALID_PARAMETER;
    }

    PDRIVER_OBJECT NewDriver = NULL;
    VEASTATUS Status;

    // 1. Siapkan Atribut untuk menaruh driver di /Driver
    ANSI_STRING NameAnsi;
    USHORT Len = 0;
    while (DriverName[Len] != '\0') Len++;
    
    NameAnsi.Buffer = DriverName;
    NameAnsi.Length = Len;
    NameAnsi.MaximumLength = Len + 1;

    OBJECT_ATTRIBUTES Attr;
    Attr.RootDirectory = ObpDriverDirectoryObject; // Masuk ke /Driver
    Attr.ObjectName = &NameAnsi;
    Attr.Attributes = OBJ_CASE_INSENSITIVE;

    // 2. Buat Objek (Fase Alokasi)
    Status = ObCreateObject(
        KernelMode,
        ObpDriverObjectType,
        &Attr,
        KernelMode,
        NULL,
        sizeof(DRIVER_OBJECT), // ObjectSize (atau 0)
        0,                     // PagedPoolCharge
        0,                     // NonPagedPoolCharge
        (PVOID*)&NewDriver
    );

    if (!VEA_SUCCESS(Status)) {
        return Status;
    }

    RtlZeroMemory(NewDriver, sizeof(DRIVER_OBJECT));

    // Inisialisasi dasar DRIVER_OBJECT
    NewDriver->DeviceObject = NULL;
    NewDriver->DriverStart = (PVOID)InitializationFunction; // Anggap ini base address-nya

    // Masukkan ke dalam Object Tree (/Driver/Serial)
    Status = ObInsertObject(NewDriver, &Attr);
    if (Status != STATUS_SUCCESS) {
        return Status;
    }

    // 5. EKSEKUSI DRIVER ENTRY!
    // Di sinilah driver mulai hidup dan membuat Device-nya
    Status = InitializationFunction(NewDriver);

    if (Status != STATUS_SUCCESS) {
        // Jika driver gagal inisialisasi, cabut lagi dari sistem
        ObDereferenceObject(NewDriver);
        return Status;
    }
    
    /* Attach them again */
    if(DriverObject != NULL)
    {
        *DriverObject = NewDriver;
    }

    return STATUS_SUCCESS;
}

VEASTATUS
VEAPI
ObioAttachInterruptToThisObject(
    PVOID Object,
    POBIO_INTERRUPT_HANDLER CustomHandler
)
{
    if (!Object || !CustomHandler) {
        return STATUS_INVALID_PARAMETER;
    }

    POBJECT_HEADER Header = OBJECT_TO_OBJECT_HEADER(Object);

    /* FIXME: Now we only support stacking Device and Driver object type
       to another object. But our architectural of VeaOS say any object
       can be attached and have it's own Interrupt Handler */
    if (Header->Type == ObpDeviceObjectType) {
        PDEVICE_OBJECT Device = (PDEVICE_OBJECT)Object;
        Device->InterruptHandler = CustomHandler;
        return STATUS_SUCCESS;
    } 
    else if (Header->Type == ObpDriverObjectType) {
        PDRIVER_OBJECT Driver = (PDRIVER_OBJECT)Object;
        Driver->InterruptHandler = CustomHandler;
        return STATUS_SUCCESS;
    }

    return STATUS_OBJECT_TYPE_MISMATCH;
}

PVOID
VEAPI
ObioAttachObject(
    IN PVOID SourceObject,
    IN PVOID TargetObject
)
{
    if (!SourceObject || !TargetObject) return NULL;

    /* Acquire Spinlock */
    KIRQL OldIrql;
    KsAcquireSpinLock(&ObioMgrSpinLock, &OldIrql);

    POBJECT_HEADER SourceHeader = OBJECT_TO_OBJECT_HEADER(SourceObject);
    POBJECT_HEADER TargetHeader = OBJECT_TO_OBJECT_HEADER(TargetObject);

    if (!SourceHeader->Type || !SourceHeader->Type->AllowAttachByDefault ||
        !TargetHeader->Type || !TargetHeader->Type->AllowAttachByDefault)
    {
        kdp_print("OBIO: Cannot attach object! ObjectType does not allow attachment.\n\r");
        return NULL;
    }

    if (SourceHeader->LowerAttachedObject != NULL)
    {
        // Source udah nempel di object lain -- gak boleh attach dobel
        KsReleaseSpinLock(&ObioMgrSpinLock, OldIrql);
        return NULL;
    }

    // Cari puncak stack TargetObject sekarang (mungkin TargetObject
    // sendiri udah ada yang numpuk di atasnya)
    PVOID CurrentTop = TargetObject;
    POBJECT_HEADER CurrentTopHeader = TargetHeader;

    while (CurrentTopHeader->UpperAttachedObject != NULL)
    {
        CurrentTop = CurrentTopHeader->UpperAttachedObject;
        CurrentTopHeader = OBJECT_TO_OBJECT_HEADER(CurrentTop);
    }

    SourceHeader->LowerAttachedObject = CurrentTop;
    CurrentTopHeader->UpperAttachedObject = SourceObject;

    SourceHeader->StackDepth = CurrentTopHeader->StackDepth + 1;

    ObReferenceObject(CurrentTop);

    KsReleaseSpinLock(&ObioMgrSpinLock, OldIrql);

    return CurrentTop;
}

VOID
VEAPI
ObioDetachObject(
    IN PVOID Object
)
{
    if (!Object) return;

    KIRQL OldIrql;
    PVOID LowerObjectToDereference = NULL;

    KsAcquireSpinLock(&ObioMgrSpinLock, &OldIrql);

    POBJECT_HEADER Header = OBJECT_TO_OBJECT_HEADER(Object);

    // SAFETY CHECK: Pastikan tidak ada objek di atasnya
    if (Header->UpperAttachedObject != NULL)
    {
        kdp_print("OBIO FATAL: Attempting to detach an object that still has attached upper devices!\n\r");
        KsReleaseSpinLock(&ObioMgrSpinLock, OldIrql);
        KsBugCheckEx(INVALID_DEVICE_DETACH, (ULONG_PTR)Object, 0, 0, 0); 
        return;
    }

    if (Header->LowerAttachedObject != NULL)
    {
        POBJECT_HEADER LowerHeader = OBJECT_TO_OBJECT_HEADER(Header->LowerAttachedObject);
        LowerHeader->UpperAttachedObject = NULL;

        // Simpan pointer untuk di-dereference NANTI di luar lock
        LowerObjectToDereference = Header->LowerAttachedObject;
        Header->LowerAttachedObject = NULL;
    }

    Header->StackDepth = 1; // balik jadi standalone

    // Lepas Spinlock DULUAN!
    KsReleaseSpinLock(&ObioMgrSpinLock, OldIrql);

    // Sekarang aman untuk dereference di luar Spinlock
    if (LowerObjectToDereference != NULL)
    {
        ObDereferenceObject(LowerObjectToDereference);
    }
}
