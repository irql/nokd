
#include <kd.h>
#include <vmwrpc.h>

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

#define KD_RPC_PROTOCOL_VERSION 0x101

#define KD_RPC_TEST_BUFFER_SIZE 512

NTSTATUS
KdVmwRpcInitialize(
    _In_ PKD_DEBUG_DEVICE DebugDevice
)
{
    return KdVmwRpcOpenChannel( &DebugDevice->VmwRpc );
}

NTSTATUS
KdVmwRpcInitProtocol(

)
{
    //
    // Negotiate a protocol version,
    // 

    CHAR    CommandType;
    ULONG32 Version;
    ULONG32 RecvLength;
    CHAR    Sig[ sizeof( KdRpcCmdRecp ) ];

    CommandType = KdRpcGetVersion;
    Version = KD_RPC_PROTOCOL_VERSION;

    KdVmwRpcBufferedIoPrep( sizeof( KdRpcCmdHead ) + sizeof( ULONG32 ) );

    KdVmwRpcBufferedIoSend( &KdRpcCmdHead, sizeof( KdRpcCmdHead ) - 1 );
    KdVmwRpcBufferedIoSend( &CommandType, 1 );
    KdVmwRpcBufferedIoSend( &Version, sizeof( ULONG32 ) );

    KdVmwRpcRecvCommandLength( &KdDebugDevice.VmwRpc, &RecvLength );
    if ( RecvLength != sizeof( ULONG32 ) + sizeof( KdRpcCmdHead ) - 1 + 2 ) {

        return STATUS_CONNECTION_REFUSED;
    }

    KdVmwRpcRecvCommandBuffer( &KdDebugDevice.VmwRpc, 2, Sig );

    if ( Sig[ 0 ] != '1' ||
         Sig[ 1 ] != ' ' ) {

        return STATUS_CONNECTION_REFUSED;
    }

    KdVmwRpcRecvCommandBuffer( &KdDebugDevice.VmwRpc, sizeof( KdRpcCmdRecp ) - 1, Sig );

    if ( memcmp( Sig, KdRpcCmdRecp, sizeof( KdRpcCmdRecp ) - 1 ) != 0 ) {

        return STATUS_CONNECTION_REFUSED;
    }

    KdVmwRpcRecvCommandBuffer( &KdDebugDevice.VmwRpc, sizeof( ULONG32 ), &Version );

    if ( Version != KD_RPC_PROTOCOL_VERSION ) {

        return STATUS_PROTOCOL_NOT_SUPPORTED;
    }

    return STATUS_SUCCESS;
}

NTSTATUS
KdVmwRpcTestConnection(

)
{
    CHAR    CommandType;
    UCHAR   Buffer[ KD_RPC_TEST_BUFFER_SIZE ];
    ULONG32 CurrentChar;
    ULONG32 RecvLength;

    CommandType = KdRpcTestConnection;

    for ( CurrentChar = 0;
          CurrentChar < KD_RPC_TEST_BUFFER_SIZE;
          CurrentChar++ ) {

        Buffer[ CurrentChar ] = CurrentChar;
    }

    KdVmwRpcBufferedIoPrep( KD_RPC_TEST_BUFFER_SIZE + sizeof( KdRpcCmdHead ) );

    KdVmwRpcBufferedIoSend( KdRpcCmdHead, sizeof( KdRpcCmdHead ) - 1 );
    KdVmwRpcBufferedIoSend( &CommandType, 1 );
    KdVmwRpcBufferedIoSend( Buffer, KD_RPC_TEST_BUFFER_SIZE );

    KdVmwRpcRecvCommandLength( &KdDebugDevice.VmwRpc, &RecvLength );

    if ( RecvLength !=
         sizeof( KdRpcCmdRecp ) +
         KD_RPC_TEST_BUFFER_SIZE +
         2 - 1 ) {

        return STATUS_CONNECTION_REFUSED;
    }

    KdVmwRpcRecvCommandBuffer( &KdDebugDevice.VmwRpc, sizeof( KdRpcCmdRecp ) - 1 + 2, Buffer );

    if ( Buffer[ 0 ] != '1' ) {
        // virtualkd doesn't compare the expected space?

        return STATUS_CONNECTION_REFUSED;
    }

    if ( memcmp( Buffer + 2, KdRpcCmdRecp, sizeof( KdRpcCmdRecp ) - 1 ) != 0 ) {

        return STATUS_CONNECTION_REFUSED;
    }

    KdVmwRpcRecvCommandBuffer( &KdDebugDevice.VmwRpc, KD_RPC_TEST_BUFFER_SIZE, Buffer );
    KdVmwRpcRecvCommandFinish( &KdDebugDevice.VmwRpc );

    for ( CurrentChar = 0;
          CurrentChar < KD_RPC_TEST_BUFFER_SIZE;
          CurrentChar++ ) {

        if ( Buffer[ CurrentChar ] != ( UCHAR )( CurrentChar ^ 0x55 ) ) {

            return STATUS_CONNECTION_REFUSED;
        }
    }

    return STATUS_SUCCESS;
}

NTSTATUS
KdVmwRpcDebuggerInitialize0(

)
{
    //
    // The real version of this function has the LOADER_PARAMETER_BLOCK/KeLoaderBlock 
    // passed to it, however this is not useful to us, and destroyed after initialization
    // of the kernel. Although it is also located on some Kd internal structure we have.
    //

    KdVmwRpcInitProtocol( );


}

KD_STATUS
KdVmwRpcSendPacket(
    _In_     KD_PACKET_TYPE PacketType,
    _In_     PSTRING        Head,
    _In_opt_ PSTRING        Body,
    _Inout_  PKD_CONTEXT    KdContext
)
{

#pragma pack(push, 2)
    struct _KD_CONTEXT_DUP {
        ULONG32 RetryCount;
        BOOLEAN BreakRequested;
    } KdContextDup = { 0 };
#pragma pack(pop)

    CHAR    CommandType;
    ULONG32 HeadLength;
    ULONG32 BodyLength;
    ULONG32 LocalState;
    CHAR    Sig[ sizeof( KdRpcCmdHead ) ];
    ULONG32 ULongSend[ KD_RPC_SEND_PASSED_ULONGS ];
    STRING  Buffer = { 0 };
    ULONG32 RecvLength;
    ULONG32 RecvLengthMinimum;

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
                                2 * FIELD_OFFSET( STRING, Buffer ),
                                sizeof( struct _KD_CONTEXT_DUP ) +
                                HeadLength + BodyLength );
        KdVmwRpcBufferedIoSend( KdRpcCmdHead, sizeof( KdRpcCmdHead ) - 1 );
        KdVmwRpcBufferedIoSend( &CommandType, 1 );
        KdVmwRpcBufferedIoSend( Head == NULL ? &Buffer : Head, FIELD_OFFSET( STRING, Buffer ) );
        KdVmwRpcBufferedIoSend( Body == NULL ? &Buffer : Body, FIELD_OFFSET( STRING, Buffer ) );
        KdVmwRpcBufferedIoSend( &KdContextDup, sizeof( struct _KD_CONTEXT_DUP ) );
        KdVmwRpcBufferedIoSend( ULongSend, sizeof( ULONG32 ) * KD_RPC_SEND_PASSED_ULONGS );

        if ( HeadLength != 0 ) {

            KdVmwRpcBufferedIoSend( Head->Buffer, HeadLength );
        }

        if ( BodyLength != 0 ) {

            KdVmwRpcBufferedIoSend( Body->Buffer, BodyLength );
        }

        KdVmwRpcRecvCommandLength( &KdDebugDevice.VmwRpc, &RecvLength );

        if ( RecvLength !=
             sizeof( KdRpcCmdRecp ) +
             2 + sizeof( struct _KD_CONTEXT_DUP ) +
             sizeof( ULONG ) ) {

            return KdStatusError;
        }

        KdVmwRpcRecvCommandBuffer( &KdDebugDevice.VmwRpc, 2, Sig );

        if ( Sig[ 0 ] != '1' ||
             Sig[ 1 ] != ' ' ) {

            return KdStatusError;
        }

        KdVmwRpcRecvCommandBuffer( &KdDebugDevice.VmwRpc, sizeof( KdRpcCmdRecp ) - 1, Sig );
        KdVmwRpcRecvCommandBuffer( &KdDebugDevice.VmwRpc, 1, &CommandType );

        if ( memcmp( Sig, KdRpcCmdRecp, sizeof( KdRpcCmdRecp ) - 1 ) != 0 ||
             CommandType != KdRpcSendPacket ) {

            return KdStatusError;
        }

        KdVmwRpcRecvCommandBuffer( &KdDebugDevice.VmwRpc, sizeof( struct _KD_CONTEXT_DUP ), &KdContextDup );

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
    //
    // Having individual handlers for packet recv & send apis
    // is because of VirtualKD, this implements a different
    // protocol, somewhat, because everything goes through their
    // host program, which forwards packets to dbgeng.dll
    //

    C_ASSERT( sizeof( KD_PACKET_TYPE ) == sizeof( ULONG32 ) );

#pragma pack(push, 2)
    struct _KD_CONTEXT_DUP {
        ULONG32 RetryCount;
        BOOLEAN BreakRequested;
    } KdContextDup = { 0 };
#pragma pack(pop)

    CHAR    CommandType;
    ULONG32 LocalState;
    ULONG32 RecvLength;
    ULONG32 RecvLengthMinimum;
    STRING  Buffer = { 0 };
    CHAR    Sig[ sizeof( KdRpcCmdRecp ) ];
    ULONG32 ULongRecv[ KD_RPC_RECV_RETURNED_ULONGS ];

    CommandType = KdRpcRecvPacket;

    LocalState = KD_LS_ENABLED_AVAIL | 1 << 8;
    if ( KdDebuggerNotPresent_ ) {

        LocalState |= KD_LS_NOT_PRESENT;
    }

    KdVmwRpcBufferedIoPrep( sizeof( KdRpcCmdHead ) +
                            2 * FIELD_OFFSET( STRING, Buffer ) +
                            2 * sizeof( ULONG32 ) +
                            sizeof( struct _KD_CONTEXT_DUP ) );
    KdVmwRpcBufferedIoSend( KdRpcCmdHead, sizeof( KdRpcCmdHead ) );
    KdVmwRpcBufferedIoSend( &CommandType, sizeof( CHAR ) );
    KdVmwRpcBufferedIoSend( &PacketType, sizeof( ULONG32 ) );
    KdVmwRpcBufferedIoSend( &LocalState, sizeof( ULONG32 ) );
    KdVmwRpcBufferedIoSend( Head == NULL ? &Buffer : Head, FIELD_OFFSET( STRING, Buffer ) );
    KdVmwRpcBufferedIoSend( Body == NULL ? &Buffer : Body, FIELD_OFFSET( STRING, Buffer ) );
    KdVmwRpcBufferedIoSend( &KdContextDup, sizeof( struct _KD_CONTEXT_DUP ) );

    KdVmwRpcRecvCommandLength( &KdDebugDevice.VmwRpc, &RecvLength );

    RecvLengthMinimum = sizeof( KdRpcCmdRecp ) +
        2 * FIELD_OFFSET( STRING, Buffer ) +
        sizeof( struct _KD_CONTEXT_DUP ) +
        sizeof( ULONG32 ) * KD_RPC_RECV_RETURNED_ULONGS +
        2;

    if ( RecvLength == -1 || RecvLength < RecvLengthMinimum ) {

        return KdStatusError;
    }

    KdVmwRpcRecvCommandBuffer( &KdDebugDevice.VmwRpc, 2, &Sig );

    if ( Sig[ 0 ] != '1' &&
         Sig[ 1 ] != ' ' ) {

        return KdStatusError;
    }

    KdVmwRpcRecvCommandBuffer( &KdDebugDevice.VmwRpc,
                               sizeof( KdRpcCmdRecp ) - 1,
                               Sig );
    KdVmwRpcRecvCommandBuffer( &KdDebugDevice.VmwRpc,
                               &CommandType,
                               1 );
    if ( memcmp( Sig, KdRpcCmdRecp, sizeof( KdRpcCmdRecp ) - 1 ) != 0 ||
         CommandType != KdRpcRecvPacket ) {

        return KdStatusError;
    }

    KdVmwRpcRecvCommandBuffer( &KdDebugDevice.VmwRpc,
                               FIELD_OFFSET( STRING, Buffer ),
                               Head == NULL ? &Buffer : Head );
    KdVmwRpcRecvCommandBuffer( &KdDebugDevice.VmwRpc,
                               FIELD_OFFSET( STRING, Buffer ),
                               Body == NULL ? &Buffer : Body );

    KdVmwRpcRecvCommandBuffer( &KdDebugDevice.VmwRpc,
                               sizeof( ULONG32 ) * KD_RPC_RECV_RETURNED_ULONGS,
                               ULongRecv );
    if ( RecvLength !=
         sizeof( KdRpcCmdRecp ) +
         ULongRecv[ 2 ] +
         ULongRecv[ 3 ] +
         2 * FIELD_OFFSET( STRING, Buffer ) +
         sizeof( struct _KD_CONTEXT_DUP ) +
         sizeof( ULONG32 ) * KD_RPC_RECV_RETURNED_ULONGS + 2 ) {

        return KdStatusError;
    }

    if ( Head != NULL && Head->Buffer != NULL && Head->MaximumLength >= ULongRecv[ 2 ] ) {

        KdVmwRpcRecvCommandBuffer( &KdDebugDevice.VmwRpc,
                                   ULongRecv[ 2 ],
                                   Head->Buffer );
    }

    if ( Body != NULL && Body->Buffer != NULL && Body->MaximumLength >= ULongRecv[ 3 ] ) {

        KdVmwRpcRecvCommandBuffer( &KdDebugDevice.VmwRpc,
                                   ULongRecv[ 3 ],
                                   Body->Buffer );
    }

    *Length = ULongRecv[ 1 ];

    LocalState = ULongRecv[ 4 ];

    KdDebuggerNotPresent_ = ( LocalState & KD_LS_NOT_PRESENT ) != 0;

    if ( LocalState & KD_LS_ENABLED_AVAIL ) {

        // Care.
    }

    KdVmwRpcRecvCommandFinish( &KdDebugDevice.VmwRpc );

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

    return STATUS_SUCCESS;
}

NTSTATUS
KdVmwRpcBufferedIoDone(

)
{

    KdVmwRpcSendCommandLength( &KdDebugDevice.VmwRpc,
                               KdpVmwRpcBufferedIoLength );
    KdVmwRpcSendCommandBuffer( &KdDebugDevice.VmwRpc,
                               KdpVmwRpcBufferedIoLength,
                               KdpVmwRpcBufferedIoBuffer );

    return STATUS_SUCCESS;
}

KD_STATUS
KdVmwRpcRecvString(
    _In_ PKD_DEBUG_DEVICE DebugDevice,
    _In_ PVOID            String,
    _In_ ULONG            Length
)
{
    PVOID   ExtraBuffer;
    ULONG32 ExtraLength;

    ExtraBuffer = DebugDevice->VmwRpc.RecvExtraBuffer;
    ExtraLength = DebugDevice->VmwRpc.RecvExtraLength;

    //
    // This buffer should first copy ALL data from the RPC channel, into 
    // the ExtraBuffer, then be operated on from there. This also makes sure
    // that timeouts work properly if ExtraLength is too small. In the 
    // UART 16550 implementation, we discard any excess/overflow which doesn't
    // match the proper size demanded by it's KdSendString Length parameter.
    //

    if ( ExtraLength > 0 ) {

        if ( ExtraLength >= Length ) {

            RtlCopyMemory( String,
                           ExtraBuffer,
                           Length );
            ExtraLength -= Length;

            DebugDevice->VmwRpc.RecvExtraLength = ExtraLength;

            return KdStatusSuccess;
        }

        RtlCopyMemory( String,
                       ExtraBuffer,
                       ExtraLength );

        DebugDevice->VmwRpc.RecvExtraLength = 0;
        String = ( PUCHAR )String + ExtraLength;
    }

    KdVmwRpcRecvCommandLength( &DebugDevice->VmwRpc,
                               &ExtraLength );


    return KdStatusSuccess;
}

KD_STATUS
KdVmwRpcSendString(
    _In_ PKD_DEBUG_DEVICE DebugDevice,
    _In_ PVOID            String,
    _In_ ULONG            Length
)
{
    NTSTATUS ntStatus;

    ntStatus = KdVmwRpcSendCommandLength( &DebugDevice->VmwRpc,
                                          Length );
    if ( !NT_SUCCESS( ntStatus ) ) {

        return KdStatusError;
    }

    ntStatus = KdVmwRpcSendCommandBuffer( &DebugDevice->VmwRpc,
                                          Length,
                                          String );

    return NT_SUCCESS( ntStatus ) ? KdStatusSuccess : KdStatusError;
}
