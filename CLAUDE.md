# Seraph — choir & vocal processor (vocals)

Per-repo working memory for Claude Code sessions on this plugin. Part of the **Basilica Audio** plugin suite — sacred-architecture DSP for heavy music (`github.com/basilica-audio`).

## What this is
Seraph is the "choir & vocal processor (vocals)" member of the suite. AU / VST3 / Standalone, JUCE 8.

## Status (v0.2.0 — deep-dive voicing pass + M2 preset system done)
**60 Catch2 tests green** (sample-rate sweeps 44.1-192 kHz, mono/stereo bus configs, long-run NaN/Inf stability, extreme/rapid parameter automation, null/reference/latency/state tests, plus v0.2.0's DeepDiveTests.cpp DSP-guarantee coverage and PresetManagerTests.cpp). CI (macOS + Windows, pluginval strictness 10 + auval) green. GUI is a functional v0.1/v0.2 slider/toggle editor plus a preset bar (custom LookAndFeel is still roadmap M3). No signing yet (roadmap M4). v0.2.0 shipped: a research-derived voicing pass across all four DSP stages (`docs/design-brief.md`, research: `docs/research-notes.md`), the suite's M2 preset system (`src/presets/`, ported from pilot plugin nave - see nave's `docs/preset-system-notes.md` for the replication recipe), nine factory presets (`docs/presets.md`), a German frame-string localisation, and the app icon wired via `ICON_BIG`. Open work is tracked in this repo's GitHub **milestones/issues**.

**Deferred from M1** (still open in v0.2.0): true formant-preserving detune (LPC/cepstral spectral-envelope correction) was not implemented - it's a substantially larger DSP feature than the doubler's existing modulated-delay architecture supports without risking the plugin's zero-latency and bit-exact-bypass invariants. See the M1 "Complete and refine the DSP" issue and `docs/architecture.md`'s Doubler section for the full reasoning; the issue is left open for this follow-up.

## DSP
Seraph is a choir/vocal processor: input runs through DeEsser (a 2nd-order IIR bandpass detector at DeEssFreq, Q driven by DeEssWidth - 3.0 narrow at 0% to 0.7 wide at 100%, v0.2.0 - feeding a one-pole envelope follower and a hard-knee downward compressor whose reduction is applied via output = input + bandpassed*(gainFactor-1), so DeEss=0% is bit-exact bypass; a DeEssListen toggle solos the detected band for tuning), then a fixed 12 kHz high-shelf (juce::dsp::IIR::Coefficients::makeHighShelf, Q lowered to 0.5 in v0.2.0 for a wider/gentler transition, range narrowed to -6/+9 dB) for Air, then GentleCompressor (a hand-rolled broadband feed-forward downward compressor mirroring DeEsser's detector structure, single Comp knob scaling threshold/ratio together, Comp=0% bit-exact bypass; v0.2.0 adds a program-dependent auto-release - a smoothed blend between a fast ~150 ms and slow ~1.0 s envelope path), then Doubler (four independent juce::dsp::DelayLine<float,Linear> voices at 9/13/19/24 ms base delays (v0.2.0, was 13/17/23/29) with distinct LFO rates, sinusoidally modulated for a click-free cents-based detune wobble with a power-law knob taper (v0.2.0), each at a fixed per-voice pan position scaled by DoubleWidth and summed onto the buffer scaled by Double, with Double=0% leaving the buffer untouched), then Output trim (juce::dsp::Gain) and a final linear crossfade against a pre-captured true-dry copy for Mix. Nothing in the chain uses oversampling or lookahead, so SeraphEngine::getLatencySamples() always returns 0 and PluginProcessor reports 0 latency unconditionally.

## Build & test
```sh
export CPM_SOURCE_CACHE="$HOME/.cache/CPM"      # shared JUCE 8.0.14 + Catch2 cache
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target Tests Seraph_Standalone --parallel 4
ctest --test-dir build --output-on-failure
```
Release/universal + pluginval + auval run in CI, not locally.

## Conventions & guardrails
- JUCE 8.0.14 via CPM · C++20 · AGPLv3 · Pamplejuce `SharedCode` pattern · manufacturer `Yvsv`, plugin code `Srph`, `com.yvesvogl.seraph`.
- **Real-time safety:** no alloc/lock/file-IO/logging on the audio thread; allocate in `prepareToPlay`; `reset()` clears all state; `ScopedNoDenormals`; smoothed params; report latency via `setLatencySamples` where the chain adds any.
- **DryWetMixer gotcha (JUCE 8.0.14):** prime `setWetMixProportion(mix)` before `reset()` in `prepare()` (else it ramps from 100% wet). See sibling `overture`.
- **`main` is protected** — no direct commits; feature branch + PR, green CI required (Conventional Commits). New DSP needs tests (null/reference, NaN/Inf sweep, state round-trip, latency).

## Roadmap
GitHub milestones (M1 DSP & tests · M2 presets/state · M3 GUI & a11y · M4 release/signing/v1.0.0) + issues. Read with `gh issue list --repo basilica-audio/seraph`.

## Suite context
Style references: sibling `basilica-audio/overture` and `basilica-audio/crypta`. The suite: overture, tenebrae, nave, silentium, requiem, seraph, aureate, firmament, triptych, apotheosis, crypta.
