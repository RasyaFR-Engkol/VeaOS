[BITS 32]

GLOBAL LdrInitializeThunk
GLOBAL KsiUserApcDispatcher
GLOBAL KsiUserExceptionDispatcher
GLOBAL KsiUserCallbackDispatcher
GLOBAL KsiRaiseUserExceptionDispatcher
EXTERN LdrpInit
EXTERN VzContinue
EXTERN VzCallbackReturn

SECTION .code

LdrInitializeThunk:
    XOR ebp, ebp
    JMP LdrpInit

ALIGN 16
KsiUserApcDispatcher:
    CLD
    POP eax                     ; Ambil NormalRoutine dari [NewEsp + 0]
                                ; Sekarang [esp+0]=NormalContext, [esp+4]=SysArg1, [esp+8]=SysArg2
    CALL eax                    ; Panggil NormalRoutine(NormalContext, SysArg1, SysArg2)

    ; Jika NormalRoutine adalah User APC biasa yang return (bukan LdrInitializeThunk),
    ; konvensi VEAPI (__stdcall) sudah membersihkan 3 argumen (12 byte), sehingga ESP
    ; kini menunjuk tepat ke UserTrapFrame (NewEsp + 16).
    MOV ecx, esp                ; ECX = &UserTrapFrame
    PUSH DWORD 1                ; TestAlert = TRUE
    PUSH ecx                    ; ContextFrame = &UserTrapFrame
    CALL VzContinue

    ; Fallback pengaman jika VzContinue gagal
    INT 3

KsiUserExceptionDispatcher:
    XOR eax, eax
    RET

KsiUserCallbackDispatcher:
    ; Stack saat masuk dari kernel:
    ; [esp + 0]  = Fake return address (0)
    ; [esp + 4]  = RoutineIndex (0 = VEAPI_CALLBACK_WNDPROC)
    ; [esp + 8]  = InputBuffer (pointer ke VEAPI_WNDPROC_CALLBACK_ARGS)
    ; [esp + 12] = InputLength
    mov eax, [esp + 4]
    cmp eax, 0
    jne .unsupported

    mov ecx, [esp + 8]          ; ecx = PVEAPI_WNDPROC_CALLBACK_ARGS
    mov edx, [ecx + 16]         ; edx = lParam mentah

    ; Fixup CREATESTRUCTA jika Msg == WM_CREATE (0x0001) dan lParam berupa offset (< 0x1000)
    cmp dword [ecx + 8], 1      ; Pastikan nilai WM_CREATE di header kamu == 1
    jne .push_args
    test edx, edx
    jz .push_args
    cmp edx, 0x1000
    jae .push_args

    add edx, ecx                ; edx = pointer CREATESTRUCTA di user stack

    mov eax, [edx + 0x24]       ; Cs->lpszName (offset dari awal Cs)
    test eax, eax
    jz .fix_class
    add [edx + 0x24], edx       ; Jadikan pointer Ring 3 valid

.fix_class:
    mov eax, [edx + 0x28]       ; Cs->lpszClass (offset dari awal Cs)
    test eax, eax
    jz .push_args
    add [edx + 0x28], edx       ; Jadikan pointer Ring 3 valid

.push_args:
    push edx                    ; lParam (sudah jadi pointer Cs jika WM_CREATE)
    push dword [ecx + 12]       ; wParam
    push dword [ecx + 8]        ; Msg
    push dword [ecx + 4]        ; hWnd
    call dword [ecx + 0]        ; Panggil WndProc(hWnd, Msg, wParam, lParam)

    ; Kirim nilai return LRESULT (EAX) balik ke KeUserModeCallback
    push dword 0                ; CallbackStatus = STATUS_SUCCESS
    push dword 4                ; ResultLength = 4 byte
    push eax                    ; Result = LRESULT dari WndProc
    call VzCallbackReturn

.unsupported:
    push dword 0xC0000002       ; STATUS_NOT_IMPLEMENTED
    push dword 0
    push dword 0
    call VzCallbackReturn

KsiRaiseUserExceptionDispatcher:
    XOR eax, eax
    RET

