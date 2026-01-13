#include <assert.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include "background-image.h"
#include "cairo.h"
#include "log.h"

enum background_mode parse_background_mode(const char *mode) {
	if (strcmp(mode, "stretch") == 0) {
		return BACKGROUND_MODE_STRETCH;
	} else if (strcmp(mode, "fill") == 0) {
		return BACKGROUND_MODE_FILL;
	} else if (strcmp(mode, "fit") == 0) {
		return BACKGROUND_MODE_FIT;
	} else if (strcmp(mode, "center") == 0) {
		return BACKGROUND_MODE_CENTER;
	} else if (strcmp(mode, "tile") == 0) {
		return BACKGROUND_MODE_TILE;
	} else if (strcmp(mode, "solid_color") == 0) {
		return BACKGROUND_MODE_SOLID_COLOR;
	}
	swaylock_log(LOG_ERROR, "Unsupported background mode: %s", mode);
	return BACKGROUND_MODE_INVALID;
}

bool is_gif_path(const char *path) {
	if (!path) {
		return false;
	}
	size_t len = strlen(path);
	if (len < 4) {
		return false;
	}
	const char *ext = path + len - 4;
	return (strcasecmp(ext, ".gif") == 0);
}

cairo_surface_t *load_background_image(const char *path) {
	cairo_surface_t *image;
#if HAVE_GDK_PIXBUF
	GError *err = NULL;
	GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file(path, &err);
	if (!pixbuf) {
		swaylock_log(LOG_ERROR, "Failed to load background image (%s).",
				err->message);
		g_error_free(err);
		return NULL;
	}
	// Correct for embedded image orientation; typical images are not
	// rotated and will be handled efficiently
	GdkPixbuf *oriented = gdk_pixbuf_apply_embedded_orientation(pixbuf);
	g_object_unref(pixbuf);
	image = gdk_cairo_image_surface_create_from_pixbuf(oriented);
	g_object_unref(oriented);
#else
	image = cairo_image_surface_create_from_png(path);
#endif // HAVE_GDK_PIXBUF
	if (!image) {
		swaylock_log(LOG_ERROR, "Failed to read background image.");
		return NULL;
	}
	if (cairo_surface_status(image) != CAIRO_STATUS_SUCCESS) {
		swaylock_log(LOG_ERROR, "Failed to read background image: %s."
#if !HAVE_GDK_PIXBUF
				"\nSway was compiled without gdk_pixbuf support, so only"
				"\nPNG images can be loaded. This is the likely cause."
#endif // !HAVE_GDK_PIXBUF
				, cairo_status_to_string(cairo_surface_status(image)));
		return NULL;
	}
	return image;
}

#if HAVE_GDK_PIXBUF

// Suppress deprecation warnings for GdkPixbufAnimation API
// These functions are deprecated but there's no replacement for animated image loading
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

struct swaylock_gif *load_gif_image(const char *path) {
	GError *err = NULL;
	GdkPixbufAnimation *animation = gdk_pixbuf_animation_new_from_file(path, &err);
	if (!animation) {
		swaylock_log(LOG_ERROR, "Failed to load GIF animation (%s).",
				err->message);
		g_error_free(err);
		return NULL;
	}

	// Check if this is actually animated
	if (gdk_pixbuf_animation_is_static_image(animation)) {
		swaylock_log(LOG_DEBUG, "GIF is static (single frame), loading as regular image");
		struct swaylock_gif *gif = calloc(1, sizeof(struct swaylock_gif));
		if (!gif) {
			g_object_unref(animation);
			return NULL;
		}

		gif->frame_count = 1;
		gif->current_frame = 0;
		gif->is_animated = false;
		gif->frames = calloc(1, sizeof(cairo_surface_t *));
		gif->frame_delays = calloc(1, sizeof(int));

		if (!gif->frames || !gif->frame_delays) {
			free(gif->frames);
			free(gif->frame_delays);
			free(gif);
			g_object_unref(animation);
			return NULL;
		}

		GdkPixbuf *pixbuf = gdk_pixbuf_animation_get_static_image(animation);
		gif->frames[0] = gdk_cairo_image_surface_create_from_pixbuf(pixbuf);
		gif->frame_delays[0] = 100; // Default delay

		g_object_unref(animation);
		return gif;
	}

	// Count frames and collect frame data
	// We need to iterate through all frames to get the count
	GTimeVal start_time = {0, 0};
	GdkPixbufAnimationIter *iter = gdk_pixbuf_animation_get_iter(animation, &start_time);

	if (!iter) {
		swaylock_log(LOG_ERROR, "Failed to create GIF animation iterator");
		g_object_unref(animation);
		return NULL;
	}

	// First pass: count frames
	int frame_count = 0;
	int max_frames = 1000; // Safety limit
	GTimeVal current_time = start_time;

	// We need to iterate and track unique frames
	// GdkPixbufAnimationIter doesn't give us frame count directly
	// So we'll collect frames as we go

	int capacity = 32;
	cairo_surface_t **frames = calloc(capacity, sizeof(cairo_surface_t *));
	int *delays = calloc(capacity, sizeof(int));

	if (!frames || !delays) {
		free(frames);
		free(delays);
		g_object_unref(iter);
		g_object_unref(animation);
		return NULL;
	}

	// Reset iterator
	g_object_unref(iter);
	current_time = start_time;
	iter = gdk_pixbuf_animation_get_iter(animation, &current_time);

	// Iterate through all frames
	do {
		if (frame_count >= max_frames) {
			swaylock_log(LOG_ERROR, "GIF has too many frames (>%d)", max_frames);
			break;
		}

		// Grow arrays if needed
		if (frame_count >= capacity) {
			int new_capacity = capacity * 2;
			cairo_surface_t **new_frames = realloc(frames, new_capacity * sizeof(cairo_surface_t *));
			int *new_delays = realloc(delays, new_capacity * sizeof(int));
			if (!new_frames || !new_delays) {
				swaylock_log(LOG_ERROR, "Failed to allocate memory for GIF frames");
				// Clean up already loaded frames
				// Use new_frames if it succeeded, otherwise use frames
				cairo_surface_t **frames_to_free = new_frames ? new_frames : frames;
				int *delays_to_free = new_delays ? new_delays : delays;
				for (int i = 0; i < frame_count; i++) {
					if (frames_to_free[i]) {
						cairo_surface_destroy(frames_to_free[i]);
					}
				}
				free(frames_to_free);
				free(delays_to_free);
				g_object_unref(iter);
				g_object_unref(animation);
				return NULL;
			}
			frames = new_frames;
			delays = new_delays;
			capacity = new_capacity;
		}

		// Get current frame
		GdkPixbuf *pixbuf = gdk_pixbuf_animation_iter_get_pixbuf(iter);
		if (pixbuf) {
			// Copy the pixbuf since the iterator owns it
			GdkPixbuf *copy = gdk_pixbuf_copy(pixbuf);
			if (copy) {
				frames[frame_count] = gdk_cairo_image_surface_create_from_pixbuf(copy);
				g_object_unref(copy);
			} else {
				frames[frame_count] = NULL;
			}
		} else {
			frames[frame_count] = NULL;
		}

		// Get delay for this frame
		int delay_ms = gdk_pixbuf_animation_iter_get_delay_time(iter);
		if (delay_ms < 0) {
			delay_ms = 100; // Default to 100ms if no delay specified
		} else if (delay_ms < 20) {
			delay_ms = 20; // Minimum delay to prevent excessive CPU usage
		}
		delays[frame_count] = delay_ms;

		frame_count++;

		// Advance to next frame
		// We use the delay time to advance
		g_time_val_add(&current_time, delay_ms * 1000); // microseconds

	} while (gdk_pixbuf_animation_iter_advance(iter, &current_time) &&
	         frame_count < max_frames &&
	         !gdk_pixbuf_animation_iter_on_currently_loading_frame(iter));

	g_object_unref(iter);
	g_object_unref(animation);

	if (frame_count == 0) {
		swaylock_log(LOG_ERROR, "GIF has no frames");
		free(frames);
		free(delays);
		return NULL;
	}

	// Create the gif structure
	struct swaylock_gif *gif = calloc(1, sizeof(struct swaylock_gif));
	if (!gif) {
		for (int i = 0; i < frame_count; i++) {
			if (frames[i]) {
				cairo_surface_destroy(frames[i]);
			}
		}
		free(frames);
		free(delays);
		return NULL;
	}

	gif->frames = frames;
	gif->frame_delays = delays;
	gif->frame_count = frame_count;
	gif->current_frame = 0;
	gif->is_animated = (frame_count > 1);

	swaylock_log(LOG_DEBUG, "Loaded GIF with %d frames, animated: %s",
			frame_count, gif->is_animated ? "yes" : "no");

	return gif;
}

#pragma GCC diagnostic pop

#else
struct swaylock_gif *load_gif_image(const char *path) {
	swaylock_log(LOG_ERROR, "GIF support requires gdk-pixbuf. "
			"Swaylock was compiled without gdk-pixbuf support.");
	return NULL;
}
#endif // HAVE_GDK_PIXBUF

void free_gif(struct swaylock_gif *gif) {
	if (!gif) {
		return;
	}

	if (gif->frames) {
		for (int i = 0; i < gif->frame_count; i++) {
			if (gif->frames[i]) {
				cairo_surface_destroy(gif->frames[i]);
			}
		}
		free(gif->frames);
	}

	free(gif->frame_delays);
	free(gif);
}

cairo_surface_t *gif_get_current_frame(struct swaylock_gif *gif) {
	if (!gif || !gif->frames || gif->frame_count == 0) {
		return NULL;
	}

	if (gif->current_frame < 0 || gif->current_frame >= gif->frame_count) {
		gif->current_frame = 0;
	}

	return gif->frames[gif->current_frame];
}

int gif_advance_frame(struct swaylock_gif *gif) {
	if (!gif || gif->frame_count <= 1) {
		return 0;
	}

	gif->current_frame = (gif->current_frame + 1) % gif->frame_count;
	return gif_get_current_delay(gif);
}

int gif_get_current_delay(struct swaylock_gif *gif) {
	if (!gif || !gif->frame_delays || gif->frame_count == 0) {
		return 100; // Default delay
	}

	if (gif->current_frame < 0 || gif->current_frame >= gif->frame_count) {
		return gif->frame_delays[0];
	}

	return gif->frame_delays[gif->current_frame];
}

void render_background_image(cairo_t *cairo, cairo_surface_t *image,
		enum background_mode mode, int buffer_width, int buffer_height) {
	double width = cairo_image_surface_get_width(image);
	double height = cairo_image_surface_get_height(image);

	cairo_save(cairo);
	switch (mode) {
	case BACKGROUND_MODE_STRETCH:
		cairo_scale(cairo,
				(double)buffer_width / width,
				(double)buffer_height / height);
		cairo_set_source_surface(cairo, image, 0, 0);
		break;
	case BACKGROUND_MODE_FILL: {
		double window_ratio = (double)buffer_width / buffer_height;
		double bg_ratio = width / height;

		if (window_ratio > bg_ratio) {
			double scale = (double)buffer_width / width;
			cairo_scale(cairo, scale, scale);
			cairo_set_source_surface(cairo, image,
					0, (double)buffer_height / 2 / scale - height / 2);
		} else {
			double scale = (double)buffer_height / height;
			cairo_scale(cairo, scale, scale);
			cairo_set_source_surface(cairo, image,
					(double)buffer_width / 2 / scale - width / 2, 0);
		}
		break;
	}
	case BACKGROUND_MODE_FIT: {
		double window_ratio = (double)buffer_width / buffer_height;
		double bg_ratio = width / height;

		if (window_ratio > bg_ratio) {
			double scale = (double)buffer_height / height;
			cairo_scale(cairo, scale, scale);
			cairo_set_source_surface(cairo, image,
					(double)buffer_width / 2 / scale - width / 2, 0);
		} else {
			double scale = (double)buffer_width / width;
			cairo_scale(cairo, scale, scale);
			cairo_set_source_surface(cairo, image,
					0, (double)buffer_height / 2 / scale - height / 2);
		}
		break;
	}
	case BACKGROUND_MODE_CENTER:
		/*
		 * Align the unscaled image to integer pixel boundaries
		 * in order to prevent loss of clarity (this only matters
		 * for odd-sized images).
		 */
		cairo_set_source_surface(cairo, image,
				(int)((double)buffer_width / 2 - width / 2),
				(int)((double)buffer_height / 2 - height / 2));
		break;
	case BACKGROUND_MODE_TILE: {
		cairo_pattern_t *pattern = cairo_pattern_create_for_surface(image);
		cairo_pattern_set_extend(pattern, CAIRO_EXTEND_REPEAT);
		cairo_set_source(cairo, pattern);
		break;
	}
	case BACKGROUND_MODE_SOLID_COLOR:
	case BACKGROUND_MODE_INVALID:
		assert(0);
		break;
	}
	cairo_paint(cairo);
	cairo_restore(cairo);
}
