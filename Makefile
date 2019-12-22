CPPFLAGS:=-Irpi_ws281x $(shell pkgconf --cflags SDL2_image)

LIBS_RAINBOW:=-Lrpi_ws281x -lws2811 -lm
LIBS_SIGN:=$(LIBS_RAINBOW) $(shell pkgconf --libs SDL2_image)

OBJ_RAINBOW=ws2812_rainbow_rpi.o
OBJ_SIGN=ws2812_sign_rpi.o

CFLAGS=-Wall -Wextra -Wdouble-promotion -ggdb -Os

all : libs ws2812_rainbow_rpi ws2812_sign_rpi

.PHONY : libs
libs : rpi_ws281x/libws2811.a

.PHONY : clean
clean :
	rm -f *~ *.o *.d rpi_ws281x/*.o rpi_ws281x/*.a ws2812_rainbow_rpi ws2812_sign_rpi

rpi_ws281x/libws2811.a :
	( cd rpi_ws281x ; scons )

ws2812_rainbow_rpi : $(OBJ_RAINBOW)
	$(CC) -o $@ $^ $(LIBS_RAINBOW)

ws2812_sign_rpi : $(OBJ_SIGN)
	$(CC) -o $@ $^ $(LIBS_SIGN)

%.d : %.c
	$(CC) $(CPPFLAGS) -MM -o $@ $^

ifneq ($(MAKECMDGOALS),clean)
include $(OBJ_RAINBOW:.o=.d)
endif
