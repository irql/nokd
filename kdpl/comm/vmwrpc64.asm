
.CODE

;
; Resources: 
; - https://github.com/sysprogs/VirtualKD/blob/master/kdvm/vmwrpc64.asm
; - https://web.archive.org/web/20100610223425/http://chitchat.at.infoseek.co.jp:80/vmware/backdoor.html#cmd1e_hb_7
; Also it seems vmware cares about the state of some registers so the code is
; very weird and awkward. I had initially started reversing their replacement
; kdcom.dll until I realised it is public source, I'm only interested in their 
; vmware rpc, at the time of writing this, serial ports are not working at all
; on my vmware setup.
;

STATUS_UNSUCCESSFUL            = 0C0000001h

VMWARE_MAGIC                   = 564D5868h  ; 'VMXh'
VMWARE_IO_PORT                 = 5658h      ; 'VX'

VMWARE_CMD_RPC                 = 1Eh
VMWARE_MAGIC_RPC               = 0C9435052h ; 'RPCI' | 80000000h

VMWRPC_CMD_OPEN_CHANNEL        = 0000h
VMWRPC_CMD_SEND_COMMAND_LENGTH = 0001h 
VMWRPC_CMD_SEND_COMMAND_BUFFER = 0
VMWRPC_CMD_RECV_COMMAND_LENGTH = 0003h
VMWRPC_CMD_RECV_COMMAND_BUFFER = 0
VMWRPC_CMD_RECV_COMMAND_FINISH = 0005h
VMWRPC_CMD_CLOSE_CHANNEL       = 0006h

PUBLIC      KdVmwRpcOpenChannel
PUBLIC      KdVmwRpcCloseChannel
PUBLIC      KdVmwRpcSendCommandLength
PUBLIC      KdVmwRpcSendCommandBuffer
PUBLIC      KdVmwRpcSendFull
PUBLIC      KdVmwRpcRecvCommandLength
PUBLIC      KdVmwRpcRecvCommandBuffer
PUBLIC      KdVmwRpcRecvCommandFinish

KD_VMWRPC_CONTROL STRUCT 8
	Cookie1    dd 0
	Cookie2    dd 0
	Channel    dd 0
	RecvId     dd 0
KD_VMWRPC_CONTROL ENDS 

ALIGN       16
KdVmwRpcOpenChannel PROC FRAME

    push    rbx
.PUSHREG    rbx
    push    rsi
.PUSHREG    rsi
    push    rdi
.PUSHREG    rdi
.ENDPROLOG

    mov     eax, VMWARE_MAGIC
    mov     ebx, VMWARE_MAGIC_RPC
    mov     r8, rcx
    mov     ecx, VMWARE_CMD_RPC or (VMWRPC_CMD_OPEN_CHANNEL shl 16) ;1Eh
    mov     edx, VMWARE_IO_PORT
    out     dx, eax
    cmp     ecx, 10000h
    jne     @KdpVmwRpcExcp
    mov     dword ptr [r8+KD_VMWRPC_CONTROL.Cookie1], esi
    mov     dword ptr [r8+KD_VMWRPC_CONTROL.Cookie2], edi
    mov     dword ptr [r8+KD_VMWRPC_CONTROL.Channel], edx
    xor     rax, rax
    jmp     @KdpVmwRcpRetn

@KdpVmwRpcExcp:
    mov     rax, STATUS_UNSUCCESSFUL
@KdpVmwRcpRetn:
    pop     rdi
    pop     rsi
    pop     rbx
    ret

KdVmwRpcOpenChannel ENDP

ALIGN       16
KdVmwRpcCloseChannel PROC FRAME

    push    rsi
.PUSHREG    rsi
    push    rdi
.PUSHREG    rdi
.ENDPROLOG

    mov     eax, VMWARE_MAGIC
    mov     edx, [rcx+KD_VMWRPC_CONTROL.Channel]
    or      edx, VMWARE_IO_PORT
    mov     esi, [rcx+KD_VMWRPC_CONTROL.Cookie1]
    mov     edi, [rcx+KD_VMWRPC_CONTROL.Cookie2]
    mov     ecx, VMWARE_CMD_RPC or (VMWRPC_CMD_CLOSE_CHANNEL shl 16) ;6001Eh
    out     dx, eax
    xor     eax, eax
    cmp     ecx, 10000h
    je      @KdpVmwRpcRetn
    mov     eax, STATUS_UNSUCCESSFUL
@KdpVmwRpcRetn:
    pop     rdi
    pop     rsi
    ret

KdVmwRpcCloseChannel ENDP

ALIGN       16
KdVmwRpcSendCommandLength PROC FRAME

    push    rbx
.PUSHREG    rbx
    push    rsi
.PUSHREG    rsi
    push    rdi
.PUSHREG    rdi
.ENDPROLOG

    mov     eax, VMWARE_MAGIC
    mov     ebx, edx
    mov     edx, dword ptr [rcx+KD_VMWRPC_CONTROL.Channel]
    or      edx, VMWARE_IO_PORT
    mov     esi, dword ptr [rcx+KD_VMWRPC_CONTROL.Cookie1]
    mov     edi, dword ptr [rcx+KD_VMWRPC_CONTROL.Cookie2]
    mov     ecx, VMWARE_CMD_RPC or (VMWRPC_CMD_SEND_COMMAND_LENGTH shl 16); 1001Eh
    out     dx, eax
    xor     eax, eax
    cmp     ecx, 810000h
    je      @KdpVmwRpcRetn
    mov     eax, STATUS_UNSUCCESSFUL
@KdpVmwRpcRetn:
    pop     rdi
    pop     rsi
    pop     rbx
    ret

KdVmwRpcSendCommandLength ENDP

ALIGN       16
KdVmwRpcSendCommandBuffer PROC FRAME

    push    rbx
.PUSHREG    rbx
    push    rsi
.PUSHREG    rsi
    push    rdi
.PUSHREG    rdi
    push    rbp
.PUSHREG    rbp
.ENDPROLOG

    mov     eax, VMWARE_MAGIC
    xchg    rcx, rdx
    mov     ebx, 10000h
    mov     rsi, r8
    mov     ebp, dword ptr [rdx+KD_VMWRPC_CONTROL.Cookie1]
    mov     edi, dword ptr [rdx+KD_VMWRPC_CONTROL.Cookie2]
    mov     edx, dword ptr [rdx+KD_VMWRPC_CONTROL.Channel]
    or      edx, VMWARE_IO_PORT + 1
    cld
    rep     outsb
    xor     eax, eax
    cmp     ebx, 10000h
    je      @KdpVmwRpcRetn
    mov     eax, STATUS_UNSUCCESSFUL
@KdpVmwRpcRetn:
    pop     rbp
    pop     rdi
    pop     rsi
    pop     rbx
    ret

KdVmwRpcSendCommandBuffer ENDP

; i dont think this is correct.
ALIGN       16
KdVmwRpcSendFull PROC FRAME

    push    rbx
.PUSHREG    rbx
    push    rsi
.PUSHREG    rsi
    push    rdi
.PUSHREG    rdi
    push    rbp
.PUSHREG    rbp
.ENDPROLOG

    mov     eax, VMWARE_MAGIC
    mov     ebx, edx
    mov     edx, dword ptr [rcx+KD_VMWRPC_CONTROL.Channel]
    or      edx, VMWARE_IO_PORT + 1
    mov     esi, dword ptr [rcx+KD_VMWRPC_CONTROL.Cookie1]
    mov     edi, dword ptr [rcx+KD_VMWRPC_CONTROL.Cookie2]
    mov     r9, rcx
    mov     ecx, 1001Eh
    out     dx, eax
    cmp     ecx, 810000h
    jne     @KdpVmwRpcExcp
    xchg    ecx, ebx
    mov     ebx, 10000h
    mov     rsi, r8
    mov     edx, dword ptr [rcx+KD_VMWRPC_CONTROL.Channel]
    or      edx, 'VX'
    mov     ebp, dword ptr [rcx+KD_VMWRPC_CONTROL.Cookie1]
    mov     edi, dword ptr [rcx+KD_VMWRPC_CONTROL.Cookie2]
    rep     outsb
    xor     eax, eax
    cmp     ebx, 10000h
    jne     @KdpVmwRpcExcp
    jmp     @KdpVmwRpcRetn
@KdpVmwRpcExcp:
    mov     eax, 0C0000001h
@KdpVmwRpcRetn:
    pop     rbp
    pop     rdi
    pop     rsi
    pop     rbx
    ret

KdVmwRpcSendFull ENDP

ALIGN       16
KdVmwRpcRecvCommandLength PROC FRAME

    push    rbx
.PUSHREG    rbx
    push    rsi
.PUSHREG    rsi
    push    rdi
.PUSHREG    rdi
.ENDPROLOG

    mov     r9, rdx
    mov     eax, VMWARE_MAGIC
    mov     edx, dword ptr [rcx+KD_VMWRPC_CONTROL.Channel]
    or      edx, VMWARE_IO_PORT
    mov     esi, dword ptr [rcx+KD_VMWRPC_CONTROL.Cookie1]
    mov     edi, dword ptr [rcx+KD_VMWRPC_CONTROL.Cookie2]
    mov     r8, rcx
    mov     ecx, VMWARE_CMD_RPC or (VMWRPC_CMD_RECV_COMMAND_LENGTH shl 16) ;3001Eh
    out     dx, eax
    mov     dword ptr [r8+KD_VMWRPC_CONTROL.RecvId], edx
    mov     dword ptr [r9], ebx
    xor     eax, eax
    ;
    ; ecx is different on my vmware version 
    ; or something, this value no longer indicates
    ; success and has been causing issues.
    ; assume success.
    ;
    ;cmp     ecx, 830000h
    ;je      @KdpVmwRpcRetn
    ;mov     eax, STATUS_UNSUCCESSFUL
@KdpVmwRpcRetn:
    pop     rdi
    pop     rsi
    pop     rbx
    ret

KdVmwRpcRecvCommandLength ENDP

ALIGN       16
KdVmwRpcRecvCommandBuffer PROC FRAME

    push    rbx
.PUSHREG    rbx
    push    rsi
.PUSHREG    rsi
    push    rdi
.PUSHREG    rdi
    push    rbp
.PUSHREG    rbp
.ENDPROLOG

    mov     eax, VMWARE_MAGIC
    xchg    rcx, rdx
    mov     rdi, r8
    mov     esi, dword ptr [rdx+KD_VMWRPC_CONTROL.Cookie1]
    mov     ebp, dword ptr [rdx+KD_VMWRPC_CONTROL.Cookie2]
    mov     edx, dword ptr [rdx+KD_VMWRPC_CONTROL.Channel]
    or      edx, VMWARE_IO_PORT + 1
    mov     ebx, 10000h
    rep     insb
    xor     eax, eax
    cmp     ebx, 10000h
    je      @KdpVmwRpcRetn
    mov     eax, STATUS_UNSUCCESSFUL
@KdpVmwRpcRetn:
    pop     rbp
    pop     rdi
    pop     rsi
    pop     rbx
    ret

KdVmwRpcRecvCommandBuffer ENDP

ALIGN       16
KdVmwRpcRecvCommandFinish PROC FRAME

    push    rbx
.PUSHREG    rbx
    push    rsi
.PUSHREG    rsi
    push    rdi
.PUSHREG    rdi
.ENDPROLOG

    mov     eax, VMWARE_MAGIC
    mov     ebx, dword ptr [rcx+KD_VMWRPC_CONTROL.RecvId]
    mov     esi, dword ptr [rcx+KD_VMWRPC_CONTROL.Cookie1]
    mov     edi, dword ptr [rcx+KD_VMWRPC_CONTROL.Cookie2]
    mov     edx, dword ptr [rcx+KD_VMWRPC_CONTROL.Channel]
    or      edx, VMWARE_IO_PORT
    mov     ecx, VMWARE_CMD_RPC or (VMWRPC_CMD_RECV_COMMAND_FINISH shl 16); 5001Eh
    out     dx, eax
    xor     eax, eax
    cmp     ecx, 10000h
    je      @KdpVmwRpcRetn
    mov     eax, STATUS_UNSUCCESSFUL
@KdpVmwRpcRetn:
    pop     rdi
    pop     rsi
    pop     rbx
    ret

KdVmwRpcRecvCommandFinish ENDP

END