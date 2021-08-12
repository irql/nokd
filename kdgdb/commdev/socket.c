

#include <kdgdb.h>

DBG_STATUS
DbgSocketSend(
    _In_ PDBG_CORE_EXTENSION Extension,
    _In_ PVOID               Buffer,
    _In_ ULONG               Length
);

DBG_STATUS
DbgSocketRecv(
    _In_  PDBG_CORE_EXTENSION Extension,
    _Out_ PVOID               Buffer,
    _In_  ULONG               Length
);

DBG_STATUS
DbgSocketFlush(
    _In_ PDBG_CORE_EXTENSION Extension
);

DBG_STATUS
DbgSocketInit(
    _In_ PDBG_CORE_DEVICE Device,
    _In_ PCSTR            Address,
    _In_ USHORT           Port
)
{
    //
    // TODO: Error check.
    //

    WSADATA     WSA;
    SOCKADDR_IN SocketAddress;
    ULONG32     NonBlocking;

    WSAStartup( 0x0202, &WSA );

    Device->Extension.Socket.SocketHandle = WSASocketW( AF_INET,
                                                        SOCK_STREAM,
                                                        IPPROTO_IP,
                                                        NULL,
                                                        0,
                                                        0 );

    RtlZeroMemory( &SocketAddress, sizeof( SOCKADDR_IN ) );
    SocketAddress.sin_family = AF_INET;
    SocketAddress.sin_addr.s_addr = inet_addr( Address );
    SocketAddress.sin_port = htons( Port );
    //(Port << 8) | (Port >> 8)

    WSAConnect( Device->Extension.Socket.SocketHandle,
        ( const struct sockaddr* )&SocketAddress,
                sizeof( SOCKADDR_IN ),
                NULL,
                NULL,
                0,
                0 );

    NonBlocking = TRUE;
    ioctlsocket( Device->Extension.Socket.SocketHandle, FIONBIO, &NonBlocking );

    Device->DbgSend = DbgSocketSend;
    Device->DbgRecv = DbgSocketRecv;
    Device->DbgFlush = DbgSocketFlush;

    return DBG_STATUS_OKAY;
}

DBG_STATUS
DbgSocketSend(
    _In_ PDBG_CORE_EXTENSION Extension,
    _In_ PVOID               Buffer,
    _In_ ULONG               Length
)
{
    return send( Extension->Socket.SocketHandle,
                 Buffer,
                 Length,
                 0 ) == Length ? DBG_STATUS_OKAY : DBG_STATUS_ERROR;
}

DBG_STATUS
DbgSocketRecv(
    _In_  PDBG_CORE_EXTENSION Extension,
    _Out_ PVOID               Buffer,
    _In_  ULONG               Length
)
{
    return recv( Extension->Socket.SocketHandle,
                 Buffer,
                 Length,
                 0 ) == Length ? DBG_STATUS_OKAY : DBG_STATUS_ERROR;
}

DBG_STATUS
DbgSocketFlush(
    _In_ PDBG_CORE_EXTENSION Extension
)
{
    UCHAR Buffer;
    while ( DBG_SUCCESS( DbgSocketRecv( Extension, &Buffer, 1 ) ) )
        ;

    return DBG_STATUS_OKAY;
}
