#ifndef M68KCPU__HEADER
#define M68KCPU__HEADER

/* ======================================================================== */
/*                         GENERIC 68K CORE                                 */
/* ======================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#if M68K_EMULATE_ADDRESS_ERROR
#include <setjmp.h>
#endif /* M68K_EMULATE_ADDRESS_ERROR */

#include "m68k.h"


// Prototypes of mass destruction
/* Read data immediately after the program counter */
u32 m68ki_read_imm_16();
u32 m68ki_read_imm_32();

/* Read data with specific function code */
u32 m68ki_read_8_fc  (u32 address);
u32 m68ki_read_16_fc (u32 address, u32 fc);
u32 m68ki_read_32_fc (u32 address, u32 fc);

/* Write data with specific function code */
void m68ki_write_8_fc (u32 address, u32 value);
void m68ki_write_16_fc(u32 address, u32 fc, u32 value);
void m68ki_write_32_fc(u32 address, u32 fc, u32 value);

/* Indexed and PC-relative ea fetching */
u32 m68ki_get_ea_pcdi();
u32 m68ki_get_ea_pcix();
u32 m68ki_get_ea_ix(u32 An);

/* Operand fetching */
u32 OPER_AY_AI_8();
u32 OPER_AY_AI_16();
u32 OPER_AY_AI_32();
u32 OPER_AY_PI_8();
u32 OPER_AY_PI_16();
u32 OPER_AY_PI_32();
u32 OPER_AY_PD_8();
u32 OPER_AY_PD_16();
u32 OPER_AY_PD_32();
u32 OPER_AY_DI_8();
u32 OPER_AY_DI_16();
u32 OPER_AY_DI_32();
u32 OPER_AY_IX_8();
u32 OPER_AY_IX_16();
u32 OPER_AY_IX_32();

u32 OPER_AX_AI_8();
u32 OPER_AX_AI_16();
u32 OPER_AX_AI_32();
u32 OPER_AX_PI_8();
u32 OPER_AX_PI_16();
u32 OPER_AX_PI_32();
u32 OPER_AX_PD_8();
u32 OPER_AX_PD_16();
u32 OPER_AX_PD_32();
u32 OPER_AX_DI_8();
u32 OPER_AX_DI_16();
u32 OPER_AX_DI_32();
u32 OPER_AX_IX_8();
u32 OPER_AX_IX_16();
u32 OPER_AX_IX_32();

u32 OPER_A7_PI_8();
u32 OPER_A7_PD_8();

u32 OPER_AW_8();
u32 OPER_AW_16();
u32 OPER_AW_32();
u32 OPER_AL_8();
u32 OPER_AL_16();
u32 OPER_AL_32();
u32 OPER_PCDI_8();
u32 OPER_PCDI_16();
u32 OPER_PCDI_32();
u32 OPER_PCIX_8();
u32 OPER_PCIX_16();
u32 OPER_PCIX_32();

/* Stack operations */
void m68ki_push_16(u32 value);
void m68ki_push_32(u32 value);
u32 m68ki_pull_16();
u32 m68ki_pull_32();

/* Program flow operations */
void m68ki_jump(u32 new_pc);
void m68ki_jump_vector(u32 vector);
void m68ki_branch_8(u32 offset);
void m68ki_branch_16(u32 offset);
void m68ki_branch_32(u32 offset);

/* Status register operations. */
void m68ki_set_s_flag(u32 value);            /* Only bit 2 of value should be set (i.e. 4 or 0) */
void m68ki_set_ccr(u32 value);               /* set the condition code register */
void m68ki_set_sr(u32 value);                /* set the status register */

/* Exception processing */
u32 m68ki_init_exception();              /* Initial exception processing */
void m68ki_stack_frame_3word(u32 pc, u32 sr); /* Stack various frame types */
#if M68K_EMULATE_ADDRESS_ERROR
void m68ki_stack_frame_buserr(u32 sr);
#endif
void m68ki_exception_trap(u32 vector);
void m68ki_exception_trapN(u32 vector);
void m68ki_exception_privilege_violation(); /* do not inline in order to reduce function size and allow inlining of read/write functions by the compile */
void m68ki_exception_1010();
void m68ki_exception_1111();
void m68ki_exception_illegal();
#if M68K_EMULATE_ADDRESS_ERROR
void m68ki_exception_address_error();
#endif
void m68ki_exception_interrupt(u32 int_level);
void m68ki_check_interrupts();            /* ASG: check for interrupts */

/* ======================================================================== */
/* ============================ GENERAL DEFINES =========================== */
/* ======================================================================== */

/* Exception Vectors handled by emulation */
#define EXCEPTION_RESET                    0
#define EXCEPTION_BUS_ERROR                2 /* This one is not emulated! */
#define EXCEPTION_ADDRESS_ERROR            3 /* This one is partially emulated (doesn't stack a proper frame yet) */
#define EXCEPTION_ILLEGAL_INSTRUCTION      4
#define EXCEPTION_ZERO_DIVIDE              5
#define EXCEPTION_CHK                      6
#define EXCEPTION_TRAPV                    7
#define EXCEPTION_PRIVILEGE_VIOLATION      8
#define EXCEPTION_TRACE                    9
#define EXCEPTION_1010                    10
#define EXCEPTION_1111                    11
#define EXCEPTION_FORMAT_ERROR            14
#define EXCEPTION_UNINITIALIZED_INTERRUPT 15
#define EXCEPTION_SPURIOUS_INTERRUPT      24
#define EXCEPTION_INTERRUPT_AUTOVECTOR    24
#define EXCEPTION_TRAP_BASE               32

/* Function codes set by CPU during data/address bus activity */
#define FUNCTION_CODE_USER_DATA          1
#define FUNCTION_CODE_USER_PROGRAM       2
#define FUNCTION_CODE_SUPERVISOR_DATA    5
#define FUNCTION_CODE_SUPERVISOR_PROGRAM 6
#define FUNCTION_CODE_CPU_SPACE          7

/* Different ways to stop the CPU */
#define STOP_LEVEL_STOP 1
#define STOP_LEVEL_HALT 2

/* Used for 68000 address error processing */
#if M68K_EMULATE_ADDRESS_ERROR
#define INSTRUCTION_YES 0
#define INSTRUCTION_NO  0x08
#define MODE_READ       0x10
#define MODE_WRITE      0

#define RUN_MODE_NORMAL          0
#define RUN_MODE_BERR_AERR_RESET 1
#endif


/* ------------------------------ CPU Access ------------------------------ */

/* Access the CPU registers */
#define REG_DA           m68ki_cpu.dar /* easy access to data and address regs */
#define REG_D            m68ki_cpu.dar
#define REG_A            (m68ki_cpu.dar+8)
#define REG_PC           m68ki_cpu.pc
#define REG_SP_BASE      m68ki_cpu.sp
#define REG_USP          m68ki_cpu.sp[0]
#define REG_ISP          m68ki_cpu.sp[4]
#define REG_SP           m68ki_cpu.dar[15]
#define REG_IR           m68ki_cpu.ir

#define FLAG_T1          m68ki_cpu.t1_flag
#define FLAG_S           m68ki_cpu.s_flag
#define FLAG_X           m68ki_cpu.x_flag
#define FLAG_N           m68ki_cpu.n_flag
#define FLAG_Z           m68ki_cpu.not_z_flag
#define FLAG_V           m68ki_cpu.v_flag
#define FLAG_C           m68ki_cpu.c_flag
#define FLAG_INT_MASK    m68ki_cpu.int_mask

#define CPU_INT_LEVEL    m68ki_cpu.int_level /* ASG: changed from CPU_INTS_PENDING */
#define CPU_STOPPED      m68ki_cpu.stopped
#define CPU_ADDRESS_MASK  0x00ffffff
#if M68K_EMULATE_ADDRESS_ERROR
#define CPU_INSTR_MODE   m68ki_cpu.instr_mode
#define CPU_RUN_MODE     m68ki_cpu.run_mode
#endif

#define CYC_INSTRUCTION   m68ki_cycles
#define CYC_EXCEPTION     m68ki_exception_cycle_table
#define CYC_BCC_NOTAKE_B  ( -2 * MUL)
#define CYC_BCC_NOTAKE_W  (  2 * MUL)
#define CYC_DBCC_F_NOEXP  ( -2 * MUL)
#define CYC_DBCC_F_EXP    (  2 * MUL)
#define CYC_SCC_R_TRUE    (  2 * MUL)
#define CYC_MOVEM_W       (  4 * MUL)
#define CYC_MOVEM_L       (  8 * MUL)
#define CYC_SHIFT         (  2 * MUL)
#define CYC_RESET         (132 * MUL)


/* ======================================================================== */
/* ================================ MACROS ================================ */
/* ======================================================================== */


/* ---------------------------- General Macros ---------------------------- */

/* Bit Isolation Macros */
u32 BIT_0(u32 A);
u32 BIT_1(u32 A);
u32 BIT_2(u32 A);
u32 BIT_3(u32 A);
u32 BIT_4(u32 A);
u32 BIT_5(u32 A);
u32 BIT_6(u32 A);
u32 BIT_7(u32 A);
u32 BIT_8(u32 A);
u32 BIT_9(u32 A);
u32 BIT_A(u32 A);
u32 BIT_B(u32 A);
u32 BIT_C(u32 A);
u32 BIT_D(u32 A);
u32 BIT_E(u32 A);
u32 BIT_F(u32 A);
u32 BIT_10(u32 A);
u32 BIT_11(u32 A);
u32 BIT_12(u32 A);
u32 BIT_13(u32 A);
u32 BIT_14(u32 A);
u32 BIT_15(u32 A);
u32 BIT_16(u32 A);
u32 BIT_17(u32 A);
u32 BIT_18(u32 A);
u32 BIT_19(u32 A);
u32 BIT_1A(u32 A);
u32 BIT_1B(u32 A);
u32 BIT_1C(u32 A);
u32 BIT_1D(u32 A);
u32 BIT_1E(u32 A);
u32 BIT_1F(u32 A);

/* Get the most significant bit for specific sizes */
u32 GET_MSB_8(u32 A);
u32 GET_MSB_9(u32 A);
u32 GET_MSB_16(u32 A);
u32 GET_MSB_17(u32 A);
u32 GET_MSB_32(u32 A);
//u32 GET_MSB_33(u32 A);

/* Isolate nibbles */
u32 LOW_NIBBLE(u32 A);
u32 HIGH_NIBBLE(u32 A);

/* These are used to isolate 8, 16, and 32 bit sizes */
u32 MASK_OUT_ABOVE_2(u32 A);
u32 MASK_OUT_ABOVE_8(u32 A);
u32 MASK_OUT_ABOVE_16(u32 A);
u32 MASK_OUT_BELOW_2(u32 A);
u32 MASK_OUT_BELOW_8(u32 A);
u32 MASK_OUT_BELOW_16(u32 A);

/* No need to mask if we are 32 bit */
#if M68K_INT_GT_32_BIT
  static u32 MASK_OUT_ABOVE_32(u32 A) { return A & 0xffffffff; }
  static u32 MASK_OUT_BELOW_32(u32 A) { return A & ~0xffffffff; }
#else
  static u32 MASK_OUT_ABOVE_32(u32 A) { return A; }
  static u32 MASK_OUT_BELOW_32(u32 A) { return 0; }
#endif /* M68K_INT_GT_32_BIT */

/* Simulate address lines of 68k family */
static u32 ADDRESS_68K(u32 A) { return A & CPU_ADDRESS_MASK; }


/* Shift & Rotate Macros. */
u32 LSL(u32 A, u32 C);
u32 LSR(u32 A, u32 C);

/* Some > 32-bit optimizations */
#if M68K_INT_GT_32_BIT
  /* Shift left and right */
  static u32 LSR_32(u32 A, u32 C) { return A >> C; }
  static u32 LSL_32(u32 A, u32 C) { return A << C; }
#else
  /* We have to do this because the morons at ANSI decided that shifts
     * by >= data size are undefined.
     */
  static u32 LSR_32(u32 A, u32 C) { return C < 32 ? A >> C : 0; }
  static u32 LSL_32(u32 A, u32 C) { return C < 32 ? A << C : 0; }
#endif /* M68K_INT_GT_32_BIT */

u32 ROL_8(u32 A, u32 C);
u32 ROL_9(u32 A, u32 C);
u32 ROL_16(u32 A, u32 C);
u32 ROL_17(u32 A, u32 C);
u32 ROL_32(u32 A, u32 C);
u32 ROL_33(u32 A, u32 C);

u32 ROR_8(u32 A, u32 C);
u32 ROR_9(u32 A, u32 C);
u32 ROR_16(u32 A, u32 C);
u32 ROR_17(u32 A, u32 C);
u32 ROR_32(u32 A, u32 C);
u32 ROR_33(u32 A, u32 C);

static u32 m68ki_read_imm_8();


/* ----------------------------- Configuration ---------------------------- */

/* These defines are dependant on the configuration defines in m68kconf.h */

/* Enable or disable callback functions */
#define m68ki_int_ack(A) M68K_INT_ACK_CALLBACK(A);

#define m68ki_output_reset()

#if M68K_TAS_HAS_CALLBACK
  #if M68K_TAS_HAS_CALLBACK == OPT_SPECIFY_HANDLER
    #define m68ki_tas_callback() M68K_TAS_CALLBACK()
  #else
    #define m68ki_tas_callback() CALLBACK_TAS_INSTR()
  #endif
#else
  #define m68ki_tas_callback() 0
#endif /* M68K_TAS_HAS_CALLBACK */


/* Enable or disable function code emulation */
#define m68ki_get_address_space() FUNCTION_CODE_USER_DATA


/* Enable or disable Address error emulation */
#if M68K_EMULATE_ADDRESS_ERROR
  #define m68ki_set_address_error_trap() \
    if(setjmp(m68ki_cpu.aerr_trap) != 0) \
    { \
      m68ki_exception_address_error(); \
    }

  #define m68ki_check_address_error(ADDR, WRITE_MODE, FC) \
    if((ADDR)&1) \
    { \
      if (m68ki_cpu.aerr_enabled) \
      { \
        m68ki_cpu.aerr_address = ADDR; \
        m68ki_cpu.aerr_write_mode = WRITE_MODE; \
        m68ki_cpu.aerr_fc = FC; \
        longjmp(m68ki_cpu.aerr_trap, 1); \
      } \
    }
#else
  #define m68ki_set_address_error_trap()
  #define m68ki_check_address_error(ADDR, WRITE_MODE, FC)
#endif /* M68K_ADDRESS_ERROR */


/* -------------------------- EA / Operand Access ------------------------- */

/*
 * The general instruction format follows this pattern:
 * .... XXX. .... .YYY
 * where XXX is register X and YYY is register Y
 */

/* Data Register Isolation */
#define DX (REG_D[(REG_IR >> 9) & 7])
#define DY (REG_D[REG_IR & 7])

/* Address Register Isolation */
#define AX (REG_A[(REG_IR >> 9) & 7])
#define AY (REG_A[REG_IR & 7])

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

#if M68K_INT_GT_32_BIT
  static u32 CFLAG_ADD_32(u32 S, u32 D, u32 R) { return R >> 24; }
  static u32 CFLAG_SUB_32(u32 S, u32 D, u32 R) { return R >> 24; }
#else
  static u32 CFLAG_ADD_32(u32 S, u32 D, u32 R) { return (((S & D) | (~R & (S | D))) >> 23); }
  static u32 CFLAG_SUB_32(u32 S, u32 D, u32 R) { return (((S & R) | (~D & (S | R)))>>23); }
#endif /* M68K_INT_GT_32_BIT */

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
#define NFLAG_SET   0x80
#define NFLAG_CLEAR 0
#define CFLAG_SET   0x100
#define CFLAG_CLEAR 0
#define XFLAG_SET   0x100
#define XFLAG_CLEAR 0
#define VFLAG_SET   0x80
#define VFLAG_CLEAR 0
#define ZFLAG_SET   0
#define ZFLAG_CLEAR 0xffffffff
#define SFLAG_SET   4
#define SFLAG_CLEAR 0

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
static const u8 m68ki_shift_8_table[65] =
{
  0x00, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff
};

static const u16 m68ki_shift_16_table[65] =
{
  0x0000, 0x8000, 0xc000, 0xe000, 0xf000, 0xf800, 0xfc00, 0xfe00, 0xff00,
  0xff80, 0xffc0, 0xffe0, 0xfff0, 0xfff8, 0xfffc, 0xfffe, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
  0xffff, 0xffff
};

static const u32 m68ki_shift_32_table[65] =
{
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
};


/* Number of clock cycles to use for exception processing.
 * I used 4 for any vectors that are undocumented for processing times.
 */
static const u16 m68ki_exception_cycle_table[256] =
{
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
};


/* ======================================================================== */
/* ============================== END OF FILE ============================= */
/* ======================================================================== */

#endif /* M68KCPU__HEADER */
