
#include "kd.h"

KD_PORT KdpPortDevice;
ULONG32 KdCompNumberRetries = 8;
ULONG32 KdCompRetryCount = 0;

ULONG32 KdCompNextPacketIdToSend;
ULONG32 KdCompPacketIdExpected;
ULONG32 KdCompTimeOuts = 0;
ULONG32 KdCompByteStalls = 0;

KD_STATUS
KdpSendControlPacket(
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
    return KdpSendString( &Packet, sizeof( KD_PACKET ) );
}

KD_STATUS
KdSendPacket(
    _In_ ULONG32     PacketType,
    _In_ PSTRING     MessageHeader,
    _In_ PSTRING     MessageData,
    _In_ PKD_CONTEXT Context
)
{
    Context;
    KD_PACKET Packet;

    NT_ASSERT( KeGetCurrentIrql( ) >= DISPATCH_LEVEL );

    Packet.Checksum = KdMessageChecksum( MessageHeader ) + KdMessageChecksum( MessageData );
    Packet.PacketLeader = 0x30303030;
    Packet.PacketLength = MessageHeader->Length + MessageData->Length;
    Packet.PacketType = ( USHORT )PacketType;

    KdpSendString( &Packet,
                   sizeof( KD_PACKET ) );
    KdpSendString( MessageHeader->Buffer,
                   MessageHeader->Length );
    if ( MessageData->Length ) {
        KdpSendString( MessageData->Buffer,
                       MessageData->Length );
    }

    return KdStatusOkay;
}

KD_STATUS
KdReceivePacket(
    _In_  ULONG32     PacketType,
    _Out_ PSTRING     Head,
    _Out_ PSTRING     Body,
    _Out_ PULONG32    Length,
    _In_  PKD_CONTEXT Context
)
{
    Body;
    Length;
    Context;

    KD_UART_STATUS UartStatus;
    UCHAR          Buffer[ sizeof( KD_PACKET ) ];
    ULONG          Index;
    PKD_PACKET     PacketBuffer = ( PKD_PACKET )&Buffer;

    NT_ASSERT( KeGetCurrentIrql( ) >= DISPATCH_LEVEL );
    NT_ASSERT( Head->MaximumLength >= sizeof( KD_PACKET ) );

    while ( 1 ) {

        Index = 0;

        //
        // Receive the KD_PACKET structure with the header.
        //

        //
        // Receive the PacketLeader.
        //

        do {
            UartStatus = KdpRecvString( Buffer + Index, 1 );

            if ( UartStatus == KdUartNoData &&
                 Index == 0 ) {

                // may consider adding some timing?
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

        KdpRecvString( &PacketBuffer->PacketType, 2 );

        if ( PacketBuffer->PacketLeader == KD_LEADER_CONTROL &&
             PacketBuffer->PacketType == KdTypeResend ) {

            return KdStatusResend;
        }

        KdpRecvString( &PacketBuffer->PacketLength, 2 );
        KdpRecvString( &PacketBuffer->PacketId, 4 );
        KdpRecvString( &PacketBuffer->Checksum, 4 );

#if 0
        //
        // Ignore checksum, we dont really care for now.
        //

        if ( PacketBuffer->PacketLength > sizeof( KD_PACKET ) ) {

            KdpRecvString( Head->Buffer + Index, PacketBuffer->PacketLength - sizeof( KD_PACKET ) );
    }
#endif

        //
        // PacketBuffer has our KD_PACKET Header
        //

        if ( PacketBuffer->PacketLeader != KD_LEADER_CONTROL ) {

            if ( PacketType == KdTypeAcknowledge ) {

                if ( PacketBuffer->PacketId != KdCompPacketIdExpected ) {

                    KdpSendControlPacket( KdTypeAcknowledge, PacketBuffer->PacketId );
                    continue;
                }
            }

            return KdStatusOkay;
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
                    return KdStatusOkay;
                }
            case KdTypeReset:
                KdCompNextPacketIdToSend = KD_PACKET_ID_RESET;
                KdCompPacketIdExpected = KD_PACKET_ID_RESET;
                KdpSendControlPacket( KdTypeReset, 0 );
                return KdStatusResend;
            case KdTypeResend:
            default:

                return KdStatusResend;
            }
        }

}

    return KdStatusTimeOut;

}

KD_UART_STATUS
KdpSendString(
    _In_ PVOID String,
    _In_ ULONG Length
)
{
    PUCHAR String1;

    String1 = String;

    while ( Length-- ) {

        NT_ASSERT( KdpPortDevice.Send( &KdpPortDevice, *String1 ) == KdUartSuccess );
        String1++;
    }

    return KdUartSuccess;
}

KD_UART_STATUS
KdpRecvString(
    _In_ PVOID String,
    _In_ ULONG Length
)
{

#if 0
    KD_UART_STATUS UartStatus;
    PUCHAR         String1;
    ULONG64        TimeStampOriginal;
    ULONG64        TimeStamp1;
    ULONG64        TimeStamp2;
    ULONG64        TimeStamp3;
    ULONG64        TimeStampDifferenceOriginal;
    ULONG64        TimeStampDifference;
    ULONG64        TimeStampDifferenceByMul;

    TimeStampDifference = 0;
    TimeStampDifferenceOriginal = 0;
    TimeStampDifferenceByMul = 0;

    //
    // This function inside kdcom.dll, has some microsoft
    // hypervisor specific stuff, such as HV_X64_MSR_TIME_REF_COUNT.
    // We don't implement that.
    //

    TimeStampOriginal = __rdtsc( );
    TimeStampDifferenceOriginal = 1;
    TimeStampDifferenceByMul = 0;

    String1 = String;

    while ( Length-- ) {

        UartStatus = KdpPortDevice.Recv( &KdpPortDevice, String1 );

        if ( *TimeOut == 0 ) {

            //HalPrivateDispatchTable[16], KdCheckPowerButton 
            KdCompTimeOuts++;
            break;
        }

        if ( *TimeOut == -1 ) {

            //HalPrivateDispatchTable[16], KdCheckPowerButton 
            KdCompByteStalls++;
            KeStallExecutionProcessor( 4 );
        }

        if ( TimeStampDifferenceByMul == 0 ) {

            TimeStamp1 = __rdtsc( );
            KeStallExecutionProcessor( 8 );
            TimeStamp2 = __rdtsc( );

            TimeStampDifference = ( TimeStamp2 - TimeStamp1 ) / 8;
            TimeStampDifferenceByMul = TimeStampDifference * *TimeOut + 1;

            if ( TimeStampDifference > TimeStampDifferenceOriginal ) {

                TimeStampDifferenceOriginal = TimeStampDifference;
            }
        }

        if ( __rdtsc( ) - TimeStampOriginal < TimeStampDifferenceByMul ) {

            KdCompByteStalls++;
            KeStallExecutionProcessor( 4 );
        }
        else {

            *TimeOut = 0;
        }

        String1++;
    }

    if ( ( unsigned int )( *TimeOut - 1 ) <= -3 && TimeStampDifferenceByMul ) {

        TimeStamp3 = ( __rdtsc( ) - TimeStampOriginal ) / TimeStampDifferenceOriginal;
        if ( TimeStamp3 < *TimeOut ) {

            *TimeOut = *TimeOut - TimeStamp3;
        }
        else {

            *TimeOut = 0;
        }

}
#endif

    KD_UART_STATUS UartStatus;
    PUCHAR         String1;

    String1 = String;

    while ( Length-- ) {

        UartStatus = KdpPortDevice.Recv( &KdpPortDevice, String1 );

        if ( UartStatus != KdUartSuccess ) {

            return UartStatus;
        }

        String1++;
    }

    return KdUartSuccess;
}
