/* sms_ntsc 0.2.3. http://www.slack.net/~ant/ */

/* Common implementation of NTSC filters */

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

import types;
import std.math;
import sms_ntsc;

const int DISABLE_CORRECTION = 0;
const float PI = 3.14159265358979323846f;

const float LUMA_CUTOFF = 0.20f;
const int gamma_size = 1;
const int rgb_bits = 8;
//const float artifacts_max = artifacts_mid * 1.5f;
const float fringing_max = fringing_mid * 2;
int STD_HUE_CONDITION(const sms_ntsc_setup_t* setup) { return 1; }

const int ext_decoder_hue     = std_decoder_hue + 15;
const int rgb_unit            = 1 << rgb_bits;
const float rgb_offset        = rgb_unit * 2 + 0.5f;

const float burst_size  = sms_ntsc_entry_size / burst_count;
const int kernel_half = 16;
const int kernel_size = kernel_half * 2 + 1;

struct init_t
{
  float[burst_count * 6] to_rgb;
  float[gamma_size] to_float;
  float contrast;
  float brightness;
  float artifacts;
  float fringing;
  float[rescale_out * kernel_size * 2] kernel;
}

static void ROTATE_IQ(float* i, float* q, float sin_b, float cos_b) {
  float t;
  t = (*i) * cos_b - (*q) * sin_b;
  (*q) = (*i) * sin_b + (*q) * cos_b;
  (*i) = t;
}

static void init_filters( init_t* impl, const sms_ntsc_setup_t* setup )
{
static if(rescale_out > 1) {
  float[kernel_size * 2] kernels;
} else {
  const float* kernels = impl.kernel;
}

  /* generate luma (y) filter using sinc kernel */
  {
    /* sinc with rolloff (dsf) */
    const float rolloff = 1 + cast(float) setup.sharpness * cast(float) 0.032;
    const float maxh = 32;
    const float pow_a_n = cast(float) pow( rolloff, maxh );
    float sum;
    int i;
    /* quadratic mapping to reduce negative (blurring) range */
    float to_angle = cast(float) setup.resolution + 1;
    to_angle = PI / maxh * cast(float) LUMA_CUTOFF * (to_angle * to_angle + 1);
    
    kernels [kernel_size * 3 / 2] = maxh; /* default center value */
    for ( i = 0; i < kernel_half * 2 + 1; i++ )
    {
      int x = i - kernel_half;
      float angle = x * to_angle;
      /* instability occurs at center point with rolloff very close to 1.0 */
      if ( x || pow_a_n > cast(float) 1.056 || pow_a_n < cast(float) 0.981 )
      {
        float rolloff_cos_a = rolloff * cast(float) cos( angle );
        float num = 1 - rolloff_cos_a -
            pow_a_n * cast(float) cos( maxh * angle ) +
            pow_a_n * rolloff * cast(float) cos( (maxh - 1) * angle );
        float den = 1 - rolloff_cos_a - rolloff_cos_a + rolloff * rolloff;
        float dsf = num / den;
        kernels [kernel_size * 3 / 2 - kernel_half + i] = dsf - cast(float) 0.5;
      }
    }
    
    /* apply blackman window and find sum */
    sum = 0;
    for ( i = 0; i < kernel_half * 2 + 1; i++ )
    {
      float x = PI * 2 / (kernel_half * 2) * i;
      float blackman = 0.42f - 0.5f * cast(float) cos( x ) + 0.08f * cast(float) cos( x * 2 );
      sum += (kernels [kernel_size * 3 / 2 - kernel_half + i] *= blackman);
    }
    
    /* normalize kernel */
    sum = 1.0f / sum;
    for ( i = 0; i < kernel_half * 2 + 1; i++ )
    {
      int x = kernel_size * 3 / 2 - kernel_half + i;
      kernels [x] *= sum;
      assert( kernels [x] == kernels [x] ); /* catch numerical instability */
    }
  }

  /* generate chroma (iq) filter using gaussian kernel */
  {
    const float cutoff_factor = -0.03125f;
    float cutoff = cast(float) setup.bleed;
    int i;
    
    if ( cutoff < 0 )
    {
      /* keep extreme value accessible only near upper end of scale (1.0) */
      cutoff *= cutoff;
      cutoff *= cutoff;
      cutoff *= cutoff;
      cutoff *= -30.0f / 0.65f;
    }
    cutoff = cutoff_factor - 0.65f * cutoff_factor * cutoff;
    
    for ( i = -kernel_half; i <= kernel_half; i++ )
      kernels [kernel_size / 2 + i] = cast(float) exp( i * i * cutoff );
    
    /* normalize even and odd phases separately */
    for ( i = 0; i < 2; i++ )
    {
      float sum = 0;
      int x;
      for ( x = i; x < kernel_size; x += 2 )
        sum += kernels [x];
      
      sum = 1.0f / sum;
      for ( x = i; x < kernel_size; x += 2 )
      {
        kernels [x] *= sum;
        assert( kernels [x] == kernels [x] ); /* catch numerical instability */
      }
    }
  }
  
  /*
  printf( "luma:\n" );
  for ( i = kernel_size; i < kernel_size * 2; i++ )
    printf( "%f\n", kernels [i] );
  printf( "chroma:\n" );
  for ( i = 0; i < kernel_size; i++ )
    printf( "%f\n", kernels [i] );
  */
  
  /* generate linear rescale kernels */
  static if(rescale_out > 1) {
  {
    float weight = 1.0f;
    float* out_var = impl.kernel;
    int n = rescale_out;
    do
    {
      float remain = 0;
      int i;
      weight -= 1.0f / rescale_in;
      for ( i = 0; i < kernel_size * 2; i++ )
      {
        float cur = kernels [i];
        float m = cur * weight;
        *out_var++ = m + remain;
        remain = cur - m;
      }
    }
    while ( --n );
  }
  }
}

static const float[6] default_decoder =
  { 0.956f, 0.621f, -0.272f, -0.647f, -1.105f, 1.702f };

static void init( init_t* impl, const sms_ntsc_setup_t* setup )
{
  impl.brightness = cast(float) setup.brightness * (0.5f * rgb_unit) + rgb_offset;
  impl.contrast   = cast(float) setup.contrast   * (0.5f * rgb_unit) + rgb_unit;
  version(default_palette_contrast) {
    if ( !setup.palette )
      impl.contrast *= default_palette_contrast;
  }
  
  impl.artifacts = cast(float) setup.artifacts;
  if ( impl.artifacts > 0 )
    impl.artifacts *= artifacts_max - artifacts_mid;
  impl.artifacts = impl.artifacts * artifacts_mid + artifacts_mid;

  impl.fringing = cast(float) setup.fringing;
  if ( impl.fringing > 0 )
    impl.fringing *= fringing_max - fringing_mid;
  impl.fringing = impl.fringing * fringing_mid + fringing_mid;
  
  init_filters( impl, setup );
  
  /* generate gamma table */
  if ( gamma_size > 1 )
  {
    const float to_float = 1.0f / (gamma_size - (gamma_size > 1));
    const float gamma = 1.1333f - cast(float) setup.gamma * 0.5f;
    /* match common PC's 2.2 gamma to TV's 2.65 gamma */
    int i;
    for ( i = 0; i < gamma_size; i++ )
      impl.to_float [i] =
          cast(float) pow( i * to_float, gamma ) * impl.contrast + impl.brightness;
  }
  
  /* setup decoder matricies */
  {
    float hue = cast(float) setup.hue * PI + PI / 180 * ext_decoder_hue;
    float sat = cast(float) setup.saturation + 1;
    const float* decoder = setup.decoder_matrix;
    if ( !decoder )
    {
      decoder = default_decoder;
      if ( STD_HUE_CONDITION( setup ) )
        hue += PI / 180 * (std_decoder_hue - ext_decoder_hue);
    }
    
    {
      float s = cast(float) sin( hue ) * sat;
      float c = cast(float) cos( hue ) * sat;
      float* out_var = impl.to_rgb;
      int n;
      
      n = burst_count;
      do
      {
        const float* in_var = decoder;
        int n = 3;
        do
        {
          float i = *in_var++;
          float q = *in_var++;
          *out_var++ = i * c - q * s;
          *out_var++ = i * s + q * c;
        }
        while ( --n );
        if ( burst_count <= 1 )
          break;
        ROTATE_IQ( &s, &c, 0.866025f, -0.5f ); /* +120 degrees */
      }
      while ( --n );
    }
  }
}

/* kernel generation */

static float RGB_TO_YIQ(float r, float g, float b, float* y, float* i) {
  (*y) = r * 0.299f + g * 0.587f + b * 0.114f;
  (*i) = r * 0.596f - g * 0.275f - b * 0.321f;
  return r * 0.212f - g * 0.523f + b * 0.311f;
}

static void YIQ_TO_RGB(float y, float i, float q, float* to_rgb, int* r, int* g, int* b) {
  (*r) = cast(int) (y + to_rgb[0] * i + to_rgb[1] * q);
  (*g) = cast(int) (y + to_rgb[2] * i + to_rgb[3] * q);
  (*b) = cast(int) (y + to_rgb[4] * i + to_rgb[5] * q);
}

static sms_ntsc_rgb_t PACK_RGB(int r, int g, int b) {
    return r << 21 | g << 11 | b << 1;
}

const float rgb_kernel_size = burst_size / alignment_count;
const int rgb_bias = rgb_unit * 2 * sms_ntsc_rgb_builder;

struct pixel_info_t
{
  int offset;
  float negate;
  float[4] kernel;
}

static if(rescale_in > 1) {
  int PIXEL_OFFSET_(int ntsc, int scaled) {
    return (kernel_size / 2 + ntsc + (scaled != 0) + (rescale_out - scaled) % rescale_out + 
        (kernel_size * 2 * scaled));
  }

  int PIXEL_OFFSET(int ntsc, int scaled) {
    return PIXEL_OFFSET_(
        (ntsc - scaled / rescale_out * rescale_in),
        ((scaled + rescale_out * 10) % rescale_out) ),
        (1.0f - ((ntsc + 100) & 2));
  }
} else {
  int PIXEL_OFFSET(int ntsc, int scaled ) {
    return (kernel_size / 2 + ntsc - scaled),
    (1.0f - ((ntsc + 100) & 2));
  }
}

/* Generate pixel at all burst phases and column alignments */
static void gen_kernel( init_t* impl, float y, float i, float q, sms_ntsc_rgb_t* out_var )
{
  /* generate for each scanline burst phase */
  float* to_rgb = impl.to_rgb;
  int burst_remain = burst_count;
  y -= rgb_offset;
  do
  {
    /* Encode yiq into *two* composite signals (to allow control over artifacting).
    Convolve these with kernels which: filter respective components, apply
    sharpening, and rescale horizontally. Convert resulting yiq to rgb and pack
    into integer. Based on algorithm by NewRisingSun. */
    const pixel_info_t* pixel = sms_ntsc_pixels;
    int alignment_remain = alignment_count;
    do
    {
      /* negate is -1 when composite starts at odd multiple of 2 */
      const float yy = y * impl.fringing * pixel.negate;
      const float ic0 = (i + yy) * pixel.kernel [0];
      const float qc1 = (q + yy) * pixel.kernel [1];
      const float ic2 = (i - yy) * pixel.kernel [2];
      const float qc3 = (q - yy) * pixel.kernel [3];
      
      const float factor = impl.artifacts * pixel.negate;
      const float ii = i * factor;
      const float yc0 = (y + ii) * pixel.kernel [0];
      const float yc2 = (y - ii) * pixel.kernel [2];
      
      const float qq = q * factor;
      const float yc1 = (y + qq) * pixel.kernel [1];
      const float yc3 = (y - qq) * pixel.kernel [3];
      
      const float* k = &impl.kernel [pixel.offset];
      int n;
      ++pixel;
      for ( n = rgb_kernel_size; n; --n )
      {
        float i = k[0]*ic0 + k[2]*ic2;
        float q = k[1]*qc1 + k[3]*qc3;
        float y = k[kernel_size+0]*yc0 + k[kernel_size+1]*yc1 +
                  k[kernel_size+2]*yc2 + k[kernel_size+3]*yc3 + rgb_offset;
        if ( rescale_out <= 1 )
          k--;
        else if ( k < &impl.kernel [kernel_size * 2 * (rescale_out - 1)] )
          k += kernel_size * 2 - 1;
        else
          k -= kernel_size * 2 * (rescale_out - 1) + 2;
        {
          int r, g, b;
          YIQ_TO_RGB(y, i, q, to_rgb, &r, &g, &b);
          *out_var++ = PACK_RGB( r, g, b ) - rgb_bias;
        }
      }
    }
    while ( alignment_count > 1 && --alignment_remain );
    
    if ( burst_count <= 1 )
      break;
    
    to_rgb += 6;
    
    ROTATE_IQ( &i, &q, -0.866025f, -0.5f ); /* -120 degrees */
  }
  while ( --burst_remain );
}

static if(DISABLE_CORRECTION) {
  static void CORRECT_ERROR(sms_ntsc_rgb_t* out_var, u32 i, u32 a) { out_var[i] += rgb_bias; }
} else {
  static void CORRECT_ERROR(sms_ntsc_rgb_t* out_var, u32 i, u32 a) { out_var[a] += error; }
}

static void RGB_PALETTE_OUT(sms_ntsc_rgb_t rgb, u8* out_) {
  u8* out_var = out_;
  sms_ntsc_rgb_t clamped = rgb;
  SMS_NTSC_CLAMP_( clamped, (8 - rgb_bits) );
  out_var[0] = cast(u8) (clamped >> 21);
  out_var[1] = cast(u8) (clamped >> 11);
  out_var[2] = cast(u8) (clamped >>  1);
}

