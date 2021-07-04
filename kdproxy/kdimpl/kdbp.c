
#include <kd.h>

KD_BREAKPOINT_ENTRY KdpBreakpointTable[ KD_BREAKPOINT_TABLE_LENGTH ];

static ULONG BreakpointCodeLength = 2;
static UCHAR BreakpointCode[ ] = {
    0xCD, 0x02
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
    Reciprocate.MaximumLength = sizeof( DBGKD_MANIPULATE_STATE64 );
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

    if ( BreakpointHandle >= KD_BREAKPOINT_TABLE_LENGTH ) {

        Packet->ReturnStatus = STATUS_UNSUCCESSFUL;
        goto KdpProcedureDone;
    }

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
    Reciprocate.MaximumLength = sizeof( DBGKD_MANIPULATE_STATE64 );
    Reciprocate.Length = sizeof( DBGKD_MANIPULATE_STATE64 );
    Reciprocate.Buffer = ( PCHAR )Packet;

    return KdDebugDevice.KdSendPacket( KdTypeStateManipulate,
                                       &Reciprocate,
                                       NULL,
                                       &KdpContext );
}
