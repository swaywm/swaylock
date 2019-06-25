#ifndef _SWAYLOCK_EFFECTS_H_
#define _SWAYLOCK_EFFECTS_H_

#include "cairo.h"

struct swaylock_effect {
	union {
		struct {
			int radius, times;
		} blur;
	} e;

	enum {
		EFFECT_BLUR,
	} tag;
};

cairo_surface_t *swaylock_effects_run(cairo_surface_t *surface,
		struct swaylock_effect *effects, int count);

#endif
