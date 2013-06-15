#ifndef M68K__HEADER
#define M68K__HEADER

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

#include <setjmp.h>
#include "types.h"
#include "macros.h"

/* ======================================================================== */
/* ==================== ARCHITECTURE-DEPENDANT DEFINES ==================== */
/* ======================================================================== */

/* Check for > 32bit sizes */
#if UINT_MAX > 0xffffffff
  #define M68K_INT_GT_32_BIT  1
#else
  #define M68K_INT_GT_32_BIT  0
#endif


/* ======================================================================== */
/* ============================ GENERAL DEFINES =========================== */

/* ======================================================================== */

/* There are 7 levels of interrupt to the 68K.
 * A transition from < 7 to 7 will cause a non-maskable interrupt (NMI).
 */
#define M68K_IRQ_NONE 0
#define M68K_IRQ_1    1
#define M68K_IRQ_2    2
#define M68K_IRQ_3    3
#define M68K_IRQ_4    4
#define M68K_IRQ_5    5
#define M68K_IRQ_6    6
#define M68K_IRQ_7    7


/* Special interrupt acknowledge values.
 * Use these as special returns from the interrupt acknowledge callback
 * (specified later in this header).
 */

/* Causes an interrupt autovector (0x18 + interrupt level) to be taken.
 * This happens in a real 68K if VPA or AVEC is asserted during an interrupt
 * acknowledge cycle instead of DTACK.
 */
#define M68K_INT_ACK_AUTOVECTOR   0xffffffff

/* Causes the spurious interrupt vector (0x18) to be taken
 * This happens in a real 68K if BERR is asserted during the interrupt
 * acknowledge cycle (i.e. no devices responded to the acknowledge).
 */
#define M68K_INT_ACK_SPURIOUS     0xfffffffe


/* Registers used by m68k_get_reg() and m68k_set_reg() */
typedef enum
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
} m68k_register_t;


/* 68k memory map structure */
typedef struct 
{
  u8 *base;                             /* memory-based access (ROM, RAM) */
  u32 (*read8)(u32 address);               /* I/O byte read access */
  u32 (*read16)(u32 address);              /* I/O word read access */
  void (*write8)(u32 address, u32 data);  /* I/O byte write access */
  void (*write16)(u32 address, u32 data); /* I/O word write access */
} cpu_memory_map;

/* 68k idle loop detection */
typedef struct
{
  u32 pc;
  u32 cycle;
  u32 detected;
} cpu_idle_t;

typedef struct
{
  cpu_memory_map memory_map[256]; /* memory mapping */

  cpu_idle_t poll;      /* polling detection */

  u32 cycles;          /* current master cycle count */ 
  u32 cycle_end;       /* aimed master cycle count for current execution frame */

  u32 dar[16];         /* Data and Address Registers */
  u32 pc;              /* Program Counter */
  u32 sp[5];           /* User and Interrupt Stack Pointers */
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
  s32  (*int_ack_callback)(s32 int_line);           /* Interrupt Acknowledge */
  void (*reset_instr_callback)();               /* Called when a RESET instruction is encountered */
  s32  (*tas_instr_callback)();                 /* Called when a TAS instruction is encountered, allows / disallows writeback */
  void (*set_fc_callback)(u32 new_fc);     /* Called when the CPU function code changes */
} m68ki_cpu_core;

/* CPU cores */
extern m68ki_cpu_core m68k;
extern m68ki_cpu_core s68k;


/* ======================================================================== */
/* ============================== CALLBACKS =============================== */
/* ======================================================================== */

/* These functions allow you to set callbacks to the host when specific events
 * occur.  Note that you must enable the corresponding value in m68kconf.h
 * in order for these to do anything useful.
 * Note: I have defined default callbacks which are used if you have enabled
 * the corresponding #define in m68kconf.h but either haven't assigned a
 * callback or have assigned a callback of NULL.
 */



/* ======================================================================== */
/* ====================== FUNCTIONS TO ACCESS THE CPU ===================== */
/* ======================================================================== */

/* Do whatever initialisations the core requires.  Should be called
 * at least once at init time.
 */
extern void m68k_init();
extern void s68k_init();

/* Pulse the RESET pin on the CPU.
 * You *MUST* reset the CPU at least once to initialize the emulation
 */
extern void m68k_pulse_reset();
extern void s68k_pulse_reset();

/* Run until given cycle count is reached */
extern void m68k_run(u32 cycles);
extern void s68k_run(u32 cycles);

/* Set the IPL0-IPL2 pins on the CPU (IRQ).
 * A transition from < 7 to 7 will cause a non-maskable interrupt (NMI).
 * Setting IRQ to 0 will clear an interrupt request.
 */
extern void m68k_set_irq(u32 int_level);
extern void m68k_set_irq_delay(u32 int_level);
extern void m68k_update_irq(u32 mask);
extern void s68k_update_irq(u32 mask);

/* Halt the CPU as if you pulsed the HALT pin. */
extern void m68k_pulse_halt();
extern void m68k_clear_halt();
extern void s68k_pulse_halt();
extern void s68k_clear_halt();


/* Peek at the internals of a CPU context.  This can either be a context
 * retrieved using m68k_get_context() or the currently running context.
 * If context is NULL, the currently running CPU context will be used.
 */
extern u32 m68k_get_reg(m68k_register_t reg);
extern u32 s68k_get_reg(m68k_register_t reg);

/* Poke values into the internals of the currently running CPU context */
extern void m68k_set_reg(m68k_register_t reg, u32 value);
extern void s68k_set_reg(m68k_register_t reg, u32 value);


/* ======================================================================== */
/* ============================== END OF FILE ============================= */
/* ======================================================================== */

#endif /* M68K__HEADER */
