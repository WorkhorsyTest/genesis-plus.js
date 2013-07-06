/***************************************************************************************
 *  Genesis Plus
 *  Input peripherals support
 *
 *  Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003  Charles Mac Donald (original code)
 *  Copyright (C) 2007-2012  Eke-Eke (Genesis Plus GX)
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

import shared;
import gamepad;
import lightgun;
import mouse;
import activator;
import xe_a1p;
import teamplayer;
import paddle;
import sportspad;
import terebi_oekaki;

/* Max. number of devices */
const int MAX_DEVICES = 8;

/* Ports configuration */
const int NO_SYSTEM           = 0; /* unconnected port*/
const int SYSTEM_MD_GAMEPAD   = 1; /* single 3-buttons or 6-buttons Control Pad */
const int SYSTEM_MOUSE        = 2; /* Sega Mouse */
const int SYSTEM_MENACER      = 3; /* Sega Menacer (port B only) */
const int SYSTEM_JUSTIFIER    = 4; /* Konami Justifiers (port B only) */
const int SYSTEM_XE_A1P       = 5; /* XE-A1P analog controller (port A only) */
const int SYSTEM_ACTIVATOR    = 6; /* Sega Activator */
const int SYSTEM_MS_GAMEPAD   = 7; /* single 2-buttons Control Pad (Master System) */
const int SYSTEM_LIGHTPHASER  = 8; /* Sega Light Phaser (Master System) */
const int SYSTEM_PADDLE       = 9; /* Sega Paddle Control (Master System) */
const int SYSTEM_SPORTSPAD   = 10; /* Sega Sports Pad (Master System) */
const int SYSTEM_TEAMPLAYER  = 11; /* Multi Tap -- Sega TeamPlayer */
const int SYSTEM_WAYPLAY     = 12; /* Multi Tap -- EA 4-Way Play (use both ports) */

/* Device type */
const int NO_DEVICE         = 0xff; /* unconnected device (fixed ID for Team Player) */
const int DEVICE_PAD3B      = 0x00; /* 3-buttons Control Pad (fixed ID for Team Player)*/
const int DEVICE_PAD6B      = 0x01; /* 6-buttons Control Pad (fixed ID for Team Player) */
const int DEVICE_PAD2B      = 0x02; /* 2-buttons Control Pad */
const int DEVICE_MOUSE      = 0x03; /* Sega Mouse */
const int DEVICE_LIGHTGUN   = 0x04; /* Sega Light Phaser, Menacer or Konami Justifiers */
const int DEVICE_PADDLE     = 0x05; /* Sega Paddle Control */
const int DEVICE_SPORTSPAD  = 0x06; /* Sega Sports Pad */
const int DEVICE_PICO       = 0x07; /* PICO tablet */
const int DEVICE_TEREBI     = 0x08; /* Terebi Oekaki tablet */
const int DEVICE_XE_A1P     = 0x09; /* XE-A1P analog controller */
const int DEVICE_ACTIVATOR  = 0x0a; /* Activator */

/* Default Input bitmasks */
const int INPUT_MODE         = 0x0800;
const int INPUT_X            = 0x0400;
const int INPUT_Y            = 0x0200;
const int INPUT_Z            = 0x0100;
const int INPUT_START        = 0x0080;
const int INPUT_A            = 0x0040;
const int INPUT_C            = 0x0020;
const int INPUT_B            = 0x0010;
const int INPUT_RIGHT        = 0x0008;
const int INPUT_LEFT         = 0x0004;
const int INPUT_DOWN         = 0x0002;
const int INPUT_UP           = 0x0001;

/* Master System specific bitmasks */
const int INPUT_BUTTON2      = 0x0020;
const int INPUT_BUTTON1      = 0x0010;

/* Mega Mouse specific bitmask */
const int INPUT_MOUSE_CENTER = 0x0040;
const int INPUT_MOUSE_RIGHT  = 0x0020;
const int INPUT_MOUSE_LEFT   = 0x0010;

/* Pico hardware specific bitmask */
const int INPUT_PICO_PEN     = 0x0080;
const int INPUT_PICO_RED     = 0x0010;

/* XE-1AP specific bitmask */
const int INPUT_XE_E1        = 0x0800;
const int INPUT_XE_E2        = 0x0400;
const int INPUT_XE_START     = 0x0200;
const int INPUT_XE_SELECT    = 0x0100;
const int INPUT_XE_A         = 0x0080;
const int INPUT_XE_B         = 0x0040;
const int INPUT_XE_C         = 0x0020;
const int INPUT_XE_D         = 0x0010;

/* Activator specific bitmasks */
const int INPUT_ACTIVATOR_8U = 0x8000;
const int INPUT_ACTIVATOR_8L = 0x4000;
const int INPUT_ACTIVATOR_7U = 0x2000;
const int INPUT_ACTIVATOR_7L = 0x1000;
const int INPUT_ACTIVATOR_6U = 0x0800;
const int INPUT_ACTIVATOR_6L = 0x0400;
const int INPUT_ACTIVATOR_5U = 0x0200;
const int INPUT_ACTIVATOR_5L = 0x0100;
const int INPUT_ACTIVATOR_4U = 0x0080;
const int INPUT_ACTIVATOR_4L = 0x0040;
const int INPUT_ACTIVATOR_3U = 0x0020;
const int INPUT_ACTIVATOR_3L = 0x0010;
const int INPUT_ACTIVATOR_2U = 0x0008;
const int INPUT_ACTIVATOR_2L = 0x0004;
const int INPUT_ACTIVATOR_1U = 0x0002;
const int INPUT_ACTIVATOR_1L = 0x0001;

struct t_input
{
  u8 system[2];              /* can be one of the SYSTEM_* values */
  u8 dev[MAX_DEVICES];       /* can be one of the DEVICE_* values */
  u16 pad[MAX_DEVICES];      /* digital inputs (any of INPUT_* values)  */
  s16 analog[MAX_DEVICES][2]; /* analog inputs (x/y) */
  int x_offset;                 /* gun horizontal offset */
  int y_offset;                 /* gun vertical offset */
}

t_input input;
int old_system[2] = {-1,-1};


void input_init()
{
  int i;
  int player = 0;

  for (i=0; i<MAX_DEVICES; i++)
  {
    input.dev[i] = NO_DEVICE;
    input.pad[i] = 0;
  }

  /* PICO tablet */
  if (system_hw == SYSTEM_PICO)
  {
    input.dev[0] = DEVICE_PICO;
    return;
  }

  /* Terebi Oekaki tablet */
  if (cart.special & HW_TEREBI_OEKAKI)
  {
    input.dev[0] = DEVICE_TEREBI;
    return;
  }

  switch (input.system[0])
  {
    case SYSTEM_MS_GAMEPAD:
    {
      input.dev[0] = DEVICE_PAD2B;
      player++;
      break;
    }

    case SYSTEM_MD_GAMEPAD:
    {
      input.dev[0] = config.input[player].padtype;
      player++;
      break;
    }

    case SYSTEM_MOUSE:
    {
      input.dev[0] = DEVICE_MOUSE;
      player++;
      break;
    }

    case SYSTEM_ACTIVATOR:
    {
      input.dev[0] = DEVICE_ACTIVATOR;
      player++;
      break;
    }

    case SYSTEM_XE_A1P:
    {
      input.dev[0] = DEVICE_XE_A1P;
      player++;
      break;
    }

    case SYSTEM_WAYPLAY:
    {
      for (i=0; i< 4; i++)
      {
        if (player < MAX_INPUTS)
        {
          input.dev[i] = config.input[player].padtype;
          player++;
        }
      }
      break;
    }

    case SYSTEM_TEAMPLAYER:
    {
      for (i=0; i<4; i++)
      {
        if (player < MAX_INPUTS)
        {
          input.dev[i] = config.input[player].padtype;
          player++;
        }
      }
      teamplayer_init(0);
      break;
    }

    case SYSTEM_LIGHTPHASER:
    {
      input.dev[0] = DEVICE_LIGHTGUN;
      player++;
      break;
    }

    case SYSTEM_PADDLE:
    {
      input.dev[0] = DEVICE_PADDLE;
      player++;
      break;
    }

    case SYSTEM_SPORTSPAD:
    {
      input.dev[0] = DEVICE_SPORTSPAD;
      player++;
      break;
    }
  }

  if (player == MAX_INPUTS)
  {
    return;
  }

  switch (input.system[1])
  {
    case SYSTEM_MS_GAMEPAD:
    {
      input.dev[4] = DEVICE_PAD2B;
      player++;
      break;
    }

    case SYSTEM_MD_GAMEPAD:
    {
      input.dev[4] = config.input[player].padtype;
      player++;
      break;
    }

    case SYSTEM_MOUSE:
    {
      input.dev[4] = DEVICE_MOUSE;
      player++;
      break;
    }

    case SYSTEM_ACTIVATOR:
    {
      input.dev[4] = DEVICE_ACTIVATOR;
      player++;
      break;
    }

    case SYSTEM_MENACER:
    {
      input.dev[4] = DEVICE_LIGHTGUN;
      player++;
      break;
    }

    case SYSTEM_JUSTIFIER:
    {
      for (i=4; i<6; i++)
      {
        if (player < MAX_INPUTS)
        {
          input.dev[i] = DEVICE_LIGHTGUN;
          player++;
        }
      }
      break;
    }

    case SYSTEM_TEAMPLAYER:
    {
      for (i=4; i<8; i++)
      {
        if (player < MAX_INPUTS)
        {
          input.dev[i] = config.input[player].padtype;
          player++;
        }
      }
      teamplayer_init(1);
      break;
    }

    case SYSTEM_LIGHTPHASER:
    {
      input.dev[4] = DEVICE_LIGHTGUN;
      player++;
      break;
    }

    case SYSTEM_PADDLE:
    {
      input.dev[4] = DEVICE_PADDLE;
      player++;
      break;
    }

    case SYSTEM_SPORTSPAD:
    {
      input.dev[4] = DEVICE_SPORTSPAD;
      player++;
      break;
    }
  }

  /* J-CART */
  if (cart.special & HW_J_CART)
  {
    /* two additional gamepads */
    for (i=5; i<7; i++)
    {
      if (player < MAX_INPUTS)
      {
        input.dev[i] = config.input[player].padtype;
        player ++;
      }
    }
  }
}

void input_reset()
{
  /* Reset input devices */
  int i;
  for (i=0; i<MAX_DEVICES; i++)
  {
    switch (input.dev[i])
    {
      case DEVICE_PAD2B:
      case DEVICE_PAD3B:
      case DEVICE_PAD6B:
      {
        gamepad_reset(i);
        break;
      }

      case DEVICE_LIGHTGUN:
      {
        lightgun_reset(i);
        break;
      }

      case DEVICE_MOUSE:
      {
        mouse_reset(i);
        break;
      }

      case DEVICE_ACTIVATOR:
      {
        activator_reset(i >> 2);
        break;
      }

      case DEVICE_XE_A1P:
      {
        xe_a1p_reset();
        break;
      }

      case DEVICE_PADDLE:
      {
        paddle_reset(i >> 2);
        break;
      }

      case DEVICE_SPORTSPAD:
      {
        sportspad_reset(i >> 2);
        break;
      }

      case DEVICE_TEREBI:
      {
        terebi_oekaki_reset();
        break;
      }

      default:
      {
        break;
      }
    }
  }

  /* Team Player */
  for (i=0; i<2; i++)
  {
    if (input.system[i] == SYSTEM_TEAMPLAYER)
    {
      teamplayer_reset(i);
    }
  }
}

void input_refresh()
{
  int i;
  for (i=0; i<MAX_DEVICES; i++)
  {
    switch (input.dev[i])
    {
      case DEVICE_PAD6B:
      {
        gamepad_refresh(i);
        break;
      }

      case DEVICE_LIGHTGUN:
      {
        lightgun_refresh(i);
        break;
      }
    }
  }
}
