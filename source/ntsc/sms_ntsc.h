/* Sega Master System/Game Gear/TI 99/4A NTSC video filter */

/* sms_ntsc 0.2.3 */
#ifndef SMS_NTSC_H
#define SMS_NTSC_H

#include "sms_ntsc_config.h"

#ifdef __cplusplus
  extern "C" {
#endif

typedef unsigned long sms_ntsc_rgb_t;

/* Image parameters, ranging from -1.0 to 1.0. Actual internal values shown
in parenthesis and should remain fairly stable in future versions. */
typedef struct sms_ntsc_setup_t
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
} sms_ntsc_setup_t;

/* Video format presets */
extern sms_ntsc_setup_t const sms_ntsc_composite; /* color bleeding + artifacts */
extern sms_ntsc_setup_t const sms_ntsc_svideo;    /* color bleeding only */
extern sms_ntsc_setup_t const sms_ntsc_rgb;       /* crisp image */
extern sms_ntsc_setup_t const sms_ntsc_monochrome;/* desaturated + artifacts */

enum { sms_ntsc_palette_size = 4096 };

/* Initializes and adjusts parameters. Can be called multiple times on the same
sms_ntsc_t object. Can pass NULL for either parameter. */
typedef struct sms_ntsc_t sms_ntsc_t;
void sms_ntsc_init( sms_ntsc_t* ntsc, sms_ntsc_setup_t const* setup );

/* Filters one row of pixels. Input pixel format is set by SMS_NTSC_IN_FORMAT
and output RGB depth is set by SMS_NTSC_OUT_DEPTH. Both default to 16-bit RGB.
In_row_width is the number of pixels to get to the next input row. */
void sms_ntsc_blit( sms_ntsc_t const* ntsc, SMS_NTSC_IN_T const* table, u8* input,
    int in_width, int vline);

static sms_ntsc_rgb_t* SMS_NTSC_RGB16(sms_ntsc_t const* ntsc, SMS_NTSC_IN_T n);
static void SMS_NTSC_CLAMP_(sms_ntsc_rgb_t io, s32 shift);

/* Interface for user-defined custom blitters */

enum { sms_ntsc_in_chunk    = 3 }; /* number of input pixels read per chunk */
enum { sms_ntsc_out_chunk   = 7 }; /* number of output pixels generated per chunk */

typedef struct {
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
} SMSBlitData;

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
enum { sms_ntsc_entry_size = 3 * 14 };
struct sms_ntsc_t {
  sms_ntsc_rgb_t table [sms_ntsc_palette_size] [sms_ntsc_entry_size];
};

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
#define sms_ntsc_rgb_builder    ((1L << 21) | (1 << 11) | (1 << 1))
#define sms_ntsc_clamp_mask     (sms_ntsc_rgb_builder * 3 / 2)
#define sms_ntsc_clamp_add      (sms_ntsc_rgb_builder * 0x101)
static void SMS_NTSC_CLAMP_(sms_ntsc_rgb_t io, s32 shift) {
  sms_ntsc_rgb_t sub = io >> (9-(shift)) & sms_ntsc_clamp_mask;
  sms_ntsc_rgb_t clamp = sms_ntsc_clamp_add - sub;
  io |= clamp;
  clamp -= sub;
  io &= clamp;
}

#ifdef __cplusplus
  }
#endif

#endif
