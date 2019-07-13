#ifndef _SWAYLOCK_EFFECTS_H_
#define _SWAYLOCK_EFFECTS_H_

#include "cairo.h"

struct swaylock_effect {
	union {
		struct {
			int radius, times;
		} blur;
		double scale;
		struct {
			double base;
			double factor;
		} vignette;
	} e;

	enum {
		EFFECT_BLUR,
		EFFECT_SCALE,
		EFFECT_GREYSCALE,
		EFFECT_VIGNETTE,
	} tag;
};

cairo_surface_t *swaylock_effects_run(cairo_surface_t *surface,
		struct swaylock_effect *effects, int count);

#endif
