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
 *    - Changed variable EA and ARG16() function to u32; this
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


bool VERBOSE = false;

#if VERBOSE
#define LOG(x)  logerror x
#else
#define LOG(x)
#endif

u32 cpu_readop(u32 a) {
	return z80_readmap[(a) >> 10][(a) & 0x03FF];
}

u32 cpu_readop_arg(u32 a) {
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

u8 *z80_readmap[64];
u8 *z80_writemap[64];

void (*z80_writemem)(u32 address, u8 data);
u8 (*z80_readmem)(u32 address);
void (*z80_writeport)(u32 port, u8 data);
u8 (*z80_readport)(u32 port);

static u32 EA;

static u8 SZ[256];       /* zero and sign flags */
static u8 SZ_BIT[256];   /* zero, sign and parity/overflow (=zero) flags for BIT opcode */
static u8 SZP[256];      /* zero, sign and parity flags */
static u8 SZHV_inc[256]; /* zero, sign, half carry and overflow flags INC r8 */
static u8 SZHV_dec[256]; /* zero, sign, half carry and overflow flags DEC r8 */

static u8 SZHVC_add[2*256*256]; /* flags for ADD opcode */
static u8 SZHVC_sub[2*256*256]; /* flags for SUB opcode */

static const u16 cc_op[0x100] = {
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

static const u16 cc_cb[0x100] = {
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

static const u16 cc_ed[0x100] = {
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


/* illegal combo should return 4 + cc_op[i] */
static const u16 cc_xy[0x100] ={
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

static const u16 cc_xycb[0x100] = {
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
static const u16 cc_ex[0x100] = {
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

static const u16 *cc[6];
#define Z80_TABLE_dd  Z80_TABLE_xy
#define Z80_TABLE_fd  Z80_TABLE_xy

typedef void (*funcptr)();


  //PROTOTYPES(Z80op,op);
  void op_00(); void op_01(); void op_02(); void op_03(); 
  void op_04(); void op_05(); void op_06(); void op_07(); 
  void op_08(); void op_09(); void op_0a(); void op_0b(); 
  void op_0c(); void op_0d(); void op_0e(); void op_0f(); 
  void op_10(); void op_11(); void op_12(); void op_13(); 
  void op_14(); void op_15(); void op_16(); void op_17(); 
  void op_18(); void op_19(); void op_1a(); void op_1b(); 
  void op_1c(); void op_1d(); void op_1e(); void op_1f(); 
  void op_20(); void op_21(); void op_22(); void op_23(); 
  void op_24(); void op_25(); void op_26(); void op_27(); 
  void op_28(); void op_29(); void op_2a(); void op_2b(); 
  void op_2c(); void op_2d(); void op_2e(); void op_2f(); 
  void op_30(); void op_31(); void op_32(); void op_33(); 
  void op_34(); void op_35(); void op_36(); void op_37(); 
  void op_38(); void op_39(); void op_3a(); void op_3b(); 
  void op_3c(); void op_3d(); void op_3e(); void op_3f(); 
  void op_40(); void op_41(); void op_42(); void op_43(); 
  void op_44(); void op_45(); void op_46(); void op_47(); 
  void op_48(); void op_49(); void op_4a(); void op_4b(); 
  void op_4c(); void op_4d(); void op_4e(); void op_4f(); 
  void op_50(); void op_51(); void op_52(); void op_53(); 
  void op_54(); void op_55(); void op_56(); void op_57(); 
  void op_58(); void op_59(); void op_5a(); void op_5b(); 
  void op_5c(); void op_5d(); void op_5e(); void op_5f(); 
  void op_60(); void op_61(); void op_62(); void op_63(); 

  void op_64(); void op_65(); void op_66(); void op_67(); 
  void op_68(); void op_69(); void op_6a(); void op_6b(); 
  void op_6c(); void op_6d(); void op_6e(); void op_6f(); 
  void op_70(); void op_71(); void op_72(); void op_73(); 
  void op_74(); void op_75(); void op_76(); void op_77(); 
  void op_78(); void op_79(); void op_7a(); void op_7b(); 
  void op_7c(); void op_7d(); void op_7e(); void op_7f(); 
  void op_80(); void op_81(); void op_82(); void op_83(); 
  void op_84(); void op_85(); void op_86(); void op_87(); 
  void op_88(); void op_89(); void op_8a(); void op_8b(); 
  void op_8c(); void op_8d(); void op_8e(); void op_8f(); 
  void op_90(); void op_91(); void op_92(); void op_93(); 
  void op_94(); void op_95(); void op_96(); void op_97(); 
  void op_98(); void op_99(); void op_9a(); void op_9b(); 
  void op_9c(); void op_9d(); void op_9e(); void op_9f(); 
  void op_a0(); void op_a1(); void op_a2(); void op_a3(); 
  void op_a4(); void op_a5(); void op_a6(); void op_a7(); 

  void op_a8(); void op_a9(); void op_aa(); void op_ab(); 
  void op_ac(); void op_ad(); void op_ae(); void op_af(); 
  void op_b0(); void op_b1(); void op_b2(); void op_b3(); 
  void op_b4(); void op_b5(); void op_b6(); void op_b7(); 

  void op_b8(); void op_b9(); void op_ba(); void op_bb(); 
  void op_bc(); void op_bd(); void op_be(); void op_bf(); 
  void op_c0(); void op_c1(); void op_c2(); void op_c3(); 
  void op_c4(); void op_c5(); void op_c6(); void op_c7(); 
  void op_c8(); void op_c9(); void op_ca(); void op_cb(); 
  void op_cc(); void op_cd(); void op_ce(); void op_cf(); 
  void op_d0(); void op_d1(); void op_d2(); void op_d3(); 
  void op_d4(); void op_d5(); void op_d6(); void op_d7(); 
  void op_d8(); void op_d9(); void op_da(); void op_db(); 
  void op_dc(); void op_dd(); void op_de(); void op_df(); 
  void op_e0(); void op_e1(); void op_e2(); void op_e3(); 
  void op_e4(); void op_e5(); void op_e6(); void op_e7(); 
  void op_e8(); void op_e9(); void op_ea(); void op_eb(); 
  void op_ec(); void op_ed(); void op_ee(); void op_ef(); 
  void op_f0(); void op_f1(); void op_f2(); void op_f3(); 
  void op_f4(); void op_f5(); void op_f6(); void op_f7(); 
  void op_f8(); void op_f9(); void op_fa(); void op_fb(); 
  void op_fc(); void op_fd(); void op_fe(); void op_ff(); 

static const funcptr Z80op[0x100] = {  
  op_00,op_01,op_02,op_03,op_04,op_05,op_06,op_07, 
  op_08,op_09,op_0a,op_0b,op_0c,op_0d,op_0e,op_0f, 
  op_10,op_11,op_12,op_13,op_14,op_15,op_16,op_17, 
  op_18,op_19,op_1a,op_1b,op_1c,op_1d,op_1e,op_1f, 
  op_20,op_21,op_22,op_23,op_24,op_25,op_26,op_27, 
  op_28,op_29,op_2a,op_2b,op_2c,op_2d,op_2e,op_2f, 
  op_30,op_31,op_32,op_33,op_34,op_35,op_36,op_37, 
  op_38,op_39,op_3a,op_3b,op_3c,op_3d,op_3e,op_3f, 
  op_40,op_41,op_42,op_43,op_44,op_45,op_46,op_47, 
  op_48,op_49,op_4a,op_4b,op_4c,op_4d,op_4e,op_4f, 
  op_50,op_51,op_52,op_53,op_54,op_55,op_56,op_57, 
  op_58,op_59,op_5a,op_5b,op_5c,op_5d,op_5e,op_5f, 
  op_60,op_61,op_62,op_63,op_64,op_65,op_66,op_67, 
  op_68,op_69,op_6a,op_6b,op_6c,op_6d,op_6e,op_6f, 
  op_70,op_71,op_72,op_73,op_74,op_75,op_76,op_77, 
  op_78,op_79,op_7a,op_7b,op_7c,op_7d,op_7e,op_7f, 
  op_80,op_81,op_82,op_83,op_84,op_85,op_86,op_87, 
  op_88,op_89,op_8a,op_8b,op_8c,op_8d,op_8e,op_8f, 
  op_90,op_91,op_92,op_93,op_94,op_95,op_96,op_97, 
  op_98,op_99,op_9a,op_9b,op_9c,op_9d,op_9e,op_9f, 
  op_a0,op_a1,op_a2,op_a3,op_a4,op_a5,op_a6,op_a7, 
  op_a8,op_a9,op_aa,op_ab,op_ac,op_ad,op_ae,op_af, 
  op_b0,op_b1,op_b2,op_b3,op_b4,op_b5,op_b6,op_b7, 
  op_b8,op_b9,op_ba,op_bb,op_bc,op_bd,op_be,op_bf, 
  op_c0,op_c1,op_c2,op_c3,op_c4,op_c5,op_c6,op_c7, 
  op_c8,op_c9,op_ca,op_cb,op_cc,op_cd,op_ce,op_cf, 
  op_d0,op_d1,op_d2,op_d3,op_d4,op_d5,op_d6,op_d7, 
  op_d8,op_d9,op_da,op_db,op_dc,op_dd,op_de,op_df, 
  op_e0,op_e1,op_e2,op_e3,op_e4,op_e5,op_e6,op_e7, 

  op_e8,op_e9,op_ea,op_eb,op_ec,op_ed,op_ee,op_ef, 
  op_f0,op_f1,op_f2,op_f3,op_f4,op_f5,op_f6,op_f7, 
  op_f8,op_f9,op_fa,op_fb,op_fc,op_fd,op_fe,op_ff  
};


  // PROTOTYPES(Z80cb,cb);
  void cb_00(); void cb_01(); void cb_02(); void cb_03(); 
  void cb_04(); void cb_05(); void cb_06(); void cb_07(); 
  void cb_08(); void cb_09(); void cb_0a(); void cb_0b(); 
  void cb_0c(); void cb_0d(); void cb_0e(); void cb_0f(); 
  void cb_10(); void cb_11(); void cb_12(); void cb_13(); 
  void cb_14(); void cb_15(); void cb_16(); void cb_17(); 
  void cb_18(); void cb_19(); void cb_1a(); void cb_1b(); 
  void cb_1c(); void cb_1d(); void cb_1e(); void cb_1f(); 
  void cb_20(); void cb_21(); void cb_22(); void cb_23(); 
  void cb_24(); void cb_25(); void cb_26(); void cb_27(); 
  void cb_28(); void cb_29(); void cb_2a(); void cb_2b(); 
  void cb_2c(); void cb_2d(); void cb_2e(); void cb_2f(); 
  void cb_30(); void cb_31(); void cb_32(); void cb_33(); 
  void cb_34(); void cb_35(); void cb_36(); void cb_37(); 
  void cb_38(); void cb_39(); void cb_3a(); void cb_3b(); 
  void cb_3c(); void cb_3d(); void cb_3e(); void cb_3f(); 
  void cb_40(); void cb_41(); void cb_42(); void cb_43(); 
  void cb_44(); void cb_45(); void cb_46(); void cb_47(); 
  void cb_48(); void cb_49(); void cb_4a(); void cb_4b(); 
  void cb_4c(); void cb_4d(); void cb_4e(); void cb_4f(); 
  void cb_50(); void cb_51(); void cb_52(); void cb_53(); 
  void cb_54(); void cb_55(); void cb_56(); void cb_57(); 
  void cb_58(); void cb_59(); void cb_5a(); void cb_5b(); 
  void cb_5c(); void cb_5d(); void cb_5e(); void cb_5f(); 
  void cb_60(); void cb_61(); void cb_62(); void cb_63(); 

  void cb_64(); void cb_65(); void cb_66(); void cb_67(); 
  void cb_68(); void cb_69(); void cb_6a(); void cb_6b(); 
  void cb_6c(); void cb_6d(); void cb_6e(); void cb_6f(); 
  void cb_70(); void cb_71(); void cb_72(); void cb_73(); 
  void cb_74(); void cb_75(); void cb_76(); void cb_77(); 
  void cb_78(); void cb_79(); void cb_7a(); void cb_7b(); 
  void cb_7c(); void cb_7d(); void cb_7e(); void cb_7f(); 
  void cb_80(); void cb_81(); void cb_82(); void cb_83(); 
  void cb_84(); void cb_85(); void cb_86(); void cb_87(); 
  void cb_88(); void cb_89(); void cb_8a(); void cb_8b(); 
  void cb_8c(); void cb_8d(); void cb_8e(); void cb_8f(); 
  void cb_90(); void cb_91(); void cb_92(); void cb_93(); 
  void cb_94(); void cb_95(); void cb_96(); void cb_97(); 
  void cb_98(); void cb_99(); void cb_9a(); void cb_9b(); 
  void cb_9c(); void cb_9d(); void cb_9e(); void cb_9f(); 
  void cb_a0(); void cb_a1(); void cb_a2(); void cb_a3(); 
  void cb_a4(); void cb_a5(); void cb_a6(); void cb_a7(); 

  void cb_a8(); void cb_a9(); void cb_aa(); void cb_ab(); 
  void cb_ac(); void cb_ad(); void cb_ae(); void cb_af(); 
  void cb_b0(); void cb_b1(); void cb_b2(); void cb_b3(); 
  void cb_b4(); void cb_b5(); void cb_b6(); void cb_b7(); 

  void cb_b8(); void cb_b9(); void cb_ba(); void cb_bb(); 
  void cb_bc(); void cb_bd(); void cb_be(); void cb_bf(); 
  void cb_c0(); void cb_c1(); void cb_c2(); void cb_c3(); 
  void cb_c4(); void cb_c5(); void cb_c6(); void cb_c7(); 
  void cb_c8(); void cb_c9(); void cb_ca(); void cb_cb(); 
  void cb_cc(); void cb_cd(); void cb_ce(); void cb_cf(); 
  void cb_d0(); void cb_d1(); void cb_d2(); void cb_d3(); 
  void cb_d4(); void cb_d5(); void cb_d6(); void cb_d7(); 
  void cb_d8(); void cb_d9(); void cb_da(); void cb_db(); 
  void cb_dc(); void cb_dd(); void cb_de(); void cb_df(); 
  void cb_e0(); void cb_e1(); void cb_e2(); void cb_e3(); 
  void cb_e4(); void cb_e5(); void cb_e6(); void cb_e7(); 
  void cb_e8(); void cb_e9(); void cb_ea(); void cb_eb(); 
  void cb_ec(); void cb_ed(); void cb_ee(); void cb_ef(); 
  void cb_f0(); void cb_f1(); void cb_f2(); void cb_f3(); 
  void cb_f4(); void cb_f5(); void cb_f6(); void cb_f7(); 
  void cb_f8(); void cb_f9(); void cb_fa(); void cb_fb(); 
  void cb_fc(); void cb_fd(); void cb_fe(); void cb_ff(); 

static const funcptr Z80cb[0x100] = {  
  cb_00,cb_01,cb_02,cb_03,cb_04,cb_05,cb_06,cb_07, 
  cb_08,cb_09,cb_0a,cb_0b,cb_0c,cb_0d,cb_0e,cb_0f, 
  cb_10,cb_11,cb_12,cb_13,cb_14,cb_15,cb_16,cb_17, 
  cb_18,cb_19,cb_1a,cb_1b,cb_1c,cb_1d,cb_1e,cb_1f, 
  cb_20,cb_21,cb_22,cb_23,cb_24,cb_25,cb_26,cb_27, 
  cb_28,cb_29,cb_2a,cb_2b,cb_2c,cb_2d,cb_2e,cb_2f, 
  cb_30,cb_31,cb_32,cb_33,cb_34,cb_35,cb_36,cb_37, 
  cb_38,cb_39,cb_3a,cb_3b,cb_3c,cb_3d,cb_3e,cb_3f, 
  cb_40,cb_41,cb_42,cb_43,cb_44,cb_45,cb_46,cb_47, 
  cb_48,cb_49,cb_4a,cb_4b,cb_4c,cb_4d,cb_4e,cb_4f, 
  cb_50,cb_51,cb_52,cb_53,cb_54,cb_55,cb_56,cb_57, 
  cb_58,cb_59,cb_5a,cb_5b,cb_5c,cb_5d,cb_5e,cb_5f, 
  cb_60,cb_61,cb_62,cb_63,cb_64,cb_65,cb_66,cb_67, 
  cb_68,cb_69,cb_6a,cb_6b,cb_6c,cb_6d,cb_6e,cb_6f, 
  cb_70,cb_71,cb_72,cb_73,cb_74,cb_75,cb_76,cb_77, 
  cb_78,cb_79,cb_7a,cb_7b,cb_7c,cb_7d,cb_7e,cb_7f, 
  cb_80,cb_81,cb_82,cb_83,cb_84,cb_85,cb_86,cb_87, 
  cb_88,cb_89,cb_8a,cb_8b,cb_8c,cb_8d,cb_8e,cb_8f, 
  cb_90,cb_91,cb_92,cb_93,cb_94,cb_95,cb_96,cb_97, 
  cb_98,cb_99,cb_9a,cb_9b,cb_9c,cb_9d,cb_9e,cb_9f, 
  cb_a0,cb_a1,cb_a2,cb_a3,cb_a4,cb_a5,cb_a6,cb_a7, 
  cb_a8,cb_a9,cb_aa,cb_ab,cb_ac,cb_ad,cb_ae,cb_af, 
  cb_b0,cb_b1,cb_b2,cb_b3,cb_b4,cb_b5,cb_b6,cb_b7, 
  cb_b8,cb_b9,cb_ba,cb_bb,cb_bc,cb_bd,cb_be,cb_bf, 
  cb_c0,cb_c1,cb_c2,cb_c3,cb_c4,cb_c5,cb_c6,cb_c7, 
  cb_c8,cb_c9,cb_ca,cb_cb,cb_cc,cb_cd,cb_ce,cb_cf, 
  cb_d0,cb_d1,cb_d2,cb_d3,cb_d4,cb_d5,cb_d6,cb_d7, 
  cb_d8,cb_d9,cb_da,cb_db,cb_dc,cb_dd,cb_de,cb_df, 
  cb_e0,cb_e1,cb_e2,cb_e3,cb_e4,cb_e5,cb_e6,cb_e7, 

  cb_e8,cb_e9,cb_ea,cb_eb,cb_ec,cb_ed,cb_ee,cb_ef, 
  cb_f0,cb_f1,cb_f2,cb_f3,cb_f4,cb_f5,cb_f6,cb_f7, 
  cb_f8,cb_f9,cb_fa,cb_fb,cb_fc,cb_fd,cb_fe,cb_ff 
};


// PROTOTYPES(Z80dd,dd);
  void dd_00(); void dd_01(); void dd_02(); void dd_03(); 
  void dd_04(); void dd_05(); void dd_06(); void dd_07(); 
  void dd_08(); void dd_09(); void dd_0a(); void dd_0b(); 
  void dd_0c(); void dd_0d(); void dd_0e(); void dd_0f(); 
  void dd_10(); void dd_11(); void dd_12(); void dd_13(); 
  void dd_14(); void dd_15(); void dd_16(); void dd_17(); 
  void dd_18(); void dd_19(); void dd_1a(); void dd_1b(); 
  void dd_1c(); void dd_1d(); void dd_1e(); void dd_1f(); 
  void dd_20(); void dd_21(); void dd_22(); void dd_23(); 
  void dd_24(); void dd_25(); void dd_26(); void dd_27(); 
  void dd_28(); void dd_29(); void dd_2a(); void dd_2b(); 
  void dd_2c(); void dd_2d(); void dd_2e(); void dd_2f(); 
  void dd_30(); void dd_31(); void dd_32(); void dd_33(); 
  void dd_34(); void dd_35(); void dd_36(); void dd_37(); 
  void dd_38(); void dd_39(); void dd_3a(); void dd_3b(); 
  void dd_3c(); void dd_3d(); void dd_3e(); void dd_3f(); 
  void dd_40(); void dd_41(); void dd_42(); void dd_43(); 
  void dd_44(); void dd_45(); void dd_46(); void dd_47(); 
  void dd_48(); void dd_49(); void dd_4a(); void dd_4b(); 
  void dd_4c(); void dd_4d(); void dd_4e(); void dd_4f(); 
  void dd_50(); void dd_51(); void dd_52(); void dd_53(); 
  void dd_54(); void dd_55(); void dd_56(); void dd_57(); 
  void dd_58(); void dd_59(); void dd_5a(); void dd_5b(); 
  void dd_5c(); void dd_5d(); void dd_5e(); void dd_5f(); 
  void dd_60(); void dd_61(); void dd_62(); void dd_63(); 
  void dd_64(); void dd_65(); void dd_66(); void dd_67(); 
  void dd_68(); void dd_69(); void dd_6a(); void dd_6b(); 
  void dd_6c(); void dd_6d(); void dd_6e(); void dd_6f(); 
  void dd_70(); void dd_71(); void dd_72(); void dd_73(); 
  void dd_74(); void dd_75(); void dd_76(); void dd_77(); 
  void dd_78(); void dd_79(); void dd_7a(); void dd_7b(); 
  void dd_7c(); void dd_7d(); void dd_7e(); void dd_7f(); 
  void dd_80(); void dd_81(); void dd_82(); void dd_83(); 
  void dd_84(); void dd_85(); void dd_86(); void dd_87(); 
  void dd_88(); void dd_89(); void dd_8a(); void dd_8b(); 
  void dd_8c(); void dd_8d(); void dd_8e(); void dd_8f(); 
  void dd_90(); void dd_91(); void dd_92(); void dd_93(); 
  void dd_94(); void dd_95(); void dd_96(); void dd_97(); 
  void dd_98(); void dd_99(); void dd_9a(); void dd_9b(); 
  void dd_9c(); void dd_9d(); void dd_9e(); void dd_9f(); 
  void dd_a0(); void dd_a1(); void dd_a2(); void dd_a3(); 
  void dd_a4(); void dd_a5(); void dd_a6(); void dd_a7(); 
  void dd_a8(); void dd_a9(); void dd_aa(); void dd_ab(); 
  void dd_ac(); void dd_ad(); void dd_ae(); void dd_af(); 
  void dd_b0(); void dd_b1(); void dd_b2(); void dd_b3(); 
  void dd_b4(); void dd_b5(); void dd_b6(); void dd_b7(); 
  void dd_b8(); void dd_b9(); void dd_ba(); void dd_bb(); 
  void dd_bc(); void dd_bd(); void dd_be(); void dd_bf(); 
  void dd_c0(); void dd_c1(); void dd_c2(); void dd_c3(); 
  void dd_c4(); void dd_c5(); void dd_c6(); void dd_c7(); 
  void dd_c8(); void dd_c9(); void dd_ca(); void dd_cb(); 
  void dd_cc(); void dd_cd(); void dd_ce(); void dd_cf(); 
  void dd_d0(); void dd_d1(); void dd_d2(); void dd_d3(); 
  void dd_d4(); void dd_d5(); void dd_d6(); void dd_d7(); 
  void dd_d8(); void dd_d9(); void dd_da(); void dd_db(); 
  void dd_dc(); void dd_dd(); void dd_de(); void dd_df(); 
  void dd_e0(); void dd_e1(); void dd_e2(); void dd_e3(); 
  void dd_e4(); void dd_e5(); void dd_e6(); void dd_e7(); 
  void dd_e8(); void dd_e9(); void dd_ea(); void dd_eb(); 
  void dd_ec(); void dd_ed(); void dd_ee(); void dd_ef(); 
  void dd_f0(); void dd_f1(); void dd_f2(); void dd_f3(); 
  void dd_f4(); void dd_f5(); void dd_f6(); void dd_f7(); 
  void dd_f8(); void dd_f9(); void dd_fa(); void dd_fb(); 
  void dd_fc(); void dd_fd(); void dd_fe(); void dd_ff(); 
static const funcptr Z80dd[0x100] = {  
  dd_00,dd_01,dd_02,dd_03,dd_04,dd_05,dd_06,dd_07, 
  dd_08,dd_09,dd_0a,dd_0b,dd_0c,dd_0d,dd_0e,dd_0f, 
  dd_10,dd_11,dd_12,dd_13,dd_14,dd_15,dd_16,dd_17, 
  dd_18,dd_19,dd_1a,dd_1b,dd_1c,dd_1d,dd_1e,dd_1f, 
  dd_20,dd_21,dd_22,dd_23,dd_24,dd_25,dd_26,dd_27, 
  dd_28,dd_29,dd_2a,dd_2b,dd_2c,dd_2d,dd_2e,dd_2f, 
  dd_30,dd_31,dd_32,dd_33,dd_34,dd_35,dd_36,dd_37, 
  dd_38,dd_39,dd_3a,dd_3b,dd_3c,dd_3d,dd_3e,dd_3f, 
  dd_40,dd_41,dd_42,dd_43,dd_44,dd_45,dd_46,dd_47, 
  dd_48,dd_49,dd_4a,dd_4b,dd_4c,dd_4d,dd_4e,dd_4f, 
  dd_50,dd_51,dd_52,dd_53,dd_54,dd_55,dd_56,dd_57, 
  dd_58,dd_59,dd_5a,dd_5b,dd_5c,dd_5d,dd_5e,dd_5f, 
  dd_60,dd_61,dd_62,dd_63,dd_64,dd_65,dd_66,dd_67, 
  dd_68,dd_69,dd_6a,dd_6b,dd_6c,dd_6d,dd_6e,dd_6f, 
  dd_70,dd_71,dd_72,dd_73,dd_74,dd_75,dd_76,dd_77, 
  dd_78,dd_79,dd_7a,dd_7b,dd_7c,dd_7d,dd_7e,dd_7f, 
  dd_80,dd_81,dd_82,dd_83,dd_84,dd_85,dd_86,dd_87, 
  dd_88,dd_89,dd_8a,dd_8b,dd_8c,dd_8d,dd_8e,dd_8f, 
  dd_90,dd_91,dd_92,dd_93,dd_94,dd_95,dd_96,dd_97, 
  dd_98,dd_99,dd_9a,dd_9b,dd_9c,dd_9d,dd_9e,dd_9f, 
  dd_a0,dd_a1,dd_a2,dd_a3,dd_a4,dd_a5,dd_a6,dd_a7, 
  dd_a8,dd_a9,dd_aa,dd_ab,dd_ac,dd_ad,dd_ae,dd_af, 
  dd_b0,dd_b1,dd_b2,dd_b3,dd_b4,dd_b5,dd_b6,dd_b7, 
  dd_b8,dd_b9,dd_ba,dd_bb,dd_bc,dd_bd,dd_be,dd_bf, 
  dd_c0,dd_c1,dd_c2,dd_c3,dd_c4,dd_c5,dd_c6,dd_c7, 
  dd_c8,dd_c9,dd_ca,dd_cb,dd_cc,dd_cd,dd_ce,dd_cf, 
  dd_d0,dd_d1,dd_d2,dd_d3,dd_d4,dd_d5,dd_d6,dd_d7, 
  dd_d8,dd_d9,dd_da,dd_db,dd_dc,dd_dd,dd_de,dd_df, 
  dd_e0,dd_e1,dd_e2,dd_e3,dd_e4,dd_e5,dd_e6,dd_e7, 
  dd_e8,dd_e9,dd_ea,dd_eb,dd_ec,dd_ed,dd_ee,dd_ef, 
  dd_f0,dd_f1,dd_f2,dd_f3,dd_f4,dd_f5,dd_f6,dd_f7, 
  dd_f8,dd_f9,dd_fa,dd_fb,dd_fc,dd_fd,dd_fe,dd_ff  
};


// PROTOTYPES(Z80ed,ed);
  void ed_00(); void ed_01(); void ed_02(); void ed_03(); 
  void ed_04(); void ed_05(); void ed_06(); void ed_07(); 
  void ed_08(); void ed_09(); void ed_0a(); void ed_0b(); 
  void ed_0c(); void ed_0d(); void ed_0e(); void ed_0f(); 
  void ed_10(); void ed_11(); void ed_12(); void ed_13(); 
  void ed_14(); void ed_15(); void ed_16(); void ed_17(); 
  void ed_18(); void ed_19(); void ed_1a(); void ed_1b(); 
  void ed_1c(); void ed_1d(); void ed_1e(); void ed_1f(); 
  void ed_20(); void ed_21(); void ed_22(); void ed_23(); 
  void ed_24(); void ed_25(); void ed_26(); void ed_27(); 
  void ed_28(); void ed_29(); void ed_2a(); void ed_2b(); 
  void ed_2c(); void ed_2d(); void ed_2e(); void ed_2f(); 
  void ed_30(); void ed_31(); void ed_32(); void ed_33(); 
  void ed_34(); void ed_35(); void ed_36(); void ed_37(); 
  void ed_38(); void ed_39(); void ed_3a(); void ed_3b(); 
  void ed_3c(); void ed_3d(); void ed_3e(); void ed_3f(); 
  void ed_40(); void ed_41(); void ed_42(); void ed_43(); 
  void ed_44(); void ed_45(); void ed_46(); void ed_47(); 
  void ed_48(); void ed_49(); void ed_4a(); void ed_4b(); 
  void ed_4c(); void ed_4d(); void ed_4e(); void ed_4f(); 
  void ed_50(); void ed_51(); void ed_52(); void ed_53(); 
  void ed_54(); void ed_55(); void ed_56(); void ed_57(); 
  void ed_58(); void ed_59(); void ed_5a(); void ed_5b(); 
  void ed_5c(); void ed_5d(); void ed_5e(); void ed_5f(); 
  void ed_60(); void ed_61(); void ed_62(); void ed_63(); 
  void ed_64(); void ed_65(); void ed_66(); void ed_67(); 
  void ed_68(); void ed_69(); void ed_6a(); void ed_6b(); 
  void ed_6c(); void ed_6d(); void ed_6e(); void ed_6f(); 
  void ed_70(); void ed_71(); void ed_72(); void ed_73(); 
  void ed_74(); void ed_75(); void ed_76(); void ed_77(); 
  void ed_78(); void ed_79(); void ed_7a(); void ed_7b(); 
  void ed_7c(); void ed_7d(); void ed_7e(); void ed_7f(); 
  void ed_80(); void ed_81(); void ed_82(); void ed_83(); 
  void ed_84(); void ed_85(); void ed_86(); void ed_87(); 
  void ed_88(); void ed_89(); void ed_8a(); void ed_8b(); 
  void ed_8c(); void ed_8d(); void ed_8e(); void ed_8f(); 
  void ed_90(); void ed_91(); void ed_92(); void ed_93(); 
  void ed_94(); void ed_95(); void ed_96(); void ed_97(); 
  void ed_98(); void ed_99(); void ed_9a(); void ed_9b(); 
  void ed_9c(); void ed_9d(); void ed_9e(); void ed_9f(); 
  void ed_a0(); void ed_a1(); void ed_a2(); void ed_a3(); 
  void ed_a4(); void ed_a5(); void ed_a6(); void ed_a7(); 
  void ed_a8(); void ed_a9(); void ed_aa(); void ed_ab(); 
  void ed_ac(); void ed_ad(); void ed_ae(); void ed_af(); 
  void ed_b0(); void ed_b1(); void ed_b2(); void ed_b3(); 
  void ed_b4(); void ed_b5(); void ed_b6(); void ed_b7(); 
  void ed_b8(); void ed_b9(); void ed_ba(); void ed_bb(); 
  void ed_bc(); void ed_bd(); void ed_be(); void ed_bf(); 
  void ed_c0(); void ed_c1(); void ed_c2(); void ed_c3(); 
  void ed_c4(); void ed_c5(); void ed_c6(); void ed_c7(); 
  void ed_c8(); void ed_c9(); void ed_ca(); void ed_cb(); 
  void ed_cc(); void ed_cd(); void ed_ce(); void ed_cf(); 
  void ed_d0(); void ed_d1(); void ed_d2(); void ed_d3(); 
  void ed_d4(); void ed_d5(); void ed_d6(); void ed_d7(); 
  void ed_d8(); void ed_d9(); void ed_da(); void ed_db(); 
  void ed_dc(); void ed_dd(); void ed_de(); void ed_df(); 
  void ed_e0(); void ed_e1(); void ed_e2(); void ed_e3(); 
  void ed_e4(); void ed_e5(); void ed_e6(); void ed_e7(); 
  void ed_e8(); void ed_e9(); void ed_ea(); void ed_eb(); 
  void ed_ec(); void ed_ed(); void ed_ee(); void ed_ef(); 
  void ed_f0(); void ed_f1(); void ed_f2(); void ed_f3(); 
  void ed_f4(); void ed_f5(); void ed_f6(); void ed_f7(); 
  void ed_f8(); void ed_f9(); void ed_fa(); void ed_fb(); 
  void ed_fc(); void ed_fd(); void ed_fe(); void ed_ff(); 
static const funcptr Z80ed[0x100] = {  
  ed_00,ed_01,ed_02,ed_03,ed_04,ed_05,ed_06,ed_07, 
  ed_08,ed_09,ed_0a,ed_0b,ed_0c,ed_0d,ed_0e,ed_0f, 
  ed_10,ed_11,ed_12,ed_13,ed_14,ed_15,ed_16,ed_17, 
  ed_18,ed_19,ed_1a,ed_1b,ed_1c,ed_1d,ed_1e,ed_1f, 
  ed_20,ed_21,ed_22,ed_23,ed_24,ed_25,ed_26,ed_27, 
  ed_28,ed_29,ed_2a,ed_2b,ed_2c,ed_2d,ed_2e,ed_2f, 
  ed_30,ed_31,ed_32,ed_33,ed_34,ed_35,ed_36,ed_37, 
  ed_38,ed_39,ed_3a,ed_3b,ed_3c,ed_3d,ed_3e,ed_3f, 
  ed_40,ed_41,ed_42,ed_43,ed_44,ed_45,ed_46,ed_47, 
  ed_48,ed_49,ed_4a,ed_4b,ed_4c,ed_4d,ed_4e,ed_4f, 
  ed_50,ed_51,ed_52,ed_53,ed_54,ed_55,ed_56,ed_57, 
  ed_58,ed_59,ed_5a,ed_5b,ed_5c,ed_5d,ed_5e,ed_5f, 
  ed_60,ed_61,ed_62,ed_63,ed_64,ed_65,ed_66,ed_67, 
  ed_68,ed_69,ed_6a,ed_6b,ed_6c,ed_6d,ed_6e,ed_6f, 
  ed_70,ed_71,ed_72,ed_73,ed_74,ed_75,ed_76,ed_77, 
  ed_78,ed_79,ed_7a,ed_7b,ed_7c,ed_7d,ed_7e,ed_7f, 
  ed_80,ed_81,ed_82,ed_83,ed_84,ed_85,ed_86,ed_87, 
  ed_88,ed_89,ed_8a,ed_8b,ed_8c,ed_8d,ed_8e,ed_8f, 
  ed_90,ed_91,ed_92,ed_93,ed_94,ed_95,ed_96,ed_97, 
  ed_98,ed_99,ed_9a,ed_9b,ed_9c,ed_9d,ed_9e,ed_9f, 
  ed_a0,ed_a1,ed_a2,ed_a3,ed_a4,ed_a5,ed_a6,ed_a7, 
  ed_a8,ed_a9,ed_aa,ed_ab,ed_ac,ed_ad,ed_ae,ed_af, 
  ed_b0,ed_b1,ed_b2,ed_b3,ed_b4,ed_b5,ed_b6,ed_b7, 
  ed_b8,ed_b9,ed_ba,ed_bb,ed_bc,ed_bd,ed_be,ed_bf, 
  ed_c0,ed_c1,ed_c2,ed_c3,ed_c4,ed_c5,ed_c6,ed_c7, 
  ed_c8,ed_c9,ed_ca,ed_cb,ed_cc,ed_cd,ed_ce,ed_cf, 
  ed_d0,ed_d1,ed_d2,ed_d3,ed_d4,ed_d5,ed_d6,ed_d7, 
  ed_d8,ed_d9,ed_da,ed_db,ed_dc,ed_dd,ed_de,ed_df, 
  ed_e0,ed_e1,ed_e2,ed_e3,ed_e4,ed_e5,ed_e6,ed_e7, 
  ed_e8,ed_e9,ed_ea,ed_eb,ed_ec,ed_ed,ed_ee,ed_ef, 
  ed_f0,ed_f1,ed_f2,ed_f3,ed_f4,ed_f5,ed_f6,ed_f7, 
  ed_f8,ed_f9,ed_fa,ed_fb,ed_fc,ed_fd,ed_fe,ed_ff  
};

// PROTOTYPES(Z80fd,fd);
  void fd_00(); void fd_01(); void fd_02(); void fd_03(); 
  void fd_04(); void fd_05(); void fd_06(); void fd_07(); 
  void fd_08(); void fd_09(); void fd_0a(); void fd_0b(); 
  void fd_0c(); void fd_0d(); void fd_0e(); void fd_0f(); 
  void fd_10(); void fd_11(); void fd_12(); void fd_13(); 
  void fd_14(); void fd_15(); void fd_16(); void fd_17(); 
  void fd_18(); void fd_19(); void fd_1a(); void fd_1b(); 
  void fd_1c(); void fd_1d(); void fd_1e(); void fd_1f(); 
  void fd_20(); void fd_21(); void fd_22(); void fd_23(); 
  void fd_24(); void fd_25(); void fd_26(); void fd_27(); 
  void fd_28(); void fd_29(); void fd_2a(); void fd_2b(); 
  void fd_2c(); void fd_2d(); void fd_2e(); void fd_2f(); 
  void fd_30(); void fd_31(); void fd_32(); void fd_33(); 
  void fd_34(); void fd_35(); void fd_36(); void fd_37(); 
  void fd_38(); void fd_39(); void fd_3a(); void fd_3b(); 
  void fd_3c(); void fd_3d(); void fd_3e(); void fd_3f(); 
  void fd_40(); void fd_41(); void fd_42(); void fd_43(); 
  void fd_44(); void fd_45(); void fd_46(); void fd_47(); 
  void fd_48(); void fd_49(); void fd_4a(); void fd_4b(); 
  void fd_4c(); void fd_4d(); void fd_4e(); void fd_4f(); 
  void fd_50(); void fd_51(); void fd_52(); void fd_53(); 
  void fd_54(); void fd_55(); void fd_56(); void fd_57(); 
  void fd_58(); void fd_59(); void fd_5a(); void fd_5b(); 
  void fd_5c(); void fd_5d(); void fd_5e(); void fd_5f(); 
  void fd_60(); void fd_61(); void fd_62(); void fd_63(); 
  void fd_64(); void fd_65(); void fd_66(); void fd_67(); 
  void fd_68(); void fd_69(); void fd_6a(); void fd_6b(); 
  void fd_6c(); void fd_6d(); void fd_6e(); void fd_6f(); 
  void fd_70(); void fd_71(); void fd_72(); void fd_73(); 
  void fd_74(); void fd_75(); void fd_76(); void fd_77(); 
  void fd_78(); void fd_79(); void fd_7a(); void fd_7b(); 
  void fd_7c(); void fd_7d(); void fd_7e(); void fd_7f(); 
  void fd_80(); void fd_81(); void fd_82(); void fd_83(); 
  void fd_84(); void fd_85(); void fd_86(); void fd_87(); 
  void fd_88(); void fd_89(); void fd_8a(); void fd_8b(); 
  void fd_8c(); void fd_8d(); void fd_8e(); void fd_8f(); 
  void fd_90(); void fd_91(); void fd_92(); void fd_93(); 
  void fd_94(); void fd_95(); void fd_96(); void fd_97(); 
  void fd_98(); void fd_99(); void fd_9a(); void fd_9b(); 
  void fd_9c(); void fd_9d(); void fd_9e(); void fd_9f(); 
  void fd_a0(); void fd_a1(); void fd_a2(); void fd_a3(); 
  void fd_a4(); void fd_a5(); void fd_a6(); void fd_a7(); 
  void fd_a8(); void fd_a9(); void fd_aa(); void fd_ab(); 
  void fd_ac(); void fd_ad(); void fd_ae(); void fd_af(); 
  void fd_b0(); void fd_b1(); void fd_b2(); void fd_b3(); 
  void fd_b4(); void fd_b5(); void fd_b6(); void fd_b7(); 
  void fd_b8(); void fd_b9(); void fd_ba(); void fd_bb(); 
  void fd_bc(); void fd_bd(); void fd_be(); void fd_bf(); 
  void fd_c0(); void fd_c1(); void fd_c2(); void fd_c3(); 
  void fd_c4(); void fd_c5(); void fd_c6(); void fd_c7(); 
  void fd_c8(); void fd_c9(); void fd_ca(); void fd_cb(); 
  void fd_cc(); void fd_cd(); void fd_ce(); void fd_cf(); 
  void fd_d0(); void fd_d1(); void fd_d2(); void fd_d3(); 
  void fd_d4(); void fd_d5(); void fd_d6(); void fd_d7(); 
  void fd_d8(); void fd_d9(); void fd_da(); void fd_db(); 
  void fd_dc(); void fd_dd(); void fd_de(); void fd_df(); 
  void fd_e0(); void fd_e1(); void fd_e2(); void fd_e3(); 
  void fd_e4(); void fd_e5(); void fd_e6(); void fd_e7(); 
  void fd_e8(); void fd_e9(); void fd_ea(); void fd_eb(); 
  void fd_ec(); void fd_ed(); void fd_ee(); void fd_ef(); 
  void fd_f0(); void fd_f1(); void fd_f2(); void fd_f3(); 
  void fd_f4(); void fd_f5(); void fd_f6(); void fd_f7(); 
  void fd_f8(); void fd_f9(); void fd_fa(); void fd_fb(); 
  void fd_fc(); void fd_fd(); void fd_fe(); void fd_ff(); 
static const funcptr Z80fd[0x100] = {  
  fd_00,fd_01,fd_02,fd_03,fd_04,fd_05,fd_06,fd_07, 
  fd_08,fd_09,fd_0a,fd_0b,fd_0c,fd_0d,fd_0e,fd_0f, 
  fd_10,fd_11,fd_12,fd_13,fd_14,fd_15,fd_16,fd_17, 
  fd_18,fd_19,fd_1a,fd_1b,fd_1c,fd_1d,fd_1e,fd_1f, 
  fd_20,fd_21,fd_22,fd_23,fd_24,fd_25,fd_26,fd_27, 
  fd_28,fd_29,fd_2a,fd_2b,fd_2c,fd_2d,fd_2e,fd_2f, 
  fd_30,fd_31,fd_32,fd_33,fd_34,fd_35,fd_36,fd_37, 
  fd_38,fd_39,fd_3a,fd_3b,fd_3c,fd_3d,fd_3e,fd_3f, 
  fd_40,fd_41,fd_42,fd_43,fd_44,fd_45,fd_46,fd_47, 
  fd_48,fd_49,fd_4a,fd_4b,fd_4c,fd_4d,fd_4e,fd_4f, 
  fd_50,fd_51,fd_52,fd_53,fd_54,fd_55,fd_56,fd_57, 
  fd_58,fd_59,fd_5a,fd_5b,fd_5c,fd_5d,fd_5e,fd_5f, 
  fd_60,fd_61,fd_62,fd_63,fd_64,fd_65,fd_66,fd_67, 
  fd_68,fd_69,fd_6a,fd_6b,fd_6c,fd_6d,fd_6e,fd_6f, 
  fd_70,fd_71,fd_72,fd_73,fd_74,fd_75,fd_76,fd_77, 
  fd_78,fd_79,fd_7a,fd_7b,fd_7c,fd_7d,fd_7e,fd_7f, 
  fd_80,fd_81,fd_82,fd_83,fd_84,fd_85,fd_86,fd_87, 
  fd_88,fd_89,fd_8a,fd_8b,fd_8c,fd_8d,fd_8e,fd_8f, 
  fd_90,fd_91,fd_92,fd_93,fd_94,fd_95,fd_96,fd_97, 
  fd_98,fd_99,fd_9a,fd_9b,fd_9c,fd_9d,fd_9e,fd_9f, 
  fd_a0,fd_a1,fd_a2,fd_a3,fd_a4,fd_a5,fd_a6,fd_a7, 
  fd_a8,fd_a9,fd_aa,fd_ab,fd_ac,fd_ad,fd_ae,fd_af, 
  fd_b0,fd_b1,fd_b2,fd_b3,fd_b4,fd_b5,fd_b6,fd_b7, 
  fd_b8,fd_b9,fd_ba,fd_bb,fd_bc,fd_bd,fd_be,fd_bf, 
  fd_c0,fd_c1,fd_c2,fd_c3,fd_c4,fd_c5,fd_c6,fd_c7, 
  fd_c8,fd_c9,fd_ca,fd_cb,fd_cc,fd_cd,fd_ce,fd_cf, 
  fd_d0,fd_d1,fd_d2,fd_d3,fd_d4,fd_d5,fd_d6,fd_d7, 
  fd_d8,fd_d9,fd_da,fd_db,fd_dc,fd_dd,fd_de,fd_df, 
  fd_e0,fd_e1,fd_e2,fd_e3,fd_e4,fd_e5,fd_e6,fd_e7, 
  fd_e8,fd_e9,fd_ea,fd_eb,fd_ec,fd_ed,fd_ee,fd_ef, 
  fd_f0,fd_f1,fd_f2,fd_f3,fd_f4,fd_f5,fd_f6,fd_f7, 
  fd_f8,fd_f9,fd_fa,fd_fb,fd_fc,fd_fd,fd_fe,fd_ff  
};


// PROTOTYPES(Z80xycb,xycb);
  void xycb_00(); void xycb_01(); void xycb_02(); void xycb_03(); 
  void xycb_04(); void xycb_05(); void xycb_06(); void xycb_07(); 
  void xycb_08(); void xycb_09(); void xycb_0a(); void xycb_0b(); 
  void xycb_0c(); void xycb_0d(); void xycb_0e(); void xycb_0f(); 
  void xycb_10(); void xycb_11(); void xycb_12(); void xycb_13(); 
  void xycb_14(); void xycb_15(); void xycb_16(); void xycb_17(); 
  void xycb_18(); void xycb_19(); void xycb_1a(); void xycb_1b(); 
  void xycb_1c(); void xycb_1d(); void xycb_1e(); void xycb_1f(); 
  void xycb_20(); void xycb_21(); void xycb_22(); void xycb_23(); 
  void xycb_24(); void xycb_25(); void xycb_26(); void xycb_27(); 
  void xycb_28(); void xycb_29(); void xycb_2a(); void xycb_2b(); 
  void xycb_2c(); void xycb_2d(); void xycb_2e(); void xycb_2f(); 
  void xycb_30(); void xycb_31(); void xycb_32(); void xycb_33(); 
  void xycb_34(); void xycb_35(); void xycb_36(); void xycb_37(); 
  void xycb_38(); void xycb_39(); void xycb_3a(); void xycb_3b(); 
  void xycb_3c(); void xycb_3d(); void xycb_3e(); void xycb_3f(); 
  void xycb_40(); void xycb_41(); void xycb_42(); void xycb_43(); 
  void xycb_44(); void xycb_45(); void xycb_46(); void xycb_47(); 
  void xycb_48(); void xycb_49(); void xycb_4a(); void xycb_4b(); 
  void xycb_4c(); void xycb_4d(); void xycb_4e(); void xycb_4f(); 
  void xycb_50(); void xycb_51(); void xycb_52(); void xycb_53(); 
  void xycb_54(); void xycb_55(); void xycb_56(); void xycb_57(); 
  void xycb_58(); void xycb_59(); void xycb_5a(); void xycb_5b(); 
  void xycb_5c(); void xycb_5d(); void xycb_5e(); void xycb_5f(); 
  void xycb_60(); void xycb_61(); void xycb_62(); void xycb_63(); 
  void xycb_64(); void xycb_65(); void xycb_66(); void xycb_67(); 
  void xycb_68(); void xycb_69(); void xycb_6a(); void xycb_6b(); 
  void xycb_6c(); void xycb_6d(); void xycb_6e(); void xycb_6f(); 
  void xycb_70(); void xycb_71(); void xycb_72(); void xycb_73(); 
  void xycb_74(); void xycb_75(); void xycb_76(); void xycb_77(); 
  void xycb_78(); void xycb_79(); void xycb_7a(); void xycb_7b(); 
  void xycb_7c(); void xycb_7d(); void xycb_7e(); void xycb_7f(); 
  void xycb_80(); void xycb_81(); void xycb_82(); void xycb_83(); 
  void xycb_84(); void xycb_85(); void xycb_86(); void xycb_87(); 
  void xycb_88(); void xycb_89(); void xycb_8a(); void xycb_8b(); 
  void xycb_8c(); void xycb_8d(); void xycb_8e(); void xycb_8f(); 
  void xycb_90(); void xycb_91(); void xycb_92(); void xycb_93(); 
  void xycb_94(); void xycb_95(); void xycb_96(); void xycb_97(); 
  void xycb_98(); void xycb_99(); void xycb_9a(); void xycb_9b(); 
  void xycb_9c(); void xycb_9d(); void xycb_9e(); void xycb_9f(); 
  void xycb_a0(); void xycb_a1(); void xycb_a2(); void xycb_a3(); 
  void xycb_a4(); void xycb_a5(); void xycb_a6(); void xycb_a7(); 
  void xycb_a8(); void xycb_a9(); void xycb_aa(); void xycb_ab(); 
  void xycb_ac(); void xycb_ad(); void xycb_ae(); void xycb_af(); 
  void xycb_b0(); void xycb_b1(); void xycb_b2(); void xycb_b3(); 
  void xycb_b4(); void xycb_b5(); void xycb_b6(); void xycb_b7(); 
  void xycb_b8(); void xycb_b9(); void xycb_ba(); void xycb_bb(); 
  void xycb_bc(); void xycb_bd(); void xycb_be(); void xycb_bf(); 
  void xycb_c0(); void xycb_c1(); void xycb_c2(); void xycb_c3(); 
  void xycb_c4(); void xycb_c5(); void xycb_c6(); void xycb_c7(); 
  void xycb_c8(); void xycb_c9(); void xycb_ca(); void xycb_cb(); 
  void xycb_cc(); void xycb_cd(); void xycb_ce(); void xycb_cf(); 
  void xycb_d0(); void xycb_d1(); void xycb_d2(); void xycb_d3(); 
  void xycb_d4(); void xycb_d5(); void xycb_d6(); void xycb_d7(); 
  void xycb_d8(); void xycb_d9(); void xycb_da(); void xycb_db(); 
  void xycb_dc(); void xycb_dd(); void xycb_de(); void xycb_df(); 
  void xycb_e0(); void xycb_e1(); void xycb_e2(); void xycb_e3(); 
  void xycb_e4(); void xycb_e5(); void xycb_e6(); void xycb_e7(); 
  void xycb_e8(); void xycb_e9(); void xycb_ea(); void xycb_eb(); 
  void xycb_ec(); void xycb_ed(); void xycb_ee(); void xycb_ef(); 
  void xycb_f0(); void xycb_f1(); void xycb_f2(); void xycb_f3(); 
  void xycb_f4(); void xycb_f5(); void xycb_f6(); void xycb_f7(); 
  void xycb_f8(); void xycb_f9(); void xycb_fa(); void xycb_fb(); 
  void xycb_fc(); void xycb_fd(); void xycb_fe(); void xycb_ff(); 
static const funcptr Z80xycb[0x100] = {  
  xycb_00,xycb_01,xycb_02,xycb_03,xycb_04,xycb_05,xycb_06,xycb_07, 
  xycb_08,xycb_09,xycb_0a,xycb_0b,xycb_0c,xycb_0d,xycb_0e,xycb_0f, 
  xycb_10,xycb_11,xycb_12,xycb_13,xycb_14,xycb_15,xycb_16,xycb_17, 
  xycb_18,xycb_19,xycb_1a,xycb_1b,xycb_1c,xycb_1d,xycb_1e,xycb_1f, 
  xycb_20,xycb_21,xycb_22,xycb_23,xycb_24,xycb_25,xycb_26,xycb_27, 
  xycb_28,xycb_29,xycb_2a,xycb_2b,xycb_2c,xycb_2d,xycb_2e,xycb_2f, 
  xycb_30,xycb_31,xycb_32,xycb_33,xycb_34,xycb_35,xycb_36,xycb_37, 
  xycb_38,xycb_39,xycb_3a,xycb_3b,xycb_3c,xycb_3d,xycb_3e,xycb_3f, 
  xycb_40,xycb_41,xycb_42,xycb_43,xycb_44,xycb_45,xycb_46,xycb_47, 
  xycb_48,xycb_49,xycb_4a,xycb_4b,xycb_4c,xycb_4d,xycb_4e,xycb_4f, 
  xycb_50,xycb_51,xycb_52,xycb_53,xycb_54,xycb_55,xycb_56,xycb_57, 
  xycb_58,xycb_59,xycb_5a,xycb_5b,xycb_5c,xycb_5d,xycb_5e,xycb_5f, 
  xycb_60,xycb_61,xycb_62,xycb_63,xycb_64,xycb_65,xycb_66,xycb_67, 
  xycb_68,xycb_69,xycb_6a,xycb_6b,xycb_6c,xycb_6d,xycb_6e,xycb_6f, 
  xycb_70,xycb_71,xycb_72,xycb_73,xycb_74,xycb_75,xycb_76,xycb_77, 
  xycb_78,xycb_79,xycb_7a,xycb_7b,xycb_7c,xycb_7d,xycb_7e,xycb_7f, 
  xycb_80,xycb_81,xycb_82,xycb_83,xycb_84,xycb_85,xycb_86,xycb_87, 
  xycb_88,xycb_89,xycb_8a,xycb_8b,xycb_8c,xycb_8d,xycb_8e,xycb_8f, 
  xycb_90,xycb_91,xycb_92,xycb_93,xycb_94,xycb_95,xycb_96,xycb_97, 
  xycb_98,xycb_99,xycb_9a,xycb_9b,xycb_9c,xycb_9d,xycb_9e,xycb_9f, 
  xycb_a0,xycb_a1,xycb_a2,xycb_a3,xycb_a4,xycb_a5,xycb_a6,xycb_a7, 
  xycb_a8,xycb_a9,xycb_aa,xycb_ab,xycb_ac,xycb_ad,xycb_ae,xycb_af, 
  xycb_b0,xycb_b1,xycb_b2,xycb_b3,xycb_b4,xycb_b5,xycb_b6,xycb_b7, 
  xycb_b8,xycb_b9,xycb_ba,xycb_bb,xycb_bc,xycb_bd,xycb_be,xycb_bf, 
  xycb_c0,xycb_c1,xycb_c2,xycb_c3,xycb_c4,xycb_c5,xycb_c6,xycb_c7, 
  xycb_c8,xycb_c9,xycb_ca,xycb_cb,xycb_cc,xycb_cd,xycb_ce,xycb_cf, 
  xycb_d0,xycb_d1,xycb_d2,xycb_d3,xycb_d4,xycb_d5,xycb_d6,xycb_d7, 
  xycb_d8,xycb_d9,xycb_da,xycb_db,xycb_dc,xycb_dd,xycb_de,xycb_df, 
  xycb_e0,xycb_e1,xycb_e2,xycb_e3,xycb_e4,xycb_e5,xycb_e6,xycb_e7, 
  xycb_e8,xycb_e9,xycb_ea,xycb_eb,xycb_ec,xycb_ed,xycb_ee,xycb_ef, 
  xycb_f0,xycb_f1,xycb_f2,xycb_f3,xycb_f4,xycb_f5,xycb_f6,xycb_f7, 
  xycb_f8,xycb_f9,xycb_fa,xycb_fb,xycb_fc,xycb_fd,xycb_fe,xycb_ff  
};

/****************************************************************************/
/* Burn an odd amount of cycles, that is instructions taking something    */
/* different from 4 T-states per opcode (and R increment)          */
/****************************************************************************/
void BURNODD(s32 cycles, s32 opcodes, s32 cyclesum)
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
void CC_op(u8 opcode) { Z80.cycles += cc[Z80_TABLE_op][opcode]; }
void CC_cb(u8 opcode) { Z80.cycles += cc[Z80_TABLE_cb][opcode]; }
void CC_dd(u8 opcode) { Z80.cycles += cc[Z80_TABLE_dd][opcode]; }
void CC_ed(u8 opcode) { Z80.cycles += cc[Z80_TABLE_ed][opcode]; }
void CC_fd(u8 opcode) { Z80.cycles += cc[Z80_TABLE_fd][opcode]; }
void CC_xy(u8 opcode) { Z80.cycles += cc[Z80_TABLE_xy][opcode]; }
void CC_xycb(u8 opcode) { Z80.cycles += cc[Z80_TABLE_xycb][opcode]; }
void CC_ex(u8 opcode) { Z80.cycles += cc[Z80_TABLE_ex][opcode]; }

/***************************************************************
 * execute an opcode
 ***************************************************************/
void EXEC_op(u8 opcode) {
  CC_op(opcode);
  (*Z80op[opcode])();
}

void EXEC_cb(u8 opcode) {
  CC_cb(opcode);
  (*Z80cb[opcode])();
}

void EXEC_dd(u8 opcode) {
  CC_dd(opcode);
  (*Z80dd[opcode])();
}

void EXEC_ed(u8 opcode) {
  CC_ed(opcode);
  (*Z80ed[opcode])();
}

void EXEC_fd(u8 opcode) {
  CC_fd(opcode);
  (*Z80fd[opcode])();
}

void EXEC_xycb(u8 opcode) {
  CC_xycb(opcode);
  (*Z80xycb[opcode])();
}

/***************************************************************
 * Enter HALT state; write 1 to fake port on first execution
 ***************************************************************/
void ENTER_HALT() {
  PC--;
  HALT = 1;
}

/***************************************************************
 * Leave HALT state; write 0 to fake port
 ***************************************************************/
void LEAVE_HALT() {
  if( HALT )
  {
    HALT = 0;
    PC++;
  }
}

/***************************************************************
 * Input a byte from given I/O port
 ***************************************************************/
u8 IN(u32 port) {
	return z80_readport(port);
}

/***************************************************************
 * Output a byte to given I/O port
 ***************************************************************/
void OUT(u32 port, u8 value) {
	z80_writeport(port, value);
}

/***************************************************************
 * Read a byte from given memory location
 ***************************************************************/
u8 RM(u32 addr) {
	return z80_readmem(addr);
}

/***************************************************************
 * Write a byte to given memory location
 ***************************************************************/
void WM(u32 address, u8 value) {
	z80_writemem(address, value);
}

/***************************************************************
 * Read a word from given memory location
 ***************************************************************/
void RM16( u32 addr, PAIR *r )
{
  r->b.l = RM(addr);
  r->b.h = RM((addr+1)&0xffff);
}

/***************************************************************
 * Write a word to given memory location
 ***************************************************************/
void WM16( u32 addr, PAIR *r )
{
  WM(addr,r->b.l);
  WM((addr+1)&0xffff,r->b.h);
}

/***************************************************************
 * ROP() is identical to RM() except it is used for
 * reading opcodes. In case of system with memory mapped I/O,
 * this function can be used to greatly speed up emulation
 ***************************************************************/
u8 ROP()
{
  u32 pc = PCD;
  PC++;
  return cpu_readop(pc);
}

/****************************************************************
 * ARG() is identical to ROP() except it is used
 * for reading opcode arguments. This difference can be used to
 * support systems that use different encoding mechanisms for
 * opcodes and opcode arguments
 ***************************************************************/
u8 ARG()
{
  u32 pc = PCD;
  PC++;
  return cpu_readop_arg(pc);
}

u32 ARG16()
{
  u32 pc = PCD;
  PC += 2;
  return cpu_readop_arg(pc) | (cpu_readop_arg((pc+1)&0xffff) << 8);
}

/***************************************************************
 * Calculate the effective address EA of an opcode using
 * IX+offset resp. IY+offset addressing.
 ***************************************************************/
void EAX() {
    EA = (u32)(u16)(IX + (s8)ARG());
    WZ = EA;
}

void EAY() {
    EA = (u32)(u16)(IY + (s8)ARG());
    WZ = EA;
}

/***************************************************************
 * POP
 ***************************************************************/
void POP_af() {
    RM16( SPD, &Z80.af );
    SP += 2;
}

void POP_bc() {
    RM16( SPD, &Z80.bc );
    SP += 2;
}

void POP_de() {
    RM16( SPD, &Z80.de );
    SP += 2;
}

void POP_hl() {
    RM16( SPD, &Z80.hl );
    SP += 2;
}

void POP_iy() {
    RM16( SPD, &Z80.iy );
    SP += 2;
}

void POP_ix() {
    RM16( SPD, &Z80.ix );
    SP += 2;
}

void POP_pc() {
    RM16( SPD, &Z80.pc );
    SP += 2;
}

/***************************************************************
 * PUSH
 ***************************************************************/
void PUSH_af() {
	SP -= 2;
	WM16( SPD, &Z80.af );
}

void PUSH_bc() {
	SP -= 2;
	WM16( SPD, &Z80.bc );
}

void PUSH_de() {
	SP -= 2;
	WM16( SPD, &Z80.de );
}

void PUSH_hl() {
	SP -= 2;
	WM16( SPD, &Z80.hl );
}

void PUSH_iy() {
	SP -= 2;
	WM16( SPD, &Z80.iy );
}

void PUSH_ix() {
	SP -= 2;
	WM16( SPD, &Z80.ix );
}

void PUSH_pc() {
	SP -= 2;
	WM16( SPD, &Z80.pc );
}

/***************************************************************
 * JP
 ***************************************************************/
void JP() {
  PCD = ARG16();
  WZ = PCD;
}

/***************************************************************
 * JP_COND
 ***************************************************************/
void JP_COND(bool cond) {
  if (cond) {
    PCD = ARG16();
    WZ = PCD;
  } else {
    WZ = ARG16(); /* implicit do PC += 2 */
  }
}

/***************************************************************
 * JR
 ***************************************************************/
void JR() {
  s8 arg = (s8)ARG(); /* ARG() also increments PC */
  PC += arg;        /* so don't do PC += ARG() */
  WZ = PC;
}

/***************************************************************
 * JR_COND
 ***************************************************************/
void JR_COND(bool cond, u8 opcode) {
  if (cond)
  {
    JR();
    CC_ex(opcode);
  }
  else PC++;
}

/***************************************************************
 * CALL
 ***************************************************************/
void CALL() {
  EA = ARG16();
  WZ = EA;
  PUSH_pc();
  PCD = EA;
}

/***************************************************************
 * CALL_COND
 ***************************************************************/
void CALL_COND(bool cond, u8 opcode) {
  if (cond)
  {
    EA = ARG16();
    WZ = EA;
    PUSH_pc();
    PCD = EA;
    CC_ex(opcode);
  }
  else
  {
    WZ = ARG16();  /* implicit call PC+=2;   */
  }
}

/***************************************************************
 * RET_COND
 ***************************************************************/
void RET_COND(bool cond, u8 opcode) {
  if (cond)
  {
    POP_pc();
    WZ = PC;
    CC_ex(opcode);
  }
}

/***************************************************************
 * RETN
 ***************************************************************/
void RETN() {
	LOG(("Z80 #%d RETN IFF1:%d IFF2:%d\n", cpu_getactivecpu(), IFF1, IFF2));
	POP_pc();
	WZ = PC;
	IFF1 = IFF2;
}

/***************************************************************
 * RETI
 ***************************************************************/
void RETI() {
  POP_pc();
  WZ = PC;
/* according to http://www.msxnet.org/tech/z80-documented.pdf */
  IFF1 = IFF2;
}

/***************************************************************
 * LD  R,A
 ***************************************************************/
void LD_R_A() {
  R = A;
  R2 = A & 0x80;  /* keep bit 7 of R */
}

/***************************************************************
 * LD  A,R
 ***************************************************************/
void LD_A_R() {
  A = (R & 0x7f) | R2;
  F = (F & CF) | SZ[A] | ( IFF2 << 2 );
}

/***************************************************************
 * LD  I,A
 ***************************************************************/
void LD_I_A() {
  I = A;
}

/***************************************************************
 * LD  A,I
 ***************************************************************/
void LD_A_I() {
  A = I;
  F = (F & CF) | SZ[A] | ( IFF2 << 2 );
}

/***************************************************************
 * RST
 ***************************************************************/
void RST(u8 addr) {
  PUSH_pc();
  PCD = addr;
  WZ = PC;
}

/***************************************************************
 * INC  r8
 ***************************************************************/
u8 INC(u8 value)
{
  u8 res = value + 1;
  F = (F & CF) | SZHV_inc[res];
  return (u8)res;
}

/***************************************************************
 * DEC  r8
 ***************************************************************/
u8 DEC(u8 value)
{
  u8 res = value - 1;
  F = (F & CF) | SZHV_dec[res];
  return res;
}

/***************************************************************
 * RLCA
 ***************************************************************/
void RLCA() {
  A = (A << 1) | (A >> 7);
  F = (F & (SF | ZF | PF)) | (A & (YF | XF | CF));
}

/***************************************************************
 * RRCA
 ***************************************************************/
void RRCA() {
  F = (F & (SF | ZF | PF)) | (A & CF);
  A = (A >> 1) | (A << 7);
  F |= (A & (YF | XF) );
}

/***************************************************************
 * RLA
 ***************************************************************/
void RLA() {
  u8 res = (A << 1) | (F & CF);
  u8 c = (A & 0x80) ? CF : 0;
  F = (F & (SF | ZF | PF)) | c | (res & (YF | XF));
  A = res;
}

/***************************************************************
 * RRA
 ***************************************************************/
void RRA() {
  u8 res = (A >> 1) | (F << 7);
  u8 c = (A & 0x01) ? CF : 0;
  F = (F & (SF | ZF | PF)) | c | (res & (YF | XF));
  A = res;
}

/***************************************************************
 * RRD
 ***************************************************************/
void RRD() {
  u8 n = RM(HL);
  WZ = HL+1;
  WM( HL, (n >> 4) | (A << 4) );
  A = (A & 0xf0) | (n & 0x0f);
  F = (F & CF) | SZP[A];
}

/***************************************************************
 * RLD
 ***************************************************************/
void RLD() {
  u8 n = RM(HL);
  WZ = HL+1;
  WM( HL, (n << 4) | (A & 0x0f) );
  A = (A & 0xf0) | (n >> 4);
  F = (F & CF) | SZP[A];
}

/***************************************************************
 * ADD  A,n
 ***************************************************************/
void ADD(u8 value) {
  u32 ah = AFD & 0xff00;
  u32 res = (u8)((ah >> 8) + value);
  F = SZHVC_add[ah | res];
  A = res;
}

/***************************************************************
 * ADC  A,n
 ***************************************************************/
void ADC(u8 value) {
  u32 ah = AFD & 0xff00, c = AFD & 1;
  u32 res = (u8)((ah >> 8) + value + c);
  F = SZHVC_add[(c << 16) | ah | res];
  A = res;
}

/***************************************************************
 * SUB  n
 ***************************************************************/
void SUB(u8 value) {
  u32 ah = AFD & 0xff00;
  u32 res = (u8)((ah >> 8) - value);
  F = SZHVC_sub[ah | res];
  A = res;
}

/***************************************************************
 * SBC  A,n
 ***************************************************************/
void SBC(u8 value) {
  u32 ah = AFD & 0xff00, c = AFD & 1;
  u32 res = (u8)((ah >> 8) - value - c);
  F = SZHVC_sub[(c<<16) | ah | res];
  A = res;
}

/***************************************************************
 * NEG
 ***************************************************************/
void NEG() {
  u8 value = A;
  A = 0;
  SUB(value);
}

/***************************************************************
 * DAA
 ***************************************************************/
void DAA() {
  u8 a = A;
  if (F & NF) {
    if ((F&HF) | ((A&0xf)>9)) a-=6;
    if ((F&CF) | (A>0x99)) a-=0x60;
  } else {
    if ((F&HF) | ((A&0xf)>9)) a+=6;
    if ((F&CF) | (A>0x99)) a+=0x60;
  }

  F = (F&(CF|NF)) | (A>0x99) | ((A^a)&HF) | SZP[a];
  A = a;
}

/***************************************************************
 * AND  n
 ***************************************************************/
void AND(u8 value) {
  A &= value;
  F = SZP[A] | HF;
}

/***************************************************************
 * OR  n
 ***************************************************************/
void OR(u8 value) {
  A |= value;
  F = SZP[A];
}

/***************************************************************
 * XOR  n
 ***************************************************************/
void XOR(u8 value) {
  A ^= value;
  F = SZP[A];
}

/***************************************************************
 * CP  n
 ***************************************************************/
void CP(u8 value) {
  u32 val = value;
  u32 ah = AFD & 0xff00;
  u32 res = (u8)((ah >> 8) - val);
  F = (SZHVC_sub[ah | res] & ~(YF | XF)) | (val & (YF | XF));
}

/***************************************************************
 * EX  AF,AF'
 ***************************************************************/
void EX_AF() {
  PAIR tmp;
  tmp = Z80.af; Z80.af = Z80.af2; Z80.af2 = tmp;
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
void EXSP_ix()
{
  PAIR tmp = { { 0, 0, 0, 0 } };
  RM16( SPD, &tmp );
  WM16( SPD, &Z80.ix );
  Z80.ix = tmp;
  WZ = Z80.ix.d;
}

void EXSP_iy()
{
  PAIR tmp = { { 0, 0, 0, 0 } };
  RM16( SPD, &tmp );
  WM16( SPD, &Z80.iy );
  Z80.iy = tmp;
  WZ = Z80.iy.d;
}

void EXSP_hl()
{
  PAIR tmp = { { 0, 0, 0, 0 } };
  RM16( SPD, &tmp );
  WM16( SPD, &Z80.hl );
  Z80.hl = tmp;
  WZ = Z80.hl.d;
}

/***************************************************************
 * ADD16
 ***************************************************************/
#define ADD16(DR,SR)                                \
{                                                   \
  u32 res = Z80.DR.d + Z80.SR.d;                 \
  WZ = Z80.DR.d + 1;                                \
  F = (F & (SF | ZF | VF)) |                        \
    (((Z80.DR.d ^ res ^ Z80.SR.d) >> 8) & HF) |     \
    ((res >> 16) & CF) | ((res >> 8) & (YF | XF));  \
  Z80.DR.w.l = (u16)res;                         \
}

/***************************************************************
 * ADC  r16,r16
 ***************************************************************/
#define ADC16(Reg)                                                      \
{                                                                       \
  u32 res = HLD + Z80.Reg.d + (F & CF);                              \
  WZ = HL + 1;                                                          \
  F = (((HLD ^ res ^ Z80.Reg.d) >> 8) & HF) |                           \
    ((res >> 16) & CF) |                                                \
    ((res >> 8) & (SF | YF | XF)) |                                     \
    ((res & 0xffff) ? 0 : ZF) |                                         \
    (((Z80.Reg.d ^ HLD ^ 0x8000) & (Z80.Reg.d ^ res) & 0x8000) >> 13);  \
  HL = (u16)res;                                                     \
}

/***************************************************************
 * SBC  r16,r16
 ***************************************************************/
#define SBC16(Reg)                                      \
{                                                       \
  u32 res = HLD - Z80.Reg.d - (F & CF);              \
  WZ = HL + 1;                                          \
  F = (((HLD ^ res ^ Z80.Reg.d) >> 8) & HF) | NF |      \
    ((res >> 16) & CF) |                                \
    ((res >> 8) & (SF | YF | XF)) |                     \
    ((res & 0xffff) ? 0 : ZF) |                         \
    (((Z80.Reg.d ^ HLD) & (HLD ^ res) &0x8000) >> 13);  \
  HL = (u16)res;                                     \
}

/***************************************************************
 * RLC  r8
 ***************************************************************/
u8 RLC(u8 value)
{
  u32 res = value;
  u32 c = (res & 0x80) ? CF : 0;
  res = ((res << 1) | (res >> 7)) & 0xff;
  F = SZP[res] | c;
  return res;
}

/***************************************************************
 * RRC  r8
 ***************************************************************/
u8 RRC(u8 value)
{
  u32 res = value;
  u32 c = (res & 0x01) ? CF : 0;
  res = ((res >> 1) | (res << 7)) & 0xff;
  F = SZP[res] | c;
  return res;
}

/***************************************************************
 * RL  r8
 ***************************************************************/
u8 RL(u8 value)
{
  u32 res = value;
  u32 c = (res & 0x80) ? CF : 0;
  res = ((res << 1) | (F & CF)) & 0xff;
  F = SZP[res] | c;
  return res;
}

/***************************************************************
 * RR  r8
 ***************************************************************/
u8 RR(u8 value)
{
  u32 res = value;
  u32 c = (res & 0x01) ? CF : 0;
  res = ((res >> 1) | (F << 7)) & 0xff;
  F = SZP[res] | c;
  return res;
}

/***************************************************************
 * SLA  r8
 ***************************************************************/
u8 SLA(u8 value)
{
  u32 res = value;
  u32 c = (res & 0x80) ? CF : 0;
  res = (res << 1) & 0xff;
  F = SZP[res] | c;
  return res;
}

/***************************************************************
 * SRA  r8
 ***************************************************************/
u8 SRA(u8 value)
{
  u32 res = value;
  u32 c = (res & 0x01) ? CF : 0;
  res = ((res >> 1) | (res & 0x80)) & 0xff;
  F = SZP[res] | c;
  return res;
}

/***************************************************************
 * SLL  r8
 ***************************************************************/
u8 SLL(u8 value)
{
  u32 res = value;
  u32 c = (res & 0x80) ? CF : 0;
  res = ((res << 1) | 0x01) & 0xff;
  F = SZP[res] | c;
  return res;
}

/***************************************************************
 * SRL  r8
 ***************************************************************/
u8 SRL(u8 value)
{
  u32 res = value;
  u32 c = (res & 0x01) ? CF : 0;
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
u8 RES(u8 bit, u8 value)
{
  return value & ~(1<<bit);
}

/***************************************************************
 * SET  bit,r8
 ***************************************************************/
u8 SET(u8 bit, u8 value)
{
  return value | (1<<bit);
}

/***************************************************************
 * LDI
 ***************************************************************/
void LDI() {
  u8 io = RM(HL);
  WM( DE, io );
  F &= SF | ZF | CF;
  if( (A + io) & 0x02 ) F |= YF; /* bit 1 -> flag 5 */
  if( (A + io) & 0x08 ) F |= XF; /* bit 3 -> flag 3 */
  HL++; DE++; BC--;
  if( BC ) F |= VF;
}

/***************************************************************
 * CPI
 ***************************************************************/
void CPI() {
  u8 val = RM(HL);
  u8 res = A - val;
  WZ++;
  HL++; BC--;
  F = (F & CF) | (SZ[res]&~(YF|XF)) | ((A^val^res)&HF) | NF;
  if( F & HF ) res -= 1;
  if( res & 0x02 ) F |= YF; /* bit 1 -> flag 5 */
  if( res & 0x08 ) F |= XF; /* bit 3 -> flag 3 */
  if( BC ) F |= VF;
}

/***************************************************************
 * INI
 ***************************************************************/
void INI() {
  u32 t;
  u8 io = IN(BC);
  WZ = BC + 1;
  CC_ex(0xa2);
  B--;
  WM( HL, io );
  HL++;
  F = SZ[B];
  t = (u32)((C + 1) & 0xff) + (u32)io;
  if( io & SF ) F |= NF;
  if( t & 0x100 ) F |= HF | CF;
  F |= SZP[(u8)(t & 0x07) ^ B] & PF;
}

/***************************************************************
 * OUTI
 ***************************************************************/
void OUTI() {
  u32 t;
  u8 io = RM(HL);
  B--;
  WZ = BC + 1;
  OUT( BC, io );
  HL++;
  F = SZ[B];
  t = (u32)L + (u32)io;
  if( io & SF ) F |= NF;
  if( t & 0x100 ) F |= HF | CF;
  F |= SZP[(u8)(t & 0x07) ^ B] & PF;
}

/***************************************************************
 * LDD
 ***************************************************************/
void LDD() {
  u8 io = RM(HL);
  WM( DE, io );
  F &= SF | ZF | CF;
  if( (A + io) & 0x02 ) F |= YF; /* bit 1 -> flag 5 */
  if( (A + io) & 0x08 ) F |= XF; /* bit 3 -> flag 3 */
  HL--; DE--; BC--;
  if( BC ) F |= VF;
}

/***************************************************************
 * CPD
 ***************************************************************/
void CPD() {
  u8 val = RM(HL);
  u8 res = A - val;
  WZ--;
  HL--; BC--;
  F = (F & CF) | (SZ[res]&~(YF|XF)) | ((A^val^res)&HF) | NF;
  if( F & HF ) res -= 1;
  if( res & 0x02 ) F |= YF; /* bit 1 -> flag 5 */
  if( res & 0x08 ) F |= XF; /* bit 3 -> flag 3 */
  if( BC ) F |= VF;
}

/***************************************************************
 * IND
 ***************************************************************/
void IND() {
  u32 t;
  u8 io = IN(BC);
  WZ = BC - 1;
  CC_ex(0xaa);
  B--;
  WM( HL, io );
  HL--;
  F = SZ[B];
  t = ((u32)(C - 1) & 0xff) + (u32)io;
  if( io & SF ) F |= NF;
  if( t & 0x100 ) F |= HF | CF;
  F |= SZP[(u8)(t & 0x07) ^ B] & PF;
}

/***************************************************************
 * OUTD
 ***************************************************************/
void OUTD() {
  u32 t;
  u8 io = RM(HL);
  B--;
  WZ = BC - 1;
  OUT( BC, io );
  HL--;
  F = SZ[B];
  t = (u32)L + (u32)io;
  if( io & SF ) F |= NF;
  if( t & 0x100 ) F |= HF | CF;
  F |= SZP[(u8)(t & 0x07) ^ B] & PF;
}

/***************************************************************
 * LDIR
 ***************************************************************/
void LDIR() {
  LDI();
  if( BC )
  {
    PC -= 2;
    WZ = PC + 1;
    CC_ex(0xb0);
  }
}

/***************************************************************
 * CPIR
 ***************************************************************/
void CPIR() {
  CPI();
  if( BC && !(F & ZF) )
  {
    PC -= 2;
   WZ = PC + 1;
    CC_ex(0xb1);
  }
}

/***************************************************************
 * INIR
 ***************************************************************/
void INIR() {
  INI();
  if( B )
  {
    PC -= 2;
    CC_ex(0xb2);
  }
}

/***************************************************************
 * OTIR
 ***************************************************************/
void OTIR() {
  OUTI();
  if( B )
  {
    PC -= 2;
    CC_ex(0xb3);
  }
}

/***************************************************************
 * LDDR
 ***************************************************************/
void LDDR() {
  LDD();
  if( BC )
  {
    PC -= 2;
    WZ = PC + 1;
    CC_ex(0xb8);
  }
}

/***************************************************************
 * CPDR
 ***************************************************************/
void CPDR() {
  CPD();
  if( BC && !(F & ZF) )
  {
    PC -= 2;
   WZ = PC + 1;
    CC_ex(0xb9);
  }
}

/***************************************************************
 * INDR
 ***************************************************************/
void INDR() {
  IND();
  if( B )
  {
    PC -= 2;
    CC_ex(0xba);
  }
}

/***************************************************************
 * OTDR
 ***************************************************************/
void OTDR() {
  OUTD();
  if( B )
  {
    PC -= 2;
    CC_ex(0xbb);
  }
}

/***************************************************************
 * EI
 ***************************************************************/
void EI() {
  IFF1 = IFF2 = 1;
  Z80.after_ei = true;
}

/**********************************************************
 * opcodes with CB prefix
 * rotate, shift and bit operations
 **********************************************************/
void cb_00() { B = RLC(B);                      } /* RLC  B           */
void cb_01() { C = RLC(C);                      } /* RLC  C           */
void cb_02() { D = RLC(D);                      } /* RLC  D           */
void cb_03() { E = RLC(E);                      } /* RLC  E           */
void cb_04() { H = RLC(H);                      } /* RLC  H           */
void cb_05() { L = RLC(L);                      } /* RLC  L           */
void cb_06() { WM( HL, RLC(RM(HL)) );           } /* RLC  (HL)        */
void cb_07() { A = RLC(A);                      } /* RLC  A           */

void cb_08() { B = RRC(B);                      } /* RRC  B           */
void cb_09() { C = RRC(C);                      } /* RRC  C           */
void cb_0a() { D = RRC(D);                      } /* RRC  D           */
void cb_0b() { E = RRC(E);                      } /* RRC  E           */
void cb_0c() { H = RRC(H);                      } /* RRC  H           */
void cb_0d() { L = RRC(L);                      } /* RRC  L           */
void cb_0e() { WM( HL, RRC(RM(HL)) );           } /* RRC  (HL)        */
void cb_0f() { A = RRC(A);                      } /* RRC  A           */

void cb_10() { B = RL(B);                       } /* RL   B           */
void cb_11() { C = RL(C);                       } /* RL   C           */
void cb_12() { D = RL(D);                       } /* RL   D           */
void cb_13() { E = RL(E);                       } /* RL   E           */
void cb_14() { H = RL(H);                       } /* RL   H           */
void cb_15() { L = RL(L);                       } /* RL   L           */
void cb_16() { WM( HL, RL(RM(HL)) );            } /* RL   (HL)        */
void cb_17() { A = RL(A);                       } /* RL   A           */

void cb_18() { B = RR(B);                       } /* RR   B           */
void cb_19() { C = RR(C);                       } /* RR   C           */
void cb_1a() { D = RR(D);                       } /* RR   D           */
void cb_1b() { E = RR(E);                       } /* RR   E           */
void cb_1c() { H = RR(H);                       } /* RR   H           */
void cb_1d() { L = RR(L);                       } /* RR   L           */
void cb_1e() { WM( HL, RR(RM(HL)) );            } /* RR   (HL)        */
void cb_1f() { A = RR(A);                       } /* RR   A           */

void cb_20() { B = SLA(B);                      } /* SLA  B           */
void cb_21() { C = SLA(C);                      } /* SLA  C           */
void cb_22() { D = SLA(D);                      } /* SLA  D           */
void cb_23() { E = SLA(E);                      } /* SLA  E           */
void cb_24() { H = SLA(H);                      } /* SLA  H           */
void cb_25() { L = SLA(L);                      } /* SLA  L           */
void cb_26() { WM( HL, SLA(RM(HL)) );           } /* SLA  (HL)        */
void cb_27() { A = SLA(A);                      } /* SLA  A           */

void cb_28() { B = SRA(B);                      } /* SRA  B           */
void cb_29() { C = SRA(C);                      } /* SRA  C           */
void cb_2a() { D = SRA(D);                      } /* SRA  D           */
void cb_2b() { E = SRA(E);                      } /* SRA  E           */
void cb_2c() { H = SRA(H);                      } /* SRA  H           */
void cb_2d() { L = SRA(L);                      } /* SRA  L           */
void cb_2e() { WM( HL, SRA(RM(HL)) );           } /* SRA  (HL)        */
void cb_2f() { A = SRA(A);                      } /* SRA  A           */

void cb_30() { B = SLL(B);                      } /* SLL  B           */
void cb_31() { C = SLL(C);                      } /* SLL  C           */
void cb_32() { D = SLL(D);                      } /* SLL  D           */
void cb_33() { E = SLL(E);                      } /* SLL  E           */
void cb_34() { H = SLL(H);                      } /* SLL  H           */
void cb_35() { L = SLL(L);                      } /* SLL  L           */
void cb_36() { WM( HL, SLL(RM(HL)) );           } /* SLL  (HL)        */
void cb_37() { A = SLL(A);                      } /* SLL  A           */

void cb_38() { B = SRL(B);                      } /* SRL  B           */
void cb_39() { C = SRL(C);                      } /* SRL  C           */
void cb_3a() { D = SRL(D);                      } /* SRL  D           */
void cb_3b() { E = SRL(E);                      } /* SRL  E           */
void cb_3c() { H = SRL(H);                      } /* SRL  H           */
void cb_3d() { L = SRL(L);                      } /* SRL  L           */
void cb_3e() { WM( HL, SRL(RM(HL)) );           } /* SRL  (HL)        */
void cb_3f() { A = SRL(A);                      } /* SRL  A           */

void cb_40() { BIT(0,B);                        } /* BIT  0,B         */
void cb_41() { BIT(0,C);                        } /* BIT  0,C         */
void cb_42() { BIT(0,D);                        } /* BIT  0,D         */
void cb_43() { BIT(0,E);                        } /* BIT  0,E         */
void cb_44() { BIT(0,H);                        } /* BIT  0,H         */
void cb_45() { BIT(0,L);                        } /* BIT  0,L         */
void cb_46() { BIT_HL(0,RM(HL));                } /* BIT  0,(HL)      */
void cb_47() { BIT(0,A);                        } /* BIT  0,A         */

void cb_48() { BIT(1,B);                        } /* BIT  1,B         */
void cb_49() { BIT(1,C);                        } /* BIT  1,C         */
void cb_4a() { BIT(1,D);                        } /* BIT  1,D         */
void cb_4b() { BIT(1,E);                        } /* BIT  1,E         */
void cb_4c() { BIT(1,H);                        } /* BIT  1,H         */
void cb_4d() { BIT(1,L);                        } /* BIT  1,L         */
void cb_4e() { BIT_HL(1,RM(HL));                } /* BIT  1,(HL)      */
void cb_4f() { BIT(1,A);                        } /* BIT  1,A         */

void cb_50() { BIT(2,B);                        } /* BIT  2,B         */
void cb_51() { BIT(2,C);                        } /* BIT  2,C         */
void cb_52() { BIT(2,D);                        } /* BIT  2,D         */
void cb_53() { BIT(2,E);                        } /* BIT  2,E         */
void cb_54() { BIT(2,H);                        } /* BIT  2,H         */
void cb_55() { BIT(2,L);                        } /* BIT  2,L         */
void cb_56() { BIT_HL(2,RM(HL));                } /* BIT  2,(HL)      */
void cb_57() { BIT(2,A);                        } /* BIT  2,A         */

void cb_58() { BIT(3,B);                        } /* BIT  3,B         */
void cb_59() { BIT(3,C);                        } /* BIT  3,C         */
void cb_5a() { BIT(3,D);                        } /* BIT  3,D         */
void cb_5b() { BIT(3,E);                        } /* BIT  3,E         */
void cb_5c() { BIT(3,H);                        } /* BIT  3,H         */
void cb_5d() { BIT(3,L);                        } /* BIT  3,L         */
void cb_5e() { BIT_HL(3,RM(HL));                } /* BIT  3,(HL)      */
void cb_5f() { BIT(3,A);                        } /* BIT  3,A         */

void cb_60() { BIT(4,B);                        } /* BIT  4,B         */
void cb_61() { BIT(4,C);                        } /* BIT  4,C         */
void cb_62() { BIT(4,D);                        } /* BIT  4,D         */
void cb_63() { BIT(4,E);                        } /* BIT  4,E         */
void cb_64() { BIT(4,H);                        } /* BIT  4,H         */
void cb_65() { BIT(4,L);                        } /* BIT  4,L         */
void cb_66() { BIT_HL(4,RM(HL));                } /* BIT  4,(HL)      */
void cb_67() { BIT(4,A);                        } /* BIT  4,A         */

void cb_68() { BIT(5,B);                        } /* BIT  5,B         */
void cb_69() { BIT(5,C);                        } /* BIT  5,C         */
void cb_6a() { BIT(5,D);                        } /* BIT  5,D         */
void cb_6b() { BIT(5,E);                        } /* BIT  5,E         */
void cb_6c() { BIT(5,H);                        } /* BIT  5,H         */
void cb_6d() { BIT(5,L);                        } /* BIT  5,L         */
void cb_6e() { BIT_HL(5,RM(HL));                } /* BIT  5,(HL)      */
void cb_6f() { BIT(5,A);                        } /* BIT  5,A         */

void cb_70() { BIT(6,B);                        } /* BIT  6,B         */
void cb_71() { BIT(6,C);                        } /* BIT  6,C         */
void cb_72() { BIT(6,D);                        } /* BIT  6,D         */
void cb_73() { BIT(6,E);                        } /* BIT  6,E         */
void cb_74() { BIT(6,H);                        } /* BIT  6,H         */
void cb_75() { BIT(6,L);                        } /* BIT  6,L         */
void cb_76() { BIT_HL(6,RM(HL));                } /* BIT  6,(HL)      */
void cb_77() { BIT(6,A);                        } /* BIT  6,A         */

void cb_78() { BIT(7,B);                        } /* BIT  7,B         */
void cb_79() { BIT(7,C);                        } /* BIT  7,C         */
void cb_7a() { BIT(7,D);                        } /* BIT  7,D         */
void cb_7b() { BIT(7,E);                        } /* BIT  7,E         */
void cb_7c() { BIT(7,H);                        } /* BIT  7,H         */
void cb_7d() { BIT(7,L);                        } /* BIT  7,L         */
void cb_7e() { BIT_HL(7,RM(HL));                } /* BIT  7,(HL)      */
void cb_7f() { BIT(7,A);                        } /* BIT  7,A         */

void cb_80() { B = RES(0,B);                    } /* RES  0,B         */
void cb_81() { C = RES(0,C);                    } /* RES  0,C         */
void cb_82() { D = RES(0,D);                    } /* RES  0,D         */
void cb_83() { E = RES(0,E);                    } /* RES  0,E         */
void cb_84() { H = RES(0,H);                    } /* RES  0,H         */
void cb_85() { L = RES(0,L);                    } /* RES  0,L         */
void cb_86() { WM( HL, RES(0,RM(HL)) );         } /* RES  0,(HL)      */
void cb_87() { A = RES(0,A);                    } /* RES  0,A         */

void cb_88() { B = RES(1,B);                    } /* RES  1,B         */
void cb_89() { C = RES(1,C);                    } /* RES  1,C         */
void cb_8a() { D = RES(1,D);                    } /* RES  1,D         */
void cb_8b() { E = RES(1,E);                    } /* RES  1,E         */
void cb_8c() { H = RES(1,H);                    } /* RES  1,H         */
void cb_8d() { L = RES(1,L);                    } /* RES  1,L         */
void cb_8e() { WM( HL, RES(1,RM(HL)) );         } /* RES  1,(HL)      */
void cb_8f() { A = RES(1,A);                    } /* RES  1,A         */

void cb_90() { B = RES(2,B);                    } /* RES  2,B         */
void cb_91() { C = RES(2,C);                    } /* RES  2,C         */
void cb_92() { D = RES(2,D);                    } /* RES  2,D         */
void cb_93() { E = RES(2,E);                    } /* RES  2,E         */
void cb_94() { H = RES(2,H);                    } /* RES  2,H         */
void cb_95() { L = RES(2,L);                    } /* RES  2,L         */
void cb_96() { WM( HL, RES(2,RM(HL)) );         } /* RES  2,(HL)      */
void cb_97() { A = RES(2,A);                    } /* RES  2,A         */

void cb_98() { B = RES(3,B);                    } /* RES  3,B         */
void cb_99() { C = RES(3,C);                    } /* RES  3,C         */
void cb_9a() { D = RES(3,D);                    } /* RES  3,D         */
void cb_9b() { E = RES(3,E);                    } /* RES  3,E         */
void cb_9c() { H = RES(3,H);                    } /* RES  3,H         */
void cb_9d() { L = RES(3,L);                    } /* RES  3,L         */
void cb_9e() { WM( HL, RES(3,RM(HL)) );         } /* RES  3,(HL)      */
void cb_9f() { A = RES(3,A);                    } /* RES  3,A         */

void cb_a0() { B = RES(4,B);                    } /* RES  4,B         */
void cb_a1() { C = RES(4,C);                    } /* RES  4,C         */
void cb_a2() { D = RES(4,D);                    } /* RES  4,D         */
void cb_a3() { E = RES(4,E);                    } /* RES  4,E         */
void cb_a4() { H = RES(4,H);                    } /* RES  4,H         */
void cb_a5() { L = RES(4,L);                    } /* RES  4,L         */
void cb_a6() { WM( HL, RES(4,RM(HL)) );         } /* RES  4,(HL)      */
void cb_a7() { A = RES(4,A);                    } /* RES  4,A         */

void cb_a8() { B = RES(5,B);                    } /* RES  5,B         */
void cb_a9() { C = RES(5,C);                    } /* RES  5,C         */
void cb_aa() { D = RES(5,D);                    } /* RES  5,D         */
void cb_ab() { E = RES(5,E);                    } /* RES  5,E         */
void cb_ac() { H = RES(5,H);                    } /* RES  5,H         */
void cb_ad() { L = RES(5,L);                    } /* RES  5,L         */
void cb_ae() { WM( HL, RES(5,RM(HL)) );         } /* RES  5,(HL)      */
void cb_af() { A = RES(5,A);                    } /* RES  5,A         */

void cb_b0() { B = RES(6,B);                    } /* RES  6,B         */
void cb_b1() { C = RES(6,C);                    } /* RES  6,C         */
void cb_b2() { D = RES(6,D);                    } /* RES  6,D         */
void cb_b3() { E = RES(6,E);                    } /* RES  6,E         */
void cb_b4() { H = RES(6,H);                    } /* RES  6,H         */
void cb_b5() { L = RES(6,L);                    } /* RES  6,L         */
void cb_b6() { WM( HL, RES(6,RM(HL)) );         } /* RES  6,(HL)      */
void cb_b7() { A = RES(6,A);                    } /* RES  6,A         */

void cb_b8() { B = RES(7,B);                    } /* RES  7,B         */
void cb_b9() { C = RES(7,C);                    } /* RES  7,C         */
void cb_ba() { D = RES(7,D);                    } /* RES  7,D         */
void cb_bb() { E = RES(7,E);                    } /* RES  7,E         */
void cb_bc() { H = RES(7,H);                    } /* RES  7,H         */
void cb_bd() { L = RES(7,L);                    } /* RES  7,L         */
void cb_be() { WM( HL, RES(7,RM(HL)) );         } /* RES  7,(HL)      */
void cb_bf() { A = RES(7,A);                    } /* RES  7,A         */

void cb_c0() { B = SET(0,B);                    } /* SET  0,B         */
void cb_c1() { C = SET(0,C);                    } /* SET  0,C         */
void cb_c2() { D = SET(0,D);                    } /* SET  0,D         */
void cb_c3() { E = SET(0,E);                    } /* SET  0,E         */
void cb_c4() { H = SET(0,H);                    } /* SET  0,H         */
void cb_c5() { L = SET(0,L);                    } /* SET  0,L         */
void cb_c6() { WM( HL, SET(0,RM(HL)) );         } /* SET  0,(HL)      */
void cb_c7() { A = SET(0,A);                    } /* SET  0,A         */

void cb_c8() { B = SET(1,B);                    } /* SET  1,B         */
void cb_c9() { C = SET(1,C);                    } /* SET  1,C         */
void cb_ca() { D = SET(1,D);                    } /* SET  1,D         */
void cb_cb() { E = SET(1,E);                    } /* SET  1,E         */
void cb_cc() { H = SET(1,H);                    } /* SET  1,H         */
void cb_cd() { L = SET(1,L);                    } /* SET  1,L         */
void cb_ce() { WM( HL, SET(1,RM(HL)) );         } /* SET  1,(HL)      */
void cb_cf() { A = SET(1,A);                    } /* SET  1,A         */

void cb_d0() { B = SET(2,B);                    } /* SET  2,B         */
void cb_d1() { C = SET(2,C);                    } /* SET  2,C         */
void cb_d2() { D = SET(2,D);                    } /* SET  2,D         */
void cb_d3() { E = SET(2,E);                    } /* SET  2,E         */
void cb_d4() { H = SET(2,H);                    } /* SET  2,H         */
void cb_d5() { L = SET(2,L);                    } /* SET  2,L         */
void cb_d6() { WM( HL, SET(2,RM(HL)) );         } /* SET  2,(HL)      */
void cb_d7() { A = SET(2,A);                    } /* SET  2,A         */

void cb_d8() { B = SET(3,B);                    } /* SET  3,B         */
void cb_d9() { C = SET(3,C);                    } /* SET  3,C         */
void cb_da() { D = SET(3,D);                    } /* SET  3,D         */
void cb_db() { E = SET(3,E);                    } /* SET  3,E         */
void cb_dc() { H = SET(3,H);                    } /* SET  3,H         */
void cb_dd() { L = SET(3,L);                    } /* SET  3,L         */
void cb_de() { WM( HL, SET(3,RM(HL)) );         } /* SET  3,(HL)      */
void cb_df() { A = SET(3,A);                    } /* SET  3,A         */

void cb_e0() { B = SET(4,B);                    } /* SET  4,B         */
void cb_e1() { C = SET(4,C);                    } /* SET  4,C         */
void cb_e2() { D = SET(4,D);                    } /* SET  4,D         */
void cb_e3() { E = SET(4,E);                    } /* SET  4,E         */
void cb_e4() { H = SET(4,H);                    } /* SET  4,H         */
void cb_e5() { L = SET(4,L);                    } /* SET  4,L         */
void cb_e6() { WM( HL, SET(4,RM(HL)) );         } /* SET  4,(HL)      */
void cb_e7() { A = SET(4,A);                    } /* SET  4,A         */

void cb_e8() { B = SET(5,B);                    } /* SET  5,B         */
void cb_e9() { C = SET(5,C);                    } /* SET  5,C         */
void cb_ea() { D = SET(5,D);                    } /* SET  5,D         */
void cb_eb() { E = SET(5,E);                    } /* SET  5,E         */
void cb_ec() { H = SET(5,H);                    } /* SET  5,H         */
void cb_ed() { L = SET(5,L);                    } /* SET  5,L         */
void cb_ee() { WM( HL, SET(5,RM(HL)) );         } /* SET  5,(HL)      */
void cb_ef() { A = SET(5,A);                    } /* SET  5,A         */

void cb_f0() { B = SET(6,B);                    } /* SET  6,B         */
void cb_f1() { C = SET(6,C);                    } /* SET  6,C         */
void cb_f2() { D = SET(6,D);                    } /* SET  6,D         */
void cb_f3() { E = SET(6,E);                    } /* SET  6,E         */
void cb_f4() { H = SET(6,H);                    } /* SET  6,H         */
void cb_f5() { L = SET(6,L);                    } /* SET  6,L         */
void cb_f6() { WM( HL, SET(6,RM(HL)) );         } /* SET  6,(HL)      */
void cb_f7() { A = SET(6,A);                    } /* SET  6,A         */

void cb_f8() { B = SET(7,B);                    } /* SET  7,B         */
void cb_f9() { C = SET(7,C);                    } /* SET  7,C         */
void cb_fa() { D = SET(7,D);                    } /* SET  7,D         */
void cb_fb() { E = SET(7,E);                    } /* SET  7,E         */
void cb_fc() { H = SET(7,H);                    } /* SET  7,H         */
void cb_fd() { L = SET(7,L);                    } /* SET  7,L         */
void cb_fe() { WM( HL, SET(7,RM(HL)) );         } /* SET  7,(HL)      */
void cb_ff() { A = SET(7,A);                    } /* SET  7,A         */


/**********************************************************
* opcodes with DD/FD CB prefix
* rotate, shift and bit operations with (IX+o)
**********************************************************/
void xycb_00() { B = RLC( RM(EA) ); WM( EA,B );            } /* RLC  B=(XY+o)    */
void xycb_01() { C = RLC( RM(EA) ); WM( EA,C );            } /* RLC  C=(XY+o)    */
void xycb_02() { D = RLC( RM(EA) ); WM( EA,D );            } /* RLC  D=(XY+o)    */
void xycb_03() { E = RLC( RM(EA) ); WM( EA,E );            } /* RLC  E=(XY+o)    */
void xycb_04() { H = RLC( RM(EA) ); WM( EA,H );            } /* RLC  H=(XY+o)    */
void xycb_05() { L = RLC( RM(EA) ); WM( EA,L );            } /* RLC  L=(XY+o)    */
void xycb_06() { WM( EA, RLC( RM(EA) ) );                  } /* RLC  (XY+o)      */
void xycb_07() { A = RLC( RM(EA) ); WM( EA,A );            } /* RLC  A=(XY+o)    */

void xycb_08() { B = RRC( RM(EA) ); WM( EA,B );            } /* RRC  B=(XY+o)    */
void xycb_09() { C = RRC( RM(EA) ); WM( EA,C );            } /* RRC  C=(XY+o)    */
void xycb_0a() { D = RRC( RM(EA) ); WM( EA,D );            } /* RRC  D=(XY+o)    */
void xycb_0b() { E = RRC( RM(EA) ); WM( EA,E );            } /* RRC  E=(XY+o)    */
void xycb_0c() { H = RRC( RM(EA) ); WM( EA,H );            } /* RRC  H=(XY+o)    */
void xycb_0d() { L = RRC( RM(EA) ); WM( EA,L );            } /* RRC  L=(XY+o)    */
void xycb_0e() { WM( EA,RRC( RM(EA) ) );                   } /* RRC  (XY+o)      */
void xycb_0f() { A = RRC( RM(EA) ); WM( EA,A );            } /* RRC  A=(XY+o)    */

void xycb_10() { B = RL( RM(EA) ); WM( EA,B );             } /* RL   B=(XY+o)    */
void xycb_11() { C = RL( RM(EA) ); WM( EA,C );             } /* RL   C=(XY+o)    */
void xycb_12() { D = RL( RM(EA) ); WM( EA,D );             } /* RL   D=(XY+o)    */
void xycb_13() { E = RL( RM(EA) ); WM( EA,E );             } /* RL   E=(XY+o)    */
void xycb_14() { H = RL( RM(EA) ); WM( EA,H );             } /* RL   H=(XY+o)    */
void xycb_15() { L = RL( RM(EA) ); WM( EA,L );             } /* RL   L=(XY+o)    */
void xycb_16() { WM( EA,RL( RM(EA) ) );                    } /* RL   (XY+o)      */
void xycb_17() { A = RL( RM(EA) ); WM( EA,A );             } /* RL   A=(XY+o)    */

void xycb_18() { B = RR( RM(EA) ); WM( EA,B );             } /* RR   B=(XY+o)    */
void xycb_19() { C = RR( RM(EA) ); WM( EA,C );             } /* RR   C=(XY+o)    */
void xycb_1a() { D = RR( RM(EA) ); WM( EA,D );             } /* RR   D=(XY+o)    */
void xycb_1b() { E = RR( RM(EA) ); WM( EA,E );             } /* RR   E=(XY+o)    */
void xycb_1c() { H = RR( RM(EA) ); WM( EA,H );             } /* RR   H=(XY+o)    */
void xycb_1d() { L = RR( RM(EA) ); WM( EA,L );             } /* RR   L=(XY+o)    */
void xycb_1e() { WM( EA,RR( RM(EA) ) );                    } /* RR   (XY+o)      */
void xycb_1f() { A = RR( RM(EA) ); WM( EA,A );             } /* RR   A=(XY+o)    */

void xycb_20() { B = SLA( RM(EA) ); WM( EA,B );            } /* SLA  B=(XY+o)    */
void xycb_21() { C = SLA( RM(EA) ); WM( EA,C );            } /* SLA  C=(XY+o)    */
void xycb_22() { D = SLA( RM(EA) ); WM( EA,D );            } /* SLA  D=(XY+o)    */
void xycb_23() { E = SLA( RM(EA) ); WM( EA,E );            } /* SLA  E=(XY+o)    */
void xycb_24() { H = SLA( RM(EA) ); WM( EA,H );            } /* SLA  H=(XY+o)    */
void xycb_25() { L = SLA( RM(EA) ); WM( EA,L );            } /* SLA  L=(XY+o)    */
void xycb_26() { WM( EA,SLA( RM(EA) ) );                   } /* SLA  (XY+o)      */
void xycb_27() { A = SLA( RM(EA) ); WM( EA,A );            } /* SLA  A=(XY+o)    */

void xycb_28() { B = SRA( RM(EA) ); WM( EA,B );            } /* SRA  B=(XY+o)    */
void xycb_29() { C = SRA( RM(EA) ); WM( EA,C );            } /* SRA  C=(XY+o)    */
void xycb_2a() { D = SRA( RM(EA) ); WM( EA,D );            } /* SRA  D=(XY+o)    */
void xycb_2b() { E = SRA( RM(EA) ); WM( EA,E );            } /* SRA  E=(XY+o)    */
void xycb_2c() { H = SRA( RM(EA) ); WM( EA,H );            } /* SRA  H=(XY+o)    */
void xycb_2d() { L = SRA( RM(EA) ); WM( EA,L );            } /* SRA  L=(XY+o)    */
void xycb_2e() { WM( EA,SRA( RM(EA) ) );                   } /* SRA  (XY+o)      */
void xycb_2f() { A = SRA( RM(EA) ); WM( EA,A );            } /* SRA  A=(XY+o)    */

void xycb_30() { B = SLL( RM(EA) ); WM( EA,B );            } /* SLL  B=(XY+o)    */
void xycb_31() { C = SLL( RM(EA) ); WM( EA,C );            } /* SLL  C=(XY+o)    */
void xycb_32() { D = SLL( RM(EA) ); WM( EA,D );            } /* SLL  D=(XY+o)    */
void xycb_33() { E = SLL( RM(EA) ); WM( EA,E );            } /* SLL  E=(XY+o)    */
void xycb_34() { H = SLL( RM(EA) ); WM( EA,H );            } /* SLL  H=(XY+o)    */
void xycb_35() { L = SLL( RM(EA) ); WM( EA,L );            } /* SLL  L=(XY+o)    */
void xycb_36() { WM( EA,SLL( RM(EA) ) );                   } /* SLL  (XY+o)      */
void xycb_37() { A = SLL( RM(EA) ); WM( EA,A );            } /* SLL  A=(XY+o)    */

void xycb_38() { B = SRL( RM(EA) ); WM( EA,B );            } /* SRL  B=(XY+o)    */
void xycb_39() { C = SRL( RM(EA) ); WM( EA,C );            } /* SRL  C=(XY+o)    */
void xycb_3a() { D = SRL( RM(EA) ); WM( EA,D );            } /* SRL  D=(XY+o)    */
void xycb_3b() { E = SRL( RM(EA) ); WM( EA,E );            } /* SRL  E=(XY+o)    */
void xycb_3c() { H = SRL( RM(EA) ); WM( EA,H );            } /* SRL  H=(XY+o)    */
void xycb_3d() { L = SRL( RM(EA) ); WM( EA,L );            } /* SRL  L=(XY+o)    */
void xycb_3e() { WM( EA,SRL( RM(EA) ) );                   } /* SRL  (XY+o)      */
void xycb_3f() { A = SRL( RM(EA) ); WM( EA,A );            } /* SRL  A=(XY+o)    */

void xycb_40() { xycb_46();                                } /* BIT  0,(XY+o)    */
void xycb_41() { xycb_46();                                } /* BIT  0,(XY+o)    */
void xycb_42() { xycb_46();                                } /* BIT  0,(XY+o)    */
void xycb_43() { xycb_46();                                } /* BIT  0,(XY+o)    */
void xycb_44() { xycb_46();                                } /* BIT  0,(XY+o)    */
void xycb_45() { xycb_46();                                } /* BIT  0,(XY+o)    */
void xycb_46() { BIT_XY(0,RM(EA));                         } /* BIT  0,(XY+o)    */
void xycb_47() { xycb_46();                                } /* BIT  0,(XY+o)    */

void xycb_48() { xycb_4e();                                } /* BIT  1,(XY+o)    */
void xycb_49() { xycb_4e();                                } /* BIT  1,(XY+o)    */
void xycb_4a() { xycb_4e();                                } /* BIT  1,(XY+o)    */
void xycb_4b() { xycb_4e();                                } /* BIT  1,(XY+o)    */
void xycb_4c() { xycb_4e();                                } /* BIT  1,(XY+o)    */
void xycb_4d() { xycb_4e();                                } /* BIT  1,(XY+o)    */
void xycb_4e() { BIT_XY(1,RM(EA));                         } /* BIT  1,(XY+o)    */
void xycb_4f() { xycb_4e();                                } /* BIT  1,(XY+o)    */

void xycb_50() { xycb_56();                                } /* BIT  2,(XY+o)    */
void xycb_51() { xycb_56();                                } /* BIT  2,(XY+o)    */
void xycb_52() { xycb_56();                                } /* BIT  2,(XY+o)    */
void xycb_53() { xycb_56();                                } /* BIT  2,(XY+o)    */
void xycb_54() { xycb_56();                                } /* BIT  2,(XY+o)    */
void xycb_55() { xycb_56();                                } /* BIT  2,(XY+o)    */
void xycb_56() { BIT_XY(2,RM(EA));                         } /* BIT  2,(XY+o)    */
void xycb_57() { xycb_56();                                } /* BIT  2,(XY+o)    */

void xycb_58() { xycb_5e();                                } /* BIT  3,(XY+o)    */
void xycb_59() { xycb_5e();                                } /* BIT  3,(XY+o)    */
void xycb_5a() { xycb_5e();                                } /* BIT  3,(XY+o)    */
void xycb_5b() { xycb_5e();                                } /* BIT  3,(XY+o)    */
void xycb_5c() { xycb_5e();                                } /* BIT  3,(XY+o)    */
void xycb_5d() { xycb_5e();                                } /* BIT  3,(XY+o)    */
void xycb_5e() { BIT_XY(3,RM(EA));                         } /* BIT  3,(XY+o)    */
void xycb_5f() { xycb_5e();                                } /* BIT  3,(XY+o)    */

void xycb_60() { xycb_66();                                } /* BIT  4,(XY+o)    */
void xycb_61() { xycb_66();                                } /* BIT  4,(XY+o)    */
void xycb_62() { xycb_66();                                } /* BIT  4,(XY+o)    */
void xycb_63() { xycb_66();                                } /* BIT  4,(XY+o)    */
void xycb_64() { xycb_66();                                } /* BIT  4,(XY+o)    */
void xycb_65() { xycb_66();                                } /* BIT  4,(XY+o)    */
void xycb_66() { BIT_XY(4,RM(EA));                         } /* BIT  4,(XY+o)    */
void xycb_67() { xycb_66();                                } /* BIT  4,(XY+o)    */

void xycb_68() { xycb_6e();                                } /* BIT  5,(XY+o)    */
void xycb_69() { xycb_6e();                                } /* BIT  5,(XY+o)    */
void xycb_6a() { xycb_6e();                                } /* BIT  5,(XY+o)    */
void xycb_6b() { xycb_6e();                                } /* BIT  5,(XY+o)    */
void xycb_6c() { xycb_6e();                                } /* BIT  5,(XY+o)    */
void xycb_6d() { xycb_6e();                                } /* BIT  5,(XY+o)    */
void xycb_6e() { BIT_XY(5,RM(EA));                         } /* BIT  5,(XY+o)    */
void xycb_6f() { xycb_6e();                                } /* BIT  5,(XY+o)    */

void xycb_70() { xycb_76();                                } /* BIT  6,(XY+o)    */
void xycb_71() { xycb_76();                                } /* BIT  6,(XY+o)    */
void xycb_72() { xycb_76();                                } /* BIT  6,(XY+o)    */
void xycb_73() { xycb_76();                                } /* BIT  6,(XY+o)    */
void xycb_74() { xycb_76();                                } /* BIT  6,(XY+o)    */
void xycb_75() { xycb_76();                                } /* BIT  6,(XY+o)    */
void xycb_76() { BIT_XY(6,RM(EA));                         } /* BIT  6,(XY+o)    */
void xycb_77() { xycb_76();                                } /* BIT  6,(XY+o)    */

void xycb_78() { xycb_7e();                                } /* BIT  7,(XY+o)    */
void xycb_79() { xycb_7e();                                } /* BIT  7,(XY+o)    */
void xycb_7a() { xycb_7e();                                } /* BIT  7,(XY+o)    */
void xycb_7b() { xycb_7e();                                } /* BIT  7,(XY+o)    */
void xycb_7c() { xycb_7e();                                } /* BIT  7,(XY+o)    */
void xycb_7d() { xycb_7e();                                } /* BIT  7,(XY+o)    */
void xycb_7e() { BIT_XY(7,RM(EA));                         } /* BIT  7,(XY+o)    */
void xycb_7f() { xycb_7e();                                } /* BIT  7,(XY+o)    */

void xycb_80() { B = RES(0, RM(EA) ); WM( EA,B );          } /* RES  0,B=(XY+o)  */
void xycb_81() { C = RES(0, RM(EA) ); WM( EA,C );          } /* RES  0,C=(XY+o)  */
void xycb_82() { D = RES(0, RM(EA) ); WM( EA,D );          } /* RES  0,D=(XY+o)  */
void xycb_83() { E = RES(0, RM(EA) ); WM( EA,E );          } /* RES  0,E=(XY+o)  */
void xycb_84() { H = RES(0, RM(EA) ); WM( EA,H );          } /* RES  0,H=(XY+o)  */
void xycb_85() { L = RES(0, RM(EA) ); WM( EA,L );          } /* RES  0,L=(XY+o)  */
void xycb_86() { WM( EA, RES(0,RM(EA)) );                  } /* RES  0,(XY+o)    */
void xycb_87() { A = RES(0, RM(EA) ); WM( EA,A );          } /* RES  0,A=(XY+o)  */

void xycb_88() { B = RES(1, RM(EA) ); WM( EA,B );          } /* RES  1,B=(XY+o)  */
void xycb_89() { C = RES(1, RM(EA) ); WM( EA,C );          } /* RES  1,C=(XY+o)  */
void xycb_8a() { D = RES(1, RM(EA) ); WM( EA,D );          } /* RES  1,D=(XY+o)  */
void xycb_8b() { E = RES(1, RM(EA) ); WM( EA,E );          } /* RES  1,E=(XY+o)  */
void xycb_8c() { H = RES(1, RM(EA) ); WM( EA,H );          } /* RES  1,H=(XY+o)  */
void xycb_8d() { L = RES(1, RM(EA) ); WM( EA,L );          } /* RES  1,L=(XY+o)  */
void xycb_8e() { WM( EA, RES(1,RM(EA)) );                  } /* RES  1,(XY+o)    */
void xycb_8f() { A = RES(1, RM(EA) ); WM( EA,A );          } /* RES  1,A=(XY+o)  */

void xycb_90() { B = RES(2, RM(EA) ); WM( EA,B );          } /* RES  2,B=(XY+o)  */
void xycb_91() { C = RES(2, RM(EA) ); WM( EA,C );          } /* RES  2,C=(XY+o)  */
void xycb_92() { D = RES(2, RM(EA) ); WM( EA,D );          } /* RES  2,D=(XY+o)  */
void xycb_93() { E = RES(2, RM(EA) ); WM( EA,E );          } /* RES  2,E=(XY+o)  */
void xycb_94() { H = RES(2, RM(EA) ); WM( EA,H );          } /* RES  2,H=(XY+o)  */
void xycb_95() { L = RES(2, RM(EA) ); WM( EA,L );          } /* RES  2,L=(XY+o)  */
void xycb_96() { WM( EA, RES(2,RM(EA)) );                  } /* RES  2,(XY+o)    */
void xycb_97() { A = RES(2, RM(EA) ); WM( EA,A );          } /* RES  2,A=(XY+o)  */

void xycb_98() { B = RES(3, RM(EA) ); WM( EA,B );          } /* RES  3,B=(XY+o)  */
void xycb_99() { C = RES(3, RM(EA) ); WM( EA,C );          } /* RES  3,C=(XY+o)  */
void xycb_9a() { D = RES(3, RM(EA) ); WM( EA,D );          } /* RES  3,D=(XY+o)  */
void xycb_9b() { E = RES(3, RM(EA) ); WM( EA,E );          } /* RES  3,E=(XY+o)  */
void xycb_9c() { H = RES(3, RM(EA) ); WM( EA,H );          } /* RES  3,H=(XY+o)  */
void xycb_9d() { L = RES(3, RM(EA) ); WM( EA,L );          } /* RES  3,L=(XY+o)  */
void xycb_9e() { WM( EA, RES(3,RM(EA)) );                  } /* RES  3,(XY+o)    */
void xycb_9f() { A = RES(3, RM(EA) ); WM( EA,A );          } /* RES  3,A=(XY+o)  */

void xycb_a0() { B = RES(4, RM(EA) ); WM( EA,B );          } /* RES  4,B=(XY+o)  */
void xycb_a1() { C = RES(4, RM(EA) ); WM( EA,C );          } /* RES  4,C=(XY+o)  */
void xycb_a2() { D = RES(4, RM(EA) ); WM( EA,D );          } /* RES  4,D=(XY+o)  */
void xycb_a3() { E = RES(4, RM(EA) ); WM( EA,E );          } /* RES  4,E=(XY+o)  */
void xycb_a4() { H = RES(4, RM(EA) ); WM( EA,H );          } /* RES  4,H=(XY+o)  */
void xycb_a5() { L = RES(4, RM(EA) ); WM( EA,L );          } /* RES  4,L=(XY+o)  */
void xycb_a6() { WM( EA, RES(4,RM(EA)) );                  } /* RES  4,(XY+o)    */
void xycb_a7() { A = RES(4, RM(EA) ); WM( EA,A );          } /* RES  4,A=(XY+o)  */

void xycb_a8() { B = RES(5, RM(EA) ); WM( EA,B );          } /* RES  5,B=(XY+o)  */
void xycb_a9() { C = RES(5, RM(EA) ); WM( EA,C );          } /* RES  5,C=(XY+o)  */
void xycb_aa() { D = RES(5, RM(EA) ); WM( EA,D );          } /* RES  5,D=(XY+o)  */
void xycb_ab() { E = RES(5, RM(EA) ); WM( EA,E );          } /* RES  5,E=(XY+o)  */
void xycb_ac() { H = RES(5, RM(EA) ); WM( EA,H );          } /* RES  5,H=(XY+o)  */
void xycb_ad() { L = RES(5, RM(EA) ); WM( EA,L );          } /* RES  5,L=(XY+o)  */
void xycb_ae() { WM( EA, RES(5,RM(EA)) );                  } /* RES  5,(XY+o)    */
void xycb_af() { A = RES(5, RM(EA) ); WM( EA,A );          } /* RES  5,A=(XY+o)  */

void xycb_b0() { B = RES(6, RM(EA) ); WM( EA,B );          } /* RES  6,B=(XY+o)  */
void xycb_b1() { C = RES(6, RM(EA) ); WM( EA,C );          } /* RES  6,C=(XY+o)  */
void xycb_b2() { D = RES(6, RM(EA) ); WM( EA,D );          } /* RES  6,D=(XY+o)  */
void xycb_b3() { E = RES(6, RM(EA) ); WM( EA,E );          } /* RES  6,E=(XY+o)  */
void xycb_b4() { H = RES(6, RM(EA) ); WM( EA,H );          } /* RES  6,H=(XY+o)  */
void xycb_b5() { L = RES(6, RM(EA) ); WM( EA,L );          } /* RES  6,L=(XY+o)  */
void xycb_b6() { WM( EA, RES(6,RM(EA)) );                  } /* RES  6,(XY+o)    */
void xycb_b7() { A = RES(6, RM(EA) ); WM( EA,A );          } /* RES  6,A=(XY+o)  */

void xycb_b8() { B = RES(7, RM(EA) ); WM( EA,B );          } /* RES  7,B=(XY+o)  */
void xycb_b9() { C = RES(7, RM(EA) ); WM( EA,C );          } /* RES  7,C=(XY+o)  */
void xycb_ba() { D = RES(7, RM(EA) ); WM( EA,D );          } /* RES  7,D=(XY+o)  */
void xycb_bb() { E = RES(7, RM(EA) ); WM( EA,E );          } /* RES  7,E=(XY+o)  */
void xycb_bc() { H = RES(7, RM(EA) ); WM( EA,H );          } /* RES  7,H=(XY+o)  */
void xycb_bd() { L = RES(7, RM(EA) ); WM( EA,L );          } /* RES  7,L=(XY+o)  */
void xycb_be() { WM( EA, RES(7,RM(EA)) );                  } /* RES  7,(XY+o)    */
void xycb_bf() { A = RES(7, RM(EA) ); WM( EA,A );          } /* RES  7,A=(XY+o)  */

void xycb_c0() { B = SET(0, RM(EA) ); WM( EA,B );          } /* SET  0,B=(XY+o)  */
void xycb_c1() { C = SET(0, RM(EA) ); WM( EA,C );          } /* SET  0,C=(XY+o)  */
void xycb_c2() { D = SET(0, RM(EA) ); WM( EA,D );          } /* SET  0,D=(XY+o)  */
void xycb_c3() { E = SET(0, RM(EA) ); WM( EA,E );          } /* SET  0,E=(XY+o)  */
void xycb_c4() { H = SET(0, RM(EA) ); WM( EA,H );          } /* SET  0,H=(XY+o)  */
void xycb_c5() { L = SET(0, RM(EA) ); WM( EA,L );          } /* SET  0,L=(XY+o)  */
void xycb_c6() { WM( EA, SET(0,RM(EA)) );                  } /* SET  0,(XY+o)    */
void xycb_c7() { A = SET(0, RM(EA) ); WM( EA,A );          } /* SET  0,A=(XY+o)  */

void xycb_c8() { B = SET(1, RM(EA) ); WM( EA,B );          } /* SET  1,B=(XY+o)  */
void xycb_c9() { C = SET(1, RM(EA) ); WM( EA,C );          } /* SET  1,C=(XY+o)  */
void xycb_ca() { D = SET(1, RM(EA) ); WM( EA,D );          } /* SET  1,D=(XY+o)  */
void xycb_cb() { E = SET(1, RM(EA) ); WM( EA,E );          } /* SET  1,E=(XY+o)  */
void xycb_cc() { H = SET(1, RM(EA) ); WM( EA,H );          } /* SET  1,H=(XY+o)  */
void xycb_cd() { L = SET(1, RM(EA) ); WM( EA,L );          } /* SET  1,L=(XY+o)  */
void xycb_ce() { WM( EA, SET(1,RM(EA)) );                  } /* SET  1,(XY+o)    */
void xycb_cf() { A = SET(1, RM(EA) ); WM( EA,A );          } /* SET  1,A=(XY+o)  */

void xycb_d0() { B = SET(2, RM(EA) ); WM( EA,B );          } /* SET  2,B=(XY+o)  */
void xycb_d1() { C = SET(2, RM(EA) ); WM( EA,C );          } /* SET  2,C=(XY+o)  */
void xycb_d2() { D = SET(2, RM(EA) ); WM( EA,D );          } /* SET  2,D=(XY+o)  */
void xycb_d3() { E = SET(2, RM(EA) ); WM( EA,E );          } /* SET  2,E=(XY+o)  */
void xycb_d4() { H = SET(2, RM(EA) ); WM( EA,H );          } /* SET  2,H=(XY+o)  */
void xycb_d5() { L = SET(2, RM(EA) ); WM( EA,L );          } /* SET  2,L=(XY+o)  */
void xycb_d6() { WM( EA, SET(2,RM(EA)) );                  } /* SET  2,(XY+o)    */
void xycb_d7() { A = SET(2, RM(EA) ); WM( EA,A );          } /* SET  2,A=(XY+o)  */

void xycb_d8() { B = SET(3, RM(EA) ); WM( EA,B );          } /* SET  3,B=(XY+o)  */
void xycb_d9() { C = SET(3, RM(EA) ); WM( EA,C );          } /* SET  3,C=(XY+o)  */
void xycb_da() { D = SET(3, RM(EA) ); WM( EA,D );          } /* SET  3,D=(XY+o)  */
void xycb_db() { E = SET(3, RM(EA) ); WM( EA,E );          } /* SET  3,E=(XY+o)  */
void xycb_dc() { H = SET(3, RM(EA) ); WM( EA,H );          } /* SET  3,H=(XY+o)  */
void xycb_dd() { L = SET(3, RM(EA) ); WM( EA,L );          } /* SET  3,L=(XY+o)  */
void xycb_de() { WM( EA, SET(3,RM(EA)) );                  } /* SET  3,(XY+o)    */
void xycb_df() { A = SET(3, RM(EA) ); WM( EA,A );          } /* SET  3,A=(XY+o)  */

void xycb_e0() { B = SET(4, RM(EA) ); WM( EA,B );          } /* SET  4,B=(XY+o)  */
void xycb_e1() { C = SET(4, RM(EA) ); WM( EA,C );          } /* SET  4,C=(XY+o)  */
void xycb_e2() { D = SET(4, RM(EA) ); WM( EA,D );          } /* SET  4,D=(XY+o)  */
void xycb_e3() { E = SET(4, RM(EA) ); WM( EA,E );          } /* SET  4,E=(XY+o)  */
void xycb_e4() { H = SET(4, RM(EA) ); WM( EA,H );          } /* SET  4,H=(XY+o)  */
void xycb_e5() { L = SET(4, RM(EA) ); WM( EA,L );          } /* SET  4,L=(XY+o)  */
void xycb_e6() { WM( EA, SET(4,RM(EA)) );                  } /* SET  4,(XY+o)    */
void xycb_e7() { A = SET(4, RM(EA) ); WM( EA,A );          } /* SET  4,A=(XY+o)  */

void xycb_e8() { B = SET(5, RM(EA) ); WM( EA,B );          } /* SET  5,B=(XY+o)  */
void xycb_e9() { C = SET(5, RM(EA) ); WM( EA,C );          } /* SET  5,C=(XY+o)  */
void xycb_ea() { D = SET(5, RM(EA) ); WM( EA,D );          } /* SET  5,D=(XY+o)  */
void xycb_eb() { E = SET(5, RM(EA) ); WM( EA,E );          } /* SET  5,E=(XY+o)  */
void xycb_ec() { H = SET(5, RM(EA) ); WM( EA,H );          } /* SET  5,H=(XY+o)  */
void xycb_ed() { L = SET(5, RM(EA) ); WM( EA,L );          } /* SET  5,L=(XY+o)  */
void xycb_ee() { WM( EA, SET(5,RM(EA)) );                  } /* SET  5,(XY+o)    */
void xycb_ef() { A = SET(5, RM(EA) ); WM( EA,A );          } /* SET  5,A=(XY+o)  */

void xycb_f0() { B = SET(6, RM(EA) ); WM( EA,B );          } /* SET  6,B=(XY+o)  */
void xycb_f1() { C = SET(6, RM(EA) ); WM( EA,C );          } /* SET  6,C=(XY+o)  */
void xycb_f2() { D = SET(6, RM(EA) ); WM( EA,D );          } /* SET  6,D=(XY+o)  */
void xycb_f3() { E = SET(6, RM(EA) ); WM( EA,E );          } /* SET  6,E=(XY+o)  */
void xycb_f4() { H = SET(6, RM(EA) ); WM( EA,H );          } /* SET  6,H=(XY+o)  */
void xycb_f5() { L = SET(6, RM(EA) ); WM( EA,L );          } /* SET  6,L=(XY+o)  */
void xycb_f6() { WM( EA, SET(6,RM(EA)) );                  } /* SET  6,(XY+o)    */
void xycb_f7() { A = SET(6, RM(EA) ); WM( EA,A );          } /* SET  6,A=(XY+o)  */

void xycb_f8() { B = SET(7, RM(EA) ); WM( EA,B );          } /* SET  7,B=(XY+o)  */
void xycb_f9() { C = SET(7, RM(EA) ); WM( EA,C );          } /* SET  7,C=(XY+o)  */
void xycb_fa() { D = SET(7, RM(EA) ); WM( EA,D );          } /* SET  7,D=(XY+o)  */
void xycb_fb() { E = SET(7, RM(EA) ); WM( EA,E );          } /* SET  7,E=(XY+o)  */
void xycb_fc() { H = SET(7, RM(EA) ); WM( EA,H );          } /* SET  7,H=(XY+o)  */
void xycb_fd() { L = SET(7, RM(EA) ); WM( EA,L );          } /* SET  7,L=(XY+o)  */
void xycb_fe() { WM( EA, SET(7,RM(EA)) );                  } /* SET  7,(XY+o)    */
void xycb_ff() { A = SET(7, RM(EA) ); WM( EA,A );          } /* SET  7,A=(XY+o)  */

void illegal_1() {
#if VERBOSE
  logerror("Z80 #%d ill. opcode $%02x $%02x\n",
      cpu_getactivecpu(), cpu_readop((PCD-1)&0xffff), cpu_readop(PCD));
#endif
}
/**********************************************************
 * IX register related opcodes (DD prefix)
 **********************************************************/
void dd_00() { illegal_1(); op_00();                             } /* DB   DD       */
void dd_01() { illegal_1(); op_01();                             } /* DB   DD       */
void dd_02() { illegal_1(); op_02();                             } /* DB   DD       */
void dd_03() { illegal_1(); op_03();                             } /* DB   DD       */
void dd_04() { illegal_1(); op_04();                             } /* DB   DD       */
void dd_05() { illegal_1(); op_05();                             } /* DB   DD       */
void dd_06() { illegal_1(); op_06();                             } /* DB   DD       */
void dd_07() { illegal_1(); op_07();                             } /* DB   DD       */

void dd_08() { illegal_1(); op_08();                             } /* DB   DD       */
void dd_09() { ADD16(ix,bc);                                     } /* ADD  IX,BC    */
void dd_0a() { illegal_1(); op_0a();                             } /* DB   DD       */
void dd_0b() { illegal_1(); op_0b();                             } /* DB   DD       */
void dd_0c() { illegal_1(); op_0c();                             } /* DB   DD       */
void dd_0d() { illegal_1(); op_0d();                             } /* DB   DD       */
void dd_0e() { illegal_1(); op_0e();                             } /* DB   DD       */
void dd_0f() { illegal_1(); op_0f();                             } /* DB   DD       */

void dd_10() { illegal_1(); op_10();                             } /* DB   DD       */
void dd_11() { illegal_1(); op_11();                             } /* DB   DD       */
void dd_12() { illegal_1(); op_12();                             } /* DB   DD       */
void dd_13() { illegal_1(); op_13();                             } /* DB   DD       */
void dd_14() { illegal_1(); op_14();                             } /* DB   DD       */
void dd_15() { illegal_1(); op_15();                             } /* DB   DD       */
void dd_16() { illegal_1(); op_16();                             } /* DB   DD       */
void dd_17() { illegal_1(); op_17();                             } /* DB   DD       */

void dd_18() { illegal_1(); op_18();                             } /* DB   DD       */
void dd_19() { ADD16(ix,de);                                     } /* ADD  IX,DE    */
void dd_1a() { illegal_1(); op_1a();                             } /* DB   DD       */
void dd_1b() { illegal_1(); op_1b();                             } /* DB   DD       */
void dd_1c() { illegal_1(); op_1c();                             } /* DB   DD       */
void dd_1d() { illegal_1(); op_1d();                             } /* DB   DD       */
void dd_1e() { illegal_1(); op_1e();                             } /* DB   DD       */
void dd_1f() { illegal_1(); op_1f();                             } /* DB   DD       */

void dd_20() { illegal_1(); op_20();                             } /* DB   DD       */
void dd_21() { IX = ARG16();                                     } /* LD   IX,w     */
void dd_22() { EA = ARG16(); WM16( EA, &Z80.ix ); WZ = EA+1;     } /* LD   (w),IX   */
void dd_23() { IX++;                                             } /* INC  IX       */
void dd_24() { HX = INC(HX);                                     } /* INC  HX       */
void dd_25() { HX = DEC(HX);                                     } /* DEC  HX       */
void dd_26() { HX = ARG();                                       } /* LD   HX,n     */
void dd_27() { illegal_1(); op_27();                             } /* DB   DD       */

void dd_28() { illegal_1(); op_28();                             } /* DB   DD       */
void dd_29() { ADD16(ix,ix);                                     } /* ADD  IX,IX    */
void dd_2a() { EA = ARG16(); RM16( EA, &Z80.ix ); WZ = EA+1;     } /* LD   IX,(w)   */
void dd_2b() { IX--;                                             } /* DEC  IX       */
void dd_2c() { LX = INC(LX);                                     } /* INC  LX       */
void dd_2d() { LX = DEC(LX);                                     } /* DEC  LX       */
void dd_2e() { LX = ARG();                                       } /* LD   LX,n     */
void dd_2f() { illegal_1(); op_2f();                             } /* DB   DD       */

void dd_30() { illegal_1(); op_30();                             } /* DB   DD       */
void dd_31() { illegal_1(); op_31();                             } /* DB   DD       */
void dd_32() { illegal_1(); op_32();                             } /* DB   DD       */
void dd_33() { illegal_1(); op_33();                             } /* DB   DD       */
void dd_34() { EAX(); WM( EA, INC(RM(EA)) );                       } /* INC  (IX+o)   */
void dd_35() { EAX(); WM( EA, DEC(RM(EA)) );                       } /* DEC  (IX+o)   */
void dd_36() { EAX(); WM( EA, ARG() );                             } /* LD   (IX+o),n */
void dd_37() { illegal_1(); op_37();                             } /* DB   DD       */

void dd_38() { illegal_1(); op_38();                             } /* DB   DD       */
void dd_39() { ADD16(ix,sp);                                     } /* ADD  IX,SP    */
void dd_3a() { illegal_1(); op_3a();                             } /* DB   DD       */
void dd_3b() { illegal_1(); op_3b();                             } /* DB   DD       */
void dd_3c() { illegal_1(); op_3c();                             } /* DB   DD       */
void dd_3d() { illegal_1(); op_3d();                             } /* DB   DD       */
void dd_3e() { illegal_1(); op_3e();                             } /* DB   DD       */
void dd_3f() { illegal_1(); op_3f();                             } /* DB   DD       */

void dd_40() { illegal_1(); op_40();                             } /* DB   DD       */
void dd_41() { illegal_1(); op_41();                             } /* DB   DD       */
void dd_42() { illegal_1(); op_42();                             } /* DB   DD       */
void dd_43() { illegal_1(); op_43();                             } /* DB   DD       */
void dd_44() { B = HX;                                           } /* LD   B,HX     */
void dd_45() { B = LX;                                           } /* LD   B,LX     */
void dd_46() { EAX(); B = RM(EA);                                  } /* LD   B,(IX+o) */
void dd_47() { illegal_1(); op_47();                             } /* DB   DD       */

void dd_48() { illegal_1(); op_48();                             } /* DB   DD       */
void dd_49() { illegal_1(); op_49();                             } /* DB   DD       */
void dd_4a() { illegal_1(); op_4a();                             } /* DB   DD       */
void dd_4b() { illegal_1(); op_4b();                             } /* DB   DD       */
void dd_4c() { C = HX;                                           } /* LD   C,HX     */
void dd_4d() { C = LX;                                           } /* LD   C,LX     */
void dd_4e() { EAX(); C = RM(EA);                                  } /* LD   C,(IX+o) */
void dd_4f() { illegal_1(); op_4f();                             } /* DB   DD       */

void dd_50() { illegal_1(); op_50();                             } /* DB   DD       */
void dd_51() { illegal_1(); op_51();                             } /* DB   DD       */
void dd_52() { illegal_1(); op_52();                             } /* DB   DD       */
void dd_53() { illegal_1(); op_53();                             } /* DB   DD       */
void dd_54() { D = HX;                                           } /* LD   D,HX     */
void dd_55() { D = LX;                                           } /* LD   D,LX     */
void dd_56() { EAX(); D = RM(EA);                                  } /* LD   D,(IX+o) */
void dd_57() { illegal_1(); op_57();                             } /* DB   DD       */

void dd_58() { illegal_1(); op_58();                             } /* DB   DD       */
void dd_59() { illegal_1(); op_59();                             } /* DB   DD       */
void dd_5a() { illegal_1(); op_5a();                             } /* DB   DD       */
void dd_5b() { illegal_1(); op_5b();                             } /* DB   DD       */
void dd_5c() { E = HX;                                           } /* LD   E,HX     */
void dd_5d() { E = LX;                                           } /* LD   E,LX     */
void dd_5e() { EAX(); E = RM(EA);                                  } /* LD   E,(IX+o) */
void dd_5f() { illegal_1(); op_5f();                             } /* DB   DD       */

void dd_60() { HX = B;                                           } /* LD   HX,B     */
void dd_61() { HX = C;                                           } /* LD   HX,C     */
void dd_62() { HX = D;                                           } /* LD   HX,D     */
void dd_63() { HX = E;                                           } /* LD   HX,E     */
void dd_64() {                                                   } /* LD   HX,HX    */
void dd_65() { HX = LX;                                          } /* LD   HX,LX    */
void dd_66() { EAX(); H = RM(EA);                                  } /* LD   H,(IX+o) */
void dd_67() { HX = A;                                           } /* LD   HX,A     */

void dd_68() { LX = B;                                           } /* LD   LX,B     */
void dd_69() { LX = C;                                           } /* LD   LX,C     */
void dd_6a() { LX = D;                                           } /* LD   LX,D     */
void dd_6b() { LX = E;                                           } /* LD   LX,E     */
void dd_6c() { LX = HX;                                          } /* LD   LX,HX    */
void dd_6d() {                                                   } /* LD   LX,LX    */
void dd_6e() { EAX(); L = RM(EA);                                  } /* LD   L,(IX+o) */
void dd_6f() { LX = A;                                           } /* LD   LX,A     */

void dd_70() { EAX(); WM( EA, B );                                 } /* LD   (IX+o),B */
void dd_71() { EAX(); WM( EA, C );                                 } /* LD   (IX+o),C */
void dd_72() { EAX(); WM( EA, D );                                 } /* LD   (IX+o),D */
void dd_73() { EAX(); WM( EA, E );                                 } /* LD   (IX+o),E */
void dd_74() { EAX(); WM( EA, H );                                 } /* LD   (IX+o),H */
void dd_75() { EAX(); WM( EA, L );                                 } /* LD   (IX+o),L */
void dd_76() { illegal_1(); op_76();                             } /* DB   DD       */
void dd_77() { EAX(); WM( EA, A );                                 } /* LD   (IX+o),A */

void dd_78() { illegal_1(); op_78();                             } /* DB   DD       */
void dd_79() { illegal_1(); op_79();                             } /* DB   DD       */
void dd_7a() { illegal_1(); op_7a();                             } /* DB   DD       */
void dd_7b() { illegal_1(); op_7b();                             } /* DB   DD       */
void dd_7c() { A = HX;                                           } /* LD   A,HX     */
void dd_7d() { A = LX;                                           } /* LD   A,LX     */
void dd_7e() { EAX(); A = RM(EA);                                  } /* LD   A,(IX+o) */
void dd_7f() { illegal_1(); op_7f();                             } /* DB   DD       */

void dd_80() { illegal_1(); op_80();                             } /* DB   DD       */
void dd_81() { illegal_1(); op_81();                             } /* DB   DD       */
void dd_82() { illegal_1(); op_82();                             } /* DB   DD       */
void dd_83() { illegal_1(); op_83();                             } /* DB   DD       */
void dd_84() { ADD(HX);                                          } /* ADD  A,HX     */
void dd_85() { ADD(LX);                                          } /* ADD  A,LX     */
void dd_86() { EAX(); ADD(RM(EA));                                 } /* ADD  A,(IX+o) */
void dd_87() { illegal_1(); op_87();                             } /* DB   DD       */

void dd_88() { illegal_1(); op_88();                             } /* DB   DD       */
void dd_89() { illegal_1(); op_89();                             } /* DB   DD       */
void dd_8a() { illegal_1(); op_8a();                             } /* DB   DD       */
void dd_8b() { illegal_1(); op_8b();                             } /* DB   DD       */
void dd_8c() { ADC(HX);                                          } /* ADC  A,HX     */
void dd_8d() { ADC(LX);                                          } /* ADC  A,LX     */
void dd_8e() { EAX(); ADC(RM(EA));                                 } /* ADC  A,(IX+o) */
void dd_8f() { illegal_1(); op_8f();                             } /* DB   DD       */

void dd_90() { illegal_1(); op_90();                             } /* DB   DD       */
void dd_91() { illegal_1(); op_91();                             } /* DB   DD       */
void dd_92() { illegal_1(); op_92();                             } /* DB   DD       */
void dd_93() { illegal_1(); op_93();                             } /* DB   DD       */
void dd_94() { SUB(HX);                                          } /* SUB  HX       */
void dd_95() { SUB(LX);                                          } /* SUB  LX       */
void dd_96() { EAX(); SUB(RM(EA));                                 } /* SUB  (IX+o)   */
void dd_97() { illegal_1(); op_97();                             } /* DB   DD       */

void dd_98() { illegal_1(); op_98();                             } /* DB   DD       */
void dd_99() { illegal_1(); op_99();                             } /* DB   DD       */
void dd_9a() { illegal_1(); op_9a();                             } /* DB   DD       */
void dd_9b() { illegal_1(); op_9b();                             } /* DB   DD       */
void dd_9c() { SBC(HX);                                          } /* SBC  A,HX     */
void dd_9d() { SBC(LX);                                          } /* SBC  A,LX     */
void dd_9e() { EAX(); SBC(RM(EA));                                 } /* SBC  A,(IX+o) */
void dd_9f() { illegal_1(); op_9f();                             } /* DB   DD       */

void dd_a0() { illegal_1(); op_a0();                             } /* DB   DD       */
void dd_a1() { illegal_1(); op_a1();                             } /* DB   DD       */
void dd_a2() { illegal_1(); op_a2();                             } /* DB   DD       */
void dd_a3() { illegal_1(); op_a3();                             } /* DB   DD       */
void dd_a4() { AND(HX);                                          } /* AND  HX       */
void dd_a5() { AND(LX);                                          } /* AND  LX       */
void dd_a6() { EAX(); AND(RM(EA));                                 } /* AND  (IX+o)   */
void dd_a7() { illegal_1(); op_a7();                             } /* DB   DD       */

void dd_a8() { illegal_1(); op_a8();                             } /* DB   DD       */
void dd_a9() { illegal_1(); op_a9();                             } /* DB   DD       */
void dd_aa() { illegal_1(); op_aa();                             } /* DB   DD       */
void dd_ab() { illegal_1(); op_ab();                             } /* DB   DD       */
void dd_ac() { XOR(HX);                                          } /* XOR  HX       */
void dd_ad() { XOR(LX);                                          } /* XOR  LX       */
void dd_ae() { EAX(); XOR(RM(EA));                                 } /* XOR  (IX+o)   */
void dd_af() { illegal_1(); op_af();                             } /* DB   DD       */

void dd_b0() { illegal_1(); op_b0();                             } /* DB   DD       */
void dd_b1() { illegal_1(); op_b1();                             } /* DB   DD       */
void dd_b2() { illegal_1(); op_b2();                             } /* DB   DD       */
void dd_b3() { illegal_1(); op_b3();                             } /* DB   DD       */
void dd_b4() { OR(HX);                                           } /* OR   HX       */
void dd_b5() { OR(LX);                                           } /* OR   LX       */
void dd_b6() { EAX(); OR(RM(EA));                                  } /* OR   (IX+o)   */
void dd_b7() { illegal_1(); op_b7();                             } /* DB   DD       */

void dd_b8() { illegal_1(); op_b8();                             } /* DB   DD       */
void dd_b9() { illegal_1(); op_b9();                             } /* DB   DD       */
void dd_ba() { illegal_1(); op_ba();                             } /* DB   DD       */
void dd_bb() { illegal_1(); op_bb();                             } /* DB   DD       */
void dd_bc() { CP(HX);                                           } /* CP   HX       */
void dd_bd() { CP(LX);                                           } /* CP   LX       */
void dd_be() { EAX(); CP(RM(EA));                                  } /* CP   (IX+o)   */
void dd_bf() { illegal_1(); op_bf();                             } /* DB   DD       */

void dd_c0() { illegal_1(); op_c0();                             } /* DB   DD       */
void dd_c1() { illegal_1(); op_c1();                             } /* DB   DD       */
void dd_c2() { illegal_1(); op_c2();                             } /* DB   DD       */
void dd_c3() { illegal_1(); op_c3();                             } /* DB   DD       */
void dd_c4() { illegal_1(); op_c4();                             } /* DB   DD       */
void dd_c5() { illegal_1(); op_c5();                             } /* DB   DD       */
void dd_c6() { illegal_1(); op_c6();                             } /* DB   DD       */
void dd_c7() { illegal_1(); op_c7();                             } /* DB   DD       */

void dd_c8() { illegal_1(); op_c8();                             } /* DB   DD       */
void dd_c9() { illegal_1(); op_c9();                             } /* DB   DD       */
void dd_ca() { illegal_1(); op_ca();                             } /* DB   DD       */
void dd_cb() { EAX(); EXEC_xycb(ARG());                            } /* **** DD CB xx */
void dd_cc() { illegal_1(); op_cc();                             } /* DB   DD       */
void dd_cd() { illegal_1(); op_cd();                             } /* DB   DD       */
void dd_ce() { illegal_1(); op_ce();                             } /* DB   DD       */
void dd_cf() { illegal_1(); op_cf();                             } /* DB   DD       */

void dd_d0() { illegal_1(); op_d0();                             } /* DB   DD       */
void dd_d1() { illegal_1(); op_d1();                             } /* DB   DD       */
void dd_d2() { illegal_1(); op_d2();                             } /* DB   DD       */
void dd_d3() { illegal_1(); op_d3();                             } /* DB   DD       */
void dd_d4() { illegal_1(); op_d4();                             } /* DB   DD       */
void dd_d5() { illegal_1(); op_d5();                             } /* DB   DD       */
void dd_d6() { illegal_1(); op_d6();                             } /* DB   DD       */
void dd_d7() { illegal_1(); op_d7();                             } /* DB   DD       */

void dd_d8() { illegal_1(); op_d8();                             } /* DB   DD       */
void dd_d9() { illegal_1(); op_d9();                             } /* DB   DD       */
void dd_da() { illegal_1(); op_da();                             } /* DB   DD       */
void dd_db() { illegal_1(); op_db();                             } /* DB   DD       */
void dd_dc() { illegal_1(); op_dc();                             } /* DB   DD       */
void dd_dd() { EXEC_dd(ROP());                                   } /* **** DD DD xx */
void dd_de() { illegal_1(); op_de();                             } /* DB   DD       */
void dd_df() { illegal_1(); op_df();                             } /* DB   DD       */

void dd_e0() { illegal_1(); op_e0();                             } /* DB   DD       */
void dd_e1() { POP_ix();                                        } /* POP  IX       */
void dd_e2() { illegal_1(); op_e2();                             } /* DB   DD       */
void dd_e3() { EXSP_ix();                                       } /* EX   (SP),IX  */
void dd_e4() { illegal_1(); op_e4();                             } /* DB   DD       */
void dd_e5() { PUSH_ix();                                       } /* PUSH IX       */
void dd_e6() { illegal_1(); op_e6();                             } /* DB   DD       */
void dd_e7() { illegal_1(); op_e7();                             } /* DB   DD       */

void dd_e8() { illegal_1(); op_e8();                             } /* DB   DD       */
void dd_e9() { PC = IX;                                          } /* JP   (IX)     */
void dd_ea() { illegal_1(); op_ea();                             } /* DB   DD       */
void dd_eb() { illegal_1(); op_eb();                             } /* DB   DD       */
void dd_ec() { illegal_1(); op_ec();                             } /* DB   DD       */
void dd_ed() { illegal_1(); op_ed();                             } /* DB   DD       */
void dd_ee() { illegal_1(); op_ee();                             } /* DB   DD       */
void dd_ef() { illegal_1(); op_ef();                             } /* DB   DD       */

void dd_f0() { illegal_1(); op_f0();                             } /* DB   DD       */
void dd_f1() { illegal_1(); op_f1();                             } /* DB   DD       */
void dd_f2() { illegal_1(); op_f2();                             } /* DB   DD       */
void dd_f3() { illegal_1(); op_f3();                             } /* DB   DD       */
void dd_f4() { illegal_1(); op_f4();                             } /* DB   DD       */
void dd_f5() { illegal_1(); op_f5();                             } /* DB   DD       */
void dd_f6() { illegal_1(); op_f6();                             } /* DB   DD       */
void dd_f7() { illegal_1(); op_f7();                             } /* DB   DD       */

void dd_f8() { illegal_1(); op_f8();                             } /* DB   DD       */
void dd_f9() { SP = IX;                                          } /* LD   SP,IX    */
void dd_fa() { illegal_1(); op_fa();                             } /* DB   DD       */
void dd_fb() { illegal_1(); op_fb();                             } /* DB   DD       */
void dd_fc() { illegal_1(); op_fc();                             } /* DB   DD       */
void dd_fd() { EXEC_fd(ROP());                                   } /* **** DD FD xx */
void dd_fe() { illegal_1(); op_fe();                             } /* DB   DD       */
void dd_ff() { illegal_1(); op_ff();                             } /* DB   DD       */

/**********************************************************
 * IY register related opcodes (FD prefix)
 **********************************************************/
void fd_00() { illegal_1(); op_00();                             } /* DB   FD       */
void fd_01() { illegal_1(); op_01();                             } /* DB   FD       */
void fd_02() { illegal_1(); op_02();                             } /* DB   FD       */
void fd_03() { illegal_1(); op_03();                             } /* DB   FD       */
void fd_04() { illegal_1(); op_04();                             } /* DB   FD       */
void fd_05() { illegal_1(); op_05();                             } /* DB   FD       */
void fd_06() { illegal_1(); op_06();                             } /* DB   FD       */
void fd_07() { illegal_1(); op_07();                             } /* DB   FD       */

void fd_08() { illegal_1(); op_08();                             } /* DB   FD       */
void fd_09() { ADD16(iy,bc);                                     } /* ADD  IY,BC    */
void fd_0a() { illegal_1(); op_0a();                             } /* DB   FD       */
void fd_0b() { illegal_1(); op_0b();                             } /* DB   FD       */
void fd_0c() { illegal_1(); op_0c();                             } /* DB   FD       */
void fd_0d() { illegal_1(); op_0d();                             } /* DB   FD       */
void fd_0e() { illegal_1(); op_0e();                             } /* DB   FD       */
void fd_0f() { illegal_1(); op_0f();                             } /* DB   FD       */

void fd_10() { illegal_1(); op_10();                             } /* DB   FD       */
void fd_11() { illegal_1(); op_11();                             } /* DB   FD       */
void fd_12() { illegal_1(); op_12();                             } /* DB   FD       */
void fd_13() { illegal_1(); op_13();                             } /* DB   FD       */
void fd_14() { illegal_1(); op_14();                             } /* DB   FD       */
void fd_15() { illegal_1(); op_15();                             } /* DB   FD       */
void fd_16() { illegal_1(); op_16();                             } /* DB   FD       */
void fd_17() { illegal_1(); op_17();                             } /* DB   FD       */

void fd_18() { illegal_1(); op_18();                             } /* DB   FD       */
void fd_19() { ADD16(iy,de);                                     } /* ADD  IY,DE    */
void fd_1a() { illegal_1(); op_1a();                             } /* DB   FD       */
void fd_1b() { illegal_1(); op_1b();                             } /* DB   FD       */
void fd_1c() { illegal_1(); op_1c();                             } /* DB   FD       */
void fd_1d() { illegal_1(); op_1d();                             } /* DB   FD       */
void fd_1e() { illegal_1(); op_1e();                             } /* DB   FD       */
void fd_1f() { illegal_1(); op_1f();                             } /* DB   FD       */

void fd_20() { illegal_1(); op_20();                             } /* DB   FD       */
void fd_21() { IY = ARG16();                                     } /* LD   IY,w     */
void fd_22() { EA = ARG16(); WM16( EA, &Z80.iy ); WZ = EA+1;     } /* LD   (w),IY   */
void fd_23() { IY++;                                             } /* INC  IY       */
void fd_24() { HY = INC(HY);                                     } /* INC  HY       */
void fd_25() { HY = DEC(HY);                                     } /* DEC  HY       */
void fd_26() { HY = ARG();                                       } /* LD   HY,n     */
void fd_27() { illegal_1(); op_27();                             } /* DB   FD       */

void fd_28() { illegal_1(); op_28();                             } /* DB   FD       */
void fd_29() { ADD16(iy,iy);                                     } /* ADD  IY,IY    */
void fd_2a() { EA = ARG16(); RM16( EA, &Z80.iy ); WZ = EA+1;     } /* LD   IY,(w)   */
void fd_2b() { IY--;                                             } /* DEC  IY       */
void fd_2c() { LY = INC(LY);                                     } /* INC  LY       */
void fd_2d() { LY = DEC(LY);                                     } /* DEC  LY       */
void fd_2e() { LY = ARG();                                       } /* LD   LY,n     */
void fd_2f() { illegal_1(); op_2f();                             } /* DB   FD       */

void fd_30() { illegal_1(); op_30();                             } /* DB   FD       */
void fd_31() { illegal_1(); op_31();                             } /* DB   FD       */
void fd_32() { illegal_1(); op_32();                             } /* DB   FD       */
void fd_33() { illegal_1(); op_33();                             } /* DB   FD       */
void fd_34() { EAY(); WM( EA, INC(RM(EA)) );                       } /* INC  (IY+o)   */
void fd_35() { EAY(); WM( EA, DEC(RM(EA)) );                       } /* DEC  (IY+o)   */
void fd_36() { EAY(); WM( EA, ARG() );                             } /* LD   (IY+o),n */
void fd_37() { illegal_1(); op_37();                             } /* DB   FD       */

void fd_38() { illegal_1(); op_38();                             } /* DB   FD       */
void fd_39() { ADD16(iy,sp);                                     } /* ADD  IY,SP    */
void fd_3a() { illegal_1(); op_3a();                             } /* DB   FD       */
void fd_3b() { illegal_1(); op_3b();                             } /* DB   FD       */
void fd_3c() { illegal_1(); op_3c();                             } /* DB   FD       */
void fd_3d() { illegal_1(); op_3d();                             } /* DB   FD       */
void fd_3e() { illegal_1(); op_3e();                             } /* DB   FD       */
void fd_3f() { illegal_1(); op_3f();                             } /* DB   FD       */

void fd_40() { illegal_1(); op_40();                             } /* DB   FD       */
void fd_41() { illegal_1(); op_41();                             } /* DB   FD       */
void fd_42() { illegal_1(); op_42();                             } /* DB   FD       */
void fd_43() { illegal_1(); op_43();                             } /* DB   FD       */
void fd_44() { B = HY;                                           } /* LD   B,HY     */
void fd_45() { B = LY;                                           } /* LD   B,LY     */
void fd_46() { EAY(); B = RM(EA);                                  } /* LD   B,(IY+o) */
void fd_47() { illegal_1(); op_47();                             } /* DB   FD       */

void fd_48() { illegal_1(); op_48();                             } /* DB   FD       */
void fd_49() { illegal_1(); op_49();                             } /* DB   FD       */
void fd_4a() { illegal_1(); op_4a();                             } /* DB   FD       */
void fd_4b() { illegal_1(); op_4b();                             } /* DB   FD       */
void fd_4c() { C = HY;                                           } /* LD   C,HY     */
void fd_4d() { C = LY;                                           } /* LD   C,LY     */
void fd_4e() { EAY(); C = RM(EA);                                  } /* LD   C,(IY+o) */
void fd_4f() { illegal_1(); op_4f();                             } /* DB   FD       */

void fd_50() { illegal_1(); op_50();                             } /* DB   FD       */
void fd_51() { illegal_1(); op_51();                             } /* DB   FD       */
void fd_52() { illegal_1(); op_52();                             } /* DB   FD       */
void fd_53() { illegal_1(); op_53();                             } /* DB   FD       */
void fd_54() { D = HY;                                           } /* LD   D,HY     */
void fd_55() { D = LY;                                           } /* LD   D,LY     */
void fd_56() { EAY(); D = RM(EA);                                  } /* LD   D,(IY+o) */
void fd_57() { illegal_1(); op_57();                             } /* DB   FD       */

void fd_58() { illegal_1(); op_58();                             } /* DB   FD       */
void fd_59() { illegal_1(); op_59();                             } /* DB   FD       */
void fd_5a() { illegal_1(); op_5a();                             } /* DB   FD       */
void fd_5b() { illegal_1(); op_5b();                             } /* DB   FD       */
void fd_5c() { E = HY;                                           } /* LD   E,HY     */
void fd_5d() { E = LY;                                           } /* LD   E,LY     */
void fd_5e() { EAY(); E = RM(EA);                                  } /* LD   E,(IY+o) */
void fd_5f() { illegal_1(); op_5f();                             } /* DB   FD       */

void fd_60() { HY = B;                                           } /* LD   HY,B     */
void fd_61() { HY = C;                                           } /* LD   HY,C     */
void fd_62() { HY = D;                                           } /* LD   HY,D     */
void fd_63() { HY = E;                                           } /* LD   HY,E     */
void fd_64() {                                                   } /* LD   HY,HY    */
void fd_65() { HY = LY;                                          } /* LD   HY,LY    */
void fd_66() { EAY(); H = RM(EA);                                  } /* LD   H,(IY+o) */
void fd_67() { HY = A;                                           } /* LD   HY,A     */

void fd_68() { LY = B;                                           } /* LD   LY,B     */
void fd_69() { LY = C;                                           } /* LD   LY,C     */
void fd_6a() { LY = D;                                           } /* LD   LY,D     */
void fd_6b() { LY = E;                                           } /* LD   LY,E     */
void fd_6c() { LY = HY;                                          } /* LD   LY,HY    */
void fd_6d() {                                                   } /* LD   LY,LY    */
void fd_6e() { EAY(); L = RM(EA);                                  } /* LD   L,(IY+o) */
void fd_6f() { LY = A;                                           } /* LD   LY,A     */

void fd_70() { EAY(); WM( EA, B );                                 } /* LD   (IY+o),B */
void fd_71() { EAY(); WM( EA, C );                                 } /* LD   (IY+o),C */
void fd_72() { EAY(); WM( EA, D );                                 } /* LD   (IY+o),D */
void fd_73() { EAY(); WM( EA, E );                                 } /* LD   (IY+o),E */
void fd_74() { EAY(); WM( EA, H );                                 } /* LD   (IY+o),H */
void fd_75() { EAY(); WM( EA, L );                                 } /* LD   (IY+o),L */
void fd_76() { illegal_1(); op_76();                             } /* DB   FD       */
void fd_77() { EAY(); WM( EA, A );                                 } /* LD   (IY+o),A */

void fd_78() { illegal_1(); op_78();                             } /* DB   FD       */
void fd_79() { illegal_1(); op_79();                             } /* DB   FD       */
void fd_7a() { illegal_1(); op_7a();                             } /* DB   FD       */
void fd_7b() { illegal_1(); op_7b();                             } /* DB   FD       */
void fd_7c() { A = HY;                                           } /* LD   A,HY     */
void fd_7d() { A = LY;                                           } /* LD   A,LY     */
void fd_7e() { EAY(); A = RM(EA);                                  } /* LD   A,(IY+o) */
void fd_7f() { illegal_1(); op_7f();                             } /* DB   FD       */

void fd_80() { illegal_1(); op_80();                             } /* DB   FD       */
void fd_81() { illegal_1(); op_81();                             } /* DB   FD       */
void fd_82() { illegal_1(); op_82();                             } /* DB   FD       */
void fd_83() { illegal_1(); op_83();                             } /* DB   FD       */
void fd_84() { ADD(HY);                                          } /* ADD  A,HY     */
void fd_85() { ADD(LY);                                          } /* ADD  A,LY     */
void fd_86() { EAY(); ADD(RM(EA));                                 } /* ADD  A,(IY+o) */
void fd_87() { illegal_1(); op_87();                             } /* DB   FD       */

void fd_88() { illegal_1(); op_88();                             } /* DB   FD       */
void fd_89() { illegal_1(); op_89();                             } /* DB   FD       */
void fd_8a() { illegal_1(); op_8a();                             } /* DB   FD       */
void fd_8b() { illegal_1(); op_8b();                             } /* DB   FD       */
void fd_8c() { ADC(HY);                                          } /* ADC  A,HY     */
void fd_8d() { ADC(LY);                                          } /* ADC  A,LY     */
void fd_8e() { EAY(); ADC(RM(EA));                                 } /* ADC  A,(IY+o) */
void fd_8f() { illegal_1(); op_8f();                             } /* DB   FD       */

void fd_90() { illegal_1(); op_90();                             } /* DB   FD       */
void fd_91() { illegal_1(); op_91();                             } /* DB   FD       */
void fd_92() { illegal_1(); op_92();                             } /* DB   FD       */
void fd_93() { illegal_1(); op_93();                             } /* DB   FD       */
void fd_94() { SUB(HY);                                          } /* SUB  HY       */
void fd_95() { SUB(LY);                                          } /* SUB  LY       */
void fd_96() { EAY(); SUB(RM(EA));                                 } /* SUB  (IY+o)   */
void fd_97() { illegal_1(); op_97();                             } /* DB   FD       */

void fd_98() { illegal_1(); op_98();                             } /* DB   FD       */
void fd_99() { illegal_1(); op_99();                             } /* DB   FD       */
void fd_9a() { illegal_1(); op_9a();                             } /* DB   FD       */
void fd_9b() { illegal_1(); op_9b();                             } /* DB   FD       */
void fd_9c() { SBC(HY);                                          } /* SBC  A,HY     */
void fd_9d() { SBC(LY);                                          } /* SBC  A,LY     */
void fd_9e() { EAY(); SBC(RM(EA));                                 } /* SBC  A,(IY+o) */
void fd_9f() { illegal_1(); op_9f();                             } /* DB   FD       */

void fd_a0() { illegal_1(); op_a0();                             } /* DB   FD       */
void fd_a1() { illegal_1(); op_a1();                             } /* DB   FD       */
void fd_a2() { illegal_1(); op_a2();                             } /* DB   FD       */
void fd_a3() { illegal_1(); op_a3();                             } /* DB   FD       */
void fd_a4() { AND(HY);                                          } /* AND  HY       */
void fd_a5() { AND(LY);                                          } /* AND  LY       */
void fd_a6() { EAY(); AND(RM(EA));                                 } /* AND  (IY+o)   */
void fd_a7() { illegal_1(); op_a7();                             } /* DB   FD       */

void fd_a8() { illegal_1(); op_a8();                             } /* DB   FD       */
void fd_a9() { illegal_1(); op_a9();                             } /* DB   FD       */
void fd_aa() { illegal_1(); op_aa();                             } /* DB   FD       */
void fd_ab() { illegal_1(); op_ab();                             } /* DB   FD       */
void fd_ac() { XOR(HY);                                          } /* XOR  HY       */
void fd_ad() { XOR(LY);                                          } /* XOR  LY       */
void fd_ae() { EAY(); XOR(RM(EA));                                 } /* XOR  (IY+o)   */
void fd_af() { illegal_1(); op_af();                             } /* DB   FD       */

void fd_b0() { illegal_1(); op_b0();                             } /* DB   FD       */
void fd_b1() { illegal_1(); op_b1();                             } /* DB   FD       */
void fd_b2() { illegal_1(); op_b2();                             } /* DB   FD       */
void fd_b3() { illegal_1(); op_b3();                             } /* DB   FD       */
void fd_b4() { OR(HY);                                           } /* OR   HY       */
void fd_b5() { OR(LY);                                           } /* OR   LY       */
void fd_b6() { EAY(); OR(RM(EA));                                  } /* OR   (IY+o)   */
void fd_b7() { illegal_1(); op_b7();                             } /* DB   FD       */

void fd_b8() { illegal_1(); op_b8();                             } /* DB   FD       */
void fd_b9() { illegal_1(); op_b9();                             } /* DB   FD       */
void fd_ba() { illegal_1(); op_ba();                             } /* DB   FD       */
void fd_bb() { illegal_1(); op_bb();                             } /* DB   FD       */
void fd_bc() { CP(HY);                                           } /* CP   HY       */
void fd_bd() { CP(LY);                                           } /* CP   LY       */
void fd_be() { EAY(); CP(RM(EA));                                  } /* CP   (IY+o)   */
void fd_bf() { illegal_1(); op_bf();                             } /* DB   FD       */

void fd_c0() { illegal_1(); op_c0();                             } /* DB   FD       */
void fd_c1() { illegal_1(); op_c1();                             } /* DB   FD       */
void fd_c2() { illegal_1(); op_c2();                             } /* DB   FD       */
void fd_c3() { illegal_1(); op_c3();                             } /* DB   FD       */
void fd_c4() { illegal_1(); op_c4();                             } /* DB   FD       */
void fd_c5() { illegal_1(); op_c5();                             } /* DB   FD       */
void fd_c6() { illegal_1(); op_c6();                             } /* DB   FD       */
void fd_c7() { illegal_1(); op_c7();                             } /* DB   FD       */

void fd_c8() { illegal_1(); op_c8();                             } /* DB   FD       */
void fd_c9() { illegal_1(); op_c9();                             } /* DB   FD       */
void fd_ca() { illegal_1(); op_ca();                             } /* DB   FD       */
void fd_cb() { EAY(); EXEC_xycb(ARG());                            } /* **** FD CB xx */
void fd_cc() { illegal_1(); op_cc();                             } /* DB   FD       */
void fd_cd() { illegal_1(); op_cd();                             } /* DB   FD       */
void fd_ce() { illegal_1(); op_ce();                             } /* DB   FD       */
void fd_cf() { illegal_1(); op_cf();                             } /* DB   FD       */

void fd_d0() { illegal_1(); op_d0();                             } /* DB   FD       */
void fd_d1() { illegal_1(); op_d1();                             } /* DB   FD       */
void fd_d2() { illegal_1(); op_d2();                             } /* DB   FD       */
void fd_d3() { illegal_1(); op_d3();                             } /* DB   FD       */
void fd_d4() { illegal_1(); op_d4();                             } /* DB   FD       */
void fd_d5() { illegal_1(); op_d5();                             } /* DB   FD       */
void fd_d6() { illegal_1(); op_d6();                             } /* DB   FD       */
void fd_d7() { illegal_1(); op_d7();                             } /* DB   FD       */

void fd_d8() { illegal_1(); op_d8();                             } /* DB   FD       */
void fd_d9() { illegal_1(); op_d9();                             } /* DB   FD       */
void fd_da() { illegal_1(); op_da();                             } /* DB   FD       */
void fd_db() { illegal_1(); op_db();                             } /* DB   FD       */
void fd_dc() { illegal_1(); op_dc();                             } /* DB   FD       */
void fd_dd() { EXEC_dd(ROP());                                   } /* **** FD DD xx */
void fd_de() { illegal_1(); op_de();                             } /* DB   FD       */
void fd_df() { illegal_1(); op_df();                             } /* DB   FD       */

void fd_e0() { illegal_1(); op_e0();                             } /* DB   FD       */
void fd_e1() { POP_iy();                                        } /* POP  IY       */
void fd_e2() { illegal_1(); op_e2();                             } /* DB   FD       */
void fd_e3() { EXSP_iy();                                       } /* EX   (SP),IY  */
void fd_e4() { illegal_1(); op_e4();                             } /* DB   FD       */
void fd_e5() { PUSH_iy();                                       } /* PUSH IY       */
void fd_e6() { illegal_1(); op_e6();                             } /* DB   FD       */
void fd_e7() { illegal_1(); op_e7();                             } /* DB   FD       */

void fd_e8() { illegal_1(); op_e8();                             } /* DB   FD       */
void fd_e9() { PC = IY;                                          } /* JP   (IY)     */
void fd_ea() { illegal_1(); op_ea();                             } /* DB   FD       */
void fd_eb() { illegal_1(); op_eb();                             } /* DB   FD       */
void fd_ec() { illegal_1(); op_ec();                             } /* DB   FD       */
void fd_ed() { illegal_1(); op_ed();                             } /* DB   FD       */
void fd_ee() { illegal_1(); op_ee();                             } /* DB   FD       */
void fd_ef() { illegal_1(); op_ef();                             } /* DB   FD       */

void fd_f0() { illegal_1(); op_f0();                             } /* DB   FD       */
void fd_f1() { illegal_1(); op_f1();                             } /* DB   FD       */
void fd_f2() { illegal_1(); op_f2();                             } /* DB   FD       */
void fd_f3() { illegal_1(); op_f3();                             } /* DB   FD       */
void fd_f4() { illegal_1(); op_f4();                             } /* DB   FD       */
void fd_f5() { illegal_1(); op_f5();                             } /* DB   FD       */
void fd_f6() { illegal_1(); op_f6();                             } /* DB   FD       */
void fd_f7() { illegal_1(); op_f7();                             } /* DB   FD       */

void fd_f8() { illegal_1(); op_f8();                             } /* DB   FD       */
void fd_f9() { SP = IY;                                          } /* LD   SP,IY    */
void fd_fa() { illegal_1(); op_fa();                             } /* DB   FD       */
void fd_fb() { illegal_1(); op_fb();                             } /* DB   FD       */
void fd_fc() { illegal_1(); op_fc();                             } /* DB   FD       */
void fd_fd() { EXEC_fd(ROP());                                   } /* **** FD FD xx */
void fd_fe() { illegal_1(); op_fe();                             } /* DB   FD       */
void fd_ff() { illegal_1(); op_ff();                             } /* DB   FD       */

void illegal_2()
{
#if VERBOSE
logerror("Z80 #%d ill. opcode $ed $%02x\n",
      cpu_getactivecpu(), cpu_readop((PCD-1)&0xffff));
#endif
}

/**********************************************************
 * special opcodes (ED prefix)
 **********************************************************/
void ed_00() { illegal_2();                                      } /* DB   ED      */
void ed_01() { illegal_2();                                      } /* DB   ED      */
void ed_02() { illegal_2();                                      } /* DB   ED      */
void ed_03() { illegal_2();                                      } /* DB   ED      */
void ed_04() { illegal_2();                                      } /* DB   ED      */
void ed_05() { illegal_2();                                      } /* DB   ED      */
void ed_06() { illegal_2();                                      } /* DB   ED      */
void ed_07() { illegal_2();                                      } /* DB   ED      */

void ed_08() { illegal_2();                                      } /* DB   ED      */
void ed_09() { illegal_2();                                      } /* DB   ED      */
void ed_0a() { illegal_2();                                      } /* DB   ED      */
void ed_0b() { illegal_2();                                      } /* DB   ED      */
void ed_0c() { illegal_2();                                      } /* DB   ED      */
void ed_0d() { illegal_2();                                      } /* DB   ED      */
void ed_0e() { illegal_2();                                      } /* DB   ED      */
void ed_0f() { illegal_2();                                      } /* DB   ED      */

void ed_10() { illegal_2();                                      } /* DB   ED      */
void ed_11() { illegal_2();                                      } /* DB   ED      */
void ed_12() { illegal_2();                                      } /* DB   ED      */
void ed_13() { illegal_2();                                      } /* DB   ED      */
void ed_14() { illegal_2();                                      } /* DB   ED      */
void ed_15() { illegal_2();                                      } /* DB   ED      */
void ed_16() { illegal_2();                                      } /* DB   ED      */
void ed_17() { illegal_2();                                      } /* DB   ED      */

void ed_18() { illegal_2();                                      } /* DB   ED      */
void ed_19() { illegal_2();                                      } /* DB   ED      */
void ed_1a() { illegal_2();                                      } /* DB   ED      */
void ed_1b() { illegal_2();                                      } /* DB   ED      */
void ed_1c() { illegal_2();                                      } /* DB   ED      */
void ed_1d() { illegal_2();                                      } /* DB   ED      */
void ed_1e() { illegal_2();                                      } /* DB   ED      */
void ed_1f() { illegal_2();                                      } /* DB   ED      */

void ed_20() { illegal_2();                                      } /* DB   ED      */
void ed_21() { illegal_2();                                      } /* DB   ED      */
void ed_22() { illegal_2();                                      } /* DB   ED      */
void ed_23() { illegal_2();                                      } /* DB   ED      */
void ed_24() { illegal_2();                                      } /* DB   ED      */
void ed_25() { illegal_2();                                      } /* DB   ED      */
void ed_26() { illegal_2();                                      } /* DB   ED      */
void ed_27() { illegal_2();                                      } /* DB   ED      */

void ed_28() { illegal_2();                                      } /* DB   ED      */
void ed_29() { illegal_2();                                      } /* DB   ED      */
void ed_2a() { illegal_2();                                      } /* DB   ED      */
void ed_2b() { illegal_2();                                      } /* DB   ED      */
void ed_2c() { illegal_2();                                      } /* DB   ED      */
void ed_2d() { illegal_2();                                      } /* DB   ED      */
void ed_2e() { illegal_2();                                      } /* DB   ED      */
void ed_2f() { illegal_2();                                      } /* DB   ED      */

void ed_30() { illegal_2();                                      } /* DB   ED      */
void ed_31() { illegal_2();                                      } /* DB   ED      */
void ed_32() { illegal_2();                                      } /* DB   ED      */
void ed_33() { illegal_2();                                      } /* DB   ED      */
void ed_34() { illegal_2();                                      } /* DB   ED      */
void ed_35() { illegal_2();                                      } /* DB   ED      */
void ed_36() { illegal_2();                                      } /* DB   ED      */
void ed_37() { illegal_2();                                      } /* DB   ED      */

void ed_38() { illegal_2();                                      } /* DB   ED      */
void ed_39() { illegal_2();                                      } /* DB   ED      */
void ed_3a() { illegal_2();                                      } /* DB   ED      */
void ed_3b() { illegal_2();                                      } /* DB   ED      */
void ed_3c() { illegal_2();                                      } /* DB   ED      */
void ed_3d() { illegal_2();                                      } /* DB   ED      */
void ed_3e() { illegal_2();                                      } /* DB   ED      */
void ed_3f() { illegal_2();                                      } /* DB   ED      */

void ed_40() { B = IN(BC); F = (F & CF) | SZP[B];                } /* IN   B,(C)   */
void ed_41() { OUT(BC, B);                                       } /* OUT  (C),B   */
void ed_42() { SBC16( bc );                                      } /* SBC  HL,BC   */
void ed_43() { EA = ARG16(); WM16( EA, &Z80.bc ); WZ = EA+1;     } /* LD   (w),BC  */
void ed_44() { NEG();                                              } /* NEG          */
void ed_45() { RETN();                                             } /* RETN;        */
void ed_46() { IM = 0;                                           } /* IM   0       */
void ed_47() { LD_I_A();                                           } /* LD   I,A     */

void ed_48() { C = IN(BC); F = (F & CF) | SZP[C];                } /* IN   C,(C)   */
void ed_49() { OUT(BC, C);                                       } /* OUT  (C),C   */
void ed_4a() { ADC16( bc );                                      } /* ADC  HL,BC   */
void ed_4b() { EA = ARG16(); RM16( EA, &Z80.bc ); WZ = EA+1;     } /* LD   BC,(w)  */
void ed_4c() { NEG();                                              } /* NEG          */
void ed_4d() { RETI();                                             } /* RETI         */
void ed_4e() { IM = 0;                                           } /* IM   0       */
void ed_4f() { LD_R_A();                                           } /* LD   R,A     */

void ed_50() { D = IN(BC); F = (F & CF) | SZP[D];                } /* IN   D,(C)   */
void ed_51() { OUT(BC, D);                                       } /* OUT  (C),D   */
void ed_52() { SBC16( de );                                      } /* SBC  HL,DE   */
void ed_53() { EA = ARG16(); WM16( EA, &Z80.de ); WZ = EA+1;     } /* LD   (w),DE  */
void ed_54() { NEG();                                              } /* NEG          */
void ed_55() { RETN();                                             } /* RETN;        */
void ed_56() { IM = 1;                                           } /* IM   1       */
void ed_57() { LD_A_I();                                           } /* LD   A,I     */

void ed_58() { E = IN(BC); F = (F & CF) | SZP[E];                } /* IN   E,(C)   */
void ed_59() { OUT(BC, E);                                       } /* OUT  (C),E   */
void ed_5a() { ADC16( de );                                      } /* ADC  HL,DE   */
void ed_5b() { EA = ARG16(); RM16( EA, &Z80.de ); WZ = EA+1;     } /* LD   DE,(w)  */
void ed_5c() { NEG();                                              } /* NEG          */
void ed_5d() { RETI();                                             } /* RETI         */
void ed_5e() { IM = 2;                                           } /* IM   2       */
void ed_5f() { LD_A_R();                                           } /* LD   A,R     */

void ed_60() { H = IN(BC); F = (F & CF) | SZP[H];                } /* IN   H,(C)   */
void ed_61() { OUT(BC, H);                                       } /* OUT  (C),H   */
void ed_62() { SBC16( hl );                                      } /* SBC  HL,HL   */
void ed_63() { EA = ARG16(); WM16( EA, &Z80.hl ); WZ = EA+1;     } /* LD   (w),HL  */
void ed_64() { NEG();                                              } /* NEG          */
void ed_65() { RETN();                                             } /* RETN;        */
void ed_66() { IM = 0;                                           } /* IM   0       */
void ed_67() { RRD();                                              } /* RRD  (HL)    */

void ed_68() { L = IN(BC); F = (F & CF) | SZP[L];                } /* IN   L,(C)   */
void ed_69() { OUT(BC, L);                                       } /* OUT  (C),L   */
void ed_6a() { ADC16( hl );                                      } /* ADC  HL,HL   */
void ed_6b() { EA = ARG16(); RM16( EA, &Z80.hl ); WZ = EA+1;     } /* LD   HL,(w)  */
void ed_6c() { NEG();                                              } /* NEG          */
void ed_6d() { RETI();                                             } /* RETI         */
void ed_6e() { IM = 0;                                           } /* IM   0       */
void ed_6f() { RLD();                                              } /* RLD  (HL)    */

void ed_70() { u8 res = IN(BC); F = (F & CF) | SZP[res];      } /* IN   0,(C)   */
void ed_71() { OUT(BC, 0);                                       } /* OUT  (C),0   */
void ed_72() { SBC16( sp );                                      } /* SBC  HL,SP   */
void ed_73() { EA = ARG16(); WM16( EA, &Z80.sp ); WZ = EA+1;     } /* LD   (w),SP  */
void ed_74() { NEG();                                              } /* NEG          */
void ed_75() { RETN();                                             } /* RETN;        */
void ed_76() { IM = 1;                                           } /* IM   1       */
void ed_77() { illegal_2();                                      } /* DB   ED,77   */

void ed_78() { A = IN(BC); F = (F & CF) | SZP[A]; WZ = BC+1;     } /* IN   E,(C)   */
void ed_79() { OUT(BC, A); WZ = BC + 1;                          } /* OUT  (C),A   */
void ed_7a() { ADC16( sp );                                      } /* ADC  HL,SP   */
void ed_7b() { EA = ARG16(); RM16( EA, &Z80.sp ); WZ = EA+1; } /* LD   SP,(w)  */
void ed_7c() { NEG();                                              } /* NEG          */
void ed_7d() { RETI();                                             } /* RETI         */
void ed_7e() { IM = 2;                                           } /* IM   2       */
void ed_7f() { illegal_2();                                      } /* DB   ED,7F   */

void ed_80() { illegal_2();                                      } /* DB   ED      */
void ed_81() { illegal_2();                                      } /* DB   ED      */
void ed_82() { illegal_2();                                      } /* DB   ED      */
void ed_83() { illegal_2();                                      } /* DB   ED      */
void ed_84() { illegal_2();                                      } /* DB   ED      */
void ed_85() { illegal_2();                                      } /* DB   ED      */
void ed_86() { illegal_2();                                      } /* DB   ED      */
void ed_87() { illegal_2();                                      } /* DB   ED      */

void ed_88() { illegal_2();                                      } /* DB   ED      */
void ed_89() { illegal_2();                                      } /* DB   ED      */
void ed_8a() { illegal_2();                                      } /* DB   ED      */
void ed_8b() { illegal_2();                                      } /* DB   ED      */
void ed_8c() { illegal_2();                                      } /* DB   ED      */
void ed_8d() { illegal_2();                                      } /* DB   ED      */
void ed_8e() { illegal_2();                                      } /* DB   ED      */
void ed_8f() { illegal_2();                                      } /* DB   ED      */

void ed_90() { illegal_2();                                      } /* DB   ED      */
void ed_91() { illegal_2();                                      } /* DB   ED      */
void ed_92() { illegal_2();                                      } /* DB   ED      */
void ed_93() { illegal_2();                                      } /* DB   ED      */
void ed_94() { illegal_2();                                      } /* DB   ED      */
void ed_95() { illegal_2();                                      } /* DB   ED      */
void ed_96() { illegal_2();                                      } /* DB   ED      */
void ed_97() { illegal_2();                                      } /* DB   ED      */

void ed_98() { illegal_2();                                      } /* DB   ED      */
void ed_99() { illegal_2();                                      } /* DB   ED      */
void ed_9a() { illegal_2();                                      } /* DB   ED      */
void ed_9b() { illegal_2();                                      } /* DB   ED      */
void ed_9c() { illegal_2();                                      } /* DB   ED      */
void ed_9d() { illegal_2();                                      } /* DB   ED      */
void ed_9e() { illegal_2();                                      } /* DB   ED      */
void ed_9f() { illegal_2();                                      } /* DB   ED      */

void ed_a0() { LDI();                                              } /* LDI          */
void ed_a1() { CPI();                                              } /* CPI          */
void ed_a2() { INI();                                              } /* INI          */
void ed_a3() { OUTI();                                             } /* OUTI         */
void ed_a4() { illegal_2();                                      } /* DB   ED      */
void ed_a5() { illegal_2();                                      } /* DB   ED      */
void ed_a6() { illegal_2();                                      } /* DB   ED      */
void ed_a7() { illegal_2();                                      } /* DB   ED      */

void ed_a8() { LDD();                                              } /* LDD          */
void ed_a9() { CPD();                                              } /* CPD          */
void ed_aa() { IND();                                              } /* IND          */
void ed_ab() { OUTD();                                             } /* OUTD         */
void ed_ac() { illegal_2();                                      } /* DB   ED      */
void ed_ad() { illegal_2();                                      } /* DB   ED      */
void ed_ae() { illegal_2();                                      } /* DB   ED      */
void ed_af() { illegal_2();                                      } /* DB   ED      */

void ed_b0() { LDIR();                                             } /* LDIR         */
void ed_b1() { CPIR();                                             } /* CPIR         */
void ed_b2() { INIR();                                             } /* INIR         */
void ed_b3() { OTIR();                                             } /* OTIR         */
void ed_b4() { illegal_2();                                      } /* DB   ED      */
void ed_b5() { illegal_2();                                      } /* DB   ED      */
void ed_b6() { illegal_2();                                      } /* DB   ED      */
void ed_b7() { illegal_2();                                      } /* DB   ED      */

void ed_b8() { LDDR();                                             } /* LDDR         */
void ed_b9() { CPDR();                                             } /* CPDR         */
void ed_ba() { INDR();                                             } /* INDR         */
void ed_bb() { OTDR();                                             } /* OTDR         */
void ed_bc() { illegal_2();                                      } /* DB   ED      */
void ed_bd() { illegal_2();                                      } /* DB   ED      */
void ed_be() { illegal_2();                                      } /* DB   ED      */
void ed_bf() { illegal_2();                                      } /* DB   ED      */

void ed_c0() { illegal_2();                                      } /* DB   ED      */
void ed_c1() { illegal_2();                                      } /* DB   ED      */
void ed_c2() { illegal_2();                                      } /* DB   ED      */
void ed_c3() { illegal_2();                                      } /* DB   ED      */
void ed_c4() { illegal_2();                                      } /* DB   ED      */
void ed_c5() { illegal_2();                                      } /* DB   ED      */
void ed_c6() { illegal_2();                                      } /* DB   ED      */
void ed_c7() { illegal_2();                                      } /* DB   ED      */

void ed_c8() { illegal_2();                                      } /* DB   ED      */
void ed_c9() { illegal_2();                                      } /* DB   ED      */
void ed_ca() { illegal_2();                                      } /* DB   ED      */
void ed_cb() { illegal_2();                                      } /* DB   ED      */
void ed_cc() { illegal_2();                                      } /* DB   ED      */
void ed_cd() { illegal_2();                                      } /* DB   ED      */
void ed_ce() { illegal_2();                                      } /* DB   ED      */
void ed_cf() { illegal_2();                                      } /* DB   ED      */

void ed_d0() { illegal_2();                                      } /* DB   ED      */
void ed_d1() { illegal_2();                                      } /* DB   ED      */
void ed_d2() { illegal_2();                                      } /* DB   ED      */
void ed_d3() { illegal_2();                                      } /* DB   ED      */
void ed_d4() { illegal_2();                                      } /* DB   ED      */
void ed_d5() { illegal_2();                                      } /* DB   ED      */
void ed_d6() { illegal_2();                                      } /* DB   ED      */
void ed_d7() { illegal_2();                                      } /* DB   ED      */

void ed_d8() { illegal_2();                                      } /* DB   ED      */
void ed_d9() { illegal_2();                                      } /* DB   ED      */
void ed_da() { illegal_2();                                      } /* DB   ED      */
void ed_db() { illegal_2();                                      } /* DB   ED      */
void ed_dc() { illegal_2();                                      } /* DB   ED      */
void ed_dd() { illegal_2();                                      } /* DB   ED      */
void ed_de() { illegal_2();                                      } /* DB   ED      */
void ed_df() { illegal_2();                                      } /* DB   ED      */

void ed_e0() { illegal_2();                                      } /* DB   ED      */
void ed_e1() { illegal_2();                                      } /* DB   ED      */
void ed_e2() { illegal_2();                                      } /* DB   ED      */
void ed_e3() { illegal_2();                                      } /* DB   ED      */
void ed_e4() { illegal_2();                                      } /* DB   ED      */
void ed_e5() { illegal_2();                                      } /* DB   ED      */
void ed_e6() { illegal_2();                                      } /* DB   ED      */
void ed_e7() { illegal_2();                                      } /* DB   ED      */

void ed_e8() { illegal_2();                                      } /* DB   ED      */
void ed_e9() { illegal_2();                                      } /* DB   ED      */
void ed_ea() { illegal_2();                                      } /* DB   ED      */
void ed_eb() { illegal_2();                                      } /* DB   ED      */
void ed_ec() { illegal_2();                                      } /* DB   ED      */
void ed_ed() { illegal_2();                                      } /* DB   ED      */
void ed_ee() { illegal_2();                                      } /* DB   ED      */
void ed_ef() { illegal_2();                                      } /* DB   ED      */

void ed_f0() { illegal_2();                                      } /* DB   ED      */
void ed_f1() { illegal_2();                                      } /* DB   ED      */
void ed_f2() { illegal_2();                                      } /* DB   ED      */
void ed_f3() { illegal_2();                                      } /* DB   ED      */
void ed_f4() { illegal_2();                                      } /* DB   ED      */
void ed_f5() { illegal_2();                                      } /* DB   ED      */
void ed_f6() { illegal_2();                                      } /* DB   ED      */
void ed_f7() { illegal_2();                                      } /* DB   ED      */

void ed_f8() { illegal_2();                                      } /* DB   ED      */
void ed_f9() { illegal_2();                                      } /* DB   ED      */
void ed_fa() { illegal_2();                                      } /* DB   ED      */
void ed_fb() { illegal_2();                                      } /* DB   ED      */
void ed_fc() { illegal_2();                                      } /* DB   ED      */
void ed_fd() { illegal_2();                                      } /* DB   ED      */
void ed_fe() { illegal_2();                                      } /* DB   ED      */
void ed_ff() { illegal_2();                                      } /* DB   ED      */


/**********************************************************
 * main opcodes
 **********************************************************/
void op_00() {                                                                                                } /* NOP              */
void op_01() { BC = ARG16();                                                                                  } /* LD   BC,w        */
void op_02() { WM( BC, A ); WZ_L = (BC + 1) & 0xFF;  WZ_H = A;                                                } /* LD   (BC),A      */
void op_03() { BC++;                                                                                          } /* INC  BC          */
void op_04() { B = INC(B);                                                                                    } /* INC  B           */
void op_05() { B = DEC(B);                                                                                    } /* DEC  B           */
void op_06() { B = ARG();                                                                                     } /* LD   B,n         */
void op_07() { RLCA();                                                                                          } /* RLCA             */

void op_08() { EX_AF();                                                                                         } /* EX   AF,AF'      */
void op_09() { ADD16(hl, bc);                                                                                 } /* ADD  HL,BC       */
void op_0a() { A = RM( BC ); WZ=BC+1;                                                                         } /* LD   A,(BC)      */
void op_0b() { BC--;                                                                                          } /* DEC  BC          */
void op_0c() { C = INC(C);                                                                                    } /* INC  C           */
void op_0d() { C = DEC(C);                                                                                    } /* DEC  C           */
void op_0e() { C = ARG();                                                                                     } /* LD   C,n         */
void op_0f() { RRCA();                                                                                          } /* RRCA             */

void op_10() { B--; JR_COND( B, 0x10 );                                                                       } /* DJNZ o           */
void op_11() { DE = ARG16();                                                                                  } /* LD   DE,w        */
void op_12() { WM( DE, A ); WZ_L = (DE + 1) & 0xFF;  WZ_H = A;                                                } /* LD   (DE),A      */
void op_13() { DE++;                                                                                          } /* INC  DE          */
void op_14() { D = INC(D);                                                                                    } /* INC  D           */
void op_15() { D = DEC(D);                                                                                    } /* DEC  D           */
void op_16() { D = ARG();                                                                                     } /* LD   D,n         */
void op_17() { RLA();                                                                                           } /* RLA              */

void op_18() { JR();                                                                                          } /* JR   o           */
void op_19() { ADD16(hl, de);                                                                                 } /* ADD  HL,DE       */
void op_1a() { A = RM( DE ); WZ=DE+1;                                                                         } /* LD   A,(DE)      */
void op_1b() { DE--;                                                                                          } /* DEC  DE          */
void op_1c() { E = INC(E);                                                                                    } /* INC  E           */
void op_1d() { E = DEC(E);                                                                                    } /* DEC  E           */
void op_1e() { E = ARG();                                                                                     } /* LD   E,n         */
void op_1f() { RRA();                                                                                           } /* RRA              */

void op_20() { JR_COND( !(F & ZF), 0x20 );                                                                    } /* JR   NZ,o        */
void op_21() { HL = ARG16();                                                                                  } /* LD   HL,w        */
void op_22() { EA = ARG16(); WM16( EA, &Z80.hl ); WZ = EA+1;                                                  } /* LD   (w),HL      */
void op_23() { HL++;                                                                                          } /* INC  HL          */
void op_24() { H = INC(H);                                                                                    } /* INC  H           */
void op_25() { H = DEC(H);                                                                                    } /* DEC  H           */
void op_26() { H = ARG();                                                                                     } /* LD   H,n         */
void op_27() { DAA();                                                                                           } /* DAA              */

void op_28() { JR_COND( F & ZF, 0x28 );                                                                       } /* JR   Z,o         */
void op_29() { ADD16(hl, hl);                                                                                 } /* ADD  HL,HL       */
void op_2a() { EA = ARG16(); RM16( EA, &Z80.hl ); WZ = EA+1;                                                  } /* LD   HL,(w)      */
void op_2b() { HL--;                                                                                          } /* DEC  HL          */
void op_2c() { L = INC(L);                                                                                    } /* INC  L           */
void op_2d() { L = DEC(L);                                                                                    } /* DEC  L           */
void op_2e() { L = ARG();                                                                                     } /* LD   L,n         */
void op_2f() { A ^= 0xff; F = (F&(SF|ZF|PF|CF))|HF|NF|(A&(YF|XF));                                            } /* CPL              */

void op_30() { JR_COND( !(F & CF), 0x30 );                                                                    } /* JR   NC,o        */
void op_31() { SP = ARG16();                                                                                  } /* LD   SP,w        */
void op_32() { EA = ARG16(); WM( EA, A ); WZ_L=(EA+1)&0xFF;WZ_H=A;                                            } /* LD   (w),A       */
void op_33() { SP++;                                                                                          } /* INC  SP          */
void op_34() { WM( HL, INC(RM(HL)) );                                                                         } /* INC  (HL)        */
void op_35() { WM( HL, DEC(RM(HL)) );                                                                         } /* DEC  (HL)        */
void op_36() { WM( HL, ARG() );                                                                               } /* LD   (HL),n      */
void op_37() { F = (F & (SF|ZF|YF|XF|PF)) | CF | (A & (YF|XF));                                               } /* SCF              */

void op_38() { JR_COND( F & CF, 0x38 );                                                                       } /* JR   C,o         */
void op_39() { ADD16(hl, sp);                                                                                 } /* ADD  HL,SP       */
void op_3a() { EA = ARG16(); A = RM( EA ); WZ = EA+1;                                                         } /* LD   A,(w)       */
void op_3b() { SP--;                                                                                          } /* DEC  SP          */
void op_3c() { A = INC(A);                                                                                    } /* INC  A           */
void op_3d() { A = DEC(A);                                                                                    } /* DEC  A           */
void op_3e() { A = ARG();                                                                                     } /* LD   A,n         */
void op_3f() { F = ((F&(SF|ZF|YF|XF|PF|CF))|((F&CF)<<4)|(A&(YF|XF)))^CF;                                      } /* CCF              */

void op_40() {                                                                                                } /* LD   B,B         */
void op_41() { B = C;                                                                                         } /* LD   B,C         */
void op_42() { B = D;                                                                                         } /* LD   B,D         */
void op_43() { B = E;                                                                                         } /* LD   B,E         */
void op_44() { B = H;                                                                                         } /* LD   B,H         */
void op_45() { B = L;                                                                                         } /* LD   B,L         */
void op_46() { B = RM(HL);                                                                                    } /* LD   B,(HL)      */
void op_47() { B = A;                                                                                         } /* LD   B,A         */

void op_48() { C = B;                                                                                         } /* LD   C,B         */
void op_49() {                                                                                                } /* LD   C,C         */
void op_4a() { C = D;                                                                                         } /* LD   C,D         */
void op_4b() { C = E;                                                                                         } /* LD   C,E         */
void op_4c() { C = H;                                                                                         } /* LD   C,H         */
void op_4d() { C = L;                                                                                         } /* LD   C,L         */
void op_4e() { C = RM(HL);                                                                                    } /* LD   C,(HL)      */
void op_4f() { C = A;                                                                                         } /* LD   C,A         */

void op_50() { D = B;                                                                                         } /* LD   D,B         */
void op_51() { D = C;                                                                                         } /* LD   D,C         */
void op_52() {                                                                                                } /* LD   D,D         */
void op_53() { D = E;                                                                                         } /* LD   D,E         */
void op_54() { D = H;                                                                                         } /* LD   D,H         */
void op_55() { D = L;                                                                                         } /* LD   D,L         */
void op_56() { D = RM(HL);                                                                                    } /* LD   D,(HL)      */
void op_57() { D = A;                                                                                         } /* LD   D,A         */

void op_58() { E = B;                                                                                         } /* LD   E,B         */
void op_59() { E = C;                                                                                         } /* LD   E,C         */
void op_5a() { E = D;                                                                                         } /* LD   E,D         */
void op_5b() {                                                                                                } /* LD   E,E         */
void op_5c() { E = H;                                                                                         } /* LD   E,H         */
void op_5d() { E = L;                                                                                         } /* LD   E,L         */
void op_5e() { E = RM(HL);                                                                                    } /* LD   E,(HL)      */
void op_5f() { E = A;                                                                                         } /* LD   E,A         */

void op_60() { H = B;                                                                                         } /* LD   H,B         */
void op_61() { H = C;                                                                                         } /* LD   H,C         */
void op_62() { H = D;                                                                                         } /* LD   H,D         */
void op_63() { H = E;                                                                                         } /* LD   H,E         */
void op_64() {                                                                                                } /* LD   H,H         */
void op_65() { H = L;                                                                                         } /* LD   H,L         */
void op_66() { H = RM(HL);                                                                                    } /* LD   H,(HL)      */
void op_67() { H = A;                                                                                         } /* LD   H,A         */

void op_68() { L = B;                                                                                         } /* LD   L,B         */
void op_69() { L = C;                                                                                         } /* LD   L,C         */
void op_6a() { L = D;                                                                                         } /* LD   L,D         */
void op_6b() { L = E;                                                                                         } /* LD   L,E         */
void op_6c() { L = H;                                                                                         } /* LD   L,H         */
void op_6d() {                                                                                                } /* LD   L,L         */
void op_6e() { L = RM(HL);                                                                                    } /* LD   L,(HL)      */
void op_6f() { L = A;                                                                                         } /* LD   L,A         */

void op_70() { WM( HL, B );                                                                                   } /* LD   (HL),B      */
void op_71() { WM( HL, C );                                                                                   } /* LD   (HL),C      */
void op_72() { WM( HL, D );                                                                                   } /* LD   (HL),D      */
void op_73() { WM( HL, E );                                                                                   } /* LD   (HL),E      */
void op_74() { WM( HL, H );                                                                                   } /* LD   (HL),H      */
void op_75() { WM( HL, L );                                                                                   } /* LD   (HL),L      */
void op_76() { ENTER_HALT();                                                                                    } /* HALT             */
void op_77() { WM( HL, A );                                                                                   } /* LD   (HL),A      */

void op_78() { A = B;                                                                                         } /* LD   A,B         */
void op_79() { A = C;                                                                                         } /* LD   A,C         */
void op_7a() { A = D;                                                                                         } /* LD   A,D         */
void op_7b() { A = E;                                                                                         } /* LD   A,E         */
void op_7c() { A = H;                                                                                         } /* LD   A,H         */
void op_7d() { A = L;                                                                                         } /* LD   A,L         */
void op_7e() { A = RM(HL);                                                                                    } /* LD   A,(HL)      */
void op_7f() {                                                                                                } /* LD   A,A         */

void op_80() { ADD(B);                                                                                        } /* ADD  A,B         */
void op_81() { ADD(C);                                                                                        } /* ADD  A,C         */
void op_82() { ADD(D);                                                                                        } /* ADD  A,D         */
void op_83() { ADD(E);                                                                                        } /* ADD  A,E         */
void op_84() { ADD(H);                                                                                        } /* ADD  A,H         */
void op_85() { ADD(L);                                                                                        } /* ADD  A,L         */
void op_86() { ADD(RM(HL));                                                                                   } /* ADD  A,(HL)      */
void op_87() { ADD(A);                                                                                        } /* ADD  A,A         */

void op_88() { ADC(B);                                                                                        } /* ADC  A,B         */
void op_89() { ADC(C);                                                                                        } /* ADC  A,C         */
void op_8a() { ADC(D);                                                                                        } /* ADC  A,D         */
void op_8b() { ADC(E);                                                                                        } /* ADC  A,E         */
void op_8c() { ADC(H);                                                                                        } /* ADC  A,H         */
void op_8d() { ADC(L);                                                                                        } /* ADC  A,L         */
void op_8e() { ADC(RM(HL));                                                                                   } /* ADC  A,(HL)      */
void op_8f() { ADC(A);                                                                                        } /* ADC  A,A         */

void op_90() { SUB(B);                                                                                        } /* SUB  B           */
void op_91() { SUB(C);                                                                                        } /* SUB  C           */
void op_92() { SUB(D);                                                                                        } /* SUB  D           */
void op_93() { SUB(E);                                                                                        } /* SUB  E           */
void op_94() { SUB(H);                                                                                        } /* SUB  H           */
void op_95() { SUB(L);                                                                                        } /* SUB  L           */
void op_96() { SUB(RM(HL));                                                                                   } /* SUB  (HL)        */
void op_97() { SUB(A);                                                                                        } /* SUB  A           */

void op_98() { SBC(B);                                                                                        } /* SBC  A,B         */
void op_99() { SBC(C);                                                                                        } /* SBC  A,C         */
void op_9a() { SBC(D);                                                                                        } /* SBC  A,D         */
void op_9b() { SBC(E);                                                                                        } /* SBC  A,E         */
void op_9c() { SBC(H);                                                                                        } /* SBC  A,H         */
void op_9d() { SBC(L);                                                                                        } /* SBC  A,L         */
void op_9e() { SBC(RM(HL));                                                                                   } /* SBC  A,(HL)      */
void op_9f() { SBC(A);                                                                                        } /* SBC  A,A         */

void op_a0() { AND(B);                                                                                        } /* AND  B           */
void op_a1() { AND(C);                                                                                        } /* AND  C           */
void op_a2() { AND(D);                                                                                        } /* AND  D           */
void op_a3() { AND(E);                                                                                        } /* AND  E           */
void op_a4() { AND(H);                                                                                        } /* AND  H           */
void op_a5() { AND(L);                                                                                        } /* AND  L           */
void op_a6() { AND(RM(HL));                                                                                   } /* AND  (HL)        */
void op_a7() { AND(A);                                                                                        } /* AND  A           */

void op_a8() { XOR(B);                                                                                        } /* XOR  B           */
void op_a9() { XOR(C);                                                                                        } /* XOR  C           */
void op_aa() { XOR(D);                                                                                        } /* XOR  D           */
void op_ab() { XOR(E);                                                                                        } /* XOR  E           */
void op_ac() { XOR(H);                                                                                        } /* XOR  H           */
void op_ad() { XOR(L);                                                                                        } /* XOR  L           */
void op_ae() { XOR(RM(HL));                                                                                   } /* XOR  (HL)        */
void op_af() { XOR(A);                                                                                        } /* XOR  A           */

void op_b0() { OR(B);                                                                                         } /* OR   B           */
void op_b1() { OR(C);                                                                                         } /* OR   C           */
void op_b2() { OR(D);                                                                                         } /* OR   D           */
void op_b3() { OR(E);                                                                                         } /* OR   E           */
void op_b4() { OR(H);                                                                                         } /* OR   H           */
void op_b5() { OR(L);                                                                                         } /* OR   L           */
void op_b6() { OR(RM(HL));                                                                                    } /* OR   (HL)        */
void op_b7() { OR(A);                                                                                         } /* OR   A           */

void op_b8() { CP(B);                                                                                         } /* CP   B           */
void op_b9() { CP(C);                                                                                         } /* CP   C           */
void op_ba() { CP(D);                                                                                         } /* CP   D           */
void op_bb() { CP(E);                                                                                         } /* CP   E           */
void op_bc() { CP(H);                                                                                         } /* CP   H           */
void op_bd() { CP(L);                                                                                         } /* CP   L           */
void op_be() { CP(RM(HL));                                                                                    } /* CP   (HL)        */
void op_bf() { CP(A);                                                                                         } /* CP   A           */

void op_c0() { RET_COND( !(F & ZF), 0xc0 );                                                                   } /* RET  NZ          */
void op_c1() { POP_bc();                                                                                     } /* POP  BC          */
void op_c2() { JP_COND( !(F & ZF) );                                                                          } /* JP   NZ,a        */
void op_c3() { JP();                                                                                            } /* JP   a           */
void op_c4() { CALL_COND( !(F & ZF), 0xc4 );                                                                  } /* CALL NZ,a        */
void op_c5() { PUSH_bc();                                                                                    } /* PUSH BC          */
void op_c6() { ADD(ARG());                                                                                    } /* ADD  A,n         */
void op_c7() { RST(0x00);                                                                                     } /* RST  0           */

void op_c8() { RET_COND( F & ZF, 0xc8 );                                                                      } /* RET  Z           */
void op_c9() { POP_pc(); WZ=PCD;                                                                             } /* RET              */
void op_ca() { JP_COND( F & ZF );                                                                             } /* JP   Z,a         */
void op_cb() { R++; EXEC_cb(ROP());                                                                           } /* **** CB xx       */
void op_cc() { CALL_COND( F & ZF, 0xcc );                                                                     } /* CALL Z,a         */
void op_cd() { CALL();                                                                                        } /* CALL a           */
void op_ce() { ADC(ARG());                                                                                    } /* ADC  A,n         */
void op_cf() { RST(0x08);                                                                                     } /* RST  1           */

void op_d0() { RET_COND( !(F & CF), 0xd0 );                                                                   } /* RET  NC          */
void op_d1() { POP_de();                                                                                     } /* POP  DE          */
void op_d2() { JP_COND( !(F & CF) );                                                                          } /* JP   NC,a        */
void op_d3() { u32 n = ARG() | (A << 8); OUT( n, A ); WZ_L = ((n & 0xff) + 1) & 0xff;  WZ_H = A; } /* OUT  (n),A       */
void op_d4() { CALL_COND( !(F & CF), 0xd4 );                                                                  } /* CALL NC,a        */
void op_d5() { PUSH_de();                                                                                    } /* PUSH DE          */
void op_d6() { SUB(ARG());                                                                                    } /* SUB  n           */
void op_d7() { RST(0x10);                                                                                     } /* RST  2           */

void op_d8() { RET_COND( F & CF, 0xd8 );                                                                      } /* RET  C           */
void op_d9() { EXX();                                                                                           } /* EXX              */
void op_da() { JP_COND( F & CF );                                                                             } /* JP   C,a         */
void op_db() { u32 n = ARG() | (A << 8); A = IN( n ); WZ = n + 1;                                        } /* IN   A,(n)       */
void op_dc() { CALL_COND( F & CF, 0xdc );                                                                     } /* CALL C,a         */
void op_dd() { R++; EXEC_dd(ROP());                                                                           } /* **** DD xx       */
void op_de() { SBC(ARG());                                                                                    } /* SBC  A,n         */
void op_df() { RST(0x18);                                                                                     } /* RST  3           */

void op_e0() { RET_COND( !(F & PF), 0xe0 );                                                                   } /* RET  PO          */
void op_e1() { POP_hl();                                                                                     } /* POP  HL          */
void op_e2() { JP_COND( !(F & PF) );                                                                          } /* JP   PO,a        */
void op_e3() { EXSP_hl();                                                                                    } /* EX   HL,(SP)     */
void op_e4() { CALL_COND( !(F & PF), 0xe4 );                                                                  } /* CALL PO,a        */
void op_e5() { PUSH_hl();                                                                                    } /* PUSH HL          */
void op_e6() { AND(ARG());                                                                                    } /* AND  n           */
void op_e7() { RST(0x20);                                                                                     } /* RST  4           */

void op_e8() { RET_COND( F & PF, 0xe8 );                                                                      } /* RET  PE          */
void op_e9() { PC = HL;                                                                                       } /* JP   (HL)        */
void op_ea() { JP_COND( F & PF );                                                                             } /* JP   PE,a        */
void op_eb() { EX_DE_HL();                                                                                      } /* EX   DE,HL       */
void op_ec() { CALL_COND( F & PF, 0xec );                                                                     } /* CALL PE,a        */
void op_ed() { R++; EXEC_ed(ROP());                                                                           } /* **** ED xx       */
void op_ee() { XOR(ARG());                                                                                    } /* XOR  n           */
void op_ef() { RST(0x28);                                                                                     } /* RST  5           */

void op_f0() { RET_COND( !(F & SF), 0xf0 );                                                                   } /* RET  P           */
void op_f1() { POP_af();                                                                                     } /* POP  AF          */
void op_f2() { JP_COND( !(F & SF) );                                                                          } /* JP   P,a         */
void op_f3() { IFF1 = IFF2 = 0;                                                                               } /* DI               */
void op_f4() { CALL_COND( !(F & SF), 0xf4 );                                                                  } /* CALL P,a         */
void op_f5() { PUSH_af();                                                                                    } /* PUSH AF          */
void op_f6() { OR(ARG());                                                                                     } /* OR   n           */
void op_f7() { RST(0x30);                                                                                     } /* RST  6           */

void op_f8() { RET_COND( F & SF, 0xf8 );                                                                      } /* RET  M           */
void op_f9() { SP = HL;                                                                                       } /* LD   SP,HL       */
void op_fa() { JP_COND(F & SF);                                                                               } /* JP   M,a         */
void op_fb() { EI();                                                                                            } /* EI               */
void op_fc() { CALL_COND( F & SF, 0xfc );                                                                     } /* CALL M,a         */
void op_fd() { R++; EXEC_fd(ROP());                                                                           } /* **** FD xx       */
void op_fe() { CP(ARG());                                                                                     } /* CP   n           */
void op_ff() { RST(0x38);                                                                                     } /* RST  7           */


static void take_interrupt()
{
  /* Check if processor was halted */
  LEAVE_HALT();

  /* Clear both interrupt flip flops */
  IFF1 = IFF2 = 0;

  LOG(("Z80 #%d single int. irq_vector $%02x\n", cpu_getactivecpu(), irq_vector));

  /* Interrupt mode 1. RST 38h */
  if( IM == 1 )
  {
    LOG(("Z80 #%d IM1 $0038\n",cpu_getactivecpu() ));
    PUSH_pc();
    PCD = 0x0038;
    /* RST $38 + 'interrupt latency' cycles */
    Z80.cycles += cc[Z80_TABLE_op][0xff] + cc[Z80_TABLE_ex][0xff];
  }
  else
  {
    /* call back the cpu interface to retrieve the vector */
    s32 irq_vector = (*Z80.irq_callback)();

    /* Interrupt mode 2. Call [Z80.i:databyte] */
    if( IM == 2 )
    {
      irq_vector = (irq_vector & 0xff) | (I << 8);
      PUSH_pc();
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
        PUSH_pc();
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
        PUSH_pc();
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
void z80_init(const void *config, s32 (*irqcallback)(s32))
{
  s32 i, p;

  s32 oldval, newval, val;
  u8 *padd = &SZHVC_add[  0*256];
  u8 *padc = &SZHVC_add[256*256];
  u8 *psub = &SZHVC_sub[  0*256];
  u8 *psbc = &SZHVC_sub[256*256];
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
void z80_reset()
{
  PC = 0x0000;
  I = 0;
  R = 0;
  R2 = 0;
  IM = 0;
  IFF1 = IFF2 = 0;
  HALT = 0;

  Z80.after_ei = false;

  WZ=PCD;
}

/****************************************************************************
 * Run until given cycle count 
 ****************************************************************************/
void z80_run(u32 cycles)
{
  while( Z80.cycles < cycles )
  {
    /* check for IRQs before each instruction */
    if (Z80.irq_state && IFF1 && !Z80.after_ei)
    {
      take_interrupt();
      if (Z80.cycles >= cycles) return;
    }

    Z80.after_ei = false;
    R++;
    EXEC_op(ROP());
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
void z80_set_irq_line(u32 state)
{
  Z80.irq_state = state;
}

void z80_set_nmi_line(u32 state)
{
  /* mark an NMI pending on the rising edge */
  if (Z80.nmi_state == CLEAR_LINE && state != CLEAR_LINE)
  {
    LOG(("Z80 #%d take NMI\n", cpu_getactivecpu()));
    LEAVE_HALT();      /* Check if processor was halted */

    IFF1 = 0;
    PUSH_pc();
    PCD = 0x0066;
    WZ=PCD;

    Z80.cycles += 11*15;
  }

  Z80.nmi_state = state;
}

