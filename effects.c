#define _POSIX_C_SOURCE 200809
#include <omp.h>
#include <stdlib.h>
#include <stdbool.h>
#include <dlfcn.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>
#include <unistd.h>
#include <spawn.h>
#include "effects.h"
#include "log.h"

extern char **environ;

static int screen_size_to_pix(struct swaylock_effect_screen_pos size, int screensize) {
	int actual = size.pos;
	if (size.is_percent)
		actual = (size.pos / 100.0) * screensize;
	return actual;
}

static int screen_pos_to_pix(struct swaylock_effect_screen_pos pos, int screensize) {
	int actual = pos.pos;
	if (pos.is_percent)
		actual = (pos.pos / 100.0) * screensize;
	if (actual < 0)
		actual = screensize + actual;
	return actual;
}

static void screen_pos_pair_to_pix(
		struct swaylock_effect_screen_pos posx,
		struct swaylock_effect_screen_pos posy,
		int objwidth, int objheight,
		int screenwidth, int screenheight, int gravity,
		int *outx, int *outy) {
	int x = screen_pos_to_pix(posx, screenwidth);
	int y = screen_pos_to_pix(posy, screenheight);

	// Adjust X
	switch (gravity) {
	case EFFECT_COMPOSE_GRAV_CENTER:
	case EFFECT_COMPOSE_GRAV_N:
	case EFFECT_COMPOSE_GRAV_S:
		x -= objwidth / 2;
		break;
	case EFFECT_COMPOSE_GRAV_NW:
	case EFFECT_COMPOSE_GRAV_SW:
	case EFFECT_COMPOSE_GRAV_W:
		break;
	case EFFECT_COMPOSE_GRAV_NE:
	case EFFECT_COMPOSE_GRAV_SE:
	case EFFECT_COMPOSE_GRAV_E:
		x -= objwidth;
		break;
	}

	// Adjust Y
	switch (gravity) {
	case EFFECT_COMPOSE_GRAV_CENTER:
	case EFFECT_COMPOSE_GRAV_W:
	case EFFECT_COMPOSE_GRAV_E:
		y -= objheight / 2;
		break;
	case EFFECT_COMPOSE_GRAV_NW:
	case EFFECT_COMPOSE_GRAV_NE:
	case EFFECT_COMPOSE_GRAV_N:
		break;
	case EFFECT_COMPOSE_GRAV_SW:
	case EFFECT_COMPOSE_GRAV_SE:
	case EFFECT_COMPOSE_GRAV_S:
		y -= objheight;
		break;
	}

	*outx = x;
	*outy = y;
}

static uint32_t blend_pixels(float alpha, uint32_t srcpix, uint32_t destpix) {
	uint8_t srcr = (srcpix & 0x00ff0000) >> 16;
	uint8_t destr = (destpix & 0x00ff0000) >> 16;
	uint8_t srcg = (srcpix & 0x0000ff00) >> 8;
	uint8_t destg = (destpix & 0x0000ff00) >> 8;
	uint8_t srcb = (srcpix & 0x000000ff) >> 0;
	uint8_t destb = (destpix & 0x000000ff) >> 0;

	return (uint32_t)0 |
		(uint32_t)255 << 24 |
		(uint32_t)(srcr + destr * (1 - alpha)) << 16 |
		(uint32_t)(srcg + destg * (1 - alpha)) << 8 |
		(uint32_t)(srcb + destb * (1 - alpha)) << 0;
}

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

// This effect_blur function, and the associated blur_* functions,
// are my own adaptations of code in yvbbrjdr's i3lock-fancy-rapid:
// https://github.com/yvbbrjdr/i3lock-fancy-rapid
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
		if (sy >= sheight) continue;
		for (int dx = 0; dx < dwidth; ++dx) {
			int sx = dx * fact;
			if (sx >= swidth) continue;
			dest[dy * dwidth + dx] = src[sy * swidth + sx];
		}
	}
}

static void effect_greyscale(uint32_t *data, int width, int height) {
#pragma omp parallel for
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			int index = y * width + x;
			int r = (data[index] & 0xff0000) >> 16;
			int g = (data[index] & 0x00ff00) >> 8;
			int b = (data[index] & 0x0000ff);
			int luma = 0.2989 * r + 0.5870 * g + 0.1140 * b;
			if (luma < 0) luma = 0;
			if (luma > 255) luma = 255;
			luma &= 0xFF;
			data[index] = luma << 16 | luma << 8 | luma;
		}
	}
}

static void effect_vignette(uint32_t *data, int width, int height,
		double base, double factor) {
	base = fmin(1, fmax(0, base));
	factor = fmin(1 - base, fmax(0, factor));
#pragma omp parallel for
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {

			double xf = (x * 1.0) / width;
			double yf = (y * 1.0) / height;
			double vignette_factor = base + factor
				* 16 * xf * yf * (1.0 - xf) * (1.0 - yf);

			int index = y * width + x;
			int r = (data[index] & 0xff0000) >> 16;
			int g = (data[index] & 0x00ff00) >> 8;
			int b = (data[index] & 0x0000ff);

			r = (int)(r * vignette_factor) & 0xFF;
			g = (int)(g * vignette_factor) & 0xFF;
			b = (int)(b * vignette_factor) & 0xFF;

			data[index] = r << 16 | g << 8 | b;
		}
	}
}

static void effect_compose(uint32_t *data, int width, int height,
		struct swaylock_effect_screen_pos posx,
		struct swaylock_effect_screen_pos posy,
		struct swaylock_effect_screen_pos posw,
		struct swaylock_effect_screen_pos posh,
		int gravity, char *imgpath) {
#if !HAVE_GDK_PIXBUF
	swaylock_log(LOG_ERROR, "Compose effect: Compiled without gdk_pixbuf support.\n");
	return;
#else
	int imgw = screen_size_to_pix(posw, width);
	int imgh = screen_size_to_pix(posh, height);
	bool preserve_aspect = imgw < 0 || imgh < 0;

	GError *err = NULL;
	GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_scale(
			imgpath, imgw, imgh, preserve_aspect, &err);
	if (!pixbuf) {
		swaylock_log(LOG_ERROR, "Compose effect: Failed to load image file '%s' (%s).",
				imgpath, err->message);
		g_error_free(err);
		return;
	}

	cairo_surface_t *image = gdk_cairo_image_surface_create_from_pixbuf(pixbuf);
	g_object_unref(pixbuf);

	int bufw = cairo_image_surface_get_width(image);
	int bufh = cairo_image_surface_get_height(image);
	uint32_t *bufdata = (uint32_t *)cairo_image_surface_get_data(image);
	int bufstride = cairo_image_surface_get_stride(image) / 4;
	bool bufalpha = cairo_image_surface_get_format(image) == CAIRO_FORMAT_ARGB32;

	int imgx, imgy;
	screen_pos_pair_to_pix(
			posx, posy, bufw, bufh,
			width, height, gravity,
			&imgx, &imgy);

#pragma omp parallel for
	for (int offy = 0; offy < bufh; ++offy) {
		if (offy + imgy < 0 || offy + imgy > height)
			continue;

		for (int offx = 0; offx < bufw; ++offx) {
			if (offx + imgx < 0 || offx + imgx > width)
				continue;

			size_t idx = (size_t)(offy + imgy) * width + (offx + imgx);
			size_t bufidx = (size_t)offy * bufstride + (offx);

			if (!bufalpha) {
				data[idx] = bufdata[bufidx];
			} else {
				uint8_t alpha = (bufdata[bufidx] & 0xff000000) >> 24;
				if (alpha == 255) {
					data[idx] = bufdata[bufidx];
				} else if (alpha != 0) {
					data[idx] = blend_pixels(alpha / 255.0, bufdata[bufidx], data[idx]);
				}
			}
		}
	}

	cairo_surface_destroy(image);
#endif
}

static void effect_custom(uint32_t *data, int width, int height,
		char *path) {
	void *dl = dlopen(path, RTLD_LAZY);
	if (dl == NULL) {
		swaylock_log(LOG_ERROR, "Custom effect: %s", dlerror());
		return;
	}

	void (*effect_func)(uint32_t *data, int width, int height) =
		dlsym(dl, "swaylock_effect");
	if (effect_func != NULL) {
		effect_func(data, width, height);
		dlclose(dl);
		return;
	}

	uint32_t (*pixel_func)(uint32_t pix, int x, int y, int width, int height) =
		dlsym(dl, "swaylock_pixel");
	if (pixel_func != NULL) {
#pragma omp parallel for
		for (int y = 0; y < height; ++y) {
			for (int x = 0; x < width; ++x) {
				data[y * width + x] =
					pixel_func(data[y * width + x], x, y, width, height);
			}
		}

		dlclose(dl);
		return;
	}

	swaylock_log(LOG_ERROR, "Custom effect: %s", dlerror());
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

			if (cairo_surface_status(surf) != CAIRO_STATUS_SUCCESS) {
				swaylock_log(LOG_ERROR, "Failed to create surface for blur effect");
				cairo_surface_destroy(surf);
				break;
			}

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

			if (cairo_surface_status(surf) != CAIRO_STATUS_SUCCESS) {
				swaylock_log(LOG_ERROR, "Failed to create surface for scale effect");
				cairo_surface_destroy(surf);
				break;
			}

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
		}

		case EFFECT_GREYSCALE: {
			effect_greyscale(
					(uint32_t *)cairo_image_surface_get_data(surface),
					cairo_image_surface_get_width(surface),
					cairo_image_surface_get_height(surface));
			cairo_surface_flush(surface);
			break;
		}

		case EFFECT_VIGNETTE: {
			effect_vignette(
					(uint32_t *)cairo_image_surface_get_data(surface),
					cairo_image_surface_get_width(surface),
					cairo_image_surface_get_height(surface),
					effect->e.vignette.base,
					effect->e.vignette.factor);
			cairo_surface_flush(surface);
			break;
		}

		case EFFECT_COMPOSE: {
			effect_compose(
					(uint32_t *)cairo_image_surface_get_data(surface),
					cairo_image_surface_get_width(surface),
					cairo_image_surface_get_height(surface),
					effect->e.compose.x, effect->e.compose.y,
					effect->e.compose.w, effect->e.compose.h,
					effect->e.compose.gravity, effect->e.compose.imgpath);
			cairo_surface_flush(surface);
			break;
		}

		case EFFECT_CUSTOM: {
			effect_custom(
					(uint32_t *)cairo_image_surface_get_data(surface),
					cairo_image_surface_get_width(surface),
					cairo_image_surface_get_height(surface),
					effect->e.custom);
			cairo_surface_flush(surface);
			break;
		} }
	}

	return surface;
}
