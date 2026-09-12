BITS 32
GLOBAL _INT10_CALL_BUFFER

_INT10_CALL_BUFFER:
    push ebp
    mov ebp, esp
    pushad

    sgdt [temp_gdtr]

    ; Ambil parameter dari stack C
    mov eax, [ebp + 8]   ; Parameter EAX
    mov ebx, [ebp + 12]  ; Parameter EBX
    mov ecx, [ebp + 16]  ; Parameter ECX (Buat nomor mode VBE)
    mov edx, [ebp + 20]  ; Parameter ES Segment
    mov edi, [ebp + 24]  ; Parameter DI Offset

    mov [temp_esp_vbe], esp

    cli
    jmp dword 0x18:pm16_vbe

BITS 16
pm16_vbe:
    ; Titip AX ke BP biar nggak ilang pas ganti segment
    mov bp, ax
    mov ax, 0x20
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov ax, bp

    mov esi, cr0
    and esi, 0x7FFFFFFE  ; Matikan bit 31 (PG) dan bit 0 (PE) sekaligus
    mov cr0, esi

    mov esi, real_mode_vbe
    sub esi, 0x10000
    push 0x1000
    push si
    retf

real_mode_vbe:
    ; Titip AX ke BP lagi
    mov bp, ax
    mov ax, 0x1000
    mov ds, ax
    mov fs, ax
    mov gs, ax
    
    ; FIX STACK: Pindah ke daerah aman (0x0000:0x7000)
    xor ax, ax
    mov ss, ax
    mov sp, 0x2000      ; <-- UBAH SEMUA KE 0x4000
    
    ; Balikin AX
    mov ax, bp

    ; SETUP ES DARI DX
    mov es, dx

    sti
    int 0x10
    cli

    mov edx, eax

    xor ax, ax
    mov ds, ax
    lgdt [dword temp_gdtr]

    mov eax, cr0
    or eax, 0x80000001   ; Nyalakan bit 31 (PG) dan bit 0 (PE) sekaligus
    mov cr0, eax

    jmp dword 0x08:pm32_vbe

BITS 32
pm32_vbe:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov esp, [temp_esp_vbe]

    ; Balikin status ke register EAX C
    mov [esp + 28], edx

    popad
    pop ebp
    ret

temp_esp_vbe dd 0

temp_gdtr:
    dw 0
    dd 0