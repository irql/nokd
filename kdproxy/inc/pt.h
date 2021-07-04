
#pragma once

//
// Taken from my carboot project.
// https://github.com/irql0/carboot/blob/master/examples/multi_basic/kernel/memory.h
//

typedef union _MMPTE_HARDWARE {
    struct {
        unsigned long long Present : 1;
        unsigned long long Write : 1;
        unsigned long long User : 1;
        unsigned long long WriteThrough : 1;
        unsigned long long CacheDisable : 1;
        unsigned long long Accessed : 1;
        unsigned long long Dirty : 1;
        unsigned long long Large : 1;
        unsigned long long Global : 1;
        unsigned long long Available0 : 3;
        unsigned long long PageFrame : 36;
        unsigned long long Reserved1 : 4;
        unsigned long long Available1 : 7;
        unsigned long long ProtectionKey : 4;
        unsigned long long ExecuteDisable : 1;
    } Table;

    struct {
        unsigned long long Present : 1;
        unsigned long long Write : 1;
        unsigned long long User : 1;
        unsigned long long WriteThrough : 1;
        unsigned long long CacheDisable : 1;
        unsigned long long Accessed : 1;
        unsigned long long Dirty : 1;
        unsigned long long PageAttribute : 1;
        unsigned long long Global : 1;
        unsigned long long Available0 : 3;
        unsigned long long PageFrame : 36;
        unsigned long long Reserved1 : 4;
        unsigned long long Available1 : 7;
        unsigned long long ProtectionKey : 4;
        unsigned long long ExecuteDisable : 1;
    } Entry;

    unsigned long long     Long;
} MMPTE_HARDWARE, *PMMPTE_HARDWARE;

C_ASSERT( sizeof( MMPTE_HARDWARE ) == 8 );

//
// Values for windows, I had no idea they hardcoded these.
//

#define MI_RECURSIVE_INDEX  493ULL
#define MI_RECURSIVE_TABLE  0xFFFFF68000000000ULL

#define MiReferenceLevel4Entry( index4 ) \
( ( PMMPTE_HARDWARE )( MI_RECURSIVE_TABLE | \
( MI_RECURSIVE_INDEX << 30ULL ) | \
( MI_RECURSIVE_INDEX << 21ULL ) | \
( ( ( unsigned long long ) ( index4 ) & 0x1FFULL ) << 12ULL) ) )

#define MiReferenceLevel3Entry( index4, index3 ) \
( ( PMMPTE_HARDWARE )( MI_RECURSIVE_TABLE | \
( MI_RECURSIVE_INDEX << 30ULL ) | \
( ( ( unsigned long long )( index4 ) & 0x1FFULL ) << 21ULL) | \
( ( ( unsigned long long )( index3 ) & 0x1FFULL ) << 12ULL) ) )

#define MiReferenceLevel2Entry( index4, index3, index2 ) \
( ( PMMPTE_HARDWARE )( MI_RECURSIVE_TABLE | \
( ( ( unsigned long long )( index4 ) & 0x1FFULL ) << 30ULL ) | \
( ( ( unsigned long long )( index3 ) & 0x1FFULL ) << 21ULL ) | \
( ( ( unsigned long long )( index2 ) & 0x1FFULL ) << 12ULL ) ) )

#define MiIndexLevel4( address )                ( ( ( unsigned long long ) ( address ) & ( 0x1FFULL << 39ULL ) ) >> 39ULL )
#define MiIndexLevel3( address )                ( ( ( unsigned long long ) ( address ) & ( 0x1FFULL << 30ULL ) ) >> 30ULL )
#define MiIndexLevel2( address )                ( ( ( unsigned long long ) ( address ) & ( 0x1FFULL << 21ULL ) ) >> 21ULL )
#define MiIndexLevel1( address )                ( ( ( unsigned long long ) ( address ) & ( 0x1FFULL << 12ULL ) ) >> 12ULL )

#define MiConstructAddress( index4, index3, index2, index1 ) \
( ( void* )( ( ( unsigned long long )( index4 ) << 39ULL ) |\
( ( unsigned long long )( index3 ) << 30ULL ) |\
( ( unsigned long long )( index2 ) << 21ULL ) |\
( ( unsigned long long )( index1 ) << 12ULL ) |\
( ( ( unsigned long long )( index4 ) / 256 ) * 0xFFFF000000000000 ) ) )

CFORCEINLINE
PMMPTE_HARDWARE
MiGetPteAddress(
    _In_ ULONG_PTR Address
)
{
    return &MiReferenceLevel2Entry( MiIndexLevel4( Address ),
                                    MiIndexLevel3( Address ),
                                    MiIndexLevel2( Address ) )[ MiIndexLevel1( Address ) ];
}

CFORCEINLINE
PMMPTE_HARDWARE
MiGetPdeAddress(
    _In_ ULONG_PTR Address
)
{
    return ( PMMPTE_HARDWARE )( ( ( Address >> 18 ) & 0x3FFFFFF8 ) + 0xFFFFF6FB40000000 );
    /*
    return &MiReferenceLevel3Entry( MiIndexLevel4( Address ),
                                    MiIndexLevel3( Address ) )[ MiIndexLevel2( Address ) ];*/
}

CFORCEINLINE
PMMPTE_HARDWARE
MiGetPpeAddress(
    _In_ ULONG_PTR Address
)
{
    return ( PMMPTE_HARDWARE )( ( ( Address >> 9 ) & 0x7FFFFFFFF8i64 ) + 0xFFFFF68000000000 );
    //return &MiReferenceLevel4Entry( MiIndexLevel4( Address ) )[ MiIndexLevel3( Address ) ];
}
