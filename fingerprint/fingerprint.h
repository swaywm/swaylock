#ifndef _FINGERPRINT_H
#define _FINGERPRINT_H

#include "swaylock.h"

void fingerprint_init(struct swaylock_state *state);
int fingerprint_verify(struct swaylock_state *state);
void fingerprint_deinit();

#endif