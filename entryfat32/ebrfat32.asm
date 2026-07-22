BITS 16
ORG 0x8000

; =====================================================================
; POINTER KE BPB (Berada di VBR Sektor 2048 / 0x7C00)
; Kita ambil variabel langsung dari RAM biar gak makan tempat.
; =====================================================================
%define BPB_SectorsPerCluster 0x7C0D
%define BPB_ReservedSectors   0x7C0E
%define BPB_FatCount          0x7C10
%define BPB_HiddenSectors     0x7C1C
%define BPB_SectorsPerFat32   0x7C24
%define BPB_RootCluster       0x7C2C

extended_boot_start:
    ; Print pesan awal
    mov si, msg_loading
    call print_string

    ; -----------------------------------------------------------------
    ; 1. HITUNG LBA AWAL DATA REGION
    ; Rumus: HiddenSectors + ReservedSectors + (FatCount * SectorsPerFat32)
    ; -----------------------------------------------------------------
    mov eax, [BPB_HiddenSectors]
    xor ebx, ebx
    mov bx, [BPB_ReservedSectors]
    add eax, ebx

    xor ecx, ecx
    mov cl, [BPB_FatCount]
    mov ebx, [BPB_SectorsPerFat32]
    imul ebx, ecx
    add eax, ebx
    mov [DataSectorLBA], eax  ; Simpan hasil ke memori

    ; -----------------------------------------------------------------
    ; 2. LOAD SEKTOR ROOT DIRECTORY KE BUFFER 0x0900:0000 (0x9000)
    ; -----------------------------------------------------------------
    mov eax, [BPB_RootCluster]
    call load_cluster_to_buffer

    ; -----------------------------------------------------------------
    ; 3. PARSING ROOT DIRECTORY UNTUK MENCARI "OSI386     "
    ; -----------------------------------------------------------------
    mov ax, 0x0900
    mov es, ax
    xor di, di       ; ES:DI = 0x0900:0000 (Buffer Root Directory)
    mov cx, 16       ; Cek 16 entri pertama aja (karena ini file pertama)

.search_loop:
    push cx
    mov cx, 11               ; Panjang nama file (8 nama + 3 ekstensi)
    mov si, filename         ; DS:SI = "OSI386     "
    push di
    rep cmpsb                ; Bandingkan string memori
    pop di
    je .file_found           ; Kalau cocok, lompat!
    
    pop cx
    add di, 32               ; Geser 32 byte ke entri file berikutnya
    loop .search_loop

    ; Kalau file tidak ditemukan
    mov si, msg_error
    call print_string
    jmp halt_cpu

.file_found:
    pop cx
    ; -----------------------------------------------------------------
    ; 4. AMBIL NOMOR CLUSTER FILE OSI386
    ; -----------------------------------------------------------------
    ; Di struktur direktori FAT32, Cluster High di offset 0x14, Low di 0x1A
    mov dx, [es:di + 0x14]
    shl edx, 16
    mov dx, [es:di + 0x1A]
    mov [FileCluster], edx

    ; -----------------------------------------------------------------
    ; 5. LOAD FILE OSI386 KE 0x1000:0000 (Alamat Fisik 0x10000)
    ; -----------------------------------------------------------------
    mov ax, 0x1000
    mov es, ax
    xor bx, bx       ; ES:BX = 0x1000:0000 (Tujuan File)

    mov eax, [FileCluster]
    mov cx, 32

.load_file_loop:
    call load_cluster_to_es_bx
    inc eax          ; Karena file pertama di disk kosong, cluster PASTI berurutan
    
    ; Geser pointer memori ES:BX untuk nampung cluster berikutnya
    xor dx, dx
    mov dl, [BPB_SectorsPerCluster]
    shl dx, 9        ; Kalikan dengan 512 (Bytes per sector)
    add bx, dx
    loop .load_file_loop

    ; -----------------------------------------------------------------
    ; 6. PERSIAPAN JUMP KE LOADER (OSI386)
    ; -----------------------------------------------------------------
    mov si, msg_ok
    call print_string
    
    ; Samakan Segment Data dengan Segment Kode Loader
    mov ax, 0x1000
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    ; Far Jump ke Entry Point OSI386 (0x1000:0000)
    jmp 0x1000:0x0000

; =====================================================================
; FUNGSI BANTUAN DISK (LBA TO CHS & INT 13H)
; =====================================================================

; Load ke buffer sementara Root Dir (0x9000)
load_cluster_to_buffer:
    pusha
    mov bx, 0x0900
    mov es, bx
    xor bx, bx
    jmp do_load_cluster

; Load langsung ke alamat ES:BX yang ditentukan
load_cluster_to_es_bx:
    pusha

do_load_cluster:
    ; Rumus LBA Cluster: LBA = DataRegionLBA + ((Cluster - 2) * SectorsPerCluster)
    sub eax, 2
    xor ecx, ecx
    mov cl, [BPB_SectorsPerCluster]
    imul eax, ecx
    add eax, [DataSectorLBA]

    ; Setup Disk Address Packet (DAP) di Stack
    push dword 0        ; Upper 32-bit LBA (0)
    push eax            ; Lower 32-bit LBA
    push es             ; Target Segment
    push bx             ; Target Offset
    push cx             ; Jumlah sektor yang dibaca (SectorsPerCluster)
    push word 16        ; Ukuran DAP

    mov ah, 0x42
    mov dl, 0x80        ; Fix harddisk ID
    mov si, sp
    int 0x13
    add sp, 16          ; Clean up stack
    
    popa
    ret

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
; Nama file di FAT32 WAJIB 11 byte. (8 Byte Nama + 3 Byte Ekstensi tanpa titik)
; Kalau nama kurang dari 8, pad pakai spasi.
filename      db "OSI386     "   

msg_loading   db "Mencari OSI386... ", 0
msg_ok        db "Ketemu! Booting Loader...", 13, 10, 0
msg_error     db "ERROR: OSI386 tidak ada!", 0

DataSectorLBA dd 0
FileCluster   dd 0