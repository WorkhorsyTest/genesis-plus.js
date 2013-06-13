/*
**
** File: ym2413.c - software implementation of YM2413
**                  FM sound generator type OPLL
**
** Copyright (C) 2002 Jarek Burczynski
**
** Version 1.0
**
*/

#ifndef _H_YM2413_
#define _H_YM2413_

extern void YM2413Init(void);
extern void YM2413ResetChip(void);
extern void YM2413Update(int *buffer, int length);
extern void YM2413Write(u32 a, u32 v);
extern u32 YM2413Read(u32 a);
extern u8 *YM2413GetContextPtr(void);
extern u32 YM2413GetContextSize(void);

#endif /*_H_YM2413_*/
