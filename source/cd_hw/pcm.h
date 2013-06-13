/***************************************************************************************
 *  Genesis Plus
 *  PCM sound chip (315-5476A) (RF5C164 compatible)
 *
 *  Copyright (C) 2012  Eke-Eke (Genesis Plus GX)
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
#ifndef _CD_PCM_
#define _CD_PCM_

#include "blip_buf.h"

/* PCM channel */
typedef struct
{
  u32 addr;  /* current Wave RAM address (16.11 fixed point) */
  u32 st;    /* Wave RAM start address (16.11 fixed point) */
  reg16_t ls;   /* Wave RAM loop address ($0000-$ffff) */
  reg16_t fd;   /* Wave RAM address increment (5.11 fixed point) */
  u8 env;    /* enveloppe multiplier */
  u8 pan;    /* stereo panning */
} chan_t;

/* PCM sound chip */
typedef struct
{
  chan_t chan[8];     /* PCM channels 1-8 */
  s16 out[2];       /* previous PCM stereo output */
  u8 *bank;        /* external RAM bank pointer */
  u8 enabled;      /* PCM chip ON/OFF status */
  u8 status;       /* channels ON/OFF status */
  u8 index;        /* current channel index */
  u8 ram[0x10000]; /* 64k external RAM */
  u32 cycles;
} pcm_t;

/* Function prototypes */
extern void pcm_init(blip_t* left, blip_t* right);
extern void pcm_reset();
extern s32 pcm_context_save(u8 *state);
extern s32 pcm_context_load(u8 *state);
extern void pcm_update(u32 samples);
extern void pcm_write(u32 address, u8 data);
extern u8 pcm_read(u32 address);
extern void pcm_ram_dma_w(u32 words);

#endif
