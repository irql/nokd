
#include <kd.h>
#include <ntstrsafe.h>

CHAR KdpPathBuffer[ 0x1000 ];

//
// This function is just designed to be a re-implementation
// of the real thing inside ntoskrnl, it's sent to tell
// the debugger to load a module and it's symbols.
// i.e. it appears from the "lm" command. 
//

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
KdReportLoadSymbolsStateChange(
    _In_    PSTRING         PathName,
    _In_    PKD_SYMBOL_INFO Symbol,
    _In_    BOOLEAN         Unload
)
{
    return ( BOOLEAN )KdNmiServiceBp( KdServiceStateChangeSymbol,
        ( PVOID )PathName,
                                      ( PVOID )Symbol,
                                      ( PVOID )Unload,
                                      NULL );
}

//
// This purely exists for us to load any module 
// on demand, because of the way the DebugService works
// a CONTEXT structure is always guarenteed to be generated.
// This function deals with that nasty bit for us, and loads
// a module by it's image name, this means the debugger
// will have invalid image paths, because they will be
// using the image name instead. It also allows us to
// set a custom prefix for our module, because the real "ImageName"
// which is passed to the debugger is actually used to name it's 
// symbols & other misc shit.
//
// ImageName - The name used by the debugger for the module. ie nt!xyz, 
//             win32k!xyz, etc. Use the .sys extension on all names,
//             windbg is buggy without it, it's removed from the symbol
//             name.
//
// ImagePath - Image name used to reference it in the current system via
//             NtQuerySystemInformation.
//

NTSTATUS
KdReportLoaded(
    _In_ PCHAR ImageName,
    _In_ PCHAR ImagePath
)
{
    //
    // TODO: This should probably do something
    //       similar to DebugService, in the sense that
    //       we need a proper CONTEXT structure, in case
    //       the debugger breaks-in. Switch processor
    //       will probably crash the system in such
    //       a state.
    //

    NTSTATUS       ntStatus;
    KD_SYMBOL_INFO SymbolInfo;
    STRING         PathName;

    ntStatus = KdImageAddress( ImagePath,
        ( PVOID* )&SymbolInfo.BaseAddress,
                               ( ULONG* )&SymbolInfo.SizeOfImage );
    if ( !NT_SUCCESS( ntStatus ) ) {

        return ntStatus;
    }

    SymbolInfo.ProcessId = 0x4;
    SymbolInfo.CheckSum = *( ULONG32* )( ( PUCHAR )( SymbolInfo.BaseAddress + *( ULONG32* )( ( PCHAR )SymbolInfo.BaseAddress + 0x3c ) ) + 0x40 );

    RtlInitString( &PathName, ImageName );

    return KdReportLoadSymbolsStateChange( &PathName,
                                           &SymbolInfo,
                                           FALSE ) ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
}

//
// This is an implementation of KdpPrintString in ntoskrnl, similarly 
// KdpPromptString is implemented using the same method, however it uses 0x3231
// for an ApiNumber, and then waits for the debugger to send a packet in response,
// this is the same packet type; KdTypePrint. 
//
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
    Print.Processor = ( USHORT )KeGetCurrentProcessorNumber( );
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

//
// Small wrapper around KdpPrintString, in-order to format
// strings printf-style.
//
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
    STRING Head;
    DBGKD_WAIT_STATE_CHANGE State = { 0 };

    KdpSetCommonState( 0x3030, ExceptContext, &State );
    RtlCopyMemory( &State.u.Exception.ExceptionRecord,
                   ExceptRecord,
                   sizeof( EXCEPTION_RECORD64 ) );
    State.u.Exception.FirstChance = !SecondChance;
    KdpSetContextState( &State, ExceptContext );

    Head.Length = sizeof( DBGKD_WAIT_STATE_CHANGE );
    Head.MaximumLength = sizeof( DBGKD_WAIT_STATE_CHANGE );
    Head.Buffer = ( PCHAR )&State;

    return KdpSendWaitContinue( D3DNIG_NONE,
                                &Head,
                                NULL,
                                ExceptContext );
}
