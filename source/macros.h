#ifndef _MACROS_H_
#define _MACROS_H_

#include <types.h>

#ifdef LSB_FIRST

u8 READ_BYTE(u8* BASE, u32 ADDR);
u16 READ_WORD(u8* BASE, u32 ADDR);
u32 READ_WORD_LONG(u8* BASE, u32 ADDR);
void WRITE_BYTE(u8* BASE, u32 ADDR, u8 VAL);
void WRITE_WORD(u8* BASE, u32 ADDR, u16 VAL);
void WRITE_WORD_LONG(u8* BASE, u32 ADDR, u32 VAL);

#else

u8 READ_BYTE(u8* BASE, u32 ADDR);
u16 READ_WORD(u8* BASE, u32 ADDR);
u32 READ_WORD_LONG(u8* BASE, u32 ADDR);
void WRITE_BYTE(u8* BASE, u32 ADDR, u8 VAL);
void WRITE_WORD(u8* BASE, u32 ADDR, u16 VAL);
void WRITE_WORD_LONG(u8* BASE, u32 ADDR, u32 VAL);

#endif

/* C89 compatibility */
#ifndef M_PI
#define M_PI 3.14159265358979323846264338327f
#endif /* M_PI */


#endif /* _MACROS_H_ */
