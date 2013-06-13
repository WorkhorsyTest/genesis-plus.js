
#ifndef TYPES_H
#define TYPES_H

#include <assert.h>

#define TRUE  1
#define FALSE 0

typedef unsigned char         u8;
typedef unsigned short        u16;
typedef unsigned int          u32;
typedef unsigned long long    u64;
typedef signed char           s8;
typedef signed short          s16;
typedef signed int            s32;
typedef signed long long      s64;


typedef union {
    u16 w;
    struct {
#ifdef LSB_FIRST
        u8 l;
        u8 h;
#else
        u8 h;
        u8 l;
#endif
    } byte;

} reg16_t;



typedef union {
#ifdef LSB_FIRST
  struct { u8 l,h,h2,h3; } b;
  struct { u16 l,h; } w;
#else
  struct { u8 h3,h2,h,l; } b;
  struct { u16 h,l; } w;
#endif
  u32 d;
}  PAIR;


#endif  /* TYPES_H */
