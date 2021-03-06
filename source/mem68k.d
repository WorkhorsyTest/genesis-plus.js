/***************************************************************************************
 *  Genesis Plus
 *  Main 68k bus handlers
 *
 *  Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003  Charles Mac Donald (original code)
 *  Copyright (C) 2007-2013  Eke-Eke (Genesis Plus GX)
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

/*--------------------------------------------------------------------------*/
/* Unused areas (return open bus data, i.e prefetched instruction word)     */
/*--------------------------------------------------------------------------*/

u32 m68k_read_bus_8(u32 address)
{
version(LOGERROR) {
  error("Unused read8 %08X (%08X)\n", address, m68k_get_reg(M68K_REG_PC));
}
  address = m68k.pc | (address & 1);
  return READ_BYTE(m68k.memory_map[((address)>>16)&0xff].base, (address) & 0xffff);
}

u32 m68k_read_bus_16(u32 address)
{
version(LOGERROR) {
  error("Unused read16 %08X (%08X)\n", address, m68k_get_reg(M68K_REG_PC));
}
  address = m68k.pc;
  return *cast(u16 *)(m68k.memory_map[((address)>>16)&0xff].base + ((address) & 0xffff));
}


void m68k_unused_8_w(u32 address, u32 data)
{
	assert(address == address);
	assert(data == data);
version(LOGERROR) {
  error("Unused write8 %08X = %02X (%08X)\n", address, data, m68k_get_reg(M68K_REG_PC));
}
}

void m68k_unused_16_w(u32 address, u32 data)
{
	assert(address == address);
	assert(data == data);
version(LOGERROR) {
  error("Unused write16 %08X = %04X (%08X)\n", address, data, m68k_get_reg(M68K_REG_PC));
}
}


/*--------------------------------------------------------------------------*/
/* Illegal areas (cause system to lock-up since !DTACK is not returned)     */
/*--------------------------------------------------------------------------*/

void m68k_lockup_w_8 (u32 address, u32 data)
{
	assert(address == address);
	assert(data == data);
version(LOGERROR) {
  error ("Lockup %08X = %02X (%08X)\n", address, data, m68k_get_reg(M68K_REG_PC));
}
  if (!config.force_dtack)
  {
    m68k_pulse_halt();
    m68k.cycles = m68k.cycle_end;
  }
}

void m68k_lockup_w_16 (u32 address, u32 data)
{
	assert(address == address);
	assert(data == data);
version(LOGERROR) {
  error ("Lockup %08X = %04X (%08X)\n", address, data, m68k_get_reg(M68K_REG_PC));
}
  if (!config.force_dtack)
  {
    m68k_pulse_halt();
    m68k.cycles = m68k.cycle_end;
  }
}

u32 m68k_lockup_r_8 (u32 address)
{ 
version(LOGERROR) {
  error ("Lockup %08X.b (%08X)\n", address, m68k_get_reg(M68K_REG_PC));
}
  if (!config.force_dtack)
  {
    m68k_pulse_halt();
    m68k.cycles = m68k.cycle_end;
  }
  address = m68k.pc | (address & 1);
  return READ_BYTE(m68k.memory_map[((address)>>16)&0xff].base, (address) & 0xffff);
}

u32 m68k_lockup_r_16 (u32 address)
{
version(LOGERROR) {
  error ("Lockup %08X.w (%08X)\n", address, m68k_get_reg(M68K_REG_PC));
}
  if (!config.force_dtack)
  {
    m68k_pulse_halt();
    m68k.cycles = m68k.cycle_end;
  }
  address = m68k.pc;
  return *cast(u16 *)(m68k.memory_map[((address)>>16)&0xff].base + ((address) & 0xffff));
}


/*--------------------------------------------------------------------------*/
/* Z80 bus (accessed through I/O chip)                                      */
/*--------------------------------------------------------------------------*/

u32 z80_read_byte(u32 address)
{
  switch ((address >> 13) & 3)
  {
    case 2:   /* YM2612 */
    {
      return fm_read(m68k.cycles, address & 3);
    }

    case 3:   /* Misc  */
    {
      /* VDP (through 68k bus) */
      if ((address & 0xFF00) == 0x7F00)
      {
        return m68k_lockup_r_8(address);
      }
      return (m68k_read_bus_8(address) | 0xFF);
    }

    default: /* ZRAM */
    {
      return zram[address & 0x1FFF];
    }
  }
}

u32 z80_read_word(u32 address)
{
  u32 data = z80_read_byte(address);
  return (data | (data << 8));
}

void z80_write_byte(u32 address, u32 data)
{
  switch ((address >> 13) & 3)
  {
    case 2: /* YM2612 */
    {
      fm_write(m68k.cycles, address & 3, data);
      return;
    }

    case 3:
    {
      switch ((address >> 8) & 0x7F)
      {
        case 0x60:  /* Bank register */
        {
          gen_zbank_w(data & 1);
          return;
        }

        case 0x7F:  /* VDP */
        {
          m68k_lockup_w_8(address, data);
          return;
        }
      
        default:
        {
          m68k_unused_8_w(address, data);
          return;
        }
      }
    }
      
    default: /* ZRAM */
    {
      zram[address & 0x1FFF] = data;
      m68k.cycles += 8; /* ZRAM access latency (fixes Pacman 2: New Adventures) */
      return;
    }
  }
}

void z80_write_word(u32 address, u32 data)
{
  z80_write_byte(address, data >> 8);
}


/*--------------------------------------------------------------------------*/
/* I/O Control                                                              */
/*--------------------------------------------------------------------------*/

static void m68k_poll_detect(u32 reg)
{
  /* detect MAIN-CPU register polling */
  if (m68k.poll.detected == cast(u32) (1 << reg))
  {
    if (m68k.cycles <= m68k.poll.cycle)
    {
      if (m68k.pc == m68k.poll.pc)
      {
        /* stop MAIN-CPU until register is modified by SUB-CPU */
version(LOG_SCD) {
        error("m68k stopped from %d cycles\n", m68k.cycles);
}
        m68k.cycles = m68k.cycle_end;
        m68k.stopped = 1 << reg;

        /* return to current instruction */
        do
        {
          m68k.pc -= 2;
        }
        while (m68k.ir != *cast(u16 *)(m68k.memory_map[(m68k.pc>>16)&0xff].base + (m68k.pc & 0xffff)));
      }
      return;
    }
  }
  else
  {
    /* set MAIN-CPU register access flag */
    m68k.poll.detected = 1 << reg;
  }

  /* restart MAIN-CPU polling detection */
  m68k.poll.cycle = m68k.cycles + 840;
  m68k.poll.pc = m68k.pc;
}

static void m68k_poll_sync(u32 reg)
{
  /* relative SUB-CPU cycle counter */
  u32 cycles = (m68k.cycles * SCYCLES_PER_LINE) / MCYCLES_PER_LINE;

  /* sync SUB-CPU with MAIN-CPU */
  if (!s68k.stopped && (s68k.cycles < cycles))
  {
    s68k_run(cycles);
  }

  /* SUB-CPU stopped on register polling ? */
  if (s68k.stopped & (3 << reg))
  {
    /* sync SUB-CPU with MAIN-CPU */
    s68k.cycles = cycles;

    /* restart SUB-CPU */
    s68k.stopped = 0;
version(LOG_SCD) {
    error("s68k started from %d cycles\n", cycles);
}
  }

  /* clear CPU register(s) access flags */
  m68k.poll.detected &= ~(3 << reg);
  s68k.poll.detected &= ~(3 << reg);
}

u32 ctrl_io_read_byte(u32 address)
{
  switch ((address >> 8) & 0xFF)
  {
    case 0x00:  /* I/O chip */
    {
      if (!(address & 0xE0))
      {
        return io_68k_read((address >> 1) & 0x0F);
      }
      return m68k_read_bus_8(address);
    }

    case 0x11:  /* Z80 BUSACK */
    {
      if (!(address & 1))
      {
        /* Unused bits return prefetched bus data (Time Killers) */
        address = m68k.pc;

        /* Check if bus has been requested and is not reseted */
        if (zstate == 3)
        {
          /* D0 is cleared */
          return (READ_BYTE(m68k.memory_map[((address)>>16)&0xff].base, (address) & 0xffff) & 0xFE);
        }

        /* D0 is set */
        return (READ_BYTE(m68k.memory_map[((address)>>16)&0xff].base, (address) & 0xffff) | 0x01);
      }
      return m68k_read_bus_8(address);
    }

    case 0x20:  /* MEGA-CD */
    {
version(LOG_SCD) {
      error("[%d][%d]read byte CD register %X (%X)\n", v_counter, m68k.cycles, address, m68k.pc);
}
      if (system_hw == SYSTEM_MCD)
      {
        /* register index ($A12000-A1203F mirrored up to $A120FF) */
        u8 index = address & 0x3f;

        /* Memory Mode */
        if (index == 0x03)
        {
          m68k_poll_detect(0x03);
          return scd.regs[0x03>>1].b.l;
        }

        /* SUB-CPU communication flags */
        if (index == 0x0f)
        {
          /* relative SUB-CPU cycle counter */
          u32 cycles = (m68k.cycles * SCYCLES_PER_LINE) / MCYCLES_PER_LINE;

          /* sync SUB-CPU with MAIN-CPU (Dracula Unleashed w/ Sega CD Model 2 OS ROM) */
          if (!s68k.stopped && (s68k.cycles < cycles))
          {
            s68k_run(cycles);
          }

          m68k_poll_detect(0x0f);
          return scd.regs[0x0f>>1].b.l;
        }

        /* default registers */
        if (index < 0x30)
        {
          /* SUB-CPU communication words */
          if (index >= 0x20)
          {
            m68k_poll_detect(index - 0x10);
          }

          /* register LSB */
          if (address & 1)
          {
            return scd.regs[index >> 1].b.l;
          }
              
          /* register MSB */
          return scd.regs[index >> 1].b.h;
        }
      }

      return m68k_read_bus_8(address); 
    }

    case 0x30:  /* TIME */
    {
      if (cart.hw.time_r)
      {
        u32 data = cart.hw.time_r(address);
        if (address & 1)
        {
          return (data & 0xFF);
        }
        return (data >> 8);
      }
      return m68k_read_bus_8(address);
    }

    case 0x41:  /* BOOT ROM */
    {
      if ((config.bios & 1) && (address & 1))
      {
        u32 data = gen_bankswitch_r() & 1;

        /* Unused bits return prefetched bus data */
        address = m68k.pc;
        data |= (READ_BYTE(m68k.memory_map[((address)>>16)&0xff].base, (address) & 0xffff) & 0xFE);
        return data;
      }
      return m68k_read_bus_8(address);
    }

    case 0x10:  /* MEMORY MODE */
    case 0x12:  /* Z80 RESET */
    case 0x13:  /* unknown */
    case 0x40:  /* TMSS */
    case 0x44:  /* RADICA */
    case 0x50:  /* SVP */
    {
      return m68k_read_bus_8(address);
    }

    default:  /* Invalid address */
    {
      return m68k_lockup_r_8(address);
    }
  }
}

u32 ctrl_io_read_word(u32 address)
{
  switch ((address >> 8) & 0xFF)
  {
    case 0x00:  /* I/O chip */
    {
      if (!(address & 0xE0))
      {
        u32 data = io_68k_read((address >> 1) & 0x0F);
        return (data << 8 | data);
      }
      return m68k_read_bus_16(address); 
    }

    case 0x11:  /* Z80 BUSACK */
    {
      /* Unused bits return prefetched bus data (Time Killers) */
      address = m68k.pc;

      /* Check if bus has been requested and is not reseted */
      if (zstate == 3)
      {
        /* D8 is cleared */
        return (*cast(u16 *)(m68k.memory_map[((address)>>16)&0xff].base + ((address) & 0xffff)) & 0xFEFF);
      }

      /* D8 is set */
      return (*cast(u16 *)(m68k.memory_map[((address)>>16)&0xff].base + ((address) & 0xffff)) | 0x0100);
    }

    case 0x20:  /* MEGA-CD */
    {
version(LOG_SCD) {
      error("[%d][%d]read word CD register %X (%X)\n", v_counter, m68k.cycles, address, m68k.pc);
}
      if (system_hw == SYSTEM_MCD)
      {
        /* register index ($A12000-A1203F mirrored up to $A120FF) */
        u8 index = address & 0x3f;

        /* Memory Mode */
        if (index == 0x02)
        {
          m68k_poll_detect(0x03);
          return scd.regs[0x03>>1].w;
        }

        /* CDC host data (word access only ?) */
        if (index == 0x08)
        {
          return cdc_host_r();
        }

        /* H-INT vector (word access only ?) */
        if (index == 0x06)
        {
          return *cast(u16 *)(m68k.memory_map[0].base + 0x72);
        }

        /* Stopwatch counter (word read access only ?) */
        if (index == 0x0c)
        {
          /* relative SUB-CPU cycle counter */
          u32 cycles = (m68k.cycles * SCYCLES_PER_LINE) / MCYCLES_PER_LINE;

          /* cycle-accurate counter value */
          return (scd.regs[0x0c>>1].w + ((cycles - scd.stopwatch) / TIMERS_SCYCLES_RATIO)) & 0xfff;
        }

        /* default registers */
        if (index < 0x30)
        {
          /* SUB-CPU communication words */
          if (index >= 0x20)
          {
            m68k_poll_detect(index - 0x10);
          }
          
          return scd.regs[index >> 1].w;
        }
      }

      /* invalid address */
      return m68k_read_bus_16(address); 
    }

    case 0x30:  /* TIME */
    {
      if (cart.hw.time_r)
      {
        return cart.hw.time_r(address);
      }
      return m68k_read_bus_16(address); 
    }

    case 0x50:  /* SVP */
    {
      if ((address & 0xFD) == 0)
      {
        return svp.ssp1601.gr[SSP_XST].b.h;
      }

      if ((address & 0xFF) == 4)
      {
        u32 data = svp.ssp1601.gr[SSP_PM0].b.h;
        svp.ssp1601.gr[SSP_PM0].b.h &= ~1;
        return data;
      }

      return m68k_read_bus_16(address);
    }

    case 0x10:  /* MEMORY MODE */
    case 0x12:  /* Z80 RESET */
    case 0x13:  /* unknown */
    case 0x40:  /* TMSS */
    case 0x41:  /* BOOT ROM */
    case 0x44:  /* RADICA */
    {
      return m68k_read_bus_16(address);
    }

    default:  /* Invalid address */
    {
      return m68k_lockup_r_16(address);
    }
  }
}

void ctrl_io_write_byte(u32 address, u32 data)
{
  switch ((address >> 8) & 0xFF)
  {
    case 0x00:  /* I/O chip */
    {
      if ((address & 0xE1) == 0x01)
      {
        /* get /LWR only */
        io_68k_write((address >> 1) & 0x0F, data);
        return;
      }
      m68k_unused_8_w(address, data);
      return;
    }

    case 0x11:  /* Z80 BUSREQ */
    {
      if (!(address & 1))
      {
        gen_zbusreq_w(data & 1, m68k.cycles);
        return;
      }
      m68k_unused_8_w(address, data);
      return;
    }

    case 0x12:  /* Z80 RESET */
    {
      if (!(address & 1))
      {
        gen_zreset_w(data & 1, m68k.cycles);
        return;
      }
      m68k_unused_8_w(address, data);
      return;
    }

    case 0x20:  /* MEGA-CD */
    {
version(LOG_SCD) {
      error("[%d][%d]write byte CD register %X -> 0x%02X (%X)\n", v_counter, m68k.cycles, address, data, m68k.pc);
}
      if (system_hw == SYSTEM_MCD)
      {
        /* register index ($A12000-A1203F mirrored up to $A120FF) */
        switch (address & 0x3f)
        {
          case 0x00:  /* SUB-CPU interrupt */
          {
            /* IFL2 bit */
            if (data & 0x01)
            {
              /* level 2 interrupt enabled ? */
              if (scd.regs[0x32>>1].b.l & 0x04)
              {
                /* relative SUB-CPU cycle counter */
                u32 cycles = (m68k.cycles * SCYCLES_PER_LINE) / MCYCLES_PER_LINE;

                /* sync SUB-CPU with MAIN-CPU */
                if (!s68k.stopped && (s68k.cycles < cycles))
                {
                  s68k_run(cycles);
                }

                /* set IFL2 flag */
                scd.regs[0x00].b.h |= 0x01;

                /* trigger level 2 interrupt */
                scd.pending |= (1 << 2);

                /* update IRQ level */
                s68k_update_irq((scd.pending & scd.regs[0x32>>1].b.l) >> 1);
              }
            }

            /* writing 0 does nothing */
            return;
          }

          case 0x01:  /* SUB-CPU control */
          {
            /* RESET bit */
            if (data & 0x01)
            {
              /* trigger reset on 0->1 transition */
              if (!(scd.regs[0x00].b.l & 0x01))
              {
                /* reset SUB-CPU */
                s68k_pulse_reset();
              }

              /* BUSREQ bit */
              if (data & 0x02)
              {
                /* SUB-CPU bus requested */
                s68k_pulse_halt();
              }
              else
              {
                /* SUB-CPU bus released */
                s68k_clear_halt();
              }
            }
            else
            {
              /* SUB-CPU is halted while !RESET is asserted */
              s68k_pulse_halt();
            }

            scd.regs[0x00].b.l = data;
            return;
          }

          case 0x02:  /* PRG-RAM Write Protection */
          {
            scd.regs[0x02>>1].b.h = data;
            return;
          }

          case 0x03:  /* Memory mode */
          {
            m68k_poll_sync(0x02);

            /* PRG-RAM 128k bank mapped to $020000-$03FFFF (resp. $420000-$43FFFF) */
            m68k.memory_map[scd.cartridge.boot + 0x02].base = scd.prg_ram + ((data & 0xc0) << 11);
            m68k.memory_map[scd.cartridge.boot + 0x03].base = m68k.memory_map[scd.cartridge.boot + 0x02].base + 0x10000;

            /* check current mode */
            if (scd.regs[0x03>>1].b.l & 0x04)
            {
              /* DMNA bit */
              if (data & 0x02)
              {
                /* writing 1 to DMNA in 1M mode will return Word-RAM to SUB-CPU in 2M mode */
                scd.dmna = 1;
              }
              else
              {
                /* writing 0 to DMNA in 1M mode actually set DMNA bit */
                data |= 0x02;

                /* update BK0-1 & DMNA bits */
                scd.regs[0x03>>1].b.l = (scd.regs[0x03>>1].b.l & ~0xc2) | (data & 0xc2);
                return;
              }
            }
            else
            {
              /* writing 0 in 2M mode does nothing */
              if (data & 0x02)
              {
                /* Word-RAM is assigned to SUB-CPU */
                scd.dmna = 1;

                /* clear RET bit */
                scd.regs[0x03>>1].b.l = (scd.regs[0x03>>1].b.l & ~0xc3) | (data & 0xc2);
                return;
              }
            }
             
            /* update BK0-1 bits */
            scd.regs[0x03>>1].b.l = (scd.regs[0x02>>1].b.l & ~0xc0) | (data & 0xc0);
            return;
          }

          case 0x0f:  /* SUB-CPU communication flags, normally read-only (Space Ace, Dragon's Lair) */
          {
            /* ROL8 operation */
            data = (data << 1) | ((data >> 7) & 1);
          }

          case 0x0e:  /* MAIN-CPU communication flags */
          {
            m68k_poll_sync(0x0e);
            scd.regs[0x0e>>1].b.h = data;
            return;
          }

          default:
          {
            /* MAIN-CPU communication words */
            if ((address & 0x30) == 0x10)
            {
              m68k_poll_sync(address & 0x1e);

              /* register LSB */
              if (address & 1)
              {
                scd.regs[(address >> 1) & 0xff].b.l = data;
                return;
              }

              /* register MSB */
              scd.regs[(address >> 1) & 0xff].b.h = data;
              return;
            }

            /* invalid address */
            m68k_unused_8_w(address, data);
            return;
          }
        }
      }

      m68k_unused_8_w(address, data);
      return;
    }

    case 0x30:  /* TIME */
    {
      cart.hw.time_w(address, data);
      return;
    }

    case 0x41:  /* BOOT ROM */
    {
      if ((config.bios & 1) && (address & 1))
      {
        gen_bankswitch_w(data & 1);
        return;
      }
      m68k_unused_8_w(address, data);
      return;
    }

    case 0x10:  /* MEMORY MODE */
    case 0x13:  /* unknown */
    case 0x40:  /* TMSS */
    case 0x44:  /* RADICA */
    case 0x50:  /* SVP */
    {
      m68k_unused_8_w(address, data);
      return;
    }

    default:  /* Invalid address */
    {
      m68k_lockup_w_8(address, data);
      return;
    }
  }
}

void ctrl_io_write_word(u32 address, u32 data)
{
  switch ((address >> 8) & 0xFF)
  {
    case 0x00:  /* I/O chip */
    {
      if (!(address & 0xE0))
      {
        io_68k_write((address >> 1) & 0x0F, data & 0xFF);
        return;
      }
      m68k_unused_16_w(address, data);
      return;
    }

    case 0x11:  /* Z80 BUSREQ */
    {
      gen_zbusreq_w((data >> 8) & 1, m68k.cycles);
      return;
    }

    case 0x12:  /* Z80 RESET */
    {
      gen_zreset_w((data >> 8) & 1, m68k.cycles);
      return;
    }

    case 0x20:  /* MEGA-CD */
    {
version(LOG_SCD) {
      error("[%d][%d]write word CD register %X -> 0x%04X (%X)\n", v_counter, m68k.cycles, address, data, m68k.pc);
}
      if (system_hw == SYSTEM_MCD)
      {
        /* register index ($A12000-A1203F mirrored up to $A120FF) */
        switch (address & 0x3e)
        {
          case 0x00:  /* SUB-CPU interrupt & control */
          {
            /* RESET bit */
            if (data & 0x01)
            {
              /* trigger reset on 0->1 transition */
              if (!(scd.regs[0x00].b.l & 0x01))
              {
                /* reset SUB-CPU */
                s68k_pulse_reset();
              }

              /* BUSREQ bit */
              if (data & 0x02)
              {
                /* SUB-CPU bus requested */
                s68k_pulse_halt();
              }
              else
              {
                /* SUB-CPU bus released */
                s68k_clear_halt();
              }
            }
            else
            {
              /* SUB-CPU is halted while !RESET is asserted */
              s68k_pulse_halt();
            }

            /* IFL2 bit */
            if (data & 0x100)
            {
              /* level 2 interrupt enabled ? */
              if (scd.regs[0x32>>1].b.l & 0x04)
              {
                /* set IFL2 flag */
                scd.regs[0x00].b.h |= 0x01;

                /* trigger level 2 interrupt */
                scd.pending |= (1 << 2);

                /* update IRQ level */
                s68k_update_irq((scd.pending & scd.regs[0x32>>1].b.l) >> 1);
              }
            }

            /* update LSB only */
            scd.regs[0x00].b.l = data & 0xff;
            return;
          }

          case 0x02:  /* Memory Mode */
          {
            m68k_poll_sync(0x02);

            /* PRG-RAM 128k bank mapped to $020000-$03FFFF (resp. $420000-$43FFFF) */
            m68k.memory_map[scd.cartridge.boot + 0x02].base = scd.prg_ram + ((data & 0xc0) << 11);
            m68k.memory_map[scd.cartridge.boot + 0x03].base = m68k.memory_map[scd.cartridge.boot + 0x02].base + 0x10000;

            /* check current mode */
            if (scd.regs[0x03>>1].b.l & 0x04)
            {
              /* DMNA bit */
              if (data & 0x02)
              {
                /* writing 1 to DMNA in 1M mode will return Word-RAM to SUB-CPU in 2M mode */
                scd.dmna = 1;
              }
              else
              {
                /* writing 0 to DMNA in 1M mode actually set DMNA bit */
                data |= 0x02;

                /* update WP0-7, BK0-1 & DMNA bits */
                scd.regs[0x02>>1].w = (scd.regs[0x02>>1].w & ~0xffc2) | (data & 0xffc2);
                return;
              }
            }
            else
            {
              /* writing 0 in 2M mode does nothing */
              if (data & 0x02)
              {
                /* Word-RAM is assigned to SUB-CPU */
                scd.dmna = 1;

                /* clear RET bit */
                scd.regs[0x02>>1].w = (scd.regs[0x02>>1].w & ~0xffc3) | (data & 0xffc2);
                return;
              }
            }
             
            /* update WP0-7 & BK0-1 bits */
            scd.regs[0x02>>1].w = (scd.regs[0x02>>1].w & ~0xffc0) | (data & 0xffc0);
            return;
          }

          case 0x06:  /* H-INT vector (word access only ?) */
          {
            *cast(u16 *)(m68k.memory_map[0].base + 0x72) = data;
            return;
          }

          case 0x0e:  /* MAIN-CPU communication flags */
          {
            m68k_poll_sync(0x0e);

            /* LSB is read-only (Mortal Kombat) */
            scd.regs[0x0e>>1].b.h = data;
            return;
          }

          default:
          {
            /* MAIN-CPU communication words */
            if ((address & 0x30) == 0x10)
            {
              m68k_poll_sync(address & 0x1e);
              scd.regs[(address >> 1) & 0xff].w = data;
              return;
            }

            /* invalid address */
            m68k_unused_16_w (address, data);
            return;
          }
        }
      }

      m68k_unused_16_w (address, data);
      return;
    }

    case 0x30:  /* TIME */
    {
      cart.hw.time_w(address, data);
      return;
    }

    case 0x40:  /* TMSS */
    {
      if (config.bios & 1)
      {
        gen_tmss_w(address & 3, data);
        return;
      }
      m68k_unused_16_w(address, data);
      return;
    }

    case 0x50:  /* SVP */
    {
      if (!(address & 0xFD))
      {
        svp.ssp1601.gr[SSP_XST].b.h = data;
        svp.ssp1601.gr[SSP_PM0].b.h |= 2;
        svp.ssp1601.emu_status &= ~SSP_WAIT_PM0;
        return;
      }
      m68k_unused_16_w(address, data);
      return;
    }

    case 0x10:  /* MEMORY MODE */
    case 0x13:  /* unknown */
    case 0x41:  /* BOOT ROM */
    case 0x44:  /* RADICA */
    {
      m68k_unused_16_w (address, data);
      return;
    }
            
    default:  /* Invalid address */
    {
      m68k_lockup_w_16 (address, data);
      return;
    }
  }
}


/*--------------------------------------------------------------------------*/
/* VDP                                                                      */
/*--------------------------------------------------------------------------*/

u32 vdp_read_byte(u32 address)
{
  switch (address & 0xFD)
  {
    case 0x00:  /* DATA */
    {
      return (vdp_68k_data_r() >> 8);
    }

    case 0x01:  /* DATA */
    {
      return (vdp_68k_data_r() & 0xFF);
    }

    case 0x04:  /* CTRL */
    {
      u32 data = (vdp_68k_ctrl_r(m68k.cycles) >> 8) & 3;

      /* Unused bits return prefetched bus data */
      address = m68k.pc;
      data |= (READ_BYTE(m68k.memory_map[((address)>>16)&0xff].base, (address) & 0xffff) & 0xFC);

      return data;
    }

    case 0x05:  /* CTRL */
    {
      return (vdp_68k_ctrl_r(m68k.cycles) & 0xFF);
    }

    case 0x08:  /* HVC */
    case 0x0C:
    {
      return (vdp_hvc_r(m68k.cycles) >> 8);
    }

    case 0x09:  /* HVC */
    case 0x0D:
    {
      return (vdp_hvc_r(m68k.cycles) & 0xFF);
    }

    case 0x18:  /* Unused */
    case 0x19:
    case 0x1C:
    case 0x1D:
    {
      return m68k_read_bus_8(address);
    }

    default:    /* Invalid address */
    {
      return m68k_lockup_r_8(address);
    }
  }
}

u32 vdp_read_word(u32 address)
{
  switch (address & 0xFC)
  {
    case 0x00:  /* DATA */
    {
      return vdp_68k_data_r();
    }

    case 0x04:  /* CTRL */
    {
      u32 data = vdp_68k_ctrl_r(m68k.cycles) & 0x3FF;

      /* Unused bits return prefetched bus data */
      address = m68k.pc;
      data |= (*cast(u16 *)(m68k.memory_map[((address)>>16)&0xff].base + ((address) & 0xffff)) & 0xFC00);

      return data;
    }

    case 0x08:  /* HVC */
    case 0x0C:
    {
      return vdp_hvc_r(m68k.cycles);
    }

    case 0x18:  /* Unused */
    case 0x1C:
    {
      return m68k_read_bus_16(address);
    }

    default:    /* Invalid address */
    {
      return m68k_lockup_r_16(address);
    }
  }
}

void vdp_write_byte(u32 address, u32 data)
{
  switch (address & 0xFC)
  {
    case 0x00:  /* Data port */
    {
      vdp_68k_data_w(data << 8 | data);
      return;
    }

    case 0x04:  /* Control port */
    {
      vdp_68k_ctrl_w(data << 8 | data);
      return;
    }

    case 0x10:  /* PSG */
    case 0x14:
    {
      if (address & 1)
      {
        SN76489_Write(m68k.cycles, data);
        return;
      }
      m68k_unused_8_w(address, data);
      return;
    }

    case 0x18: /* Unused */
    {
      m68k_unused_8_w(address, data);
      return;
    }

    case 0x1C:  /* TEST register */
    {
      vdp_test_w(data << 8 | data);
      return;
    }

    default:  /* Invalid address */
    {
      m68k_lockup_w_8(address, data);
      return;
    }
  }
}

void vdp_write_word(u32 address, u32 data)
{
  switch (address & 0xFC)
  {
    case 0x00:  /* DATA */
    {
      vdp_68k_data_w(data);
      return;
    }

    case 0x04:  /* CTRL */
    {
      vdp_68k_ctrl_w(data);
      return;
    }

    case 0x10:  /* PSG */
    case 0x14:
    {
      SN76489_Write(m68k.cycles, data & 0xFF);
      return;
    }

    case 0x18:  /* Unused */
    {
      m68k_unused_16_w(address, data);
      return;
    }
    
    case 0x1C:  /* Test register */
    {
      vdp_test_w(data);
      return;
    }

    default:  /* Invalid address */
    {
      m68k_lockup_w_16 (address, data);
      return;
    }
  }
}


/*--------------------------------------------------------------------------*/
/* PICO (incomplete)                                                        */
/*--------------------------------------------------------------------------*/

u32 pico_read_byte(u32 address)
{
  switch (address & 0xFF)
  {
    case 0x01:  /* VERSION register */
    {
      return (region_code >> 1);
    }

    case 0x03:  /* IO register */
    {
      return ~input.pad[0];
    }

    case 0x05:  /* PEN X coordinate (MSB) */
    {
      return (input.analog[0][0] >> 8);
    }

    case 0x07:  /* PEN X coordinate (LSB) */
    {
      return (input.analog[0][0] & 0xFF);
    }

    case 0x09:  /* PEN Y coordinate (MSB) */
    {
      return (input.analog[0][1] >> 8);
    }

    case 0x0B:  /* PEN Y coordinate (LSB) */
    {
      return (input.analog[0][1] & 0xFF);
    }

    case 0x0D:  /* PAGE register */
    {
      return (1 << pico_current) - 1;
    }

    case 0x10:  /* ADPCM data registers (TODO) */
    case 0x11:
    {
      return 0xff;
    }

    case 0x12:  /* ADPCM control registers (TODO) */
    {
      return 0x80;
    }

    default:
    {
      return m68k_read_bus_8(address);
    }
  }
}

u32 pico_read_word(u32 address)
{
  return (pico_read_byte(address | 1) | (pico_read_byte(address) << 8));
}
