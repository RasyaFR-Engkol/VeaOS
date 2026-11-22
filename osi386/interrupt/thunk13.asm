BITS 32
GLOBAL _LBA_READ

_LBA_READ:
    push ebp
    mov ebp, esp
    pushad

    sgdt [temp_gdtr]

    ; Simpan CR0 asli (aman baik sebelum maupun sesudah paging aktif)
    mov eax, cr0
    mov [temp_cr0], eax

    ; Ambil parameter cdecl
    mov edx, [ebp + 8]      ; dl = drive ID (0x80)
    mov esi, [ebp + 12]     ; esi = linear address &dap

    ; Hitung seg:off dinamis untuk DAP -> CX (segment), SI (offset)
    mov eax, esi
    shr eax, 4
    mov cx, ax
    and esi, 0x000F

    mov [temp_esp], esp

    cli

    ; Turun ke 16-bit Protected Mode (gdt_code16 = 0x18)
    jmp dword 0x18:pm16_step1

BITS 16
pm16_step1:
    mov ax, 0x20            ; gdt_data16 = 0x20
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Matikan PE (dan PG jika aktif) untuk masuk Real Mode
    mov eax, cr0
    and eax, 0x7FFFFFFE
    mov cr0, eax

    ; Far jump ke Real Mode dengan seg:off dinamis (anti >64KB truncation)
    mov ebx, real_mode_entry ; R_386_32 -> LLD aman
    mov eax, ebx
    shr eax, 4
    push ax                  ; CS
    and bx, 0x000F
    push bx                  ; IP
    retf

real_mode_entry:
    ; Set DS ke segment DAP
    mov ds, cx
    mov es, cx
    mov fs, cx
    mov gs, cx

    ; Stack Real Mode aman di 0x0000:0x7C00 (tepat di bawah MBR)
    xor ax, ax
    mov ss, ax
    mov sp, 0x7C00

    ; Panggil BIOS Extended Read (DL = drive ID, DS:SI = DAP)
    sti
    mov ah, 0x42
    int 0x13
    cli

    ; Tangkap status error BIOS (0 = sukses, AH = kode error jika CF=1)
    mov bl, 0
    jnc .read_ok
    mov bl, ah
.read_ok:
    ; Baca temp_cr0 lewat DS:SI dinamis (offset < 16, aman dari limit 64KB)
    mov eax, temp_cr0
    mov edx, eax
    shr edx, 4
    mov ds, dx
    and ax, 0x000F
    mov si, ax
    mov ecx, [si]

    ; Load temp_gdtr lewat DS:DI dinamis
    mov eax, temp_gdtr
    mov edx, eax
    shr edx, 4
    mov ds, dx
    and ax, 0x000F
    mov di, ax
    lgdt [di]

    ; Restore CR0 asli
    mov cr0, ecx

    ; Kembali ke 32-bit Protected Mode (gdt_code = 0x08)
    jmp dword 0x08:pm32_step2

BITS 32
pm32_step2:
    mov ax, 0x10            ; gdt_data = 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov esp, [temp_esp]

    ; Simpan status BIOS sebelum popad menimpa EBX
    movzx eax, bl
    mov [temp_status], eax

    popad
    pop ebp

    mov eax, [temp_status]
    ret

temp_esp    dd 0
temp_cr0    dd 0
temp_status dd 0

temp_gdtr:
    dw 0
    dd 0