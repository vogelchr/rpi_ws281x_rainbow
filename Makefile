CPPFLAGS=-Irpi_ws281x
LIBS=-Lrpi_ws281x -lws2811 -lm

OBJECTS=ws2812_rainbow_rpi.o
CFLAGS=-Wall -Wextra -Wdouble-promotion -ggdb -Os

all : libs ws2812_rainbow_rpi

.PHONY : libs
libs : rpi_ws281x/libws2811.a

.PHONY : clean
clean :
	rm -f *~ *.o rpi_ws281x/*.o rpi_ws281x/*.a ws2812_rainbow_rpi

rpi_ws281x/libws2811.a :
	( cd rpi_ws281x ; scons )

ws2812_rainbow_rpi : $(OBJECTS)
	$(CC) -o $@ $^ $(LIBS)

%.d : %.c
	$(CC) $(CPPFLAGS) -MM -o $@ $^

ifneq ($(MAKECMDGOALS),clean)
include $(OBJECTS:.o=.d)
endif
