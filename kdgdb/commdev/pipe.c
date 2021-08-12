
#include <kdgdb.h>

DBG_STATUS
DbgPipeSend(
    _In_ PDBG_CORE_EXTENSION Extension,
    _In_ PVOID               Buffer,
    _In_ ULONG               Length
);

DBG_STATUS
DbgPipeRecv(
    _In_  PDBG_CORE_EXTENSION Extension,
    _Out_ PVOID               Buffer,
    _In_  ULONG               Length
);

DBG_STATUS
DbgPipeFlush(
    _In_ PDBG_CORE_EXTENSION Extension
);

DBG_STATUS
DbgPipeInit(
    _In_ PDBG_CORE_DEVICE Device,
    _In_ PCSTR            PipeName,
    _In_ BOOLEAN          OwnPipe
)
{

    if ( OwnPipe ) {

        Device->Extension.Pipe.FileHandle = CreateNamedPipeA( PipeName,
                                                              PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
                                                              PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_ACCEPT_REMOTE_CLIENTS,
                                                              128,
                                                              0x10000,
                                                              0x10000,
                                                              0,
                                                              NULL );
    }
    else {

        Device->Extension.Pipe.FileHandle = CreateFileA( PipeName,
                                                         GENERIC_READ | GENERIC_WRITE,
                                                         0,
                                                         NULL,
                                                         OPEN_EXISTING,
                                                         0,
                                                         NULL );
    }

    if ( Device->Extension.Pipe.FileHandle == INVALID_HANDLE_VALUE ) {

        return DBG_STATUS_ERROR;
    }

    Device->DbgSend = DbgPipeSend;
    Device->DbgRecv = DbgPipeRecv;
    Device->DbgFlush = DbgPipeFlush;

    return DBG_STATUS_OKAY;
}

DBG_STATUS
DbgPipeSend(
    _In_ PDBG_CORE_EXTENSION Extension,
    _In_ PVOID               Buffer,
    _In_ ULONG               Length
)
{
    DWORD BytesWritten;
    return WriteFile( Extension->Pipe.FileHandle,
                      Buffer,
                      Length,
                      &BytesWritten,
                      NULL ) ? DBG_STATUS_OKAY : DBG_STATUS_ERROR;
}

DBG_STATUS
DbgPipeRecv(
    _In_  PDBG_CORE_EXTENSION Extension,
    _Out_ PVOID               Buffer,
    _In_  ULONG               Length
)
{
    DWORD   BytesRead;
    ULONG32 TimeOut;

    TimeOut = 0;
    do {

        if ( PeekNamedPipe( Extension->Pipe.FileHandle, NULL, 0, NULL, &BytesRead, NULL ) &&
             BytesRead >= Length ) {

            break;
        }

        TimeOut++;

        if ( TimeOut >= 50 ) {

            return DBG_STATUS_ERROR;
        }

    } while ( TRUE );

    return ReadFile( Extension->Pipe.FileHandle,
                     Buffer,
                     Length,
                     &BytesRead,
                     NULL ) ? DBG_STATUS_OKAY : DBG_STATUS_ERROR;
}

DBG_STATUS
DbgPipeFlush(
    _In_ PDBG_CORE_EXTENSION Extension
)
{
    UCHAR Buffer[ 256 ];
    DWORD Bytes;
    DWORD Bytes2;

    while ( TRUE ) {
        PeekNamedPipe( Extension->Pipe.FileHandle,
                       NULL,
                       0,
                       NULL,
                       &Bytes,
                       NULL );
        if ( Bytes == 0 ) {

            return DBG_STATUS_OKAY;
        }

        ReadFile( Extension->Pipe.FileHandle,
                  Buffer,
                  Bytes > 256 ? 256 : Bytes,
                  &Bytes2,
                  NULL );
    }

    return DBG_STATUS_OKAY;
}
