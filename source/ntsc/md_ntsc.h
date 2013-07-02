/* Sega Genesis/Mega Drive NTSC video filter */

/* md_ntsc 0.1.2 */
#ifndef MD_NTSC_H
#define MD_NTSC_H

#include "md_ntsc_config.h"

#ifdef __cplusplus
  extern "C" {
#endif

typedef u32 md_ntsc_rgb_t;

/* Image parameters, ranging from -1.0 to 1.0. Actual internal values shown
in parenthesis and should remain fairly stable in future versions. */
typedef struct md_ntsc_setup_t
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
} md_ntsc_setup_t;

/* Video format presets */
extern md_ntsc_setup_t const md_ntsc_composite; /* color bleeding + artifacts */
extern md_ntsc_setup_t const md_ntsc_svideo;    /* color bleeding only */
extern md_ntsc_setup_t const md_ntsc_rgb;       /* crisp image */
extern md_ntsc_setup_t const md_ntsc_monochrome;/* desaturated + artifacts */

enum { md_ntsc_palette_size = 512 };

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

enum { md_ntsc_in_chunk  = 4 }; /* number of input pixels read per chunk */
enum { md_ntsc_out_chunk = 8 }; /* number of output pixels generated per chunk */
enum { md_ntsc_black     = 0 }; /* palette index for black */

typedef struct {
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
} BlitData;

/* Begin input pixel */
static void MD_NTSC_COLOR_IN_(BlitData* data, s32 index, MD_NTSC_IN_T color, md_ntsc_t const* table) {
  u32 color_;
  switch(index) {
    case 0:
      data->kernelx0 = data->kernel0;
      data->kernel0 = (color_ = color, MD_NTSC_IN_FORMAT( table, color_ ));
      break;
    case 1:
      data->kernelx1 = data->kernel1;
      data->kernel1 = (color_ = color, MD_NTSC_IN_FORMAT( table, color_ ));
      break;
    case 2:
      data->kernelx2 = data->kernel2;
      data->kernel2 = (color_ = color, MD_NTSC_IN_FORMAT( table, color_ ));
      break;
    case 3:
      data->kernelx3 = data->kernel3;
      data->kernel3 = (color_ = color, MD_NTSC_IN_FORMAT( table, color_ ));
      break;
  }
}

static void MD_NTSC_COLOR_IN(BlitData* data, s32 index, md_ntsc_t const* ntsc, MD_NTSC_IN_T color) {
  MD_NTSC_COLOR_IN_(data, index, color, ntsc);
}

/* x is always zero except in snes_ntsc library */
static md_ntsc_out_t MD_NTSC_RGB_OUT_(BlitData* data, s32 x) {
    return (data->raw_>>(13-x)& 0xF800)|(data->raw_>>(8-x)&0x07E0)|(data->raw_>>(4-x)&0x001F);
}

/* Generate output pixel */
static void MD_NTSC_RGB_OUT(BlitData* data, s32 x) {
  data->raw_ =
    data->kernel0  [x+ 0] + data->kernel1  [(x+6)%8+16] + data->kernel2  [(x+4)%8  ] + data->kernel3  [(x+2)%8+16] +
    data->kernelx0 [x+ 8] + data->kernelx1 [(x+6)%8+24] + data->kernelx2 [(x+4)%8+8] + data->kernelx3 [(x+2)%8+24];
  MD_NTSC_CLAMP_(data->raw_, 0);
  *data->line_out = MD_NTSC_RGB_OUT_(data, 0);
  data->line_out++;
}


/* private */
enum { md_ntsc_entry_size = 2 * 16 };
struct md_ntsc_t {
  md_ntsc_rgb_t table [md_ntsc_palette_size] [md_ntsc_entry_size];
};

static md_ntsc_rgb_t* MD_NTSC_BGR9(md_ntsc_t const* ntsc, MD_NTSC_IN_T n) {
  return ntsc->table [n & 0x1FF];
}

static md_ntsc_rgb_t* MD_NTSC_RGB16(md_ntsc_t const* ntsc, MD_NTSC_IN_T n) {
  return (md_ntsc_rgb_t*) ((char*) (ntsc)->table +
  ((n << 9 & 0x3800) | (n & 0x0700) | (n >> 8 & 0x00E0)) *
  (md_ntsc_entry_size * sizeof (md_ntsc_rgb_t) / 32));
}

static md_ntsc_rgb_t* MD_NTSC_RGB15(md_ntsc_t const* ntsc, MD_NTSC_IN_T n) {
  return (md_ntsc_rgb_t*) ((char*) (ntsc)->table +
  ((n << 8 & 0x1C00) | (n & 0x0380) | (n >> 8 & 0x0070)) *
  (md_ntsc_entry_size * sizeof (md_ntsc_rgb_t) / 16));
}

/* common ntsc macros */
#define md_ntsc_rgb_builder    ((1L << 21) | (1 << 11) | (1 << 1))
#define md_ntsc_clamp_mask     (md_ntsc_rgb_builder * 3 / 2)
#define md_ntsc_clamp_add      (md_ntsc_rgb_builder * 0x101)
static void MD_NTSC_CLAMP_(md_ntsc_rgb_t io, s32 shift) {
  md_ntsc_rgb_t sub = (io) >> (9-(shift)) & md_ntsc_clamp_mask;
  md_ntsc_rgb_t clamp = md_ntsc_clamp_add - sub;
  io |= clamp;
  clamp -= sub;
  io &= clamp;
}

#ifdef __cplusplus
}
#endif

#endif
