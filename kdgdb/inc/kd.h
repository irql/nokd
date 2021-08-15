
#pragma once

EXTERN_C DBG_CORE_DEVICE     DbgKdpCommDevice;

EXTERN_C BOOLEAN             DbgKdDebuggerEnabled;

EXTERN_C UCHAR               DbgKdMessageBuffer[ ];
EXTERN_C ULONG32             DbgKdProcessorCount;
EXTERN_C ULONG32             DbgKdProcessorLevel;
EXTERN_C ULONG64             DbgKdKernelBase;

EXTERN_C ULONG64             KiWaitAlways;
EXTERN_C ULONG64             KiWaitNever;
EXTERN_C ULONG64             KdpDataBlockEncoded;

EXTERN_C KDDEBUGGER_DATA64   DbgKdDebuggerBlock;
EXTERN_C DBGKD_GET_VERSION64 DbgKdVersionBlock;

EXTERN_C ULONG64             DbgKdNtosHeaders;

EXTERN_C ULONG64             DbgKdProcessorBlock[ ];

EXTERN_C DBG_CORE_ENGINE     DbgCoreEngine;
EXTERN_C ULONG32             DbgKiFeatureSettings;

typedef struct _DBGKD_BREAKPOINT_ENTRY {
    ULONG64 Address;
    ULONG32 Flags;
} DBGKD_BREAKPOINT_ENTRY, *PDBGKD_BREAKPOINT_ENTRY;

#define DBGKD_BP_ENABLED    0x00000001L
#define DBGKD_BP_WATCHPOINT 0x00000010L
#define DBGKD_BP_EXECUTE    0x00000020L
#define DBGKD_BP_WRITE      0x00000040L
#define DBGKD_BP_READ_WRITE 0x00000080L

#define DBGKD_BP_MAXIMUM    0x20

EXTERN_C DBGKD_BREAKPOINT_ENTRY DbgKdBreakpointTable[ ];

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

VOID
DbgKdPrint(
    _In_ PCHAR Format,
    _In_ ...
);

VOID
DbgKdpDecodeDataBlock(
    _In_ PKDDEBUGGER_DATA64 DebuggerData
);

VOID
DbgKdpSetCommonState(
    _In_    ULONG32                  ApiNumber,
    _In_    PCONTEXT                 Context,
    _Inout_ PDBGKD_WAIT_STATE_CHANGE State
);

VOID
DbgKdpSetContextState(
    _Inout_ PDBGKD_WAIT_STATE_CHANGE State,
    _In_    PCONTEXT                 Context
);

VOID
DbgKdpReadVirtualMemory(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body
);

VOID
DbgKdpWriteVirtualMemory(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body
);

VOID
DbgKdpGetVersion(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body
);

VOID
DbgKdpGetContext(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body
);

VOID
DbgKdpSetContext(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body
);

VOID
DbgKdpGetContextEx(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body
);

VOID
DbgKdpSetContextEx(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body
);

VOID
DbgKdpReadControlSpace(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body
);

VOID
DbgKdpWriteControlSpace(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body
);

VOID
DbgKdpInsertBreakpoint(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body
);

VOID
DbgKdpClearBreakpoint(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body
);

typedef union _DEBUG_CONTROL {
    ULONG64 Long;
    struct {
        ULONG64 L0 : 1;
        ULONG64 G0 : 1;
        ULONG64 L1 : 1;
        ULONG64 G1 : 1;
        ULONG64 L2 : 1;
        ULONG64 G2 : 1;
        ULONG64 L3 : 1;
        ULONG64 G3 : 1;
        ULONG64 Le : 1;
        ULONG64 Ge : 1;
        ULONG64 Reserved_1_0 : 1;
        ULONG64 Rtm : 1;
        ULONG64 Reserved_0_0 : 1;
        ULONG64 Gd : 1;
        ULONG64 Reserved_0_1 : 2;
        ULONG64 Rwe0 : 2;
        ULONG64 Len0 : 2;
        ULONG64 Rwe1 : 2;
        ULONG64 Len1 : 2;
        ULONG64 Rwe2 : 2;
        ULONG64 Len2 : 2;
        ULONG64 Rwe3 : 2;
        ULONG64 Len3 : 2;
    };
} DEBUG_CONTROL, *PDEBUG_CONTROL;

typedef union _DEBUG_STATUS {
    ULONG64 Long;
    struct {
        ULONG64 B0 : 1;
        ULONG64 B1 : 1;
        ULONG64 B2 : 1;
        ULONG64 B3 : 1;
        ULONG64 Reserved_1_0 : 8;
        ULONG64 Reserved_0_0 : 1;
        ULONG64 Bd : 1;
        ULONG64 Bs : 1;
        ULONG64 Bt : 1;
        ULONG64 Rtm : 1;
        ULONG64 Reserved_1_1 : 15;
    };
} DEBUG_STATUS, *PDEBUG_STATUS;

typedef ULONG64 DEBUG_LINEAR, *PDEBUG_LINEAR;

typedef struct _DBGKD_DEBUG_STATE {
    //
    // This is the structure used to represent 
    // the debug registers being emulated.
    //
    // NOTES: 
    // 
    // 1. Emulation ignores BT flag, BD flag, RTM flag, GD flag
    // 2. LE & GE are emulated by default, regardless of the flag
    //

    DEBUG_LINEAR  Dr0;
    DEBUG_LINEAR  Dr1;
    DEBUG_LINEAR  Dr2;
    DEBUG_LINEAR  Dr3;
    DEBUG_STATUS  Dr6;
    DEBUG_CONTROL Dr7;

    DBGKD_BREAKPOINT_ENTRY Dr0AssocBp;
    DBGKD_BREAKPOINT_ENTRY Dr1AssocBp;
    DBGKD_BREAKPOINT_ENTRY Dr2AssocBp;
    DBGKD_BREAKPOINT_ENTRY Dr3AssocBp;

} DBGKD_DEBUG_STATE, *PDBGKD_DEBUG_STATE;

#define DEBUG_EXECUTE       0
#define DEBUG_WRITE         1
#define DEBUG_IO_RW         2
#define DEBUG_READ_WRITE    3

EXTERN_C DBGKD_DEBUG_STATE DbgKdpDebugEmu;

#if 0
#define IA32_DR6_B0         ( 1 << 0 )
#define IA32_DR6_B1         ( 1 << 1 )
#define IA32_DR6_B2         ( 1 << 2 )
#define IA32_DR6_B3         ( 1 << 3 )

#define IA32_DR6_BS         ( 1 << 14 )

#define IA32_DR7_L0         ( 1 << 0 )
#define IA32_DR7_L1         ( 1 << 2 )
#define IA32_DR7_L2         ( 1 << 4 )
#define IA32_DR7_L3         ( 1 << 6 )

#define IA32_DR7_G0         ( 1 << 1 )
#define IA32_DR7_G1         ( 1 << 3 )
#define IA32_DR7_G2         ( 1 << 5 )
#define IA32_DR7_G3         ( 1 << 7 )

#define IA32_DR7_LE         ( 1 << 8 )
#define IA32_DR7_GE         ( 1 << 9 )

#define IA32_DR7_RWE0
#endif

VOID
DbgKdpEmuDebugUpdate(
    _In_ ULONG64* DebugRegisters
);

VOID
DbgKdpClearBreakpoint(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body
);

#define DBGKD_KVAS_ENABLED      (0x00000001)

typedef struct _DBGKD_KVAS_STATE {
    ULONG32 KvaShadow;
    ULONG64 UserDirBase;

} DBGKD_KVAS_STATE, *PDBGKD_KVAS_STATE;

VOID
DbgKdpBuildContextRecord(
    _Out_ PCONTEXT Context
);

VOID
DbgKdStoreKvaShadow(
    _In_ PCONTEXT          Context,
    _In_ PDBGKD_KVAS_STATE KvaState
);

VOID
DbgKdRestoreKvaShadow(
    _In_ PCONTEXT          Context,
    _In_ PDBGKD_KVAS_STATE KvaState
);
