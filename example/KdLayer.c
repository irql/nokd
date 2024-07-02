
#include <Limevisor.h>
#include <Nokd.h>

#define MM_COPY_MEMORY_PHYSICAL             0x1
#define MM_COPY_MEMORY_VIRTUAL              0x2

KSTATUS DbgKdInitStatus       = STATUS_UNSUCCESSFUL;
KSTATUS DbgKdConnectionStatus = STATUS_DISCONNECTED;

ULONG64 KdKernelBase;
ULONG32 KdKernelSize;

BOOLEAN DbgKdBreakRequested   = FALSE;

//
// Get the SectionBase, SectionSize from an image by name.
//
// SectionBase & SectionSize should be relative to the image base, so 
// ImageBase could be 0x7FFE00000000, and .text could be 0x7FFE00001000, so SectionBase = 0x1000
//
KSTATUS
KAPI
KdImageSection(
    _In_      PVOID  ImageBase,
    _In_      PCHAR  SectionName,
    _Out_opt_ PVOID* SectionBase,
    _Out_opt_ ULONG* SectionSize
    )
{
    return RtlImageSection(ImageBase,
                           SectionName,
                           SectionBase,
                           SectionSize);
}

//
// Pattern scan function which takes IDA-style patterns.
//
PVOID
KAPI
KdSearchSignature(
    _In_ PVOID BaseAddress,
    _In_ ULONG Length,
    _In_ PCHAR Signature
    )
{
    return KshSearchSequence((ULONG64)BaseAddress,
                             (ULONG64)Length,
                             Signature);
}

//
// Returns the current PCR, this is how I read it from the guest.
//
PVOID
KAPI
DbgKdQueryPcr(
    VOID
    )
{
    ULONG64 PrivilegeLevel;
    ULONG64 PcrAddress;

    KeVmGet(VMCS_GUEST_CS_SELECTOR, &PrivilegeLevel);
    PrivilegeLevel &= 3;

    if (PrivilegeLevel == 0) {
        KeVmGet(VMCS_GUEST_GS_BASE, &PcrAddress);
    }
    else {
        PcrAddress = KeGetMsr(MSR_AMD64_KERNEL_GS_BASE);
    }

    return (PVOID)PcrAddress;
}

ULONG32
KAPI
DbgKdQueryProcessorCount(
    VOID
    )
{
    return KeEnabledProcessorCount;
}

//
// Simply copy memory, this should account for the fact that normally, when KD
// is running on windows, it will reach kernel space on all processors, and then break-in,
// which causes the KVA shadow CR3 to be loaded, if you are in a hypervisor and you break-in
// during user mode, you may have issues due to the kernel not being mapped. I personally just
// disable KVA, but an easy fix is to just load it from the guest's PCR.  
//
KSTATUS
KAPI
DbgKdMmCopyMemory(
    _In_ PVOID    TargetAddress,
    _In_ PVOID    SourceAddress,
    _In_ ULONG64  NumberOfBytes,
    _In_ ULONG32  Flags,
    _In_ ULONG64* NumberOfBytesTransferred
    )
{
    //
    // TODO: Implement a KVA shadowing fix for when the guest is in user mode.
    //

    switch (Flags) {
    case MM_COPY_MEMORY_VIRTUAL:

        __try {

            RtlCopyMemory(TargetAddress,
                          SourceAddress,
                          NumberOfBytes);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {

            if (ARGUMENT_PRESENT(NumberOfBytesTransferred)) {
                *NumberOfBytesTransferred = 0;
            }
            return STATUS_UNSUCCESSFUL;
        }

        if (ARGUMENT_PRESENT(NumberOfBytesTransferred)) {
            *NumberOfBytesTransferred = NumberOfBytes;
        }
        return STATUS_SUCCESS;
    case MM_COPY_MEMORY_PHYSICAL:

        __try {

            KshCopyLogicalToLinear((ULONG64)TargetAddress,
                                   (ULONG64)SourceAddress,
                                   (ULONG64)NumberOfBytes);
        } 
        __except (EXCEPTION_EXECUTE_HANDLER) {

            if (ARGUMENT_PRESENT(NumberOfBytesTransferred)) {
                *NumberOfBytesTransferred = 0;
            }
            return STATUS_UNSUCCESSFUL;
        }

        if (ARGUMENT_PRESENT(NumberOfBytesTransferred)) {
            *NumberOfBytesTransferred = NumberOfBytes;
        }
        return STATUS_SUCCESS;
    default:
        return STATUS_INVALID_PARAMETER;
    }
}

VOID
KAPI
DbgKdFrozenBreakpoint(
    _In_ PKTRAP_FRAME      TrapFrame,
    _In_ PKEXCEPTION_FRAME ExceptionFrame
    )
{
    UNREFERENCED_PARAMETER(TrapFrame);
    UNREFERENCED_PARAMETER(ExceptionFrame);

    DBG_PRINT("0x80000003\n");

    KdReportStateChange(0x80000003 /* STATUS_BREAKPOINT */, DbgKdQueryProcessorContext(KeGetPcr()->ProcessorNumber));
}

//
// My step implementation uses MTFs, see Freeze.c for how this case is caught.
//
VOID
KAPI
DbgKdFrozenStep(
    _In_ PKTRAP_FRAME      TrapFrame,
    _In_ PKEXCEPTION_FRAME ExceptionFrame
    )
{
    UNREFERENCED_PARAMETER(TrapFrame);
    UNREFERENCED_PARAMETER(ExceptionFrame);
    //
    // All processors frozen in here, override the callback internally...
    //

    DbgKdSetFreezeParameters(KeGetPcr()->ProcessorNumber, DbgKdFrozenBreakpoint);

    KdReportStateChange(0x80000004 /* STATUS_SINGLE_STEP */, DbgKdQueryProcessorContext(KeGetPcr()->ProcessorNumber));
}

//
// This is how my breakpoints work, once it is hit, 
// we just need to break-in to KD, and it should do the rest.
//
KSTATUS
KAPI
DbgKdTrapBreakpoint(
    _In_ PKTRAP_FRAME        TrapFrame,
    _In_ PKEXCEPTION_FRAME   ExceptionFrame,
    _In_ PKHYPER_TRAP_RECORD TrapRecord
    )
{
    //
    // This is the shim routine, and trap dispatch!
    //
    // Depending on whether we're going to accept generic VM breakpoints
    // this should be either inserted or not inserted into the exit trap 
    // dispatch, instead of checking anything here.
    //

    DbgKdAcquireDebuggerLock();
    DbgKdFreezeProcessors(TrapFrame, 
                          ExceptionFrame,
                          TrapRecord,
                          DbgKdFrozenBreakpoint);
    DbgKdReleaseDebuggerLock();

    return STATUS_SUCCESS;
}

//
// Simply installs a breakpoint. As commented below, you could also hook
// KdpSetOwedBreakpoints, and install breakpoints when memory is paged in.
//
KSTATUS
KAPI
DbgKdInsertBreakpoint(
    _In_ ULONG64 Address
    )
{
    //
    // TODO: For a proper KD implementation, this should deal
    //       with any owed breakpoints too, ie hooking #PF and
    //       watching for page-in's, a prototype for this was
    //       written in kdproxy, but couldn't be properly done
    //       because of pg.
    //
    // remove bp -> MTF -> insert bp
    //

    KshBreakpointShimSet(Address,
                         SHIM_GLOBAL,
                         DbgKdTrapBreakpoint);
    return STATUS_SUCCESS;
}

//
// Remove a breakpoint
//
KSTATUS
KAPI
DbgKdRemoveBreakpoint(
    _In_ ULONG64 Address
    )
{
    KshBreakpointShimRemove(Address);
    return STATUS_SUCCESS;
}

//
// Poll whether KD has requested a break-in.
//
BOOLEAN
KAPI
DbgKdpPollBreak(
    VOID
    )
{
    if (!K_SUCCESS(DbgKdInitStatus) || !K_SUCCESS(DbgKdConnectionStatus)) {
        return FALSE;
    }

    if (DbgKdBreakRequested) {
        DbgKdBreakRequested = FALSE;
        return TRUE;
    }

    return KdPollBreakIn();
}

//
// This is not necessary at all, you can just type `.reload` into windbg
// and it will automatically read all the loaded modules, etc.
//
// But if you were to implement some form of module load hook, this will
// notify windbg that it has loaded. It can be nice for when you are breakpointing
// on the entry point of a module.
//
ULONG64 DbgKdpSymbol_ImageBase;
ULONG64 DbgKdpSymbol_ImageSize;
ULONG64 DbgKdpSymbol_ProcessId;
PCHAR   DbgKdpSymbol_ImageName;
BOOLEAN DbgKdpSymbol_Unload;

VOID
KAPI
DbgKdFrozenLoadSymbols(
    VOID
    )
{
    KdLoadSymbols(DbgKdQueryProcessorContext(KeGetPcr()->ProcessorNumber),
                  DbgKdpSymbol_ImageName,
                  DbgKdpSymbol_ImageBase,
                  (ULONG32)DbgKdpSymbol_ImageSize,
                  (ULONG32)DbgKdpSymbol_ProcessId,
                  (ULONG64)DbgKdpSymbol_Unload);
}
