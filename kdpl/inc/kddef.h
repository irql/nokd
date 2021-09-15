
#pragma once

#ifndef DBGKD_H
typedef struct _KDESCRIPTOR {
    USHORT Pad[ 3 ];
    USHORT Limit;
    PVOID  Base;
} KDESCRIPTOR, *PKDESCRIPTOR;

typedef struct _KSPECIAL_REGISTERS {
    ULONG64     Cr0;
    ULONG64     Cr2;
    ULONG64     Cr3;
    ULONG64     Cr4;
    ULONG64     KernelDr0;
    ULONG64     KernelDr1;
    ULONG64     KernelDr2;
    ULONG64     KernelDr3;
    ULONG64     KernelDr6;
    ULONG64     KernelDr7;
    KDESCRIPTOR Gdtr;
    KDESCRIPTOR Idtr;
    USHORT      Tr;
    USHORT      Ldtr;
    ULONG32     MxCsr;
    ULONG64     DebugControl;
    ULONG64     LastBranchToRip;
    ULONG64     LastBranchFromRip;
    ULONG64     LastExceptionToRip;
    ULONG64     LastExceptionFromRip;
    ULONG64     Cr8;
    ULONG64     MsrGsBase;
    ULONG64     MsrGsSwap;
    ULONG64     MsrStar;
    ULONG64     MsrLStar;
    ULONG64     MsrCStar;
    ULONG64     MsrSyscallMask;
    ULONG64     Xcr0;
    ULONG64     MsrFsBase;
    ULONG64     SpecialPadding0;
} KSPECIAL_REGISTERS, *PKSPECIAL_REGISTERS;
#endif

#ifndef MM_COPY_MEMORY_VIRTUAL
#define MM_COPY_MEMORY_VIRTUAL   0x2
#endif

#ifndef MM_COPY_MEMORY_PHYSICAL
#define MM_COPY_MEMORY_PHYSICAL  0x1
#endif

//#pragma comment(lib, "kdpl.lib")

//
// KDPLAPI = function definitions required by the linker
// KDAPI   = function definitions exported by the library 
//

#ifdef  KDPL
#define KDPLAPI EXTERN_C
#define KDAPI   
#else
#define KDPLAPI 
#define KDAPI   EXTERN_C
#endif

KDPLAPI ULONG64 MmKernelBase;
KDPLAPI ULONG32 MmKernelSize;

KDPLAPI
PKPCR
DbgKdQueryPcr(

);

KDPLAPI
ULONG32
DbgKdQueryProcessorCount(

);

//
// Expects the system to call KdReportStateChange on the processor
// specified by the processor number.
//
KDPLAPI
VOID
DbgKdSwapProcessor(
    _In_ ULONG32 ProcessorNumber
);

KDPLAPI
NTSTATUS
DbgKdMmCopyMemory(
    _In_ PVOID    TargetAddress,
    _In_ PVOID    SourceAddress,
    _In_ ULONG64  NumberOfBytes,
    _In_ ULONG32  Flags,
    _In_ ULONG64* NumberOfBytesTransferred
);

KDPLAPI
PCONTEXT
DbgKdQueryProcessorContext(
    _In_ ULONG32 ProcessorNumber
);

KDPLAPI
PKSPECIAL_REGISTERS
DbgKdQuerySpecialRegisters(
    _In_ ULONG32 ProcessorNumber
);

KDPLAPI
NTSTATUS
DbgKdInsertBreakpoint(
    _In_ ULONG64 Address
);

KDPLAPI
NTSTATUS
DbgKdRemoveBreakpoint(
    _In_ ULONG64 Address
);

KDAPI
NTSTATUS
KdDriverLoad(

);

KDAPI
VOID
KdDriverUnload(

);

KDAPI
BOOLEAN
KdPollBreakIn(

);

#define STATUS_RSC_LOAD_SYMBOLS ( ( NTSTATUS )( ( FACILITY_DEBUGGER << 16 ) | ( STATUS_SEVERITY_ERROR << 30 ) | ( 1 << 29 ) ) )

KDAPI
VOID
KdReportStateChange(
    _In_    NTSTATUS Status,
    _Inout_ PCONTEXT ContextRecord
);

KDAPI
VOID
KdPrint(
    _In_ PCHAR Format,
    _In_ ...
);
