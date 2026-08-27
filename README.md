<p align="center"><img src="docs/assets/icon.png" alt="Seraph icon" width="160"/></p>

# Seraph

*Voices from above — a choir and vocal processor for operatic metal vocals.*

[![CI](https://github.com/basilica-audio/seraph/actions/workflows/ci.yml/badge.svg)](https://github.com/basilica-audio/seraph/actions/workflows/ci.yml)
[![License: AGPL v3](https://img.shields.io/badge/License-AGPL%20v3-blue.svg)](https://www.gnu.org/licenses/agpl-3.0)

> **Work in progress.** Seraph is pre-1.0 and under active development. Binaries for macOS and Windows are available from the [Releases](../../releases) page (macOS builds are signed with a Developer ID certificate, notarized and stapled); building from source works too. Expect breaking changes until v1.0.0 ships (see [Roadmap](#roadmap)).

<!-- ==BEGIN BODY== (plugin engineer: replace this block with What it is / Features / Signal flow / Roadmap) -->
## What it is

Seraph is a choir and vocal processor built for operatic metal vocals: it tames sibilance, adds airy openness above the vocal's natural top end, glues dynamics together with a gentle broadband compressor, and thickens a lead or choir line with a click-free four-voice doubler - all in one channel-strip-style plugin. AU / VST3 / Standalone.

See [`docs/manual.md`](docs/manual.md) for the full user manual (signal-flow explanation, complete parameter reference, and mixing tips).

## Features

- **De-Ess** - a single-band dynamic EQ that detects sibilance energy around a tunable center frequency (`DeEssFreq`, ~5-9 kHz) with an adjustable detection bandwidth (`DeEssWidth`), a soft knee (`DeEssKnee`), optional stereo linking (`DeEssLink`) and optional lookahead (`DeEssLookahead`, 0-2 ms, off by default), and reduces it dynamically. A **Listen** mode solos the detected band for tuning by ear.
- **Air** - a high-shelf with a wide, gentle transition for adding (or removing) openness above the vocal's presence range, at a selectable 10/12/15 kHz corner (`AirFreq`).
- **Gentle Compressor** - a single-knob, zero-latency broadband "glue" compressor (`Comp`, 0-100%) with a program-dependent auto-release and optional stereo linking (`CompLink`), scaling threshold and ratio together up to a deliberately gentle 3:1 maximum, sitting after Air and before the Doubler so all doubled voices track a consistent level.
- **Doubler** - four voices at fixed per-voice pan positions (a small-choir spread, not a single symmetric L/R pair), detuned (`DoubleDetune`, in cents) and spread across the stereo field (`DoubleWidth`), blended in (`Double`) on top of the centered dry signal. Three engines (`DoubleMode`): **Classic** modulated-delay chorus, **Micro** constant-offset micropitch, and **Shift** spectral pitch shifting with optional formant preservation (`DoubleFormant`). `Humanize` drifts each voice's timing, pitch and level independently so a stack decorrelates the way real singers do.
- **Mix** / **Output** - overall dry/wet blend and output trim.
- **Presets** - a preset bar with factory and user presets, save/rename/delete, and single-file/bank import-export.

## Signal flow

```
input -> De-Ess (sibilance dynamic EQ, + Width/Knee/Link/Lookahead + Listen mode)
       -> Air (high-shelf, 10/12/15 kHz) -> Gentle Compressor (broadband glue, auto-release, + Link)
       -> Doubler (4 voices, per-voice pan, Classic/Micro/Shift + Humanize)
       -> Output trim -> Mix -> output
```

See [`docs/architecture.md`](docs/architecture.md) for the full diagram, the de-esser/compressor/doubler DSP design, and the real-time-safety notes.

**Latency**: in its default configuration Seraph reports 0 samples, so it needs no host-side delay compensation. Two settings change that on purpose - the doubler's **Shift** mode (~30 ms) and **De-Ess Lookahead** (0-2 ms) - and both are non-automatable for exactly that reason. See the manual's latency section.

## Parameters

| Parameter | Range | Default | Unit |
|---|---|---|---|
| De-Ess | 0-100 | 30 | % |
| De-Ess Freq | 3,000-12,000 | 7,000 | Hz |
| De-Ess Width | 0-100 | 40 | % |
| De-Ess Listen | off/on | off | - |
| Air | -6 to +9 | +2 | dB |
| Comp | 0-100 | 0 | % |
| Double | 0-100 | 25 | % |
| Double Detune | 0-50 | 10 | cents |
| Double Width | 0-100 | 100 | % |
| Mix | 0-100 | 100 | % |
| Output | -24 to +24 | 0 | dB |
| De-Ess Knee | 0-12 | 0 | dB |
| De-Ess Lookahead | 0-2 | 0 | ms |
| De-Ess Link | off/on | off | - |
| Comp Link | off/on | off | - |
| Air Freq | 10/12/15 kHz | 12 kHz | - |
| Double Mode | Classic / Micro / Shift | Classic | - |
| Humanize | 0-100 | 0 | % |
| Formant Preserve | off/on | on | - |

Full descriptions of what each parameter does musically are in [`docs/manual.md`](docs/manual.md); the sourced/reasoned design rationale behind the v0.2.0 ranges/defaults is in [`docs/design-brief.md`](docs/design-brief.md).

## Roadmap

Tracked as GitHub milestones and issues (M1 DSP & tests - done for v0.1.0 - · M2 presets/state - done for v0.2.0 - · M3 custom GUI & a11y · M4 release/signing/v1.0.0). Read them with `gh issue list` / `gh api repos/basilica-audio/seraph/milestones`.
<!-- ==END BODY== -->

## Documentation

- [`docs/manual.md`](docs/manual.md) — the user manual: what every control does, and how to use it
- [`docs/presets.md`](docs/presets.md) — what each factory preset is for
- [`CHANGELOG.md`](CHANGELOG.md) — what shipped in each release
- [Seraph on basilica-audio.github.io](https://basilica-audio.github.io/website/seraph/) — the product page (English and German)

## Installation

Download the archive for your platform from the [Releases](../../releases) page and copy the bundles into the standard plugin locations:

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
