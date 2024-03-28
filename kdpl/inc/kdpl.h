
#pragma once

#pragma warning(disable:4201)
#pragma warning(disable:4214)

#define KDPL

#define KD_STARTUP_SIG \
"              __          __     \n" \
"             /\\ \\        /\\ \\    \n" \
"  ___     ___\\ \\ \\/'\\    \\_\\ \\   \n" \
"/' _ `\\  / __`\\ \\ , <    /'_` \\  \n" \
"/\\ \\/\\ \\/\\ \\L\\ \\ \\ \\\\`\\ /\\ \\L\\ \\ \n" \
"\\ \\_\\ \\_\\ \\____/\\ \\_\\ \\_\\ \\___,_\\\n" \
" \\/_/\\/_/\\/___/  \\/_/\\/_/\\/__,_ /\n" \
"\n"

#include <ntifs.h>
#include <ntstrsafe.h>
#undef KdPrint
#define DBGKD_H
#include "dbgkd.h"

#define MM_COPY_MEMORY_PHYSICAL             0x1
#define MM_COPY_MEMORY_VIRTUAL              0x2

#define KD_BREAKPOINT_TABLE_LENGTH          32

typedef struct _KD_VMWRPC_CONTROL {
    ULONG32 Cookie1;
    ULONG32 Cookie2;
    ULONG32 Channel;
    ULONG32 RecvId;

    ULONG32 RecvExtraLength;
    PVOID   RecvExtraBuffer;
} KD_VMWRPC_CONTROL, *PKD_VMWRPC_CONTROL;

typedef struct _KD_DEBUG_DEVICE *PKD_DEBUG_DEVICE;

#define KD_BPE_SET      (0x00000001)
#define KD_BPE_OWE      (0x00000002)

typedef struct _KD_BREAKPOINT_ENTRY {
    PEPROCESS Process;
    ULONG64   Address;
    ULONG32   Flags;
    ULONG32   ContentLength;
    UCHAR     Content[ 0x20 ];
} KD_BREAKPOINT_ENTRY, *PKD_BREAKPOINT_ENTRY;

typedef enum _KD_STATUS {
    KdStatusSuccess = 0,
    KdStatusTimeOut = 1,
    KdStatusError   = 2,
    KdStatusResend,
} KD_STATUS, *PKD_STATUS;

typedef struct _KD_DEBUG_DEVICE {

    //
    // This structure is supposed to replace the interface of which 
    // kd* dll's provide, these dll's export a few functions which the OS 
    // would call, such as KdReceivePacket & KdSendPacket, and they are 
    // responsible for communication between the target & host. 
    //

    union {
        KD_VMWRPC_CONTROL VmwRpc;
    };

    //
    // List of kd* dll api's:
    //  KdReceivePacket
    //  KdSendPacket
    //  KdPower
    //  KdInitialize
    //  KdSetHiberRange
    //

    KD_STATUS( *KdSendPacket )(
        _In_     KD_PACKET_TYPE PacketType,
        _In_     PSTRING        Head,
        _In_opt_ PSTRING        Body,
        _Inout_  PKD_CONTEXT    KdContext );
    KD_STATUS( *KdReceivePacket )(
        _In_    KD_PACKET_TYPE PacketType,
        _Inout_ PSTRING        Head,
        _Inout_ PSTRING        Body,
        _Out_   PULONG32       Length,
        _Inout_ PKD_CONTEXT    KdContext );

} KD_DEBUG_DEVICE, *PKD_DEBUG_DEVICE;

EXTERN_C KD_DEBUG_DEVICE             KdDebugDevice;

EXTERN_C UCHAR                       KdpMessageBuffer[ 0x1000 ];
EXTERN_C CHAR                        KdpPathBuffer[ 0x1000 ];

EXTERN_C PUSHORT                     KeProcessorLevel;
EXTERN_C ULONG32                     KdDebuggerNotPresent_;

EXTERN_C KD_BREAKPOINT_ENTRY         KdpBreakpointTable[ KD_BREAKPOINT_TABLE_LENGTH ];

EXTERN_C KDDEBUGGER_DATA64           KdDebuggerDataBlock;

EXTERN_C DBGKD_GET_VERSION64         KdVersionBlock;
EXTERN_C LIST_ENTRY                  KdpDebuggerDataListHead;
EXTERN_C ULONG64                     KdpLoaderDebuggerBlock;
EXTERN_C KD_CONTEXT                  KdpContext;

#include "vmwrpc.h"
#include "uart.h"
#include "kddef.h"

#define DbgKdQueryCurrentProcessorNumber( ) DbgKdRead32( ( ULONG32* )( ( ULONG64 )DbgKdQueryPcr( ) + 0x1A4 ) )//DbgKdRead32( ( ULONG32* )( ( ULONG64 )DbgKdQueryPcr( ) + 0x184 ) )
#define DbgKdQueryCurrentPrcbNumber( )      DbgKdRead32( ( ULONG32* )( ( ULONG64 )DbgKdQueryPcr( ) + 0x1A4 ) )
#define DbgKdQueryCurrentThread( )          DbgKdRead64( ( ULONG64* )( ( ULONG64 )DbgKdQueryPcr( ) + 0x188 ) )

//

ULONG64
DbgKdRead64(
    _In_ PULONG64 Address
);

ULONG32
DbgKdRead32(
    _In_ PULONG32 Address
);

USHORT
DbgKdRead16(
    _In_ PUSHORT Address
);

UCHAR
DbgKdRead8(
    _In_ PUCHAR Address
);

//

VOID
KdpSetContextState(
    _Inout_ PDBGKD_WAIT_STATE_CHANGE Change,
    _In_    PCONTEXT                 Context
);

VOID
KdpSetCommonState(
    _In_    ULONG32                  ApiNumber,
    _In_    PCONTEXT                 Context,
    _Inout_ PDBGKD_WAIT_STATE_CHANGE Change
);

BOOLEAN
KdpSendWaitContinue(
    _In_ ULONG64  Unused,
    _In_ PSTRING  StateChangeHead,
    _In_ PSTRING  StateChangeBody,
    _In_ PCONTEXT Context
);

BOOLEAN
KdpReportExceptionStateChange(
    _In_ PEXCEPTION_RECORD64 ExceptRecord,
    _In_ PCONTEXT            ExceptContext,
    _In_ BOOLEAN             SecondChance
);

BOOLEAN
KdpReportLoadSymbolsStateChange(
    _In_    PSTRING         PathName,
    _In_    PKD_SYMBOL_INFO Symbol,
    _In_    BOOLEAN         Unload,
    _Inout_ PCONTEXT        Context
);

VOID
KdpPrintString_(
    _In_ PANSI_STRING String
);

VOID
KdPrint_(
    _In_ PCHAR Format,
    _In_ ...
);

//
// KDAPI.C
//

KD_STATUS
KdpReadVirtualMemory(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body
);

KD_STATUS
KdpWriteVirtualMemory(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body
);

VOID
KdpGetBaseContext(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body,
    _Inout_ PCONTEXT                  Context
);

KD_STATUS
KdpGetContext(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body,
    _Inout_ PCONTEXT                  Context
);

KD_STATUS
KdpGetContextEx(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body,
    _Inout_ PCONTEXT                  Context
);

KD_STATUS
KdpSetContext(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body,
    _Inout_ PCONTEXT                  Context
);

KD_STATUS
KdpSetContextEx(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body,
    _Inout_ PCONTEXT                  Context
);

KD_STATUS
KdpGetStateChangeApi(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body,
    _Inout_ PCONTEXT                  Context
);

KD_STATUS
KdpReadControlSpace(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body
);

KD_STATUS
KdpWriteControlSpace(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body
);

KD_STATUS
KdpReadIoSpace(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body
);

KD_STATUS
KdpWriteIoSpace(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body
);

KD_STATUS
KdpReadPhysicalMemory(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body
);

KD_STATUS
KdpWritePhysicalMemory(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body
);

KD_STATUS
KdpGetVersion(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body
);

KD_STATUS
KdpQueryMemory(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body
);

KD_STATUS
KdpInsertBreakpoint(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body
);

KD_STATUS
KdpRemoveBreakpoint(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body
);