/* Configure library by modifying this file */

/* Format of source & output pixels (RGB565 only)*/
alias MD_NTSC_RGB16 MD_NTSC_IN_FORMAT;
const int MD_NTSC_OUT_DEPTH = 16;

/* The following affect the built-in blitter only; a custom blitter can
handle things however it wants. */

/* Type of input pixel values (fixed to 16-bit) */
alias u16 MD_NTSC_IN_T;

/* For each pixel, this is the basic operation:
output_color = MD_NTSC_ADJ_IN( MD_NTSC_IN_T ) */

alias u16 md_ntsc_out_t;


