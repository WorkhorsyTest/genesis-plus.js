
# Makefile for Genesis Plus D
#
# (c) 1999, 2000, 2001, 2002, 2003  Charles MacDonald
# modified by Eke-Eke <eke_eke31@yahoo.fr>
# (c) 2013 Matthew Brennan Jones <mattjones@workhorsy.org>
#
# Defines :
# -DLSB_FIRST : for little endian systems.
# -DLOGERROR  : enable message logging
# -DLOGVDP    : enable VDP debug messages
# -DLOGSOUND  : enable AUDIO debug messages
# -DLOG_SCD   : enable SCD debug messages
# -DLOG_CDD   : enable CDD debug messages
# -DLOG_CDC   : enable CDC debug messages
# -DLOG_PCM   : enable PCM debug messages
# -DLOGSOUND  : enable AUDIO debug messages

NAME      = gen_sdl.exe

CC        = dmd
CFLAGS    = -g #-O
DEFINES   = -version=LSB_FIRST

INCLUDES  = \
		-Isource \
		-Isource/z80 \
		-Isource/m68k \
		-Isource/sound \
		-Isource/input_hw \
		-Isource/cart_hw \
		-Isource/cd_hw \
		-Isource/cart_hw/svp \
		-Isource/ntsc -Isource/win

LIBS      = -L-lSDL

OBJDIR    = ./build_sdl

OBJECTS   = 

OBJECTS += $(OBJDIR)/z80.o

OBJECTS += \
		$(OBJDIR)/m68kcpu.o \
		$(OBJDIR)/s68kcpu.o

OBJECTS += \
		$(OBJDIR)/genesis.o \
		$(OBJDIR)/vdp_ctrl.o \
		$(OBJDIR)/vdp_render.o \
		$(OBJDIR)/system.o \
		$(OBJDIR)/io_ctrl.o \
		$(OBJDIR)/mem68k.o \
		$(OBJDIR)/memz80.o \
		$(OBJDIR)/membnk.o \
		$(OBJDIR)/state.o \
		$(OBJDIR)/loadrom.o

OBJECTS += \
		$(OBJDIR)/input.o \
		$(OBJDIR)/gamepad.o \
		$(OBJDIR)/lightgun.o \
		$(OBJDIR)/mouse.o \
		$(OBJDIR)/activator.o \
		$(OBJDIR)/xe_a1p.o \
		$(OBJDIR)/teamplayer.o \
		$(OBJDIR)/paddle.o \
		$(OBJDIR)/sportspad.o \
		$(OBJDIR)/terebi_oekaki.o

OBJECTS += \
		$(OBJDIR)/sound.o \
		$(OBJDIR)/sn76489.o \
		$(OBJDIR)/ym2413.o \
		$(OBJDIR)/ym2612.o

OBJECTS += $(OBJDIR)/blip_buf.o

OBJECTS += $(OBJDIR)/eq.o

OBJECTS += \
		$(OBJDIR)/sram.o \
		$(OBJDIR)/svp.o \
		$(OBJDIR)/ssp16.o \
		$(OBJDIR)/ggenie.o \
		$(OBJDIR)/areplay.o \
		$(OBJDIR)/eeprom_93c.o \
		$(OBJDIR)/eeprom_i2c.o \
		$(OBJDIR)/eeprom_spi.o \
		$(OBJDIR)/md_cart.o \
		$(OBJDIR)/sms_cart.o

OBJECTS += \
		$(OBJDIR)/scd.o \
		$(OBJDIR)/cdd.o \
		$(OBJDIR)/cdc.o \
		$(OBJDIR)/gfx.o \
		$(OBJDIR)/pcm.o \
		$(OBJDIR)/cd_cart.o

OBJECTS += \
		$(OBJDIR)/sms_ntsc.o \
		$(OBJDIR)/md_ntsc.o

OBJECTS += \
		$(OBJDIR)/main.o \
		$(OBJDIR)/config.o \
		$(OBJDIR)/error.o \
		$(OBJDIR)/unzip.o \
		$(OBJDIR)/fileio.o

#OBJECTS	+=	$(OBJDIR)/icon.o

all: $(NAME)

$(NAME): $(OBJDIR) $(OBJECTS)
		$(CC) $(LDFLAGS) $(OBJECTS) $(LIBS) -of$@

$(OBJDIR):
		@[ -d $@ ] || mkdir -p $@

$(OBJDIR)/%.o : source/%.d
		$(CC) -c $(CFLAGS) $(DEFINES) $(INCLUDES) $< -of$@

$(OBJDIR)/%.o : source/sound/%.d
		$(CC) -c $(CFLAGS) $(DEFINES) $(INCLUDES) $< -of$@

$(OBJDIR)/%.o : source/input_hw/%.d
		$(CC) -c $(CFLAGS) $(DEFINES) $(INCLUDES) $< -of$@

$(OBJDIR)/%.o : source/cart_hw/%.d
		$(CC) -c $(CFLAGS) $(DEFINES) $(INCLUDES) $< -of$@

$(OBJDIR)/%.o : source/cart_hw/svp/%.d
		$(CC) -c $(CFLAGS) $(DEFINES) $(INCLUDES) $< -of$@

$(OBJDIR)/%.o : source/cart_hw/svp/%.d
		$(CC) -c $(CFLAGS) $(DEFINES) $(INCLUDES) $< -of$@

$(OBJDIR)/%.o : source/cd_hw/%.d
		$(CC) -c $(CFLAGS) $(DEFINES) $(INCLUDES) $< -of$@

$(OBJDIR)/%.o : source/z80/%.d
		$(CC) -c $(CFLAGS) $(DEFINES) $(INCLUDES) $< -of$@

$(OBJDIR)/%.o : source/m68k/%.d
		$(CC) -c $(CFLAGS) $(DEFINES) $(INCLUDES) $< -of$@

$(OBJDIR)/%.o : source/ntsc/%.d
		$(CC) -c $(CFLAGS) $(DEFINES) $(INCLUDES) $< -of$@

$(OBJDIR)/%.o : source/win/%.d
		$(CC) -c $(CFLAGS) $(DEFINES) $(INCLUDES) $< -of$@

#$(OBJDIR)/icon.o :
#		windres source/win/icon.rc $@

pack:
		strip $(NAME)
		upx -9 $(NAME)

clean:
	rm -f $(OBJECTS) $(NAME)
	rm -rf -f $(OBJDIR)


