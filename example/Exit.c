
#include <Limevisor.h>
#include <Hypervisor/KiHpr.h>
#include <Nokd.h>
#include <Hypervisor/Ksrp.h>

VOID
KAPI
KiHprExitDispatch(
    _In_ PKTRAP_FRAME      TrapFrame,
    _In_ PKEXCEPTION_FRAME ExceptionFrame
    )
{

    //
    // ...
    //

    //
    // Check for a debug break on any processor.
    //
#if LIME_HV_ENABLE_NOKD

    if (K_SUCCESS(DbgKdInitStatus) && K_SUCCESS(DbgKdConnectionStatus)) {

        if (DbgKdTryAcquireDebuggerLock()) {

            if (DbgKdpPollBreak()) {
                DBG_PRINT("Break requested\n");

                DbgKdFreezeProcessors(TrapFrame,
                                      ExceptionFrame,
                                      &TrapRecord,
                                      DbgKdFrozenBreakpoint);
            }
            else {
                DbgKdReleaseDebuggerLock();
            }
        }
    }
#endif

    //
    // Handle your vmexit here...
    //

    //
    // All other processors should reach this statement, and be frozen.
    //
    if ((DbgKdpFreezeFlags & (KD_FREEZE_ACTIVE | KD_TRAP_ACTIVE)) == KD_FREEZE_ACTIVE) {

        DbgKdFreezeTrap(TrapFrame,
                        ExceptionFrame,
                        &TrapRecord);
    }

    //
    // ...
    //
}

