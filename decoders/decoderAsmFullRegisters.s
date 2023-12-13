global decoderAsmFullRegisters

%define DICT_LENGTH 4096
%define prefixStart(code,onStack) (esp + 4 * onStack + 8 * code)
%define entryLength(code,onStack) (esp + 4 * onStack + 8 * code + 4)

section .text

; cdecl calling convention
; arg1 - const uint8_t* src
; arg2 - size_t srcLen
; arg3 - uint8_t* out
; arg4 - size_t outLen
; return value - size_t bytesWrote or -1 if error occurred
decoderAsmFullRegisters:
    push ebx
    push edi
    push esi
    push ebp
    ; init src and dst
    mov esi, [esp + 4 * 5 + 4 * 0] ; src
    mov edi, [esp + 4 * 5 + 4 * 2] ; dst

    ; alloc arrays
    lea esp, [esp - 4 * DICT_LENGTH - 4 * DICT_LENGTH]
    ; esp + onStack = prefixStart
    ; esp + onStack + 4 * DICT_LENGTH = entryLengths

    ; init dict
    mov ecx, DICT_LENGTH - 1

    initNewCodesLoop:
    mov DWORD [entryLength(ecx, 0)], 0
    dec ecx
    cmp ecx, 256
    jnb initNewCodesLoop
    initCharsLoop:
    mov DWORD [entryLength(ecx, 0)], 1
    dec ecx
    jnz initCharsLoop
    mov DWORD [entryLength(0, 0)], 1
    ; finished dict init
    xor ebp, ebp ; outputIndex
    xor eax, eax ; cntCode
    ;mov edx, 0xFFFFFFFF ; oldCode = -1
    mov edx, 258 ; newCode
    push DWORD -1 ; oldCode
    push DWORD 0 ; bitsRead
    push DWORD 9 ; codeLen

    mainLoop:
    ; retrieve cntCode
    mov ecx, [esp + 4] ; bitsRead
    xor eax, eax
    mov ebx, ecx
    and ebx, 7
    add ebx, [esp] ; lowest byte from codeLen
    add ebx, 7
    shr ecx, 3 ; pos

    cmp ebx, 16
    ja retrieve3Bytes
    ; extract 2 bytes
    mov ax, [esi + ecx]
    xchg ah, al
    jmp codeRetrieved

    retrieve3Bytes:
    add ecx, 3
    sub ebx, 8
    cmp ecx, [esp + 4 * (3 + 2 * DICT_LENGTH + 5) + 4 * 1] ; cmp with n(arg2)
    jnb retrieveExactly3Bytes ; it is data end and 3 bytes are left
    ; now 4 bytes are valid
    movbe eax, [esi + ecx - 3]
    sub ebx, 8
    codeRetrieved:
    ; todo: bextr
    neg ebx
    add ebx, 23
    ;mov ecx, ebx
    ;shr eax, cl
    mov bh, [esp]
    bextr eax, eax, ebx

    mov ebx, [esp]
    add [esp + 4], ebx ; bitsRead += codeLen

    cmp ebp, [esp + 4 * (3 + 2 * DICT_LENGTH + 5) + 4 * 3]
    jne haveFree
    sub eax, 256
    cmp eax, 1
    ja decoderFailExit

    add eax, 256
    haveFree:
    ; now ax contains code
    ; ebx and ecx are free for use now
    cmp eax, 256
    jnb cntCodeNotChar
    ; char
    mov [edi + ebp], al
    mov [prefixStart(eax, 3)], ebp
    inc ebp

    cmp DWORD [esp + 8], 0
    jl noOldCode ; oldCode == -1

    mov ebx, [esp + 8]
    ;shr ebx, 16 ; ebx contains oldCode
    ;and edx, 0xFFFF ; clean oldCode
    mov ecx, [entryLength(ebx, 3)]
    inc ecx
    mov [entryLength(edx, 3)], ecx
    mov [prefixStart(edx, 3)], ebp
    sub [prefixStart(edx, 3)], ecx
    inc edx ; newCode++
    ; update codeLen
    lea ebx, [edx + 1]
    mov ecx, [esp]
    shr ebx, cl
    add [esp], ebx

    noOldCode:
    ; here oldCode is 0
    ;shl eax, 16
    ; mov ax, dx ; save new code
    ;and edx, 0xFFFF
    ;or edx, eax ; update oldCode
    mov [esp + 8], eax
    jmp loopTestConditions

    cntCodeNotChar:
    cmp eax, 258
    jb controlCodeRetrieved
    ; now code >= 258 and a new code

    ; set entryLength and prefixStart for newCode
    ;mov ebx, edx
    ;shr ebx, 16
    ;and edx, 0xFFFF ; clear oldCode
    mov ebx, [esp + 8]
    mov ecx, [entryLength(ebx, 3)] ; ecx <- entryLength[oldCode]
    mov [prefixStart(edx, 3)], ebp
    sub [prefixStart(edx, 3)], ecx
    inc ecx
    mov [entryLength(edx, 3)], ecx

    mov ebx, [esp + 4 * (3 + 2 * DICT_LENGTH + 5) + 4 * 3] ; outLen
    sub ebx, ebp ; ebx = rest

    ; after if iterations in ecx, baseRead in ebx
    cmp eax, edx
    ja decoderFailExit ; cntCode > newCode
    je cntCodeIsNew ; cntCode == newCode
    ; now cntCode < newCode
    mov ecx, [entryLength(eax, 3)]
    cmp ecx, ebx
    cmovnb ecx, ebx ; min(len[cnt], rest)
    mov ebx, [prefixStart(eax, 3)]
    mov [prefixStart(eax, 3)], ebp
    jmp writeResultString

    cntCodeIsNew:
    ;mov ecx, [entryLength(edx, 2)] - in ecx already
    cmp ecx, ebx
    cmovnb ecx, ebx ; min(len[newCode], rest)
    dec ecx
    mov ebx, [prefixStart(edx, 3)]
    mov al, [edi + ebx] ; can break eax, because it is equal to edx
    mov [edi + ebp], al
    inc ebx
    inc ebp
    mov eax, edx

    writeResultString:
    ;shl eax, 16
    ;and edx, 0xFFFF
    ;or edx, eax ; update oldCode
    mov [esp + 8], eax
    cmp ecx, 4
    jb shortCopy

    bigCopy:
    lea esi, [edi + ebx]
    add edi, ebp
    add ebp, ecx ; outputIndex += iterations
    REP movsb ; copy
    ; restore esi and edi
    mov esi, [esp + 4 * (3 + 2 * DICT_LENGTH + 5) + 4 * 0] ; src
    mov edi, [esp + 4 * (3 + 2 * DICT_LENGTH + 5) + 4 * 2] ; dst
    jmp copiedAll
    shortCopy:
    cmp ecx, 2
    jb noTwoShort
    mov ax, [edi + ebx]
    mov [edi + ebp], ax
    add ebx, 2
    add ebp, 2
    sub ecx, 2
    noTwoShort:
    cmp ecx, 1
    jb copiedAll
    mov al, [edi + ebx]
    mov [edi + ebp], al
    inc ebx
    inc ebp
    dec ecx

    copiedAll:
    inc edx ; newCode++
    ; update code len
    lea ebx, [edx + 1]
    mov ecx, [esp]
    shr ebx, cl
    add [esp], ebx

    ; test here loop condition
    loopTestConditions:
    mov ebx, [esp + 4]
    add ebx, [esp]
    mov ecx, [esp + 4 * (3 + 2 * DICT_LENGTH + 5) + 4 * 1]
    shl ecx, 3
    cmp ebx, ecx
    jnb decoderFailExit
    cmp ebp, [esp + 4 * (3 + 2 * DICT_LENGTH + 5) + 4 * 3]
    ja decoderFailExit
    jmp mainLoop

    decoderFailExit:
    mov eax, 0xFFFFFFFF ; decoder didn't finish successfully
    decoderExit:
    lea esp, [esp + 4 * (3 + 2 * DICT_LENGTH)] ; free stack
    pop ebp
    pop esi
    pop edi
    pop ebx
    ret

    retrieveExactly3Bytes:
    mov al, [esi + ecx - 3]
    shl eax, 16
    mov ah, [esi + ecx - 2]
    mov al, [esi + ecx - 1]
    jmp codeRetrieved

    controlCodeRetrieved:
    cmp eax, 256 ; clear code
    jne EOFCode
    mov edx, 258 ; new code
    mov DWORD [esp + 8], -1
    mov DWORD [esp], 9 ; codeLen
    jmp loopTestConditions
    EOFCode:
    mov eax, ebp ; finished successfully
    jmp decoderExit
