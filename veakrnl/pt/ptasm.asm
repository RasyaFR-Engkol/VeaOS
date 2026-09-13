BITS 32

%define KTHREAD_KERNEL_STACK 0x00

GLOBAL PtSwapContext
PtSwapContext:
    pushfd
    push ebp
    push ebx
    push esi
    push edi
    mov eax, [esp + 24]
    mov edx, [esp + 28]
    mov [eax + KTHREAD_KERNEL_STACK], esp
    mov esp, [edx + KTHREAD_KERNEL_STACK]
    pop edi
    pop esi
    pop ebx
    pop ebp
    popfd

    ret 8