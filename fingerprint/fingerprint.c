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

static FprintDBusManager *manager = NULL;
static GDBusConnection *connection = NULL;
static gboolean g_fatal_warnings = FALSE;
static FprintDBusDevice *device = NULL;

static void
create_manager (void)
{
    g_autoptr(GError) error = NULL;

    connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
    if (connection == NULL)
    {
        g_print ("Failed to connect to session bus: %s\n", error->message);
        return;
    }

    manager = fprint_dbus_manager_proxy_new_sync (connection,
                                                  G_DBUS_PROXY_FLAGS_NONE,
                                                  "net.reactivated.Fprint",
                                                  "/net/reactivated/Fprint/Manager",
                                                  NULL, &error);
    if (manager == NULL)
    {
        g_print ("Failed to get Fprintd manager: %s\n", error->message);
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
        g_print ("Impossible to verify: %s\n", error->message);
        return NULL;
    }

    g_print ("Using device %s\n", path);

    dev = fprint_dbus_device_proxy_new_sync (connection,
                                             G_DBUS_PROXY_FLAGS_NONE,
                                             "net.reactivated.Fprint",
                                             path, NULL, &error);

    if (error)
    {
        g_print ("failed to connect to device: %s\n", error->message);
        return NULL;
    }

    if (!fprint_dbus_device_call_claim_sync (dev, username, NULL, &error))
    {
        g_print ("failed to claim device: %s\n", error->message);
        return NULL;
    }

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

    g_print ("Verify result: %s (%s)\n", result, done ? "done" : "not done");
    verify_state->match = g_str_equal (result, "verify-match");

    if (done != FALSE) {
        verify_state->completed = TRUE;
        g_autoptr(GError) error = NULL;
        if (!fprint_dbus_device_call_verify_stop_sync (device, NULL, &error))
        {
            g_print ("VerifyStop failed: %s\n", error->message);
        }
    }
}

static void
verify_finger_selected (GObject *object, const char *name, void *user_data)
{
    g_print ("Verifying: %s\n", name);
}

static void
verify_started_cb (GObject      *obj,
                   GAsyncResult *res,
                   gpointer      user_data)
{
    struct VerifyState *verify_state = user_data;

    if (fprint_dbus_device_call_verify_start_finish (FPRINT_DBUS_DEVICE (obj), res, &verify_state->error))
    {
        g_print ("Verify started!\n");
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

    g_signal_connect (dev, "g-signal", G_CALLBACK (proxy_signal_cb),
                      &verify_state);

    fprint_dbus_device_call_verify_start (dev, "any", NULL,
                                          verify_started_cb,
                                          &verify_state);

    /* Wait for verify start while discarding any VerifyStatus signals */
    while (!verify_state.started && !verify_state.error)
        g_main_context_iteration (NULL, TRUE);

    if (verify_state.error)
    {
        g_print ("VerifyStart failed: %s\n", verify_state.error->message);
        g_clear_error (&verify_state.error);
        return;
    }
}

static void
release_device (FprintDBusDevice *dev)
{
    g_autoptr(GError) error = NULL;
    if (!fprint_dbus_device_call_release_sync (dev, NULL, &error))
    {
        g_print ("ReleaseDevice failed: %s\n", error->message);
    }
}

void fingerprint_init ( void )
{
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

    start_verify(device);
}

void fingerprint_deinit ( void ) {
    if(device) {
        release_device (device);
    }
}

int fingerprint_verify ( void ) {
    /* VerifyStatus signals are processing, do not wait for completion. */
    g_main_context_iteration (NULL, FALSE);
    if(manager == NULL || connection == NULL || device == NULL) {
        return false;
    }

    if(!verify_state.completed) {
        return false;
    }

    g_signal_handlers_disconnect_by_func (device, proxy_signal_cb,
                                          &verify_state);
    release_device (device);
    device = NULL;
    return true;
}
