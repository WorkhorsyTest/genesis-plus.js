/*
**
** software implementation of Yamaha FM sound generator (YM2612/YM3438)
**
** Original code (MAME fm.c)
**
** Copyright (C) 2001, 2002, 2003 Jarek Burczynski (bujar at mame dot net)
** Copyright (C) 1998 Tatsuyuki Satoh , MultiArcadeMachineEmulator development
**
** Version 1.4 (final beta) 
**
** Additional code & fixes by Eke-Eke for Genesis Plus GX
**
*/

#ifndef _H_YM2612_
#define _H_YM2612_

extern void YM2612Init();
extern void YM2612Config(u8 dac_bits);
extern void YM2612ResetChip();
extern void YM2612Update(int *buffer, int length);
extern void YM2612Write(u32 a, u32 v);
extern u32 YM2612Read();
extern int YM2612LoadContext(u8 *state);
extern int YM2612SaveContext(u8 *state);

#endif /* _YM2612_ */
