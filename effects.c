#include "effects.h"

#include <omp.h>

static void blur_h(uint32_t *dest, uint32_t *src, int width, int height,
		int radius) {
	double coeff = 1.0 / (radius * 2 + 1);
#pragma omp parallel for
	for (int i = 0; i < height; ++i) {
		int iwidth = i * width;
		double r_acc = 0.0;
		double g_acc = 0.0;
		double b_acc = 0.0;
		for (int j = -radius; j < width; ++j) {
			if (j - radius - 1 >= 0) {
				int index = iwidth + j - radius - 1;
				r_acc -= coeff * ((src[index] & 0xff0000) >> 16);
				g_acc -= coeff * ((src[index] & 0x00ff00) >> 8);
				b_acc -= coeff * ((src[index] & 0x0000ff));
			}
			if (j + radius < width) {
				int index = iwidth + j + radius;
				r_acc += coeff * ((src[index] & 0xff0000) >> 16);
				g_acc += coeff * ((src[index] & 0x00ff00) >> 8);
				b_acc += coeff * ((src[index] & 0x0000ff));
			}
			if (j < 0)
				continue;
			int index = iwidth + j;
			dest[index] = 0 |
				(((uint32_t)(r_acc + 0.5) & 0xff) << 16) |
				(((uint32_t)(g_acc + 0.5) & 0xff) << 8) |
				(((uint32_t)(b_acc + 0.5) & 0xff));
		}
	}
}

static void blur_v(uint32_t *dest, uint32_t *src, int width, int height,
		int radius) {
	double coeff = 1.0 / (radius * 2 + 1);
#pragma omp parallel for
	for (int j = 0; j < width; ++j) {
		double r_acc = 0.0;
		double g_acc = 0.0;
		double b_acc = 0.0;
		for (int i = -radius; i < height; ++i) {
			if (i - radius - 1 >= 0) {
				int index = (i - radius - 1) * width + j;
				r_acc -= coeff * ((src[index] & 0xff0000) >> 16);
				g_acc -= coeff * ((src[index] & 0x00ff00) >> 8);
				b_acc -= coeff * ((src[index] & 0x0000ff));
			}
			if (i + radius < height) {
				int index = (i + radius) * width + j;
				r_acc += coeff * ((src[index] & 0xff0000) >> 16);
				g_acc += coeff * ((src[index] & 0x00ff00) >> 8);
				b_acc += coeff * ((src[index] & 0x0000ff));
			}
			if (i < 0)
				continue;
			int index = i * width + j;
			dest[index] = 0 |
				(((uint32_t)(r_acc + 0.5) & 0xff) << 16) |
				(((uint32_t)(g_acc + 0.5) & 0xff) << 8) |
				(((uint32_t)(b_acc + 0.5) & 0xff));
		}
	}
}

static void blur_once(uint32_t *dest, uint32_t *src, uint32_t *scratch,
		int width, int height, int radius) {
	blur_h(scratch, src, width, height, radius);
	blur_v(dest, scratch, width, height, radius);
}

static void effect_blur(uint32_t *dest, uint32_t *src, int width, int height,
		int radius, int times) {
	uint32_t *origdest = dest;

	uint32_t *scratch = malloc(width * height * sizeof(*scratch));
	blur_once(dest, src, scratch, width, height, radius);
	for (int i = 0; i < times - 1; ++i) {
		uint32_t *tmp = src;
		src = dest;
		dest = tmp;
		blur_once(dest, src, scratch, width, height, radius);
	}
	free(scratch);

	// We're flipping between using dest and src;
	// if the last buffer we used was src, copy that over to dest.
	if (dest != origdest)
		memcpy(origdest, dest, width * height * sizeof(*dest));
}

static void effect_scale(uint32_t *dest, uint32_t *src, int swidth, int sheight,
		double scale) {
	int dwidth = swidth * scale;
	int dheight = sheight * scale;
	double fact = 1.0 / scale;
#pragma omp parallel for
	for (int dy = 0; dy < dheight; ++dy) {
		int sy = dy * fact;
		for (int dx = 0; dx < dwidth; ++dx) {
			int sx = dx * fact;
			dest[dy * dwidth + dx] = src[sy * swidth + sx];
		}
	}
}

cairo_surface_t *swaylock_effects_run(cairo_surface_t *surface,
		struct swaylock_effect *effects, int count) {

	for (int i = 0; i < count; ++i) {
		struct swaylock_effect *effect = &effects[i];
		switch (effect->tag) {
		case EFFECT_BLUR: {
			cairo_surface_t *surf = cairo_image_surface_create(
					CAIRO_FORMAT_RGB24,
					cairo_image_surface_get_width(surface),
					cairo_image_surface_get_height(surface));
			effect_blur(
					(uint32_t *)cairo_image_surface_get_data(surf),
					(uint32_t *)cairo_image_surface_get_data(surface),
					cairo_image_surface_get_width(surface),
					cairo_image_surface_get_height(surface),
					effect->e.blur.radius, effect->e.blur.times);
			cairo_surface_flush(surf);
			cairo_surface_destroy(surface);
			surface = surf;
			break;
		}

		case EFFECT_SCALE: {
			cairo_surface_t *surf = cairo_image_surface_create(
					CAIRO_FORMAT_RGB24,
					cairo_image_surface_get_width(surface) * effect->e.scale,
					cairo_image_surface_get_height(surface) * effect->e.scale);
			effect_scale(
					(uint32_t *)cairo_image_surface_get_data(surf),
					(uint32_t *)cairo_image_surface_get_data(surface),
					cairo_image_surface_get_width(surface),
					cairo_image_surface_get_height(surface),
					effect->e.scale);
			cairo_surface_flush(surf);
			cairo_surface_destroy(surface);
			surface = surf;
			break;
		} }
	}

	return surface;
}
