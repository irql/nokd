
#include <kd.h>

//
// TODO: Need to code wrappers around basic memory functionality
//       to catch ANY exceptions, and return appropriate status codes
//       for convenience, we also need to play with the 
//       Process->DirectoryTableBase.
//

typedef ULONG_PTR KADDRESS_SPACE, *PKADDRESS_SPACE;

KADDRESS_SPACE
KdGetProcessSpace(
    _In_ PEPROCESS Process
)
{
    return *( KADDRESS_SPACE* )( ( ULONG_PTR )Process + KdDebuggerDataBlock.OffsetEprocessDirectoryTableBase );
}

KADDRESS_SPACE
KdSetSpace(
    _In_ KADDRESS_SPACE AddressSpace
)
{
    KADDRESS_SPACE PreviousAddressSpace;

    PreviousAddressSpace = __readcr3( );

    __writecr3( AddressSpace );

    return PreviousAddressSpace;
}

NTSTATUS
KdCopyProcessSpace(
    _In_ PEPROCESS Process,
    _In_ PVOID     Destination,
    _In_ PVOID     Source,
    _In_ ULONG     Length
)
{
    NTSTATUS       ntStatus;
    KADDRESS_SPACE AddressSpace;

    //
    // If you crash on this statement, there's no saving anything.
    //

    AddressSpace = KdSetSpace( KdGetProcessSpace( Process ) );

    __try {

        RtlCopyMemory( Destination, Source, Length );
        ntStatus = STATUS_SUCCESS;
    }
    __except ( TRUE ) {

        ntStatus = STATUS_UNSUCCESSFUL;
    }

    KdSetSpace( AddressSpace );

    return ntStatus;
}
