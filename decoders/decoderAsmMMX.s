global decoderAsmMMX

%define DICT_LENGTH 4096
%define prefixStart(code,onStack) (esp + 4 * onStack + 4 * code)
%define entryLength(code,onStack) (esp + 4 * onStack + 4 * DICT_LENGTH + code * 2)

section .text

; cdecl calling convention
; arg1 - const uint8_t* src
; arg2 - size_t srcLen
; arg3 - uint8_t* out
; arg4 - size_t outLen
; return value - size_t bytesWrote or -1 if error occurred
decoderAsmMMX:
    push ebx
    push edi
    push esi
    push ebp
    ; init src and dst
    mov esi, [esp + 4 * 5 + 4 * 0] ; src
    mov edi, [esp + 4 * 5 + 4 * 2] ; dst

    ; alloc arrays
    lea esp, [esp - 4 * DICT_LENGTH - 2 * DICT_LENGTH]
    ; esp + onStack = prefixStart
    ; esp + onStack + 4 * DICT_LENGTH = entryLengths

    ; init dict
    mov ecx, DICT_LENGTH

    initNewCodesLoop:
    mov WORD [entryLength(ecx, 0) - 4], 0
    dec ecx
    cmp ecx, 256
    ja initNewCodesLoop
    initCharsLoop:
    mov WORD [entryLength(ecx, 0) - 4], 1
    dec ecx
    jnz initCharsLoop

    mov ecx, DICT_LENGTH
    initPrefsLoop:
    mov DWORD [prefixStart(ecx, 0) - 4], 0
    dec ecx
    cmp ecx, 256
    ja initPrefsLoop
    initPCharsLoop:
    mov DWORD [prefixStart(ecx, 0) - 4], 0
    dec ecx
    jnz initPCharsLoop
    ; finished dict init
    xor ebp, ebp ; outputIndex
    xor eax, eax ; cntCode
    mov edx, 258 ; newCode
    push DWORD 0 ; entryLen[oldCode]
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
    cmp ecx, [esp + 4 * (3 + 5) + 6 * DICT_LENGTH + 4 * 1] ; cmp with n(arg2)
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

    cmp ebp, [esp + 4 * (3 + 5) + 6 * DICT_LENGTH + 4 * 3]
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
    inc ebp

    cmp DWORD [esp + 8], 0
    je noOldCode ; entryLength[oldCode] == 0

    mov ecx, [esp + 8] ; todo: oldCode
    inc ecx
    mov [entryLength(edx, 3)], cx
    sub ebp, ecx
    mov [prefixStart(edx, 3)], ebp
    add ebp, ecx
    inc edx ; newCode++
    ; update codeLen
    lea ebx, [edx + 1]
    mov ecx, [esp]
    shr ebx, cl
    add [esp], ebx

    noOldCode:
    ; here oldCode is 0
    mov DWORD [esp + 8], 1 ; entryLength[cntCode] = 1
    jmp loopTestConditions

    cntCodeNotChar:
    cmp eax, 258
    jb controlCodeRetrieved
    ; now code >= 258 and a new code

    ; set entryLength and prefixStart for newCode

    mov ecx, [esp + 8] ; todo: oldCode
    sub ebp, ecx
    mov [prefixStart(edx, 3)], ebp
    add ebp, ecx
    inc ecx
    mov [entryLength(edx, 3)], cx

    mov ebx, [esp + 4 * (3 + 5) + 6 * DICT_LENGTH + 4 * 3] ; outLen
    sub ebx, ebp ; ebx = rest

    mov cx, [entryLength(eax, 3)]
    mov [esp + 8], ecx ; upd entryLength[oldCode]
    cmp ecx, ebx
    cmovnb ecx, ebx ; min(len[cnt], rest)

    mov ebx, [prefixStart(eax, 3)]

    ; after if iterations in ecx, baseRead in ebx
    cmp eax, edx
    ;ja decoderFailExit ; cntCode > newCode
    jne writeResultString ; cntCode < newCode
    ; now cntCode == newCode

    dec ecx
    mov al, [edi + ebx] ; can break eax, because it is equal to edx
    mov [edi + ebp], al
    inc ebx
    inc ebp
    mov eax, edx

    writeResultString:
    cmp eax, edx
    ja decoderFailExit
    cmp ecx, 8
    jb shortCopy

    bigCopy:
    movq mm0, [edi + ebx]
    movq [edi + ebp], mm0
    add ebx, 8
    add ebp, 8
    sub ecx, 8
    cmp ecx, 8
    jnb bigCopy
    ;lea esi, [edi + ebx]
    ;add edi, ebp
    ;add ebp, ecx ; outputIndex += iterations
    ;REP movsb ; copy
    ; restore esi and edi
    ;mov esi, [esp + 4 * (3 + 5) + 6 * DICT_LENGTH + 4 * 0] ; src
    ;mov edi, [esp + 4 * (3 + 5) + 6 * DICT_LENGTH + 4 * 2] ; dst
    ;jmp copiedAll
    shortCopy:
    cmp ecx, 4
    jb noFourShort
    mov eax, [edi + ebx]
    mov [edi + ebp], eax
    add ebx, 4
    add ebp, 4
    sub ecx, 4
    noFourShort:
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
    mov ecx, [esp + 4 * (3 + 5) + 6 * DICT_LENGTH + 4 * 1]
    shl ecx, 3
    cmp ebx, ecx
    jnb decoderFailExit
    cmp ebp, [esp + 4 * (3 + 5) + 6 * DICT_LENGTH + 4 * 3]
    ja decoderFailExit
    jmp mainLoop

    decoderFailExit:
    mov eax, 0xFFFFFFFF ; decoder didn't finish successfully
    decoderExit:
    lea esp, [esp + 4 * (3) + 6 * DICT_LENGTH] ; free stack
    pop ebp
    pop esi
    pop edi
    pop ebx
    emms ; clean fpu stack
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
    mov DWORD [esp + 8], 0
    mov DWORD [esp], 9 ; codeLen
    jmp loopTestConditions
    EOFCode:
    mov eax, ebp ; finished successfully
    jmp decoderExit
