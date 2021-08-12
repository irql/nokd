
#include <kdgdb.h>
#include <winhttp.h>

//
// Most of this code is completely ripped from 
// https://github.com/irql0/carbon/kd/kdwin32/pdb.cc
//

VOID
DbgPdbBuildSymbolUrl(
    _Out_ PWSTR    Url,
    _In_  PCSTR    FileName,
    _In_  LPGUID   UniqueId,
    _In_  ULONG32  Age
)
{
    //L"/download/symbols/ntkrnlmp.pdb/51E2A8DACCB0D928DD0BE3C3599542351/ntkrnlmp.pdb"

    // no %llx : (
    wsprintfW( Url, L"/download/symbols/%S/" L"%04lX" L"%02lX" L"%02lX"

               L"%02X"
               L"%02X"
               L"%02X"
               L"%02X"
               L"%02X"
               L"%02X"
               L"%02X"
               L"%02X"

               L"%d" L"/%S",
               FileName,
               UniqueId->Data1,
               ( DWORD )UniqueId->Data2,
               ( DWORD )UniqueId->Data3,
               UniqueId->Data4[ 0 ],
               UniqueId->Data4[ 1 ],
               UniqueId->Data4[ 2 ],
               UniqueId->Data4[ 3 ],
               UniqueId->Data4[ 4 ],
               UniqueId->Data4[ 5 ],
               UniqueId->Data4[ 6 ],
               UniqueId->Data4[ 7 ],
               Age,
               FileName );
}

VOID
DbgPdbBuildSymbolFile(
    _Out_ PWSTR    File,
    _In_  PCSTR    FileName,
    _In_  LPGUID   UniqueId,
    _In_  ULONG32  Age
)
{
    //
    // This function just assumes you plan on writing a file, 
    // if one doesn't exist and so it creates the directories
    //

    WCHAR CurrentDirectory[ 256 ];

    //
    // TODO: Query _NT_SYMBOL_PATH
    //

    GetCurrentDirectoryW( 256, CurrentDirectory );

    // no %llx : (

    wsprintfW( File, L"%s\\%S",
               CurrentDirectory,
               FileName );
    CreateDirectoryW( File, NULL );

    wsprintfW( File, L"%s\\" L"%04lX" L"%02lX" L"%02lX"

               L"%02X"
               L"%02X"
               L"%02X"
               L"%02X"
               L"%02X"
               L"%02X"
               L"%02X"
               L"%02X"

               L"%d",
               File,
               UniqueId->Data1,
               ( DWORD )UniqueId->Data2,
               ( DWORD )UniqueId->Data3,
               UniqueId->Data4[ 0 ],
               UniqueId->Data4[ 1 ],
               UniqueId->Data4[ 2 ],
               UniqueId->Data4[ 3 ],
               UniqueId->Data4[ 4 ],
               UniqueId->Data4[ 5 ],
               UniqueId->Data4[ 6 ],
               UniqueId->Data4[ 7 ],
               Age );
    CreateDirectoryW( File, NULL );

    wsprintfW( File, L"%s\\%S",
               File,
               FileName );

#if 0
    wsprintfW( File, L"%s\\%S\\" L"%04lX" L"%02lX" L"%02lX" L"%08p" L"%d" L"\\%S",
               CurrentDirectory,
               FileName,
               UniqueId->Data1,
               ( DWORD )UniqueId->Data2,
               ( DWORD )UniqueId->Data3,
               *( ULONG64* )&UniqueId->Data4,
               Age,
               FileName );
#endif
}

HRESULT
DbgPdbDownload(
    _In_  PWSTR    Url,
    _Out_ PVOID    Buffer,
    _Out_ PULONG32 Length
)
{
    //
    // This is a little shitty for getting the size of the 
    // file/querying headers but it works.
    //

    HINTERNET Session;
    HINTERNET Connect;
    HINTERNET Request;
    DWORD     Read;
    DWORD     ReadLength;
    DWORD     Available;
    DWORD     HeaderLength;
    PVOID     HeaderBuffer;

    *Length = 0;

    Session = WinHttpOpen( L"WinHTTP Example/1.0",
                           WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                           WINHTTP_NO_PROXY_NAME,
                           WINHTTP_NO_PROXY_BYPASS,
                           0 );
    if ( Session == NULL ) {


    }

    Connect = WinHttpConnect( Session,
                              L"msdl.microsoft.com",
                              INTERNET_DEFAULT_HTTPS_PORT,
                              0 );
    if ( Connect == NULL ) {


    }

    Request = WinHttpOpenRequest( Connect,
                                  L"GET",
                                  Url,
                                  NULL,
                                  WINHTTP_NO_REFERER,
                                  WINHTTP_DEFAULT_ACCEPT_TYPES,
                                  WINHTTP_FLAG_SECURE );
    if ( Request == NULL ) {


    }

    WinHttpSendRequest( Request,
                        WINHTTP_NO_ADDITIONAL_HEADERS,
                        0,
                        WINHTTP_NO_REQUEST_DATA,
                        0,
                        0,
                        0 );

    WinHttpReceiveResponse( Request,
                            NULL );

    WinHttpQueryHeaders( Request,
                         WINHTTP_QUERY_CONTENT_LENGTH,
                         WINHTTP_HEADER_NAME_BY_INDEX,
                         NULL,
                         &HeaderLength,
                         WINHTTP_NO_HEADER_INDEX );

    HeaderBuffer = HeapAlloc( GetProcessHeap( ), 0, HeaderLength );

    WinHttpQueryHeaders( Request,
                         WINHTTP_QUERY_CONTENT_LENGTH,
                         WINHTTP_HEADER_NAME_BY_INDEX,
                         HeaderBuffer,
                         &HeaderLength,
                         WINHTTP_NO_HEADER_INDEX );

    *Length = ( ULONG32 )wcstoull( HeaderBuffer, NULL, 10 );

    if ( Buffer != NULL ) {

        ReadLength = 0;

        do {

            WinHttpQueryDataAvailable( Request,
                                       &Available );

            WinHttpReadData( Request,
                ( PUCHAR )Buffer + ReadLength,
                             Available,
                             &Read );
            ReadLength += Read;

        } while ( Available != 0 );
    }

    HeapFree( GetProcessHeap( ), 0, HeaderBuffer );

    WinHttpCloseHandle( Request );
    WinHttpCloseHandle( Connect );
    WinHttpCloseHandle( Session );

    return S_OK;
}

HRESULT
DbgPdbLoad(
    _In_  PWSTR           FileName,
    _Out_ PDBGPDB_CONTEXT Context
)
{

    HRESULT hResult;

    wcscpy( Context->FileName, FileName );

    hResult = CoInitialize( NULL );
    hResult = CoCreateInstance( &CLSID_DiaSource,
                                NULL,
                                CLSCTX_INPROC_SERVER,
                                &IID_IDiaDataSource,
                                ( LPVOID* )&Context->Source );

    if ( hResult != S_OK ) {

        //
        // regsvr32.exe bin/amd64/msdia140.dll
        //

        return hResult;
    }

    hResult = Context->Source->lpVtbl->loadDataFromPdb( Context->Source, Context->FileName );
    hResult = Context->Source->lpVtbl->openSession( Context->Source, &Context->Session );
    hResult = Context->Session->lpVtbl->get_globalScope( Context->Session, &Context->Global );

    return hResult;
}

HRESULT
DbgPdbAddressOfName(
    _In_  PDBGPDB_CONTEXT Context,
    _In_  PCWSTR          Name,
    _Out_ PULONG          Rva
)
{
    HRESULT          hResult;
    IDiaEnumSymbols* EnumSymbols;
    IDiaSymbol*      Symbol;
    ULONG            Celt;
    ULONG            LocationType;

    //
    // This is a little wrong because its using SymTagPublicSymbol, 
    // typically you use SymTagData but ntoskrnl is a bit meaty and stripped.
    //

    Celt = 0;
    hResult = Context->Global->lpVtbl->findChildren( Context->Global,
                                                     SymTagPublicSymbol,
                                                     Name,
                                                     nsCaseInRegularExpression,
                                                     &EnumSymbols );
    if ( hResult != S_OK ) {

        return hResult;
    }

    hResult = EnumSymbols->lpVtbl->Next( EnumSymbols, 1, &Symbol, &Celt );

    if ( hResult != S_OK || Celt != 1 ) {

        EnumSymbols->lpVtbl->Release( EnumSymbols );
        return hResult;
    }

    if ( Symbol->lpVtbl->get_locationType( Symbol, &LocationType ) == S_OK ) {

        switch ( LocationType ) {
        case LocIsStatic:
        case LocIsTLS:
        case LocInMetaData:
        case LocIsIlRel:

            return Symbol->lpVtbl->get_relativeVirtualAddress( Symbol, Rva );
        default:
            break;
        }
    }

    return ( HRESULT )-1;
}

//
// This function purely exists because when I was debugging this,
// I had to print out all the addresses which were being read by windbg.
//

HRESULT
DbgPdbNameOfAddress(
    _In_  PDBGPDB_CONTEXT Context,
    _In_  ULONG32         Address,
    _Out_ PWSTR*          Name,
    _Out_ PLONG32         Disp
)
{
    HRESULT     hResult;
    IDiaSymbol* Symbol;

    hResult = Context->Session->lpVtbl->findSymbolByRVAEx( Context->Session,
                                                           Address,
                                                           SymTagNull,
                                                           &Symbol,
                                                           Disp );
    if ( hResult != S_OK ) {

        return hResult;
    }

    hResult = Symbol->lpVtbl->get_name( Symbol, Name );
    if ( hResult != S_OK ) {

        Symbol->lpVtbl->Release( Symbol );
        return hResult;
    }

    return S_OK;
}

HRESULT
DbgPdbFieldOffset(
    _In_  PDBGPDB_CONTEXT Context,
    _In_  PCWSTR          TypeName,
    _In_  PCWSTR          FieldName,
    _Out_ PLONG32         FieldOffset
)
{
    //
    // TODO: Make this function recursive (like -b, no pointer expansion)
    //

    HRESULT          hResult;
    IDiaEnumSymbols* EnumSymbols;
    IDiaEnumSymbols* EnumChildren;
    IDiaSymbol*      Symbol;
    IDiaSymbol*      Child;
    ULONG            Celt;
    BSTR             Name;

    Celt = 0;
    hResult = Context->Global->lpVtbl->findChildren( Context->Global,
                                                     SymTagUDT,
                                                     TypeName,
                                                     nsCaseInsensitive,
                                                     &EnumSymbols );

    //
    // Assume it's the first found.
    //

    hResult = EnumSymbols->lpVtbl->Next( EnumSymbols, 1, &Symbol, &Celt );

    //
    // Take a look into get_symTag
    //

    Symbol->lpVtbl->findChildren( Symbol, SymTagNull, NULL, nsNone, &EnumChildren );

    while ( EnumChildren->lpVtbl->Next( EnumChildren, 1, &Child, &Celt ) == S_OK &&
            Celt == 1 ) {

        Child->lpVtbl->get_name( Child, &Name );
        Child->lpVtbl->get_offset( Child, FieldOffset );

        if ( lstrcmpiW( Name, FieldName ) == 0 ) {

            Child->lpVtbl->Release( Child );
            return S_OK;
        }

        Child->lpVtbl->Release( Child );
    }

    Symbol->lpVtbl->Release( Symbol );
    EnumSymbols->lpVtbl->Release( EnumSymbols );

    return ( HRESULT )-1;
}
