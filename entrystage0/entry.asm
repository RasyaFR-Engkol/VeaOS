BITS 16
ORG 0x0600          ; MBR standar biasanya memindahkan dirinya ke 0x0600

start:
    ; 1. Matikan interupsi dan bersihkan Segment Register
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00  ; Setup stack persis di bawah tempat MBR dimuat awal
    sti

    ; 2. Relokasi MBR dari 0x7C00 (asal) ke 0x0600 (tujuan)
    ; Biar kita bisa me-load VBR (Stage 1) ke 0x7C00 tanpa menimpa diri sendiri
    mov cx, 0x0100  ; 256 words (512 byte)
    mov si, 0x7C00
    mov di, 0x0600
    rep movsw

    ; Lompat ke kode yang sudah direlokasi di 0x0600
    jmp 0x0000:relocated

relocated:
    ; DL otomatis berisi nomor drive BIOS saat booting (jangan diubah)
    
    ; 3. Cari partisi Active (Bootable) di Partition Table (mulai di 0x0600 + 446 = 0x07BE)
    mov bx, 0x07BE
.find_active:
    cmp byte [bx], 0x80     ; Cek bit bootable (0x80)
    je .active_found
    add bx, 16              ; Lompat ke entry partisi berikutnya (ukuran entry 16 byte)
    cmp bx, 0x07FE          ; Cek apakah sudah sampai akhir MBR
    jne .find_active

    ; Kalau gak nemu partisi bootable, hang.
    jmp .error

.active_found:
    ; BX sekarang menunjuk ke entry partisi yang aktif.
    ; Kita ambil LBA start dari partisi tersebut (di offset +8 dari entry)
    
    ; 4. Setup Disk Address Packet (DAP) di Stack untuk int 13h (AH=42h)
    push dword 0            ; LBA Upper 32-bit (0)
    push dword [bx + 8]     ; LBA Lower 32-bit (ambil dari Partisi, yaitu 2048)
    push word 0x0000        ; Buffer Segment (0x0000)
    push word 0x7C00        ; Buffer Offset (0x7C00) - kita load Stage 1 ke sini
    push word 1             ; Jumlah sektor yang dibaca (1 sektor VBR)
    push word 16            ; Ukuran struktur DAP (16 byte)

    ; 5. Panggil BIOS int 13h
    mov ah, 0x42
    mov si, sp              ; SI menunjuk ke DAP di stack
    int 0x13
    jc .error               ; Kalau gagal baca disk (Carry flag set), hang.

    ; 6. Lompat ke Stage 1 (Volume Boot Record FAT32)
    ; Konvensi MBR: DL tetap berisi Drive ID, DS:SI menunjuk ke entry partisi yang diload
    mov si, bx              
    jmp 0x0000:0x7C00       

.error:
    cli
    hlt
    jmp .error

; =====================================================================
; PADDING DAN PARTITION TABLE
; =====================================================================
times 446 - ($ - $$) db 0   ; Penuhi sisa ruang dengan 0 sampai batas partisi

; Entry 1 (Bootable, mulai di LBA 2048, tipe FAT32 LBA)
db 0x80                     ; Status: Active/Bootable
db 0, 0, 0                  ; CHS First (diabaikan)
db 0x0C                     ; Tipe Partisi: FAT32 LBA
db 0, 0, 0                  ; CHS Last (diabaikan)
dd 2048                     ; LBA Start (Sektor 2048)
dd 260000

; Entry 2, 3, 4 dikosongkan
times 16 * 3 db 0

; Boot Signature Wajib MBR
dw 0xAA55