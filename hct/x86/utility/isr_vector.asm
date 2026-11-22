BITS 32

GLOBAL HctCommonTrapHandler
GLOBAL HctTrapStubs
EXTERN HctpInterruptHandler

section .text

%macro TRAP_NO_ERR 1
align 8
HctTrap%1:
    push dword 0        ; Dummy ErrCode
    push dword %1       ; IsrVector
    jmp HctCommonTrapHandler
%endmacro

%macro TRAP_WITH_ERR 1
align 8
HctTrap%1:
    push dword %1       ; IsrVector
    jmp HctCommonTrapHandler
%endmacro

; Generate Trap Stubs (0x00 s/d 0x19)
%assign i 0
%rep 0x1A
    %if i == 8 || i == 10 || i == 11 || i == 12 || i == 13 || i == 14 || i == 17
        TRAP_WITH_ERR %[i]
    %else
        TRAP_NO_ERR %[i]
    %endif
    %assign i i+1
%endrep

;
; VOID VEAPI HctCommonTrapHandler(VOID)
;
align 16
HctCommonTrapHandler:
    sub esp, 0x60               ; Alokasikan sisa KTRAP_FRAME (0x00 s/d 0x5F)

    mov [esp + 0x44], eax       ; Simpan EAX ke KTRAP_FRAME.Eax
    mov eax, [esp + 0x60]       ; EAX = IsrVector (baca nilai dari stack)

    ; Simpan General Purpose Registers ke KTRAP_FRAME
    mov [esp + 0x60], ebp       ; KTRAP_FRAME.Ebp (menimpa lokasi bekas IsrVector)
    mov [esp + 0x5C], ebx       ; KTRAP_FRAME.Ebx
    mov [esp + 0x58], esi       ; KTRAP_FRAME.Esi
    mov [esp + 0x54], edi       ; KTRAP_FRAME.Edi
    mov [esp + 0x40], ecx       ; KTRAP_FRAME.Ecx
    mov [esp + 0x3C], edx       ; KTRAP_FRAME.Edx

    ; Simpan Segment Registers (zero-extend 32-bit)
    xor ecx, ecx
    mov cx, fs
    mov [esp + 0x50], ecx       ; KTRAP_FRAME.SegFs
    mov cx, ds
    mov [esp + 0x38], ecx       ; KTRAP_FRAME.SegDs
    mov cx, es
    mov [esp + 0x34], ecx       ; KTRAP_FRAME.SegEs
    mov cx, gs
    mov [esp + 0x30], ecx       ; KTRAP_FRAME.SegGs

    ; Paksa pindah ke Segment Kernel (DS/ES = 0x10, FS = 0x30 KPCR)
    cld
    mov cx, 0x10                ; KGDT32_R0_DATA
    mov ds, cx
    mov es, cx
    mov cx, 0x30                ; KGDT32_R0_PCR
    mov fs, cx

    ; Sekarang [fs:0] aman baca KPCR
    mov ecx, [fs:0]
    mov [esp + 0x4C], ecx       ; KTRAP_FRAME.ExceptionList

    ; Set PreviousMode berdasarkan RPL dari SegCs ([esp + 0x6C])
    mov ecx, [esp + 0x6C]
    and ecx, 3
    shr ecx, 1                  ; RPL 3 -> 1 (UserMode), RPL 0 -> 0 (KernelMode)
    mov [esp + 0x48], ecx       ; KTRAP_FRAME.PreviousPreviousMode

    ; Panggil VEAPI (__stdcall): HctpInterruptHandler(PKTRAP_FRAME Frame, ULONG IsrVector)
    ; Fungsi ini otomatis melakukan `ret 8`, jangan tambah `add esp, 8`!
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

section .data
align 4
HctTrapStubs:
%assign i 0
%rep 0x1A
    dd HctTrap%[i]
    %assign i i+1
%endrep
