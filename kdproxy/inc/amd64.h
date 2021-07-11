
#pragma once

#pragma pack( push, 1 )

typedef union _KIDT_GATE {
    struct {
        ULONG64 OffsetPart0 : 16;
        ULONG64 Segment : 16;
        ULONG64 Ist : 3;
        ULONG64 Reserved : 5;
        ULONG64 Type : 4;
        ULONG64 Reserved1 : 1;
        ULONG64 PrivilegeLevel : 2;
        ULONG64 Present : 1;
        ULONG64 OffsetPart1 : 16;

        ULONG64 OffsetPart2 : 32;
        ULONG64 Reserved2 : 32;
    };
    struct {
        ULONG64 Long0;
        ULONG64 Long1;
    };
} KIDT_GATE, *PKIDT_GATE;

C_ASSERT( sizeof( KIDT_GATE ) == 16 );

typedef struct _KDESCRIPTOR_TABLE {
    USHORT  Limit;
    ULONG64 Base;
} KDESCRIPTOR_TABLE, *PKDESCRIPTOR_TABLE;

C_ASSERT( sizeof( KDESCRIPTOR_TABLE ) == 10 );

#pragma pack( pop )
