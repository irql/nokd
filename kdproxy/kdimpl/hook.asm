
.CODE

PUBLIC      KeBugCheckExHook

EXTERN      KdBugCheckEx:PROC

ALIGN       16
KeBugCheckExHook PROC FRAME
    add     qword ptr [rsp], 8
    mov     qword ptr [rsp+10h], rcx 
    mov     qword ptr [rsp+18h], rdx 
    mov     qword ptr [rsp+20h], r8 
    mov     qword ptr [rsp+28h], r9 
    sub     rsp, 28h
    .ALLOCSTACK 28h
    .ENDPROLOG
    
    call    KdBugCheckEx
    test    al, al
    jz      @BugCheckProceed
    add     rsp, 8
@BugCheckProceed:
    ret     28h

KeBugCheckExHook ENDP

END