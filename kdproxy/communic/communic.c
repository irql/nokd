
#include <kd.h>

KD_DEBUG_DEVICE  KdDebugDevice;

VOLATILE ULONG32 KdpFrozenCount = 0;
VOLATILE ULONG32 KdpFreezeOwner;
VOLATILE BOOLEAN KdpFrozen = FALSE;
PVOID            KdpNmiHandle = NULL;

//
// This structure is exists to manage breakpoints &
// the trap flag.
//
PKD_PROCESSOR    KdProcessorBlock;

BOOLEAN
KdServiceInterrupt(
    _In_ PVOID   Context,
    _In_ BOOLEAN Handled
)
{
    Context;
    Handled;

    //
    // I'm sad because KiHandleNmi on windows 7 is super simple,
    // there's no spectre mitigation the interrupt handler is super short and simple
    // and it just calls all NMI handlers with no issue, in Windows 10
    // it's absolutely massive and disgusting to read through.
    //
    // TODO: The system has various watchdogs in place, which will cause a 
    //       bugcheck if we stay here for too long, HalpWatchdogCheckPreResetNMI
    //       is an example of this which is invoked by HalPrivateDispatchTable[125]
    //       -> HalpPreprocessNmi @ phase=1, it uses SharedUserData for timing 
    //       information we must update this information after completing the 
    //       callback. Something to note: there is a hyper-v built-in nmi 
    //       watchdog which can deal with this timing itself, this synthetic 
    //       timer is detected by:
    //       if ( *(unsigned int*)( HalpWatchdogTimer + 228 ) == 8 )
    //       in the future, we should remove flag 0x8 from this,
    //       because we can't spoof this timer.
    //
    //       Bugcheck Codes: 0x1CA, 0x1CF
    //
    // KiNmiInterruptStart is the handler on my system, although this and
    // KiNmiInterruptEnd are only markers for RECURSIVE_NMI detection,
    // these functions set up a KTRAP_FRAME and do various misc shit, that's
    // not relevant to us, however, recursive nmi's may be a problem, and 
    // they're detected by the following conditions:
    //
    // if ( Prcb->NmiActive ||
    //      ( TrapFrame.SegCs & 1 ) == 0 &&
    //      KiNmiInterruptStart <= TrapFrame.Rip &&
    //      KiNmiInterruptEnd > TrapFrame.Rip ||
    //      KiNmiInterruptShadow <= TrapFrame.Rip &&
    //      KiNmiInterruptShadowEnd > TrapFrame.Rip ) {
    //
    //      KiBugCheckDispatch( RECURSIVE_NMI, 0, 0, 0 );
    // }
    //
    // Just realised this is per-prcb, not per-system-state. Oh well.
    //

    ULONG_PTR          Prcb;
    ULONG              ProcessorNumber;
    PCONTEXT           TrapContext;
    EXCEPTION_RECORD64 ExceptRecord = { 0 };
    ULONG64            SmapFlag;
    ULONG64            WpFlag;
    PKTRAP_FRAME       TrapFrame = ( PKTRAP_FRAME )(
        ( ( ( ULONG64 )_AddressOfReturnAddress( ) + 0x1000 ) & ~0xFFF ) - sizeof( KTRAP_FRAME ) - 32 );

    WpFlag = __readcr0( ) & ( 1ULL << 16 );
    __writecr0( __readcr0( ) & ~( 1ULL << 16 ) );

    SmapFlag = __readcr4( ) & ( 1ULL << 21 );
    __writecr4( __readcr4( ) & ~( 1ULL << 21 ) );

    Prcb = KeGetCurrentPrcb( );
    ProcessorNumber = KdGetPrcbNumber( Prcb );
    TrapContext = KdGetPrcbContext( Prcb );

    //
    // Handle a tracepoint on the current processor, this is very
    // simple, a tracepoint in this system is just cd 02 being placed
    // after the previous instruction, although, as of writing this, 
    // it will not work for any branch instructions, gota fix that.
    //
    // Tracepoints also send a state change to the debugger,
    // the debugger will re-enable the breakpoint and we can continue.
    // They're also quite dangerous for multi-processor because another
    // processor could hit the cd 02 tracepoint, which would be weird
    // and could crash the system because of the instruction pointer
    // never being re-aligned due to KDPR_FLAG_TPE not being set.
    //

    if ( KdProcessorBlock[ ProcessorNumber ].Flags & KDPR_FLAG_TPE ) {

        KdPrint( "Tracepoint Processor nmi!\n" );
    }

    if ( KdProcessorBlock[ ProcessorNumber ].Flags & KDPR_FLAG_TPE &&
         KdProcessorBlock[ ProcessorNumber ].Tracepoint == TrapContext->Rip - KdpBreakpointCodeLength ) {

        RtlCopyMemory( ( void* )KdProcessorBlock[ ProcessorNumber ].Tracepoint,
            ( void* )KdProcessorBlock[ ProcessorNumber ].TracepointCode,
                       0x20 );

        TrapContext->Rip -= KdpBreakpointCodeLength;
        KdProcessorBlock[ ProcessorNumber ].Flags &= ~KDPR_FLAG_TPE;

        KeSweepLocalCaches( );
        KdPrint( "Tracepoint caught! %p\n", TrapContext->Rip );
    }

    if ( !KdpFrozen ) {

        //
        // KdEnterDebugger and KdExitDebugger deal with the
        // state of KdpFrozen and KdpFreezeOwner.
        // This means an int2 breakpoint was hit.
        //

        KdFreezeProcessors( );
        KdpFrozenCount = 1;
    }
    else {

        KdpFrozenCount++;
    }

    //
    // Make sure all processors are here & ready.
    //

    while ( KdpFrozenCount != KeQueryActiveProcessorCountEx( 0xFFFF ) ) {

        YieldProcessor( );
    }

    //
    // In the future, this could be optimized to halt any processors that arent
    // the KdpFreezeOwner, and then use another NMI to wake it up, but for now
    // we just hammer the processor & wait.
    //

    ExceptRecord.ExceptionAddress = TrapContext->Rip;
    ExceptRecord.ExceptionCode = STATUS_BREAKPOINT;

    while ( KdpFrozen ) {

        if ( KdpFreezeOwner == ProcessorNumber ) {

            // KD_SERVICE_BP_MAGIC inside nmibp.asm
            if ( TrapContext->R11 == 0x42244224 ) {

                //
                // This is a simple thing to just prevent the debugger from entering 
                // this statement twice, this would only happen if we switched processor
                // and then switched back.
                //
                TrapContext->R11++;

                switch ( TrapContext->R10 ) {
                case KdServiceStateChangeSymbol:
                    KdpReportLoadSymbolsStateChange( ( PSTRING )TrapContext->Rcx,
                        ( PKD_SYMBOL_INFO )TrapContext->Rdx,
                                                     ( BOOLEAN )TrapContext->R8,
                                                     TrapContext );
                    break;
                case KdServiceStateChangeExcept:
                default:
                    KdpReportExceptionStateChange( &ExceptRecord,
                                                   TrapContext,
                                                   FALSE );
                    break;
                }

            }
            else {

                KdpReportExceptionStateChange( &ExceptRecord,
                                               TrapContext,
                                               FALSE );
            }

            if ( KdpFreezeOwner == ProcessorNumber ) {

                //
                // Assume we're ready to continue, this is an obscure 
                // condition which should only be met by that standard.
                //
                // TODO: Properly deal with KdpSendWaitContinue, KdpReport
                //       results, to handle this condition.
                //

                break;
            }
        }
        else {

            YieldProcessor( );
        }

    }

    if ( ProcessorNumber == KdpFreezeOwner ) {

        KdThawProcessors( );
    }

    if ( KdProcessorBlock[ ProcessorNumber ].Flags & KDPR_FLAG_TPE ) {

        KdPrint( "Tracepoint continue: %p -> %p\n", TrapFrame->Rip, TrapContext->Rip );
    }

    __writecr4( __readcr4( ) | SmapFlag );
    __writecr0( __readcr0( ) | WpFlag );

    TrapFrame->Rip = TrapContext->Rip;

    //
    // TODO: Very unlikely, but if another component or faulty hardware
    //       issues an NMI, we should not be returning TRUE, and we should
    //       not really signal a state change, a work-around would be to check 
    //       for CD 02, or/and signal when we use HalSendNMI.
    //

    return TRUE;
}

NTSTATUS
KdFreezeLoad(

)
{
    KdpNmiHandle = KeRegisterNmiCallback( ( PNMI_CALLBACK )KdServiceInterrupt, NULL );

    if ( KdpNmiHandle == NULL ) {

        return STATUS_UNSUCCESSFUL;
    }

    return STATUS_SUCCESS;
}

VOID
KdFreezeUnload(

)
{

    if ( KdpNmiHandle != NULL ) {

        KeDeregisterNmiCallback( KdpNmiHandle );
    }
}

VOID
KdDebugBreak(

)
{
    //
    // Unfortunately, after building my project around
    // KeFreezeExecution, KeThawExecution, 
    // KeSwitchFrozenProcessor. I have realised they're
    // not suitable and I'll have to re-write them myself.
    // 
    // I took a small look at how they work, and they use 
    // HalSendNmi, and frozen processor api's integrated into the 
    // KiProcessNMI function, I've decided to re-create this feature
    // by using HalSendNMI too, alongside KeRegisterNmiCallback.
    //
    // Taking a look at the handler, we can see the CONTEXT structure
    // is conveniently stored on the PRCB. I also chose to use NMI callbacks
    // because there is no watchdog/timeout (nvm), which exists inside IPI 
    // callbacks even though they're almost the same thing.
    //

    KdNmiBp( );
}

VOID
KdFreezeProcessors(

)
{
    //
    // We need to freeze all processors and set the 
    // state of KdpFrozen, KdpFreezeOwner.
    //

    USHORT           CurrentGroup;
    KAFFINITY_EX     Affinity;
    //ULONG32          Index;
    //PROCESSOR_NUMBER ProcessorNumber;

    KdpFreezeOwner = KdGetCurrentPrcbNumber( );
    KdpFrozen = TRUE;

    Affinity.Count = 1;
    Affinity.Size = 32;
    RtlZeroMemory( &Affinity.Reserved, sizeof( KAFFINITY_EX ) - 4 );

    //
    // Re-implement the KiCopyAffinityEx from KeActiveProcessors,
    // at the time of writing KiMaximumGroups is 32.
    //
    // KiCopyAffinityEx( &Affinity, 32, &KeActiveProcessors );
    //

    for ( CurrentGroup = 0;
          CurrentGroup < 32;
          CurrentGroup++ ) {

        Affinity.Bitmap[ CurrentGroup ] = KeQueryGroupAffinity( CurrentGroup );

    }

    //
    // KeRemovePorcessorAffinityEx - Converts processor number argument to 
    // processor index using KiProcessorIndexToNumberMappingTable, then 
    // removes the bit from the bitmap.
    //

    //ProcessorNumber.Group = ( USHORT )( KdpFreezeOwner / 64 );
    //ProcessorNumber.Number = KdpFreezeOwner % 64;

    //Index = KeGetProcessorIndexFromNumber( &ProcessorNumber );

    //Affinity.Bitmap[ Index / 64 ] &= ~( 1ull << ( Index % 64 ) );
    Affinity.Bitmap[ KdpFreezeOwner / 64 ] &= ~( 1ull << ( KdpFreezeOwner % 64 ) );

    //
    // Fire an NMI on all processors but this one. 
    // This means we have a context record saved on 
    // all Prcbs, and we can handle any kd stuff 
    // inside KdServiceInterrupt
    //

    HalSendNMI( &Affinity );
}

VOID
KdThawProcessors(

)
{
    KdpFrozen = FALSE;
}

VOID
KdSwitchFrozenProcessor(
    _In_ ULONG32 NewProcessor
)
{
    KdpFreezeOwner = NewProcessor;
}
