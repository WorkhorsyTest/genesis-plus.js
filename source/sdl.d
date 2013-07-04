
// Copied from
// https://github.com/erdemoncel/oyun/blob/master/src/sdl.d

module sdl;

private import std.stdio;

alias byte Sint8;
alias ubyte Uint8;
alias short Sint16;
alias ushort Uint16;
alias int Sint32;
alias uint Uint32;

enum uint SDL_INIT_TIMER = 0x00000001;
enum uint SDL_INIT_AUDIO = 0x00000010;
enum uint SDL_INIT_VIDEO = 0x00000020;
enum uint SDL_INIT_CDROM = 0x00000100;
enum uint SDL_INIT_JOYSTICK = 0x00000200;
enum uint SDL_INIT_NOPARACHUTE = 0x00100000; /* Don't catch fatal signals */
enum uint SDL_INIT_EVENTTHREAD = 0x01000000; /* Not supported on all OS's */
enum uint SDL_INIT_EVERYTHING = 0x0000FFFF;

/* SDL_Surface Flags
These are the currently supported flags for the SDL_surface
*/


/* Available for SDL_CreateRGBSurface() or SDL_SetVideoMode() */
enum uint SDL_SWSURFACE = 0x00000000; /* Surface is in system memory */
enum uint SDL_HWSURFACE = 0x00000001; /* Surface is in video memory */
enum uint SDL_ASYNCBLIT = 0x00000004; /* Use asynchronous blits if possible */

/* Available for SDL_SetVideoMode() */
enum uint SDL_ANYFORMAT = 0x10000000; /* Allow any video depth/pixel-format */
enum uint SDL_HWPALETTE = 0x20000000; /* Surface has exclusive palette */
enum uint SDL_DOUBLEBUF = 0x40000000; /* Set up double-buffered video mode */
enum uint SDL_FULLSCREEN = 0x80000000; /* Surface is a full screen display */
enum uint SDL_OPENGL = 0x00000002; /* Create an OpenGL rendering context */
enum uint SDL_OPENGLBLIT = 0x0000000A; /* Create an OpenGL rendering context
and use it for blitting */
enum uint SDL_RESIZABLE = 0x00000010; /* This video mode may be resized */
enum uint SDL_NOFRAME = 0x00000020; /* No window caption or edge frame */

/* Used internally (read-only) */
enum uint SDL_HWACCEL = 0x00000100; /* Blit uses hardware acceleration */
enum uint SDL_SRCCOLORKEY = 0x00001000; /* Blit uses a source color key */
enum uint SDL_RLEACCELOK = 0x00002000; /* Private flag */
enum uint SDL_RLEACCEL = 0x00004000; /* Surface is RLE encoded */
enum uint SDL_SRCALPHA = 0x00010000; /* Blit uses source alpha blending */
enum uint SDL_PREALLOC = 0x01000000; /* Surface uses preallocated memory */

extern(C) struct SDL_Rect
{
    Sint16 x, y;
    Uint16 w, h;
}

extern(C) struct SDL_Color
{
    Uint8 r;
    Uint8 g;
    Uint8 b;
    Uint8 unused;
}
alias SDL_Color SDL_Colour;

extern(C) struct SDL_Palette
{
    int ncolors;
    SDL_Color *colors;
}

extern(C) struct SDL_PixelFormat
{
    SDL_Palette *palette;
    Uint8 BitsPerPixel;
    Uint8 BytesPerPixel;
    Uint8 Rloss;
    Uint8 Gloss;
    Uint8 Bloss;
    Uint8 Aloss;
    Uint8 Rshift;
    Uint8 Gshift;
    Uint8 Bshift;
    Uint8 Ashift;
    Uint32 Rmask;
    Uint32 Gmask;
    Uint32 Bmask;
    Uint32 Amask;
    /* RGB color key information */
    Uint32 colorkey;
    /* Alpha value information (per-surface alpha) */
    Uint8 alpha;
}

extern(C) struct SDL_Surface
{
    Uint32 flags; /* Read-only */
    SDL_PixelFormat *format; /* Read-only */
    int w, h; /* Read-only */
    Uint16 pitch; /* Read-only */
    void *pixels; /* Read-write */
    int offset; /* Private */

    // Hardware-specific surface info
    void /*struct private_hwdata*/ *hwdata;

    // clipping information
    SDL_Rect clip_rect; /* Read-only */
    Uint32 unused1; /* for binary compatibility */

    // Allow recursive locks
    Uint32 locked; /* Private */

    // info for fast blit mapping to other surfaces */
    void /*struct SDL_BlitMap*/ *map; /* Private */

    // format version, bumped at every change to invalidate blit maps
    uint format_version; /* Private */

    // Reference count -- used when freeing surface
    int refcount; /* Read-mostly */
}

extern(C) struct SDL_RWops
{
    int function(SDL_RWops *context, int offset, int whence) seek;
    int function(SDL_RWops *context, void *ptr, int size, int maxnum) read;
    int function(SDL_RWops *context, const void *ptr, int size, int num) write;
    int function(SDL_RWops *context) close;

    Uint32 type;

    union
    {
        struct
        {
            int autoclose;
            FILE *fp;
        }
        struct
        {
            Uint8 *base;
            Uint8 *here;
            Uint8 *stop;
        }
        struct
        {
            void *data1;
        }
    }
}
alias SDL_UpperBlit SDL_BlitSurface;

extern(C)
{
    alias uint SDLKey;

    enum
    {
        /* ASCII mapped keysyms
The keyboard syms have been cleverly chosen to map to ASCII
*/
        SDLK_UNKNOWN = 0,
        SDLK_FIRST = 0,
        SDLK_BACKSPACE = 8,
        SDLK_TAB = 9,
        SDLK_CLEAR = 12,
        SDLK_RETURN = 13,
        SDLK_PAUSE = 19,
        SDLK_ESCAPE = 27,
        SDLK_SPACE = 32,
        SDLK_EXCLAIM = 33,
        SDLK_QUOTEDBL = 34,
        SDLK_HASH = 35,
        SDLK_DOLLAR = 36,
        SDLK_AMPERSAND = 38,
        SDLK_QUOTE = 39,
        SDLK_LEFTPAREN = 40,
        SDLK_RIGHTPAREN = 41,
        SDLK_ASTERISK = 42,
        SDLK_PLUS = 43,
        SDLK_COMMA = 44,
        SDLK_MINUS = 45,
        SDLK_PERIOD = 46,
        SDLK_SLASH = 47,
        SDLK_0 = 48,
        SDLK_1 = 49,
        SDLK_2 = 50,
        SDLK_3 = 51,
        SDLK_4 = 52,
        SDLK_5 = 53,
        SDLK_6 = 54,
        SDLK_7 = 55,
        SDLK_8 = 56,
        SDLK_9 = 57,
        SDLK_COLON = 58,
        SDLK_SEMICOLON = 59,
        SDLK_LESS = 60,
        SDLK_EQUALS = 61,
        SDLK_GREATER = 62,
        SDLK_QUESTION = 63,
        SDLK_AT = 64,
        /*
Skip uppercase letters
*/
        SDLK_LEFTBRACKET = 91,
        SDLK_BACKSLASH = 92,
        SDLK_RIGHTBRACKET= 93,
        SDLK_CARET = 94,
        SDLK_UNDERSCORE = 95,
        SDLK_BACKQUOTE = 96,
        SDLK_a = 97,
        SDLK_b = 98,
        SDLK_c = 99,
        SDLK_d = 100,
        SDLK_e = 101,
        SDLK_f = 102,
        SDLK_g = 103,
        SDLK_h = 104,
        SDLK_i = 105,
        SDLK_j = 106,
        SDLK_k = 107,
        SDLK_l = 108,
        SDLK_m = 109,
        SDLK_n = 110,
        SDLK_o = 111,
        SDLK_p = 112,
        SDLK_q = 113,
        SDLK_r = 114,
        SDLK_s = 115,
        SDLK_t = 116,
        SDLK_u = 117,
        SDLK_v = 118,
        SDLK_w = 119,
        SDLK_x = 120,
        SDLK_y = 121,
        SDLK_z = 122,
        SDLK_DELETE = 127,
        /* End of ASCII mapped keysyms */

        /* International keyboard syms */
        SDLK_WORLD_0 = 160, /* 0xA0 */
        SDLK_WORLD_1 = 161,
        SDLK_WORLD_2 = 162,
        SDLK_WORLD_3 = 163,
        SDLK_WORLD_4 = 164,
        SDLK_WORLD_5 = 165,
        SDLK_WORLD_6 = 166,
        SDLK_WORLD_7 = 167,
        SDLK_WORLD_8 = 168,
        SDLK_WORLD_9 = 169,
        SDLK_WORLD_10 = 170,
        SDLK_WORLD_11 = 171,
        SDLK_WORLD_12 = 172,
        SDLK_WORLD_13 = 173,
        SDLK_WORLD_14 = 174,
        SDLK_WORLD_15 = 175,
        SDLK_WORLD_16 = 176,
        SDLK_WORLD_17 = 177,
        SDLK_WORLD_18 = 178,
        SDLK_WORLD_19 = 179,
        SDLK_WORLD_20 = 180,
        SDLK_WORLD_21 = 181,
        SDLK_WORLD_22 = 182,
        SDLK_WORLD_23 = 183,
        SDLK_WORLD_24 = 184,
        SDLK_WORLD_25 = 185,
        SDLK_WORLD_26 = 186,
        SDLK_WORLD_27 = 187,
        SDLK_WORLD_28 = 188,
        SDLK_WORLD_29 = 189,
        SDLK_WORLD_30 = 190,
        SDLK_WORLD_31 = 191,
        SDLK_WORLD_32 = 192,
        SDLK_WORLD_33 = 193,
        SDLK_WORLD_34 = 194,
        SDLK_WORLD_35 = 195,
        SDLK_WORLD_36 = 196,
        SDLK_WORLD_37 = 197,
        SDLK_WORLD_38 = 198,
        SDLK_WORLD_39 = 199,
        SDLK_WORLD_40 = 200,
        SDLK_WORLD_41 = 201,
        SDLK_WORLD_42 = 202,
        SDLK_WORLD_43 = 203,
        SDLK_WORLD_44 = 204,
        SDLK_WORLD_45 = 205,
        SDLK_WORLD_46 = 206,
        SDLK_WORLD_47 = 207,
        SDLK_WORLD_48 = 208,
        SDLK_WORLD_49 = 209,
        SDLK_WORLD_50 = 210,
        SDLK_WORLD_51 = 211,
        SDLK_WORLD_52 = 212,
        SDLK_WORLD_53 = 213,
        SDLK_WORLD_54 = 214,
        SDLK_WORLD_55 = 215,
        SDLK_WORLD_56 = 216,
        SDLK_WORLD_57 = 217,
        SDLK_WORLD_58 = 218,
        SDLK_WORLD_59 = 219,
        SDLK_WORLD_60 = 220,
        SDLK_WORLD_61 = 221,
        SDLK_WORLD_62 = 222,
        SDLK_WORLD_63 = 223,
        SDLK_WORLD_64 = 224,
        SDLK_WORLD_65 = 225,
        SDLK_WORLD_66 = 226,
        SDLK_WORLD_67 = 227,
        SDLK_WORLD_68 = 228,
        SDLK_WORLD_69 = 229,
        SDLK_WORLD_70 = 230,
        SDLK_WORLD_71 = 231,
        SDLK_WORLD_72 = 232,
        SDLK_WORLD_73 = 233,
        SDLK_WORLD_74 = 234,
        SDLK_WORLD_75 = 235,
        SDLK_WORLD_76 = 236,
        SDLK_WORLD_77 = 237,
        SDLK_WORLD_78 = 238,
        SDLK_WORLD_79 = 239,
        SDLK_WORLD_80 = 240,
        SDLK_WORLD_81 = 241,
        SDLK_WORLD_82 = 242,
        SDLK_WORLD_83 = 243,
        SDLK_WORLD_84 = 244,
        SDLK_WORLD_85 = 245,
        SDLK_WORLD_86 = 246,
        SDLK_WORLD_87 = 247,
        SDLK_WORLD_88 = 248,
        SDLK_WORLD_89 = 249,
        SDLK_WORLD_90 = 250,
        SDLK_WORLD_91 = 251,
        SDLK_WORLD_92 = 252,
        SDLK_WORLD_93 = 253,
        SDLK_WORLD_94 = 254,
        SDLK_WORLD_95 = 255, /* 0xFF */

        /* Numeric keypad */
        SDLK_KP0 = 256,
        SDLK_KP1 = 257,
        SDLK_KP2 = 258,
        SDLK_KP3 = 259,
        SDLK_KP4 = 260,
        SDLK_KP5 = 261,
        SDLK_KP6 = 262,
        SDLK_KP7 = 263,
        SDLK_KP8 = 264,
        SDLK_KP9 = 265,
        SDLK_KP_PERIOD = 266,
        SDLK_KP_DIVIDE = 267,
        SDLK_KP_MULTIPLY= 268,
        SDLK_KP_MINUS = 269,
        SDLK_KP_PLUS = 270,
        SDLK_KP_ENTER = 271,
        SDLK_KP_EQUALS = 272,

        /* Arrows + Home/End pad */
        SDLK_UP = 273,
        SDLK_DOWN = 274,
        SDLK_RIGHT = 275,
        SDLK_LEFT = 276,
        SDLK_INSERT = 277,
        SDLK_HOME = 278,
        SDLK_END = 279,
        SDLK_PAGEUP = 280,
        SDLK_PAGEDOWN = 281,

        /* Function keys */
        SDLK_F1 = 282,
        SDLK_F2 = 283,
        SDLK_F3 = 284,
        SDLK_F4 = 285,
        SDLK_F5 = 286,
        SDLK_F6 = 287,
        SDLK_F7 = 288,
        SDLK_F8 = 289,
        SDLK_F9 = 290,
        SDLK_F10 = 291,
        SDLK_F11 = 292,
        SDLK_F12 = 293,
        SDLK_F13 = 294,
        SDLK_F14 = 295,
        SDLK_F15 = 296,

        /* Key state modifier keys */
        SDLK_NUMLOCK = 300,
        SDLK_CAPSLOCK = 301,
        SDLK_SCROLLOCK = 302,
        SDLK_RSHIFT = 303,
        SDLK_LSHIFT = 304,
        SDLK_RCTRL = 305,
        SDLK_LCTRL = 306,
        SDLK_RALT = 307,
        SDLK_LALT = 308,
        SDLK_RMETA = 309,
        SDLK_LMETA = 310,
        SDLK_LSUPER = 311, /* Left "Windows" key */
        SDLK_RSUPER = 312, /* Right "Windows" key */
        SDLK_MODE = 313, /* "Alt Gr" key */
        SDLK_COMPOSE = 314, /* Multi-key compose key */
                                        
        /* Miscellaneous function keys */
        SDLK_HELP = 315,
        SDLK_PRINT = 316,
        SDLK_SYSREQ = 317,
        SDLK_BREAK = 318,
        SDLK_MENU = 319,
        SDLK_POWER = 320, /* Power Macintosh power key */
        SDLK_EURO = 321, /* Some european keyboards */
        SDLK_UNDO = 322, /* Atari keyboard has Undo */
                                        
        /* Add any other keys here */
        SDLK_LAST
    }
}

extern(C)
{
    alias uint SDLMod;
    
    enum
    {
        KMOD_NONE = 0x0000,
        KMOD_LSHIFT = 0x0001,
        KMOD_RSHIFT = 0x0002,
        KMOD_LCTRL = 0x0040,
        KMOD_RCTRL = 0x0080,
        KMOD_LALT = 0x0100,
        KMOD_RALT = 0x0200,
        KMOD_LMETA = 0x0400,
        KMOD_RMETA = 0x0800,
        KMOD_NUM = 0x1000,
        KMOD_CAPS = 0x2000,
        KMOD_MODE = 0x4000,
        KMOD_RESERVED = 0x8000
     }
}

extern(C) struct SDL_ActiveEvent
{
    Uint8 type; /* SDL_ACTIVEEVENT */
    Uint8 gain; /* Whether given states were gained or lost (1/0) */
    Uint8 state; /* A mask of the focus states */
}

/** Keyboard event structure */
extern(C) struct SDL_KeyboardEvent
{
    Uint8 type; /* SDL_KEYDOWN or SDL_KEYUP */
    Uint8 which; /* The keyboard device index */
    Uint8 state; /* SDL_PRESSED or SDL_RELEASED */
    SDL_keysym keysym;
}

/** Mouse motion event structure */
extern(C) struct SDL_MouseMotionEvent
{
    Uint8 type; /* SDL_MOUSEMOTION */
    Uint8 which; /* The mouse device index */
    Uint8 state; /* The current button state */
    Uint16 x, y; /* The X/Y coordinates of the mouse */
    Sint16 xrel; /* The relative motion in the X direction */
    Sint16 yrel; /* The relative motion in the Y direction */
}

/** Mouse button event structure */
extern(C) struct SDL_MouseButtonEvent
{
    Uint8 type; /* SDL_MOUSEBUTTONDOWN or SDL_MOUSEBUTTONUP */
    Uint8 which; /* The mouse device index */
    Uint8 button; /* The mouse button index */
    Uint8 state; /* SDL_PRESSED or SDL_RELEASED */
    Uint16 x, y; /* The X/Y coordinates of the mouse at press time */
}

/** Joystick axis motion event structure */
extern(C) struct SDL_JoyAxisEvent
{
    Uint8 type; /* SDL_JOYAXISMOTION */
    Uint8 which; /* The joystick device index */
    Uint8 axis; /* The joystick axis index */
    Sint16 value; /* The axis value (range: -32768 to 32767) */
}

/** Joystick trackball motion event structure */
extern(C) struct SDL_JoyBallEvent
{
    Uint8 type; /* SDL_JOYBALLMOTION */
    Uint8 which; /* The joystick device index */
    Uint8 ball; /* The joystick trackball index */
    Sint16 xrel; /* The relative motion in the X direction */
    Sint16 yrel; /* The relative motion in the Y direction */
}

/** Joystick hat position change event structure */
extern(C) struct SDL_JoyHatEvent
{
    Uint8 type; /* SDL_JOYHATMOTION */
    Uint8 which; /* The joystick device index */
    Uint8 hat; /* The joystick hat index */
    Uint8 value; /* The hat position value:
* SDL_HAT_LEFTUP SDL_HAT_UP SDL_HAT_RIGHTUP
* SDL_HAT_LEFT SDL_HAT_CENTERED SDL_HAT_RIGHT
* SDL_HAT_LEFTDOWN SDL_HAT_DOWN SDL_HAT_RIGHTDOWN
* Note that zero means the POV is centered.
*/
}

/** Joystick button event structure */
extern(C) struct SDL_JoyButtonEvent
{
    Uint8 type; /* SDL_JOYBUTTONDOWN or SDL_JOYBUTTONUP */
    Uint8 which; /* The joystick device index */
    Uint8 button; /* The joystick button index */
    Uint8 state; /* SDL_PRESSED or SDL_RELEASED */
}

/** The "window resized" event
* When you get this event, you are responsible for setting a new video
* mode with the new width and height.
*/
extern(C) struct SDL_ResizeEvent
{
    Uint8 type; /* SDL_VIDEORESIZE */
    int w; /* New width */
    int h; /* New height */
}

/** The "screen redraw" event */
extern(C) struct SDL_ExposeEvent
{
    Uint8 type; /* SDL_VIDEOEXPOSE */
}

/** The "quit requested" event */
extern(C) struct SDL_QuitEvent
{
    Uint8 type; /* SDL_QUIT */
}

extern(C) struct SDL_UserEvent
{
    Uint8 type; /* SDL_USEREVENT through SDL_NUMEVENTS-1 */
    int code; /* User defined event code */
    void *data1; /* User defined data pointer */
    void *data2; /* User defined data pointer */
}


extern (C) struct SDL_SysWMmsg;
// alias SDL_SysWMmsg SDL_SysWMmsg;
struct SDL_SysWMEvent
{
    Uint8 type;
    SDL_SysWMmsg *msg;
}

extern(C) struct SDL_keysym
{
    Uint8 scancode; /* hardware specific scancode */
    SDLKey sym; /* SDL virtual keysym */
    SDLMod mod; /* current key modifiers */
    Uint16 unicode; /* translated character */
}

extern(C) union SDL_Event
{
    Uint8 type;
    SDL_ActiveEvent active;
    SDL_KeyboardEvent key;
    SDL_MouseMotionEvent motion;
    SDL_MouseButtonEvent button;
    SDL_JoyAxisEvent jaxis;
    SDL_JoyBallEvent jball;
    SDL_JoyHatEvent jhat;
    SDL_JoyButtonEvent jbutton;
    SDL_ResizeEvent resize;
    SDL_ExposeEvent expose;
    SDL_QuitEvent quit;
    SDL_UserEvent user;
    SDL_SysWMEvent syswm;
}


extern(C) enum
{
    SDL_NOEVENT = 0, /* Unused (do not remove) */
    SDL_ACTIVEEVENT, /* Application loses/gains visibility */
    SDL_KEYDOWN, /* Keys pressed */
    SDL_KEYUP, /* Keys released */
    SDL_MOUSEMOTION, /* Mouse moved */
    SDL_MOUSEBUTTONDOWN, /* Mouse button pressed */
    SDL_MOUSEBUTTONUP, /* Mouse button released */
    SDL_JOYAXISMOTION, /* Joystick axis motion */
    SDL_JOYBALLMOTION, /* Joystick trackball motion */
    SDL_JOYHATMOTION, /* Joystick hat position change */
    SDL_JOYBUTTONDOWN, /* Joystick button pressed */
    SDL_JOYBUTTONUP, /* Joystick button released */
    SDL_QUIT, /* User-requested quit */
    SDL_SYSWMEVENT, /* System specific event */
    SDL_EVENT_RESERVEDA, /* Reserved for future use.. */
    SDL_EVENT_RESERVEDB, /* Reserved for future use.. */
    SDL_VIDEORESIZE, /* User resized video mode */
    SDL_VIDEOEXPOSE, /* Screen needs to be redrawn */
    SDL_EVENT_RESERVED2, /* Reserved for future use.. */
    SDL_EVENT_RESERVED3, /* Reserved for future use.. */
    SDL_EVENT_RESERVED4, /* Reserved for future use.. */
    SDL_EVENT_RESERVED5, /* Reserved for future use.. */
    SDL_EVENT_RESERVED6, /* Reserved for future use.. */
    SDL_EVENT_RESERVED7, /* Reserved for future use.. */
    /** Events SDL_USEREVENT through SDL_MAXEVENTS-1 are for your use */
    SDL_USEREVENT = 24,
    /** This last event is only for bounding internal arrays
* It is the number of bits in the event mask datatype -- Uint32
*/
    SDL_NUMEVENTS = 32
}

extern(C)
{
    int SDL_FillRect (SDL_Surface *dst, SDL_Rect *dstrect, Uint32 color);

    int SDL_UpperBlit(SDL_Surface *src, SDL_Rect *srcrect,
                      SDL_Surface *dst, SDL_Rect *dstrect);
    int SDL_LowerBlit(SDL_Surface *src, SDL_Rect *srcrect,
                      SDL_Surface *dst, SDL_Rect *dstrect);
    int SDL_Flip(SDL_Surface *screen);
    int SDL_Init(Uint32 flags);
    int SDL_PollEvent(SDL_Event *event);
    int SDL_SetColorKey(SDL_Surface *surface, Uint32 flag, Uint32 key);
    int SDL_LockSurface(SDL_Surface *surface);

    Uint32 SDL_MapRGB (const SDL_PixelFormat * format,
                       const Uint8 r, const Uint8 g, const Uint8 b);

    Uint8 * SDL_GetKeyState(int *numkeys);
    Uint32 SDL_GetTicks();
    SDL_Surface * SDL_DisplayFormat(SDL_Surface *surface);
    SDL_Surface * SDL_SetVideoMode(int width, int height, int bpp, Uint32 flags);
    SDL_Surface * SDL_LoadBMP (const char * file)
    {
        return SDL_LoadBMP_RW(SDL_RWFromFile(file, "rb"), 1);
    }
    bool SDL_MUSTLOCK(SDL_Surface * surface)
    {
        return (surface.offset ||
                ((surface.flags & (SDL_HWSURFACE|SDL_ASYNCBLIT|SDL_RLEACCEL)) != 0));
    }

    SDL_Surface * SDL_LoadBMP_RW(SDL_RWops * src, int freesrc);
    SDL_Surface * IMG_Load(const char *file);
    SDL_Surface * SDL_DisplayFormatAlpha(SDL_Surface *surface);
    SDL_RWops * SDL_RWFromFile(const char * file, const char * mode);

    void SDL_GL_SwapBuffers();
    void SDL_FreeSurface(SDL_Surface *surface);
    void SDL_Delay(Uint32 ms);
    void SDL_Quit();
    void SDL_UnlockSurface(SDL_Surface *surface);
    void SDL_UpdateRect(SDL_Surface *screen, Sint32 x, Sint32 y, Uint32 w, Uint32 h);
    void SDL_WM_SetCaption(const char * title, const char * icon);
/*
* Yeni Eklenenler
*/
   void SDL_WM_SetIcon(SDL_Surface *icon, Uint8 *mask);
   int SDL_WM_ToggleFullScreen(SDL_Surface *surface);
   SDL_Surface* SDL_ConvertSurface(SDL_Surface* src,
                                   SDL_PixelFormat* fmt,
                                   Uint32 flags);
   char* SDL_GetError();
   void SDL_SetError(const char* fmt, ...);
   int SDL_WaitEvent(SDL_Event* event);
   int SDL_EnableKeyRepeat(int delay, int interval);
   int SDL_SetAlpha(SDL_Surface* surface, Uint32 flags, Uint8 alpha);
   SDL_Surface* SDL_CreateRGBSurface(Uint32 flags, int width, int height, int depth,
                                     Uint32 Rmask,
                                     Uint32 Gmask,
                                     Uint32 Bmask,
                                     Uint32 Amask);
/*** Yeni Eklenenler ***/

}

extern(C) // dmd sdl -debug -L-lSDL -L-lSDL_ttf
{
    /* The internal structure containing font information */
    struct TTF_Font;

    /* Initialize the TTF engine - returns 0 if successful, -1 on error */
    int TTF_Init();

    TTF_Font* TTF_OpenFont(const char* file, int ptsize);
    
    TTF_Font* TTF_OpenFontIndex(const char* file, int ptsize,
                                                  long index);
    
    TTF_Font* TTF_OpenFontRW(SDL_RWops* src, int freesrc,
                                             int ptsize);
    
    TTF_Font* TTF_OpenFontIndexRW(SDL_RWops* src, int freesrc,
                                                  int ptsize,
                                                  long index);
    /* Set and retrieve the font style */
    enum int TTF_STYLE_NORMAL = 0x00;
    enum int TTF_STYLE_BOLD = 0x01;
    enum int TTF_STYLE_ITALIC = 0x02;
    enum int TTF_STYLE_UNDERLINE = 0x04;
    /*
* This font style is implemented by modifying the font glyphs, and
* doesn't reflect any inherent properties of the truetype font file.
*/
    int TTF_GetFontStyle(const TTF_Font* font);
    void TTF_SetFontStyle(TTF_Font* font, int style);

    /* Get the total height of the font - usually equal to point size */
    int TTF_FontHeight(const TTF_Font* font);

    /* Get the offset from the baseline to the top of the font */
    int TTF_FontAscent(const TTF_Font* font);

    /* Get the offset from the baseline to the bottom of the font */
    int TTF_FontDescent(const TTF_Font* font);

    /* Get the recommended spacing between lines of text for this font */
    int TTF_FontLineSkip(const TTF_Font* font);

    /* Get the number of faces of the font */
    long TTF_FontFaces(const TTF_Font* font);

    /* Get the font face attributes, if any */
    int TTF_FontFaceIsFixedWidth(const TTF_Font* font);
    char* TTF_FontFaceFamilyName(const TTF_Font* font);
    char* TTF_FontFaceStyleName(const TTF_Font* font);

    /* Get the metrics (dimensions) of a glyph */
    int TTF_GlyphMetrics(TTF_Font* font, Uint16 ch, int* minx, int* maxx,
                                     int* miny, int* maxy, int* advance);
                                     
    /* Create an 8-bit palettized surface and render the given text at fast
* quality with the given font and color. The 0 px is the colorkey,
* giving a transparent background, and the 1 px is set to the text color.
*/
    SDL_Surface* TTF_RenderText_Shaded(TTF_Font* font, const char* text,
                                             SDL_Color fg, SDL_Color bg);
                                       
    SDL_Surface* TTF_RenderGlyph_Solid(TTF_Font* font, Uint16 ch,
                                                   SDL_Color fg);

    SDL_Surface* TTF_RenderText_Solid(TTF_Font* font, const char* text,
                                                         SDL_Color fg);

    /* Get the dimensions of a rendered string of text */
    int TTF_SizeText(TTF_Font* font, const char* text, int* w, int* h);
    int TTF_SizeUTF8(TTF_Font* font, const char* text, int* w, int* h);
    int TTF_SizeUNICODE(TTF_Font* font, const Uint16* text, int* w, int* h);
    
    /* ZERO WIDTH NO-BREAKSPACE (Unicode byte order mark) */
    enum int UNICODE_BOM_NATIVE = 0xFEFF;
    enum int UNICODE_BOM_SWAPPED = 0xFFFE;

    /* This function tells the library whether UNICODE text is generally
* byteswapped. A UNICODE BOM character in a string will override
* this setting for the remainder of that string.
*/
    void TTF_ByteSwappedUNICODE(int swapped);

    SDL_Surface* TTF_RenderUTF8_Shaded(TTF_Font* font, const char* text,
                                            SDL_Color fg, SDL_Color bg);

    SDL_Surface* TTF_RenderUNICODE_Shaded(TTF_Font* font, const Uint16* text,
                                                 SDL_Color fg, SDL_Color bg);

    SDL_Surface* TTF_RenderUTF8_Solid(TTF_Font* font, const char* text,
                                                         SDL_Color fg);

    SDL_Surface* TTF_RenderUNICODE_Solid(TTF_Font* font, const Uint16* text,
                                                              SDL_Color fg);
    /* Close an opened font file */
    void TTF_CloseFont(TTF_Font* font);

    /* De-initialize the TTF engine */
    void TTF_Quit();

    /* Check if the TTF engine is initialized */
    int TTF_WasInit();

    /* We'll use SDL for reporting errors */
    alias SDL_SetError TTF_SetError;
    alias SDL_GetError TTF_GetError;
}

alias SDL_Surface* Grafik2D; // silsek mi?
alias SDL_Surface* EkranYüzeyi; // ... struct
alias SDL_Rect Dörtgen; // ... struct
alias SDL_Event Olay; // ... union
alias SDL_PollEvent Olaylarıİşle; // ... func (SDL_Event *);
alias SDL_Flip EkranaBas; // ... func (SDL_Surface *);
alias SDL_SetVideoMode EkranModunuAyarla; // ... func (int, int, int, Uint32);
alias SDL_WM_SetCaption UygulamaTanımla; // ... func (char *, char *);
alias TTF_Font* YazıTipi;

struct Renk
{
    ubyte k;
    ubyte y;
    ubyte m;
    ubyte a;
    static {
        enum turkuaz = Renk(100, 149, 237, 0);
        enum mavi = Renk(0, 0, 255, 0);
        enum kırmızı = Renk(255, 0, 0, 0);
        enum yeşil = Renk(0, 255, 0, 0);
        enum beyaz = Renk(255, 255, 255, 0);
        enum siyah = Renk(0, 0, 0, 0);
    }
}

debug {
    void main() {
        SDL_Init(SDL_INIT_EVERYTHING);
        EkranYüzeyi ekran = EkranModunuAyarla(100, 100, 0, SDL_HWSURFACE);
        assert(ekran != null);
        UygulamaTanımla("SDL", "Çalışıyor...");
        SDL_Delay(999);
        "SDL""-ok-"
        .writeln();
    }
}


