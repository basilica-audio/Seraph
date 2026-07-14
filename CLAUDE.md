# Seraph — choir & vocal processor (vocals)

Per-repo working memory for Claude Code sessions on this plugin. Part of the **Metal up your ass** symphonic-metal plugin suite (`github.com/metal-up-your-ass`).

## What this is
Seraph is the "choir & vocal processor (vocals)" member of the suite. AU / VST3 / Standalone, JUCE 8.

## Status (v0.1 — bootstrap complete)
Core DSP working, **18 Catch2 tests green**, CI (macOS + Windows, pluginval strictness 10 + auval) green. GUI is a functional v0.1 slider editor (custom LookAndFeel is roadmap M3). No signing yet (roadmap M4). Open work is tracked in this repo's GitHub **milestones/issues**.

## DSP
Seraph is a choir/vocal processor: input runs through DeEsser (a 2nd-order IIR bandpass detector at DeEssFreq, Q=1.2, feeding a one-pole envelope follower and a hard-knee downward compressor whose reduction is applied via output = input + bandpassed*(gainFactor-1), so DeEss=0% is bit-exact bypass), then a fixed 12 kHz high-shelf (juce::dsp::IIR::Coefficients::makeHighShelf) for Air, then Doubler (two independent juce::dsp::DelayLine<float,Linear> voices at 17 ms/23 ms base delay, sinusoidally modulated for a click-free cents-based detune wobble, panned via DoubleWidth and summed onto the buffer scaled by Double, with Double=0% leaving the buffer untouched), then Output trim (juce::dsp::Gain) and a final linear crossfade against a pre-captured true-dry copy for Mix. Nothing in the chain uses oversampling or lookahead, so SeraphEngine::getLatencySamples() always returns 0 and PluginProcessor reports 0 latency unconditionally.

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
GitHub milestones (M1 DSP & tests · M2 presets/state · M3 GUI & a11y · M4 release/signing/v1.0.0) + issues. Read with `gh issue list --repo metal-up-your-ass/seraph`.

## Suite context
Style references: sibling `metal-up-your-ass/overture` and `metal-up-your-ass/twist-your-guts`. The suite: overture, tenebrae, nave, silentium, requiem, seraph, aureate, firmament, triptych, apotheosis, twist-your-guts.
