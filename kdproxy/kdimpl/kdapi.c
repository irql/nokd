
#include <kd.h>

ULONG32             KdTransportMaxPacketSize = 0xFA0; // 4000.

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
#if 0
    Packet->ReturnStatus = MmCopyMemory( Body->Buffer,
        ( PVOID )Packet->u.ReadMemory.TargetBaseAddress,
                                         ReadCount,
                                         MM_COPY_ADDRESS_VIRTUAL,
                                         &ReadCount );
#endif

    /*
    PAGE_FAULT_IN_NONPAGED_AREA (50)
        Invalid system memory was referenced.  This cannot be protected by try-except.
        Typically the address is just plain bad or it is pointing at freed memory.
    */

    //
    // Also the reason I don't use MmCopyMemory or MmCopyVirtualMemory
    // is because they won't read stuff like the UserSharedData
    //
    // To account for this, we're going to validate the memory
    // referenced. Oh well, just remembered MmIsAddressValid exists
    // and that works fine to check if I'll #PF.
    // 
    // MmNonPagedPoolStart, MmNonPagedPoolEnd
    //

    if ( !MmIsAddressValid( ( PVOID )Packet->u.ReadMemory.TargetBaseAddress ) ||
        ( Packet->u.ReadMemory.TargetBaseAddress >= 0xFFFF8000'00000000 &&
          Packet->u.ReadMemory.TargetBaseAddress <= 0xFFFFA000'00000000 ) ) {

        Packet->ReturnStatus = STATUS_UNSUCCESSFUL;
        Body->Length = 0;
        Packet->u.ReadMemory.ActualBytesRead = 0;
    }
    else {

        Packet->ReturnStatus = STATUS_SUCCESS;
        __try {
            memcpy( Body->Buffer,
                ( void* )Packet->u.ReadMemory.TargetBaseAddress,
                    ReadCount );
            Body->Length = ( USHORT )ReadCount;
            Packet->u.ReadMemory.ActualBytesRead = ( unsigned int )ReadCount;
        }
        __except ( EXCEPTION_EXECUTE_HANDLER ) {

            Packet->ReturnStatus = STATUS_UNSUCCESSFUL;
            Body->Length = 0;
            Packet->u.ReadMemory.ActualBytesRead = 0;
        }
    }

    Reciprocate.MaximumLength = sizeof( DBGKD_MANIPULATE_STATE64 );
    Reciprocate.Length = sizeof( DBGKD_MANIPULATE_STATE64 );
    Reciprocate.Buffer = ( PCHAR )Packet;
#if 1
    DbgPrint( "KdpReadVirtualMemory: %p %d %lx\n",
              Packet->u.ReadMemory.TargetBaseAddress,
              ReadCount,
              Packet->ReturnStatus );
#endif

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
    STRING Reciprocate;
#if 0
    SIZE_T Length;

    Packet->ReturnStatus = MmCopyMemory( ( PVOID )Packet->u.WriteMemory.TargetBaseAddress,
                                         Body->Buffer,
                                         Body->Length,
                                         MM_COPY_ADDRESS_VIRTUAL,
                                         &Length );
    Packet->u.WriteMemory.TransferCount = ( unsigned int )Length;
#endif
    if ( !MmIsAddressValid( ( PVOID )Packet->u.ReadMemory.TargetBaseAddress ) ) {

        Packet->ReturnStatus = STATUS_UNSUCCESSFUL;
        Packet->u.WriteMemory.TransferCount = 0;
    }
    else {
        //
        // Currently, not bothered to make this HVCI compliant,
        // it's very easy to setup some PTE mechanism, similar
        // to MmDbg.
        //

        Packet->ReturnStatus = STATUS_SUCCESS;
        __writecr0( __readcr0( ) & ~0x10000 );

        __try {
            memcpy( ( void* )Packet->u.WriteMemory.TargetBaseAddress,
                    Body->Buffer,
                    Body->Length );
            Packet->u.WriteMemory.TransferCount = ( unsigned int )Body->Length;
        }
        __except ( EXCEPTION_EXECUTE_HANDLER ) {
            Packet->ReturnStatus = STATUS_UNSUCCESSFUL;
            Packet->u.WriteMemory.TransferCount = 0;
        }

        __writecr0( __readcr0( ) | 0x10000 );
    }

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

    if ( Packet->Processor != KeGetCurrentProcessorNumber( ) ) {

        Context = ( PCONTEXT )( KeQueryPrcbAddress( Packet->Processor ) + KdDebuggerDataBlock.OffsetPrcbContext );
    }

    // This is actually 32 bytes higher. but is then lowered to sizeof(CONTEXT),
    // if there's no CONTEXT_XSTATE.
    Length = sizeof( CONTEXT );

    if ( ( Context->ContextFlags & CONTEXT_XSTATE ) == CONTEXT_XSTATE ) {

        // wont be set by me, so we don't really need to care.
        Length = 0xFFFFFF;
        //Length = SharedUserData->XState.Size + 0x320;
    }

    // why?    
    //Length += 15;

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

    if ( Packet->Processor != KeGetCurrentProcessorNumber( ) ) {

        Context = ( PCONTEXT )( KeQueryPrcbAddress( Packet->Processor ) + KdDebuggerDataBlock.OffsetPrcbContext );
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

    if ( Packet->Processor != KeGetCurrentProcessorNumber( ) ) {

        Context = ( PCONTEXT )( KeQueryPrcbAddress( Packet->Processor ) + KdDebuggerDataBlock.OffsetPrcbContext );
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

    Reciprocate.MaximumLength = sizeof( DBGKD_MANIPULATE_STATE64 );
    Reciprocate.Length = sizeof( DBGKD_MANIPULATE_STATE64 );
    Reciprocate.Buffer = ( PCHAR )Packet;

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
    STRING Reciprocate;

    Packet->ReturnStatus = KdpSysWriteControlSpace( Packet->Processor,
        ( ULONG )Packet->u.WriteMemory.TargetBaseAddress,
                                                    Body->Buffer,
                                                    Body->Length,
                                                    ( PULONG )&Packet->u.WriteMemory.ActualBytesWritten );

    Reciprocate.MaximumLength = sizeof( DBGKD_MANIPULATE_STATE64 );
    Reciprocate.Length = sizeof( DBGKD_MANIPULATE_STATE64 );
    Reciprocate.Buffer = ( PCHAR )Packet;

    return KdDebugDevice.KdSendPacket( KdTypeStateManipulate,
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
    STRING Reciprocate;
    SIZE_T ReadCount;
    PVOID  Buffer;

    ReadCount = Packet->u.ReadMemory.TransferCount;
    ReadCount = min( KdTransportMaxPacketSize - sizeof( DBGKD_MANIPULATE_STATE64 ), ReadCount );
    ReadCount = min( Body->MaximumLength, ReadCount );
#if 0
    Packet->ReturnStatus = MmCopyMemory( Body->Buffer,
        ( PVOID )Packet->u.ReadMemory.TargetBaseAddress,
                                         ReadCount,
                                         MM_COPY_ADDRESS_PHYSICAL,
                                         &ReadCount );
#endif

    Buffer = MmMapIoSpace( *( PPHYSICAL_ADDRESS )&Packet->u.ReadMemory.TargetBaseAddress,
                           ReadCount,
                           MmCached );

    if ( Buffer == NULL ) {

        Packet->ReturnStatus = STATUS_UNSUCCESSFUL;
        goto KdpProcedureDone;
    }

    __try {
        memcpy( Body->Buffer,
                Buffer,
                Body->Length );
        Packet->u.ReadMemory.TransferCount = ( unsigned int )Body->Length;
        Packet->ReturnStatus = STATUS_SUCCESS;
    }
    __except ( EXCEPTION_EXECUTE_HANDLER ) {
        Packet->ReturnStatus = STATUS_UNSUCCESSFUL;
        Packet->u.ReadMemory.TransferCount = 0;
    }

    Body->Length = ( USHORT )ReadCount;
    Packet->u.ReadMemory.ActualBytesRead = ( unsigned int )ReadCount;
    MmUnmapIoSpace( Buffer, ReadCount );

KdpProcedureDone:
    Reciprocate.MaximumLength = sizeof( DBGKD_MANIPULATE_STATE64 );
    Reciprocate.Length = sizeof( DBGKD_MANIPULATE_STATE64 );
    Reciprocate.Buffer = ( PCHAR )Packet;

    if ( !NT_SUCCESS( Packet->ReturnStatus ) ) {

        DbgPrint( "failed to read physical memory: %p\n", Packet->u.ReadMemory.TargetBaseAddress );
    }

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
    PVOID  Buffer;

    ReadCount = Packet->u.WriteMemory.TransferCount;
    ReadCount = min( KdTransportMaxPacketSize - sizeof( DBGKD_MANIPULATE_STATE64 ), ReadCount );
    ReadCount = min( Body->MaximumLength, ReadCount );

    Buffer = MmMapIoSpace( *( PPHYSICAL_ADDRESS )&Packet->u.WriteMemory.TargetBaseAddress,
                           ReadCount,
                           MmCached );

    if ( Buffer == NULL ) {

        Packet->ReturnStatus = STATUS_UNSUCCESSFUL;
        goto KdpProcedureDone;
    }

#if 0
    Packet->ReturnStatus = MmCopyMemory( Buffer,
                                         Body->Buffer,
                                         ReadCount,
                                         MM_COPY_ADDRESS_VIRTUAL,
                                         &ReadCount );
#endif

    __try {
        memcpy( Buffer,
                Body->Buffer,
                Body->Length );
        Packet->u.WriteMemory.TransferCount = ( unsigned int )Body->Length;
        Packet->ReturnStatus = STATUS_SUCCESS;
    }
    __except ( EXCEPTION_EXECUTE_HANDLER ) {
        Packet->ReturnStatus = STATUS_UNSUCCESSFUL;
        Packet->u.WriteMemory.TransferCount = 0;
    }

    Packet->u.WriteMemory.ActualBytesWritten = ( unsigned int )ReadCount;
    MmUnmapIoSpace( Buffer, ReadCount );

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

            if ( MmIsSessionAddress( Packet->u.QueryMemory.Address ) ) {

                Packet->u.QueryMemory.AddressSpace = SessionSpace;
            }
            else {

                Packet->u.QueryMemory.AddressSpace = SystemSpace;
            }
        }
        else {

            Packet->u.QueryMemory.AddressSpace = UserSpace;
        }

        DbgPrint( "%p -> %s\n", Packet->u.QueryMemory.Address,
            ( ( char*[ ] ){ "UserSpace", "SessionSpace", "SystemSpace" } )[ Packet->u.QueryMemory.AddressSpace ] );

        Packet->u.QueryMemory.Flags = 7;
        Packet->ReturnStatus = STATUS_SUCCESS;
    }

    //
    // Windows sets this to zero for some reason.
    //

    Packet->u.QueryMemory.Reserved = 0;

    Reciprocate.Length = sizeof( DBGKD_MANIPULATE_STATE64 );
    Reciprocate.MaximumLength = sizeof( DBGKD_MANIPULATE_STATE64 );
    Reciprocate.Buffer = ( PCHAR )&Packet;

    return KdDebugDevice.KdSendPacket( KdTypeStateManipulate,
                                       &Reciprocate,
                                       NULL,
                                       &KdpContext );
}
