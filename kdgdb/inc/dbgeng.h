
#pragma once

//
// This is currently the gdb order ripped from
// amd64-tdep.h
//
// TODO: Improve architecture specific structures & control.
//

typedef struct _DBG_CORE_ENGINE *PDBG_CORE_ENGINE;

typedef enum _DBG_CORE_REGISTER {
    Amd64RegisterRax,
    Amd64RegisterRbx,
    Amd64RegisterRcx,
    Amd64RegisterRdx,
    Amd64RegisterRsi,
    Amd64RegisterRdi,
    Amd64RegisterRbp,
    Amd64RegisterRsp,
    Amd64RegisterR8,
    Amd64RegisterR9,
    Amd64RegisterR10,
    Amd64RegisterR11,
    Amd64RegisterR12,
    Amd64RegisterR13,
    Amd64RegisterR14,
    Amd64RegisterR15,

    Amd64RegisterRip,
    Amd64RegisterEFlags,

    Amd64RegisterSegCs,
    Amd64RegisterSegSs,
    Amd64RegisterSegDs,
    Amd64RegisterSegEs,
    Amd64RegisterSegFs,
    Amd64RegisterSegGs,

    Amd64RegisterSt0,
    Amd64RegisterSt1,
    Amd64RegisterSt2,
    Amd64RegisterSt3,
    Amd64RegisterSt4,
    Amd64RegisterSt5,
    Amd64RegisterSt6,
    Amd64RegisterSt7,
    Amd64RegisterFCtrl,
    Amd64RegisterFStat,
    Amd64RegisterFTag,

    Amd64RegisterXmm0 = 40,
    Amd64RegisterXmm1,
    Amd64RegisterXmm2,
    Amd64RegisterXmm3,
    Amd64RegisterXmm4,
    Amd64RegisterXmm5,
    Amd64RegisterXmm6,
    Amd64RegisterXmm7,
    Amd64RegisterXmm8,
    Amd64RegisterXmm9,
    Amd64RegisterXmm10,
    Amd64RegisterXmm11,
    Amd64RegisterXmm12,
    Amd64RegisterXmm13,
    Amd64RegisterXmm14,
    Amd64RegisterXmm15,

    Amd64RegisterMxcsr,

    Amd64RegisterYmm0h,
    Amd64RegisterYmm1h,
    Amd64RegisterYmm2h,
    Amd64RegisterYmm3h,
    Amd64RegisterYmm4h,
    Amd64RegisterYmm5h,
    Amd64RegisterYmm6h,
    Amd64RegisterYmm7h,
    Amd64RegisterYmm8h,
    Amd64RegisterYmm9h,
    Amd64RegisterYmm10h,
    Amd64RegisterYmm11h,
    Amd64RegisterYmm12h,
    Amd64RegisterYmm13h,
    Amd64RegisterYmm14h,
    Amd64RegisterYmm15h,

    Amd64RegisterBnd0r,
    Amd64RegisterBnd1r,
    Amd64RegisterBnd2r,
    Amd64RegisterBnd3r,
    Amd64RegisterBndcfgu,
    Amd64RegisterBndstatus,

    Amd64RegisterXmm16,
    Amd64RegisterXmm17,
    Amd64RegisterXmm18,
    Amd64RegisterXmm19,
    Amd64RegisterXmm20,
    Amd64RegisterXmm21,
    Amd64RegisterXmm22,
    Amd64RegisterXmm23,
    Amd64RegisterXmm24,
    Amd64RegisterXmm25,
    Amd64RegisterXmm26,
    Amd64RegisterXmm27,
    Amd64RegisterXmm28,
    Amd64RegisterXmm29,
    Amd64RegisterXmm30,
    Amd64RegisterXmm31,

    Amd64RegisterYmm16h,
    Amd64RegisterYmm17h,
    Amd64RegisterYmm18h,
    Amd64RegisterYmm19h,
    Amd64RegisterYmm20h,
    Amd64RegisterYmm21h,
    Amd64RegisterYmm22h,
    Amd64RegisterYmm23h,
    Amd64RegisterYmm24h,
    Amd64RegisterYmm25h,
    Amd64RegisterYmm26h,
    Amd64RegisterYmm27h,
    Amd64RegisterYmm28h,
    Amd64RegisterYmm29h,
    Amd64RegisterYmm30h,
    Amd64RegisterYmm31h,

    Amd64RegisterK0,
    Amd64RegisterK1,
    Amd64RegisterK2,
    Amd64RegisterK3,
    Amd64RegisterK4,
    Amd64RegisterK5,
    Amd64RegisterK6,
    Amd64RegisterK7,

    Amd64RegisterZmm0h,
    Amd64RegisterZmm1h,
    Amd64RegisterZmm2h,
    Amd64RegisterZmm3h,
    Amd64RegisterZmm4h,
    Amd64RegisterZmm5h,
    Amd64RegisterZmm6h,
    Amd64RegisterZmm7h,
    Amd64RegisterZmm8h,
    Amd64RegisterZmm9h,
    Amd64RegisterZmm10h,
    Amd64RegisterZmm11h,
    Amd64RegisterZmm12h,
    Amd64RegisterZmm13h,
    Amd64RegisterZmm14h,
    Amd64RegisterZmm15h,
    Amd64RegisterZmm16h,
    Amd64RegisterZmm17h,
    Amd64RegisterZmm18h,
    Amd64RegisterZmm19h,
    Amd64RegisterZmm20h,
    Amd64RegisterZmm21h,
    Amd64RegisterZmm22h,
    Amd64RegisterZmm23h,
    Amd64RegisterZmm24h,
    Amd64RegisterZmm25h,
    Amd64RegisterZmm26h,
    Amd64RegisterZmm27h,
    Amd64RegisterZmm28h,
    Amd64RegisterZmm29h,
    Amd64RegisterZmm30h,
    Amd64RegisterZmm31h,

    Amd64RegisterPkru,
    Amd64RegisterBaseFs,
    Amd64RegisterBaseGs,

    //
    // Non-gdb ones, somehow gdb supports super obscure 
    // shit extensions like Intel MPX, but can't read a 
    // fucking cr.
    //

    Amd64RegisterCr0,
    Amd64RegisterCr2,
    Amd64RegisterCr3,
    Amd64RegisterCr4,
    Amd64RegisterCr8,

    Amd64RegisterDr0,
    Amd64RegisterDr1,
    Amd64RegisterDr2,
    Amd64RegisterDr3,
    Amd64RegisterDr6,
    Amd64RegisterDr7,

    Amd64RegisterBaseGdt,
    Amd64RegisterBaseIdt,

    Amd64RegisterBaseTr,
    Amd64RegisterBaseLdtr,

    Amd64RegisterBaseXcr0,

} DBG_CORE_REGISTER, *PDBG_CORE_REGISTER;

EXTERN_C ULONG32 DbgAmd64RegisterSizeTable[ ];

typedef enum _DBG_CORE_BREAKPOINT {
    BreakOnExecute,
    BreakOnRead,
    BreakOnWrite,
    BreakOnAccess,

} DBG_CORE_BREAKPOINT, *PDBG_CORE_BREAKPOINT;

typedef DBG_STATUS( *DBG_CORE_MEMORY_READ )(
    _In_  PDBG_CORE_ENGINE Engine,
    _In_  ULONG_PTR        Address,
    _In_  ULONG32          Length,
    _Out_ PVOID            Buffer
    );

typedef DBG_STATUS( *DBG_CORE_MEMORY_WRITE )(
    _In_  PDBG_CORE_ENGINE Engine,
    _In_  ULONG_PTR        Address,
    _In_  ULONG32          Length,
    _Out_ PVOID            Buffer
    );

typedef DBG_STATUS( *DBG_CORE_REGISTER_READ )(
    _In_  PDBG_CORE_ENGINE  Engine,
    _In_  DBG_CORE_REGISTER RegisterId,
    _Out_ PVOID             Buffer
    );

typedef DBG_STATUS( *DBG_CORE_REGISTER_WRITE )(
    _In_ PDBG_CORE_ENGINE  Engine,
    _In_ DBG_CORE_REGISTER RegisterId,
    _In_ PVOID             Buffer
    );

typedef DBG_STATUS( *DBG_CORE_BREAKPOINT_INSERT )(
    _In_ PDBG_CORE_ENGINE    Engine,
    _In_ DBG_CORE_BREAKPOINT Breakpoint,
    _In_ ULONG_PTR           Address,
    _In_ ULONG32             Length
    );

typedef DBG_STATUS( *DBG_CORE_BREAKPOINT_CLEAR )(
    _In_ PDBG_CORE_ENGINE    Engine,
    _In_ DBG_CORE_BREAKPOINT Breakpoint,
    _In_ ULONG_PTR           Address
    );

typedef DBG_STATUS( *DBG_CORE_PROCESSOR_SWITCH )(
    _In_ PDBG_CORE_ENGINE Engine,
    _In_ ULONG32          Processor
    );

typedef DBG_STATUS( *DBG_CORE_PROCESSOR_BREAK )(
    _In_  PDBG_CORE_ENGINE Engine,
    _Out_ PULONG32         Processor
    );

typedef DBG_STATUS( *DBG_CORE_PROCESSOR_CONTINUE )(
    _In_ PDBG_CORE_ENGINE Engine
    );

typedef DBG_STATUS( *DBG_CORE_PROCESSOR_STEP )(
    _In_ PDBG_CORE_ENGINE Engine
    );

typedef DBG_STATUS( *DBG_CORE_PROCESSOR_QUERY )(
    _In_  PDBG_CORE_ENGINE Engine,
    _Out_ PULONG32         Processor
    );

typedef DBG_STATUS( *DBG_CORE_MSR_READ )(
    _In_  PDBG_CORE_ENGINE Engine,
    _In_  ULONG32          Register,
    _Out_ PULONG64         Buffer
    );

typedef DBG_STATUS( *DBG_CORE_MSR_WRITE )(
    _In_ PDBG_CORE_ENGINE Engine,
    _In_ ULONG32          Register,
    _In_ ULONG64          Buffer
    );

typedef DBG_STATUS( *DBG_CORE_IO_READ )(
    _In_  PDBG_CORE_ENGINE Engine,
    _In_  USHORT           Port,
    _In_  ULONG32          Length,
    _Out_ PVOID            Buffer
    );

typedef DBG_STATUS( *DBG_CORE_IO_WRITE )(
    _In_ PDBG_CORE_ENGINE Engine,
    _In_ USHORT           Port,
    _In_ ULONG32          Length,
    _In_ PVOID            Buffer
    );

typedef DBG_STATUS( *DBG_CORE_CHECK_QUEUE )(
    _In_ PDBG_CORE_ENGINE Engine
    );

typedef union _DBG_CORE_EXTENSION *PDBG_CORE_EXTENSION;

typedef DBG_STATUS( *DBG_CORE_SEND )(
    _In_ PDBG_CORE_EXTENSION Extension,
    _In_ PVOID               Buffer,
    _In_ ULONG               Length
    );

typedef DBG_STATUS( *DBG_CORE_RECV )(
    _In_  PDBG_CORE_EXTENSION Extension,
    _Out_ PVOID               Buffer,
    _In_  ULONG               Length
    );

typedef DBG_STATUS( *DBG_CORE_FLUSH )(
    _In_ PDBG_CORE_EXTENSION Extension
    );

typedef struct _DBG_CORE_SOCKET {

    SOCKET SocketHandle;
} DBG_CORE_SOCKET, *PDBG_CORE_SOCKET;

typedef struct _DBG_CORE_PIPE {

    HANDLE FileHandle;
} DBG_CORE_PIPE, *PDBG_CORE_PIPE;

typedef union _DBG_CORE_EXTENSION {
    DBG_CORE_SOCKET Socket;
    DBG_CORE_PIPE   Pipe;
} DBG_CORE_EXTENSION, *PDBG_CORE_EXTENSION;

typedef struct _DBG_CORE_DEVICE {
    DBG_CORE_SEND      DbgSend;
    DBG_CORE_RECV      DbgRecv;
    DBG_CORE_FLUSH     DbgFlush;

    DBG_CORE_EXTENSION Extension;

} DBG_CORE_DEVICE, *PDBG_CORE_DEVICE;

typedef struct _DBG_CORE_ENGINE {
    DBG_CORE_DEVICE             DbgCommDevice;

    DBG_CORE_CHECK_QUEUE        DbgCheckQueue;

    DBG_CORE_MEMORY_READ        DbgMemoryRead;
    DBG_CORE_MEMORY_WRITE       DbgMemoryWrite;
    DBG_CORE_REGISTER_READ      DbgRegisterRead;
    DBG_CORE_REGISTER_WRITE     DbgRegisterWrite;
    DBG_CORE_BREAKPOINT_INSERT  DbgBreakpointInsert;
    DBG_CORE_BREAKPOINT_CLEAR   DbgBreakpointClear;
    DBG_CORE_PROCESSOR_SWITCH   DbgProcessorSwitch;
    DBG_CORE_PROCESSOR_BREAK    DbgProcessorBreak;
    DBG_CORE_PROCESSOR_CONTINUE DbgProcessorContinue;
    DBG_CORE_PROCESSOR_STEP     DbgProcessorStep;
    DBG_CORE_PROCESSOR_QUERY    DbgProcessorQuery;

    //
    // Architecture specific definitions.
    //

    DBG_CORE_MSR_READ           DbgMsrRead;
    DBG_CORE_MSR_WRITE          DbgMsrWrite;
    DBG_CORE_IO_READ            DbgIoRead;
    DBG_CORE_IO_WRITE           DbgIoWrite;

} DBG_CORE_ENGINE, *PDBG_CORE_ENGINE;
