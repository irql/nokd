
#pragma once

#define _CRT_SECURE_NO_WARNINGS

#define WIN32_NO_STATUS 
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <stdio.h>

//
// Gdb
//

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment (lib, "Ws2_32.lib")

//
// Pdb
//

#include <dia2.h>

//
// Kd
//

#include <winternl.h>
#undef WIN32_NO_STATUS
#include <ntstatus.h>

#pragma pack( push, 1 )
typedef struct _IMAGE_DEBUG_CODE_VIEW {
    // 'SDSR'
    ULONG32 Signature;
    GUID    UniqueId;
    ULONG32 Age;
    CHAR    Path[ 0 ];
} IMAGE_DEBUG_CODE_VIEW, *PIMAGE_DEBUG_CODE_VIEW;
#pragma pack( pop )

#define STATIC              static
#define VOLATILE            volatile

#define DBG_STATUS_OKAY     (0x00000000L)
#define DBG_STATUS_ERROR    (0xC0000001L)
#define DBG_STATUS_RESEND   (0xC0000002L)
#define DBG_SUCCESS( x )    ( ( ( DBG_STATUS )( x ) ) >= 0 )

typedef LONG32 DBG_STATUS;

#if 0 // winternl.h
typedef struct _STRING {
    USHORT Length;
    USHORT MaximumLength;
    PCHAR  Buffer;
} STRING, *PSTRING;
#endif

//
// DBGKD.H FROM KDPROXY.SYS
//

#include "../../kdproxy/inc/dbgkd.h"

#include "dbgeng.h"

#include "gdb.h"
#include "kd.h"

#define DbgKdpFactory                           "DbgKdp"
#define DbgGdbFactory                           "DbgGdb"

#define DbgKdpTraceLogLevel0( Factory, ... )    printf( "[" Factory "] " __VA_ARGS__ )
#define DbgKdpTraceLogLevel1( Factory, ... )    printf( "[" Factory "] " __VA_ARGS__ )
#define DbgKdpTraceLogLevel2( Factory, ... )    DbgKdPrint( "[" Factory "] " __VA_ARGS__ )

DBG_STATUS
DbgGdbInit(
    _In_ PDBG_CORE_ENGINE Engine
);

DBG_STATUS
DbgKdInit(

);

typedef struct _DBGPDB_CONTEXT {
    WCHAR           FileName[ 256 ];

    IDiaDataSource* Source;
    IDiaSession*    Session;
    IDiaSymbol*     Global;

} DBGPDB_CONTEXT, *PDBGPDB_CONTEXT;

HRESULT
DbgPdbLoad(
    _In_  PWSTR           FileName,
    _Out_ PDBGPDB_CONTEXT Context
);

HRESULT
DbgPdbAddressOfName(
    _In_  PDBGPDB_CONTEXT Context,
    _In_  PCWSTR          Name,
    _Out_ PULONG          Rva
);

VOID
DbgPdbBuildSymbolUrl(
    _Out_ PWSTR    Url,
    _In_  PCSTR    FileName,
    _In_  LPGUID   UniqueId,
    _In_  ULONG32  Age
);

VOID
DbgPdbBuildSymbolFile(
    _Out_ PWSTR    File,
    _In_  PCSTR    FileName,
    _In_  LPGUID   UniqueId,
    _In_  ULONG32  Age
);

HRESULT
DbgPdbDownload(
    _In_  PWSTR    Url,
    _Out_ PVOID    Buffer,
    _Out_ PULONG32 Length
);

HRESULT
DbgPdbNameOfAddress(
    _In_  PDBGPDB_CONTEXT Context,
    _In_  ULONG32         Address,
    _Out_ PWSTR*          Name,
    _Out_ PLONG32         Disp
);

HRESULT
DbgPdbFieldOffset(
    _In_  PDBGPDB_CONTEXT Context,
    _In_  PCWSTR          TypeName,
    _In_  PCWSTR          FieldName,
    _Out_ PLONG32         FieldOffset
);

EXTERN_C DBGPDB_CONTEXT      DbgPdbKernelContext;

DBG_STATUS
DbgPipeInit(
    _In_ PDBG_CORE_DEVICE Device,
    _In_ PCSTR            PipeName,
    _In_ BOOLEAN          OwnPipe
);

DBG_STATUS
DbgSocketInit(
    _In_ PDBG_CORE_DEVICE Device,
    _In_ PCSTR            Address,
    _In_ USHORT           Port
);
