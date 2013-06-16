/***************************************************************************************
 *  Genesis Plus
 *  Video Display Processor (68k & Z80 CPU interface)
 *
 *  Support for SG-1000, Master System (315-5124 & 315-5246), Game Gear & Mega Drive VDP
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

#ifndef _VDP_H_
#define _VDP_H_

/* VDP context */
extern u8 reg[0x20];
extern u8 sat[0x400];
extern u8 vram[0x10000];
extern u8 cram[0x80];
extern u8 vsram[0x80];
extern u8 hint_pending;
extern u8 vint_pending;
extern u16 status;
extern u32 dma_length;

/* Global variables */
extern u16 ntab;
extern u16 ntbb;
extern u16 ntwb;
extern u16 satb;
extern u16 hscb;
extern u8 bg_name_dirty[0x800];
extern u16 bg_name_list[0x800];
extern u16 bg_list_index;
extern u8 hscroll_mask;
extern u8 playfield_shift;
extern u8 playfield_col_mask;
extern u16 playfield_row_mask;
extern u8 odd_frame;
extern u8 im2_flag;
extern u8 interlaced;
extern u8 vdp_pal;
extern u16 v_counter;
extern u16 vc_max;
extern u16 vscroll;
extern u16 lines_per_frame;
extern s32 fifo_write_cnt;
extern u32 fifo_lastwrite;
extern u32 hvc_latch;
extern const u8 *hctab;

/* Function pointers */
extern void (*vdp_68k_data_w)(u32 data);
extern void (*vdp_z80_data_w)(u32 data);
extern u32 (*vdp_68k_data_r)();
extern u32 (*vdp_z80_data_r)();

/* Function prototypes */
extern void vdp_init();
extern void vdp_reset();
extern int vdp_context_save(u8 *state);
extern int vdp_context_load(u8 *state);
extern void vdp_dma_update(u32 cycles);
extern void vdp_68k_ctrl_w(u32 data);
extern void vdp_z80_ctrl_w(u32 data);
extern void vdp_sms_ctrl_w(u32 data);
extern void vdp_tms_ctrl_w(u32 data);
extern u32 vdp_68k_ctrl_r(u32 cycles);
extern u32 vdp_z80_ctrl_r(u32 cycles);
extern u32 vdp_hvc_r(u32 cycles);
extern void vdp_test_w();
extern int vdp_68k_irq_ack();

#endif /* _VDP_H_ */
