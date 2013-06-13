/***************************************************************************************
 *  Genesis Plus
 *  CD graphics processor
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
#ifndef _CD_GFX_
#define _CD_GFX_

#define gfx scd.gfx_hw

typedef struct
{
  u32 cycles;                    /* current cycles count for graphics operation */
  u32 cyclesPerLine;             /* current graphics operation timings */
  u32 dotMask;                   /* stamp map size mask */
  u16 *tracePtr;                 /* trace vector pointer */
  u16 *mapPtr;                   /* stamp map table base address */
  u8 stampShift;                 /* stamp pixel shift value (related to stamp size) */
  u8 mapShift;                   /* stamp map table shift value (related to stamp map size) */
  u16 bufferOffset;              /* image buffer column offset */
  u32 bufferStart;               /* image buffer start index */
  u16 lut_offset[0x8000];        /* Cell Image -> WORD-RAM offset lookup table (1M Mode) */
  u8 lut_prio[4][0x100][0x100];  /* WORD-RAM data writes priority lookup table */
  u8 lut_pixel[0x200];           /* Graphics operation dot offset lookup table */
  u8 lut_cell[0x100];            /* Graphics operation stamp offset lookup table */
} gfx_t;


/***************************************************************/
/*          WORD-RAM DMA interfaces (1M & 2M modes)            */
/***************************************************************/
extern void word_ram_0_dma_w(u32 words);
extern void word_ram_1_dma_w(u32 words);
extern void word_ram_2M_dma_w(u32 words);

/***************************************************************/
/*          WORD-RAM 0 & 1 CPU interfaces (1M mode)            */
/***************************************************************/
extern u32 word_ram_0_read16(u32 address);
extern u32 word_ram_1_read16(u32 address);
extern void word_ram_0_write16(u32 address, u32 data);
extern void word_ram_1_write16(u32 address, u32 data);
extern u32 word_ram_0_read8(u32 address);
extern u32 word_ram_1_read8(u32 address);
extern void word_ram_0_write8(u32 address, u32 data);
extern void word_ram_1_write8(u32 address, u32 data);

/***************************************************************/
/*     WORD-RAM 0 & 1 DOT image SUB-CPU interface (1M mode)    */
/***************************************************************/
extern u32 dot_ram_0_read16(u32 address);
extern u32 dot_ram_1_read16(u32 address);
extern void dot_ram_0_write16(u32 address, u32 data);
extern void dot_ram_1_write16(u32 address, u32 data);
extern u32 dot_ram_0_read8(u32 address);
extern u32 dot_ram_1_read8(u32 address);
extern void dot_ram_0_write8(u32 address, u32 data);
extern void dot_ram_1_write8(u32 address, u32 data);


/***************************************************************/
/*    WORD-RAM 0 & 1 CELL image MAIN-CPU interface (1M mode)   */
/***************************************************************/
extern u32 cell_ram_0_read16(u32 address);
extern u32 cell_ram_1_read16(u32 address);
extern void cell_ram_0_write16(u32 address, u32 data);
extern void cell_ram_1_write16(u32 address, u32 data);
extern u32 cell_ram_0_read8(u32 address);
extern u32 cell_ram_1_read8(u32 address);
extern void cell_ram_0_write8(u32 address, u32 data);
extern void cell_ram_1_write8(u32 address, u32 data);


/***************************************************************/
/*            Rotation / Scaling operation (2M mode)           */
/***************************************************************/
extern void gfx_init(void);
extern void gfx_reset(void);
extern int gfx_context_save(u8 *state);
extern int gfx_context_load(u8 *state);
extern void gfx_start(u32 base, int cycles);
extern void gfx_update(int cycles);

#endif
