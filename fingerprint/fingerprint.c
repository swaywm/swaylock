/*
 * Based on fprintd example to verify a fingerprint
 * Copyright (C) 2008 Daniel Drake <dsd@gentoo.org>
 * Copyright (C) 2020 Marco Trevisan <marco.trevisan@canonical.com>
 * Copyright (C) 2022 Alexandr Lutsai <s.lyra@ya.ru>
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

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <gio/gio.h>

#include "fingerprint.h"
#include "fingerprint/fprintd-dbus.h"
#include "swaylock.h"

static FprintDBusManager *manager = NULL;
static GDBusConnection *connection = NULL;
static gboolean g_fatal_warnings = FALSE;
static FprintDBusDevice *device = NULL;
struct swaylock_state *sw_state = NULL;

static char fp_str[128] = {0};
char * get_fp_string() {
    return fp_str;
}


static void
create_manager (void)
{
    g_autoptr(GError) error = NULL;

    connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
    if (connection == NULL)
    {
        snprintf(fp_str, sizeof(fp_str), "Failed to connect to session bus: %s\n", error->message);
        sw_state->auth_state = AUTH_STATE_FINGERPRINT;
        sw_state->fingerprint_msg = fp_str;
        damage_state(sw_state);
        schedule_indicator_clear(sw_state);
        g_print ("%s\n", fp_str);
        return;
    }

    manager = fprint_dbus_manager_proxy_new_sync (connection,
                                                  G_DBUS_PROXY_FLAGS_NONE,
                                                  "net.reactivated.Fprint",
                                                  "/net/reactivated/Fprint/Manager",
                                                  NULL, &error);

    if (manager == NULL)
    {
        snprintf(fp_str, sizeof(fp_str), "Failed to get Fprintd manager: %s\n", error->message);
        sw_state->auth_state = AUTH_STATE_FINGERPRINT;
        sw_state->fingerprint_msg = fp_str;
        damage_state(sw_state);
        schedule_indicator_clear(sw_state);
        g_print ("%s\n", fp_str);
        return;
    }
}

static FprintDBusDevice *
open_device (const char *username)
{
    g_autoptr(FprintDBusDevice) dev = NULL;
    g_autoptr(GError) error = NULL;
    g_autofree char *path = NULL;

    if (!fprint_dbus_manager_call_get_default_device_sync (manager, &path,
                                                           NULL, &error))
    {
        snprintf(fp_str, sizeof(fp_str), "Impossible to verify: %s\n", error->message);
        sw_state->auth_state = AUTH_STATE_FINGERPRINT;
        sw_state->fingerprint_msg = fp_str;
        damage_state(sw_state);
        schedule_indicator_clear(sw_state);
        g_print ("%s\n", fp_str);
        return NULL;
    }

    snprintf(fp_str, sizeof(fp_str), "Using device %s\n", path);
    sw_state->auth_state = AUTH_STATE_FINGERPRINT;
        sw_state->fingerprint_msg = fp_str;
        damage_state(sw_state);
        schedule_indicator_clear(sw_state);
    g_print ("%s\n", fp_str);

    dev = fprint_dbus_device_proxy_new_sync (connection,
                                             G_DBUS_PROXY_FLAGS_NONE,
                                             "net.reactivated.Fprint",
                                             path, NULL, &error);

    if (error)
    {
        snprintf(fp_str, sizeof(fp_str), "failed to connect to device: %s\n", error->message);
        sw_state->auth_state = AUTH_STATE_FINGERPRINT;
        sw_state->fingerprint_msg = fp_str;
        damage_state(sw_state);
        schedule_indicator_clear(sw_state);
        g_print ("%s\n", fp_str);
        return NULL;
    }

    if (!fprint_dbus_device_call_claim_sync (dev, username, NULL, &error))
    {
        snprintf(fp_str, sizeof(fp_str), "failed to claim device: %s\n", error->message);
        sw_state->auth_state = AUTH_STATE_FINGERPRINT;
        sw_state->fingerprint_msg = fp_str;
        damage_state(sw_state);
        schedule_indicator_clear(sw_state);
        g_print ("%s\n", fp_str);
        return NULL;
    }

    const gchar * str = fprint_dbus_device_get_name(dev);
    g_print("name name: %s\n", str);

    return g_steal_pointer (&dev);
}

struct VerifyState
{
    GError  *error;
    gboolean started;
    gboolean completed;
    gboolean match;
};

static void
verify_result (GObject *object, const char *result, gboolean done, void *user_data)
{
    struct VerifyState *verify_state = user_data;

    size_t o = snprintf(fp_str, sizeof(fp_str), "Verify result: %s (%s)", result, done ? "done" : "not done");
    sw_state->auth_state = AUTH_STATE_FINGERPRINT;
        sw_state->fingerprint_msg = fp_str;
        damage_state(sw_state);
        schedule_indicator_clear(sw_state);
    g_print ("%lu %s\n", o, fp_str);
    verify_state->match = g_str_equal (result, "verify-match");

    if(g_str_equal (result, "verify-retry-scan")) {
        return;
    }

    g_autoptr(GError) error = NULL;
    if (!fprint_dbus_device_call_verify_stop_sync (device, NULL, &error))
    {
        snprintf(fp_str, sizeof(fp_str), "VerifyStop failed: %s\n", error->message);
        sw_state->auth_state = AUTH_STATE_FINGERPRINT;
        sw_state->fingerprint_msg = fp_str;
        damage_state(sw_state);
        schedule_indicator_clear(sw_state);
    }
    verify_state->completed = TRUE;
}

static void
verify_finger_selected (GObject *object, const char *name, void *user_data)
{
    //snprintf(fp_str, sizeof(fp_str), "Verifying: %s\n", name);
    //g_print ("Verifying: %s\n", name);
}

static void
verify_started_cb (GObject      *obj,
                   GAsyncResult *res,
                   gpointer      user_data)
{
    struct VerifyState *verify_state = user_data;

    if (fprint_dbus_device_call_verify_start_finish (FPRINT_DBUS_DEVICE (obj), res, &verify_state->error))
    {
        snprintf(fp_str, sizeof(fp_str), "Verify started!\n");
        sw_state->auth_state = AUTH_STATE_FINGERPRINT;
        sw_state->fingerprint_msg = fp_str;
        damage_state(sw_state);
        schedule_indicator_clear(sw_state);
        g_print ("%s\n", fp_str);
        verify_state->started = TRUE;
    }
}

static void
proxy_signal_cb (GDBusProxy  *proxy,
                 const gchar *sender_name,
                 const gchar *signal_name,
                 GVariant    *parameters,
                 gpointer     user_data)
{
    struct VerifyState *verify_state = user_data;

    if (!verify_state->started)
        return;

    if (g_str_equal (signal_name, "VerifyStatus"))
    {
        const gchar *result;
        gboolean done;

        g_variant_get (parameters, "(&sb)", &result, &done);
        verify_result (G_OBJECT (proxy), result, done, user_data);
    }
    else if (g_str_equal (signal_name, "VerifyFingerSelected"))
    {
        const gchar *name;

        g_variant_get (parameters, "(&s)", &name);
        verify_finger_selected (G_OBJECT (proxy), name, user_data);
    }
}

static struct VerifyState verify_state = { 0 };
static void
start_verify (FprintDBusDevice *dev)
{
    /* This one is funny. We connect to the signal immediately to avoid
     * race conditions. However, we must ignore any authentication results
     * that happen before our start call returns.
     * This is because the verify call itself may internally try to verify
     * against fprintd (possibly using a separate account).
     *
     * To do so, we *must* use the async version of the verify call, as the
     * sync version would cause the signals to be queued and only processed
     * after it returns.
     */

    fprint_dbus_device_call_verify_start (dev, "any", NULL,
                                          verify_started_cb,
                                          &verify_state);

    /* Wait for verify start while discarding any VerifyStatus signals */
    while (!verify_state.started && !verify_state.error)
        g_main_context_iteration (NULL, TRUE);

    if (verify_state.error)
    {
        snprintf(fp_str, sizeof(fp_str), "VerifyStart failed: %s\n", verify_state.error->message);
        sw_state->auth_state = AUTH_STATE_FINGERPRINT;
        sw_state->fingerprint_msg = fp_str;
        damage_state(sw_state);
        schedule_indicator_clear(sw_state);
        g_print ("%s\n", fp_str);
        g_clear_error (&verify_state.error);
        return;
    }
}

static void release_callback(GObject *source_object,
                             GAsyncResult *res,
                             gpointer user_data) {
    //g_print ("ReleaseDevice failed: %s\n", error->message);
}

static void
release_device (FprintDBusDevice *dev)
{
    fprint_dbus_device_call_release (dev, NULL, release_callback, NULL);
}

void fingerprint_init ( struct swaylock_state *state )
{
    sw_state = state;
    if (g_fatal_warnings)
    {
        GLogLevelFlags fatal_mask;

        fatal_mask = g_log_set_always_fatal (G_LOG_FATAL_MASK);
        fatal_mask |= G_LOG_LEVEL_WARNING | G_LOG_LEVEL_CRITICAL;
        g_log_set_always_fatal (fatal_mask);
    }

    create_manager ();
    if(manager == NULL || connection == NULL) {
        return;
    }
    device = open_device ("");
    if(device == NULL) {
        return;
    }
    g_signal_connect (device, "g-signal", G_CALLBACK (proxy_signal_cb),
                      &verify_state);
    start_verify(device);
}

void fingerprint_deinit ( void ) {
    g_print("rel\n");
    if(device) {
        g_signal_handlers_disconnect_by_func (device, proxy_signal_cb,
                                              &verify_state);
        release_device (device);
        device = NULL;
    }
}

int fingerprint_verify ( struct swaylock_state *state ) {
    sw_state = state;
    /* VerifyStatus signals are processing, do not wait for completion. */
    g_main_context_iteration (NULL, FALSE);
    if(manager == NULL || connection == NULL || device == NULL) {
        return false;
    }

    const gchar * str = fprint_dbus_device_get_name(device);
    g_print("name: %s\n", str);

    if(!verify_state.completed) {
        return false;
    }

    if(!verify_state.match) {
        verify_state.completed = 0;
        verify_state.match = 0;
        fingerprint_deinit();
        fingerprint_init(state);
        return false;
    }

    return true;
}
