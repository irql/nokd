
#pragma once

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
