# Seraph — Design Brief v2 (target v0.2.0)

Status: DRAFT for review. Companion research: `seraph-research-notes.md` (same
directory) — every default below is sourced to that file or explicitly reasoned where
no source exists. No brand or hardware/plugin names appear below in parameter or UI
naming; the research notes cite sources freely, this brief does not.

## 0. Why v1 falls short

Seraph v1 (M1, v0.1.0) is a correctly-engineered, real-time-safe, zero-latency
four-stage channel strip: 28 green Catch2 tests, bit-exact bypass at each stage's
"off" value, no NaN/Inf under stress, 0 samples of latency in all configurations. That
engineering is not in question and nothing below asks to weaken it.

What v1 lacks is **voicing** — the accumulated, sourced design decisions that make a
processor in each of these four categories (de-esser, air shelf, glue compressor,
vocal doubler) sound like a *considered instance of that category* rather than a
generically correct implementation of the textbook algorithm. Concretely, per the
research notes:

- **De-Ess**: a fixed, hidden Q = 1.2 detection bandwidth and a fixed, absolute
  -28 dBFS threshold, with no bandwidth control exposed. Both reference plugins
  studied (FabFilter Pro-DS, Waves Sibilance) treat detection bandwidth as a primary,
  user-facing control — Seraph currently doesn't expose the single knob most engineers
  reach for first when a de-esser catches the wrong sound.
- **Air**: a conventional single-corner 12 kHz shelf with a ±12 dB range. The
  reference unit studied (Maag EQ4 Air Band) achieves its "air" character specifically
  by *not* being a conventional shelf — it spreads a gentle rise across 10-40 kHz
  with ~5-6 dB max audible lift. Seraph's shelf, run hot, reads as EQ boost, not air.
- **Comp**: fixed single-time-constant release. The reference unit studied (SSL-style
  bus compressor) is defined by program-dependent auto-release — the one feature most
  responsible for the "glue" character the category is named for. Seraph has none.
- **Doubler**: base delays (13-29 ms) and detune range (0-50 cents) both sit outside
  the reference cluster found across three independent sources (Abbey Road ADT:
  8-12 ms; Eventide MicroPitch doubling folklore: 6-25 ms / 4-12 cents "doesn't sound
  like an effect" zone; Roland Dimension D: depth-compensated-by-rate design, which
  Seraph already does correctly). The doubler's underlying architecture is sound and
  partially *validated* by the research (Dimension-D's depth/rate coupling matches
  Seraph's existing math) — this module needs retuning, not rearchitecting.

v2's job is to close these four gaps without touching the invariants that make v1
correct: 0 latency, bit-exact bypass, real-time safety, no lookahead.

## 1. Topology (unchanged)

```
input -> De-Ess (sibilance dynamic EQ, + Listen mode) -> Air (high-shelf)
       -> Gentle Compressor (broadband glue) -> Doubler (4 voices, per-voice pan)
       -> Output trim -> Mix -> output
```

No stage is added, removed, or reordered in v2. `SeraphEngine::getLatencySamples()`
continues to return 0 unconditionally — none of the module changes below require
lookahead, oversampling, or a delay-compensation path.

## 2. Module specs

### 2.1 De-Ess

| Parameter | v1 | v2 | Source / reasoning |
|---|---|---|---|
| `DeEss` (amount) | 0-100%, default 30% | unchanged | no gap found |
| `DeEssFreq` | 3,000-12,000 Hz, log, default 7,000 Hz | unchanged | 7 kHz sits inside the 4-8 kHz sibilance-center literature range and the Pro-DS 2-20 kHz sidechain range; no reference source suggested a different default |
| `DeEssWidth` (new) | — | 0-100%, default 40%, maps to detector Q **0.7 (wide) → 3.0 (narrow)** at 100%→0% | New control closing the single largest gap in the research notes: both Pro-DS ("Detection... Q or width") and Sibilance expose bandwidth as a primary control. v1's fixed Q=1.2 becomes the *default* (≈40% on the new control), not a hidden constant — existing presets/state remain audibly close after tolerant import (§6). Q range (0.7-3.0) is reasoned, not sourced to an exact figure (neither manual publishes a raw Q number) — stated explicitly in §5. |
| Threshold | fixed -28 dBFS | unchanged constant, but **documented as a known departure** from the reference class's relative/adaptive detectors | No source gives an exact number to replace -28 dBFS with; making the detector level-relative is a bigger architectural change than v2's scope (would touch the null-test invariant's signal-independence). Flagged as a v0.3+ candidate, not implemented now. |
| Attack / Release | 1 ms / 80 ms fixed | unchanged | 1 ms attack matches the patent-literature "≈1 ms" figure closely; 80 ms release is slower than the ~30-50 ms figures found, but no source ties a specific release time to a specific de-esser character strongly enough to justify a breaking ballistics change this cycle — left as-is, noted honestly in §5 rather than silently kept. |
| `DeEssListen` | bool, off | unchanged | no gap found |

Max reduction range (`DeEss * 24 dB`) and the `output = input + bandpassed *
(gainFactor - 1)` bit-exact-bypass construction are unchanged — `DeEssWidth` only
changes which coefficients feed `makeBandPass`, not the reduction math, so `DeEss =
0%` stays a bit-exact bypass regardless of `DeEssWidth`.

### 2.2 Air

| Parameter | v1 | v2 | Source / reasoning |
|---|---|---|---|
| `Air` (gain) | -12 to +12 dB, default +3 dB | **-6 to +9 dB**, default +2 dB | Range narrowed and re-centered toward the reference class's effective ~5-6 dB max audible lift (Maag EQ4 Air Band measured curve: 2 dB@10 kHz / 3 dB@20 kHz / 5 dB@40 kHz at its hottest setting). A small negative range is kept (cut is still useful per the existing manual's "cut if... vocal sounds thin or harsh" guidance) but is now narrower than boost, matching that boost is the primary use case in the reference class. |
| Corner frequency | fixed 12 kHz | unchanged, but **shelf shape reasoned to widen** — see below | No source publishes an exact alternate corner; 12 kHz is already inside the reference unit's documented multi-octave rise (10-40 kHz) as a reasonable single representative point. |
| Shelf slope/shape | standard 2nd-order Butterworth-Q shelf | **wider, gentler transition** — implemented as a lower explicit shelf Q (≈0.5 instead of the Butterworth default ≈0.707) so the curve starts rising roughly an octave earlier and reaches its full gain roughly an octave later | Reasoned, not measured: the source material describes the reference curve's *character* (gentle, multi-octave, low-phase-shift) precisely but does not publish filter-design coefficients: a lower explicit Q is the standard, well-understood way to widen a shelf's transition band without adding a second filter stage or changing the real-time-safety/latency profile. Stated explicitly as reasoned in §5. |

At `Air == 0 dB` the shelf still collapses to (near-)identity regardless of the Q
change — this must remain part of the null test.

### 2.3 Gentle Compressor

| Parameter | v1 | v2 | Source / reasoning |
|---|---|---|---|
| `Comp` (amount) | 0-100%, co-scales threshold 0→-20 dBFS and ratio 1:1→3:1 | unchanged mapping | the coupled single-knob "amount" design is a deliberate, defensible minimal-UI simplification already documented in v1; no source in the reference class argues against a single-knob amount control per se — SSL's switched controls solve a different problem (recallable presets on hardware), not a philosophical objection to single-knob glue compressors |
| Attack | 15 ms fixed | unchanged | inside the reference class's switched attack range; no specific figure sourced to justify a change |
| Release | 150 ms fixed one-pole | **program-dependent two-stage release**: fast envelope path (~150 ms, unchanged) blended with a slow envelope path (~1.0 s) via a **release-adapting mix** that weights toward the slow path the longer gain reduction has been continuously active, and snaps back to the fast path on a fresh transient | Directly sourced: SSL-class "Auto... provides a program-dependent, multi-stage release for the greatest degree of transparency" is the single most-cited defining feature of the reference class (§3, research notes). Exact time constants (0.1/0.3/0.6/1.2 s switched, or Auto's internal multi-stage timing) are not published in enough detail to copy exactly — v2's two-stage blend is a reasoned approximation of the *documented behavior* (transparent on transients, glued on sustained program material), explicitly flagged as reasoned rather than measured in §5. |
| `Comp == 0%` bypass | bit-exact | unchanged | the two-stage release only affects the reduction envelope, which is already skip-computed on bypass exactly as v1 does |

The two-stage release must not introduce audible zipper/stepping at the blend
boundary — a smoothed (not switched) crossfade between the two envelope paths is
required, verified by a dedicated test (§4).

### 2.4 Doubler

| Parameter | v1 | v2 | Source / reasoning |
|---|---|---|---|
| Base delays | 13 / 17 / 23 / 29 ms | **9 / 13 / 19 / 24 ms** | Re-centered into the reference cluster: Abbey Road ADT (8-12 ms) sets the tight end, Eventide MicroPitch doubling folklore (6-25 ms, commonly-cited pairs like 15/25 ms) sets the outer end. New spread (9-24 ms) keeps four *distinct* delays (preserving the existing "no shared LFO" de-correlation design) while moving the whole cluster down from a chorus-leaning 13-29 ms into a doubler-leaning 9-24 ms neighborhood. |
| LFO rates | 0.23 / 0.31 / 0.17 / 0.37 Hz | unchanged | no reference source published exact LFO rates for a 4-voice design (Dimension D's 0.25/0.5 Hz figures are for a 2-mode, not 4-voice, unit); v1's already-distinct rates are kept as reasoned, not replaced with unsourced new numbers |
| `DoubleDetune` | 0-50 cents, linear, default 15 | **0-50 cents, reshaped taper** (see below), default **10 cents** | Range kept at 50 (the plugin's "small choir spread" goal genuinely needs headroom beyond the reference class's tight-double numbers), but the *default* moves from 15 to 10 cents — inside the Eventide "doesn't sound like an effect" 4-12 cent zone identified across two independent sources — and the control taper is reshaped (see below) so the reference-class 4-12 cent "tight double" register isn't crammed into the first few degrees of knob travel. |
| `DoubleDetune` taper | linear 0-50 | **log-ish (power) taper**: normalized position `p` in 0-1 maps to `cents = 50 * p^2.2` | Reasoned, not sourced to an exact curve: the goal — give the reference-class-validated 4-20 cent "double" register more usable knob travel than the 20-50 cent "chorus" register — is directly stated by the research (§4 takeaway), but no source publishes an exact power-law exponent for any hardware/plugin's detune taper. Exponent 2.2 is a standard perceptual-curve starting point (consistent with the log-frequency taper already used elsewhere in this codebase for `DeEssFreq`), explicitly flagged as reasoned in §5. |
| `DoubleWidth`, `Double` (send) | unchanged | unchanged | no gap found |

`Double == 0%` remains a bit-exact no-op on the buffer; delay-line/LFO phase state
continues to advance internally regardless, exactly as v1's documented behavior.

### 2.5 Unchanged

`Mix`, `Output` — no gap identified in either the architecture review or the research
notes.

## 3. Factory Presets (for the upcoming M2 preset system)

Six presets spanning the manual's own documented placements (lead / choir bus /
spoken-growled interlude) plus two utility extremes. Settings are starting points
expressed against the v2 parameter set above, not exact prescriptions.

1. **Lead — Cut Through** — intent: solo operatic lead over dense guitars/orchestra.
   `DeEss 35%, DeEssFreq 7.5kHz, DeEssWidth 40%, Air +5dB, Comp 25%, Double 15%,
   DoubleDetune 8¢, DoubleWidth 60%.`
2. **Lead — Intimate/Close-Mic** — intent: close-mic'd solo take, mild sibilance,
   minimal doubling. `DeEss 45%, DeEssFreq 6.5kHz, DeEssWidth 55%, Air +1dB, Comp
   15%, Double 8%, DoubleDetune 6¢, DoubleWidth 40%.`
3. **Choir — Wide Spread** — intent: turn a handful of real takes into a full
   small-choir width. `DeEss 20%, DeEssFreq 7kHz, DeEssWidth 35%, Air +3dB, Comp
   35%, Double 55%, DoubleDetune 18¢, DoubleWidth 100%.`
4. **Choir — Tight Blend** — intent: layered backing vocals that need to sit
   underneath a lead, not compete with it. `DeEss 25%, DeEssFreq 7kHz, DeEssWidth
   35%, Air 0dB, Comp 40%, Double 30%, DoubleDetune 10¢, DoubleWidth 70%.`
5. **Spoken/Growled Interlude** — intent: level-consistent narration/growl against a
   quiet orchestral backing, per the manual's own guidance that de-essing is often
   unnecessary here. `DeEss 5%, DeEssFreq 7kHz, DeEssWidth 40%, Air +4dB, Comp 55%,
   Double 5%, DoubleDetune 5¢, DoubleWidth 20%.`
6. **Glue Only** — intent: dynamics/level-consistency utility, minimal tonal or
   doubling change, for engineers who want just the compressor stage.
   `DeEss 0%, Air 0dB, Comp 50%, Double 0%, DoubleWidth 0%.`
7. **De-Ess Only (Surgical)** — intent: isolate sibilance control as a standalone
   utility insert. `DeEss 50%, DeEssFreq 7kHz, DeEssWidth 50%, Air 0dB, Comp 0%,
   Double 0%.`
8. **Wide Double (No Dynamics)** — intent: doubler-only utility for engineers routing
   dynamics elsewhere. `DeEss 0%, Air 0dB, Comp 0%, Double 60%, DoubleDetune 20¢,
   DoubleWidth 100%.`

## 4. Catch2 test guarantees (v2 additions on top of the existing 28)

Existing suite (`CoverageTests`, `EngineTests`, `LatencyTests`, `ParameterTests`,
`RobustnessTests`, `StateTests`) stays green and is extended, not replaced.

- **De-Ess bandwidth curve**: sweep `DeEssWidth` 0→100% at fixed `DeEssFreq`, measure
  the detector's -3 dB bandwidth via a swept-sine or multi-tone probe signal through
  `DeEsser::process()` in isolation; assert monotonic bandwidth narrowing as
  `DeEssWidth` decreases (wide→narrow per the mapping in §2.1), and that the Q
  extremes land within ±10% of the specified 0.7/3.0 targets.
- **De-Ess null test still holds with the new control**: `DeEss = 0%` is bit-exact
  bypass across the full `DeEssWidth` 0-100% range (not just at the old fixed Q),
  parameterized/looped over `DeEssWidth` values.
- **Air curve shape**: measure the shelf's magnitude response at 1 kHz (reference),
  6 kHz, 12 kHz (corner), and 20 kHz at `Air = +9 dB` (new max); assert the response
  at 6 kHz is measurably non-zero (i.e., confirms the widened transition actually
  starts earlier than the old Butterworth-Q shelf would have) and that the curve is
  monotonically non-decreasing from 1 kHz to 20 kHz.
- **Air null test**: `Air = 0 dB` bit-exact/near-identity within the existing -90 dBFS
  tolerance, unchanged assertion, re-run against the new Q constant.
- **Comp auto-release ballistics**: feed a step transient followed by sustained
  program-level material; assert gain reduction recovers *faster* immediately after
  an isolated transient than it does after continuous sustained reduction (the
  defining, testable signature of program-dependent release) — i.e. measure
  time-to-90%-recovery in both cases and assert `t_transient < t_sustained`.
- **Comp release-blend continuity**: assert no discontinuity (sample-to-sample delta
  exceeding a small epsilon) at the fast/slow envelope blend boundary, across a
  swept program-material test signal — guards against the exact zipper/stepping
  artifact §2.3 calls out as a hard requirement.
- **Comp null test**: `Comp = 0%` bit-exact bypass, unchanged assertion, re-run
  against the new release path (which must still be entirely skip-computed at
  bypass).
- **Doubler delay-time bounds**: assert each voice's instantaneous delay (base ±
  modulation depth) stays within the new 9-24 ms base-delay neighborhood's expected
  envelope at max `DoubleDetune`, and within the delay line's allocated capacity
  (existing defensive-clamp test, re-run against new base delays).
- **Doubler detune taper**: assert `DoubleDetune` at the reshaped taper's 50%
  knob-position control value produces a cents value below the taper's midpoint
  (confirms the power-law taper's "more room in the tight-double register" property
  is actually present, not just described).
- **Doubler null test**: `Double = 0%` bit-exact no-op on the buffer, unchanged
  assertion, re-run against new base delays/taper.
- **State round-trip / tolerant import** (see §6): save a v0.1.0-shaped state
  (missing `DeEssWidth`), load it into the v2 processor, assert `DeEssWidth` falls
  back to its documented default (40%) rather than 0/garbage, and that all other
  carried-over parameter values match their v0.1.0 values exactly (no silent
  rescaling of `DoubleDetune`'s old linear values through the new taper — see §6 for
  the exact interpretation rule).
- **Latency**: `getLatencySamples() == 0` unconditionally, re-run across the full v2
  parameter space (existing test pattern, extended to cover `DeEssWidth` and the new
  Comp release path).
- All existing sample-rate sweep (44.1-192 kHz), mono/stereo, long-run NaN/Inf, and
  rapid-automation tests re-run unchanged against the v2 DSP — no test is deleted,
  only extended.

## 5. Honesty section

This brief is **research-derived, not hardware-measured**. No reference unit or
plugin studied here was run through a measurement rig, an audio analyzer, or A/B'd by
ear against Seraph — all figures come from public manuals, help documentation,
engineering/patent literature, and credible secondhand technical writeups (magazine
reviews, forum measurement threads, splice/production-blog technical explainers), all
cited with URLs in `seraph-research-notes.md`.

Explicitly flagged as **reasoned, not sourced to an exact number**:

- `DeEssWidth`'s Q range (0.7-3.0) — no manual studied publishes a raw Q figure for
  either reference de-esser's bandwidth control.
- The Air shelf's new explicit Q (~0.5) — the reference unit's *curve character* is
  well documented, its filter-design coefficients are not.
- The Comp two-stage auto-release's exact time constants (~150 ms fast / ~1.0 s slow,
  and the blend-weighting function) — the reference class's *existence and purpose*
  of program-dependent release is strongly sourced; its exact internal timing is
  proprietary and not published in the sources found.
- The Doubler's power-law detune taper exponent (2.2) — the *goal* (favor the
  reference-validated 4-20 cent register with more knob travel) is directly derived
  from sourced figures; the specific curve shape used to achieve it is a standard
  audio-taper choice, not itself sourced.

Two PDF primary sources (SSL 500-series G-Comp module user guide, Waves SSL G-Master
manual, Maag EQ4 user guide) were fetched but returned as unparseable binary/image
data rather than extractable text; the figures attributed to them in the research
notes are corroborated via search-indexed secondary summaries of the same documents,
not a direct read of the primary PDF text. This is noted so a future pass with a
proper PDF-text extraction step (rather than WebFetch's HTML-oriented summarizer) can
verify the exact wording directly against the primary source.

## 6. Versioning

- Target: **v0.2.0**. Breaking parameter changes are allowed pre-1.0 per the existing
  roadmap (M1 done, M2 = presets/state — this brief is scoped to land inside M2).
- **Breaking changes in this brief**: `Air` range narrows from ±12 dB to -6/+9 dB
  (existing saved states with `Air` outside the new range must be clamped on load,
  not rejected); `DoubleDetune`'s control taper changes from linear to power-law
  (the *stored parameter value remains cents*, per JUCE's normalizable-range design —
  only the knob-position↔cents mapping changes, so a saved state's cents value is
  reproduced exactly regardless of taper, this is not actually a breaking change to
  stored state, only to the UI curve).
- **New parameter**: `DeEssWidth` (0-100%, default 40%). State migration: **tolerant
  import** — loading a v0.1.0-saved state (which has no `DeEssWidth` entry) must not
  fail or reset unrelated parameters; the missing parameter falls back to its default
  (40%, chosen to reproduce v1's fixed Q≈1.2 behavior as closely as the new range
  allows — see §2.1). This matches the existing `AudioProcessorValueTreeState`
  pattern already used for `DeEssListen`'s addition in M1 (both are booleans/floats
  added to an existing layout, both must degrade gracefully on old-state import).
- No parameter is removed or renamed in this brief — all IDs in
  `src/params/ParameterIds.h` remain valid; only ranges/defaults/one new ID change.
