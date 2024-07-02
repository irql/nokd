
#include <Limevisor.h>
#include <Nokd.h>

KSPIN_LOCK                  DbgKdpDebuggerLock = 0;

VOLATILE ULONG32            DbgKdpFreezeFlags  = 0;
VOLATILE ULONG32            DbgKdpFreezeCount  = 0;
VOLATILE ULONG32            DbgKdpFreezeOwner  = 0;
VOLATILE PKD_FREEZE_ROUTINE DbgKdpFreezeRoutine;

VOLATILE PNT_CONTEXT           DbgKdpContextRecords[64];
VOLATILE PNT_SPECIAL_REGISTERS DbgKdpSpecialRegisters[64];

PNT_CONTEXT
KAPI
DbgKdQueryProcessorContext(
    _In_ ULONG32 ProcessorNumber
    )
{
    return DbgKdpContextRecords[ProcessorNumber];
}

PNT_SPECIAL_REGISTERS
KAPI
DbgKdQuerySpecialRegisters(
    _In_ ULONG32 ProcessorNumber
    )
{
    return DbgKdpSpecialRegisters[ProcessorNumber];
}


VOID
KAPI
DbgKdAcquireDebuggerLock(
    VOID
    )
{
    //
    // It is anticipated that this will only be called by things like the VM exit handler,
    // which is called with IF = 0. This is written under the assumption that 
    // interrupts ARE disabled.
    //
    KIRQL PreviousIrql;

    KeRaiseIrql(DISPATCH_LEVEL, &PreviousIrql);

    DBG_PRINT("Locking this nigga on %d\n", KeGetPcr()->ProcessorNumber);

    do {
        //
        // Check for freeze trap.
        //
    } while (_InterlockedCompareExchange64((volatile long long*)&DbgKdpDebuggerLock, 1, 0) != 0);

    KeLowerIrql(PreviousIrql);
}

BOOLEAN
KAPI
DbgKdTryAcquireDebuggerLock(
    VOID
    )
{
    if (DbgKdpDebuggerLock == 0) {
        return _InterlockedCompareExchange64((volatile long long*)&DbgKdpDebuggerLock, 1, 0) == 0;
    }
    else {
        return FALSE;
    }
}

VOID
KAPI
DbgKdReleaseDebuggerLock(
    VOID
    )
{
    DbgKdpDebuggerLock = 0;
}

KSTATUS
KAPI
DbgKdTrapStep(
    _In_ PKTRAP_FRAME        TrapFrame,
    _In_ PKEXCEPTION_FRAME   ExceptionFrame,
    _In_ PKHYPER_TRAP_RECORD TrapRecord,
    _In_ PVOID               TrapContext
    )
{
    UNREFERENCED_PARAMETER(TrapContext);

    DbgKdpFreezeFlags  &= ~KD_TRAP_ACTIVE;
    DbgKdpFreezeRoutine = DbgKdFrozenStep;
    DbgKdFreezeTrap(TrapFrame,
                    ExceptionFrame,
                    TrapRecord);

    if (DbgKdpFreezeFlags & KD_TRAP_ACTIVE) {
        //
        // Still stepping.
        // Could return STATUS_TRAP_AGAIN
        //
        return STATUS_SUCCESS;
    }

    DbgKdpFreezeCount = 0;
    DbgKdpFreezeFlags = 0;
    DbgKdReleaseDebuggerLock();
    return STATUS_SUCCESS;
}

VOID
KAPI
DbgKdFreezeTrap(
    _In_ PKTRAP_FRAME        TrapFrame,
    _In_ PKEXCEPTION_FRAME   ExceptionFrame,
    _In_ PKHYPER_TRAP_RECORD TrapRecord
    )
{
    UNREFERENCED_PARAMETER(TrapRecord);

    PKPCR                Pcr;
    ULONG32              PreviousProcessor;
    NT_CONTEXT           ContextRecord;
    NT_SPECIAL_REGISTERS SpecialRegisters;
    ULONG64              Long;
    BOOLEAN              Trap;

    Pcr  = KeGetPcr();
    Trap = FALSE;

    DBG_PRINT("Freeze Trap Hit!\n");

    RtlZeroMemory(&ContextRecord, sizeof(NT_CONTEXT));
    RtlZeroMemory(&SpecialRegisters, sizeof(NT_SPECIAL_REGISTERS));

    KeVmGet(VMCS_GUEST_RIP, &TrapFrame->Rip);
    KeVmGet(VMCS_GUEST_RSP, &TrapFrame->Rsp);

    ContextRecord.Rax   = TrapFrame->Rax;
    ContextRecord.Rcx   = TrapFrame->Rcx;
    ContextRecord.Rdx   = TrapFrame->Rdx;
    ContextRecord.R8    = TrapFrame->R8;
    ContextRecord.R9    = TrapFrame->R9;
    ContextRecord.R10   = TrapFrame->R10;
    ContextRecord.R11   = TrapFrame->R11;

    ContextRecord.Xmm0  = TrapFrame->Xmm0;
    ContextRecord.Xmm1  = TrapFrame->Xmm1;
    ContextRecord.Xmm2  = TrapFrame->Xmm2;
    ContextRecord.Xmm3  = TrapFrame->Xmm3;
    ContextRecord.Xmm4  = TrapFrame->Xmm4;
    ContextRecord.Xmm5  = TrapFrame->Xmm5;

    ContextRecord.Rbx   = ExceptionFrame->Rbx;
    ContextRecord.Rbp   = ExceptionFrame->Rbp;
    ContextRecord.Rsi   = ExceptionFrame->Rsi;
    ContextRecord.Rdi   = ExceptionFrame->Rdi;
    ContextRecord.R12   = ExceptionFrame->R12;
    ContextRecord.R13   = ExceptionFrame->R13;
    ContextRecord.R14   = ExceptionFrame->R14;
    ContextRecord.R15   = ExceptionFrame->R15;

    ContextRecord.Xmm6  = ExceptionFrame->Xmm6;
    ContextRecord.Xmm7  = ExceptionFrame->Xmm7;
    ContextRecord.Xmm8  = ExceptionFrame->Xmm8;
    ContextRecord.Xmm9  = ExceptionFrame->Xmm9;
    ContextRecord.Xmm10 = ExceptionFrame->Xmm10;
    ContextRecord.Xmm11 = ExceptionFrame->Xmm11;
    ContextRecord.Xmm12 = ExceptionFrame->Xmm12;
    ContextRecord.Xmm13 = ExceptionFrame->Xmm13;
    ContextRecord.Xmm14 = ExceptionFrame->Xmm14;
    ContextRecord.Xmm15 = ExceptionFrame->Xmm15;
    ContextRecord.ContextFlags |= CONTEXT_INTEGER | CONTEXT_FLOATING_POINT;

    ContextRecord.Rip    = TrapFrame->Rip;
    ContextRecord.Rsp    = TrapFrame->Rsp;
    ContextRecord.EFlags = (ULONG)TrapFrame->EFlags;
    ContextRecord.ContextFlags |= CONTEXT_CONTROL;

    KeVmGet(VMCS_GUEST_CS_SELECTOR, &Long);
    ContextRecord.SegCs = (USHORT)Long;
    KeVmGet(VMCS_GUEST_DS_SELECTOR, &Long);
    ContextRecord.SegDs = (USHORT)Long;
    KeVmGet(VMCS_GUEST_ES_SELECTOR, &Long);
    ContextRecord.SegEs = (USHORT)Long;
    KeVmGet(VMCS_GUEST_FS_SELECTOR, &Long);
    ContextRecord.SegFs = (USHORT)Long;
    KeVmGet(VMCS_GUEST_GS_SELECTOR, &Long);
    ContextRecord.SegGs = (USHORT)Long;
    KeVmGet(VMCS_GUEST_SS_SELECTOR, &Long);
    ContextRecord.SegSs = (USHORT)Long;
    ContextRecord.ContextFlags |= CONTEXT_SEGMENTS;

    KeVmGet(VMCS_GUEST_CR0, &SpecialRegisters.Cr0);
    KeVmGet(VMCS_GUEST_CR3, &SpecialRegisters.Cr3);
    KeVmGet(VMCS_GUEST_CR4, &SpecialRegisters.Cr4);

    KeVmGet(VMCS_GUEST_GDTR_BASE, (ULONG64*)&SpecialRegisters.Gdtr.Base);
    KeVmGet(VMCS_GUEST_GDTR_LIMIT, &Long);
    SpecialRegisters.Gdtr.Limit = (USHORT)Long;

    KeVmGet(VMCS_GUEST_IDTR_BASE, (ULONG64*)&SpecialRegisters.Idtr.Base);
    KeVmGet(VMCS_GUEST_IDTR_LIMIT, &Long);
    SpecialRegisters.Idtr.Limit = (USHORT)Long;

    MmHprAttachSpace();

    DbgKdpContextRecords[Pcr->ProcessorNumber]   = &ContextRecord;
    DbgKdpSpecialRegisters[Pcr->ProcessorNumber] = &SpecialRegisters;

    InterlockedIncrement32(&DbgKdpFreezeCount);

    DBG_PRINT("FreezeCount: %d EnabledProcessorCount: %d... waiting...\n", DbgKdpFreezeCount, KeEnabledProcessorCount);

    while (DbgKdpFreezeCount != KeEnabledProcessorCount)
        ;

    DBG_PRINT("done waiting...\n");

    while (DbgKdpFreezeFlags & KD_FREEZE_ACTIVE) {

        if (Pcr->ProcessorNumber == DbgKdpFreezeOwner) {

            PreviousProcessor = DbgKdpFreezeOwner;

            //
            // Call freeze callback
            //
            DbgKdpFreezeRoutine(TrapFrame, ExceptionFrame);

            if (ContextRecord.EFlags & 0x100) {

                KiHprEnableMtf(TrapFrame, ExceptionFrame, TrapRecord, DbgKdTrapStep, NULL);

                Trap                  = TRUE;
                DbgKdpFreezeFlags    |= KD_TRAP_ACTIVE;
                ContextRecord.EFlags &= ~0x100;
                break;
            }

            if (PreviousProcessor == DbgKdpFreezeOwner) {
                //
                // Thaw, otherwise switch processor and stay frozen.
                //
                DbgKdThawProcessors();
                break;
            }
        }
    }

    //
    // N.B. Ignoring a lot of state shit here...
    //
    TrapFrame->Rip    = ContextRecord.Rip;
    TrapFrame->Rsp    = ContextRecord.Rsp;
    TrapFrame->SegCs  = ContextRecord.SegCs;
    TrapFrame->SegSs  = ContextRecord.SegSs;
    TrapFrame->EFlags = ContextRecord.EFlags;

    TrapFrame->Rax = ContextRecord.Rax;
    TrapFrame->Rcx = ContextRecord.Rcx;
    TrapFrame->Rdx = ContextRecord.Rdx;
    TrapFrame->R8  = ContextRecord.R8;
    TrapFrame->R9  = ContextRecord.R9;
    TrapFrame->R10 = ContextRecord.R10;
    TrapFrame->R11 = ContextRecord.R11;

    TrapFrame->Xmm0 = ContextRecord.Xmm0;
    TrapFrame->Xmm1 = ContextRecord.Xmm1;
    TrapFrame->Xmm2 = ContextRecord.Xmm2;
    TrapFrame->Xmm3 = ContextRecord.Xmm3;
    TrapFrame->Xmm4 = ContextRecord.Xmm4;
    TrapFrame->Xmm5 = ContextRecord.Xmm5;

    ExceptionFrame->Rbx = ContextRecord.Rbx;
    ExceptionFrame->Rbp = ContextRecord.Rbp;
    ExceptionFrame->Rsi = ContextRecord.Rsi;
    ExceptionFrame->Rdi = ContextRecord.Rdi;
    ExceptionFrame->R12 = ContextRecord.R12;
    ExceptionFrame->R13 = ContextRecord.R13;
    ExceptionFrame->R14 = ContextRecord.R14;
    ExceptionFrame->R15 = ContextRecord.R15;

    ExceptionFrame->Xmm6  = ContextRecord.Xmm6;
    ExceptionFrame->Xmm7  = ContextRecord.Xmm7;
    ExceptionFrame->Xmm8  = ContextRecord.Xmm8;
    ExceptionFrame->Xmm9  = ContextRecord.Xmm9;
    ExceptionFrame->Xmm10 = ContextRecord.Xmm10;
    ExceptionFrame->Xmm11 = ContextRecord.Xmm11;
    ExceptionFrame->Xmm12 = ContextRecord.Xmm12;
    ExceptionFrame->Xmm13 = ContextRecord.Xmm13;
    ExceptionFrame->Xmm14 = ContextRecord.Xmm14;
    ExceptionFrame->Xmm15 = ContextRecord.Xmm15;

    KeVmSet(VMCS_GUEST_RIP, TrapFrame->Rip);
    KeVmSet(VMCS_GUEST_RSP, TrapFrame->Rsp);

    InterlockedDecrement32(&DbgKdpFreezeCount);
    //KeSweepLocalCaches();
}

VOID
KAPI
DbgKdThawProcessors(
    VOID
    )
{
    DbgKdpFreezeFlags = 0;
    //KeSweepLocalCaches();
}

VOID
KAPI
DbgKdFreezeProcessors(
    _In_ PKTRAP_FRAME        TrapFrame,
    _In_ PKEXCEPTION_FRAME   ExceptionFrame,
    _In_ PKHYPER_TRAP_RECORD TrapRecord,
    _In_ PKD_FREEZE_ROUTINE  FreezeRoutine
    )
{
    //
    // Environment: Debugger lock held.
    //
    PKPCR   Pcr;
    ULONG32 Processor;

    Pcr = KeGetPcr();

    //
    // Ensure the previous freeze was complete.
    //
    while (DbgKdpFreezeCount != 0)
        ;

    DbgKdpFreezeRoutine = FreezeRoutine; 
    DbgKdpFreezeFlags   = KD_FREEZE_ACTIVE;
    DbgKdpFreezeOwner   = Pcr->ProcessorNumber;
    DbgKdpFreezeCount   = 0;

    for (Processor = 0; Processor < KeEnabledProcessorCount; Processor++) {

        if (Processor == DbgKdpFreezeOwner) {
            continue;
        }

        KeWakeNmi(Processor);

        DBG_PRINT("WakeNMI sent to %d\n", Processor);
    }

    DbgKdFreezeTrap(TrapFrame, ExceptionFrame, TrapRecord);

    DbgKdReleaseDebuggerLock();
}

VOID
KAPI
DbgKdSetFreezeParameters(
    _In_ ULONG32            FreezeOwner,
    _In_ PKD_FREEZE_ROUTINE FreezeRoutine
    )
{
    DbgKdpFreezeOwner   = FreezeOwner;
    DbgKdpFreezeRoutine = FreezeRoutine;
}

VOID
KAPI
DbgKdSwapProcessor(
    _In_ ULONG32 ProcessorNumber
    )
{
    DbgKdpFreezeOwner = ProcessorNumber;
}
