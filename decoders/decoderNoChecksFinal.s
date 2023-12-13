global decoderFinalNoChecks

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
; May get out of bounds of both arrays
; Maximal out-of-bounds for src - 2 bytes
; Maximal out-of-bounds for dst - 4096 bytes
decoderFinalNoChecks:
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
    neg ebx
    add ebx, 32
    shr ecx, 3 ; pos

    movbe eax, [esi + ecx]
    ;sub ebx, 8
    codeRetrieved:
    mov bh, [esp]
    bextr eax, eax, ebx

    mov ebx, [esp]
    add [esp + 4], ebx ; bitsRead += codeLen

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

    mov ecx, [esp + 8]
    inc ecx
    mov [entryLength(edx, 3)], ecx
    sub ebp, ecx
    mov [prefixStart(edx, 3)], ebp
    add ebp, ecx
    inc edx ; newCode++
    ; update codeLen
    mov ecx, [esp] ; independent with 2 previous
    lea ebx, [edx + 1]
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

    mov ecx, [esp + 8]
    sub ebp, ecx
    mov [prefixStart(edx, 3)], ebp
    add ebp, ecx
    inc ecx
    mov [entryLength(edx, 3)], ecx

    mov ecx, [entryLength(eax, 3)]
    mov [esp + 8], ecx ; upd entryLength[oldCode]
    ; we don't check if we have enough space

    mov ebx, [prefixStart(eax, 3)]

    ; after if iterations in ecx, baseRead in ebx
    cmp eax, edx
    ja decoderFailExit ; cntCode > newCode

    dec ecx
    mov al, [edi + ebx] ; can break eax, because it is equal to edx
    mov [edi + ebp], al
    inc ebx
    inc ebp
    mov eax, edx

    writeResultString:

    bigCopy: ; may copy come rubbish but will fix it
    mov eax, [edi + ebx]
    mov [edi + ebp], eax
    add ebx, 4
    add ebp, 4
    sub ecx, 4
    jnb bigCopy ; if non-negative

    ; if ecx is < 0, need to fix ebp
    add ebp, ecx

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