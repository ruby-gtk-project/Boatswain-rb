/*
 * bs-types.h
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

#include <glib.h>

G_BEGIN_DECLS

#define BS_DECLARE_INTERNAL_TYPE(ModuleObjName, module_obj_name, MODULE, OBJ_NAME, ParentName)        \
  GType module_obj_name##_get_type (void);                                                            \
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS                                                                    \
  typedef struct _##ModuleObjName ModuleObjName;                                                      \
  typedef struct _##ModuleObjName##Class ModuleObjName##Class;                                        \
                                                                                                      \
  _GLIB_DEFINE_AUTOPTR_CHAINUP (ModuleObjName, ParentName)                                            \
  G_DEFINE_AUTOPTR_CLEANUP_FUNC (ModuleObjName##Class, g_type_class_unref)                            \
                                                                                                      \
  G_GNUC_UNUSED static inline ModuleObjName * MODULE##_##OBJ_NAME (gpointer ptr) {                    \
    return G_TYPE_CHECK_INSTANCE_CAST (ptr, module_obj_name##_get_type (), ModuleObjName); }          \
  G_GNUC_UNUSED static inline ModuleObjName##Class * MODULE##_##OBJ_NAME##_CLASS (gpointer ptr) {     \
    return G_TYPE_CHECK_CLASS_CAST (ptr, module_obj_name##_get_type (), ModuleObjName##Class); }      \
  G_GNUC_UNUSED static inline gboolean MODULE##_IS_##OBJ_NAME (gpointer ptr) {                        \
    return G_TYPE_CHECK_INSTANCE_TYPE (ptr, module_obj_name##_get_type ()); }                         \
  G_GNUC_UNUSED static inline gboolean MODULE##_IS_##OBJ_NAME##_CLASS (gpointer ptr) {                \
    return G_TYPE_CHECK_CLASS_TYPE (ptr, module_obj_name##_get_type ()); }                            \
  G_GNUC_UNUSED static inline ModuleObjName##Class * MODULE##_##OBJ_NAME##_GET_CLASS (gpointer ptr) { \
    return G_TYPE_INSTANCE_GET_CLASS (ptr, module_obj_name##_get_type (), ModuleObjName##Class); }    \
  G_GNUC_END_IGNORE_DEPRECATIONS

typedef enum _BsImageFormat BsImageFormat;
typedef enum _BsRendererFlags BsRendererFlags;

typedef struct _BsActionable BsActionable;
typedef struct _BsAction BsAction;
typedef struct _BsActionFactory BsActionFactory;
typedef struct _BsActionInfo BsActionInfo;
typedef struct _BsButton BsButton;
typedef struct _BsButtonEvent BsButtonEvent;
typedef struct _BsButtonGrid BsButtonGrid;
typedef struct _BsCursorEvent BsCursorEvent;
typedef struct _BsContext BsContext;
typedef struct _BsDesktopController BsDesktopController;
typedef struct _BsDevice BsDevice;
typedef struct _BsDeviceManager BsDeviceManager;
typedef struct _BsDeviceRegion BsDeviceRegion;
typedef struct _BsDial BsDial;
typedef struct _BsDialGrid BsDialGrid;
typedef struct _BsDialEvent BsDialEvent;
typedef struct _BsEmptyAction BsEmptyAction;
typedef struct _BsEvent BsEvent;
typedef struct _BsIcon BsIcon;
typedef struct _BsImageInfo BsImageInfo;
typedef struct _BsPage BsPage;
typedef struct _BsProfile BsProfile;
typedef struct _BsRenderer BsRenderer;
typedef struct _BsSelectionController BsSelectionController;
typedef struct _BsTouchscreen BsTouchscreen;
typedef struct _BsTouchscreenEvent BsTouchscreenEvent;
typedef struct _BsTouchscreenContent BsTouchscreenContent;
typedef struct _BsTouchscreenSlot BsTouchscreenSlot;

G_END_DECLS
