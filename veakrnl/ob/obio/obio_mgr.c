#include <veakrnl.h>

VEASTATUS
VEAPI
ObioCreateNewDevice(
    PDRIVER_OBJECT DriverObject,
    PCHAR DeviceName,         // Contoh: "serial0"
    ULONG DeviceType,
    PDEVICE_OBJECT *NewDevice // Output pointer
)
{
    if(!DriverObject || !NewDevice)
    {
        return STATUS_INVALID_PARAMETER;
    }

    PDEVICE_OBJECT CreatedDevice = NULL;
    VEASTATUS Status;
    ANSI_STRING NameAnsi;
    OBJECT_ATTRIBUTES Attr;

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
        ObpDeviceObjectType, 
        (DeviceName != NULL) ? &Attr : NULL, // Bisa bikin unnamed device kalau DeviceName NULL
        (PVOID*)&CreatedDevice
    );

    if (Status != STATUS_SUCCESS) {
        return Status;
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
    PDRIVER_INITIALIZE InitializationFunction
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
    Status = ObCreateObject(ObpDriverObjectType, &Attr, (PVOID*)&NewDriver);
    if (Status != STATUS_SUCCESS) {
        return Status;
    }

    // 3. Inisialisasi dasar DRIVER_OBJECT
    NewDriver->DeviceObject = NULL;
    NewDriver->DriverStart = (PVOID)InitializationFunction; // Anggap ini base address-nya

    // 4. Masukkan ke dalam Object Tree (/Driver/Serial)
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