# Third-Party Notices

Seraph itself is licensed under the GNU Affero General Public License v3.0
(see `LICENSE`). It additionally incorporates the third-party components
listed below. Each of them is distributed under a licence that is compatible
with the AGPLv3, and each licence's required copyright/permission notice is
reproduced in full.

Components fetched at configure time by CPM (`CMakeLists.txt`) are not
vendored into this repository as source; the pins recorded here are what a
build of this tag resolves.

---

## Signalsmith Stretch

Used by `src/dsp/SpectralShifter.h` / `.cpp` as the STFT pitch-shifting and
formant-preservation engine behind the doubler's `Shift` mode (see
`docs/architecture.md`).

- Upstream: https://github.com/Signalsmith-Audio/signalsmith-stretch
- Pinned commit: `57b93f4e9206a089a45387eaa39bdc9f310d3308` (header version 1.3.2)
- Licence: MIT

The pin is a development-head commit rather than the newest release tag
(`1.1.0`) on purpose: the formant API this plugin relies on
(`setFormantSemitones()`, `setFormantBase()`) was added after `1.1.0` and has
not yet been tagged. The pin is a full commit SHA, so the resolved source is
exact and reproducible.

```
MIT License

Copyright (c) 2022 Geraint Luff / Signalsmith Audio Ltd.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

---

## Signalsmith Linear

A transitive dependency of Signalsmith Stretch (its STFT/FFT backend). Seraph
does not include it directly; it is added to the include path so that
`signalsmith-stretch/signalsmith-stretch.h` resolves
`signalsmith-linear/stft.h`.

- Upstream: https://github.com/Signalsmith-Audio/linear
- Pinned tag: `0.3.1`
- Licence: MIT

Seraph deliberately consumes this package as headers only, without its own
CMakeLists: that CMakeLists defaults `SIGNALSMITH_USE_ACCELERATE` to `ON` for
Apple targets and defines `ACCELERATE_NEW_LAPACK`, whose symbols are not
available at Seraph's macOS 11.0 deployment target. The portable C++ FFT path
is used instead.

```
MIT License

Copyright (c) 2025 Signalsmith Audio

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

---

## JUCE

The application framework. Used under the terms of the AGPLv3 option of the
JUCE 8 licence, which is why Seraph itself is AGPLv3 (see
`docs/adr/0002-agplv3-licensing.md`) and why `JUCE_DISPLAY_SPLASH_SCREEN=0` is
permitted.

- Upstream: https://github.com/juce-framework/JUCE
- Pinned tag: `8.0.14`
- Licence: AGPLv3 (with commercial options not exercised here)

## Catch2

Test-only dependency; not linked into any shipped plugin binary.

- Upstream: https://github.com/catchorg/Catch2
- Pinned tag: `v3.15.2`
- Licence: Boost Software License 1.0

## CPM.cmake

Build-tooling dependency (`cmake/CPM.cmake`); not linked into any shipped
binary.

- Upstream: https://github.com/cpm-cmake/CPM.cmake
- Licence: MIT
