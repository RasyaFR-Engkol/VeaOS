#include <veakrnl.h>

POIP
VEAPI
ObioAllocateOip(
    ULONG StackCount
)
{
    ULONG TotalSize = sizeof(OIP) + (StackCount * sizeof(OIP_STACK));

    POIP Oip = (POIP)UlAllocatePoolWithTag(NonPagedPool, TotalSize, OB_OIP_TAG);
    if(!Oip) return NULL;

    RtlZeroMemory(Oip, TotalSize);

    Oip->Size = TotalSize;
    Oip->StackCount = StackCount;
    Oip->CurrentLocation = StackCount + 1;
    Oip->CurrentStackLocation = (POIP_STACK)(Oip + 1) + StackCount;

    return Oip;
}

VEASTATUS
VEAPI
ObioGoInterruptObject(
    PVOID TargetObject,
    POIP Oip
)
{
    POBJECT_HEADER Header = OBJECT_TO_OBJECT_HEADER(TargetObject);

    Oip->CurrentLocation--;
    if (Oip->CurrentLocation <= 0) {
        // FATAL: Driver ngirim OIP terlalu dalam melebihi StackCount awal!
        return STATUS_INVALID_PARAMETER; 
    }

    Oip->CurrentStackLocation--;

    POIP_STACK Stack = Oip->CurrentStackLocation;
    Stack->TargetObject = TargetObject;

    if (Header->Type->InterruptHandler != NULL) {
        return Header->Type->InterruptHandler(TargetObject, Oip);
    }

    Oip->IoStatus = STATUS_NOT_SUPPORTED;
    return STATUS_NOT_SUPPORTED;
}

VOID
VEAPI
ObioCompleteRequest(
    POIP Oip
)
{
    POIP_STACK Stack;
    VEASTATUS Status;

    if (!Oip) {
        KsBugCheckEx(OIP_STACK_CORRUPTION, 0, 0, 0, 0);
    }

    if (Oip->CurrentLocation > Oip->StackCount + 1) {
        KsBugCheckEx(OIP_MULTIPLE_COMPLETE_REQUESTS, (ULONG_PTR)Oip, Oip->CurrentLocation, 0, 0);
    }

    while(Oip->CurrentLocation <= Oip->StackCount)
    {
        Stack = Oip->CurrentStackLocation;

        ULONG_PTR MaxStackAddress = (ULONG_PTR)Oip + Oip->Size;
        if ((ULONG_PTR)Stack >= MaxStackAddress) {
            KsBugCheckEx(OIP_STACK_CORRUPTION, (ULONG_PTR)Oip, (ULONG_PTR)Stack, 0, 0);
        }

        if(Stack->CompletionRoutine != NULL)
        {
            POIP_COMPLETION_ROUTINE Routine = Stack->CompletionRoutine;
            Status = Routine(Stack->TargetObject, Oip, Stack->Context);

            if (Status == STATUS_MORE_PROCESSING_REQUIRED) {
                // STOP UNWINDING! Biarkan OIP tertahan di layer ini.
                // Driver tersebut bertanggung jawab memanggil ObioCompleteRequest 
                // lagi nanti kalau tugas lanjutannya udah beneran beres.
                return; 
            }
        }

        Oip->CurrentLocation++;
        Oip->CurrentStackLocation++;
    }

    if (Oip->UserEvent != NULL) {
        PKEVENT Event = (PKEVENT)Oip->UserEvent;
        Event->Signaled = TRUE;
        
        // Nanti kalau ada scheduler:
        // KeSetEvent(Event, ...); // Bangunin thread yang lagi tidur nunggu event ini
    }

    if (Oip->UserIosb != NULL) {
        *(Oip->UserIosb) = Oip->IoStatus; // Kasih tau status akhirnya ke user
    }

    if (Oip->Flags & OIP_DEALLOCATE_ON_COMPLETION) {
        
        // Cuma free SystemBuffer kalau flag-nya nyala
        if (Oip->SystemBuffer != NULL) {
            UlFreePoolWithTag(Oip->SystemBuffer, Oip->SystemTag); 
        }   
        
        // Hancurkan OIP
        ObioFreeOip(Oip);
    }
}

VEASTATUS
VEAPI
ObioSendWriteRequest(
    PDEVICE_OBJECT DeviceObject,
    PVOID DataBuffer,
    ULONG DataLength
)
{
    if (!DeviceObject || !DataBuffer || DataLength == 0) 
    {
        return STATUS_INVALID_PARAMETER;
    }

    POIP Oip = ObioAllocateOip(1);
    if(!Oip)
    {
        return STATUS_INSUFFICIENT_MEMORY;
    }

    POIP_STACK Stack = ObioGetNextOipStackLocation(Oip);

    Oip->SystemBuffer = UlAllocatePoolWithTag(NonPagedPool, DataLength, 'OIPP');
    if(!Oip->SystemBuffer)
    {
        //ObioFreeOip(Oip);
        return STATUS_INSUFFICIENT_MEMORY;
    }
    Oip->SystemTag = 'OIPP';
    RtlCopyMemory(Oip->SystemBuffer, DataBuffer, DataLength);

    Stack->MajorFunction = OIP_MJ_WRITE; // Perintah Write
    Stack->Parameters.ReadWrite.Length = DataLength;
    Stack->TargetObject = DeviceObject;

    BOOLEAN Handled = ObioGoInterruptObject(DeviceObject, Oip);
    VEASTATUS FinalStatus = Oip->IoStatus;

    UlFreePoolWithTag(Oip->SystemBuffer, Oip->SystemTag);
    ObioFreeOip(Oip);

    if (!Handled) {
        return STATUS_UNSUCCESSFULL;
    }

    return FinalStatus;
}

VEASTATUS
VEAPI
ObioSendReadRequest(
    PDEVICE_OBJECT DeviceObject,
    PVOID DataBuffer,       // Buffer tujuan (di-alokasi oleh pemanggil)
    ULONG DataLength,       // Berapa byte yang mau dibaca
    PULONG BytesRead        // Output: Berapa byte yang aktual berhasil dibaca
)
{
    if (!DeviceObject || !DataBuffer || DataLength == 0 || !BytesRead) {
        return STATUS_INVALID_PARAMETER;
    }

    *BytesRead = 0; // Inisialisasi awal

    // 1. Alokasi OIP dengan 1 layer stack
    POIP Oip = ObioAllocateOip(1);
    if (!Oip) {
        return STATUS_INSUFFICIENT_MEMORY;
    }

    // 2. Ambil next stack
    POIP_STACK Stack = ObioGetNextOipStackLocation(Oip);

    // 3. Setup SystemBuffer untuk operasi Read (Buffered I/O)
    Oip->SystemBuffer = UlAllocatePoolWithTag(NonPagedPool, DataLength, 'OIPP');
    if (!Oip->SystemBuffer) {
        ObioFreeOip(Oip);
        return STATUS_INSUFFICIENT_MEMORY;
    }
    Oip->SystemTag = 'OIPP';

    // (Tidak perlu mengkopi DataBuffer ke SystemBuffer karena ini operasi READ)

    // 4. Setup parameter Stack
    Stack->MajorFunction = OIP_MJ_READ;
    Stack->Parameters.ReadWrite.Length = DataLength;
    Stack->TargetObject = DeviceObject;

    // 5. Eksekusi
    BOOLEAN Handled = ObioGoInterruptObject(DeviceObject, Oip);
    VEASTATUS FinalStatus = Oip->IoStatus;

    if (Handled && FinalStatus == STATUS_SUCCESS) {
        // Jika sukses, copy data dari Kernel Buffer ke User/Caller Buffer
        ULONG ReadAmount = (ULONG)Oip->Information;
        if (ReadAmount > DataLength) {
            ReadAmount = DataLength; // Safety check
        }
        
        RtlCopyMemory(DataBuffer, Oip->SystemBuffer, ReadAmount);
        *BytesRead = ReadAmount;
    }

    UlFreePoolWithTag(Oip->SystemBuffer, Oip->SystemTag);
    ObioFreeOip(Oip);

    if (!Handled) {
        return STATUS_UNSUCCESSFULL;
    }

    return FinalStatus;
}

POIP
VEAPI
ObioBuildSynchronusFsdRequest(
    ULONG MajorFunction,
    PVOID Object,
    PVOID Buffer,
    ULONG Length,
    PLARGE_INTEGER StartingOffset, // Opsional: Untuk file seek offset
    PKEVENT Event,                 // Event untuk me-wait sampai I/O selesai
    PVEASTATUS IoStatusBlock       // Tempat menaruh status akhir
)
{
    POBJECT_HEADER Header = OBJECT_TO_OBJECT_HEADER(Object);
    CHAR StackSize = Header->StackDepth;

    POIP Oip = ObioAllocateOip(StackSize);
    if(!Oip)
    {
        return NULL;
    }

    Oip->UserEvent = Event;
    Oip->UserIosb = IoStatusBlock; // (Lu perlu tambahkan UserIosb di struktur OIP)

    POIP_STACK Stack = ObioGetNextOipStackLocation(Oip);
    Stack->MajorFunction = (UCHAR)MajorFunction;
    Stack->TargetObject = Object;
    Stack->Parameters.ReadWrite.Length = Length;

    if (StartingOffset) {
        Stack->Parameters.ReadWrite.Offset = *StartingOffset;
    }

    if (Buffer != NULL && Length > 0) {
        Oip->SystemBuffer = UlAllocatePoolWithTag(NonPagedPool, Length, 'OIPP');
        if (!Oip->SystemBuffer) {
            ObioFreeOip(Oip);
            return NULL;
        }
        Oip->SystemTag = 'OIPP';

        // Jika operasi WRITE, copy dari User ke System
        if (MajorFunction == OIP_MJ_WRITE) {
            RtlCopyMemory(Oip->SystemBuffer, Buffer, Length);
        }
    }

    return Oip;
}

POIP
VEAPI
ObioBuildAsynchronousFsdRequest(
    ULONG MajorFunction,
    PVOID Object,
    PVOID Buffer,
    ULONG Length,
    PLARGE_INTEGER StartingOffset,
    PVEASTATUS IoStatusBlock
)
{
    POBJECT_HEADER Header = OBJECT_TO_OBJECT_HEADER(Object);
    CHAR StackSize = Header->StackDepth;

    POIP Oip = ObioAllocateOip(StackSize);
    if (!Oip) return NULL;

    Oip->UserEvent = NULL; // Tidak ada event sinkronisasi
    Oip->UserIosb = IoStatusBlock;
    Oip->Flags |= OIP_DEALLOCATE_ON_COMPLETION;

    POIP_STACK Stack = ObioGetNextOipStackLocation(Oip);
    Stack->MajorFunction = (UCHAR)MajorFunction;
    Stack->TargetObject = Object;
    Stack->Parameters.ReadWrite.Length = Length;

    if (StartingOffset) {
        Stack->Parameters.ReadWrite.Offset = *StartingOffset;
    }

    if (Buffer != NULL && Length > 0) {
        Oip->SystemBuffer = UlAllocatePoolWithTag(NonPagedPool, Length, 'OIPP');
        if (!Oip->SystemBuffer) {
            ObioFreeOip(Oip);
            return NULL;
        }
        Oip->SystemTag = 'OIPP';

        if (MajorFunction == OIP_MJ_WRITE) {
            RtlCopyMemory(Oip->SystemBuffer, Buffer, Length);
        }
    }

    return Oip;
}

VOID
VEAPI
ObioFreeOip(
    POIP Oip
)
{
    if (!Oip) return;

    // Pastikan tag-nya sama dengan yang lu pakai di ObioAllocateOip
    UlFreePoolWithTag(Oip, OB_OIP_TAG); 
}