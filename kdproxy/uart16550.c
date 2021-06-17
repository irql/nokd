
#include "kd.h"

KD_UART_STATUS
KdUart16550SendByte(
    _In_ PKD_PORT Port,
    _In_ UCHAR    Byte
);

KD_UART_STATUS
KdUart16550RecvByte(
    _In_ PKD_PORT Port,
    _In_ PUCHAR   Byte
);

#define COM_DATA_REG                0
#define COM_INTERRUPT_ENABLE_REG    1

#define COM_LSB_BAUD_RATE           0
#define COM_MSB_BAUD_RATE           1

#define COM_INT_IDENT_FIFO_CR       2
#define COM_LINE_CONTROL_REG        3 //MSB IS THE DLAB
#define COM_MODEM_CONTROL_REG       4
#define COM_LINE_STATUS_REG         5
#define COM_MODEM_STATUS_REG        6
#define COM_SCRATCH_REG             7

#define COM_LC_DLAB                 ( 1 << 7 )

#define COM_LS_DR                   ( 1 << 0 )
#define COM_LS_OE                   ( 1 << 0 )
#define COM_LS_PE                   ( 1 << 2 )
#define COM_LS_FE                   ( 1 << 3 )
#define COM_LS_BI                   ( 1 << 4 )
#define COM_LS_THRE                 ( 1 << 5 )
#define COM_LS_TEMT                 ( 1 << 6 )
#define COM_LS_ER_INP               ( 1 << 7 )

#define COM_LC_DB_5                 0
#define COM_LC_DB_6                 1
#define COM_LC_DB_7                 2
#define COM_LC_DB_8                 3

#define COM_LC_SB_1                 0
#define COM_LC_SB_2                 (1 << 2)

KD_UART_STATUS
KdUart16550InitializePort(
    _In_ PKD_PORT Port,
    _In_ ULONG64  Index
)
{
    Port->Uart.Index = Index;

    switch ( Port->Uart.Index ) {
    case 1:
        Port->Uart.Base = 0x3F8;
        break;
    case 2:
        Port->Uart.Base = 0x2F8;
    case 3:
        Port->Uart.Base = 0x3E8;
        break;
    case 4:
        Port->Uart.Base = 0x2E8;
        break;
    default:
        NT_ASSERT( FALSE );
    }

    __outbyte( Port->Uart.Base + COM_INTERRUPT_ENABLE_REG, 0 );

    __outbyte( Port->Uart.Base + COM_LINE_CONTROL_REG, COM_LC_DLAB );

    __outbyte( Port->Uart.Base + COM_LSB_BAUD_RATE, 1 );
    __outbyte( Port->Uart.Base + COM_MSB_BAUD_RATE, 0 );

    __outbyte( Port->Uart.Base + COM_LINE_CONTROL_REG, 3 );

    __outbyte( Port->Uart.Base + COM_INT_IDENT_FIFO_CR, 0xC7 );
    __outbyte( Port->Uart.Base + COM_MODEM_CONTROL_REG, 0xB );

    Port->Recv = KdUart16550RecvByte;
    Port->Send = KdUart16550SendByte;

    return KdUartSuccess;
}

BOOLEAN
KdUart16550SendReady(
    _In_ PKD_PORT Port
)
{
    return ( __inbyte( Port->Uart.Base + COM_LINE_STATUS_REG ) & COM_LS_THRE ) == COM_LS_THRE;
}

BOOLEAN
KdUart16550RecvReady(
    _In_ PKD_PORT Port
)
{
    return ( __inbyte( Port->Uart.Base + COM_LINE_STATUS_REG ) & COM_LS_DR ) == COM_LS_DR;
}

KD_UART_STATUS
KdUart16550SendByte(
    _In_ PKD_PORT Port,
    _In_ UCHAR    Byte
)
{
    ULONG64 TimeOut = 4000;

    while ( !KdUart16550SendReady( Port ) && TimeOut != 0 ) {

        TimeOut--;
    }

    if ( TimeOut > 0 ) {
        __outbyte( Port->Uart.Base + COM_DATA_REG, Byte );

        return KdUartSuccess;
    }
    else {

        return KdUartError;
    }
}

KD_UART_STATUS
KdUart16550RecvByte(
    _In_ PKD_PORT Port,
    _In_ PUCHAR   Byte
)
{
    ULONG64 TimeOut = 4000;

    DbgPrint( "begin recv! %lx\n", Port->Uart.Base );

    while ( !KdUart16550RecvReady( Port ) && TimeOut != 0 ) {

        TimeOut--;
    }

    if ( TimeOut > 0 ) {
        *Byte = __inbyte( Port->Uart.Base + COM_DATA_REG );

        return KdUartSuccess;
    }
    else {
        DbgPrint( "time out!\n" );

        return KdUartNoData;
    }
}
