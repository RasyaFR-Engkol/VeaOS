BITS 32
GLOBAL _INT15_E820_CALL

_INT15_E820_CALL:
    push ebp
    mov ebp, esp
    pushad

    sgdt [temp_gdtr_e820]

    mov esi, [ebp + 8]
    mov edx, [ebp + 12]
    mov edi, [ebp + 16]

    mov ebx, [esi]
    mov [temp_ebx_ptr], esi

    mov [temp_esp_e820], esp

    cli
    jmp dword 0x18:pm16_e820

BITS 16
pm16_e820:
    mov bp, ax
    mov ax, 0x20
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov ax, bp

    mov esi, cr0
    and esi, ~1
    mov cr0, esi

    mov esi, real_mode_e820
    sub esi, 0x10000
    push 0x1000
    push si
    retf

real_mode_e820:
    ; Setup segment & stack aman
    mov bp, ax
    mov ax, 0x1000
    mov ds, ax
    mov fs, ax
    mov gs, ax
    xor ax, ax
    mov ss, ax
    mov sp, 0x4000
    mov ax, bp

    mov es, dx

    mov eax, 0xE820
    mov edx, 0x534D4150
    mov ecx, 24  ; Minta 24 byte (standar ACPI 3.0)

    sti
    int 0x15
    cli

    pushf
    pop dx

    xor ax, ax
    mov ds, ax
    lgdt [dword temp_gdtr_e820]

    mov eax, cr0
    or eax, 1
    mov cr0, eax

    jmp dword 0x08:pm32_e820

BITS 32
pm32_e820:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov esp, [temp_esp_e820]

    ; Tulis balik nilai EBX terbaru ke pointer C
    mov esi, [temp_ebx_ptr]
    mov [esi], ebx

    ; Cek Carry Flag dari DX (bit ke-0). 
    ; Kalau CF=1 (error/habis), return 0. Kalau CF=0 (sukses), return 1.
    test dx, 1
    jnz .done_failed

.done_success:
    mov dword [esp + 28], 1  ; Timpa slot EAX di pushad dengan 1
    jmp .exit
.done_failed:
    mov dword [esp + 28], 0  ; Timpa slot EAX di pushad dengan 0

.exit:
    popad
    pop ebp
    ret

; Variabel lokalan thunk ini
temp_esp_e820 dd 0
temp_ebx_ptr  dd 0
temp_gdtr_e820:
    dw 0
    dd 0