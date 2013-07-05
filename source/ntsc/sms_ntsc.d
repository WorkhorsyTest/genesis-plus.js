/* sms_ntsc 0.2.3. http://www.slack.net/~ant/ */

/* Modified for use with Genesis Plus GX -- EkeEke */

/* Copyright (C) 2006-2007 Shay Green. This module is free software; you
can redistribute it and/or modify it under the terms of the GNU Lesser
General Public License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version. This
module is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
details. You should have received a copy of the GNU Lesser General Public
License along with this module; if not, write to the Free Software Foundation,
Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA */

import shared.d;
import sms_ntsc.d;
import sms_ntsc_config.d;
import sms_ntsc_impl.d;


alias u32 sms_ntsc_rgb_t;

/* Image parameters, ranging from -1.0 to 1.0. Actual internal values shown
in parenthesis and should remain fairly stable in future versions. */
struct sms_ntsc_setup_t
{
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

const int sms_ntsc_palette_size = 4096;

/* Initializes and adjusts parameters. Can be called multiple times on the same
sms_ntsc_t object. Can pass NULL for either parameter. */
struct sms_ntsc_t sms_ntsc_t {
}

/* Interface for user-defined custom blitters */

const int sms_ntsc_in_chunk    = 3; /* number of input pixels read per chunk */
const int sms_ntsc_out_chunk   = 7; /* number of output pixels generated per chunk */

struct SMSBlitData {
  sms_ntsc_rgb_t raw_;
  u32 sms_ntsc_pixel0_;
  u32 sms_ntsc_pixel1_;
  u32 sms_ntsc_pixel2_;
  sms_ntsc_rgb_t* kernel0;
  sms_ntsc_rgb_t* kernel1;
  sms_ntsc_rgb_t* kernel2;
  sms_ntsc_rgb_t* kernelx0;
  sms_ntsc_rgb_t* kernelx1;
  sms_ntsc_rgb_t* kernelx2;
  sms_ntsc_out_t* line_out;
}

/* Begins input pixel */
static void SMS_NTSC_COLOR_IN_(SMSBlitData* data, s32 index, SMS_NTSC_IN_T color, sms_ntsc_t const* table ) {
  u32 color_;
  switch(index) {
    case 0:
      data->kernelx0 = data->kernel0;
      data->kernel0 = (color_ = color, SMS_NTSC_IN_FORMAT( table, color_ ));
      break;
    case 1:
      data->kernelx1 = data->kernel1;
      data->kernel1 = (color_ = color, SMS_NTSC_IN_FORMAT( table, color_ ));
      break;
    case 2:
      data->kernelx2 = data->kernel2;
      data->kernel2 = (color_ = color, SMS_NTSC_IN_FORMAT( table, color_ ));
      break;
  }
}

static void SMS_NTSC_COLOR_IN(SMSBlitData* data, s32 in_index, sms_ntsc_t const* ntsc, SMS_NTSC_IN_T color_in ) {
  SMS_NTSC_COLOR_IN_(data, in_index, color_in, ntsc );
}

/* Generates output pixel */
static sms_ntsc_out_t SMS_NTSC_RGB_OUT_(SMSBlitData* data, s32 x) {
  return (data->raw_>>(13-x)& 0xF800)|(data->raw_>>(8-x)&0x07E0)|(data->raw_>>(4-x)&0x001F);
}

static void SMS_NTSC_RGB_OUT(SMSBlitData* data, s32 x) {
  data->raw_ =
    data->kernel0  [x       ] + data->kernel1  [(x+12)%7+14] + data->kernel2  [(x+10)%7+28] +
    data->kernelx0 [(x+7)%14] + data->kernelx1 [(x+ 5)%7+21] + data->kernelx2 [(x+ 3)%7+35];
  SMS_NTSC_CLAMP_( data->raw_, 0 );
  data->line_out = SMS_NTSC_RGB_OUT_( data, 0 );
  data->line_out++;
}

/* private */
const int sms_ntsc_entry_size = 3 * 14;
struct sms_ntsc_t {
  sms_ntsc_rgb_t[sms_ntsc_palette_size][sms_ntsc_entry_size] table;
}

static sms_ntsc_rgb_t* SMS_NTSC_BGR12(sms_ntsc_t const* ntsc, SMS_NTSC_IN_T n) {
  return ntsc->table [n & 0xFFF];
}

static sms_ntsc_rgb_t* SMS_NTSC_RGB16(sms_ntsc_t const* ntsc, SMS_NTSC_IN_T n) {
  return (sms_ntsc_rgb_t*) ((char*) ntsc->table +
  ((n << 10 & 0x7800) | (n & 0x0780) | (n >> 9 & 0x0078)) *
  (sms_ntsc_entry_size * sizeof (sms_ntsc_rgb_t) / 8));
}

static sms_ntsc_rgb_t* SMS_NTSC_RGB15(sms_ntsc_t const* ntsc, SMS_NTSC_IN_T n) {
  return (sms_ntsc_rgb_t*) ((char*) ntsc->table +
  ((n << 9 & 0x3C00) | (n & 0x03C0) | (n >> 9 & 0x003C)) *
  (sms_ntsc_entry_size * sizeof (sms_ntsc_rgb_t) / 4));
}

/* common ntsc macros */
const int sms_ntsc_rgb_builder    = ((1L << 21) | (1 << 11) | (1 << 1));
const int sms_ntsc_clamp_mask     = (sms_ntsc_rgb_builder * 3 / 2);
const int sms_ntsc_clamp_add      = (sms_ntsc_rgb_builder * 0x101);

static void SMS_NTSC_CLAMP_(sms_ntsc_rgb_t io, s32 shift) {
  sms_ntsc_rgb_t sub = io >> (9-(shift)) & sms_ntsc_clamp_mask;
  sms_ntsc_rgb_t clamp = sms_ntsc_clamp_add - sub;
  io |= clamp;
  clamp -= sub;
  io &= clamp;
}


/* Video format presets */
const sms_ntsc_setup_t[] sms_ntsc_monochrome = { 0,-1, 0, 0,.2,  0, .2,-.2,-.2,-1, 0,  0 }; /* desaturated + artifacts */
const sms_ntsc_setup_t[] sms_ntsc_composite  = { 0, 0, 0, 0, 0,  0,.25,  0,  0, 0, 0,  0 }; /* color bleeding + artifacts */
const sms_ntsc_setup_t[] sms_ntsc_svideo     = { 0, 0, 0, 0, 0,  0,.25, -1, -1, 0, 0,  0 }; /* color bleeding only */
const sms_ntsc_setup_t[] sms_ntsc_rgb        = { 0, 0, 0, 0,.2,  0,.70, -1, -1,-1, 0,  0 }; /* crisp image */

const int alignment_count = 3;
const int burst_count     = 1;
const int rescale_in      = 8;
const int rescale_out     = 7;

const float artifacts_mid  = 0.4f;
const float artifacts_max  = 1.2f;
const float fringing_mid   = 0.8f;
const int std_decoder_hue  = 0;

const int gamma_size      = 16;

/* 3 input pixels -> 8 composite samples */
const pixel_info_t[alignment_count] sms_ntsc_pixels = {
  { PIXEL_OFFSET( -4, -9 ), { 1, 1, .6667f, 0 } },
  { PIXEL_OFFSET( -2, -7 ), {       .3333f, 1, 1, .3333f } },
  { PIXEL_OFFSET(  0, -5 ), {                  0, .6667f, 1, 1 } },
};

static void correct_errors( sms_ntsc_rgb_t color, sms_ntsc_rgb_t* out_var )
{
  u32 i;
  for ( i = 0; i < rgb_kernel_size / 2; i++ )
  {
    sms_ntsc_rgb_t error = color -
        out_var [i    ] - out_var [(i+12)%14+14] - out_var [(i+10)%14+28] -
        out_var [i + 7] - out_var [i + 5    +14] - out_var [i + 3    +28];
    CORRECT_ERROR(out_var, i, i + 3 + 28 );
  }
}

void sms_ntsc_init( sms_ntsc_t* ntsc, sms_ntsc_setup_t const* setup )
{
  int entry;
  init_t impl;
  if ( !setup )
    setup = &sms_ntsc_composite;
  init( &impl, setup );
  
  for ( entry = 0; entry < sms_ntsc_palette_size; entry++ )
  {
    float bb = impl.to_float [entry >> 8 & 0x0F];
    float gg = impl.to_float [entry >> 4 & 0x0F];
    float rr = impl.to_float [entry      & 0x0F];
    
    float y, i, q = RGB_TO_YIQ(rr, gg, bb, &y, &i);
    
    int r, g, b;
    YIQ_TO_RGB(y, i, q, impl.to_rgb, &r, &g, &b);
    sms_ntsc_rgb_t rgb = PACK_RGB( r, g, b );
    
    if ( setup->palette_out )
      RGB_PALETTE_OUT( rgb, &setup->palette_out [entry * 3] );
    
    if ( ntsc )
    {
      gen_kernel( &impl, y, i, q, ntsc->table [entry] );
      correct_errors( rgb, ntsc->table [entry] );
    }
  }
}

/* Filters one row of pixels. Input pixel format is set by SMS_NTSC_IN_FORMAT
and output RGB depth is set by SMS_NTSC_OUT_DEPTH. Both default to 16-bit RGB.
In_row_width is the number of pixels to get to the next input row. */
version(CUSTOM_BLITTER) {
void sms_ntsc_blit( sms_ntsc_t const* ntsc, SMS_NTSC_IN_T const* table, u8* input,
                    int in_width, int vline)
{
  int const chunk_count = in_width / sms_ntsc_in_chunk;

  /* handle extra 0, 1, or 2 pixels by placing them at beginning of row */
  int const in_extra = in_width - chunk_count * sms_ntsc_in_chunk;
  u32 const extra2 = (u32) -(in_extra >> 1 & 1); /* (u32) -1 = ~0 */
  u32 const extra1 = (u32) -(in_extra & 1) | extra2;

  /* use palette entry 0 for unused pixels */
  SMS_NTSC_IN_T border = table[0];

  SMSBlitData blit_data;
  blit_data.sms_ntsc_pixel0_ = border;
  blit_data.kernel0  = SMS_NTSC_IN_FORMAT( ntsc, blit_data.sms_ntsc_pixel0_ );
  blit_data.sms_ntsc_pixel1_ = table[input[0]] & extra2;
  blit_data.kernel1  = SMS_NTSC_IN_FORMAT( ntsc, blit_data.sms_ntsc_pixel1_ );
  blit_data.sms_ntsc_pixel2_ = table[input[extra2 & 1]] & extra1;
  blit_data.kernel2  = SMS_NTSC_IN_FORMAT( ntsc, blit_data.sms_ntsc_pixel2_ );
  blit_data.kernelx1 = blit_data.kernel0;
  blit_data.kernelx2 = blit_data.kernel0;

  blit_data.line_out  = (sms_ntsc_out_t*)(&bitmap.data[(vline * bitmap.pitch)]);

  int n;
  input += in_extra;

  for ( n = chunk_count; n; --n )
  {
    /* order of input and output pixels must not be altered */
    SMS_NTSC_COLOR_IN(&blit_data, 0, ntsc, table[*input++]);
    SMS_NTSC_RGB_OUT(&blit_data, 0);
    SMS_NTSC_RGB_OUT(&blit_data, 1);
    
    SMS_NTSC_COLOR_IN(&blit_data, 1, ntsc, table[*input++]);
    SMS_NTSC_RGB_OUT(&blit_data, 2);
    SMS_NTSC_RGB_OUT(&blit_data, 3);
      
    SMS_NTSC_COLOR_IN(&blit_data, 2, ntsc, table[*input++]);
    SMS_NTSC_RGB_OUT(&blit_data, 4);
    SMS_NTSC_RGB_OUT(&blit_data, 5);
    SMS_NTSC_RGB_OUT(&blit_data, 6);
  }

  /* finish final pixels */
  SMS_NTSC_COLOR_IN(&blit_data, 0, ntsc, border );
  SMS_NTSC_RGB_OUT(&blit_data, 0);
  SMS_NTSC_RGB_OUT(&blit_data, 1);

  SMS_NTSC_COLOR_IN(&blit_data, 1, ntsc, border );
  SMS_NTSC_RGB_OUT(&blit_data, 2);
  SMS_NTSC_RGB_OUT(&blit_data, 3);

  SMS_NTSC_COLOR_IN(&blit_data, 2, ntsc, border );
  SMS_NTSC_RGB_OUT(&blit_data, 4);
  SMS_NTSC_RGB_OUT(&blit_data, 5);
  SMS_NTSC_RGB_OUT(&blit_data, 6);
}
}

