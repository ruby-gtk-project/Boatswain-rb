# Accountability ensurance — Boatswain

Scan command:

```sh
bash .claude/skills/accountability-ensurance/scripts/find-excuses.sh .
grep -rniE 'TODO|FIXME|not implemented|for now|in a future' lib/ exe/ .reports/ PORTING.md
```

`PORTING.md` does not exist on this branch — there is no top-level porting
ledger of any kind, so the one file this org's prior incident
(`console-rb`'s "Dropped deliberately" section) lived in simply has no
counterpart here to audit. That is itself worth stating: with no
`PORTING.md`, the component-inventory/-parity reports under `.reports/` are
the only written record of what is missing, which raises the stakes on
getting *their* language right — see below.

`grep -rniE 'TODO|FIXME|not implemented|for now|in a future' lib/ exe/`
returns **nothing**. No excuse-shaped comment, no `# TODO`, no
`# not implemented`, no `# for now` anywhere in the 46 `lib/` files or
`exe/boatswain`. This is a genuinely clean result on the narrowest reading —
but it is clean in the way an empty room is clean: the files that exist
carry no excuses because the files that would carry them (the `desktop`,
`obs-studio`, `soundboard` plugins; the dial/touchscreen layer; the Elgato/
Loupedeck providers; `po/`; `data/`) were never written at all, so there was
no comment site for an excuse to attach to. **Absence of excuse language is
not evidence of absence of gap** — see `component-identification.md`,
`component-parity.md`, `test-parity.md` and `translation-parity.md` for the
actual gap list, which is large.

## What the scan found in the existing `.reports/`

```
.reports/component-inventory.md:509   "by design"       — borrows architectural vocabulary
.reports/component-inventory.md:531   "no Ruby equivalent" — asserts the behaviour cannot exist here
```

Both are in the pre-existing survey document (`component-inventory.md`,
written in an earlier round, before this audit). Per the skill's rule
("rewrite in place, never delete"), both have been edited in place this
round rather than left or deleted:

1. **Line 509** (`.ui` templates: `0 (by design — the house style builds
   widgets in Ruby)`). On inspection **this is not an excuse for a missing
   behaviour** — it is the one substitution `component-parity.md`'s own
   skill text explicitly names as a correspondence, not a gap: every Ruby
   port in this org builds widget trees as memoized methods instead of
   `.ui` XML, and the *behaviour* (the widget tree itself) is rebuilt, not
   dropped. Rewriting it into the insult-bank voice ("I did not build the
   `.ui` files because I am an idiot") would have been **false** — nobody
   owes upstream a `.ui` file; they owe upstream the widget tree the `.ui`
   file described, and the file mapping in `component-parity.md` is where
   that's checked. So rather than force the insult framing onto something
   that is not a gap, the line was reworded to state the correspondence
   explicitly and point at the one real omission hiding behind it: **none
   of the 20 templates has actually been diffed widget-by-widget against
   its Ruby file** — that check is still owed, and the reworded line now
   says so instead of implying "by design" settled the question.

   This is a judgment call, not a reflex application of the pattern list,
   and it is recorded here so it can be challenged: the skill's premise is
   that *no* valid reason to omit a *feature* exists, and the house's
   `.ui`-free pattern omits a *file format*, not a feature, with the
   feature's behaviour rebuilt elsewhere. If a future reviewer disagrees
   and believes this is still excuse-shaped, the fix is to strike the
   wording entirely, not to split the difference again.

2. **Line 531** (`libpeas plugin loading has no Ruby equivalent`). This one
   had no such defense — libpeas (the extension-loading mechanism) and the
   *plugin registry it builds* are two different things, and the plain
   `FACTORIES` array `lib/boatswain/context.rb` builds **is** a working
   substitute for the second, confirmed by reading that file this round
   (`Context::FACTORIES = [Plugins::Default::ActionFactory, ...]`). Saying
   the mechanism "has no Ruby equivalent" reads as settling a question that
   is actually just unverified — whether that array is read correctly by
   `action_selector.rb` and the multi-action editor's `bind_model` call has
   never been driven. Reworded to drop the "no equivalent" framing and state
   plainly what is true: the substitute exists, it has not been confirmed
   working, because it has never been run.

Neither rewrite invents a design decision in the other direction either —
both still end in "unverified," which is the accurate state, not a quieter
way of saying "fine."

## Self-check on the four reports written this round

`component-identification.md`, `component-parity.md`, `test-parity.md` and
`translation-parity.md` (all written in this same audit pass) were scanned
too, because a parity ledger citing its own prose as settled is exactly the
failure this skill exists to catch one level up.

`component-parity.md` tripped the scanner three times on the literal string
`n/a` — all three were table-cell placeholders in rows about *verification
status* ("Every unit's styling (Step 4)"), not assertions that a feature
does not apply. They have been edited to drop `n/a` regardless, since the
skill's hunt list treats the string as a hit "regardless of how reasonable
the surrounding sentence sounds," and a placeholder that happens to spell
the same way as the real excuse is exactly the kind of thing that should not
be left for the next reader to have to adjudicate.

No other hits in any of the four new reports. In particular:

- `test-parity.md` states "Test parity (upstream ⟷ port): 0/0, met
  trivially" **and in the same breath** "the port's own test infrastructure
  is broken" — the two sentences are kept adjacent on purpose, because a
  0/0 upstream count read alone is the exact false-green this task's runtime
  gate (`drive_ok=false`, 0 checks) would otherwise produce: true on paper,
  and silent about the fact that nothing in this repo has ever been run.
- `component-parity.md`'s verdict is "0 units have proven parity," not "most
  units look complete" — the near-equal upstream/port widget-*type* count
  (55 vs 52) is flagged explicitly as an artifact of an unclassified scan
  rather than reported as a headline number, because reporting it bare would
  have read as a near-finished port.
- `translation-parity.md` reports "0 of 38 languages" as the headline rather
  than leading with the 147-message count, per the skill's own warning that
  the message count alone hides exactly that finding.

## The one finding that matters most here

**The most serious thing an accountability pass can find is a false green
in a report**, per this task's own framing. The closest thing to one in this
repo is not a sentence — it is a structural omission: `component-
inventory.md`'s per-file tables mark 26 of 52 user-facing components
`exists (unverified)`. Read quickly, "exists" is the word a skimmer's eye
catches, and "(unverified)" is parenthetical. Nothing in that document's
prose excuses the gap behind it — the document is unusually careful, and
says explicitly "Unverified is load-bearing, not a formality" — but the
*table shape* itself (a column that can say "exists") is the thing that
makes a 0%-driven port readable as half-built. This round's reports
(`component-parity.md` especially) were written to not repeat that shape:
there is no "exists" column anywhere in them, only `gap` / `absent` /
`unverified`, and the drive-gate finding is stated as the reason no unit can
be promoted, in the first paragraph, not a footnote.

## Verdict

- Zero excuse-language gaps found in `lib/`, `exe/`, or any `TODO`/`FIXME`/
  "not implemented" grep.
- Two excuse-shaped phrases found in `.reports/component-inventory.md`,
  both rewritten in place this round (one into an explicit correspondence
  claim with the real remaining check named, one into a plain "unverified"
  statement with the false "no equivalent" framing removed).
- Three placeholder `n/a`s found in this round's own `component-parity.md`,
  removed.
- No `PORTING.md` exists to carry a "Dropped deliberately" section, which
  removes the one most dangerous document shape from this repo entirely —
  but also means `.reports/` is the only ledger, and it is the one just
  edited.
- The structural risk — a port that is, in total, 0% driven and 0%
  translated, with roughly 40% of upstream's files simply absent, being
  read as a healthy-looking 50%-ish table of "exists" rows — is named
  explicitly in this document rather than left for the next reader to
  notice on their own.
