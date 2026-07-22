section .text
global _start
extern ks_system_startup ; Fungsi C lu

_start:
    mov eax, [esp + 4]
    push eax

    ; Panggil fungsi startup C lu!
    call ks_system_startup

.hang:
    cli       ; Matikan interrupt
    hlt       ; Tidurkan CPU
    jmp .hang ; Loop abadi