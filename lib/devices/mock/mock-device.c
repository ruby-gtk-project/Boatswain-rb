/*
 * mock-device.c
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

#include "bs-config.h"
#include "bs-device-layout-builder-private.h"
#include "bs-renderer-private.h"
#include "mock-device.h"

#include <glib/gi18n.h>

struct _MockDevice
{
  BsDevice parent_instance;

  MockDeviceModel model;

  char *serial_number;
  const char *name;
};

G_DEFINE_FINAL_TYPE (MockDevice, mock_device, BS_TYPE_DEVICE)

enum {
  PROP_0,
  PROP_MODEL,
  N_PROPS,
};

static GParamSpec *properties [N_PROPS];

/*
 * Fake device for testing
 */

static char *
get_serial_number_fake (MockDevice *self)
{
  switch (self->model)
    {
    case MOCK_DEVICE_MODEL_HANGAR_MILD:
      {
        static unsigned int counter = 0;
        return g_strdup_printf ("mild-%u", counter++);
      }

    case MOCK_DEVICE_MODEL_HANGAR_XL:
      {
        static unsigned int counter = 0;
        return g_strdup_printf ("xl-%u", counter++);
      }

    default:
      g_assert_not_reached ();
    }
}

typedef struct
{
  uint8_t product_id;
  const char *name;
  enum {
    MOCK_DEVICE_FEATURE_BUTTONS = 1 << 0,
    MOCK_DEVICE_FEATURE_TOUCHSCREEN = 1 << 1,
    MOCK_DEVICE_FEATURE_DIALS = 1 << 2,
  } features;

  struct {
    uint8_t n_buttons;
    uint8_t columns;
    BsImageInfo image_info;
  } button_layout;

  struct {
      uint32_t n_slots;
      BsImageInfo image_info;
  } touchscreen_layout;

  struct {
    uint8_t n_dials;
    uint8_t columns;
  } dial_layout;

  char * (*get_serial_number) (MockDevice *self);
  char * (*get_firmware_version) (MockDevice *self);
} MockDeviceModelInfo;

static const MockDeviceModelInfo models_vtable[] = {
  [MOCK_DEVICE_MODEL_HANGAR_MILD] = {
    .product_id = 0x0001,
    .name = N_("Feaneron Hangar Mild"),
    .features = MOCK_DEVICE_FEATURE_BUTTONS,
    .button_layout = {
      .n_buttons = 15,
      .columns = 5,
      .image_info = {
        .width = 72,
        .height = 72,
        .format = BS_IMAGE_FORMAT_JPEG,
        .flags = BS_RENDERER_FLAG_FLIP_X | BS_RENDERER_FLAG_FLIP_Y,
      },
    },
    .get_serial_number = get_serial_number_fake,
  },

  [MOCK_DEVICE_MODEL_HANGAR_XL] = {
    .product_id = 0x0001,
    .name = N_("Feaneron Hangar XL"),
    .features = MOCK_DEVICE_FEATURE_BUTTONS,
    .button_layout = {
      .n_buttons = 32,
      .columns = 8,
      .image_info = {
        .width = 96,
        .height = 96,
        .format = BS_IMAGE_FORMAT_JPEG,
        .flags = BS_RENDERER_FLAG_FLIP_X | BS_RENDERER_FLAG_FLIP_Y,
      },
    },
    .get_serial_number = get_serial_number_fake,
  },
};


/*
 * BsDevice overrides
 */

static GListModel *
mock_device_create_layout (BsDevice *device)
{
  g_autoptr (BsDeviceLayoutBuilder) layout_builder = NULL;
  const MockDeviceModelInfo *model_info;
  MockDevice *self;
  unsigned int row = 0;

  self = (MockDevice *) device;
  g_assert (MOCK_IS_DEVICE (self));

  model_info = &models_vtable[self->model];

  layout_builder = bs_device_layout_builder_new (device);

  if (model_info->features & MOCK_DEVICE_FEATURE_BUTTONS)
    {
      bs_device_layout_builder_add_button_grid (layout_builder,
                                                "main-button-grid",
                                                &model_info->button_layout.image_info,
                                                model_info->button_layout.n_buttons,
                                                model_info->button_layout.columns,
                                                0, row++, 1, 1);
    }

  if (model_info->features & MOCK_DEVICE_FEATURE_TOUCHSCREEN)
    {
      bs_device_layout_builder_add_touchscreen (layout_builder,
                                                "touchscreen",
                                                &model_info->touchscreen_layout.image_info,
                                                model_info->touchscreen_layout.n_slots,
                                                0, row++, 1, 1);
    }

  if (model_info->features & MOCK_DEVICE_FEATURE_DIALS)
    {
      bs_device_layout_builder_add_dial_grid (layout_builder,
                                              "dial-grid",
                                              model_info->dial_layout.n_dials,
                                              model_info->dial_layout.columns,
                                              0, row++, 1, 1);
    }

  return bs_device_layout_builder_build (layout_builder);
}

static const char *
mock_device_get_firmware_version (BsDevice *device)
{
  MockDevice *self = (MockDevice *) device;

  g_assert (MOCK_IS_DEVICE (self));

  return PACKAGE_VERSION;
}

static const char *
mock_device_get_name (BsDevice *device)
{
  MockDevice *self = (MockDevice *) device;

  g_assert (MOCK_IS_DEVICE (self));

  return self->name;
}

static const char *
mock_device_get_serial_number (BsDevice *device)
{
  MockDevice *self = (MockDevice *) device;

  g_assert (MOCK_IS_DEVICE (self));

  return self->serial_number;
}


/*
 * GObject overrides
 */

static void
mock_device_finalize (GObject *object)
{
  MockDevice *self = (MockDevice *)object;

  g_clear_pointer (&self->serial_number, g_free);

  G_OBJECT_CLASS (mock_device_parent_class)->finalize (object);
}

static void
mock_device_constructed (GObject *object)
{
  const MockDeviceModelInfo *model_info;
  MockDevice *self = (MockDevice *)object;

  G_OBJECT_CLASS (mock_device_parent_class)->constructed (object);

  model_info = &models_vtable[self->model];

  self->serial_number = model_info->get_serial_number (self);
  self->name = _(model_info->name);
}

static void
mock_stream_deck_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  MockDevice *self = MOCK_DEVICE (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      g_value_set_int (value, self->model);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mock_stream_deck_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  MockDevice *self = MOCK_DEVICE (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      self->model = g_value_get_int (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mock_device_class_init (MockDeviceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  BsDeviceClass *device_class = BS_DEVICE_CLASS (klass);

  object_class->finalize = mock_device_finalize;
  object_class->constructed = mock_device_constructed;
  object_class->get_property = mock_stream_deck_get_property;
  object_class->set_property = mock_stream_deck_set_property;

  device_class->create_layout = mock_device_create_layout;
  device_class->get_firmware_version = mock_device_get_firmware_version;
  device_class->get_name = mock_device_get_name;
  device_class->get_serial_number = mock_device_get_serial_number;

  properties[PROP_MODEL] = g_param_spec_int ("model", NULL, NULL,
                                             MOCK_DEVICE_MODEL_HANGAR_MILD,
                                             MOCK_DEVICE_MODEL_HANGAR_XL,
                                             MOCK_DEVICE_MODEL_HANGAR_MILD,
                                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
mock_device_init (MockDevice *self)
{
  self->model = MOCK_DEVICE_MODEL_HANGAR_MILD;
}

BsDevice *
mock_device_new (MockDeviceModel model)
{
  return g_initable_new (MOCK_TYPE_DEVICE,
                         NULL,
                         NULL,
                         "model", model,
                         NULL);
}
