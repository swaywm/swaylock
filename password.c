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

#if HAVE_SYSTEMD
#include <systemd/sd-bus.h>
#include <systemd/sd-login.h>
#elif HAVE_ELOGIND
#include <elogind/sd-bus.h>
#include <elogind/sd-login.h>
#endif

void clear_buffer(char *buf, size_t size) {
	// Use volatile keyword so so compiler can't optimize this out.
	volatile char *buffer = buf;
	volatile char zero = '\0';
	for (size_t i = 0; i < size; ++i) {
		buffer[i] = zero;
	}
}

void clear_password_buffer(struct swaylock_password *pw) {
	clear_buffer(pw->buffer, sizeof(pw->buffer));
	pw->len = 0;
}

static bool backspace(struct swaylock_password *pw) {
	if (pw->len != 0) {
		pw->buffer[--pw->len] = 0;
		return true;
	}
	return false;
}

static void append_ch(struct swaylock_password *pw, uint32_t codepoint) {
	size_t utf8_size = utf8_chsize(codepoint);
	if (pw->len + utf8_size + 1 >= sizeof(pw->buffer)) {
		// TODO: Display error
		return;
	}
	utf8_encode(&pw->buffer[pw->len], codepoint);
	pw->buffer[pw->len + utf8_size] = 0;
	pw->len += utf8_size;
}

static void clear_indicator(void *data) {
	struct swaylock_state *state = data;
	state->clear_indicator_timer = NULL;
	state->auth_state = AUTH_STATE_IDLE;
	damage_state(state);
}

void schedule_indicator_clear(struct swaylock_state *state) {
	if (state->clear_indicator_timer) {
		loop_remove_timer(state->eventloop, state->clear_indicator_timer);
	}
	state->clear_indicator_timer = loop_add_timer(
			state->eventloop, 3000, clear_indicator, state);
}

static void clear_password(void *data) {
	struct swaylock_state *state = data;
	state->clear_password_timer = NULL;
	state->auth_state = AUTH_STATE_CLEAR;
	clear_password_buffer(&state->password);
	damage_state(state);
	schedule_indicator_clear(state);
}

static void schedule_password_clear(struct swaylock_state *state) {
	if (state->clear_password_timer) {
		loop_remove_timer(state->eventloop, state->clear_password_timer);
	}
	state->clear_password_timer = loop_add_timer(
			state->eventloop, 10000, clear_password, state);
}

static void submit_password(struct swaylock_state *state) {
	if (state->args.ignore_empty && state->password.len == 0) {
		return;
	}

	state->auth_state = AUTH_STATE_VALIDATING;

	if (!write_comm_request(&state->password)) {
		state->auth_state = AUTH_STATE_INVALID;
		schedule_indicator_clear(state);
	}

	damage_state(state);
}

#if HAVE_SYSTEMD || HAVE_ELOGIND

static struct sd_bus *bus = NULL;

void connect_to_bus(void) {
	int ret = sd_bus_default_system(&bus);
	if (ret < 0) {
		errno = -ret;
		swaylock_log_errno(LOG_ERROR, "Failed to open D-Bus connection");
		return;
	}
}

static void suspend(void) {
	sd_bus_message *msg = NULL;
	sd_bus_error error = SD_BUS_ERROR_NULL;
	int ret = sd_bus_call_method(bus, "org.freedesktop.login1",
			"/org/freedesktop/login1",
			"org.freedesktop.login1.Manager", "Suspend",
			&error, &msg, "b", true);
	if (ret < 0) {
		swaylock_log(LOG_ERROR, "Failed to suspend: %s", error.message);
	}

	sd_bus_error_free(&error);
	sd_bus_message_unref(msg);
}

#endif

void swaylock_handle_key(struct swaylock_state *state,
		xkb_keysym_t keysym, uint32_t codepoint) {
	// Ignore input events if validating
	if (state->auth_state == AUTH_STATE_VALIDATING) {
		return;
	}

	switch (keysym) {
	case XKB_KEY_KP_Enter: /* fallthrough */
	case XKB_KEY_Return:
		submit_password(state);
		break;
	case XKB_KEY_Delete:
#if HAVE_SYSTEMD || HAVE_ELOGIND
		if (state->xkb.super) {
			clear_password_buffer(&state->password);
			state->auth_state = AUTH_STATE_IDLE;
			damage_state(state);
			suspend();
			break;
		}
#endif
		// fallthrough
	case XKB_KEY_BackSpace:
		if (backspace(&state->password)) {
			state->auth_state = AUTH_STATE_BACKSPACE;
		} else {
			state->auth_state = AUTH_STATE_CLEAR;
		}
		damage_state(state);
		schedule_indicator_clear(state);
		schedule_password_clear(state);
		break;
	case XKB_KEY_Escape:
		clear_password_buffer(&state->password);
		state->auth_state = AUTH_STATE_CLEAR;
		damage_state(state);
		schedule_indicator_clear(state);
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
		state->auth_state = AUTH_STATE_INPUT_NOP;
		damage_state(state);
		schedule_indicator_clear(state);
		schedule_password_clear(state);
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
			state->auth_state = AUTH_STATE_CLEAR;
			damage_state(state);
			schedule_indicator_clear(state);
			break;
		}
		// fallthrough
	default:
		if (codepoint) {
			append_ch(&state->password, codepoint);
			state->auth_state = AUTH_STATE_INPUT;
			damage_state(state);
			schedule_indicator_clear(state);
			schedule_password_clear(state);
		}
		break;
	}
}
