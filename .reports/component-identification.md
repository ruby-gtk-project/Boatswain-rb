# Boatswain — component identification (refresh)

Produced with the `component-identification` skill against:

| | |
|---|---|
| Upstream | `origin/main` @ `0262d32` (worktree: "Update Catalan translation") |
| Port | `ruby` @ `7855f51` |

This is a refresh of `.reports/component-inventory.md` (the prior survey, still
valid as the per-unit widget tables) with the whole-tree scan numbers the
`component-parity` report below is built from, and an explicit account of what
changed since that survey (`3d10462` → `583ae64` → `8612c43`/`fe0ed3a`/
`3feda48` → `2f9b0d4` → `fc981bb` → `7855f51`).

## Scan totals

```
scripts/gir-symbols.sh   (environment-dependent; not rebuilt this run — see Caveat)
scripts/component-scan.sh <upstream worktree> > up-components.tsv    (427 rows, 72 files)
scripts/component-scan.sh .                    > port-components.tsv (250 rows, 21 files)
```

| Axis | Upstream distinct | Port distinct |
|---|---:|---:|
| Widget types | 55 | 52 |
| Signals | 50 | 20 |
| Actions | 7 | 8 |
| CSS classes | 21 | 21 |
| Files carrying any row | 72 | 21 |

**Caveat — read this before trusting the widget-type row.** No `symbols.tsv`
was rebuilt in the `nix develop` shell for this run (the checked-in
`scripts/symbols.tsv` is stale/empty for this app's own GIR set: Boatswain
pulls in `Gudev`, `Soup3` and `Glycin` typelibs that a generic table will not
know). Per Step 4 of the skill, every row that is not classified by a
symbol table defaults to `widget`, so **the port's "52 widget types" number is
inflated by its own enums, helper classes and non-widget GObjects** (e.g.
`Boatswain::ObservableList`, `Boatswain::Events`, `Boatswain::SelectionController`
are not GTK widgets and are almost certainly in that 52). The near-parity in
widget-type *count* is therefore not evidence of component parity — it is an
artifact of an unclassified scan — and the number that is not an artifact,
**signals (50 vs 20)**, is the one to read: the port connects fewer than half
of upstream's behaviour even where its widget types look present. This matches
the shape the skill calls out by name ("the UI laid out and the behaviour not
yet wired").

## What changed since the last survey (`.reports/component-inventory.md`)

That survey, written against `3d10462`, found 26/52 user-facing components
with a `lib/` file, all marked `exists (unverified)`, and called out as
entirely missing: the `desktop`, `obs-studio`, `soundboard` and `gaming`
plugins, the dial/touchscreen widgets, and `po/`.

Since then:

- `fc981bb` ported the **network** plugin's HTTP request action
  (`lib/boatswain/plugins/network/{action_factory,http_action,http_action_prefs}.rb`).
- `7855f51` ported the **gaming** plugin's score action
  (`lib/boatswain/plugins/gaming/{action_factory,score_action}.rb`), wired into
  `Context::FACTORIES`.
- `2f9b0d4` added `gudev`, `soup3`, `glycin`, `ashpd` as runtime deps (no new
  `lib/` files).

So of the four plugins the prior survey marked absent, two (`network`,
`gaming`) now have code; two (`desktop`, `obs-studio`) and a third
(`soundboard`) are still entirely absent — **confirmed again this run**:

```
$ find lib -iname '*dial*' -o -iname '*touchscreen*'
(no output)
$ ls lib/boatswain/plugins
default  gaming  launcher  network
$ grep -c 'Plugins::Desktop\|Plugins::ObsStudio\|Plugins::Soundboard' lib/boatswain/context.rb
0
```

`lib/boatswain/context.rb`'s `FACTORIES` list still only wires `Default`,
`Launcher`, `Network`, `Gaming` — 4 of upstream's 6 action plugins.

Still absent, unchanged from the prior survey:

- **Dial / touchscreen layer.** Upstream's `bs-dial.c`, `bs-dial-grid.c`,
  `bs-touchscreen.c`, `bs-touchscreen-content.c`, `bs-touchscreen-slot.c` —
  the whole Stream Deck Plus / Neo touch-strip and rotary-dial model — has
  zero `lib/` counterpart. No touchscreen background picker dialog either
  (upstream's `src/bs-touchscreen-content-dialog.*` equivalent).
- **`src/plugins/desktop/`** (keyboard-shortcut action + its shortcut-capture
  dialog) — 0 files.
- **`src/plugins/obs-studio/`** (connection manager, scene/source actions,
  record/stream/switch-scene/toggle-source/virtualcam actions, the
  connection-settings dialog) — 0 files. This is upstream's largest single
  plugin by file count (18 `.c`/`.h` files) and it has no port presence at
  all.
- **`src/plugins/soundboard/`** (MPRIS controller, play action + its
  preferences widget) — 0 files.
- **`po/`** — 0 files (see `.reports/translation-parity.md`).
- **Elgato and Loupedeck device providers** — upstream's real USB HID layer
  (`lib/devices/elgato/*`, 15 files covering 10 physical Stream Deck models,
  and `lib/devices/loupedeck/*`, 5 files). The port has only the **mock**
  provider (`lib/boatswain/devices/mock/{device,device_provider}.rb`). No
  physical Stream Deck or Loupedeck device can be driven by this port, at
  all, on any OS. This is not a parity nuance — it removes the app's entire
  reason to exist for anyone who owns the hardware.
- **No `data/` directory**: no `.desktop` file, no AppStream metainfo, no
  GSettings schema (`com.feaneron.Boatswain.gschema.xml`'s one key,
  `desktop-controller-restore-token`, has no port-side settings file despite
  `lib/boatswain/settings.rb` existing — see below), no icons.

`lib/boatswain/settings.rb` exists but there is no `.gschema.xml` in the port
for it to bind against — a settings wrapper with no schema to read is
unverified by construction, not just by omission of a test.

## Per-file tables

Unchanged from `.reports/component-inventory.md` for every file that survey
already covered; that document's tables are not reproduced here to avoid
drift between two copies of the same claim. Net new since that survey (the
network and gaming plugin files) were not re-tabulated per-widget in this
pass — see the Gaps list in `.reports/component-parity.md` for why their
component-parity status is still `unverified`, not `parity`.

## Rules checklist (self-audit of this report)

- [x] Both trees scanned with the same script.
- [x] Scan read as a lead, not a verdict — the symbol-table caveat above is
  stated rather than silently absorbed into the headline numbers.
- [ ] Per-component CSS/signal/action tables for the two newest files
  (network, gaming) — **not done this pass**; counted only in the aggregate.
  This is itself a gap in this report's own completeness, carried into
  `component-parity.md`.
