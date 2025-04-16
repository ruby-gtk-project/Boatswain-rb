/*
 * mock-device.h
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

#pragma once

#include "bs-device-private.h"

G_BEGIN_DECLS

typedef enum
{
  MOCK_DEVICE_MODEL_HANGAR_MILD,
  MOCK_DEVICE_MODEL_HANGAR_XL,
} MockDeviceModel;

#define MOCK_TYPE_DEVICE (mock_device_get_type())
G_DECLARE_FINAL_TYPE (MockDevice, mock_device, MOCK, DEVICE, BsDevice)

BsDevice * mock_device_new (MockDeviceModel model);

G_END_DECLS
