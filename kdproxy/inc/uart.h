
#pragma once

#define COM_THR       0
#define COM_RBR       0
#define COM_DLL       0

#define COM_IER       1
#define COM_DLH       1

#define COM_FCR       2
#define COM_IIR       2
#define COM_LCR       3
#define COM_MCR       4
#define COM_LSR       5
#define COM_MSR       6
#define COM_SCR       7

#define COM_LS_DR                   ( 1 << 0 )
#define COM_LS_OE                   ( 1 << 0 )
#define COM_LS_PE                   ( 1 << 2 )
#define COM_LS_FE                   ( 1 << 3 )
#define COM_LS_BI                   ( 1 << 4 )
#define COM_LS_THRE                 ( 1 << 5 )
#define COM_LS_TEMT                 ( 1 << 6 )
#define COM_LS_ER_INP               ( 1 << 7 )

KD_STATUS
KdUart16550RecvString(
    _In_ PKD_DEBUG_DEVICE DebugDevice,
    _In_ PVOID            String,
    _In_ ULONG            Length
);

KD_STATUS
KdUart16550SendString(
    _In_ PKD_DEBUG_DEVICE DebugDevice,
    _In_ PVOID            String,
    _In_ ULONG            Length
);

NTSTATUS
KdUart16550InitializePort(
    _In_ PKD_DEBUG_DEVICE DebugDevice,
    _In_ ULONG64          Index
);

KD_STATUS
KdUart16550RecvByte(
    _In_ PKD_UART_CONTROL Uart,
    _In_ PUCHAR           Byte
);

NTSTATUS
KdUart16550Initialize(
    _In_ PKD_DEBUG_DEVICE Port
);
