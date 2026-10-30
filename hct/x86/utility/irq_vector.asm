BITS 32

GLOBAL HctCommonIrqHandler
GLOBAL HctIrqStubs
EXTERN HctpInterruptHandler

section .text

%macro IRQ_STUB 1
align 8
HctIrq%1:
    push dword 0        ; Dummy ErrCode
    push dword %1       ; IsrVector (0x20 .. 0xFF)
    jmp HctCommonIrqHandler
%endmacro

; Generate IRQ Stubs (0x20 s/d 0xFF)
%assign i 0x20
%rep (0x100 - 0x20)
    IRQ_STUB %[i]
    %assign i i+1
%endrep

;
; VOID CDECL HctCommonIrqHandler(VOID)
;
align 16
HctCommonIrqHandler:
    sub esp, 0x60               ; Alokasikan sisa KTRAP_FRAME (0x00 s/d 0x5F)

    mov [esp + 0x44], eax       ; Simpan EAX ke KTRAP_FRAME.Eax
    mov eax, [esp + 0x60]       ; EAX = IsrVector (baca dari stack)

    ; Simpan General Purpose Registers ke KTRAP_FRAME
    mov [esp + 0x60], ebp       ; KTRAP_FRAME.Ebp (menimpa lokasi bekas IsrVector)
    mov [esp + 0x5C], ebx       ; KTRAP_FRAME.Ebx
    mov [esp + 0x58], esi       ; KTRAP_FRAME.Esi
    mov [esp + 0x54], edi       ; KTRAP_FRAME.Edi
    mov [esp + 0x40], ecx       ; KTRAP_FRAME.Ecx
    mov [esp + 0x3C], edx       ; KTRAP_FRAME.Edx

    ; Simpan Segment Registers
    mov [esp + 0x50], fs        ; KTRAP_FRAME.SegFs
    mov [esp + 0x38], ds        ; KTRAP_FRAME.SegDs
    mov [esp + 0x34], es        ; KTRAP_FRAME.SegEs
    mov [esp + 0x30], gs        ; KTRAP_FRAME.SegGs

    ; ExceptionList & PreviousMode
    mov ecx, [fs:0]
    mov [esp + 0x4C], ecx       ; KTRAP_FRAME.ExceptionList
    mov dword [esp + 0x48], 0   ; KTRAP_FRAME.PreviousPreviousMode (0 = KernelMode)

    ; Panggil C Function: HctpInterruptHandler(PKTRAP_FRAME Frame, ULONG IsrVector)
    push eax                    ; Parameter 2: IsrVector
    lea ecx, [esp + 4]          ; ECX = Pointer tepat ke offset 0x00 KTRAP_FRAME
    push ecx                    ; Parameter 1: PKTRAP_FRAME Frame
    call HctpInterruptHandler

    ; Restore Segment Registers
    mov gs, [esp + 0x30]
    mov es, [esp + 0x34]
    mov ds, [esp + 0x38]
    mov fs, [esp + 0x50]

    ; Restore General Purpose Registers
    mov edx, [esp + 0x3C]
    mov ecx, [esp + 0x40]
    mov eax, [esp + 0x44]
    mov edi, [esp + 0x54]
    mov esi, [esp + 0x58]
    mov ebx, [esp + 0x5C]
    mov ebp, [esp + 0x60]

    ; Pop KTRAP_FRAME (0x60) + ErrCode (0x04) -> Langsung menunjuk ke EIP (0x68)
    add esp, 0x68
    iretd

;
; Tabel Pointer IRQ Stubs
;
section .data
align 4
HctIrqStubs:
%assign i 0x20
%rep (0x100 - 0x20)
    dd HctIrq%[i]
    %assign i i+1
%endrep
