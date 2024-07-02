
#include <kdpl.h>
#include <pci.h>

#define KeInPort8                   __inbyte
#define KeInPort16                  __inword
#define KeInPort32                  __indword
#define KeOutPort8                  __outbyte
#define KeOutPort16                 __outword
#define KeOutPort32                 __outdword

UCHAR
KdpPciRead08(
    _In_ ULONG32 BusId,
    _In_ ULONG32 DeviceId,
    _In_ ULONG32 FunctionId,
    _In_ ULONG32 Offset
    )
{
    KeOutPort32(PCI_CONFIG_ADDRESS, 
                PCI_GET_ADDRESS(BusId, 
                                DeviceId, 
                                FunctionId, 
                                Offset));
    return KeInPort8(PCI_CONFIG_DATA + (Offset & 3));
}

USHORT 
KdpPciRead16(
    _In_ ULONG32 BusId,
    _In_ ULONG32 DeviceId,
    _In_ ULONG32 FunctionId,
    _In_ ULONG32 Offset
    )
{
    KeOutPort32(PCI_CONFIG_ADDRESS, 
                PCI_GET_ADDRESS(BusId, 
                                DeviceId, 
                                FunctionId, 
                                Offset));
    return KeInPort16(PCI_CONFIG_DATA + (Offset & 2));
}

ULONG32
KdpPciRead32(
    _In_ ULONG32 BusId,
    _In_ ULONG32 DeviceId,
    _In_ ULONG32 FunctionId,
    _In_ ULONG32 Offset
    )
{
    KeOutPort32(PCI_CONFIG_ADDRESS, 
                PCI_GET_ADDRESS(BusId, 
                                DeviceId, 
                                FunctionId, 
                                Offset));
    return KeInPort32(PCI_CONFIG_DATA);
}

VOID
KdpPciWrite08(
    _In_ ULONG32 BusId,
    _In_ ULONG32 DeviceId,
    _In_ ULONG32 FunctionId,
    _In_ ULONG32 Offset,
    _In_ UCHAR Long
    )
{
    KeOutPort32(PCI_CONFIG_ADDRESS, 
                PCI_GET_ADDRESS(BusId, 
                                DeviceId, 
                                FunctionId, 
                                Offset));
    KeOutPort8(PCI_CONFIG_DATA + (Offset & 3), Long);
}

VOID
KdpPciWrite16(
    _In_ ULONG32 BusId,
    _In_ ULONG32 DeviceId,
    _In_ ULONG32 FunctionId,
    _In_ ULONG32 Offset,
    _In_ USHORT  Long
    )
{
    KeOutPort32(PCI_CONFIG_ADDRESS, 
                PCI_GET_ADDRESS(BusId, 
                                DeviceId, 
                                FunctionId, 
                                Offset));
    KeOutPort16(PCI_CONFIG_DATA + (Offset & 2), Long);
}

VOID
KdpPciWrite32(
    _In_ ULONG32 BusId,
    _In_ ULONG32 DeviceId,
    _In_ ULONG32 FunctionId,
    _In_ ULONG32 Offset,
    _In_ ULONG32 Long
    )
{
    KeOutPort32(PCI_CONFIG_ADDRESS, 
                PCI_GET_ADDRESS(BusId, 
                                DeviceId, 
                                FunctionId, 
                                Offset));
    KeOutPort32(PCI_CONFIG_DATA, Long);
}