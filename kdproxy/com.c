
#include "kd.h"

KD_PORT KdpPortDevice;

KD_UART_STATUS
KdpSendString(
    _In_ PVOID String,
    _In_ ULONG Length
)
{
    PUCHAR String1;

    String1 = String;

    while ( Length-- ) {

        KdpPortDevice.Send( &KdpPortDevice, *String1 );
        String1++;
    }
}

KD_UART_STATUS
KdpRecvString(
    _In_ PVOID String,
    _In_ ULONG Length
)
{
    PUCHAR String1;

    String1 = String;

    while ( Length-- ) {

        KdpPortDevice.Recv( &KdpPortDevice, String1 );
        String1++;
    }
}
