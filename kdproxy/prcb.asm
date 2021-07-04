
.CODE

PUBLIC      KeGetCurrentPrcb

ALIGN       16
KeGetCurrentPrcb PROC FRAME
    .ENDPROLOG
    mov     rax, gs:[20h]
    ret
KeGetCurrentPrcb ENDP

END