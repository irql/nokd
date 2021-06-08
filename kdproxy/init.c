

#include "kd.h"

BOOLEAN( *KeFreezeExecution )(

    );
BOOLEAN( *KeThawExecution )(
    _In_ BOOLEAN Interrupts
    );

PBOOLEAN KdPitchDebugger;
PULONG   KdpDebugRoutineSelect;
KTIMER   KdBreakTimer;
KDPC     KdBreakDpc;

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

    KIRQL PreviousIrql;
    ULONG Interrupts;

    KeRaiseIrql( HIGH_LEVEL, &PreviousIrql );
    Interrupts = __readeflags( ) & 0x200;
    _disable( );

    __writeeflags( __readeflags( ) | Interrupts );
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

    PVOID          ImageBase;
    ULONG          ImageSize;
    PVOID          SectionBase;
    ULONG          SectionSize;
    ULONG_PTR      KdChangeOptionAddress;
    UNICODE_STRING KdChangeOptionName = RTL_CONSTANT_STRING( L"KdChangeOption" );
    ULONG_PTR      KdTrapAddress;
    ULONG_PTR      KeFreezeExecutionAddress;
    ULONG_PTR      KeThawExecutionAddress;
    LARGE_INTEGER  DueTime;

    //
    //////////////////////////////////////////////////////////////////////
    //      ALL NOTES ON KD DETECTION VECTORS AND KD RELATED SHIT.      //
    //////////////////////////////////////////////////////////////////////
    //
    // 1. Prcb->DebuggerSavedIRQL is never restored to 0, if KdEnterDebugger is called
    //    this field is set, and never zeroed.
    // 2. (HalPrivateDispatchTable->Fn[102]) xHalTimerWatchdogStop .data pointer is called 
    //    inside KdEnterDebugger and could be hooked.
    // 3. hypervisors could easily vmexit on any privileged instruction inside any Kd function
    //    and detect the presence of a debugger.
    // 4. KdpSendWaitContinue is the main loop for Kd, the main cross references for this are
    //    KdpReportCommandStringStateChange, 
    //    KdpReportExceptionStateChange, 
    //    KdpReportLoadSymbolsStateChange.
    // 5. Main notable reference for breakpoint handling is all references to KdTrap, this 
    //    eventually enters the debugger from KdpReportExceptionStateChange, if KdpDebugRoutineSelect is
    //    FALSE, then there are checks which may cause an issue with bp's because of enable checks because
    //    of KdpStub instead of KdpTrap, this var is controlled by Enable/Disable kd routines.
    //
    //
    // 1. KdpSendWaitContinue is only ever called under certain conditions, one of the issues with 
    //    this function, is that everything goes through it, and we are very capable of hooking it
    //    inside our dpc procedure, the main issue is with breakpoints and the KdpBreakpointTable.
    //    we need to figure some method to get around this table being checked by drivers, without
    //    losing the functionality.
    // 2. All Kd global vars indicate the presence of Kd, these variables can be discarded until a 
    //    breakpoint or something fires.
    // 3. Hooking KdpSendWaitContinue also fixes the problem of Kd being disabled when interrupt from
    //    somewhere away from our loop.
    // 4. Current thoughts are to use KdpDebugRoutineSelect to force KdpStub, and have the debugger disabled,
    //    such that no anti-debugger bp's are picked up. KdPitchDebugger can instantly cause it to ignore.
    // 5. When kd is not enabled by the bcd, kd.dll is loaded solely, when bcd instructs that there is
    //    another requested method for communication of kd and kd is enabled, kdcom.dll will be loaded and kd.dll
    //    no. My current idea is to change the name from kdcom.dll to kd.dll. TODO: verify the ntoskrnl imports from
    //    kdcom.dll and how they behave under different circumstances.

    DriverObject->DriverUnload = KdDriverUnload;

    ImageBase = NULL;
    ImageSize = 0;

    SectionBase = NULL;
    SectionSize = 0;

    DbgPrint( "KdImageAddress returned %lx %p %lx\n",
              KdImageAddress( "ntoskrnl.exe", &ImageBase, &ImageSize ),
              ImageBase,
              ImageSize );

    DbgPrint( "KdImageSection returned %lx %p %lx\n",
              KdImageSection( ImageBase, ".text\0\0", &SectionBase, &SectionSize ),
              SectionBase,
              SectionSize );

    KdChangeOptionAddress = ( ULONG_PTR )MmGetSystemRoutineAddress( &KdChangeOptionName );

    //                              KdChangeOption proc near
    //  0:  45 33 d2                xor    r10d, r10d
    //  3 : 44 38 15 xx xx xx xx    cmp    byte ptr [rip+KdPitchDebugger], r10b
    KdPitchDebugger = ( PBOOLEAN )( KdChangeOptionAddress + 10 + *( ULONG32* )( KdChangeOptionAddress + 6 ) );

    DbgPrint( "KdPitchDebugger at %p with value %d\n", KdPitchDebugger, *KdPitchDebugger );


    KdTrapAddress = ( ULONG_PTR )KdSearchSignature( ( PVOID )( ( ULONG_PTR )ImageBase + ( ULONG_PTR )SectionBase ),
                                                    SectionSize,
                                                    "48 83 EC 38 83 3D ? ? ? ? ? 8A 44 24 68" );

    KdpDebugRoutineSelect = ( PULONG )( KdTrapAddress + 11 + *( ULONG32* )( KdTrapAddress + 6 ) );

    DbgPrint( "KdTrapAddress at %p, KdpDebugRoutineSelect at %p with value %d\n", KdTrapAddress, KdpDebugRoutineSelect, *KdpDebugRoutineSelect );

    KeFreezeExecutionAddress = ( ULONG_PTR )KdSearchSignature( ( PVOID )( ( ULONG_PTR )ImageBase + ( ULONG_PTR )SectionBase ),
                                                               SectionSize,
                                                               "E8 ? ? ? ? 44 8A F0 48 8B 05 ? ? ? ?" );

    KeFreezeExecution = ( PVOID )( KeFreezeExecutionAddress + 5 + *( ULONG32* )( KeFreezeExecutionAddress + 1 ) );

    DbgPrint( "KeFreezeExecution at %p\n", KeFreezeExecution );

    KeThawExecutionAddress = ( ULONG_PTR )KdSearchSignature( ( PVOID )( ( ULONG_PTR )ImageBase + ( ULONG_PTR )SectionBase ),
                                                             SectionSize,
                                                             "E8 ? ? ? ? 48 83 3D ? ? ? ? ? 74 4B" );

    KeThawExecution = ( PVOID )( KeThawExecutionAddress + 5 + *( ULONG32* )( KeThawExecutionAddress + 1 ) );

    DbgPrint( "KeThawExecution at %p\n", KeThawExecution );

    KeInitializeDpc( &KdBreakDpc,
        ( PKDEFERRED_ROUTINE )KdBreakTimerDpcCallback,
                     NULL );

    KeInitializeTimerEx( &KdBreakTimer, NotificationTimer );

    DueTime.QuadPart = -10000000;
    KeSetTimerEx( &KdBreakTimer, DueTime, 1000, &KdBreakDpc );

    *KdpDebugRoutineSelect = 0;


    return STATUS_SUCCESS;
}
