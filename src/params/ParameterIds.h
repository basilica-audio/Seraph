#pragma once

// Central definition of all AudioProcessorValueTreeState parameter IDs for
// Seraph. See docs/architecture.md for the corresponding signal-flow diagram.
//
// FROZEN AS OF THE v0.1 PARAMETER LAYOUT:
// Parameter IDs below must NEVER change once shipped - saved sessions and
// presets persist the APVTS state keyed by these string IDs, and renaming or
// removing one would silently break every user's saved state. Ranges,
// defaults, and skew MAY still be refined during voicing/tuning milestones;
// only the IDs themselves are frozen.
namespace ParamIDs
{
    // De-esser amount, 0-100%. Drives the maximum gain reduction applied to
    // the detected sibilance band (see deEssFreq); 0% is a bit-exact bypass.
    inline constexpr auto deEss = "deEss";

    // Center frequency of the sibilance detection/reduction band, Hz.
    // Sits within the ~5-9 kHz sibilance register.
    inline constexpr auto deEssFreq = "deEssFreq";

    // "Air" high-shelf gain, dB (cut or boost) at a fixed shelf frequency in
    // the ~10-16 kHz region - adds (or removes) the sense of airy openness
    // above the de-esser band.
    inline constexpr auto air = "air";

    // Doubler send amount, 0-100%: how much of the two delayed/detuned
    // doubled voices is blended in on top of the centered main signal. 0% is
    // a bit-exact bypass of the doubler.
    inline constexpr auto doubleAmount = "double";

    // Doubler detune depth, in cents, applied as a small continuous
    // modulated-delay pitch wobble on each doubled voice (not a discrete
    // pitch shift) - the classic click-free "doubler" detune trick.
    inline constexpr auto doubleDetune = "doubleDetune";

    // Doubler stereo width, 0-100%: 0% keeps both doubled voices centered
    // (mono-compatible chorus), 100% pans them hard left/right.
    inline constexpr auto doubleWidth = "doubleWidth";

    // Overall dry/wet mix. At 0% the plugin is a passthrough of the input
    // (0 samples of reported latency - see docs/architecture.md).
    inline constexpr auto mix = "mix";

    // Output trim, applied after the doubler and before the dry/wet mix.
    inline constexpr auto output = "output";
}
