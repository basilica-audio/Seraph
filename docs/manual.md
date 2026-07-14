<p align="center"><img src="assets/icon.png" alt="Seraph icon" width="120"/></p>

# Seraph — user manual

*Voices from above — a choir and vocal processor for operatic symphonic metal.*

## What Seraph is

Seraph is a channel-strip-style vocal processor built for the lead and choir vocal parts of operatic symphonic metal (Nightwish/Epica/Dimmu-Borgir territory): a soprano lead line, a layered choral backing, or a spoken/growled interlude that needs to sit cleanly against heavy layered guitars and an orchestra without disappearing or turning harsh.

It combines four processing stages that are normally reached for separately on a vocal:

1. **De-Ess** - tames sibilance ("s", "sh", "t" consonants) that a bright vocal mic and heavy top-end EQ elsewhere in the mix (cymbals, distorted guitar fizz, string sections) tend to make fatiguing.
2. **Air** - adds (or removes) the sense of airy openness above the vocal's natural presence range, the kind of shimmer that helps an operatic soprano cut through a wall of guitars.
3. **Gentle Compressor** - evens out dynamics with a "glue" style compressor, so the vocal sits at a consistent level in the mix without audibly pumping.
4. **Doubler** - a four-voice, click-free vocal doubler/chorus that thickens a single take into a small-choir spread, without the discrete pitch-shift artifacts of granular doublers.

Everything downstream of Mix/Output is a single self-contained channel strip: put Seraph on a vocal or choir bus, dial in de-essing and air to taste, add a touch of glue compression if the take is dynamically uneven, and use the doubler to widen a lead line or thicken a choir part.

## Where it sits in a symphonic-metal signal chain

Seraph is designed to run on vocal/choir tracks or a vocal bus, typically:

```
Vocal/choir recording -> (tuning/editing, if used) -> Seraph -> reverb/delay send -> mix bus
```

Because Seraph reports **0 samples of latency**, it never needs host-side plugin-delay-compensation accounting - it is safe to insert anywhere in a vocal chain, including in parallel (e.g. a doubled/blended parallel vocal bus) without phase-alignment surprises against a dry path.

A few practical placements in a symphonic-metal production:

- **Lead vocal track**: De-Ess first (mic proximity and consonants), a touch of Air to help an operatic voice cut through distorted guitars and orchestral strings, a little Comp for consistency, and a *subtle* amount of Double (10-20%) if the take needs filling out without sounding artificially doubled.
- **Choir/backing vocal bus**: heavier Double (40-70%) with full Width for a wide, layered choir spread from a smaller number of recorded takes; De-Ess and Air set more conservatively since choir blends are usually already less sibilant/harsh per-voice than a solo lead.
- **Spoken-word/growled interlude**: De-Ess is often unnecessary (little sibilance energy in a growled performance); Air and a stronger Comp setting help a spoken interlude stay present and level-consistent against a quiet orchestral backing.

## Signal flow

```
input -> De-Ess (sibilance dynamic EQ, + Listen mode) -> Air (12 kHz high-shelf)
       -> Gentle Compressor (broadband glue) -> Doubler (4 voices, per-voice pan)
       -> Output trim -> Mix -> output
```

See [`architecture.md`](architecture.md) for the full technical signal-flow diagram and DSP design notes.

## Parameter reference

| Parameter | Range | Default | Unit | What it does |
|---|---|---|---|---|
| **De-Ess** | 0-100 | 30 | % | Sibilance gain-reduction amount. Scales the maximum reduction applied to the detected band (up to 24 dB at 100%). 0% is an exact bypass of the de-esser. Start low (20-40%) and raise only as far as needed - overdoing de-essing makes "s" sounds sound lisped or muffled. |
| **De-Ess Freq** | 3,000-12,000 | 7,000 | Hz | Center frequency of the sibilance detection/reduction band. Female/soprano vocals often sibilate higher (7-9 kHz); lower male vocals or heavily proximity-mic'd takes may need 5-6 kHz. Use **De-Ess Listen** to find the right frequency by ear. |
| **De-Ess Listen** | off/on | off | - | Solos the detected sibilance band instead of the processed vocal, so you can sweep De-Ess Freq and hear exactly which frequency band is being targeted before dialling in reduction. Switch back off before mixing - Listen mode is a tuning aid, not a mix setting. |
| **Air** | -12 to +12 | +3 | dB | Fixed 12 kHz high-shelf. Boost for openness/shimmer above a vocal's natural top end (typical for a lead that needs to cut through a dense mix); cut if a bright mic/preamp or aggressive de-essing has left the vocal sounding thin or harsh. |
| **Comp** | 0-100 | 0 | % | Gentle broadband downward-compressor amount. Scales both threshold (down to -20 dBFS) and ratio (up to 3:1) together - a "glue" setting, not a squashing limiter. 0% is an exact bypass. No automatic makeup gain is applied; use **Output** to compensate if a higher Comp setting makes the vocal feel quieter. |
| **Double** | 0-100 | 25 | % | Doubler send amount: how much of the four doubled voices blends in on top of the centered dry signal. 0% is an exact bypass of the doubler. Subtle amounts (10-25%) thicken a lead without an obvious "chorus" effect; higher amounts (40%+) build a fuller small-choir spread, best suited to backing/choir parts rather than an exposed lead line. |
| **Double Detune** | 0-50 | 15 | cents | Depth of the doubler's continuous pitch wobble (a smooth modulated-delay detune, not a discrete pitch shift - always click-free). Lower values (5-10 cents) sound like a tight, subtle double; higher values (30-50 cents) sound looser and more chorus-like. |
| **Double Width** | 0-100 | 100 | % | Stereo spread of the doubler's four voices. 0% keeps all four voices centered (mono-compatible, useful if the vocal needs to stay centered in a mono-fold-down-sensitive mix); 100% spreads them across the full stereo field for a wide choir effect. |
| **Mix** | 0-100 | 100 | % | Overall dry/wet blend. Defaults to 100% (fully processed) since Seraph is meant to be run as a full channel strip, not blended - lower it only for parallel-processing setups (e.g. blending in a de-essed/doubled signal under an otherwise-untouched dry vocal). |
| **Output** | -24 to +24 | 0 | dB | Output trim, applied after the doubler and before Mix. Use to compensate level changes introduced by Comp or Double before the signal hits the next stage in your chain. |

All parameters are smoothed (no zipper noise on automation or manual knob moves) and safe to automate.

## Tips

- **De-ess before adding Air or Comp.** Sibilance energy sits in the same region Air boosts, and a broadband compressor will react to sibilant peaks just like any other transient - de-essing first keeps both of those stages working on a cleaner signal.
- **Use De-Ess Listen when you're not sure where the sibilance is.** It's much faster to sweep De-Ess Freq while soloing the detected band than to sweep it by ear against the full mix.
- **Comp is a glue knob, not a leveling tool.** If a take has wildly inconsistent level (very quiet verses, very loud choruses), fix that with clip gain or a dedicated leveling compressor upstream first; Comp's gentle 3:1 maximum ratio is meant to add consistency and cohesion on an already-reasonably-level take, not to rescue a wildly uneven one.
- **Double is additive, not a replacement for real doubled takes.** For choir parts, a handful of Double at low-to-moderate amounts on top of a couple of real recorded layers usually sounds fuller and more natural than relying on Double alone to simulate an entire choir from a single take.
- **Watch Width on a mono-sensitive mix.** If your material may be folded to mono (streaming platforms, some broadcast chains), check the doubler with Width pulled back toward 0% to make sure the doubled voices don't cancel unpleasantly when summed.
- **Zero latency means Seraph is safe in parallel chains.** Because Seraph never reports plugin delay, you can freely blend a Seraph-processed vocal bus against an untouched dry vocal bus (or duplicate a track and run two different Seraph settings on each) without needing your DAW to time-align anything.

## Known limitations (v0.1.0)

- The GUI is a functional slider/knob editor (custom vector-drawn GUI is a later milestone - see the project roadmap).
- No factory presets yet (also a later milestone).
- The doubler's detune is a continuous vibrato-style pitch wobble, not a full formant-preserving pitch shift - within its 0-50 cent range this does not produce audible "chipmunk" formant artifacts, but it is not a substitute for a dedicated harmonizer/pitch-shifter plugin if you need larger, formant-corrected pitch intervals.
