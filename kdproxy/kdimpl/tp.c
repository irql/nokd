
#include <kd.h>
#include "../hde64/hde64.h"

//
// This kinda sucks, but a few weeks after writing this,
// I discovered windows already has the exact same shit 
// already implemented, known as "KiDynamicTraceEnabled",
// which emulates instructions too, and places bp's after
// the currently executing one.
//
// => KiTpHandleTrap
//

NTSTATUS
KdpTraceIncrementPc(
    _In_ ULONG64* Pc,
    _In_ ULONG32* Flags,
    _In_ CONTEXT* Context
)
{
    HDE64S  HdeCode;
    ULONG32 CodeLength;
    ULONG64 IncTp;

    IncTp = *Pc;
    *Flags &= ~( KDPR_FLAG_TP_ENABLE | KDPR_FLAG_TP_OWE | KDPR_FLAG_TP_OWE_NO_INC );

    if ( !MmIsAddressValid( ( PVOID )IncTp ) ) {

        //
        // MUST create a timeout for tracepoints, 
        // read comments in some other file.
        //

        *Flags |= KDPR_FLAG_TP_OWE | KDPR_FLAG_TP_ENABLE;
        return STATUS_SUCCESS;
    }

    __try {
        CodeLength = Hde64Decode( ( const void* )IncTp, &HdeCode );

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
        // Instructions: LOOP/Z/NZ, SYSCALL, SYSRET
        //
        // Although this is a little weird, a good note is that rep
        // prefixes are not accounted for, where as they are for
        // trap flag usage.
        //
        // TODO: Sib bytes are not handled.
        //       Segment overrides are ignored.
        //
        // This could also be partially my fault, I don't handle any errors generated 
        // here and assume you don't have your debugger execute junk.
        //

        if ( HdeCode.flags & F_ERROR ||
             HdeCode.Length == 0 ) {

            KdPrint( "WARNING: HDE64 Error.\n" );
            return STATUS_UNSUCCESSFUL;
        }

        switch ( HdeCode.opcode ) {

            //
            // Near return, read the return address from the top of
            // the stack.
            //
        case 0xC3:
        case 0xC2:

            IncTp = *( ULONG64* )Context->Rsp;
            break;

            //
            // Jump rel8
            //
        case 0xEB:
            IncTp += CodeLength;
            IncTp += ( LONG64 )( CHAR )HdeCode.imm.imm8;
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
                IncTp += CodeLength;
                IncTp += ( LONG64 )( LONG32 )HdeCode.imm.imm16;
            }
            else {
                IncTp += CodeLength;
                IncTp += ( LONG64 )( LONG32 )HdeCode.imm.imm32;
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

                        IncTp = *( ULONG64* )(
                            IncTp +
                            CodeLength +
                            ( LONG64 )( LONG32 )HdeCode.disp.disp32 );
                    }
                    else {
                        if ( HdeCode.flags & F_PREFIX_REX ) {

                            IncTp = **( ( ULONG64** )&Context->R8 + HdeCode.modrm_rm );
                        }
                        else {

                            IncTp = **( ( ULONG64** )&Context->Rax + HdeCode.modrm_rm );
                        }
                    }

                    break;
                case 1:
                    //
                    // [ Register + Displacement8 ]
                    //

                    if ( HdeCode.flags & F_PREFIX_REX ) {

                        IncTp = *( ULONG64* )(
                            *( &Context->R8 + HdeCode.modrm_rm ) + ( LONG64 )( CHAR )HdeCode.disp.disp8 );
                    }
                    else {

                        IncTp = *( ULONG64* )(
                            *( &Context->Rax + HdeCode.modrm_rm ) + ( LONG64 )( CHAR )HdeCode.disp.disp8 );
                    }

                    break;
                case 2:
                    //
                    // [ Register + Displacement32 ]
                    //

                    if ( HdeCode.flags & F_PREFIX_REX ) {

                        IncTp = *( ULONG64* )(
                            *( &Context->R8 + HdeCode.modrm_rm ) + ( LONG64 )( LONG32 )HdeCode.disp.disp32 );
                    }
                    else {

                        IncTp = *( ULONG64* )(
                            *( &Context->Rax + HdeCode.modrm_rm ) + ( LONG64 )( LONG32 )HdeCode.disp.disp32 );
                    }

                    break;
                case 3:
                    //
                    // Register
                    //

                    if ( HdeCode.flags & F_PREFIX_REX ) {

                        IncTp = *( &Context->R8 + HdeCode.modrm_rm );
                    }
                    else {

                        IncTp = *( &Context->Rax + HdeCode.modrm_rm );
                    }
                    break;
                default:
                    __assume( 0 );
                }
            }
            else {

                IncTp += CodeLength;
            }

            break;

            //
            // All conditional short jumps
            //
        case 0x70: // Jo
            IncTp += CodeLength;

            if ( ( Context->EFlags & EFL_OF ) == EFL_OF ) {

                IncTp += ( LONG64 )( CHAR )HdeCode.imm.imm8;
            }
            break;
        case 0x71: // Jno
            IncTp += CodeLength;

            if ( ( Context->EFlags & EFL_OF ) == 0 ) {

                IncTp += ( LONG64 )( CHAR )HdeCode.imm.imm8;
            }
            break;
        case 0x72: // Jc
            IncTp += CodeLength;

            if ( ( Context->EFlags & EFL_CF ) == EFL_CF ) {

                IncTp += ( LONG64 )( CHAR )HdeCode.imm.imm8;
            }
            break;
        case 0x73: // Jnc
            IncTp += CodeLength;

            if ( ( Context->EFlags & EFL_CF ) == 0 ) {

                IncTp += ( LONG64 )( CHAR )HdeCode.imm.imm8;
            }
            break;
        case 0x74: // Jz
            IncTp += CodeLength;

            if ( ( Context->EFlags & EFL_ZF ) == EFL_ZF ) {

                IncTp += ( LONG64 )( CHAR )HdeCode.imm.imm8;
            }
            break;
        case 0x75: // Jnz
            IncTp += CodeLength;

            if ( ( Context->EFlags & EFL_ZF ) == 0 ) {

                IncTp += ( LONG64 )( CHAR )HdeCode.imm.imm8;
            }
            break;
        case 0x76: // Jbe
            IncTp += CodeLength;

            if ( ( Context->EFlags & ( EFL_CF | EFL_ZF ) ) != 0 ) {

                IncTp += ( LONG64 )( CHAR )HdeCode.imm.imm8;
            }
            break;
        case 0x77: // Jnbe
            IncTp += CodeLength;

            if ( ( Context->EFlags & ( EFL_CF | EFL_ZF ) ) == 0 ) {

                IncTp += ( LONG64 )( CHAR )HdeCode.imm.imm8;
            }
            break;
        case 0x78: // Js
            IncTp += CodeLength;

            if ( ( Context->EFlags & EFL_SF ) == EFL_SF ) {

                IncTp += ( LONG64 )( CHAR )HdeCode.imm.imm8;
            }
            break;
        case 0x79: // Jns
            IncTp += CodeLength;

            if ( ( Context->EFlags & EFL_SF ) == 0 ) {

                IncTp += ( LONG64 )( CHAR )HdeCode.imm.imm8;
            }
            break;
        case 0x7A: // Jp
            IncTp += CodeLength;

            if ( ( Context->EFlags & EFL_PF ) == EFL_PF ) {

                IncTp += ( LONG64 )( CHAR )HdeCode.imm.imm8;
            }
            break;
        case 0x7B: // Jnp
            IncTp += CodeLength;

            if ( ( Context->EFlags & EFL_PF ) == 0 ) {

                IncTp += ( LONG64 )( CHAR )HdeCode.imm.imm8;
            }
            break;
        case 0x7C: // Jnge
            IncTp += CodeLength;

            if ( ( ( Context->EFlags & EFL_SF ) == EFL_SF ) !=
                ( ( Context->EFlags & EFL_OF ) == EFL_OF ) ) {

                IncTp += ( LONG64 )( CHAR )HdeCode.imm.imm8;
            }
            break;
        case 0x7D: // Jge
            IncTp += CodeLength;

            if ( ( ( Context->EFlags & EFL_SF ) == EFL_SF ) ==
                ( ( Context->EFlags & EFL_OF ) == EFL_OF ) ) {

                IncTp += ( LONG64 )( CHAR )HdeCode.imm.imm8;
            }
            break;
        case 0x7E: // Jng
            IncTp += CodeLength;

            if ( ( ( Context->EFlags & EFL_ZF ) == EFL_ZF ) ||
                ( ( Context->EFlags & EFL_SF ) == EFL_SF ) !=
                 ( ( Context->EFlags & EFL_OF ) == EFL_OF ) ) {

                IncTp += ( LONG64 )( CHAR )HdeCode.imm.imm8;
            }
            break;
        case 0x7F: // Jg
            IncTp += CodeLength;

            if ( ( ( Context->EFlags & EFL_ZF ) == 0 ) &&
                ( ( Context->EFlags & EFL_SF ) == EFL_SF ) ==
                 ( ( Context->EFlags & EFL_OF ) == EFL_OF ) ) {

                IncTp += ( LONG64 )( CHAR )HdeCode.imm.imm8;
            }
            break;
        case 0xE3: // Jrcxz & Jecxz
            IncTp += CodeLength;

            if ( HdeCode.p_67 ) {

                if ( ( Context->Rcx & 0xFFFFFFFF ) == 0 ) {

                    IncTp += ( LONG64 )( CHAR )HdeCode.imm.imm8;
                }
            }
            else {

                if ( Context->Rcx == 0 ) {

                    IncTp += ( LONG64 )( CHAR )HdeCode.imm.imm8;
                }
            }

            break;
            //
            // All conditional near jumps
            //
        case 0x0F:

            switch ( HdeCode.opcode2 ) {
            case 0x80: // Jo
                IncTp += CodeLength;

                if ( ( Context->EFlags & EFL_OF ) == EFL_OF ) {

                    IncTp += ( LONG64 )( LONG32 )HdeCode.disp.disp32;
                }
                break;
            case 0x81: // Jno
                IncTp += CodeLength;

                if ( ( Context->EFlags & EFL_OF ) == 0 ) {

                    IncTp += ( LONG64 )( LONG32 )HdeCode.disp.disp32;
                }
                break;
            case 0x82: // Jc
                IncTp += CodeLength;

                if ( ( Context->EFlags & EFL_CF ) == EFL_CF ) {

                    IncTp += ( LONG64 )( LONG32 )HdeCode.disp.disp32;
                }
                break;
            case 0x83: // Jnc
                IncTp += CodeLength;

                if ( ( Context->EFlags & EFL_CF ) == 0 ) {

                    IncTp += ( LONG64 )( LONG32 )HdeCode.disp.disp32;
                }
                break;
            case 0x84: // Jz
                IncTp += CodeLength;

                if ( ( Context->EFlags & EFL_ZF ) == EFL_ZF ) {

                    IncTp += ( LONG64 )( LONG32 )HdeCode.disp.disp32;
                }
                break;
            case 0x85: // Jnz
                IncTp += CodeLength;

                if ( ( Context->EFlags & EFL_ZF ) == 0 ) {

                    IncTp += ( LONG64 )( LONG32 )HdeCode.disp.disp32;
                }
                break;
            case 0x86: // Jbe
                IncTp += CodeLength;

                if ( ( Context->EFlags & ( EFL_CF | EFL_ZF ) ) != 0 ) {

                    IncTp += ( LONG64 )( LONG32 )HdeCode.disp.disp32;
                }
                break;
            case 0x87: // Jnbe
                IncTp += CodeLength;

                if ( ( Context->EFlags & ( EFL_CF | EFL_ZF ) ) == 0 ) {

                    IncTp += ( LONG64 )( LONG32 )HdeCode.disp.disp32;
                }
                break;
            case 0x88: // Js
                IncTp += CodeLength;

                if ( ( Context->EFlags & EFL_SF ) == EFL_SF ) {

                    IncTp += ( LONG64 )( LONG32 )HdeCode.disp.disp32;
                }
                break;
            case 0x89: // Jns
                IncTp += CodeLength;

                if ( ( Context->EFlags & EFL_SF ) == 0 ) {

                    IncTp += ( LONG64 )( LONG32 )HdeCode.disp.disp32;
                }
                break;
            case 0x8A: // Jp
                IncTp += CodeLength;

                if ( ( Context->EFlags & EFL_PF ) == EFL_PF ) {

                    IncTp += ( LONG64 )( LONG32 )HdeCode.disp.disp32;
                }
                break;
            case 0x8B: // Jnp
                IncTp += CodeLength;

                if ( ( Context->EFlags & EFL_PF ) == 0 ) {

                    IncTp += ( LONG64 )( LONG32 )HdeCode.disp.disp32;
                }
                break;
            case 0x8C: // Jnge
                IncTp += CodeLength;

                if ( ( ( Context->EFlags & EFL_SF ) == EFL_SF ) !=
                    ( ( Context->EFlags & EFL_OF ) == EFL_OF ) ) {

                    IncTp += ( LONG64 )( LONG32 )HdeCode.disp.disp32;
                }
                break;
            case 0x8D: // Jge
                IncTp += CodeLength;

                if ( ( ( Context->EFlags & EFL_SF ) == EFL_SF ) ==
                    ( ( Context->EFlags & EFL_OF ) == EFL_OF ) ) {

                    IncTp += ( LONG64 )( LONG32 )HdeCode.disp.disp32;
                }
                break;
            case 0x8E: // Jng
                IncTp += CodeLength;

                if ( ( ( Context->EFlags & EFL_ZF ) == EFL_ZF ) ||
                    ( ( Context->EFlags & EFL_SF ) == EFL_SF ) !=
                     ( ( Context->EFlags & EFL_OF ) == EFL_OF ) ) {

                    IncTp += ( LONG64 )( LONG32 )HdeCode.disp.disp32;
                }
                break;
            case 0x8F: // Jg
                IncTp += CodeLength;

                if ( ( ( Context->EFlags & EFL_ZF ) == 0 ) &&
                    ( ( Context->EFlags & EFL_SF ) == EFL_SF ) ==
                     ( ( Context->EFlags & EFL_OF ) == EFL_OF ) ) {

                    IncTp += ( LONG64 )( LONG32 )HdeCode.disp.disp32;
                }
                break;
            default:
                IncTp += CodeLength;
                break;
            }

            break;
        default:
            IncTp += CodeLength;
            break;
        }

        if ( !MmIsAddressValid( ( PVOID )IncTp ) ) {

            *Flags |= KDPR_FLAG_TP_OWE_NO_INC;
        }

        *Flags |= KDPR_FLAG_TP_ENABLE;
        *Pc = IncTp;
        return STATUS_SUCCESS;
    }
    __except ( TRUE ) {

        return STATUS_UNSUCCESSFUL;
    }
}

VOID
KdSetTracepoint(
    _In_ CONTEXT* Context
)
{
    NTSTATUS ntStatus;
    ULONG32  Processor;

    //
    // KDPR_FLAG_TP_ENABLE     -> Tracepoint is enabled and set
    // KDPR_FLAG_TP_OWE        -> Tracepoint is enabled but exists
    //                            only in paged-out memory and has
    //                            not been pc-incremented uet.
    // KDPR_FLAG_TP_OWE_NO_INC -> Tracepoint is enabled and incremented,
    //                            but exists in paged-out memory, so we have
    //                            to apply changes on #PF.
    //

    Processor = KdGetCurrentPrcbNumber( );
    KdProcessorBlock[ Processor ].Flags &= ~( KDPR_FLAG_TP_ENABLE |
                                              KDPR_FLAG_TP_OWE |
                                              KDPR_FLAG_TP_OWE_NO_INC );
    KdProcessorBlock[ Processor ].Tracepoint = Context->Rip;

    ntStatus = KdpTraceIncrementPc( &KdProcessorBlock[ Processor ].Tracepoint,
                                    &KdProcessorBlock[ Processor ].Flags,
                                    Context );
    if ( !NT_SUCCESS( ntStatus ) ) {

        KdPrint( "KdSetTracepoint->KdpTraceIncrementPc unsuccessful.\n" );
        return;
    }

    if ( KdProcessorBlock[ Processor ].Flags & ( KDPR_FLAG_TP_OWE | KDPR_FLAG_TP_OWE_NO_INC ) ) {

#if KD_PAGED_MEMORY_FIX
        KdpOweTracepoint++;
        KdOwePrepare( );
#endif

#if KD_TRACE_POINT_LOGGING
        KdPrint( "Owed tracepoint set: %p %x\n",
                 KdProcessorBlock[ Processor ].Tracepoint,
                 KdProcessorBlock[ Processor ].Flags );
#endif

        return;
    }

    __try {

        RtlCopyMemory( ( void* )KdProcessorBlock[ Processor ].TracepointCode,
            ( void* )KdProcessorBlock[ Processor ].Tracepoint,
                       0x20 );

        RtlCopyMemory( ( void* )KdProcessorBlock[ Processor ].Tracepoint,
                       KdpBreakpointCode,
                       KdpBreakpointCodeLength );

        KeSweepLocalCaches( );
    }
    __except ( TRUE ) {

        KdPrint( "KdSetTracepoint unsuccessful.\n" );
        KdProcessorBlock[ Processor ].Flags &= ~( KDPR_FLAG_TP_ENABLE |
                                                  KDPR_FLAG_TP_OWE |
                                                  KDPR_FLAG_TP_OWE_NO_INC );
    }

#if KD_TRACE_POINT_LOGGING
    KdPrint( "Tracepoint set: %p -> %p\n",
             Context->Rip,
             KdProcessorBlock[ Processor ].Tracepoint );
#endif
}

VOID
KdOweTracepoint(
    _In_ ULONG32 Processor
)
{
    //
    // Called by KdPageFault to handle owed tracepoints.
    //

    NTSTATUS ntStatus;

    if ( KdProcessorBlock[ Processor ].Flags & KDPR_FLAG_TP_OWE_NO_INC ) {

        __try {

            RtlCopyMemory( ( void* )KdProcessorBlock[ Processor ].TracepointCode,
                ( void* )KdProcessorBlock[ Processor ].Tracepoint,
                           0x20 );

            RtlCopyMemory( ( void* )KdProcessorBlock[ Processor ].Tracepoint,
                           KdpBreakpointCode,
                           KdpBreakpointCodeLength );

            KeSweepLocalCaches( );

            KdProcessorBlock[ Processor ].Flags &= ~KDPR_FLAG_TP_OWE_NO_INC;
        }
        __except ( TRUE ) {

            //KdPrint( "KdOweTracepoint unsuccessful.\n" );
            KdProcessorBlock[ Processor ].Flags &= ~( KDPR_FLAG_TP_ENABLE |
                                                      KDPR_FLAG_TP_OWE |
                                                      KDPR_FLAG_TP_OWE_NO_INC );
        }
    }

    if ( KdProcessorBlock[ Processor ].Flags & KDPR_FLAG_TP_OWE ) {

        KdProcessorBlock[ Processor ].Flags &= ~KDPR_FLAG_TP_OWE;

        ntStatus = KdpTraceIncrementPc( &KdProcessorBlock[ Processor ].Tracepoint,
                                        &KdProcessorBlock[ Processor ].Flags,
                                        KdGetPrcbContext( KeQueryPrcbAddress( Processor ) ) );
        if ( !NT_SUCCESS( ntStatus ) ) {

            //KdPrint( "KdOweTracepoint->KdpTraceIncrementPc unsuccessful.\n" );
            KdProcessorBlock[ Processor ].Flags &= ~( KDPR_FLAG_TP_ENABLE |
                                                      KDPR_FLAG_TP_OWE |
                                                      KDPR_FLAG_TP_OWE_NO_INC );
            return;
        }

        if ( KdProcessorBlock[ Processor ].Flags & ( KDPR_FLAG_TP_OWE | KDPR_FLAG_TP_OWE_NO_INC ) ) {

            return;
        }

        __try {

            RtlCopyMemory( ( void* )KdProcessorBlock[ Processor ].TracepointCode,
                ( void* )KdProcessorBlock[ Processor ].Tracepoint,
                           0x20 );

            RtlCopyMemory( ( void* )KdProcessorBlock[ Processor ].Tracepoint,
                           KdpBreakpointCode,
                           KdpBreakpointCodeLength );

            KeSweepLocalCaches( );
        }
        __except ( TRUE ) {

            //KdPrint( "KdOweTracepoint unsuccessful.\n" );
            KdProcessorBlock[ Processor ].Flags &= ~( KDPR_FLAG_TP_ENABLE |
                                                      KDPR_FLAG_TP_OWE |
                                                      KDPR_FLAG_TP_OWE_NO_INC );
        }
    }

}
