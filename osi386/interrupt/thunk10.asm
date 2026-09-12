BITS 32
GLOBAL _INT10_CALL

_INT10_CALL:
    ; void _INT10_CALL(uint32_t eax_in, uint32_t ebx_in);
    push ebp
    mov ebp, esp
    pushad              

    sgdt [temp_gdtr]
    
    ; Ambil argumen dari C
    mov eax, [ebp + 8]   ; Parameter EAX
    mov ebx, [ebp + 12]  ; Parameter EBX
    
    ; Save ESP
    mov [temp_esp_vga], esp 
    
    cli                 
    jmp dword 0x18:pm16_vga 

BITS 16
pm16_vga:
    ; Pakai CX buat nge-set segment, biarin EAX & EBX utuh bawa data lu
    mov cx, 0x20
    mov ds, cx
    mov es, cx
    mov fs, cx
    mov gs, cx
    mov ss, cx

    mov ecx, cr0
    and ecx, 0x7FFFFFFE   
    mov cr0, ecx

    ; The RETF Trick buat nembus LLD
    mov ecx, real_mode_vga  
    sub ecx, 0x10000          
    push 0x1000               
    push cx                   
    retf                      

real_mode_vga:
    mov cx, 0x1000
    mov ds, cx
    mov es, cx
    mov fs, cx
    mov gs, cx
    
    ; FIX STACK: Pindah ke daerah yang BENAR-BENAR AMAN
    ; Jauhkan dari GDT yang ada di 0x6E90!
    xor cx, cx
    mov ss, cx
    mov sp, 0x2000      ; <-- UBAH SEMUA KE 0x4000
    
    sti                 
    int 0x10            ; Sekarang BIOS punya 8KB area stack bebas
    cli
    
    ; SIMPAN RETURN VALUE!
    mov edi, eax       

    xor ax, ax
    mov ds, ax
    lgdt [dword temp_gdtr] 

    mov ecx, cr0
    or ecx, 0x80000001    
    mov cr0, ecx

    jmp dword 0x08:pm32_vga

BITS 32
pm32_vga:
    mov cx, 0x10
    mov ds, cx
    mov es, cx
    mov fs, cx
    mov gs, cx
    mov ss, cx

    mov esp, [temp_esp_vga]

    ; HACK JENIUS: EDI berisi status BIOS (EAX lama).
    ; Layout 'pushad' naruh slot EAX di [ESP + 28].
    ; Kita timpa memori itu, jadi pas 'popad', return valuenya langsung pindah ke EAX C!
    mov [esp + 28], edi 

    popad           
    pop ebp
    ret             

temp_esp_vga dd 0

temp_gdtr:
    dw 0
    dd 0