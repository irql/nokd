
#include <kd.h>

KD_BREAKPOINT_ENTRY KdpBreakpointTable[ KD_BREAKPOINT_TABLE_LENGTH ];

typedef struct _KD_BP_CONTEXT {
    ULONG64   EFlags;
    ULONG64   SegCs;
    ULONG64   SegGs;
    ULONG64   SegFs;
    ULONG64   SegEs;
    ULONG64   SegDs;
    ULONG64   R15;
    ULONG64   R14;
    ULONG64   R13;
    ULONG64   R12;
    ULONG64   R11;
    ULONG64   R10;
    ULONG64   R9;
    ULONG64   R8;
    ULONG64   Rdi;
    ULONG64   Rsi;
    ULONG64   Rbp;
    ULONG64   Rbx;
    ULONG64   Rdx;
    ULONG64   Rcx;
    ULONG64   Rax;
    ULONG64   Rsp;
    ULONG64   Rip;
} KD_BP_CONTEXT, *PKD_BP_CONTEXT;

VOID
KdBpHandle(
    _Inout_ KD_BP_CONTEXT BpContext
)
{
    CONTEXT                 Context = { 0 };
    STRING                  Head;
    DBGKD_WAIT_STATE_CHANGE Exception = { 0 };

    Context.EFlags = ( ULONG )BpContext.EFlags;
    Context.SegCs = ( USHORT )BpContext.SegCs;
    Context.SegGs = ( USHORT )BpContext.SegGs;
    Context.SegFs = ( USHORT )BpContext.SegFs;
    Context.SegEs = ( USHORT )BpContext.SegEs;
    Context.SegDs = ( USHORT )BpContext.SegDs;
    Context.R15 = BpContext.R15;
    Context.R14 = BpContext.R14;
    Context.R13 = BpContext.R13;
    Context.R12 = BpContext.R12;
    Context.R11 = BpContext.R11;
    Context.R10 = BpContext.R10;
    Context.R9 = BpContext.R9;
    Context.R8 = BpContext.R8;
    Context.Rdi = BpContext.Rdi;
    Context.Rsi = BpContext.Rsi;
    Context.Rbp = BpContext.Rbp;
    Context.Rbx = BpContext.Rbx;
    Context.Rdx = BpContext.Rdx;
    Context.Rcx = BpContext.Rcx;
    Context.Rax = BpContext.Rax;
    Context.Rsp = BpContext.Rsp;
    Context.Rip = BpContext.Rip;

    Context.ContextFlags = CONTEXT_AMD64 | CONTEXT_INTEGER | CONTEXT_SEGMENTS;

    Head.Length = sizeof( DBGKD_WAIT_STATE_CHANGE );
    Head.MaximumLength = sizeof( DBGKD_WAIT_STATE_CHANGE );
    Head.Buffer = ( PCHAR )&Exception;

    KdpSetCommonState( 0x3030, &Context, &Exception );
    KdpSetContextState( &Exception, &Context );

    Exception.u.Exception.FirstChance = TRUE;
    Exception.u.Exception.ExceptionRecord.ExceptionCode = STATUS_BREAKPOINT;
    Exception.u.Exception.ExceptionRecord.ExceptionAddress = Context.Rip;

    KdpSendWaitContinue( 0,
                         &Head,
                         NULL,
                         &Context );

    BpContext.EFlags = ( ULONG )Context.EFlags;
    BpContext.SegCs = ( USHORT )Context.SegCs;
    BpContext.SegGs = ( USHORT )Context.SegGs;
    BpContext.SegFs = ( USHORT )Context.SegFs;
    BpContext.SegEs = ( USHORT )Context.SegEs;
    BpContext.SegDs = ( USHORT )Context.SegDs;
    BpContext.R15 = Context.R15;
    BpContext.R14 = Context.R14;
    BpContext.R13 = Context.R13;
    BpContext.R12 = Context.R12;
    BpContext.R11 = Context.R11;
    BpContext.R10 = Context.R10;
    BpContext.R9 = Context.R9;
    BpContext.R8 = Context.R8;
    BpContext.Rdi = Context.Rdi;
    BpContext.Rsi = Context.Rsi;
    BpContext.Rbp = Context.Rbp;
    BpContext.Rbx = Context.Rbx;
    BpContext.Rdx = Context.Rdx;
    BpContext.Rcx = Context.Rcx;
    BpContext.Rax = Context.Rax;
    BpContext.Rsp = Context.Rsp;
    BpContext.Rip = Context.Rip;
}

//
// The current implementation for breakpoints is to 
// write some shellcode that will jump to some more shellcode,
// this shellcode will save the state of all registers and then
// send an exception to the debugger.
//

//
// The aim is to have this stub in under 15 bytes
//
// ff 15 00 00 00 00        call    qword ptr [rip]
// xx xx xx xx xx xx xx xx  dq      shellcode_address
//

static ULONG BreakpointCodeLength = 6 + 8;
static UCHAR BreakpointCode[ ] = {
    0xff, 0x15, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

KD_STATUS
KdpAddBreakpoint(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body
)
{
    Body;

    //
    // Because of the nature of this,
    // you are able to have any code setup
    // for breakpoints such as a jump instead.
    //

    STRING  Reciprocate;
    SIZE_T  Length;
    ULONG32 CurrentBreakpoint;
    ULONG32 BreakpointHandle;

    *( ULONG_PTR* )( BreakpointCode + 6 ) = ( ULONG_PTR )KdBpContextStore;

    if ( Packet->u.WriteBreakPoint.BreakPointAddress < 0x7FFFFFFEFFFF ) {

        //
        // Because of our breakpoint implementation, it should be clear this isn't
        // going to work for user mode breakpoints, for now we'll disable them
        // by returning an error if the address space is not SessionSpace or KernelSpace.
        //
        // In the future, figure out some nice way to transition to a ring 0 cs,
        // and jump to our code, that would work with UserSpace.
        //

        Packet->ReturnStatus = STATUS_UNSUCCESSFUL;
        goto KdpProcedureDone;
    }

    BreakpointHandle = ( ULONG32 )-1;
    for ( CurrentBreakpoint = 0;
          CurrentBreakpoint < KD_BREAKPOINT_TABLE_LENGTH;
          CurrentBreakpoint++ ) {

        if ( ( KdpBreakpointTable[ CurrentBreakpoint ].Flags & KD_BPE_SET ) == 0 ) {

            BreakpointHandle = CurrentBreakpoint;
            break;
        }
    }

    if ( BreakpointHandle == ( ULONG32 )-1 ) {

        Packet->ReturnStatus = STATUS_UNSUCCESSFUL;
        goto KdpProcedureDone;
    }

    KdpBreakpointTable[ BreakpointHandle ].Address = Packet->u.WriteBreakPoint.BreakPointAddress;
    KdpBreakpointTable[ BreakpointHandle ].Process = PsGetCurrentProcess( );

    Packet->ReturnStatus = MmCopyMemory( KdpBreakpointTable[ BreakpointHandle ].Content,
        ( PVOID )KdpBreakpointTable[ BreakpointHandle ].Address,
                                         BreakpointCodeLength,
                                         MM_COPY_ADDRESS_VIRTUAL,
                                         &Length );
    if ( !NT_SUCCESS( Packet->ReturnStatus ) ) {

        goto KdpProcedureDone;
    }

    Packet->u.WriteBreakPoint.BreakPointHandle = BreakpointHandle;

    Packet->ReturnStatus = MmCopyMemory( ( PVOID )KdpBreakpointTable[ BreakpointHandle ].Address,
                                         BreakpointCode,
                                         BreakpointCodeLength,
                                         MM_COPY_ADDRESS_VIRTUAL,
                                         &Length );
    if ( !NT_SUCCESS( Packet->ReturnStatus ) ) {

        goto KdpProcedureDone;
    }

    KdpBreakpointTable[ BreakpointHandle ].Flags |= KD_BPE_SET;

KdpProcedureDone:
    Reciprocate.MaximumLength = 0;
    Reciprocate.Length = sizeof( DBGKD_MANIPULATE_STATE64 );
    Reciprocate.Buffer = ( PCHAR )Packet;

    return KdDebugDevice.KdSendPacket( KdTypeStateManipulate,
                                       &Reciprocate,
                                       NULL,
                                       &KdpContext );
}

KD_STATUS
KdpDeleteBreakpoint(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body
)
{
    Body;

    STRING     Reciprocate;
    ULONG32    BreakpointHandle;
    SIZE_T     Length;
    KAPC_STATE ApcState;

    // TODO: Needs range check.

    BreakpointHandle = Packet->u.RestoreBreakPoint.BreakPointHandle;

    if ( ( KdpBreakpointTable[ BreakpointHandle ].Flags & KD_BPE_SET ) == 0 ) {

        Packet->ReturnStatus = STATUS_SUCCESS;
        goto KdpProcedureDone;
    }

    KeStackAttachProcess( KdpBreakpointTable[ BreakpointHandle ].Process, &ApcState );

    Packet->ReturnStatus = MmCopyMemory( ( PVOID )KdpBreakpointTable[ BreakpointHandle ].Address,
                                         KdpBreakpointTable[ BreakpointHandle ].Content,
                                         BreakpointCodeLength,
                                         MM_COPY_ADDRESS_VIRTUAL,
                                         &Length );
    KdpBreakpointTable[ BreakpointHandle ].Flags &= ~KD_BPE_SET;

    KeUnstackDetachProcess( &ApcState );

KdpProcedureDone:
    Reciprocate.MaximumLength = 0;
    Reciprocate.Length = sizeof( DBGKD_MANIPULATE_STATE64 );
    Reciprocate.Buffer = ( PCHAR )Packet;

    return KdDebugDevice.KdSendPacket( KdTypeStateManipulate,
                                       &Reciprocate,
                                       NULL,
                                       &KdpContext );
}
