# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog 1.1.0](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

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
