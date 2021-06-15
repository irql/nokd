
#include "kd.h"

//
// A lot of the MmDbg/MiDbg api's are pretty long-winded
// and I don't really care for reversing most of them 
// accurately, my assumption is that there is a single PTE
// initialized in physical and virtual memory, MmDebugPte,
// and this PTE's physical memory is interchanged.
//
// The api's are written in a way that respects
// machine endianess.
//
// I tried to copy the prototype declarations for these routines 
// but they're not accurate to ntoskrnl.
//
#if 0
NTSTATUS
MmDbgCopyMemory(
    _In_ PVOID Source,
    _In_ PVOID Destination,
    _In_ ULONG Length,
    _In_ ULONG Flags
)
{

    if ( Length != 1 &&
         Length != 2 &&
         Length != 4 &&
         Length != 8 ) {


    }

}

#endif
