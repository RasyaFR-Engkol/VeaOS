BITS 32
GLOBAL _LBA_READ

_LBA_READ:
    ; 1. SETUP STACK 32-BIT ALA C (cdecl)
    push ebp
    mov ebp, esp
    pushad              

    sgdt [temp_gdtr]

    ; 2. AMBIL PARAMETER (Simpan langsung di register, bukan memori!)
    mov edx, [ebp + 8]   ; dl = drive ID
    mov esi, [ebp + 12]  ; esi = DAP pointer
    
    ; Langsung ubah ESI jadi offset Real Mode (kurangi base 0x10000)
    sub esi, 0x10000

    ; WAJIB SAVE ESP: (Pake memory 32-bit gapapa, LLD aman kalau 32-bit)
    mov [temp_esp], esp 

    cli                 

    ; 3. TURUN KE 16-BIT PROTECTED MODE
    jmp dword 0x18:pm16_step1 

BITS 16
pm16_step1:
    mov ax, 0x20
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov eax, cr0
    and eax, ~1     
    mov cr0, eax

    ; ========================================================
    ; 4. FAR JUMP KE REAL MODE (THE RETF TRICK)
    ; Bypass error R_386_16 LLD dengan menghitung offset secara dinamik
    ; ========================================================
    mov ebx, real_mode_entry   ; Pake register 32-bit = LLD seneng (R_386_32)
    sub ebx, 0x10000           ; Ubah alamat absolute jadi offset 16-bit
    push 0x1000                ; Push CS (Segment 0x1000)
    push bx                    ; Push IP (Offset)
    retf                       ; CPU nge-pop IP dan CS -> BOOM, Far Jump!

real_mode_entry:
    mov ax, 0x1000
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    xor ax, ax
    mov ss, ax
    mov sp, 0x2000      ; <-- UBAH SEMUA KE 0x4000
    
    ; 6. LOGIKA LBA READ
    ; Kerennya: DL dan SI masih utuh bawaan dari mode 32-bit tadi!
    ; Gak perlu ngambil dari memori temp_dl/temp_si lagi.
    sti                 
    mov ah, 0x42        
    int 0x13
    cli               

    xor ax, ax
    mov ds, ax
    lgdt [dword temp_gdtr]  

    ; 7. NAIK GIGI LAGI KE 32-BIT PROTECTED MODE
    mov eax, cr0
    or eax, 1       
    mov cr0, eax

    ; 8. FAR JUMP KE 32-BIT CODE (Paksa 32-bit offset)
    jmp dword 0x08:pm32_step2

BITS 32
pm32_step2:
    ; 9. RESTORE SEGMENT 32-BIT
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; 10. RESTORE STACK 32-BIT!
    mov esp, [temp_esp]

    ; 11. RESTORE STATE 32-BIT C
    popad           
    pop ebp
    
    ret             

; Variabel sementara (Sisa ESP doang, karena 32-bit relocation aman)
temp_esp dd 0

temp_gdtr:
    dw 0
    dd 0