[BITS 32]
section .text

global @ExInterlockedPopEntrySList@8
global @InterlockedPopEntrySList@4
global _ExpInterlockedPopEntrySListResume
global _ExpInterlockedPopEntrySListFault
global _ExpInterlockedPopEntrySListEnd

global @ExInterlockedPushEntrySList@12
global @InterlockedPushEntrySList@8

; =========================================================================
; PSINGLE_LIST_ENTRY __fastcall InterlockedPopEntrySList(PSLIST_HEADER ListHead)
; ECX = ListHead
; EDX = Lock (Unused di lockless S-List)
; =========================================================================

@ExInterlockedPopEntrySList@8:
@InterlockedPopEntrySList@4:

    ; Save non-volatile registers
    push ebx
    push ebp

    ; Pointer to ListHead
    mov ebp, ecx

_ExpInterlockedPopEntrySListResume:
    mov edx, [ebp + 4]          ; EDX = Sequence & Depth (32-bit high)
    mov eax, [ebp]              ; EAX = Next pointer (32-bit low)

    ; Check if list is empty (EAX == NULL)
    or eax, eax
    jz ExpInterlockedPopEntrySListEmpty ; <-- Ganti ke label non-local

    ; Copy Sequence/Depth and decrement Depth (-1)
    lea ecx, [edx - 1]

_ExpInterlockedPopEntrySListFault:
    mov ebx, [eax]              ; EBX = Next->Next (Bisa trigger Page Fault jika bad ptr)
_ExpInterlockedPopEntrySListEnd:
    lock cmpxchg8b [ebp]        ; Atomic Compare & Swap 64-bit
    jnz _ExpInterlockedPopEntrySListResume

ExpInterlockedPopEntrySListEmpty: ; <-- Ganti ke label non-local (tanpa titik)
    pop ebp
    pop ebx
    ret

; =========================================================================
; PSINGLE_LIST_ENTRY __fastcall InterlockedPushEntrySList(
;     PSLIST_HEADER ListHead, 
;     PSINGLE_LIST_ENTRY ListEntry
; )
; ECX = ListHead, EDX = ListEntry
; =========================================================================
@ExInterlockedPushEntrySList@12:
    ; ExInterlocked punya 3 argumen (Lock di stack). Pop 4 byte untuk buang parameter Lock.
    pop dword [esp]

@InterlockedPushEntrySList@8:

    ; Save non-volatile registers
    push ebx
    push ebp

    ; Setup pointers
    mov ebp, ecx                ; EBP = ListHead
    mov ebx, edx                ; EBX = ListEntry yang mau di-push

    ; Fetch current sequence and head pointer
    mov edx, [ebp + 4]
    mov eax, [ebp]

.retry_push:
    ; Set ListEntry->Next = Old Head Pointer
    mov [ebx], eax

    ; Increment Depth (+1) dan Sequence (+1) -> 0x00010001
    lea ecx, [edx + 0x00010001]

    ; Atomic exchange 64-bit:
    ; EDX:EAX vs [EBP]. Jika sama -> [EBP] = ECX:EBX. Jika beda -> EDX:EAX = [EBP]
    lock cmpxchg8b [ebp]
    jnz .retry_push

    ; Restore and return (EAX = Head Pointer lama sebelum push)
    pop ebp
    pop ebx
    ret

;
; EOF
;