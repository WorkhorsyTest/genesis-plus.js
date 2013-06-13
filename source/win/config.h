
#ifndef _CONFIG_H_
#define _CONFIG_H_

/****************************************************************************
 * Config Option 
 *
 ****************************************************************************/
typedef struct 
{
  u8 padtype;
} t_input_config;

typedef struct 
{
  u8 hq_fm;
  u8 filter;
  u8 psgBoostNoise;
  u8 dac_bits;
  u8 ym2413;
  s16 psg_preamp;
  s16 fm_preamp;
  s16 lp_range;
  s16 low_freq;
  s16 high_freq;
  s16 lg;
  s16 mg;
  s16 hg;
  u8 system;
  u8 region_detect;
  u8 vdp_mode;
  u8 master_clock;
  u8 force_dtack;
  u8 addr_error;
  u8 tmss;
  u8 bios;
  u8 lock_on;
  u8 hot_swap;
  u8 invert_mouse;
  u8 gun_cursor[2];
  u8 overscan;
  u8 gg_extra;
  u8 ntsc;
  u8 render;
  t_input_config input[MAX_INPUTS];
} t_config;

/* Global variables */
extern t_config config;
extern void set_config_defaults(void);

#endif /* _CONFIG_H_ */

