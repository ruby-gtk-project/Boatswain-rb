/*
 * bs-desktop-controller.c
 *
 * Copyright 2023 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "Desktop Controller"

#include "bs-desktop-controller-private.h"

#include <libportal-gtk4/portal-gtk4.h>

struct _BsDesktopController
{
  GObject parent_instance;

  GSettings *settings;

  XdpPortal *portal;
  XdpSession *session;

  GCancellable *cancellable;
  GPtrArray *tasks;

  int64_t use_count;
};

G_DEFINE_FINAL_TYPE (BsDesktopController, bs_desktop_controller, G_TYPE_OBJECT)


/*
 * Auxiliary functions
 */

static void
propagate_error_to_tasks (BsDesktopController *self,
                          GError              *error)
{
  g_assert (BS_IS_DESKTOP_CONTROLLER (self));
  g_assert (error != NULL);

  for (size_t i = 0; i < self->tasks->len; i++)
    {
      GTask *task = g_ptr_array_index (self->tasks, i);
      g_task_return_error (task, error);
    }

  g_clear_pointer (&self->tasks, g_ptr_array_unref);
  g_clear_object (&self->cancellable);
}


/*
 * Callbacks
 */

static void
on_session_started_cb (GObject      *source_object,
                       GAsyncResult *result,
                       gpointer      user_data)
{
  g_autoptr (BsDesktopController) self = BS_DESKTOP_CONTROLLER (user_data);
  g_autoptr (GError) error = NULL;
  g_autofree char *restore_token = NULL;

  if (!xdp_session_start_finish (self->session, result, &error))
    {
      propagate_error_to_tasks (self, error);
      return;
    }

  restore_token = xdp_session_get_restore_token (self->session);
  g_settings_set_string (self->settings,
                         "desktop-controller-restore-token",
                         restore_token ?: "");

  for (size_t i = 0; i < self->tasks->len; i++)
    {
      GTask *task = g_ptr_array_index (self->tasks, i);
      g_task_return_boolean (task, TRUE);
      self->use_count++;
    }

  g_clear_pointer (&self->tasks, g_ptr_array_unref);
  g_clear_object (&self->cancellable);
}

static void
on_remote_desktop_session_created_cb (GObject      *source_object,
                                      GAsyncResult *result,
                                      gpointer      user_data)
{
  g_autoptr (BsDesktopController) self = BS_DESKTOP_CONTROLLER (user_data);
  g_autoptr (XdpSession) session = NULL;
  g_autoptr (XdpParent) parent = NULL;
  g_autoptr (GError) error = NULL;
  GtkApplication *application;
  GtkWindow *window;

  g_assert (self->tasks != NULL);
  g_assert (self->tasks->len > 0);

  session = xdp_portal_create_remote_desktop_session_finish (self->portal, result , &error);

  if (error)
    {
      g_warning ("Error creating remote desktop session: %s", error->message);

      propagate_error_to_tasks (self, error);
      return;
    }

  application = GTK_APPLICATION (g_application_get_default ());
  g_assert (GTK_IS_APPLICATION (application));

  window = gtk_application_get_active_window (application);
  if (window)
    parent = xdp_parent_new_gtk (window);

  self->session = g_steal_pointer (&session);

  xdp_session_start (self->session,
                     parent,
                     self->cancellable,
                     on_session_started_cb,
                     g_object_ref (self));
}


/*
 * GObject overrides
 */

static void
bs_desktop_controller_finalize (GObject *object)
{
  BsDesktopController *self = (BsDesktopController *)object;

  g_cancellable_cancel (self->cancellable);

  if (self->session)
    {
      xdp_session_close (self->session);
      g_clear_object (&self->session);
    }

  g_clear_object (&self->cancellable);
  g_clear_object (&self->portal);
  g_clear_object (&self->settings);

  G_OBJECT_CLASS (bs_desktop_controller_parent_class)->finalize (object);
}

static void
bs_desktop_controller_class_init (BsDesktopControllerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = bs_desktop_controller_finalize;
}

static void
bs_desktop_controller_init (BsDesktopController *self)
{
  self->settings = g_settings_new ("com.feaneron.Boatswain");
  self->portal = xdp_portal_new ();
}

BsDesktopController *
bs_desktop_controller_new (void)
{
  return g_object_new (BS_TYPE_DESKTOP_CONTROLLER, NULL);
}

/**
 * bs_desktop_controller_acquire:
 * @self: an #BsDesktopController
 * @cancellable: (nullable): a #GCancellable
 * @callback: a #GAsyncReadyCallback to execute upon completion
 * @user_data: closure data for @callback
 *
 * Acquire control of the desktop. Each individual action that
 * interacts with the desktop controller must acquire it before
 * using any of its APIs.
 */
void
bs_desktop_controller_acquire (BsDesktopController *self,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  g_autofree char *restore_token = NULL;
  g_autoptr (GTask) task = NULL;

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_static_name (task, "bs_desktop_controller_acquire");
  g_task_set_source_tag (task, bs_desktop_controller_acquire);

  /* TODO: connect to cancellable's cancelled */

  if (self->session)
    {
      g_task_return_boolean (task, TRUE);
      self->use_count++;
      return;
    }

  if (self->tasks)
    {
      g_ptr_array_add (self->tasks, g_steal_pointer (&task));
      return;
    }

  g_assert (self->tasks == NULL);
  g_assert (self->cancellable == NULL);

  self->cancellable = g_cancellable_new ();

  self->tasks = g_ptr_array_new_with_free_func (g_object_unref);
  g_ptr_array_add (self->tasks, g_steal_pointer (&task));

  restore_token = g_settings_get_string (self->settings, "desktop-controller-restore-token");
  if (strlen (restore_token) == 0)
    g_set_str (&restore_token, NULL);

  xdp_portal_create_remote_desktop_session_full (self->portal,
                                                 XDP_DEVICE_KEYBOARD,
                                                 XDP_OUTPUT_NONE,
                                                 XDP_REMOTE_DESKTOP_FLAG_NONE,
                                                 XDP_CURSOR_MODE_HIDDEN,
                                                 XDP_PERSIST_MODE_PERSISTENT,
                                                 restore_token,
                                                 self->cancellable,
                                                 on_remote_desktop_session_created_cb,
                                                 g_object_ref (self));
}

/**
 * bs_desktop_controller_acquire_finish:
 * @self: an #BsDesktopController
 * @result: a #GAsyncResult provided to callback
 * @error: a location for a #GError, or %NULL
 *
 * Finishes acquiring control the desktop.
 *
 * Returns: whether acquiring control of the desktop was successful or not
 */
gboolean
bs_desktop_controller_acquire_finish (BsDesktopController  *self,
                                      GAsyncResult         *result,
                                      GError              **error)
{
  g_return_val_if_fail (BS_IS_DESKTOP_CONTROLLER (self), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  g_return_val_if_fail (g_task_is_valid (result, self), FALSE);
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == bs_desktop_controller_acquire, FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

/**
 * bs_desktop_controller_release:
 * @self: an #BsDesktopController
 *
 * Releases control of the desktop. Each action must call it
 * exactly once, whenever it's not necessary to control the
 * desktop anymore.
 *
 * This function must only be called after successfully acquiring
 * control of the desktop.
 */
void
bs_desktop_controller_release (BsDesktopController *self)
{
  g_return_if_fail (BS_IS_DESKTOP_CONTROLLER (self));

  if (self->use_count == 0)
    {
      g_critical ("Trying to release desktop controller but the "
                  "release count is zero. Did you call release too much?");
      return;
    }

  self->use_count--;

  if (self->use_count == 0 && self->session)
    {
      g_debug ("Releasing desktop controller session");

      xdp_session_close (self->session);
      g_clear_object (&self->session);
    }
}

/**
 * bs_desktop_controller_press_key:
 * @self: an #BsDesktopController
 * @keysym: a keysym
 *
 * Emulates a key press on the desktop.
 */
void
bs_desktop_controller_press_key (BsDesktopController *self,
                                 int                  keysym)
{
  g_return_if_fail (BS_IS_DESKTOP_CONTROLLER (self));
  g_return_if_fail (XDP_IS_SESSION (self->session));

  xdp_session_keyboard_key (self->session, TRUE, keysym, XDP_KEY_PRESSED);
}

/**
 * bs_desktop_controller_release_key:
 * @self: an #BsDesktopController
 * @keysym: a keysym
 *
 * Emulates a key release on the desktop.
 */
void
bs_desktop_controller_release_key (BsDesktopController *self,
                                   int                  keysym)
{
  g_return_if_fail (BS_IS_DESKTOP_CONTROLLER (self));
  g_return_if_fail (XDP_IS_SESSION (self->session));

  xdp_session_keyboard_key (self->session, TRUE, keysym, XDP_KEY_RELEASED);
}
