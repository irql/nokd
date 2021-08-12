
#pragma once

#define GDB_PACKET_START    '$'
#define GDB_PACKET_END      '#'
#define GDB_PACKET_ACK      '+'
#define GDB_PACKET_NACK     '-'

#define GDB_BP_SOFTWARE     0
#define GDB_BP_HARDWARE     1
#define GDB_BP_WATCH_WRITE  2
#define GDB_BP_WATCH_READ   3
#define GDB_BP_WATCH_ACCESS 4

typedef struct _DBGGDB_AMD64_EXECUTE {

    //
    // Thanks to gdb's poor implementation :^)
    //
    // TODO: We can optimize this function by have a value to indicate which
    //       registers are actually cared about.
    //

    ULONG64 Rax;
    ULONG64 Rbx;
    ULONG64 Rcx;
    ULONG64 Rdx;
    ULONG64 Rsi;
    ULONG64 Rdi;
    ULONG64 Rbp;
    ULONG64 Rsp;
    ULONG64 R8;
    ULONG64 R9;
    ULONG64 R10;
    ULONG64 R11;
    ULONG64 R12;
    ULONG64 R13;
    ULONG64 R14;
    ULONG64 R15;
    ULONG64 Rip;
    ULONG32 EFlags;
    ULONG32 SegCs;
    ULONG32 SegSs;
    ULONG32 SegDs;
    ULONG32 SegEs;
    ULONG32 SegFs;
    ULONG32 SegGs;

} DBGGDB_AMD64_EXECUTE, *PDBGGDB_AMD64_EXECUTE;

DBG_STATUS
DbgGdbAmd64Execute(
    _In_ PDBG_CORE_ENGINE      Engine,
    _In_ PDBGGDB_AMD64_EXECUTE Context,
    _In_ PVOID                 Code,
    _In_ ULONG32               CodeCount,
    _In_ ULONG32               CodeLength
);
