﻿
#include <kd.h>
#include <uart.h>

ULONG32 KdCompNumberRetries = 8;
ULONG32 KdCompRetryCount = 0;

ULONG32 KdCompNextPacketIdToSend;
ULONG32 KdCompPacketIdExpected;
ULONG32 KdCompTimeOuts = 0;
ULONG32 KdCompByteStalls = 0;

//https://en.wikibooks.org/wiki/Serial_Programming/8250_UART_Programming



#if 0

//
// TODO: Add a function to query which UART
//       version this actually is. i.e: 8250, 16550, 16750, etc.
//

PWSTR
KdpIdentifyUart(
    _In_ PKD_DEBUG_DEVICE DebugDevice
)
{
    __outbyte( DebugDevice->Uart.Base + COM_FCR, 0xE7 );

    return NULL;
}
#endif

NTSTATUS
KdUartInitialize(

)
{
    if ( !NT_SUCCESS( KdUartInitializePort( &KdDebugDevice, 1 ) ) &&
         !NT_SUCCESS( KdUartInitializePort( &KdDebugDevice, 2 ) ) &&
         !NT_SUCCESS( KdUartInitializePort( &KdDebugDevice, 3 ) ) &&
         !NT_SUCCESS( KdUartInitializePort( &KdDebugDevice, 4 ) ) ) {

        return STATUS_UNSUCCESSFUL;
    }

    return STATUS_SUCCESS;
}

NTSTATUS
KdUartInitializePort(
    _In_ PKD_DEBUG_DEVICE DebugDevice,
    _In_ ULONG64          Index
)
{
    UCHAR FaultByte;

    DebugDevice->Uart.Index = Index;

    switch ( DebugDevice->Uart.Index ) {
    case 1:
        DebugDevice->Uart.Base = 0x3F8;
        break;
    case 2:
        DebugDevice->Uart.Base = 0x2F8;
        break;
    case 3:
        DebugDevice->Uart.Base = 0x3E8;
        break;
    case 4:
        DebugDevice->Uart.Base = 0x2E8;
        break;
    default:
        NT_ASSERT( FALSE );
    }

    //
    // No interrupts should be generated by the chip,
    // data will be polled.
    //

    __outbyte( DebugDevice->Uart.Base + COM_IER, 0 );

    //
    // Set the DLAB to access to DL bytes.
    //

    __outbyte( DebugDevice->Uart.Base + COM_LCR, 0x80 );

    //
    // Set the divisor latch to 1. 
    // 115200/1 = 115200.
    //

    __outbyte( DebugDevice->Uart.Base + COM_DLL, 0x01 );
    __outbyte( DebugDevice->Uart.Base + COM_DLH, 0x00 );

    //
    // Clear the DLAB, set 8 bits, no parity and one stop bit.
    //

    __outbyte( DebugDevice->Uart.Base + COM_LCR, 0x03 );

    //
    // Enable FIFOs and clear receive/transmit FIFO.
    //

    __outbyte( DebugDevice->Uart.Base + COM_FCR, 0xC7 );//0x03 );

    //
    // Request to send, data terminal ready (RTS/DTR).
    //

    __outbyte( DebugDevice->Uart.Base + COM_MCR, 0x0B );

    //
    // Put the chip in loopback mode and send 0b10101110,
    // then query the RBR and check data was received 
    // successfully, this just checks if the chip is faulty
    // or not.
    //

    __outbyte( DebugDevice->Uart.Base + COM_MCR, 0x1E );
    __outbyte( DebugDevice->Uart.Base + COM_THR, 0xAE );

    //if ( __inbyte( DebugDevice->Uart.Base + COM_RBR ) != 0xAE ) {

    if ( KdUartRecvByte( &DebugDevice->Uart, &FaultByte ) != KdStatusSuccess ||
         FaultByte != 0xAE ) {

        return STATUS_UNSUCCESSFUL;
    }

    __outbyte( DebugDevice->Uart.Base + COM_MCR, 0x0F );

    DebugDevice->KdSendPacket = KdUartSendPacket;
    DebugDevice->KdReceivePacket = KdUartRecvPacket;

    return STATUS_SUCCESS;
}

BOOLEAN
KdUartSendReady(
    _In_ PKD_UART_CONTROL Uart
)
{
    return ( __inbyte( Uart->Base + COM_LSR ) & COM_LS_THRE ) == COM_LS_THRE;
}

BOOLEAN
KdUartRecvReady(
    _In_ PKD_UART_CONTROL Uart
)
{
    return ( __inbyte( Uart->Base + COM_LSR ) & COM_LS_DR ) == COM_LS_DR;
}

KD_STATUS
KdUartSendByte(
    _In_ PKD_UART_CONTROL Uart,
    _In_ UCHAR            Byte
)
{
    ULONG64 TimeOut = 5000;

    while ( !KdUartSendReady( Uart ) && TimeOut != 0 ) {

        KeStallExecutionProcessor( 100 );
        TimeOut--;
    }

    if ( TimeOut > 0 ) {
        __outbyte( Uart->Base + COM_THR, Byte );

        return KdStatusSuccess;
    }
    else {

        return KdStatusTimeOut;
    }
}

KD_STATUS
KdUartRecvByte(
    _In_ PKD_UART_CONTROL Uart,
    _In_ PUCHAR           Byte
)
{
    ULONG64 TimeOut = 5000;

    while ( !KdUartRecvReady( Uart ) && TimeOut != 0 ) {

        KeStallExecutionProcessor( 100 );
        TimeOut--;
    }

    if ( TimeOut > 0 ) {
        *Byte = __inbyte( Uart->Base + COM_RBR );

        return KdStatusSuccess;
    }
    else {

        return KdStatusTimeOut;
    }
}

KD_STATUS
KdUartRecvString(
    _In_ PKD_DEBUG_DEVICE DebugDevice,
    _In_ PVOID            String,
    _In_ ULONG            Length
)
{
    PUCHAR String1;

    String1 = String;

    while ( Length-- ) {

        if ( KdUartRecvByte( &DebugDevice->Uart, String1 ) != KdStatusSuccess ) {

            return KdStatusTimeOut;
        }

        String1++;
    }

    return KdStatusSuccess;
}

KD_STATUS
KdUartSendString(
    _In_ PKD_DEBUG_DEVICE DebugDevice,
    _In_ PVOID            String,
    _In_ ULONG            Length
)
{
    PUCHAR String1;

    String1 = String;

    while ( Length-- ) {

        if ( KdUartSendByte( &DebugDevice->Uart, *String1 ) != KdStatusSuccess ) {

            return KdStatusError;
        }

        String1++;
    }

    return KdStatusSuccess;
}

KD_STATUS
KdUart16550SendControlPacket(
    _In_ ULONG32 PacketType,
    _In_ ULONG32 PacketId
)
{
    KD_PACKET Packet;

    Packet.PacketType = ( USHORT )PacketType;
    Packet.PacketLeader = KD_LEADER_CONTROL;
    Packet.PacketLength = 0;
    Packet.Checksum = 0;
    Packet.PacketId = PacketId;
    return KdUartSendString( &KdDebugDevice, &Packet, sizeof( KD_PACKET ) );
}

KD_STATUS
KdUartSendPacket(
    _In_     KD_PACKET_TYPE PacketType,
    _In_     PSTRING        Head,
    _In_opt_ PSTRING        Body,
    _Inout_  PKD_CONTEXT    KdContext
)
{
    KdContext;
    KD_PACKET Packet;

    NT_ASSERT( KeGetCurrentIrql( ) >= DISPATCH_LEVEL );

    Packet.Checksum = KdMessageChecksum( Head ) + KdMessageChecksum( Body );
    Packet.PacketLeader = 0x30303030;
    Packet.PacketLength = Head->Length + Body->Length;
    Packet.PacketType = ( USHORT )PacketType;

    KdUartSendString( &KdDebugDevice,
                      &Packet,
                      sizeof( KD_PACKET ) );
    KdUartSendString( &KdDebugDevice,
                      Head->Buffer,
                      Head->Length );
    if ( Body->Length ) {
        KdUartSendString( &KdDebugDevice,
                          Body->Buffer,
                          Body->Length );
    }

    return KdStatusSuccess;
}

KD_STATUS
KdUartRecvPacket(
    _In_    KD_PACKET_TYPE PacketType,
    _Inout_ PSTRING        Head,
    _Inout_ PSTRING        Body,
    _Out_   PULONG32       Length,
    _Inout_ PKD_CONTEXT    KdContext
)
{
    Body;
    Length;
    KdContext;

    KD_STATUS  Status;
    UCHAR      Buffer[ sizeof( KD_PACKET ) ];
    ULONG      Index;
    PKD_PACKET PacketBuffer = ( PKD_PACKET )&Buffer;

    NT_ASSERT( KeGetCurrentIrql( ) >= DISPATCH_LEVEL );

    while ( 1 ) {

        Index = 0;

        //
        // Receive the KD_PACKET structure with the header.
        //

        //
        // Receive the PacketLeader.
        //

        do {
            Status = KdUartRecvString( &KdDebugDevice, Buffer + Index, 1 );

            if ( Status != KdStatusSuccess ) {

                return KdStatusTimeOut;
            }

            switch ( Buffer[ Index ] ) {
            case KD_LEADER_BREAK_IN_BYTE:
            case KD_LEADER_PACKET_BYTE:
            case KD_LEADER_CONTROL_BYTE:

                if ( Index > 0 && Buffer[ Index ] != Buffer[ 0 ] ) {

                    Index = 0;
                }
                else {

                    Index++;
                }

                break;
            default:

                Index = 0;
                break;
            }

        } while ( Index < 4 );

        KdUartRecvString( &KdDebugDevice, &PacketBuffer->PacketType, 2 );

        if ( PacketBuffer->PacketLeader == KD_LEADER_CONTROL &&
             PacketBuffer->PacketType == KdTypeResend ) {

            return KdStatusResend;
        }

        KdUartRecvString( &KdDebugDevice, &PacketBuffer->PacketLength, 2 );
        KdUartRecvString( &KdDebugDevice, &PacketBuffer->PacketId, 4 );
        KdUartRecvString( &KdDebugDevice, &PacketBuffer->Checksum, 4 );

#if 0
        //
        // Ignore checksum, we dont really care for now.
        //

        if ( PacketBuffer->PacketLength > sizeof( KD_PACKET ) ) {

            KdUartRecvString( &KdDebugDevice,
                              Head->Buffer + Index,
                              PacketBuffer->PacketLength - sizeof( KD_PACKET ) );
        }
#endif

        if ( Head != NULL && Head->MaximumLength >= sizeof( KD_PACKET ) ) {

            PacketBuffer->PacketLength -= sizeof( KD_PACKET );
            if ( PacketBuffer->PacketLength > 0 &&
                 Head->MaximumLength > sizeof( KD_PACKET ) ) {

                KdUartRecvString( &KdDebugDevice,
                                  Head->Buffer,
                                  Head->Length );
                PacketBuffer->PacketLength -= Head->Length;

                if ( Body != NULL ) {

                    KdUartRecvString( &KdDebugDevice,
                                      Body->Buffer,
                                      Body->Length );
                    PacketBuffer->PacketLength -= Body->Length;

                    if ( PacketBuffer->PacketLength != 0 )
                        DbgPrint( "NON ZERO!" );
                }

            }
        }

        //
        // PacketBuffer has our KD_PACKET Header
        //

        if ( PacketBuffer->PacketLeader != KD_LEADER_CONTROL ) {

            if ( PacketType == KdTypeAcknowledge ) {

                if ( PacketBuffer->PacketId != KdCompPacketIdExpected ) {

                    KdUart16550SendControlPacket( KdTypeAcknowledge, PacketBuffer->PacketId );
                    continue;
                }
            }

            if ( PacketBuffer->PacketId != KdCompPacketIdExpected ) {

                // cry about it.
            }

            KdUart16550SendControlPacket( KdTypeAcknowledge, PacketBuffer->PacketId );
            KdCompPacketIdExpected ^= 1;

            return KdStatusSuccess;
        }
        else {

            switch ( PacketBuffer->PacketType ) {
            case KdTypeAcknowledge:
                if ( PacketBuffer->PacketId != ( KdCompNextPacketIdToSend & ~0x800 ) ||
                     PacketType != KdTypeAcknowledge ) {

                    return KdStatusResend;
                }
                else {

                    KdCompNextPacketIdToSend ^= 1;
                    return KdStatusSuccess;
                }
            case KdTypeReset:
                KdCompNextPacketIdToSend = KD_PACKET_ID_RESET;
                KdCompPacketIdExpected = KD_PACKET_ID_RESET;
                KdUart16550SendControlPacket( KdTypeReset, 0 );
                return KdStatusResend;
            case KdTypeResend:
            default:

                return KdStatusResend;
            }
        }

    }

    return KdStatusTimeOut;
}
