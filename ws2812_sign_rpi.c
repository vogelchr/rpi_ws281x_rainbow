#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <stdint.h> // ws2811.h needs uint32_t ...
#include <math.h>

#include <sys/queue.h>

#include <ws2811.h>
#include <SDL_image.h>

#define DEFAULT_WIDTH 42
#define DEFAULT_HEIGHT 7

#define GPIO_PIN 18
#define DMANUM 10
//#define STRIP_TYPE            WS2811_STRIP_RGB		// WS2812/SK6812RGB
//integrated chip+leds
#define STRIP_TYPE WS2811_STRIP_GBR // WS2812/SK6812RGB integrated chip+leds
//#define STRIP_TYPE            SK6812_STRIP_RGBW		// SK6812RGBW (NOT
//SK6812RGB)

#define TWOPIf (((float)(M_PI)) * 2.0f)

ws2811_t ledstring = {.freq = WS2811_TARGET_FREQ,
		      .dmanum = DMANUM,
		      .channel = {[0] = {.gpionum = GPIO_PIN,
					 .count = 0,
					 .invert = 0,
					 .brightness = 255,
					 .strip_type = STRIP_TYPE},
				  [1] = {
					  /* fill with zero */
				  }}};

SLIST_HEAD(sign_item_list, sign_item);

struct sign_item {
	SLIST_ENTRY(sign_item) sign_items;
	unsigned long frame_time_us;
	unsigned long delay_time_us;
	SDL_Surface *image;
};

int read_sign_items(char *fn, struct sign_item_list *head)
{
	FILE *f;
	char buf[128], *s;
	struct sign_item *item, *last_item;
	unsigned long ftime, dtime;
	SDL_Surface *image;
	int ret = 0, line = 0;

	if ((f = fopen(fn, "r")) == NULL) {
		perror(fn);
		return -1;
	}

	last_item = NULL;

	for (;;) {
		line++;
		s = fgets(buf, sizeof(buf), f);
		if (s == NULL)
			break;
		buf[sizeof(buf) - 1] = '\0';

		/* remove '\n' at EOL */
		if ((s = strchr(buf, '\n')))
			*s = '\0';
		/* empty lines */
		if (strlen(buf) == 0)
			continue;
		/* comments */
		if (buf[0] == ';' || buf[0] == '#')
			continue;

		ftime = strtoul(buf, &s, 0);
		if (*s == '\0') {
			fprintf(stderr,
				"%s:%d short line or parse error after reading frame time!\n",
				fn, line);
			continue;
		}

		dtime = strtoul(s, &s, 0);
		if (*s == '\0') {
			fprintf(stderr,
				"%s:%d short line or parse error after reading delay time!\n",
				fn, line);
			continue;
		}

		while (*s == ' ')
			s++;

		image = IMG_Load(s);
		if (!image) {
			fprintf(stderr, "%s:%d could not load image %s.\n", fn,
				line, s);
			continue;
		}

		printf("%s:%d ftime=%lu dtime=%lu fn=%s width=%d height=%d\n",
		       fn, line, ftime, dtime, s, image->w, image->h);

		item = malloc(sizeof(*item));
		if (!item) {
			perror("malloc()");
			return -1;
		}

		bzero(item, sizeof(*item));

		item->frame_time_us = ftime;
		item->delay_time_us = dtime;
		item->image = image;

		if (!last_item)
			SLIST_INSERT_HEAD(head, item, sign_items);
		else
			SLIST_INSERT_AFTER(last_item, item, sign_items);
		last_item = item;

		ret++;
	}

	fclose(f);
	return ret;
}

volatile int keep_running = 1;

static void sighandler_int_term(int i)
{
	(void)i;
	keep_running = 0;
}

static void usage(char *argv0)
{
	fprintf(stderr, "Usage: %s [options] png_image_list_file\n", argv0);
	fprintf(stderr, "-h              this help\n");
}

static uint32_t image_getrgb(SDL_Surface *image, unsigned int x, unsigned int y)
{
	int bpp = image->format->BytesPerPixel;
	/* Here p is the address to the pixel we want to retrieve */
	Uint8 *p;

	if (x >= (unsigned int)image->w || y >= (unsigned int)image->h)
		return 0;

	p = (Uint8 *)image->pixels + y * image->pitch + x * bpp;

	switch (bpp) {
	case 1:
		return *p;
		break;

	case 2:
		return *(Uint16 *)p;
		break;

	case 3:
		if (SDL_BYTEORDER == SDL_BIG_ENDIAN)
			return p[0] << 16 | p[1] << 8 | p[2];
		else
			return p[0] | p[1] << 8 | p[2] << 16;
		break;

	case 4:
		return *(Uint32 *)p;
		break;

	default:
		return 0; /* shouldn't happen, but avoids warnings */
	}
}

static void image_to_pixels(uint32_t *pixels, SDL_Surface *image,
			    unsigned int width, unsigned int height,
			    unsigned int xoffs)
{
	unsigned int bpp = image->format->BytesPerPixel;
	unsigned int pix_offs;
	uint8_t r, g, b;
	uint32_t src;

	for (unsigned int y = 0; y < height; y++) {
		for (unsigned int x = 0; x < width; x++) {
			if (y & 1)
				pix_offs = (y + 1) * width - (x + 1);
			else
				pix_offs = y * width + x;

			src = image_getrgb(image, x + xoffs, y);
			SDL_GetRGB(src, image->format, &r, &g, &b);
#if 1
			if (x < 27) {
				r = r / 4;
				g = g / 4;
				b = b / 4;
			}
#endif
			pixels[pix_offs] = r | (g << 8) | (b << 16);
		}
	}
}

static struct sign_item_list sign_item_list;

int main(int argc, char **argv)
{
	int ret;
	int i;
	struct sign_item *item = NULL;
	unsigned int width = DEFAULT_WIDTH;
	unsigned int height = DEFAULT_HEIGHT;
	unsigned int offs_x = 0;

	while ((i = getopt(argc, argv, "hw:h:")) != -1) {
		switch (i) {
		case 'h':
			usage(argv[0]);
			exit(1);
			break;
		case 'W':
			width = atoi(argv[optind]);
			break;
		case 'H':
			height = atoi(argv[optind]);
			break;
		}
	}

	if (optind >= argc) {
		fprintf(stderr,
			"You have to specify the textfile listing the png images to show on the sign!\n");
		usage(argv[0]);
		exit(1);
	}

	SLIST_INIT(&sign_item_list);
	i = read_sign_items(argv[optind], &sign_item_list);
	if (i == -1 || i == 0) {
		fprintf(stderr, "Could not read valid items from file %s.\n",
			argv[optind]);
		exit(1);
	}

	fprintf(stderr, "Read %d items from file %s.\n", i, argv[optind]);

	signal(SIGINT, &sighandler_int_term);
	signal(SIGTERM, &sighandler_int_term);

	ledstring.channel[0].count = width * height;
	fprintf(stderr, "Initializing %u x %u pixels (%u in total).\n", width,
		height, ledstring.channel[0].count);

	if ((ret = ws2811_init(&ledstring)) != WS2811_SUCCESS) {
		fprintf(stderr, "ws2811_init failed: %s\n",
			ws2811_get_return_t_str(ret));
		return ret;
	}

	memset(ledstring.channel[0].leds, '\0',
	       sizeof(uint32_t) * ledstring.channel[0].count);

	item = SLIST_FIRST(&sign_item_list);
	while (keep_running) {
#if 0
		fprintf(stderr, "Tick. Offs x=%u, img width=%u, delay %lu us.\n",
				offs_x, (unsigned int)item->image->w, item->frame_time_us);
#endif

		image_to_pixels(ledstring.channel[0].leds, item->image, width,
				height, offs_x);
		if ((ret = ws2811_render(&ledstring)) != WS2811_SUCCESS) {
			fprintf(stderr, "ws2811_render failed: %s\n",
				ws2811_get_return_t_str(ret));
			break;
		}

		usleep(item->frame_time_us);

		offs_x++;
		if (((unsigned int)item->image->w) <= width
		    || offs_x > (((unsigned int)item->image->w) - width)) {
			//			fprintf(stderr, "Next item, sleeping
			//for %lu us.\n", item->delay_time_us);
			usleep(item->delay_time_us);
			offs_x = 0;
			item = SLIST_NEXT(item, sign_items);
			if (!item) {
				item = SLIST_FIRST(&sign_item_list);
				//				fprintf(stderr,
				//"Wraparound!\n");
			}
		}
	}

out:
	ws2811_fini(&ledstring);
	return 0;
}
