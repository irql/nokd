
#include "kd.h"

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

VOID
KdLoadSystem(

)
{
    //
    // This shouldn't really be in this file, but I don't want
    // to move any of the definitions out of this local C file,
    // and I need to enumerate all process modules.
    //
    // TODO: Alternatively, you could use this to initialize the system
    //       ready for the debugger.
    //


    NTSTATUS             ntStatus;
    ULONG                Length;
    ULONG64              CurrentModule;
    PRTL_PROCESS_MODULES ModuleInformation;
    KD_SYMBOL_INFO       SymbolInfo;
    STRING               PathName;
    ULONG64              SmapFlag;

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
        return;
    }

    for ( CurrentModule = 0;
          CurrentModule < ModuleInformation->NumberOfModules;
          CurrentModule++ ) {

        if ( strcmp( ( PCHAR )ModuleInformation->Modules[ CurrentModule ].FullPathName +
                     ModuleInformation->Modules[ CurrentModule ].OffsetToFileName,
                     KD_FILE_NAME ) == 0 ) {

            continue;
        }

        if ( strcmp( ( PCHAR )ModuleInformation->Modules[ CurrentModule ].FullPathName +
                     ModuleInformation->Modules[ CurrentModule ].OffsetToFileName,
                     "ntoskrnl.exe" ) == 0 ) {

            continue;
        }

        SymbolInfo.ProcessId = 0x4;
        SymbolInfo.BaseAddress = ( ULONG64 )ModuleInformation->Modules[ CurrentModule ].ImageBase;
        SymbolInfo.SizeOfImage = ModuleInformation->Modules[ CurrentModule ].ImageSize;
        if ( MmIsAddressValid( ( PVOID )SymbolInfo.BaseAddress ) ) {

            SmapFlag = __readcr4( ) & ( 1ULL << 21 );
            __writecr4( __readcr4( ) & ~( 1ULL << 21 ) );

            __try {

                SymbolInfo.CheckSum = *( ULONG32* )( ( PUCHAR )( SymbolInfo.BaseAddress + *( LONG32* )( ( PCHAR )SymbolInfo.BaseAddress + 0x3c ) ) + 0x40 );
            }
            __except ( TRUE ) {

                SymbolInfo.CheckSum = 0;
            }

            __writecr4( __readcr4( ) | SmapFlag );
        }
        else {

            SymbolInfo.CheckSum = 0;
        }

        RtlInitString( &PathName, ( PCHAR )ModuleInformation->Modules[ CurrentModule ].FullPathName +
                       ModuleInformation->Modules[ CurrentModule ].OffsetToFileName );

        KdReportLoadSymbolsStateChange( &PathName,
                                        &SymbolInfo,
                                        FALSE );

#if 0
        //
        // TODO: This just calls into KdImageAddress which is a 
        //       copy of this function, to acquire address & size.
        //

        KdReportLoaded( ( PCHAR )( ModuleInformation->Modules[ CurrentModule ].FullPathName +
                                   ModuleInformation->Modules[ CurrentModule ].OffsetToFileName ),
                                   ( PCHAR )( ModuleInformation->Modules[ CurrentModule ].FullPathName +
                                              ModuleInformation->Modules[ CurrentModule ].OffsetToFileName ) );
#endif
    }

    ExFreePoolWithTag( ModuleInformation, 'dKoN' );
}
