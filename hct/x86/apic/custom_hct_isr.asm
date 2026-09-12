BITS 32

EXTERN HctApicTimerTickC
GLOBAL HctApicTimerTick
HctApicTimerTick:
    push 0
    push 0D0h
    pushad
    push ds
    push es
    push fs
    push gs
    
    push esp
    call HctApicTimerTickC

    pop gs                  ; Restore Segment Registers
    pop fs
    pop es
    pop ds
    popad                   ; Restore General Purpose Registers
    
    add esp, 8
    iretd

EXTERN HctpDispatcherHandler
GLOBAL HctpDispatch
HctpDispatch:
    push 0
    push 20h
    pushad
    push ds
    push es
    push fs
    push gs
    
    push esp
    call HctpDispatcherHandler

    pop gs                  ; Restore Segment Registers
    pop fs
    pop es
    pop ds
    popad                   ; Restore General Purpose Registers
    
    add esp, 8
    iretd