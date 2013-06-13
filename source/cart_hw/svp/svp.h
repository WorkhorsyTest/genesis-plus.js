/*
   basic, incomplete SSP160x (SSP1601?) interpreter
   with SVP memory controller emu

   (c) Copyright 2008, Grazvydas "notaz" Ignotas
   Free for non-commercial use.

   For commercial use, separate licencing terms must be obtained.

   Modified for Genesis Plus GX (Eke-Eke): added BIG ENDIAN support, fixed addr/code inversion
*/

#ifndef _SVP_H_
#define _SVP_H_

#include "shared.h"
#include "ssp16.h"

typedef struct {
  u8 iram_rom[0x20000]; /* IRAM (0-0x7ff) and program ROM (0x800-0x1ffff) */
  u8 dram[0x20000];
  ssp1601_t ssp1601;
} svp_t;

extern svp_t *svp;

extern void svp_init();
extern void svp_reset();
extern void svp_write_dram(u32 address, u32 data);
extern u32 svp_read_cell_1(u32 address);
extern u32 svp_read_cell_2(u32 address);

#endif
