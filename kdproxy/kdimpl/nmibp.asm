
.CODE

PUBLIC      KdNmiBp
PUBLIC      KdNmiServiceBp

KD_SERVICE_BP_MAGIC = 42244224h

ALIGN       16
KdNmiBp     PROC FRAME
    .ENDPROLOG
    xor     r11, r11
    int     2
    ret
KdNmiBp     ENDP

ALIGN       16
KdNmiServiceBp PROC FRAME
    .ENDPROLOG
    ;
    ; R10 = ServiceNumber
    ; R11 = KD_SERVICE_BP_MAGIC
    ; 

    mov     r11, KD_SERVICE_BP_MAGIC
    mov     r10, rcx

    mov     rcx, rdx
    mov     rdx, r8
    mov     r8, r9
    mov     r9, qword ptr [rsp+8]
    int     2
    ret
KdNmiServiceBp ENDP

END