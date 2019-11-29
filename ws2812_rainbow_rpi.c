#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <stdint.h> // ws2811.h needs uint32_t ...
#include <math.h>

#include <ws2811.h>

#define GPIO_PIN                18
#define DMANUM                     10
//#define STRIP_TYPE            WS2811_STRIP_RGB		// WS2812/SK6812RGB integrated chip+leds
//#define STRIP_TYPE              WS2811_STRIP_GBR		// WS2812/SK6812RGB integrated chip+leds
#define STRIP_TYPE            SK6812_STRIP_RGBW		// SK6812RGBW (NOT SK6812RGB)
#define LED_COUNT               112

#define TWOPIf (((float)(M_PI))*2.0f)

ws2811_t ledstring = {
	.freq = WS2811_TARGET_FREQ,
	.dmanum = DMANUM,
	.channel = {
		[0] = {
			.gpionum = GPIO_PIN,
			.count = LED_COUNT,
			.invert = 0,
			.brightness = 255,
			.strip_type = STRIP_TYPE
		}, [1] = {
			/* fill with zero */
		}
	}
};

volatile int keep_running;

static void
sighandler_int_term(int i)
{
	(void) i;
	keep_running = 0;
}

static float
deg_to_rad(float deg) {
	return deg * (((float)M_PI)/180.0f);
}

static float
single_color(float phase) {
	return powf((0.5f * (sinf(phase) + 1.0f)),3.0f);
}

uint32_t
gen_new_pixel(float max_bright) {
	static float rgb_phase = 0.0f;
	static float saturation_phase = 0.0f;
	float r, g, b, w, sat;
	uint8_t r8, g8, b8, w8;

	rgb_phase += deg_to_rad(5.0f);
	saturation_phase += deg_to_rad(0.5f);

	r = single_color(rgb_phase);
	g = single_color(rgb_phase + deg_to_rad(120.0f));
	b = single_color(rgb_phase + deg_to_rad(240.0f));
	sat = single_color(saturation_phase);

	r *= sat;
	g *= sat;
	b *= sat;
	w = 1.0f-sat;

	r8 = rintf(max_bright * r);
	g8 = rintf(max_bright * g);
	b8 = rintf(max_bright * b);
	w8 = rintf(max_bright * w);

	if (rgb_phase > TWOPIf)
		rgb_phase -= TWOPIf;
	if (saturation_phase > TWOPIf)
		saturation_phase -= TWOPIf;

//	printf("rgb=%5.3f sat=%5.3f\n", rgb_phase, saturation_phase);

	return r8 | (g8 << 8) | (b8 << 16) | (w8 << 24);
}

int
main(int argc, char **argv) {
	int ret;

	fprintf(stderr, "Hello, world!\n");

	signal(SIGINT, &sighandler_int_term);
	signal(SIGTERM, &sighandler_int_term);

	if ((ret = ws2811_init(&ledstring)) != WS2811_SUCCESS) {
		fprintf(stderr, "ws2811_init failed: %s\n", ws2811_get_return_t_str(ret));
		return ret;
	}

	memset(ledstring.channel[0].leds, '\0', sizeof(uint32_t) * ledstring.channel[0].count);

	keep_running = 1;
	while (keep_running) {
		usleep(33333);

		memmove((void*)(ledstring.channel[0].leds) + sizeof(uint32_t),
			(void*)(ledstring.channel[0].leds),
			sizeof(uint32_t) * (ledstring.channel[0].count - 1));
		ledstring.channel[0].leds[0] = gen_new_pixel(255.0f);

		if ((ret = ws2811_render(&ledstring)) != WS2811_SUCCESS) {
			fprintf(stderr, "ws2811_render failed: %s\n", ws2811_get_return_t_str(ret));
			break;
		}
	}

	ws2811_fini(&ledstring);
}
