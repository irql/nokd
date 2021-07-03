
#include "kd.h"

KD_DEBUG_DEVICE KdDebugDevice;

BOOLEAN
KdEnterDebugger(

)
{
    BOOLEAN IntState;

    IntState = KeFreezeExecution( );


    return IntState;
}

VOID
KdExitDebugger(
    _In_ BOOLEAN IntState
)
{

    KeThawExecution( IntState );
}
