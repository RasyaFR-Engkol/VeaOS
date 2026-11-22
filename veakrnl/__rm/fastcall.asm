[BITS 32]

;; This Macro is mean KGDT
%define KGDT32_R0_CODE  0x08
%define KGDT32_R0_DATA  0x10
%define KGDT32_R3_CODE  0x1B
%define KGDT32_R3_DATA  0x23
%define KGDT32_R0_PCR   0x30

;; KTRAP_FRAME Struct Definition
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

GLOBAL KsiFastCallService
GLOBAL KsiSystemCallTrampoline
EXTERN RiSyscallDispatcherHandler

SECTION .text

ALIGN 16
KsiFastCallService:
    SUB esp, KTRAP_FRAME_size

    MOV [esp + KTRAP_FRAME.Eax], eax
    MOV [esp + KTRAP_FRAME.Ecx], ecx
    MOV [esp + KTRAP_FRAME.Edx], edx
    MOV [esp + KTRAP_FRAME.Ebx], ebx
    MOV [esp + KTRAP_FRAME.Ebp], ebp
    MOV [esp + KTRAP_FRAME.Esi], esi
    MOV [esp + KTRAP_FRAME.Edi], edi

    MOV ax, ds
    MOV [esp + KTRAP_FRAME.SegDs], eax
    MOV [esp + KTRAP_FRAME.Ecx], ecx
    MOV [esp + KTRAP_FRAME.Edx], edx
    MOV [esp + KTRAP_FRAME.Ebx], ebx
    MOV [esp + KTRAP_FRAME.Ebp], ebp
    MOV [esp + KTRAP_FRAME.Esi], esi
    MOV [esp + KTRAP_FRAME.Edi], edi

    MOV DWORD [esp + KTRAP_FRAME.HardwareSegSs], KGDT32_R3_DATA

    LEA eax, [edx + 4]
    MOV [esp + KTRAP_FRAME.HardwareEsp], eax

    MOV eax, [edx]
    MOV [esp + KTRAP_FRAME.Eip], eax

    MOV eax, [esp + KTRAP_FRAME.Eax]

    MOV DWORD [esp + KTRAP_FRAME.SegCs], KGDT32_R3_CODE
    MOV DWORD [esp + KTRAP_FRAME.EFlags], 0x0202 ; EFLAGS default dengan IF=1
    MOV DWORD [esp + KTRAP_FRAME.ErrCode], 0
    MOV DWORD [esp + KTRAP_FRAME.PreviousPreviousMode], 1 ; 1 = UserMode

    MOV ax, KGDT32_R0_DATA
    MOV ds, ax
    MOV es, ax
    MOV ax, KGDT32_R0_PCR
    MOV fs, ax

    STI

    PUSH 0
    LEA eax, [esp + 4]
    PUSH eax
    CALL RiSyscallDispatcherHandler


ALIGN 16
; ==============================================================================
; VEASTATUS VEAPI KsiSystemCallTrampoline(
;     IN PVOID TargetFunction,  ; [ebp + 8]
;     IN PULONG_PTR KernelArgs, ; [ebp + 12]
;     IN ULONG ParamBytes       ; [ebp + 16]
; )
; ==============================================================================
KsiSystemCallTrampoline:
    PUSH ebp
    MOV ebp, esp

    ; Preserve callee-saved registers (EBX, ESI, EDI)
    PUSH esi
    PUSH edi
    PUSH ebx

    MOV eax, [ebp + 8]          ; TargetFunction Address
    MOV esi, [ebp + 12]         ; KernelArgs Array Pointer
    MOV ecx, [ebp + 16]         ; ParamBytes

    SHR ecx, 2      
    JZ .call_target
    LEA edi, [esi + ecx * 4 - 4]

.copy_loop:
    MOV edx, [edi]
    PUSH edx            
    SUB edi, 4           
    DEC ecx
    JNZ .copy_loop

.call_target:
    CALL eax

    ; Restore callee-saved registers
    POP ebx
    POP edi
    POP esi
    POP ebp

    ; Clean up 3 parameter 
    RET 12

;; EOF