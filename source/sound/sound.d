/***************************************************************************************
 *  Genesis Plus
 *  Sound Hardware
 *
 *  Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003  Charles Mac Donald (original code)
 *  Copyright (C) 2007-2012  Eke-Eke (Genesis Plus GX)
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
import blip_buf;

/* FM output buffer (large enough to hold a whole frame at original chips rate) */
static int[1080 * 2] fm_buffer;
static int[2] fm_last;
static int* fm_ptr;

/* Cycle-accurate FM samples */
static u32 fm_cycles_ratio;
static u32 fm_cycles_start;
static u32 fm_cycles_count;

/* YM chip function pointers */
static void function() YM_Reset;
static void function(int* buffer, int length) YM_Update;
static void function(u32 a, u32 v) YM_Write;

/* Run FM chip until required M-cycles */
void fm_update(u32 cycles)
{
  if (cycles > fm_cycles_count)
  {
    /* number of samples to run */
    u32 samples = (cycles - fm_cycles_count + fm_cycles_ratio - 1) / fm_cycles_ratio;

    /* run FM chip to sample buffer */
    YM_Update(fm_ptr, samples);

    /* update FM buffer pointer */
    fm_ptr += (samples << 1);

    /* update FM cycle counter */
    fm_cycles_count += samples * fm_cycles_ratio;
  }
}

void sound_init()
{
  /* Initialize FM chip */
  if ((system_hw & SYSTEM_PBC) == SYSTEM_MD)
  {
    /* YM2612 */
    YM2612Init();
    YM2612Config(config.dac_bits);
    YM_Reset = YM2612ResetChip;
    YM_Update = YM2612Update;
    YM_Write = YM2612Write;

    /* chip is running a VCLK / 144 = MCLK / 7 / 144 */
    fm_cycles_ratio = 144 * 7;
  }
  else
  {
    /* YM2413 */
    YM2413Init();
    YM_Reset = YM2413ResetChip;
    YM_Update = YM2413Update;
    YM_Write = YM2413Write;

    /* chip is running a ZCLK / 72 = MCLK / 15 / 72 */
    fm_cycles_ratio = 72 * 15;
  }

  /* Initialize PSG chip */
  SN76489_Config(0, config.psg_preamp, config.psgBoostNoise, 0xff);
}

void sound_reset()
{
  /* reset sound chips */
  YM_Reset();
  SN76489_Reset();

  /* reset FM buffer ouput */
  fm_last[0] = fm_last[1] = 0;

  /* reset FM buffer pointer */
  fm_ptr = fm_buffer;
  
  /* reset FM cycle counters */
  fm_cycles_start = fm_cycles_count = 0;
}

int sound_update(u32 cycles)
{
  int delta, preamp, time, l, r;
  int* ptr;

  /* Run PSG & FM chips until end of frame */
  SN76489_Update(cycles);
  fm_update(cycles);

	/* FM output pre-amplification */
  preamp = config.fm_preamp;

  /* FM frame initial timestamp */
  time = fm_cycles_start;

  /* Restore last FM outputs from previous frame */
  l = fm_last[0];
  r = fm_last[1];

  /* FM buffer start pointer */
  ptr = fm_buffer;

  /* flush FM samples */
  if (config.hq_fm)
  {
    /* high-quality Band-Limited synthesis */
    do
    {
      /* left channel */
      delta = ((*ptr++ * preamp) / 100) - l;
      l += delta;
      blip_add_delta(snd.blips[0][0], time, delta);
      
      /* right channel */
      delta = ((*ptr++ * preamp) / 100) - r;
      r += delta;
      blip_add_delta(snd.blips[0][1], time, delta);

      /* increment time counter */
      time += fm_cycles_ratio;
    }
    while (time < cast(int) cycles);
  }
  else
  {
    /* faster Linear Interpolation */
    do
    {
      /* left channel */
      delta = ((*ptr++ * preamp) / 100) - l;
      l += delta;
      blip_add_delta_fast(snd.blips[0][0], time, delta);
      
      /* right channel */
      delta = ((*ptr++ * preamp) / 100) - r;
      r += delta;
      blip_add_delta_fast(snd.blips[0][1], time, delta);

      /* increment time counter */
      time += fm_cycles_ratio;
    }
    while (time < cast(int) cycles);
  }

  /* reset FM buffer pointer */
  fm_ptr = fm_buffer;

  /* save last FM output for next frame */
  fm_last[0] = l;
  fm_last[1] = r;

  /* adjust FM cycle counters for next frame */
  fm_cycles_count = fm_cycles_start = time - cycles;
	
  /* end of blip buffers time frame */
  blip_end_frame(snd.blips[0][0], cycles);
  blip_end_frame(snd.blips[0][1], cycles);

  /* return number of available samples */
  return blip_samples_avail(snd.blips[0][0]);
}

int sound_context_save(u8 *state)
{
  int bufferptr = 0;
  
  if ((system_hw & SYSTEM_PBC) == SYSTEM_MD)
  {
    bufferptr = YM2612SaveContext(state);
  }
  else
  {
    save_param(&bufferptr, state, YM2413GetContextPtr(),YM2413GetContextSize());
  }

  save_param(&bufferptr, state, SN76489_GetContextPtr(),SN76489_GetContextSize());

  save_param(&bufferptr, state, &fm_cycles_start,sizeof(fm_cycles_start));

  return bufferptr;
}

int sound_context_load(u8 *state)
{
  int bufferptr = 0;

  if ((system_hw & SYSTEM_PBC) == SYSTEM_MD)
  {
    bufferptr = YM2612LoadContext(state);
  }
  else
  {
    load_param(&bufferptr, state, YM2413GetContextPtr(),YM2413GetContextSize());
  }

  load_param(&bufferptr, state, SN76489_GetContextPtr(),SN76489_GetContextSize());

  load_param(&bufferptr, state, &fm_cycles_start,sizeof(fm_cycles_start));
  fm_cycles_count = fm_cycles_start;

  return bufferptr;
}

void fm_reset(u32 cycles)
{
  /* synchronize FM chip with CPU */
  fm_update(cycles);

  /* reset FM chip */
  YM_Reset();
}

void fm_write(u32 cycles, u32 address, u32 data)
{
  /* synchronize FM chip with CPU (on data port write only) */
  if (address & 1)
  {
    fm_update(cycles);
  }
  
  /* write FM register */
  YM_Write(address, data);
}

u32 fm_read(u32 cycles, u32 address)
{
	assert(cycles == cycles);
	assert(address == address);
  /* synchronize FM chip with CPU */
  fm_update(cycles);

  /* read FM status (YM2612 only) */
  return YM2612Read();
}
