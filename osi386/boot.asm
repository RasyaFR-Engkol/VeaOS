SECTION .text
BITS 16
global _start
extern main

_start:
    cli

    ; Setup DS = 0 biar CPU bisa baca memori pakai absolute address > 0xFFFF
    xor ax, ax
    mov ds, ax

    in al, 0x92
    or al, 2
    out 0x92, al

    ; PAKSA 32-bit relocation pakai dword
    lgdt [dword gdt_descriptor]

    mov eax, cr0
    or eax, 1        ; pake eax lebih aman dari al
    mov cr0, eax

    ; PAKSA 32-bit offset jump
    jmp dword 0x08:init_32bit

; ==========================================
; GLOBAL DESCRIPTOR TABLE (GDT)
; ==========================================
align 4
gdt_start:

gdt_null:           ; Entry 1: Null
    dd 0x0
    dd 0x0

gdt_code:           ; Entry 2: Code 32-bit
    dw 0xFFFF       
    dw 0x0000       
    db 0x00         
    db 10011010b    
    db 11001111b    
    db 0x00         

gdt_data:           ; Entry 3: Data 32-bit
    dw 0xFFFF       
    dw 0x0000       
    db 0x00         
    db 10010010b    
    db 11001111b    
    db 0x00         

gdt_code16:         ; Entry 4: Code 16-bit
    dw 0xFFFF       
    dw 0x0000       
    db 0x00         
    db 10011010b    
    db 00001111b    
    db 0x00         

gdt_data16:         ; Entry 5: Data 16-bit
    dw 0xFFFF       
    dw 0x0000       
    db 0x00         
    db 10010010b    
    db 00001111b    
    db 0x00

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1  
    dd gdt_start                

BITS 32
init_32bit:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov esp, 0x90000
    call main

    jmp $