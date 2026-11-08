# Component parity — Boatswain (refresh 2)

| | |
|---|---|
| Upstream | `main` @ `0262d32` |
| Port | `ruby` @ `93105a4` |
| This round's drive result (as reported by the harness) | drive_ok=true, 148/148 checks, 41 components |
| This round's drive result (independently re-run, 7×) | **148/148 passed 6 of 7 times; 1 of 7 runs failed 6 checks** |
| Units with full (4-axis, driven) parity | still **0 / ~58**, for a reason below that is new this round |

## The headline finding this round: the gate is not the solid floor it reads as

The task framing for this audit treats `drive_ok=true, 148 checks passed, 0
failed` as a fixed fact about this round. It is not reproducible as stated.
Re-running the exact same command against the exact same commit
(`xvfb-run -a ruby test/drive_boatswain.rb`), with no code changes between
runs, this audit got:

```
run 1: 142 of 148 checks passed
  failed check: the button's action is the Folder action
  failed check: its icon followed
  failed check: the view popped back to the button page
  failed check: the remove-action row is visible now there is an action
  failed check: the device pushed a page
  failed check: its action is Go Up
DRIVE FAILED (6)

runs 2-7: 148 of 148 checks passed, DRIVE OK  (6 further runs, all clean)
```

One run in seven failed, on identical code, with no flag changed. All six
failing checks cluster around one interaction (assigning an action to a
button, then navigating back to the button page and checking the remove-
action row and a Go Up follow-on) — the shape of a **race**: a GTK main-loop
tick firing a check before a signal it depends on has actually settled, not
a deterministic bug the harness would catch every time. The `93105a4` commit
message itself says the prior fix was for "the profile sidebar going stale
on delete" and "settle widget tree before screenshotting" — i.e. this exact
class of race has already needed one fix in this file, and this round found
it is not fully closed.

**This is reported as the primary finding, not a footnote, because of what
it does to the number everyone downstream treats as ground truth.** A gate
that is "green" 6 times out of 7 is not the same claim as a gate that is
green, full stop — the one failing run is not a fluke to discard, it is
positive evidence that the thing being asserted ("the button's action
followed", "the page was pushed") is not yet reliably true, only usually
true. Per the task's framing, *this counts as "a failed or narrow drive
run," itself a parity gap* — the race is a real behavioural gap in the
button-editor/navigation interaction, independent of the component-file
tables below.

## What the drive run actually proves, and does not

41 `d.step` blocks (42 by raw count, one of which is the mock-device-provider
setup, not a user-facing component) now exercise the application, the
window, the shortcuts/about dialogs, the primary menu, profile switching,
multi-action editing, the network plugin, the device editor, the dial grid
and touchscreen slot widgets end-to-end, and a corrupt-profile-file recovery
path — a large, genuine advance on the prior round's "`exe/boatswain` was
launched once, nothing was clicked, no assertion was made." This is real
headless UI testing of real widgets, screenshotted, and it is the right
shape of evidence per the `component-parity` skill's Steps 4/5.

It still proves parity for **zero units at the "4-axis, driven" bar**,
because:

1. The one race found above means even the units the driver does cover are
   not yet reliably passing — "usually passes" is not "parity."
2. The driver only covers the plugins and widgets that exist. It cannot and
   does not exercise `desktop`, `obs-studio`, `soundboard`, real Elgato/
   Loupedeck device I/O, or any `data/`/`po/` content, because none of that
   exists to drive (see `.reports/component-identification.md`). **A
   component no check exercises is unverified, not done** — stated per this
   audit's own framing, and it is the correct reading of every row in the
   "absent" table below.
3. No per-unit axis table (widget/CSS/signal/behaviour, one row per
   component) has been rebuilt against the new dial/touchscreen files or the
   `HardwareDetector`, for the same reason as last round: doing so from the
   `.rb` source without the symbol table rebuild is a reading, not a scan.

## File mapping (updated)

```
src/bs-application.c                          -> lib/boatswain/application.rb        driven
src/bs-window.c + .ui                          -> lib/boatswain/window.rb             driven
Adw.AboutDialog / ShortcutsDialog equivalents  -> lib/boatswain/{about,shortcuts}_dialog.rb  driven
lib/bs-device*.c, bs-profile.c, bs-page*.c     -> lib/boatswain/{device,profile,page}*.rb    driven
lib/bs-button*.c                                -> lib/boatswain/button*.rb            driven, but see the race above
lib/bs-dial*.c, bs-touchscreen*.c (NEW)        -> lib/boatswain/{dial,touchscreen}*.rb  driven (step 40)
lib/bs-context.c                                -> lib/boatswain/context.rb            driven (4/6 plugins wired)
lib/devices/mock/*.c                            -> lib/boatswain/devices/mock/*.rb      driven
lib/devices/elgato/*.c, loupedeck/*.c (HID)     -> (absent — HardwareDetector covers enumeration only, not HID)
src/plugins/default/*, launcher/*, network/*   -> lib/boatswain/plugins/{default,launcher,network}/*.rb  driven
src/plugins/gaming/*.js                         -> lib/boatswain/plugins/gaming/*.rb    driven (step: score action counts up/down/resets)
--- no port file on the right ---
src/plugins/desktop/*                           -> (absent)
src/plugins/obs-studio/*                        -> (absent)
src/plugins/soundboard/*                        -> (absent)
data/*.gschema.xml, *.desktop.in, *.metainfo*  -> (absent)
po/*                                             -> (absent)
```

Still **6 of upstream's roughly 13 top-level areas with no port file at
all** — unchanged in count from the prior round; the dial/touchscreen row
moved from "absent" to "present and driven," which is the one area that
actually improved.

**Correction while drafting this report:** an earlier pass of this audit
claimed the `gaming` plugin was not exercised by the driver. Re-checked by
grep before publishing — it is: `test/drive_boatswain.rb` has a step
exercising `Boatswain::Plugins::Gaming::ScoreAction` counting up, down and
resetting. That claim is withdrawn here rather than left uncorrected.

## Stylesheet

```
$ find . -iname '*.css'
(no output)
```

Step 37 of the drive script does assert `Style's CSS parses cleanly, with no
errors swallowed` — a real improvement on the prior round's "not diffed at
all." But parsing cleanly is not the same claim as matching upstream's
`src/style.css` content; that diff still has not been done.

## Text axis

Unchanged: see `.reports/translation-parity.md` — 0/147 messages, 0/38
languages. Every unit's text axis is still an automatic gap.

## Gaps (component-level, this round)

| Area | Upstream | Port | State |
|---|---|---|---|
| Elgato Stream Deck HID I/O (10 models) | `lib/devices/elgato/*` (15 files) | enumeration only (`HardwareDetector`) | **gap** |
| Loupedeck HID I/O | `lib/devices/loupedeck/*` (5 files) | enumeration only | **gap** |
| `desktop` plugin | 6 files | absent | **gap** |
| `obs-studio` plugin | 18 files | absent | **gap** |
| `soundboard` plugin | 9 files | absent | **gap** |
| `data/` (desktop file, metainfo, gschema, icons) | present | absent | **gap** |
| Button-editor navigate-back-after-assigning-action flow | — | races ~1/7 runs | **gap (flaky, not fixed)** |
| Dial/touchscreen per-widget CSS/signal table | 7 new files | not rebuilt | **unverified** |
| Style.css content vs `lib/boatswain/style.rb` | present | parses clean, not diffed | **unverified** |
| Text axis, all units | (cross-cutting) | 0/147 | **gap** |

## Rules check against this report itself

- "Never report parity from the scan alone." — honoured: the race was found
  by re-running the actual driver, not by reading source.
- "A component no check exercises is unverified, not done." — applied to
  `gaming`, to the three absent plugins, and to the HID-I/O half of the
  device providers.
- "Treat a failed or narrow drive run as a parity gap in its own right." —
  this round's own re-runs produced exactly that failed run, and it is
  reported as the headline finding, not smoothed into "148/148, DRIVE OK"
  because six of seven attempts said so.
- No unit in this report is marked `parity`. None has been driven reliably
  enough to earn it.
