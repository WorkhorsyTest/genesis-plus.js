/* 
    SN76489 emulation
    by Maxim in 2001 and 2002
*/

#ifndef _SN76489_H_
#define _SN76489_H_

#include "blip_buf.h"

#define SN_DISCRETE    0
#define SN_INTEGRATED  1

/* Function prototypes */
extern void SN76489_Init(blip_t* left, blip_t* right, int type);
extern void SN76489_Reset();
extern void SN76489_Config(u32 clocks, int preAmp, int boostNoise, int stereo);
extern void SN76489_Write(u32 clocks, u32 data);
extern void SN76489_Update(u32 cycles);
extern void *SN76489_GetContextPtr();
extern int SN76489_GetContextSize();

#endif /* _SN76489_H_ */
