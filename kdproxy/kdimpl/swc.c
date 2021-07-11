
#include <kd.h>
#include "../hde64/hde64.h"

UCHAR KdpMessageBuffer[ 0x1000 ];

BOOLEAN
KdPollBreakIn(

)
{
    if ( KdpContext.BreakRequested == TRUE ) {

        KdpContext.BreakRequested = FALSE;
        return TRUE;
    }

    return KdDebugDevice.KdReceivePacket( KdTypePollBreakin,
                                          NULL,
                                          NULL,
                                          NULL,
                                          &KdpContext ) == KdStatusSuccess;
}

VOID
KdpSetCommonState(
    _In_    ULONG32                  ApiNumber,
    _In_    PCONTEXT                 Context,
    _Inout_ PDBGKD_WAIT_STATE_CHANGE Change
)
{
    //
    // TODO: This procedure also deletes the breakpoint range at
    //       the context's rip.
    //

    ULONG64 InstructionCount;
    ULONG32 CurrentHandle;

    Change->ApiNumber = ApiNumber;
    Change->Processor = ( USHORT )KdGetCurrentPrcbNumber( );
    Change->ProcessorCount = KeQueryActiveProcessorCountEx( 0xFFFF );
    if ( Change->ProcessorCount <= Change->Processor ) {

        Change->ProcessorCount = Change->Processor + 1;
    }

    Change->ProcessorLevel = *KeProcessorLevel;
    Change->CurrentThread = ( ULONG64 )KeGetCurrentThread( );

    RtlZeroMemory( &Change->ControlReport, sizeof( DBGKD_CONTROL_REPORT ) );

    //
    // This is important breakpoint handling code,
    // this will set a stub-bp directly after the previous instruction,
    // this will be caught, and the old bp will be re-added.
    //

    for ( CurrentHandle = 0;
          CurrentHandle < KD_BREAKPOINT_TABLE_LENGTH;
          CurrentHandle++ ) {

        if ( KdpBreakpointTable[ CurrentHandle ].Flags & KD_BPE_SET ) {

            if ( KdpBreakpointTable[ CurrentHandle ].Address ==
                 Context->Rip - KdpBreakpointCodeLength ) {

                __try {
                    RtlCopyMemory( ( void* )KdpBreakpointTable[ CurrentHandle ].Address,
                        ( void* )KdpBreakpointTable[ CurrentHandle ].Content,
                                   KdpBreakpointCodeLength );

                    RtlCopyMemory( ( void* )( KdpBreakpointTable[ CurrentHandle ].Address +
                                              KdpBreakpointTable[ CurrentHandle ].ContentLength ),
                                   KdpBreakpointCode,
                                   KdpBreakpointCodeLength );
                }
                __except ( TRUE ) {

                    NT_ASSERT( FALSE );
                }

                //_mm_clflush( ( void* )KdProcessorBlock[ CurrentHandle ].Tracepoint );

                Context->Rip -= KdpBreakpointCodeLength;
                break;
            }
        }
    }

    Change->ProgramCounter = Context->Rip;

    __try {
        RtlCopyMemory( Change->ControlReport.InstructionStream, ( PVOID )Context->Rip, 0x10 );
        InstructionCount = 0x10;
    }
    __except ( EXCEPTION_EXECUTE_HANDLER ) {

        InstructionCount = 0;
    }
    Change->ControlReport.InstructionCount = ( USHORT )InstructionCount;

}

VOID
KdpSetContextState(
    _Inout_ PDBGKD_WAIT_STATE_CHANGE Change,
    _In_    PCONTEXT                 Context
)
{
    Context;
    // TODO: Must call real KdpSetContextState.
    // dr's come from Prcb->ProcessorState.

    Change->ControlReport.Dr6 = KdGetPrcbSpecialRegisters( KeGetCurrentPrcb( ) )->KernelDr6;
    Change->ControlReport.Dr7 = KdGetPrcbSpecialRegisters( KeGetCurrentPrcb( ) )->KernelDr7;

    Change->ControlReport.SegCs = Context->SegCs;
    Change->ControlReport.SegDs = Context->SegDs;
    Change->ControlReport.SegEs = Context->SegEs;
    Change->ControlReport.SegFs = Context->SegFs;

    Change->ControlReport.EFlags = Context->EFlags;
    Change->ControlReport.ReportFlags = 1;

    if ( Context->SegCs == 0x10 ||//KdDebuggerDataBlock.GdtR0Code ||
         Context->SegCs == 0x33 ) {//KdDebuggerDataBlock.GdtR3Code ) {

        Change->ControlReport.ReportFlags = 0x3;
    }
}

BOOLEAN
KdpSendWaitContinue(
    _In_ ULONG64  Unused,
    _In_ PSTRING  StateChangeHead,
    _In_ PSTRING  StateChangeBody,
    _In_ PCONTEXT Context
)
{
    Context;
    Unused;

    //
    // This function prototype is matched with the nt
    // function, the first parameter is infact unreferenced.
    // 
    // Some things are quite different later down, but it still
    // works fine with WinDbg.
    //

    KD_STATUS                Status;
    STRING                   Head;
    STRING                   Body;
    ULONG32                  Length;
    DBGKD_MANIPULATE_STATE64 Packet;
    ULONG32                  Processor;
    HDE64S                   HdeCode;
    ULONG32                  CodeLength;

    Head.MaximumLength = sizeof( DBGKD_MANIPULATE_STATE64 );
    Head.Length = 0;
    Head.Buffer = ( PCHAR )&Packet;

    Body.MaximumLength = 0x1000;
    Body.Length = 0;
    Body.Buffer = ( PCHAR )&KdpMessageBuffer;

KdpResendPacket:
    KdDebugDevice.KdSendPacket( KdTypeStateChange,
                                StateChangeHead,
                                StateChangeBody,
                                &KdpContext );

    while ( !KdDebuggerNotPresent_ ) {

        while ( 1 ) {

            RtlZeroMemory( &Packet, sizeof( DBGKD_MANIPULATE_STATE64 ) );

            Head.MaximumLength = sizeof( DBGKD_MANIPULATE_STATE64 );
            Head.Length = 0;

            Body.MaximumLength = 0x1000;
            Body.Length = 0;

            Status = KdDebugDevice.KdReceivePacket( KdTypeStateManipulate,
                                                    &Head,
                                                    &Body,
                                                    &Length,
                                                    &KdpContext );
            if ( Status == KdStatusResend ) {

                //
                // This seems a little weird, but this is the same as 
                // nt!KdpSendWaitContinue, if a resend is requested at any point
                // the state is sent, I believe it is for when the debugger
                // is restarted maybe? Not sure.
                //

                DbgPrint( "Resend!\n" );

                goto KdpResendPacket;
            }

            CHAR DbgKdApi[ DbgKdMaximumManipulate - DbgKdMinimumManipulate ][ 48 ] = {
                "DbgKdReadVirtualMemoryApi",
                "DbgKdWriteVirtualMemoryApi",
                "DbgKdGetContextApi",
                "DbgKdSetContextApi",
                "DbgKdWriteBreakPointApi",
                "DbgKdRestoreBreakPointApi",
                "DbgKdContinueApi",
                "DbgKdReadControlSpaceApi",
                "DbgKdWriteControlSpaceApi",
                "DbgKdReadIoSpaceApi",
                "DbgKdWriteIoSpaceApi",
                "DbgKdRebootApi",
                "DbgKdContinueApi2",
                "DbgKdReadPhysicalMemoryApi",
                "DbgKdWritePhysicalMemoryApi",
                "DbgKdQuerySpecialCallsApi",
                "DbgKdSetSpecialCallApi",
                "DbgKdClearSpecialCallsApi",
                "DbgKdSetInternalBreakPointApi",
                "DbgKdGetInternalBreakPointApi",
                "DbgKdReadIoSpaceExtendedApi",
                "DbgKdWriteIoSpaceExtendedApi",
                "DbgKdGetVersionApi",
                "DbgKdWriteBreakPointExApi",
                "DbgKdRestoreBreakPointExApi",
                "DbgKdCauseBugCheckApi",
                "DbgKdPlaceholder1",
                "DbgKdPlaceholder2",
                "DbgKdPlaceholder3",
                "DbgKdPlaceholder4",
                "DbgKdPlaceholder5",
                "DbgKdPlaceholder6",
                "DbgKdSwitchProcessor",
                "DbgKdPageInApi",
                "DbgKdReadMachineSpecificRegister",
                "DbgKdWriteMachineSpecificRegister",
                "OldVlm1",
                "OldVlm2",
                "DbgKdSearchMemoryApi",
                "DbgKdGetBusDataApi",
                "DbgKdSetBusDataApi",
                "DbgKdCheckLowMemoryApi",
                "DbgKdClearAllInternalBreakpointsApi",
                "DbgKdFillMemoryApi",
                "DbgKdQueryMemoryApi",
                "DbgKdSwitchPartition",
                "DbgKdGetContextEx",
                "DbgKdSetContextEx",
                "DbgKdWriteCustomBreakpointEx",
                "DbgKdReadPhysicalMemoryLong",
            };

            if ( Status == KdStatusSuccess &&
                 Packet.ApiNumber >= DbgKdMinimumManipulate &&
                 Packet.ApiNumber < DbgKdMaximumManipulate ) {

                switch ( Packet.ApiNumber ) {
                case DbgKdReadVirtualMemoryApi:
                    KdpReadVirtualMemory( &Packet,
                                          &Body );
                    break;
                case DbgKdWriteVirtualMemoryApi:
                    KdpWriteVirtualMemory( &Packet,
                                           &Body );
                    break;
                case DbgKdGetContextApi:
                    KdpGetContext( &Packet,
                                   &Body,
                                   Context );
                    break;
                case DbgKdSetContextApi:
                    KdpSetContext( &Packet,
                                   &Body,
                                   Context );
                    break;
                case DbgKdWriteBreakPointApi:
                    KdpAddBreakpoint( &Packet,
                                      &Body );
                    break;
                case DbgKdRestoreBreakPointApi:
                    KdpDeleteBreakpoint( &Packet,
                                         &Body );
                    break;
                case DbgKdContinueApi:
                    return NT_SUCCESS( Packet.u.Continue.ContinueStatus );
                case DbgKdReadControlSpaceApi:
                    KdpReadControlSpace( &Packet,
                                         &Body );
                    break;
                case DbgKdWriteControlSpaceApi:
                    KdpWriteControlSpace( &Packet,
                                          &Body );
                    break;
                case DbgKdReadIoSpaceApi:
                    KdpReadIoSpace( &Packet,
                                    &Body );
                    break;
                case DbgKdWriteIoSpaceApi:
                    KdpWriteIoSpace( &Packet,
                                     &Body );
                    break;
                case DbgKdRebootApi:
                case DbgKdCauseBugCheckApi:

                    //
                    // Set the nig mode. 
                    //
#if !(KD_DEBUG_NO_FREEZE)
                    KdThawProcessors( );
#endif

                    KeBugCheck( D3DNIG_MAXIMUM );
                    break;
                case DbgKdContinueApi2:
                    if ( !NT_SUCCESS( Packet.u.Continue.ContinueStatus ) ) {

                        return FALSE;
                    }

                    if ( Packet.u.Continue2.ControlSet.TraceFlag ) {

                        Processor = KdGetCurrentPrcbNumber( );

                        KdProcessorBlock[ Processor ].Flags |= KDPR_FLAG_TPE;

                        KdProcessorBlock[ Processor ].Tracepoint = Context->Rip;

                        __try {
                            CodeLength = Hde64Decode( ( const void* )KdProcessorBlock[ Processor ].Tracepoint, &HdeCode );

                            //
                            // This switch statement is responsible for calculating
                            // the instruction pointer after said instruction is executed.
                            // This means if it's a branch/return/any branching instruction,
                            // we have to calculate where it will land.
                            //
                            // We don't account for any far jumps or far returns, 
                            // and you should have any far jumps or far returns in your
                            // code.
                            //
                            // Instructions: LOOP/Z/NZ, SYSCALL,
                            //
                            // Although this is a little weird, a good note is that rep
                            // prefixes are not accounted for, where as they are for
                            // trap flag usage.
                            //
                            // TODO: Sib bytes are not handled.
                            //       Segment overrides are ignored.
                            //
                            // NOTE:
                            // Hde64 is quite disappointing, after writing this 500 or so line 
                            // branch emulator, I've noticed it's unable to decode some instructions
                            // the first example has it's length set to 0:
                            // 450fb6f1        movzx   r14d,r9b
                            //
                            // This could also be partially my fault, I don't handle any errors generated 
                            // here and assume you don't have your debugger execute junk.
                            //


                            switch ( HdeCode.opcode ) {

                                //
                                // Near return, read the return address from the top of
                                // the stack.
                                //
                            case 0xC3:
                            case 0xC2:

                                KdProcessorBlock[ Processor ].Tracepoint = *( ULONG64* )Context->Rsp;
                                break;

                                //
                                // Jump rel8
                                //
                            case 0xEB:
                                KdProcessorBlock[ Processor ].Tracepoint += CodeLength;
                                KdProcessorBlock[ Processor ].Tracepoint += ( LONG64 )( CHAR )HdeCode.imm.imm8;
                                break;

                                //
                                // Jump/Call rel16/rel32
                                //
                            case 0xE9:
                            case 0xE8:

                                //
                                // Inconsistent hde meaning this is an immediate, not a displacement,
                                // as expected. I suppose it is only 200 lines.
                                // 

                                if ( HdeCode.p_66 ) {
                                    KdProcessorBlock[ Processor ].Tracepoint += CodeLength;
                                    KdProcessorBlock[ Processor ].Tracepoint += ( LONG64 )( LONG32 )HdeCode.imm.imm16;
                                }
                                else {
                                    KdProcessorBlock[ Processor ].Tracepoint += CodeLength;
                                    KdProcessorBlock[ Processor ].Tracepoint += ( LONG64 )( LONG32 )HdeCode.imm.imm32;
                                }
                                break;

                                //
                                // Jump/Call modrm
                                //
                            case 0xFF:

                                //
                                // The only difference between a FF call and FF jmp
                                // is that, the modrm's reg field is 2 on a jmp, 
                                // and 1 on a call. it's 0 for increment, and 3 for
                                // push, so we need to avoid those.
                                //

                                if ( HdeCode.modrm_reg == 2 ||
                                     HdeCode.modrm_reg == 1 ) {

                                    switch ( HdeCode.modrm_mod ) {
                                    case 0:
                                        //
                                        // [ Register ]
                                        //

                                        //
                                        // Rbp rm (5) is used for Rip relative addressing.
                                        //

                                        if ( HdeCode.modrm_rm == 5 ) {

                                            KdProcessorBlock[ Processor ].Tracepoint = *( ULONG64* )(
                                                KdProcessorBlock[ Processor ].Tracepoint +
                                                CodeLength +
                                                ( LONG64 )( LONG32 )HdeCode.disp.disp32 );
                                        }
                                        else {
                                            if ( HdeCode.flags & F_PREFIX_REX ) {

                                                KdProcessorBlock[ Processor ].Tracepoint = **( ( ULONG64** )&Context->R8 + HdeCode.modrm_rm );
                                            }
                                            else {

                                                KdProcessorBlock[ Processor ].Tracepoint = **( ( ULONG64** )&Context->Rax + HdeCode.modrm_rm );
                                            }
                                        }

                                        break;
                                    case 1:
                                        //
                                        // [ Register + Displacement8 ]
                                        //

                                        if ( HdeCode.flags & F_PREFIX_REX ) {

                                            KdProcessorBlock[ Processor ].Tracepoint = *( ULONG64* )(
                                                *( &Context->R8 + HdeCode.modrm_rm ) + ( LONG64 )( CHAR )HdeCode.disp.disp8 );
                                        }
                                        else {

                                            KdProcessorBlock[ Processor ].Tracepoint = *( ULONG64* )(
                                                *( &Context->Rax + HdeCode.modrm_rm ) + ( LONG64 )( CHAR )HdeCode.disp.disp8 );
                                        }

                                        break;
                                    case 2:
                                        //
                                        // [ Register + Displacement32 ]
                                        //

                                        if ( HdeCode.flags & F_PREFIX_REX ) {

                                            KdProcessorBlock[ Processor ].Tracepoint = *( ULONG64* )(
                                                *( &Context->R8 + HdeCode.modrm_rm ) + ( LONG64 )( LONG32 )HdeCode.disp.disp32 );
                                        }
                                        else {

                                            KdProcessorBlock[ Processor ].Tracepoint = *( ULONG64* )(
                                                *( &Context->Rax + HdeCode.modrm_rm ) + ( LONG64 )( LONG32 )HdeCode.disp.disp32 );
                                        }

                                        break;
                                    case 3:
                                        //
                                        // Register
                                        //

                                        if ( HdeCode.flags & F_PREFIX_REX ) {

                                            KdProcessorBlock[ Processor ].Tracepoint = *( &Context->R8 + HdeCode.modrm_rm );
                                        }
                                        else {

                                            KdProcessorBlock[ Processor ].Tracepoint = *( &Context->Rax + HdeCode.modrm_rm );
                                        }
                                        break;
                                    default:
                                        __assume( 0 );
                                    }
                                }

                                break;

                                //
                                // All conditional short jumps
                                //
                            case 0x70: // Jo
                                KdProcessorBlock[ Processor ].Tracepoint += CodeLength;

                                if ( ( Context->EFlags & EFL_OF ) == EFL_OF ) {

                                    KdProcessorBlock[ Processor ].Tracepoint +=  ( LONG64 )( CHAR )HdeCode.disp.disp8;
                                }
                                break;
                            case 0x71: // Jno
                                KdProcessorBlock[ Processor ].Tracepoint += CodeLength;

                                if ( ( Context->EFlags & EFL_OF ) == 0 ) {

                                    KdProcessorBlock[ Processor ].Tracepoint +=  ( LONG64 )( CHAR )HdeCode.disp.disp8;
                                }
                                break;
                            case 0x72: // Jc
                                KdProcessorBlock[ Processor ].Tracepoint += CodeLength;

                                if ( ( Context->EFlags & EFL_CF ) == EFL_CF ) {

                                    KdProcessorBlock[ Processor ].Tracepoint +=  ( LONG64 )( CHAR )HdeCode.disp.disp8;
                                }
                                break;
                            case 0x73: // Jnc
                                KdProcessorBlock[ Processor ].Tracepoint += CodeLength;

                                if ( ( Context->EFlags & EFL_CF ) == 0 ) {

                                    KdProcessorBlock[ Processor ].Tracepoint +=  ( LONG64 )( CHAR )HdeCode.disp.disp8;
                                }
                                break;
                            case 0x74: // Jz
                                KdProcessorBlock[ Processor ].Tracepoint += CodeLength;

                                if ( ( Context->EFlags & EFL_ZF ) == EFL_ZF ) {

                                    KdProcessorBlock[ Processor ].Tracepoint +=  ( LONG64 )( CHAR )HdeCode.disp.disp8;
                                }
                                break;
                            case 0x75: // Jnz
                                KdProcessorBlock[ Processor ].Tracepoint += CodeLength;

                                if ( ( Context->EFlags & EFL_ZF ) == 0 ) {

                                    KdProcessorBlock[ Processor ].Tracepoint +=  ( LONG64 )( CHAR )HdeCode.disp.disp8;
                                }
                                break;
                            case 0x76: // Jbe
                                KdProcessorBlock[ Processor ].Tracepoint += CodeLength;

                                if ( ( Context->EFlags & ( EFL_CF | EFL_ZF ) ) != 0 ) {

                                    KdProcessorBlock[ Processor ].Tracepoint +=  ( LONG64 )( CHAR )HdeCode.disp.disp8;
                                }
                                break;
                            case 0x77: // Jnbe
                                KdProcessorBlock[ Processor ].Tracepoint += CodeLength;

                                if ( ( Context->EFlags & ( EFL_CF | EFL_ZF ) ) == 0 ) {

                                    KdProcessorBlock[ Processor ].Tracepoint +=  ( LONG64 )( CHAR )HdeCode.disp.disp8;
                                }
                                break;
                            case 0x78: // Js
                                KdProcessorBlock[ Processor ].Tracepoint += CodeLength;

                                if ( ( Context->EFlags & EFL_SF ) == EFL_SF ) {

                                    KdProcessorBlock[ Processor ].Tracepoint +=  ( LONG64 )( CHAR )HdeCode.disp.disp8;
                                }
                                break;
                            case 0x79: // Jns
                                KdProcessorBlock[ Processor ].Tracepoint += CodeLength;

                                if ( ( Context->EFlags & EFL_SF ) == 0 ) {

                                    KdProcessorBlock[ Processor ].Tracepoint +=  ( LONG64 )( CHAR )HdeCode.disp.disp8;
                                }
                                break;
                            case 0x7A: // Jp
                                KdProcessorBlock[ Processor ].Tracepoint += CodeLength;

                                if ( ( Context->EFlags & EFL_PF ) == EFL_PF ) {

                                    KdProcessorBlock[ Processor ].Tracepoint +=  ( LONG64 )( CHAR )HdeCode.disp.disp8;
                                }
                                break;
                            case 0x7B: // Jnp
                                KdProcessorBlock[ Processor ].Tracepoint += CodeLength;

                                if ( ( Context->EFlags & EFL_PF ) == 0 ) {

                                    KdProcessorBlock[ Processor ].Tracepoint +=  ( LONG64 )( CHAR )HdeCode.disp.disp8;
                                }
                                break;
                            case 0x7C: // Jnge
                                KdProcessorBlock[ Processor ].Tracepoint += CodeLength;

                                if ( ( ( Context->EFlags & EFL_SF ) == EFL_SF ) !=
                                    ( ( Context->EFlags & EFL_OF ) == EFL_OF ) ) {

                                    KdProcessorBlock[ Processor ].Tracepoint +=  ( LONG64 )( CHAR )HdeCode.disp.disp8;
                                }
                                break;
                            case 0x7D: // Jge
                                KdProcessorBlock[ Processor ].Tracepoint += CodeLength;

                                if ( ( ( Context->EFlags & EFL_SF ) == EFL_SF ) ==
                                    ( ( Context->EFlags & EFL_OF ) == EFL_OF ) ) {

                                    KdProcessorBlock[ Processor ].Tracepoint +=  ( LONG64 )( CHAR )HdeCode.disp.disp8;
                                }
                                break;
                            case 0x7E: // Jng
                                KdProcessorBlock[ Processor ].Tracepoint += CodeLength;

                                if ( ( ( Context->EFlags & EFL_ZF ) == EFL_ZF ) ||
                                    ( ( Context->EFlags & EFL_SF ) == EFL_SF ) !=
                                     ( ( Context->EFlags & EFL_OF ) == EFL_OF ) ) {

                                    KdProcessorBlock[ Processor ].Tracepoint +=  ( LONG64 )( CHAR )HdeCode.disp.disp8;
                                }
                                break;
                            case 0x7F: // Jg
                                KdProcessorBlock[ Processor ].Tracepoint += CodeLength;

                                if ( ( ( Context->EFlags & EFL_ZF ) == 0 ) &&
                                    ( ( Context->EFlags & EFL_SF ) == EFL_SF ) ==
                                     ( ( Context->EFlags & EFL_OF ) == EFL_OF ) ) {

                                    KdProcessorBlock[ Processor ].Tracepoint +=  ( LONG64 )( CHAR )HdeCode.disp.disp8;
                                }
                                break;
                            case 0xE3: // Jrcxz & Jecxz
                                KdProcessorBlock[ Processor ].Tracepoint += CodeLength;

                                if ( HdeCode.p_67 ) {

                                    if ( ( Context->Rcx & 0xFFFFFFFF ) == 0 ) {

                                        KdProcessorBlock[ Processor ].Tracepoint +=  ( LONG64 )( CHAR )HdeCode.disp.disp8;
                                    }
                                }
                                else {

                                    if ( Context->Rcx == 0 ) {

                                        KdProcessorBlock[ Processor ].Tracepoint +=  ( LONG64 )( CHAR )HdeCode.disp.disp8;
                                    }
                                }

                                break;
                                //
                                // All conditional near jumps
                                //
                            case 0x0F:

                                switch ( HdeCode.opcode2 ) {
                                case 0x80: // Jo
                                    KdProcessorBlock[ Processor ].Tracepoint += CodeLength;

                                    if ( ( Context->EFlags & EFL_OF ) == EFL_OF ) {

                                        KdProcessorBlock[ Processor ].Tracepoint +=  ( LONG64 )( LONG32 )HdeCode.disp.disp32;
                                    }
                                    break;
                                case 0x81: // Jno
                                    KdProcessorBlock[ Processor ].Tracepoint += CodeLength;

                                    if ( ( Context->EFlags & EFL_OF ) == 0 ) {

                                        KdProcessorBlock[ Processor ].Tracepoint +=  ( LONG64 )( LONG32 )HdeCode.disp.disp32;
                                    }
                                    break;
                                case 0x82: // Jc
                                    KdProcessorBlock[ Processor ].Tracepoint += CodeLength;

                                    if ( ( Context->EFlags & EFL_CF ) == EFL_CF ) {

                                        KdProcessorBlock[ Processor ].Tracepoint +=  ( LONG64 )( LONG32 )HdeCode.disp.disp32;
                                    }
                                    break;
                                case 0x83: // Jnc
                                    KdProcessorBlock[ Processor ].Tracepoint += CodeLength;

                                    if ( ( Context->EFlags & EFL_CF ) == 0 ) {

                                        KdProcessorBlock[ Processor ].Tracepoint +=  ( LONG64 )( LONG32 )HdeCode.disp.disp32;
                                    }
                                    break;
                                case 0x84: // Jz
                                    KdProcessorBlock[ Processor ].Tracepoint += CodeLength;

                                    if ( ( Context->EFlags & EFL_ZF ) == EFL_ZF ) {

                                        KdProcessorBlock[ Processor ].Tracepoint +=  ( LONG64 )( LONG32 )HdeCode.disp.disp32;
                                    }
                                    break;
                                case 0x85: // Jnz
                                    KdProcessorBlock[ Processor ].Tracepoint += CodeLength;

                                    if ( ( Context->EFlags & EFL_ZF ) == 0 ) {

                                        KdProcessorBlock[ Processor ].Tracepoint +=  ( LONG64 )( LONG32 )HdeCode.disp.disp32;
                                    }
                                    break;
                                case 0x86: // Jbe
                                    KdProcessorBlock[ Processor ].Tracepoint += CodeLength;

                                    if ( ( Context->EFlags & ( EFL_CF | EFL_ZF ) ) != 0 ) {

                                        KdProcessorBlock[ Processor ].Tracepoint +=  ( LONG64 )( LONG32 )HdeCode.disp.disp32;
                                    }
                                    break;
                                case 0x87: // Jnbe
                                    KdProcessorBlock[ Processor ].Tracepoint += CodeLength;

                                    if ( ( Context->EFlags & ( EFL_CF | EFL_ZF ) ) == 0 ) {

                                        KdProcessorBlock[ Processor ].Tracepoint +=  ( LONG64 )( LONG32 )HdeCode.disp.disp32;
                                    }
                                    break;
                                case 0x88: // Js
                                    KdProcessorBlock[ Processor ].Tracepoint += CodeLength;

                                    if ( ( Context->EFlags & EFL_SF ) == EFL_SF ) {

                                        KdProcessorBlock[ Processor ].Tracepoint +=  ( LONG64 )( LONG32 )HdeCode.disp.disp32;
                                    }
                                    break;
                                case 0x89: // Jns
                                    KdProcessorBlock[ Processor ].Tracepoint += CodeLength;

                                    if ( ( Context->EFlags & EFL_SF ) == 0 ) {

                                        KdProcessorBlock[ Processor ].Tracepoint +=  ( LONG64 )( LONG32 )HdeCode.disp.disp32;
                                    }
                                    break;
                                case 0x8A: // Jp
                                    KdProcessorBlock[ Processor ].Tracepoint += CodeLength;

                                    if ( ( Context->EFlags & EFL_PF ) == EFL_PF ) {

                                        KdProcessorBlock[ Processor ].Tracepoint +=  ( LONG64 )( LONG32 )HdeCode.disp.disp32;
                                    }
                                    break;
                                case 0x8B: // Jnp
                                    KdProcessorBlock[ Processor ].Tracepoint += CodeLength;

                                    if ( ( Context->EFlags & EFL_PF ) == 0 ) {

                                        KdProcessorBlock[ Processor ].Tracepoint +=  ( LONG64 )( LONG32 )HdeCode.disp.disp32;
                                    }
                                    break;
                                case 0x8C: // Jnge
                                    KdProcessorBlock[ Processor ].Tracepoint += CodeLength;

                                    if ( ( ( Context->EFlags & EFL_SF ) == EFL_SF ) !=
                                        ( ( Context->EFlags & EFL_OF ) == EFL_OF ) ) {

                                        KdProcessorBlock[ Processor ].Tracepoint +=  ( LONG64 )( LONG32 )HdeCode.disp.disp32;
                                    }
                                    break;
                                case 0x8D: // Jge
                                    KdProcessorBlock[ Processor ].Tracepoint += CodeLength;

                                    if ( ( ( Context->EFlags & EFL_SF ) == EFL_SF ) ==
                                        ( ( Context->EFlags & EFL_OF ) == EFL_OF ) ) {

                                        KdProcessorBlock[ Processor ].Tracepoint +=  ( LONG64 )( LONG32 )HdeCode.disp.disp32;
                                    }
                                    break;
                                case 0x8E: // Jng
                                    KdProcessorBlock[ Processor ].Tracepoint += CodeLength;

                                    if ( ( ( Context->EFlags & EFL_ZF ) == EFL_ZF ) ||
                                        ( ( Context->EFlags & EFL_SF ) == EFL_SF ) !=
                                         ( ( Context->EFlags & EFL_OF ) == EFL_OF ) ) {

                                        KdProcessorBlock[ Processor ].Tracepoint +=  ( LONG64 )( LONG32 )HdeCode.disp.disp32;
                                    }
                                    break;
                                case 0x8F: // Jg
                                    KdProcessorBlock[ Processor ].Tracepoint += CodeLength;

                                    if ( ( ( Context->EFlags & EFL_ZF ) == 0 ) &&
                                        ( ( Context->EFlags & EFL_SF ) == EFL_SF ) ==
                                         ( ( Context->EFlags & EFL_OF ) == EFL_OF ) ) {

                                        KdProcessorBlock[ Processor ].Tracepoint +=  ( LONG64 )( LONG32 )HdeCode.disp.disp32;
                                    }
                                    break;
                                default:

                                    break;
                                }

                                break;
                            default:
                                KdProcessorBlock[ Processor ].Tracepoint += CodeLength;
                                break;
                            }

                            RtlCopyMemory( ( void* )KdProcessorBlock[ Processor ].TracepointCode,
                                ( void* )KdProcessorBlock[ Processor ].Tracepoint,
                                           0x20 );

                            RtlCopyMemory( ( void* )KdProcessorBlock[ Processor ].Tracepoint,
                                           KdpBreakpointCode,
                                           KdpBreakpointCodeLength );

                            KeSweepLocalCaches( );
                        }
                        __except ( TRUE ) {


                        }

                        KdPrint( "Tracepoint set: %p -> %p\n",
                                 Context->Rip,
                                 KdProcessorBlock[ Processor ].Tracepoint );
                    }

                    for ( Processor = 0;
                          Processor < KeQueryActiveProcessorCountEx( 0xFFFF );
                          Processor++ ) {

                        KdGetPrcbSpecialRegisters( KeQueryPrcbAddress( Processor ) )->KernelDr6 = 0;
                        KdGetPrcbSpecialRegisters( KeQueryPrcbAddress( Processor ) )->KernelDr7 =
                            Packet.u.Continue2.ControlSet.Dr7;
                    }

                    // write about tf and apc

                    //KdpGetStateChange( &Packet,
                    //                   Context );
                    return TRUE;
                case DbgKdReadPhysicalMemoryApi:
                    KdpReadPhysicalMemory( &Packet,
                                           &Body );
                    break;
                case DbgKdWritePhysicalMemoryApi:
                    KdpWritePhysicalMemory( &Packet,
                                            &Body );
                    break;
                case DbgKdGetVersionApi:
                    KdpGetVersion( &Packet,
                                   &Body );
                    break;
                case DbgKdSetSpecialCallApi:
                case DbgKdClearSpecialCallsApi:
                case DbgKdSetInternalBreakPointApi:
                case DbgKdClearAllInternalBreakpointsApi:
                    continue;
                case DbgKdGetContextEx:
                    KdpGetContextEx( &Packet,
                                     &Body,
                                     Context );
                    break;
                case DbgKdSetContextEx:
                    KdpSetContextEx( &Packet,
                                     &Body,
                                     Context );
                    break;
                case DbgKdQueryMemoryApi:
                    KdpQueryMemory( &Packet,
                                    &Body );
                    break;
                case DbgKdSwitchProcessor:

                    if ( Packet.Processor == KeGetCurrentProcessorNumber( ) ) {

                        goto KdpResendPacket;
                    }

                    //KeSetTargetProcessorDpc( &KdBreakDpc, ( CCHAR )Packet.Processor );
                    //KeInsertQueueDpc( &KdBreakDpc, NULL, NULL );

                    //KeSwitchFrozenProcessor( Packet.Processor );

                    //while ( 1 );

                    KdSwitchFrozenProcessor( Packet.Processor );
                    return TRUE;
                default:
                    DbgPrint( "KdApiNumber unhandled: %#.4lx\n", Packet.ApiNumber );
                    Packet.ReturnStatus = STATUS_UNSUCCESSFUL;
                    Head.Buffer = ( PCHAR )&Packet;
                    Head.Length = sizeof( DBGKD_MANIPULATE_STATE64 );
                    KdDebugDevice.KdSendPacket( KdTypeStateManipulate,
                                                &Head,
                                                NULL,
                                                &KdpContext );
                    //KdPrint( "KdApiNumber unhandled: %#.4lx\n", Packet.ApiNumber );
                    break;
                }

#if KD_RECV_PACKET_LOGGING
                DbgPrint( "%s %lx\n",
                          DbgKdApi[ Packet.ApiNumber - DbgKdMinimumManipulate ],
                          Packet.ReturnStatus );
#endif

            }

#if KD_DEBUG_NO_FREEZE
            //LARGE_INTEGER Time;
            //Time.QuadPart = -500000;
            //KeDelayExecutionThread( KernelMode, FALSE, &Time );
#else
            //KeStallExecutionProcessor( 500 * 1000 );
#endif
        }

        KdDebugDevice.KdSendPacket( KdTypeStateChange,
                                    StateChangeHead,
                                    StateChangeBody,
                                    &KdpContext );
    }

    return TRUE;
}
