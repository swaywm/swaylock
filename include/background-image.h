#ifndef _SWAY_BACKGROUND_IMAGE_H
#define _SWAY_BACKGROUND_IMAGE_H
#include <stdbool.h>
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

// Structure to hold GIF animation data
struct swaylock_gif {
	cairo_surface_t **frames;      // Array of cairo surfaces for each frame
	int *frame_delays;             // Delay in milliseconds for each frame
	int frame_count;               // Total number of frames
	int current_frame;             // Current frame index
	bool is_animated;              // True if this is an animated GIF (more than 1 frame)
};

enum background_mode parse_background_mode(const char *mode);

// Load a static background image (PNG, JPEG, etc.)
cairo_surface_t *load_background_image(const char *path);

// Load a GIF animation from file
// Returns NULL on failure, or a swaylock_gif structure on success
// If the GIF has only one frame, is_animated will be false
struct swaylock_gif *load_gif_image(const char *path);

// Free a GIF animation structure and all its frames
void free_gif(struct swaylock_gif *gif);

// Get the current frame's cairo surface
cairo_surface_t *gif_get_current_frame(struct swaylock_gif *gif);

// Advance to the next frame, returns the delay until the next frame in ms
// Wraps around to frame 0 after the last frame
int gif_advance_frame(struct swaylock_gif *gif);

// Get the delay for the current frame in milliseconds
int gif_get_current_delay(struct swaylock_gif *gif);

// Check if a file path looks like a GIF file
bool is_gif_path(const char *path);

void render_background_image(cairo_t *cairo, cairo_surface_t *image,
		enum background_mode mode, int buffer_width, int buffer_height);

#endif
