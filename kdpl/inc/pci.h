
#pragma once

#define PCI_CONFIG_ADDRESS  0x0CF8
#define PCI_CONFIG_DATA     0x0CFC

#define PCI_GET_ADDRESS(Bus, Device, Function, Offset)      \
    ((Bus)      << 16) |                                    \
    ((Device)   << 11) |                                    \
    ((Function) << 8 ) |                                    \
    ((Offset)    & ~3) | (1 << 31)

#define PCI_CMD_INTERRUPT_DISABLE   0x400
#define PCI_CMD_IO_SPACE            0x001
#define PCI_CMD_MEMORY_SPACE        0x002
#define PCI_CMD_BUS_MASTER          0x004

typedef struct _KPCI_DEVICE_HEADER {
    USHORT  VendorId;
    USHORT  DeviceId;
    USHORT  Command;
    USHORT  Status;
    UCHAR   RevisionId;
    UCHAR   ProgrammingInterface;
    UCHAR   SubClass;
    UCHAR   Class;
    UCHAR   CacheLineSize;
    UCHAR   LatencyTimer;
    UCHAR   HeaderType;
    UCHAR   Bist;
} KPCI_DEVICE_HEADER, *PKPCI_DEVICE_HEADER;

// 
// Description               USB Controller (xHCI)
// Location                  bus 0 (0x00), device 20 (0x14), function 0 (0x00)
// Common header                
//      Vendor ID            0x8086 (Intel)
//      Model ID             0xA2AF
//      Revision ID          0x00
// PCI header                
//      Address 0 (memory)   0x00000000F7410004
//      Subvendor ID         0x1043 (ASUSTeK)
//      Subsystem ID         0x8694
//      Int. Line            0x00
//      Int. Pin             0x01
// PCI capability                
//      Caps class           Power Management
//      Caps offset          0x70
//      Caps version         1.1
// PCI capability                
//      Caps class           Message Signalled Interrupts
//      Caps offset          0x80
// 

typedef struct _KPCI_DEVICE {
    KPCI_DEVICE_HEADER Header;

    union {

        //
        // HeaderType = 0.
        //
        struct {
            ULONG32 Bar[6];
            ULONG32 CardbusInfoAddress;
            USHORT  SubsystemVendorId;
            USHORT  SubsystemId;
            ULONG32 ExpansionRomBase;
            UCHAR   CapsAddress;
            UCHAR   Reserved[7];
            UCHAR   InterruptLine;
            UCHAR   InterruptPin;
            UCHAR   MinimumGrant;
            UCHAR   MaximumLatency;
        } Common;
    };


} KPCI_DEVICE, *PKPCI_DEVICE;

UCHAR  
KdpPciRead08(
    _In_ ULONG32 BusId,
    _In_ ULONG32 DeviceId,
    _In_ ULONG32 FunctionId,
    _In_ ULONG32 Offset
    );

USHORT 
KdpPciRead16(
    _In_ ULONG32 BusId,
    _In_ ULONG32 DeviceId,
    _In_ ULONG32 FunctionId,
    _In_ ULONG32 Offset
    );

ULONG32
KdpPciRead32(
    _In_ ULONG32 BusId,
    _In_ ULONG32 DeviceId,
    _In_ ULONG32 FunctionId,
    _In_ ULONG32 Offset
    );

VOID
KdpPciWrite08(
    _In_ ULONG32 BusId,
    _In_ ULONG32 DeviceId,
    _In_ ULONG32 FunctionId,
    _In_ ULONG32 Offset,
    _In_ UCHAR   Long
    );

VOID
KdpPciWrite16(
    _In_ ULONG32 BusId,
    _In_ ULONG32 DeviceId,
    _In_ ULONG32 FunctionId,
    _In_ ULONG32 Offset,
    _In_ USHORT  Long
    );

VOID
KdpPciWrite32(
    _In_ ULONG32 BusId,
    _In_ ULONG32 DeviceId,
    _In_ ULONG32 FunctionId,
    _In_ ULONG32 Offset,
    _In_ ULONG32 Long
    );