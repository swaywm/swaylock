#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <wayland-client.h>
#include "cairo.h"
#include "background-image.h"
#include "swaylock.h"
#include "log.h"

#define M_PI 3.14159265358979323846
const float TYPE_INDICATOR_RANGE = M_PI / 3.0f;
#define MAX_DOTS 512

static void set_color_for_state(cairo_t *cairo, struct swaylock_state *state,
		struct swaylock_colorset *colorset) {
	if (state->input_state == INPUT_STATE_CLEAR) {
		cairo_set_source_u32(cairo, colorset->cleared);
	} else if (state->auth_state == AUTH_STATE_VALIDATING) {
		cairo_set_source_u32(cairo, colorset->verifying);
	} else if (state->auth_state == AUTH_STATE_INVALID) {
		cairo_set_source_u32(cairo, colorset->wrong);
	} else {
		if (state->xkb.caps_lock && state->args.show_caps_lock_indicator) {
			cairo_set_source_u32(cairo, colorset->caps_lock);
		} else if (state->xkb.caps_lock && !state->args.show_caps_lock_indicator &&
				state->args.show_caps_lock_text) {
			uint32_t inputtextcolor = state->args.colors.text.input;
			state->args.colors.text.input = state->args.colors.text.caps_lock;
			cairo_set_source_u32(cairo, colorset->input);
			state->args.colors.text.input = inputtextcolor;
		} else {
			cairo_set_source_u32(cairo, colorset->input);
		}
	}
}

static void surface_frame_handle_done(void *data, struct wl_callback *callback,
		uint32_t time) {
	struct swaylock_surface *surface = data;

	wl_callback_destroy(callback);
	surface->frame = NULL;

	render(surface);
}

static const struct wl_callback_listener surface_frame_listener = {
	.done = surface_frame_handle_done,
};

static bool render_frame(struct swaylock_surface *surface);

void render(struct swaylock_surface *surface) {
	struct swaylock_state *state = surface->state;

	int buffer_width = surface->width * surface->scale;
	int buffer_height = surface->height * surface->scale;
	if (buffer_width == 0 || buffer_height == 0) {
		return; // not yet configured
	}

	if (!surface->dirty || surface->frame) {
		// Nothing to do or frame already pending
		return;
	}

	bool need_destroy = false;
	struct pool_buffer buffer;

	if (buffer_width != surface->last_buffer_width ||
			buffer_height != surface->last_buffer_height) {
		need_destroy = true;
		if (!create_buffer(state->shm, &buffer, buffer_width, buffer_height,
				WL_SHM_FORMAT_ARGB8888)) {
			swaylock_log(LOG_ERROR,
				"Failed to create new buffer for frame background.");
			return;
		}

		cairo_t *cairo = buffer.cairo;
		cairo_set_antialias(cairo, CAIRO_ANTIALIAS_BEST);

		cairo_save(cairo);
		cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);
		cairo_set_source_u32(cairo, state->args.colors.background);
		cairo_paint(cairo);
		if (surface->image && state->args.mode != BACKGROUND_MODE_SOLID_COLOR) {
			cairo_set_operator(cairo, CAIRO_OPERATOR_OVER);
			render_background_image(cairo, surface->image,
				state->args.mode, buffer_width, buffer_height);
		}
		cairo_restore(cairo);
		cairo_identity_matrix(cairo);

		wl_surface_attach(surface->surface, buffer.buffer, 0, 0);
		wl_surface_damage_buffer(surface->surface, 0, 0, INT32_MAX, INT32_MAX);
		need_destroy = true;

		surface->last_buffer_width = buffer_width;
		surface->last_buffer_height = buffer_height;
	}

	// It is possible for the surface scale to change even if the wl_buffer size hasn't
	wl_surface_set_buffer_scale(surface->surface, surface->scale);

	render_frame(surface);
	surface->dirty = false;
	surface->frame = wl_surface_frame(surface->surface);
	wl_callback_add_listener(surface->frame, &surface_frame_listener, surface);
	wl_surface_commit(surface->surface);

	if (need_destroy) {
		destroy_buffer(&buffer);
	}
}

static void configure_font_drawing(cairo_t *cairo, struct swaylock_state *state,
		enum wl_output_subpixel subpixel, int arc_radius) {
	cairo_font_options_t *fo = cairo_font_options_create();
	cairo_font_options_set_hint_style(fo, CAIRO_HINT_STYLE_FULL);
	cairo_font_options_set_antialias(fo, CAIRO_ANTIALIAS_SUBPIXEL);
	cairo_font_options_set_subpixel_order(fo, to_cairo_subpixel_order(subpixel));

	cairo_set_font_options(cairo, fo);
	cairo_select_font_face(cairo, state->args.font,
		CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
	if (state->args.font_size > 0) {
		cairo_set_font_size(cairo, state->args.font_size);
	} else {
		cairo_set_font_size(cairo, arc_radius / 3.0f);
	}
	cairo_font_options_destroy(fo);
}

// Draws state->dot_count dots at pseudo-random places in the buffer, keeping
// clear of the indicator. The count is only drawn while something is typed. The places derive from state->dot_seed, so
// they are stable between redraws and change on every key press.
static void draw_dots(cairo_t *cairo, struct swaylock_state *state,
		struct swaylock_surface *surface, int width, int height,
		int cx, int cy, int arc_radius, int arc_thickness) {
	size_t count = state->password.len > 0 ? state->dot_count : 0;
	if (count > MAX_DOTS) {
		count = MAX_DOTS;
	}
	double dot_radius = arc_thickness / 2.0;
	if (dot_radius < 2.0 * surface->scale) {
		dot_radius = 2.0 * surface->scale;
	}
	double margin = 4.0 * dot_radius;
	double keep_clear = arc_radius + arc_thickness + 4.0 * dot_radius;
	if (width <= 2 * margin || height <= 2 * margin) {
		return;
	}

	set_color_for_state(cairo, state, &state->args.colors.text);
	uint32_t rng = state->dot_seed;
	for (size_t i = 0; i < count; ++i) {
		for (int attempt = 0; attempt < 16; ++attempt) {
			rng = rng * 1664525u + 1013904223u;
			double x = margin + (width - 2 * margin) * ((rng >> 8) / 16777216.0);
			rng = rng * 1664525u + 1013904223u;
			double y = margin + (height - 2 * margin) * ((rng >> 8) / 16777216.0);
			if (hypot(x - cx, y - cy) < keep_clear) {
				continue;
			}
			cairo_new_sub_path(cairo);
			cairo_arc(cairo, x, y, dot_radius, 0, 2 * M_PI);
			cairo_fill(cairo);
			break;
		}
	}
}

static bool render_frame(struct swaylock_surface *surface) {
	struct swaylock_state *state = surface->state;

	// First, compute the text that will be drawn, if any, since this
	// determines the size/positioning of the surface

	char attempts[4]; // like i3lock: count no more than 999
	char *text = NULL;
	const char *layout_text = NULL;

	bool draw_indicator = state->args.show_indicator &&
		(state->auth_state != AUTH_STATE_IDLE ||
			state->input_state != INPUT_STATE_IDLE ||
			state->args.indicator_idle_visible);

	if (draw_indicator) {
		if (state->input_state == INPUT_STATE_CLEAR) {
			// This message has highest priority
			text = "Cleared";
		} else if (state->auth_state == AUTH_STATE_VALIDATING) {
			text = "Verifying";
		} else if (state->auth_state == AUTH_STATE_INVALID) {
			text = "Wrong";
		} else {
			// Caps Lock has higher priority
			if (state->xkb.caps_lock && state->args.show_caps_lock_text) {
				text = "Caps Lock";
			} else if (state->args.show_failed_attempts &&
					state->failed_attempts > 0) {
				if (state->failed_attempts > 999) {
					text = "999+";
				} else {
					snprintf(attempts, sizeof(attempts), "%d", state->failed_attempts);
					text = attempts;
				}
			}

			if (state->xkb.keymap) {
				xkb_layout_index_t num_layout = xkb_keymap_num_layouts(state->xkb.keymap);
				if (!state->args.hide_keyboard_layout &&
						(state->args.show_keyboard_layout || num_layout > 1)) {
					xkb_layout_index_t curr_layout = 0;

					// advance to the first active layout (if any)
					while (curr_layout < num_layout &&
						xkb_state_layout_index_is_active(state->xkb.state,
							curr_layout, XKB_STATE_LAYOUT_EFFECTIVE) != 1) {
						++curr_layout;
					}
					// will handle invalid index if none are active
					layout_text = xkb_keymap_layout_get_name(state->xkb.keymap, curr_layout);
				}
			}
		}
	}

	// Compute the size of the buffer needed
	int arc_radius = state->args.radius * surface->scale;
	int arc_thickness = state->args.thickness * surface->scale;
	int buffer_diameter = (arc_radius + arc_thickness) * 2;
	int buffer_width = buffer_diameter;
	int buffer_height = buffer_diameter;

	if (text || layout_text) {
		cairo_set_antialias(state->test_cairo, CAIRO_ANTIALIAS_BEST);
		configure_font_drawing(state->test_cairo, state, surface->subpixel, arc_radius);

		if (text) {
			cairo_text_extents_t extents;
			cairo_text_extents(state->test_cairo, text, &extents);
			if (buffer_width < extents.width) {
				buffer_width = extents.width;
			}
		}
		if (layout_text) {
			cairo_text_extents_t extents;
			cairo_font_extents_t fe;
			double box_padding = 4.0 * surface->scale;
			cairo_text_extents(state->test_cairo, layout_text, &extents);
			cairo_font_extents(state->test_cairo, &fe);
			buffer_height += fe.height + 2 * box_padding;
			if (buffer_width < extents.width + 2 * box_padding) {
				buffer_width = extents.width + 2 * box_padding;
			}
		}
	}
	// Ensure buffer size is multiple of buffer scale - required by protocol
	buffer_height += surface->scale - (buffer_height % surface->scale);
	buffer_width += surface->scale - (buffer_width % surface->scale);

	// Where the circle is centered inside the buffer
	int cx = buffer_width / 2;
	int cy = buffer_diameter / 2;

	int subsurf_xpos;
	int subsurf_ypos;

	// Center the indicator unless overridden by the user
	if (state->args.override_indicator_x_position) {
		subsurf_xpos = state->args.indicator_x_position -
			buffer_width / (2 * surface->scale) + 2 / surface->scale;
	} else {
		subsurf_xpos = surface->width / 2 -
			buffer_width / (2 * surface->scale) + 2 / surface->scale;
	}

	if (state->args.override_indicator_y_position) {
		subsurf_ypos = state->args.indicator_y_position -
			(state->args.radius + state->args.thickness);
	} else {
		subsurf_ypos = surface->height / 2 -
			(state->args.radius + state->args.thickness);
	}

	// The dots are scattered around the whole output, so the indicator
	// surface has to cover it. Then the circle is centered inside of it.
	bool draw_dots_now = draw_indicator && state->args.indicator_dots;
	if (draw_dots_now) {
		buffer_width = surface->width * surface->scale;
		buffer_height = surface->height * surface->scale;
		cx = (state->args.override_indicator_x_position ?
			state->args.indicator_x_position : surface->width / 2) *
			surface->scale;
		cy = (state->args.override_indicator_y_position ?
			state->args.indicator_y_position : surface->height / 2) *
			surface->scale;
		subsurf_xpos = 0;
		subsurf_ypos = 0;
	}

	struct pool_buffer *buffer = get_next_buffer(state->shm,
			surface->indicator_buffers, buffer_width, buffer_height);
	if (buffer == NULL) {
		swaylock_log(LOG_ERROR, "No buffer");
		return false;
	}

	// Render the buffer
	cairo_t *cairo = buffer->cairo;
	cairo_set_antialias(cairo, CAIRO_ANTIALIAS_BEST);

	cairo_identity_matrix(cairo);

	// Clear
	cairo_save(cairo);
	cairo_set_source_rgba(cairo, 0, 0, 0, 0);
	cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);
	cairo_paint(cairo);
	cairo_restore(cairo);

	if (draw_indicator) {
		if (draw_dots_now) {
			draw_dots(cairo, state, surface, buffer_width, buffer_height,
				cx, cy, arc_radius, arc_thickness);
		}

		// Fill inner circle
		cairo_set_line_width(cairo, 0);
		cairo_arc(cairo, cx, cy,
				arc_radius - arc_thickness / 2, 0, 2 * M_PI);
		set_color_for_state(cairo, state, &state->args.colors.inside);
		cairo_fill_preserve(cairo);
		cairo_stroke(cairo);

		// Draw ring
		cairo_set_line_width(cairo, arc_thickness);
		cairo_arc(cairo, cx, cy, arc_radius,
				0, 2 * M_PI);
		set_color_for_state(cairo, state, &state->args.colors.ring);
		cairo_stroke(cairo);

		// Draw a message
		configure_font_drawing(cairo, state, surface->subpixel, arc_radius);
		set_color_for_state(cairo, state, &state->args.colors.text);

		if (text) {
			cairo_text_extents_t extents;
			cairo_font_extents_t fe;
			double x, y;
			cairo_text_extents(cairo, text, &extents);
			cairo_font_extents(cairo, &fe);
			x = (cx) -
				(extents.width / 2 + extents.x_bearing);
			y = (cy) +
				(fe.height / 2 - fe.descent);

			cairo_move_to(cairo, x, y);
			cairo_show_text(cairo, text);
			cairo_close_path(cairo);
			cairo_new_sub_path(cairo);
		}

		// Typing indicator: Highlight random part on keypress
		if (state->input_state == INPUT_STATE_LETTER ||
				state->input_state == INPUT_STATE_BACKSPACE) {
			double highlight_start = state->highlight_start * (M_PI / 1024.0);
			cairo_arc(cairo, cx, cy,
					arc_radius, highlight_start,
					highlight_start + TYPE_INDICATOR_RANGE);
			if (state->input_state == INPUT_STATE_LETTER) {
				if (state->xkb.caps_lock && state->args.show_caps_lock_indicator) {
					cairo_set_source_u32(cairo, state->args.colors.caps_lock_key_highlight);
				} else {
					cairo_set_source_u32(cairo, state->args.colors.key_highlight);
				}
			} else {
				if (state->xkb.caps_lock && state->args.show_caps_lock_indicator) {
					cairo_set_source_u32(cairo, state->args.colors.caps_lock_bs_highlight);
				} else {
					cairo_set_source_u32(cairo, state->args.colors.bs_highlight);
				}
			}
			cairo_stroke(cairo);

			// Draw borders
			double inner_radius = arc_radius + arc_thickness - arc_thickness * 1.5;
			double outer_radius = arc_radius + arc_thickness - arc_thickness / 2.0;

			cairo_set_line_width(cairo, 2.0 * surface->scale);
			cairo_set_source_u32(cairo, state->args.colors.separator);
			cairo_move_to(cairo,
				cx + cos(highlight_start) * inner_radius,
				cy + sin(highlight_start) * inner_radius
			);
			cairo_line_to(cairo,
				cx + cos(highlight_start) * outer_radius,
				cy + sin(highlight_start) * outer_radius
			);
			cairo_stroke(cairo);

			cairo_move_to(cairo,
				cx + cos(highlight_start + TYPE_INDICATOR_RANGE) * inner_radius,
				cy + sin(highlight_start + TYPE_INDICATOR_RANGE) * inner_radius
			);
			cairo_line_to(cairo,
				cx + cos(highlight_start + TYPE_INDICATOR_RANGE) * outer_radius,
				cy + sin(highlight_start + TYPE_INDICATOR_RANGE) * outer_radius
			);
			cairo_stroke(cairo);
		}

		// Draw inner + outer border of the circle
		set_color_for_state(cairo, state, &state->args.colors.line);
		cairo_set_line_width(cairo, 2.0 * surface->scale);
		cairo_arc(cairo, cx, cy,
				arc_radius - arc_thickness / 2, 0, 2 * M_PI);
		cairo_stroke(cairo);
		cairo_arc(cairo, cx, cy,
				arc_radius + arc_thickness / 2, 0, 2 * M_PI);
		cairo_stroke(cairo);

		// display layout text separately
		if (layout_text) {
			cairo_text_extents_t extents;
			cairo_font_extents_t fe;
			double x, y;
			double box_padding = 4.0 * surface->scale;
			cairo_text_extents(cairo, layout_text, &extents);
			cairo_font_extents(cairo, &fe);
			// upper left coordinates for box
			x = (cx) - (extents.width / 2) - box_padding;
			y = cy + arc_radius + arc_thickness;

			// background box
			cairo_rectangle(cairo, x, y,
				extents.width + 2.0 * box_padding,
				fe.height + 2.0 * box_padding);
			cairo_set_source_u32(cairo, state->args.colors.layout_background);
			cairo_fill_preserve(cairo);
			// border
			cairo_set_source_u32(cairo, state->args.colors.layout_border);
			cairo_stroke(cairo);

			// take font extents and padding into account
			cairo_move_to(cairo,
				x - extents.x_bearing + box_padding,
				y + (fe.height - fe.descent) + box_padding);
			cairo_set_source_u32(cairo, state->args.colors.layout_text);
			cairo_show_text(cairo, layout_text);
			cairo_new_sub_path(cairo);
		}
	}

	// Send Wayland requests
	wl_subsurface_set_position(surface->subsurface, subsurf_xpos, subsurf_ypos);

	wl_surface_set_buffer_scale(surface->child, surface->scale);
	wl_surface_attach(surface->child, buffer->buffer, 0, 0);
	wl_surface_damage_buffer(surface->child, 0, 0, INT32_MAX, INT32_MAX);
	wl_surface_commit(surface->child);

	return true;
}
