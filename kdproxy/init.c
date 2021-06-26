

#include "kd.h"

BOOLEAN( *KeFreezeExecution )(

    );
BOOLEAN( *KeThawExecution )(
    _In_ BOOLEAN EnableInterrupts
    );

ULONG64( *KeSwitchFrozenProcessor )(
    _In_ ULONG32 Number
    );

PVOID( *MmGetPagedPoolCommitPointer )(

    );

NTSTATUS( *KdpSysReadControlSpace )(
    _In_  ULONG  Processor,
    _In_  ULONG  Address,
    _In_  PVOID  Buffer,
    _In_  ULONG  Length,
    _Out_ PULONG TransferLength
    );

NTSTATUS( *KdpSysWriteControlSpace )(
    _In_  ULONG  Processor,
    _In_  ULONG  Address,
    _In_  PVOID  Buffer,
    _In_  ULONG  Length,
    _Out_ PULONG TransferLength
    );

PUSHORT KeProcessorLevel;

//
// This function enables the TraceFlag if requested,
// and sets the dr7 on all processors in the KiProcessorBlock
//
VOID( *KdpGetStateChange )(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PCONTEXT                  Context
    );

PBOOLEAN                    KdPitchDebugger;
PULONG                      KdpDebugRoutineSelect;

BOOLEAN                     KdDebuggerNotPresent_;
BOOLEAN                     KdEnteredDebugger;
KTIMER                      KdBreakTimer;
KDPC                        KdBreakDpc;

KDDEBUGGER_DATA64           KdDebuggerDataBlock;
DBGKD_GET_VERSION64         KdVersionBlock = { 0, 0, 6, 2, 0x46, 0x8664, 0x0C, 0, 0, 0, 0, 0, 0, 0 };
LIST_ENTRY                  KdpDebuggerDataListHead;
ULONG64                     KdpLoaderDebuggerBlock = 0;
KD_CONTEXT                  KdpContext;

DECLSPEC_IMPORT PLIST_ENTRY PsLoadedModuleList;

VOID
KdBreakTimerDpcCallback(
    _In_     PKDPC Dpc,
    _In_opt_ PVOID DeferredContext,
    _In_opt_ PVOID SystemArgument1,
    _In_opt_ PVOID SystemArgument2
)
{
    Dpc;
    DeferredContext;
    SystemArgument1;
    SystemArgument2;

    //
    // Inside ... -> KeClockInterruptNotify -> KiUpdateRunTime -> KeAccumulateTicks
    //
    // if ( ( KdDebuggerEnabled || KdEventLoggingEnabled ) && KiPollSlot == Prcb->Number ) {
    //   
    //     KdCheckForDebugBreak( );
    // }
    //
    // VOID 
    // KdCheckForDebugBreak( 
    //
    // )
    // {
    //
    //     if ( !KdPitchDebugger && KdDebuggerEnabled || KdEventLoggingEnabled ) {
    // 
    //         if ( KdPollBreakIn( ) ) {
    //
    //             DbgBreakPointWithStatus( 1u );
    //         }
    //     }
    // }
    //
    //

    KeCancelTimer( &KdBreakTimer );

    KIRQL                   PreviousIrql;
    //ULONG                   Interrupts;
    //STRING                  Head;
    //DBGKD_WAIT_STATE_CHANGE Change = { 0 };
    //CONTEXT                 ZeroContext = { 0 };
    //SIZE_T                  InstructionCount;

    KeRaiseIrql( DISPATCH_LEVEL, &PreviousIrql );
    //KeRaiseIrql( HIGH_LEVEL, &PreviousIrql );
    //Interrupts = __readeflags( ) & 0x200;
    //_disable( );

    DbgPrint( "Kd Query!\n" );

    if ( KdPollBreakIn( ) ) {

        DbgPrint( "Broke in!\n" );

        //Interrupts = KeFreezeExecution( );
#if 0
        Head.Length = sizeof( DBGKD_WAIT_STATE_CHANGE );
        Head.MaximumLength = 0;
        Head.Buffer = ( PCHAR )&Change;

        Change.ApiNumber = 0x3030;
        Change.CurrentThread = ( ULONG64 )KeGetCurrentThread( );
        Change.Processor = ( USHORT )KeGetCurrentProcessorNumber( );
        Change.ProgramCounter = ( ULONG64 )KdBreakTimerDpcCallback;
        Change.ProcessorLevel = *KeProcessorLevel;
        Change.u.Exception.FirstChance = FALSE;
        Change.u.Exception.ExceptionRecord.ExceptionCode = 0x80000003;
        Change.u.Exception.ExceptionRecord.ExceptionAddress = ( ULONG64 )KdBreakTimerDpcCallback;
        Change.ControlReport.SegCs = 0x10;
        Change.ControlReport.ReportFlags = 3;

        RtlZeroMemory( &Change.ControlReport, sizeof( DBGKD_CONTROL_REPORT ) );

        MmCopyMemory( Change.ControlReport.InstructionStream,
            ( PVOID )Change.ProgramCounter,
                      0x10,
                      MM_COPY_ADDRESS_VIRTUAL,
                      &InstructionCount );
        Change.ControlReport.InstructionCount = ( USHORT )InstructionCount;

        KdpSendWaitContinue( 0,
                             &Head,
                             NULL,
                             &ZeroContext );
#endif
        //KeThawExecution( ( BOOLEAN )Interrupts );
    }

    //__writeeflags( __readeflags( ) | Interrupts );
    KeLowerIrql( PreviousIrql );

}

VOID
KdDriverUnload(
    _In_ PDRIVER_OBJECT DriverObject
)
{
    DriverObject;

    KeCancelTimer( &KdBreakTimer );

}

NTSTATUS
KdDriverLoad(
    _In_ PDRIVER_OBJECT  DriverObject,
    _In_ PUNICODE_STRING RegistryPath
)
{
    RegistryPath;

    PVOID              ImageBase;
    ULONG              ImageSize;
    PVOID              TextSectionBase;
    ULONG              TextSectionSize;
    PVOID              PageKdSectionBase;
    ULONG              PageKdSectionSize;
    ULONG_PTR          KdChangeOptionAddress;
    UNICODE_STRING     KdChangeOptionName = RTL_CONSTANT_STRING( L"KdChangeOption" );
    ULONG_PTR          KdTrapAddress;
    ULONG_PTR          KeFreezeExecutionAddress;
    ULONG_PTR          KeThawExecutionAddress;
    ULONG_PTR          KeSwitchFrozenProcessorAddress;
    ULONG_PTR          MmGetPagedPoolCommitPointerAddress;
    ULONG_PTR          KdCopyDataBlockAddress;
    ULONG_PTR          KdpSysReadControlSpaceAddress;
    ULONG_PTR          KdpSysWriteControlSpaceAddress;
    ULONG_PTR          KdpGetStateChangeAddress;
    ULONG_PTR          KeProcessorLevelAddress;
    PKDDEBUGGER_DATA64 KdDebuggerDataBlockDefault;
    LARGE_INTEGER      DueTime;

    //
    //////////////////////////////////////////////////////////////////////
    //      ALL NOTES ON KD DETECTION VECTORS AND KD RELATED SHIT.      //
    //////////////////////////////////////////////////////////////////////
    //

    DriverObject->DriverUnload = KdDriverUnload;

    ImageBase = NULL;
    ImageSize = 0;

    TextSectionBase = NULL;
    TextSectionSize = 0;

    PageKdSectionBase = NULL;
    PageKdSectionSize = 0;

    DbgPrint( "KdImageAddress returned %lx %p %lx\n",
              KdImageAddress( "ntoskrnl.exe", &ImageBase, &ImageSize ),
              ImageBase,
              ImageSize );

    DbgPrint( "KdImageSection returned %lx %p %lx\n",
              KdImageSection( ImageBase, ".text\0\0", &TextSectionBase, &TextSectionSize ),
              TextSectionBase,
              TextSectionSize );

    DbgPrint( "KdImageSection returned %lx %p %lx\n",
              KdImageSection( ImageBase, "PAGEKD\0", &PageKdSectionBase, &PageKdSectionSize ),
              PageKdSectionBase,
              PageKdSectionSize );

    KdChangeOptionAddress = ( ULONG_PTR )MmGetSystemRoutineAddress( &KdChangeOptionName );

    //                              KdChangeOption proc near
    //  0:  45 33 d2                xor    r10d, r10d
    //  3 : 44 38 15 xx xx xx xx    cmp    byte ptr [rip+KdPitchDebugger], r10b
    KdPitchDebugger = ( PBOOLEAN )( KdChangeOptionAddress + 10 + *( LONG32* )( KdChangeOptionAddress + 6 ) );

    DbgPrint( "KdPitchDebugger at %p with value %d\n", KdPitchDebugger, *KdPitchDebugger );

    KdTrapAddress = ( ULONG_PTR )KdSearchSignature( ( PVOID )( ( ULONG_PTR )ImageBase + ( ULONG_PTR )TextSectionBase ),
                                                    TextSectionSize,
                                                    "48 83 EC 38 83 3D ? ? ? ? ? 8A 44 24 68" );

    if ( KdTrapAddress == 0 ) {

        DbgPrint( "KdTrap not found\n" );
        return STATUS_UNSUCCESSFUL;
    }

    KdpDebugRoutineSelect = ( PULONG )( KdTrapAddress + 11 + *( LONG32* )( KdTrapAddress + 6 ) );

    DbgPrint( "KdTrapAddress at %p, KdpDebugRoutineSelect at %p with value %d\n", KdTrapAddress, KdpDebugRoutineSelect, *KdpDebugRoutineSelect );

    KeFreezeExecutionAddress = ( ULONG_PTR )KdSearchSignature( ( PVOID )( ( ULONG_PTR )ImageBase + ( ULONG_PTR )PageKdSectionBase ),
                                                               PageKdSectionSize,
                                                               "E8 ? ? ? ? 44 8A F0 48 8B 05 ? ? ? ?" );

    if ( KeFreezeExecutionAddress == 0 ) {

        DbgPrint( "KeFreezeExecution not found\n" );
        return STATUS_UNSUCCESSFUL;
    }

    KeFreezeExecution = ( PVOID )( KeFreezeExecutionAddress + 5 + *( LONG32* )( KeFreezeExecutionAddress + 1 ) );

    DbgPrint( "KeFreezeExecution at %p\n", KeFreezeExecution );

    KeThawExecutionAddress = ( ULONG_PTR )KdSearchSignature( ( PVOID )( ( ULONG_PTR )ImageBase + ( ULONG_PTR )PageKdSectionBase ),
                                                             PageKdSectionSize,
                                                             "E8 ? ? ? ? 48 83 3D ? ? ? ? ? 74 4B" );

    if ( KeThawExecutionAddress == 0 ) {

        DbgPrint( "KeThawExecution not found\n" );
        return STATUS_UNSUCCESSFUL;
    }

    KeThawExecution = ( PVOID )( KeThawExecutionAddress + 5 + *( LONG32* )( KeThawExecutionAddress + 1 ) );

    DbgPrint( "KeThawExecution at %p\n", KeThawExecution );

    KeSwitchFrozenProcessorAddress = ( ULONG_PTR )KdSearchSignature( ( PVOID )( ( ULONG_PTR )ImageBase + ( ULONG_PTR )TextSectionBase ),
                                                                     TextSectionSize,
                                                                     "40 53 48 83 EC 20 8B D9 B9 ? ? ? ? E8 ? ? ? ? 3B D8 0F 83 ? ? ? ? 80 3D ? ? ? ? ? 0F 85 ? ? ? ? 0F AE E8 48 8D 0D ? ? ? ? " );

    if ( KeSwitchFrozenProcessorAddress == 0 ) {

        DbgPrint( "KeSwitchFrozenProcessor not found\n" );
        return STATUS_UNSUCCESSFUL;
    }

    //E8 ? ? ? ? E9 ? ? ? ? FD 
    //KeSwitchFrozenProcessor = ( PVOID )( KeSwitchFrozenProcessorAddress + 5 + *( LONG32* )( KeSwitchFrozenProcessorAddress + 1 ) );
    KeSwitchFrozenProcessor = ( PVOID )( KeSwitchFrozenProcessorAddress );

    DbgPrint( "KeSwitchFrozenProcessor at %p\n", KeSwitchFrozenProcessor );

    MmGetPagedPoolCommitPointerAddress = ( ULONG_PTR )KdSearchSignature( ( PVOID )( ( ULONG_PTR )ImageBase + ( ULONG_PTR )PageKdSectionBase ),
                                                                         PageKdSectionSize,
                                                                         "E8 ? ? ? ? 48 89 05 ? ? ? ? 48 8D 3D ? ? ? ?" );
    //"E8 ? ? ? ? 48 89 05 ? ? ? ? 48 8D 1D ? ? ? ? 48 8D 05 ? ? ? ? " ); TextSec

    if ( MmGetPagedPoolCommitPointerAddress == 0 ) {

        DbgPrint( "MmGetPagedPoolCommitPointer not found\n" );
        return STATUS_UNSUCCESSFUL;
    }

    MmGetPagedPoolCommitPointer = ( PVOID )( MmGetPagedPoolCommitPointerAddress + 5 + *( LONG32* )( MmGetPagedPoolCommitPointerAddress + 1 ) );

    DbgPrint( "MmGetPagedPoolCommitPointer: %p\n", MmGetPagedPoolCommitPointer );

    KdCopyDataBlockAddress = ( ULONG_PTR )KdSearchSignature( ( PVOID )( ( ULONG_PTR )ImageBase + ( ULONG_PTR )TextSectionBase ),
                                                             TextSectionSize,
                                                             "80 3D ? ? ? ? ? 4C 8D 05 ? ? ? ?" );

    if ( KdCopyDataBlockAddress == 0 ) {

        DbgPrint( "KdCopyDataBlock not found\n" );
        return STATUS_UNSUCCESSFUL;
    }

    KdDebuggerDataBlockDefault = ( PKDDEBUGGER_DATA64 )( KdCopyDataBlockAddress + 14 + *( LONG32* )( KdCopyDataBlockAddress + 10 ) );

    DbgPrint( "KdDebuggerDataBlock at %p\n", KdDebuggerDataBlockDefault );

    KdpSysReadControlSpaceAddress = ( ULONG_PTR )KdSearchSignature( ( PVOID )( ( ULONG_PTR )ImageBase + ( ULONG_PTR )PageKdSectionBase ),
                                                                    PageKdSectionSize,
                                                                    "E8 ? ? ? ? 39 5C 24 60" );
    if ( KdpSysReadControlSpaceAddress == 0 ) {

        DbgPrint( "KdpSysReadControlSpace not found\n" );
        return STATUS_UNSUCCESSFUL;
    }

    KdpSysReadControlSpace = ( PVOID )( KdpSysReadControlSpaceAddress + 5 + *( LONG32* )( KdpSysReadControlSpaceAddress + 1 ) );

    DbgPrint( "KdpSysReadControlSpace: %p\n", KdpSysReadControlSpace );

    KdpSysWriteControlSpaceAddress = ( ULONG_PTR )KdSearchSignature( ( PVOID )( ( ULONG_PTR )ImageBase + ( ULONG_PTR )PageKdSectionBase ),
                                                                     PageKdSectionSize,
                                                                     "E8 ? ? ? ? 89 43 08 4C 8D 0D ? ? ? ? 8B 44 24 60 48 8D 54 24 ? 4C 8B C7" );
    if ( KdpSysWriteControlSpaceAddress == 0 ) {

        DbgPrint( "KdpSysWriteControlSpace not found\n" );
        return STATUS_UNSUCCESSFUL;
    }

    KdpSysWriteControlSpace = ( PVOID )( KdpSysWriteControlSpaceAddress + 5 + *( LONG32* )( KdpSysWriteControlSpaceAddress + 1 ) );

    DbgPrint( "KdpSysWriteControlSpace: %p\n", KdpSysWriteControlSpace );

    KdpGetStateChangeAddress = ( ULONG_PTR )KdSearchSignature( ( PVOID )( ( ULONG_PTR )ImageBase + ( ULONG_PTR )PageKdSectionBase ),
                                                               PageKdSectionSize,
                                                               "40 53 48 83 EC 20 83 79 10 00" );
    if ( KdpGetStateChangeAddress == 0 ) {

        DbgPrint( "KdpGetStateChange not found\n" );
        return STATUS_UNSUCCESSFUL;
    }

    KdpGetStateChange = ( PVOID )KdpGetStateChangeAddress;

    DbgPrint( "KdpGetStateChange: %p\n", KdpGetStateChange );

    KeProcessorLevelAddress = ( ULONG_PTR )KdSearchSignature( ( PVOID )( ( ULONG_PTR )ImageBase + ( ULONG_PTR )TextSectionBase ),
                                                              TextSectionSize,
                                                              "66 89 01 0F B7 05 ? ? ? ? 66 89 41 02" );
    //"0F B7 05 ? ? ? ? 49 8B D8" );

    if ( KeProcessorLevelAddress == 0 ) {

        DbgPrint( "KeProcessorLevel not found\n" );
        return STATUS_UNSUCCESSFUL;
    }

    KeProcessorLevel = ( PUSHORT )( KeProcessorLevelAddress + 10 + *( LONG32* )( KeProcessorLevelAddress + 6 ) );

    DbgPrint( "KeProcessorLevel: %p\n", KeProcessorLevel );

    if ( !NT_SUCCESS( KdUart16550Initialize( &KdDebugDevice ) ) ) {

        return STATUS_UNSUCCESSFUL;
    }
    else {

        DbgPrint( "KdDebugDevice initialized to UART16550 #%d\n", KdDebugDevice.Uart.Index );
    }

    //*KdpDebugRoutineSelect = 0;

    KdEnteredDebugger = FALSE;
    KdDebuggerNotPresent_ = FALSE;

    RtlCopyMemory( &KdDebuggerDataBlock,
                   KdDebuggerDataBlockDefault,
                   sizeof( KDDEBUGGER_DATA64 ) );

    KdDebuggerDataBlock.Header.OwnerTag = 'GBDK';
    KdDebuggerDataBlock.Header.Size = sizeof( KDDEBUGGER_DATA64 );

    InitializeListHead( &KdpDebuggerDataListHead );
    InsertTailList( &KdpDebuggerDataListHead, ( PLIST_ENTRY )&KdDebuggerDataBlock.Header.List );

    KdDebuggerDataBlock.MmPagedPoolCommit = ( ULONG64 )MmGetPagedPoolCommitPointer( );
    KdDebuggerDataBlock.KeLoaderBlock = ( ULONG64 )&KdpLoaderDebuggerBlock;

    KdVersionBlock.MajorVersion = ( USHORT )SharedUserData->NtMajorVersion;
    KdVersionBlock.MinorVersion = ( USHORT )SharedUserData->NtMinorVersion;
    //KdVersionBlock.MaxStateChange = 3;
    //KdVersionBlock.MaxManipulate = 0x33;
    *( USHORT* )&KdVersionBlock.MaxStateChange = 0x3303;
    KdVersionBlock.Flags |= 1;

    KdVersionBlock.DebuggerDataList = ( ULONG64 )&KdpDebuggerDataListHead;
    KdVersionBlock.PsLoadedModuleList = ( ULONG64 )PsLoadedModuleList;
    KdVersionBlock.KernBase = ( ULONG64 )ImageBase;

    // TODO: Initialize KdpContext.

    KIRQL x = KeRaiseIrqlToDpcLevel( );
    KdPollBreakIn( );
    KeLowerIrql( x );

    DbgPrint( "Polled break in!\n" );

    KeInitializeDpc( &KdBreakDpc,
        ( PKDEFERRED_ROUTINE )KdBreakTimerDpcCallback,
                     NULL );

    KeInitializeTimerEx( &KdBreakTimer, NotificationTimer );

    DueTime.QuadPart = -10000000;
    //KeSetTimerEx( &KdBreakTimer, DueTime, 1000, &KdBreakDpc );

    return STATUS_SUCCESS;
}
