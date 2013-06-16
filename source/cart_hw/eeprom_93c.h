/****************************************************************************
 *  Genesis Plus
 *  Microwire Serial EEPROM (93C46 only) support
 *
 *  Copyright (C) 2011  Eke-Eke (Genesis Plus GX)
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

#ifndef _EEPROM_93C_H_
#define _EEPROM_93C_H_

typedef enum
{
  STATE_WAIT_STANDBY,
  STATE_WAIT_START,
  STATE_GET_OPCODE,
  STATE_WRITE_WORD,
  STATE_READ_WORD
} T_STATE_93C;

typedef struct
{
  u8 enabled;  /* 1: chip enabled */
  u8 cs;       /* CHIP SELECT line state */
  u8 clk;      /* CLK line state */
  u8 data;     /* DATA OUT line state */
  u8 cycles;   /* current operation cycle */
  u8 we;       /* 1: write enabled */
  u8 opcode;   /* 8-bit opcode + address */
  u16 buffer;  /* 16-bit data buffer */
  T_STATE_93C state; /* current operation state */
} T_EEPROM_93C;

/* global variables */
extern T_EEPROM_93C eeprom_93c;

/* Function prototypes */
extern void eeprom_93c_init();
extern void eeprom_93c_write(u8 data);
extern u8 eeprom_93c_read();

#endif
