BITS 32

GLOBAL KiLoadIdt
GLOBAL KiLoadIdtPointer

EXTERN KiIdt
EXTERN KsIdt32

KiLoadIdt:
    ; 1. Set Limit: (256 descriptor * 8 byte) - 1 = 2047 (0x07FF)
    mov word [KsIdt32], 0x07FF

    ; 2. Set Base Address: Alamat memori dari array KiIdt
    mov dword [KsIdt32 + 2], KiIdt

    ; 3. Load Interrupt Descriptor Table Register
    lidt [KsIdt32]
    ret

KiLoadIdtPointer:
    mov eax, [esp + 4]      ; Ambil pointer PKIDTR32 dari stack
    lidt [eax]             ; Load IDT dari lokasi pointer
    ret