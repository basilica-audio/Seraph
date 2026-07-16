#pragma once

// Central definition of all AudioProcessorValueTreeState parameter IDs for
// Seraph. See docs/architecture.md for the corresponding signal-flow diagram.
//
// FROZEN AS OF THE v0.1.0 PARAMETER LAYOUT:
// Parameter IDs below must NEVER change once shipped - saved sessions and
// presets persist the APVTS state keyed by these string IDs, and renaming or
// removing one would silently break every user's saved state. Ranges,
// defaults, and skew MAY still be refined during voicing/tuning milestones;
// only the IDs themselves are frozen. deEssListen and comp were added during
// M1 (before the v0.1.0 tag), so they are part of this same frozen set.
namespace ParamIDs
{
    // De-esser amount, 0-100%. Drives the maximum gain reduction applied to
    // the detected sibilance band (see deEssFreq); 0% is a bit-exact bypass.
    inline constexpr auto deEss = "deEss";

    // Center frequency of the sibilance detection/reduction band, Hz.
    // Sits within the ~5-9 kHz sibilance register.
    inline constexpr auto deEssFreq = "deEssFreq";

    // Detection bandwidth of the sibilance band, 0-100%, default 40%. Added
    // in v0.2.0 (deep-dive brief, docs/design-brief.md ss2.1): maps to the
    // detector's bandpass Q, 0% (narrow, Q=3.0) to 100% (wide, Q=0.7) - the
    // single most load-bearing control both reference de-essers in the
    // research notes expose that v0.1.0 did not. New parameter added after
    // v0.1.0 shipped: tolerant state import falls back to the documented
    // default (40%) when loading a state saved before this ID existed (see
    // tests/StateTests.cpp).
    inline constexpr auto deEssWidth = "deEssWidth";

    // Sibilance-listen ("solo") mode: when on, the de-esser stage outputs
    // only the detected sibilance band (the bandpassed detector signal)
    // instead of the gain-reduced full signal, so DeEssFreq can be tuned by
    // ear. Off by default and a bit-exact no-op on the rest of the chain
    // when off. See DeEsser::process().
    inline constexpr auto deEssListen = "deEssListen";

    // "Air" high-shelf gain, dB (cut or boost) at a fixed shelf frequency in
    // the ~10-16 kHz region - adds (or removes) the sense of airy openness
    // above the de-esser band.
    inline constexpr auto air = "air";

    // Gentle broadband downward-compressor amount, 0-100%: scales both
    // threshold and ratio from fully transparent (0%, bit-exact bypass) to a
    // gentle "glue" setting (100%, see GentleCompressor). No auto makeup
    // gain is applied - use Output to compensate perceived level changes.
    inline constexpr auto comp = "comp";

    // Doubler send amount, 0-100%: how much of the four delayed/detuned
    // doubled voices is blended in on top of the centered main signal. 0% is
    // a bit-exact bypass of the doubler. See Doubler.h for the four-voice,
    // per-voice-pan design (M1).
    inline constexpr auto doubleAmount = "double";

    // Doubler detune depth, in cents, applied as a small continuous
    // modulated-delay pitch wobble on each doubled voice (not a discrete
    // pitch shift) - the classic click-free "doubler" detune trick.
    inline constexpr auto doubleDetune = "doubleDetune";

    // Doubler stereo width, 0-100%: 0% keeps all four doubled voices
    // centered (mono-compatible chorus), 100% spreads them across the full
    // stereo field at their fixed per-voice pan positions.
    inline constexpr auto doubleWidth = "doubleWidth";

    // Overall dry/wet mix. At 0% the plugin is a passthrough of the input
    // (0 samples of reported latency - see docs/architecture.md).
    inline constexpr auto mix = "mix";

    // Output trim, applied after the doubler and before the dry/wet mix.
    inline constexpr auto output = "output";
}
