BITS 32

extern ksi_dispatch_exception
global isr_common_stub

isr_common_stub:
    pushad                  ; Push EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI
    
    push ds                 ; Push Segment Registers
    push es
    push fs
    push gs

    mov ax, 0x10            ; Load Kernel Data Segment Selector kamu
    mov ds, ax
    mov es, ax

    ; reset other segment register
    mov ax, 30h
    mov fs, ax
    mov gs, ax

    push esp                ; ESP sekarang jadi pointer ke KREGISTER_FRAME
    call ksi_dispatch_exception

    pop gs                  ; Restore Segment Registers
    pop fs
    pop es
    pop ds
    popad                   ; Restore General Purpose Registers
    
    add esp, 8              ; Bersihkan Vector & Error Code dari stack
    iretd

%assign i 0
%rep 32
    global isr_%[i]_entry
    isr_%[i]_entry:
        ; Cek apakah exception ini otomatis ngasih Error Code dari CPU
        ; Exception dengan error code: 8, 10, 11, 12, 13, 14, 17, 30
        %if i == 8 || (i >= 10 && i <= 14) || i == 17 || i == 30
            push i          ; CPU udah push error code, kita tinggal push nomor Vectormya
            jmp isr_common_stub
        %else
            push 0          ; Dummy error code karena CPU ga nge-push
            push i          ; Push nomor Vector
            jmp isr_common_stub
        %endif
%assign i i+1
%endrep
