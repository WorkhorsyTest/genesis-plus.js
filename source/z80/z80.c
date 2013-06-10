/*****************************************************************************
 *
 *   z80.c
 *   Portable Z80 emulator V3.9
 *
 *   Copyright Juergen Buchmueller, all rights reserved.
 *
 *   - This source code is released as freeware for non-commercial purposes.
 *   - You are free to use and redistribute this code in modified or
 *     unmodified form, provided you list me in the credits.
 *   - If you modify this source code, you must add a notice to each modified
 *     source file that it has been changed.  If you're a nice person, you
 *     will clearly mark each change too.  :)
 *   - If you wish to use this for commercial purposes, please contact me at
 *     pullmoll@t-online.de
 *   - The author of this copywritten work reserves the right to change the
 *     terms of its usage and license at any time, including retroactively
 *   - This entire notice must remain in the source code.
 *
 *   TODO:
 *    - If LD A,I or LD A,R is interrupted, P/V flag gets reset, even if IFF2
 *      was set before this instruction
 *    - Ideally, the tiny differences between Z80 types should be supported,
 *      currently known differences:
 *       - LD A,I/R P/V flag reset glitch is fixed on CMOS Z80
 *       - OUT (C),0 outputs 0 on NMOS Z80, $FF on CMOS Z80
 *       - SCF/CCF X/Y flags is ((flags | A) & 0x28) on SGS/SHARP/ZiLOG NMOS Z80,
 *         (flags & A & 0x28) on NEC NMOS Z80, other models unknown.
 *         However, people from the Speccy scene mention that SCF/CCF X/Y results
 *         are inconsistant and may be influenced by I and R registers.
 *      This Z80 emulator assumes a ZiLOG NMOS model.
 *
 *   Additional changes [Eke-Eke]:
 *    - Removed z80_burn function (unused)
 *    - Discarded multi-chip support (unused)
 *    - Fixed cycle counting for FD and DD prefixed instructions
 *    - Fixed behavior of chained FD and DD prefixes (R register should be only incremented by one
 *    - Implemented cycle-accurate INI/IND (needed by SMS emulation)
 *    - Fixed Z80 reset
 *    - Made SZHVC_add & SZHVC_sub tables statically allocated
 *   Changes in 3.9:
 *    - Fixed cycle counts for LD IYL/IXL/IYH/IXH,n [Marshmellow]
 *    - Fixed X/Y flags in CCF/SCF/BIT, ZEXALL is happy now [hap]
 *    - Simplified DAA, renamed MEMPTR (3.8) to WZ, added TODO [hap]
 *    - Fixed IM2 interrupt cycles [eke]
 *   Changes in 3.8 [Miodrag Milanovic]:
 *   - Added MEMPTR register (according to informations provided
 *     by Vladimir Kladov
 *   - BIT n,(HL) now return valid values due to use of MEMPTR
 *   - Fixed BIT 6,(XY+o) undocumented instructions
 *   Changes in 3.7 [Aaron Giles]:
 *   - Changed NMI handling. NMIs are now latched in set_irq_state
 *     but are not taken there. Instead they are taken at the start of the
 *     execute loop.
 *   - Changed IRQ handling. IRQ state is set in set_irq_state but not taken
 *     except during the inner execute loop.
 *   - Removed x86 assembly hacks and obsolete timing loop catchers.
 *   Changes in 3.6:
 *   - Got rid of the code that would inexactly emulate a Z80, i.e. removed
 *     all the #if Z80_EXACT #else branches.
 *   - Removed leading underscores from local register name shortcuts as
 *     this violates the C99 standard.
 *   - Renamed the registers inside the Z80 context to lower case to avoid
 *     ambiguities (shortcuts would have had the same names as the fields
 *     of the structure).
 *   Changes in 3.5:
 *   - Implemented OTIR, INIR, etc. without look-up table for PF flag.
 *     [Ramsoft, Sean Young]
 *   Changes in 3.4:
 *   - Removed Z80-MSX specific code as it's not needed any more.
 *   - Implemented DAA without look-up table [Ramsoft, Sean Young]
 *   Changes in 3.3:
 *   - Fixed undocumented flags XF & YF in the non-asm versions of CP,
 *     and all the 16 bit arithmetic instructions. [Sean Young]
 *   Changes in 3.2:
 *   - Fixed undocumented flags XF & YF of RRCA, and CF and HF of
 *     INI/IND/OUTI/OUTD/INIR/INDR/OTIR/OTDR [Sean Young]
 *   Changes in 3.1:
 *   - removed the REPEAT_AT_ONCE execution of LDIR/CPIR etc. opcodes
 *     for readabilities sake and because the implementation was buggy
 *     (and I was not able to find the difference)
 *   Changes in 3.0:
 *   - 'finished' switch to dynamically overrideable cycle count tables
 *   Changes in 2.9:
 *   - added methods to access and override the cycle count tables
 *   - fixed handling and timing of multiple DD/FD prefixed opcodes
 *   Changes in 2.8:
 *   - OUTI/OUTD/OTIR/OTDR also pre-decrement the B register now.
 *     This was wrong because of a bug fix on the wrong side
 *     (astrocade sound driver).
 *   Changes in 2.7:
 *    - removed z80_vm specific code, it's not needed (and never was).
 *   Changes in 2.6:
 *    - BUSY_LOOP_HACKS needed to call change_pc() earlier, before
 *    checking the opcodes at the new address, because otherwise they
 *    might access the old (wrong or even NULL) banked memory region.
 *    Thanks to Sean Young for finding this nasty bug.
 *   Changes in 2.5:
 *    - Burning cycles always adjusts the ICount by a multiple of 4.
 *    - In REPEAT_AT_ONCE cases the R register wasn't incremented twice
 *    per repetition as it should have been. Those repeated opcodes
 *    could also underflow the ICount.
 *    - Simplified TIME_LOOP_HACKS for BC and added two more for DE + HL
 *    timing loops. I think those hacks weren't endian safe before too.
 *   Changes in 2.4:
 *    - z80_reset zaps the entire context, sets IX and IY to 0xffff(!) and
 *    sets the Z flag. With these changes the Tehkan World Cup driver
 *    _seems_ to work again.
 *   Changes in 2.3:
 *    - External termination of the execution loop calls z80_burn() and
 *    z80_vm_burn() to burn an amount of cycles (R adjustment)
 *    - Shortcuts which burn CPU cycles (BUSY_LOOP_HACKS and TIME_LOOP_HACKS)
 *    now also adjust the R register depending on the skipped opcodes.
 *   Changes in 2.2:
 *    - Fixed bugs in CPL, SCF and CCF instructions flag handling.
 *    - Changed variable EA and ARG16() function to UINT32; this
 *    produces slightly more efficient code.
 *    - The DD/FD XY CB opcodes where XY is 40-7F and Y is not 6/E
 *    are changed to calls to the X6/XE opcodes to reduce object size.
 *    They're hardly ever used so this should not yield a speed penalty.
 *   New in 2.0:
 *    - Optional more exact Z80 emulation (#define Z80_EXACT 1) according
 *    to a detailed description by Sean Young which can be found at:
 *      http://www.msxnet.org/tech/z80-documented.pdf
 *****************************************************************************/
#include <stdbool.h>
#include "shared.h"
#include "z80.h"

/* execute main opcodes inside a big switch statement */
bool BIG_SWITCH = TRUE;

bool VERBOSE = FALSE;

#if VERBOSE
#define LOG(x)  logerror x
#else
#define LOG(x)
#endif

unsigned cpu_readop(unsigned a) {
	return z80_readmap[(a) >> 10][(a) & 0x03FF];
}

unsigned cpu_readop_arg(unsigned a) {
	return z80_readmap[(a) >> 10][(a) & 0x03FF];
}

#define CF  0x01
#define NF  0x02
#define PF  0x04
#define VF  PF
#define XF  0x08
#define HF  0x10
#define YF  0x20
#define ZF  0x40
#define SF  0x80

#define INT_IRQ 0x01
#define NMI_IRQ 0x02

#define PCD  Z80.pc.d
#define PC Z80.pc.w.l

#define SPD Z80.sp.d
#define SP Z80.sp.w.l

#define AFD Z80.af.d
#define AF Z80.af.w.l
#define A Z80.af.b.h
#define F Z80.af.b.l

#define BCD Z80.bc.d
#define BC Z80.bc.w.l
#define B Z80.bc.b.h
#define C Z80.bc.b.l

#define DED Z80.de.d
#define DE Z80.de.w.l
#define D Z80.de.b.h
#define E Z80.de.b.l

#define HLD Z80.hl.d
#define HL Z80.hl.w.l
#define H Z80.hl.b.h
#define L Z80.hl.b.l

#define IXD Z80.ix.d
#define IX Z80.ix.w.l
#define HX Z80.ix.b.h
#define LX Z80.ix.b.l

#define IYD Z80.iy.d
#define IY Z80.iy.w.l
#define HY Z80.iy.b.h
#define LY Z80.iy.b.l

#define WZ   Z80.wz.w.l
#define WZ_H Z80.wz.b.h
#define WZ_L Z80.wz.b.l

#define I Z80.i
#define R Z80.r
#define R2 Z80.r2
#define IM Z80.im
#define IFF1 Z80.iff1
#define IFF2 Z80.iff2
#define HALT Z80.halt

Z80_Regs Z80;

unsigned char *z80_readmap[64];
unsigned char *z80_writemap[64];

void (*z80_writemem)(unsigned int address, unsigned char data);
unsigned char (*z80_readmem)(unsigned int address);
void (*z80_writeport)(unsigned int port, unsigned char data);
unsigned char (*z80_readport)(unsigned int port);

static UINT32 EA;

static UINT8 SZ[256];       /* zero and sign flags */
static UINT8 SZ_BIT[256];   /* zero, sign and parity/overflow (=zero) flags for BIT opcode */
static UINT8 SZP[256];      /* zero, sign and parity flags */
static UINT8 SZHV_inc[256]; /* zero, sign, half carry and overflow flags INC r8 */
static UINT8 SZHV_dec[256]; /* zero, sign, half carry and overflow flags DEC r8 */

static UINT8 SZHVC_add[2*256*256]; /* flags for ADD opcode */
static UINT8 SZHVC_sub[2*256*256]; /* flags for SUB opcode */

static const UINT16 cc_op[0x100] = {
   4*15,10*15, 7*15, 6*15, 4*15, 4*15, 7*15, 4*15, 4*15,11*15, 7*15, 6*15, 4*15, 4*15, 7*15, 4*15,
   8*15,10*15, 7*15, 6*15, 4*15, 4*15, 7*15, 4*15,12*15,11*15, 7*15, 6*15, 4*15, 4*15, 7*15, 4*15,
   7*15,10*15,16*15, 6*15, 4*15, 4*15, 7*15, 4*15, 7*15,11*15,16*15, 6*15, 4*15, 4*15, 7*15, 4*15,
   7*15,10*15,13*15, 6*15,11*15,11*15,10*15, 4*15, 7*15,11*15,13*15, 6*15, 4*15, 4*15, 7*15, 4*15,
   4*15, 4*15, 4*15, 4*15, 4*15, 4*15, 7*15, 4*15, 4*15, 4*15, 4*15, 4*15, 4*15, 4*15, 7*15, 4*15,
   4*15, 4*15, 4*15, 4*15, 4*15, 4*15, 7*15, 4*15, 4*15, 4*15, 4*15, 4*15, 4*15, 4*15, 7*15, 4*15,
   4*15, 4*15, 4*15, 4*15, 4*15, 4*15, 7*15, 4*15, 4*15, 4*15, 4*15, 4*15, 4*15, 4*15, 7*15, 4*15,
   7*15, 7*15, 7*15, 7*15, 7*15, 7*15, 4*15, 7*15, 4*15, 4*15, 4*15, 4*15, 4*15, 4*15, 7*15, 4*15,
   4*15, 4*15, 4*15, 4*15, 4*15, 4*15, 7*15, 4*15, 4*15, 4*15, 4*15, 4*15, 4*15, 4*15, 7*15, 4*15,
   4*15, 4*15, 4*15, 4*15, 4*15, 4*15, 7*15, 4*15, 4*15, 4*15, 4*15, 4*15, 4*15, 4*15, 7*15, 4*15,
   4*15, 4*15, 4*15, 4*15, 4*15, 4*15, 7*15, 4*15, 4*15, 4*15, 4*15, 4*15, 4*15, 4*15, 7*15, 4*15,
   4*15, 4*15, 4*15, 4*15, 4*15, 4*15, 7*15, 4*15, 4*15, 4*15, 4*15, 4*15, 4*15, 4*15, 7*15, 4*15,
   5*15,10*15,10*15,10*15,10*15,11*15, 7*15,11*15, 5*15,10*15,10*15, 0*15,10*15,17*15, 7*15,11*15,
   5*15,10*15,10*15,11*15,10*15,11*15, 7*15,11*15, 5*15, 4*15,10*15,11*15,10*15, 0*15, 7*15,11*15,
   5*15,10*15,10*15,19*15,10*15,11*15, 7*15,11*15, 5*15, 4*15,10*15, 4*15,10*15, 0*15, 7*15,11*15,
   5*15,10*15,10*15, 4*15,10*15,11*15, 7*15,11*15, 5*15, 6*15,10*15, 4*15,10*15, 0*15, 7*15,11*15};

static const UINT16 cc_cb[0x100] = {
   8*15, 8*15, 8*15, 8*15, 8*15, 8*15,15*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15,15*15, 8*15,
   8*15, 8*15, 8*15, 8*15, 8*15, 8*15,15*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15,15*15, 8*15,
   8*15, 8*15, 8*15, 8*15, 8*15, 8*15,15*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15,15*15, 8*15,
   8*15, 8*15, 8*15, 8*15, 8*15, 8*15,15*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15,15*15, 8*15,
   8*15, 8*15, 8*15, 8*15, 8*15, 8*15,12*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15,12*15, 8*15,
   8*15, 8*15, 8*15, 8*15, 8*15, 8*15,12*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15,12*15, 8*15,
   8*15, 8*15, 8*15, 8*15, 8*15, 8*15,12*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15,12*15, 8*15,
   8*15, 8*15, 8*15, 8*15, 8*15, 8*15,12*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15,12*15, 8*15,
   8*15, 8*15, 8*15, 8*15, 8*15, 8*15,15*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15,15*15, 8*15,
   8*15, 8*15, 8*15, 8*15, 8*15, 8*15,15*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15,15*15, 8*15,
   8*15, 8*15, 8*15, 8*15, 8*15, 8*15,15*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15,15*15, 8*15,
   8*15, 8*15, 8*15, 8*15, 8*15, 8*15,15*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15,15*15, 8*15,
   8*15, 8*15, 8*15, 8*15, 8*15, 8*15,15*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15,15*15, 8*15,
   8*15, 8*15, 8*15, 8*15, 8*15, 8*15,15*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15,15*15, 8*15,
   8*15, 8*15, 8*15, 8*15, 8*15, 8*15,15*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15,15*15, 8*15,
   8*15, 8*15, 8*15, 8*15, 8*15, 8*15,15*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15,15*15, 8*15};

static const UINT16 cc_ed[0x100] = {
   8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15,
   8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15,
   8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15,
   8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15,
  12*15,12*15,15*15,20*15, 8*15,14*15, 8*15, 9*15,12*15,12*15,15*15,20*15, 8*15,14*15, 8*15, 9*15,
  12*15,12*15,15*15,20*15, 8*15,14*15, 8*15, 9*15,12*15,12*15,15*15,20*15, 8*15,14*15, 8*15, 9*15,
  12*15,12*15,15*15,20*15, 8*15,14*15, 8*15,18*15,12*15,12*15,15*15,20*15, 8*15,14*15, 8*15,18*15,
  12*15,12*15,15*15,20*15, 8*15,14*15, 8*15, 8*15,12*15,12*15,15*15,20*15, 8*15,14*15, 8*15, 8*15,
   8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15,
   8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15,
  16*15,16*15,16*15,16*15, 8*15, 8*15, 8*15, 8*15,16*15,16*15,16*15,16*15, 8*15, 8*15, 8*15, 8*15,
  16*15,16*15,16*15,16*15, 8*15, 8*15, 8*15, 8*15,16*15,16*15,16*15,16*15, 8*15, 8*15, 8*15, 8*15,
   8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15,
   8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15,
   8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15,
   8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15, 8*15};

/*static const UINT8 cc_xy[0x100] = {
 4*15, 4*15, 4*15, 4*15, 4*15, 4*15, 4*15, 4*15, 4*15,15*15, 4*15, 4*15, 4*15, 4*15, 4*15, 4*15,
 4*15, 4*15, 4*15, 4*15, 4*15, 4*15, 4*15, 4*15, 4*15,15*15, 4*15, 4*15, 4*15, 4*15, 4*15, 4*15,
 4*15,14*15,20*15,10*15, 9*15, 9*15,11*15, 4*15, 4*15,15*15,20*15,10*15, 9*15, 9*15,11*15, 4*15,
 4*15, 4*15, 4*15, 4*15,23*15,23*15,19*15, 4*15, 4*15,15*15, 4*15, 4*15, 4*15, 4*15, 4*15, 4*15,
 4*15, 4*15, 4*15, 4*15, 9*15, 9*15,19*15, 4*15, 4*15, 4*15, 4*15, 4*15, 9*15, 9*15,19*15, 4*15,
 4*15, 4*15, 4*15, 4*15, 9*15, 9*15,19*15, 4*15, 4*15, 4*15, 4*15, 4*15, 9*15, 9*15,19*15, 4*15,
 9*15, 9*15, 9*15, 9*15, 9*15, 9*15,19*15, 9*15, 9*15, 9*15, 9*15, 9*15, 9*15, 9*15,19*15, 9*15,
19*15,19*15,19*15,19*15,19*15,19*15, 4*15,19*15, 4*15, 4*15, 4*15, 4*15, 9*15, 9*15,19*15, 4*15,
 4*15, 4*15, 4*15, 4*15, 9*15, 9*15,19*15, 4*15, 4*15, 4*15, 4*15, 4*15, 9*15, 9*15,19*15, 4*15,
 4*15, 4*15, 4*15, 4*15, 9*15, 9*15,19*15, 4*15, 4*15, 4*15, 4*15, 4*15, 9*15, 9*15,19*15, 4*15,
 4*15, 4*15, 4*15, 4*15, 9*15, 9*15,19*15, 4*15, 4*15, 4*15, 4*15, 4*15, 9*15, 9*15,19*15, 4*15,
 4*15, 4*15, 4*15, 4*15, 9*15, 9*15,19*15, 4*15, 4*15, 4*15, 4*15, 4*15, 9*15, 9*15,19*15, 4*15,
 4*15, 4*15, 4*15, 4*15, 4*15, 4*15, 4*15, 4*15, 4*15, 4*15, 4*15, 0*15, 4*15, 4*15, 4*15, 4*15,
 4*15, 4*15, 4*15, 4*15, 4*15, 4*15, 4*15, 4*15, 4*15, 4*15, 4*15, 4*15, 4*15, 4*15, 4*15, 4*15,
 4*15,14*15, 4*15,23*15, 4*15,15*15, 4*15, 4*15, 4*15, 8*15, 4*15, 4*15, 4*15, 4*15, 4*15, 4*15,
 4*15, 4*15, 4*15, 4*15, 4*15, 4*15, 4*15, 4*15, 4*15,10*15, 4*15, 4*15, 4*15, 4*15, 4*15, 4*15};
*/

/* illegal combo should return 4 + cc_op[i] */
static const UINT16 cc_xy[0x100] ={
   8*15,14*15,11*15,10*15, 8*15, 8*15,11*15, 8*15, 8*15,15*15,11*15,10*15, 8*15, 8*15,11*15, 8*15,
  12*15,14*15,11*15,10*15, 8*15, 8*15,11*15, 8*15,16*15,15*15,11*15,10*15, 8*15, 8*15,11*15, 8*15,
  11*15,14*15,20*15,10*15, 9*15, 9*15,12*15, 8*15,11*15,15*15,20*15,10*15, 9*15, 9*15,12*15, 8*15,
  11*15,14*15,17*15,10*15,23*15,23*15,19*15, 8*15,11*15,15*15,17*15,10*15, 8*15, 8*15,11*15, 8*15,
   8*15, 8*15, 8*15, 8*15, 9*15, 9*15,19*15, 8*15, 8*15, 8*15, 8*15, 8*15, 9*15, 9*15,19*15, 8*15,
   8*15, 8*15, 8*15, 8*15, 9*15, 9*15,19*15, 8*15, 8*15, 8*15, 8*15, 8*15, 9*15, 9*15,19*15, 8*15,
   9*15, 9*15, 9*15, 9*15, 9*15, 9*15,19*15, 9*15, 9*15, 9*15, 9*15, 9*15, 9*15, 9*15,19*15, 9*15,
  19*15,19*15,19*15,19*15,19*15,19*15, 8*15,19*15, 8*15, 8*15, 8*15, 8*15, 9*15, 9*15,19*15, 8*15,
   8*15, 8*15, 8*15, 8*15, 9*15, 9*15,19*15, 8*15, 8*15, 8*15, 8*15, 8*15, 9*15, 9*15,19*15, 8*15,
   8*15, 8*15, 8*15, 8*15, 9*15, 9*15,19*15, 8*15, 8*15, 8*15, 8*15, 8*15, 9*15, 9*15,19*15, 8*15,
   8*15, 8*15, 8*15, 8*15, 9*15, 9*15,19*15, 8*15, 8*15, 8*15, 8*15, 8*15, 9*15, 9*15,19*15, 8*15,
   8*15, 8*15, 8*15, 8*15, 9*15, 9*15,19*15, 8*15, 8*15, 8*15, 8*15, 8*15, 9*15, 9*15,19*15, 8*15,
   9*15,14*15,14*15,14*15,14*15,15*15,11*15,15*15, 9*15,14*15,14*15, 0*15,14*15,21*15,11*15,15*15,
   9*15,14*15,14*15,15*15,14*15,15*15,11*15,15*15, 9*15, 8*15,14*15,15*15,14*15, 4*15,11*15,15*15,
   9*15,14*15,14*15,23*15,14*15,15*15,11*15,15*15, 9*15, 8*15,14*15, 8*15,14*15, 4*15,11*15,15*15,
   9*15,14*15,14*15, 8*15,14*15,15*15,11*15,15*15, 9*15,10*15,14*15, 8*15,14*15, 4*15,11*15,15*15};

static const UINT16 cc_xycb[0x100] = {
  23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,
  23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,
  23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,
  23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,
  20*15,20*15,20*15,20*15,20*15,20*15,20*15,20*15,20*15,20*15,20*15,20*15,20*15,20*15,20*15,20*15,
  20*15,20*15,20*15,20*15,20*15,20*15,20*15,20*15,20*15,20*15,20*15,20*15,20*15,20*15,20*15,20*15,
  20*15,20*15,20*15,20*15,20*15,20*15,20*15,20*15,20*15,20*15,20*15,20*15,20*15,20*15,20*15,20*15,
  20*15,20*15,20*15,20*15,20*15,20*15,20*15,20*15,20*15,20*15,20*15,20*15,20*15,20*15,20*15,20*15,
  23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,
  23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,
  23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,
  23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,
  23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,
  23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,
  23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,
  23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15,23*15};

/* extra cycles if jr/jp/call taken and 'interrupt latency' on rst 0-7 */
static const UINT16 cc_ex[0x100] = {
 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15,
 5*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15,  /* DJNZ */
 5*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 5*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15,  /* JR NZ/JR Z */
 5*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 5*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15,  /* JR NC/JR C */
 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15,
 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15,
 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15,
 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15,
 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15,
 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15,
 0*15, 0*15, 4*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 0*15, 4*15, 0*15, 0*15, 0*15, 0*15, 0*15,  /* INI/IND (cycle-accurate I/O port reads) */
 5*15, 5*15, 5*15, 5*15, 0*15, 0*15, 0*15, 0*15, 5*15, 5*15, 5*15, 5*15, 0*15, 0*15, 0*15, 0*15,  /* LDIR/CPIR/INIR/OTIR LDDR/CPDR/INDR/OTDR */
 6*15, 0*15, 0*15, 0*15, 7*15, 0*15, 0*15, 2*15, 6*15, 0*15, 0*15, 0*15, 7*15, 0*15, 0*15, 2*15,
 6*15, 0*15, 0*15, 0*15, 7*15, 0*15, 0*15, 2*15, 6*15, 0*15, 0*15, 0*15, 7*15, 0*15, 0*15, 2*15,
 6*15, 0*15, 0*15, 0*15, 7*15, 0*15, 0*15, 2*15, 6*15, 0*15, 0*15, 0*15, 7*15, 0*15, 0*15, 2*15,
 6*15, 0*15, 0*15, 0*15, 7*15, 0*15, 0*15, 2*15, 6*15, 0*15, 0*15, 0*15, 7*15, 0*15, 0*15, 2*15};

static const UINT16 *cc[6];
#define Z80_TABLE_dd  Z80_TABLE_xy
#define Z80_TABLE_fd  Z80_TABLE_xy

typedef void (*funcptr)(void);

#define PROTOTYPES(tablename,prefix) \
  INLINE void prefix##_00(void); INLINE void prefix##_01(void); INLINE void prefix##_02(void); INLINE void prefix##_03(void); \
  INLINE void prefix##_04(void); INLINE void prefix##_05(void); INLINE void prefix##_06(void); INLINE void prefix##_07(void); \
  INLINE void prefix##_08(void); INLINE void prefix##_09(void); INLINE void prefix##_0a(void); INLINE void prefix##_0b(void); \
  INLINE void prefix##_0c(void); INLINE void prefix##_0d(void); INLINE void prefix##_0e(void); INLINE void prefix##_0f(void); \
  INLINE void prefix##_10(void); INLINE void prefix##_11(void); INLINE void prefix##_12(void); INLINE void prefix##_13(void); \
  INLINE void prefix##_14(void); INLINE void prefix##_15(void); INLINE void prefix##_16(void); INLINE void prefix##_17(void); \
  INLINE void prefix##_18(void); INLINE void prefix##_19(void); INLINE void prefix##_1a(void); INLINE void prefix##_1b(void); \
  INLINE void prefix##_1c(void); INLINE void prefix##_1d(void); INLINE void prefix##_1e(void); INLINE void prefix##_1f(void); \
  INLINE void prefix##_20(void); INLINE void prefix##_21(void); INLINE void prefix##_22(void); INLINE void prefix##_23(void); \
  INLINE void prefix##_24(void); INLINE void prefix##_25(void); INLINE void prefix##_26(void); INLINE void prefix##_27(void); \
  INLINE void prefix##_28(void); INLINE void prefix##_29(void); INLINE void prefix##_2a(void); INLINE void prefix##_2b(void); \
  INLINE void prefix##_2c(void); INLINE void prefix##_2d(void); INLINE void prefix##_2e(void); INLINE void prefix##_2f(void); \
  INLINE void prefix##_30(void); INLINE void prefix##_31(void); INLINE void prefix##_32(void); INLINE void prefix##_33(void); \
  INLINE void prefix##_34(void); INLINE void prefix##_35(void); INLINE void prefix##_36(void); INLINE void prefix##_37(void); \
  INLINE void prefix##_38(void); INLINE void prefix##_39(void); INLINE void prefix##_3a(void); INLINE void prefix##_3b(void); \
  INLINE void prefix##_3c(void); INLINE void prefix##_3d(void); INLINE void prefix##_3e(void); INLINE void prefix##_3f(void); \
  INLINE void prefix##_40(void); INLINE void prefix##_41(void); INLINE void prefix##_42(void); INLINE void prefix##_43(void); \
  INLINE void prefix##_44(void); INLINE void prefix##_45(void); INLINE void prefix##_46(void); INLINE void prefix##_47(void); \
  INLINE void prefix##_48(void); INLINE void prefix##_49(void); INLINE void prefix##_4a(void); INLINE void prefix##_4b(void); \
  INLINE void prefix##_4c(void); INLINE void prefix##_4d(void); INLINE void prefix##_4e(void); INLINE void prefix##_4f(void); \
  INLINE void prefix##_50(void); INLINE void prefix##_51(void); INLINE void prefix##_52(void); INLINE void prefix##_53(void); \
  INLINE void prefix##_54(void); INLINE void prefix##_55(void); INLINE void prefix##_56(void); INLINE void prefix##_57(void); \
  INLINE void prefix##_58(void); INLINE void prefix##_59(void); INLINE void prefix##_5a(void); INLINE void prefix##_5b(void); \
  INLINE void prefix##_5c(void); INLINE void prefix##_5d(void); INLINE void prefix##_5e(void); INLINE void prefix##_5f(void); \
  INLINE void prefix##_60(void); INLINE void prefix##_61(void); INLINE void prefix##_62(void); INLINE void prefix##_63(void); \
  INLINE void prefix##_64(void); INLINE void prefix##_65(void); INLINE void prefix##_66(void); INLINE void prefix##_67(void); \
  INLINE void prefix##_68(void); INLINE void prefix##_69(void); INLINE void prefix##_6a(void); INLINE void prefix##_6b(void); \
  INLINE void prefix##_6c(void); INLINE void prefix##_6d(void); INLINE void prefix##_6e(void); INLINE void prefix##_6f(void); \
  INLINE void prefix##_70(void); INLINE void prefix##_71(void); INLINE void prefix##_72(void); INLINE void prefix##_73(void); \
  INLINE void prefix##_74(void); INLINE void prefix##_75(void); INLINE void prefix##_76(void); INLINE void prefix##_77(void); \
  INLINE void prefix##_78(void); INLINE void prefix##_79(void); INLINE void prefix##_7a(void); INLINE void prefix##_7b(void); \
  INLINE void prefix##_7c(void); INLINE void prefix##_7d(void); INLINE void prefix##_7e(void); INLINE void prefix##_7f(void); \
  INLINE void prefix##_80(void); INLINE void prefix##_81(void); INLINE void prefix##_82(void); INLINE void prefix##_83(void); \
  INLINE void prefix##_84(void); INLINE void prefix##_85(void); INLINE void prefix##_86(void); INLINE void prefix##_87(void); \
  INLINE void prefix##_88(void); INLINE void prefix##_89(void); INLINE void prefix##_8a(void); INLINE void prefix##_8b(void); \
  INLINE void prefix##_8c(void); INLINE void prefix##_8d(void); INLINE void prefix##_8e(void); INLINE void prefix##_8f(void); \
  INLINE void prefix##_90(void); INLINE void prefix##_91(void); INLINE void prefix##_92(void); INLINE void prefix##_93(void); \
  INLINE void prefix##_94(void); INLINE void prefix##_95(void); INLINE void prefix##_96(void); INLINE void prefix##_97(void); \
  INLINE void prefix##_98(void); INLINE void prefix##_99(void); INLINE void prefix##_9a(void); INLINE void prefix##_9b(void); \
  INLINE void prefix##_9c(void); INLINE void prefix##_9d(void); INLINE void prefix##_9e(void); INLINE void prefix##_9f(void); \
  INLINE void prefix##_a0(void); INLINE void prefix##_a1(void); INLINE void prefix##_a2(void); INLINE void prefix##_a3(void); \
  INLINE void prefix##_a4(void); INLINE void prefix##_a5(void); INLINE void prefix##_a6(void); INLINE void prefix##_a7(void); \
  INLINE void prefix##_a8(void); INLINE void prefix##_a9(void); INLINE void prefix##_aa(void); INLINE void prefix##_ab(void); \
  INLINE void prefix##_ac(void); INLINE void prefix##_ad(void); INLINE void prefix##_ae(void); INLINE void prefix##_af(void); \
  INLINE void prefix##_b0(void); INLINE void prefix##_b1(void); INLINE void prefix##_b2(void); INLINE void prefix##_b3(void); \
  INLINE void prefix##_b4(void); INLINE void prefix##_b5(void); INLINE void prefix##_b6(void); INLINE void prefix##_b7(void); \
  INLINE void prefix##_b8(void); INLINE void prefix##_b9(void); INLINE void prefix##_ba(void); INLINE void prefix##_bb(void); \
  INLINE void prefix##_bc(void); INLINE void prefix##_bd(void); INLINE void prefix##_be(void); INLINE void prefix##_bf(void); \
  INLINE void prefix##_c0(void); INLINE void prefix##_c1(void); INLINE void prefix##_c2(void); INLINE void prefix##_c3(void); \
  INLINE void prefix##_c4(void); INLINE void prefix##_c5(void); INLINE void prefix##_c6(void); INLINE void prefix##_c7(void); \
  INLINE void prefix##_c8(void); INLINE void prefix##_c9(void); INLINE void prefix##_ca(void); INLINE void prefix##_cb(void); \
  INLINE void prefix##_cc(void); INLINE void prefix##_cd(void); INLINE void prefix##_ce(void); INLINE void prefix##_cf(void); \
  INLINE void prefix##_d0(void); INLINE void prefix##_d1(void); INLINE void prefix##_d2(void); INLINE void prefix##_d3(void); \
  INLINE void prefix##_d4(void); INLINE void prefix##_d5(void); INLINE void prefix##_d6(void); INLINE void prefix##_d7(void); \
  INLINE void prefix##_d8(void); INLINE void prefix##_d9(void); INLINE void prefix##_da(void); INLINE void prefix##_db(void); \
  INLINE void prefix##_dc(void); INLINE void prefix##_dd(void); INLINE void prefix##_de(void); INLINE void prefix##_df(void); \
  INLINE void prefix##_e0(void); INLINE void prefix##_e1(void); INLINE void prefix##_e2(void); INLINE void prefix##_e3(void); \
  INLINE void prefix##_e4(void); INLINE void prefix##_e5(void); INLINE void prefix##_e6(void); INLINE void prefix##_e7(void); \
  INLINE void prefix##_e8(void); INLINE void prefix##_e9(void); INLINE void prefix##_ea(void); INLINE void prefix##_eb(void); \
  INLINE void prefix##_ec(void); INLINE void prefix##_ed(void); INLINE void prefix##_ee(void); INLINE void prefix##_ef(void); \
  INLINE void prefix##_f0(void); INLINE void prefix##_f1(void); INLINE void prefix##_f2(void); INLINE void prefix##_f3(void); \
  INLINE void prefix##_f4(void); INLINE void prefix##_f5(void); INLINE void prefix##_f6(void); INLINE void prefix##_f7(void); \
  INLINE void prefix##_f8(void); INLINE void prefix##_f9(void); INLINE void prefix##_fa(void); INLINE void prefix##_fb(void); \
  INLINE void prefix##_fc(void); INLINE void prefix##_fd(void); INLINE void prefix##_fe(void); INLINE void prefix##_ff(void); \
static const funcptr tablename[0x100] = {  \
  prefix##_00,prefix##_01,prefix##_02,prefix##_03,prefix##_04,prefix##_05,prefix##_06,prefix##_07, \
  prefix##_08,prefix##_09,prefix##_0a,prefix##_0b,prefix##_0c,prefix##_0d,prefix##_0e,prefix##_0f, \
  prefix##_10,prefix##_11,prefix##_12,prefix##_13,prefix##_14,prefix##_15,prefix##_16,prefix##_17, \
  prefix##_18,prefix##_19,prefix##_1a,prefix##_1b,prefix##_1c,prefix##_1d,prefix##_1e,prefix##_1f, \
  prefix##_20,prefix##_21,prefix##_22,prefix##_23,prefix##_24,prefix##_25,prefix##_26,prefix##_27, \
  prefix##_28,prefix##_29,prefix##_2a,prefix##_2b,prefix##_2c,prefix##_2d,prefix##_2e,prefix##_2f, \
  prefix##_30,prefix##_31,prefix##_32,prefix##_33,prefix##_34,prefix##_35,prefix##_36,prefix##_37, \
  prefix##_38,prefix##_39,prefix##_3a,prefix##_3b,prefix##_3c,prefix##_3d,prefix##_3e,prefix##_3f, \
  prefix##_40,prefix##_41,prefix##_42,prefix##_43,prefix##_44,prefix##_45,prefix##_46,prefix##_47, \
  prefix##_48,prefix##_49,prefix##_4a,prefix##_4b,prefix##_4c,prefix##_4d,prefix##_4e,prefix##_4f, \
  prefix##_50,prefix##_51,prefix##_52,prefix##_53,prefix##_54,prefix##_55,prefix##_56,prefix##_57, \
  prefix##_58,prefix##_59,prefix##_5a,prefix##_5b,prefix##_5c,prefix##_5d,prefix##_5e,prefix##_5f, \
  prefix##_60,prefix##_61,prefix##_62,prefix##_63,prefix##_64,prefix##_65,prefix##_66,prefix##_67, \
  prefix##_68,prefix##_69,prefix##_6a,prefix##_6b,prefix##_6c,prefix##_6d,prefix##_6e,prefix##_6f, \
  prefix##_70,prefix##_71,prefix##_72,prefix##_73,prefix##_74,prefix##_75,prefix##_76,prefix##_77, \
  prefix##_78,prefix##_79,prefix##_7a,prefix##_7b,prefix##_7c,prefix##_7d,prefix##_7e,prefix##_7f, \
  prefix##_80,prefix##_81,prefix##_82,prefix##_83,prefix##_84,prefix##_85,prefix##_86,prefix##_87, \
  prefix##_88,prefix##_89,prefix##_8a,prefix##_8b,prefix##_8c,prefix##_8d,prefix##_8e,prefix##_8f, \
  prefix##_90,prefix##_91,prefix##_92,prefix##_93,prefix##_94,prefix##_95,prefix##_96,prefix##_97, \
  prefix##_98,prefix##_99,prefix##_9a,prefix##_9b,prefix##_9c,prefix##_9d,prefix##_9e,prefix##_9f, \
  prefix##_a0,prefix##_a1,prefix##_a2,prefix##_a3,prefix##_a4,prefix##_a5,prefix##_a6,prefix##_a7, \
  prefix##_a8,prefix##_a9,prefix##_aa,prefix##_ab,prefix##_ac,prefix##_ad,prefix##_ae,prefix##_af, \
  prefix##_b0,prefix##_b1,prefix##_b2,prefix##_b3,prefix##_b4,prefix##_b5,prefix##_b6,prefix##_b7, \
  prefix##_b8,prefix##_b9,prefix##_ba,prefix##_bb,prefix##_bc,prefix##_bd,prefix##_be,prefix##_bf, \
  prefix##_c0,prefix##_c1,prefix##_c2,prefix##_c3,prefix##_c4,prefix##_c5,prefix##_c6,prefix##_c7, \
  prefix##_c8,prefix##_c9,prefix##_ca,prefix##_cb,prefix##_cc,prefix##_cd,prefix##_ce,prefix##_cf, \
  prefix##_d0,prefix##_d1,prefix##_d2,prefix##_d3,prefix##_d4,prefix##_d5,prefix##_d6,prefix##_d7, \
  prefix##_d8,prefix##_d9,prefix##_da,prefix##_db,prefix##_dc,prefix##_dd,prefix##_de,prefix##_df, \
  prefix##_e0,prefix##_e1,prefix##_e2,prefix##_e3,prefix##_e4,prefix##_e5,prefix##_e6,prefix##_e7, \
  prefix##_e8,prefix##_e9,prefix##_ea,prefix##_eb,prefix##_ec,prefix##_ed,prefix##_ee,prefix##_ef, \
  prefix##_f0,prefix##_f1,prefix##_f2,prefix##_f3,prefix##_f4,prefix##_f5,prefix##_f6,prefix##_f7, \
  prefix##_f8,prefix##_f9,prefix##_fa,prefix##_fb,prefix##_fc,prefix##_fd,prefix##_fe,prefix##_ff  \
}

PROTOTYPES(Z80op,op);
PROTOTYPES(Z80cb,cb);
PROTOTYPES(Z80dd,dd);
PROTOTYPES(Z80ed,ed);
PROTOTYPES(Z80fd,fd);
PROTOTYPES(Z80xycb,xycb);

/****************************************************************************/
/* Burn an odd amount of cycles, that is instructions taking something    */
/* different from 4 T-states per opcode (and R increment)          */
/****************************************************************************/
INLINE void BURNODD(int cycles, int opcodes, int cyclesum)
{
  if( cycles > 0 )
  {
    R += (cycles / cyclesum) * opcodes;
    Z80.cycles += (cycles / cyclesum) * cyclesum * 15;
  }
}

/***************************************************************
 * adjust cycle count by n T-states
 ***************************************************************/
#define CC(prefix,opcode) Z80.cycles += cc[Z80_TABLE_##prefix][opcode]

/***************************************************************
 * execute an opcode
 ***************************************************************/
#define EXEC(prefix,opcode)      \
{                                \
  unsigned op = opcode;          \
  CC(prefix,op);                 \
  (*Z80##prefix[op])();          \
}

#if BIG_SWITCH
#define EXEC_INLINE(prefix,opcode)  \
{                                   \
  unsigned op = opcode;             \
  CC(prefix,op);                    \
  switch(op)                        \
  {                                 \
  case 0x00:prefix##_##00();break; case 0x01:prefix##_##01();break; case 0x02:prefix##_##02();break; case 0x03:prefix##_##03();break; \
  case 0x04:prefix##_##04();break; case 0x05:prefix##_##05();break; case 0x06:prefix##_##06();break; case 0x07:prefix##_##07();break; \
  case 0x08:prefix##_##08();break; case 0x09:prefix##_##09();break; case 0x0a:prefix##_##0a();break; case 0x0b:prefix##_##0b();break; \
  case 0x0c:prefix##_##0c();break; case 0x0d:prefix##_##0d();break; case 0x0e:prefix##_##0e();break; case 0x0f:prefix##_##0f();break; \
  case 0x10:prefix##_##10();break; case 0x11:prefix##_##11();break; case 0x12:prefix##_##12();break; case 0x13:prefix##_##13();break; \
  case 0x14:prefix##_##14();break; case 0x15:prefix##_##15();break; case 0x16:prefix##_##16();break; case 0x17:prefix##_##17();break; \
  case 0x18:prefix##_##18();break; case 0x19:prefix##_##19();break; case 0x1a:prefix##_##1a();break; case 0x1b:prefix##_##1b();break; \
  case 0x1c:prefix##_##1c();break; case 0x1d:prefix##_##1d();break; case 0x1e:prefix##_##1e();break; case 0x1f:prefix##_##1f();break; \
  case 0x20:prefix##_##20();break; case 0x21:prefix##_##21();break; case 0x22:prefix##_##22();break; case 0x23:prefix##_##23();break; \
  case 0x24:prefix##_##24();break; case 0x25:prefix##_##25();break; case 0x26:prefix##_##26();break; case 0x27:prefix##_##27();break; \
  case 0x28:prefix##_##28();break; case 0x29:prefix##_##29();break; case 0x2a:prefix##_##2a();break; case 0x2b:prefix##_##2b();break; \
  case 0x2c:prefix##_##2c();break; case 0x2d:prefix##_##2d();break; case 0x2e:prefix##_##2e();break; case 0x2f:prefix##_##2f();break; \
  case 0x30:prefix##_##30();break; case 0x31:prefix##_##31();break; case 0x32:prefix##_##32();break; case 0x33:prefix##_##33();break; \
  case 0x34:prefix##_##34();break; case 0x35:prefix##_##35();break; case 0x36:prefix##_##36();break; case 0x37:prefix##_##37();break; \
  case 0x38:prefix##_##38();break; case 0x39:prefix##_##39();break; case 0x3a:prefix##_##3a();break; case 0x3b:prefix##_##3b();break; \
  case 0x3c:prefix##_##3c();break; case 0x3d:prefix##_##3d();break; case 0x3e:prefix##_##3e();break; case 0x3f:prefix##_##3f();break; \
  case 0x40:prefix##_##40();break; case 0x41:prefix##_##41();break; case 0x42:prefix##_##42();break; case 0x43:prefix##_##43();break; \
  case 0x44:prefix##_##44();break; case 0x45:prefix##_##45();break; case 0x46:prefix##_##46();break; case 0x47:prefix##_##47();break; \
  case 0x48:prefix##_##48();break; case 0x49:prefix##_##49();break; case 0x4a:prefix##_##4a();break; case 0x4b:prefix##_##4b();break; \
  case 0x4c:prefix##_##4c();break; case 0x4d:prefix##_##4d();break; case 0x4e:prefix##_##4e();break; case 0x4f:prefix##_##4f();break; \
  case 0x50:prefix##_##50();break; case 0x51:prefix##_##51();break; case 0x52:prefix##_##52();break; case 0x53:prefix##_##53();break; \
  case 0x54:prefix##_##54();break; case 0x55:prefix##_##55();break; case 0x56:prefix##_##56();break; case 0x57:prefix##_##57();break; \
  case 0x58:prefix##_##58();break; case 0x59:prefix##_##59();break; case 0x5a:prefix##_##5a();break; case 0x5b:prefix##_##5b();break; \
  case 0x5c:prefix##_##5c();break; case 0x5d:prefix##_##5d();break; case 0x5e:prefix##_##5e();break; case 0x5f:prefix##_##5f();break; \
  case 0x60:prefix##_##60();break; case 0x61:prefix##_##61();break; case 0x62:prefix##_##62();break; case 0x63:prefix##_##63();break; \
  case 0x64:prefix##_##64();break; case 0x65:prefix##_##65();break; case 0x66:prefix##_##66();break; case 0x67:prefix##_##67();break; \
  case 0x68:prefix##_##68();break; case 0x69:prefix##_##69();break; case 0x6a:prefix##_##6a();break; case 0x6b:prefix##_##6b();break; \
  case 0x6c:prefix##_##6c();break; case 0x6d:prefix##_##6d();break; case 0x6e:prefix##_##6e();break; case 0x6f:prefix##_##6f();break; \
  case 0x70:prefix##_##70();break; case 0x71:prefix##_##71();break; case 0x72:prefix##_##72();break; case 0x73:prefix##_##73();break; \
  case 0x74:prefix##_##74();break; case 0x75:prefix##_##75();break; case 0x76:prefix##_##76();break; case 0x77:prefix##_##77();break; \
  case 0x78:prefix##_##78();break; case 0x79:prefix##_##79();break; case 0x7a:prefix##_##7a();break; case 0x7b:prefix##_##7b();break; \
  case 0x7c:prefix##_##7c();break; case 0x7d:prefix##_##7d();break; case 0x7e:prefix##_##7e();break; case 0x7f:prefix##_##7f();break; \
  case 0x80:prefix##_##80();break; case 0x81:prefix##_##81();break; case 0x82:prefix##_##82();break; case 0x83:prefix##_##83();break; \
  case 0x84:prefix##_##84();break; case 0x85:prefix##_##85();break; case 0x86:prefix##_##86();break; case 0x87:prefix##_##87();break; \
  case 0x88:prefix##_##88();break; case 0x89:prefix##_##89();break; case 0x8a:prefix##_##8a();break; case 0x8b:prefix##_##8b();break; \
  case 0x8c:prefix##_##8c();break; case 0x8d:prefix##_##8d();break; case 0x8e:prefix##_##8e();break; case 0x8f:prefix##_##8f();break; \
  case 0x90:prefix##_##90();break; case 0x91:prefix##_##91();break; case 0x92:prefix##_##92();break; case 0x93:prefix##_##93();break; \
  case 0x94:prefix##_##94();break; case 0x95:prefix##_##95();break; case 0x96:prefix##_##96();break; case 0x97:prefix##_##97();break; \
  case 0x98:prefix##_##98();break; case 0x99:prefix##_##99();break; case 0x9a:prefix##_##9a();break; case 0x9b:prefix##_##9b();break; \
  case 0x9c:prefix##_##9c();break; case 0x9d:prefix##_##9d();break; case 0x9e:prefix##_##9e();break; case 0x9f:prefix##_##9f();break; \
  case 0xa0:prefix##_##a0();break; case 0xa1:prefix##_##a1();break; case 0xa2:prefix##_##a2();break; case 0xa3:prefix##_##a3();break; \
  case 0xa4:prefix##_##a4();break; case 0xa5:prefix##_##a5();break; case 0xa6:prefix##_##a6();break; case 0xa7:prefix##_##a7();break; \
  case 0xa8:prefix##_##a8();break; case 0xa9:prefix##_##a9();break; case 0xaa:prefix##_##aa();break; case 0xab:prefix##_##ab();break; \
  case 0xac:prefix##_##ac();break; case 0xad:prefix##_##ad();break; case 0xae:prefix##_##ae();break; case 0xaf:prefix##_##af();break; \
  case 0xb0:prefix##_##b0();break; case 0xb1:prefix##_##b1();break; case 0xb2:prefix##_##b2();break; case 0xb3:prefix##_##b3();break; \
  case 0xb4:prefix##_##b4();break; case 0xb5:prefix##_##b5();break; case 0xb6:prefix##_##b6();break; case 0xb7:prefix##_##b7();break; \
  case 0xb8:prefix##_##b8();break; case 0xb9:prefix##_##b9();break; case 0xba:prefix##_##ba();break; case 0xbb:prefix##_##bb();break; \
  case 0xbc:prefix##_##bc();break; case 0xbd:prefix##_##bd();break; case 0xbe:prefix##_##be();break; case 0xbf:prefix##_##bf();break; \
  case 0xc0:prefix##_##c0();break; case 0xc1:prefix##_##c1();break; case 0xc2:prefix##_##c2();break; case 0xc3:prefix##_##c3();break; \
  case 0xc4:prefix##_##c4();break; case 0xc5:prefix##_##c5();break; case 0xc6:prefix##_##c6();break; case 0xc7:prefix##_##c7();break; \
  case 0xc8:prefix##_##c8();break; case 0xc9:prefix##_##c9();break; case 0xca:prefix##_##ca();break; case 0xcb:prefix##_##cb();break; \
  case 0xcc:prefix##_##cc();break; case 0xcd:prefix##_##cd();break; case 0xce:prefix##_##ce();break; case 0xcf:prefix##_##cf();break; \
  case 0xd0:prefix##_##d0();break; case 0xd1:prefix##_##d1();break; case 0xd2:prefix##_##d2();break; case 0xd3:prefix##_##d3();break; \
  case 0xd4:prefix##_##d4();break; case 0xd5:prefix##_##d5();break; case 0xd6:prefix##_##d6();break; case 0xd7:prefix##_##d7();break; \
  case 0xd8:prefix##_##d8();break; case 0xd9:prefix##_##d9();break; case 0xda:prefix##_##da();break; case 0xdb:prefix##_##db();break; \
  case 0xdc:prefix##_##dc();break; case 0xdd:prefix##_##dd();break; case 0xde:prefix##_##de();break; case 0xdf:prefix##_##df();break; \
  case 0xe0:prefix##_##e0();break; case 0xe1:prefix##_##e1();break; case 0xe2:prefix##_##e2();break; case 0xe3:prefix##_##e3();break; \
  case 0xe4:prefix##_##e4();break; case 0xe5:prefix##_##e5();break; case 0xe6:prefix##_##e6();break; case 0xe7:prefix##_##e7();break; \
  case 0xe8:prefix##_##e8();break; case 0xe9:prefix##_##e9();break; case 0xea:prefix##_##ea();break; case 0xeb:prefix##_##eb();break; \
  case 0xec:prefix##_##ec();break; case 0xed:prefix##_##ed();break; case 0xee:prefix##_##ee();break; case 0xef:prefix##_##ef();break; \
  case 0xf0:prefix##_##f0();break; case 0xf1:prefix##_##f1();break; case 0xf2:prefix##_##f2();break; case 0xf3:prefix##_##f3();break; \
  case 0xf4:prefix##_##f4();break; case 0xf5:prefix##_##f5();break; case 0xf6:prefix##_##f6();break; case 0xf7:prefix##_##f7();break; \
  case 0xf8:prefix##_##f8();break; case 0xf9:prefix##_##f9();break; case 0xfa:prefix##_##fa();break; case 0xfb:prefix##_##fb();break; \
  case 0xfc:prefix##_##fc();break; case 0xfd:prefix##_##fd();break; case 0xfe:prefix##_##fe();break; case 0xff:prefix##_##ff();break; \
  }                                                                                                                                   \
}
#else
#define EXEC_INLINE EXEC
#endif


/***************************************************************
 * Enter HALT state; write 1 to fake port on first execution
 ***************************************************************/
#define ENTER_HALT {                          \
  PC--;                                       \
  HALT = 1;                                   \
}

/***************************************************************
 * Leave HALT state; write 0 to fake port
 ***************************************************************/
#define LEAVE_HALT {                          \
  if( HALT )                                  \
  {                                           \
    HALT = 0;                                 \
    PC++;                                     \
  }                                           \
}

/***************************************************************
 * Input a byte from given I/O port
 ***************************************************************/
#define IN(port) z80_readport(port)

/***************************************************************
 * Output a byte to given I/O port
 ***************************************************************/
#define OUT(port,value) z80_writeport(port,value)

/***************************************************************
 * Read a byte from given memory location
 ***************************************************************/
#define RM(addr) z80_readmem(addr)

/***************************************************************
 * Write a byte to given memory location
 ***************************************************************/
#define WM(addr,value) z80_writemem(addr,value)

/***************************************************************
 * Read a word from given memory location
 ***************************************************************/
INLINE void RM16( UINT32 addr, PAIR *r )
{
  r->b.l = RM(addr);
  r->b.h = RM((addr+1)&0xffff);
}

/***************************************************************
 * Write a word to given memory location
 ***************************************************************/
INLINE void WM16( UINT32 addr, PAIR *r )
{
  WM(addr,r->b.l);
  WM((addr+1)&0xffff,r->b.h);
}

/***************************************************************
 * ROP() is identical to RM() except it is used for
 * reading opcodes. In case of system with memory mapped I/O,
 * this function can be used to greatly speed up emulation
 ***************************************************************/
INLINE UINT8 ROP(void)
{
  unsigned pc = PCD;
  PC++;
  return cpu_readop(pc);
}

/****************************************************************
 * ARG() is identical to ROP() except it is used
 * for reading opcode arguments. This difference can be used to
 * support systems that use different encoding mechanisms for
 * opcodes and opcode arguments
 ***************************************************************/
INLINE UINT8 ARG(void)
{
  unsigned pc = PCD;
  PC++;
  return cpu_readop_arg(pc);
}

INLINE UINT32 ARG16(void)
{
  unsigned pc = PCD;
  PC += 2;
  return cpu_readop_arg(pc) | (cpu_readop_arg((pc+1)&0xffff) << 8);
}

/***************************************************************
 * Calculate the effective address EA of an opcode using
 * IX+offset resp. IY+offset addressing.
 ***************************************************************/
#define EAX   do { EA = (UINT32)(UINT16)(IX + (INT8)ARG()); WZ = EA; } while (0)
#define EAY   do { EA = (UINT32)(UINT16)(IY + (INT8)ARG()); WZ = EA; } while (0)

/***************************************************************
 * POP
 ***************************************************************/
#define POP(DR) do { RM16( SPD, &Z80.DR ); SP += 2; } while (0)

/***************************************************************
 * PUSH
 ***************************************************************/
#define PUSH(SR) do { SP -= 2; WM16( SPD, &Z80.SR ); } while (0)

/***************************************************************
 * JP
 ***************************************************************/
#define JP {                                    \
  PCD = ARG16();                                \
  WZ = PCD;                                     \
}

/***************************************************************
 * JP_COND
 ***************************************************************/
#define JP_COND(cond) {                         \
  if (cond)                                     \
  {                                             \
    PCD = ARG16();                              \
    WZ = PCD;                                   \
  }                                             \
  else                                          \
  {                                             \
    WZ = ARG16(); /* implicit do PC += 2 */     \
  }                                             \
}

/***************************************************************
 * JR
 ***************************************************************/
#define JR() {                                            \
  INT8 arg = (INT8)ARG(); /* ARG() also increments PC */  \
  PC += arg;        /* so don't do PC += ARG() */         \
  WZ = PC;                                                \
}

/***************************************************************
 * JR_COND
 ***************************************************************/
#define JR_COND(cond, opcode) {   \
  if (cond)                       \
  {                               \
    JR();                         \
    CC(ex, opcode);               \
  }                               \
  else PC++;                      \
}

/***************************************************************
 * CALL
 ***************************************************************/
#define CALL() {                  \
  EA = ARG16();                   \
  WZ = EA;                        \
  PUSH(pc);                       \
  PCD = EA;                       \
}

/***************************************************************
 * CALL_COND
 ***************************************************************/
#define CALL_COND(cond, opcode) { \
  if (cond)                       \
  {                               \
    EA = ARG16();                 \
    WZ = EA;                      \
    PUSH(pc);                     \
    PCD = EA;                     \
    CC(ex, opcode);               \
  }                               \
  else                            \
  {                               \
    WZ = ARG16();  /* implicit call PC+=2;   */ \
  }                               \
}

/***************************************************************
 * RET_COND
 ***************************************************************/
#define RET_COND(cond, opcode) do { \
  if (cond)                         \
  {                                 \
    POP(pc);                        \
    WZ = PC;                        \
    CC(ex, opcode);                 \
  }                                 \
} while (0)

/***************************************************************
 * RETN
 ***************************************************************/
#define RETN do { \
  LOG(("Z80 #%d RETN IFF1:%d IFF2:%d\n", cpu_getactivecpu(), IFF1, IFF2)); \
  POP( pc ); \
  WZ = PC; \
  IFF1 = IFF2; \
} while (0)

/***************************************************************
 * RETI
 ***************************************************************/
#define RETI { \
  POP( pc ); \
  WZ = PC; \
/* according to http://www.msxnet.org/tech/z80-documented.pdf */ \
  IFF1 = IFF2; \
}

/***************************************************************
 * LD  R,A
 ***************************************************************/
#define LD_R_A {  \
  R = A;  \
  R2 = A & 0x80;  /* keep bit 7 of R */ \
}

/***************************************************************
 * LD  A,R
 ***************************************************************/
#define LD_A_R {  \
  A = (R & 0x7f) | R2;  \
  F = (F & CF) | SZ[A] | ( IFF2 << 2 ); \
}

/***************************************************************
 * LD  I,A
 ***************************************************************/
#define LD_I_A {  \
  I = A;  \
}

/***************************************************************
 * LD  A,I
 ***************************************************************/
#define LD_A_I {  \
  A = I;  \
  F = (F & CF) | SZ[A] | ( IFF2 << 2 ); \
}

/***************************************************************
 * RST
 ***************************************************************/
#define RST(addr) \
  PUSH( pc ); \
  PCD = addr; \
  WZ = PC;  \

/***************************************************************
 * INC  r8
 ***************************************************************/
INLINE UINT8 INC(UINT8 value)
{
  UINT8 res = value + 1;
  F = (F & CF) | SZHV_inc[res];
  return (UINT8)res;
}

/***************************************************************
 * DEC  r8
 ***************************************************************/
INLINE UINT8 DEC(UINT8 value)
{
  UINT8 res = value - 1;
  F = (F & CF) | SZHV_dec[res];
  return res;
}

/***************************************************************
 * RLCA
 ***************************************************************/
#define RLCA                                        \
  A = (A << 1) | (A >> 7);                          \
  F = (F & (SF | ZF | PF)) | (A & (YF | XF | CF))

/***************************************************************
 * RRCA
 ***************************************************************/
#define RRCA                                        \
  F = (F & (SF | ZF | PF)) | (A & CF);              \
  A = (A >> 1) | (A << 7);                          \
  F |= (A & (YF | XF) )

/***************************************************************
 * RLA
 ***************************************************************/
#define RLA {                                       \
  UINT8 res = (A << 1) | (F & CF);                  \
  UINT8 c = (A & 0x80) ? CF : 0;                    \
  F = (F & (SF | ZF | PF)) | c | (res & (YF | XF)); \
  A = res;                                          \
}

/***************************************************************
 * RRA
 ***************************************************************/
#define RRA {                                       \
  UINT8 res = (A >> 1) | (F << 7);                  \
  UINT8 c = (A & 0x01) ? CF : 0;                    \
  F = (F & (SF | ZF | PF)) | c | (res & (YF | XF)); \
  A = res;                                          \
}

/***************************************************************
 * RRD
 ***************************************************************/
#define RRD {                                       \
  UINT8 n = RM(HL);                                 \
  WZ = HL+1;                                        \
  WM( HL, (n >> 4) | (A << 4) );                    \
  A = (A & 0xf0) | (n & 0x0f);                      \
  F = (F & CF) | SZP[A];                            \
}

/***************************************************************
 * RLD
 ***************************************************************/
#define RLD {                                       \
  UINT8 n = RM(HL);                                 \
  WZ = HL+1;                                        \
  WM( HL, (n << 4) | (A & 0x0f) );                  \
  A = (A & 0xf0) | (n >> 4);                        \
  F = (F & CF) | SZP[A];                            \
}

/***************************************************************
 * ADD  A,n
 ***************************************************************/
#define ADD(value)                                  \
{                                                   \
  UINT32 ah = AFD & 0xff00;                         \
  UINT32 res = (UINT8)((ah >> 8) + value);          \
  F = SZHVC_add[ah | res];                          \
  A = res;                                          \
}

/***************************************************************
 * ADC  A,n
 ***************************************************************/
#define ADC(value)                                  \
{                                                   \
  UINT32 ah = AFD & 0xff00, c = AFD & 1;            \
  UINT32 res = (UINT8)((ah >> 8) + value + c);      \
  F = SZHVC_add[(c << 16) | ah | res];              \
  A = res;                                          \
}

/***************************************************************
 * SUB  n
 ***************************************************************/
#define SUB(value)                                  \
{                                                   \
  UINT32 ah = AFD & 0xff00;                         \
  UINT32 res = (UINT8)((ah >> 8) - value);          \
  F = SZHVC_sub[ah | res];                          \
  A = res;                                          \
}

/***************************************************************
 * SBC  A,n
 ***************************************************************/
#define SBC(value)                                  \
{                                                   \
  UINT32 ah = AFD & 0xff00, c = AFD & 1;            \
  UINT32 res = (UINT8)((ah >> 8) - value - c);      \
  F = SZHVC_sub[(c<<16) | ah | res];                \
  A = res;                                          \
}

/***************************************************************
 * NEG
 ***************************************************************/
#define NEG {                                       \
  UINT8 value = A;                                  \
  A = 0;                                            \
  SUB(value);                                       \
}

/***************************************************************
 * DAA
 ***************************************************************/
#define DAA {                                       \
  UINT8 a = A;                                      \
  if (F & NF) {                                     \
    if ((F&HF) | ((A&0xf)>9)) a-=6;                 \
    if ((F&CF) | (A>0x99)) a-=0x60;                 \
  }                                                 \
  else {                                            \
    if ((F&HF) | ((A&0xf)>9)) a+=6;                 \
    if ((F&CF) | (A>0x99)) a+=0x60;                 \
  }                                                 \
                                                    \
  F = (F&(CF|NF)) | (A>0x99) | ((A^a)&HF) | SZP[a]; \
  A = a;                                            \
}

/***************************************************************
 * AND  n
 ***************************************************************/
#define AND(value)  \
  A &= value;       \
  F = SZP[A] | HF

/***************************************************************
 * OR  n
 ***************************************************************/
#define OR(value)   \
  A |= value;       \
  F = SZP[A]

/***************************************************************
 * XOR  n
 ***************************************************************/
#define XOR(value)  \
  A ^= value;       \
  F = SZP[A]

/***************************************************************
 * CP  n
 ***************************************************************/
#define CP(value)                                             \
{                                                             \
  unsigned val = value;                                       \
  UINT32 ah = AFD & 0xff00;                                   \
  UINT32 res = (UINT8)((ah >> 8) - val);                      \
  F = (SZHVC_sub[ah | res] & ~(YF | XF)) | (val & (YF | XF)); \
}

/***************************************************************
 * EX  AF,AF'
 ***************************************************************/
#define EX_AF                                       \
{                                                   \
  PAIR tmp;                                         \
  tmp = Z80.af; Z80.af = Z80.af2; Z80.af2 = tmp;    \
}

/***************************************************************
 * EX  DE,HL
 ***************************************************************/
void EX_DE_HL()
{
  PAIR tmp;
  tmp = Z80.de; Z80.de = Z80.hl; Z80.hl = tmp;
}

/***************************************************************
 * EXX
 ***************************************************************/
void EXX()
{
  PAIR tmp;
  tmp = Z80.bc; Z80.bc = Z80.bc2; Z80.bc2 = tmp;
  tmp = Z80.de; Z80.de = Z80.de2; Z80.de2 = tmp;
  tmp = Z80.hl; Z80.hl = Z80.hl2; Z80.hl2 = tmp;
}

/***************************************************************
 * EX  (SP),r16
 ***************************************************************/
#define EXSP(DR)                                    \
{                                                   \
  PAIR tmp = { { 0, 0, 0, 0 } };                    \
  RM16( SPD, &tmp );                                \
  WM16( SPD, &Z80.DR );                             \
  Z80.DR = tmp;                                     \
  WZ = Z80.DR.d;                                    \
}


/***************************************************************
 * ADD16
 ***************************************************************/
#define ADD16(DR,SR)                                \
{                                                   \
  UINT32 res = Z80.DR.d + Z80.SR.d;                 \
  WZ = Z80.DR.d + 1;                                \
  F = (F & (SF | ZF | VF)) |                        \
    (((Z80.DR.d ^ res ^ Z80.SR.d) >> 8) & HF) |     \
    ((res >> 16) & CF) | ((res >> 8) & (YF | XF));  \
  Z80.DR.w.l = (UINT16)res;                         \
}

/***************************************************************
 * ADC  r16,r16
 ***************************************************************/
#define ADC16(Reg)                                                      \
{                                                                       \
  UINT32 res = HLD + Z80.Reg.d + (F & CF);                              \
  WZ = HL + 1;                                                          \
  F = (((HLD ^ res ^ Z80.Reg.d) >> 8) & HF) |                           \
    ((res >> 16) & CF) |                                                \
    ((res >> 8) & (SF | YF | XF)) |                                     \
    ((res & 0xffff) ? 0 : ZF) |                                         \
    (((Z80.Reg.d ^ HLD ^ 0x8000) & (Z80.Reg.d ^ res) & 0x8000) >> 13);  \
  HL = (UINT16)res;                                                     \
}

/***************************************************************
 * SBC  r16,r16
 ***************************************************************/
#define SBC16(Reg)                                      \
{                                                       \
  UINT32 res = HLD - Z80.Reg.d - (F & CF);              \
  WZ = HL + 1;                                          \
  F = (((HLD ^ res ^ Z80.Reg.d) >> 8) & HF) | NF |      \
    ((res >> 16) & CF) |                                \
    ((res >> 8) & (SF | YF | XF)) |                     \
    ((res & 0xffff) ? 0 : ZF) |                         \
    (((Z80.Reg.d ^ HLD) & (HLD ^ res) &0x8000) >> 13);  \
  HL = (UINT16)res;                                     \
}

/***************************************************************
 * RLC  r8
 ***************************************************************/
INLINE UINT8 RLC(UINT8 value)
{
  unsigned res = value;
  unsigned c = (res & 0x80) ? CF : 0;
  res = ((res << 1) | (res >> 7)) & 0xff;
  F = SZP[res] | c;
  return res;
}

/***************************************************************
 * RRC  r8
 ***************************************************************/
INLINE UINT8 RRC(UINT8 value)
{
  unsigned res = value;
  unsigned c = (res & 0x01) ? CF : 0;
  res = ((res >> 1) | (res << 7)) & 0xff;
  F = SZP[res] | c;
  return res;
}

/***************************************************************
 * RL  r8
 ***************************************************************/
INLINE UINT8 RL(UINT8 value)
{
  unsigned res = value;
  unsigned c = (res & 0x80) ? CF : 0;
  res = ((res << 1) | (F & CF)) & 0xff;
  F = SZP[res] | c;
  return res;
}

/***************************************************************
 * RR  r8
 ***************************************************************/
INLINE UINT8 RR(UINT8 value)
{
  unsigned res = value;
  unsigned c = (res & 0x01) ? CF : 0;
  res = ((res >> 1) | (F << 7)) & 0xff;
  F = SZP[res] | c;
  return res;
}

/***************************************************************
 * SLA  r8
 ***************************************************************/
INLINE UINT8 SLA(UINT8 value)
{
  unsigned res = value;
  unsigned c = (res & 0x80) ? CF : 0;
  res = (res << 1) & 0xff;
  F = SZP[res] | c;
  return res;
}

/***************************************************************
 * SRA  r8
 ***************************************************************/
INLINE UINT8 SRA(UINT8 value)
{
  unsigned res = value;
  unsigned c = (res & 0x01) ? CF : 0;
  res = ((res >> 1) | (res & 0x80)) & 0xff;
  F = SZP[res] | c;
  return res;
}

/***************************************************************
 * SLL  r8
 ***************************************************************/
INLINE UINT8 SLL(UINT8 value)
{
  unsigned res = value;
  unsigned c = (res & 0x80) ? CF : 0;
  res = ((res << 1) | 0x01) & 0xff;
  F = SZP[res] | c;
  return res;
}

/***************************************************************
 * SRL  r8
 ***************************************************************/
INLINE UINT8 SRL(UINT8 value)
{
  unsigned res = value;
  unsigned c = (res & 0x01) ? CF : 0;
  res = (res >> 1) & 0xff;
  F = SZP[res] | c;
  return res;
}

/***************************************************************
 * BIT  bit,r8
 ***************************************************************/
#undef BIT
#define BIT(bit,reg)    \
  F = (F & CF) | HF | (SZ_BIT[reg & (1<<bit)] & ~(YF|XF)) | (reg & (YF|XF))

/***************************************************************
 * BIT  bit,(HL)
 ***************************************************************/
#define BIT_HL(bit,reg) \
  F = (F & CF) | HF | (SZ_BIT[reg & (1<<bit)] & ~(YF|XF)) | (WZ_H & (YF|XF))

/***************************************************************
 * BIT  bit,(IX/Y+o)
 ***************************************************************/
#define BIT_XY(bit,reg) \
  F = (F & CF) | HF | (SZ_BIT[reg & (1<<bit)] & ~(YF|XF)) | ((EA>>8) & (YF|XF))

/***************************************************************
 * RES  bit,r8
 ***************************************************************/
INLINE UINT8 RES(UINT8 bit, UINT8 value)
{
  return value & ~(1<<bit);
}

/***************************************************************
 * SET  bit,r8
 ***************************************************************/
INLINE UINT8 SET(UINT8 bit, UINT8 value)
{
  return value | (1<<bit);
}

/***************************************************************
 * LDI
 ***************************************************************/
#define LDI {                                           \
  UINT8 io = RM(HL);                                    \
  WM( DE, io );                                         \
  F &= SF | ZF | CF;                                    \
  if( (A + io) & 0x02 ) F |= YF; /* bit 1 -> flag 5 */  \
  if( (A + io) & 0x08 ) F |= XF; /* bit 3 -> flag 3 */  \
  HL++; DE++; BC--;                                     \
  if( BC ) F |= VF;                                     \
}

/***************************************************************
 * CPI
 ***************************************************************/
#define CPI {                                                 \
  UINT8 val = RM(HL);                                         \
  UINT8 res = A - val;                                        \
  WZ++;                                                       \
  HL++; BC--;                                                 \
  F = (F & CF) | (SZ[res]&~(YF|XF)) | ((A^val^res)&HF) | NF;  \
  if( F & HF ) res -= 1;                                      \
  if( res & 0x02 ) F |= YF; /* bit 1 -> flag 5 */             \
  if( res & 0x08 ) F |= XF; /* bit 3 -> flag 3 */             \
  if( BC ) F |= VF;                                           \
}

/***************************************************************
 * INI
 ***************************************************************/
#define INI {                                           \
  unsigned t;                                           \
  UINT8 io = IN(BC);                                    \
  WZ = BC + 1;                                          \
  CC(ex,0xa2);                                          \
  B--;                                                  \
  WM( HL, io );                                         \
  HL++;                                                 \
  F = SZ[B];                                            \
  t = (unsigned)((C + 1) & 0xff) + (unsigned)io;        \
  if( io & SF ) F |= NF;                                \
  if( t & 0x100 ) F |= HF | CF;                         \
  F |= SZP[(UINT8)(t & 0x07) ^ B] & PF;                 \
}

/***************************************************************
 * OUTI
 ***************************************************************/
#define OUTI {                                          \
  unsigned t;                                           \
  UINT8 io = RM(HL);                                    \
  B--;                                                  \
  WZ = BC + 1;                                          \
  OUT( BC, io );                                        \
  HL++;                                                 \
  F = SZ[B];                                            \
  t = (unsigned)L + (unsigned)io;                       \
  if( io & SF ) F |= NF;                                \
  if( t & 0x100 ) F |= HF | CF;                         \
  F |= SZP[(UINT8)(t & 0x07) ^ B] & PF;                 \
}

/***************************************************************
 * LDD
 ***************************************************************/
#define LDD {                                           \
  UINT8 io = RM(HL);                                    \
  WM( DE, io );                                         \
  F &= SF | ZF | CF;                                    \
  if( (A + io) & 0x02 ) F |= YF; /* bit 1 -> flag 5 */  \
  if( (A + io) & 0x08 ) F |= XF; /* bit 3 -> flag 3 */  \
  HL--; DE--; BC--;                                     \
  if( BC ) F |= VF;                                     \
}

/***************************************************************
 * CPD
 ***************************************************************/
#define CPD {                                                 \
  UINT8 val = RM(HL);                                         \
  UINT8 res = A - val;                                        \
  WZ--;                                                       \
  HL--; BC--;                                                 \
  F = (F & CF) | (SZ[res]&~(YF|XF)) | ((A^val^res)&HF) | NF;  \
  if( F & HF ) res -= 1;                                      \
  if( res & 0x02 ) F |= YF; /* bit 1 -> flag 5 */             \
  if( res & 0x08 ) F |= XF; /* bit 3 -> flag 3 */             \
  if( BC ) F |= VF;                                           \
}

/***************************************************************
 * IND
 ***************************************************************/
#define IND {                                           \
  unsigned t;                                           \
  UINT8 io = IN(BC);                                    \
  WZ = BC - 1;                                          \
  CC(ex,0xaa);                                          \
  B--;                                                  \
  WM( HL, io );                                         \
  HL--;                                                 \
  F = SZ[B];                                            \
  t = ((unsigned)(C - 1) & 0xff) + (unsigned)io;        \
  if( io & SF ) F |= NF;                                \
  if( t & 0x100 ) F |= HF | CF;                         \
  F |= SZP[(UINT8)(t & 0x07) ^ B] & PF;                 \
}

/***************************************************************
 * OUTD
 ***************************************************************/
#define OUTD {                                          \
  unsigned t;                                           \
  UINT8 io = RM(HL);                                    \
  B--;                                                  \
  WZ = BC - 1;                                          \
  OUT( BC, io );                                        \
  HL--;                                                 \
  F = SZ[B];                                            \
  t = (unsigned)L + (unsigned)io;                       \
  if( io & SF ) F |= NF;                                \
  if( t & 0x100 ) F |= HF | CF;                         \
  F |= SZP[(UINT8)(t & 0x07) ^ B] & PF;                 \
}

/***************************************************************
 * LDIR
 ***************************************************************/
#define LDIR            \
  LDI;                  \
  if( BC )              \
  {                     \
    PC -= 2;            \
    WZ = PC + 1;        \
    CC(ex,0xb0);        \
  }

/***************************************************************
 * CPIR
 ***************************************************************/
#define CPIR            \
  CPI;                  \
  if( BC && !(F & ZF) ) \
  {                     \
    PC -= 2;            \
   WZ = PC + 1;         \
    CC(ex,0xb1);        \
  }

/***************************************************************
 * INIR
 ***************************************************************/
#define INIR      \
  INI;            \
  if( B )         \
  {               \
    PC -= 2;      \
    CC(ex,0xb2);  \
  }

/***************************************************************
 * OTIR
 ***************************************************************/
void OTIR() {
  OUTI;
  if( B )
  {
    PC -= 2;
    CC(ex,0xb3);
  }
}

/***************************************************************
 * LDDR
 ***************************************************************/
void LDDR() {
  LDD;
  if( BC )
  {
    PC -= 2;
    WZ = PC + 1;
    CC(ex,0xb8);
  }
}

/***************************************************************
 * CPDR
 ***************************************************************/
void CPDR() {
  CPD;
  if( BC && !(F & ZF) )
  {
    PC -= 2;
   WZ = PC + 1;
    CC(ex,0xb9);
  }
}

/***************************************************************
 * INDR
 ***************************************************************/
void INDR() {
  IND;
  if( B )
  {
    PC -= 2;
    CC(ex,0xba);
  }
}

/***************************************************************
 * OTDR
 ***************************************************************/
void OTDR() {
  OUTD;
  if( B )
  {
    PC -= 2;
    CC(ex,0xbb);
  }
}

/***************************************************************
 * EI
 ***************************************************************/
void EI() {
  IFF1 = IFF2 = 1;
  Z80.after_ei = TRUE;
}

/**********************************************************
 * opcodes with CB prefix
 * rotate, shift and bit operations
 **********************************************************/
INLINE void cb_00(void) { B = RLC(B);                      } /* RLC  B           */
INLINE void cb_01(void) { C = RLC(C);                      } /* RLC  C           */
INLINE void cb_02(void) { D = RLC(D);                      } /* RLC  D           */
INLINE void cb_03(void) { E = RLC(E);                      } /* RLC  E           */
INLINE void cb_04(void) { H = RLC(H);                      } /* RLC  H           */
INLINE void cb_05(void) { L = RLC(L);                      } /* RLC  L           */
INLINE void cb_06(void) { WM( HL, RLC(RM(HL)) );           } /* RLC  (HL)        */
INLINE void cb_07(void) { A = RLC(A);                      } /* RLC  A           */

INLINE void cb_08(void) { B = RRC(B);                      } /* RRC  B           */
INLINE void cb_09(void) { C = RRC(C);                      } /* RRC  C           */
INLINE void cb_0a(void) { D = RRC(D);                      } /* RRC  D           */
INLINE void cb_0b(void) { E = RRC(E);                      } /* RRC  E           */
INLINE void cb_0c(void) { H = RRC(H);                      } /* RRC  H           */
INLINE void cb_0d(void) { L = RRC(L);                      } /* RRC  L           */
INLINE void cb_0e(void) { WM( HL, RRC(RM(HL)) );           } /* RRC  (HL)        */
INLINE void cb_0f(void) { A = RRC(A);                      } /* RRC  A           */

INLINE void cb_10(void) { B = RL(B);                       } /* RL   B           */
INLINE void cb_11(void) { C = RL(C);                       } /* RL   C           */
INLINE void cb_12(void) { D = RL(D);                       } /* RL   D           */
INLINE void cb_13(void) { E = RL(E);                       } /* RL   E           */
INLINE void cb_14(void) { H = RL(H);                       } /* RL   H           */
INLINE void cb_15(void) { L = RL(L);                       } /* RL   L           */
INLINE void cb_16(void) { WM( HL, RL(RM(HL)) );            } /* RL   (HL)        */
INLINE void cb_17(void) { A = RL(A);                       } /* RL   A           */

INLINE void cb_18(void) { B = RR(B);                       } /* RR   B           */
INLINE void cb_19(void) { C = RR(C);                       } /* RR   C           */
INLINE void cb_1a(void) { D = RR(D);                       } /* RR   D           */
INLINE void cb_1b(void) { E = RR(E);                       } /* RR   E           */
INLINE void cb_1c(void) { H = RR(H);                       } /* RR   H           */
INLINE void cb_1d(void) { L = RR(L);                       } /* RR   L           */
INLINE void cb_1e(void) { WM( HL, RR(RM(HL)) );            } /* RR   (HL)        */
INLINE void cb_1f(void) { A = RR(A);                       } /* RR   A           */

INLINE void cb_20(void) { B = SLA(B);                      } /* SLA  B           */
INLINE void cb_21(void) { C = SLA(C);                      } /* SLA  C           */
INLINE void cb_22(void) { D = SLA(D);                      } /* SLA  D           */
INLINE void cb_23(void) { E = SLA(E);                      } /* SLA  E           */
INLINE void cb_24(void) { H = SLA(H);                      } /* SLA  H           */
INLINE void cb_25(void) { L = SLA(L);                      } /* SLA  L           */
INLINE void cb_26(void) { WM( HL, SLA(RM(HL)) );           } /* SLA  (HL)        */
INLINE void cb_27(void) { A = SLA(A);                      } /* SLA  A           */

INLINE void cb_28(void) { B = SRA(B);                      } /* SRA  B           */
INLINE void cb_29(void) { C = SRA(C);                      } /* SRA  C           */
INLINE void cb_2a(void) { D = SRA(D);                      } /* SRA  D           */
INLINE void cb_2b(void) { E = SRA(E);                      } /* SRA  E           */
INLINE void cb_2c(void) { H = SRA(H);                      } /* SRA  H           */
INLINE void cb_2d(void) { L = SRA(L);                      } /* SRA  L           */
INLINE void cb_2e(void) { WM( HL, SRA(RM(HL)) );           } /* SRA  (HL)        */
INLINE void cb_2f(void) { A = SRA(A);                      } /* SRA  A           */

INLINE void cb_30(void) { B = SLL(B);                      } /* SLL  B           */
INLINE void cb_31(void) { C = SLL(C);                      } /* SLL  C           */
INLINE void cb_32(void) { D = SLL(D);                      } /* SLL  D           */
INLINE void cb_33(void) { E = SLL(E);                      } /* SLL  E           */
INLINE void cb_34(void) { H = SLL(H);                      } /* SLL  H           */
INLINE void cb_35(void) { L = SLL(L);                      } /* SLL  L           */
INLINE void cb_36(void) { WM( HL, SLL(RM(HL)) );           } /* SLL  (HL)        */
INLINE void cb_37(void) { A = SLL(A);                      } /* SLL  A           */

INLINE void cb_38(void) { B = SRL(B);                      } /* SRL  B           */
INLINE void cb_39(void) { C = SRL(C);                      } /* SRL  C           */
INLINE void cb_3a(void) { D = SRL(D);                      } /* SRL  D           */
INLINE void cb_3b(void) { E = SRL(E);                      } /* SRL  E           */
INLINE void cb_3c(void) { H = SRL(H);                      } /* SRL  H           */
INLINE void cb_3d(void) { L = SRL(L);                      } /* SRL  L           */
INLINE void cb_3e(void) { WM( HL, SRL(RM(HL)) );           } /* SRL  (HL)        */
INLINE void cb_3f(void) { A = SRL(A);                      } /* SRL  A           */

INLINE void cb_40(void) { BIT(0,B);                        } /* BIT  0,B         */
INLINE void cb_41(void) { BIT(0,C);                        } /* BIT  0,C         */
INLINE void cb_42(void) { BIT(0,D);                        } /* BIT  0,D         */
INLINE void cb_43(void) { BIT(0,E);                        } /* BIT  0,E         */
INLINE void cb_44(void) { BIT(0,H);                        } /* BIT  0,H         */
INLINE void cb_45(void) { BIT(0,L);                        } /* BIT  0,L         */
INLINE void cb_46(void) { BIT_HL(0,RM(HL));                } /* BIT  0,(HL)      */
INLINE void cb_47(void) { BIT(0,A);                        } /* BIT  0,A         */

INLINE void cb_48(void) { BIT(1,B);                        } /* BIT  1,B         */
INLINE void cb_49(void) { BIT(1,C);                        } /* BIT  1,C         */
INLINE void cb_4a(void) { BIT(1,D);                        } /* BIT  1,D         */
INLINE void cb_4b(void) { BIT(1,E);                        } /* BIT  1,E         */
INLINE void cb_4c(void) { BIT(1,H);                        } /* BIT  1,H         */
INLINE void cb_4d(void) { BIT(1,L);                        } /* BIT  1,L         */
INLINE void cb_4e(void) { BIT_HL(1,RM(HL));                } /* BIT  1,(HL)      */
INLINE void cb_4f(void) { BIT(1,A);                        } /* BIT  1,A         */

INLINE void cb_50(void) { BIT(2,B);                        } /* BIT  2,B         */
INLINE void cb_51(void) { BIT(2,C);                        } /* BIT  2,C         */
INLINE void cb_52(void) { BIT(2,D);                        } /* BIT  2,D         */
INLINE void cb_53(void) { BIT(2,E);                        } /* BIT  2,E         */
INLINE void cb_54(void) { BIT(2,H);                        } /* BIT  2,H         */
INLINE void cb_55(void) { BIT(2,L);                        } /* BIT  2,L         */
INLINE void cb_56(void) { BIT_HL(2,RM(HL));                } /* BIT  2,(HL)      */
INLINE void cb_57(void) { BIT(2,A);                        } /* BIT  2,A         */

INLINE void cb_58(void) { BIT(3,B);                        } /* BIT  3,B         */
INLINE void cb_59(void) { BIT(3,C);                        } /* BIT  3,C         */
INLINE void cb_5a(void) { BIT(3,D);                        } /* BIT  3,D         */
INLINE void cb_5b(void) { BIT(3,E);                        } /* BIT  3,E         */
INLINE void cb_5c(void) { BIT(3,H);                        } /* BIT  3,H         */
INLINE void cb_5d(void) { BIT(3,L);                        } /* BIT  3,L         */
INLINE void cb_5e(void) { BIT_HL(3,RM(HL));                } /* BIT  3,(HL)      */
INLINE void cb_5f(void) { BIT(3,A);                        } /* BIT  3,A         */

INLINE void cb_60(void) { BIT(4,B);                        } /* BIT  4,B         */
INLINE void cb_61(void) { BIT(4,C);                        } /* BIT  4,C         */
INLINE void cb_62(void) { BIT(4,D);                        } /* BIT  4,D         */
INLINE void cb_63(void) { BIT(4,E);                        } /* BIT  4,E         */
INLINE void cb_64(void) { BIT(4,H);                        } /* BIT  4,H         */
INLINE void cb_65(void) { BIT(4,L);                        } /* BIT  4,L         */
INLINE void cb_66(void) { BIT_HL(4,RM(HL));                } /* BIT  4,(HL)      */
INLINE void cb_67(void) { BIT(4,A);                        } /* BIT  4,A         */

INLINE void cb_68(void) { BIT(5,B);                        } /* BIT  5,B         */
INLINE void cb_69(void) { BIT(5,C);                        } /* BIT  5,C         */
INLINE void cb_6a(void) { BIT(5,D);                        } /* BIT  5,D         */
INLINE void cb_6b(void) { BIT(5,E);                        } /* BIT  5,E         */
INLINE void cb_6c(void) { BIT(5,H);                        } /* BIT  5,H         */
INLINE void cb_6d(void) { BIT(5,L);                        } /* BIT  5,L         */
INLINE void cb_6e(void) { BIT_HL(5,RM(HL));                } /* BIT  5,(HL)      */
INLINE void cb_6f(void) { BIT(5,A);                        } /* BIT  5,A         */

INLINE void cb_70(void) { BIT(6,B);                        } /* BIT  6,B         */
INLINE void cb_71(void) { BIT(6,C);                        } /* BIT  6,C         */
INLINE void cb_72(void) { BIT(6,D);                        } /* BIT  6,D         */
INLINE void cb_73(void) { BIT(6,E);                        } /* BIT  6,E         */
INLINE void cb_74(void) { BIT(6,H);                        } /* BIT  6,H         */
INLINE void cb_75(void) { BIT(6,L);                        } /* BIT  6,L         */
INLINE void cb_76(void) { BIT_HL(6,RM(HL));                } /* BIT  6,(HL)      */
INLINE void cb_77(void) { BIT(6,A);                        } /* BIT  6,A         */

INLINE void cb_78(void) { BIT(7,B);                        } /* BIT  7,B         */
INLINE void cb_79(void) { BIT(7,C);                        } /* BIT  7,C         */
INLINE void cb_7a(void) { BIT(7,D);                        } /* BIT  7,D         */
INLINE void cb_7b(void) { BIT(7,E);                        } /* BIT  7,E         */
INLINE void cb_7c(void) { BIT(7,H);                        } /* BIT  7,H         */
INLINE void cb_7d(void) { BIT(7,L);                        } /* BIT  7,L         */
INLINE void cb_7e(void) { BIT_HL(7,RM(HL));                } /* BIT  7,(HL)      */
INLINE void cb_7f(void) { BIT(7,A);                        } /* BIT  7,A         */

INLINE void cb_80(void) { B = RES(0,B);                    } /* RES  0,B         */
INLINE void cb_81(void) { C = RES(0,C);                    } /* RES  0,C         */
INLINE void cb_82(void) { D = RES(0,D);                    } /* RES  0,D         */
INLINE void cb_83(void) { E = RES(0,E);                    } /* RES  0,E         */
INLINE void cb_84(void) { H = RES(0,H);                    } /* RES  0,H         */
INLINE void cb_85(void) { L = RES(0,L);                    } /* RES  0,L         */
INLINE void cb_86(void) { WM( HL, RES(0,RM(HL)) );         } /* RES  0,(HL)      */
INLINE void cb_87(void) { A = RES(0,A);                    } /* RES  0,A         */

INLINE void cb_88(void) { B = RES(1,B);                    } /* RES  1,B         */
INLINE void cb_89(void) { C = RES(1,C);                    } /* RES  1,C         */
INLINE void cb_8a(void) { D = RES(1,D);                    } /* RES  1,D         */
INLINE void cb_8b(void) { E = RES(1,E);                    } /* RES  1,E         */
INLINE void cb_8c(void) { H = RES(1,H);                    } /* RES  1,H         */
INLINE void cb_8d(void) { L = RES(1,L);                    } /* RES  1,L         */
INLINE void cb_8e(void) { WM( HL, RES(1,RM(HL)) );         } /* RES  1,(HL)      */
INLINE void cb_8f(void) { A = RES(1,A);                    } /* RES  1,A         */

INLINE void cb_90(void) { B = RES(2,B);                    } /* RES  2,B         */
INLINE void cb_91(void) { C = RES(2,C);                    } /* RES  2,C         */
INLINE void cb_92(void) { D = RES(2,D);                    } /* RES  2,D         */
INLINE void cb_93(void) { E = RES(2,E);                    } /* RES  2,E         */
INLINE void cb_94(void) { H = RES(2,H);                    } /* RES  2,H         */
INLINE void cb_95(void) { L = RES(2,L);                    } /* RES  2,L         */
INLINE void cb_96(void) { WM( HL, RES(2,RM(HL)) );         } /* RES  2,(HL)      */
INLINE void cb_97(void) { A = RES(2,A);                    } /* RES  2,A         */

INLINE void cb_98(void) { B = RES(3,B);                    } /* RES  3,B         */
INLINE void cb_99(void) { C = RES(3,C);                    } /* RES  3,C         */
INLINE void cb_9a(void) { D = RES(3,D);                    } /* RES  3,D         */
INLINE void cb_9b(void) { E = RES(3,E);                    } /* RES  3,E         */
INLINE void cb_9c(void) { H = RES(3,H);                    } /* RES  3,H         */
INLINE void cb_9d(void) { L = RES(3,L);                    } /* RES  3,L         */
INLINE void cb_9e(void) { WM( HL, RES(3,RM(HL)) );         } /* RES  3,(HL)      */
INLINE void cb_9f(void) { A = RES(3,A);                    } /* RES  3,A         */

INLINE void cb_a0(void) { B = RES(4,B);                    } /* RES  4,B         */
INLINE void cb_a1(void) { C = RES(4,C);                    } /* RES  4,C         */
INLINE void cb_a2(void) { D = RES(4,D);                    } /* RES  4,D         */
INLINE void cb_a3(void) { E = RES(4,E);                    } /* RES  4,E         */
INLINE void cb_a4(void) { H = RES(4,H);                    } /* RES  4,H         */
INLINE void cb_a5(void) { L = RES(4,L);                    } /* RES  4,L         */
INLINE void cb_a6(void) { WM( HL, RES(4,RM(HL)) );         } /* RES  4,(HL)      */
INLINE void cb_a7(void) { A = RES(4,A);                    } /* RES  4,A         */

INLINE void cb_a8(void) { B = RES(5,B);                    } /* RES  5,B         */
INLINE void cb_a9(void) { C = RES(5,C);                    } /* RES  5,C         */
INLINE void cb_aa(void) { D = RES(5,D);                    } /* RES  5,D         */
INLINE void cb_ab(void) { E = RES(5,E);                    } /* RES  5,E         */
INLINE void cb_ac(void) { H = RES(5,H);                    } /* RES  5,H         */
INLINE void cb_ad(void) { L = RES(5,L);                    } /* RES  5,L         */
INLINE void cb_ae(void) { WM( HL, RES(5,RM(HL)) );         } /* RES  5,(HL)      */
INLINE void cb_af(void) { A = RES(5,A);                    } /* RES  5,A         */

INLINE void cb_b0(void) { B = RES(6,B);                    } /* RES  6,B         */
INLINE void cb_b1(void) { C = RES(6,C);                    } /* RES  6,C         */
INLINE void cb_b2(void) { D = RES(6,D);                    } /* RES  6,D         */
INLINE void cb_b3(void) { E = RES(6,E);                    } /* RES  6,E         */
INLINE void cb_b4(void) { H = RES(6,H);                    } /* RES  6,H         */
INLINE void cb_b5(void) { L = RES(6,L);                    } /* RES  6,L         */
INLINE void cb_b6(void) { WM( HL, RES(6,RM(HL)) );         } /* RES  6,(HL)      */
INLINE void cb_b7(void) { A = RES(6,A);                    } /* RES  6,A         */

INLINE void cb_b8(void) { B = RES(7,B);                    } /* RES  7,B         */
INLINE void cb_b9(void) { C = RES(7,C);                    } /* RES  7,C         */
INLINE void cb_ba(void) { D = RES(7,D);                    } /* RES  7,D         */
INLINE void cb_bb(void) { E = RES(7,E);                    } /* RES  7,E         */
INLINE void cb_bc(void) { H = RES(7,H);                    } /* RES  7,H         */
INLINE void cb_bd(void) { L = RES(7,L);                    } /* RES  7,L         */
INLINE void cb_be(void) { WM( HL, RES(7,RM(HL)) );         } /* RES  7,(HL)      */
INLINE void cb_bf(void) { A = RES(7,A);                    } /* RES  7,A         */

INLINE void cb_c0(void) { B = SET(0,B);                    } /* SET  0,B         */
INLINE void cb_c1(void) { C = SET(0,C);                    } /* SET  0,C         */
INLINE void cb_c2(void) { D = SET(0,D);                    } /* SET  0,D         */
INLINE void cb_c3(void) { E = SET(0,E);                    } /* SET  0,E         */
INLINE void cb_c4(void) { H = SET(0,H);                    } /* SET  0,H         */
INLINE void cb_c5(void) { L = SET(0,L);                    } /* SET  0,L         */
INLINE void cb_c6(void) { WM( HL, SET(0,RM(HL)) );         } /* SET  0,(HL)      */
INLINE void cb_c7(void) { A = SET(0,A);                    } /* SET  0,A         */

INLINE void cb_c8(void) { B = SET(1,B);                    } /* SET  1,B         */
INLINE void cb_c9(void) { C = SET(1,C);                    } /* SET  1,C         */
INLINE void cb_ca(void) { D = SET(1,D);                    } /* SET  1,D         */
INLINE void cb_cb(void) { E = SET(1,E);                    } /* SET  1,E         */
INLINE void cb_cc(void) { H = SET(1,H);                    } /* SET  1,H         */
INLINE void cb_cd(void) { L = SET(1,L);                    } /* SET  1,L         */
INLINE void cb_ce(void) { WM( HL, SET(1,RM(HL)) );         } /* SET  1,(HL)      */
INLINE void cb_cf(void) { A = SET(1,A);                    } /* SET  1,A         */

INLINE void cb_d0(void) { B = SET(2,B);                    } /* SET  2,B         */
INLINE void cb_d1(void) { C = SET(2,C);                    } /* SET  2,C         */
INLINE void cb_d2(void) { D = SET(2,D);                    } /* SET  2,D         */
INLINE void cb_d3(void) { E = SET(2,E);                    } /* SET  2,E         */
INLINE void cb_d4(void) { H = SET(2,H);                    } /* SET  2,H         */
INLINE void cb_d5(void) { L = SET(2,L);                    } /* SET  2,L         */
INLINE void cb_d6(void) { WM( HL, SET(2,RM(HL)) );         } /* SET  2,(HL)      */
INLINE void cb_d7(void) { A = SET(2,A);                    } /* SET  2,A         */

INLINE void cb_d8(void) { B = SET(3,B);                    } /* SET  3,B         */
INLINE void cb_d9(void) { C = SET(3,C);                    } /* SET  3,C         */
INLINE void cb_da(void) { D = SET(3,D);                    } /* SET  3,D         */
INLINE void cb_db(void) { E = SET(3,E);                    } /* SET  3,E         */
INLINE void cb_dc(void) { H = SET(3,H);                    } /* SET  3,H         */
INLINE void cb_dd(void) { L = SET(3,L);                    } /* SET  3,L         */
INLINE void cb_de(void) { WM( HL, SET(3,RM(HL)) );         } /* SET  3,(HL)      */
INLINE void cb_df(void) { A = SET(3,A);                    } /* SET  3,A         */

INLINE void cb_e0(void) { B = SET(4,B);                    } /* SET  4,B         */
INLINE void cb_e1(void) { C = SET(4,C);                    } /* SET  4,C         */
INLINE void cb_e2(void) { D = SET(4,D);                    } /* SET  4,D         */
INLINE void cb_e3(void) { E = SET(4,E);                    } /* SET  4,E         */
INLINE void cb_e4(void) { H = SET(4,H);                    } /* SET  4,H         */
INLINE void cb_e5(void) { L = SET(4,L);                    } /* SET  4,L         */
INLINE void cb_e6(void) { WM( HL, SET(4,RM(HL)) );         } /* SET  4,(HL)      */
INLINE void cb_e7(void) { A = SET(4,A);                    } /* SET  4,A         */

INLINE void cb_e8(void) { B = SET(5,B);                    } /* SET  5,B         */
INLINE void cb_e9(void) { C = SET(5,C);                    } /* SET  5,C         */
INLINE void cb_ea(void) { D = SET(5,D);                    } /* SET  5,D         */
INLINE void cb_eb(void) { E = SET(5,E);                    } /* SET  5,E         */
INLINE void cb_ec(void) { H = SET(5,H);                    } /* SET  5,H         */
INLINE void cb_ed(void) { L = SET(5,L);                    } /* SET  5,L         */
INLINE void cb_ee(void) { WM( HL, SET(5,RM(HL)) );         } /* SET  5,(HL)      */
INLINE void cb_ef(void) { A = SET(5,A);                    } /* SET  5,A         */

INLINE void cb_f0(void) { B = SET(6,B);                    } /* SET  6,B         */
INLINE void cb_f1(void) { C = SET(6,C);                    } /* SET  6,C         */
INLINE void cb_f2(void) { D = SET(6,D);                    } /* SET  6,D         */
INLINE void cb_f3(void) { E = SET(6,E);                    } /* SET  6,E         */
INLINE void cb_f4(void) { H = SET(6,H);                    } /* SET  6,H         */
INLINE void cb_f5(void) { L = SET(6,L);                    } /* SET  6,L         */
INLINE void cb_f6(void) { WM( HL, SET(6,RM(HL)) );         } /* SET  6,(HL)      */
INLINE void cb_f7(void) { A = SET(6,A);                    } /* SET  6,A         */

INLINE void cb_f8(void) { B = SET(7,B);                    } /* SET  7,B         */
INLINE void cb_f9(void) { C = SET(7,C);                    } /* SET  7,C         */
INLINE void cb_fa(void) { D = SET(7,D);                    } /* SET  7,D         */
INLINE void cb_fb(void) { E = SET(7,E);                    } /* SET  7,E         */
INLINE void cb_fc(void) { H = SET(7,H);                    } /* SET  7,H         */
INLINE void cb_fd(void) { L = SET(7,L);                    } /* SET  7,L         */
INLINE void cb_fe(void) { WM( HL, SET(7,RM(HL)) );         } /* SET  7,(HL)      */
INLINE void cb_ff(void) { A = SET(7,A);                    } /* SET  7,A         */


/**********************************************************
* opcodes with DD/FD CB prefix
* rotate, shift and bit operations with (IX+o)
**********************************************************/
INLINE void xycb_00(void) { B = RLC( RM(EA) ); WM( EA,B );            } /* RLC  B=(XY+o)    */
INLINE void xycb_01(void) { C = RLC( RM(EA) ); WM( EA,C );            } /* RLC  C=(XY+o)    */
INLINE void xycb_02(void) { D = RLC( RM(EA) ); WM( EA,D );            } /* RLC  D=(XY+o)    */
INLINE void xycb_03(void) { E = RLC( RM(EA) ); WM( EA,E );            } /* RLC  E=(XY+o)    */
INLINE void xycb_04(void) { H = RLC( RM(EA) ); WM( EA,H );            } /* RLC  H=(XY+o)    */
INLINE void xycb_05(void) { L = RLC( RM(EA) ); WM( EA,L );            } /* RLC  L=(XY+o)    */
INLINE void xycb_06(void) { WM( EA, RLC( RM(EA) ) );                  } /* RLC  (XY+o)      */
INLINE void xycb_07(void) { A = RLC( RM(EA) ); WM( EA,A );            } /* RLC  A=(XY+o)    */

INLINE void xycb_08(void) { B = RRC( RM(EA) ); WM( EA,B );            } /* RRC  B=(XY+o)    */
INLINE void xycb_09(void) { C = RRC( RM(EA) ); WM( EA,C );            } /* RRC  C=(XY+o)    */
INLINE void xycb_0a(void) { D = RRC( RM(EA) ); WM( EA,D );            } /* RRC  D=(XY+o)    */
INLINE void xycb_0b(void) { E = RRC( RM(EA) ); WM( EA,E );            } /* RRC  E=(XY+o)    */
INLINE void xycb_0c(void) { H = RRC( RM(EA) ); WM( EA,H );            } /* RRC  H=(XY+o)    */
INLINE void xycb_0d(void) { L = RRC( RM(EA) ); WM( EA,L );            } /* RRC  L=(XY+o)    */
INLINE void xycb_0e(void) { WM( EA,RRC( RM(EA) ) );                   } /* RRC  (XY+o)      */
INLINE void xycb_0f(void) { A = RRC( RM(EA) ); WM( EA,A );            } /* RRC  A=(XY+o)    */

INLINE void xycb_10(void) { B = RL( RM(EA) ); WM( EA,B );             } /* RL   B=(XY+o)    */
INLINE void xycb_11(void) { C = RL( RM(EA) ); WM( EA,C );             } /* RL   C=(XY+o)    */
INLINE void xycb_12(void) { D = RL( RM(EA) ); WM( EA,D );             } /* RL   D=(XY+o)    */
INLINE void xycb_13(void) { E = RL( RM(EA) ); WM( EA,E );             } /* RL   E=(XY+o)    */
INLINE void xycb_14(void) { H = RL( RM(EA) ); WM( EA,H );             } /* RL   H=(XY+o)    */
INLINE void xycb_15(void) { L = RL( RM(EA) ); WM( EA,L );             } /* RL   L=(XY+o)    */
INLINE void xycb_16(void) { WM( EA,RL( RM(EA) ) );                    } /* RL   (XY+o)      */
INLINE void xycb_17(void) { A = RL( RM(EA) ); WM( EA,A );             } /* RL   A=(XY+o)    */

INLINE void xycb_18(void) { B = RR( RM(EA) ); WM( EA,B );             } /* RR   B=(XY+o)    */
INLINE void xycb_19(void) { C = RR( RM(EA) ); WM( EA,C );             } /* RR   C=(XY+o)    */
INLINE void xycb_1a(void) { D = RR( RM(EA) ); WM( EA,D );             } /* RR   D=(XY+o)    */
INLINE void xycb_1b(void) { E = RR( RM(EA) ); WM( EA,E );             } /* RR   E=(XY+o)    */
INLINE void xycb_1c(void) { H = RR( RM(EA) ); WM( EA,H );             } /* RR   H=(XY+o)    */
INLINE void xycb_1d(void) { L = RR( RM(EA) ); WM( EA,L );             } /* RR   L=(XY+o)    */
INLINE void xycb_1e(void) { WM( EA,RR( RM(EA) ) );                    } /* RR   (XY+o)      */
INLINE void xycb_1f(void) { A = RR( RM(EA) ); WM( EA,A );             } /* RR   A=(XY+o)    */

INLINE void xycb_20(void) { B = SLA( RM(EA) ); WM( EA,B );            } /* SLA  B=(XY+o)    */
INLINE void xycb_21(void) { C = SLA( RM(EA) ); WM( EA,C );            } /* SLA  C=(XY+o)    */
INLINE void xycb_22(void) { D = SLA( RM(EA) ); WM( EA,D );            } /* SLA  D=(XY+o)    */
INLINE void xycb_23(void) { E = SLA( RM(EA) ); WM( EA,E );            } /* SLA  E=(XY+o)    */
INLINE void xycb_24(void) { H = SLA( RM(EA) ); WM( EA,H );            } /* SLA  H=(XY+o)    */
INLINE void xycb_25(void) { L = SLA( RM(EA) ); WM( EA,L );            } /* SLA  L=(XY+o)    */
INLINE void xycb_26(void) { WM( EA,SLA( RM(EA) ) );                   } /* SLA  (XY+o)      */
INLINE void xycb_27(void) { A = SLA( RM(EA) ); WM( EA,A );            } /* SLA  A=(XY+o)    */

INLINE void xycb_28(void) { B = SRA( RM(EA) ); WM( EA,B );            } /* SRA  B=(XY+o)    */
INLINE void xycb_29(void) { C = SRA( RM(EA) ); WM( EA,C );            } /* SRA  C=(XY+o)    */
INLINE void xycb_2a(void) { D = SRA( RM(EA) ); WM( EA,D );            } /* SRA  D=(XY+o)    */
INLINE void xycb_2b(void) { E = SRA( RM(EA) ); WM( EA,E );            } /* SRA  E=(XY+o)    */
INLINE void xycb_2c(void) { H = SRA( RM(EA) ); WM( EA,H );            } /* SRA  H=(XY+o)    */
INLINE void xycb_2d(void) { L = SRA( RM(EA) ); WM( EA,L );            } /* SRA  L=(XY+o)    */
INLINE void xycb_2e(void) { WM( EA,SRA( RM(EA) ) );                   } /* SRA  (XY+o)      */
INLINE void xycb_2f(void) { A = SRA( RM(EA) ); WM( EA,A );            } /* SRA  A=(XY+o)    */

INLINE void xycb_30(void) { B = SLL( RM(EA) ); WM( EA,B );            } /* SLL  B=(XY+o)    */
INLINE void xycb_31(void) { C = SLL( RM(EA) ); WM( EA,C );            } /* SLL  C=(XY+o)    */
INLINE void xycb_32(void) { D = SLL( RM(EA) ); WM( EA,D );            } /* SLL  D=(XY+o)    */
INLINE void xycb_33(void) { E = SLL( RM(EA) ); WM( EA,E );            } /* SLL  E=(XY+o)    */
INLINE void xycb_34(void) { H = SLL( RM(EA) ); WM( EA,H );            } /* SLL  H=(XY+o)    */
INLINE void xycb_35(void) { L = SLL( RM(EA) ); WM( EA,L );            } /* SLL  L=(XY+o)    */
INLINE void xycb_36(void) { WM( EA,SLL( RM(EA) ) );                   } /* SLL  (XY+o)      */
INLINE void xycb_37(void) { A = SLL( RM(EA) ); WM( EA,A );            } /* SLL  A=(XY+o)    */

INLINE void xycb_38(void) { B = SRL( RM(EA) ); WM( EA,B );            } /* SRL  B=(XY+o)    */
INLINE void xycb_39(void) { C = SRL( RM(EA) ); WM( EA,C );            } /* SRL  C=(XY+o)    */
INLINE void xycb_3a(void) { D = SRL( RM(EA) ); WM( EA,D );            } /* SRL  D=(XY+o)    */
INLINE void xycb_3b(void) { E = SRL( RM(EA) ); WM( EA,E );            } /* SRL  E=(XY+o)    */
INLINE void xycb_3c(void) { H = SRL( RM(EA) ); WM( EA,H );            } /* SRL  H=(XY+o)    */
INLINE void xycb_3d(void) { L = SRL( RM(EA) ); WM( EA,L );            } /* SRL  L=(XY+o)    */
INLINE void xycb_3e(void) { WM( EA,SRL( RM(EA) ) );                   } /* SRL  (XY+o)      */
INLINE void xycb_3f(void) { A = SRL( RM(EA) ); WM( EA,A );            } /* SRL  A=(XY+o)    */

INLINE void xycb_40(void) { xycb_46();                                } /* BIT  0,(XY+o)    */
INLINE void xycb_41(void) { xycb_46();                                } /* BIT  0,(XY+o)    */
INLINE void xycb_42(void) { xycb_46();                                } /* BIT  0,(XY+o)    */
INLINE void xycb_43(void) { xycb_46();                                } /* BIT  0,(XY+o)    */
INLINE void xycb_44(void) { xycb_46();                                } /* BIT  0,(XY+o)    */
INLINE void xycb_45(void) { xycb_46();                                } /* BIT  0,(XY+o)    */
INLINE void xycb_46(void) { BIT_XY(0,RM(EA));                         } /* BIT  0,(XY+o)    */
INLINE void xycb_47(void) { xycb_46();                                } /* BIT  0,(XY+o)    */

INLINE void xycb_48(void) { xycb_4e();                                } /* BIT  1,(XY+o)    */
INLINE void xycb_49(void) { xycb_4e();                                } /* BIT  1,(XY+o)    */
INLINE void xycb_4a(void) { xycb_4e();                                } /* BIT  1,(XY+o)    */
INLINE void xycb_4b(void) { xycb_4e();                                } /* BIT  1,(XY+o)    */
INLINE void xycb_4c(void) { xycb_4e();                                } /* BIT  1,(XY+o)    */
INLINE void xycb_4d(void) { xycb_4e();                                } /* BIT  1,(XY+o)    */
INLINE void xycb_4e(void) { BIT_XY(1,RM(EA));                         } /* BIT  1,(XY+o)    */
INLINE void xycb_4f(void) { xycb_4e();                                } /* BIT  1,(XY+o)    */

INLINE void xycb_50(void) { xycb_56();                                } /* BIT  2,(XY+o)    */
INLINE void xycb_51(void) { xycb_56();                                } /* BIT  2,(XY+o)    */
INLINE void xycb_52(void) { xycb_56();                                } /* BIT  2,(XY+o)    */
INLINE void xycb_53(void) { xycb_56();                                } /* BIT  2,(XY+o)    */
INLINE void xycb_54(void) { xycb_56();                                } /* BIT  2,(XY+o)    */
INLINE void xycb_55(void) { xycb_56();                                } /* BIT  2,(XY+o)    */
INLINE void xycb_56(void) { BIT_XY(2,RM(EA));                         } /* BIT  2,(XY+o)    */
INLINE void xycb_57(void) { xycb_56();                                } /* BIT  2,(XY+o)    */

INLINE void xycb_58(void) { xycb_5e();                                } /* BIT  3,(XY+o)    */
INLINE void xycb_59(void) { xycb_5e();                                } /* BIT  3,(XY+o)    */
INLINE void xycb_5a(void) { xycb_5e();                                } /* BIT  3,(XY+o)    */
INLINE void xycb_5b(void) { xycb_5e();                                } /* BIT  3,(XY+o)    */
INLINE void xycb_5c(void) { xycb_5e();                                } /* BIT  3,(XY+o)    */
INLINE void xycb_5d(void) { xycb_5e();                                } /* BIT  3,(XY+o)    */
INLINE void xycb_5e(void) { BIT_XY(3,RM(EA));                         } /* BIT  3,(XY+o)    */
INLINE void xycb_5f(void) { xycb_5e();                                } /* BIT  3,(XY+o)    */

INLINE void xycb_60(void) { xycb_66();                                } /* BIT  4,(XY+o)    */
INLINE void xycb_61(void) { xycb_66();                                } /* BIT  4,(XY+o)    */
INLINE void xycb_62(void) { xycb_66();                                } /* BIT  4,(XY+o)    */
INLINE void xycb_63(void) { xycb_66();                                } /* BIT  4,(XY+o)    */
INLINE void xycb_64(void) { xycb_66();                                } /* BIT  4,(XY+o)    */
INLINE void xycb_65(void) { xycb_66();                                } /* BIT  4,(XY+o)    */
INLINE void xycb_66(void) { BIT_XY(4,RM(EA));                         } /* BIT  4,(XY+o)    */
INLINE void xycb_67(void) { xycb_66();                                } /* BIT  4,(XY+o)    */

INLINE void xycb_68(void) { xycb_6e();                                } /* BIT  5,(XY+o)    */
INLINE void xycb_69(void) { xycb_6e();                                } /* BIT  5,(XY+o)    */
INLINE void xycb_6a(void) { xycb_6e();                                } /* BIT  5,(XY+o)    */
INLINE void xycb_6b(void) { xycb_6e();                                } /* BIT  5,(XY+o)    */
INLINE void xycb_6c(void) { xycb_6e();                                } /* BIT  5,(XY+o)    */
INLINE void xycb_6d(void) { xycb_6e();                                } /* BIT  5,(XY+o)    */
INLINE void xycb_6e(void) { BIT_XY(5,RM(EA));                         } /* BIT  5,(XY+o)    */
INLINE void xycb_6f(void) { xycb_6e();                                } /* BIT  5,(XY+o)    */

INLINE void xycb_70(void) { xycb_76();                                } /* BIT  6,(XY+o)    */
INLINE void xycb_71(void) { xycb_76();                                } /* BIT  6,(XY+o)    */
INLINE void xycb_72(void) { xycb_76();                                } /* BIT  6,(XY+o)    */
INLINE void xycb_73(void) { xycb_76();                                } /* BIT  6,(XY+o)    */
INLINE void xycb_74(void) { xycb_76();                                } /* BIT  6,(XY+o)    */
INLINE void xycb_75(void) { xycb_76();                                } /* BIT  6,(XY+o)    */
INLINE void xycb_76(void) { BIT_XY(6,RM(EA));                         } /* BIT  6,(XY+o)    */
INLINE void xycb_77(void) { xycb_76();                                } /* BIT  6,(XY+o)    */

INLINE void xycb_78(void) { xycb_7e();                                } /* BIT  7,(XY+o)    */
INLINE void xycb_79(void) { xycb_7e();                                } /* BIT  7,(XY+o)    */
INLINE void xycb_7a(void) { xycb_7e();                                } /* BIT  7,(XY+o)    */
INLINE void xycb_7b(void) { xycb_7e();                                } /* BIT  7,(XY+o)    */
INLINE void xycb_7c(void) { xycb_7e();                                } /* BIT  7,(XY+o)    */
INLINE void xycb_7d(void) { xycb_7e();                                } /* BIT  7,(XY+o)    */
INLINE void xycb_7e(void) { BIT_XY(7,RM(EA));                         } /* BIT  7,(XY+o)    */
INLINE void xycb_7f(void) { xycb_7e();                                } /* BIT  7,(XY+o)    */

INLINE void xycb_80(void) { B = RES(0, RM(EA) ); WM( EA,B );          } /* RES  0,B=(XY+o)  */
INLINE void xycb_81(void) { C = RES(0, RM(EA) ); WM( EA,C );          } /* RES  0,C=(XY+o)  */
INLINE void xycb_82(void) { D = RES(0, RM(EA) ); WM( EA,D );          } /* RES  0,D=(XY+o)  */
INLINE void xycb_83(void) { E = RES(0, RM(EA) ); WM( EA,E );          } /* RES  0,E=(XY+o)  */
INLINE void xycb_84(void) { H = RES(0, RM(EA) ); WM( EA,H );          } /* RES  0,H=(XY+o)  */
INLINE void xycb_85(void) { L = RES(0, RM(EA) ); WM( EA,L );          } /* RES  0,L=(XY+o)  */
INLINE void xycb_86(void) { WM( EA, RES(0,RM(EA)) );                  } /* RES  0,(XY+o)    */
INLINE void xycb_87(void) { A = RES(0, RM(EA) ); WM( EA,A );          } /* RES  0,A=(XY+o)  */

INLINE void xycb_88(void) { B = RES(1, RM(EA) ); WM( EA,B );          } /* RES  1,B=(XY+o)  */
INLINE void xycb_89(void) { C = RES(1, RM(EA) ); WM( EA,C );          } /* RES  1,C=(XY+o)  */
INLINE void xycb_8a(void) { D = RES(1, RM(EA) ); WM( EA,D );          } /* RES  1,D=(XY+o)  */
INLINE void xycb_8b(void) { E = RES(1, RM(EA) ); WM( EA,E );          } /* RES  1,E=(XY+o)  */
INLINE void xycb_8c(void) { H = RES(1, RM(EA) ); WM( EA,H );          } /* RES  1,H=(XY+o)  */
INLINE void xycb_8d(void) { L = RES(1, RM(EA) ); WM( EA,L );          } /* RES  1,L=(XY+o)  */
INLINE void xycb_8e(void) { WM( EA, RES(1,RM(EA)) );                  } /* RES  1,(XY+o)    */
INLINE void xycb_8f(void) { A = RES(1, RM(EA) ); WM( EA,A );          } /* RES  1,A=(XY+o)  */

INLINE void xycb_90(void) { B = RES(2, RM(EA) ); WM( EA,B );          } /* RES  2,B=(XY+o)  */
INLINE void xycb_91(void) { C = RES(2, RM(EA) ); WM( EA,C );          } /* RES  2,C=(XY+o)  */
INLINE void xycb_92(void) { D = RES(2, RM(EA) ); WM( EA,D );          } /* RES  2,D=(XY+o)  */
INLINE void xycb_93(void) { E = RES(2, RM(EA) ); WM( EA,E );          } /* RES  2,E=(XY+o)  */
INLINE void xycb_94(void) { H = RES(2, RM(EA) ); WM( EA,H );          } /* RES  2,H=(XY+o)  */
INLINE void xycb_95(void) { L = RES(2, RM(EA) ); WM( EA,L );          } /* RES  2,L=(XY+o)  */
INLINE void xycb_96(void) { WM( EA, RES(2,RM(EA)) );                  } /* RES  2,(XY+o)    */
INLINE void xycb_97(void) { A = RES(2, RM(EA) ); WM( EA,A );          } /* RES  2,A=(XY+o)  */

INLINE void xycb_98(void) { B = RES(3, RM(EA) ); WM( EA,B );          } /* RES  3,B=(XY+o)  */
INLINE void xycb_99(void) { C = RES(3, RM(EA) ); WM( EA,C );          } /* RES  3,C=(XY+o)  */
INLINE void xycb_9a(void) { D = RES(3, RM(EA) ); WM( EA,D );          } /* RES  3,D=(XY+o)  */
INLINE void xycb_9b(void) { E = RES(3, RM(EA) ); WM( EA,E );          } /* RES  3,E=(XY+o)  */
INLINE void xycb_9c(void) { H = RES(3, RM(EA) ); WM( EA,H );          } /* RES  3,H=(XY+o)  */
INLINE void xycb_9d(void) { L = RES(3, RM(EA) ); WM( EA,L );          } /* RES  3,L=(XY+o)  */
INLINE void xycb_9e(void) { WM( EA, RES(3,RM(EA)) );                  } /* RES  3,(XY+o)    */
INLINE void xycb_9f(void) { A = RES(3, RM(EA) ); WM( EA,A );          } /* RES  3,A=(XY+o)  */

INLINE void xycb_a0(void) { B = RES(4, RM(EA) ); WM( EA,B );          } /* RES  4,B=(XY+o)  */
INLINE void xycb_a1(void) { C = RES(4, RM(EA) ); WM( EA,C );          } /* RES  4,C=(XY+o)  */
INLINE void xycb_a2(void) { D = RES(4, RM(EA) ); WM( EA,D );          } /* RES  4,D=(XY+o)  */
INLINE void xycb_a3(void) { E = RES(4, RM(EA) ); WM( EA,E );          } /* RES  4,E=(XY+o)  */
INLINE void xycb_a4(void) { H = RES(4, RM(EA) ); WM( EA,H );          } /* RES  4,H=(XY+o)  */
INLINE void xycb_a5(void) { L = RES(4, RM(EA) ); WM( EA,L );          } /* RES  4,L=(XY+o)  */
INLINE void xycb_a6(void) { WM( EA, RES(4,RM(EA)) );                  } /* RES  4,(XY+o)    */
INLINE void xycb_a7(void) { A = RES(4, RM(EA) ); WM( EA,A );          } /* RES  4,A=(XY+o)  */

INLINE void xycb_a8(void) { B = RES(5, RM(EA) ); WM( EA,B );          } /* RES  5,B=(XY+o)  */
INLINE void xycb_a9(void) { C = RES(5, RM(EA) ); WM( EA,C );          } /* RES  5,C=(XY+o)  */
INLINE void xycb_aa(void) { D = RES(5, RM(EA) ); WM( EA,D );          } /* RES  5,D=(XY+o)  */
INLINE void xycb_ab(void) { E = RES(5, RM(EA) ); WM( EA,E );          } /* RES  5,E=(XY+o)  */
INLINE void xycb_ac(void) { H = RES(5, RM(EA) ); WM( EA,H );          } /* RES  5,H=(XY+o)  */
INLINE void xycb_ad(void) { L = RES(5, RM(EA) ); WM( EA,L );          } /* RES  5,L=(XY+o)  */
INLINE void xycb_ae(void) { WM( EA, RES(5,RM(EA)) );                  } /* RES  5,(XY+o)    */
INLINE void xycb_af(void) { A = RES(5, RM(EA) ); WM( EA,A );          } /* RES  5,A=(XY+o)  */

INLINE void xycb_b0(void) { B = RES(6, RM(EA) ); WM( EA,B );          } /* RES  6,B=(XY+o)  */
INLINE void xycb_b1(void) { C = RES(6, RM(EA) ); WM( EA,C );          } /* RES  6,C=(XY+o)  */
INLINE void xycb_b2(void) { D = RES(6, RM(EA) ); WM( EA,D );          } /* RES  6,D=(XY+o)  */
INLINE void xycb_b3(void) { E = RES(6, RM(EA) ); WM( EA,E );          } /* RES  6,E=(XY+o)  */
INLINE void xycb_b4(void) { H = RES(6, RM(EA) ); WM( EA,H );          } /* RES  6,H=(XY+o)  */
INLINE void xycb_b5(void) { L = RES(6, RM(EA) ); WM( EA,L );          } /* RES  6,L=(XY+o)  */
INLINE void xycb_b6(void) { WM( EA, RES(6,RM(EA)) );                  } /* RES  6,(XY+o)    */
INLINE void xycb_b7(void) { A = RES(6, RM(EA) ); WM( EA,A );          } /* RES  6,A=(XY+o)  */

INLINE void xycb_b8(void) { B = RES(7, RM(EA) ); WM( EA,B );          } /* RES  7,B=(XY+o)  */
INLINE void xycb_b9(void) { C = RES(7, RM(EA) ); WM( EA,C );          } /* RES  7,C=(XY+o)  */
INLINE void xycb_ba(void) { D = RES(7, RM(EA) ); WM( EA,D );          } /* RES  7,D=(XY+o)  */
INLINE void xycb_bb(void) { E = RES(7, RM(EA) ); WM( EA,E );          } /* RES  7,E=(XY+o)  */
INLINE void xycb_bc(void) { H = RES(7, RM(EA) ); WM( EA,H );          } /* RES  7,H=(XY+o)  */
INLINE void xycb_bd(void) { L = RES(7, RM(EA) ); WM( EA,L );          } /* RES  7,L=(XY+o)  */
INLINE void xycb_be(void) { WM( EA, RES(7,RM(EA)) );                  } /* RES  7,(XY+o)    */
INLINE void xycb_bf(void) { A = RES(7, RM(EA) ); WM( EA,A );          } /* RES  7,A=(XY+o)  */

INLINE void xycb_c0(void) { B = SET(0, RM(EA) ); WM( EA,B );          } /* SET  0,B=(XY+o)  */
INLINE void xycb_c1(void) { C = SET(0, RM(EA) ); WM( EA,C );          } /* SET  0,C=(XY+o)  */
INLINE void xycb_c2(void) { D = SET(0, RM(EA) ); WM( EA,D );          } /* SET  0,D=(XY+o)  */
INLINE void xycb_c3(void) { E = SET(0, RM(EA) ); WM( EA,E );          } /* SET  0,E=(XY+o)  */
INLINE void xycb_c4(void) { H = SET(0, RM(EA) ); WM( EA,H );          } /* SET  0,H=(XY+o)  */
INLINE void xycb_c5(void) { L = SET(0, RM(EA) ); WM( EA,L );          } /* SET  0,L=(XY+o)  */
INLINE void xycb_c6(void) { WM( EA, SET(0,RM(EA)) );                  } /* SET  0,(XY+o)    */
INLINE void xycb_c7(void) { A = SET(0, RM(EA) ); WM( EA,A );          } /* SET  0,A=(XY+o)  */

INLINE void xycb_c8(void) { B = SET(1, RM(EA) ); WM( EA,B );          } /* SET  1,B=(XY+o)  */
INLINE void xycb_c9(void) { C = SET(1, RM(EA) ); WM( EA,C );          } /* SET  1,C=(XY+o)  */
INLINE void xycb_ca(void) { D = SET(1, RM(EA) ); WM( EA,D );          } /* SET  1,D=(XY+o)  */
INLINE void xycb_cb(void) { E = SET(1, RM(EA) ); WM( EA,E );          } /* SET  1,E=(XY+o)  */
INLINE void xycb_cc(void) { H = SET(1, RM(EA) ); WM( EA,H );          } /* SET  1,H=(XY+o)  */
INLINE void xycb_cd(void) { L = SET(1, RM(EA) ); WM( EA,L );          } /* SET  1,L=(XY+o)  */
INLINE void xycb_ce(void) { WM( EA, SET(1,RM(EA)) );                  } /* SET  1,(XY+o)    */
INLINE void xycb_cf(void) { A = SET(1, RM(EA) ); WM( EA,A );          } /* SET  1,A=(XY+o)  */

INLINE void xycb_d0(void) { B = SET(2, RM(EA) ); WM( EA,B );          } /* SET  2,B=(XY+o)  */
INLINE void xycb_d1(void) { C = SET(2, RM(EA) ); WM( EA,C );          } /* SET  2,C=(XY+o)  */
INLINE void xycb_d2(void) { D = SET(2, RM(EA) ); WM( EA,D );          } /* SET  2,D=(XY+o)  */
INLINE void xycb_d3(void) { E = SET(2, RM(EA) ); WM( EA,E );          } /* SET  2,E=(XY+o)  */
INLINE void xycb_d4(void) { H = SET(2, RM(EA) ); WM( EA,H );          } /* SET  2,H=(XY+o)  */
INLINE void xycb_d5(void) { L = SET(2, RM(EA) ); WM( EA,L );          } /* SET  2,L=(XY+o)  */
INLINE void xycb_d6(void) { WM( EA, SET(2,RM(EA)) );                  } /* SET  2,(XY+o)    */
INLINE void xycb_d7(void) { A = SET(2, RM(EA) ); WM( EA,A );          } /* SET  2,A=(XY+o)  */

INLINE void xycb_d8(void) { B = SET(3, RM(EA) ); WM( EA,B );          } /* SET  3,B=(XY+o)  */
INLINE void xycb_d9(void) { C = SET(3, RM(EA) ); WM( EA,C );          } /* SET  3,C=(XY+o)  */
INLINE void xycb_da(void) { D = SET(3, RM(EA) ); WM( EA,D );          } /* SET  3,D=(XY+o)  */
INLINE void xycb_db(void) { E = SET(3, RM(EA) ); WM( EA,E );          } /* SET  3,E=(XY+o)  */
INLINE void xycb_dc(void) { H = SET(3, RM(EA) ); WM( EA,H );          } /* SET  3,H=(XY+o)  */
INLINE void xycb_dd(void) { L = SET(3, RM(EA) ); WM( EA,L );          } /* SET  3,L=(XY+o)  */
INLINE void xycb_de(void) { WM( EA, SET(3,RM(EA)) );                  } /* SET  3,(XY+o)    */
INLINE void xycb_df(void) { A = SET(3, RM(EA) ); WM( EA,A );          } /* SET  3,A=(XY+o)  */

INLINE void xycb_e0(void) { B = SET(4, RM(EA) ); WM( EA,B );          } /* SET  4,B=(XY+o)  */
INLINE void xycb_e1(void) { C = SET(4, RM(EA) ); WM( EA,C );          } /* SET  4,C=(XY+o)  */
INLINE void xycb_e2(void) { D = SET(4, RM(EA) ); WM( EA,D );          } /* SET  4,D=(XY+o)  */
INLINE void xycb_e3(void) { E = SET(4, RM(EA) ); WM( EA,E );          } /* SET  4,E=(XY+o)  */
INLINE void xycb_e4(void) { H = SET(4, RM(EA) ); WM( EA,H );          } /* SET  4,H=(XY+o)  */
INLINE void xycb_e5(void) { L = SET(4, RM(EA) ); WM( EA,L );          } /* SET  4,L=(XY+o)  */
INLINE void xycb_e6(void) { WM( EA, SET(4,RM(EA)) );                  } /* SET  4,(XY+o)    */
INLINE void xycb_e7(void) { A = SET(4, RM(EA) ); WM( EA,A );          } /* SET  4,A=(XY+o)  */

INLINE void xycb_e8(void) { B = SET(5, RM(EA) ); WM( EA,B );          } /* SET  5,B=(XY+o)  */
INLINE void xycb_e9(void) { C = SET(5, RM(EA) ); WM( EA,C );          } /* SET  5,C=(XY+o)  */
INLINE void xycb_ea(void) { D = SET(5, RM(EA) ); WM( EA,D );          } /* SET  5,D=(XY+o)  */
INLINE void xycb_eb(void) { E = SET(5, RM(EA) ); WM( EA,E );          } /* SET  5,E=(XY+o)  */
INLINE void xycb_ec(void) { H = SET(5, RM(EA) ); WM( EA,H );          } /* SET  5,H=(XY+o)  */
INLINE void xycb_ed(void) { L = SET(5, RM(EA) ); WM( EA,L );          } /* SET  5,L=(XY+o)  */
INLINE void xycb_ee(void) { WM( EA, SET(5,RM(EA)) );                  } /* SET  5,(XY+o)    */
INLINE void xycb_ef(void) { A = SET(5, RM(EA) ); WM( EA,A );          } /* SET  5,A=(XY+o)  */

INLINE void xycb_f0(void) { B = SET(6, RM(EA) ); WM( EA,B );          } /* SET  6,B=(XY+o)  */
INLINE void xycb_f1(void) { C = SET(6, RM(EA) ); WM( EA,C );          } /* SET  6,C=(XY+o)  */
INLINE void xycb_f2(void) { D = SET(6, RM(EA) ); WM( EA,D );          } /* SET  6,D=(XY+o)  */
INLINE void xycb_f3(void) { E = SET(6, RM(EA) ); WM( EA,E );          } /* SET  6,E=(XY+o)  */
INLINE void xycb_f4(void) { H = SET(6, RM(EA) ); WM( EA,H );          } /* SET  6,H=(XY+o)  */
INLINE void xycb_f5(void) { L = SET(6, RM(EA) ); WM( EA,L );          } /* SET  6,L=(XY+o)  */
INLINE void xycb_f6(void) { WM( EA, SET(6,RM(EA)) );                  } /* SET  6,(XY+o)    */
INLINE void xycb_f7(void) { A = SET(6, RM(EA) ); WM( EA,A );          } /* SET  6,A=(XY+o)  */

INLINE void xycb_f8(void) { B = SET(7, RM(EA) ); WM( EA,B );          } /* SET  7,B=(XY+o)  */
INLINE void xycb_f9(void) { C = SET(7, RM(EA) ); WM( EA,C );          } /* SET  7,C=(XY+o)  */
INLINE void xycb_fa(void) { D = SET(7, RM(EA) ); WM( EA,D );          } /* SET  7,D=(XY+o)  */
INLINE void xycb_fb(void) { E = SET(7, RM(EA) ); WM( EA,E );          } /* SET  7,E=(XY+o)  */
INLINE void xycb_fc(void) { H = SET(7, RM(EA) ); WM( EA,H );          } /* SET  7,H=(XY+o)  */
INLINE void xycb_fd(void) { L = SET(7, RM(EA) ); WM( EA,L );          } /* SET  7,L=(XY+o)  */
INLINE void xycb_fe(void) { WM( EA, SET(7,RM(EA)) );                  } /* SET  7,(XY+o)    */
INLINE void xycb_ff(void) { A = SET(7, RM(EA) ); WM( EA,A );          } /* SET  7,A=(XY+o)  */

INLINE void illegal_1(void) {
#if VERBOSE
  logerror("Z80 #%d ill. opcode $%02x $%02x\n",
      cpu_getactivecpu(), cpu_readop((PCD-1)&0xffff), cpu_readop(PCD));
#endif
}
/**********************************************************
 * IX register related opcodes (DD prefix)
 **********************************************************/
INLINE void dd_00(void) { illegal_1(); op_00();                             } /* DB   DD       */
INLINE void dd_01(void) { illegal_1(); op_01();                             } /* DB   DD       */
INLINE void dd_02(void) { illegal_1(); op_02();                             } /* DB   DD       */
INLINE void dd_03(void) { illegal_1(); op_03();                             } /* DB   DD       */
INLINE void dd_04(void) { illegal_1(); op_04();                             } /* DB   DD       */
INLINE void dd_05(void) { illegal_1(); op_05();                             } /* DB   DD       */
INLINE void dd_06(void) { illegal_1(); op_06();                             } /* DB   DD       */
INLINE void dd_07(void) { illegal_1(); op_07();                             } /* DB   DD       */

INLINE void dd_08(void) { illegal_1(); op_08();                             } /* DB   DD       */
INLINE void dd_09(void) { ADD16(ix,bc);                                     } /* ADD  IX,BC    */
INLINE void dd_0a(void) { illegal_1(); op_0a();                             } /* DB   DD       */
INLINE void dd_0b(void) { illegal_1(); op_0b();                             } /* DB   DD       */
INLINE void dd_0c(void) { illegal_1(); op_0c();                             } /* DB   DD       */
INLINE void dd_0d(void) { illegal_1(); op_0d();                             } /* DB   DD       */
INLINE void dd_0e(void) { illegal_1(); op_0e();                             } /* DB   DD       */
INLINE void dd_0f(void) { illegal_1(); op_0f();                             } /* DB   DD       */

INLINE void dd_10(void) { illegal_1(); op_10();                             } /* DB   DD       */
INLINE void dd_11(void) { illegal_1(); op_11();                             } /* DB   DD       */
INLINE void dd_12(void) { illegal_1(); op_12();                             } /* DB   DD       */
INLINE void dd_13(void) { illegal_1(); op_13();                             } /* DB   DD       */
INLINE void dd_14(void) { illegal_1(); op_14();                             } /* DB   DD       */
INLINE void dd_15(void) { illegal_1(); op_15();                             } /* DB   DD       */
INLINE void dd_16(void) { illegal_1(); op_16();                             } /* DB   DD       */
INLINE void dd_17(void) { illegal_1(); op_17();                             } /* DB   DD       */

INLINE void dd_18(void) { illegal_1(); op_18();                             } /* DB   DD       */
INLINE void dd_19(void) { ADD16(ix,de);                                     } /* ADD  IX,DE    */
INLINE void dd_1a(void) { illegal_1(); op_1a();                             } /* DB   DD       */
INLINE void dd_1b(void) { illegal_1(); op_1b();                             } /* DB   DD       */
INLINE void dd_1c(void) { illegal_1(); op_1c();                             } /* DB   DD       */
INLINE void dd_1d(void) { illegal_1(); op_1d();                             } /* DB   DD       */
INLINE void dd_1e(void) { illegal_1(); op_1e();                             } /* DB   DD       */
INLINE void dd_1f(void) { illegal_1(); op_1f();                             } /* DB   DD       */

INLINE void dd_20(void) { illegal_1(); op_20();                             } /* DB   DD       */
INLINE void dd_21(void) { IX = ARG16();                                     } /* LD   IX,w     */
INLINE void dd_22(void) { EA = ARG16(); WM16( EA, &Z80.ix ); WZ = EA+1;     } /* LD   (w),IX   */
INLINE void dd_23(void) { IX++;                                             } /* INC  IX       */
INLINE void dd_24(void) { HX = INC(HX);                                     } /* INC  HX       */
INLINE void dd_25(void) { HX = DEC(HX);                                     } /* DEC  HX       */
INLINE void dd_26(void) { HX = ARG();                                       } /* LD   HX,n     */
INLINE void dd_27(void) { illegal_1(); op_27();                             } /* DB   DD       */

INLINE void dd_28(void) { illegal_1(); op_28();                             } /* DB   DD       */
INLINE void dd_29(void) { ADD16(ix,ix);                                     } /* ADD  IX,IX    */
INLINE void dd_2a(void) { EA = ARG16(); RM16( EA, &Z80.ix ); WZ = EA+1;     } /* LD   IX,(w)   */
INLINE void dd_2b(void) { IX--;                                             } /* DEC  IX       */
INLINE void dd_2c(void) { LX = INC(LX);                                     } /* INC  LX       */
INLINE void dd_2d(void) { LX = DEC(LX);                                     } /* DEC  LX       */
INLINE void dd_2e(void) { LX = ARG();                                       } /* LD   LX,n     */
INLINE void dd_2f(void) { illegal_1(); op_2f();                             } /* DB   DD       */

INLINE void dd_30(void) { illegal_1(); op_30();                             } /* DB   DD       */
INLINE void dd_31(void) { illegal_1(); op_31();                             } /* DB   DD       */
INLINE void dd_32(void) { illegal_1(); op_32();                             } /* DB   DD       */
INLINE void dd_33(void) { illegal_1(); op_33();                             } /* DB   DD       */
INLINE void dd_34(void) { EAX; WM( EA, INC(RM(EA)) );                       } /* INC  (IX+o)   */
INLINE void dd_35(void) { EAX; WM( EA, DEC(RM(EA)) );                       } /* DEC  (IX+o)   */
INLINE void dd_36(void) { EAX; WM( EA, ARG() );                             } /* LD   (IX+o),n */
INLINE void dd_37(void) { illegal_1(); op_37();                             } /* DB   DD       */

INLINE void dd_38(void) { illegal_1(); op_38();                             } /* DB   DD       */
INLINE void dd_39(void) { ADD16(ix,sp);                                     } /* ADD  IX,SP    */
INLINE void dd_3a(void) { illegal_1(); op_3a();                             } /* DB   DD       */
INLINE void dd_3b(void) { illegal_1(); op_3b();                             } /* DB   DD       */
INLINE void dd_3c(void) { illegal_1(); op_3c();                             } /* DB   DD       */
INLINE void dd_3d(void) { illegal_1(); op_3d();                             } /* DB   DD       */
INLINE void dd_3e(void) { illegal_1(); op_3e();                             } /* DB   DD       */
INLINE void dd_3f(void) { illegal_1(); op_3f();                             } /* DB   DD       */

INLINE void dd_40(void) { illegal_1(); op_40();                             } /* DB   DD       */
INLINE void dd_41(void) { illegal_1(); op_41();                             } /* DB   DD       */
INLINE void dd_42(void) { illegal_1(); op_42();                             } /* DB   DD       */
INLINE void dd_43(void) { illegal_1(); op_43();                             } /* DB   DD       */
INLINE void dd_44(void) { B = HX;                                           } /* LD   B,HX     */
INLINE void dd_45(void) { B = LX;                                           } /* LD   B,LX     */
INLINE void dd_46(void) { EAX; B = RM(EA);                                  } /* LD   B,(IX+o) */
INLINE void dd_47(void) { illegal_1(); op_47();                             } /* DB   DD       */

INLINE void dd_48(void) { illegal_1(); op_48();                             } /* DB   DD       */
INLINE void dd_49(void) { illegal_1(); op_49();                             } /* DB   DD       */
INLINE void dd_4a(void) { illegal_1(); op_4a();                             } /* DB   DD       */
INLINE void dd_4b(void) { illegal_1(); op_4b();                             } /* DB   DD       */
INLINE void dd_4c(void) { C = HX;                                           } /* LD   C,HX     */
INLINE void dd_4d(void) { C = LX;                                           } /* LD   C,LX     */
INLINE void dd_4e(void) { EAX; C = RM(EA);                                  } /* LD   C,(IX+o) */
INLINE void dd_4f(void) { illegal_1(); op_4f();                             } /* DB   DD       */

INLINE void dd_50(void) { illegal_1(); op_50();                             } /* DB   DD       */
INLINE void dd_51(void) { illegal_1(); op_51();                             } /* DB   DD       */
INLINE void dd_52(void) { illegal_1(); op_52();                             } /* DB   DD       */
INLINE void dd_53(void) { illegal_1(); op_53();                             } /* DB   DD       */
INLINE void dd_54(void) { D = HX;                                           } /* LD   D,HX     */
INLINE void dd_55(void) { D = LX;                                           } /* LD   D,LX     */
INLINE void dd_56(void) { EAX; D = RM(EA);                                  } /* LD   D,(IX+o) */
INLINE void dd_57(void) { illegal_1(); op_57();                             } /* DB   DD       */

INLINE void dd_58(void) { illegal_1(); op_58();                             } /* DB   DD       */
INLINE void dd_59(void) { illegal_1(); op_59();                             } /* DB   DD       */
INLINE void dd_5a(void) { illegal_1(); op_5a();                             } /* DB   DD       */
INLINE void dd_5b(void) { illegal_1(); op_5b();                             } /* DB   DD       */
INLINE void dd_5c(void) { E = HX;                                           } /* LD   E,HX     */
INLINE void dd_5d(void) { E = LX;                                           } /* LD   E,LX     */
INLINE void dd_5e(void) { EAX; E = RM(EA);                                  } /* LD   E,(IX+o) */
INLINE void dd_5f(void) { illegal_1(); op_5f();                             } /* DB   DD       */

INLINE void dd_60(void) { HX = B;                                           } /* LD   HX,B     */
INLINE void dd_61(void) { HX = C;                                           } /* LD   HX,C     */
INLINE void dd_62(void) { HX = D;                                           } /* LD   HX,D     */
INLINE void dd_63(void) { HX = E;                                           } /* LD   HX,E     */
INLINE void dd_64(void) {                                                   } /* LD   HX,HX    */
INLINE void dd_65(void) { HX = LX;                                          } /* LD   HX,LX    */
INLINE void dd_66(void) { EAX; H = RM(EA);                                  } /* LD   H,(IX+o) */
INLINE void dd_67(void) { HX = A;                                           } /* LD   HX,A     */

INLINE void dd_68(void) { LX = B;                                           } /* LD   LX,B     */
INLINE void dd_69(void) { LX = C;                                           } /* LD   LX,C     */
INLINE void dd_6a(void) { LX = D;                                           } /* LD   LX,D     */
INLINE void dd_6b(void) { LX = E;                                           } /* LD   LX,E     */
INLINE void dd_6c(void) { LX = HX;                                          } /* LD   LX,HX    */
INLINE void dd_6d(void) {                                                   } /* LD   LX,LX    */
INLINE void dd_6e(void) { EAX; L = RM(EA);                                  } /* LD   L,(IX+o) */
INLINE void dd_6f(void) { LX = A;                                           } /* LD   LX,A     */

INLINE void dd_70(void) { EAX; WM( EA, B );                                 } /* LD   (IX+o),B */
INLINE void dd_71(void) { EAX; WM( EA, C );                                 } /* LD   (IX+o),C */
INLINE void dd_72(void) { EAX; WM( EA, D );                                 } /* LD   (IX+o),D */
INLINE void dd_73(void) { EAX; WM( EA, E );                                 } /* LD   (IX+o),E */
INLINE void dd_74(void) { EAX; WM( EA, H );                                 } /* LD   (IX+o),H */
INLINE void dd_75(void) { EAX; WM( EA, L );                                 } /* LD   (IX+o),L */
INLINE void dd_76(void) { illegal_1(); op_76();                             } /* DB   DD       */
INLINE void dd_77(void) { EAX; WM( EA, A );                                 } /* LD   (IX+o),A */

INLINE void dd_78(void) { illegal_1(); op_78();                             } /* DB   DD       */
INLINE void dd_79(void) { illegal_1(); op_79();                             } /* DB   DD       */
INLINE void dd_7a(void) { illegal_1(); op_7a();                             } /* DB   DD       */
INLINE void dd_7b(void) { illegal_1(); op_7b();                             } /* DB   DD       */
INLINE void dd_7c(void) { A = HX;                                           } /* LD   A,HX     */
INLINE void dd_7d(void) { A = LX;                                           } /* LD   A,LX     */
INLINE void dd_7e(void) { EAX; A = RM(EA);                                  } /* LD   A,(IX+o) */
INLINE void dd_7f(void) { illegal_1(); op_7f();                             } /* DB   DD       */

INLINE void dd_80(void) { illegal_1(); op_80();                             } /* DB   DD       */
INLINE void dd_81(void) { illegal_1(); op_81();                             } /* DB   DD       */
INLINE void dd_82(void) { illegal_1(); op_82();                             } /* DB   DD       */
INLINE void dd_83(void) { illegal_1(); op_83();                             } /* DB   DD       */
INLINE void dd_84(void) { ADD(HX);                                          } /* ADD  A,HX     */
INLINE void dd_85(void) { ADD(LX);                                          } /* ADD  A,LX     */
INLINE void dd_86(void) { EAX; ADD(RM(EA));                                 } /* ADD  A,(IX+o) */
INLINE void dd_87(void) { illegal_1(); op_87();                             } /* DB   DD       */

INLINE void dd_88(void) { illegal_1(); op_88();                             } /* DB   DD       */
INLINE void dd_89(void) { illegal_1(); op_89();                             } /* DB   DD       */
INLINE void dd_8a(void) { illegal_1(); op_8a();                             } /* DB   DD       */
INLINE void dd_8b(void) { illegal_1(); op_8b();                             } /* DB   DD       */
INLINE void dd_8c(void) { ADC(HX);                                          } /* ADC  A,HX     */
INLINE void dd_8d(void) { ADC(LX);                                          } /* ADC  A,LX     */
INLINE void dd_8e(void) { EAX; ADC(RM(EA));                                 } /* ADC  A,(IX+o) */
INLINE void dd_8f(void) { illegal_1(); op_8f();                             } /* DB   DD       */

INLINE void dd_90(void) { illegal_1(); op_90();                             } /* DB   DD       */
INLINE void dd_91(void) { illegal_1(); op_91();                             } /* DB   DD       */
INLINE void dd_92(void) { illegal_1(); op_92();                             } /* DB   DD       */
INLINE void dd_93(void) { illegal_1(); op_93();                             } /* DB   DD       */
INLINE void dd_94(void) { SUB(HX);                                          } /* SUB  HX       */
INLINE void dd_95(void) { SUB(LX);                                          } /* SUB  LX       */
INLINE void dd_96(void) { EAX; SUB(RM(EA));                                 } /* SUB  (IX+o)   */
INLINE void dd_97(void) { illegal_1(); op_97();                             } /* DB   DD       */

INLINE void dd_98(void) { illegal_1(); op_98();                             } /* DB   DD       */
INLINE void dd_99(void) { illegal_1(); op_99();                             } /* DB   DD       */
INLINE void dd_9a(void) { illegal_1(); op_9a();                             } /* DB   DD       */
INLINE void dd_9b(void) { illegal_1(); op_9b();                             } /* DB   DD       */
INLINE void dd_9c(void) { SBC(HX);                                          } /* SBC  A,HX     */
INLINE void dd_9d(void) { SBC(LX);                                          } /* SBC  A,LX     */
INLINE void dd_9e(void) { EAX; SBC(RM(EA));                                 } /* SBC  A,(IX+o) */
INLINE void dd_9f(void) { illegal_1(); op_9f();                             } /* DB   DD       */

INLINE void dd_a0(void) { illegal_1(); op_a0();                             } /* DB   DD       */
INLINE void dd_a1(void) { illegal_1(); op_a1();                             } /* DB   DD       */
INLINE void dd_a2(void) { illegal_1(); op_a2();                             } /* DB   DD       */
INLINE void dd_a3(void) { illegal_1(); op_a3();                             } /* DB   DD       */
INLINE void dd_a4(void) { AND(HX);                                          } /* AND  HX       */
INLINE void dd_a5(void) { AND(LX);                                          } /* AND  LX       */
INLINE void dd_a6(void) { EAX; AND(RM(EA));                                 } /* AND  (IX+o)   */
INLINE void dd_a7(void) { illegal_1(); op_a7();                             } /* DB   DD       */

INLINE void dd_a8(void) { illegal_1(); op_a8();                             } /* DB   DD       */
INLINE void dd_a9(void) { illegal_1(); op_a9();                             } /* DB   DD       */
INLINE void dd_aa(void) { illegal_1(); op_aa();                             } /* DB   DD       */
INLINE void dd_ab(void) { illegal_1(); op_ab();                             } /* DB   DD       */
INLINE void dd_ac(void) { XOR(HX);                                          } /* XOR  HX       */
INLINE void dd_ad(void) { XOR(LX);                                          } /* XOR  LX       */
INLINE void dd_ae(void) { EAX; XOR(RM(EA));                                 } /* XOR  (IX+o)   */
INLINE void dd_af(void) { illegal_1(); op_af();                             } /* DB   DD       */

INLINE void dd_b0(void) { illegal_1(); op_b0();                             } /* DB   DD       */
INLINE void dd_b1(void) { illegal_1(); op_b1();                             } /* DB   DD       */
INLINE void dd_b2(void) { illegal_1(); op_b2();                             } /* DB   DD       */
INLINE void dd_b3(void) { illegal_1(); op_b3();                             } /* DB   DD       */
INLINE void dd_b4(void) { OR(HX);                                           } /* OR   HX       */
INLINE void dd_b5(void) { OR(LX);                                           } /* OR   LX       */
INLINE void dd_b6(void) { EAX; OR(RM(EA));                                  } /* OR   (IX+o)   */
INLINE void dd_b7(void) { illegal_1(); op_b7();                             } /* DB   DD       */

INLINE void dd_b8(void) { illegal_1(); op_b8();                             } /* DB   DD       */
INLINE void dd_b9(void) { illegal_1(); op_b9();                             } /* DB   DD       */
INLINE void dd_ba(void) { illegal_1(); op_ba();                             } /* DB   DD       */
INLINE void dd_bb(void) { illegal_1(); op_bb();                             } /* DB   DD       */
INLINE void dd_bc(void) { CP(HX);                                           } /* CP   HX       */
INLINE void dd_bd(void) { CP(LX);                                           } /* CP   LX       */
INLINE void dd_be(void) { EAX; CP(RM(EA));                                  } /* CP   (IX+o)   */
INLINE void dd_bf(void) { illegal_1(); op_bf();                             } /* DB   DD       */

INLINE void dd_c0(void) { illegal_1(); op_c0();                             } /* DB   DD       */
INLINE void dd_c1(void) { illegal_1(); op_c1();                             } /* DB   DD       */
INLINE void dd_c2(void) { illegal_1(); op_c2();                             } /* DB   DD       */
INLINE void dd_c3(void) { illegal_1(); op_c3();                             } /* DB   DD       */
INLINE void dd_c4(void) { illegal_1(); op_c4();                             } /* DB   DD       */
INLINE void dd_c5(void) { illegal_1(); op_c5();                             } /* DB   DD       */
INLINE void dd_c6(void) { illegal_1(); op_c6();                             } /* DB   DD       */
INLINE void dd_c7(void) { illegal_1(); op_c7();                             } /* DB   DD       */

INLINE void dd_c8(void) { illegal_1(); op_c8();                             } /* DB   DD       */
INLINE void dd_c9(void) { illegal_1(); op_c9();                             } /* DB   DD       */
INLINE void dd_ca(void) { illegal_1(); op_ca();                             } /* DB   DD       */
INLINE void dd_cb(void) { EAX; EXEC(xycb,ARG());                            } /* **** DD CB xx */
INLINE void dd_cc(void) { illegal_1(); op_cc();                             } /* DB   DD       */
INLINE void dd_cd(void) { illegal_1(); op_cd();                             } /* DB   DD       */
INLINE void dd_ce(void) { illegal_1(); op_ce();                             } /* DB   DD       */
INLINE void dd_cf(void) { illegal_1(); op_cf();                             } /* DB   DD       */

INLINE void dd_d0(void) { illegal_1(); op_d0();                             } /* DB   DD       */
INLINE void dd_d1(void) { illegal_1(); op_d1();                             } /* DB   DD       */
INLINE void dd_d2(void) { illegal_1(); op_d2();                             } /* DB   DD       */
INLINE void dd_d3(void) { illegal_1(); op_d3();                             } /* DB   DD       */
INLINE void dd_d4(void) { illegal_1(); op_d4();                             } /* DB   DD       */
INLINE void dd_d5(void) { illegal_1(); op_d5();                             } /* DB   DD       */
INLINE void dd_d6(void) { illegal_1(); op_d6();                             } /* DB   DD       */
INLINE void dd_d7(void) { illegal_1(); op_d7();                             } /* DB   DD       */

INLINE void dd_d8(void) { illegal_1(); op_d8();                             } /* DB   DD       */
INLINE void dd_d9(void) { illegal_1(); op_d9();                             } /* DB   DD       */
INLINE void dd_da(void) { illegal_1(); op_da();                             } /* DB   DD       */
INLINE void dd_db(void) { illegal_1(); op_db();                             } /* DB   DD       */
INLINE void dd_dc(void) { illegal_1(); op_dc();                             } /* DB   DD       */
INLINE void dd_dd(void) { EXEC(dd,ROP());                                   } /* **** DD DD xx */
INLINE void dd_de(void) { illegal_1(); op_de();                             } /* DB   DD       */
INLINE void dd_df(void) { illegal_1(); op_df();                             } /* DB   DD       */

INLINE void dd_e0(void) { illegal_1(); op_e0();                             } /* DB   DD       */
INLINE void dd_e1(void) { POP( ix );                                        } /* POP  IX       */
INLINE void dd_e2(void) { illegal_1(); op_e2();                             } /* DB   DD       */
INLINE void dd_e3(void) { EXSP( ix );                                       } /* EX   (SP),IX  */
INLINE void dd_e4(void) { illegal_1(); op_e4();                             } /* DB   DD       */
INLINE void dd_e5(void) { PUSH( ix );                                       } /* PUSH IX       */
INLINE void dd_e6(void) { illegal_1(); op_e6();                             } /* DB   DD       */
INLINE void dd_e7(void) { illegal_1(); op_e7();                             } /* DB   DD       */

INLINE void dd_e8(void) { illegal_1(); op_e8();                             } /* DB   DD       */
INLINE void dd_e9(void) { PC = IX;                                          } /* JP   (IX)     */
INLINE void dd_ea(void) { illegal_1(); op_ea();                             } /* DB   DD       */
INLINE void dd_eb(void) { illegal_1(); op_eb();                             } /* DB   DD       */
INLINE void dd_ec(void) { illegal_1(); op_ec();                             } /* DB   DD       */
INLINE void dd_ed(void) { illegal_1(); op_ed();                             } /* DB   DD       */
INLINE void dd_ee(void) { illegal_1(); op_ee();                             } /* DB   DD       */
INLINE void dd_ef(void) { illegal_1(); op_ef();                             } /* DB   DD       */

INLINE void dd_f0(void) { illegal_1(); op_f0();                             } /* DB   DD       */
INLINE void dd_f1(void) { illegal_1(); op_f1();                             } /* DB   DD       */
INLINE void dd_f2(void) { illegal_1(); op_f2();                             } /* DB   DD       */
INLINE void dd_f3(void) { illegal_1(); op_f3();                             } /* DB   DD       */
INLINE void dd_f4(void) { illegal_1(); op_f4();                             } /* DB   DD       */
INLINE void dd_f5(void) { illegal_1(); op_f5();                             } /* DB   DD       */
INLINE void dd_f6(void) { illegal_1(); op_f6();                             } /* DB   DD       */
INLINE void dd_f7(void) { illegal_1(); op_f7();                             } /* DB   DD       */

INLINE void dd_f8(void) { illegal_1(); op_f8();                             } /* DB   DD       */
INLINE void dd_f9(void) { SP = IX;                                          } /* LD   SP,IX    */
INLINE void dd_fa(void) { illegal_1(); op_fa();                             } /* DB   DD       */
INLINE void dd_fb(void) { illegal_1(); op_fb();                             } /* DB   DD       */
INLINE void dd_fc(void) { illegal_1(); op_fc();                             } /* DB   DD       */
INLINE void dd_fd(void) { EXEC(fd,ROP());                                   } /* **** DD FD xx */
INLINE void dd_fe(void) { illegal_1(); op_fe();                             } /* DB   DD       */
INLINE void dd_ff(void) { illegal_1(); op_ff();                             } /* DB   DD       */

/**********************************************************
 * IY register related opcodes (FD prefix)
 **********************************************************/
INLINE void fd_00(void) { illegal_1(); op_00();                             } /* DB   FD       */
INLINE void fd_01(void) { illegal_1(); op_01();                             } /* DB   FD       */
INLINE void fd_02(void) { illegal_1(); op_02();                             } /* DB   FD       */
INLINE void fd_03(void) { illegal_1(); op_03();                             } /* DB   FD       */
INLINE void fd_04(void) { illegal_1(); op_04();                             } /* DB   FD       */
INLINE void fd_05(void) { illegal_1(); op_05();                             } /* DB   FD       */
INLINE void fd_06(void) { illegal_1(); op_06();                             } /* DB   FD       */
INLINE void fd_07(void) { illegal_1(); op_07();                             } /* DB   FD       */

INLINE void fd_08(void) { illegal_1(); op_08();                             } /* DB   FD       */
INLINE void fd_09(void) { ADD16(iy,bc);                                     } /* ADD  IY,BC    */
INLINE void fd_0a(void) { illegal_1(); op_0a();                             } /* DB   FD       */
INLINE void fd_0b(void) { illegal_1(); op_0b();                             } /* DB   FD       */
INLINE void fd_0c(void) { illegal_1(); op_0c();                             } /* DB   FD       */
INLINE void fd_0d(void) { illegal_1(); op_0d();                             } /* DB   FD       */
INLINE void fd_0e(void) { illegal_1(); op_0e();                             } /* DB   FD       */
INLINE void fd_0f(void) { illegal_1(); op_0f();                             } /* DB   FD       */

INLINE void fd_10(void) { illegal_1(); op_10();                             } /* DB   FD       */
INLINE void fd_11(void) { illegal_1(); op_11();                             } /* DB   FD       */
INLINE void fd_12(void) { illegal_1(); op_12();                             } /* DB   FD       */
INLINE void fd_13(void) { illegal_1(); op_13();                             } /* DB   FD       */
INLINE void fd_14(void) { illegal_1(); op_14();                             } /* DB   FD       */
INLINE void fd_15(void) { illegal_1(); op_15();                             } /* DB   FD       */
INLINE void fd_16(void) { illegal_1(); op_16();                             } /* DB   FD       */
INLINE void fd_17(void) { illegal_1(); op_17();                             } /* DB   FD       */

INLINE void fd_18(void) { illegal_1(); op_18();                             } /* DB   FD       */
INLINE void fd_19(void) { ADD16(iy,de);                                     } /* ADD  IY,DE    */
INLINE void fd_1a(void) { illegal_1(); op_1a();                             } /* DB   FD       */
INLINE void fd_1b(void) { illegal_1(); op_1b();                             } /* DB   FD       */
INLINE void fd_1c(void) { illegal_1(); op_1c();                             } /* DB   FD       */
INLINE void fd_1d(void) { illegal_1(); op_1d();                             } /* DB   FD       */
INLINE void fd_1e(void) { illegal_1(); op_1e();                             } /* DB   FD       */
INLINE void fd_1f(void) { illegal_1(); op_1f();                             } /* DB   FD       */

INLINE void fd_20(void) { illegal_1(); op_20();                             } /* DB   FD       */
INLINE void fd_21(void) { IY = ARG16();                                     } /* LD   IY,w     */
INLINE void fd_22(void) { EA = ARG16(); WM16( EA, &Z80.iy ); WZ = EA+1;     } /* LD   (w),IY   */
INLINE void fd_23(void) { IY++;                                             } /* INC  IY       */
INLINE void fd_24(void) { HY = INC(HY);                                     } /* INC  HY       */
INLINE void fd_25(void) { HY = DEC(HY);                                     } /* DEC  HY       */
INLINE void fd_26(void) { HY = ARG();                                       } /* LD   HY,n     */
INLINE void fd_27(void) { illegal_1(); op_27();                             } /* DB   FD       */

INLINE void fd_28(void) { illegal_1(); op_28();                             } /* DB   FD       */
INLINE void fd_29(void) { ADD16(iy,iy);                                     } /* ADD  IY,IY    */
INLINE void fd_2a(void) { EA = ARG16(); RM16( EA, &Z80.iy ); WZ = EA+1;     } /* LD   IY,(w)   */
INLINE void fd_2b(void) { IY--;                                             } /* DEC  IY       */
INLINE void fd_2c(void) { LY = INC(LY);                                     } /* INC  LY       */
INLINE void fd_2d(void) { LY = DEC(LY);                                     } /* DEC  LY       */
INLINE void fd_2e(void) { LY = ARG();                                       } /* LD   LY,n     */
INLINE void fd_2f(void) { illegal_1(); op_2f();                             } /* DB   FD       */

INLINE void fd_30(void) { illegal_1(); op_30();                             } /* DB   FD       */
INLINE void fd_31(void) { illegal_1(); op_31();                             } /* DB   FD       */
INLINE void fd_32(void) { illegal_1(); op_32();                             } /* DB   FD       */
INLINE void fd_33(void) { illegal_1(); op_33();                             } /* DB   FD       */
INLINE void fd_34(void) { EAY; WM( EA, INC(RM(EA)) );                       } /* INC  (IY+o)   */
INLINE void fd_35(void) { EAY; WM( EA, DEC(RM(EA)) );                       } /* DEC  (IY+o)   */
INLINE void fd_36(void) { EAY; WM( EA, ARG() );                             } /* LD   (IY+o),n */
INLINE void fd_37(void) { illegal_1(); op_37();                             } /* DB   FD       */

INLINE void fd_38(void) { illegal_1(); op_38();                             } /* DB   FD       */
INLINE void fd_39(void) { ADD16(iy,sp);                                     } /* ADD  IY,SP    */
INLINE void fd_3a(void) { illegal_1(); op_3a();                             } /* DB   FD       */
INLINE void fd_3b(void) { illegal_1(); op_3b();                             } /* DB   FD       */
INLINE void fd_3c(void) { illegal_1(); op_3c();                             } /* DB   FD       */
INLINE void fd_3d(void) { illegal_1(); op_3d();                             } /* DB   FD       */
INLINE void fd_3e(void) { illegal_1(); op_3e();                             } /* DB   FD       */
INLINE void fd_3f(void) { illegal_1(); op_3f();                             } /* DB   FD       */

INLINE void fd_40(void) { illegal_1(); op_40();                             } /* DB   FD       */
INLINE void fd_41(void) { illegal_1(); op_41();                             } /* DB   FD       */
INLINE void fd_42(void) { illegal_1(); op_42();                             } /* DB   FD       */
INLINE void fd_43(void) { illegal_1(); op_43();                             } /* DB   FD       */
INLINE void fd_44(void) { B = HY;                                           } /* LD   B,HY     */
INLINE void fd_45(void) { B = LY;                                           } /* LD   B,LY     */
INLINE void fd_46(void) { EAY; B = RM(EA);                                  } /* LD   B,(IY+o) */
INLINE void fd_47(void) { illegal_1(); op_47();                             } /* DB   FD       */

INLINE void fd_48(void) { illegal_1(); op_48();                             } /* DB   FD       */
INLINE void fd_49(void) { illegal_1(); op_49();                             } /* DB   FD       */
INLINE void fd_4a(void) { illegal_1(); op_4a();                             } /* DB   FD       */
INLINE void fd_4b(void) { illegal_1(); op_4b();                             } /* DB   FD       */
INLINE void fd_4c(void) { C = HY;                                           } /* LD   C,HY     */
INLINE void fd_4d(void) { C = LY;                                           } /* LD   C,LY     */
INLINE void fd_4e(void) { EAY; C = RM(EA);                                  } /* LD   C,(IY+o) */
INLINE void fd_4f(void) { illegal_1(); op_4f();                             } /* DB   FD       */

INLINE void fd_50(void) { illegal_1(); op_50();                             } /* DB   FD       */
INLINE void fd_51(void) { illegal_1(); op_51();                             } /* DB   FD       */
INLINE void fd_52(void) { illegal_1(); op_52();                             } /* DB   FD       */
INLINE void fd_53(void) { illegal_1(); op_53();                             } /* DB   FD       */
INLINE void fd_54(void) { D = HY;                                           } /* LD   D,HY     */
INLINE void fd_55(void) { D = LY;                                           } /* LD   D,LY     */
INLINE void fd_56(void) { EAY; D = RM(EA);                                  } /* LD   D,(IY+o) */
INLINE void fd_57(void) { illegal_1(); op_57();                             } /* DB   FD       */

INLINE void fd_58(void) { illegal_1(); op_58();                             } /* DB   FD       */
INLINE void fd_59(void) { illegal_1(); op_59();                             } /* DB   FD       */
INLINE void fd_5a(void) { illegal_1(); op_5a();                             } /* DB   FD       */
INLINE void fd_5b(void) { illegal_1(); op_5b();                             } /* DB   FD       */
INLINE void fd_5c(void) { E = HY;                                           } /* LD   E,HY     */
INLINE void fd_5d(void) { E = LY;                                           } /* LD   E,LY     */
INLINE void fd_5e(void) { EAY; E = RM(EA);                                  } /* LD   E,(IY+o) */
INLINE void fd_5f(void) { illegal_1(); op_5f();                             } /* DB   FD       */

INLINE void fd_60(void) { HY = B;                                           } /* LD   HY,B     */
INLINE void fd_61(void) { HY = C;                                           } /* LD   HY,C     */
INLINE void fd_62(void) { HY = D;                                           } /* LD   HY,D     */
INLINE void fd_63(void) { HY = E;                                           } /* LD   HY,E     */
INLINE void fd_64(void) {                                                   } /* LD   HY,HY    */
INLINE void fd_65(void) { HY = LY;                                          } /* LD   HY,LY    */
INLINE void fd_66(void) { EAY; H = RM(EA);                                  } /* LD   H,(IY+o) */
INLINE void fd_67(void) { HY = A;                                           } /* LD   HY,A     */

INLINE void fd_68(void) { LY = B;                                           } /* LD   LY,B     */
INLINE void fd_69(void) { LY = C;                                           } /* LD   LY,C     */
INLINE void fd_6a(void) { LY = D;                                           } /* LD   LY,D     */
INLINE void fd_6b(void) { LY = E;                                           } /* LD   LY,E     */
INLINE void fd_6c(void) { LY = HY;                                          } /* LD   LY,HY    */
INLINE void fd_6d(void) {                                                   } /* LD   LY,LY    */
INLINE void fd_6e(void) { EAY; L = RM(EA);                                  } /* LD   L,(IY+o) */
INLINE void fd_6f(void) { LY = A;                                           } /* LD   LY,A     */

INLINE void fd_70(void) { EAY; WM( EA, B );                                 } /* LD   (IY+o),B */
INLINE void fd_71(void) { EAY; WM( EA, C );                                 } /* LD   (IY+o),C */
INLINE void fd_72(void) { EAY; WM( EA, D );                                 } /* LD   (IY+o),D */
INLINE void fd_73(void) { EAY; WM( EA, E );                                 } /* LD   (IY+o),E */
INLINE void fd_74(void) { EAY; WM( EA, H );                                 } /* LD   (IY+o),H */
INLINE void fd_75(void) { EAY; WM( EA, L );                                 } /* LD   (IY+o),L */
INLINE void fd_76(void) { illegal_1(); op_76();                             } /* DB   FD       */
INLINE void fd_77(void) { EAY; WM( EA, A );                                 } /* LD   (IY+o),A */

INLINE void fd_78(void) { illegal_1(); op_78();                             } /* DB   FD       */
INLINE void fd_79(void) { illegal_1(); op_79();                             } /* DB   FD       */
INLINE void fd_7a(void) { illegal_1(); op_7a();                             } /* DB   FD       */
INLINE void fd_7b(void) { illegal_1(); op_7b();                             } /* DB   FD       */
INLINE void fd_7c(void) { A = HY;                                           } /* LD   A,HY     */
INLINE void fd_7d(void) { A = LY;                                           } /* LD   A,LY     */
INLINE void fd_7e(void) { EAY; A = RM(EA);                                  } /* LD   A,(IY+o) */
INLINE void fd_7f(void) { illegal_1(); op_7f();                             } /* DB   FD       */

INLINE void fd_80(void) { illegal_1(); op_80();                             } /* DB   FD       */
INLINE void fd_81(void) { illegal_1(); op_81();                             } /* DB   FD       */
INLINE void fd_82(void) { illegal_1(); op_82();                             } /* DB   FD       */
INLINE void fd_83(void) { illegal_1(); op_83();                             } /* DB   FD       */
INLINE void fd_84(void) { ADD(HY);                                          } /* ADD  A,HY     */
INLINE void fd_85(void) { ADD(LY);                                          } /* ADD  A,LY     */
INLINE void fd_86(void) { EAY; ADD(RM(EA));                                 } /* ADD  A,(IY+o) */
INLINE void fd_87(void) { illegal_1(); op_87();                             } /* DB   FD       */

INLINE void fd_88(void) { illegal_1(); op_88();                             } /* DB   FD       */
INLINE void fd_89(void) { illegal_1(); op_89();                             } /* DB   FD       */
INLINE void fd_8a(void) { illegal_1(); op_8a();                             } /* DB   FD       */
INLINE void fd_8b(void) { illegal_1(); op_8b();                             } /* DB   FD       */
INLINE void fd_8c(void) { ADC(HY);                                          } /* ADC  A,HY     */
INLINE void fd_8d(void) { ADC(LY);                                          } /* ADC  A,LY     */
INLINE void fd_8e(void) { EAY; ADC(RM(EA));                                 } /* ADC  A,(IY+o) */
INLINE void fd_8f(void) { illegal_1(); op_8f();                             } /* DB   FD       */

INLINE void fd_90(void) { illegal_1(); op_90();                             } /* DB   FD       */
INLINE void fd_91(void) { illegal_1(); op_91();                             } /* DB   FD       */
INLINE void fd_92(void) { illegal_1(); op_92();                             } /* DB   FD       */
INLINE void fd_93(void) { illegal_1(); op_93();                             } /* DB   FD       */
INLINE void fd_94(void) { SUB(HY);                                          } /* SUB  HY       */
INLINE void fd_95(void) { SUB(LY);                                          } /* SUB  LY       */
INLINE void fd_96(void) { EAY; SUB(RM(EA));                                 } /* SUB  (IY+o)   */
INLINE void fd_97(void) { illegal_1(); op_97();                             } /* DB   FD       */

INLINE void fd_98(void) { illegal_1(); op_98();                             } /* DB   FD       */
INLINE void fd_99(void) { illegal_1(); op_99();                             } /* DB   FD       */
INLINE void fd_9a(void) { illegal_1(); op_9a();                             } /* DB   FD       */
INLINE void fd_9b(void) { illegal_1(); op_9b();                             } /* DB   FD       */
INLINE void fd_9c(void) { SBC(HY);                                          } /* SBC  A,HY     */
INLINE void fd_9d(void) { SBC(LY);                                          } /* SBC  A,LY     */
INLINE void fd_9e(void) { EAY; SBC(RM(EA));                                 } /* SBC  A,(IY+o) */
INLINE void fd_9f(void) { illegal_1(); op_9f();                             } /* DB   FD       */

INLINE void fd_a0(void) { illegal_1(); op_a0();                             } /* DB   FD       */
INLINE void fd_a1(void) { illegal_1(); op_a1();                             } /* DB   FD       */
INLINE void fd_a2(void) { illegal_1(); op_a2();                             } /* DB   FD       */
INLINE void fd_a3(void) { illegal_1(); op_a3();                             } /* DB   FD       */
INLINE void fd_a4(void) { AND(HY);                                          } /* AND  HY       */
INLINE void fd_a5(void) { AND(LY);                                          } /* AND  LY       */
INLINE void fd_a6(void) { EAY; AND(RM(EA));                                 } /* AND  (IY+o)   */
INLINE void fd_a7(void) { illegal_1(); op_a7();                             } /* DB   FD       */

INLINE void fd_a8(void) { illegal_1(); op_a8();                             } /* DB   FD       */
INLINE void fd_a9(void) { illegal_1(); op_a9();                             } /* DB   FD       */
INLINE void fd_aa(void) { illegal_1(); op_aa();                             } /* DB   FD       */
INLINE void fd_ab(void) { illegal_1(); op_ab();                             } /* DB   FD       */
INLINE void fd_ac(void) { XOR(HY);                                          } /* XOR  HY       */
INLINE void fd_ad(void) { XOR(LY);                                          } /* XOR  LY       */
INLINE void fd_ae(void) { EAY; XOR(RM(EA));                                 } /* XOR  (IY+o)   */
INLINE void fd_af(void) { illegal_1(); op_af();                             } /* DB   FD       */

INLINE void fd_b0(void) { illegal_1(); op_b0();                             } /* DB   FD       */
INLINE void fd_b1(void) { illegal_1(); op_b1();                             } /* DB   FD       */
INLINE void fd_b2(void) { illegal_1(); op_b2();                             } /* DB   FD       */
INLINE void fd_b3(void) { illegal_1(); op_b3();                             } /* DB   FD       */
INLINE void fd_b4(void) { OR(HY);                                           } /* OR   HY       */
INLINE void fd_b5(void) { OR(LY);                                           } /* OR   LY       */
INLINE void fd_b6(void) { EAY; OR(RM(EA));                                  } /* OR   (IY+o)   */
INLINE void fd_b7(void) { illegal_1(); op_b7();                             } /* DB   FD       */

INLINE void fd_b8(void) { illegal_1(); op_b8();                             } /* DB   FD       */
INLINE void fd_b9(void) { illegal_1(); op_b9();                             } /* DB   FD       */
INLINE void fd_ba(void) { illegal_1(); op_ba();                             } /* DB   FD       */
INLINE void fd_bb(void) { illegal_1(); op_bb();                             } /* DB   FD       */
INLINE void fd_bc(void) { CP(HY);                                           } /* CP   HY       */
INLINE void fd_bd(void) { CP(LY);                                           } /* CP   LY       */
INLINE void fd_be(void) { EAY; CP(RM(EA));                                  } /* CP   (IY+o)   */
INLINE void fd_bf(void) { illegal_1(); op_bf();                             } /* DB   FD       */

INLINE void fd_c0(void) { illegal_1(); op_c0();                             } /* DB   FD       */
INLINE void fd_c1(void) { illegal_1(); op_c1();                             } /* DB   FD       */
INLINE void fd_c2(void) { illegal_1(); op_c2();                             } /* DB   FD       */
INLINE void fd_c3(void) { illegal_1(); op_c3();                             } /* DB   FD       */
INLINE void fd_c4(void) { illegal_1(); op_c4();                             } /* DB   FD       */
INLINE void fd_c5(void) { illegal_1(); op_c5();                             } /* DB   FD       */
INLINE void fd_c6(void) { illegal_1(); op_c6();                             } /* DB   FD       */
INLINE void fd_c7(void) { illegal_1(); op_c7();                             } /* DB   FD       */

INLINE void fd_c8(void) { illegal_1(); op_c8();                             } /* DB   FD       */
INLINE void fd_c9(void) { illegal_1(); op_c9();                             } /* DB   FD       */
INLINE void fd_ca(void) { illegal_1(); op_ca();                             } /* DB   FD       */
INLINE void fd_cb(void) { EAY; EXEC(xycb,ARG());                            } /* **** FD CB xx */
INLINE void fd_cc(void) { illegal_1(); op_cc();                             } /* DB   FD       */
INLINE void fd_cd(void) { illegal_1(); op_cd();                             } /* DB   FD       */
INLINE void fd_ce(void) { illegal_1(); op_ce();                             } /* DB   FD       */
INLINE void fd_cf(void) { illegal_1(); op_cf();                             } /* DB   FD       */

INLINE void fd_d0(void) { illegal_1(); op_d0();                             } /* DB   FD       */
INLINE void fd_d1(void) { illegal_1(); op_d1();                             } /* DB   FD       */
INLINE void fd_d2(void) { illegal_1(); op_d2();                             } /* DB   FD       */
INLINE void fd_d3(void) { illegal_1(); op_d3();                             } /* DB   FD       */
INLINE void fd_d4(void) { illegal_1(); op_d4();                             } /* DB   FD       */
INLINE void fd_d5(void) { illegal_1(); op_d5();                             } /* DB   FD       */
INLINE void fd_d6(void) { illegal_1(); op_d6();                             } /* DB   FD       */
INLINE void fd_d7(void) { illegal_1(); op_d7();                             } /* DB   FD       */

INLINE void fd_d8(void) { illegal_1(); op_d8();                             } /* DB   FD       */
INLINE void fd_d9(void) { illegal_1(); op_d9();                             } /* DB   FD       */
INLINE void fd_da(void) { illegal_1(); op_da();                             } /* DB   FD       */
INLINE void fd_db(void) { illegal_1(); op_db();                             } /* DB   FD       */
INLINE void fd_dc(void) { illegal_1(); op_dc();                             } /* DB   FD       */
INLINE void fd_dd(void) { EXEC(dd,ROP());                                   } /* **** FD DD xx */
INLINE void fd_de(void) { illegal_1(); op_de();                             } /* DB   FD       */
INLINE void fd_df(void) { illegal_1(); op_df();                             } /* DB   FD       */

INLINE void fd_e0(void) { illegal_1(); op_e0();                             } /* DB   FD       */
INLINE void fd_e1(void) { POP( iy );                                        } /* POP  IY       */
INLINE void fd_e2(void) { illegal_1(); op_e2();                             } /* DB   FD       */
INLINE void fd_e3(void) { EXSP( iy );                                       } /* EX   (SP),IY  */
INLINE void fd_e4(void) { illegal_1(); op_e4();                             } /* DB   FD       */
INLINE void fd_e5(void) { PUSH( iy );                                       } /* PUSH IY       */
INLINE void fd_e6(void) { illegal_1(); op_e6();                             } /* DB   FD       */
INLINE void fd_e7(void) { illegal_1(); op_e7();                             } /* DB   FD       */

INLINE void fd_e8(void) { illegal_1(); op_e8();                             } /* DB   FD       */
INLINE void fd_e9(void) { PC = IY;                                          } /* JP   (IY)     */
INLINE void fd_ea(void) { illegal_1(); op_ea();                             } /* DB   FD       */
INLINE void fd_eb(void) { illegal_1(); op_eb();                             } /* DB   FD       */
INLINE void fd_ec(void) { illegal_1(); op_ec();                             } /* DB   FD       */
INLINE void fd_ed(void) { illegal_1(); op_ed();                             } /* DB   FD       */
INLINE void fd_ee(void) { illegal_1(); op_ee();                             } /* DB   FD       */
INLINE void fd_ef(void) { illegal_1(); op_ef();                             } /* DB   FD       */

INLINE void fd_f0(void) { illegal_1(); op_f0();                             } /* DB   FD       */
INLINE void fd_f1(void) { illegal_1(); op_f1();                             } /* DB   FD       */
INLINE void fd_f2(void) { illegal_1(); op_f2();                             } /* DB   FD       */
INLINE void fd_f3(void) { illegal_1(); op_f3();                             } /* DB   FD       */
INLINE void fd_f4(void) { illegal_1(); op_f4();                             } /* DB   FD       */
INLINE void fd_f5(void) { illegal_1(); op_f5();                             } /* DB   FD       */
INLINE void fd_f6(void) { illegal_1(); op_f6();                             } /* DB   FD       */
INLINE void fd_f7(void) { illegal_1(); op_f7();                             } /* DB   FD       */

INLINE void fd_f8(void) { illegal_1(); op_f8();                             } /* DB   FD       */
INLINE void fd_f9(void) { SP = IY;                                          } /* LD   SP,IY    */
INLINE void fd_fa(void) { illegal_1(); op_fa();                             } /* DB   FD       */
INLINE void fd_fb(void) { illegal_1(); op_fb();                             } /* DB   FD       */
INLINE void fd_fc(void) { illegal_1(); op_fc();                             } /* DB   FD       */
INLINE void fd_fd(void) { EXEC(fd,ROP());                                   } /* **** FD FD xx */
INLINE void fd_fe(void) { illegal_1(); op_fe();                             } /* DB   FD       */
INLINE void fd_ff(void) { illegal_1(); op_ff();                             } /* DB   FD       */

INLINE void illegal_2()
{
#if VERBOSE
logerror("Z80 #%d ill. opcode $ed $%02x\n",
      cpu_getactivecpu(), cpu_readop((PCD-1)&0xffff));
#endif
}

/**********************************************************
 * special opcodes (ED prefix)
 **********************************************************/
INLINE void ed_00(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_01(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_02(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_03(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_04(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_05(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_06(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_07(void) { illegal_2();                                      } /* DB   ED      */

INLINE void ed_08(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_09(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_0a(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_0b(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_0c(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_0d(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_0e(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_0f(void) { illegal_2();                                      } /* DB   ED      */

INLINE void ed_10(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_11(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_12(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_13(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_14(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_15(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_16(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_17(void) { illegal_2();                                      } /* DB   ED      */

INLINE void ed_18(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_19(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_1a(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_1b(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_1c(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_1d(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_1e(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_1f(void) { illegal_2();                                      } /* DB   ED      */

INLINE void ed_20(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_21(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_22(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_23(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_24(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_25(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_26(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_27(void) { illegal_2();                                      } /* DB   ED      */

INLINE void ed_28(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_29(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_2a(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_2b(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_2c(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_2d(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_2e(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_2f(void) { illegal_2();                                      } /* DB   ED      */

INLINE void ed_30(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_31(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_32(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_33(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_34(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_35(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_36(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_37(void) { illegal_2();                                      } /* DB   ED      */

INLINE void ed_38(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_39(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_3a(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_3b(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_3c(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_3d(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_3e(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_3f(void) { illegal_2();                                      } /* DB   ED      */

INLINE void ed_40(void) { B = IN(BC); F = (F & CF) | SZP[B];                } /* IN   B,(C)   */
INLINE void ed_41(void) { OUT(BC, B);                                       } /* OUT  (C),B   */
INLINE void ed_42(void) { SBC16( bc );                                      } /* SBC  HL,BC   */
INLINE void ed_43(void) { EA = ARG16(); WM16( EA, &Z80.bc ); WZ = EA+1;     } /* LD   (w),BC  */
INLINE void ed_44(void) { NEG;                                              } /* NEG          */
INLINE void ed_45(void) { RETN;                                             } /* RETN;        */
INLINE void ed_46(void) { IM = 0;                                           } /* IM   0       */
INLINE void ed_47(void) { LD_I_A;                                           } /* LD   I,A     */

INLINE void ed_48(void) { C = IN(BC); F = (F & CF) | SZP[C];                } /* IN   C,(C)   */
INLINE void ed_49(void) { OUT(BC, C);                                       } /* OUT  (C),C   */
INLINE void ed_4a(void) { ADC16( bc );                                      } /* ADC  HL,BC   */
INLINE void ed_4b(void) { EA = ARG16(); RM16( EA, &Z80.bc ); WZ = EA+1;     } /* LD   BC,(w)  */
INLINE void ed_4c(void) { NEG;                                              } /* NEG          */
INLINE void ed_4d(void) { RETI;                                             } /* RETI         */
INLINE void ed_4e(void) { IM = 0;                                           } /* IM   0       */
INLINE void ed_4f(void) { LD_R_A;                                           } /* LD   R,A     */

INLINE void ed_50(void) { D = IN(BC); F = (F & CF) | SZP[D];                } /* IN   D,(C)   */
INLINE void ed_51(void) { OUT(BC, D);                                       } /* OUT  (C),D   */
INLINE void ed_52(void) { SBC16( de );                                      } /* SBC  HL,DE   */
INLINE void ed_53(void) { EA = ARG16(); WM16( EA, &Z80.de ); WZ = EA+1;     } /* LD   (w),DE  */
INLINE void ed_54(void) { NEG;                                              } /* NEG          */
INLINE void ed_55(void) { RETN;                                             } /* RETN;        */
INLINE void ed_56(void) { IM = 1;                                           } /* IM   1       */
INLINE void ed_57(void) { LD_A_I;                                           } /* LD   A,I     */

INLINE void ed_58(void) { E = IN(BC); F = (F & CF) | SZP[E];                } /* IN   E,(C)   */
INLINE void ed_59(void) { OUT(BC, E);                                       } /* OUT  (C),E   */
INLINE void ed_5a(void) { ADC16( de );                                      } /* ADC  HL,DE   */
INLINE void ed_5b(void) { EA = ARG16(); RM16( EA, &Z80.de ); WZ = EA+1;     } /* LD   DE,(w)  */
INLINE void ed_5c(void) { NEG;                                              } /* NEG          */
INLINE void ed_5d(void) { RETI;                                             } /* RETI         */
INLINE void ed_5e(void) { IM = 2;                                           } /* IM   2       */
INLINE void ed_5f(void) { LD_A_R;                                           } /* LD   A,R     */

INLINE void ed_60(void) { H = IN(BC); F = (F & CF) | SZP[H];                } /* IN   H,(C)   */
INLINE void ed_61(void) { OUT(BC, H);                                       } /* OUT  (C),H   */
INLINE void ed_62(void) { SBC16( hl );                                      } /* SBC  HL,HL   */
INLINE void ed_63(void) { EA = ARG16(); WM16( EA, &Z80.hl ); WZ = EA+1;     } /* LD   (w),HL  */
INLINE void ed_64(void) { NEG;                                              } /* NEG          */
INLINE void ed_65(void) { RETN;                                             } /* RETN;        */
INLINE void ed_66(void) { IM = 0;                                           } /* IM   0       */
INLINE void ed_67(void) { RRD;                                              } /* RRD  (HL)    */

INLINE void ed_68(void) { L = IN(BC); F = (F & CF) | SZP[L];                } /* IN   L,(C)   */
INLINE void ed_69(void) { OUT(BC, L);                                       } /* OUT  (C),L   */
INLINE void ed_6a(void) { ADC16( hl );                                      } /* ADC  HL,HL   */
INLINE void ed_6b(void) { EA = ARG16(); RM16( EA, &Z80.hl ); WZ = EA+1;     } /* LD   HL,(w)  */
INLINE void ed_6c(void) { NEG;                                              } /* NEG          */
INLINE void ed_6d(void) { RETI;                                             } /* RETI         */
INLINE void ed_6e(void) { IM = 0;                                           } /* IM   0       */
INLINE void ed_6f(void) { RLD;                                              } /* RLD  (HL)    */

INLINE void ed_70(void) { UINT8 res = IN(BC); F = (F & CF) | SZP[res];      } /* IN   0,(C)   */
INLINE void ed_71(void) { OUT(BC, 0);                                       } /* OUT  (C),0   */
INLINE void ed_72(void) { SBC16( sp );                                      } /* SBC  HL,SP   */
INLINE void ed_73(void) { EA = ARG16(); WM16( EA, &Z80.sp ); WZ = EA+1;     } /* LD   (w),SP  */
INLINE void ed_74(void) { NEG;                                              } /* NEG          */
INLINE void ed_75(void) { RETN;                                             } /* RETN;        */
INLINE void ed_76(void) { IM = 1;                                           } /* IM   1       */
INLINE void ed_77(void) { illegal_2();                                      } /* DB   ED,77   */

INLINE void ed_78(void) { A = IN(BC); F = (F & CF) | SZP[A]; WZ = BC+1;     } /* IN   E,(C)   */
INLINE void ed_79(void) { OUT(BC, A); WZ = BC + 1;                          } /* OUT  (C),A   */
INLINE void ed_7a(void) { ADC16( sp );                                      } /* ADC  HL,SP   */
INLINE void ed_7b(void) { EA = ARG16(); RM16( EA, &Z80.sp ); WZ = EA+1; } /* LD   SP,(w)  */
INLINE void ed_7c(void) { NEG;                                              } /* NEG          */
INLINE void ed_7d(void) { RETI;                                             } /* RETI         */
INLINE void ed_7e(void) { IM = 2;                                           } /* IM   2       */
INLINE void ed_7f(void) { illegal_2();                                      } /* DB   ED,7F   */

INLINE void ed_80(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_81(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_82(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_83(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_84(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_85(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_86(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_87(void) { illegal_2();                                      } /* DB   ED      */

INLINE void ed_88(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_89(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_8a(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_8b(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_8c(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_8d(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_8e(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_8f(void) { illegal_2();                                      } /* DB   ED      */

INLINE void ed_90(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_91(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_92(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_93(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_94(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_95(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_96(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_97(void) { illegal_2();                                      } /* DB   ED      */

INLINE void ed_98(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_99(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_9a(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_9b(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_9c(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_9d(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_9e(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_9f(void) { illegal_2();                                      } /* DB   ED      */

INLINE void ed_a0(void) { LDI;                                              } /* LDI          */
INLINE void ed_a1(void) { CPI;                                              } /* CPI          */
INLINE void ed_a2(void) { INI;                                              } /* INI          */
INLINE void ed_a3(void) { OUTI;                                             } /* OUTI         */
INLINE void ed_a4(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_a5(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_a6(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_a7(void) { illegal_2();                                      } /* DB   ED      */

INLINE void ed_a8(void) { LDD;                                              } /* LDD          */
INLINE void ed_a9(void) { CPD;                                              } /* CPD          */
INLINE void ed_aa(void) { IND;                                              } /* IND          */
INLINE void ed_ab(void) { OUTD;                                             } /* OUTD         */
INLINE void ed_ac(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_ad(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_ae(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_af(void) { illegal_2();                                      } /* DB   ED      */

INLINE void ed_b0(void) { LDIR;                                             } /* LDIR         */
INLINE void ed_b1(void) { CPIR;                                             } /* CPIR         */
INLINE void ed_b2(void) { INIR;                                             } /* INIR         */
INLINE void ed_b3(void) { OTIR();                                             } /* OTIR         */
INLINE void ed_b4(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_b5(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_b6(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_b7(void) { illegal_2();                                      } /* DB   ED      */

INLINE void ed_b8(void) { LDDR();                                             } /* LDDR         */
INLINE void ed_b9(void) { CPDR();                                             } /* CPDR         */
INLINE void ed_ba(void) { INDR();                                             } /* INDR         */
INLINE void ed_bb(void) { OTDR();                                             } /* OTDR         */
INLINE void ed_bc(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_bd(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_be(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_bf(void) { illegal_2();                                      } /* DB   ED      */

INLINE void ed_c0(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_c1(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_c2(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_c3(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_c4(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_c5(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_c6(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_c7(void) { illegal_2();                                      } /* DB   ED      */

INLINE void ed_c8(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_c9(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_ca(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_cb(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_cc(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_cd(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_ce(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_cf(void) { illegal_2();                                      } /* DB   ED      */

INLINE void ed_d0(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_d1(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_d2(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_d3(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_d4(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_d5(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_d6(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_d7(void) { illegal_2();                                      } /* DB   ED      */

INLINE void ed_d8(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_d9(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_da(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_db(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_dc(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_dd(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_de(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_df(void) { illegal_2();                                      } /* DB   ED      */

INLINE void ed_e0(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_e1(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_e2(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_e3(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_e4(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_e5(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_e6(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_e7(void) { illegal_2();                                      } /* DB   ED      */

INLINE void ed_e8(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_e9(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_ea(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_eb(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_ec(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_ed(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_ee(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_ef(void) { illegal_2();                                      } /* DB   ED      */

INLINE void ed_f0(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_f1(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_f2(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_f3(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_f4(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_f5(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_f6(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_f7(void) { illegal_2();                                      } /* DB   ED      */

INLINE void ed_f8(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_f9(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_fa(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_fb(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_fc(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_fd(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_fe(void) { illegal_2();                                      } /* DB   ED      */
INLINE void ed_ff(void) { illegal_2();                                      } /* DB   ED      */


/**********************************************************
 * main opcodes
 **********************************************************/
INLINE void op_00(void) {                                                                                                } /* NOP              */
INLINE void op_01(void) { BC = ARG16();                                                                                  } /* LD   BC,w        */
INLINE void op_02(void) { WM( BC, A ); WZ_L = (BC + 1) & 0xFF;  WZ_H = A;                                                } /* LD   (BC),A      */
INLINE void op_03(void) { BC++;                                                                                          } /* INC  BC          */
INLINE void op_04(void) { B = INC(B);                                                                                    } /* INC  B           */
INLINE void op_05(void) { B = DEC(B);                                                                                    } /* DEC  B           */
INLINE void op_06(void) { B = ARG();                                                                                     } /* LD   B,n         */
INLINE void op_07(void) { RLCA;                                                                                          } /* RLCA             */

INLINE void op_08(void) { EX_AF;                                                                                         } /* EX   AF,AF'      */
INLINE void op_09(void) { ADD16(hl, bc);                                                                                 } /* ADD  HL,BC       */
INLINE void op_0a(void) { A = RM( BC ); WZ=BC+1;                                                                         } /* LD   A,(BC)      */
INLINE void op_0b(void) { BC--;                                                                                          } /* DEC  BC          */
INLINE void op_0c(void) { C = INC(C);                                                                                    } /* INC  C           */
INLINE void op_0d(void) { C = DEC(C);                                                                                    } /* DEC  C           */
INLINE void op_0e(void) { C = ARG();                                                                                     } /* LD   C,n         */
INLINE void op_0f(void) { RRCA;                                                                                          } /* RRCA             */

INLINE void op_10(void) { B--; JR_COND( B, 0x10 );                                                                       } /* DJNZ o           */
INLINE void op_11(void) { DE = ARG16();                                                                                  } /* LD   DE,w        */
INLINE void op_12(void) { WM( DE, A ); WZ_L = (DE + 1) & 0xFF;  WZ_H = A;                                                } /* LD   (DE),A      */
INLINE void op_13(void) { DE++;                                                                                          } /* INC  DE          */
INLINE void op_14(void) { D = INC(D);                                                                                    } /* INC  D           */
INLINE void op_15(void) { D = DEC(D);                                                                                    } /* DEC  D           */
INLINE void op_16(void) { D = ARG();                                                                                     } /* LD   D,n         */
INLINE void op_17(void) { RLA;                                                                                           } /* RLA              */

INLINE void op_18(void) { JR();                                                                                          } /* JR   o           */
INLINE void op_19(void) { ADD16(hl, de);                                                                                 } /* ADD  HL,DE       */
INLINE void op_1a(void) { A = RM( DE ); WZ=DE+1;                                                                         } /* LD   A,(DE)      */
INLINE void op_1b(void) { DE--;                                                                                          } /* DEC  DE          */
INLINE void op_1c(void) { E = INC(E);                                                                                    } /* INC  E           */
INLINE void op_1d(void) { E = DEC(E);                                                                                    } /* DEC  E           */
INLINE void op_1e(void) { E = ARG();                                                                                     } /* LD   E,n         */
INLINE void op_1f(void) { RRA;                                                                                           } /* RRA              */

INLINE void op_20(void) { JR_COND( !(F & ZF), 0x20 );                                                                    } /* JR   NZ,o        */
INLINE void op_21(void) { HL = ARG16();                                                                                  } /* LD   HL,w        */
INLINE void op_22(void) { EA = ARG16(); WM16( EA, &Z80.hl ); WZ = EA+1;                                                  } /* LD   (w),HL      */
INLINE void op_23(void) { HL++;                                                                                          } /* INC  HL          */
INLINE void op_24(void) { H = INC(H);                                                                                    } /* INC  H           */
INLINE void op_25(void) { H = DEC(H);                                                                                    } /* DEC  H           */
INLINE void op_26(void) { H = ARG();                                                                                     } /* LD   H,n         */
INLINE void op_27(void) { DAA;                                                                                           } /* DAA              */

INLINE void op_28(void) { JR_COND( F & ZF, 0x28 );                                                                       } /* JR   Z,o         */
INLINE void op_29(void) { ADD16(hl, hl);                                                                                 } /* ADD  HL,HL       */
INLINE void op_2a(void) { EA = ARG16(); RM16( EA, &Z80.hl ); WZ = EA+1;                                                  } /* LD   HL,(w)      */
INLINE void op_2b(void) { HL--;                                                                                          } /* DEC  HL          */
INLINE void op_2c(void) { L = INC(L);                                                                                    } /* INC  L           */
INLINE void op_2d(void) { L = DEC(L);                                                                                    } /* DEC  L           */
INLINE void op_2e(void) { L = ARG();                                                                                     } /* LD   L,n         */
INLINE void op_2f(void) { A ^= 0xff; F = (F&(SF|ZF|PF|CF))|HF|NF|(A&(YF|XF));                                            } /* CPL              */

INLINE void op_30(void) { JR_COND( !(F & CF), 0x30 );                                                                    } /* JR   NC,o        */
INLINE void op_31(void) { SP = ARG16();                                                                                  } /* LD   SP,w        */
INLINE void op_32(void) { EA = ARG16(); WM( EA, A ); WZ_L=(EA+1)&0xFF;WZ_H=A;                                            } /* LD   (w),A       */
INLINE void op_33(void) { SP++;                                                                                          } /* INC  SP          */
INLINE void op_34(void) { WM( HL, INC(RM(HL)) );                                                                         } /* INC  (HL)        */
INLINE void op_35(void) { WM( HL, DEC(RM(HL)) );                                                                         } /* DEC  (HL)        */
INLINE void op_36(void) { WM( HL, ARG() );                                                                               } /* LD   (HL),n      */
INLINE void op_37(void) { F = (F & (SF|ZF|YF|XF|PF)) | CF | (A & (YF|XF));                                               } /* SCF              */

INLINE void op_38(void) { JR_COND( F & CF, 0x38 );                                                                       } /* JR   C,o         */
INLINE void op_39(void) { ADD16(hl, sp);                                                                                 } /* ADD  HL,SP       */
INLINE void op_3a(void) { EA = ARG16(); A = RM( EA ); WZ = EA+1;                                                         } /* LD   A,(w)       */
INLINE void op_3b(void) { SP--;                                                                                          } /* DEC  SP          */
INLINE void op_3c(void) { A = INC(A);                                                                                    } /* INC  A           */
INLINE void op_3d(void) { A = DEC(A);                                                                                    } /* DEC  A           */
INLINE void op_3e(void) { A = ARG();                                                                                     } /* LD   A,n         */
INLINE void op_3f(void) { F = ((F&(SF|ZF|YF|XF|PF|CF))|((F&CF)<<4)|(A&(YF|XF)))^CF;                                      } /* CCF              */

INLINE void op_40(void) {                                                                                                } /* LD   B,B         */
INLINE void op_41(void) { B = C;                                                                                         } /* LD   B,C         */
INLINE void op_42(void) { B = D;                                                                                         } /* LD   B,D         */
INLINE void op_43(void) { B = E;                                                                                         } /* LD   B,E         */
INLINE void op_44(void) { B = H;                                                                                         } /* LD   B,H         */
INLINE void op_45(void) { B = L;                                                                                         } /* LD   B,L         */
INLINE void op_46(void) { B = RM(HL);                                                                                    } /* LD   B,(HL)      */
INLINE void op_47(void) { B = A;                                                                                         } /* LD   B,A         */

INLINE void op_48(void) { C = B;                                                                                         } /* LD   C,B         */
INLINE void op_49(void) {                                                                                                } /* LD   C,C         */
INLINE void op_4a(void) { C = D;                                                                                         } /* LD   C,D         */
INLINE void op_4b(void) { C = E;                                                                                         } /* LD   C,E         */
INLINE void op_4c(void) { C = H;                                                                                         } /* LD   C,H         */
INLINE void op_4d(void) { C = L;                                                                                         } /* LD   C,L         */
INLINE void op_4e(void) { C = RM(HL);                                                                                    } /* LD   C,(HL)      */
INLINE void op_4f(void) { C = A;                                                                                         } /* LD   C,A         */

INLINE void op_50(void) { D = B;                                                                                         } /* LD   D,B         */
INLINE void op_51(void) { D = C;                                                                                         } /* LD   D,C         */
INLINE void op_52(void) {                                                                                                } /* LD   D,D         */
INLINE void op_53(void) { D = E;                                                                                         } /* LD   D,E         */
INLINE void op_54(void) { D = H;                                                                                         } /* LD   D,H         */
INLINE void op_55(void) { D = L;                                                                                         } /* LD   D,L         */
INLINE void op_56(void) { D = RM(HL);                                                                                    } /* LD   D,(HL)      */
INLINE void op_57(void) { D = A;                                                                                         } /* LD   D,A         */

INLINE void op_58(void) { E = B;                                                                                         } /* LD   E,B         */
INLINE void op_59(void) { E = C;                                                                                         } /* LD   E,C         */
INLINE void op_5a(void) { E = D;                                                                                         } /* LD   E,D         */
INLINE void op_5b(void) {                                                                                                } /* LD   E,E         */
INLINE void op_5c(void) { E = H;                                                                                         } /* LD   E,H         */
INLINE void op_5d(void) { E = L;                                                                                         } /* LD   E,L         */
INLINE void op_5e(void) { E = RM(HL);                                                                                    } /* LD   E,(HL)      */
INLINE void op_5f(void) { E = A;                                                                                         } /* LD   E,A         */

INLINE void op_60(void) { H = B;                                                                                         } /* LD   H,B         */
INLINE void op_61(void) { H = C;                                                                                         } /* LD   H,C         */
INLINE void op_62(void) { H = D;                                                                                         } /* LD   H,D         */
INLINE void op_63(void) { H = E;                                                                                         } /* LD   H,E         */
INLINE void op_64(void) {                                                                                                } /* LD   H,H         */
INLINE void op_65(void) { H = L;                                                                                         } /* LD   H,L         */
INLINE void op_66(void) { H = RM(HL);                                                                                    } /* LD   H,(HL)      */
INLINE void op_67(void) { H = A;                                                                                         } /* LD   H,A         */

INLINE void op_68(void) { L = B;                                                                                         } /* LD   L,B         */
INLINE void op_69(void) { L = C;                                                                                         } /* LD   L,C         */
INLINE void op_6a(void) { L = D;                                                                                         } /* LD   L,D         */
INLINE void op_6b(void) { L = E;                                                                                         } /* LD   L,E         */
INLINE void op_6c(void) { L = H;                                                                                         } /* LD   L,H         */
INLINE void op_6d(void) {                                                                                                } /* LD   L,L         */
INLINE void op_6e(void) { L = RM(HL);                                                                                    } /* LD   L,(HL)      */
INLINE void op_6f(void) { L = A;                                                                                         } /* LD   L,A         */

INLINE void op_70(void) { WM( HL, B );                                                                                   } /* LD   (HL),B      */
INLINE void op_71(void) { WM( HL, C );                                                                                   } /* LD   (HL),C      */
INLINE void op_72(void) { WM( HL, D );                                                                                   } /* LD   (HL),D      */
INLINE void op_73(void) { WM( HL, E );                                                                                   } /* LD   (HL),E      */
INLINE void op_74(void) { WM( HL, H );                                                                                   } /* LD   (HL),H      */
INLINE void op_75(void) { WM( HL, L );                                                                                   } /* LD   (HL),L      */
INLINE void op_76(void) { ENTER_HALT;                                                                                    } /* HALT             */
INLINE void op_77(void) { WM( HL, A );                                                                                   } /* LD   (HL),A      */

INLINE void op_78(void) { A = B;                                                                                         } /* LD   A,B         */
INLINE void op_79(void) { A = C;                                                                                         } /* LD   A,C         */
INLINE void op_7a(void) { A = D;                                                                                         } /* LD   A,D         */
INLINE void op_7b(void) { A = E;                                                                                         } /* LD   A,E         */
INLINE void op_7c(void) { A = H;                                                                                         } /* LD   A,H         */
INLINE void op_7d(void) { A = L;                                                                                         } /* LD   A,L         */
INLINE void op_7e(void) { A = RM(HL);                                                                                    } /* LD   A,(HL)      */
INLINE void op_7f(void) {                                                                                                } /* LD   A,A         */

INLINE void op_80(void) { ADD(B);                                                                                        } /* ADD  A,B         */
INLINE void op_81(void) { ADD(C);                                                                                        } /* ADD  A,C         */
INLINE void op_82(void) { ADD(D);                                                                                        } /* ADD  A,D         */
INLINE void op_83(void) { ADD(E);                                                                                        } /* ADD  A,E         */
INLINE void op_84(void) { ADD(H);                                                                                        } /* ADD  A,H         */
INLINE void op_85(void) { ADD(L);                                                                                        } /* ADD  A,L         */
INLINE void op_86(void) { ADD(RM(HL));                                                                                   } /* ADD  A,(HL)      */
INLINE void op_87(void) { ADD(A);                                                                                        } /* ADD  A,A         */

INLINE void op_88(void) { ADC(B);                                                                                        } /* ADC  A,B         */
INLINE void op_89(void) { ADC(C);                                                                                        } /* ADC  A,C         */
INLINE void op_8a(void) { ADC(D);                                                                                        } /* ADC  A,D         */
INLINE void op_8b(void) { ADC(E);                                                                                        } /* ADC  A,E         */
INLINE void op_8c(void) { ADC(H);                                                                                        } /* ADC  A,H         */
INLINE void op_8d(void) { ADC(L);                                                                                        } /* ADC  A,L         */
INLINE void op_8e(void) { ADC(RM(HL));                                                                                   } /* ADC  A,(HL)      */
INLINE void op_8f(void) { ADC(A);                                                                                        } /* ADC  A,A         */

INLINE void op_90(void) { SUB(B);                                                                                        } /* SUB  B           */
INLINE void op_91(void) { SUB(C);                                                                                        } /* SUB  C           */
INLINE void op_92(void) { SUB(D);                                                                                        } /* SUB  D           */
INLINE void op_93(void) { SUB(E);                                                                                        } /* SUB  E           */
INLINE void op_94(void) { SUB(H);                                                                                        } /* SUB  H           */
INLINE void op_95(void) { SUB(L);                                                                                        } /* SUB  L           */
INLINE void op_96(void) { SUB(RM(HL));                                                                                   } /* SUB  (HL)        */
INLINE void op_97(void) { SUB(A);                                                                                        } /* SUB  A           */

INLINE void op_98(void) { SBC(B);                                                                                        } /* SBC  A,B         */
INLINE void op_99(void) { SBC(C);                                                                                        } /* SBC  A,C         */
INLINE void op_9a(void) { SBC(D);                                                                                        } /* SBC  A,D         */
INLINE void op_9b(void) { SBC(E);                                                                                        } /* SBC  A,E         */
INLINE void op_9c(void) { SBC(H);                                                                                        } /* SBC  A,H         */
INLINE void op_9d(void) { SBC(L);                                                                                        } /* SBC  A,L         */
INLINE void op_9e(void) { SBC(RM(HL));                                                                                   } /* SBC  A,(HL)      */
INLINE void op_9f(void) { SBC(A);                                                                                        } /* SBC  A,A         */

INLINE void op_a0(void) { AND(B);                                                                                        } /* AND  B           */
INLINE void op_a1(void) { AND(C);                                                                                        } /* AND  C           */
INLINE void op_a2(void) { AND(D);                                                                                        } /* AND  D           */
INLINE void op_a3(void) { AND(E);                                                                                        } /* AND  E           */
INLINE void op_a4(void) { AND(H);                                                                                        } /* AND  H           */
INLINE void op_a5(void) { AND(L);                                                                                        } /* AND  L           */
INLINE void op_a6(void) { AND(RM(HL));                                                                                   } /* AND  (HL)        */
INLINE void op_a7(void) { AND(A);                                                                                        } /* AND  A           */

INLINE void op_a8(void) { XOR(B);                                                                                        } /* XOR  B           */
INLINE void op_a9(void) { XOR(C);                                                                                        } /* XOR  C           */
INLINE void op_aa(void) { XOR(D);                                                                                        } /* XOR  D           */
INLINE void op_ab(void) { XOR(E);                                                                                        } /* XOR  E           */
INLINE void op_ac(void) { XOR(H);                                                                                        } /* XOR  H           */
INLINE void op_ad(void) { XOR(L);                                                                                        } /* XOR  L           */
INLINE void op_ae(void) { XOR(RM(HL));                                                                                   } /* XOR  (HL)        */
INLINE void op_af(void) { XOR(A);                                                                                        } /* XOR  A           */

INLINE void op_b0(void) { OR(B);                                                                                         } /* OR   B           */
INLINE void op_b1(void) { OR(C);                                                                                         } /* OR   C           */
INLINE void op_b2(void) { OR(D);                                                                                         } /* OR   D           */
INLINE void op_b3(void) { OR(E);                                                                                         } /* OR   E           */
INLINE void op_b4(void) { OR(H);                                                                                         } /* OR   H           */
INLINE void op_b5(void) { OR(L);                                                                                         } /* OR   L           */
INLINE void op_b6(void) { OR(RM(HL));                                                                                    } /* OR   (HL)        */
INLINE void op_b7(void) { OR(A);                                                                                         } /* OR   A           */

INLINE void op_b8(void) { CP(B);                                                                                         } /* CP   B           */
INLINE void op_b9(void) { CP(C);                                                                                         } /* CP   C           */
INLINE void op_ba(void) { CP(D);                                                                                         } /* CP   D           */
INLINE void op_bb(void) { CP(E);                                                                                         } /* CP   E           */
INLINE void op_bc(void) { CP(H);                                                                                         } /* CP   H           */
INLINE void op_bd(void) { CP(L);                                                                                         } /* CP   L           */
INLINE void op_be(void) { CP(RM(HL));                                                                                    } /* CP   (HL)        */
INLINE void op_bf(void) { CP(A);                                                                                         } /* CP   A           */

INLINE void op_c0(void) { RET_COND( !(F & ZF), 0xc0 );                                                                   } /* RET  NZ          */
INLINE void op_c1(void) { POP( bc );                                                                                     } /* POP  BC          */
INLINE void op_c2(void) { JP_COND( !(F & ZF) );                                                                          } /* JP   NZ,a        */
INLINE void op_c3(void) { JP;                                                                                            } /* JP   a           */
INLINE void op_c4(void) { CALL_COND( !(F & ZF), 0xc4 );                                                                  } /* CALL NZ,a        */
INLINE void op_c5(void) { PUSH( bc );                                                                                    } /* PUSH BC          */
INLINE void op_c6(void) { ADD(ARG());                                                                                    } /* ADD  A,n         */
INLINE void op_c7(void) { RST(0x00);                                                                                     } /* RST  0           */

INLINE void op_c8(void) { RET_COND( F & ZF, 0xc8 );                                                                      } /* RET  Z           */
INLINE void op_c9(void) { POP( pc ); WZ=PCD;                                                                             } /* RET              */
INLINE void op_ca(void) { JP_COND( F & ZF );                                                                             } /* JP   Z,a         */
INLINE void op_cb(void) { R++; EXEC(cb,ROP());                                                                           } /* **** CB xx       */
INLINE void op_cc(void) { CALL_COND( F & ZF, 0xcc );                                                                     } /* CALL Z,a         */
INLINE void op_cd(void) { CALL();                                                                                        } /* CALL a           */
INLINE void op_ce(void) { ADC(ARG());                                                                                    } /* ADC  A,n         */
INLINE void op_cf(void) { RST(0x08);                                                                                     } /* RST  1           */

INLINE void op_d0(void) { RET_COND( !(F & CF), 0xd0 );                                                                   } /* RET  NC          */
INLINE void op_d1(void) { POP( de );                                                                                     } /* POP  DE          */
INLINE void op_d2(void) { JP_COND( !(F & CF) );                                                                          } /* JP   NC,a        */
INLINE void op_d3(void) { unsigned n = ARG() | (A << 8); OUT( n, A ); WZ_L = ((n & 0xff) + 1) & 0xff;  WZ_H = A; } /* OUT  (n),A       */
INLINE void op_d4(void) { CALL_COND( !(F & CF), 0xd4 );                                                                  } /* CALL NC,a        */
INLINE void op_d5(void) { PUSH( de );                                                                                    } /* PUSH DE          */
INLINE void op_d6(void) { SUB(ARG());                                                                                    } /* SUB  n           */
INLINE void op_d7(void) { RST(0x10);                                                                                     } /* RST  2           */

INLINE void op_d8(void) { RET_COND( F & CF, 0xd8 );                                                                      } /* RET  C           */
INLINE void op_d9(void) { EXX();                                                                                           } /* EXX              */
INLINE void op_da(void) { JP_COND( F & CF );                                                                             } /* JP   C,a         */
INLINE void op_db(void) { unsigned n = ARG() | (A << 8); A = IN( n ); WZ = n + 1;                                        } /* IN   A,(n)       */
INLINE void op_dc(void) { CALL_COND( F & CF, 0xdc );                                                                     } /* CALL C,a         */
INLINE void op_dd(void) { R++; EXEC(dd,ROP());                                                                           } /* **** DD xx       */
INLINE void op_de(void) { SBC(ARG());                                                                                    } /* SBC  A,n         */
INLINE void op_df(void) { RST(0x18);                                                                                     } /* RST  3           */

INLINE void op_e0(void) { RET_COND( !(F & PF), 0xe0 );                                                                   } /* RET  PO          */
INLINE void op_e1(void) { POP( hl );                                                                                     } /* POP  HL          */
INLINE void op_e2(void) { JP_COND( !(F & PF) );                                                                          } /* JP   PO,a        */
INLINE void op_e3(void) { EXSP( hl );                                                                                    } /* EX   HL,(SP)     */
INLINE void op_e4(void) { CALL_COND( !(F & PF), 0xe4 );                                                                  } /* CALL PO,a        */
INLINE void op_e5(void) { PUSH( hl );                                                                                    } /* PUSH HL          */
INLINE void op_e6(void) { AND(ARG());                                                                                    } /* AND  n           */
INLINE void op_e7(void) { RST(0x20);                                                                                     } /* RST  4           */

INLINE void op_e8(void) { RET_COND( F & PF, 0xe8 );                                                                      } /* RET  PE          */
INLINE void op_e9(void) { PC = HL;                                                                                       } /* JP   (HL)        */
INLINE void op_ea(void) { JP_COND( F & PF );                                                                             } /* JP   PE,a        */
INLINE void op_eb(void) { EX_DE_HL();                                                                                      } /* EX   DE,HL       */
INLINE void op_ec(void) { CALL_COND( F & PF, 0xec );                                                                     } /* CALL PE,a        */
INLINE void op_ed(void) { R++; EXEC(ed,ROP());                                                                           } /* **** ED xx       */
INLINE void op_ee(void) { XOR(ARG());                                                                                    } /* XOR  n           */
INLINE void op_ef(void) { RST(0x28);                                                                                     } /* RST  5           */

INLINE void op_f0(void) { RET_COND( !(F & SF), 0xf0 );                                                                   } /* RET  P           */
INLINE void op_f1(void) { POP( af );                                                                                     } /* POP  AF          */
INLINE void op_f2(void) { JP_COND( !(F & SF) );                                                                          } /* JP   P,a         */
INLINE void op_f3(void) { IFF1 = IFF2 = 0;                                                                               } /* DI               */
INLINE void op_f4(void) { CALL_COND( !(F & SF), 0xf4 );                                                                  } /* CALL P,a         */
INLINE void op_f5(void) { PUSH( af );                                                                                    } /* PUSH AF          */
INLINE void op_f6(void) { OR(ARG());                                                                                     } /* OR   n           */
INLINE void op_f7(void) { RST(0x30);                                                                                     } /* RST  6           */

INLINE void op_f8(void) { RET_COND( F & SF, 0xf8 );                                                                      } /* RET  M           */
INLINE void op_f9(void) { SP = HL;                                                                                       } /* LD   SP,HL       */
INLINE void op_fa(void) { JP_COND(F & SF);                                                                               } /* JP   M,a         */
INLINE void op_fb(void) { EI();                                                                                            } /* EI               */
INLINE void op_fc(void) { CALL_COND( F & SF, 0xfc );                                                                     } /* CALL M,a         */
INLINE void op_fd(void) { R++; EXEC(fd,ROP());                                                                           } /* **** FD xx       */
INLINE void op_fe(void) { CP(ARG());                                                                                     } /* CP   n           */
INLINE void op_ff(void) { RST(0x38);                                                                                     } /* RST  7           */


static void take_interrupt(void)
{
  /* Check if processor was halted */
  LEAVE_HALT;

  /* Clear both interrupt flip flops */
  IFF1 = IFF2 = 0;

  LOG(("Z80 #%d single int. irq_vector $%02x\n", cpu_getactivecpu(), irq_vector));

  /* Interrupt mode 1. RST 38h */
  if( IM == 1 )
  {
    LOG(("Z80 #%d IM1 $0038\n",cpu_getactivecpu() ));
    PUSH( pc );
    PCD = 0x0038;
    /* RST $38 + 'interrupt latency' cycles */
    Z80.cycles += cc[Z80_TABLE_op][0xff] + cc[Z80_TABLE_ex][0xff];
  }
  else
  {
    /* call back the cpu interface to retrieve the vector */
    int irq_vector = (*Z80.irq_callback)(0);

    /* Interrupt mode 2. Call [Z80.i:databyte] */
    if( IM == 2 )
    {
      irq_vector = (irq_vector & 0xff) | (I << 8);
      PUSH( pc );
      RM16( irq_vector, &Z80.pc );
      LOG(("Z80 #%d IM2 [$%04x] = $%04x\n",cpu_getactivecpu() , irq_vector, PCD));
        /* CALL $xxxx + 'interrupt latency' cycles */
      Z80.cycles += cc[Z80_TABLE_op][0xcd] + cc[Z80_TABLE_ex][0xff];
    }
    else
    {
      /* Interrupt mode 0. We check for CALL and JP instructions, */
      /* if neither of these were found we assume a 1 byte opcode */
      /* was placed on the databus                */
      LOG(("Z80 #%d IM0 $%04x\n",cpu_getactivecpu() , irq_vector));
      switch (irq_vector & 0xff0000)
      {
        case 0xcd0000:  /* call */
        PUSH( pc );
        PCD = irq_vector & 0xffff;
           /* CALL $xxxx + 'interrupt latency' cycles */
        Z80.cycles += cc[Z80_TABLE_op][0xcd] + cc[Z80_TABLE_ex][0xff];
          break;
        case 0xc30000:  /* jump */
        PCD = irq_vector & 0xffff;
          /* JP $xxxx + 2 cycles */
        Z80.cycles += cc[Z80_TABLE_op][0xc3] + cc[Z80_TABLE_ex][0xff];
          break;
        default:    /* rst (or other opcodes?) */
        PUSH( pc );
        PCD = irq_vector & 0x0038;
          /* RST $xx + 2 cycles */
        Z80.cycles += cc[Z80_TABLE_op][0xff] + cc[Z80_TABLE_ex][0xff];
          break;
      }
    }
  }
  WZ=PCD;
}

/****************************************************************************
 * Processor initialization
 ****************************************************************************/
void z80_init(const void *config, int (*irqcallback)(int))
{
  int i, p;

  int oldval, newval, val;
  UINT8 *padd = &SZHVC_add[  0*256];
  UINT8 *padc = &SZHVC_add[256*256];
  UINT8 *psub = &SZHVC_sub[  0*256];
  UINT8 *psbc = &SZHVC_sub[256*256];
  for (oldval = 0; oldval < 256; oldval++)
  {
    for (newval = 0; newval < 256; newval++)
    {
      /* add or adc w/o carry set */
      val = newval - oldval;
      *padd = (newval) ? ((newval & 0x80) ? SF : 0) : ZF;
      *padd |= (newval & (YF | XF));  /* undocumented flag bits 5+3 */
      if( (newval & 0x0f) < (oldval & 0x0f) ) *padd |= HF;
      if( newval < oldval ) *padd |= CF;
      if( (val^oldval^0x80) & (val^newval) & 0x80 ) *padd |= VF;
      padd++;

      /* adc with carry set */
      val = newval - oldval - 1;
      *padc = (newval) ? ((newval & 0x80) ? SF : 0) : ZF;
      *padc |= (newval & (YF | XF));  /* undocumented flag bits 5+3 */
      if( (newval & 0x0f) <= (oldval & 0x0f) ) *padc |= HF;
      if( newval <= oldval ) *padc |= CF;
      if( (val^oldval^0x80) & (val^newval) & 0x80 ) *padc |= VF;
      padc++;

      /* cp, sub or sbc w/o carry set */
      val = oldval - newval;
      *psub = NF | ((newval) ? ((newval & 0x80) ? SF : 0) : ZF);
      *psub |= (newval & (YF | XF));  /* undocumented flag bits 5+3 */
      if( (newval & 0x0f) > (oldval & 0x0f) ) *psub |= HF;
      if( newval > oldval ) *psub |= CF;
      if( (val^oldval) & (oldval^newval) & 0x80 ) *psub |= VF;
      psub++;

      /* sbc with carry set */
      val = oldval - newval - 1;
      *psbc = NF | ((newval) ? ((newval & 0x80) ? SF : 0) : ZF);
      *psbc |= (newval & (YF | XF));  /* undocumented flag bits 5+3 */
      if( (newval & 0x0f) >= (oldval & 0x0f) ) *psbc |= HF;
      if( newval >= oldval ) *psbc |= CF;
      if( (val^oldval) & (oldval^newval) & 0x80 ) *psbc |= VF;
      psbc++;
    }
  }

  for (i = 0; i < 256; i++)
  {
    p = 0;
    if( i&0x01 ) ++p;
    if( i&0x02 ) ++p;
    if( i&0x04 ) ++p;
    if( i&0x08 ) ++p;
    if( i&0x10 ) ++p;
    if( i&0x20 ) ++p;
    if( i&0x40 ) ++p;
    if( i&0x80 ) ++p;
    SZ[i] = i ? i & SF : ZF;
    SZ[i] |= (i & (YF | XF));    /* undocumented flag bits 5+3 */
    SZ_BIT[i] = i ? i & SF : ZF | PF;
    SZ_BIT[i] |= (i & (YF | XF));  /* undocumented flag bits 5+3 */
    SZP[i] = SZ[i] | ((p & 1) ? 0 : PF);
    SZHV_inc[i] = SZ[i];
    if( i == 0x80 ) SZHV_inc[i] |= VF;
    if( (i & 0x0f) == 0x00 ) SZHV_inc[i] |= HF;
    SZHV_dec[i] = SZ[i] | NF;
    if( i == 0x7f ) SZHV_dec[i] |= VF;
    if( (i & 0x0f) == 0x0f ) SZHV_dec[i] |= HF;
  }

  /* Initialize Z80 */
  memset(&Z80, 0, sizeof(Z80));
  Z80.daisy = config;
  Z80.irq_callback = irqcallback;

  /* Clear registers values (NB: should be random on real hardware ?) */
  AF = BC = DE = HL = SP = IX = IY =0;
  F = ZF; /* Zero flag is set */

  /* setup cycle tables */
  cc[Z80_TABLE_op] = cc_op;
  cc[Z80_TABLE_cb] = cc_cb;
  cc[Z80_TABLE_ed] = cc_ed;
  cc[Z80_TABLE_xy] = cc_xy;
  cc[Z80_TABLE_xycb] = cc_xycb;
  cc[Z80_TABLE_ex] = cc_ex;
}

/****************************************************************************
 * Do a reset
 ****************************************************************************/
void z80_reset(void)
{
  PC = 0x0000;
  I = 0;
  R = 0;
  R2 = 0;
  IM = 0;
  IFF1 = IFF2 = 0;
  HALT = 0;

  Z80.after_ei = FALSE;

  WZ=PCD;
}

/****************************************************************************
 * Run until given cycle count 
 ****************************************************************************/
void z80_run(unsigned int cycles)
{
  while( Z80.cycles < cycles )
  {
    /* check for IRQs before each instruction */
    if (Z80.irq_state && IFF1 && !Z80.after_ei)
    {
      take_interrupt();
      if (Z80.cycles >= cycles) return;
    }

    Z80.after_ei = FALSE;
    R++;
    EXEC_INLINE(op,ROP());
  }
} 

/****************************************************************************
 * Get all registers in given buffer
 ****************************************************************************/
void z80_get_context (void *dst)
{
  if( dst )
    *(Z80_Regs*)dst = Z80;
}

/****************************************************************************
 * Set all registers to given values
 ****************************************************************************/
void z80_set_context (void *src)
{
  if( src )
    Z80 = *(Z80_Regs*)src;
}

/****************************************************************************
 * Set IRQ lines
 ****************************************************************************/
void z80_set_irq_line(unsigned int state)
{
  Z80.irq_state = state;
}

void z80_set_nmi_line(unsigned int state)
{
  /* mark an NMI pending on the rising edge */
  if (Z80.nmi_state == CLEAR_LINE && state != CLEAR_LINE)
  {
    LOG(("Z80 #%d take NMI\n", cpu_getactivecpu()));
    LEAVE_HALT;      /* Check if processor was halted */

    IFF1 = 0;
    PUSH( pc );
    PCD = 0x0066;
    WZ=PCD;

    Z80.cycles += 11*15;
  }

  Z80.nmi_state = state;
}

