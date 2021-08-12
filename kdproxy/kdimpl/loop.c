
#include <kd.h>
#include "../hde64/hde64.h"

UCHAR KdpMessageBuffer[ 0x1000 ];

BOOLEAN
KdPollBreakIn(

)
{
    if ( KdpContext.BreakRequested == TRUE ) {

        KdpContext.BreakRequested = FALSE;
        return TRUE;
    }

    return KdDebugDevice.KdReceivePacket( KdTypeCheckQueue,
                                          NULL,
                                          NULL,
                                          NULL,
                                          &KdpContext ) == KdStatusSuccess;
}

VOID
KdpSetCommonState(
    _In_    ULONG32                  ApiNumber,
    _In_    PCONTEXT                 Context,
    _Inout_ PDBGKD_WAIT_STATE_CHANGE Change
)
{
    //
    // TODO: This procedure also deletes the breakpoint range at
    //       the context's rip.
    //

    ULONG64 InstructionCount;
    ULONG32 CurrentHandle;

    Change->ApiNumber = ApiNumber;
    Change->Processor = ( USHORT )KdGetCurrentPrcbNumber( );
    Change->ProcessorCount = KeQueryActiveProcessorCountEx( 0xFFFF );
    if ( Change->ProcessorCount <= Change->Processor ) {

        Change->ProcessorCount = Change->Processor + 1;
    }

    Change->ProcessorLevel = *KeProcessorLevel;
    Change->CurrentThread = ( ULONG64 )KeGetCurrentThread( );

    RtlZeroMemory( &Change->ControlReport, sizeof( DBGKD_CONTROL_REPORT ) );

    //
    // This is important breakpoint handling code,
    // this will set a stub-bp directly after the previous instruction,
    // this will be caught, and the old bp will be re-added.
    //

    for ( CurrentHandle = 0;
          CurrentHandle < KD_BREAKPOINT_TABLE_LENGTH;
          CurrentHandle++ ) {

        if ( KdpBreakpointTable[ CurrentHandle ].Flags & KD_BPE_SET ) {

            if ( KdpBreakpointTable[ CurrentHandle ].Address ==
                 Context->Rip - KdpBreakpointCodeLength ) {

                __try {
                    RtlCopyMemory( ( void* )KdpBreakpointTable[ CurrentHandle ].Address,
                        ( void* )KdpBreakpointTable[ CurrentHandle ].Content,
                                   KdpBreakpointCodeLength );

                    RtlCopyMemory( ( void* )( KdpBreakpointTable[ CurrentHandle ].Address +
                                              KdpBreakpointTable[ CurrentHandle ].ContentLength ),
                                   KdpBreakpointCode,
                                   KdpBreakpointCodeLength );
                }
                __except ( TRUE ) {

                    NT_ASSERT( FALSE );
                }

                KeSweepLocalCaches( );
                Context->Rip -= KdpBreakpointCodeLength;
                break;
            }
        }
    }

    Change->ProgramCounter = Context->Rip;

    __try {
        RtlCopyMemory( Change->ControlReport.InstructionStream, ( PVOID )Context->Rip, 0x10 );
        InstructionCount = 0x10;
    }
    __except ( EXCEPTION_EXECUTE_HANDLER ) {

        InstructionCount = 0;
    }
    Change->ControlReport.InstructionCount = ( USHORT )InstructionCount;

}

VOID
KdpSetContextState(
    _Inout_ PDBGKD_WAIT_STATE_CHANGE Change,
    _In_    PCONTEXT                 Context
)
{
    Context;
    // TODO: Must call real KdpSetContextState.
    // dr's come from Prcb->ProcessorState.

    Change->ControlReport.Dr6 = KdGetPrcbSpecialRegisters( KeGetCurrentPrcb( ) )->KernelDr6;
    Change->ControlReport.Dr7 = KdGetPrcbSpecialRegisters( KeGetCurrentPrcb( ) )->KernelDr7;

    Change->ControlReport.SegCs = Context->SegCs;
    Change->ControlReport.SegDs = Context->SegDs;
    Change->ControlReport.SegEs = Context->SegEs;
    Change->ControlReport.SegFs = Context->SegFs;

    Change->ControlReport.EFlags = Context->EFlags;
    Change->ControlReport.ReportFlags = 1;

    if ( Context->SegCs == 0x10 ||//KdDebuggerDataBlock.GdtR0Code ||
         Context->SegCs == 0x33 ) {//KdDebuggerDataBlock.GdtR3Code ) {

        Change->ControlReport.ReportFlags = 0x3;
    }
}

BOOLEAN
KdpSendWaitContinue(
    _In_ ULONG64  Unused,
    _In_ PSTRING  StateChangeHead,
    _In_ PSTRING  StateChangeBody,
    _In_ PCONTEXT Context
)
{
    Context;
    Unused;

    //
    // This function prototype is matched with the nt
    // function, the first parameter is infact unreferenced.
    // 
    // Some things are quite different later down, but it still
    // works fine with WinDbg.
    //

    KD_STATUS                Status;
    STRING                   Head;
    STRING                   Body;
    ULONG32                  Length;
    DBGKD_MANIPULATE_STATE64 Packet;

    Head.MaximumLength = sizeof( DBGKD_MANIPULATE_STATE64 );
    Head.Length = 0;
    Head.Buffer = ( PCHAR )&Packet;

    Body.MaximumLength = 0x1000;
    Body.Length = 0;
    Body.Buffer = ( PCHAR )&KdpMessageBuffer;

KdpResendPacket:
    KdDebugDevice.KdSendPacket( KdTypeStateChange,
                                StateChangeHead,
                                StateChangeBody,
                                &KdpContext );

    while ( !KdDebuggerNotPresent_ ) {

        while ( 1 ) {

            RtlZeroMemory( &Packet, sizeof( DBGKD_MANIPULATE_STATE64 ) );

            Head.MaximumLength = sizeof( DBGKD_MANIPULATE_STATE64 );
            Head.Length = 0;

            Body.MaximumLength = 0x1000;
            Body.Length = 0;

            Status = KdDebugDevice.KdReceivePacket( KdTypeStateManipulate,
                                                    &Head,
                                                    &Body,
                                                    &Length,
                                                    &KdpContext );
            if ( Status == KdStatusResend ) {

                //
                // This seems a little weird, but this is the same as 
                // nt!KdpSendWaitContinue, if a resend is requested at any point
                // the state is sent, I believe it is for when the debugger
                // is restarted maybe? Not sure.
                //

                DbgPrint( "Resend!\n" );

                goto KdpResendPacket;
            }

#if KD_RECV_PACKET_LOGGING
            CHAR DbgKdApi[ DbgKdMaximumManipulate - DbgKdMinimumManipulate ][ 48 ] = {
                "DbgKdReadVirtualMemoryApi",
                "DbgKdWriteVirtualMemoryApi",
                "DbgKdGetContextApi",
                "DbgKdSetContextApi",
                "DbgKdWriteBreakPointApi",
                "DbgKdRestoreBreakPointApi",
                "DbgKdContinueApi",
                "DbgKdReadControlSpaceApi",
                "DbgKdWriteControlSpaceApi",
                "DbgKdReadIoSpaceApi",
                "DbgKdWriteIoSpaceApi",
                "DbgKdRebootApi",
                "DbgKdContinueApi2",
                "DbgKdReadPhysicalMemoryApi",
                "DbgKdWritePhysicalMemoryApi",
                "DbgKdQuerySpecialCallsApi",
                "DbgKdSetSpecialCallApi",
                "DbgKdClearSpecialCallsApi",
                "DbgKdSetInternalBreakPointApi",
                "DbgKdGetInternalBreakPointApi",
                "DbgKdReadIoSpaceExtendedApi",
                "DbgKdWriteIoSpaceExtendedApi",
                "DbgKdGetVersionApi",
                "DbgKdWriteBreakPointExApi",
                "DbgKdRestoreBreakPointExApi",
                "DbgKdCauseBugCheckApi",
                "DbgKdPlaceholder1",
                "DbgKdPlaceholder2",
                "DbgKdPlaceholder3",
                "DbgKdPlaceholder4",
                "DbgKdPlaceholder5",
                "DbgKdPlaceholder6",
                "DbgKdSwitchProcessor",
                "DbgKdPageInApi",
                "DbgKdReadMachineSpecificRegister",
                "DbgKdWriteMachineSpecificRegister",
                "OldVlm1",
                "OldVlm2",
                "DbgKdSearchMemoryApi",
                "DbgKdGetBusDataApi",
                "DbgKdSetBusDataApi",
                "DbgKdCheckLowMemoryApi",
                "DbgKdClearAllInternalBreakpointsApi",
                "DbgKdFillMemoryApi",
                "DbgKdQueryMemoryApi",
                "DbgKdSwitchPartition",
                "DbgKdGetContextEx",
                "DbgKdSetContextEx",
                "DbgKdWriteCustomBreakpointEx",
                "DbgKdReadPhysicalMemoryLong",
            };
#endif

            if ( Status == KdStatusSuccess &&
                 Packet.ApiNumber >= DbgKdMinimumManipulate &&
                 Packet.ApiNumber < DbgKdMaximumManipulate ) {

                switch ( Packet.ApiNumber ) {
                case DbgKdReadVirtualMemoryApi:
                    KdpReadVirtualMemory( &Packet,
                                          &Body );
                    break;
                case DbgKdWriteVirtualMemoryApi:
                    KdpWriteVirtualMemory( &Packet,
                                           &Body );
                    break;
                case DbgKdGetContextApi:
                    KdpGetContext( &Packet,
                                   &Body,
                                   Context );
                    break;
                case DbgKdSetContextApi:
                    KdpSetContext( &Packet,
                                   &Body,
                                   Context );
                    break;
                case DbgKdWriteBreakPointApi:
                    KdpAddBreakpoint( &Packet,
                                      &Body );
                    break;
                case DbgKdRestoreBreakPointApi:
                    KdpDeleteBreakpoint( &Packet,
                                         &Body );
                    break;
                case DbgKdContinueApi:
                    return NT_SUCCESS( Packet.u.Continue.ContinueStatus );
                case DbgKdReadControlSpaceApi:
                    KdpReadControlSpace( &Packet,
                                         &Body );
                    break;
                case DbgKdWriteControlSpaceApi:
                    KdpWriteControlSpace( &Packet,
                                          &Body );
                    break;
                case DbgKdReadIoSpaceApi:
                    KdpReadIoSpace( &Packet,
                                    &Body );
                    break;
                case DbgKdWriteIoSpaceApi:
                    KdpWriteIoSpace( &Packet,
                                     &Body );
                    break;
                case DbgKdRebootApi:
                case DbgKdCauseBugCheckApi:

                    //
                    // Set the nig mode. 
                    //
#if !(KD_DEBUG_NO_FREEZE)
                    KdThawProcessors( );
#endif

                    KeBugCheck( D3DNIG_MAXIMUM );
                    break;
                case DbgKdContinueApi2:
                    if ( !NT_SUCCESS( Packet.u.Continue2.ContinueStatus ) ) {

                        return FALSE;
                    }

                    if ( Packet.u.Continue2.ControlSet.TraceFlag ) {

                        KdSetTracepoint( Context );
                    }

                    if ( Packet.u.Continue2.ControlSet.Dr7 != 0x400 &&
                         Packet.u.Continue2.ControlSet.Dr7 != 0 ) {

                        KdPrint( "WARNING: hardware breakpoints are not supported.\n" );
                    }
#if 0
                    for ( Processor = 0;
                          Processor < KeQueryActiveProcessorCountEx( 0xFFFF );
                          Processor++ ) {

                        KdGetPrcbSpecialRegisters( KeQueryPrcbAddress( Processor ) )->KernelDr6 = 0;
                        KdGetPrcbSpecialRegisters( KeQueryPrcbAddress( Processor ) )->KernelDr7 =
                            Packet.u.Continue2.ControlSet.Dr7;
                    }
#endif

                    //KdpGetStateChange( &Packet,
                    //                   Context );
                    return TRUE;
                case DbgKdReadPhysicalMemoryApi:
                    KdpReadPhysicalMemory( &Packet,
                                           &Body );
                    break;
                case DbgKdWritePhysicalMemoryApi:
                    KdpWritePhysicalMemory( &Packet,
                                            &Body );
                    break;
                case DbgKdGetVersionApi:
                    KdpGetVersion( &Packet,
                                   &Body );
                    break;
                case DbgKdSetSpecialCallApi:
                case DbgKdClearSpecialCallsApi:
                case DbgKdSetInternalBreakPointApi:
                case DbgKdClearAllInternalBreakpointsApi:
                    continue;
                case DbgKdGetContextEx:
                    KdpGetContextEx( &Packet,
                                     &Body,
                                     Context );
                    break;
                case DbgKdSetContextEx:
                    KdpSetContextEx( &Packet,
                                     &Body,
                                     Context );
                    break;
                case DbgKdQueryMemoryApi:
                    KdpQueryMemory( &Packet,
                                    &Body );
                    break;
                case DbgKdSwitchProcessor:

                    if ( Packet.Processor == KeGetCurrentProcessorNumber( ) ) {

                        goto KdpResendPacket;
                    }

                    //KeSetTargetProcessorDpc( &KdBreakDpc, ( CCHAR )Packet.Processor );
                    //KeInsertQueueDpc( &KdBreakDpc, NULL, NULL );

                    //KeSwitchFrozenProcessor( Packet.Processor );

                    //while ( 1 );

                    KdSwitchFrozenProcessor( Packet.Processor );
                    return TRUE;
                default:
                    DbgPrint( "KdApiNumber unhandled: %#.4lx\n", Packet.ApiNumber );
                    Packet.ReturnStatus = STATUS_UNSUCCESSFUL;
                    Head.Buffer = ( PCHAR )&Packet;
                    Head.Length = sizeof( DBGKD_MANIPULATE_STATE64 );
                    KdDebugDevice.KdSendPacket( KdTypeStateManipulate,
                                                &Head,
                                                NULL,
                                                &KdpContext );
                    //KdPrint( "KdApiNumber unhandled: %#.4lx\n", Packet.ApiNumber );
                    break;
                }

#if KD_RECV_PACKET_LOGGING
                DbgPrint( "%s %lx\n",
                          DbgKdApi[ Packet.ApiNumber - DbgKdMinimumManipulate ],
                          Packet.ReturnStatus );
#endif

            }

#if KD_DEBUG_NO_FREEZE
            //LARGE_INTEGER Time;
            //Time.QuadPart = -500000;
            //KeDelayExecutionThread( KernelMode, FALSE, &Time );
#else
            //KeStallExecutionProcessor( 500 * 1000 );
#endif
        }

        KdDebugDevice.KdSendPacket( KdTypeStateChange,
                                    StateChangeHead,
                                    StateChangeBody,
                                    &KdpContext );
    }

    return TRUE;
}
