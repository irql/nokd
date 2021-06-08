
#pragma once

#include <ntifs.h>

typedef struct _KD_UART_CONTROL {
    ULONG64 Index;
    ULONG64 Base;
} KD_UART_CONTROL, *PKD_UART_CONTROL;

typedef enum _KD_UART_STATUS {
    KdUartSuccess,
    KdUartError,
    KdUartNoData

} KD_UART_STATUS, *PKD_UART_STATUS;

typedef struct _KD_PORT *PKD_PORT;

typedef struct _KD_PORT {

    KD_UART_CONTROL  Uart;
    KD_UART_STATUS( *Recv )(
        _In_ PKD_PORT Port,
        _In_ PUCHAR   Byte );
    KD_UART_STATUS( *Send )(
        _In_ PKD_PORT Port,
        _In_ UCHAR    Byte );

} KD_PORT, *PKD_PORT;

KD_UART_STATUS
KdUart16550InitializePort(
    _In_ PKD_PORT Port,
    _In_ ULONG64  Index
);

NTSTATUS
KdImageAddress(
    _In_      PCHAR  ImageName,
    _Out_opt_ PVOID* ImageBase,
    _Out_opt_ ULONG* ImageSize
);

NTSTATUS
KdImageSection(
    _In_      PVOID  ImageBase,
    _In_      PCHAR  SectionName,
    _Out_opt_ PVOID* SectionBase,
    _Out_opt_ ULONG* SectionSize
);

PVOID
KdSearchSignature(
    _In_ PVOID BaseAddress,
    _In_ ULONG Length,
    _In_ PCHAR Signature
);
