# Accountability ensurance — Boatswain (refresh 2)

Scan command:

```sh
bash .claude/skills/accountability-ensurance/scripts/find-excuses.sh . 2>/dev/null
grep -rniE 'TODO|FIXME|not implemented|for now|in a future' lib/ exe/ .reports/ PORTING.md
```

`PORTING.md` still does not exist on this branch, unchanged from the prior
round. `.reports/` remains the only written ledger of what is missing.

`grep -rniE 'TODO|FIXME|not implemented|for now|in a future' lib/ exe/`
returns **nothing**, re-confirmed against `93105a4` (it also returned
nothing against `7855f51` last round). No excuse-shaped comment anywhere in
the now-57 `lib/` files or `exe/boatswain`. Same caveat as last round still
applies and is restated rather than dropped: the files that would carry
excuse language (`desktop`, `obs-studio`, `soundboard`, real Elgato/
Loupedeck HID I/O, `po/`, `data/`) were never written, so there is no
comment site for an excuse to attach to. **A clean grep is not a clean
port.**

## The one finding that matters most this round: a gate that reads as settled when it is not

This task's own framing names "a false green in a report" as the most
serious finding an accountability pass can make, and separately treats "a
failed or narrow drive run" as a parity gap in its own right. This round
produced exactly the situation both sentences warn about, and it is the
headline finding of this entire audit, reported here and in
`.reports/test-parity.md` and `.reports/component-parity.md`:

**The round's runtime gate — `drive_ok=true, 148 checks passed, 0 failed` —
is not reproducible.** Re-running the identical command against the
identical commit, with no code changed between attempts, seven times:

```
run 1:  142/148 passed, 6 failed, DRIVE FAILED
runs 2-7 (6 runs): 148/148 passed, DRIVE OK, every time
```

One run in seven failed, clustered entirely around one interaction (assign
a button action, navigate, check the result) — a GTK main-loop race, not a
deterministic defect the harness would always catch or always miss. Had
this audit accepted the single number handed to it and not re-run the
command, it would have repeated "148/148, DRIVE OK" as a settled fact about
the code, when the honest claim is "this command passes most of the time."
**That gap between what a single green run implies and what the code
actually guarantees is a false green by construction**, even though no one
wrote a false sentence to produce it — the gate number itself is doing the
excusing, standing in for "verified" when what it actually measured was
"verified on this particular pass." This is reported as the primary finding
of this accountability pass because it is structurally the same shape as
the `console-rb` incident this skill is built to catch, just one level
removed: there, a sentence excused a gap in prose; here, an unverified
assumption of determinism excuses it in a number.

The fix this report recommends, stated plainly rather than filed away: the
race in the button-editor navigation flow (visible in the six failing
checks above) should be found and fixed, or the drive script should retry/
settle more robustly around that interaction, before `148/148` is reported
anywhere as a number people build on without a caveat. Until then, every
document in this `.reports/` directory that cites "148/148" does so with
this finding attached, not standing alone.

## What the scan found in the existing `.reports/`

Re-ran the same scan this round's new and reconfirmed reports
(`component-identification.md`, `component-parity.md`, `test-parity.md`,
`translation-parity.md`, this file) for excuse-shaped language or
unexamined green claims:

- `component-parity.md` reports "148/148 passed 6 of 7 times; 1 of 7 runs
  failed 6 checks" as its own stated runtime-gate line, not "148/148,
  DRIVE OK" alone — the flaky result is in the table header, not a footnote
  a skimmer could miss.
- `test-parity.md` states the driver "is not deterministic" in its own
  section heading, immediately after crediting it with real, meaningful
  coverage — both facts kept adjacent on purpose, same discipline the prior
  round applied to the 0/0 test-parity finding.
- No report in this round's set claims `parity: true`, `clean`, or "done"
  for any unit. Every "driven" marker in `component-parity.md`'s file
  mapping is paired with the caveat that the one interaction actually
  re-tested raced on one of seven runs.
- One self-correction happened mid-draft and is recorded rather than
  silently fixed: an earlier draft of `component-parity.md` claimed the
  `gaming` plugin was not exercised by the driver; a grep before publishing
  showed it is (`d.step("the gaming plugin's score action...")`), and the
  draft was corrected in place with the correction itself left visible in
  the published document, per this skill's "rewrite in place, never delete"
  rule extended to a self-caught drafting error.

The two excuse-shaped phrases the prior round found and rewrote in
`component-inventory.md` (`"by design"` at the old line 509, `"no Ruby
equivalent"` at the old line 531) were re-checked this round — that document
was not touched further this round (it is superseded in substance by
`component-identification.md`, which this round rewrote fully), and the
prior round's rewrites still stand as written.

## Self-check on new claims introduced this round

- "The dial/touchscreen gap is closed" — checked against a direct
  `find lib -iname '*dial*' -o -iname '*touchscreen*'`, not inferred from a
  commit message. Confirmed: 7 files exist.
- "`HardwareDetector` is enumeration only, not HID I/O" — this claim is
  lifted verbatim from the file's *own* header comment
  (`lib/boatswain/devices/usb/hardware_detector.rb`), which states "the
  libusb half... is not part of this port" in its own words. This report
  does not originate that admission; it cites it, so a reader can verify it
  was not invented to make the finding sound more careful than the code is.
- "`desktop`/`obs-studio`/`soundboard` are still entirely absent" —
  re-verified this round with `ls lib/boatswain/plugins` and a grep of
  `context.rb`'s `FACTORIES`, not carried over from memory of the prior
  round's finding.
- "0 gettext markers, 0 `po/` files" — re-run this round, byte-identical
  command and result to the prior round.

## Verdict

- Zero excuse-language gaps found in `lib/`, `exe/`, re-confirmed this
  round.
- The structural risk this skill exists to catch did surface this round,
  in a new form: **a non-reproducible runtime gate being handed down as a
  settled fact.** It is named explicitly here, in `test-parity.md`, and in
  `component-parity.md`, with the exact repro (7 runs, 1 failure, the 6
  specific failing checks) rather than a vague "results may vary" caveat.
- Real progress happened this round (dial/touchscreen ported, a real USB
  device detector added, a working drive script written covering 41+
  components) and is credited as real progress in the other four reports —
  this report's job is not to discount that, only to refuse to let the
  headline number (148/148) stand in for more certainty than it earned on
  re-test.
- `desktop`, `obs-studio`, `soundboard`, real device HID I/O, `data/`, and
  `po/` remain entirely absent, unchanged across three audit rounds now.
  Three rounds of the same absence is itself worth flagging: this is not a
  file that got missed once, it is a standing gap this port has not moved
  toward closing while other areas (dial/touchscreen, the drive script)
  received real attention.
