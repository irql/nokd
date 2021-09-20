
#include <kdpl.h>

// cba nigga
EXTERN_C
NTSTATUS
KdImageSection(
    _In_      PVOID  ImageBase,
    _In_      PCHAR  SectionName,
    _Out_opt_ PVOID* SectionBase,
    _Out_opt_ ULONG* SectionSize
);

EXTERN_C
PVOID
KdSearchSignature(
    _In_ PVOID BaseAddress,
    _In_ ULONG Length,
    _In_ PCHAR Signature
);

KD_DEBUG_DEVICE             KdDebugDevice;

UCHAR                       KdpMessageBuffer[ 0x1000 ];
CHAR                        KdpPathBuffer[ 0x1000 ];

PUSHORT                     KeProcessorLevel;
ULONG32                     KdDebuggerNotPresent_ = TRUE;

KD_BREAKPOINT_ENTRY         KdpBreakpointTable[ KD_BREAKPOINT_TABLE_LENGTH ] = { 0 };

KDDEBUGGER_DATA64           KdDebuggerDataBlock = { 0 };

DBGKD_GET_VERSION64         KdVersionBlock = {
    0x0F,
    0x536A,
    0x06,
    0x02,
    0x46,
    0x8664,
    0x0C,
    0x03,
    0x33,
    0,
    0,
    0,
    0,
    0 };
LIST_ENTRY                  KdpDebuggerDataListHead;
KD_CONTEXT                  KdpContext = { 0 };

ULONG64
DbgKdRead64(
    _In_ PULONG64 Address
)
{
    NTSTATUS ntStatus;
    ULONG64  Buffer;
    ULONG64  Transferred;

    ntStatus = DbgKdMmCopyMemory( &Buffer,
                                  Address,
                                  sizeof( ULONG64 ),
                                  MM_COPY_MEMORY_VIRTUAL,
                                  &Transferred );
    NT_ASSERT( NT_SUCCESS( ntStatus ) );
    NT_ASSERT( Transferred == sizeof( ULONG64 ) );

    return Buffer;
}

ULONG32
DbgKdRead32(
    _In_ PULONG32 Address
)
{
    NTSTATUS ntStatus;
    ULONG32  Buffer;
    ULONG64  Transferred;

    ntStatus = DbgKdMmCopyMemory( &Buffer,
                                  Address,
                                  sizeof( ULONG32 ),
                                  MM_COPY_MEMORY_VIRTUAL,
                                  &Transferred );
    NT_ASSERT( NT_SUCCESS( ntStatus ) );
    NT_ASSERT( Transferred == sizeof( ULONG32 ) );

    return Buffer;
}

USHORT
DbgKdRead16(
    _In_ PUSHORT Address
)
{
    NTSTATUS ntStatus;
    USHORT   Buffer;
    ULONG64  Transferred;

    ntStatus = DbgKdMmCopyMemory( &Buffer,
                                  Address,
                                  sizeof( USHORT ),
                                  MM_COPY_MEMORY_VIRTUAL,
                                  &Transferred );
    NT_ASSERT( NT_SUCCESS( ntStatus ) );
    NT_ASSERT( Transferred == sizeof( USHORT ) );

    return Buffer;
}

UCHAR
DbgKdRead8(
    _In_ PUCHAR Address
)
{
    NTSTATUS ntStatus;
    UCHAR    Buffer;
    ULONG64  Transferred;

    ntStatus = DbgKdMmCopyMemory( &Buffer,
                                  Address,
                                  sizeof( UCHAR ),
                                  MM_COPY_MEMORY_VIRTUAL,
                                  &Transferred );
    NT_ASSERT( NT_SUCCESS( ntStatus ) );
    NT_ASSERT( Transferred == sizeof( UCHAR ) );

    return Buffer;
}

BOOLEAN
KdPollBreakIn(

)
{
    if ( KdpContext.BreakRequested == TRUE ) {

        KdpContext.BreakRequested = FALSE;
        return TRUE;
    }

    return KdDebugDevice.KdReceivePacket( KdTypeCheckQueue,
                                          NULL,
                                          NULL,
                                          NULL,
                                          &KdpContext ) == KdStatusSuccess;
}

VOID
KdReportStateChange(
    _In_    NTSTATUS Status,
    _Inout_ PCONTEXT ContextRecord
)
{
    EXCEPTION_RECORD64 ExceptRecord = { 0 };

    ExceptRecord.ExceptionAddress = ContextRecord->Rip;
    ExceptRecord.ExceptionCode = Status;

    KdpReportExceptionStateChange( &ExceptRecord,
                                   ContextRecord,
                                   FALSE );
}

VOID
KdLoadSymbols(
    _Inout_ PCONTEXT ContextRecord,
    _In_    PCHAR    ImageName,
    _In_    ULONG64  ImageBase,
    _In_    ULONG32  ImageSize,
    _In_    HANDLE   ProcessId,
    _In_    BOOLEAN  Unload
)
{
    KD_SYMBOL_INFO     Symbol;
    LONG32             Elfanew;
    STRING             PathName;

    Symbol.BaseAddress = ImageBase;
    Symbol.SizeOfImage = ImageSize;
    Symbol.ProcessId = ( ULONG64 )ProcessId;

    if ( NT_SUCCESS( DbgKdMmCopyMemory( &Elfanew,
        ( PVOID )( ImageBase + 0x3C ),
                                        sizeof( LONG32 ),
                                        MM_COPY_MEMORY_VIRTUAL,
                                        NULL ) ) ) {

        Symbol.CheckSum = 0;
        DbgKdMmCopyMemory( &Symbol.CheckSum,
            ( PVOID )( ImageBase + Elfanew + 0x40 ),
                           sizeof( ULONG32 ),
                           MM_COPY_MEMORY_VIRTUAL,
                           NULL );
    }

    RtlInitString( &PathName, ImageName );

    KdpReportLoadSymbolsStateChange( &PathName, &Symbol, Unload, ContextRecord );
}

VOID
KdpSetCommonState(
    _In_    ULONG32                  ApiNumber,
    _In_    PCONTEXT                 Context,
    _Inout_ PDBGKD_WAIT_STATE_CHANGE Change
)
{
    //
    // TODO: This procedure also deletes the breakpoint range at
    //       the context's rip.
    //

    ULONG64 InstructionCount;
    ULONG32 CurrentHandle;

    Change->ApiNumber = ApiNumber;
    Change->Processor = ( USHORT )DbgKdQueryCurrentPrcbNumber( );
    Change->ProcessorCount = DbgKdQueryProcessorCount( );
    if ( Change->ProcessorCount <= Change->Processor ) {

        Change->ProcessorCount = Change->Processor + 1;
    }

    Change->ProcessorLevel = *KeProcessorLevel;
    Change->CurrentThread = DbgKdQueryCurrentThread( );

    RtlZeroMemory( &Change->ControlReport, sizeof( DBGKD_CONTROL_REPORT ) );

    //
    // This is important breakpoint handling code,
    // this will set a stub-bp directly after the previous instruction,
    // this will be caught, and the old bp will be re-added.
    //

    for ( CurrentHandle = 0;
          CurrentHandle < KD_BREAKPOINT_TABLE_LENGTH;
          CurrentHandle++ ) {

        if ( KdpBreakpointTable[ CurrentHandle ].Flags & KD_BPE_SET ) {

            if ( KdpBreakpointTable[ CurrentHandle ].Address == Context->Rip ) {

                //
                // Normally the bp would be hit and the instruction pointer would 
                // be += the bp length -- assume dealt with
                //

                DbgKdRemoveBreakpoint( KdpBreakpointTable[ CurrentHandle ].Address );
                break;
            }
        }
    }

    Change->ProgramCounter = Context->Rip;

    __try {
        // TODO: DbgKdMmCopy
        RtlCopyMemory( Change->ControlReport.InstructionStream, ( PVOID )Context->Rip, 0x10 );
        InstructionCount = 0x10;
    }
    __except ( EXCEPTION_EXECUTE_HANDLER ) {

        InstructionCount = 0;
    }
    Change->ControlReport.InstructionCount = ( USHORT )InstructionCount;

}

VOID
KdpSetContextState(
    _Inout_ PDBGKD_WAIT_STATE_CHANGE Change,
    _In_    PCONTEXT                 Context
)
{
    Change->ControlReport.Dr6 = 0xFFFF0FF0;//KdGetPrcbSpecialRegisters( KeGetCurrentPrcb( ) )->KernelDr6;
    Change->ControlReport.Dr7 = 0x400;//KdGetPrcbSpecialRegisters( KeGetCurrentPrcb( ) )->KernelDr7;

    Change->ControlReport.SegCs = Context->SegCs;
    Change->ControlReport.SegDs = Context->SegDs;
    Change->ControlReport.SegEs = Context->SegEs;
    Change->ControlReport.SegFs = Context->SegFs;

    Change->ControlReport.EFlags = Context->EFlags;
    Change->ControlReport.ReportFlags = 1;

    if ( Context->SegCs == 0x10 ||
         Context->SegCs == 0x33 ) {

        Change->ControlReport.ReportFlags = 0x3;
    }
}

VOID
KdpPrintString(
    _In_ PANSI_STRING String
)
{
    STRING             Body;
    STRING             Head;
    DBGKD_PRINT_STRING Print = { 0 };

    Print.ProcessorLevel = *KeProcessorLevel;
    Print.ApiNumber = 0x3230;
    Print.Processor = ( USHORT )DbgKdQueryCurrentProcessorNumber( );
    Print.Length = String->Length;

    RtlCopyMemory( KdpMessageBuffer, String->Buffer, String->Length );

    Head.Length = sizeof( DBGKD_PRINT_STRING );
    Head.MaximumLength = sizeof( DBGKD_PRINT_STRING );
    Head.Buffer = ( PCHAR )&Print;

    Body.Length = String->Length;
    Body.MaximumLength = 0x1000;
    Body.Buffer = ( PCHAR )KdpMessageBuffer;

    KdDebugDevice.KdSendPacket( KdTypePrint,
                                &Head,
                                &Body,
                                &KdpContext );
}

VOID
KdPrint(
    _In_ PCHAR Format,
    _In_ ...
)
{
    CHAR        Buffer[ 512 ];
    ANSI_STRING String;
    va_list     List;

    va_start( List, Format );
    RtlStringCchVPrintfA( Buffer,
                          512,
                          Format,
                          List );
    va_end( List );
    String.MaximumLength = 512;
    String.Length = ( USHORT )( strlen( Buffer ) );
    String.Buffer = ( PCHAR )Buffer;

    KdpPrintString( &String );
}

BOOLEAN
KdpReportExceptionStateChange(
    _In_ PEXCEPTION_RECORD64 ExceptRecord,
    _In_ PCONTEXT            ExceptContext,
    _In_ BOOLEAN             SecondChance
)
{
    STRING                  Head;
    DBGKD_WAIT_STATE_CHANGE State = { 0 };

    KdpSetCommonState( 0x3030, ExceptContext, &State );
    RtlCopyMemory( &State.u.Exception.ExceptionRecord,
                   ExceptRecord,
                   sizeof( EXCEPTION_RECORD64 ) );
    State.u.Exception.FirstChance = !SecondChance;
    State.u.Exception.ExceptionRecord.ExceptionAddress = ExceptContext->Rip;
    KdpSetContextState( &State, ExceptContext );

    Head.Length = sizeof( DBGKD_WAIT_STATE_CHANGE );
    Head.MaximumLength = sizeof( DBGKD_WAIT_STATE_CHANGE );
    Head.Buffer = ( PCHAR )&State;

    return KdpSendWaitContinue( 0,
                                &Head,
                                NULL,
                                ExceptContext );
}

BOOLEAN
KdpReportLoadSymbolsStateChange(
    _In_    PSTRING         PathName,
    _In_    PKD_SYMBOL_INFO Symbol,
    _In_    BOOLEAN         Unload,
    _Inout_ PCONTEXT        Context
)
{
    STRING                  Head;
    STRING                  Body;
    PSTRING                 BodyAddress;
    DBGKD_WAIT_STATE_CHANGE LoadSymbol = { 0 };

    KdpSetCommonState( 0x3031, Context, &LoadSymbol );
    KdpSetContextState( &LoadSymbol, Context );

    LoadSymbol.u.LoadSymbols.BaseOfDll = Symbol->BaseAddress;
    LoadSymbol.u.LoadSymbols.ProcessId = Symbol->ProcessId;
    LoadSymbol.u.LoadSymbols.CheckSum = Symbol->CheckSum;
    LoadSymbol.u.LoadSymbols.SizeOfImage = Symbol->SizeOfImage;
    LoadSymbol.u.LoadSymbols.UnloadSymbols = Unload;

    if ( PathName != NULL ) {

        BodyAddress = &Body;

        LoadSymbol.u.LoadSymbols.PathNameLength = PathName->Length + 1;
        Body.Length = PathName->Length + 1;
        Body.Buffer = KdpPathBuffer;
        Body.MaximumLength = 0x1000;

        RtlCopyMemory( KdpPathBuffer,
                       PathName->Buffer,
                       PathName->Length );
        KdpPathBuffer[ PathName->Length ] = 0;
    }
    else {
        Body.Length = 0;
        LoadSymbol.u.LoadSymbols.PathNameLength = 0;
        BodyAddress = NULL;
    }

    Head.Length = sizeof( DBGKD_WAIT_STATE_CHANGE );
    Head.MaximumLength = sizeof( DBGKD_WAIT_STATE_CHANGE );
    Head.Buffer = ( PCHAR )&LoadSymbol;

    return KdpSendWaitContinue( 0,
                                &Head,
                                BodyAddress,
                                Context );
}

BOOLEAN
KdpSendWaitContinue(
    _In_ ULONG64  Unused,
    _In_ PSTRING  StateChangeHead,
    _In_ PSTRING  StateChangeBody,
    _In_ PCONTEXT Context
)
{
    Context;
    Unused;

    //
    // This function prototype is matched with the nt
    // function, the first parameter is infact unreferenced.
    // 
    // Some things are quite different later down, but it still
    // works fine with WinDbg.
    //

    KD_STATUS                Status;
    STRING                   Head;
    STRING                   Body;
    ULONG32                  Length;
    ULONG32                  Processor;
    DBGKD_MANIPULATE_STATE64 Packet;

    Head.MaximumLength = sizeof( DBGKD_MANIPULATE_STATE64 );
    Head.Length = 0;
    Head.Buffer = ( PCHAR )&Packet;

    Body.MaximumLength = 0x1000;
    Body.Length = 0;
    Body.Buffer = ( PCHAR )&KdpMessageBuffer;

KdpResendPacket:
    KdDebugDevice.KdSendPacket( KdTypeStateChange,
                                StateChangeHead,
                                StateChangeBody,
                                &KdpContext );

    while ( !KdDebuggerNotPresent_ ) {

        while ( 1 ) {

            RtlZeroMemory( &Packet, sizeof( DBGKD_MANIPULATE_STATE64 ) );

            Head.MaximumLength = sizeof( DBGKD_MANIPULATE_STATE64 );
            Head.Length = 0;

            Body.MaximumLength = 0x1000;
            Body.Length = 0;

            Status = KdDebugDevice.KdReceivePacket( KdTypeStateManipulate,
                                                    &Head,
                                                    &Body,
                                                    &Length,
                                                    &KdpContext );
            if ( Status == KdStatusResend ) {

                //
                // This seems a little weird, but this is the same as 
                // nt!KdpSendWaitContinue, if a resend is requested at any point
                // the state is sent, I believe it is for when the debugger
                // is restarted maybe? Not sure.
                //

                goto KdpResendPacket;
            }

            if ( Status == KdStatusSuccess &&
                 Packet.ApiNumber >= DbgKdMinimumManipulate &&
                 Packet.ApiNumber < DbgKdMaximumManipulate ) {

                switch ( Packet.ApiNumber ) {
                case DbgKdReadVirtualMemoryApi:
                    KdpReadVirtualMemory( &Packet,
                                          &Body );
                    break;
                case DbgKdWriteVirtualMemoryApi:
                    KdpWriteVirtualMemory( &Packet,
                                           &Body );
                    break;
                case DbgKdGetContextApi:
                    KdpGetContext( &Packet,
                                   &Body,
                                   Context );
                    break;
                case DbgKdSetContextApi:
                    KdpSetContext( &Packet,
                                   &Body,
                                   Context );
                    break;
                case DbgKdWriteBreakPointApi:
                    KdpInsertBreakpoint( &Packet,
                                         &Body );
                    break;
                case DbgKdRestoreBreakPointApi:
                    KdpRemoveBreakpoint( &Packet,
                                         &Body );
                    break;
                case DbgKdContinueApi:
                    return NT_SUCCESS( Packet.u.Continue.ContinueStatus );
                case DbgKdReadControlSpaceApi:
                    KdpReadControlSpace( &Packet,
                                         &Body );
                    break;
                case DbgKdWriteControlSpaceApi:
                    KdpWriteControlSpace( &Packet,
                                          &Body );
                    break;
                case DbgKdReadIoSpaceApi:
                    KdpReadIoSpace( &Packet,
                                    &Body );
                    break;
                case DbgKdWriteIoSpaceApi:
                    KdpWriteIoSpace( &Packet,
                                     &Body );
                    break;
                case DbgKdRebootApi:
                case DbgKdCauseBugCheckApi:

                    //KdThawProcessors( );
                    //KeBugCheck( 0 );
                    break;
                case DbgKdContinueApi2:
                    if ( !NT_SUCCESS( Packet.u.Continue2.ContinueStatus ) ) {

                        return FALSE;
                    }

                    if ( Packet.u.Continue2.ControlSet.TraceFlag ) {

                        // maybe we call a function and let the nigcode deal with it?
                        // my mtf code will catch this and handle it silently but still.
                        Context->EFlags |= 0x100;
                    }

                    for ( Processor = 0;
                          Processor < DbgKdQueryProcessorCount( );
                          Processor++ ) {

                        DbgKdQueryProcessorContext( Processor )->Dr6 = 0;
                        DbgKdQueryProcessorContext( Processor )->Dr7 = Packet.u.Continue2.ControlSet.Dr7;
                    }

                    return TRUE;
                case DbgKdReadPhysicalMemoryApi:
                    KdpReadPhysicalMemory( &Packet,
                                           &Body );
                    break;
                case DbgKdWritePhysicalMemoryApi:
                    KdpWritePhysicalMemory( &Packet,
                                            &Body );
                    break;
                case DbgKdGetVersionApi:
                    KdpGetVersion( &Packet,
                                   &Body );
                    break;
                case DbgKdSetSpecialCallApi:
                case DbgKdClearSpecialCallsApi:
                case DbgKdSetInternalBreakPointApi:
                case DbgKdClearAllInternalBreakpointsApi:
                    continue;
                case DbgKdGetContextEx:
                    KdpGetContextEx( &Packet,
                                     &Body,
                                     Context );
                    break;
                case DbgKdSetContextEx:
                    KdpSetContextEx( &Packet,
                                     &Body,
                                     Context );
                    break;
                case DbgKdQueryMemoryApi:
                    KdpQueryMemory( &Packet,
                                    &Body );
                    break;
                case DbgKdSwitchProcessor:

                    if ( Packet.Processor == ( USHORT )DbgKdQueryCurrentProcessorNumber( ) ) {

                        goto KdpResendPacket;
                    }

                    DbgKdSwapProcessor( Packet.Processor );
                    return TRUE;
                default:
                    Packet.ReturnStatus = STATUS_UNSUCCESSFUL;
                    Head.Buffer = ( PCHAR )&Packet;
                    Head.Length = sizeof( DBGKD_MANIPULATE_STATE64 );
                    KdDebugDevice.KdSendPacket( KdTypeStateManipulate,
                                                &Head,
                                                NULL,
                                                &KdpContext );
                    //KdPrint( "KdApiNumber unhandled: %#.4lx\n", Packet.ApiNumber );
                    break;
                }
            }
        }

        KdDebugDevice.KdSendPacket( KdTypeStateChange,
                                    StateChangeHead,
                                    StateChangeBody,
                                    &KdpContext );
    }

    return TRUE;
}

NTSTATUS
KdTryConnect(

)
{

    if ( !NT_SUCCESS( KdVmwRpcConnect( ) ) ) {

        return STATUS_UNSUCCESSFUL;
    }

    //KdDebuggerNotPresent_ = FALSE;

    return STATUS_SUCCESS;
}

NTSTATUS
KdDriverLoad(

)
{
    PVOID              ImageBase;
    ULONG              ImageSize;
    PVOID              SectionTextBase;
    ULONG              SectionTextSize;
    PVOID              SectionPageLKBase;
    ULONG              SectionPageLKSize;
    ULONG_PTR          KdCopyDataBlockAddress;
    ULONG_PTR          KeProcessorLevelAddress;
    ULONG_PTR          KdpDataBlockEncodedAddress;
    ULONG_PTR          KiWaitAlwaysAddress;
    ULONG_PTR          KiWaitNeverAddress;
    ULONG_PTR          KdEncodeDataBlockAddress;
    ULONG_PTR          KiWaitAlways;
    ULONG_PTR          KiWaitNever;
    ULONG32            Length;
    ULONG64*           Long;

    ULONG_PTR          KdDebuggerDataBlockAddress;

    ImageBase = NULL;
    ImageSize = 0;

    SectionTextBase = NULL;
    SectionTextSize = 0;

    //
    // "Guestify" everything!
    //

    ImageBase = ( PVOID )MmKernelBase;
    ImageSize = ( ULONG )MmKernelSize;

    KdImageSection( ImageBase, ".text\0\0", &SectionTextBase, &SectionTextSize );
    KdImageSection( ImageBase, "PAGELK\0", &SectionPageLKBase, &SectionPageLKSize );

    KdCopyDataBlockAddress = ( ULONG_PTR )KdSearchSignature( ( PVOID )( ( ULONG_PTR )ImageBase + ( ULONG_PTR )SectionTextBase ),
                                                             SectionTextSize,
                                                             "80 3D ? ? ? ? ? 4C 8D 05 ? ? ? ?" );

    if ( KdCopyDataBlockAddress == 0 ) {

        return STATUS_UNSUCCESSFUL;
    }
#if 0
    KdDebuggerDataBlockAddress = ( ULONG64 )( KdCopyDataBlockAddress + 14 + ( LONG32 )DbgKdRead32( ( ULONG32* )( KdCopyDataBlockAddress + 10 ) ) );


    KeProcessorLevelAddress = ( ULONG_PTR )KdSearchSignature( ( PVOID )( ( ULONG_PTR )ImageBase + ( ULONG_PTR )SectionTextBase ),
                                                              SectionTextSize,
                                                              "66 89 01 0F B7 05 ? ? ? ? 66 89 41 02" );

    if ( KeProcessorLevelAddress == 0 ) {

        return STATUS_UNSUCCESSFUL;
    }

    KeProcessorLevel = ( PUSHORT )( KeProcessorLevelAddress + 10 + ( LONG32 )DbgKdRead32( ( ULONG32* )( KeProcessorLevelAddress + 6 ) ) );


    KdEncodeDataBlockAddress = ( ULONG_PTR )KdSearchSignature( ( PVOID )( ( ULONG_PTR )ImageBase + ( ULONG_PTR )SectionPageLKBase ),
                                                               SectionPageLKSize,
                                                               "E8 ? ? ? ? 33 DB 48 8D 8F ? ? ? ?" );

    KdEncodeDataBlockAddress =  KdEncodeDataBlockAddress + 5 + ( LONG64 )( LONG32 )DbgKdRead32( ( ULONG32* )( KdEncodeDataBlockAddress + 1 ) );

    KiWaitNeverAddress = KdEncodeDataBlockAddress + 12;
    KiWaitNeverAddress = ( ULONG64 )( KiWaitNeverAddress + 4 + ( LONG64 )( LONG32 )DbgKdRead32( ( ULONG32* )KiWaitNeverAddress ) );
    KdpDataBlockEncodedAddress =  KdEncodeDataBlockAddress + 25;
    KdpDataBlockEncodedAddress = ( ULONG64 )( KdpDataBlockEncodedAddress + 5 + ( LONG64 )( LONG32 )DbgKdRead32( ( ULONG32* )KdpDataBlockEncodedAddress ) );
    KiWaitAlwaysAddress = KdEncodeDataBlockAddress + 49;
    KiWaitAlwaysAddress = ( ULONG64 )( KiWaitAlwaysAddress + 4 + ( LONG64 )( LONG32 )DbgKdRead32( ( ULONG32* )KiWaitAlwaysAddress ) );

    DbgKdMmCopyMemory( &KdDebuggerDataBlock,
        ( PVOID )KdDebuggerDataBlockAddress,
                       sizeof( KDDEBUGGER_DATA64 ),
                       MM_COPY_MEMORY_VIRTUAL,
                       NULL );
#if 0
    RtlCopyMemory( &KdDebuggerDataBlock,
        ( PVOID )KdDebuggerDataBlockAddress,
                   sizeof( KDDEBUGGER_DATA64 ) );
#endif

    KiWaitAlways = DbgKdRead64( ( ULONG64* )KiWaitAlwaysAddress );
    KiWaitNever = DbgKdRead64( ( ULONG64* )KiWaitNeverAddress );
#else
    KdDebuggerDataBlockAddress = ( ULONG64 )( KdCopyDataBlockAddress + 14 + *( LONG32 * )( KdCopyDataBlockAddress + 10 ) );


    KeProcessorLevelAddress = ( ULONG_PTR )KdSearchSignature( ( PVOID )( ( ULONG_PTR )ImageBase + ( ULONG_PTR )SectionTextBase ),
                                                              SectionTextSize,
                                                              "66 89 01 0F B7 05 ? ? ? ? 66 89 41 02" );

    if ( KeProcessorLevelAddress == 0 ) {

        return STATUS_UNSUCCESSFUL;
    }

    KeProcessorLevel = ( PUSHORT )( KeProcessorLevelAddress + 10 + *( LONG32* )( KeProcessorLevelAddress + 6 ) );


    KdEncodeDataBlockAddress = ( ULONG_PTR )KdSearchSignature( ( PVOID )( ( ULONG_PTR )ImageBase + ( ULONG_PTR )SectionPageLKBase ),
                                                               SectionPageLKSize,
                                                               "E8 ? ? ? ? 33 DB 48 8D 8F ? ? ? ?" );

    KdEncodeDataBlockAddress =  KdEncodeDataBlockAddress + 5 + ( LONG64 )*( LONG32* )( KdEncodeDataBlockAddress + 1 );

    KiWaitNeverAddress = KdEncodeDataBlockAddress + 12;
    KiWaitNeverAddress = ( ULONG64 )( KiWaitNeverAddress + 4 + ( LONG64 )*( LONG32 * )( KiWaitNeverAddress ) );
    KdpDataBlockEncodedAddress =  KdEncodeDataBlockAddress + 25;
    KdpDataBlockEncodedAddress = ( ULONG64 )( KdpDataBlockEncodedAddress + 5 + ( LONG64 )*( LONG32* )( KdpDataBlockEncodedAddress ) );
    KiWaitAlwaysAddress = KdEncodeDataBlockAddress + 49;
    KiWaitAlwaysAddress = ( ULONG64 )( KiWaitAlwaysAddress + 4 + ( LONG64 )*( LONG32* )( KiWaitAlwaysAddress ) );

    /*
    DbgKdMmCopyMemory( &KdDebuggerDataBlock,
                       KdDebuggerDataBlockAddress,
                       sizeof( KDDEBUGGER_DATA64 ),
                       MM_COPY_MEMORY_VIRTUAL,
                       NULL );
    */
    RtlCopyMemory( &KdDebuggerDataBlock,
        ( PVOID )KdDebuggerDataBlockAddress,
                   sizeof( KDDEBUGGER_DATA64 ) );

    KiWaitAlways = *( PULONG64 )KiWaitAlwaysAddress;
    KiWaitNever = *( PULONG64 )KiWaitNeverAddress;
#endif

    //
    // DbgKdpDecodeDataBlock
    //

    //if ( DbgKdRead8( ( UCHAR* )KdpDataBlockEncodedAddress ) ) {
    if ( *( UCHAR* )KdpDataBlockEncodedAddress ) {


        Long = ( ULONG64* )&KdDebuggerDataBlock;
        Length = sizeof( KDDEBUGGER_DATA64 ) / sizeof( ULONG64 );

        while ( Length ) {

            *Long = KiWaitAlways ^
                _byteswap_uint64( KdpDataBlockEncodedAddress ^
                                  _rotl64( *Long ^ KiWaitNever, ( int )KiWaitNever ) );
            Long++;
            Length--;
        }
    }

    //
    // ASSUME: assumes that the guest can read our memory just fine, 
    // which should be the case really -- if necessary, a catch can be added
    // inside KdpReadVirtualMemory. We could tag root addresses using the sign
    // extended upper 16 bits
    //

    InitializeListHead( &KdpDebuggerDataListHead );
    InsertTailList( &KdpDebuggerDataListHead, ( PLIST_ENTRY )&KdDebuggerDataBlock.Header.List );

    KdDebuggerDataBlock.KernBase = ( ULONG64 )ImageBase;

    KdVersionBlock.Flags |= 1;

    KdVersionBlock.DebuggerDataList = ( ULONG64 )&KdpDebuggerDataListHead;
    KdVersionBlock.PsLoadedModuleList = KdDebuggerDataBlock.PsLoadedModuleList;
    KdVersionBlock.KernBase = KdDebuggerDataBlock.KernBase;

    NT_ASSERT( KdDebuggerDataBlock.Header.OwnerTag == 'GBDK' );
    NT_ASSERT( KdDebuggerDataBlock.Header.Size == 0x380 );

    //
    // a nigga's gotta say :raised_back_of_hand::skin-tone-4:
    //

    //KdPrint( KD_STARTUP_SIG );

    //
    // TODO: Should require a page fault hook.
    //

    KdVmwRpcInitialize( );
    KdTryConnect( );
    KdDebuggerNotPresent_ = FALSE;

    return STATUS_SUCCESS;
}

VOID
KdDriverUnload(

)
{

}
