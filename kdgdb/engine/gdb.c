
#include <kdgdb.h>

//
// TODO: Migrate to RPCRT4.
//
// TODO: GDB port & address selection.
//
// https://github.com/mborgerson/gdbstub/blob/master/gdbstub.c
// https://github.com/bminor/binutils-gdb/blob/master/gdb/stubs/i386-stub.c
//

UCHAR               DbgGdbMessageBuffer[ 0x2000 ];

#define GDB_PORT 8864
#define GDB_ADDR "127.0.0.1"

#define FROM_HEX_DIGIT( x ) ( ( ( ( x ) <= '9' ? ( x ) - '0' : ( ( x ) - 'a' + 0xA ) ) ) & 0x0F )
#define TO_HEX_DIGIT( x )   ( ( ( x ) <= 9 ? ( x ) + '0' : ( ( x ) + 'a' - 0xA ) ) )

DBG_STATUS
DbgGdbCheckQueue(
    _In_ PDBG_CORE_ENGINE Engine
);

DBG_STATUS
DbgGdbSendPacket(
    _In_    PDBG_CORE_ENGINE Engine,
    _Inout_ PSTRING          Packet
);

DBG_STATUS
DbgGdbRecvPacket(
    _In_    PDBG_CORE_ENGINE Engine,
    _Inout_ PSTRING          Packet
);

DBG_STATUS
DbgGdbProcessorStep(
    _In_ PDBG_CORE_ENGINE Engine
);

DBG_STATUS
DbgGdbSendf(
    _In_ PDBG_CORE_ENGINE Engine,
    _In_ PCHAR            Format,
    _In_ ...
);

DBG_STATUS
DbgGdbMemoryRead(
    _In_  PDBG_CORE_ENGINE Engine,
    _In_  ULONG_PTR        Address,
    _In_  ULONG32          Length,
    _Out_ PVOID            Buffer
);

DBG_STATUS
DbgGdbMemoryWrite(
    _In_  PDBG_CORE_ENGINE Engine,
    _In_  ULONG_PTR        Address,
    _In_  ULONG32          Length,
    _Out_ PVOID            Buffer
);

DBG_STATUS
DbgGdbExec(
    _In_ PCONTEXT CodeContext,
    _In_ PVOID    Code,
    _In_ ULONG32  OpCount,
    _In_ ULONG32  Length
);

DBG_STATUS
DbgGdbRegisterWrite(
    _In_ PDBG_CORE_ENGINE  Engine,
    _In_ DBG_CORE_REGISTER RegisterId,
    _In_ PVOID             Content
);

DBG_STATUS
DbgGdbRegisterRead(
    _In_  PDBG_CORE_ENGINE  Engine,
    _In_  DBG_CORE_REGISTER RegisterId,
    _Out_ PVOID             Content
);

DBG_STATUS
DbgGdbProcessorBreak(
    _In_  PDBG_CORE_ENGINE Engine,
    _Out_ PULONG32         Processor
);

DBG_STATUS
DbgGdbProcessorContinue(
    _In_ PDBG_CORE_ENGINE Engine
);

DBG_STATUS
DbgGdbBreakpointInsert(
    _In_ PDBG_CORE_ENGINE    Engine,
    _In_ DBG_CORE_BREAKPOINT Breakpoint,
    _In_ ULONG_PTR           Address,
    _In_ ULONG32             Length
);

DBG_STATUS
DbgGdbBreakpointClear(
    _In_ PDBG_CORE_ENGINE    Engine,
    _In_ DBG_CORE_BREAKPOINT Breakpoint,
    _In_ ULONG_PTR           Address
);

DBG_STATUS
DbgGdbProcessorQuery(
    _In_  PDBG_CORE_ENGINE Engine,
    _Out_ PULONG32         ProcessorNumber
);

DBG_STATUS
DbgGdbProcessorSwitch(
    _In_ PDBG_CORE_ENGINE Engine,
    _In_ ULONG32          Processor
);

DBG_STATUS
DbgGdbAmd64MsrRead(
    _In_  PDBG_CORE_ENGINE Engine,
    _In_  ULONG32          Register,
    _Out_ PULONG64         Buffer
);

DBG_STATUS
DbgGdbAmd64MsrWrite(
    _In_ PDBG_CORE_ENGINE Engine,
    _In_ ULONG32          Register,
    _In_ ULONG64         Buffer
);

VOID
DbgGdbEncodeHex(
    _Out_ PVOID   Output,
    _In_  PVOID   Input,
    _In_  ULONG32 Length
)
{
    ULONG32 CurrentChar;
    PUCHAR  Input1;
    PUCHAR  Output1;

    Input1 = Input;
    Output1 = Output;

    for ( CurrentChar = 0;
          CurrentChar < Length;
          CurrentChar++ ) {

        *Output1++ = TO_HEX_DIGIT( Input1[ CurrentChar ] >> 4 );
        *Output1++ = TO_HEX_DIGIT( Input1[ CurrentChar ] & 0x0F );
    }
}

VOID
DbgGdbDecodeHex(
    _Out_ PVOID   Output,
    _In_  PVOID   Input,
    _In_  ULONG32 Length
)
{
    ULONG32 CurrentChar;
    PUCHAR  Input1;
    PUCHAR  Output1;

    Input1 = Input;
    Output1 = Output;

    for ( CurrentChar = 0;
          CurrentChar < Length;
          CurrentChar++ ) {

        Output1[ CurrentChar ] = FROM_HEX_DIGIT( *Input1 ) << 4;
        Input1++;
        Output1[ CurrentChar ] |= FROM_HEX_DIGIT( *Input1 );
        Input1++;
    }
}

UCHAR
DbgGdbChecksum(
    _In_ PUCHAR  Buffer,
    _In_ ULONG32 Length
)
{
    UCHAR Checksum;

    Checksum = 0;

    while ( Length-- ) {

        Checksum += *Buffer++;
    }

    return Checksum;
}

DBG_STATUS
DbgGdbInit(
    _In_ PDBG_CORE_ENGINE Engine
)
{
    DbgSocketInit( &Engine->DbgCommDevice,
                   GDB_ADDR,
                   GDB_PORT );

    Engine->DbgCheckQueue        = DbgGdbCheckQueue;

    Engine->DbgMemoryRead        = DbgGdbMemoryRead;
    Engine->DbgMemoryWrite       = DbgGdbMemoryWrite;
    Engine->DbgRegisterRead      = DbgGdbRegisterRead;
    Engine->DbgRegisterWrite     = DbgGdbRegisterWrite;
    Engine->DbgBreakpointInsert  = DbgGdbBreakpointInsert;
    Engine->DbgBreakpointClear   = DbgGdbBreakpointClear;
    Engine->DbgProcessorSwitch   = DbgGdbProcessorSwitch;
    Engine->DbgProcessorBreak    = DbgGdbProcessorBreak;
    Engine->DbgProcessorContinue = DbgGdbProcessorContinue;
    Engine->DbgProcessorStep     = DbgGdbProcessorStep;
    Engine->DbgProcessorQuery    = DbgGdbProcessorQuery;

    //
    // Amd64 specific
    //

    Engine->DbgMsrRead           = DbgGdbAmd64MsrRead;
    Engine->DbgMsrWrite          = DbgGdbAmd64MsrWrite;

    return DBG_STATUS_OKAY;
}

DBG_STATUS
DbgGdbCheckQueue(
    _In_ PDBG_CORE_ENGINE Engine
)
{
    STRING String;

    String.MaximumLength = sizeof( DbgGdbMessageBuffer );
    String.Length = 0;
    String.Buffer = DbgGdbMessageBuffer;

    return DbgGdbRecvPacket( Engine, &String );
}

DBG_STATUS
DbgGdbSendPacket(
    _In_    PDBG_CORE_ENGINE Engine,
    _Inout_ PSTRING          Packet
)
{
    UCHAR CurrentChar;
    UCHAR Checksum[ 2 ];
    ULONG TimeOut;

    CurrentChar = GDB_PACKET_START;
    Engine->DbgCommDevice.DbgSend( &Engine->DbgCommDevice.Extension,
                                   &CurrentChar,
                                   1 );

    Engine->DbgCommDevice.DbgSend( &Engine->DbgCommDevice.Extension,
                                   Packet->Buffer,
                                   Packet->Length );

    CurrentChar = GDB_PACKET_END;
    Engine->DbgCommDevice.DbgSend( &Engine->DbgCommDevice.Extension,
                                   &CurrentChar,
                                   1 );

    CurrentChar = DbgGdbChecksum( Packet->Buffer,
                                  Packet->Length );
    DbgGdbEncodeHex( Checksum, &CurrentChar, 1 );

    Engine->DbgCommDevice.DbgSend( &Engine->DbgCommDevice.Extension,
                                   Checksum,
                                   2 );

    //
    // VMWARE GDB STUB IS BAD!
    //

    Packet->Buffer[ Packet->Length ] = 0;

    TimeOut = 0;
    while ( !DBG_SUCCESS( Engine->DbgCommDevice.DbgRecv( &Engine->DbgCommDevice.Extension,
                                                         &CurrentChar,
                                                         1 ) ) ) {

        TimeOut++;
        Sleep( 10 );
        if ( TimeOut > 150 ) {

            return DBG_STATUS_ERROR;
        }
    }

    return CurrentChar == GDB_PACKET_ACK ? DBG_STATUS_OKAY : DBG_STATUS_ERROR;
}

DBG_STATUS
DbgGdbRecvPacket(
    _In_    PDBG_CORE_ENGINE Engine,
    _Inout_ PSTRING Packet
)
{
    UCHAR CurrentChar;
    UCHAR Checksum[ 2 ];
    ULONG TimeOut = 0;

    CurrentChar = 0;
    do {
        if ( !DBG_SUCCESS( Engine->DbgCommDevice.DbgRecv( &Engine->DbgCommDevice.Extension,
                                                          &CurrentChar,
                                                          1 ) ) ) {

        }

        if ( CurrentChar == GDB_PACKET_START ) {

            break;
        }

        TimeOut++;
        Sleep( 10 );

        if ( TimeOut > 150 ) {

            return DBG_STATUS_ERROR;
        }
    } while ( TRUE );

    Packet->Length = 0;

    do {

        if ( !DBG_SUCCESS( Engine->DbgCommDevice.DbgRecv( &Engine->DbgCommDevice.Extension,
                                                          &CurrentChar,
                                                          1 ) ) ) {

            continue;
            //return DBG_STATUS_ERROR;
        }

        if ( CurrentChar == GDB_PACKET_END ) {

            if ( Packet->Length == 0 ) {

                DbgKdpTraceLogLevel1( DbgGdbFactory, "Unsupported packet!\n" );
            }

            break;
        }

        if ( Packet->Length > Packet->MaximumLength ) {

            break;
        }

        Packet->Buffer[ Packet->Length++ ] = CurrentChar;

    } while ( TRUE );

    if ( CurrentChar != GDB_PACKET_END ) {

        //
        // Buffer too small.
        //

        return DBG_STATUS_ERROR;
    }

    while ( !DBG_SUCCESS( Engine->DbgCommDevice.DbgRecv( &Engine->DbgCommDevice.Extension,
                                                         Checksum,
                                                         2 ) ) ) {

        continue;
    }

    CurrentChar = GDB_PACKET_ACK;
    Engine->DbgCommDevice.DbgSend( &Engine->DbgCommDevice.Extension,
                                   &CurrentChar,
                                   1 );

    //Packet->Buffer[ Packet->Length ] = 0;
    //printf( "DbgRecv: %s\n", Packet->Buffer );

    return DBG_STATUS_OKAY;
}

DBG_STATUS
DbgGdbProcessorStep(
    _In_ PDBG_CORE_ENGINE Engine
)
{
    CHAR       Buffer[ 512 ];
    STRING     String;
    DBG_STATUS Status;

    Buffer[ 0 ] = 's';
    String.Buffer = Buffer;
    String.Length = 1;
    String.MaximumLength = 512;

    Status = DbgGdbSendPacket( Engine,
                               &String );
    if ( !DBG_SUCCESS( Status ) ) {

        return Status;
    }

    //
    // not sure what this even returns because gdb is by far the worst shit
    // i've worked with, it's open source and somehow significantly worse to work with
    // than closed-source windbg/kd.
    //
    // "T05thread:00000001;06:0000000000000000;07:68d53b2100f8ffff;10:48e92a1f00f8ffff;"
    //
    // T05thread:processor?;06;?;?;rsp;?;rip;
    //

    return DbgGdbRecvPacket( Engine, &String );
}

DBG_STATUS
DbgGdbSendf(
    _In_ PDBG_CORE_ENGINE Engine,
    _In_ PCHAR            Format,
    _In_ ...
)
{
    CHAR    Buffer[ 512 ];
    STRING  String;
    va_list List;

    va_start( List, Format );
    vsprintf( Buffer,
              Format,
              List );
    va_end( List );
    String.MaximumLength = 512;
    String.Length = ( USHORT )strlen( Buffer );
    String.Buffer = ( PCHAR )Buffer;

    return DbgGdbSendPacket( Engine, &String );
}

DBG_STATUS
DbgGdbMemoryRead(
    _In_  PDBG_CORE_ENGINE Engine,
    _In_  ULONG_PTR        Address,
    _In_  ULONG32          Length,
    _Out_ PVOID            Buffer
)
{
    //
    // When debugging KdDebuggerDataBlock and wondering why
    // my decode data block procedure doesn't work, I realised
    // it's because the vmware gdb stub can only read up to 0x1FF
    // bytes
    //

    STRING     String;
    DBG_STATUS Status;

    while ( Length > 0 ) {

        String.Buffer = DbgGdbMessageBuffer;
        String.Length = 0;
        String.MaximumLength = sizeof( DbgGdbMessageBuffer );

        Status = DbgGdbSendf( Engine, "m%llx,%04x", Address, min( Length, 0x1FF ) );
        if ( !DBG_SUCCESS( Status ) ) {

            return Status;
        }

        RtlZeroMemory( DbgGdbMessageBuffer, 0x200 * 2 );
        Status = DbgGdbRecvPacket( Engine, &String );
        if ( !DBG_SUCCESS( Status ) ) {

            return Status;
        }

        if ( strlen( String.Buffer ) == 3 &&
             String.Buffer[ 0 ] == 'E' ) {

            //
            // Exx returned, meaning an error.
            //

            return DBG_STATUS_ERROR;
        }

        DbgGdbDecodeHex( Buffer,
                         DbgGdbMessageBuffer,
                         ( ULONG32 )String.Length / 2 );

        if ( ( String.Length / 2 ) != min( Length, 0x1FF ) ) {

            __debugbreak( );
        }

        //assert ( ( strlen( DbgGdbMessageBuffer ) / 2 ) != min( Length, 0x1FF ) ) 

        Buffer = ( PVOID )( ( ULONG64 )Buffer + 0x1FF );
        Address += 0x1FF;
        Length -= min( Length, 0x1FF );
    }

    return DBG_STATUS_OKAY;
}

DBG_STATUS
DbgGdbMemoryWrite(
    _In_  PDBG_CORE_ENGINE Engine,
    _In_  ULONG_PTR        Address,
    _In_  ULONG32          Length,
    _Out_ PVOID            Buffer
)
{
    STRING     String;
    ULONG32    Byte;
    UCHAR      Hex[ 3 ] = { 0 };
    DBG_STATUS Status;
    ULONG32    CurrentLength;

    //
    // 0x1FF packet size maximum, account for it with some limits
    // similar to the reading counterpart.
    //
    // 0x1F0 is fine, when including the M%llx,%04x:
    //

    while ( Length > 0 ) {

        CurrentLength = min( Length, 0x1E0 );

        String.Buffer = DbgGdbMessageBuffer;
        String.Length = 0;
        String.MaximumLength = sizeof( DbgGdbMessageBuffer );

        sprintf( String.Buffer, "M%llx,%04x:", Address, CurrentLength );
        for ( Byte = 0; Byte < CurrentLength; Byte++ ) {

            DbgGdbEncodeHex( Hex, ( PUCHAR )Buffer + Byte, 1 );
            strcat( String.Buffer, ( PCSTR )Hex );
        }

        String.Length = ( USHORT )strlen( String.Buffer );
        Status = DbgGdbSendPacket( Engine, &String );
        if ( !DBG_SUCCESS( Status ) ) {

            return Status;
        }

        DbgGdbRecvPacket( Engine, &String );

        Buffer = ( PVOID )( ( ULONG64 )Buffer + 0x1E0 );
        Address += 0x1E0;
        Length -= min( Length, 0x1E0 );
    }

    return DBG_STATUS_OKAY;
}

#if 0
DBG_STATUS
DbgGdbGetContext(
    _Out_ PCONTEXT Context
)
{
    STRING     String;
    DWORD      Segment;
    DBG_STATUS Status;

    String.Length = 1;
    String.MaximumLength = sizeof( DbgGdbMessageBuffer );
    String.Buffer = DbgGdbMessageBuffer;

    DbgGdbMessageBuffer[ 0 ] = 'g';

    Status = DbgGdbSendPacket( &String );
    if ( !DBG_SUCCESS( Status ) ) {

        return Status;
    }

    Status = DbgGdbDevice.Gdb.DbgRecvPacket( &String );
    if ( !DBG_SUCCESS( Status ) ) {

        return Status;
    }

    DbgGdbDecodeHex( &Context->Rax, DbgGdbMessageBuffer, 8 );
    DbgGdbDecodeHex( &Context->Rbx, DbgGdbMessageBuffer + 16, 8 );
    DbgGdbDecodeHex( &Context->Rcx, DbgGdbMessageBuffer + 32, 8 );
    DbgGdbDecodeHex( &Context->Rdx, DbgGdbMessageBuffer + 48, 8 );
    DbgGdbDecodeHex( &Context->Rsi, DbgGdbMessageBuffer + 64, 8 );
    DbgGdbDecodeHex( &Context->Rdi, DbgGdbMessageBuffer + 80, 8 );
    DbgGdbDecodeHex( &Context->Rbp, DbgGdbMessageBuffer + 96, 8 );
    DbgGdbDecodeHex( &Context->Rsp, DbgGdbMessageBuffer + 112, 8 );
    DbgGdbDecodeHex( &Context->R8, DbgGdbMessageBuffer + 128, 8 );
    DbgGdbDecodeHex( &Context->R9, DbgGdbMessageBuffer + 144, 8 );
    DbgGdbDecodeHex( &Context->R10, DbgGdbMessageBuffer + 160, 8 );
    DbgGdbDecodeHex( &Context->R11, DbgGdbMessageBuffer + 176, 8 );
    DbgGdbDecodeHex( &Context->R12, DbgGdbMessageBuffer + 192, 8 );
    DbgGdbDecodeHex( &Context->R13, DbgGdbMessageBuffer + 208, 8 );
    DbgGdbDecodeHex( &Context->R14, DbgGdbMessageBuffer + 224, 8 );
    DbgGdbDecodeHex( &Context->R15, DbgGdbMessageBuffer + 240, 8 );

    DbgGdbDecodeHex( &Context->Rip, DbgGdbMessageBuffer + 256, 8 );

    DbgGdbDecodeHex( &Context->EFlags, DbgGdbMessageBuffer + 272, 4 );
    DbgGdbDecodeHex( &Segment, DbgGdbMessageBuffer + 280, 4 );
    Context->SegCs = ( WORD )Segment;
    DbgGdbDecodeHex( &Segment, DbgGdbMessageBuffer + 288, 4 );
    Context->SegSs = ( WORD )Segment;
    DbgGdbDecodeHex( &Segment, DbgGdbMessageBuffer + 296, 4 );
    Context->SegDs = ( WORD )Segment;
    DbgGdbDecodeHex( &Segment, DbgGdbMessageBuffer + 304, 4 );
    Context->SegEs = ( WORD )Segment;
    DbgGdbDecodeHex( &Segment, DbgGdbMessageBuffer + 312, 4 );
    Context->SegFs = ( WORD )Segment;
    DbgGdbDecodeHex( &Segment, DbgGdbMessageBuffer + 320, 4 );
    Context->SegGs = ( WORD )Segment;

    Context->ContextFlags = CONTEXT_AMD64 | CONTEXT_INTEGER | CONTEXT_SEGMENTS | CONTEXT_CONTROL;

    return DBG_STATUS_OKAY;
}

#endif

DBG_STATUS
DbgGdbRegisterWrite(
    _In_ PDBG_CORE_ENGINE  Engine,
    _In_ DBG_CORE_REGISTER RegisterId,
    _In_ PVOID             Content
)
{
    STRING               String;
    ULONG64              Temp[ 4 ];

    DBGGDB_AMD64_EXECUTE Amd64Execute;
    UCHAR                MovCr3[ ] = { 0x0F, 0x22, 0xD8 };

    String.Length = 0;
    String.MaximumLength = sizeof( DbgGdbMessageBuffer );
    String.Buffer = DbgGdbMessageBuffer;

    //
    // TODO: Ensure size is correct.
    //

    RtlZeroMemory( Temp, sizeof( Temp ) );
    RtlCopyMemory( Temp, Content, DbgAmd64RegisterSizeTable[ RegisterId ] );

    if ( RegisterId == Amd64RegisterCr3 ) {

        Amd64Execute.Rax = Temp[ 0 ];

        return DbgGdbAmd64Execute( Engine,
                                   &Amd64Execute,
                                   MovCr3,
                                   1,
                                   3 );
    }
    else {

        DbgGdbEncodeHex( DbgGdbMessageBuffer,
                         Temp,
                         DbgAmd64RegisterSizeTable[ RegisterId ] );

        DbgGdbSendf( Engine,
                     "P%x=%s",
                     RegisterId,
                     DbgGdbMessageBuffer );

        return DbgGdbRecvPacket( Engine, &String );
    }
}

DBG_STATUS
DbgGdbRegisterRead(
    _In_  PDBG_CORE_ENGINE  Engine,
    _In_  DBG_CORE_REGISTER RegisterId,
    _Out_ PVOID             Content
)
{
    STRING               String;
    ULONG64              Temp[ 4 ];
    DBGGDB_AMD64_EXECUTE Amd64Execute;
    UCHAR                MovCr3[ ] = { 0x0F, 0x20, 0xD8 };

    String.Length = 0;
    String.MaximumLength = sizeof( DbgGdbMessageBuffer );
    String.Buffer = DbgGdbMessageBuffer;

    if ( RegisterId == Amd64RegisterCr3 ) {

        DbgGdbAmd64Execute( Engine,
                            &Amd64Execute,
                            MovCr3,
                            1,
                            3 );

        Temp[ 0 ] = Amd64Execute.Rax;

        RtlCopyMemory( Content, Temp, DbgAmd64RegisterSizeTable[ RegisterId ] );

        return DBG_STATUS_OKAY;
    }
    else {

        DbgGdbSendf( Engine,
                     "p%x",
                     RegisterId );

        DbgGdbRecvPacket( Engine, &String );

        DbgGdbDecodeHex( Temp,
                         DbgGdbMessageBuffer,
                         ( ULONG32 )String.Length / 2 );

        RtlCopyMemory( Content, Temp, DbgAmd64RegisterSizeTable[ RegisterId ] );

        //
        // TODO: Gdb size table
        //

        return DBG_STATUS_OKAY;
    }
}

#if 0
DBG_STATUS
DbgGdbSetContext(
    _Out_ PCONTEXT Context
)
{
    UCHAR      Buffer[ 0x148 + 1 ] = { 0 };
    STRING     String;
    DWORD      Segment;
    DBG_STATUS Status;

    String.Buffer = Buffer;
    String.Length = sizeof( Buffer );
    String.MaximumLength = sizeof( Buffer );

    Buffer[ 0 ] = 'G';

    DbgGdbEncodeHex( Buffer + 1, &Context->Rax, 8 );
    DbgGdbEncodeHex( Buffer + 1 + 16, &Context->Rbx, 8 );
    DbgGdbEncodeHex( Buffer + 1 + 32, &Context->Rcx, 8 );
    DbgGdbEncodeHex( Buffer + 1 + 48, &Context->Rdx, 8 );
    DbgGdbEncodeHex( Buffer + 1 + 64, &Context->Rsi, 8 );
    DbgGdbEncodeHex( Buffer + 1 + 80, &Context->Rdi, 8 );
    DbgGdbEncodeHex( Buffer + 1 + 96, &Context->Rbp, 8 );
    DbgGdbEncodeHex( Buffer + 1 + 112, &Context->Rsp, 8 );
    DbgGdbEncodeHex( Buffer + 1 + 128, &Context->R8, 8 );
    DbgGdbEncodeHex( Buffer + 1 + 144, &Context->R9, 8 );
    DbgGdbEncodeHex( Buffer + 1 + 160, &Context->R10, 8 );
    DbgGdbEncodeHex( Buffer + 1 + 176, &Context->R11, 8 );
    DbgGdbEncodeHex( Buffer + 1 + 192, &Context->R12, 8 );
    DbgGdbEncodeHex( Buffer + 1 + 208, &Context->R13, 8 );
    DbgGdbEncodeHex( Buffer + 1 + 224, &Context->R14, 8 );
    DbgGdbEncodeHex( Buffer + 1 + 240, &Context->R15, 8 );

    DbgGdbEncodeHex( Buffer + 1 + 256, &Context->Rip, 8 );

    DbgGdbEncodeHex( Buffer + 1 + 272, &Context->EFlags, 4 );
    Segment = ( DWORD )Context->SegCs;
    DbgGdbEncodeHex( Buffer + 1 + 280, &Segment, 4 );
    Segment = ( DWORD )Context->SegSs;
    DbgGdbEncodeHex( Buffer + 1 + 288, &Segment, 4 );
    Segment = ( DWORD )Context->SegDs;
    DbgGdbEncodeHex( Buffer + 1 + 296, &Segment, 4 );
    Segment = ( DWORD )Context->SegEs;
    DbgGdbEncodeHex( Buffer + 1 + 304, &Segment, 4 );
    Segment = ( DWORD )Context->SegFs;
    DbgGdbEncodeHex( Buffer + 1 + 312, &Segment, 4 );
    Segment = ( DWORD )Context->SegGs;
    DbgGdbEncodeHex( Buffer + 1 + 320, &Segment, 4 );

    Status = DbgGdbDevice.Gdb.DbgSendPacket( &String );
    if ( !DBG_SUCCESS( Status ) ) {

        return Status;
    }

    //
    // Returns "OK".
    //

    return DbgGdbDevice.Gdb.DbgRecvPacket( &String );
}
#endif

DBG_STATUS
DbgGdbProcessorBreak(
    _In_  PDBG_CORE_ENGINE Engine,
    _Out_ PULONG32         Processor
)
{
    CHAR       Buffer[ 512 ];
    STRING     String;
    DBG_STATUS Status;

    String.Length = 1;
    String.MaximumLength = 512;
    String.Buffer = Buffer;

    Buffer[ 0 ] = 0x03;

    Engine->DbgCommDevice.DbgSend( &Engine->DbgCommDevice.Extension,
                                   Buffer,
                                   1 );

    Status = DbgGdbRecvPacket( Engine, &String );

    if ( !DBG_SUCCESS( Status ) ) {

        return Status;
    }

    Status = DbgGdbProcessorStep( Engine );

    if ( !DBG_SUCCESS( Status ) ) {

        return Status;
    }

    return DbgGdbProcessorQuery( Engine, Processor );
}

DBG_STATUS
DbgGdbProcessorContinue(
    _In_ PDBG_CORE_ENGINE Engine
)
{
    CHAR   Buffer;
    STRING String;

    Buffer = 'c';

    String.Length = 1;
    String.MaximumLength = 1;
    String.Buffer = &Buffer;

    return DbgGdbSendPacket( Engine, &String );
}

ULONG32 DbgGdbBpTypeTable[ ] = {
    GDB_BP_HARDWARE,
    GDB_BP_WATCH_READ,
    GDB_BP_WATCH_WRITE,
    GDB_BP_WATCH_ACCESS
};

DBG_STATUS
DbgGdbBreakpointInsert(
    _In_ PDBG_CORE_ENGINE    Engine,
    _In_ DBG_CORE_BREAKPOINT Breakpoint,
    _In_ ULONG_PTR           Address,
    _In_ ULONG32             Length
)
{
    STRING  String;

    String.Length = 0;
    String.MaximumLength = sizeof( DbgGdbMessageBuffer );
    String.Buffer = DbgGdbMessageBuffer;

    DbgGdbSendf( Engine, "Z%d,%016llx,%d", DbgGdbBpTypeTable[ Breakpoint ], Address, Length );

    return DBG_SUCCESS( DbgGdbRecvPacket( Engine, &String ) ) &&
        DbgGdbMessageBuffer[ 0 ] != 'E' ? DBG_STATUS_OKAY : DBG_STATUS_ERROR;
}

DBG_STATUS
DbgGdbBreakpointClear(
    _In_ PDBG_CORE_ENGINE    Engine,
    _In_ DBG_CORE_BREAKPOINT Breakpoint,
    _In_ ULONG_PTR           Address
)
{
    STRING String;

    String.Length = 0;
    String.MaximumLength = sizeof( DbgGdbMessageBuffer );
    String.Buffer = DbgGdbMessageBuffer;

    DbgGdbSendf( Engine, "z%d,%016llx,1", DbgGdbBpTypeTable[ Breakpoint ], Address );

    return DbgGdbRecvPacket( Engine, &String );
}

// TODO: ERROR CHECK
DBG_STATUS
DbgGdbProcessorQuery(
    _In_  PDBG_CORE_ENGINE Engine,
    _Out_ PULONG32         Processor
)
{
    STRING String;
    USHORT Number;

    String.Length = 0;
    String.MaximumLength = sizeof( DbgGdbMessageBuffer );
    String.Buffer = DbgGdbMessageBuffer;

    DbgGdbSendf( Engine, "qC" );

    DbgGdbRecvPacket( Engine, &String );

    Number = String.Buffer[ 2 ] - '1';
    *Processor = ( ULONG32 )Number;

    return DBG_STATUS_OKAY;
}

DBG_STATUS
DbgGdbProcessorSwitch(
    _In_ PDBG_CORE_ENGINE Engine,
    _In_ ULONG32          Processor
)
{
    STRING String;

    String.Length = 0;
    String.MaximumLength = sizeof( DbgGdbMessageBuffer );
    String.Buffer = DbgGdbMessageBuffer;

    Processor++;

    DbgGdbSendf( Engine, "Hg%d", Processor );

    return DbgGdbRecvPacket( Engine, &String );
}

DBG_STATUS
DbgGdbAmd64Execute(
    _In_ PDBG_CORE_ENGINE      Engine,
    _In_ PDBGGDB_AMD64_EXECUTE Context,
    _In_ PVOID                 Code,
    _In_ ULONG32               CodeCount,
    _In_ ULONG32               CodeLength
)
{
    STATIC ULONG64       DbgKdKvaSafeAddress = 0;

    UCHAR                CodeOriginal[ 128 ];
    DBGGDB_AMD64_EXECUTE SaveContext;
    ULONG64              CodeAddress;
    ULONG32              CurrentOp;
    DBG_STATUS           Status;

    USHORT               OptHeadersLength;
    IMAGE_SECTION_HEADER CodeSection;

    //
    // TODO: Keep track of current processor.
    //

    ULONG32              CurrentProcessor;

    Engine->DbgRegisterRead( Engine, Amd64RegisterSegCs, &Context->SegCs );

    if ( DbgKdKernelBase == 0 ) {

        while ( Context->SegCs != 0x10 ) {

            //
            // Code can only reach here, if this is the first execution,
            // used for getting the kernel base.
            //

            Engine->DbgProcessorContinue( Engine );
            Sleep( 100 );
            Engine->DbgProcessorBreak( Engine, &CurrentProcessor );
            Engine->DbgRegisterRead( Engine, Amd64RegisterSegCs, &Context->SegCs );
            DbgKdpTraceLogLevel1( DbgGdbFactory, "Step until kernel mode => SegCs=%04lx\n", Context->SegCs );
        }
    }

    //
    // TODO: Because we cannot get a safe-to-execute-on page
    //       whilst having such little information, we have to
    //       step-by-step until we reach a kernel address,
    //       Before calling this function. In reality, we should
    //       find the kernel code section, and execute it at offset 0,
    //       but it doesn't matter a great deal for small code snippets.
    //

    Engine->DbgRegisterRead( Engine, Amd64RegisterRax, &SaveContext.Rax );
    Engine->DbgRegisterRead( Engine, Amd64RegisterRbx, &SaveContext.Rbx );
    Engine->DbgRegisterRead( Engine, Amd64RegisterRcx, &SaveContext.Rcx );
    Engine->DbgRegisterRead( Engine, Amd64RegisterRdx, &SaveContext.Rdx );
    Engine->DbgRegisterRead( Engine, Amd64RegisterRsi, &SaveContext.Rsi );
    Engine->DbgRegisterRead( Engine, Amd64RegisterRdi, &SaveContext.Rdi );
    Engine->DbgRegisterRead( Engine, Amd64RegisterRbp, &SaveContext.Rbp );
    Engine->DbgRegisterRead( Engine, Amd64RegisterRsp, &SaveContext.Rsp );
    Engine->DbgRegisterRead( Engine, Amd64RegisterR8, &SaveContext.R8 );
    Engine->DbgRegisterRead( Engine, Amd64RegisterR9, &SaveContext.R9 );
    Engine->DbgRegisterRead( Engine, Amd64RegisterR10, &SaveContext.R10 );
    Engine->DbgRegisterRead( Engine, Amd64RegisterR11, &SaveContext.R11 );
    Engine->DbgRegisterRead( Engine, Amd64RegisterR12, &SaveContext.R12 );
    Engine->DbgRegisterRead( Engine, Amd64RegisterR13, &SaveContext.R13 );
    Engine->DbgRegisterRead( Engine, Amd64RegisterR14, &SaveContext.R14 );
    Engine->DbgRegisterRead( Engine, Amd64RegisterR15, &SaveContext.R15 );

    Engine->DbgRegisterRead( Engine, Amd64RegisterRip, &SaveContext.Rip );
    Engine->DbgRegisterRead( Engine, Amd64RegisterEFlags, &SaveContext.EFlags );
    Engine->DbgRegisterRead( Engine, Amd64RegisterSegCs, &SaveContext.SegCs );
    Engine->DbgRegisterRead( Engine, Amd64RegisterSegSs, &SaveContext.SegSs );
    Engine->DbgRegisterRead( Engine, Amd64RegisterSegDs, &SaveContext.SegDs );
    Engine->DbgRegisterRead( Engine, Amd64RegisterSegEs, &SaveContext.SegEs );
    Engine->DbgRegisterRead( Engine, Amd64RegisterSegFs, &SaveContext.SegFs );
    Engine->DbgRegisterRead( Engine, Amd64RegisterSegGs, &SaveContext.SegGs );

    if ( DbgKdKernelBase != 0 ) {

        if ( DbgKdKvaSafeAddress == 0 ) {

            Engine->DbgMemoryRead(
                Engine,
                DbgKdNtosHeaders + FIELD_OFFSET(
                    IMAGE_NT_HEADERS64,
                    FileHeader.SizeOfOptionalHeader ),
                sizeof( USHORT ),
                &OptHeadersLength );

            do {
                Engine->DbgMemoryRead(
                    Engine,
                    DbgKdNtosHeaders +
                    OptHeadersLength +
                    FIELD_OFFSET( IMAGE_NT_HEADERS64, OptionalHeader ),
                    sizeof( IMAGE_SECTION_HEADER ),
                    &CodeSection );

                OptHeadersLength += sizeof( IMAGE_SECTION_HEADER );
            } while ( memcmp( CodeSection.Name, "KVASCODE", 8 ) != 0 );

            DbgKdKvaSafeAddress = DbgKdKernelBase + CodeSection.VirtualAddress;
        }

        Context->Rip = DbgKdKvaSafeAddress;
        Context->SegCs = DbgKdDebuggerBlock.GdtR0Code;
        Context->SegDs = DbgKdDebuggerBlock.GdtR0Data;
        Context->SegEs = DbgKdDebuggerBlock.GdtR0Data;
        Context->SegFs = DbgKdDebuggerBlock.GdtR0Data;
        Context->SegGs = DbgKdDebuggerBlock.GdtR0Pcr;
        Context->SegSs = DbgKdDebuggerBlock.GdtR0Data;
        Context->EFlags = 0x2;

        DbgKdpTraceLogLevel1( DbgGdbFactory,
                              "Executing at code base: %016llx\n",
                              DbgKdKvaSafeAddress );
    }
    else {

        Context->Rip = SaveContext.Rip;
        Context->SegCs = SaveContext.SegCs;
        Context->SegDs = SaveContext.SegDs;
        Context->SegEs = SaveContext.SegEs;
        Context->SegFs = SaveContext.SegFs;
        Context->SegGs = SaveContext.SegGs;
        Context->SegSs = SaveContext.SegSs;
        Context->EFlags = SaveContext.EFlags;
    }

    Engine->DbgRegisterWrite( Engine, Amd64RegisterRax, &Context->Rax );
    Engine->DbgRegisterWrite( Engine, Amd64RegisterRbx, &Context->Rbx );
    Engine->DbgRegisterWrite( Engine, Amd64RegisterRcx, &Context->Rcx );
    Engine->DbgRegisterWrite( Engine, Amd64RegisterRdx, &Context->Rdx );
    Engine->DbgRegisterWrite( Engine, Amd64RegisterRsi, &Context->Rsi );
    Engine->DbgRegisterWrite( Engine, Amd64RegisterRdi, &Context->Rdi );
    Engine->DbgRegisterWrite( Engine, Amd64RegisterRbp, &Context->Rbp );
    Engine->DbgRegisterWrite( Engine, Amd64RegisterRsp, &Context->Rsp );
    Engine->DbgRegisterWrite( Engine, Amd64RegisterR8, &Context->R8 );
    Engine->DbgRegisterWrite( Engine, Amd64RegisterR9, &Context->R9 );
    Engine->DbgRegisterWrite( Engine, Amd64RegisterR10, &Context->R10 );
    Engine->DbgRegisterWrite( Engine, Amd64RegisterR11, &Context->R11 );
    Engine->DbgRegisterWrite( Engine, Amd64RegisterR12, &Context->R12 );
    Engine->DbgRegisterWrite( Engine, Amd64RegisterR13, &Context->R13 );
    Engine->DbgRegisterWrite( Engine, Amd64RegisterR14, &Context->R14 );
    Engine->DbgRegisterWrite( Engine, Amd64RegisterR15, &Context->R15 );

    Engine->DbgRegisterWrite( Engine, Amd64RegisterRip, &Context->Rip );
    Engine->DbgRegisterWrite( Engine, Amd64RegisterEFlags, &Context->EFlags );
    Engine->DbgRegisterWrite( Engine, Amd64RegisterSegCs, &Context->SegCs );
    Engine->DbgRegisterWrite( Engine, Amd64RegisterSegSs, &Context->SegSs );
    Engine->DbgRegisterWrite( Engine, Amd64RegisterSegDs, &Context->SegDs );
    Engine->DbgRegisterWrite( Engine, Amd64RegisterSegEs, &Context->SegEs );
    Engine->DbgRegisterWrite( Engine, Amd64RegisterSegFs, &Context->SegFs );
    Engine->DbgRegisterWrite( Engine, Amd64RegisterSegGs, &Context->SegGs );

    CodeAddress = Context->Rip;

    Status = Engine->DbgMemoryRead( Engine,
                                    CodeAddress,
                                    CodeLength,
                                    CodeOriginal );
    if ( !DBG_SUCCESS( Status ) ) {

        return Status;
    }

    Status = Engine->DbgMemoryWrite( Engine,
                                     CodeAddress,
                                     CodeLength,
                                     Code );
    if ( !DBG_SUCCESS( Status ) ) {

        return Status;
    }

    for ( CurrentOp = 0; CurrentOp < CodeCount; CurrentOp++ ) {

        Status = Engine->DbgProcessorStep( Engine );
    }

    Status = Engine->DbgMemoryWrite( Engine,
                                     CodeAddress,
                                     CodeLength,
                                     CodeOriginal );
    if ( !DBG_SUCCESS( Status ) ) {

        return Status;
    }

    //
    // Save new code context
    //

    Engine->DbgRegisterRead( Engine, Amd64RegisterRax, &Context->Rax );
    Engine->DbgRegisterRead( Engine, Amd64RegisterRbx, &Context->Rbx );
    Engine->DbgRegisterRead( Engine, Amd64RegisterRcx, &Context->Rcx );
    Engine->DbgRegisterRead( Engine, Amd64RegisterRdx, &Context->Rdx );
    Engine->DbgRegisterRead( Engine, Amd64RegisterRsi, &Context->Rsi );
    Engine->DbgRegisterRead( Engine, Amd64RegisterRdi, &Context->Rdi );
    Engine->DbgRegisterRead( Engine, Amd64RegisterRbp, &Context->Rbp );
    Engine->DbgRegisterRead( Engine, Amd64RegisterRsp, &Context->Rsp );
    Engine->DbgRegisterRead( Engine, Amd64RegisterR8, &Context->R8 );
    Engine->DbgRegisterRead( Engine, Amd64RegisterR9, &Context->R9 );
    Engine->DbgRegisterRead( Engine, Amd64RegisterR10, &Context->R10 );
    Engine->DbgRegisterRead( Engine, Amd64RegisterR11, &Context->R11 );
    Engine->DbgRegisterRead( Engine, Amd64RegisterR12, &Context->R12 );
    Engine->DbgRegisterRead( Engine, Amd64RegisterR13, &Context->R13 );
    Engine->DbgRegisterRead( Engine, Amd64RegisterR14, &Context->R14 );
    Engine->DbgRegisterRead( Engine, Amd64RegisterR15, &Context->R15 );

    //
    // Restore original context
    //

    Engine->DbgRegisterWrite( Engine, Amd64RegisterRax, &SaveContext.Rax );
    Engine->DbgRegisterWrite( Engine, Amd64RegisterRbx, &SaveContext.Rbx );
    Engine->DbgRegisterWrite( Engine, Amd64RegisterRcx, &SaveContext.Rcx );
    Engine->DbgRegisterWrite( Engine, Amd64RegisterRdx, &SaveContext.Rdx );
    Engine->DbgRegisterWrite( Engine, Amd64RegisterRsi, &SaveContext.Rsi );
    Engine->DbgRegisterWrite( Engine, Amd64RegisterRdi, &SaveContext.Rdi );
    Engine->DbgRegisterWrite( Engine, Amd64RegisterRbp, &SaveContext.Rbp );
    Engine->DbgRegisterWrite( Engine, Amd64RegisterRsp, &SaveContext.Rsp );
    Engine->DbgRegisterWrite( Engine, Amd64RegisterR8, &SaveContext.R8 );
    Engine->DbgRegisterWrite( Engine, Amd64RegisterR9, &SaveContext.R9 );
    Engine->DbgRegisterWrite( Engine, Amd64RegisterR10, &SaveContext.R10 );
    Engine->DbgRegisterWrite( Engine, Amd64RegisterR11, &SaveContext.R11 );
    Engine->DbgRegisterWrite( Engine, Amd64RegisterR12, &SaveContext.R12 );
    Engine->DbgRegisterWrite( Engine, Amd64RegisterR13, &SaveContext.R13 );
    Engine->DbgRegisterWrite( Engine, Amd64RegisterR14, &SaveContext.R14 );
    Engine->DbgRegisterWrite( Engine, Amd64RegisterR15, &SaveContext.R15 );

    Engine->DbgRegisterWrite( Engine, Amd64RegisterRip, &SaveContext.Rip );
    Engine->DbgRegisterWrite( Engine, Amd64RegisterEFlags, &SaveContext.EFlags );
    Engine->DbgRegisterWrite( Engine, Amd64RegisterSegCs, &SaveContext.SegCs );
    Engine->DbgRegisterWrite( Engine, Amd64RegisterSegSs, &SaveContext.SegSs );
    Engine->DbgRegisterWrite( Engine, Amd64RegisterSegDs, &SaveContext.SegDs );
    Engine->DbgRegisterWrite( Engine, Amd64RegisterSegEs, &SaveContext.SegEs );
    Engine->DbgRegisterWrite( Engine, Amd64RegisterSegFs, &SaveContext.SegFs );
    Engine->DbgRegisterWrite( Engine, Amd64RegisterSegGs, &SaveContext.SegGs );

    return Status;
}

DBG_STATUS
DbgGdbAmd64MsrRead(
    _In_  PDBG_CORE_ENGINE Engine,
    _In_  ULONG32          Register,
    _Out_ PULONG64         Buffer
)
{
    DBG_STATUS           Status;
    DBGGDB_AMD64_EXECUTE Amd64Rdmsr = { 0 };
    UCHAR                CodeRdmsr[ 2 ] = { 0x0F, 0x32 };

    Amd64Rdmsr.Rcx = ( ULONG64 )Register;
    Amd64Rdmsr.Rax = ( ULONG64 )Register;

    Status = DbgGdbAmd64Execute( Engine,
                                 &Amd64Rdmsr,
                                 CodeRdmsr,
                                 1,
                                 2 );

    if ( Amd64Rdmsr.Rax == Register ) {

        return DBG_STATUS_ERROR;
    }

    *Buffer = Amd64Rdmsr.Rax | Amd64Rdmsr.Rdx << 32;

    return Status;
}

DBG_STATUS
DbgGdbAmd64MsrWrite(
    _In_ PDBG_CORE_ENGINE Engine,
    _In_ ULONG32          Register,
    _In_ ULONG64          Buffer
)
{
    DBG_STATUS           Status;
    DBGGDB_AMD64_EXECUTE Amd64Wrmsr = { 0 };
    UCHAR                CodeWrmsr[ 2 ] = { 0x0F, 0x30 };

    Amd64Wrmsr.Rcx = ( ULONG64 )Register;
    Amd64Wrmsr.Rdx = Buffer >> 32;
    Amd64Wrmsr.Rax = ( ULONG64 )( ULONG32 )Buffer;

    Status = DbgGdbAmd64Execute( Engine,
                                 &Amd64Wrmsr,
                                 CodeWrmsr,
                                 1,
                                 2 );

    return Status;
}
