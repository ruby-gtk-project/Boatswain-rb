/* bs-application.c
 *
 * Copyright 2022 Georges Basile Stavracas Neto
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
 */

#include "bs-application.h"
#include "bs-config.h"
#include "bs-device-manager.h"
#include "bs-window.h"

#include <glib/gi18n.h>
#include <glib-unix.h>
#include <libportal/portal.h>
#include <libportal-gtk4/portal-gtk4.h>

struct _BsApplication
{
  AdwApplication parent_instance;

  GtkWindow *window;

  XdpPortal *portal;
};

static void on_request_background_called_cb (GObject      *object,
                                             GAsyncResult *result,
                                             gpointer      user_data);

G_DEFINE_TYPE (BsApplication, bs_application, ADW_TYPE_APPLICATION)

static GOptionEntry bs_application_options[] = {
  {
    "debug", 0, 0,
    G_OPTION_ARG_NONE, NULL,
    N_("Enable debug messages"), NULL
  },
  { NULL }
};


/*
 * Callbacks
 */

static void
on_background_status_set_cb (GObject      *object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  g_autoptr (GError) error = NULL;

  xdp_portal_set_background_status_finish (XDP_PORTAL (object), result, &error);

  if (error)
    g_warning ("Error requesting background: %s", error->message);
}

static void
on_device_manager_items_changed_cb (GListModel    *model,
                                    unsigned int   position,
                                    unsigned int   removed,
                                    unsigned int   added,
                                    BsApplication *self)
{
  g_autofree char *message = NULL;
  uint32_t n_devices;

  n_devices = g_list_model_get_n_items (model);

  /* Translators: "device" refers to Elgato Stream Deck devices */
  message = g_strdup_printf (g_dngettext (GETTEXT_PACKAGE,
                                          "Connected to %d device",
                                          "Connected to %d devices",
                                          n_devices),
                             n_devices);

  xdp_portal_set_background_status (self->portal,
                                    message,
                                    NULL,
                                    on_background_status_set_cb,
                                    NULL);
}

static gboolean
on_unix_signal_cb (GApplication *application)
{
  g_application_quit (application);
  return G_SOURCE_REMOVE;
}

static void
on_request_background_called_cb (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  BsApplication *self = BS_APPLICATION (user_data);
  g_autoptr (GError) error = NULL;

  xdp_portal_request_background_finish (XDP_PORTAL (object), result, &error);

  if (!error)
    g_application_hold (G_APPLICATION (self));
  else
    g_warning ("Error requesting background: %s", error->message);

  g_idle_add_once ((GSourceOnceFunc) gtk_window_destroy, self->window);
  self->window = NULL;
}

static gboolean
on_window_close_requested_cb (GtkWindow     *window,
                              BsApplication *self)
{
  g_autoptr (XdpParent) parent = NULL;
  XdpBackgroundFlags background_flags;

  g_assert (self->window != NULL);

  parent = xdp_parent_new_gtk (self->window);
  background_flags = XDP_BACKGROUND_FLAG_ACTIVATABLE;

  xdp_portal_request_background (self->portal,
                                 parent,
                                 _("Boatswain needs to run in background to detect and execute Stream Deck actions."),
                                 NULL,
                                 background_flags,
                                 NULL,
                                 on_request_background_called_cb,
                                 self);

  return TRUE;
}


/*
 * GApplication overrides
 */

static void
bs_application_startup (GApplication *application)
{
  g_autoptr (GError) error = NULL;
  AdwStyleManager *style_manager;
  BsApplication *self;
  BsContext *context;

  self = BS_APPLICATION (application);

  /* Add legacy gdk-pixbuf loaders to the search path */
  gdk_pixbuf_init_modules ("/app/lib/gdk-pixbuf-2.0/2.10.0", NULL);

  G_APPLICATION_CLASS (bs_application_parent_class)->startup (application);

  bs_init ();

  self->portal = xdp_portal_new ();

  context = bs_context_get_default ();
  g_signal_connect (bs_context_get_devices (context),
                    "items-changed",
                    G_CALLBACK (on_device_manager_items_changed_cb),
                    self);

  style_manager = adw_application_get_style_manager (ADW_APPLICATION (application));
  adw_style_manager_set_color_scheme (style_manager, ADW_COLOR_SCHEME_PREFER_DARK);
}

static void
bs_application_activate (GApplication *application)
{
  BsApplication *self = BS_APPLICATION (application);

  if (!self->window)
    {
      self->window = g_object_new (BS_TYPE_WINDOW,
                                   "application", application,
                                   NULL);
      g_signal_connect (self->window, "close-request", G_CALLBACK (on_window_close_requested_cb), self);
    }

  gtk_window_present (self->window);
}

static void
bs_application_shutdown (GApplication *application)
{
  BsApplication *self = BS_APPLICATION (application);

  g_clear_pointer (&self->window, gtk_window_destroy);
  g_clear_object (&self->portal);

  bs_shutdown ();

  G_APPLICATION_CLASS (bs_application_parent_class)->shutdown (application);
}


/*
 * GObject overrides
 */

static void
bs_application_finalize (GObject *object)
{
  BsApplication *self = (BsApplication *)object;

  g_clear_object (&self->portal);

  G_OBJECT_CLASS (bs_application_parent_class)->finalize (object);
}

static void
bs_application_class_init (BsApplicationClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GApplicationClass *app_class = G_APPLICATION_CLASS (klass);

  object_class->finalize = bs_application_finalize;

  app_class->startup = bs_application_startup;
  app_class->activate = bs_application_activate;
  app_class->shutdown = bs_application_shutdown;
}

static void
bs_application_init (BsApplication *self)
{
  g_autoptr (GSimpleAction) quit_action = g_simple_action_new ("quit", NULL);
  g_signal_connect_swapped (quit_action, "activate", G_CALLBACK (g_application_quit), self);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (quit_action));

  gtk_application_set_accels_for_action (GTK_APPLICATION (self),
                                         "app.quit",
                                         (const char *[]) {
                                           "<primary>q",
                                           NULL,
                                         });

  g_application_add_main_option_entries (G_APPLICATION (self), bs_application_options);

  g_unix_signal_add (SIGHUP, (GSourceFunc) on_unix_signal_cb, self);
  g_unix_signal_add (SIGINT, (GSourceFunc) on_unix_signal_cb, self);
  g_unix_signal_add (SIGTERM, (GSourceFunc) on_unix_signal_cb, self);
  g_unix_signal_add (SIGUSR1, (GSourceFunc) on_unix_signal_cb, self);
  g_unix_signal_add (SIGUSR2, (GSourceFunc) on_unix_signal_cb, self);
}

BsApplication *
bs_application_new (void)
{
  return g_object_new (BS_TYPE_APPLICATION,
                       "application-id", APPLICATION_ID,
                       "flags", G_APPLICATION_DEFAULT_FLAGS,
                       "resource-base-path", "/com/feaneron/Boatswain",
                       NULL);
}
