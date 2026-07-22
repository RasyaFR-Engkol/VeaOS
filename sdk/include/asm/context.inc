; file: context.inc

%ifndef TRAP_FRAME_INC
%define TRAP_FRAME_INC

%macro RtlSaveTrapFrameContext 0
    push ebp
    push ebx
    push esi
    push edi
    push fs
    push dword [fs:0]
    push 0
    push eax
    push ecx
    push edx
    push ds
    push es
    push gs
    sub esp, 0x30
    
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov dword [fs:0], -1 
%endmacro

%macro RtlRestoreTrapFrameContext 0
    add esp, 0x30
    pop gs
    pop es
    pop ds
    pop edx
    pop ecx
    pop eax
    add esp, 4
    pop dword [fs:0]
    pop fs
    pop edi
    pop esi
    pop ebx
    pop ebp
%endmacro

%endif ; TRAP_FRAME_INC