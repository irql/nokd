
#include <kdpl.h>

#define KeInPort8                   __inbyte
#define KeOutPort8                  __outbyte

#define IOCOM_THR 0 // Transmitter Holding Buffer
#define IOCOM_RBR 0 // Receiver Buffer

#define IOCOM_DLL 0 // Divisor Latch Low Byte
#define IOCOM_IER 1 // Interrupt Enable Register
#define IOCOM_DLH 1 // Divisor Latch High Byte
#define IOCOM_FCR 2 // FIFO Control Register
#define IOCOM_IIR 2 // Interrupt Identification Register
#define IOCOM_LCR 3 // Line Control Register
#define IOCOM_MCR 4 // Modem Control Register
#define IOCOM_LSR 5 // Line Status Register
#define IOCOM_MSR 6 // Modem Status Register
#define IOCOM_SR  7 // Scratch Register

#define IOCOM_LC_SBE (1 << 6) // Set Break Enable
#define IOCOM_LC_DLA (1 << 7) // Divisor Latch Access

#define IOCOM_LS_DR     (1 << 0)
#define IOCOM_LS_OE     (1 << 0)
#define IOCOM_LS_PE     (1 << 2)
#define IOCOM_LS_FE     (1 << 3)
#define IOCOM_LS_BI     (1 << 4)
#define IOCOM_LS_THRE   (1 << 5)
#define IOCOM_LS_TEMT   (1 << 6)
#define IOCOM_LS_ER_INP (1 << 7)

#define IOCOM_LC_DB_5 0
#define IOCOM_LC_DB_6 1
#define IOCOM_LC_DB_7 2
#define IOCOM_LC_DB_8 3

#define IOCOM_LC_SB_1 0
#define IOCOM_LC_SB_2 (1 << 2)

#define IOCOM_FC_64BYTE (1 << 5) // (16750) Enable 64 byte FIFO
#define IOCOM_FC_DMA    (1 << 3) // DMA mode select
#define IOCOM_FC_CT     (1 << 2) // Clear transmit
#define IOCOM_FC_CR     (1 << 1) // Clear receive
#define IOCOM_FC_EF     (1 << 0) // Enable FIFO

#define IOCOM_MC_AUTOFLOW (1 << 5) // (16750)
#define IOCOM_MC_LOOP     (1 << 4) // Loopback mode
#define IOCOM_MC_OUT2     (1 << 3) // Auxiliary Output 2
#define IOCOM_MC_OUT1     (1 << 2) // Auxiliary Output 1
#define IOCOM_MC_RTS      (1 << 1) // Request to send
#define IOCOM_MC_DTS      (1 << 0) // Data terminal ready (DTR, no??)

//
// Default port is 0x3F8 / COM0. 
//
USHORT  KdpComPort = 0x3F8; 
BOOLEAN KdpComOpen = FALSE;

ULONG32 KdpComBaudRate = 115200;

BOOLEAN
KdpComSendReady(
    VOID
    )
{
    return (KeInPort8(KdpComPort + IOCOM_LSR) & IOCOM_LS_THRE) == IOCOM_LS_THRE;
}

BOOLEAN
KdpComReceiveReady(
    VOID
    )
{
    return (KeInPort8(KdpComPort + IOCOM_LSR) & IOCOM_LS_DR) == IOCOM_LS_DR;
}

BOOLEAN
KdpComSendChar(
    _In_ UCHAR Char
    )
{
    ULONG64 Timeout;

    Timeout = 2 * (40000000000ull / KdpComBaudRate);

    while (Timeout > 0 && !KdpComSendReady())
        Timeout--;

    if (Timeout == 0) {
        return FALSE;
    }

    KeOutPort8(KdpComPort + IOCOM_THR, Char);
    return TRUE;
}

BOOLEAN
KdpComReceiveChar(
    _Out_ PUCHAR Char
    )
{
    ULONG64 Timeout;

    Timeout = 2 * (40000000000ull / KdpComBaudRate);

    while (Timeout > 0 && !KdpComReceiveReady())
        Timeout--;

    if (Timeout == 0) {
        return FALSE;
    }

    *Char = KeInPort8(KdpComPort + IOCOM_RBR);
    return TRUE;
}

BOOLEAN
KdpComSendString(
    _In_ PUCHAR  Buffer,
    _In_ ULONG32 Length
    )
{
    ULONG32 Tries;

    Tries = 20;

    while (Length--) {

        while (Tries > 0 && !KdpComSendChar(*Buffer)) {
            Tries--;
        }

        if (Tries == 0) {
            return FALSE;
        }

        Buffer += 1;
        Tries   = 20;
    }
    return TRUE;
}

BOOLEAN
KdpComReceiveString(
    _In_ PUCHAR  Buffer,
    _In_ ULONG32 Length
    )
{
    ULONG32 Tries;

    Tries = 20;

    while (Length--) {

        while (Tries > 0 && !KdpComReceiveChar(Buffer)) {
            Tries--;
        }

        if (Tries == 0) {
            return FALSE;
        }

        Buffer += 1;
        Tries   = 20;
    }
    return TRUE;
}

NTSTATUS
KdpComLoadDriver(
    VOID
    )
{
    //
    // References: 
    // https://en.wikibooks.org/wiki/Serial_Programming/8250_UART_Programming
    // https://www.latticesemi.com/-/media/LatticeSemi/Documents/ReferenceDesigns/SZ/UART16550Transceiver-Documentation.ashx?document_id=48168
    // https://www.nxp.com/docs/en/data-sheet/SC16C750B.pdf
    //

    UCHAR Lcr;
    UCHAR Mcr;
    UCHAR Msr;
    UCHAR Char;
    //
    // 7.10 SC16C750B defines the reset conditions.
    //

    Mcr = KeInPort8(KdpComPort + IOCOM_MCR);

    KeOutPort8(KdpComPort + IOCOM_MCR, IOCOM_MC_LOOP);
    KeOutPort8(KdpComPort + IOCOM_MCR, IOCOM_MC_LOOP);

    Msr = KeInPort8(KdpComPort + IOCOM_MSR);

    if ((Msr & (0x10 | 0x20 | 0x40 | 0x80)) == 0) {
        
        KeOutPort8(KdpComPort + IOCOM_MCR, IOCOM_MC_LOOP | IOCOM_MC_OUT1);

        Msr = KeInPort8(KdpComPort + IOCOM_MSR);

        KeOutPort8(KdpComPort + IOCOM_MCR, Mcr);

        if (Msr & 0x40) {
            KdpComOpen = TRUE;
        }
        else {
            KdpComOpen = FALSE;

            Char = 0;
            do {
                KeOutPort8(KdpComPort + 0x14, Char);

                if (KeInPort8(KdpComPort + 0x14) != Char) {
                    return STATUS_UNSUCCESSFUL;
                }
            } while (++Char != 0);

            KdpComOpen = TRUE;
        }
    }

    //
    // Disable device interrupts.
    //
    KeOutPort8(KdpComPort + IOCOM_LCR, 0);
    KeOutPort8(KdpComPort + IOCOM_IER, 0);

    KeOutPort8(KdpComPort + IOCOM_MCR, IOCOM_MC_RTS | IOCOM_MC_DTS | IOCOM_MC_OUT2);

    //
    // Set the Baud
    //
    Lcr = KeInPort8(KdpComPort + IOCOM_LCR);

    KeOutPort8(KdpComPort + IOCOM_LCR, Lcr | IOCOM_LC_DLA);
    KeOutPort8(KdpComPort + IOCOM_DLL, 1);
    KeOutPort8(KdpComPort + IOCOM_DLH, 0);
    KeOutPort8(KdpComPort + IOCOM_LCR, Lcr);

    //
    // 8-bit word length.
    //
    KeOutPort8(KdpComPort + IOCOM_LCR, 3);
    //
    // Clear & enable FIFO.
    //
    KeOutPort8(KdpComPort + IOCOM_FCR, IOCOM_FC_EF | IOCOM_FC_CR | IOCOM_FC_CT);

    KeInPort8(KdpComPort + IOCOM_RBR);

    return STATUS_SUCCESS;
}

ULONG32
KdpGetChecksum(
    _In_ PSTRING Contents
    )
{
    ULONG32 Index;
    ULONG32 Checksum;

    Checksum = 0;

    for (Index = 0; Index < Contents->Length; Index++) {

        Checksum += Contents->Buffer[Index];
    }

    return Checksum;
}

VOID
KdpSendControlPacket(
    _In_ KD_PACKET_TYPE Type,
    _In_ ULONG32        PacketId
    );

KD_STATUS
KdUartRecvPacket(
    _In_    KD_PACKET_TYPE PacketType,
    _Inout_ PSTRING        Head,
    _Inout_ PSTRING        Body,
    _Out_   PULONG32       Length,
    _Inout_ PKD_CONTEXT    KdContext
    );

KD_STATUS
KdUartSendPacket(
    _In_     KD_PACKET_TYPE PacketType,
    _In_     PSTRING        Head,
    _In_opt_ PSTRING        Body,
    _Inout_  PKD_CONTEXT    KdContext
    );

NTSTATUS
KdUartConnect(
    VOID
    )
{
    KdpComLoadDriver();

    KdDebugDevice.KdSendPacket = KdUartSendPacket;
    KdDebugDevice.KdReceivePacket = KdUartRecvPacket;

    return (NT_SUCCESS(KdpComLoadDriver()) && KdpComOpen) 
        ? STATUS_SUCCESS 
        : STATUS_CONNECTION_REFUSED;
}

ULONG32 KdCompPacketIdExpected   = 0x80800000;
ULONG32 KdCompNextPacketIdToSend = 0x80800800;
ULONG32 KdCompRetryCount;
ULONG32 KdCompNumberRetries;

KD_STATUS
KdUartSendPacket(
    _In_     KD_PACKET_TYPE PacketType,
    _In_     PSTRING        Head,
    _In_opt_ PSTRING        Body,
    _Inout_  PKD_CONTEXT    KdContext
    )
{
    KD_PACKET Packet;

    Packet.Checksum     = KdpGetChecksum(Head) + (ARGUMENT_PRESENT(Body) ? KdpGetChecksum(Body) : 0);
    Packet.PacketLeader = 0x30303030;
    Packet.PacketLength = Head->Length + (ARGUMENT_PRESENT(Body) ? Body->Length : 0);
    Packet.PacketType   = PacketType;

    Packet.PacketId     = KdCompNextPacketIdToSend;

    KdCompNumberRetries = KdCompRetryCount;

    do {
        KdpComSendString((PVOID)&Packet, 16);
        KdpComSendString((PVOID)Head->Buffer, Head->Length);
        if (ARGUMENT_PRESENT(Body)) {
            KdpComSendString((PVOID)Body->Buffer, Body->Length);
        }
        KdpComSendChar(0xAA);
        if (KdUartRecvPacket(KdTypeAcknowledge, 0, 0, 0, KdContext) == KdStatusTimeOut) {
            //KdCompNumberRetries--;
        }
        else {
            break;
        }
    } while (KdCompNumberRetries != 0);

    KdCompRetryCount          = KdContext->RetryCount;
    KdCompNextPacketIdToSend &= ~0x800;

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
    Length;

    ULONG32   TimeOut;
    UCHAR     Buffer[4];
    UCHAR     Char;
    ULONG32   Index;
    KD_PACKET Packet;

    if (PacketType == KdTypeCheckQueue) {
        if (KdpComReceiveReady() && KdpComReceiveChar(&Char) && Char == 0x62) {
            return KdStatusSuccess;
        }
        return KdStatusTimeOut;
    }

    TimeOut = 1000000;
    Index   = 0;

    //
    // PacketLeader
    //
    do {

        if (KdpComReceiveReady()) {
            KdpComReceiveChar(&Char);

            if (Char == 0x62) {
                KdContext->BreakRequested = 1;
                continue;
            }

            if (Char == 0x30 || Char == 0x69) {
                if (Index != 0 && Buffer[Index] != Char) {
                    Index = 0;
                }
                
                Buffer[Index] = Char;
                Index++;
            }
            else {
                Index = 0;
            }
        }
        else {
            TimeOut--;
        }

    } while (Index < 4 && TimeOut > 0);

    if (TimeOut == 0) {
        return KdStatusTimeOut;
    }

    TimeOut = 1000000;

    if (Buffer[0] == 0x30) {
        Packet.PacketLeader = 0x30303030;
    }
    else {
        Packet.PacketLeader = 0x69696969;
    }

    KdCompNumberRetries = KdCompRetryCount;
    
    do {
        do {
            if (!KdpComReceiveString((PVOID)&Packet.PacketType, 2)) {

                //KdpResend:
                //    if (Packet.PacketLeader == 0x69696969) {
                //        continue;
                //    }
                //    KdpSendControlPacket(KdTypeAcknowledge, 0);
                //    continue;

                return KdStatusTimeOut;
            }
            break;
        } while (FALSE);

        if (Packet.PacketLeader == 0x69696969 && Packet.PacketType == KdTypeResend) {
            return KdStatusResend;
        }

        if (!KdpComReceiveString((PVOID)&Packet.PacketLength, 2)) {
            return KdStatusTimeOut;
        }

        if (!KdpComReceiveString((PVOID)&Packet.PacketId, 4)) {
            return KdStatusTimeOut;
        }

        if (!KdpComReceiveString((PVOID)&Packet.Checksum, 4)) {
            return KdStatusTimeOut;
        }

        if (Packet.PacketLeader != 0x69696969) {
            if (PacketType == KdTypeAcknowledge) {
                if (Packet.PacketId != KdCompPacketIdExpected) {
                    KdpSendControlPacket(KdTypeAcknowledge, Packet.PacketId);
                    continue;
                }
                KdpSendControlPacket(KdTypeResend, 0);
                KdCompNextPacketIdToSend ^= 1u;
            }
            else {

                if (!KdpComReceiveString((PVOID)Head->Buffer, Head->MaximumLength)) {
                    // resend
                    return KdStatusTimeOut;
                }

                if (ARGUMENT_PRESENT(Body)) {
                    if (!KdpComReceiveString((PVOID)Body->Buffer, Packet.PacketLength - Head->MaximumLength)) {
                        // resend
                        return KdStatusTimeOut;
                    }
                }

                if (!KdpComReceiveString(&Char, 1)) {
                    return KdStatusTimeOut;
                }

                if (Char != 0xAA) {
                    // resend
                    return KdStatusTimeOut;
                }

                //
                // 1. Some PacketId shit i dont feel like writing
                // 2. Some Checksum checking, fuck no
                //

                KdpSendControlPacket(KdTypeAcknowledge, Packet.PacketId);
                KdCompPacketIdExpected ^= 1u;
            }
            return KdStatusSuccess;
        }
        switch (Packet.PacketType) {
        case KdTypeAcknowledge:
            if (Packet.PacketId != (KdCompNextPacketIdToSend & ~0x800u) || PacketType != KdTypeAcknowledge) {
                continue;
            }
            KdCompNextPacketIdToSend ^= 1u;
            return KdStatusSuccess;
        case KdTypeReset:
            KdCompNextPacketIdToSend = 0x80800000;
            KdCompPacketIdExpected = 0x80800000;
            KdpSendControlPacket(KdTypeReset, 0);
            break;
        case KdTypeResend:
            break;
        default:
            continue;
        }
        return KdStatusError;
    } while (TRUE);
}

VOID
KdpSendControlPacket(
    _In_ KD_PACKET_TYPE Type,
    _In_ ULONG32        PacketId
    )
{
    KD_PACKET Packet;

    Packet.PacketType = Type;
    Packet.PacketLeader = 0x69696969;
    Packet.PacketLength = 0;
    Packet.Checksum = 0;
    Packet.PacketId = PacketId;
    KdpComSendString((PVOID)&Packet, 16);
}