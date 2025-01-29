/*
 * bs-device-update.c
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

#define G_LOG_DOMAIN "Update"

#include "bs-device-update.h"

#include "bs-button-private.h"
#include "bs-debug.h"
#include "bs-touchscreen-private.h"

struct _BsDeviceUpdate
{
  GObject parent_instance;

  gboolean sealed;

  GPtrArray *button_updates;
  GPtrArray *touchscreen_updates;
};

G_DEFINE_FINAL_TYPE (BsDeviceUpdate, bs_device_update, G_TYPE_OBJECT)


/*
 * BsButtonUpdate
 */

static void
bs_button_update_free (BsButtonUpdate *button_update)
{
  g_clear_object (&button_update->button);
  g_clear_pointer (&button_update, g_free);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (BsButtonUpdate, bs_button_update_free)

static BsButtonUpdate *
bs_button_update_new (BsButton *button)
{
  g_autoptr (BsButtonUpdate) button_update = NULL;

  g_assert (BS_IS_BUTTON (button));

  button_update = g_new0 (BsButtonUpdate, 1);
  button_update->button = g_object_ref (button);

  return g_steal_pointer (&button_update);
}


/*
 * BsTouchscreenUpdate
 */

static void
bs_touchscreen_update_free (BsTouchscreenUpdate *touchscreen_update)
{
  g_clear_object (&touchscreen_update->touchscreen);
  g_clear_pointer (&touchscreen_update, g_free);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (BsTouchscreenUpdate, bs_touchscreen_update_free)

static BsTouchscreenUpdate *
bs_touchscreen_update_new (BsTouchscreen         *touchscreen,
                           const graphene_rect_t *region)
{
  g_autoptr (BsTouchscreenUpdate) touchscreen_update = NULL;

  g_assert (BS_IS_TOUCHSCREEN (touchscreen));
  g_assert (region != NULL);

  touchscreen_update = g_new0 (BsTouchscreenUpdate, 1);
  touchscreen_update->touchscreen = g_object_ref (touchscreen);
  graphene_rect_init_from_rect (&touchscreen_update->region, region);

  return g_steal_pointer (&touchscreen_update);
}


/*
 * GObject overrides
 */

static void
bs_device_update_finalize (GObject *object)
{
  BsDeviceUpdate *self = (BsDeviceUpdate *)object;

  g_clear_pointer (&self->button_updates, g_ptr_array_unref);
  g_clear_pointer (&self->touchscreen_updates, g_ptr_array_unref);

  G_OBJECT_CLASS (bs_device_update_parent_class)->finalize (object);
}

static void
bs_device_update_class_init (BsDeviceUpdateClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = bs_device_update_finalize;
}

static void
bs_device_update_init (BsDeviceUpdate *self)
{
  self->button_updates = g_ptr_array_new_null_terminated (1, (GDestroyNotify) bs_button_update_free, TRUE);
  self->touchscreen_updates = g_ptr_array_new_null_terminated (1, (GDestroyNotify) bs_touchscreen_update_free, TRUE);
}

BsDeviceUpdate *
bs_device_update_new (void)
{
  return g_object_new (BS_TYPE_DEVICE_UPDATE, NULL);
}

void
bs_device_update_add_button (BsDeviceUpdate *self,
                             BsButton       *button)
{
  g_assert (BS_IS_DEVICE_UPDATE (self));
  g_assert (BS_IS_BUTTON (button));
  g_assert (!self->sealed);

  for (size_t i = 0; i < self->button_updates->len; i++)
    {
      BsButtonUpdate *aux = g_ptr_array_index (self->button_updates, i);

      if (aux->button == button)
        return;
    }

  g_ptr_array_add (self->button_updates, bs_button_update_new (button));
}

void
bs_device_update_add_touchscreen_region (BsDeviceUpdate        *self,
                                         BsTouchscreen         *touchscreen,
                                         const graphene_rect_t *region)
{
  g_assert (BS_IS_DEVICE_UPDATE (self));
  g_assert (BS_IS_TOUCHSCREEN (touchscreen));
  g_assert (region != NULL);
  g_assert (!self->sealed);

  for (size_t i = 0; i < self->touchscreen_updates->len; i++)
    {
      BsTouchscreenUpdate *aux = g_ptr_array_index (self->touchscreen_updates, i);

      if (aux->touchscreen == touchscreen)
        {
          graphene_rect_union (&aux->region, region, &aux->region);
          return;
        }
    }

  g_ptr_array_add (self->touchscreen_updates, bs_touchscreen_update_new (touchscreen, region));
}

void
bs_device_update_seal (BsDeviceUpdate *self)
{
  g_assert (BS_IS_DEVICE_UPDATE (self));
  g_assert (!self->sealed);

  self->sealed = TRUE;
}

BsButtonUpdate **
bs_device_update_get_button_updates (BsDeviceUpdate *self)
{
  g_assert (BS_IS_DEVICE_UPDATE (self));
  g_assert (self->sealed);

  return (BsButtonUpdate **) self->button_updates->pdata;
}

BsTouchscreenUpdate **
bs_device_update_get_touchscreen_updates (BsDeviceUpdate *self)
{
  g_assert (BS_IS_DEVICE_UPDATE (self));
  g_assert (self->sealed);

  return (BsTouchscreenUpdate **) self->touchscreen_updates->pdata;
}
