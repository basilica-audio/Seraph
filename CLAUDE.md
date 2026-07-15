# Seraph — choir & vocal processor (vocals)

Per-repo working memory for Claude Code sessions on this plugin. Part of the **Basilica Audio** plugin suite — sacred-architecture DSP for heavy music (`github.com/basilica-audio`).

## What this is
Seraph is the "choir & vocal processor (vocals)" member of the suite. AU / VST3 / Standalone, JUCE 8.

## Status (v0.1.0 — M1 DSP completion & test coverage done)
Core DSP complete for v0.1.0, **28 Catch2 tests green** (sample-rate sweeps 44.1-192 kHz, mono/stereo bus configs, long-run NaN/Inf stability, extreme/rapid parameter automation, in addition to null/reference/latency/state tests). CI (macOS + Windows, pluginval strictness 10 + auval) green. GUI is a functional v0.1 slider/toggle editor (custom LookAndFeel is roadmap M3). No signing yet (roadmap M4). Open work is tracked in this repo's GitHub **milestones/issues**.

**Deferred from M1**: true formant-preserving detune (LPC/cepstral spectral-envelope correction) was not implemented - it's a substantially larger DSP feature than the doubler's existing modulated-delay architecture supports without risking the plugin's zero-latency and bit-exact-bypass invariants. See the M1 "Complete and refine the DSP" issue and `docs/architecture.md`'s Doubler section for the full reasoning; the issue is left open for this follow-up.

## DSP
Seraph is a choir/vocal processor: input runs through DeEsser (a 2nd-order IIR bandpass detector at DeEssFreq, Q=1.2, feeding a one-pole envelope follower and a hard-knee downward compressor whose reduction is applied via output = input + bandpassed*(gainFactor-1), so DeEss=0% is bit-exact bypass; a DeEssListen toggle solos the detected band for tuning), then a fixed 12 kHz high-shelf (juce::dsp::IIR::Coefficients::makeHighShelf) for Air, then GentleCompressor (a hand-rolled broadband feed-forward downward compressor mirroring DeEsser's detector structure, single Comp knob scaling threshold/ratio together, Comp=0% bit-exact bypass), then Doubler (four independent juce::dsp::DelayLine<float,Linear> voices at 13/17/23/29 ms base delays with distinct LFO rates, sinusoidally modulated for a click-free cents-based detune wobble, each at a fixed per-voice pan position scaled by DoubleWidth and summed onto the buffer scaled by Double, with Double=0% leaving the buffer untouched), then Output trim (juce::dsp::Gain) and a final linear crossfade against a pre-captured true-dry copy for Mix. Nothing in the chain uses oversampling or lookahead, so SeraphEngine::getLatencySamples() always returns 0 and PluginProcessor reports 0 latency unconditionally.

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
