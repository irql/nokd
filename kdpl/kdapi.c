
#include <kdpl.h>

ULONG32             KdTransportMaxPacketSize = 0xFA0; // 4000.

KD_STATUS
KdpReadVirtualMemory(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body
)
{
    STRING  Reciprocate;
    ULONG64 ReadCount;

    ReadCount = Packet->u.ReadMemory.TransferCount;
    ReadCount = min( KdTransportMaxPacketSize - sizeof( DBGKD_MANIPULATE_STATE64 ), ReadCount );
    ReadCount = min( Body->MaximumLength, ReadCount );

    Packet->ReturnStatus = DbgKdMmCopyMemory( Body->Buffer,
        ( PVOID )Packet->u.ReadMemory.TargetBaseAddress,
                                              ReadCount,
                                              MM_COPY_MEMORY_VIRTUAL,
                                              &ReadCount );

    Body->Length = ( USHORT )ReadCount;
    Packet->u.ReadMemory.ActualBytesRead = ( unsigned int )ReadCount;

    Reciprocate.MaximumLength = sizeof( DBGKD_MANIPULATE_STATE64 );
    Reciprocate.Length = sizeof( DBGKD_MANIPULATE_STATE64 );
    Reciprocate.Buffer = ( PCHAR )Packet;

    return KdDebugDevice.KdSendPacket( KdTypeStateManipulate,
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
    STRING  Reciprocate;
    ULONG64 Length;

    Packet->ReturnStatus = DbgKdMmCopyMemory( ( PVOID )Packet->u.WriteMemory.TargetBaseAddress,
                                              Body->Buffer,
                                              Body->Length,
                                              MM_COPY_MEMORY_VIRTUAL,
                                              &Length );
    Packet->u.WriteMemory.TransferCount = ( unsigned int )Length;

    Reciprocate.MaximumLength = sizeof( DBGKD_MANIPULATE_STATE64 );
    Reciprocate.Length = sizeof( DBGKD_MANIPULATE_STATE64 );
    Reciprocate.Buffer = ( PCHAR )Packet;

    return KdDebugDevice.KdSendPacket( KdTypeStateManipulate,
                                       &Reciprocate,
                                       NULL,
                                       &KdpContext );
}

VOID
KdpGetBaseContext(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body,
    _Inout_ PCONTEXT                  Context
)
{
    ULONG32 Length;

    if ( Packet->Processor != DbgKdQueryCurrentPrcbNumber( ) ) {

        Context = DbgKdQueryProcessorContext( Packet->Processor );
    }

    Length = sizeof( CONTEXT );
    Length += 15;

    if ( Length <= Body->MaximumLength ) {

        RtlCopyMemory( Body->Buffer, Context, Length );
        Body->Length = ( USHORT )Length;
        Packet->ReturnStatus = STATUS_SUCCESS;
    }
    else {

        Body->Length = 0;
        Packet->ReturnStatus = STATUS_UNSUCCESSFUL;
    }
}

KD_STATUS
KdpGetContext(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body,
    _Inout_ PCONTEXT                  Context
)
{
    STRING  Reciprocate;

    KdpGetBaseContext( Packet, Body, Context );

    Reciprocate.MaximumLength = sizeof( DBGKD_MANIPULATE_STATE64 );
    Reciprocate.Length = sizeof( DBGKD_MANIPULATE_STATE64 );
    Reciprocate.Buffer = ( PCHAR )Packet;

    return KdDebugDevice.KdSendPacket( KdTypeStateManipulate,
                                       &Reciprocate,
                                       Body,
                                       &KdpContext );
}

KD_STATUS
KdpGetContextEx(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body,
    _Inout_ PCONTEXT                  Context
)
{
    STRING  Reciprocate;
    ULONG64 BodyLength;
    ULONG64 BodyOffset;

    Reciprocate.Length = sizeof( DBGKD_MANIPULATE_STATE64 );
    Reciprocate.MaximumLength = sizeof( DBGKD_MANIPULATE_STATE64 );
    Reciprocate.Buffer = ( PCHAR )Packet;

    KdpGetBaseContext( Packet, Body, Context );

    Packet->u.GetContextEx.BytesCopied = 0;

    if ( NT_SUCCESS( Packet->ReturnStatus ) ) {

        BodyOffset = Body->Length;

        if ( Packet->u.GetContextEx.Offset < BodyOffset ) {

            BodyOffset = Packet->u.GetContextEx.Offset;
        }

        BodyLength = Body->Length - BodyOffset;
        if ( Packet->u.GetContextEx.ByteCount <= BodyLength ) {

            BodyLength = Packet->u.GetContextEx.ByteCount;
        }

        if ( BodyOffset != 0 && BodyLength != 0 ) {

            RtlMoveMemory( Body->Buffer,
                           Body->Buffer + BodyOffset,
                           BodyLength );
        }

        Packet->u.GetContextEx.Offset = ( unsigned int )BodyOffset;
        Packet->u.GetContextEx.ByteCount = Body->Length;
        Packet->u.GetContextEx.BytesCopied = ( unsigned int )BodyLength;
        Body->Length = ( USHORT )BodyLength;
    }

    return KdDebugDevice.KdSendPacket( KdTypeStateManipulate,
                                       &Reciprocate,
                                       Body,
                                       &KdpContext );
}

KD_STATUS
KdpSetContext(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body,
    _Inout_ PCONTEXT                  Context
)
{
    STRING Reciprocate;

    Reciprocate.Length = sizeof( DBGKD_MANIPULATE_STATE64 );
    Reciprocate.MaximumLength = sizeof( DBGKD_MANIPULATE_STATE64 );
    Reciprocate.Buffer = ( PCHAR )Packet;

    //
    // They have the field Packet.u.SetContext.ContextFlags, but 
    // it's not used or even referenced.
    //

    if ( Packet->Processor != DbgKdQueryCurrentPrcbNumber( ) ) {

        Context = DbgKdQueryProcessorContext( Packet->Processor );
    }

    RtlCopyMemory( Context, Body->Buffer, Body->Length );
    Packet->ReturnStatus = STATUS_SUCCESS;

    return KdDebugDevice.KdSendPacket( KdTypeStateManipulate,
                                       &Reciprocate,
                                       NULL,
                                       &KdpContext );
}

KD_STATUS
KdpSetContextEx(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body,
    _Inout_ PCONTEXT                  Context
)
{

    STRING Reciprocate;

    Reciprocate.Length = sizeof( DBGKD_MANIPULATE_STATE64 );
    Reciprocate.MaximumLength = sizeof( DBGKD_MANIPULATE_STATE64 );
    Reciprocate.Buffer = ( PCHAR )Packet;

    //
    // Windows use a global var as an intermediate buffer
    // for the context.
    //
    // TODO: Basic range checks are done by windows.
    //

    if ( Packet->Processor != DbgKdQueryCurrentPrcbNumber( ) ) {

        Context = DbgKdQueryProcessorContext( Packet->Processor );
    }

    RtlCopyMemory( ( PCHAR )Context + Packet->u.SetContextEx.Offset,
                   Body->Buffer, Packet->u.SetContextEx.ByteCount );
    Packet->u.SetContextEx.BytesCopied = Packet->u.SetContextEx.ByteCount;
    Packet->ReturnStatus = STATUS_SUCCESS;

    return KdDebugDevice.KdSendPacket( KdTypeStateManipulate,
                                       &Reciprocate,
                                       NULL,
                                       &KdpContext );
}

KD_STATUS
KdpGetStateChangeApi(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body,
    _Inout_ PCONTEXT                  Context
)
{
    Packet;
    Body;
    Context;
    return KdStatusSuccess;
}

KD_STATUS
KdpReadControlSpace(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body
)
{
    STRING  Reciprocate;
    PVOID   ControlSpace;
    ULONG64 Pcr;
    ULONG64 Prcb;
    ULONG64 ReadCount;
    ULONG64 MaximumLength;
    ULONG64 Pointer;

    Pcr = ( ULONG64 )DbgKdQueryPcr( );
    Prcb = Pcr + 0x180;

    Packet->ReturnStatus = STATUS_SUCCESS;

    Reciprocate.MaximumLength = sizeof( DBGKD_MANIPULATE_STATE64 );
    Reciprocate.Length = sizeof( DBGKD_MANIPULATE_STATE64 );
    Reciprocate.Buffer = ( PCHAR )Packet;

    MaximumLength = 8;

    ReadCount = Packet->u.ReadMemory.TransferCount;
    ReadCount = min( KdTransportMaxPacketSize - sizeof( DBGKD_MANIPULATE_STATE64 ), ReadCount );
    ReadCount = min( Body->MaximumLength, ReadCount );

    switch ( Packet->u.ReadMemory.TargetBaseAddress ) {
    case AMD64_DEBUG_CONTROL_SPACE_PCR:
        ControlSpace = &Pcr;
        break;
    case AMD64_DEBUG_CONTROL_SPACE_PRCB:
        ControlSpace = &Prcb;
        break;
    case AMD64_DEBUG_CONTROL_SPACE_KSPECIAL:
        MaximumLength = sizeof( KSPECIAL_REGISTERS );

        ControlSpace = DbgKdQuerySpecialRegisters( DbgKdQueryCurrentProcessorNumber( ) );
        break;
    case AMD64_DEBUG_CONTROL_SPACE_THREAD:
        //
        // ProcessorPrcb->CurrentThread
        //
        Pointer = Pcr + 0x188;
        ControlSpace = &Pointer;
        break;
    default:
        ControlSpace = NULL; // Compiler
        NT_ASSERT( FALSE );
    }

    if ( MaximumLength > ReadCount ) {

        MaximumLength = ReadCount;
    }

    Packet->u.ReadMemory.ActualBytesRead = ( ULONG32 )MaximumLength;
    Body->Length = ( USHORT )MaximumLength;

    if ( Packet->u.ReadMemory.TargetBaseAddress == AMD64_DEBUG_CONTROL_SPACE_KSPECIAL ) {

        RtlCopyMemory( Body->Buffer, ControlSpace, MaximumLength );
    }
    else {
        DbgKdMmCopyMemory( Body->Buffer,
                           ControlSpace,
                           MaximumLength,
                           MM_COPY_MEMORY_VIRTUAL,
                           &ReadCount );
        Packet->u.ReadMemory.ActualBytesRead = ( ULONG32 )ReadCount;
    }

    return KdDebugDevice.KdSendPacket( KdTypeStateManipulate,
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
    STRING              Reciprocate;
    PVOID               ControlSpace;
    ULONG64             Pcr;
    ULONG64             Prcb;
    ULONG64             ReadCount;
    ULONG64             MaximumLength;
    ULONG64             Pointer;
    PKSPECIAL_REGISTERS Special;

    Pcr = ( ULONG64 )DbgKdQueryPcr( );
    Prcb = Pcr + 0x180;

    Packet->ReturnStatus = STATUS_SUCCESS;

    Reciprocate.MaximumLength = sizeof( DBGKD_MANIPULATE_STATE64 );
    Reciprocate.Length = sizeof( DBGKD_MANIPULATE_STATE64 );
    Reciprocate.Buffer = ( PCHAR )Packet;

    MaximumLength = 8;

    ReadCount = Packet->u.ReadMemory.TransferCount;
    ReadCount = min( KdTransportMaxPacketSize - sizeof( DBGKD_MANIPULATE_STATE64 ), ReadCount );
    ReadCount = min( Body->MaximumLength, ReadCount );

    if (Packet->u.ReadMemory.TargetBaseAddress != AMD64_DEBUG_CONTROL_SPACE_KSPECIAL) {

        // had some issues with this function, this is actually in the kernel to return unsuccessful 
        // when it's not type 2, so I figured I'd just have it here too, 
        // only real use is to set KSPECIAL tbh 
        Packet->ReturnStatus = STATUS_UNSUCCESSFUL;
        return KdDebugDevice.KdSendPacket(KdTypeStateManipulate,
                                          &Reciprocate,
                                          Body,
                                          &KdpContext);
    }

    switch ( Packet->u.ReadMemory.TargetBaseAddress ) {
    case AMD64_DEBUG_CONTROL_SPACE_PCR:
        ControlSpace = ( PVOID )Pcr;
        //ControlSpace = &Pcr;
        break;
    case AMD64_DEBUG_CONTROL_SPACE_PRCB:
        ControlSpace = ( PVOID )Prcb;
        //ControlSpace = &Prcb;
        break;
    case AMD64_DEBUG_CONTROL_SPACE_KSPECIAL:
        //
        // ProcessorPrcb->ProcessorState
        //
        // Must be emulated because this can be considered part of the
        // context record.
        //

        MaximumLength = sizeof( KSPECIAL_REGISTERS );

        Special = DbgKdQuerySpecialRegisters( DbgKdQueryCurrentProcessorNumber( ) );
        ControlSpace = ( PVOID )Special;
        //ControlSpace = &Special;
        break;
    case AMD64_DEBUG_CONTROL_SPACE_THREAD:
        //
        // ProcessorPrcb->CurrentThread
        //
        Pointer = Pcr + 0x188;
        ControlSpace = ( PVOID )Pointer;
        //ControlSpace = &Pointer;
        break;
    default:
        ControlSpace = NULL; // Compiler
        NT_ASSERT( FALSE );
    }

    if ( MaximumLength > ReadCount ) {

        MaximumLength = ReadCount;
    }

    Packet->u.ReadMemory.ActualBytesRead = ( ULONG32 )MaximumLength;
    Body->Length = ( USHORT )MaximumLength;

    if ( Packet->u.ReadMemory.TargetBaseAddress == AMD64_DEBUG_CONTROL_SPACE_KSPECIAL ) {

        RtlCopyMemory( ControlSpace, Body->Buffer, MaximumLength );
    }
    else {
        DbgKdMmCopyMemory( ControlSpace,
                           Body->Buffer,
                           MaximumLength,
                           MM_COPY_MEMORY_VIRTUAL,
                           &ReadCount );
        Packet->u.ReadMemory.ActualBytesRead = ( ULONG32 )ReadCount;
    }

    // oddly enough, this does send the body back?
    return KdDebugDevice.KdSendPacket( KdTypeStateManipulate,
                                       &Reciprocate,
                                       Body,
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

    //
    // TODO: probably allow the DbgKd to do this, and dont do it here!
    // 
    // Although it is very rare that this will be needed.
    //

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

    Reciprocate.MaximumLength = sizeof( DBGKD_MANIPULATE_STATE64 );
    Reciprocate.Length = sizeof( DBGKD_MANIPULATE_STATE64 );
    Reciprocate.Buffer = ( PCHAR )Packet;

    return KdDebugDevice.KdSendPacket( KdTypeStateManipulate,
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

    Reciprocate.MaximumLength = sizeof( DBGKD_MANIPULATE_STATE64 );
    Reciprocate.Length = sizeof( DBGKD_MANIPULATE_STATE64 );
    Reciprocate.Buffer = ( PCHAR )Packet;

    return KdDebugDevice.KdSendPacket( KdTypeStateManipulate,
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
    STRING  Reciprocate;
    ULONG64 ReadCount;

    ReadCount = Packet->u.ReadMemory.TransferCount;
    ReadCount = min( KdTransportMaxPacketSize - sizeof( DBGKD_MANIPULATE_STATE64 ), ReadCount );
    ReadCount = min( Body->MaximumLength, ReadCount );

    Packet->ReturnStatus = DbgKdMmCopyMemory( Body->Buffer,
        ( PVOID )Packet->u.ReadMemory.TargetBaseAddress,
                                              ReadCount,
                                              MM_COPY_MEMORY_PHYSICAL,
                                              &ReadCount );

    Body->Length = ( USHORT )ReadCount;
    Packet->u.ReadMemory.ActualBytesRead = ( unsigned int )ReadCount;

    Reciprocate.MaximumLength = sizeof( DBGKD_MANIPULATE_STATE64 );
    Reciprocate.Length = sizeof( DBGKD_MANIPULATE_STATE64 );
    Reciprocate.Buffer = ( PCHAR )Packet;

    return KdDebugDevice.KdSendPacket( KdTypeStateManipulate,
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

    ReadCount = Packet->u.WriteMemory.TransferCount;
    ReadCount = min( KdTransportMaxPacketSize - sizeof( DBGKD_MANIPULATE_STATE64 ), ReadCount );
    ReadCount = min( Body->MaximumLength, ReadCount );

    Packet->ReturnStatus = DbgKdMmCopyMemory( ( PVOID )Packet->u.WriteMemory.TargetBaseAddress,
                                              Body->Buffer,
                                              ReadCount,
                                              MM_COPY_MEMORY_VIRTUAL,
                                              &ReadCount );

    Packet->u.WriteMemory.ActualBytesWritten = ( unsigned int )ReadCount;

    Reciprocate.MaximumLength = sizeof( DBGKD_MANIPULATE_STATE64 );
    Reciprocate.Length = sizeof( DBGKD_MANIPULATE_STATE64 );
    Reciprocate.Buffer = ( PCHAR )Packet;

    return KdDebugDevice.KdSendPacket( KdTypeStateManipulate,
                                       &Reciprocate,
                                       NULL,
                                       &KdpContext );

}

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

    Reciprocate.MaximumLength = sizeof( DBGKD_MANIPULATE_STATE64 );
    Reciprocate.Length = sizeof( DBGKD_MANIPULATE_STATE64 );
    Reciprocate.Buffer = ( PCHAR )Packet;

    return KdDebugDevice.KdSendPacket( KdTypeStateManipulate,
                                       &Reciprocate,
                                       NULL,
                                       &KdpContext );
}

KD_STATUS
KdpQueryMemory(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body
)
{
    Body;

    STRING Reciprocate;

    if ( Packet->u.QueryMemory.AddressSpace != 0 ) {

        //
        // Bit weird, but okay windows.
        //

        Packet->ReturnStatus = STATUS_INVALID_PARAMETER;
    }
    else {

        if ( Packet->u.QueryMemory.Address >= 0x7FFFFFFEFFFF ) {

            //
            // TODO: We can replace MmIsSessionAddress in the future, we
            //       can use KdDebuggerData.MmSessionBase - KdDebuggerData.MmSessionSize
            //       or hardcode FFFFF900`00000000 to FFFFF97F`FFFFFFFF
            //

            if ( FALSE ) {
                //if ( MmIsSessionAddress( Packet->u.QueryMemory.Address )  ) {

                Packet->u.QueryMemory.AddressSpace = SessionSpace;
            }
            else {

                Packet->u.QueryMemory.AddressSpace = SystemSpace;
            }
        }
        else {

            Packet->u.QueryMemory.AddressSpace = UserSpace;
        }

        Packet->u.QueryMemory.Flags = 7;
        Packet->ReturnStatus = STATUS_SUCCESS;
    }

    //
    // Windows sets this to zero for some reason.
    //

    Packet->u.QueryMemory.Reserved = 0;

    Reciprocate.Length = sizeof( DBGKD_MANIPULATE_STATE64 );
    Reciprocate.MaximumLength = sizeof( DBGKD_MANIPULATE_STATE64 );
    Reciprocate.Buffer = ( PCHAR )Packet;

    return KdDebugDevice.KdSendPacket( KdTypeStateManipulate,
                                       &Reciprocate,
                                       NULL,
                                       &KdpContext );
}

KD_STATUS
KdpInsertBreakpoint(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body
)
{
    Body;

    STRING  Reciprocate;
    ULONG32 Handle;
    ULONG32 CurrentBreakpoint;

    Handle = ( ULONG32 )( -1 );
    for ( CurrentBreakpoint = 0;
          CurrentBreakpoint < KD_BREAKPOINT_TABLE_LENGTH;
          CurrentBreakpoint++ ) {

        if ( ( KdpBreakpointTable[ CurrentBreakpoint ].Flags & KD_BPE_SET ) == 0 ) {

            Handle = CurrentBreakpoint;
            break;
        }
    }

    if ( Handle == ( ULONG32 )( -1 ) ) {

        Packet->ReturnStatus = STATUS_UNSUCCESSFUL;
        goto DbgKdpProcedureDone;
    }

    Packet->ReturnStatus = STATUS_SUCCESS;

    KdpBreakpointTable[ Handle ].Address = Packet->u.WriteBreakPoint.BreakPointAddress;
    KdpBreakpointTable[ Handle ].Flags = KD_BPE_SET;

    Packet->u.WriteBreakPoint.BreakPointHandle = Handle;

    DbgKdInsertBreakpoint( KdpBreakpointTable[ Handle ].Address );
DbgKdpProcedureDone:

    Reciprocate.MaximumLength = sizeof( DBGKD_MANIPULATE_STATE64 );
    Reciprocate.Length = sizeof( DBGKD_MANIPULATE_STATE64 );
    Reciprocate.Buffer = ( PCHAR )Packet;

    return KdDebugDevice.KdSendPacket( KdTypeStateManipulate,
                                       &Reciprocate,
                                       NULL,
                                       &KdpContext );
}

KD_STATUS
KdpRemoveBreakpoint(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body
)
{
    Body;

    STRING Reciprocate;

    if ( Packet->u.RestoreBreakPoint.BreakPointHandle >= KD_BREAKPOINT_TABLE_LENGTH ) {

        Packet->ReturnStatus = STATUS_UNSUCCESSFUL;
    }
    else if ( KdpBreakpointTable[ Packet->u.RestoreBreakPoint.BreakPointHandle ].Flags & KD_BPE_SET ) {

        DbgKdRemoveBreakpoint( KdpBreakpointTable[ Packet->u.RestoreBreakPoint.BreakPointHandle ].Address );

        KdpBreakpointTable[ Packet->u.RestoreBreakPoint.BreakPointHandle ].Flags &= ~KD_BPE_SET;
        Packet->ReturnStatus = STATUS_SUCCESS;
    }
    else {

        Packet->ReturnStatus = STATUS_SUCCESS;
    }

    Reciprocate.MaximumLength = sizeof( DBGKD_MANIPULATE_STATE64 );
    Reciprocate.Length = sizeof( DBGKD_MANIPULATE_STATE64 );
    Reciprocate.Buffer = ( PCHAR )Packet;

    return KdDebugDevice.KdSendPacket( KdTypeStateManipulate,
                                       &Reciprocate,
                                       NULL,
                                       &KdpContext );
}
