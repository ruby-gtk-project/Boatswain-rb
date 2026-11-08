# Boatswain — component identification (refresh 2)

Produced with the `component-identification` skill against:

| | |
|---|---|
| Upstream | `origin/main` @ `0262d32` (unchanged since the prior refresh) |
| Port | `ruby` @ `93105a4` |

This supersedes `.reports/component-identification.md`'s previous pass
(against `7855f51`) and `.reports/component-inventory.md`'s original survey
(against `3d10462`). Six commits landed since the last audit round
(`c079cad` "Parity reports" → `93105a4`):

```
6496d6e  Port the actionable interface, dial, dial grid, touchscreen slot and device update models
fd8f237  Make gudev and ashpd genuinely called; drop glycin as unused
f8588e9  Fix a reentrant-signal hang in ButtonEditor that silently blocked the full UI drive, and add real gudev/ashpd coverage to it
680dc2f  Inventory upstream components
817b242  Revert "Inventory upstream components"
b3bd70c  Port the dial and touchscreen widgets, and the touchscreen region model
93105a4  Fix the profile sidebar going stale on delete; settle widget tree before screenshotting
```

## What changed since the last survey

The dial/touchscreen gap the prior two rounds both called "entirely absent"
is now filled:

```
$ find lib -iname '*dial*' -o -iname '*touchscreen*'
lib/boatswain/dial.rb
lib/boatswain/dial_grid.rb
lib/boatswain/dial_widget.rb
lib/boatswain/touchscreen.rb
lib/boatswain/touchscreen_slot.rb
lib/boatswain/touchscreen_slot_widget.rb
lib/boatswain/touchscreen_widget.rb
```

A `Devices::Usb::HardwareDetector` was also added
(`lib/boatswain/devices/usb/hardware_detector.rb`), using the real `gudev`
gem to enumerate Elgato/Loupedeck/Razer vendor:product pairs over USB.
**Read its own header comment carefully before counting it as a device
provider**: it says plainly that it is "the read-only half" of upstream's
`elgato-device-provider.c` / `loupedeck-device-provider.c`, that "the libusb
half (opening the device file, claiming the interface, uploading key
images)... is not part of this port", and that `Device#create_layout` "has
no subclass that owns a HID handle, only `Devices::Mock::Device`." This is
the port's own documentation of a gap, not a report's inference — it genuinely
enumerates USB devices now, and it genuinely still cannot drive one.

`test/drive_boatswain.rb` and `test/gtk_driver.rb` now exist (they did not at
the last audit round) — see `.reports/test-parity.md` for what they do and do
not prove.

## Still absent, unchanged across all three survey rounds

```
$ ls lib/boatswain/plugins
default  gaming  launcher  network
$ grep -c 'Plugins::Desktop\|Plugins::ObsStudio\|Plugins::Soundboard' lib/boatswain/context.rb
0
$ ls data po PORTING.md 2>&1
ls: cannot access 'data': No such file or directory
ls: cannot access 'po': No such file or directory
ls: cannot access 'PORTING.md': No such file or directory
$ find lib -iname '*elgato*' -o -iname '*loupedeck*' | grep -v usb/hardware_detector
(no output beyond the enumerator above)
```

- **`desktop` plugin** (keyboard-shortcut action + capture dialog) — 0 files.
- **`obs-studio` plugin** (connection manager, scene/source actions,
  record/stream/switch-scene/toggle-source/virtualcam actions, connection
  settings dialog) — upstream's largest single plugin (18 files) — 0 files.
- **`soundboard` plugin** (MPRIS controller, play action + preferences) — 0
  files.
- **Elgato and Loupedeck device *providers*** (the part that actually talks
  HID to a physical Stream Deck/Loupedeck and renders key images to it) —
  0 files. The port can now *notice* such a device over USB
  (`HardwareDetector`) but still cannot *drive* one; `Device#create_layout`
  still only has a `Devices::Mock::Device` subclass.
- **`po/`** — 0 files, 0 gettext call sites anywhere in `lib/`/`exe/` (see
  `.reports/translation-parity.md`, unchanged).
- **`data/`** — no `.desktop` file, no AppStream metainfo, no GSettings
  schema, no icons. `lib/boatswain/settings.rb` still has no `.gschema.xml`
  for its one key to bind against.
- **`spec/`** — still only `spec/README.md` and `spec/spec_helper.rb`; zero
  `_spec.rb` files, and the helper still cannot load (`kettle/test/rspec`
  unresolvable) — see `.reports/test-parity.md`.

## Scan totals (not rebuilt this round)

The prior round's scan caveat stands: `scripts/symbols.tsv` was not rebuilt
in the dev shell for Boatswain's own GIR set (`Gudev`, `Soup3`, `Ashpd`
typelibs), so a fresh widget-type/signal/CSS scan this round would carry the
same "defaults to `widget`" distortion already documented in the superseded
report. Rather than re-run a scan known to be unreliable and repeat the same
caveat, this round instead re-verified the finding that *does* hold up
without a symbol table — file-level presence/absence, which is a grep, not a
classification — and re-ran the actual drive (see `.reports/test-parity.md`
and `.reports/component-parity.md`) for the axes the scan cannot settle
anyway (styling, behaviour).

## Rules checklist (self-audit)

- [x] Compared against the same upstream ref as the prior two rounds
  (`0262d32`), so deltas are real deltas, not ref drift.
- [x] Every "still absent" claim re-verified this round by command, not
  carried over from memory of the prior report.
- [x] The one genuinely new file (`HardwareDetector`) is not counted as
  closing the device-provider gap — its own comment is quoted, not
  paraphrased into something rosier.
- [ ] Full per-widget CSS/signal table for the dial/touchscreen files —
  **not redone this pass**; `component-parity.md` below carries this
  forward as its own open item, same as last round's network/gaming files.
