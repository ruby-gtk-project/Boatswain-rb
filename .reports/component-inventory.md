# Boatswain-rb component inventory

Survey of upstream `origin/main` against the `ruby` port's `lib/`, for the
full-parity port. No app code was written for this pass.

Upstream shape: a GObject/C core (`lib/bs-*.c`) plus a GTK widget layer
(`src/bs-*.c` + `src/bs-*.ui`), a device-provider layer per hardware vendor
(`lib/devices/{elgato,loupedeck,mock}/`), and an action-plugin layer per
feature (`src/plugins/{default,desktop,gaming,gjs-base,launcher,network,
obs-studio,soundboard}/`). The Ruby port mirrors this under
`lib/boatswain/{,devices/,plugins/}`.

Legend: **present** = the `lib/*.rb` file exists and is reachable from
`lib/boatswain.rb` (traced via `require_relative`). **missing** = no port
file corresponds to the upstream file.

## Window, pages and core widgets

| Upstream | → `lib/boatswain/*.rb` | Kind | Present? |
|---|---|---|---|
| `src/bs-window.c`, `src/bs-window.ui` | `window.rb` | window | yes |
| `src/bs-action-selector.c/.ui` | `action_selector.rb` | dialog/selector | yes |
| `src/bs-button-editor.c/.ui` | `button_editor.rb` | editor pane | yes |
| `src/bs-button-grid-widget.c/.ui` | `button_grid_widget.rb` | widget | yes |
| `src/bs-button-widget.c/.ui` | `button_widget.rb` | widget (row/cell) | yes |
| `src/bs-device-editor.c/.ui` | `device_editor.rb` | dialog | yes |
| `src/bs-dial-widget.c/.ui` | — | widget | **missing** (only the model `dial.rb` exists, not the GTK widget) |
| `src/bs-profile-row.c/.ui` | `profile_row.rb` | row | yes |
| `src/bs-selection-controller.c` | `selection_controller.rb` | controller | yes |
| `src/bs-touchscreen-widget.c/.ui` | — | widget | **missing** |
| `src/bs-touchscreen-slot-widget.c/.ui` | — | widget | **missing** (only the model `touchscreen_slot.rb` exists) |
| `src/bs-touchscreen-slot-editor.c/.ui` | — | editor dialog | **missing** |
| `src/bs-touchscreen-background-row.c/.ui` | — | row | **missing** |
| `src/bs-touchscreen-background-dialog.c/.ui` | — | dialog | **missing** |
| `src/shortcuts-dialog.ui` | `shortcuts_dialog.rb` | dialog | yes |
| `src/bs-application.c/.h` | `application.rb` | app class | yes |
| `src/style.css` | `style.rb` | stylesheet | yes |
| (About dialog, built inline in upstream window/application) | `about_dialog.rb` | dialog | yes |

## Core (non-widget) model/business layer — `lib/bs-*.c`

| Upstream | → `lib/boatswain/*.rb` | Kind | Present? |
|---|---|---|---|
| `lib/bs-actionable.c` | `actionable.rb` | mixin/interface | yes |
| `lib/bs-action.c` | `action.rb` | model | yes |
| `lib/bs-action-factory.c` | `action_factory.rb` | factory | yes |
| `lib/bs-action-info.c` | `action_info.rb` | model | yes |
| `lib/bs-button.c` | `button.rb` | model | yes |
| `lib/bs-button-grid.c` | `button_grid.rb` | model | yes |
| `lib/bs-context.c` | `context.rb` | model | yes |
| `lib/bs-desktop-controller.c` | — | controller | **missing** (desktop remote-control session logic) |
| `lib/bs-device.c` | `device.rb` | model | yes |
| `lib/bs-device-layout-builder.c` | — | builder | **missing** |
| `lib/bs-device-manager.c` | `device_manager.rb` | manager | yes |
| `lib/bs-device-provider.c` | (base class folded into `devices/*/device_provider.rb`) | abstract base | yes (as per-provider subclasses, no standalone base file) |
| `lib/bs-device-region.c` | `device_region.rb` | model | yes |
| `lib/bs-device-update.c` | `device_update.rb` | model | yes |
| `lib/bs-dial.c` | `dial.rb` | model | yes |
| `lib/bs-dial-grid.c` | `dial_grid.rb` | model | yes |
| `lib/bs-empty-action.c` | `empty_action.rb` | model | yes |
| `lib/bs-events.c` | `events.rb` | model/event bus | yes |
| `lib/bs-global.c` | — | globals/app-wide state | **missing** (or folded elsewhere — verify) |
| `lib/bs-icon.c` | `icon.rb` | model | yes |
| (icon rendering widget, no single upstream file) | `icon_widget.rb` | widget | yes (port-added; verify against `bs-icon.c` rendering paths) |
| `lib/bs-page.c` | `page.rb` | model | yes |
| `lib/bs-page-item.c` | `page_item.rb` | model | yes |
| `lib/bs-profile.c` | `profile.rb` | model | yes |
| `lib/bs-renderer.c` | — | renderer | **missing** (renders button/dial content to device-native image format) |
| `lib/bs-touchscreen.c` | — | model | **missing** |
| `lib/bs-touchscreen-content.c` | — | model | **missing** |
| `lib/bs-touchscreen-slot.c` | `touchscreen_slot.rb` | model | yes |
| `lib/bs-utils.c` | — | utils | **missing** (or inlined elsewhere — verify) |

## Device providers — `lib/devices/*`

| Upstream | → `lib/boatswain/devices/*` | Kind | Present? |
|---|---|---|---|
| `lib/devices/elgato/*` (14 files: provider + 11 Stream Deck model subclasses) | `devices/usb/hardware_detector.rb` only | device provider + per-model drivers | **partial/missing** — no Elgato-specific provider or per-model (mini/original/mk2/neo/plus/xl/pedal/gen1/gen2/v2) classes in the port |
| `lib/devices/loupedeck/*` (loupedeck device, provider, Razer Stream Controller X) | — | device provider | **missing** |
| `lib/devices/mock/*` | `devices/mock/device.rb`, `devices/mock/device_provider.rb` | device provider | yes |

## Action plugins — `src/plugins/*`

| Upstream plugin | → `lib/boatswain/plugins/*` | Present? |
|---|---|---|
| `default/` (factory, brightness, multi-action, multi-action editor/row, page-up, switch-page, switch-profile) | `plugins/default/*` (all 7 files) | yes |
| `desktop/` (keyboard-shortcut action, shortcut dialog/factory) | — | **missing** |
| `gaming/` (score action/factory; `gaming.js`, `score.js` GJS base) | `plugins/gaming/action_factory.rb`, `score_action.rb` | yes (functional pieces) |
| `gjs-base/` (GJS plugin-loader base, not a feature) | N/A | n/a — infra, not a port-time component |
| `launcher/` (launch action, launch preferences, open-file, open-url, factory) | `plugins/launcher/*` (all 5 files) | yes |
| `network/` (http action, http action prefs, factory) | `plugins/network/*` (all 3 files) | yes |
| `obs-studio/` (action, action-factory, connection, connection manager, connection settings + `.ui`, record/stream/switch-scene/toggle-source/virtualcam actions, scene, source) | — | **missing entirely** |
| `soundboard/` (bus-watch-namespace, mpris-controller, action-factory, mpris/play actions, play-action-prefs + `.ui`) | — | **missing entirely** |

## Data / platform

| Upstream | Kind | Port status |
|---|---|---|
| `data/com.feaneron.Boatswain.gschema.xml` | GSettings schema | `settings.rb` wraps GSettings; confirm schema id and keys match 1:1 — not verified this pass |
| `data/com.feaneron.Boatswain.metainfo.xml.in.in` | AppStream metadata | not a `lib/` component; verify packaging |
| `data/com.feaneron.Boatswain.desktop.in.in` | desktop file | not a `lib/` component |
| `po/*.po`, `po/boatswain.pot`, `po/LINGUAS` | translation catalogue | out of scope for this pass — see translation-parity |

## Summary

Upstream: ~90 widget/model source files (`lib/bs-*.c` + `.ui` companions),
3 device-provider backends, 8 action plugins.

`ruby` branch `lib/`: 58 files. Strong coverage of the window, the main
editor/selector dialogs, the `default`/`launcher`/`network`/`gaming` action
plugins, and the core model layer (action, button, device, page, profile,
dial, context, events).

Clear full-parity gaps, in the order a user would hit them:

1. **Touchscreen support is entirely unported** — no touchscreen widget,
   slot widget, slot editor, background row/dialog, or the underlying
   `bs-touchscreen*` / `bs-touchscreen-content` models. This is a whole
   feature area (Stream Deck Plus's touch strip) with zero port surface.
2. **The dial widget is unported** — `dial.rb`/`dial_grid.rb` model the
   data but there is no `Gtk::Widget` rendering a dial on screen.
3. **Device support is Mock-only** — no Elgato or Loupedeck provider/driver
   classes, so the port cannot talk to real hardware yet, only the
   in-process mock device.
4. **Two whole plugins are unported**: `obs-studio` (OBS Studio actions —
   scenes, recording, streaming, source toggling) and `soundboard`
   (MPRIS-based audio actions). The `desktop` plugin (keyboard-shortcut
   action) is also unported.
5. **`bs-renderer.c`** (renders button/dial Ruby-side image content down to
   device-native bitmaps) has no port file — likely blocks any real device
   from showing anything, even once a device provider exists.
6. `bs-desktop-controller.c`, `bs-device-layout-builder.c`, `bs-global.c`
   and `bs-utils.c` have no obvious port counterpart; need a closer read to
   confirm whether their logic was folded into an existing file (e.g.
   `device_manager.rb`, `context.rb`) or genuinely dropped.

This inventory does not evaluate per-widget CSS classes/signals/actions
(component-parity-level detail) — it establishes, file by file, which
upstream components have no port home yet. That finer pass belongs to
`component-parity` once each missing file above is taken as its own unit.
