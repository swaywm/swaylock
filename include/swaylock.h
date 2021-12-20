#ifndef _SWAYLOCK_H
#define _SWAYLOCK_H
#include <stdbool.h>
#include <stdint.h>
#include <wayland-client.h>
#include "background-image.h"
#include "cairo.h"
#include "pool-buffer.h"
#include "seat.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

enum auth_state {
	AUTH_STATE_IDLE,
	AUTH_STATE_CLEAR,
	AUTH_STATE_INPUT,
	AUTH_STATE_INPUT_NOP,
	AUTH_STATE_BACKSPACE,
	AUTH_STATE_VALIDATING,
	AUTH_STATE_INVALID,
};

struct swaylock_colorset {
	uint32_t input;
	uint32_t cleared;
	uint32_t caps_lock;
	uint32_t verifying;
	uint32_t wrong;
};

struct swaylock_colors {
	uint32_t background;
	uint32_t bs_highlight;
	uint32_t key_highlight;
	uint32_t caps_lock_bs_highlight;
	uint32_t caps_lock_key_highlight;
	uint32_t separator;
	uint32_t layout_background;
	uint32_t layout_border;
	uint32_t layout_text;
	struct swaylock_colorset inside;
	struct swaylock_colorset line;
	struct swaylock_colorset ring;
	struct swaylock_colorset text;
};

struct swaylock_args {
	struct swaylock_colors colors;
	enum background_mode mode;
	char *font;
	uint32_t font_size;
	uint32_t radius;
	uint32_t thickness;
	uint32_t indicator_x_position;
	uint32_t indicator_y_position;
	bool override_indicator_x_position;
	bool override_indicator_y_position;
	bool ignore_empty;
	bool show_indicator;
	bool show_caps_lock_text;
	bool show_caps_lock_indicator;
	bool show_keyboard_layout;
	bool hide_keyboard_layout;
	bool show_failed_attempts;
	bool daemonize;
	bool indicator_idle_visible;
};

struct swaylock_password {
	size_t len;
	char buffer[1024];
};

struct swaylock_state {
	struct loop *eventloop;
	struct loop_timer *clear_indicator_timer; // clears the indicator
	struct loop_timer *clear_password_timer;  // clears the password buffer
	struct wl_display *display;
	struct wl_compositor *compositor;
	struct wl_subcompositor *subcompositor;
	struct zwlr_layer_shell_v1 *layer_shell;
	struct zwlr_input_inhibit_manager_v1 *input_inhibit_manager;
	struct wl_shm *shm;
	struct wl_list surfaces;
	struct wl_list images;
	struct swaylock_args args;
	struct swaylock_password password;
	struct swaylock_xkb xkb;
	enum auth_state auth_state;
	int failed_attempts;
	bool run_display;
	struct zxdg_output_manager_v1 *zxdg_output_manager;
	struct ext_session_lock_manager_v1 *ext_session_lock_manager_v1;
	struct ext_session_lock_v1 *ext_session_lock_v1;
};

struct swaylock_surface {
	cairo_surface_t *image;
	struct swaylock_state *state;
	struct wl_output *output;
	uint32_t output_global_name;
	struct zxdg_output_v1 *xdg_output;
	struct wl_surface *surface;
	struct wl_surface *child; // surface made into subsurface
	struct wl_subsurface *subsurface;
	struct zwlr_layer_surface_v1 *layer_surface;
	struct ext_session_lock_surface_v1 *ext_session_lock_surface_v1;
	struct pool_buffer buffers[2];
	struct pool_buffer indicator_buffers[2];
	struct pool_buffer *current_buffer;
	bool frame_pending, dirty;
	uint32_t width, height;
	uint32_t indicator_width, indicator_height;
	int32_t scale;
	enum wl_output_subpixel subpixel;
	char *output_name;
	struct wl_list link;
};

// There is exactly one swaylock_image for each -i argument
struct swaylock_image {
	char *path;
	char *output_name;
	cairo_surface_t *cairo_surface;
	struct wl_list link;
};

void swaylock_handle_key(struct swaylock_state *state,
		xkb_keysym_t keysym, uint32_t codepoint);
void render_frame_background(struct swaylock_surface *surface);
void render_frame(struct swaylock_surface *surface);
void render_frames(struct swaylock_state *state);
void damage_surface(struct swaylock_surface *surface);
void damage_state(struct swaylock_state *state);
void clear_password_buffer(struct swaylock_password *pw);
void schedule_indicator_clear(struct swaylock_state *state);

void initialize_pw_backend(int argc, char **argv);
void run_pw_backend_child(void);
void clear_buffer(char *buf, size_t size);

#endif
