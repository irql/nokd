
#pragma once

//

// Entry count of KdpBreakpointTable, 32 on windows.
#define KD_BREAKPOINT_TABLE_LENGTH      256

//

#include <ntifs.h>

//nonstandard extension, function / data pointer conversion in expression
#pragma warning( disable : 4152 ) 
//nonstandard extension used : nameless struct / union
#pragma warning( disable : 4201 ) 
//nonstandard extension used : bit field types other than int
#pragma warning( disable : 4214 )

#define MM_COPY_ADDRESS_PHYSICAL (0x00000001)
#define MM_COPY_ADDRESS_VIRTUAL  (0x00000002)

NTSTATUS
MmCopyMemory(
    _In_ PVOID   Target,
    _In_ PVOID   Source,
    _In_ SIZE_T  Length,
    _In_ ULONG   Flags,
    _In_ PSIZE_T TransferLength
);

NTSTATUS
MmCopyVirtualMemory(
    _In_  PEPROCESS       SourceProcess,
    _In_  PVOID           SourceAddress,
    _In_  PEPROCESS       TargetProcess,
    _In_  PVOID           TargetAddress,
    _In_  SIZE_T          Length,
    _In_  KPROCESSOR_MODE PreviousMode,
    _Out_ PSIZE_T         TransferLength
);

//
// DBGKD structures which are mostly
// message headers, and other general 
// enumerations and structures.
//

#include "dbgkd.h"

//
// STRUCTURES, ENUMERATIONS AND TYPEDEFS
//

typedef struct _KD_UART_CONTROL {
    ULONG64 Index;
    USHORT  Base;
} KD_UART_CONTROL, *PKD_UART_CONTROL;

typedef enum _KD_UART_STATUS {
    KdUartSuccess,
    KdUartError,
    KdUartNoData

} KD_UART_STATUS, *PKD_UART_STATUS;

typedef struct _KD_PORT *PKD_PORT;

typedef struct _KD_PORT {

    KD_UART_CONTROL  Uart;
    KD_UART_STATUS( *Send )(
        _In_ PKD_PORT Port,
        _In_ UCHAR    Byte );
    KD_UART_STATUS( *Recv )(
        _In_ PKD_PORT Port,
        _In_ PUCHAR   Byte );

} KD_PORT, *PKD_PORT;

typedef struct _KD_PRCB {
    ULONG32           Number;
    KIRQL             DebuggerSavedIrql;
    XSAVE_AREA_HEADER SupervisorState;
    PVOID             NtPrcb;

} KD_PRCB, *PKD_PRCB;

typedef enum _KD_STATUS {
    KdStatusOkay,
    KdStatusTimeOut,
    KdStatusResend,
} KD_STATUS, *PKD_STATUS;

#define KD_BPE_SET      (0x00000001)

typedef struct _KD_BREAKPOINT_ENTRY {
    PEPROCESS Process;
    ULONG64   Address;
    ULONG32   Flags;
    UCHAR     Content[ 8 ];
} KD_BREAKPOINT_ENTRY, *PKD_BREAKPOINT_ENTRY;

//
// definitions for kdapi, which are 
// used per KD_API_NUMBER, declared
// inside kdapi.c.
//

#define KDAPI 
#include "kdapi.h"

//
// FUNCTION DEFINITIONS
//

KD_UART_STATUS
KdUart16550InitializePort(
    _In_ PKD_PORT Port,
    _In_ ULONG64  Index
);

NTSTATUS
KdImageAddress(
    _In_      PCHAR  ImageName,
    _Out_opt_ PVOID* ImageBase,
    _Out_opt_ ULONG* ImageSize
);

NTSTATUS
KdImageSection(
    _In_      PVOID  ImageBase,
    _In_      PCHAR  SectionName,
    _Out_opt_ PVOID* SectionBase,
    _Out_opt_ ULONG* SectionSize
);

PVOID
KdSearchSignature(
    _In_ PVOID BaseAddress,
    _In_ ULONG Length,
    _In_ PCHAR Signature
);

KD_STATUS
KdpSendControlPacket(
    _In_ ULONG32 PacketType,
    _In_ ULONG32 PacketId
);

KD_STATUS
KdSendPacket(
    _In_ ULONG32     PacketType,
    _In_ PSTRING     MessageHeader,
    _In_ PSTRING     MessageData,
    _In_ PKD_CONTEXT Context
);

KD_STATUS
KdReceivePacket(
    _In_  ULONG32     PacketType,
    _Out_ PSTRING     Head,
    _Out_ PSTRING     Body,
    _Out_ PULONG32    Length,
    _In_  PKD_CONTEXT Context
);

KD_UART_STATUS
KdpRecvString(
    _In_ PVOID String,
    _In_ ULONG Length
);

KD_UART_STATUS
KdpSendString(
    _In_ PVOID String,
    _In_ ULONG Length
);

BOOLEAN
KdPollBreakIn(

);

BOOLEAN
KdpSendWaitContinue(
    _In_ ULONG64  Unused,
    _In_ PSTRING  StateChangeHead,
    _In_ PSTRING  StateChangeBody,
    _In_ PCONTEXT Context
);

VOID
KdpSetCommonState(
    _In_    ULONG32                  ApiNumber,
    _In_    PCONTEXT                 Context,
    _Inout_ PDBGKD_WAIT_STATE_CHANGE Change
);

//
// IMPORTS
//



//
// VARIABLES
//

EXTERN_C BOOLEAN( *KeFreezeExecution )(

    );
EXTERN_C BOOLEAN( *KeThawExecution )(
    _In_ BOOLEAN EnableInterrupts
    );
EXTERN_C ULONG64( *KeSwitchFrozenProcessor )(
    _In_ ULONG32 Number
    );
EXTERN_C PVOID( *MmGetPagedPoolCommitPointer )(

    );
EXTERN_C NTSTATUS( *KdpSysReadControlSpace )(
    _In_  ULONG  Processor,
    _In_  ULONG  Address,
    _In_  PVOID  Buffer,
    _In_  ULONG  Length,
    _Out_ PULONG TransferLength
    );
EXTERN_C NTSTATUS( *KdpSysWriteControlSpace )(
    _In_  ULONG  Processor,
    _In_  ULONG  Address,
    _In_  PVOID  Buffer,
    _In_  ULONG  Length,
    _Out_ PULONG TransferLength
    );
EXTERN_C VOID( *KdpGetStateChange )(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PCONTEXT                  Context
    );

EXTERN_C PBOOLEAN            KdPitchDebugger;
EXTERN_C PULONG              KdpDebugRoutineSelect;

EXTERN_C BOOLEAN             KdDebuggerNotPresent_;
EXTERN_C BOOLEAN             KdEnteredDebugger;
EXTERN_C KTIMER              KdBreakTimer;
EXTERN_C KDPC                KdBreakDpc;

EXTERN_C KDDEBUGGER_DATA64   KdDebuggerDataBlock;
EXTERN_C DBGKD_GET_VERSION64 KdVersionBlock;
EXTERN_C LIST_ENTRY          KdpDebuggerDataListHead;
EXTERN_C ULONG64             KdpLoaderDebuggerBlock;
EXTERN_C KD_CONTEXT          KdpContext;

EXTERN_C KD_PORT             KdpPortDevice;

EXTERN_C PUSHORT             KeProcessorLevel;
