#ifndef _SWAY_BACKGROUND_IMAGE_H
#define _SWAY_BACKGROUND_IMAGE_H
#include <wayland-client.h>
#include "cairo.h"

enum background_mode {
	BACKGROUND_MODE_STRETCH,
	BACKGROUND_MODE_FILL,
	BACKGROUND_MODE_FIT,
	BACKGROUND_MODE_CENTER,
	BACKGROUND_MODE_TILE,
	BACKGROUND_MODE_SOLID_COLOR,
	BACKGROUND_MODE_INVALID,
};

struct swaylock_surface;

enum background_mode parse_background_mode(const char *mode);
cairo_surface_t *load_background_image(const char *path);
cairo_surface_t *load_background_from_buffer(void *buf, uint32_t format,
		uint32_t width, uint32_t height, uint32_t stride, enum wl_output_transform transform);
void render_background_image(cairo_t *cairo, cairo_surface_t *image,
		enum background_mode mode, int buffer_width, int buffer_height);

#endif
