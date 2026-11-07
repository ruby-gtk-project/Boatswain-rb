# Test parity — Boatswain

| | |
|---|---|
| Upstream | `main` @ `0262d32` |
| Port | `ruby` @ `7855f51` |
| Upstream tests | 0 |
| Ported | 0 |
| Gaps | 0 |
| Test command (specs) | `ruby -e 'require "rspec/core"; exit RSpec::Core::Runner.run(["--format","progress","--require","spec_helper","spec"])'` — **does not run** (see below) |
| Drive command | `xvfb-run ruby test/drive_*.rb` — **the `test/` directory does not exist** |

## Step 1 — census the upstream suite

```sh
scripts/test-census.sh <upstream worktree>
```

returns **zero rows**. Cross-checked by hand:

```sh
find <upstream worktree> -iname '*test*' -not -path '*/.git/*' | grep -v -E '\.(svg|png)$'
-> data/icon-tester.blend          # a Blender asset, not a test
```

No `tests/` directory, no `g_test_add_func`, no GLib `Test.add_func`. The
only `test(...)` registrations anywhere in the build are three **build-time
validation checks** in `data/meson.build` — `Validate desktop file`,
`Validate metainfo file`, `Validate schema file` — which run `desktop-file-
validate`/`appstreamcli`/`glib-compile-schemas` against the packaged data
files. These are not unit or integration tests of app behaviour and the
skill's census deliberately does not count them (they assert nothing about
`BsDevice`, `BsButton`, the action plugins, or any behaviour a user sees).

**Per the skill's "When upstream has no tests" section: this is the
legitimate 0/0 case.** Boatswain upstream genuinely ships no test suite.
Parity here is met trivially, and the one-sentence finding that must not be
left implicit: **the port inherits no safety net from upstream.** Whatever
confidence this port has in its own correctness has to come from the port's
own tests — there is no upstream knowledge being silently dropped on this
axis, but there is also none being inherited for free.

## Step 2 — the port's own suite (the real finding)

This is where the round's runtime gate (`drive_ok=false`, 0 checks, 0
components covered) lands, and it is worse than "no inherited safety net":
**the port has no safety net of its own, and what little scaffolding exists
does not run.**

```sh
$ find spec -type f
spec/README.md
spec/spec_helper.rb
$ find test -type f
(directory does not exist)
```

- `spec/` contains only a harness README and a `spec_helper.rb`. **Zero
  `_spec.rb` files.** `rspec spec` would report "no examples" even if it
  could load.
- It cannot load, regardless:

  ```sh
  $ nix develop --command bash -c 'ruby -e "require \"rspec/core\"; \
      exit RSpec::Core::Runner.run([\"--format\",\"progress\", \
      \"--require\",\"spec_helper\",\"spec\"])"'
  ...
  Failure/Error: require "kettle/test/rspec"
  LoadError:
    cannot load such file -- kettle/test/rspec
  0 examples, 0 failures, 2 errors occurred outside of examples
  ```

  `spec/spec_helper.rb` requires `kettle/test/rspec`, which is not resolvable
  in the dev shell (the `kettle-test` gem the house `spec/README.md`
  documents is not present in the bundle this devshell builds). This is not
  a flaky test — it is the test harness itself failing to boot, confirmed by
  running the exact command the project's own `spec/README.md` and devshell
  banner document.

- `test/` — the directory the devshell banner points at
  (`drive the app: xvfb-run ruby test/drive_*.rb`) **does not exist at
  all**:

  ```sh
  $ nix develop --command xvfb-run -a ruby test/drive_app.rb
  /nix/store/.../ruby: No such file or directory -- test/drive_app.rb (LoadError)
  ```

  There is no `drive_*.rb` of any name under `test/`, and no `test/`
  directory under which one could be found. The `ruby-gtk-testing` skill's
  headless-driver pattern — the thing CLAUDE.md says proves any GTK change
  works, and the thing Step 5 of `component-parity` requires before any unit
  can be called `parity` — has never been written for this port.

This is the concrete shape of this round's runtime gate: **0 checks passed,
0 failed, 0 components covered** is not a flaky or incomplete run — it is
the correct result of there being no checks to run. `drive_ok=false` is
earned, not a measurement error.

## Step 3/4 — port the gaps / prove it

Nothing to port (0/0 upstream), and nothing to prove for the port's own
suite because there is no suite:

- counts: 0 upstream tests, 0 port tests under `spec/` or `test/` — vacuously
  "equal", but meaningless given neither side has content;
- the port's suite does **not** pass under its own test target, because
  loading it errors before a single example runs;
- the drive script named in the devshell banner does not exist, so "drive
  the app" cannot be attempted at all, let alone pass.

## What this report is not allowed to say

It would be easy to write "0/0, parity met, clean" and stop, because the
upstream number really is zero. That sentence, alone, is the false-green
this audit's `accountability-ensurance` pass exists to catch: it is true on
the upstream side and silently omits that **the port's own harness is
broken and its own driver script is missing**, which is the actual state
behind `drive_ok=false`. Both facts go in the ledger together:

- **Test parity (upstream ⟷ port): 0/0, met trivially.**
- **Port's own test infrastructure: broken.** `spec/` cannot load
  (`kettle/test/rspec` unresolvable), has zero spec files regardless, and
  `test/drive_*.rb` does not exist. No GTK code in this port has ever been
  exercised by an automated check. Every "unverified" marker in
  `.reports/component-inventory.md` and `.reports/component-parity.md`
  stays unverified because there is no mechanism in this repo that could
  move it to verified.

## Extra

None. There is nothing under `spec/` or `test/` to list.
