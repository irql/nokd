
#pragma once

//
// KD_DEBUG_NO_FREEZE creates a system thread, to poll for break-in,
// this was only intended for use when debugging.
//
#define KD_DEBUG_NO_FREEZE              0

//
// Haven't really tested the UART functionality, it was
// written at a very early stage in this project's lifespan.
// It's significantly slower than vmwrpc, which is enabled
// by default, also - sorry vbox users lol.
//
// Although porting this to vbox should be very trivial, or even
// any other interface due to the abstractions provided by
// KD_DEBUG_DEVICE.
//
#define KD_UART_ENABLED                 0

//
// If this is enabled, then KdpSendWaitContinue will
// log all packets received using a DbgPrint - this doesn't
// display on the windbg instance for obvious reasons.
//
#define KD_RECV_PACKET_LOGGING          0

//
// Breakpoint entry count of KdpBreakpointTable, 
// this is 32 on windows, and not really a big deal.
//
#define KD_BREAKPOINT_TABLE_LENGTH      32

//
// Tracepoints are implemented such that they're unsafe
// on multiprocessor systems, this is because the 
// following opcode is overwritten by a cd 02 bp,
// this is then caught by the NMI handler,
// every time this happens, all processors are 
// resumed and another processor could hit the same
// trace point, theoretically. Although a super slim
// chance. This switch means only the tracing processor
// will be resumed during this short time frame.
// Would recommend leaving enabled.
//
#define KD_SAFE_TRACE_POINTS            1

//
// This is a trick that will cause patchguard
// to explode, when enabled, you can use user mode
// breakpoints (until pg inevitably bugchecks) because
// cd 02 will execute fine at cpl=3, instead of generating
// a #GP.
//
#define KD_SET_NMI_DPL                  1

//
// When this is enabled, the driver will patch
// a small snippet of code inside KiPageFault
// to allow breakpoints & tracepoints to be set
// on paged-out memory. This is unlikely to trigger
// patchguard, but could, assuming patchguard scans during
// a small unfortunate timeframe, or a breakpoint takes a long
// time to be paged-in. The code is patched and reverted
// for when it is needed to be executed.
//
#define KD_PAGED_MEMORY_FIX             1

//
// Does not bypass patchguard, I realised this is out of the scope of this 
// project, and just abandonded it, half-written. I don't think ByePg
// can be fixed up to work on 21354 anyways and this certainly wont work
// in it's current state.
//
#define KD_BYPASS_PG                    0

//
// This performs a KdPrint of different events surrounding
// tracepoint operations. Just leave disabled. On another note:
// hde64 might want to be replaced, it chokes on tons of instructions.
//
#define KD_TRACE_POINT_LOGGING          1

//
// When enabled, it will call KdLoadSystem which enumerates
// all modules on the system and reports loaded symbols,
// having this alongside "DbgBreakPoint() on start"/"Stop
// debugger automatically" may cause issues because the debugger
// is always prompted to break-in when reporting a load-symbol-state-change.
//
// The alternative is to just type .reload, and windbg will search and load all modules.
//
// TODO: Consider adding callbacks like: 
//       PsSetLoadImageNotifyRoutine, 
//       KeRegisterBugCheckCallback,
//       DbgSetDebugPrintCallback
//
//! This seems slightly buggy, would recommend using .reload instead.
//! Would like to fix the issue with this in the future, but for now
//! it is irrelevant.
//
#define KD_LOAD_SYSTEM_SYMBOLS          0

//
// Patterns for everything this driver requires.
//

#if 0
#define KD_SECTION_BASE( x )   ( PVOID )( ( ULONG_PTR )ImageBase + ( ULONG_PTR )KD_SECTION_SIZE_( x ))
#define KD_SECTION_SIZE( x )   KD_SECTION_SIZE_( x )
#define KD_SECTION_BASE_( x )  Section##x##Base  
#define KD_SECTION_SIZE_( x )  Section##x##Size

#define KD_SIG_KdTrap_SECTION  Text
#define KD_SIG_KdTrap_PATTERN  "48 83 EC 38 83 3D ? ? ? ? ? 8A 44 24 68"
#endif

#define KD_STARTUP_SIG \
"              __          __     \n" \
"             /\\ \\        /\\ \\    \n" \
"  ___     ___\\ \\ \\/'\\    \\_\\ \\   \n" \
"/' _ `\\  / __`\\ \\ , <    /'_` \\  \n" \
"/\\ \\/\\ \\/\\ \\L\\ \\ \\ \\\\`\\ /\\ \\L\\ \\ \n" \
"\\ \\_\\ \\_\\ \\____/\\ \\_\\ \\_\\ \\___,_\\\n" \
" \\/_/\\/_/\\/___/  \\/_/\\/_/\\/__,_ /\n" \
"\n"

#define KD_SYMBOLIC_NAME    "kdproxy"
#define KD_FILE_NAME        "kdproxy.sys"

#include <ntifs.h>

//
// Undefine any conflicts with this drivers implementations.
//

#undef KdPrint

//nonstandard extension, function / data pointer conversion in expression
#pragma warning( disable : 4152 ) 
//nonstandard extension used : nameless struct / union
#pragma warning( disable : 4201 ) 
//nonstandard extension used : bit field types other than int
#pragma warning( disable : 4214 )

//
// Great function I stumbled across when searching for a way
// to acquire KiProcessorBlock, not defined in headers but it is 
// exported, simply performs a `return KiProcessorBlock[ Processor ];`
//
// What a waste, KiProcessorBlock is on the KDDEBUGGER_DATA64 structure
//
ULONG_PTR
KeQueryPrcbAddress(
    _In_ ULONG32 Processor
);

typedef struct _KAFFINITY_EX {
    //
    // This structure is taken from my 21354 build of windows, geoffchappel
    // documents the structure pretty well, although the size is incorrect
    // it's now 32 AFFINITY masks. KiMaximumGroups defines this value.
    //
    // References: HalpNmiReboot, KeFreezeExecution -> KiSendFreeze
    //
    // When used alongside HalSendNMI, the following sequence of operations
    // is performed.
    //
    // AffinityEx.Count = 1;
    // AffinityEx.Size = 0x20;
    // RtlZeroMemory( &AffinityEx->Reserved, sizeof( KAFFINITY_EX ) - 4 );
    // KiCopyAffinityEx( &AffinityEx, 0x20, &KeActiveProcessors );
    // KeRemoveProcessorAffinityEx( &AffinityEx, KeGetCurrentProcessorNumber( ) );
    // HalSendNMI( &AffinityEx );
    //
    // NOTE: KeQueryGroupAffinity is a direct ref to any 
    // of the processors in any group under KeActiveProcessors.Bitmap
    //

    USHORT    Count;
    USHORT    Size;
    ULONG     Reserved;
    KAFFINITY Bitmap[ 32 ];

} KAFFINITY_EX, *PKAFFINITY_EX;

C_ASSERT( sizeof( KAFFINITY_EX ) == 0x108 );

VOID
HalSendNMI(
    _In_ PKAFFINITY_EX Affinity
);

#define STATIC      static
#define VOLATILE    volatile

//
// DBGKD structures which are mostly
// message headers, and other general 
// enumerations and structures.
//

#include "dbgkd.h"

//
// STRUCTURES, ENUMERATIONS AND TYPEDEFS
//

typedef struct _KD_VMWRPC_CONTROL {
    ULONG32 Cookie1;
    ULONG32 Cookie2;
    ULONG32 Channel;
    ULONG32 RecvId;

    ULONG32 RecvExtraLength;
    PVOID   RecvExtraBuffer;
} KD_VMWRPC_CONTROL, *PKD_VMWRPC_CONTROL;

typedef struct _KD_UART_CONTROL {
    ULONG64 Index;
    USHORT  Base;
} KD_UART_CONTROL, *PKD_UART_CONTROL;

typedef struct _KD_DEBUG_DEVICE *PKD_DEBUG_DEVICE;

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
        KD_UART_CONTROL   Uart;
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

#define KD_BPE_SET      (0x00000001)
#define KD_BPE_OWE      (0x00000002)

typedef struct _KD_BREAKPOINT_ENTRY {
    PEPROCESS Process;
    ULONG64   Address;
    ULONG32   Flags;
    ULONG32   ContentLength;
    UCHAR     Content[ 0x20 ];
} KD_BREAKPOINT_ENTRY, *PKD_BREAKPOINT_ENTRY;

//
// Defines constants that describe the nig mode.
//

typedef enum _D3DNIGMODE {
    D3DNIG_NONE     = 0,
    D3DNIG_MODE     = 1,
    D3DNIG_LOAD     = 2,
    D3DNIG_MAXIMUM  = 3,
} D3DNIGMODE, *PD3DNIGMODE;

//
// Type for Process->DirectoryTableBase.
//

typedef ULONG_PTR KADDRESS_SPACE, *PKADDRESS_SPACE;

//
// ServiceNumbers for KdServiceInterrupt
//

typedef enum _KD_SERVICE_NUMBER {
    KdServiceStateChangeSymbol,
    KdServiceStateChangeExcept,
    KdServiceStateChange
} KD_SERVICE_NUMBER, *PKD_SERVICE_NUMBER;

#define KDPR_FLAG_TP_ENABLE     (0x00000001)
#define KDPR_FLAG_TP_OWE        (0x00000002)
#define KDPR_FLAG_TP_OWE_NO_INC (0x00000004)

typedef struct _KD_PROCESSOR {
    ULONG32 Flags;
    ULONG64 Tracepoint;
    UCHAR   TracepointCode[ 0x20 ];
} KD_PROCESSOR, *PKD_PROCESSOR;

//
// definitions for kdapi, which are 
// used per KD_API_NUMBER, declared
// inside kdimpl.
//

#define KDAPI 
#include "kdapi.h"

//
// FUNCTION DEFINITIONS
//

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

VOID
KdLoadSystem(

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

VOID
KdpSetContextState(
    _Inout_ PDBGKD_WAIT_STATE_CHANGE Change,
    _In_    PCONTEXT                 Context
);

BOOLEAN
KdpReportLoadSymbolsStateChange(
    _In_    PSTRING         PathName,
    _In_    PKD_SYMBOL_INFO Symbol,
    _In_    BOOLEAN         Unload,
    _Inout_ PCONTEXT        Context
);

BOOLEAN
KdReportLoadSymbolsStateChange(
    _In_    PSTRING         PathName,
    _In_    PKD_SYMBOL_INFO Symbol,
    _In_    BOOLEAN         Unload
);

NTSTATUS
KdReportLoaded(
    _In_ PCHAR ImageName,
    _In_ PCHAR ImagePath
);

VOID
KdPrint(
    _In_ PCHAR Format,
    _In_ ...
);

ULONG_PTR
KeGetCurrentPrcb(

);

BOOLEAN
KdServiceInterrupt(
    _In_ PVOID   Context,
    _In_ BOOLEAN Handled
);

NTSTATUS
KdFreezeLoad(

);

VOID
KdFreezeUnload(

);

VOID
KdDebugBreak(

);

VOID
KdNmiBp(

);

ULONG
KdNmiServiceBp(
    _In_ ULONG ServiceNumber,
    _In_ PVOID Arg1,
    _In_ PVOID Arg2,
    _In_ PVOID Arg3,
    _In_ PVOID Arg4
);

VOID
KdFreezeProcessors(

);

VOID
KdThawProcessors(

);

VOID
KdSwitchFrozenProcessor(
    _In_ ULONG32 NewProcessor
);

BOOLEAN
KdpReportExceptionStateChange(
    _In_ PEXCEPTION_RECORD64 ExceptRecord,
    _In_ PCONTEXT            ExceptContext,
    _In_ BOOLEAN             SecondChance
);

KADDRESS_SPACE
KdGetProcessSpace(
    _In_ PEPROCESS Process
);

KADDRESS_SPACE
KdSetSpace(
    _In_ KADDRESS_SPACE AddressSpace
);

NTSTATUS
KdCopyProcessSpace(
    _In_ PEPROCESS Process,
    _In_ PVOID     Destination,
    _In_ PVOID     Source,
    _In_ ULONG     Length
);

ULONG32
KdGetPrcbNumber(
    _In_ ULONG_PTR Prcb
);

ULONG32
KdGetCurrentPrcbNumber(

);

PCONTEXT
KdGetPrcbContext(
    _In_ ULONG_PTR Prcb
);

PCONTEXT
KdGetCurrentPrcbContext(

);

PKSPECIAL_REGISTERS
KdGetPrcbSpecialRegisters(
    _In_ ULONG_PTR Prcb
);

VOID
KdPageFault(
    _In_ ULONG64 FaultAddress
);

VOID
KdSetTracepoint(
    _In_ CONTEXT* Context
);

VOID
KdOweTracepoint(
    _In_ ULONG32 Processor
);

VOID
KeBugCheckExHook(
    _In_ ULONG     BugCheckCode,
    _In_ ULONG_PTR BugCheckParameter1,
    _In_ ULONG_PTR BugCheckParameter2,
    _In_ ULONG_PTR BugCheckParameter3
);

BOOLEAN
KdBugCheckEx(
    _In_ ULONG     BugCheckCode,
    _In_ ULONG_PTR BugCheckParameter1,
    _In_ ULONG_PTR BugCheckParameter2,
    _In_ ULONG_PTR BugCheckParameter3
);

#if KD_PAGED_MEMORY_FIX
VOID
KdOwePrepare(

);
#endif

//
// IMPORTS
//



//
// x86 JUNK
//

//
// Any flag which could cause a branch.
//

#define EFL_CF 0x00000001
#define EFL_PF 0x00000004
#define EFL_ZF 0x00000040
#define EFL_SF 0x00000080
#define EFL_OF 0x00000800

#define KeSweepLocalCaches __wbinvd

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
    _In_  ULONG   Processor,
    _In_  ULONG64 Address,
    _In_  PVOID   Buffer,
    _In_  ULONG   Length,
    _Out_ PULONG  TransferLength
    );

EXTERN_C NTSTATUS( *KdpSysWriteControlSpace )(
    _In_  ULONG   Processor,
    _In_  ULONG64 Address,
    _In_  PVOID   Buffer,
    _In_  ULONG   Length,
    _Out_ PULONG  TransferLength
    );

EXTERN_C VOID( *KdpGetStateChange )(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PCONTEXT                  Context
    );

EXTERN_C BOOLEAN( *MmIsSessionAddress )(
    _In_ ULONG_PTR Address
    );

EXTERN_C PBOOLEAN            KdPitchDebugger;
EXTERN_C PULONG              KdpDebugRoutineSelect;

EXTERN_C BOOLEAN             KdDebuggerNotPresent_;
EXTERN_C KTIMER              KdBreakTimer;
EXTERN_C KDPC                KdBreakDpc;

EXTERN_C ULONG64( *KdDecodeDataBlock )(

    );

EXTERN_C KDDEBUGGER_DATA64   KdDebuggerDataBlock;
EXTERN_C DBGKD_GET_VERSION64 KdVersionBlock;
EXTERN_C LIST_ENTRY          KdpDebuggerDataListHead;
EXTERN_C ULONG64             KdpLoaderDebuggerBlock;
EXTERN_C KD_CONTEXT          KdpContext;

EXTERN_C KD_DEBUG_DEVICE     KdDebugDevice;

EXTERN_C PUSHORT             KeProcessorLevel;

EXTERN_C UCHAR               KdpMessageBuffer[ 0x1000 ];
EXTERN_C KD_BREAKPOINT_ENTRY KdpBreakpointTable[ KD_BREAKPOINT_TABLE_LENGTH ];
EXTERN_C ULONG               KdpBreakpointCodeLength;
EXTERN_C UCHAR               KdpBreakpointCode[ ];

EXTERN_C PKD_PROCESSOR       KdProcessorBlock;

#if KD_PAGED_MEMORY_FIX
EXTERN_C ULONG64             KdpOweCodeAddress;
EXTERN_C VOLATILE ULONG64    KdpOweBreakpoint;
EXTERN_C VOLATILE ULONG64    KdpOweTracepoint;
EXTERN_C UCHAR               KdpOweCodeOrig[ 23 ];
EXTERN_C UCHAR               KdpOweCodeHook[ 23 ];
#endif

#if KD_BYPASS_PG
EXTERN_C ULONG64             KdpBugCheckExAddress;
EXTERN_C UCHAR               KdpBugCheckExOrig[ 20 ];
EXTERN_C UCHAR               KdpBugCheckExCode[ 20 ];
#endif
