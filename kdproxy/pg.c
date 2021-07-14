
#include <kd.h>

BOOLEAN
KdBugCheckEx(
    _In_ ULONG     BugCheckCode,
    _In_ ULONG_PTR BugCheckParameter1,
    _In_ ULONG_PTR BugCheckParameter2,
    _In_ ULONG_PTR BugCheckParameter3
)
{
    BugCheckCode;
    BugCheckParameter1;
    BugCheckParameter2;
    BugCheckParameter3;

    //
    // This is called by KeBugCheckExHook,
    // returning FALSE discards the bugcheck
    // and returning TRUE allows it to proceed.
    //

    KdPrint( "BUGCHECK: %.8LX\n", BugCheckCode );
    KdNmiBp( );

    //
    // KiBugCheckActive => 0
    // KiHardwareTrigger => 0
    //

    _enable( );
    KeLowerIrql( PASSIVE_LEVEL );
    PsTerminateSystemThread( STATUS_SUCCESS );

    return ( BugCheckCode != STATUS_BREAKPOINT &&
             BugCheckCode != 0x109 );
}
