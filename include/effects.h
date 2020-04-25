#ifndef _SWAYLOCK_EFFECTS_H
#define _SWAYLOCK_EFFECTS_H

#include <stdbool.h>

#include "cairo.h"

struct swaylock_effect_screen_pos {
	float pos;
	bool is_percent;
};

struct swaylock_effect {
	union {
		struct {
			int radius, times;
		} blur;
		struct {
			int factor;
		} pixelate;
		double scale;
		struct {
			double base;
			double factor;
		} vignette;
		struct {
			struct swaylock_effect_screen_pos x;
			struct swaylock_effect_screen_pos y;
			struct swaylock_effect_screen_pos w;
			struct swaylock_effect_screen_pos h;
			enum {
				EFFECT_COMPOSE_GRAV_CENTER,
				EFFECT_COMPOSE_GRAV_NW,
				EFFECT_COMPOSE_GRAV_NE,
				EFFECT_COMPOSE_GRAV_SW,
				EFFECT_COMPOSE_GRAV_SE,
				EFFECT_COMPOSE_GRAV_N,
				EFFECT_COMPOSE_GRAV_S,
				EFFECT_COMPOSE_GRAV_E,
				EFFECT_COMPOSE_GRAV_W,
			} gravity;
			char *imgpath;
		} compose;
		char *custom;
	} e;

	enum {
		EFFECT_BLUR,
		EFFECT_PIXELATE,
		EFFECT_SCALE,
		EFFECT_GREYSCALE,
		EFFECT_VIGNETTE,
		EFFECT_COMPOSE,
		EFFECT_CUSTOM,
	} tag;
};

cairo_surface_t *swaylock_effects_run(cairo_surface_t *surface,
		struct swaylock_effect *effects, int count);

#endif
