/***************************************************************************************
 *  Genesis Plus
 *  Virtual System emulation
 *
 *  Support for "Genesis", "Genesis + CD" & "Master System" modes
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

#ifndef _SYSTEM_H_
#define _SYSTEM_H_

#include "blip_buf.h"

/* Supported hardware models */
#define SYSTEM_SG         0x10
#define SYSTEM_MARKIII    0x11
#define SYSTEM_SMS        0x20
#define SYSTEM_SMS2       0x21
#define SYSTEM_GG         0x40
#define SYSTEM_GGMS       0x41
#define SYSTEM_MD         0x80
#define SYSTEM_PBC        0x81
#define SYSTEM_PICO       0x82
#define SYSTEM_MCD        0x84

/* NTSC & PAL Master Clock frequencies */
#define MCLOCK_NTSC 53693175
#define MCLOCK_PAL  53203424

/* Number of M-Cycles executed per line */
#define MCYCLES_PER_LINE  3420

/* Horizontal timing offsets when running in Z80 mode */
#define SMS_CYCLE_OFFSET  520 
#define PBC_CYCLE_OFFSET  550 

typedef struct
{
  u8 *data;      /* Bitmap data */
  s32 width;        /* Bitmap width */
  s32 height;       /* Bitmap height */
  s32 pitch;        /* Bitmap pitch */
  struct
  {
    s32 x;          /* X offset of viewport within bitmap */
    s32 y;          /* Y offset of viewport within bitmap */
    s32 w;          /* Width of viewport */
    s32 h;          /* Height of viewport */
    s32 ow;         /* Previous width of viewport */
    s32 oh;         /* Previous height of viewport */
    s32 changed;    /* 1= Viewport width or height have changed */
  } viewport;
} t_bitmap;

typedef struct
{
  s32 sample_rate;      /* Output Sample rate (8000-48000) */
  double frame_rate;    /* Output Frame rate (usually 50 or 60 frames per second) */
  s32 enabled;          /* 1= sound emulation is enabled */
  blip_t* blips[3][2];  /* Blip Buffer resampling */
} t_snd;


/* Global variables */
extern t_bitmap bitmap;
extern t_snd snd;
extern u32 mcycles_vdp;
extern s16 SVP_cycles; 
extern u8 system_hw;
extern u8 system_bios;
extern u32 system_clock;

/* Function prototypes */
extern s32 audio_init(s32 samplerate, double framerate);
extern void audio_reset();
extern void audio_shutdown();
extern s32 audio_update(s16 *buffer);
extern void audio_set_equalizer();
extern void system_init();
extern void system_reset();
extern void system_frame_gen(s32 do_skip);
extern void system_frame_scd(s32 do_skip);
extern void system_frame_sms(s32 do_skip);

#endif /* _SYSTEM_H_ */

