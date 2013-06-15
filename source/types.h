
#ifndef TYPES_H
#define TYPES_H

#include <assert.h>
#include <stdint.h>

#define TRUE  1
#define FALSE 0

typedef uint8_t     u8;
typedef uint16_t    u16;
typedef uint32_t    u32;
typedef uint64_t    u64;
typedef int8_t      s8;
typedef int16_t     s16;
typedef int32_t     s32;
typedef int64_t     s64;


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
