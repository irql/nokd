
#include <kdgdb.h>

DBG_CORE_DEVICE        DbgKdpCommDevice;

ULONG32                KdCompNextPacketIdToSend;
ULONG32                KdCompPacketIdExpected;
ULONG32                KdCompRetryCount = 5;
ULONG32                KdCompNumberRetries = 5;

ULONG32                KdComBailPrintString = 0;
ULONG32                KdComBailLoadSymbols = 0;
ULONG32                KdComBailCreateFile = 0;
ULONG32                KdComBailTraceIo = 0;

ULONG32                KdTransportMaxPacketSize = 0xFA0;

DBGKD_BREAKPOINT_ENTRY DbgKdBreakpointTable[ DBGKD_BP_MAXIMUM ];

DBGKD_DEBUG_STATE      DbgKdpDebugEmu = {
    { 0 },
{ 0 },
{ 0 },
{ 0 },
{ 0xFFFF0FF0 },
{ 0x400 } };

ULONG32
DbgKdChecksum(
    _In_ PUCHAR  Buffer,
    _In_ ULONG32 Length
)
{
    ULONG32 Checksum;

    Checksum = 0;

    while ( Length-- ) {

        Checksum += *Buffer++;
    }

    return Checksum;
}

DBG_STATUS
DbgKdSendPacket(
    _In_     KD_PACKET_TYPE PacketType,
    _In_     PSTRING        Head,
    _In_opt_ PSTRING        Body,
    _Inout_  PKD_CONTEXT    KdContext
);

DBG_STATUS
DbgKdRecvPacket(
    _In_    KD_PACKET_TYPE PacketType,
    _Inout_ PSTRING        Head,
    _Inout_ PSTRING        Body,
    _Inout_ PKD_CONTEXT    KdContext
);

DBG_STATUS
DbgKdInit(

)
{

    //
    // TODO: Generate VM LUID for pipe. LUID can be gdb port/server/combo
    //

    DbgPipeInit( &DbgKdpCommDevice,
                 "\\\\.\\pipe\\nokd",
                 TRUE );

    KdCompNextPacketIdToSend = 0x80800800;
    KdCompPacketIdExpected   = 0x80800000;

    return DBG_STATUS_OKAY;
}

DBG_STATUS
DbgKdSendPacket(
    _In_     KD_PACKET_TYPE PacketType,
    _In_     PSTRING        Head,
    _In_opt_ PSTRING        Body,
    _Inout_  PKD_CONTEXT    KdContext
)
{
    KD_PACKET  Packet;
    DBG_STATUS Status;

    Packet.Checksum =
        ( Head == NULL ? 0 : DbgKdChecksum( Head->Buffer, Head->Length ) ) +
        ( Body == NULL ? 0 : DbgKdChecksum( Body->Buffer, Body->Length ) );
    Packet.PacketLeader = KD_LEADER_PACKET;
    Packet.PacketLength =
        ( Head == NULL ? 0 : Head->Length ) +
        ( Body == NULL ? 0 : Body->Length );
    Packet.PacketType = ( USHORT )PacketType;
    Packet.PacketId = KdCompNextPacketIdToSend;

    KdCompNumberRetries = KdCompRetryCount;

    do {

        if ( KdCompNumberRetries == 0 ) {

            switch ( PacketType ) {
            case KdTypePrint:

                if ( *( ULONG32* )Head->Buffer == 0x3230 ) {

                    KdComBailPrintString++;
                    goto DbgKdBailed;
                }
                break;
            case KdTypeStateChange:
                if ( *( ULONG32* )Head->Buffer == 0x3031 ) {

                    KdComBailLoadSymbols++;
                    goto DbgKdBailed;
                }
                break;
            case KdTypeFileIo:
                if ( *( ULONG32* )Head->Buffer == 0x3430 ) {

                    KdComBailCreateFile++;
                    goto DbgKdBailed;
                }
                break;
            case KdTypeTraceIo:
                if ( *( ULONG32* )Head->Buffer == 0x3330 ) {

                    KdComBailTraceIo++;
                    goto DbgKdBailed;
                }
                break;
            DbgKdBailed:
                KdCompPacketIdExpected = 0x80800000;
                KdCompNextPacketIdToSend = 0x80800800;
                return DBG_STATUS_ERROR;
            default:
                break;
            }
        }

        Packet.PacketId = KdCompNextPacketIdToSend;

        DbgKdpCommDevice.DbgSend(
            &DbgKdpCommDevice.Extension,
            &Packet,
            sizeof( KD_PACKET ) );

        if ( Head != NULL && Head->Length != 0 ) {

            DbgKdpCommDevice.DbgSend(
                &DbgKdpCommDevice.Extension,
                Head->Buffer,
                Head->Length );
        }

        if ( Body != NULL && Body->Length != 0 ) {

            DbgKdpCommDevice.DbgSend(
                &DbgKdpCommDevice.Extension,
                Body->Buffer,
                Body->Length );
        }

        DbgKdpCommDevice.DbgSend(
            &DbgKdpCommDevice.Extension,
            "\xAA",
            1 );

        Status = DbgKdRecvPacket( KdTypeAcknowledge, NULL, NULL, KdContext );
        if ( DBG_SUCCESS( Status ) ) {

            break;
        }

        KdCompNumberRetries--;
    } while ( TRUE );

    KdCompNextPacketIdToSend &= ~0x800;
    //KdCompRetryCount = KdContext->RetryCount;

    return DBG_STATUS_OKAY;
}

DBG_STATUS
DbgKdSendControlPacket(
    _In_ KD_PACKET_TYPE PacketType,
    _In_ ULONG32        PacketId
)
{
    KD_PACKET Packet;

    Packet.PacketType = ( USHORT )PacketType;
    Packet.PacketLeader = KD_LEADER_CONTROL;
    Packet.PacketLength = 0;
    Packet.Checksum = 0;
    Packet.PacketId = PacketId;

    return DbgKdpCommDevice.DbgSend(
        &DbgKdpCommDevice.Extension,
        &Packet,
        sizeof( KD_PACKET ) );
}

DBG_STATUS
DbgKdRecvPacket(
    _In_    KD_PACKET_TYPE PacketType,
    _Inout_ PSTRING        Head,
    _Inout_ PSTRING        Body,
    _Inout_ PKD_CONTEXT    KdContext
)
{
    UCHAR      Buffer[ sizeof( KD_PACKET ) ];
    PKD_PACKET PacketBuffer = ( PKD_PACKET )&Buffer;
    ULONG32    Index;
    UCHAR      End;

    Index = 0;

    if ( PacketType == KdTypeCheckQueue ) {

        return ( DBG_SUCCESS( DbgKdpCommDevice.DbgRecv( &DbgKdpCommDevice.Extension,
                                                        Buffer,
                                                        1 ) ) && Buffer[ 0 ] == 0x62 ) ?
            DBG_STATUS_OKAY :
            DBG_STATUS_ERROR;
    }

    //
    // TODO: MUST IMPLEMENT A PROPER TIMEOUT.
    //

    KdCompNumberRetries = KdCompRetryCount;

    while ( TRUE ) {

        do {

            if ( !DBG_SUCCESS( DbgKdpCommDevice.DbgRecv( &DbgKdpCommDevice.Extension,
                                                         Buffer + Index,
                                                         1 ) ) ) {

                Sleep( 10 );
                continue;
            }

            switch ( Buffer[ Index ] ) {
            case KD_LEADER_BREAK_IN_BYTE:
            case KD_LEADER_PACKET_BYTE:
            case KD_LEADER_CONTROL_BYTE:

                if ( Index > 0 && Buffer[ Index ] != Buffer[ 0 ] ) {

                    Index = 0;
                }
                else {

                    Index++;
                }

                break;
            default:

                Index = 0;
                break;
            }

        } while ( Index < 4 );

        /*
        while ( Avail < sizeof( KD_PACKET ) - sizeof( ULONG32 ) ) {

            PeekNamedPipe( DbgKdDevice.Ext.Pipe.Handle, NULL, 0, NULL, &Avail, NULL );
        }

        ReadFile( DbgKdDevice.Ext.Pipe.Handle, Buffer + 4, sizeof( KD_PACKET ) - sizeof( ULONG32 ), &Bytes, NULL );
        */

        DbgKdpCommDevice.DbgRecv( &DbgKdpCommDevice.Extension,
                                  &PacketBuffer->PacketType,
                                  sizeof( PacketBuffer->PacketType ) );

        if ( PacketBuffer->PacketLeader == KD_LEADER_CONTROL &&
             PacketBuffer->PacketType == KdTypeResend ) {

            return DBG_STATUS_RESEND;
        }

        //
        // If there's any error, then all statements should:
        // KdpSendControlPacket( KdTypeResend, PacketId );
        // so long as the received leader is not control
        //

        DbgKdpCommDevice.DbgRecv( &DbgKdpCommDevice.Extension,
                                  &PacketBuffer->PacketLength,
                                  sizeof( PacketBuffer->PacketLength ) );

        DbgKdpCommDevice.DbgRecv( &DbgKdpCommDevice.Extension,
                                  &PacketBuffer->PacketId,
                                  sizeof( PacketBuffer->PacketId ) );

        DbgKdpCommDevice.DbgRecv( &DbgKdpCommDevice.Extension,
                                  &PacketBuffer->Checksum,
                                  sizeof( PacketBuffer->Checksum ) );

        //
        // Wait until the entire packet is transferred into the buffer
        //

#if 0
        printf( "DbgKdRecvPacket:\n\tPacketLeader: %08lx PacketId: %08lx PacketType: %04d\n",
                PacketBuffer->PacketLeader,
                PacketBuffer->PacketId,
                PacketBuffer->PacketType );
#endif

        if ( PacketBuffer->PacketLeader == KD_LEADER_CONTROL ) {

            switch ( PacketBuffer->PacketType ) {
            case KdTypeReset:

                KdCompNextPacketIdToSend = 0x80800000;
                KdCompPacketIdExpected = 0x80800000;

                //
                // With hardware ports, this data would only be received a 
                // single time, the uart 16650 has an intermediate buffer
                // of 16 bytes, so kd can send this packet many times,
                // but the OS is expected to only recieve it once.
                // We need to discard any data in the pipe.
                //

                DbgKdpCommDevice.DbgFlush( &DbgKdpCommDevice.Extension );
                DbgKdSendControlPacket( KdTypeReset, 0 );
                break;
            case KdTypeAcknowledge:

                if ( PacketBuffer->PacketId != ( KdCompNextPacketIdToSend & ~0x800 ) ||
                     PacketType != 4 ) {

                    continue;
                }
                KdCompNextPacketIdToSend ^= 1;
                return DBG_STATUS_OKAY;
            case KdTypeResend:
                break;
            default:
                continue;
            }

            return DBG_STATUS_ERROR;
        }
        else {

            if ( PacketType == KdTypeAcknowledge ) {

                if ( PacketBuffer->PacketId != KdCompPacketIdExpected ) {

                    DbgKdSendControlPacket( KdTypeAcknowledge, PacketBuffer->PacketId );
                    continue;
                }

                DbgKdpCommDevice.DbgFlush( &DbgKdpCommDevice.Extension );
                DbgKdSendControlPacket( KdTypeResend, 0 );
                KdCompNextPacketIdToSend ^= 1;
            }
            else {

                if ( PacketBuffer->PacketLength == 0 ) {

                    return DBG_STATUS_OKAY;
                }
                else {

                    if ( PacketBuffer->PacketLength - Head->Length > Body->MaximumLength ) {

                        DbgKdSendControlPacket( KdTypeResend, 0 );
                        continue;
                    }

                    /*
                  while ( Avail < PacketBuffer->PacketLength ) {

                      PeekNamedPipe( DbgKdDevice.Ext.Pipe.Handle, NULL, 0, NULL, &Avail, NULL );
                      Sleep( 0 );
                  }
                  */

                  //
                  // TODO: Size checks.
                  //

                    if ( Head != NULL ) {

                        if ( PacketBuffer->PacketLength > Head->Length ) {

                            DbgKdpCommDevice.DbgRecv( &DbgKdpCommDevice.Extension,
                                                      Head->Buffer,
                                                      Head->Length );
                            PacketBuffer->PacketLength -= Head->Length;
                        }
                        else {

                            DbgKdpCommDevice.DbgRecv( &DbgKdpCommDevice.Extension,
                                                      Head->Buffer,
                                                      PacketBuffer->PacketLength );
                            return DBG_STATUS_OKAY;
                        }
                    }

                    if ( Body != NULL ) {
                        if ( PacketBuffer->PacketLength > Body->MaximumLength ) {

                            __debugbreak( );
                        }
                        else {

                            DbgKdpCommDevice.DbgRecv( &DbgKdpCommDevice.Extension,
                                                      Body->Buffer,
                                                      PacketBuffer->PacketLength );
                            Body->Length = PacketBuffer->PacketLength;
                            PacketBuffer->PacketLength = 0;
                        }
                    }

                    DbgKdpCommDevice.DbgRecv( &DbgKdpCommDevice.Extension,
                                              &End,
                                              1 );

                    // NOTE IN INIT.C
                    //DiscardPipe( );

                    if ( End != 0xAA ) {

                        DbgKdSendControlPacket( KdTypeResend, PacketBuffer->PacketId );
                        continue;
                    }

                    if ( PacketType != PacketBuffer->PacketType ) {

                        DbgKdSendControlPacket( KdTypeAcknowledge, PacketBuffer->PacketId );
                        continue;
                    }

                    if ( PacketBuffer->PacketId - 0x80800000 > 1 ) {

                        DbgKdSendControlPacket( KdTypeResend, PacketBuffer->PacketId );
                    }

                    if ( PacketBuffer->PacketId != KdCompPacketIdExpected ) {

                        DbgKdSendControlPacket( KdTypeAcknowledge, PacketBuffer->PacketId );
                        continue;
                    }

                    //
                    // CHECKSUM VERIFICATION!
                    //

                    DbgKdSendControlPacket( KdTypeAcknowledge, PacketBuffer->PacketId );
                    KdCompPacketIdExpected ^= 1;
                }
            }

#if 0
            if ( PacketBuffer->PacketId == KdCompPacketIdExpected ) {

                DbgKdSendControlPacket( KdTypeResend, 0 );
                KdCompNextPacketIdToSend ^= 1;
                return DBG_STATUS_ERROR;
            }
            else {

                DbgKdSendControlPacket( KdTypeAcknowledge, PacketBuffer->PacketId );
                continue;
            }
#endif

            return DBG_STATUS_OKAY;
        }

        return DBG_STATUS_ERROR;
    }
}

VOID
DbgKdpApplyContextRecord(
    _Out_ PCONTEXT Context
)
{
    //
    // The registers gathered here are minimal because of performance
    // reading all ~150 or so of the fuckers will crush performance.
    //

    DbgCoreEngine.DbgRegisterWrite( &DbgCoreEngine, Amd64RegisterRax, &Context->Rax );
    DbgCoreEngine.DbgRegisterWrite( &DbgCoreEngine, Amd64RegisterRbx, &Context->Rbx );
    DbgCoreEngine.DbgRegisterWrite( &DbgCoreEngine, Amd64RegisterRcx, &Context->Rcx );
    DbgCoreEngine.DbgRegisterWrite( &DbgCoreEngine, Amd64RegisterRdx, &Context->Rdx );
    DbgCoreEngine.DbgRegisterWrite( &DbgCoreEngine, Amd64RegisterRsi, &Context->Rsi );
    DbgCoreEngine.DbgRegisterWrite( &DbgCoreEngine, Amd64RegisterRdi, &Context->Rdi );
    DbgCoreEngine.DbgRegisterWrite( &DbgCoreEngine, Amd64RegisterRbp, &Context->Rbp );
    DbgCoreEngine.DbgRegisterWrite( &DbgCoreEngine, Amd64RegisterRsp, &Context->Rsp );
    DbgCoreEngine.DbgRegisterWrite( &DbgCoreEngine, Amd64RegisterR8, &Context->R8 );
    DbgCoreEngine.DbgRegisterWrite( &DbgCoreEngine, Amd64RegisterR9, &Context->R9 );
    DbgCoreEngine.DbgRegisterWrite( &DbgCoreEngine, Amd64RegisterR10, &Context->R10 );
    DbgCoreEngine.DbgRegisterWrite( &DbgCoreEngine, Amd64RegisterR11, &Context->R11 );
    DbgCoreEngine.DbgRegisterWrite( &DbgCoreEngine, Amd64RegisterR12, &Context->R12 );
    DbgCoreEngine.DbgRegisterWrite( &DbgCoreEngine, Amd64RegisterR13, &Context->R13 );
    DbgCoreEngine.DbgRegisterWrite( &DbgCoreEngine, Amd64RegisterR14, &Context->R14 );
    DbgCoreEngine.DbgRegisterWrite( &DbgCoreEngine, Amd64RegisterR15, &Context->R15 );

    DbgCoreEngine.DbgRegisterWrite( &DbgCoreEngine, Amd64RegisterRip, &Context->Rip );
    DbgCoreEngine.DbgRegisterWrite( &DbgCoreEngine, Amd64RegisterEFlags, &Context->EFlags );
    DbgCoreEngine.DbgRegisterWrite( &DbgCoreEngine, Amd64RegisterSegCs, &Context->SegCs );
    DbgCoreEngine.DbgRegisterWrite( &DbgCoreEngine, Amd64RegisterSegSs, &Context->SegSs );
    DbgCoreEngine.DbgRegisterWrite( &DbgCoreEngine, Amd64RegisterSegDs, &Context->SegDs );
    DbgCoreEngine.DbgRegisterWrite( &DbgCoreEngine, Amd64RegisterSegEs, &Context->SegEs );
    DbgCoreEngine.DbgRegisterWrite( &DbgCoreEngine, Amd64RegisterSegFs, &Context->SegFs );
    DbgCoreEngine.DbgRegisterWrite( &DbgCoreEngine, Amd64RegisterSegGs, &Context->SegGs );
}

VOID
DbgKdpBuildContextRecord(
    _Out_ PCONTEXT Context
)
{
    //
    // The registers gathered here are minimal because of performance
    // reading all ~150 or so of the fuckers will crush performance.
    //

    Context->ContextFlags = CONTEXT_AMD64 | CONTEXT_INTEGER | CONTEXT_SEGMENTS | CONTEXT_CONTROL;

    DbgCoreEngine.DbgRegisterRead( &DbgCoreEngine, Amd64RegisterRax, &Context->Rax );
    DbgCoreEngine.DbgRegisterRead( &DbgCoreEngine, Amd64RegisterRbx, &Context->Rbx );
    DbgCoreEngine.DbgRegisterRead( &DbgCoreEngine, Amd64RegisterRcx, &Context->Rcx );
    DbgCoreEngine.DbgRegisterRead( &DbgCoreEngine, Amd64RegisterRdx, &Context->Rdx );
    DbgCoreEngine.DbgRegisterRead( &DbgCoreEngine, Amd64RegisterRsi, &Context->Rsi );
    DbgCoreEngine.DbgRegisterRead( &DbgCoreEngine, Amd64RegisterRdi, &Context->Rdi );
    DbgCoreEngine.DbgRegisterRead( &DbgCoreEngine, Amd64RegisterRbp, &Context->Rbp );
    DbgCoreEngine.DbgRegisterRead( &DbgCoreEngine, Amd64RegisterRsp, &Context->Rsp );
    DbgCoreEngine.DbgRegisterRead( &DbgCoreEngine, Amd64RegisterR8, &Context->R8 );
    DbgCoreEngine.DbgRegisterRead( &DbgCoreEngine, Amd64RegisterR9, &Context->R9 );
    DbgCoreEngine.DbgRegisterRead( &DbgCoreEngine, Amd64RegisterR10, &Context->R10 );
    DbgCoreEngine.DbgRegisterRead( &DbgCoreEngine, Amd64RegisterR11, &Context->R11 );
    DbgCoreEngine.DbgRegisterRead( &DbgCoreEngine, Amd64RegisterR12, &Context->R12 );
    DbgCoreEngine.DbgRegisterRead( &DbgCoreEngine, Amd64RegisterR13, &Context->R13 );
    DbgCoreEngine.DbgRegisterRead( &DbgCoreEngine, Amd64RegisterR14, &Context->R14 );
    DbgCoreEngine.DbgRegisterRead( &DbgCoreEngine, Amd64RegisterR15, &Context->R15 );

    DbgCoreEngine.DbgRegisterRead( &DbgCoreEngine, Amd64RegisterRip, &Context->Rip );
    DbgCoreEngine.DbgRegisterRead( &DbgCoreEngine, Amd64RegisterEFlags, &Context->EFlags );
    DbgCoreEngine.DbgRegisterRead( &DbgCoreEngine, Amd64RegisterSegCs, &Context->SegCs );
    DbgCoreEngine.DbgRegisterRead( &DbgCoreEngine, Amd64RegisterSegSs, &Context->SegSs );
    DbgCoreEngine.DbgRegisterRead( &DbgCoreEngine, Amd64RegisterSegDs, &Context->SegDs );
    DbgCoreEngine.DbgRegisterRead( &DbgCoreEngine, Amd64RegisterSegEs, &Context->SegEs );
    DbgCoreEngine.DbgRegisterRead( &DbgCoreEngine, Amd64RegisterSegFs, &Context->SegFs );
    DbgCoreEngine.DbgRegisterRead( &DbgCoreEngine, Amd64RegisterSegGs, &Context->SegGs );
}

VOID
DbgKdPrint(
    _In_ PCHAR Format,
    _In_ ...
)
{
    CHAR               Buffer[ 512 ];
    va_list            List;
    DBGKD_PRINT_STRING Print = { 0 };
    STRING             Head;
    STRING             Body;

    Print.ProcessorLevel = 6;
    Print.ApiNumber = 0x3230;
    Print.Processor = 0;

    Head.Buffer = ( PCHAR )&Print;
    Head.Length = sizeof( DBGKD_PRINT_STRING );
    Head.MaximumLength = sizeof( DBGKD_PRINT_STRING );

    va_start( List, Format );
    vsprintf( Buffer,
              Format,
              List );
    va_end( List );

    Body.MaximumLength = 512;
    Print.Length = ( ULONG32 )strlen( Buffer );
    Body.Length = ( USHORT )Print.Length;
    Body.Buffer = ( PCHAR )Buffer;

    DbgKdSendPacket( KdTypePrint,
                     &Head,
                     &Body,
                     NULL );
}


VOID
DbgKdpDecodeDataBlock(
    _In_ PKDDEBUGGER_DATA64 DebuggerData
)
{
    ULONG32  Length;
    ULONG64* Long;

    Long = ( ULONG64* )DebuggerData;

    //  *( ULONG64* )DebuggerData = 
    // KiWaitAlways ^ 
    // _byteswap_uint64((unsigned __int64)&KdpDataBlockEncoded ^ 
    //  __ROL8__(KiWaitNever ^  *( ULONG64* )DebuggerData, KiWaitNever));

    Length = sizeof( KDDEBUGGER_DATA64 ) / sizeof( ULONG64 );

    while ( Length ) {

        *Long = KiWaitAlways ^
            _byteswap_uint64( KdpDataBlockEncoded ^
                              _rotl64( *Long ^ KiWaitNever, ( int )KiWaitNever ) );
        Long++;
        Length--;
    }
}

ULONG_PTR
DbgKdGetCurrentPcr(

)
{
#if 0
    CONTEXT CodeContext = { 0 };
    UCHAR   Code[ 2 ] = { 0x0F, 0x32 };

    DbgGdbGetContext( &CodeContext );

    //CodeContext.Rcx = CodeContext.SegCs == 0x10 ? 0xC0000101 : 0xC0000102;
    CodeContext.Rcx = 0xC0000101;
    DbgGdbExec( &CodeContext, Code, 1, 2 );

    return CodeContext.Rax | ( CodeContext.Rdx << 32 );
#endif

    ULONG32 Processor;

    DbgCoreEngine.DbgProcessorQuery( &DbgCoreEngine,
                                     &Processor );

    return DbgKdProcessorBlock[ Processor ] - 0x180;
}

ULONG32
DbgKdGetCurrentProcessor(
    _In_ ULONG_PTR Pcr
)
{
    ULONG32 PrcbNumber;

    PrcbNumber = 0;

    DbgCoreEngine.DbgMemoryRead( &DbgCoreEngine,
                                 Pcr + 0x180 + DbgKdDebuggerBlock.OffsetPrcbNumber,
                                 sizeof( ULONG32 ),
                                 &PrcbNumber );

    return PrcbNumber;
}

ULONG64
DbgKdGetCurrentThread(
    _In_ ULONG_PTR Pcr
)
{
    ULONG64 CurrentThread;

    CurrentThread = 0;

    DbgCoreEngine.DbgMemoryRead( &DbgCoreEngine,
                                 Pcr + 0x188,//0x180 + DbgKdDebuggerBlock.OffsetPrcbCurrentThread,
                                 sizeof( ULONG64 ),
                                 &CurrentThread );

    return CurrentThread;
}

VOID
DbgKdpSetCommonState(
    _In_    ULONG32                  ApiNumber,
    _In_    PCONTEXT                 Context,
    _Inout_ PDBGKD_WAIT_STATE_CHANGE State
)
{
    ULONG_PTR Pcr;

    //
    // TODO: Read as much of this from the kernel using symbols.
    //

    Pcr = DbgKdGetCurrentPcr( );

    DbgKdpTraceLogLevel1( DbgKdpFactory, "DbgKdGetCurrentPcr: %016llx\n", Pcr );

    State->ApiNumber = ApiNumber;
    State->Processor = DbgKdGetCurrentProcessor( Pcr );
    State->ProcessorCount = DbgKdProcessorCount;

    if ( State->Processor > State->ProcessorCount ) {

        __debugbreak( );
    }

    State->ProcessorLevel = DbgKdProcessorLevel;
    State->CurrentThread = DbgKdGetCurrentThread( Pcr );

    State->ProgramCounter = Context->Rip;

    DbgCoreEngine.DbgMemoryRead( &DbgCoreEngine,
                                 Context->Rip,
                                 0x10,
                                 State->ControlReport.InstructionStream );
    State->ControlReport.InstructionCount = 0x10;
}

VOID
DbgKdpSetContextState(
    _Inout_ PDBGKD_WAIT_STATE_CHANGE State,
    _In_    PCONTEXT                 Context
)
{
    State->ControlReport.Dr6 = 0xFFFF0FF0;
    State->ControlReport.Dr7 = 0x400;

    State->ControlReport.SegCs = Context->SegCs;
    State->ControlReport.SegDs = Context->SegDs;
    State->ControlReport.SegEs = Context->SegEs;
    State->ControlReport.SegFs = Context->SegFs;

    State->ControlReport.EFlags = Context->EFlags;
    State->ControlReport.ReportFlags = 1;

    if ( Context->SegCs == 0x10 ||
         Context->SegCs == 0x33 ) {

        State->ControlReport.ReportFlags = 3;
    }

}

VOID
DbgKdpReadVirtualMemory(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body
)
{
    DBG_STATUS Status;
    STRING     Recip;
    ULONG32    ReadCount;

    //
    // TODO: Bytes read on DbgGdbReadVirt
    //

    ReadCount = Packet->u.ReadMemory.TransferCount;
    ReadCount = min( KdTransportMaxPacketSize - sizeof( DBGKD_MANIPULATE_STATE64 ), ReadCount );
    ReadCount = min( Body->MaximumLength, ReadCount );

    Status = DbgCoreEngine.DbgMemoryRead( &DbgCoreEngine,
                                          Packet->u.ReadMemory.TargetBaseAddress,
                                          ReadCount,
                                          Body->Buffer );

    if ( !DBG_SUCCESS( Status ) ) {

        Body->Length = 0;
        Packet->u.ReadMemory.ActualBytesRead = 0;
        Packet->ReturnStatus = STATUS_UNSUCCESSFUL;
    }
    else {

        Body->Length = ReadCount;
        Packet->u.ReadMemory.ActualBytesRead = ReadCount;
        Packet->ReturnStatus = STATUS_SUCCESS;
    }

    Recip.MaximumLength = sizeof( DBGKD_MANIPULATE_STATE64 );
    Recip.Length = sizeof( DBGKD_MANIPULATE_STATE64 );
    Recip.Buffer = ( PCHAR )Packet;

    DbgKdSendPacket( KdTypeStateManipulate,
                     &Recip,
                     Body,
                     NULL );
}

VOID
DbgKdpWriteVirtualMemory(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body
)
{
    DBG_STATUS Status;
    STRING     Recip;

    Status = DbgCoreEngine.DbgMemoryWrite( &DbgCoreEngine,
                                           Packet->u.WriteMemory.TargetBaseAddress,
                                           Body->Length,
                                           Body->Buffer );

    if ( !DBG_SUCCESS( Status ) ) {

        Packet->u.WriteMemory.TransferCount = 0;
        Packet->u.WriteMemory.ActualBytesWritten = 0;
        Packet->ReturnStatus = STATUS_UNSUCCESSFUL;
    }
    else {

        Packet->u.WriteMemory.TransferCount = Body->Length;
        Packet->u.WriteMemory.ActualBytesWritten = Body->Length;
        Packet->ReturnStatus = STATUS_SUCCESS;
    }

    Recip.MaximumLength = sizeof( DBGKD_MANIPULATE_STATE64 );
    Recip.Length = sizeof( DBGKD_MANIPULATE_STATE64 );
    Recip.Buffer = ( PCHAR )Packet;

    DbgKdSendPacket( KdTypeStateManipulate,
                     &Recip,
                     NULL,
                     NULL );
}

VOID
DbgKdpGetVersion(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body
)
{
    STRING Recip;

    Packet->u.GetVersion64 = DbgKdVersionBlock;
    Packet->ReturnStatus = STATUS_SUCCESS;

    Recip.MaximumLength = sizeof( DBGKD_MANIPULATE_STATE64 );
    Recip.Length = sizeof( DBGKD_MANIPULATE_STATE64 );
    Recip.Buffer = ( PCHAR )Packet;

    DbgKdSendPacket( KdTypeStateManipulate,
                     &Recip,
                     NULL,
                     NULL );
}

VOID
DbgKdpGetContext(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body
)
{
    STRING   Recip;
    PCONTEXT Context;

    Body->Length = sizeof( CONTEXT );
    RtlZeroMemory( Body->Buffer, Body->Length );
    Context = ( PCONTEXT )Body->Buffer;
    DbgKdpBuildContextRecord( Context );

    Context->ContextFlags |= CONTEXT_DEBUG_REGISTERS;

    Context->Dr0 = DbgKdpDebugEmu.Dr0;
    Context->Dr1 = DbgKdpDebugEmu.Dr1;
    Context->Dr2 = DbgKdpDebugEmu.Dr2;
    Context->Dr3 = DbgKdpDebugEmu.Dr3;
    Context->Dr6 = DbgKdpDebugEmu.Dr6.Long;
    Context->Dr7 = DbgKdpDebugEmu.Dr7.Long;

    Packet->ReturnStatus = STATUS_SUCCESS;

    Recip.MaximumLength = sizeof( DBGKD_MANIPULATE_STATE64 );
    Recip.Length = sizeof( DBGKD_MANIPULATE_STATE64 );
    Recip.Buffer = ( PCHAR )Packet;

    DbgKdSendPacket( KdTypeStateManipulate,
                     &Recip,
                     Body,
                     NULL );
}

VOID
DbgKdpSetContext(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body
)
{
    STRING   Recip;
    PCONTEXT Context;

    Context = ( PCONTEXT )Body->Buffer;

    if ( ( Context->ContextFlags & CONTEXT_DEBUG_REGISTERS ) == CONTEXT_DEBUG_REGISTERS ) {

        DbgKdpEmuDebugUpdate( &Context->Dr0 );
    }

    DbgKdpApplyContextRecord( Context );

    Packet->ReturnStatus = STATUS_SUCCESS;

    Recip.MaximumLength = sizeof( DBGKD_MANIPULATE_STATE64 );
    Recip.Length = sizeof( DBGKD_MANIPULATE_STATE64 );
    Recip.Buffer = ( PCHAR )Packet;

    DbgKdSendPacket( KdTypeStateManipulate,
                     &Recip,
                     NULL,
                     NULL );
}

VOID
DbgKdpGetContextEx(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body
)
{
    STRING   Recip;
    ULONG64  BodyLength;
    ULONG64  BodyOffset;
    PCONTEXT Context;

    Body->Length = sizeof( CONTEXT );
    RtlZeroMemory( Body->Buffer, Body->Length );
    Context = ( PCONTEXT )( Body->Buffer );
    DbgKdpBuildContextRecord( Context );

    Context->ContextFlags |= CONTEXT_DEBUG_REGISTERS;

    Context->Dr0 = DbgKdpDebugEmu.Dr0;
    Context->Dr1 = DbgKdpDebugEmu.Dr1;
    Context->Dr2 = DbgKdpDebugEmu.Dr2;
    Context->Dr3 = DbgKdpDebugEmu.Dr3;
    Context->Dr6 = DbgKdpDebugEmu.Dr6.Long;
    Context->Dr7 = DbgKdpDebugEmu.Dr7.Long;

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

    Packet->ReturnStatus = STATUS_SUCCESS;

    Recip.MaximumLength = sizeof( DBGKD_MANIPULATE_STATE64 );
    Recip.Length = sizeof( DBGKD_MANIPULATE_STATE64 );
    Recip.Buffer = ( PCHAR )Packet;

    DbgKdSendPacket( KdTypeStateManipulate,
                     &Recip,
                     Body,
                     NULL );
}

VOID
DbgKdpSetContextEx(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body
)
{
    STRING  Recip;
    CONTEXT Context;
    ULONG32 Processor;

    DbgKdpBuildContextRecord( &Context );

    Context.ContextFlags |= CONTEXT_DEBUG_REGISTERS;

    Context.Dr0 = DbgKdpDebugEmu.Dr0;
    Context.Dr1 = DbgKdpDebugEmu.Dr1;
    Context.Dr2 = DbgKdpDebugEmu.Dr2;
    Context.Dr3 = DbgKdpDebugEmu.Dr3;
    Context.Dr6 = DbgKdpDebugEmu.Dr6.Long;
    Context.Dr7 = DbgKdpDebugEmu.Dr7.Long;

    DbgCoreEngine.DbgProcessorQuery( &DbgCoreEngine, &Processor );

    // TODO: Handle mp

    if ( Packet->Processor != Processor ) {

        __debugbreak( );
        // Context = KdGetPrcbContext( KeQueryPrcbAddress( Packet->Processor ) );
    }


    RtlCopyMemory( ( PCHAR )&Context + Packet->u.SetContextEx.Offset,
                   Body->Buffer, Packet->u.SetContextEx.ByteCount );
    Packet->u.SetContextEx.BytesCopied = Packet->u.SetContextEx.ByteCount;
    Packet->ReturnStatus = STATUS_SUCCESS;

    Recip.MaximumLength = sizeof( DBGKD_MANIPULATE_STATE64 );
    Recip.Length = sizeof( DBGKD_MANIPULATE_STATE64 );
    Recip.Buffer = ( PCHAR )Packet;

    DbgKdpEmuDebugUpdate( &Context.Dr0 );

    DbgKdpApplyContextRecord( &Context );

    DbgKdSendPacket( KdTypeStateManipulate,
                     &Recip,
                     NULL,
                     NULL );
}

VOID
DbgKdpGetSpecialRegisters(
    _Inout_ PKSPECIAL_REGISTERS SpecialRegisters
)
{
#if 0
    /*

    This isn't very hvci safe, or even safe at all,
    but it should work fine for most cases, in the future
    this should not really use code for storage,
    we can allocate some memory and keep track of it,
    just a pool for any exec code, maybe with
    better safety mechanisms too

    KiSaveProcessorState:
        mov     rax, cr0
        and     rax, ~0x10000
        mov     cr0, rax
        mov     rcx, cr2
        mov     rdx, cr3
        mov     rbx, cr4
        mov     rsp, dr0
        mov     rbp, dr1
        mov     rsi, dr2
        mov     rdi, dr3
        mov     r8, dr6
        mov     r9, dr7
        sgdt    [rip+KiSaveProcessorState]
        movzx   r10, word ptr [rip+KiSaveProcessorState]
        shl     r10, 48
        mov     r11, qword ptr [rip+KiSaveProcessorState+2]
        sidt    [rip+KiSaveProcessorState]
        movzx   r12, word ptr [rip+KiSaveProcessorState]
        shl     r12, 48
        mov     r13, qword ptr [rip+KiSaveProcessorState+8]
        stmxcsr dword ptr [rip+KiSaveProcessorState]
        mov     r15d, dword ptr [rip+KiSaveProcessorState]
        shr     r15, 32
        str     r14d
        or      r14, r15
        mov     r15, cr8
        or      rax, 0x10000
        mov     cr0, rax

    */

    UCHAR KiSaveProcessorState[ ] = {
        0x64, 0x0F, 0x20, 0xC0, 0x48, 0x25, 0xFF, 0xFF,
        0xFE, 0xFF, 0x0F, 0x22, 0xC0, 0x0F, 0x20, 0xD1,
        0x0F, 0x20, 0xDA, 0x0F, 0x20, 0xE3, 0x0F, 0x21,
        0xC4, 0x0F, 0x21, 0xCD, 0x0F, 0x21, 0xD6, 0x0F,
        0x21, 0xDF, 0x41, 0x0F, 0x21, 0xF0, 0x41, 0x0F,
        0x21, 0xF9, 0x0F, 0x01, 0x05, 0xD0, 0xFF, 0xFF,
        0xFF, 0x4C, 0x0F, 0xB7, 0x15, 0xC8, 0xFF, 0xFF,
        0xFF, 0x49, 0xC1, 0xE2, 0x30, 0x4C, 0x8B, 0x1D,
        0xBF, 0xFF, 0xFF, 0xFF, 0x0F, 0x01, 0x0D, 0xB6,
        0xFF, 0xFF, 0xFF, 0x4C, 0x0F, 0xB7, 0x25, 0xAE,
        0xFF, 0xFF, 0xFF, 0x49, 0xC1, 0xE4, 0x30, 0x4C,
        0x8B, 0x2D, 0xAB, 0xFF, 0xFF, 0xFF, 0x0F, 0xAE,
        0x1D, 0x9C, 0xFF, 0xFF, 0xFF, 0x44, 0x8B, 0x3D,
        0x95, 0xFF, 0xFF, 0xFF, 0x49, 0xC1, 0xEF, 0x20,
        0x41, 0x0F, 0x00, 0xCE, 0x4D, 0x09, 0xFE, 0x45,
        0x0F, 0x20, 0xC7, 0x48, 0x0D, 0x00, 0x00, 0x01,
        0x00, 0x0F, 0x22, 0xC0
    };

    CONTEXT CodeContext;

    do {
        DbgGdbGetContext( &CodeContext );

        DbgGdbExec( &CodeContext,
                    KiSaveProcessorState,
                    28,
                    sizeof( KiSaveProcessorState ) );

        RtlCopyMemory( SpecialRegisters,
                       &CodeContext.Rax,
                       FIELD_OFFSET( CONTEXT, R15 ) );
        static_assert( FIELD_OFFSET( KSPECIAL_REGISTERS, Idtr.Base ) ==
                       FIELD_OFFSET( CONTEXT, R13 ) - FIELD_OFFSET( CONTEXT, Rax ),
                       "Nigcode!" );

        SpecialRegisters->Cr8 = CodeContext.R15;

    } while ( SpecialRegisters->Gdtr.Limit != 0x57 );

    //
    // NIGCODE!
    //
#endif

    //
    // No one really needs to view these, hopefully
    // and executing code on the guest is very dangerous and causes
    // tons of instability, maybe if the gdb protocol was better...
    // Instead I've just hardcoded some.
    //
    SpecialRegisters->Cr0 = 0x80010001;

    DbgCoreEngine.DbgRegisterRead( &DbgCoreEngine,
                                   Amd64RegisterCr2,
                                   &SpecialRegisters->Cr2 );

    /*
    DbgCoreEngine.DbgRegisterRead( &DbgCoreEngine,
                                   Amd64RegisterCr3,
                                   &SpecialRegisters->Cr3 );
    */
    SpecialRegisters->Cr3 = 0;
    SpecialRegisters->Cr4 = 0;

    SpecialRegisters->Cr8 = 0;

    DbgCoreEngine.DbgMemoryRead( &DbgCoreEngine,
                                 DbgKdGetCurrentPcr( ),
                                 sizeof( ULONG64 ),
                                 &SpecialRegisters->Gdtr.Base );
    SpecialRegisters->Gdtr.Limit = 0x57;
    DbgCoreEngine.DbgMemoryRead( &DbgCoreEngine,
                                 DbgKdGetCurrentPcr( ) + 0x38,
                                 sizeof( ULONG64 ),
                                 &SpecialRegisters->Idtr.Base );
    SpecialRegisters->Idtr.Limit = 0xFFF;

    SpecialRegisters->KernelDr0 = DbgKdpDebugEmu.Dr0;
    SpecialRegisters->KernelDr1 = DbgKdpDebugEmu.Dr1;
    SpecialRegisters->KernelDr2 = DbgKdpDebugEmu.Dr2;
    SpecialRegisters->KernelDr3 = DbgKdpDebugEmu.Dr3;
    SpecialRegisters->KernelDr6 = DbgKdpDebugEmu.Dr6.Long;
    SpecialRegisters->KernelDr7 = DbgKdpDebugEmu.Dr7.Long;
}

VOID
DbgKdpReadControlSpace(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body
)
{
    STRING             Recip;
    PVOID              ControlSpace;
    ULONG64            Pcr;
    ULONG64            Prcb;
    ULONG32            ReadCount;
    ULONG32            MaximumLength;
    ULONG64            Pointer;
    KSPECIAL_REGISTERS Special;

    Pcr = DbgKdGetCurrentPcr( );
    Prcb = Pcr + 0x180;

    Packet->ReturnStatus = STATUS_SUCCESS;

    Recip.MaximumLength = sizeof( DBGKD_MANIPULATE_STATE64 );
    Recip.Length = sizeof( DBGKD_MANIPULATE_STATE64 );
    Recip.Buffer = ( PCHAR )Packet;

    MaximumLength = 8;

    ReadCount = Packet->u.ReadMemory.TransferCount;
    ReadCount = min( KdTransportMaxPacketSize - sizeof( DBGKD_MANIPULATE_STATE64 ), ReadCount );
    ReadCount = min( Body->MaximumLength, ReadCount );

    //__debugbreak( );

#if 0
    printf( "ReadControlSpace: %d %d ",
        ( ULONG32 )Packet->u.ReadMemory.TargetBaseAddress,
            ReadCount );
#endif

    switch ( Packet->u.ReadMemory.TargetBaseAddress ) {
    case AMD64_DEBUG_CONTROL_SPACE_PCR:
        ControlSpace = &Pcr;
        break;
    case AMD64_DEBUG_CONTROL_SPACE_PRCB:
        ControlSpace = &Prcb;
        break;
    case AMD64_DEBUG_CONTROL_SPACE_KSPECIAL:
        //
        // ProcessorPrcb->ProcessorState
        //
        // Must be emulated because this can be considered part of the
        // context record.
        //

        MaximumLength = sizeof( KSPECIAL_REGISTERS );

        RtlZeroMemory( &Special, sizeof( KSPECIAL_REGISTERS ) );
        DbgKdpGetSpecialRegisters( &Special );
        ControlSpace = &Special;
        break;
    case AMD64_DEBUG_CONTROL_SPACE_THREAD:
        //
        // ProcessorPrcb->CurrentThread
        //
        Pointer = Pcr + 0x188;
        ControlSpace = &Pointer;//DbgKdGetCurrentThread( Pcr );
        break;
    default:
        __debugbreak( );
        break;
    }

    if ( MaximumLength > ReadCount ) {

        MaximumLength = ReadCount;
    }

    Packet->u.ReadMemory.ActualBytesRead = MaximumLength;
    Body->Length = ( USHORT )MaximumLength;
#if 0
    if ( Packet->u.ReadMemory.TargetBaseAddress != AMD64_DEBUG_CONTROL_SPACE_KSPECIAL ) {

        DbgGdbReadVirtualMemory( ControlSpace,
                                 Body->Buffer,
                                 MaximumLength );
    }
    else {
#endif
        RtlCopyMemory( Body->Buffer, ControlSpace, MaximumLength );
#if 0        
    }
#endif

    DbgKdSendPacket( KdTypeStateManipulate,
                     &Recip,
                     Body,
                     NULL );
}

VOID
DbgKdpWriteControlSpace(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body
)
{
    STRING             Recip;
    PVOID              ControlSpace;
    ULONG64            Pcr;
    ULONG64            Prcb;
    ULONG32            ReadCount;
    ULONG32            MaximumLength;
    ULONG64            Pointer;
    KSPECIAL_REGISTERS Special;

    Pcr = DbgKdGetCurrentPcr( );
    Prcb = Pcr + 0x180;

    Packet->ReturnStatus = STATUS_SUCCESS;

    Recip.MaximumLength = sizeof( DBGKD_MANIPULATE_STATE64 );
    Recip.Length = sizeof( DBGKD_MANIPULATE_STATE64 );
    Recip.Buffer = ( PCHAR )Packet;

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
        //
        // ProcessorPrcb->ProcessorState
        //
        // Must be emulated because this can be considered part of the
        // context record.
        //

        MaximumLength = sizeof( KSPECIAL_REGISTERS );

        RtlZeroMemory( &Special, sizeof( KSPECIAL_REGISTERS ) );
        DbgKdpGetSpecialRegisters( &Special );
        ControlSpace = &Special;
        break;
    case AMD64_DEBUG_CONTROL_SPACE_THREAD:
        //
        // ProcessorPrcb->CurrentThread
        //
        Pointer = Pcr + 0x188;
        ControlSpace = &Pointer;//DbgKdGetCurrentThread( Pcr );
        break;
    default:
        __debugbreak( );
        break;
    }

    if ( MaximumLength > ReadCount ) {

        MaximumLength = ReadCount;
    }

    Packet->u.ReadMemory.ActualBytesRead = MaximumLength;
    Body->Length = ( USHORT )MaximumLength;
#if 0
    if ( Packet->u.ReadMemory.TargetBaseAddress != AMD64_DEBUG_CONTROL_SPACE_KSPECIAL ) {

        DbgGdbReadVirtualMemory( ControlSpace,
                                 Body->Buffer,
                                 MaximumLength );
    }
    else {
#endif
        RtlCopyMemory( ControlSpace, Body->Buffer, MaximumLength );
#if 0        
    }
#endif

    if ( Packet->u.ReadMemory.TargetBaseAddress == AMD64_DEBUG_CONTROL_SPACE_KSPECIAL ) {

        DbgKdpEmuDebugUpdate( &Special.KernelDr0 );
#if 0
        DbgCoreEngine.DbgRegisterWrite( &DbgCoreEngine,
                                        Amd64RegisterCr3,
                                        &Special.Cr3 );
#endif
    }

    DbgKdSendPacket( KdTypeStateManipulate,
                     &Recip,
                     Body,
                     NULL );
}

VOID
DbgKdpInsertBreakpoint(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body
)
{
    STRING  Recip;
    ULONG32 Handle;
    ULONG32 CurrentBreakpoint;

    Handle = ( ULONG32 )( -1 );
    for ( CurrentBreakpoint = 0;
          CurrentBreakpoint < DBGKD_BP_MAXIMUM;
          CurrentBreakpoint++ ) {

        if ( ( DbgKdBreakpointTable[ CurrentBreakpoint ].Flags & DBGKD_BP_ENABLED ) == 0 ) {

            Handle = CurrentBreakpoint;
            break;
        }
    }

    if ( Handle == ( ULONG32 )( -1 ) ) {

        Packet->ReturnStatus = STATUS_UNSUCCESSFUL;
        goto DbgKdpProcedureDone;
    }

    Packet->ReturnStatus = STATUS_SUCCESS;

    DbgKdBreakpointTable[ Handle ].Address = Packet->u.WriteBreakPoint.BreakPointAddress;
    DbgKdBreakpointTable[ Handle ].Flags = DBGKD_BP_ENABLED;

    Packet->u.WriteBreakPoint.BreakPointHandle = Handle;

    DbgCoreEngine.DbgBreakpointInsert( &DbgCoreEngine,
                                       BreakOnExecute,
                                       DbgKdBreakpointTable[ Handle ].Address,
                                       1 );
DbgKdpProcedureDone:

    Recip.MaximumLength = sizeof( DBGKD_MANIPULATE_STATE64 );
    Recip.Length = sizeof( DBGKD_MANIPULATE_STATE64 );
    Recip.Buffer = ( PCHAR )Packet;

    DbgKdSendPacket( KdTypeStateManipulate,
                     &Recip,
                     NULL,
                     NULL );
}

VOID
DbgKdpClearBreakpoint(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body
)
{
    STRING Recip;

    if ( Packet->u.RestoreBreakPoint.BreakPointHandle >= DBGKD_BP_MAXIMUM ) {

        Packet->ReturnStatus = STATUS_UNSUCCESSFUL;
    }
    else if ( DbgKdBreakpointTable
              [ Packet->u.RestoreBreakPoint.BreakPointHandle ].
              Flags & DBGKD_BP_ENABLED ) {

        DbgCoreEngine.DbgBreakpointClear( &DbgCoreEngine,
            ( DbgKdBreakpointTable
              [ Packet->u.RestoreBreakPoint.BreakPointHandle ].
              Flags & DBGKD_BP_WRITE ) ? BreakOnWrite :
              ( ( DbgKdBreakpointTable
                  [ Packet->u.RestoreBreakPoint.BreakPointHandle ]
        .Flags & DBGKD_BP_READ_WRITE ) ? BreakOnAccess : BreakOnExecute ),
                                          DbgKdBreakpointTable
            [ Packet->u.RestoreBreakPoint.BreakPointHandle ].
            Address );

        DbgKdBreakpointTable[ Packet->u.RestoreBreakPoint.BreakPointHandle ].Flags &= ~DBGKD_BP_ENABLED;
        Packet->ReturnStatus = STATUS_SUCCESS;
    }
    else {

        Packet->ReturnStatus = STATUS_SUCCESS;
    }

    Recip.MaximumLength = sizeof( DBGKD_MANIPULATE_STATE64 );
    Recip.Length = sizeof( DBGKD_MANIPULATE_STATE64 );
    Recip.Buffer = ( PCHAR )Packet;

    DbgKdSendPacket( KdTypeStateManipulate,
                     &Recip,
                     NULL,
                     NULL );
}

VOID
DbgKdpEmuSingleUpdate(
    _In_  DEBUG_CONTROL           Dr7,
    _In_  DEBUG_LINEAR            Linear,
    _In_  ULONG32                 Length,
    _In_  ULONG32                 Attribute,
    _Out_ PDBGKD_BREAKPOINT_ENTRY AssocBp
)
{

    AssocBp->Address = Linear;

    switch ( Attribute ) {
    case DEBUG_EXECUTE:
        DbgCoreEngine.DbgBreakpointInsert( &DbgCoreEngine,
                                           BreakOnExecute,
                                           Linear,
                                           Length );
        AssocBp->Flags = DBGKD_BP_ENABLED | DBGKD_BP_EXECUTE;
        break;
    case DEBUG_WRITE:
        DbgCoreEngine.DbgBreakpointInsert( &DbgCoreEngine,
                                           BreakOnWrite,
                                           Linear,
                                           Length );
        AssocBp->Flags = DBGKD_BP_ENABLED | DBGKD_BP_WRITE | DBGKD_BP_WATCHPOINT;
        break;
    case DEBUG_READ_WRITE:
        DbgCoreEngine.DbgBreakpointInsert( &DbgCoreEngine,
                                           BreakOnAccess,
                                           Linear,
                                           Length );
        AssocBp->Flags = DBGKD_BP_ENABLED | DBGKD_BP_READ_WRITE | DBGKD_BP_WATCHPOINT;
        break;
    default:
        __debugbreak( );
    }
}

VOID
DbgKdpEmuDebugUpdate(
    _In_ ULONG64* DebugRegisters
)
{
    //
    // The pointer provided to this api will most likely be the start
    // of a CONTEXT record, they're all sequential and it's just easiest.
    //

    DEBUG_CONTROL Dr7 = { 0 };

    Dr7.Long = DebugRegisters[ 5 ];

    //
    // You could do some mask & bit magic but that doesn't change the fact
    // that this is going to be disgusting, you could even incorporate an array, but 
    // that's just unnecessary
    //
    // Local breakpoints are kinda useless, just ignore.
    //

    if ( ( Dr7.G0 && !DbgKdpDebugEmu.Dr7.G0 ) ||
        ( Dr7.L0 && !DbgKdpDebugEmu.Dr7.L0 ) ) {

        DbgKdpEmuSingleUpdate( Dr7,
                               DebugRegisters[ 0 ],
                               ( ULONG32 )Dr7.Len0,
                               ( ULONG32 )Dr7.Rwe0,
                               &DbgKdpDebugEmu.Dr0AssocBp );
    }

    if ( ( Dr7.G1 && !DbgKdpDebugEmu.Dr7.G1 ) ||
        ( Dr7.L1 && !DbgKdpDebugEmu.Dr7.L1 ) ) {

        DbgKdpEmuSingleUpdate( Dr7,
                               DebugRegisters[ 1 ],
                               ( ULONG32 )Dr7.Len1,
                               ( ULONG32 )Dr7.Rwe1,
                               &DbgKdpDebugEmu.Dr1AssocBp );
    }

    if ( ( Dr7.G2 && !DbgKdpDebugEmu.Dr7.G2 ) ||
        ( Dr7.L2 && !DbgKdpDebugEmu.Dr7.L2 ) ) {

        DbgKdpEmuSingleUpdate( Dr7,
                               DebugRegisters[ 2 ],
                               ( ULONG32 )Dr7.Len2,
                               ( ULONG32 )Dr7.Rwe2,
                               &DbgKdpDebugEmu.Dr2AssocBp );
    }

    if ( ( Dr7.G3 && !DbgKdpDebugEmu.Dr7.G3 ) ||
        ( Dr7.L3 && !DbgKdpDebugEmu.Dr7.L3 ) ) {

        DbgKdpEmuSingleUpdate( Dr7,
                               DebugRegisters[ 3 ],
                               ( ULONG32 )Dr7.Len3,
                               ( ULONG32 )Dr7.Rwe3,
                               &DbgKdpDebugEmu.Dr3AssocBp );
    }

    if ( !Dr7.G0 && DbgKdpDebugEmu.Dr7.G0 ) {
        //
        // G0 bp removed.
        //

    }

    DbgKdpDebugEmu.Dr0 = DebugRegisters[ 0 ];
    DbgKdpDebugEmu.Dr1 = DebugRegisters[ 1 ];
    DbgKdpDebugEmu.Dr2 = DebugRegisters[ 2 ];
    DbgKdpDebugEmu.Dr3 = DebugRegisters[ 3 ];
    DbgKdpDebugEmu.Dr6.Long = DebugRegisters[ 4 ];
    DbgKdpDebugEmu.Dr7.Long = DebugRegisters[ 5 ];
}

#if 1
ULONG32 DbgKdpOffsetShadowFlags = 0;
ULONG32 DbgKdpOffsetKernelDirBase = 0;
#else
ULONG32 DbgKdpOffsetUserDirBase = 0;
ULONG32 DbgKdpOffsetDirBase = 0;
ULONG32 DbgKdpOffsetApcProcess = 0;
#endif

VOID
DbgKdStoreKvaShadow(
    _In_ PCONTEXT          Context,
    _In_ PDBGKD_KVAS_STATE KvaState
)
{

    //
    // KiKvaShadow            is for if system-wide kva shadowing is enabled
    // Prcb->ShadowFlags & 1  is for active address policy
    //
    // KiIsKvaShadowDisabled  return (KiFeatureSettings & 2) != 0;
    //
    // TODO: Collect init flags at start-up
    //

#if 0
    ULONG_PTR CurrentProcess;
    ULONG_PTR UserDirBase;
    ULONG_PTR KernelDirBase;
    ULONG_PTR CurrentDirBase;

    if ( DbgKdpOffsetDirBase == 0 &&
         DbgKdpOffsetUserDirBase == 0 ) {

        DbgPdbFieldOffset( &DbgPdbKernelContext,
                           L"_KPROCESS",
                           L"DirectoryTableBase",
                           ( PLONG32 )&DbgKdpOffsetDirBase );

        DbgPdbFieldOffset( &DbgPdbKernelContext,
                           L"_KPROCESS",
                           L"UserDirectoryTableBase",
                           ( PLONG32 )&DbgKdpOffsetUserDirBase );

        DbgPdbFieldOffset( &DbgPdbKernelContext,
                           L"_KTHREAD",
                           L"ApcState",
                           ( PLONG32 )&DbgKdpOffsetApcProcess );
        DbgKdpOffsetApcProcess += 0x20;
        // .Process
    }

    DbgCoreEngine.DbgMemoryRead( &DbgCoreEngine,
                                 DbgKdGetCurrentThread( DbgKdGetCurrentPcr( ) ) +
                                 DbgKdpOffsetApcProcess,
                                 sizeof( ULONG_PTR ),
                                 &CurrentProcess );

    KvaState->KvaShadow = 0;

    DbgCoreEngine.DbgMemoryRead( &DbgCoreEngine,
                                 CurrentProcess + DbgKdpOffsetUserDirBase,
                                 sizeof( ULONG_PTR ),
                                 &UserDirBase );

    DbgCoreEngine.DbgRegisterRead( &DbgCoreEngine,
                                   Amd64RegisterCr3,
                                   &CurrentDirBase );

    DbgCoreEngine.DbgMemoryRead( &DbgCoreEngine,
                                 CurrentProcess + DbgKdpOffsetDirBase,
                                 sizeof( ULONG_PTR ),
                                 &KernelDirBase );

    DbgKdpTraceLogLevel1( DbgKdpFactory,
                          "DirBase: %llx %llx %llx\n",
                          CurrentDirBase,
                          UserDirBase,
                          KernelDirBase );

    DbgCoreEngine.DbgRegisterWrite( &DbgCoreEngine,
                                    Amd64RegisterCr3,
                                    &KernelDirBase );

    KvaState->KvaShadow = DBGKD_KVAS_ENABLED;
    KvaState->UserDirBase = CurrentDirBase;

#if 0
    if ( UserDirBase == CurrentDirBase ) {

        printf( "Kva Time!\n" );

        KvaState->KvaShadow = DBGKD_KVAS_ENABLED;
        KvaState->UserDirBase = UserDirBase;

        DbgCoreEngine.DbgRegisterWrite( &DbgCoreEngine,
                                        Amd64RegisterCr3,
                                        &KernelDirBase );
    }
#endif

#else

    ULONG32        ShadowFlags;
    ULONG64        KernelDirBase;

    if ( ( DbgKiFeatureSettings & 2 ) != 0 ) {

        KvaState->KvaShadow = 0;
        return;
    }

    if ( DbgKdpOffsetShadowFlags == 0 &&
         DbgKdpOffsetKernelDirBase == 0 ) {

        //
        // Offset+0x180 is because this is directly on the 
        // KPCR at offset 0x180.
        //

        DbgPdbFieldOffset( &DbgPdbKernelContext,
                           L"_KPRCB",
                           L"ShadowFlags",
                           ( PLONG32 )&DbgKdpOffsetShadowFlags );
        DbgKdpOffsetShadowFlags += 0x180;

        DbgPdbFieldOffset( &DbgPdbKernelContext,
                           L"_KPRCB",
                           L"KernelDirectoryTableBase",
                           ( PLONG32 )&DbgKdpOffsetKernelDirBase );
        DbgKdpOffsetKernelDirBase += 0x180;
    }

    DbgCoreEngine.DbgMemoryRead( &DbgCoreEngine,
                                 DbgKdGetCurrentPcr( ) + DbgKdpOffsetShadowFlags,
                                 sizeof( ULONG32 ),
                                 &ShadowFlags );

    DbgCoreEngine.DbgRegisterRead( &DbgCoreEngine,
                                   Amd64RegisterCr3,
                                   &KvaState->UserDirBase );

    DbgCoreEngine.DbgMemoryRead( &DbgCoreEngine,
                                 DbgKdGetCurrentPcr( ) + DbgKdpOffsetKernelDirBase,
                                 sizeof( ULONG64 ),
                                 &KernelDirBase );
    // weird bug.
    KernelDirBase &= ~0x8000000000000000;

    KvaState->KvaShadow = 0;

    DbgKdpTraceLogLevel1( DbgKdpFactory,
                          "KVAS audit. KernelDirBase=%016llx UserDirBase=%016llx\n",
                          KernelDirBase,
                          KvaState->UserDirBase );

    if ( !( ShadowFlags & 2 ) && KvaState->UserDirBase != KernelDirBase ) {

        KvaState->KvaShadow = DBGKD_KVAS_ENABLED;

        DbgCoreEngine.DbgRegisterWrite( &DbgCoreEngine,
                                        Amd64RegisterCr3,
                                        &KernelDirBase );

        DbgKdpTraceLogLevel1( DbgKdpFactory,
                              "KVAS store. KernelDirBase=%016llx UserDirBase=%016llx\n",
                              KernelDirBase,
                              KvaState->UserDirBase );
    }

#endif

}

VOID
DbgKdRestoreKvaShadow(
    _In_ PCONTEXT          Context,
    _In_ PDBGKD_KVAS_STATE KvaState
)
{
    Context;

    if ( KvaState->KvaShadow & DBGKD_KVAS_ENABLED ) {

        DbgKdpTraceLogLevel1( DbgKdpFactory, "KVAS restore.\n" );

        DbgCoreEngine.DbgRegisterWrite( &DbgCoreEngine,
                                        Amd64RegisterCr3,
                                        &KvaState->UserDirBase );
        KvaState->KvaShadow = 0;
    }
}
