
#include "kd.h"

ULONG32             KdTransportMaxPacketSize = 0xFA0; // 4000.
KD_BREAKPOINT_ENTRY KdpBreakpointTable[ KD_BREAKPOINT_TABLE_LENGTH ];

#if 0
KDAPI
NTSTATUS
KdpCopyMemoryChunks(
    _In_      PVOID  Source,
    _In_      PVOID  Destination,
    _In_      ULONG  TotalLength,
    _In_      ULONG  ChunkLength,
    _In_      ULONG  Flags,
    _Out_opt_ PULONG LengthRead
)
{


}
#endif


KD_STATUS
KdpReadVirtualMemory(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body
)
{
    //
    // This function has a seemingly optimized out parameter,
    // the caller inside KdpSendWaitContinue, does not pass any 
    // meaningful value, whilst the function utilizes an r8 home
    // address, though this is unreferenced.
    //

    STRING Reciprocate;
    SIZE_T ReadCount;

    ReadCount = Packet->u.ReadMemory.TransferCount;
    ReadCount = min( KdTransportMaxPacketSize - sizeof( DBGKD_MANIPULATE_STATE64 ), ReadCount );
    ReadCount = min( Body->MaximumLength, ReadCount );

    Packet->ReturnStatus = MmCopyMemory( Body->Buffer,
        ( PVOID )Packet->u.ReadMemory.TargetBaseAddress,
                                         ReadCount,
                                         MM_COPY_ADDRESS_VIRTUAL,
                                         &ReadCount );
    Body->Length = ( USHORT )ReadCount;
    Packet->u.ReadMemory.ActualBytesRead = ( unsigned int )ReadCount;

    Reciprocate.MaximumLength = 0;
    Reciprocate.Length = sizeof( DBGKD_MANIPULATE_STATE64 );
    Reciprocate.Buffer = ( PCHAR )Packet;

    return KdSendPacket( KdTypeStateManipulate,
                         &Reciprocate,
                         Body,
                         &KdpContext );
}

KD_STATUS
KdpWriteVirtualMemory(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body
)
{
    STRING Reciprocate;
    SIZE_T Length;

    Packet->ReturnStatus = MmCopyMemory( ( PVOID )Packet->u.WriteMemory.TargetBaseAddress,
                                         Body->Buffer,
                                         Body->Length,
                                         MM_COPY_ADDRESS_VIRTUAL,
                                         &Length );
    Packet->u.WriteMemory.TransferCount = ( unsigned int )Length;

    Reciprocate.MaximumLength = 0;
    Reciprocate.Length = sizeof( DBGKD_MANIPULATE_STATE64 );
    Reciprocate.Buffer = ( PCHAR )Packet;

    return KdSendPacket( KdTypeStateManipulate,
                         &Reciprocate,
                         NULL,
                         &KdpContext );
}

KD_STATUS
KdpGetContext(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body
)
{

    //
    // Currently unimplemented, I think this function is 
    // dependant on some PCRB shit, alongside the 
    // Freeze/Thaw apis. This means we may need to sig it.
    //

    Packet;
    Body;

    return KdStatusOkay;
}

KD_STATUS
KdpSetContext(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body
)
{

    //
    // Same applies
    //

    Packet;
    Body;

    return KdStatusOkay;
}

static ULONG BreakpointCodeLength = 1;
static UCHAR BreakpointCode[ ] = { 0xCC };

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

    BreakpointHandle = ( ULONG32 )-1;
    for ( CurrentBreakpoint = 0;
          CurrentBreakpoint < 256;
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

    return KdSendPacket( KdTypeStateManipulate,
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

    BreakpointHandle = Packet->u.RestoreBreakPoint.BreakPointHandle;

    if ( ( KdpBreakpointTable[ BreakpointHandle ].Flags & KD_BPE_SET ) == 0 ) {

        Packet->ReturnStatus = STATUS_UNSUCCESSFUL;
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

    return KdSendPacket( KdTypeStateManipulate,
                         &Reciprocate,
                         NULL,
                         &KdpContext );
}

KD_STATUS
KdpReadControlSpace(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body
)
{
    STRING Reciprocate;
    ULONG  ReadCount;

    ReadCount = Packet->u.ReadMemory.TransferCount;
    ReadCount = min( KdTransportMaxPacketSize - sizeof( DBGKD_MANIPULATE_STATE64 ), ReadCount );
    ReadCount = min( Body->MaximumLength, ReadCount );

    //
    // Address is an enumeration of types,
    // and this function reads things such
    // as control registers, debug registers,
    // segment registers, etc.
    //

    Packet->ReturnStatus = KdpSysReadControlSpace( Packet->Processor,
        ( ULONG )Packet->u.ReadMemory.TargetBaseAddress,
                                                   Body->Buffer,
                                                   ReadCount,
                                                   ( PULONG )&Packet->u.ReadMemory.ActualBytesRead );
    if ( Packet->u.ReadMemory.ActualBytesRead < ReadCount ) {

        ReadCount = Packet->u.ReadMemory.ActualBytesRead;
    }
    Body->Length = ( USHORT )ReadCount;

    Reciprocate.MaximumLength = 0;
    Reciprocate.Length = sizeof( DBGKD_MANIPULATE_STATE64 );
    Reciprocate.Buffer = ( PCHAR )Packet;

    return KdSendPacket( KdTypeStateManipulate,
                         &Reciprocate,
                         Body,
                         &KdpContext );
}

KD_STATUS
KdpWriteControlSpace(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body
)
{
    STRING Reciprocate;

    Packet->ReturnStatus = KdpSysWriteControlSpace( Packet->Processor,
        ( ULONG )Packet->u.WriteMemory.TargetBaseAddress,
                                                    Body->Buffer,
                                                    Body->Length,
                                                    ( PULONG )&Packet->u.WriteMemory.ActualBytesWritten );

    Reciprocate.MaximumLength = 0;
    Reciprocate.Length = sizeof( DBGKD_MANIPULATE_STATE64 );
    Reciprocate.Buffer = ( PCHAR )Packet;

    return KdSendPacket( KdTypeStateManipulate,
                         &Reciprocate,
                         NULL,
                         &KdpContext );
}

KD_STATUS
KdpReadIoSpace(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body
)
{
    Body;

    STRING Reciprocate;
    USHORT Address;

    Address = ( USHORT )Packet->u.ReadWriteIo.IoAddress;

    switch ( Packet->u.ReadWriteIo.DataSize ) {
    case 1:
        Packet->u.ReadWriteIo.DataValue = __inbyte( Address );
        Packet->ReturnStatus = STATUS_SUCCESS;
        break;
    case 2:
        if ( ( Address & 1 ) == 0 ) {

            Packet->u.ReadWriteIo.DataValue = __inword( Address );
            Packet->ReturnStatus = STATUS_SUCCESS;
        }
        else {

            Packet->ReturnStatus = STATUS_DATATYPE_MISALIGNMENT;
        }
        break;
    case 4:
        if ( ( Address & 3 ) == 0 ) {

            Packet->u.ReadWriteIo.DataValue = __inword( Address );
            Packet->ReturnStatus = STATUS_SUCCESS;
        }
        else {

            Packet->ReturnStatus = STATUS_DATATYPE_MISALIGNMENT;
        }
        break;
    default:
        Packet->ReturnStatus = STATUS_INVALID_PARAMETER;
        break;
    }

    Reciprocate.MaximumLength = 0;
    Reciprocate.Length = sizeof( DBGKD_MANIPULATE_STATE64 );
    Reciprocate.Buffer = ( PCHAR )Packet;

    return KdSendPacket( KdTypeStateManipulate,
                         &Reciprocate,
                         NULL,
                         &KdpContext );
}

KD_STATUS
KdpWriteIoSpace(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body
)
{
    Body;

    STRING Reciprocate;
    USHORT Address;

    Address = ( USHORT )Packet->u.ReadWriteIo.IoAddress;

    switch ( Packet->u.ReadWriteIo.DataSize ) {
    case 1:
        __outbyte( Address, ( UCHAR )Packet->u.ReadWriteIo.DataValue );
        Packet->ReturnStatus = STATUS_SUCCESS;
        break;
    case 2:
        if ( ( Address & 1 ) == 0 ) {

            __outword( Address, ( USHORT )Packet->u.ReadWriteIo.DataValue );
            Packet->ReturnStatus = STATUS_SUCCESS;
        }
        else {

            Packet->ReturnStatus = STATUS_DATATYPE_MISALIGNMENT;
        }
        break;
    case 4:
        if ( ( Address & 3 ) == 0 ) {

            __outdword( Address, ( ULONG )Packet->u.ReadWriteIo.DataValue );
            Packet->ReturnStatus = STATUS_SUCCESS;
        }
        else {

            Packet->ReturnStatus = STATUS_DATATYPE_MISALIGNMENT;
        }
        break;
    default:
        Packet->ReturnStatus = STATUS_INVALID_PARAMETER;
        break;
    }

    Reciprocate.MaximumLength = 0;
    Reciprocate.Length = sizeof( DBGKD_MANIPULATE_STATE64 );
    Reciprocate.Buffer = ( PCHAR )Packet;

    return KdSendPacket( KdTypeStateManipulate,
                         &Reciprocate,
                         NULL,
                         &KdpContext );
}

KD_STATUS
KdpReadPhysicalMemory(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body
)
{
    STRING Reciprocate;
    SIZE_T ReadCount;

    ReadCount = Packet->u.ReadMemory.TransferCount;
    ReadCount = min( KdTransportMaxPacketSize - sizeof( DBGKD_MANIPULATE_STATE64 ), ReadCount );
    ReadCount = min( Body->MaximumLength, ReadCount );

    Packet->ReturnStatus = MmCopyMemory( Body->Buffer,
        ( PVOID )Packet->u.ReadMemory.TargetBaseAddress,
                                         ReadCount,
                                         MM_COPY_ADDRESS_PHYSICAL,
                                         &ReadCount );
    Body->Length = ( USHORT )ReadCount;
    Packet->u.ReadMemory.ActualBytesRead = ( unsigned int )ReadCount;

    Reciprocate.MaximumLength = 0;
    Reciprocate.Length = sizeof( DBGKD_MANIPULATE_STATE64 );
    Reciprocate.Buffer = ( PCHAR )Packet;

    return KdSendPacket( KdTypeStateManipulate,
                         &Reciprocate,
                         Body,
                         &KdpContext );
}

KD_STATUS
KdpWritePhysicalMemory(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body
)
{
    //
    // This real ntoskrnl version of this function
    // handles packet with ApiNumber 0x3162, this is the same,
    // however, the buffer is run length encoded.
    //

    STRING Reciprocate;
    SIZE_T ReadCount;
    PVOID  Buffer;

    ReadCount = Packet->u.ReadMemory.TransferCount;
    ReadCount = min( KdTransportMaxPacketSize - sizeof( DBGKD_MANIPULATE_STATE64 ), ReadCount );
    ReadCount = min( Body->MaximumLength, ReadCount );

    Buffer = MmMapIoSpace( *( PPHYSICAL_ADDRESS )&Packet->u.ReadMemory.TargetBaseAddress,
                           ReadCount,
                           MmCached );

    if ( Buffer == NULL ) {

        Packet->ReturnStatus = STATUS_UNSUCCESSFUL;
        goto KdpProcedureDone;
    }

    Packet->ReturnStatus = MmCopyMemory( Buffer,
                                         Body->Buffer,
                                         ReadCount,
                                         MM_COPY_ADDRESS_PHYSICAL,
                                         &ReadCount );
    Body->Length = ( USHORT )ReadCount;
    Packet->u.ReadMemory.ActualBytesRead = ( unsigned int )ReadCount;
    MmUnmapIoSpace( Buffer, ReadCount );

KdpProcedureDone:
    Reciprocate.MaximumLength = 0;
    Reciprocate.Length = sizeof( DBGKD_MANIPULATE_STATE64 );
    Reciprocate.Buffer = ( PCHAR )Packet;

    return KdSendPacket( KdTypeStateManipulate,
                         &Reciprocate,
                         Body,
                         &KdpContext );

}

//
//

KD_STATUS
KdpGetVersion(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body
)
{
    Body;

    STRING Reciprocate;

    Packet->u.GetVersion64 = KdVersionBlock;
    Packet->ReturnStatus = STATUS_SUCCESS;

    Reciprocate.MaximumLength = 0;
    Reciprocate.Length = sizeof( DBGKD_MANIPULATE_STATE64 );
    Reciprocate.Buffer = ( PCHAR )Packet;

    return KdSendPacket( KdTypeStateManipulate,
                         &Reciprocate,
                         NULL,
                         &KdpContext );
}
