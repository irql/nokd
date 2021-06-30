
#include <kd.h>
#include <vmwrpc.h>

//
// TODO: MUST CALL KdVmwRpcRecvCommandFinish AFTER A FUNCTION
//       FAILURE, OTHERWISE NO NEW RPC DATA WILL BE DELIVERED
//       TO THE RESPECTIVE CHANNEL.
//

NTSTATUS
KdVmwRpcBufferedIoPrep(
    _In_ ULONG32 Length
);

NTSTATUS
KdVmwRpcBufferedIoSend(
    _In_ PVOID   Buffer,
    _In_ ULONG32 Length
);

NTSTATUS
KdVmwRpcBufferedIoDone(

);

static CHAR KdRpcCmdHead[ ] = "~kdVMvA ";
static CHAR KdRpcCmdRecp[ ] = "++kdVMvA ";

typedef enum _KD_RPC_CMD {
    KdRpcTestConnection = 't',
    KdRpcRecvPacket = 'r',
    KdRpcSendPacket = 's',
    KdRpcGetVersion = 'v'
} KD_RPC_CMD, *PKD_RPC_CMD;

#define KD_LS_NOT_PRESENT   0x01
#define KD_LS_RETRY         0x02
#define KD_LS_ENABLED_AVAIL 0x04

#define KD_RPC_RECV_RETURNED_ULONGS 5
#define KD_RPC_SEND_PASSED_ULONGS   4

//
// 0x101 is the VirtualKD version uploaded to github, but supporting
// the redux version is a larger concern, so i've upped it to 2020.2 and 
// start reading about their interface.
//

// 2020 << 16 | 2 => 2020.2
#define KD_RPC_PROTOCOL_VERSION     0x07E40002


#define KD_RPC_TEST_BUFFER_SIZE     512

//
// Sob, fuck you.
//
//C_ASSERT( FIELD_OFFSET( STRING, Buffer ) == 4 );

#pragma pack( push, 2 )

typedef struct _STRING_HEAD {
    USHORT Length;
    USHORT MaximumLength;
} STRING_HEAD, *PSTRING_HEAD;

C_ASSERT( FIELD_OFFSET( STRING, MaximumLength ) ==
          FIELD_OFFSET( STRING_HEAD, MaximumLength ) );
C_ASSERT( sizeof( STRING_HEAD ) == 4 );

#pragma pack( pop )
//C_ASSERT( sizeof( STRING_HEAD ) == FIELD_OFFSET( STRING, Buffer ) );

NTSTATUS
KdVmwRpcInitialize(

)
{
    NTSTATUS ntStatus;

    ntStatus = KdVmwRpcOpenChannel( &KdDebugDevice.VmwRpc );

    if ( !NT_SUCCESS( ntStatus ) ) {

        return ntStatus;
    }

    DbgPrint( "KdDebugDevice.VmwRpc.Channel set! %d\n", KdDebugDevice.VmwRpc.Channel );

    KdDebugDevice.KdSendPacket = KdVmwRpcSendPacket;
    KdDebugDevice.KdReceivePacket = KdVmwRpcRecvPacket;

    return NT_SUCCESS( KdVmwRpcInitProtocol( ) ) ? KdVmwRpcTestConnection( ) : STATUS_UNSUCCESSFUL;
}

NTSTATUS
KdVmwRpcInitProtocol(

)
{
    //
    // Negotiate a protocol version,
    // 

    NTSTATUS ntStatus;
    CHAR     CommandType;
    ULONG32  Version;
    ULONG32  RecvLength;
    CHAR     Sig[ sizeof( KdRpcCmdRecp ) ];

    CommandType = KdRpcGetVersion;
    Version = KD_RPC_PROTOCOL_VERSION;

    KdVmwRpcBufferedIoPrep( sizeof( KdRpcCmdHead ) + sizeof( ULONG32 ) );

    KdVmwRpcBufferedIoSend( &KdRpcCmdHead, sizeof( KdRpcCmdHead ) - 1 );
    KdVmwRpcBufferedIoSend( &CommandType, 1 );
    KdVmwRpcBufferedIoSend( &Version, sizeof( ULONG32 ) );
    ntStatus = KdVmwRpcBufferedIoDone( );

    if ( !NT_SUCCESS( ntStatus ) ) {

        DbgPrint( "KdVmwRpcInitProtocol failed on KdVmwRpcBufferedIoDone.\n" );
        return ntStatus;
    }

    KdVmwRpcRecvCommandLength( &KdDebugDevice.VmwRpc, &RecvLength );
    if ( RecvLength < sizeof( ULONG32 ) + sizeof( KdRpcCmdRecp ) - 1 + 2 ) {

        return STATUS_CONNECTION_REFUSED;
    }

    KdVmwRpcRecvCommandBuffer( &KdDebugDevice.VmwRpc, 2, Sig );

    if ( Sig[ 0 ] != '1' ||
         Sig[ 1 ] != ' ' ) {

        KdVmwRpcRecvCommandFinish( &KdDebugDevice.VmwRpc );
        return STATUS_CONNECTION_REFUSED;
    }

    KdVmwRpcRecvCommandBuffer( &KdDebugDevice.VmwRpc, sizeof( KdRpcCmdRecp ) - 1, Sig );

    if ( memcmp( Sig, KdRpcCmdRecp, sizeof( KdRpcCmdRecp ) - 1 ) != 0 ) {

        KdVmwRpcRecvCommandFinish( &KdDebugDevice.VmwRpc );
        return STATUS_CONNECTION_REFUSED;
    }

    KdVmwRpcRecvCommandBuffer( &KdDebugDevice.VmwRpc, sizeof( ULONG32 ), &Version );

    if ( Version != KD_RPC_PROTOCOL_VERSION ) {

        KdVmwRpcRecvCommandFinish( &KdDebugDevice.VmwRpc );
        return STATUS_PROTOCOL_NOT_SUPPORTED;
    }

    KdVmwRpcRecvCommandFinish( &KdDebugDevice.VmwRpc );

    DbgPrint( "KdVmwRpcInitProtocol version negotiation successful, %d.%d\n",
              KD_RPC_PROTOCOL_VERSION >> 16,
              KD_RPC_PROTOCOL_VERSION & 0xFFFF );

    return STATUS_SUCCESS;
}

NTSTATUS
KdVmwRpcTestConnection(

)
{
    NTSTATUS ntStatus;
    CHAR     CommandType;
    UCHAR    Buffer[ KD_RPC_TEST_BUFFER_SIZE ];
    ULONG32  CurrentChar;
    ULONG32  RecvLength;

    CommandType = KdRpcTestConnection;

    for ( CurrentChar = 0;
          CurrentChar < KD_RPC_TEST_BUFFER_SIZE;
          CurrentChar++ ) {

        Buffer[ CurrentChar ] = ( UCHAR )CurrentChar;
    }

    KdVmwRpcBufferedIoPrep( KD_RPC_TEST_BUFFER_SIZE + sizeof( KdRpcCmdHead ) );

    KdVmwRpcBufferedIoSend( KdRpcCmdHead, sizeof( KdRpcCmdHead ) - 1 );
    KdVmwRpcBufferedIoSend( &CommandType, 1 );
    KdVmwRpcBufferedIoSend( Buffer, KD_RPC_TEST_BUFFER_SIZE );
    ntStatus = KdVmwRpcBufferedIoDone( );

    if ( !NT_SUCCESS( ntStatus ) ) {

        DbgPrint( "KdVmwRpcTestConnection failed on KdVmwRpcBufferedIoDone.\n" );
        return ntStatus;
    }

    KdVmwRpcRecvCommandLength( &KdDebugDevice.VmwRpc, &RecvLength );
    if ( RecvLength <
         sizeof( KdRpcCmdRecp ) +
         KD_RPC_TEST_BUFFER_SIZE +
         2 - 1 ) {

        return STATUS_CONNECTION_REFUSED;
    }

    KdVmwRpcRecvCommandBuffer( &KdDebugDevice.VmwRpc, sizeof( KdRpcCmdRecp ) - 1 + 2, Buffer );

    if ( Buffer[ 0 ] != '1' &&
         Buffer[ 1 ] != ' ' ) {
        // virtualkd doesn't compare the expected space here, in their implementation

        KdVmwRpcRecvCommandFinish( &KdDebugDevice.VmwRpc );
        return STATUS_CONNECTION_REFUSED;
    }

    if ( memcmp( Buffer + 2, KdRpcCmdRecp, sizeof( KdRpcCmdRecp ) - 1 ) != 0 ) {

        KdVmwRpcRecvCommandFinish( &KdDebugDevice.VmwRpc );
        return STATUS_CONNECTION_REFUSED;
    }

    KdVmwRpcRecvCommandBuffer( &KdDebugDevice.VmwRpc, KD_RPC_TEST_BUFFER_SIZE, Buffer );
    KdVmwRpcRecvCommandFinish( &KdDebugDevice.VmwRpc );

    for ( CurrentChar = 0;
          CurrentChar < KD_RPC_TEST_BUFFER_SIZE;
          CurrentChar++ ) {

        if ( Buffer[ CurrentChar ] != ( UCHAR )( CurrentChar ^ 0x55 ) ) {

            return STATUS_CONNECTION_ABORTED;
        }
    }

    return STATUS_SUCCESS;
}

KD_STATUS
KdVmwRpcSendPacket(
    _In_     KD_PACKET_TYPE PacketType,
    _In_     PSTRING        Head,
    _In_opt_ PSTRING        Body,
    _Inout_  PKD_CONTEXT    KdContext
)
{
    KdContext;

    KD_CONTEXT KdContextDup = { 0 };

    NTSTATUS ntStatus;
    CHAR     CommandType;
    ULONG32  HeadLength;
    ULONG32  BodyLength;
    ULONG32  LocalState;
    CHAR     Sig[ sizeof( KdRpcCmdRecp ) ];
    ULONG32  ULongSend[ KD_RPC_SEND_PASSED_ULONGS ];
    STRING   Buffer = { 0 };
    ULONG32  RecvLength;

    HeadLength = Head != NULL ? Head->Length : 0;
    BodyLength = Body != NULL ? Body->Length : 0;

    while ( TRUE ) {

        LocalState = KD_LS_ENABLED_AVAIL | 1 << 8;
        if ( KdDebuggerNotPresent_ ) {

            LocalState |= KD_LS_NOT_PRESENT;
        }

        ULongSend[ 0 ] = PacketType;
        ULongSend[ 1 ] = HeadLength;
        ULongSend[ 2 ] = BodyLength;
        ULongSend[ 3 ] = LocalState;

        CommandType = KdRpcSendPacket;

        KdVmwRpcBufferedIoPrep( sizeof( KdRpcCmdHead ) +
                                sizeof( ULONG32 ) * KD_RPC_SEND_PASSED_ULONGS +
                                2 * sizeof( STRING_HEAD ) +//FIELD_OFFSET( STRING, Buffer ) +
                                sizeof( KD_CONTEXT ) +
                                HeadLength + BodyLength );
        KdVmwRpcBufferedIoSend( KdRpcCmdHead, sizeof( KdRpcCmdHead ) - 1 );
        KdVmwRpcBufferedIoSend( &CommandType, 1 );
        KdVmwRpcBufferedIoSend( Head == NULL ? &Buffer : Head, sizeof( STRING_HEAD ) );//FIELD_OFFSET( STRING, Buffer ) );
        KdVmwRpcBufferedIoSend( Body == NULL ? &Buffer : Body, sizeof( STRING_HEAD ) );//FIELD_OFFSET( STRING, Buffer ) );
        KdVmwRpcBufferedIoSend( KdContext == NULL ? &KdContextDup : KdContext, sizeof( KD_CONTEXT ) );
        KdVmwRpcBufferedIoSend( ULongSend, sizeof( ULONG32 ) * KD_RPC_SEND_PASSED_ULONGS );

        if ( HeadLength != 0 ) {

            KdVmwRpcBufferedIoSend( Head->Buffer, HeadLength );
        }

        if ( BodyLength != 0 ) {

            KdVmwRpcBufferedIoSend( Body->Buffer, BodyLength );
        }

        ntStatus = KdVmwRpcBufferedIoDone( );

        if ( !NT_SUCCESS( ntStatus ) ) {

            DbgPrint( "KdVmwRpcSendPacket failed on KdVmwRpcBufferedIoDone.\n" );
            return KdStatusError;
        }

        KdVmwRpcRecvCommandLength( &KdDebugDevice.VmwRpc, &RecvLength );

        if ( RecvLength <
             sizeof( KdRpcCmdRecp ) +
             2 + sizeof( KD_CONTEXT ) +
             sizeof( ULONG ) ) {

            DbgPrint( "KdVmwRpcSendPacket invalid reply size: %d\n", RecvLength );

            KdVmwRpcRecvCommandFinish( &KdDebugDevice.VmwRpc );
            return KdStatusError;
        }

        KdVmwRpcRecvCommandBuffer( &KdDebugDevice.VmwRpc, 2, Sig );

        if ( Sig[ 0 ] != '1' ||
             Sig[ 1 ] != ' ' ) {

            KdVmwRpcRecvCommandFinish( &KdDebugDevice.VmwRpc );
            return KdStatusError;
        }

        KdVmwRpcRecvCommandBuffer( &KdDebugDevice.VmwRpc, sizeof( KdRpcCmdRecp ) - 1, Sig );
        KdVmwRpcRecvCommandBuffer( &KdDebugDevice.VmwRpc, 1, &CommandType );

        if ( memcmp( Sig, KdRpcCmdRecp, sizeof( KdRpcCmdRecp ) - 1 ) != 0 ||
             CommandType != KdRpcSendPacket ) {

            KdVmwRpcRecvCommandFinish( &KdDebugDevice.VmwRpc );
            return KdStatusError;
        }

        KdVmwRpcRecvCommandBuffer( &KdDebugDevice.VmwRpc,
                                   sizeof( KD_CONTEXT ),
                                   KdContext == NULL ? &KdContextDup : KdContext );

        KdVmwRpcRecvCommandBuffer( &KdDebugDevice.VmwRpc, sizeof( ULONG32 ), &LocalState );
        KdVmwRpcRecvCommandFinish( &KdDebugDevice.VmwRpc );

        KdDebuggerNotPresent_ = ( LocalState & KD_LS_NOT_PRESENT ) != 0;
        if ( LocalState & KD_LS_ENABLED_AVAIL ) {

            // Care.
        }

        if ( LocalState & KD_LS_RETRY ) {

            continue;
        }

        break;
    }

    return KdStatusSuccess;
}

KD_STATUS
KdVmwRpcRecvPacket(
    _In_    KD_PACKET_TYPE PacketType,
    _Inout_ PSTRING        Head,
    _Inout_ PSTRING        Body,
    _Out_   PULONG32       Length,
    _Inout_ PKD_CONTEXT    KdContext
)
{
    KdContext;

    //
    //! DO NOT RELY ON THE LENGTH PARAMETER
    //! USE (Head/Body)->Length INSTEAD!
    //

    //
    // Having individual handlers for packet recv & send apis
    // is because of VirtualKD, this implements a different
    // protocol, somewhat, because everything goes through their
    // host program, which forwards packets to dbgeng.dll
    //

    C_ASSERT( sizeof( KD_PACKET_TYPE ) == sizeof( ULONG32 ) );

    KD_CONTEXT KdContextDup = { 0 };

    NTSTATUS ntStatus;
    CHAR     CommandType;
    ULONG32  LocalState;
    ULONG32  RecvLength;
    ULONG32  RecvLengthMinimum;
    STRING   Buffer = { 0 };
    CHAR     Sig[ sizeof( KdRpcCmdRecp ) ];
    ULONG32  ULongRecv[ KD_RPC_RECV_RETURNED_ULONGS ];

    CommandType = KdRpcRecvPacket;

    LocalState = KD_LS_ENABLED_AVAIL | 1 << 8;
    if ( KdDebuggerNotPresent_ ) {

        LocalState |= KD_LS_NOT_PRESENT;
    }

    KdVmwRpcBufferedIoPrep( sizeof( KdRpcCmdHead ) +
                            2 * sizeof( STRING_HEAD ) +//FIELD_OFFSET( STRING, Buffer ) +
                            2 * sizeof( ULONG32 ) +
                            sizeof( KD_CONTEXT ) );
    KdVmwRpcBufferedIoSend( KdRpcCmdHead, sizeof( KdRpcCmdHead ) - 1 );
    KdVmwRpcBufferedIoSend( &CommandType, 1 );
    KdVmwRpcBufferedIoSend( &PacketType, sizeof( ULONG32 ) );
    KdVmwRpcBufferedIoSend( &LocalState, sizeof( ULONG32 ) );
    KdVmwRpcBufferedIoSend( Head == NULL ? &Buffer : Head, sizeof( STRING_HEAD ) );//FIELD_OFFSET( STRING, Buffer ) );
    KdVmwRpcBufferedIoSend( Body == NULL ? &Buffer : Body, sizeof( STRING_HEAD ) );//FIELD_OFFSET( STRING, Buffer ) );
    KdVmwRpcBufferedIoSend( KdContext == NULL ? &KdContextDup : KdContext, sizeof( KD_CONTEXT ) );
    ntStatus = KdVmwRpcBufferedIoDone( );

    if ( !NT_SUCCESS( ntStatus ) ) {

        DbgPrint( "KdVmwRpcRecvPacket failed on KdVmwRpcBufferedIoDone.\n" );
        return ntStatus;
    }

    KdVmwRpcRecvCommandLength( &KdDebugDevice.VmwRpc, &RecvLength );

    RecvLengthMinimum = sizeof( KdRpcCmdRecp ) +
        2 * sizeof( STRING_HEAD ) +//FIELD_OFFSET( STRING, Buffer ) +
        sizeof( KD_CONTEXT ) +
        sizeof( ULONG32 ) * KD_RPC_RECV_RETURNED_ULONGS +
        2;

    if ( RecvLength < RecvLengthMinimum ) {

        DbgPrint( "KdVmwRpcRecvPacket failed, has: %d, min: %d\n", RecvLength, RecvLengthMinimum );
        return KdStatusError;
    }

    KdVmwRpcRecvCommandBuffer( &KdDebugDevice.VmwRpc, 2, &Sig );

    if ( Sig[ 0 ] != '1' &&
         Sig[ 1 ] != ' ' ) {

        DbgPrint( "KdVmwRpcRecvPacket failed, no sig %d\n", RecvLength );
        KdVmwRpcRecvCommandFinish( &KdDebugDevice.VmwRpc );
        return KdStatusError;
    }

    KdVmwRpcRecvCommandBuffer( &KdDebugDevice.VmwRpc,
                               sizeof( KdRpcCmdRecp ) - 1,
                               Sig );
    KdVmwRpcRecvCommandBuffer( &KdDebugDevice.VmwRpc,
                               1,
                               &CommandType );
    if ( memcmp( Sig, KdRpcCmdRecp, sizeof( KdRpcCmdRecp ) - 1 ) != 0 ||
         CommandType != KdRpcRecvPacket ) {

        DbgPrint( "KdVmwRpcRecvPacket failed, bad recp sig\n" );
        KdVmwRpcRecvCommandFinish( &KdDebugDevice.VmwRpc );
        return KdStatusError;
    }

    KdVmwRpcRecvCommandBuffer( &KdDebugDevice.VmwRpc,
                               sizeof( STRING_HEAD ),//FIELD_OFFSET( STRING, Buffer ),
                               Head == NULL ? &Buffer : Head );
    KdVmwRpcRecvCommandBuffer( &KdDebugDevice.VmwRpc,
                               sizeof( STRING_HEAD ),//FIELD_OFFSET( STRING, Buffer ),
                               Body == NULL ? &Buffer : Body );

    KdVmwRpcRecvCommandBuffer( &KdDebugDevice.VmwRpc,
                               sizeof( KD_CONTEXT ),
                               KdContext == NULL ? &KdContextDup : KdContext );

    KdVmwRpcRecvCommandBuffer( &KdDebugDevice.VmwRpc,
                               sizeof( ULONG32 ) * KD_RPC_RECV_RETURNED_ULONGS,
                               ULongRecv );

    if ( Head != NULL ) {

        //DbgPrint( "Head->Length: %d => %d\n", Head->Length, ULongRecv[ 2 ] );
    }

    if ( Body != NULL ) {

        //DbgPrint( "Body->Length: %d => %d\n", Body->Length, ULongRecv[ 3 ] );
    }

    //
    // TODO: Range check on ULongRecv[2, 3] against KdTransportMaxPacketSize.
    //
    // TODO: Potentially adjust all RecvLength checks. (<, !=)
    //

    //DbgPrint( "ULongRecv: %d %d\n", ULongRecv[ 2 ], ULongRecv[ 3 ] );

    if ( RecvLength !=
         sizeof( KdRpcCmdRecp ) +
         ULongRecv[ 2 ] +
         ULongRecv[ 3 ] +
         2 * sizeof( STRING_HEAD ) +//FIELD_OFFSET( STRING, Buffer ) +
         sizeof( KD_CONTEXT ) +
         sizeof( ULONG32 ) * KD_RPC_RECV_RETURNED_ULONGS + 2 ) {

        KdVmwRpcRecvCommandFinish( &KdDebugDevice.VmwRpc );
        DbgPrint( "KdVmwRpcRecvPacket failed2, has: %d, min: %d\n", RecvLength, RecvLengthMinimum );
        return KdStatusError;
    }

    if ( Head != NULL ) {

        //DbgPrint( "Head->MaximumLength: %d => %d\n", Head->MaximumLength, ULongRecv[ 2 ] );
    }

    if ( Body != NULL ) {

        //DbgPrint( "Body->MaximumLength: %d => %d\n", Body->MaximumLength, ULongRecv[ 3 ] );
    }

    //
    // Surprised code actually reaches this point, RecvLength has had inconsistent
    // and invalid values for a while.
    //

    if ( Head != NULL && Head->MaximumLength >= ULongRecv[ 2 ] && ULongRecv[ 2 ] > 0 ) {

        KdVmwRpcRecvCommandBuffer( &KdDebugDevice.VmwRpc,
                                   ULongRecv[ 2 ],
                                   Head->Buffer );
    }

    if ( Body != NULL && Body->MaximumLength >= ULongRecv[ 3 ] && ULongRecv[ 3 ] > 0 ) {

        KdVmwRpcRecvCommandBuffer( &KdDebugDevice.VmwRpc,
                                   ULongRecv[ 3 ],
                                   Body->Buffer );
    }

    //
    // ULongRecv[ 1 ] is seemingly invalid, perhaps 
    // *Length = ULongRecv[ 2 ] + ULongRecv[ 3 ] + sizeof(...)
    // and manually calculate this size.
    //

    if ( Length != NULL ) {

        *Length = ULongRecv[ 1 ];
    }

    LocalState = ULongRecv[ 4 ];

    KdDebuggerNotPresent_ = ( LocalState & KD_LS_NOT_PRESENT ) != 0;

    if ( LocalState & KD_LS_ENABLED_AVAIL ) {

        // Care.
    }

    KdVmwRpcRecvCommandFinish( &KdDebugDevice.VmwRpc );

    //DbgPrint( "done! ULongRecv->Result=%d\n", ULongRecv[ 0 ] );

    return ( KD_STATUS )ULongRecv[ 0 ];
}

//
// No need for any synchronization.
//
static ULONG32 KdpVmwRpcBufferedIoLength;
static ULONG32 KdpVmwRpcBufferedIoIndex;
static UCHAR   KdpVmwRpcBufferedIoBuffer[ 0x1000 ];

NTSTATUS
KdVmwRpcBufferedIoPrep(
    _In_ ULONG32 Length
)
{
    if ( Length > 0x1000 ) {

        return STATUS_INSUFFICIENT_RESOURCES;
    }

    KdpVmwRpcBufferedIoLength = Length;
    KdpVmwRpcBufferedIoIndex = 0;
    RtlZeroMemory( KdpVmwRpcBufferedIoBuffer, 0x1000 );

    return STATUS_SUCCESS;
}

NTSTATUS
KdVmwRpcBufferedIoSend(
    _In_ PVOID   Buffer,
    _In_ ULONG32 Length
)
{

    if ( Length > KdpVmwRpcBufferedIoIndex + KdpVmwRpcBufferedIoLength ) {

        return STATUS_INVALID_BUFFER_SIZE;
    }

    RtlCopyMemory( KdpVmwRpcBufferedIoBuffer + KdpVmwRpcBufferedIoIndex,
                   Buffer,
                   Length );
    KdpVmwRpcBufferedIoIndex += Length;

    if ( KdpVmwRpcBufferedIoIndex >= KdpVmwRpcBufferedIoLength ) {

        //DbgPrint( "KdVmwRpcBufferedIoDone privately called.\n" );
        //KdVmwRpcBufferedIoDone( );
    }

    return STATUS_SUCCESS;
}

NTSTATUS
KdVmwRpcBufferedIoDone(

)
{
    NT_ASSERT( KdpVmwRpcBufferedIoLength >= KdpVmwRpcBufferedIoIndex );

    KdVmwRpcSendCommandLength( &KdDebugDevice.VmwRpc,
                               KdpVmwRpcBufferedIoLength );
    KdVmwRpcSendCommandBuffer( &KdDebugDevice.VmwRpc,
                               KdpVmwRpcBufferedIoLength,
                               KdpVmwRpcBufferedIoBuffer );

    return STATUS_SUCCESS;
}
