#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <locale.h>
#include <wayland-client.h>
#include "cairo.h"
#include "background-image.h"
#include "swaylock.h"

#define M_PI 3.14159265358979323846
const float TYPE_INDICATOR_RANGE = M_PI / 3.0f;
const float TYPE_INDICATOR_BORDER_THICKNESS = M_PI / 128.0f;

static void set_color_for_state(cairo_t *cairo, struct swaylock_state *state,
		struct swaylock_colorset *colorset) {
	if (state->auth_state == AUTH_STATE_VALIDATING) {
		cairo_set_source_u32(cairo, colorset->verifying);
	} else if (state->auth_state == AUTH_STATE_INVALID) {
		cairo_set_source_u32(cairo, colorset->wrong);
	} else if (state->auth_state == AUTH_STATE_CLEAR) {
		cairo_set_source_u32(cairo, colorset->cleared);
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

static void timetext(struct swaylock_surface *surface, char **tstr, char **dstr) {
	static char dbuf[256];
	static char tbuf[256];

	// Use user's locale for strftime calls
	char *prevloc = setlocale(LC_TIME, NULL);
	setlocale(LC_TIME, "");

	time_t t = time(NULL);
	struct tm *tm = localtime(&t);

	if (surface->state->args.timestr[0]) {
		strftime(tbuf, sizeof(tbuf), surface->state->args.timestr, tm);
		*tstr = tbuf;
	} else {
		*tstr = NULL;
	}

	if (surface->state->args.datestr[0]) {
		strftime(dbuf, sizeof(dbuf), surface->state->args.datestr, tm);
		*dstr = dbuf;
	} else {
		*dstr = NULL;
	}

	// Set it back, so we don't break stuff
	setlocale(LC_TIME, prevloc);
}

void render_frame_background(struct swaylock_surface *surface) {
	struct swaylock_state *state = surface->state;

	int buffer_width = surface->width * surface->scale;
	int buffer_height = surface->height * surface->scale;
	if (buffer_width == 0 || buffer_height == 0) {
		return; // not yet configured
	}

	surface->current_buffer = get_next_buffer(state->shm,
			surface->buffers, buffer_width, buffer_height);
	if (surface->current_buffer == NULL) {
		return;
	}

	cairo_t *cairo = surface->current_buffer->cairo;
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

	wl_surface_set_buffer_scale(surface->surface, surface->scale);
	wl_surface_attach(surface->surface, surface->current_buffer->buffer, 0, 0);
	wl_surface_damage(surface->surface, 0, 0, surface->width, surface->height);
	wl_surface_commit(surface->surface);
}

void render_frame(struct swaylock_surface *surface) {
	struct swaylock_state *state = surface->state;

	int arc_radius = state->args.radius * surface->scale;
	int arc_thickness = state->args.thickness * surface->scale;
	int buffer_diameter = (arc_radius + arc_thickness) * 2;

	int buffer_width = surface->indicator_width;
	int buffer_height = surface->indicator_height;
	int new_width = buffer_diameter;
	int new_height = buffer_diameter;

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

	wl_subsurface_set_position(surface->subsurface, subsurf_xpos, subsurf_ypos);

	surface->current_buffer = get_next_buffer(state->shm,
			surface->indicator_buffers, buffer_width, buffer_height);
	if (surface->current_buffer == NULL) {
		return;
	}

	// Hide subsurface until we want it visible
	wl_surface_attach(surface->child, NULL, 0, 0);
	wl_surface_commit(surface->child);

	cairo_t *cairo = surface->current_buffer->cairo;
	cairo_set_antialias(cairo, CAIRO_ANTIALIAS_BEST);
	cairo_font_options_t *fo = cairo_font_options_create();
	cairo_font_options_set_hint_style(fo, CAIRO_HINT_STYLE_FULL);
	cairo_font_options_set_antialias(fo, CAIRO_ANTIALIAS_SUBPIXEL);
	cairo_font_options_set_subpixel_order(fo, to_cairo_subpixel_order(surface->subpixel));
	cairo_set_font_options(cairo, fo);
	cairo_font_options_destroy(fo);
	cairo_identity_matrix(cairo);

	// Clear
	cairo_save(cairo);
	cairo_set_source_rgba(cairo, 0, 0, 0, 0);
	cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);
	cairo_paint(cairo);
	cairo_restore(cairo);

	float type_indicator_border_thickness =
		TYPE_INDICATOR_BORDER_THICKNESS * surface->scale;

	// This is a bit messy.
	// After the fork, upstream added their own --indicator-idle-visible option,
	// but it works slightly differently from swaylock-effects' --indicator
	// option. To maintain compatibility with upstream swaylock scripts as well
	// as with old swaylock-effects scripts, I will keep both flags.
	bool upstream_show_indicator =
		state->args.show_indicator && (state->auth_state != AUTH_STATE_IDLE ||
			state->args.indicator_idle_visible);

	if (state->args.indicator || upstream_show_indicator) {
		// Draw circle
		cairo_set_line_width(cairo, arc_thickness);
		cairo_arc(cairo, buffer_width / 2, buffer_diameter / 2, arc_radius,
				0, 2 * M_PI);
		set_color_for_state(cairo, state, &state->args.colors.inside);
		cairo_fill_preserve(cairo);
		set_color_for_state(cairo, state, &state->args.colors.ring);
		cairo_stroke(cairo);

		// Draw a message
		char *text = NULL;
		char *text_l1 = NULL;
		char *text_l2 = NULL;
		const char *layout_text = NULL;
		double font_size;
		char attempts[4]; // like i3lock: count no more than 999
		set_color_for_state(cairo, state, &state->args.colors.text);
		cairo_select_font_face(cairo, state->args.font,
				CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
		if (state->args.font_size > 0) {
			font_size = state->args.font_size;
		} else {
			font_size = arc_radius / 3.0f;
		}
		cairo_set_font_size(cairo, font_size);
		switch (state->auth_state) {
		case AUTH_STATE_VALIDATING:
			text = "verifying";
			break;
		case AUTH_STATE_INVALID:
			text = "wrong";
			break;
		case AUTH_STATE_CLEAR:
			text = "cleared";
			break;
		case AUTH_STATE_INPUT:
		case AUTH_STATE_INPUT_NOP:
		case AUTH_STATE_BACKSPACE:
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
			} else if (state->args.clock) {
				timetext(surface, &text_l1, &text_l2);
			}

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
			break;
		default:
			if (state->args.clock)
				timetext(surface, &text_l1, &text_l2);
			break;
		}

		if (text_l1 && !text_l2)
			text = text_l1;
		if (text_l2 && !text_l1)
			text = text_l2;

		if (text) {
			cairo_text_extents_t extents;
			cairo_font_extents_t fe;
			double x, y;
			cairo_text_extents(cairo, text, &extents);
			cairo_font_extents(cairo, &fe);
			x = (buffer_width / 2) -
				(extents.width / 2 + extents.x_bearing);
			y = (buffer_diameter / 2) +
				(fe.height / 2 - fe.descent);

			cairo_move_to(cairo, x, y);
			cairo_show_text(cairo, text);
			cairo_close_path(cairo);
			cairo_new_sub_path(cairo);

			if (new_width < extents.width) {
				new_width = extents.width;
			}
		} else if (text_l1 && text_l2) {
			cairo_text_extents_t extents_l1, extents_l2;
			cairo_font_extents_t fe_l1, fe_l2;
			double x_l1, y_l1, x_l2, y_l2;

			/* Top */

			cairo_text_extents(cairo, text_l1, &extents_l1);
			cairo_font_extents(cairo, &fe_l1);
			x_l1 = (buffer_width / 2) -
				(extents_l1.width / 2 + extents_l1.x_bearing);
			y_l1 = (buffer_diameter / 2) +
				(fe_l1.height / 2 - fe_l1.descent) - arc_radius / 10.0f;

			cairo_move_to(cairo, x_l1, y_l1);
			cairo_show_text(cairo, text_l1);
			cairo_close_path(cairo);
			cairo_new_sub_path(cairo);

			/* Bottom */

			cairo_set_font_size(cairo, arc_radius / 6.0f);
			cairo_text_extents(cairo, text_l2, &extents_l2);
			cairo_font_extents(cairo, &fe_l2);
			x_l2 = (buffer_width / 2) -
				(extents_l2.width / 2 + extents_l2.x_bearing);
			y_l2 = (buffer_diameter / 2) +
				(fe_l2.height / 2 - fe_l2.descent) + arc_radius / 3.5f;

			cairo_move_to(cairo, x_l2, y_l2);
			cairo_show_text(cairo, text_l2);
			cairo_close_path(cairo);
			cairo_new_sub_path(cairo);

			if (new_width < extents_l1.width)
				new_width = extents_l1.width;
			if (new_width < extents_l2.width)
				new_width = extents_l2.width;


			cairo_set_font_size(cairo, font_size);
		}

		// Typing indicator: Highlight random part on keypress
		if (state->auth_state == AUTH_STATE_INPUT
				|| state->auth_state == AUTH_STATE_BACKSPACE) {

			static double highlight_start = 0;
			if (state->indicator_dirty) {
				highlight_start +=
					(rand() % (int)(M_PI * 100)) / 100.0 + M_PI * 0.5;
				state->indicator_dirty = false;
			}

			cairo_arc(cairo, buffer_width / 2, buffer_diameter / 2,
					arc_radius, highlight_start,
					highlight_start + TYPE_INDICATOR_RANGE);
			if (state->auth_state == AUTH_STATE_INPUT) {
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
			cairo_set_source_u32(cairo, state->args.colors.separator);
			cairo_arc(cairo, buffer_width / 2, buffer_diameter / 2,
					arc_radius, highlight_start,
					highlight_start + type_indicator_border_thickness);
			cairo_stroke(cairo);

			cairo_arc(cairo, buffer_width / 2, buffer_diameter / 2,
					arc_radius, highlight_start + TYPE_INDICATOR_RANGE,
					highlight_start + TYPE_INDICATOR_RANGE +
						type_indicator_border_thickness);
			cairo_stroke(cairo);
		}

		// Draw inner + outer border of the circle
		set_color_for_state(cairo, state, &state->args.colors.line);
		cairo_set_line_width(cairo, 2.0 * surface->scale);
		cairo_arc(cairo, buffer_width / 2, buffer_diameter / 2,
				arc_radius - arc_thickness / 2, 0, 2 * M_PI);
		cairo_stroke(cairo);
		cairo_arc(cairo, buffer_width / 2, buffer_diameter / 2,
				arc_radius + arc_thickness / 2, 0, 2 * M_PI);
		cairo_stroke(cairo);

		// display layout text seperately
		if (layout_text) {
			cairo_text_extents_t extents;
			cairo_font_extents_t fe;
			double x, y;
			double box_padding = 4.0 * surface->scale;
			cairo_text_extents(cairo, layout_text, &extents);
			cairo_font_extents(cairo, &fe);
			// upper left coordinates for box
			x = (buffer_width / 2) - (extents.width / 2) - box_padding;
			y = buffer_diameter;

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

			new_height += fe.height + 2 * box_padding;
			if (new_width < extents.width + 2 * box_padding) {
				new_width = extents.width + 2 * box_padding;
			}
		}

		if (buffer_width != new_width || buffer_height != new_height) {
			destroy_buffer(surface->current_buffer);
			surface->indicator_width = new_width;
			surface->indicator_height = new_height;
			render_frame(surface);
		}
	}

	wl_surface_set_buffer_scale(surface->child, surface->scale);
	wl_surface_attach(surface->child, surface->current_buffer->buffer, 0, 0);
	wl_surface_damage(surface->child, 0, 0, surface->current_buffer->width, surface->current_buffer->height);
	wl_surface_commit(surface->child);

	wl_surface_commit(surface->surface);
}

void render_frames(struct swaylock_state *state) {
	struct swaylock_surface *surface;
	wl_list_for_each(surface, &state->surfaces, link) {
		render_frame(surface);
	}
}
