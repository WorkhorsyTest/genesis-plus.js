/***************************************************************************************
 *  Genesis Plus
 *  CD data controller (LC89510 compatible)
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
#ifndef _HW_CDC_
#define _HW_CDC_

#define cdc scd.cdc_hw

/* CDC hardware */
typedef struct
{
  u8 ifstat;
  u8 ifctrl;
  reg16_t dbc;
  reg16_t dac;
  reg16_t pt;
  reg16_t wa;
  u8 ctrl[2];
  u8 head[2][4];
  u8 stat[4];
  s32 cycles;
  void (*dma_w)(u32 words);  /* DMA transfer callback */
  u8 ram[0x4000 + 2352]; /* 16K external RAM (with one block overhead to handle buffer overrun) */
} cdc_t; 

/* Function prototypes */
extern void cdc_init(void);
extern void cdc_reset(void);
extern s32 cdc_context_save(u8 *state);
extern s32 cdc_context_load(u8 *state);
extern void cdc_dma_update(void);
extern s32 cdc_decoder_update(u32 header);
extern void cdc_reg_w(u8 data);
extern u8 cdc_reg_r(void);
extern u16 cdc_host_r(void);

#endif
