BITS 32
GLOBAL _INT16_CALL

_INT16_CALL:
    push ebp
    mov ebp, esp
    pushad              
    
    sgdt [temp_gdtr]

    mov eax, [ebp + 8]   ; Parameter EAX (Command)
    
    mov [temp_esp_kbd], esp 
    
    cli                 
    jmp dword 0x18:pm16_kbd 

BITS 16
pm16_kbd:
    mov cx, 0x20
    mov ds, cx
    mov es, cx
    mov fs, cx
    mov gs, cx
    mov ss, cx

    mov ecx, cr0
    and ecx, 0x7FFFFFFE    
    mov cr0, ecx

    mov ecx, real_mode_kbd  
    sub ecx, 0x10000          
    push 0x1000               
    push cx                   
    retf                      

real_mode_kbd:
    mov cx, 0x1000
    mov ds, cx
    mov es, cx
    mov fs, cx
    mov gs, cx
    
    ; FIX STACK KBD: Pindah ke 0x7000
    xor ax, ax
    mov ss, ax
    mov sp, 0x2000      ; <-- UBAH SEMUA KE 0x4000
    
    sti                 
    int 0x16            ; MANGGIL BIOS KEYBOARD!
    cli                 
    
    mov edi, eax        ; Amankan hasil (AL = ASCII, AH = Scancode)

    xor ax, ax
    mov ds, ax
    lgdt [dword temp_gdtr]

    mov ecx, cr0
    or ecx, 0x80000001   
    mov cr0, ecx

    jmp dword 0x08:pm32_kbd

BITS 32
pm32_kbd:
    mov cx, 0x10
    mov ds, cx
    mov es, cx
    mov fs, cx
    mov gs, cx
    mov ss, cx

    mov esp, [temp_esp_kbd]

    ; Balikin hasil ke EAX C
    mov [esp + 28], edi 

    popad           
    pop ebp
    ret             

temp_esp_kbd dd 0

temp_gdtr:
    dw 0
    dd 0