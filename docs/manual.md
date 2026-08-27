<p align="center"><img src="assets/icon.png" alt="Seraph icon" width="120"/></p>

# Seraph — user manual

*Voices from above — a choir and vocal processor for operatic metal vocals.*

## What Seraph is

Seraph is a channel-strip-style vocal processor built for the lead and choir vocal parts of operatic metal (big, cinematic productions): a soprano lead line, a layered choral backing, or a spoken/growled interlude that needs to sit cleanly against heavy layered guitars and an orchestra without disappearing or turning harsh.

It combines four processing stages that are normally reached for separately on a vocal:

1. **De-Ess** - tames sibilance ("s", "sh", "t" consonants) that a bright vocal mic and heavy top-end EQ elsewhere in the mix (cymbals, distorted guitar fizz, string sections) tend to make fatiguing.
2. **Air** - adds (or removes) the sense of airy openness above the vocal's natural presence range, the kind of shimmer that helps an operatic soprano cut through a wall of guitars.
3. **Gentle Compressor** - evens out dynamics with a "glue" style compressor, so the vocal sits at a consistent level in the mix without audibly pumping.
4. **Doubler** - a four-voice vocal doubler that thickens a single take into a small-choir spread, in three selectable engines (see [Doubler modes](#doubler-modes) below).

Everything downstream of Mix/Output is a single self-contained channel strip: put Seraph on a vocal or choir bus, dial in de-essing and air to taste, add a touch of glue compression if the take is dynamically uneven, and use the doubler to widen a lead line or thicken a choir part.

## Where it sits in a heavy-music signal chain

Seraph is designed to run on vocal/choir tracks or a vocal bus, typically:

```
Vocal/choir recording -> (tuning/editing, if used) -> Seraph -> reverb/delay send -> mix bus
```

In its default configuration Seraph reports **0 samples of latency**, so it needs no host-side delay-compensation accounting and is safe to insert anywhere in a vocal chain, including in parallel. Two settings change that deliberately - see [Latency and delay compensation](#latency-and-delay-compensation).

A few practical placements in a heavy-music production:

- **Lead vocal track**: De-Ess first (mic proximity and consonants), a touch of Air to help an operatic voice cut through distorted guitars and orchestral strings, a little Comp for consistency, and a *subtle* amount of Double (10-20%) if the take needs filling out without sounding artificially doubled.
- **Choir/backing vocal bus**: heavier Double (40-70%) with full Width for a wide, layered choir spread from a smaller number of recorded takes; De-Ess and Air set more conservatively since choir blends are usually already less sibilant/harsh per-voice than a solo lead.
- **Spoken-word/growled interlude**: De-Ess is often unnecessary (little sibilance energy in a growled performance); Air and a stronger Comp setting help a spoken interlude stay present and level-consistent against a quiet orchestral backing.

## Signal flow

```
input -> De-Ess (sibilance dynamic EQ, + Width/Knee/Link/Lookahead + Listen mode)
       -> Air (10/12/15 kHz high-shelf) -> Gentle Compressor (broadband glue, auto-release, + Link)
       -> Doubler (4 voices, per-voice pan, Classic/Micro/Shift + Humanize)
       -> Output trim -> Mix -> output
```

See [`architecture.md`](architecture.md) for the full technical signal-flow diagram and DSP design notes, and [`design-brief.md`](design-brief.md) for the v0.2.0 research-derived voicing pass behind the ranges/defaults below.

## Presets

Seraph ships with a preset bar docked at the top of the plugin window: browse Factory and User presets from the name menu, step through them with the `<`/`>` arrows, and use Save/Save As.../Delete/Import.../Export... to manage your own. Twelve factory presets cover lead, choir, spoken-interlude, and single-stage utility use cases - see [`presets.md`](presets.md) for the full list and each preset's intent. "Set current as default" (in the preset name menu) sets what loads the next time you open a fresh instance of Seraph. User presets are stored per-user (`~/Library/Audio/Presets/Basilica Audio/Seraph/` on macOS) and can be exported/imported as single files or shared as a bank.

## Parameter reference

| Parameter | Range | Default | Unit | What it does |
|---|---|---|---|---|
| **De-Ess** | 0-100 | 30 | % | Sibilance gain-reduction amount. Scales the maximum reduction applied to the detected band (up to 24 dB at 100%). 0% is an exact bypass of the de-esser. Start low (20-40%) and raise only as far as needed - overdoing de-essing makes "s" sounds sound lisped or muffled. |
| **De-Ess Freq** | 3,000-12,000 | 7,000 | Hz | Center frequency of the sibilance detection/reduction band. Female/soprano vocals often sibilate higher (7-9 kHz); lower male vocals or heavily proximity-mic'd takes may need 5-6 kHz. Use **De-Ess Listen** to find the right frequency by ear. |
| **De-Ess Width** | 0-100 | 40 | % | Detection bandwidth of the sibilance band. Lower values narrow the detector onto just the "ess" energy (more surgical, less likely to catch other high-frequency content); higher values widen it to catch "sh"/breathy/"woosh"-type sibilance too. If De-Ess is reacting to the wrong sound, try adjusting Width before reaching for De-Ess Freq. |
| **De-Ess Listen** | off/on | off | - | Solos the detected sibilance band instead of the processed vocal, so you can sweep De-Ess Freq/Width and hear exactly which frequency content is being targeted before dialling in reduction. Switch back off before mixing - Listen mode is a tuning aid, not a mix setting. |
| **Air** | -6 to +9 | +2 | dB | Fixed 12 kHz high-shelf with a wide, gentle transition (starts rising well before the corner). Boost for openness/shimmer above a vocal's natural top end (typical for a lead that needs to cut through a dense mix); cut if a bright mic/preamp or aggressive de-essing has left the vocal sounding thin or harsh. |
| **Comp** | 0-100 | 0 | % | Gentle broadband downward-compressor amount with a program-dependent ("auto") release: recovers quickly after an isolated loud moment, glues more audibly during sustained loud passages. Scales both threshold (down to -20 dBFS) and ratio (up to 3:1) together - a "glue" setting, not a squashing limiter. 0% is an exact bypass. No automatic makeup gain is applied; use **Output** to compensate if a higher Comp setting makes the vocal feel quieter. |
| **Double** | 0-100 | 25 | % | Doubler send amount: how much of the four doubled voices blends in on top of the centered dry signal. 0% is an exact bypass of the doubler. Subtle amounts (10-25%) thicken a lead without an obvious "chorus" effect; higher amounts (40%+) build a fuller small-choir spread, best suited to backing/choir parts rather than an exposed lead line. |
| **Double Detune** | 0-50 | 10 | cents | Depth of the doubler's continuous pitch wobble (a smooth modulated-delay detune, not a discrete pitch shift - always click-free). The knob spends more of its travel in the low-cents range: values around 5-12 cents sound like a tight, subtle double; the upper end (30-50 cents) sounds looser and more chorus-like. |
| **Double Width** | 0-100 | 100 | % | Stereo spread of the doubler's four voices. 0% keeps all four voices centered (mono-compatible, useful if the vocal needs to stay centered in a mono-fold-down-sensitive mix); 100% spreads them across the full stereo field for a wide choir effect. |
| **Mix** | 0-100 | 100 | % | Overall dry/wet blend. Defaults to 100% (fully processed) since Seraph is meant to be run as a full channel strip, not blended - lower it only for parallel-processing setups (e.g. blending in a de-essed/doubled signal under an otherwise-untouched dry vocal). |
| **Output** | -24 to +24 | 0 | dB | Output trim, applied after the doubler and before Mix. Use to compensate level changes introduced by Comp or Double before the signal hits the next stage in your chain. |
| **De-Ess Knee** | 0-12 | 0 | dB | How gradually de-essing engages around its threshold. At 0 the reduction snaps on the moment sibilance crosses the threshold - surgical, and audible as a "grab" on borderline consonants. Raising it starts reducing gently below the threshold and reaches full strength above it, which reads as a de-esser that is simply always slightly there rather than one that catches. 4-8 dB suits a lead vocal; leave it at 0 for surgical repair work. |
| **De-Ess Lookahead** | 0-2 | 0 | ms | Lets the de-esser see the ess coming, so the gain is already down when it arrives instead of catching up over the first millisecond. Removes the bright "tick" at the front of a hard consonant that no amount of extra reduction fixes. **Adds latency** (see below) and is not automatable. 1-2 ms is plenty; there is nothing to gain from more, which is why the range stops there. |
| **De-Ess Link** | off/on | off | - | Off, each channel is de-essed by its own detector. On, both channels are reduced together by whichever is louder. Turn it on for anything stereo where an ess should not shove the image sideways - a doubled or spread choir, a stereo room take. Leave it off for two genuinely unrelated mono sources. |
| **Comp Link** | off/on | off | - | The same idea for the compressor: on, one shared envelope (including the auto-release) drives both channels, so the stereo image stays put under compression. Recommended on a stereo vocal bus. |
| **Air Freq** | 10/12/15 kHz | 12 kHz | - | Where the Air shelf starts lifting. 12 kHz is what Seraph has always used. 10 kHz reaches further down into presence, useful on a darker take or a duller mic. 15 kHz stays out of the sibilance region entirely, which pairs well with heavy de-essing - you can add openness without feeding the ess you just removed. |
| **Double Mode** | Classic / Micro / Shift | Classic | - | Which doubler engine runs. See [Doubler modes](#doubler-modes). Not automatable, because two of the three report different latency. |
| **Humanize** | 0-100 | 0 | % | How much each doubled voice drifts on its own - slowly, in timing, pitch and level. At 0 the voices are mathematically related to each other, which is what a doubler has always sounded like. Raising it decorrelates them the way four real singers never quite agree. 20-40% is enough to remove the machine quality; higher settings get loose and choir-like. Deterministic: the same settings always produce the same drift. |
| **Formant Preserve** | off/on | on | - | Only active in Shift mode. On, the voice keeps its own vowel character while the pitch moves. Off, the formants move with the pitch. Within Seraph's +/-50 cent range the difference is subtle either way, so this is mostly insurance for the Shift engine's higher-quality path. |

All parameters are smoothed (no zipper noise on automation or manual knob moves). All are safe to automate except **Double Mode** and **De-Ess Lookahead**, which change reported latency - see below.

## Doubler modes

The three modes share the same four voices, the same per-voice pan positions and the same Amount/Detune/Width laws, so switching between them keeps the arrangement and changes only how the detune is produced. A switch is masked by a short fade, so you can audition them while audio is running.

| Mode | What it does | Latency | Use it for |
|---|---|---|---|
| **Classic** | The engine Seraph has always had: each voice's delay line is wobbled by a slow sine, which shifts pitch continuously up and down around the note. Never in tune, never out of tune. | None | The familiar Seraph doubler sound. Anything that shipped before v0.3.0 uses this and is unchanged. |
| **Micro** | A real constant detune - each voice sits a fixed number of cents away and stays there. Accurate to well under a cent. | None | Stacks that need to hold an interval: choir parts, wide lead doubles, anywhere the Classic wobble reads as "chorus" when you wanted "another singer". Slappier than Classic by design (see below). |
| **Shift** | Spectral pitch shifting, with the option to hold the vowel's character in place while the pitch moves. The most accurate and the most expensive. | ~30 ms | The cleanest doubling, and the mode to reach for when Detune is pushed toward the top of its range. |

Two things are worth knowing about **Micro**. Its voices sit further back in time than Classic's - around 34-49 ms rather than 9-24 ms - because the pitch shift is produced by continuously sliding the delay. That is a deliberate character difference, not a fault: Micro is slappier and reads as a wider, more separate double. And because that ~25 ms is the effect rather than processing delay, Micro reports no latency at all; if you need the doubled voices tight against the dry signal, Classic is the tighter mode.

**Shift** is the only mode that reports latency, and it is the only one where the plugin has to be delay-compensated by the host. Every current DAW does this automatically.

## Latency and delay compensation

| Setting | Reported latency at 48 kHz |
|---|---|
| Default (Classic, no lookahead) | 0 samples |
| Micro mode | 0 samples |
| Shift mode | 1440 samples (30.0 ms) |
| De-Ess Lookahead at 2 ms | 96 samples |

The two add: Shift mode with 2 ms of lookahead reports 1536 samples. The figure scales with sample rate - Shift mode is always ~30 ms, so it is 2880 samples at 96 kHz.

Both settings are **not automatable**, on purpose. Hosts cope badly with a latency change arriving mid-automation, so Seraph only ever changes what it reports in response to a deliberate move on your part, and masks the change itself with a 10 ms fade. You can still switch modes with audio running; you just cannot draw it into an automation lane.

When either is engaged, the whole plugin - including the dry side of the Mix control - is delayed by the reported amount, so Mix stays a clean blend rather than a smear. Parallel routing still works; your DAW's delay compensation aligns the Seraph-processed path against the untouched one automatically.

These numbers are measured, not asserted: a click pushed through the plugin arrives where the reported latency says it will, to within one sample, in Micro, in Shift, in Shift with 2 ms of lookahead, and in lookahead alone.

## Under the hood

A few mechanisms worth knowing about if you want to understand why the three doubler engines and the de-esser's lookahead behave the way they do, not just what the knobs are labelled:

**Micro holds a real interval by continuously ramping a delay, not by hopping pitch.** A delay line whose length changes at `dτ/dt = 1 − r` produces exactly the pitch ratio `r`; Micro reads that delay with cubic Catmull-Rom interpolation through a dual-head design, crossfading so that whichever head is mid-wrap at a given instant is silent then. Measured accurate to 0.5 cents at ±30 cents, and to a −100 dBFS null against a plain static delay at zero detune - because at zero detune that's exactly what it becomes. The sweep runs upward from the base delay rather than centred on it, since centring would mean reading samples that haven't arrived yet; the consequence is the 34-49 ms voice delay described above, documented rather than hidden.

**Shift mode runs the MIT-licensed Signalsmith Stretch phase vocoder, one instance per voice**, pinned to a fixed upstream commit and configured against Seraph's own latency budget rather than the engine's own defaults: a 30 ms window at a 7.5 ms hop, specified in seconds (not as a bin count) so a 96 kHz session keeps the same physical window instead of silently halving it. The engine's own default preset would use roughly 150 ms, which is well outside what a tracking-vocal insert can afford. It's vendored rather than hand-rolled for a specific reason: the alternative techniques (LPC/cepstral formant estimation, TD-PSOLA-style pitch marking) both assume a monophonic source and degrade on exactly the material Seraph is routinely fed - stacked vocals and choir buses. Full license text lives in `THIRD-PARTY-NOTICES.md`.

**Humanize drifts each voice independently but deterministically.** Three slow random walks per voice - timing, pitch and level - run from a seeded generator through a slow one-pole filter, on a fixed control clock that never consults the host's block size. Two renders from the same reset state come out bit-identical, and a host handing over audio 64 samples at a time produces exactly the same drift as one handing it over 256 samples at a time. At 0% every offset is exactly zero, so the Classic engine stays bit-identical to v0.2.0.

**De-Ess Lookahead delays the detected band along with the audio, not just the audio.** The de-esser works by adding a scaled copy of the detected sibilant band back onto the signal, which only reduces level if the two are time-aligned. The tempting simpler implementation - delay the audio path and run the detector on the undelayed input - would leave the two misaligned by the lookahead length, and because sibilance is effectively noise-like and decorrelated at a 2 ms lag, that misalignment would make the "subtraction" add roughly 0.8x the band's power back in at maximum reduction: the de-esser would boost esses instead of reducing them. Seraph delays both, runs the detector on the undelayed band, and passes the resulting gain through a sliding-minimum window so it reaches its target before the delayed ess arrives rather than chasing it. Measured effect: the onset overshoot a fast attack would otherwise let through drops to at most 0.5 dB of excess over the settled reduction, versus more than 3 dB without lookahead engaged.

**Engineering hygiene:** 108 test cases run on macOS and Windows on every push, plus pluginval at strictness 10 and `auval -strict` - covering zero heap allocations on the audio thread under a replaced allocator (including live mode switches inside the guard), a neutral null at −90 dBFS or better across six sample rates from 44.1 to 192 kHz, and Shift mode processing a full second of 48 kHz stereo audio in well under 100 ms of wall-clock time in a Release build.

## Tips

- **De-ess before adding Air or Comp.** Sibilance energy sits in the same region Air boosts, and a broadband compressor will react to sibilant peaks just like any other transient - de-essing first keeps both of those stages working on a cleaner signal.
- **Use De-Ess Listen when you're not sure where the sibilance is.** It's much faster to sweep De-Ess Freq while soloing the detected band than to sweep it by ear against the full mix.
- **Reach for De-Ess Width before widening De-Ess Freq's sweep.** If de-essing is catching the wrong sound (too "woosh"y, or missing the actual "s"), narrowing Width (lower values) first is usually more surgical than moving the center frequency.
- **Comp's release adapts on its own - you don't need a release knob.** It stays fast and transparent on isolated loud moments, and glues more noticeably during sustained loud passages, without any extra control to set.
- **Comp is a glue knob, not a leveling tool.** If a take has wildly inconsistent level (very quiet verses, very loud choruses), fix that with clip gain or a dedicated leveling compressor upstream first; Comp's gentle 3:1 maximum ratio is meant to add consistency and cohesion on an already-reasonably-level take, not to rescue a wildly uneven one.
- **Double is additive, not a replacement for real doubled takes.** For choir parts, a handful of Double at low-to-moderate amounts on top of a couple of real recorded layers usually sounds fuller and more natural than relying on Double alone to simulate an entire choir from a single take.
- **Watch Width on a mono-sensitive mix.** If your material may be folded to mono (streaming platforms, some broadcast chains), check the doubler with Width pulled back toward 0% to make sure the doubled voices don't cancel unpleasantly when summed.
- **Try Micro before reaching for more Detune.** If a double sounds like a chorus effect rather than a second singer, the problem is usually the Classic engine's wobble, not the amount of detune. Micro at 8-14 cents often reads as "another take" where Classic at the same setting reads as "effect".
- **Humanize is what stops a stack sounding synthetic.** Four voices at an exact detune are still four copies of one performance. A little drift - 20-30% - is usually the difference between a doubler you notice and one you don't.
- **Turn the links on for stereo sources.** De-Ess Link and Comp Link both cost nothing and both prevent the same problem: a loud moment on one side quietly shifting the whole image.
- **Pair Air Freq at 15 kHz with heavier de-essing.** It lets you add openness above the sibilance region instead of on top of it.
- **Seraph is still safe in parallel chains.** In its default configuration it reports no latency at all; when Shift mode or lookahead is engaged, your DAW's delay compensation handles the alignment, and the dry side of Mix is delayed internally to match.

## The editor

Seraph ships the suite's M3 vector editor: a fully runtime-drawn black/gold surface (no bitmap assets) with pointer knobs on engraved scale rings, lamp toggles, and five stage panels laid out in signal-flow order - **De-Ess**, **Air**, **Compressor**, **Doubler**, and **Output**. Choice parameters (Doubler Mode, the Air shelf corner) are detented knobs that snap to and announce their mode names.

Two needle meters show live gain reduction: **ESS** on the De-Ess panel and **COMP** on the Compressor panel, both in dB of reduction with gentle meter ballistics (the needle rests right at 0 dB and sweeps left as reduction deepens).

### Accessibility

The editor is built to WCAG 2.1 AA:

- **Keyboard**: every knob and toggle is Tab-reachable. Arrows step knobs by 1% of their range, Shift+Arrow steps fine (0.1%), PageUp/PageDown by 10%, Home/End jump to the extremes; choice knobs step exactly one mode per arrow press. Toggles flip with Space/Enter. Shift-drag is the mouse analog of Shift+Arrow (fine adjustment).
- **Visible focus**: a gold focus ring (with a dark halo) marks the focused control at all times.
- **Screen readers**: every control exposes its printed label as its accessible name, its value with unit ("40.0 %", "7000 Hz"), and its proper role; the five panels are announced as named groups; the meters expose their smoothed reading as a read-only value on demand.
- **Contrast**: all rendered text/marking pairs hold at least 4.5:1 (WCAG AA), enforced by unit tests against the exact colours drawn.

## Known limitations (v0.3.0)

- Detune is capped at +/-50 cents in every mode. Shift mode's engine is capable of far more, but larger intervals need per-voice control and a harmonizer's interface, which is a separate feature rather than a bigger number on this knob.
- Formant preservation is only meaningful in Shift mode; Classic and Micro do not resample the spectrum, so there is nothing for it to correct.
- De-Ess's detection threshold is still a fixed, absolute level (not level-relative/adaptive) - a very quiet take may need its gain staged up before De-Ess reacts meaningfully. See `docs/design-brief.md` ss2.1 for the reasoning.
- The Air shelf is not decramped, so at 15 kHz on a 44.1 kHz session its curve is slightly steeper near Nyquist than the same setting at 96 kHz. Correcting this would change the sound at the default setting and needs its own voicing pass.
- If a host ever feeds Seraph non-finite audio (NaN/Inf, e.g. from an upstream plugin misbehaving), a transport stop/start or reopening the plugin - which triggers a host `reset()` call - is the reliable way to clear it. This isn't unique to Seraph (envelope followers and filters latch NaN by construction once a state variable goes non-finite), but Shift mode's vendored engine specifically does not recover even from its own internal reset, so the wrapper substitutes silence for non-finite input before the engine ever sees it.
