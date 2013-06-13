
#ifndef TYPES_H
#define TYPES_H

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

typedef unsigned char         uint8;
typedef unsigned short        uint16;
typedef unsigned int          uint32;
typedef unsigned long long    uint64;
typedef signed char           int8;
typedef signed short          int16;
typedef signed int            int32;
typedef signed long long      int64;

// Alternative S for signed
typedef int8                  sint8;
typedef int16                 sint16;
typedef int32                 sint32;
typedef int64                 sint64;

// Alternative upper case
/*
typedef uint8                 UINT8;
typedef uint16                UINT16;
typedef uint32                UINT32;
typedef uint64                UINT64;
typedef int8                  INT8;
typedef int16                 INT16;
typedef int32                 INT32;
typedef int64                 INT64;
*/
// Alternative S for signed and upper case
typedef int8                  SINT8;
typedef int16                 SINT16;
typedef int32                 SINT32;
typedef int64                 SINT64;

// Alternative
typedef int32                 sint;
typedef uint32                uint;

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
