#include <veakrnl.h>

POBJECT_TYPE ObpDriverObjectType = NULL;
POBJECT_TYPE ObpDeviceObjectType = NULL;
PVOID ObpFileSystemDirectoryObject = NULL; // Folder baru buat File System

VEASTATUS
VEAPI
ObioDummyInterruptHandler(
    PVOID Object,
    POIP Oip
);

/* EXAMPLE DRIVER*/

VOID 
VEAPI
SerialWrite(char c)
{
    while ((inb(COM1_PORT + 5) & 0x20) == 0) {
        // CPU nunggu (bisa dikasih instruksi 'pause' di x86 buat efisiensi)
        __asm__ volatile("pause");
    }
    // Tembak karakternya ke port data!
    outb(COM1_PORT, c);
}

BOOLEAN
VEAPI
SerialProcessInterrupt(PVOID Object, POIP Oip)
{
    PDEVICE_OBJECT Device = (PDEVICE_OBJECT)Object;

    POIP_STACK Stack = Oip->CurrentStackLocation;
    VEASTATUS Status = STATUS_SUCCESS;

    if (!Stack) {
        Oip->IoStatus = STATUS_INVALID_PARAMETER;
        return FALSE; // Gagal proses
    }

    switch (Stack->MajorFunction) {
        
        case OIP_MJ_CREATE:
            // Aplikasi memanggil CreateFile("/Device/serial0")
            kdp_print("SERIAL: Membuka koneksi ke port serial...\n\r");
            Oip->Information = 0;
            break;

        case OIP_MJ_CLOSE:
            // Aplikasi menutup handle file
            kdp_print("SERIAL: Menutup koneksi port serial...\n\r");
            Oip->Information = 0;
            break;

        case OIP_MJ_READ:
            // Aplikasi memanggil ReadFile
            KdPrintf("SERIAL: Membaca %d bytes dari serial port...\n\r", Stack->Parameters.ReadWrite.Length);
            
            // TODO: Tambahkan logika baca dari Port I/O hardware (misal: inb)
            // Simpan data ke Oip->SystemBuffer (atau Oip->UserBuffer tergantung tipe I/O)
            
            // Lapor berapa byte yang sukses dibaca
            Oip->Information = Stack->Parameters.ReadWrite.Length; 
            break;

        case OIP_MJ_WRITE:
            // Aplikasi memanggil WriteFile
            KdPrintf("SERIAL: Menulis %d bytes ke serial port...\n\r", Stack->Parameters.ReadWrite.Length);
            
            PCHAR WriteBuffer = (PCHAR)Oip->SystemBuffer;
            ULONG WriteLength = Stack->Parameters.ReadWrite.Length;
            ULONG BytesWritten = 0;

            if (WriteBuffer != NULL && WriteLength > 0) {
                
                // 3. Tembak karakternya satu per satu pakai fungsi lu
                for (ULONG i = 0; i < WriteLength; i++) {
                    SerialWrite(WriteBuffer[i]);
                    BytesWritten++;
                }
                
                Status = STATUS_SUCCESS;
            } else {
                Status = STATUS_INVALID_PARAMETER;
            }
            
            // Lapor berapa byte yang sukses ditulis
            Oip->Information = BytesWritten;
            break;

        case OIP_MJ_DEVICE_CONTROL:
            // Aplikasi memanggil DeviceIoControl (misal set Baud Rate)
            KdPrintf("SERIAL: Menerima IOCTL code: 0x%X\n\r", Stack->Parameters.DeviceControl.IoControlCode);
            Oip->Information = 0;
            break;

        default:
            // Perintah tidak didukung oleh driver serial
            KdPrintf("SERIAL: Operasi tidak dikenal (Major: 0x%X)\n\r", Stack->MajorFunction);
            Status = STATUS_INVALID_DEVICE_REQUEST;
            Oip->Information = 0;
            break;
    }

    // Update status akhir paket OIP
    Oip->IoStatus = Status;

    ObioCompleteRequest(Oip);

    // Return TRUE menandakan interrupt/OIP ini milik kita dan sukses ditangani
    return TRUE;
}

VEASTATUS
VEAPI
SerialDriverEntry(PDRIVER_OBJECT DriverObject)
{
    PDEVICE_OBJECT SerialDevice0 = NULL;
    VEASTATUS Status;

    kdp_print("SERIAL: DriverEntry dipanggil! Mempersiapkan driver...\n\r");

    // 1. Buat Device dan namai "serial0" agar masuk ke /Device/serial0
    // Anggap 3 adalah konstanta untuk tipe FILE_DEVICE_SERIAL_PORT
    Status = ObioCreateNewDevice(DriverObject, "serial0", 3, &SerialDevice0);
    if (Status != STATUS_SUCCESS) {
        kdp_print("SERIAL: Gagal membuat objek device serial0!\n\r");
        return Status;
    }

    // 2. Tancapkan Custom OIP/Interrupt Handler ke Device yang baru dibuat
    Status = ObioAttachInterruptToThisObject(SerialDevice0, SerialProcessInterrupt);
    if (Status != STATUS_SUCCESS) {
        kdp_print("SERIAL: Gagal menancapkan OIP handler ke serial0!\n\r");
        return Status;
    }

    kdp_print("SERIAL: Driver siap! Device /Device/serial0 berhasil dibuat dan di-routing.\n\r");
    return STATUS_SUCCESS;
}

VOID
VEAPI
TestSerialOutput(VOID)
{
    // 1. Cari Device "/Device/serial0" di Object Tree
    ANSI_STRING DevPath;
    DevPath.Buffer = "/Device/serial0";
    DevPath.Length = 15;
    DevPath.MaximumLength = 16;

    OBJECT_ATTRIBUTES Attr;
    Attr.Length = sizeof(OBJECT_ATTRIBUTES);
    Attr.RootDirectory = NULL;
    Attr.ObjectName = &DevPath;
    Attr.Attributes = OBJ_CASE_INSENSITIVE;

    PDEVICE_OBJECT SerialDevice = NULL;
    
    // Resolve nama path menjadi Pointer Body Device
    VEASTATUS Status = ObReferenceObjectByName(
        &Attr, 
        ObpDeviceObjectType, 
        (PVOID*)&SerialDevice
    );

    if (Status != STATUS_SUCCESS) {
        kdp_print("TEST: Gagal menemukan /Device/serial0!\n\r");
        return;
    }

    // 2. Data yang ingin ditulis ke Serial Port
    char *Pesan = "Halo dari VeaOS Paket OIP!\r\n";
    ULONG PesanLen = 28;

    // 3. Kirim Paket OIP!
    kdp_print("TEST: Mengirim OIP Write ke Serial...\n\r");
    Status = ObioSendWriteRequest(SerialDevice, Pesan, PesanLen);

    if (Status == STATUS_SUCCESS) {
        kdp_print("TEST: OIP Berhasil diproses oleh Driver Serial!\n\r");
    }

    // 4. Lepas referensi device setelah selesai dipakai
    ObDereferenceObject(SerialDevice);
}

/* END OF EXAMPLE DRIVER*/

BOOLEAN
VEAPI
ObioInitSubsystem(
    VOID
)
{
    OBJECT_TYPE_INITIALIZER TypeInit;
    OBJECT_ATTRIBUTES Attr;
    ANSI_STRING NameAnsi;

    RtlZeroMemory(&TypeInit, sizeof(OBJECT_TYPE_INITIALIZER));
    TypeInit.Length = sizeof(OBJECT_TYPE_INITIALIZER);
    TypeInit.PoolTag = 'Driv';
    TypeInit.ObjectSize = sizeof(DRIVER_OBJECT);
    TypeInit.InterruptHandler = ObioDummyInterruptHandler; // <--- Tancapkan OIP Receiver

    ObCreateObjectType("Driver", &TypeInit, &ObpDriverObjectType);

    RtlZeroMemory(&TypeInit, sizeof(OBJECT_TYPE_INITIALIZER));
    TypeInit.Length = sizeof(OBJECT_TYPE_INITIALIZER);
    TypeInit.PoolTag = 'Devc';
    TypeInit.ObjectSize = sizeof(DEVICE_OBJECT);
    TypeInit.InterruptHandler = ObioDummyInterruptHandler; // <--- Tancapkan OIP Receiver

    ObCreateObjectType("Device", &TypeInit, &ObpDeviceObjectType);

    // FIX: Bikin /FileSystem dengan standar arsitektur baru (pakai Object Attributes)
    NameAnsi.Buffer = "FileSystem";
    NameAnsi.Length = 10;
    InitializeObjectAttributes(&Attr, &NameAnsi, 0, ObpRootDirectoryObject, NULL);

    // 1. Alokasi memori (sekarang sudah ada slot untuk NameInfo di belakang layar)
    ObAllocateObject(ObpDirectoryObjectType, &Attr, &ObpFileSystemDirectoryObject);
    
    // 2. Inisialisasi list kepala direktori
    InitializeListHead(&((POBJECT_DIRECTORY)ObpFileSystemDirectoryObject)->Head);
    
    // 3. Masukkan ke object tree (Otomatis mengisi NameInfo & insert ke Root)
    ObInsertObject(ObpFileSystemDirectoryObject, &Attr);

    kdp_print("OBIO: I/O Subsystem successfully initialized!\n\r");

    VEASTATUS Status = ObioCreateDriver("Serial0", SerialDriverEntry);
    
    if (Status == STATUS_SUCCESS) {
        kdp_print("KERNEL: Driver 'Serial' sukses dimuat dan beroperasi.\n\r");
    } else {
        KdPrintf("KERNEL: Gagal memuat driver 'Serial' (Status: 0x%X).\n\r", Status);
    }

    TestSerialOutput();

    return TRUE;
}