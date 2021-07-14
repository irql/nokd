
#include <kd.h>
#include "../hde64/hde64.h"

KD_BREAKPOINT_ENTRY KdpBreakpointTable[ KD_BREAKPOINT_TABLE_LENGTH ] = { 0 };

ULONG KdpBreakpointCodeLength = 2;
UCHAR KdpBreakpointCode[ ] = {
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
    ULONG32 CurrentBreakpoint;
    ULONG32 BreakpointHandle;
#if !(KD_SET_NMI_DPL)
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
#endif

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
    KdpBreakpointTable[ BreakpointHandle ].Flags = 0;

    if ( MmIsAddressValid( ( PVOID )KdpBreakpointTable[ BreakpointHandle ].Address ) ) {

        __try {

            RtlCopyMemory( ( void* )KdpBreakpointTable[ BreakpointHandle ].Content,
                ( void* )KdpBreakpointTable[ BreakpointHandle ].Address,
                           0x20 );

            RtlCopyMemory( ( void* )KdpBreakpointTable[ BreakpointHandle ].Address,
                           KdpBreakpointCode,
                           KdpBreakpointCodeLength );

            KeSweepLocalCaches( );

            Packet->ReturnStatus = STATUS_SUCCESS;
        }
        __except ( TRUE ) {

            DbgPrint( "write failed cr0=%x!\n", __readcr0( ) );
            Packet->ReturnStatus = STATUS_UNSUCCESSFUL;
            goto KdpProcedureDone;
        }
    }
    else {

        //
        // This memory is most-likely paged-out, but yes, it could
        // be invalid instead of paged-out, although that's irrelevant.
        //

#if KD_PAGED_MEMORY_FIX
        KdpOweBreakpoint++;
        KdOwePrepare( );
#endif
        Packet->ReturnStatus = STATUS_SUCCESS;
        KdpBreakpointTable[ BreakpointHandle ].Flags |= KD_BPE_OWE;
    }

    Packet->u.WriteBreakPoint.BreakPointHandle = BreakpointHandle;

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

    BreakpointHandle = Packet->u.RestoreBreakPoint.BreakPointHandle;

    if ( BreakpointHandle >= KD_BREAKPOINT_TABLE_LENGTH ) {

        Packet->ReturnStatus = STATUS_UNSUCCESSFUL;
        goto KdpProcedureDone;
    }

    if ( ( KdpBreakpointTable[ BreakpointHandle ].Flags & KD_BPE_SET ) == 0 ) {

        Packet->ReturnStatus = STATUS_SUCCESS;
        goto KdpProcedureDone;
    }

    if ( ( KdpBreakpointTable[ BreakpointHandle ].Flags & KD_BPE_OWE ) == KD_BPE_OWE ) {

        KdpOweBreakpoint--;
        KdOwePrepare( );

        Packet->ReturnStatus = STATUS_SUCCESS;
        goto KdpProcedureDone;
    }

    KdpBreakpointTable[ BreakpointHandle ].Flags &= ~KD_BPE_SET;

    Packet->ReturnStatus = STATUS_SUCCESS;

    __try {

        KdCopyProcessSpace( ( void* )KdpBreakpointTable[ BreakpointHandle ].Process,
            ( void* )KdpBreakpointTable[ BreakpointHandle ].Address,
                            KdpBreakpointTable[ BreakpointHandle ].Content,
                            KdpBreakpointTable[ BreakpointHandle ].ContentLength +
                            KdpBreakpointCodeLength );
        KeSweepLocalCaches( );
    }
    __except ( TRUE ) {

        goto KdpProcedureDone;
    }

KdpProcedureDone:
    Reciprocate.MaximumLength = sizeof( DBGKD_MANIPULATE_STATE64 );
    Reciprocate.Length = sizeof( DBGKD_MANIPULATE_STATE64 );
    Reciprocate.Buffer = ( PCHAR )Packet;

    return KdDebugDevice.KdSendPacket( KdTypeStateManipulate,
                                       &Reciprocate,
                                       NULL,
                                       &KdpContext );
}
