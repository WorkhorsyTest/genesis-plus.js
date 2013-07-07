
/* ======================================================================== */
/* ========================= LICENSING & COPYRIGHT ======================== */
/* ======================================================================== */
/*
 *                                  MUSASHI
 *                                Version 3.32
 *
 * A portable Motorola M680x0 processor emulation engine.
 * Copyright Karl Stenerud.  All rights reserved.
 *
 * This code may be freely used for non-commercial purposes as long as this
 * copyright notice remains unaltered in the source code and any binary files
 * containing this code in compiled form.
 *
 * All other licensing terms must be negotiated with the author
 * (Karl Stenerud).
 *
 * The latest version of this code can be obtained at:
 * http://kstenerud.cjb.net
 */

 /* Modified by Eke-Eke for Genesis Plus GX:

    - removed unused stuff to reduce memory usage / optimize execution (multiple CPU types support, NMI support, ...)
    - moved stuff to compile statically in a single object file
    - implemented support for global cycle count (shared by 68k & Z80 CPU)
    - added support for interrupt latency (Sesame's Street Counting Cafe, Fatal Rewind)
    - added proper cycle use on reset
    - added cycle accurate timings for MUL/DIV instructions (thanks to Jorge Cwik !) 
    - fixed undocumented flags for DIV instructions (Blood Shot)
    - added MAIN-CPU & SUB-CPU support for Mega CD emulation
    
  */

/* ======================================================================== */
/* ================================ INCLUDES ============================== */
/* ======================================================================== */

import types;
import macros;

/* ======================================================================== */
/* ==================== ARCHITECTURE-DEPENDANT DEFINES ==================== */
/* ======================================================================== */

/* Check for > 32bit sizes */
const int M68K_INT_GT_32_BIT = 0;


/* ======================================================================== */
/* ============================ GENERAL DEFINES =========================== */

/* ======================================================================== */

/* There are 7 levels of interrupt to the 68K.
 * A transition from < 7 to 7 will cause a non-maskable interrupt (NMI).
 */
const int M68K_IRQ_NONE = 0;
const int M68K_IRQ_1    = 1;
const int M68K_IRQ_2    = 2;
const int M68K_IRQ_3    = 3;
const int M68K_IRQ_4    = 4;
const int M68K_IRQ_5    = 5;
const int M68K_IRQ_6    = 6;
const int M68K_IRQ_7    = 7;


/* Special interrupt acknowledge values.
 * Use these as special returns from the interrupt acknowledge callback
 * (specified later in this header).
 */

/* Causes an interrupt autovector (0x18 + interrupt level) to be taken.
 * This happens in a real 68K if VPA or AVEC is asserted during an interrupt
 * acknowledge cycle instead of DTACK.
 */
const int M68K_INT_ACK_AUTOVECTOR   = 0xffffffff;

/* Causes the spurious interrupt vector (0x18) to be taken
 * This happens in a real 68K if BERR is asserted during the interrupt
 * acknowledge cycle (i.e. no devices responded to the acknowledge).
 */
const int M68K_INT_ACK_SPURIOUS     = 0xfffffffe;


/* Registers used by m68k_get_reg() and m68k_set_reg() */
enum m68k_register_t
{
  /* Real registers */
  M68K_REG_D0,    /* Data registers */
  M68K_REG_D1,
  M68K_REG_D2,
  M68K_REG_D3,
  M68K_REG_D4,
  M68K_REG_D5,
  M68K_REG_D6,
  M68K_REG_D7,
  M68K_REG_A0,    /* Address registers */
  M68K_REG_A1,
  M68K_REG_A2,
  M68K_REG_A3,
  M68K_REG_A4,
  M68K_REG_A5,
  M68K_REG_A6,
  M68K_REG_A7,
  M68K_REG_PC,    /* Program Counter */
  M68K_REG_SR,    /* Status Register */
  M68K_REG_SP,    /* The current Stack Pointer (located in A7) */
  M68K_REG_USP,   /* User Stack Pointer */
  M68K_REG_ISP,   /* Interrupt Stack Pointer */

  /* Convenience registers */
  M68K_REG_IR    /* Instruction register */
}


/* 68k memory map structure */
struct cpu_memory_map
{
  u8* base;                             /* memory-based access (ROM, RAM) */
  u32 function(u32 address) read8;               /* I/O byte read access */
  u32 function(u32 address) read16;              /* I/O word read access */
  void function(u32 address, u32 data) write8;  /* I/O byte write access */
  void function(u32 address, u32 data) write16; /* I/O word write access */
}

/* 68k idle loop detection */
struct cpu_idle_t
{
  u32 pc;
  u32 cycle;
  u32 detected;
}

struct m68ki_cpu_core
{
  cpu_memory_map[256] memory_map; /* memory mapping */

  cpu_idle_t poll;      /* polling detection */

  u32 cycles;          /* current master cycle count */ 
  u32 cycle_end;       /* aimed master cycle count for current execution frame */

  u32[16] dar;         /* Data and Address Registers */
  u32 pc;              /* Program Counter */
  u32[5] sp;           /* User and Interrupt Stack Pointers */
  u32 ir;              /* Instruction Register */
  u32 t1_flag;         /* Trace 1 */
  u32 s_flag;          /* Supervisor */
  u32 x_flag;          /* Extend */
  u32 n_flag;          /* Negative */
  u32 not_z_flag;      /* Zero, inverted for speedups */
  u32 v_flag;          /* Overflow */
  u32 c_flag;          /* Carry */
  u32 int_mask;        /* I0-I2 */
  u32 int_level;       /* State of interrupt pins IPL0-IPL2 -- ASG: changed from ints_pending */
  u32 stopped;         /* Stopped state */

  u32 pref_addr;       /* Last prefetch address */
  u32 pref_data;       /* Data in the prefetch queue */

  u32 instr_mode;      /* Stores whether we are in instruction mode or group 0/1 exception mode */
  u32 run_mode;        /* Stores whether we are processing a reset, bus error, address error, or something else */
  u32 aerr_enabled;    /* Enables/deisables address error checks at runtime */
  jmp_buf aerr_trap;    /* Address error jump */
  u32 aerr_address;    /* Address error location */
  u32 aerr_write_mode; /* Address error write mode */
  u32 aerr_fc;         /* Address error FC code */

  u32 tracing;         /* Tracing enable flag */

  u32 address_space;   /* Current FC code */

  /* Callbacks to host */
  s32  function(s32 int_line) int_ack_callback;           /* Interrupt Acknowledge */
  void function() reset_instr_callback;               /* Called when a RESET instruction is encountered */
  s32  function() tas_instr_callback;                 /* Called when a TAS instruction is encountered, allows / disallows writeback */
  void function(u32 new_fc) set_fc_callback;     /* Called when the CPU function code changes */
}





