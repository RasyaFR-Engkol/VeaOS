section .text
global ksi_switch_stack

; VOID ksi_switch_stack(ULONG_PTR NewStackTop, PVOID NextFunction, PVOID Argument);
ksi_switch_stack:
    ; Ambil parameter dari stack lama (sebelum kita ganti ESP)
    mov ecx, [esp + 4]  ; Parameter 1: Alamat pucuk stack baru (NewStackTop)
    mov edx, [esp + 8]  ; Parameter 2: Fungsi C selanjutnya (NextFunction)
    mov eax, [esp + 12] ; Parameter 3: Argumen (misal: block_boot)

    mov esp, ecx
    
    ; Bersihkan EBP biar debugger tahu ini adalah ujung/awal dari stack trace
    xor ebp, ebp

    ; Siapkan argumen di stack baru untuk NextFunction
    push eax

    ; Lompat ke fungsi berikutnya (ul_system_startup)
    call edx

    ; Kalau NextFunction tiba-tiba me-return (harusnya nggak boleh), 
    ; kita kunci CPU biar nggak mengeksekusi memori sampah.
.hang:
    cli
    hlt
    jmp .hang

global ks_load_idt
ks_load_idt:
    mov eax, [esp + 4]    ; Ambil pointer &KiIdtr yang dikirim dari argumen C
    lidt [eax]            ; Load IDT Register!
    ret 4