

#include <ntifs.h>

typedef struct _IMAGE_FILE_HEADER {
    USHORT  Machine;
    USHORT  NumberOfSections;
    ULONG32 TimeDateStamp;
    ULONG32 PointerToSymbolTable;
    ULONG32 NumberOfSymbols;
    USHORT  SizeOfOptionalHeader;
    USHORT  Characteristics;
} IMAGE_FILE_HEADER, *PIMAGE_FILE_HEADER;

typedef struct _IMAGE_DATA_DIRECTORY {
    ULONG32 VirtualAddress;
    ULONG32 Size;
} IMAGE_DATA_DIRECTORY, *PIMAGE_DATA_DIRECTORY;

#define IMAGE_NUMBEROF_DIRECTORY_ENTRIES    16

typedef struct _IMAGE_OPTIONAL_HEADER64 {
    //
    // Standard fields.
    //

    USHORT  Magic;
    UCHAR   MajorLinkerVersion;
    UCHAR   MinorLinkerVersion;
    ULONG32 SizeOfCode;
    ULONG32 SizeOfInitializedData;
    ULONG32 SizeOfUninitializedData;
    ULONG32 AddressOfEntryPoint;
    ULONG32 BaseOfCode;
    //ULONG32  BaseOfData;

    //
    // NT additional fields.
    //

    ULONG64  ImageBase;
    ULONG32  SectionAlignment;
    ULONG32  FileAlignment;
    USHORT   MajorOperatingSystemVersion;
    USHORT   MinorOperatingSystemVersion;
    USHORT   MajorImageVersion;
    USHORT   MinorImageVersion;
    USHORT   MajorSubsystemVersion;
    USHORT   MinorSubsystemVersion;
    ULONG32  Win32VersionValue;
    ULONG32  SizeOfImage;
    ULONG32  SizeOfHeaders;
    ULONG32  CheckSum;
    USHORT   Subsystem;
    USHORT   DllCharacteristics;
    ULONG64  SizeOfStackReserve;
    ULONG64  SizeOfStackCommit;
    ULONG64  SizeOfHeapReserve;
    ULONG64  SizeOfHeapCommit;
    ULONG32  LoaderFlags;
    ULONG32  NumberOfRvaAndSizes;
    IMAGE_DATA_DIRECTORY DataDirectory[ IMAGE_NUMBEROF_DIRECTORY_ENTRIES ];
} IMAGE_OPTIONAL_HEADER64, *PIMAGE_OPTIONAL_HEADER64;

typedef struct _IMAGE_NT_HEADERS64 {
    ULONG32                 Signature;
    IMAGE_FILE_HEADER       FileHeader;
    IMAGE_OPTIONAL_HEADER64 OptionalHeader;
} IMAGE_NT_HEADERS64, *PIMAGE_NT_HEADERS64;

typedef struct _IMAGE_SECTION_HEADER {
    UCHAR       Name[ 8 ];
    union {
        ULONG32 PhysicalAddress;
        ULONG32 VirtualSize;
    } Misc;
    ULONG32     VirtualAddress;
    ULONG32     SizeOfRawData;
    ULONG32     PointerToRawData;
    ULONG32     PointerToRelocations;
    ULONG32     PointerToLinenumbers;
    USHORT      NumberOfRelocations;
    USHORT      NumberOfLinenumbers;
    ULONG32     Characteristics;
} IMAGE_SECTION_HEADER, *PIMAGE_SECTION_HEADER;

#define IMAGE_FIRST_SECTION( ntheader ) ((PIMAGE_SECTION_HEADER)\
    ((ULONG64)(ntheader) +\
     FIELD_OFFSET( IMAGE_NT_HEADERS64, OptionalHeader ) +\
     ((ntheader))->FileHeader.SizeOfOptionalHeader\
    ))

typedef struct _RTL_PROCESS_MODULE_INFORMATION {
    HANDLE Section;
    PVOID  MappedBase;
    PVOID  ImageBase;
    ULONG  ImageSize;
    ULONG  Flags;
    USHORT LoadOrderIndex;
    USHORT InitOrderIndex;
    USHORT LoadCount;
    USHORT OffsetToFileName;
    UCHAR  FullPathName[ 256 ];
} RTL_PROCESS_MODULE_INFORMATION, *PRTL_PROCESS_MODULE_INFORMATION;

typedef struct _RTL_PROCESS_MODULES {
    ULONG                          NumberOfModules;
    RTL_PROCESS_MODULE_INFORMATION Modules[ ANYSIZE_ARRAY ];
} RTL_PROCESS_MODULES, *PRTL_PROCESS_MODULES;

DECLSPEC_IMPORT NTSTATUS NtQuerySystemInformation( _In_  ULONG  SystemInformationClass,
                                                   _Out_ PVOID  SystemInformation,
                                                   _In_  ULONG  SystemInformationLength,
                                                   _Out_ PULONG ReturnLength );

DECLSPEC_IMPORT BOOLEAN KdPollBreakIn( );

PBOOLEAN KdPitchDebugger;
PULONG   KdpDebugRoutineSelect;

NTSTATUS
KdImageAddress(
    _In_      PCHAR  ImageName,
    _Out_opt_ PVOID* ImageBase,
    _Out_opt_ ULONG* ImageSize
)
{
    NTSTATUS             ntStatus;
    ULONG                Length;
    ULONG64              CurrentModule;
    PRTL_PROCESS_MODULES ModuleInformation;

    NtQuerySystemInformation( 11,
                              NULL,
                              0,
                              &Length );

    ModuleInformation = ExAllocatePoolWithTag( NonPagedPoolNx,
                                               Length,
                                               'dKoN' );

    ntStatus = NtQuerySystemInformation( 11,
                                         ModuleInformation,
                                         Length,
                                         &Length );
    if ( !NT_SUCCESS( ntStatus ) ) {

        ExFreePoolWithTag( ModuleInformation, 'dKoN' );
        return ntStatus;
    }

    for ( CurrentModule = 0;
          CurrentModule < ModuleInformation->NumberOfModules;
          CurrentModule++ ) {

        if ( strcmp( ( PCHAR )ModuleInformation->Modules[ CurrentModule ].FullPathName +
                     ModuleInformation->Modules[ CurrentModule ].OffsetToFileName,
                     ImageName ) == 0 ) {

            if ( ImageBase != NULL ) {

                *ImageBase = ModuleInformation->Modules[ CurrentModule ].ImageBase;
            }

            if ( ImageSize != NULL ) {

                *ImageSize = ModuleInformation->Modules[ CurrentModule ].ImageSize;
            }

            ExFreePoolWithTag( ModuleInformation, 'dKoN' );
            return STATUS_SUCCESS;
        }
    }

    ExFreePoolWithTag( ModuleInformation, 'dKoN' );
    return STATUS_NOT_FOUND;
}

NTSTATUS
KdImageSection(
    _In_      PVOID  ImageBase,
    _In_      PCHAR  SectionName,
    _Out_opt_ PVOID* SectionBase,
    _Out_opt_ ULONG* SectionSize
)
{
    ULONG64               ImageBase_;
    ULONG64               CurrentSection;
    PIMAGE_NT_HEADERS64   NtHeaders;
    PIMAGE_SECTION_HEADER SectionHeaders;

    ImageBase_ = ( ULONG64 )ImageBase;
    NtHeaders = ( PIMAGE_NT_HEADERS64 )( ImageBase_ + *( ULONG32* )( ImageBase_ + 0x3c ) );
    SectionHeaders = IMAGE_FIRST_SECTION( NtHeaders );

    for ( CurrentSection = 0;
          CurrentSection < NtHeaders->FileHeader.NumberOfSections;
          CurrentSection++ ) {

        if ( *( ULONG64* )SectionHeaders[ CurrentSection ].Name ==
             *( ULONG64* )SectionName ) {

            if ( SectionBase != NULL ) {

                *SectionBase = ( PVOID )SectionHeaders[ CurrentSection ].VirtualAddress;
            }

            if ( SectionSize != NULL ) {

                *SectionSize = SectionHeaders[ CurrentSection ].Misc.VirtualSize;
            }

            return STATUS_SUCCESS;
        }
    }

    return STATUS_NOT_FOUND;
}

PVOID
KdSearchSignature(
    _In_ PVOID BaseAddress,
    _In_ ULONG Length,
    _In_ PCHAR Signature
)
{

    UCHAR* Pos;
    UCHAR* End;
    CHAR*  Sig;
    UCHAR* Match;

    Sig = Signature;
    Pos = ( UCHAR* )BaseAddress;
    End = ( UCHAR* )BaseAddress + Length;
    Match = NULL;

#define _HEX_CHAR_( x ) ( x <= '9' ? x - '0' : (x - 'A' + 0xA) )

    for ( ; Pos < End; Pos++ ) {

        if ( *Sig == '?' ||
             *Pos == ( _HEX_CHAR_( Sig[ 0 ] ) << 4 | _HEX_CHAR_( Sig[ 1 ] ) ) ) {

            if ( Match == NULL ) {

                Match = Pos;
            }

            Sig += ( *Sig != '?' ) + 1;

            if ( *Sig == 0 || *++Sig == 0 ) {

                return Match;
            }
        }
        else if ( Match ) {

            Sig = Signature;
            Pos = Match;
            Match = NULL;
        }
    }

#undef _HEX_CHAR_

    return NULL;
}

KTIMER KdBreakTimer;
KDPC   KdBreakDpc;

VOID
KdBreakTimerDpcCallback(
    _In_     PKDPC Dpc,
    _In_opt_ PVOID DeferredContext,
    _In_opt_ PVOID SystemArgument1,
    _In_opt_ PVOID SystemArgument2
)
{
    Dpc;
    DeferredContext;
    SystemArgument1;
    SystemArgument2;

    //
    // Inside ... -> KeClockInterruptNotify -> KiUpdateRunTime -> KeAccumulateTicks
    //
    // if ( ( KdDebuggerEnabled || KdEventLoggingEnabled ) && KiPollSlot == Prcb->Number ) {
    //   
    //     KdCheckForDebugBreak( );
    // }
    //
    // VOID 
    // KdCheckForDebugBreak( 
    //
    // )
    // {
    //
    //     if ( !KdPitchDebugger && KdDebuggerEnabled || KdEventLoggingEnabled ) {
    // 
    //         if ( KdPollBreakIn( ) ) {
    //
    //             DbgBreakPointWithStatus( 1u );
    //         }
    //     }
    // }
    //
    //

    KIRQL PreviousIrql;
    ULONG Interrupts;

    KeRaiseIrql( HIGH_LEVEL, &PreviousIrql );
    Interrupts = __readeflags( ) & 0x200;
    _disable( );

    *KdDebuggerEnabled = TRUE;
    *KdDebuggerNotPresent = FALSE;
    *KdPitchDebugger = FALSE;

    *KdpDebugRoutineSelect = 1;

    if ( KdPollBreakIn( ) ) {

        DbgBreakPointWithStatus( 1u );
    }

    *KdpDebugRoutineSelect = 0;

    *KdDebuggerEnabled = FALSE;
    *KdDebuggerNotPresent = TRUE;
    *KdPitchDebugger = TRUE;

    __writeeflags( __readeflags( ) | Interrupts );
    KeLowerIrql( PreviousIrql );

}

VOID
KdDriverUnload(
    _In_ PDRIVER_OBJECT DriverObject
)
{
    DriverObject;

    KeCancelTimer( &KdBreakTimer );

    *KdDebuggerEnabled = TRUE;
    *KdDebuggerNotPresent = FALSE;
    *KdPitchDebugger = FALSE;

    *KdpDebugRoutineSelect = 1;

    SharedUserData->KdDebuggerEnabled = TRUE;
}

NTSTATUS
KdDriverLoad(
    _In_ PDRIVER_OBJECT  DriverObject,
    _In_ PUNICODE_STRING RegistryPath
)
{
    RegistryPath;

    PVOID          ImageBase;
    ULONG          ImageSize;
    PVOID          SectionBase;
    ULONG          SectionSize;
    ULONG_PTR      KdChangeOptionAddress;
    UNICODE_STRING KdChangeOptionName = RTL_CONSTANT_STRING( L"KdChangeOption" );
    ULONG_PTR      KdTrapAddress;
    LARGE_INTEGER  DueTime;

    //
    //////////////////////////////////////////////////////////////////////
    //      ALL NOTES ON KD DETECTION VECTORS AND KD RELATED SHIT.      //
    //////////////////////////////////////////////////////////////////////
    //
    // 1. Prcb->DebuggerSavedIRQL is never restored to 0, if KdEnterDebugger is called
    //    this field is set, and never zeroed.
    // 2. (HalPrivateDispatchTable->Fn[102]) xHalTimerWatchdogStop .data pointer is called 
    //    inside KdEnterDebugger and could be hooked.
    // 3. hypervisors could easily vmexit on any privileged instruction inside any Kd function
    //    and detect the presence of a debugger.
    // 4. KdpSendWaitContinue is the main loop for Kd, the main cross references for this are
    //    KdpReportCommandStringStateChange, 
    //    KdpReportExceptionStateChange, 
    //    KdpReportLoadSymbolsStateChange.
    // 5. Main notable reference for breakpoint handling is all references to KdTrap, this 
    //    eventually enters the debugger from KdpReportExceptionStateChange, if KdpDebugRoutineSelect is
    //    FALSE, then there are checks which may cause an issue with bp's because of enable checks because
    //    of KdpStub instead of KdpTrap, this var is controlled by Enable/Disable kd routines.
    //
    //
    // 1. KdpSendWaitContinue is only ever called under certain conditions, one of the issues with 
    //    this function, is that everything goes through it, and we are very capable of hooking it
    //    inside our dpc procedure, the main issue is with breakpoints and the KdpBreakpointTable.
    //    we need to figure some method to get around this table being checked by drivers, without
    //    losing the functionality.
    // 2. All Kd global vars indicate the presence of Kd, these variables can be discarded until a 
    //    breakpoint or something fires.
    // 3. Hooking KdpSendWaitContinue also fixes the problem of Kd being disabled when interrupt from
    //    somewhere away from our loop.
    // 4. Current thoughts are to use KdpDebugRoutineSelect to force KdpStub, and have the debugger disabled,
    //    such that no anti-debugger bp's are picked up. KdPitchDebugger can instantly cause it to ignore.
    // 5. When kd is not enabled by the bcd, kd.dll is loaded solely, when bcd instructs that there is
    //    another requested method for communication of kd and kd is enabled, kdcom.dll will be loaded and kd.dll
    //    no. My current idea is to change the name from kdcom.dll to kd.dll. TODO: verify the ntoskrnl imports from
    //    kdcom.dll and how they behave under different circumstances.

    DriverObject->DriverUnload = KdDriverUnload;

    ImageBase = NULL;
    ImageSize = 0;

    SectionBase = NULL;
    SectionSize = 0;

    DbgPrint( "KdImageAddress returned %lx %p %lx\n",
              KdImageAddress( "ntoskrnl.exe", &ImageBase, &ImageSize ),
              ImageBase,
              ImageSize );

    DbgPrint( "KdImageSection returned %lx %p %lx\n",
              KdImageSection( ImageBase, ".text\0\0", &SectionBase, &SectionSize ),
              SectionBase,
              SectionSize );

    KdChangeOptionAddress = ( ULONG_PTR )MmGetSystemRoutineAddress( &KdChangeOptionName );

    //                              KdChangeOption proc near
    //  0:  45 33 d2                xor    r10d, r10d
    //  3 : 44 38 15 xx xx xx xx    cmp    byte ptr [rip+KdPitchDebugger], r10b
    KdPitchDebugger = ( PBOOLEAN )( KdChangeOptionAddress + 10 + *( ULONG32* )( KdChangeOptionAddress + 6 ) );

    DbgPrint( "KdPitchDebugger at %p with value %d\n", KdPitchDebugger, *KdPitchDebugger );


    KdTrapAddress = ( ULONG_PTR )KdSearchSignature( ( PVOID )( ( ULONG_PTR )ImageBase + ( ULONG_PTR )SectionBase ),
                                                    SectionSize,
                                                    "48 83 EC 38 83 3D ? ? ? ? ? 8A 44 24 68" );

    KdpDebugRoutineSelect = ( PULONG )( KdTrapAddress + 11 + *( ULONG32* )( KdTrapAddress + 6 ) );

    DbgPrint( "KdTrapAddress at %p, KdpDebugRoutineSelect at %p with value %d\n", KdTrapAddress, KdpDebugRoutineSelect, *KdpDebugRoutineSelect );

    KeInitializeDpc( &KdBreakDpc,
        ( PKDEFERRED_ROUTINE )KdBreakTimerDpcCallback,
                     NULL );

    KeInitializeTimerEx( &KdBreakTimer, NotificationTimer );

    DueTime.QuadPart = -10000000;
    KeSetTimerEx( &KdBreakTimer, DueTime, 1000, &KdBreakDpc );

    //
    // Special code, affecting the debugger directly.
    //

    *KdDebuggerEnabled = FALSE;
    *KdDebuggerNotPresent = TRUE;
    *KdPitchDebugger = TRUE;

    *KdpDebugRoutineSelect = 0;

    //
    // Code that only gives an idea of the presence and can be safely removed.
    // without affecting the debugger.
    //

    SharedUserData->KdDebuggerEnabled = FALSE;

    return STATUS_SUCCESS;
}
