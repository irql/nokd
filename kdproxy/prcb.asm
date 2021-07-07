
.CODE

PUBLIC      KeGetCurrentPrcb

ALIGN       16
KeGetCurrentPrcb PROC FRAME
    .ENDPROLOG

    ;mov     ecx, 0C0000102h
    ;shl     rdx, 32
    ;mov     eax, eax
    ;or      rax, rdx
    ;mov     rax, [rax+20h]
    mov     rax, gs:[20h]
    ret
KeGetCurrentPrcb ENDP

END