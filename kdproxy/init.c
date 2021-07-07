

#include <kd.h>
#include <uart.h>
#include <vmwrpc.h>
#include <pt.h>

#if KD_DEBUG_NO_FREEZE 

HANDLE KdDebugThread;

#endif

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
    _In_  ULONG   Processor,
    _In_  ULONG64 Address,
    _In_  PVOID   Buffer,
    _In_  ULONG   Length,
    _Out_ PULONG  TransferLength
    );

NTSTATUS( *KdpSysWriteControlSpace )(
    _In_  ULONG   Processor,
    _In_  ULONG64 Address,
    _In_  PVOID   Buffer,
    _In_  ULONG   Length,
    _Out_ PULONG  TransferLength
    );

PUSHORT KeProcessorLevel;

//
// This function enables the TraceFlag if requested,
// and sets the dr7 on all processors in the KiProcessorBlock
//
// After the realisation mentioned inside kdapi.c, this could 
// easily be re-written.
//
VOID( *KdpGetStateChange )(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PCONTEXT                  Context
    );

//
// Referenced inside KdpQueryMemory, to check if
// the address lies in SessionSpace versus SystemSpace.
//
BOOLEAN( *MmIsSessionAddress )(
    _In_ ULONG_PTR Address
    );

PBOOLEAN                    KdPitchDebugger;
PULONG                      KdpDebugRoutineSelect;

BOOLEAN                     KdDebuggerNotPresent_;
BOOLEAN                     KdEnteredDebugger;
KTIMER                      KdBreakTimer;
KDPC                        KdBreakDpc;

ULONG64( *KdDecodeDataBlock )(

    );

KDDEBUGGER_DATA64           KdDebuggerDataBlock = { 0 };

/* Taken from a debugging session with build 21354
   +0x000 MajorVersion     : 0xf
   +0x002 MinorVersion     : 0x536a
   +0x004 ProtocolVersion  : 0x6 ''
   +0x005 KdSecondaryVersion : 0x2 ''
   +0x006 Flags            : 0x47
   +0x008 MachineType      : 0x8664
   +0x00a MaxPacketType    : 0xc ''
   +0x00b MaxStateChange   : 0x3 ''
   +0x00c MaxManipulate    : 0x33 '3'
   +0x00d Simulation       : 0 ''
   +0x00e Unused           : [1] 0
   +0x010 KernBase         : 0xfffff800`2b807000
   +0x018 PsLoadedModuleList : 0xfffff800`2c4305a0
   +0x020 DebuggerDataList : 0xfffff800`2c4472d0
   More comments on this structure inside KdDriverLoad.
*/

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
ULONG64                     KdpLoaderDebuggerBlock = 0;
KD_CONTEXT                  KdpContext;

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

    LARGE_INTEGER           DueTime;

    //KdPrint( "KdBreakDpc executed on processor: %d\n", KeGetCurrentProcessorNumber( ) );

    if ( KdPollBreakIn( ) ) {

        KdDebugBreak( );
    }

    DueTime.QuadPart = -10000000;
    KeSetTimer( &KdBreakTimer, DueTime, &KdBreakDpc );
}

#if KD_DEBUG_NO_FREEZE
VOID
KdDebugThreadProcedure(

)
{

    STRING                  Head;
    DBGKD_WAIT_STATE_CHANGE Exception = { 0 };
    CONTEXT                 Context = { 0 };

    while ( !KdDebuggerNotPresent_ ) {

        if ( KdPollBreakIn( ) ) {

            DbgPrint( "KdDebugThreadProcedure broke in.\n" );

            //
            // This is extremely buggy, most of the time, because of the nature
            // of kd's context management & it's strong relationship with the prcb, 
            // freeze/thaw execution apis.
            //
#if 1
            Context.Rip = ( ULONG64 )KdDebuggerDataBlock.BreakpointWithStatus;
            Context.Rsp = 0;

            Context.EFlags = 2;
            Context.SegCs = ( USHORT )KdDebuggerDataBlock.GdtR0Code;
            Context.SegGs = ( USHORT )KdDebuggerDataBlock.GdtR0Data;
            Context.SegFs = ( USHORT )KdDebuggerDataBlock.GdtR0Data;
            Context.SegEs = ( USHORT )KdDebuggerDataBlock.GdtR0Data;
            Context.SegDs = ( USHORT )KdDebuggerDataBlock.GdtR0Data;
            Context.ContextFlags = CONTEXT_AMD64 | CONTEXT_INTEGER | CONTEXT_SEGMENTS;

            Head.Length = sizeof( DBGKD_WAIT_STATE_CHANGE );
            Head.MaximumLength = sizeof( DBGKD_WAIT_STATE_CHANGE );
            Head.Buffer = ( PCHAR )&Exception;

            KdpSetCommonState( 0x3030, &Context, &Exception );
            KdpSetContextState( &Exception, &Context );

            Exception.u.Exception.FirstChance = TRUE;
            Exception.u.Exception.ExceptionRecord.ExceptionCode = STATUS_BREAKPOINT;
            Exception.u.Exception.ExceptionRecord.ExceptionAddress = Context.Rip;

            KdpSendWaitContinue( 0,
                                 &Head,
                                 NULL,
                                 &Context );
#endif
            //
            // Treat as regular bp.
            //

            //KdBpContextStore( );
        }

        LARGE_INTEGER Time;
        Time.QuadPart = -10000000;
        KeDelayExecutionThread( KernelMode, FALSE, &Time );
    }

    PsTerminateSystemThread( STATUS_SUCCESS );
}
#endif

VOID
KdDriverUnload(
    _In_ PDRIVER_OBJECT DriverObject
)
{
    DriverObject;

#if KD_DEBUG_NO_FREEZE

    KdDebuggerNotPresent_ = TRUE;

    ZwWaitForSingleObject( KdDebugThread,
                           FALSE,
                           NULL );
    ZwClose( KdDebugThread );

#else
    KeCancelTimer( &KdBreakTimer );
#endif

    KdFreezeUnload( );

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
    PVOID              SectionTextBase;
    ULONG              SectionTextSize;
    PVOID              SectionPageKdBase;
    ULONG              SectionPageKdSize;
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
    ULONG_PTR          KdDecodeDataBlockAddress;
    ULONG_PTR          MmIsSessionAddressAddress;
    PKDDEBUGGER_DATA64 KdDebuggerDataBlockDefault;
#if !(KD_DEBUG_NO_FREEZE)
    LARGE_INTEGER      DueTime;
#endif

    //
    //////////////////////////////////////////////////////////////////////
    //      ALL NOTES ON KD DETECTION VECTORS AND KD RELATED SHIT.      //
    //////////////////////////////////////////////////////////////////////
    //

    DriverObject->DriverUnload = KdDriverUnload;

    ImageBase = NULL;
    ImageSize = 0;

    SectionTextBase = NULL;
    SectionTextSize = 0;

    SectionPageKdBase = NULL;
    SectionPageKdSize = 0;

    DbgPrint( "KdImageAddress returned %lx %p %lx\n",
              KdImageAddress( "ntoskrnl.exe", &ImageBase, &ImageSize ),
              ImageBase,
              ImageSize );

    DbgPrint( "KdImageSection returned %lx %p %lx\n",
              KdImageSection( ImageBase, ".text\0\0", &SectionTextBase, &SectionTextSize ),
              SectionTextBase,
              SectionTextSize );

    DbgPrint( "KdImageSection returned %lx %p %lx\n",
              KdImageSection( ImageBase, "PAGEKD\0", &SectionPageKdBase, &SectionPageKdSize ),
              SectionPageKdBase,
              SectionPageKdSize );

    KdChangeOptionAddress = ( ULONG_PTR )MmGetSystemRoutineAddress( &KdChangeOptionName );

    //                              KdChangeOption proc near
    //  0:  45 33 d2                xor    r10d, r10d
    //  3 : 44 38 15 xx xx xx xx    cmp    byte ptr [rip+KdPitchDebugger], r10b
    KdPitchDebugger = ( PBOOLEAN )( KdChangeOptionAddress + 10 + *( LONG32* )( KdChangeOptionAddress + 6 ) );

    DbgPrint( "KdPitchDebugger at %p with value %d\n", KdPitchDebugger, *KdPitchDebugger );

    KdTrapAddress = ( ULONG_PTR )KdSearchSignature( ( PVOID )( ( ULONG_PTR )ImageBase + ( ULONG_PTR )SectionTextBase ),
                                                    SectionTextSize,
                                                    "48 83 EC 38 83 3D ? ? ? ? ? 8A 44 24 68" );
    if ( KdTrapAddress == 0 ) {

        DbgPrint( "KdTrap not found\n" );
        return STATUS_UNSUCCESSFUL;
    }

    KdpDebugRoutineSelect = ( PULONG )( KdTrapAddress + 11 + *( LONG32* )( KdTrapAddress + 6 ) );

    DbgPrint( "KdTrapAddress at %p, KdpDebugRoutineSelect at %p with value %d\n", KdTrapAddress, KdpDebugRoutineSelect, *KdpDebugRoutineSelect );

    KeFreezeExecutionAddress = ( ULONG_PTR )KdSearchSignature( ( PVOID )( ( ULONG_PTR )ImageBase + ( ULONG_PTR )SectionPageKdBase ),
                                                               SectionPageKdSize,
                                                               "E8 ? ? ? ? 44 8A F0 48 8B 05 ? ? ? ?" );

    if ( KeFreezeExecutionAddress == 0 ) {

        DbgPrint( "KeFreezeExecution not found\n" );
        return STATUS_UNSUCCESSFUL;
    }

    KeFreezeExecution = ( PVOID )( KeFreezeExecutionAddress + 5 + *( LONG32* )( KeFreezeExecutionAddress + 1 ) );

    DbgPrint( "KeFreezeExecution at %p\n", KeFreezeExecution );

    KeThawExecutionAddress = ( ULONG_PTR )KdSearchSignature( ( PVOID )( ( ULONG_PTR )ImageBase + ( ULONG_PTR )SectionPageKdBase ),
                                                             SectionPageKdSize,
                                                             "E8 ? ? ? ? 48 83 3D ? ? ? ? ? 74 4B" );

    if ( KeThawExecutionAddress == 0 ) {

        DbgPrint( "KeThawExecution not found\n" );
        return STATUS_UNSUCCESSFUL;
    }

    KeThawExecution = ( PVOID )( KeThawExecutionAddress + 5 + *( LONG32* )( KeThawExecutionAddress + 1 ) );

    DbgPrint( "KeThawExecution at %p\n", KeThawExecution );

    KeSwitchFrozenProcessorAddress = ( ULONG_PTR )KdSearchSignature( ( PVOID )( ( ULONG_PTR )ImageBase + ( ULONG_PTR )SectionTextBase ),
                                                                     SectionTextSize,
                                                                     "40 53 48 83 EC 20 8B D9 B9 ? ? ? ? E8 ? ? ? ? 3B D8 0F 83 ? ? ? ? 80 3D ? ? ? ? ? 0F 85 ? ? ? ? 0F AE E8 48 8D 0D ? ? ? ? " );

    if ( KeSwitchFrozenProcessorAddress == 0 ) {

        DbgPrint( "KeSwitchFrozenProcessor not found\n" );
        return STATUS_UNSUCCESSFUL;
    }

    KeSwitchFrozenProcessor = ( PVOID )( KeSwitchFrozenProcessorAddress );

    DbgPrint( "KeSwitchFrozenProcessor at %p\n", KeSwitchFrozenProcessor );

    MmGetPagedPoolCommitPointerAddress = ( ULONG_PTR )KdSearchSignature( ( PVOID )( ( ULONG_PTR )ImageBase + ( ULONG_PTR )SectionPageKdBase ),
                                                                         SectionPageKdSize,
                                                                         "E8 ? ? ? ? 48 89 05 ? ? ? ? 48 8D 3D ? ? ? ?" );

    if ( MmGetPagedPoolCommitPointerAddress == 0 ) {

        DbgPrint( "MmGetPagedPoolCommitPointer not found\n" );
        return STATUS_UNSUCCESSFUL;
    }

    MmGetPagedPoolCommitPointer = ( PVOID )( MmGetPagedPoolCommitPointerAddress + 5 + *( LONG32* )( MmGetPagedPoolCommitPointerAddress + 1 ) );

    DbgPrint( "MmGetPagedPoolCommitPointer: %p\n", MmGetPagedPoolCommitPointer );

    KdCopyDataBlockAddress = ( ULONG_PTR )KdSearchSignature( ( PVOID )( ( ULONG_PTR )ImageBase + ( ULONG_PTR )SectionTextBase ),
                                                             SectionTextSize,
                                                             "80 3D ? ? ? ? ? 4C 8D 05 ? ? ? ?" );

    if ( KdCopyDataBlockAddress == 0 ) {

        DbgPrint( "KdCopyDataBlock not found\n" );
        return STATUS_UNSUCCESSFUL;
    }

    KdDebuggerDataBlockDefault = ( PKDDEBUGGER_DATA64 )( KdCopyDataBlockAddress + 14 + *( LONG32* )( KdCopyDataBlockAddress + 10 ) );

    DbgPrint( "KdDebuggerDataBlock at %p\n", KdDebuggerDataBlockDefault );

    KdpSysReadControlSpaceAddress = ( ULONG_PTR )KdSearchSignature( ( PVOID )( ( ULONG_PTR )ImageBase + ( ULONG_PTR )SectionPageKdBase ),
                                                                    SectionPageKdSize,
                                                                    "E8 ? ? ? ? 39 5C 24 60" );
    if ( KdpSysReadControlSpaceAddress == 0 ) {

        DbgPrint( "KdpSysReadControlSpace not found\n" );
        return STATUS_UNSUCCESSFUL;
    }

    KdpSysReadControlSpace = ( PVOID )( KdpSysReadControlSpaceAddress + 5 + *( LONG32* )( KdpSysReadControlSpaceAddress + 1 ) );

    DbgPrint( "KdpSysReadControlSpace: %p\n", KdpSysReadControlSpace );

    KdpSysWriteControlSpaceAddress = ( ULONG_PTR )KdSearchSignature( ( PVOID )( ( ULONG_PTR )ImageBase + ( ULONG_PTR )SectionPageKdBase ),
                                                                     SectionPageKdSize,
                                                                     "E8 ? ? ? ? 89 43 08 4C 8D 0D ? ? ? ? 8B 44 24 60 48 8D 54 24 ? 4C 8B C7" );
    if ( KdpSysWriteControlSpaceAddress == 0 ) {

        DbgPrint( "KdpSysWriteControlSpace not found\n" );
        return STATUS_UNSUCCESSFUL;
    }

    KdpSysWriteControlSpace = ( PVOID )( KdpSysWriteControlSpaceAddress + 5 + *( LONG32* )( KdpSysWriteControlSpaceAddress + 1 ) );

    DbgPrint( "KdpSysWriteControlSpace: %p\n", KdpSysWriteControlSpace );

    KdpGetStateChangeAddress = ( ULONG_PTR )KdSearchSignature( ( PVOID )( ( ULONG_PTR )ImageBase + ( ULONG_PTR )SectionPageKdBase ),
                                                               SectionPageKdSize,
                                                               "40 53 48 83 EC 20 83 79 10 00" );
    if ( KdpGetStateChangeAddress == 0 ) {

        DbgPrint( "KdpGetStateChange not found\n" );
        return STATUS_UNSUCCESSFUL;
    }

    KdpGetStateChange = ( PVOID )KdpGetStateChangeAddress;

    DbgPrint( "KdpGetStateChange: %p\n", KdpGetStateChange );

    KeProcessorLevelAddress = ( ULONG_PTR )KdSearchSignature( ( PVOID )( ( ULONG_PTR )ImageBase + ( ULONG_PTR )SectionTextBase ),
                                                              SectionTextSize,
                                                              "66 89 01 0F B7 05 ? ? ? ? 66 89 41 02" );

    if ( KeProcessorLevelAddress == 0 ) {

        DbgPrint( "KeProcessorLevel not found\n" );
        return STATUS_UNSUCCESSFUL;
    }

    KeProcessorLevel = ( PUSHORT )( KeProcessorLevelAddress + 10 + *( LONG32* )( KeProcessorLevelAddress + 6 ) );

    DbgPrint( "KeProcessorLevel: %p\n", KeProcessorLevel );

    KdDecodeDataBlockAddress = ( ULONG_PTR )KdSearchSignature( ( PVOID )( ( ULONG_PTR )ImageBase + ( ULONG_PTR )SectionTextBase ),
                                                               SectionTextSize,
                                                               "E8 ? ? ? ? 48 8B 45 88 4C 8D 9D ? ? ? ? " );

    if ( KdDecodeDataBlockAddress == 0 ) {

        DbgPrint( "KdDecodeDataBlock not found\n" );
        return STATUS_UNSUCCESSFUL;
    }

    KdDecodeDataBlock = ( PVOID )( KdDecodeDataBlockAddress + 5 + *( LONG32* )( KdDecodeDataBlockAddress + 1 ) );

    DbgPrint( "KdDecodeDataBlock: %p\n", KdDecodeDataBlock );

    MmIsSessionAddressAddress = ( ULONG_PTR )KdSearchSignature( ( PVOID )( ( ULONG_PTR )ImageBase + ( ULONG_PTR )SectionTextBase ),
                                                                SectionTextSize,
                                                                "48 2B 0D ? ? ? ? 33 C0" );

    if ( MmIsSessionAddressAddress == 0 ) {

        DbgPrint( "MmIsSessionAddress not found\n" );
        return STATUS_UNSUCCESSFUL;
    }

    MmIsSessionAddress = ( PVOID )( MmIsSessionAddressAddress );

#if KD_UART_ENABLED
    if ( !NT_SUCCESS( KdUartInitialize( &KdDebugDevice ) ) ) {

        return STATUS_UNSUCCESSFUL;
    }
    else {

        DbgPrint( "KdDebugDevice using UART port %d\n", KdDebugDevice.Uart.Index );
    }
#endif

    if ( !NT_SUCCESS( KdVmwRpcInitialize( ) ) ) {

        return STATUS_UNSUCCESSFUL;
    }
    else {

        DbgPrint( "KdDebugDevice using VMWare ERPC channel %d\n", KdDebugDevice.VmwRpc.Channel );
    }

    //
    // It's been a while since I've looked at KdTrap, but I believe if you wanted
    // to catch any exceptions and even proper breakpoints, you could use KdpDebugRoutineSelect
    // 
    // KdpDebugRoutineSelect switches between KdpTrap & KdpStub respectively.
    // 
    // KdpStub - Checks all global variables, if set accordingly, invokes KdpTrap
    // KdpTrap - directly invokes Kd with whatever was expected, checking if it's
    //           debug service, a real bp or an exception. Debug service is indicated
    //           by Record->ExceptionInformation[ 0 ], I don't remember what causes this
    //           to be set. KdpTrap invokves KdpReport on exception, which calls 
    //           KdpReportExceptionStateChange.
    //
    // The typical invocation of KdpReport/KdpSymbol/etc function goes as follows
    //
    // // TrapFrame is only null-checked in this function?
    // // If it's non-null then KdTimerStop & KdTimerDifference are set.
    // Interrupts = KdEnterDebugger( TrapFrame );
    // Prcb = KeGetCurrentPrcb( );
    // 
    // // Order doesn't matter.
    // KiSaveProcessorControlState( &Prcb->ProcessorState );
    // KdpCopyContext( Prcb->Context, Context->ContextFlags, Context );
    //
    // KdpReportXyz( ... );
    //
    // KdpCopyContext( Context, Prcb->Context->ContextFlags, Prcb->Context );
    // KiRestoreProcessorControlState( &Prcb->ProcessorState );
    //
    // KdExitDebugger( Interrupts );
    //

    //*KdpDebugRoutineSelect = 0;

    KdEnteredDebugger = FALSE;
    KdDebuggerNotPresent_ = FALSE;

    //
    // Inside nt!KeInitSystem, there is a call
    // to nt!KdEncodeDataBlock which means this variable
    // has almost garbage data, to fix this, we simply
    // call nt!KdDecodeDataBlock lol, what's the point 
    // in this ms?
    //
    // TODO: Call KdEncodeDataBlock, otherwise this leads a
    //       detection vector.
    //

    KdDecodeDataBlock( );

    RtlCopyMemory( &KdDebuggerDataBlock,
                   KdDebuggerDataBlockDefault,
                   sizeof( KDDEBUGGER_DATA64 ) );

    //
    // This initialization is performed by nt!KdRegisterDebuggerDataBlock usually
    // we initialize the head, insert the flink and setup the header, this function
    // also plays with a spinlock & other stuff, but this function is only
    // referenced at once initialization? 
    //
    // if ( KdpDebuggerDataListHead.Flink == NULL )  {
    //
    //     ...  
    // }
    //

    KdDebuggerDataBlock.Header.OwnerTag = 'GBDK';
    KdDebuggerDataBlock.Header.Size = sizeof( KDDEBUGGER_DATA64 );

    InitializeListHead( &KdpDebuggerDataListHead );
    InsertTailList( &KdpDebuggerDataListHead, ( PLIST_ENTRY )&KdDebuggerDataBlock.Header.List );

    //
    // nt!KdInitSystem initialization.
    //

    KdDebuggerDataBlock.MmPagedPoolCommit = ( ULONG64 )MmGetPagedPoolCommitPointer( );

    //
    // This field is set to KdpLoaderDebuggerBlock, which is freed before it's even possible to break-in,
    // KdpLoaderDebuggerBlock is zeroed at the end of KdInitSystem.
    //
    // TODO: Potentially, set the MaxBreakpoints and MaxWatchpoints, I'm not sure
    //       where they're referenced or used inside dbgeng, but might be more safe.
    //
    KdDebuggerDataBlock.KeLoaderBlock = ( ULONG64 )&KdpLoaderDebuggerBlock;

    KdDebuggerDataBlock.KernBase = ( ULONG64 )ImageBase;

    //
    //KdVersionBlock.MajorVersion = NtBuildNumber >> 28;
    //KdVersionBlock.MinorVersion = NtBuildNumber;
    //

    KdVersionBlock.Flags |= 1;

    KdVersionBlock.DebuggerDataList = ( ULONG64 )&KdpDebuggerDataListHead;
    KdVersionBlock.PsLoadedModuleList = KdDebuggerDataBlock.PsLoadedModuleList;
    KdVersionBlock.KernBase = KdDebuggerDataBlock.KernBase;

    //
    // When kd breaks in there are some reads which I've logged here in a list.
    // Kernel base: 0xFFFFF801`3B21C000
    //
    // KdDebuggerDataBlock.Header       sizeof(DBGKD_DEBUG_DATA_HEADER64)
    // KdDebuggerDataBlock              KdDebuggerDataBlock.Size
    // nt!NtBuildLabEx                  261
    // nt!PsLoadedModuleList            sizeof(LIST_ENTRY)
    // FFFFBA03`32E73050                136                                 ???????
    // FFFF8006`00402950                128                                 ???????
    // nt!ExpSystemProcessName          128                                 
    // nt!ExpSystemProcessName+0x110    128
    // nt!ExpSystemProcessName+0x190    128
    // nt!ExpSystemProcessName+0x210    128
    // nt!ExpSystemProcessName+0x290    1240
    // nt!load_config_used              264
    //
    // nt!_NULL_IMPORT_DESCRIPTOR <PERF> (nt+0x13f00c) 128
    // 
    //

    if ( !NT_SUCCESS( KdFreezeLoad( ) ) ) {

        return STATUS_UNSUCCESSFUL;
    }

    KdProcessorBlock = ( PKD_PROCESSOR )ExAllocatePoolWithTag( NonPagedPoolNx,
                                                               sizeof( KD_PROCESSOR ) *
                                                               KeQueryActiveProcessorCountEx( 0xFFFF ),
                                                               'dKoN' );

    KdPrint( "pte at: %p versus %p\n", MiGetPteAddress( ( ULONG_PTR )KdProcessorBlock ),
             &MiReferenceLevel2Entry( MiIndexLevel4( ( ( ULONG_PTR )KdProcessorBlock ) ),
                                      MiIndexLevel3( ( ( ULONG_PTR )KdProcessorBlock ) ),
                                      MiIndexLevel2( ( ( ULONG_PTR )KdProcessorBlock ) ) )
             [ MiIndexLevel1( ( ( ULONG_PTR )KdProcessorBlock ) ) ] );
    RtlZeroMemory( KdProcessorBlock,
                   sizeof( KD_PROCESSOR ) *
                   KeQueryActiveProcessorCountEx( 0xFFFF ) );

    //
    // Gotta print our big ass ascii text of "nokd" after
    // loading, and before allowing a break-in, this wouldn't
    // be a real project without it.
    //

    KdPrint( KD_STARTUP_SIG );

    //
    // If you set breakpoint on start, then you may break-in here.
    //

    KdReportLoaded( KD_SYMBOLIC_NAME ".sys", KD_FILE_NAME );
    //KdLoadSystem( );

#if KD_DEBUG_NO_FREEZE

    OBJECT_ATTRIBUTES Thread;

    InitializeObjectAttributes( &Thread,
                                NULL,
                                OBJ_KERNEL_HANDLE,
                                NULL,
                                NULL );

    PsCreateSystemThread( &KdDebugThread,
                          THREAD_ALL_ACCESS,
                          &Thread,
                          NULL,
                          NULL,
                          ( PKSTART_ROUTINE )KdDebugThreadProcedure,
                          NULL );

#else

    KeInitializeDpc( &KdBreakDpc,
        ( PKDEFERRED_ROUTINE )KdBreakTimerDpcCallback,
                     NULL );

    KeInitializeTimerEx( &KdBreakTimer, NotificationTimer );

    DueTime.QuadPart = -10000000;

    KeSetTimer( &KdBreakTimer, DueTime, &KdBreakDpc );

#endif

    return STATUS_SUCCESS;
}
