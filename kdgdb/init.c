
#include <kdgdb.h>

BOOLEAN             DbgKdDebuggerEnabled = FALSE;

UCHAR               DbgKdMessageBuffer[ 0x1000 ];

ULONG32             DbgKdProcessorCount = 2;

//
// Not sure what this value really is, 
// it's 6 on my kdproxy system.
//
ULONG32             DbgKdProcessorLevel = 6;

ULONG64             DbgKdKernelBase = 0;

//
// Variables from ntos for decoding the datablock
// as shown inside KdCopyDataBlock.
//
// KdpDataBlockEncode is an ntos pointer!!!
//
ULONG64             KiWaitAlways;
ULONG64             KiWaitNever;
ULONG64             KdpDataBlockEncoded;

KDDEBUGGER_DATA64   DbgKdDebuggerBlock;
DBGKD_GET_VERSION64 DbgKdVersionBlock;

ULONG64             DbgKdNtosHeaders;

ULONG64             DbgKdProcessorBlock[ 16 ];

DBGPDB_CONTEXT      DbgPdbKernelContext;

DBG_CORE_ENGINE     DbgCoreEngine;

ULONG32             DbgAmd64RegisterSizeTable[ ] = {
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,

    8,
    4,

    //
    // Segment registers are currently set to dword size
    // because this makes it easier to program alongside 
    // gdb, not architecturally correct, they should be 2.
    //
#if 0
    4,
    4,
    4,
    4,
    4,
    4,
#endif
    2,
    2,
    2,
    2,
    2,
    2,

    10,
    10,
    10,
    10,
    10,
    10,
    10,
    10,

    // not even sure about FCtrl, FStat and FTag
    // TODO: Missing fiseg, fioff, foseg, fooff, fop
    4,
    4,
    4,

    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,

    // Mxcsr
    4,

    32,
    32,
    32,
    32,
    32,
    32,
    32,
    32,
    32,
    32,
    32,
    32,
    32,
    32,
    32,
    32,

    // 
    // I have no idea about MPX nor do I really care,
    // the fact that gdb supports this but not cr's is 
    // stupid.
    //
    4,
    4,
    4,
    4,
    4,
    4,

    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,
    16,

    32,
    32,
    32,
    32,
    32,
    32,
    32,
    32,
    32,
    32,
    32,
    32,
    32,
    32,
    32,
    32,

    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,

    64,
    64,
    64,
    64,
    64,
    64,
    64,
    64,
    64,
    64,
    64,
    64,
    64,
    64,
    64,
    64,
    64,
    64,
    64,
    64,
    64,
    64,
    64,
    64,
    64,
    64,
    64,
    64,
    64,
    64,
    64,
    64,

    4,
    8,
    8,

    8,
    8,
    8,
    8,
    8,

    8,
    8,
    8,
    8,
    8,
    8,

    8,
    8,

    2,
    2,

    8,
};

int main( ) {

    DBG_STATUS               Status;
    STRING                   Head;
    STRING                   Body;
    STARTUPINFOA             Start = { 0 };
    PROCESS_INFORMATION      Process = { 0 };
    DBGKD_WAIT_STATE_CHANGE  State = { 0 };
    DBGKD_MANIPULATE_STATE64 Manip = { 0 };
    ULONG32                  CurrentProcessor;

    CONTEXT                  CodeContext = { 0 };
    UCHAR                    Code[ 2 ] = { 0x0F, 0x32 };
    USHORT                   DosHeader;

    LONG32                   NewDos;
    IMAGE_DATA_DIRECTORY     Directory;
    PIMAGE_DEBUG_DIRECTORY   DebugDirectory;
    PIMAGE_DEBUG_CODE_VIEW   CodeView;

    HANDLE                   FileHandle;
    ULONG32                  Length;
    PVOID                    Pdb;
    WCHAR                    Url[ 256 ];
    WCHAR                    File[ 256 ];
    DWORD                    Bytes;

    ULONG32                  KdVersionBlockRva;
    ULONG64                  KdVersionBlockAddress;
    ULONG32                  KdDebuggerDataBlockRva;
    ULONG64                  KdDebuggerDataBlockAddress;
    ULONG32                  KiWaitAlwaysRva;
    ULONG64                  KiWaitAlwaysAddress;
    ULONG32                  KiWaitNeverRva;
    ULONG64                  KiWaitNeverAddress;
    ULONG32                  KdpDataBlockEncodedRva;
    ULONG64                  KdpDataBlockEncodedAddress;
    BOOLEAN                  KdpDataBlockEncodedValue;
    // TODO: KeNumberProcessors
    ULONG32                  KiProcessorBlockRva;
    ULONG64                  KiProcessorBlockAddress;
    DBGKD_KVAS_STATE         KvaState;

    Status = DbgGdbInit( &DbgCoreEngine );

    if ( !DBG_SUCCESS( Status ) ) {

        DbgKdpTraceLogLevel1( DbgKdpFactory, "Failed to connect to gdb stub\n" );
        ExitProcess( EXIT_FAILURE );
    }

    Status = DbgCoreEngine.DbgProcessorContinue( &DbgCoreEngine );

    if ( !DBG_SUCCESS( Status ) ) {

        DbgKdpTraceLogLevel1( DbgKdpFactory, "Failed to connect to gdb stub\n" );
        ExitProcess( EXIT_FAILURE );
    }

    Status = DbgCoreEngine.DbgProcessorBreak( &DbgCoreEngine,
                                              &CurrentProcessor );

    if ( !DBG_SUCCESS( Status ) ) {

        DbgKdpTraceLogLevel1( DbgKdpFactory, "Failed to connect to gdb stub\n" );
        ExitProcess( EXIT_FAILURE );
    }

    Status = DbgKdInit( );

    if ( !DBG_SUCCESS( Status ) ) {

        DbgKdpTraceLogLevel1( DbgKdpFactory, "Failed to create kd pipe\n" );
        ExitProcess( EXIT_FAILURE );
    }

    Start.lpDesktop = "WinSta0\\Default";

    if ( !CreateProcessA( "C:\\Program Files (x86)\\Windows Kits\\10\\Debuggers\\x64\\windbg.exe",
                          " -k com:pipe,port=\\\\.\\pipe\\nokd,resets=0,reconnect",
                          NULL,
                          NULL,
                          FALSE,
                          0,
                          NULL,
                          NULL,
                          &Start,
                          &Process ) ) {

        DbgKdpTraceLogLevel1( DbgKdpFactory, "Failed to create windbg process\n" );
        ExitProcess( EXIT_FAILURE );
    }

    CloseHandle( Process.hThread );

    //WaitForSingleObject( DbgKdpCommDevice.Extension.Pipe.FileHandle, INFINITE );

    DbgCoreEngine.DbgMsrRead( &DbgCoreEngine,
                              0xC0000082,
                              &DbgKdKernelBase );

    if ( DbgKdKernelBase == 0 ) {

        __debugbreak( );
    }

    DbgKdpTraceLogLevel1( DbgKdpFactory,
                          "IA32_MSR_LSTAR => %016llx\n",
                          DbgKdKernelBase );
    DbgKdKernelBase &= ~0xFFF;

    //DbgGdbSendf( &DbgCoreEngine, "qXfer:features:read:rax" );

        //
        // On my system, this is 10mb from the headers,
        // so I've decided to just take 10mb here, temporarily.
        // We should be able to take maybe 8mb safely?
        //
        // 0xAAC000
        //

        //DbgKdKernelBase -= 0xAAC000 - 0x1000;

    do {

        DbgKdKernelBase -= 0x1000;
        DbgCoreEngine.DbgMemoryRead( &DbgCoreEngine,
                                     DbgKdKernelBase,
                                     2,
                                     &DosHeader );
    } while ( DosHeader != IMAGE_DOS_SIGNATURE );

    DbgKdpTraceLogLevel1( DbgKdpFactory,
                          "DbgKdKernelBase => %016llx\n",
                          DbgKdKernelBase );

    //
    // Unfortunately, the Pcr + 0x108 field for KdVersionBlock, is only filled in
    // when kd is running, so we have to get a handle to KdVersionBlock through 
    // other means.
    //

    Status =  DbgCoreEngine.DbgMemoryRead( &DbgCoreEngine,
                                           DbgKdKernelBase +
                                           FIELD_OFFSET( IMAGE_DOS_HEADER, e_lfanew ),
                                           sizeof( LONG32 ),
                                           &NewDos );

    DbgKdNtosHeaders = DbgKdKernelBase + NewDos;

    Status = DbgCoreEngine.DbgMemoryRead( &DbgCoreEngine,
                                          DbgKdNtosHeaders +
                                          FIELD_OFFSET( IMAGE_NT_HEADERS,
                                                        OptionalHeader.
                                                        DataDirectory[ IMAGE_DIRECTORY_ENTRY_DEBUG ] ),
                                          sizeof( IMAGE_DATA_DIRECTORY ),
                                          &Directory );

    // ASSERT Directory present.

    DebugDirectory = ( PIMAGE_DEBUG_DIRECTORY )HeapAlloc( GetProcessHeap( ),
                                                          0,
                                                          Directory.Size );

    DbgKdpTraceLogLevel1( DbgKdpFactory,
                          "IMAGE_DEBUG_DIRECTORY read from: %016llx to %016llx (length: %d)\n",
                          DbgKdKernelBase + Directory.VirtualAddress,
                          ( ULONG64 )DebugDirectory,
                          Directory.Size );

    Status = DbgCoreEngine.DbgMemoryRead( &DbgCoreEngine,
                                          DbgKdKernelBase + Directory.VirtualAddress,
                                          Directory.Size,
                                          DebugDirectory );

    // ASSERT DebugDirectory.Type == IMAGE_DEBUG_TYPE_CODEVIEW

    CodeView = ( PIMAGE_DEBUG_CODE_VIEW )HeapAlloc( GetProcessHeap( ),
                                                    0,
                                                    DebugDirectory->SizeOfData );

    DbgKdpTraceLogLevel1( DbgKdpFactory,
                          "IMAGE_DEBUG_CODE_VIEW read from: %016llx to %016llx (length: %d)\n",
                          DbgKdKernelBase + DebugDirectory->AddressOfRawData,
                          ( ULONG64 )CodeView,
                          DebugDirectory->SizeOfData );

    Status = DbgCoreEngine.DbgMemoryRead( &DbgCoreEngine,
                                          DbgKdKernelBase + DebugDirectory->AddressOfRawData,
                                          DebugDirectory->SizeOfData,
                                          CodeView );

    HeapFree( GetProcessHeap( ), 0, DebugDirectory );
    DebugDirectory = NULL;

    DbgPdbBuildSymbolFile( File,
                           CodeView->Path,
                           &CodeView->UniqueId,
                           CodeView->Age );

    DbgKdpTraceLogLevel1( DbgKdpFactory, "Kernel PDB => %S\n", File );

    FileHandle = CreateFileW( File,
                              FILE_READ_DATA,
                              FILE_SHARE_READ,
                              NULL,
                              OPEN_EXISTING,
                              0,
                              NULL );

    //
    // TODO: Check if GetLastError != ERROR_FILE_NOT_FOUND 
    //

    if ( FileHandle == INVALID_HANDLE_VALUE ) {

        DbgPdbBuildSymbolUrl( Url,
                              CodeView->Path,
                              &CodeView->UniqueId,
                              CodeView->Age );

        DbgKdpTraceLogLevel1( DbgKdpFactory, "Kernel PDB Downloading => %S\n", Url );

        DbgPdbDownload( Url,
                        NULL,
                        &Length );

        DbgKdpTraceLogLevel1( DbgKdpFactory,
                              "Kernel PDB Length => %d\n",
                              Length );

        Pdb = VirtualAlloc( NULL,
                            Length + 0x2000,
                            MEM_COMMIT | MEM_RESERVE,
                            PAGE_READWRITE );

        DbgPdbDownload( Url,
                        Pdb,
                        &Length );

        FileHandle = CreateFileW( File,
                                  FILE_READ_DATA | FILE_WRITE_DATA,
                                  FILE_SHARE_READ | FILE_SHARE_WRITE,
                                  NULL,
                                  CREATE_ALWAYS,
                                  FILE_ATTRIBUTE_NORMAL,
                                  NULL );

        WriteFile( FileHandle,
                   Pdb,
                   Length,
                   &Bytes,
                   NULL );

        CloseHandle( FileHandle );

        VirtualFree( Pdb, 0, MEM_RELEASE | MEM_FREE );
    }
    else {

        //
        // Already downloaded in a previous run.
        //

        CloseHandle( FileHandle );
    }

    HeapFree( GetProcessHeap( ), 0, CodeView );
    CodeView = NULL;

    DbgPdbLoad( File, &DbgPdbKernelContext );

    //KiWaitAlways
    //KiWaitNever
    //KdpDataBlockEncoded

    DbgPdbAddressOfName( &DbgPdbKernelContext,
                         L"KiWaitAlways",
                         &KiWaitAlwaysRva );
    KiWaitAlwaysAddress = DbgKdKernelBase + KiWaitAlwaysRva;
    Status = DbgCoreEngine.DbgMemoryRead( &DbgCoreEngine,
                                          KiWaitAlwaysAddress,
                                          sizeof( ULONG64 ),
                                          &KiWaitAlways );
    DbgKdpTraceLogLevel1( DbgKdpFactory,
                          "KiWaitAlways => %016llx\n",
                          KiWaitAlwaysAddress );

    DbgPdbAddressOfName( &DbgPdbKernelContext,
                         L"KiWaitNever",
                         &KiWaitNeverRva );
    KiWaitNeverAddress = DbgKdKernelBase + KiWaitNeverRva;
    Status = DbgCoreEngine.DbgMemoryRead( &DbgCoreEngine,
                                          KiWaitNeverAddress,
                                          sizeof( ULONG64 ),
                                          &KiWaitNever );
    DbgKdpTraceLogLevel1( DbgKdpFactory,
                          "KiWaitNever => %016llx\n",
                          KiWaitNeverAddress );

    DbgPdbAddressOfName( &DbgPdbKernelContext,
                         L"KdpDataBlockEncoded",
                         &KdpDataBlockEncodedRva );
    KdpDataBlockEncodedAddress = DbgKdKernelBase + KdpDataBlockEncodedRva;
    KdpDataBlockEncoded = KdpDataBlockEncodedAddress;
    Status = DbgCoreEngine.DbgMemoryRead( &DbgCoreEngine,
                                          KdpDataBlockEncodedAddress,
                                          sizeof( BOOLEAN ),
                                          &KdpDataBlockEncodedValue );

    DbgKdpTraceLogLevel1( DbgKdpFactory,
                          "KdpDataBlockEncoded => %016llx\n", KdpDataBlockEncodedAddress );

    DbgPdbAddressOfName( &DbgPdbKernelContext,
                         L"KdDebuggerDataBlock",
                         &KdDebuggerDataBlockRva );
    KdDebuggerDataBlockAddress = DbgKdKernelBase + KdDebuggerDataBlockRva;
    Status = DbgCoreEngine.DbgMemoryRead( &DbgCoreEngine,
                                          KdDebuggerDataBlockAddress,
                                          sizeof( KDDEBUGGER_DATA64 ),
                                          &DbgKdDebuggerBlock );
    DbgKdpTraceLogLevel1( DbgKdpFactory,
                          "KdDebuggerDataBlock => %016llx\n", KdDebuggerDataBlockAddress );

    if ( KdpDataBlockEncodedValue != 0 ) {

        DbgKdpDecodeDataBlock( &DbgKdDebuggerBlock );
        KdpDataBlockEncodedValue = 0;
        Status = DbgCoreEngine.DbgMemoryWrite( &DbgCoreEngine,
                                               KdpDataBlockEncodedAddress,
                                               sizeof( BOOLEAN ),
                                               &KdpDataBlockEncodedValue );
        DbgKdpTraceLogLevel1( DbgKdpFactory,
                              "KdDebuggerDataBlock decoded, value set.\n" );
        Status = DbgCoreEngine.DbgMemoryWrite( &DbgCoreEngine,
                                               KdDebuggerDataBlockAddress,
                                               sizeof( KDDEBUGGER_DATA64 ),
                                               &DbgKdDebuggerBlock );
        DbgKdpTraceLogLevel1( DbgKdpFactory,
                              "KdDebuggerDataBlock written back!\n" );
    }

    if ( DbgKdDebuggerBlock.Header.OwnerTag != 'GBDK' ) {

        DbgKdpTraceLogLevel1( DbgKdpFactory, "Invalid KDBG signature!\n" );
        __debugbreak( );
    }

    DbgPdbAddressOfName( &DbgPdbKernelContext,
                         L"KdVersionBlock",
                         &KdVersionBlockRva );
    KdVersionBlockAddress = DbgKdKernelBase + KdVersionBlockRva;
    Status = DbgCoreEngine.DbgMemoryRead( &DbgCoreEngine,
                                          KdVersionBlockAddress,
                                          sizeof( DBGKD_GET_VERSION64 ),
                                          &DbgKdVersionBlock );
    DbgKdpTraceLogLevel1( DbgKdpFactory,
                          "KdVersionBlock => %016llx\n",
                          KdVersionBlockAddress );

    DbgPdbAddressOfName( &DbgPdbKernelContext,
                         L"KiProcessorBlock",
                         &KiProcessorBlockRva );
    KiProcessorBlockAddress = DbgKdKernelBase + KiProcessorBlockRva;
    Status = DbgCoreEngine.DbgMemoryRead( &DbgCoreEngine,
                                          KiProcessorBlockAddress,
                                          sizeof( ULONG64 ) * DbgKdProcessorCount,
                                          &DbgKdProcessorBlock );
    DbgKdpTraceLogLevel1( DbgKdpFactory,
                          "KiProcessorBlock => %016llx\n",
                          KiProcessorBlockAddress );

    //
    // Write back the decoded debugger data block
    //
    // TODO: Update KdpDataBlockEncoded
    //

    goto DbgKdTraceFlag;

    while ( TRUE ) {

        //
        // Poll for a break-in from windows kd or
        // a break-in from gdb such as a breakpoint being hit.
        //

        Status = DbgCoreEngine.DbgCheckQueue( &DbgCoreEngine );

        if ( !DBG_SUCCESS( Status ) ) {

            Status = DbgKdRecvPacket( KdTypeCheckQueue,
                                      NULL,
                                      NULL,
                                      NULL );

            if ( !DBG_SUCCESS( Status ) ) {

                //Sleep( 200 );
                continue;
            }
            else {

                DbgKdpTraceLogLevel1( DbgKdpFactory, "Kd break-in.\n" );
            }

            while ( !DBG_SUCCESS(
                DbgCoreEngine.DbgProcessorBreak(
                    &DbgCoreEngine,
                    &CurrentProcessor ) ) );
            DbgCoreEngine.DbgCommDevice.DbgFlush( &DbgCoreEngine.DbgCommDevice.Extension );
        }
        else {

            DbgKdpTraceLogLevel1( DbgKdpFactory, "Engine break-in.\n" );
        }

        //
        // This is the KdpSendWaitContinue loop of ntoskrnl, but 
        // a re-implementation.
        //
        // TODO: Better context management.
        //

    DbgKdTraceFlag:

        DbgKdStoreKvaShadow( &CodeContext, &KvaState );

        DbgKdpBuildContextRecord( &CodeContext );

        //
        // TODO: Hw bp emu sup
        //

        DbgCoreEngine.DbgBreakpointClear( &DbgCoreEngine,
                                          BreakOnExecute,
                                          CodeContext.Rip );
    DbgKdpResend:
        RtlZeroMemory( &State, sizeof( DBGKD_WAIT_STATE_CHANGE ) );

        State.u.Exception.ExceptionRecord.ExceptionCode = STATUS_BREAKPOINT;

        State.u.Exception.FirstChance = TRUE;
        State.u.Exception.ExceptionRecord.ExceptionAddress = CodeContext.Rip;

        DbgKdpSetCommonState( 0x3030, &CodeContext, &State );
        DbgKdpSetContextState( &State, &CodeContext );

        Head.Length = sizeof( DBGKD_WAIT_STATE_CHANGE );
        Head.MaximumLength = sizeof( DBGKD_WAIT_STATE_CHANGE );
        Head.Buffer = ( PCHAR )&State;

        Body.Length = 0;
        Body.MaximumLength = 0x1000;
        Body.Buffer = DbgKdMessageBuffer;

        DbgKdSendPacket( KdTypeStateChange,
                         &Head,
                         &Body,
                         NULL );

        DbgKdDebuggerEnabled = TRUE;

        do {

            //
            // Just a note: we might want to call DiscardPipe more
            // often, because of the way the real uart16550 operates
            // with a 16 bytes FIFO, only capable of holding a single
            // KD_PACKET structure at all times, this means all resends 
            // are not actually going to be received.
            //

            Head.Length = sizeof( DBGKD_MANIPULATE_STATE64 );
            Head.MaximumLength = sizeof( DBGKD_MANIPULATE_STATE64 );
            Head.Buffer = ( PCHAR )&Manip;

            Body.Length = 0;
            Body.MaximumLength = 0x1000;
            Body.Buffer = DbgKdMessageBuffer;

            RtlZeroMemory( &Manip, sizeof( DBGKD_MANIPULATE_STATE64 ) );

            Status = DbgKdRecvPacket( KdTypeStateManipulate,
                                      &Head,
                                      &Body,
                                      NULL );

            if ( Status == DBG_STATUS_RESEND ) {

                DbgKdpTraceLogLevel1( DbgKdpFactory, "Resend!\n" );
                goto DbgKdpResend;
            }

            if ( !DBG_SUCCESS( Status ) ) {

                //
                // TODO: Deal with resend.
                //

                continue;
            }

            if ( Manip.ApiNumber >= DbgKdMinimumManipulate &&
                 Manip.ApiNumber < DbgKdMaximumManipulate ) {

                CHAR DbgKdApi[ DbgKdMaximumManipulate - DbgKdMinimumManipulate ][ 48 ] = {
                    "DbgKdReadVirtualMemoryApi",
                    "DbgKdWriteVirtualMemoryApi",
                    "DbgKdGetContextApi",
                    "DbgKdSetContextApi",
                    "DbgKdWriteBreakPointApi",
                    "DbgKdRestoreBreakPointApi",
                    "DbgKdContinueApi",
                    "DbgKdReadControlSpaceApi",
                    "DbgKdWriteControlSpaceApi",
                    "DbgKdReadIoSpaceApi",
                    "DbgKdWriteIoSpaceApi",
                    "DbgKdRebootApi",
                    "DbgKdContinueApi2",
                    "DbgKdReadPhysicalMemoryApi",
                    "DbgKdWritePhysicalMemoryApi",
                    "DbgKdQuerySpecialCallsApi",
                    "DbgKdSetSpecialCallApi",
                    "DbgKdClearSpecialCallsApi",
                    "DbgKdSetInternalBreakPointApi",
                    "DbgKdGetInternalBreakPointApi",
                    "DbgKdReadIoSpaceExtendedApi",
                    "DbgKdWriteIoSpaceExtendedApi",
                    "DbgKdGetVersionApi",
                    "DbgKdWriteBreakPointExApi",
                    "DbgKdRestoreBreakPointExApi",
                    "DbgKdCauseBugCheckApi",
                    "DbgKdPlaceholder1",
                    "DbgKdPlaceholder2",
                    "DbgKdPlaceholder3",
                    "DbgKdPlaceholder4",
                    "DbgKdPlaceholder5",
                    "DbgKdPlaceholder6",
                    "DbgKdSwitchProcessor",
                    "DbgKdPageInApi",
                    "DbgKdReadMachineSpecificRegister",
                    "DbgKdWriteMachineSpecificRegister",
                    "OldVlm1",
                    "OldVlm2",
                    "DbgKdSearchMemoryApi",
                    "DbgKdGetBusDataApi",
                    "DbgKdSetBusDataApi",
                    "DbgKdCheckLowMemoryApi",
                    "DbgKdClearAllInternalBreakpointsApi",
                    "DbgKdFillMemoryApi",
                    "DbgKdQueryMemoryApi",
                    "DbgKdSwitchPartition",
                    "DbgKdPlaceholder7",
                    "DbgKdGetContextEx",
                    "DbgKdSetContextEx",
                    "DbgKdWriteCustomBreakpointEx",
                    "DbgKdReadPhysicalMemoryLong",
                };

                DbgKdpTraceLogLevel0( DbgKdpFactory,
                                      "%s %lx\n",
                                      DbgKdApi[ Manip.ApiNumber -
                                      DbgKdMinimumManipulate ],
                                      Manip.ApiNumber );

                switch ( Manip.ApiNumber ) {
                case DbgKdReadVirtualMemoryApi:
                    DbgKdpReadVirtualMemory( &Manip,
                                             &Body );
#if 0
                    if ( !NT_SUCCESS( Manip.ReturnStatus ) )
                        printf( "Address: %016llx Length: %08lx Status: %08lx ",
                                Manip.u.ReadMemory.TargetBaseAddress,
                                Manip.u.ReadMemory.TransferCount,
                                Manip.ReturnStatus );
#endif
#if 0
                    PWSTR  Nigcode;
                    LONG32 Nigzone;

                    DbgPdbNameOfAddress( &DbgPdbKernelContext,
                                         Manip.u.ReadMemory.TargetBaseAddress -
                                         DbgKdKernelBase,
                                         &Nigcode,
                                         &Nigzone );

                    printf( "Address: %016llx Length: %08lx Status: %08lx ",
                            Manip.u.ReadMemory.TargetBaseAddress,
                            Manip.u.ReadMemory.TransferCount,
                            Manip.ReturnStatus );
                    if ( Nigzone != -1 ) {
                        printf( "Symbol: %S", Nigcode );
                        if ( Nigzone ) {

                            if ( Nigzone > 0 ) {
                                printf( "+%d", Nigzone );
                            }
                            else {
                                printf( "%d", Nigzone );
                            }
                        }
                }
#endif
                    break;
                case DbgKdWriteVirtualMemoryApi:
                    DbgKdpWriteVirtualMemory( &Manip,
                                              &Body );
                    break;
                case DbgKdGetVersionApi:
                    DbgKdpGetVersion( &Manip,
                                      &Body );
                    break;
                case DbgKdGetContextApi:
                    DbgKdpGetContext( &Manip,
                                      &Body );
                    break;
                case DbgKdSetContextApi:
                    DbgKdpSetContext( &Manip,
                                      &Body );
                    break;
                case DbgKdGetContextEx:
                    DbgKdpGetContextEx( &Manip,
                                        &Body );
                    break;
                case DbgKdSetContextEx:
                    DbgKdpSetContextEx( &Manip,
                                        &Body );
                    break;
                case DbgKdReadControlSpaceApi:
                    DbgKdpReadControlSpace( &Manip,
                                            &Body );
                    break;
                case DbgKdWriteControlSpaceApi:
                    DbgKdpWriteControlSpace( &Manip,
                                             &Body );
                    break;
                case DbgKdContinueApi:

                    //
                    // I think my KdReceivePacket function may be fucked,
                    // this was my only fix, otherwise it would resend this packet 
                    // indefinitely, and freeze up.
                    //

                    Manip.ReturnStatus = STATUS_UNSUCCESSFUL;
                    DbgKdSendPacket( KdTypeStateManipulate,
                                     &Head,
                                     NULL,
                                     NULL );
                    DbgKdDebuggerEnabled = FALSE;

                    DbgKdpCommDevice.DbgFlush( &DbgKdpCommDevice.Extension );

                    break;
                case DbgKdContinueApi2:

                    Manip.ReturnStatus = STATUS_UNSUCCESSFUL;
                    DbgKdSendPacket( KdTypeStateManipulate,
                                     &Head,
                                     NULL,
                                     NULL );

                    DbgKdpCommDevice.DbgFlush( &DbgKdpCommDevice.Extension );

                    if ( !NT_SUCCESS( Manip.u.Continue2.ContinueStatus ) ) {

                        DbgKdDebuggerEnabled = FALSE;
                        break;
                    }

                    if ( Manip.u.Continue2.ControlSet.TraceFlag ) {

                        //
                        // TODO: Error code => 80000004
                        //

                        DbgKdRestoreKvaShadow( &CodeContext, &KvaState );

                        DbgKdpDebugEmu.Dr6.Bs = TRUE;
                        DbgCoreEngine.DbgProcessorStep( &DbgCoreEngine );
                        goto DbgKdTraceFlag;
                    }

                    if ( Manip.u.Continue2.ControlSet.Dr7 != 0x400 &&
                         Manip.u.Continue2.ControlSet.Dr7 != 0 ) {

                        printf( "WARNING: shut the fuck up.\n" );
                    }

                    DbgKdDebuggerEnabled = FALSE;
                    break;
                case DbgKdWriteBreakPointApi:
                    DbgKdpInsertBreakpoint( &Manip,
                                            &Body );
                    break;
                case DbgKdRestoreBreakPointApi:
                    DbgKdpClearBreakpoint( &Manip,
                                           &Body );
                    break;
                case DbgKdQueryMemoryApi:

                    if ( Manip.u.QueryMemory.Address >= 0x7FFFFFFEFFFF ) {

                        Manip.u.QueryMemory.AddressSpace = SystemSpace;
                    }
                    else {

                        Manip.u.QueryMemory.AddressSpace = UserSpace;
                    }
                    Manip.u.QueryMemory.Flags = 7;
                    Manip.u.QueryMemory.Reserved = 0;
                    Manip.ReturnStatus = STATUS_SUCCESS;

                    DbgKdSendPacket( KdTypeStateManipulate,
                                     &Head,
                                     NULL,
                                     NULL );

                    break;
                case DbgKdSetSpecialCallApi:
                case DbgKdClearSpecialCallsApi:
                case DbgKdSetInternalBreakPointApi:
                case DbgKdClearAllInternalBreakpointsApi:
                    Manip.ReturnStatus = STATUS_SUCCESS;
                    DbgKdSendPacket( KdTypeStateManipulate,
                                     &Head,
                                     NULL,
                                     NULL );
                    continue;
                case DbgKdSwitchProcessor:
                    Manip.ReturnStatus = STATUS_SUCCESS;
                    DbgKdSendPacket( KdTypeStateManipulate,
                                     &Head,
                                     NULL,
                                     NULL );
                    DbgCoreEngine.DbgProcessorSwitch( &DbgCoreEngine,
                                                      Manip.Processor );
                    goto DbgKdTraceFlag;
                default:
                    DbgKdpTraceLogLevel1( DbgKdpFactory,
                                          "Unsupported api number: %s\n",
                                          DbgKdApi[ Manip.ApiNumber - DbgKdMinimumManipulate ] );

                    Manip.ReturnStatus = STATUS_UNSUCCESSFUL;
                    DbgKdSendPacket( KdTypeStateManipulate,
                                     &Head,
                                     NULL,
                                     NULL );
                    break;
            }
        }
            else {

                DbgKdpTraceLogLevel1( DbgKdpFactory,
                                      "Invalid api number: %lx\n",
                                      Manip.ApiNumber );
            }

    } while ( DbgKdDebuggerEnabled );

    DbgKdRestoreKvaShadow( &CodeContext, &KvaState );
    DbgCoreEngine.DbgProcessorContinue( &DbgCoreEngine );
    DbgCoreEngine.DbgCommDevice.DbgFlush( &DbgCoreEngine.DbgCommDevice.Extension );
}

    //DbgGdbSendf( "D" );
    //closesocket( DbgGdbDevice.Ext.Sock.Socket );

    __debugbreak( );

    WaitForSingleObject( Process.hProcess, INFINITE );
    ExitProcess( 0 );
}
