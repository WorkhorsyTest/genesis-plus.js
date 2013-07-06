/* ======================================================================== */
/*                            MAIN 68K CORE                                 */
/* ======================================================================== */

import types.d;
import m68k.d;


alias m68k m68ki_cpu;
const int MUL = 7;

/* ======================================================================== */
/* ================================ INCLUDES ============================== */
/* ======================================================================== */

version(BUILD_TABLES) {
import m68ki_cycles.d;
}

import m68kconf.d;
import m68kcpu.d;
import m68kops.d;


/* ======================================================================== */
/* ============================ GENERAL DEFINES =========================== */
/* ======================================================================== */

/* Exception Vectors handled by emulation */
static const s32 EXCEPTION_RESET                    = 0;
static const s32 EXCEPTION_BUS_ERROR                = 2; /* This one is not emulated! */
static const s32 EXCEPTION_ADDRESS_ERROR            = 3; /* This one is partially emulated (doesn't stack a proper frame yet) */
static const s32 EXCEPTION_ILLEGAL_INSTRUCTION      = 4;
static const s32 EXCEPTION_ZERO_DIVIDE              = 5;
static const s32 EXCEPTION_CHK                      = 6;
static const s32 EXCEPTION_TRAPV                    = 7;
static const s32 EXCEPTION_PRIVILEGE_VIOLATION      = 8;
static const s32 EXCEPTION_TRACE                    = 9;
static const s32 EXCEPTION_1010                    = 10;
static const s32 EXCEPTION_1111                    = 11;
static const s32 EXCEPTION_FORMAT_ERROR            = 14;
static const s32 EXCEPTION_UNINITIALIZED_INTERRUPT = 15;
static const s32 EXCEPTION_SPURIOUS_INTERRUPT      = 24;
static const s32 EXCEPTION_INTERRUPT_AUTOVECTOR    = 24;
static const s32 EXCEPTION_TRAP_BASE               = 32;

/* Function codes set by CPU during data/address bus activity */
static const s32 FUNCTION_CODE_USER_DATA          = 1;
static const s32 FUNCTION_CODE_USER_PROGRAM       = 2;
static const s32 FUNCTION_CODE_SUPERVISOR_DATA    = 5;
static const s32 FUNCTION_CODE_SUPERVISOR_PROGRAM = 6;
static const s32 FUNCTION_CODE_CPU_SPACE          = 7;

/* Different ways to stop the CPU */
static const s32 STOP_LEVEL_STOP = 1;
static const s32 STOP_LEVEL_HALT = 2;

/* Used for 68000 address error processing */
static if(M68K_EMULATE_ADDRESS_ERROR) {
static const s32 INSTRUCTION_YES = 0;
static const s32 INSTRUCTION_NO  = 0x08;
static const s32 MODE_READ       = 0x10;
static const s32 MODE_WRITE      = 0;

static const s32 RUN_MODE_NORMAL          = 0;
static const s32 RUN_MODE_BERR_AERR_RESET = 1;
}


/* ------------------------------ CPU Access ------------------------------ */

/* Access the CPU registers */
alias m68ki_cpu.dar                 REG_DA;/* easy access to data and address regs */
alias m68ki_cpu.dar                 REG_D;
alias (m68ki_cpu.dar+8)             REG_A;
alias m68ki_cpu.pc                  REG_PC;
alias m68ki_cpu.sp                  REG_SP_BASE;
alias m68ki_cpu.sp[0]               REG_USP;
alias m68ki_cpu.sp[4]               REG_ISP;
alias m68ki_cpu.dar[15]             REG_SP;
alias m68ki_cpu.ir                  REG_IR;

alias m68ki_cpu.t1_flag             FLAG_T1;
alias m68ki_cpu.s_flag              FLAG_S;
alias m68ki_cpu.x_flag              FLAG_X;
alias m68ki_cpu.n_flag              FLAG_N;
alias m68ki_cpu.not_z_flag          FLAG_Z;
alias m68ki_cpu.v_flag              FLAG_V;
alias m68ki_cpu.c_flag              FLAG_C;
alias m68ki_cpu.int_mask            FLAG_INT_MASK;

alias m68ki_cpu.int_level           CPU_INT_LEVEL; /* ASG: changed from CPU_INTS_PENDING */
alias m68ki_cpu.stopped             CPU_STOPPED;
alias 0x00ffffff                    CPU_ADDRESS_MASK;
static if(M68K_EMULATE_ADDRESS_ERROR) {
alias m68ki_cpu.instr_mode          CPU_INSTR_MODE;
alias m68ki_cpu.run_mode            CPU_RUN_MODE;
}

alias m68ki_cycles                  CYC_INSTRUCTION;
alias m68ki_exception_cycle_table   CYC_EXCEPTION;
alias ( -2 * MUL)                   CYC_BCC_NOTAKE_B;
alias (  2 * MUL)                   CYC_BCC_NOTAKE_W;
alias ( -2 * MUL)                   CYC_DBCC_F_NOEXP;
alias (  2 * MUL)                   CYC_DBCC_F_EXP;
alias (  2 * MUL)                   CYC_SCC_R_TRUE;
alias (  4 * MUL)                   CYC_MOVEM_W;
alias (  8 * MUL)                   CYC_MOVEM_L;
alias (  2 * MUL)                   CYC_SHIFT;
alias (132 * MUL)                   CYC_RESET;


/* No need to mask if we are 32 bit */
static if(M68K_INT_GT_32_BIT) {
  static u32 MASK_OUT_ABOVE_32(u32 A) { return A & 0xffffffff; }
  static u32 MASK_OUT_BELOW_32(u32 A) { return A & ~0xffffffff; }
} else {
  static u32 MASK_OUT_ABOVE_32(u32 A) { return A; }
  static u32 MASK_OUT_BELOW_32(u32 A) { return 0; }
} /* M68K_INT_GT_32_BIT */

/* Simulate address lines of 68k family */
static u32 ADDRESS_68K(u32 A) { return A & CPU_ADDRESS_MASK; }

/* Some > 32-bit optimizations */
static if(M68K_INT_GT_32_BIT) {
  /* Shift left and right */
  static u32 LSR_32(u32 A, u32 C) { return A >> C; }
  static u32 LSL_32(u32 A, u32 C) { return A << C; }
} else {
  /* We have to do this because the morons at ANSI decided that shifts
     * by >= data size are undefined.
     */
  static u32 LSR_32(u32 A, u32 C) { return C < 32 ? A >> C : 0; }
  static u32 LSL_32(u32 A, u32 C) { return C < 32 ? A << C : 0; }
} /* M68K_INT_GT_32_BIT */


/* ----------------------------- Configuration ---------------------------- */

/* These defines are dependant on the configuration defines in m68kconf.h */

/* Enable or disable callback functions */
static void m68ki_int_ack(u32 A) { M68K_INT_ACK_CALLBACK(A); }


static if(M68K_TAS_HAS_CALLBACK) {
  static if(M68K_TAS_HAS_CALLBACK == OPT_SPECIFY_HANDLER) {
    void m68ki_tas_callback() { M68K_TAS_CALLBACK(); }
  } else {
    void m68ki_tas_callback() { CALLBACK_TAS_INSTR(); }
  }
} else {
  int m68ki_tas_callback() { return 0; }
} /* M68K_TAS_HAS_CALLBACK */


/* Enable or disable function code emulation */
const s32 m68ki_get_address_space() { return FUNCTION_CODE_USER_DATA; }


/* Enable or disable Address error emulation */
static if(M68K_EMULATE_ADDRESS_ERROR) {
  void m68ki_set_address_error_trap() {
    if(setjmp(m68ki_cpu.aerr_trap) != 0)
    {
      m68ki_exception_address_error();
    }
  }

  void m68ki_check_address_error(u32 ADDR, u32 WRITE_MODE, u32 FC) {
    if(ADDR & 1) {
    {
      if (m68ki_cpu.aerr_enabled)
      {
        m68ki_cpu.aerr_address = ADDR;
        m68ki_cpu.aerr_write_mode = WRITE_MODE;
        m68ki_cpu.aerr_fc = FC;
        longjmp(m68ki_cpu.aerr_trap, 1);
      }
    }
  }
} else {
  void m68ki_set_address_error_trap() {
  }

  void m68ki_check_address_error(u32 ADDR, u32 WRITE_MODE, u32 FC) {
  }
} /* M68K_ADDRESS_ERROR */


/* -------------------------- EA / Operand Access ------------------------- */

/*
 * The general instruction format follows this pattern:
 * .... XXX. .... .YYY
 * where XXX is register X and YYY is register Y
 */

/* Data Register Isolation */
alias (REG_D[(REG_IR >> 9) & 7])         DX;
alias (REG_D[REG_IR & 7])                DY;

/* Address Register Isolation */
alias (REG_A[(REG_IR >> 9) & 7])         AX;
alias (REG_A[REG_IR & 7])                AY;

/* Effective Address Calculations */
static u32 EA_AY_AI_8() { return AY; }                       /* address register indirect */
static u32 EA_AY_AI_16() { return EA_AY_AI_8(); }
static u32 EA_AY_AI_32() { return EA_AY_AI_8(); }
static u32 EA_AY_PI_8() { return AY++; }                                /* postincrement (size = byte) */
static u32 EA_AY_PI_16() { return (AY+=2)-2; }                           /* postincrement (size = word) */
static u32 EA_AY_PI_32() { return (AY+=4)-4; }                           /* postincrement (size = long) */
static u32 EA_AY_PD_8() { return --AY; }                                /* predecrement (size = byte) */
static u32 EA_AY_PD_16() { return AY-=2; }                               /* predecrement (size = word) */
static u32 EA_AY_PD_32() { return AY-=4; }                               /* predecrement (size = long) */
static u32 EA_AY_DI_8() { return AY+ (s16) m68ki_read_imm_16(); } /* displacement */
static u32 EA_AY_DI_16() { return EA_AY_DI_8(); }
static u32 EA_AY_DI_32() { return EA_AY_DI_8(); }
static u32 EA_AY_IX_8() { return m68ki_get_ea_ix(AY); }                   /* indirect + index */
static u32 EA_AY_IX_16() { return EA_AY_IX_8(); }
static u32 EA_AY_IX_32() { return EA_AY_IX_8(); }

static u32 EA_AX_AI_8() { return AX; }
static u32 EA_AX_AI_16() { return EA_AX_AI_8(); }
static u32 EA_AX_AI_32() { return EA_AX_AI_8(); }
static u32 EA_AX_PI_8() { return AX++; }
static u32 EA_AX_PI_16() { return (AX+=2)-2; }
static u32 EA_AX_PI_32() { return (AX+=4)-4; }
static u32 EA_AX_PD_8() { return --AX; }
static u32 EA_AX_PD_16() { return AX-=2; }
static u32 EA_AX_PD_32() { return AX-=4; }
static u32 EA_AX_DI_8() { return AX+ (s16) m68ki_read_imm_16(); }
static u32 EA_AX_DI_16() { return EA_AX_DI_8(); }
static u32 EA_AX_DI_32() { return EA_AX_DI_8(); }
static u32 EA_AX_IX_8() { return m68ki_get_ea_ix(AX); }
static u32 EA_AX_IX_16() { return EA_AX_IX_8(); }
static u32 EA_AX_IX_32() { return EA_AX_IX_8(); }

static u32 EA_A7_PI_8() { return (REG_A[7]+=2)-2; }
static u32 EA_A7_PD_8() { return REG_A[7]-=2; }

static u32 EA_AW_8() { return (s16) m68ki_read_imm_16(); }      /* absolute word */
static u32 EA_AW_16() { return EA_AW_8(); }
static u32 EA_AW_32() { return EA_AW_8(); }
static u32 EA_AL_8() { return m68ki_read_imm_32(); }            /* absolute long */
static u32 EA_AL_16() { return EA_AL_8(); }
static u32 EA_AL_32() { return EA_AL_8(); }
static u32 EA_PCDI_8() { return m68ki_get_ea_pcdi(); }          /* pc indirect + displacement */
static u32 EA_PCDI_16() { return EA_PCDI_8(); }
static u32 EA_PCDI_32() { return EA_PCDI_8(); }
static u32 EA_PCIX_8() { return m68ki_get_ea_pcix(); }          /* pc indirect + index */
static u32 EA_PCIX_16() { return EA_PCIX_8(); }
static u32 EA_PCIX_32() { return EA_PCIX_8(); }

static u32 OPER_I_8() { return m68ki_read_imm_8(); }
static u32 OPER_I_16() { return m68ki_read_imm_16(); }
static u32 OPER_I_32() { return m68ki_read_imm_32(); }


/* --------------------------- Status Register ---------------------------- */

/* Flag Calculation Macros */
static u32 CFLAG_8(u32 A) { return A; }
static u32 CFLAG_16(u32 A) { return A >> 8; }

static if(M68K_INT_GT_32_BIT) {
  static u32 CFLAG_ADD_32(u32 S, u32 D, u32 R) { return R >> 24; }
  static u32 CFLAG_SUB_32(u32 S, u32 D, u32 R) { return R >> 24; }
} else {
  static u32 CFLAG_ADD_32(u32 S, u32 D, u32 R) { return (((S & D) | (~R & (S | D))) >> 23); }
  static u32 CFLAG_SUB_32(u32 S, u32 D, u32 R) { return (((S & R) | (~D & (S | R)))>>23); }
} /* M68K_INT_GT_32_BIT */

static u32 VFLAG_ADD_8(u32 S, u32 D, u32 R) { return (S^R) & (D^R); }
static u32 VFLAG_ADD_16(u32 S, u32 D, u32 R) { return ((S^R) & (D^R)) >> 8; }
static u32 VFLAG_ADD_32(u32 S, u32 D, u32 R) { return ((S^R) & (D^R)) >> 24; }

static u32 VFLAG_SUB_8(u32 S, u32 D, u32 R) { return (S^D) & (R^D); }
static u32 VFLAG_SUB_16(u32 S, u32 D, u32 R) { return ((S^D) & (R^D)) >> 8; }
static u32 VFLAG_SUB_32(u32 S, u32 D, u32 R) { return ((S^D) & (R^D)) >> 24; }

static u32 NFLAG_8(u32 A) { return A; }
static u32 NFLAG_16(u32 A) { return A >> 8; }
static u32 NFLAG_32(u32 A) { return A >> 24; }
static u64 NFLAG_64(u64 A) { return A >> 56; }

static u32 ZFLAG_8(s32 A) { return MASK_OUT_ABOVE_8(A); }
static u32 ZFLAG_16(s32 A) { return MASK_OUT_ABOVE_16(A); }
static u32 ZFLAG_32(s32 A) { return MASK_OUT_ABOVE_32(A); }


/* Flag values */
static const s32 NFLAG_SET   = 0x80;
static const s32 NFLAG_CLEAR = 0;
static const s32 CFLAG_SET   = 0x100;
static const s32 CFLAG_CLEAR = 0;
static const s32 XFLAG_SET   = 0x100;
static const s32 XFLAG_CLEAR = 0;
static const s32 VFLAG_SET   = 0x80;
static const s32 VFLAG_CLEAR = 0;
static const s32 ZFLAG_SET   = 0;
static const s32 ZFLAG_CLEAR = 0xffffffff;
static const s32 SFLAG_SET   = 4;
static const s32 SFLAG_CLEAR = 0;

/* Turn flag values into 1 or 0 */
static u32 XFLAG_AS_1() { return (FLAG_X>>8)&1; }
static u32 NFLAG_AS_1() { return (FLAG_N>>7)&1; }
static u32 VFLAG_AS_1() { return (FLAG_V>>7)&1; }
static u32 ZFLAG_AS_1() { return !FLAG_Z; }
static u32 CFLAG_AS_1() { return (FLAG_C>>8)&1; }


/* Conditions */
static u32 COND_CS() { return FLAG_C & 0x100; }
static u32 COND_CC() { return !COND_CS(); }
static u32 COND_VS() { return FLAG_V & 0x80; }
static u32 COND_VC() { return !COND_VS(); }
static u32 COND_NE() { return FLAG_Z; }
static u32 COND_EQ() { return !COND_NE(); }
static u32 COND_MI() { return FLAG_N & 0x80; }
static u32 COND_PL() { return !COND_MI(); }
static u32 COND_LT() { return (FLAG_N^FLAG_V) & 0x80; }
static u32 COND_GE() { return !COND_LT(); }
static u32 COND_HI() { return COND_CC() && COND_NE(); }
static u32 COND_LS() { return COND_CS() || COND_EQ(); }
static u32 COND_GT() { return COND_GE() && COND_NE(); }
static u32 COND_LE() { return COND_LT() || COND_EQ(); }

/* Reversed conditions */
static u32 COND_NOT_CS() { return COND_CC(); }
static u32 COND_NOT_CC() { return COND_CS(); }
static u32 COND_NOT_VS() { return COND_VC(); }
static u32 COND_NOT_VC() { return COND_VS(); }
static u32 COND_NOT_NE() { return COND_EQ(); }
static u32 COND_NOT_EQ() { return COND_NE(); }
static u32 COND_NOT_MI() { return COND_PL(); }
static u32 COND_NOT_PL() { return COND_MI(); }
static u32 COND_NOT_LT() { return COND_GE(); }
static u32 COND_NOT_GE() { return COND_LT(); }
static u32 COND_NOT_HI() { return COND_LS(); }
static u32 COND_NOT_LS() { return COND_HI(); }
static u32 COND_NOT_GT() { return COND_LE(); }
static u32 COND_NOT_LE() { return COND_GT(); }

/* Not real conditions, but here for convenience */
static u32 COND_XS() { return FLAG_X & 0x100; }
static u32 COND_XC() { return !COND_XS; }


/* Get the condition code register */
static u32 m68ki_get_ccr() {
    return (COND_XS() >> 4) | 
             (COND_MI() >> 4) | 
             (COND_EQ() << 2) | 
             (COND_VS() >> 6) | 
             (COND_CS() >> 8);
}

/* Get the status register */
static u32 m68ki_get_sr() {
    return FLAG_T1  | 
            (FLAG_S        << 11) | 
             FLAG_INT_MASK        | 
             m68ki_get_ccr();
}



/* ---------------------------- Cycle Counting ---------------------------- */

static void USE_CYCLES(u32 A) { m68ki_cpu.cycles += A; }
static void SET_CYCLES(u32 A) { m68ki_cpu.cycles = A; }


/* ----------------------------- Read / Write ----------------------------- */

/* Read data immediately following the PC */
static u32 m68k_read_immediate_16(u32 address) {
	return *(u16 *)(m68ki_cpu.memory_map[((address)>>16)&0xff].base + ((address) & 0xffff));
}
static u32 m68k_read_immediate_32(u32 address) {
	return (m68k_read_immediate_16(address) << 16) | (m68k_read_immediate_16(address+2));
}

/* Read data relative to the PC */
static u32 m68k_read_pcrelative_8(u32 address) {
    return READ_BYTE(m68ki_cpu.memory_map[((address)>>16)&0xff].base, (address) & 0xffff);
}
static u32 m68k_read_pcrelative_16(u32 address) {
    return m68k_read_immediate_16(address);
}
static u32 m68k_read_pcrelative_32(u32 address) {
    return m68k_read_immediate_32(address);
}

/* Read from the current address space */
static u32 m68ki_read_8(u32 A) { return m68ki_read_8_fc(A); }
static u32 m68ki_read_16(u32 A) { return m68ki_read_16_fc(A, FLAG_S | m68ki_get_address_space()); }
static u32 m68ki_read_32(u32 A) { return m68ki_read_32_fc(A, FLAG_S | m68ki_get_address_space()); }

/* Write to the current data space */
static void m68ki_write_8(u32 A, u32 V) { m68ki_write_8_fc (A, V); }
static void m68ki_write_16(u32 A, u32 V) { m68ki_write_16_fc(A, FLAG_S | FUNCTION_CODE_USER_DATA, V); }
static void m68ki_write_32(u32 A, u32 V) { m68ki_write_32_fc(A, FLAG_S | FUNCTION_CODE_USER_DATA, V); }

/* map read immediate 8 to read immediate 16 */
static u32 m68ki_read_imm_8() { return MASK_OUT_ABOVE_8(m68ki_read_imm_16()); }

/* Map PC-relative reads */
static u32 m68ki_read_pcrel_8(u32 A) { return m68k_read_pcrelative_8(A); }
static u32 m68ki_read_pcrel_16(u32 A) { return m68k_read_pcrelative_16(A); }
static u32 m68ki_read_pcrel_32(u32 A) { return m68k_read_pcrelative_32(A); }

/* Read from the program space */
static u32 m68ki_read_program_8(u32 A) { return m68ki_read_8_fc(A); }
static u32 m68ki_read_program_16(u32 A) { return m68ki_read_16_fc(A, FLAG_S | FUNCTION_CODE_USER_PROGRAM); }
static u32 m68ki_read_program_32(u32 A) { return m68ki_read_32_fc(A, FLAG_S | FUNCTION_CODE_USER_PROGRAM); }

/* Read from the data space */
static u32 m68ki_read_data_8(u32 A) { return m68ki_read_8_fc(A); }
static u32 m68ki_read_data_16(u32 A) { return m68ki_read_16_fc(A, FLAG_S | FUNCTION_CODE_USER_DATA); }
static u32 m68ki_read_data_32(u32 A) { return m68ki_read_32_fc(A, FLAG_S | FUNCTION_CODE_USER_DATA); }



/* ======================================================================== */
/* =============================== PROTOTYPES ============================= */
/* ======================================================================== */

/* Used by shift & rotate instructions */
static const u8[65] m68ki_shift_8_table =
[
  0x00, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff
];

static const u16[65] m68ki_shift_16_table =
[
  0x0000, 0x8000, 0xc000, 0xe000, 0xf000, 0xf800, 0xfc00, 0xfe00, 0xff00,
  0xff80, 0xffc0, 0xffe0, 0xfff0, 0xfff8, 0xfffc, 0xfffe, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff
];

static const u32[65] m68ki_shift_32_table =
[
  0x00000000, 0x80000000, 0xc0000000, 0xe0000000, 0xf0000000, 0xf8000000,
  0xfc000000, 0xfe000000, 0xff000000, 0xff800000, 0xffc00000, 0xffe00000,
  0xfff00000, 0xfff80000, 0xfffc0000, 0xfffe0000, 0xffff0000, 0xffff8000,
  0xffffc000, 0xffffe000, 0xfffff000, 0xfffff800, 0xfffffc00, 0xfffffe00,
  0xffffff00, 0xffffff80, 0xffffffc0, 0xffffffe0, 0xfffffff0, 0xfffffff8,
  0xfffffffc, 0xfffffffe, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
  0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
  0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
  0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
  0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
  0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff
];


/* Number of clock cycles to use for exception processing.
 * I used 4 for any vectors that are undocumented for processing times.
 */
static const u16[256] m68ki_exception_cycle_table =
[
     40*MUL, /*  0: Reset - Initial Stack Pointer                      */
      4*MUL, /*  1: Reset - Initial Program Counter                    */
     50*MUL, /*  2: Bus Error                             (unemulated) */
     50*MUL, /*  3: Address Error                         (unemulated) */
     34*MUL, /*  4: Illegal Instruction                                */
     38*MUL, /*  5: Divide by Zero -- ASG: changed from 42             */
     40*MUL, /*  6: CHK -- ASG: chanaged from 44                       */
     34*MUL, /*  7: TRAPV                                              */
     34*MUL, /*  8: Privilege Violation                                */
     34*MUL, /*  9: Trace                                              */
      4*MUL, /* 10: 1010                                               */
      4*MUL, /* 11: 1111                                               */
      4*MUL, /* 12: RESERVED                                           */
      4*MUL, /* 13: Coprocessor Protocol Violation        (unemulated) */
      4*MUL, /* 14: Format Error                                       */
     44*MUL, /* 15: Uninitialized Interrupt                            */
      4*MUL, /* 16: RESERVED                                           */
      4*MUL, /* 17: RESERVED                                           */
      4*MUL, /* 18: RESERVED                                           */
      4*MUL, /* 19: RESERVED                                           */
      4*MUL, /* 20: RESERVED                                           */
      4*MUL, /* 21: RESERVED                                           */
      4*MUL, /* 22: RESERVED                                           */
      4*MUL, /* 23: RESERVED                                           */
     44*MUL, /* 24: Spurious Interrupt                                 */
     44*MUL, /* 25: Level 1 Interrupt Autovector                       */
     44*MUL, /* 26: Level 2 Interrupt Autovector                       */
     44*MUL, /* 27: Level 3 Interrupt Autovector                       */
     44*MUL, /* 28: Level 4 Interrupt Autovector                       */
     44*MUL, /* 29: Level 5 Interrupt Autovector                       */
     44*MUL, /* 30: Level 6 Interrupt Autovector                       */
     44*MUL, /* 31: Level 7 Interrupt Autovector                       */
     34*MUL, /* 32: TRAP #0 -- ASG: chanaged from 38                   */
     34*MUL, /* 33: TRAP #1                                            */
     34*MUL, /* 34: TRAP #2                                            */
     34*MUL, /* 35: TRAP #3                                            */
     34*MUL, /* 36: TRAP #4                                            */
     34*MUL, /* 37: TRAP #5                                            */
     34*MUL, /* 38: TRAP #6                                            */
     34*MUL, /* 39: TRAP #7                                            */
     34*MUL, /* 40: TRAP #8                                            */
     34*MUL, /* 41: TRAP #9                                            */
     34*MUL, /* 42: TRAP #10                                           */
     34*MUL, /* 43: TRAP #11                                           */
     34*MUL, /* 44: TRAP #12                                           */
     34*MUL, /* 45: TRAP #13                                           */
     34*MUL, /* 46: TRAP #14                                           */
     34*MUL, /* 47: TRAP #15                                           */
      4*MUL, /* 48: FP Branch or Set on Unknown Condition (unemulated) */
      4*MUL, /* 49: FP Inexact Result                     (unemulated) */
      4*MUL, /* 50: FP Divide by Zero                     (unemulated) */
      4*MUL, /* 51: FP Underflow                          (unemulated) */
      4*MUL, /* 52: FP Operand Error                      (unemulated) */
      4*MUL, /* 53: FP Overflow                           (unemulated) */
      4*MUL, /* 54: FP Signaling NAN                      (unemulated) */
      4*MUL, /* 55: FP Unimplemented Data Type            (unemulated) */
      4*MUL, /* 56: MMU Configuration Error               (unemulated) */
      4*MUL, /* 57: MMU Illegal Operation Error           (unemulated) */
      4*MUL, /* 58: MMU Access Level Violation Error      (unemulated) */
      4*MUL, /* 59: RESERVED                                           */
      4*MUL, /* 60: RESERVED                                           */
      4*MUL, /* 61: RESERVED                                           */
      4*MUL, /* 62: RESERVED                                           */
      4*MUL, /* 63: RESERVED                                           */
         /* 64-255: User Defined                                   */
      4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,
      4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,
      4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,
      4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,
      4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,
      4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL,4*MUL
];

/* Bit Isolation Macros */
u32 BIT_0(u32 A) { return A & 0x00000001; }
u32 BIT_1(u32 A) { return A & 0x00000002; }
u32 BIT_2(u32 A) { return A & 0x00000004; }
u32 BIT_3(u32 A) { return A & 0x00000008; }
u32 BIT_4(u32 A) { return A & 0x00000010; }
u32 BIT_5(u32 A) { return A & 0x00000020; }
u32 BIT_6(u32 A) { return A & 0x00000040; }
u32 BIT_7(u32 A) { return A & 0x00000080; }
u32 BIT_8(u32 A) { return A & 0x00000100; }
u32 BIT_9(u32 A) { return A & 0x00000200; }
u32 BIT_A(u32 A) { return A & 0x00000400; }
u32 BIT_B(u32 A) { return A & 0x00000800; }
u32 BIT_C(u32 A) { return A & 0x00001000; }
u32 BIT_D(u32 A) { return A & 0x00002000; }
u32 BIT_E(u32 A) { return A & 0x00004000; }
u32 BIT_F(u32 A) { return A & 0x00008000; }
u32 BIT_10(u32 A){ return A & 0x00010000; }
u32 BIT_11(u32 A){ return A & 0x00020000; }
u32 BIT_12(u32 A){ return A & 0x00040000; }
u32 BIT_13(u32 A){ return A & 0x00080000; }
u32 BIT_14(u32 A){ return A & 0x00100000; }
u32 BIT_15(u32 A){ return A & 0x00200000; }
u32 BIT_16(u32 A){ return A & 0x00400000; }
u32 BIT_17(u32 A){ return A & 0x00800000; }
u32 BIT_18(u32 A){ return A & 0x01000000; }
u32 BIT_19(u32 A){ return A & 0x02000000; }
u32 BIT_1A(u32 A){ return A & 0x04000000; }
u32 BIT_1B(u32 A){ return A & 0x08000000; }
u32 BIT_1C(u32 A){ return A & 0x10000000; }
u32 BIT_1D(u32 A){ return A & 0x20000000; }
u32 BIT_1E(u32 A){ return A & 0x40000000; }
u32 BIT_1F(u32 A){ return A & 0x80000000; }

/* Get the most significant bit for specific sizes */
u32 GET_MSB_8(u32 A) { return A & 0x80; }
u32 GET_MSB_9(u32 A) { return A & 0x100; }
u32 GET_MSB_16(u32 A) { return A & 0x8000; }
u32 GET_MSB_17(u32 A) { return A & 0x10000; }
u32 GET_MSB_32(u32 A) { return A & 0x80000000; }
//u32 GET_MSB_33(u32 A) { return A & 0x100000000; }

/* Isolate nibbles */
u32 LOW_NIBBLE(u32 A) { return A & 0x0f; }
u32 HIGH_NIBBLE(u32 A) { return A & 0xf0; }

/* These are used to isolate 8, 16, and 32 bit sizes */
u32 MASK_OUT_ABOVE_2(u32 A) { return A & 3; }
u32 MASK_OUT_ABOVE_8(u32 A) { return A & 0xff; }
u32 MASK_OUT_ABOVE_16(u32 A) { return A & 0xffff; }
u32 MASK_OUT_BELOW_2(u32 A) { return A & ~3; }
u32 MASK_OUT_BELOW_8(u32 A) { return A & ~0xff; }
u32 MASK_OUT_BELOW_16(u32 A) { return A & ~0xffff; }

/* Shift & Rotate Macros. */
u32 LSL(u32 A, u32 C) { return A << C; }
u32 LSR(u32 A, u32 C) { return A >> C; }

u32 ROL_8(u32 A, u32 C)  { return   MASK_OUT_ABOVE_8(LSL(A, C) | LSR(A, 8-(C))); }
u32 ROL_9(u32 A, u32 C)  { return                   (LSL(A, C) | LSR(A, 9-(C))); }
u32 ROL_16(u32 A, u32 C) { return  MASK_OUT_ABOVE_16(LSL(A, C) | LSR(A, 16-(C))); }
u32 ROL_17(u32 A, u32 C) { return   (LSL(A, C) | LSR(A, 17-(C))); }
u32 ROL_32(u32 A, u32 C) { return  MASK_OUT_ABOVE_32(LSL_32(A, C) | LSR_32(A, 32-(C))); }
u32 ROL_33(u32 A, u32 C) { return                   (LSL_32(A, C) | LSR_32(A, 33-(C))); }

u32 ROR_8(u32 A, u32 C)  { return   MASK_OUT_ABOVE_8(LSR(A, C) | LSL(A, 8-(C))); }
u32 ROR_9(u32 A, u32 C)  { return                   (LSR(A, C) | LSL(A, 9-(C))); }
u32 ROR_16(u32 A, u32 C) { return  MASK_OUT_ABOVE_16(LSR(A, C) | LSL(A, 16-(C))); }
u32 ROR_17(u32 A, u32 C) { return                   (LSR(A, C) | LSL(A, 17-(C))); }
u32 ROR_32(u32 A, u32 C) { return  MASK_OUT_ABOVE_32(LSR_32(A, C) | LSL_32(A, 32-(C))); }
u32 ROR_33(u32 A, u32 C) { return                   (LSR_32(A, C) | LSL_32(A, 33-(C))); }
/* ======================================================================== */
/* ================================= DATA ================================= */
/* ======================================================================== */

version(BUILD_TABLES) {
static u8 m68ki_cycles[0x10000];
}

static s32 irq_latency;

m68ki_cpu_core m68k;


/* ======================================================================== */
/* =============================== CALLBACKS ============================== */
/* ======================================================================== */

/* Default callbacks used if the callback hasn't been set yet, or if the
 * callback is set to NULL
 */


/* ======================================================================== */
/* ================================= API ================================== */
/* ======================================================================== */

/* Access the internals of the CPU */
u32 m68k_get_reg(m68k_register_t regnum)
{
  switch(regnum)
  {
    case M68K_REG_D0:  return m68ki_cpu.dar[0];
    case M68K_REG_D1:  return m68ki_cpu.dar[1];
    case M68K_REG_D2:  return m68ki_cpu.dar[2];
    case M68K_REG_D3:  return m68ki_cpu.dar[3];
    case M68K_REG_D4:  return m68ki_cpu.dar[4];
    case M68K_REG_D5:  return m68ki_cpu.dar[5];
    case M68K_REG_D6:  return m68ki_cpu.dar[6];
    case M68K_REG_D7:  return m68ki_cpu.dar[7];
    case M68K_REG_A0:  return m68ki_cpu.dar[8];
    case M68K_REG_A1:  return m68ki_cpu.dar[9];
    case M68K_REG_A2:  return m68ki_cpu.dar[10];
    case M68K_REG_A3:  return m68ki_cpu.dar[11];
    case M68K_REG_A4:  return m68ki_cpu.dar[12];
    case M68K_REG_A5:  return m68ki_cpu.dar[13];
    case M68K_REG_A6:  return m68ki_cpu.dar[14];
    case M68K_REG_A7:  return m68ki_cpu.dar[15];
    case M68K_REG_PC:  return MASK_OUT_ABOVE_32(m68ki_cpu.pc);
    case M68K_REG_SR:  return  m68ki_cpu.t1_flag        |
                  (m68ki_cpu.s_flag << 11)              |
                   m68ki_cpu.int_mask                   |
                  ((m68ki_cpu.x_flag & XFLAG_SET) >> 4) |
                  ((m68ki_cpu.n_flag & NFLAG_SET) >> 4) |
                  ((!m68ki_cpu.not_z_flag) << 2)        |
                  ((m68ki_cpu.v_flag & VFLAG_SET) >> 6) |
                  ((m68ki_cpu.c_flag & CFLAG_SET) >> 8);
    case M68K_REG_SP:  return m68ki_cpu.dar[15];
    case M68K_REG_USP:  return m68ki_cpu.s_flag ? m68ki_cpu.sp[0] : m68ki_cpu.dar[15];
    case M68K_REG_ISP:  return m68ki_cpu.s_flag ? m68ki_cpu.dar[15] : m68ki_cpu.sp[4];
    case M68K_REG_IR:  return m68ki_cpu.ir;
    default:      return 0;
  }
}

void m68k_set_reg(m68k_register_t regnum, u32 value)
{
  switch(regnum)
  {
    case M68K_REG_D0:  REG_D[0] = MASK_OUT_ABOVE_32(value); return;
    case M68K_REG_D1:  REG_D[1] = MASK_OUT_ABOVE_32(value); return;
    case M68K_REG_D2:  REG_D[2] = MASK_OUT_ABOVE_32(value); return;
    case M68K_REG_D3:  REG_D[3] = MASK_OUT_ABOVE_32(value); return;
    case M68K_REG_D4:  REG_D[4] = MASK_OUT_ABOVE_32(value); return;
    case M68K_REG_D5:  REG_D[5] = MASK_OUT_ABOVE_32(value); return;
    case M68K_REG_D6:  REG_D[6] = MASK_OUT_ABOVE_32(value); return;
    case M68K_REG_D7:  REG_D[7] = MASK_OUT_ABOVE_32(value); return;
    case M68K_REG_A0:  REG_A[0] = MASK_OUT_ABOVE_32(value); return;
    case M68K_REG_A1:  REG_A[1] = MASK_OUT_ABOVE_32(value); return;
    case M68K_REG_A2:  REG_A[2] = MASK_OUT_ABOVE_32(value); return;
    case M68K_REG_A3:  REG_A[3] = MASK_OUT_ABOVE_32(value); return;
    case M68K_REG_A4:  REG_A[4] = MASK_OUT_ABOVE_32(value); return;
    case M68K_REG_A5:  REG_A[5] = MASK_OUT_ABOVE_32(value); return;
    case M68K_REG_A6:  REG_A[6] = MASK_OUT_ABOVE_32(value); return;
    case M68K_REG_A7:  REG_A[7] = MASK_OUT_ABOVE_32(value); return;
    case M68K_REG_PC:  m68ki_jump(MASK_OUT_ABOVE_32(value)); return;
    case M68K_REG_SR:  m68ki_set_sr(value); return;
    case M68K_REG_SP:  REG_SP = MASK_OUT_ABOVE_32(value); return;
    case M68K_REG_USP:  if(FLAG_S)
                REG_USP = MASK_OUT_ABOVE_32(value);
              else
                REG_SP = MASK_OUT_ABOVE_32(value);
              return;
    case M68K_REG_ISP:  if(FLAG_S)
                REG_SP = MASK_OUT_ABOVE_32(value);
              else
                REG_ISP = MASK_OUT_ABOVE_32(value);
              return;
    case M68K_REG_IR:  REG_IR = MASK_OUT_ABOVE_16(value); return;
    default:      return;
  }
}

/* Set the callbacks */

version(LOGVDP) {
//extern void error(char *format, ...);
//extern u16 v_counter;
}

/* ASG: rewrote so that the int_level is a mask of the IPL0/IPL1/IPL2 bits */
/* KS: Modified so that IPL* bits match with mask positions in the SR
 *     and cleaned out remenants of the interrupt controller.
 */
void m68k_update_irq(u32 mask)
{
  /* Update IRQ level */
  CPU_INT_LEVEL |= (mask << 8);
  
version(LOGVDP) {
  error("[%d(%d)][%d(%d)] IRQ Level = %d(0x%02x) (%x)\n", v_counter, m68k.cycles/3420, m68k.cycles, m68k.cycles%3420,CPU_INT_LEVEL>>8,FLAG_INT_MASK,m68k_get_reg(M68K_REG_PC));
}
}

void m68k_set_irq(u32 int_level)
{
  /* Set IRQ level */
  CPU_INT_LEVEL = int_level << 8;
  
version(LOGVDP) {
  error("[%d(%d)][%d(%d)] IRQ Level = %d(0x%02x) (%x)\n", v_counter, m68k.cycles/3420, m68k.cycles, m68k.cycles%3420,CPU_INT_LEVEL>>8,FLAG_INT_MASK,m68k_get_reg(M68K_REG_PC));
}
}

/* IRQ latency (Fatal Rewind, Sesame's Street Counting Cafe)*/
void m68k_set_irq_delay(u32 int_level)
{
  /* Prevent reentrance */
  if (!irq_latency)
  {
    /* This is always triggered from MOVE instructions (VDP CTRL port write) */
    /* We just make sure this is not a MOVE.L instruction as we could be in */
    /* the middle of its execution (first memory write).                   */
    if ((REG_IR & 0xF000) != 0x2000)
    {
      /* Finish executing current instruction */
      USE_CYCLES(CYC_INSTRUCTION[REG_IR]);

      /* One instruction delay before interrupt */
      irq_latency = 1;
      REG_IR = m68ki_read_imm_16();
      m68ki_instruction_jump_table[REG_IR]();
      irq_latency = 0;
    }

    /* Set IRQ level */
    CPU_INT_LEVEL = int_level << 8;
  }
  
version(LOGVDP) {
  error("[%d(%d)][%d(%d)] IRQ Level = %d(0x%02x) (%x)\n", v_counter, m68k.cycles/3420, m68k.cycles, m68k.cycles%3420,CPU_INT_LEVEL>>8,FLAG_INT_MASK,m68k_get_reg(M68K_REG_PC));
}

  /* Check interrupt mask to process IRQ  */
  m68ki_check_interrupts(); /* Level triggered (IRQ) */
}

void m68k_run(u32 cycles) 
{
  /* Make sure CPU is not already ahead */
  if (m68k.cycles >= cycles)
  {
    return;
  }

  /* Check interrupt mask to process IRQ if needed */
  m68ki_check_interrupts();

  /* Make sure we're not stopped */
  if (CPU_STOPPED)
  {
    m68k.cycles = cycles;
    return;
  }

  /* Save end cycles count for when CPU is stopped */
  m68k.cycle_end = cycles;

  /* Return point for when we have an address error (TODO: use goto) */
  m68ki_set_address_error_trap(); /* auto-disable (see m68kcpu.h) */

version(LOGVDP) {
  error("[%d][%d] m68k run to %d cycles (%x)\n", v_counter, m68k.cycles, cycles, m68k.pc);
}
   
  while (m68k.cycles < cycles)
  {
    /* Decode next instruction */
    REG_IR = m68ki_read_imm_16();
	
    /* Execute instruction */
	m68ki_instruction_jump_table[REG_IR]();
    USE_CYCLES(CYC_INSTRUCTION[REG_IR]);
  }
}

void m68k_init()
{
version(BUILD_TABLES) {
  static uint emulation_initialized = 0;

  /* The first call to this function initializes the opcode handler jump table */
  if(!emulation_initialized)
  {
    m68ki_build_opcode_table();
    emulation_initialized = 1;
  }
}
}

/* Pulse the RESET line on the CPU */
void m68k_pulse_reset()
{
  /* Clear all stop levels */
  CPU_STOPPED = 0;
static if(M68K_EMULATE_ADDRESS_ERROR) {
  CPU_RUN_MODE = RUN_MODE_BERR_AERR_RESET;
}

  /* Turn off tracing */
  FLAG_T1 = 0;

  /* Interrupt mask to level 7 */
  FLAG_INT_MASK = 0x0700;
  CPU_INT_LEVEL = 0;
  irq_latency = 0;

  /* Go to supervisor mode */
  m68ki_set_s_flag(SFLAG_SET);

  /* Read the initial stack pointer and program counter */
  m68ki_jump(0);
  REG_SP = m68ki_read_imm_32();
  REG_PC = m68ki_read_imm_32();
  m68ki_jump(REG_PC);

static if(M68K_EMULATE_ADDRESS_ERROR) {
  CPU_RUN_MODE = RUN_MODE_NORMAL;
}

  USE_CYCLES(CYC_EXCEPTION[EXCEPTION_RESET]);
}

void m68k_pulse_halt()
{
  /* Pulse the HALT line on the CPU */
  CPU_STOPPED |= STOP_LEVEL_HALT;
}

void m68k_clear_halt()
{
  /* Clear the HALT line on the CPU */
  CPU_STOPPED &= ~STOP_LEVEL_HALT;
}


/* ======================================================================== */
/* =========================== UTILITY FUNCTIONS ========================== */
/* ======================================================================== */


/* ---------------------------- Read Immediate ---------------------------- */

/* Handles all immediate reads, does address error check, function code setting,
 * and prefetching if they are enabled in m68kconf.h
 */
u32 m68ki_read_imm_16()
{
  u32 pc = REG_PC;
  REG_PC += 2;
  return m68k_read_immediate_16(pc);
}

u32 m68ki_read_imm_32()
{
  u32 pc = REG_PC;
  REG_PC += 4;
  return m68k_read_immediate_32(pc);
}



/* ------------------------- Top level read/write ------------------------- */

/* Handles all memory accesses (except for immediate reads if they are
 * configured to use separate functions in m68kconf.h).
 * All memory accesses must go through these top level functions.
 * These functions will also check for address error and set the function
 * code if they are enabled in m68kconf.h.
 */
u32 m68ki_read_8_fc(u32 address)
{
  cpu_memory_map *temp = &m68ki_cpu.memory_map[((address)>>16)&0xff];

  if (temp->read8) return (*temp->read8)(ADDRESS_68K(address));
  else return READ_BYTE(temp->base, (address) & 0xffff);
}

u32 m68ki_read_16_fc(u32 address, u32 fc)
{
  cpu_memory_map *temp;
  m68ki_check_address_error(address, MODE_READ, fc) /* auto-disable (see m68kcpu.h) */
  
  temp = &m68ki_cpu.memory_map[((address)>>16)&0xff];
  if (temp->read16) return (*temp->read16)(ADDRESS_68K(address));
  else return *(u16 *)(temp->base + ((address) & 0xffff));
}

u32 m68ki_read_32_fc(u32 address, u32 fc)
{
  cpu_memory_map *temp;

  m68ki_check_address_error(address, MODE_READ, fc) /* auto-disable (see m68kcpu.h) */

  temp = &m68ki_cpu.memory_map[((address)>>16)&0xff];
  if (temp->read16) return ((*temp->read16)(ADDRESS_68K(address)) << 16) | ((*temp->read16)(ADDRESS_68K(address + 2)));
  else return m68k_read_immediate_32(address);
}

void m68ki_write_8_fc(u32 address, u32 value)
{
  cpu_memory_map *temp;

  temp = &m68ki_cpu.memory_map[((address)>>16)&0xff];
  if (temp->write8) (*temp->write8)(ADDRESS_68K(address),value);
  else WRITE_BYTE(temp->base, (address) & 0xffff, value);
}

void m68ki_write_16_fc(u32 address, u32 fc, u32 value)
{
  cpu_memory_map *temp;

  m68ki_check_address_error(address, MODE_WRITE, fc); /* auto-disable (see m68kcpu.h) */

  temp = &m68ki_cpu.memory_map[((address)>>16)&0xff];
  if (temp->write16) (*temp->write16)(ADDRESS_68K(address),value);
  else *(u16 *)(temp->base + ((address) & 0xffff)) = value;
}

void m68ki_write_32_fc(u32 address, u32 fc, u32 value)
{
  cpu_memory_map *temp;

  m68ki_check_address_error(address, MODE_WRITE, fc) /* auto-disable (see m68kcpu.h) */

  temp = &m68ki_cpu.memory_map[((address)>>16)&0xff];
  if (temp->write16) (*temp->write16)(ADDRESS_68K(address),value>>16);
  else *(u16 *)(temp->base + ((address) & 0xffff)) = value >> 16;

  temp = &m68ki_cpu.memory_map[((address + 2)>>16)&0xff];
  if (temp->write16) (*temp->write16)(ADDRESS_68K(address+2),value&0xffff);
  else *(u16 *)(temp->base + ((address + 2) & 0xffff)) = value;
}


/* --------------------- Effective Address Calculation -------------------- */

/* The program counter relative addressing modes cause operands to be
 * retrieved from program space, not data space.
 */
u32 m68ki_get_ea_pcdi()
{
  u32 old_pc = REG_PC;
  return old_pc + (s16) m68ki_read_imm_16();
}


u32 m68ki_get_ea_pcix()
{
  return m68ki_get_ea_ix(REG_PC);
}

/* Indexed addressing modes are encoded as follows:
 *
 * Base instruction format:
 * F E D C B A 9 8 7 6 | 5 4 3 | 2 1 0
 * x x x x x x x x x x | 1 1 0 | BASE REGISTER      (An)
 *
 * Base instruction format for destination EA in move instructions:
 * F E D C | B A 9    | 8 7 6 | 5 4 3 2 1 0
 * x x x x | BASE REG | 1 1 0 | X X X X X X       (An)
 *
 * Brief extension format:
 *  F  |  E D C   |  B  |  A 9  | 8 | 7 6 5 4 3 2 1 0
 * D/A | REGISTER | W/L | SCALE | 0 |  DISPLACEMENT
 *
 * Full extension format:
 *  F     E D C      B     A 9    8   7    6    5 4       3   2 1 0
 * D/A | REGISTER | W/L | SCALE | 1 | BS | IS | BD SIZE | 0 | I/IS
 * BASE DISPLACEMENT (0, 16, 32 bit)                (bd)
 * OUTER DISPLACEMENT (0, 16, 32 bit)               (od)
 *
 * D/A:     0 = Dn, 1 = An                          (Xn)
 * W/L:     0 = W (sign extend), 1 = L              (.SIZE)
 * SCALE:   00=1, 01=2, 10=4, 11=8                  (*SCALE)
 * BS:      0=add base reg, 1=suppress base reg     (An suppressed)
 * IS:      0=add index, 1=suppress index           (Xn suppressed)
 * BD SIZE: 00=reserved, 01=NULL, 10=Word, 11=Long  (size of bd)
 *
 * IS I/IS Operation
 * 0  000  No Memory Indirect
 * 0  001  indir prex with null outer
 * 0  010  indir prex with word outer
 * 0  011  indir prex with long outer
 * 0  100  reserved
 * 0  101  indir postx with null outer
 * 0  110  indir postx with word outer
 * 0  111  indir postx with long outer
 * 1  000  no memory indirect
 * 1  001  mem indir with null outer
 * 1  010  mem indir with word outer
 * 1  011  mem indir with long outer
 * 1  100-111  reserved
 */
u32 m68ki_get_ea_ix(u32 An)
{
  /* An = base register */
  u32 extension = m68ki_read_imm_16();

  u32 Xn = 0;                        /* Index register */

  /* Calculate index */
  Xn = REG_DA[extension>>12];     /* Xn */
  if(!BIT_B(extension))           /* W/L */
    Xn = (s16) Xn;

  /* Add base register and displacement and return */
  return An + Xn + (s8) extension;
}


/* Fetch operands */
u32 OPER_AY_AI_8()  {u32 ea = EA_AY_AI_8();  return m68ki_read_8(ea); }
u32 OPER_AY_AI_16() {u32 ea = EA_AY_AI_16(); return m68ki_read_16(ea);}
u32 OPER_AY_AI_32() {u32 ea = EA_AY_AI_32(); return m68ki_read_32(ea);}
u32 OPER_AY_PI_8()  {u32 ea = EA_AY_PI_8();  return m68ki_read_8(ea); }
u32 OPER_AY_PI_16() {u32 ea = EA_AY_PI_16(); return m68ki_read_16(ea);}
u32 OPER_AY_PI_32() {u32 ea = EA_AY_PI_32(); return m68ki_read_32(ea);}
u32 OPER_AY_PD_8()  {u32 ea = EA_AY_PD_8();  return m68ki_read_8(ea); }
u32 OPER_AY_PD_16() {u32 ea = EA_AY_PD_16(); return m68ki_read_16(ea);}
u32 OPER_AY_PD_32() {u32 ea = EA_AY_PD_32(); return m68ki_read_32(ea);}
u32 OPER_AY_DI_8()  {u32 ea = EA_AY_DI_8();  return m68ki_read_8(ea); }
u32 OPER_AY_DI_16() {u32 ea = EA_AY_DI_16(); return m68ki_read_16(ea);}
u32 OPER_AY_DI_32() {u32 ea = EA_AY_DI_32(); return m68ki_read_32(ea);}
u32 OPER_AY_IX_8()  {u32 ea = EA_AY_IX_8();  return m68ki_read_8(ea); }
u32 OPER_AY_IX_16() {u32 ea = EA_AY_IX_16(); return m68ki_read_16(ea);}
u32 OPER_AY_IX_32() {u32 ea = EA_AY_IX_32(); return m68ki_read_32(ea);}

u32 OPER_AX_AI_8()  {u32 ea = EA_AX_AI_8();  return m68ki_read_8(ea); }
u32 OPER_AX_AI_16() {u32 ea = EA_AX_AI_16(); return m68ki_read_16(ea);}
u32 OPER_AX_AI_32() {u32 ea = EA_AX_AI_32(); return m68ki_read_32(ea);}
u32 OPER_AX_PI_8()  {u32 ea = EA_AX_PI_8();  return m68ki_read_8(ea); }
u32 OPER_AX_PI_16() {u32 ea = EA_AX_PI_16(); return m68ki_read_16(ea);}
u32 OPER_AX_PI_32() {u32 ea = EA_AX_PI_32(); return m68ki_read_32(ea);}
u32 OPER_AX_PD_8()  {u32 ea = EA_AX_PD_8();  return m68ki_read_8(ea); }
u32 OPER_AX_PD_16() {u32 ea = EA_AX_PD_16(); return m68ki_read_16(ea);}
u32 OPER_AX_PD_32() {u32 ea = EA_AX_PD_32(); return m68ki_read_32(ea);}
u32 OPER_AX_DI_8()  {u32 ea = EA_AX_DI_8();  return m68ki_read_8(ea); }
u32 OPER_AX_DI_16() {u32 ea = EA_AX_DI_16(); return m68ki_read_16(ea);}
u32 OPER_AX_DI_32() {u32 ea = EA_AX_DI_32(); return m68ki_read_32(ea);}
u32 OPER_AX_IX_8()  {u32 ea = EA_AX_IX_8();  return m68ki_read_8(ea); }
u32 OPER_AX_IX_16() {u32 ea = EA_AX_IX_16(); return m68ki_read_16(ea);}
u32 OPER_AX_IX_32() {u32 ea = EA_AX_IX_32(); return m68ki_read_32(ea);}

u32 OPER_A7_PI_8()  {u32 ea = EA_A7_PI_8();  return m68ki_read_8(ea); }
u32 OPER_A7_PD_8()  {u32 ea = EA_A7_PD_8();  return m68ki_read_8(ea); }

u32 OPER_AW_8()     {u32 ea = EA_AW_8();     return m68ki_read_8(ea); }
u32 OPER_AW_16()    {u32 ea = EA_AW_16();    return m68ki_read_16(ea);}
u32 OPER_AW_32()    {u32 ea = EA_AW_32();    return m68ki_read_32(ea);}
u32 OPER_AL_8()     {u32 ea = EA_AL_8();     return m68ki_read_8(ea); }
u32 OPER_AL_16()    {u32 ea = EA_AL_16();    return m68ki_read_16(ea);}
u32 OPER_AL_32()    {u32 ea = EA_AL_32();    return m68ki_read_32(ea);}
u32 OPER_PCDI_8()   {u32 ea = EA_PCDI_8();   return m68ki_read_pcrel_8(ea); }
u32 OPER_PCDI_16()  {u32 ea = EA_PCDI_16();  return m68ki_read_pcrel_16(ea);}
u32 OPER_PCDI_32()  {u32 ea = EA_PCDI_32();  return m68ki_read_pcrel_32(ea);}
u32 OPER_PCIX_8()   {u32 ea = EA_PCIX_8();   return m68ki_read_pcrel_8(ea); }
u32 OPER_PCIX_16()  {u32 ea = EA_PCIX_16();  return m68ki_read_pcrel_16(ea);}
u32 OPER_PCIX_32()  {u32 ea = EA_PCIX_32();  return m68ki_read_pcrel_32(ea);}



/* ---------------------------- Stack Functions --------------------------- */

/* Push/pull data from the stack */
/* Optimized access assuming stack is always located in ROM/RAM [EkeEke] */  
void m68ki_push_16(u32 value)
{
  REG_SP = MASK_OUT_ABOVE_32(REG_SP - 2);
  /*m68ki_write_16(REG_SP, value);*/
  *(u16 *)(m68ki_cpu.memory_map[(REG_SP>>16)&0xff].base + (REG_SP & 0xffff)) = value;
}

void m68ki_push_32(u32 value)
{
  REG_SP = MASK_OUT_ABOVE_32(REG_SP - 4);
  /*m68ki_write_32(REG_SP, value);*/
  *(u16 *)(m68ki_cpu.memory_map[(REG_SP>>16)&0xff].base + (REG_SP & 0xffff)) = value >> 16;
  *(u16 *)(m68ki_cpu.memory_map[((REG_SP + 2)>>16)&0xff].base + ((REG_SP + 2) & 0xffff)) = value & 0xffff;
}

u32 m68ki_pull_16()
{
  u32 sp = REG_SP;
  REG_SP = MASK_OUT_ABOVE_32(REG_SP + 2);
  return m68k_read_immediate_16(sp);
  /*return m68ki_read_16(sp);*/
}

u32 m68ki_pull_32()
{
  u32 sp = REG_SP;
  REG_SP = MASK_OUT_ABOVE_32(REG_SP + 4);
  return m68k_read_immediate_32(sp);
  /*return m68ki_read_32(sp);*/
}



/* ----------------------------- Program Flow ----------------------------- */

/* Jump to a new program location or vector.
 * These functions will also call the pc_changed callback if it was enabled
 * in m68kconf.h.
 */
void m68ki_jump(u32 new_pc)
{
  REG_PC = new_pc;
}

void m68ki_jump_vector(u32 vector)
{
  REG_PC = m68ki_read_data_32(vector<<2);
}


/* Branch to a new memory location.
 * The 32-bit branch will call pc_changed if it was enabled in m68kconf.h.
 * So far I've found no problems with not calling pc_changed for 8 or 16
 * bit branches.
 */
void m68ki_branch_8(u32 offset)
{
  REG_PC += (s8) offset;
}

void m68ki_branch_16(u32 offset)
{
  REG_PC += (s16) offset;
}

void m68ki_branch_32(u32 offset)
{
  REG_PC += offset;
}



/* ---------------------------- Status Register --------------------------- */

/* Set the S flag and change the active stack pointer.
 * Note that value MUST be 4 or 0.
 */
void m68ki_set_s_flag(u32 value)
{
  /* Backup the old stack pointer */
  REG_SP_BASE[FLAG_S] = REG_SP;
  /* Set the S flag */
  FLAG_S = value;
  /* Set the new stack pointer */
  REG_SP = REG_SP_BASE[FLAG_S];
}


/* Set the condition code register */
void m68ki_set_ccr(u32 value)
{
  FLAG_X = BIT_4(value)  << 4;
  FLAG_N = BIT_3(value)  << 4;
  FLAG_Z = !BIT_2(value);
  FLAG_V = BIT_1(value)  << 6;
  FLAG_C = BIT_0(value)  << 8;
}


/* Set the status register and check for interrupts */
void m68ki_set_sr(u32 value)
{
  /* Set the status register */
  FLAG_T1 = BIT_F(value);
  FLAG_INT_MASK = value & 0x0700;
  m68ki_set_ccr(value);
  m68ki_set_s_flag((value >> 11) & 4);

  /* Check current IRQ status */
  m68ki_check_interrupts();
}


/* ------------------------- Exception Processing ------------------------- */

/* Initiate exception processing */
u32 m68ki_init_exception()
{
  /* Save the old status register */
  u32 sr = m68ki_get_sr();

  /* Turn off trace flag, clear pending traces */
  FLAG_T1 = 0;

  /* Enter supervisor mode */
  m68ki_set_s_flag(SFLAG_SET);

  return sr;
}

/* 3 word stack frame (68000 only) */
void m68ki_stack_frame_3word(u32 pc, u32 sr)
{
  m68ki_push_32(pc);
  m68ki_push_16(sr);
}

static if(M68K_EMULATE_ADDRESS_ERROR) {
/* Bus error stack frame (68000 only).
 */
void m68ki_stack_frame_buserr(u32 sr)
{
  m68ki_push_32(REG_PC);
  m68ki_push_16(sr);
  m68ki_push_16(REG_IR);
  m68ki_push_32(m68ki_cpu.aerr_address);  /* access address */
  /* 0 0 0 0 0 0 0 0 0 0 0 R/W I/N FC
     * R/W  0 = write, 1 = read
     * I/N  0 = instruction, 1 = not
     * FC   3-bit function code
     */
  m68ki_push_16(m68ki_cpu.aerr_write_mode | CPU_INSTR_MODE | m68ki_cpu.aerr_fc);
}
}

/* Used for Group 2 exceptions.
 */
void m68ki_exception_trap(u32 vector)
{
  u32 sr = m68ki_init_exception();

  m68ki_stack_frame_3word(REG_PC, sr);

  m68ki_jump_vector(vector);

  /* Use up some clock cycles */
  USE_CYCLES(CYC_EXCEPTION[vector]);
}

/* Trap#n stacks a 0 frame but behaves like group2 otherwise */
void m68ki_exception_trapN(u32 vector)
{
  u32 sr = m68ki_init_exception();
  m68ki_stack_frame_3word(REG_PC, sr);
  m68ki_jump_vector(vector);

  /* Use up some clock cycles */
  USE_CYCLES(CYC_EXCEPTION[vector]);
}

/* Exception for privilege violation */
void m68ki_exception_privilege_violation()
{
  u32 sr = m68ki_init_exception();

  CPU_INSTR_MODE = INSTRUCTION_NO;

  m68ki_stack_frame_3word(REG_PC-2, sr);
  m68ki_jump_vector(EXCEPTION_PRIVILEGE_VIOLATION);

  /* Use up some clock cycles and undo the instruction's cycles */
  USE_CYCLES(CYC_EXCEPTION[EXCEPTION_PRIVILEGE_VIOLATION] - CYC_INSTRUCTION[REG_IR]);
}

/* Exception for A-Line instructions */
void m68ki_exception_1010()
{
  u32 sr = m68ki_init_exception();
  m68ki_stack_frame_3word(REG_PC-2, sr);
  m68ki_jump_vector(EXCEPTION_1010);

  /* Use up some clock cycles and undo the instruction's cycles */
  USE_CYCLES(CYC_EXCEPTION[EXCEPTION_1010] - CYC_INSTRUCTION[REG_IR]);
}

/* Exception for F-Line instructions */
void m68ki_exception_1111()
{
  u32 sr = m68ki_init_exception();
  m68ki_stack_frame_3word(REG_PC-2, sr);
  m68ki_jump_vector(EXCEPTION_1111);

  /* Use up some clock cycles and undo the instruction's cycles */
  USE_CYCLES(CYC_EXCEPTION[EXCEPTION_1111] - CYC_INSTRUCTION[REG_IR]);
}

/* Exception for illegal instructions */
void m68ki_exception_illegal()
{
  u32 sr = m68ki_init_exception();

  CPU_INSTR_MODE = INSTRUCTION_NO;

  m68ki_stack_frame_3word(REG_PC-2, sr);
  m68ki_jump_vector(EXCEPTION_ILLEGAL_INSTRUCTION);

  /* Use up some clock cycles and undo the instruction's cycles */
  USE_CYCLES(CYC_EXCEPTION[EXCEPTION_ILLEGAL_INSTRUCTION] - CYC_INSTRUCTION[REG_IR]);
}


static if(M68K_EMULATE_ADDRESS_ERROR) {
/* Exception for address error */
void m68ki_exception_address_error()
{
  u32 sr = m68ki_init_exception();

  /* If we were processing a bus error, address error, or reset,
     * this is a catastrophic failure.
     * Halt the CPU
     */
  if(CPU_RUN_MODE == RUN_MODE_BERR_AERR_RESET)
  {
    CPU_STOPPED = STOP_LEVEL_HALT;
    SET_CYCLES(m68ki_cpu.cycle_end - CYC_INSTRUCTION[REG_IR]);
    return;
  }
  CPU_RUN_MODE = RUN_MODE_BERR_AERR_RESET;

  /* Note: This is implemented for 68000 only! */
  m68ki_stack_frame_buserr(sr);

  m68ki_jump_vector(EXCEPTION_ADDRESS_ERROR);

  /* Use up some clock cycles and undo the instruction's cycles */
  USE_CYCLES(CYC_EXCEPTION[EXCEPTION_ADDRESS_ERROR] - CYC_INSTRUCTION[REG_IR]);
}
}

/* Service an interrupt request and start exception processing */
void m68ki_exception_interrupt(u32 int_level)
{
  u32 vector, sr, new_pc;

  CPU_INSTR_MODE = INSTRUCTION_NO;

  /* Turn off the stopped state */
  CPU_STOPPED &= STOP_LEVEL_HALT;

  /* If we are halted, don't do anything */
  if(CPU_STOPPED)
    return;

  /* Always use the autovectors. */
  vector = EXCEPTION_INTERRUPT_AUTOVECTOR+int_level;

  /* Start exception processing */
  sr = m68ki_init_exception();

  /* Set the interrupt mask to the level of the one being serviced */
  FLAG_INT_MASK = int_level<<8;

  /* Acknowledge the interrupt */
  m68ki_int_ack(int_level);

  /* Get the new PC */
  new_pc = m68ki_read_data_32(vector<<2);

  /* If vector is uninitialized, call the uninitialized interrupt vector */
  if(new_pc == 0)
    new_pc = m68ki_read_data_32((EXCEPTION_UNINITIALIZED_INTERRUPT<<2));

  /* Generate a stack frame */
  m68ki_stack_frame_3word(REG_PC, sr);

  m68ki_jump(new_pc);

  /* Update cycle count now */
  USE_CYCLES(CYC_EXCEPTION[vector]);
}

/* ASG: Check for interrupts */
void m68ki_check_interrupts()
{
  if(CPU_INT_LEVEL > FLAG_INT_MASK)
    m68ki_exception_interrupt(CPU_INT_LEVEL>>8);
}


/* ======================================================================== */
/* ============================== END OF FILE ============================= */
/* ======================================================================== */
