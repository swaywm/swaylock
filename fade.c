#include "fade.h"
#include "pool-buffer.h"
#include "swaylock.h"
#include <stdio.h>
#include <omp.h>
#include <stdalign.h>
#include <string.h>

#ifdef FADE_PROFILE
#include <time.h>
double get_time() {
	struct timespec tv;
	clock_gettime(CLOCK_MONOTONIC, &tv);
	return tv.tv_sec + (tv.tv_nsec / 1000000000.0);
}
#endif

#ifdef __SSE2__
#define set_alpha set_alpha_sse

#include <immintrin.h>

static void set_alpha_sse(uint32_t *orig, struct pool_buffer *buf, float alpha) {
	int alpha_factor = (int)(alpha * (1 << 16));
	if (alpha_factor != 0)
		alpha_factor -= 1;

	__m128i alpha_vec = _mm_set_epi16(
			alpha_factor, alpha_factor, alpha_factor, alpha_factor,
			alpha_factor, alpha_factor, alpha_factor, alpha_factor);
	__m128i dummy_vec = _mm_set_epi16(0, 0, 0, 0, 0, 0, 0, 0);

	uint8_t *orig_bytes = (uint8_t *)orig;
	uint8_t *dest_bytes = (uint8_t *)buf->data;
	size_t length = ((size_t)buf->width * (size_t)buf->height * 4) / 8;

	for (size_t i = 0; i < length; ++i) {
		size_t index = i * 8;

		// Read data into SSE register, where each byte is an u16
		__m128i argb_vec = _mm_loadu_si64(orig_bytes + index);
		argb_vec = _mm_unpacklo_epi8(argb_vec, dummy_vec);

		// Multiply the 8 argb u16s with the 8 alpha u16s
		argb_vec = _mm_mulhi_epu16(argb_vec, alpha_vec);

		// Put the low bytes of each argb u16 into the destination buffer
		argb_vec = _mm_packus_epi16(argb_vec, dummy_vec);
		_mm_storeu_si64(dest_bytes + index, argb_vec);
	}
}

#else
#define set_alpha set_alpha_slow

static void set_alpha_slow(uint32_t *orig, struct pool_buffer *buf, float alpha) {
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

#endif

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

#ifdef FADE_PROFILE
	double before = get_time();
#endif

	set_alpha(fade->original_buffer, buffer, alpha);

#ifdef FADE_PROFILE
	double after = get_time();
	printf("set alpha in %fms (%fFPS). %fms since last time, FPS: %f\n",
			(after - before) * 1000, 1 / (after - before),
			delta, 1000 / delta);
#endif
}

bool fade_is_complete(struct swaylock_fade *fade) {
	return fade->target_time == 0 || fade->current_time >= fade->target_time;
}

void fade_destroy(struct swaylock_fade *fade) {
	free(fade->original_buffer);
}
