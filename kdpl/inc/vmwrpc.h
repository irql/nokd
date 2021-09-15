
#pragma once

NTSTATUS
KdVmwRpcInitialize(

);

NTSTATUS
KdVmwRpcInitProtocol(

);

NTSTATUS
KdVmwRpcTestConnection(

);

KD_STATUS
KdVmwRpcSendPacket(
    _In_     KD_PACKET_TYPE PacketType,
    _In_     PSTRING        Head,
    _In_opt_ PSTRING        Body,
    _Inout_  PKD_CONTEXT    KdContext
);

KD_STATUS
KdVmwRpcRecvPacket(
    _In_    KD_PACKET_TYPE PacketType,
    _Inout_ PSTRING        Head,
    _Inout_ PSTRING        Body,
    _Out_   PULONG32       Length,
    _Inout_ PKD_CONTEXT    KdContext
);

NTSTATUS
KdVmwRpcOpenChannel(
    _In_ PKD_VMWRPC_CONTROL VmwRpc
);

NTSTATUS
KdVmwRpcCloseChannel(
    _In_ PKD_VMWRPC_CONTROL VmwRpc
);

NTSTATUS
KdVmwRpcSendCommandLength(
    _In_ PKD_VMWRPC_CONTROL VmwRpc,
    _In_ ULONG32            Length
);

NTSTATUS
KdVmwRpcSendCommandBuffer(
    _In_ PKD_VMWRPC_CONTROL VmwRpc,
    _In_ ULONG32            Length,
    _In_ PVOID              Buffer
);

NTSTATUS
KdVmwRpcRecvCommandLength(
    _In_ PKD_VMWRPC_CONTROL VmwRpc,
    _In_ PULONG32           Length
);

NTSTATUS
KdVmwRpcRecvCommandBuffer(
    _In_ PKD_VMWRPC_CONTROL VmwRpc,
    _In_ ULONG32            Length,
    _In_ PVOID              Buffer
);

//PUBLIC      KdVmwRpcSendFull

NTSTATUS
KdVmwRpcRecvCommandFinish(
    _In_ PKD_VMWRPC_CONTROL VmwRpc
);
