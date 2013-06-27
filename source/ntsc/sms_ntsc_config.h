/* Configure library by modifying this file */

#ifndef SMS_NTSC_CONFIG_H
#define SMS_NTSC_CONFIG_H

/* Format of source & output pixels (RGB565 only) */
#define SMS_NTSC_IN_FORMAT SMS_NTSC_RGB16
#define SMS_NTSC_OUT_DEPTH 16

/* Original CRAM format (not used) */
/* #define SMS_NTSC_IN_FORMAT SMS_NTSC_BGR12 */

/* The following affect the built-in blitter only; a custom blitter can
handle things however it wants. */

/* Type of input pixel values (fixed to 16-bit)*/
#define SMS_NTSC_IN_T u16

/* Each raw pixel input value is passed through this. You might want to mask
the pixel index if you use the high bits as flags, etc. */
#define SMS_NTSC_ADJ_IN( in ) in

/* For each pixel, this is the basic operation:
output_color = SMS_NTSC_ADJ_IN( SMS_NTSC_IN_T ) */

#endif
