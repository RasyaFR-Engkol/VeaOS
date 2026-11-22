[BITS 32]
section .text

extern KspSwitchToUserCallback

global KsiCallUserMode
KsiCallUserMode:
    ; SavedTrapFrame, SavedCallbackStack, SavedInitialStack, SavedEsp0
    sub esp, 16

    push ebx
    push esi
    push edi
    push ebp

    push esp
    call KspSwitchToUserCallback

global KsiCallbackReturn
KsiCallbackReturn:
    mov eax, [esp + 8]              ; Return value Status untuk KiCallUserMode
    mov ecx, [esp + 4]              ; Frame (PKCALLOUT_FRAME)

    mov esp, ecx                    ; Pindah ESP kembali ke KCALLOUT_FRAME
    pop ebp
    pop edi
    pop esi
    pop ebx
    add esp, 16                     ; Lewati 4 field state kernel yang sudah direstore C
    ret 8                           ; Pop ReturnAddress + bersihkan 2 argumen KiCallUserMode