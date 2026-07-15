<p align="center"><img src="docs/assets/icon.png" alt="Seraph icon" width="160"/></p>

# Seraph

*Voices from above — a choir and vocal processor for operatic metal vocals.*

[![CI](https://github.com/basilica-audio/seraph/actions/workflows/ci.yml/badge.svg)](https://github.com/basilica-audio/seraph/actions/workflows/ci.yml)
[![License: AGPL v3](https://img.shields.io/badge/License-AGPL%20v3-blue.svg)](https://www.gnu.org/licenses/agpl-3.0)

> **Work in progress.** Seraph is pre-1.0 and under active development. There are no built binaries or releases yet — building from source is currently the only way to run it. Expect breaking changes until v1.0.0 ships (see [Roadmap](#roadmap)).

<!-- ==BEGIN BODY== (plugin engineer: replace this block with What it is / Features / Signal flow / Roadmap) -->
## What it is

Seraph is a choir and vocal processor built for operatic metal vocals: it tames sibilance, adds airy openness above the vocal's natural top end, glues dynamics together with a gentle broadband compressor, and thickens a lead or choir line with a click-free four-voice doubler - all in one channel-strip-style plugin. AU / VST3 / Standalone.

See [`docs/manual.md`](docs/manual.md) for the full user manual (signal-flow explanation, complete parameter reference, and mixing tips).

## Features

- **De-Ess** - a single-band, zero-latency dynamic EQ that detects sibilance energy around a tunable center frequency (`DeEssFreq`, ~5-9 kHz) and reduces it dynamically, without a lookahead delay. A **Listen** mode solos the detected band for tuning `DeEssFreq` by ear.
- **Air** - a fixed-frequency (12 kHz) high-shelf for adding (or removing) openness above the vocal's presence range.
- **Gentle Compressor** - a single-knob, zero-latency broadband "glue" compressor (`Comp`, 0-100%) that scales threshold and ratio together up to a deliberately gentle 3:1 maximum, sitting after Air and before the Doubler so all doubled voices track a consistent level.
- **Doubler** - four short, independently modulated-delay voices at fixed per-voice pan positions (a small-choir spread, not a single symmetric L/R pair), slightly detuned (`DoubleDetune`, in cents) and spread across the stereo field (`DoubleWidth`), blended in (`Double`) on top of the centered dry signal - a classic vocal-doubling trick, implemented click-free (continuous delay modulation, never a discrete pitch-shift reset).
- **Mix** / **Output** - overall dry/wet blend and output trim.

## Signal flow

```
input -> De-Ess (sibilance dynamic EQ, + Listen mode) -> Air (high-shelf)
       -> Gentle Compressor (broadband glue) -> Doubler (4 voices, per-voice pan)
       -> Output trim -> Mix -> output
```

See [`docs/architecture.md`](docs/architecture.md) for the full diagram, the de-esser/compressor/doubler DSP design, and the real-time-safety notes. Seraph reports 0 samples of latency: nothing in the chain needs host-side plugin delay compensation.

## Parameters

| Parameter | Range | Default | Unit |
|---|---|---|---|
| De-Ess | 0-100 | 30 | % |
| De-Ess Freq | 3,000-12,000 | 7,000 | Hz |
| De-Ess Listen | off/on | off | - |
| Air | -12 to +12 | +3 | dB |
| Comp | 0-100 | 0 | % |
| Double | 0-100 | 25 | % |
| Double Detune | 0-50 | 15 | cents |
| Double Width | 0-100 | 100 | % |
| Mix | 0-100 | 100 | % |
| Output | -24 to +24 | 0 | dB |

Full descriptions of what each parameter does musically are in [`docs/manual.md`](docs/manual.md).

## Roadmap

Tracked as GitHub milestones and issues (M1 DSP & tests - done for v0.1.0 - · M2 presets/state · M3 custom GUI & a11y · M4 release/signing/v1.0.0). Read them with `gh issue list` / `gh api repos/basilica-audio/seraph/milestones`.
<!-- ==END BODY== -->

## Installation

No pre-built binaries are published yet (see the work-in-progress notice above). Once releases begin, installation will follow the standard plugin locations:

**macOS**

| Format | Path |
|---|---|
| AU (Component) | `~/Library/Audio/Plug-Ins/Components/` |
| VST3 | `~/Library/Audio/Plug-Ins/VST3/` |

If Logic Pro doesn't pick up the plugin after installing, force a rescan by resetting the AU cache:

```sh
killall -9 AudioComponentRegistrar
auval -a
```

**Windows**

| Format | Path |
|---|---|
| VST3 | `C:\Program Files\Common Files\VST3\` |

## Building from source

Requires JUCE 8.0.14, C++20, and CMake ≥ 3.24. See [`docs/building.md`](docs/building.md) for full prerequisites and step-by-step build/test commands for macOS and Windows.

```sh
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

## License

Seraph is licensed under the [GNU Affero General Public License v3.0](LICENSE) (AGPLv3).

This project uses [JUCE](https://juce.com) 8, whose open-source tier is licensed under AGPLv3 (as of JUCE 8; JUCE 7 and earlier used GPLv3), which is why this project is AGPLv3 rather than GPLv3. See [`docs/adr/0002-agplv3-licensing.md`](docs/adr/0002-agplv3-licensing.md) for the full reasoning.

VST is a registered trademark of Steinberg Media Technologies GmbH.

Seraph is an independent open-source project and is not affiliated with, endorsed by, or sponsored by any plugin manufacturer.
