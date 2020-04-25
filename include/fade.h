#ifndef _SWAYLOCK_FADE_H
#define _SWAYLOCK_FADE_H

#include <stdbool.h>
#include <stdint.h>

struct pool_buffer;

struct swaylock_fade {
	float current_time;
	float target_time;
	uint32_t old_time;
	uint32_t *original_buffer;
};

void fade_prepare(struct swaylock_fade *fade, struct pool_buffer *buffer);
void fade_update(struct swaylock_fade *fade, struct pool_buffer *buffer, uint32_t time);
bool fade_is_complete(struct swaylock_fade *fade);
void fade_destroy(struct swaylock_fade *fade);

#endif
