
#include <kd.h>

#if KD_PAGED_MEMORY_FIX

VOID
KdOwePrepare(

)
{

    ULONG64 WpFlag;

    if ( KdpOweBreakpoint + KdpOweTracepoint == 1 ) {
        WpFlag = __readcr0( ) & ( 1ULL << 16 );
        __writecr0( __readcr0( ) & ~( 1ULL << 16 ) );

        RtlCopyMemory( ( void* )KdpOweCodeAddress, KdpOweCodeHook, 23 );

        __writecr0( __readcr0( ) | WpFlag );
        KeSweepLocalCaches( );
    }
    else if ( KdpOweBreakpoint + KdpOweTracepoint == 0 ) {
        WpFlag = __readcr0( ) & ( 1ULL << 16 );
        __writecr0( __readcr0( ) & ~( 1ULL << 16 ) );

        RtlCopyMemory( ( void* )KdpOweCodeAddress, KdpOweCodeOrig, 23 );

        __writecr0( __readcr0( ) | WpFlag );
        KeSweepLocalCaches( );
    }

}

VOID
KdPageFault(
    _In_ ULONG64 FaultAddress
)
{
    //
    // This is supposed to be a re-implementation of KdSetOwedBreakpoints
    // but because we don't work the same with ntos, in this respect, I've
    // just written it completely different.
    //

    FaultAddress;
    ULONG32 CurrentBreakpoint;
    ULONG64 WpFlag;
    ULONG64 SmapFlag;
    ULONG32 CurrentProcessor;
    //KIRQL   PreviousIrql;

    //
    // This might look a little bit insane but it's fine
    //
    // TODO: Just push, jmp instead of call, jmp inside shellcode.
    //
    // The reason we just MmIsAddressValid and ignore the fault address 
    // is because, if a page fault occurs, it will most likely just page-in 
    // the entire CONTROL_AREA/SEGMENT/SECTION_OBJECT/whatever, instead of
    // just the page.
    //

    *( ULONG64* )_AddressOfReturnAddress( ) = ( ULONG64 )_ReturnAddress( ) + 5 + ( LONG64 )( *( LONG32* )( ( ULONG64 )_ReturnAddress( ) + 1 ) );

    WpFlag = __readcr0( ) & ( 1ULL << 16 );
    __writecr0( __readcr0( ) & ~( 1ULL << 16 ) );

    SmapFlag = __readcr4( ) & ( 1ULL << 21 );
    __writecr4( __readcr4( ) & ~( 1ULL << 21 ) );

    if ( KdpOweTracepoint != 0 ) {
#if KD_SAFE_TRACE_POINTS

        CurrentProcessor = KdGetCurrentPrcbNumber( );

        NT_ASSERT( KdProcessorBlock[ CurrentProcessor ].Flags & KDPR_FLAG_TP_ENABLE );
        //NT_ASSERT( ( KdProcessorBlock[ CurrentProcessor ].Tracepoint & ~0xFFF ) == ( FaultAddress & ~0xFFF ) );
        NT_ASSERT( KdProcessorBlock[ CurrentProcessor ].Flags & ( KDPR_FLAG_TP_OWE | KDPR_FLAG_TP_OWE_NO_INC ) );

        // Cope with the prefetcher mechanisms :xanax:
        //if ( ( KdProcessorBlock[ CurrentProcessor ].Tracepoint & ~0xFFF ) == ( FaultAddress & ~0xFFF ) ) {
        if ( MmIsAddressValid( ( PVOID )KdProcessorBlock[ CurrentProcessor ].Tracepoint ) ) {

            KdpOweTracepoint--;
            KdOwePrepare( );
            KdOweTracepoint( CurrentProcessor );
        }

#else
        for ( CurrentProcessor = 0;
              CurrentProcessor < KeQueryActiveProcessorCountEx( 0xFFFF );
              CurrentProcessor++ ) {

            if ( KdProcessorBlock[ CurrentProcessor ].Flags & KDPR_FLAG_TP_ENABLE ) {

                if ( ( KdProcessorBlock[ CurrentProcessor ].Tracepoint & ~0xFFF ) == ( FaultAddress & ~0xFFF ) ) {

                    //if ( KdProcessorBlock[ CurrentProcessor ].Flags & ( KDPR_FLAG_TP_OWE | KDPR_FLAG_TP_OWE_NO_INC ) ) {
                    if ( MmIsAddressValid( ( PVOID )KdProcessorBlock[ CurrentProcessor ].Tracepoint ) ) {

                        KdpOweTracepoint--;
                        KdOwePrepare( );
                        KdOweTracepoint( CurrentProcessor );
        }
    }
}
        }
#endif
    }

    if ( KdpOweBreakpoint ) {

        for ( CurrentBreakpoint = 0;
              CurrentBreakpoint < KD_BREAKPOINT_TABLE_LENGTH;
              CurrentBreakpoint++ ) {

            if ( ( KdpBreakpointTable[ CurrentBreakpoint ].Flags & ( KD_BPE_SET | KD_BPE_OWE ) ) == ( KD_BPE_SET | KD_BPE_OWE ) ) {

                //if ( ( KdpBreakpointTable[ CurrentBreakpoint ].Address & ~0xFFF ) == ( FaultAddress & ~0xFFF ) &&
                if ( MmIsAddressValid( ( PVOID )KdpBreakpointTable[ CurrentBreakpoint ].Address ) &&
                     KdpBreakpointTable[ CurrentBreakpoint ].Process == PsGetCurrentProcess( ) ) {

                    //
                    // TODO: This could be on the page boundary and could cause
                    //       an issue because our bp is 2 bytes at the time of 
                    //       writing.
                    //

                    KdpOweBreakpoint--;
                    KdOwePrepare( );

                    __try {

                        RtlCopyMemory( ( void* )KdpBreakpointTable[ CurrentBreakpoint ].Content,
                            ( void* )KdpBreakpointTable[ CurrentBreakpoint ].Address,
                                       0x20 );

                        RtlCopyMemory( ( void* )KdpBreakpointTable[ CurrentBreakpoint ].Address,
                                       KdpBreakpointCode,
                                       KdpBreakpointCodeLength );

                        KeSweepLocalCaches( );
                        KdpBreakpointTable[ CurrentBreakpoint ].Flags &= ~KD_BPE_OWE;
                    }
                    __except ( TRUE ) {

                    }
                }
            }
        }
    }

    __writecr4( __readcr4( ) | SmapFlag );
    __writecr0( __readcr0( ) | WpFlag );
}
#endif
