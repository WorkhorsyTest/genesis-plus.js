/*
   basic, incomplete SSP160x (SSP1601?) interpreter
   with SVP memory controller emu

   (c) Copyright 2008, Grazvydas "notaz" Ignotas
   Free for non-commercial use.

   For commercial use, separate licencing terms must be obtained.

   Modified for Genesis Plus GX (Eke-Eke): added BIG ENDIAN support, fixed addr/code inversion
*/

import common;
import ssp16;

struct svp_t {
  u8[0x20000] iram_rom; /* IRAM (0-0x7ff) and program ROM (0x800-0x1ffff) */
  u8[0x20000] dram;
  ssp1601_t ssp1601;
}


svp_t* svp = null;

void svp_init()
{
  svp = cast(void *) (cast(char *)cart.rom + 0x200000);
  memset(svp, 0, sizeof(*svp));
}

void svp_reset()
{
  memcpy(svp.iram_rom + 0x800, cart.rom + 0x800, 0x20000 - 0x800);
  ssp1601_reset(&svp.ssp1601);
}

void svp_write_dram(u32 address, u32 data)
{
  *cast(u16 *)(svp.dram + (address & 0x1fffe)) = data;
  if ((address == 0x30fe06) && data) svp.ssp1601.emu_status &= ~SSP_WAIT_30FE06;
  if ((address == 0x30fe08) && data) svp.ssp1601.emu_status &= ~SSP_WAIT_30FE08;
}

u32 svp_read_cell_1(u32 address)
{
  address >>= 1;
  address = (address & 0x7001) | ((address & 0x3e) << 6) | ((address & 0xfc0) >> 5);
  return *cast(u16 *)(svp.dram + (address & 0x1fffe));
}

u32 svp_read_cell_2(u32 address)
{
  address >>= 1;
  address = (address & 0x7801) | ((address & 0x1e) << 6) | ((address & 0x7e0) >> 4);
  return *cast(u16 *)(svp.dram + (address & 0x1fffe));
}

