/* Configure library by modifying this file */

import types;
import sms_ntsc;

/* Format of source & output pixels (RGB565 only) */
alias SMS_NTSC_RGB16 SMS_NTSC_IN_FORMAT;
const int SMS_NTSC_OUT_DEPTH = 16;

/* The following affect the built-in blitter only; a custom blitter can
handle things however it wants. */

/* Type of input pixel values (fixed to 16-bit)*/
alias u16 SMS_NTSC_IN_T;

/* For each pixel, this is the basic operation:
output_color = SMS_NTSC_ADJ_IN( SMS_NTSC_IN_T ) */

alias u16 sms_ntsc_out_t;

