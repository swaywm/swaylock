#include <assert.h>
#include <errno.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <xkbcommon/xkbcommon.h>
#include "comm.h"
#include "log.h"
#include "loop.h"
#include "seat.h"
#include "swaylock.h"
#include "unicode.h"

void clear_buffer(char *buf, size_t size) {
	// Use volatile keyword so so compiler can't optimize this out.
	volatile char *buffer = buf;
	volatile char zero = '\0';
	for (size_t i = 0; i < size; ++i) {
		buffer[i] = zero;
	}
}

void clear_password_buffer(struct swaylock_password *pw) {
	clear_buffer(pw->buffer, pw->buffer_len);
	pw->len = 0;
}

static bool backspace(struct swaylock_password *pw) {
	if (pw->len != 0) {
		pw->len -= utf8_last_size(pw->buffer);
		pw->buffer[pw->len] = 0;
		return true;
	}
	return false;
}

static void append_ch(struct swaylock_password *pw, uint32_t codepoint) {
	size_t utf8_size = utf8_chsize(codepoint);
	if (pw->len + utf8_size + 1 >= pw->buffer_len) {
		// TODO: Display error
		return;
	}
	utf8_encode(&pw->buffer[pw->len], codepoint);
	pw->buffer[pw->len + utf8_size] = 0;
	pw->len += utf8_size;
}

static void set_input_idle(void *data) {
	struct swaylock_state *state = data;
	state->input_idle_timer = NULL;
	state->input_state = INPUT_STATE_IDLE;
	damage_state(state);
}

static void set_auth_idle(void *data) {
	struct swaylock_state *state = data;
	state->auth_idle_timer = NULL;
	state->auth_state = AUTH_STATE_IDLE;
	damage_state(state);
}

static void schedule_input_idle(struct swaylock_state *state) {
	if (state->input_idle_timer) {
		loop_remove_timer(state->eventloop, state->input_idle_timer);
	}
	state->input_idle_timer = loop_add_timer(
		state->eventloop, 1500, set_input_idle, state);
}

static void cancel_input_idle(struct swaylock_state *state) {
	if (state->input_idle_timer) {
		loop_remove_timer(state->eventloop, state->input_idle_timer);
		state->input_idle_timer = NULL;
	}
}

void schedule_auth_idle(struct swaylock_state *state) {
	if (state->auth_idle_timer) {
		loop_remove_timer(state->eventloop, state->auth_idle_timer);
	}
	state->auth_idle_timer = loop_add_timer(
		state->eventloop, 3000, set_auth_idle, state);
}

static void clear_password(void *data) {
	struct swaylock_state *state = data;
	state->clear_password_timer = NULL;
	state->input_state = INPUT_STATE_CLEAR;
	schedule_input_idle(state);
	clear_password_buffer(&state->password);
	damage_state(state);
}

static void schedule_password_clear(struct swaylock_state *state) {
	if (state->clear_password_timer) {
		loop_remove_timer(state->eventloop, state->clear_password_timer);
	}
	state->clear_password_timer = loop_add_timer(
			state->eventloop, 10000, clear_password, state);
}

static void cancel_password_clear(struct swaylock_state *state) {
	if (state->clear_password_timer) {
		loop_remove_timer(state->eventloop, state->clear_password_timer);
		state->clear_password_timer = NULL;
	}
}

static void submit_password(struct swaylock_state *state) {
	if (state->args.ignore_empty && state->password.len == 0) {
		return;
	}

	state->input_state = INPUT_STATE_IDLE;
	state->auth_state = AUTH_STATE_VALIDATING;
	cancel_password_clear(state);
	cancel_input_idle(state);

	if (!write_comm_request(&state->password)) {
		state->auth_state = AUTH_STATE_INVALID;
		schedule_auth_idle(state);
	}

	damage_state(state);
}

static void update_highlight(struct swaylock_state *state) {
	// Advance a random amount between 1/4 and 3/4 of a full turn
	state->highlight_start =
		(state->highlight_start + (rand() % 1024) + 512) % 2048;
}

void swaylock_handle_key(struct swaylock_state *state,
		xkb_keysym_t keysym, uint32_t codepoint) {

	switch (keysym) {
	case XKB_KEY_KP_Enter: /* fallthrough */
	case XKB_KEY_Return:
		submit_password(state);
		break;
	case XKB_KEY_Delete:
	case XKB_KEY_BackSpace:
		if (backspace(&state->password)) {
			state->input_state = INPUT_STATE_BACKSPACE;
			schedule_password_clear(state);
			update_highlight(state);
		} else {
			state->input_state = INPUT_STATE_CLEAR;
			cancel_password_clear(state);
		}
		schedule_input_idle(state);
		damage_state(state);
		break;
	case XKB_KEY_Escape:
		clear_password_buffer(&state->password);
		state->input_state = INPUT_STATE_CLEAR;
		cancel_password_clear(state);
		schedule_input_idle(state);
		damage_state(state);
		break;
	case XKB_KEY_Caps_Lock:
	case XKB_KEY_Shift_L:
	case XKB_KEY_Shift_R:
	case XKB_KEY_Control_L:
	case XKB_KEY_Control_R:
	case XKB_KEY_Meta_L:
	case XKB_KEY_Meta_R:
	case XKB_KEY_Alt_L:
	case XKB_KEY_Alt_R:
	case XKB_KEY_Super_L:
	case XKB_KEY_Super_R:
		state->input_state = INPUT_STATE_NEUTRAL;
		schedule_password_clear(state);
		schedule_input_idle(state);
		damage_state(state);
		break;
	case XKB_KEY_m: /* fallthrough */
	case XKB_KEY_d:
	case XKB_KEY_j:
		if (state->xkb.control) {
			submit_password(state);
			break;
		}
		// fallthrough
	case XKB_KEY_c: /* fallthrough */
	case XKB_KEY_u:
		if (state->xkb.control) {
			clear_password_buffer(&state->password);
			state->input_state = INPUT_STATE_CLEAR;
			cancel_password_clear(state);
			schedule_input_idle(state);
			damage_state(state);
			break;
		}
		// fallthrough
	default:
		if (codepoint) {
			append_ch(&state->password, codepoint);
			state->input_state = INPUT_STATE_LETTER;
			schedule_password_clear(state);
			schedule_input_idle(state);
			update_highlight(state);
			damage_state(state);
		}
		break;
	}
}
