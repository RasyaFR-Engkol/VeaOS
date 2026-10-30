BITS 32

;
; == X86 INTRIN for BOOTLOADER == 
;

GLOBAL LdrpReloadSegments
LdrpReloadSegments:
    mov eax, [esp + 4]          ; Ambil pointer Gdtr
    lgdt [eax]                  ; Load GDT Higher-Half baru

    ; Far jump untuk me-reload CS ke KGDT32_R0_CODE (0x08)
    jmp 0x08:.reload_cs

.reload_cs:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax

    mov ax, 0x30
    mov fs, ax

    xor ax, ax
    mov gs, ax

    mov ax, [esp + 8]
    ltr ax

    ret 8