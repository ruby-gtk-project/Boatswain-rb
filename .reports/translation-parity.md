# Translation parity — Boatswain (reconfirmed at `93105a4`)

| | |
|---|---|
| Upstream | `main` @ `0262d32` (unchanged) |
| Port | `ruby` @ `93105a4` |

**Reconfirmed this round, no change.** Six commits landed since this report
was first written (`7855f51` → `93105a4`: the gaming plugin was already in
by the time this was written; since then the dial/touchscreen widgets and a
USB hardware detector were ported). None of them touch `po/`, add a gettext
dependency, or mark a single string. Every command below was re-run against
current `HEAD` and produced the identical result:

```sh
$ find . -iname '*.po' -o -iname '*.pot'
(no output)
$ grep -rn '\bgettext\b\|\b_(\|\bN_(\|\bn_(' lib/ exe/
(no output)
```

The table and ledger below are the original findings, verified unchanged.
| Domain | `boatswain` (`po/meson.build`) |
| Upstream messages | 147 |
| Ported | **0** |
| Gaps | **147** |
| Extra (untranslated) | 0 |
| Upstream occurrences | 165 across 147 messages (15 reused) |
| Port occurrences | 0 |
| Upstream languages | 38 `.po` files, 5,515 translated, 54 untranslated, 1 fuzzy |
| Languages shipped by the port | **0** |
| Unresolved markers (upstream) | 5 — see below |
| Build command | none — no gettext dependency exists in the port |

```
147/147 messages MISSING · 165/165 occurrences MISSING · 38/38 languages MISSING
0 strings translated · no non-English locale check run (nothing to check)
```

## Step 1 — census

```sh
scripts/catalogue.rb <upstream worktree> --role upstream > upstream-catalogue.yaml
scripts/catalogue.rb .                   --role port     > port-catalogue.yaml
scripts/catalogue.rb --compare upstream-catalogue.yaml port-catalogue.yaml
```

Upstream: **147** distinct `(msgctxt, msgid)` keys, **165** call sites (15
messages reused — `File` at 4 sites, `Boatswain`/`Brightness`/`Delay`/
`Elgato Stream Deck`/`No Background`/`Open`/`Remove Action`/`Rename`/
`Select Action` at 2 each), **1 plural** (`Connected to %d device`), **3**
with a `msgctxt` (all `"shortcut window"`: `General`, `Quit`,
`Show Shortcuts`).

**5 unresolved markers** — arguments that are not string literals, so the
census cannot read them without a human following the indexed table:

```
lib/bs-action-factory.c:275-276   gettext(entry->name), gettext(entry->description)
lib/devices/mock/mock-device.c:248   _(model_info->name)
src/bs-touchscreen-background-dialog.c:84-85   _(builtin_groups[i].title)  (×2 call sites)
```

These resolve to strings already captured elsewhere in the table by their
literal form (action names/descriptions registered by each plugin's factory,
the mock device model names, the built-in touchscreen background titles) —
they are not additional undiscovered messages, but they are messages this
census could only find by reading the indexed struct by hand, and that
reading has not been done to confirm every one maps onto an existing row
rather than hiding a 148th message. Flagging per the skill's rule: **a
ledger may not be written while `totals.unresolved` is non-zero without
hand-resolving each one**; this report states them rather than silently
rounding to 147 as final.

Upstream languages: `po/LINGUAS` (38 entries) matches the 38 `.po` files on
disk exactly (`linguas_matches_files: true`), plus `po/boatswain.pot`.
**5,515 translated, 54 untranslated, 1 fuzzy strings.**

## Step 2 — the port

```sh
$ scripts/catalogue.rb . --role port
totals: {messages: 0, occurrences: 0, reused: 0, plural: 0, with_context: 0, unresolved: 0}
languages: {count: 0, list: [], translated_strings: 0}
```

**There is no `po/` directory in the port at all.** No `.po`, no `LINGUAS`,
no `.pot`, no `POTFILES.in`, and grep confirms no gettext marker (`_()`,
`N_()`, `gettext`) call exists anywhere in `lib/` or `exe/`:

```sh
$ find . -iname '*.po' -o -iname '*.pot'
(no output)
$ grep -rn '\bgettext\b\|\b_(\|\bN_(\|\bn_(' lib/ exe/
(no output)
```

Every user-visible string the port currently builds is a bare Ruby literal.
This is not a case of strings marked with the wrong key — **nothing is
marked at all**, so the state for every one of upstream's 147 messages is
`gap`, and none of the "reworded"/"retyped" nuance applies because there is
no port-side string to compare word-for-word in the first place.

## Summary (generated)

```yaml
summary:
  ported: 0
  gaps: 147
  extra: 0
  kind_mismatch: 0
  uses_mismatch: 0
  occurrences: {upstream: 165, port: 0}
  languages: {upstream: 38, port: 0}
  domain_matches: false
  parity: false
```

## Ledger — gaps, grouped by upstream source area

One row per upstream message would be 147 rows; grouped by area instead,
full message text is in `/tmp/up-cat.yaml` (`scripts/catalogue.rb` output,
reproducible by rerunning Step 1) and nothing here is summarized away — every
message counted is listed by area below, with the highest-reuse ones named
individually since they carry the most weight.

| Area | Upstream file(s) | Messages | State |
|---|---|---:|---|
| Window chrome, profile row, button/device editors | `src/bs-window.ui`, `bs-profile-row.ui`, `bs-button-editor.ui`, `bs-device-editor.ui`, `bs-touchscreen-slot-editor.ui` | ~38 | **gap** (all) |
| Touchscreen background dialog (6 built-in background names + dialog strings) | `src/bs-touchscreen-background-dialog.{c,ui}` | ~9 | **gap** — and the dialog itself has no port file at all (component-parity.md), so this is a gap with no component to even wait on |
| `default` plugin (brightness, multi-action, switch-profile, page-up) | `src/plugins/default/*` | ~20 | **gap** — plugin exists in the port (`lib/boatswain/plugins/default/`) but carries zero translation markers |
| `launcher` plugin | `src/plugins/launcher/*` | ~7 | **gap** — same: ported logic, zero `_()` |
| `network` plugin | `src/plugins/network/*` | ~10 | **gap** — same |
| `gaming` plugin (score action) | `src/plugins/gaming/*.js` | ~8 | **gap** — same |
| `desktop` plugin | `src/plugins/desktop/*` | 6 | **gap**, waits on the component gap (plugin absent) |
| `obs-studio` plugin | `src/plugins/obs-studio/*` | ~26 | **gap**, waits on the component gap (plugin absent) |
| `soundboard` plugin | `src/plugins/soundboard/*` | ~18 | **gap**, waits on the component gap (plugin absent) |
| Device/model names (Elgato × 7, Loupedeck × 1, mock × 2) | `lib/devices/*`, `lib/bs-device.c`, `lib/bs-profile.c` | ~11 | **gap** — the mock device names (`Feaneron Hangar Mild`, `Feaneron Hangar XL`) are reachable today since the mock provider is ported, and are still unmarked |
| `data/com.feaneron.Boatswain.{desktop,metainfo,gschema}.*` | `data/*` | ~13 | **gap**, waits on the component gap (no `data/` tree in the port at all) |
| Shortcuts dialog (msgctxt "shortcut window") | `src/shortcuts-dialog.ui` | 3 | **gap** — `lib/boatswain/shortcuts_dialog.rb` exists and is unmarked |

High-reuse messages individually (most load-bearing if closed first, per the
skill's Step 2 metric 5 ordering):

| msgid | Uses (up) | Port site |
|---|---:|---|
| `File` | 4 | none |
| `Boatswain` | 2 | none |
| `Brightness` | 2 | none |
| `Delay` | 2 | none |
| `Elgato Stream Deck` | 2 | none |
| `No Background` | 2 | none |
| `Open` | 2 | none |
| `Remove Action` | 2 | none |
| `Rename` | 2 | none |
| `Select Action` | 2 | none |

## Step 3 — close the gaps (not done this round)

Not started. The four-line discipline from the skill (copy `po/` unchanged,
keep the `boatswain` domain, transcribe byte-for-byte, re-census) has not
been attempted. `po/LINGUAS`, the 38 `.po` files and `boatswain.pot` were
not copied into the port tree in this pass.

## Step 5 — prove it

All three required checks fail or are unrun:

- `summary.parity` is **false** (147 gaps, 38 missing languages, no domain).
- No `.mo` is built or installed — there is no gettext dependency in the
  gemspec/Gemfile at all, so `msgfmt`/`rmsgfmt` was never invoked. **This
  check could not run and is reported as unrun, not as passed.**
- Non-English locale check (`LANGUAGE=de LC_ALL=de_DE.UTF-8 ruby exe/boatswain`,
  asserting a known string) — **not run**, because there is nothing to
  assert: the app emits no string through any catalogue in any locale.

## Rules check against this report

- No gap closed by rewording upstream's msgid — none were touched.
- No `summary.parity: true` claimed; the generated YAML says `false` and
  this document repeats that number rather than rounding it up.
- Unresolved markers stated explicitly (5), not silently dropped from the
  147 total.
- "0 of 38 languages" stated as the headline finding, not buried under the
  147-message count, per the skill's explicit warning that a raw message
  count hides exactly this.
