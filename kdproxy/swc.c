
#include "kd.h"

UCHAR KdpMessageBuffer[ 0x1000 ];

BOOLEAN
KdPollBreakIn(

)
{
    return KdReceivePacket( KdTypePollBreakin,
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
    SIZE_T InstructionCount;

    Change->ApiNumber = ApiNumber;
    Change->Processor = ( USHORT )KeGetCurrentProcessorNumber( );
    Change->ProcessorCount = KeQueryActiveProcessorCountEx( 0xFFFF );
    Change->ProcessorLevel = *KeProcessorLevel;
    Change->CurrentThread = ( ULONG64 )KeGetCurrentThread( );
    Change->ProgramCounter = Context->Rip;

    RtlZeroMemory( &Change->ControlReport, sizeof( DBGKD_CONTROL_REPORT ) );

    MmCopyMemory( Change->ControlReport.InstructionStream,
        ( PVOID )Context->Rip,
                  0x10,
                  MM_COPY_ADDRESS_VIRTUAL,
                  &InstructionCount );
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

    Change->ControlReport.Dr6 = 0;
    Change->ControlReport.Dr7 = 0;
    Change->ControlReport.SegCs = 0x10;
    Change->ControlReport.ReportFlags = 0x3;
}

BOOLEAN
KdpSendWaitContinue(
    _In_ ULONG64  Unused,
    _In_ PSTRING  StateChangeHead,
    _In_ PSTRING  StateChangeBody,
    _In_ PCONTEXT Context
)
{
    Unused;

    //
    // Tried to keep this routine as close to the
    // real one inside ntoskrnl, the first parameter
    // is completely unused?
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
    KdSendPacket( KdTypeStateChange,
                  StateChangeHead,
                  StateChangeBody,
                  &KdpContext );

    while ( !KdDebuggerNotPresent_ ) {

        while ( 1 ) {

            Status = KdReceivePacket( KdTypeStateManipulate,
                                      &Head,
                                      &Body,
                                      &Length,
                                      &KdpContext );
            if ( Status == KdStatusResend ) {

                goto KdpResendPacket;
            }

            if ( Status != KdStatusTimeOut ) {

                switch ( Packet.ApiNumber ) {
                case KdApiReadMemory:
                    KdpReadVirtualMemory( &Packet,
                                          &Body );
                    break;
                case KdApiWriteMemory:
                    KdpWriteVirtualMemory( &Packet,
                                           &Body );
                    break;
                case KdApiGetContext:
                    KdpGetContext( &Packet,
                                   &Body );
                    break;
                case KdApiSetContext:
                    KdpSetContext( &Packet,
                                   &Body );
                    break;
                case KdApiWriteBreakPoint:
                    KdpAddBreakpoint( &Packet,
                                      &Body );
                    break;
                case KdApiRestoreBreakPoint:
                    KdpDeleteBreakpoint( &Packet,
                                         &Body );
                    break;
                case KdApiContinue:
                    return NT_SUCCESS( Packet.u.Continue.ContinueStatus );
                case KdApiReadControlSpace:
                    KdpReadControlSpace( &Packet,
                                         &Body );
                    break;
                case KdApiWriteControlSpace:
                    KdpWriteControlSpace( &Packet,
                                          &Body );
                    break;
                case KdApiReadIoSpace:
                    KdpReadIoSpace( &Packet,
                                    &Body );
                    break;
                case KdApiWriteIoSpace:
                    KdpWriteIoSpace( &Packet,
                                     &Body );
                    break;
                case KdApiReboot:

                    break;
                case KdApiContinueStateChange:
                    if ( !NT_SUCCESS( Packet.u.Continue.ContinueStatus ) ) {

                        return FALSE;
                    }
                    KdpGetStateChange( &Packet,
                                       Context );
                    return TRUE;
                case KdApiReadPhysicalMemory:
                    KdpReadPhysicalMemory( &Packet,
                                           &Body );
                    break;
                case KdApiWritePhysicalMemory:
                    KdpWritePhysicalMemory( &Packet,
                                            &Body );
                    break;
                case KdApiGetVersion:
                    KdpGetVersion( &Packet,
                                   &Body );
                    break;
                default:
                    DbgPrint( "sobby api %#.8lx!", Packet.ApiNumber );
                    Packet.ReturnStatus = STATUS_UNSUCCESSFUL;
                    Head.Buffer = ( PCHAR )&Packet;
                    Head.Length = sizeof( DBGKD_MANIPULATE_STATE64 );
                    KdSendPacket( KdTypeStateManipulate,
                                  &Head,
                                  NULL,
                                  &KdpContext );
                    break;
                }
            }

        }

    }

    return TRUE;
}
