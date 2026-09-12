BITS 16
ORG 0x8000

%define BPB_SectorsPerCluster 0x7C0D
%define BPB_ReservedSectors   0x7C0E
%define BPB_FatCount          0x7C10
%define BPB_HiddenSectors     0x7C1C
%define BPB_SectorsPerFat32   0x7C24
%define BPB_RootCluster       0x7C2C

extended_boot_start:
    ; -----------------------------------------------------------------
    ; 1. INISIALISASI SEGMENT & REGISTERS
    ; -----------------------------------------------------------------
    xor ax, ax
    mov ds, ax
    mov es, ax
    cld 
    
    ; Print pesan awal
    mov si, msg_loading
    call print_string

    ; -----------------------------------------------------------------
    ; 2. HITUNG FAT START LBA & DATA SECTOR LBA
    ; -----------------------------------------------------------------
    ; FatStartLBA = HiddenSectors + ReservedSectors
    mov eax, [BPB_HiddenSectors]
    xor ebx, ebx
    mov bx, [BPB_ReservedSectors]
    add eax, ebx
    mov [FatStartLBA], eax

    ; DataSectorLBA = FatStartLBA + (FatCount * SectorsPerFat32)
    xor ecx, ecx
    mov cl, [BPB_FatCount]
    mov ebx, [BPB_SectorsPerFat32]
    imul ebx, ecx
    add eax, ebx
    mov [DataSectorLBA], eax

    ; -----------------------------------------------------------------
    ; 3. LOAD ROOT DIRECTORY TO 0x0900:0000 (0x9000)
    ; -----------------------------------------------------------------
    mov eax, [BPB_RootCluster]
    call load_cluster_to_buffer

    ; -----------------------------------------------------------------
    ; 4. PARSING ROOT DIRECTORY UNTUK MENCARI "OSI386   "
    ; -----------------------------------------------------------------
    mov ax, 0x0900
    mov es, ax
    xor di, di
    mov cx, 128        ; Cari di 128 entry pertama (1 cluster root dir)

.search_loop:
    push cx
    mov cx, 11
    mov si, filename
    push di
    cld 
    rep cmpsb
    pop di
    je .file_found
    
    pop cx
    add di, 32
    loop .search_loop

    mov si, msg_error
    call print_string
    jmp halt_cpu

.file_found:
    pop cx
    ; -----------------------------------------------------------------
    ; 5. AMBIL NOMOR FIRST CLUSTER FILE OSI386
    ; -----------------------------------------------------------------
    mov dx, [es:di + 0x14] ; Cluster High (2 bytes)
    shl edx, 16
    mov dx, [es:di + 0x1A] ; Cluster Low (2 bytes)
    mov [FileCluster], edx

    ; -----------------------------------------------------------------
    ; 6. LOAD FILE OSI386 DENGAN TRAVERSAL FAT TABLE (SELESAIKAN FRAGMENTASI)
    ; -----------------------------------------------------------------
    mov ax, 0x1000
    mov es, ax
    xor bx, bx        ; ES:BX = 0x1000:0000 (Alamat Fisik 0x10000)

    mov eax, [FileCluster]

.load_file_loop:
    ; Cek apakah EAX adalah Marker EOF (End of File >= 0x0FFFFFF8) atau Invalid Cluster (< 2)
    cmp eax, 0x0FFFFFF8
    jae .load_finished

    cmp eax, 2
    jb .load_finished

    ; Baca cluster saat ini ke ES:BX
    push eax
    call load_cluster_to_es_bx
    pop eax

    ; Dapatkan nomor cluster BERIKUTNYA dari FAT Table
    call get_next_cluster ; Mengembalikan Next Cluster ID di EAX

    ; Geser Segment ES sebesar (SectorsPerCluster * 512) / 16
    xor dx, dx
    mov dl, [BPB_SectorsPerCluster]
    shl dx, 5         ; Multiply 32 (Sectors * 512 / 16)
    mov cx, es
    add cx, dx
    mov es, cx        ; ES bertambah tanpa menyentuh BX!

    jmp .load_file_loop

.load_finished:
    ; -----------------------------------------------------------------
    ; 7. PERSIAPAN JUMP KE LOADER (OSI386)
    ; -----------------------------------------------------------------
    mov si, msg_ok
    call print_string
    
    mov ax, 0x1000
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    ; Jump ke Entry Point OSI386 (0x1000:0x0000)
    jmp 0x1000:0x0000

; =====================================================================
; FUNGSI BANTUAN DISK & FAT PARSER
; =====================================================================

; Input: EAX = Cluster ID saat ini
; Output: EAX = Next Cluster ID dari FAT Table
get_next_cluster:
    push es
    push bx
    push ecx
    push edx

    mov ecx, eax        ; Simpan Cluster ID

    ; Sector Offset dalam FAT = Cluster / 128 (karena 1 sektor 512 byte berisi 128 entry @ 4 byte)
    shr eax, 7
    add eax, [FatStartLBA] ; EAX = LBA Sektor FAT Table yang berisi cluster entry ini

    ; Read 1 sektor FAT ke buffer temporary 0x0900:0000
    mov bx, 0x0900
    mov es, bx
    xor bx, bx
    call read_single_sector

    ; Byte Offset dalam Sektor = (Cluster % 128) * 4
    and ecx, 0x7F
    shl ecx, 2          ; ECX = Byte offset di dalam buffer sektor

    ; Read 32-bit entry dari FAT
    mov eax, [es:ecx]
    and eax, 0x0FFFFFFF ; FAT32 cuma pakai 28-bit (4 bit atas reserved)

    pop edx
    pop ecx
    pop bx
    pop es
    ret

load_cluster_to_buffer:
    pusha
    mov bx, 0x0900
    mov es, bx
    xor bx, bx
    jmp do_load_cluster

load_cluster_to_es_bx:
    pusha

do_load_cluster:
    sub eax, 2
    xor ecx, ecx
    mov cl, [BPB_SectorsPerCluster]
    imul eax, ecx
    add eax, [DataSectorLBA]

    ; Setup DAP (Disk Address Packet) untuk INT 13h AH=42h
    push dword 0        ; LBA High (32-bit) = 0
    push eax            ; LBA Low (32-bit)
    push es             ; Target Segment
    push bx             ; Target Offset
    push cx             ; Number of sectors (SectorsPerCluster)
    push word 16        ; DAP Size (16 bytes)

    mov ah, 0x42
    mov dl, 0x80
    mov si, sp
    int 0x13
    jc disk_error       ; FIX: Menggunakan global label disk_error

    add sp, 16
    popa
    ret

read_single_sector:
    pusha
    push dword 0
    push eax
    push es
    push bx
    push word 1         ; Cuma baca 1 sektor FAT
    push word 16

    mov ah, 0x42
    mov dl, 0x80
    mov si, sp
    int 0x13
    jc disk_error       ; FIX: Menggunakan global label disk_error

    add sp, 16
    popa
    ret

; FIX: Dijadikan global label (tanpa titik di depan)
disk_error:
    mov si, msg_disk_err
    call print_string
    jmp halt_cpu

print_string:
    mov ah, 0x0E
.loop:
    lodsb
    or al, al
    jz .done
    int 0x10
    jmp .loop
.done:
    ret

halt_cpu:
    cli
    hlt
    jmp halt_cpu

; =====================================================================
; VARIABEL & DATA
; =====================================================================
filename     db "OSI386     "   
msg_loading  db "Mencari OSI386... ", 0
msg_ok       db "Ketemu! Booting Loader...", 13, 10, 0
msg_error    db "ERROR: OSI386 tidak ditemukan!", 13, 10, 0
msg_disk_err db "ERROR: Gagal membaca disk (INT 13h)!", 13, 10, 0

FatStartLBA   dd 0
DataSectorLBA dd 0
FileCluster   dd 0