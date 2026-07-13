/*
 * Copyright (C) 2023 Alexandr Lutsai <s.lyra@ya.ru>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef _FINGERPRINT_H
#define _FINGERPRINT_H

#include "swaylock.h"
#include "fingerprint/fprintd-dbus.h"

struct FingerprintState {
	GError	*error;
	gboolean started;
	gboolean completed;
	gboolean match;

	char status[128];

	FprintDBusManager *manager;
	GDBusConnection *connection;
	FprintDBusDevice *device;
	struct swaylock_state *sw_state;
};

void fingerprint_init(struct FingerprintState *fingerprint_state, struct swaylock_state *state);
int fingerprint_verify(struct FingerprintState *fingerprint_state);
void fingerprint_deinit(struct FingerprintState *fingerprint_state);

#endif