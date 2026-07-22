BITS 16
ORG 0x7C00

start:
    jmp short sector_start
    nop

; =====================================================================
; FAT32 BIOS PARAMETER BLOCK (BPB) HEADER
; (Biarkan sama persis seperti yang lo tulis)
; =====================================================================
OEM_Name            db "VeaOS   "     
BytesPerSector      dw 512            
SectorsPerCluster   db 2              
ReservedSectors     dw 32             
FatCount            db 2              
RootEntryCount      dw 0              
TotalSectors16      dw 0              
MediaType           db 0xF8           
SectorsPerFat16     dw 0              
SectorsPerTrack     dw 63             
HeadCount           dw 255            
HiddenSectors       dd 2048           
TotalSectors32      dd 260000         
SectorsPerFat32     dd 2000           
ExtendedFlags       dw 0
FSVersion           dw 0
RootCluster         dd 2              
FSInfoSector        dw 1              
BackupBootSector    dw 6              
times 12 db 0                         
DriveNumber         db 0x80           
Reserved1           db 0
BootSignature       db 0x29           
VolumeID            dd 0xDEADC0DE     
VolumeLabel         db "VELOS_BOOT "  
FileSystemType      db "FAT32   "     

; =====================================================================
; STUB VBR (SEKTOR 0 DARI PARTISI)
; =====================================================================
sector_start:
    ; 1. Setup Environment yang Aman
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00      ; Stack di bawah VBR
    sti

    ; (HAPUS BARIS INI: mov [DriveNumber], dl) 
    ; Kita gak usah nyimpen DL sisaan MBR karena rawan korup.

    ; 2. Load Extended Boot Code (Reserved Sectors) ke 0x7E00
    mov eax, [HiddenSectors]
    inc eax

    ; Setup Disk Address Packet (DAP) di Stack untuk int 13h
    push dword 0        ; LBA Upper 32-bit
    push eax            ; LBA Lower 32-bit (Sektor 2049)
    push word 0x0000    ; Buffer Segment
    push word 0x7E00    ; Buffer Offset (Load ke 0x7E00)
    push word 31        ; Jumlah sektor yang dibaca (ReservedSectors - 1)
    push word 16        ; Ukuran DAP

    ; 3. Panggil BIOS
    mov ah, 0x42
    mov dl, [DriveNumber] ; <--- TAMBAHKAN INI: Ambil nilai murni 0x80 dari header BPB kita!
    mov si, sp            ; SI menunjuk ke DAP di stack
    int 0x13
    jc .error             ; Kalau gagal baca, lompat ke error

    ; Bersihkan stack dari DAP (opsional tapi best practice biar stack bersih)
    add sp, 16

    ; 4. Lompat ke Extended Boot Code yang baru saja di-load!
    jmp 0x0000:0x8000

.error:
    ; Print huruf 'E' kalau disk error
    mov ah, 0x0E
    mov al, 'E'
    int 0x10
    cli
    hlt
    jmp $

; Padding sektor 0 agar pas 512 byte dan diakhiri boot signature
times 510 - ($ - $$) db 0
dw 0xAA55   

times 512 db 0