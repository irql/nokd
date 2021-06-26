
#pragma once

KDAPI
KD_STATUS
KdpReadVirtualMemory(
    _In_  PDBGKD_MANIPULATE_STATE64 Packet,
    _Out_ PSTRING                   Body
);

KDAPI
KD_STATUS
KdpWriteVirtualMemory(
    _In_ PDBGKD_MANIPULATE_STATE64 Packet,
    _In_ PSTRING                   Body
);

KDAPI
KD_STATUS
KdpGetContext(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body
);

KDAPI
KD_STATUS
KdpSetContext(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body
);

KDAPI
KD_STATUS
KdpAddBreakpoint(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body
);

KDAPI
KD_STATUS
KdpDeleteBreakpoint(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body
);

KDAPI
KD_STATUS
KdpReadControlSpace(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body
);

KDAPI
KD_STATUS
KdpWriteControlSpace(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body
);

KDAPI
KD_STATUS
KdpReadIoSpace(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body
);

KDAPI
KD_STATUS
KdpWriteIoSpace(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body
);

KDAPI
KD_STATUS
KdpReadPhysicalMemory(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body
);

KDAPI
KD_STATUS
KdpWritePhysicalMemory(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body
);

KDAPI
KD_STATUS
KdpGetVersion(
    _Inout_ PDBGKD_MANIPULATE_STATE64 Packet,
    _Inout_ PSTRING                   Body
);
