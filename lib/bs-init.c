/*
 * bs-init.c
 *
 * Copyright 2025 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#include "bs-context-private.h"
#include "bs-events-private.h"
#include "bs-debug.h"
#include "bs-init.h"
#include "bs-log-private.h"
#include "bs-macros.h"

#include <libdex.h>

void
bs_init (void)
{
  dex_init ();

  bs_log_init ();
  bs_event_init_types_once ();
  /* FIXME: propagate error */
  bs_context_init_default (NULL);
}

static DexFuture *
bs_shutdown_fiber (gpointer data G_GNUC_UNUSED)
{
  BS_ENTRY;

  g_assert (BS_IS_MAIN_THREAD ());

  bs_context_shutdown ();

  BS_RETURN (NULL);
}

static DexFuture *
bs_shutdown_cb (DexFuture *future,
                gpointer   data)
{
  g_main_loop_quit ((GMainLoop *)data);

  return NULL;
}

void
bs_shutdown (void)
{
  g_autoptr (GMainLoop) loop = NULL;
  g_autoptr (DexFuture) future = NULL;

  BS_ENTRY;

  loop = g_main_loop_new (NULL, FALSE);
  future = dex_future_finally (dex_scheduler_spawn (NULL,
                                                    0,
                                                    bs_shutdown_fiber,
                                                    NULL,
                                                    NULL),
                               bs_shutdown_cb,
                               g_main_loop_ref (loop),
                               (GDestroyNotify)g_main_loop_unref);
  g_main_loop_run (loop);

  BS_EXIT;
}
