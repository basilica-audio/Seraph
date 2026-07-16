# Seraph deep-dive research notes

Scope: choir/vocal channel strip — de-esser, "air" high-shelf, gentle glue compressor,
click-free vocal doubler. Reference class chosen per module (2-3 each), primary-source
quotes with URLs. NOTE: template file
`/private/tmp/.../scratchpad/miserere-design-brief-v2.md` referenced in the task did
not exist on disk at research time (checked, not found) — this brief follows the
structure described in the task prompt directly (why-v1-falls-short → topology →
module specs w/ sourced defaults → test guarantees → honesty → versioning).

---

## 1. De-esser reference class: FabFilter Pro-DS, Waves Sibilance

### FabFilter Pro-DS
- Sidechain frequency range: **2 kHz – 20 kHz**. "Normal vocal s-sounds usually have
  frequencies around 8 to 10 kHz."
  https://www.fabfilter.com/help/pro-ds/using/basiccontrols
- Two processing modes: **Full Band** ("Pro-DS will lower the overall gain of the audio
  when sibilance is detected") vs **Split Band** ("only high frequencies will be
  attenuated"; split point auto-derived from the HP sidechain filter setting).
  Split-band adds latency; full-band vocal de-essing is often sufficient for a single
  vocal. https://www.fabfilter.com/help/pro-ds/using/advancedcontrols
- Lookahead up to **15 ms**, "about 10 ms is optimal" for vocal work; with lookahead
  off + wideband + no oversampling the plugin runs at zero latency.
  https://www.fabfilter.com/help/pro-ds/using/advancedcontrols
- Single Vocal mode uses a trained/intelligent detector that separates sibilant from
  non-sibilant content rather than a fixed band; Allround mode triggers purely on the
  HP/LP-filtered sidechain band + Threshold.
  https://www.fabfilter.com/help/pro-ds/using/overview

### Waves Sibilance
- **Detection = the Q/width of the band being analyzed** — "lower values zero in on
  just the 'ess-y' part of things, and higher values are a little wider... for more
  'wooshy' kinds of sounds." This is a *user-exposed bandwidth control*, unlike a fixed
  Q.
- **Mode** is a continuous Wide↔Split blend: Wide = full-band gain reduction on
  detected sibilance; **Split = only frequencies above 4 kHz are attenuated** (a
  documented, specific default split point).
- Lookahead control exists "to make detection more subtle and nuanced" (implies a
  standard modern de-esser is not zero-latency by default).
  https://assets.wavescdn.com/pdf/plugins/sibilance.pdf (search-indexed text, above),
  cross-checked via https://www.waves.com/how-to-de-ess-vocals-sibilance-plugin-tutorial

### General ballistics literature
- Patent/engineering literature on combined de-esser+HF-enhancer designs: "appropriate
  attack and release times for the de-essing function are quite fast (attack ≈ 1 ms
  and release ≈ 30 ms)."
  https://image-ppubs.uspto.gov/dirsearch-public/print/downloadPdf/5574791
- Practical mix-engineer starting points repeatedly cited: **attack ~4-5 ms, release
  ~50 ms** as a starting point, tightened/loosened by ear.
  https://unison.audio/de-essing/ , https://producerhive.com/music-production-recording-tips/what-is-a-de-esser/
- Sibilance energy commonly cited center: **4-8 kHz**, narrower than Seraph's current
  3-12 kHz sweep range but consistent with Seraph's 7 kHz default.

### Gap vs. Seraph v1 (`src/dsp/DeEsser.cpp`)
- v1: fixed **1 ms attack / 80 ms release**, fixed **Q = 1.2** 2nd-order bandpass,
  fixed **-28 dBFS threshold**, single `DeEss` amount knob (0-100% → 0-24 dB max
  reduction), no lookahead (deliberate, for the 0-latency invariant), no detection
  bandwidth control, no split/full-band choice, no auto/adaptive threshold.
- The 1 ms attack matches the patent-literature "fast" figure closely; the 80 ms
  release is materially slower than the ~30-50 ms figures cited above — this reads as
  a defensible but *unstated* design choice, not something derived from the reference
  class.
- The single fixed Q = 1.2 (roughly 1.2 octaves wide) is close to Waves Sibilance's
  "wider, woosh-catching" end of its Detection range, but v1 exposes it as a hidden
  constant, not a control — a bandwidth knob is the single most load-bearing
  omission versus both reference plugins.
- A fixed absolute threshold (-28 dBFS) means de-essing behavior is level-dependent:
  a quiet take never crosses -28 dBFS and the de-esser effectively does nothing
  regardless of `DeEss` amount, unlike Pro-DS/Sibilance's relative/adaptive detectors.

---

## 2. "Air" shelf reference class: Maag Audio EQ4 "Air Band"

- Corner design goes **well beyond audible range**: "shelf boost from 2.5 to 40 kHz."
- Measured curve at max boost setting: **~2 dB @ 10 kHz, 3 dB @ 20 kHz, 5 dB @ 40 kHz**
  — i.e. a very gentle, slowly-rising multi-octave shelf, not a sharp single-corner
  shelf.
  https://www.maagaudio.com/manuals/Maag%20Audio%20EQ4%20User%20Guide.pdf (fetched,
  binary/unparseable directly — figures corroborated via search-indexed manual text)
- "Because of the EQ4's unique design, phase shift is very minimal... The AIR version
  sounds much gentler than the dedicated 2.5 kHz shelf band even with ostensibly the
  same [gain] setting" — i.e. deliberately much lower effective gain-per-dB-setting
  than a conventional shelf, and the bands are documented as *interacting* (summing)
  with the EQ's other bands rather than being a clean isolated filter.
  https://gearspace.com/board/so-much-gear-so-little-time/1341266-how-far-does-the-air-band-shelf-on-a-maag-eq4-actually-reach.html
  (search-indexed), https://www.maagaudio.com/manuals/Maag%20Audio%20EQ4%20User%20Guide.pdf

### Gap vs. Seraph v1 (`SeraphEngine`'s Air stage, `docs/architecture.md` §Air)
- v1: single `juce::dsp::IIR::Coefficients::makeHighShelf`, **fixed 12 kHz corner**,
  Butterworth Q, **±12 dB range**. This is a conventional single-corner RBJ shelf —
  the opposite design philosophy of the reference unit, which spends its entire
  design effort on making a shelf sound *unnaturally gentle and phase-clean* by
  pushing the mathematical corner far above the audible band.
- ±12 dB is roughly 2-4x the *usable* gain range of the reference unit (whose real
  curves top out around 5-6 dB audible lift even at max boost) — at Seraph's higher
  Air settings the fixed-Q 12 kHz shelf will sound comparatively harsh/peaky rather
  than airy, because the boost is concentrated closer to the audible corner instead of
  smeared across 10-40 kHz.
- No documented low-Q "spread" behavior or corner-frequency choice — real air-EQs
  commonly offer a small number of selectable corner frequencies (their manual lists
  multiple Air Band frequency options), whereas Seraph's Air is a single frozen
  12 kHz constant.

---

## 3. Glue compressor reference class: SSL G-Series Bus Compressor

- Threshold: **continuously variable, -15 dB to +15 dB** (relative/hardware scale).
- Ratio: **switched, 2:1 / 4:1 / 10:1** — "2:1 for the most transparent sound... 10:1
  for tougher, more audible sound... 4:1 for more moderate compression."
- Attack and Release are **switched, multi-position**, not continuous.
- Release: **switched between 0.1, 0.3, 0.6, 1.2 s, or Auto** — "Auto setting provides
  a program-dependent, multi-stage release for the greatest degree of transparency."
- "The knee point of the compressor, set with the THRESHOLD control, purposely changes
  depending on the setting of the RATIO control" — i.e. ratio and threshold are
  *not* simply coupled on one continuous "amount" axis; they are independent switches
  whose interaction is a deliberately voiced, documented behavior.
  https://help.uaudio.com/hc/en-us/articles/30847649785748-SSL-4000-G-Bus-Compressor-Manual
  (search-indexed summary; source PDFs at
  https://www.solidstatelogic.com/assets/uploads/downloads/SSL_500_Series_G_Comp_Module_User_Guide.pdf
  and https://assets.wavescdn.com/pdf/plugins/ssl-g-master-buss-compressor.pdf fetched
  but returned binary/unparseable text — figures corroborated via the UA manual
  summary page, cross-referenced against well-documented public knowledge of the
  4000 G bus compressor's switched control layout, which is consistent across
  multiple hardware/plugin manuals of this unit)

### Gap vs. Seraph v1 (`src/dsp/GentleCompressor.cpp`)
- v1: single `Comp` knob (0-100%) linearly co-scales threshold (0 → -20 dBFS) and
  ratio (1:1 → 3:1) *together*, fixed **15 ms attack / 150 ms release** one-pole
  ballistics, no program-dependent/auto release.
- The reference class's defining "glue" character comes specifically from the
  **Auto program-dependent release** (a multi-stage envelope that reacts differently
  to transients vs. sustained program material) — this is the single most-cited
  reason engineers reach for an SSL-style bus compressor over a plain VCA compressor,
  and it is entirely absent from v1's fixed single-time-constant release.
- Coupling threshold and ratio into one knob is a reasonable minimal-UI simplification
  (documented as deliberate), but the reference class treats ratio as the primary
  "flavor" choice and threshold as the "how much" choice — v1's coupling forces them
  to move together, which cannot reproduce e.g. "heavy ratio, light threshold" or
  vice versa.
- No fixed release-time menu / auto-release model in v1 at all — a single 150 ms
  one-pole is a plausible average of the reference's 0.1-1.2 s switched range, but is
  not sourced to a specific behavior in the reference class.

---

## 4. Vocal doubler reference class: Abbey Road ADT, Eventide MicroPitch/H3000, Roland Dimension D

### Abbey Road ADT (Ken Townsend, 1966)
- Core technique: a **single delayed copy** of the signal, played back at a
  varispeed-modulated capstan rate controlled by an oscillator, summed with the
  original.
- "The number of milliseconds between the two voices was totally controllable but
  **normally between 8 and 12 milliseconds**."
  https://en.wikipedia.org/wiki/Automatic_double_tracking ,
  https://www.abbeyroad.com/news/inside-abbey-road-artificial-double-tracking-2530
- The oscillator-driven varispeed "lent the effect of human touch" — i.e. the pitch
  wobble is the *point*, not a side effect to be minimized; this matches Seraph's own
  design philosophy (continuous modulation vs. discrete pitch-shift resets), but at a
  much *tighter* delay-time neighborhood (8-12 ms) than Seraph's 13-29 ms base delays.

### Eventide MicroPitch / classic H3000 vocal-doubler patches
- Typical modern MicroPitch vocal-doubling settings: **"4-12 cents pitch-shift, up and
  down on left and right, with delay time anywhere between 6 and 20 ms."**
  https://splice.com/blog/the-classic-vocal-mix-trick-every-producer-should-know/
- Widely-cited H3000 recreations: **"one side detuned -9 cents with 15 ms delay and
  the other +11 cents with 25 ms delay"**; or **"Left: 15 ms delay / +0.05 [semitone,
  ≈5 cents], Right: 20 ms delay / -0.05."** A common simplified modern recreation
  uses **±7 cents** symmetric.
  https://gearspace.com/board/high-end/35224-classic-h3000-vocal-preset.html
- The historically famous H3000 preset #519 ("Micro Pitch Shift / Multi-Shift") is
  Eventide's own description of the "fatten/widen without adding color" goal — i.e.
  the whole reference class treats **single-digit-to-low-teens cents** as the
  "doesn't sound like an obvious effect" zone, and treats delay times as tightly
  clustered **6-25 ms**, not spread out to 30 ms+.

### Roland Dimension D (SDD-320, 1981) — chorus/doubler cousin
- Four fixed **preset modes**, not continuous rate/depth knobs: "Mode 1 has slow rate
  (0.25 Hz) and low depth; Mode 2 has slow rate and normal depth; Mode 3 has fast rate
  (0.5 Hz) and normal depth; Mode 4 has fast rate and normal depth with boost."
- "The depth is actually reduced in modes 3 and 4 to compensate for the higher rate...
  the amount of pitch shift is roughly the same as mode 2" — i.e. the unit's designers
  deliberately kept the *perceived* modulation depth constant across rate changes by
  trading depth against rate, exactly the same math Seraph already uses for
  `DoubleDetune` (peak pitch-ratio deviation solved per-voice from its own LFO rate) —
  this is a strong validation of Seraph's existing approach, not a gap.
- LFO is a **triangle wave**, 180° out of phase between channels (BBD-based, 1024
  stages/channel). Notably *not* sinusoidal, unlike Seraph's per-voice sine LFOs.
  https://www.vintagedigital.com.au/roland-sdd-320-dimension-d-chorus/ ,
  https://www.muzines.co.uk/articles/roland-dimension-d/4054

### Gap vs. Seraph v1 (`src/dsp/Doubler.cpp`, `docs/architecture.md` §Doubler)
- v1: four voices at **13/17/23/29 ms** base delay, `DoubleDetune` **0-50 cents**
  shared across all voices, `DoubleWidth` scales a fixed 4-voice pan spread
  (-1, -1/3, +1/3, +1).
- Delay range (13-29 ms) sits *above* both reference neighborhoods (ADT: 8-12 ms;
  MicroPitch doubling: 6-25 ms, center-of-mass ~15-20 ms) — Seraph's shortest voice
  (13 ms) is already at the outer edge of "tight ADT" territory, and its longest voice
  (29 ms) is beyond typical vocal-doubler delay times into more obviously
  chorus/slapback territory.
- Detune range (0-50 cents, default 15) dwarfs the reference class's "doesn't sound
  like an effect" zone (4-12 cents, occasionally up to ~15-20 cents for a looser
  double). 50 cents at the top of Seraph's range is a genuinely large, obviously
  chorus-y wobble by reference-class standards — this isn't wrong for a "small choir
  spread" design goal, but the current single linear 0-50 range doesn't distinguish
  between "tight double" territory (≤12 cents) and "loose chorus" territory (>20
  cents), and the 15-cent default sits in a reasonable but unsourced middle.
- All four voices currently share one `DoubleDetune` value; every reference example
  found uses **asymmetric per-side cents** (e.g. -9/+11, or matched-magnitude opposite
  sign) rather than a single shared magnitude — Seraph's existing per-voice distinct
  base delay + LFO rate already produces de-correlation, so this is a minor, not
  urgent, gap.

---

## 5. Cross-cutting takeaways for the brief

1. De-esser: add a **bandwidth/detection-width** control (the single most load-bearing
   missing control vs. both reference plugins), and reconsider whether a fixed
   absolute -28 dBFS threshold vs. a level-relative detector better serves varied
   input gain-staging. Keep zero-latency/no-lookahead as the documented tradeoff
   (explicitly *not* matching Pro-DS/Sibilance's optional lookahead — that's a real,
   sourced design departure to state honestly, not silently miss).
2. Air: reshape the fixed 12 kHz conventional shelf toward a gentler, wider,
   multi-octave-feeling curve and/or reduce max gain range from ±12 dB toward
   something closer to the reference class's effective ±6 dB, so high settings still
   read as "air" rather than "harsh boost."
3. Comp: the single biggest authentically-voiced upgrade is a **program-dependent
   (auto) release** behavior, not just a faster/slower fixed one-pole. This is the
   most load-bearing, most-cited differentiator of the reference class.
4. Doubler: tighten the base-delay neighborhood toward the 8-25 ms reference cluster
   (currently 13-29 ms), and reconsider the detune range/taper so the "tight double"
   register (≤12 cents) and "loose chorus" register (>20 cents) are both reachable
   with sensible resolution — e.g. reshape the control taper rather than changing the
   underlying architecture, which the Dimension-D cross-check already validates.
