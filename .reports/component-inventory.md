# Boatswain — upstream component inventory

Survey of `origin/main` (C/GTK4/Libadwaita + libpeas plugins, one GJS plugin)
against `lib/` on the `ruby` branch. Produced with the
`component-identification` skill; no app code was written in this step.

Upstream is read with `git show origin/main:<path>`. The port is written from
scratch against it as the spec.

## How upstream is laid out

- `src/` — the app: window, editors, dialogs, rows, widgets. Each component is
  **two files**, a `.c` with the callbacks and a `.ui` GtkBuilder template with
  the widget tree, CSS classes and signal handler names. There are **no
  blueprint files**; the 20 `.ui` templates under `src/` are the authoritative
  widget trees.
- `src/plugins/<name>/` — six action plugins (`default`, `desktop`, `launcher`,
  `network`, `obs-studio`, `soundboard`) plus one written in GJS (`gaming`).
  Each plugin has an action factory that registers `BsActionEntry` rows, and
  each action may return a preferences widget built **in C, not in a `.ui`** —
  those are under-reported by any scan and were read by hand.
- `lib/` — the device/HID layer (`BsDevice`, `BsProfile`, `BsPage`, `BsButton`,
  `BsDial`, `BsTouchscreen`, `BsIcon`, `BsRenderer`, and the Elgato / Loupedeck
  / mock device providers). Not user-facing widgets, but **nothing in `src/`
  renders without it**; see Blockers.
- `data/com.feaneron.Boatswain.gschema.xml` — one key only:
  `desktop-controller-restore-token` (string). There is no preferences dialog
  in this app.
- `src/boatswain.gresource.xml` — 15 resources (14 `.ui` + `style.css`), 19
  symbolic icons under `.../icons/scalable/emblems`, 5 touchscreen background
  PNGs under `.../assets/backgrounds`. Each plugin has its own
  `<name>.gresource.xml` with its icons.
- `po/` — 38 locales in `LINGUAS`, 39 `.po` files plus `boatswain.pot`.
  `POTFILES.in` lists 62 files, including 11 `.ui` files, meaning the port must
  keep the translatable strings that currently live in templates.

## What the port has today

`lib/` on `ruby` holds three files:

| Path | What it is |
|---|---|
| `lib/boatswain.rb` | entry point, `require_relative "boatswain/application"` |
| `lib/boatswain/application.rb` | `Gtk::Application` + `Adwaita::ApplicationWindow` + `Adwaita::ToolbarView` + `Adwaita::HeaderBar` + an `Adwaita::StatusPage` reading "The Ruby port has not been written yet." |
| `lib/boatswain/rb/version.rb` | gem version |

So **one** of the 52 components below exists, and only in placeholder form: the
application unit has its object, its window and a status page, but none of
upstream's actions, accelerators, About dialog or device wiring.

`spec/` holds only `spec_helper.rb` and a README — no component tests.

---

## Core app

### src/bs-application.c — the application object

| Widget / item | × | CSS | Signals / actions | Notes |
|---|---:|---|---|---|
| `Adw.Application` subclass | 1 | — | `startup`, `activate`, `shutdown` | `Adwaita::Application` is broken in the Ruby bindings — use `Gtk::Application` (see `ruby-gtk` skill) |
| action `app.quit` | 1 | — | `activate` → `g_application_quit` | accel `<primary>q` |
| colour scheme | — | — | — | `adw_style_manager_set_color_scheme (PREFER_DARK)` at startup |
| background portal | — | — | — | `xdp_portal_request_background` on window close-request; keeps the app held in the background and sets a "Connected to %d device(s)" background status |
| CLI option | — | — | — | `--debug`, "Enable debug messages" |
| UNIX signals | — | — | — | SIGHUP/INT/TERM/USR1/USR2 → quit |

Port target: `lib/boatswain/application.rb` — **exists (placeholder only)**.

### src/bs-window.c + src/bs-window.ui — the main window

| Widget | × | CSS | Signals / actions | Notes |
|---|---:|---|---|---|
| `Adw.ApplicationWindow` (template root) | 1 | `devel` (development profile only) | action `win.about`; binding `<primary>w` → `window.close` | title "Boatswain", 1200×720 |
| `Gtk.Stack` `main_stack` | 1 | — | — | two pages, `empty` and `devices` |
| `Adw.ToolbarView` | 1 | — | — | the `empty` page |
| `Adw.HeaderBar` | 2 | — | — | one on each page |
| `Gtk.MenuButton` (primary) | 2 | — | `menu-model` `primary_menu` | `open-menu-symbolic` |
| `Adw.StatusPage` `empty_page` | 1 | — | — | "No Stream Deck Found" / "Plug in a Stream Deck device to use it"; icon swaps to `…Boatswain.Devel` on the development profile |
| `Adw.NavigationSplitView` | 1 | — | — | the `devices` page |
| `Adw.NavigationPage` | 2 | — | — | tags `sidebar` (title "Devices") and `content` (title "Device") |
| `Gtk.MenuButton` `devices_menu_button` | 1 | `flat` | — | title widget of the sidebar header bar |
| `Adw.WindowTitle` | 1 | — | — | bound to `device.name` / `device.serial-number` |
| `Gtk.Image` | 1 | — | — | `pan-down-symbolic` |
| `Gtk.Popover` `devices_popover` | 1 | — | — | width-request 200 |
| `Gtk.ListBox` `devices_listbox` | 1 | `navigation-sidebar` | `row-activated` → `on_devices_listbox_row_activated_cb` | selection-mode none; `bind_model` over `BsContext.devices` |
| `Gtk.ListBoxRow` (device row, built in C) | *n* | child labels carry `caption` + `dim-label` | — | **repeater** over the device list: `Gtk.Box` vertical + two `Gtk.Label` (name, serial) |
| `Gtk.ScrolledWindow` | 1 | — | — | vscrollbar-policy never |
| `Gtk.ListBox` `profiles_listbox` | 1 | `navigation-sidebar` | `row-activated` → `on_profiles_listbox_row_activated_cb` | selection-mode browse; **repeater** of `BsProfileRow` via `create_profile_row_cb`, each wired to the row's `move` signal |
| `Gtk.Entry` `new_profile_name_entry` | 1 | `new-profile-entry` | `activate` → `on_new_profile_name_entry_activate_cb` | placeholder "New profile…" |
| `Gtk.Grid` (bottom bar) | 1 | — | — | rows 1 and 4 |
| `Gtk.Label` | 3 | `dim-label` (the two captions) | — | "Brightness", "Firmware", and `firmware_version_label` (selectable, xalign 1.0) |
| `Gtk.Scale` `brightness_scale` + `Gtk.Adjustment` | 1 | — | — | 0.0–1.0, step 0.02, page 0.05, round-digits 3; bound to the device |
| `Gtk.Stack` `devices_stack` | 1 | — | — | crossfade; **repeater** of one `BsDeviceEditor` per device, named by pointer |
| menu `primary_menu` | 1 | — | `app.shortcuts`, `win.about`, `app.quit` | 3 items, one section |

Port target: `lib/boatswain/window.rb` — **absent**.

### About dialog (built in src/bs-window.c, `on_show_about_action_activated_cb`)

`adw_show_about_dialog` with application-name "Boatswain", the app icon,
version, copyright "© 2022 Georges Basile Stavracas Neto", one developer, two
artists (Jakub Steiner, Sam Hewitt), GPL-3.0, website and issue URL. No `.ui`
file; it is a component with no file of its own.

Port target: `lib/boatswain/about_dialog.rb` — **absent**.

### src/shortcuts-dialog.ui — keyboard shortcuts

| Widget | × | CSS | Signals / actions | Notes |
|---|---:|---|---|---|
| `Adw.ShortcutsDialog` `shortcuts_dialog` | 1 | — | — | standalone object, no backing `.c` file |
| `Adw.ShortcutsSection` | 1 | — | — | title "General" (context `shortcut window`) |
| `Adw.ShortcutsItem` | 2 | — | `app.shortcuts`, `app.quit` | "Show Shortcuts", "Quit" |

**Upstream loose end to reproduce carefully:** `primary_menu` and this dialog
both reference `app.shortcuts`, but no source file installs that action or
loads `shortcuts-dialog.ui` — the resource is only listed in
`boatswain.gresource.xml`. The port should install `app.shortcuts` and present
this dialog; that is a gap in upstream, not a reason to drop the component.

Port target: `lib/boatswain/shortcuts_dialog.rb` — **absent**.

### src/bs-profile-row.c + src/bs-profile-row.ui — one profile in the sidebar

| Widget | × | CSS | Signals / actions | Notes |
|---|---:|---|---|---|
| `Adw.PreferencesRow` (template root) | 1 | — | signal `move (uint)`; action group `profile-row` | |
| `Gtk.CenterBox` | 1 | — | — | |
| `Gtk.Box` | 2 | — | — | start box (spacing 6), and the rename box inside the popover |
| `Gtk.Label` `title` | 1 | `title` | — | bound to the row's `title`/`use-underline`/`title-selectable`/`use-markup` |
| `Gtk.Image` `selected_icon` | 1 | — | — | `object-select-symbolic`; visible for the loaded profile |
| `Gtk.MenuButton` `menu_button` | 1 | `flat` | — | `view-more-symbolic`; styled smaller by `style.css` and dimmed when the row is not selected |
| `Gtk.PopoverMenu` | 1 | — | — | menu model `menu` with a `custom="rename"` slot |
| `Gtk.Entry` `rename_entry` | 1 | — | `activate` → `on_rename_entry_activate_cb`; `notify::text` → `on_rename_entry_text_changed_cb` | |
| `Gtk.Button` `rename_button` | 1 | `suggested-action` | `clicked` → `on_rename_button_clicked_cb` | label "Rename" |
| menu `menu` | 1 | — | `profile-row.move-up`, `profile-row.move-down`, `profile-row.delete` | submenu "Rename" hosting the custom slot; "Delete" in its own section |

Port target: `lib/boatswain/profile_row.rb` — **absent**.

### src/bs-device-editor.c + src/bs-device-editor.ui — one device's editing surface

| Widget | × | CSS | Signals / actions | Notes |
|---|---:|---|---|---|
| `Adw.Bin` (template root) | 1 | css-name `streamdeckeditor` | — | |
| `Adw.OverlaySplitView` | 1 | — | — | sidebar-position end |
| `Adw.ToolbarView` | 2 | — | — | content side, and the `empty_page` in the sidebar; both `top-bar-style` flat |
| `Adw.HeaderBar` | 2 | — | — | both with title and title-buttons suppressed |
| `Adw.Clamp` | 1 | — | — | max 700, tightening 500, margins 24 |
| `Gtk.Grid` `regions_grid` | 1 | — | — | **repeater**: one widget per `BsDeviceRegion` — `BsButtonGridWidget`, `BsDialWidget` wrapped in `Gtk.FlowBoxChild`, or `BsTouchscreenWidget` |
| `Adw.Bin` `editor_bin` | 1 | — | — | width-request 280; swapped between `empty_page`, `BsButtonEditor` and `BsTouchscreenSlotEditor` on selection |
| `Adw.StatusPage` | 1 | — | — | "Select a button" |

Port target: `lib/boatswain/device_editor.rb` — **absent**.

### src/bs-selection-controller.c — selection shared across region widgets

No widgets. One `selection-changed` signal, consumed by `BsDeviceEditor`,
`BsButtonGridWidget` and `BsTouchscreenWidget`. It is the unit that keeps the
three region widgets' selections mutually exclusive, so it must be ported as
its own object.

Port target: `lib/boatswain/selection_controller.rb` — **absent**.

### src/bs-button-grid-widget.c + .ui — the grid of device buttons

| Widget | × | CSS | Signals / actions | Notes |
|---|---:|---|---|---|
| `Gtk.Widget` (template root) | 1 | css-name `buttongridwidget` | — | `Gtk.BinLayout` layout manager |
| `Gtk.FlowBox` `flowbox` | 1 | — | `child-activated` → `on_flowbox_child_activated_cb`; `selected-children-changed` → `on_flowbox_selected_children_changed_cb` | homogeneous, single selection, 18px spacings, activate-on-single-click false |
| `BsButtonWidget` | *n* | — | — | **repeater** over the grid's buttons |

Port target: `lib/boatswain/button_grid_widget.rb` — **absent**.

### src/bs-button-widget.c + .ui — one button on the device

| Widget | × | CSS | Signals / actions | Notes |
|---|---:|---|---|---|
| `Gtk.FlowBoxChild` (template root) | 1 | css-name `streamdeckbuttonwidget`; `style.css` styles `:selected`, `:selected:focus:focus-visible`, `:drop(active)` | — | |
| `Gtk.DragSource` `drag_source` (controller) | 1 | — | `prepare` → `on_drag_prepare_cb`; `drag-begin` → `on_drag_begin_cb` | actions move |
| `Gtk.DropTarget` `drop_target` (controller, added in C) | 1 | — | — | `BsButton` / move; added via `gtk_widget_add_controller` |
| `Gtk.Picture` `picture` | 1 | `card` | — | width/height bound to `button.icon-width` / `icon-height` |

Port target: `lib/boatswain/button_widget.rb` — **absent**.

### src/bs-dial-widget.c + .ui — one dial

| Widget | × | CSS | Signals / actions | Notes |
|---|---:|---|---|---|
| `Gtk.Widget` (template root) | 1 | css-name `streamdeckdialwidget`; `style.css` gives it 72×72, full border-radius and `@card_bg_color` | — | centred; no children — it is drawn, not composed |

Port target: `lib/boatswain/dial_widget.rb` — **absent**.

### src/bs-touchscreen-widget.c + .ui — the touchscreen strip

| Widget | × | CSS | Signals / actions | Notes |
|---|---:|---|---|---|
| `Gtk.Widget` (template root) | 1 | `card`; css-name `touchscreenwidget` | — | overflow hidden, no expansion |
| `Gtk.Picture` `picture` | 1 | — | — | content-fit fill; the background image |
| `Gtk.FlowBox` `slots_flowbox` | 1 | — | `selected-children-changed` → `on_slots_flowbox_selected_children_changed_cb` | homogeneous, single selection, 1–4 children per line |
| `BsTouchscreenSlotWidget` | *n* | — | — | **repeater** over the touchscreen's slots |

Port target: `lib/boatswain/touchscreen_widget.rb` — **absent**.

### src/bs-touchscreen-slot-widget.c + .ui — one touchscreen slot

| Widget | × | CSS | Signals / actions | Notes |
|---|---:|---|---|---|
| `Gtk.FlowBoxChild` (template root) | 1 | css-name `touchscreenslotwidget`; `style.css` styles `:selected` (2px accent outline), first/last-child rounding and the RTL mirrors | — | width-request 1, expands |
| `Gtk.Picture` `picture` | 1 | — | — | `paintable` bound to the slot itself |

Port target: `lib/boatswain/touchscreen_slot_widget.rb` — **absent**.

### src/bs-button-editor.c + src/bs-button-editor.ui — the button properties pane

| Widget | × | CSS | Signals / actions | Notes |
|---|---:|---|---|---|
| `Adw.Bin` (template root) | 1 | css-name `streamdeckbuttoneditor`; `style.css` defines `.cutout` within it | — | |
| `Adw.NavigationView` `navigation_view` | 1 | — | `navigation.push` targets `'actions'` | two pages, tags `button` and `actions` |
| `Adw.NavigationPage` | 2 | — | — | "Button Properties", "Actions" |
| `Adw.ToolbarView` | 1 | — | — | top-bar-style flat |
| `Adw.HeaderBar` | 1 | — | — | start title buttons off |
| `Adw.PreferencesPage` `button_preferences_page` | 1 | — | — | |
| `Adw.PreferencesGroup` | 4 | — | — | icon group; `action_preferences_group` (hidden until an action is set); the Select-Action group (visible = NOT `remove_group.visible`); `remove_group` |
| `Gtk.Overlay` | 1 | — | — | holds the icon preview and its two overlay controls |
| `Gtk.Picture` `icon_picture` | 1 | `card` | — | 128×128, overflow hidden |
| `Adw.Bin` | 1 | `cutout` | — | wraps the remove button; visibility bound to it |
| `Gtk.Button` `remove_custom_icon_button` | 1 | `circular`, `outline` | `clicked` → `on_remove_custom_icon_button_clicked_cb` | `window-close-symbolic` |
| `Gtk.MenuButton` `custom_icon_menubutton` | 1 | `circular`, `outline`, `cutout` | — | `document-edit-symbolic` |
| `Gtk.Popover` | 1 | `custom-icon-popover` | — | `style.css` zeroes its padding and themes the grid view inside it |
| `Gtk.SearchEntry` `builtin_icons_search_entry` | 1 | — | — | drives the `Gtk.StringFilter`'s `search` |
| `Gtk.Button` | 1 | — | `clicked` → `on_custom_icon_button_clicked_cb` | `document-open-symbolic`; opens a `Gtk.FileDialog` titled "Select icon", accept label "Open", with image filters |
| `Gtk.Separator` | 1 | — | — | |
| `Gtk.ScrolledWindow` | 1 | — | — | 300×300 min content, hscrollbar never |
| `Gtk.GridView` `icons_gridview` | 1 | — | `activate` → `on_icons_gridview_activate_cb` | single-click-activate; `Gtk.NoSelection` over the sorted/filtered built-in icon list; `Gtk.BuilderListItemFactory` whose item is a `Gtk.Image` with icon-name and tooltip bound to the string |
| `Gtk.ListBox` | 1 | `boxed-list` | — | selection-mode none |
| `Adw.EntryRow` `custom_icon_text_row` | 1 | — | `notify::text` → `on_custom_icon_text_row_text_changed_cb` | title "Name" |
| `Adw.ActionRow` `background_color_row` | 1 | — | — | title "Background Color", activatable widget the colour button |
| `Gtk.ColorDialogButton` `background_color_dialog_button` + `Gtk.ColorDialog` | 1 | — | `notify::rgba` → `on_background_color_dialog_button_rgba_changed_cb` | modal, no alpha |
| `Adw.ButtonRow` | 2 | `suggested-action` / `destructive-action` | `navigation.push 'actions'`; `activated` → `on_remove_row_activated_cb` | "Select Action" (end icon `go-next-symbolic`), "Remove Action" |
| `BsActionSelector` | 1 | — | `action-selected` → `on_action_selector_action_selected_cb` | the `actions` page's child |
| models (not widgets) | — | — | — | `Gtk.SortListModel` + `Gtk.StringSorter` over `Gtk.FilterListModel` + `Gtk.StringFilter` (substring), fed from `Gtk.StringList` of icon names built in C |

Port target: `lib/boatswain/button_editor.rb` — **absent**.

### src/bs-action-selector.c + .ui — the action catalogue

| Widget | × | CSS | Signals / actions | Notes |
|---|---:|---|---|---|
| `Adw.Bin` (template root) | 1 | — | signal `action-selected (BsActionFactory, BsActionInfo)` | |
| `Adw.ToolbarView` | 1 | — | — | top-bar-style flat |
| `Adw.HeaderBar` | 1 | — | — | start title buttons off |
| `Adw.PreferencesPage` + `Adw.PreferencesGroup` | 1 each | — | — | |
| `Gtk.ListBox` `actions_listbox` | 1 | `boxed-list` | — | selection-mode none; `bind_model` over the available action factories |
| `Adw.ExpanderRow` | *n* | — | — | **repeater**, one per plugin: title from the plugin name, `Gtk.Image` prefix from the plugin icon |
| `Adw.ActionRow` | *n* | — | `activated` → `on_action_row_activated_cb` | **repeater**, one per non-hidden `BsActionInfo`: title, subtitle, `Gtk.Image` prefix; activating collapses every expander row and emits `action-selected` |

Port target: `lib/boatswain/action_selector.rb` — **absent**.

### src/bs-touchscreen-slot-editor.c + .ui — the touchscreen properties pane

| Widget | × | CSS | Signals / actions | Notes |
|---|---:|---|---|---|
| `Adw.Bin` (template root) | 1 | css-name `touchscreensloteditor`; `style.css` styles `box.header` inside it | — | |
| `Adw.NavigationView` `navigation_view` | 1 | — | `navigation.push 'actions'` | pages `button` ("Touchscreen Properties") and `actions` ("Actions") |
| `Adw.NavigationPage` | 2 | — | — | |
| `Adw.ToolbarView` + `Adw.HeaderBar` | 1 each | — | — | flat; start title buttons off |
| `Adw.PreferencesPage` `touchscreen_preferences_page` | 1 | — | — | |
| `Adw.PreferencesGroup` | 4 | — | — | "Touchscreen"; `action_preferences_group` ("Action", hidden); the Select-Action group (visible = NOT `remove_action_group.visible`); `remove_action_group` |
| `BsTouchscreenBackgroundRow` | 1 | — | — | `touchscreen` bound through the slot |
| `Adw.ButtonRow` | 2 | `suggested-action` / `destructive-action` | `navigation.push 'actions'`; `activated` → `on_remove_row_activated_cb` | "Select Action", "Remove Action" |
| `BsActionSelector` | 1 | — | `action-selected` → `on_action_selector_action_selected_cb` | |

Port target: `lib/boatswain/touchscreen_slot_editor.rb` — **absent**.

### src/bs-touchscreen-background-row.c + .ui — the background preview row

| Widget | × | CSS | Signals / actions | Notes |
|---|---:|---|---|---|
| `Adw.PreferencesRow` (template root) | 1 | `touchscreen-background` (padding 0, min-height 50 in `style.css`) | action `backgroundrow.select-background`; installs the `backgroundrow` action group | overflow hidden |
| `Gtk.Overlay` | 1 | — | — | |
| `Gtk.Picture` | 1 | — | — | content-fit cover; `paintable` bound through `touchscreen.content.background-paintable` |
| `Gtk.CenterBox` | 1 | `header` | — | expands |
| `Gtk.Button` | 1 | `osd`, `circular` | action `backgroundrow.select-background` | `document-edit-symbolic` |

Port target: `lib/boatswain/touchscreen_background_row.rb` — **absent**.

### src/bs-touchscreen-background-dialog.c + .ui — background picker

| Widget | × | CSS | Signals / actions | Notes |
|---|---:|---|---|---|
| `Adw.Dialog` (template root) | 1 | — | action group `backgrounddialog`: `set-background(s)`, `set-default-background` | 700×600 |
| `Adw.ToolbarView` + `Adw.HeaderBar` | 1 each | — | — | |
| `Adw.PreferencesPage` | 1 | — | — | |
| `Adw.PreferencesGroup` | 2 | — | — | the no-background group, and `adwaita_group` titled "Adwaita" |
| `Adw.ActionRow` | 1 | `touchscreen-default-background` (bold in `style.css`) | `backgrounddialog.set-default-background` | "No Background", `action-unavailable-symbolic`, tooltip "No Background" |
| `Adw.PreferencesRow` (built in C) | 5 | `touchscreen-background` | `backgrounddialog.set-background` with the resource URI as target | **repeater** over the built-ins: "Adwaita", "Glass", "Oh Sheet!", "Pills", "Swoosh"; each 80px tall, overflow hidden, activatable, tooltip = title, child a `Gtk.Picture` (content-fit cover) from `/com/feaneron/Boatswain/assets/backgrounds/*.png` |

Port target: `lib/boatswain/touchscreen_background_dialog.rb` — **absent**.

### src/style.css — the app stylesheet

19 rules. Classes and css-names the port must define or honour:
`.new-profile-entry`, `.cutout`, `.custom-icon-popover`,
`.touchscreen-background`, `.touchscreen-default-background`, `box.header`,
`.omnibar` (under `prefers-contrast: more`), plus the six css-names
`streamdeckbuttonwidget`, `streamdeckbuttoneditor`, `streamdeckdialwidget`,
`streamdeckeditor`, `touchscreenwidget`, `touchscreenslotwidget`,
`touchscreensloteditor`, `buttongridwidget`, and the
`list.navigation-sidebar row menubutton` rules that shrink and dim the profile
row's menu button.

Port target: `lib/boatswain/style.rb` (or `data/style.css` loaded by it) —
**absent**.

### data/com.feaneron.Boatswain.gschema.xml — settings

One key, `desktop-controller-restore-token` (string, default `''`).

Port target: `lib/boatswain/settings.rb` — **absent**.

---

## Plugin: default (src/plugins/default/)

### default-action-factory.c — the action catalogue for this plugin

Five `BsActionEntry` rows, each becoming one `Adw.ActionRow` in the action
selector: "Folder" (`folder-symbolic`), "Switch Profile"
(`view-list-bullet-symbolic`), "Brightness" (`display-brightness-symbolic`),
"Multiple Actions" (`stacked-plates-symbolic`), and "Go Up"
(`go-up-symbolic`, **hidden** — never shown in the selector).

Port target: `lib/boatswain/plugins/default/action_factory.rb` — **absent**.

| Upstream file | Component | Widgets built in C | Port target | Exists |
|---|---|---|---|---|
| `default-switch-page-action.c` | "Folder" action | none (no preferences) | `lib/boatswain/plugins/default/switch_page_action.rb` | no |
| `default-page-up-action.c` | "Go Up" action, hidden | none | `lib/boatswain/plugins/default/page_up_action.rb` | no |
| `default-switch-profile-action.c` | "Switch Profile" action | `Adw.PreferencesGroup` + 2 `Adw.ComboRow` ("Stream Deck", "Profile"), each with a property expression over the name | `lib/boatswain/plugins/default/switch_profile_action.rb` | no |
| `default-brightness-action.c` | "Brightness" action | `Gtk.ListBox` `boxed-list` + `Adw.ComboRow` "Mode" over a `Gtk.StringList` + `Adw.ActionRow` "Value" with a `Gtk.SpinButton` (0.01–1.0, step 0.01); icon swaps between three brightness icons | `lib/boatswain/plugins/default/brightness_action.rb` | no |
| `default-multi-action.c` | "Multiple Actions" action | returns its editor | `lib/boatswain/plugins/default/multi_action.rb` | no |

### default-multi-action-editor.c + .ui — the multi-action editor

| Widget | × | CSS | Signals / actions | Notes |
|---|---:|---|---|---|
| `Gtk.Box` (template root) | 1 | — | — | vertical, spacing 18 |
| `Adw.PreferencesGroup` | 2 | — | — | `entries_group`, and the add-action group |
| `Gtk.ListBox` `entries_listbox` | 1 | `boxed-list` | — | selection-mode none; holds `DefaultMultiActionRow`s |
| `Adw.ActionRow` | 1 | — | `activated` → `on_add_action_row_activated_cb` | "Add action", `Gtk.Image` `go-next-symbolic` suffix |
| `Gtk.Dialog` `actions_dialog` | 1 | — | — | "Add Action", modal, header-bar, 450×450, hide-on-close; `Gtk.HeaderBar` + `Adw.PreferencesPage` with `actions_group` ("Actions") and `others_group` ("Others") |
| `Adw.ExpanderRow` / `Adw.ActionRow` inside it | *n* | — | — | **repeater** over the factories, same shape as `BsActionSelector` |
| `Adw.ActionRow` (delay row, built in C) | 1 | — | — | in `others_group`, prefixed `preferences-system-time-symbolic` |
| `Gtk.Dialog` `preferences_dialog` | 1 | — | `close-request` → `on_preferences_dialog_close_request_cb` | "Settings", modal, header-bar, 450×450; `Adw.Bin` `preferences_bin` hosts the sub-action's preferences |

Port target: `lib/boatswain/plugins/default/multi_action_editor.rb` —
**absent**. Note these are `Gtk.Dialog`s, not `Adw.Dialog`s, upstream.

### default-multi-action-row.c + .ui — one entry in the multi-action list

| Widget | × | CSS | Signals / actions | Notes |
|---|---:|---|---|---|
| `Adw.ActionRow` (template root) | 1 | — | signals `changed`, `edit`, `remove` | |
| `Gtk.Image` `icon` | 1 | — | — | prefix |
| `Gtk.SpinButton` `delay_spinbutton` + `Gtk.Adjustment` | 1 | — | `notify::value` → `on_delay_adjustment_value_changed_cb` | hidden unless the entry is a delay; 100–99999999, default 250 |
| `Gtk.Button` `edit_button` | 1 | `flat` | `clicked` → `on_edit_button_clicked_cb` | `preferences-system-symbolic`, hidden unless the sub-action has preferences |
| `Gtk.Button` (remove) | 1 | `flat` | `clicked` → `on_remove_button_clicked_cb` | `edit-delete-symbolic` |

Port target: `lib/boatswain/plugins/default/multi_action_row.rb` — **absent**.

---

## Plugin: desktop (src/plugins/desktop/)

| Upstream file | Component | Detail | Port target | Exists |
|---|---|---|---|---|
| `desktop-action-factory.c` | action catalogue | 1 entry: "Keyboard Shortcut" / "Trigger a keyboard shortcut", `preferences-desktop-keyboard-shortcuts-symbolic` | `lib/boatswain/plugins/desktop/action_factory.rb` | no |
| `desktop-keyboard-shortcut-action.c` | the action | prefs built in C: `Adw.PreferencesGroup` + activatable `Adw.ActionRow` "Keyboard Shortcut" with a `Gtk.Label` and a `go-next-symbolic` `Gtk.Image`; activating opens `DesktopShortcutDialog` | `lib/boatswain/plugins/desktop/keyboard_shortcut_action.rb` | no |

### desktop-shortcut-dialog.c + .ui — capture a shortcut

| Widget | × | CSS | Signals / actions | Notes |
|---|---:|---|---|---|
| `Adw.Window` (template root) | 1 | — | — | "Set Shortcut", 400×300, non-resizable, modal |
| `Gtk.EventControllerKey` `key_controller` (controller) | 1 | — | `key-pressed` → `on_key_pressed_cb` | capture phase |
| `Gtk.Stack` `stack` | 1 | — | — | pages `shortcut` and `review` |
| `Adw.ToolbarView` | 2 | — | — | one per page |
| `Adw.HeaderBar` | 2 | — | — | all title buttons off |
| `Gtk.CenterBox` | 2 | — | — | |
| `Gtk.Box` | 2 | — | — | vertical, spacing 18, centred |
| `Gtk.Picture` | 1 | — | — | `resource:///…/plugins/desktop/assets/enter-keyboard-shortcut.svg`, can-shrink false |
| `Gtk.Label` | 1 | `dim-label` | — | "Press Esc to cancel", wrapping |
| `Gtk.Button` `cancel_button` | 1 | — | `window.close` | "_Cancel", hidden until review |
| `Gtk.Button` `set_button` | 1 | `suggested-action` | `clicked` → `on_set_button_clicked_cb` | "_Set", hidden until review |
| `Gtk.ShortcutLabel` `shortcut_label` | 1 | — | — | shows the captured accelerator |

Port target: `lib/boatswain/plugins/desktop/shortcut_dialog.rb` — **absent**.

---

## Plugin: launcher (src/plugins/launcher/)

| Upstream file | Component | Detail | Port target | Exists |
|---|---|---|---|---|
| `launcher-action-factory.c` | action catalogue | 3 entries: "Launch Application" (`app-launch-symbolic`), "Open File" (`folder-documents-symbolic`), "Open URL" (`open-link-symbolic`) | `lib/boatswain/plugins/launcher/action_factory.rb` | no |
| `launcher-launch-action.c` | "Launch Application" action | returns `LauncherLaunchPreferences`; icon falls back to `application-x-executable-symbolic` | `lib/boatswain/plugins/launcher/launch_action.rb` | no |
| `launcher-launch-preferences.c` + `.ui` | its preferences | `Adw.PreferencesGroup` root; `Adw.ActionRow` `app_row` "Choose Application", activatable, `activated` → `on_app_row_activated_cb` (swapped); `Gtk.Label` `app_name` (hidden until chosen) and a `go-next-symbolic` `Gtk.Image` whose visibility is the inverse | `lib/boatswain/plugins/launcher/launch_preferences.rb` | no |
| `launcher-open-file-action.c` | "Open File" action | prefs in C: `Adw.PreferencesGroup` + `Adw.ActionRow` "File" (subtitle is the basename or "No file selected", gains/loses the `property` class), `document-open-symbolic` `Gtk.Image`, and a `flat` clear button `edit-clear-symbolic`; `Gtk.FileDialog` "Select File"; launches with `Gtk.FileLauncher` | `lib/boatswain/plugins/launcher/open_file_action.rb` | no |
| `launcher-open-url-action.c` | "Open URL" action | prefs in C: `Adw.PreferencesGroup` + `Adw.EntryRow` "URL"; launches with `Gtk.UriLauncher`; icon `web-browser-symbolic` | `lib/boatswain/plugins/launcher/open_url_action.rb` | no |

---

## Plugin: network (src/plugins/network/)

| Upstream file | Component | Detail | Port target | Exists |
|---|---|---|---|---|
| `network-action-factory.c` | action catalogue | 1 entry: "HTTP Request" / "Send GET and POST requests to a webserver", `network-transmit-symbolic` | `lib/boatswain/plugins/network/action_factory.rb` | no |
| `network-http-action.c` | the action | returns `NetworkHttpActionPrefs`; performs the request | `lib/boatswain/plugins/network/http_action.rb` | no |

### network-http-action-prefs.c + .ui

| Widget | × | CSS | Signals / actions | Notes |
|---|---:|---|---|---|
| `Gtk.Box` (template root) | 1 | — | — | vertical, spacing 24 |
| `Adw.PreferencesGroup` | 2 | — | — | the second is `post_group`, shown only for POST |
| `Adw.EntryRow` `uri_row` | 1 | — | `notify::text` → `on_uri_row_text_changed_cb` | "URI" |
| `Adw.ComboRow` `method_row` | 1 | — | `notify::selected-item` → `on_method_row_selected_changed_cb` | "Method", `Gtk.StringList` GET / POST |
| `Adw.ComboRow` `payload_src_row` | 1 | — | `notify::selected-item` → `on_payload_src_row_selected_changed_cb` | "Payload source", Text / File |
| `Adw.ActionRow` `file_row` | 1 | — | `activated` → `on_file_row_activated_cb` | "File"; `Gtk.Label` `filename_label` (ellipsize middle, 35 chars) + `document-open-symbolic` `Gtk.Image`; opens a `Gtk.FileDialog` "Select payload file" with filters |
| `Adw.ActionRow` `edit_row` | 1 | — | `activated` → `on_edit_row_activated_cb` | "Edit payload", `Gtk.Image` `edit_row_icon` `text-editor-symbolic` |
| `Adw.Window` `text_edit_dialog` | 1 | — | — | "HTTP Request Payload", 500×500, hide-on-close; `Adw.ToolbarView` + `Adw.HeaderBar` + `Gtk.ScrolledWindow` + `Gtk.TextView` (12px margins) over `payload_text_buffer` |

Port target: `lib/boatswain/plugins/network/http_action_prefs.rb` — **absent**.
The payload dialog is a second top-level object in the same `.ui`, so it is
part of this component, not a separate file.

---

## Plugin: obs-studio (src/plugins/obs-studio/)

| Upstream file | Component | Detail | Port target | Exists |
|---|---|---|---|---|
| `obs-action-factory.c` | action catalogue | 6 entries: "Switch Scene" (`obs-switch-scene-symbolic`), "Record" (`media-record-symbolic`), "Stream" (`transmit-symbolic`), "Virtual Camera" (`cameras-symbolic`), "Toggle Mute" (`audio-volume-muted-symbolic`), "Show / Hide Source" (`eye-not-looking-symbolic`) — the last two are the **same** class with different source caps | `lib/boatswain/plugins/obs_studio/action_factory.rb` | no |
| `obs-action.c` | base action | its `get_preferences` returns the `ObsConnectionSettings` group that every OBS action appends to its own preferences | `lib/boatswain/plugins/obs_studio/obs_action.rb` | no |
| `obs-connection-settings.c` + `.ui` | connection settings group | `Adw.PreferencesGroup` root; `Adw.ExpanderRow` "Connection Settings"; `Adw.EntryRow` `host_row` "Host" (show-apply-button, `apply` → `on_host_row_applied_cb`); `Adw.ActionRow` "Port" with `Gtk.SpinButton` 0–65535 default 4455 (`notify::value` → `on_port_adjustment_value_changed_cb`); `Adw.PasswordEntryRow` `password_row` "Password" (show-apply-button, `apply` → `on_password_row_applied_cb`, gains/loses the **`error`** class on auth failure) | `lib/boatswain/plugins/obs_studio/connection_settings.rb` | no |
| `obs-switch-scene-action.c` | "Switch Scene" | prefs: `Gtk.Box` vertical/18 + `Adw.PreferencesGroup` + `Adw.ComboRow` "Scene" over a property expression on `name`, then the inherited connection settings; icon `obs-scene-symbolic` | `lib/boatswain/plugins/obs_studio/switch_scene_action.rb` | no |
| `obs-record-action.c` | "Record" | no own prefs; icon cycles `media-record-symbolic` / `media-playback-pause-symbolic` | `lib/boatswain/plugins/obs_studio/record_action.rb` | no |
| `obs-stream-action.c` | "Stream" | no own prefs; icon `transmit-symbolic` | `lib/boatswain/plugins/obs_studio/stream_action.rb` | no |
| `obs-virtualcam-action.c` | "Virtual Camera" | no own prefs; icon `cameras-symbolic` | `lib/boatswain/plugins/obs_studio/virtualcam_action.rb` | no |
| `obs-toggle-source-action.c` | "Toggle Mute" and "Show / Hide Source" | prefs: `Gtk.Box` vertical/18 + group + `Adw.ComboRow` "Behavior" + `Adw.ComboRow` titled "Audio Source" or "Source" depending on caps, over a `Gtk.FilterListModel` with a `Gtk.CustomFilter`, then the inherited connection settings; icon swaps across six states | `lib/boatswain/plugins/obs_studio/toggle_source_action.rb` | no |
| `obs-connection-manager.c`, `obs-connection.c`, `obs-scene.c`, `obs-source.c`, `obs-utils.c` | the websocket layer | no widgets, but every OBS component's data | `lib/boatswain/plugins/obs_studio/*.rb` | no |

---

## Plugin: soundboard (src/plugins/soundboard/)

| Upstream file | Component | Detail | Port target | Exists |
|---|---|---|---|---|
| `soundboard-action-factory.c` | action catalogue | 2 entries: "Music Player" / "Control the active music player" (`audio-x-generic-symbolic`), "Play Audio" (`media-playback-start-symbolic`) | `lib/boatswain/plugins/soundboard/action_factory.rb` | no |
| `soundboard-mpris-action.c` | "Music Player" | prefs in C: `Adw.PreferencesGroup` + `Adw.ComboRow` "Playback Action" over a `Gtk.StringList`; icon swaps across six playback icons | `lib/boatswain/plugins/soundboard/mpris_action.rb` | no |
| `mpris-controller.c`, `bus-watch-namespace.c` | MPRIS plumbing | no widgets | `lib/boatswain/plugins/soundboard/mpris_controller.rb` | no |
| `soundboard-play-action.c` | "Play Audio" | returns `SoundboardPlayActionPrefs`; plays through `Gtk.MediaFile` | `lib/boatswain/plugins/soundboard/play_action.rb` | no |
| `soundboard-play-action-prefs.c` + `.ui` | its preferences | `Adw.PreferencesGroup` root; `Adw.ActionRow` `file_row` "File" activatable (`activated` → `on_file_row_activated_cb`, `Gtk.FileDialog` "Select audio file") with `Gtk.Label` `filename_label` (ellipsize middle, 35 chars); `Adw.ComboRow` `behavior_row` "Behavior" with five translated items (Play / Stop, Play / Overlap, Play / Restart, Loop / Stop, Press & Hold); `Adw.ActionRow` "Volume" with a `Gtk.Scale` 0.0–1.0 default 1.0 | `lib/boatswain/plugins/soundboard/play_action_prefs.rb` | no |

---

## Plugin: gaming (src/plugins/gaming/, GJS)

| Upstream file | Component | Detail | Port target | Exists |
|---|---|---|---|---|
| `gaming.js` | action catalogue | 1 entry: "Score", `score-symbolic` | `lib/boatswain/plugins/gaming/action_factory.rb` | no |
| `score.js` | "Score" action | prefs: `Gtk.Box` vertical + `Adw.PreferencesGroup` + `Adw.SwitchRow` "Restore Score"; `Adw.ActionRow` "Text Color" with a `Gtk.ColorDialogButton`; a second group with `Adw.SwitchRow` "Save to File" ("Save score to a text file. Other apps such as OBS Studio may read from it.") and an `Adw.ActionRow` "File" opening a `Gtk.FileDialog` with a "Text File (.txt)" filter, default name "score.txt"; long-press handling uses `Gtk.Settings.gtk_long_press_time` | `lib/boatswain/plugins/gaming/score_action.rb` | no |

---

## Translations

`po/LINGUAS` lists 38 locales; 39 `.po` files plus `boatswain.pot` are
committed. `po/POTFILES.in` names 62 files, 11 of them `.ui` templates. Because
the port has no `.ui` files, every string those templates contribute must move
into the corresponding `lib/*.rb` and `POTFILES.in` must be rewritten against
the Ruby paths. Counted as one unit: `po/` — **absent** from the port.

---

## Totals

| | Upstream | Port (`ruby`) |
|---|---:|---:|
| User-facing components (this inventory) | 52 | 1, placeholder only |
| `.ui` templates / top-level builder objects under `src/` | 20 | 0 (by design — the house style builds widgets in Ruby) |
| Action catalogue entries shown in the selector | 18 (+1 hidden) | 0 |
| Distinct CSS classes and css-names in `style.css` | 7 classes, 8 css-names | 0 |
| GSettings keys | 1 | 0 |
| Locales | 38 | 0 |

## Blockers

1. **No device layer in the port.** Everything in `src/` is driven by `lib/`
   upstream — `BsDevice`, `BsProfile`, `BsPage`, `BsButton`, `BsDial`,
   `BsTouchscreen`, `BsIcon`, `BsRenderer`, `BsContext`, and the Elgato,
   Loupedeck and mock device providers over USB HID. Until at least the mock
   provider exists in Ruby, the window can only ever show its
   "No Stream Deck Found" status page, so no component below the window can be
   exercised. The mock provider (`lib/devices/mock/`) is the cheapest way to
   make the rest of the UI reachable and should be ported first.
2. **libpeas plugin loading has no Ruby equivalent.** Upstream discovers action
   factories as libpeas extensions (including a GJS loader for the `gaming`
   plugin). The port needs a plain Ruby registry that `BsContext` equivalents
   can enumerate; the action selector and the multi-action editor both
   `bind_model` over it.
3. **Path convention needs a decision.** The task's house shape says one
   upstream file maps to one `lib/<thing>.rb`, and the pilot
   `gnome-contacts-rb` uses a flat `lib/*.rb`. This repo is packaged as a gem
   and already has `lib/boatswain.rb` + `lib/boatswain/application.rb`. This
   inventory therefore targets `lib/boatswain/<thing>.rb` and
   `lib/boatswain/plugins/<plugin>/<thing>.rb`. Confirm before the first port
   unit lands, because changing it later renames 50 files.
4. **`app.shortcuts` is unimplemented upstream.** The menu item and
   `shortcuts-dialog.ui` exist; no code installs the action or loads the
   dialog. The port should implement it rather than inherit the gap.
5. **Several upstream dialogs are `Gtk.Dialog`, which is deprecated.** The
   multi-action editor's two dialogs in particular. Reproducing them as
   `Adw.Dialog` changes behaviour; reproducing them as `Gtk::Dialog` keeps
   parity. Decide per dialog and record it, do not let the choice be implicit.
6. **The port has no component or test parity record yet.** `COMPONENT_PARITY.md`
   and `TEST_PARITY.md` named in `AGENTS.md` do not exist, and `spec/` holds
   only `spec_helper.rb`.
