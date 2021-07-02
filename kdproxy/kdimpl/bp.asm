
.CODE

EXTERN      KdBpHandle:PROC

PUBLIC      KdBpContextStore
PUBLIC      KdBpContextRestore

;
; IMPORTANT TODO: Must save & restore extended state information
; via xsave, or even fxsave64 will do because this driver & system 
; components shouldn't use anything further than SSE2
;

KD_BP_CONTEXT STRUCT 8
    SegCs   dq 0
    SegGs   dq 0
    SegFs   dq 0
    SegEs   dq 0
    SegDs   dq 0
    RegR15  dq 0
    RegR14  dq 0
    RegR13  dq 0
    RegR12  dq 0
    RegR11  dq 0
    RegR10  dq 0
    RegR9   dq 0
    RegR8   dq 0
    RegRdi  dq 0
    RegRsi  dq 0
    RegRbp  dq 0
    RegRbx  dq 0
    RegRdx  dq 0
    RegRcx  dq 0
    RegRax  dq 0
    RegRsp  dq 0
    RegRip  dq 0
KD_BP_CONTEXT ENDS

ALIGN       16
KdBpContextStore PROC

    ; 
    ; Read comments on BreakpointCode in kdbp.c, this
    ; is the shellcode which is to be executed by our
    ; breakpoint stub.
    ;
    ; We go back by 6 bytes, meaning we revert the rip
    ; increment from executing call qword ptr [rip], 
    ; and then build our KD_BP_CONTEXT structure on the stack.
    ;
    ; The RegRsp is only pushed for convenience, 16 is added
    ; to account for the 2 pushes (RegRip, and itself).
    ;

    sub     qword ptr [rsp], 6
    push    rsp
    add     qword ptr [rsp], 16
    push    rax
    push    rcx
    push    rdx
    push    rbx
    push    rbp
    push    rsi
    push    rdi
    push    r8
    push    r9
    push    r10
    push    r11
    push    r12
    push    r13
    push    r14
    push    r15

    mov     rax, ds
    push    rax
    mov     rax, es
    push    rax
    mov     rax, fs
    push    rax
    mov     rax, gs
    push    rax
    mov     rax, cs
    push    rax
    pushfq

    mov     rcx, rsp
    mov     rdx, KdBpContextRestore
    push    rdx
    jmp     KdBpHandle
KdBpContextStore ENDP

ALIGN       16
KdBpContextRestore PROC
    
    ;
    ; The return address is decremented, if the breakpoint 
    ; hasn't been removed, it will be executed again.
    ;

    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     r11
    pop     r10
    pop     r9
    pop     r8
    pop     rdi
    pop     rsi
    pop     rbp
    pop     rbx
    pop     rdx
    pop     rcx
    pop     rax
    add     rsp, 8
    ret
KdBpContextRestore ENDP


END