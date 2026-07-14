# Seraph

*Voices from above — a choir and vocal processor for operatic symphonic metal.*

[![CI](https://github.com/metal-up-your-ass/seraph/actions/workflows/ci.yml/badge.svg)](https://github.com/metal-up-your-ass/seraph/actions/workflows/ci.yml)
[![License: AGPL v3](https://img.shields.io/badge/License-AGPL%20v3-blue.svg)](https://www.gnu.org/licenses/agpl-3.0)

> **Work in progress.** Seraph is pre-1.0 and under active development. There are no built binaries or releases yet — building from source is currently the only way to run it. Expect breaking changes until v1.0.0 ships (see [Roadmap](#roadmap)).

<!-- ==BEGIN BODY== (plugin engineer: replace this block with What it is / Features / Signal flow / Roadmap) -->
## What it is

Seraph is a choir and vocal processor built for operatic symphonic-metal vocals: it tames sibilance, adds airy openness above the vocal's natural top end, and thickens a lead or choir line with a click-free stereo doubler - all in one channel-strip-style plugin. AU / VST3 / Standalone.

## Features

- **De-Ess** - a single-band, zero-latency dynamic EQ that detects sibilance energy around a tunable center frequency (`DeEssFreq`, ~5-9 kHz) and reduces it dynamically, without a lookahead delay.
- **Air** - a fixed-frequency (12 kHz) high-shelf for adding (or removing) openness above the vocal's presence range.
- **Doubler** - two short, independently modulated-delay voices, slightly detuned (`DoubleDetune`, in cents) and panned left/right (`DoubleWidth`), blended in (`Double`) on top of the centered dry signal - a classic vocal-doubling trick, implemented click-free (continuous delay modulation, never a discrete pitch-shift reset).
- **Mix** / **Output** - overall dry/wet blend and output trim.

## Signal flow

```
input -> De-Ess (sibilance dynamic EQ) -> Air (high-shelf) -> Doubler (2 detuned voices, panned L/R) -> Output trim -> Mix -> output
```

See [`docs/architecture.md`](docs/architecture.md) for the full diagram, the de-esser/doubler DSP design, and the real-time-safety notes. Seraph reports 0 samples of latency: nothing in the chain needs host-side plugin delay compensation.

## Roadmap

Tracked as GitHub milestones and issues (M1 DSP & tests - done for v0.1 - · M2 presets/state · M3 custom GUI & a11y · M4 release/signing/v1.0.0). Read them with `gh issue list` / `gh api repos/metal-up-your-ass/seraph/milestones`.
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
