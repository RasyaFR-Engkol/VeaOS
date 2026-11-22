BITS 32

%define KTHREAD_KERNEL_STACK 0x00
%define KTHREAD_FXSAVE_AREA 0x1AC

GLOBAL PtSwapContext
PtSwapContext:
    pushfd
    push ebp
    push ebx
    push esi
    push edi

    mov eax, [esp + 24]             ; OldThread
    mov edx, [esp + 28]             ; NewThread

    ; 1. Simpan state FPU/SSE milik OldThread
    mov ecx, [eax + KTHREAD_FXSAVE_AREA]
    test ecx, ecx
    jz .skip_fxsave
    fxsave [ecx]
.skip_fxsave:

    ; 2. Tukar kernel stack
    mov [eax + KTHREAD_KERNEL_STACK], esp
    mov esp, [edx + KTHREAD_KERNEL_STACK]

    ; 3. Pulihkan state FPU/SSE milik NewThread
    mov ecx, [edx + KTHREAD_FXSAVE_AREA]
    test ecx, ecx
    jz .skip_fxrstor
    fxrstor [ecx]
.skip_fxrstor:

    pop edi
    pop esi
    pop ebx
    pop ebp
    popfd

    ret 8