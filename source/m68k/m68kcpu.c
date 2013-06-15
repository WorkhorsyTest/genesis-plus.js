/* ======================================================================== */
/*                            MAIN 68K CORE                                 */
/* ======================================================================== */

#include "types.h"

extern s32 vdp_68k_irq_ack(s32 int_level);

#define m68ki_cpu m68k
#define MUL (7)

/* ======================================================================== */
/* ================================ INCLUDES ============================== */
/* ======================================================================== */

#ifndef BUILD_TABLES
#include "m68ki_cycles.h"
#endif

#include "m68kconf.h"
#include "m68kcpu.h"
#include "m68kops.h"


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

/* ======================================================================== */
/* ================================= DATA ================================= */
/* ======================================================================== */

#ifdef BUILD_TABLES
static u8 m68ki_cycles[0x10000];
#endif

static s32 irq_latency;

m68ki_cpu_core m68k;


/* ======================================================================== */
/* =============================== CALLBACKS ============================== */
/* ======================================================================== */

/* Default callbacks used if the callback hasn't been set yet, or if the
 * callback is set to NULL
 */

#if M68K_EMULATE_INT_ACK == OPT_ON
/* Interrupt acknowledge */
static s32 default_int_ack_callback(s32 int_level)
{
  CPU_INT_LEVEL = 0;
  return M68K_INT_ACK_AUTOVECTOR;
}
#endif

#if M68K_EMULATE_RESET == OPT_ON
/* Called when a reset instruction is executed */
static void default_reset_instr_callback()
{
}
#endif

#if M68K_TAS_HAS_CALLBACK == OPT_ON
/* Called when a tas instruction is executed */
static s32 default_tas_instr_callback()
{
  return 1; // allow writeback
}
#endif

#if M68K_EMULATE_FC == OPT_ON
/* Called every time there's bus activity (read/write to/from memory */
static void default_set_fc_callback(u32 new_fc)
{
}
#endif


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
#if M68K_EMULATE_PREFETCH
    case M68K_REG_PREF_ADDR:  return m68ki_cpu.pref_addr;
    case M68K_REG_PREF_DATA:  return m68ki_cpu.pref_data;
#endif
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
#if M68K_EMULATE_PREFETCH
    case M68K_REG_PREF_ADDR:  CPU_PREF_ADDR = MASK_OUT_ABOVE_32(value); return;
#endif
    default:      return;
  }
}

/* Set the callbacks */
#if M68K_EMULATE_INT_ACK == OPT_ON
void m68k_set_int_ack_callback(s32  (*callback)(s32 int_level))
{
  CALLBACK_INT_ACK = callback ? callback : default_int_ack_callback;
}
#endif

#if M68K_EMULATE_RESET == OPT_ON
void m68k_set_reset_instr_callback(void  (*callback)())
{
  CALLBACK_RESET_INSTR = callback ? callback : default_reset_instr_callback;
}
#endif

#if M68K_TAS_HAS_CALLBACK == OPT_ON
void m68k_set_tas_instr_callback(s32  (*callback)())
{
  CALLBACK_TAS_INSTR = callback ? callback : default_tas_instr_callback;
}
#endif

#if M68K_EMULATE_FC == OPT_ON
void m68k_set_fc_callback(void  (*callback)(u32 new_fc))
{
  CALLBACK_SET_FC = callback ? callback : default_set_fc_callback;
}
#endif

#ifdef LOGVDP
extern void error(char *format, ...);
extern u16 v_counter;
#endif

/* ASG: rewrote so that the int_level is a mask of the IPL0/IPL1/IPL2 bits */
/* KS: Modified so that IPL* bits match with mask positions in the SR
 *     and cleaned out remenants of the interrupt controller.
 */
void m68k_update_irq(u32 mask)
{
  /* Update IRQ level */
  CPU_INT_LEVEL |= (mask << 8);
  
#ifdef LOGVDP
  error("[%d(%d)][%d(%d)] IRQ Level = %d(0x%02x) (%x)\n", v_counter, m68k.cycles/3420, m68k.cycles, m68k.cycles%3420,CPU_INT_LEVEL>>8,FLAG_INT_MASK,m68k_get_reg(M68K_REG_PC));
#endif
}

void m68k_set_irq(u32 int_level)
{
  /* Set IRQ level */
  CPU_INT_LEVEL = int_level << 8;
  
#ifdef LOGVDP
  error("[%d(%d)][%d(%d)] IRQ Level = %d(0x%02x) (%x)\n", v_counter, m68k.cycles/3420, m68k.cycles, m68k.cycles%3420,CPU_INT_LEVEL>>8,FLAG_INT_MASK,m68k_get_reg(M68K_REG_PC));
#endif
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
      m68ki_trace_t1() /* auto-disable (see m68kcpu.h) */
      m68ki_use_data_space() /* auto-disable (see m68kcpu.h) */
      REG_IR = m68ki_read_imm_16();
      m68ki_instruction_jump_table[REG_IR]();
      m68ki_exception_if_trace() /* auto-disable (see m68kcpu.h) */
      irq_latency = 0;
    }

    /* Set IRQ level */
    CPU_INT_LEVEL = int_level << 8;
  }
  
#ifdef LOGVDP
  error("[%d(%d)][%d(%d)] IRQ Level = %d(0x%02x) (%x)\n", v_counter, m68k.cycles/3420, m68k.cycles, m68k.cycles%3420,CPU_INT_LEVEL>>8,FLAG_INT_MASK,m68k_get_reg(M68K_REG_PC));
#endif

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
  m68ki_set_address_error_trap() /* auto-disable (see m68kcpu.h) */

#ifdef LOGVDP
  error("[%d][%d] m68k run to %d cycles (%x)\n", v_counter, m68k.cycles, cycles, m68k.pc);
#endif
   
  while (m68k.cycles < cycles)
  {
    /* Set tracing accodring to T1. */
    m68ki_trace_t1() /* auto-disable (see m68kcpu.h) */

    /* Set the address space for reads */
    m68ki_use_data_space() /* auto-disable (see m68kcpu.h) */

    /* Decode next instruction */
    REG_IR = m68ki_read_imm_16();
	
    /* Execute instruction */
	m68ki_instruction_jump_table[REG_IR]();
    USE_CYCLES(CYC_INSTRUCTION[REG_IR]);

    /* Trace m68k_exception, if necessary */
    m68ki_exception_if_trace(); /* auto-disable (see m68kcpu.h) */
  }
}

void m68k_init()
{
#ifdef BUILD_TABLES
  static uint emulation_initialized = 0;

  /* The first call to this function initializes the opcode handler jump table */
  if(!emulation_initialized)
  {
    m68ki_build_opcode_table();
    emulation_initialized = 1;
  }
#endif

#if M68K_EMULATE_INT_ACK == OPT_ON
  m68k_set_int_ack_callback(NULL);
#endif
#if M68K_EMULATE_RESET == OPT_ON
  m68k_set_reset_instr_callback(NULL);
#endif
#if M68K_TAS_HAS_CALLBACK == OPT_ON
  m68k_set_tas_instr_callback(NULL);
#endif
#if M68K_EMULATE_FC == OPT_ON
  m68k_set_fc_callback(NULL);
#endif
}

/* Pulse the RESET line on the CPU */
void m68k_pulse_reset()
{
  /* Clear all stop levels */
  CPU_STOPPED = 0;
#if M68K_EMULATE_ADDRESS_ERROR
  CPU_RUN_MODE = RUN_MODE_BERR_AERR_RESET;
#endif

  /* Turn off tracing */
  FLAG_T1 = 0;
  m68ki_clear_trace()

  /* Interrupt mask to level 7 */
  FLAG_INT_MASK = 0x0700;
  CPU_INT_LEVEL = 0;
  irq_latency = 0;

  /* Go to supervisor mode */
  m68ki_set_s_flag(SFLAG_SET);

  /* Invalidate the prefetch queue */
#if M68K_EMULATE_PREFETCH
  /* Set to arbitrary number since our first fetch is from 0 */
  CPU_PREF_ADDR = 0x1000;
#endif /* M68K_EMULATE_PREFETCH */

  /* Read the initial stack pointer and program counter */
  m68ki_jump(0);
  REG_SP = m68ki_read_imm_32();
  REG_PC = m68ki_read_imm_32();
  m68ki_jump(REG_PC);

#if M68K_EMULATE_ADDRESS_ERROR
  CPU_RUN_MODE = RUN_MODE_NORMAL;
#endif

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
  m68ki_set_fc(FLAG_S | FUNCTION_CODE_USER_PROGRAM) /* auto-disable (see m68kcpu.h) */
#if M68K_CHECK_PC_ADDRESS_ERROR
  m68ki_check_address_error(REG_PC, MODE_READ, FLAG_S | FUNCTION_CODE_USER_PROGRAM) /* auto-disable (see m68kcpu.h) */
#endif
#if M68K_EMULATE_PREFETCH
  if(MASK_OUT_BELOW_2(REG_PC) != CPU_PREF_ADDR)
  {
    CPU_PREF_ADDR = MASK_OUT_BELOW_2(REG_PC);
    CPU_PREF_DATA = m68k_read_immediate_32(CPU_PREF_ADDR);
  }
  REG_PC += 2;
  return MASK_OUT_ABOVE_16(CPU_PREF_DATA >> ((2-((REG_PC-2)&2))<<3));
#else
  u32 pc = REG_PC;
  REG_PC += 2;
  return m68k_read_immediate_16(pc);
#endif /* M68K_EMULATE_PREFETCH */
}

u32 m68ki_read_imm_32()
{
#if M68K_EMULATE_PREFETCH
  u32 temp_val;

  m68ki_set_fc(FLAG_S | FUNCTION_CODE_USER_PROGRAM) /* auto-disable (see m68kcpu.h) */
#if M68K_CHECK_PC_ADDRESS_ERROR
  m68ki_check_address_error(REG_PC, MODE_READ, FLAG_S | FUNCTION_CODE_USER_PROGRAM) /* auto-disable (see m68kcpu.h) */
#endif
  if(MASK_OUT_BELOW_2(REG_PC) != CPU_PREF_ADDR)
  {
    CPU_PREF_ADDR = MASK_OUT_BELOW_2(REG_PC);
    CPU_PREF_DATA = m68k_read_immediate_32(CPU_PREF_ADDR);
  }
  temp_val = CPU_PREF_DATA;
  REG_PC += 2;
  if(MASK_OUT_BELOW_2(REG_PC) != CPU_PREF_ADDR)
  {
    CPU_PREF_ADDR = MASK_OUT_BELOW_2(REG_PC);
    CPU_PREF_DATA = m68k_read_immediate_32(CPU_PREF_ADDR);
    temp_val = MASK_OUT_ABOVE_32((temp_val << 16) | (CPU_PREF_DATA >> 16));
  }
  REG_PC += 2;

  return temp_val;
#else
  m68ki_set_fc(FLAG_S | FUNCTION_CODE_USER_PROGRAM) /* auto-disable (see m68kcpu.h) */
#if M68K_CHECK_PC_ADDRESS_ERROR
  m68ki_check_address_error(REG_PC, MODE_READ, FLAG_S | FUNCTION_CODE_USER_PROGRAM) /* auto-disable (see m68kcpu.h) */
#endif
  u32 pc = REG_PC;
  REG_PC += 4;
  return m68k_read_immediate_32(pc);
#endif /* M68K_EMULATE_PREFETCH */
}



/* ------------------------- Top level read/write ------------------------- */

/* Handles all memory accesses (except for immediate reads if they are
 * configured to use separate functions in m68kconf.h).
 * All memory accesses must go through these top level functions.
 * These functions will also check for address error and set the function
 * code if they are enabled in m68kconf.h.
 */
u32 m68ki_read_8_fc(u32 address, u32 fc)
{
  cpu_memory_map *temp = &m68ki_cpu.memory_map[((address)>>16)&0xff];;

  m68ki_set_fc(fc) /* auto-disable (see m68kcpu.h) */

  if (temp->read8) return (*temp->read8)(ADDRESS_68K(address));
  else return READ_BYTE(temp->base, (address) & 0xffff);
}

u32 m68ki_read_16_fc(u32 address, u32 fc)
{
  cpu_memory_map *temp;

  m68ki_set_fc(fc) /* auto-disable (see m68kcpu.h) */
  m68ki_check_address_error(address, MODE_READ, fc) /* auto-disable (see m68kcpu.h) */
  
  temp = &m68ki_cpu.memory_map[((address)>>16)&0xff];
  if (temp->read16) return (*temp->read16)(ADDRESS_68K(address));
  else return *(u16 *)(temp->base + ((address) & 0xffff));
}

u32 m68ki_read_32_fc(u32 address, u32 fc)
{
  cpu_memory_map *temp;

  m68ki_set_fc(fc) /* auto-disable (see m68kcpu.h) */
  m68ki_check_address_error(address, MODE_READ, fc) /* auto-disable (see m68kcpu.h) */

  temp = &m68ki_cpu.memory_map[((address)>>16)&0xff];
  if (temp->read16) return ((*temp->read16)(ADDRESS_68K(address)) << 16) | ((*temp->read16)(ADDRESS_68K(address + 2)));
  else return m68k_read_immediate_32(address);
}

void m68ki_write_8_fc(u32 address, u32 fc, u32 value)
{
  cpu_memory_map *temp;

  m68ki_set_fc(fc) /* auto-disable (see m68kcpu.h) */

  temp = &m68ki_cpu.memory_map[((address)>>16)&0xff];
  if (temp->write8) (*temp->write8)(ADDRESS_68K(address),value);
  else WRITE_BYTE(temp->base, (address) & 0xffff, value);
}

void m68ki_write_16_fc(u32 address, u32 fc, u32 value)
{
  cpu_memory_map *temp;

  m68ki_set_fc(fc) /* auto-disable (see m68kcpu.h) */
  m68ki_check_address_error(address, MODE_WRITE, fc); /* auto-disable (see m68kcpu.h) */

  temp = &m68ki_cpu.memory_map[((address)>>16)&0xff];
  if (temp->write16) (*temp->write16)(ADDRESS_68K(address),value);
  else *(u16 *)(temp->base + ((address) & 0xffff)) = value;
}

void m68ki_write_32_fc(u32 address, u32 fc, u32 value)
{
  cpu_memory_map *temp;

  m68ki_set_fc(fc) /* auto-disable (see m68kcpu.h) */
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
  m68ki_use_program_space() /* auto-disable */
  return old_pc + (s16) m68ki_read_imm_16();
}


u32 m68ki_get_ea_pcix()
{
  m68ki_use_program_space() /* auto-disable */
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
  m68ki_clear_trace()

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

#if M68K_EMULATE_ADDRESS_ERROR
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
#endif

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

#if M68K_EMULATE_TRACE
/* Exception for trace mode */
void m68ki_exception_trace()
{
  u32 sr = m68ki_init_exception();

  #if M68K_EMULATE_ADDRESS_ERROR == OPT_ON
  CPU_INSTR_MODE = INSTRUCTION_NO;
  #endif /* M68K_EMULATE_ADDRESS_ERROR */

  m68ki_stack_frame_3word(REG_PC, sr);
  m68ki_jump_vector(EXCEPTION_TRACE);

  /* Trace nullifies a STOP instruction */
  CPU_STOPPED &= ~STOP_LEVEL_STOP;

  /* Use up some clock cycles */
  USE_CYCLES(CYC_EXCEPTION[EXCEPTION_TRACE]);
}
#endif

/* Exception for privilege violation */
void m68ki_exception_privilege_violation()
{
  u32 sr = m68ki_init_exception();

  #if M68K_EMULATE_ADDRESS_ERROR == OPT_ON
  CPU_INSTR_MODE = INSTRUCTION_NO;
  #endif /* M68K_EMULATE_ADDRESS_ERROR */

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

  #if M68K_EMULATE_ADDRESS_ERROR == OPT_ON
  CPU_INSTR_MODE = INSTRUCTION_NO;
  #endif /* M68K_EMULATE_ADDRESS_ERROR */

  m68ki_stack_frame_3word(REG_PC-2, sr);
  m68ki_jump_vector(EXCEPTION_ILLEGAL_INSTRUCTION);

  /* Use up some clock cycles and undo the instruction's cycles */
  USE_CYCLES(CYC_EXCEPTION[EXCEPTION_ILLEGAL_INSTRUCTION] - CYC_INSTRUCTION[REG_IR]);
}


#if M68K_EMULATE_ADDRESS_ERROR
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
#endif

/* Service an interrupt request and start exception processing */
void m68ki_exception_interrupt(u32 int_level)
{
  u32 vector, sr, new_pc;

  #if M68K_EMULATE_ADDRESS_ERROR == OPT_ON
  CPU_INSTR_MODE = INSTRUCTION_NO;
  #endif /* M68K_EMULATE_ADDRESS_ERROR */

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
