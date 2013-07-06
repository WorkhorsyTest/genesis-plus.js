
Genesis Plus JS
=====

A port of Genesis Plus GX to javascript. This project is in 
the very early stages, and does not work yet.

The current plan is:

1.  Replace C macros with functions.
2.  Replace C89 data types (int, unsigned long long, short) with C99 types from stdint.h (s32, u64, s16).
3.  Port code to D, but keep C style of pointers and function pointers.
4.  Move to D style arrays and function pointers.
5.  Replace unions with something browser friendly.
6.  Replace all 8, 16, and 32 bit types with ints.
7.  Port everything to something that runs in the browser: JS, CoffeeScript, Dart, TypeScript, LLJS, Asm.js, Emscripten



For Genesis Plus GX see:
[http://code.google.com/p/genplus-gx/](http://code.google.com/p/genplus-gx/)

Build
-----

    make
    ./gen_sdl.exe game.smd

