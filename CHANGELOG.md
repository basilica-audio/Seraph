# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog 1.1.0](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- **M3 custom vector editor** (issue #4, ported from Miserere's merged M3
  implementation, basilica-audio/miserere PR #31): the interim slider/dropdown
  editor is replaced by the suite's fully vector-drawn black/gold surface —
  pointer knobs with engraved scale rings (choice parameters as detented knobs
  announcing the choice *name*), lamp toggles, EB Garamond typography embedded
  via BinaryData (OFL licensed), and five signal-flow stage panels (De-Ess /
  Air / Compressor / Doubler / Output). No photoreal PNG assets; everything is
  drawn at runtime with `juce::Graphics`/`juce::Path`.
- **Gain-reduction needle meters**: two vector needle meters (De-Esser and
  Compressor) driven by new relaxed-atomic meter getters on the processor
  (`getDeEssGainReductionMeterDb()` / `getCompGainReductionMeterDb()`, fed from
  the stages' existing per-block gain-reduction readings) via a 30 Hz GUI
  timer with one-pole ballistics.
- **Accessible parameter surface** (WCAG 2.1 AA): every control keyboard-
  operable (WAI-ARIA stepping: Arrow 1%, Shift+Arrow fine, PageUp/Down 10%,
  Home/End extremes), visible focus rings on all custom-painted controls,
  name/value/role for every knob/toggle/meter (unit-suffixed accessible values,
  read-only meter values), stage panels as accessibility focus containers
  (grouped AT navigation without trapping Tab), and WCAG-contrast unit tests
  pinned to the exact rendered colour pairs. New test suites:
  `tests/gui/EditorAccessibilityTests.cpp`, `EditorLayoutTests.cpp`,
  `BasilicaLookAndFeelContrastTests.cpp`, `NeedleMeterTests.cpp`.

## [0.3.0] - 2026-07-27

The doubler stops faking it. v0.1/v0.2 detuned by wobbling a delay line, which
is click-free but means a stacked voice is never actually in tune - it drifts
around the note. v0.3.0 adds two engines that hold a constant interval, plus
per-voice humanisation so a stack decorrelates the way real singers do instead
of tracking clock-locked LFOs. The de-esser gains the three controls that
separate a utility de-esser from a mix-grade one.

Every existing session and preset loads and renders **bit-identically** -
verified by a null test at 44.1/48/96 kHz and block sizes 64 and 1024, and by
re-measuring the v0.2.0 default render's hash after every commit in this
release.

### Added

- **Doubler engine modes** (`Double Mode`, default Classic). **Classic** is the
  shipped v0.1/v0.2 modulated-delay chorus, unchanged byte for byte.
  **Micro** is a constant-offset dual-head micropitch shifter with cubic
  Catmull-Rom interpolation - a real fixed detune, so a stack holds its
  interval; measured accurate to 0.02 cents at +/-30 cents. **Shift** is an
  STFT pitch shifter with optional formant preservation, and the only mode
  that reports host latency. Closes the formant-preserving detune work
  deferred from M1.
- **Humanize** (0-100%, default 0%): three slow seeded random walks per voice
  drifting timing (+/-10 ms), pitch (+/-3 cents) and level (+/-1.5 dB). Runs
  on a fixed control clock, so the drift is independent of the host's block
  size and two renders from the same reset state are bit-identical. At 0% every
  offset is exactly zero.
- **Formant Preserve** (default on, active only in Shift mode): holds the
  vowel's spectral envelope in place while the partials move, so a shifted
  voice keeps its character instead of sounding transposed.
- **De-Ess Link** (default off): one shared gain driven by the louder channel,
  so a hard-panned ess cannot pull the stereo image sideways.
- **De-Ess Knee** (0-12 dB, default 0): quadratic soft knee around the
  threshold. At 0 the v0.2.0 hard-knee expression is evaluated verbatim.
- **De-Ess Lookahead** (0-2 ms, default 0): the gain reaches its target before
  the ess arrives rather than chasing it, removing the onset overshoot a 1 ms
  attack otherwise lets through. Adds exactly `round(ms * sr / 1000)` samples
  of reported latency.
- **Comp Link** (default off): one shared envelope, including the
  program-dependent auto-release, driven by the louder channel.
- **Air Freq** (10/12/15 kHz, default 12 kHz - the fixed constant v0.1/v0.2
  always used, hence neutral).
- **Three factory presets** for the new engines: "Choir - Sacred Shift",
  "Doubler - Vintage Micro" and "Lead - Tight Stack" (twelve in total, see
  `docs/presets.md`).
- **State schema versioning**: saved state now carries `stateVersion = 2`.
  States without the attribute are version 1 and need no rewriting, because
  every parameter added here defaults to the value that reproduces v0.2.0.
- `tests/DetuneTests.cpp` and substantial additions across the suite: pitch
  accuracy in cents, formant placement, crossfade and STFT artifact levels,
  comb ripple, latency measured by click arrival, de-esser knee curves,
  lookahead alignment, allocation guards over Shift mode including live mode
  switches, and a Release-mode CPU measurement. 109 test cases in total.

### Changed

- **Latency is no longer unconditionally zero.** Classic and Micro still
  report 0, and the default configuration is unchanged. Shift mode reports
  1440 samples (30.0 ms at 48 kHz) and De-Ess Lookahead reports its own
  length; the two add. Both parameters that can change it are marked
  non-automatable, so a host only ever sees a latency change as a deliberate
  user action. The doubler's main path is delayed to match, and the dry signal
  used for the Mix crossfade runs through a compensation delay of the same
  length, so the plugin's output really does arrive where it says it will -
  measured to within one sample.
- **Smoothed parameters now advance in 32-sample slices** instead of once per
  host block, so fast automation no longer produces a block-sized staircase.
  At static settings the sliced path is bit-identical to the old one. The Air
  shelf's coefficient recompute deliberately stays at block rate.
- The editor grows from two rows of controls to four to fit the eight new
  parameters, using the same generic knob/toggle/combo grid. The photoreal GUI
  remains a later milestone.
- The nine existing factory presets gained the eight new keys at their neutral
  values and a bumped `pluginVersion`; a test confirms none of them changed
  sonically.

### Fixed

- **Non-finite input can no longer permanently poison Shift mode.** A phase
  vocoder's per-bin phase accumulators latch NaN, and measurement showed the
  vendored engine does not recover even from its own `reset()`. The wrapper
  now substitutes silence for non-finite input before the engine sees it.

### Third-party

- Adds **Signalsmith Stretch** (MIT, Geraint Luff / Signalsmith Audio Ltd.)
  as the Shift mode's STFT engine, with its **Signalsmith Linear** FFT backend
  (MIT). Both are MIT and therefore compatible with this project's AGPLv3;
  full copyright and permission notices are reproduced in the new
  `THIRD-PARTY-NOTICES.md`. Stretch is pinned to a development-head commit
  rather than the newest release tag, because the formant API this release
  depends on has not been tagged yet - the pin is a full commit SHA, so the
  resolved source is exact.

## [0.2.0] - 2026-07-16

### Added

- **M2 preset system**: a `PresetManager`/`PresetBar` implementation (`src/presets/`) following the suite-wide binding spec (`.scaffold/specs/preset-system-m2.md`, pilot: sibling plugin `nave`) - factory presets (embedded via BinaryData), user presets (`~/Library/Audio/Presets/Yves Vogl/Seraph/` on macOS), save/save-as/rename/delete, single-file and zip-bank import/export, a dirty-state indicator, default-preset resolution (user "Default" > factory "Default" > built-in parameter defaults), and a preset-bar strip docked at the top of the editor. Nine factory presets ship (`presets/factory/*.json`, documented in `docs/presets.md`): Default, Lead - Cut Through, Lead - Intimate/Close-Mic, Choir - Wide Spread, Choir - Tight Blend, Spoken/Growled Interlude, Glue Only, De-Ess Only (Surgical), Wide Double (No Dynamics).
- **German frame-string localisation** (`resources/i18n/de.txt`): the preset bar's labels, menus, and dialogs are wrapped in `TRANS()` and translated to German (auto-selected from the system language); parameter names/units/DSP terminology are never translated, per the binding i18n spec.
- **`DeEssWidth` parameter** (0-100%, default 40%): exposes the de-esser's detection bandwidth as a user-facing control for the first time, mapping to the detector's bandpass Q (3.0 narrow at 0% -> 0.7 wide at 100%) - the single largest gap identified against the reference de-esser class in `docs/research-notes.md`. New parameter, tolerant state import (see Changed below).
- Research-derived deep-dive design brief (`docs/design-brief.md`) and its supporting research notes (`docs/research-notes.md`), documenting every default/range change below with a source or an explicit "reasoned, not sourced" flag - see the brief's own Honesty section (ss5).
- `tests/DeepDiveTests.cpp`: new Catch2 coverage for the v0.2.0 DSP changes - DeEssWidth bandwidth-curve and Q-extremes measurement, DeEssWidth null-test coverage, Air magnitude-response shape (1/6/12/20 kHz) and null test, GentleCompressor auto-release ballistics (transient-vs-sustained recovery time) and blend-boundary continuity, and Doubler delay-time bounds with the new base delays.
- `tests/PresetManagerTests.cpp`: 17 Catch2 tests covering the preset system (round-trip, tolerant import, format/plugin validation, factory-preset plausibility, default resolution, dirty-flag lifecycle, prev/next ordering, save/rename/delete guards, single-file and bank export/import, and audio-thread-safety-by-design).
- `tests/StateTests.cpp`: a tolerant state-migration test - loading a hand-built v0.1.0-shaped state (missing `DeEssWidth`) leaves `DeEssWidth` at its documented default and every other carried-over parameter exact.

### Changed

- **De-Ess**: the previously hidden, fixed detector Q (1.2) is now the `DeEssWidth` control's default position (40%) rather than a constant - existing sessions/presets remain audibly close after the tolerant import described above. Threshold/attack/release are unchanged (both flagged in `docs/design-brief.md` ss5 as departures from the reference class that are honestly documented rather than silently kept).
- **Air**: range narrowed and re-centered from -12/+12 dB to **-6/+9 dB** (default +3 dB -> **+2 dB**), and the shelf's explicit Q lowered from the Butterworth default (~0.707) to **0.5**, widening the transition band so higher settings read as "air" rather than harsh boost - sourced against the reference "air" shelf class in `docs/research-notes.md`. **Breaking**: existing saved states with `Air` outside the new range are clamped on load, not rejected.
- **Gentle Compressor**: the fixed single-time-constant release (150 ms) is replaced by a **program-dependent ("auto") release** - a smoothed blend between a fast (~150 ms) and a slow (~1.0 s) envelope path, biased toward transparency on isolated transients and toward "glue" on sustained program material - directly sourced from the reference glue-compressor class's most-cited defining feature (`docs/research-notes.md`).
- **Doubler**: base delays re-centered from 13/17/23/29 ms to **9/13/19/24 ms** (into the reference doubler cluster documented in `docs/research-notes.md`); `DoubleDetune`'s default lowered from 15 to **10 cents** and its knob taper reshaped from linear to a power curve (`cents = 50 * p^2.2`), giving the reference-validated 4-20 cent "tight double" register more knob travel. The taper change only affects the knob-position-to-cents mapping, not the parameter's stored real-unit (cents) value, so it is not a breaking change to saved state.
- Editor: added a De-Ess Width knob (now two rows of six controls) and the preset bar; `docs/architecture.md` and `docs/manual.md` updated throughout for the above.
- Version bumped to 0.2.0.

## [0.1.1] - 2026-07-16

### Changed

- Housekeeping: canonical squircle icon cutout embedded into the plugin binary (`ICON_BIG`) and README/manual, org link sweep, heavy-music copy reframe, README pointed at GitHub Releases, and the signed tag-triggered release CI workflow added.

### Fixed

- **Audio-thread heap allocation in `SeraphEngine`'s Air high-shelf**: `process()` recomputed the Air filter's coefficients via `Coefficients::makeHighShelf()` every block, which heap-allocates a new `Coefficients` object internally. Switched to the allocation-free `ArrayCoefficients::makeHighShelf()`, written in place into the existing filter state.
- **Audio-thread heap allocation in `DeEsser`'s bandpass detector**: `process()` recomputed the detector's coefficients via `Coefficients::makeBandPass()` every block (even when `DeEss` amount was 0%), likewise heap-allocating internally. Switched to the allocation-free `ArrayCoefficients::makeBandPass()`, written in place into the existing detector coefficients.
- Added a permanent audio-thread allocation regression test (`tests/AllocationTests.cpp`, `TestAlloc::AllocationGuard`) so a future `process()`-time heap allocation in any DSP stage fails CI instead of passing silently.

## [0.1.0] - 2026-07-14

### Added

- Project bootstrap: README, license, contributing guide, architecture and build docs, ADRs, and CI workflow.
- DSP core: initial working Seraph signal path (De-Ess, Air, Doubler, Mix/Output) with unit tests.
- **Gentle Compressor** DSP stage (`Comp` parameter, 0-100%): a hand-rolled, zero-latency, bit-exact-bypassable broadband downward compressor placed after Air and before the Doubler, for evening out dynamics before the signal is duplicated into doubled voices.
- **De-Ess Listen** mode (`DeEssListen` parameter): solos the de-esser's detected sibilance band so `DeEssFreq` can be tuned by ear, independent of the current `DeEss` reduction amount.
- Doubler extended from two to **four voices**, each with its own fixed per-voice pan position (a small-choir spread scaled by `DoubleWidth`, rather than a single symmetric L/R pair); gain-staging is compensated so the added level matches the original two-voice design at `DoubleWidth == 0`.
- GUI: added a Comp knob and a De-Ess Listen toggle button to the v0.1 slider editor (now two rows of five controls).
- Test suite broadened from 18 to 28 Catch2 tests: sample-rate sweeps (44.1-192 kHz) for the null test and for the full chain at hot settings, mono/stereo/rejected bus-layout coverage, a long-run (~10.7 s simulated) NaN/Inf and unbounded-growth stability sweep, prepareToPlay sample-rate-change robustness, and dedicated coverage for the new compressor, listen mode, and four-voice doubler.
- `docs/manual.md`: a full user manual (what Seraph is, where it sits in a symphonic-metal vocal chain, signal flow, complete parameter reference, and mixing tips).

### Deferred

- True formant-preserving detune (LPC/cepstral spectral-envelope correction for the doubler) was requested for M1 but not implemented - it is a substantially larger DSP feature than fits safely alongside the rest of this milestone without risking the plugin's zero-latency and bit-exact-bypass invariants. See `docs/architecture.md`'s Doubler section and the M1 "Complete and refine the DSP" issue, left open for this follow-up.
