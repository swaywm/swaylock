#include "fade.h"
#include "pool-buffer.h"
#include "swaylock.h"
#include <stdio.h>
#include <time.h>
#include <omp.h>

// TODO: optimize this with AVX instructions.
// On my laptop, using 100% of all cores, it would barely be able to fade
// one screen at 90FPS.
// The reason this isn't just a simple "set the alpha byte of each pixel" operation
// is that Wayland uses pre-multiplied alpha.
static void set_alpha(uint32_t *orig, struct pool_buffer *buf, float alpha) {
#pragma omp parallel for
	for (size_t y = 0; y < buf->height; ++y) {
		for (size_t x = 0; x < buf->width; ++x) {
			size_t index = y * buf->width + x;
			uint32_t srcpix = orig[index];
			int srcr = (srcpix & 0x00ff0000u) >> 16;
			int srcg = (srcpix & 0x0000ff00u) >> 8;
			int srcb = (srcpix & 0x000000ffu);

			((uint32_t *)buf->data)[index] = 0 |
				(uint32_t)(alpha * 255) << 24 |
				(uint32_t)(srcr * alpha) << 16 |
				(uint32_t)(srcg * alpha) << 8 |
				(uint32_t)(srcb * alpha);
		}
	}
}

void fade_prepare(struct swaylock_fade *fade, struct pool_buffer *buffer) {
	if (!fade->target_time) {
		fade->original_buffer = NULL;
		return;
	}

	size_t size = (size_t)buffer->width * (size_t)buffer->height * 4;
	fade->original_buffer = malloc(size);
	memcpy(fade->original_buffer, buffer->data, size);

	set_alpha(fade->original_buffer, buffer, 0);
}

void fade_update(struct swaylock_fade *fade, struct pool_buffer *buffer, uint32_t time) {
	if (fade->current_time >= fade->target_time) {
		return;
	}

	double delta = 0;
	if (fade->old_time != 0) {
		delta = time - fade->old_time;
	}
	fade->old_time = time;

	fade->current_time += delta;
	if (fade->current_time > fade->target_time) {
		fade->current_time = fade->target_time;
	}

	double alpha = (double)fade->current_time / (double)fade->target_time;
	set_alpha(fade->original_buffer, buffer, alpha);
}

bool fade_is_complete(struct swaylock_fade *fade) {
	return fade->target_time == 0 || fade->current_time >= fade->target_time;
}

void fade_destroy(struct swaylock_fade *fade) {
	free(fade->original_buffer);
}
