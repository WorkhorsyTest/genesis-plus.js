/* md_ntsc 0.1.2. http://www.slack.net/~ant/ */

/* Modified for use with Genesis Plus GX -- EkeEke */

/* Copyright (C) 2006 Shay Green. This module is free software; you
can redistribute it and/or modify it under the terms of the GNU Lesser
General Public License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version. This
module is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
details. You should have received a copy of the GNU Lesser General Public
License along with this module; if not, write to the Free Software Foundation,
Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA */

import shared;
import md_ntsc;
import md_ntsc_config;
import md_ntsc_impl;

alias u32 md_ntsc_rgb_t;

/* Image parameters, ranging from -1.0 to 1.0. Actual internal values shown
in parenthesis and should remain fairly stable in future versions. */
struct md_ntsc_setup_t {
  /* Basic parameters */
  double hue;        /* -1 = -180 degrees     +1 = +180 degrees */
  double saturation; /* -1 = grayscale (0.0)  +1 = oversaturated colors (2.0) */
  double contrast;   /* -1 = dark (0.5)       +1 = light (1.5) */
  double brightness; /* -1 = dark (0.5)       +1 = light (1.5) */
  double sharpness;  /* edge contrast enhancement/blurring */

  /* Advanced parameters */
  double gamma;      /* -1 = dark (1.5)       +1 = light (0.5) */
  double resolution; /* image resolution */
  double artifacts;  /* artifacts caused by color changes */
  double fringing;   /* color artifacts caused by brightness changes */
  double bleed;      /* color bleed (color resolution reduction) */
  float const* decoder_matrix; /* optional RGB decoder matrix, 6 elements */

  u8* palette_out;  /* optional RGB palette out, 3 bytes per color */
}

const int md_ntsc_palette_size = 512;

/* Initializes and adjusts parameters. Can be called multiple times on the same
md_ntsc_t object. Can pass NULL for either parameter. */
typedef struct md_ntsc_t md_ntsc_t;
void md_ntsc_init( md_ntsc_t* ntsc, md_ntsc_setup_t const* setup );

/* Filters one row of pixels. Input pixel format is set by MD_NTSC_IN_FORMAT
and output RGB depth is set by MD_NTSC_OUT_DEPTH. Both default to 16-bit RGB.
In_row_width is the number of pixels to get to the next input row. */
void md_ntsc_blit( md_ntsc_t const* ntsc, MD_NTSC_IN_T const* table, u8* input,
    int in_width, int vline);

static md_ntsc_rgb_t* MD_NTSC_RGB16(md_ntsc_t const* ntsc, MD_NTSC_IN_T n);
static void MD_NTSC_CLAMP_(md_ntsc_rgb_t io, s32 shift);

/* Interface for user-defined custom blitters */

const int md_ntsc_in_chunk  = 4; /* number of input pixels read per chunk */
const int md_ntsc_out_chunk = 8; /* number of output pixels generated per chunk */
const int md_ntsc_black     = 0; /* palette index for black */

struct MDBlitData {
  md_ntsc_rgb_t raw_;
  u32 md_pixel0_;
  u32 md_pixel1_;
  u32 md_pixel2_;
  u32 md_pixel3_;
  md_ntsc_rgb_t* kernel0;
  md_ntsc_rgb_t* kernel1;
  md_ntsc_rgb_t* kernel2;
  md_ntsc_rgb_t* kernel3;
  md_ntsc_rgb_t* kernelx0;
  md_ntsc_rgb_t* kernelx1;
  md_ntsc_rgb_t* kernelx2;
  md_ntsc_rgb_t* kernelx3;
  md_ntsc_out_t* line_out;
}

/* Begin input pixel */
static void MD_NTSC_COLOR_IN_(MDBlitData* data, s32 index, MD_NTSC_IN_T color, md_ntsc_t const* table) {
  u32 color_;
  switch(index) {
    case 0:
      data.kernelx0 = data.kernel0;
      data.kernel0 = (color_ = color, MD_NTSC_IN_FORMAT( table, color_ ));
      break;
    case 1:
      data.kernelx1 = data.kernel1;
      data.kernel1 = (color_ = color, MD_NTSC_IN_FORMAT( table, color_ ));
      break;
    case 2:
      data.kernelx2 = data.kernel2;
      data.kernel2 = (color_ = color, MD_NTSC_IN_FORMAT( table, color_ ));
      break;
    case 3:
      data.kernelx3 = data.kernel3;
      data.kernel3 = (color_ = color, MD_NTSC_IN_FORMAT( table, color_ ));
      break;
  }
}

static void MD_NTSC_COLOR_IN(MDBlitData* data, s32 index, md_ntsc_t const* ntsc, MD_NTSC_IN_T color) {
  MD_NTSC_COLOR_IN_(data, index, color, ntsc);
}

/* x is always zero except in snes_ntsc library */
static md_ntsc_out_t MD_NTSC_RGB_OUT_(MDBlitData* data, s32 x) {
    return (data.raw_>>(13-x)& 0xF800)|(data.raw_>>(8-x)&0x07E0)|(data.raw_>>(4-x)&0x001F);
}

/* Generate output pixel */
static void MD_NTSC_RGB_OUT(MDBlitData* data, s32 x) {
  data.raw_ =
    data.kernel0  [x+ 0] + data.kernel1  [(x+6)%8+16] + data.kernel2  [(x+4)%8  ] + data.kernel3  [(x+2)%8+16] +
    data.kernelx0 [x+ 8] + data.kernelx1 [(x+6)%8+24] + data.kernelx2 [(x+4)%8+8] + data.kernelx3 [(x+2)%8+24];
  MD_NTSC_CLAMP_(data.raw_, 0);
  *data.line_out = MD_NTSC_RGB_OUT_(data, 0);
  data.line_out++;
}


/* private */
const int md_ntsc_entry_size = 2 * 16;
struct md_ntsc_t {
  md_ntsc_rgb_t[md_ntsc_palette_size][md_ntsc_entry_size] table;
}

static md_ntsc_rgb_t* MD_NTSC_BGR9(md_ntsc_t const* ntsc, MD_NTSC_IN_T n) {
  return ntsc.table [n & 0x1FF];
}

static md_ntsc_rgb_t* MD_NTSC_RGB16(md_ntsc_t const* ntsc, MD_NTSC_IN_T n) {
  return (md_ntsc_rgb_t*) ((char*) (ntsc).table +
  ((n << 9 & 0x3800) | (n & 0x0700) | (n >> 8 & 0x00E0)) *
  (md_ntsc_entry_size * sizeof (md_ntsc_rgb_t) / 32));
}

static md_ntsc_rgb_t* MD_NTSC_RGB15(md_ntsc_t const* ntsc, MD_NTSC_IN_T n) {
  return (md_ntsc_rgb_t*) ((char*) (ntsc).table +
  ((n << 8 & 0x1C00) | (n & 0x0380) | (n >> 8 & 0x0070)) *
  (md_ntsc_entry_size * sizeof (md_ntsc_rgb_t) / 16));
}

/* common ntsc macros */
const int md_ntsc_rgb_builder    = ((1L << 21) | (1 << 11) | (1 << 1));
const int md_ntsc_clamp_mask     = (md_ntsc_rgb_builder * 3 / 2);
const int md_ntsc_clamp_add      = (md_ntsc_rgb_builder * 0x101);
static void MD_NTSC_CLAMP_(md_ntsc_rgb_t io, s32 shift) {
  md_ntsc_rgb_t sub = (io) >> (9-(shift)) & md_ntsc_clamp_mask;
  md_ntsc_rgb_t clamp = md_ntsc_clamp_add - sub;
  io |= clamp;
  clamp -= sub;
  io &= clamp;
}

/* Video format presets */
const md_ntsc_setup_t md_ntsc_monochrome = { 0,-1, 0, 0,.2,  0, 0,-.2,-.2,-1, 0,  0 }; /* desaturated + artifacts */
const md_ntsc_setup_t md_ntsc_composite  = { 0, 0, 0, 0, 0,  0, 0,  0,  0, 0, 0,  0 }; /* color bleeding + artifacts */
const md_ntsc_setup_t md_ntsc_svideo     = { 0, 0, 0, 0, 0,  0,.2, -1, -1, 0, 0,  0 }; /* color bleeding only */
const md_ntsc_setup_t md_ntsc_rgb        = { 0, 0, 0, 0,.2,  0,.7, -1, -1,-1, 0,  0 }; /* crisp image */

const int alignment_count = 2;
const int burst_count     = 1;
const int rescale_in      = 1;
const int rescale_out     = 1;

const float artifacts_mid   = 0.40f;
const float fringing_mid    = 0.30f;
const int std_decoder_hue   = 0;

const int gamma_size        = 8;
const float artifacts_max   = 1.00f;
const int LUMA_CUTOFF       = 0.1974;


/* 2 input pixels -> 4 composite samples */
const pixel_info_t[alignment_count] md_ntsc_pixels = {
  { PIXEL_OFFSET( -4, -9 ), { 0.1f, 0.9f, 0.9f, 0.1f } },
  { PIXEL_OFFSET( -2, -7 ), { 0.1f, 0.9f, 0.9f, 0.1f } },
};

static void correct_errors( md_ntsc_rgb_t color, md_ntsc_rgb_t* out )
{
  u32 i;
  for ( i = 0; i < rgb_kernel_size / 4; i++ )
  {
    md_ntsc_rgb_t error = color -
        out [i    ] - out [i + 2    +16] - out [i + 4    ] - out [i + 6    +16] -
        out [i + 8] - out [(i+10)%16+16] - out [(i+12)%16] - out [(i+14)%16+16];
    CORRECT_ERROR(out, i, i + 6 + 16 );
    /*DISTRIBUTE_ERROR( 2+16, 4, 6+16 );*/
  }
}

void md_ntsc_init( md_ntsc_t* ntsc, md_ntsc_setup_t const* setup )
{
  int entry;
  init_t impl;
  if ( !setup )
    setup = &md_ntsc_composite;
  init( &impl, setup );

  for ( entry = 0; entry < md_ntsc_palette_size; entry++ )
  {
    float bb = impl.to_float [entry >> 6 & 7];
    float gg = impl.to_float [entry >> 3 & 7];
    float rr = impl.to_float [entry      & 7];

    float y, i, q = RGB_TO_YIQ(rr, gg, bb, &y, &i);

    int r, g, b;
    YIQ_TO_RGB(y, i, q, impl.to_rgb, &r, &g, &b);
    md_ntsc_rgb_t rgb = PACK_RGB( r, g, b );

    if ( setup.palette_out )
      RGB_PALETTE_OUT( rgb, &setup.palette_out [entry * 3] );

    if ( ntsc )
    {
      gen_kernel( &impl, y, i, q, ntsc.table [entry] );
      correct_errors( rgb, ntsc.table [entry] );
    }
  }
}

version(CUSTOM_BLITTER) {
void md_ntsc_blit( md_ntsc_t const* ntsc, MD_NTSC_IN_T const* table, u8* input,
                   int in_width, int vline)
{
  int const chunk_count = in_width / md_ntsc_in_chunk - 1;

  /* use palette entry 0 for unused pixels */
  MD_NTSC_IN_T border = table[0];

  MDBlitData blit_data;
  blit_data.md_pixel0_ = border;
  blit_data.kernel0  = MD_NTSC_IN_FORMAT( ntsc, blit_data.md_pixel0_ );
  blit_data.md_pixel1_ = table[*input++];
  blit_data.kernel1  = MD_NTSC_IN_FORMAT( ntsc, blit_data.md_pixel1_ );
  blit_data.md_pixel2_ = table[*input++];
  blit_data.kernel2  = MD_NTSC_IN_FORMAT( ntsc, blit_data.md_pixel2_ );
  blit_data.md_pixel3_ = table[*input++];
  blit_data.kernel3  = MD_NTSC_IN_FORMAT( ntsc, blit_data.md_pixel3_ );
  blit_data.kernelx1 = blit_data.kernel0;
  blit_data.kernelx2 = blit_data.kernel0;
  blit_data.kernelx3 = blit_data.kernel0;


  blit_data.line_out  = (md_ntsc_out_t*)(&bitmap.data[(vline * bitmap.pitch)]);

  int n;

  for ( n = chunk_count; n; --n )
  {
    /* order of input and output pixels must not be altered */
    MD_NTSC_COLOR_IN(&blit_data, 0, ntsc, table[*input++]);
    MD_NTSC_RGB_OUT(&blit_data, 0);
    MD_NTSC_RGB_OUT(&blit_data, 1);

    MD_NTSC_COLOR_IN(&blit_data, 1, ntsc, table[*input++]);
    MD_NTSC_RGB_OUT(&blit_data, 2);
    MD_NTSC_RGB_OUT(&blit_data, 3);

    MD_NTSC_COLOR_IN(&blit_data, 2, ntsc, table[*input++]);
    MD_NTSC_RGB_OUT(&blit_data, 4);
    MD_NTSC_RGB_OUT(&blit_data, 5);

    MD_NTSC_COLOR_IN(&blit_data, 3, ntsc, table[*input++]);
    MD_NTSC_RGB_OUT(&blit_data, 6);
    MD_NTSC_RGB_OUT(&blit_data, 7);
  }

  /* finish final pixels */
  MD_NTSC_COLOR_IN(&blit_data, 0, ntsc, table[*input++]);
  MD_NTSC_RGB_OUT(&blit_data, 0);
  MD_NTSC_RGB_OUT(&blit_data, 1);

  MD_NTSC_COLOR_IN(&blit_data, 1, ntsc, border);
  MD_NTSC_RGB_OUT(&blit_data, 2);
  MD_NTSC_RGB_OUT(&blit_data, 3);

  MD_NTSC_COLOR_IN(&blit_data, 2, ntsc, border);
  MD_NTSC_RGB_OUT(&blit_data, 4);
  MD_NTSC_RGB_OUT(&blit_data, 5);

  MD_NTSC_COLOR_IN(&blit_data, 3, ntsc, border);
  MD_NTSC_RGB_OUT(&blit_data, 6);
  MD_NTSC_RGB_OUT(&blit_data, 7);
}
}
