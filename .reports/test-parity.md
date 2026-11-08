# Test parity — Boatswain (refresh 2)

| | |
|---|---|
| Upstream | `main` @ `0262d32` |
| Port | `ruby` @ `93105a4` |
| Upstream tests | 0 |
| Ported | 0 |
| Gaps | 0 |
| Port's own `spec/` suite | still cannot load (`kettle/test/rspec` unresolvable) |
| Port's own drive script | **now exists** (`test/drive_boatswain.rb` + `test/gtk_driver.rb`) — but is flaky |

## Step 1 — upstream census (unchanged)

Re-confirmed this round: upstream ships no test suite (0 `g_test_add_func`,
no `tests/` directory; the three `meson.build` `test(...)` entries are
build-time desktop/metainfo/schema validators, not behavioural tests, and
the skill's census correctly excludes them). **Test parity on the
upstream⟷port axis is still met trivially at 0/0.** As last round: this
means no safety net is being silently dropped from upstream, and also that
none is being inherited for free.

## Step 2 — the port's own suite: a real driver now exists

This is the one significant change since the last `test-parity.md`.
`test/drive_boatswain.rb` (779 lines) and `test/gtk_driver.rb` (214 lines,
a small reusable GTK-driving harness) did not exist at the last audit round
and now do. This closes the single largest gap that round identified: "the
drive script the devshell banner points at does not exist."

```sh
$ find test -type f
test/drive_boatswain.rb
test/gtk_driver.rb
$ nix develop --command xvfb-run -a ruby test/drive_boatswain.rb
...
148 of 148 checks passed
DRIVE OK
```

42 `d.step` blocks drive the application through startup, the window, the
shortcuts/about dialogs, the primary menu and its accelerators, profile
switching, the multi-action editor, the network plugin's HTTP action, the
gaming plugin's score action, the device editor, the dial grid and
touchscreen widgets, GSettings persistence, profile save/reload round-trips,
and a corrupt-profile recovery path, with 9 screenshots written to
`tmp/shots/`. This is genuine Step 5 ("drive it, assert on live state")
coverage for a meaningful slice of the app — a real advance, and it should
be named as one rather than buried under the finding below.

## The finding this round adds: the driver is not deterministic

Re-running the identical command, against the identical commit, with
nothing changed between runs, seven times:

```
run 1:  142/148 — DRIVE FAILED (6)
  failed check: the button's action is the Folder action
  failed check: its icon followed
  failed check: the view popped back to the button page
  failed check: the remove-action row is visible now there is an action
  failed check: the device pushed a page
  failed check: its action is Go Up
runs 2-7:  148/148 — DRIVE OK, every time
```

**1 run in 7 failed.** All six failing checks sit inside one interaction —
assign an action to a button, then navigate and check the follow-on state —
which is the textbook shape of a GTK main-loop race: a `d.step` tick firing
a check before an earlier step's signal chain (action-assignment →
icon update → navigation → row visibility) has actually settled. This is
not a flaky *test* in the sense of "asserts on randomness" — it is evidence
that the thing being asserted (the button's action took effect, the page
was pushed) is not yet reliably true on every run, only on most runs.

This matters for exactly the reason the task framing calls out: **a
"148 checks passed, 0 failed" runtime gate is being treated as this round's
ground truth, and it is not reproducible as stated.** Re-running it is
cheap and this audit did so before accepting the number. Any report that
repeats "148/148, DRIVE OK" as settled, full stop, without noting that the
same command failed 6 checks on one of seven identical re-runs, would be
reporting a result that happened to land on this particular invocation, not
a property of the code. The commit immediately before this report
(`93105a4`, "Fix the profile sidebar going stale on delete; settle widget
tree before screenshotting") already shows this exact race has needed one
fix in this area before — this round's finding is that it is not fully
closed.

## `spec/` — unchanged, still broken

```sh
$ find spec -type f
spec/README.md
spec/spec_helper.rb
$ nix develop --command bash -c 'ruby -e "require \"rspec/core\"; \
    exit RSpec::Core::Runner.run([\"--format\",\"progress\", \
    \"--require\",\"spec_helper\",\"spec\"])"'
LoadError: cannot load such file -- kettle/test/rspec
0 examples, 0 failures, 2 errors occurred outside of examples
```

Identical failure to the prior round, byte for byte. Zero `_spec.rb` files
exist regardless, so even a working `spec_helper.rb` would run zero
examples. This is a second, independent test-infrastructure gap sitting
alongside the drive script — the drive script's existence does not fix it,
and the two should not be conflated: `test/` now works (mostly); `spec/`
still does not work at all.

## What this report is not allowed to say

Not "0/0 upstream parity, and the port now has a driver, so test parity is
clean." Three facts belong in the ledger together, as last round insisted:

- **Test parity (upstream ⟷ port): 0/0, met trivially.** Unchanged.
- **The port's `spec/` harness: still broken**, identical to the prior
  round — `kettle/test/rspec` unresolvable, zero spec files regardless.
- **The port's `test/drive_boatswain.rb`: exists, covers real behaviour,
  and is not deterministic** — it failed 6 checks on 1 of 7 identical
  re-runs this audit performed. Reporting it as simply "DRIVE OK" without
  that caveat would be the false green this audit's accountability pass
  exists to catch, one level removed from a sentence and into a number.

## Coverage this driver does not and cannot reach

Per the task's framing ("a component no check exercises is unverified, not
done"), the following have no `d.step` anywhere in `test/drive_boatswain.rb`
and so remain entirely unverified by any automated check, regardless of the
148-check total: `desktop`, `obs-studio`, `soundboard` plugins (all absent,
so trivially untested); real Elgato/Loupedeck HID I/O (absent); the
`.gschema.xml`/`.desktop`/metainfo content (absent); any non-English locale
behaviour (absent, see `.reports/translation-parity.md`); and a byte-level
diff of `lib/boatswain/style.rb` against upstream's `src/style.css` (Step 37
only checks the port's own CSS parses, not that it matches upstream's).

## Extra

`tmp/shots/*.png` (9 files) are written by the drive run and are evidence,
not test artifacts to assert against — nothing in the driver reads them
back or diffs them against a reference image.
