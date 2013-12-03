/***************************************************************************************
 *  Genesis Plus
 *  Mega CD / Sega CD hardware
 *
 *  Copyright (C) 2012-2013  Eke-Eke (Genesis Plus GX)
 *
 *  Redistribution and use of this code or any derivative works are permitted
 *  provided that the following conditions are met:
 *
 *   - Redistributions may not be sold, nor may they be used in a commercial
 *     product or activity.
 *
 *   - Redistributions that are modified from the original source must include the
 *     complete source code, including the source code for all components used by a
 *     binary built from the modified sources. However, as a special exception, the
 *     source code distributed need not include anything that is normally distributed
 *     (in either source or binary form) with the major components (compiler, kernel,
 *     and so on) of the operating system on which the executable runs, unless that
 *     component itself accompanies the executable.
 *
 *   - Redistributions must reproduce the above copyright notice, this list of
 *     conditions and the following disclaimer in the documentation and/or other
 *     materials provided with the distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************************/

import common;
import module_cdd;
import module_cdc;
import module_gfx;
import module_pcm;
import cd_cart;

ref cd_hw_t scd() { return ext.cd_hw; }

/* 5000000 SCD clocks/s = ~3184 clocks/line with a Master Clock of 53.693175 MHz */
/* This would be slightly (~30 clocks) more on PAL systems because of the slower */
/* Master Clock (53.203424 MHz) but not enough to really care about since clocks */
/* are not running in sync anyway. */
const int SCD_CLOCK = 50000000;
const int SCYCLES_PER_LINE = 3184;

/* Timer & Stopwatch clocks divider */
const int TIMERS_SCYCLES_RATIO = 384 * 4;

/* CD hardware */
struct cd_hw_t
{
  cd_cart_t cartridge;        /* ROM/RAM Cartridge */
  u8[0x20000] bootrom;     /* 128K internal BOOT ROM */
  u8[0x80000] prg_ram;     /* 512K PRG-RAM */
  u8[2][0x20000] word_ram; /* 2 x 128K Word RAM (1M mode) */
  u8[0x40000] word_ram_2M; /* 256K Word RAM (2M mode) */
  u8[0x2000] bram;         /* 8K Backup RAM */
  reg16_t[0x100] regs;        /* 256 x 16-bit ASIC registers */
  u32 cycles;              /* Master clock counter */
  s32 stopwatch;            /* Stopwatch counter */
  s32 timer;                /* Timer counter */
  u8 pending;              /* Pending interrupts */
  u8 dmna;                 /* Pending DMNA write status */
  gfx_t gfx_hw;               /* Graphics processor */
  cdc_t cdc_hw;               /* CD data controller */
  cdd_t cdd_hw;               /* CD drive processor */
  pcm_t pcm_hw;               /* PCM chip */
}

/*--------------------------------------------------------------------------*/
/* Unused area (return open bus data, i.e prefetched instruction word)      */
/*--------------------------------------------------------------------------*/
static u32 s68k_read_bus_8(u32 address)
{
version(LOGERROR) {
  error("[SUB 68k] Unused read8 %08X (%08X)\n", address, s68k.pc);
}
  address = s68k.pc | (address & 1);
  return READ_BYTE(s68k.memory_map[((address)>>16)&0xff].base, (address) & 0xffff);
}

static u32 s68k_read_bus_16(u32 address)
{
version(LOGERROR) {
  error("[SUB 68k] Unused read16 %08X (%08X)\n", address, s68k.pc);
}
  address = s68k.pc;
  return *cast(u16 *)(s68k.memory_map[((address)>>16)&0xff].base + ((address) & 0xffff));
}

static void s68k_unused_8_w(u32 address, u32 data)
{
version(LOGERROR) {
  error("[SUB 68k] Unused write8 %08X = %02X (%08X)\n", address, data, s68k.pc);
}
}

static void s68k_unused_16_w(u32 address, u32 data)
{
version(LOGERROR) {
  error("[SUB 68k] Unused write16 %08X = %04X (%08X)\n", address, data, s68k.pc);
}
}

/*--------------------------------------------------------------------------*/
/* PRG-RAM DMA access                                                       */
/*--------------------------------------------------------------------------*/
void prg_ram_dma_w(u32 words)
{
  u16 data;

  /* CDC buffer source address */
  u16 src_index = cdc.dac.w & 0x3ffe;
  
  /* PRG-RAM destination address*/
  u32 dst_index = (scd.regs[0x0a>>1].w << 3) & 0x7fffe;
  
  /* update DMA destination address */
  scd.regs[0x0a>>1].w += (words >> 2);

  /* update DMA source address */
  cdc.dac.w += (words << 1);

  /* check PRG-RAM write protected area */
  if (dst_index < (scd.regs[0x02>>1].b.h << 9))
  {
    return;
  }

  /* DMA transfer */
  while (words--)
  {
    /* read 16-bit word from CDC buffer */
    data = *cast(u16 *)(cdc.ram + src_index);

version(LSB_FIRST) {
    /* source data is stored in big endian format */
    data = ((data >> 8) | (data << 8)) & 0xffff;
}

    /* write 16-bit word to PRG-RAM */
    *cast(u16 *)(scd.prg_ram + dst_index) = data ;

    /* increment CDC buffer source address */
    src_index = (src_index + 2) & 0x3ffe;

    /* increment PRG-RAM destination address */
    dst_index = (dst_index + 2) & 0x7fffe;
  }
}

/*--------------------------------------------------------------------------*/
/* PRG-RAM write protected area                                             */
/*--------------------------------------------------------------------------*/
static void prg_ram_write_byte(u32 address, u32 data)
{
  address &= 0x7ffff;
  if (address >= (scd.regs[0x02>>1].b.h << 9))
  {
    WRITE_BYTE(scd.prg_ram, address, data);
    return;
  }
version(LOGERROR) {
  error("[SUB 68k] PRG-RAM protected write8 %08X = %02X (%08X)\n", address, data, s68k.pc);
}
}

static void prg_ram_write_word(u32 address, u32 data)
{
  address &= 0x7fffe;
  if (address >= (scd.regs[0x02>>1].b.h << 9))
  {
    *cast(u16 *)(scd.prg_ram + address) = data;
    return;
  }
version(LOGERROR) {
  error("[SUB 68k] PRG-RAM protected write16 %08X = %02X (%08X)\n", address, data, s68k.pc);
}
}

/*--------------------------------------------------------------------------*/
/* internal backup RAM (8KB)                                                */
/*--------------------------------------------------------------------------*/
static u32 bram_read_byte(u32 address)
{
  /* LSB only */
  if (address & 1)
  {
    return scd.bram[(address >> 1) & 0x1fff];
  }

  return 0xff;
}

static u32 bram_read_word(u32 address)
{
  return (scd.bram[(address >> 1) & 0x1fff] | 0xff00);
}

static void bram_write_byte(u32 address, u32 data)
{
  /* LSB only */
  if (address & 1)
  {
    scd.bram[(address >> 1) & 0x1fff] = data;
  }
}

static void bram_write_word(u32 address, u32 data)
{
  scd.bram[(address >> 1) & 0x1fff] = data & 0xff;
}

/*--------------------------------------------------------------------------*/
/* PCM chip & Gate-Array area                                               */
/*--------------------------------------------------------------------------*/

static void s68k_poll_detect(u32 reg)
{
  /* detect SUB-CPU register polling */
  if (s68k.poll.detected == (1 << reg))
  {
    if (s68k.cycles <= s68k.poll.cycle)
    {
      if (s68k.pc == s68k.poll.pc)
      {
        /* stop SUB-CPU until register is modified by MAIN-CPU */
version(LOG_SCD) {
        error("s68k stopped from %d cycles\n", s68k.cycles);
}
        s68k.cycles = s68k.cycle_end;
        s68k.stopped = 1 << reg;
      }
      return;
    }
  }
  else
  {
    /* set SUB-CPU register access flag */
    s68k.poll.detected = 1 << reg;
  }

  /* restart SUB-CPU polling detection */
  s68k.poll.cycle = s68k.cycles + 392;
  s68k.poll.pc = s68k.pc;
}

static void s68k_poll_sync(u32 reg)
{
  /* relative MAIN-CPU cycle counter */
  u32 cycles = (s68k.cycles * MCYCLES_PER_LINE) / SCYCLES_PER_LINE;

  /* sync MAIN-CPU with SUB-CPU */
  if (!m68k.stopped && (m68k.cycles < cycles))
  {
    m68k_run(cycles);
  }

  /* MAIN-CPU stopped on register polling ? */
  if (m68k.stopped & (3 << reg))
  {
    /* sync MAIN-CPU with SUB-CPU */
    m68k.cycles = cycles;

    /* restart MAIN-CPU */
    m68k.stopped = 0;
version(LOG_SCD) {
    error("m68k started from %d cycles\n", cycles);
}
  }

  /* clear CPU register(s) access flags */
  m68k.poll.detected &= ~(3 << reg);
  s68k.poll.detected &= ~(3 << reg);
}

static u32 scd_read_byte(u32 address)
{
  /* PCM area (8K) is mirrored into $FF0000-$FF7FFF */
  if (address < 0xff8000)
  {
    /* get /LDS only */
    if (address & 1)
    {
      return pcm_read((address >> 1) & 0x1fff);
    }

    return s68k_read_bus_8(address);
  }

version(LOG_SCD) {
  error("[%d][%d]read byte CD register %X (%X)\n", v_counter, s68k.cycles, address, s68k.pc);
}

  /* Memory Mode */
  if (address == 0xff8003)
  {
    s68k_poll_detect(0x03);
    return scd.regs[0x03>>1].b.l;
  }

  /* MAIN-CPU communication flags */
  if (address == 0xff800e)
  {
    s68k_poll_detect(0x0e);
    return scd.regs[0x0e>>1].b.h;
  }

  /* CDC register data (controlled by BIOS, byte access only ?) */
  if (address == 0xff8007)
  {
    u32 data = cdc_reg_r();
version(LOG_CDC) {
    error("CDC register %X read 0x%02X (%X)\n", scd.regs[0x04>>1].b.l & 0x0F, data, s68k.pc);
}
    return data;
  }
  
  /* LED status */
  if (address == 0xff8000)
  {
    /* register $00 is reserved for MAIN-CPU, we use $06 instead */
    return scd.regs[0x06>>1].b.h;
  }

  /* RESET status */
  if (address == 0xff8001)
  {
    /* always return 1 */
    return 0x01;
  }

  /* Font data */
  if ((address >= 0xff8050) && (address <= 0xff8056))
  {
    /* shifted 4-bit input (xxxx00) */
    u8 bits = (scd.regs[0x4e>>1].w >> (((address & 6) ^ 6) << 1)) << 2;
    
    /* color code */
    u8 code = scd.regs[0x4c>>1].b.l;
    
    /* 16-bit font data (4 pixels = 16 bits) */
    u16 data = (code >> (bits & 4)) & 0x0f;

    bits = bits >> 1;
    data = data | (((code >> (bits & 4)) << 4) & 0xf0);

    bits = bits >> 1;
    data = data | (((code >> (bits & 4)) << 8) & 0xf00);

    bits = bits >> 1;
    data = data | (((code >> (bits & 4)) << 12) & 0xf000);

    return (address & 1) ? (data & 0xff) : (data >> 8);
  }

  /* MAIN-CPU communication words */
  if ((address & 0x1f0) == 0x10)
  {
    s68k_poll_detect(address & 0x1f);
  }

  /* default registers */
  if (address & 1)
  {
    /* register LSB */
    return scd.regs[(address >> 1) & 0xff].b.l;
  }

  /* register MSB */
  return scd.regs[(address >> 1) & 0xff].b.h;
}

static u32 scd_read_word(u32 address)
{
  /* PCM area (8K) is mirrored into $FF0000-$FF7FFF */
  if (address < 0xff8000)
  {
    /* get /LDS only */
    return pcm_read((address >> 1) & 0x1fff);
  }

version(LOG_SCD) {
  error("[%d][%d]read word CD register %X (%X)\n", v_counter, s68k.cycles, address, s68k.pc);
}

  /* Memory Mode */
  if (address == 0xff8002)
  {
    s68k_poll_detect(0x03);
    return scd.regs[0x03>>1].w;
  }

  /* CDC host data (word access only ?) */
  if (address == 0xff8008)
  {
    return cdc_host_r();
  }

  /* LED & RESET status */
  if (address == 0xff8000)
  {
    /* register $00 is reserved for MAIN-CPU, we use $06 instead */
    return scd.regs[0x06>>1].w;
  }

  /* Stopwatch counter (word access only ?) */
  if (address == 0xff800c)
  {
    /* cycle-accurate counter value */
    return (scd.regs[0x0c>>1].w + ((s68k.cycles - scd.stopwatch) / TIMERS_SCYCLES_RATIO)) & 0xfff;
  }

  /* Font data */
  if ((address >= 0xff8050) && (address <= 0xff8056))
  {
    /* shifted 4-bit input (xxxx00) */
    u8 bits = (scd.regs[0x4e>>1].w >> (((address & 6) ^ 6) << 1)) << 2;
    
    /* color code */
    u8 code = scd.regs[0x4c>>1].b.l;
    
    /* 16-bit font data (4 pixels = 16 bits) */
    u16 data = (code >> (bits & 4)) & 0x0f;

    bits = bits >> 1;
    data = data | (((code >> (bits & 4)) << 4) & 0xf0);

    bits = bits >> 1;
    data = data | (((code >> (bits & 4)) << 8) & 0xf00);

    bits = bits >> 1;
    data = data | (((code >> (bits & 4)) << 12) & 0xf000);

    return data;
  }

  /* MAIN-CPU communication words */
  if ((address & 0x1f0) == 0x10)
  {
    /* relative MAIN-CPU cycle counter */
    u32 cycles = (s68k.cycles * MCYCLES_PER_LINE) / SCYCLES_PER_LINE;

    /* sync MAIN-CPU with SUB-CPU (Mighty Morphin Power Rangers) */
    if (!m68k.stopped && (m68k.cycles < cycles))
    {
      m68k_run(cycles);
    }

    s68k_poll_detect(address & 0x1e);
  }

  /* default registers */
  return scd.regs[(address >> 1) & 0xff].w;
}

void word_ram_switch(u8 mode)
{
  s32 i;
  u16 *ptr1 = cast(u16 *)(scd.word_ram_2M);
  u16 *ptr2 = cast(u16 *)(scd.word_ram[0]);
  u16 *ptr3 = cast(u16 *)(scd.word_ram[1]);

  if (mode & 0x04)
  {
    /* 2M -> 1M mode */
    for (i=0; i<0x10000; i++)
    {
      *ptr2++=*ptr1++;
      *ptr3++=*ptr1++;
    }
  }
  else
  {
    /* 1M -> 2M mode */
    for (i=0; i<0x10000; i++)
    {
      *ptr1++=*ptr2++;
      *ptr1++=*ptr3++;
    }

    /* allow Word-RAM access from both CPU in 2M mode (fixes sync issues in Mortal Kombat) */
    for (i=scd.cartridge.boot+0x20; i<scd.cartridge.boot+0x24; i++)
    {
      /* MAIN-CPU: $200000-$23FFFF is mapped to 256K Word-RAM */
      m68k.memory_map[i].base    = scd.word_ram_2M + ((i & 0x03) << 16);
      m68k.memory_map[i].read8   = null;
      m68k.memory_map[i].read16  = null;
      m68k.memory_map[i].write8  = null;
      m68k.memory_map[i].write16 = null;
      zbank_memory_map[i].read   = null;
      zbank_memory_map[i].write  = null;
    }

    for (i=0x08; i<0x0c; i++)
    {
      /* SUB-CPU: $080000-$0BFFFF is mapped to 256K Word-RAM */
      s68k.memory_map[i].read8   = null;
      s68k.memory_map[i].read16  = null;
      s68k.memory_map[i].write8  = null;
      s68k.memory_map[i].write16 = null;
    }

    for (i=0x0c; i<0x0e; i++)
    {
      /* SUB-CPU: $0C0000-$0DFFFF is unmapped */
      s68k.memory_map[i].read8   = s68k_read_bus_8;
      s68k.memory_map[i].read16  = s68k_read_bus_16;
      s68k.memory_map[i].write8  = s68k_unused_8_w;
      s68k.memory_map[i].write16 = s68k_unused_16_w;
    }
  }
}

static void scd_write_byte(u32 address, u32 data)
{
  /* PCM area (8K) is mirrored into $FF0000-$FF7FFF */
  if (address < 0xff8000)
  {
    /* get /LDS only */
    if (address & 1)
    {
      pcm_write((address >> 1) & 0x1fff, data);
      return;
    }

    s68k_unused_8_w(address, data);
    return;
  }

version(LOG_SCD) {
  error("[%d][%d]write byte CD register %X -> 0x%02x (%X)\n", v_counter, s68k.cycles, address, data, s68k.pc);
}

  /* Gate-Array registers */
  switch (address & 0x1ff)
  {
    case 0x00: /* LED status */
    {
      /* register $00 is reserved for MAIN-CPU, use $06 instead */
      scd.regs[0x06 >> 1].b.h = data;
      return;
    }

    case 0x01: /* RESET status */
    {
      /* RESET bit cleared ? */      
      if (!(data & 0x01))
      {
        /* reset CD hardware */
        scd_reset(0);
      }
      return;
    }

    case 0x03: /* Memory Mode */
    {
      s68k_poll_sync(0x02);

      /* detect MODE & RET bits modifications */
      if ((data ^ scd.regs[0x03 >> 1].b.l) & 0x05)
      {
        s32 i;
        
        /* MODE bit */
        if (data & 0x04)
        {
          /* 2M->1M mode switch */
          if (!(scd.regs[0x03 >> 1].b.l & 0x04))
          {
            /* re-arrange Word-RAM banks */
            word_ram_switch(0x04);
          }

          /* RET bit in 1M Mode */
          if (data & 0x01)
          {
            /* Word-RAM 1 assigned to MAIN-CPU */
            for (i=scd.cartridge.boot+0x20; i<scd.cartridge.boot+0x22; i++)
            {
              /* Word-RAM 1 data mapped at $200000-$21FFFF */
              m68k.memory_map[i].base = scd.word_ram[1] + ((i & 0x01) << 16);
            }

            for (i=scd.cartridge.boot+0x22; i<scd.cartridge.boot+0x24; i++)
            {
              /* VRAM cell image mapped at $220000-$23FFFF */
              m68k.memory_map[i].read8   = cell_ram_1_read8;
              m68k.memory_map[i].read16  = cell_ram_1_read16;
              m68k.memory_map[i].write8  = cell_ram_1_write8;
              m68k.memory_map[i].write16 = cell_ram_1_write16;
              zbank_memory_map[i].read   = cell_ram_1_read8;
              zbank_memory_map[i].write  = cell_ram_1_write8;
            }

            /* Word-RAM 0 assigned to SUB-CPU */
            for (i=0x08; i<0x0c; i++)
            {
              /* DOT image mapped at $080000-$0BFFFF */
              s68k.memory_map[i].read8   = dot_ram_0_read8;
              s68k.memory_map[i].read16  = dot_ram_0_read16;
              s68k.memory_map[i].write8  = dot_ram_0_write8;
              s68k.memory_map[i].write16 = dot_ram_0_write16;
            }

            for (i=0x0c; i<0x0e; i++)
            {
              /* Word-RAM 0 data mapped at $0C0000-$0DFFFF */
              s68k.memory_map[i].base    = scd.word_ram[0] + ((i & 0x01) << 16);
              s68k.memory_map[i].read8   = null;
              s68k.memory_map[i].read16  = null;
              s68k.memory_map[i].write8  = null;
              s68k.memory_map[i].write16 = null;
            }

            /* writing 1 to RET bit in 1M mode returns Word-RAM to MAIN-CPU in 2M mode */
            scd.dmna = 0;
          }
          else
          {
            /* Word-RAM 0 assigned to MAIN-CPU */
            for (i=scd.cartridge.boot+0x20; i<scd.cartridge.boot+0x22; i++)
            {
              /* Word-RAM 0 data mapped at $200000-$21FFFF */
              m68k.memory_map[i].base = scd.word_ram[0] + ((i & 0x01) << 16);
            }

            for (i=scd.cartridge.boot+0x22; i<scd.cartridge.boot+0x24; i++)
            {
              /* VRAM cell image mapped at $220000-$23FFFF */
              m68k.memory_map[i].read8   = cell_ram_0_read8;
              m68k.memory_map[i].read16  = cell_ram_0_read16;
              m68k.memory_map[i].write8  = cell_ram_0_write8;
              m68k.memory_map[i].write16 = cell_ram_0_write16;
              zbank_memory_map[i].read   = cell_ram_0_read8;
              zbank_memory_map[i].write  = cell_ram_0_write8;
            }

            /* Word-RAM 1 assigned to SUB-CPU */
            for (i=0x08; i<0x0c; i++)
            {
              /* DOT image mapped at $080000-$0BFFFF */
              s68k.memory_map[i].read8   = dot_ram_1_read8;
              s68k.memory_map[i].read16  = dot_ram_1_read16;
              s68k.memory_map[i].write8  = dot_ram_1_write8;
              s68k.memory_map[i].write16 = dot_ram_1_write16;
            }

            for (i=0x0c; i<0x0e; i++)
            {
              /* Word-RAM 1 data mapped at $0C0000-$0DFFFF */
              s68k.memory_map[i].base    = scd.word_ram[1] + ((i & 0x01) << 16);
              s68k.memory_map[i].read8   = null;
              s68k.memory_map[i].read16  = null;
              s68k.memory_map[i].write8  = null;
              s68k.memory_map[i].write16 = null;
            }
          }

          /* clear DMNA bit (swap completed) */
          scd.regs[0x02 >> 1].b.l = (scd.regs[0x02 >> 1].b.l & ~0x1f) | (data & 0x1d);
          return;
        }
        else
        {
          /* 1M->2M mode switch */
          if (scd.regs[0x02 >> 1].b.l & 0x04)
          {
            /* re-arrange Word-RAM banks */
            word_ram_switch(0x00);

            /* RET bit set during 1M mode ? */
            data |= ~scd.dmna & 0x01;
            
            /* check if RET bit is cleared */
            if (!(data & 0x01))
            {
              /* set DMNA bit */
              data |= 0x02;

              /* mask BK0-1 bits (MAIN-CPU side only) */
              scd.regs[0x02 >> 1].b.l = (scd.regs[0x02 >> 1].b.l & ~0x1f) | (data & 0x1f);
              return;
            }
          }
          
          /* RET bit set in 2M mode */
          if (data & 0x01)
          {
            /* Word-RAM is returned to MAIN-CPU */
            scd.dmna = 0;

            /* clear DMNA bit */
            scd.regs[0x02 >> 1].b.l = (scd.regs[0x02 >> 1].b.l & ~0x1f) | (data & 0x1d);
            return;
          }
        }
      }

      /* update PM0-1 & MODE bits */
      scd.regs[0x02 >> 1].b.l = (scd.regs[0x02 >> 1].b.l & ~0x1c) | (data & 0x1c);
      return;
    }

    case 0x07: /* CDC register write */
    {
      cdc_reg_w(data);
      return;
    }

    case 0x0e: /* MAIN-CPU communication flags, normally read-only (Space Ace, Dragon's Lair) */
    {
      /* ROR8 operation */
      data = (data >> 1) | ((data << 7) & 1);
    }

    case 0x0f:  /* SUB-CPU communication flags */
    {
      s68k_poll_sync(0x0e);
      scd.regs[0x0f>>1].b.l = data;
      return;
    }

    case 0x31: /* Timer */
    {
      /* reload timer (one timer clock = 384 CPU cycles) */
      scd.timer = data * TIMERS_SCYCLES_RATIO;

      /* only non-zero data starts timer, writing zero stops it */
      if (data)
      {
        /* adjust regarding current CPU cycle */
        scd.timer += (s68k.cycles - scd.cycles);
      }

      scd.regs[0x30>>1].b.l = data;
      return;
    }

    case 0x33: /* Interrupts */
    {
      /* update register value before updating interrupts */
      scd.regs[0x32>>1].b.l = data;

      /* update IEN2 flag */
      scd.regs[0x00].b.h = (scd.regs[0x00].b.h & 0x7f) | ((data & 0x04) << 5);
      
      /* update IRQ level */
      s68k_update_irq((scd.pending & data) >> 1);
      return;
    }

    case 0x37: /* CDD control (controlled by BIOS, byte access only ?) */
    {
      /* CDD communication started ? */
      if ((data & 0x04) && !(scd.regs[0x37>>1].b.l & 0x04))
      {
        /* reset CDD cycle counter */
        cdd.cycles = (scd.cycles - s68k.cycles) * 3;

        /* set pending interrupt level 4 */
        scd.pending |= (1 << 4);

        /* update IRQ level if interrupt is enabled */
        if (scd.regs[0x32>>1].b.l & 0x10)
        {
          s68k_update_irq((scd.pending & scd.regs[0x32>>1].b.l) >> 1);
        }
      }

      scd.regs[0x37>>1].b.l = data;
      return;
    }

    default:
    {
      /* SUB-CPU communication words */
      if ((address & 0xf0) == 0x20)
      {
        s68k_poll_sync((address - 0x10) & 0x1e);
      }

      /* default registers */
      if (address & 1)
      {
        /* register LSB */
        scd.regs[(address >> 1) & 0xff].b.l = data;
        return;
      }

      /* register MSB */
      scd.regs[(address >> 1) & 0xff].b.h = data;
      return;
    }
  }
}

static void scd_write_word(u32 address, u32 data)
{
  /* PCM area (8K) is mirrored into $FF0000-$FF7FFF */
  if (address < 0xff8000)
  {
    /* get /LDS only */
    pcm_write((address >> 1) & 0x1fff, data);
    return;
  }

version(LOG_SCD) {
  error("[%d][%d]write word CD register %X -> 0x%04x (%X)\n", v_counter, s68k.cycles, address, data, s68k.pc);
}

  /* Gate-Array registers */
  switch (address & 0x1fe)
  {
    case 0x00: /* LED status & RESET */
    {
      /* only update LED status (register $00 is reserved for MAIN-CPU, use $06 instead) */
      scd.regs[0x06>>1].b.h = data >> 8;

      /* RESET bit cleared ? */      
      if (!(data & 0x01))
      {
        /* reset CD hardware */
        scd_reset(0);
      }
      return;
    }

    case 0x02: /* Memory Mode */
    {
      s68k_poll_sync(0x02);

      /* detect MODE & RET bits modifications */
      if ((data ^ scd.regs[0x03>>1].b.l) & 0x05)
      {
        s32 i;

        /* MODE bit */
        if (data & 0x04)
        {
          /* 2M->1M mode switch */
          if (!(scd.regs[0x03 >> 1].b.l & 0x04))
          {
            /* re-arrange Word-RAM banks */
            word_ram_switch(0x04);
          }

          /* RET bit in 1M Mode */
          if (data & 0x01)
          {
            /* Word-RAM 1 assigned to MAIN-CPU */
            for (i=scd.cartridge.boot+0x20; i<scd.cartridge.boot+0x22; i++)
            {
              /* Word-RAM 1 data mapped at $200000-$21FFFF */
              m68k.memory_map[i].base = scd.word_ram[1] + ((i & 0x01) << 16);
            }

            for (i=scd.cartridge.boot+0x22; i<scd.cartridge.boot+0x24; i++)
            {
              /* VRAM cell image mapped at $220000-$23FFFF */
              m68k.memory_map[i].read8   = cell_ram_1_read8;
              m68k.memory_map[i].read16  = cell_ram_1_read16;
              m68k.memory_map[i].write8  = cell_ram_1_write8;
              m68k.memory_map[i].write16 = cell_ram_1_write16;
              zbank_memory_map[i].read   = cell_ram_1_read8;
              zbank_memory_map[i].write  = cell_ram_1_write8;
            }

            /* Word-RAM 0 assigned to SUB-CPU */
            for (i=0x08; i<0x0c; i++)
            {
              /* DOT image mapped at $080000-$0BFFFF */
              s68k.memory_map[i].read8   = dot_ram_0_read8;
              s68k.memory_map[i].read16  = dot_ram_0_read16;
              s68k.memory_map[i].write8  = dot_ram_0_write8;
              s68k.memory_map[i].write16 = dot_ram_0_write16;
            }

            for (i=0x0c; i<0x0e; i++)
            {
              /* Word-RAM 0 data mapped at $0C0000-$0DFFFF */
              s68k.memory_map[i].base    = scd.word_ram[0] + ((i & 0x01) << 16);
              s68k.memory_map[i].read8   = null;
              s68k.memory_map[i].read16  = null;
              s68k.memory_map[i].write8  = null;
              s68k.memory_map[i].write16 = null;
            }

            /* writing 1 to RET bit in 1M mode returns Word-RAM to MAIN-CPU in 2M mode */
            scd.dmna = 0;
          }
          else
          {
            /* Word-RAM 0 assigned to MAIN-CPU */
            for (i=scd.cartridge.boot+0x20; i<scd.cartridge.boot+0x22; i++)
            {
              /* Word-RAM 0 data mapped at $200000-$21FFFF */
              m68k.memory_map[i].base = scd.word_ram[0] + ((i & 0x01) << 16);
            }

            for (i=scd.cartridge.boot+0x22; i<scd.cartridge.boot+0x24; i++)
            {
              /* VRAM cell image mapped at $220000-$23FFFF */
              m68k.memory_map[i].read8   = cell_ram_0_read8;
              m68k.memory_map[i].read16  = cell_ram_0_read16;
              m68k.memory_map[i].write8  = cell_ram_0_write8;
              m68k.memory_map[i].write16 = cell_ram_0_write16;
              zbank_memory_map[i].read   = cell_ram_0_read8;
              zbank_memory_map[i].write  = cell_ram_0_write8;
            }

            /* Word-RAM 1 assigned to SUB-CPU */
            for (i=0x08; i<0x0c; i++)
            {
              /* DOT image mapped at $080000-$0BFFFF */
              s68k.memory_map[i].read8   = dot_ram_1_read8;
              s68k.memory_map[i].read16  = dot_ram_1_read16;
              s68k.memory_map[i].write8  = dot_ram_1_write8;
              s68k.memory_map[i].write16 = dot_ram_1_write16;
            }

            for (i=0x0c; i<0x0e; i++)
            {
              /* Word-RAM 1 data mapped at $0C0000-$0DFFFF */
              s68k.memory_map[i].base    = scd.word_ram[1] + ((i & 0x01) << 16);
              s68k.memory_map[i].read8   = null;
              s68k.memory_map[i].read16  = null;
              s68k.memory_map[i].write8  = null;
              s68k.memory_map[i].write16 = null;
            }
          }

          /* clear DMNA bit (swap completed) */
          scd.regs[0x03>>1].b.l = (scd.regs[0x03>>1].b.l & ~0x1f) | (data & 0x1d);
          return;
        }
        else
        {
          /* 1M->2M mode switch */
          if (scd.regs[0x03>>1].b.l & 0x04)
          {
            /* re-arrange Word-RAM banks */
            word_ram_switch(0x00);

            /* RET bit set during 1M mode ? */
            data |= ~scd.dmna & 0x01;
            
            /* check if RET bit is cleared */
            if (!(data & 0x01))
            {
              /* set DMNA bit */
              data |= 0x02;

              /* mask BK0-1 bits (MAIN-CPU side only) */
              scd.regs[0x03>>1].b.l = (scd.regs[0x03>>1].b.l & ~0x1f) | (data & 0x1f);
              return;
            }
          }
          
          /* RET bit set in 2M mode */
          if (data & 0x01)
          {
            /* Word-RAM is returned to MAIN-CPU */
            scd.dmna = 0;

            /* clear DMNA bit */
            scd.regs[0x03>>1].b.l = (scd.regs[0x03>>1].b.l & ~0x1f) | (data & 0x1d);
            return;
          }
        }
      }

      /* update PM0-1 & MODE bits */
      scd.regs[0x03>>1].b.l = (scd.regs[0x03>>1].b.l & ~0x1c) | (data & 0x1c);
      return;
    }

    case 0x06: /* CDC register write */
    {
      cdc_reg_w(data);
      return;
    }

    case 0x0c: /* Stopwatch (word access only) */
    {
      /* synchronize the counter with SUB-CPU */
      s32 ticks = (s68k.cycles - scd.stopwatch) / TIMERS_SCYCLES_RATIO;
      scd.stopwatch += (ticks * TIMERS_SCYCLES_RATIO);

      /* any writes clear the counter */
      scd.regs[0x0c>>1].w = 0;
      return;
    }

    case 0x0e:  /* SUB-CPU communication flags */
    {
      s68k_poll_sync(0x0e);

      /* MSB is read-only */
      scd.regs[0x0f>>1].b.l = data;
      return;
    }

    case 0x30: /* Timer */
    {
      /* LSB only */
      data &= 0xff;

      /* reload timer (one timer clock = 384 CPU cycles) */
      scd.timer = data * TIMERS_SCYCLES_RATIO;

      /* only non-zero data starts timer, writing zero stops it */
      if (data)
      {
        /* adjust regarding current CPU cycle */
        scd.timer += (s68k.cycles - scd.cycles);
      }

      scd.regs[0x30>>1].b.l = data;
      return;
    }

    case 0x32: /* Interrupts */
    {
      /* LSB only */
      data &= 0xff;

      /* update register value before updating interrupts */
      scd.regs[0x32>>1].b.l = data;

      /* update IEN2 flag */
      scd.regs[0x00].b.h = (scd.regs[0x00].b.h & 0x7f) | ((data & 0x04) << 5);
      
      /* update IRQ level */
      s68k_update_irq((scd.pending & data) >> 1);
      return;
    }

    case 0x4a: /* CDD command 9 (controlled by BIOS, word access only ?) */
    {
      scd.regs[0x4a>>1].w = 0;
      cdd_process();
version(LOG_CDD) {
      error("CDD command: %02x %02x %02x %02x %02x %02x %02x %02x\n",scd.regs[0x42>>1].b.h, scd.regs[0x42>>1].b.l, scd.regs[0x44>>1].b.h, scd.regs[0x44>>1].b.l, scd.regs[0x46>>1].b.h, scd.regs[0x46>>1].b.l, scd.regs[0x48>>1].b.h, scd.regs[0x48>>1].b.l);
      error("CDD status:  %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",scd.regs[0x38>>1].b.h, scd.regs[0x38>>1].b.l, scd.regs[0x3a>>1].b.h, scd.regs[0x3a>>1].b.l, scd.regs[0x3c>>1].b.h, scd.regs[0x3c>>1].b.l, scd.regs[0x3e>>1].b.h, scd.regs[0x3e>>1].b.l, scd.regs[0x40>>1].b.h, scd.regs[0x40>>1].b.l);
}
      break;
    }

    case 0x66: /* Trace vector base address */
    {
      scd.regs[0x66>>1].w = data;
      
      /* start GFX operation */
      gfx_start(data, s68k.cycles);
      return;
    }

    default:
    {
      /* SUB-CPU communication words */
      if ((address & 0xf0) == 0x20)
      {
        s68k_poll_sync((address - 0x10) & 0x1e);
      }

      /* default registers */
      scd.regs[(address >> 1) & 0xff].w = data;
      return;
    }
  }
}


void scd_init()
{
  s32 i;
  
  /****************************************************************/
  /*  MAIN-CPU low memory map ($000000-$7FFFFF)                   */
  /****************************************************************/

  /* 0x00: boot from CD (Mode 2), 0x40: boot from cartridge (Mode 1) */
  u8 base = scd.cartridge.boot;

  /* $400000-$7FFFFF (resp. $000000-$3FFFFF): cartridge area (4MB) */
  cd_cart_init();

  /* $000000-$1FFFFF (resp. $400000-$5FFFFF): CD memory area */
  for (i=base; i<base+0x20; i++)
  {
    if (i & 2)
    {
      /* $020000-$03FFFF (resp. $420000-$43FFFF): PRG-RAM (first 128KB bank, mirrored each 256KB) */
      m68k.memory_map[i].base    = scd.prg_ram + ((i & 1) << 16);
      m68k.memory_map[i].read8   = null;
      m68k.memory_map[i].read16  = null;
      m68k.memory_map[i].write8  = null;
      m68k.memory_map[i].write16 = null;
      zbank_memory_map[i].read   = null;
      zbank_memory_map[i].write  = null;

    }
    else
    {
      /* $000000-$01FFFF (resp. $400000-$41FFFF): internal ROM (128KB, mirrored each 256KB) */
      /* NB: Flux expects it to be mapped at $440000-$45FFFF */
      m68k.memory_map[i].base    = scd.bootrom + ((i & 1) << 16);
      m68k.memory_map[i].read8   = null;
      m68k.memory_map[i].read16  = null;
      m68k.memory_map[i].write8  = m68k_unused_8_w;
      m68k.memory_map[i].write16 = m68k_unused_16_w;
      zbank_memory_map[i].read   = null;
      zbank_memory_map[i].write  = zbank_unused_w;
    }
  }

  /* $200000-$3FFFFF (resp. $600000-$7FFFFF): Word-RAM in 2M mode (256KB mirrored) */
  for (i=base+0x20; i<base+0x40; i++)
  {
    m68k.memory_map[i].base    = scd.word_ram_2M + ((i & 3) << 16);
    m68k.memory_map[i].read8   = null;
    m68k.memory_map[i].read16  = null;
    m68k.memory_map[i].write8  = null;
    m68k.memory_map[i].write16 = null;
    zbank_memory_map[i].read   = null;
    zbank_memory_map[i].write  = null;
  }
  
  /****************************************************************/
  /*  SUB-CPU memory map ($000000-$FFFFFF)                        */
  /****************************************************************/

  /* $000000-$07FFFF: PRG-RAM (512KB) */
  for (i=0x00; i<0x08; i++)
  {
    s68k.memory_map[i].base    = scd.prg_ram + (i << 16);
    s68k.memory_map[i].read8   = null;
    s68k.memory_map[i].read16  = null;

    /* first 128KB is write-protected */
    s68k.memory_map[i].write8  = (i < 0x02) ? prg_ram_write_byte : null;
    s68k.memory_map[i].write16 = (i < 0x02) ? prg_ram_write_word : null;
  }

  /* $080000-$0BFFFF:  Word-RAM in 2M mode (256KB)*/
  for (i=0x08; i<0x0c; i++)
  {
    s68k.memory_map[i].base    = scd.word_ram_2M + ((i & 3) << 16);
    s68k.memory_map[i].read8   = null;
    s68k.memory_map[i].read16  = null;
    s68k.memory_map[i].write8  = null;
    s68k.memory_map[i].write16 = null;
  }
  
  /* $0C0000-$FD0000: Unused area (Word-RAM mirrored ?) */
  for (i=0x0c; i<0xfd; i++)
  {
    s68k.memory_map[i].base     = scd.word_ram_2M + ((i & 3) << 16);
    s68k.memory_map[i].read8    = s68k_read_bus_8;
    s68k.memory_map[i].read16   = s68k_read_bus_16;
    s68k.memory_map[i].write8   = s68k_unused_8_w;
    s68k.memory_map[i].write16  = s68k_unused_16_w;
  }

  /* $FD0000-$FF0000 (odd address only): 8KB backup RAM, mirrored(Wonder Mega / X'Eye BIOS access it at $FD0000-$FD1FFF) */
  for (i=0xfd; i<0xff; i++)
  {
    s68k.memory_map[i].base     = null;
    s68k.memory_map[i].read8    = bram_read_byte;
    s68k.memory_map[i].read16   = bram_read_word;
    s68k.memory_map[i].write8   = bram_write_byte;
    s68k.memory_map[i].write16  = bram_write_word;
  }

  /* $FF0000-$FFFFFF: PCM hardware & SUB-CPU registers  */
  s68k.memory_map[0xff].base     = null;
  s68k.memory_map[0xff].read8    = scd_read_byte;
  s68k.memory_map[0xff].read16   = scd_read_word;
  s68k.memory_map[0xff].write8   = scd_write_byte;
  s68k.memory_map[0xff].write16  = scd_write_word;

  /* Initialize CD hardware */
  cdc_init();
  gfx_init();

  /* Clear RAM */
  scd.prg_ram[] = 0;
  scd.word_ram[] = 0;
  scd.word_ram_2M[] = 0;
  scd.bram[] = 0;
}

void scd_reset(s32 hard)
{
  /* TODO: figure what exactly is resetted when RESET bit is cleared by SUB-CPU */
  if (hard)
  {
    s32 i;
    
    /* Clear all ASIC registers by default */
    scd.regs[] = 0;

    /* Clear pending DMNA write status */
    scd.dmna = 0;

    /* H-INT default vector */
    *cast(u16 *)(m68k.memory_map[0].base + 0x70) = 0x00FF;
    *cast(u16 *)(m68k.memory_map[0].base + 0x72) = 0xFFFF;

    /* Power ON initial values (MAIN-CPU side) */
    scd.regs[0x00>>1].w = 0x0002;
    scd.regs[0x02>>1].w = 0x0001;

    /* 2M mode */
    word_ram_switch(0);

    /* reset PRG-RAM banking on MAIN-CPU side */
    for (i=scd.cartridge.boot+0x02; i<scd.cartridge.boot+0x20; i+=4)
    {
      /* MAIN-CPU: $020000-$03FFFF (resp. $420000-$43FFFF) mapped to first 128KB PRG-RAM bank (mirrored each 256KB) */
      m68k.memory_map[i].base = scd.prg_ram;
      m68k.memory_map[i+1].base = scd.prg_ram + 0x10000;
    }
  }
  else
  {
    /* Clear only SUB-CPU side registers */
    memset(&scd.regs[0x04>>1], 0, sizeof(scd.regs) - 4);
  }

  /* SUB-CPU side default values */
  scd.regs[0x08>>1].w = 0xffff;
  scd.regs[0x0a>>1].w = 0xffff;
  scd.regs[0x36>>1].w = 0x0100;
  scd.regs[0x40>>1].w = 0x000f;
  scd.regs[0x42>>1].w = 0xffff;
  scd.regs[0x44>>1].w = 0xffff;
  scd.regs[0x46>>1].w = 0xffff;
  scd.regs[0x48>>1].w = 0xffff;
  scd.regs[0x4a>>1].w = 0xffff;

  /* RESET register always return 1 (register $06 is unused by both sides, it is used for SUB-CPU first register) */
  scd.regs[0x06>>1].b.l = 0x01;

  /* Reset Timer & Stopwatch counters */
  scd.timer = 0;
  scd.stopwatch = 0;

  /* Reset frame cycle counter */
  scd.cycles = 0;

  /* Clear pending interrupts */
  scd.pending = 0;

  /* Clear CPU polling detection */
  m68k.poll[] = 0;
  s68k.poll[] = 0;

  /* Reset CD hardware */
  cdd_reset();
  cdc_reset();
  gfx_reset();
  pcm_reset();
}

void scd_update(u32 cycles)
{
  /* update CDC DMA transfer */
  if (cdc.dma_w)
  {
    cdc_dma_update();
  }

  /* run both CPU in sync until end of line */
  do
  {
    m68k_run(cycles);
    s68k_run(scd.cycles + SCYCLES_PER_LINE);
  }
  while ((m68k.cycles < cycles) || (s68k.cycles < (scd.cycles + SCYCLES_PER_LINE)));

  /* increment CD hardware cycle counter */
  scd.cycles += SCYCLES_PER_LINE;

  /* CDD processing at 75Hz (one clock = 12500000/75 = 500000/3 CPU clocks) */
  cdd.cycles += (SCYCLES_PER_LINE * 3);
  if (cdd.cycles >= (500000 * 4))
  {
    /* reload CDD cycle counter */
    cdd.cycles -= (500000 * 4);

    /* update CDD sector */
    cdd_update();

    /* check if a new CDD command has been processed */
    if (!(scd.regs[0x4a>>1].b.l & 0xf0))
    {
      /* reset CDD command wait flag */
      scd.regs[0x4a>>1].b.l = 0xf0;

      /* pending level 4 interrupt */
      scd.pending |= (1 << 4);

      /* level 4 interrupt enabled */
      if (scd.regs[0x32>>1].b.l & 0x10)
      {
        /* update IRQ level */
        s68k_update_irq((scd.pending & scd.regs[0x32>>1].b.l) >> 1);
      }
    }
  }

  /* Timer */
  if (scd.timer)
  {
    /* decrement timer */
    scd.timer -= SCYCLES_PER_LINE;
    if (scd.timer <= 0)
    {
      /* reload timer (one timer clock = 384 CPU cycles) */
      scd.timer += (scd.regs[0x30>>1].b.l * TIMERS_SCYCLES_RATIO);

      /* level 3 interrupt enabled ? */
      if (scd.regs[0x32>>1].b.l & 0x08)
      {
        /* trigger level 3 interrupt */
        scd.pending |= (1 << 3);

        /* update IRQ level */
        s68k_update_irq((scd.pending & scd.regs[0x32>>1].b.l) >> 1);
      }
    }
  }

  /* GFX processing */
  if (scd.regs[0x58>>1].b.h & 0x80)
  {
    /* update graphics operation if running */
    gfx_update(scd.cycles);
  }
}

void scd_end_frame(u32 cycles)
{
  /* run Stopwatch until end of frame */
  s32 ticks = (cycles - scd.stopwatch) / TIMERS_SCYCLES_RATIO;
  scd.regs[0x0c>>1].w = (scd.regs[0x0c>>1].w + ticks) & 0xfff;

  /* adjust Stopwatch counter for next frame (can be negative) */
  scd.stopwatch += (ticks * TIMERS_SCYCLES_RATIO) - cycles;

  /* adjust SUB-CPU & GPU cycle counters for next frame */
  s68k.cycles -= cycles;
  gfx.cycles  -= cycles;

  /* reset CPU registers polling */
  m68k.poll.cycle = 0;
  s68k.poll.cycle = 0;
}

s32 scd_context_save(u8 *state)
{
  u16 tmp16;
  u32 tmp32;
  s32 bufferptr = 0;

  /* internal harware */
  save_param(&bufferptr, state, scd.regs, sizeof(scd.regs));
  save_param(&bufferptr, state, &scd.cycles, sizeof(scd.cycles));
  save_param(&bufferptr, state, &scd.timer, sizeof(scd.timer));
  save_param(&bufferptr, state, &scd.pending, sizeof(scd.pending));
  save_param(&bufferptr, state, &scd.dmna, sizeof(scd.dmna));

  /* GFX processor */
  bufferptr += gfx_context_save(&state[bufferptr]);

  /* CD Data controller */
  bufferptr += cdc_context_save(&state[bufferptr]);

  /* CD Drive processor */
  bufferptr += cdd_context_save(&state[bufferptr]);

  /* PCM chip */
  bufferptr += pcm_context_save(&state[bufferptr]);

  /* PRG-RAM */
  save_param(&bufferptr, state, scd.prg_ram, sizeof(scd.prg_ram));

  /* Word-RAM */
  if (scd.regs[0x03>>1].b.l & 0x04)
  {
    /* 1M mode */
    save_param(&bufferptr, state, scd.word_ram, sizeof(scd.word_ram));
  }
  else
  {
    /* 2M mode */
    save_param(&bufferptr, state, scd.word_ram_2M, sizeof(scd.word_ram_2M));
  }

  /* MAIN-CPU & SUB-CPU polling */
  save_param(&bufferptr, state, &m68k.poll, sizeof(m68k.poll));
  save_param(&bufferptr, state, &s68k.poll, sizeof(s68k.poll));

  /* H-INT default vector */
  tmp16 = *cast(u16 *)(m68k.memory_map[0].base + 0x72);
  save_param(&bufferptr, state, &tmp16, 2);

  /* SUB-CPU internal state */
  save_param(&bufferptr, state, &s68k.cycles, sizeof(s68k.cycles));
  save_param(&bufferptr, state, &s68k.int_level, sizeof(s68k.int_level));
  save_param(&bufferptr, state, &s68k.stopped, sizeof(s68k.stopped));

  /* SUB-CPU registers */
  tmp32 = s68k_get_reg(M68K_REG_D0);  save_param(&bufferptr, state, &tmp32, 4);
  tmp32 = s68k_get_reg(M68K_REG_D1);  save_param(&bufferptr, state, &tmp32, 4);
  tmp32 = s68k_get_reg(M68K_REG_D2);  save_param(&bufferptr, state, &tmp32, 4);
  tmp32 = s68k_get_reg(M68K_REG_D3);  save_param(&bufferptr, state, &tmp32, 4);
  tmp32 = s68k_get_reg(M68K_REG_D4);  save_param(&bufferptr, state, &tmp32, 4);
  tmp32 = s68k_get_reg(M68K_REG_D5);  save_param(&bufferptr, state, &tmp32, 4);
  tmp32 = s68k_get_reg(M68K_REG_D6);  save_param(&bufferptr, state, &tmp32, 4);
  tmp32 = s68k_get_reg(M68K_REG_D7);  save_param(&bufferptr, state, &tmp32, 4);
  tmp32 = s68k_get_reg(M68K_REG_A0);  save_param(&bufferptr, state, &tmp32, 4);
  tmp32 = s68k_get_reg(M68K_REG_A1);  save_param(&bufferptr, state, &tmp32, 4);
  tmp32 = s68k_get_reg(M68K_REG_A2);  save_param(&bufferptr, state, &tmp32, 4);
  tmp32 = s68k_get_reg(M68K_REG_A3);  save_param(&bufferptr, state, &tmp32, 4);
  tmp32 = s68k_get_reg(M68K_REG_A4);  save_param(&bufferptr, state, &tmp32, 4);
  tmp32 = s68k_get_reg(M68K_REG_A5);  save_param(&bufferptr, state, &tmp32, 4);
  tmp32 = s68k_get_reg(M68K_REG_A6);  save_param(&bufferptr, state, &tmp32, 4);
  tmp32 = s68k_get_reg(M68K_REG_A7);  save_param(&bufferptr, state, &tmp32, 4);
  tmp32 = s68k_get_reg(M68K_REG_PC);  save_param(&bufferptr, state, &tmp32, 4);
  tmp16 = s68k_get_reg(M68K_REG_SR);  save_param(&bufferptr, state, &tmp16, 2); 
  tmp32 = s68k_get_reg(M68K_REG_USP); save_param(&bufferptr, state, &tmp32, 4);
  tmp32 = s68k_get_reg(M68K_REG_ISP); save_param(&bufferptr, state, &tmp32, 4);

  /* bootable MD cartridge */
  if (scd.cartridge.boot)
  {
    bufferptr += md_cart_context_save(&state[bufferptr]);
  }

  return bufferptr;
}

s32 scd_context_load(u8 *state)
{
  s32 i;
  u16 tmp16;
  u32 tmp32;
  s32 bufferptr = 0;

  /* internal harware */
  load_param(&bufferptr, state, scd.regs, sizeof(scd.regs));
  load_param(&bufferptr, state, &scd.cycles, sizeof(scd.cycles));
  load_param(&bufferptr, state, &scd.timer, sizeof(scd.timer));
  load_param(&bufferptr, state, &scd.pending, sizeof(scd.pending));
  load_param(&bufferptr, state, &scd.dmna, sizeof(scd.dmna));

  /* GFX processor */
  bufferptr += gfx_context_load(&state[bufferptr]);

  /* CD Data controller */
  bufferptr += cdc_context_load(&state[bufferptr]);

  /* CD Drive processor */
  bufferptr += cdd_context_load(&state[bufferptr]);

  /* PCM chip */
  bufferptr += pcm_context_load(&state[bufferptr]);

  /* PRG-RAM */
  load_param(&bufferptr, state, scd.prg_ram, sizeof(scd.prg_ram));

  /* PRG-RAM 128k bank mapped to $020000-$03FFFF (resp. $420000-$43FFFF) */
  m68k.memory_map[scd.cartridge.boot + 0x02].base = scd.prg_ram + ((scd.regs[0x03>>1].b.l & 0xc0) << 11);
  m68k.memory_map[scd.cartridge.boot + 0x03].base = m68k.memory_map[scd.cartridge.boot + 0x02].base + 0x10000;

  /* Word-RAM */
  if (scd.regs[0x03>>1].b.l & 0x04)
  {
    /* 1M Mode */
    load_param(&bufferptr, state, scd.word_ram, sizeof(scd.word_ram));
  
    if (scd.regs[0x03>>1].b.l & 0x01)
    {
      /* Word-RAM 1 assigned to MAIN-CPU */
      for (i=scd.cartridge.boot+0x20; i<scd.cartridge.boot+0x22; i++)
      {
        /* Word-RAM 1 data mapped at $200000-$21FFFF */
        m68k.memory_map[i].base = scd.word_ram[1] + ((i & 0x01) << 16);
      }

      for (i=scd.cartridge.boot+0x22; i<scd.cartridge.boot+0x24; i++)
      {
        /* VRAM cell image mapped at $220000-$23FFFF */
        m68k.memory_map[i].read8   = cell_ram_1_read8;
        m68k.memory_map[i].read16  = cell_ram_1_read16;
        m68k.memory_map[i].write8  = cell_ram_1_write8;
        m68k.memory_map[i].write16 = cell_ram_1_write16;
        zbank_memory_map[i].read   = cell_ram_1_read8;
        zbank_memory_map[i].write  = cell_ram_1_write8;
      }

      /* Word-RAM 0 assigned to SUB-CPU */
      for (i=0x08; i<0x0c; i++)
      {
        /* DOT image mapped at $080000-$0BFFFF */
        s68k.memory_map[i].read8   = dot_ram_0_read8;
        s68k.memory_map[i].read16  = dot_ram_0_read16;
        s68k.memory_map[i].write8  = dot_ram_0_write8;
        s68k.memory_map[i].write16 = dot_ram_0_write16;
      }

      for (i=0x0c; i<0x0e; i++)
      {
        /* Word-RAM 0 data mapped at $0C0000-$0DFFFF */
        s68k.memory_map[i].base    = scd.word_ram[0] + ((i & 0x01) << 16);
        s68k.memory_map[i].read8   = null;
        s68k.memory_map[i].read16  = null;
        s68k.memory_map[i].write8  = null;
        s68k.memory_map[i].write16 = null;
      }
    }
    else
    {
      /* Word-RAM 0 assigned to MAIN-CPU */
      for (i=scd.cartridge.boot+0x20; i<scd.cartridge.boot+0x22; i++)
      {
        /* Word-RAM 0 data mapped at $200000-$21FFFF */
        m68k.memory_map[i].base = scd.word_ram[0] + ((i & 0x01) << 16);
      }

      for (i=scd.cartridge.boot+0x22; i<scd.cartridge.boot+0x24; i++)
      {
        /* VRAM cell image mapped at $220000-$23FFFF */
        m68k.memory_map[i].read8   = cell_ram_0_read8;
        m68k.memory_map[i].read16  = cell_ram_0_read16;
        m68k.memory_map[i].write8  = cell_ram_0_write8;
        m68k.memory_map[i].write16 = cell_ram_0_write16;
        zbank_memory_map[i].read   = cell_ram_0_read8;
        zbank_memory_map[i].write  = cell_ram_0_write8;
      }

      /* Word-RAM 1 assigned to SUB-CPU */
      for (i=0x08; i<0x0c; i++)
      {
        /* DOT image mapped at $080000-$0BFFFF */
        s68k.memory_map[i].read8   = dot_ram_1_read8;
        s68k.memory_map[i].read16  = dot_ram_1_read16;
        s68k.memory_map[i].write8  = dot_ram_1_write8;
        s68k.memory_map[i].write16 = dot_ram_1_write16;
      }

      for (i=0x0c; i<0x0e; i++)
      {
        /* Word-RAM 1 data mapped at $0C0000-$0DFFFF */
        s68k.memory_map[i].base    = scd.word_ram[1] + ((i & 0x01) << 16);
        s68k.memory_map[i].read8   = null;
        s68k.memory_map[i].read16  = null;
        s68k.memory_map[i].write8  = null;
        s68k.memory_map[i].write16 = null;
      }
    }
  }
  else
  {
    /* 2M mode */
    load_param(&bufferptr, state, scd.word_ram_2M, sizeof(scd.word_ram_2M));

    for (i=scd.cartridge.boot+0x20; i<scd.cartridge.boot+0x24; i++)
    {
      /* MAIN-CPU: $200000-$23FFFF is mapped to 256K Word-RAM */
      m68k.memory_map[i].base    = scd.word_ram_2M + ((i & 0x03) << 16);
      m68k.memory_map[i].read8   = null;
      m68k.memory_map[i].read16  = null;
      m68k.memory_map[i].write8  = null;
      m68k.memory_map[i].write16 = null;
      zbank_memory_map[i].read   = null;
      zbank_memory_map[i].write  = null;
    }

    for (i=0x08; i<0x0c; i++)
    {
      /* SUB-CPU: $080000-$0BFFFF is mapped to 256K Word-RAM */
      s68k.memory_map[i].read8   = null;
      s68k.memory_map[i].read16  = null;
      s68k.memory_map[i].write8  = null;
      s68k.memory_map[i].write16 = null;
    }

    for (i=0x0c; i<0x0e; i++)
    {
      /* SUB-CPU: $0C0000-$0DFFFF is unmapped */
      s68k.memory_map[i].read8   = s68k_read_bus_8;
      s68k.memory_map[i].read16  = s68k_read_bus_16;
      s68k.memory_map[i].write8  = s68k_unused_8_w;
      s68k.memory_map[i].write16 = s68k_unused_16_w;
    }
  }

  /* MAIN-CPU & SUB-CPU polling */
  load_param(&bufferptr, state, &m68k.poll, sizeof(m68k.poll));
  load_param(&bufferptr, state, &s68k.poll, sizeof(s68k.poll));

  /* H-INT default vector */
  load_param(&bufferptr, state, &tmp16, 2);
  *cast(u16 *)(m68k.memory_map[0].base + 0x72) = tmp16;

  /* SUB-CPU internal state */
  load_param(&bufferptr, state, &s68k.cycles, sizeof(s68k.cycles));
  load_param(&bufferptr, state, &s68k.int_level, sizeof(s68k.int_level));
  load_param(&bufferptr, state, &s68k.stopped, sizeof(s68k.stopped));

  /* SUB-CPU registers */
  load_param(&bufferptr, state, &tmp32, 4); s68k_set_reg(M68K_REG_D0, tmp32);
  load_param(&bufferptr, state, &tmp32, 4); s68k_set_reg(M68K_REG_D1, tmp32);
  load_param(&bufferptr, state, &tmp32, 4); s68k_set_reg(M68K_REG_D2, tmp32);
  load_param(&bufferptr, state, &tmp32, 4); s68k_set_reg(M68K_REG_D3, tmp32);
  load_param(&bufferptr, state, &tmp32, 4); s68k_set_reg(M68K_REG_D4, tmp32);
  load_param(&bufferptr, state, &tmp32, 4); s68k_set_reg(M68K_REG_D5, tmp32);
  load_param(&bufferptr, state, &tmp32, 4); s68k_set_reg(M68K_REG_D6, tmp32);
  load_param(&bufferptr, state, &tmp32, 4); s68k_set_reg(M68K_REG_D7, tmp32);
  load_param(&bufferptr, state, &tmp32, 4); s68k_set_reg(M68K_REG_A0, tmp32);
  load_param(&bufferptr, state, &tmp32, 4); s68k_set_reg(M68K_REG_A1, tmp32);
  load_param(&bufferptr, state, &tmp32, 4); s68k_set_reg(M68K_REG_A2, tmp32);
  load_param(&bufferptr, state, &tmp32, 4); s68k_set_reg(M68K_REG_A3, tmp32);
  load_param(&bufferptr, state, &tmp32, 4); s68k_set_reg(M68K_REG_A4, tmp32);
  load_param(&bufferptr, state, &tmp32, 4); s68k_set_reg(M68K_REG_A5, tmp32);
  load_param(&bufferptr, state, &tmp32, 4); s68k_set_reg(M68K_REG_A6, tmp32);
  load_param(&bufferptr, state, &tmp32, 4); s68k_set_reg(M68K_REG_A7, tmp32);
  load_param(&bufferptr, state, &tmp32, 4); s68k_set_reg(M68K_REG_PC, tmp32);  
  load_param(&bufferptr, state, &tmp16, 2); s68k_set_reg(M68K_REG_SR, tmp16);
  load_param(&bufferptr, state, &tmp32, 4); s68k_set_reg(M68K_REG_USP,tmp32);
  load_param(&bufferptr, state, &tmp32, 4); s68k_set_reg(M68K_REG_ISP,tmp32);

  /* bootable MD cartridge hardware */
  if (scd.cartridge.boot)
  {
    bufferptr += md_cart_context_load(&state[bufferptr]);
  }

  return bufferptr;
}

s32 scd_68k_irq_ack(s32 level)
{
version(LOG_SCD) {
  error("INT ack level %d  (%X)\n", level, s68k.pc);
}

    /* clear pending interrupt flag */
    scd.pending &= ~(1 << level);

    /* level 2 interrupt acknowledge */
    if (level == 2)
    {
      /* clear IFL2 flag */
      scd.regs[0x00].b.h &= ~0x01;
    }

    /* update IRQ level */
    s68k_update_irq((scd.pending & scd.regs[0x32>>1].b.l) >> 1);

  return M68K_INT_ACK_AUTOVECTOR;
}
