SECTION .text
BITS 16
global _start
extern main

_start:
    cli

    mov ax, 0x1000
    mov ds, ax

    ; Enable A20 Gate (Fast A20)
    in al, 0x92
    or al, 2
    out 0x92, al

    ; Ambil alamat absolut gdt_descriptor (misal: 0x10050)
    ; lalu kurangi 0x10000 untuk mendapatkan offset lokal (misal: 0x0050)
    mov ebx, gdt_descriptor
    sub ebx, 0x10000

    ; Load GDT menggunakan register BX (16-bit)
    ; DS:BX = 0x1000:0x0050 -> Linear 0x10050. 
    ; Nilai BX dijamin < 0xFFFF, jadi aman dari GPF!
    lgdt [bx]

    ; Aktifkan Protected Mode
    mov eax, cr0
    or eax, 1
    mov cr0, eax

    ; Far jump ke 32-bit (Otomatis limit berpindah jadi 4GB)
    jmp dword 0x08:init_32bit

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

gdt_reserved:       ; Entry 5: Reserved / User (0x28)
    dq 0            ; Slot kosong 8-byte

gdt_pcr:            ; Entry 6: KPCR / FS Register (0x30)
    dq 0            ; Nanti bakal di-overwrite oleh LdrSetFsSegmentBase()

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