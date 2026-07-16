# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog 1.1.0](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

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
