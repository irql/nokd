
#include <kd.h>

KD_DEBUG_DEVICE  KdDebugDevice;

//
// Variables for recovering from a hde64 branch/decode mis-predict.
// This only works on multi-processor systems, but it should let
// the system recover from a failed trace point. It uses 
// KeQueryPerformanceCounter
//
VOLATILE ULONG64 KdpTraceTime;
VOLATILE ULONG32 KdpTraceRecoverer;

VOLATILE ULONG32 KdpFrozenCount = 0;
VOLATILE ULONG32 KdpFreezeOwner;
VOLATILE BOOLEAN KdpFrozen = FALSE;
PVOID            KdpNmiHandle = NULL;

//
// This structure is exists to manage breakpoints &
// the trap flag.
//
PKD_PROCESSOR    KdProcessorBlock;

#if KD_SAFE_TRACE_POINTS
VOLATILE BOOLEAN KdpTracing = FALSE;
#endif

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
    KAFFINITY_EX       Affinity;

    //
    // The system in place by windows, and shown
    // inside KxNmiInterrupt's prologue, and KiRestoreProcessorState
    // is very fucking disgusting. Half of the registers are saved to KTRAP_FRAME
    // and the other half is on KEXCEPTION_FRAME, fortunately for us, these 
    // structures are sequential on the stack.
    //

    PKTRAP_FRAME       TrapFrame;
    PKEXCEPTION_FRAME  ExceptFrame;
    KPROCESSOR_MODE    ProcessorMode;

    //
    // This seems a bit insane, and yes, I should probably read the
    // stack address from the tss ist, but, this should 
    // get the stack base, subtract 32 for the push of r8, rcx, rax, rdx,
    // and then subtract the sizeof of KTRAP_FRAME, in order to get the address
    // of the KTRAP_FRAME structure which is pushed for each interrupt handler.
    // On another note, this would also break is kva shadowing is not enabled
    // because the 32 bytes may not be pushed by the xxxShadow stub.
    //
    // TODO: Get it from the tss.
    //

    TrapFrame = ( PKTRAP_FRAME )( ( ( ( ULONG64 )_AddressOfReturnAddress( ) + 0x1000 ) & ~0xFFF ) - sizeof( KTRAP_FRAME ) - 32 );
    ExceptFrame = ( PKEXCEPTION_FRAME )( ( ULONG64 )TrapFrame - sizeof( KEXCEPTION_FRAME ) );

    //
    // Processor mode is determined by the code segment RPL for
    // interrupt handlers.
    //

    ProcessorMode = ( TrapFrame->SegCs & 1 ) == 1 ? UserMode : KernelMode;

    Prcb = KeGetCurrentPrcb( );
    ProcessorNumber = KdGetPrcbNumber( Prcb );
    TrapContext = KdGetPrcbContext( Prcb );

    //
    // Protect against real NMI's, this is a super unlikely condition
    // unless other drivers are sending NMIs. We can assume it's meant
    // for the debugger if the TrapContext->Rip points to cd 02.
    //

    SmapFlag = __readcr4( ) & ( 1ULL << 21 );
    __writecr4( __readcr4( ) & ~( 1ULL << 21 ) );

    if ( !KdpFrozen && !KdpTracing ) {

        __try {

            if ( memcmp( ( void* )( TrapContext->Rip - KdpBreakpointCodeLength ),
                         KdpBreakpointCode,
                         KdpBreakpointCodeLength ) != 0 ) {

                __writecr4( __readcr4( ) | SmapFlag );
                return FALSE;
            }
        }
        __except ( TRUE ) {

            __writecr4( __readcr4( ) | SmapFlag );
            return FALSE;
        }
    }

    WpFlag = __readcr0( ) & ( 1ULL << 16 );
    __writecr0( __readcr0( ) & ~( 1ULL << 16 ) );

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
    // never being re-aligned due to KDPR_FLAG_TP_ENABLE not being set.
    //

    if ( KdProcessorBlock[ ProcessorNumber ].Flags & KDPR_FLAG_TP_ENABLE &&
         KdProcessorBlock[ ProcessorNumber ].Tracepoint == TrapContext->Rip - KdpBreakpointCodeLength ) {

        RtlCopyMemory( ( void* )KdProcessorBlock[ ProcessorNumber ].Tracepoint,
            ( void* )KdProcessorBlock[ ProcessorNumber ].TracepointCode,
                       0x20 );

        TrapContext->Rip -= KdpBreakpointCodeLength;
        KdProcessorBlock[ ProcessorNumber ].Flags &= ~KDPR_FLAG_TP_ENABLE;

        KeSweepLocalCaches( );
#if KD_TRACE_POINT_LOGGING
        KdPrint( "Tracepoint caught: %p\n", TrapContext->Rip );
#endif
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

            //
            // TODO: Would be very nice to have a time-out watchdog, in-case an exception occurs
            //       during tracing.
            //

            YieldProcessor( );

            if ( KdpTracing &&
                 KdpFrozenCount != KeQueryActiveProcessorCountEx( 0xFFFF ) &&
                 ProcessorNumber == KdpTraceRecoverer &&
                 ( ULONG64 )KeQueryPerformanceCounter( 0 ).QuadPart + 0x1000 > KdpTraceTime ) {

                Affinity.Count = 1;
                Affinity.Size = 32;
                RtlZeroMemory( &Affinity.Reserved, sizeof( KAFFINITY_EX ) - 4 );

                Affinity.Bitmap[ KdpFreezeOwner / 64 ] &= ~( 1ull << ( KdpFreezeOwner % 64 ) );

                HalSendNMI( &Affinity );
            }
        }

    }

#if KD_SAFE_TRACE_POINTS
    KdpTracing = ( KdProcessorBlock[ ProcessorNumber ].Flags & KDPR_FLAG_TP_ENABLE ) == KDPR_FLAG_TP_ENABLE;

    if ( ProcessorNumber == KdpFreezeOwner && !KdpTracing ) {
#else
    if ( ProcessorNumber == KdpFreezeOwner ) {
#endif

        KdThawProcessors( );
    }
#if KD_SAFE_TRACE_POINTS
    else {

        KdpFrozenCount--;

        if ( KeQueryActiveProcessorCountEx( 0xFFFF ) > 1 ) {

            KdpTraceRecoverer = ProcessorNumber++;
            if ( KdpTraceRecoverer >= KeQueryActiveProcessorCountEx( 0xFFFF ) ) {

                KdpTraceRecoverer = 0;
            }
            KdpTraceTime = ( ULONG64 )KeQueryPerformanceCounter( 0 ).QuadPart;
        }
    }
#endif

    if ( KdProcessorBlock[ ProcessorNumber ].Flags & KDPR_FLAG_TP_ENABLE ) {

#if KD_TRACE_POINT_LOGGING
        KdPrint( "Tracepoint continue: %p -> %p\n", TrapFrame->Rip, TrapContext->Rip );
#endif
    }

    __writecr4( __readcr4( ) | SmapFlag );
    __writecr0( __readcr0( ) | WpFlag );

    //
    // This is a re-implementation of KiRestoreProcessorState/KeContextToKframes/KxContextToKframes,
    // windows has a truly awful system for saving and restoring the
    // state of the processor between interrupts. This is because it's 
    // split between two structures on the stack, KEXCEPTION_FRAME &
    // KTRAP_FRAME. 
    //
    // TODO: KiRestoreProcessorControlState, RtlXRestore
    //

    if ( ( TrapContext->ContextFlags & CONTEXT_CONTROL ) == CONTEXT_CONTROL ) {

        if ( ProcessorMode == KernelMode ) {

            TrapContext->EFlags &= 0x250FD5;
        }
        else {

            TrapContext->EFlags &= 0x210DD5;
            TrapContext->EFlags |= 0x200;
        }

        TrapFrame->EFlags = TrapContext->EFlags;

        TrapFrame->Rip = TrapContext->Rip;
        TrapFrame->Rsp = TrapContext->Rsp;

        if ( ProcessorMode == UserMode ) {

            TrapFrame->SegSs = 0x2B;
            if ( TrapFrame->SegCs != 0x33 ) {

                TrapFrame->SegCs = 0x23;
            }

            if ( TrapFrame->SegCs == 0x23 ) {

                //
                // lmfao, nice validation.
                //

                TrapFrame->Rip <<= 16;
                TrapFrame->Rip >>= 16;
            }
        }
        else {

            TrapFrame->SegCs = 0x10;
            TrapFrame->SegSs = 0x18;
        }
    }

    if ( ( TrapContext->ContextFlags & CONTEXT_INTEGER ) == CONTEXT_INTEGER ) {

        TrapFrame->Rax = TrapContext->Rax;
        TrapFrame->Rcx = TrapContext->Rcx;
        TrapFrame->Rdx = TrapContext->Rdx;
        TrapFrame->R8 = TrapContext->R8;
        TrapFrame->R9 = TrapContext->R9;
        TrapFrame->R10 = TrapContext->R10;
        TrapFrame->R11 = TrapContext->R11;
        TrapFrame->Rbp = TrapContext->Rbp;
        ExceptFrame->Rbx = TrapContext->Rbx;
        ExceptFrame->Rsi = TrapContext->Rsi;
        ExceptFrame->Rdi = TrapContext->Rdi;
        ExceptFrame->R12 = TrapContext->R12;
        ExceptFrame->R13 = TrapContext->R13;
        ExceptFrame->R14 = TrapContext->R14;
        ExceptFrame->R15 = TrapContext->R15;
    }

    if ( ( TrapContext->ContextFlags & CONTEXT_XSTATE ) == CONTEXT_XSTATE ) {

        //
        // TODO: RtlXRestoreS/KiCopyXStateArea.
        //
    }

    if ( ( TrapContext->ContextFlags & CONTEXT_FLOATING_POINT ) == CONTEXT_FLOATING_POINT ) {

        TrapFrame->Xmm0 = TrapContext->Xmm0;
        TrapFrame->Xmm1 = TrapContext->Xmm1;
        TrapFrame->Xmm2 = TrapContext->Xmm2;
        TrapFrame->Xmm3 = TrapContext->Xmm3;
        TrapFrame->Xmm4 = TrapContext->Xmm4;
        TrapFrame->Xmm5 = TrapContext->Xmm5;
        ExceptFrame->Xmm6 = TrapContext->Xmm6;
        ExceptFrame->Xmm7 = TrapContext->Xmm7;
        ExceptFrame->Xmm8 = TrapContext->Xmm8;
        ExceptFrame->Xmm9 = TrapContext->Xmm9;
        ExceptFrame->Xmm10 = TrapContext->Xmm10;
        ExceptFrame->Xmm11 = TrapContext->Xmm11;
        ExceptFrame->Xmm12 = TrapContext->Xmm12;
        ExceptFrame->Xmm13 = TrapContext->Xmm13;
        ExceptFrame->Xmm14 = TrapContext->Xmm14;
        ExceptFrame->Xmm15 = TrapContext->Xmm15;
        TrapFrame->MxCsr = 0xFFBF/*KiMxCsrMask*/ & TrapContext->MxCsr;

        if ( ProcessorMode == UserMode ) {

            TrapContext->FltSave.MxCsr = _mm_getcsr( );
            TrapContext->FltSave.ControlWord &= 0x1F3F;
        }
    }

    if ( ( TrapContext->ContextFlags & CONTEXT_DEBUG_REGISTERS ) == CONTEXT_DEBUG_REGISTERS ) {

        if ( ProcessorMode == UserMode ) {

            TrapFrame->Dr0 = TrapContext->Dr0 > 0x7FFFFFFEFFFF ? 0 : TrapContext->Dr0;
            TrapFrame->Dr1 = TrapContext->Dr1 > 0x7FFFFFFEFFFF ? 0 : TrapContext->Dr1;
            TrapFrame->Dr2 = TrapContext->Dr2 > 0x7FFFFFFEFFFF ? 0 : TrapContext->Dr2;
            TrapFrame->Dr3 = TrapContext->Dr3 > 0x7FFFFFFEFFFF ? 0 : TrapContext->Dr3;
        }
        else {
            TrapFrame->Dr0 = TrapContext->Dr0;
            TrapFrame->Dr1 = TrapContext->Dr1;
            TrapFrame->Dr2 = TrapContext->Dr2;
            TrapFrame->Dr3 = TrapContext->Dr3;
        }

        TrapFrame->Dr6 = 0;
        TrapFrame->Dr7 = TrapContext->Dr7 & 0xFFFF0355;
    }

    //
    // Very weird that they do this inside KeContextToKframes
    //

    _fxrstor( &TrapContext->FltSave );

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
