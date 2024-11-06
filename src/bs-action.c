/* bs-action.c
 *
 * Copyright 2022 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#define G_LOG_DOMAIN "Action"

#include "bs-action-private.h"

#include "bs-debug.h"
#include "bs-events.h"
#include "bs-icon.h"
#include "bs-button.h"

typedef struct
{
  char *id;
  char *name;
  BsActionFactory *factory;
  BsIcon *icon;
  BsButton *button;
} BsActionPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (BsAction, bs_action, G_TYPE_OBJECT)

enum
{
  PROP_0,
  PROP_BUTTON,
  N_PROPS,
};

enum
{
  CHANGED,
  N_SIGNALS,
};

static guint signals[N_SIGNALS];
static GParamSpec* properties[N_PROPS];


/*
 * BsAction overrides
 */

static BsIcon *
bs_action_real_get_icon (BsAction *self)
{
  BsActionPrivate *priv = bs_action_get_instance_private (self);

  return priv->icon;
}

/*
 * GObject overrides
 */

static void
bs_action_finalize (GObject *object)
{
  BsAction *self = (BsAction *)object;
  BsActionPrivate *priv = bs_action_get_instance_private (self);

  BS_ENTRY;

  BS_TRACE_MSG ("Finalizing %s", G_OBJECT_TYPE_NAME (object));

  g_clear_object (&priv->icon);
  g_clear_pointer (&priv->id, g_free);
  g_clear_pointer (&priv->name, g_free);

  G_OBJECT_CLASS (bs_action_parent_class)->finalize (object);

  BS_EXIT;
}

static void
bs_action_get_property (GObject    *object,
                        guint       prop_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  BsAction *self = BS_ACTION (object);
  BsActionPrivate *priv = bs_action_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_BUTTON:
      g_value_set_object (value, priv->button);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bs_action_set_property (GObject      *object,
                        guint         prop_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  BsAction *self = BS_ACTION (object);
  BsActionPrivate *priv = bs_action_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_BUTTON:
      g_assert (priv->button == NULL);
      priv->button = g_value_get_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
bs_action_class_init (BsActionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = bs_action_finalize;
  object_class->get_property = bs_action_get_property;
  object_class->set_property = bs_action_set_property;

  klass->get_icon = bs_action_real_get_icon;

  properties[PROP_BUTTON] = g_param_spec_object ("button", NULL, NULL,
                                                 BS_TYPE_BUTTON,
                                                 G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals[CHANGED] = g_signal_new ("changed",
                                   BS_TYPE_ACTION,
                                   G_SIGNAL_RUN_LAST,
                                   0, NULL, NULL, NULL,
                                   G_TYPE_NONE,
                                   0);
}

static void
bs_action_init (BsAction *self)
{
  BsActionPrivate *priv = bs_action_get_instance_private (self);

  priv->icon = bs_icon_new_empty ();
}

BsActionFactory *
bs_action_get_factory (BsAction *self)
{
  BsActionPrivate *priv;

  g_return_val_if_fail (BS_IS_ACTION (self), NULL);

  priv = bs_action_get_instance_private (self);
  return priv->factory;
}

void
bs_action_set_factory (BsAction        *self,
                       BsActionFactory *factory)
{

  BsActionPrivate *priv = bs_action_get_instance_private (self);

  g_return_if_fail (BS_IS_ACTION (self));
  g_return_if_fail (factory != NULL);
  g_return_if_fail (priv->factory == NULL);

  priv->factory = factory;
}

/**
 * bs_action_get_id:
 * @self: a #BsAction
 *
 * Retrieves the action id.
 *
 * Returns: (transfer none): id of @self
 */
const char *
bs_action_get_id (BsAction *self)
{
  BsActionPrivate *priv;

  g_return_val_if_fail (BS_IS_ACTION (self), NULL);

  priv = bs_action_get_instance_private (self);
  return priv->id;
}

void
bs_action_set_id (BsAction   *self,
                  const char *id)
{
  BsActionPrivate *priv = bs_action_get_instance_private (self);

  g_return_if_fail (BS_IS_ACTION (self));
  g_return_if_fail (id != NULL);
  g_return_if_fail (priv->id == NULL);

  priv->id = g_strdup (id);
}

const char *
bs_action_get_name (BsAction *self)
{
  BsActionPrivate *priv;

  g_return_val_if_fail (BS_IS_ACTION (self), NULL);

  priv = bs_action_get_instance_private (self);
  return priv->name;
}

void
bs_action_set_name (BsAction   *self,
                    const char *name)
{
  BsActionPrivate *priv = bs_action_get_instance_private (self);

  g_return_if_fail (BS_IS_ACTION (self));
  g_return_if_fail (name != NULL);
  g_return_if_fail (priv->name == NULL);

  priv->name = g_strdup (name);
}

/**
 * bs_action_get_icon:
 * @self: a #BsAction
 *
 * Retrieves the #BsIcon of @self.
 *
 * Returns: (transfer none)(nullable): a #BsIcon
 */
BsIcon *
bs_action_get_icon (BsAction *self)
{
  g_return_val_if_fail (BS_IS_ACTION (self), NULL);
  g_return_val_if_fail (BS_ACTION_GET_CLASS (self)->get_icon, NULL);

  return BS_ACTION_GET_CLASS (self)->get_icon (self);
}

/**
 * bs_action_get_preferences:
 * @self: a #BsAction
 *
 * Retrieves the preferences controls of @self.
 *
 * Returns: (transfer none)(nullable): a #GtkWidget
 */
GtkWidget *
bs_action_get_preferences (BsAction *self)
{
  g_return_val_if_fail (BS_IS_ACTION (self), NULL);

  if (BS_ACTION_GET_CLASS (self)->get_preferences)
    return BS_ACTION_GET_CLASS (self)->get_preferences (self);
  else
    return NULL;
}

JsonNode *
bs_action_serialize_settings (BsAction *self)
{
  g_return_val_if_fail (BS_IS_ACTION (self), NULL);

  BS_ENTRY;

  g_debug ("Serializing settings of %s", G_OBJECT_TYPE_NAME (self));

  if (BS_ACTION_GET_CLASS (self)->serialize_settings)
    {
      g_autoptr (JsonNode) node = NULL;

      node = BS_ACTION_GET_CLASS (self)->serialize_settings (self);

      if (!JSON_NODE_HOLDS_OBJECT (node))
        {
          g_warning ("Serialized action settings must be JsonObjects");
          BS_RETURN (NULL);
        }

      BS_RETURN (g_steal_pointer (&node));
    }
  else
    {
      BS_RETURN (NULL);
    }
}

void
bs_action_deserialize_settings (BsAction   *self,
                                JsonObject *settings)
{
  g_return_if_fail (BS_IS_ACTION (self));
  g_return_if_fail (settings != NULL);

  BS_ENTRY;

  g_debug ("Deserializing settings of %s", G_OBJECT_TYPE_NAME (self));

  if (BS_ACTION_GET_CLASS (self)->deserialize_settings)
    BS_ACTION_GET_CLASS (self)->deserialize_settings (self, settings);

  BS_EXIT;
}

/**
 * bs_action_get_button:
 * @self: a #BsAction
 *
 * Retrieves the #BsButton that @self is attached
 * to, or %NULL if it's not attached to any physical button.
 *
 * Returns: (transfer none)(nullable): a #BsButton
 */
BsButton *
bs_action_get_button (BsAction *self)
{
  BsActionPrivate *priv;

  g_return_val_if_fail (BS_IS_ACTION (self), NULL);

  priv = bs_action_get_instance_private (self);
  return priv->button;
}

/**
 * bs_action_changed:
 * @self: a #BsAction
 *
 * Emits the 'changed' signal.
 */
void
bs_action_changed (BsAction *self)
{
  g_return_if_fail (BS_IS_ACTION (self));

  g_signal_emit (self, signals[CHANGED], 0);
}

void
bs_action_handle_event (BsAction *self,
                        BsEvent  *event)
{
  g_return_if_fail (BS_IS_ACTION (self));
  g_return_if_fail (BS_IS_EVENT (event));

  BS_ENTRY;

  g_debug ("Action %s (%p) handling event %s",
           G_OBJECT_TYPE_NAME (self),
           self,
           G_OBJECT_TYPE_NAME (event));

  if (BS_ACTION_GET_CLASS (self)->handle_event)
    BS_ACTION_GET_CLASS (self)->handle_event (self, event);
}
