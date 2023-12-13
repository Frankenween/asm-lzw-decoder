global decoderAsm1

%define DICT_LENGTH 4096
%define prefixStart(code,onStack) (esp + 4 * onStack + 4 * code)
%define entryLength(code,onStack) (esp + 4 * onStack + 4 * DICT_LENGTH + 4 * code)

section .text

; cdecl calling convention
; arg1 - const uint8_t* src
; arg2 - size_t srcLen
; arg3 - uint8_t* out
; arg4 - size_t outLen
; return value - size_t bytesWrote or -1 if error occurred
decoderAsm1:
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
    xor ecx, ecx

    initCharLoop:
    mov DWORD [entryLength(ecx, 0)], 1
    mov DWORD [prefixStart(ecx, 0)], 0
    inc ecx
    cmp ecx, 256
    jb initCharLoop
    initNewCodesLoop:
    mov DWORD [entryLength(ecx, 0)], 0
    mov DWORD [prefixStart(ecx, 0)], 0
    inc ecx
    cmp ecx, DICT_LENGTH
    jb initNewCodesLoop
    ; finished dict init
    xor ebp, ebp ; outputIndex
    xor eax, eax ; cntCode
    mov edx, 0xFFFFFFFF ; oldCode = -1
    mov dx, 258 ; newCode
    push DWORD 0 ; bitsRead
    push DWORD 9 ; codeLen

    mainLoop:
    ; retrieve cntCode
    mov ecx, [esp + 4] ; bitsRead
    xor eax, eax
    xor ebx, ebx
    mov bl, cl ; lowerByte
    and bl, 7
    add bl, [esp] ; lowest byte from codeLen
    add bl, 7
    shr ecx, 3 ; pos

    cmp bl, 16
    ja retrieve3Bytes
    ; extract 2 bytes
    mov ax, [esi + ecx]
    xchg ah, al
    jmp codeRetrieved

    retrieve3Bytes:
    add ecx, 3
    sub bx, 8
    cmp ecx, [esp + 4 * (2 + 2 * DICT_LENGTH + 5) + 4 * 1] ; cmp with n(arg2)
    jnb retrieveExactly3Bytes ; it is data end and 3 bytes are left
    ; now 4 bytes are valid
    movbe eax, [esi + ecx - 3]
    sub bx, 8
    codeRetrieved:
    ; todo: bextr
    neg bx
    add bx, 23
    mov ecx, ebx
    shr eax, cl
    mov ebx, [esp]
    bzhi eax, eax, ebx
    add [esp + 4], ebx ; bitsRead += codeLen

    cmp ebp, [esp + 4 * (2 + 2 * DICT_LENGTH + 5) + 4 * 3]
    jne haveFree
    cmp eax, 256
    jb decoderFailExit
    cmp eax, 257
    ja decoderFailExit
    haveFree:
    ; now ax contains code
    ; ebx and ecx are free for use now
    cmp ax, 256
    jnb cntCodeNotChar
    ; char
    mov [edi + ebp], al
    mov [prefixStart(eax, 2)], ebp
    inc ebp

    cmp edx, 0
    jl noOldCode ; oldCode == -1

    mov ebx, edx
    shr ebx, 16 ; ebx contains oldCode
    and edx, 0xFFFF ; clean oldCode
    mov ecx, [entryLength(ebx, 2)]
    inc ecx
    mov [entryLength(edx, 2)], ecx
    mov [prefixStart(edx, 2)], ebp
    sub [prefixStart(edx, 2)], ecx
    inc edx ; newCode++
    ; update codeLen
    lea ebx, [edx + 1]
    mov ecx, [esp]
    shr ebx, cl
    add [esp], ebx

    noOldCode:
    ; here oldCode is 0
    shl eax, 16
    ; mov ax, dx ; save new code
    and edx, 0xFFFF
    or edx, eax ; update oldCode
    jmp loopTestConditions

    cntCodeNotChar:
    cmp ax, 258
    jb controlCodeRetrieved
    ; now code >= 258 and a new code

    ; set entryLength and prefixStart for newCode
    mov ebx, edx
    shr ebx, 16
    and edx, 0xFFFF ; clear oldCode
    mov ecx, [entryLength(ebx, 2)] ; ecx <- entryLength[oldCode]
    mov [prefixStart(edx, 2)], ebp
    sub [prefixStart(edx, 2)], ecx
    inc ecx
    mov [entryLength(edx, 2)], ecx

    mov ebx, [esp + 4 * (2 + 2 * DICT_LENGTH + 5) + 4 * 3] ; outLen
    sub ebx, ebp ; ebx = rest

    ; after if iterations in ecx, baseRead in ebx
    cmp eax, edx
    ja decoderFailExit ; cntCode > newCode
    je cntCodeIsNew ; cntCode == newCode
    ; now cntCode < newCode
    mov ecx, [entryLength(eax, 2)]
    cmp ecx, ebx
    cmovnb ecx, ebx ; min(len[cnt], rest)
    mov ebx, [prefixStart(eax, 2)]
    mov [prefixStart(eax, 2)], ebp
    jmp writeResultString

    cntCodeIsNew:
    ;mov ecx, [entryLength(edx, 2)] - in ecx already
    cmp ecx, ebx
    cmovnb ecx, ebx ; min(len[newCode], rest)
    dec ecx
    mov ebx, [prefixStart(edx, 2)]
    mov al, [edi + ebx] ; can break eax, because it is equal to edx
    mov [edi + ebp], al
    inc ebx
    inc ebp
    mov eax, edx

    writeResultString:
    ; version 1 - movsb
    lea esi, [edi + ebx]
    add edi, ebp
    add ebp, ecx ; outputIndex += iterations
    REP movsb ; copy
    ; restore esi and edi
    mov esi, [esp + 4 * (2 + 2 * DICT_LENGTH + 5) + 4 * 0] ; src
    mov edi, [esp + 4 * (2 + 2 * DICT_LENGTH + 5) + 4 * 2] ; dst
    inc edx ; newCode++
    ; update code len
    lea ebx, [edx + 1]
    mov ecx, [esp]
    shr ebx, cl
    add [esp], ebx
    ; oldCode = cntCode
    shl eax, 16
    and edx, 0xFFFF
    or edx, eax ; update oldCode

    ; test here loop condition
    loopTestConditions:
    mov ebx, [esp + 4]
    add ebx, [esp]
    mov ecx, [esp + 4 * (2 + 2 * DICT_LENGTH + 5) + 4 * 1]
    shl ecx, 3
    cmp ebx, ecx
    jnb decoderFailExit
    cmp ebp, [esp + 4 * (2 + 2 * DICT_LENGTH + 5) + 4 * 3]
    ja decoderFailExit
    jmp mainLoop

    decoderFailExit:
    mov eax, 0xFFFFFFFF ; decoder didn't finish successfully
    decoderExit:
    lea esp, [esp + 4 * (2 + 2 * DICT_LENGTH)] ; free stack
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
    cmp ax, 256 ; clear code
    jne EOFCode
    mov edx, 0xFFFFFFFF ; oldCode = -1
    mov dx, 258 ; newCode
    mov DWORD [esp], 9 ; codeLen
    jmp loopTestConditions
    EOFCode:
    mov eax, ebp ; finished successfully
    jmp decoderExit
