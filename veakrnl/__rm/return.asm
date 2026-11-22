[BITS 32]

GLOBAL KsiFastCallExitHandler
GLOBAL KsiSystemCallReturn
EXTERN KsiServiceExit

; Segment Selectors Standard VeaOS
%define KGDT32_R0_CODE 0x08
%define KGDT32_R0_DATA 0x10
%define KGDT32_R0_PCR  0x30
%define KGDT32_R3_CODE 0x1B
%define KGDT32_R3_DATA 0x23
%define KGDT32_R3_TEB  0x43

STRUC KTRAP_FRAME
    .DbgEbp               RESD 1
    .DbgEip               RESD 1
    .DbgArgMark           RESD 1
    .DbgArgPointer        RESD 1
    .TempSegCs            RESD 1
    .TempEsp              RESD 1
    .Dr0                  RESD 1
    .Dr1                  RESD 1
    .Dr2                  RESD 1
    .Dr3                  RESD 1
    .Dr6                  RESD 1
    .Dr7                  RESD 1
    .SegGs                RESD 1
    .SegEs                RESD 1
    .SegDs                RESD 1
    .Edx                  RESD 1
    .Ecx                  RESD 1
    .Eax                  RESD 1
    .PreviousPreviousMode RESD 1
    .ExceptionList        RESD 1
    .SegFs                RESD 1
    .Edi                  RESD 1
    .Esi                  RESD 1
    .Ebx                  RESD 1
    .Ebp                  RESD 1
    .ErrCode              RESD 1
    .Eip                  RESD 1
    .SegCs                RESD 1
    .EFlags               RESD 1
    .HardwareEsp          RESD 1
    .HardwareSegSs        RESD 1
    .V86Es                RESD 1
    .V86Ds                RESD 1
    .V86Fs                RESD 1
    .V86Gs                RESD 1
ENDSTRUC

SECTION .text

ALIGN 16
; ==============================================================================
; NORET VOID VEAPI KsiFastCallExitHandler(IN PKTRAP_FRAME TrapFrame)
; Return ke User Mode via SYSEXIT (Aturan: EDX=EIP, ECX=ESP, EAX=Status)
; ==============================================================================
KsiFastCallExitHandler:
    CLI                         ; Matikan IRQ sejak awal sebelum ubah segment register
    MOV ecx, [esp + 4]          ; ECX = Pointer KTRAP_FRAME

    ; Restore Selector Segment Ring 3
    MOV ax, KGDT32_R3_DATA
    MOV ds, ax
    MOV es, ax
    
    MOV ax, KGDT32_R3_TEB
    MOV fs, ax

    ; Restore Callee-Saved Registers
    MOV ebx, [ecx + KTRAP_FRAME.Ebx]
    MOV ebp, [ecx + KTRAP_FRAME.Ebp]
    MOV esi, [ecx + KTRAP_FRAME.Esi]
    MOV edi, [ecx + KTRAP_FRAME.Edi]

    ; Setup Register SYSEXIT
    MOV edx, [ecx + KTRAP_FRAME.Eip]            ; EDX = User EIP
    MOV eax, [ecx + KTRAP_FRAME.Eax]            ; EAX = Return Status
    MOV ecx, [ecx + KTRAP_FRAME.HardwareEsp]    ; ECX = User ESP

    STI
    SYSEXIT

ALIGN 16
KsiSystemCallReturn:
    MOV ecx, [esp + 4]

    MOV ax, [ecx + KTRAP_FRAME.SegDs]
    CMP ax, 0
    JNE .has_ds
    MOV ax, KGDT32_R0_DATA
.has_ds:
    MOV ds, ax

    MOV ax, [ecx + KTRAP_FRAME.SegEs]
    CMP ax, 0
    JNE .has_es
    MOV ax, KGDT32_R0_DATA
.has_es:
    MOV es, ax

    MOV ax, [ecx + KTRAP_FRAME.SegFs]
    CMP ax, 0
    JNE .has_fs
    MOV ax, KGDT32_R0_PCR
.has_fs:
    MOV fs, ax

    MOV ebx, [ecx + KTRAP_FRAME.Ebx]
    MOV esi, [ecx + KTRAP_FRAME.Esi]
    MOV edi, [ecx + KTRAP_FRAME.Edi]
    MOV ebp, [ecx + KTRAP_FRAME.Ebp]

    MOV eax, [ecx + KTRAP_FRAME.Eax]
    MOV edx, [ecx + KTRAP_FRAME.Eip]

    PUSH DWORD [ecx + KTRAP_FRAME.EFlags]
    POPFD

    LEA esp, [ecx + KTRAP_FRAME_size]
    JMP edx