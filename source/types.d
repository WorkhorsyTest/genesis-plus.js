

import std.stdint;

alias int8_t    s8;
alias int16_t   s16;
alias int32_t   s32;
alias int64_t   s64;

alias uint8_t   u8;
alias uint16_t  u16;
alias uint32_t  u32;
alias uint64_t  u64;


union reg16_t {
	u16 w;
	struct reg_16_byte_t {
version(LSB_FIRST) {
		u8 l;
		u8 h;
} else {
		u8 h;
		u8 l;
}
	}
	reg_16_byte_t b;
}

version(LSB_FIRST) {
	struct PAIR_BYTE { u8 l, h, h2, h3; };
	struct PAIR_WORD { u16 l, h; };
} else {
	struct PAIR_BYTE { u8 h3, h2, h, l; };
	struct PAIR_WORD { u16 h, l; };
}


union PAIR {
	PAIR_BYTE b;
	PAIR_WORD w;
	u32 d;
}


