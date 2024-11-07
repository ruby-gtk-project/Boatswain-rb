/*
 * bs-touchscreen-slot.c
 *
 * Copyright 2024 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#include "bs-touchscreen-slot.h"

#include "bs-actionable-private.h"
#include "bs-action.h"
#include "bs-touchscreen.h"

#include <graphene.h>

struct _BsTouchscreenSlot
{
  GObject parent_instance;

  graphene_size_t size;

  BsAction *action; /* (transfer full)(nullable) */
  BsTouchscreen *touchscreen; /* (transfer none) */

  gulong content_invalidated_id;
};

static void bs_actionable_interface_init (BsActionableInterface *iface);
static void gdk_paintable_interface_init (GdkPaintableInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (BsTouchscreenSlot, bs_touchscreen_slot, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (BS_TYPE_ACTIONABLE, bs_actionable_interface_init)
                               G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE, gdk_paintable_interface_init))

enum {
  PROP_0,
  PROP_TOUCHSCREEN,
  N_PROPS,

  /* Interface properties */
  PROP_ACTION,
};

static GParamSpec *properties [N_PROPS];


/*
 * Callbacks
 */

static void
on_action_icon_invalidate_contents_cb (GdkPaintable         *paintable,
                                       BsTouchscreenContent *self)
{
  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
}


/*
 * GdkPaintable interface
 */

static void
bs_touchscreen_content_snapshot (GdkPaintable *paintable,
                                 GdkSnapshot  *snapshot,
                                 double        width,
                                 double        height)
{
  BsTouchscreenSlot *self = (BsTouchscreenSlot *) paintable;
  BsIcon *icon;

  g_assert (BS_IS_TOUCHSCREEN_SLOT (self));

  if (!self->action)
    return;

  icon = bs_action_get_icon (self->action);

  gdk_paintable_snapshot (GDK_PAINTABLE (icon), snapshot, width, height);
}

static int
bs_touchscreen_content_get_intrinsic_width (GdkPaintable *paintable)
{
  BsTouchscreenSlot *self = (BsTouchscreenSlot *) paintable;

  g_assert (BS_IS_TOUCHSCREEN_SLOT (self));

  return self->size.width;
}

static int
bs_touchscreen_content_get_intrinsic_height (GdkPaintable *paintable)
{
  BsTouchscreenSlot *self = (BsTouchscreenSlot *) paintable;

  g_assert (BS_IS_TOUCHSCREEN_SLOT (self));

  return self->size.height;
}

static GdkPaintableFlags
bs_touchscreen_content_get_flags (GdkPaintable *paintable)
{
  return GDK_PAINTABLE_STATIC_SIZE;
}

static void
gdk_paintable_interface_init (GdkPaintableInterface *iface)
{
  iface->snapshot = bs_touchscreen_content_snapshot;
  iface->get_intrinsic_width = bs_touchscreen_content_get_intrinsic_width;
  iface->get_intrinsic_height = bs_touchscreen_content_get_intrinsic_height;
  iface->get_flags = bs_touchscreen_content_get_flags;
}


/*
 * BsActionable interface
 */

static BsAction *
bs_touchscreen_slot_actionable_get_action (BsActionable *actionable)
{
  BsTouchscreenSlot *self = (BsTouchscreenSlot *) actionable;

  g_assert (BS_IS_TOUCHSCREEN_SLOT (actionable));

  return self->action;
}

static void
bs_touchscreen_slot_actionable_set_action (BsActionable *actionable,
                                           BsAction     *action)
{
  BsTouchscreenSlot *self = (BsTouchscreenSlot *) actionable;

  g_assert (BS_IS_TOUCHSCREEN_SLOT (actionable));

  if (self->action == action)
    return;

  if (self->action)
    g_clear_signal_handler (&self->content_invalidated_id, bs_action_get_icon (self->action));

  g_set_object (&self->action, action);

  if (self->action)
    {
      self->content_invalidated_id = g_signal_connect (bs_action_get_icon (self->action),
                                                       "invalidate-contents",
                                                       G_CALLBACK (on_action_icon_invalidate_contents_cb),
                                                       self);
    }

  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
  g_object_notify (G_OBJECT (self), "action");
}

static void
bs_actionable_interface_init (BsActionableInterface *iface)
{
  iface->get_action = bs_touchscreen_slot_actionable_get_action;
  iface->set_action = bs_touchscreen_slot_actionable_set_action;
}


/*
 * GObject overrides
 */

static void
bs_touchscreen_slot_finalize (GObject *object)
{
  BsTouchscreenSlot *self = (BsTouchscreenSlot *)object;

  if (self->action)
    g_clear_signal_handler (&self->content_invalidated_id, bs_action_get_icon (self->action));

  g_clear_object (&self->action);

  G_OBJECT_CLASS (bs_touchscreen_slot_parent_class)->finalize (object);
}

static void
bs_touchscreen_slot_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  BsTouchscreenSlot *self = BS_TOUCHSCREEN_SLOT (object);

  switch (prop_id)
    {
    case PROP_ACTION:
      g_value_set_object (value, self->action);
      break;

    case PROP_TOUCHSCREEN:
      g_value_set_object (value, self->touchscreen);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bs_touchscreen_slot_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  BsTouchscreenSlot *self = BS_TOUCHSCREEN_SLOT (object);

  switch (prop_id)
    {
    case PROP_TOUCHSCREEN:
      g_assert (self->touchscreen == NULL);
      self->touchscreen = g_value_get_object (value);
      g_assert (BS_IS_TOUCHSCREEN (self->touchscreen));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bs_touchscreen_slot_class_init (BsTouchscreenSlotClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = bs_touchscreen_slot_finalize;
  object_class->get_property = bs_touchscreen_slot_get_property;
  object_class->set_property = bs_touchscreen_slot_set_property;

  properties[PROP_TOUCHSCREEN] = g_param_spec_object ("touchscreen", NULL, NULL,
                                                      BS_TYPE_TOUCHSCREEN,
                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  g_object_class_override_property (object_class, PROP_ACTION, "action");
}

static void
bs_touchscreen_slot_init (BsTouchscreenSlot *self)
{
}

BsTouchscreenSlot *
bs_touchscreen_slot_new (BsTouchscreen *touchscreen,
                         uint32_t       width,
                         uint32_t       height)
{
  g_autoptr (BsTouchscreenSlot) slot = NULL;

  slot = g_object_new (BS_TYPE_TOUCHSCREEN_SLOT,
                       "touchscreen", touchscreen,
                       NULL);
  slot->size = GRAPHENE_SIZE_INIT (width, height);

  return g_steal_pointer (&slot);
}

/**
 * bs_touchscreen_slot_get_touchscreen:
 *
 * Retrieves the #BsTouchscreen of @self.
 *
 * Returns: (transfer none): the #BsTouchscreen this slot belongs to
 */
BsTouchscreen *
bs_touchscreen_slot_get_touchscreen (BsTouchscreenSlot *self)
{
  g_return_val_if_fail (BS_IS_TOUCHSCREEN_SLOT (self), NULL);

  return self->touchscreen;
}
