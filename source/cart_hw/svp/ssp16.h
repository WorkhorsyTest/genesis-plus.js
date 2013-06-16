/*
   basic, incomplete SSP160x (SSP1601?) interpreter
   with SVP memory controller emu

   (c) Copyright 2008, Grazvydas "notaz" Ignotas
   Free for non-commercial use.

   For commercial use, separate licencing terms must be obtained.

   Modified for Genesis Plus GX (Eke-Eke): added BIG ENDIAN support, fixed addr/code inversion
*/

#ifndef _SSP16_H_
#define _SSP16_H_

/* emulation event logging (from Picodrive) */
#ifdef LOG_SVP
#define EL_SVP     0x00004000 /* SVP stuff  */
#define EL_ANOMALY 0x80000000 /* some unexpected conditions (during emulation) */
#define elprintf(w,f,...) error("%d(%d): " f "\n",frame_count,v_counter,##__VA_ARGS__);
#endif

u32 REG_READ(u32 r);
void REG_WRITE(u32 r, u32 d);

/* register names */
enum {
  SSP_GR0, SSP_X,     SSP_Y,   SSP_A,
  SSP_ST,  SSP_STACK, SSP_PC,  SSP_P,
  SSP_PM0, SSP_PM1,   SSP_PM2, SSP_XST,
  SSP_PM4, SSP_gr13,  SSP_PMC, SSP_AL
};

typedef union
{
  u32 v;
  struct {
#ifdef LSB_FIRST
  u16 l;
  u16 h;
#else
  u16 h;
  u16 l;
#endif
  } byte;
} ssp_reg_t;

typedef struct
{
  union {
    u16 RAM[256*2];  /* 2 internal RAM banks */
    struct {
      u16 RAM0[256];
      u16 RAM1[256];
    } bank;
  } mem;
  ssp_reg_t gr[16];  /* general registers */
  union {
    u8 r[8];  /* BANK pointers */
    struct {
      u8 r0[4];
      u8 r1[4];
    } bank;
  } ptr;
  u16 stack[6];
  u32 pmac[2][6];  /* read/write modes/addrs for PM0-PM5 */
  #define SSP_PMC_HAVE_ADDR  0x0001 /* address written to PMAC, waiting for mode */
  #define SSP_PMC_SET        0x0002 /* PMAC is set */
  #define SSP_HANG           0x1000 /* 68000 hangs SVP */
  #define SSP_WAIT_PM0       0x2000 /* bit1 in PM0 */
  #define SSP_WAIT_30FE06    0x4000 /* ssp tight loops on 30FE08 to become non-zero */
  #define SSP_WAIT_30FE08    0x8000 /* same for 30FE06 */
  #define SSP_WAIT_MASK      0xf000
  u32 emu_status;
  u32 pad[30];
} ssp1601_t;


void ssp1601_reset(ssp1601_t *ssp);
void ssp1601_run(int cycles);

#endif
