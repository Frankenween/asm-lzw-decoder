global lzw_decode

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
lzw_decode:
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

        add ecx, 3
        cmp ecx, [esp + 4 * (3 + 2 * DICT_LENGTH + 5) + 4 * 1] ; cmp with n(arg2)
        jnb retrieveExactly ; it is data end and 3 or less bytes are left
        ; now 4 bytes are valid
        movbe eax, [esi + ecx - 3]
        codeRetrieved:

        mov bh, [esp]
        bextr eax, eax, ebx

        mov ebx, [esp]
        add [esp + 4], ebx ; bitsRead += codeLen

        prefetcht0 [prefixStart(edx, 3)]

        cmp ebp, [esp + 4 * (3 + 2 * DICT_LENGTH + 5) + 4 * 3]
        je bufferFull

        haveFree:
        ; now ax contains code
        ; ebx and ecx are free for use now
        mov ecx, [esp + 8]

        ; update data for new entry
        sub ebp, ecx
        mov [prefixStart(edx, 3)], ebp
        add ebp, ecx
        mov [entryLength(edx, 3)], ecx
        inc DWORD [entryLength(edx, 3)]

        cmp eax, 256
        jnb cntCodeNotChar
            ; char
            mov [edi + ebp], al
            inc ebp

            mov DWORD [esp + 8], 1
            cmp ecx, 0
            jne updateNewCode ; entryLength[oldCode] != 0
            ; no entries were created - need to fix len for 258
            ; mov DWORD [entryLength(258, 3)], 0
            ; in fact no need to fix
            jmp loopTestConditions

        cntCodeNotChar:
            cmp eax, 258
            jb controlCodeRetrieved
            ; now code >= 258 and a new code

            mov ebx, [esp + 4 * (3 + 2 * DICT_LENGTH + 5) + 4 * 3] ; outLen
            sub ebx, ebp ; ebx = rest

            mov ecx, [entryLength(eax, 3)]
            mov [esp + 8], ecx ; upd entryLength[oldCode]
            cmp ecx, ebx
            ja decoderFailExit ; fail

            mov ebx, [prefixStart(eax, 3)]
            prefetchw [edi + ebx]
            ;prefetchw [edi + ebp]

            ; after if iterations in ecx, baseRead in ebx
            cmp eax, edx
            ja decoderFailExit ; cntCode > newCode
            ; now cntCode == newCode

            dec ecx
            mov al, [edi + ebx] ; can break eax, because it is equal to edx
            mov [edi + ebp], al
            inc ebx
            inc ebp
            mov eax, edx

        writeResultString:
            ; sse3
            lea ecx, [ecx + ebp + 16] ; if change, don't forget to fix in copyExact
            cmp ecx, [esp + 4 * (3 + 2 * DICT_LENGTH + 5) + 4 * 3] ; out len
            ja copyExact
            sub ecx, ebp
            sub ecx, 16
            inexactBigCopy:
                movdqu xmm0, [edi + ebx]
                movdqu [edi + ebp], xmm0
                add ebx, 16
                add ebp, 16
                sub ecx, 16
                ja inexactBigCopy

        copiedAll:
        add ebp, ecx ; fix misstep from both versions


        updateNewCode:
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
        ; all through
    decoderExit:
        lea esp, [esp + 4 * (3 + 2 * DICT_LENGTH)] ; free stack
        pop ebp
        pop esi
        pop edi
        pop ebx
        ret

    retrieveExactly:
        ; 1 byte
        sub ecx, 3
        xor eax, eax
        mov al, [esi + ecx]
        inc ecx
        cmp ecx, [esp + 4 * (3 + 2 * DICT_LENGTH + 5) + 4 * 1]
        je doneShortRead1
        ; 2 bytes
        shl eax, 8
        mov al, [esi + ecx]
        inc ecx
        cmp ecx, [esp + 4 * (3 + 2 * DICT_LENGTH + 5) + 4 * 1]
        je doneShortRead2
        ; byte 3
        shl eax, 8
        mov al, [esi + ecx]
        inc ecx
        shl eax, 8
        jmp codeRetrieved

        doneShortRead1:
            shl eax, 24
            jmp codeRetrieved
        doneShortRead2:
            shl eax, 16
            jmp codeRetrieved

    bufferFull:
        sub eax, 256
        cmp eax, 1
        ja decoderFailExit
        add eax, 256
        ; fall through
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

    copyExact:
        sub ecx, ebp ; fix moved
        sub ecx, 16
        cmp ecx, 4
        jb shortCopy

        bigCopy:
            mov eax, [edi + ebx]
            mov [edi + ebp], eax
            add ebx, 4
            add ebp, 4
            sub ecx, 4
            cmp ecx, 4
            jnb bigCopy

        shortCopy:
            cmp ecx, 1
            jb copiedAll
            ja moreThanOne

            mov ah, [edi + ebx]
            mov [edi + ebp], ah
            jmp copiedAll

        moreThanOne:
            cmp ecx, 2
            ja copyThree
            mov eax, [edi + ebx]
            mov [edi + ebp], eax
            jmp copiedAll
        copyThree:
            mov ah, [edi + ebx]
            mov [edi + ebp], ah
            mov ebx, [edi + ebx + 1]
            mov [edi + ebp + 1], ebx
        jmp copiedAll