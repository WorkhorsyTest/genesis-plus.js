/***************************************************************************************
 *  Genesis Plus
 *  Main 68k bus handlers
 *
 *  Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003  Charles Mac Donald (original code)
 *  Copyright (C) 2007-2013  Eke-Eke (Genesis Plus GX)
 *
 *  Redistribution and use of this code or any derivative works are permitted
 *  provided that the following conditions are met:
 *
 *   - Redistributions may not be sold, nor may they be used in a commercial
 *     product or activity.
 *
 *   - Redistributions that are modified from the original source must include the
 *     complete source code, including the source code for all components used by a
 *     binary built from the modified sources. However, as a special exception, the
 *     source code distributed need not include anything that is normally distributed
 *     (in either source or binary form) with the major components (compiler, kernel,
 *     and so on) of the operating system on which the executable runs, unless that
 *     component itself accompanies the executable.
 *
 *   - Redistributions must reproduce the above copyright notice, this list of
 *     conditions and the following disclaimer in the documentation and/or other
 *     materials provided with the distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************************/

#ifndef _MEM68K_H_
#define _MEM68K_H_

/* unused areas */
extern u32 m68k_read_bus_8(u32 address);
extern u32 m68k_read_bus_16(u32 address);
extern void m68k_unused_8_w(u32 address, u32 data);
extern void m68k_unused_16_w(u32 address, u32 data);

/* illegal areas */
extern u32 m68k_lockup_r_8(u32 address);
extern u32 m68k_lockup_r_16(u32 address);
extern void m68k_lockup_w_8(u32 address, u32 data);
extern void m68k_lockup_w_16(u32 address, u32 data);

/* Z80 bus */
extern u32 z80_read_byte(u32 address);
extern u32 z80_read_word(u32 address);
extern void z80_write_byte(u32 address, u32 data);
extern void z80_write_word(u32 address, u32 data);

/* I/O & Control registers */
extern u32 ctrl_io_read_byte(u32 address);
extern u32 ctrl_io_read_word(u32 address);
extern void ctrl_io_write_byte(u32 address, u32 data);
extern void ctrl_io_write_word(u32 address, u32 data);

/* VDP */
extern u32 vdp_read_byte(u32 address);
extern u32 vdp_read_word(u32 address);
extern void vdp_write_byte(u32 address, u32 data);
extern void vdp_write_word(u32 address, u32 data);

/* PICO */
extern u32 pico_read_byte(u32 address);
extern u32 pico_read_word(u32 address);

#endif /* _MEM68K_H_ */
