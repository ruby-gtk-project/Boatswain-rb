# Component parity — Boatswain

| | |
|---|---|
| Upstream | `main` @ `0262d32` |
| Port | `ruby` @ `7855f51` |
| Units with full (4-axis, driven) parity | **0 / ~58** |
| Runtime gate this round | **drive_ok = false, 0 checks, 0 components covered** |

58 is the unit count from `.reports/component-inventory.md`'s 52 user-facing
components plus the ~17 device/model-layer files minus overlap, rounded to
the nearest whole unit; see that document and `component-identification.md`
for the per-file breakdown.

## Verdict, stated plainly

**Zero units have proven component parity**, for one structural reason that
overrides every individual widget comparison below: **no check in this repo
has ever run against a live GTK widget tree.** `ruby -c` (syntax) passes on
all 46 `lib/` files, and that is the *only* thing that has been verified. The
`component-parity` skill is explicit that parity is proven in Steps 4 and 5 —
stylesheet rendering and signals actually firing, both checked by driving the
app — and neither step has ever been executed here. A widget type and a CSS
class string appearing in a `.rb` file is not a built widget; it is a claim
about one.

This round's runtime gate (`drive_ok=false`, 0 checks, 0 components covered)
is not a separate, smaller finding alongside the component tables — it is
the reason the component tables below cannot say `parity` for anything, no
matter how many axes look right on paper.

## Step 1/2 — inventory and file mapping (summary)

See `.reports/component-identification.md` for the scan and
`.reports/component-inventory.md` for the per-file widget/CSS/signal tables
inherited from the prior survey. The file mapping, restated:

```
src/bs-application.c                          -> lib/boatswain/application.rb
src/bs-window.c + .ui                          -> lib/boatswain/window.rb
src/bs-about-dialog equivalent (Adw built-in)  -> lib/boatswain/about_dialog.rb
src/bs-shortcuts-window equivalent             -> lib/boatswain/shortcuts_dialog.rb
lib/bs-device*.c                               -> lib/boatswain/device*.rb
lib/bs-profile.c                               -> lib/boatswain/profile.rb
lib/bs-page*.c                                 -> lib/boatswain/page*.rb
lib/bs-button*.c                               -> lib/boatswain/button*.rb
lib/bs-context.c                               -> lib/boatswain/context.rb
lib/devices/mock/*.c                           -> lib/boatswain/devices/mock/*.rb
src/plugins/default/*.c                        -> lib/boatswain/plugins/default/*.rb
src/plugins/launcher/*.c                       -> lib/boatswain/plugins/launcher/*.rb
src/plugins/network/*.c                        -> lib/boatswain/plugins/network/*.rb
src/plugins/gaming/*.js                        -> lib/boatswain/plugins/gaming/*.rb
--- no port file on the right ---
lib/bs-dial*.c, lib/bs-touchscreen*.c          -> (absent)
src/plugins/desktop/*                          -> (absent)
src/plugins/obs-studio/*                       -> (absent)
src/plugins/soundboard/*                       -> (absent)
lib/devices/elgato/*                           -> (absent)
lib/devices/loupedeck/*                        -> (absent)
data/*.gschema.xml, *.desktop.in, *.metainfo*  -> (absent)
po/*                                           -> (absent)
```

Every row on the left with nothing on the right is a gap under this skill's
own rule ("an upstream file with no port file opposite it is the first and
largest kind of gap"). That is **6 of upstream's roughly 13 top-level
areas** (device providers, 3 of 6 action plugins, the dial/touchscreen
widget family, and the whole `data/`+`po/` tree) with **no port file at
all** — not a partial build, zero.

## Step 3/4/5 — axis comparison, styling, behaviour

Not performed per-unit this round, for a reason that is itself the finding:
**there is nothing to drive.** Steps 4 and 5 require running the port headless
(`ruby-gtk-testing`) and asserting on live widget state; `exe/boatswain` was
launched once during this audit (`xvfb-run ruby exe/boatswain`, 25s, no crash,
Vulkan/DRI3 warnings only) which proves the application object constructs and
the main loop starts — and proves nothing beyond that, because the run was
not driven, nothing was clicked, and no assertion was made. No `test/drive_*`
script exists to do that driving (see `.reports/test-parity.md`).

So every unit in the inventory is in exactly the state
`component-identification.md`'s prior survey already named:
**`exists (unverified)` or `absent`**, and this report cannot promote a
single one of them past that without the drive step actually running. Doing
so anyway — reading the `.rb` source and asserting "looks right" — is exactly
the shortcut both skills prohibit (parity is proven by driving, not by
reading).

### What the per-file reading *can* say today (static only, not parity)

- `lib/boatswain/window.rb`, `about_dialog.rb`, `shortcuts_dialog.rb`,
  `device_editor.rb`, `button_editor.rb`, `button_grid_widget.rb`,
  `button_widget.rb`, `action_selector.rb`, `profile_row.rb` are present and
  `ruby -c` clean, following the house `.tap`/memoized-accessor style.
- `lib/boatswain/context.rb` wires exactly 4 of 6 upstream action plugins
  (confirmed by grep, see `component-identification.md`) — this is not a
  reading uncertainty, it is a direct count.
- Signal count from the scan (20 port vs 50 upstream, see
  `component-identification.md`) is consistent with widgets being built but
  under-connected — the classic "screenshot, not an app" shape the skill
  warns about. It cannot be split per-unit without the symbol table rebuild
  and per-file reading that did not happen this round (see that report's
  self-audit checklist).

## Stylesheet

```
$ find . -iname '*.css'
(no output)
$ grep -rn 'CssProvider\|load(data:' lib/ exe/
lib/boatswain/style.rb   (defines Style, not grepped as a literal hit above
                          because it is not named CssProvider directly in
                          this file — content not verified against upstream's
                          style.css this round)
```

Upstream's `src/style.css` (referenced via `boatswain.gresource.xml`) was not
diffed against `lib/boatswain/style.rb`'s content in this pass. That is a gap
in this report, not a clean bill for the file — open it and diff before
claiming any CSS-axis parity.

## Text axis

Not evaluated per-component here; see `.reports/translation-parity.md`,
whose finding (0/147 messages, 0/38 languages) makes every unit's text axis
an automatic **gap** regardless of what the widget/CSS/signal axes eventually
show, because no string in the port goes through a translation marker at
all.

## Gaps (component-level, this round)

| Area | Upstream | Port | State |
|---|---|---|---|
| Elgato Stream Deck device provider (10 models) | `lib/devices/elgato/*` (15 files) | absent | **gap** |
| Loupedeck device provider | `lib/devices/loupedeck/*` (5 files) | absent | **gap** |
| Dial / touchscreen widget family | `lib/bs-dial*.c`, `lib/bs-touchscreen*.c` (9 files) | absent | **gap** |
| `desktop` plugin (keyboard-shortcut action + capture dialog) | 6 files | absent | **gap** |
| `obs-studio` plugin (6 actions, connection manager/settings) | 18 files | absent | **gap** |
| `soundboard` plugin (MPRIS play action + prefs) | 9 files | absent | **gap** |
| `data/` (desktop file, metainfo, gschema, icons) | present | absent | **gap** |
| Every unit's styling (Step 4) | (cross-cutting) | not run | **unverified** |
| Every unit's behaviour (Step 5) | (cross-cutting) | not run | **unverified** |
| Text axis, all units | (cross-cutting) | 0/147 | **gap** (see translation-parity.md) |

## Rules check against this report itself

- "Never report parity from the scan alone." — honoured: this report asserts
  **zero** parity, specifically because Steps 4/5 did not run, rather than
  inferring parity from the scan's near-equal widget-type count.
- "Never compare app-wide totals and call it parity." — the app-wide signal
  count (20/50) is reported as a symptom, not as the verdict; the verdict is
  the per-area gap table above and the 0-drive finding.
- No unit in this report is marked `parity`. None has been driven.
